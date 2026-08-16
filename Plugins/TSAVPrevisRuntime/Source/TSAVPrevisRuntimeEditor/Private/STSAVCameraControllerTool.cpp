// Copyright TSAV. All Rights Reserved.

#include "STSAVCameraControllerTool.h"

#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "STSAVCameraControllerTool"

namespace TSAVCameraControllerTool::Private
{
	FString GetCameraDisplayName(const ATSAVCameraActor* Camera)
	{
		if (!Camera)
		{
			return TEXT("None (clear slot)");
		}
		return FString::Printf(TEXT("%s  |  %d x %d"),
			*Camera->GetActorLabel(), Camera->OutputResolution.X, Camera->OutputResolution.Y);
	}

	FText GetCameraTypeDisplayName(const ETSAVCameraType CameraType)
	{
		if (const UEnum* Enum = StaticEnum<ETSAVCameraType>())
		{
			return Enum->GetDisplayNameTextByValue(static_cast<int64>(CameraType));
		}
		return LOCTEXT("UnknownCameraType", "Unknown");
	}

	struct FLiveControlDelegates
	{
		FSimpleDelegate Begin;
		FSimpleDelegate Changed;
		FSimpleDelegate End;
		FSimpleDelegate Committed;
	};

	TSharedRef<SWidget> MakeFloatControl(
		float* Value,
		const float InputMinimum,
		const float InputMaximum,
		const float SliderMinimum,
		const float SliderMaximum,
		const FLiveControlDelegates Delegates)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SSlider)
				.PreventThrottling(true)
				.OnMouseCaptureBegin(Delegates.Begin)
				.OnMouseCaptureEnd(Delegates.End)
				.Value_Lambda([Value, SliderMinimum, SliderMaximum]()
				{
					return FMath::GetMappedRangeValueClamped(
						FVector2D(SliderMinimum, SliderMaximum), FVector2D(0.0, 1.0), *Value);
				})
				.OnValueChanged_Lambda([Value, SliderMinimum, SliderMaximum, Delegates](const float Normalized)
				{
					*Value = FMath::Lerp(SliderMinimum, SliderMaximum, Normalized);
					Delegates.Changed.ExecuteIfBound();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(105.0f)
				[
					SNew(SNumericEntryBox<float>)
					.MinValue(InputMinimum)
					.MaxValue(InputMaximum)
					.Value_Lambda([Value]() { return *Value; })
					.OnBeginSliderMovement(Delegates.Begin)
					.OnEndSliderMovement_Lambda([Value, InputMinimum, InputMaximum, Delegates](const float NewValue)
					{
						*Value = FMath::Clamp(NewValue, InputMinimum, InputMaximum);
						Delegates.End.ExecuteIfBound();
					})
					.OnValueChanged_Lambda([Value, InputMinimum, InputMaximum, Delegates](const float NewValue)
					{
						*Value = FMath::Clamp(NewValue, InputMinimum, InputMaximum);
						Delegates.Changed.ExecuteIfBound();
					})
					.OnValueCommitted_Lambda([Value, InputMinimum, InputMaximum, Delegates](const float NewValue, ETextCommit::Type)
					{
						*Value = FMath::Clamp(NewValue, InputMinimum, InputMaximum);
						Delegates.Committed.ExecuteIfBound();
					})
				]
			];
	}

	TSharedRef<SWidget> MakeLogFloatControl(
		float* Value,
		const float Minimum,
		const float Maximum,
		const FLiveControlDelegates Delegates)
	{
		const float LogMinimum = FMath::Loge(Minimum);
		const float LogMaximum = FMath::Loge(Maximum);
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SSlider)
				.PreventThrottling(true)
				.OnMouseCaptureBegin(Delegates.Begin)
				.OnMouseCaptureEnd(Delegates.End)
				.Value_Lambda([Value, LogMinimum, LogMaximum]()
				{
					return FMath::GetMappedRangeValueClamped(
						FVector2D(LogMinimum, LogMaximum), FVector2D(0.0, 1.0),
						FMath::Loge(FMath::Max(*Value, 1.0f)));
				})
				.OnValueChanged_Lambda([Value, LogMinimum, LogMaximum, Delegates](const float Normalized)
				{
					*Value = FMath::Exp(FMath::Lerp(LogMinimum, LogMaximum, Normalized));
					Delegates.Changed.ExecuteIfBound();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(105.0f)
				[
					SNew(SNumericEntryBox<float>)
					.MinValue(Minimum)
					.MaxValue(Maximum)
					.Value_Lambda([Value]() { return *Value; })
					.OnBeginSliderMovement(Delegates.Begin)
					.OnEndSliderMovement_Lambda([Value, Minimum, Maximum, Delegates](const float NewValue)
					{
						*Value = FMath::Clamp(NewValue, Minimum, Maximum);
						Delegates.End.ExecuteIfBound();
					})
					.OnValueChanged_Lambda([Value, Minimum, Maximum, Delegates](const float NewValue)
					{
						*Value = FMath::Clamp(NewValue, Minimum, Maximum);
						Delegates.Changed.ExecuteIfBound();
					})
					.OnValueCommitted_Lambda([Value, Minimum, Maximum, Delegates](const float NewValue, ETextCommit::Type)
					{
						*Value = FMath::Clamp(NewValue, Minimum, Maximum);
						Delegates.Committed.ExecuteIfBound();
					})
				]
			];
	}

	TSharedRef<SWidget> MakeDoubleControl(
		double* Value,
		const double InputMinimum,
		const double InputMaximum,
		const double SliderMinimum,
		const double SliderMaximum,
		const FLiveControlDelegates Delegates)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SSlider)
				.PreventThrottling(true)
				.OnMouseCaptureBegin(Delegates.Begin)
				.OnMouseCaptureEnd(Delegates.End)
				.Value_Lambda([Value, SliderMinimum, SliderMaximum]()
				{
					return static_cast<float>(FMath::GetMappedRangeValueClamped(
						FVector2D(SliderMinimum, SliderMaximum), FVector2D(0.0, 1.0), *Value));
				})
				.OnValueChanged_Lambda([Value, SliderMinimum, SliderMaximum, Delegates](const float Normalized)
				{
					*Value = FMath::Lerp(SliderMinimum, SliderMaximum, static_cast<double>(Normalized));
					Delegates.Changed.ExecuteIfBound();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(105.0f)
				[
					SNew(SNumericEntryBox<double>)
					.MinValue(InputMinimum)
					.MaxValue(InputMaximum)
					.Value_Lambda([Value]() { return *Value; })
					.OnBeginSliderMovement(Delegates.Begin)
					.OnEndSliderMovement_Lambda([Value, InputMinimum, InputMaximum, Delegates](const double NewValue)
					{
						*Value = FMath::Clamp(NewValue, InputMinimum, InputMaximum);
						Delegates.End.ExecuteIfBound();
					})
					.OnValueChanged_Lambda([Value, InputMinimum, InputMaximum, Delegates](const double NewValue)
					{
						*Value = FMath::Clamp(NewValue, InputMinimum, InputMaximum);
						Delegates.Changed.ExecuteIfBound();
					})
					.OnValueCommitted_Lambda([Value, InputMinimum, InputMaximum, Delegates](const double NewValue, ETextCommit::Type)
					{
						*Value = FMath::Clamp(NewValue, InputMinimum, InputMaximum);
						Delegates.Committed.ExecuteIfBound();
					})
				]
			];
	}

	TSharedRef<SWidget> MakeIntControl(
		int32* Value,
		const int32 Minimum,
		const int32 Maximum,
		const FLiveControlDelegates Delegates)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SSlider)
				.PreventThrottling(true)
				.OnMouseCaptureBegin(Delegates.Begin)
				.OnMouseCaptureEnd(Delegates.End)
				.Value_Lambda([Value, Minimum, Maximum]()
				{
					return FMath::GetMappedRangeValueClamped(
						FVector2D(Minimum, Maximum), FVector2D(0.0, 1.0), *Value);
				})
				.OnValueChanged_Lambda([Value, Minimum, Maximum, Delegates](const float Normalized)
				{
					*Value = FMath::RoundToInt(FMath::Lerp(static_cast<float>(Minimum), static_cast<float>(Maximum), Normalized));
					Delegates.Changed.ExecuteIfBound();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(105.0f)
				[
					SNew(SNumericEntryBox<int32>)
					.MinValue(Minimum)
					.MaxValue(Maximum)
					.Value_Lambda([Value]() { return *Value; })
					.OnBeginSliderMovement(Delegates.Begin)
					.OnEndSliderMovement_Lambda([Value, Minimum, Maximum, Delegates](const int32 NewValue)
					{
						*Value = FMath::Clamp(NewValue, Minimum, Maximum);
						Delegates.End.ExecuteIfBound();
					})
					.OnValueChanged_Lambda([Value, Minimum, Maximum, Delegates](const int32 NewValue)
					{
						*Value = FMath::Clamp(NewValue, Minimum, Maximum);
						Delegates.Changed.ExecuteIfBound();
					})
					.OnValueCommitted_Lambda([Value, Minimum, Maximum, Delegates](const int32 NewValue, ETextCommit::Type)
					{
						*Value = FMath::Clamp(NewValue, Minimum, Maximum);
						Delegates.Committed.ExecuteIfBound();
					})
				]
			];
	}
}

