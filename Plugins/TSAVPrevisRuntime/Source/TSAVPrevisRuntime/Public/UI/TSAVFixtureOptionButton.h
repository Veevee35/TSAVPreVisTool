// Copyright TSAV. All Rights Reserved.

#pragma once

#include "Components/Button.h"
#include "CoreMinimal.h"

#include "TSAVFixtureOptionButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTSAVFixtureOptionClicked, FName, DefinitionId);

/** Runtime fixture-browser row that retains the generated catalog definition it represents. */
UCLASS()
class TSAVPREVISRUNTIME_API UTSAVFixtureOptionButton final : public UButton
{
	GENERATED_BODY()

public:
	void InitializeForDefinition(FName InDefinitionId);

	UPROPERTY(BlueprintAssignable)
	FTSAVFixtureOptionClicked OnFixtureOptionClicked;

private:
	UFUNCTION()
	void HandleClicked();

	FName DefinitionId;
};
