// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "TSAVSelectable.generated.h"

UINTERFACE(BlueprintType)
class TSAVPREVISRUNTIME_API UTSAVSelectable : public UInterface
{
	GENERATED_BODY()
};

/** Runtime selection contract for editable TSAV scene actors. */
class TSAVPREVISRUNTIME_API ITSAVSelectable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "TSAV PreVis|Selection")
	bool CanSelect() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "TSAV PreVis|Selection")
	void OnSelectionChanged(bool bSelected);
};
