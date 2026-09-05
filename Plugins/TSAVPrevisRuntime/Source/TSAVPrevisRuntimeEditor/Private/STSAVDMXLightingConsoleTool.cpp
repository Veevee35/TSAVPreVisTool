// Copyright TSAV. All Rights Reserved.

#include "STSAVDMXLightingConsoleTool.h"
#include "TSAVDMXPatchPlan.h"
#include "TSAVSuperStageDMX.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TSAVDMXFixture.h"
#include "Lighting/TSAVLightingShow.h"
#include "ScopedTransaction.h"
#include "Framework/Docking/TabManager.h"

#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXLibrary.h"
#include "IO/DMXOutputPort.h"
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

namespace
{
	bool SendToNativeShow(AActor* Actor, const TMap<FName, float>& Attributes)
	{
		if (auto* Fixture = Cast<ATSAVDMXFixture>(Actor))
			if (auto* Show = ATSAVLightingShow::Find(Fixture->GetWorld())) { Show->SetProgrammer({Fixture}, Attributes); return true; }
		return false;
	}
}

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
				.Text(LOCTEXT("Subtitle", "Place fixtures from the TSAV library, then select their scene rows to patch and control individual lights. Each fixture uses its own mode attributes."))
				.AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
			[SNew(SButton).Text(LOCTEXT("OpenShowPlayback", "Open Lighting Show — groups, presets, cues, effects and recording"))
				.OnClicked_Lambda([] { FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("TSAVLightingShow"))); return FReply::Handled(); })]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SSplitter)
				+ SSplitter::Slot().Value(0.42f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(SearchBox, SSearchBox).HintText(LOCTEXT("SearchHint", "Search fixture, ID, universe, or address…")).OnTextChanged(this, &STSAVDMXLightingConsoleTool::SearchChanged)
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
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)[SNew(SButton).Text(LOCTEXT("PlaceSelected", "Place selected library fixtures")).OnClicked(this, &STSAVDMXLightingConsoleTool::PlaceSelectedClicked)]
						+ SVerticalBox::Slot().AutoHeight()[SNew(SCheckBox)
							.IsChecked_Lambda([this] { return bSceneOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
							.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bSceneOnly = State == ECheckBoxState::Checked; ApplyFilter(); })
							[SNew(STextBlock).Text(LOCTEXT("SceneOnly", "Show placed fixtures only"))]]
						+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(this, &STSAVDMXLightingConsoleTool::GetSelectionText).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
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
						+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 5.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("PatchHeading", "PATCH SELECTED FIXTURES")).Font(FAppStyle::GetFontStyle(TEXT("HeadingSmall")))]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[SNew(STextBlock).Text(this, &STSAVDMXLightingConsoleTool::GetPatchSummary).AutoWrapText(true)]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(LOCTEXT("PatchUniverse", "Universe  "))]
								+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 8.0f, 0.0f)[SNew(SNumericEntryBox<int32>).MinValue(1).MaxValue(63999)
									.Value_Lambda([this] { return EditedUniverse; }).OnValueChanged_Lambda([this](int32 Value) { EditedUniverse = Value; bPatchDraftDirty = true; })]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(LOCTEXT("PatchAddress", "Start address  "))]
								+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SNumericEntryBox<int32>).MinValue(1).MaxValue(512)
									.Value_Lambda([this] { return EditedAddress; }).OnValueChanged_Lambda([this](int32 Value) { EditedAddress = Value; bPatchDraftDirty = true; })]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 4.0f, 0.0f)[SNew(SButton).Text(LOCTEXT("PatchApply", "Patch selected in list order")).OnClicked(this, &STSAVDMXLightingConsoleTool::ApplyPatchClicked)]
								+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("UsePrimary", "Use primary address")).OnClicked(this, &STSAVDMXLightingConsoleTool::UsePrimaryPatchClicked)]
							]
							+ SVerticalBox::Slot().AutoHeight()[SNew(SButton).Visibility_Lambda([] { return FindObject<UClass>(nullptr, TEXT("/Script/SuperCore.SuperDmxActorBase")) ? EVisibility::Visible : EVisibility::Collapsed; }).Text(LOCTEXT("SyncSuperStage", "Sync scene patch with SuperStage console")).OnClicked(this, &STSAVDMXLightingConsoleTool::SyncSuperStageClicked)]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[SNew(SCheckBox)
								.Visibility_Lambda([] { return FindObject<UClass>(nullptr, TEXT("/Script/SuperCore.SuperDmxActorBase")) ? EVisibility::Visible : EVisibility::Collapsed; })
								.IsChecked_Lambda([] { return TSAVSuperStageDMX::IsOutputEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
								.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { const bool bOK = TSAVSuperStageDMX::SetOutputEnabled(State == ECheckBoxState::Checked); SetStatus(bOK ? TEXT("SuperStage console output updated.") : TEXT("Open this project with SuperStage enabled and activate its console first."), bOK); })
								[SNew(STextBlock).Text(LOCTEXT("SuperOutput", "SuperStage console output enabled"))]]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
							[SNew(STextBlock).Visibility_Lambda([] { return FindObject<UClass>(nullptr, TEXT("/Script/SuperCore.SuperDmxActorBase")) ? EVisibility::Visible : EVisibility::Collapsed; }).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground()).Text(LOCTEXT("SuperStageSetupHelp", "For optional SuperStage lights, sign in through the vendor panel and use DMX control mode. TSAV fixtures do not need SuperStage."))]
							+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground()).Text(LOCTEXT("PatchHelp", "Multiple fixtures are placed consecutively, rolling to the next universe when needed. Overlaps are rejected. Save All to keep patch edits."))]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
							[SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground()).Text(LOCTEXT("LibraryPatchHelp", "Scene rows patch individual lights. Library rows edit shared templates. Patch newly placed copies to separate addresses before controlling them independently."))]
						]
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

