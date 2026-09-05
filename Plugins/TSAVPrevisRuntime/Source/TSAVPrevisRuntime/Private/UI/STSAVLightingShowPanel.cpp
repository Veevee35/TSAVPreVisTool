// Copyright TSAV. All Rights Reserved.
#include "UI/STSAVLightingShowPanel.h"
#include "Lighting/TSAVLightingShow.h"
#include "TSAVDMXFixture.h"
#include "TSAVDMXPatchPlan.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Interaction/TSAVCommandSubsystem.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

namespace TSAVShowPanel
{
	TSharedRef<SWidget> Text(const FString& Value) { return SNew(STextBlock).Text(FText::FromString(Value)).AutoWrapText(true); }
	template<typename T> TSharedRef<SWidget> Number(const FString& Label, T* Value, T Minimum, T Maximum)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1)[Text(Label)]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(82)[SNew(SNumericEntryBox<T>)
				.MinValue(Minimum).MaxValue(Maximum).Value_Lambda([Value] { return *Value; }).OnValueChanged_Lambda([Value](T NewValue) { *Value = NewValue; })]];
	}
}

void STSAVLightingShowPanel::Construct(const FArguments& Args)
{
	using namespace TSAVShowPanel;
	World = Args._World;
	FilePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TSAV Shows/Lighting.tsavlight"));
	for (FName Kind : {TEXT("Group"), TEXT("Preset"), TEXT("Cue"), TEXT("Effect"), TEXT("Recording")}) SlotNumbers.Add(Kind, 1);
	for (FName Attribute : {TEXT("Pan"), TEXT("Tilt"), TEXT("Dimmer"), TEXT("Red"), TEXT("Green"), TEXT("Blue"), TEXT("Zoom")})
		FaderValues.Add(Attribute, Attribute == TEXT("Pan") || Attribute == TEXT("Tilt") ? 0.5f : Attribute == TEXT("Zoom") ? 0.0f : 1.0f);
	auto Programmer = SNew(SVerticalBox);
	Programmer->AddSlot().AutoHeight()[Text(TEXT("PROGRAMMER"))];
	for (FName Attribute : {TEXT("Pan"), TEXT("Tilt"), TEXT("Dimmer"), TEXT("Red"), TEXT("Green"), TEXT("Blue"), TEXT("Zoom")})
		Programmer->AddSlot().AutoHeight().Padding(0, 3)[Fader(Attribute)];
	Programmer->AddSlot().AutoHeight().Padding(0, 8)[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1)[Text(TEXT("Grand master"))]
		+ SHorizontalBox::Slot().FillWidth(2)[SNew(SSlider).Value_Lambda([this] { auto* S = Show(false); return S ? S->GetMaster() : 1.0f; })
			.OnValueChanged_Lambda([this](float Value) { if (auto* S = Show()) S->SetMaster(Value); })]];
	Programmer->AddSlot().AutoHeight()[SNew(SCheckBox)
		.IsChecked_Lambda([this] { auto* S = Show(false); return S && S->IsBlackout() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { if (auto* S = Show()) S->SetBlackout(State == ECheckBoxState::Checked); })[Text(TEXT("BLACKOUT"))]];
	Programmer->AddSlot().AutoHeight().Padding(0, 5)[SNew(SButton).Text(FText::FromString(TEXT("Release programmer to playback")))
		.OnClicked_Lambda([this] { if (auto* S = Show(false)) S->ClearProgrammer(); return FReply::Handled(); })];
	Programmer->AddSlot().AutoHeight().Padding(0, 8)[Text(TEXT("PRIMARY MODE ATTRIBUTES"))];
	Programmer->AddSlot().AutoHeight()[SAssignNew(RawFaders, SVerticalBox)];
	Programmer->AddSlot().AutoHeight().Padding(0,8)[Text(TEXT("TSAV OPTICS"))];
	for (FName Attribute : {TEXT("Shutter"),TEXT("Strobe"),TEXT("Iris"),TEXT("Frost"),TEXT("ColorWheel"),TEXT("Cyan"),TEXT("Magenta"),TEXT("Yellow"),TEXT("CTO"),TEXT("Gobo"),TEXT("GoboRotation"),TEXT("GoboSpin"),TEXT("Prism"),TEXT("PrismRotation"),TEXT("BladeTop"),TEXT("BladeBottom"),TEXT("BladeLeft"),TEXT("BladeRight")}) {
		FaderValues.Add(Attribute,Attribute==TEXT("Shutter") || Attribute==TEXT("Iris") ? 1.0f : Attribute==TEXT("GoboSpin") ? 0.5f : 0.0f);
		Programmer->AddSlot().AutoHeight().Padding(0,3)[Fader(Attribute)];
	}

	ChildSlot[SNew(SBorder).Padding(12)[SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1)[Text(TEXT("TSAV LIGHTING SHOW — NATIVE CONSOLE"))]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Visibility(Args._OnClose.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
				.Text(FText::FromString(TEXT("Close"))).OnClicked_Lambda([Close = Args._OnClose] { Close.ExecuteIfBound(); return FReply::Handled(); })]]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5)[Text(TEXT("Select scene fixtures. Store looks before master/blackout; release the programmer when running cues."))]
		+ SVerticalBox::Slot().FillHeight(1)[SNew(SSplitter)
			+ SSplitter::Slot().Value(0.27f)[SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Select all"))).OnClicked_Lambda([this] { if (World.IsValid()) for (TActorIterator<ATSAVDMXFixture> It(World.Get()); It; ++It) Selected.Add(ATSAVLightingShow::FixtureId(*It)); RebuildRawFaders(); return FReply::Handled(); })]
					+ SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Clear"))).OnClicked_Lambda([this] { Selected.Reset(); RebuildRawFaders(); return FReply::Handled(); })]]
				+ SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(FixtureRows, SVerticalBox)]]
				+ SVerticalBox::Slot().AutoHeight()[Number(TEXT("Universe"), &Universe, 1, 63999)]
				+ SVerticalBox::Slot().AutoHeight()[Number(TEXT("Start address"), &Address, 1, 512)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 5)[SNew(SButton).Text(FText::FromString(TEXT("Patch selected consecutively"))).OnClicked_Lambda([this] { PatchSelection(); return FReply::Handled(); })]
				+ SVerticalBox::Slot().AutoHeight()[Text(TEXT("Add fixtures from the Lighting menu / fixture library. Scene instances use separate patches."))]]
			+ SSplitter::Slot().Value(0.30f)[SNew(SScrollBox)+SScrollBox::Slot().Padding(10,0)[Programmer]]
			+ SSplitter::Slot().Value(0.43f)[SNew(SScrollBox)+SScrollBox::Slot().Padding(10,0)[SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SNew(SEditableTextBox).HintText(FText::FromString(TEXT("Name for the stored slot"))).Text_Lambda([this] { return FText::FromString(SlotName); }).OnTextChanged_Lambda([this](const FText& Value) { SlotName = Value.ToString(); })]
				+ SVerticalBox::Slot().AutoHeight()[SlotSection(TEXT("Group"), TEXT("GROUPS"))]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SButton).Text_Lambda([this] { return FText::FromString(TEXT("Store scope: ")+StaticEnum<ETSAVLightingStoreScope>()->GetNameStringByValue(StoreScope)+TEXT("  (click to change)")); }).OnClicked_Lambda([this] { StoreScope=(StoreScope+1)%6; return FReply::Handled(); })]
				+ SVerticalBox::Slot().AutoHeight()[SlotSection(TEXT("Preset"), TEXT("PRESETS"))]
				+ SVerticalBox::Slot().AutoHeight()[Number(TEXT("Cue fade (seconds)"), &Fade, 0.0f, 86400.0f)]
				+ SVerticalBox::Slot().AutoHeight()[Number(TEXT("Cue delay"), &Delay, 0.0f, 86400.0f)]
				+ SVerticalBox::Slot().AutoHeight()[Number(TEXT("Follow hold (-1 = manual)"), &Follow, -1.0f, 86400.0f)]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SCheckBox).IsChecked_Lambda([this] { return bTrack ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bTrack = State == ECheckBoxState::Checked; })[Text(TEXT("Track values from earlier cues"))]]
				+ SVerticalBox::Slot().AutoHeight()[SlotSection(TEXT("Cue"), TEXT("CUES"))]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Previous"))).OnClicked_Lambda([this] { if (auto* S = Show(false)) S->Previous(); return FReply::Handled(); })]
					+ SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("GO next"))).OnClicked_Lambda([this] { if (auto* S = Show()) Status(S->Next() ? TEXT("Cue running.") : TEXT("No next cue.")); return FReply::Handled(); })]
					+ SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text_Lambda([this] { auto* S = Show(false); return FText::FromString(S && S->IsPaused() ? TEXT("Resume") : TEXT("Pause")); }).OnClicked_Lambda([this] { if (auto* S = Show(false)) S->SetPaused(!S->IsPaused()); return FReply::Handled(); })]
					+ SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Stop"))).OnClicked_Lambda([this] { if (auto* S = Show(false)) S->Stop(); return FReply::Handled(); })]]
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this] { auto* S = Show(false); return FText::FromString(S && S->GetCurrentCue() != INDEX_NONE ? FString::Printf(TEXT("Cue %d — %.2f s"), S->GetCurrentCue(), S->GetCueTime()) : TEXT("Playback stopped")); })]
				+ SVerticalBox::Slot().AutoHeight()[Number(TEXT("Seek current cue (seconds)"), &SeekSeconds, 0.0f, 86400.0f)]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SButton).Text(FText::FromString(TEXT("Seek"))).OnClicked_Lambda([this] { if (auto* S = Show(false)) S->Seek(SeekSeconds); return FReply::Handled(); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,8)[Text(TEXT("EFFECT PARAMETERS"))]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SEditableTextBox).Text_Lambda([this] { return FText::FromString(EffectAttribute); }).OnTextChanged_Lambda([this](const FText& Value) { EffectAttribute = Value.ToString(); })]
				+ SVerticalBox::Slot().AutoHeight()[Number(TEXT("Wave: 0 sine, 1 triangle, 2 square, 3 saw"), &Wave, 0, 3)]
				+ SVerticalBox::Slot().AutoHeight()[Number(TEXT("Period (seconds)"), &Period, 0.01f, 86400.0f)]
				+ SVerticalBox::Slot().AutoHeight()[Number(TEXT("Minimum"), &Minimum, 0.0f, 1.0f)]
				+ SVerticalBox::Slot().AutoHeight()[Number(TEXT("Maximum"), &Maximum, 0.0f, 1.0f)]
				+ SVerticalBox::Slot().AutoHeight()[Number(TEXT("Phase spread (cycles)"), &Spread, -100.0f, 100.0f)]
				+ SVerticalBox::Slot().AutoHeight()[SlotSection(TEXT("Effect"), TEXT("EFFECTS"))]
				+ SVerticalBox::Slot().AutoHeight()[TransportSection()]
				+ SVerticalBox::Slot().AutoHeight()[ExecutorSection()]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,12)[Text(TEXT("IMAGE PIXEL MAPPING"))]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SEditableTextBox).HintText(FText::FromString(TEXT("PNG / JPEG image path"))).Text_Lambda([this] { return FText::FromString(PixelMapPath); }).OnTextChanged_Lambda([this](const FText& V) { PixelMapPath=V.ToString(); })]
				+ SVerticalBox::Slot().AutoHeight()[Number(TEXT("Fixture grid columns"),&PixelMapColumns,1,512)]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SButton).Text(FText::FromString(TEXT("Map image to selected fixtures / matrix cells"))).OnClicked_Lambda([this] { if (auto* S=Show()) { FString Error; const bool OK=S->ApplyPixelMap(Selection(),PixelMapColumns,PixelMapPath,Error); Status(OK?TEXT("Image mapped into the programmer. Store a preset or cue to keep this look."):Error,OK); } return FReply::Handled(); })]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SButton).Text(FText::FromString(TEXT("Stop effects"))).OnClicked_Lambda([this] { if (auto* S = Show(false)) S->StopEffects(); return FReply::Handled(); })]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SCheckBox).IsChecked_Lambda([this] { auto* S = Show(false); return S && S->bNetworkOutput ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState State) { if (auto* S = Show()) { S->bNetworkOutput = State == ECheckBoxState::Checked; S->Flush(); } })[Text(TEXT("Send show DMX to configured output ports"))]]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SCheckBox).IsChecked_Lambda([this] { auto* S = Show(false); return S && S->bLoop ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState State) { if (auto* S = Show()) S->bLoop = State == ECheckBoxState::Checked; })[Text(TEXT("Loop cue stack"))]]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SEditableTextBox).Text_Lambda([this] { return FText::FromString(FilePath); }).OnTextChanged_Lambda([this](const FText& Value) { FilePath = Value.ToString(); })]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Save show data"))).OnClicked_Lambda([this] { auto* S = Show(); const bool OK = S && S->SaveShow(FilePath); Status(OK ? TEXT("Show data saved. Save the scene/project for fixture geometry and patches.") : TEXT("Could not save show file."), OK); return FReply::Handled(); })]
					+ SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Load show data"))).OnClicked_Lambda([this] { auto* S = Show(); const FString Before = S ? S->CaptureTSAVState() : FString(); Remember(S, Before, S && S->LoadShow(FilePath)); return FReply::Handled(); })]]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Show undo"))).OnClicked_Lambda([this] { UndoShow(false); return FReply::Handled(); })]
					+ SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Show redo"))).OnClicked_Lambda([this] { UndoShow(true); return FReply::Handled(); })]]
			]]]
		+ SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return FText::FromString(StatusMessage); }).ColorAndOpacity_Lambda([this] { return bStatusOK ? FLinearColor(0.3f,0.9f,0.5f) : FLinearColor(1.0f,0.35f,0.2f); })]
	]];
	RefreshFixtures(); RefreshSlots();
}

