#pragma once

#include "Core/SourceTraceResult.h"
#include "Core/SourceVector.h"

// Everything the simulation may ask about the world; keeps Unreal types out of the port
class ISourceWorldQuery
{
public:
	virtual ~ISourceWorldQuery() = default;

	// Sweeps the player box from Start to End
	virtual void TraceHull(const FSrcVec3& Start, const FSrcVec3& End, const FSrcVec3& Mins, const FSrcVec3& Maxs, int32 ContentMask, const FSourceTraceFilter& Filter, FSourceTraceResult& Out) = 0;

	// Content bits at a point, used by the water checks
	virtual int32 GetPointContents(const FSrcVec3& Point, int32 ContentMask) = 0;

	virtual srcfloat GetSurfaceFriction(int32 SurfacePropsId) const = 0;
	virtual srcfloat GetSurfaceJumpFactor(int32 SurfacePropsId) const = 0;
	virtual srcfloat GetSurfaceMaxSpeedFactor(int32 SurfacePropsId) const = 0;

	virtual bool IsSurfaceClimbable(int32 SurfacePropsId) const = 0;

	virtual FSrcVec3 GetEntityAbsVelocity(const FSrcEntityRef& Entity) const = 0;
};
