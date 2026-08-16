// Copyright TSAV. All Rights Reserved.

#include "UI/TSAVMainWidget.h"

#include "TSAVPrevisRuntime.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/PointLight.h"
#include "Engine/RectLight.h"
#include "Engine/SpotLight.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Interaction/TSAVCommandSubsystem.h"
#include "Interaction/TSAVModeSubsystem.h"
#include "Interaction/TSAVPlayerController.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "Interaction/TSAVSceneObjectActor.h"
#include "Interaction/TSAVSelectionSubsystem.h"
#include "Interaction/TSAVTransformGizmoActor.h"
#include "Project/TSAVProjectSubsystem.h"
#include "TSAVMediaSurfaceActor.h"
#include "TSAVStateSerializable.h"
#include "TSAVDMXFixture.h"
#include "TSAVLEDPanel.h"
#include "TSAVLEDWall.h"
#include "TSAVVideoSwitcher.h"
#include "Video/TSAVCameraActor.h"
#include "UI/TSAVMenuButton.h"
#include "UI/TSAVOutlinerButton.h"
#include "UI/TSAVSwitcherInputButton.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVMainWidget)

namespace TSAVMainWidget::Private
{
	const FLinearColor ChromeColor(0.025f, 0.032f, 0.045f, 0.96f);
	const FLinearColor PanelColor(0.045f, 0.055f, 0.075f, 0.94f);
	const FLinearColor AccentColor(0.04f, 0.62f, 0.86f, 1.0f);
	const FLinearColor PrimaryTextColor(0.88f, 0.92f, 0.96f, 1.0f);
	const FLinearColor MutedTextColor(0.52f, 0.62f, 0.72f, 1.0f);

	enum ELEDWallFieldIndex : int32
	{
		LEDColumns,
		LEDRows,
		LEDPanelWidth,
		LEDPanelHeight,
		LEDPanelDepth,
		LEDPanelResolutionX,
		LEDPanelResolutionY,
		LEDPanelGap,
		LEDBorder,
		LEDRoundRadius,
		LEDCanvasWidth,
		LEDCanvasHeight,
		LEDCanvasX,
		LEDCanvasY,
		LEDColumnSeams,
		LEDRowSeams,
		LEDCurvedColumns,
		LEDCurveAnglesA,
		LEDCurveAnglesB,
		LEDFlatRows,
		LEDEmissiveStrength,
		LEDFieldCount,
	};

	FString JoinFloatValues(const TArray<float>& Values)
	{
		TArray<FString> Parts;
		Parts.Reserve(Values.Num());
		for (const float Value : Values) { Parts.Add(FString::SanitizeFloat(Value, 2)); }
		return FString::Join(Parts, TEXT(", "));
	}

	FString JoinEnabledIndices(const TArray<bool>& Values)
	{
		TArray<FString> Parts;
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			if (Values[Index]) { Parts.Add(FString::FromInt(Index + 1)); }
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
			if (Values.IsValidIndex(Index)) { Values[Index] = true; }
		}
		return Values;
	}

	UTextBlock* CreateText(UWidgetTree& Tree, const FText& Text, const int32 FontSize, const FLinearColor& Color)
	{
		UTextBlock* TextBlock = Tree.ConstructWidget<UTextBlock>();
		TextBlock->SetText(Text);
		TextBlock->SetColorAndOpacity(Color);
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		return TextBlock;
	}

	UButton* CreateModeButton(UWidgetTree& Tree, const FText& Label)
	{
		UButton* Button = Tree.ConstructWidget<UButton>();
		Button->SetBackgroundColor(FLinearColor(0.075f, 0.09f, 0.12f, 1.0f));
		Button->SetContent(CreateText(Tree, Label, 12, PrimaryTextColor));
		return Button;
	}

	UEditableTextBox* CreateEditField(UWidgetTree& Tree, const FText& Hint)
	{
		UEditableTextBox* Field = Tree.ConstructWidget<UEditableTextBox>();
		Field->SetHintText(Hint);
		Field->SetForegroundColor(PrimaryTextColor);
		Field->SetSelectAllTextWhenFocused(true);
		Field->SetClearKeyboardFocusOnCommit(true);
		return Field;
	}

	void AddLabeledField(UWidgetTree& Tree, UVerticalBox& Parent, const FText& Label, UEditableTextBox& Field)
	{
		UTextBlock* LabelText = CreateText(Tree, Label, 10, MutedTextColor);
		if (UVerticalBoxSlot* LabelSlot = Parent.AddChildToVerticalBox(LabelText))
		{
			LabelSlot->SetPadding(FMargin(0.0f, 5.0f, 0.0f, 2.0f));
		}
		Parent.AddChildToVerticalBox(&Field);
	}

	void AddCanvasPanel(
		UCanvasPanel& Canvas,
		UWidget& Widget,
		const FAnchors& Anchors,
		const FMargin& Offsets,
		const FVector2D& Alignment = FVector2D::ZeroVector,
		const int32 ZOrder = 0)
	{
		UCanvasPanelSlot* Slot = Canvas.AddChildToCanvas(&Widget);
		Slot->SetAnchors(Anchors);
		Slot->SetOffsets(Offsets);
		Slot->SetAlignment(Alignment);
		Slot->SetZOrder(ZOrder);
	}

	AActor* GetSelectedActor(const UUserWidget& Widget)
	{
		const ULocalPlayer* LocalPlayer = Widget.GetOwningLocalPlayer();
		const UTSAVSelectionSubsystem* Selection = LocalPlayer ? LocalPlayer->GetSubsystem<UTSAVSelectionSubsystem>() : nullptr;
		return Selection ? Selection->GetPrimarySelection() : nullptr;
	}
}

TSharedRef<SWidget> UTSAVMainWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	UE_LOG(LogTSAVPrevisRuntime, Display, TEXT("Main application widget rebuilt (tree=%s, root=%s)."),
		WidgetTree ? TEXT("valid") : TEXT("null"),
		WidgetTree && WidgetTree->RootWidget ? TEXT("valid") : TEXT("null"));

	return Super::RebuildWidget();
}

void UTSAVMainWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UE_LOG(LogTSAVPrevisRuntime, Display, TEXT("Main application widget constructed."));

	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UTSAVSelectionSubsystem* SelectionSubsystem = LocalPlayer->GetSubsystem<UTSAVSelectionSubsystem>())
		{
			SelectionSubsystem->OnSelectionChanged.AddUniqueDynamic(this, &UTSAVMainWidget::HandleSelectionChanged);
			UpdateInspector(SelectionSubsystem->GetPrimarySelection());
		}
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UTSAVModeSubsystem* ModeSubsystem = GameInstance->GetSubsystem<UTSAVModeSubsystem>())
		{
			ModeSubsystem->OnModeChanged.AddUniqueDynamic(this, &UTSAVMainWidget::HandleModeChanged);
			HandleModeChanged(ModeSubsystem->GetMode(), ModeSubsystem->GetMode());
		}
		if (UTSAVCommandSubsystem* CommandSubsystem = GameInstance->GetSubsystem<UTSAVCommandSubsystem>())
		{
			CommandSubsystem->OnObjectChanged.AddUniqueDynamic(this, &UTSAVMainWidget::HandleCommandObjectChanged);
			CommandSubsystem->OnHistoryChanged.AddUniqueDynamic(this, &UTSAVMainWidget::HandleCommandHistoryChanged);
		}
		if (UTSAVProjectSubsystem* ProjectSubsystem = GameInstance->GetSubsystem<UTSAVProjectSubsystem>())
		{
			ProjectSubsystem->OnProjectChanged.AddUniqueDynamic(this, &UTSAVMainWidget::HandleProjectChanged);
		}
	}
	RefreshOutliner();
	RefreshProjectStatus();
}

void UTSAVMainWidget::NativeDestruct()
{
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UTSAVSelectionSubsystem* SelectionSubsystem = LocalPlayer->GetSubsystem<UTSAVSelectionSubsystem>())
		{
			SelectionSubsystem->OnSelectionChanged.RemoveDynamic(this, &UTSAVMainWidget::HandleSelectionChanged);
		}
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UTSAVModeSubsystem* ModeSubsystem = GameInstance->GetSubsystem<UTSAVModeSubsystem>())
		{
			ModeSubsystem->OnModeChanged.RemoveDynamic(this, &UTSAVMainWidget::HandleModeChanged);
		}
		if (UTSAVCommandSubsystem* CommandSubsystem = GameInstance->GetSubsystem<UTSAVCommandSubsystem>())
		{
			CommandSubsystem->OnObjectChanged.RemoveDynamic(this, &UTSAVMainWidget::HandleCommandObjectChanged);
			CommandSubsystem->OnHistoryChanged.RemoveDynamic(this, &UTSAVMainWidget::HandleCommandHistoryChanged);
		}
		if (UTSAVProjectSubsystem* ProjectSubsystem = GameInstance->GetSubsystem<UTSAVProjectSubsystem>())
		{
			ProjectSubsystem->OnProjectChanged.RemoveDynamic(this, &UTSAVMainWidget::HandleProjectChanged);
		}
	}

	Super::NativeDestruct();
}

void UTSAVMainWidget::BuildLayout()
{
	using namespace TSAVMainWidget::Private;

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TSAVRootCanvas"));
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = RootCanvas;

	UBorder* TopBar = WidgetTree->ConstructWidget<UBorder>();
	TopBar->SetBrushColor(ChromeColor);
	TopBar->SetPadding(FMargin(12.0f, 6.0f));
	UHorizontalBox* TopBarContent = WidgetTree->ConstructWidget<UHorizontalBox>();
	TopBar->SetContent(TopBarContent);
	AddCanvasPanel(*RootCanvas, *TopBar, FAnchors(0.0f, 0.0f, 1.0f, 0.0f), FMargin(0.0f, 0.0f, 0.0f, 42.0f), FVector2D::ZeroVector, 10);

	UTextBlock* AppTitle = CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "AppTitle", "TSAV  PREVIS"), 16, AccentColor);
	if (UHorizontalBoxSlot* TitleSlot = TopBarContent->AddChildToHorizontalBox(AppTitle))
	{
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 28.0f, 0.0f));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
	}

#define TSAV_ADD_TOP_MENU(Label, Handler) \
	do \
	{ \
		UButton* Button = WidgetTree->ConstructWidget<UButton>(); \
		Button->SetBackgroundColor(FLinearColor(0.04f, 0.05f, 0.07f, 0.35f)); \
		Button->SetContent(CreateText(*WidgetTree, FText::FromString(TEXT(Label)), 12, PrimaryTextColor)); \
		Button->OnClicked.AddDynamic(this, &UTSAVMainWidget::Handler); \
		if (UHorizontalBoxSlot* ButtonSlot = TopBarContent->AddChildToHorizontalBox(Button)) \
		{ \
			ButtonSlot->SetPadding(FMargin(2.0f, 0.0f)); \
			ButtonSlot->SetVerticalAlignment(VAlign_Center); \
		} \
	} while (false)

	TSAV_ADD_TOP_MENU("File", FileMenuClicked);
	TSAV_ADD_TOP_MENU("Edit", EditMenuClicked);
	TSAV_ADD_TOP_MENU("Build", BuildMenuClicked);
	TSAV_ADD_TOP_MENU("LED", LEDMenuClicked);
	TSAV_ADD_TOP_MENU("Lighting", LightingMenuClicked);
	TSAV_ADD_TOP_MENU("Video", VideoMenuClicked);
	TSAV_ADD_TOP_MENU("Camera", CameraMenuClicked);
	TSAV_ADD_TOP_MENU("View", ViewMenuClicked);