ATSAVLightingShow* STSAVLightingShowPanel::Show(bool bCreate) const { return ATSAVLightingShow::Find(World.Get(), bCreate); }

void STSAVLightingShowPanel::SelectFixtures(const TArray<ATSAVDMXFixture*>& Fixtures)
{
	Selected.Reset(); for (auto* Fixture : Fixtures) if (IsValid(Fixture) && Fixture->GetWorld()==World.Get()) Selected.Add(ATSAVLightingShow::FixtureId(Fixture));
	RefreshFixtures();
}

TArray<ATSAVDMXFixture*> STSAVLightingShowPanel::Selection() const
{
	TArray<ATSAVDMXFixture*> Result;
	if (World.IsValid()) for (TActorIterator<ATSAVDMXFixture> It(World.Get()); It; ++It) if (Selected.Contains(ATSAVLightingShow::FixtureId(*It, false))) Result.Add(*It);
	Result.Sort([](const ATSAVDMXFixture& A, const ATSAVDMXFixture& B) { return A.GetActorNameOrLabel() < B.GetActorNameOrLabel(); });
	return Result;
}

void STSAVLightingShowPanel::RefreshFixtures()
{
	if (!World.IsValid()) return;
	TArray<ATSAVDMXFixture*> Fixtures;
	for (TActorIterator<ATSAVDMXFixture> It(World.Get()); It; ++It) Fixtures.Add(*It);
	Fixtures.Sort([](const ATSAVDMXFixture& A, const ATSAVDMXFixture& B) { return A.GetActorNameOrLabel() < B.GetActorNameOrLabel(); });
	FString Signature; TSet<FGuid> Existing;
	for (auto* Fixture : Fixtures)
	{
		const FGuid Id = ATSAVLightingShow::FixtureId(Fixture); Existing.Add(Id);
		const auto* Patch = Fixture->GetFixturePatch();
		Signature += Id.ToString() + Fixture->GetActorNameOrLabel() + FString::Printf(TEXT(":%d:%d;"), Patch ? Patch->GetUniverseID() : 0, Patch ? Patch->GetStartingChannel() : 0);
	}
	Selected = Selected.Intersect(Existing);
	if (Signature != FixtureSignature)
	{
		FixtureSignature = Signature; FixtureRows->ClearChildren();
		for (auto* Fixture : Fixtures)
		{
			const FGuid Id = ATSAVLightingShow::FixtureId(Fixture); const auto* Patch = Fixture->GetFixturePatch();
			const FString Label = Fixture->GetActorNameOrLabel() + (Patch ? FString::Printf(TEXT("  U%d.%03d"), Patch->GetUniverseID(), Patch->GetStartingChannel()) : TEXT(" — unpatched"));
			FixtureRows->AddSlot().AutoHeight().Padding(0,3)[SNew(SCheckBox).IsChecked_Lambda([this, Id] { return Selected.Contains(Id) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this, Id](ECheckBoxState State) { if (State == ECheckBoxState::Checked) Selected.Add(Id); else Selected.Remove(Id); RebuildRawFaders(); })[TSAVShowPanel::Text(Label)]];
		}
	}
	RebuildRawFaders();
}

