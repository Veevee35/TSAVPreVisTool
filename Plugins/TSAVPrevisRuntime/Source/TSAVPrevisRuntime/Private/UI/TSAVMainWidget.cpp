// Copyright TSAV. All Rights Reserved.

#include "UI/TSAVMainWidget.h"

#include "TSAVPrevisRuntime.h"
#include "Blueprint/WidgetTree.h"
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
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Interaction/TSAVCommandSubsystem.h"
#include "Interaction/TSAVModeSubsystem.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "Interaction/TSAVSelectionSubsystem.h"
#include "Project/TSAVProjectSubsystem.h"
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

	const TCHAR* MenuLabels[] = { TEXT("File"), TEXT("Edit"), TEXT("Build"), TEXT("LED"), TEXT("Lighting"), TEXT("Video"), TEXT("Camera"), TEXT("View") };
	for (const TCHAR* MenuLabel : MenuLabels)
	{
		UTextBlock* MenuText = CreateText(*WidgetTree, FText::FromString(MenuLabel), 12, PrimaryTextColor);
		if (UHorizontalBoxSlot* MenuSlot = TopBarContent->AddChildToHorizontalBox(MenuText))
		{
			MenuSlot->SetPadding(FMargin(9.0f, 0.0f));
			MenuSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

#define TSAV_ADD_TOP_ACTION(Label, Handler) \
	do \
	{ \
		UButton* Button = CreateModeButton(*WidgetTree, FText::FromString(TEXT(Label))); \
		Button->OnClicked.AddDynamic(this, &UTSAVMainWidget::Handler); \
		if (UHorizontalBoxSlot* ButtonSlot = TopBarContent->AddChildToHorizontalBox(Button)) \
		{ \
			ButtonSlot->SetPadding(FMargin(3.0f, 0.0f)); \
			ButtonSlot->SetVerticalAlignment(VAlign_Center); \
		} \
	} while (false)

	TSAV_ADD_TOP_ACTION("New", NewProjectClicked);
	TSAV_ADD_TOP_ACTION("Save", SaveProjectClicked);
	TSAV_ADD_TOP_ACTION("Load", LoadProjectClicked);
	TSAV_ADD_TOP_ACTION("Add Cube", AddCubeClicked);
	TSAV_ADD_TOP_ACTION("Undo", UndoClicked);
	TSAV_ADD_TOP_ACTION("Redo", RedoClicked);

#undef TSAV_ADD_TOP_ACTION

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