STSAVCameraControllerTool::~STSAVCameraControllerTool()
{
	ActiveLiveTransaction.Reset();
}

void STSAVCameraControllerTool::Construct(const FArguments& InArgs)
{
	CameraSlots.SetNum(4);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		.Padding(12.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "FOUR-CAMERA CONTROLLER"))
				.Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Description", "Assign up to four cameras. Sliders update the Unreal camera live; VISCA-enabled banks send rate-limited live moves and a final exact value on release. Typed values apply when committed."))
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Refresh", "Refresh Cameras"))
					.OnClicked(this, &STSAVCameraControllerTool::RefreshCameras)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("UseSelected", "Load Selected (Max 4)"))
					.OnClicked(this, &STSAVCameraControllerTool::UseSelectedCameras)
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SSpacer)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ApplyAllPreview", "SYNC ALL NOW"))
					.IsEnabled_Lambda([this]() { return GetAssignedCameraCount() > 0; })
					.OnClicked(this, &STSAVCameraControllerTool::ApplyAllPreviews)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("ApplyAllVisca", "SEND ALL CURRENT"))
					.IsEnabled_Lambda([this]() { return GetAssignedCameraCount() > 0; })
					.OnClicked(this, &STSAVCameraControllerTool::ApplyAllAndSend)
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FMargin(0.0f, 0.0f, 8.0f, 8.0f))
					+ SUniformGridPanel::Slot(0, 0)[BuildCameraBank(0)]
					+ SUniformGridPanel::Slot(1, 0)[BuildCameraBank(1)]
					+ SUniformGridPanel::Slot(0, 1)[BuildCameraBank(2)]
					+ SUniformGridPanel::Slot(1, 1)[BuildCameraBank(3)]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return StatusText; })
				.ColorAndOpacity(this, &STSAVCameraControllerTool::GetStatusColor)
				.AutoWrapText(true)
			]
		]
	];

	RefreshCameraOptions();
}

