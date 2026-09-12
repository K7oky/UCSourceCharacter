#include "Actors/SourcePlayerPawn.h"

#include "Actors/SourceHullComponent.h"
#include "Actors/SourceMovementAudioComponent.h"
#include "Actors/SourceMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Input/SourceBindManager.h"
#include "SourceMovementCVars.h"
#include "SourceUnits.h"

ASourcePlayerPawn::ASourcePlayerPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// A plain scene root, not a capsule: the solver sweeps its own axis-aligned hull
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	HullProxy = CreateDefaultSubobject<USourceHullComponent>(TEXT("HullProxy"));
	HullProxy->SetupAttachment(SceneRoot);
	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(SceneRoot);
	ViewCamera->bUsePawnControlRotation = false;
	SourceMovement = CreateDefaultSubobject<USourceMovementComponent>(TEXT("SourceMovement"));
	SourceMovement->SetUpdatedComponent(SceneRoot);
	MovementAudio = CreateDefaultSubobject<USourceMovementAudioComponent>(TEXT("MovementAudio"));

	// Unreal's rotation plumbing is bypassed entirely, the simulation owns the view angles
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
}

UPawnMovementComponent* ASourcePlayerPawn::GetMovementComponent() const
{
	return SourceMovement;
}

void ASourcePlayerPawn::BeginPlay()
{
	Super::BeginPlay();

	if (SourceMovement)
	{
		SourceMovement->InputProvider = [](FSourceInputState& OutInput)
		{
			FSourceBindManager::Get().BuildInputState(OutInput);
		};
	}

	// Bound commands must run as this player, or `noclip` never reaches a pawn
	TWeakObjectPtr<ASourcePlayerPawn> WeakSelf(this);
	FSourceBindManager::Get().CommandExecutor = [WeakSelf](const FString& Command)
	{
		if (const ASourcePlayerPawn* Self = WeakSelf.Get())
		{
			if (APlayerController* PC = Cast<APlayerController>(Self->GetController()))
			{
				PC->ConsoleCommand(Command, true);
				return;
			}
		}

		if (GEngine)
		{
			GEngine->Exec(nullptr, *Command);
		}
	};

	UpdateView();
}

void ASourcePlayerPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The bind manager outlives the level, so a key held at teardown would stay down next session
	FSourceBindManager::Get().ReleaseAllButtons();

	if (SourceMovement)
	{
		SourceMovement->InputProvider = nullptr;
	}

	FSourceBindManager::Get().CommandExecutor = nullptr;

	Super::EndPlay(EndPlayReason);
}

void ASourcePlayerPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateView();
}

void ASourcePlayerPawn::UpdateView()
{
	if (!SourceMovement || !ViewCamera)
	{
		return;
	}

	const FSourceMovementState& State = SourceMovement->GetMovementState();

	ViewCamera->SetRelativeLocation(SourceUnits::ToUEPosition(State.ViewOffset));

	// Punch is added for rendering only and never written back, so it cannot drift the aim
	const FSrcAngles ViewWithPunch = State.ViewAngles + State.m_viewPunchAngle;
	ViewCamera->SetRelativeRotation(SourceUnits::ToUERotator(ViewWithPunch));
}

void ASourcePlayerPawn::SetForwardInput(float Value)
{
	if (SourceMovement)
	{
		SourceMovement->GetMutableInputState().ForwardMove = static_cast<srcfloat>(FMath::Clamp(Value, -1.f, 1.f));
	}
}

void ASourcePlayerPawn::SetSideInput(float Value)
{
	if (SourceMovement)
	{
		SourceMovement->GetMutableInputState().SideMove = static_cast<srcfloat>(FMath::Clamp(Value, -1.f, 1.f));
	}
}

void ASourcePlayerPawn::SetUpInput(float Value)
{
	if (SourceMovement)
	{
		SourceMovement->GetMutableInputState().UpMove = static_cast<srcfloat>(FMath::Clamp(Value, -1.f, 1.f));
	}
}

void ASourcePlayerPawn::SetJumpHeld(bool bHeld)
{
	if (SourceMovement)
	{
		SourceMovement->GetMutableInputState().bJump = bHeld;
	}
}

void ASourcePlayerPawn::SetDuckHeld(bool bHeld)
{
	if (SourceMovement)
	{
		SourceMovement->GetMutableInputState().bDuck = bHeld;
	}
}

