// Copyright TSAV. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TSAVLaserWidget.generated.h"
TSAVPREVISRUNTIME_API TSharedRef<SWidget> MakeTSAVLaserPanel(UWorld* World,FSimpleDelegate Close);
UCLASS()
class TSAVPREVISRUNTIME_API UTSAVLaserWidget final : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