#undef TSAV_ADD_TOP_MENU

	MenuPopup = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TopMenuPopup"));
	MenuPopup->SetBrushColor(FLinearColor(0.025f, 0.032f, 0.045f, 0.99f));
	MenuPopup->SetPadding(FMargin(6.0f));
	MenuPopupEntries = WidgetTree->ConstructWidget<UVerticalBox>();
	MenuPopup->SetContent(MenuPopupEntries);
	MenuPopupSlot = RootCanvas->AddChildToCanvas(MenuPopup);
	MenuPopupSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	MenuPopupSlot->SetOffsets(FMargin(160.0f, 42.0f, 235.0f, 300.0f));
	MenuPopupSlot->SetZOrder(50);
	MenuPopup->SetVisibility(ESlateVisibility::Collapsed);

	UBorder* LeftPanel = WidgetTree->ConstructWidget<UBorder>();
	LeftPanel->SetBrushColor(PanelColor);
	LeftPanel->SetPadding(FMargin(12.0f));
	UVerticalBox* LeftContent = WidgetTree->ConstructWidget<UVerticalBox>();
	LeftPanel->SetContent(LeftContent);
	AddCanvasPanel(*RootCanvas, *LeftPanel, FAnchors(0.0f, 0.0f, 0.0f, 1.0f), FMargin(0.0f, 43.0f, 260.0f, 31.0f), FVector2D::ZeroVector, 5);

	LeftContent->AddChildToVerticalBox(CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "OutlinerHeader", "PROJECT OUTLINER"), 13, AccentColor));
	OutlinerSelectionText = CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "NothingSelected", "No object selected"), 11, MutedTextColor);
	if (UVerticalBoxSlot* SelectionSlot = LeftContent->AddChildToVerticalBox(OutlinerSelectionText))
	{
		SelectionSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 16.0f));
	}
	UScrollBox* OutlinerScroll = WidgetTree->ConstructWidget<UScrollBox>();
	OutlinerEntries = WidgetTree->ConstructWidget<UVerticalBox>();
	OutlinerScroll->AddChild(OutlinerEntries);
	if (UVerticalBoxSlot* OutlinerSlot = LeftContent->AddChildToVerticalBox(OutlinerScroll))
	{
		OutlinerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		OutlinerSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	LeftContent->AddChildToVerticalBox(CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "ModesHeader", "AUTHORING MODES"), 13, AccentColor));

#define TSAV_ADD_MODE_BUTTON(Label, Handler) \
	do \
	{ \
		UButton* Button = CreateModeButton(*WidgetTree, FText::FromString(TEXT(Label))); \
		Button->OnClicked.AddDynamic(this, &UTSAVMainWidget::Handler); \
		if (UVerticalBoxSlot* ButtonSlot = LeftContent->AddChildToVerticalBox(Button)) \
		{ \
			ButtonSlot->SetPadding(FMargin(0.0f, 3.0f)); \
		} \
	} while (false)

	TSAV_ADD_MODE_BUTTON("Select", SelectModeClicked);
	TSAV_ADD_MODE_BUTTON("Venue", VenueModeClicked);
	TSAV_ADD_MODE_BUTTON("Stage", StageModeClicked);
	TSAV_ADD_MODE_BUTTON("Truss", TrussModeClicked);
	TSAV_ADD_MODE_BUTTON("Lighting", LightingModeClicked);
	TSAV_ADD_MODE_BUTTON("LED", LEDModeClicked);
	TSAV_ADD_MODE_BUTTON("Camera", CameraModeClicked);
	TSAV_ADD_MODE_BUTTON("Video", VideoModeClicked);
	TSAV_ADD_MODE_BUTTON("Characters", CharactersModeClicked);
	TSAV_ADD_MODE_BUTTON("Walkthrough", WalkthroughModeClicked);

#undef TSAV_ADD_MODE_BUTTON

	UBorder* RightPanel = WidgetTree->ConstructWidget<UBorder>();
	RightPanel->SetBrushColor(PanelColor);
	RightPanel->SetPadding(FMargin(14.0f));
	UScrollBox* InspectorScroll = WidgetTree->ConstructWidget<UScrollBox>();
	UVerticalBox* InspectorContent = WidgetTree->ConstructWidget<UVerticalBox>();
	InspectorScroll->AddChild(InspectorContent);
	RightPanel->SetContent(InspectorScroll);
	AddCanvasPanel(*RootCanvas, *RightPanel, FAnchors(1.0f, 0.0f, 1.0f, 1.0f), FMargin(-300.0f, 43.0f, 300.0f, 31.0f), FVector2D::ZeroVector, 5);

	InspectorTitleText = CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "InspectorHeader", "INSPECTOR"), 13, AccentColor);
	InspectorContent->AddChildToVerticalBox(InspectorTitleText);
	InspectorBodyText = CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "InspectorEmpty", "Select a TSAV object in the viewport."), 11, PrimaryTextColor);
	InspectorBodyText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* InspectorSlot = InspectorContent->AddChildToVerticalBox(InspectorBodyText))
	{
		InspectorSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));
	}

	NameField = CreateEditField(*WidgetTree, NSLOCTEXT("TSAVPreVis", "NameHint", "Object name"));
	NameField->OnTextCommitted.AddDynamic(this, &UTSAVMainWidget::NameCommitted);
	AddLabeledField(*WidgetTree, *InspectorContent, NSLOCTEXT("TSAVPreVis", "NameLabel", "NAME"), *NameField);

	auto AddTransformRow = [&](const FText& Label, TArray<TObjectPtr<UEditableTextBox>>& Fields)
	{
		InspectorContent->AddChildToVerticalBox(CreateText(*WidgetTree, Label, 10, MutedTextColor));
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		if (UVerticalBoxSlot* RowSlot = InspectorContent->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 3.0f));
		}
		const TCHAR* AxisLabels[] = { TEXT("X"), TEXT("Y"), TEXT("Z") };
		for (const TCHAR* AxisLabel : AxisLabels)
		{
			UEditableTextBox* Field = CreateEditField(*WidgetTree, FText::FromString(AxisLabel));
			Fields.Add(Field);
			if (UHorizontalBoxSlot* FieldSlot = Row->AddChildToHorizontalBox(Field))
			{
				FieldSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				FieldSlot->SetPadding(FMargin(1.0f));
			}
		}
	};

	AddTransformRow(NSLOCTEXT("TSAVPreVis", "LocationLabel", "LOCATION (CM)"), LocationFields);
	AddTransformRow(NSLOCTEXT("TSAVPreVis", "RotationLabel", "ROTATION (DEG)"), RotationFields);
	AddTransformRow(NSLOCTEXT("TSAVPreVis", "ScaleLabel", "SCALE"), ScaleFields);
	LocationFields[0]->OnTextCommitted.AddDynamic(this, &UTSAVMainWidget::LocationXCommitted);
	LocationFields[1]->OnTextCommitted.AddDynamic(this, &UTSAVMainWidget::LocationYCommitted);
	LocationFields[2]->OnTextCommitted.AddDynamic(this, &UTSAVMainWidget::LocationZCommitted);
	RotationFields[0]->OnTextCommitted.AddDynamic(this, &UTSAVMainWidget::RotationPitchCommitted);
	RotationFields[1]->OnTextCommitted.AddDynamic(this, &UTSAVMainWidget::RotationYawCommitted);
	RotationFields[2]->OnTextCommitted.AddDynamic(this, &UTSAVMainWidget::RotationRollCommitted);
	ScaleFields[0]->OnTextCommitted.AddDynamic(this, &UTSAVMainWidget::ScaleXCommitted);
	ScaleFields[1]->OnTextCommitted.AddDynamic(this, &UTSAVMainWidget::ScaleYCommitted);
	ScaleFields[2]->OnTextCommitted.AddDynamic(this, &UTSAVMainWidget::ScaleZCommitted);

	UHorizontalBox* ObjectActions = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* ActionSlot = InspectorContent->AddChildToVerticalBox(ObjectActions))
	{
		ActionSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	}
#define TSAV_ADD_OBJECT_ACTION(Label, Handler) \
	do \
	{ \
		UButton* Button = CreateModeButton(*WidgetTree, FText::FromString(TEXT(Label))); \
		Button->OnClicked.AddDynamic(this, &UTSAVMainWidget::Handler); \
		if (UHorizontalBoxSlot* ButtonSlot = ObjectActions->AddChildToHorizontalBox(Button)) \
		{ \
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); \
			ButtonSlot->SetPadding(FMargin(1.0f)); \
		} \
	} while (false)
	TSAV_ADD_OBJECT_ACTION("Lock", ToggleLockedClicked);
	TSAV_ADD_OBJECT_ACTION("Hide", ToggleVisibleClicked);
	TSAV_ADD_OBJECT_ACTION("Delete", DeleteClicked);
#undef TSAV_ADD_OBJECT_ACTION

	ContextToolPanel = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UVerticalBoxSlot* ContextSlot = InspectorContent->AddChildToVerticalBox(ContextToolPanel))
	{
		ContextSlot->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 12.0f));
	}

	UBorder* BottomBar = WidgetTree->ConstructWidget<UBorder>();
	BottomBar->SetBrushColor(ChromeColor);
	BottomBar->SetPadding(FMargin(12.0f, 5.0f));
	UHorizontalBox* BottomContent = WidgetTree->ConstructWidget<UHorizontalBox>();
	BottomBar->SetContent(BottomContent);
	AddCanvasPanel(*RootCanvas, *BottomBar, FAnchors(0.0f, 1.0f, 1.0f, 1.0f), FMargin(0.0f, -30.0f, 0.0f, 30.0f), FVector2D::ZeroVector, 10);

	ModeStatusText = CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "InitialStatus", "SELECT MODE"), 11, AccentColor);
	BottomContent->AddChildToHorizontalBox(ModeStatusText);
	ProjectStatusText = CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "StatusItems", "PROJECT: UNTITLED"), 10, MutedTextColor);
	if (UHorizontalBoxSlot* StatusSlot = BottomContent->AddChildToHorizontalBox(ProjectStatusText))
	{
		StatusSlot->SetPadding(FMargin(24.0f, 0.0f));
	}
	UndoStatusText = CreateText(*WidgetTree, FText::GetEmpty(), 10, MutedTextColor);
	if (UHorizontalBoxSlot* UndoSlot = BottomContent->AddChildToHorizontalBox(UndoStatusText))
	{
		UndoSlot->SetPadding(FMargin(24.0f, 0.0f));
	}

	UTextBlock* ViewportHint = CreateText(
		*WidgetTree,
		NSLOCTEXT("TSAVPreVis", "ViewportHint", "RMB + WASD/Q/E  Fly     LMB  Select/Drag     W/E/R  Move/Rotate/Scale     X  World/Local     Ctrl+Z/Y  Undo/Redo"),
		11,
		MutedTextColor);
	ViewportHint->SetJustification(ETextJustify::Center);
	AddCanvasPanel(*RootCanvas, *ViewportHint, FAnchors(0.5f, 1.0f), FMargin(0.0f, -58.0f, 560.0f, 22.0f), FVector2D(0.5f, 1.0f), 2);
}

void UTSAVMainWidget::ToggleMenu(const ETSAVTopMenu Menu, const float LeftPosition)
{
	if (!MenuPopup || !MenuPopupEntries || !MenuPopupSlot)
	{
		return;
	}
	if (OpenMenu == Menu && MenuPopup->GetVisibility() == ESlateVisibility::Visible)
	{
		HideMenu();
		return;
	}

	OpenMenu = Menu;
	MenuPopupEntries->ClearChildren();
	const AActor* Selection = TSAVMainWidget::Private::GetSelectedActor(*this);
	const UTSAVCommandSubsystem* Commands = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>() : nullptr;

	switch (Menu)
	{
	case ETSAVTopMenu::File:
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuNew", "New Project"), ETSAVMenuAction::NewProject);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuSave", "Save Project    Ctrl+S"), ETSAVMenuAction::SaveProject);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuLoad", "Load Project    Ctrl+O"), ETSAVMenuAction::LoadProject);
		break;
	case ETSAVTopMenu::Edit:
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuUndo", "Undo    Ctrl+Z"), ETSAVMenuAction::Undo, Commands && Commands->CanUndo());
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuRedo", "Redo    Ctrl+Y"), ETSAVMenuAction::Redo, Commands && Commands->CanRedo());
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuDuplicate", "Duplicate Selection    Ctrl+D"), ETSAVMenuAction::DuplicateSelection, Selection != nullptr);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuDelete", "Delete Selection    Del"), ETSAVMenuAction::DeleteSelection, Selection != nullptr);
		break;
	case ETSAVTopMenu::Build:
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuVenueFloor", "Add Venue Floor"), ETSAVMenuAction::AddVenueFloor);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuStageDeck", "Add Stage Deck"), ETSAVMenuAction::AddStageDeck);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuScenicCube", "Add Scenic Cube"), ETSAVMenuAction::AddScenicCube);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuTruss", "Add Truss Segment"), ETSAVMenuAction::AddTrussSegment);
		break;
	case ETSAVTopMenu::LED:
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuLEDWall", "Add / Configure LED Wall"), ETSAVMenuAction::AddLEDWall);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuLEDPanel", "Add LED Panel"), ETSAVMenuAction::AddLEDPanel);
		break;
	case ETSAVTopMenu::Lighting:
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuDMXFixture", "Add DMX Fixture"), ETSAVMenuAction::AddDMXFixture);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuPointLight", "Add Point Light"), ETSAVMenuAction::AddPointLight);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuSpotLight", "Add Spot Light"), ETSAVMenuAction::AddSpotLight);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuRectLight", "Add Rect Light"), ETSAVMenuAction::AddRectLight);
		break;
	case ETSAVTopMenu::Video:
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuVideoSwitcher", "Add Video Switcher"), ETSAVMenuAction::AddVideoSwitcher);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuVideoSurface", "Add Video Surface"), ETSAVMenuAction::AddVideoSurface);
		break;
	case ETSAVTopMenu::Camera:
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuCamera", "Add Camera From Current View"), ETSAVMenuAction::AddCamera);
		break;
	case ETSAVTopMenu::View:
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuSelectView", "Edit View"), ETSAVMenuAction::SelectView);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuWalkthrough", "Walkthrough View"), ETSAVMenuAction::WalkthroughView);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuFrameSelection", "Frame Selection"), ETSAVMenuAction::FrameSelection, Selection != nullptr);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuTranslate", "Translate Tool    W"), ETSAVMenuAction::TranslateTool);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuRotate", "Rotate Tool    E"), ETSAVMenuAction::RotateTool);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuScale", "Scale Tool    R"), ETSAVMenuAction::ScaleTool);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuCoordinates", "Toggle World / Local    X"), ETSAVMenuAction::ToggleCoordinateSpace);
		break;
	default:
		break;
	}

	const float PopupHeight = 12.0f + MenuPopupEntries->GetChildrenCount() * 32.0f;
	MenuPopupSlot->SetOffsets(FMargin(LeftPosition, 42.0f, 235.0f, PopupHeight));
	MenuPopup->SetVisibility(ESlateVisibility::Visible);
}

