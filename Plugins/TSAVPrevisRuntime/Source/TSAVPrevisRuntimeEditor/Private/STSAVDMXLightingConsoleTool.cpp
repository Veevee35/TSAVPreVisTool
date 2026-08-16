// Copyright TSAV. All Rights Reserved.

#include "STSAVDMXLightingConsoleTool.h"

#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Styling/AppStyle.h"
#include "TSAVDMXFixtureCatalog.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "STSAVDMXLightingConsoleTool"

void STSAVDMXLightingConsoleTool::Construct(const FArguments& InArgs)
{
	Catalog = TSAVDMXEditorUtils::LoadCatalog();
	StatusText = Catalog
		? LOCTEXT("ReadyStatus", "Select fixture patches, then move any fader. Values transmit live with no Apply button.")
		: LOCTEXT("MissingStatus", "Fixture catalog missing. Run Tools > Build Complete GDTF Fixture Library.");

	ChildSlot
	[
		SNew(SBorder)
		.Padding(12.0f)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(LOCTEXT("Title", "TSAV Lighting Console")).Font(FAppStyle::GetFontStyle(TEXT("HeadingLarge")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Subtitle", "Program any combination of the 607 library patches. Common controls drive mixed fixture types; raw attribute faders expose every function on the primary selected mode."))
				.AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SSplitter)
				+ SSplitter::Slot().Value(0.42f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(SearchBox, SSearchBox).HintText(LOCTEXT("SearchHint", "Search all 607 fixture patches…")).OnTextChanged(this, &STSAVDMXLightingConsoleTool::SearchChanged)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 3.0f, 0.0f)[SNew(SButton).Text(LOCTEXT("Refresh", "Refresh")).OnClicked(this, &STSAVDMXLightingConsoleTool::RefreshClicked)]
						+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f)[SNew(SButton).Text(LOCTEXT("SelectVisible", "Select Visible")).OnClicked(this, &STSAVDMXLightingConsoleTool::SelectVisibleClicked)]
						+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f)[SNew(SButton).Text(LOCTEXT("SelectPlaced", "Select Placed")).OnClicked(this, &STSAVDMXLightingConsoleTool::SelectPlacedClicked)]
						+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f)[SNew(SButton).Text(LOCTEXT("Clear", "Clear")).OnClicked(this, &STSAVDMXLightingConsoleTool::ClearSelectionClicked)]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
					[
						SNew(STextBlock).Text(this, &STSAVDMXLightingConsoleTool::GetSelectionText).ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						SAssignNew(FixtureList, SListView<TSharedPtr<FTSAVDMXConsoleListItem>>)
						.ListItemsSource(&FilteredRows)
						.SelectionMode(ESelectionMode::None)
						.OnGenerateRow(this, &STSAVDMXLightingConsoleTool::GenerateFixtureRow)
					]
				]
				+ SSplitter::Slot().Value(0.58f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 5.0f)[SNew(STextBlock).Text(LOCTEXT("CommonHeading", "COMMON PROGRAMMER")).Font(FAppStyle::GetFontStyle(TEXT("HeadingSmall")))]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)[MakeCommonControl(LOCTEXT("Pan", "Pan"), &Values.Pan)]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)[MakeCommonControl(LOCTEXT("Tilt", "Tilt"), &Values.Tilt)]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)[MakeCommonControl(LOCTEXT("Dimmer", "Dimmer"), &Values.Dimmer)]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)[MakeCommonControl(LOCTEXT("Red", "Red"), &Values.Red)]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)[MakeCommonControl(LOCTEXT("Green", "Green"), &Values.Green)]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)[MakeCommonControl(LOCTEXT("Blue", "Blue"), &Values.Blue)]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)[MakeCommonControl(LOCTEXT("Zoom", "Zoom"), &Values.Zoom)]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 8.0f, 8.0f, 2.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(92.0f)[SNew(STextBlock).Text(LOCTEXT("GrandMaster", "Grand Master"))]]
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4.0f, 0.0f, 8.0f, 0.0f)
							[
								SNew(SSlider).PreventThrottling(true).Value_Lambda([this]() { return GrandMaster; }).OnValueChanged_Lambda([this](float Value) { GrandMaster = Value; SendCommonValues(); })
							]
							+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(76.0f)[SNew(SNumericEntryBox<float>).MinValue(0.0f).MaxValue(1.0f).Value_Lambda([this]() { return GrandMaster; }).OnValueChanged_Lambda([this](float Value) { GrandMaster = Value; SendCommonValues(); })]]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 7.0f)
						[
							SNew(SGridPanel)
							+ SGridPanel::Slot(0, 0).Padding(2.0f)[SNew(SButton).Text(LOCTEXT("Home", "Home")).OnClicked(this, &STSAVDMXLightingConsoleTool::HomeClicked)]
							+ SGridPanel::Slot(1, 0).Padding(2.0f)[SNew(SButton).Text(LOCTEXT("Full", "Full")).OnClicked(this, &STSAVDMXLightingConsoleTool::FullClicked)]
							+ SGridPanel::Slot(2, 0).Padding(2.0f)[SNew(SButton).Text(LOCTEXT("White", "White")).OnClicked(this, &STSAVDMXLightingConsoleTool::WhiteClicked)]
							+ SGridPanel::Slot(3, 0).Padding(2.0f)[SNew(SButton).Text(LOCTEXT("RedPreset", "Red")).OnClicked(this, &STSAVDMXLightingConsoleTool::RedClicked)]
							+ SGridPanel::Slot(4, 0).Padding(2.0f)[SNew(SButton).Text(LOCTEXT("GreenPreset", "Green")).OnClicked(this, &STSAVDMXLightingConsoleTool::GreenClicked)]
							+ SGridPanel::Slot(5, 0).Padding(2.0f)[SNew(SButton).Text(LOCTEXT("BluePreset", "Blue")).OnClicked(this, &STSAVDMXLightingConsoleTool::BlueClicked)]
							+ SGridPanel::Slot(0, 1).ColumnSpan(3).Padding(2.0f)[SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.45f, 0.02f, 0.02f)).Text(LOCTEXT("Blackout", "BLACKOUT")).OnClicked(this, &STSAVDMXLightingConsoleTool::BlackoutClicked)]
							+ SGridPanel::Slot(3, 1).ColumnSpan(3).Padding(2.0f)[SNew(SButton).Text(LOCTEXT("Release", "RELEASE BLACKOUT")).OnClicked(this, &STSAVDMXLightingConsoleTool::ReleaseBlackoutClicked)]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 8.0f, 8.0f, 3.0f)[SNew(STextBlock).Text(LOCTEXT("AttributeHeading", "PRIMARY MODE ATTRIBUTE FADERS")).Font(FAppStyle::GetFontStyle(TEXT("HeadingSmall")))]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f)[SAssignNew(AttributeFaders, SVerticalBox)]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock).Text_Lambda([this]() { return StatusText; }).ColorAndOpacity(this, &STSAVDMXLightingConsoleTool::GetStatusColor).AutoWrapText(true)
			]
		]
	];
	RefreshRows();
	RebuildAttributeFaders();
}

