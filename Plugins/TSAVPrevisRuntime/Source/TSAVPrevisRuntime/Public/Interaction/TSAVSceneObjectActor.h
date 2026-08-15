// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/TSAVSelectable.h"

#include "TSAVSceneObjectActor.generated.h"

class UStaticMeshComponent;
class UTSAVSceneObjectComponent;

/** Minimal editable scene object used by the first standalone application shell. */
UCLASS(Blueprintable)
class TSAVPREVISRUNTIME_API ATSAVSceneObjectActor final : public AActor, public ITSAVSelectable
{
	GENERATED_BODY()

public:
	ATSAVSceneObjectActor();

	virtual bool CanSelect_Implementation() const override;
	virtual void OnSelectionChanged_Implementation(bool bSelected) override;

	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }
	UTSAVSceneObjectComponent* GetSceneObjectComponent() const { return SceneObjectComponent; }

private:
	UPROPERTY(VisibleAnywhere, Category = "TSAV PreVis|Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, Category = "TSAV PreVis|Components")
	TObjectPtr<UTSAVSceneObjectComponent> SceneObjectComponent;
};
