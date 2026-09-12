// Closed-form unit tests for the Source movement port

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/SourceMovementTestFixture.h"

#include "Misc/AutomationTest.h"

namespace
{
	constexpr EAutomationTestFlags SrcTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr srcfloat Dt64 = 1.0f / 64.0f;
	constexpr srcfloat HalfGravity64 = 800.0f * 0.5f * Dt64;  // 6.25
	constexpr srcfloat FullGravity64 = 800.0f * Dt64;  // 12.5
}

// Math layer

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcMathVectorNormalizeTest, "SourceMovement.Math.VectorNormalize", SrcTestFlags)

bool FSrcMathVectorNormalizeTest::RunTest(const FString&)
{
	{
		FSrcVec3 V(3.f, 4.f, 0.f);
		const srcfloat Len = SourceMath::VectorNormalize(V);

		TestTrue(TEXT("returns the original radius"), SrcNearly(Len, 5.0f, 1.e-6f));
		TestTrue(TEXT("x normalized"), SrcNearly(V.x, 0.6f, 1.e-6f));
		TestTrue(TEXT("y normalized"), SrcNearly(V.y, 0.8f, 1.e-6f));

		// The epsilon guard makes the result strictly shorter than 1, never longer
		TestTrue(TEXT("result is not longer than unit"), V.Length() <= 1.0f);
	}

	{
		FSrcVec3 Zero(0.f, 0.f, 0.f);
		const srcfloat Len = SourceMath::VectorNormalize(Zero);

		TestEqual(TEXT("zero vector radius"), Len, 0.0f);
		TestEqual(TEXT("zero vector stays zero (x)"), Zero.x, 0.0f);
		TestEqual(TEXT("zero vector stays zero (y)"), Zero.y, 0.0f);
		TestEqual(TEXT("zero vector stays zero (z)"), Zero.z, 0.0f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcMathAngleVectorsTest, "SourceMovement.Math.AngleVectors", SrcTestFlags)

bool FSrcMathAngleVectorsTest::RunTest(const FString&)
{
	{
		FSrcVec3 F, R, U;
		SourceMath::AngleVectors(FSrcAngles(0.f, 0.f, 0.f), &F, &R, &U);

		TestTrue(TEXT("yaw 0 forward is +X"), SrcNearly(F.x, 1.f, 1.e-6f) && SrcNearly(F.y, 0.f, 1.e-6f) && SrcNearly(F.z, 0.f, 1.e-6f));
		TestTrue(TEXT("yaw 0 right is -Y"), SrcNearly(R.x, 0.f, 1.e-6f) && SrcNearly(R.y, -1.f, 1.e-6f) && SrcNearly(R.z, 0.f, 1.e-6f));
		TestTrue(TEXT("yaw 0 up is +Z"), SrcNearly(U.z, 1.f, 1.e-6f));
	}

	// Yaw +90 turns LEFT in Source, so forward becomes +Y
	{
		FSrcVec3 F, R, U;
		SourceMath::AngleVectors(FSrcAngles(0.f, 90.f, 0.f), &F, &R, &U);

		TestTrue(TEXT("yaw 90 forward is +Y"), SrcNearly(F.x, 0.f, 1.e-5f) && SrcNearly(F.y, 1.f, 1.e-5f));
		TestTrue(TEXT("yaw 90 right is +X"), SrcNearly(R.x, 1.f, 1.e-5f) && SrcNearly(R.y, 0.f, 1.e-5f));
	}

	// Pitch is positive DOWN in Source
	{
		FSrcVec3 F;
		SourceMath::AngleVectors(FSrcAngles(90.f, 0.f, 0.f), &F);
		TestTrue(TEXT("pitch +90 looks down"), SrcNearly(F.z, -1.f, 1.e-5f));
	}

	return true;
}

// Gravity

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcGravityFreeFallTest, "SourceMovement.Gravity.FreeFall", SrcTestFlags)

bool FSrcGravityFreeFallTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.State.Origin = FSrcVec3(0.f, 0.f, 5000.f);
	Fx.State.Velocity = FSrcVec3::Zero;
	Fx.State.GroundEntity = FSrcEntityRef::None();
	const srcfloat StartZ = Fx.State.Origin.z;

	Fx.StepIdle();

	TestTrue(TEXT("one tick of free fall costs g*dt"), SrcNearly(Fx.State.Velocity.z, -FullGravity64, 1.e-4f));
	TestFalse(TEXT("still airborne"), Fx.State.IsOnGround());

	// 0.5 * 800 * (1/64)^2 = 0.09765625
	TestTrue(TEXT("first tick drop is 0.5*g*dt^2"), SrcNearly(StartZ - Fx.State.Origin.z, 0.09765625f, 1.e-5f));
	constexpr int32 N = 16;
	Fx.StepN(N - 1);

	TestTrue(TEXT("velocity after N ticks is -12.5*N"), SrcNearly(Fx.State.Velocity.z, -FullGravity64 * N, 1.e-3f));
	const srcfloat ExpectedDrop = 0.5f * 800.0f * (N * Dt64) * (N * Dt64);
	TestTrue(TEXT("drop after N ticks is 0.5*g*t^2"), SrcNearly(StartZ - Fx.State.Origin.z, ExpectedDrop, 1.e-2f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcGravityTickRate128Test, "SourceMovement.Gravity.TickRate128", SrcTestFlags)

bool FSrcGravityTickRate128Test::RunTest(const FString&)
{
	// Same derivation at 128 Hz: g*dt = 800/128 = 6.25 per tick
	FSourceTestFixture Fx(128);
	Fx.AddFlatGround(0.f);
	Fx.State.Origin = FSrcVec3(0.f, 0.f, 5000.f);
	Fx.State.GroundEntity = FSrcEntityRef::None();

	Fx.StepIdle();

	TestTrue(TEXT("one tick at 128 Hz costs 800/128"), SrcNearly(Fx.State.Velocity.z, -6.25f, 1.e-4f));
	return true;
}

// Ground contact

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcGroundSettleHeightTest, "SourceMovement.Ground.SettleHeight", SrcTestFlags)

bool FSrcGroundSettleHeightTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);

	TestTrue(TEXT("settles on ground"), Fx.SettleOnGround(0.f));

	TestTrue(TEXT("resting height is DIST_EPSILON above the surface"), SrcNearly(Fx.State.Origin.z, FSourceMovementParams::DistEpsilon, 1.e-5f));

	TestTrue(TEXT("grounded players have zero vertical velocity"), SrcNearly(Fx.State.Velocity.z, 0.f, 1.e-6f));

	TestTrue(TEXT("FL_ONGROUND is set"), Fx.State.HasFlag(SRC_FL_ONGROUND));

	// CategorizeGroundSurface: MIN( 0.8 * 1.25, 1.0 ) == 1.0 for the default material
	TestTrue(TEXT("surface friction resolves to 1.0"), SrcNearly(Fx.State.m_surfaceFriction, 1.0f, 1.e-6f));
	const srcfloat SettledZ = Fx.State.Origin.z;
	Fx.StepN(20);
	TestTrue(TEXT("idle ticks do not drift vertically"), SrcNearly(Fx.State.Origin.z, SettledZ, 1.e-6f));
	TestTrue(TEXT("still grounded after idling"), Fx.State.IsOnGround());
	return true;
}

