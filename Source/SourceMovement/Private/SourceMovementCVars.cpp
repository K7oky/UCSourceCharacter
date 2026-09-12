#include "SourceMovementCVars.h"

#include "Core/SourceMovementParams.h"
#include "Input/SourceInputTranslator.h"
#include "SourceMovementModule.h"

#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarSvCheats( TEXT("sv_cheats"), 0, TEXT("Allow cheats on server.\n")
	TEXT("While 0, every sv_* movement variable and every cl_* debug variable is locked at its\n")
	TEXT("default and any attempt to change one is rejected. Setting it back to 0 also reverts\n")
	TEXT("anything that was changed while it was 1."), ECVF_Default);

// sv_*, gated by sv_cheats below rather than by per-variable flags

static TAutoConsoleVariable<float> CVarSvGravity(TEXT("sv_gravity"), 800.0f, TEXT("World gravity."), ECVF_Default);
static TAutoConsoleVariable<float> CVarSvFriction(TEXT("sv_friction"), 5.2f, TEXT("World friction."), ECVF_Default);
static TAutoConsoleVariable<float> CVarSvStopSpeed(TEXT("sv_stopspeed"), 80.0f, TEXT("Minimum stopping speed when on ground."), ECVF_Default);
static TAutoConsoleVariable<float> CVarSvAccelerate(TEXT("sv_accelerate"), 5.5f, TEXT("Linear acceleration amount (old value is 5.6)."), ECVF_Default);
static TAutoConsoleVariable<float> CVarSvAirAccelerate(TEXT("sv_airaccelerate"), 12.0f, TEXT("Air acceleration amount."), ECVF_Default);
static TAutoConsoleVariable<float> CVarSvMaxSpeed(TEXT("sv_maxspeed"), 320.0f, TEXT("Upper bound on the player's max speed."), ECVF_Default);

static TAutoConsoleVariable<float> CVarSvMaxVelocity(TEXT("sv_maxvelocity"), 3500.0f, TEXT("Maximum speed any ballistically moving object is allowed to attain per axis."), ECVF_Default);

static TAutoConsoleVariable<float> CVarSvStepSize(TEXT("sv_stepsize"), 18.0f, TEXT("Maximum height the player can step up."), ECVF_Default);
static TAutoConsoleVariable<float> CVarSvBounce(TEXT("sv_bounce"), 0.0f, TEXT("Bounce multiplier for the airborne single-plane clip path."), ECVF_Default);

// 0 by default, so CalcRoll always returns 0
static TAutoConsoleVariable<float> CVarSvRollAngle(TEXT("sv_rollangle"), 0.0f, TEXT("Max view roll angle."), ECVF_Default);
static TAutoConsoleVariable<float> CVarSvRollSpeed(TEXT("sv_rollspeed"), 200.0f, TEXT("View roll speed."), ECVF_Default);

static TAutoConsoleVariable<float> CVarSvJumpImpulse(TEXT("sv_jump_impulse"), 301.993377f, TEXT("Initial upward velocity for player jumps; sqrt(2*gravity*height)."), ECVF_Default);

static TAutoConsoleVariable<float> CVarSvStaminaJumpCost(TEXT("sv_staminajumpcost"), 0.080f, TEXT("Stamina penalty for jumping."), ECVF_Default);
static TAutoConsoleVariable<float> CVarSvStaminaLandCost(TEXT("sv_staminalandcost"), 0.050f, TEXT("Stamina penalty for landing."), ECVF_Default);
static TAutoConsoleVariable<float> CVarSvStaminaRecoveryRate(TEXT("sv_staminarecoveryrate"), 60.0f, TEXT("Rate at which stamina recovers (units/sec)."), ECVF_Default);
static TAutoConsoleVariable<float> CVarSvStaminaMax(TEXT("sv_staminamax"), 80.0f, TEXT("Maximum stamina penalty."), ECVF_Default);

static TAutoConsoleVariable<int32> CVarSvEnableBunnyHopping( TEXT("sv_enablebunnyhopping"), 0, TEXT("Allow player speed to exceed maximum running speed.\n")
	TEXT("0 (matchmaking default) runs PreventBunnyJumping on every jump, clamping total speed to\n")
	TEXT("sv_enablebunnyhopping_maxspeedfactor * the player's latched max speed (1.1 * 260 = 286).\n")
	TEXT("1 removes the lock entirely, which is what surf and bhop servers do."), ECVF_Default);

