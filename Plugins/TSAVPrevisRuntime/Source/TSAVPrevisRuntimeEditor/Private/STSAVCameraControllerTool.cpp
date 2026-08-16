// Copyright TSAV. All Rights Reserved.

#include "STSAVCameraControllerTool.h"

#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "STSAVCameraControllerTool"

namespace TSAVCameraControllerTool::Private
{
	FString GetCameraDisplayName(const ATSAVCameraActor* Camera)
	{
		if (!Camera)
		{
			return TEXT("None");
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
}

void STSAVCameraControllerTool::Construct(const FArguments& InArgs)
{
	auto MakeFloatField = [](float* Value, const float Minimum, const float Maximum) -> TSharedRef<SWidget>
	{
		return SNew(SNumericEntryBox<float>)
			.MinValue(Minimum)
			.MaxValue(Maximum)
			.MinSliderValue(Minimum)
			.MaxSliderValue(Maximum)
			.Value_Lambda([Value]() { return *Value; })
			.OnValueChanged_Lambda([Value, Minimum, Maximum](const float NewValue)
			{
				*Value = FMath::Clamp(NewValue, Minimum, Maximum);
			})
			.OnValueCommitted_Lambda([Value, Minimum, Maximum](const float NewValue, ETextCommit::Type)
			{
				*Value = FMath::Clamp(NewValue, Minimum, Maximum);
			});
	};

	auto MakePositionField = [](const FText& AxisLabel, double* Value) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(AxisLabel)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SNumericEntryBox<double>)
				.MinValue(-1000000.0)
				.MaxValue(1000000.0)
				.Value_Lambda([Value]() { return *Value; })
				.OnValueChanged_Lambda([Value](const double NewValue)
				{
					*Value = FMath::Clamp(NewValue, -1000000.0, 1000000.0);
				})
				.OnValueCommitted_Lambda([Value](const double NewValue, ETextCommit::Type)
				{
					*Value = FMath::Clamp(NewValue, -1000000.0, 1000000.0);
				})
			];
	};

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		.Padding(12.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title", "CAMERA CONTROLLER"))
					.Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Description", "Control every TSAV camera from one panel. Preview changes locally or send PTZ and image controls to a physical camera using VISCA over IP."))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(CameraCombo, SComboBox<TSharedPtr<FCameraOption>>)
						.OptionsSource(&CameraOptions)
						.OnGenerateWidget(this, &STSAVCameraControllerTool::GenerateCameraOption)
						.OnSelectionChanged(this, &STSAVCameraControllerTool::CameraOptionChanged)
						[
							SNew(STextBlock).Text(this, &STSAVCameraControllerTool::GetActiveCameraText)
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Refresh", "Refresh"))
						.OnClicked(this, &STSAVCameraControllerTool::RefreshCameras)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("UseSelected", "Use Selected"))
						.OnClicked(this, &STSAVCameraControllerTool::UseSelectedCamera)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("SelectLevel", "Select"))
						.IsEnabled_Lambda([this]() { return ActiveCamera.IsValid(); })
						.OnClicked(this, &STSAVCameraControllerTool::SelectCameraInLevel)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(this, &STSAVCameraControllerTool::GetCameraSummaryText)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 12.0f)
				[
					SNew(SSeparator)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CameraControlsHeading", "CAMERA CONTROLS"))
					.Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(SGridPanel)
					.IsEnabled_Lambda([this]() { return ActiveCamera.IsValid(); })
					.FillColumn(1, 1.0f)
					+ SGridPanel::Slot(0, 0).Padding(0.0f, 4.0f, 12.0f, 4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("NameLabel", "CAMERA NAME"))
					]
					+ SGridPanel::Slot(1, 0).Padding(0.0f, 4.0f)
					[
						SAssignNew(CameraNameField, SEditableTextBox)
						.OnTextChanged_Lambda([this](const FText& Text) { CameraName = Text.ToString(); })
					]
					+ SGridPanel::Slot(0, 1).Padding(0.0f, 4.0f, 12.0f, 4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("PanTiltLabel", "PAN / TILT (DEG)"))
					]
					+ SGridPanel::Slot(1, 1).Padding(0.0f, 4.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)[MakeFloatField(&PanDegrees, -170.0f, 170.0f)]
						+ SHorizontalBox::Slot().FillWidth(1.0f)[MakeFloatField(&TiltDegrees, -30.0f, 90.0f)]
					]
					+ SGridPanel::Slot(0, 2).Padding(0.0f, 4.0f, 12.0f, 4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("ZoomLabel", "ZOOM (%)"))
					]
					+ SGridPanel::Slot(1, 2).Padding(0.0f, 4.0f)
					[
						MakeFloatField(&ZoomPercent, 0.0f, 100.0f)
					]
					+ SGridPanel::Slot(0, 3).Padding(0.0f, 4.0f, 12.0f, 4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("IrisLabel", "IRIS (F-STOP)"))
					]
					+ SGridPanel::Slot(1, 3).Padding(0.0f, 4.0f)
					[
						MakeFloatField(&Iris, 0.7f, 64.0f)
					]
					+ SGridPanel::Slot(0, 4).Padding(0.0f, 4.0f, 12.0f, 4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("FocusLabel", "FOCUS DISTANCE (CM)"))
					]
					+ SGridPanel::Slot(1, 4).Padding(0.0f, 4.0f)
					[
						MakeFloatField(&FocusDistanceCm, 1.0f, 1000000.0f)
					]
					+ SGridPanel::Slot(0, 5).Padding(0.0f, 4.0f, 12.0f, 4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("GainLabel", "GAIN (DB)"))
					]
					+ SGridPanel::Slot(1, 5).Padding(0.0f, 4.0f)
					[
						MakeFloatField(&GainDb, -12.0f, 36.0f)
					]
					+ SGridPanel::Slot(0, 6).Padding(0.0f, 4.0f, 12.0f, 4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("PositionLabel", "WORLD POSITION (CM)"))
					]
					+ SGridPanel::Slot(1, 6).Padding(0.0f, 4.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)[MakePositionField(LOCTEXT("PositionX", "X"), &WorldPosition.X)]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)[MakePositionField(LOCTEXT("PositionY", "Y"), &WorldPosition.Y)]
						+ SHorizontalBox::Slot().FillWidth(1.0f)[MakePositionField(LOCTEXT("PositionZ", "Z"), &WorldPosition.Z)]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 12.0f)
				[
					SNew(SSeparator)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ViscaHeading", "VISCA OVER IP CONTROL"))
					.Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(SGridPanel)
					.IsEnabled_Lambda([this]() { return ActiveCamera.IsValid(); })
					.FillColumn(1, 1.0f)
					+ SGridPanel::Slot(0, 0).Padding(0.0f, 4.0f, 12.0f, 4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("ViscaEnabledLabel", "ENABLE VISCA IP"))
					]
					+ SGridPanel::Slot(1, 0).Padding(0.0f, 4.0f)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this]() { return bViscaEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](const ECheckBoxState State) { bViscaEnabled = State == ECheckBoxState::Checked; })
						[
							SNew(STextBlock).Text(LOCTEXT("ViscaUdpLabel", "Send Sony VISCA commands over UDP"))
						]
					]
					+ SGridPanel::Slot(0, 1).Padding(0.0f, 4.0f, 12.0f, 4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("ViscaIpLabel", "CAMERA IP ADDRESS"))
					]
					+ SGridPanel::Slot(1, 1).Padding(0.0f, 4.0f)
					[
						SAssignNew(ViscaIpField, SEditableTextBox)
						.HintText(LOCTEXT("ViscaIpHint", "192.168.1.100"))
						.OnTextChanged_Lambda([this](const FText& Text) { ViscaIpAddress = Text.ToString(); })
					]
					+ SGridPanel::Slot(0, 2).Padding(0.0f, 4.0f, 12.0f, 4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("ViscaPortLabel", "UDP PORT"))
					]
					+ SGridPanel::Slot(1, 2).Padding(0.0f, 4.0f)
					[
						SNew(SNumericEntryBox<int32>)
						.MinValue(1).MaxValue(65535)
						.Value_Lambda([this]() { return ViscaPort; })
						.OnValueChanged_Lambda([this](const int32 Value) { ViscaPort = FMath::Clamp(Value, 1, 65535); })
						.OnValueCommitted_Lambda([this](const int32 Value, ETextCommit::Type) { ViscaPort = FMath::Clamp(Value, 1, 65535); })
					]
					+ SGridPanel::Slot(0, 3).Padding(0.0f, 4.0f, 12.0f, 4.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("ViscaSpeedLabel", "PAN / TILT SPEED"))
					]
					+ SGridPanel::Slot(1, 3).Padding(0.0f, 4.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
						[
							SNew(SNumericEntryBox<int32>)
							.MinValue(1).MaxValue(24).MinSliderValue(1).MaxSliderValue(24)
							.Value_Lambda([this]() { return ViscaPanSpeed; })
							.OnValueChanged_Lambda([this](const int32 Value) { ViscaPanSpeed = FMath::Clamp(Value, 1, 24); })
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SNew(SNumericEntryBox<int32>)
							.MinValue(1).MaxValue(20).MinSliderValue(1).MaxSliderValue(20)
							.Value_Lambda([this]() { return ViscaTiltSpeed; })
							.OnValueChanged_Lambda([this](const int32 Value) { ViscaTiltSpeed = FMath::Clamp(Value, 1, 20); })
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 14.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("ApplyPreview", "APPLY TO PREVIEW"))
						.HAlign(HAlign_Center)
						.IsEnabled_Lambda([this]() { return ActiveCamera.IsValid(); })
						.OnClicked(this, &STSAVCameraControllerTool::ApplyPreview)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("ApplyAndSend", "APPLY + SEND VISCA"))
						.HAlign(HAlign_Center)
						.IsEnabled_Lambda([this]() { return ActiveCamera.IsValid(); })
						.OnClicked(this, &STSAVCameraControllerTool::ApplyAndSendVisca)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("ViscaHome", "VISCA HOME"))
						.HAlign(HAlign_Center)
						.IsEnabled_Lambda([this]() { return ActiveCamera.IsValid() && bViscaEnabled; })
						.OnClicked(this, &STSAVCameraControllerTool::SendViscaHome)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("ViscaStop", "VISCA STOP"))
						.HAlign(HAlign_Center)
						.IsEnabled_Lambda([this]() { return ActiveCamera.IsValid() && bViscaEnabled; })
						.OnClicked(this, &STSAVCameraControllerTool::SendViscaStop)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 10.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return StatusText; })
					.ColorAndOpacity(this, &STSAVCameraControllerTool::GetStatusColor)
					.AutoWrapText(true)
				]
			]
		]
	];

	RefreshCameraOptions();
}

