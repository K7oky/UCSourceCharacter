// Collision response and ground detection

#include "Core/SourceMovementSim.h"

using namespace SourceMath;

int32 FSourceMovementSim::ClipVelocity(const FSrcVec3& in, const FSrcVec3& normal, FSrcVec3& out, srcfloat overbounce)
{
	const srcfloat angle = normal[2];
	int32 blocked = 0x00;

	// These thresholds are not the 0.7 / 0.0001 pair TryPlayerMove uses for its own flags
	if (angle > 0)
	{
		blocked |= 0x01;  // floor
	}
	if (!angle)
	{
		blocked |= 0x02;  // vertical wall / step
	}

	const srcfloat backoff = DotProduct(in, normal) * overbounce;

	for (int32 i = 0; i < 3; i++)
	{
		const srcfloat change = normal[i] * backoff;
		out[i] = in[i] - change;
	}

	srcfloat adjust = DotProduct(out, normal);
	if (adjust < 0.0f)
	{
		adjust = SrcMin(adjust, -FSourceMovementParams::DistEpsilon);
		out -= (normal * adjust);
	}

	return blocked;
}

int32 FSourceMovementSim::TryPlayerMove(const FSrcVec3* pFirstDest, const FSourceTraceResult* pFirstTrace)
{
	// The core slide solver: up to four bumps, clipping velocity against every plane it hits
	int32 bumpcount, numbumps;
	FSrcVec3 dir;
	srcfloat d = 0.f;
	int32 numplanes;
	FSrcVec3 planes[FSourceMovementParams::MaxClipPlanes];
	FSrcVec3 primal_velocity, original_velocity;
	FSrcVec3 new_velocity;
	int32 i = 0, j = 0;
	FSourceTraceResult pm;
	FSrcVec3 end;
	srcfloat time_left, allFraction;
	int32 blocked;

	numbumps = FSourceMovementParams::NumBumps;  // 4

	blocked = 0;
	numplanes = 0;

	VectorCopy(mv->Velocity, original_velocity);
	VectorCopy(mv->Velocity, primal_velocity);
	allFraction = 0;
	time_left = frametime;

	// Zero here, and the airborne single-plane branch can copy it to velocity
	new_velocity.Init();

	for (bumpcount = 0; bumpcount < numbumps; bumpcount++)
	{
		if (mv->Velocity.Length() == 0.0)
		{
			break;
		}

		VectorMA(mv->GetAbsOrigin(), time_left, mv->Velocity, end);

		if (params->bMovementOptimizations)
		{
			// The reuse gate is an exact float comparison of the destination vectors
			if (pFirstDest && (end == *pFirstDest))
			{
				pm = *pFirstTrace;
			}
			else
			{
				TracePlayerBBox(mv->GetAbsOrigin(), end, PlayerSolidMask(), pm);
			}
		}
		else
		{
			TracePlayerBBox(mv->GetAbsOrigin(), end, PlayerSolidMask(), pm);
		}

		// "extremely tiny move fractions cause problems in later computations"
		if (pm.Fraction > 0 && pm.Fraction < FSourceMovementParams::MinimumMoveFraction)
		{
			pm.Fraction = 0;
		}

		allFraction += pm.Fraction;

		if (pm.bAllSolid)
		{
			VectorCopy(FSrcVec3::Zero, mv->Velocity);
			return 4;
		}

		if (pm.Fraction > 0)
		{
			// Terrain tracing can report a clean full sweep whose end position is inside a triangle
			if (numbumps > 0 && pm.Fraction == 1)
			{
				FSourceTraceResult stuck;
				TracePlayerBBox(pm.EndPos, pm.EndPos, PlayerSolidMask(), stuck);
				if (stuck.bStartSolid || stuck.Fraction != 1.0f)
				{
					VectorCopy(FSrcVec3::Zero, mv->Velocity);
					break;
				}
			}

			mv->SetAbsOrigin(pm.EndPos);
			VectorCopy(mv->Velocity, original_velocity);
			numplanes = 0;
		}

		if (pm.Fraction == 1)
		{
			break;  // covered the whole distance
		}

		// Not ported: the touch list that notifies entities the player brushed past

		if (pm.PlaneNormal[2] > FSourceMovementParams::StandableGroundNormalZ)
		{
			blocked |= 1;  // floor
		}

		// A near-horizontal normal is snapped to exactly zero, and the snapped value is stored
		if (FMath::Abs(pm.PlaneNormal[2]) < FSourceMovementParams::EffectivelyHorizontalNormalZ)
		{
			pm.PlaneNormal[2] = 0;
			blocked |= 2;  // step / wall
		}

		time_left -= time_left * pm.Fraction;

		if (numplanes >= FSourceMovementParams::MaxClipPlanes)
		{
			// "this shouldn't really happen" - stop dead
			VectorCopy(FSrcVec3::Zero, mv->Velocity);
			break;
		}

		VectorCopy(pm.PlaneNormal, planes[numplanes]);
		numplanes++;

		if (numplanes == 1 && player->MoveType == ESrcMoveType::Walk && !player->GroundEntity.IsValid())
		{
			for (i = 0; i < numplanes; i++)
			{
				if (planes[i][2] > FSourceMovementParams::StandableGroundNormalZ)
				{
					ClipVelocity(original_velocity, planes[i], new_velocity, 1);
					VectorCopy(new_velocity, original_velocity);
				}
				else
				{
					ClipVelocity(original_velocity, planes[i], new_velocity, 1.0f + params->Bounce * (1 - player->m_surfaceFriction));
				}
			}

			VectorCopy(new_velocity, mv->Velocity);
			VectorCopy(new_velocity, original_velocity);
		}
		else
		{
			for (i = 0; i < numplanes; i++)
			{
				ClipVelocity(original_velocity, planes[i], mv->Velocity, 1);

				for (j = 0; j < numplanes; j++)
				{
					if (j != i)
					{
						// Are we now moving against this plane?
						if (mv->Velocity.Dot(planes[j]) < 0)
						{
							break;  // not ok
						}
					}
				}

				if (j == numplanes)  // Didn't have to clip, so we're ok
				{
					break;
				}
			}

			if (i != numplanes)
			{
				// Go along this plane - mv->Velocity was already set by ClipVelocity
				;
			}
			else
			{
				// Crease movement is only defined for EXACTLY two planes
				if (numplanes != 2)
				{
					VectorCopy(FSrcVec3::Zero, mv->Velocity);
					break;
				}
				CrossProduct(planes[0], planes[1], dir);
				dir.NormalizeInPlace();
				d = dir.Dot(mv->Velocity);
				VectorScale(dir, d, mv->Velocity);
			}

			d = mv->Velocity.Dot(primal_velocity);
			if (d <= 0)
			{
				VectorCopy(FSrcVec3::Zero, mv->Velocity);
				break;
			}
		}
	}

	// No progress across every bump discards the velocity entirely
	if (allFraction == 0)
	{
		VectorCopy(FSrcVec3::Zero, mv->Velocity);
	}

	// Wall-slam feedback
	srcfloat fSlamVol = 0.0f;
	const srcfloat fLateralStoppingAmount = primal_velocity.Length2D() - mv->Velocity.Length2D();
	if (fLateralStoppingAmount > FSourceMovementParams::PlayerMaxSafeFallSpeed * 2.0f)
	{
		fSlamVol = 1.0f;
	}
	else if (fLateralStoppingAmount > FSourceMovementParams::PlayerMaxSafeFallSpeed)
	{
		fSlamVol = 0.85f;
	}

	if (fSlamVol > 0.0f)
	{
		PlayerRoughLandingEffects(fSlamVol);
	}

	Telemetry.CollisionPlanes = numplanes;
	Telemetry.Blocked = blocked;
	return blocked;
}

