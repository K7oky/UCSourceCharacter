// Duck / unduck and crouch-jump tests

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/SourceMovementTestFixture.h"

#include "Misc/AutomationTest.h"

namespace
{
	constexpr EAutomationTestFlags SrcDuckTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	// Standing hull top minus ducked hull top: 72 - 54
	constexpr srcfloat HullHeightDelta = 18.0f;

	// CS:GO uses HALF of it for the airborne stance change
	constexpr srcfloat AirDuckOriginShift = HullHeightDelta * 0.5f;  // 9
}

// On the ground

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcDuckOnGroundKeepsOriginTest, "SourceMovement.Duck.OnGroundDoesNotMoveOrigin", SrcDuckTestFlags)

bool FSrcDuckOnGroundKeepsOriginTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	const srcfloat StartZ = Fx.State.Origin.z;
	Fx.Input.bDuck = true;
	Fx.StepN(60);  // far more than enough to complete the transition

	TestTrue(TEXT("ends up fully ducked"), Fx.State.m_bDucked);
	TestTrue(TEXT("duck amount saturates at 1"), SrcNearly(Fx.State.m_flDuckAmount, 1.f, 1.e-4f));
	TestTrue(TEXT("origin did not move"), SrcNearly(Fx.State.Origin.z, StartZ, 1.e-4f));
	TestTrue(TEXT("still grounded"), Fx.State.IsOnGround());
	TestTrue(TEXT("FL_DUCKING is set"), Fx.State.HasFlag(SRC_FL_DUCKING));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcDuckHullSwitchTest, "SourceMovement.Duck.HullSwitchesOnlyWhenFullyDucked", SrcDuckTestFlags)

bool FSrcDuckHullSwitchTest::RunTest(const FString&)
{
	// The hull switches on m_bDucked, and only FinishDuck sets it
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	Fx.Input.bDuck = true;
	bool bSawIntermediate = false;
	bool bHullEverIntermediate = false;

	for (int32 i = 0; i < 60 && !Fx.State.m_bDucked; ++i)
	{
		Fx.Step();

		if (Fx.State.m_flDuckAmount > 0.f && Fx.State.m_flDuckAmount < 1.f)
		{
			bSawIntermediate = true;

			// While mid-transition the hull must still be the standing one
			const FSrcVec3& Maxs = Fx.Params.Hulls.GetPlayerMaxs(Fx.State.m_bDucked);
			if (!SrcNearly(Maxs.z, 72.f, 1.e-4f))
			{
				bHullEverIntermediate = true;
			}
		}
	}

	TestTrue(TEXT("the duck really did interpolate rather than snapping"), bSawIntermediate);
	TestFalse(TEXT("the hull was never an intermediate height"), bHullEverIntermediate);
	TestTrue(TEXT("hull is the ducked one once fully ducked"), SrcNearly(Fx.Params.Hulls.GetPlayerMaxs(Fx.State.m_bDucked).z, 54.f, 1.e-4f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcDuckSpeedCropTest, "SourceMovement.Duck.SpeedCropScalesMaxSpeed", SrcDuckTestFlags)

bool FSrcDuckSpeedCropTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	Fx.Input.bDuck = true;
	Fx.StepN(60);
	TestTrue(TEXT("fully ducked"), Fx.State.m_bDucked);

	// One more tick so the telemetry reflects a steady fully-ducked state
	Fx.HoldForward();
	Fx.Step();

	TestTrue(TEXT("max speed is scaled by the duck modifier"), SrcNearly(Fx.Sim.GetTelemetry().MaxSpeed, 260.f * 0.34f, 1.e-1f));

	// And the terminal crouch-walk speed follows it
	Fx.StepN(120);
	TestTrue(TEXT("crouched terminal speed is 260 * 0.34"), SrcNearly(Fx.State.Velocity.Length2D(), 260.f * 0.34f, 1.5f));
	return true;
}

// Crouch-jump

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcDuckAirFinishesImmediatelyTest, "SourceMovement.Duck.CrouchJumpRaisesOriginByHalfHeightDelta", SrcDuckTestFlags)

