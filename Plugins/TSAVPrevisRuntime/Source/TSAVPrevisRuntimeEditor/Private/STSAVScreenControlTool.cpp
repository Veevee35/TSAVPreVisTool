// Copyright TSAV. All Rights Reserved.

#include "STSAVScreenControlTool.h"

#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "STSAVScreenControlTool"

namespace TSAVScreenControlTool::Private
{
	FString GetScreenDisplayName(const ATSAVMediaSurfaceActor* Screen)
	{
		if (!Screen)
		{
			return TEXT("None");
		}
		const FIntPoint Resolution = Screen->GetSurfaceResolutionPixels();
		return FString::Printf(TEXT("%s  |  %d x %d"), *Screen->GetActorLabel(), Resolution.X, Resolution.Y);
	}
}

void STSAVScreenControlTool::Construct(const FArguments& InArgs)
{
	auto MakeTransformField = [](const FText& AxisLabel, double* Value) -> TSharedRef<SWidget>
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
				.OnValueChanged_Lambda([Value](const double NewValue) { *Value = FMath::Clamp(NewValue, -1000000.0, 1000000.0); })
				.OnValueCommitted_Lambda([Value](const double NewValue, ETextCommit::Type) { *Value = FMath::Clamp(NewValue, -1000000.0, 1000000.0); })
			];
	};

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
				.Text(LOCTEXT("Title", "SCREEN CONTROL"))
				.Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 12.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Description", "Select any TSAV LED wall or panel and update its core screen settings, canvas origin, and physical transform from one place."))
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(ScreenCombo, SComboBox<TSharedPtr<FScreenOption>>)
					.OptionsSource(&ScreenOptions)
					.OnGenerateWidget(this, &STSAVScreenControlTool::GenerateScreenOption)
					.OnSelectionChanged(this, &STSAVScreenControlTool::ScreenOptionChanged)
					[
						SNew(STextBlock).Text(this, &STSAVScreenControlTool::GetActiveScreenText)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Refresh", "Refresh"))
					.ToolTipText(LOCTEXT("RefreshTooltip", "Refresh the list of screens in the current level"))
					.OnClicked(this, &STSAVScreenControlTool::RefreshScreens)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("UseSelected", "Use Selected"))
					.ToolTipText(LOCTEXT("UseSelectedTooltip", "Use the currently selected LED wall or panel"))
					.OnClicked(this, &STSAVScreenControlTool::UseSelectedScreen)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("SelectLevel", "Select"))
					.ToolTipText(LOCTEXT("SelectLevelTooltip", "Select the active screen in the level"))
					.IsEnabled_Lambda([this]() { return ActiveScreen.IsValid(); })
					.OnClicked(this, &STSAVScreenControlTool::SelectScreenInLevel)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(this, &STSAVScreenControlTool::GetScreenResolutionText)
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
				SNew(SGridPanel)
				.FillColumn(1, 1.0f)
				+ SGridPanel::Slot(0, 0)
				.Padding(0.0f, 4.0f, 12.0f, 4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("NameLabel", "SCREEN NAME"))
				]
				+ SGridPanel::Slot(1, 0)
				.Padding(0.0f, 4.0f)
				[
					SAssignNew(ScreenNameField, SEditableTextBox)
					.IsEnabled_Lambda([this]() { return ActiveScreen.IsValid(); })
					.OnTextChanged_Lambda([this](const FText& Text) { ScreenName = Text.ToString(); })
				]
				+ SGridPanel::Slot(0, 1)
				.Padding(0.0f, 4.0f, 12.0f, 4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("BrightnessLabel", "BRIGHTNESS"))
				]
				+ SGridPanel::Slot(1, 1)
				.Padding(0.0f, 4.0f)
				[
					SNew(SNumericEntryBox<float>)
					.IsEnabled_Lambda([this]() { return ActiveScreen.IsValid(); })
					.MinValue(0.0f)
					.MaxValue(20.0f)
					.MinSliderValue(0.0f)
					.MaxSliderValue(20.0f)
					.Value_Lambda([this]() { return Brightness; })
					.OnValueChanged_Lambda([this](const float Value) { Brightness = FMath::Clamp(Value, 0.0f, 20.0f); })
					.OnValueCommitted_Lambda([this](const float Value, ETextCommit::Type) { Brightness = FMath::Clamp(Value, 0.0f, 20.0f); })
				]
				+ SGridPanel::Slot(0, 2)
				.Padding(0.0f, 4.0f, 12.0f, 4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("CanvasStartLabel", "CANVAS START X / Y"))
				]
				+ SGridPanel::Slot(1, 2)
				.Padding(0.0f, 4.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SNumericEntryBox<int32>)
						.IsEnabled_Lambda([this]() { return ActiveScreen.IsValid(); })
						.MinValue(0)
						.MaxValue(1000000)
						.Value_Lambda([this]() { return CanvasStartX; })
						.OnValueChanged_Lambda([this](const int32 Value) { CanvasStartX = FMath::Clamp(Value, 0, 1000000); })
						.OnValueCommitted_Lambda([this](const int32 Value, ETextCommit::Type) { CanvasStartX = FMath::Clamp(Value, 0, 1000000); })
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("CanvasBy", "x"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SNumericEntryBox<int32>)
						.IsEnabled_Lambda([this]() { return ActiveScreen.IsValid(); })
						.MinValue(0)
						.MaxValue(1000000)
						.Value_Lambda([this]() { return CanvasStartY; })
						.OnValueChanged_Lambda([this](const int32 Value) { CanvasStartY = FMath::Clamp(Value, 0, 1000000); })
						.OnValueCommitted_Lambda([this](const int32 Value, ETextCommit::Type) { CanvasStartY = FMath::Clamp(Value, 0, 1000000); })
					]
				]
				+ SGridPanel::Slot(0, 3)
				.Padding(0.0f, 4.0f, 12.0f, 4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("LocationLabel", "WORLD LOCATION (CM)"))
				]
				+ SGridPanel::Slot(1, 3)
				.Padding(0.0f, 4.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)[MakeTransformField(LOCTEXT("LocationX", "X"), &WorldLocation.X)]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)[MakeTransformField(LOCTEXT("LocationY", "Y"), &WorldLocation.Y)]
					+ SHorizontalBox::Slot().FillWidth(1.0f)[MakeTransformField(LOCTEXT("LocationZ", "Z"), &WorldLocation.Z)]
				]
				+ SGridPanel::Slot(0, 4)
				.Padding(0.0f, 4.0f, 12.0f, 4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("RotationLabel", "WORLD ROTATION (DEG)"))
				]
				+ SGridPanel::Slot(1, 4)
				.Padding(0.0f, 4.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)[MakeTransformField(LOCTEXT("RotationPitch", "P"), &WorldRotation.Pitch)]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)[MakeTransformField(LOCTEXT("RotationYaw", "Y"), &WorldRotation.Yaw)]
					+ SHorizontalBox::Slot().FillWidth(1.0f)[MakeTransformField(LOCTEXT("RotationRoll", "R"), &WorldRotation.Roll)]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 14.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Apply", "APPLY TO SCREEN"))
				.HAlign(HAlign_Center)
				.IsEnabled_Lambda([this]() { return ActiveScreen.IsValid(); })
				.OnClicked(this, &STSAVScreenControlTool::ApplyChanges)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return StatusText; })
				.ColorAndOpacity(this, &STSAVScreenControlTool::GetStatusColor)
				.AutoWrapText(true)
			]
		]
	];

	RefreshScreenOptions();
}