static TAutoConsoleVariable<int32> CVarSvAutoBunnyHopping( TEXT("sv_autobunnyhopping"), 0, TEXT("Players automatically re-jump while holding jump button."), ECVF_Default);
static TAutoConsoleVariable<float> CVarSvBunnyJumpMaxSpeedFactor( TEXT("sv_enablebunnyhopping_maxspeedfactor"), 1.1f, TEXT("Speed cap factor applied by PreventBunnyJumping."), ECVF_Default);
static TAutoConsoleVariable<float> CVarSvTimeBetweenDucks( TEXT("sv_timebetweenducks"), 0.4f, TEXT("Minimum time before recognizing consecutive duck key."), ECVF_Default);
static TAutoConsoleVariable<int32> CVarSvAccelerateUseWeaponSpeed( TEXT("sv_accelerate_use_weapon_speed"), 1, TEXT("Scale ground acceleration by the active weapon's max speed."), ECVF_Default);
static TAutoConsoleVariable<int32> CVarSvOptimizedMovement( TEXT("sv_optimizedmovement"), 1, TEXT("Skip the leading CategorizePosition for walking players."), ECVF_Default);
static TAutoConsoleVariable<float> CVarSvCrouchSpamPenalty( TEXT("sv_crouch_spam_penalty"), 2.0f, TEXT("Duck-speed drained per duck key edge."), ECVF_Default);
static TAutoConsoleVariable<float> CVarSvNoClipSpeed( TEXT("sv_noclipspeed"), 5.0f, TEXT("Noclip speed, as a multiplier on sv_maxspeed."), ECVF_Default);
static TAutoConsoleVariable<float> CVarSvNoClipAccelerate( TEXT("sv_noclipaccelerate"), 5.0f, TEXT("Noclip acceleration. Zero or less assigns velocity directly instead of accelerating."), ECVF_Default);
static TAutoConsoleVariable<int32> CVarSvFootsteps( TEXT("sv_footsteps"), 1, TEXT("Play footstep sounds for players."), ECVF_Default);
static TAutoConsoleVariable<float> CVarSvFootstepSoundFrequency( TEXT("sv_footstep_sound_frequency"), 0.97f, TEXT("How frequent to hear the player's step sound. Scales the interval, so LOWER is faster."), ECVF_Default);

static TAutoConsoleVariable<float> CVarSvFootstepVolumeWalk( TEXT("sv_footstep_volume_walk"), 0.2f, TEXT("NON-SOURCE: walking footstep volume (Source: surfaceproperties, CHAR_TEX_CONCRETE = 0.2)."), ECVF_Default);
static TAutoConsoleVariable<float> CVarSvFootstepVolumeRun( TEXT("sv_footstep_volume_run"), 0.5f, TEXT("NON-SOURCE: running footstep volume (Source: surfaceproperties, CHAR_TEX_CONCRETE = 0.5)."), ECVF_Default);

// cl_* client input
static TAutoConsoleVariable<float> CVarClForwardSpeed( TEXT("cl_forwardspeed"), 450.0f, TEXT("Forward move command magnitude."), ECVF_Default);
static TAutoConsoleVariable<float> CVarClSideSpeed( TEXT("cl_sidespeed"), 450.0f, TEXT("Side move command magnitude."), ECVF_Default);
static TAutoConsoleVariable<float> CVarClBackSpeed( TEXT("cl_backspeed"), 450.0f, TEXT("Backward move command magnitude."), ECVF_Default);
static TAutoConsoleVariable<float> CVarClUpSpeed( TEXT("cl_upspeed"), 320.0f, TEXT("Vertical move command magnitude (swim / noclip / ladder)."), ECVF_Default);
static TAutoConsoleVariable<float> CVarClPitchDown( TEXT("cl_pitchdown"), 89.0f, TEXT("Maximum downward view pitch."), ECVF_Default);
static TAutoConsoleVariable<float> CVarClPitchUp( TEXT("cl_pitchup"), 89.0f, TEXT("Maximum upward view pitch."), ECVF_Default);

static TAutoConsoleVariable<float> CVarSensitivity( TEXT("sensitivity"), 2.5f, TEXT("Mouse sensitivity."), ECVF_Default);
static TAutoConsoleVariable<float> CVarMYaw(TEXT("m_yaw"), 0.022f, TEXT("Mouse yaw factor."), ECVF_Default);
static TAutoConsoleVariable<float> CVarMPitch(TEXT("m_pitch"), 0.022f, TEXT("Mouse pitch factor."), ECVF_Default);

// cl_* debug readouts

