#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "SourceMovementGameModeBase.generated.h"

UCLASS(Blueprintable, meta = (DisplayName = "Source Movement Game Mode"))
class SOURCEMOVEMENT_API ASourceMovementGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASourceMovementGameModeBase();
};
