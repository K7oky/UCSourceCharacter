#include "Core/SourceMovementSim.h"

#include "Core/ISourceWorldQuery.h"
#include "Core/SourceMovementParams.h"

void FSourceMovementSim::GetStepSoundVelocities(srcfloat* velwalk, srcfloat* velrun) const
{
	if (player->HasFlag(SRC_FL_DUCKING) || player->MoveType == ESrcMoveType::Ladder)
	{
		*velwalk = 60;
		*velrun = 80;
	}
	else
	{
		*velwalk = 90;
		*velrun = 220;
	}
}

void FSourceMovementSim::SetStepSoundTime(ESrcStepSoundTime iStepSoundTime, bool bWalking)
{
	switch (iStepSoundTime)
	{
	case ESrcStepSoundTime::Normal:
	case ESrcStepSoundTime::WaterFoot:
		player->m_flStepSoundTime = bWalking ? 400 : 300;
		break;

	case ESrcStepSoundTime::OnLadder:
		player->m_flStepSoundTime = 200;
		break;

	case ESrcStepSoundTime::WaterKnee:
		player->m_flStepSoundTime = 600;
		break;

	default:
		// Source asserts here
		break;
	}

	player->m_flStepSoundTime *= params->FootstepSoundFrequency;

	// +100 ms lands after the frequency scale, so the cvar does not apply to it
	if (player->HasFlag(SRC_FL_DUCKING) || player->MoveType == ESrcMoveType::Ladder)
	{
		player->m_flStepSoundTime += 100;
	}
}

void FSourceMovementSim::PlayStepSound(srcfloat fvol)
{
	// Records the step, the audio component is what actually plays it
	if (!params->bFootsteps)
	{
		return;
	}

	Events.bStepSound = true;
	Events.StepVolume = fvol;
	Events.SurfacePropsId = player->m_surfaceProps;
}

void FSourceMovementSim::UpdateStepSound()
{
	if (IsDead())
	{
		return;
	}

	const srcfloat speedSqr = mv->Velocity.LengthSqr();
	const srcfloat flWalkSpeed = params->PlayerSpeedRun * params->PlayerSpeedWalkModifier;

	if ((speedSqr < flWalkSpeed * flWalkSpeed) || player->m_bIsWalking)
	{
		if (speedSqr < 10.0f)
		{
			// Stopping resets the step tracking, so the next step fires immediately
			SetStepSoundTime(ESrcStepSoundTime::Normal, false);
		}

		return;  // player is not running, no footsteps
	}

	UpdateStepSound_BaseClass();
}

void FSourceMovementSim::UpdateStepSound_BaseClass()
{
	bool bWalking;
	srcfloat fvol;
	srcfloat speed;
	srcfloat velrun;
	srcfloat velwalk;
	bool fLadder;

	// Ticks down before the early-outs, so it runs while standing still too
	if (player->m_flStepSoundTime > 0)
	{
		player->m_flStepSoundTime -= 1000.0f * frametime;
		if (player->m_flStepSoundTime < 0)
		{
			player->m_flStepSoundTime = 0;
		}
	}

	if (player->m_flStepSoundTime > 0)
	{
		return;
	}

	if (player->HasFlag(SRC_FL_FROZEN) || player->HasFlag(SRC_FL_ATCONTROLS))
	{
		return;
	}

	if (player->MoveType == ESrcMoveType::NoClip || player->MoveType == ESrcMoveType::Observer)
	{
		return;
	}

	if (!params->bFootsteps)
	{
		return;
	}

	// Walk/run threshold uses the 3D speed, "is moving" uses the 2D one
	speed = mv->Velocity.Length();
	const srcfloat groundspeed = mv->Velocity.Length2D();
	fLadder = (player->MoveType == ESrcMoveType::Ladder);

	GetStepSoundVelocities(&velwalk, &velrun);
	const bool onground = player->GroundEntity.IsValid();
	const bool movingalongground = (groundspeed > 0.0001f);
	const bool moving_fast_enough = (speed >= velwalk);

	if (!moving_fast_enough || !(fLadder || (onground && movingalongground)))
	{
		return;
	}

	bWalking = speed < velrun;

	// Not ported: the ladder and water step timings, so everything uses the normal one
	SetStepSoundTime(ESrcStepSoundTime::Normal, bWalking);

	// No surfaceproperties table, so every surface gets the CONCRETE numbers
	fvol = bWalking ? params->FootstepVolumeWalk : params->FootstepVolumeRun;
	if (player->HasFlag(SRC_FL_DUCKING))
	{
		fvol *= 0.65f;
	}

	PlayStepSound(fvol);
}
