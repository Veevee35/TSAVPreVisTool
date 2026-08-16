// Copyright TSAV. All Rights Reserved.

#include "UI/TSAVMainWidget.h"

#include "TSAVPrevisRuntime.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraActor.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
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
#include "TSAVDMXFixture.h"
#include "TSAVLEDPanel.h"
#include "TSAVLEDWall.h"
#include "UI/TSAVMenuButton.h"
#include "UI/TSAVOutlinerButton.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVMainWidget)

namespace TSAVMainWidget::Private
{
	const FLinearColor ChromeColor(0.025f, 0.032f, 0.045f, 0.96f);
	const FLinearColor PanelColor(0.045f, 0.055f, 0.075f, 0.94f);
	const FLinearColor AccentColor(0.04f, 0.62f, 0.86f, 1.0f);
	const FLinearColor PrimaryTextColor(0.88f, 0.92f, 0.96f, 1.0f);
	const FLinearColor MutedTextColor(0.52f, 0.62f, 0.72f, 1.0f);

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
	UVerticalBox* InspectorContent = WidgetTree->ConstructWidget<UVerticalBox>();
	RightPanel->SetContent(InspectorContent);
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
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuLEDWall", "Add LED Wall"), ETSAVMenuAction::AddLEDWall);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuLEDPanel", "Add LED Panel"), ETSAVMenuAction::AddLEDPanel);
		break;
	case ETSAVTopMenu::Lighting:
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuDMXFixture", "Add DMX Fixture"), ETSAVMenuAction::AddDMXFixture);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuPointLight", "Add Point Light"), ETSAVMenuAction::AddPointLight);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuSpotLight", "Add Spot Light"), ETSAVMenuAction::AddSpotLight);
		AddMenuEntry(NSLOCTEXT("TSAVPreVis", "MenuRectLight", "Add Rect Light"), ETSAVMenuAction::AddRectLight);
		break;
	case ETSAVTopMenu::Video:
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
		SpawnAndSelect(ACameraActor::StaticClass(), CameraTransform,
			NSLOCTEXT("TSAVPreVis", "NewCamera", "Camera"), ETSAVObjectType::Camera);
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
		OutlinerSelectionText->SetText(NSLOCTEXT("TSAVPreVis", "NothingSelected", "No object selected"));
		InspectorBodyText->SetText(NSLOCTEXT("TSAVPreVis", "InspectorEmpty", "Select a TSAV object in the viewport."));
		if (NameField) { NameField->SetText(FText::GetEmpty()); NameField->SetIsEnabled(false); }
		for (UEditableTextBox* Field : LocationFields) { if (Field) { Field->SetText(FText::GetEmpty()); Field->SetIsEnabled(false); } }
		for (UEditableTextBox* Field : RotationFields) { if (Field) { Field->SetText(FText::GetEmpty()); Field->SetIsEnabled(false); } }
		for (UEditableTextBox* Field : ScaleFields) { if (Field) { Field->SetText(FText::GetEmpty()); Field->SetIsEnabled(false); } }
		return;
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
