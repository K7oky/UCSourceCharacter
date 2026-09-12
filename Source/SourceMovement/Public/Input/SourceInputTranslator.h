#pragma once

#include "Core/SourceMovementState.h"

// What the pawn collects each tick; everything engine-specific stops here
struct FSourceInputState
{
	// +1 = forward
	srcfloat ForwardMove = 0.f;

	// +1 = right (matches IN_MOVERIGHT)
	srcfloat SideMove = 0.f;

	// +1 = up; only used by swim/noclip/ladder
	srcfloat UpMove = 0.f;
	bool bJump = false;
	bool bDuck = false;

	// IN_SPEED, the CS:GO "walk" (shift) modifier
	bool bWalk = false;

	// Absolute view angles in Source convention (pitch, yaw, roll)
	FSrcAngles ViewAngles = FSrcAngles();

	void ClearButtons()
	{
		bJump = bDuck = bWalk = false;
	}
};

struct FSourceInputTranslator
{
	// cl_forwardspeed, 450 in this build
	srcfloat ForwardSpeed = 450.f;

	// cl_backspeed
	srcfloat BackSpeed = 450.f;

	// cl_sidespeed
	srcfloat SideSpeed = 450.f;

	// cl_upspeed
	srcfloat UpSpeed = 320.f;

	FSourceUserCmd Translate(const FSourceInputState& Input, int32 CommandNumber, int32 TickCount) const
	{
		FSourceUserCmd Cmd;
		Cmd.CommandNumber = CommandNumber;
		Cmd.TickCount = TickCount;
		Cmd.ViewAngles = Input.ViewAngles;
		Cmd.ForwardMove = (Input.ForwardMove >= 0.f) ? (Input.ForwardMove * ForwardSpeed) : (Input.ForwardMove * BackSpeed);

		Cmd.SideMove = Input.SideMove * SideSpeed;
		Cmd.UpMove = Input.UpMove * UpSpeed;
		uint32 Buttons = 0;
		if (Input.bJump) { Buttons |= SRC_IN_JUMP; }
		if (Input.bDuck) { Buttons |= SRC_IN_DUCK; }
		if (Input.bWalk) { Buttons |= SRC_IN_SPEED; }

		// CheckParameters reads these direction bits, not the analog values
		if (Input.ForwardMove > 0.f) { Buttons |= SRC_IN_FORWARD; }
		if (Input.ForwardMove < 0.f) { Buttons |= SRC_IN_BACK; }
		if (Input.SideMove > 0.f) { Buttons |= SRC_IN_MOVERIGHT; }
		if (Input.SideMove < 0.f) { Buttons |= SRC_IN_MOVELEFT; }

		Cmd.Buttons = Buttons;
		return Cmd;
	}
};
