// Copyright TSAV. All Rights Reserved.

#pragma once

#include "Components/Button.h"
#include "CoreMinimal.h"

#include "TSAVLEDPanelCellButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTSAVLEDPanelCellClicked, int32, Column, int32, Row);

/** Coordinate-aware cabinet button used by the runtime LED wall layout editor. */
UCLASS()
class TSAVPREVISRUNTIME_API UTSAVLEDPanelCellButton final : public UButton
{
	GENERATED_BODY()

public:
	void InitializeCell(int32 InColumn, int32 InRow);

	UPROPERTY(BlueprintAssignable, Category = "TSAV PreVis|LED Wall")
	FTSAVLEDPanelCellClicked OnCellClicked;

private:
	UFUNCTION()
	void HandleClicked();

	int32 Column = INDEX_NONE;
	int32 Row = INDEX_NONE;
};