TSharedRef<SWidget> STSAVLightingShowPanel::Fader(FName Attribute)
{
	FaderValues.FindOrAdd(Attribute);
	auto Send = [this, Attribute](float Value) { FaderValues.FindOrAdd(Attribute) = Value; if (auto* S = Show()) S->SetProgrammer(Selection(), {{Attribute, Value}}); };
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1)[TSAVShowPanel::Text(Attribute.ToString())]
		+ SHorizontalBox::Slot().FillWidth(2)[SNew(SSlider).PreventThrottling(true).Value_Lambda([this, Attribute] { return FaderValues.FindRef(Attribute); }).OnValueChanged_Lambda(Send)]
		+ SHorizontalBox::Slot().AutoWidth().Padding(5,0)[SNew(SBox).WidthOverride(64)[SNew(SNumericEntryBox<float>).MinValue(0).MaxValue(1).Value_Lambda([this, Attribute] { return FaderValues.FindRef(Attribute); }).OnValueChanged_Lambda(Send)]];
}

void STSAVLightingShowPanel::RebuildRawFaders()
{
	const auto Fixtures = Selection();
	const auto* Patch = Fixtures.IsEmpty() ? nullptr : Fixtures[0]->GetFixturePatch();
	const auto* Mode = Patch ? Patch->GetActiveMode() : nullptr;
	FString Signature;
	if (!Fixtures.IsEmpty()) Signature = ATSAVLightingShow::FixtureId(Fixtures[0]).ToString();
	if (Mode) for (const auto& Fn : Mode->Functions) Signature += Fn.Attribute.Name.ToString() + TEXT(";");
	const FIntPoint Dimensions=Fixtures.IsEmpty() ? FIntPoint::ZeroValue : Fixtures[0]->GetMatrixDimensions();
	Signature += FString::Printf(TEXT("%dx%d"),Dimensions.X,Dimensions.Y);
	if (Mode && Dimensions.X>0) for (const auto& Attribute : Mode->FixtureMatrixConfig.CellAttributes) Signature += Attribute.Attribute.Name.ToString();
	if (Signature == RawSignature) { RefreshReadout(); return; }
	RawSignature = Signature; RawFaders->ClearChildren();
	TSet<FName> Seen;
	if (Mode) for (const auto& Fn : Mode->Functions) if (!Fn.Attribute.Name.IsNone() && !Seen.Contains(Fn.Attribute.Name)) { Seen.Add(Fn.Attribute.Name); RawFaders->AddSlot().AutoHeight().Padding(0,3)[Fader(Fn.Attribute.Name)]; }
	if (Mode && Dimensions.X>0) for (int32 Y=0; Y<Dimensions.Y; ++Y) for (int32 X=0; X<Dimensions.X; ++X)
		for (const auto& Attribute : Mode->FixtureMatrixConfig.CellAttributes) RawFaders->AddSlot().AutoHeight().Padding(0,3)[Fader(ATSAVDMXFixture::MatrixAttributeKey(FIntPoint(X,Y),Attribute.Attribute.Name))];
	RefreshReadout();
}

