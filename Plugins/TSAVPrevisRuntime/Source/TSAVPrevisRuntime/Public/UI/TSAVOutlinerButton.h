// Copyright TSAV. All Rights Reserved.

#pragma once

#include "Components/Button.h"
#include "CoreMinimal.h"

#include "TSAVOutlinerButton.generated.h"

class AActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTSAVOutlinerActorClicked, AActor*, Actor);

/** Small runtime outliner button that retains the actor represented by its row. */
UCLASS()
class TSAVPREVISRUNTIME_API UTSAVOutlinerButton final : public UButton
{
	GENERATED_BODY()

public:
	void InitializeForActor(AActor* Actor);

	UPROPERTY(BlueprintAssignable)
	FTSAVOutlinerActorClicked OnActorClicked;

private:
	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	TObjectPtr<AActor> TargetActor;
};
