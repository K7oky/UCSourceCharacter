#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/SourceMovementTestFixture.h"

#include "Misc/AutomationTest.h"

namespace
{
	constexpr EAutomationTestFlags SrcNoClipTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcNoClipPassesThroughSolidTest, "SourceMovement.NoClip.PassesThroughSolid", SrcNoClipTestFlags)

bool FSrcNoClipPassesThroughSolidTest::RunTest(const FString&)
{
	// FullNoClipMove just adds velocity to the origin
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);

	// A solid slab spanning x in [100, 200], directly ahead
	Fx.World.AddBox(FSrcVec3(100.f, -512.f, -64.f), FSrcVec3(200.f, 512.f, 512.f));
	Fx.State.Origin = FSrcVec3(0.f, 0.f, 64.f);
	Fx.State.Velocity = FSrcVec3::Zero;
	Fx.State.MoveType = ESrcMoveType::NoClip;

	Fx.SetYaw(0.f);
	Fx.HoldForward();
	Fx.StepN(32);

	TestTrue(TEXT("noclip travelled straight through the slab"), Fx.State.Origin.x > 250.f);
	TestTrue(TEXT("and is still in noclip"), Fx.State.MoveType == ESrcMoveType::NoClip);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcNoClipIgnoresGravityTest, "SourceMovement.NoClip.IgnoresGravity", SrcNoClipTestFlags)

bool FSrcNoClipIgnoresGravityTest::RunTest(const FString&)
{
	// Gravity lives in FullWalkMove, which noclip never reaches
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.State.Origin = FSrcVec3(0.f, 0.f, 500.f);
	Fx.State.Velocity = FSrcVec3::Zero;
	Fx.State.MoveType = ESrcMoveType::NoClip;

	Fx.StepN(64);

	TestTrue(TEXT("no vertical drift with no input"), SrcNearly(Fx.State.Origin.z, 500.f, 1.e-3f));
	TestTrue(TEXT("velocity stayed zero"), SrcNearly(Fx.State.Velocity.Length(), 0.f, 1.e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcNoClipUpMoveTest, "SourceMovement.NoClip.UpMoveFlies", SrcNoClipTestFlags)

bool FSrcNoClipUpMoveTest::RunTest(const FString&)
{
	// Upmove ADDS to the vertical the look direction already gave (`+=`, not `=`)
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.State.Origin = FSrcVec3(0.f, 0.f, 100.f);
	Fx.State.Velocity = FSrcVec3::Zero;
	Fx.State.MoveType = ESrcMoveType::NoClip;
	Fx.Input.UpMove = 1.f;
	Fx.StepN(32);

	TestTrue(TEXT("+moveup climbs in noclip"), Fx.State.Origin.z > 200.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcNoClipNoAccelBranchesTest, "SourceMovement.NoClip.NonPositiveAccelerationBranches", SrcNoClipTestFlags)

bool FSrcNoClipNoAccelBranchesTest::RunTest(const FString&)
{
	// Accel == 0 keeps the velocity, accel < 0 zeroes it after the move
	{
		FSourceTestFixture Fx(64);
		Fx.AddFlatGround(0.f);
		Fx.Params.NoClipAccelerate = 0.f;
		Fx.State.Origin = FSrcVec3(0.f, 0.f, 100.f);
		Fx.State.Velocity = FSrcVec3::Zero;
		Fx.State.MoveType = ESrcMoveType::NoClip;

		Fx.SetYaw(0.f);
		Fx.HoldForward();
		Fx.Step();

		TestTrue(TEXT("accel == 0: the player moved"), Fx.State.Origin.x > 1.f);
		TestTrue(TEXT("accel == 0: velocity is KEPT"), Fx.State.Velocity.Length() > 1.f);
	}

	{
		FSourceTestFixture Fx(64);
		Fx.AddFlatGround(0.f);
		Fx.Params.NoClipAccelerate = -1.f;
		Fx.State.Origin = FSrcVec3(0.f, 0.f, 100.f);
		Fx.State.Velocity = FSrcVec3::Zero;
		Fx.State.MoveType = ESrcMoveType::NoClip;

		Fx.SetYaw(0.f);
		Fx.HoldForward();
		Fx.Step();

		TestTrue(TEXT("accel < 0: the player still moved"), Fx.State.Origin.x > 1.f);
		TestTrue(TEXT("accel < 0: velocity is zeroed afterwards"), SrcNearly(Fx.State.Velocity.Length(), 0.f, 1.e-3f));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcNoClipSpeedIsAMultiplierTest, "SourceMovement.NoClip.SpeedIsAMultiplierOnMaxSpeed", SrcNoClipTestFlags)

bool FSrcNoClipSpeedIsAMultiplierTest::RunTest(const FString&)
{
	// sv_noclipspeed is a factor on sv_maxspeed, so double it and the distance doubles
	auto TravelWithFactor = [](srcfloat Factor)
	{
		FSourceTestFixture Fx(64);
		Fx.AddFlatGround(0.f);
		Fx.Params.NoClipSpeed = Factor;
		Fx.Params.NoClipAccelerate = 0.f;  // assignment path, so the result is not friction-shaped

		Fx.State.Origin = FSrcVec3(0.f, 0.f, 100.f);
		Fx.State.Velocity = FSrcVec3::Zero;
		Fx.State.MoveType = ESrcMoveType::NoClip;

		Fx.SetYaw(0.f);
		Fx.HoldForward();
		Fx.StepN(16);
		return Fx.State.Origin.x;
	};
	const srcfloat Slow = TravelWithFactor(1.f);
	const srcfloat Fast = TravelWithFactor(2.f);

	TestTrue(TEXT("both factors moved the player"), Slow > 1.f);
	TestTrue(TEXT("doubling sv_noclipspeed roughly doubles the distance"), SrcNearly(Fast / Slow, 2.0f, 0.05f));
	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