void STSAVDMXLightingConsoleTool::RefreshRows(const bool bReloadCatalog)
{
	TMap<FName, FString> PreviousSignatures;
	TSet<FName> PreviousVendorIds;
	for (const auto& Row : AllRows)
	{
		PreviousSignatures.Add(Row->DefinitionId, FString::Printf(TEXT("%s:%d:%d:%d:%d:%s"), *Row->Label, Row->Universe, Row->Address, Row->Span, Row->ActorCount, *Row->ModeSignature));
		if (Row->bSuperStage) PreviousVendorIds.Add(Row->DefinitionId);
	}
	AllRows.Reset();
	if (bReloadCatalog) Catalog = TSAVDMXEditorUtils::LoadCatalog();
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	// Enumerate the level once per refresh, instead of once per catalog entry.
	TMap<FName, TSet<ATSAVDMXFixture*>> ActorsByDefinition;
	TMap<UDMXEntityFixturePatch*, TSet<ATSAVDMXFixture*>> ActorsByPatch;
	if (World)
	{
		for (TActorIterator<ATSAVDMXFixture> It(World); It; ++It)
		{
			ActorsByDefinition.FindOrAdd(It->FixtureDefinitionId).Add(*It);
			if (UDMXEntityFixturePatch* Patch = It->GetFixturePatch()) ActorsByPatch.FindOrAdd(Patch).Add(*It);
			auto Item = MakeShared<FTSAVDMXConsoleListItem>();
			Item->DefinitionId = FName(*(TEXT("TSAVScene:") + It->GetActorGuid().ToString()));
			Item->Label = TEXT("[TSAV scene] ") + It->GetActorLabel();
			Item->Actor = *It;
			Item->ActorCount = 1;
			Item->bNativeActor = true;
			if (UDMXEntityFixturePatch* Patch = It->GetFixturePatch())
			{
				Item->Universe = Patch->GetUniverseID();
				Item->Address = Patch->GetStartingChannel();
				Item->Span = Patch->GetChannelSpan();
				if (const FDMXFixtureMode* Mode = Patch->GetActiveMode())
					for (const auto& Function : Mode->Functions)
						Item->ModeSignature += FString::Printf(TEXT("%s:%d;"), *Function.Attribute.Name.ToString(), Function.Channel);
			}
			AllRows.Add(Item);
		}
	}
	if (Catalog)
	{
		for (const FTSAVDMXFixtureDefinition& Definition : Catalog->Fixtures)
		{
			UDMXEntityFixturePatch* Patch = TSAVDMXEditorUtils::ResolvePatch(Definition);
			TSharedPtr<FTSAVDMXConsoleListItem> Item = MakeShared<FTSAVDMXConsoleListItem>();
			Item->DefinitionId = Definition.DefinitionId;
			Item->Label = FString::Printf(TEXT("[Library] %s — %s"), Definition.Manufacturer.IsEmpty() ? TEXT("Unknown") : *Definition.Manufacturer.ToString(), *Definition.DisplayName.ToString());
			Item->Universe = Patch ? Patch->GetUniverseID() : Definition.Universe;
			Item->Address = Patch ? Patch->GetStartingChannel() : Definition.Address;
			Item->ActorCount = ActorsByDefinition.FindRef(Definition.DefinitionId).Union(ActorsByPatch.FindRef(Patch)).Num();
			Item->Span = Patch ? Patch->GetChannelSpan() : Definition.ChannelSpan;
			if (const FDMXFixtureMode* Mode = Patch ? Patch->GetActiveMode() : nullptr)
			{
				for (const auto& Function : Mode->Functions)
					Item->ModeSignature += FString::Printf(TEXT("%s:%d;"), *Function.Attribute.Name.ToString(), Function.Channel);
			}
			AllRows.Add(Item);
		}
	}
	for (const auto& Fixture : TSAVSuperStageDMX::GetSceneFixtures(World))
	{
		AActor* Actor = Fixture.Actor.Get();
		auto Item = MakeShared<FTSAVDMXConsoleListItem>();
		Item->DefinitionId = FName(*(TEXT("SuperStage:") + Actor->GetActorGuid().ToString()));
		Item->Label = FString::Printf(TEXT("[SuperStage #%d] %s"), Fixture.FixtureId, *Actor->GetActorLabel());
		Item->Universe = Fixture.Universe;
		Item->Address = Fixture.Address;
		Item->Span = Fixture.Span;
		Item->FixtureId = Fixture.FixtureId;
		Item->ActorCount = 1;
		Item->bSuperStage = true;
		Item->Actor = Actor;
		for (const auto& Attribute : Fixture.Attributes)
			Item->ModeSignature += FString::Printf(TEXT("%s:%d:%d:%d:%d;"), *Attribute.Name.ToString(), Attribute.Instance, Attribute.Channel, Attribute.Fine, Attribute.Ultra);
		AllRows.Add(Item);
	}
	AllRows.Sort([](const auto& A, const auto& B)
	{
		const bool bSceneA = A->bNativeActor || A->bSuperStage;
		const bool bSceneB = B->bNativeActor || B->bSuperStage;
		if (bSceneA != bSceneB) return bSceneA;
		if (A->bSuperStage != B->bSuperStage) return A->bSuperStage;
		if (A->bSuperStage && A->FixtureId != B->FixtureId) return A->FixtureId < B->FixtureId;
		return A->Label == B->Label ? A->DefinitionId.LexicalLess(B->DefinitionId) : A->Label < B->Label;
	});
	TSet<FName> ExistingIds;
	bool bChanged = PreviousSignatures.Num() != AllRows.Num();
	bool bVendorChanged = false;
	for (const auto& Row : AllRows)
	{
		ExistingIds.Add(Row->DefinitionId);
		const FString Signature = FString::Printf(TEXT("%s:%d:%d:%d:%d:%s"), *Row->Label, Row->Universe, Row->Address, Row->Span, Row->ActorCount, *Row->ModeSignature);
		const bool bRowChanged = PreviousSignatures.FindRef(Row->DefinitionId) != Signature;
		bChanged |= bRowChanged;
		bVendorChanged |= Row->bSuperStage && bRowChanged;
		PreviousVendorIds.Remove(Row->DefinitionId);
	}
	SelectedDefinitionIds = SelectedDefinitionIds.Intersect(ExistingIds);
	if (bVendorChanged || !PreviousVendorIds.IsEmpty()) TSAVSuperStageDMX::SyncConsole(false);
	if (bChanged || bReloadCatalog)
	{
		ApplyFilter();
		RebuildAttributeFaders();
		if (!bPatchDraftDirty) UsePrimaryPatchClicked();
	}
}

