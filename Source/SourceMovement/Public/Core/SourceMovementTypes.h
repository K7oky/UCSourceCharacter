#pragma once

#include "Core/SourceAngles.h"
#include "Core/SourceVector.h"

enum ESrcButtons : uint32
{
	SRC_IN_JUMP = (1u << 1), SRC_IN_DUCK = (1u << 2), SRC_IN_FORWARD = (1u << 3),
	SRC_IN_BACK = (1u << 4), SRC_IN_CANCEL = (1u << 6), SRC_IN_LEFT = (1u << 7),
	SRC_IN_RIGHT = (1u << 8), SRC_IN_MOVELEFT = (1u << 9), SRC_IN_MOVERIGHT = (1u << 10),
	SRC_IN_RUN = (1u << 12), SRC_IN_ALT1 = (1u << 14), SRC_IN_ALT2 = (1u << 15),
	SRC_IN_SCORE = (1u << 16), SRC_IN_SPEED = (1u << 17), SRC_IN_WALK = (1u << 18), SRC_IN_ZOOM = (1u << 19),
	SRC_IN_WEAPON1 = (1u << 20), SRC_IN_WEAPON2 = (1u << 21), SRC_IN_BULLRUSH = (1u << 22),

	// CS reuses IN_BULLRUSH to remember the pre-gameplay IN_DUCK value
	SRC_IN_RAWDUCK = SRC_IN_BULLRUSH
};

// Values must match Source, they are compared numerically
enum class ESrcMoveType : uint8
{
	None = 0,
	Isometric = 1,
	Walk = 2,
	Step = 3,
	Fly = 4,
	FlyGravity = 5,
	VPhysics = 6,
	Push = 7,
	NoClip = 8,
	Ladder = 9,
	Observer = 10,
	Custom = 11
};

enum class ESrcWaterLevel : uint8
{
	NotInWater = 0, Feet = 1, Waist = 2, Eyes = 3
};

enum class ESrcStepSoundTime : uint8
{
	Normal = 0, OnLadder = 1, WaterKnee = 2, WaterFoot = 3
};

// Only the flags the movement code reads or writes
enum ESrcEntityFlags : uint32
{
	SRC_FL_ONGROUND = (1u << 0),
	SRC_FL_DUCKING = (1u << 1),  // fully crouched
	SRC_FL_ANIMDUCKING = (1u << 2),  // crouching or uncrouching, possibly mid-transition
	SRC_FL_ONTRAIN = (1u << 4),
	SRC_FL_FROZEN = (1u << 6),
	SRC_FL_ATCONTROLS = (1u << 7),
	SRC_FL_BASEVELOCITY = (1u << 24)
};

// Content bits used by PlayerSolidMask
enum ESrcContents : int32
{
	SRC_CONTENTS_EMPTY = 0, SRC_CONTENTS_SOLID = 0x1, SRC_CONTENTS_WINDOW = 0x2, SRC_CONTENTS_GRATE = 0x8,
	SRC_CONTENTS_SLIME = 0x10, SRC_CONTENTS_WATER = 0x20, SRC_CONTENTS_PLAYERCLIP = 0x10000,
	SRC_CONTENTS_MONSTERCLIP = 0x20000, SRC_CONTENTS_LADDER = 0x100000, SRC_CONTENTS_MOVEABLE = 0x4000,
	SRC_CONTENTS_MONSTER = 0x2000000, SRC_CONTENTS_TEAM1 = 0x800, SRC_CONTENTS_TEAM2 = 0x1000,
	SRC_MASK_PLAYERSOLID = SRC_CONTENTS_SOLID | SRC_CONTENTS_MOVEABLE | SRC_CONTENTS_PLAYERCLIP
		| SRC_CONTENTS_WINDOW | SRC_CONTENTS_MONSTER | SRC_CONTENTS_GRATE,
	SRC_MASK_PLAYERSOLID_BRUSHONLY = SRC_CONTENTS_SOLID | SRC_CONTENTS_MOVEABLE
		| SRC_CONTENTS_WINDOW | SRC_CONTENTS_PLAYERCLIP | SRC_CONTENTS_GRATE,

	SRC_MASK_WATER = SRC_CONTENTS_WATER | SRC_CONTENTS_SLIME
};

// Which speed crops have already been applied this command
enum ESrcSpeedCropped : uint8
{
	SRC_SPEED_CROPPED_RESET = 0, SRC_SPEED_CROPPED_DUCK = 1, SRC_SPEED_CROPPED_WEAPON = 2
};

// Handle to another entity: the ground we stand on, or whatever a sweep hit
struct FSrcEntityRef
{
	int32 EntityId = INDEX_NONE;
	bool bIsPlayer = false;
	bool bIsWorld = false;
	FSrcVec3 AbsVelocity = FSrcVec3::Zero;

	FORCEINLINE bool IsValid() const { return EntityId != INDEX_NONE; }

	static FORCEINLINE FSrcEntityRef None() { return FSrcEntityRef(); }

	static FORCEINLINE FSrcEntityRef World()
	{
		FSrcEntityRef Ref;
		Ref.EntityId = 0;
		Ref.bIsWorld = true;
		return Ref;
	}

	FORCEINLINE bool operator==(const FSrcEntityRef& Other) const { return EntityId == Other.EntityId; }
	FORCEINLINE bool operator!=(const FSrcEntityRef& Other) const { return EntityId != Other.EntityId; }
};

// Ground state, reported for telemetry and tests
enum class ESrcGroundState : uint8
{
	Airborne = 0, Grounded = 1
};

enum class ESrcMoveState : uint8
{
	Idle = 0, Walk = 1, Run = 2
};
