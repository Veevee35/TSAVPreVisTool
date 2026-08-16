// Copyright TSAV. All Rights Reserved.

#include "STSAVDMXPatchTool.h"

#include "Library/DMXEntityFixturePatch.h"
#include "Styling/AppStyle.h"
#include "TSAVDMXFixtureCatalog.h"
#include "Widgets/Input/SButton.h"
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
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "STSAVDMXPatchTool"

namespace TSAVDMXPatchTool::Private
{
	class SPatchRow final : public SMultiColumnTableRow<TSharedPtr<FTSAVDMXPatchListItem>>
	{
	public:
		SLATE_BEGIN_ARGS(SPatchRow) {}
			SLATE_ARGUMENT(TSharedPtr<FTSAVDMXPatchListItem>, Item)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable)
		{
			Item = InArgs._Item;
			SMultiColumnTableRow::Construct(FSuperRowType::FArguments().Padding(2.0f), OwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			FText Text;
			if (ColumnName == TEXT("Fixture"))
			{
				Text = FText::FromString(FString::Printf(TEXT("%s — %s"), *Item->Manufacturer, *Item->FixtureName));
			}
			else if (ColumnName == TEXT("Mode"))
			{
				Text = FText::FromString(Item->ModeName);
			}
			else if (ColumnName == TEXT("Patch"))
			{
				Text = FText::FromString(FString::Printf(TEXT("U%d.%03d–%03d"), Item->Universe, Item->Address, Item->Address + Item->Span - 1));
			}
			else if (ColumnName == TEXT("Channels"))
			{
				Text = FText::AsNumber(Item->Span);
			}
			else
			{
				Text = Item->bPatchValid ? LOCTEXT("PatchReady", "Ready") : LOCTEXT("PatchMissing", "Missing");
			}
			return SNew(STextBlock)
				.Text(Text)
				.ToolTipText(Text)
				.ColorAndOpacity(ColumnName == TEXT("Status") && !Item->bPatchValid
					? FSlateColor(FLinearColor(0.95f, 0.2f, 0.15f)) : FSlateColor::UseForeground());
		}

	private:
		TSharedPtr<FTSAVDMXPatchListItem> Item;
	};
}