void UTSAVMainWidget::HideMenu()
{
	OpenMenu = ETSAVTopMenu::None;
	if (MenuPopup)
	{
		MenuPopup->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTSAVMainWidget::AddMenuEntry(const FText& Label, const ETSAVMenuAction Action, const bool bEnabled)
{
	if (!MenuPopupEntries || !WidgetTree)
	{
		return;
	}
	UTSAVMenuButton* Button = WidgetTree->ConstructWidget<UTSAVMenuButton>();
	Button->SetBackgroundColor(FLinearColor(0.055f, 0.07f, 0.095f, 1.0f));
	Button->SetContent(TSAVMainWidget::Private::CreateText(*WidgetTree, Label, 11,
		bEnabled ? TSAVMainWidget::Private::PrimaryTextColor : TSAVMainWidget::Private::MutedTextColor));
	Button->SetIsEnabled(bEnabled);
	Button->InitializeForAction(Action);
	Button->OnActionClicked.AddUniqueDynamic(this, &UTSAVMainWidget::HandleMenuActionClicked);
	if (UVerticalBoxSlot* MenuEntrySlot = MenuPopupEntries->AddChildToVerticalBox(Button))
	{
		MenuEntrySlot->SetPadding(FMargin(0.0f, 1.0f));
		MenuEntrySlot->SetHorizontalAlignment(HAlign_Fill);
	}
}

FTransform UTSAVMainWidget::MakePlacementTransform(const FVector& Scale, const float Distance) const
{
	if (const APlayerController* Controller = GetOwningPlayer())
	{
		if (const APlayerCameraManager* Camera = Controller->PlayerCameraManager)
		{
			const FRotator CameraRotation = Camera->GetCameraRotation();
			return FTransform(
				FRotator(0.0f, CameraRotation.Yaw + 180.0f, 0.0f),
				Camera->GetCameraLocation() + CameraRotation.Vector() * Distance,
				Scale);
		}
	}
	return FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 100.0f), Scale);
}

AActor* UTSAVMainWidget::SpawnAndSelect(
	TSubclassOf<AActor> ActorClass,
	const FTransform& Transform,
	const FText& DisplayName,
	const ETSAVObjectType ObjectType)
{
	if (!GetGameInstance() || !GetWorld())
	{
		return nullptr;
	}
	AActor* Actor = GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>()->SpawnActorClass(
		GetWorld(), ActorClass, Transform, DisplayName, ObjectType);
	if (Actor && GetOwningLocalPlayer())
	{
		GetOwningLocalPlayer()->GetSubsystem<UTSAVSelectionSubsystem>()->SelectActor(Actor);
	}
	return Actor;
}

void UTSAVMainWidget::HandleMenuActionClicked(const ETSAVMenuAction Action)
{
	HideMenu();
	ExecuteMenuAction(Action);
}

void UTSAVMainWidget::ExecuteMenuAction(const ETSAVMenuAction Action)
{
	switch (Action)
	{
	case ETSAVMenuAction::NewProject: NewProjectClicked(); break;
	case ETSAVMenuAction::SaveProject: SaveProjectClicked(); break;
	case ETSAVMenuAction::LoadProject: LoadProjectClicked(); break;
	case ETSAVMenuAction::Undo: UndoClicked(); break;
	case ETSAVMenuAction::Redo: RedoClicked(); break;
	case ETSAVMenuAction::DuplicateSelection: DuplicateClicked(); break;
	case ETSAVMenuAction::DeleteSelection: DeleteClicked(); break;
	case ETSAVMenuAction::AddVenueFloor:
		SetAppMode(ETSAVAppMode::Venue);
		SpawnAndSelect(ATSAVSceneObjectActor::StaticClass(), MakePlacementTransform(FVector(10.0f, 10.0f, 0.1f), 700.0f),
			NSLOCTEXT("TSAVPreVis", "NewVenueFloor", "Venue Floor"), ETSAVObjectType::Venue);
		break;
	case ETSAVMenuAction::AddStageDeck:
		SetAppMode(ETSAVAppMode::Stage);
		SpawnAndSelect(ATSAVSceneObjectActor::StaticClass(), MakePlacementTransform(FVector(4.0f, 2.0f, 0.2f)),
			NSLOCTEXT("TSAVPreVis", "NewStageDeck", "Stage Deck"), ETSAVObjectType::Stage);
		break;
	case ETSAVMenuAction::AddScenicCube:
		AddCubeClicked();
		break;
	case ETSAVMenuAction::AddTrussSegment:
		SetAppMode(ETSAVAppMode::Truss);
		SpawnAndSelect(ATSAVSceneObjectActor::StaticClass(), MakePlacementTransform(FVector(3.0f, 0.15f, 0.15f)),
			NSLOCTEXT("TSAVPreVis", "NewTrussSegment", "Truss Segment"), ETSAVObjectType::Truss);
		break;
	case ETSAVMenuAction::AddLEDWall:
		SetAppMode(ETSAVAppMode::LED);
		SpawnAndSelect(ATSAVLEDWall::StaticClass(), MakePlacementTransform(),
			NSLOCTEXT("TSAVPreVis", "NewLEDWall", "LED Wall"), ETSAVObjectType::LED);
		break;
	case ETSAVMenuAction::AddLEDPanel:
		SetAppMode(ETSAVAppMode::LED);
		SpawnAndSelect(ATSAVLEDPanel::StaticClass(), MakePlacementTransform(),
			NSLOCTEXT("TSAVPreVis", "NewLEDPanel", "LED Panel"), ETSAVObjectType::LED);
		break;
	case ETSAVMenuAction::AddDMXFixture:
		SetAppMode(ETSAVAppMode::Lighting);
		SpawnAndSelect(ATSAVDMXFixture::StaticClass(), MakePlacementTransform(FVector::OneVector, 350.0f),
			NSLOCTEXT("TSAVPreVis", "NewDMXFixture", "DMX Fixture"), ETSAVObjectType::Fixture);
		break;
	case ETSAVMenuAction::AddPointLight:
		SetAppMode(ETSAVAppMode::Lighting);
		SpawnAndSelect(APointLight::StaticClass(), MakePlacementTransform(FVector::OneVector, 350.0f),
			NSLOCTEXT("TSAVPreVis", "NewPointLight", "Point Light"), ETSAVObjectType::Fixture);
		break;
	case ETSAVMenuAction::AddSpotLight:
		SetAppMode(ETSAVAppMode::Lighting);
		SpawnAndSelect(ASpotLight::StaticClass(), MakePlacementTransform(FVector::OneVector, 350.0f),
			NSLOCTEXT("TSAVPreVis", "NewSpotLight", "Spot Light"), ETSAVObjectType::Fixture);
		break;
	case ETSAVMenuAction::AddRectLight:
		SetAppMode(ETSAVAppMode::Lighting);
		SpawnAndSelect(ARectLight::StaticClass(), MakePlacementTransform(FVector::OneVector, 350.0f),
			NSLOCTEXT("TSAVPreVis", "NewRectLight", "Rect Light"), ETSAVObjectType::Fixture);
		break;
	case ETSAVMenuAction::AddVideoSurface:
		SetAppMode(ETSAVAppMode::Video);
		SpawnAndSelect(ATSAVLEDPanel::StaticClass(), MakePlacementTransform(FVector(4.0f, 1.0f, 2.25f)),
			NSLOCTEXT("TSAVPreVis", "NewVideoSurface", "Video Surface"), ETSAVObjectType::Video);
		break;
	case ETSAVMenuAction::AddVideoSwitcher:
		SetAppMode(ETSAVAppMode::Video);
		SpawnAndSelect(ATSAVVideoSwitcher::StaticClass(), MakePlacementTransform(FVector::OneVector, 350.0f),
			NSLOCTEXT("TSAVPreVis", "NewVideoSwitcher", "Video Switcher"), ETSAVObjectType::Video);
		break;
	case ETSAVMenuAction::AddCamera:
	{
		SetAppMode(ETSAVAppMode::Camera);
		FTransform CameraTransform = FTransform::Identity;
		if (const APlayerController* Controller = GetOwningPlayer())
		{
			if (const APlayerCameraManager* Camera = Controller->PlayerCameraManager)
			{
				CameraTransform = FTransform(Camera->GetCameraRotation(), Camera->GetCameraLocation());
			}
		}
		int32 CameraNumber = 1;
		if (GetWorld())
		{
			for (TActorIterator<ATSAVCameraActor> It(GetWorld()); It; ++It) { ++CameraNumber; }
		}
		const FText CameraName = FText::Format(NSLOCTEXT("TSAVPreVis", "NewCameraNumbered", "CAM {0}"), FText::AsNumber(CameraNumber));
		SpawnAndSelect(ATSAVCameraActor::StaticClass(), CameraTransform, CameraName, ETSAVObjectType::Camera);
		break;
	}
	case ETSAVMenuAction::SelectView: SetAppMode(ETSAVAppMode::Select); break;
	case ETSAVMenuAction::WalkthroughView: SetAppMode(ETSAVAppMode::Walkthrough); break;
	case ETSAVMenuAction::TranslateTool:
		if (ATSAVPlayerController* Controller = Cast<ATSAVPlayerController>(GetOwningPlayer())) { Controller->SetTransformTool(ETSAVTransformMode::Translate); }
		break;
	case ETSAVMenuAction::RotateTool:
		if (ATSAVPlayerController* Controller = Cast<ATSAVPlayerController>(GetOwningPlayer())) { Controller->SetTransformTool(ETSAVTransformMode::Rotate); }
		break;
	case ETSAVMenuAction::ScaleTool:
		if (ATSAVPlayerController* Controller = Cast<ATSAVPlayerController>(GetOwningPlayer())) { Controller->SetTransformTool(ETSAVTransformMode::Scale); }
		break;
	case ETSAVMenuAction::ToggleCoordinateSpace:
		if (ATSAVPlayerController* Controller = Cast<ATSAVPlayerController>(GetOwningPlayer())) { Controller->ToggleTransformCoordinateSpace(); }
		break;
	case ETSAVMenuAction::FrameSelection:
		if (ATSAVPlayerController* Controller = Cast<ATSAVPlayerController>(GetOwningPlayer())) { Controller->FrameSelection(); }
		break;
	}
}

void UTSAVMainWidget::FileMenuClicked() { ToggleMenu(ETSAVTopMenu::File, 158.0f); }
void UTSAVMainWidget::EditMenuClicked() { ToggleMenu(ETSAVTopMenu::Edit, 205.0f); }
void UTSAVMainWidget::BuildMenuClicked() { ToggleMenu(ETSAVTopMenu::Build, 252.0f); }
void UTSAVMainWidget::LEDMenuClicked() { ToggleMenu(ETSAVTopMenu::LED, 306.0f); }
void UTSAVMainWidget::LightingMenuClicked() { ToggleMenu(ETSAVTopMenu::Lighting, 350.0f); }
void UTSAVMainWidget::VideoMenuClicked() { ToggleMenu(ETSAVTopMenu::Video, 420.0f); }
void UTSAVMainWidget::CameraMenuClicked() { ToggleMenu(ETSAVTopMenu::Camera, 474.0f); }
void UTSAVMainWidget::ViewMenuClicked() { ToggleMenu(ETSAVTopMenu::View, 542.0f); }

