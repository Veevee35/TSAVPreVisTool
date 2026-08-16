// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TSAVDMXEditorUtils.h"
#include "Widgets/SCompoundWidget.h"

class SSearchBox;
class UTSAVDMXFixtureCatalog;
template <typename ItemType> class SListView;

struct FTSAVDMXPatchListItem
{
	FName DefinitionId;
	FString Manufacturer;
	FString FixtureName;
	FString ModeName;
	int32 Universe = 1;
	int32 Address = 1;
	int32 Span = 1;
	bool bPatchValid = false;
};

/** Searchable patch, addressing, spawning, and test panel for the complete generated GDTF library. */
class STSAVDMXPatchTool final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STSAVDMXPatchTool) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void RefreshRows();
	void ApplyFilter();
	TSharedRef<ITableRow> GenerateRow(TSharedPtr<FTSAVDMXPatchListItem> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	void SelectionChanged(TSharedPtr<FTSAVDMXPatchListItem> Item, ESelectInfo::Type SelectionType);
	void SearchChanged(const FText& Text);
	const struct FTSAVDMXFixtureDefinition* GetSelectedDefinition() const;
	TSharedRef<SWidget> MakeTestControl(const FText& Label, float* Value);
	void SendSelectedTest(bool bSnap = false);
	FReply RefreshClicked();
	FReply RepackClicked();
	FReply ValidateClicked();
	FReply ApplyAddressClicked();
	FReply SpawnClicked();
	FReply SelectPlacedClicked();
	FReply HomeClicked();
	FReply FullWhiteClicked();
	FReply TestClicked();
	FReply BlackoutClicked();
	FText GetSelectionSummary() const;
	FSlateColor GetStatusColor() const;
	void SetStatus(const FString& Message, bool bSuccess);

	UTSAVDMXFixtureCatalog* Catalog = nullptr;
	TArray<TSharedPtr<FTSAVDMXPatchListItem>> AllRows;
	TArray<TSharedPtr<FTSAVDMXPatchListItem>> FilteredRows;
	TSharedPtr<SListView<TSharedPtr<FTSAVDMXPatchListItem>>> PatchList;
	TSharedPtr<SSearchBox> SearchBox;
	FName SelectedDefinitionId;
	FString SearchText;
	int32 EditedUniverse = 1;
	int32 EditedAddress = 1;
	TSAVDMXEditorUtils::FControlValues TestValues;
	FText StatusText;
	bool bStatusSuccess = true;
};
