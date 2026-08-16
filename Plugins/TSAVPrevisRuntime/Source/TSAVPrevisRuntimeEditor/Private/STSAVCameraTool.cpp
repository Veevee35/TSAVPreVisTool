// Copyright TSAV. All Rights Reserved.

#include "STSAVCameraTool.h"

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LevelEditorViewport.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "TSAVVideoSwitcher.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "STSAVCameraTool"

namespace TSAVCameraTool::Private
{
	FTransform GetEditorViewTransform()
	{
		if (const FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient)
		{
			return FTransform(ViewportClient->GetViewRotation(), ViewportClient->GetViewLocation());
		}
		return FTransform::Identity;
	}

	FText GetLensDisplayName(const ETSAVLensPreset Preset)
	{
		if (const UEnum* Enum = StaticEnum<ETSAVLensPreset>())
		{
			return Enum->GetDisplayNameTextByValue(static_cast<int64>(Preset));
		}
		return LOCTEXT("UnknownLens", "Unknown Lens");
	}
}

void STSAVCameraTool::Construct(const FArguments& InArgs)
{
	PopulateLensOptions();
	CameraName = MakeDefaultCameraName();

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
				.Text(LOCTEXT("Title", "CREATE CAMERA INPUT"))
				.Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 12.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Description", "Configure the camera feed, then create it from the current editor view. The new feed is automatically added to every TSAV Video Switcher in the level."))
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SGridPanel)
				.FillColumn(1, 1.0f)
				+ SGridPanel::Slot(0, 0)
				.Padding(0.0f, 3.0f, 12.0f, 3.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("NameLabel", "INPUT NAME"))
				]
				+ SGridPanel::Slot(1, 0)
				.Padding(0.0f, 3.0f)
				[
					SAssignNew(CameraNameField, SEditableTextBox)
					.Text(FText::FromString(CameraName))
					.HintText(LOCTEXT("NameHint", "CAM 1"))
				]
				+ SGridPanel::Slot(0, 1)
				.Padding(0.0f, 3.0f, 12.0f, 3.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("ResolutionLabel", "OUTPUT RESOLUTION"))
				]
				+ SGridPanel::Slot(1, 1)
				.Padding(0.0f, 3.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SNumericEntryBox<int32>)
						.MinValue(160)
						.MaxValue(3840)
						.MinSliderValue(160)
						.MaxSliderValue(3840)
						.Value_Lambda([this]() { return OutputWidth; })
						.OnValueChanged_Lambda([this](const int32 Value) { OutputWidth = FMath::Clamp(Value, 160, 3840); })
						.OnValueCommitted_Lambda([this](const int32 Value, ETextCommit::Type) { OutputWidth = FMath::Clamp(Value, 160, 3840); })
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("ResolutionBy", "x"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SNumericEntryBox<int32>)
						.MinValue(90)
						.MaxValue(2160)
						.MinSliderValue(90)
						.MaxSliderValue(2160)
						.Value_Lambda([this]() { return OutputHeight; })
						.OnValueChanged_Lambda([this](const int32 Value) { OutputHeight = FMath::Clamp(Value, 90, 2160); })
						.OnValueCommitted_Lambda([this](const int32 Value, ETextCommit::Type) { OutputHeight = FMath::Clamp(Value, 90, 2160); })
					]
				]
				+ SGridPanel::Slot(0, 2)
				.Padding(0.0f, 3.0f, 12.0f, 3.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("LensLabel", "LENS"))
				]
				+ SGridPanel::Slot(1, 2)
				.Padding(0.0f, 3.0f)
				[
					SAssignNew(LensCombo, SComboBox<TSharedPtr<FLensOption>>)
					.OptionsSource(&LensOptions)
					.OnGenerateWidget(this, &STSAVCameraTool::GenerateLensOption)
					.OnSelectionChanged(this, &STSAVCameraTool::LensOptionChanged)
					.IsEnabled_Lambda([this]() { return !bIsPTZ; })
					[
						SNew(STextBlock).Text(this, &STSAVCameraTool::GetSelectedLensText)
					]
				]
				+ SGridPanel::Slot(0, 3)
				.Padding(0.0f, 3.0f, 12.0f, 3.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("PTZLabel", "PTZ CAMERA"))
				]
				+ SGridPanel::Slot(1, 3)
				.Padding(0.0f, 3.0f)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() { return bIsPTZ ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](const ECheckBoxState State) { bIsPTZ = State == ECheckBoxState::Checked; })
					[
						SNew(STextBlock).Text(LOCTEXT("PTZCheckLabel", "Enable pan, tilt, and zoom"))
					]
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
				SNew(SButton)
				.Text(LOCTEXT("CreateInput", "CREATE CAMERA INPUT"))
				.HAlign(HAlign_Center)
				.OnClicked(this, &STSAVCameraTool::CreateCameraInput)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(this, &STSAVCameraTool::GetStatusText)
				.ColorAndOpacity(this, &STSAVCameraTool::GetStatusColor)
				.AutoWrapText(true)
			]
		]
	];

	if (LensCombo)
	{
		const TSharedPtr<FLensOption>* Selected = LensOptions.FindByPredicate([this](const TSharedPtr<FLensOption>& Item)
		{
			return Item.IsValid() && *Item == LensPreset;
		});
		if (Selected)
		{
			LensCombo->SetSelectedItem(*Selected);
		}
	}
	SetStatus(LOCTEXT("Ready", "Configure the input and click Create Camera Input."), true);
}

