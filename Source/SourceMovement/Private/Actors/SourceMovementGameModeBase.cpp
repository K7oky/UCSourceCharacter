#include "Actors/SourceMovementGameModeBase.h"

#include "Actors/SourcePlayerPawn.h"

ASourceMovementGameModeBase::ASourceMovementGameModeBase()
{
	DefaultPawnClass = ASourcePlayerPawn::StaticClass();
}