void STSAVScreenControlTool::RefreshScreenOptions()
{
	ATSAVMediaSurfaceActor* Previous = ActiveScreen.Get();
	ScreenOptions.Reset();
	if (GEditor)
	{
		if (UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			for (TActorIterator<ATSAVMediaSurfaceActor> It(World); It; ++It)
			{
				ScreenOptions.Add(MakeShared<FScreenOption>(*It));
			}
		}
	}
	ScreenOptions.Sort([](const TSharedPtr<FScreenOption>& Left, const TSharedPtr<FScreenOption>& Right)
	{
		return TSAVScreenControlTool::Private::GetScreenDisplayName(Left.IsValid() ? Left->Get() : nullptr)
			< TSAVScreenControlTool::Private::GetScreenDisplayName(Right.IsValid() ? Right->Get() : nullptr);
	});
	if (ScreenCombo)
	{
		ScreenCombo->RefreshOptions();
	}
	ATSAVMediaSurfaceActor* Next = nullptr;
	if (Previous && ScreenOptions.ContainsByPredicate([Previous](const TSharedPtr<FScreenOption>& Item) { return Item.IsValid() && Item->Get() == Previous; }))
	{
		Next = Previous;
	}
	else if (ATSAVMediaSurfaceActor* Selected = FindSelectedScreen())
	{
		Next = Selected;
	}
	else if (!ScreenOptions.IsEmpty())
	{
		Next = ScreenOptions[0]->Get();
	}
	SetActiveScreen(Next);
	if (Next)
	{
		SetStatus(FText::Format(LOCTEXT("Ready", "Editing {0}. Adjust values and click Apply To Screen."), FText::FromString(Next->GetActorLabel())), true);
	}
	else
	{
		SetStatus(LOCTEXT("NoScreens", "No TSAV LED walls or panels were found in the current level."), false);
	}
}

