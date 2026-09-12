// Velocity model: friction, ground/air acceleration, walk/air move, jump, stamina

#include "Core/SourceMovementSim.h"

using namespace SourceMath;

void FSourceMovementSim::Friction()
{
	srcfloat speed, newspeed, control;
	srcfloat friction = 0.f;  // Source leaves this uninitialized; only read on the ground path
	srcfloat drop;

	if (player->m_flWaterJumpTime)
	{
		return;
	}

	speed = VectorLength(mv->Velocity);

	// Under 0.1 u/s no friction at all, and velocity is not snapped to zero
	if (speed < 0.1f)
	{
		return;
	}

	drop = 0;

	// The ground check is repeated here even though FullWalkMove already gated the call
	if (player->GroundEntity.IsValid())
	{
		friction = params->Friction * player->m_surfaceFriction;

		if (params->bIsGameConsole)
		{
			if (player->m_bDucked)
			{
				control = (speed < params->StopSpeed) ? params->StopSpeed : speed;
			}
			else
			{
				control = (speed < params->StopSpeed) ? (params->StopSpeed * 2.0f) : speed;
			}
		}
		else
		{
			control = (speed < params->StopSpeed) ? params->StopSpeed : speed;
		}

		drop += control * friction * frametime;
	}

	newspeed = speed - drop;
	if (newspeed < 0)
	{
		newspeed = 0;
	}

	if (newspeed != speed)
	{
		// newspeed is converted in place from an absolute speed into a ratio
		newspeed /= speed;
		VectorScale(mv->Velocity, newspeed, mv->Velocity);
	}

	// newspeed is a ratio only when the branch above ran, otherwise still an absolute speed
	mv->OutWishVel -= (1.f - newspeed) * mv->Velocity;
	Telemetry.FrictionDrop = drop;
	Telemetry.AppliedFriction = friction;
}

// Acceleration

bool FSourceMovementSim::CanAccelerate()
{
	if (player->deadflag)
	{
		return false;
	}

	return player->m_flWaterJumpTime == 0;
}

void FSourceMovementSim::Accelerate(FSrcVec3& wishdir, srcfloat wishspeed, srcfloat accel)
{
	// The exponential acceleration branch is dead: its time constant is a compile-time zero
	if (!CanAccelerate())
	{
		return;
	}

	srcfloat flStoredAccel = accel;
	const srcfloat currentspeed = mv->Velocity.Dot(wishdir);

	// Reduce wishspeed by the amount of veer
	const srcfloat addspeed = wishspeed - currentspeed;

	if (addspeed <= 0)
	{
		return;
	}

	// Any of the three duck states counts, not just the finished one
	const bool bIsDucking = (mv->Buttons & SRC_IN_DUCK) || player->m_bDucking || player->HasFlag(SRC_FL_DUCKING);

	// IN_WALK is commented out in the original
	const bool bIsWalking = ((mv->Buttons & SRC_IN_SPEED) != 0) && !bIsDucking;

	// 250 is hard-coded in Accelerate, unrelated to sv_maxspeed and to SPEED_RUN
	const srcfloat flMaxSpeed = params->AccelerateReferenceSpeed;
	srcfloat fAccelerationScale = SrcMax(flMaxSpeed, wishspeed);
	srcfloat flGoalSpeed = fAccelerationScale;
	bool bIsSlowSniperScoped = false;

	if (params->bAccelerateUseWeaponSpeed && params->WeaponMaxSpeed >= 0.0f)
	{
		const srcfloat WeaponMax = params->WeaponMaxSpeed;

		bIsSlowSniperScoped = (params->WeaponZoomLevel > 0 && params->WeaponZoomLevels > 1 && (WeaponMax * params->PlayerSpeedWalkModifier) < 110.0f);

		flGoalSpeed *= SrcMin(1.0f, (WeaponMax / flMaxSpeed));

		if ((!bIsDucking && !bIsWalking) || ((bIsWalking || bIsDucking) && bIsSlowSniperScoped))
		{
			fAccelerationScale *= SrcMin(1.0f, (WeaponMax / flMaxSpeed));
		}
	}

	if (bIsDucking)
	{
		if (!bIsSlowSniperScoped)
		{
			fAccelerationScale *= params->PlayerSpeedDuckModifier;
		}

		flGoalSpeed *= params->PlayerSpeedDuckModifier;
	}

	if (bIsWalking)
	{
		if (!bIsSlowSniperScoped)
		{
			fAccelerationScale *= params->PlayerSpeedWalkModifier;
		}

		flGoalSpeed *= params->PlayerSpeedWalkModifier;
	}

	// The walk taper
	if (bIsWalking && currentspeed > (flGoalSpeed - 5))
	{
		flStoredAccel *= SrcClamp(1.0f - (SrcMax(0.0f, currentspeed - (flGoalSpeed - 5)) / SrcMax(0.0f, flGoalSpeed - (flGoalSpeed - 5))), 0.0f, 1.0f);
	}

	srcfloat accelspeed = flStoredAccel * frametime * fAccelerationScale * player->m_surfaceFriction;

	if (accelspeed > addspeed)
	{
		accelspeed = addspeed;
	}

	mv->Velocity += (accelspeed * wishdir);
	player->m_flGroundAccelLinearFracLastTime = curtime;
	Telemetry.AccelSpeed = accelspeed;

	UpdateTrailingVelocity();
}

