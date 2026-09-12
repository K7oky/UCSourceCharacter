#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosEngineInterface.h"  // EPhysicalSurface
#include "Components/ActorComponent.h"

#include "SourceMovementAudioComponent.generated.h"

class UAudioComponent;
class USoundBase;
class USourceMovementComponent;

USTRUCT(BlueprintType)
struct FSourceSurfaceSounds
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source Audio")
	TArray<TSoftObjectPtr<USoundBase>> Footsteps;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source Audio")
	TArray<TSoftObjectPtr<USoundBase>> Land;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source Audio")
	TSoftObjectPtr<USoundBase> JumpLaunch;
};

UCLASS(ClassGroup = (SourceMovement), meta = (BlueprintSpawnableComponent))
class SOURCEMOVEMENT_API USourceMovementAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USourceMovementAudioComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source Audio")
	FSourceSurfaceSounds DefaultSounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source Audio")
	TMap<TEnumAsByte<EPhysicalSurface>, FSourceSurfaceSounds> SurfaceSounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source Audio", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float FootstepVolumeScale = 1.0f;

	// Scales the landing sound, whose volume is derived from the impact velocity
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source Audio", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float LandVolumeScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source Audio", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float JumpVolumeScale = 1.0f;

	// Landing velocity that maps to full volume
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source Audio", meta = (ClampMin = "271.0"))
	float LandVelocityForFullVolume = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source Audio|Surf")
	bool bEnableSurfWind = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source Audio|Surf")
	TSoftObjectPtr<USoundBase> SurfWindLoop;

	// Speed at which the wind starts to be audible, and the speed at which it is at full volume
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source Audio|Surf")
	FVector2D SurfWindSpeedRange = FVector2D(400.f, 1600.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source Audio|Surf", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float SurfWindVolumeScale = 1.0f;

protected:
	void HandleMovementEvents(const struct FSourceMovementEvents& Events);

	const FSourceSurfaceSounds& ResolveSounds(int32 SurfacePropsId) const;

	USoundBase* PickRandom(const TArray<TSoftObjectPtr<USoundBase>>& Clips, int32& InOutLastIndex) const;

	// 2D plays without attenuation; these are the local player's own sounds
	void PlayLocal(USoundBase* Sound, float Volume) const;

	void UpdateSurfWind();

private:
	UPROPERTY(Transient)
	TObjectPtr<USourceMovementComponent> Movement;

	// The wind bed, spawned once and kept alive across frames; only its volume changes
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> SurfWindAudio;

	// One-shot latch for the "sound assets are missing" warning
	mutable bool bWarnedMissingSounds = false;
	int32 LastFootstepIndex = INDEX_NONE;
	int32 LastLandIndex = INDEX_NONE;
};
