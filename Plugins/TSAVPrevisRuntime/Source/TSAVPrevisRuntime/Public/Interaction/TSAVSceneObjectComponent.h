// Copyright TSAV. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Core/TSAVTypes.h"
#include "CoreMinimal.h"

#include "TSAVSceneObjectComponent.generated.h"

/** Persistent identity and common runtime-authoring state for a scene object. */
UCLASS(ClassGroup = "TSAV PreVis", BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class TSAVPREVISRUNTIME_API UTSAVSceneObjectComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	UTSAVSceneObjectComponent();

	virtual void OnRegister() override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Scene Object")
	void EnsureObjectId();

	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Scene Object")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Scene Object")
	void SetObjectVisible(bool bInVisible);

	UFUNCTION(BlueprintPure, Category = "TSAV PreVis|Scene Object")
	bool IsSelected() const { return bSelected; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "TSAV PreVis|Scene Object")
	FGuid ObjectId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "TSAV PreVis|Scene Object")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "TSAV PreVis|Scene Object")
	ETSAVObjectType ObjectType = ETSAVObjectType::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "TSAV PreVis|Scene Object")
	bool bLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "TSAV PreVis|Scene Object")
	bool bVisible = true;

private:
	void ApplyVisibility() const;

	UPROPERTY(Transient)
	bool bSelected = false;
};
