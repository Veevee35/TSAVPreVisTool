// Copyright TSAV. All Rights Reserved.

#include "UI/TSAVLEDWallConfiguratorWidget.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/TSAVCommandSubsystem.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "Interaction/TSAVSelectionSubsystem.h"
#include "MediaSource.h"
#include "TSAVLEDPanelDefinition.h"
#include "TSAVPrevisRuntime.h"
#include "TSAVVideoSwitcher.h"
#include "UI/TSAVLEDPanelCellButton.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVLEDWallConfiguratorWidget)

namespace TSAVLEDConfigurator::Private
{
	const FLinearColor BackgroundColor(0.018f, 0.024f, 0.035f, 0.995f);
	const FLinearColor PanelColor(0.035f, 0.046f, 0.064f, 1.0f);
	const FLinearColor RaisedColor(0.065f, 0.082f, 0.11f, 1.0f);
	const FLinearColor AccentColor(0.04f, 0.62f, 0.86f, 1.0f);
	const FLinearColor PrimaryTextColor(0.88f, 0.92f, 0.96f, 1.0f);
	const FLinearColor MutedTextColor(0.52f, 0.62f, 0.72f, 1.0f);
	const FLinearColor SuccessColor(0.15f, 0.8f, 0.35f, 1.0f);
	const FLinearColor ErrorColor(0.9f, 0.2f, 0.15f, 1.0f);

	enum EFieldIndex : int32
	{
		WallName,
		PanelWidth,
		PanelHeight,
		PanelDepth,
		PanelResolutionX,
		PanelResolutionY,
		Columns,
		Rows,
		PanelGap,
		Border,
		RoundRadius,
		ColumnSeams,
		RowSeams,
		CurvedColumns,
		CurveAnglesA,
		CurveAnglesB,
		FlatRows,
		CanvasWidth,
		CanvasHeight,
		CanvasX,
		CanvasY,
		EmissiveStrength,
		SubpixelStrength,
		FieldCount,
	};

	FString JoinFloatValues(const TArray<float>& Values)
	{
		TArray<FString> Parts;
		Parts.Reserve(Values.Num());
		for (const float Value : Values)
		{
			Parts.Add(FString::SanitizeFloat(Value, 2));
		}
		return FString::Join(Parts, TEXT(", "));
	}

	FString JoinEnabledIndices(const TArray<bool>& Values)
	{
		TArray<FString> Parts;
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			if (Values[Index])
			{
				Parts.Add(FString::FromInt(Index + 1));
			}
		}
		return FString::Join(Parts, TEXT(", "));
	}

	TArray<float> ParseAngles(const FString& Text, const int32 Count)
	{
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","), false);
		TArray<float> Values;
		Values.SetNumZeroed(FMath::Max(Count, 0));
		for (int32 Index = 0; Index < Values.Num() && Index < Parts.Num(); ++Index)
		{
			Values[Index] = FMath::GridSnap(FMath::Clamp(FCString::Atof(*Parts[Index].TrimStartAndEnd()), -90.0f, 90.0f), 0.5f);
		}
		return Values;
	}

	TArray<bool> ParseEnabledIndices(const FString& Text, const int32 Count)
	{
		TArray<bool> Values;
		Values.Init(false, FMath::Max(Count, 0));
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","), true);
		for (const FString& Part : Parts)
		{
			const int32 Index = FCString::Atoi(*Part.TrimStartAndEnd()) - 1;
			if (Values.IsValidIndex(Index))
			{
				Values[Index] = true;
			}
		}
		return Values;
	}

	UTextBlock* CreateText(UWidgetTree& Tree, const FText& Text, const int32 FontSize, const FLinearColor& Color, const bool bWrap = false)
	{
		UTextBlock* TextBlock = Tree.ConstructWidget<UTextBlock>();
		TextBlock->SetText(Text);
		TextBlock->SetColorAndOpacity(Color);
		TextBlock->SetAutoWrapText(bWrap);
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		return TextBlock;
	}

	UButton* CreateButton(UWidgetTree& Tree, const FText& Label, const FLinearColor& Color = RaisedColor)
	{
		UButton* Button = Tree.ConstructWidget<UButton>();
		Button->SetBackgroundColor(Color);
		Button->SetContent(CreateText(Tree, Label, 11, PrimaryTextColor));
		return Button;
	}

	UEditableTextBox* CreateField(UWidgetTree& Tree, const FText& Hint)
	{
		UEditableTextBox* Field = Tree.ConstructWidget<UEditableTextBox>();
		Field->SetHintText(Hint);
		Field->SetForegroundColor(PrimaryTextColor);
		Field->SetSelectAllTextWhenFocused(true);
		Field->SetClearKeyboardFocusOnCommit(true);
		return Field;
	}

	void AddVertical(UVerticalBox& Parent, UWidget& Child, const FMargin& Padding = FMargin(0.0f), const bool bFill = false)
	{
		if (UVerticalBoxSlot* Slot = Parent.AddChildToVerticalBox(&Child))
		{
			Slot->SetPadding(Padding);
			Slot->SetHorizontalAlignment(HAlign_Fill);
			if (bFill)
			{
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}
	}

	void AddLabeledField(UWidgetTree& Tree, UVerticalBox& Parent, const FText& Label, UEditableTextBox& Field)
	{
		AddVertical(Parent, *CreateText(Tree, Label, 9, MutedTextColor), FMargin(0.0f, 5.0f, 0.0f, 2.0f));
		AddVertical(Parent, Field);
	}

	void AddSection(UWidgetTree& Tree, UVerticalBox& Parent, const FText& Number, const FText& Title, const FText& Help)
	{
		UBorder* Header = Tree.ConstructWidget<UBorder>();
		Header->SetBrushColor(RaisedColor);
		Header->SetPadding(FMargin(8.0f));
		UVerticalBox* Texts = Tree.ConstructWidget<UVerticalBox>();
		UTextBlock* TitleText = CreateText(Tree, FText::Format(NSLOCTEXT("TSAVPreVis", "LEDSectionTitle", "{0}  {1}"), Number, Title), 13, AccentColor);
		UTextBlock* HelpText = CreateText(Tree, Help, 9, MutedTextColor, true);
		AddVertical(*Texts, *TitleText);
		AddVertical(*Texts, *HelpText, FMargin(0.0f, 2.0f, 0.0f, 0.0f));
		Header->SetContent(Texts);
		AddVertical(Parent, *Header, FMargin(0.0f, 7.0f, 0.0f, 5.0f));
	}

	FText GetObjectDisplayName(const AActor* Actor)
	{
		if (const UTSAVSceneObjectComponent* SceneObject = Actor ? Actor->FindComponentByClass<UTSAVSceneObjectComponent>() : nullptr)
		{
			return SceneObject->DisplayName;
		}
		return FText::FromString(GetNameSafe(Actor));
	}

	const TCHAR* GetStyleAbbreviation(const ETSAVLEDPanelEdgeStyle Style)
	{
		switch (Style)
		{
		case ETSAVLEDPanelEdgeStyle::Square: return TEXT("SQ");
		case ETSAVLEDPanelEdgeStyle::DiagonalTopLeft: return TEXT("D-TL");
		case ETSAVLEDPanelEdgeStyle::DiagonalTopRight: return TEXT("D-TR");
		case ETSAVLEDPanelEdgeStyle::DiagonalBottomLeft: return TEXT("D-BL");
		case ETSAVLEDPanelEdgeStyle::DiagonalBottomRight: return TEXT("D-BR");
		case ETSAVLEDPanelEdgeStyle::RoundTopLeft: return TEXT("R-TL");
		case ETSAVLEDPanelEdgeStyle::RoundTopRight: return TEXT("R-TR");
		case ETSAVLEDPanelEdgeStyle::RoundBottomLeft: return TEXT("R-BL");
		case ETSAVLEDPanelEdgeStyle::RoundBottomRight: return TEXT("R-BR");
		case ETSAVLEDPanelEdgeStyle::Disabled: return TEXT("EMPTY");
		default: return TEXT("?");
		}
	}
}

TSharedRef<SWidget> UTSAVLEDWallConfiguratorWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	return Super::RebuildWidget();
}

void UTSAVLEDWallConfiguratorWidget::OpenForWall(ATSAVLEDWall* Wall)
{
	ConfiguredWall = Wall;
	if (WidgetTree && WidgetTree->RootWidget)
	{
		RefreshAssetOptions();
		RefreshRouteOptions();
		LoadFromWall();
	}
}

void UTSAVLEDWallConfiguratorWidget::CloseConfigurator()
{
	RemoveFromParent();
}

