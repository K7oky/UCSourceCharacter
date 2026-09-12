#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"

#include "SourceHullComponent.generated.h"

// Query-only box that mirrors the simulation's current player hull
UCLASS(ClassGroup = (SourceMovement), meta = (BlueprintSpawnableComponent))
class SOURCEMOVEMENT_API USourceHullComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	USourceHullComponent();

	void SetFromSourceHull(const struct FSrcVec3& Mins, const struct FSrcVec3& Maxs);
};
