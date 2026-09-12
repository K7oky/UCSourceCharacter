#pragma once

#include "CoreMinimal.h"
#include "Engine/LocalPlayer.h"

#include "SourceLocalPlayer.generated.h"

// Claims `exec` before ULocalPlayer::Exec does, which is the only place to take the name
UCLASS()
class SOURCEMOVEMENT_API USourceLocalPlayer : public ULocalPlayer
{
	GENERATED_BODY()

public:
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

private:
	void HandleNoClip(FOutputDevice& Ar);
};