void STSAVDMXLightingConsoleTool::RefreshRows()
{
	AllRows.Reset();
	Catalog = TSAVDMXEditorUtils::LoadCatalog();
	if (Catalog)
	{
		for (const FTSAVDMXFixtureDefinition& Definition : Catalog->Fixtures)
		{
			UDMXEntityFixturePatch* Patch = TSAVDMXEditorUtils::ResolvePatch(Definition);
			TSharedPtr<FTSAVDMXConsoleListItem> Item = MakeShared<FTSAVDMXConsoleListItem>();
			Item->DefinitionId = Definition.DefinitionId;
			Item->Label = FString::Printf(TEXT("%s — %s"), Definition.Manufacturer.IsEmpty() ? TEXT("Unknown") : *Definition.Manufacturer.ToString(), *Definition.DisplayName.ToString());
			Item->Universe = Patch ? Patch->GetUniverseID() : Definition.Universe;
			Item->Address = Patch ? Patch->GetStartingChannel() : Definition.Address;
			Item->ActorCount = TSAVDMXEditorUtils::FindMatchingActors(Definition).Num();
			AllRows.Add(Item);
		}
		AllRows.Sort([](const TSharedPtr<FTSAVDMXConsoleListItem>& A, const TSharedPtr<FTSAVDMXConsoleListItem>& B) { return A->Label < B->Label; });
	}
	ApplyFilter();
}

