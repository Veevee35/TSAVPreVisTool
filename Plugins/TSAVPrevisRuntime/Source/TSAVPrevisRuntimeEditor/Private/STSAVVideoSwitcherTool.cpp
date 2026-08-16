// Copyright TSAV. All Rights Reserved.

#include "STSAVVideoSwitcherTool.h"

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "IDetailsView.h"
#include "LevelEditorViewport.h"
#include "MediaSource.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "STSAVVideoSwitcherTool"

namespace TSAVVideoSwitcherTool::Private
{
	FString GetSwitcherDisplayName(const ATSAVVideoSwitcher* Switcher)
	{
		return Switcher ? Switcher->GetActorLabel() : TEXT("None");
	}

	FTransform GetSwitcherSpawnTransform()
	{
		if (const FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient)
		{
			const FRotator ViewRotation = ViewportClient->GetViewRotation();
			return FTransform(FRotator(0.0f, ViewRotation.Yaw, 0.0f), ViewportClient->GetViewLocation() + ViewRotation.Vector() * 300.0f);
		}
		return FTransform::Identity;
	}

	FText GetInputDetail(const FTSAVVideoInput& Input)
	{
		switch (Input.Kind)
		{
		case ETSAVVideoInputKind::CameraFeed:
			return LOCTEXT("CameraFeedDetail", "Camera feed");
		case ETSAVVideoInputKind::MediaAsset:
			return Input.MediaSource
				? FText::Format(LOCTEXT("MediaAssetDetail", "Media asset  |  {0}"), FText::FromString(Input.MediaSource->GetName()))
				: LOCTEXT("MissingMediaAssetDetail", "Media asset unavailable");
		case ETSAVVideoInputKind::StreamUrl:
			return FText::FromString(Input.StreamUrl.StartsWith(TEXT("ndi://"), ESearchCase::IgnoreCase)
				? FString::Printf(TEXT("NDI  |  %s"), *Input.StreamUrl.RightChop(6))
				: FString::Printf(TEXT("Stream  |  %s"), *Input.StreamUrl));
		}
		return FText::GetEmpty();
	}
}

