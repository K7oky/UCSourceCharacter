#include "Actors/SourceMovementComponent.h"

#include "Actors/SourceHullComponent.h"
#include "SourceMovementModule.h"
#include "SourceUnits.h"
#include "World/SourceWorldQuery_Analytic.h"
#include "World/SourceWorldQuery_UE.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "SourceMovementCVars.h"

USourceMovementComponent::USourceMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bUpdateOnlyIfRendered = false;
	bAutoRegisterUpdatedComponent = true;
}

void USourceMovementComponent::RebuildParams()
{
	Params = FSourceMovementParams();
	Params.TickInterval = 1.0f / static_cast<srcfloat>(FMath::Max(1, TickRateHz));
	Params.WaterSurfacePropsId = static_cast<int32>(WaterSurface.GetValue());
	Params.SlimeSurfacePropsId = static_cast<int32>(SlimeSurface.GetValue());

	SourceMovementCVars::ApplyToParams(Params);
	SourceMovementCVars::ApplyToInputTranslator(Translator);
}

void USourceMovementComponent::InitializeWorldQuery()
{
	AnalyticWorld = nullptr;

	if (WorldQueryMode == ESourceWorldQueryMode::UnrealGeometry)
	{
		FSourceWorldQuery_UE* UEQuery = new FSourceWorldQuery_UE();
		UEQuery->TraceChannel = TraceChannel.GetValue();
		UEQuery->bTraceComplex = bTraceComplex;

		UEQuery->Initialize(GetWorld(), GetOwner());

		for (const TEnumAsByte<EPhysicalSurface>& Surface : ClimbableSurfaces)
		{
			UEQuery->SetSurfaceProperties(static_cast<int32>(Surface.GetValue()), Params.DefaultSurfaceFriction, Params.DefaultSurfaceJumpFactor, Params.DefaultSurfaceMaxSpeedFactor, /*bClimbable=*/true);
		}

		UEQuery->SetWaterSurface(Params.WaterSurfacePropsId, SRC_CONTENTS_WATER);
		UEQuery->SetWaterSurface(Params.SlimeSurfacePropsId, SRC_CONTENTS_SLIME);

		WorldQuery = TUniquePtr<ISourceWorldQuery>(UEQuery);
		return;
	}

	FSourceWorldQuery_Analytic* Analytic = new FSourceWorldQuery_Analytic();
	AnalyticWorld = Analytic;
	WorldQuery = TUniquePtr<ISourceWorldQuery>(Analytic);

	Analytic->AddGroundPlane(static_cast<srcfloat>(FlatGroundZ));

	UE_LOG(LogSourceMovement, Warning, TEXT("USourceMovementComponent: running the ANALYTIC reference backend: the player will not " "collide with level geometry, only with the slab at z = %.2f. Switch WorldQueryMode to " "UnrealGeometry for normal play."), FlatGroundZ);
}

void USourceMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	RebuildParams();
	InitializeWorldQuery();

	if (UpdatedComponent)
	{
		State.Origin = SourceUnits::ToSrcPosition(UpdatedComponent->GetComponentLocation());
		State.ViewAngles = SourceUnits::ToSrcAngles(UpdatedComponent->GetComponentRotation());
		PendingInput.ViewAngles = State.ViewAngles;
		LastStepViewAngles = State.ViewAngles;

		// Rotation now lives in ViewAngles, the root stays axis-aligned
		UpdatedComponent->SetWorldRotation(FQuat::Identity);
	}

	State.Velocity = FSrcVec3::Zero;
	State.MoveType = ESrcMoveType::Walk;
	State.ViewOffset = Params.Hulls.GetPlayerViewOffset(false);
	State.m_flMaxspeed = Params.PlayerSpeedRun;
	State.GroundEntity = FSrcEntityRef::None();
	State.m_surfaceFriction = 1.0f;
	Accumulator = 0.f;
	NextCommandNumber = 1;

	BindInputOrdering();

	PublishTransform();
}

void USourceMovementComponent::BindInputOrdering()
{
	// The controller reads the mouse in TG_PrePhysics too and Unreal does not order the two, so
	// without this every step sees a frame-stale view angle and strafes feel heavy
	APawn* Pawn = Cast<APawn>(GetOwner());
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (Controller)
	{
		PrimaryComponentTick.AddPrerequisite(Controller, Controller->PrimaryActorTick);
	}
}

void USourceMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!UpdatedComponent || !WorldQuery.IsValid())
	{
		return;
	}

	if (ShouldSkipUpdate(DeltaTime))
	{
		return;
	}

	// Once per frame, not per step: the console state must not change mid-loop
	RebuildParams();
	const float TickInterval = static_cast<float>(Params.TickInterval);
	Accumulator += DeltaTime;

	// The mouse lands once per frame but a frame can be several ticks. Giving them all the same
	// angle throws away the turn between them, which is most of what air-strafe gain is made of.
	const int32 PlannedSteps = FMath::Min(FMath::FloorToInt(Accumulator / TickInterval), MaxStepsPerFrame);
	const FSrcAngles FrameStartAngles = LastStepViewAngles;

	FSrcAngles ViewDelta = PendingInput.ViewAngles - FrameStartAngles;
	ViewDelta.y = SourceMath::AngleNormalize(ViewDelta.y);

	int32 Steps = 0;
	while (Accumulator >= TickInterval)
	{
		if (Steps >= MaxStepsPerFrame)
		{
			if (!bWarnedStepBudget)
			{
				bWarnedStepBudget = true;
				UE_LOG(LogSourceMovement, Warning, TEXT("USourceMovementComponent: exceeded MaxStepsPerFrame (%d): %0.1f ms of " "simulation time discarded. Replays and differential runs must never hit this."), MaxStepsPerFrame, Accumulator * 1000.f);
			}

			Accumulator = 0.f;
			break;
		}

		// Sampled per step, not per frame, so held keys land on every tick
		if (InputProvider)
		{
			InputProvider(PendingInput);
		}

		FSourceInputState StepInput = PendingInput;
		if (PlannedSteps > 1)
		{
			const srcfloat Alpha = static_cast<srcfloat>(Steps + 1) / static_cast<srcfloat>(PlannedSteps);
			StepInput.ViewAngles.x = FrameStartAngles.x + ViewDelta.x * Alpha;
			StepInput.ViewAngles.y = SourceMath::AngleNormalize(FrameStartAngles.y + ViewDelta.y * Alpha);
			StepInput.ViewAngles.z = FrameStartAngles.z + ViewDelta.z * Alpha;
		}

		const FSourceUserCmd Cmd = Translator.Translate(StepInput, NextCommandNumber, State.m_nTickBase);
		++NextCommandNumber;

		Sim.RunCommand(State, Cmd, Params, *WorldQuery);

		// Drained inside the loop: the events are cleared at the top of every step
		if (OnMovementEvents.IsBound())
		{
			OnMovementEvents.Broadcast(Sim.GetEvents());
		}

		Accumulator -= TickInterval;
		++Steps;
	}

	if (Steps > 0)
	{
		// Only once a step consumed it, otherwise a frame that ran no steps would drop its turn
		LastStepViewAngles = PendingInput.ViewAngles;

		PublishTransform();

		// Nothing here reads it back, but animation and AI expect it to be current
		Velocity = SourceUnits::ToUEDirection(State.Velocity);
	}

	DrawDebug(DeltaTime);
}