TSharedRef<SWidget> STSAVCameraControllerTool::BuildCameraBank(const int32 SlotIndex)
{
	FCameraControlSlot& Slot = CameraSlots[SlotIndex];
	auto Label = [](const FText& Text) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock).Text(Text);
	};
	auto LiveDelegates = [this, SlotIndex](const ELiveControlGroup Group)
	{
		TSAVCameraControllerTool::Private::FLiveControlDelegates Delegates;
		Delegates.Begin = FSimpleDelegate::CreateLambda([this, SlotIndex]() { BeginLiveControl(SlotIndex); });
		Delegates.Changed = FSimpleDelegate::CreateLambda([this, SlotIndex, Group]() { UpdateLiveControl(SlotIndex, Group, false); });
		Delegates.End = FSimpleDelegate::CreateLambda([this, SlotIndex, Group]() { EndLiveControl(SlotIndex, Group); });
		Delegates.Committed = FSimpleDelegate::CreateLambda([this, SlotIndex, Group]() { CommitTypedControl(SlotIndex, Group); });
		return Delegates;
	};

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		.Padding(10.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("CameraBankHeading", "CAMERA BANK {0}"), FText::AsNumber(SlotIndex + 1)))
				.Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(Slot.CameraCombo, SComboBox<TSharedPtr<FCameraOption>>)
					.OptionsSource(&CameraOptions)
					.OnGenerateWidget(this, &STSAVCameraControllerTool::GenerateCameraOption)
					.OnSelectionChanged_Lambda([this, SlotIndex](const TSharedPtr<FCameraOption> Item, const ESelectInfo::Type SelectionType)
					{
						CameraOptionChanged(Item, SelectionType, SlotIndex);
					})
					[
						SNew(STextBlock).Text_Lambda([this, SlotIndex]() { return GetSlotCameraText(SlotIndex); })
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("SelectInLevel", "Select"))
					.IsEnabled_Lambda([this, SlotIndex]() { return CameraSlots[SlotIndex].Camera.IsValid(); })
					.OnClicked_Lambda([this, SlotIndex]() { return SelectSlotCamera(SlotIndex); })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 6.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this, SlotIndex]() { return GetSlotSummaryText(SlotIndex); })
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SGridPanel)
				.IsEnabled_Lambda([this, SlotIndex]() { return CameraSlots[SlotIndex].Camera.IsValid(); })
				.FillColumn(1, 1.0f)
				+ SGridPanel::Slot(0, 0).Padding(0.0f, 3.0f, 8.0f, 3.0f).VAlign(VAlign_Center)[Label(LOCTEXT("NameLabel", "NAME"))]
				+ SGridPanel::Slot(1, 0).Padding(0.0f, 3.0f)
				[
					SAssignNew(Slot.CameraNameField, SEditableTextBox)
					.OnTextChanged_Lambda([this, SlotIndex](const FText& Text) { CameraSlots[SlotIndex].CameraName = Text.ToString(); })
					.OnTextCommitted_Lambda([this, SlotIndex](const FText&, ETextCommit::Type) { CommitTypedControl(SlotIndex, ELiveControlGroup::Metadata); })
				]
				+ SGridPanel::Slot(0, 1).Padding(0.0f, 3.0f, 8.0f, 3.0f).VAlign(VAlign_Center)[Label(LOCTEXT("PanLabel", "PAN (DEG)"))]
				+ SGridPanel::Slot(1, 1).Padding(0.0f, 3.0f)[TSAVCameraControllerTool::Private::MakeFloatControl(&Slot.PanDegrees, -170.0f, 170.0f, -170.0f, 170.0f, LiveDelegates(ELiveControlGroup::PTZ))]
				+ SGridPanel::Slot(0, 2).Padding(0.0f, 3.0f, 8.0f, 3.0f).VAlign(VAlign_Center)[Label(LOCTEXT("TiltLabel", "TILT (DEG)"))]
				+ SGridPanel::Slot(1, 2).Padding(0.0f, 3.0f)[TSAVCameraControllerTool::Private::MakeFloatControl(&Slot.TiltDegrees, -30.0f, 90.0f, -30.0f, 90.0f, LiveDelegates(ELiveControlGroup::PTZ))]
				+ SGridPanel::Slot(0, 3).Padding(0.0f, 3.0f, 8.0f, 3.0f).VAlign(VAlign_Center)[Label(LOCTEXT("ZoomLabel", "ZOOM (%)"))]
				+ SGridPanel::Slot(1, 3).Padding(0.0f, 3.0f)[TSAVCameraControllerTool::Private::MakeFloatControl(&Slot.ZoomPercent, 0.0f, 100.0f, 0.0f, 100.0f, LiveDelegates(ELiveControlGroup::PTZ))]
				+ SGridPanel::Slot(0, 4).Padding(0.0f, 3.0f, 8.0f, 3.0f).VAlign(VAlign_Center)[Label(LOCTEXT("IrisLabel", "IRIS (F)"))]
				+ SGridPanel::Slot(1, 4).Padding(0.0f, 3.0f)[TSAVCameraControllerTool::Private::MakeFloatControl(&Slot.Iris, 0.7f, 64.0f, 0.7f, 22.0f, LiveDelegates(ELiveControlGroup::Image))]
				+ SGridPanel::Slot(0, 5).Padding(0.0f, 3.0f, 8.0f, 3.0f).VAlign(VAlign_Center)[Label(LOCTEXT("FocusLabel", "FOCUS (CM)"))]
				+ SGridPanel::Slot(1, 5).Padding(0.0f, 3.0f)[TSAVCameraControllerTool::Private::MakeLogFloatControl(&Slot.FocusDistanceCm, 1.0f, 1000000.0f, LiveDelegates(ELiveControlGroup::Image))]
				+ SGridPanel::Slot(0, 6).Padding(0.0f, 3.0f, 8.0f, 3.0f).VAlign(VAlign_Center)[Label(LOCTEXT("GainLabel", "GAIN (DB)"))]
				+ SGridPanel::Slot(1, 6).Padding(0.0f, 3.0f)[TSAVCameraControllerTool::Private::MakeFloatControl(&Slot.GainDb, -12.0f, 36.0f, -12.0f, 36.0f, LiveDelegates(ELiveControlGroup::Image))]
				+ SGridPanel::Slot(0, 7).Padding(0.0f, 3.0f, 8.0f, 3.0f).VAlign(VAlign_Center)[Label(LOCTEXT("PositionXLabel", "POSITION X"))]
				+ SGridPanel::Slot(1, 7).Padding(0.0f, 3.0f)[TSAVCameraControllerTool::Private::MakeDoubleControl(&Slot.WorldPosition.X, -1000000.0, 1000000.0, -100000.0, 100000.0, LiveDelegates(ELiveControlGroup::Position))]
				+ SGridPanel::Slot(0, 8).Padding(0.0f, 3.0f, 8.0f, 3.0f).VAlign(VAlign_Center)[Label(LOCTEXT("PositionYLabel", "POSITION Y"))]
				+ SGridPanel::Slot(1, 8).Padding(0.0f, 3.0f)[TSAVCameraControllerTool::Private::MakeDoubleControl(&Slot.WorldPosition.Y, -1000000.0, 1000000.0, -100000.0, 100000.0, LiveDelegates(ELiveControlGroup::Position))]
				+ SGridPanel::Slot(0, 9).Padding(0.0f, 3.0f, 8.0f, 3.0f).VAlign(VAlign_Center)[Label(LOCTEXT("PositionZLabel", "POSITION Z"))]
				+ SGridPanel::Slot(1, 9).Padding(0.0f, 3.0f)[TSAVCameraControllerTool::Private::MakeDoubleControl(&Slot.WorldPosition.Z, -1000000.0, 1000000.0, -100000.0, 100000.0, LiveDelegates(ELiveControlGroup::Position))]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
			[
				SNew(SSeparator)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ViscaHeading", "VISCA OVER IP"))
				.Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
			[
				SNew(SGridPanel)
				.IsEnabled_Lambda([this, SlotIndex]() { return CameraSlots[SlotIndex].Camera.IsValid(); })
				.FillColumn(1, 1.0f)
				+ SGridPanel::Slot(0, 0).Padding(0.0f, 3.0f, 8.0f, 3.0f).VAlign(VAlign_Center)[Label(LOCTEXT("ViscaEnabledLabel", "ENABLED"))]
				+ SGridPanel::Slot(1, 0).Padding(0.0f, 3.0f)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this, SlotIndex]() { return CameraSlots[SlotIndex].bViscaEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this, SlotIndex](const ECheckBoxState State)
					{
						CameraSlots[SlotIndex].bViscaEnabled = State == ECheckBoxState::Checked;
						CommitTypedControl(SlotIndex, ELiveControlGroup::Connection);
					})
					[
						SNew(STextBlock).Text(LOCTEXT("ViscaUdp", "Sony VISCA UDP"))
					]
				]
				+ SGridPanel::Slot(0, 1).Padding(0.0f, 3.0f, 8.0f, 3.0f).VAlign(VAlign_Center)[Label(LOCTEXT("ViscaIpLabel", "IP ADDRESS"))]
				+ SGridPanel::Slot(1, 1).Padding(0.0f, 3.0f)
				[
					SAssignNew(Slot.ViscaIpField, SEditableTextBox)
					.OnTextChanged_Lambda([this, SlotIndex](const FText& Text) { CameraSlots[SlotIndex].ViscaIpAddress = Text.ToString(); })
					.OnTextCommitted_Lambda([this, SlotIndex](const FText&, ETextCommit::Type) { CommitTypedControl(SlotIndex, ELiveControlGroup::Connection); })
				]
				+ SGridPanel::Slot(0, 2).Padding(0.0f, 3.0f, 8.0f, 3.0f).VAlign(VAlign_Center)[Label(LOCTEXT("ViscaPortLabel", "UDP PORT"))]
				+ SGridPanel::Slot(1, 2).Padding(0.0f, 3.0f)[TSAVCameraControllerTool::Private::MakeIntControl(&Slot.ViscaPort, 1, 65535, LiveDelegates(ELiveControlGroup::Connection))]
				+ SGridPanel::Slot(0, 3).Padding(0.0f, 3.0f, 8.0f, 3.0f).VAlign(VAlign_Center)[Label(LOCTEXT("PanSpeedLabel", "PAN SPEED"))]
				+ SGridPanel::Slot(1, 3).Padding(0.0f, 3.0f)[TSAVCameraControllerTool::Private::MakeIntControl(&Slot.ViscaPanSpeed, 1, 24, LiveDelegates(ELiveControlGroup::Connection))]
				+ SGridPanel::Slot(0, 4).Padding(0.0f, 3.0f, 8.0f, 3.0f).VAlign(VAlign_Center)[Label(LOCTEXT("TiltSpeedLabel", "TILT SPEED"))]
				+ SGridPanel::Slot(1, 4).Padding(0.0f, 3.0f)[TSAVCameraControllerTool::Private::MakeIntControl(&Slot.ViscaTiltSpeed, 1, 20, LiveDelegates(ELiveControlGroup::Connection))]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ApplyPreview", "SYNC NOW"))
					.HAlign(HAlign_Center)
					.IsEnabled_Lambda([this, SlotIndex]() { return CameraSlots[SlotIndex].Camera.IsValid(); })
					.OnClicked_Lambda([this, SlotIndex]() { return ApplySlotPreview(SlotIndex); })
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ApplySend", "SEND CURRENT"))
					.HAlign(HAlign_Center)
					.IsEnabled_Lambda([this, SlotIndex]() { return CameraSlots[SlotIndex].Camera.IsValid(); })
					.OnClicked_Lambda([this, SlotIndex]() { return ApplySlotAndSend(SlotIndex); })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ViscaHome", "VISCA HOME"))
					.HAlign(HAlign_Center)
					.IsEnabled_Lambda([this, SlotIndex]() { return CameraSlots[SlotIndex].Camera.IsValid() && CameraSlots[SlotIndex].bViscaEnabled; })
					.OnClicked_Lambda([this, SlotIndex]() { return SendSlotHome(SlotIndex); })
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ViscaStop", "VISCA STOP"))
					.HAlign(HAlign_Center)
					.IsEnabled_Lambda([this, SlotIndex]() { return CameraSlots[SlotIndex].Camera.IsValid() && CameraSlots[SlotIndex].bViscaEnabled; })
					.OnClicked_Lambda([this, SlotIndex]() { return SendSlotStop(SlotIndex); })
				]
			]
		];
}

