// Copyright TSAV. All Rights Reserved.
#include "UI/TSAVLaserWidget.h"
#include "Laser/TSAVLaserPreview.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "Interaction/TSAVCommandSubsystem.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

namespace TSAVLaserUI
{
	TSharedRef<SWidget> Label(const FString& Text) { return SNew(STextBlock).Text(FText::FromString(Text)).AutoWrapText(true); }
	template<typename T> TSharedRef<SWidget> Number(const FString& Name,T* Value,T Min,T Max) {
		return SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[Label(Name)]
			+SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(150)[SNew(SNumericEntryBox<T>).MinValue(Min).MaxValue(Max).Value_Lambda([Value] { return *Value; }).OnValueChanged_Lambda([Value](T V) { *Value=V; })]];
	}
	class SPanel final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPanel) {} SLATE_ARGUMENT(UWorld*,World) SLATE_EVENT(FSimpleDelegate,OnClose) SLATE_END_ARGS()
		void Construct(const FArguments& Args) {
			World=Args._World; Path=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TSAV Shows/Lasers.ild")));
			ChildSlot[SNew(SBorder).Padding(20)[SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[Label(TEXT("TSAV LASER PREVIEW AND ILDA"))]
					+SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("Close"))).Visibility(Args._OnClose.IsBound()?EVisibility::Visible:EVisibility::Collapsed).OnClicked_Lambda([Close=Args._OnClose] { Close.ExecuteIfBound(); return FReply::Handled(); })]]
				+SVerticalBox::Slot().AutoHeight().Padding(0,10)[Label(TEXT("Create an original pattern or import ILDA frames. Move and aim the projector with the scene transform tools. Project saves include the frames and preview settings."))]
				+SVerticalBox::Slot().FillHeight(1)[SNew(SHorizontalBox)
					+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,20,0)[SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
						+SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
							+SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Circle pattern"))).OnClicked_Lambda([this] { Pattern(0); return FReply::Handled(); })]
							+SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Fan pattern"))).OnClicked_Lambda([this] { Pattern(1); return FReply::Handled(); })]
							+SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Lissajous pattern"))).OnClicked_Lambda([this] { Pattern(2); return FReply::Handled(); })]]
						+SVerticalBox::Slot().AutoHeight().Padding(0,8)[Number(TEXT("Frames per second"),&Draft.FramesPerSecond,1.0f,120.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,8)[Number(TEXT("ILDA projector number"),&Draft.Projector,0,255)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,8)[Number(TEXT("Projection width (cm)"),&Draft.WidthCm,1.0f,100000.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,8)[Number(TEXT("Projection height (cm)"),&Draft.HeightCm,1.0f,100000.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,8)[Number(TEXT("Projection distance (cm)"),&Draft.DistanceCm,1.0f,100000.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,8)[Number(TEXT("Preview line width (cm)"),&Draft.LineWidthCm,0.01f,100.0f)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,8)[Number(TEXT("Intensity (0–1)"),&Draft.Intensity,0.0f,1.0f)]
						+SVerticalBox::Slot().AutoHeight()[Check(TEXT("Show beam rays"),&Draft.bShowRays)]
						+SVerticalBox::Slot().AutoHeight()[Check(TEXT("Loop frames"),&Draft.bLoop)]
						+SVerticalBox::Slot().AutoHeight()[Check(TEXT("Follow TSAV lighting timeline"),&Draft.bFollowShowTimeline)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,10)[SNew(SButton).Text(FText::FromString(TEXT("Apply preview settings"))).OnClicked_Lambda([this] { Apply(); return FReply::Handled(); })]
						+SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
							+SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Play"))).OnClicked_Lambda([this] { if (Current.IsValid()) Current->Play(); return FReply::Handled(); })]
							+SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Pause"))).OnClicked_Lambda([this] { if (Current.IsValid()) Current->Pause(); return FReply::Handled(); })]
							+SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Stop / rewind"))).OnClicked_Lambda([this] { if (Current.IsValid()) { Current->Pause(); Current->Seek(0); } return FReply::Handled(); })]]
						+SVerticalBox::Slot().AutoHeight().Padding(0,8)[Number(TEXT("Seek time (seconds)"),&SeekTime,0.0f,86400.0f)]
						+SVerticalBox::Slot().AutoHeight()[SNew(SButton).Text(FText::FromString(TEXT("Seek"))).OnClicked_Lambda([this] { if (Current.IsValid()) Current->Seek(SeekTime); return FReply::Handled(); })]
						+SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(STextBlock).Text_Lambda([this] { return FText::FromString(Current.IsValid()?FString::Printf(TEXT("%s — %.2f s — %d frame records"),Current->IsPlaying()?TEXT("Playing"):TEXT("Paused"),Current->GetTime(),Current->GetData().Frames.Num()):TEXT("No projector selected")); })]
					]]
					+SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
						+SVerticalBox::Slot().AutoHeight()[SNew(SButton).Text(FText::FromString(TEXT("Refresh projectors"))).OnClicked_Lambda([this] { Refresh(); return FReply::Handled(); })]
						+SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(Projectors,SVerticalBox)]]
						+SVerticalBox::Slot().AutoHeight()[SNew(SButton).Text(FText::FromString(TEXT("New projector"))).OnClicked_Lambda([this] { Current.Reset(); Draft={}; Pattern(0); return FReply::Handled(); })]
						+SVerticalBox::Slot().AutoHeight().Padding(0,12)[SNew(SEditableTextBox).Text_Lambda([this] { return FText::FromString(Path); }).OnTextChanged_Lambda([this](const FText& Value) { Path=Value.ToString(); })]
						+SVerticalBox::Slot().AutoHeight()[Check(TEXT("Export indexed color for older software"),&bIndexed)]
						+SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
							+SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Import ILDA"))).OnClicked_Lambda([this] { Import(); return FReply::Handled(); })]
							+SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Export ILDA"))).OnClicked_Lambda([this] { if (Current.IsValid()) { FString Error; Message=Current->ExportILDA(Path,Error,bIndexed)?TEXT("ILDA frames exported."):Error; } return FReply::Handled(); })]]
					]]
				+SVerticalBox::Slot().AutoHeight().Padding(0,10)[SNew(STextBlock).Text_Lambda([this] { return FText::FromString(Message); }).AutoWrapText(true)]
			]]; Refresh();
		}
	private:
		TSharedRef<SWidget> Check(const FString& Text,bool* Value) { return SNew(SCheckBox).IsChecked_Lambda([Value] { return *Value?ECheckBoxState::Checked:ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([Value](ECheckBoxState State) { *Value=State==ECheckBoxState::Checked; })[Label(Text)]; }
		UTSAVCommandSubsystem* Commands() const { auto* GI=World.IsValid()?World->GetGameInstance():nullptr; return GI?GI->GetSubsystem<UTSAVCommandSubsystem>():nullptr; }
		void Refresh() {
			Projectors->ClearChildren(); if (!World.IsValid()) return;
			for (TActorIterator<ATSAVLaserPreview> It(World.Get()); It; ++It) {
				const TWeakObjectPtr<ATSAVLaserPreview> Actor(*It);
				Projectors->AddSlot().AutoHeight().Padding(0,4)[SNew(SButton).Text(FText::FromString(It->GetActorNameOrLabel())).OnClicked_Lambda([this,Actor] { if (Actor.IsValid()) { Current=Actor; Draft=Actor->GetData(); } return FReply::Handled(); })];
			}
		}
		void Pattern(int32 Index) { Draft.Frames=TSAVILDA::MakePattern(Index); Draft.Projector=0; Apply(); }
		void Apply() {
			if (!World.IsValid() || !ATSAVLaserPreview::Validate(Draft)) { Message=TEXT("Choose frames and valid preview settings."); return; }
#if WITH_EDITOR
			const FScopedTransaction Transaction(FText::FromString(TEXT("Configure TSAV laser preview"))); World->GetCurrentLevel()->Modify();
#endif
			const bool bNew=!Current.IsValid(); const FString Before=bNew?FString():Current->CaptureTSAVState();
			if (bNew) { FActorSpawnParameters Params; Params.ObjectFlags|=RF_Transactional; Current=World->SpawnActor<ATSAVLaserPreview>(ATSAVLaserPreview::StaticClass(),FTransform(FVector(0,0,200)),Params); }
			if (!Current.IsValid()) { Message=TEXT("Could not create projector."); return; }
			Current->Configure(Draft);
			if (bNew) { Current->FindComponentByClass<UTSAVSceneObjectComponent>()->DisplayName=FText::FromString(TEXT("TSAV Laser Projector"));
#if WITH_EDITOR
				Current->SetActorLabel(TEXT("TSAV Laser Projector"));
#endif
			}
			if (auto* History=Commands()) { if (bNew) History->CommitSpawnedActor(Current.Get(),FText::FromString(TEXT("Create laser projector"))); else History->CommitAppliedActorState(Current.Get(),Before,FText::FromString(TEXT("Configure laser preview"))); }
			Message=TEXT("Preview updated. Save the scene/project to keep frames and settings."); Refresh();
		}
		void Import() {
			const int64 Size=IFileManager::Get().FileSize(*Path); TArray<uint8> Bytes; TArray<FTSAVLaserFrame> Frames;
			if (Size<32 || Size>64*1024*1024 || !FFileHelper::LoadFileToArray(Bytes,*Path)) { Message=TEXT("Choose a readable ILDA file up to 64 MB."); return; }
			if (!TSAVILDA::Decode(Bytes,Frames,Message)) return;
			if (Frames.IsEmpty()) { Message=TEXT("The file contains no laser frames."); return; }
			Draft.Frames=MoveTemp(Frames); Draft.Projector=Draft.Frames[0].Projector; Apply();
		}
		TWeakObjectPtr<UWorld> World; TWeakObjectPtr<ATSAVLaserPreview> Current; FTSAVLaserData Draft;
		TSharedPtr<SVerticalBox> Projectors; FString Path,Message; float SeekTime=0; bool bIndexed=false;
	};
}
TSharedRef<SWidget> MakeTSAVLaserPanel(UWorld* World,FSimpleDelegate Close) { return SNew(TSAVLaserUI::SPanel).World(World).OnClose(Close); }
TSharedRef<SWidget> UTSAVLaserWidget::RebuildWidget() { return MakeTSAVLaserPanel(GetWorld(),FSimpleDelegate::CreateWeakLambda(this,[this] { RemoveFromParent(); })); }