bool FSrcDuckAirFinishesImmediatelyTest::RunTest(const FString&)
{
	// The two halves of the crouch-jump
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);

	// Airborne, mid-transition, duck held
	Fx.State.Origin = FSrcVec3(0.f, 0.f, 5000.f);
	Fx.State.GroundEntity = FSrcEntityRef::None();
	Fx.State.RemoveFlag(SRC_FL_ONGROUND);
	Fx.State.m_bDucking = true;
	Fx.State.m_flDuckAmount = 0.5f;
	Fx.Input.bDuck = true;
	FSourceMoveData& Move = Fx.BindStageContext();
	const srcfloat BeforeZ = Move.GetAbsOrigin().z;

	Fx.Sim.Test_Duck();

	TestTrue(TEXT("airborne duck completes immediately"), Fx.State.m_bDucked);
	TestTrue(TEXT("duck amount snapped to 1"), SrcNearly(Fx.State.m_flDuckAmount, 1.f, 1.e-5f));

	TestTrue(TEXT("origin rose by exactly half the hull height delta"), SrcNearly(Move.GetAbsOrigin().z - BeforeZ, AirDuckOriginShift, 1.e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcUnduckAirDropsOriginTest, "SourceMovement.Duck.UnduckInAirDropsOriginByHalfHeightDelta", SrcDuckTestFlags)

bool FSrcUnduckAirDropsOriginTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.State.Origin = FSrcVec3(0.f, 0.f, 5000.f);
	Fx.State.GroundEntity = FSrcEntityRef::None();
	Fx.State.RemoveFlag(SRC_FL_ONGROUND);
	Fx.State.m_bDucked = true;
	Fx.State.m_bDucking = true;
	Fx.State.m_flDuckAmount = 1.0f;
	Fx.State.AddFlag(SRC_FL_DUCKING | SRC_FL_ANIMDUCKING);
	Fx.Input.bDuck = false;
	FSourceMoveData& Move = Fx.BindStageContext();
	const srcfloat BeforeZ = Move.GetAbsOrigin().z;

	Fx.Sim.Test_Duck();

	TestFalse(TEXT("no longer ducked"), Fx.State.m_bDucked);
	TestTrue(TEXT("duck amount is zero"), SrcNearly(Fx.State.m_flDuckAmount, 0.f, 1.e-5f));

	TestTrue(TEXT("origin dropped by exactly half the hull height delta"), SrcNearly(BeforeZ - Move.GetAbsOrigin().z, AirDuckOriginShift, 1.e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcCrouchJumpReachesHigherTest, "SourceMovement.Duck.CrouchJumpClearsMoreThanPlainJump", SrcDuckTestFlags)

bool FSrcCrouchJumpReachesHigherTest::RunTest(const FString&)
{
	auto PeakOriginZ = [](bool bHoldDuck) -> srcfloat
	{
		FSourceTestFixture Fx(64);
		Fx.AddFlatGround(0.f);
		Fx.SettleOnGround(0.f);
		Fx.Input.bJump = true;
		Fx.Input.bDuck = bHoldDuck;
		srcfloat Peak = Fx.State.Origin.z;
		for (int32 i = 0; i < 40; ++i)
		{
			Fx.Step();
			Peak = FMath::Max(Peak, Fx.State.Origin.z);
		}
		return Peak;
	};
	const srcfloat PlainPeak = PeakOriginZ(false);
	const srcfloat CrouchPeak = PeakOriginZ(true);

	TestTrue(TEXT("a plain jump lifts the feet ~54.7 units, not the nominal 57"), PlainPeak > 52.f && PlainPeak < 58.f);

	TestTrue(TEXT("crouch-jumping gets the feet strictly higher"), CrouchPeak > PlainPeak);

	// The gain is the 9-unit stance shift plus the small extra from the assignment branch
	TestTrue(TEXT("the gain is at least the 9-unit stance shift"), (CrouchPeak - PlainPeak) >= AirDuckOriginShift - 0.5f);
	return true;
}

// Quirks

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcUnduckClearsDuckedEarlyTest, "SourceMovement.Duck.UnduckClearsDuckedFlagImmediately", SrcDuckTestFlags)

bool FSrcUnduckClearsDuckedEarlyTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	Fx.Input.bDuck = true;
	Fx.StepN(60);
	TestTrue(TEXT("fully ducked first"), Fx.State.m_bDucked);
	TestTrue(TEXT("FL_DUCKING set"), Fx.State.HasFlag(SRC_FL_DUCKING));
	Fx.Input.bDuck = false;
	Fx.Step();

	TestFalse(TEXT("m_bDucked cleared on the very first un-duck tick"), Fx.State.m_bDucked);
	TestTrue(TEXT("but the duck is still interpolating"), Fx.State.m_flDuckAmount > 0.f && Fx.State.m_flDuckAmount < 1.f);
	TestTrue(TEXT("so the collision hull is already the standing one"), SrcNearly(Fx.Params.Hulls.GetPlayerMaxs(Fx.State.m_bDucked).z, 72.f, 1.e-4f));
	TestTrue(TEXT("while FL_DUCKING still survives above 0.75"), Fx.State.m_flDuckAmount <= 0.75f || Fx.State.HasFlag(SRC_FL_DUCKING));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcUnduckBlockedTest, "SourceMovement.Duck.BlockedByCeilingSnapsBackToFullyDucked", SrcDuckTestFlags)