void STSAVCameraControllerTool::RefreshCameraOptions()
{
	TArray<TWeakObjectPtr<ATSAVCameraActor>> Previous;
	Previous.Reserve(CameraSlots.Num());
	for (const FCameraControlSlot& Slot : CameraSlots)
	{
		Previous.Add(Slot.Camera);
	}

	CameraOptions.Reset();
	CameraOptions.Add(MakeShared<FCameraOption>());
	TArray<ATSAVCameraActor*> Cameras;
	if (GEditor)
	{
		if (UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			for (TActorIterator<ATSAVCameraActor> It(World); It; ++It)
			{
				Cameras.Add(*It);
			}
		}
	}
	Cameras.Sort([](const ATSAVCameraActor& Left, const ATSAVCameraActor& Right)
	{
		return Left.GetActorLabel() < Right.GetActorLabel();
	});
	for (ATSAVCameraActor* Camera : Cameras)
	{
		CameraOptions.Add(MakeShared<FCameraOption>(Camera));
	}

	for (FCameraControlSlot& Slot : CameraSlots)
	{
		if (Slot.CameraCombo)
		{
			Slot.CameraCombo->RefreshOptions();
		}
		Slot.Camera.Reset();
	}

	TSet<ATSAVCameraActor*> Assigned;
	for (int32 Index = 0; Index < CameraSlots.Num(); ++Index)
	{
		ATSAVCameraActor* Camera = Previous.IsValidIndex(Index) ? Previous[Index].Get() : nullptr;
		if (Camera && Cameras.Contains(Camera) && !Assigned.Contains(Camera))
		{
			CameraSlots[Index].Camera = Camera;
			Assigned.Add(Camera);
		}
	}
	if (Assigned.IsEmpty())
	{
		for (int32 Index = 0; Index < CameraSlots.Num() && Index < Cameras.Num(); ++Index)
		{
			CameraSlots[Index].Camera = Cameras[Index];
			Assigned.Add(Cameras[Index]);
		}
	}

	for (int32 Index = 0; Index < CameraSlots.Num(); ++Index)
	{
		ATSAVCameraActor* Camera = CameraSlots[Index].Camera.Get();
		const TSharedPtr<FCameraOption>* Match = CameraOptions.FindByPredicate([Camera](const TSharedPtr<FCameraOption>& Item)
		{
			return Item.IsValid() && Item->Get() == Camera;
		});
		if (CameraSlots[Index].CameraCombo && Match)
		{
			CameraSlots[Index].CameraCombo->SetSelectedItem(*Match);
		}
		LoadSlotFromCamera(Index);
	}

	SetStatus(FText::Format(
		LOCTEXT("Refreshed", "Found {0} camera(s); {1} assigned to controller banks."),
		FText::AsNumber(Cameras.Num()), FText::AsNumber(GetAssignedCameraCount())), !Cameras.IsEmpty());
}