void FSourceMovementSim::UpdateTrailingVelocity()
{
	if (mv->TrailingVelocity.IsZero() || (curtime - mv->TrailingVelocityTime) > 0.35f)
	{
		mv->TrailingVelocity = mv->Velocity;
		mv->TrailingVelocityTime = curtime;
		return;
	}

	FSrcVec3 vNormalizedCurrent(mv->Velocity.x, mv->Velocity.y, 0.f);
	VectorNormalize(vNormalizedCurrent);

	FSrcVec3 vNormalizedPrev(mv->TrailingVelocity.x, mv->TrailingVelocity.y, 0.f);
	VectorNormalize(vNormalizedPrev);
	const srcfloat flDot = vNormalizedCurrent.x * vNormalizedPrev.x + vNormalizedCurrent.y * vNormalizedPrev.y;

	if (flDot > 0.8f)
	{
		// Same direction: latch the larger magnitude
		if (mv->TrailingVelocity.Length2DSqr() < mv->Velocity.Length2DSqr())
		{
			mv->TrailingVelocity = mv->Velocity;
			mv->TrailingVelocityTime = curtime;
		}
	}
	else if (flDot < -0.8f)
	{
		// Opposite direction: the fishtail test
		const srcfloat PrevLen = mv->TrailingVelocity.Length2D();
		if (PrevLen < 225.0f && PrevLen > 115.0f && mv->Velocity.Length2D() > 115.0f)
		{
			FSrcVec3 vEyeForward;
			AngleVectors(mv->ViewAngles, &vEyeForward);
			const srcfloat flEyeDot = vEyeForward.x * vNormalizedCurrent.x + vEyeForward.y * vNormalizedCurrent.y;
			if (flEyeDot > -0.3f && flEyeDot < 0.3f)
			{
				mv->TrailingVelocity = mv->Velocity;
				mv->TrailingVelocityTime = curtime;
			}
		}
	}
}