void STSAVVideoSwitcherTool::Construct(const FArguments& InArgs)
{
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bLockable = false;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::ObjectsUseNameArea;
	DetailsArgs.ViewIdentifier = TEXT("TSAVVideoSwitcherToolDetails");
	DetailsView = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateDetailView(DetailsArgs);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		.Padding(10.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "TSAV VIDEO SWITCHER"))
				.Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Description", "Route visible cameras, media assets, and NDI senders to switcher buses, then assign every video wall to the bus it should display."))
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SAssignNew(SwitcherCombo, SComboBox<TSharedPtr<FSwitcherOption>>)
					.OptionsSource(&SwitcherOptions)
					.OnGenerateWidget(this, &STSAVVideoSwitcherTool::GenerateSwitcherOption)
					.OnSelectionChanged(this, &STSAVVideoSwitcherTool::SwitcherOptionChanged)
					[
						SNew(STextBlock).Text(this, &STSAVVideoSwitcherTool::GetActiveSwitcherText)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("RefreshSwitchers", "Refresh"))
					.OnClicked(this, &STSAVVideoSwitcherTool::RefreshSwitchers)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CreateSwitcher", "Create Video Switcher"))
					.OnClicked(this, &STSAVVideoSwitcherTool::CreateSwitcher)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("UseSelected", "Use Selected"))
					.OnClicked(this, &STSAVVideoSwitcherTool::UseSelectedSwitcher)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("SelectInLevel", "Select In Level"))
					.IsEnabled_Lambda([this]() { return ActiveSwitcher.IsValid(); })
					.OnClicked(this, &STSAVVideoSwitcherTool::SelectSwitcherInLevel)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("RefreshInputs", "Refresh Visible Inputs"))
					.IsEnabled_Lambda([this]() { return ActiveSwitcher.IsValid(); })
					.OnClicked(this, &STSAVVideoSwitcherTool::RefreshInputs)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Cut", "CUT"))
					.IsEnabled_Lambda([this]() { return ActiveSwitcher.IsValid(); })
					.OnClicked(this, &STSAVVideoSwitcherTool::Cut)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Auto", "AUTO"))
					.IsEnabled_Lambda([this]() { return ActiveSwitcher.IsValid(); })
					.OnClicked(this, &STSAVVideoSwitcherTool::AutoTransition)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(STextBlock).Text(this, &STSAVVideoSwitcherTool::GetBusSummary, FName(TEXT("Program")))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock).Text(this, &STSAVVideoSwitcherTool::GetBusSummary, FName(TEXT("Preview")))
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)
				+ SSplitter::Slot()
				.Value(0.40f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 3.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("InputsHeading", "VISIBLE INPUTS  |  ROUTE SOURCE TO BUS"))
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SAssignNew(InputList, SListView<TSharedPtr<FTSAVSwitcherEditorInputItem>>)
						.ListItemsSource(&InputItems)
						.OnGenerateRow(this, &STSAVVideoSwitcherTool::GenerateInputRow)
						.SelectionMode(ESelectionMode::None)
					]
				]
				+ SSplitter::Slot()
				.Value(0.34f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 3.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("SurfacesHeading", "VIDEO WALL OUTPUTS  |  ROUTE BUS TO WALL"))
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SAssignNew(SurfaceList, SListView<TSharedPtr<FTSAVSwitcherEditorSurfaceItem>>)
						.ListItemsSource(&SurfaceItems)
						.OnGenerateRow(this, &STSAVVideoSwitcherTool::GenerateSurfaceRow)
						.SelectionMode(ESelectionMode::None)
					]
				]
				+ SSplitter::Slot()
				.Value(0.26f)
				[
					DetailsView.ToSharedRef()
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 8.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("ManualInputHeading", "MANUAL STREAM / NDI SOURCE (OPTIONAL)"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.35f)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SAssignNew(ManualNameField, SEditableTextBox)
					.HintText(LOCTEXT("NameHint", "Input name"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.65f)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SAssignNew(ManualUrlField, SEditableTextBox)
					.HintText(LOCTEXT("UrlHint", "Stream URL or bare NDI source name"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("AddManualInput", "Add"))
					.IsEnabled_Lambda([this]() { return ActiveSwitcher.IsValid(); })
					.OnClicked(this, &STSAVVideoSwitcherTool::AddManualInput)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(this, &STSAVVideoSwitcherTool::GetStatusText)
				.ColorAndOpacity(this, &STSAVVideoSwitcherTool::GetStatusColor)
				.AutoWrapText(true)
			]
		]
	];

	RefreshSwitcherOptions();
	if (ATSAVVideoSwitcher* Selected = FindSelectedSwitcher())
	{
		SetActiveSwitcher(Selected);
	}
	else if (!SwitcherOptions.IsEmpty())
	{
		SetActiveSwitcher(SwitcherOptions[0]->Get());
	}
	else
	{
		RebuildSurfaceList();
		SetStatus(LOCTEXT("NoSwitcherStatus", "No TSAV Video Switcher is present in the current level."), false);
	}
}

void STSAVVideoSwitcherTool::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (InCurrentTime < NextInputSyncTime)
	{
		return;
	}
	NextInputSyncTime = InCurrentTime + 0.5;

	const ATSAVVideoSwitcher* Switcher = ActiveSwitcher.Get();
	if (!Switcher)
	{
		return;
	}
	bool bInputListChanged = Switcher->Inputs.Num() != InputItems.Num();
	for (int32 Index = 0; !bInputListChanged && Index < Switcher->Inputs.Num(); ++Index)
	{
		bInputListChanged = !InputItems.IsValidIndex(Index)
			|| InputItems[Index]->InputId != Switcher->Inputs[Index].InputId
			|| !InputItems[Index]->Label.EqualTo(Switcher->Inputs[Index].Label);
	}
	if (bInputListChanged)
	{
		RebuildInputList();
		RebuildSurfaceList();
	}
}

