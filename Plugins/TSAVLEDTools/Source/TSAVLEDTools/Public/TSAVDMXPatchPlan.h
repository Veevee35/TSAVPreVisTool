// Copyright TSAV. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

namespace TSAVDMXPatchPlan
{
	struct FRange
	{
		FName Id;
		FString Label;
		int32 Universe = 1;
		int32 Address = 1;
		int32 Span = 1;
		int32 MaxUniverse = 63999;
	};
	/** Preflight the whole batch; no actor or asset is modified on failure. */
	TSAVLEDTOOLS_API bool Build(const TArray<FRange>& Selected, const TArray<FRange>& Occupied,
		int32 Universe, int32 Address, TArray<FRange>& OutPlan, FString& OutError);
}