void UTSAVMainWidget::SetAppMode(const ETSAVAppMode NewMode)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UTSAVModeSubsystem* ModeSubsystem = GameInstance->GetSubsystem<UTSAVModeSubsystem>())
		{
			ModeSubsystem->SetMode(NewMode);
		}
	}
}

void UTSAVMainWidget::UpdateInspector(AActor* SelectedActor)
{
	if (!OutlinerSelectionText || !InspectorBodyText)
	{
		return;
	}

	if (!IsValid(SelectedActor))
	{
		ContextActor = nullptr;
		BuildContextTools(nullptr);
		OutlinerSelectionText->SetText(NSLOCTEXT("TSAVPreVis", "NothingSelected", "No object selected"));
		InspectorBodyText->SetText(NSLOCTEXT("TSAVPreVis", "InspectorEmpty", "Select a TSAV object in the viewport."));
		if (NameField) { NameField->SetText(FText::GetEmpty()); NameField->SetIsEnabled(false); }
		for (UEditableTextBox* Field : LocationFields) { if (Field) { Field->SetText(FText::GetEmpty()); Field->SetIsEnabled(false); } }
		for (UEditableTextBox* Field : RotationFields) { if (Field) { Field->SetText(FText::GetEmpty()); Field->SetIsEnabled(false); } }
		for (UEditableTextBox* Field : ScaleFields) { if (Field) { Field->SetText(FText::GetEmpty()); Field->SetIsEnabled(false); } }
		return;
	}
	if (ContextActor != SelectedActor)
	{
		ContextActor = SelectedActor;
		BuildContextTools(SelectedActor);
	}

	FText DisplayName = FText::FromString(SelectedActor->GetName());
	FString ObjectId = TEXT("Unassigned");
	if (const UTSAVSceneObjectComponent* SceneObject = SelectedActor->FindComponentByClass<UTSAVSceneObjectComponent>())
	{
		if (!SceneObject->DisplayName.IsEmpty())
		{
			DisplayName = SceneObject->DisplayName;
		}
		ObjectId = SceneObject->ObjectId.ToString(EGuidFormats::DigitsWithHyphens);
	}

	OutlinerSelectionText->SetText(DisplayName);
	const FTransform Transform = SelectedActor->GetActorTransform();
	const FVector Location = Transform.GetLocation();
	const FRotator Rotation = Transform.Rotator();
	const FVector Scale = Transform.GetScale3D();
	InspectorBodyText->SetText(FText::FromString(FString::Printf(
		TEXT("%s\nObject ID  %s"),
		*DisplayName.ToString(),
		*ObjectId)));
	if (NameField)
	{
		NameField->SetIsEnabled(true);
		NameField->SetText(DisplayName);
	}
	const FVector Values[] = { Location, FVector(Rotation.Pitch, Rotation.Yaw, Rotation.Roll), Scale };
	TArray<TObjectPtr<UEditableTextBox>>* Groups[] = { &LocationFields, &RotationFields, &ScaleFields };
	for (int32 GroupIndex = 0; GroupIndex < 3; ++GroupIndex)
	{
		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			if (UEditableTextBox* Field = (*Groups[GroupIndex])[AxisIndex])
			{
				Field->SetIsEnabled(true);
				Field->SetText(FText::FromString(FString::SanitizeFloat(Values[GroupIndex][AxisIndex], GroupIndex == 2 ? 3 : 2)));
			}
		}
	}
}

void UTSAVMainWidget::CommitContextState(AActor* Actor, const FString& BeforeState, const FText& Description)
{
	if (Actor && GetGameInstance())
	{
		GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>()->CommitAppliedActorState(Actor, BeforeState, Description);
	}
}