void STSAVVideoSwitcherTool::RefreshSwitcherOptions()
{
	SwitcherOptions.Reset();
	if (GEditor)
	{
		if (UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			for (TActorIterator<ATSAVVideoSwitcher> It(World); It; ++It)
			{
				SwitcherOptions.Add(MakeShared<FSwitcherOption>(*It));
			}
		}
	}
	SwitcherOptions.Sort([](const TSharedPtr<FSwitcherOption>& Left, const TSharedPtr<FSwitcherOption>& Right)
	{
		return TSAVVideoSwitcherTool::Private::GetSwitcherDisplayName(Left->Get()) < TSAVVideoSwitcherTool::Private::GetSwitcherDisplayName(Right->Get());
	});
	if (SwitcherCombo)
	{
		SwitcherCombo->RefreshOptions();
	}
}

void STSAVVideoSwitcherTool::SetActiveSwitcher(ATSAVVideoSwitcher* Switcher)
{
	ActiveSwitcher = Switcher;
	if (DetailsView)
	{
		DetailsView->SetObject(Switcher);
	}
	if (SwitcherCombo && Switcher)
	{
		const TSharedPtr<FSwitcherOption>* Match = SwitcherOptions.FindByPredicate([Switcher](const TSharedPtr<FSwitcherOption>& Item)
		{
			return Item->Get() == Switcher;
		});
		if (Match)
		{
			SwitcherCombo->SetSelectedItem(*Match);
		}
	}
	RebuildInputList();
	RebuildSurfaceList();
	if (Switcher)
	{
		SetStatus(FText::Format(LOCTEXT("EditingSwitcher", "Editing {0}. Refresh Inputs to update visible NDI senders and level cameras."), GetActiveSwitcherText()), true);
	}
}

ATSAVVideoSwitcher* STSAVVideoSwitcherTool::FindSelectedSwitcher() const
{
	if (!GEditor || !GEditor->GetSelectedActors())
	{
		return nullptr;
	}
	for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
	{
		if (ATSAVVideoSwitcher* Switcher = Cast<ATSAVVideoSwitcher>(*It))
		{
			return Switcher;
		}
	}
	return nullptr;
}

TSharedRef<SWidget> STSAVVideoSwitcherTool::GenerateSwitcherOption(const TSharedPtr<FSwitcherOption> Item) const
{
	return SNew(STextBlock).Text(FText::FromString(TSAVVideoSwitcherTool::Private::GetSwitcherDisplayName(Item.IsValid() ? Item->Get() : nullptr)));
}

void STSAVVideoSwitcherTool::SwitcherOptionChanged(const TSharedPtr<FSwitcherOption> Item, ESelectInfo::Type SelectionType)
{
	if (Item.IsValid())
	{
		SetActiveSwitcher(Item->Get());
	}
}

FText STSAVVideoSwitcherTool::GetActiveSwitcherText() const
{
	return FText::FromString(TSAVVideoSwitcherTool::Private::GetSwitcherDisplayName(ActiveSwitcher.Get()));
}

void STSAVVideoSwitcherTool::RebuildInputList()
{
	InputItems.Reset();
	if (ATSAVVideoSwitcher* Switcher = ActiveSwitcher.Get())
	{
		for (const FTSAVVideoInput& Input : Switcher->Inputs)
		{
			TSharedPtr<FTSAVSwitcherEditorInputItem> Item = MakeShared<FTSAVSwitcherEditorInputItem>();
			Item->InputId = Input.InputId;
			Item->Label = Input.Label;
			Item->Detail = TSAVVideoSwitcherTool::Private::GetInputDetail(Input);
			InputItems.Add(Item);
		}
	}
	if (InputList)
	{
		InputList->RequestListRefresh();
	}
}