void STSAVCameraControllerTool::SetSlotCamera(const int32 SlotIndex, ATSAVCameraActor* Camera)
{
	if (!CameraSlots.IsValidIndex(SlotIndex))
	{
		return;
	}
	if (Camera)
	{
		for (int32 OtherIndex = 0; OtherIndex < CameraSlots.Num(); ++OtherIndex)
		{
			if (OtherIndex != SlotIndex && CameraSlots[OtherIndex].Camera.Get() == Camera)
			{
				CameraSlots[OtherIndex].Camera.Reset();
				LoadSlotFromCamera(OtherIndex);
				if (CameraSlots[OtherIndex].CameraCombo && !CameraOptions.IsEmpty())
				{
					CameraSlots[OtherIndex].CameraCombo->SetSelectedItem(CameraOptions[0]);
				}
			}
		}
	}
	CameraSlots[SlotIndex].Camera = Camera;
	LoadSlotFromCamera(SlotIndex);
}

void STSAVCameraControllerTool::LoadSlotFromCamera(const int32 SlotIndex)
{
	if (!CameraSlots.IsValidIndex(SlotIndex))
	{
		return;
	}
	FCameraControlSlot& Slot = CameraSlots[SlotIndex];
	ATSAVCameraActor* Camera = Slot.Camera.Get();
	if (!Camera)
	{
		Slot.CameraName.Reset();
		if (Slot.CameraNameField) { Slot.CameraNameField->SetText(FText::GetEmpty()); }
		if (Slot.ViscaIpField) { Slot.ViscaIpField->SetText(FText::GetEmpty()); }
		return;
	}
	Slot.CameraName = Camera->GetActorLabel();
	Slot.PanDegrees = Camera->PanDegrees;
	Slot.TiltDegrees = Camera->TiltDegrees;
	Slot.ZoomPercent = Camera->ZoomNormalized * 100.0f;
	Slot.Iris = Camera->Aperture;
	Slot.FocusDistanceCm = Camera->FocusDistanceCm;
	Slot.GainDb = Camera->GainDb;
	Slot.WorldPosition = Camera->GetActorLocation();
	Slot.bViscaEnabled = Camera->bEnableViscaOverIp;
	Slot.ViscaIpAddress = Camera->ViscaIpAddress;
	Slot.ViscaPort = Camera->ViscaPort;
	Slot.ViscaPanSpeed = Camera->ViscaPanSpeed;
	Slot.ViscaTiltSpeed = Camera->ViscaTiltSpeed;
	if (Slot.CameraNameField) { Slot.CameraNameField->SetText(FText::FromString(Slot.CameraName)); }
	if (Slot.ViscaIpField) { Slot.ViscaIpField->SetText(FText::FromString(Slot.ViscaIpAddress)); }
}

void STSAVCameraControllerTool::CameraOptionChanged(
	const TSharedPtr<FCameraOption> Item,
	ESelectInfo::Type SelectionType,
	const int32 SlotIndex)
{
	SetSlotCamera(SlotIndex, Item.IsValid() ? Item->Get() : nullptr);
}

TSharedRef<SWidget> STSAVCameraControllerTool::GenerateCameraOption(const TSharedPtr<FCameraOption> Item) const
{
	return SNew(STextBlock).Text(FText::FromString(
		TSAVCameraControllerTool::Private::GetCameraDisplayName(Item.IsValid() ? Item->Get() : nullptr)));
}

FText STSAVCameraControllerTool::GetSlotCameraText(const int32 SlotIndex) const
{
	return FText::FromString(TSAVCameraControllerTool::Private::GetCameraDisplayName(
		CameraSlots.IsValidIndex(SlotIndex) ? CameraSlots[SlotIndex].Camera.Get() : nullptr));
}

