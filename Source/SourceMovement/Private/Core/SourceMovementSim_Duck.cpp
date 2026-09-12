// Duck / unduck

#include "Core/SourceMovementSim.h"

using namespace SourceMath;

void FSourceMovementSim::TraceHullExplicit(const FSrcVec3& start, const FSrcVec3& end, const FSrcVec3& mins, const FSrcVec3& maxs, int32 fMask, FSourceTraceResult& pm)
{
	++m_nTraceCount;
	world->TraceHull(start, end, mins, maxs, fMask, MakeTraceFilter(), pm);
}

void FSourceMovementSim::SetDuckedEyeOffset(srcfloat duckFraction)
{
	duckFraction = SimpleSpline(duckFraction);
	const FSrcVec3 vDuckHullMin = params->Hulls.GetPlayerMins(true);
	const FSrcVec3 vStandHullMin = params->Hulls.GetPlayerMins(false);

	// `fMore` compensates for the hull MIN moving between the two stances
	const srcfloat fMore = (vDuckHullMin.z - vStandHullMin.z);
	const FSrcVec3 vecDuckViewOffset = params->Hulls.GetPlayerViewOffset(true);
	const FSrcVec3 vecStandViewOffset = params->Hulls.GetPlayerViewOffset(false);
	FSrcVec3 temp = player->ViewOffset;
	temp.z = ((vecDuckViewOffset.z - fMore) * duckFraction) + (vecStandViewOffset.z * (1 - duckFraction));
	player->ViewOffset = temp;
}

void FSourceMovementSim::FixPlayerCrouchStuck(bool upward)
{
	FSourceTraceResult dummy;

	// 1 or 0, so "downward" actually means "do not move", exactly as in the original
	const int32 direction = upward ? 1 : 0;

	if (!TestPlayerPosition(mv->GetAbsOrigin(), dummy))
	{
		return;
	}

	FSrcVec3 test;
	VectorCopy(mv->GetAbsOrigin(), test);

	for (int32 i = 0; i < 36; i++)
	{
		FSrcVec3 org = mv->GetAbsOrigin();
		org.z += direction;
		mv->SetAbsOrigin(org);

		if (!TestPlayerPosition(mv->GetAbsOrigin(), dummy))
		{
			return;
		}
	}

	mv->SetAbsOrigin(test);  // Failed
}

bool FSourceMovementSim::CanUnduck()
{
	// Cannot stand up while planting the bomb
	if (player->m_bDuckOverride)
	{
		return false;
	}

	if (player->MoveType == ESrcMoveType::NoClip)
	{
		return true;
	}

	FSourceTraceResult trace;
	FSrcVec3 newOrigin;

	VectorCopy(mv->GetAbsOrigin(), newOrigin);

	if (player->GroundEntity.IsValid())
	{
		newOrigin += params->Hulls.GetHullMinDelta();
	}
	else
	{
		// Airborne uses HALF the height difference, not the full one
		const FSrcVec3 hullSizeNormal = params->Hulls.GetHullSizeNormal();
		const FSrcVec3 hullSizeCrouch = params->Hulls.GetHullSizeCrouch();
		newOrigin += -0.5f * (hullSizeNormal - hullSizeCrouch);
	}

	// Traces the STANDING hull explicitly, not GetPlayerMins/Maxs
	TraceHullExplicit(mv->GetAbsOrigin(), newOrigin, params->Hulls.GetPlayerMins(false), params->Hulls.GetPlayerMaxs(false), PlayerSolidMask(), trace);
	if (trace.bStartSolid || (trace.Fraction != 1.0f))
	{
		return false;
	}

	return true;
}

// Stance transitions

void FSourceMovementSim::FinishDuck()
{
	ensureMsgf(!player->m_bDucked, TEXT("FinishDuck called while already ducked. Source asserts on this."));

	FSrcVec3 newOrigin = mv->GetAbsOrigin();

	if (player->GroundEntity.IsValid() || player->MoveType == ESrcMoveType::Ladder)
	{
		// Zero in CS:GO, where both hulls share the same mins, so the origin does not move
		newOrigin -= params->Hulls.GetHullMinDelta();
	}
	else
	{
		// Airborne uses half the height difference, not the full one
		const FSrcVec3 hullSizeNormal = params->Hulls.GetHullSizeNormal();
		const FSrcVec3 hullSizeCrouch = params->Hulls.GetHullSizeCrouch();
		const FSrcVec3 viewDelta = -0.5f * (hullSizeNormal - hullSizeCrouch);
		newOrigin -= viewDelta;
	}

	mv->SetAbsOrigin(newOrigin);
	player->ViewOffset = params->Hulls.GetPlayerViewOffset(true);
	player->m_bDucking = false;
	player->m_bDucked = true;
	player->m_flLastDuckTime = curtime;

	player->AddFlag(SRC_FL_ANIMDUCKING | SRC_FL_DUCKING);

	// Only FinishDuck does this, never FinishUnDuck
	FixPlayerCrouchStuck(true);

	// Ducking changes the origin, so the ground has to be re-evaluated immediately
	CategorizePosition();
	player->m_flDuckAmount = 1.0f;
}

