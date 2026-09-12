#pragma once

#include "Core/SourceScalar.h"

// Lowercase x/y/z on purpose; anything crossing into Unreal space goes through SourceUnits
struct FSrcVec3
{
	srcfloat x = 0.f;
	srcfloat y = 0.f;
	srcfloat z = 0.f;

	FSrcVec3() = default;

	FORCEINLINE constexpr FSrcVec3(srcfloat InX, srcfloat InY, srcfloat InZ) : x(InX), y(InY), z(InZ)
	{
	}

	FORCEINLINE void Init(srcfloat InX = 0.f, srcfloat InY = 0.f, srcfloat InZ = 0.f)
	{
		x = InX; y = InY; z = InZ;
	}

	FORCEINLINE srcfloat& operator[](int32 i)
	{
		checkSlow(i >= 0 && i < 3);
		return (&x)[i];
	}

	FORCEINLINE srcfloat operator[](int32 i) const
	{
		checkSlow(i >= 0 && i < 3);
		return (&x)[i];
	}

	FORCEINLINE FSrcVec3 operator+(const FSrcVec3& V) const { return FSrcVec3(x + V.x, y + V.y, z + V.z); }
	FORCEINLINE FSrcVec3 operator-(const FSrcVec3& V) const { return FSrcVec3(x - V.x, y - V.y, z - V.z); }
	FORCEINLINE FSrcVec3 operator*(srcfloat S) const { return FSrcVec3(x * S, y * S, z * S); }
	FORCEINLINE FSrcVec3 operator/(srcfloat S) const { return FSrcVec3(x / S, y / S, z / S); }
	FORCEINLINE FSrcVec3 operator-() const { return FSrcVec3(-x, -y, -z); }

	FORCEINLINE FSrcVec3& operator+=(const FSrcVec3& V) { x += V.x; y += V.y; z += V.z; return *this; }
	FORCEINLINE FSrcVec3& operator-=(const FSrcVec3& V) { x -= V.x; y -= V.y; z -= V.z; return *this; }
	FORCEINLINE FSrcVec3& operator*=(srcfloat S) { x *= S; y *= S; z *= S; return *this; }
	FORCEINLINE FSrcVec3& operator/=(srcfloat S) { x /= S; y /= S; z /= S; return *this; }

	// Exact float equality
	FORCEINLINE bool operator==(const FSrcVec3& V) const { return x == V.x && y == V.y && z == V.z; }
	FORCEINLINE bool operator!=(const FSrcVec3& V) const { return !(*this == V); }

	// Summed (x*x + y*y) + z*z, left to right; the rounding depends on it
	FORCEINLINE srcfloat LengthSqr() const { return x * x + y * y + z * z; }

	FORCEINLINE srcfloat Length() const { return SourceMath::FastSqrt(x * x + y * y + z * z); }

	FORCEINLINE srcfloat Length2DSqr() const { return x * x + y * y; }

	FORCEINLINE srcfloat Length2D() const { return SourceMath::FastSqrt(x * x + y * y); }

	FORCEINLINE srcfloat Dot(const FSrcVec3& V) const { return x * V.x + y * V.y + z * V.z; }

	FORCEINLINE bool IsZero(srcfloat Tolerance = 0.01f) const
	{
		return (x > -Tolerance && x < Tolerance && y > -Tolerance && y < Tolerance && z > -Tolerance && z < Tolerance);
	}

	FORCEINLINE void Negate() { x = -x; y = -y; z = -z; }

	srcfloat NormalizeInPlace();

	static const FSrcVec3 Zero;
};
inline const FSrcVec3 FSrcVec3::Zero = FSrcVec3(0.f, 0.f, 0.f);

static_assert(sizeof(FSrcVec3) == 3 * sizeof(srcfloat), "FSrcVec3 must be three tightly packed floats for operator[] and for Source-compatible layout.");

FORCEINLINE FSrcVec3 operator*(srcfloat S, const FSrcVec3& V) { return V * S; }

namespace SourceMath
{
	FORCEINLINE srcfloat VectorNormalize(FSrcVec3& Vec)
	{
		const srcfloat Radius = FastSqrt(Vec.x * Vec.x + Vec.y * Vec.y + Vec.z * Vec.z);
		const srcfloat IRadius = 1.f / (Radius + FltEpsilon);

		Vec.x *= IRadius;
		Vec.y *= IRadius;
		Vec.z *= IRadius;
		return Radius;
	}

	FORCEINLINE srcfloat VectorLength(const FSrcVec3& V) { return V.Length(); }

	FORCEINLINE srcfloat DotProduct(const FSrcVec3& A, const FSrcVec3& B) { return A.Dot(B); }
	FORCEINLINE void CrossProduct(const FSrcVec3& A, const FSrcVec3& B, FSrcVec3& Out)
	{
		Out.x = A.y * B.z - A.z * B.y;
		Out.y = A.z * B.x - A.x * B.z;
		Out.z = A.x * B.y - A.y * B.x;
	}
	FORCEINLINE void VectorCopy(const FSrcVec3& Src, FSrcVec3& Dst) { Dst = Src; }
	FORCEINLINE void VectorScale(const FSrcVec3& In, srcfloat Scale, FSrcVec3& Out)
	{
		Out.x = In.x * Scale;
		Out.y = In.y * Scale;
		Out.z = In.z * Scale;
	}
	FORCEINLINE void VectorAdd(const FSrcVec3& A, const FSrcVec3& B, FSrcVec3& Out)
	{
		Out.x = A.x + B.x;
		Out.y = A.y + B.y;
		Out.z = A.z + B.z;
	}
	FORCEINLINE void VectorSubtract(const FSrcVec3& A, const FSrcVec3& B, FSrcVec3& Out)
	{
		Out.x = A.x - B.x;
		Out.y = A.y - B.y;
		Out.z = A.z - B.z;
	}
	FORCEINLINE void VectorMultiply(const FSrcVec3& A, srcfloat Scale, FSrcVec3& Out)
	{
		VectorScale(A, Scale, Out);
	}
	FORCEINLINE void VectorMA(const FSrcVec3& Start, srcfloat Scale, const FSrcVec3& Direction, FSrcVec3& Dest)
	{
		Dest.x = Start.x + Scale * Direction.x;
		Dest.y = Start.y + Scale * Direction.y;
		Dest.z = Start.z + Scale * Direction.z;
	}
}

FORCEINLINE srcfloat FSrcVec3::NormalizeInPlace()
{
	return SourceMath::VectorNormalize(*this);
}
