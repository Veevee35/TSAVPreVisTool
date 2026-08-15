// Copyright TSAV. All Rights Reserved.

#pragma once

#include "Core/TSAVTypes.h"
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "TSAVMainWidget.generated.h"

class AActor;
class UCanvasPanel;
class UTextBlock;

/** Asset-independent runtime application chrome for the first packaged shell. */
UCLASS()
class TSAVPREVISRUNTIME_API UTSAVMainWidget final : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildLayout();
	void SetAppMode(ETSAVAppMode NewMode);
	void UpdateInspector(AActor* SelectedActor);
	static FText GetModeText(ETSAVAppMode Mode);

	UFUNCTION()
	void HandleSelectionChanged(AActor* SelectedActor);

	UFUNCTION()
	void HandleModeChanged(ETSAVAppMode NewMode, ETSAVAppMode PreviousMode);

	UFUNCTION()
	void SelectModeClicked();

	UFUNCTION()
	void VenueModeClicked();

	UFUNCTION()
	void StageModeClicked();

	UFUNCTION()
	void TrussModeClicked();

	UFUNCTION()
	void LightingModeClicked();

	UFUNCTION()
	void LEDModeClicked();

	UFUNCTION()
	void CameraModeClicked();

	UFUNCTION()
	void VideoModeClicked();

	UFUNCTION()
	void CharactersModeClicked();

	UFUNCTION()
	void WalkthroughModeClicked();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OutlinerSelectionText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InspectorTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InspectorBodyText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ModeStatusText;
};