void STSAVDMXLightingConsoleTool::Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime)
{
	SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
	RefreshElapsed += DeltaTime;
	if (RefreshElapsed >= 0.5f) { RefreshElapsed = 0.0f; RefreshRows(false); }
}

void STSAVDMXLightingConsoleTool::ApplyFilter()
{
	FilteredRows.Reset();
	const FString Query = SearchText.TrimStartAndEnd().ToLower();
	for (const TSharedPtr<FTSAVDMXConsoleListItem>& Item : AllRows)
	{
		const FString Searchable = FString::Printf(TEXT("%s u%d.%03d"), *Item->Label, Item->Universe, Item->Address).ToLower();
		if ((!bSceneOnly || Item->bNativeActor || Item->bSuperStage) && (Query.IsEmpty() || Searchable.Contains(Query)))
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
			SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("U%d.%03d | %d ch | %d placed"), Item->Universe, Item->Address, Item->Span, Item->ActorCount))).ColorAndOpacity(FSlateColor::UseSubduedForeground())
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
		PrimaryId = DefinitionId;
		bPatchDraftDirty = false;
	}
	else
	{
		SelectedDefinitionIds.Remove(DefinitionId);
	}
	RebuildAttributeFaders();
	if (!bPatchDraftDirty) UsePrimaryPatchClicked();
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
	if (SelectedDefinitionIds.IsEmpty())
	{
		return;
	}
	TSAVDMXEditorUtils::FControlValues EffectiveValues = Values;
	EffectiveValues.Dimmer = bBlackout ? 0.0f : Values.Dimmer * GrandMaster;
	int32 SentCount = 0;
	for (const auto& Row : AllRows)
	{
		if (!SelectedDefinitionIds.Contains(Row->DefinitionId)) continue;
		if (Row->bNativeActor && SendToNativeShow(Row->Actor.Get(), {{TEXT("Pan"),EffectiveValues.Pan},{TEXT("Tilt"),EffectiveValues.Tilt},
			{TEXT("Dimmer"),EffectiveValues.Dimmer},{TEXT("Red"),EffectiveValues.Red},{TEXT("Green"),EffectiveValues.Green},{TEXT("Blue"),EffectiveValues.Blue},{TEXT("Zoom"),EffectiveValues.Zoom}})) { ++SentCount; continue; }
		if (Row->bSuperStage)
		{
			TSAVSuperStageDMX::FFixture Fixture;
			TArray<TSAVSuperStageDMX::FValue> AttributeBatch;
			if (TSAVSuperStageDMX::ReadFixture(Row->Actor.Get(), Fixture))
			{
				for (const auto& Attribute : Fixture.Attributes)
				{
					const FString Name = TSAVDMXEditorUtils::CanonicalizeAttribute(Attribute.Name);
					TOptional<float> Value;
					if (Name == TEXT("pan")) Value = EffectiveValues.Pan;
					else if (Name == TEXT("tilt")) Value = EffectiveValues.Tilt;
					else if (Name == TEXT("dimmer") || Name == TEXT("intensity")) Value = EffectiveValues.Dimmer;
					else if (Name == TEXT("red") || Name == TEXT("coloraddr") || Name == TEXT("colorrgbred")) Value = EffectiveValues.Red;
					else if (Name == TEXT("green") || Name == TEXT("coloraddg") || Name == TEXT("colorrgbgreen")) Value = EffectiveValues.Green;
					else if (Name == TEXT("blue") || Name == TEXT("coloraddb") || Name == TEXT("colorrgbblue")) Value = EffectiveValues.Blue;
					else if (Name == TEXT("zoom") || Name == TEXT("beamangle")) Value = EffectiveValues.Zoom;
					if (Value.IsSet()) AttributeBatch.Add({Attribute.Name, Value.GetValue(), Attribute.Instance});
				}
			}
			SentCount += TSAVSuperStageDMX::SendAttributes(Row->Actor.Get(), AttributeBatch) ? 1 : 0;
		}
		else
		{
			FTSAVDMXFixtureDefinition Definition;
			if (GetRowDefinition(*Row, Definition)) SentCount += TSAVDMXEditorUtils::SendControlValues(Definition, EffectiveValues, false) ? 1 : 0;
		}
	}
	SetStatus(FString::Printf(TEXT("Submitted controls for %d/%d selected patches%s.%s"), SentCount, SelectedDefinitionIds.Num(), bBlackout ? TEXT(" (blackout active)") : TEXT(""),
		SentCount < SelectedDefinitionIds.Num() ? TEXT(" Check fixture modes and patch configuration.") : TEXT("")), SentCount == SelectedDefinitionIds.Num());
}