void UTSAVMainWidget::BuildContextTools(AActor* SelectedActor)
{
	using namespace TSAVMainWidget::Private;
	if (!ContextToolPanel || !WidgetTree)
	{
		return;
	}
	ContextToolPanel->ClearChildren();
	ContextSwitcher = Cast<ATSAVVideoSwitcher>(SelectedActor);
	ContextMediaSurface = Cast<ATSAVMediaSurfaceActor>(SelectedActor);
	ContextLEDWall = Cast<ATSAVLEDWall>(SelectedActor);
	ContextCamera = Cast<ATSAVCameraActor>(SelectedActor);
	SwitcherInputNameField = nullptr;
	SwitcherInputUrlField = nullptr;
	LEDWallFields.Reset();
	LEDPanelColumnField = nullptr;
	LEDPanelRowField = nullptr;
	LEDPanelStyleCombo = nullptr;
	LEDLinkPatternCombo = nullptr;
	LEDSubpixelCombo = nullptr;
	CameraTypeCombo = nullptr;
	CameraLensCombo = nullptr;
	CameraFocalLengthField = nullptr;
	CameraApertureField = nullptr;
	CameraFocusField = nullptr;
	CameraViscaIpField = nullptr;
	CameraViscaPortField = nullptr;
	CameraPanField = nullptr;
	CameraTiltField = nullptr;
	CameraZoomField = nullptr;

	if (ContextSwitcher)
	{
		ContextToolPanel->AddChildToVerticalBox(CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "SwitcherToolHeader", "VIDEO SWITCHER"), 13, AccentColor));
		UTextBlock* Status = CreateText(*WidgetTree, FText::Format(
			NSLOCTEXT("TSAVPreVis", "SwitcherStatus", "PROGRAM  {0}\nPREVIEW  {1}"),
			ContextSwitcher->GetBusInputLabel(TEXT("Program")), ContextSwitcher->GetBusInputLabel(TEXT("Preview"))), 11, PrimaryTextColor);
		if (UVerticalBoxSlot* StatusSlot = ContextToolPanel->AddChildToVerticalBox(Status))
		{
			StatusSlot->SetPadding(FMargin(0.0f, 6.0f));
		}

		UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
		ContextToolPanel->AddChildToVerticalBox(Actions);
		auto AddAction = [&](const FText& Label, const FName HandlerName)
		{
			UButton* Button = CreateModeButton(*WidgetTree, Label);
			FScriptDelegate Delegate;
			Delegate.BindUFunction(this, HandlerName);
			Button->OnClicked.Add(Delegate);
			if (UHorizontalBoxSlot* ActionSlot = Actions->AddChildToHorizontalBox(Button))
			{
				ActionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				ActionSlot->SetPadding(FMargin(1.0f));
			}
		};
		AddAction(NSLOCTEXT("TSAVPreVis", "DiscoverVideoSources", "Discover"), GET_FUNCTION_NAME_CHECKED(UTSAVMainWidget, SwitcherDiscoverClicked));
		AddAction(NSLOCTEXT("TSAVPreVis", "CutVideo", "CUT"), GET_FUNCTION_NAME_CHECKED(UTSAVMainWidget, SwitcherCutClicked));
		AddAction(NSLOCTEXT("TSAVPreVis", "AutoVideo", "AUTO"), GET_FUNCTION_NAME_CHECKED(UTSAVMainWidget, SwitcherAutoClicked));

		ContextToolPanel->AddChildToVerticalBox(CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "SwitcherInputs", "INPUT CROSSPOINTS"), 10, MutedTextColor));
		for (const FTSAVVideoInput& Input : ContextSwitcher->Inputs)
		{
			if (UVerticalBoxSlot* InputLabelSlot = ContextToolPanel->AddChildToVerticalBox(CreateText(*WidgetTree, Input.Label, 10, PrimaryTextColor)))
			{
				InputLabelSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 1.0f));
			}
			UHorizontalBox* Crosspoints = WidgetTree->ConstructWidget<UHorizontalBox>();
			ContextToolPanel->AddChildToVerticalBox(Crosspoints);
			const FName BusNames[] = { TEXT("Preview"), TEXT("Program"), TEXT("Aux 1"), TEXT("Aux 2") };
			const TCHAR* BusLabels[] = { TEXT("PVW"), TEXT("PGM"), TEXT("AUX1"), TEXT("AUX2") };
			for (int32 Index = 0; Index < 4; ++Index)
			{
				UTSAVSwitcherInputButton* Button = WidgetTree->ConstructWidget<UTSAVSwitcherInputButton>();
				const bool bSelected = ContextSwitcher->GetBusInputId(BusNames[Index]) == Input.InputId;
				Button->SetBackgroundColor(bSelected ? AccentColor : FLinearColor(0.055f, 0.07f, 0.095f, 1.0f));
				Button->SetContent(CreateText(*WidgetTree, FText::FromString(BusLabels[Index]), 9, PrimaryTextColor));
				Button->InitializeRoute(ContextSwitcher, Input.InputId, BusNames[Index]);
				Button->OnRouteClicked.AddUniqueDynamic(this, &UTSAVMainWidget::HandleSwitcherRouteClicked);
				if (UHorizontalBoxSlot* RouteSlot = Crosspoints->AddChildToHorizontalBox(Button))
				{
					RouteSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
					RouteSlot->SetPadding(FMargin(1.0f));
				}
			}
		}

		ContextToolPanel->AddChildToVerticalBox(CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "AddStreamInputHeader", "ADD URL / NDI INPUT"), 10, MutedTextColor));
		SwitcherInputNameField = CreateEditField(*WidgetTree, NSLOCTEXT("TSAVPreVis", "InputNameHint", "Input name"));
		SwitcherInputUrlField = CreateEditField(*WidgetTree, NSLOCTEXT("TSAVPreVis", "InputUrlHint", "Stream or NDI URL"));
		ContextToolPanel->AddChildToVerticalBox(SwitcherInputNameField);
		ContextToolPanel->AddChildToVerticalBox(SwitcherInputUrlField);
		UButton* AddUrl = CreateModeButton(*WidgetTree, NSLOCTEXT("TSAVPreVis", "AddInputButton", "Add Input"));
		AddUrl->OnClicked.AddDynamic(this, &UTSAVMainWidget::SwitcherAddUrlClicked);
		ContextToolPanel->AddChildToVerticalBox(AddUrl);
		return;
	}

	if (ContextLEDWall)
	{
		ContextToolPanel->AddChildToVerticalBox(CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDConfiguratorHeader", "LED WALL CONFIGURATOR"), 13, AccentColor));
		const FIntPoint WallResolution = ContextLEDWall->GetWallResolutionPixels();
		ContextToolPanel->AddChildToVerticalBox(CreateText(*WidgetTree, FText::Format(
			NSLOCTEXT("TSAVPreVis", "LEDWallSummary", "{0} x {1} cabinets  |  {2} x {3} px\n{4} cm wide  |  {5} cm high"),
			FText::AsNumber(ContextLEDWall->Columns), FText::AsNumber(ContextLEDWall->Rows),
			FText::AsNumber(WallResolution.X), FText::AsNumber(WallResolution.Y),
			FText::AsNumber(ContextLEDWall->GetWallWidthCm()), FText::AsNumber(ContextLEDWall->GetWallHeightCm())), 10, PrimaryTextColor));

		LEDWallFields.SetNum(LEDFieldCount);
		auto AddLEDField = [&](const int32 Index, const FText& Label, const FString& Value, const FText& Hint = FText::GetEmpty())
		{
			UEditableTextBox* Field = CreateEditField(*WidgetTree, Hint);
			Field->SetText(FText::FromString(Value));
			LEDWallFields[Index] = Field;
			AddLabeledField(*WidgetTree, *ContextToolPanel, Label, *Field);
		};
		AddLEDField(LEDColumns, NSLOCTEXT("TSAVPreVis", "LEDColumns", "COLUMNS"), FString::FromInt(ContextLEDWall->Columns));
		AddLEDField(LEDRows, NSLOCTEXT("TSAVPreVis", "LEDRows", "ROWS"), FString::FromInt(ContextLEDWall->Rows));
		AddLEDField(LEDPanelWidth, NSLOCTEXT("TSAVPreVis", "LEDPanelWidth", "CABINET WIDTH (CM)"), FString::SanitizeFloat(ContextLEDWall->PanelWidthCm));
		AddLEDField(LEDPanelHeight, NSLOCTEXT("TSAVPreVis", "LEDPanelHeight", "CABINET HEIGHT (CM)"), FString::SanitizeFloat(ContextLEDWall->PanelHeightCm));
		AddLEDField(LEDPanelDepth, NSLOCTEXT("TSAVPreVis", "LEDPanelDepth", "CABINET DEPTH (CM)"), FString::SanitizeFloat(ContextLEDWall->WallDepthCm));
		AddLEDField(LEDPanelResolutionX, NSLOCTEXT("TSAVPreVis", "LEDPanelResX", "CABINET RESOLUTION X"), FString::FromInt(ContextLEDWall->PanelResolutionX));
		AddLEDField(LEDPanelResolutionY, NSLOCTEXT("TSAVPreVis", "LEDPanelResY", "CABINET RESOLUTION Y"), FString::FromInt(ContextLEDWall->PanelResolutionY));
		AddLEDField(LEDPanelGap, NSLOCTEXT("TSAVPreVis", "LEDPanelGap", "CABINET GAP (CM)"), FString::SanitizeFloat(ContextLEDWall->PanelGapCm));
		AddLEDField(LEDBorder, NSLOCTEXT("TSAVPreVis", "LEDBorder", "BORDER / CHAMFER (CM)"), FString::SanitizeFloat(ContextLEDWall->BorderCm));
		AddLEDField(LEDRoundRadius, NSLOCTEXT("TSAVPreVis", "LEDRoundRadius", "ROUND EDGE RADIUS (M)"), FString::SanitizeFloat(ContextLEDWall->RoundEdgeRadiusMeters));
		AddLEDField(LEDCanvasWidth, NSLOCTEXT("TSAVPreVis", "LEDCanvasWidth", "PROCESSOR CANVAS WIDTH"), FString::FromInt(ContextLEDWall->CanvasResolution.X));
		AddLEDField(LEDCanvasHeight, NSLOCTEXT("TSAVPreVis", "LEDCanvasHeight", "PROCESSOR CANVAS HEIGHT"), FString::FromInt(ContextLEDWall->CanvasResolution.Y));
		AddLEDField(LEDCanvasX, NSLOCTEXT("TSAVPreVis", "LEDCanvasX", "CANVAS X"), FString::FromInt(ContextLEDWall->CanvasPosition.X));
		AddLEDField(LEDCanvasY, NSLOCTEXT("TSAVPreVis", "LEDCanvasY", "CANVAS Y"), FString::FromInt(ContextLEDWall->CanvasPosition.Y));
		AddLEDField(LEDColumnSeams, NSLOCTEXT("TSAVPreVis", "LEDColumnSeams", "COLUMN SEAM ANGLES (CSV)"), JoinFloatValues(ContextLEDWall->ColumnSeamAnglesDegrees), NSLOCTEXT("TSAVPreVis", "LEDColumnSeamHint", "Example: 0, 15, 15, 0"));
		AddLEDField(LEDRowSeams, NSLOCTEXT("TSAVPreVis", "LEDRowSeams", "ROW SEAM ANGLES (CSV)"), JoinFloatValues(ContextLEDWall->RowSeamAnglesDegrees));
		AddLEDField(LEDCurvedColumns, NSLOCTEXT("TSAVPreVis", "LEDCurvedColumns", "INTERNALLY CURVED COLUMNS (1-BASED CSV)"), JoinEnabledIndices(ContextLEDWall->ColumnInternalCurveEnabled), NSLOCTEXT("TSAVPreVis", "LEDCurvedColumnsHint", "Example: 3, 4"));
		AddLEDField(LEDCurveAnglesA, NSLOCTEXT("TSAVPreVis", "LEDCurveA", "INTERNAL CURVE A ANGLES (CSV)"), JoinFloatValues(ContextLEDWall->ColumnInternalCurveAngleADegrees));
		AddLEDField(LEDCurveAnglesB, NSLOCTEXT("TSAVPreVis", "LEDCurveB", "INTERNAL CURVE B ANGLES (CSV)"), JoinFloatValues(ContextLEDWall->ColumnInternalCurveAngleBDegrees));
		AddLEDField(LEDFlatRows, NSLOCTEXT("TSAVPreVis", "LEDFlatRows", "ROWS IGNORING COLUMN CURVES (1-BASED CSV)"), JoinEnabledIndices(ContextLEDWall->RowIgnoreInternalColumnCurves));
		AddLEDField(LEDEmissiveStrength, NSLOCTEXT("TSAVPreVis", "LEDEmissive", "VIDEO BRIGHTNESS"), FString::SanitizeFloat(ContextLEDWall->EmissiveStrength));

		LEDLinkPatternCombo = WidgetTree->ConstructWidget<UComboBoxString>();
		LEDLinkPatternCombo->AddOption(TEXT("Rows: Left to Right"));
		LEDLinkPatternCombo->AddOption(TEXT("Rows: Serpentine"));
		LEDLinkPatternCombo->SetSelectedIndex(static_cast<int32>(ContextLEDWall->LinkPattern));
		ContextToolPanel->AddChildToVerticalBox(CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDLinkPattern", "SIGNAL LINK PATTERN"), 10, MutedTextColor));
		ContextToolPanel->AddChildToVerticalBox(LEDLinkPatternCombo);

		LEDSubpixelCombo = WidgetTree->ConstructWidget<UComboBoxString>();
		LEDSubpixelCombo->AddOption(TEXT("Off (Solid Video)"));
		LEDSubpixelCombo->AddOption(TEXT("Rectangle RGB"));
		LEDSubpixelCombo->AddOption(TEXT("Round RGB"));
		LEDSubpixelCombo->SetSelectedIndex(static_cast<int32>(ContextLEDWall->SubpixelLayout));
		ContextToolPanel->AddChildToVerticalBox(CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDSubpixelLayout", "SUBPIXEL LAYOUT"), 10, MutedTextColor));
		ContextToolPanel->AddChildToVerticalBox(LEDSubpixelCombo);

		UButton* ApplyConfiguration = CreateModeButton(*WidgetTree, NSLOCTEXT("TSAVPreVis", "ApplyLEDConfiguration", "APPLY / REBUILD LED WALL"));
		ApplyConfiguration->OnClicked.AddDynamic(this, &UTSAVMainWidget::LEDWallApplyConfigurationClicked);
		ContextToolPanel->AddChildToVerticalBox(ApplyConfiguration);
		UButton* ToggleSeams = CreateModeButton(*WidgetTree, ContextLEDWall->bShowPanelSeams
			? NSLOCTEXT("TSAVPreVis", "HideLEDSeams", "Hide Cabinet Seams") : NSLOCTEXT("TSAVPreVis", "ShowLEDSeams", "Show Cabinet Seams"));
		ToggleSeams->OnClicked.AddDynamic(this, &UTSAVMainWidget::LEDWallToggleSeamsClicked);
		ContextToolPanel->AddChildToVerticalBox(ToggleSeams);

		ContextToolPanel->AddChildToVerticalBox(CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDPanelShapeHeader", "INDIVIDUAL CABINET SHAPE"), 12, AccentColor));
		LEDPanelColumnField = CreateEditField(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDPanelColumnHint", "1-based column"));
		LEDPanelRowField = CreateEditField(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LEDPanelRowHint", "1-based row"));
		AddLabeledField(*WidgetTree, *ContextToolPanel, NSLOCTEXT("TSAVPreVis", "LEDPanelColumn", "COLUMN"), *LEDPanelColumnField);
		AddLabeledField(*WidgetTree, *ContextToolPanel, NSLOCTEXT("TSAVPreVis", "LEDPanelRow", "ROW"), *LEDPanelRowField);
		LEDPanelStyleCombo = WidgetTree->ConstructWidget<UComboBoxString>();
		if (const UEnum* EdgeEnum = StaticEnum<ETSAVLEDPanelEdgeStyle>())
		{
			for (int32 Index = 0; Index < EdgeEnum->NumEnums() - 1; ++Index) { LEDPanelStyleCombo->AddOption(EdgeEnum->GetDisplayNameTextByIndex(Index).ToString()); }
		}
		LEDPanelStyleCombo->SetSelectedIndex(0);
		ContextToolPanel->AddChildToVerticalBox(LEDPanelStyleCombo);
		UButton* ApplyPanelStyle = CreateModeButton(*WidgetTree, NSLOCTEXT("TSAVPreVis", "ApplyLEDPanelStyle", "Apply Cabinet Shape"));
		ApplyPanelStyle->OnClicked.AddDynamic(this, &UTSAVMainWidget::LEDWallApplyPanelStyleClicked);
		ContextToolPanel->AddChildToVerticalBox(ApplyPanelStyle);
	}

	if (ContextMediaSurface)
	{
		ContextToolPanel->AddChildToVerticalBox(CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "VideoRouteHeader", "VIDEO ROUTING"), 13, AccentColor));
		ATSAVVideoSwitcher* Switcher = ContextMediaSurface->GetVideoSwitcher();
		if (!Switcher && GetWorld())
		{
			for (TActorIterator<ATSAVVideoSwitcher> It(GetWorld()); It; ++It)
			{
				Switcher = *It;
				break;
			}
		}
		ContextSwitcher = Switcher;
		const FText RouteText = ContextMediaSurface->bUseVideoSwitcher
			? FText::Format(NSLOCTEXT("TSAVPreVis", "CurrentVideoRoute", "Following {0}"), FText::FromName(ContextMediaSurface->VideoBusName))
			: NSLOCTEXT("TSAVPreVis", "DirectVideoRoute", "Direct Media Source");
		ContextToolPanel->AddChildToVerticalBox(CreateText(*WidgetTree, RouteText, 11, PrimaryTextColor));
		if (!ContextSwitcher)
		{
			ContextToolPanel->AddChildToVerticalBox(CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "NoSwitcher", "Add a Video Switcher to enable live bus routing."), 10, MutedTextColor));
			return;
		}
		const TCHAR* Labels[] = { TEXT("Program"), TEXT("Preview"), TEXT("Aux 1"), TEXT("Aux 2") };
		const FName Handlers[] = {
			GET_FUNCTION_NAME_CHECKED(UTSAVMainWidget, SurfaceRouteProgramClicked), GET_FUNCTION_NAME_CHECKED(UTSAVMainWidget, SurfaceRoutePreviewClicked),
			GET_FUNCTION_NAME_CHECKED(UTSAVMainWidget, SurfaceRouteAux1Clicked), GET_FUNCTION_NAME_CHECKED(UTSAVMainWidget, SurfaceRouteAux2Clicked) };
		for (int32 Index = 0; Index < 4; ++Index)
		{
			UButton* Button = CreateModeButton(*WidgetTree, FText::FromString(Labels[Index]));
			FScriptDelegate Delegate;
			Delegate.BindUFunction(this, Handlers[Index]);
			Button->OnClicked.Add(Delegate);
			if (UVerticalBoxSlot* RouteSlot = ContextToolPanel->AddChildToVerticalBox(Button))
			{
				RouteSlot->SetPadding(FMargin(0.0f, 1.0f));
			}
		}
		UButton* DirectButton = CreateModeButton(*WidgetTree, NSLOCTEXT("TSAVPreVis", "DirectMediaButton", "Use Direct Media Source"));
		DirectButton->OnClicked.AddDynamic(this, &UTSAVMainWidget::SurfaceRouteDirectClicked);
		ContextToolPanel->AddChildToVerticalBox(DirectButton);
		return;
	}

	if (ContextCamera)
	{
		ContextToolPanel->AddChildToVerticalBox(CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "CameraToolHeader", "CAMERA CONFIGURATION"), 13, AccentColor));
		ContextToolPanel->AddChildToVerticalBox(CreateText(*WidgetTree, ContextCamera->CameraLabel, 11, PrimaryTextColor));

		CameraTypeCombo = WidgetTree->ConstructWidget<UComboBoxString>();
		if (const UEnum* Enum = StaticEnum<ETSAVCameraType>())
		{
			for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index) { CameraTypeCombo->AddOption(Enum->GetDisplayNameTextByIndex(Index).ToString()); }
		}
		CameraTypeCombo->SetSelectedIndex(static_cast<int32>(ContextCamera->CameraType));
		CameraTypeCombo->OnSelectionChanged.AddDynamic(this, &UTSAVMainWidget::CameraTypeChanged);
		ContextToolPanel->AddChildToVerticalBox(CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "CameraTypeLabel", "CAMERA TYPE"), 10, MutedTextColor));
		ContextToolPanel->AddChildToVerticalBox(CameraTypeCombo);

		CameraLensCombo = WidgetTree->ConstructWidget<UComboBoxString>();
		if (const UEnum* Enum = StaticEnum<ETSAVLensPreset>())
		{
			for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index) { CameraLensCombo->AddOption(Enum->GetDisplayNameTextByIndex(Index).ToString()); }
		}
		CameraLensCombo->SetSelectedIndex(static_cast<int32>(ContextCamera->LensPreset));
		CameraLensCombo->OnSelectionChanged.AddDynamic(this, &UTSAVMainWidget::CameraLensChanged);
		ContextToolPanel->AddChildToVerticalBox(CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "LensPresetLabel", "LENS PRESET"), 10, MutedTextColor));
		ContextToolPanel->AddChildToVerticalBox(CameraLensCombo);

		CameraFocalLengthField = CreateEditField(*WidgetTree, FText::GetEmpty());
		CameraFocalLengthField->SetText(FText::AsNumber(ContextCamera->FocalLengthMm));
		CameraFocalLengthField->OnTextCommitted.AddDynamic(this, &UTSAVMainWidget::CameraFocalLengthCommitted);
		AddLabeledField(*WidgetTree, *ContextToolPanel, NSLOCTEXT("TSAVPreVis", "FocalLengthLabel", "FOCAL LENGTH (MM)"), *CameraFocalLengthField);
		CameraApertureField = CreateEditField(*WidgetTree, FText::GetEmpty());
		CameraApertureField->SetText(FText::AsNumber(ContextCamera->Aperture));
		CameraApertureField->OnTextCommitted.AddDynamic(this, &UTSAVMainWidget::CameraApertureCommitted);
		AddLabeledField(*WidgetTree, *ContextToolPanel, NSLOCTEXT("TSAVPreVis", "ApertureLabel", "APERTURE (F-STOP)"), *CameraApertureField);
		CameraFocusField = CreateEditField(*WidgetTree, FText::GetEmpty());
		CameraFocusField->SetText(FText::AsNumber(ContextCamera->FocusDistanceCm));
		CameraFocusField->OnTextCommitted.AddDynamic(this, &UTSAVMainWidget::CameraFocusCommitted);
		AddLabeledField(*WidgetTree, *ContextToolPanel, NSLOCTEXT("TSAVPreVis", "FocusLabel", "FOCUS DISTANCE (CM)"), *CameraFocusField);

		ContextToolPanel->AddChildToVerticalBox(CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "PtzHeader", "PTZ / VISCA OVER IP"), 12, AccentColor));
		CameraViscaIpField = CreateEditField(*WidgetTree, FText::GetEmpty());
		CameraViscaIpField->SetText(FText::FromString(ContextCamera->ViscaIpAddress));
		CameraViscaIpField->OnTextCommitted.AddDynamic(this, &UTSAVMainWidget::CameraViscaIpCommitted);
		AddLabeledField(*WidgetTree, *ContextToolPanel, NSLOCTEXT("TSAVPreVis", "ViscaIpLabel", "CAMERA IP"), *CameraViscaIpField);
		CameraViscaPortField = CreateEditField(*WidgetTree, FText::GetEmpty());
		CameraViscaPortField->SetText(FText::AsNumber(ContextCamera->ViscaPort));
		CameraViscaPortField->OnTextCommitted.AddDynamic(this, &UTSAVMainWidget::CameraViscaPortCommitted);
		AddLabeledField(*WidgetTree, *ContextToolPanel, NSLOCTEXT("TSAVPreVis", "ViscaPortLabel", "VISCA UDP PORT"), *CameraViscaPortField);
		CameraPanField = CreateEditField(*WidgetTree, FText::GetEmpty()); CameraPanField->SetText(FText::AsNumber(ContextCamera->PanDegrees));
		CameraTiltField = CreateEditField(*WidgetTree, FText::GetEmpty()); CameraTiltField->SetText(FText::AsNumber(ContextCamera->TiltDegrees));
		CameraZoomField = CreateEditField(*WidgetTree, FText::GetEmpty()); CameraZoomField->SetText(FText::AsNumber(ContextCamera->ZoomNormalized));
		AddLabeledField(*WidgetTree, *ContextToolPanel, NSLOCTEXT("TSAVPreVis", "PanLabel", "PAN (-170 TO 170)"), *CameraPanField);
		AddLabeledField(*WidgetTree, *ContextToolPanel, NSLOCTEXT("TSAVPreVis", "TiltLabel", "TILT (-30 TO 90)"), *CameraTiltField);
		AddLabeledField(*WidgetTree, *ContextToolPanel, NSLOCTEXT("TSAVPreVis", "ZoomLabel", "ZOOM (0 TO 1)"), *CameraZoomField);

		UButton* EnableVisca = CreateModeButton(*WidgetTree, ContextCamera->bEnableViscaOverIp
			? NSLOCTEXT("TSAVPreVis", "DisableVisca", "Disable VISCA Output") : NSLOCTEXT("TSAVPreVis", "EnableVisca", "Enable VISCA Output"));
		EnableVisca->OnClicked.AddDynamic(this, &UTSAVMainWidget::CameraToggleViscaClicked);
		ContextToolPanel->AddChildToVerticalBox(EnableVisca);
		UButton* ApplyPtz = CreateModeButton(*WidgetTree, NSLOCTEXT("TSAVPreVis", "ApplyPtz", "Apply / Send PTZ"));
		ApplyPtz->OnClicked.AddDynamic(this, &UTSAVMainWidget::CameraApplyPtzClicked);
		ContextToolPanel->AddChildToVerticalBox(ApplyPtz);
		UHorizontalBox* PtzActions = WidgetTree->ConstructWidget<UHorizontalBox>();
		ContextToolPanel->AddChildToVerticalBox(PtzActions);
		UButton* Home = CreateModeButton(*WidgetTree, NSLOCTEXT("TSAVPreVis", "ViscaHome", "HOME"));
		Home->OnClicked.AddDynamic(this, &UTSAVMainWidget::CameraViscaHomeClicked);
		PtzActions->AddChildToHorizontalBox(Home);
		UButton* Stop = CreateModeButton(*WidgetTree, NSLOCTEXT("TSAVPreVis", "ViscaStop", "STOP"));
		Stop->OnClicked.AddDynamic(this, &UTSAVMainWidget::CameraViscaStopClicked);
		PtzActions->AddChildToHorizontalBox(Stop);
		UButton* ViewThrough = CreateModeButton(*WidgetTree, NSLOCTEXT("TSAVPreVis", "ViewThroughCamera", "View Through Camera"));
		ViewThrough->OnClicked.AddDynamic(this, &UTSAVMainWidget::CameraViewThroughClicked);
		ContextToolPanel->AddChildToVerticalBox(ViewThrough);
		UButton* ReturnEditor = CreateModeButton(*WidgetTree, NSLOCTEXT("TSAVPreVis", "ReturnEditorCamera", "Return to Editor Camera"));
		ReturnEditor->OnClicked.AddDynamic(this, &UTSAVMainWidget::CameraReturnToEditorClicked);
		ContextToolPanel->AddChildToVerticalBox(ReturnEditor);
	}
}