void FSourceMovementSim::StepMove(FSrcVec3& vecDestination, FSourceTraceResult& trace)
{
	FSrcVec3 vecPos, vecVel;
	VectorCopy(mv->GetAbsOrigin(), vecPos);
	VectorCopy(mv->Velocity, vecVel);

	// Attempt 1: straight there
	FSrcVec3 vecEndPos;
	VectorCopy(vecDestination, vecEndPos);
	TryPlayerMove(&vecEndPos, &trace);
	FSrcVec3 vecDownPos, vecDownVel;
	VectorCopy(mv->GetAbsOrigin(), vecDownPos);
	VectorCopy(mv->Velocity, vecDownVel);

	// Rewind
	mv->SetAbsOrigin(vecPos);
	VectorCopy(vecVel, mv->Velocity);

	// Attempt 2: up, forward, down
	VectorCopy(mv->GetAbsOrigin(), vecEndPos);
	if (params->bAllowAutoMovement)
	{
		vecEndPos.z += params->StepSize + FSourceMovementParams::DistEpsilon;
	}

	TracePlayerBBox(mv->GetAbsOrigin(), vecEndPos, PlayerSolidMask(), trace);

	// Start or end solid leaves the origin alone, but the move still runs un-raised
	if (!trace.bStartSolid && !trace.bAllSolid)
	{
		mv->SetAbsOrigin(trace.EndPos);
	}
	TryPlayerMove();

	VectorCopy(mv->GetAbsOrigin(), vecEndPos);
	if (params->bAllowAutoMovement)
	{
		vecEndPos.z -= params->StepSize + FSourceMovementParams::DistEpsilon;
	}

	TracePlayerBBox(mv->GetAbsOrigin(), vecEndPos, PlayerSolidMask(), trace);

	// A downward trace that hit nothing leaves the normal zero, so z < 0.7 and this branch runs
	if (trace.PlaneNormal[2] < FSourceMovementParams::StandableGroundNormalZ)
	{
		mv->SetAbsOrigin(vecDownPos);
		VectorCopy(vecDownVel, mv->Velocity);
		const srcfloat flStepDist = mv->GetAbsOrigin().z - vecPos.z;
		if (flStepDist > 0.0f)
		{
			mv->OutStepHeight += flStepDist;
		}
		return;
	}

	if (!trace.bStartSolid && !trace.bAllSolid)
	{
		mv->SetAbsOrigin(trace.EndPos);
	}

	FSrcVec3 vecUpPos;
	VectorCopy(mv->GetAbsOrigin(), vecUpPos);

	// Purely horizontal comparison, so a step-up that gained height but less ground is rejected
	const srcfloat flDownDist = (vecDownPos.x - vecPos.x) * (vecDownPos.x - vecPos.x)
			+ (vecDownPos.y - vecPos.y) * (vecDownPos.y - vecPos.y);
	const srcfloat flUpDist = (vecUpPos.x - vecPos.x) * (vecUpPos.x - vecPos.x)
			+ (vecUpPos.y - vecPos.y) * (vecUpPos.y - vecPos.y);

	if (flDownDist > flUpDist)
	{
		mv->SetAbsOrigin(vecDownPos);
		VectorCopy(vecDownVel, mv->Velocity);
	}
	else
	{
		// When the step-up wins, only velocity.z comes from the down attempt
		mv->Velocity.z = vecDownVel.z;
	}

	const srcfloat flStepDist = mv->GetAbsOrigin().z - vecPos.z;
	if (flStepDist > 0)
	{
		mv->OutStepHeight += flStepDist;
	}
}

