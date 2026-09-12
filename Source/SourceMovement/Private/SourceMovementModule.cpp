#include "SourceMovementModule.h"

#include "Input/SourceBindManager.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Modules/ModuleManager.h"
#include "SourceMovementCVars.h"

DEFINE_LOG_CATEGORY(LogSourceMovement);

void FSourceMovementModule::StartupModule()
{
	SourceMovementCVars::RegisterCheatGate();

	FSourceBindManager::Get().Initialize();
}

void FSourceMovementModule::ShutdownModule()
{
	FSourceBindManager::Get().WriteConfig(TEXT("config.cfg"), *GLog);
}

// PRIMARY because this is the only game module; a monolithic build needs exactly one
IMPLEMENT_PRIMARY_GAME_MODULE(FSourceMovementModule, SourceMovement, "UCSourceCharacter")
