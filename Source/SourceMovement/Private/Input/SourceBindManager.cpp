#include "Input/SourceBindManager.h"

#include "SourceMovementCVars.h"
#include "SourceMovementModule.h"

#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

FSourceBindManager& FSourceBindManager::Get()
{
	static FSourceBindManager Instance;
	return Instance;
}

int32 FSourceBindManager::KeyIdOf(const FKey& Key)
{
	// Any stable per-key integer works here
	const int32 Hash = static_cast<int32>(GetTypeHash(Key.GetFName()));
	return Hash == 0 ? 1 : Hash;
}

namespace
{
	struct FKeyNamePair
	{
		const TCHAR* SourceName;
		FKey Key;
	};

	const TArray<FKeyNamePair>& GetKeyNameTable()
	{
		static const TArray<FKeyNamePair> Table = {
			// Letters and digits use their literal character in Source, same as Unreal
			{ TEXT("SPACE"), EKeys::SpaceBar }, { TEXT("ENTER"), EKeys::Enter },
			{ TEXT("TAB"), EKeys::Tab }, { TEXT("ESCAPE"), EKeys::Escape },
			{ TEXT("BACKSPACE"), EKeys::BackSpace }, { TEXT("CAPSLOCK"), EKeys::CapsLock },

			{ TEXT("CTRL"), EKeys::LeftControl }, { TEXT("RCTRL"), EKeys::RightControl },
			{ TEXT("SHIFT"), EKeys::LeftShift }, { TEXT("RSHIFT"), EKeys::RightShift },
			{ TEXT("ALT"), EKeys::LeftAlt }, { TEXT("RALT"), EKeys::RightAlt },

			{ TEXT("UPARROW"), EKeys::Up }, { TEXT("DOWNARROW"), EKeys::Down },
			{ TEXT("LEFTARROW"), EKeys::Left }, { TEXT("RIGHTARROW"), EKeys::Right },

			{ TEXT("INS"), EKeys::Insert }, { TEXT("DEL"), EKeys::Delete },
			{ TEXT("HOME"), EKeys::Home }, { TEXT("END"), EKeys::End },
			{ TEXT("PGUP"), EKeys::PageUp }, { TEXT("PGDN"), EKeys::PageDown },

			{ TEXT("MOUSE1"), EKeys::LeftMouseButton }, { TEXT("MOUSE2"), EKeys::RightMouseButton },
			{ TEXT("MOUSE3"), EKeys::MiddleMouseButton }, { TEXT("MOUSE4"), EKeys::ThumbMouseButton },
			{ TEXT("MOUSE5"), EKeys::ThumbMouseButton2 }, { TEXT("MWHEELUP"), EKeys::MouseScrollUp },
			{ TEXT("MWHEELDOWN"), EKeys::MouseScrollDown },

			{ TEXT("SEMICOLON"), EKeys::Semicolon }, { TEXT("["), EKeys::LeftBracket },
			{ TEXT("]"), EKeys::RightBracket }, { TEXT("'"), EKeys::Apostrophe },
			{ TEXT(","), EKeys::Comma }, { TEXT("."), EKeys::Period },
			{ TEXT("/"), EKeys::Slash }, { TEXT("\\"), EKeys::Backslash },
			{ TEXT("-"), EKeys::Hyphen }, { TEXT("="), EKeys::Equals },
			{ TEXT("`"), EKeys::Tilde },

			{ TEXT("KP_INS"), EKeys::NumPadZero }, { TEXT("KP_END"), EKeys::NumPadOne },
			{ TEXT("KP_DOWNARROW"), EKeys::NumPadTwo }, { TEXT("KP_PGDN"), EKeys::NumPadThree },
			{ TEXT("KP_LEFTARROW"), EKeys::NumPadFour }, { TEXT("KP_5"), EKeys::NumPadFive },
			{ TEXT("KP_RIGHTARROW"), EKeys::NumPadSix }, { TEXT("KP_HOME"), EKeys::NumPadSeven },
			{ TEXT("KP_UPARROW"), EKeys::NumPadEight }, { TEXT("KP_PGUP"), EKeys::NumPadNine },
			{ TEXT("KP_ENTER"), EKeys::Enter }, { TEXT("KP_SLASH"), EKeys::Divide },
			{ TEXT("KP_MULTIPLY"), EKeys::Multiply }, { TEXT("KP_MINUS"), EKeys::Subtract },
			{ TEXT("KP_PLUS"), EKeys::Add }, { TEXT("KP_DEL"), EKeys::Decimal }, };
		return Table;
	}
}

