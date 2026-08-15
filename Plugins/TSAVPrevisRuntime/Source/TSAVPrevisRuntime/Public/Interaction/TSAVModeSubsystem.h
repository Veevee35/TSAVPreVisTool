// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/TSAVTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "TSAVModeSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTSAVModeChanged, ETSAVAppMode, NewMode, ETSAVAppMode, PreviousMode);

/** Central source of truth for the application's current authoring mode. */
UCLASS()
class TSAVPREVISRUNTIME_API UTSAVModeSubsystem final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Mode")
	void SetMode(ETSAVAppMode NewMode);

	UFUNCTION(BlueprintPure, Category = "TSAV PreVis|Mode")
	ETSAVAppMode GetMode() const { return CurrentMode; }

	UFUNCTION(BlueprintPure, Category = "TSAV PreVis|Mode")
	bool IsInMode(ETSAVAppMode Mode) const { return CurrentMode == Mode; }

	UPROPERTY(BlueprintAssignable, Category = "TSAV PreVis|Mode")
	FTSAVModeChanged OnModeChanged;

private:
	UPROPERTY(VisibleAnywhere, Category = "TSAV PreVis|Mode")
	ETSAVAppMode CurrentMode = ETSAVAppMode::Select;
};