void STSAVScreenControlTool::SetActiveScreen(ATSAVMediaSurfaceActor* Screen)
{
	ActiveScreen = Screen;
	if (ScreenCombo && Screen)
	{
		const TSharedPtr<FScreenOption>* Match = ScreenOptions.FindByPredicate([Screen](const TSharedPtr<FScreenOption>& Item)
		{
			return Item.IsValid() && Item->Get() == Screen;
		});
		if (Match && ScreenCombo->GetSelectedItem() != *Match)
		{
			ScreenCombo->SetSelectedItem(*Match);
		}
	}
	else if (ScreenCombo)
	{
		ScreenCombo->ClearSelection();
	}
	LoadFormFromScreen();
}

void STSAVScreenControlTool::LoadFormFromScreen()
{
	ATSAVMediaSurfaceActor* Screen = ActiveScreen.Get();
	if (!Screen)
	{
		ScreenName.Reset();
		if (ScreenNameField) { ScreenNameField->SetText(FText::GetEmpty()); }
		return;
	}
	ScreenName = Screen->GetActorLabel();
	Brightness = Screen->EmissiveStrength;
	CanvasStartX = Screen->CanvasPosition.X;
	CanvasStartY = Screen->CanvasPosition.Y;
	WorldLocation = Screen->GetActorLocation();
	WorldRotation = Screen->GetActorRotation();
	if (ScreenNameField)
	{
		ScreenNameField->SetText(FText::FromString(ScreenName));
	}
}

ATSAVMediaSurfaceActor* STSAVScreenControlTool::FindSelectedScreen() const
{
	if (!GEditor || !GEditor->GetSelectedActors())
	{
		return nullptr;
	}
	for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
	{
		if (ATSAVMediaSurfaceActor* Screen = Cast<ATSAVMediaSurfaceActor>(*It))
		{
			return Screen;
		}
	}
	return nullptr;
}

TSharedRef<SWidget> STSAVScreenControlTool::GenerateScreenOption(const TSharedPtr<FScreenOption> Item) const
{
	return SNew(STextBlock).Text(FText::FromString(
		TSAVScreenControlTool::Private::GetScreenDisplayName(Item.IsValid() ? Item->Get() : nullptr)));
}

void STSAVScreenControlTool::ScreenOptionChanged(const TSharedPtr<FScreenOption> Item, ESelectInfo::Type SelectionType)
{
	SetActiveScreen(Item.IsValid() ? Item->Get() : nullptr);
}

FText STSAVScreenControlTool::GetActiveScreenText() const
{
	return FText::FromString(TSAVScreenControlTool::Private::GetScreenDisplayName(ActiveScreen.Get()));
}

