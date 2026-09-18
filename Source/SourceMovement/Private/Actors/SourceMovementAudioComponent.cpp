#include "Actors/SourceMovementAudioComponent.h"

#include "Actors/SourceMovementComponent.h"
#include "Components/AudioComponent.h"
#include "Core/SourceMovementParams.h"
#include "Core/SourceMovementState.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "SourceMovementModule.h"

namespace
{
	// The shipped concrete set, so the component works with no setup
	const TCHAR* const GDefaultFootstepPaths[] = {
		TEXT("/Game/sounds/player/footsteps/concrete_ct_01.concrete_ct_01"),
		TEXT("/Game/sounds/player/footsteps/concrete_ct_02.concrete_ct_02"),
		TEXT("/Game/sounds/player/footsteps/concrete_ct_03.concrete_ct_03"),
		TEXT("/Game/sounds/player/footsteps/concrete_ct_04.concrete_ct_04"),
		TEXT("/Game/sounds/player/footsteps/concrete_ct_05.concrete_ct_05"),
		TEXT("/Game/sounds/player/footsteps/concrete_ct_06.concrete_ct_06"),
		TEXT("/Game/sounds/player/footsteps/concrete_ct_07.concrete_ct_07"),
		TEXT("/Game/sounds/player/footsteps/concrete_ct_08.concrete_ct_08"),
		TEXT("/Game/sounds/player/footsteps/concrete_ct_09.concrete_ct_09"),
		TEXT("/Game/sounds/player/footsteps/concrete_ct_10.concrete_ct_10"),
		TEXT("/Game/sounds/player/footsteps/concrete_ct_11.concrete_ct_11"),
		TEXT("/Game/sounds/player/footsteps/concrete_ct_12.concrete_ct_12"),
		TEXT("/Game/sounds/player/footsteps/concrete_ct_13.concrete_ct_13"),
		TEXT("/Game/sounds/player/footsteps/concrete_ct_14.concrete_ct_14"),
		TEXT("/Game/sounds/player/footsteps/concrete_ct_15.concrete_ct_15"),
		TEXT("/Game/sounds/player/footsteps/concrete_ct_16.concrete_ct_16"),
		TEXT("/Game/sounds/player/footsteps/concrete_ct_17.concrete_ct_17"),
	};

	const TCHAR* const GDefaultLandPaths[] = {
		TEXT("/Game/sounds/player/land/land.land"),
		TEXT("/Game/sounds/player/land/land2.land2"),
		TEXT("/Game/sounds/player/land/land3.land3"),
		TEXT("/Game/sounds/player/land/land4.land4"),
	};
	const TCHAR* const GDefaultJumpPath = TEXT("/Game/sounds/player/land/jump_launch_01.jump_launch_01");
	const TCHAR* const GDefaultWindPath = TEXT("/Game/sounds/player/surf/slow_wind_lp_a_02.slow_wind_lp_a_02");

	const TCHAR* const GWoodLadderPaths[] = {
		TEXT("/Game/sounds/player/ladder/wood/ladder_wood_01.ladder_wood_01"),
		TEXT("/Game/sounds/player/ladder/wood/ladder_wood_02.ladder_wood_02"),
		TEXT("/Game/sounds/player/ladder/wood/ladder_wood_03.ladder_wood_03"),
		TEXT("/Game/sounds/player/ladder/wood/ladder_wood_04.ladder_wood_04"),
		TEXT("/Game/sounds/player/ladder/wood/ladder_wood_05.ladder_wood_05"),
		TEXT("/Game/sounds/player/ladder/wood/ladder_wood_06.ladder_wood_06"),
		TEXT("/Game/sounds/player/ladder/wood/ladder_wood_07.ladder_wood_07"),
		TEXT("/Game/sounds/player/ladder/wood/ladder_wood_08.ladder_wood_08"),
		TEXT("/Game/sounds/player/ladder/wood/ladder_wood_09.ladder_wood_09"),
		TEXT("/Game/sounds/player/ladder/wood/ladder_wood_10.ladder_wood_10"),
		TEXT("/Game/sounds/player/ladder/wood/ladder_wood_11.ladder_wood_11"),
		TEXT("/Game/sounds/player/ladder/wood/ladder_wood_12.ladder_wood_12"),
		TEXT("/Game/sounds/player/ladder/wood/ladder_wood_13.ladder_wood_13"),
		TEXT("/Game/sounds/player/ladder/wood/ladder_wood_14.ladder_wood_14"),
	};