TSharedRef<ITableRow> STSAVVideoSwitcherTool::GenerateInputRow(const TSharedPtr<FTSAVSwitcherEditorInputItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const FGuid InputId = Item->InputId;
	return SNew(STableRow<TSharedPtr<FTSAVSwitcherEditorInputItem>>, OwnerTable)
		.Padding(2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Item->Label)]
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Item->Detail).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth().Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ProgramShort", "PGM"))
				.ToolTipText(LOCTEXT("ProgramTooltip", "Route this input to Program"))
				.ButtonColorAndOpacity(GetRouteButtonColor(InputId, TEXT("Program")))
				.OnClicked_Lambda([this, InputId]() { return RouteInput(InputId, TEXT("Program")); })
			]
			+ SHorizontalBox::Slot()
			.AutoWidth().Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("PreviewShort", "PVW"))
				.ToolTipText(LOCTEXT("PreviewTooltip", "Route this input to Preview"))
				.ButtonColorAndOpacity(GetRouteButtonColor(InputId, TEXT("Preview")))
				.OnClicked_Lambda([this, InputId]() { return RouteInput(InputId, TEXT("Preview")); })
			]
			+ SHorizontalBox::Slot()
			.AutoWidth().Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Aux1Short", "A1"))
				.ToolTipText(LOCTEXT("Aux1Tooltip", "Route this input to Aux 1"))
				.ButtonColorAndOpacity(GetRouteButtonColor(InputId, TEXT("Aux 1")))
				.OnClicked_Lambda([this, InputId]() { return RouteInput(InputId, TEXT("Aux 1")); })
			]
			+ SHorizontalBox::Slot()
			.AutoWidth().Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Aux2Short", "A2"))
				.ToolTipText(LOCTEXT("Aux2Tooltip", "Route this input to Aux 2"))
				.ButtonColorAndOpacity(GetRouteButtonColor(InputId, TEXT("Aux 2")))
				.OnClicked_Lambda([this, InputId]() { return RouteInput(InputId, TEXT("Aux 2")); })
			]
		];
}

FReply STSAVVideoSwitcherTool::RouteInput(const FGuid InputId, const FName BusName)
{
	ATSAVVideoSwitcher* Switcher = ActiveSwitcher.Get();
	if (!Switcher)
	{
		return FReply::Handled();
	}
	const FScopedTransaction Transaction(FText::Format(LOCTEXT("RouteTransaction", "Route TSAV Switcher {0}"), FText::FromName(BusName)));
	Switcher->Modify();
	if (Switcher->SetBusInput(BusName, InputId))
	{
		Switcher->PostEditChange();
		Switcher->MarkPackageDirty();
		RebuildInputList();
		RebuildSurfaceList();
		SetStatus(FText::Format(LOCTEXT("RouteSuccess", "{0} is now {1}."), FText::FromName(BusName), Switcher->GetBusInputLabel(BusName)), true);
	}
	return FReply::Handled();
}

FSlateColor STSAVVideoSwitcherTool::GetRouteButtonColor(const FGuid InputId, const FName BusName) const
{
	const ATSAVVideoSwitcher* Switcher = ActiveSwitcher.Get();
	if (!Switcher || Switcher->GetBusInputId(BusName) != InputId)
	{
		return FLinearColor::White;
	}
	if (BusName.IsEqual(TEXT("Program")))
	{
		return FLinearColor(0.80f, 0.10f, 0.10f);
	}
	if (BusName.IsEqual(TEXT("Preview")))
	{
		return FLinearColor(0.10f, 0.65f, 0.20f);
	}
	return FLinearColor(0.10f, 0.35f, 0.75f);
}

FText STSAVVideoSwitcherTool::GetBusSummary(const FName BusName) const
{
	const ATSAVVideoSwitcher* Switcher = ActiveSwitcher.Get();
	return FText::Format(LOCTEXT("BusSummary", "{0}: {1}"), FText::FromName(BusName), Switcher ? Switcher->GetBusInputLabel(BusName) : LOCTEXT("NoBusSource", "None"));
}

