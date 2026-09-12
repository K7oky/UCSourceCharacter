// The held bit versus the press and release bits
#if WITH_DEV_AUTOMATION_TESTS

#include "Input/SourceKButton.h"

#include "Misc/AutomationTest.h"

namespace
{
	constexpr EAutomationTestFlags SrcKButtonTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	constexpr int32 SomeKey = 1234;
	constexpr int32 OtherKey = 5678;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcKButtonTapRegistersTest, "SourceMovement.KButton.TapWithinOneFrameStillPressesTheButton", SrcKButtonTestFlags)

bool FSrcKButtonTapRegistersTest::RunTest(const FString&)
{
	// A mouse wheel notch: down and up with no frame boundary between them
	FSourceKButton Button;
	Button.KeyDown(SomeKey);
	Button.KeyUp(SomeKey);

	// KeyUp cleared the held bit (state &= ~1) and set impulse-up, leaving state == 2|4
	TestFalse(TEXT("the key is not held any more"), Button.IsHeld());

	// But CalcButtonBits tests `state & 3`, and the impulse-down bit is still set
	TestTrue(TEXT("the button bit is still reported for this command"), Button.GetButtonBit());

	// Resetting clears only the press bit, so the release bit survives
	Button.ClearButtonImpulse();

	TestFalse(TEXT("and is consumed after exactly one command"), Button.GetButtonBit());
	TestFalse(TEXT("still not held"), Button.IsHeld());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcKButtonHeldSurvivesResetTest, "SourceMovement.KButton.HeldKeySurvivesTheImpulseClear", SrcKButtonTestFlags)

bool FSrcKButtonHeldSurvivesResetTest::RunTest(const FString&)
{
	// The other half
	FSourceKButton Button;
	Button.KeyDown(SomeKey);

	TestTrue(TEXT("held"), Button.IsHeld());
	TestTrue(TEXT("button bit set on the first command"), Button.GetButtonBit());

	Button.ClearButtonImpulse();

	TestTrue(TEXT("still held after the impulse is consumed"), Button.IsHeld());
	TestTrue(TEXT("and still reported on the next command"), Button.GetButtonBit());

	Button.KeyUp(SomeKey);
	Button.ClearButtonImpulse();

	TestFalse(TEXT("released"), Button.IsHeld());
	TestFalse(TEXT("and no longer reported"), Button.GetButtonBit());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcKButtonKeyStateFractionsTest, "SourceMovement.KButton.KeyStateFractions", SrcKButtonTestFlags)

bool FSrcKButtonKeyStateFractionsTest::RunTest(const FString&)
{
	{
		FSourceKButton Held;
		Held.KeyDown(SomeKey);
		Held.KeyState();  // consume the initial impulse
		TestEqual(TEXT("held for the whole frame -> 1.0"), Held.KeyState(), 1.0f);
	}

	{
		FSourceKButton Pressed;
		Pressed.KeyDown(SomeKey);
		TestEqual(TEXT("pressed part-way through the frame -> 0.5"), Pressed.KeyState(), 0.5f);
	}

	{
		FSourceKButton Tapped;
		Tapped.KeyDown(SomeKey);
		Tapped.KeyUp(SomeKey);
		TestEqual(TEXT("pressed and released within one frame -> 0.25"), Tapped.KeyState(), 0.25f);
	}

	{
		FSourceKButton Idle;
		TestEqual(TEXT("untouched -> 0.0"), Idle.KeyState(), 0.0f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcKButtonTwoKeysTest, "SourceMovement.KButton.TwoKeysHoldOneButton", SrcKButtonTestFlags)

bool FSrcKButtonTwoKeysTest::RunTest(const FString&)
{
	FSourceKButton Button;
	Button.KeyDown(SomeKey);
	Button.KeyDown(OtherKey);

	TestTrue(TEXT("held by two keys"), Button.IsHeld());

	Button.KeyUp(SomeKey);
	TestTrue(TEXT("still held by the second key"), Button.IsHeld());

	Button.KeyUp(OtherKey);
	TestFalse(TEXT("released once both are up"), Button.IsHeld());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrcKButtonConsoleReleaseTest, "SourceMovement.KButton.ConsoleReleaseForcesUp", SrcKButtonTestFlags)

bool FSrcKButtonConsoleReleaseTest::RunTest(const FString&)
{
	FSourceKButton Button;
	Button.KeyDown(SomeKey);
	Button.KeyDown(OtherKey);

	Button.KeyUp(0);

	TestFalse(TEXT("force-released despite two keys being down"), Button.IsHeld());
	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
