// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "TSAVCommandSubsystem.generated.h"

class AActor;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTSAVCommandHistoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTSAVCommandObjectChanged, AActor*, Actor);

/** Runtime command history used by every authoring operation in the packaged app. */
UCLASS()
class TSAVPREVISRUNTIME_API UTSAVCommandSubsystem final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	class ITSAVCommand;

	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Commands")
	AActor* SpawnSceneObject(UWorld* World, const FTransform& Transform, const FText& DisplayName);

	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Commands")
	AActor* DuplicateActor(AActor* SourceActor, const FVector& WorldOffset = FVector(50.0f, 50.0f, 0.0f));

	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Commands")
	bool DeleteActor(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Commands")
	bool SetActorTransform(AActor* Actor, const FTransform& NewTransform, const FText& Description);

	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Commands")
	bool SetDisplayName(AActor* Actor, const FText& NewDisplayName);

	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Commands")
	bool SetLocked(AActor* Actor, bool bLocked);

	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Commands")
	bool SetVisible(AActor* Actor, bool bVisible);

	/** Begin/end collapse a live gizmo drag into one transform command. */
	void BeginTransformTransaction(AActor* Actor);
	void UpdateTransformTransaction(AActor* Actor, const FTransform& NewTransform);
	void EndTransformTransaction(AActor* Actor, const FText& Description);
	void CancelTransformTransaction();

	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Commands")
	bool Undo();

	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Commands")
	bool Redo();

	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Commands")
	void ClearHistory();

	UFUNCTION(BlueprintPure, Category = "TSAV PreVis|Commands")
	bool CanUndo() const { return !UndoStack.IsEmpty(); }

	UFUNCTION(BlueprintPure, Category = "TSAV PreVis|Commands")
	bool CanRedo() const { return !RedoStack.IsEmpty(); }

	UFUNCTION(BlueprintPure, Category = "TSAV PreVis|Commands")
	FText GetUndoDescription() const;

	UFUNCTION(BlueprintPure, Category = "TSAV PreVis|Commands")
	FText GetRedoDescription() const;

	UPROPERTY(BlueprintAssignable, Category = "TSAV PreVis|Commands")
	FTSAVCommandHistoryChanged OnHistoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "TSAV PreVis|Commands")
	FTSAVCommandObjectChanged OnObjectChanged;

private:
	void ExecuteAndStore(const TSharedRef<ITSAVCommand>& Command);
	void StoreApplied(const TSharedRef<ITSAVCommand>& Command);
	void NotifyMutation(AActor* Actor);

	TArray<TSharedPtr<ITSAVCommand>> UndoStack;
	TArray<TSharedPtr<ITSAVCommand>> RedoStack;
	TWeakObjectPtr<AActor> TransactionActor;
	FTransform TransactionStartTransform;
	bool bTransformTransactionActive = false;
};