TSharedRef<SWidget> STSAVLightingShowPanel::SlotSection(FName Kind, const FString& Title)
{
	SlotNumbers.FindOrAdd(Kind,1);
	auto List = SNew(SVerticalBox); SlotLists.Add(Kind, List);
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0,8)[TSAVShowPanel::Text(Title)]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1)[SNew(SNumericEntryBox<int32>).MinValue(1).MaxValue(99999).Value_Lambda([this,Kind] { return SlotNumbers[Kind]; }).OnValueChanged_Lambda([this,Kind](int32 Value) { SlotNumbers[Kind] = Value; })]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(Kind == TEXT("Recording") ? TEXT("Record") : TEXT("Store/replace"))).OnClicked_Lambda([this,Kind] { Store(Kind); return FReply::Handled(); })]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(Kind == TEXT("Cue") ? TEXT("GO") : Kind == TEXT("Effect") ? TEXT("Start") : Kind == TEXT("Recording") ? TEXT("Play") : TEXT("Recall"))).OnClicked_Lambda([this,Kind] { Recall(Kind); return FReply::Handled(); })]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("Delete"))).OnClicked_Lambda([this,Kind] { Delete(Kind); return FReply::Handled(); })]]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).MaxDesiredHeight(100)[SNew(SScrollBox)+SScrollBox::Slot()[List]]];
}