void STSAVDMXLightingConsoleTool::ApplyFilter()
{
	FilteredRows.Reset();
	const FString Query = SearchText.TrimStartAndEnd().ToLower();
	for (const TSharedPtr<FTSAVDMXConsoleListItem>& Item : AllRows)
	{
		const FString Searchable = FString::Printf(TEXT("%s u%d.%03d"), *Item->Label, Item->Universe, Item->Address).ToLower();
		if (Query.IsEmpty() || Searchable.Contains(Query))
		{
			FilteredRows.Add(Item);
		}
	}
	if (FixtureList.IsValid())
	{
		FixtureList->RequestListRefresh();
	}
}

TSharedRef<ITableRow> STSAVDMXLightingConsoleTool::GenerateFixtureRow(
	TSharedPtr<FTSAVDMXConsoleListItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FTSAVDMXConsoleListItem>>, OwnerTable)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f)
		[
			SNew(SCheckBox).IsChecked_Lambda([this, Item]() { return IsFixtureSelected(Item->DefinitionId); })
			.OnCheckStateChanged_Lambda([this, Item](ECheckBoxState State) { SetFixtureSelected(Item->DefinitionId, State); })
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(3.0f)
		[
			SNew(STextBlock).Text(FText::FromString(Item->Label)).ToolTipText(FText::FromString(Item->Label))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3.0f)
		[
			SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("U%d.%03d  |  %d placed"), Item->Universe, Item->Address, Item->ActorCount))).ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]
	];
}

void STSAVDMXLightingConsoleTool::SearchChanged(const FText& Text)
{
	SearchText = Text.ToString();
	ApplyFilter();
}

void STSAVDMXLightingConsoleTool::SetFixtureSelected(const FName DefinitionId, const ECheckBoxState State)
{
	if (State == ECheckBoxState::Checked)
	{
		SelectedDefinitionIds.Add(DefinitionId);
	}
	else
	{
		SelectedDefinitionIds.Remove(DefinitionId);
	}
	RebuildAttributeFaders();
}

ECheckBoxState STSAVDMXLightingConsoleTool::IsFixtureSelected(const FName DefinitionId) const
{
	return SelectedDefinitionIds.Contains(DefinitionId) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

TSharedRef<SWidget> STSAVDMXLightingConsoleTool::MakeCommonControl(const FText& Label, float* Value)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(92.0f)[SNew(STextBlock).Text(Label)]]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(SSlider).PreventThrottling(true).Value_Lambda([Value]() { return *Value; }).OnValueChanged_Lambda([this, Value](float NewValue) { *Value = NewValue; SendCommonValues(); })
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride(76.0f)
			[
				SNew(SNumericEntryBox<float>).MinValue(0.0f).MaxValue(1.0f).MinSliderValue(0.0f).MaxSliderValue(1.0f)
				.Value_Lambda([Value]() { return *Value; }).OnValueChanged_Lambda([this, Value](float NewValue) { *Value = FMath::Clamp(NewValue, 0.0f, 1.0f); SendCommonValues(); })
			]
		];
}

void STSAVDMXLightingConsoleTool::SendCommonValues()
{
	if (!Catalog || SelectedDefinitionIds.IsEmpty())
	{
		return;
	}
	TSAVDMXEditorUtils::FControlValues EffectiveValues = Values;
	EffectiveValues.Dimmer = bBlackout ? 0.0f : Values.Dimmer * GrandMaster;
	int32 SentCount = 0;
	for (const FName DefinitionId : SelectedDefinitionIds)
	{
		if (const FTSAVDMXFixtureDefinition* Definition = FindDefinition(DefinitionId))
		{
			SentCount += TSAVDMXEditorUtils::SendControlValues(*Definition, EffectiveValues, false) ? 1 : 0;
		}
	}
	SetStatus(FString::Printf(TEXT("Live DMX sent to %d selected fixture patch%s%s."), SentCount, SentCount == 1 ? TEXT("") : TEXT("es"), bBlackout ? TEXT(" (blackout active)") : TEXT("")), SentCount > 0);
}

