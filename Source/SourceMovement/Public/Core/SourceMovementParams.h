#pragma once

#include "Core/SourceMovementTypes.h"  // SRC_MASK_PLAYERSOLID, content bits
#include "Core/SourcePlayerBounds.h"
#include "Core/SourceScalar.h"

struct FSourceMovementParams
{
	// Timestep
	srcfloat TickInterval = 1.0f / 64.0f;
	srcfloat LaggedMovementValue = 1.0f;

	// Server movement variables
	srcfloat Gravity = 800.0f;
	srcfloat Friction = 5.2f;
	srcfloat StopSpeed = 80.0f;
	srcfloat Accelerate = 5.5f;
	srcfloat AirAccelerate = 12.0f;
	srcfloat MaxSpeed = 320.0f;
	srcfloat MaxVelocity = 3500.0f;
	srcfloat Bounce = 0.0f;
	srcfloat StepSize = 18.0f;
	srcfloat RollAngle = 0.0f;
	srcfloat RollSpeed = 200.0f;

	// Punch decay
	srcfloat ViewPunchDecay = 18.0f;
	srcfloat WeaponRecoilDecay2Exp = 8.0f;
	srcfloat WeaponRecoilDecay2Lin = 18.0f;
	srcfloat WeaponRecoilVelDecay = 4.5f;

	// Jump and stamina
	srcfloat JumpImpulse = 301.993377f;
	srcfloat StaminaJumpCost = 0.080f;
	srcfloat StaminaLandCost = 0.050f;
	srcfloat StaminaRecoveryRate = 60.0f;
	srcfloat StaminaMax = 80.0f;
	srcfloat StaminaRange = 100.0f;

	bool bEnableBunnyHopping = false;
	bool bAutoBunnyHopping = false;
	srcfloat BunnyJumpMaxSpeedFactor = 1.1f;

	srcfloat TimeBetweenDucks = 0.4f;
	srcfloat CrouchSpamPenalty = 2.0f;

	// CS player speeds and the modifiers applied to them
	bool bAccelerateUseWeaponSpeed = true;
	srcfloat VelocityRecoveryRate = 1.0f / 2.5f;
	srcfloat PlayerSpeedRun = 260.0f;
	srcfloat PlayerSpeedStopped = 1.0f;
	srcfloat PlayerSpeedDuckModifier = 0.34f;
	srcfloat PlayerSpeedWalkModifier = 0.52f;
	srcfloat PlayerDuckSpeedIdeal = 8.0f;
	srcfloat AccelerateReferenceSpeed = 250.0f;

	// Compile-time constants
	static constexpr int32 MaxClipPlanes = 5;
	static constexpr int32 NumBumps = 4;
	static constexpr srcfloat MinimumMoveFraction = 0.0001f;
	static constexpr srcfloat EffectivelyHorizontalNormalZ = 0.0001f;
	static constexpr srcfloat DistEpsilon = 0.03125f;
	static constexpr srcfloat CoordResolution = 1.0f / 32.0f;
	static constexpr srcfloat StandableGroundNormalZ = 0.7f;
	static constexpr srcfloat NonJumpVelocity = 140.0f;
	static constexpr srcfloat OptimizedAirborneVelocityZ = 250.0f;
	static constexpr srcfloat GroundTraceOffset = 2.0f;
	static constexpr int32 TimeToDuckMsecs = 200;
	static constexpr int32 TimeToUnduckMsecs = 200;
	static constexpr int32 GameMovementDuckTime = 1000;
	static constexpr int32 GameMovementJumpTime = 510;
	static constexpr srcfloat PlayerMaxSafeFallSpeed = 580.0f;
	static constexpr srcfloat PlayerFatalFallSpeed = 1024.0f;
	static constexpr srcfloat PlayerMinBounceSpeed = 200.0f;
	static constexpr srcfloat PlayerFallPunchThreshold = 350.0f;
	static constexpr srcfloat MaxClimbSpeed = 200.0f;

	// Noclip
	srcfloat NoClipSpeed = 5.0f;
	srcfloat NoClipAccelerate = 5.0f;

	// Footstep audio. These drive the decision to step, not the playback
	bool bFootsteps = true;
	srcfloat FootstepSoundFrequency = 0.97f;
	srcfloat FootstepVolumeWalk = 0.2f;
	srcfloat FootstepVolumeRun = 0.5f;
	static constexpr srcfloat JumpStepSoundSpeed = 126.0f;
	static constexpr srcfloat LandSoundFallVelocity = 270.0f;

	// NOT interchangeable: one skips the leading ground check, the other gates solver shortcuts
	bool bOptimizedMovement = true;
	bool bMovementOptimizations = true;
	bool bIsGameConsole = false;
	bool bAllowAutoMovement = true;

	// Values that could not be derived from the original code
	srcfloat DefaultSurfaceFriction = 0.8f;
	srcfloat SurfaceFrictionScale = 1.25f;
	srcfloat DefaultSurfaceJumpFactor = 1.0f;
	srcfloat DefaultSurfaceMaxSpeedFactor = 1.0f;
	srcfloat AirborneRisingSurfaceFriction = 0.25f;
	srcfloat WeaponMaxSpeed = -1.0f;
	int32 WeaponZoomLevel = 0;
	int32 WeaponZoomLevels = 1;

	// Starting value of the velocity modifier that recovers on the ground
	srcfloat VelocityModifierInitial = 1.0f;

	// Geometry
	FSourcePlayerHullSet Hulls;
	int32 PlayerSolidMask = SRC_MASK_PLAYERSOLID;

	// Derived helpers
	FORCEINLINE srcfloat FractionDucked(int32 Msecs) const
	{
		return SourceMath::SrcClamp(static_cast<srcfloat>(Msecs) / static_cast<srcfloat>(TimeToDuckMsecs), 0.0f, 1.0f);
	}

	FORCEINLINE srcfloat FractionUnDucked(int32 Msecs) const
	{
		return SourceMath::SrcClamp(static_cast<srcfloat>(Msecs) / static_cast<srcfloat>(TimeToUnduckMsecs), 0.0f, 1.0f);
	}

	FORCEINLINE srcfloat GetDuckSpeedModifier(srcfloat DuckFraction) const
	{
		return PlayerSpeedDuckModifier * DuckFraction + 1.0f - DuckFraction;
	}
};
