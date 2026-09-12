#pragma once

#include "Core/SourceScalar.h"

struct FSourceKButton
{
	// The key ids currently holding this button, 0 means empty
	int32 Down[2] = { 0, 0 };

	// Bit 1 held, bit 2 pressed this frame, bit 4 released this frame
	int32 State = 0;

	// Held right now, ignoring the press/release bits
	FORCEINLINE bool IsHeld() const { return (State & 1) != 0; }

	// What CalcButtonBits tests: held OR pressed this frame, which is what makes wheel binds work
	FORCEINLINE bool GetButtonBit() const { return (State & 3) != 0; }

	// CalcButtonBits clears the impulse bit with `clearmask = ~2` when resetting state
	FORCEINLINE void ClearButtonImpulse() { State &= ~2; }

	void Reset()
	{
		Down[0] = Down[1] = 0;
		State = 0;
	}

	void KeyDown(int32 KeyId)
	{
		if (KeyId == Down[0] || KeyId == Down[1])
		{
			return;  // repeating key
		}

		if (!Down[0])
		{
			Down[0] = KeyId;
		}
		else if (!Down[1])
		{
			Down[1] = KeyId;
		}
		else
		{
			// Source logs "Three keys down for a button" and drops the third
			return;
		}

		if (State & 1)
		{
			return;  // still down
		}

		State |= 1 + 2;  // down + impulse down
	}

	void KeyUp(int32 KeyId)
	{
		if (KeyId == 0)
		{
			Down[0] = Down[1] = 0;
			State = 4;  // impulse up
			return;
		}

		if (Down[0] == KeyId)
		{
			Down[0] = 0;
		}
		else if (Down[1] == KeyId)
		{
			Down[1] = 0;
		}
		else
		{
			return;  // key up without a corresponding down (menu pass-through)
		}

		if (Down[0] || Down[1])
		{
			return;  // some other key is still holding it down
		}

		if (!(State & 1))
		{
			return;  // still up (this should not happen)
		}

		State &= ~1;  // now up
		State |= 4;  // impulse up
	}

	// Keeps the dead branch that assigns 0 either way
	srcfloat KeyState()
	{
		srcfloat val = 0.0f;
		const int32 impulsedown = State & 2;
		const int32 impulseup = State & 4;
		const int32 down = State & 1;

		if (impulsedown && !impulseup)
		{
			// Pressed and held this frame?
			val = down ? 0.5f : 0.0f;
		}

		if (impulseup && !impulsedown)
		{
			// Released this frame?
			val = down ? 0.0f : 0.0f;
		}

		if (!impulsedown && !impulseup)
		{
			// Held the entire frame?
			val = down ? 1.0f : 0.0f;
		}

		if (impulsedown && impulseup)
		{
			if (down)
			{
				val = 0.75f;  // released and re-pressed this frame
			}
			else
			{
				val = 0.25f;  // pressed and released this frame
			}
		}

		State &= 1;  // clear impulses
		return val;
	}
};
