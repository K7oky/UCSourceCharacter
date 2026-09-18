#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosEngineInterface.h"  // EPhysicalSurface
#include "Engine/EngineTypes.h"  // ECollisionChannel
#include "GameFramework/PawnMovementComponent.h"

#include "Core/ISourceWorldQuery.h"  // TUniquePtr<ISourceWorldQuery> needs the complete type
#include "Core/SourceMovementParams.h"
#include "Core/SourceMovementSim.h"
#include "Core/SourceMovementState.h"
#include "Input/SourceInputTranslator.h"

#include "SourceMovementComponent.generated.h"

class FSourceWorldQuery_Analytic;

UENUM(BlueprintType)
enum class ESourceWorldQueryMode : uint8
{
	UnrealGeometry UMETA(DisplayName = "Unreal geometry (Chaos sweeps)"),

	AnalyticFlatGround UMETA(DisplayName = "Analytic - flat ground only (reference)")
};

DECLARE_MULTICAST_DELEGATE_OneParam(FSourceMovementEventsSignature, const FSourceMovementEvents&);

// Host shell for FSourceMovementSim
UCLASS(ClassGroup = (SourceMovement), meta = (BlueprintSpawnableComponent))
class SOURCEMOVEMENT_API USourceMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()

public:
	USourceMovementComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source Movement|Timestep", meta = (ClampMin = "1", ClampMax = "1000"))
	int32 TickRateHz = 64;

	// Upper bound on simulation steps per rendered frame, so a hitch cannot stall the game
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source Movement|Timestep", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaxStepsPerFrame = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source Movement|Collision")
	ESourceWorldQueryMode WorldQueryMode = ESourceWorldQueryMode::UnrealGeometry;

	// Trace channel used by the Chaos backend
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source Movement|Collision")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

	// Sweep against per-triangle collision instead of the simple collision primitives
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source Movement|Collision")
	bool bTraceComplex = false;

	// Height of the synthetic ground plane, in Source units, for AnalyticFlatGround only
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source Movement|Collision")
	float FlatGroundZ = 0.f;

	// A ladder mesh needs one of these assigned, as its material or as Phys Material Override
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source Movement|Collision")
	TSet<TEnumAsByte<EPhysicalSurface>> ClimbableSurfaces = { SurfaceType2, SurfaceType3 };

	// Assigned the same way, but these stop being solid to the player. Swimming needs a volume
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source Movement|Collision")
	TEnumAsByte<EPhysicalSurface> WaterSurface = SurfaceType4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source Movement|Collision")
	TEnumAsByte<EPhysicalSurface> SlimeSurface = SurfaceType5;

	void SetInputState(const FSourceInputState& InInput) { PendingInput = InInput; }
	const FSourceInputState& GetInputState() const { return PendingInput; }
	FSourceInputState& GetMutableInputState() { return PendingInput; }

	// Optional hook called once per SIMULATION STEP, immediately before the command is built
	TFunction<void(FSourceInputState&)> InputProvider;

	const FSourceMovementState& GetMovementState() const { return State; }
	FSourceMovementState& GetMutableMovementState() { return State; }

	const FSourceMovementParams& GetParams() const { return Params; }
	const FSourceMovementTelemetry& GetTelemetry() const { return Sim.GetTelemetry(); }

	FSourceMovementEventsSignature OnMovementEvents;

	void RebuildParams();

	// Ticks this component after the controller has read the mouse. Call again on possession
	void BindInputOrdering();

	ISourceWorldQuery* GetWorldQuery() const { return WorldQuery.Get(); }

	FSourceWorldQuery_Analytic* GetAnalyticWorld() const { return AnalyticWorld; }

	UFUNCTION(BlueprintPure, Category = "Source Movement") FVector GetSourceVelocity() const;
	UFUNCTION(BlueprintPure, Category = "Source Movement") float GetSourceSpeed() const;
	UFUNCTION(BlueprintPure, Category = "Source Movement") float GetSourceHorizontalSpeed() const;
	UFUNCTION(BlueprintPure, Category = "Source Movement") bool IsOnGround() const;
	UFUNCTION(BlueprintPure, Category = "Source Movement") float GetStamina() const;
	UFUNCTION(BlueprintPure, Category = "Source Movement") int32 GetTickBase() const;

	UFUNCTION(BlueprintPure, Category = "Source Movement") FVector GetViewOffset() const;

	UFUNCTION(BlueprintCallable, Category = "Source Movement")
	void TeleportTo(const FVector& NewUELocation, bool bResetVelocity = true);

protected:
	void InitializeWorldQuery();

	void PublishTransform();

	// On-screen telemetry and hull draw, driven by cl_showpos and cl_sourcemove_drawhull
	void DrawDebug(float DeltaTime);

private:
	FSourceMovementSim Sim;
	FSourceMovementState State;
	FSourceMovementParams Params;
	FSourceInputTranslator Translator;
	FSourceInputState PendingInput;

	TUniquePtr<ISourceWorldQuery> WorldQuery;

	// Non-owning view of WorldQuery when the analytic backend is active
	FSourceWorldQuery_Analytic* AnalyticWorld = nullptr;
	float Accumulator = 0.f;

	// What the last step used, so a frame's turn can be split across its steps
	FSrcAngles LastStepViewAngles = FSrcAngles();
	int32 NextCommandNumber = 1;
	bool bWarnedStepBudget = false;
};