void FSourceMovementSim::FinishUnDuck()
{
	FSrcVec3 newOrigin = mv->GetAbsOrigin();

	if (player->GroundEntity.IsValid() || player->MoveType == ESrcMoveType::Ladder)
	{
		newOrigin += params->Hulls.GetHullMinDelta();
	}
	else
	{
		const FSrcVec3 hullSizeNormal = params->Hulls.GetHullSizeNormal();
		const FSrcVec3 hullSizeCrouch = params->Hulls.GetHullSizeCrouch();
		const FSrcVec3 viewDelta = -0.5f * (hullSizeNormal - hullSizeCrouch);
		newOrigin += viewDelta;
	}

	mv->SetAbsOrigin(newOrigin);

	player->RemoveFlag(SRC_FL_DUCKING | SRC_FL_ANIMDUCKING);
	player->m_bDucked = false;
	player->m_bDucking = false;
	player->m_nDuckTimeMsecs = 0;  // legacy

	player->ViewOffset = params->Hulls.GetPlayerViewOffset(false);

	// No FixPlayerCrouchStuck here, unlike FinishDuck
	CategorizePosition();
	player->m_flDuckAmount = 0.0f;
}

// Bot auto crouch-jump

void FSourceMovementSim::DuckUntilOnGround()
{
	// Bots hold the duck through a jump and stand up again as they come down
	const bool playerTouchingGround = player->GroundEntity.IsValid();
	const bool bInAir = !playerTouchingGround && player->MoveType != ESrcMoveType::Ladder;

	if ((player->Flags & SRC_FL_DUCKING) == 0)
	{
		player->m_duckUntilOnGround = false;
		return;
	}

	if (!bInAir)
	{
		player->m_duckUntilOnGround = false;

		if (CanUnduck())
		{
			FinishUnDuck();
		}
		return;
	}

	if (mv->Velocity.z > 0.0f)
	{
		return;
	}

	FSourceTraceResult trace;
	FSrcVec3 newOrigin;
	FSrcVec3 groundCheck;

	VectorCopy(mv->GetAbsOrigin(), newOrigin);

	// Full height delta here, half of it in CanUnduck and FinishUnDuck
	const FSrcVec3 hullSizeNormal = params->Hulls.GetHullSizeNormal();
	const FSrcVec3 hullSizeCrouch = params->Hulls.GetHullSizeCrouch();
	newOrigin -= (hullSizeNormal - hullSizeCrouch);
	groundCheck = newOrigin;
	groundCheck.z -= params->StepSize;

	TraceHullExplicit(newOrigin, groundCheck, params->Hulls.GetPlayerMins(false), params->Hulls.GetPlayerMaxs(false), PlayerSolidMask(), trace);

	// No room to unduck, or still too high off the ground
	if (trace.bStartSolid || trace.Fraction == 1.0f)
	{
		return;
	}

	player->m_duckUntilOnGround = false;

	if (CanUnduck())
	{
		FinishUnDuck();
	}
}

