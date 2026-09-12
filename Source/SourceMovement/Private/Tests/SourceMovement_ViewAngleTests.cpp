// View-angle / units-conversion contract tests

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/SourceMovementTestFixture.h"

#include "Misc/AutomationTest.h"
#include "SourceUnits.h"

namespace
{
	constexpr EAutomationTestFlags SrcViewTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcAngleRoundTripTest, "SourceMovement.View.RotatorRoundTrip", SrcViewTestFlags)

bool FSrcAngleRoundTripTest::RunTest(const FString&)
{
	// ToUERotator o ToSrcAngles must be the identity
	const FRotator Cases[] = {
		FRotator(0.f, 0.f, 0.f), FRotator(0.f, 90.f, 0.f), FRotator(0.f, 180.f, 0.f), FRotator(0.f, -90.f, 0.f),
		FRotator(30.f, 45.f, 0.f), FRotator(-30.f, -135.f, 0.f), };

	for (const FRotator& R : Cases)
	{
		const FRotator Back = SourceUnits::ToUERotator(SourceUnits::ToSrcAngles(R));

		TestTrue(*FString::Printf(TEXT("round trip preserves %s"), *R.ToCompactString()), Back.Equals(R, 1.e-3f));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcForwardFollowsViewYawTest, "SourceMovement.View.ForwardFollowsViewYaw", SrcViewTestFlags)

bool FSrcForwardFollowsViewYawTest::RunTest(const FString&)
{
	// Holding +forward at view yaw Y must move the player along Y
	const srcfloat Yaws[] = { 0.f, 90.f, 180.f, -90.f, 45.f };

	for (const srcfloat Yaw : Yaws)
	{
		FSourceTestFixture Fx(64);
		Fx.AddFlatGround(0.f);
		Fx.SettleOnGround(0.f);

		Fx.SetYaw(Yaw);
		Fx.HoldForward();
		Fx.StepN(8);
		const srcfloat Speed2D = Fx.State.Velocity.Length2D();
		TestTrue(*FString::Printf(TEXT("yaw %.0f: the player actually moved"), Yaw), Speed2D > 10.f);

		// Source convention: forward = (cos yaw, sin yaw)
		const srcfloat VelYaw = Fx.GetVelocityYawDegrees();
		const srcfloat Delta = FMath::UnwindDegrees(static_cast<float>(VelYaw - Yaw));

		TestTrue(*FString::Printf(TEXT("yaw %.0f: velocity heading matches the view (off by %.3f)"), Yaw, Delta), FMath::Abs(Delta) < 0.1f);

		// And the same after conversion, against the Unreal forward vector for that same yaw
		const FVector UEVelocity = SourceUnits::ToUEDirection(Fx.State.Velocity).GetSafeNormal2D();
		const FVector UEForward = SourceUnits::ToUERotator(FSrcAngles(0.f, Yaw, 0.f)).Vector().GetSafeNormal2D();

		TestTrue(*FString::Printf(TEXT("yaw %.0f: Unreal-space heading matches too (dot %.4f)"), Yaw, FVector::DotProduct(UEVelocity, UEForward)), FVector::DotProduct(UEVelocity, UEForward) > 0.999f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcStrafeIsRightOfViewTest, "SourceMovement.View.StrafeIsRightOfView", SrcViewTestFlags)

bool FSrcStrafeIsRightOfViewTest::RunTest(const FString&)
{
	FSourceTestFixture Fx(64);
	Fx.AddFlatGround(0.f);
	Fx.SettleOnGround(0.f);

	Fx.SetYaw(0.f);
	Fx.HoldRight();
	Fx.StepN(8);
	const FVector UEVelocity = SourceUnits::ToUEDirection(Fx.State.Velocity).GetSafeNormal2D();

	// View yaw 0 in Unreal is +X; its right is +Y
	TestTrue(TEXT("+moveright at yaw 0 moves along Unreal +Y"), FVector::DotProduct(UEVelocity, FVector(0.f, 1.f, 0.f)) > 0.999f);
	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
