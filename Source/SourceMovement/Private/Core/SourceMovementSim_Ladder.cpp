#include "Core/SourceMovementSim.h"

using namespace SourceMath;

bool FSourceMovementSim::OnLadder(const FSourceTraceResult& trace) const
{
	// CS refuses a perfectly flat face, otherwise you would climb the floor
	if (trace.PlaneNormal[2] == 1.0f)
	{
		return false;
	}

	if (trace.Contents & SRC_CONTENTS_LADDER)
	{
		return true;
	}

	return world->IsSurfaceClimbable(trace.SurfacePropsId);
}

srcfloat FSourceMovementSim::ClimbSpeed() const
{
	if ((mv->Buttons & SRC_IN_DUCK) || (mv->Buttons & SRC_IN_SPEED))
	{
		return FSourceMovementParams::MaxClimbSpeed * params->PlayerSpeedClimbModifier;
	}
	return FSourceMovementParams::MaxClimbSpeed;
}

srcfloat FSourceMovementSim::LadderDistance() const
{
	// Once attached the reach grows, so a ladder is harder to fall off than to grab
	return (player->MoveType == ESrcMoveType::Ladder) ? FSourceMovementParams::LadderReachOn : FSourceMovementParams::LadderReachOff;
}

int32 FSourceMovementSim::LadderMask() const
{
	return params->PlayerSolidMask & ~SRC_CONTENTS_PLAYERCLIP;
}

