// Copyright TSAV. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TSAVLightingShowWidget.generated.h"

UCLASS()
class TSAVPREVISRUNTIME_API UTSAVLightingShowWidget final : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
