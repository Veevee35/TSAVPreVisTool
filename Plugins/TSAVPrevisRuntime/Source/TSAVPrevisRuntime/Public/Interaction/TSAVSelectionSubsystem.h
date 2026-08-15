// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"

#include "TSAVSelectionSubsystem.generated.h"

class AActor;
class APlayerController;

USTRUCT(BlueprintType)
struct TSAVPREVISRUNTIME_API FTSAVSelectionSet
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TSAV PreVis|Selection")
	TArray<TObjectPtr<AActor>> Actors;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTSAVSelectionChanged, AActor*, PrimarySelection);

/** Local-player-owned selection service independent of Unreal Editor selection. */
UCLASS()
class TSAVPREVISRUNTIME_API UTSAVSelectionSubsystem final : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Selection")
	bool SelectFromScreenPosition(APlayerController* PlayerController, FVector2D ScreenPosition, float TraceDistance = 1000000.0f);

	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Selection")
	bool SelectActor(AActor* Actor, bool bAddToSelection = false);

	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Selection")
	void ClearSelection();

	UFUNCTION(BlueprintPure, Category = "TSAV PreVis|Selection")
	AActor* GetPrimarySelection() const;

	UFUNCTION(BlueprintPure, Category = "TSAV PreVis|Selection")
	FTSAVSelectionSet GetSelectionSet() const { return Selection; }

	UPROPERTY(BlueprintAssignable, Category = "TSAV PreVis|Selection")
	FTSAVSelectionChanged OnSelectionChanged;

private:
	static bool IsActorSelectable(const AActor* Actor);
	static void NotifySelectionState(AActor* Actor, bool bSelected);

	UPROPERTY(Transient)
	FTSAVSelectionSet Selection;
};
