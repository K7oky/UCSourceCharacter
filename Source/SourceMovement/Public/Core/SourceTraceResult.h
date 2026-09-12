#pragma once

#include "Core/SourceMovementTypes.h"
#include "Core/SourceVector.h"

// What a sweep returns, in Source's terms. The movement code never sees an FHitResult.
struct FSourceTraceResult
{
	bool bDidHit = false;
	bool bStartSolid = false;
	bool bAllSolid = false;

	// True when PlaneNormal/PlaneDist were actually written by a surface hit
	bool bValidPlane = false;
	srcfloat Fraction = 1.0f;
	FSrcVec3 StartPos = FSrcVec3::Zero;
	FSrcVec3 EndPos = FSrcVec3::Zero;

	// Zero on a miss, deliberately: StepMove depends on it
	FSrcVec3 PlaneNormal = FSrcVec3::Zero;
	srcfloat PlaneDist = 0.0f;

	// Depth the sweep origin was embedded when bStartSolid
	srcfloat PenetrationDepth = 0.0f;
	int32 Contents = SRC_CONTENTS_EMPTY;
	int32 SurfacePropsId = INDEX_NONE;

	// Reduced to what the movement code actually asks of a hit entity
	FSrcEntityRef Entity = FSrcEntityRef::None();

	FORCEINLINE bool DidHitWorld() const { return Entity.bIsWorld; }

	FORCEINLINE bool DidHit() const { return bDidHit; }

	// Resets to the "completed the whole sweep, hit nothing" state
	FORCEINLINE void Reset(const FSrcVec3& InStart, const FSrcVec3& InEnd)
	{
		bDidHit = false;
		bStartSolid = false;
		bAllSolid = false;
		bValidPlane = false;
		Fraction = 1.0f;
		StartPos = InStart;
		EndPos = InEnd;
		PlaneNormal = FSrcVec3::Zero;
		PlaneDist = 0.0f;
		PenetrationDepth = 0.0f;
		Contents = SRC_CONTENTS_EMPTY;
		SurfacePropsId = INDEX_NONE;
		Entity = FSrcEntityRef::None();
	}
};

struct FSourceTraceFilter
{
	// The player is always skipped by its own sweeps
	int32 SkipEntityId = INDEX_NONE;

	// A second entity to skip
	int32 SkipEntityId2 = INDEX_NONE;

	// Team content bits, so teammates can be passed through
	int32 TeamMask = 0;

	// The team logic only applies to player movement sweeps
	bool bPlayerMovementGroup = true;
};