void ASourcePlayerPawn::SetWalkHeld(bool bHeld)
{
	if (SourceMovement)
	{
		SourceMovement->GetMutableInputState().bWalk = bHeld;
	}
}

void ASourcePlayerPawn::ClampViewAngles(FSrcAngles& Angles)
{
	const srcfloat PitchDown = static_cast<srcfloat>(SourceMovementCVars::GetPitchDown());
	const srcfloat PitchUp = static_cast<srcfloat>(SourceMovementCVars::GetPitchUp());

	// cl_pitchdown is the positive limit and cl_pitchup the negative one, as in Source
	if (Angles[SRC_PITCH] > PitchDown)
	{
		Angles[SRC_PITCH] = PitchDown;
	}
	if (Angles[SRC_PITCH] < -PitchUp)
	{
		Angles[SRC_PITCH] = -PitchUp;
	}

	if (Angles[SRC_ROLL] > 50.f)
	{
		Angles[SRC_ROLL] = 50.f;
	}
	if (Angles[SRC_ROLL] < -50.f)
	{
		Angles[SRC_ROLL] = -50.f;
	}

	Angles[SRC_YAW] = SourceMath::AngleNormalize(Angles[SRC_YAW]);
}

void ASourcePlayerPawn::AddViewInput(float PitchDelta, float YawDelta)
{
	if (!SourceMovement)
	{
		return;
	}

	FSourceInputState& Input = SourceMovement->GetMutableInputState();
	Input.ViewAngles[SRC_PITCH] += static_cast<srcfloat>(PitchDelta);
	Input.ViewAngles[SRC_YAW] += static_cast<srcfloat>(YawDelta);

	ClampViewAngles(Input.ViewAngles);
}

void ASourcePlayerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!PlayerInputComponent)
	{
		return;
	}

	PlayerInputComponent->BindAxis(TEXT("SrcTurn"), this, &ASourcePlayerPawn::Input_Turn);
	PlayerInputComponent->BindAxis(TEXT("SrcLookUp"), this, &ASourcePlayerPawn::Input_LookUp);

	// One handler for every key; binding named Unreal actions would make `bind` a no-op
	TArray<FKey> AllKeys;
	EKeys::GetAllKeys(AllKeys);

	for (const FKey& Key : AllKeys)
	{
		if (!Key.IsValid() || Key.IsAxis1D() || Key.IsAxis2D() || Key.IsAxis3D())
		{
			continue;
		}
		if (Key.IsGamepadKey() || Key.IsTouch())
		{
			continue;
		}

		PlayerInputComponent->BindKey(Key, IE_Pressed, this, &ASourcePlayerPawn::OnAnyKeyPressed);
		PlayerInputComponent->BindKey(Key, IE_Released, this, &ASourcePlayerPawn::OnAnyKeyReleased);
	}
}

void ASourcePlayerPawn::OnAnyKeyPressed(FKey Key)
{
	FSourceBindManager::Get().OnKeyEvent(Key, true);
}

void ASourcePlayerPawn::OnAnyKeyReleased(FKey Key)
{
	FSourceBindManager::Get().OnKeyEvent(Key, false);
}

namespace
{
	// Unreal pre-scales the legacy mouse axes by this; divide it out to get raw counts
	constexpr float UELegacyMouseAxisSensitivity = 0.07f;
}

void ASourcePlayerPawn::Input_Turn(float Value)
{
	const float Counts = Value / UELegacyMouseAxisSensitivity;
	const float Degrees = Counts * SourceMovementCVars::GetMouseYawFactor() * SourceMovementCVars::GetSensitivity();

	AddViewInput(0.f, -Degrees);
}

void ASourcePlayerPawn::Input_LookUp(float Value)
{
	const float Counts = Value / UELegacyMouseAxisSensitivity;
	const float Degrees = Counts * SourceMovementCVars::GetMousePitchFactor() * SourceMovementCVars::GetSensitivity();
	const float Sign = bInvertMouseY ? 1.f : -1.f;
	AddViewInput(Sign * Degrees, 0.f);
}

void ASourcePlayerPawn::SetSourceViewAngles(float Pitch, float Yaw, float Roll)
{
	if (!SourceMovement)
	{
		return;
	}

	FSourceInputState& Input = SourceMovement->GetMutableInputState();
	Input.ViewAngles = FSrcAngles(static_cast<srcfloat>(Pitch), static_cast<srcfloat>(Yaw), static_cast<srcfloat>(Roll));

	ClampViewAngles(Input.ViewAngles);
}
