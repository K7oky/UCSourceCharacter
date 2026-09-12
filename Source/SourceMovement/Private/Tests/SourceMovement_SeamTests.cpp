// Regression: walking across the seam between two separate floor brushes

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/SourceMovementTestFixture.h"

#include "Misc/AutomationTest.h"

namespace
{
	constexpr EAutomationTestFlags SrcSeamTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	void BuildSeamFloor(FSourceTestFixture& Fx, srcfloat TopZ = 0.f)
	{
		Fx.World.Reset();
		Fx.World.AddBox(FSrcVec3(-2048.f, -2048.f, TopZ - 512.f), FSrcVec3(0.f, 2048.f, TopZ));
		Fx.World.AddBox(FSrcVec3(0.f, -2048.f, TopZ - 512.f), FSrcVec3(2048.f, 2048.f, TopZ));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcSeamWalkAcrossTest, "SourceMovement.Seam.WalkAcrossFlushFloorJoin", SrcSeamTestFlags)

bool FSrcSeamWalkAcrossTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	BuildSeamFloor(Fx);
	Fx.SettleOnGround(0.f);
	Fx.State.Origin = FSrcVec3(-40.f, 0.f, FSourceMovementParams::DistEpsilon);
	Fx.State.Velocity = FSrcVec3::Zero;

	Fx.SetYaw(0.f);
	Fx.HoldForward();
	const srcfloat StartX = Fx.State.Origin.x;

	// 64 ticks = 1 s
	Fx.StepN(64);

	TestTrue(TEXT("the player crossed the seam instead of stopping at it"), Fx.State.Origin.x > 0.f);

	TestTrue(TEXT("the player kept moving"), Fx.State.Velocity.Length2D() > 100.f);

	TestTrue(TEXT("and covered a sensible distance"), (Fx.State.Origin.x - StartX) > 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcSeamDiagonalTest, "SourceMovement.Seam.DiagonalApproachDoesNotPin", SrcSeamTestFlags)

bool FSrcSeamDiagonalTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	BuildSeamFloor(Fx);
	Fx.SettleOnGround(0.f);
	Fx.State.Origin = FSrcVec3(-40.f, 40.f, FSourceMovementParams::DistEpsilon);
	Fx.State.Velocity = FSrcVec3::Zero;

	Fx.SetYaw(-41.f);  // roughly the wishdir from the bug report
	Fx.HoldForward();
	Fx.StepN(64);

	TestTrue(TEXT("a diagonal crossing does not pin the player either"), Fx.State.Origin.x > 0.f);

	TestTrue(TEXT("velocity was never zeroed out"), Fx.State.Velocity.Length2D() > 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcSeamNeverStallsTest, "SourceMovement.Seam.NeverStallsWhileAccelerating", SrcSeamTestFlags)

bool FSrcSeamNeverStallsTest::RunTest(const FString&)
{
	// The distinctive signature of the bug
	FSourceTestFixture Fx(64);
	BuildSeamFloor(Fx);
	Fx.SettleOnGround(0.f);
	Fx.State.Origin = FSrcVec3(-40.f, 0.f, FSourceMovementParams::DistEpsilon);
	Fx.State.Velocity = FSrcVec3::Zero;

	Fx.SetYaw(0.f);
	Fx.HoldForward();

	// One tick to get moving, so the check below is not tripped by the standing start
	Fx.Step();
	int32 StalledTicks = 0;
	for (int32 i = 0; i < 96; ++i)
	{
		const FSrcVec3 Before = Fx.State.Origin;
		Fx.Step();
		const srcfloat Moved = (Fx.State.Origin - Before).Length2D();
		if (Moved < 0.01f)
		{
			++StalledTicks;
		}
	}

	TestEqual(TEXT("no tick moved the player by nothing while accelerating"), StalledTicks, 0);
	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