TSharedRef<SWidget> STSAVLightingShowPanel::TransportSection()
{
	using namespace TSAVShowPanel;
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[SlotSection(TEXT("Recording"),TEXT("NATIVE OUTPUT RECORDINGS"))]
		+ SVerticalBox::Slot().AutoHeight()[Number(TEXT("Samples / second (maximum)"),&RecordRate,1,60)]
		+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this] { auto* S=Show(false); return FText::FromString(S ? FString::Printf(TEXT("%s  %.2f s | %d attribute samples available"),S->IsRecording()?TEXT("Recording"):S->IsPlayingRecording()?TEXT("Playing"):TEXT("Stopped"),S->GetRecordingTime(),S->GetRecordingCapacity()) : FString()); }).AutoWrapText(true)]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Stop recording / keep take"))).OnClicked_Lambda([this] { if (auto* S=Show(false)) { if (S->IsRecording()) { const FString Before=RecordingBeforeState.IsEmpty()?S->CaptureTSAVState():RecordingBeforeState; S->EndRecording(); Remember(S,Before,true); RecordingBeforeState.Reset(); } } return FReply::Handled(); })]
			+ SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Release recording playback"))).OnClicked_Lambda([this] { if (auto* S=Show(false)) S->StopRecordingPlayback(); return FReply::Handled(); })]]
		+ SVerticalBox::Slot().AutoHeight().Padding(0,8)[Text(TEXT("TIMELINE / SEEK"))]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SCheckBox).IsChecked_Lambda([this] { const auto* S=Show(false); return S && S->GetData().bFollowEngineTimecode?ECheckBoxState::Checked:ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState State) { if (auto* S=Show()) S->SetExternalTimecode(State==ECheckBoxState::Checked,TimecodeOffset); })[Text(TEXT("Follow Unreal's synchronized timecode provider"))]]
		+ SVerticalBox::Slot().AutoHeight()[Number(TEXT("Timecode offset (seconds)"),&TimecodeOffset,-86400.0f,86400.0f)]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SButton).Text(FText::FromString(TEXT("Apply timecode offset"))).OnClicked_Lambda([this] { if (auto* S=Show()) S->SetExternalTimecode(S->GetData().bFollowEngineTimecode,TimecodeOffset); return FReply::Handled(); })]
		+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this] { const auto* S=Show(false); return FText::FromString(S && S->GetData().bFollowEngineTimecode?(S->IsExternalTimecodeReady()?TEXT("Timecode provider synchronized"):TEXT("Waiting for a synchronized Unreal timecode provider")):TEXT("Internal timeline clock")); })]
		+ SVerticalBox::Slot().AutoHeight()[Number(TEXT("Time (seconds)"),&TransportSeconds,0.0f,86400.0f)]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SButton).Text(FText::FromString(TEXT("Seek recording to time"))).OnClicked_Lambda([this] { if (auto* S=Show(false)) S->SeekRecording(TransportSeconds); return FReply::Handled(); })]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SButton).Text(FText::FromString(TEXT("Schedule selected cue number at time"))).OnClicked_Lambda([this] { if (auto* S=Show(false)) { const FString Before=S->CaptureTSAVState(); Remember(S,Before,S->StoreTimecodeEvent(TransportSeconds,SlotNumbers[TEXT("Cue")])); } return FReply::Handled(); })]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SButton).Text(FText::FromString(TEXT("Remove event at time"))).OnClicked_Lambda([this] { if (auto* S=Show(false)) { const FString Before=S->CaptureTSAVState(); Remember(S,Before,S->DeleteTimecodeEvent(TransportSeconds)); } return FReply::Handled(); })]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).MaxDesiredHeight(100)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(TimelineRows,SVerticalBox)]]]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Play timeline"))).OnClicked_Lambda([this] { if (auto* S=Show(false)) S->StartTimeline(); return FReply::Handled(); })]
			+ SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Stop timeline"))).OnClicked_Lambda([this] { if (auto* S=Show(false)) S->StopTimeline(); return FReply::Handled(); })]
			+ SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Seek timeline"))).OnClicked_Lambda([this] { if (auto* S=Show(false)) S->SeekTimeline(TransportSeconds); return FReply::Handled(); })]]
		+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this] { auto* S=Show(false); return FText::FromString(S ? FString::Printf(TEXT("Timeline %s — %.2f s"),S->IsTimelineRunning()?TEXT("running"):TEXT("stopped"),S->GetTimelineTime()) : FString()); })];
}