void STSAVCameraControllerTool::RefreshCameraOptions()
{
	ATSAVCameraActor* Previous = ActiveCamera.Get();
	CameraOptions.Reset();
	if (GEditor)
	{
		if (UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			for (TActorIterator<ATSAVCameraActor> It(World); It; ++It)
			{
				CameraOptions.Add(MakeShared<FCameraOption>(*It));
			}
		}
	}
	CameraOptions.Sort([](const TSharedPtr<FCameraOption>& Left, const TSharedPtr<FCameraOption>& Right)
	{
		return TSAVCameraControllerTool::Private::GetCameraDisplayName(Left.IsValid() ? Left->Get() : nullptr)
			< TSAVCameraControllerTool::Private::GetCameraDisplayName(Right.IsValid() ? Right->Get() : nullptr);
	});
	if (CameraCombo)
	{
		CameraCombo->RefreshOptions();
	}

	ATSAVCameraActor* Next = nullptr;
	if (Previous && CameraOptions.ContainsByPredicate([Previous](const TSharedPtr<FCameraOption>& Item)
	{
		return Item.IsValid() && Item->Get() == Previous;
	}))
	{
		Next = Previous;
	}
	else if (ATSAVCameraActor* Selected = FindSelectedCamera())
	{
		Next = Selected;
	}
	else if (!CameraOptions.IsEmpty())
	{
		Next = CameraOptions[0]->Get();
	}
	SetActiveCamera(Next);
	if (Next)
	{
		SetStatus(FText::Format(LOCTEXT("Ready", "Controlling {0}."), FText::FromString(Next->GetActorLabel())), true);
	}
	else
	{
		SetStatus(LOCTEXT("NoCameras", "No TSAV cameras were found. Use Tools > TSAV Camera Tool to create one."), false);
	}
}