void STSAVVideoSwitcherTool::RebuildSurfaceList()
{
	SurfaceItems.Reset();
	if (GEditor)
	{
		if (UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			for (TActorIterator<ATSAVMediaSurfaceActor> It(World); It; ++It)
			{
				TSharedPtr<FTSAVSwitcherEditorSurfaceItem> Item = MakeShared<FTSAVSwitcherEditorSurfaceItem>();
				Item->Surface = *It;
				Item->Label = FText::FromString(It->GetActorLabel());
				SurfaceItems.Add(Item);
			}
		}
	}
	SurfaceItems.Sort([](const TSharedPtr<FTSAVSwitcherEditorSurfaceItem>& Left, const TSharedPtr<FTSAVSwitcherEditorSurfaceItem>& Right)
	{
		return Left->Label.ToString() < Right->Label.ToString();
	});
	if (SurfaceList)
	{
		SurfaceList->RequestListRefresh();
	}
}

TSharedRef<ITableRow> STSAVVideoSwitcherTool::GenerateSurfaceRow(const TSharedPtr<FTSAVSwitcherEditorSurfaceItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const TWeakObjectPtr<ATSAVMediaSurfaceActor> Surface = Item->Surface;
	return SNew(STableRow<TSharedPtr<FTSAVSwitcherEditorSurfaceItem>>, OwnerTable)
		.Padding(2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Item->Label)]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(this, &STSAVVideoSwitcherTool::GetSurfaceRouteSummary, Surface)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth().Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("WallProgramShort", "PGM"))
				.ToolTipText(LOCTEXT("WallProgramTooltip", "Make this wall follow Program"))
				.IsEnabled_Lambda([this]() { return ActiveSwitcher.IsValid(); })
				.ButtonColorAndOpacity(GetSurfaceRouteButtonColor(Surface.Get(), TEXT("Program")))
				.OnClicked_Lambda([this, Surface]() { return RouteSurface(Surface, TEXT("Program")); })
			]
			+ SHorizontalBox::Slot()
			.AutoWidth().Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("WallPreviewShort", "PVW"))
				.ToolTipText(LOCTEXT("WallPreviewTooltip", "Make this wall follow Preview"))
				.IsEnabled_Lambda([this]() { return ActiveSwitcher.IsValid(); })
				.ButtonColorAndOpacity(GetSurfaceRouteButtonColor(Surface.Get(), TEXT("Preview")))
				.OnClicked_Lambda([this, Surface]() { return RouteSurface(Surface, TEXT("Preview")); })
			]
			+ SHorizontalBox::Slot()
			.AutoWidth().Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("WallAux1Short", "A1"))
				.ToolTipText(LOCTEXT("WallAux1Tooltip", "Make this wall follow Aux 1"))
				.IsEnabled_Lambda([this]() { return ActiveSwitcher.IsValid(); })
				.ButtonColorAndOpacity(GetSurfaceRouteButtonColor(Surface.Get(), TEXT("Aux 1")))
				.OnClicked_Lambda([this, Surface]() { return RouteSurface(Surface, TEXT("Aux 1")); })
			]
			+ SHorizontalBox::Slot()
			.AutoWidth().Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("WallAux2Short", "A2"))
				.ToolTipText(LOCTEXT("WallAux2Tooltip", "Make this wall follow Aux 2"))
				.IsEnabled_Lambda([this]() { return ActiveSwitcher.IsValid(); })
				.ButtonColorAndOpacity(GetSurfaceRouteButtonColor(Surface.Get(), TEXT("Aux 2")))
				.OnClicked_Lambda([this, Surface]() { return RouteSurface(Surface, TEXT("Aux 2")); })
			]
			+ SHorizontalBox::Slot()
			.AutoWidth().Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("WallDirectShort", "DIRECT"))
				.ToolTipText(LOCTEXT("WallDirectTooltip", "Stop following the switcher and use the wall's assigned Media Source"))
				.ButtonColorAndOpacity(Surface.IsValid() && !Surface->bUseVideoSwitcher ? FLinearColor(0.55f, 0.35f, 0.10f) : FLinearColor::White)
				.OnClicked_Lambda([this, Surface]() { return ClearSurfaceRoute(Surface); })
			]
			+ SHorizontalBox::Slot()
			.AutoWidth().Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("SelectWallShort", "SELECT"))
				.ToolTipText(LOCTEXT("SelectWallTooltip", "Select this wall in the level"))
				.OnClicked_Lambda([this, Surface]() { return SelectSurfaceInLevel(Surface); })
			]
		];
}