void STSAVDMXLightingConsoleTool::SendAttributeValue(const FName AttributeName, const float Value)
{
	int32 SentCount = 0;
	for (const FName DefinitionId : SelectedDefinitionIds)
	{
		if (const FTSAVDMXFixtureDefinition* Definition = FindDefinition(DefinitionId))
		{
			SentCount += TSAVDMXEditorUtils::SendAttributeValue(*Definition, AttributeName, Value) ? 1 : 0;
		}
	}
	SetStatus(FString::Printf(TEXT("%s = %.3f sent to %d compatible selected patch%s."), *AttributeName.ToString(), Value, SentCount, SentCount == 1 ? TEXT("") : TEXT("es")), SentCount > 0);
}

void STSAVDMXLightingConsoleTool::RebuildAttributeFaders()
{
	if (!AttributeFaders.IsValid())
	{
		return;
	}
	AttributeFaders->ClearChildren();
	if (SelectedDefinitionIds.IsEmpty())
	{
		AttributeFaders->AddSlot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("SelectForAttributes", "Select a fixture to expose its active mode attributes.")).ColorAndOpacity(FSlateColor::UseSubduedForeground())];
		return;
	}

	const FTSAVDMXFixtureDefinition* PrimaryDefinition = nullptr;
	for (const TSharedPtr<FTSAVDMXConsoleListItem>& Item : AllRows)
	{
		if (SelectedDefinitionIds.Contains(Item->DefinitionId))
		{
			PrimaryDefinition = FindDefinition(Item->DefinitionId);
			break;
		}
	}
	UDMXEntityFixturePatch* Patch = PrimaryDefinition ? TSAVDMXEditorUtils::ResolvePatch(*PrimaryDefinition) : nullptr;
	const FDMXFixtureMode* Mode = Patch ? Patch->GetActiveMode() : nullptr;
	if (!Mode)
	{
		AttributeFaders->AddSlot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("MissingMode", "The primary selected patch has no active mode."))];
		return;
	}

	AttributeFaders->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
	[
		SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s — %s (%d functions)"), *PrimaryDefinition->DisplayName.ToString(), *Mode->ModeName, Mode->Functions.Num()))).ColorAndOpacity(FSlateColor::UseSubduedForeground())
	];
	for (const FDMXFixtureFunction& Function : Mode->Functions)
	{
		const FName AttributeName = Function.Attribute.Name;
		if (AttributeName.IsNone() || AttributeValues.Contains(AttributeName))
		{
			continue;
		}
		AttributeValues.Add(AttributeName, 0.0f);
		AttributeFaders->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(150.0f)[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s  [ch %d]"), *AttributeName.ToString(), Function.Channel))).ToolTipText(FText::FromString(Function.FunctionName))]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SSlider).PreventThrottling(true)
				.Value_Lambda([this, AttributeName]() { return AttributeValues.FindRef(AttributeName); })
				.OnValueChanged_Lambda([this, AttributeName](float NewValue) { AttributeValues.FindOrAdd(AttributeName) = NewValue; SendAttributeValue(AttributeName, NewValue); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(76.0f)
				[
					SNew(SNumericEntryBox<float>).MinValue(0.0f).MaxValue(1.0f).Value_Lambda([this, AttributeName]() { return AttributeValues.FindRef(AttributeName); })
					.OnValueChanged_Lambda([this, AttributeName](float NewValue) { AttributeValues.FindOrAdd(AttributeName) = NewValue; SendAttributeValue(AttributeName, NewValue); })
				]
			]
		];
	}
}

const FTSAVDMXFixtureDefinition* STSAVDMXLightingConsoleTool::FindDefinition(const FName DefinitionId) const
{
	return Catalog ? Catalog->FindFixture(DefinitionId) : nullptr;
}

FReply STSAVDMXLightingConsoleTool::RefreshClicked()
{
	RefreshRows();
	RebuildAttributeFaders();
	SetStatus(FString::Printf(TEXT("Loaded %d of 607 patches; %d match the current filter."), AllRows.Num(), FilteredRows.Num()), AllRows.Num() == 607);
	return FReply::Handled();
}