void STSAVCameraControllerTool::SetActiveCamera(ATSAVCameraActor* Camera)
{
	ActiveCamera = Camera;
	if (CameraCombo && Camera)
	{
		const TSharedPtr<FCameraOption>* Match = CameraOptions.FindByPredicate([Camera](const TSharedPtr<FCameraOption>& Item)
		{
			return Item.IsValid() && Item->Get() == Camera;
		});
		if (Match && CameraCombo->GetSelectedItem() != *Match)
		{
			CameraCombo->SetSelectedItem(*Match);
		}
	}
	else if (CameraCombo)
	{
		CameraCombo->ClearSelection();
	}
	LoadFormFromCamera();
}

void STSAVCameraControllerTool::LoadFormFromCamera()
{
	ATSAVCameraActor* Camera = ActiveCamera.Get();
	if (!Camera)
	{
		CameraName.Reset();
		if (CameraNameField) { CameraNameField->SetText(FText::GetEmpty()); }
		if (ViscaIpField) { ViscaIpField->SetText(FText::GetEmpty()); }
		return;
	}
	CameraName = Camera->GetActorLabel();
	PanDegrees = Camera->PanDegrees;
	TiltDegrees = Camera->TiltDegrees;
	ZoomPercent = Camera->ZoomNormalized * 100.0f;
	Iris = Camera->Aperture;
	FocusDistanceCm = Camera->FocusDistanceCm;
	GainDb = Camera->GainDb;
	WorldPosition = Camera->GetActorLocation();
	bViscaEnabled = Camera->bEnableViscaOverIp;
	ViscaIpAddress = Camera->ViscaIpAddress;
	ViscaPort = Camera->ViscaPort;
	ViscaPanSpeed = Camera->ViscaPanSpeed;
	ViscaTiltSpeed = Camera->ViscaTiltSpeed;
	if (CameraNameField) { CameraNameField->SetText(FText::FromString(CameraName)); }
	if (ViscaIpField) { ViscaIpField->SetText(FText::FromString(ViscaIpAddress)); }
}

