#include "World/SourceWorldQuery_Analytic.h"

using namespace SourceMath;

// World construction

int32 FSourceWorldQuery_Analytic::AddBrush(const FBrush& Brush)
{
	return Brushes.Add(Brush);
}

int32 FSourceWorldQuery_Analytic::AddBox(const FSrcVec3& Mins, const FSrcVec3& Maxs, int32 Contents, FSrcEntityRef Entity, int32 SurfacePropsId)
{
	FBrush Brush;
	Brush.Contents = Contents;
	Brush.Entity = Entity;
	Brush.SurfacePropsId = SurfacePropsId;

	// Six axis-aligned half-spaces
	Brush.Planes.Add({ FSrcVec3(1.f, 0.f, 0.f), Maxs.x });
	Brush.Planes.Add({ FSrcVec3(-1.f, 0.f, 0.f), -Mins.x });
	Brush.Planes.Add({ FSrcVec3(0.f, 1.f, 0.f), Maxs.y });
	Brush.Planes.Add({ FSrcVec3(0.f, -1.f, 0.f), -Mins.y });
	Brush.Planes.Add({ FSrcVec3(0.f, 0.f, 1.f), Maxs.z });
	Brush.Planes.Add({ FSrcVec3(0.f, 0.f, -1.f), -Mins.z });

	return AddBrush(Brush);
}

int32 FSourceWorldQuery_Analytic::AddGroundPlane(srcfloat TopZ, srcfloat HalfExtent, int32 SurfacePropsId)
{
	return AddBox(FSrcVec3(-HalfExtent, -HalfExtent, TopZ - HalfExtent), FSrcVec3(HalfExtent, HalfExtent, TopZ), SRC_CONTENTS_SOLID, FSrcEntityRef::World(), SurfacePropsId);
}

int32 FSourceWorldQuery_Analytic::AddRampAlongX(srcfloat MinX, srcfloat MaxX, srcfloat Height, srcfloat HalfWidthY, srcfloat BottomZ, int32 SurfacePropsId)
{
	// Sloped top face: rises from z = 0 at x = MinX to z = Height at x = MaxX
	const srcfloat dx = MaxX - MinX;

	FSrcVec3 SlopeNormal(-Height, 0.f, dx);
	VectorNormalize(SlopeNormal);

	// Dist so that the plane passes through (MinX, *, 0)
	const srcfloat SlopeDist = SlopeNormal.x * MinX + SlopeNormal.z * 0.f;
	FBrush Brush;
	Brush.Contents = SRC_CONTENTS_SOLID;
	Brush.Entity = FSrcEntityRef::World();
	Brush.SurfacePropsId = SurfacePropsId;

	Brush.Planes.Add({SlopeNormal, SlopeDist});  // sloped top
	Brush.Planes.Add({FSrcVec3(1.f, 0.f, 0.f), MaxX});  // +X cap
	Brush.Planes.Add({FSrcVec3(-1.f, 0.f, 0.f), -MinX});  // -X cap
	Brush.Planes.Add({FSrcVec3(0.f, 1.f, 0.f), HalfWidthY});  // +Y cap
	Brush.Planes.Add({FSrcVec3(0.f, -1.f, 0.f), HalfWidthY});  // -Y cap
	Brush.Planes.Add({FSrcVec3(0.f, 0.f, -1.f), -BottomZ});  // bottom

	return AddBrush(Brush);
}

void FSourceWorldQuery_Analytic::SetSurfaceProperties(int32 SurfacePropsId, srcfloat Friction, srcfloat JumpFactor, srcfloat MaxSpeedFactor, bool bClimbable)
{
	FSurfaceProps Props;
	Props.Friction = Friction;
	Props.JumpFactor = JumpFactor;
	Props.MaxSpeedFactor = MaxSpeedFactor;
	Props.bClimbable = bClimbable;
	SurfacePropsTable.Add(SurfacePropsId, Props);
}

// Trace

bool FSourceWorldQuery_Analytic::ShouldHitEntity(const FSrcEntityRef& Entity, const FSourceTraceFilter& Filter) const
{
	if (Entity.EntityId == Filter.SkipEntityId || Entity.EntityId == Filter.SkipEntityId2)
	{
		return false;
	}

	// Not ported: Source also skips same-team entities for player movement sweeps
	return true;
}