FKey FSourceBindManager::KeyFromSourceName(const FString& InName)
{
	const FString Name = InName.TrimStartAndEnd().TrimQuotes();

	for (const FKeyNamePair& Pair : GetKeyNameTable())
	{
		if (Name.Equals(Pair.SourceName, ESearchCase::IgnoreCase))
		{
			return Pair.Key;
		}
	}

	// F1..F12
	if (Name.Len() >= 2 && (Name[0] == TEXT('f') || Name[0] == TEXT('F')))
	{
		const FString Digits = Name.Mid(1);
		if (Digits.IsNumeric())
		{
			const int32 Index = FCString::Atoi(*Digits);
			if (Index >= 1 && Index <= 12)
			{
				static const FKey FKeysTable[12] = {
					EKeys::F1, EKeys::F2, EKeys::F3, EKeys::F4, EKeys::F5, EKeys::F6,
					EKeys::F7, EKeys::F8, EKeys::F9, EKeys::F10, EKeys::F11, EKeys::F12
				};
				return FKeysTable[Index - 1];
			}
		}
	}

	// Single characters: letters and digits map straight onto Unreal's key names
	if (Name.Len() == 1)
	{
		const TCHAR C = FChar::ToUpper(Name[0]);
		if (C >= TEXT('A') && C <= TEXT('Z'))
		{
			return FKey(FName(*FString::Chr(C)));
		}
		if (C >= TEXT('0') && C <= TEXT('9'))
		{
			static const FKey Digits[10] = {
				EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
				EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine
			};
			return Digits[C - TEXT('0')];
		}
	}

	// Fallback: an Unreal key name, so anything the table misses is still bindable
	const FName KeyFName(*Name);
	const FKey AsUnrealKey(KeyFName);
	if (AsUnrealKey.IsValid())
	{
		return AsUnrealKey;
	}

	return FKey();
}

FString FSourceBindManager::SourceNameFromKey(const FKey& Key)
{
	for (const FKeyNamePair& Pair : GetKeyNameTable())
	{
		if (Pair.Key == Key)
		{
			return Pair.SourceName;
		}
	}
	return Key.GetFName().ToString();
}

void FSourceBindManager::RegisterButtons()
{
	// Names are stored WITHOUT the sign; the sign selects KeyDown vs KeyUp
	ButtonsByName.Add(TEXT("forward"), &in_forward);
	ButtonsByName.Add(TEXT("back"), &in_back);
	ButtonsByName.Add(TEXT("moveleft"), &in_moveleft);
	ButtonsByName.Add(TEXT("moveright"), &in_moveright);
	ButtonsByName.Add(TEXT("left"), &in_left);
	ButtonsByName.Add(TEXT("right"), &in_right);
	ButtonsByName.Add(TEXT("moveup"), &in_up);
	ButtonsByName.Add(TEXT("movedown"), &in_down);
	ButtonsByName.Add(TEXT("strafe"), &in_strafe);
	ButtonsByName.Add(TEXT("klook"), &in_klook);
	ButtonsByName.Add(TEXT("speed"), &in_speed);
	ButtonsByName.Add(TEXT("jump"), &in_jump);
	ButtonsByName.Add(TEXT("duck"), &in_duck);
	ButtonsByName.Add(TEXT("walk"), &in_walk);

	ButtonsByName.Add(TEXT("score"), &in_score);
	ButtonsByName.Add(TEXT("showscores"), &in_score);  // Source aliases both onto in_score
	ButtonsByName.Add(TEXT("zoom"), &in_zoom);
	ButtonsByName.Add(TEXT("graph"), &in_graph);
	ButtonsByName.Add(TEXT("break"), &in_break);
	ButtonsByName.Add(TEXT("jlook"), &in_jlook);
	ButtonsByName.Add(TEXT("lookup"), &in_lookup);
	ButtonsByName.Add(TEXT("lookdown"), &in_lookdown);
	ButtonsByName.Add(TEXT("lookspin"), &in_lookspin);
	ButtonsByName.Add(TEXT("alt1"), &in_alt1);
	ButtonsByName.Add(TEXT("alt2"), &in_alt2);
	ButtonsByName.Add(TEXT("grenade1"), &in_grenade1);
	ButtonsByName.Add(TEXT("grenade2"), &in_grenade2);
}