void STSAVDMXLightingConsoleTool::SendAttributeValue(const FName AttributeName, const float Value)
{
	int32 SentCount = 0;
	const FString Canonical = TSAVDMXEditorUtils::CanonicalizeAttribute(AttributeName);
	const float Effective = (Canonical == TEXT("dimmer") || Canonical == TEXT("intensity")) ? (bBlackout ? 0.0f : Value * GrandMaster) : Value;
	for (const auto& Row : AllRows)
	{
		if (!SelectedDefinitionIds.Contains(Row->DefinitionId)) continue;
		if (Row->bSuperStage) SentCount += TSAVSuperStageDMX::SendAttribute(Row->Actor.Get(), AttributeName, Effective) ? 1 : 0;
		else if (Row->bNativeActor && SendToNativeShow(Row->Actor.Get(), {{AttributeName, Effective}})) ++SentCount;
		else
		{
			FTSAVDMXFixtureDefinition Definition;
			if (GetRowDefinition(*Row, Definition)) SentCount += TSAVDMXEditorUtils::SendAttributeValue(Definition, AttributeName, Effective) ? 1 : 0;
		}
	}
	SetStatus(FString::Printf(TEXT("%s = %.3f sent to %d compatible selected patch%s."), *AttributeName.ToString(), Value, SentCount, SentCount == 1 ? TEXT("") : TEXT("es")), SentCount > 0);
}

