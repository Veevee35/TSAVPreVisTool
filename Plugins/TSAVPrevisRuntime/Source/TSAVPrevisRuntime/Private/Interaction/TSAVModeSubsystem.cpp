// Copyright TSAV. All Rights Reserved.

#include "Interaction/TSAVModeSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVModeSubsystem)

void UTSAVModeSubsystem::SetMode(const ETSAVAppMode NewMode)
{
	if (CurrentMode == NewMode)
	{
		return;
	}

	const ETSAVAppMode PreviousMode = CurrentMode;
	CurrentMode = NewMode;
	OnModeChanged.Broadcast(CurrentMode, PreviousMode);
}
