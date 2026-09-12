#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Core/SourceMovementSim.h"
#include "Input/SourceInputTranslator.h"
#include "World/SourceWorldQuery_Analytic.h"

struct FSourceTestFixture
{
	FSourceMovementSim Sim;
	FSourceMovementState State;
	FSourceMovementParams Params;
	FSourceWorldQuery_Analytic World;
	FSourceInputTranslator Translator;
	FSourceInputState Input;
	int32 NextCommandNumber = 1;

	// 64 or 128, the two rates CS actually runs at
	explicit FSourceTestFixture(int32 TickRateHz = 64)
	{
		Params.TickInterval = 1.0f / static_cast<srcfloat>(TickRateHz);

		State.MoveType = ESrcMoveType::Walk;
		State.ViewOffset = Params.Hulls.GetPlayerViewOffset(false);

		State.m_flMaxspeed = Params.PlayerSpeedRun;

		// CategorizePosition's own reset value
		State.m_surfaceFriction = 1.0f;
	}

	srcfloat Dt() const { return Params.TickInterval; }

	void AddFlatGround(srcfloat TopZ = 0.f)
	{
		World.AddGroundPlane(TopZ);
	}

	bool SettleOnGround(srcfloat GroundTopZ = 0.f, int32 MaxTicks = 8)
	{
		State.Origin = FSrcVec3(0.f, 0.f, GroundTopZ + FSourceMovementParams::DistEpsilon);

		State.Velocity = FSrcVec3::Zero;

		for (int32 i = 0; i < MaxTicks; ++i)
		{
			StepIdle();
			if (State.IsOnGround())
			{
				return true;
			}
		}
		return State.IsOnGround();
	}

	void Step()
	{
		const FSourceUserCmd Cmd = Translator.Translate(Input, NextCommandNumber++, State.m_nTickBase);
		Sim.RunCommand(State, Cmd, Params, World);
	}

	void StepIdle()
	{
		const FSourceInputState Saved = Input;
		Input.ForwardMove = 0.f;
		Input.SideMove = 0.f;
		Input.UpMove = 0.f;
		Input.ClearButtons();
		Step();

		Input = Saved;
	}

	void StepN(int32 N)
	{
		for (int32 i = 0; i < N; ++i)
		{
			Step();
		}
	}

	void HoldForward() { Input.ForwardMove = 1.f; }
	void HoldBack() { Input.ForwardMove = -1.f; }
	void HoldRight() { Input.SideMove = 1.f; }
	void HoldLeft() { Input.SideMove = -1.f; }
	void ReleaseMove() { Input.ForwardMove = 0.f; Input.SideMove = 0.f; Input.UpMove = 0.f; }

	void SetYaw(srcfloat Yaw) { Input.ViewAngles.y = Yaw; }

	srcfloat GetVelocityYawDegrees() const
	{
		return FMath::RadiansToDegrees(FMath::Atan2(State.Velocity.y, State.Velocity.x));
	}

	FSourceMoveData& BindStageContext()
	{
		const FSourceUserCmd Cmd = Translator.Translate(Input, NextCommandNumber, State.m_nTickBase);
		FSourceMovementSim::SetupMove(State, Cmd, Params, StageMove);

		StageMove.MaxSpeed = Params.PlayerSpeedRun;
		Sim.Test_BindContext(State, StageMove, Params, World);

		return StageMove;
	}

	FSourceMoveData StageMove;
};

// Shorthand used throughout the tests; values are in the hundreds so this is ~1e-6 relative
inline bool SrcNearly(srcfloat A, srcfloat B, srcfloat Tolerance = 1.e-3f)
{
	return FMath::Abs(A - B) <= Tolerance;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