TSharedPtr<FTSAVDMXConsoleListItem> STSAVDMXLightingConsoleTool::GetPrimaryRow() const
{
	for (const auto& Row : AllRows)
	{
		if (Row->DefinitionId == PrimaryId && SelectedDefinitionIds.Contains(PrimaryId)) return Row;
	}
	for (const auto& Row : AllRows)
	{
		if (SelectedDefinitionIds.Contains(Row->DefinitionId)) return Row;
	}
	return nullptr;
}

void STSAVDMXLightingConsoleTool::AddAttributeFader(const FName AttributeName, const FString& Label, const int32 Instance)
{
	const auto Primary = GetPrimaryRow();
	if (!Primary) return;
	const FName ValueKey(*FString::Printf(TEXT("%s:%s:%d"), *Primary->DefinitionId.ToString(), *AttributeName.ToString(), Instance));
	AttributeValues.FindOrAdd(ValueKey, 0.0f);
	const auto Send = [this, AttributeName, ValueKey, Instance](float Value)
	{
		AttributeValues.FindOrAdd(ValueKey) = Value;
		// Common raw faders address the same attribute on all selected fixtures.
		// Matrix module rows address that module index on compatible SuperStage fixtures.
		if (Instance == INDEX_NONE) { SendAttributeValue(AttributeName, Value); return; }
		const FString Canonical = TSAVDMXEditorUtils::CanonicalizeAttribute(AttributeName);
		const float Effective = (Canonical == TEXT("dimmer") || Canonical == TEXT("intensity")) ? (bBlackout ? 0.0f : Value * GrandMaster) : Value;
		int32 Sent = 0;
		for (const auto& Row : AllRows)
		{
			if (!SelectedDefinitionIds.Contains(Row->DefinitionId)) continue;
			if (Row->bSuperStage) Sent += TSAVSuperStageDMX::SendAttribute(Row->Actor.Get(), AttributeName, Effective, Instance) ? 1 : 0;
			else if (Instance == 0)
			{
				if (Row->bNativeActor && SendToNativeShow(Row->Actor.Get(), {{AttributeName, Effective}})) { ++Sent; continue; }
				FTSAVDMXFixtureDefinition Definition;
				if (GetRowDefinition(*Row, Definition)) Sent += TSAVDMXEditorUtils::SendAttributeValue(Definition, AttributeName, Effective) ? 1 : 0;
			}
		}
		SetStatus(FString::Printf(TEXT("%s module %d: updated %d compatible fixtures."), *AttributeName.ToString(), Instance + 1, Sent), Sent > 0);
	};
	AttributeFaders->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[SNew(SBox).WidthOverride(170.0f)[SNew(STextBlock).Text(FText::FromString(Label)).ToolTipText(FText::FromString(Label))]]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f, 8.0f, 0.0f)
		[SNew(SSlider).PreventThrottling(true).Value_Lambda([this, ValueKey] { return AttributeValues.FindRef(ValueKey); }).OnValueChanged_Lambda(Send)]
		+ SHorizontalBox::Slot().AutoWidth()
		[SNew(SBox).WidthOverride(76.0f)[SNew(SNumericEntryBox<float>).MinValue(0.0f).MaxValue(1.0f)
			.Value_Lambda([this, ValueKey] { return AttributeValues.FindRef(ValueKey); }).OnValueChanged_Lambda(Send)]]
	];
}