void STSAVDMXPatchTool::Construct(const FArguments& InArgs)
{
	Catalog = TSAVDMXEditorUtils::LoadCatalog();
	StatusText = Catalog
		? LOCTEXT("ReadyStatus", "Ready. Select any catalog entry to patch, spawn, or test it.")
		: LOCTEXT("MissingCatalogStatus", "Fixture catalog missing. Run Tools > Build Complete GDTF Fixture Library.");
	using namespace TSAVDMXPatchTool::Private;

	ChildSlot
	[
		SNew(SBorder)
		.Padding(12.0f)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(LOCTEXT("Title", "TSAV DMX Patch & Fixture Test")).Font(FAppStyle::GetFontStyle(TEXT("HeadingLarge")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Subtitle", "Patch, validate, spawn, and exercise all 607 generated GDTF fixtures. Tests transmit through the real master-library patch and update matching viewport fixtures."))
				.AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(SearchBox, SSearchBox).HintText(LOCTEXT("SearchHint", "Search manufacturer, fixture, mode, or patch…")).OnTextChanged(this, &STSAVDMXPatchTool::SearchChanged)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f)
				[
					SNew(SButton).Text(LOCTEXT("Refresh", "Refresh 607 Fixtures")).OnClicked(this, &STSAVDMXPatchTool::RefreshClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SButton).Text(LOCTEXT("Repack", "Auto-Repack All")).OnClicked(this, &STSAVDMXPatchTool::RepackClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).Text(LOCTEXT("Validate", "Validate All Patches")).OnClicked(this, &STSAVDMXPatchTool::ValidateClicked)
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SSplitter)
				+ SSplitter::Slot().Value(0.67f)
				[
					SAssignNew(PatchList, SListView<TSharedPtr<FTSAVDMXPatchListItem>>)
					.ListItemsSource(&FilteredRows)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow(this, &STSAVDMXPatchTool::GenerateRow)
					.OnSelectionChanged(this, &STSAVDMXPatchTool::SelectionChanged)
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column(TEXT("Fixture")).DefaultLabel(LOCTEXT("FixtureColumn", "Fixture")).FillWidth(0.42f)
						+ SHeaderRow::Column(TEXT("Mode")).DefaultLabel(LOCTEXT("ModeColumn", "Mode")).FillWidth(0.25f)
						+ SHeaderRow::Column(TEXT("Patch")).DefaultLabel(LOCTEXT("PatchColumn", "Patch")).FillWidth(0.16f)
						+ SHeaderRow::Column(TEXT("Channels")).DefaultLabel(LOCTEXT("ChannelsColumn", "Ch")).FixedWidth(48.0f)
						+ SHeaderRow::Column(TEXT("Status")).DefaultLabel(LOCTEXT("StatusColumn", "Status")).FixedWidth(62.0f)
					)
				]
				+ SSplitter::Slot().Value(0.33f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f)
						[
							SNew(STextBlock).Text(this, &STSAVDMXPatchTool::GetSelectionSummary).AutoWrapText(true).Font(FAppStyle::GetFontStyle(TEXT("HeadingMedium")))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)
						[
							SNew(SGridPanel)
							+ SGridPanel::Slot(0, 0).Padding(2.0f)[SNew(STextBlock).Text(LOCTEXT("Universe", "Universe"))]
							+ SGridPanel::Slot(1, 0).Padding(2.0f)[SNew(SNumericEntryBox<int32>).MinValue(1).MaxValue(63999).Value_Lambda([this]() { return EditedUniverse; }).OnValueChanged_Lambda([this](int32 Value) { EditedUniverse = Value; })]
							+ SGridPanel::Slot(0, 1).Padding(2.0f)[SNew(STextBlock).Text(LOCTEXT("Address", "Address"))]
							+ SGridPanel::Slot(1, 1).Padding(2.0f)[SNew(SNumericEntryBox<int32>).MinValue(1).MaxValue(512).Value_Lambda([this]() { return EditedAddress; }).OnValueChanged_Lambda([this](int32 Value) { EditedAddress = Value; })]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 5.0f)
						[
							SNew(SButton).Text(LOCTEXT("ApplyAddress", "Apply Address (overlap-safe)")).HAlign(HAlign_Center).OnClicked(this, &STSAVDMXPatchTool::ApplyAddressClicked)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 5.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 3.0f, 0.0f)[SNew(SButton).Text(LOCTEXT("Spawn", "Spawn in Level")).HAlign(HAlign_Center).OnClicked(this, &STSAVDMXPatchTool::SpawnClicked)]
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f, 0.0f)[SNew(SButton).Text(LOCTEXT("SelectPlaced", "Select Placed")).HAlign(HAlign_Center).OnClicked(this, &STSAVDMXPatchTool::SelectPlacedClicked)]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 10.0f, 8.0f, 3.0f)[SNew(STextBlock).Text(LOCTEXT("TestHeading", "LIVE FIXTURE TEST")).Font(FAppStyle::GetFontStyle(TEXT("HeadingSmall")))]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)[MakeTestControl(LOCTEXT("Pan", "Pan"), &TestValues.Pan)]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)[MakeTestControl(LOCTEXT("Tilt", "Tilt"), &TestValues.Tilt)]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)[MakeTestControl(LOCTEXT("Dimmer", "Dimmer"), &TestValues.Dimmer)]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)[MakeTestControl(LOCTEXT("Red", "Red"), &TestValues.Red)]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)[MakeTestControl(LOCTEXT("Green", "Green"), &TestValues.Green)]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)[MakeTestControl(LOCTEXT("Blue", "Blue"), &TestValues.Blue)]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)[MakeTestControl(LOCTEXT("Zoom", "Zoom"), &TestValues.Zoom)]
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 6.0f)
						[
							SNew(SGridPanel)
							+ SGridPanel::Slot(0, 0).Padding(2.0f)[SNew(SButton).Text(LOCTEXT("Home", "Home")).OnClicked(this, &STSAVDMXPatchTool::HomeClicked)]
							+ SGridPanel::Slot(1, 0).Padding(2.0f)[SNew(SButton).Text(LOCTEXT("FullWhite", "Full White")).OnClicked(this, &STSAVDMXPatchTool::FullWhiteClicked)]
							+ SGridPanel::Slot(0, 1).Padding(2.0f)[SNew(SButton).Text(LOCTEXT("SendTest", "Send Test")).OnClicked(this, &STSAVDMXPatchTool::TestClicked)]
							+ SGridPanel::Slot(1, 1).Padding(2.0f)[SNew(SButton).Text(LOCTEXT("Blackout", "Blackout")).OnClicked(this, &STSAVDMXPatchTool::BlackoutClicked)]
						]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock).Text_Lambda([this]() { return StatusText; }).ColorAndOpacity(this, &STSAVDMXPatchTool::GetStatusColor).AutoWrapText(true)
			]
		]
	];
	RefreshRows();
}

