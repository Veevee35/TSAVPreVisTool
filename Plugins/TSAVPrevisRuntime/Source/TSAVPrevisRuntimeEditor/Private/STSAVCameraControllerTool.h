// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Video/TSAVCameraActor.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
template <typename OptionType> class SComboBox;

/** Central editor controller for every TSAV camera, including VISCA-over-IP PTZ output. */
class STSAVCameraControllerTool final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STSAVCameraControllerTool) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	using FCameraOption = TWeakObjectPtr<ATSAVCameraActor>;

	void RefreshCameraOptions();
	void SetActiveCamera(ATSAVCameraActor* Camera);
	void LoadFormFromCamera();
	ATSAVCameraActor* FindSelectedCamera() const;
	TSharedRef<SWidget> GenerateCameraOption(TSharedPtr<FCameraOption> Item) const;
	void CameraOptionChanged(TSharedPtr<FCameraOption> Item, ESelectInfo::Type SelectionType);
	FText GetActiveCameraText() const;
	FText GetCameraSummaryText() const;
	FReply RefreshCameras();
	FReply UseSelectedCamera();
	FReply SelectCameraInLevel();
	FReply ApplyPreview();
	FReply ApplyAndSendVisca();
	FReply SendViscaHome();
	FReply SendViscaStop();
	bool ApplyChanges(bool bSendVisca);
	void SetStatus(const FText& Message, bool bSuccess);
	FSlateColor GetStatusColor() const;

	TWeakObjectPtr<ATSAVCameraActor> ActiveCamera;
	TArray<TSharedPtr<FCameraOption>> CameraOptions;
	TSharedPtr<SComboBox<TSharedPtr<FCameraOption>>> CameraCombo;
	TSharedPtr<SEditableTextBox> CameraNameField;
	TSharedPtr<SEditableTextBox> ViscaIpField;
	FString CameraName;
	float PanDegrees = 0.0f;
	float TiltDegrees = 0.0f;
	float ZoomPercent = 0.0f;
	float Iris = 2.8f;
	float FocusDistanceCm = 1000.0f;
	float GainDb = 0.0f;
	FVector WorldPosition = FVector::ZeroVector;
	bool bViscaEnabled = false;
	FString ViscaIpAddress = TEXT("192.168.1.100");
	int32 ViscaPort = 52381;
	int32 ViscaPanSpeed = 12;
	int32 ViscaTiltSpeed = 10;
	FText StatusText;
	bool bStatusSuccess = true;
};
