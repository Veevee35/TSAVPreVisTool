// Copyright TSAV. All Rights Reserved.

#include "UI/TSAVMainWidget.h"

#include "TSAVPrevisRuntime.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Actor.h"
#include "Interaction/TSAVModeSubsystem.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "Interaction/TSAVSelectionSubsystem.h"
#include "Project/TSAVProjectSubsystem.h"

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
	}
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

	UBorder* BottomBar = WidgetTree->ConstructWidget<UBorder>();
	BottomBar->SetBrushColor(ChromeColor);
	BottomBar->SetPadding(FMargin(12.0f, 5.0f));
	UHorizontalBox* BottomContent = WidgetTree->ConstructWidget<UHorizontalBox>();
	BottomBar->SetContent(BottomContent);
	AddCanvasPanel(*RootCanvas, *BottomBar, FAnchors(0.0f, 1.0f, 1.0f, 1.0f), FMargin(0.0f, -30.0f, 0.0f, 30.0f), FVector2D::ZeroVector, 10);

	ModeStatusText = CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "InitialStatus", "SELECT MODE"), 11, AccentColor);
	BottomContent->AddChildToHorizontalBox(ModeStatusText);
	UTextBlock* StatusItems = CreateText(*WidgetTree, NSLOCTEXT("TSAVPreVis", "StatusItems", "DMX   |   NDI   |   VIDEO ROUTER   |   PROJECT: UNTITLED"), 10, MutedTextColor);
	if (UHorizontalBoxSlot* StatusSlot = BottomContent->AddChildToHorizontalBox(StatusItems))
	{
		StatusSlot->SetPadding(FMargin(24.0f, 0.0f));
	}

	UTextBlock* ViewportHint = CreateText(
		*WidgetTree,
		NSLOCTEXT("TSAVPreVis", "ViewportHint", "RMB + Mouse  Look     WASD / Q / E  Fly     Mouse Wheel  Speed     LMB  Select"),
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
		TEXT("%s\n\nObject ID\n%s\n\nLocation\nX %.1f   Y %.1f   Z %.1f\n\nRotation\nP %.1f   Y %.1f   R %.1f\n\nScale\nX %.2f   Y %.2f   Z %.2f"),
		*DisplayName.ToString(),
		*ObjectId,
		Location.X,
		Location.Y,
		Location.Z,
		Rotation.Pitch,
		Rotation.Yaw,
		Rotation.Roll,
		Scale.X,
		Scale.Y,
		Scale.Z)));
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
