// Copyright TSAV. All Rights Reserved.

#include "Interaction/TSAVSceneObjectComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVSceneObjectComponent)

UTSAVSceneObjectComponent::UTSAVSceneObjectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTSAVSceneObjectComponent::OnRegister()
{
	Super::OnRegister();
	EnsureObjectId();
}

void UTSAVSceneObjectComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureObjectId();
	ApplyVisibility();
}

void UTSAVSceneObjectComponent::EnsureObjectId()
{
	const AActor* Owner = GetOwner();
	if (!ObjectId.IsValid() && Owner && !Owner->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		ObjectId = FGuid::NewGuid();
	}
}

void UTSAVSceneObjectComponent::SetSelected(const bool bInSelected)
{
	bSelected = bInSelected;

	if (AActor* Owner = GetOwner())
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		Owner->GetComponents(PrimitiveComponents);
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			PrimitiveComponent->SetRenderCustomDepth(bSelected);
			PrimitiveComponent->SetCustomDepthStencilValue(bSelected ? 252 : 0);
		}
	}
}

void UTSAVSceneObjectComponent::SetObjectVisible(const bool bInVisible)
{
	bVisible = bInVisible;
	ApplyVisibility();
}

void UTSAVSceneObjectComponent::ApplyVisibility() const
{
	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorHiddenInGame(!bVisible);
	}
}