static TAutoConsoleVariable<int32> CVarClShowPos( TEXT("cl_showpos"), 0, TEXT("On-screen movement readout.\n")
	TEXT("0 = off\n")
	TEXT("1 = speed / ground state / wish direction / ground normal and slope angle\n")
	TEXT("2 = also stamina, surface friction, trace counts, collision planes, duck state"), ECVF_Default);

static TAutoConsoleVariable<int32> CVarClDrawHull( TEXT("cl_sourcemove_drawhull"), 0, TEXT("Draw the AABB the movement solver actually sweeps (not the proxy box component)."), ECVF_Default);

static TAutoConsoleVariable<int32> CVarClTraceLog( TEXT("cl_sourcemove_tracelog"), 0, TEXT("Log the plane reconstruction for sweeps that block with fraction 0 (the 'player is pinned' case)."), ECVF_Default);

static TAutoConsoleVariable<int32> CVarClDrawTraces( TEXT("cl_sourcemove_drawtraces"), 0, TEXT("Visualise hull sweeps issued by the solver.\n")
	TEXT("0 = off\n")
	TEXT("1 = sweeps that hit something: stop box, impact point, plane normal\n")
	TEXT("2 = every sweep including clean misses (a tick issues 5..20 of them)"), ECVF_Default);

// The sv_cheats gate

namespace
{
	// Snapshots each gated variable's default and restores it when sv_cheats goes to 0
	struct FSourceCheatGate
	{
		struct FEntry
		{
			IConsoleVariable* Var = nullptr;
			FString Name;
			FString DefaultValue;
		};
		TArray<FEntry> Entries;

		// Re-entrancy guard: our own reverts fire the change callback again
		bool bSuppress = false;
		bool bRegistered = false;

		void Register(const TCHAR* Name, IConsoleVariable* Var)
		{
			if (!Var)
			{
				return;
			}

			FEntry Entry;
			Entry.Var = Var;
			Entry.Name = Name;
			Entry.DefaultValue = Var->GetString();

			Entries.Add(Entry);

			Var->SetOnChangedCallback(FConsoleVariableDelegate::CreateRaw(this, &FSourceCheatGate::OnGatedVarChanged));
		}

		void OnGatedVarChanged(IConsoleVariable* Var)
		{
			if (bSuppress || SourceMovementCVars::AreCheatsEnabled())
			{
				return;
			}

			for (FEntry& Entry : Entries)
			{
				if (Entry.Var != Var)
				{
					continue;
				}

				if (Entry.Var->GetString() == Entry.DefaultValue)
				{
					return;  // already at the default, nothing was actually changed
				}

				// SetByConsole outranks SetByCode, so a revert by code is ignored
				TGuardValue<bool> Guard(bSuppress, true);
				Entry.Var->Set(*Entry.DefaultValue, ECVF_SetByConsole);

				UE_LOG(LogSourceMovement, Warning, TEXT("Can't use cheat cvar %s in multiplayer, unless the server has sv_cheats set to 1."), *Entry.Name);
				return;
			}
		}

		const FEntry* Find(const FString& Name) const
		{
			for (const FEntry& Entry : Entries)
			{
				if (Entry.Name.Equals(Name, ESearchCase::IgnoreCase))
				{
					return &Entry;
				}
			}
			return nullptr;
		}

		void RevertAllModifiedLocalState()
		{
			TGuardValue<bool> Guard(bSuppress, true);
			int32 Reverted = 0;
			for (FEntry& Entry : Entries)
			{
				if (Entry.Var->GetString() != Entry.DefaultValue)
				{
					Entry.Var->Set(*Entry.DefaultValue, ECVF_SetByConsole);
					++Reverted;
				}
			}

			if (Reverted > 0)
			{
				UE_LOG(LogSourceMovement, Log, TEXT("sv_cheats 0: reverted %d cheat variable(s) to their defaults."), Reverted);
			}
		}
	};
	FSourceCheatGate GCheatGate;

	void OnSvCheatsChanged(IConsoleVariable* Var)
	{
		if (Var && Var->GetInt() == 0)
		{
			GCheatGate.RevertAllModifiedLocalState();
		}
	}
}