FText STSAVCameraControllerTool::GetSlotSummaryText(const int32 SlotIndex) const
{
	const ATSAVCameraActor* Camera = CameraSlots.IsValidIndex(SlotIndex) ? CameraSlots[SlotIndex].Camera.Get() : nullptr;
	if (!Camera)
	{
		return LOCTEXT("EmptyBank", "No camera assigned");
	}
	return FText::Format(
		LOCTEXT("CameraSummary", "{0}  |  {1} x {2}  |  VISCA {3}"),
		TSAVCameraControllerTool::Private::GetCameraTypeDisplayName(Camera->CameraType),
		FText::AsNumber(Camera->OutputResolution.X), FText::AsNumber(Camera->OutputResolution.Y),
		Camera->bEnableViscaOverIp ? LOCTEXT("ViscaOn", "On") : LOCTEXT("ViscaOff", "Off"));
}

void STSAVCameraControllerTool::BeginLiveControl(const int32 SlotIndex)
{
	ATSAVCameraActor* Camera = CameraSlots.IsValidIndex(SlotIndex) ? CameraSlots[SlotIndex].Camera.Get() : nullptr;
	if (!Camera)
	{
		return;
	}
	ActiveLiveTransaction.Reset();
	ActiveLiveSlot = SlotIndex;
	ActiveLiveTransaction = MakeUnique<FScopedTransaction>(FText::Format(
		LOCTEXT("LiveControlTransaction", "Live Control Camera {0}"), FText::FromString(Camera->GetActorLabel())));
	Camera->Modify();
}

void STSAVCameraControllerTool::UpdateLiveControl(
	const int32 SlotIndex,
	const ELiveControlGroup Group,
	const bool bFinalValue)
{
	if (!CameraSlots.IsValidIndex(SlotIndex))
	{
		return;
	}
	FCameraControlSlot& Slot = CameraSlots[SlotIndex];
	ATSAVCameraActor* Camera = Slot.Camera.Get();
	if (!Camera)
	{
		return;
	}

	Camera->bEnableViscaOverIp = Slot.bViscaEnabled;
	Camera->ViscaIpAddress = Slot.ViscaIpField
		? Slot.ViscaIpField->GetText().ToString().TrimStartAndEnd()
		: Slot.ViscaIpAddress.TrimStartAndEnd();
	Camera->ViscaPort = FMath::Clamp(Slot.ViscaPort, 1, 65535);
	Camera->ViscaPanSpeed = FMath::Clamp(Slot.ViscaPanSpeed, 1, 24);
	Camera->ViscaTiltSpeed = FMath::Clamp(Slot.ViscaTiltSpeed, 1, 20);

	switch (Group)
	{
	case ELiveControlGroup::PTZ:
		Camera->ApplyPTZ(Slot.PanDegrees, Slot.TiltDegrees, Slot.ZoomPercent / 100.0f, false);
		break;
	case ELiveControlGroup::Image:
		Camera->ApplyImageControls(Slot.Iris, Slot.FocusDistanceCm, Slot.GainDb, false);
		break;
	case ELiveControlGroup::Position:
		Camera->SetActorLocation(Slot.WorldPosition, false, nullptr, ETeleportType::TeleportPhysics);
		break;
	case ELiveControlGroup::Connection:
		break;
	case ELiveControlGroup::Metadata:
	{
		FString ResolvedName = Slot.CameraNameField
			? Slot.CameraNameField->GetText().ToString().TrimStartAndEnd()
			: Slot.CameraName.TrimStartAndEnd();
		if (!ResolvedName.IsEmpty())
		{
			Camera->SetActorLabel(ResolvedName);
			Camera->CameraLabel = FText::FromString(ResolvedName);
			Slot.CameraName = ResolvedName;
		}
		break;
	}
	}

	bool bViscaSent = true;
	const bool bSendsVisca = Camera->bEnableViscaOverIp
		&& (Group == ELiveControlGroup::PTZ || Group == ELiveControlGroup::Image);
	if (bSendsVisca)
	{
		const double Now = FPlatformTime::Seconds();
		if (bFinalValue || Now - Slot.LastLiveViscaSendTime >= 0.05)
		{
			bViscaSent = Group == ELiveControlGroup::PTZ
				? Camera->SendViscaPtzControls()
				: Camera->SendViscaImageControls();
			Slot.LastLiveViscaSendTime = Now;
		}
	}

	Camera->MarkPackageDirty();
	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}
	if (bFinalValue)
	{
		Camera->PostEditChange();
		SetStatus(FText::Format(
			bSendsVisca
				? LOCTEXT("LiveControlFinalVisca", "Bank {0}: live value applied and final VISCA command sent.")
				: LOCTEXT("LiveControlFinalPreview", "Bank {0}: live value applied."),
			FText::AsNumber(SlotIndex + 1)), !bSendsVisca || bViscaSent);
	}
}

void STSAVCameraControllerTool::EndLiveControl(const int32 SlotIndex, const ELiveControlGroup Group)
{
	UpdateLiveControl(SlotIndex, Group, true);
	ActiveLiveTransaction.Reset();
	ActiveLiveSlot = INDEX_NONE;
}

void STSAVCameraControllerTool::CommitTypedControl(const int32 SlotIndex, const ELiveControlGroup Group)
{
	ATSAVCameraActor* Camera = CameraSlots.IsValidIndex(SlotIndex) ? CameraSlots[SlotIndex].Camera.Get() : nullptr;
	if (!Camera)
	{
		return;
	}
	const FScopedTransaction Transaction(FText::Format(
		LOCTEXT("CommitLiveControlTransaction", "Set Camera {0} Control"), FText::FromString(Camera->GetActorLabel())));
	Camera->Modify();
	UpdateLiveControl(SlotIndex, Group, true);
}

