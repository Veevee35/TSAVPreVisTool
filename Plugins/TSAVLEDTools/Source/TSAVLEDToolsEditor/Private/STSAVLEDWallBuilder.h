// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TSAVLEDWall.h"
#include "Widgets/SCompoundWidget.h"

class ATSAVLEDWall;
class UMediaSource;
class UTSAVLEDPanelDefinition;
struct FAssetData;

/** Guided, operator-facing LED wall creation tool. */
class STSAVLEDWallBuilder final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STSAVLEDWallBuilder) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void OnPanelDefinitionChanged(const FAssetData& AssetData);
	void OnMediaSourceChanged(const FAssetData& AssetData);
	FReply SavePanelPreset();
	FReply LoadSelectedWall();
	FReply CreateWall();
	FReply UpdateWall();
	void ApplySettings(ATSAVLEDWall& Wall) const;
	void ResizeLayoutData(int32 NewColumns, int32 NewRows);
	void ApplyPanelStyle(int32 Column, int32 Row, bool bResetToSquare);
	ATSAVLEDWall* FindSelectedWall() const;
	bool DoesScreenFitCanvas() const;
	FIntPoint GetWallResolution() const;
	FVector2D GetPixelPitchMm() const;
	FText GetWallSummary() const;
	FText GetCanvasStatus() const;
	FSlateColor GetCanvasStatusColor() const;
	FText GetSelectionStatus() const;
	void SetStatus(const FText& Message, bool bSuccess);

	FString WallName = TEXT("LED Wall");
	FString PanelPresetName = TEXT("Custom_LED_Panel");
	float PanelWidthCm = 50.0f;
	float PanelHeightCm = 50.0f;
	float PanelDepthCm = 8.0f;
	int32 PanelResolutionX = 128;
	int32 PanelResolutionY = 128;
	int32 Columns = 8;
	int32 Rows = 4;
	float PanelGapCm = 0.5f;
	float BorderCm = 2.0f;
	double RoundEdgeRadiusMeters = 0.5;
	int32 CanvasWidth = 4096;
	int32 CanvasHeight = 2160;
	int32 CanvasX = 0;
	int32 CanvasY = 0;
	bool bSerpentine = true;
	bool bShowSeams = true;
	bool bPreviewInEditor = true;
	ETSAVLEDSubpixelLayout SubpixelLayout = ETSAVLEDSubpixelLayout::None;
	float SubpixelStrength = 1.0f;
	TArray<float> ColumnSeamAnglesDegrees;
	TArray<float> RowSeamAnglesDegrees;
	TArray<bool> ColumnInternalCurveEnabled;
	TArray<double> ColumnInternalCurveRadiusAMeters;
	TArray<double> ColumnInternalCurveRadiusBMeters;
	TArray<bool> RowIgnoreInternalColumnCurves;
	TArray<ETSAVLEDPanelEdgeStyle> PanelEdgeStyles;
	ETSAVLEDPanelEdgeStyle SelectedPanelStyle = ETSAVLEDPanelEdgeStyle::Square;
	int32 LayoutDataColumns = 8;
	int32 LayoutDataRows = 4;

	TWeakObjectPtr<UTSAVLEDPanelDefinition> PanelDefinition;
	TWeakObjectPtr<UMediaSource> MediaSource;
	TWeakObjectPtr<ATSAVLEDWall> ActiveWall;
	FText StatusMessage;
	bool bStatusSuccess = true;
};
