// Copyright TSAV. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scene/TSAVSceneGenerator.h"
#include "TSAVSceneBuilderWidget.generated.h"

TSAVPREVISRUNTIME_API TSharedRef<SWidget> MakeTSAVSceneBuilder(UWorld* World, FSimpleDelegate Close, ETSAVScenicKind InitialKind=ETSAVScenicKind::Stage);
UCLASS()
class TSAVPREVISRUNTIME_API UTSAVSceneBuilderWidget final : public UUserWidget
{
	GENERATED_BODY()
public:
	ETSAVScenicKind InitialKind=ETSAVScenicKind::Stage;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
