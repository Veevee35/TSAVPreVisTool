// Copyright TSAV. All Rights Reserved.
#include "UI/TSAVRigWidget.h"
#include "Lighting/TSAVRigExchange.h"
#include "TSAVDMXFixture.h"
#include "Interaction/TSAVCommandSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif
namespace TSAVRigUI
{
	TSharedRef<SWidget> Text(const FString& Value) { return SNew(STextBlock).Text(FText::FromString(Value)).AutoWrapText(true); }
	template<typename T> TSharedRef<SWidget> Number(const FString& Name,T* Value,T Min,T Max) {
		return SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[Text(Name)]+SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(140)[SNew(SNumericEntryBox<T>).MinValue(Min).MaxValue(Max).Value_Lambda([Value] { return *Value; }).OnValueChanged_Lambda([Value](T V) { *Value=V; })]];
	}
	class SPanel final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPanel) {} SLATE_ARGUMENT(UWorld*,World) SLATE_EVENT(FSimpleDelegate,OnClose) SLATE_END_ARGS()
		void Construct(const FArguments& Args) {
			World=Args._World; MVRPath=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TSAV Shows/Rig.mvr")));
			ChildSlot[SNew(SBorder).Padding(20)[SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[Text(TEXT("TSAV FIXTURE ARRAYS AND RIG EXCHANGE"))]
					+SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("Close"))).Visibility(Args._OnClose.IsBound()?EVisibility::Visible:EVisibility::Collapsed).OnClicked_Lambda([Close=Args._OnClose] { Close.ExecuteIfBound(); return FReply::Handled(); })]]
				+SVerticalBox::Slot().AutoHeight().Padding(0,10)[Text(TEXT("Place independently patched fixture arrays from a scene fixture or GDTF profile. MVR transfers fixture identities, transforms, modes and patches; non-fixture scenery stays in the source file."))]
				+SVerticalBox::Slot().FillHeight(1)[SNew(SHorizontalBox)
					+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,20,0)[SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
						+SVerticalBox::Slot().AutoHeight()[Text(TEXT("GDTF path"))]
						+SVerticalBox::Slot().AutoHeight()[SNew(SEditableTextBox).Text_Lambda([this] { return FText::FromString(GDTFPath); }).OnTextChanged_Lambda([this](const FText& V) { GDTFPath=V.ToString(); })]
						+SVerticalBox::Slot().AutoHeight().Padding(0,6)[SNew(SButton).Text(FText::FromString(TEXT("Read GDTF profile"))).OnClicked_Lambda([this] { LoadProfile(); return FReply::Handled(); })]
						+SVerticalBox::Slot().AutoHeight()[SAssignNew(ModeRows,SVerticalBox)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SButton).Text(FText::FromString(TEXT("Refresh scene fixtures"))).OnClicked_Lambda([this] { Refresh(); return FReply::Handled(); })]
						+SVerticalBox::Slot().AutoHeight()[SAssignNew(FixtureRows,SVerticalBox)]]]
					+SHorizontalBox::Slot().FillWidth(1)[SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
						+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this] { return FText::FromString(bUseProfile && Profile.Modes.IsValidIndex(ModeIndex)?Profile.Name+TEXT(" — ")+Profile.Modes[ModeIndex].DMX.ModeName:Source.IsValid()?Source->GetActorNameOrLabel():TEXT("Select a source fixture or GDTF mode")); }).AutoWrapText(true)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,6)[Number(TEXT("Columns"),&Columns,1,512)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,6)[Number(TEXT("Rows"),&Rows,1,512)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,6)[Number(TEXT("Column spacing (cm)"),&SpacingX,-100000.0f,100000.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,6)[Number(TEXT("Row spacing (cm)"),&SpacingY,-100000.0f,100000.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,6)[Number(TEXT("Origin X (cm)"),&X,-1000000.0f,1000000.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,6)[Number(TEXT("Origin Y (cm)"),&Y,-1000000.0f,1000000.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,6)[Number(TEXT("Origin Z (cm)"),&Z,-1000000.0f,1000000.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,6)[Number(TEXT("Yaw (degrees)"),&Yaw,-360.0f,360.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,6)[Number(TEXT("Starting universe"),&Universe,1,63999)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,6)[Number(TEXT("Starting address"),&Address,1,512)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SButton).Text(FText::FromString(TEXT("Place and patch fixture array"))).OnClicked_Lambda([this] { Place(); return FReply::Handled(); })]
						+SVerticalBox::Slot().AutoHeight().Padding(0,12)[Text(TEXT("MVR fixture rig path"))]
						+SVerticalBox::Slot().AutoHeight()[SNew(SEditableTextBox).Text_Lambda([this] { return FText::FromString(MVRPath); }).OnTextChanged_Lambda([this](const FText& V) { MVRPath=V.ToString(); })]
						+SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SHorizontalBox)
							+SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Inspect MVR"))).OnClicked_Lambda([this] { ReadRig(); return FReply::Handled(); })]
							+SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Import inspected rig"))).OnClicked_Lambda([this] { Import(); return FReply::Handled(); })]]
						+SVerticalBox::Slot().AutoHeight()[SNew(SButton).Text(FText::FromString(TEXT("Export all scene fixtures to MVR"))).OnClicked_Lambda([this] { TArray<ATSAVDMXFixture*> Fixtures; if (World.IsValid()) for (TActorIterator<ATSAVDMXFixture> It(World.Get()); It; ++It) Fixtures.Add(*It); if (TSAVRigExchange::ExportMVR(MVRPath,Fixtures,Message)) Message=TEXT("Fixture rig exported with embedded GDTF profiles."); return FReply::Handled(); })]
					]]]
				+SVerticalBox::Slot().AutoHeight().Padding(0,12)[SNew(STextBlock).Text_Lambda([this] { return FText::FromString(Message); }).AutoWrapText(true)]
			]]; Refresh();
		}
	private:
		void Refresh() { FixtureRows->ClearChildren(); if (!World.IsValid()) return; for (TActorIterator<ATSAVDMXFixture> It(World.Get()); It; ++It) {
			const TWeakObjectPtr<ATSAVDMXFixture> Actor(*It); FixtureRows->AddSlot().AutoHeight().Padding(0,3)[SNew(SButton).Text(FText::FromString(It->GetActorNameOrLabel())).OnClicked_Lambda([this,Actor] { Source=Actor; bUseProfile=false; return FReply::Handled(); })];
		} }
		void LoadProfile() { FTSAVGDTFProfile Candidate; if (!TSAVRigExchange::LoadGDTF(GDTFPath,Candidate,Message)) return; Profile=MoveTemp(Candidate); ModeIndex=0; bUseProfile=true; ModeRows->ClearChildren();
			for (int32 I=0; I<Profile.Modes.Num(); ++I) ModeRows->AddSlot().AutoHeight().Padding(0,3)[SNew(SButton).Text(FText::FromString(FString::Printf(TEXT("%s — %d channels"),*Profile.Modes[I].DMX.ModeName,Profile.Modes[I].DMX.ChannelSpan))).OnClicked_Lambda([this,I] { ModeIndex=I; bUseProfile=true; return FReply::Handled(); })];
			Message=Profile.Manufacturer+TEXT(" — ")+Profile.Name+TEXT(". Runtime preview uses TSAV fixture geometry. ")+FString::Join(Profile.Warnings,TEXT("\n"));
		}
		void Remember(const TArray<ATSAVDMXFixture*>& Created) { if (auto* GI=World.IsValid()?World->GetGameInstance():nullptr) if (auto* Commands=GI->GetSubsystem<UTSAVCommandSubsystem>()) { TArray<AActor*> Actors; for (auto* A : Created) Actors.Add(A); Commands->CommitSpawnedActors(Actors,FText::FromString(TEXT("Place TSAV rig"))); } Refresh(); }
		void Place() {
			if (!World.IsValid()) return;
#if WITH_EDITOR
			const FScopedTransaction Transaction(FText::FromString(TEXT("Place TSAV fixture array"))); World->GetCurrentLevel()->Modify();
#endif
			TArray<ATSAVDMXFixture*> Created; if (TSAVRigExchange::PlaceGrid(World.Get(),bUseProfile?nullptr:Source.Get(),bUseProfile?&Profile:nullptr,ModeIndex,FIntPoint(Columns,Rows),FVector2D(SpacingX,SpacingY),FTransform(FRotator(0,Yaw,0),FVector(X,Y,Z)),Universe,Address,Created,Message)) { Remember(Created); Message=FString::Printf(TEXT("Placed %d independently patched fixtures. Save the scene/project to keep the rig."),Created.Num()); }
		}
		void ReadRig() { FTSAVRigDocument Candidate; if (!TSAVRigExchange::ReadMVR(MVRPath,Candidate,Message)) return; Rig=MoveTemp(Candidate); InspectedPath=MVRPath; Message=FString::Printf(TEXT("Ready: %d fixtures, %d profiles. "),Rig.Fixtures.Num(),Rig.Profiles.Num())+FString::Join(Rig.Warnings,TEXT("\n")); }
		void Import() {
			if (!World.IsValid() || Rig.Fixtures.IsEmpty() || MVRPath!=InspectedPath) { Message=TEXT("Inspect the MVR file before importing it."); return; }
#if WITH_EDITOR
			const FScopedTransaction Transaction(FText::FromString(TEXT("Import TSAV MVR rig"))); World->GetCurrentLevel()->Modify();
#endif
			TArray<ATSAVDMXFixture*> Created; if (TSAVRigExchange::ImportRig(World.Get(),Rig,Created,Message)) { Remember(Created); Message=FString::Printf(TEXT("Imported %d fixtures. Save the scene/project to keep the rig."),Created.Num()); }
		}
		TWeakObjectPtr<UWorld> World; TWeakObjectPtr<ATSAVDMXFixture> Source;
		TSharedPtr<SVerticalBox> FixtureRows,ModeRows; FTSAVGDTFProfile Profile; FTSAVRigDocument Rig;
		FString GDTFPath,MVRPath,InspectedPath,Message; bool bUseProfile=false; int32 ModeIndex=0,Rows=1,Columns=1,Universe=1,Address=1;
		float SpacingX=100,SpacingY=100,X=0,Y=0,Z=300,Yaw=0;
	};
}
TSharedRef<SWidget> MakeTSAVRigPanel(UWorld* World,FSimpleDelegate Close) { return SNew(TSAVRigUI::SPanel).World(World).OnClose(Close); }
TSharedRef<SWidget> UTSAVRigWidget::RebuildWidget() { return MakeTSAVRigPanel(GetWorld(),FSimpleDelegate::CreateWeakLambda(this,[this] { RemoveFromParent(); })); }