	// CS ships no ladder_07
	const TCHAR* const GMetalLadderPaths[] = {
		TEXT("/Game/sounds/player/ladder/metal/ladder_01.ladder_01"),
		TEXT("/Game/sounds/player/ladder/metal/ladder_02.ladder_02"),
		TEXT("/Game/sounds/player/ladder/metal/ladder_03.ladder_03"),
		TEXT("/Game/sounds/player/ladder/metal/ladder_04.ladder_04"),
		TEXT("/Game/sounds/player/ladder/metal/ladder_05.ladder_05"),
		TEXT("/Game/sounds/player/ladder/metal/ladder_06.ladder_06"),
		TEXT("/Game/sounds/player/ladder/metal/ladder_08.ladder_08"),
		TEXT("/Game/sounds/player/ladder/metal/ladder_09.ladder_09"),
		TEXT("/Game/sounds/player/ladder/metal/ladder_10.ladder_10"),
		TEXT("/Game/sounds/player/ladder/metal/ladder_11.ladder_11"),
		TEXT("/Game/sounds/player/ladder/metal/ladder_12.ladder_12"),
	};
}

USourceMovementAudioComponent::USourceMovementAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// After the movement component, so the wind reads a settled speed
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	for (const TCHAR* Path : GDefaultFootstepPaths)
	{
		DefaultSounds.Footsteps.Add(TSoftObjectPtr<USoundBase>(FSoftObjectPath(Path)));
	}

	for (const TCHAR* Path : GDefaultLandPaths)
	{
		DefaultSounds.Land.Add(TSoftObjectPtr<USoundBase>(FSoftObjectPath(Path)));
	}

	DefaultSounds.JumpLaunch = TSoftObjectPtr<USoundBase>(FSoftObjectPath(GDefaultJumpPath));
	SurfWindLoop = TSoftObjectPtr<USoundBase>(FSoftObjectPath(GDefaultWindPath));

	// A ladder step reports the ladder's material, so it lands in one of these
	FSourceSurfaceSounds& WoodLadder = SurfaceSounds.Add(SurfaceType2);
	for (const TCHAR* Path : GWoodLadderPaths)
	{
		WoodLadder.Footsteps.Add(TSoftObjectPtr<USoundBase>(FSoftObjectPath(Path)));
	}

	FSourceSurfaceSounds& MetalLadder = SurfaceSounds.Add(SurfaceType3);
	for (const TCHAR* Path : GMetalLadderPaths)
	{
		MetalLadder.Footsteps.Add(TSoftObjectPtr<USoundBase>(FSoftObjectPath(Path)));
	}
}

void USourceMovementAudioComponent::BeginPlay()
{
	Super::BeginPlay();
	Movement = GetOwner() ? GetOwner()->FindComponentByClass<USourceMovementComponent>() : nullptr;

	if (!Movement)
	{
		UE_LOG(LogSourceMovement, Warning, TEXT("%s: no USourceMovementComponent on %s, movement audio is disabled."), *GetName(), GetOwner() ? *GetOwner()->GetName() : TEXT("(no owner)"));
		return;
	}

	Movement->OnMovementEvents.AddUObject(this, &USourceMovementAudioComponent::HandleMovementEvents);
}

void USourceMovementAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Movement)
	{
		Movement->OnMovementEvents.RemoveAll(this);
	}

	if (SurfWindAudio)
	{
		SurfWindAudio->Stop();
		SurfWindAudio = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void USourceMovementAudioComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateSurfWind();
}

const FSourceSurfaceSounds* USourceMovementAudioComponent::FindSurfaceSounds(int32 SurfacePropsId) const
{
	if (SurfacePropsId == INDEX_NONE)
	{
		return nullptr;
	}

	return SurfaceSounds.Find(TEnumAsByte<EPhysicalSurface>(static_cast<EPhysicalSurface>(SurfacePropsId)));
}

