#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

SOURCEMOVEMENT_API DECLARE_LOG_CATEGORY_EXTERN(LogSourceMovement, Log, All);

class FSourceMovementModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
