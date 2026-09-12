#pragma once

#include "Core/ISourceWorldQuery.h"
#include "Core/SourceMovementParams.h"
#include "Core/SourceMovementState.h"
#include "Core/SourceTraceResult.h"

class SOURCEMOVEMENT_API FSourceMovementSim
{
public:
	FSourceMovementSim() = default;

	void RunCommand(FSourceMovementState& InOutState, const FSourceUserCmd& Cmd, const FSourceMovementParams& InParams, ISourceWorldQuery& InWorld);

	void ProcessMovement(FSourceMovementState& InOutState, FSourceMoveData& InOutMove, const FSourceMovementParams& InParams, ISourceWorldQuery& InWorld);

	static void SetupMove(const FSourceMovementState& State, const FSourceUserCmd& Cmd, const FSourceMovementParams& Params, FSourceMoveData& OutMove);

	static void FinishMoveToState(const FSourceMoveData& Move, FSourceMovementState& OutState);

	// Diagnostics only, nothing in the simulation reads this back
	const FSourceMovementTelemetry& GetTelemetry() const { return Telemetry; }
	const FSourceMovementEvents& GetEvents() const { return Events; }
	int32 GetTraceCount() const { return m_nTraceCount; }

#if WITH_DEV_AUTOMATION_TESTS
	void Test_BindContext(FSourceMovementState& InState, FSourceMoveData& InMove, const FSourceMovementParams& InParams, ISourceWorldQuery& InWorld);

	void Test_Friction() { Friction(); }
	void Test_StartGravity() { StartGravity(); }
	void Test_FinishGravity() { FinishGravity(); }
	void Test_CheckVelocity() { CheckVelocity(); }
	void Test_CategorizePosition() { CategorizePosition(); }
	void Test_StayOnGround() { StayOnGround(); }
	bool Test_CheckJumpButton() { return CheckJumpButton(); }
	void Test_PreventBunnyJumping() { PreventBunnyJumping(); }

	void Test_Duck() { Duck(); }
	bool Test_CanUnduck() { return CanUnduck(); }
	void Test_FinishDuck() { FinishDuck(); }
	void Test_FinishUnDuck() { FinishUnDuck(); }

	void Test_Accelerate(FSrcVec3& wishdir, srcfloat wishspeed, srcfloat accel) { Accelerate(wishdir, wishspeed, accel); }
	void Test_AirAccelerate(FSrcVec3& wishdir, srcfloat wishspeed, srcfloat accel) { AirAccelerate(wishdir, wishspeed, accel); }

	int32 Test_ClipVelocity(const FSrcVec3& in, const FSrcVec3& normal, FSrcVec3& out, srcfloat overbounce)
	{
		return ClipVelocity(in, normal, out, overbounce);
	}

	int32 Test_TryPlayerMove() { return TryPlayerMove(); }

	srcfloat Test_GetPlayerMaxSpeed() const { return GetPlayerMaxSpeed(); }
#endif

private:
	// Per-command context, valid only inside RunCommand
	FSourceMovementState* player = nullptr;
	FSourceMoveData* mv = nullptr;
	const FSourceMovementParams* params = nullptr;
	ISourceWorldQuery* world = nullptr;

	// The tick interval, scaled by the lagged movement value
	srcfloat frametime = 0.f;

	// The player's own clock: tick base times tick interval
	srcfloat curtime = 0.f;

	// Latch so each speed crop applies at most once per command
	int32 m_iSpeedCropped = SRC_SPEED_CROPPED_RESET;
	ESrcWaterLevel m_nOldWaterLevel = ESrcWaterLevel::NotInWater;
	srcfloat m_flWaterEntryTime = 0.f;
	int32 m_nOnLadder = 0;

	// Basis vectors built from the command view angles
	FSrcVec3 m_vecForward = FSrcVec3::Zero;
	FSrcVec3 m_vecRight = FSrcVec3::Zero;
	FSrcVec3 m_vecUp = FSrcVec3::Zero;
	int32 m_nTraceCount = 0;
	bool m_bProcessingMovement = false;
	bool m_bInStuckTest = false;
	FSourceMovementTelemetry Telemetry;
	FSourceMovementEvents Events;