bool FSourceMovementSim::LadderMove()
{
	if (player->MoveType == ESrcMoveType::NoClip)
	{
		return false;
	}

	FSrcVec3 wishdir;

	if (player->MoveType == ESrcMoveType::Ladder)
	{
		// Already climbing: keep probing the face we are on rather than where we look
		wishdir = -player->m_vecLadderNormal;
	}
	else if (mv->ForwardMove || mv->SideMove)
	{
		for (int32 i = 0; i < 3; i++)
		{
			wishdir[i] = m_vecForward[i] * mv->ForwardMove + m_vecRight[i] * mv->SideMove;
		}
		VectorNormalize(wishdir);
	}
	else
	{
		// Standing still never grabs a ladder
		return false;
	}

	FSourceTraceResult pm;
	FSrcVec3 end;
	VectorMA(mv->GetAbsOrigin(), LadderDistance(), wishdir, end);
	TracePlayerBBox(mv->GetAbsOrigin(), end, LadderMask(), pm);

	if (pm.Fraction == 1.0f || !OnLadder(pm))
	{
		bool bCatchCliffLadder = false;

		// Walking off a ledge with a ladder behind you grabs it on the way down
		if (player->m_bHasWalkMovedSinceLastJump && player->MoveType != ESrcMoveType::Ladder && !player->GroundEntity.IsValid() && mv->Velocity.z <= 0 && mv->Velocity.z > -50.0f && FMath::Abs(mv->Velocity.x) > 0 && FMath::Abs(mv->Velocity.y) > 0)
		{
			FSrcVec3 vecTraceFrom = mv->GetAbsOrigin();
			vecTraceFrom.z -= FSourceMovementParams::LadderEdgeGrabHeightDelta;

			FSrcVec3 vecNormalizedVel = mv->Velocity;
			VectorNormalize(vecNormalizedVel);

			FSrcVec3 vecTraceTo;
			VectorMA(mv->GetAbsOrigin(), -FSourceMovementParams::LadderEdgeGrabDistance, vecNormalizedVel, vecTraceTo);

			TracePlayerBBox(vecTraceFrom, vecTraceTo, PlayerSolidMask() & ~SRC_CONTENTS_PLAYERCLIP, pm);

			if (pm.Fraction != 1.0f && OnLadder(pm) && pm.PlaneNormal[2] != 1.0f)
			{
				player->MoveType = ESrcMoveType::Ladder;
				player->m_vecLadderNormal = pm.PlaneNormal;
				mv->Velocity.Init();

				// The grab ignored playerclip so a clip brush flush with the ladder cannot block
				// it; re-trace with the clip mask before snapping, or we warp into the brush
				TracePlayerBBox(vecTraceFrom, vecTraceTo, PlayerSolidMask(), pm);
				mv->SetAbsOrigin(pm.EndPos);

				bCatchCliffLadder = true;
			}
		}

		if (!bCatchCliffLadder)
		{
			return false;
		}
	}

	if (player->MoveType != ESrcMoveType::Ladder)
	{
		// Swallow the jump button for a moment, or the press that carried you in bounces you off
		player->m_ignoreLadderJumpTime = curtime + FSourceMovementParams::LadderIgnoreJumpTime;
	}

	player->MoveType = ESrcMoveType::Ladder;
	player->m_vecLadderNormal = pm.PlaneNormal;

	// Step sounds want the ladder's material, not the ground we last stood on
	player->m_surfaceProps = pm.SurfacePropsId;

	FSrcVec3 floor = mv->GetAbsOrigin();
	floor[2] += GetPlayerMins()[2] - 1;

	const bool onFloor = (world->GetPointContents(floor, params->PlayerSolidMask) & SRC_CONTENTS_SOLID) || player->GroundEntity.IsValid();

	// No effect: StartGravity tests the VALUE, and 0 falls through to 1.0
	player->m_flGravityScale = 0.f;

	const srcfloat climbSpeed = ClimbSpeed();

	srcfloat forwardSpeed = 0.f;
	srcfloat rightSpeed = 0.f;

	if (mv->Buttons & SRC_IN_BACK) { forwardSpeed -= climbSpeed; }
	if (mv->Buttons & SRC_IN_FORWARD) { forwardSpeed += climbSpeed; }
	if (mv->Buttons & SRC_IN_MOVELEFT) { rightSpeed -= climbSpeed; }
	if (mv->Buttons & SRC_IN_MOVERIGHT) { rightSpeed += climbSpeed; }

	if (mv->Buttons & SRC_IN_JUMP)
	{
		if (player->m_ignoreLadderJumpTime <= curtime)
		{
			player->MoveType = ESrcMoveType::Walk;
			VectorScale(pm.PlaneNormal, FSourceMovementParams::LadderJumpOffSpeed, mv->Velocity);
		}
	}
	else if (forwardSpeed != 0.f || rightSpeed != 0.f)
	{
		FSrcVec3 velocity, perp, cross, lateral, tmp;

		VectorScale(m_vecForward, forwardSpeed, velocity);
		VectorMA(velocity, rightSpeed, m_vecRight, velocity);

		// Perpendicular to the ladder, in the horizontal plane
		tmp.Init(0.f, 0.f, 1.f);
		CrossProduct(tmp, pm.PlaneNormal, perp);
		VectorNormalize(perp);

		// Split the intended move into the part going into the ladder face and the rest
		const srcfloat normal = DotProduct(velocity, pm.PlaneNormal);
		VectorScale(pm.PlaneNormal, normal, cross);
		VectorSubtract(velocity, cross, lateral);

		// Pushing into the face becomes movement up it, and both parts are summed, so you can
		// look up and travel down
		CrossProduct(pm.PlaneNormal, perp, tmp);

		const srcfloat tmpDist = DotProduct(tmp, lateral);
		const srcfloat perpDist = DotProduct(perp, lateral);

		FSrcVec3 angleVec = perp * perpDist;
		angleVec += cross;
		VectorNormalize(angleVec);

		const srcfloat angleDot = DotProduct(angleVec, pm.PlaneNormal);
		if (angleDot < params->LadderAngle)
		{
			lateral = (tmp * tmpDist) + (perp * params->LadderDampen * perpDist);
		}

		VectorMA(lateral, -normal, tmp, mv->Velocity);

		if (params->LadderScaleSpeed > 0.f)
		{
			VectorScale(mv->Velocity, params->LadderScaleSpeed, mv->Velocity);
		}

		if (onFloor && normal > 0)
		{
			// Standing at the foot of the ladder and pushing away from it: step back off
			VectorMA(mv->Velocity, FSourceMovementParams::MaxClimbSpeed, pm.PlaneNormal, mv->Velocity);
		}
	}
	else
	{
		mv->Velocity.Init();
	}

	return true;
}

void FSourceMovementSim::FullLadderMove()
{
	CheckWater();

	if (mv->Buttons & SRC_IN_JUMP)
	{
		CheckJumpButton();
	}
	else
	{
		mv->OldButtons &= ~SRC_IN_JUMP;
	}

	VectorAdd(mv->Velocity, player->BaseVelocity, mv->Velocity);
	TryPlayerMove();
	VectorSubtract(mv->Velocity, player->BaseVelocity, mv->Velocity);
}