void FSourceMovementSim::StayOnGround()
{
	FSourceTraceResult trace;
	FSrcVec3 start(mv->GetAbsOrigin());
	FSrcVec3 end(mv->GetAbsOrigin());
	start.z += 2;
	end.z -= params->StepSize;

	// See how far up we can go without getting stuck
	TracePlayerBBox(mv->GetAbsOrigin(), start, PlayerSolidMask(), trace);
	start = trace.EndPos;

	// Then drop from there to one step below, and snap down if the floor is close enough
	TracePlayerBBox(start, end, PlayerSolidMask(), trace);
	// Must go somewhere, hit something, not be embedded, and not be too steep
	if (trace.Fraction > 0.0f && trace.Fraction < 1.0f && !trace.bStartSolid && trace.PlaneNormal[2] >= FSourceMovementParams::StandableGroundNormalZ)
	{
		const srcfloat flDelta = FMath::Abs(mv->GetAbsOrigin().z - trace.EndPos.z);

		// The original calls this "incredibly hacky"
		if (flDelta > 0.5f * FSourceMovementParams::CoordResolution)
		{
			mv->SetAbsOrigin(trace.EndPos);
		}
	}
}

// Ground detection

bool FSourceMovementSim::CheckValidStandableGroundCandidate(const FSourceTraceResult& pm, srcfloat flStandableZ) const
{
	if (!pm.DidHit() || !pm.Entity.IsValid())
	{
		return false;
	}

	// Players are standable at ANY plane normal
	if (pm.Entity.bIsPlayer)
	{
		return true;
	}

	return (pm.PlaneNormal[2] >= flStandableZ);
}