void SourceMovementCVars::RegisterCheatGate()
{
	if (GCheatGate.bRegistered)
	{
		return;
	}
	GCheatGate.bRegistered = true;

	if (IConsoleVariable* CheatsVar = CVarSvCheats.AsVariable())
	{
		CheatsVar->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnSvCheatsChanged));
	}

	auto Gate = [](const TCHAR* Name)
	{
		GCheatGate.Register(Name, IConsoleManager::Get().FindConsoleVariable(Name));
	};

	// sv_* movement
	Gate(TEXT("sv_gravity"));
	Gate(TEXT("sv_friction"));
	Gate(TEXT("sv_stopspeed"));
	Gate(TEXT("sv_accelerate"));
	Gate(TEXT("sv_airaccelerate"));
	Gate(TEXT("sv_maxspeed"));
	Gate(TEXT("sv_maxvelocity"));
	Gate(TEXT("sv_stepsize"));
	Gate(TEXT("sv_bounce"));
	Gate(TEXT("sv_rollangle"));
	Gate(TEXT("sv_rollspeed"));
	Gate(TEXT("sv_jump_impulse"));
	Gate(TEXT("sv_staminajumpcost"));
	Gate(TEXT("sv_staminalandcost"));
	Gate(TEXT("sv_staminarecoveryrate"));
	Gate(TEXT("sv_staminamax"));
	Gate(TEXT("sv_enablebunnyhopping"));
	Gate(TEXT("sv_autobunnyhopping"));
	Gate(TEXT("sv_enablebunnyhopping_maxspeedfactor"));
	Gate(TEXT("sv_timebetweenducks"));
	Gate(TEXT("sv_accelerate_use_weapon_speed"));
	Gate(TEXT("sv_optimizedmovement"));
	Gate(TEXT("sv_crouch_spam_penalty"));
	Gate(TEXT("sv_noclipspeed"));
	Gate(TEXT("sv_noclipaccelerate"));
	Gate(TEXT("sv_footstep_sound_frequency"));
	Gate(TEXT("sv_footstep_volume_walk"));
	Gate(TEXT("sv_footstep_volume_run"));

	// cl_* input
	Gate(TEXT("cl_forwardspeed"));
	Gate(TEXT("cl_sidespeed"));
	Gate(TEXT("cl_backspeed"));
	Gate(TEXT("cl_upspeed"));
	Gate(TEXT("cl_pitchdown"));
	Gate(TEXT("cl_pitchup"));

	// Left out on purpose: sensitivity, m_yaw, m_pitch, sv_footsteps

	// cl_* debug
	Gate(TEXT("cl_showpos"));
	Gate(TEXT("cl_sourcemove_drawhull"));
	Gate(TEXT("cl_sourcemove_drawtraces"));
	Gate(TEXT("cl_sourcemove_tracelog"));

	UE_LOG(LogSourceMovement, Log, TEXT("SourceMovement: %d console variables registered behind sv_cheats."), GCheatGate.Entries.Num());
}

bool SourceMovementCVars::AreCheatsEnabled()
{
	return CVarSvCheats.GetValueOnAnyThread() != 0;
}

void SourceMovementCVars::ApplyToParams(FSourceMovementParams& OutParams)
{
	OutParams.Gravity = CVarSvGravity.GetValueOnGameThread();
	OutParams.Friction = CVarSvFriction.GetValueOnGameThread();
	OutParams.StopSpeed = CVarSvStopSpeed.GetValueOnGameThread();
	OutParams.Accelerate = CVarSvAccelerate.GetValueOnGameThread();
	OutParams.AirAccelerate = CVarSvAirAccelerate.GetValueOnGameThread();
	OutParams.MaxSpeed = CVarSvMaxSpeed.GetValueOnGameThread();
	OutParams.MaxVelocity = CVarSvMaxVelocity.GetValueOnGameThread();
	OutParams.StepSize = CVarSvStepSize.GetValueOnGameThread();
	OutParams.Bounce = CVarSvBounce.GetValueOnGameThread();
	OutParams.RollAngle = CVarSvRollAngle.GetValueOnGameThread();
	OutParams.RollSpeed = CVarSvRollSpeed.GetValueOnGameThread();
	OutParams.JumpImpulse = CVarSvJumpImpulse.GetValueOnGameThread();
	OutParams.StaminaJumpCost = CVarSvStaminaJumpCost.GetValueOnGameThread();
	OutParams.StaminaLandCost = CVarSvStaminaLandCost.GetValueOnGameThread();
	OutParams.StaminaRecoveryRate = CVarSvStaminaRecoveryRate.GetValueOnGameThread();
	OutParams.StaminaMax = CVarSvStaminaMax.GetValueOnGameThread();
	OutParams.bEnableBunnyHopping = CVarSvEnableBunnyHopping.GetValueOnGameThread() != 0;
	OutParams.bAutoBunnyHopping = CVarSvAutoBunnyHopping.GetValueOnGameThread() != 0;
	OutParams.BunnyJumpMaxSpeedFactor = CVarSvBunnyJumpMaxSpeedFactor.GetValueOnGameThread();
	OutParams.TimeBetweenDucks = CVarSvTimeBetweenDucks.GetValueOnGameThread();
	OutParams.CrouchSpamPenalty = CVarSvCrouchSpamPenalty.GetValueOnGameThread();
	OutParams.NoClipSpeed = CVarSvNoClipSpeed.GetValueOnGameThread();
	OutParams.NoClipAccelerate = CVarSvNoClipAccelerate.GetValueOnGameThread();
	OutParams.bFootsteps = CVarSvFootsteps.GetValueOnGameThread() != 0;
	OutParams.FootstepSoundFrequency = CVarSvFootstepSoundFrequency.GetValueOnGameThread();
	OutParams.FootstepVolumeWalk = CVarSvFootstepVolumeWalk.GetValueOnGameThread();
	OutParams.FootstepVolumeRun = CVarSvFootstepVolumeRun.GetValueOnGameThread();
	OutParams.bAccelerateUseWeaponSpeed = CVarSvAccelerateUseWeaponSpeed.GetValueOnGameThread() != 0;
	OutParams.bOptimizedMovement = CVarSvOptimizedMovement.GetValueOnGameThread() != 0;
}

