// Port of CGameMovement / CCSGameMovement: command pipeline, parameters, gravity, falling

#include "Core/SourceMovementSim.h"

#include "SourceMovementModule.h"

using namespace SourceMath;

void FSourceMovementSim::SetupMove(const FSourceMovementState& State, const FSourceUserCmd& Cmd, const FSourceMovementParams& Params, FSourceMoveData& OutMove)
{
	OutMove.bFirstRunOfFunctions = true;
	OutMove.bGameCodeMovedPlayer = false;
	OutMove.ImpulseCommand = Cmd.ImpulseCommand;
	OutMove.ViewAngles = Cmd.ViewAngles;

	// No move parent, so the absolute angles are just the command angles
	OutMove.AbsViewAngles = Cmd.ViewAngles;
	OutMove.Buttons = Cmd.Buttons;

	if (State.HasFlag(SRC_FL_ATCONTROLS))
	{
		OutMove.ForwardMove = 0.f;
		OutMove.SideMove = 0.f;
		OutMove.UpMove = 0.f;
	}
	else
	{
		OutMove.ForwardMove = Cmd.ForwardMove;
		OutMove.SideMove = Cmd.SideMove;
		OutMove.UpMove = Cmd.UpMove;
	}

	OutMove.ClientMaxSpeed = State.m_flMaxspeed;
	OutMove.OldButtons = State.m_nOldButtons;
	OutMove.Angles = State.ViewAngles;
	OutMove.OldAngles = State.ViewAngles;
	OutMove.Velocity = State.Velocity;
	OutMove.SetAbsOrigin(State.Origin);

	// CMoveData is a global in Source and SetupMove never clears it, so this persists
	OutMove.TrailingVelocity = State.TrailingVelocityCache;
	OutMove.TrailingVelocityTime = State.TrailingVelocityCacheTime;

	// Constraints are unused until a constraint entity exists; radius 0 disables the factor
	OutMove.ConstraintCenter = FSrcVec3::Zero;
	OutMove.ConstraintRadius = 0.f;
	OutMove.ConstraintWidth = 0.f;
	OutMove.ConstraintSpeedFactor = 0.f;
	OutMove.bConstraintPastRadius = false;

	(void)Params;
}

void FSourceMovementSim::FinishMoveToState(const FSourceMoveData& Move, FSourceMovementState& OutState)
{
	OutState.m_flMaxspeed = Move.ClientMaxSpeed;
	OutState.Origin = Move.GetAbsOrigin();
	OutState.Velocity = Move.Velocity;
	OutState.m_nOldButtons = Move.Buttons;
	srcfloat Pitch = Move.Angles[SRC_PITCH];
	if (Pitch > 180.0f)
	{
		Pitch -= 360.0f;
	}
	Pitch = SrcClamp(Pitch, -90.0f, 90.0f);

	// Clamped pitch goes to the body only; pl.v_angle keeps the raw angles
	OutState.m_flBodyPitch = Pitch;
	OutState.TrailingVelocityCache = Move.TrailingVelocity;
	OutState.TrailingVelocityCacheTime = Move.TrailingVelocityTime;
}

void FSourceMovementSim::RunCommand(FSourceMovementState& InOutState, const FSourceUserCmd& Cmd, const FSourceMovementParams& InParams, ISourceWorldQuery& InWorld)
{
	// Captured before the command overwrites them, the move data wants both
	const FSrcAngles PrevViewAngles = InOutState.ViewAngles;
	InOutState.ViewAngles = Cmd.ViewAngles;
	FSourceMoveData Move;
	SetupMove(InOutState, Cmd, InParams, Move);
	Move.OldAngles = PrevViewAngles;

	ProcessMovement(InOutState, Move, InParams, InWorld);

	FinishMoveToState(Move, InOutState);

	if (InParams.TickInterval > 0.f)
	{
		++InOutState.m_nTickBase;
	}
}

