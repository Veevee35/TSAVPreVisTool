// Copyright TSAV. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "TSAVLEDWall.h"

#include "TSAVLEDWallConfiguratorWidget.generated.h"

class ATSAVVideoSwitcher;
class UBorder;
class UButton;
class UCanvasPanel;
class UCheckBox;
class UComboBoxString;
class UEditableTextBox;
class UMediaSource;
class UTextBlock;
class UTSAVLEDPanelDefinition;
class UUniformGridPanel;
class UVerticalBox;

/**
 * Full-screen, packaged-build counterpart to the editor LED Wall Builder.
 * It deliberately depends only on runtime modules and writes through the
 * command subsystem so every applied configuration participates in undo/redo
 * and .tsav persistence.
 */
UCLASS()
class TSAVPREVISRUNTIME_API UTSAVLEDWallConfiguratorWidget final : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Load a wall into the configurator. The widget may already be in the viewport. */
	void OpenForWall(ATSAVLEDWall* Wall);

	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|LED Wall")
	void CloseConfigurator();

	UFUNCTION(BlueprintPure, Category = "TSAV PreVis|LED Wall")
	ATSAVLEDWall* GetConfiguredWall() const { return ConfiguredWall; }

	UFUNCTION(BlueprintPure, Category = "TSAV PreVis|LED Wall")
	int32 GetPanelCellCount() const { return PanelCellCount; }

	UFUNCTION(BlueprintPure, Category = "TSAV PreVis|LED Wall")
	int32 GetPanelDefinitionCount() const { return AvailablePanelDefinitions.Num(); }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void BuildLayout();
	void LoadFromWall();
	void ApplyFieldsToWall(ATSAVLEDWall& Wall);
	void ResizeLayoutData(int32 NewColumns, int32 NewRows);
	void RebuildPanelGrid();
	void RefreshSummaryAndCanvas();
	void RefreshAssetOptions();
	void RefreshRouteOptions();
	void UpdateUndoRedoButtons();
	bool ReadAndValidateDimensions(int32& OutColumns, int32& OutRows) const;
	bool DoesScreenFitCanvas() const;
	FIntPoint GetDraftWallResolution() const;
	FText GetWallSummary() const;
	FText GetCanvasStatus() const;
	FTransform MakePlacementTransform() const;
	FString GetFieldText(int32 Index) const;
	int32 GetIntField(int32 Index, int32 Fallback) const;
	float GetFloatField(int32 Index, float Fallback) const;
	void SetFieldText(int32 Index, const FString& Value);
	void SetStatus(const FText& Message, bool bSuccess);

	UFUNCTION()
	void CloseClicked();

	UFUNCTION()
	void ApplyClicked();

	UFUNCTION()
	void CreateNewClicked();

	UFUNCTION()
	void UndoClicked();

	UFUNCTION()
	void RedoClicked();

	UFUNCTION()
	void RefreshPreviewClicked();

	UFUNCTION()
	void SelectAllClicked();

	UFUNCTION()
	void ClearSelectionClicked();

	UFUNCTION()
	void ApplyStyleClicked();

	UFUNCTION()
	void ResetStyleClicked();

	UFUNCTION()
	void HandlePanelCellClicked(int32 Column, int32 Row);

	UFUNCTION()
	void PanelPresetChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void PanelStyleChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UPROPERTY(Transient)
	TObjectPtr<ATSAVLEDWall> ConfiguredWall;

	/** Ordered text fields; the implementation owns the stable field indexes. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UEditableTextBox>> Fields;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> PanelPresetCombo;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> PanelStyleCombo;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> LinkPatternCombo;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> SubpixelCombo;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> SwitcherCombo;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> VideoBusCombo;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> MediaSourceCombo;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> ShowSeamsCheck;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> AutoPlayCheck;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> PanelGrid;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> CanvasPreview;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ScreenPreview;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SummaryText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CanvasStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SelectionStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EditingWallText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> UndoButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RedoButton;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTSAVLEDPanelDefinition>> AvailablePanelDefinitions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMediaSource>> AvailableMediaSources;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ATSAVVideoSwitcher>> AvailableSwitchers;

	TArray<ETSAVLEDPanelEdgeStyle> PanelEdgeStyles;
	TArray<bool> PanelSelection;
	int32 LayoutColumns = 1;
	int32 LayoutRows = 1;
	int32 PanelCellCount = 0;
	ETSAVLEDPanelEdgeStyle SelectedPanelStyle = ETSAVLEDPanelEdgeStyle::Square;
	bool bStatusSuccess = true;
};
