// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Video/TSAVCameraActor.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
template <typename OptionType> class SComboBox;

/** Editor-facing production camera creation and configuration panel. */
class STSAVCameraTool final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STSAVCameraTool) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	using FCameraOption = TWeakObjectPtr<ATSAVCameraActor>;

	void RefreshCameraOptions();
	void SetActiveCamera(ATSAVCameraActor* Camera);
	ATSAVCameraActor* FindSelectedCamera() const;
	TSharedRef<SWidget> GenerateCameraOption(TSharedPtr<FCameraOption> Item) const;
	void CameraOptionChanged(TSharedPtr<FCameraOption> Item, ESelectInfo::Type SelectionType);
	FText GetActiveCameraText() const;
	FText GetStatusText() const { return StatusText; }
	FSlateColor GetStatusColor() const;
	FReply CreateCameraFromView();
	FReply UseSelectedCamera();
	FReply RefreshCameras();
	FReply SnapCameraToView();
	FReply SelectCameraInLevel();
	void SetStatus(const FText& Message, bool bSuccess);

	TWeakObjectPtr<ATSAVCameraActor> ActiveCamera;
	TArray<TSharedPtr<FCameraOption>> CameraOptions;
	TSharedPtr<SComboBox<TSharedPtr<FCameraOption>>> CameraCombo;
	TSharedPtr<IDetailsView> DetailsView;
	FText StatusText;
	bool bStatusSuccess = true;
};