bool FSourceBindManager::DispatchButtonCommand(const FString& Token, bool bPressed, int32 KeyId)
{
	if (Token.IsEmpty())
	{
		return false;
	}

	const TCHAR Sign = Token[0];
	if (Sign != TEXT('+') && Sign != TEXT('-'))
	{
		return false;
	}

	// The bind only ever names the '+' half, and the engine synthesises the '-' on release
	const FString Name = Token.Mid(1).TrimStartAndEnd();
	FSourceKButton** Found = ButtonsByName.Find(Name);
	if (!Found || !*Found)
	{
		UE_LOG(LogSourceMovement, Warning, TEXT("Unknown command: %s"), *Token);
		return true;  // it looked like an alias, so do not fall through to the engine console
	}

	const bool bWantDown = (Sign == TEXT('+')) ? bPressed : !bPressed;

	if (bWantDown)
	{
		(*Found)->KeyDown(KeyId);
	}
	else
	{
		(*Found)->KeyUp(KeyId);
	}

	return true;
}

void FSourceBindManager::ExecuteBindCommand(const FString& CommandLine, bool bPressed, int32 KeyId)
{
	// Source allows several commands in one bind, separated by semicolons
	TArray<FString> Tokens;
	CommandLine.ParseIntoArray(Tokens, TEXT(";"), true);

	for (FString Token : Tokens)
	{
		Token = Token.TrimStartAndEnd();
		if (Token.IsEmpty())
		{
			continue;
		}

		if (DispatchButtonCommand(Token, bPressed, KeyId))
		{
			continue;
		}

		if (!bPressed)
		{
			continue;
		}

		if (CommandExecutor)
		{
			CommandExecutor(Token);
		}
		else if (GEngine)
		{
			// No pawn has claimed the executor yet - a bind fired from a startup config, say
			GEngine->Exec(nullptr, *Token);
		}
	}
}

void FSourceBindManager::OnKeyEvent(const FKey& Key, bool bPressed)
{
	const FString* Command = Binds.Find(Key);
	if (!Command)
	{
		return;
	}

	ExecuteBindCommand(*Command, bPressed, KeyIdOf(Key));

	// The mouse wheel has no release event in Unreal - it is reported as a single press
	if (bPressed && (Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown))
	{
		ExecuteBindCommand(*Command, false, KeyIdOf(Key));
	}
}

void FSourceBindManager::ReleaseAllButtons()
{
	for (TPair<FString, FSourceKButton*>& Pair : ButtonsByName)
	{
		if (Pair.Value)
		{
			Pair.Value->Reset();
		}
	}
}

