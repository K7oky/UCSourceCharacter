#pragma once

#include "Core/SourceAngles.h"
#include "Core/SourceMovementTypes.h"
#include "Core/SourceVector.h"

struct FSourceUserCmd
{
	int32 CommandNumber = 0;
	int32 TickCount = 0;
	FSrcAngles ViewAngles = FSrcAngles();

	srcfloat ForwardMove = 0.f;
	srcfloat SideMove = 0.f;
	srcfloat UpMove = 0.f;
	uint32 Buttons = 0;
	int32 ImpulseCommand = 0;

	FORCEINLINE bool HasButton(uint32 Bit) const { return (Buttons & Bit) != 0; }
};

// The working copy for one command. SetupMove fills it from the state, FinishMove writes back.
struct FSourceMoveData
{
	bool bFirstRunOfFunctions = true;
	bool bGameCodeMovedPlayer = false;
	int32 ImpulseCommand = 0;
	FSrcAngles ViewAngles = FSrcAngles();
	FSrcAngles AbsViewAngles = FSrcAngles();

	uint32 Buttons = 0;
	uint32 OldButtons = 0;
	srcfloat ForwardMove = 0.f;
	srcfloat SideMove = 0.f;
	srcfloat UpMove = 0.f;
	srcfloat MaxSpeed = 0.f;
	srcfloat ClientMaxSpeed = 0.f;
	FSrcVec3 Velocity = FSrcVec3::Zero;
	FSrcVec3 AbsOrigin = FSrcVec3::Zero;
	FSrcAngles Angles = FSrcAngles();
	FSrcAngles OldAngles = FSrcAngles();

	FSrcVec3 TrailingVelocity = FSrcVec3::Zero;
	srcfloat TrailingVelocityTime = 0.f;

	// Output-only
	srcfloat OutStepHeight = 0.f;
	FSrcVec3 OutWishVel = FSrcVec3::Zero;
	FSrcVec3 OutJumpVel = FSrcVec3::Zero;
	FSrcVec3 ConstraintCenter = FSrcVec3::Zero;
	srcfloat ConstraintRadius = 0.f;
	srcfloat ConstraintWidth = 0.f;
	srcfloat ConstraintSpeedFactor = 0.f;
	bool bConstraintPastRadius = false;

	FORCEINLINE void SetAbsOrigin(const FSrcVec3& V) { AbsOrigin = V; }
	FORCEINLINE const FSrcVec3& GetAbsOrigin() const { return AbsOrigin; }
};

struct FSourceMovementState
{
	// Transform
	FSrcVec3 Origin = FSrcVec3::Zero;
	FSrcVec3 Velocity = FSrcVec3::Zero;
	FSrcVec3 BaseVelocity = FSrcVec3::Zero;
	FSrcAngles ViewAngles = FSrcAngles();
	FSrcVec3 ViewOffset = FSrcVec3(0.f, 0.f, 64.f);

	// Movement mode
	ESrcMoveType MoveType = ESrcMoveType::Walk;
	uint32 Flags = 0;
	FSrcEntityRef GroundEntity = FSrcEntityRef::None();

	uint32 m_nOldButtons = 0;
	int32 m_nTickBase = 0;

	// Surface
	srcfloat m_surfaceFriction = 1.0f;
	int32 m_surfaceProps = INDEX_NONE;
	uint8 m_chTextureType = 0;
	srcfloat m_flSurfaceJumpFactor = 1.0f;
	srcfloat m_flSurfaceMaxSpeedFactor = 1.0f;

	// Duck
	bool m_bDucked = false;
	bool m_bDucking = false;
	bool m_bInDuckJump = false;
	srcfloat m_flDuckAmount = 0.f;
	srcfloat m_flDuckSpeed = 8.0f;
	// -1, not Source's 0: there curtime is map time and starts well past the lockout
	srcfloat m_flLastDuckTime = -1.0f;
	srcfloat m_vecLastPositionAtFullCrouchSpeedX = 0.f;
	srcfloat m_vecLastPositionAtFullCrouchSpeedY = 0.f;
	int32 m_nDuckTimeMsecs = 0;
	int32 m_nDuckJumpTimeMsecs = 0;
	int32 m_nJumpTimeMsecs = 0;
	bool m_duckUntilOnGround = false;
	bool m_bDuckOverride = false;
	bool m_bIsDefusing = false;