void FSourceMovementSim::ProcessMovement(FSourceMovementState& InOutState, FSourceMoveData& InOutMove, const FSourceMovementParams& InParams, ISourceWorldQuery& InWorld)
{
	m_nTraceCount = 0;
	Telemetry.Reset();
	Events.Reset();
	player = &InOutState;
	mv = &InOutMove;
	params = &InParams;
	world = &InWorld;

	// Curtime is the player's own clock, not the server's
	curtime = InOutState.GetCurTime(InParams.TickInterval);
	frametime = InParams.TickInterval * InParams.LaggedMovementValue;
	m_iSpeedCropped = SRC_SPEED_CROPPED_RESET;
	mv->MaxSpeed = GetPlayerMaxSpeed();
	m_bProcessingMovement = true;
	m_bInStuckTest = false;

	PlayerMove();

	FinishMove();
	m_bProcessingMovement = false;

	// Telemetry (write-only; see FSourceMovementTelemetry)
	Telemetry.TraceCount = m_nTraceCount;
	Telemetry.MaxSpeed = mv->MaxSpeed;
	Telemetry.Speed = mv->Velocity.Length();
	Telemetry.HorizontalSpeed = mv->Velocity.Length2D();
	Telemetry.StepHeight = mv->OutStepHeight;
	Telemetry.GroundNormal = LastGroundNormal;
}

void FSourceMovementSim::FinishMove()
{
	mv->OldButtons = mv->Buttons;
}

#if WITH_DEV_AUTOMATION_TESTS
void FSourceMovementSim::Test_BindContext(FSourceMovementState& InState, FSourceMoveData& InMove, const FSourceMovementParams& InParams, ISourceWorldQuery& InWorld)
{
	player = &InState;
	mv = &InMove;
	params = &InParams;
	world = &InWorld;
	curtime = InState.GetCurTime(InParams.TickInterval);
	frametime = InParams.TickInterval * InParams.LaggedMovementValue;
	m_nTraceCount = 0;
	m_iSpeedCropped = SRC_SPEED_CROPPED_RESET;

	AngleVectors(InMove.ViewAngles, &m_vecForward, &m_vecRight, &m_vecUp);
}
#endif

srcfloat FSourceMovementSim::GetPlayerMaxSpeed() const
{
	if (player->MoveType == ESrcMoveType::None)
	{
		return params->PlayerSpeedStopped;
	}

	srcfloat fMaxSpeed = params->MaxSpeed;
	if (player->m_flMaxspeed > 0.0f && player->m_flMaxspeed < fMaxSpeed)
	{
		fMaxSpeed = player->m_flMaxspeed;
	}

	srcfloat Speed = SrcMin(params->PlayerSpeedRun, fMaxSpeed);

	if (params->WeaponMaxSpeed >= 0.0f)
	{
		Speed = SrcMin(params->WeaponMaxSpeed, Speed);
	}

	return Speed;
}

void FSourceMovementSim::PlayerMove()
{
	if (!CanMove())
	{
		mv->ForwardMove = 0.f;
		mv->SideMove = 0.f;
		mv->UpMove = 0.f;
		mv->Buttons &= ~(SRC_IN_JUMP | SRC_IN_FORWARD | SRC_IN_BACK | SRC_IN_MOVELEFT | SRC_IN_MOVERIGHT);
	}

	CheckParameters();

	mv->OutWishVel.Init();
	mv->OutJumpVel.Init();

	ReduceTimers();

	// From the COMMAND view angles, not the punch-adjusted player angles
	AngleVectors(mv->ViewAngles, &m_vecForward, &m_vecRight, &m_vecUp);

	{
		const ESrcMoveType moveType = player->MoveType;
		if (moveType != ESrcMoveType::NoClip && moveType != ESrcMoveType::None && moveType != ESrcMoveType::Isometric && moveType != ESrcMoveType::Observer && !player->deadflag)
		{
			if (CheckInterval_Stuck())
			{
				if (CheckStuck())
				{
					return;
				}
			}
		}
	}

	// Walking players skip this leading ground check unless the game moved them
	if (player->MoveType != ESrcMoveType::Walk || mv->bGameCodeMovedPlayer || !params->bOptimizedMovement)
	{
		CategorizePosition();
	}
	else
	{
		if (mv->Velocity.z > FSourceMovementParams::OptimizedAirborneVelocityZ)
		{
			SetGroundEntity(nullptr);
		}
	}

	m_nOldWaterLevel = player->WaterLevel;

	// Fall speed is latched at the top of the tick, so landing uses the entry speed
	if (!player->GroundEntity.IsValid())
	{
		player->m_flFallVelocity = -mv->Velocity.z;
	}

	m_nOnLadder = 0;

	UpdateStepSound();

	UpdateDuckJumpEyeOffset();
	Duck();

	if (!player->deadflag && !player->HasFlag(SRC_FL_ONTRAIN))
	{
		if (!LadderMove() && (player->MoveType == ESrcMoveType::Ladder))
		{
			player->MoveType = ESrcMoveType::Walk;
		}
	}

	switch (player->MoveType)
	{
	case ESrcMoveType::None:
		break;

	case ESrcMoveType::NoClip:
		FullNoClipMove(params->NoClipSpeed, params->NoClipAccelerate);
		break;

	case ESrcMoveType::Fly:
	case ESrcMoveType::FlyGravity:
		// Not ported: the ballistic toss move
		break;

	case ESrcMoveType::Ladder:
		// Not ported: ladder climbing
		break;

	case ESrcMoveType::Walk:
		FullWalkMove();
		break;

	case ESrcMoveType::Isometric:
		// Source routes ISOMETRIC through FullWalkMove too
		FullWalkMove();
		break;

	case ESrcMoveType::Observer:
		// Not ported: free-roaming spectator
		break;

	default:
		break;
	}

	// The velocity modifier recovers only while on the ground
	if (player->HasFlag(SRC_FL_ONGROUND))
	{
		if (player->m_flVelocityModifier < 1.0f)
		{
			player->m_flVelocityModifier = SrcClamp(player->m_flVelocityModifier + frametime * params->VelocityRecoveryRate, 0.0f, 1.0f);
		}
	}

	if (!IsDead())
	{
		if (!player->HasFlag(SRC_FL_DUCKING) && !player->m_bDucking && !player->m_bDucked)
		{
			player->ViewOffset = params->Hulls.GetPlayerViewOffset(false);
		}
		else if (player->m_duckUntilOnGround)
		{
			// Ducked hull but airborne: put the eye where it would be with the hull pulled to the top
			const FSrcVec3 hullSizeNormal = params->Hulls.GetHullSizeNormal();
			const FSrcVec3 hullSizeCrouch = params->Hulls.GetHullSizeCrouch();
			const FSrcVec3 lowerClearance = hullSizeNormal - hullSizeCrouch;
			player->ViewOffset = params->Hulls.GetPlayerViewOffset(false) - lowerClearance;
		}
		else if (player->m_bDucked && !player->m_bDucking)
		{
			player->ViewOffset = params->Hulls.GetPlayerViewOffset(true);
		}
	}
}

