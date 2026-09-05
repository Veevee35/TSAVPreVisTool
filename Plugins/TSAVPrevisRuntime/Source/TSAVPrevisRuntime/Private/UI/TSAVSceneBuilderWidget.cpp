// Copyright TSAV. All Rights Reserved.
#include "UI/TSAVSceneBuilderWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "Interaction/TSAVCommandSubsystem.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

namespace TSAVSceneBuilder
{
	TSharedRef<SWidget> Label(const FString& Text) { return SNew(STextBlock).Text(FText::FromString(Text)).AutoWrapText(true); }
	FText KindName(ETSAVScenicKind Kind) { return StaticEnum<ETSAVScenicKind>()->GetDisplayNameTextByValue(static_cast<int64>(Kind)); }
	template<typename T> TSharedRef<SWidget> Field(const FString& Name,T* Value,T Min,T Max) {
		return SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[Label(Name)]
			+SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(150)[SNew(SNumericEntryBox<T>).MinValue(Min).MaxValue(Max).Value_Lambda([Value] { return *Value; }).OnValueChanged_Lambda([Value](T NewValue) { *Value=NewValue; })]];
	}
	class SBuilder final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBuilder) {} SLATE_ARGUMENT(UWorld*,World) SLATE_ARGUMENT(ETSAVScenicKind,InitialKind) SLATE_EVENT(FSimpleDelegate,OnClose) SLATE_END_ARGS()
		void Construct(const FArguments& Args) {
			World=Args._World; Settings=ATSAVSceneGenerator::Preset(Args._InitialKind);
			for (int32 I=0; I<=static_cast<int32>(ETSAVScenicKind::Rail); ++I) Kinds.Add(MakeShared<ETSAVScenicKind>(static_cast<ETSAVScenicKind>(I)));
			ChildSlot[SNew(SBorder).Padding(20)[SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[Label(TEXT("TSAV STAGE AND SCENIC BUILDER"))]
					+SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("Close"))).Visibility(Args._OnClose.IsBound()?EVisibility::Visible:EVisibility::Collapsed).OnClicked_Lambda([Close=Args._OnClose] { Close.ExecuteIfBound(); return FReply::Handled(); })]]
				+SVerticalBox::Slot().AutoHeight().Padding(0,10)[Label(TEXT("Original parametric geometry. Dimensions are centimeters. Choose a preset or load an existing generated object, then apply your settings."))]
				+SVerticalBox::Slot().FillHeight(1)[SNew(SHorizontalBox)
					+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,20,0)[SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
						+SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(SComboBox<TSharedPtr<ETSAVScenicKind>>).OptionsSource(&Kinds)
							.OnGenerateWidget_Lambda([](TSharedPtr<ETSAVScenicKind> Kind) { return SNew(STextBlock).Text(KindName(*Kind)); })
							.OnSelectionChanged_Lambda([this](TSharedPtr<ETSAVScenicKind> Kind,ESelectInfo::Type) { if (Kind) Settings=ATSAVSceneGenerator::Preset(*Kind); })[SNew(STextBlock).Text_Lambda([this] { return KindName(Settings.Kind); })]]
						+SVerticalBox::Slot().AutoHeight().Padding(0,4)[Field(TEXT("Width / ring diameter"),&Settings.Width,1.0f,100000.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,4)[Field(TEXT("Depth / truss width / pleat depth"),&Settings.Depth,1.0f,100000.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,4)[Field(TEXT("Height / lift travel"),&Settings.Height,1.0f,100000.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,4)[Field(TEXT("Deck thickness / tube diameter"),&Settings.Thickness,0.1f,1000.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,4)[Field(TEXT("Segments / steps / crowd columns"),&Settings.Columns,1,100)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,4)[Field(TEXT("Scaffold levels / crowd rows"),&Settings.Rows,1,100)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,4)[Field(TEXT("Lift / rail position (0–1)"),&Settings.Position,0.0f,1.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(SCheckBox).IsChecked_Lambda([this] { return Settings.bAnimate?ECheckBoxState::Checked:ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState State) { Settings.bAnimate=State==ECheckBoxState::Checked; })[Label(TEXT("Animate lift / rail"))]]
						+SVerticalBox::Slot().AutoHeight().Padding(0,4)[Field(TEXT("Motion cycle (seconds)"),&Settings.PeriodSeconds,0.1f,86400.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,12)[Label(TEXT("NEW OBJECT PLACEMENT"))]
						+SVerticalBox::Slot().AutoHeight()[SNew(SEditableTextBox).HintText(FText::FromString(TEXT("Optional new object name"))).OnTextChanged_Lambda([this](const FText& Value) { Name=Value.ToString(); })]
						+SVerticalBox::Slot().AutoHeight().Padding(0,4)[Field(TEXT("World X"),&X,-1000000.0f,1000000.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,4)[Field(TEXT("World Y"),&Y,-1000000.0f,1000000.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,4)[Field(TEXT("World Z"),&Z,-1000000.0f,1000000.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,4)[Field(TEXT("Yaw (degrees)"),&Yaw,-360.0f,360.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,4)[Field(TEXT("Placement grid (0 = no snapping)"),&Grid,0.0f,10000.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,10)[SNew(SButton).Text(FText::FromString(TEXT("Create object"))).OnClicked_Lambda([this] { Create(); return FReply::Handled(); })]
					]]
					+SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
						+SVerticalBox::Slot().AutoHeight()[SNew(SButton).Text(FText::FromString(TEXT("Refresh generated objects"))).OnClicked_Lambda([this] { Refresh(); return FReply::Handled(); })]
						+SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(Objects,SVerticalBox)]]
						+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this] { return FText::FromString(Current.IsValid()?TEXT("Editing: ")+Current->GetActorNameOrLabel():TEXT("Choose an object above to edit it.")); })]
						+SVerticalBox::Slot().AutoHeight().Padding(0,10)[SNew(SButton).Text(FText::FromString(TEXT("Apply settings to chosen object"))).OnClicked_Lambda([this] { Apply(); return FReply::Handled(); })]
					]]
				+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this] { return FText::FromString(Message); }).AutoWrapText(true)]
			]]; Refresh();
		}
	private:
		UTSAVCommandSubsystem* Commands() const { auto* GI=World.IsValid()?World->GetGameInstance():nullptr; return GI?GI->GetSubsystem<UTSAVCommandSubsystem>():nullptr; }
		void Refresh() {
			Objects->ClearChildren(); if (!World.IsValid()) return;
			for (TActorIterator<ATSAVSceneGenerator> It(World.Get()); It; ++It) {
				const TWeakObjectPtr<ATSAVSceneGenerator> Actor(*It);
				Objects->AddSlot().AutoHeight().Padding(0,4)[SNew(SButton).Text(FText::FromString(It->GetActorNameOrLabel()))
					.OnClicked_Lambda([this,Actor] { if (Actor.IsValid()) { Current=Actor; Settings=Actor->Settings; } return FReply::Handled(); })];
			}
		}
		void Create() {
			if (!World.IsValid() || !ATSAVSceneGenerator::Validate(Settings) || !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z) || !FMath::IsFinite(Yaw) || !FMath::IsFinite(Grid)) { Message=TEXT("Check the dimensions, counts and placement values."); return; }
