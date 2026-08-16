// Copyright TSAV. All Rights Reserved.

#pragma once

#include "Components/Button.h"
#include "CoreMinimal.h"

#include "TSAVSwitcherInputButton.generated.h"

class ATSAVVideoSwitcher;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTSAVSwitcherRouteClicked, ATSAVVideoSwitcher*, Switcher, FGuid, InputId, FName, BusName);

/** Dynamic switcher crosspoint button used by the packaged routing panel. */
UCLASS()
class TSAVPREVISRUNTIME_API UTSAVSwitcherInputButton final : public UButton
{
	GENERATED_BODY()

public:
	void InitializeRoute(ATSAVVideoSwitcher* InSwitcher, FGuid InInputId, FName InBusName);

	UPROPERTY(BlueprintAssignable)
	FTSAVSwitcherRouteClicked OnRouteClicked;

private:
	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	TObjectPtr<ATSAVVideoSwitcher> Switcher;

	FGuid InputId;
	FName BusName;
};