bool FSourceMovementSim::CanMove() const
{
	return !(player->MoveType == ESrcMoveType::None);
}

bool FSourceMovementSim::DuckingEnabled()
{
	// Anti-duck-spam: mashing the duck key drains m_flDuckSpeed; below 1.5 the key is ignored
	if (player->m_flDuckSpeed < 1.5f)
	{
		return false;
	}

	// After a completed duck/unduck we cannot re-duck for sv_timebetweenducks
	if ((player->Flags & SRC_FL_DUCKING) == 0 && curtime < player->m_flLastDuckTime + params->TimeBetweenDucks)
	{
		return false;
	}

	return true;
}

void FSourceMovementSim::CheckParameters()
{
	FSrcAngles v_angle;

	// IN_RAWDUCK is IN_BULLRUSH reused to remember the pre-gameplay IN_DUCK value
	if ((mv->Buttons & SRC_IN_DUCK) != 0)
	{
		mv->Buttons |= SRC_IN_RAWDUCK;
	}

	// A CHANGE in either direction (press OR release) costs duck speed
	if ((mv->Buttons & SRC_IN_RAWDUCK) != (mv->OldButtons & SRC_IN_RAWDUCK))
	{
		player->m_flDuckSpeed = SrcMax(0.f, player->m_flDuckSpeed - params->CrouchSpamPenalty);
	}

	// Pressing +duck cancels a bot's auto-duck
	if (player->m_duckUntilOnGround && (mv->Buttons & SRC_IN_DUCK))
	{
		player->m_duckUntilOnGround = false;
	}

	if (!DuckingEnabled())
	{
		mv->Buttons &= ~SRC_IN_DUCK;
	}

	// Bots maintain auto-duck through a jump
	if (player->m_duckUntilOnGround && !player->GroundEntity.IsValid() && player->MoveType != ESrcMoveType::Ladder)
	{
		mv->Buttons |= SRC_IN_DUCK;
	}

	// Forced duck during the bomb-plant animation
	if (player->m_bDuckOverride)
	{
		mv->Buttons |= SRC_IN_DUCK;
	}

	// IN_WALK is commented out of this test in the original
	bool walkButtonIsDown = (mv->Buttons & SRC_IN_SPEED) != 0;

	const bool runButtonIsDown = (mv->Buttons & (SRC_IN_FORWARD | SRC_IN_BACK | SRC_IN_MOVERIGHT | SRC_IN_MOVELEFT | SRC_IN_RUN)) != 0;
	const bool moveForward = (mv->Buttons & SRC_IN_FORWARD) != 0;
	const bool moveBackward = (mv->Buttons & SRC_IN_BACK) != 0;
	const bool moveRight = (mv->Buttons & SRC_IN_MOVERIGHT) != 0;
	const bool moveLeft = (mv->Buttons & SRC_IN_MOVELEFT) != 0;
	const bool opposingForwardBack = (moveForward && moveBackward);
	const bool opposingRightLeft = (moveRight && moveLeft);

	if ((mv->Buttons & SRC_IN_DUCK) || player->m_bDucking || player->HasFlag(SRC_FL_DUCKING))
	{
		walkButtonIsDown = false;
	}

	if (walkButtonIsDown)
	{
		// Hysteretic: the cap only engages within 25 u/s of walk speed
		const srcfloat currentspeed = player->Velocity.Length();
		if (currentspeed < ((mv->MaxSpeed * params->PlayerSpeedWalkModifier) + 25))
		{
			mv->MaxSpeed *= params->PlayerSpeedWalkModifier;
			player->m_bIsWalking = true;
		}
	}
	else
	{
		player->m_bIsWalking = false;
	}

	srcfloat speed_squared = 0.0f;

	if (player->MoveType != ESrcMoveType::Isometric && player->MoveType != ESrcMoveType::NoClip && player->MoveType != ESrcMoveType::Observer)
	{
		// Clamp includes UPMOVE, so upmove steals horizontal budget
		speed_squared = (mv->ForwardMove * mv->ForwardMove) + (mv->SideMove * mv->SideMove) +
				(mv->UpMove * mv->UpMove);
		srcfloat flSpeedFactor = player->m_flSurfaceMaxSpeedFactor;
		const srcfloat flConstraintSpeedFactor = ComputeConstraintSpeedFactor();
		if (flConstraintSpeedFactor < flSpeedFactor)
		{
			flSpeedFactor = flConstraintSpeedFactor;
		}

		if (player->HasFlag(SRC_FL_ONGROUND))
		{
			flSpeedFactor *= player->m_flVelocityModifier;
		}

		mv->MaxSpeed *= flSpeedFactor;

		// The stamina speed scale is SQUARED
		if (player->m_flStamina > 0)
		{
			srcfloat fSpeedScale = SrcClamp(1.0f - player->m_flStamina / params->StaminaRange, 0.f, 1.f);
			fSpeedScale *= fSpeedScale;
			mv->MaxSpeed *= fSpeedScale;
		}

		if (params->bMovementOptimizations)
		{
			// Only take the sqrt when the clamp will actually bite
			if ((speed_squared != 0.0) && (speed_squared > mv->MaxSpeed * mv->MaxSpeed))
			{
				const srcfloat fRatio = mv->MaxSpeed / FastSqrt(speed_squared);
				mv->ForwardMove *= fRatio;
				mv->SideMove *= fRatio;
				mv->UpMove *= fRatio;
			}
		}
		else
		{
			const srcfloat spd = FastSqrt(speed_squared);
			if ((spd != 0.0) && (spd > mv->MaxSpeed))
			{
				const srcfloat fRatio = mv->MaxSpeed / spd;
				mv->ForwardMove *= fRatio;
				mv->SideMove *= fRatio;
				mv->UpMove *= fRatio;
			}
		}
	}

	if (player->HasFlag(SRC_FL_FROZEN) || player->HasFlag(SRC_FL_ONTRAIN) || IsDead())
	{
		mv->ForwardMove = 0;
		mv->SideMove = 0;
		mv->UpMove = 0;
	}

	DecayViewPunchAngle();
	DecayAimPunchAngle();

	if (!IsDead())
	{
		v_angle = mv->Angles;
		v_angle = v_angle + player->m_viewPunchAngle;

		if (player->MoveType != ESrcMoveType::Isometric && player->MoveType != ESrcMoveType::NoClip)
		{
			mv->Angles[SRC_ROLL] = CalcRoll(v_angle, mv->Velocity, params->RollAngle, params->RollSpeed);
		}
		else
		{
			mv->Angles[SRC_ROLL] = 0.0;
		}
		mv->Angles[SRC_PITCH] = v_angle[SRC_PITCH];
		mv->Angles[SRC_YAW] = v_angle[SRC_YAW];
	}
	else
	{
		mv->Angles = mv->OldAngles;
	}

	// CS normalizes the yaw properly here, the base class only subtracted 360 once
	mv->Angles[SRC_YAW] = AngleNormalize(mv->Angles[SRC_YAW]);

	// Not ported: the escape hatch that shoves forwardmove to maxspeed*3 when players stack up
	player->m_iMoveState = ESrcMoveState::Idle;
	if (runButtonIsDown)
	{
		if (opposingForwardBack && opposingRightLeft)
		{
			player->m_iMoveState = ESrcMoveState::Idle;
		}
		else if (opposingForwardBack || opposingRightLeft)
		{
			if ((opposingForwardBack && (moveRight || moveLeft)) || (opposingRightLeft && (moveForward || moveBackward)))
			{
				player->m_iMoveState = ESrcMoveState::Run;
			}
			else
			{
				player->m_iMoveState = ESrcMoveState::Idle;
			}
		}
		else
		{
			player->m_iMoveState = ESrcMoveState::Run;
		}
	}

	if ((player->m_iMoveState == ESrcMoveState::Run) && walkButtonIsDown)
	{
		player->m_iMoveState = ESrcMoveState::Walk;
	}
}