void STSAVDMXLightingConsoleTool::RebuildAttributeFaders()
{
	if (!AttributeFaders.IsValid()) return;
	AttributeFaders->ClearChildren();
	const auto Primary = GetPrimaryRow();
	if (!Primary)
	{
		AttributeFaders->AddSlot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("SelectForAttributes", "Select a fixture to expose its active mode attributes."))];
		return;
	}
	AttributeFaders->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
	[SNew(STextBlock).Text(FText::FromString(Primary->Label)).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())];
	if (Primary->bSuperStage)
	{
		TSAVSuperStageDMX::FFixture Fixture;
		if (TSAVSuperStageDMX::ReadFixture(Primary->Actor.Get(), Fixture))
		{
			TSet<FString> Added;
			for (const auto& Attribute : Fixture.Attributes)
			{
				const FString Key = FString::Printf(TEXT("%d:%s"), Attribute.Instance, *Attribute.Name.ToString());
				if (Added.Contains(Key)) continue;
				Added.Add(Key);
				AddAttributeFader(Attribute.Name, FString::Printf(TEXT("%s [M%d ch %d]"), *Attribute.Name.ToString(), Attribute.Instance + 1, Attribute.Channel), Attribute.Instance);
			}
		}
		return;
	}
	FTSAVDMXFixtureDefinition Definition;
	UDMXEntityFixturePatch* Patch = GetRowDefinition(*Primary, Definition) ? TSAVDMXEditorUtils::ResolvePatch(Definition) : nullptr;
	const FDMXFixtureMode* Mode = Patch ? Patch->GetActiveMode() : nullptr;
	if (!Mode) return;
	// Deduplicate this rebuild, not against the persistent value cache. Previously
	// selecting another fixture removed every fader whose value had been cached.
	TSet<FName> Added;
	for (const FDMXFixtureFunction& Function : Mode->Functions)
	{
		if (Function.Attribute.Name.IsNone() || Added.Contains(Function.Attribute.Name)) continue;
		Added.Add(Function.Attribute.Name);
		AddAttributeFader(Function.Attribute.Name, FString::Printf(TEXT("%s [ch %d]"), *Function.Attribute.Name.ToString(), Function.Channel));
	}
}

FReply STSAVDMXLightingConsoleTool::UsePrimaryPatchClicked()
{
	if (const auto Primary = GetPrimaryRow())
	{
		EditedUniverse = Primary->Universe;
		EditedAddress = Primary->Address;
	}
	bPatchDraftDirty = false;
	return FReply::Handled();
}

FText STSAVDMXLightingConsoleTool::GetPatchSummary() const
{
	const auto Primary = GetPrimaryRow();
	return Primary ? FText::FromString(FString::Printf(TEXT("Primary: %s | U%d.%03d–%03d | %d selected"),
		*Primary->Label, Primary->Universe, Primary->Address, Primary->Address + Primary->Span - 1, SelectedDefinitionIds.Num()))
		: LOCTEXT("NoPatchSelection", "Select fixtures on the left. The last checked fixture is primary.");
}

FReply STSAVDMXLightingConsoleTool::SyncSuperStageClicked()
{
	const bool bSynced = TSAVSuperStageDMX::SyncConsole(true);
	RefreshRows(false);
	SetStatus(bSynced ? TEXT("SuperStage console imported and synchronized the scene patch. Addresses are read from the same placed fixtures.")
		: TEXT("SuperStage console is unavailable. Launch this project with SuperStage and activate its console."), bSynced);
	return FReply::Handled();
}