void UTSAVMainWidget::HandleSwitcherRouteClicked(ATSAVVideoSwitcher* Switcher, const FGuid InputId, const FName BusName)
{
	if (!Switcher) { return; }
	const FString Before = Switcher->CaptureTSAVState();
	Switcher->SetBusInput(BusName, InputId);
	CommitContextState(Switcher, Before, FText::Format(NSLOCTEXT("TSAVPreVis", "RouteBusCommand", "Route {0}"), FText::FromName(BusName)));
	BuildContextTools(Switcher);
}

void UTSAVMainWidget::SwitcherDiscoverClicked()
{
	if (!ContextSwitcher) { return; }
	const FString Before = ContextSwitcher->CaptureTSAVState();
	ContextSwitcher->DiscoverSources();
	CommitContextState(ContextSwitcher, Before, NSLOCTEXT("TSAVPreVis", "DiscoverSourcesCommand", "Discover Video Sources"));
	BuildContextTools(ContextSwitcher);
}

void UTSAVMainWidget::SwitcherCutClicked()
{
	if (!ContextSwitcher) { return; }
	const FString Before = ContextSwitcher->CaptureTSAVState();
	ContextSwitcher->Cut();
	CommitContextState(ContextSwitcher, Before, NSLOCTEXT("TSAVPreVis", "SwitcherCutCommand", "Switcher Cut"));
	BuildContextTools(ContextSwitcher);
}

void UTSAVMainWidget::SwitcherAutoClicked()
{
	if (!ContextSwitcher) { return; }
	const FString Before = ContextSwitcher->CaptureTSAVState();
	ContextSwitcher->AutoTransition();
	CommitContextState(ContextSwitcher, Before, NSLOCTEXT("TSAVPreVis", "SwitcherAutoCommand", "Switcher Auto"));
	BuildContextTools(ContextSwitcher);
}

void UTSAVMainWidget::SwitcherAddUrlClicked()
{
	if (!ContextSwitcher || !SwitcherInputUrlField) { return; }
	const FString Before = ContextSwitcher->CaptureTSAVState();
	ContextSwitcher->AddStreamInput(SwitcherInputNameField ? SwitcherInputNameField->GetText() : FText::GetEmpty(), SwitcherInputUrlField->GetText().ToString());
	CommitContextState(ContextSwitcher, Before, NSLOCTEXT("TSAVPreVis", "AddVideoInputCommand", "Add Video Input"));
	BuildContextTools(ContextSwitcher);
}

