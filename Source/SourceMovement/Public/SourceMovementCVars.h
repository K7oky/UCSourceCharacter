#pragma once

#include "CoreMinimal.h"

struct FSourceMovementParams;
struct FSourceInputTranslator;

namespace SourceMovementCVars
{
	SOURCEMOVEMENT_API void RegisterCheatGate();

	SOURCEMOVEMENT_API bool AreCheatsEnabled();

	SOURCEMOVEMENT_API void ApplyToParams(FSourceMovementParams& OutParams);

	// cl_forwardspeed / cl_sidespeed / cl_backspeed / cl_upspeed
	SOURCEMOVEMENT_API void ApplyToInputTranslator(FSourceInputTranslator& OutTranslator);

	// cl_pitchdown, the positive (looking DOWN) clamp
	SOURCEMOVEMENT_API float GetPitchDown();

	// cl_pitchup, the negative (looking UP) clamp
	SOURCEMOVEMENT_API float GetPitchUp();

	SOURCEMOVEMENT_API float GetSensitivity();

	// m_yaw, degrees per mouse count
	SOURCEMOVEMENT_API float GetMouseYawFactor();

	// m_pitch, degrees per mouse count
	SOURCEMOVEMENT_API float GetMousePitchFactor();

	SOURCEMOVEMENT_API int32 GetShowPos();

	// cl_sourcemove_drawhull
	SOURCEMOVEMENT_API bool GetDrawHull();

	// cl_sourcemove_drawtraces, 0 off, 1 sweeps that hit, 2 every sweep including misses
	SOURCEMOVEMENT_API int32 GetDrawTraces();

	// cl_sourcemove_tracelog
	SOURCEMOVEMENT_API bool GetTraceLog();

	struct FPersistableCVar
	{
		FString Name;
		FString Value;
		FString DefaultValue;
		bool bIsCheatGated = false;

		bool IsAtDefault() const { return Value == DefaultValue; }
	};

	SOURCEMOVEMENT_API void GetPersistableCVars(TArray<FPersistableCVar>& Out);
}