void SourceMovementCVars::ApplyToInputTranslator(FSourceInputTranslator& OutTranslator)
{
	OutTranslator.ForwardSpeed = CVarClForwardSpeed.GetValueOnGameThread();
	OutTranslator.BackSpeed = CVarClBackSpeed.GetValueOnGameThread();
	OutTranslator.SideSpeed = CVarClSideSpeed.GetValueOnGameThread();
	OutTranslator.UpSpeed = CVarClUpSpeed.GetValueOnGameThread();
}

float SourceMovementCVars::GetPitchDown() { return CVarClPitchDown.GetValueOnGameThread(); }
float SourceMovementCVars::GetPitchUp() { return CVarClPitchUp.GetValueOnGameThread(); }

float SourceMovementCVars::GetSensitivity() { return CVarSensitivity.GetValueOnGameThread(); }
float SourceMovementCVars::GetMouseYawFactor() { return CVarMYaw.GetValueOnGameThread(); }
float SourceMovementCVars::GetMousePitchFactor() { return CVarMPitch.GetValueOnGameThread(); }

int32 SourceMovementCVars::GetShowPos()
{
	return AreCheatsEnabled() ? CVarClShowPos.GetValueOnGameThread() : 0;
}

bool SourceMovementCVars::GetDrawHull()
{
	return AreCheatsEnabled() && CVarClDrawHull.GetValueOnGameThread() != 0;
}

bool SourceMovementCVars::GetTraceLog()
{
	return AreCheatsEnabled() && CVarClTraceLog.GetValueOnGameThread() != 0;
}

int32 SourceMovementCVars::GetDrawTraces()
{
	return AreCheatsEnabled() ? CVarClDrawTraces.GetValueOnGameThread() : 0;
}

// Config persistence

void SourceMovementCVars::GetPersistableCVars(TArray<FPersistableCVar>& Out)
{
	Out.Reset();

	// Cheat-gated set
	for (const FSourceCheatGate::FEntry& Entry : GCheatGate.Entries)
	{
		FPersistableCVar Item;
		Item.Name = Entry.Name;
		Item.Value = Entry.Var ? Entry.Var->GetString() : Entry.DefaultValue;
		Item.DefaultValue = Entry.DefaultValue;
		Item.bIsCheatGated = true;
		Out.Add(MoveTemp(Item));
	}

	// Ungated personal settings
	auto AddUngated = [&Out](const TCHAR* Name, const TCHAR* Default)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			FPersistableCVar Item;
			Item.Name = Name;
			Item.Value = Var->GetString();
			Item.DefaultValue = Default;
			Item.bIsCheatGated = false;
			Out.Add(MoveTemp(Item));
		}
	};

	AddUngated(TEXT("sensitivity"), TEXT("2.5"));
	AddUngated(TEXT("m_yaw"), TEXT("0.022"));
	AddUngated(TEXT("m_pitch"), TEXT("0.022"));
	AddUngated(TEXT("sv_footsteps"), TEXT("1"));
	AddUngated(TEXT("sv_cheats"), TEXT("0"));
}