void STSAVDMXPatchTool::RefreshRows()
{
	const FName PreviousSelection = SelectedDefinitionId;
	AllRows.Reset();
	Catalog = TSAVDMXEditorUtils::LoadCatalog();
	if (Catalog)
	{
		for (const FTSAVDMXFixtureDefinition& Definition : Catalog->Fixtures)
		{
			UDMXEntityFixturePatch* Patch = TSAVDMXEditorUtils::ResolvePatch(Definition);
			TSharedPtr<FTSAVDMXPatchListItem> Row = MakeShared<FTSAVDMXPatchListItem>();
			Row->DefinitionId = Definition.DefinitionId;
			Row->Manufacturer = Definition.Manufacturer.IsEmpty() ? TEXT("Unknown") : Definition.Manufacturer.ToString();
			Row->FixtureName = Definition.DisplayName.ToString();
			Row->ModeName = Definition.GDTFModeName.IsEmpty() ? TEXT("Default") : Definition.GDTFModeName;
			Row->Universe = Patch ? Patch->GetUniverseID() : Definition.Universe;
			Row->Address = Patch ? Patch->GetStartingChannel() : Definition.Address;
			Row->Span = Patch ? FMath::Max(Patch->GetChannelSpan(), 1) : FMath::Max(Definition.ChannelSpan, 1);
			Row->bPatchValid = Patch && Patch->GetActiveMode();
			AllRows.Add(Row);
		}
		AllRows.Sort([](const TSharedPtr<FTSAVDMXPatchListItem>& A, const TSharedPtr<FTSAVDMXPatchListItem>& B)
		{
			return A->Manufacturer == B->Manufacturer ? A->FixtureName < B->FixtureName : A->Manufacturer < B->Manufacturer;
		});
	}
	ApplyFilter();
	if (PatchList.IsValid() && !PreviousSelection.IsNone())
	{
		if (const TSharedPtr<FTSAVDMXPatchListItem>* Match = FilteredRows.FindByPredicate([PreviousSelection](const TSharedPtr<FTSAVDMXPatchListItem>& Row) { return Row->DefinitionId == PreviousSelection; }))
		{
			PatchList->SetSelection(*Match);
		}
	}
}

void STSAVDMXPatchTool::ApplyFilter()
{
	FilteredRows.Reset();
	const FString Query = SearchText.TrimStartAndEnd().ToLower();
	for (const TSharedPtr<FTSAVDMXPatchListItem>& Row : AllRows)
	{
		const FString Searchable = FString::Printf(TEXT("%s %s %s u%d.%03d"), *Row->Manufacturer, *Row->FixtureName, *Row->ModeName, Row->Universe, Row->Address).ToLower();
		if (Query.IsEmpty() || Searchable.Contains(Query))
		{
			FilteredRows.Add(Row);
		}
	}
	if (PatchList.IsValid())
	{
		PatchList->RequestListRefresh();
	}
}

TSharedRef<ITableRow> STSAVDMXPatchTool::GenerateRow(TSharedPtr<FTSAVDMXPatchListItem> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(TSAVDMXPatchTool::Private::SPatchRow, OwnerTable).Item(Item);
}

void STSAVDMXPatchTool::SelectionChanged(TSharedPtr<FTSAVDMXPatchListItem> Item, ESelectInfo::Type SelectionType)
{
	SelectedDefinitionId = Item ? Item->DefinitionId : NAME_None;
	if (Item)
	{
		EditedUniverse = Item->Universe;
		EditedAddress = Item->Address;
	}
}

void STSAVDMXPatchTool::SearchChanged(const FText& Text)
{
	SearchText = Text.ToString();
	ApplyFilter();
}

const FTSAVDMXFixtureDefinition* STSAVDMXPatchTool::GetSelectedDefinition() const
{
	return Catalog ? Catalog->FindFixture(SelectedDefinitionId) : nullptr;
}

TSharedRef<SWidget> STSAVDMXPatchTool::MakeTestControl(const FText& Label, float* Value)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(58.0f)[SNew(STextBlock).Text(Label)]]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(SSlider).PreventThrottling(true).Value_Lambda([Value]() { return *Value; })
			.OnValueChanged_Lambda([this, Value](float NewValue) { *Value = NewValue; SendSelectedTest(false); })
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride(76.0f)
			[
				SNew(SNumericEntryBox<float>).MinValue(0.0f).MaxValue(1.0f).MinSliderValue(0.0f).MaxSliderValue(1.0f)
				.Value_Lambda([Value]() { return *Value; })
				.OnValueChanged_Lambda([this, Value](float NewValue) { *Value = FMath::Clamp(NewValue, 0.0f, 1.0f); SendSelectedTest(false); })
			]
		];
}

void STSAVDMXPatchTool::SendSelectedTest(const bool bSnap)
{
	if (const FTSAVDMXFixtureDefinition* Definition = GetSelectedDefinition())
	{
		TSAVDMXEditorUtils::SendControlValues(*Definition, TestValues, bSnap);
	}
}

FReply STSAVDMXPatchTool::RefreshClicked()
{
	RefreshRows();
	SetStatus(FString::Printf(TEXT("Loaded %d of 607 fixture patches; %d match the current filter."), AllRows.Num(), FilteredRows.Num()), AllRows.Num() == 607);
	return FReply::Handled();
}