void FSourceBindManager::BuildInputState(FSourceInputState& OutInput)
{
	srcfloat Forward = 0.f;
	srcfloat Side = 0.f;
	srcfloat Up = 0.f;

	// +klook suppresses the forward/back contribution entirely
	if (!in_klook.IsHeld())
	{
		Forward += in_forward.KeyState();
		Forward -= in_back.KeyState();
	}
	else
	{
		// Still consume the impulses so they do not leak into the next command
		in_forward.KeyState();
		in_back.KeyState();
	}

	// While +strafe is held, +left and +right act as +moveleft and +moveright
	if (in_strafe.IsHeld())
	{
		Side += in_right.KeyState();
		Side -= in_left.KeyState();
	}
	else
	{
		in_right.KeyState();
		in_left.KeyState();
	}

	Side += in_moveright.KeyState();
	Side -= in_moveleft.KeyState();
	Up += in_up.KeyState();
	Up -= in_down.KeyState();
	OutInput.ForwardMove = FMath::Clamp(Forward, -1.f, 1.f);
	OutInput.SideMove = FMath::Clamp(Side, -1.f, 1.f);
	OutInput.UpMove = FMath::Clamp(Up, -1.f, 1.f);

	auto ButtonBit = [](FSourceKButton& Button)
	{
		const bool bSet = Button.GetButtonBit();
		Button.ClearButtonImpulse();
		return bSet;
	};
	OutInput.bJump = ButtonBit(in_jump);
	OutInput.bDuck = ButtonBit(in_duck);
	OutInput.bWalk = ButtonBit(in_speed);

	// in_walk (+walk) is deliberately NOT mapped to the walk modifier
}

void FSourceBindManager::Bind(const FString& KeyName, const FString& Command, FOutputDevice& Ar)
{
	const FKey Key = KeyFromSourceName(KeyName);
	if (!Key.IsValid())
	{
		Ar.Logf(TEXT("bind: unknown key \"%s\""), *KeyName);
		return;
	}

	const FString Clean = Command.TrimStartAndEnd().TrimQuotes();
	Binds.Add(Key, Clean);

	Ar.Logf(TEXT("bind \"%s\" \"%s\""), *SourceNameFromKey(Key), *Clean);
}

void FSourceBindManager::Unbind(const FString& KeyName, FOutputDevice& Ar)
{
	const FKey Key = KeyFromSourceName(KeyName);
	if (!Key.IsValid())
	{
		Ar.Logf(TEXT("unbind: unknown key \"%s\""), *KeyName);
		return;
	}

	// Release whatever the key was holding, otherwise unbinding a held key leaves it stuck down
	if (const FString* Command = Binds.Find(Key))
	{
		ExecuteBindCommand(*Command, false, KeyIdOf(Key));
	}

	Binds.Remove(Key);
	Ar.Logf(TEXT("unbind \"%s\""), *SourceNameFromKey(Key));
}

void FSourceBindManager::UnbindAll(FOutputDevice& Ar)
{
	Binds.Reset();
	ReleaseAllButtons();
	Ar.Logf(TEXT("All binds cleared."));
}

void FSourceBindManager::ListBinds(FOutputDevice& Ar) const
{
	Ar.Logf(TEXT("bind list (%d)"), Binds.Num());
	for (const TPair<FKey, FString>& Pair : Binds)
	{
		Ar.Logf(TEXT("bind \"%s\" \"%s\""), *SourceNameFromKey(Pair.Key), *Pair.Value);
	}
}

void FSourceBindManager::ApplyDefaultBinds(FOutputDevice& Ar)
{
	Bind(TEXT("w"), TEXT("+forward"), Ar);
	Bind(TEXT("s"), TEXT("+back"), Ar);
	Bind(TEXT("a"), TEXT("+moveleft"), Ar);
	Bind(TEXT("d"), TEXT("+moveright"), Ar);
	Bind(TEXT("SPACE"), TEXT("+jump"), Ar);
	Bind(TEXT("CTRL"), TEXT("+duck"), Ar);
	Bind(TEXT("SHIFT"), TEXT("+speed"), Ar);
	Bind(TEXT("TAB"), TEXT("+showscores"), Ar);
}

// Config files

FString FSourceBindManager::GetConfigPath(const FString& FileName)
{
	FString Name = FileName.TrimStartAndEnd().TrimQuotes();

	// Source's exec takes a bare name and appends .cfg
	if (!Name.EndsWith(TEXT(".cfg"), ESearchCase::IgnoreCase))
	{
		Name += TEXT(".cfg");
	}

	// Refuse to escape the cfg directory
	Name = FPaths::GetCleanFilename(Name);
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("cfg"));

	IFileManager::Get().MakeDirectory(*Dir, /*Tree=*/true);
	return Dir / Name;
}

