#pragma once

#include "Core/SourceVector.h"

// Pitch, yaw, roll in Source convention
enum
{
	SRC_PITCH = 0, SRC_YAW = 1, SRC_ROLL = 2
};

struct FSrcAngles
{
	srcfloat x = 0.f;  // PITCH
	srcfloat y = 0.f;  // YAW
	srcfloat z = 0.f;  // ROLL

	FSrcAngles() = default;

	FORCEINLINE constexpr FSrcAngles(srcfloat InPitch, srcfloat InYaw, srcfloat InRoll)
		: x(InPitch), y(InYaw), z(InRoll)
	{
	}

	FORCEINLINE void Init(srcfloat InPitch = 0.f, srcfloat InYaw = 0.f, srcfloat InRoll = 0.f)
	{
		x = InPitch; y = InYaw; z = InRoll;
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

	FORCEINLINE FSrcAngles operator+(const FSrcAngles& A) const { return FSrcAngles(x + A.x, y + A.y, z + A.z); }
	FORCEINLINE FSrcAngles operator-(const FSrcAngles& A) const { return FSrcAngles(x - A.x, y - A.y, z - A.z); }
	FORCEINLINE FSrcAngles operator*(srcfloat S) const { return FSrcAngles(x * S, y * S, z * S); }

	FORCEINLINE FSrcAngles& operator+=(const FSrcAngles& A) { x += A.x; y += A.y; z += A.z; return *this; }
	FORCEINLINE FSrcAngles& operator*=(srcfloat S) { x *= S; y *= S; z *= S; return *this; }

	FORCEINLINE bool operator==(const FSrcAngles& A) const { return x == A.x && y == A.y && z == A.z; }
	FORCEINLINE bool operator!=(const FSrcAngles& A) const { return !(*this == A); }

	// Magnitude, used by the view punch decay
	FORCEINLINE srcfloat Length() const { return SourceMath::FastSqrt(x * x + y * y + z * z); }
	FORCEINLINE srcfloat LengthSqr() const { return x * x + y * y + z * z; }
};

namespace SourceMath
{
	inline void AngleVectors(const FSrcAngles& Angles, FSrcVec3* Forward, FSrcVec3* Right, FSrcVec3* Up)
	{
		srcfloat sr, sp, sy, cr, cp, cy;

		SinCos(Deg2Rad(Angles[SRC_YAW]), sy, cy);
		SinCos(Deg2Rad(Angles[SRC_PITCH]), sp, cp);
		SinCos(Deg2Rad(Angles[SRC_ROLL]), sr, cr);

		if (Forward)
		{
			Forward->x = cp * cy;
			Forward->y = cp * sy;
			Forward->z = -sp;
		}

		// The `-1 *` folds are transcribed as written; simplifying them changes the rounding
		if (Right)
		{
			Right->x = (-1 * sr * sp * cy + -1 * cr * -sy);
			Right->y = (-1 * sr * sp * sy + -1 * cr * cy);

			Right->z = -1 * sr * cp;
		}

		if (Up)
		{
			Up->x = (cr * sp * cy + -sr * -sy);
			Up->y = (cr * sp * sy + -sr * cy);

			Up->z = cr * cp;
		}
	}

	inline void AngleVectors(const FSrcAngles& Angles, FSrcVec3* Forward)
	{
		AngleVectors(Angles, Forward, nullptr, nullptr);
	}

	inline void DecayAngles(FSrcAngles& V, srcfloat FExp, srcfloat FLin, srcfloat DT)
	{
		FExp *= DT;
		FLin *= DT;
		V *= std::exp(-FExp);
		const srcfloat FMag = V.Length();
		if (FMag > FLin)
		{
			V *= (1.0f - FLin / FMag);
		}
		else
		{
			V.Init(0.0f, 0.0f, 0.0f);
		}
	}
}