void FSourceMovementSim::ReduceTimers()
{
	// Stamina recovers first, then the legacy millisecond timers below
	if (player->m_flStamina > 0)
	{
		player->m_flStamina -= frametime * params->StaminaRecoveryRate;

		if (player->m_flStamina < 0)
		{
			player->m_flStamina = 0;
		}
	}

	// The integer truncation is deliberate and lossy
	const srcfloat frame_msec = 1000.0f * frametime;
	const int32 nFrameMsec = static_cast<int32>(frame_msec);

	if (player->m_nDuckTimeMsecs > 0)
	{
		player->m_nDuckTimeMsecs -= nFrameMsec;
		if (player->m_nDuckTimeMsecs < 0)
		{
			player->m_nDuckTimeMsecs = 0;
		}
	}
	if (player->m_nDuckJumpTimeMsecs > 0)
	{
		player->m_nDuckJumpTimeMsecs -= nFrameMsec;
		if (player->m_nDuckJumpTimeMsecs < 0)
		{
			player->m_nDuckJumpTimeMsecs = 0;
		}
	}
	if (player->m_nJumpTimeMsecs > 0)
	{
		player->m_nJumpTimeMsecs -= nFrameMsec;
		if (player->m_nJumpTimeMsecs < 0)
		{
			player->m_nJumpTimeMsecs = 0;
		}
	}
	if (player->m_flSwimSoundTime > 0)
	{
		player->m_flSwimSoundTime -= frame_msec;
		if (player->m_flSwimSoundTime < 0)
		{
			player->m_flSwimSoundTime = 0;
		}
	}
}