ATSAVCameraActor* STSAVCameraControllerTool::FindSelectedCamera() const
{
	if (!GEditor || !GEditor->GetSelectedActors())
	{
		return nullptr;
	}
	for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
	{
		if (ATSAVCameraActor* Camera = Cast<ATSAVCameraActor>(*It))
		{
			return Camera;
		}
	}
	return nullptr;
}

TSharedRef<SWidget> STSAVCameraControllerTool::GenerateCameraOption(const TSharedPtr<FCameraOption> Item) const
{
	return SNew(STextBlock).Text(FText::FromString(
		TSAVCameraControllerTool::Private::GetCameraDisplayName(Item.IsValid() ? Item->Get() : nullptr)));
}

void STSAVCameraControllerTool::CameraOptionChanged(const TSharedPtr<FCameraOption> Item, ESelectInfo::Type SelectionType)
{
	SetActiveCamera(Item.IsValid() ? Item->Get() : nullptr);
}

FText STSAVCameraControllerTool::GetActiveCameraText() const
{
	return FText::FromString(TSAVCameraControllerTool::Private::GetCameraDisplayName(ActiveCamera.Get()));
}

FText STSAVCameraControllerTool::GetCameraSummaryText() const
{
	const ATSAVCameraActor* Camera = ActiveCamera.Get();
	if (!Camera)
	{
		return LOCTEXT("NoCameraSelected", "No camera selected");
	}
	return FText::Format(
		LOCTEXT("CameraSummary", "Type: {0}   |   Output: {1} x {2}   |   VISCA: {3}"),
		TSAVCameraControllerTool::Private::GetCameraTypeDisplayName(Camera->CameraType),
		FText::AsNumber(Camera->OutputResolution.X), FText::AsNumber(Camera->OutputResolution.Y),
		Camera->bEnableViscaOverIp ? LOCTEXT("ViscaOn", "Enabled") : LOCTEXT("ViscaOff", "Disabled"));
}