bool STSAVCameraControllerTool::ApplySlot(const int32 SlotIndex, const bool bSendVisca, const bool bUpdateStatus)
{
	if (!CameraSlots.IsValidIndex(SlotIndex))
	{
		return false;
	}
	FCameraControlSlot& Slot = CameraSlots[SlotIndex];
	ATSAVCameraActor* Camera = Slot.Camera.Get();
	if (!Camera)
	{
		return false;
	}
	FString ResolvedName = Slot.CameraNameField
		? Slot.CameraNameField->GetText().ToString().TrimStartAndEnd()
		: Slot.CameraName.TrimStartAndEnd();
	if (ResolvedName.IsEmpty())
	{
		ResolvedName = Camera->GetActorLabel();
	}
	Slot.ViscaIpAddress = Slot.ViscaIpField
		? Slot.ViscaIpField->GetText().ToString().TrimStartAndEnd()
		: Slot.ViscaIpAddress.TrimStartAndEnd();

	const FScopedTransaction Transaction(FText::Format(
		LOCTEXT("ApplyTransaction", "Control Camera {0}"), FText::FromString(Camera->GetActorLabel())));
	Camera->Modify();
	Camera->SetActorLabel(ResolvedName);
	Camera->CameraLabel = FText::FromString(ResolvedName);
	Camera->bEnableViscaOverIp = Slot.bViscaEnabled;
	Camera->ViscaIpAddress = Slot.ViscaIpAddress;
	Camera->ViscaPort = FMath::Clamp(Slot.ViscaPort, 1, 65535);
	Camera->ViscaPanSpeed = FMath::Clamp(Slot.ViscaPanSpeed, 1, 24);
	Camera->ViscaTiltSpeed = FMath::Clamp(Slot.ViscaTiltSpeed, 1, 20);
	Camera->SetActorLocation(Slot.WorldPosition, false, nullptr, ETeleportType::TeleportPhysics);
	Camera->ApplyPTZ(Slot.PanDegrees, Slot.TiltDegrees, Slot.ZoomPercent / 100.0f, false);
	Camera->ApplyImageControls(Slot.Iris, Slot.FocusDistanceCm, Slot.GainDb, false);
	Camera->PostEditChange();
	Camera->MarkPackageDirty();

	bool bSent = true;
	if (bSendVisca)
	{
		bSent = Camera->bEnableViscaOverIp
			&& Camera->SendViscaPtzControls()
			&& Camera->SendViscaImageControls();
	}
	LoadSlotFromCamera(SlotIndex);
	if (bUpdateStatus)
	{
		if (bSendVisca && !bSent)
		{
			SetStatus(FText::Format(
				LOCTEXT("ViscaSendFailed", "Updated bank {0}, but VISCA could not send. Check Enabled, IPv4 address, and UDP port."),
				FText::AsNumber(SlotIndex + 1)), false);
		}
		else
		{
			SetStatus(FText::Format(
				bSendVisca
					? LOCTEXT("AppliedAndSent", "Updated {0} and sent all controls over VISCA UDP.")
					: LOCTEXT("AppliedPreview", "Updated the Unreal preview for {0}; no network commands were sent."),
				FText::FromString(ResolvedName)), true);
		}
	}
	return !bSendVisca || bSent;
}

FReply STSAVCameraControllerTool::RefreshCameras()
{
	RefreshCameraOptions();
	return FReply::Handled();
}

FReply STSAVCameraControllerTool::UseSelectedCameras()
{
	if (!GEditor || !GEditor->GetSelectedActors())
	{
		return FReply::Handled();
	}
	TArray<ATSAVCameraActor*> Selected;
	for (FSelectionIterator It(*GEditor->GetSelectedActors()); It && Selected.Num() < 4; ++It)
	{
		if (ATSAVCameraActor* Camera = Cast<ATSAVCameraActor>(*It))
		{
			Selected.AddUnique(Camera);
		}
	}
	if (Selected.IsEmpty())
	{
		SetStatus(LOCTEXT("NoSelectedCameras", "Select one to four TSAV Production Cameras in the level first."), false);
		return FReply::Handled();
	}
	for (int32 Index = 0; Index < CameraSlots.Num(); ++Index)
	{
		ATSAVCameraActor* Camera = Selected.IsValidIndex(Index) ? Selected[Index] : nullptr;
		CameraSlots[Index].Camera = Camera;
		const TSharedPtr<FCameraOption>* Match = CameraOptions.FindByPredicate([Camera](const TSharedPtr<FCameraOption>& Item)
		{
			return Item.IsValid() && Item->Get() == Camera;
		});
		if (CameraSlots[Index].CameraCombo && Match)
		{
			CameraSlots[Index].CameraCombo->SetSelectedItem(*Match);
		}
		LoadSlotFromCamera(Index);
	}
	SetStatus(FText::Format(LOCTEXT("LoadedSelected", "Loaded {0} selected camera(s) into the controller banks."), FText::AsNumber(Selected.Num())), true);
	return FReply::Handled();
}

FReply STSAVCameraControllerTool::SelectSlotCamera(const int32 SlotIndex)
{
	ATSAVCameraActor* Camera = CameraSlots.IsValidIndex(SlotIndex) ? CameraSlots[SlotIndex].Camera.Get() : nullptr;
	if (GEditor && Camera)
	{
		GEditor->SelectNone(false, true, false);
		GEditor->SelectActor(Camera, true, true, true);
	}
	return FReply::Handled();
}

FReply STSAVCameraControllerTool::ApplySlotPreview(const int32 SlotIndex)
{
	ApplySlot(SlotIndex, false);
	return FReply::Handled();
}

FReply STSAVCameraControllerTool::ApplySlotAndSend(const int32 SlotIndex)
{
	ApplySlot(SlotIndex, true);
	return FReply::Handled();
}

FReply STSAVCameraControllerTool::ApplyAllPreviews()
{
	const FScopedTransaction Transaction(LOCTEXT("ApplyAllPreviewTransaction", "Control All Assigned Cameras"));
	int32 Applied = 0;
	for (int32 Index = 0; Index < CameraSlots.Num(); ++Index)
	{
		if (CameraSlots[Index].Camera.IsValid() && ApplySlot(Index, false, false))
		{
			++Applied;
		}
	}
	SetStatus(FText::Format(
		LOCTEXT("AppliedAllPreview", "Updated {0} camera preview(s); no network commands were sent."),
		FText::AsNumber(Applied)), Applied > 0);
	return FReply::Handled();
}

