// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Video/TSAVCameraActor.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
template <typename OptionType> class SComboBox;

/** Focused editor form that creates a configured, switcher-routable camera input. */
class STSAVCameraTool final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STSAVCameraTool) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	using FLensOption = ETSAVLensPreset;

	void PopulateLensOptions();
	TSharedRef<SWidget> GenerateLensOption(TSharedPtr<FLensOption> Item) const;
	void LensOptionChanged(TSharedPtr<FLensOption> Item, ESelectInfo::Type SelectionType);
	FText GetSelectedLensText() const;
	FText GetStatusText() const { return StatusText; }
	FSlateColor GetStatusColor() const;
	FReply CreateCameraInput();
	FString MakeDefaultCameraName() const;
	void SetStatus(const FText& Message, bool bSuccess);

	FString CameraName;
	int32 OutputWidth = 1920;
	int32 OutputHeight = 1080;
	bool bIsPTZ = false;
	ETSAVLensPreset LensPreset = ETSAVLensPreset::BroadcastZoom;
	TArray<TSharedPtr<FLensOption>> LensOptions;
	TSharedPtr<SComboBox<TSharedPtr<FLensOption>>> LensCombo;
	TSharedPtr<SEditableTextBox> CameraNameField;
	FText StatusText;
	bool bStatusSuccess = true;
};