USoundBase* USourceMovementAudioComponent::PickRandom(const TArray<TSoftObjectPtr<USoundBase>>& Clips, int32& InOutLastIndex) const
{
	if (Clips.Num() == 0)
	{
		return nullptr;
	}

	int32 Index = FMath::RandHelper(Clips.Num());

	if (Clips.Num() > 1 && Index == InOutLastIndex)
	{
		Index = (Index + 1 + FMath::RandHelper(Clips.Num() - 1)) % Clips.Num();
	}

	InOutLastIndex = Index;
	USoundBase* Sound = Clips[Index].LoadSynchronous();

	if (!Sound && !bWarnedMissingSounds)
	{
		bWarnedMissingSounds = true;
		UE_LOG(LogSourceMovement, Warning, TEXT("%s: '%s' did not load, so that sound is silent. Check the asset is at that exact path, " "or assign your own on the MovementAudio component."), *GetName(), *Clips[Index].ToSoftObjectPath().ToString());
	}

	return Sound;
}

void USourceMovementAudioComponent::PlayLocal(USoundBase* Sound, float Volume) const
{
	if (!Sound || Volume <= 0.f)
	{
		return;
	}

	UGameplayStatics::PlaySound2D(this, Sound, Volume);
}

void USourceMovementAudioComponent::HandleMovementEvents(const FSourceMovementEvents& Events)
{
	const FSourceSurfaceSounds* Surface = FindSurfaceSounds(Events.SurfacePropsId);

	// Per list, not per set, or a surface with only footsteps would silence landing and jumping
	const TArray<TSoftObjectPtr<USoundBase>>& Footsteps = Surface && Surface->Footsteps.Num() ? Surface->Footsteps : DefaultSounds.Footsteps;
	const TArray<TSoftObjectPtr<USoundBase>>& Land = Surface && Surface->Land.Num() ? Surface->Land : DefaultSounds.Land;
	const TSoftObjectPtr<USoundBase>& JumpLaunch = Surface && !Surface->JumpLaunch.IsNull() ? Surface->JumpLaunch : DefaultSounds.JumpLaunch;

	if (Events.bJumpSound)
	{
		PlayLocal(JumpLaunch.LoadSynchronous(), JumpVolumeScale);
	}

	if (Events.bStepSound)
	{
		PlayLocal(PickRandom(Footsteps, LastFootstepIndex), Events.StepVolume * FootstepVolumeScale);
	}

	if (Events.bLandSound)
	{
		const float Alpha = FMath::GetMappedRangeValueClamped(FVector2f(FSourceMovementParams::LandSoundFallVelocity, LandVelocityForFullVolume), FVector2f(0.35f, 1.0f), static_cast<float>(Events.LandVelocity));

		PlayLocal(PickRandom(Land, LastLandIndex), Alpha * LandVolumeScale);
		PlayLocal(PickRandom(Footsteps, LastFootstepIndex), Alpha * FootstepVolumeScale);
	}
}

void USourceMovementAudioComponent::UpdateSurfWind()
{
	if (!bEnableSurfWind || !Movement || SurfWindLoop.IsNull())
	{
		if (SurfWindAudio)
		{
			SurfWindAudio->Stop();
			SurfWindAudio = nullptr;
		}
		return;
	}

	const float Speed = Movement->GetSourceSpeed();
	const float MinSpeed = static_cast<float>(SurfWindSpeedRange.X);
	const float MaxSpeed = FMath::Max(static_cast<float>(SurfWindSpeedRange.Y), MinSpeed + 1.f);

	const float Alpha = FMath::GetMappedRangeValueClamped(FVector2f(MinSpeed, MaxSpeed), FVector2f(0.f, 1.f), Speed);

	if (!SurfWindAudio)
	{
		if (Alpha <= 0.f)
		{
			return;
		}

		USoundBase* Loop = SurfWindLoop.LoadSynchronous();
		if (!Loop)
		{
			return;
		}

		// bAutoDestroy=false: this component owns the bed and stops it in EndPlay
		SurfWindAudio = UGameplayStatics::SpawnSound2D(this, Loop, /*VolumeMultiplier=*/0.f, /*PitchMultiplier=*/1.f, /*StartTime=*/0.f, /*ConcurrencySettings=*/nullptr, /*bPersistAcrossLevelTransition=*/false, /*bAutoDestroy=*/false);

		if (!SurfWindAudio)
		{
			return;
		}

		if (!SurfWindAudio->Sound || !SurfWindAudio->Sound->IsLooping())
		{
			UE_LOG(LogSourceMovement, Warning, TEXT("%s: surf wind loop '%s' is not marked Looping, so it will play once and stop. " "Set Looping on the Sound Wave, or wrap it in a looping Sound Cue."), *GetName(), *GetNameSafe(Loop));
		}
	}

	SurfWindAudio->SetVolumeMultiplier(Alpha * SurfWindVolumeScale);
}
