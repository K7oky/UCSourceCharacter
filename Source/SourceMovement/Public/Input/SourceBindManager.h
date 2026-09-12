#pragma once

#include "CoreMinimal.h"
#include "Input/SourceInputTranslator.h"
#include "Input/SourceKButton.h"
#include "InputCoreTypes.h"

class SOURCEMOVEMENT_API FSourceBindManager
{
public:
	static FSourceBindManager& Get();

	void Initialize();
	void OnKeyEvent(const FKey& Key, bool bPressed);
	void BuildInputState(FSourceInputState& OutInput);

	bool HandleExec(const TCHAR* Cmd, FOutputDevice& Ar);
	void Bind(const FString& KeyName, const FString& Command, FOutputDevice& Ar);
	void Unbind(const FString& KeyName, FOutputDevice& Ar);
	void UnbindAll(FOutputDevice& Ar);
	void ListBinds(FOutputDevice& Ar) const;
	void ApplyDefaultBinds(FOutputDevice& Ar);
	void ReleaseAllButtons();

	static FKey KeyFromSourceName(const FString& Name);
	static FString SourceNameFromKey(const FKey& Key);

	void ExecuteBindCommand(const FString& CommandLine, bool bPressed, int32 KeyId);

	// How a bound plain command runs. The pawn supplies it, so `noclip` reaches a pawn.
	TFunction<void(const FString&)> CommandExecutor;

	static FString GetConfigPath(const FString& FileName);
	bool ExecConfig(const FString& FileName, FOutputDevice& Ar);
	bool WriteConfig(const FString& FileName, FOutputDevice& Ar);

	// Runs config.cfg then autoexec.cfg, in that order
	void ExecStartupConfigs();

private:
	FSourceBindManager() = default;

	void RegisterConsoleCommands();
	void RegisterButtons();
	bool ExecuteCommand(const FString& Verb, const TArray<FString>& Args, FOutputDevice& Ar);
	bool DispatchButtonCommand(const FString& Token, bool bPressed, int32 KeyId);

	// Any stable per-key integer will do
	static int32 KeyIdOf(const FKey& Key);

	TMap<FKey, FString> Binds;

	// "+forward" (stored without the sign) -> button
	TMap<FString, FSourceKButton*> ButtonsByName;

	// Backing storage for the alias command names
	TArray<FString> AliasNameStorage;
	bool bInitialized = false;
	bool bStartupConfigsExecuted = false;

	// Guards against a config that execs itself
	int32 ExecDepth = 0;

public:
	FSourceKButton in_forward;
	FSourceKButton in_back;
	FSourceKButton in_moveleft;
	FSourceKButton in_moveright;
	FSourceKButton in_left;
	FSourceKButton in_right;
	FSourceKButton in_up;
	FSourceKButton in_down;
	FSourceKButton in_strafe;
	FSourceKButton in_klook;
	FSourceKButton in_speed;
	FSourceKButton in_jump;
	FSourceKButton in_duck;
	FSourceKButton in_walk;

	// Registered so binds resolve and do not error, but not consumed by the movement pipeline
	FSourceKButton in_score;
	FSourceKButton in_zoom;
	FSourceKButton in_graph;
	FSourceKButton in_break;
	FSourceKButton in_jlook;
	FSourceKButton in_lookup;
	FSourceKButton in_lookdown;
	FSourceKButton in_lookspin;
	FSourceKButton in_alt1;
	FSourceKButton in_alt2;
	FSourceKButton in_grenade1;
	FSourceKButton in_grenade2;
};