// Friction

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcFrictionStopSpeedFloorTest, "SourceMovement.Friction.StopSpeedFloor", SrcTestFlags)

bool FSrcFrictionStopSpeedFloorTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	Fx.State.Velocity = FSrcVec3(50.f, 0.f, 0.f);
	Fx.StepIdle();

	TestTrue(TEXT("50 u/s loses a fixed 6.5 u/s"), SrcNearly(Fx.State.Velocity.x, 43.5f, 1.e-3f));
	TestTrue(TEXT("no lateral drift"), SrcNearly(Fx.State.Velocity.y, 0.f, 1.e-5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcFrictionProportionalTest, "SourceMovement.Friction.AboveStopSpeed", SrcTestFlags)

bool FSrcFrictionProportionalTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	Fx.State.Velocity = FSrcVec3(200.f, 0.f, 0.f);
	Fx.StepIdle();

	TestTrue(TEXT("200 u/s loses 16.25 u/s"), SrcNearly(Fx.State.Velocity.x, 183.75f, 1.e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcFrictionThresholdTest, "SourceMovement.Friction.BelowThresholdLeavesVelocityUntouched", SrcTestFlags)

bool FSrcFrictionThresholdTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	Fx.State.Velocity = FSrcVec3(0.05f, 0.f, 0.f);
	FSourceMoveData& Move = Fx.BindStageContext();
	Fx.Sim.Test_Friction();

	TestEqual(TEXT("velocity below 0.1 u/s is untouched"), Move.Velocity.x, 0.05f);

	// And just above the threshold it IS reduced, by the stop-speed floor amount
	Fx.State.Velocity = FSrcVec3(0.2f, 0.f, 0.f);
	FSourceMoveData& Move2 = Fx.BindStageContext();
	Fx.Sim.Test_Friction();

	// Drop = 6.5 > 0.2, so newspeed clamps to 0 and the velocity is scaled to zero
	TestTrue(TEXT("velocity above 0.1 u/s is bled to zero by the 6.5 u/s drop"), SrcNearly(Move2.Velocity.x, 0.f, 1.e-6f));
	return true;
}

// Ground acceleration

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcGroundAccelFromRestTest, "SourceMovement.GroundAccel.FromRest", SrcTestFlags)

bool FSrcGroundAccelFromRestTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);

	Fx.HoldForward();
	Fx.Step();

	TestTrue(TEXT("wishspeed is clamped to the player max speed"), SrcNearly(Fx.Sim.GetTelemetry().WishSpeed, 260.f, 1.e-2f));

	TestTrue(TEXT("first tick of ground acceleration adds 22.34375 u/s"), SrcNearly(Fx.State.Velocity.x, 22.34375f, 1.e-3f));
	TestTrue(TEXT("no lateral component from pure forward input"), SrcNearly(Fx.State.Velocity.y, 0.f, 1.e-4f));
	TestTrue(TEXT("grounded players keep zero vertical velocity"), SrcNearly(Fx.State.Velocity.z, 0.f, 1.e-6f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcGroundAccelTickRate128Test, "SourceMovement.GroundAccel.TickRate128", SrcTestFlags)

bool FSrcGroundAccelTickRate128Test::RunTest(const FString&)
{
	// 5.5 * (1/128) * 260 = 11.171875
	FSourceTestFixture Fx(128);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);

	Fx.HoldForward();
	Fx.Step();

	TestTrue(TEXT("128 Hz ground acceleration is 11.171875 u/s"), SrcNearly(Fx.State.Velocity.x, 11.171875f, 1.e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcGroundTerminalSpeedTest, "SourceMovement.GroundAccel.TerminalSpeed", SrcTestFlags)

bool FSrcGroundTerminalSpeedTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);

	Fx.HoldForward();
	Fx.StepN(200);

	TestTrue(TEXT("terminal ground speed is the max speed"), SrcNearly(Fx.State.Velocity.Length2D(), 260.f, 1.e-2f));
	TestTrue(TEXT("ground speed never exceeds max speed"), Fx.State.Velocity.Length2D() <= 260.f + 1.e-2f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcInputDiagonalClampTest, "SourceMovement.Input.DiagonalIsNotFaster", SrcTestFlags)

bool FSrcInputDiagonalClampTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);

	Fx.HoldForward();
	Fx.HoldRight();
	Fx.Step();
	const FSourceMovementTelemetry& T = Fx.Sim.GetTelemetry();

	TestTrue(TEXT("diagonal wish speed is still 260"), SrcNearly(T.WishSpeed, 260.f, 1.e-2f));

	const srcfloat Component = 260.0f / FMath::Sqrt(2.0f);  // 183.8477631
	TestTrue(TEXT("forward component is 260/sqrt(2)"), SrcNearly(T.WishVel.x, Component, 1.e-2f));

	// Right is -Y in Source's basis
	TestTrue(TEXT("right component is -260/sqrt(2)"), SrcNearly(T.WishVel.y, -Component, 1.e-2f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcInputUpMoveStealsBudgetTest, "SourceMovement.Input.UpMoveStealsHorizontalBudget", SrcTestFlags)

bool FSrcInputUpMoveStealsBudgetTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);

	Fx.HoldForward();
	Fx.Input.UpMove = 1.f;  // cl_upspeed 320
	Fx.Step();

	TestTrue(TEXT("upmove reduces the horizontal wish speed"), SrcNearly(Fx.Sim.GetTelemetry().WishSpeed, 211.8977f, 1.e-1f));
	TestTrue(TEXT("and it is strictly below the 260 it would otherwise be"), Fx.Sim.GetTelemetry().WishSpeed < 259.f);
	return true;
}

