// Copyright TSAV. All Rights Reserved.

#include "UI/TSAVMenuButton.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVMenuButton)

void UTSAVMenuButton::InitializeForAction(const ETSAVMenuAction InAction)
{
	Action = InAction;
	OnClicked.AddUniqueDynamic(this, &UTSAVMenuButton::HandleClicked);
}

void UTSAVMenuButton::HandleClicked()
{
	OnActionClicked.Broadcast(Action);
}