FReply STSAVCameraControllerTool::ApplyAllAndSend()
{
	const FScopedTransaction Transaction(LOCTEXT("ApplyAllViscaTransaction", "Control All Assigned Cameras and Send VISCA"));
	int32 Applied = 0;
	int32 Failed = 0;
	for (int32 Index = 0; Index < CameraSlots.Num(); ++Index)
	{
		if (!CameraSlots[Index].Camera.IsValid())
		{
			continue;
		}
		++Applied;
		if (!ApplySlot(Index, true, false))
		{
			++Failed;
		}
	}
	SetStatus(FText::Format(
		Failed == 0
			? LOCTEXT("AppliedAllVisca", "Updated and sent VISCA controls to {0} camera(s).")
			: LOCTEXT("AppliedAllViscaFailed", "Updated {0} camera(s), but VISCA send failed for {1}. Check each bank's connection settings."),
		FText::AsNumber(Applied), FText::AsNumber(Failed)), Applied > 0 && Failed == 0);
	return FReply::Handled();
}

FReply STSAVCameraControllerTool::SendSlotHome(const int32 SlotIndex)
{
	if (ApplySlot(SlotIndex, false, false))
	{
		ATSAVCameraActor* Camera = CameraSlots[SlotIndex].Camera.Get();
		const bool bSent = Camera && Camera->SendViscaHome();
		SetStatus(bSent
			? FText::Format(LOCTEXT("HomeSent", "Bank {0}: VISCA Home sent."), FText::AsNumber(SlotIndex + 1))
			: FText::Format(LOCTEXT("HomeFailed", "Bank {0}: VISCA Home could not send."), FText::AsNumber(SlotIndex + 1)), bSent);
	}
	return FReply::Handled();
}

FReply STSAVCameraControllerTool::SendSlotStop(const int32 SlotIndex)
{
	if (ApplySlot(SlotIndex, false, false))
	{
		ATSAVCameraActor* Camera = CameraSlots[SlotIndex].Camera.Get();
		const bool bSent = Camera && Camera->SendViscaStop();
		SetStatus(bSent
			? FText::Format(LOCTEXT("StopSent", "Bank {0}: VISCA Stop sent."), FText::AsNumber(SlotIndex + 1))
			: FText::Format(LOCTEXT("StopFailed", "Bank {0}: VISCA Stop could not send."), FText::AsNumber(SlotIndex + 1)), bSent);
	}
	return FReply::Handled();
}

void STSAVCameraControllerTool::SetStatus(const FText& Message, const bool bSuccess)
{
	StatusText = Message;
	bStatusSuccess = bSuccess;
}

FSlateColor STSAVCameraControllerTool::GetStatusColor() const
{
	return bStatusSuccess ? FLinearColor(0.35f, 0.85f, 0.45f) : FLinearColor(1.0f, 0.55f, 0.20f);
}

int32 STSAVCameraControllerTool::GetAssignedCameraCount() const
{
	int32 Count = 0;
	for (const FCameraControlSlot& Slot : CameraSlots)
	{
		Count += Slot.Camera.IsValid() ? 1 : 0;
	}
	return Count;
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTSAVCameraControllerWidgetConstructionTest,
	"TSAV.Editor.CameraController.ConstructsFourBanks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTSAVCameraControllerWidgetConstructionTest::RunTest(const FString& Parameters)
{
	const TSharedRef<STSAVCameraControllerTool> Controller = SNew(STSAVCameraControllerTool);
	TestEqual(TEXT("Camera controller has a root widget"), Controller->GetChildren()->Num(), 1);
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Editor world is available"), World))
	{
		return false;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient | RF_Transactional;
	ATSAVCameraActor* Camera = World->SpawnActor<ATSAVCameraActor>(
		ATSAVCameraActor::StaticClass(), FTransform::Identity, SpawnParameters);
	if (!TestNotNull(TEXT("Transient live-control camera was created"), Camera))
	{
		return false;
	}

	Controller->SetSlotCamera(0, Camera);
	STSAVCameraControllerTool::FCameraControlSlot& Slot = Controller->CameraSlots[0];
	Slot.bViscaEnabled = false;
	Slot.PanDegrees = 42.0f;
	Slot.TiltDegrees = 18.0f;
	Slot.ZoomPercent = 65.0f;
	Controller->BeginLiveControl(0);
	Controller->UpdateLiveControl(0, STSAVCameraControllerTool::ELiveControlGroup::PTZ, false);
	Controller->EndLiveControl(0, STSAVCameraControllerTool::ELiveControlGroup::PTZ);
	TestTrue(TEXT("Live pan applied"), FMath::IsNearlyEqual(Camera->PanDegrees, 42.0f));
	TestTrue(TEXT("Live tilt applied"), FMath::IsNearlyEqual(Camera->TiltDegrees, 18.0f));
	TestTrue(TEXT("Live zoom applied"), FMath::IsNearlyEqual(Camera->ZoomNormalized, 0.65f));

	Slot.Iris = 4.0f;
	Slot.FocusDistanceCm = 2500.0f;
	Slot.GainDb = 6.0f;
	Controller->CommitTypedControl(0, STSAVCameraControllerTool::ELiveControlGroup::Image);
	TestTrue(TEXT("Live iris applied"), FMath::IsNearlyEqual(Camera->Aperture, 4.0f));
	TestTrue(TEXT("Live focus applied"), FMath::IsNearlyEqual(Camera->FocusDistanceCm, 2500.0f));
	TestTrue(TEXT("Live gain applied"), FMath::IsNearlyEqual(Camera->GainDb, 6.0f));

	Slot.WorldPosition = FVector(100.0, 200.0, 300.0);
	Controller->CommitTypedControl(0, STSAVCameraControllerTool::ELiveControlGroup::Position);
	TestTrue(TEXT("Live position applied"), Camera->GetActorLocation().Equals(Slot.WorldPosition));
	Camera->Destroy();
	return true;
}
#endif

#undef LOCTEXT_NAMESPACE
