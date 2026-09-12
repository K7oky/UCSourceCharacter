#pragma once

#include "Core/SourceVector.h"

// 32x32x72 standing, 32x32x54 ducked. Same mins, so ducking does not move the origin.
struct FSourcePlayerHullSet
{
	FSrcVec3 StandMins{ -16.f, -16.f, 0.f };
	FSrcVec3 StandMaxs{ 16.f, 16.f, 72.f };
	FSrcVec3 StandView{ 0.f, 0.f, 64.f };
	FSrcVec3 DuckMins{ -16.f, -16.f, 0.f };
	FSrcVec3 DuckMaxs{ 16.f, 16.f, 54.f };
	FSrcVec3 DuckView{ 0.f, 0.f, 46.f };
	FSrcVec3 ObsMins{ -10.f, -10.f, -10.f };
	FSrcVec3 ObsMaxs{ 10.f, 10.f, 10.f };
	FSrcVec3 DeadView{ 0.f, 0.f, 14.f };

	FORCEINLINE const FSrcVec3& GetPlayerMins(bool bDucked) const { return bDucked ? DuckMins : StandMins; }
	FORCEINLINE const FSrcVec3& GetPlayerMaxs(bool bDucked) const { return bDucked ? DuckMaxs : StandMaxs; }
	FORCEINLINE const FSrcVec3& GetPlayerViewOffset(bool bDucked) const { return bDucked ? DuckView : StandView; }

	FORCEINLINE FSrcVec3 GetHullSizeNormal() const { return StandMaxs - StandMins; }
	FORCEINLINE FSrcVec3 GetHullSizeCrouch() const { return DuckMaxs - DuckMins; }
	FORCEINLINE FSrcVec3 GetHullMinDelta() const { return DuckMins - StandMins; }
};
