#include "Core/SourceMovementSim.h"

using namespace SourceMath;

void FSourceMovementSim::GetWaterCheckPosition(ESrcWaterLevel waterLevel, FSrcVec3& pos) const
{
	pos[0] = mv->GetAbsOrigin()[0] + (GetPlayerMins()[0] + GetPlayerMaxs()[0]) * 0.5f;
	pos[1] = mv->GetAbsOrigin()[1] + (GetPlayerMins()[1] + GetPlayerMaxs()[1]) * 0.5f;

	switch (waterLevel)
	{
	case ESrcWaterLevel::Eyes:
		pos[2] = mv->GetAbsOrigin()[2] + player->ViewOffset[2];
		return;

	case ESrcWaterLevel::Waist:
		pos[2] = mv->GetAbsOrigin()[2] + (GetPlayerMins()[2] + GetPlayerMaxs()[2]) * 0.5f;
		return;

	default:
		pos[2] = mv->GetAbsOrigin()[2] + GetPlayerMins()[2] + 1;
	}
}

bool FSourceMovementSim::CheckWater()
{
	FSrcVec3 point;

	GetWaterCheckPosition(ESrcWaterLevel::Feet, point);

	player->WaterLevel = ESrcWaterLevel::NotInWater;
	player->WaterType = SRC_CONTENTS_EMPTY;

	// Source caches these three lookups per tick, we just ask
	int32 cont = world->GetPointContents(point, SRC_MASK_WATER);

	if (cont & SRC_MASK_WATER)
	{
		player->WaterType = cont;
		player->WaterLevel = ESrcWaterLevel::Feet;

		GetWaterCheckPosition(ESrcWaterLevel::Waist, point);
		cont = world->GetPointContents(point, SRC_MASK_WATER);

		if (cont & SRC_MASK_WATER)
		{
			player->WaterLevel = ESrcWaterLevel::Waist;

			GetWaterCheckPosition(ESrcWaterLevel::Eyes, point);
			cont = world->GetPointContents(point, SRC_MASK_WATER);

			if (cont & SRC_MASK_WATER)
			{
				player->WaterLevel = ESrcWaterLevel::Eyes;
			}
		}
	}

	if (m_nOldWaterLevel == ESrcWaterLevel::NotInWater && player->WaterLevel > ESrcWaterLevel::NotInWater)
	{
		m_flWaterEntryTime = curtime;
	}

	return player->WaterLevel > ESrcWaterLevel::Feet;
}

void FSourceMovementSim::WaterJump()
{
	if (player->m_flWaterJumpTime > FSourceMovementParams::WaterJumpMaxMsecs)
	{
		player->m_flWaterJumpTime = FSourceMovementParams::WaterJumpMaxMsecs;
	}

	if (!player->m_flWaterJumpTime)
	{
		return;
	}

	player->m_flWaterJumpTime -= 1000.0f * frametime;

	if (player->m_flWaterJumpTime <= 0 || player->WaterLevel == ESrcWaterLevel::NotInWater)
	{
		player->m_flWaterJumpTime = 0;
		player->RemoveFlag(SRC_FL_WATERJUMP);
	}

	// Latched for the whole hop, so you cannot steer out of it once it starts
	mv->Velocity[0] = player->m_vecWaterJumpVel[0];
	mv->Velocity[1] = player->m_vecWaterJumpVel[1];
}

void FSourceMovementSim::CheckWaterJump()
{
	if (player->m_flWaterJumpTime)
	{
		return;
	}

	// Only on the way up, or falling in would throw you straight back out
	if (mv->Velocity[2] < FSourceMovementParams::WaterJumpEntryVelocityZ)
	{
		return;
	}

	FSrcVec3 flatvelocity(mv->Velocity[0], mv->Velocity[1], 0.f);
	const srcfloat curspeed = VectorNormalize(flatvelocity);

	FSrcVec3 flatforward(m_vecForward[0], m_vecForward[1], 0.f);
	VectorNormalize(flatforward);

	// Backing into water off a step should not pop you forward
	if (curspeed != 0.0f && DotProduct(flatvelocity, flatforward) < 0.0f)
	{
		return;
	}

	FSrcVec3 vecStart = mv->GetAbsOrigin() + (GetPlayerMins() + GetPlayerMaxs()) * 0.5f;

	FSrcVec3 vecEnd;
	VectorMA(vecStart, FSourceMovementParams::WaterJumpProbeDistance, flatforward, vecEnd);

	FSourceTraceResult tr;
	TracePlayerBBox(vecStart, vecEnd, PlayerSolidMask(), tr);

	if (tr.Fraction < 1.0f)
	{
		// Not ported: the original also refuses to climb a physics prop the player is carrying

		vecStart.z = mv->GetAbsOrigin().z + player->ViewOffset.z + FSourceMovementParams::WaterJumpHeight;
		VectorMA(vecStart, FSourceMovementParams::WaterJumpProbeDistance, flatforward, vecEnd);
		VectorMA(FSrcVec3::Zero, FSourceMovementParams::WaterJumpOutVelocity, tr.PlaneNormal, player->m_vecWaterJumpVel);

		TracePlayerBBox(vecStart, vecEnd, PlayerSolidMask(), tr);

		if (tr.Fraction == 1.0f)
		{
			// Clear at eye height: check there is something standable to land on
			VectorCopy(vecEnd, vecStart);
			vecEnd.z -= FSourceMovementParams::WaterJumpLandProbeDepth;
			TracePlayerBBox(vecStart, vecEnd, PlayerSolidMask(), tr);

			if (tr.Fraction < 1.0f && tr.PlaneNormal.z >= FSourceMovementParams::StandableGroundNormalZ)
			{
				mv->Velocity[2] = FSourceMovementParams::WaterJumpPushUp;
				mv->OldButtons |= SRC_IN_JUMP;
				player->AddFlag(SRC_FL_WATERJUMP);
				player->m_flWaterJumpTime = FSourceMovementParams::WaterJumpDurationMsecs;
			}
		}
	}
}