void FSourceMovementSim::AirAccelerate(FSrcVec3& wishdir, srcfloat wishspeed, srcfloat accel)
{
	srcfloat addspeed, accelspeed, currentspeed;
	srcfloat wishspd;
	wishspd = wishspeed;

	if (player->deadflag)
	{
		return;
	}

	if (player->m_flWaterJumpTime)
	{
		return;
	}

	// This is the air-strafe mechanism: only the first 30 u/s of wish speed can be added
	if (wishspd > 30)
	{
		wishspd = 30;
	}

	currentspeed = mv->Velocity.Dot(wishdir);
	addspeed = wishspd - currentspeed;

	if (addspeed <= 0)
	{
		return;
	}

	// But the amount actually applied is computed from the UNCAPPED wishspeed
	accelspeed = accel * wishspeed * frametime * player->m_surfaceFriction;

	if (accelspeed > addspeed)
	{
		accelspeed = addspeed;
	}

	for (int32 i = 0; i < 3; i++)
	{
		mv->Velocity[i] += accelspeed * wishdir[i];
		mv->OutWishVel[i] += accelspeed * wishdir[i];
	}

	Telemetry.AccelSpeed = accelspeed;
}

void FSourceMovementSim::WalkMove()
{
	FSrcVec3 wishvel;
	srcfloat spd;
	srcfloat fmove, smove;
	FSrcVec3 wishdir;
	srcfloat wishspeed;
	FSrcVec3 dest;
	FSourceTraceResult pm;
	FSrcVec3 forward, right, up;

	AngleVectors(mv->ViewAngles, &forward, &right, &up);

	// Latched before the move, because stair stepping is only allowed if we started grounded
	const FSrcEntityRef oldground = player->GroundEntity;
	fmove = mv->ForwardMove;
	smove = mv->SideMove;

	if (params->bMovementOptimizations)
	{
		// The optimized path re-normalizes ONLY when z was non-zero
		if (forward[2] != 0)
		{
			forward[2] = 0;
			VectorNormalize(forward);
		}

		if (right[2] != 0)
		{
			right[2] = 0;
			VectorNormalize(right);
		}
	}
	else
	{
		forward[2] = 0;
		right[2] = 0;

		VectorNormalize(forward);
		VectorNormalize(right);
	}

	for (int32 i = 0; i < 2; i++)
	{
		wishvel[i] = forward[i] * fmove + right[i] * smove;
	}

	wishvel[2] = 0;

	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	if ((wishspeed != 0.0f) && (wishspeed > mv->MaxSpeed))
	{
		VectorScale(wishvel, mv->MaxSpeed / wishspeed, wishvel);
		wishspeed = mv->MaxSpeed;
	}

	// Velocity.z is zeroed both BEFORE and AFTER Accelerate
	mv->Velocity[2] = 0;
	Accelerate(wishdir, wishspeed, params->Accelerate);
	mv->Velocity[2] = 0;

	// The hard ground speed cap
	if (mv->Velocity.LengthSqr() > mv->MaxSpeed * mv->MaxSpeed)
	{
		const srcfloat fRatio = mv->MaxSpeed / mv->Velocity.Length();
		mv->Velocity *= fRatio;
	}

	VectorAdd(mv->Velocity, player->BaseVelocity, mv->Velocity);
	spd = VectorLength(mv->Velocity);

	// Below 1 u/s the player is frozen and StayOnGround is skipped, so no snap-down on a slope
	if (spd < 1.0f)
	{
		mv->Velocity.Init();
		VectorSubtract(mv->Velocity, player->BaseVelocity, mv->Velocity);
		return;
	}

	// Try the flat destination first - note dest.z is the CURRENT z, not z + vz*dt
	dest[0] = mv->GetAbsOrigin()[0] + mv->Velocity[0] * frametime;
	dest[1] = mv->GetAbsOrigin()[1] + mv->Velocity[1] * frametime;
	dest[2] = mv->GetAbsOrigin()[2];

	TracePlayerBBox(mv->GetAbsOrigin(), dest, PlayerSolidMask(), pm);
	mv->OutWishVel += wishdir * wishspeed;
	Telemetry.WishDir = wishdir;
	Telemetry.WishVel = wishvel;
	Telemetry.WishSpeed = wishspeed;

	if (pm.Fraction == 1)
	{
		mv->SetAbsOrigin(pm.EndPos);
		VectorSubtract(mv->Velocity, player->BaseVelocity, mv->Velocity);

		StayOnGround();
		return;
	}

	// Do not walk up stairs if we were not on the ground when the move started
	if (!oldground.IsValid() && player->WaterLevel == ESrcWaterLevel::NotInWater)
	{
		VectorSubtract(mv->Velocity, player->BaseVelocity, mv->Velocity);
		return;
	}

	if (player->m_flWaterJumpTime)
	{
		VectorSubtract(mv->Velocity, player->BaseVelocity, mv->Velocity);
		return;
	}

	StepMove(dest, pm);

	VectorSubtract(mv->Velocity, player->BaseVelocity, mv->Velocity);

	StayOnGround();
}