FReply STSAVVideoSwitcherTool::RouteSurface(const TWeakObjectPtr<ATSAVMediaSurfaceActor> Surface, const FName BusName)
{
	ATSAVMediaSurfaceActor* SurfaceActor = Surface.Get();
	ATSAVVideoSwitcher* Switcher = ActiveSwitcher.Get();
	if (!SurfaceActor || !Switcher)
	{
		return FReply::Handled();
	}
	const FScopedTransaction Transaction(FText::Format(LOCTEXT("RouteSurfaceTransaction", "Route {0} To TSAV Switcher"), FText::FromString(SurfaceActor->GetActorLabel())));
	SurfaceActor->Modify();
	SurfaceActor->SetVideoRoute(Switcher, BusName);
	SurfaceActor->PostEditChange();
	SurfaceActor->MarkPackageDirty();
	RebuildSurfaceList();
	SetStatus(FText::Format(
		LOCTEXT("RouteSurfaceSuccess", "{0} now follows {1}: {2}."),
		FText::FromString(SurfaceActor->GetActorLabel()), FText::FromName(BusName), Switcher->GetBusInputLabel(BusName)), true);
	return FReply::Handled();
}

FReply STSAVVideoSwitcherTool::ClearSurfaceRoute(const TWeakObjectPtr<ATSAVMediaSurfaceActor> Surface)
{
	ATSAVMediaSurfaceActor* SurfaceActor = Surface.Get();
	if (!SurfaceActor)
	{
		return FReply::Handled();
	}
	const FScopedTransaction Transaction(FText::Format(LOCTEXT("ClearSurfaceRouteTransaction", "Use Direct Media On {0}"), FText::FromString(SurfaceActor->GetActorLabel())));
	SurfaceActor->Modify();
	SurfaceActor->ClearVideoRoute();
	SurfaceActor->PostEditChange();
	SurfaceActor->MarkPackageDirty();
	RebuildSurfaceList();
	SetStatus(FText::Format(LOCTEXT("ClearSurfaceRouteSuccess", "{0} now uses its directly assigned Media Source."), FText::FromString(SurfaceActor->GetActorLabel())), true);
	return FReply::Handled();
}

FReply STSAVVideoSwitcherTool::SelectSurfaceInLevel(const TWeakObjectPtr<ATSAVMediaSurfaceActor> Surface)
{
	if (ATSAVMediaSurfaceActor* SurfaceActor = Surface.Get(); GEditor && SurfaceActor)
	{
		GEditor->SelectNone(false, true, false);
		GEditor->SelectActor(SurfaceActor, true, true, true);
	}
	return FReply::Handled();
}

FSlateColor STSAVVideoSwitcherTool::GetSurfaceRouteButtonColor(const ATSAVMediaSurfaceActor* Surface, const FName BusName) const
{
	if (!Surface || !Surface->bUseVideoSwitcher || Surface->GetVideoSwitcher() != ActiveSwitcher.Get() || !Surface->VideoBusName.IsEqual(BusName))
	{
		return FLinearColor::White;
	}
	if (BusName.IsEqual(TEXT("Program")))
	{
		return FLinearColor(0.80f, 0.10f, 0.10f);
	}
	if (BusName.IsEqual(TEXT("Preview")))
	{
		return FLinearColor(0.10f, 0.65f, 0.20f);
	}
	return FLinearColor(0.10f, 0.35f, 0.75f);
}