FText STSAVScreenControlTool::GetScreenResolutionText() const
{
	const ATSAVMediaSurfaceActor* Screen = ActiveScreen.Get();
	if (!Screen)
	{
		return LOCTEXT("NoResolution", "No screen selected");
	}
	const FIntPoint ScreenResolution = Screen->GetSurfaceResolutionPixels();
	return FText::Format(
		LOCTEXT("ResolutionSummary", "Screen resolution: {0} x {1} px   |   Canvas: {2} x {3} px"),
		FText::AsNumber(ScreenResolution.X), FText::AsNumber(ScreenResolution.Y),
		FText::AsNumber(Screen->CanvasResolution.X), FText::AsNumber(Screen->CanvasResolution.Y));
}

FReply STSAVScreenControlTool::ApplyChanges()
{
	ATSAVMediaSurfaceActor* Screen = ActiveScreen.Get();
	if (!Screen)
	{
		SetStatus(LOCTEXT("ApplyNoScreen", "Select a screen before applying changes."), false);
		return FReply::Handled();
	}
	FString ResolvedName = ScreenNameField ? ScreenNameField->GetText().ToString().TrimStartAndEnd() : ScreenName.TrimStartAndEnd();
	if (ResolvedName.IsEmpty())
	{
		ResolvedName = Screen->GetActorLabel();
	}
	const FScopedTransaction Transaction(FText::Format(
		LOCTEXT("ApplyTransaction", "Update Screen {0}"), FText::FromString(Screen->GetActorLabel())));
	Screen->Modify();
	Screen->SetActorLabel(ResolvedName);
	Screen->EmissiveStrength = FMath::Clamp(Brightness, 0.0f, 20.0f);
	Screen->CanvasPosition = FIntPoint(FMath::Max(CanvasStartX, 0), FMath::Max(CanvasStartY, 0));
	Screen->SetActorLocationAndRotation(WorldLocation, WorldRotation, false, nullptr, ETeleportType::TeleportPhysics);
	Screen->PostEditChange();
	Screen->MarkPackageDirty();
	LoadFormFromScreen();
	RefreshScreenOptions();
	SetStatus(FText::Format(
		LOCTEXT("Applied", "Updated {0}: brightness {1}, canvas start ({2}, {3}), location and rotation."),
		FText::FromString(ResolvedName), FText::AsNumber(Screen->EmissiveStrength),
		FText::AsNumber(Screen->CanvasPosition.X), FText::AsNumber(Screen->CanvasPosition.Y)), true);
	return FReply::Handled();
}

FReply STSAVScreenControlTool::RefreshScreens()
{
	RefreshScreenOptions();
	return FReply::Handled();
}

FReply STSAVScreenControlTool::UseSelectedScreen()
{
	if (ATSAVMediaSurfaceActor* Screen = FindSelectedScreen())
	{
		SetActiveScreen(Screen);
		SetStatus(FText::Format(LOCTEXT("UsingSelected", "Editing selected screen {0}."), FText::FromString(Screen->GetActorLabel())), true);
	}
	else
	{
		SetStatus(LOCTEXT("SelectionNotScreen", "Select a TSAV LED wall or panel in the level first."), false);
	}
	return FReply::Handled();
}

FReply STSAVScreenControlTool::SelectScreenInLevel()
{
	if (ATSAVMediaSurfaceActor* Screen = ActiveScreen.Get(); GEditor && Screen)
	{
		GEditor->SelectNone(false, true, false);
		GEditor->SelectActor(Screen, true, true, true);
	}
	return FReply::Handled();
}

void STSAVScreenControlTool::SetStatus(const FText& Message, const bool bSuccess)
{
	StatusText = Message;
	bStatusSuccess = bSuccess;
}

FSlateColor STSAVScreenControlTool::GetStatusColor() const
{
	return bStatusSuccess ? FLinearColor(0.35f, 0.85f, 0.45f) : FLinearColor(1.0f, 0.55f, 0.20f);
}

#undef LOCTEXT_NAMESPACE