void FSourceMovementSim::AirMove()
{
	FSrcVec3 wishvel;
	srcfloat fmove, smove;
	FSrcVec3 wishdir;
	srcfloat wishspeed;
	FSrcVec3 forward, right, up;

	AngleVectors(mv->ViewAngles, &forward, &right, &up);
	fmove = mv->ForwardMove;
	smove = mv->SideMove;

	// This path always re-normalizes, even when z was already zero
	forward[2] = 0;
	right[2] = 0;
	VectorNormalize(forward);
	VectorNormalize(right);

	for (int32 i = 0; i < 2; i++)
	{
		wishvel[i] = forward[i] * fmove + right[i] * smove;
	}
	wishvel[2] = 0;

	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	if (wishspeed != 0 && (wishspeed > mv->MaxSpeed))
	{
		VectorScale(wishvel, mv->MaxSpeed / wishspeed, wishvel);
		wishspeed = mv->MaxSpeed;
	}

	Telemetry.WishDir = wishdir;
	Telemetry.WishVel = wishvel;
	Telemetry.WishSpeed = wishspeed;

	// There is NO equivalent of WalkMove's post-Accelerate max-speed clamp here
	AirAccelerate(wishdir, wishspeed, params->AirAccelerate);

	VectorAdd(mv->Velocity, player->BaseVelocity, mv->Velocity);

	TryPlayerMove();

	VectorSubtract(mv->Velocity, player->BaseVelocity, mv->Velocity);
}

// Jump

void FSourceMovementSim::PreventBunnyJumping()
{
	// Built from the latched spawn max speed, not the per-tick one that walk and duck scale
	const srcfloat maxscaledspeed = params->BunnyJumpMaxSpeedFactor * player->m_flMaxspeed;
	if (maxscaledspeed <= 0.0f)
	{
		return;
	}

	// 3D speed, the vertical component counts
	const srcfloat spd = mv->Velocity.Length();

	if (spd <= maxscaledspeed)
	{
		return;
	}

	const srcfloat fraction = (maxscaledspeed / spd);
	mv->Velocity *= fraction;
}