bool FSourceBindManager::ExecConfig(const FString& FileName, FOutputDevice& Ar)
{
	const FString Path = GetConfigPath(FileName);

	if (ExecDepth > 8)
	{
		Ar.Logf(TEXT("exec: too many nested execs, refusing to run \"%s\" (a config execs itself?)"), *Path);
		return false;
	}

	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *Path))
	{
		Ar.Logf(TEXT("exec: couldn't open \"%s\""), *Path);
		return false;
	}

	++ExecDepth;
	int32 Executed = 0;
	for (FString Line : Lines)
	{
		// Strip `//` comments, the way Source's config parser does
		const int32 DoubleSlash = Line.Find(TEXT("//"));
		if (DoubleSlash != INDEX_NONE)
		{
			Line = Line.Left(DoubleSlash);
		}

		Line = Line.TrimStartAndEnd();
		if (Line.IsEmpty())
		{
			continue;
		}

		if (!HandleExec(*Line, Ar) && !IConsoleManager::Get().ProcessUserConsoleInput(*Line, Ar, nullptr))
		{
			Ar.Logf(TEXT("exec: unknown command \"%s\""), *Line);
		}
		++Executed;
	}

	--ExecDepth;

	if (Executed == 0)
	{
		Ar.Logf(TEXT("exec: \"%s\" has no active lines (every line is blank or a // comment)"), *Path);
	}
	else
	{
		Ar.Logf(TEXT("exec: ran %d line(s) from \"%s\""), Executed, *Path);
	}
	return true;
}

bool FSourceBindManager::WriteConfig(const FString& FileName, FOutputDevice& Ar)
{
	const FString Path = GetConfigPath(FileName);
	FString Out;
	Out += TEXT("// Generated by host_writeconfig, SourceMovement\n");
	Out += TEXT("// Regenerated automatically. Put hand-written settings in autoexec.cfg instead,\n");
	Out += TEXT("// which runs after this file and is never overwritten.\n\n");
	Out += TEXT("unbindall\n");
	for (const TPair<FKey, FString>& Pair : Binds)
	{
		Out += FString::Printf(TEXT("bind \"%s\" \"%s\"\n"), *SourceNameFromKey(Pair.Key), *Pair.Value);
	}

	TArray<SourceMovementCVars::FPersistableCVar> CVars;
	SourceMovementCVars::GetPersistableCVars(CVars);

	// Ungated first: these apply unconditionally
	FString Ungated;
	FString Gated;
	for (const SourceMovementCVars::FPersistableCVar& Var : CVars)
	{
		if (Var.IsAtDefault())
		{
			continue;  // keep the file short and readable
		}

		const FString Line = FString::Printf(TEXT("%s \"%s\"\n"), *Var.Name, *Var.Value);
		if (Var.bIsCheatGated)
		{
			Gated += Line;
		}
		else
		{
			Ungated += Line;
		}
	}

	if (!Ungated.IsEmpty())
	{
		Out += TEXT("\n// settings\n");
		Out += Ungated;
	}

	if (!Gated.IsEmpty())
	{
		// Only live while sv_cheats is 1; the gate reverts them when it goes to 0
		Out += TEXT("\n// cheat-protected, requires sv_cheats 1, reverted when it returns to 0\n");
		Out += TEXT("sv_cheats \"1\"\n");
		Out += Gated;
	}

	if (!FFileHelper::SaveStringToFile(Out, *Path))
	{
		Ar.Logf(TEXT("host_writeconfig: couldn't write \"%s\""), *Path);
		return false;
	}

	Ar.Logf(TEXT("host_writeconfig: wrote \"%s\""), *Path);
	return true;
}