TSharedRef<SWidget> STSAVLightingShowPanel::ExecutorSection()
{
	using namespace TSAVShowPanel;
	return SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight().Padding(0,12)[Text(TEXT("INDEPENDENT CUE EXECUTORS"))]
		+SVerticalBox::Slot().AutoHeight()[Text(TEXT("Cue order (comma separated). Executor dimmers merge highest; latest GO wins other attributes. The programmer stays on top."))]
		+SVerticalBox::Slot().AutoHeight()[SNew(SEditableTextBox).Text_Lambda([this] { return FText::FromString(ExecutorCues); }).OnTextChanged_Lambda([this](const FText& V) { ExecutorCues=V.ToString(); })]
		+SVerticalBox::Slot().AutoHeight()[SNew(SCheckBox).IsChecked_Lambda([this] { return bExecutorLoop?ECheckBoxState::Checked:ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState V) { bExecutorLoop=V==ECheckBoxState::Checked; })[Text(TEXT("Loop executor sequence"))]]
		+SVerticalBox::Slot().AutoHeight()[SlotSection(TEXT("Executor"),TEXT("EXECUTORS — Recall runs the next cue"))]
		+SVerticalBox::Slot().AutoHeight()[SNew(SSlider).PreventThrottling(true).Value_Lambda([this] { auto* S=Show(false); return S?S->GetExecutorLevel(SlotNumbers[TEXT("Executor")]):1.0f; }).OnValueChanged_Lambda([this](float V) { if (auto* S=Show(false)) S->SetExecutorLevel(SlotNumbers[TEXT("Executor")],V); })]
		+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this] { auto* S=Show(false); const int32 Cue=S?S->GetExecutorCue(SlotNumbers[TEXT("Executor")]):INDEX_NONE; return FText::FromString(Cue==INDEX_NONE?TEXT("Selected executor stopped"):FString::Printf(TEXT("Selected executor: cue %d — %.0f%%"),Cue,S->GetExecutorLevel(SlotNumbers[TEXT("Executor")])*100)); })]
		+SVerticalBox::Slot().AutoHeight()[SNew(SButton).Text(FText::FromString(TEXT("Release selected executor"))).OnClicked_Lambda([this] { if (auto* S=Show(false)) S->StopExecutor(SlotNumbers[TEXT("Executor")]); return FReply::Handled(); })];
}