// Air acceleration

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcAirAccelBudgetTest, "SourceMovement.AirAccel.BudgetIsThirty", SrcTestFlags)

bool FSrcAirAccelBudgetTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.State.Origin = FSrcVec3(0.f, 0.f, 5000.f);
	Fx.State.Velocity = FSrcVec3(0.f, 0.f, -100.f);  // falling, so surfaceFriction stays 1.0
	Fx.State.GroundEntity = FSrcEntityRef::None();
	Fx.State.m_surfaceFriction = 1.0f;

	Fx.HoldForward();
	Fx.Step();

	TestTrue(TEXT("airborne gain from rest is the 30 u/s budget, not 48.75"), SrcNearly(Fx.State.Velocity.x, 30.f, 1.e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcAirAccelRisingTest, "SourceMovement.AirAccel.QuarteredWhileRisingSlowly", SrcTestFlags)

bool FSrcAirAccelRisingTest::RunTest(const FString&)
{
	// With the precise window
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.State.Origin = FSrcVec3(0.f, 0.f, 5000.f);
	Fx.State.GroundEntity = FSrcEntityRef::None();

	// Rising slowly: below the 140 u/s "definitely jumping" threshold
	Fx.State.Velocity = FSrcVec3(0.f, 0.f, 100.f);

	Fx.StepIdle();

	TestTrue(TEXT("rising slowly quarters the surface friction"), SrcNearly(Fx.State.m_surfaceFriction, 0.25f, 1.e-6f));
	const srcfloat BeforeX = Fx.State.Velocity.x;

	Fx.HoldForward();
	Fx.Step();

	TestTrue(TEXT("air acceleration is quartered to 12.1875 u/s"), SrcNearly(Fx.State.Velocity.x - BeforeX, 12.1875f, 1.e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcAirAccelFastRiseNotQuarteredTest, "SourceMovement.AirAccel.FastRiseKeepsFullFriction", SrcTestFlags)

bool FSrcAirAccelFastRiseNotQuarteredTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.State.Origin = FSrcVec3(0.f, 0.f, 5000.f);
	Fx.State.GroundEntity = FSrcEntityRef::None();
	Fx.State.Velocity = FSrcVec3(0.f, 0.f, 250.f);  // > 140

	Fx.StepIdle();

	TestTrue(TEXT("rising fast leaves surface friction at 1.0"), SrcNearly(Fx.State.m_surfaceFriction, 1.0f, 1.e-6f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcAirAccelNoGainStraightAheadTest, "SourceMovement.AirAccel.NoGainWhenAlreadyFastAlongWishDir", SrcTestFlags)

bool FSrcAirAccelNoGainStraightAheadTest::RunTest(const FString&)
{
	// The flip side of the 30 u/s budget
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.State.Origin = FSrcVec3(0.f, 0.f, 5000.f);
	Fx.State.Velocity = FSrcVec3(260.f, 0.f, -100.f);
	Fx.State.GroundEntity = FSrcEntityRef::None();
	Fx.State.m_surfaceFriction = 1.0f;

	Fx.SetYaw(0.f);
	Fx.HoldForward();
	Fx.Step();

	TestTrue(TEXT("holding forward at 260 u/s adds nothing"), SrcNearly(Fx.State.Velocity.x, 260.f, 1.e-4f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcAirStrafeGainsSpeedTest, "SourceMovement.AirAccel.AirStrafeGainsSpeed", SrcTestFlags)

bool FSrcAirStrafeGainsSpeedTest::RunTest(const FString&)
{
	// The canonical air-strafe, in its exactly-solvable form
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.State.Origin = FSrcVec3(0.f, 0.f, 5000.f);
	Fx.State.Velocity = FSrcVec3(260.f, 0.f, -100.f);
	Fx.State.GroundEntity = FSrcEntityRef::None();
	Fx.State.m_surfaceFriction = 1.0f;

	Fx.HoldLeft();
	constexpr int32 N = 60;
	srcfloat PrevSpeed = Fx.State.Velocity.Length2D();
	bool bMonotonic = true;

	for (int32 i = 0; i < N; ++i)
	{
		// Keep the wish direction perpendicular to the current velocity
		Fx.SetYaw(Fx.GetVelocityYawDegrees());
		Fx.Step();
		const srcfloat Speed = Fx.State.Velocity.Length2D();
		if (Speed <= PrevSpeed)
		{
			bMonotonic = false;
		}
		PrevSpeed = Speed;
	}

	TestTrue(TEXT("horizontal speed increases every tick"), bMonotonic);

	const srcfloat Expected = FMath::Sqrt(260.f * 260.f + 900.f * N);  // ~348.71
	TestTrue(TEXT("speed follows sqrt(v0^2 + 900*N)"), SrcNearly(Fx.State.Velocity.Length2D(), Expected, 0.5f));

	TestTrue(TEXT("still airborne for the whole run"), !Fx.State.IsOnGround());
	return true;
}

// Jump

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcJumpStandingTest, "SourceMovement.Jump.StandingFromRest", SrcTestFlags)

bool FSrcJumpStandingTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	Fx.Input.bJump = true;
	Fx.Step();

	TestFalse(TEXT("left the ground"), Fx.State.IsOnGround());

	TestTrue(TEXT("jump tick loses 1.5 * g * dt, not 1.0"), SrcNearly(Fx.State.Velocity.z, 283.243377f, 1.e-3f));

	TestTrue(TEXT("stamina cost is computed from the pre-FinishGravity delta"), SrcNearly(Fx.State.m_flStamina, 23.65947016f, 1.e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcJumpPogoBlockedTest, "SourceMovement.Jump.PogoBlockedWhileHeld", SrcTestFlags)

bool FSrcJumpPogoBlockedTest::RunTest(const FString&)
{
	// Holding jump does not re-jump unless sv_autobunnyhopping is on
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	Fx.Input.bJump = true;
	Fx.StepN(120);  // full jump arc plus landing, jump held the entire time

	TestTrue(TEXT("landed"), Fx.State.IsOnGround());
	TestTrue(TEXT("did not re-jump while the button was held"), SrcNearly(Fx.State.Velocity.z, 0.f, 1.e-4f));
	Fx.Input.bJump = false;
	Fx.Step();
	TestTrue(TEXT("still grounded after releasing jump"), Fx.State.IsOnGround());
	Fx.Input.bJump = true;
	Fx.Step();
	TestFalse(TEXT("re-pressing jump leaves the ground again"), Fx.State.IsOnGround());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcJumpDuckedOverwritesTest, "SourceMovement.Jump.DuckedOverwritesVerticalVelocity", SrcTestFlags)

bool FSrcJumpDuckedOverwritesTest::RunTest(const FString&)
{
	srcfloat StandingZ = 0.f;
	srcfloat DuckedZ = 0.f;

	{
		FSourceTestFixture Fx(64);
		Fx.AddFlatGround(0.f);
		Fx.SettleOnGround(0.f);
		Fx.State.Velocity = FSrcVec3(0.f, 0.f, 100.f);
		Fx.Input.bJump = true;
		Fx.Step();
		StandingZ = Fx.State.Velocity.z;
	}

	{
		FSourceTestFixture Fx(64);
		Fx.AddFlatGround(0.f);
		Fx.SettleOnGround(0.f);

		// Settled standing first, then flipped to the crouch stance
		Fx.State.m_bDucked = true;
		Fx.State.m_bDucking = false;
		Fx.State.m_flDuckAmount = 1.0f;
		Fx.State.AddFlag(SRC_FL_DUCKING | SRC_FL_ANIMDUCKING);
		Fx.State.Velocity = FSrcVec3(0.f, 0.f, 100.f);

		// Held, so Duck sees button-down against m_flDuckAmount == 1 and leaves the stance alone
		Fx.Input.bDuck = true;
		Fx.Input.bJump = true;
		Fx.Step();
		DuckedZ = Fx.State.Velocity.z;
	}

	TestTrue(TEXT("standing jump adds to existing upward velocity"), SrcNearly(StandingZ, 383.243377f, 1.e-2f));
	TestTrue(TEXT("ducked jump overwrites it"), SrcNearly(DuckedZ, 289.493377f, 1.e-2f));
	TestTrue(TEXT("the difference is exactly the discarded velocity"), SrcNearly(StandingZ - DuckedZ, 93.75f, 1.e-2f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcJumpStaminaTest, "SourceMovement.Jump.StaminaReducesHeight", SrcTestFlags)

bool FSrcJumpStaminaTest::RunTest(const FString&)
{
	// Derivation: stamina scales velocity.z AFTER the impulse, linearly
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	Fx.State.m_flStamina = 50.f;
	Fx.Input.bJump = true;
	Fx.Step();

	TestTrue(TEXT("stamina scales the jump velocity, after one tick of recovery"), SrcNearly(Fx.State.Velocity.z, 138.1442827f, 1.e-2f));

	{
		FSourceTestFixture Quiet(64);
		Quiet.AddFlatGround(0.f);
		Quiet.SettleOnGround(0.f);
		Quiet.State.m_flStamina = 50.f;
		Quiet.Step();

		TestTrue(TEXT("ReduceTimers drains frametime * sv_staminarecoveryrate per tick"), SrcNearly(Quiet.State.m_flStamina, 50.f - 60.f / 64.f, 1.e-4f));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcStaminaSquaredSpeedPenaltyTest, "SourceMovement.Jump.StaminaSpeedPenaltyIsSquared", SrcTestFlags)

bool FSrcStaminaSquaredSpeedPenaltyTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	Fx.State.m_flStamina = 50.f;
	Fx.HoldForward();
	Fx.Step();

	TestTrue(TEXT("max speed is scaled by the SQUARE of the stamina factor"), SrcNearly(Fx.Sim.GetTelemetry().MaxSpeed, 65.f, 1.e-1f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcPreventBunnyJumpingTest, "SourceMovement.Jump.PreventBunnyJumpingCap", SrcTestFlags)

bool FSrcPreventBunnyJumpingTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	Fx.State.Velocity = FSrcVec3(400.f, 0.f, 0.f);
	FSourceMoveData& Move = Fx.BindStageContext();
	Fx.Sim.Test_PreventBunnyJumping();

	TestTrue(TEXT("speed is capped at 1.1 * 260"), SrcNearly(Move.Velocity.Length(), 286.f, 1.e-3f));
	TestTrue(TEXT("direction is preserved"), SrcNearly(Move.Velocity.y, 0.f, 1.e-5f));

	// Below the cap nothing happens
	Fx.State.Velocity = FSrcVec3(200.f, 0.f, 0.f);
	FSourceMoveData& Move2 = Fx.BindStageContext();
	Fx.Sim.Test_PreventBunnyJumping();
	TestTrue(TEXT("below the cap the velocity is untouched"), SrcNearly(Move2.Velocity.x, 200.f, 1.e-6f));
	return true;
}

// Velocity clamping and collision response primitives

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcCheckVelocityPerAxisTest, "SourceMovement.CheckVelocity.ClampIsPerAxis", SrcTestFlags)

bool FSrcCheckVelocityPerAxisTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.State.Velocity = FSrcVec3(5000.f, -5000.f, 5000.f);
	FSourceMoveData& Move = Fx.BindStageContext();
	Fx.Sim.Test_CheckVelocity();

	TestTrue(TEXT("x clamped"), SrcNearly(Move.Velocity.x, 3500.f, 1.e-3f));
	TestTrue(TEXT("y clamped to the negative bound"), SrcNearly(Move.Velocity.y, -3500.f, 1.e-3f));
	TestTrue(TEXT("z clamped"), SrcNearly(Move.Velocity.z, 3500.f, 1.e-3f));

	TestTrue(TEXT("magnitude exceeds sv_maxvelocity because the clamp is per axis"), Move.Velocity.Length() > 6000.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcClipVelocityTest, "SourceMovement.ClipVelocity.PushOutAndBlockedFlags", SrcTestFlags)

bool FSrcClipVelocityTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.BindStageContext();

	// Basic projection: sliding along a floor removes the vertical component
	{
		FSrcVec3 Out;
		const int32 Blocked = Fx.Sim.Test_ClipVelocity(FSrcVec3(100.f, 0.f, -50.f), FSrcVec3(0.f, 0.f, 1.f), Out, 1.f);

		TestTrue(TEXT("horizontal velocity survives"), SrcNearly(Out.x, 100.f, 1.e-4f));
		TestTrue(TEXT("vertical velocity is removed"), SrcNearly(Out.z, 0.f, 1.e-4f));
		TestEqual(TEXT("floor flag set"), Blocked, 0x01);
	}
	{
		FSrcVec3 Out;
		const int32 Blocked = Fx.Sim.Test_ClipVelocity(FSrcVec3(100.f, 0.f, 0.f), FSrcVec3(-1.f, 0.f, 0.f), Out, 1.f);
		TestEqual(TEXT("a vertical wall reports the wall flag only"), Blocked, 0x02);
	}
	{
		// A 5-degree ramp is far from standable, yet ClipVelocity still calls it a floor
		FSrcVec3 Normal(0.0872f, 0.f, 0.9962f);
		FSrcVec3 Out;
		const int32 Blocked = Fx.Sim.Test_ClipVelocity(FSrcVec3(100.f, 0.f, 0.f), Normal, Out, 1.f);
		TestEqual(TEXT("any positive normal.z reports floor"), Blocked, 0x01);
	}
	{
		FSrcVec3 Out;
		Fx.Sim.Test_ClipVelocity(FSrcVec3(0.f, 0.f, -1.e-6f), FSrcVec3(0.f, 0.f, 1.f), Out, 0.f);
		const srcfloat Expected = -1.e-6f + FSourceMovementParams::DistEpsilon;
		TestTrue(TEXT("push-out is at least DIST_EPSILON, not the residual"), SrcNearly(Out.z, Expected, 1.e-6f));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcWalkMoveFreezeTest, "SourceMovement.WalkMove.FreezeBelowOneUnit", SrcTestFlags)

bool FSrcWalkMoveFreezeTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);
	const srcfloat StartX = Fx.State.Origin.x;
	Fx.State.Velocity = FSrcVec3(0.5f, 0.f, 0.f);
	Fx.StepIdle();

	TestTrue(TEXT("velocity is zeroed"), SrcNearly(Fx.State.Velocity.Length(), 0.f, 1.e-6f));
	TestTrue(TEXT("and the player did not move"), SrcNearly(Fx.State.Origin.x, StartX, 1.e-6f));
	return true;
}

// Determinism

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcDeterminismTest, "SourceMovement.Determinism.SameInputsSameResult", SrcTestFlags)

bool FSrcDeterminismTest::RunTest(const FString&)
{
	// Required by replay, client prediction and differential testing
	auto RunSequence = [](FSourceMovementState& OutState)
	{
		FSourceTestFixture Fx(64);
		Fx.AddFlatGround(0.f);
		Fx.SettleOnGround(0.f);

		for (int32 i = 0; i < 200; ++i)
		{
			// A deliberately messy but deterministic input pattern: turning, strafing, jumping
			Fx.SetYaw(static_cast<srcfloat>(i) * 3.7f);
			Fx.Input.ForwardMove = ((i / 7) % 2) ? 1.f : -1.f;
			Fx.Input.SideMove = ((i / 5) % 2) ? 1.f : -1.f;
			Fx.Input.bJump = ((i % 23) == 0);
			Fx.Input.bWalk = ((i / 11) % 2) != 0;
			Fx.Step();
		}

		OutState = Fx.State;
	};
	FSourceMovementState A, B;
	RunSequence(A);
	RunSequence(B);

	// Exact comparisons on purpose
	TestTrue(TEXT("origin is bit-identical"), A.Origin.x == B.Origin.x && A.Origin.y == B.Origin.y && A.Origin.z == B.Origin.z);
	TestTrue(TEXT("velocity is bit-identical"), A.Velocity.x == B.Velocity.x && A.Velocity.y == B.Velocity.y && A.Velocity.z == B.Velocity.z);
	TestTrue(TEXT("stamina is bit-identical"), A.m_flStamina == B.m_flStamina);
	TestTrue(TEXT("surface friction is bit-identical"), A.m_surfaceFriction == B.m_surfaceFriction);
	TestTrue(TEXT("ground state matches"), A.IsOnGround() == B.IsOnGround());
	TestTrue(TEXT("flags match"), A.Flags == B.Flags);
	TestEqual(TEXT("tick base"), A.m_nTickBase, B.m_nTickBase);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcInvariantsTest, "SourceMovement.Invariants.NeverInSolidAndBoundedSpeed", SrcTestFlags)

bool FSrcInvariantsTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);

	FRandomStream Rand(20260910);
	bool bNeverInSolid = true;
	bool bSpeedBounded = true;
	bool bGroundSpeedBounded = true;
	bool bStaminaBounded = true;

	for (int32 i = 0; i < 2000; ++i)
	{
		Fx.SetYaw(Rand.FRandRange(-180.f, 180.f));
		Fx.Input.ForwardMove = static_cast<srcfloat>(Rand.FRandRange(-1.f, 1.f));
		Fx.Input.SideMove = static_cast<srcfloat>(Rand.FRandRange(-1.f, 1.f));
		Fx.Input.bJump = Rand.FRand() < 0.1f;
		Fx.Input.bWalk = Rand.FRand() < 0.3f;
		Fx.Step();

		// Never left inside solid geometry: a zero-length hull test at the origin must be clear
		{
			FSourceTraceResult Probe;
			FSourceTraceFilter Filter;
			Filter.SkipEntityId = Fx.State.EntIndex;
			Fx.World.TraceHull(Fx.State.Origin, Fx.State.Origin, Fx.Params.Hulls.GetPlayerMins(Fx.State.m_bDucked), Fx.Params.Hulls.GetPlayerMaxs(Fx.State.m_bDucked), Fx.Params.PlayerSolidMask, Filter, Probe);
			if (Probe.bStartSolid || Probe.bAllSolid)
			{
				bNeverInSolid = false;
			}
		}

		// CheckVelocity's per-axis bound
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (FMath::Abs(Fx.State.Velocity[Axis]) > Fx.Params.MaxVelocity + 1.e-3f)
			{
				bSpeedBounded = false;
			}
		}

		// Ground speed can never exceed the per-tick max speed
		if (Fx.State.IsOnGround() && Fx.State.Velocity.Length2D() > Fx.Params.PlayerSpeedRun + 1.f)
		{
			bGroundSpeedBounded = false;
		}

		if (Fx.State.m_flStamina < 0.f || Fx.State.m_flStamina > Fx.Params.StaminaMax + 1.e-3f)
		{
			bStaminaBounded = false;
		}
	}

	TestTrue(TEXT("the player is never left inside solid geometry"), bNeverInSolid);
	TestTrue(TEXT("per-axis velocity stays within sv_maxvelocity"), bSpeedBounded);
	TestTrue(TEXT("ground speed never exceeds the max speed"), bGroundSpeedBounded);
	TestTrue(TEXT("stamina stays within [0, sv_staminamax]"), bStaminaBounded);
	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
