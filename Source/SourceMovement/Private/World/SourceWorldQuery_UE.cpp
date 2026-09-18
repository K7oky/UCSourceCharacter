#include "World/SourceWorldQuery_UE.h"

#include "SourceMovementCVars.h"
#include "SourceMovementModule.h"
#include "SourceUnits.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/HitResult.h"
#include "Engine/Level.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodyInstance.h"

using namespace SourceMath;

namespace
{
	static const FName SourceMovementTraceTag(TEXT("SourceMovementHull"));

	// Water surfaces one sweep will step past before giving up and calling it a miss
	constexpr int32 MaxNonSolidRetries = 4;
	constexpr float WaterProbeExtent = 1.0f;
}

void FSourceWorldQuery_UE::Initialize(UWorld* InWorld, AActor* InIgnoreActor)
{
	World = InWorld;
	IgnoreActor = InIgnoreActor;
	EntityCache.Reset();
	StableEntityIds.Reset();
}

int32 FSourceWorldQuery_UE::ResolveEntityId(const AActor* Actor)
{
	const uint32 RuntimeId = Actor->GetUniqueID();
	if (const int32* Cached = StableEntityIds.Find(RuntimeId))
	{
		return *Cached;
	}

	// GetUniqueID indexes GUObjectArray and GetTypeHash(FName) indexes the name table, so both differ
	// between processes. The saved names do not. PIE renames the package, not the actor.
	uint32 Hash = GetTypeHash(Actor->GetName());
	if (const ULevel* Level = Actor->GetLevel())
	{
		Hash = HashCombine(Hash, GetTypeHash(UWorld::RemovePIEPrefix(Level->GetOutermost()->GetName())));
	}

	// 0 is the world and INDEX_NONE is "no entity", so a real actor must not land on either
	int32 Id = static_cast<int32>(Hash & 0x7fffffffu);
	if (Id == 0)
	{
		Id = 1;
	}

	if (const TWeakObjectPtr<const AActor>* Existing = EntityCache.Find(Id))
	{
		if (Existing->IsValid() && Existing->Get() != Actor)
		{
			UE_LOG(LogSourceMovement, Warning, TEXT("Entity id %d collides between '%s' and '%s', so ground identity will flip between them. " "Rename one."), Id, *Existing->Get()->GetName(), *Actor->GetName());
		}
	}

	StableEntityIds.Add(RuntimeId, Id);
	return Id;
}

FSrcEntityRef FSourceWorldQuery_UE::MakeEntityRef(const AActor* Actor)
{
	FSrcEntityRef Ref;

	if (!Actor)
	{
		return FSrcEntityRef::World();
	}

	Ref.EntityId = ResolveEntityId(Actor);
	Ref.bIsPlayer = Actor->IsA<APawn>();

	// Only CheckJumpButton reads bIsWorld, to tell a player apart from the map
	Ref.bIsWorld = !Ref.bIsPlayer && Actor->IsRootComponentStatic();

	EntityCache.Add(Ref.EntityId, Actor);
	return Ref;
}

int32 FSourceWorldQuery_UE::ResolveSurfacePropsId(const FHitResult& Hit)
{
	// EPhysicalSurface is Unreal's closest analogue of Source's surfaceproperties index
	if (const UPhysicalMaterial* PhysMat = Hit.PhysMaterial.Get())
	{
		return static_cast<int32>(PhysMat->SurfaceType.GetValue());
	}
	return static_cast<int32>(SurfaceType_Default);
}

