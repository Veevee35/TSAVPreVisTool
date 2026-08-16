// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Video/TSAVCameraActor.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class FScopedTransaction;
template <typename OptionType> class SComboBox;

/** Four-bank editor controller for TSAV cameras, including VISCA-over-IP PTZ output. */
class STSAVCameraControllerTool final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STSAVCameraControllerTool) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~STSAVCameraControllerTool() override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FTSAVCameraControllerWidgetConstructionTest;
#endif
	using FCameraOption = TWeakObjectPtr<ATSAVCameraActor>;
	enum class ELiveControlGroup : uint8
	{
		PTZ,
		Image,
		Position,
		Connection,
		Metadata,
	};

	struct FCameraControlSlot
	{
		TWeakObjectPtr<ATSAVCameraActor> Camera;
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
		double LastLiveViscaSendTime = 0.0;
	};

	TSharedRef<SWidget> BuildCameraBank(int32 SlotIndex);
	void RefreshCameraOptions();
	void SetSlotCamera(int32 SlotIndex, ATSAVCameraActor* Camera);
	void LoadSlotFromCamera(int32 SlotIndex);
	void CameraOptionChanged(TSharedPtr<FCameraOption> Item, ESelectInfo::Type SelectionType, int32 SlotIndex);
	TSharedRef<SWidget> GenerateCameraOption(TSharedPtr<FCameraOption> Item) const;
	FText GetSlotCameraText(int32 SlotIndex) const;
	FText GetSlotSummaryText(int32 SlotIndex) const;
	void BeginLiveControl(int32 SlotIndex);
	void UpdateLiveControl(int32 SlotIndex, ELiveControlGroup Group, bool bFinalValue);
	void EndLiveControl(int32 SlotIndex, ELiveControlGroup Group);
	void CommitTypedControl(int32 SlotIndex, ELiveControlGroup Group);
	bool ApplySlot(int32 SlotIndex, bool bSendVisca, bool bUpdateStatus = true);
	FReply RefreshCameras();
	FReply UseSelectedCameras();
	FReply SelectSlotCamera(int32 SlotIndex);
	FReply ApplySlotPreview(int32 SlotIndex);
	FReply ApplySlotAndSend(int32 SlotIndex);
	FReply ApplyAllPreviews();
	FReply ApplyAllAndSend();
	FReply SendSlotHome(int32 SlotIndex);
	FReply SendSlotStop(int32 SlotIndex);
	void SetStatus(const FText& Message, bool bSuccess);
	FSlateColor GetStatusColor() const;
	int32 GetAssignedCameraCount() const;

	TArray<TSharedPtr<FCameraOption>> CameraOptions;
	TArray<FCameraControlSlot> CameraSlots;
	TUniquePtr<FScopedTransaction> ActiveLiveTransaction;
	int32 ActiveLiveSlot = INDEX_NONE;
	FText StatusText;
	bool bStatusSuccess = true;
};
