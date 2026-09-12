#include "Actors/SourceLocalPlayer.h"

#include "Actors/SourceMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Input/SourceBindManager.h"
#include "Misc/Parse.h"
#include "SourceMovementCVars.h"

bool USourceLocalPlayer::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	{
		const TCHAR* Stream = Cmd;
		if (FParse::Command(&Stream, TEXT("noclip")))
		{
			HandleNoClip(Ar);
			return true;
		}
	}

	if (FSourceBindManager::Get().HandleExec(Cmd, Ar))
	{
		return true;
	}

	return Super::Exec(InWorld, Cmd, Ar);
}

void USourceLocalPlayer::HandleNoClip(FOutputDevice& Ar)
{
	// noclip is a cheat in Source, so it needs sv_cheats 1
	if (!SourceMovementCVars::AreCheatsEnabled())
	{
		Ar.Logf(TEXT("Can't use cheat command noclip in multiplayer, unless the server has sv_cheats set to 1."));
		return;
	}

	APlayerController* PC = PlayerController;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	USourceMovementComponent* Movement = Pawn ? Pawn->FindComponentByClass<USourceMovementComponent>() : nullptr;

	if (!Movement)
	{
		Ar.Logf(TEXT("noclip: the possessed pawn has no USourceMovementComponent."));
		return;
	}

	FSourceMovementState& State = Movement->GetMutableMovementState();

	if (State.MoveType == ESrcMoveType::NoClip)
	{
		State.MoveType = ESrcMoveType::Walk;
		State.GroundEntity = FSrcEntityRef::None();

		Ar.Logf(TEXT("noclip OFF"));
	}
	else
	{
		// Noclip never runs the ground check, so clear the stale ground
		State.MoveType = ESrcMoveType::NoClip;
		State.GroundEntity = FSrcEntityRef::None();

		Ar.Logf(TEXT("noclip ON   (sv_noclipspeed %.0f x sv_maxspeed, sv_noclipaccelerate %.0f)"), Movement->GetParams().NoClipSpeed, Movement->GetParams().NoClipAccelerate);
	}
}
