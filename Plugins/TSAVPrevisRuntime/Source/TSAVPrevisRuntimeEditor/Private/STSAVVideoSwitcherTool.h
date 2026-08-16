// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TSAVMediaSurfaceActor.h"
#include "TSAVVideoSwitcher.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class SEditableTextBox;
template <typename ItemType> class SListView;
template <typename OptionType> class SComboBox;

struct FTSAVSwitcherEditorInputItem
{
	FGuid InputId;
	FText Label;
	FText Detail;
};

struct FTSAVSwitcherEditorSurfaceItem
{
	TWeakObjectPtr<ATSAVMediaSurfaceActor> Surface;
	FText Label;
};

/** Editor-facing video switcher panel with visible-source discovery and bus routing. */
class STSAVVideoSwitcherTool final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STSAVVideoSwitcherTool) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

private:
	using FSwitcherOption = TWeakObjectPtr<ATSAVVideoSwitcher>;

	void RefreshSwitcherOptions();
	void SetActiveSwitcher(ATSAVVideoSwitcher* Switcher);
	ATSAVVideoSwitcher* FindSelectedSwitcher() const;
	TSharedRef<SWidget> GenerateSwitcherOption(TSharedPtr<FSwitcherOption> Item) const;
	void SwitcherOptionChanged(TSharedPtr<FSwitcherOption> Item, ESelectInfo::Type SelectionType);
	FText GetActiveSwitcherText() const;
	void RebuildInputList();
	TSharedRef<ITableRow> GenerateInputRow(TSharedPtr<FTSAVSwitcherEditorInputItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	FReply RouteInput(FGuid InputId, FName BusName);
	FSlateColor GetRouteButtonColor(FGuid InputId, FName BusName) const;
	FText GetBusSummary(FName BusName) const;
	void RebuildSurfaceList();
	TSharedRef<ITableRow> GenerateSurfaceRow(TSharedPtr<FTSAVSwitcherEditorSurfaceItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	FReply RouteSurface(TWeakObjectPtr<ATSAVMediaSurfaceActor> Surface, FName BusName);
	FReply ClearSurfaceRoute(TWeakObjectPtr<ATSAVMediaSurfaceActor> Surface);
	FReply SelectSurfaceInLevel(TWeakObjectPtr<ATSAVMediaSurfaceActor> Surface);
	FSlateColor GetSurfaceRouteButtonColor(const ATSAVMediaSurfaceActor* Surface, FName BusName) const;
	FText GetSurfaceRouteSummary(TWeakObjectPtr<ATSAVMediaSurfaceActor> Surface) const;
	FText GetStatusText() const { return StatusText; }
	FSlateColor GetStatusColor() const;
	FReply CreateSwitcher();
	FReply UseSelectedSwitcher();
	FReply RefreshSwitchers();
	FReply RefreshInputs();
	FReply Cut();
	FReply AutoTransition();
	FReply AddManualInput();
	FReply SelectSwitcherInLevel();
	void SetStatus(const FText& Message, bool bSuccess);

	TWeakObjectPtr<ATSAVVideoSwitcher> ActiveSwitcher;
	TArray<TSharedPtr<FSwitcherOption>> SwitcherOptions;
	TSharedPtr<SComboBox<TSharedPtr<FSwitcherOption>>> SwitcherCombo;
	TArray<TSharedPtr<FTSAVSwitcherEditorInputItem>> InputItems;
	TSharedPtr<SListView<TSharedPtr<FTSAVSwitcherEditorInputItem>>> InputList;
	TArray<TSharedPtr<FTSAVSwitcherEditorSurfaceItem>> SurfaceItems;
	TSharedPtr<SListView<TSharedPtr<FTSAVSwitcherEditorSurfaceItem>>> SurfaceList;
	TSharedPtr<SEditableTextBox> ManualNameField;
	TSharedPtr<SEditableTextBox> ManualUrlField;
	TSharedPtr<IDetailsView> DetailsView;
	FText StatusText;
	bool bStatusSuccess = true;
	double NextInputSyncTime = 0.0;
};