void STSAVLightingShowPanel::RefreshSlots()
{
	auto* S = Show(false);
	if (S == LastShow.Get() && S && S->GetRevision() == LastRevision) return;
	if (S != LastShow.Get()) { UndoStates.Reset(); RedoStates.Reset(); }
	LastShow = S; LastRevision = S ? S->GetRevision() : -1;
	for (auto& Pair : SlotLists) Pair.Value->ClearChildren();
	TimelineRows->ClearChildren();
	if (!S) return;
	TimecodeOffset=S->GetData().TimecodeOffsetSeconds;
	const auto Add = [this](FName Kind, int32 Number, const FString& Name) {
		SlotLists[Kind]->AddSlot().AutoHeight()[SNew(SButton).Text(FText::FromString(FString::Printf(TEXT("%d  %s"), Number, *Name)))
			.OnClicked_Lambda([this,Kind,Number,Name] { SlotNumbers[Kind] = Number; SlotName = Name; if (auto* S = Show(false)) {
				if (Kind == TEXT("Cue")) if (const auto* C = S->GetData().Cues.FindByPredicate([&](const auto& V) { return V.Number == Number; })) { Fade = C->FadeSeconds; Delay = C->DelaySeconds; Follow = C->FollowSeconds; bTrack = C->bTrack; }
				if (Kind == TEXT("Effect")) if (const auto* E = S->GetData().Effects.FindByPredicate([&](const auto& V) { return V.Number == Number; })) { EffectAttribute = E->Attribute.ToString(); Wave = static_cast<int32>(E->Wave); Period = E->PeriodSeconds; Minimum = E->Minimum; Maximum = E->Maximum; Spread = E->SpreadCycles; }
				if (Kind == TEXT("Executor")) if (const auto* E=S->GetData().Executors.FindByPredicate([&](const auto& V) { return V.Number==Number; })) { ExecutorCues.Reset(); for (int32 Cue : E->Cues) { if (!ExecutorCues.IsEmpty()) ExecutorCues+=TEXT(","); ExecutorCues+=FString::FromInt(Cue); } bExecutorLoop=E->bLoop; }
			} return FReply::Handled(); })];
	};
	for (const auto& V : S->GetData().Groups) Add(TEXT("Group"), V.Number, V.Name);
	for (const auto& V : S->GetData().Presets) Add(TEXT("Preset"), V.Number, V.Name);
	for (const auto& V : S->GetData().Cues) Add(TEXT("Cue"), V.Number, V.Name);
	for (const auto& V : S->GetData().Effects) Add(TEXT("Effect"), V.Number, V.Name);
	for (const auto& V : S->GetData().Recordings) Add(TEXT("Recording"), V.Number, V.Name);
	for (const auto& V : S->GetData().Executors) Add(TEXT("Executor"),V.Number,V.Name);
	for (const auto& E : S->GetData().Timecode) TimelineRows->AddSlot().AutoHeight()[SNew(SButton)
		.Text(FText::FromString(FString::Printf(TEXT("%.2f s  —  cue %d"),E.Seconds,E.CueNumber)))
		.OnClicked_Lambda([this,E] { TransportSeconds=E.Seconds; SlotNumbers[TEXT("Cue")]=E.CueNumber; return FReply::Handled(); })];
}

void STSAVLightingShowPanel::Remember(ATSAVLightingShow* Target, const FString& Before, bool bSuccess)
{
	RefreshSlots();
	if (bSuccess && Target) { UndoStates.Add(Before); RedoStates.Reset(); }
	Status(bSuccess ? TEXT("Show updated. Save the scene/project or show data to keep it.") : TEXT("Operation failed. Check the selection, slot number, timing or file."), bSuccess);
	RefreshSlots();
}
void STSAVLightingShowPanel::Store(FName Kind)
{
	auto* S = Show(); if (!S) return;
	const FString Before = S->CaptureTSAVState(); const auto Fixtures = Selection(); const int32 N = SlotNumbers[Kind]; bool OK = false;
	if (Kind == TEXT("Group")) OK = S->StoreGroup(N, SlotName, Fixtures);
	else if (Kind == TEXT("Preset")) OK = S->StorePreset(N, SlotName, Fixtures,static_cast<ETSAVLightingStoreScope>(StoreScope));
	else if (Kind == TEXT("Cue")) OK = S->StoreCue(N, SlotName, Fixtures, Fade, Delay, Follow, bTrack,static_cast<ETSAVLightingStoreScope>(StoreScope));
	else if (Kind == TEXT("Recording")) { OK=S->BeginRecording(N,SlotName,Fixtures,RecordRate); if (OK) RecordingBeforeState=Before; Status(OK ? TEXT("Recording selected native fixture output before master/blackout. Stop recording to keep this take.") : TEXT("Could not start recording. Select fixtures or stop the active take."),OK); return; }
	else if (Kind == TEXT("Executor")) { FTSAVLightingExecutor E; E.Number=N; E.Name=SlotName; E.bLoop=bExecutorLoop; TArray<FString> Parts; ExecutorCues.ParseIntoArray(Parts,TEXT(","),true); for (const auto& Part : Parts) { int32 Cue=0; if (!LexTryParseString(Cue,*Part.TrimStartAndEnd())) { Status(TEXT("Enter cue numbers separated by commas."),false); return; } E.Cues.Add(Cue); } OK=S->StoreExecutor(E); }
	else {
		FTSAVLightingEffect E; E.Number = N; E.Name = SlotName; E.Attribute = FName(*EffectAttribute); E.Wave = static_cast<ETSAVLightingWave>(Wave); E.PeriodSeconds = Period; E.Minimum = Minimum; E.Maximum = Maximum; E.SpreadCycles = Spread;
		for (auto* F : Fixtures) E.Fixtures.Add(ATSAVLightingShow::FixtureId(F));
		OK = !Fixtures.IsEmpty() && S->StoreEffect(E);
	}
	Remember(S, Before, OK);
}
void STSAVLightingShowPanel::Recall(FName Kind)
{
	auto* S = Show(false); if (!S) return; bool OK = false; const int32 N = SlotNumbers[Kind];
	if (Kind == TEXT("Group")) {
		if (const auto* Group = S->GetData().Groups.FindByPredicate([&](const auto& V) { return V.Number == N; })) { Selected.Reset(); for (const auto& Id : Group->Fixtures) if (S->ResolveFixture(Id)) Selected.Add(Id); RebuildRawFaders(); OK = true; }
	} else if (Kind == TEXT("Preset")) OK = S->RecallPreset(N);
	else if (Kind == TEXT("Cue")) OK = S->Go(N);
	else if (Kind == TEXT("Recording")) OK = S->PlayRecording(N);
	else if (Kind == TEXT("Executor")) OK=S->GoExecutor(N);
	else OK = S->StartEffect(N);
	Status(OK ? TEXT("Slot recalled. Programmer values override cue/effect playback until released.") : TEXT("That slot does not exist."), OK);
}
void STSAVLightingShowPanel::Delete(FName Kind) { if (auto* S = Show(false)) { const FString Before = S->CaptureTSAVState(); Remember(S, Before, S->DeleteSlot(Kind, SlotNumbers[Kind])); } }
void STSAVLightingShowPanel::UndoShow(bool bRedo)
{
	auto* S = Show(false); auto& From = bRedo ? RedoStates : UndoStates; auto& To = bRedo ? UndoStates : RedoStates;
	if (!S || From.IsEmpty()) return;
	const FString Current = S->CaptureTSAVState(); const FString State = From.Last();
	if (S->RestoreTSAVState(State)) { From.Pop(); To.Add(Current); RefreshSlots(); }
}