void STSAVCameraTool::PopulateLensOptions()
{
	LensOptions.Reset();
	const ETSAVLensPreset Presets[] = {
		ETSAVLensPreset::UltraWide12,
		ETSAVLensPreset::Wide18,
		ETSAVLensPreset::Wide24,
		ETSAVLensPreset::Standard35,
		ETSAVLensPreset::Standard50,
		ETSAVLensPreset::Portrait85,
		ETSAVLensPreset::Telephoto135,
		ETSAVLensPreset::BroadcastZoom,
	};
	for (const ETSAVLensPreset Preset : Presets)
	{
		LensOptions.Add(MakeShared<FLensOption>(Preset));
	}
}

TSharedRef<SWidget> STSAVCameraTool::GenerateLensOption(const TSharedPtr<FLensOption> Item) const
{
	return SNew(STextBlock).Text(Item.IsValid() ? TSAVCameraTool::Private::GetLensDisplayName(*Item) : FText::GetEmpty());
}

void STSAVCameraTool::LensOptionChanged(const TSharedPtr<FLensOption> Item, ESelectInfo::Type SelectionType)
{
	if (Item.IsValid())
	{
		LensPreset = *Item;
	}
}

FText STSAVCameraTool::GetSelectedLensText() const
{
	return bIsPTZ
		? TSAVCameraTool::Private::GetLensDisplayName(ETSAVLensPreset::PTZZoom)
		: TSAVCameraTool::Private::GetLensDisplayName(LensPreset);
}

FSlateColor STSAVCameraTool::GetStatusColor() const
{
	return bStatusSuccess ? FLinearColor(0.35f, 0.85f, 0.45f) : FLinearColor(1.0f, 0.55f, 0.20f);
}

FReply STSAVCameraTool::CreateCameraInput()
{
	if (!GEditor)
	{
		return FReply::Handled();
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		SetStatus(LOCTEXT("NoWorld", "No editable level is currently open."), false);
		return FReply::Handled();
	}

	FString ResolvedName = CameraNameField ? CameraNameField->GetText().ToString().TrimStartAndEnd() : FString();
	if (ResolvedName.IsEmpty())
	{
		ResolvedName = MakeDefaultCameraName();
	}
	OutputWidth = FMath::Clamp(OutputWidth, 160, 3840);
	OutputHeight = FMath::Clamp(OutputHeight, 90, 2160);

	const FScopedTransaction Transaction(LOCTEXT("CreateCameraInputTransaction", "Create TSAV Camera Input"));
	World->Modify();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transactional;
	ATSAVCameraActor* Camera = World->SpawnActor<ATSAVCameraActor>(
		ATSAVCameraActor::StaticClass(), TSAVCameraTool::Private::GetEditorViewTransform(), SpawnParameters);
	if (!Camera)
	{
		SetStatus(LOCTEXT("CreateFailed", "The camera input could not be created."), false);
		return FReply::Handled();
	}

	Camera->Modify();
	Camera->CameraLabel = FText::FromString(ResolvedName);
	Camera->SetActorLabel(ResolvedName);
	Camera->OutputResolution = FIntPoint(OutputWidth, OutputHeight);
	Camera->bEnableVideoOutput = true;
	Camera->SetCameraType(bIsPTZ ? ETSAVCameraType::PTZ : ETSAVCameraType::Broadcast);
	Camera->SetLensPreset(bIsPTZ ? ETSAVLensPreset::PTZZoom : LensPreset);
	Camera->PostEditChange();
	Camera->MarkPackageDirty();

	int32 SwitcherCount = 0;
	for (TActorIterator<ATSAVVideoSwitcher> It(World); It; ++It)
	{
		ATSAVVideoSwitcher* Switcher = *It;
		Switcher->Modify();
		Switcher->DiscoverSources();
		Switcher->PostEditChange();
		Switcher->MarkPackageDirty();
		if (Switcher->Inputs.ContainsByPredicate([Camera](const FTSAVVideoInput& Input)
		{
			return Input.Kind == ETSAVVideoInputKind::CameraFeed && Input.ProviderId == Camera->CameraId;
		}))
		{
			++SwitcherCount;
		}
	}

	GEditor->SelectNone(false, true, false);
	GEditor->SelectActor(Camera, true, true, true);
	CameraName = MakeDefaultCameraName();
	if (CameraNameField)
	{
		CameraNameField->SetText(FText::FromString(CameraName));
	}

	if (SwitcherCount > 0)
	{
		SetStatus(FText::Format(
			LOCTEXT("CreatedAndRegistered", "Created {0} at {1} x {2} and registered its feed with {3} video switcher(s)."),
			FText::FromString(ResolvedName), FText::AsNumber(OutputWidth), FText::AsNumber(OutputHeight), FText::AsNumber(SwitcherCount)), true);
	}
	else
	{
		SetStatus(FText::Format(
			LOCTEXT("CreatedNoSwitcher", "Created {0} at {1} x {2}. Create or refresh a Video Switcher and the feed will appear automatically."),
			FText::FromString(ResolvedName), FText::AsNumber(OutputWidth), FText::AsNumber(OutputHeight)), true);
	}
	return FReply::Handled();
}

FString STSAVCameraTool::MakeDefaultCameraName() const
{
	int32 CameraNumber = 1;
	if (GEditor)
	{
		if (UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			for (TActorIterator<ATSAVCameraActor> It(World); It; ++It)
			{
				++CameraNumber;
			}
		}
	}
	return FString::Printf(TEXT("CAM %d"), CameraNumber);
}

void STSAVCameraTool::SetStatus(const FText& Message, const bool bSuccess)
{
	StatusText = Message;
	bStatusSuccess = bSuccess;
}

#undef LOCTEXT_NAMESPACE