void FSourceMovementSim::FullNoClipMove(srcfloat factor, srcfloat maxacceleration)
{
	FSrcVec3 wishvel;
	FSrcVec3 forward, right, up;
	FSrcVec3 wishdir;
	srcfloat wishspeed;

	// Maxspeed uses the ORIGINAL factor, before the +speed halving below
	const srcfloat maxspeed = params->MaxSpeed * factor;

	AngleVectors(mv->ViewAngles, &forward, &right, &up);

	if (mv->Buttons & SRC_IN_SPEED)
	{
		factor /= 2.0f;
	}

	const srcfloat fmove = mv->ForwardMove * factor;
	const srcfloat smove = mv->SideMove * factor;

	VectorNormalize(forward);
	VectorNormalize(right);

	for (int32 i = 0; i < 3; i++)
	{
		wishvel[i] = forward[i] * fmove + right[i] * smove;
	}

	// `+=`, not `=`
	wishvel[2] += mv->UpMove * factor;

	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	if (wishspeed > maxspeed)
	{
		VectorScale(wishvel, maxspeed / wishspeed, wishvel);
		wishspeed = maxspeed;
	}

	if (maxacceleration > 0.0f)
	{
		// Same Accelerate as the ground move, so weapon/duck/walk scaling applies to noclip too
		Accelerate(wishdir, wishspeed, maxacceleration);
		const srcfloat spd = VectorLength(mv->Velocity);
		if (spd < 1.0f)
		{
			mv->Velocity.Init();
			return;
		}

		// Hand-rolled friction, control floor is maxspeed/4, not sv_stopspeed
		const srcfloat control = (spd < maxspeed / 4.0f) ? maxspeed / 4.0f : spd;
		const srcfloat friction = params->Friction * player->m_surfaceFriction;
		const srcfloat drop = control * friction * frametime;
		srcfloat newspeed = spd - drop;
		if (newspeed < 0)
		{
			newspeed = 0;
		}

		newspeed /= spd;
		VectorScale(mv->Velocity, newspeed, mv->Velocity);
	}
	else
	{
		VectorCopy(wishvel, mv->Velocity);
	}

	// Just move - no clipping, no trace, no ground
	FSrcVec3 out;
	VectorMA(mv->GetAbsOrigin(), frametime, mv->Velocity, out);
	mv->SetAbsOrigin(out);

	// Only STRICTLY negative zeroes velocity; == 0 keeps it
	if (maxacceleration < 0.0f)
	{
		mv->Velocity.Init();
	}
}