void FSourceWorldQuery_Analytic::ClipBoxToBrush(const FBrush& Brush, const FSrcVec3& P1, const FSrcVec3& P2, const FSrcVec3& Extents, bool bIsPoint, FSourceTraceResult& Out) const
{
	if (Brush.Planes.Num() == 0)
	{
		return;
	}

	constexpr srcfloat DistEps = FSourceMovementParams::DistEpsilon;
	srcfloat enterfrac = -1.f;
	srcfloat leavefrac = 1.f;
	const FBrushPlane* clipplane = nullptr;
	bool getout = false;
	bool startout = false;

	for (const FBrushPlane& Plane : Brush.Planes)
	{
		srcfloat dist;

		if (!bIsPoint)
		{
			// "push the plane out apropriately for mins/maxs"
			const srcfloat Offset = -(FMath::Abs(Plane.Normal.x) * Extents.x + FMath::Abs(Plane.Normal.y) * Extents.y + FMath::Abs(Plane.Normal.z) * Extents.z);
			dist = Plane.Dist - Offset;
		}
		else
		{
			dist = Plane.Dist;
		}

		const srcfloat d1 = DotProduct(P1, Plane.Normal) - dist;
		const srcfloat d2 = DotProduct(P2, Plane.Normal) - dist;

		if (d2 > 0)
		{
			getout = true;  // endpoint is outside this plane, so not fully inside the brush
		}
		if (d1 > 0)
		{
			startout = true;  // startpoint is outside this plane
		}

		// Completely in front of this face: the sweep cannot intersect the brush at all
		if (d1 > 0 && d2 >= d1)
		{
			return;
		}

		// Completely behind this face: it does not constrain the interval
		if (d1 <= 0 && d2 <= 0)
		{
			continue;
		}

		if (d1 > d2)
		{
			// Entering the half-space
			const srcfloat f = (d1 - DistEps) / (d1 - d2);
			if (f > enterfrac)
			{
				enterfrac = f;
				clipplane = &Plane;
			}
		}
		else
		{
			// Leaving the half-space
			const srcfloat f = (d1 + DistEps) / (d1 - d2);
			if (f < leavefrac)
			{
				leavefrac = f;
			}
		}
	}

	if (!startout)
	{
		// The start point was inside the brush
		Out.bStartSolid = true;
		Out.bDidHit = true;
		Out.Contents |= Brush.Contents;

		if (!getout)
		{
			// Entirely inside: TryPlayerMove reacts to this by zeroing velocity and returning 4
			Out.bAllSolid = true;
			Out.Fraction = 0.f;
		}

		// Startsolid without allsolid leaves Fraction alone; it can stay 1.0
		return;
	}

	if (enterfrac < leavefrac)
	{
		if (enterfrac > -1 && enterfrac < Out.Fraction)
		{
			if (enterfrac < 0)
			{
				enterfrac = 0;
			}

			Out.Fraction = enterfrac;
			Out.PlaneNormal = clipplane ? clipplane->Normal : FSrcVec3::Zero;
			Out.PlaneDist = clipplane ? clipplane->Dist : 0.f;
			Out.bValidPlane = (clipplane != nullptr);
			Out.Contents = Brush.Contents;
			Out.SurfacePropsId = Brush.SurfacePropsId;
			Out.Entity = Brush.Entity;
			Out.bDidHit = true;
		}
	}
}

void FSourceWorldQuery_Analytic::TraceHull(const FSrcVec3& Start, const FSrcVec3& End, const FSrcVec3& Mins, const FSrcVec3& Maxs, int32 ContentMask, const FSourceTraceFilter& Filter, FSourceTraceResult& Out)
{
	Out.Reset(Start, End);
	FSrcVec3 Extents = (Maxs - Mins) * 0.5f;
	const FSrcVec3 StartOffset = (Mins + Maxs) * 0.5f;
	const bool bIsRay = (Extents.LengthSqr() < 1e-6f);
	const FSrcVec3 P1 = Start + StartOffset;
	const FSrcVec3 P2 = End + StartOffset;

	for (const FBrush& Brush : Brushes)
	{
		if ((Brush.Contents & ContentMask) == 0)
		{
			continue;
		}

		if (!ShouldHitEntity(Brush.Entity, Filter))
		{
			continue;
		}

		ClipBoxToBrush(Brush, P1, P2, Extents, bIsRay, Out);

		// CM_TraceToLeaf bails out once the trace is fully solid
		if (Out.bAllSolid)
		{
			break;
		}
	}

	// Trace.endpos = start + fraction * (end - start)
	if (Out.Fraction >= 1.0f)
	{
		Out.EndPos = End;
	}
	else
	{
		VectorMA(Start, Out.Fraction, End - Start, Out.EndPos);
	}

	Out.bDidHit = (Out.Fraction < 1.0f) || Out.bStartSolid || Out.bAllSolid;
}

int32 FSourceWorldQuery_Analytic::GetPointContents(const FSrcVec3& Point, int32 ContentMask)
{
	int32 Contents = SRC_CONTENTS_EMPTY;

	for (const FBrush& Brush : Brushes)
	{
		if ((Brush.Contents & ContentMask) == 0)
		{
			continue;
		}

		bool bInside = true;
		for (const FBrushPlane& Plane : Brush.Planes)
		{
			if (DotProduct(Point, Plane.Normal) - Plane.Dist > 0.f)
			{
				bInside = false;
				break;
			}
		}

		if (bInside)
		{
			Contents |= Brush.Contents;
		}
	}

	return Contents;
}

// Surface properties

srcfloat FSourceWorldQuery_Analytic::GetSurfaceFriction(int32 SurfacePropsId) const
{
	// 0.8 is the "default" material's friction
	if (const FSurfaceProps* Props = SurfacePropsTable.Find(SurfacePropsId))
	{
		return Props->Friction;
	}
	return 0.8f;
}

srcfloat FSourceWorldQuery_Analytic::GetSurfaceJumpFactor(int32 SurfacePropsId) const
{
	if (const FSurfaceProps* Props = SurfacePropsTable.Find(SurfacePropsId))
	{
		return Props->JumpFactor;
	}
	return 1.0f;
}

srcfloat FSourceWorldQuery_Analytic::GetSurfaceMaxSpeedFactor(int32 SurfacePropsId) const
{
	if (const FSurfaceProps* Props = SurfacePropsTable.Find(SurfacePropsId))
	{
		return Props->MaxSpeedFactor;
	}
	return 1.0f;
}

bool FSourceWorldQuery_Analytic::IsSurfaceClimbable(int32 SurfacePropsId) const
{
	if (const FSurfaceProps* Props = SurfacePropsTable.Find(SurfacePropsId))
	{
		return Props->bClimbable;
	}
	return false;
}

FSrcVec3 FSourceWorldQuery_Analytic::GetEntityAbsVelocity(const FSrcEntityRef& Entity) const
{
	if (const FSrcVec3* Vel = EntityVelocities.Find(Entity.EntityId))
	{
		return *Vel;
	}
	return FSrcVec3::Zero;
}
