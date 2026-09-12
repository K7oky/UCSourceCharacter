// Footstep audio decision tests
#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/SourceMovementTestFixture.h"

#include "Misc/AutomationTest.h"

namespace
{
	constexpr EAutomationTestFlags SrcStepTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	constexpr srcfloat CSWalkSpeedThreshold = 135.2f;

	// Runs the player forward at full input for N ticks and counts the footsteps
	int32 CountSteps(FSourceTestFixture& Fx, int32 Ticks, bool bHoldWalk)
	{
		Fx.Input.ForwardMove = 1.f;
		Fx.Input.bWalk = bHoldWalk;
		int32 Steps = 0;
		for (int32 i = 0; i < Ticks; ++i)
		{
			Fx.Step();
			if (Fx.Sim.GetEvents().bStepSound)
			{
				++Steps;
			}
		}
		return Steps;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcFootstepRunningIsAudibleTest, "SourceMovement.Footsteps.RunningIsAudible", SrcStepTestFlags)

bool FSrcFootstepRunningIsAudibleTest::RunTest(const FString&)
{
	// Baseline for the walking test below
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);

	// 128 ticks = 2 s at 64 tick
	const int32 Steps = CountSteps(Fx, 128, /*bHoldWalk=*/false);

	TestTrue(TEXT("a running player makes footsteps"), Steps >= 4);
	TestTrue(TEXT("running speed cleared the CS audibility threshold"), Fx.State.Velocity.Length() > CSWalkSpeedThreshold);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcFootstepWalkingIsSilentTest, "SourceMovement.Footsteps.WalkingIsSilent", SrcStepTestFlags)

bool FSrcFootstepWalkingIsSilentTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	const int32 Steps = CountSteps(Fx, 128, /*bHoldWalk=*/true);

	TestEqual(TEXT("+speed produces no footsteps at all"), Steps, 0);
	TestTrue(TEXT("the player really was moving, not just stuck"), Fx.State.Velocity.Length2D() > 50.f);
	TestTrue(TEXT("m_bIsWalking latched"), Fx.State.m_bIsWalking);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcFootstepWalkCapHysteresisTest, "SourceMovement.Footsteps.WalkLatchIsHysteretic", SrcStepTestFlags)

bool FSrcFootstepWalkCapHysteresisTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	Fx.Input.ForwardMove = 1.f;
	Fx.StepN(64);
	const srcfloat RunSpeed = Fx.State.Velocity.Length();
	TestTrue(TEXT("reached running speed"), RunSpeed > CSWalkSpeedThreshold);
	Fx.Input.bWalk = true;
	Fx.Step();

	TestFalse(TEXT("shift at full speed does not latch m_bIsWalking immediately"), Fx.State.m_bIsWalking);

	// It engages once the player slows to within 25 u/s of 260 * 0.52 = 135.2
	for (int32 i = 0; i < 64 && !Fx.State.m_bIsWalking; ++i)
	{
		Fx.Step();
	}

	TestTrue(TEXT("m_bIsWalking latches once the player has decelerated"), Fx.State.m_bIsWalking);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcFootstepSlowMovementIsSilentTest, "SourceMovement.Footsteps.BelowThresholdIsSilent", SrcStepTestFlags)

bool FSrcFootstepSlowMovementIsSilentTest::RunTest(const FString&)
{
	// The other half of the CS gate: below 135.2 u/s there are no footsteps even without +speed
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	Fx.State.Velocity = FSrcVec3(CSWalkSpeedThreshold - 1.f, 0.f, 0.f);
	Fx.Step();
	TestFalse(TEXT("just under the threshold is silent"), Fx.Sim.GetEvents().bStepSound);
	Fx.State.m_flStepSoundTime = 0.f;
	Fx.State.Velocity = FSrcVec3(CSWalkSpeedThreshold + 20.f, 0.f, 0.f);
	Fx.Step();
	TestTrue(TEXT("just over the threshold is audible"), Fx.Sim.GetEvents().bStepSound);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcFootstepStoppedResetsTimerTest, "SourceMovement.Footsteps.StoppingResetsTheTimer", SrcStepTestFlags)

bool FSrcFootstepStoppedResetsTimerTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	Fx.State.m_flStepSoundTime = 0.f;
	Fx.State.Velocity = FSrcVec3::Zero;
	Fx.StepIdle();

	TestTrue(TEXT("standing still re-arms the step timer to the running interval"), SrcNearly(Fx.State.m_flStepSoundTime, 291.f, 1.e-2f));
	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
