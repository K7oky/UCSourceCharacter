// The matchmaking speed lock: PreventBunnyJumping and the sv_enablebunnyhopping switch

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/SourceMovementTestFixture.h"

#include "Misc/AutomationTest.h"

namespace
{
	constexpr EAutomationTestFlags SrcLockTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	// The speed factor times the latched run speed: 1.1 * 260
	constexpr srcfloat MatchmakingSpeedCap = 286.0f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcSpeedLockEnabledTest, "SourceMovement.SpeedLock.LockedByDefaultCapsJumpSpeed", SrcLockTestFlags)

bool FSrcSpeedLockEnabledTest::RunTest(const FString&)
{
	// sv_enablebunnyhopping 0 is the matchmaking default
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);

	TestFalse(TEXT("bunny hopping is locked by default"), Fx.Params.bEnableBunnyHopping);
	Fx.State.Velocity = FSrcVec3(400.f, 0.f, 0.f);
	Fx.Input.bJump = true;
	Fx.Step();

	TestFalse(TEXT("left the ground"), Fx.State.IsOnGround());
	TestTrue(TEXT("horizontal speed was clamped to 1.1 * 260"), SrcNearly(Fx.State.Velocity.Length2D(), MatchmakingSpeedCap, 0.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcSpeedLockDisabledTest, "SourceMovement.SpeedLock.UnlockedPreservesSpeed", SrcLockTestFlags)

bool FSrcSpeedLockDisabledTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	Fx.Params.bEnableBunnyHopping = true;
	Fx.State.Velocity = FSrcVec3(400.f, 0.f, 0.f);
	Fx.Input.bJump = true;
	Fx.Step();

	TestFalse(TEXT("left the ground"), Fx.State.IsOnGround());
	TestTrue(TEXT("horizontal speed is preserved"), SrcNearly(Fx.State.Velocity.Length2D(), 400.f, 0.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcSpeedLockFactorTest, "SourceMovement.SpeedLock.FactorIsConfigurable", SrcLockTestFlags)

bool FSrcSpeedLockFactorTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	Fx.Params.BunnyJumpMaxSpeedFactor = 1.5f;
	Fx.State.Velocity = FSrcVec3(500.f, 0.f, 0.f);
	Fx.Input.bJump = true;
	Fx.Step();

	TestTrue(TEXT("cap follows the factor: 1.5 * 260 = 390"), SrcNearly(Fx.State.Velocity.Length2D(), 390.f, 0.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcSpeedLockUsesLatchedMaxSpeedTest, "SourceMovement.SpeedLock.UsesLatchedMaxSpeedNotPerTick", SrcLockTestFlags)

bool FSrcSpeedLockUsesLatchedMaxSpeedTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	Fx.State.m_flStamina = 50.f;
	Fx.State.Velocity = FSrcVec3(400.f, 0.f, 0.f);
	Fx.Input.bJump = true;
	Fx.Step();

	TestTrue(TEXT("the per-tick max speed really was scaled down by stamina"), SrcNearly(Fx.Sim.GetTelemetry().MaxSpeed, 65.f, 1.f));

	TestTrue(TEXT("but the lock still used the latched 260"), SrcNearly(Fx.State.Velocity.Length2D(), MatchmakingSpeedCap, 0.5f));

	TestFalse(TEXT("and definitely not the stamina-scaled 71.5"), SrcNearly(Fx.State.Velocity.Length2D(), 71.5f, 5.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcSpeedLockBelowCapTest, "SourceMovement.SpeedLock.BelowCapIsUntouched", SrcLockTestFlags)

bool FSrcSpeedLockBelowCapTest::RunTest(const FString&)
{
	// The lock is a clamp, not a set
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	Fx.State.Velocity = FSrcVec3(200.f, 0.f, 0.f);
	Fx.Input.bJump = true;
	Fx.Step();

	TestTrue(TEXT("speed below the cap passes through unchanged"), SrcNearly(Fx.State.Velocity.Length2D(), 200.f, 0.1f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcSpeedLockGroundCapStillAppliesTest, "SourceMovement.SpeedLock.GroundCapIsIndependentOfBunnyHopping", SrcLockTestFlags)

bool FSrcSpeedLockGroundCapStillAppliesTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	Fx.Params.bEnableBunnyHopping = true;
	Fx.State.Velocity = FSrcVec3(400.f, 0.f, 0.f);

	Fx.StepIdle();

	TestTrue(TEXT("still grounded"), Fx.State.IsOnGround());
	TestTrue(TEXT("the grounded clamp pulled speed down to the running speed regardless"), Fx.State.Velocity.Length2D() <= 260.f + 0.1f);
	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