void FSourceMovementSim::FullWalkMove()
{
	if (!CheckWater())
	{
		StartGravity();
	}

	if (player->m_flWaterJumpTime)
	{
		// Not ported: the water jump
		return;
	}

	if (player->WaterLevel >= ESrcWaterLevel::Waist)
	{
		// Not ported: swimming
		return;
	}

	if (mv->Buttons & SRC_IN_JUMP)
	{
		CheckJumpButton();
	}
	else
	{
		mv->OldButtons &= ~SRC_IN_JUMP;
	}

	if (player->GroundEntity.IsValid())
	{
		// Velocity.z is zeroed here, and again after FinishGravity below
		mv->Velocity.z = 0.0;
		player->m_flFallVelocity = 0.0f;
		Friction();
	}

	CheckVelocity();

	if (player->GroundEntity.IsValid())
	{
		WalkMove();
		player->m_bHasWalkMovedSinceLastJump = true;
	}
	else
	{
		AirMove();
	}

	// Ground state is (re)determined AFTER the move
	CategorizePosition();

	CheckVelocity();
	if (!CheckWater())
	{
		FinishGravity();
	}

	if (player->GroundEntity.IsValid())
	{
		mv->Velocity.z = 0;
	}

	CheckFalling();

	// Not ported: the water-transition splash, which is audio only
}

// Gravity

void FSourceMovementSim::StartGravity()
{
	srcfloat ent_gravity;

	// The test is on the VALUE, not on "has an override"
	if (player->m_flGravityScale)
	{
		ent_gravity = player->m_flGravityScale;
	}
	else
	{
		ent_gravity = 1.0;
	}

	mv->Velocity[2] -= (ent_gravity * params->Gravity * 0.5f * frametime);
	mv->Velocity[2] += player->BaseVelocity[2] * frametime;
	FSrcVec3 temp = player->BaseVelocity;
	temp[2] = 0;
	player->BaseVelocity = temp;

	CheckVelocity();
}

void FSourceMovementSim::FinishGravity()
{
	if (player->m_flWaterJumpTime)
	{
		return;
	}

	srcfloat ent_gravity;

	if (player->m_flGravityScale)
	{
		ent_gravity = player->m_flGravityScale;
	}
	else
	{
		ent_gravity = 1.0;
	}

	mv->Velocity[2] -= (ent_gravity * params->Gravity * frametime * 0.5f);

	CheckVelocity();
}

void FSourceMovementSim::CheckVelocity()
{
	FSrcVec3 org = mv->GetAbsOrigin();

	for (int32 i = 0; i < 3; i++)
	{
		if (IsNan(mv->Velocity[i]))
		{
			mv->Velocity[i] = 0;
		}

		if (IsNan(org[i]))
		{
			org[i] = 0;
			mv->SetAbsOrigin(org);
		}

		// This is a PER-AXIS clamp, not a magnitude clamp
		if (mv->Velocity[i] > params->MaxVelocity)
		{
			mv->Velocity[i] = params->MaxVelocity;
		}
		else if (mv->Velocity[i] < -params->MaxVelocity)
		{
			mv->Velocity[i] = -params->MaxVelocity;
		}
	}
}

