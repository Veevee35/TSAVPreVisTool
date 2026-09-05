// Copyright TSAV. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TSAVRigWidget.generated.h"
TSAVPREVISRUNTIME_API TSharedRef<SWidget> MakeTSAVRigPanel(UWorld* World,FSimpleDelegate Close);
UCLASS()
class TSAVPREVISRUNTIME_API UTSAVRigWidget final : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