bool FSrcUnduckBlockedTest::RunTest(const FString&)
{
	// The `else` of `if (CanUnduck)`
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.World.AddBox(FSrcVec3(-1000.f, -1000.f, 60.f), FSrcVec3(1000.f, 1000.f, 100.f));
	Fx.SettleOnGround(0.f);
	Fx.Input.bDuck = true;
	Fx.StepN(60);
	TestTrue(TEXT("ducked under the ceiling"), Fx.State.m_bDucked);

	// Sanity: standing up genuinely is impossible here
	Fx.BindStageContext();
	TestFalse(TEXT("CanUnduck reports blocked"), Fx.Sim.Test_CanUnduck());
	Fx.Input.bDuck = false;
	Fx.StepN(30);

	TestTrue(TEXT("still fully ducked"), Fx.State.m_bDucked);
	TestTrue(TEXT("duck amount forced back to 1"), SrcNearly(Fx.State.m_flDuckAmount, 1.f, 1.e-4f));
	TestFalse(TEXT("transition flag cleared so it stops retrying"), Fx.State.m_bDucking);
	TestTrue(TEXT("FL_DUCKING re-added"), Fx.State.HasFlag(SRC_FL_DUCKING));
	TestTrue(TEXT("FL_ANIMDUCKING re-added"), Fx.State.HasFlag(SRC_FL_ANIMDUCKING));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcDuckSpamPenaltyTest, "SourceMovement.Duck.SpamPenaltyDisablesDuckKey", SrcDuckTestFlags)

bool FSrcDuckSpamPenaltyTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);

	TestTrue(TEXT("starts at the ideal duck speed"), SrcNearly(Fx.State.m_flDuckSpeed, 8.0f, 1.e-3f));

	// Toggle duck every tick: two edges per two ticks, 2.0 each, against 3.0/sec recovery
	for (int32 i = 0; i < 12; ++i)
	{
		Fx.Input.bDuck = (i % 2) == 0;
		Fx.Step();
	}

	TestTrue(TEXT("spamming drained the duck-speed budget below the 1.5 floor"), Fx.State.m_flDuckSpeed < 1.5f);

	// With the budget drained, holding duck does nothing at all
	const srcfloat AmountBefore = Fx.State.m_flDuckAmount;
	Fx.Input.bDuck = true;
	Fx.StepN(5);

	TestTrue(TEXT("holding duck no longer increases the duck amount"), Fx.State.m_flDuckAmount <= AmountBefore + 1.e-4f);

	// It recovers at 3/sec, so after ~1 second the key works again
	Fx.Input.bDuck = false;
	Fx.StepN(80);
	TestTrue(TEXT("the budget recovers over time"), Fx.State.m_flDuckSpeed > 1.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcDuckJumpOverwriteTest, "SourceMovement.Duck.JumpWhileDuckingUsesOverwriteBranch", SrcDuckTestFlags)

bool FSrcDuckJumpOverwriteTest::RunTest(const FString&)
{
	srcfloat StandingZ = 0.f;
	srcfloat DuckedZ = 0.f;

	{
		FSourceTestFixture Fx(64);
		Fx.AddFlatGround(0.f);
		Fx.SettleOnGround(0.f);
		Fx.Input.bJump = true;
		Fx.Step();
		StandingZ = Fx.State.Velocity.z;
	}

	{
		FSourceTestFixture Fx(64);
		Fx.AddFlatGround(0.f);
		Fx.SettleOnGround(0.f);
		Fx.Input.bJump = true;
		Fx.Input.bDuck = true;
		Fx.Step();
		DuckedZ = Fx.State.Velocity.z;
	}

	TestTrue(TEXT("standing jump adds to the post-StartGravity velocity"), SrcNearly(StandingZ, 283.243377f, 1.e-2f));
	TestTrue(TEXT("crouch jump assigns instead, keeping the extra 6.25"), SrcNearly(DuckedZ, 289.493377f, 1.e-2f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcDuckEyeOffsetTest, "SourceMovement.Duck.EyeOffsetFollowsSimpleSpline", SrcDuckTestFlags)

bool FSrcDuckEyeOffsetTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);

	TestTrue(TEXT("standing eye height is 64"), SrcNearly(Fx.State.ViewOffset.z, 64.f, 1.e-3f));
	Fx.Input.bDuck = true;

	// Walk the transition and check the eye against the closed form at every tick
	bool bEyeMatchesSpline = true;
	for (int32 i = 0; i < 60 && !Fx.State.m_bDucked; ++i)
	{
		Fx.Step();

		if (Fx.State.m_bDucked)
		{
			break;
		}

		const srcfloat f = SourceMath::SimpleSpline(Fx.State.m_flDuckAmount);
		const srcfloat Expected = 46.f * f + 64.f * (1.f - f);

		if (!SrcNearly(Fx.State.ViewOffset.z, Expected, 1.e-2f))
		{
			bEyeMatchesSpline = false;
		}
	}

	TestTrue(TEXT("eye height follows SimpleSpline throughout the transition"), bEyeMatchesSpline);
	TestTrue(TEXT("ducked eye height is 46"), SrcNearly(Fx.State.ViewOffset.z, 46.f, 1.e-3f));
	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