void FSourceMovementSim::TracePlayerBBoxForGround(const FSrcVec3& start, const FSrcVec3& end, const FSrcVec3& minsSrc, const FSrcVec3& maxsSrc, int32 fMask, FSourceTraceResult& pm, srcfloat minGroundNormalZ, bool overwriteEndpos)
{
	// Four sweeps, one per quadrant of the hull, so a foot hanging off an edge still finds ground
	FSrcVec3 mins, maxs;
	const FSourceTraceFilter Filter = MakeTraceFilter();

	// Fraction and endpos are captured here and restored on every exit path
	const srcfloat fraction = pm.Fraction;
	const FSrcVec3 endpos = pm.EndPos;

	// Quadrant -x, -y
	mins = minsSrc;
	maxs.Init(SrcMin(0.f, maxsSrc.x), SrcMin(0.f, maxsSrc.y), maxsSrc.z);
	++m_nTraceCount;
	world->TraceHull(start, end, mins, maxs, fMask, Filter, pm);
	if (pm.Entity.IsValid() && pm.PlaneNormal[2] >= minGroundNormalZ)
	{
		if (overwriteEndpos)
		{
			pm.Fraction = fraction;
			pm.EndPos = endpos;
		}
		return;
	}

	// Quadrant +x, +y
	mins.Init(SrcMax(0.f, minsSrc.x), SrcMax(0.f, minsSrc.y), minsSrc.z);
	maxs = maxsSrc;
	++m_nTraceCount;
	world->TraceHull(start, end, mins, maxs, fMask, Filter, pm);
	if (pm.Entity.IsValid() && pm.PlaneNormal[2] >= minGroundNormalZ)
	{
		if (overwriteEndpos)
		{
			pm.Fraction = fraction;
			pm.EndPos = endpos;
		}
		return;
	}

	// Quadrant -x, +y
	mins.Init(minsSrc.x, SrcMax(0.f, minsSrc.y), minsSrc.z);
	maxs.Init(SrcMin(0.f, maxsSrc.x), maxsSrc.y, maxsSrc.z);
	++m_nTraceCount;
	world->TraceHull(start, end, mins, maxs, fMask, Filter, pm);

	// This quadrant alone compares against the literal 0.7, not the minGroundNormalZ parameter
	if (pm.Entity.IsValid() && pm.PlaneNormal[2] >= 0.7f)
	{
		if (overwriteEndpos)
		{
			pm.Fraction = fraction;
			pm.EndPos = endpos;
		}
		return;
	}

	// Quadrant +x, -y
	mins.Init(SrcMax(0.f, minsSrc.x), minsSrc.y, minsSrc.z);
	maxs.Init(maxsSrc.x, SrcMin(0.f, maxsSrc.y), maxsSrc.z);
	++m_nTraceCount;
	world->TraceHull(start, end, mins, maxs, fMask, Filter, pm);
	if (pm.Entity.IsValid() && pm.PlaneNormal[2] >= minGroundNormalZ)
	{
		if (overwriteEndpos)
		{
			pm.Fraction = fraction;
			pm.EndPos = endpos;
		}
		return;
	}

	if (overwriteEndpos)
	{
		pm.Fraction = fraction;
		pm.EndPos = endpos;
	}
}

void FSourceMovementSim::CategorizeGroundSurface(const FSourceTraceResult& pm)
{
	player->m_surfaceProps = pm.SurfacePropsId;
	player->m_surfaceFriction = world->GetSurfaceFriction(pm.SurfacePropsId);

	// Magic number, scales physics-material friction into player friction
	player->m_surfaceFriction *= params->SurfaceFrictionScale;
	if (player->m_surfaceFriction > 1.0f)
	{
		player->m_surfaceFriction = 1.0f;
	}

	player->m_flSurfaceJumpFactor = world->GetSurfaceJumpFactor(pm.SurfacePropsId);
	player->m_flSurfaceMaxSpeedFactor = world->GetSurfaceMaxSpeedFactor(pm.SurfacePropsId);
}