FText STSAVVideoSwitcherTool::GetSurfaceRouteSummary(const TWeakObjectPtr<ATSAVMediaSurfaceActor> Surface) const
{
	const ATSAVMediaSurfaceActor* SurfaceActor = Surface.Get();
	if (!SurfaceActor)
	{
		return LOCTEXT("MissingSurface", "Wall unavailable");
	}
	if (!SurfaceActor->bUseVideoSwitcher)
	{
		return SurfaceActor->MediaSource
			? FText::Format(LOCTEXT("DirectSurfaceSource", "Direct  |  {0}"), FText::FromString(SurfaceActor->MediaSource->GetName()))
			: LOCTEXT("DirectSurfaceNone", "Direct  |  No Media Source");
	}
	const ATSAVVideoSwitcher* SurfaceSwitcher = SurfaceActor->GetVideoSwitcher();
	if (!SurfaceSwitcher)
	{
		return FText::Format(LOCTEXT("MissingSurfaceSwitcher", "Missing switcher  |  {0}"), FText::FromName(SurfaceActor->VideoBusName));
	}
	if (SurfaceSwitcher == ActiveSwitcher.Get())
	{
		return FText::Format(
			LOCTEXT("ActiveSurfaceRoute", "{0}  |  {1}"),
			FText::FromName(SurfaceActor->VideoBusName), SurfaceSwitcher->GetBusInputLabel(SurfaceActor->VideoBusName));
	}
	return FText::Format(
		LOCTEXT("OtherSurfaceRoute", "{0} / {1}  |  {2}"),
		FText::FromString(SurfaceSwitcher->GetActorLabel()), FText::FromName(SurfaceActor->VideoBusName),
		SurfaceSwitcher->GetBusInputLabel(SurfaceActor->VideoBusName));
}

FSlateColor STSAVVideoSwitcherTool::GetStatusColor() const
{
	return bStatusSuccess ? FLinearColor(0.35f, 0.85f, 0.45f) : FLinearColor(1.0f, 0.55f, 0.20f);
}

FReply STSAVVideoSwitcherTool::CreateSwitcher()
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
	const FScopedTransaction Transaction(LOCTEXT("CreateSwitcherTransaction", "Create TSAV Video Switcher"));
	World->Modify();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transactional;
	ATSAVVideoSwitcher* Switcher = World->SpawnActor<ATSAVVideoSwitcher>(ATSAVVideoSwitcher::StaticClass(), TSAVVideoSwitcherTool::Private::GetSwitcherSpawnTransform(), SpawnParameters);
	if (!Switcher)
	{
		SetStatus(LOCTEXT("CreateFailed", "The video switcher could not be created."), false);
		return FReply::Handled();
	}
	Switcher->SetActorLabel(TEXT("TSAV Video Switcher"));
	Switcher->Modify();
	const int32 Added = Switcher->DiscoverSources();
	Switcher->PostEditChange();
	Switcher->MarkPackageDirty();
	RefreshSwitcherOptions();
	SetActiveSwitcher(Switcher);
	SelectSwitcherInLevel();
	SetStatus(FText::Format(LOCTEXT("SwitcherCreated", "Created the switcher and found {0} available inputs."), FText::AsNumber(Added)), true);
	return FReply::Handled();
}

FReply STSAVVideoSwitcherTool::UseSelectedSwitcher()
{
	if (ATSAVVideoSwitcher* Switcher = FindSelectedSwitcher())
	{
		RefreshSwitcherOptions();
		SetActiveSwitcher(Switcher);
	}
	else
	{
		SetStatus(LOCTEXT("SelectionNotSwitcher", "Select a TSAV Video Switcher actor in the level first."), false);
	}
	return FReply::Handled();
}

FReply STSAVVideoSwitcherTool::RefreshSwitchers()
{
	ATSAVVideoSwitcher* Previous = ActiveSwitcher.Get();
	RefreshSwitcherOptions();
	if (Previous && SwitcherOptions.ContainsByPredicate([Previous](const TSharedPtr<FSwitcherOption>& Item) { return Item->Get() == Previous; }))
	{
		SetActiveSwitcher(Previous);
	}
	else if (!SwitcherOptions.IsEmpty())
	{
		SetActiveSwitcher(SwitcherOptions[0]->Get());
	}
	else
	{
		ActiveSwitcher.Reset();
		DetailsView->SetObject(nullptr);
		RebuildInputList();
		RebuildSurfaceList();
		SetStatus(LOCTEXT("NoSwitchersAfterRefresh", "No TSAV Video Switcher actors were found in the current level."), false);
	}
	return FReply::Handled();
}