void FSourceBindManager::ExecStartupConfigs()
{
	if (bStartupConfigsExecuted)
	{
		return;
	}
	bStartupConfigsExecuted = true;

	// config.cfg first, so anything in autoexec.cfg wins, same order as Source
	ExecConfig(TEXT("config.cfg"), *GLog);
	const FString AutoexecPath = GetConfigPath(TEXT("autoexec.cfg"));
	if (!FPaths::FileExists(AutoexecPath))
	{
		// Seed a commented template rather than leaving the player to guess the syntax
		const FString Template = TEXT("// autoexec.cfg, your own settings. Runs after config.cfg and is never\n")
			TEXT("// overwritten, so anything set here wins.\n")
			TEXT("//\n")
			TEXT("// bind \"z\" \"+jump\"\n")
			TEXT("// bind \"MOUSE4\" \"+duck\"\n")
			TEXT("// sensitivity \"1.8\"\n")
			TEXT("//\n")
			TEXT("// Movement variables are cheat-protected, so set sv_cheats first:\n")
			TEXT("// sv_cheats \"1\"\n")
			TEXT("// sv_enablebunnyhopping \"1\"\n");

		FFileHelper::SaveStringToFile(Template, *AutoexecPath);
		UE_LOG(LogSourceMovement, Log, TEXT("Created template \"%s\""), *AutoexecPath);
	}

	ExecConfig(TEXT("autoexec.cfg"), *GLog);
}

// Console commands

namespace
{
	void TokenizeCommandLine(const TCHAR* Stream, TArray<FString>& OutTokens)
	{
		FString Token;
		while (FParse::Token(Stream, Token, false))
		{
			Token.TrimStartAndEndInline();
			if (!Token.IsEmpty())
			{
				OutTokens.Add(MoveTemp(Token));
			}
		}
	}

	FString JoinTail(const TArray<FString>& Args, int32 FirstIndex)
	{
		FString Result;
		for (int32 Index = FirstIndex; Index < Args.Num(); ++Index)
		{
			if (Index > FirstIndex)
			{
				Result += TEXT(" ");
			}
			Result += Args[Index];
		}
		return Result;
	}
}

bool FSourceBindManager::HandleExec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	TArray<FString> Tokens;
	TokenizeCommandLine(Cmd, Tokens);

	if (Tokens.Num() == 0)
	{
		return false;
	}

	// The verb is taken as a whole token rather than matched as a prefix
	const FString Verb = Tokens[0];
	Tokens.RemoveAt(0);

	return ExecuteCommand(Verb, Tokens, Ar);
}

bool FSourceBindManager::ExecuteCommand(const FString& Verb, const TArray<FString>& Args, FOutputDevice& Ar)
{
	auto Is = [&Verb](const TCHAR* Name)
	{
		return Verb.Equals(Name, ESearchCase::IgnoreCase);
	};

	if (Is(TEXT("bind")))
	{
		if (Args.Num() == 0)
		{
			ListBinds(Ar);
			return true;
		}

		if (Args.Num() == 1)
		{
			// `bind <key>` with no command prints what the key currently does
			const FKey Key = KeyFromSourceName(Args[0]);
			if (const FString* Command = Binds.Find(Key))
			{
				Ar.Logf(TEXT("\"%s\" = \"%s\""), *SourceNameFromKey(Key), **Command);
			}
			else
			{
				Ar.Logf(TEXT("\"%s\" is not bound"), *Args[0]);
			}
			return true;
		}

		// Everything after the key is the command, so an unquoted `bind x +jump; +duck` still works
		Bind(Args[0], JoinTail(Args, 1), Ar);
		return true;
	}

	if (Is(TEXT("unbind")))
	{
		if (Args.Num() >= 1)
		{
			Unbind(Args[0], Ar);
		}
		else
		{
			Ar.Logf(TEXT("usage: unbind <key>"));
		}
		return true;
	}

	if (Is(TEXT("unbindall")))
	{
		UnbindAll(Ar);
		return true;
	}

	if (Is(TEXT("bindlist")) || Is(TEXT("key_listboundkeys")))
	{
		ListBinds(Ar);
		return true;
	}

	if (Is(TEXT("bind_defaults")))
	{
		ApplyDefaultBinds(Ar);
		return true;
	}

	if (Is(TEXT("exec")))
	{
		if (Args.Num() >= 1)
		{
			ExecConfig(Args[0], Ar);
		}
		else
		{
			Ar.Logf(TEXT("usage: exec <file>   (configs live in %s)"), *FPaths::GetPath(GetConfigPath(TEXT("autoexec.cfg"))));
		}
		return true;
	}

	if (Is(TEXT("echo")))
	{
		// Source has `echo`; Unreal does not
		Ar.Logf(TEXT("%s"), *JoinTail(Args, 0));
		return true;
	}

	if (Is(TEXT("host_writeconfig")) || Is(TEXT("writeconfig")))
	{
		WriteConfig(Args.Num() >= 1 ? Args[0] : FString(TEXT("config.cfg")), Ar);
		return true;
	}

	// A +/- alias typed straight into the console, which is legal in CS:GO
	if (Verb.Len() > 1 && (Verb[0] == TEXT('+') || Verb[0] == TEXT('-')))
	{
		const FString Name = Verb.Mid(1);
		if (ButtonsByName.Contains(Name))
		{
			ExecuteBindCommand(TEXT("+") + Name, Verb[0] == TEXT('+'), 0);
			return true;
		}
	}

	return false;
}

