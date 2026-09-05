// Copyright TSAV. All Rights Reserved.
#include "TSAVDMXPatchPlan.h"

bool TSAVDMXPatchPlan::Build(const TArray<FRange>& Selected, const TArray<FRange>& Occupied,
	int32 Universe, int32 Address, TArray<FRange>& OutPlan, FString& OutError)
{
	OutPlan.Reset();
	OutError.Reset();
	if (Selected.IsEmpty() || Universe < 1 || Address < 1 || Address > 512)
	{
		OutError = TEXT("Select fixtures and enter a positive universe and a start address from 1 to 512.");
		return false;
	}
	TSet<FName> Moving;
	for (const FRange& Range : Selected)
	{
		if (Range.Id.IsNone() || Moving.Contains(Range.Id) || Range.Span < 1 || Range.Span > 512)
		{
			OutError = TEXT("A selected fixture has no usable mode, an invalid footprint, or a duplicate identity.");
			return false;
		}
		Moving.Add(Range.Id);
	}
	TArray<FRange> Plan;
	for (FRange Range : Selected)
	{
		if (Address + Range.Span - 1 > 512)
		{
			if (Plan.IsEmpty())
			{
				OutError = FString::Printf(TEXT("%s needs %d channels and does not fit at address %d."), *Range.Label, Range.Span, Address);
				return false;
			}
			++Universe;
			Address = 1;
		}
		if (Universe > Range.MaxUniverse)
		{
			OutError = FString::Printf(TEXT("%s supports universes 1–%d."), *Range.Label, Range.MaxUniverse);
			return false;
		}
		Range.Universe = Universe;
		Range.Address = Address;
		for (const FRange& Other : Occupied)
		{
			if (!Moving.Contains(Other.Id) && Other.Universe == Universe && Other.Span > 0
				&& Address <= Other.Address + Other.Span - 1 && Other.Address <= Address + Range.Span - 1)
			{
				OutError = FString::Printf(TEXT("%s overlaps %s at U%d.%03d–%03d. No patches changed."),
					*Range.Label, *Other.Label, Universe, Other.Address, Other.Address + Other.Span - 1);
				return false;
			}
		}
		Plan.Add(Range);
		Address += Range.Span;
	}
	OutPlan = MoveTemp(Plan);
	return true;
}
