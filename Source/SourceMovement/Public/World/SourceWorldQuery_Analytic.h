#pragma once

#include "Core/ISourceWorldQuery.h"
#include "Core/SourceMovementParams.h"

#include "Containers/Array.h"

// Reference backend: exact plane math over hand-built brushes, no Chaos involved
class SOURCEMOVEMENT_API FSourceWorldQuery_Analytic final : public ISourceWorldQuery
{
public:
	struct FBrushPlane
	{
		FSrcVec3 Normal = FSrcVec3(0.f, 0.f, 1.f);

		srcfloat Dist = 0.f;
	};

	struct FBrush
	{
		TArray<FBrushPlane> Planes;
		int32 Contents = SRC_CONTENTS_SOLID;
		FSrcEntityRef Entity = FSrcEntityRef::World();

		int32 SurfacePropsId = 0;
	};

	FSourceWorldQuery_Analytic() = default;

	void Reset() { Brushes.Reset(); }

	int32 AddBox(const FSrcVec3& Mins, const FSrcVec3& Maxs, int32 Contents = SRC_CONTENTS_SOLID, FSrcEntityRef Entity = FSrcEntityRef::World(), int32 SurfacePropsId = 0);

	int32 AddGroundPlane(srcfloat TopZ = 0.f, srcfloat HalfExtent = 16384.f, int32 SurfacePropsId = 0);

	int32 AddRampAlongX(srcfloat MinX, srcfloat MaxX, srcfloat Height, srcfloat HalfWidthY = 4096.f, srcfloat BottomZ = -4096.f, int32 SurfacePropsId = 0);

	int32 AddBrush(const FBrush& Brush);
	void SetSurfaceProperties(int32 SurfacePropsId, srcfloat Friction, srcfloat JumpFactor = 1.f, srcfloat MaxSpeedFactor = 1.f, bool bClimbable = false);

	virtual void TraceHull(const FSrcVec3& Start, const FSrcVec3& End, const FSrcVec3& Mins, const FSrcVec3& Maxs, int32 ContentMask, const FSourceTraceFilter& Filter, FSourceTraceResult& Out) override;

	virtual int32 GetPointContents(const FSrcVec3& Point, int32 ContentMask) override;

	virtual srcfloat GetSurfaceFriction(int32 SurfacePropsId) const override;
	virtual srcfloat GetSurfaceJumpFactor(int32 SurfacePropsId) const override;
	virtual srcfloat GetSurfaceMaxSpeedFactor(int32 SurfacePropsId) const override;
	virtual bool IsSurfaceClimbable(int32 SurfacePropsId) const override;
	virtual FSrcVec3 GetEntityAbsVelocity(const FSrcEntityRef& Entity) const override;

	// Entity velocities for moving platforms; absent entries are stationary
	void SetEntityVelocity(int32 EntityId, const FSrcVec3& Velocity) { EntityVelocities.Add(EntityId, Velocity); }

	int32 GetBrushCount() const { return Brushes.Num(); }

private:
	struct FSurfaceProps
	{
		srcfloat Friction = 0.8f;  // the "default" material
		srcfloat JumpFactor = 1.0f;
		srcfloat MaxSpeedFactor = 1.0f;
		bool bClimbable = false;
	};

	void ClipBoxToBrush(const FBrush& Brush, const FSrcVec3& P1, const FSrcVec3& P2, const FSrcVec3& Extents, bool bIsPoint, FSourceTraceResult& Out) const;

	bool ShouldHitEntity(const FSrcEntityRef& Entity, const FSourceTraceFilter& Filter) const;

	TArray<FBrush> Brushes;
	TMap<int32, FSurfaceProps> SurfacePropsTable;
	TMap<int32, FSrcVec3> EntityVelocities;
};
