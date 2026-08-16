// Copyright TSAV. All Rights Reserved.

#include "UI/TSAVFixtureOptionButton.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVFixtureOptionButton)

void UTSAVFixtureOptionButton::InitializeForDefinition(const FName InDefinitionId)
{
	DefinitionId = InDefinitionId;
	OnClicked.AddUniqueDynamic(this, &UTSAVFixtureOptionButton::HandleClicked);
}

void UTSAVFixtureOptionButton::HandleClicked()
{
	OnFixtureOptionClicked.Broadcast(DefinitionId);
}