void FSourceMovementSim::CheckFalling()
{
	if (!player->GroundEntity.IsValid() || player->m_flFallVelocity <= 0)
	{
		return;
	}

	if (!IsDead() && player->m_flFallVelocity >= FSourceMovementParams::PlayerFallPunchThreshold)
	{
		srcfloat fvol = 0.5f;

		if (player->WaterLevel > ESrcWaterLevel::NotInWater)
		{
			// Landed in water - no effects
		}
		else
		{
			const FSrcVec3 GroundVel = world->GetEntityAbsVelocity(player->GroundEntity);
			if (GroundVel.z < 0.0f)
			{
				player->m_flFallVelocity += GroundVel.z;
				player->m_flFallVelocity = SrcMax(0.1f, player->m_flFallVelocity);
			}

			if (player->m_flFallVelocity > FSourceMovementParams::PlayerMaxSafeFallSpeed)
			{
				// Source deals fall damage here too. That is game rules, so only the sound is left.
				fvol = 1.0f;
			}
			else if (player->m_flFallVelocity > FSourceMovementParams::PlayerMaxSafeFallSpeed / 2)
			{
				fvol = 0.85f;
			}
			else if (player->m_flFallVelocity < FSourceMovementParams::PlayerMinBounceSpeed)
			{
				fvol = 0;
			}
		}

		PlayerRoughLandingEffects(fvol);
	}

	// The landing view punch CS adds on top of the base game
	{
		const srcfloat flFallVel = player->m_flFallVelocity;
		if (flFallVel > 16.0f && flFallVel <= FSourceMovementParams::PlayerFatalFallSpeed)
		{
			FSrcAngles punchAngle = player->m_viewPunchAngle;
			punchAngle.x = (flFallVel * 0.001f);

			if (punchAngle.x < 0.75f)
			{
				punchAngle.x = 0.75f;
			}

			player->m_viewPunchAngle = punchAngle;
		}
	}

	OnLand(player->m_flFallVelocity);
	player->m_flFallVelocity = 0;
	Telemetry.bLandedThisTick = true;
}

void FSourceMovementSim::PlayerRoughLandingEffects(srcfloat fvol)
{
	if (fvol > 0.0)
	{
		// The original sets ROLL and then tests PITCH, so the clamp below never fires
		player->m_viewPunchAngle[SRC_ROLL] =
			(player->m_flFallVelocity - FSourceMovementParams::PlayerMaxSafeFallSpeed) * 0.013f;

		if (player->m_viewPunchAngle[SRC_PITCH] > 8)
		{
			player->m_viewPunchAngle[SRC_PITCH] = 8;
		}
	}
}

srcfloat FSourceMovementSim::CalcRoll(const FSrcAngles& angles, const FSrcVec3& velocity, srcfloat rollangle, srcfloat rollspeed)
{
	FSrcVec3 forward, right, up;
	AngleVectors(angles, &forward, &right, &up);
	srcfloat side = DotProduct(velocity, right);
	const srcfloat sign = side < 0 ? -1.f : 1.f;
	side = FMath::Abs(side);
	const srcfloat value = rollangle;
	if (side < rollspeed)
	{
		side = side * value / rollspeed;
	}
	else
	{
		side = value;
	}

	return side * sign;
}

void FSourceMovementSim::DecayViewPunchAngle()
{
	// Driven by raw TICK_INTERVAL, not by the lag-scaled frametime
	FSrcAngles punchAngle = player->m_viewPunchAngle;
	DecayAngles(punchAngle, params->ViewPunchDecay, 0.0f, params->TickInterval);
	player->m_viewPunchAngle = punchAngle;
}

void FSourceMovementSim::DecayAimPunchAngle()
{
	FSrcAngles punchAngle = player->m_aimPunchAngle;
	FSrcAngles punchAngleVel = player->m_aimPunchAngleVel;

	DecayAngles(punchAngle, params->WeaponRecoilDecay2Exp, params->WeaponRecoilDecay2Lin, params->TickInterval);

	// A symmetric (half-step, decay, half-step) velocity integration
	punchAngle += punchAngleVel * params->TickInterval * 0.5f;
	punchAngleVel *= std::exp(params->TickInterval * -params->WeaponRecoilVelDecay);
	punchAngle += punchAngleVel * params->TickInterval * 0.5f;
	player->m_aimPunchAngle = punchAngle;
	player->m_aimPunchAngleVel = punchAngleVel;
}