void FSourceMovementSim::SetGroundEntity(const FSourceTraceResult* pm)
{
	const FSrcEntityRef newGround = pm ? pm->Entity : FSrcEntityRef::None();
	const FSrcEntityRef oldGround = player->GroundEntity;
	FSrcVec3 vecBaseVelocity = player->BaseVelocity;

	if (!oldGround.IsValid() && newGround.IsValid())
	{
		const FSrcVec3 NewGroundVel = world->GetEntityAbsVelocity(newGround);
		vecBaseVelocity -= NewGroundVel;
		vecBaseVelocity.z = NewGroundVel.z;
	}
	else if (oldGround.IsValid() && !newGround.IsValid())
	{
		const FSrcVec3 OldGroundVel = world->GetEntityAbsVelocity(oldGround);
		vecBaseVelocity += OldGroundVel;
		vecBaseVelocity.z = OldGroundVel.z;
	}

	player->BaseVelocity = vecBaseVelocity;
	player->GroundEntity = newGround;

	// CheckParameters and the duck speed crop read FL_ONGROUND, so keep it in sync
	if (newGround.IsValid())
	{
		player->AddFlag(SRC_FL_ONGROUND);
	}
	else
	{
		player->RemoveFlag(SRC_FL_ONGROUND);
	}

	if (newGround.IsValid())
	{
		CategorizeGroundSurface(*pm);
		player->m_flWaterJumpTime = 0;

		// Landing kills downward velocity here, inside CategorizePosition, before CheckFalling
		if (player->MoveType != ESrcMoveType::NoClip)
		{
			mv->Velocity.z = 0.0f;
		}
	}
}

void FSourceMovementSim::CategorizePosition()
{
	FSrcVec3 point;
	FSourceTraceResult pm;

	// Reset so we don't carry bogus friction into a jump-into-water transition
	player->m_surfaceFriction = 1.0f;

	CheckWater();

	// Observers have no ground entity
	if (player->MoveType == ESrcMoveType::Observer)
	{
		return;
	}

	const srcfloat flOffset = FSourceMovementParams::GroundTraceOffset;  // 2.0f

	point[0] = mv->GetAbsOrigin()[0];
	point[1] = mv->GetAbsOrigin()[1];
	point[2] = mv->GetAbsOrigin()[2] - flOffset;
	FSrcVec3 bumpOrigin = mv->GetAbsOrigin();
	const srcfloat zvel = mv->Velocity[2];
	const bool bMovingUp = zvel > 0.0f;
	bool bMovingUpRapidly = zvel > FSourceMovementParams::NonJumpVelocity;  // 140
	srcfloat flGroundEntityVelZ = 0.0f;
	if (bMovingUpRapidly)
	{
		// Standing on a rising lift must not read as "jumping"
		if (player->GroundEntity.IsValid())
		{
			flGroundEntityVelZ = world->GetEntityAbsVelocity(player->GroundEntity).z;
			bMovingUpRapidly = (zvel - flGroundEntityVelZ) > FSourceMovementParams::NonJumpVelocity;
		}
	}

	const bool bUnderwater = (player->WaterLevel >= ESrcWaterLevel::Eyes);
	bool bMoveToEndPos = false;
	if (player->MoveType == ESrcMoveType::Walk && player->GroundEntity.IsValid() && !bUnderwater)
	{
		bMoveToEndPos = true;
		point.z -= params->StepSize;
	}

	if (bMovingUpRapidly || (bMovingUp && player->MoveType == ESrcMoveType::Ladder))
	{
		SetGroundEntity(nullptr);
		bMoveToEndPos = false;
	}
	else
	{
		TracePlayerBBox(bumpOrigin, point, PlayerSolidMask(), pm);

		const srcfloat flStandableZ = FSourceMovementParams::StandableGroundNormalZ;  // 0.7

		if (!CheckValidStandableGroundCandidate(pm, flStandableZ))
		{
			// Test four sub-boxes, in case one of them finds a shallower slope we could stand on
			TracePlayerBBoxForGround(bumpOrigin, point, GetPlayerMins(), GetPlayerMaxs(), PlayerSolidMask(), pm, flStandableZ, true);

			if (!CheckValidStandableGroundCandidate(pm, flStandableZ))
			{
				SetGroundEntity(nullptr);

				// Airborne AND rising -> quarter surface friction
				if ((mv->Velocity.z > 0.0f) && (player->MoveType != ESrcMoveType::NoClip))
				{
					player->m_surfaceFriction = params->AirborneRisingSurfaceFriction;
				}
				bMoveToEndPos = false;
			}
			else
			{
				SetGroundEntity(&pm);
			}
		}
		else
		{
			SetGroundEntity(&pm);
		}
	}

	// "This logic block essentially lifted from StayOnGround"
	if (bMoveToEndPos && !pm.bStartSolid && pm.Fraction > 0.0f && pm.Fraction < 1.0f)
	{
		mv->SetAbsOrigin(pm.EndPos);
	}

	LastGroundNormal = pm.PlaneNormal;
}