void FSourceMovementSim::Duck()
{
	const bool playerTouchingGround = player->GroundEntity.IsValid();

	if (mv->Buttons & SRC_IN_DUCK)
	{
		mv->OldButtons |= SRC_IN_DUCK;
	}
	else
	{
		mv->OldButtons &= ~SRC_IN_DUCK;
	}

	if (IsDead())
	{
		player->m_bDuckOverride = false;

		if (player->HasFlag(SRC_FL_DUCKING))
		{
			FinishUnDuck();
		}
		return;
	}

	if (player->m_duckUntilOnGround)
	{
		DuckUntilOnGround();
		return;
	}

	// Duck speed recovers toward the ideal over time
	player->m_flDuckSpeed = Approach(params->PlayerDuckSpeedIdeal, player->m_flDuckSpeed, frametime * 3.0f);

	if (player->m_flDuckSpeed >= params->PlayerDuckSpeedIdeal)
	{
		player->m_vecLastPositionAtFullCrouchSpeedX = mv->GetAbsOrigin().x;
		player->m_vecLastPositionAtFullCrouchSpeedY = mv->GetAbsOrigin().y;
	}
	else if (player->m_flDuckAmount <= 0 || player->m_flDuckAmount >= 1)
	{
		// Moving far enough away from where the spam happened restores the budget faster
		const srcfloat dx = mv->GetAbsOrigin().x - player->m_vecLastPositionAtFullCrouchSpeedX;
		const srcfloat dy = mv->GetAbsOrigin().y - player->m_vecLastPositionAtFullCrouchSpeedY;
		const srcfloat flDistToLastPositionAtFullCrouchSpeed = dx * dx + dy * dy;

		if (flDistToLastPositionAtFullCrouchSpeed > (64 * 64))
		{
			player->m_flDuckSpeed = Approach(params->PlayerDuckSpeedIdeal, player->m_flDuckSpeed, frametime * 6.0f);
		}
	}

	const bool duckButtonHeld = (mv->Buttons & SRC_IN_DUCK) != 0;

	// m_bDucking means "transition in progress", re-asserted every tick
	if (!duckButtonHeld && player->m_flDuckAmount > 0)
	{
		player->m_bDucking = true;
	}
	else if (duckButtonHeld && player->m_flDuckAmount < 1)
	{
		player->m_bDucking = true;
	}

	if (duckButtonHeld && player->m_bDucking)
	{
		ensureMsgf(!player->m_bDucked, TEXT("Ducking down while already fully ducked. Source asserts on this."));

		// Ducking is always slower than standing up
		srcfloat duckSpeed = player->m_flDuckSpeed * 0.8f;

		if (player->m_bIsDefusing)
		{
			duckSpeed *= 0.4f;
		}

		player->m_flDuckAmount = Approach(1.0f, player->m_flDuckAmount, frametime * duckSpeed);

		// Losing the ground finishes the duck immediately, whatever the interpolation says
		if (player->m_flDuckAmount >= 1.0f || !playerTouchingGround)
		{
			FinishDuck();
		}
		else
		{
			SetDuckedEyeOffset(player->m_flDuckAmount);
		}

		if (player->m_flDuckAmount >= 0.1f && !player->HasFlag(SRC_FL_ANIMDUCKING))
		{
			player->AddFlag(SRC_FL_ANIMDUCKING);
		}
	}

	if (!duckButtonHeld && player->m_bDucking && (params->bAllowAutoMovement || !playerTouchingGround))
	{
		if (CanUnduck())
		{
			// The floor of 1.5 stops a spammed duck key leaving you standing up in slow motion
			srcfloat duckSpeed = SrcMax(1.5f, player->m_flDuckSpeed);

			if (player->m_bIsDefusing)
			{
				duckSpeed *= 0.4f;
			}

			player->m_flDuckAmount = Approach(0.0f, player->m_flDuckAmount, frametime * duckSpeed);

			// m_bDucked is cleared here, at the start of the interpolation, not in FinishUnDuck
			player->m_bDucked = false;

			if (player->m_flDuckAmount <= 0.0f || !playerTouchingGround)
			{
				FinishUnDuck();
			}
			else
			{
				SetDuckedEyeOffset(player->m_flDuckAmount);
			}

			// Both flags drop at 0.75, which the original itself calls inconsistent
			if (player->m_flDuckAmount <= 0.75f && player->HasFlag(SRC_FL_ANIMDUCKING | SRC_FL_DUCKING))
			{
				player->RemoveFlag(SRC_FL_ANIMDUCKING | SRC_FL_DUCKING);
			}
		}
		else
		{
			// Blocked by a ceiling
			player->m_flDuckAmount = 1.0f;
			player->m_bDucked = true;
			player->m_bDucking = false;
			player->AddFlag(SRC_FL_ANIMDUCKING | SRC_FL_DUCKING);

			SetDuckedEyeOffset(player->m_flDuckAmount);
		}
	}

	if (player->m_flDuckAmount <= 0 && player->HasFlag(SRC_FL_ANIMDUCKING))
	{
		player->RemoveFlag(SRC_FL_ANIMDUCKING);
	}

	HandleDuckingSpeedCrop(player->m_flDuckAmount);
}

void FSourceMovementSim::HandleDuckingSpeedCrop(srcfloat duckFraction)
{
	// Runs at the end of Duck, after the inputs were already clamped, so the crop compounds
	if (!(m_iSpeedCropped & SRC_SPEED_CROPPED_DUCK))
	{
		// Note the test is on the BUTTON or either duck flag - not on duckFraction
		if ((mv->Buttons & SRC_IN_DUCK) || player->m_bDucking || player->HasFlag(SRC_FL_DUCKING))
		{
			const srcfloat duckSpeedModifier = params->GetDuckSpeedModifier(duckFraction);
			mv->ForwardMove *= duckSpeedModifier;
			mv->SideMove *= duckSpeedModifier;
			mv->UpMove *= duckSpeedModifier;
			mv->MaxSpeed *= duckSpeedModifier;
			m_iSpeedCropped |= SRC_SPEED_CROPPED_DUCK;
		}
	}
}

// Legacy duck-jump eye interpolation

void FSourceMovementSim::UpdateDuckJumpEyeOffset()
{
	if (player->m_nDuckJumpTimeMsecs != 0)
	{
		const int32 nDuckMilliseconds =
			SrcMax(0, FSourceMovementParams::GameMovementDuckTime - player->m_nDuckJumpTimeMsecs);

		if (nDuckMilliseconds > FSourceMovementParams::TimeToUnduckMsecs)
		{
			player->m_nDuckJumpTimeMsecs = 0;
			SetDuckedEyeOffset(0.0f);
		}
		else
		{
			const srcfloat flDuckFraction = SimpleSpline(1.0f - params->FractionUnDucked(nDuckMilliseconds));
			SetDuckedEyeOffset(flDuckFraction);
		}
	}
}