bool STSAVCameraControllerTool::ApplyChanges(const bool bSendVisca)
{
	ATSAVCameraActor* Camera = ActiveCamera.Get();
	if (!Camera)
	{
		SetStatus(LOCTEXT("ApplyNoCamera", "Select a camera before applying controls."), false);
		return false;
	}
	FString ResolvedName = CameraNameField ? CameraNameField->GetText().ToString().TrimStartAndEnd() : CameraName.TrimStartAndEnd();
	if (ResolvedName.IsEmpty())
	{
		ResolvedName = Camera->GetActorLabel();
	}
	ViscaIpAddress = ViscaIpField ? ViscaIpField->GetText().ToString().TrimStartAndEnd() : ViscaIpAddress.TrimStartAndEnd();

	const FScopedTransaction Transaction(FText::Format(
		LOCTEXT("ApplyTransaction", "Control Camera {0}"), FText::FromString(Camera->GetActorLabel())));
	Camera->Modify();
	Camera->SetActorLabel(ResolvedName);
	Camera->CameraLabel = FText::FromString(ResolvedName);
	Camera->bEnableViscaOverIp = bViscaEnabled;
	Camera->ViscaIpAddress = ViscaIpAddress;
	Camera->ViscaPort = FMath::Clamp(ViscaPort, 1, 65535);
	Camera->ViscaPanSpeed = FMath::Clamp(ViscaPanSpeed, 1, 24);
	Camera->ViscaTiltSpeed = FMath::Clamp(ViscaTiltSpeed, 1, 20);
	Camera->SetActorLocation(WorldPosition, false, nullptr, ETeleportType::TeleportPhysics);
	Camera->ApplyPTZ(PanDegrees, TiltDegrees, ZoomPercent / 100.0f, false);
	Camera->ApplyImageControls(Iris, FocusDistanceCm, GainDb, false);
	Camera->PostEditChange();
	Camera->MarkPackageDirty();

	bool bSent = true;
	if (bSendVisca)
	{
		bSent = Camera->bEnableViscaOverIp
			&& Camera->SendViscaPtzControls()
			&& Camera->SendViscaImageControls();
	}
	LoadFormFromCamera();
	if (CameraCombo)
	{
		CameraCombo->RefreshOptions();
	}
	if (bSendVisca && !bSent)
	{
		SetStatus(FText::Format(
			LOCTEXT("ViscaSendFailed", "Updated {0}, but VISCA could not send. Enable VISCA and check the IPv4 address and UDP port."),
			FText::FromString(ResolvedName)), false);
		return false;
	}
	SetStatus(FText::Format(
		bSendVisca
			? LOCTEXT("AppliedAndSent", "Updated {0} and sent pan, tilt, zoom, iris, focus, and gain over VISCA UDP.")
			: LOCTEXT("AppliedPreview", "Updated the Unreal preview for {0}. No network commands were sent."),
		FText::FromString(ResolvedName)), true);
	return true;
}

FReply STSAVCameraControllerTool::RefreshCameras()
{
	RefreshCameraOptions();
	return FReply::Handled();
}

FReply STSAVCameraControllerTool::UseSelectedCamera()
{
	if (ATSAVCameraActor* Camera = FindSelectedCamera())
	{
		SetActiveCamera(Camera);
		SetStatus(FText::Format(LOCTEXT("UsingSelected", "Controlling selected camera {0}."), FText::FromString(Camera->GetActorLabel())), true);
	}
	else
	{
		SetStatus(LOCTEXT("SelectionNotCamera", "Select a TSAV Production Camera in the level first."), false);
	}
	return FReply::Handled();
}

FReply STSAVCameraControllerTool::SelectCameraInLevel()
{
	if (ATSAVCameraActor* Camera = ActiveCamera.Get(); GEditor && Camera)
	{
		GEditor->SelectNone(false, true, false);
		GEditor->SelectActor(Camera, true, true, true);
	}
	return FReply::Handled();
}

FReply STSAVCameraControllerTool::ApplyPreview()
{
	ApplyChanges(false);
	return FReply::Handled();
}

FReply STSAVCameraControllerTool::ApplyAndSendVisca()
{
	ApplyChanges(true);
	return FReply::Handled();
}

FReply STSAVCameraControllerTool::SendViscaHome()
{
	if (ApplyChanges(false))
	{
		ATSAVCameraActor* Camera = ActiveCamera.Get();
		const bool bSent = Camera && Camera->SendViscaHome();
		SetStatus(bSent
			? LOCTEXT("HomeSent", "VISCA Home command sent over UDP.")
			: LOCTEXT("HomeFailed", "VISCA Home could not send. Check the enabled state, IPv4 address, and UDP port."), bSent);
	}
	return FReply::Handled();
}

FReply STSAVCameraControllerTool::SendViscaStop()
{
	if (ApplyChanges(false))
	{
		ATSAVCameraActor* Camera = ActiveCamera.Get();
		const bool bSent = Camera && Camera->SendViscaStop();
		SetStatus(bSent
			? LOCTEXT("StopSent", "VISCA Stop command sent over UDP.")
			: LOCTEXT("StopFailed", "VISCA Stop could not send. Check the enabled state, IPv4 address, and UDP port."), bSent);
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

#undef LOCTEXT_NAMESPACE
