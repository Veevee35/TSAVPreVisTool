// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TSAVDMXEditorUtils.h"
#include "Widgets/SCompoundWidget.h"

class SSearchBox;
class SVerticalBox;
class UTSAVDMXFixtureCatalog;
template <typename ItemType> class SListView;

struct FTSAVDMXConsoleListItem
{
	FName DefinitionId;
	FString Label;
	int32 Universe = 1;
	int32 Address = 1;
	int32 ActorCount = 0;
};

/** Programmer-style multi-fixture editor console backed by the complete generated DMX library. */
class STSAVDMXLightingConsoleTool final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STSAVDMXLightingConsoleTool) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void RefreshRows();
	void ApplyFilter();
	TSharedRef<ITableRow> GenerateFixtureRow(TSharedPtr<FTSAVDMXConsoleListItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void SearchChanged(const FText& Text);
	void SetFixtureSelected(FName DefinitionId, ECheckBoxState State);
	ECheckBoxState IsFixtureSelected(FName DefinitionId) const;
	TSharedRef<SWidget> MakeCommonControl(const FText& Label, float* Value);
	void SendCommonValues();
	void SendAttributeValue(FName AttributeName, float Value);
	void RebuildAttributeFaders();
	const struct FTSAVDMXFixtureDefinition* FindDefinition(FName DefinitionId) const;
	FReply RefreshClicked();
	FReply SelectVisibleClicked();
	FReply SelectPlacedClicked();
	FReply ClearSelectionClicked();
	FReply HomeClicked();
	FReply FullClicked();
	FReply WhiteClicked();
	FReply RedClicked();
	FReply GreenClicked();
	FReply BlueClicked();
	FReply BlackoutClicked();
	FReply ReleaseBlackoutClicked();
	FText GetSelectionText() const;
	FSlateColor GetStatusColor() const;
	void SetStatus(const FString& Message, bool bSuccess = true);

	UTSAVDMXFixtureCatalog* Catalog = nullptr;
	TArray<TSharedPtr<FTSAVDMXConsoleListItem>> AllRows;
	TArray<TSharedPtr<FTSAVDMXConsoleListItem>> FilteredRows;
	TSharedPtr<SListView<TSharedPtr<FTSAVDMXConsoleListItem>>> FixtureList;
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<SVerticalBox> AttributeFaders;
	TSet<FName> SelectedDefinitionIds;
	TMap<FName, float> AttributeValues;
	FString SearchText;
	TSAVDMXEditorUtils::FControlValues Values;
	float GrandMaster = 1.0f;
	bool bBlackout = false;
	FText StatusText;
	bool bStatusSuccess = true;
};