FReply STSAVDMXLightingConsoleTool::ApplyPatchClicked()
{
	// Refresh canonical addresses before conflict detection, without overwriting the user's draft.
	const int32 RequestedUniverse = EditedUniverse;
	const int32 RequestedAddress = EditedAddress;
	RefreshRows(false);
	TArray<TSAVDMXPatchPlan::FRange> Selected, Occupied, Plan;
	TMap<FName, TSharedPtr<FTSAVDMXConsoleListItem>> ById;
	const bool bPatchingLibrary = AllRows.ContainsByPredicate([this](const auto& Row)
	{
		return !Row->bSuperStage && !Row->bNativeActor && SelectedDefinitionIds.Contains(Row->DefinitionId);
	});
	TSet<UDMXEntityFixturePatch*> MovingLibraryPatches;
	for (const auto& Row : AllRows)
	{
		if (!Row->bSuperStage && !Row->bNativeActor && SelectedDefinitionIds.Contains(Row->DefinitionId))
		{
			FTSAVDMXFixtureDefinition Definition;
			if (GetRowDefinition(*Row, Definition)) MovingLibraryPatches.Add(TSAVDMXEditorUtils::ResolvePatch(Definition));
		}
	}
	for (const auto& Row : AllRows)
	{
		TSAVDMXPatchPlan::FRange Range{Row->DefinitionId, Row->Label, Row->Universe, Row->Address, Row->Span, Row->bSuperStage ? 512 : 63999};
		ById.Add(Row->DefinitionId, Row);
		if (SelectedDefinitionIds.Contains(Row->DefinitionId)) Selected.Add(Range);
		bool bMovesWithLibrary = false;
		if (const auto* Actor = Row->bNativeActor ? Cast<ATSAVDMXFixture>(Row->Actor.Get()) : nullptr)
			bMovesWithLibrary = MovingLibraryPatches.Contains(Actor->GetFixturePatch());
		if (bMovesWithLibrary && SelectedDefinitionIds.Contains(Row->DefinitionId))
		{
			SetStatus(TEXT("The selection includes a scene fixture and its shared library template. Select one of those rows before patching."), false);
			return FReply::Handled();
		}
		// Templates don't reserve scene addresses; actual placed fixtures do.
		if (!bMovesWithLibrary && (Row->bSuperStage || Row->bNativeActor || bPatchingLibrary)) Occupied.Add(Range);
	}
	FString Error;
	if (!TSAVDMXPatchPlan::Build(Selected, Occupied, RequestedUniverse, RequestedAddress, Plan, Error))
	{
		SetStatus(Error, false);
		return FReply::Handled();
	}
	for (const auto& Range : Plan)
	{
		const auto Row = ById[Range.Id];
		FTSAVDMXFixtureDefinition Definition;
		if (((Row->bSuperStage || Row->bNativeActor) && !Row->Actor.IsValid())
			|| (!Row->bSuperStage && (!GetRowDefinition(*Row, Definition) || !TSAVDMXEditorUtils::ResolvePatch(Definition))))
		{
			SetStatus(TEXT("A selected fixture or patch no longer exists. No patches changed."), false);
			return FReply::Handled();
		}
	}
	bool bVendorChanged = false;
	bool bFailed = false;
	bool bOutputConfigured = true;
	{
		const FScopedTransaction Transaction(LOCTEXT("PatchTransaction", "Patch TSAV and SuperStage fixtures"));
		for (const auto& Range : Plan)
		{
			const auto Row = ById[Range.Id];
			if (Row->bSuperStage)
			{
				if (!TSAVSuperStageDMX::SetPatch(Row->Actor.Get(), Range.Universe, Range.Address))
				{
					bFailed = true;
					break;
				}
				bVendorChanged = true;
			}
			else
			{
				UDMXEntityFixturePatch* Patch = nullptr;
				if (Row->bNativeActor)
				{
					auto* Actor = CastChecked<ATSAVDMXFixture>(Row->Actor.Get());
					if (!Actor->SetIndividualPatchAddress(Range.Universe, Range.Address)) { bFailed = true; break; }
					Patch = Actor->GetFixturePatch();
				}
				else
				{
					auto* Definition = Catalog->Fixtures.FindByPredicate([&Range](const auto& Item) { return Item.DefinitionId == Range.Id; });
					Patch = TSAVDMXEditorUtils::ResolvePatch(*Definition);
					Patch->Modify();
					Catalog->Modify();
					Patch->SetUniverseID(Range.Universe);
					Patch->SetStartingChannel(Range.Address);
					Definition->Universe = Range.Universe;
					Definition->Address = Range.Address;
					Definition->ChannelSpan = Range.Span;
					Patch->MarkPackageDirty();
					Catalog->MarkPackageDirty();
				}
				bool bHasPort = false;
				if (const UDMXLibrary* Library = Patch->GetParentLibrary())
				{
					for (const auto& Port : Library->GetOutputPorts()) bHasPort |= Port->IsLocalUniverseInPortRange(Range.Universe);
				}
				bOutputConfigured &= bHasPort;
			}
		}
	}
	if (bFailed)
	{
		if (GEditor) GEditor->UndoTransaction(false);
		TSAVSuperStageDMX::SyncConsole(false);
		RefreshRows(false);
		SetStatus(TEXT("A fixture rejected the patch update. The patch transaction was rolled back."), false);
		return FReply::Handled();
	}
	const bool bSynced = !bVendorChanged || TSAVSuperStageDMX::SyncConsole(true);
	bPatchDraftDirty = false;
	RefreshRows(false);
	SetStatus(FString::Printf(TEXT("Patched %d fixtures starting at U%d.%03d. Save All to keep changes.%s%s"), Plan.Num(), RequestedUniverse, RequestedAddress,
		bSynced ? TEXT("") : TEXT(" Scene addresses changed; open SuperStage console and click Sync to update its imported patch."),
		bOutputConfigured ? TEXT("") : TEXT(" A TSAV universe is outside the configured output ports. Extend its range in Project Settings > DMX before transmitting.")), bSynced && bOutputConfigured);
	return FReply::Handled();
}