void UTSAVLEDWallConfiguratorWidget::BuildLayout()
{
	using namespace TSAVLEDConfigurator::Private;
	Fields.Reset();

	UBorder* Root = WidgetTree->ConstructWidget<UBorder>();
	Root->SetBrushColor(BackgroundColor);
	Root->SetPadding(FMargin(16.0f));
	WidgetTree->RootWidget = Root;

	UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>();
	Root->SetContent(Page);

	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBox* Titles = WidgetTree->ConstructWidget<UVerticalBox>();
	AddVertical(*Titles, *CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "RuntimeLEDBuilderTitle", "LED WALL CONFIGURATOR"), 24, AccentColor));
	AddVertical(*Titles, *CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "RuntimeLEDBuilderSubtitle", "Full runtime cabinet, geometry, processor-canvas, and video-routing authoring"), 10, MutedTextColor));
	if (UHorizontalBoxSlot* LayoutSlot = Header->AddChildToHorizontalBox(Titles))
	{
		LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LayoutSlot->SetVerticalAlignment(VAlign_Center);
	}
	EditingWallText = CreateText(*WidgetTree, FText::GetEmpty(), 11, PrimaryTextColor);
	if (UHorizontalBoxSlot* LayoutSlot = Header->AddChildToHorizontalBox(EditingWallText))
	{
		LayoutSlot->SetPadding(FMargin(10.0f, 0.0f));
		LayoutSlot->SetVerticalAlignment(VAlign_Center);
	}
	UndoButton = CreateButton(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDBuilderUndo", "UNDO"));
	UndoButton->OnClicked.AddDynamic(this, &UTSAVLEDWallConfiguratorWidget::UndoClicked);
	RedoButton = CreateButton(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDBuilderRedo", "REDO"));
	RedoButton->OnClicked.AddDynamic(this, &UTSAVLEDWallConfiguratorWidget::RedoClicked);
	UButton* CloseButton = CreateButton(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDBuilderClose", "CLOSE"), FLinearColor(0.42f, 0.08f, 0.09f, 1.0f));
	CloseButton->OnClicked.AddDynamic(this, &UTSAVLEDWallConfiguratorWidget::CloseClicked);
	for (UButton* Button : { UndoButton.Get(), RedoButton.Get(), CloseButton })
	{
		if (UHorizontalBoxSlot* LayoutSlot = Header->AddChildToHorizontalBox(Button))
		{
			LayoutSlot->SetPadding(FMargin(3.0f));
			LayoutSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	AddVertical(*Page, *Header, FMargin(0.0f, 0.0f, 0.0f, 10.0f));

	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddVertical(*Page, *Body, FMargin(0.0f), true);

	auto MakeColumn = [this, Body](const float Width) -> UVerticalBox*
	{
		using namespace TSAVLEDConfigurator::Private;
		UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
		Border->SetBrushColor(PanelColor);
		Border->SetPadding(FMargin(10.0f));
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		Scroll->AddChild(Content);
		Border->SetContent(Scroll);
		USizeBox* WidthBox = WidgetTree->ConstructWidget<USizeBox>();
		WidthBox->SetWidthOverride(Width);
		WidthBox->SetContent(Border);
		if (UHorizontalBox* Parent = Body)
		{
			if (UHorizontalBoxSlot* Slot = Parent->AddChildToHorizontalBox(WidthBox))
			{
				Slot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
				Slot->SetVerticalAlignment(VAlign_Fill);
			}
		}
		return Content;
	};

	UVerticalBox* Left = MakeColumn(350.0f);

	auto AddField = [this](UVerticalBox& Parent, const FText& Label, const FText& Hint) -> UEditableTextBox*
	{
		UEditableTextBox* Field = TSAVLEDConfigurator::Private::CreateField(*WidgetTree, Hint);
		Fields.Add(Field);
		TSAVLEDConfigurator::Private::AddLabeledField(*WidgetTree, Parent, Label, *Field);
		return Field;
	};

	AddSection(*WidgetTree, *Left, FText::FromString(TEXT("1")), NSLOCTEXT("TSAVPreVis", "LEDCabinetSection", "CABINET"),
		NSLOCTEXT("TSAVPreVis", "LEDCabinetHelp", "Pick a cooked cabinet definition or enter a custom physical size and native pixel resolution."));
	AddField(*Left, NSLOCTEXT("TSAVPreVis", "LEDWallName", "Wall name"), NSLOCTEXT("TSAVPreVis", "LEDWallNameHint", "LED Wall"));
	PanelPresetCombo = WidgetTree->ConstructWidget<UComboBoxString>();
	PanelPresetCombo->OnSelectionChanged.AddDynamic(this, &UTSAVLEDWallConfiguratorWidget::PanelPresetChanged);
	AddVertical(*Left, *CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDPreset", "Cabinet definition"), 9, MutedTextColor), FMargin(0.0f, 5.0f, 0.0f, 2.0f));
	AddVertical(*Left, *PanelPresetCombo);
	AddField(*Left, NSLOCTEXT("TSAVPreVis", "LEDPanelWidth", "Width (cm)"), FText::FromString(TEXT("50")));
	AddField(*Left, NSLOCTEXT("TSAVPreVis", "LEDPanelHeight", "Height (cm)"), FText::FromString(TEXT("50")));
	AddField(*Left, NSLOCTEXT("TSAVPreVis", "LEDPanelDepth", "Depth (cm)"), FText::FromString(TEXT("12")));
	AddField(*Left, NSLOCTEXT("TSAVPreVis", "LEDPanelPixelsX", "Pixels X"), FText::FromString(TEXT("128")));
	AddField(*Left, NSLOCTEXT("TSAVPreVis", "LEDPanelPixelsY", "Pixels Y"), FText::FromString(TEXT("128")));

	AddSection(*WidgetTree, *Left, FText::FromString(TEXT("2")), NSLOCTEXT("TSAVPreVis", "LEDWallSection", "WALL"),
		NSLOCTEXT("TSAVPreVis", "LEDWallHelp", "Set the cabinet grid and physical finish. Refresh Layout preserves existing cabinet shapes in the overlapping region."));
	AddField(*Left, NSLOCTEXT("TSAVPreVis", "LEDColumns", "Columns (1-64)"), FText::FromString(TEXT("8")));
	AddField(*Left, NSLOCTEXT("TSAVPreVis", "LEDRows", "Rows (1-64)"), FText::FromString(TEXT("4")));
	AddField(*Left, NSLOCTEXT("TSAVPreVis", "LEDPanelGap", "Rear cabinet gap (cm)"), FText::FromString(TEXT("0.5")));
	AddField(*Left, NSLOCTEXT("TSAVPreVis", "LEDBorder", "Outer border (cm)"), FText::FromString(TEXT("2")));
	AddField(*Left, NSLOCTEXT("TSAVPreVis", "LEDRoundRadius", "Rounded edge radius (m)"), FText::FromString(TEXT("0.5")));
	LinkPatternCombo = WidgetTree->ConstructWidget<UComboBoxString>();
	LinkPatternCombo->AddOption(TEXT("Rows: Left to Right"));
	LinkPatternCombo->AddOption(TEXT("Rows: Serpentine"));
	LinkPatternCombo->SetSelectedIndex(1);
	AddVertical(*Left, *CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDLinkPattern", "Cabinet signal linking"), 9, MutedTextColor), FMargin(0.0f, 5.0f, 0.0f, 2.0f));
	AddVertical(*Left, *LinkPatternCombo);
	ShowSeamsCheck = WidgetTree->ConstructWidget<UCheckBox>();
	ShowSeamsCheck->SetIsChecked(true);
	ShowSeamsCheck->SetContent(CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDShowSeams", "Show cabinet seams"), 10, PrimaryTextColor));
	AddVertical(*Left, *ShowSeamsCheck, FMargin(0.0f, 8.0f, 0.0f, 2.0f));
	UButton* RefreshLayout = CreateButton(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDRefreshLayout", "REFRESH LAYOUT PREVIEW"), AccentColor);
	RefreshLayout->OnClicked.AddDynamic(this, &UTSAVLEDWallConfiguratorWidget::RefreshPreviewClicked);
	AddVertical(*Left, *RefreshLayout, FMargin(0.0f, 8.0f, 0.0f, 0.0f));

	// Center column: the visual cabinet layout and processor-canvas preview.
	UBorder* CenterBorder = WidgetTree->ConstructWidget<UBorder>();
	CenterBorder->SetBrushColor(PanelColor);
	CenterBorder->SetPadding(FMargin(10.0f));
	if (UHorizontalBoxSlot* LayoutSlot = Body->AddChildToHorizontalBox(CenterBorder))
	{
		LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LayoutSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		LayoutSlot->SetVerticalAlignment(VAlign_Fill);
	}
	UVerticalBox* Center = WidgetTree->ConstructWidget<UVerticalBox>();
	UScrollBox* CenterScroll = WidgetTree->ConstructWidget<UScrollBox>();
	CenterScroll->AddChild(Center);
	CenterBorder->SetContent(CenterScroll);
	AddSection(*WidgetTree, *Center, FText::FromString(TEXT("3")), NSLOCTEXT("TSAVPreVis", "LEDLayoutSection", "CABINET SHAPE GRID"),
		NSLOCTEXT("TSAVPreVis", "LEDLayoutHelp", "Click cabinets to select them, choose a shape, then apply. Empty removes cabinets; rounded and diagonal shapes match the editor builder."));
	UHorizontalBox* ShapeTools = WidgetTree->ConstructWidget<UHorizontalBox>();
	PanelStyleCombo = WidgetTree->ConstructWidget<UComboBoxString>();
	for (const TCHAR* Option : { TEXT("Square"), TEXT("Diagonal: Top Left"), TEXT("Diagonal: Top Right"), TEXT("Diagonal: Bottom Left"), TEXT("Diagonal: Bottom Right"), TEXT("Round: Top Left"), TEXT("Round: Top Right"), TEXT("Round: Bottom Left"), TEXT("Round: Bottom Right"), TEXT("Disabled / Empty") })
	{
		PanelStyleCombo->AddOption(Option);
	}
	PanelStyleCombo->SetSelectedIndex(0);
	PanelStyleCombo->OnSelectionChanged.AddDynamic(this, &UTSAVLEDWallConfiguratorWidget::PanelStyleChanged);
	if (UHorizontalBoxSlot* LayoutSlot = ShapeTools->AddChildToHorizontalBox(PanelStyleCombo))
	{
		LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LayoutSlot->SetPadding(FMargin(0.0f, 0.0f, 5.0f, 0.0f));
	}
	UButton* ApplyStyle = CreateButton(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDApplyStyle", "APPLY SHAPE"), AccentColor);
	ApplyStyle->OnClicked.AddDynamic(this, &UTSAVLEDWallConfiguratorWidget::ApplyStyleClicked);
	ShapeTools->AddChildToHorizontalBox(ApplyStyle);
	AddVertical(*Center, *ShapeTools);
	UHorizontalBox* SelectionTools = WidgetTree->ConstructWidget<UHorizontalBox>();
	UButton* SelectAll = CreateButton(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDSelectAll", "SELECT ALL"));
	SelectAll->OnClicked.AddDynamic(this, &UTSAVLEDWallConfiguratorWidget::SelectAllClicked);
	UButton* ClearSelection = CreateButton(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDClearSelection", "CLEAR"));
	ClearSelection->OnClicked.AddDynamic(this, &UTSAVLEDWallConfiguratorWidget::ClearSelectionClicked);
	UButton* ResetShape = CreateButton(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDResetShape", "RESET SQUARE"));
	ResetShape->OnClicked.AddDynamic(this, &UTSAVLEDWallConfiguratorWidget::ResetStyleClicked);
	for (UButton* Button : { SelectAll, ClearSelection, ResetShape })
	{
		if (UHorizontalBoxSlot* LayoutSlot = SelectionTools->AddChildToHorizontalBox(Button))
		{
			LayoutSlot->SetPadding(FMargin(0.0f, 5.0f, 5.0f, 0.0f));
		}
	}
	SelectionStatusText = CreateText(*WidgetTree, FText::GetEmpty(), 9, MutedTextColor);
	if (UHorizontalBoxSlot* LayoutSlot = SelectionTools->AddChildToHorizontalBox(SelectionStatusText))
	{
		LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LayoutSlot->SetVerticalAlignment(VAlign_Center);
	}
	AddVertical(*Center, *SelectionTools);

	UScaleBox* GridScale = WidgetTree->ConstructWidget<UScaleBox>();
	GridScale->SetStretch(EStretch::ScaleToFit);
	GridScale->SetStretchDirection(EStretchDirection::DownOnly);
	PanelGrid = WidgetTree->ConstructWidget<UUniformGridPanel>();
	PanelGrid->SetMinDesiredSlotWidth(42.0f);
	PanelGrid->SetMinDesiredSlotHeight(34.0f);
	GridScale->SetContent(PanelGrid);
	USizeBox* GridHeight = WidgetTree->ConstructWidget<USizeBox>();
	GridHeight->SetHeightOverride(365.0f);
	GridHeight->SetContent(GridScale);
	AddVertical(*Center, *GridHeight, FMargin(0.0f, 5.0f), true);

	SummaryText = CreateText(*WidgetTree, FText::GetEmpty(), 10, PrimaryTextColor, true);
	AddVertical(*Center, *SummaryText, FMargin(0.0f, 2.0f, 0.0f, 7.0f));
	AddSection(*WidgetTree, *Center, FText::FromString(TEXT("4")), NSLOCTEXT("TSAVPreVis", "LEDCanvasSection", "PROCESSOR CANVAS"),
		NSLOCTEXT("TSAVPreVis", "LEDCanvasHelp", "The screen rectangle must fit entirely inside the processor canvas before it can be applied."));
	CanvasPreview = WidgetTree->ConstructWidget<UCanvasPanel>();
	UBorder* CanvasBackground = WidgetTree->ConstructWidget<UBorder>();
	CanvasBackground->SetBrushColor(FLinearColor(0.012f, 0.016f, 0.023f, 1.0f));
	if (UCanvasPanelSlot* LayoutSlot = CanvasPreview->AddChildToCanvas(CanvasBackground))
	{
		LayoutSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		LayoutSlot->SetOffsets(FMargin(0.0f));
		LayoutSlot->SetZOrder(0);
	}
	ScreenPreview = WidgetTree->ConstructWidget<UBorder>();
	ScreenPreview->SetBrushColor(FLinearColor(0.04f, 0.62f, 0.86f, 0.72f));
	ScreenPreview->SetContent(CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDCanvasScreen", "LED WALL"), 11, PrimaryTextColor));
	if (UCanvasPanelSlot* LayoutSlot = CanvasPreview->AddChildToCanvas(ScreenPreview))
	{
		LayoutSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.25f, 0.25f));
		LayoutSlot->SetOffsets(FMargin(0.0f));
		LayoutSlot->SetZOrder(1);
	}
	USizeBox* CanvasHeightBox = WidgetTree->ConstructWidget<USizeBox>();
	CanvasHeightBox->SetHeightOverride(170.0f);
	CanvasHeightBox->SetContent(CanvasPreview);
	AddVertical(*Center, *CanvasHeightBox);
	CanvasStatusText = CreateText(*WidgetTree, FText::GetEmpty(), 10, SuccessColor, true);
	AddVertical(*Center, *CanvasStatusText, FMargin(0.0f, 5.0f, 0.0f, 0.0f));

	UVerticalBox* Right = MakeColumn(390.0f);
	AddSection(*WidgetTree, *Right, FText::FromString(TEXT("5")), NSLOCTEXT("TSAVPreVis", "LEDCurvesSection", "CURVES & OVERRIDES"),
		NSLOCTEXT("TSAVPreVis", "LEDCurvesHelp", "Comma-separated seam angles are in degrees. Column and row lists use one-based indexes, matching the editor tool."));
	AddField(*Right, NSLOCTEXT("TSAVPreVis", "LEDColumnSeams", "Column seam angles (C-1)"), FText::FromString(TEXT("0, 0, 0")));
	AddField(*Right, NSLOCTEXT("TSAVPreVis", "LEDRowSeams", "Row seam angles (R-1)"), FText::FromString(TEXT("0, 0")));
	AddField(*Right, NSLOCTEXT("TSAVPreVis", "LEDCurvedColumns", "Internally curved columns"), FText::FromString(TEXT("3, 4")));
	AddField(*Right, NSLOCTEXT("TSAVPreVis", "LEDCurveA", "Internal half A angles (C)"), FText::FromString(TEXT("0, 0, 30, 30")));
	AddField(*Right, NSLOCTEXT("TSAVPreVis", "LEDCurveB", "Internal half B angles (C)"), FText::FromString(TEXT("0, 0, 30, 30")));
	AddField(*Right, NSLOCTEXT("TSAVPreVis", "LEDFlatRows", "Flat row overrides"), FText::FromString(TEXT("1")));

	AddSection(*WidgetTree, *Right, FText::FromString(TEXT("6")), NSLOCTEXT("TSAVPreVis", "LEDCanvasInputsSection", "CANVAS & LOOK"),
		NSLOCTEXT("TSAVPreVis", "LEDCanvasInputsHelp", "Set the top-left processor pixel, output canvas, brightness, and physical subpixel simulation."));
	AddField(*Right, NSLOCTEXT("TSAVPreVis", "LEDCanvasWidth", "Canvas width"), FText::FromString(TEXT("4096")));
	AddField(*Right, NSLOCTEXT("TSAVPreVis", "LEDCanvasHeight", "Canvas height"), FText::FromString(TEXT("2160")));
	AddField(*Right, NSLOCTEXT("TSAVPreVis", "LEDCanvasX", "Screen X"), FText::FromString(TEXT("0")));
	AddField(*Right, NSLOCTEXT("TSAVPreVis", "LEDCanvasY", "Screen Y"), FText::FromString(TEXT("0")));
	AddField(*Right, NSLOCTEXT("TSAVPreVis", "LEDEmissive", "Emissive strength"), FText::FromString(TEXT("3")));
	SubpixelCombo = WidgetTree->ConstructWidget<UComboBoxString>();
	SubpixelCombo->AddOption(TEXT("Off (Solid Video)"));
	SubpixelCombo->AddOption(TEXT("Rectangle RGB"));
	SubpixelCombo->AddOption(TEXT("Round RGB"));
	SubpixelCombo->AddOption(TEXT("Round Linear"));
	SubpixelCombo->SetSelectedIndex(0);
	AddVertical(*Right, *CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDSubpixel", "Subpixel layout"), 9, MutedTextColor), FMargin(0.0f, 5.0f, 0.0f, 2.0f));
	AddVertical(*Right, *SubpixelCombo);
	AddField(*Right, NSLOCTEXT("TSAVPreVis", "LEDSubpixelStrength", "Subpixel strength (0-1)"), FText::FromString(TEXT("1")));

	AddSection(*WidgetTree, *Right, FText::FromString(TEXT("7")), NSLOCTEXT("TSAVPreVis", "LEDVideoSection", "VIDEO ROUTING"),
		NSLOCTEXT("TSAVPreVis", "LEDVideoHelp", "Follow a live switcher bus, or assign any cooked Media/NDI Source directly. Routes remain live after CUT and AUTO."));
	SwitcherCombo = WidgetTree->ConstructWidget<UComboBoxString>();
	VideoBusCombo = WidgetTree->ConstructWidget<UComboBoxString>();
	for (const TCHAR* Bus : { TEXT("Program"), TEXT("Preview"), TEXT("Aux 1"), TEXT("Aux 2") })
	{
		VideoBusCombo->AddOption(Bus);
	}
	VideoBusCombo->SetSelectedIndex(0);
	MediaSourceCombo = WidgetTree->ConstructWidget<UComboBoxString>();
	AddVertical(*Right, *CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDSwitcher", "Video switcher"), 9, MutedTextColor), FMargin(0.0f, 5.0f, 0.0f, 2.0f));
	AddVertical(*Right, *SwitcherCombo);
	AddVertical(*Right, *CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDBus", "Switcher bus"), 9, MutedTextColor), FMargin(0.0f, 5.0f, 0.0f, 2.0f));
	AddVertical(*Right, *VideoBusCombo);
	AddVertical(*Right, *CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDDirectSource", "Direct Media / NDI Source"), 9, MutedTextColor), FMargin(0.0f, 5.0f, 0.0f, 2.0f));
	AddVertical(*Right, *MediaSourceCombo);
	AutoPlayCheck = WidgetTree->ConstructWidget<UCheckBox>();
	AutoPlayCheck->SetIsChecked(true);
	AutoPlayCheck->SetContent(CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDAutoPlay", "Auto-play video"), 10, PrimaryTextColor));
	AddVertical(*Right, *AutoPlayCheck, FMargin(0.0f, 7.0f, 0.0f, 0.0f));

	AddSection(*WidgetTree, *Right, FText::FromString(TEXT("8")), NSLOCTEXT("TSAVPreVis", "LEDCreateSection", "CREATE OR UPDATE"),
		NSLOCTEXT("TSAVPreVis", "LEDCreateHelp", "Apply updates the loaded wall. Create New duplicates this draft as a separate wall in front of the camera."));
	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
	UButton* CreateNew = CreateButton(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDCreateNew", "CREATE NEW WALL"));
	CreateNew->OnClicked.AddDynamic(this, &UTSAVLEDWallConfiguratorWidget::CreateNewClicked);
	UButton* Apply = CreateButton(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDApply", "APPLY / UPDATE"), AccentColor);
	Apply->OnClicked.AddDynamic(this, &UTSAVLEDWallConfiguratorWidget::ApplyClicked);
	for (UButton* Button : { CreateNew, Apply })
	{
		if (UHorizontalBoxSlot* LayoutSlot = Actions->AddChildToHorizontalBox(Button))
		{
			LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LayoutSlot->SetPadding(FMargin(2.0f));
		}
	}
	AddVertical(*Right, *Actions);
	StatusText = CreateText(*WidgetTree, FText::GetEmpty(), 10, SuccessColor, true);
	AddVertical(*Right, *StatusText, FMargin(0.0f, 7.0f, 0.0f, 0.0f));

	ensureMsgf(Fields.Num() == FieldCount, TEXT("LED configurator field order changed (%d/%d)."), Fields.Num(), FieldCount);
	RefreshAssetOptions();
	RefreshRouteOptions();
	if (ConfiguredWall)
	{
		LoadFromWall();
	}
	else
	{
		ResizeLayoutData(8, 4);
		RefreshSummaryAndCanvas();
		SetStatus(NSLOCTEXT("TSAVPreVis", "LEDNoWallLoaded", "No wall is loaded. Configure the draft and click Create New Wall."), true);
	}
	UpdateUndoRedoButtons();
	UE_LOG(LogTSAVPrevisRuntime, Display, TEXT("Runtime full-screen LED wall configurator built."));
}

void UTSAVLEDWallConfiguratorWidget::LoadFromWall()
{
	using namespace TSAVLEDConfigurator::Private;
	if (!ConfiguredWall || Fields.Num() != FieldCount)
	{
		return;
	}

	const UTSAVSceneObjectComponent* SceneObject = ConfiguredWall->FindComponentByClass<UTSAVSceneObjectComponent>();
	SetFieldText(WallName, SceneObject ? SceneObject->DisplayName.ToString() : GetNameSafe(ConfiguredWall));
	SetFieldText(PanelWidth, FString::SanitizeFloat(ConfiguredWall->PanelWidthCm));
	SetFieldText(PanelHeight, FString::SanitizeFloat(ConfiguredWall->PanelHeightCm));
	SetFieldText(PanelDepth, FString::SanitizeFloat(ConfiguredWall->WallDepthCm));
	SetFieldText(PanelResolutionX, FString::FromInt(ConfiguredWall->PanelResolutionX));
	SetFieldText(PanelResolutionY, FString::FromInt(ConfiguredWall->PanelResolutionY));
	SetFieldText(Columns, FString::FromInt(ConfiguredWall->Columns));
	SetFieldText(Rows, FString::FromInt(ConfiguredWall->Rows));
	SetFieldText(PanelGap, FString::SanitizeFloat(ConfiguredWall->PanelGapCm));
	SetFieldText(Border, FString::SanitizeFloat(ConfiguredWall->BorderCm));
	SetFieldText(RoundRadius, FString::SanitizeFloat(ConfiguredWall->RoundEdgeRadiusMeters));
	SetFieldText(ColumnSeams, JoinFloatValues(ConfiguredWall->ColumnSeamAnglesDegrees));
	SetFieldText(RowSeams, JoinFloatValues(ConfiguredWall->RowSeamAnglesDegrees));
	SetFieldText(CurvedColumns, JoinEnabledIndices(ConfiguredWall->ColumnInternalCurveEnabled));
	SetFieldText(CurveAnglesA, JoinFloatValues(ConfiguredWall->ColumnInternalCurveAngleADegrees));
	SetFieldText(CurveAnglesB, JoinFloatValues(ConfiguredWall->ColumnInternalCurveAngleBDegrees));
	SetFieldText(FlatRows, JoinEnabledIndices(ConfiguredWall->RowIgnoreInternalColumnCurves));
	SetFieldText(CanvasWidth, FString::FromInt(ConfiguredWall->CanvasResolution.X));
	SetFieldText(CanvasHeight, FString::FromInt(ConfiguredWall->CanvasResolution.Y));
	SetFieldText(CanvasX, FString::FromInt(ConfiguredWall->CanvasPosition.X));
	SetFieldText(CanvasY, FString::FromInt(ConfiguredWall->CanvasPosition.Y));
	SetFieldText(EmissiveStrength, FString::SanitizeFloat(ConfiguredWall->EmissiveStrength));
	SetFieldText(SubpixelStrength, FString::SanitizeFloat(ConfiguredWall->SubpixelStrength));

	ResizeLayoutData(ConfiguredWall->Columns, ConfiguredWall->Rows);
	PanelEdgeStyles = ConfiguredWall->PanelEdgeStyles;
	PanelEdgeStyles.SetNum(LayoutColumns * LayoutRows);
	PanelSelection.Init(false, LayoutColumns * LayoutRows);
	if (LinkPatternCombo)
	{
		LinkPatternCombo->SetSelectedIndex(ConfiguredWall->LinkPattern == ETSAVLEDLinkPattern::RowsLeftToRight ? 0 : 1);
	}
	if (SubpixelCombo)
	{
		SubpixelCombo->SetSelectedIndex(static_cast<int32>(ConfiguredWall->SubpixelLayout));
	}
	if (ShowSeamsCheck)
	{
		ShowSeamsCheck->SetIsChecked(ConfiguredWall->bShowPanelSeams);
	}
	if (AutoPlayCheck)
	{
		AutoPlayCheck->SetIsChecked(ConfiguredWall->bAutoPlay);
	}

	if (PanelPresetCombo)
	{
		int32 PresetIndex = 0;
		if (ConfiguredWall->bUsePanelDefinition && ConfiguredWall->PanelDefinition)
		{
			const int32 Found = AvailablePanelDefinitions.IndexOfByKey(ConfiguredWall->PanelDefinition);
			PresetIndex = Found == INDEX_NONE ? 0 : Found + 1;
		}
		PanelPresetCombo->SetSelectedIndex(PresetIndex);
	}

	RefreshRouteOptions();
	if (SwitcherCombo)
	{
		int32 SwitcherIndex = 0;
		if (ConfiguredWall->bUseVideoSwitcher && ConfiguredWall->GetVideoSwitcher())
		{
			const int32 Found = AvailableSwitchers.IndexOfByKey(ConfiguredWall->GetVideoSwitcher());
			SwitcherIndex = Found == INDEX_NONE ? 0 : Found + 1;
		}
		SwitcherCombo->SetSelectedIndex(SwitcherIndex);
	}
	if (VideoBusCombo)
	{
		const int32 BusIndex = VideoBusCombo->FindOptionIndex(ConfiguredWall->VideoBusName.ToString());
		VideoBusCombo->SetSelectedIndex(BusIndex == INDEX_NONE ? 0 : BusIndex);
	}
	if (MediaSourceCombo)
	{
		const int32 Found = AvailableMediaSources.IndexOfByKey(ConfiguredWall->MediaSource.Get());
		MediaSourceCombo->SetSelectedIndex(Found == INDEX_NONE ? 0 : Found + 1);
	}

	EditingWallText->SetText(FText::Format(NSLOCTEXT("TSAVPreVis", "LEDEditingWall", "EDITING  {0}"), GetObjectDisplayName(ConfiguredWall)));
	SetStatus(FText::Format(NSLOCTEXT("TSAVPreVis", "LEDLoadedWall", "Loaded {0}. Changes are staged until Apply / Update."), GetObjectDisplayName(ConfiguredWall)), true);
	RebuildPanelGrid();
	RefreshSummaryAndCanvas();
	UpdateUndoRedoButtons();
}

void UTSAVLEDWallConfiguratorWidget::ApplyFieldsToWall(ATSAVLEDWall& Wall)
{
	using namespace TSAVLEDConfigurator::Private;
	int32 DraftColumns = 1;
	int32 DraftRows = 1;
	ReadAndValidateDimensions(DraftColumns, DraftRows);
	ResizeLayoutData(DraftColumns, DraftRows);

	const int32 PresetIndex = PanelPresetCombo ? PanelPresetCombo->GetSelectedIndex() : 0;
	UTSAVLEDPanelDefinition* Definition = PresetIndex > 0 && AvailablePanelDefinitions.IsValidIndex(PresetIndex - 1)
		? AvailablePanelDefinitions[PresetIndex - 1] : nullptr;
	Wall.PanelDefinition = Definition;
	Wall.bUsePanelDefinition = Definition != nullptr;
	Wall.PanelWidthCm = FMath::Max(GetFloatField(PanelWidth, Wall.PanelWidthCm), 1.0f);
	Wall.PanelHeightCm = FMath::Max(GetFloatField(PanelHeight, Wall.PanelHeightCm), 1.0f);
	Wall.WallDepthCm = FMath::Max(GetFloatField(PanelDepth, Wall.WallDepthCm), 0.1f);
	Wall.PanelResolutionX = FMath::Max(GetIntField(PanelResolutionX, Wall.PanelResolutionX), 1);
	Wall.PanelResolutionY = FMath::Max(GetIntField(PanelResolutionY, Wall.PanelResolutionY), 1);
	Wall.Columns = DraftColumns;
	Wall.Rows = DraftRows;
	Wall.PanelGapCm = FMath::Max(GetFloatField(PanelGap, Wall.PanelGapCm), 0.0f);
	Wall.BorderCm = FMath::Max(GetFloatField(Border, Wall.BorderCm), 0.0f);
	Wall.RoundEdgeRadiusMeters = FMath::Max(static_cast<double>(GetFloatField(RoundRadius, static_cast<float>(Wall.RoundEdgeRadiusMeters))), 0.5);
	Wall.ColumnSeamAnglesDegrees = ParseAngles(GetFieldText(ColumnSeams), FMath::Max(Wall.Columns - 1, 0));
	Wall.RowSeamAnglesDegrees = ParseAngles(GetFieldText(RowSeams), FMath::Max(Wall.Rows - 1, 0));
	Wall.ColumnInternalCurveEnabled = ParseEnabledIndices(GetFieldText(CurvedColumns), Wall.Columns);
	Wall.ColumnInternalCurveAngleADegrees = ParseAngles(GetFieldText(CurveAnglesA), Wall.Columns);
	Wall.ColumnInternalCurveAngleBDegrees = ParseAngles(GetFieldText(CurveAnglesB), Wall.Columns);
	Wall.RowIgnoreInternalColumnCurves = ParseEnabledIndices(GetFieldText(FlatRows), Wall.Rows);
	Wall.ColumnInternalCurveRadiusAMeters.Reset();
	Wall.ColumnInternalCurveRadiusBMeters.Reset();
	Wall.PanelEdgeStyles = PanelEdgeStyles;
	Wall.PanelEdgeStyles.SetNum(Wall.Columns * Wall.Rows);
	Wall.bShowPanelSeams = ShowSeamsCheck && ShowSeamsCheck->IsChecked();
	Wall.LinkPattern = LinkPatternCombo && LinkPatternCombo->GetSelectedIndex() == 0
		? ETSAVLEDLinkPattern::RowsLeftToRight : ETSAVLEDLinkPattern::RowsSerpentine;
	Wall.CanvasResolution = FIntPoint(FMath::Max(GetIntField(CanvasWidth, 4096), 1), FMath::Max(GetIntField(CanvasHeight, 2160), 1));
	Wall.CanvasPosition = FIntPoint(FMath::Max(GetIntField(CanvasX, 0), 0), FMath::Max(GetIntField(CanvasY, 0), 0));
	Wall.bUseCanvasMapping = true;
	Wall.EmissiveStrength = FMath::Max(GetFloatField(EmissiveStrength, Wall.EmissiveStrength), 0.0f);
	Wall.SubpixelLayout = static_cast<ETSAVLEDSubpixelLayout>(FMath::Clamp(SubpixelCombo ? SubpixelCombo->GetSelectedIndex() : 0, 0, 3));
	Wall.SubpixelStrength = FMath::Clamp(GetFloatField(SubpixelStrength, Wall.SubpixelStrength), 0.0f, 1.0f);
	Wall.bAutoPlay = AutoPlayCheck && AutoPlayCheck->IsChecked();

	const int32 SwitcherIndex = SwitcherCombo ? SwitcherCombo->GetSelectedIndex() : 0;
	if (SwitcherIndex > 0 && AvailableSwitchers.IsValidIndex(SwitcherIndex - 1))
	{
		const FName BusName(*(VideoBusCombo ? VideoBusCombo->GetSelectedOption() : FString(TEXT("Program"))));
		Wall.SetVideoRoute(AvailableSwitchers[SwitcherIndex - 1], BusName);
	}
	else
	{
		Wall.ClearVideoRoute();
		const int32 SourceIndex = MediaSourceCombo ? MediaSourceCombo->GetSelectedIndex() : 0;
		Wall.MediaSource = SourceIndex > 0 && AvailableMediaSources.IsValidIndex(SourceIndex - 1)
			? AvailableMediaSources[SourceIndex - 1] : nullptr;
	}

	Wall.RebuildPanelLayout();
	Wall.RefreshMedia();
}

void UTSAVLEDWallConfiguratorWidget::ResizeLayoutData(int32 NewColumns, int32 NewRows)
{
	NewColumns = FMath::Clamp(NewColumns, 1, 64);
	NewRows = FMath::Clamp(NewRows, 1, 64);
	TArray<ETSAVLEDPanelEdgeStyle> NewStyles;
	NewStyles.Init(ETSAVLEDPanelEdgeStyle::Square, NewColumns * NewRows);
	TArray<bool> NewSelection;
	NewSelection.Init(false, NewColumns * NewRows);
	for (int32 Row = 0; Row < FMath::Min(LayoutRows, NewRows); ++Row)
	{
		for (int32 Column = 0; Column < FMath::Min(LayoutColumns, NewColumns); ++Column)
		{
			const int32 OldIndex = Row * LayoutColumns + Column;
			const int32 NewIndex = Row * NewColumns + Column;
			if (PanelEdgeStyles.IsValidIndex(OldIndex))
			{
				NewStyles[NewIndex] = PanelEdgeStyles[OldIndex];
			}
			if (PanelSelection.IsValidIndex(OldIndex))
			{
				NewSelection[NewIndex] = PanelSelection[OldIndex];
			}
		}
	}
	LayoutColumns = NewColumns;
	LayoutRows = NewRows;
	PanelEdgeStyles = MoveTemp(NewStyles);
	PanelSelection = MoveTemp(NewSelection);
	RebuildPanelGrid();
}

void UTSAVLEDWallConfiguratorWidget::RebuildPanelGrid()
{
	using namespace TSAVLEDConfigurator::Private;
	if (!PanelGrid || !WidgetTree)
	{
		return;
	}
	PanelGrid->ClearChildren();
	PanelCellCount = 0;
	const bool bSerpentine = LinkPatternCombo && LinkPatternCombo->GetSelectedIndex() != 0;
	for (int32 Row = 0; Row < LayoutRows; ++Row)
	{
		for (int32 Column = 0; Column < LayoutColumns; ++Column)
		{
			const int32 Index = Row * LayoutColumns + Column;
			const ETSAVLEDPanelEdgeStyle Style = PanelEdgeStyles.IsValidIndex(Index) ? PanelEdgeStyles[Index] : ETSAVLEDPanelEdgeStyle::Square;
			const bool bSelected = PanelSelection.IsValidIndex(Index) && PanelSelection[Index];
			const int32 LinkedColumn = bSerpentine && (Row % 2 == 1) ? LayoutColumns - Column - 1 : Column;
			const int32 LinkIndex = Row * LayoutColumns + LinkedColumn + 1;
			UTSAVLEDPanelCellButton* Cell = WidgetTree->ConstructWidget<UTSAVLEDPanelCellButton>();
			Cell->InitializeCell(Column, Row);
			Cell->OnCellClicked.AddUniqueDynamic(this, &UTSAVLEDWallConfiguratorWidget::HandlePanelCellClicked);
			FLinearColor CellColor = Style == ETSAVLEDPanelEdgeStyle::Disabled
				? FLinearColor(0.035f, 0.035f, 0.04f, 0.9f)
				: (Style == ETSAVLEDPanelEdgeStyle::Square ? RaisedColor : FLinearColor(0.42f, 0.24f, 0.06f, 1.0f));
			if (bSelected)
			{
				CellColor = AccentColor;
			}
			Cell->SetBackgroundColor(CellColor);
			UTextBlock* Label = CreateText(*WidgetTree,
				FText::FromString(FString::Printf(TEXT("#%d\n%s"), LinkIndex, GetStyleAbbreviation(Style))), 8,
				Style == ETSAVLEDPanelEdgeStyle::Disabled ? MutedTextColor : PrimaryTextColor);
			Cell->SetContent(Label);
			if (UUniformGridSlot* LayoutSlot = PanelGrid->AddChildToUniformGrid(Cell, Row, Column))
			{
				LayoutSlot->SetHorizontalAlignment(HAlign_Fill);
				LayoutSlot->SetVerticalAlignment(VAlign_Fill);
			}
			++PanelCellCount;
		}
	}
	int32 SelectedCount = 0;
	for (const bool bSelected : PanelSelection)
	{
		SelectedCount += bSelected ? 1 : 0;
	}
	if (SelectionStatusText)
	{
		SelectionStatusText->SetText(FText::Format(NSLOCTEXT("TSAVPreVis", "LEDSelectionCount", "{0} selected / {1} cabinets"),
			FText::AsNumber(SelectedCount), FText::AsNumber(PanelCellCount)));
	}
}

void UTSAVLEDWallConfiguratorWidget::RefreshSummaryAndCanvas()
{
	using namespace TSAVLEDConfigurator::Private;
	if (SummaryText)
	{
		SummaryText->SetText(GetWallSummary());
	}
	const bool bFits = DoesScreenFitCanvas();
	if (CanvasStatusText)
	{
		CanvasStatusText->SetText(GetCanvasStatus());
		CanvasStatusText->SetColorAndOpacity(bFits ? SuccessColor : ErrorColor);
	}
	if (ScreenPreview)
	{
		ScreenPreview->SetBrushColor(bFits ? FLinearColor(0.04f, 0.62f, 0.86f, 0.72f) : FLinearColor(0.9f, 0.12f, 0.08f, 0.78f));
		if (UCanvasPanelSlot* LayoutSlot = Cast<UCanvasPanelSlot>(ScreenPreview->Slot))
		{
			const float CanvasW = static_cast<float>(FMath::Max(GetIntField(CanvasWidth, 1), 1));
			const float CanvasH = static_cast<float>(FMath::Max(GetIntField(CanvasHeight, 1), 1));
			const FIntPoint WallResolution = GetDraftWallResolution();
			const float X0 = FMath::Clamp(static_cast<float>(GetIntField(CanvasX, 0)) / CanvasW, 0.0f, 1.0f);
			const float Y0 = FMath::Clamp(static_cast<float>(GetIntField(CanvasY, 0)) / CanvasH, 0.0f, 1.0f);
			const float X1 = FMath::Clamp(static_cast<float>(GetIntField(CanvasX, 0) + WallResolution.X) / CanvasW, X0, 1.0f);
			const float Y1 = FMath::Clamp(static_cast<float>(GetIntField(CanvasY, 0) + WallResolution.Y) / CanvasH, Y0, 1.0f);
			LayoutSlot->SetAnchors(FAnchors(X0, Y0, X1, Y1));
			LayoutSlot->SetOffsets(FMargin(0.0f));
		}
	}
	UpdateUndoRedoButtons();
}

void UTSAVLEDWallConfiguratorWidget::RefreshAssetOptions()
{
	if (!PanelPresetCombo || !MediaSourceCombo)
	{
		return;
	}
	AvailablePanelDefinitions.Reset();
	AvailableMediaSources.Reset();
	PanelPresetCombo->ClearOptions();
	PanelPresetCombo->AddOption(TEXT("Custom cabinet"));
	MediaSourceCombo->ClearOptions();
	MediaSourceCombo->AddOption(TEXT("None"));

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& Registry = AssetRegistryModule.Get();
	FARFilter PanelFilter;
	PanelFilter.ClassPaths.Add(UTSAVLEDPanelDefinition::StaticClass()->GetClassPathName());
	PanelFilter.bRecursiveClasses = true;
	TArray<FAssetData> PanelAssets;
	Registry.GetAssets(PanelFilter, PanelAssets);
	for (const FAssetData& Asset : PanelAssets)
	{
		if (UTSAVLEDPanelDefinition* Definition = Cast<UTSAVLEDPanelDefinition>(Asset.GetAsset()))
		{
			AvailablePanelDefinitions.Add(Definition);
			PanelPresetCombo->AddOption(Definition->ModelName.IsEmpty() ? Asset.AssetName.ToString() : Definition->ModelName);
		}
	}

	FARFilter MediaFilter;
	MediaFilter.ClassPaths.Add(UMediaSource::StaticClass()->GetClassPathName());
	MediaFilter.bRecursiveClasses = true;
	TArray<FAssetData> MediaAssets;
	Registry.GetAssets(MediaFilter, MediaAssets);
	for (const FAssetData& Asset : MediaAssets)
	{
		if (UMediaSource* Source = Cast<UMediaSource>(Asset.GetAsset()))
		{
			AvailableMediaSources.Add(Source);
			MediaSourceCombo->AddOption(Asset.AssetName.ToString());
		}
	}
	PanelPresetCombo->SetSelectedIndex(0);
	MediaSourceCombo->SetSelectedIndex(0);
}

void UTSAVLEDWallConfiguratorWidget::RefreshRouteOptions()
{
	if (!SwitcherCombo)
	{
		return;
	}
	AvailableSwitchers.Reset();
	SwitcherCombo->ClearOptions();
	SwitcherCombo->AddOption(TEXT("Direct Media / NDI Source"));
	if (GetWorld())
	{
		for (TActorIterator<ATSAVVideoSwitcher> It(GetWorld()); It; ++It)
		{
			AvailableSwitchers.Add(*It);
			SwitcherCombo->AddOption(TSAVLEDConfigurator::Private::GetObjectDisplayName(*It).ToString());
		}
	}
	SwitcherCombo->SetSelectedIndex(0);
}

void UTSAVLEDWallConfiguratorWidget::UpdateUndoRedoButtons()
{
	const UTSAVCommandSubsystem* Commands = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>() : nullptr;
	if (UndoButton)
	{
		UndoButton->SetIsEnabled(Commands && Commands->CanUndo());
	}
	if (RedoButton)
	{
		RedoButton->SetIsEnabled(Commands && Commands->CanRedo());
	}
}

bool UTSAVLEDWallConfiguratorWidget::ReadAndValidateDimensions(int32& OutColumns, int32& OutRows) const
{
	using namespace TSAVLEDConfigurator::Private;
	OutColumns = FMath::Clamp(GetIntField(Columns, LayoutColumns), 1, 64);
	OutRows = FMath::Clamp(GetIntField(Rows, LayoutRows), 1, 64);
	return OutColumns > 0 && OutRows > 0;
}

bool UTSAVLEDWallConfiguratorWidget::DoesScreenFitCanvas() const
{
	using namespace TSAVLEDConfigurator::Private;
	const FIntPoint WallResolution = GetDraftWallResolution();
	const int32 Width = GetIntField(CanvasWidth, 0);
	const int32 Height = GetIntField(CanvasHeight, 0);
	const int32 X = GetIntField(CanvasX, 0);
	const int32 Y = GetIntField(CanvasY, 0);
	return Width > 0 && Height > 0 && X >= 0 && Y >= 0 && X + WallResolution.X <= Width && Y + WallResolution.Y <= Height;
}

FIntPoint UTSAVLEDWallConfiguratorWidget::GetDraftWallResolution() const
{
	using namespace TSAVLEDConfigurator::Private;
	return FIntPoint(
		FMath::Clamp(GetIntField(Columns, LayoutColumns), 1, 64) * FMath::Max(GetIntField(PanelResolutionX, 1), 1),
		FMath::Clamp(GetIntField(Rows, LayoutRows), 1, 64) * FMath::Max(GetIntField(PanelResolutionY, 1), 1));
}

FText UTSAVLEDWallConfiguratorWidget::GetWallSummary() const
{
	using namespace TSAVLEDConfigurator::Private;
	const int32 DraftColumns = FMath::Clamp(GetIntField(Columns, LayoutColumns), 1, 64);
	const int32 DraftRows = FMath::Clamp(GetIntField(Rows, LayoutRows), 1, 64);
	const FIntPoint Resolution = GetDraftWallResolution();
	int32 EmptyCount = 0;
	int32 ShapedCount = 0;
	for (const ETSAVLEDPanelEdgeStyle Style : PanelEdgeStyles)
	{
		EmptyCount += Style == ETSAVLEDPanelEdgeStyle::Disabled ? 1 : 0;
		ShapedCount += Style != ETSAVLEDPanelEdgeStyle::Square && Style != ETSAVLEDPanelEdgeStyle::Disabled ? 1 : 0;
	}
	return FText::Format(NSLOCTEXT("TSAVPreVis", "RuntimeLEDWallSummary", "{0} active / {1} empty cabinets  |  {2} x {3} px  |  {4} x {5} cm  |  {6} shaped"),
		FText::AsNumber(FMath::Max(DraftColumns * DraftRows - EmptyCount, 0)), FText::AsNumber(EmptyCount),
		FText::AsNumber(Resolution.X), FText::AsNumber(Resolution.Y),
		FText::AsNumber(DraftColumns * GetFloatField(PanelWidth, 50.0f)), FText::AsNumber(DraftRows * GetFloatField(PanelHeight, 50.0f)),
		FText::AsNumber(ShapedCount));
}

FText UTSAVLEDWallConfiguratorWidget::GetCanvasStatus() const
{
	using namespace TSAVLEDConfigurator::Private;
	const FIntPoint Resolution = GetDraftWallResolution();
	const int32 Width = GetIntField(CanvasWidth, 0);
	const int32 Height = GetIntField(CanvasHeight, 0);
	const int32 X = GetIntField(CanvasX, 0);
	const int32 Y = GetIntField(CanvasY, 0);
	if (DoesScreenFitCanvas())
	{
		return FText::Format(NSLOCTEXT("TSAVPreVis", "RuntimeLEDCanvasFits", "Screen fits: X {0}-{1}, Y {2}-{3} on the {4} x {5} canvas."),
			FText::AsNumber(X), FText::AsNumber(X + Resolution.X - 1), FText::AsNumber(Y), FText::AsNumber(Y + Resolution.Y - 1),
			FText::AsNumber(Width), FText::AsNumber(Height));
	}
	return FText::Format(NSLOCTEXT("TSAVPreVis", "RuntimeLEDCanvasOverflow", "Screen does not fit: {0} x {1} px at X={2}, Y={3} exceeds the {4} x {5} canvas."),
		FText::AsNumber(Resolution.X), FText::AsNumber(Resolution.Y), FText::AsNumber(X), FText::AsNumber(Y), FText::AsNumber(Width), FText::AsNumber(Height));
}

FTransform UTSAVLEDWallConfiguratorWidget::MakePlacementTransform() const
{
	if (const APlayerController* Controller = GetOwningPlayer())
	{
		if (const APlayerCameraManager* Camera = Controller->PlayerCameraManager)
		{
			const FRotator CameraRotation = Camera->GetCameraRotation();
			return FTransform(FRotator(0.0f, CameraRotation.Yaw + 180.0f, 0.0f), Camera->GetCameraLocation() + CameraRotation.Vector() * 800.0f);
		}
	}
	return FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 200.0f));
}

FString UTSAVLEDWallConfiguratorWidget::GetFieldText(const int32 Index) const
{
	return Fields.IsValidIndex(Index) && Fields[Index] ? Fields[Index]->GetText().ToString() : FString();
}

int32 UTSAVLEDWallConfiguratorWidget::GetIntField(const int32 Index, const int32 Fallback) const
{
	const FString Text = GetFieldText(Index);
	return Text.IsEmpty() ? Fallback : FCString::Atoi(*Text);
}

float UTSAVLEDWallConfiguratorWidget::GetFloatField(const int32 Index, const float Fallback) const
{
	const FString Text = GetFieldText(Index);
	return Text.IsEmpty() ? Fallback : FCString::Atof(*Text);
}

void UTSAVLEDWallConfiguratorWidget::SetFieldText(const int32 Index, const FString& Value)
{
	if (Fields.IsValidIndex(Index) && Fields[Index])
	{
		Fields[Index]->SetText(FText::FromString(Value));
	}
}

void UTSAVLEDWallConfiguratorWidget::SetStatus(const FText& Message, const bool bSuccess)
{
	bStatusSuccess = bSuccess;
	if (StatusText)
	{
		StatusText->SetText(Message);
		StatusText->SetColorAndOpacity(bSuccess ? TSAVLEDConfigurator::Private::SuccessColor : TSAVLEDConfigurator::Private::ErrorColor);
	}
}

void UTSAVLEDWallConfiguratorWidget::CloseClicked()
{
	CloseConfigurator();
}

void UTSAVLEDWallConfiguratorWidget::ApplyClicked()
{
	if (!IsValid(ConfiguredWall))
	{
		SetStatus(NSLOCTEXT("TSAVPreVis", "LEDApplyNoWall", "The loaded wall no longer exists. Click Create New Wall."), false);
		return;
	}
	if (!DoesScreenFitCanvas())
	{
		RefreshSummaryAndCanvas();
		SetStatus(GetCanvasStatus(), false);
		return;
	}
	const FString Before = ConfiguredWall->CaptureTSAVState();
	ApplyFieldsToWall(*ConfiguredWall);
	if (UTSAVCommandSubsystem* Commands = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>() : nullptr)
	{
		Commands->CommitAppliedActorState(ConfiguredWall, Before, NSLOCTEXT("TSAVPreVis", "RuntimeConfigureLEDWallCommand", "Configure LED Wall"));
		const FString DraftName = GetFieldText(TSAVLEDConfigurator::Private::WallName).TrimStartAndEnd();
		if (!DraftName.IsEmpty())
		{
			Commands->SetDisplayName(ConfiguredWall, FText::FromString(DraftName));
		}
	}
	SetStatus(FText::Format(NSLOCTEXT("TSAVPreVis", "LEDApplySuccess", "Updated {0}. Geometry, canvas mapping, and video route are live."),
		TSAVLEDConfigurator::Private::GetObjectDisplayName(ConfiguredWall)), true);
	EditingWallText->SetText(FText::Format(NSLOCTEXT("TSAVPreVis", "LEDEditingWall", "EDITING  {0}"), TSAVLEDConfigurator::Private::GetObjectDisplayName(ConfiguredWall)));
	RebuildPanelGrid();
	RefreshSummaryAndCanvas();
}

void UTSAVLEDWallConfiguratorWidget::CreateNewClicked()
{
	if (!DoesScreenFitCanvas())
	{
		RefreshSummaryAndCanvas();
		SetStatus(GetCanvasStatus(), false);
		return;
	}
	UTSAVCommandSubsystem* Commands = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>() : nullptr;
	if (!Commands || !GetWorld())
	{
		SetStatus(NSLOCTEXT("TSAVPreVis", "LEDCreateNoWorld", "The runtime world is not ready to create an LED wall."), false);
		return;
	}
	const FString DraftName = GetFieldText(TSAVLEDConfigurator::Private::WallName).TrimStartAndEnd();
	ATSAVLEDWall* NewWall = Cast<ATSAVLEDWall>(Commands->SpawnActorClass(GetWorld(), ATSAVLEDWall::StaticClass(), MakePlacementTransform(),
		FText::FromString(DraftName.IsEmpty() ? TEXT("LED Wall") : DraftName), ETSAVObjectType::LED));
	if (!NewWall)
	{
		SetStatus(NSLOCTEXT("TSAVPreVis", "LEDCreateFailed", "Unreal could not create the LED wall."), false);
		return;
	}
	const FString Before = NewWall->CaptureTSAVState();
	ConfiguredWall = NewWall;
	ApplyFieldsToWall(*NewWall);
	Commands->CommitAppliedActorState(NewWall, Before, NSLOCTEXT("TSAVPreVis", "RuntimeCreateLEDConfigurationCommand", "Configure New LED Wall"));
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		LocalPlayer->GetSubsystem<UTSAVSelectionSubsystem>()->SelectActor(NewWall);
	}
	EditingWallText->SetText(FText::Format(NSLOCTEXT("TSAVPreVis", "LEDEditingWall", "EDITING  {0}"), TSAVLEDConfigurator::Private::GetObjectDisplayName(NewWall)));
	SetStatus(FText::Format(NSLOCTEXT("TSAVPreVis", "LEDCreateSuccess", "Created {0}."), TSAVLEDConfigurator::Private::GetObjectDisplayName(NewWall)), true);
	RefreshSummaryAndCanvas();
}

void UTSAVLEDWallConfiguratorWidget::UndoClicked()
{
	if (UTSAVCommandSubsystem* Commands = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>() : nullptr)
	{
		Commands->Undo();
	}
	if (IsValid(ConfiguredWall))
	{
		LoadFromWall();
	}
	else
	{
		ConfiguredWall = nullptr;
		EditingWallText->SetText(NSLOCTEXT("TSAVPreVis", "LEDNoActiveWall", "NO ACTIVE WALL"));
		UpdateUndoRedoButtons();
	}
}

void UTSAVLEDWallConfiguratorWidget::RedoClicked()
{
	if (UTSAVCommandSubsystem* Commands = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>() : nullptr)
	{
		Commands->Redo();
	}
	if (IsValid(ConfiguredWall))
	{
		LoadFromWall();
	}
	else
	{
		UpdateUndoRedoButtons();
	}
}

void UTSAVLEDWallConfiguratorWidget::RefreshPreviewClicked()
{
	int32 NewColumns = LayoutColumns;
	int32 NewRows = LayoutRows;
	ReadAndValidateDimensions(NewColumns, NewRows);
	SetFieldText(TSAVLEDConfigurator::Private::Columns, FString::FromInt(NewColumns));
	SetFieldText(TSAVLEDConfigurator::Private::Rows, FString::FromInt(NewRows));
	ResizeLayoutData(NewColumns, NewRows);
	RefreshSummaryAndCanvas();
	SetStatus(NSLOCTEXT("TSAVPreVis", "LEDLayoutRefreshed", "Layout preview refreshed. Click Apply / Update to rebuild the wall."), true);
}

void UTSAVLEDWallConfiguratorWidget::SelectAllClicked()
{
	PanelSelection.Init(true, LayoutColumns * LayoutRows);
	RebuildPanelGrid();
}

void UTSAVLEDWallConfiguratorWidget::ClearSelectionClicked()
{
	PanelSelection.Init(false, LayoutColumns * LayoutRows);
	RebuildPanelGrid();
}

void UTSAVLEDWallConfiguratorWidget::ApplyStyleClicked()
{
	if (!PanelSelection.Contains(true))
	{
		SetStatus(NSLOCTEXT("TSAVPreVis", "LEDNoCabinetSelection", "Select one or more cabinets in the grid first."), false);
		return;
	}
	for (int32 Index = 0; Index < PanelEdgeStyles.Num(); ++Index)
	{
		if (PanelSelection.IsValidIndex(Index) && PanelSelection[Index])
		{
			PanelEdgeStyles[Index] = SelectedPanelStyle;
		}
	}
	RebuildPanelGrid();
	RefreshSummaryAndCanvas();
	SetStatus(NSLOCTEXT("TSAVPreVis", "LEDShapeStaged", "Cabinet shapes staged. Click Apply / Update to rebuild the wall."), true);
}

void UTSAVLEDWallConfiguratorWidget::ResetStyleClicked()
{
	if (!PanelSelection.Contains(true))
	{
		return;
	}
	for (int32 Index = 0; Index < PanelEdgeStyles.Num(); ++Index)
	{
		if (PanelSelection.IsValidIndex(Index) && PanelSelection[Index])
		{
			PanelEdgeStyles[Index] = ETSAVLEDPanelEdgeStyle::Square;
		}
	}
	RebuildPanelGrid();
	RefreshSummaryAndCanvas();
}

void UTSAVLEDWallConfiguratorWidget::HandlePanelCellClicked(const int32 Column, const int32 Row)
{
	const int32 Index = Row * LayoutColumns + Column;
	if (PanelSelection.IsValidIndex(Index))
	{
		PanelSelection[Index] = !PanelSelection[Index];
		RebuildPanelGrid();
	}
}

void UTSAVLEDWallConfiguratorWidget::PanelPresetChanged(const FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	using namespace TSAVLEDConfigurator::Private;
	if (!PanelPresetCombo || SelectionType == ESelectInfo::Direct)
	{
		return;
	}
	const int32 Index = PanelPresetCombo->GetSelectedIndex() - 1;
	if (!AvailablePanelDefinitions.IsValidIndex(Index))
	{
		return;
	}
	const UTSAVLEDPanelDefinition* Definition = AvailablePanelDefinitions[Index];
	SetFieldText(PanelWidth, FString::SanitizeFloat(Definition->WidthCm));
	SetFieldText(PanelHeight, FString::SanitizeFloat(Definition->HeightCm));
	SetFieldText(PanelDepth, FString::SanitizeFloat(Definition->DepthCm));
	SetFieldText(PanelResolutionX, FString::FromInt(Definition->ResolutionX));
	SetFieldText(PanelResolutionY, FString::FromInt(Definition->ResolutionY));
	RefreshSummaryAndCanvas();
}

void UTSAVLEDWallConfiguratorWidget::PanelStyleChanged(const FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (PanelStyleCombo)
	{
		SelectedPanelStyle = static_cast<ETSAVLEDPanelEdgeStyle>(FMath::Clamp(PanelStyleCombo->GetSelectedIndex(), 0, static_cast<int32>(ETSAVLEDPanelEdgeStyle::Disabled)));
	}
}