FReply STSAVDMXPatchTool::RepackClicked()
{
	if (!Catalog)
	{
		SetStatus(TEXT("Fixture catalog is missing."), false);
		return FReply::Handled();
	}
	FString Message;
	const bool bSuccess = TSAVDMXEditorUtils::RepackCatalog(*Catalog, true, Message);
	SetStatus(Message, bSuccess);
	if (bSuccess)
	{
		RefreshRows();
	}
	return FReply::Handled();
}

FReply STSAVDMXPatchTool::ValidateClicked()
{
	if (!Catalog)
	{
		SetStatus(TEXT("Fixture catalog is missing."), false);
		return FReply::Handled();
	}
	FString Summary;
	TArray<FString> Errors;
	const bool bValid = TSAVDMXEditorUtils::ValidateCatalog(*Catalog, Summary, Errors);
	if (!Errors.IsEmpty())
	{
		Summary += TEXT(" | First issue: ") + Errors[0];
	}
	SetStatus(Summary, bValid);
	return FReply::Handled();
}

FReply STSAVDMXPatchTool::ApplyAddressClicked()
{
	if (!Catalog || SelectedDefinitionId.IsNone())
	{
		SetStatus(TEXT("Select a fixture before changing its patch."), false);
		return FReply::Handled();
	}
	FString Message;
	const bool bSuccess = TSAVDMXEditorUtils::UpdatePatchAddress(*Catalog, SelectedDefinitionId, EditedUniverse, EditedAddress, Message);
	SetStatus(Message, bSuccess);
	if (bSuccess)
	{
		RefreshRows();
	}
	return FReply::Handled();
}

FReply STSAVDMXPatchTool::SpawnClicked()
{
	const FTSAVDMXFixtureDefinition* Definition = GetSelectedDefinition();
	const bool bSuccess = Definition && TSAVDMXEditorUtils::SpawnFixture(*Definition);
	SetStatus(bSuccess ? TEXT("Spawned and selected a patched fixture in front of the active viewport.") : TEXT("Select a valid fixture before spawning."), bSuccess);
	return FReply::Handled();
}

FReply STSAVDMXPatchTool::SelectPlacedClicked()
{
	const FTSAVDMXFixtureDefinition* Definition = GetSelectedDefinition();
	const bool bSuccess = Definition && TSAVDMXEditorUtils::SelectMatchingActors(*Definition);
	SetStatus(bSuccess ? TEXT("Selected every placed actor using this fixture patch.") : TEXT("No placed actor currently uses this fixture patch."), bSuccess);
	return FReply::Handled();
}

FReply STSAVDMXPatchTool::HomeClicked()
{
	TestValues.Pan = 0.5f;
	TestValues.Tilt = 0.5f;
	TestValues.Zoom = 0.0f;
	SendSelectedTest(true);
	return FReply::Handled();
}

FReply STSAVDMXPatchTool::FullWhiteClicked()
{
	TestValues.Dimmer = TestValues.Red = TestValues.Green = TestValues.Blue = 1.0f;
	SendSelectedTest(false);
	return FReply::Handled();
}

FReply STSAVDMXPatchTool::TestClicked()
{
	SendSelectedTest(false);
	return FReply::Handled();
}

FReply STSAVDMXPatchTool::BlackoutClicked()
{
	TestValues.Dimmer = 0.0f;
	SendSelectedTest(true);
	return FReply::Handled();
}

FText STSAVDMXPatchTool::GetSelectionSummary() const
{
	if (const FTSAVDMXFixtureDefinition* Definition = GetSelectedDefinition())
	{
		return FText::FromString(FString::Printf(TEXT("%s\n%s | %s\n%d channels | U%d.%03d"),
			*Definition->DisplayName.ToString(),
			Definition->Manufacturer.IsEmpty() ? TEXT("Unknown manufacturer") : *Definition->Manufacturer.ToString(),
			Definition->GDTFModeName.IsEmpty() ? TEXT("Default mode") : *Definition->GDTFModeName,
			Definition->ChannelSpan, EditedUniverse, EditedAddress));
	}
	return LOCTEXT("NoSelection", "Select one of the 607 fixture patches.");
}

FSlateColor STSAVDMXPatchTool::GetStatusColor() const
{
	return bStatusSuccess ? FLinearColor(0.15f, 0.8f, 0.35f) : FLinearColor(0.95f, 0.2f, 0.15f);
}

void STSAVDMXPatchTool::SetStatus(const FString& Message, const bool bSuccess)
{
	StatusText = FText::FromString(Message);
	bStatusSuccess = bSuccess;
}

#undef LOCTEXT_NAMESPACE