void UTSAVMainWidget::LEDWallApplyConfigurationClicked()
{
	using namespace TSAVMainWidget::Private;
	if (!ContextLEDWall || LEDWallFields.Num() != LEDFieldCount) { return; }
	const FString Before = ContextLEDWall->CaptureTSAVState();
	auto TextAt = [&](const int32 Index) { return LEDWallFields.IsValidIndex(Index) && LEDWallFields[Index] ? LEDWallFields[Index]->GetText().ToString() : FString(); };
	auto IntAt = [&](const int32 Index, const int32 Fallback) { const FString Text = TextAt(Index); return Text.IsEmpty() ? Fallback : FCString::Atoi(*Text); };
	auto FloatAt = [&](const int32 Index, const float Fallback) { const FString Text = TextAt(Index); return Text.IsEmpty() ? Fallback : FCString::Atof(*Text); };

	const int32 OldColumns = FMath::Clamp(ContextLEDWall->Columns, 1, 64);
	const int32 OldRows = FMath::Clamp(ContextLEDWall->Rows, 1, 64);
	const TArray<ETSAVLEDPanelEdgeStyle> OldStyles = ContextLEDWall->PanelEdgeStyles;
	ContextLEDWall->Columns = FMath::Clamp(IntAt(LEDColumns, ContextLEDWall->Columns), 1, 64);
	ContextLEDWall->Rows = FMath::Clamp(IntAt(LEDRows, ContextLEDWall->Rows), 1, 64);
	ContextLEDWall->bUsePanelDefinition = false;
	ContextLEDWall->PanelWidthCm = FMath::Max(FloatAt(LEDPanelWidth, ContextLEDWall->PanelWidthCm), 10.0f);
	ContextLEDWall->PanelHeightCm = FMath::Max(FloatAt(LEDPanelHeight, ContextLEDWall->PanelHeightCm), 10.0f);
	ContextLEDWall->WallDepthCm = FMath::Max(FloatAt(LEDPanelDepth, ContextLEDWall->WallDepthCm), 1.0f);
	ContextLEDWall->PanelResolutionX = FMath::Max(IntAt(LEDPanelResolutionX, ContextLEDWall->PanelResolutionX), 1);
	ContextLEDWall->PanelResolutionY = FMath::Max(IntAt(LEDPanelResolutionY, ContextLEDWall->PanelResolutionY), 1);
	ContextLEDWall->PanelGapCm = FMath::Max(FloatAt(LEDPanelGap, ContextLEDWall->PanelGapCm), 0.0f);
	ContextLEDWall->BorderCm = FMath::Max(FloatAt(LEDBorder, ContextLEDWall->BorderCm), 0.0f);
	ContextLEDWall->RoundEdgeRadiusMeters = FMath::Max(static_cast<double>(FloatAt(LEDRoundRadius, static_cast<float>(ContextLEDWall->RoundEdgeRadiusMeters))), 0.5);
	ContextLEDWall->ColumnSeamAnglesDegrees = ParseAngles(TextAt(LEDColumnSeams), FMath::Max(ContextLEDWall->Columns - 1, 0));
	ContextLEDWall->RowSeamAnglesDegrees = ParseAngles(TextAt(LEDRowSeams), FMath::Max(ContextLEDWall->Rows - 1, 0));
	ContextLEDWall->ColumnInternalCurveEnabled = ParseEnabledIndices(TextAt(LEDCurvedColumns), ContextLEDWall->Columns);
	ContextLEDWall->ColumnInternalCurveAngleADegrees = ParseAngles(TextAt(LEDCurveAnglesA), ContextLEDWall->Columns);
	ContextLEDWall->ColumnInternalCurveAngleBDegrees = ParseAngles(TextAt(LEDCurveAnglesB), ContextLEDWall->Columns);
	ContextLEDWall->RowIgnoreInternalColumnCurves = ParseEnabledIndices(TextAt(LEDFlatRows), ContextLEDWall->Rows);
	ContextLEDWall->EmissiveStrength = FMath::Max(FloatAt(LEDEmissiveStrength, ContextLEDWall->EmissiveStrength), 0.0f);
	ContextLEDWall->LinkPattern = LEDLinkPatternCombo && LEDLinkPatternCombo->GetSelectedIndex() == 0
		? ETSAVLEDLinkPattern::RowsLeftToRight : ETSAVLEDLinkPattern::RowsSerpentine;
	ContextLEDWall->SubpixelLayout = static_cast<ETSAVLEDSubpixelLayout>(FMath::Clamp(LEDSubpixelCombo ? LEDSubpixelCombo->GetSelectedIndex() : 0, 0, 2));

	ContextLEDWall->PanelEdgeStyles.Init(ETSAVLEDPanelEdgeStyle::Square, ContextLEDWall->Columns * ContextLEDWall->Rows);
	for (int32 Row = 0; Row < FMath::Min(OldRows, ContextLEDWall->Rows); ++Row)
	{
		for (int32 Column = 0; Column < FMath::Min(OldColumns, ContextLEDWall->Columns); ++Column)
		{
			const int32 OldIndex = Row * OldColumns + Column;
			const int32 NewIndex = Row * ContextLEDWall->Columns + Column;
			if (OldStyles.IsValidIndex(OldIndex)) { ContextLEDWall->PanelEdgeStyles[NewIndex] = OldStyles[OldIndex]; }
		}
	}

	ContextLEDWall->CanvasPosition.X = FMath::Max(IntAt(LEDCanvasX, ContextLEDWall->CanvasPosition.X), 0);
	ContextLEDWall->CanvasPosition.Y = FMath::Max(IntAt(LEDCanvasY, ContextLEDWall->CanvasPosition.Y), 0);
	const int32 MinimumCanvasWidth = ContextLEDWall->CanvasPosition.X + ContextLEDWall->Columns * ContextLEDWall->PanelResolutionX;
	const int32 MinimumCanvasHeight = ContextLEDWall->CanvasPosition.Y + ContextLEDWall->Rows * ContextLEDWall->PanelResolutionY;
	ContextLEDWall->CanvasResolution.X = FMath::Max(IntAt(LEDCanvasWidth, ContextLEDWall->CanvasResolution.X), MinimumCanvasWidth);
	ContextLEDWall->CanvasResolution.Y = FMath::Max(IntAt(LEDCanvasHeight, ContextLEDWall->CanvasResolution.Y), MinimumCanvasHeight);
	ContextLEDWall->bUseCanvasMapping = true;
	ContextLEDWall->RebuildPanelLayout();
	ContextLEDWall->RefreshMedia();
	CommitContextState(ContextLEDWall, Before, NSLOCTEXT("TSAVPreVis", "ConfigureLEDWallCommand", "Configure LED Wall"));
	BuildContextTools(ContextLEDWall);
}

void UTSAVMainWidget::LEDWallToggleSeamsClicked()
{
	if (!ContextLEDWall) { return; }
	const FString Before = ContextLEDWall->CaptureTSAVState();
	ContextLEDWall->bShowPanelSeams = !ContextLEDWall->bShowPanelSeams;
	ContextLEDWall->RebuildPanelLayout();
	CommitContextState(ContextLEDWall, Before, NSLOCTEXT("TSAVPreVis", "ToggleLEDSeamsCommand", "Toggle LED Cabinet Seams"));
	BuildContextTools(ContextLEDWall);
}

void UTSAVMainWidget::LEDWallApplyPanelStyleClicked()
{
	if (!ContextLEDWall || !LEDPanelColumnField || !LEDPanelRowField || !LEDPanelStyleCombo) { return; }
	const int32 Column = FCString::Atoi(*LEDPanelColumnField->GetText().ToString()) - 1;
	const int32 Row = FCString::Atoi(*LEDPanelRowField->GetText().ToString()) - 1;
	const int32 StyleIndex = LEDPanelStyleCombo->GetSelectedIndex();
	const int32 PanelIndex = Row * ContextLEDWall->Columns + Column;
	if (Column < 0 || Column >= ContextLEDWall->Columns || Row < 0 || Row >= ContextLEDWall->Rows ||
		!ContextLEDWall->PanelEdgeStyles.IsValidIndex(PanelIndex) || StyleIndex < 0) { return; }
	const FString Before = ContextLEDWall->CaptureTSAVState();
	ContextLEDWall->PanelEdgeStyles[PanelIndex] = static_cast<ETSAVLEDPanelEdgeStyle>(FMath::Clamp(StyleIndex, 0, static_cast<int32>(ETSAVLEDPanelEdgeStyle::Disabled)));
	ContextLEDWall->RebuildPanelLayout();
	CommitContextState(ContextLEDWall, Before, NSLOCTEXT("TSAVPreVis", "ShapeLEDCabinetCommand", "Shape LED Cabinet"));
	BuildContextTools(ContextLEDWall);
}

void UTSAVMainWidget::RouteContextSurface(const FName BusName)
{
	if (!ContextMediaSurface || !ContextSwitcher) { return; }
	const FString Before = ContextMediaSurface->CaptureTSAVState();
	ContextMediaSurface->SetVideoRoute(ContextSwitcher, BusName);
	CommitContextState(ContextMediaSurface, Before, NSLOCTEXT("TSAVPreVis", "SetVideoRouteCommand", "Set Video Route"));
	BuildContextTools(ContextMediaSurface);
}

void UTSAVMainWidget::SurfaceRouteProgramClicked() { RouteContextSurface(TEXT("Program")); }
void UTSAVMainWidget::SurfaceRoutePreviewClicked() { RouteContextSurface(TEXT("Preview")); }
void UTSAVMainWidget::SurfaceRouteAux1Clicked() { RouteContextSurface(TEXT("Aux 1")); }
void UTSAVMainWidget::SurfaceRouteAux2Clicked() { RouteContextSurface(TEXT("Aux 2")); }

void UTSAVMainWidget::SurfaceRouteDirectClicked()
{
	if (!ContextMediaSurface) { return; }
	const FString Before = ContextMediaSurface->CaptureTSAVState();
	ContextMediaSurface->ClearVideoRoute();
	CommitContextState(ContextMediaSurface, Before, NSLOCTEXT("TSAVPreVis", "DirectVideoCommand", "Use Direct Video Source"));
	BuildContextTools(ContextMediaSurface);
}

void UTSAVMainWidget::CameraTypeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!ContextCamera || !CameraTypeCombo || SelectionType == ESelectInfo::Direct) { return; }
	const FString Before = ContextCamera->CaptureTSAVState();
	ContextCamera->SetCameraType(static_cast<ETSAVCameraType>(CameraTypeCombo->FindOptionIndex(SelectedItem)));
	CommitContextState(ContextCamera, Before, NSLOCTEXT("TSAVPreVis", "CameraTypeCommand", "Change Camera Type"));
	BuildContextTools(ContextCamera);
}

void UTSAVMainWidget::CameraLensChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!ContextCamera || !CameraLensCombo || SelectionType == ESelectInfo::Direct) { return; }
	const FString Before = ContextCamera->CaptureTSAVState();
	ContextCamera->SetLensPreset(static_cast<ETSAVLensPreset>(CameraLensCombo->FindOptionIndex(SelectedItem)));
	CommitContextState(ContextCamera, Before, NSLOCTEXT("TSAVPreVis", "CameraLensCommand", "Change Camera Lens"));
	BuildContextTools(ContextCamera);
}

void UTSAVMainWidget::CameraFocalLengthCommitted(const FText& Text, ETextCommit::Type)
{
	if (!ContextCamera) { return; }
	const FString Before = ContextCamera->CaptureTSAVState();
	ContextCamera->SetLens(FCString::Atof(*Text.ToString()), ContextCamera->Aperture, ContextCamera->FocusDistanceCm);
	CommitContextState(ContextCamera, Before, NSLOCTEXT("TSAVPreVis", "FocalLengthCommand", "Change Focal Length"));
}

void UTSAVMainWidget::CameraApertureCommitted(const FText& Text, ETextCommit::Type)
{
	if (!ContextCamera) { return; }
	const FString Before = ContextCamera->CaptureTSAVState();
	ContextCamera->SetLens(ContextCamera->FocalLengthMm, FCString::Atof(*Text.ToString()), ContextCamera->FocusDistanceCm);
	CommitContextState(ContextCamera, Before, NSLOCTEXT("TSAVPreVis", "ApertureCommand", "Change Aperture"));
}

void UTSAVMainWidget::CameraFocusCommitted(const FText& Text, ETextCommit::Type)
{
	if (!ContextCamera) { return; }
	const FString Before = ContextCamera->CaptureTSAVState();
	ContextCamera->SetLens(ContextCamera->FocalLengthMm, ContextCamera->Aperture, FCString::Atof(*Text.ToString()));
	CommitContextState(ContextCamera, Before, NSLOCTEXT("TSAVPreVis", "FocusCommand", "Change Focus Distance"));
}

void UTSAVMainWidget::CameraViscaIpCommitted(const FText& Text, ETextCommit::Type)
{
	if (!ContextCamera) { return; }
	const FString Before = ContextCamera->CaptureTSAVState();
	ContextCamera->ViscaIpAddress = Text.ToString();
	CommitContextState(ContextCamera, Before, NSLOCTEXT("TSAVPreVis", "ViscaIpCommand", "Change VISCA Address"));
}

void UTSAVMainWidget::CameraViscaPortCommitted(const FText& Text, ETextCommit::Type)
{
	if (!ContextCamera) { return; }
	const FString Before = ContextCamera->CaptureTSAVState();
	ContextCamera->ViscaPort = FMath::Clamp(FCString::Atoi(*Text.ToString()), 1, 65535);
	CommitContextState(ContextCamera, Before, NSLOCTEXT("TSAVPreVis", "ViscaPortCommand", "Change VISCA Port"));
}

void UTSAVMainWidget::CameraToggleViscaClicked()
{
	if (!ContextCamera) { return; }
	const FString Before = ContextCamera->CaptureTSAVState();
	ContextCamera->bEnableViscaOverIp = !ContextCamera->bEnableViscaOverIp;
	CommitContextState(ContextCamera, Before, NSLOCTEXT("TSAVPreVis", "ToggleViscaCommand", "Toggle VISCA Output"));
	BuildContextTools(ContextCamera);
}

void UTSAVMainWidget::CameraApplyPtzClicked()
{
	if (!ContextCamera) { return; }
	const FString Before = ContextCamera->CaptureTSAVState();
	if (CameraViscaIpField) { ContextCamera->ViscaIpAddress = CameraViscaIpField->GetText().ToString(); }
	if (CameraViscaPortField) { ContextCamera->ViscaPort = FMath::Clamp(FCString::Atoi(*CameraViscaPortField->GetText().ToString()), 1, 65535); }
	const float Pan = CameraPanField ? FCString::Atof(*CameraPanField->GetText().ToString()) : ContextCamera->PanDegrees;
	const float Tilt = CameraTiltField ? FCString::Atof(*CameraTiltField->GetText().ToString()) : ContextCamera->TiltDegrees;
	const float Zoom = CameraZoomField ? FCString::Atof(*CameraZoomField->GetText().ToString()) : ContextCamera->ZoomNormalized;
	ContextCamera->ApplyPTZ(Pan, Tilt, Zoom, true);
	CommitContextState(ContextCamera, Before, NSLOCTEXT("TSAVPreVis", "ApplyPtzCommand", "Apply PTZ"));
	BuildContextTools(ContextCamera);
}

void UTSAVMainWidget::CameraViscaHomeClicked() { if (ContextCamera) { ContextCamera->SendViscaHome(); } }
void UTSAVMainWidget::CameraViscaStopClicked() { if (ContextCamera) { ContextCamera->SendViscaStop(); } }
void UTSAVMainWidget::CameraViewThroughClicked() { if (ContextCamera) { if (ATSAVPlayerController* Controller = Cast<ATSAVPlayerController>(GetOwningPlayer())) { Controller->ViewThroughCamera(ContextCamera); } } }
void UTSAVMainWidget::CameraReturnToEditorClicked() { if (ATSAVPlayerController* Controller = Cast<ATSAVPlayerController>(GetOwningPlayer())) { Controller->ReturnToEditorCamera(); } }