	// Stamina / speed modifiers
	srcfloat m_flStamina = 0.f;
	srcfloat m_flVelocityModifier = 1.0f;
	srcfloat m_flGroundAccelLinearFracLastTime = 0.f;
	bool m_bIsWalking = false;
	ESrcMoveState m_iMoveState = ESrcMoveState::Idle;
	FSrcVec3 TrailingVelocityCache = FSrcVec3::Zero;
	srcfloat TrailingVelocityCacheTime = 0.f;
	srcfloat m_flBodyPitch = 0.f;
	srcfloat m_flMaxspeed = 260.0f;

	// Falling / landing
	srcfloat m_flFallVelocity = 0.f;
	bool m_bHasWalkMovedSinceLastJump = true;
	srcfloat m_flStepSoundTime = 0.f;
	ESrcWaterLevel WaterLevel = ESrcWaterLevel::NotInWater;
	int32 WaterType = SRC_CONTENTS_EMPTY;
	srcfloat m_flWaterJumpTime = 0.f;
	FSrcVec3 m_vecWaterJumpVel = FSrcVec3::Zero;
	srcfloat m_flSwimSoundTime = 0.f;
	FSrcVec3 m_vecLadderNormal = FSrcVec3::Zero;
	srcfloat m_ignoreLadderJumpTime = 0.f;
	srcfloat m_flGravityScale = 1.0f;
	FSrcAngles m_viewPunchAngle = FSrcAngles();
	FSrcAngles m_aimPunchAngle = FSrcAngles();
	FSrcAngles m_aimPunchAngleVel = FSrcAngles();

	int32 m_iHealth = 100;
	bool deadflag = false;
	int32 m_StuckLast = 0;
	int32 EntIndex = 1;
	bool bIsBot = false;

	FORCEINLINE bool HasFlag(uint32 Bit) const { return (Flags & Bit) != 0; }
	FORCEINLINE void AddFlag(uint32 Bit) { Flags |= Bit; }
	FORCEINLINE void RemoveFlag(uint32 Bit) { Flags &= ~Bit; }
	FORCEINLINE bool IsOnGround() const { return GroundEntity.IsValid(); }

	FORCEINLINE ESrcGroundState GetGroundState() const
	{
		return IsOnGround() ? ESrcGroundState::Grounded : ESrcGroundState::Airborne;
	}

	FORCEINLINE bool IsDead() const { return m_iHealth <= 0; }
	FORCEINLINE srcfloat GetCurTime(srcfloat TickInterval) const
	{
		return static_cast<srcfloat>(m_nTickBase) * TickInterval;
	}
};

struct FSourceMovementEvents
{
	bool bStepSound = false;
	srcfloat StepVolume = 0.f;
	bool bJumpSound = false;
	bool bLandSound = false;
	srcfloat LandVelocity = 0.f;
	int32 SurfacePropsId = INDEX_NONE;

	FORCEINLINE void Reset()
	{
		*this = FSourceMovementEvents();
	}
};

struct FSourceMovementTelemetry
{
	FSrcVec3 WishDir = FSrcVec3::Zero;
	FSrcVec3 WishVel = FSrcVec3::Zero;
	srcfloat WishSpeed = 0.f;
	srcfloat AccelSpeed = 0.f;
	srcfloat FrictionDrop = 0.f;
	srcfloat AppliedFriction = 0.f;
	FSrcVec3 GroundNormal = FSrcVec3::Zero;
	srcfloat MaxSpeed = 0.f;
	int32 TraceCount = 0;
	int32 CollisionPlanes = 0;
	int32 Blocked = 0;
	srcfloat StepHeight = 0.f;
	srcfloat HorizontalSpeed = 0.f;
	srcfloat Speed = 0.f;
	bool bJumpedThisTick = false;
	bool bLandedThisTick = false;

	FORCEINLINE void Reset()
	{
		*this = FSourceMovementTelemetry();
	}
};