#if WITH_EDITOR
			const FScopedTransaction Transaction(FText::FromString(TEXT("Create TSAV scenery")));
			World->GetCurrentLevel()->Modify();
#endif
			FVector Location(X,Y,Z); if (Grid>0) Location=Location.GridSnap(Grid);
			FActorSpawnParameters Params; Params.ObjectFlags|=RF_Transactional; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Actor=World->SpawnActor<ATSAVSceneGenerator>(ATSAVSceneGenerator::StaticClass(),FTransform(FRotator(0,Yaw,0),Location),Params);
			if (!Actor) { Message=TEXT("Could not create scenery."); return; }
			Actor->Configure(Settings); Current=Actor;
			auto* Identity=Actor->FindComponentByClass<UTSAVSceneObjectComponent>(); Identity->DisplayName=Name.IsEmpty()?KindName(Settings.Kind):FText::FromString(Name);
#if WITH_EDITOR
			Actor->SetActorLabel(Identity->DisplayName.ToString());
#endif
			if (auto* History=Commands()) History->CommitSpawnedActor(Actor,FText::FromString(TEXT("Create TSAV scenery")));
			Message=TEXT("Created. Save the scene/project to keep this object."); Refresh();
		}
		void Apply() {
			if (!Current.IsValid() || !ATSAVSceneGenerator::Validate(Settings)) { Message=TEXT("Choose an object and enter valid dimensions."); return; }
#if WITH_EDITOR
			const FScopedTransaction Transaction(FText::FromString(TEXT("Configure TSAV scenery")));
#endif
			const FString Before=Current->CaptureTSAVState(); Current->Configure(Settings);
			if (auto* History=Commands()) History->CommitAppliedActorState(Current.Get(),Before,FText::FromString(TEXT("Configure TSAV scenery")));
			Message=TEXT("Updated geometry and motion. Existing placement is preserved; use the scene transform tools to move it.");
		}
		TWeakObjectPtr<UWorld> World; TWeakObjectPtr<ATSAVSceneGenerator> Current;
		FTSAVScenicSettings Settings; TArray<TSharedPtr<ETSAVScenicKind>> Kinds; TSharedPtr<SVerticalBox> Objects;
		FString Name,Message; float X=0,Y=0,Z=0,Yaw=0,Grid=25;
	};
}
TSharedRef<SWidget> MakeTSAVSceneBuilder(UWorld* World,FSimpleDelegate Close,ETSAVScenicKind InitialKind) { return SNew(TSAVSceneBuilder::SBuilder).World(World).OnClose(Close).InitialKind(InitialKind); }
TSharedRef<SWidget> UTSAVSceneBuilderWidget::RebuildWidget() { return MakeTSAVSceneBuilder(GetWorld(),FSimpleDelegate::CreateWeakLambda(this,[this] { RemoveFromParent(); }),InitialKind); }
