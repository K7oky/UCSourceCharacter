#include "Actors/SourceHullComponent.h"

#include "SourceUnits.h"

USourceHullComponent::USourceHullComponent()
{
	// Standing hull 32 x 32 x 72, so half extents (16, 16, 36)
	InitBoxExtent(FVector(16.0 * SourceUnits::UnitsToUU, 16.0 * SourceUnits::UnitsToUU, 36.0 * SourceUnits::UnitsToUU));

	SetRelativeLocation(FVector(0.0, 0.0, 36.0 * SourceUnits::UnitsToUU));

	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_Pawn);
	SetGenerateOverlapEvents(true);
	bDynamicObstacle = false;

	// Source player bounds are axis-aligned and don't rotate with the view
	SetUsingAbsoluteRotation(true);
}

void USourceHullComponent::SetFromSourceHull(const FSrcVec3& Mins, const FSrcVec3& Maxs)
{
	FVector CenterOffset;
	FVector Extent;
	SourceUnits::ToUEBox(Mins, Maxs, CenterOffset, Extent);

	SetBoxExtent(Extent, false);
	SetRelativeLocation(CenterOffset);
}