	// Last ground plane normal reported by CategorizePosition
	FSrcVec3 LastGroundNormal = FSrcVec3::Zero;

	srcfloat GetPlayerMaxSpeed() const;

	bool CanMove() const;
	bool CheckInterval_Stuck() const;

	void UpdateStepSound();

	void UpdateStepSound_BaseClass();

	void GetStepSoundVelocities(srcfloat* velwalk, srcfloat* velrun) const;

	void SetStepSoundTime(ESrcStepSoundTime iStepSoundTime, bool bWalking);

	// Records the step; playback is the audio component's job
	void PlayStepSound(srcfloat fvol);
	void PlayerRoughLandingEffects(srcfloat fvol);

	void PlayerMove();
	void FinishMove();
	void CheckParameters();
	void ReduceTimers();
	void FullWalkMove();
	void FullNoClipMove(srcfloat factor, srcfloat maxacceleration);

	// Gravity
	void StartGravity();
	void FinishGravity();

	// Friction / acceleration
	void Friction();
	bool CanAccelerate();
	void Accelerate(FSrcVec3& wishdir, srcfloat wishspeed, srcfloat accel);
	void AirAccelerate(FSrcVec3& wishdir, srcfloat wishspeed, srcfloat accel);
	void UpdateTrailingVelocity();
	void WalkMove();
	void AirMove();

	// Jump
	bool CheckJumpButton();
	void PreventBunnyJumping();
	void OnJump(srcfloat fImpulse);
	void OnLand(srcfloat fVelocity);

	// Collision
	int32 TryPlayerMove(const FSrcVec3* pFirstDest = nullptr, const FSourceTraceResult* pFirstTrace = nullptr);
	int32 ClipVelocity(const FSrcVec3& in, const FSrcVec3& normal, FSrcVec3& out, srcfloat overbounce);
	void StepMove(FSrcVec3& vecDestination, FSourceTraceResult& trace);
	void StayOnGround();
	void CategorizePosition();
	void SetGroundEntity(const FSourceTraceResult* pm);
	void CategorizeGroundSurface(const FSourceTraceResult& pm);
	bool CheckValidStandableGroundCandidate(const FSourceTraceResult& pm, srcfloat flStandableZ) const;
	void TracePlayerBBoxForGround(const FSrcVec3& start, const FSrcVec3& end, const FSrcVec3& minsSrc, const FSrcVec3& maxsSrc, int32 fMask, FSourceTraceResult& pm, srcfloat minGroundNormalZ, bool overwriteEndpos);
	void CheckVelocity();
	void CheckFalling();

	void TracePlayerBBox(const FSrcVec3& start, const FSrcVec3& end, int32 fMask, FSourceTraceResult& pm);
	bool TestPlayerPosition(const FSrcVec3& pos, FSourceTraceResult& pm);
	int32 PlayerSolidMask(bool bBrushOnly = false) const;
	FSourceTraceFilter MakeTraceFilter() const;
	const FSrcVec3& GetPlayerMins() const;
	const FSrcVec3& GetPlayerMaxs() const;
	const FSrcVec3& GetPlayerViewOffset(bool bDucked) const;
	bool IsDead() const;
	srcfloat ComputeConstraintSpeedFactor();
	srcfloat CalcRoll(const FSrcAngles& angles, const FSrcVec3& velocity, srcfloat rollangle, srcfloat rollspeed);
	void DecayViewPunchAngle();
	void DecayAimPunchAngle();
	bool DuckingEnabled();

	// Duck / unduck
	void Duck();
	bool CanUnduck();
	void FinishDuck();
	void FinishUnDuck();
	void SetDuckedEyeOffset(srcfloat duckFraction);
	void FixPlayerCrouchStuck(bool upward);
	void UpdateDuckJumpEyeOffset();
	void HandleDuckingSpeedCrop(srcfloat duckFraction);
	void DuckUntilOnGround();
	void TraceHullExplicit(const FSrcVec3& start, const FSrcVec3& end, const FSrcVec3& mins, const FSrcVec3& maxs, int32 fMask, FSourceTraceResult& pm);

	// Not yet implemented
	int32 CheckStuck();
	bool CheckWater();
	bool LadderMove();
};
