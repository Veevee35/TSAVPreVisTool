// Copyright TSAV. All Rights Reserved.

#include "UI/TSAVOutlinerButton.h"

#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVOutlinerButton)

void UTSAVOutlinerButton::InitializeForActor(AActor* Actor)
{
	TargetActor = Actor;
	OnClicked.AddUniqueDynamic(this, &UTSAVOutlinerButton::HandleClicked);
}

void UTSAVOutlinerButton::HandleClicked()
{
	OnActorClicked.Broadcast(TargetActor);
}
