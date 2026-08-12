// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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
	int32 CanvasWidth = 4096;
	int32 CanvasHeight = 2160;
	int32 CanvasX = 0;
	int32 CanvasY = 0;
	bool bSerpentine = true;
	bool bShowSeams = true;
	bool bPreviewInEditor = true;

	TWeakObjectPtr<UTSAVLEDPanelDefinition> PanelDefinition;
	TWeakObjectPtr<UMediaSource> MediaSource;
	TWeakObjectPtr<ATSAVLEDWall> ActiveWall;
	FText StatusMessage;
	bool bStatusSuccess = true;
};