srcfloat FSourceMovementSim::ComputeConstraintSpeedFactor()
{
	if (!mv || mv->ConstraintRadius == 0.0f)
	{
		return 1.0f;
	}

	const FSrcVec3 Delta0 = mv->GetAbsOrigin() - mv->ConstraintCenter;
	const srcfloat flDistSq = Delta0.LengthSqr();
	const srcfloat flOuterRadiusSq = mv->ConstraintRadius * mv->ConstraintRadius;
	srcfloat flInnerRadiusSq = mv->ConstraintRadius - mv->ConstraintWidth;
	flInnerRadiusSq *= flInnerRadiusSq;

	if ((flDistSq <= flInnerRadiusSq) || (flDistSq >= flOuterRadiusSq))
	{
		return 1.0f;
	}

	// Only slow down when running AWAY from the centre
	FSrcVec3 vecDesired;
	VectorMultiply(m_vecForward, mv->ForwardMove, vecDesired);
	VectorMA(vecDesired, mv->SideMove, m_vecRight, vecDesired);
	VectorMA(vecDesired, mv->UpMove, m_vecUp, vecDesired);
	FSrcVec3 vecDelta;
	VectorSubtract(mv->GetAbsOrigin(), mv->ConstraintCenter, vecDelta);
	VectorNormalize(vecDelta);
	VectorNormalize(vecDesired);
	if (DotProduct(vecDelta, vecDesired) < 0.0f)
	{
		return 1.0f;
	}

	const srcfloat flFrac = (FastSqrt(flDistSq) - (mv->ConstraintRadius - mv->ConstraintWidth)) / mv->ConstraintWidth;

	return 1.0f + (mv->ConstraintSpeedFactor - 1.0f) * flFrac;
}

bool FSourceMovementSim::IsDead() const
{
	return player->m_iHealth <= 0;
}

const FSrcVec3& FSourceMovementSim::GetPlayerMins() const
{
	// Selects on m_bDucked alone, not FL_DUCKING and not m_flDuckAmount
	return params->Hulls.GetPlayerMins(player->m_bDucked);
}

const FSrcVec3& FSourceMovementSim::GetPlayerMaxs() const
{
	return params->Hulls.GetPlayerMaxs(player->m_bDucked);
}

const FSrcVec3& FSourceMovementSim::GetPlayerViewOffset(bool bDucked) const
{
	return params->Hulls.GetPlayerViewOffset(bDucked);
}

int32 FSourceMovementSim::PlayerSolidMask(bool bBrushOnly) const
{
	const bool isBot = player->bIsBot;
	int32 uMask = SRC_MASK_PLAYERSOLID_BRUSHONLY;

	if (!bBrushOnly)
	{
		uMask = params->PlayerSolidMask;
	}

	if (isBot)
	{
		uMask |= SRC_CONTENTS_MONSTERCLIP;
	}

	return uMask;
}

FSourceTraceFilter FSourceMovementSim::MakeTraceFilter() const
{
	FSourceTraceFilter Filter;
	Filter.SkipEntityId = player->EntIndex;
	Filter.SkipEntityId2 = INDEX_NONE;
	Filter.TeamMask = params->PlayerSolidMask & (SRC_CONTENTS_TEAM1 | SRC_CONTENTS_TEAM2);
	Filter.bPlayerMovementGroup = true;
	return Filter;
}

void FSourceMovementSim::TracePlayerBBox(const FSrcVec3& start, const FSrcVec3& end, int32 fMask, FSourceTraceResult& pm)
{
	++m_nTraceCount;
	world->TraceHull(start, end, GetPlayerMins(), GetPlayerMaxs(), fMask, MakeTraceFilter(), pm);
}

bool FSourceMovementSim::TestPlayerPosition(const FSrcVec3& pos, FSourceTraceResult& pm)
{
	++m_nTraceCount;
	world->TraceHull(pos, pos, GetPlayerMins(), GetPlayerMaxs(), PlayerSolidMask(), MakeTraceFilter(), pm);

	return (pm.Contents & PlayerSolidMask()) != 0 && pm.Entity.IsValid();
}

bool FSourceMovementSim::CheckInterval_Stuck() const
{
	if (!params->bMovementOptimizations)
	{
		return true;
	}

	// The branch is a transcribed ternary, so the first arm assigning 1 to a 1 is not a mistake
	int32 tickInterval = 1;
	if (player->m_StuckLast != 0)
	{
		tickInterval = 1;
	}
	else
	{
		// Once a second, versus every tick once we are already stuck
		tickInterval = static_cast<int32>(1.0f / params->TickInterval);
	}

	if (tickInterval <= 0)
	{
		tickInterval = 1;
	}

	// Offsetting by the entity index staggers the check so players do not all test on one tick
	return ((player->m_nTickBase + player->EntIndex) % tickInterval) == 0;
}

// Stubs. The call sites above are already in the right place.

int32 FSourceMovementSim::CheckStuck()
{
	// Not ported: the 54-entry jitter table that nudges a stuck player free
	return 0;
}

bool FSourceMovementSim::CheckWater()
{
	// Stub: nothing ever raises the water level, so this only reports what was set
	return player->WaterLevel > ESrcWaterLevel::Feet;
}

bool FSourceMovementSim::LadderMove()
{
	// Stub: ladders are not ported, so the player is never on one
	return false;
}