void FSourceMovementSim::WaterMove()
{
	FSrcVec3 wishvel;
	FSrcVec3 forward, right, up;

	AngleVectors(mv->ViewAngles, &forward, &right, &up);

	for (int32 i = 0; i < 3; i++)
	{
		wishvel[i] = forward[i] * mv->ForwardMove + right[i] * mv->SideMove;
	}

	if (mv->Buttons & SRC_IN_JUMP)
	{
		wishvel[2] += mv->ClientMaxSpeed;
	}
	else if (!mv->ForwardMove && !mv->SideMove && !mv->UpMove)
	{
		wishvel[2] -= FSourceMovementParams::WaterSinkSpeed;
	}
	else
	{
		// Looking up while swimming forward also carries you up
		srcfloat upwardMovement = mv->ForwardMove * forward.z * 2;
		upwardMovement = SrcClamp(upwardMovement, 0.f, mv->ClientMaxSpeed);
		wishvel[2] += mv->UpMove + upwardMovement;
	}

	FSrcVec3 wishdir;
	VectorCopy(wishvel, wishdir);
	srcfloat wishspeed = VectorNormalize(wishdir);

	if (wishspeed > mv->MaxSpeed)
	{
		VectorScale(wishvel, mv->MaxSpeed / wishspeed, wishvel);
		wishspeed = mv->MaxSpeed;
	}

	wishspeed *= FSourceMovementParams::WaterSwimSpeedScale;

	FSrcVec3 temp;
	VectorCopy(mv->Velocity, temp);
	const srcfloat speed = VectorNormalize(temp);
	srcfloat newspeed = 0.f;

	if (speed)
	{
		newspeed = speed - frametime * speed * params->Friction * player->m_surfaceFriction;
		if (newspeed < 0.1f)
		{
			newspeed = 0;
		}

		VectorScale(mv->Velocity, newspeed / speed, mv->Velocity);
	}

	if (wishspeed >= 0.1f)
	{
		const srcfloat addspeed = wishspeed - newspeed;
		if (addspeed > 0)
		{
			VectorNormalize(wishvel);

			srcfloat accelspeed = params->Accelerate * wishspeed * frametime * player->m_surfaceFriction;
			if (accelspeed > addspeed)
			{
				accelspeed = addspeed;
			}

			for (int32 i = 0; i < 3; i++)
			{
				const srcfloat deltaSpeed = accelspeed * wishvel[i];
				mv->Velocity[i] += deltaSpeed;
				mv->OutWishVel[i] += deltaSpeed;
			}
		}
	}

	VectorAdd(mv->Velocity, player->BaseVelocity, mv->Velocity);

	FSrcVec3 dest;
	VectorMA(mv->GetAbsOrigin(), frametime, mv->Velocity, dest);

	FSourceTraceResult pm;
	TracePlayerBBox(mv->GetAbsOrigin(), dest, PlayerSolidMask(), pm);

	if (pm.Fraction == 1.0f)
	{
		// Assume it was a stair or a slope, so press down from stepheight above
		FSrcVec3 start;
		VectorCopy(dest, start);
		if (params->bAllowAutoMovement)
		{
			start[2] += params->StepSize + 1;
		}

		TracePlayerBBox(start, dest, PlayerSolidMask(), pm);

		if (!pm.bStartSolid && !pm.bAllSolid)
		{
			const srcfloat stepDist = pm.EndPos.z - mv->GetAbsOrigin().z;
			mv->OutStepHeight += stepDist;
			mv->SetAbsOrigin(pm.EndPos);
			VectorSubtract(mv->Velocity, player->BaseVelocity, mv->Velocity);
			return;
		}

		TryPlayerMove();
	}
	else
	{
		if (!player->GroundEntity.IsValid())
		{
			TryPlayerMove();
			VectorSubtract(mv->Velocity, player->BaseVelocity, mv->Velocity);
			return;
		}

		StepMove(dest, pm);
	}

	VectorSubtract(mv->Velocity, player->BaseVelocity, mv->Velocity);
}