void FSourceWorldQuery_UE::TraceHull(const FSrcVec3& Start, const FSrcVec3& End, const FSrcVec3& Mins, const FSrcVec3& Maxs, int32 ContentMask, const FSourceTraceFilter& Filter, FSourceTraceResult& Out)
{
	Out.Reset(Start, End);

	(void)Filter;
	UWorld* W = World.Get();
	if (!W)
	{
		return;
	}

	FVector CenterOffset, Extent;
	SourceUnits::ToUEBox(Mins, Maxs, CenterOffset, Extent);
	const FSrcVec3 Delta = End - Start;
	const bool bDegenerate = (Delta.LengthSqr() <= 0.f);

	if (bDegenerate)
	{
		TestHullOverlap(Start, Mins, Maxs, Filter, Out);
		return;
	}

	const FVector UEStart = SourceUnits::ToUEPosition(Start) + CenterOffset;
	const FVector UEEnd = SourceUnits::ToUEPosition(End) + CenterOffset;

	FCollisionQueryParams Params(SourceMovementTraceTag, bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	if (AActor* Ignore = IgnoreActor.Get())
	{
		Params.AddIgnoredActor(Ignore);
	}

	FHitResult Hit;
	bool bBlocking = false;

	// Water is not in MASK_PLAYERSOLID, so it must not stop the player. Retrying past it keeps the
	// ordinary case at one sweep, where a multi-sweep would cost every level.
	for (int32 Attempt = 0; Attempt < MaxNonSolidRetries; ++Attempt)
	{
		++QueryCount;
		bBlocking = W->SweepSingleByChannel(Hit, UEStart, UEEnd, FQuat::Identity, TraceChannel, FCollisionShape::MakeBox(Extent), Params);

		if (!bBlocking)
		{
			break;
		}

		const int32 Water = WaterContentsForSurface(ResolveSurfacePropsId(Hit));
		if (Water == 0 || (Water & ContentMask) != 0)
		{
			break;
		}

		Params.AddIgnoredComponent(Hit.GetComponent());
		bBlocking = false;
	}

	if (!bBlocking)
	{
		// Reset already left this as a completed sweep
		return;
	}

	Out.bDidHit = true;
	Out.Contents = SRC_CONTENTS_SOLID;
	Out.Entity = MakeEntityRef(Hit.GetActor());
	Out.SurfacePropsId = ResolveSurfacePropsId(Hit);

	// Source always reports a brush FACE plane
	FVector UENormal = Hit.ImpactNormal;
	if (UENormal.IsNearlyZero())
	{
		UENormal = Hit.Normal;
	}

	Out.PlaneNormal = SourceUnits::ToSrcNormal(UENormal.GetSafeNormal());
	Out.bValidPlane = !Out.PlaneNormal.IsZero(UE_KINDA_SMALL_NUMBER);

	if (Hit.bStartPenetrating)
	{
		// !startout -> startsolid; and if the END is also inside, allsolid with fraction 0
		Out.bStartSolid = true;
		Out.PenetrationDepth = static_cast<srcfloat>(Hit.PenetrationDepth * SourceUnits::UUToUnits);

		// Same Params, so water skipped above stays skipped here
		++QueryCount;
		const bool bEndSolid = W->OverlapBlockingTestByChannel(UEEnd, FQuat::Identity, TraceChannel, FCollisionShape::MakeBox(Extent), Params);

		if (bEndSolid)
		{
			Out.bAllSolid = true;
			Out.Fraction = 0.f;
			Out.EndPos = Start;
		}
		else
		{
			// Startsolid without allsolid keeps fraction 1.0; CanUnduck relies on that
			Out.Fraction = 1.0f;
			Out.EndPos = End;
		}
		return;
	}

	// Chaos only tells us which plane blocks; the fraction comes from Source's own plane math
	const FSrcVec3& N = Out.PlaneNormal;
	const FSrcVec3 ExtentSrc = (Maxs - Mins) * 0.5f;
	const FSrcVec3 CenterSrc = (Mins + Maxs) * 0.5f;

	// "push the plane out apropriately for mins/maxs" - the support of the box along N
	const srcfloat Support = FMath::Abs(N.x) * ExtentSrc.x + FMath::Abs(N.y) * ExtentSrc.y + FMath::Abs(N.z) * ExtentSrc.z;
	const FSrcVec3 ImpactSrc = SourceUnits::ToSrcPosition(Hit.ImpactPoint);
	const srcfloat DistEff = DotProduct(ImpactSrc, N) + Support;

	// Signed distances of the swept box CENTRE to the expanded plane, at both ends of the sweep
	const srcfloat d1 = DotProduct(Start + CenterSrc, N) - DistEff;
	const srcfloat d2 = DotProduct(End + CenterSrc, N) - DistEff;

	// Completely in front of the face, or moving away from it - no intersection
	if (d1 > 0.f && d2 >= d1)
	{
		Out.Reset(Start, End);
		return;
	}

	// Completely behind this face, so it adds no entry constraint and the next plane decides
	if (d1 <= 0.f && d2 <= 0.f)
	{
		Out.Reset(Start, End);
		return;
	}

	// Only an ENTERING crossing constrains the sweep
	if (d1 <= d2)
	{
		Out.Reset(Start, End);
		return;
	}

	const srcfloat f = (d1 - FSourceMovementParams::DistEpsilon) / (d1 - d2);

	if (f <= 0.f && SourceMovementCVars::GetTraceLog())
	{
		UE_LOG(LogSourceMovement, Warning, TEXT("tracelog DEGENERATE  N(%.4f %.4f %.4f) d1=%.6f d2=%.6f f=%.6f  chaosTime=%.6f\n")
			TEXT("impact(%.3f %.3f %.3f) start(%.3f %.3f %.3f) delta(%.4f %.4f %.4f)\n")
			TEXT("support=%.4f distEff=%.4f actor=%s"), N.x, N.y, N.z, d1, d2, f, Hit.Time,
			Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z, Start.x, Start.y, Start.z,
			Delta.x, Delta.y, Delta.z, Support, DistEff, *GetNameSafe(Hit.GetActor()));
	}

	srcfloat Fraction = f;

	if (f <= 0.f)
	{
		// Already at or past the standoff before the sweep starts, so this plane yields no fraction
		++QueryCount;
		const bool bEndSolid = W->OverlapBlockingTestByChannel(UEEnd, FQuat::Identity, TraceChannel, FCollisionShape::MakeBox(Extent), Params);

		if (SourceMovementCVars::GetTraceLog())
		{
			UE_LOG(LogSourceMovement, Warning, TEXT("-> endSolid=%d (%s)"), bEndSolid ? 1 : 0, bEndSolid ? TEXT("real wall, plane kept") : TEXT("seam, plane dropped"));
		}

		if (!bEndSolid)
		{
			Out.Reset(Start, End);
			return;
		}

		Fraction = 0.f;
	}

	Out.Fraction = SrcClamp(Fraction, 0.0f, 1.0f);
	VectorMA(Start, Out.Fraction, Delta, Out.EndPos);

	DrawSweepDebug(Start, Mins, Maxs, Out);
}

void FSourceWorldQuery_UE::DrawSweepDebug(const FSrcVec3& Start, const FSrcVec3& Mins, const FSrcVec3& Maxs, const FSourceTraceResult& Result) const
{
	const int32 Level = SourceMovementCVars::GetDrawTraces();
	if (Level <= 0)
	{
		return;
	}

	if (Level < 2 && !Result.bDidHit)
	{
		return;
	}

	UWorld* W = World.Get();
	if (!W)
	{
		return;
	}

	FVector CenterOffset, Extent;
	SourceUnits::ToUEBox(Mins, Maxs, CenterOffset, Extent);
	const FVector UEStart = SourceUnits::ToUEPosition(Start) + CenterOffset;
	const FVector UEStop = SourceUnits::ToUEPosition(Result.EndPos) + CenterOffset;

	const FColor Color = Result.bAllSolid ? FColor::Red : Result.bStartSolid ? FColor::Magenta : Result.bDidHit ? FColor::Orange : FColor::Silver;

	DrawDebugLine(W, UEStart, UEStop, Color, false, -1.f, 0, 0.5f);
	DrawDebugBox(W, UEStop, Extent, Color, false, -1.f, 0, 0.5f);

	if (Result.bValidPlane)
	{
		const FVector UENormal = SourceUnits::ToUENormal(Result.PlaneNormal);
		DrawDebugDirectionalArrow(W, UEStop, UEStop + UENormal * 40.0, 8.f, FColor::Cyan, false, -1.f, 0, 0.5f);
	}
}

void FSourceWorldQuery_UE::TestHullOverlap(const FSrcVec3& At, const FSrcVec3& Mins, const FSrcVec3& Maxs, const FSourceTraceFilter& Filter, FSourceTraceResult& Out)
{
	(void)Filter;
	UWorld* W = World.Get();
	if (!W)
	{
		return;
	}

	FVector CenterOffset, Extent;
	SourceUnits::ToUEBox(Mins, Maxs, CenterOffset, Extent);
	const FVector UEAt = SourceUnits::ToUEPosition(At) + CenterOffset;

	FCollisionQueryParams Params(SourceMovementTraceTag, bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	if (AActor* Ignore = IgnoreActor.Get())
	{
		Params.AddIgnoredActor(Ignore);
	}

	TArray<FOverlapResult> Overlaps;
	++QueryCount;
	W->OverlapMultiByChannel(Overlaps, UEAt, FQuat::Identity, TraceChannel, FCollisionShape::MakeBox(Extent), Params);
	const FOverlapResult* Blocking = nullptr;
	for (const FOverlapResult& Result : Overlaps)
	{
		// Standing in water is not being stuck
		if (Result.bBlockingHit && WaterContentsForComponent(Result.GetComponent()) == 0)
		{
			Blocking = &Result;
			break;
		}
	}

	if (!Blocking)
	{
		return;
	}

	Out.bDidHit = true;
	Out.bStartSolid = true;
	Out.bAllSolid = true;
	Out.Fraction = 0.f;
	Out.EndPos = At;
	Out.Contents = SRC_CONTENTS_SOLID;
	Out.Entity = MakeEntityRef(Blocking->GetActor());

	// Source leaves the plane zero-initialized here, and StepMove depends on that
}

int32 FSourceWorldQuery_UE::GetPointContents(const FSrcVec3& Point, int32 ContentMask)
{
	UWorld* W = World.Get();
	if (!W)
	{
		return SRC_CONTENTS_EMPTY;
	}

	const FVector UEPoint = SourceUnits::ToUEPosition(Point);

	FCollisionQueryParams Params(SourceMovementTraceTag, bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	if (AActor* Ignore = IgnoreActor.Get())
	{
		Params.AddIgnoredActor(Ignore);
	}

	// Containment, so water has to be a volume with SIMPLE collision: an Unreal overlap cannot see a
	// Complex-as-Simple mesh, which is what a map export leaves behind.
	TArray<FOverlapResult> Overlaps;
	++QueryCount;
	W->OverlapMultiByChannel(Overlaps, UEPoint, FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(WaterProbeExtent), Params);

	int32 Contents = SRC_CONTENTS_EMPTY;
	for (const FOverlapResult& Result : Overlaps)
	{
		const int32 Water = WaterContentsForComponent(Result.GetComponent());
		Contents |= Water;

		if (Result.bBlockingHit && Water == SRC_CONTENTS_EMPTY)
		{
			Contents |= SRC_CONTENTS_SOLID;
		}
	}

	return Contents & ContentMask;
}

int32 FSourceWorldQuery_UE::WaterContentsForSurface(int32 SurfacePropsId) const
{
	const int32* Bit = WaterSurfaces.Find(SurfacePropsId);
	return Bit ? *Bit : SRC_CONTENTS_EMPTY;
}

int32 FSourceWorldQuery_UE::WaterContentsForComponent(const UPrimitiveComponent* Component) const
{
	if (!Component || WaterSurfaces.IsEmpty())
	{
		return SRC_CONTENTS_EMPTY;
	}

	FBodyInstance* Body = Component->GetBodyInstance();
	if (const UPhysicalMaterial* Simple = Body ? Body->GetSimplePhysicalMaterial() : nullptr)
	{
		const int32 Bits = WaterContentsForSurface(static_cast<int32>(Simple->SurfaceType.GetValue()));
		if (Bits != SRC_CONTENTS_EMPTY)
		{
			return Bits;
		}
	}

	// Phys Material Override missed, so try the mesh materials, where imported geometry carries it
	int32 Contents = SRC_CONTENTS_EMPTY;
	const int32 NumMaterials = Component->GetNumMaterials();
	for (int32 Index = 0; Index < NumMaterials; ++Index)
	{
		const UMaterialInterface* Material = Component->GetMaterial(Index);
		if (const UPhysicalMaterial* PhysMat = Material ? Material->GetPhysicalMaterial() : nullptr)
		{
			Contents |= WaterContentsForSurface(static_cast<int32>(PhysMat->SurfaceType.GetValue()));
		}
	}

	return Contents;
}

void FSourceWorldQuery_UE::SetWaterSurface(int32 SurfacePropsId, int32 ContentsBit)
{
	WaterSurfaces.Add(SurfacePropsId, ContentsBit);
}

srcfloat FSourceWorldQuery_UE::GetSurfaceFriction(int32 SurfacePropsId) const
{
	if (const FSurfaceProps* Props = SurfacePropsTable.Find(SurfacePropsId))
	{
		return Props->Friction;
	}

	// The "default" surface material
	return 0.8f;
}

srcfloat FSourceWorldQuery_UE::GetSurfaceJumpFactor(int32 SurfacePropsId) const
{
	if (const FSurfaceProps* Props = SurfacePropsTable.Find(SurfacePropsId))
	{
		return Props->JumpFactor;
	}
	return 1.0f;
}

srcfloat FSourceWorldQuery_UE::GetSurfaceMaxSpeedFactor(int32 SurfacePropsId) const
{
	if (const FSurfaceProps* Props = SurfacePropsTable.Find(SurfacePropsId))
	{
		return Props->MaxSpeedFactor;
	}
	return 1.0f;
}

bool FSourceWorldQuery_UE::IsSurfaceClimbable(int32 SurfacePropsId) const
{
	if (const FSurfaceProps* Props = SurfacePropsTable.Find(SurfacePropsId))
	{
		return Props->bClimbable;
	}
	return false;
}

FSrcVec3 FSourceWorldQuery_UE::GetEntityAbsVelocity(const FSrcEntityRef& Entity) const
{
	if (const TWeakObjectPtr<const AActor>* Found = EntityCache.Find(Entity.EntityId))
	{
		if (const AActor* Actor = Found->Get())
		{
			return SourceUnits::ToSrcPosition(Actor->GetVelocity());
		}
	}
	return FSrcVec3::Zero;
}

void FSourceWorldQuery_UE::SetSurfaceProperties(int32 SurfacePropsId, srcfloat Friction, srcfloat JumpFactor, srcfloat MaxSpeedFactor, bool bClimbable)
{
	FSurfaceProps Props;
	Props.Friction = Friction;
	Props.JumpFactor = JumpFactor;
	Props.MaxSpeedFactor = MaxSpeedFactor;
	Props.bClimbable = bClimbable;
	SurfacePropsTable.Add(SurfacePropsId, Props);
}
