// Copyright TSAV. All Rights Reserved.

#include "Interaction/TSAVSelectionSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "Interaction/TSAVSelectable.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVSelectionSubsystem)

bool UTSAVSelectionSubsystem::SelectFromScreenPosition(
	APlayerController* PlayerController,
	const FVector2D ScreenPosition,
	const float TraceDistance,
	const bool bAddToSelection)
{
	if (!PlayerController || !PlayerController->GetWorld())
	{
		return false;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!PlayerController->DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, WorldOrigin, WorldDirection))
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TSAVSelection), true);
	if (!PlayerController->GetWorld()->LineTraceSingleByChannel(
		Hit,
		WorldOrigin,
		WorldOrigin + WorldDirection * FMath::Max(TraceDistance, 100.0f),
		ECC_Visibility,
		QueryParams))
	{
		if (!bAddToSelection)
		{
			ClearSelection();
		}
		return false;
	}

	return SelectActor(Hit.GetActor(), bAddToSelection);
}

bool UTSAVSelectionSubsystem::SelectActor(AActor* Actor, const bool bAddToSelection)
{
	if (!IsActorSelectable(Actor))
	{
		if (!bAddToSelection)
		{
			ClearSelection();
		}
		return false;
	}

	if (!bAddToSelection)
	{
		for (AActor* SelectedActor : Selection.Actors)
		{
			if (SelectedActor != Actor)
			{
				NotifySelectionState(SelectedActor, false);
			}
		}
		Selection.Actors.Reset();
	}

	if (!Selection.Actors.Contains(Actor))
	{
		Selection.Actors.Add(Actor);
		NotifySelectionState(Actor, true);
	}

	OnSelectionChanged.Broadcast(GetPrimarySelection());
	return true;
}

bool UTSAVSelectionSubsystem::SelectActorFromOutliner(AActor* Actor)
{
	if (!IsValid(Actor) || !Actor->FindComponentByClass<UTSAVSceneObjectComponent>())
	{
		return false;
	}
	for (AActor* SelectedActor : Selection.Actors)
	{
		if (SelectedActor != Actor)
		{
			NotifySelectionState(SelectedActor, false);
		}
	}
	Selection.Actors.Reset();
	Selection.Actors.Add(Actor);
	NotifySelectionState(Actor, true);
	OnSelectionChanged.Broadcast(Actor);
	return true;
}

void UTSAVSelectionSubsystem::ClearSelection()
{
	if (Selection.Actors.IsEmpty())
	{
		return;
	}

	for (AActor* Actor : Selection.Actors)
	{
		NotifySelectionState(Actor, false);
	}
	Selection.Actors.Reset();
	OnSelectionChanged.Broadcast(nullptr);
}

AActor* UTSAVSelectionSubsystem::GetPrimarySelection() const
{
	return Selection.Actors.IsEmpty() || !IsValid(Selection.Actors.Last()) ? nullptr : Selection.Actors.Last();
}

bool UTSAVSelectionSubsystem::IsActorSelectable(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}

	if (Actor->Implements<UTSAVSelectable>())
	{
		return ITSAVSelectable::Execute_CanSelect(Actor);
	}

	const UTSAVSceneObjectComponent* SceneObject = Actor->FindComponentByClass<UTSAVSceneObjectComponent>();
	return SceneObject && !SceneObject->bLocked && SceneObject->bVisible;
}

void UTSAVSelectionSubsystem::NotifySelectionState(AActor* Actor, const bool bSelected)
{
	if (!IsValid(Actor))
	{
		return;
	}

	if (Actor->Implements<UTSAVSelectable>())
	{
		ITSAVSelectable::Execute_OnSelectionChanged(Actor, bSelected);
		return;
	}

	if (UTSAVSceneObjectComponent* SceneObject = Actor->FindComponentByClass<UTSAVSceneObjectComponent>())
	{
		SceneObject->SetSelected(bSelected);
	}
}
