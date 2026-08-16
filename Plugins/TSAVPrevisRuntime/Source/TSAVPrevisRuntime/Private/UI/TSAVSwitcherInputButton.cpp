// Copyright TSAV. All Rights Reserved.

#include "UI/TSAVSwitcherInputButton.h"

#include "TSAVVideoSwitcher.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVSwitcherInputButton)

void UTSAVSwitcherInputButton::InitializeRoute(ATSAVVideoSwitcher* InSwitcher, const FGuid InInputId, const FName InBusName)
{
	Switcher = InSwitcher;
	InputId = InInputId;
	BusName = InBusName;
	OnClicked.AddUniqueDynamic(this, &UTSAVSwitcherInputButton::HandleClicked);
}

void UTSAVSwitcherInputButton::HandleClicked()
{
	OnRouteClicked.Broadcast(Switcher, InputId, BusName);
}
