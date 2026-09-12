#pragma once

#include "CoreMinimal.h"
#include "Core/SourceAngles.h"
#include "Core/SourceVector.h"

// The only conversion layer. Source is right-handed, Unreal is left-handed, so Y is negated.
namespace SourceUnits
{
	// 1 Source unit == 1 Unreal unit, so the player is 72 UU tall. 2.54 for real centimetres.
	inline constexpr double UnitsToUU = 1.0;
	inline constexpr double UUToUnits = 1.0 / UnitsToUU;

	FORCEINLINE FVector ToUEPosition(const FSrcVec3& V)
	{
		return FVector(static_cast<double>(V.x) * UnitsToUU, static_cast<double>(-V.y) * UnitsToUU, static_cast<double>(V.z) * UnitsToUU);
	}

	FORCEINLINE FSrcVec3 ToSrcPosition(const FVector& P)
	{
		return FSrcVec3(static_cast<srcfloat>(P.X * UUToUnits), static_cast<srcfloat>(-P.Y * UUToUnits), static_cast<srcfloat>(P.Z * UUToUnits));
	}

	FORCEINLINE FVector ToUEDirection(const FSrcVec3& V)
	{
		return ToUEPosition(V);
	}

	// Unit normal: reflection only, never scaled
	FORCEINLINE FVector ToUENormal(const FSrcVec3& N)
	{
		return FVector(static_cast<double>(N.x), static_cast<double>(-N.y), static_cast<double>(N.z));
	}

	FORCEINLINE FSrcVec3 ToSrcNormal(const FVector& N)
	{
		return FSrcVec3(static_cast<srcfloat>(N.X), static_cast<srcfloat>(-N.Y), static_cast<srcfloat>(N.Z));
	}

	FORCEINLINE void ToUEBox(const FSrcVec3& Mins, const FSrcVec3& Maxs, FVector& OutCenterOffset, FVector& OutExtent)
	{
		const FSrcVec3 SrcCenter = (Mins + Maxs) * 0.5f;
		const FSrcVec3 SrcExtent = (Maxs - Mins) * 0.5f;
		OutCenterOffset = ToUEPosition(SrcCenter);

		OutExtent = FVector(FMath::Abs(static_cast<double>(SrcExtent.x) * UnitsToUU), FMath::Abs(static_cast<double>(SrcExtent.y) * UnitsToUU), FMath::Abs(static_cast<double>(SrcExtent.z) * UnitsToUU));
	}

	FORCEINLINE FRotator ToUERotator(const FSrcAngles& A)
	{
		return FRotator(static_cast<double>(-A.x), static_cast<double>(-A.y), static_cast<double>(A.z));
	}

	FORCEINLINE FSrcAngles ToSrcAngles(const FRotator& R)
	{
		return FSrcAngles(static_cast<srcfloat>(-R.Pitch), static_cast<srcfloat>(-R.Yaw), static_cast<srcfloat>(R.Roll));
	}
}