FReply STSAVDMXLightingConsoleTool::SelectVisibleClicked()
{
	for (const TSharedPtr<FTSAVDMXConsoleListItem>& Item : FilteredRows)
	{
		SelectedDefinitionIds.Add(Item->DefinitionId);
	}
	if (FixtureList.IsValid())
	{
		FixtureList->RequestListRefresh();
	}
	RebuildAttributeFaders();
	return FReply::Handled();
}

FReply STSAVDMXLightingConsoleTool::SelectPlacedClicked()
{
	SelectedDefinitionIds.Reset();
	for (const TSharedPtr<FTSAVDMXConsoleListItem>& Item : AllRows)
	{
		if (Item->ActorCount > 0)
		{
			SelectedDefinitionIds.Add(Item->DefinitionId);
		}
	}
	if (FixtureList.IsValid())
	{
		FixtureList->RequestListRefresh();
	}
	RebuildAttributeFaders();
	SetStatus(FString::Printf(TEXT("Selected %d fixture type%s currently placed in the level."), SelectedDefinitionIds.Num(), SelectedDefinitionIds.Num() == 1 ? TEXT("") : TEXT("s")), !SelectedDefinitionIds.IsEmpty());
	return FReply::Handled();
}

FReply STSAVDMXLightingConsoleTool::ClearSelectionClicked()
{
	SelectedDefinitionIds.Reset();
	if (FixtureList.IsValid())
	{
		FixtureList->RequestListRefresh();
	}
	RebuildAttributeFaders();
	return FReply::Handled();
}

FReply STSAVDMXLightingConsoleTool::HomeClicked()
{
	Values.Pan = Values.Tilt = 0.5f;
	Values.Zoom = 0.0f;
	bBlackout = false;
	SendCommonValues();
	return FReply::Handled();
}

FReply STSAVDMXLightingConsoleTool::FullClicked()
{
	Values.Dimmer = Values.Red = Values.Green = Values.Blue = GrandMaster = 1.0f;
	bBlackout = false;
	SendCommonValues();
	return FReply::Handled();
}

FReply STSAVDMXLightingConsoleTool::WhiteClicked()
{
	Values.Red = Values.Green = Values.Blue = 1.0f;
	bBlackout = false;
	SendCommonValues();
	return FReply::Handled();
}

FReply STSAVDMXLightingConsoleTool::RedClicked()
{
	Values.Red = 1.0f; Values.Green = Values.Blue = 0.0f; bBlackout = false; SendCommonValues(); return FReply::Handled();
}

FReply STSAVDMXLightingConsoleTool::GreenClicked()
{
	Values.Green = 1.0f; Values.Red = Values.Blue = 0.0f; bBlackout = false; SendCommonValues(); return FReply::Handled();
}

FReply STSAVDMXLightingConsoleTool::BlueClicked()
{
	Values.Blue = 1.0f; Values.Red = Values.Green = 0.0f; bBlackout = false; SendCommonValues(); return FReply::Handled();
}

FReply STSAVDMXLightingConsoleTool::BlackoutClicked()
{
	bBlackout = true;
	SendCommonValues();
	return FReply::Handled();
}

FReply STSAVDMXLightingConsoleTool::ReleaseBlackoutClicked()
{
	bBlackout = false;
	SendCommonValues();
	return FReply::Handled();
}

FText STSAVDMXLightingConsoleTool::GetSelectionText() const
{
	return FText::FromString(FString::Printf(TEXT("%d selected | %d shown | %d total"), SelectedDefinitionIds.Num(), FilteredRows.Num(), AllRows.Num()));
}

FSlateColor STSAVDMXLightingConsoleTool::GetStatusColor() const
{
	return bStatusSuccess ? FLinearColor(0.15f, 0.8f, 0.35f) : FLinearColor(0.95f, 0.2f, 0.15f);
}

void STSAVDMXLightingConsoleTool::SetStatus(const FString& Message, const bool bSuccess)
{
	StatusText = FText::FromString(Message);
	bStatusSuccess = bSuccess;
}

#undef LOCTEXT_NAMESPACE