FText UTSAVMainWidget::GetModeText(const ETSAVAppMode Mode)
{
	if (const UEnum* ModeEnum = StaticEnum<ETSAVAppMode>())
	{
		return ModeEnum->GetDisplayNameTextByValue(static_cast<int64>(Mode));
	}
	return NSLOCTEXT("TSAVPreVis", "UnknownMode", "Unknown");
}

void UTSAVMainWidget::HandleSelectionChanged(AActor* SelectedActor)
{
	UpdateInspector(SelectedActor);
}

void UTSAVMainWidget::HandleModeChanged(const ETSAVAppMode NewMode, const ETSAVAppMode PreviousMode)
{
	const FText ModeText = GetModeText(NewMode);
	if (ModeStatusText)
	{
		ModeStatusText->SetText(FText::Format(NSLOCTEXT("TSAVPreVis", "ModeStatusFormat", "{0} MODE"), ModeText.ToUpper()));
	}
	if (InspectorTitleText)
	{
		InspectorTitleText->SetText(FText::Format(NSLOCTEXT("TSAVPreVis", "ModeToolsFormat", "{0} / INSPECTOR"), ModeText.ToUpper()));
	}
}

void UTSAVMainWidget::HandleCommandObjectChanged(AActor* Actor)
{
	RefreshOutliner();
	UpdateInspector(TSAVMainWidget::Private::GetSelectedActor(*this));
}

void UTSAVMainWidget::HandleCommandHistoryChanged()
{
	if (!UndoStatusText || !GetGameInstance())
	{
		return;
	}
	const UTSAVCommandSubsystem* Commands = GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>();
	UndoStatusText->SetText(FText::Format(
		NSLOCTEXT("TSAVPreVis", "UndoStatus", "UNDO: {0}   |   REDO: {1}"),
		Commands->CanUndo() ? Commands->GetUndoDescription() : NSLOCTEXT("TSAVPreVis", "None", "None"),
		Commands->CanRedo() ? Commands->GetRedoDescription() : NSLOCTEXT("TSAVPreVis", "None", "None")));
	BuildContextTools(TSAVMainWidget::Private::GetSelectedActor(*this));
}

void UTSAVMainWidget::HandleProjectChanged()
{
	RefreshOutliner();
	RefreshProjectStatus();
	UpdateInspector(TSAVMainWidget::Private::GetSelectedActor(*this));
}

void UTSAVMainWidget::HandleOutlinerActorClicked(AActor* Actor)
{
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		LocalPlayer->GetSubsystem<UTSAVSelectionSubsystem>()->SelectActorFromOutliner(Actor);
	}
}

void UTSAVMainWidget::RefreshOutliner()
{
	if (!OutlinerEntries || !GetWorld())
	{
		return;
	}
	OutlinerEntries->ClearChildren();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const UTSAVSceneObjectComponent* SceneObject = It->FindComponentByClass<UTSAVSceneObjectComponent>();
		if (!SceneObject)
		{
			continue;
		}
		UTSAVOutlinerButton* Button = WidgetTree->ConstructWidget<UTSAVOutlinerButton>();
		Button->SetBackgroundColor(FLinearColor(0.055f, 0.065f, 0.085f, 1.0f));
		Button->SetContent(TSAVMainWidget::Private::CreateText(
			*WidgetTree,
			SceneObject->DisplayName.IsEmpty() ? FText::FromString(It->GetName()) : SceneObject->DisplayName,
			11,
			SceneObject->bLocked ? TSAVMainWidget::Private::MutedTextColor : TSAVMainWidget::Private::PrimaryTextColor));
		Button->InitializeForActor(*It);
		Button->OnActorClicked.AddUniqueDynamic(this, &UTSAVMainWidget::HandleOutlinerActorClicked);
		if (UVerticalBoxSlot* EntrySlot = OutlinerEntries->AddChildToVerticalBox(Button))
		{
			EntrySlot->SetPadding(FMargin(0.0f, 1.0f));
		}
	}
}

void UTSAVMainWidget::RefreshProjectStatus()
{
	if (!ProjectStatusText || !GetGameInstance())
	{
		return;
	}
	const UTSAVProjectSubsystem* Project = GetGameInstance()->GetSubsystem<UTSAVProjectSubsystem>();
	ProjectStatusText->SetText(FText::FromString(FString::Printf(
		TEXT("DMX   |   NDI   |   PROJECT: %s%s   |   %s"),
		*Project->GetProjectName(),
		Project->IsDirty() ? TEXT(" *") : TEXT(""),
		Project->GetCurrentProjectPath().IsEmpty() ? TEXT("Not saved") : *Project->GetCurrentProjectPath())));
	HandleCommandHistoryChanged();
}

void UTSAVMainWidget::NewProjectClicked()
{
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		LocalPlayer->GetSubsystem<UTSAVSelectionSubsystem>()->ClearSelection();
	}
	if (GetGameInstance())
	{
		GetGameInstance()->GetSubsystem<UTSAVProjectSubsystem>()->NewProject();
	}
}

void UTSAVMainWidget::SaveProjectClicked()
{
	if (GetGameInstance()) { GetGameInstance()->GetSubsystem<UTSAVProjectSubsystem>()->SaveProject(); }
}

void UTSAVMainWidget::LoadProjectClicked()
{
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer()) { LocalPlayer->GetSubsystem<UTSAVSelectionSubsystem>()->ClearSelection(); }
	if (GetGameInstance()) { GetGameInstance()->GetSubsystem<UTSAVProjectSubsystem>()->LoadProject(); }
}

void UTSAVMainWidget::AddCubeClicked()
{
	if (!GetGameInstance() || !GetWorld()) { return; }
	const FVector Location = GetOwningPlayer() && GetOwningPlayer()->PlayerCameraManager
		? GetOwningPlayer()->PlayerCameraManager->GetCameraLocation() + GetOwningPlayer()->PlayerCameraManager->GetCameraRotation().Vector() * 500.0f
		: FVector(0.0f, 0.0f, 50.0f);
	AActor* Actor = GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>()->SpawnSceneObject(
		GetWorld(), FTransform(FRotator::ZeroRotator, Location), NSLOCTEXT("TSAVPreVis", "NewSceneCube", "Scene Cube"));
	if (Actor && GetOwningLocalPlayer()) { GetOwningLocalPlayer()->GetSubsystem<UTSAVSelectionSubsystem>()->SelectActor(Actor); }
}

void UTSAVMainWidget::UndoClicked()
{
	if (GetGameInstance()) { GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>()->Undo(); }
}

void UTSAVMainWidget::RedoClicked()
{
	if (GetGameInstance()) { GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>()->Redo(); }
}

void UTSAVMainWidget::DeleteClicked()
{
	AActor* Actor = TSAVMainWidget::Private::GetSelectedActor(*this);
	if (Actor && GetGameInstance())
	{
		GetOwningLocalPlayer()->GetSubsystem<UTSAVSelectionSubsystem>()->ClearSelection();
		GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>()->DeleteActor(Actor);
	}
}

void UTSAVMainWidget::DuplicateClicked()
{
	if (!GetGameInstance() || !GetOwningLocalPlayer()) { return; }
	if (AActor* Duplicate = GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>()->DuplicateActor(TSAVMainWidget::Private::GetSelectedActor(*this)))
	{
		GetOwningLocalPlayer()->GetSubsystem<UTSAVSelectionSubsystem>()->SelectActor(Duplicate);
	}
}

void UTSAVMainWidget::ToggleLockedClicked()
{
	AActor* Actor = TSAVMainWidget::Private::GetSelectedActor(*this);
	const UTSAVSceneObjectComponent* SceneObject = Actor ? Actor->FindComponentByClass<UTSAVSceneObjectComponent>() : nullptr;
	if (SceneObject && GetGameInstance()) { GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>()->SetLocked(Actor, !SceneObject->bLocked); }
}

void UTSAVMainWidget::ToggleVisibleClicked()
{
	AActor* Actor = TSAVMainWidget::Private::GetSelectedActor(*this);
	const UTSAVSceneObjectComponent* SceneObject = Actor ? Actor->FindComponentByClass<UTSAVSceneObjectComponent>() : nullptr;
	if (SceneObject && GetGameInstance()) { GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>()->SetVisible(Actor, !SceneObject->bVisible); }
}

void UTSAVMainWidget::NameCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	if (GetGameInstance()) { GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>()->SetDisplayName(TSAVMainWidget::Private::GetSelectedActor(*this), Text); }
}

void UTSAVMainWidget::CommitTransformValue(const int32 GroupIndex, const int32 AxisIndex, const FText& Text)
{
	AActor* Actor = TSAVMainWidget::Private::GetSelectedActor(*this);
	if (!Actor || !GetGameInstance() || GroupIndex < 0 || GroupIndex > 2 || AxisIndex < 0 || AxisIndex > 2)
	{
		return;
	}
	const double Value = FCString::Atod(*Text.ToString());
	FTransform Transform = Actor->GetActorTransform();
	if (GroupIndex == 0)
	{
		FVector Location = Transform.GetLocation(); Location[AxisIndex] = Value; Transform.SetLocation(Location);
	}
	else if (GroupIndex == 1)
	{
		FRotator Rotation = Transform.Rotator();
		if (AxisIndex == 0) { Rotation.Pitch = Value; } else if (AxisIndex == 1) { Rotation.Yaw = Value; } else { Rotation.Roll = Value; }
		Transform.SetRotation(Rotation.Quaternion());
	}
	else
	{
		FVector Scale = Transform.GetScale3D(); Scale[AxisIndex] = FMath::Max(Value, 0.01); Transform.SetScale3D(Scale);
	}
	GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>()->SetActorTransform(Actor, Transform, NSLOCTEXT("TSAVPreVis", "InspectorTransformCommand", "Edit Transform"));
}

void UTSAVMainWidget::LocationXCommitted(const FText& Text, ETextCommit::Type) { CommitTransformValue(0, 0, Text); }
void UTSAVMainWidget::LocationYCommitted(const FText& Text, ETextCommit::Type) { CommitTransformValue(0, 1, Text); }
void UTSAVMainWidget::LocationZCommitted(const FText& Text, ETextCommit::Type) { CommitTransformValue(0, 2, Text); }
void UTSAVMainWidget::RotationPitchCommitted(const FText& Text, ETextCommit::Type) { CommitTransformValue(1, 0, Text); }
void UTSAVMainWidget::RotationYawCommitted(const FText& Text, ETextCommit::Type) { CommitTransformValue(1, 1, Text); }
void UTSAVMainWidget::RotationRollCommitted(const FText& Text, ETextCommit::Type) { CommitTransformValue(1, 2, Text); }
void UTSAVMainWidget::ScaleXCommitted(const FText& Text, ETextCommit::Type) { CommitTransformValue(2, 0, Text); }
void UTSAVMainWidget::ScaleYCommitted(const FText& Text, ETextCommit::Type) { CommitTransformValue(2, 1, Text); }
void UTSAVMainWidget::ScaleZCommitted(const FText& Text, ETextCommit::Type) { CommitTransformValue(2, 2, Text); }

void UTSAVMainWidget::SelectModeClicked() { SetAppMode(ETSAVAppMode::Select); }
void UTSAVMainWidget::VenueModeClicked() { SetAppMode(ETSAVAppMode::Venue); }
void UTSAVMainWidget::StageModeClicked() { SetAppMode(ETSAVAppMode::Stage); }
void UTSAVMainWidget::TrussModeClicked() { SetAppMode(ETSAVAppMode::Truss); }
void UTSAVMainWidget::LightingModeClicked() { SetAppMode(ETSAVAppMode::Lighting); }
void UTSAVMainWidget::LEDModeClicked() { SetAppMode(ETSAVAppMode::LED); }
void UTSAVMainWidget::CameraModeClicked() { SetAppMode(ETSAVAppMode::Camera); }
void UTSAVMainWidget::VideoModeClicked() { SetAppMode(ETSAVAppMode::Video); }
void UTSAVMainWidget::CharactersModeClicked() { SetAppMode(ETSAVAppMode::Characters); }
void UTSAVMainWidget::WalkthroughModeClicked() { SetAppMode(ETSAVAppMode::Walkthrough); }