bool FSourceMovementSim::CheckJumpButton()
{
	if (player->deadflag)
	{
		mv->OldButtons |= SRC_IN_JUMP;
		return false;
	}

	if (player->m_flWaterJumpTime)
	{
		player->m_flWaterJumpTime -= frametime;
		if (player->m_flWaterJumpTime < 0)
		{
			player->m_flWaterJumpTime = 0;
		}

		return false;
	}

	// Not ported: the gate that blocks jumping during a taunt

	if (player->WaterLevel >= ESrcWaterLevel::Waist)
	{
		SetGroundEntity(nullptr);

		if (player->WaterType == SRC_CONTENTS_WATER)
		{
			mv->Velocity[2] = 100;
		}
		else if (player->WaterType == SRC_CONTENTS_SLIME)
		{
			mv->Velocity[2] = 80;
		}

		if (player->m_flSwimSoundTime <= 0)
		{
			player->m_flSwimSoundTime = 1000;
		}

		return false;
	}

	// Jumping off another player's head
	bool bStandingOnOtherPlayer = false;
	bool bStandingOnFallingPlayer = false;
	{
		const FSrcEntityRef& groundEntity = player->GroundEntity;
		if (groundEntity.IsValid() && !groundEntity.bIsWorld && groundEntity.bIsPlayer)
		{
			bStandingOnOtherPlayer = true;

			// Always false: that needs the other pawn's ground state, which is not tracked
			bStandingOnFallingPlayer = false;
		}
	}

	player->m_bHasWalkMovedSinceLastJump = false;

	if (!player->GroundEntity.IsValid())
	{
		mv->OldButtons |= SRC_IN_JUMP;
		return false;
	}

	// "don't pogo stick" - sv_autobunnyhopping bypasses it
	if ((mv->OldButtons & SRC_IN_JUMP) != 0 && !params->bAutoBunnyHopping)
	{
		return false;
	}

	if (!params->bEnableBunnyHopping)
	{
		PreventBunnyJumping();
	}

	SetGroundEntity(nullptr);

	// "if we're walking or standing still, play only a local sound"
	if (mv->Velocity.Length() > FSourceMovementParams::JumpStepSoundSpeed)
	{
		// Forced past the m_flStepSoundTime gate (Source's bForce)
		PlayStepSound(1.0f);
	}

	Events.bJumpSound = true;
	Events.SurfacePropsId = player->m_surfaceProps;

	// A surface can scale the jump; with no surface under us the factor stays 1
	srcfloat flGroundFactor = 1.0f;
	if (player->m_surfaceProps != INDEX_NONE)
	{
		flGroundFactor = player->m_flSurfaceJumpFactor;
	}

	// Bots and hostages crouch-jump programmatically
	if (player->bIsBot && !(mv->Buttons & SRC_IN_DUCK))
	{
		player->m_duckUntilOnGround = true;
		FinishDuck();
	}

	// startz is sampled after StartGravity already subtracted half a gravity step
	const srcfloat startz = mv->Velocity[2];

	if (bStandingOnFallingPlayer)
	{
		// No impulse at all: jumping off a falling player just kills your vertical velocity
		mv->Velocity[2] = 0.0f;
	}
	else if (player->m_duckUntilOnGround || player->m_bDucking || player->HasFlag(SRC_FL_DUCKING) || bStandingOnOtherPlayer)
	{
		// ASSIGNMENT, not +=
		mv->Velocity[2] = flGroundFactor * params->JumpImpulse;
	}
	else
	{
		// Standing: the impulse ADDS to the existing velocity
		mv->Velocity[2] += flGroundFactor * params->JumpImpulse;
	}

	// Stamina scales the result after the impulse, linearly, not squared
	if (player->m_flStamina > 0)
	{
		mv->Velocity[2] *= SrcClamp(1.0f - player->m_flStamina / params->StaminaRange, 0.f, 1.f);
	}

	// Gravity lands a second time on the jump tick, so a jump gets one and a half steps of pull
	FinishGravity();

	// The delta goes in OutWishVel.z, not OutJumpVel, and OnJump reads it from there
	mv->OutWishVel.z += mv->Velocity[2] - startz;
	mv->OutStepHeight += 0.1f;

	OnJump(mv->OutWishVel.z);
	mv->OldButtons |= SRC_IN_JUMP;
	Telemetry.bJumpedThisTick = true;
	return true;
}

void FSourceMovementSim::OnJump(srcfloat fImpulse)
{
	const srcfloat flStamCost = params->StaminaJumpCost;
	player->m_flStamina = SrcClamp(player->m_flStamina + flStamCost * fImpulse, 0.0f, params->StaminaMax);

	// Not ported: the original also tells the active weapon about the jump
}

void FSourceMovementSim::OnLand(srcfloat fVelocity)
{
	player->m_flStamina = SrcClamp(player->m_flStamina + params->StaminaLandCost * fVelocity, 0.0f, params->StaminaMax);

	if (fVelocity > FSourceMovementParams::LandSoundFallVelocity)
	{
		Events.bLandSound = true;
		Events.LandVelocity = fVelocity;
		Events.SurfacePropsId = player->m_surfaceProps;
	}
}