void FSourceBindManager::RegisterConsoleCommands()
{
	IConsoleManager& Console = IConsoleManager::Get();

	auto RegisterVerb = [&Console](const TCHAR* Name, const TCHAR* Help)
	{
		Console.RegisterConsoleCommand(Name, Help, FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateLambda([VerbName = FString(Name)](const TArray<FString>& Args, FOutputDevice& Ar) { FSourceBindManager::Get().ExecuteCommand(VerbName, Args, Ar); }));
	};

	RegisterVerb(TEXT("bind"), TEXT("bind <key> <command>, e.g. bind \"z\" \"+jump\". With no command, prints the current bind."));
	RegisterVerb(TEXT("unbind"), TEXT("unbind <key>"));
	RegisterVerb(TEXT("unbindall"), TEXT("unbindall, clears every binding."));
	RegisterVerb(TEXT("bindlist"), TEXT("bindlist, prints every binding."));
	RegisterVerb(TEXT("key_listboundkeys"), TEXT("key_listboundkeys, alias of bindlist."));
	RegisterVerb(TEXT("bind_defaults"), TEXT("bind_defaults, restores the CS:GO default bindings."));
	RegisterVerb(TEXT("echo"), TEXT("echo <text>, prints to the console. Source has it, Unreal does not."));
	RegisterVerb(TEXT("host_writeconfig"), TEXT("host_writeconfig [file], saves binds and changed variables. Defaults to config.cfg."));
	RegisterVerb(TEXT("writeconfig"), TEXT("writeconfig [file], alias of host_writeconfig."));

	AliasNameStorage.Reset();
	AliasNameStorage.Reserve(ButtonsByName.Num() * 2);

	for (const TPair<FString, FSourceKButton*>& Pair : ButtonsByName)
	{
		const FString Name = Pair.Key;
		const int32 PlusIndex = AliasNameStorage.Add(TEXT("+") + Name);
		const int32 MinusIndex = AliasNameStorage.Add(TEXT("-") + Name);

		Console.RegisterConsoleCommand(*AliasNameStorage[PlusIndex], TEXT("Source movement button (press)."), FConsoleCommandDelegate::CreateLambda([Name]() { FSourceBindManager::Get().ExecuteBindCommand(TEXT("+") + Name, true, 0); }));

		Console.RegisterConsoleCommand(*AliasNameStorage[MinusIndex], TEXT("Source movement button (release)."), FConsoleCommandDelegate::CreateLambda([Name]() { FSourceBindManager::Get().ExecuteBindCommand(TEXT("+") + Name, false, 0); }));
	}
}

void FSourceBindManager::Initialize()
{
	if (bInitialized)
	{
		return;
	}
	bInitialized = true;

	RegisterButtons();
	RegisterConsoleCommands();
	ApplyDefaultBinds(*GLog);

	// Deferred, not run here: half the console variables a config touches do not exist yet
	FCoreDelegates::OnPostEngineInit.AddLambda([]() { FSourceBindManager::Get().ExecStartupConfigs(); });

	UE_LOG(LogSourceMovement, Log, TEXT("SourceMovement: bind system ready (%d aliases). Try: bind \"z\" \"+jump\""), ButtonsByName.Num());
}