void USourceMovementComponent::DrawDebug(float DeltaTime)
{
	const int32 TelemetryLevel = SourceMovementCVars::GetShowPos();

	if (TelemetryLevel > 0 && GEngine)
	{
		const FSourceMovementTelemetry& T = Sim.GetTelemetry();
		const int32 BaseKey = 0x5000 + GetUniqueID() % 1000;

		GEngine->AddOnScreenDebugMessage(BaseKey + 0, 0.f, FColor::White, FString::Printf(TEXT("[Source] speed %7.2f   horiz %7.2f   vel (%8.2f %8.2f %8.2f)"), State.Velocity.Length(), State.Velocity.Length2D(), State.Velocity.x, State.Velocity.y, State.Velocity.z));

		GEngine->AddOnScreenDebugMessage(BaseKey + 1, 0.f, State.IsOnGround() ? FColor::Green : FColor::Yellow, FString::Printf(TEXT("[Source] %s   maxspeed %6.2f   wishspeed %6.2f   wishdir (%5.2f %5.2f)"), State.IsOnGround() ? TEXT("GROUNDED") : TEXT("AIRBORNE"), T.MaxSpeed, T.WishSpeed, T.WishDir.x, T.WishDir.y));

		{
			const srcfloat NormalZ = T.GroundNormal.z;
			const bool bWalkable = NormalZ >= FSourceMovementParams::StandableGroundNormalZ;
			const float SlopeDeg = (NormalZ > 0.f && NormalZ <= 1.f)
				? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(NormalZ, -1.f, 1.f))) : -1.f;

			GEngine->AddOnScreenDebugMessage(BaseKey + 5, 0.f, bWalkable ? FColor::Green : FColor::Orange, FString::Printf(TEXT("[Source] groundNormal (%5.2f %5.2f %5.2f)  z %5.3f  slope %5.1f deg  %s (walk needs z >= 0.70, i.e. <= 45.6 deg)"), T.GroundNormal.x, T.GroundNormal.y, T.GroundNormal.z, NormalZ, SlopeDeg, bWalkable ? TEXT("WALKABLE") : TEXT("TOO STEEP -> surf")));
		}

		if (TelemetryLevel > 1)
		{
			GEngine->AddOnScreenDebugMessage(BaseKey + 2, 0.f, FColor::Cyan, FString::Printf(TEXT("[Source] stamina %6.2f   surfFric %4.2f   accel %6.2f   step %5.2f"), State.m_flStamina, State.m_surfaceFriction, T.AccelSpeed, T.StepHeight));

			GEngine->AddOnScreenDebugMessage(BaseKey + 3, 0.f, FColor::Cyan, FString::Printf(TEXT("[Source] traces %2d   planes %d   blocked %d   tick %d   origin (%.3f %.3f %.3f)"), T.TraceCount, T.CollisionPlanes, T.Blocked, State.m_nTickBase, State.Origin.x, State.Origin.y, State.Origin.z));

			GEngine->AddOnScreenDebugMessage(BaseKey + 4, 0.f, FColor::Silver, FString::Printf(TEXT("[Source] groundNormal (%5.2f %5.2f %5.2f)   ducked %d   duckAmount %4.2f"), T.GroundNormal.x, T.GroundNormal.y, T.GroundNormal.z, State.m_bDucked ? 1 : 0, State.m_flDuckAmount));

			static const TCHAR* const WaterLevelNames[] = { TEXT("dry"), TEXT("FEET"), TEXT("WAIST"), TEXT("EYES") };
			const int32 WaterIndex = FMath::Clamp(static_cast<int32>(State.WaterLevel), 0, 3);

			GEngine->AddOnScreenDebugMessage(BaseKey + 6, 0.f, State.WaterLevel > ESrcWaterLevel::NotInWater ? FColor::Blue : FColor::Silver, FString::Printf(TEXT("[Source] water %s   type %s   waterJump %6.1f   surfaceProps %d"), WaterLevelNames[WaterIndex], State.WaterType & SRC_CONTENTS_SLIME ? TEXT("slime") : State.WaterType & SRC_CONTENTS_WATER ? TEXT("water") : TEXT("none"), State.m_flWaterJumpTime, State.m_surfaceProps));

			GEngine->AddOnScreenDebugMessage(BaseKey + 7, 0.f, FColor::Magenta, FString::Printf(TEXT("[Source] backend %s"), WorldQueryMode == ESourceWorldQueryMode::UnrealGeometry ? TEXT("Chaos sweeps") : TEXT("analytic flat ground")));
		}
	}

	if (SourceMovementCVars::GetDrawHull())
	{
		FVector CenterOffset, Extent;
		SourceUnits::ToUEBox(Params.Hulls.GetPlayerMins(State.m_bDucked), Params.Hulls.GetPlayerMaxs(State.m_bDucked), CenterOffset, Extent);

		DrawDebugBox(GetWorld(), SourceUnits::ToUEPosition(State.Origin) + CenterOffset, Extent, State.IsOnGround() ? FColor::Green : FColor::Yellow, false, -1.f, 0, 1.f);
	}

	(void)DeltaTime;
}

void USourceMovementComponent::PublishTransform()
{
	if (!UpdatedComponent)
	{
		return;
	}

	// Identity on purpose: the hull is axis-aligned and the view lives in State.ViewAngles
	UpdatedComponent->SetWorldLocationAndRotation(SourceUnits::ToUEPosition(State.Origin), FQuat::Identity, false);

	if (AActor* Owner = GetOwner())
	{
		if (USourceHullComponent* Hull = Owner->FindComponentByClass<USourceHullComponent>())
		{
			Hull->SetFromSourceHull(Params.Hulls.GetPlayerMins(State.m_bDucked), Params.Hulls.GetPlayerMaxs(State.m_bDucked));
		}
	}
}

void USourceMovementComponent::TeleportTo(const FVector& NewUELocation, bool bResetVelocity)
{
	State.Origin = SourceUnits::ToSrcPosition(NewUELocation);

	if (bResetVelocity)
	{
		State.Velocity = FSrcVec3::Zero;
	}

	// Dropped so the next step re-finds the ground from where we landed
	State.GroundEntity = FSrcEntityRef::None();
	State.RemoveFlag(SRC_FL_ONGROUND);

	PublishTransform();
}

FVector USourceMovementComponent::GetSourceVelocity() const
{
	return SourceUnits::ToUEDirection(State.Velocity);
}

float USourceMovementComponent::GetSourceSpeed() const
{
	return static_cast<float>(State.Velocity.Length());
}

float USourceMovementComponent::GetSourceHorizontalSpeed() const
{
	return static_cast<float>(State.Velocity.Length2D());
}

bool USourceMovementComponent::IsOnGround() const
{
	return State.IsOnGround();
}

float USourceMovementComponent::GetStamina() const
{
	return static_cast<float>(State.m_flStamina);
}

int32 USourceMovementComponent::GetTickBase() const
{
	return State.m_nTickBase;
}

FVector USourceMovementComponent::GetViewOffset() const
{
	return SourceUnits::ToUEPosition(State.ViewOffset);
}
