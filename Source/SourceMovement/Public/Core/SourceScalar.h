#pragma once

#include "CoreMinimal.h"

#include <cfloat>
#include <cmath>
#include <limits>

using srcfloat = float;

static_assert(sizeof(srcfloat) == 4, "srcfloat must be IEEE single precision for Source compatibility.");
static_assert(std::numeric_limits<srcfloat>::is_iec559, "srcfloat must be IEEE 754.");

namespace SourceMath
{
	// Divide-by-zero guard inside VectorNormalize
	inline constexpr srcfloat FltEpsilon = FLT_EPSILON;
	inline constexpr srcfloat PiF = 3.14159265358979323846f;
	inline constexpr srcfloat DegToRadF = static_cast<srcfloat>(PiF / 180.f);

	FORCEINLINE constexpr srcfloat Deg2Rad(srcfloat Degrees)
	{
		return Degrees * DegToRadF;
	}

	FORCEINLINE srcfloat FastSqrt(srcfloat X)
	{
		return std::sqrt(X);
	}

	FORCEINLINE void SinCos(srcfloat Radians, srcfloat& OutSin, srcfloat& OutCos)
	{
		OutSin = std::sin(Radians);
		OutCos = std::cos(Radians);
	}

	template <typename T>
	FORCEINLINE constexpr T SrcClamp(const T& Val, const T& MinVal, const T& MaxVal)
	{
		if (Val < MinVal)
		{
			return MinVal;
		}
		else if (Val > MaxVal)
		{
			return MaxVal;
		}
		else
		{
			return Val;
		}
	}

	template <typename T>
	FORCEINLINE constexpr T SrcMin(const T& A, const T& B) { return (A < B) ? A : B; }

	template <typename T>
	FORCEINLINE constexpr T SrcMax(const T& A, const T& B) { return (A > B) ? A : B; }

	// Do not fold to value*value*(3 - 2*value), the rounding differs
	FORCEINLINE srcfloat SimpleSpline(srcfloat Value)
	{
		const srcfloat ValueSquared = Value * Value;
		return (3 * ValueSquared - 2 * ValueSquared * Value);
	}

	FORCEINLINE srcfloat SplineFraction(srcfloat Value, srcfloat Scale)
	{
		Value = Scale * Value;
		const srcfloat ValueSquared = Value * Value;
		return 3 * ValueSquared - 2 * ValueSquared * Value;
	}

	FORCEINLINE srcfloat Approach(srcfloat Target, srcfloat Value, srcfloat Speed)
	{
		const srcfloat Delta = Target - Value;

		if (Delta > Speed)
		{
			Value += Speed;
		}
		else if (Delta < -Speed)
		{
			Value -= Speed;
		}
		else
		{
			Value = Target;
		}

		return Value;
	}

	FORCEINLINE srcfloat AngleNormalize(srcfloat Angle)
	{
		Angle = std::fmod(Angle, 360.0f);
		if (Angle > 180)
		{
			Angle -= 360;
		}
		if (Angle < -180)
		{
			Angle += 360;
		}
		return Angle;
	}

	// The NaN guard CheckVelocity relies on
	FORCEINLINE bool IsNan(srcfloat X)
	{
		return std::isnan(X);
	}
}