FReply STSAVVideoSwitcherTool::RefreshInputs()
{
	ATSAVVideoSwitcher* Switcher = ActiveSwitcher.Get();
	if (!Switcher)
	{
		return FReply::Handled();
	}
	const FScopedTransaction Transaction(LOCTEXT("RefreshInputsTransaction", "Refresh TSAV Video Inputs"));
	Switcher->Modify();
	const int32 Added = Switcher->DiscoverSources();
	Switcher->PostEditChange();
	Switcher->MarkPackageDirty();
	RebuildInputList();
	RebuildSurfaceList();
	SetStatus(FText::Format(LOCTEXT("RefreshInputsSuccess", "Input list refreshed: {0} total, {1} newly discovered."), FText::AsNumber(Switcher->Inputs.Num()), FText::AsNumber(Added)), true);
	return FReply::Handled();
}

FReply STSAVVideoSwitcherTool::Cut()
{
	ATSAVVideoSwitcher* Switcher = ActiveSwitcher.Get();
	if (Switcher)
	{
		const FScopedTransaction Transaction(LOCTEXT("CutTransaction", "TSAV Video Switcher Cut"));
		Switcher->Modify();
		Switcher->Cut();
		Switcher->PostEditChange();
		Switcher->MarkPackageDirty();
		RebuildInputList();
		SetStatus(LOCTEXT("CutSuccess", "Program and Preview were cut."), true);
	}
	return FReply::Handled();
}

FReply STSAVVideoSwitcherTool::AutoTransition()
{
	ATSAVVideoSwitcher* Switcher = ActiveSwitcher.Get();
	if (Switcher)
	{
		const FScopedTransaction Transaction(LOCTEXT("AutoTransaction", "TSAV Video Switcher Auto Transition"));
		Switcher->Modify();
		Switcher->AutoTransition();
		Switcher->PostEditChange();
		Switcher->MarkPackageDirty();
		RebuildInputList();
		SetStatus(LOCTEXT("AutoSuccess", "Auto transition completed."), true);
	}
	return FReply::Handled();
}

FReply STSAVVideoSwitcherTool::AddManualInput()
{
	ATSAVVideoSwitcher* Switcher = ActiveSwitcher.Get();
	const FString Url = ManualUrlField ? ManualUrlField->GetText().ToString().TrimStartAndEnd() : FString();
	if (!Switcher || Url.IsEmpty())
	{
		SetStatus(LOCTEXT("ManualUrlMissing", "Enter a stream URL or bare NDI source name first."), false);
		return FReply::Handled();
	}
	const FScopedTransaction Transaction(LOCTEXT("AddInputTransaction", "Add TSAV Video Input"));
	Switcher->Modify();
	const FGuid InputId = Switcher->AddStreamInput(ManualNameField ? ManualNameField->GetText() : FText::GetEmpty(), Url);
	if (InputId.IsValid())
	{
		Switcher->PostEditChange();
		Switcher->MarkPackageDirty();
		if (ManualNameField) { ManualNameField->SetText(FText::GetEmpty()); }
		if (ManualUrlField) { ManualUrlField->SetText(FText::GetEmpty()); }
		RebuildInputList();
		SetStatus(LOCTEXT("ManualInputAdded", "The manual video input was added."), true);
	}
	return FReply::Handled();
}

FReply STSAVVideoSwitcherTool::SelectSwitcherInLevel()
{
	if (ATSAVVideoSwitcher* Switcher = ActiveSwitcher.Get(); GEditor && Switcher)
	{
		GEditor->SelectNone(false, true, false);
		GEditor->SelectActor(Switcher, true, true, true);
	}
	return FReply::Handled();
}

void STSAVVideoSwitcherTool::SetStatus(const FText& Message, const bool bSuccess)
{
	StatusText = Message;
	bStatusSuccess = bSuccess;
}

#undef LOCTEXT_NAMESPACE