void STSAVLightingShowPanel::PatchSelection()
{
	if (!World.IsValid()) return;
	using namespace TSAVDMXPatchPlan;
	const auto Fixtures = Selection(); TArray<FRange> SelectedRanges, Occupied, Plan;
	TMap<FName, ATSAVDMXFixture*> Actors;
	for (TActorIterator<ATSAVDMXFixture> It(World.Get()); It; ++It) {
		auto* Patch = It->GetFixturePatch(); if (!Patch) continue;
		const FGuid Id = ATSAVLightingShow::FixtureId(*It); const FName Key(*Id.ToString());
		FRange Range{Key, It->GetActorNameOrLabel(), Patch->GetUniverseID(), Patch->GetStartingChannel(), Patch->GetChannelSpan()};
		Occupied.Add(Range); Actors.Add(Key, *It);
	}
	for (auto* Fixture : Fixtures) {
		auto* Patch = Fixture->GetFixturePatch();
		if (!Patch) { Status(TEXT("A selected fixture has no DMX mode or patch."), false); return; }
		SelectedRanges.Add({FName(*ATSAVLightingShow::FixtureId(Fixture).ToString()), Fixture->GetActorNameOrLabel(), Patch->GetUniverseID(), Patch->GetStartingChannel(), Patch->GetChannelSpan()});
	}
	FString Error; if (!Build(SelectedRanges, Occupied, Universe, Address, Plan, Error)) { Status(Error, false); return; }
#if WITH_EDITOR
	const FScopedTransaction Transaction(FText::FromString(TEXT("Patch TSAV show fixtures")));
#endif
	TArray<FString> Before; for (auto* Fixture : Fixtures) Before.Add(Fixture->CaptureTSAVState());
	for (const auto& Range : Plan) if (!Actors[Range.Id]->SetIndividualPatchAddress(Range.Universe, Range.Address)) {
		for (int32 I=0; I<Fixtures.Num(); ++I) Fixtures[I]->RestoreTSAVState(Before[I]);
		Status(TEXT("Patch rejected; the fixture states were restored."), false); return;
	}
	if (UGameInstance* GI = World->GetGameInstance()) if (auto* Commands = GI->GetSubsystem<UTSAVCommandSubsystem>()) {
		TArray<AActor*> ChangedActors; for (auto* Fixture : Fixtures) ChangedActors.Add(Fixture);
		Commands->CommitAppliedActorStates(ChangedActors,Before,FText::FromString(TEXT("Patch lighting fixtures")));
	}
	Status(TEXT("Fixtures patched. Save the scene/project to keep the addresses.")); RefreshFixtures();
}
void STSAVLightingShowPanel::Status(const FString& Message, bool bSuccess) { StatusMessage = Message; bStatusOK = bSuccess; }
void STSAVLightingShowPanel::Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime)
{
	SCompoundWidget::Tick(Geometry,CurrentTime,DeltaTime); RefreshTime += DeltaTime;
	if (RefreshTime < 0.5f) return;
	RefreshTime=0; RefreshFixtures(); RefreshSlots();
}

void STSAVLightingShowPanel::RefreshReadout()
{
	const auto Fixtures=Selection(); if (Fixtures.IsEmpty()) return;
	TMap<FName,float> Readout;
	if (const auto* S=Show(false)) { const auto Looks=S->Capture({Fixtures[0]}); if (!Looks.IsEmpty()) Readout=Looks[0].Attributes; }
	else for (const auto& Pair : Fixtures[0]->GetAttributeValues().Map) Readout.Add(ATSAVLightingShow::CanonicalAttribute(Pair.Key.Name),Pair.Value);
	for (auto& Fader : FaderValues) if (const float* Value=Readout.Find(ATSAVLightingShow::CanonicalAttribute(Fader.Key))) Fader.Value=*Value;
}
