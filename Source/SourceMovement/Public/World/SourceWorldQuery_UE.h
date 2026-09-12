#pragma once

#include "Core/ISourceWorldQuery.h"
#include "Core/SourceMovementParams.h"

#include "Engine/EngineTypes.h"
#include "UObject/WeakObjectPtr.h"

class AActor;
class UWorld;

// Chaos-backed world query: sweeps Source's player AABB against the real level collision
class SOURCEMOVEMENT_API FSourceWorldQuery_UE final : public ISourceWorldQuery
{
public:
	FSourceWorldQuery_UE() = default;

	void Initialize(UWorld* InWorld, AActor* InIgnoreActor);

	ECollisionChannel TraceChannel = ECC_Pawn;
	bool bTraceComplex = false;

	virtual void TraceHull(const FSrcVec3& Start, const FSrcVec3& End, const FSrcVec3& Mins, const FSrcVec3& Maxs, int32 ContentMask, const FSourceTraceFilter& Filter, FSourceTraceResult& Out) override;

	virtual int32 GetPointContents(const FSrcVec3& Point, int32 ContentMask) override;

	virtual srcfloat GetSurfaceFriction(int32 SurfacePropsId) const override;
	virtual srcfloat GetSurfaceJumpFactor(int32 SurfacePropsId) const override;
	virtual srcfloat GetSurfaceMaxSpeedFactor(int32 SurfacePropsId) const override;
	virtual bool IsSurfaceClimbable(int32 SurfacePropsId) const override;
	virtual FSrcVec3 GetEntityAbsVelocity(const FSrcEntityRef& Entity) const override;

	void SetSurfaceProperties(int32 SurfacePropsId, srcfloat Friction, srcfloat JumpFactor = 1.f, srcfloat MaxSpeedFactor = 1.f, bool bClimbable = false);

	int32 GetQueryCount() const { return QueryCount; }
	void ResetQueryCounter() { QueryCount = 0; }

private:
	struct FSurfaceProps
	{
		srcfloat Friction = 0.8f;
		srcfloat JumpFactor = 1.0f;
		srcfloat MaxSpeedFactor = 1.0f;
		bool bClimbable = false;
	};

	// Degenerate (Start == End) sweep, routed to an overlap test
	void TestHullOverlap(const FSrcVec3& At, const FSrcVec3& Mins, const FSrcVec3& Maxs, const FSourceTraceFilter& Filter, FSourceTraceResult& Out);

	// Builds FSrcEntityRef from a hit actor and caches it for GetEntityAbsVelocity
	FSrcEntityRef MakeEntityRef(const AActor* Actor);

	static int32 ResolveSurfacePropsId(const struct FHitResult& Hit);

	// cl_sourcemove_drawtraces visualisation of a single sweep
	void DrawSweepDebug(const FSrcVec3& Start, const FSrcVec3& Mins, const FSrcVec3& Maxs, const FSourceTraceResult& Result) const;

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<AActor> IgnoreActor;
	TMap<int32, FSurfaceProps> SurfacePropsTable;
	TMap<int32, TWeakObjectPtr<const AActor>> EntityCache;
	int32 QueryCount = 0;
};