const FTSAVDMXFixtureDefinition* STSAVDMXLightingConsoleTool::FindDefinition(const FName DefinitionId) const
{
	return Catalog ? Catalog->FindFixture(DefinitionId) : nullptr;
}

bool STSAVDMXLightingConsoleTool::GetRowDefinition(const FTSAVDMXConsoleListItem& Row, FTSAVDMXFixtureDefinition& Out) const
{
	if (Row.bSuperStage) return false;
	if (!Row.bNativeActor)
	{
		const auto* Definition = FindDefinition(Row.DefinitionId);
		if (!Definition) return false;
		Out = *Definition;
		return true;
	}
	const auto* Actor = Cast<ATSAVDMXFixture>(Row.Actor.Get());
	UDMXEntityFixturePatch* Patch = Actor ? Actor->GetFixturePatch() : nullptr;
	if (!Patch || !Patch->GetParentLibrary()) return false;
	Out = {};
	Out.DefinitionId = Actor->FixtureDefinitionId;
	Out.DMXLibrary = Patch->GetParentLibrary();
	Out.FixturePatchId = Patch->GetID();
	Out.Universe = Patch->GetUniverseID();
	Out.Address = Patch->GetStartingChannel();
	Out.ChannelSpan = Patch->GetChannelSpan();
	return true;
}

FReply STSAVDMXLightingConsoleTool::PlaceSelectedClicked()
{
	TSet<FName> PlacedIds;
	for (const auto& Row : AllRows)
	{
		if (Row->bNativeActor || Row->bSuperStage || !SelectedDefinitionIds.Contains(Row->DefinitionId)) continue;
		const auto* Definition = FindDefinition(Row->DefinitionId);
		if (ATSAVDMXFixture* Actor = Definition ? TSAVDMXEditorUtils::SpawnFixture(*Definition) : nullptr)
			PlacedIds.Add(FName(*(TEXT("TSAVScene:") + Actor->GetActorGuid().ToString())));
	}
	if (PlacedIds.IsEmpty())
	{
		SetStatus(TEXT("Select library rows to place fixtures in the scene."), false);
		return FReply::Handled();
	}
	SelectedDefinitionIds = MoveTemp(PlacedIds);
	bSceneOnly = true;
	bPatchDraftDirty = false;
	RefreshRows(false);
	ApplyFilter();
	RebuildAttributeFaders();
	UsePrimaryPatchClicked();
	SetStatus(FString::Printf(TEXT("Placed %d fixtures. Enter universe and start address, then patch the selected scene rows."), SelectedDefinitionIds.Num()));
	return FReply::Handled();
}

FReply STSAVDMXLightingConsoleTool::RefreshClicked()
{
	RefreshRows();
	RebuildAttributeFaders();
	SetStatus(FString::Printf(TEXT("Loaded %d patches; %d match the current filter."), AllRows.Num(), FilteredRows.Num()), !AllRows.IsEmpty());
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
	if (!bPatchDraftDirty) UsePrimaryPatchClicked();
	return FReply::Handled();
}

FReply STSAVDMXLightingConsoleTool::SelectPlacedClicked()
{
	SelectedDefinitionIds.Reset();
	for (const TSharedPtr<FTSAVDMXConsoleListItem>& Item : AllRows)
	{
		if (Item->bNativeActor || Item->bSuperStage)
		{
			SelectedDefinitionIds.Add(Item->DefinitionId);
		}
	}
	if (FixtureList.IsValid())
	{
		FixtureList->RequestListRefresh();
	}
	RebuildAttributeFaders();
	if (!bPatchDraftDirty) UsePrimaryPatchClicked();
	SetStatus(FString::Printf(TEXT("Selected %d patches currently placed in the level."), SelectedDefinitionIds.Num()), !SelectedDefinitionIds.IsEmpty());
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
