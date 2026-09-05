// Copyright TSAV. All Rights Reserved.
#include "Lighting/TSAVLightingShow.h"
#include "TSAVDMXFixture.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "Project/TSAVProjectSubsystem.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/TimecodeProvider.h"
#include "EngineUtils.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace TSAVShow
{
	template<typename T> bool Put(TArray<T>& Items, const T& Item)
	{
		if (Item.Number < 1) return false;
		if (T* Existing = Items.FindByPredicate([&](const T& Value) { return Value.Number == Item.Number; })) *Existing = Item;
		else Items.Add(Item);
		Items.Sort([](const T& A, const T& B) { return A.Number < B.Number; });
		return true;
	}
	TMap<FName, float> Read(ATSAVDMXFixture* Fixture)
	{
		TMap<FName, float> Result;
		for (const auto& Pair : Fixture->GetAttributeValues().Map) Result.Add(ATSAVLightingShow::CanonicalAttribute(Pair.Key.Name), Pair.Value);
		return Result;
	}
	void Overlay(TMap<FName, float>& To, const TMap<FName, float>& From)
	{
		for (const auto& Pair : From) To.Add(Pair.Key, Pair.Value);
	}
}

ATSAVLightingShow::ATSAVLightingShow()
{
	PrimaryActorTick.bCanEverTick = true;
	// Resolve local programmer/master priority after fixture DMX component updates.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	bAllowTickBeforeBeginPlay = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	CreateDefaultSubobject<UTSAVSceneObjectComponent>(TEXT("ShowIdentity"));
}

ATSAVLightingShow* ATSAVLightingShow::Find(UWorld* World, bool bCreate)
{
	if (!World) return nullptr;
	for (TActorIterator<ATSAVLightingShow> It(World); It; ++It) return *It;
	if (!bCreate) return nullptr;
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transactional;
	ATSAVLightingShow* Show = World->SpawnActor<ATSAVLightingShow>(StaticClass(), FTransform::Identity, Params);
	if (Show)
	{
		auto* Identity = Show->FindComponentByClass<UTSAVSceneObjectComponent>();
		Identity->DisplayName = FText::FromString(TEXT("TSAV Lighting Show"));
#if WITH_EDITOR
		Show->SetActorLabel(TEXT("TSAV Lighting Show"));
#endif
	}
	return Show;
}

FGuid ATSAVLightingShow::FixtureId(ATSAVDMXFixture* Fixture, bool bCreate)
{
	if (!IsValid(Fixture)) return {};
	auto* Identity = Fixture->FindComponentByClass<UTSAVSceneObjectComponent>();
	if (!Identity && bCreate)
	{
		Fixture->Modify();
		Identity = UTSAVSceneObjectComponent::EnsureForActor(Fixture);
		Identity->ObjectType = ETSAVObjectType::Fixture;
		Identity->DisplayName = FText::FromString(Fixture->GetActorNameOrLabel());
	}
	return Identity ? Identity->ObjectId : FGuid();
}

FName ATSAVLightingShow::CanonicalAttribute(FName Name)
{
	FIntPoint Cell; FName CellAttribute;
	if (ATSAVDMXFixture::ParseMatrixAttributeKey(Name,Cell,CellAttribute)) return ATSAVDMXFixture::MatrixAttributeKey(Cell,CellAttribute);
	FString Text = Name.ToString().ToLower().Replace(TEXT("_"), TEXT("")).Replace(TEXT("-"), TEXT("")).Replace(TEXT(" "), TEXT(""));
	if (Text == TEXT("intensity") || Text == TEXT("masterdimmer")) Text = TEXT("dimmer");
	if (Text == TEXT("coloraddr") || Text == TEXT("colorrgbred")) Text = TEXT("red");
	if (Text == TEXT("coloraddg") || Text == TEXT("colorrgbgreen")) Text = TEXT("green");
	if (Text == TEXT("coloraddb") || Text == TEXT("colorrgbblue")) Text = TEXT("blue");
	if (Text == TEXT("beamangle")) Text = TEXT("zoom");
	if (Text == TEXT("color1")) Text = TEXT("colorwheel");
	if (Text == TEXT("gobo1")) Text = TEXT("gobo");
	if (Text == TEXT("gobo1pos")) Text = TEXT("goborotation");
	if (Text == TEXT("shutter1")) Text = TEXT("shutter");
	if (Text == TEXT("frost1")) Text = TEXT("frost");
	if (Text == TEXT("colorsubc")) Text = TEXT("cyan");
	if (Text == TEXT("colorsubm")) Text = TEXT("magenta");
	if (Text == TEXT("colorsuby")) Text = TEXT("yellow");
	return FName(*Text);
}

ATSAVDMXFixture* ATSAVLightingShow::ResolveFixture(FGuid Id) const
{
	if (!Id.IsValid() || !GetWorld()) return nullptr;
	for (TActorIterator<ATSAVDMXFixture> It(GetWorld()); It; ++It)
		if (FixtureId(*It, false) == Id) return *It;
	return nullptr;
}

void ATSAVLightingShow::Changed()
{
	MarkPackageDirty();
	++Revision;
	if (UGameInstance* GI = GetGameInstance())
		if (auto* Project = GI->GetSubsystem<UTSAVProjectSubsystem>()) Project->MarkDirty();
}

void ATSAVLightingShow::Touch(ATSAVDMXFixture* Fixture)
{
	const FGuid Id = FixtureId(Fixture);
	if (!Baseline.Contains(Id)) Baseline.Add(Id, {Id, TSAVShow::Read(Fixture)});
}

TArray<FTSAVFixtureLook> ATSAVLightingShow::Capture(const TArray<ATSAVDMXFixture*>& Fixtures) const
{
	TArray<FTSAVFixtureLook> Values;
	TSet<FGuid> Seen;
	for (auto* Fixture : Fixtures)
	{
		const FGuid Id = FixtureId(Fixture);
		if (Id.IsValid() && !Seen.Contains(Id)) { Seen.Add(Id); Values.Add({Id, CurrentBase(Fixture)}); }
	}
	return Values;
}

TMap<FName, float> ATSAVLightingShow::CurrentBase(ATSAVDMXFixture* Fixture) const
{
	const FGuid Id = FixtureId(Fixture, false);
	TMap<FName, float> Values = Baseline.Contains(Id) ? Baseline[Id].Attributes : TSAVShow::Read(Fixture);
	if (const auto* Look = Playback.Find(Id)) TSAVShow::Overlay(Values, Look->Attributes);
	OverlayExecutors(Id,Values);
	if (const auto* Look = Programmer.Find(Id)) TSAVShow::Overlay(Values, Look->Attributes);
	return Values;
}

void ATSAVLightingShow::SetProgrammer(const TArray<ATSAVDMXFixture*>& Fixtures, const TMap<FName, float>& Attributes)
{
	for (auto* Fixture : Fixtures)
	{
		if (!IsValid(Fixture)) continue;
		Touch(Fixture);
		const FGuid Id = FixtureId(Fixture);
		auto& Look = Programmer.FindOrAdd(Id);
		Look.FixtureId = Id;
		for (const auto& Pair : Attributes)
			if (!Pair.Key.IsNone() && FMath::IsFinite(Pair.Value)) Look.Attributes.Add(CanonicalAttribute(Pair.Key), FMath::Clamp(Pair.Value, 0.0f, 1.0f));
	}
	Flush();
}

void ATSAVLightingShow::ClearProgrammer() { Programmer.Reset(); Flush(); }

bool ATSAVLightingShow::StoreGroup(int32 Number, const FString& Name, const TArray<ATSAVDMXFixture*>& Fixtures)
{
	if (Number < 1 || Fixtures.IsEmpty()) return false;
	FTSAVLightingGroup Group; Group.Number = Number; Group.Name = Name;
	for (auto* Fixture : Fixtures) { const FGuid Id = FixtureId(Fixture); if (Id.IsValid()) Group.Fixtures.AddUnique(Id); }
	if (Group.Fixtures.IsEmpty()) return false;
	Modify(); TSAVShow::Put(Data.Groups, Group); Changed(); return true;
}

TArray<FTSAVFixtureLook> ATSAVLightingShow::CaptureForStore(const TArray<ATSAVDMXFixture*>& Fixtures,ETSAVLightingStoreScope Scope) const
{
	if (static_cast<uint8>(Scope)>static_cast<uint8>(ETSAVLightingStoreScope::Beam)) return {};
	auto Looks=Capture(Fixtures);
	for (auto& Look : Looks) {
		if (Scope==ETSAVLightingStoreScope::Programmer) { const auto* Values=Programmer.Find(Look.FixtureId); Look.Attributes=Values?Values->Attributes:TMap<FName,float>(); continue; }
		if (Scope==ETSAVLightingStoreScope::All) continue;
		for (auto It=Look.Attributes.CreateIterator(); It; ++It) {
			FName Key=CanonicalAttribute(It.Key()); FIntPoint Cell; FName CellAttribute; if (ATSAVDMXFixture::ParseMatrixAttributeKey(Key,Cell,CellAttribute)) Key=CellAttribute;
			const bool bColor=Key==TEXT("red") || Key==TEXT("green") || Key==TEXT("blue") || Key==TEXT("white") || Key==TEXT("amber") || Key==TEXT("cyan") || Key==TEXT("magenta") || Key==TEXT("yellow") || Key==TEXT("cto") || Key==TEXT("colorwheel");
			const bool bPosition=Key==TEXT("pan") || Key==TEXT("tilt"),bDimmer=Key==TEXT("dimmer");
			const bool Keep=Scope==ETSAVLightingStoreScope::Color?bColor:Scope==ETSAVLightingStoreScope::Position?bPosition:Scope==ETSAVLightingStoreScope::Dimmer?bDimmer:!bColor && !bPosition && !bDimmer;
			if (!Keep) It.RemoveCurrent();
		}
	}
	Looks.RemoveAll([](const auto& Look) { return Look.Attributes.IsEmpty(); }); return Looks;
}

bool ATSAVLightingShow::StorePreset(int32 Number, const FString& Name, const TArray<ATSAVDMXFixture*>& Fixtures,ETSAVLightingStoreScope Scope)
{
	FTSAVLightingPreset Preset; Preset.Number = Number; Preset.Name = Name; Preset.Values = CaptureForStore(Fixtures,Scope);
	if (Number < 1 || Preset.Values.IsEmpty()) return false;
	Modify(); TSAVShow::Put(Data.Presets, Preset); Changed(); return true;
}

bool ATSAVLightingShow::RecallPreset(int32 Number)
{
	const auto* Preset = Data.Presets.FindByPredicate([&](const auto& Item) { return Item.Number == Number; });
	if (!Preset) return false;
	TMap<FGuid,ATSAVDMXFixture*> Fixtures;
	for (TActorIterator<ATSAVDMXFixture> It(GetWorld()); It; ++It) Fixtures.Add(FixtureId(*It,false),*It);
	for (const auto& Look : Preset->Values)
		if (auto* Fixture = Fixtures.FindRef(Look.FixtureId)) {
			Touch(Fixture); auto& Target=Programmer.FindOrAdd(Look.FixtureId); Target.FixtureId=Look.FixtureId;
			TSAVShow::Overlay(Target.Attributes,Look.Attributes);
		}
	Flush();
	return true;
}

bool ATSAVLightingShow::StoreCue(int32 Number, const FString& Name, const TArray<ATSAVDMXFixture*>& Fixtures, float Fade, float Delay, float Follow, bool bTrack,ETSAVLightingStoreScope Scope)
{
	FTSAVLightingCue Cue; Cue.Number = Number; Cue.Name = Name; Cue.Values = CaptureForStore(Fixtures,Scope);
	Cue.FadeSeconds = Fade; Cue.DelaySeconds = Delay; Cue.FollowSeconds = Follow; Cue.bTrack = bTrack;
	FTSAVLightingShowData Candidate = Data;
	if (Cue.Values.IsEmpty() || !TSAVShow::Put(Candidate.Cues, Cue)) return false;
	FString Error; if (!Validate(Candidate, Error)) return false;
	Modify(); Data = MoveTemp(Candidate); Changed(); return true;
}

bool ATSAVLightingShow::StoreEffect(const FTSAVLightingEffect& Effect)
{
	FTSAVLightingShowData Candidate = Data;
	if (!TSAVShow::Put(Candidate.Effects, Effect)) return false;
	FString Error; if (!Validate(Candidate, Error)) return false;
	Modify(); Data = MoveTemp(Candidate); Changed(); return true;
}

bool ATSAVLightingShow::DeleteSlot(FName Kind, int32 Number)
{
	Modify(); int32 Removed = 0;
	const auto Match = [&](const auto& Item) { return Item.Number == Number; };
	if (Kind == TEXT("Group")) Removed = Data.Groups.RemoveAll(Match);
	else if (Kind == TEXT("Preset")) Removed = Data.Presets.RemoveAll(Match);
	else if (Kind == TEXT("Cue")) { if (CurrentCue == Number) Stop(); Removed = Data.Cues.RemoveAll(Match); Data.Timecode.RemoveAll([&](const auto& Event) { return Event.CueNumber == Number; }); }
	else if (Kind == TEXT("Effect")) { ActiveEffects.Remove(Number); Removed = Data.Effects.RemoveAll(Match); Flush(); }
	else if (Kind == TEXT("Recording")) { if (PlayingRecording == Number) StopRecordingPlayback(); Removed = Data.Recordings.RemoveAll(Match); }
	else if (Kind == TEXT("Executor")) { StopExecutor(Number); Removed=Data.Executors.RemoveAll(Match); }
	if (Kind==TEXT("Cue") && Removed) { for (auto& Executor : Data.Executors) Executor.Cues.Remove(Number); Data.Executors.RemoveAll([](const auto& E) { return E.Cues.IsEmpty(); }); ExecutorPlayback.Reset(); }
	if (Removed) Changed(); return Removed > 0;
}

bool ATSAVLightingShow::Go(int32 Number)
{
	const int32 Index = Data.Cues.IndexOfByPredicate([&](const auto& Item) { return Item.Number == Number; });
	if (Index == INDEX_NONE) return false;
	// Resolve tracking from the start of the stack, independent of how the user jumped here.
	TMap<FGuid, FTSAVFixtureLook> Target;
	for (int32 I = 0; I <= Index; ++I)
	{
		if (!Data.Cues[I].bTrack) Target.Reset();
		for (const auto& Look : Data.Cues[I].Values)
		{
			auto& Value = Target.FindOrAdd(Look.FixtureId); Value.FixtureId = Look.FixtureId;
			TSAVShow::Overlay(Value.Attributes, Look.Attributes);
		}
	}
	for (const auto& Pair : Target) if (auto* Fixture = ResolveFixture(Pair.Key)) Touch(Fixture);
	FadeFrom.Reset(); FadeTo = Baseline;
	for (const auto& Pair : Baseline)
	{
		FTSAVFixtureLook From = Pair.Value;
		if (const auto* Previous = Playback.Find(Pair.Key)) TSAVShow::Overlay(From.Attributes, Previous->Attributes);
		FadeFrom.Add(Pair.Key, MoveTemp(From));
	}
	for (const auto& Pair : Target) TSAVShow::Overlay(FadeTo.FindOrAdd(Pair.Key).Attributes, Pair.Value.Attributes);
	CurrentCue = Number; CueTime = 0.0f; bPaused = false;
	EvaluateCue(); Flush(); return true;
}

bool ATSAVLightingShow::Next()
{
	for (const auto& Cue : Data.Cues) if (CurrentCue == INDEX_NONE || Cue.Number > CurrentCue) return Go(Cue.Number);
	return bLoop && !Data.Cues.IsEmpty() ? Go(Data.Cues[0].Number) : false;
}

bool ATSAVLightingShow::Previous()
{
	for (int32 I = Data.Cues.Num() - 1; I >= 0; --I) if (Data.Cues[I].Number < CurrentCue) return Go(Data.Cues[I].Number);
	return false;
}

void ATSAVLightingShow::Stop()
{
	bTimeline = false; bRecordingPlayback = false; RecordedPlayback.Reset(); PlayingRecording = INDEX_NONE;
	CurrentCue = INDEX_NONE; CueTime = 0; Playback.Reset(); FadeFrom.Reset(); FadeTo.Reset(); ActiveEffects.Reset(); bPaused = false;
	ExecutorPlayback.Reset(); Flush();
}

void ATSAVLightingShow::SetMaster(float Value) { if (FMath::IsFinite(Value)) Master = FMath::Clamp(Value, 0.0f, 1.0f); Flush(); }
void ATSAVLightingShow::SetBlackout(bool bValue) { bBlackout = bValue; Flush(); }

bool ATSAVLightingShow::StartEffect(int32 Number)
{
	const auto* Effect = Data.Effects.FindByPredicate([&](const auto& Item) { return Item.Number == Number; });
	if (!Effect) return false;
	for (const auto& Id : Effect->Fixtures) if (auto* Fixture = ResolveFixture(Id)) Touch(Fixture);
	ActiveEffects.AddUnique(Number); Flush(); return true;
}
void ATSAVLightingShow::StopEffects() { ActiveEffects.Reset(); Flush(); }

void ATSAVLightingShow::EvaluateCue()
{
	const auto* Cue = Data.Cues.FindByPredicate([&](const auto& Item) { return Item.Number == CurrentCue; });
	if (!Cue) return;
	const float Alpha = CueTime < Cue->DelaySeconds ? 0.0f : Cue->FadeSeconds <= 0.0f ? 1.0f : FMath::Clamp((CueTime - Cue->DelaySeconds) / Cue->FadeSeconds, 0.0f, 1.0f);
	Playback = FadeTo;
	for (auto& Pair : Playback)
	{
		Pair.Value.FixtureId = Pair.Key;
		const auto* From=FadeFrom.Find(Pair.Key);
		for (auto& Attr : Pair.Value.Attributes)
			Attr.Value = FMath::Lerp(From ? From->Attributes.FindRef(Attr.Key) : 0.0f, Attr.Value, Alpha);
	}
}

void ATSAVLightingShow::Seek(float Seconds)
{
	if (!FMath::IsFinite(Seconds)) return;
	CueTime = FMath::Max(0.0f, Seconds); EvaluateCue(); Flush();
}

void ATSAVLightingShow::Advance(float DeltaSeconds)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f) return;
	if (!bPaused)
	{
		EffectTime += DeltaSeconds;
		if (bTimeline) AdvanceTimeline(DeltaSeconds);
		else
		{
			CueTime += DeltaSeconds; EvaluateCue();
			// Carry elapsed time through positive-duration follows. Bound instant loops.
			for (int32 Transitions=0; Transitions<128; ++Transitions)
			{
				const auto* Cue = Data.Cues.FindByPredicate([&](const auto& Item) { return Item.Number == CurrentCue; });
				if (!Cue || Cue->FollowSeconds < 0) break;
				const float Duration = Cue->DelaySeconds + Cue->FadeSeconds + Cue->FollowSeconds;
				if (CueTime < Duration) break;
				const float Remainder = CueTime - Duration;
				if (!Next()) break;
				CueTime = Remainder; EvaluateCue();
				if (Duration <= 0) break;
			}
		}
		if (bRecordingPlayback) { RecordingPlaybackTime += DeltaSeconds; EvaluateRecording(); }
		AdvanceExecutors(DeltaSeconds);
	}
	Flush();
	if (bRecording)
	{
		RecordingTime += DeltaSeconds;
		if (RecordingBuffer.Frames.IsEmpty() || RecordingTime - RecordingBuffer.Frames.Last().Seconds >= 1.0f/RecordingRate) CaptureRecordingFrame();
	}
}

void ATSAVLightingShow::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bTimeline && Data.bFollowEngineTimecode) {
		if (IsExternalTimecodeReady()) FollowExternalTimecode(GEngine->GetTimecodeProvider()->GetDelayedQualifiedFrameTime().AsSeconds());
		Flush(); // Hold the evaluated look and programmer even between provider frames.
		return;
	}
	if (CurrentCue != INDEX_NONE || !ActiveEffects.IsEmpty() || !ExecutorPlayback.IsEmpty() || !Programmer.IsEmpty()
		|| (Baseline.Num()>0 && (bBlackout || Master<1)) || bRecording || bRecordingPlayback || bTimeline) Advance(DeltaSeconds);
}

void ATSAVLightingShow::SetExternalTimecode(bool bEnabled,double OffsetSeconds)
{
	if (!FMath::IsFinite(OffsetSeconds) || FMath::Abs(OffsetSeconds)>86400) return;
	Modify(); Data.bFollowEngineTimecode=bEnabled; Data.TimecodeOffsetSeconds=OffsetSeconds; Changed();
}
bool ATSAVLightingShow::IsExternalTimecodeReady() const { const auto* Provider=GEngine?GEngine->GetTimecodeProvider():nullptr; return Provider && Provider->GetSynchronizationState()==ETimecodeProviderSynchronizationState::Synchronized && Provider->GetFrameRate().IsValid(); }
void ATSAVLightingShow::FollowExternalTimecode(double Seconds)
{
	if (!bTimeline || !Data.bFollowEngineTimecode || !FMath::IsFinite(Seconds)) return;
	const float Target=FMath::Clamp(static_cast<float>(Seconds+Data.TimecodeOffsetSeconds),0.0f,86400.0f);
	const float Delta=Target-TimelineTime;
	if (Delta<0 || Delta>0.5f) SeekTimeline(Target);
	else if (Delta>0) Advance(Delta);
}

void ATSAVLightingShow::Flush()
{
	if (!GetWorld()) return;
	TMap<FGuid, ATSAVDMXFixture*> Fixtures;
	for (TActorIterator<ATSAVDMXFixture> It(GetWorld()); It; ++It) Fixtures.Add(FixtureId(*It, false), *It);
	for (const auto& Pair : Baseline)
	{
		ATSAVDMXFixture* Fixture = Fixtures.FindRef(Pair.Key); if (!Fixture) continue;
		TMap<FName, float> Values = Pair.Value.Attributes;
		if (const auto* Look = Playback.Find(Pair.Key)) TSAVShow::Overlay(Values, Look->Attributes);
		OverlayExecutors(Pair.Key,Values);
		for (int32 Number : ActiveEffects)
		{
			const auto* Effect = Data.Effects.FindByPredicate([&](const auto& Item) { return Item.Number == Number; });
			if (!Effect) continue;
			const int32 Index = Effect->Fixtures.IndexOfByKey(Pair.Key); if (Index == INDEX_NONE) continue;
			const double Phase = EffectTime / Effect->PeriodSeconds + (Effect->Fixtures.Num() > 1 ? Effect->SpreadCycles * Index / (Effect->Fixtures.Num() - 1) : 0.0);
			const float P = Phase - FMath::FloorToDouble(Phase);
			float Value = P;
			switch (Effect->Wave) {
			case ETSAVLightingWave::Sine: Value = 0.5f - 0.5f * FMath::Cos(P * 2 * PI); break;
			case ETSAVLightingWave::Triangle: Value = 1.0f - FMath::Abs(2 * P - 1); break;
			case ETSAVLightingWave::Square: Value = P < 0.5f ? 0.0f : 1.0f; break;
			default: break;
			}
			Values.Add(CanonicalAttribute(Effect->Attribute), FMath::Lerp(Effect->Minimum, Effect->Maximum, Value));
		}
		if (const auto* Look = RecordedPlayback.Find(Pair.Key)) TSAVShow::Overlay(Values, Look->Attributes);
		if (const auto* Look = Programmer.Find(Pair.Key)) TSAVShow::Overlay(Values, Look->Attributes);
		LastOutput.Add(Pair.Key, {Pair.Key, Values});
		Values.FindOrAdd(TEXT("dimmer")) *= bBlackout ? 0.0f : Master;
		FDMXNormalizedAttributeValueMap Input;
		for (const auto& Attr : Values) Input.Map.Add(FDMXAttributeName(Attr.Key), Attr.Value);
		Fixture->ApplyAttributeValues(Input);
		if (bNetworkOutput)
		{
			UDMXEntityFixturePatch* Patch = Fixture->GetFixturePatch();
			const FDMXFixtureMode* Mode = Patch ? Patch->GetActiveMode() : nullptr;
			if (!Mode) continue;
			TMap<FDMXAttributeName, int32> DMX;
			for (const auto& Function : Mode->Functions)
			{
				if (const float* Value = Values.Find(CanonicalAttribute(Function.Attribute.Name)))
				{
					const uint64 Max = Function.GetNumChannels() >= 4 ? MAX_uint32 : (1ULL << (8 * Function.GetNumChannels())) - 1;
					DMX.Add(Function.Attribute, static_cast<int32>(static_cast<uint32>(FMath::RoundToInt64(*Value * Max))));
				}
			}
			Patch->SendDMX(DMX);
			if (Mode->bFixtureMatrixEnabled) for (const auto& CellPair : Values) {
				FIntPoint Cell; FName Attribute;
				if (!ATSAVDMXFixture::ParseMatrixAttributeKey(CellPair.Key,Cell,Attribute)) continue;
				for (const auto& Entry : Mode->FixtureMatrixConfig.CellAttributes) if (CanonicalAttribute(Entry.Attribute.Name)==Attribute) Patch->SendNormalizedMatrixCellValue(Cell,Entry.Attribute,CellPair.Value);
			}
		}
	}
}

bool ATSAVLightingShow::Validate(const FTSAVLightingShowData& Candidate, FString& Error)
{
	const auto Fail = [&](const TCHAR* Message) { Error = Message; return false; };
	if (Candidate.Version != 1) return Fail(TEXT("Unsupported lighting show version."));
	if (!FMath::IsFinite(Candidate.TimecodeOffsetSeconds) || FMath::Abs(Candidate.TimecodeOffsetSeconds)>86400) return Fail(TEXT("Invalid timecode offset."));
	const auto SlotsValid = [](const auto& Items) { TSet<int32> Seen; for (const auto& Item : Items) { if (Item.Number < 1 || Seen.Contains(Item.Number)) return false; Seen.Add(Item.Number); } return true; };
	if (!SlotsValid(Candidate.Groups) || !SlotsValid(Candidate.Presets) || !SlotsValid(Candidate.Cues) || !SlotsValid(Candidate.Effects) || !SlotsValid(Candidate.Recordings)) return Fail(TEXT("Slot numbers must be positive and unique within each collection."));
	if (!SlotsValid(Candidate.Executors)) return Fail(TEXT("Executor numbers must be positive and unique."));
	for (const auto& Executor : Candidate.Executors) {
		if (Executor.Cues.IsEmpty()) return Fail(TEXT("An executor needs at least one cue."));
		for (int32 Number : Executor.Cues) if (!Candidate.Cues.ContainsByPredicate([&](const auto& Cue) { return Cue.Number==Number; })) return Fail(TEXT("Executor references an unavailable cue."));
	}
	const auto LooksValid = [](const TArray<FTSAVFixtureLook>& Looks) { TSet<FGuid> Seen; for (const auto& Look : Looks) { if (!Look.FixtureId.IsValid() || Seen.Contains(Look.FixtureId)) return false; Seen.Add(Look.FixtureId); for (const auto& Attr : Look.Attributes) if (Attr.Key.IsNone() || !FMath::IsFinite(Attr.Value) || Attr.Value < 0 || Attr.Value > 1) return false; } return true; };
	const auto IdsValid = [](const TArray<FGuid>& Ids) { TSet<FGuid> Seen; for (const auto& Id : Ids) { if (!Id.IsValid() || Seen.Contains(Id)) return false; Seen.Add(Id); } return !Ids.IsEmpty(); };
	for (const auto& Group : Candidate.Groups) if (!IdsValid(Group.Fixtures)) return Fail(TEXT("Group fixture IDs must be valid and unique."));
	for (const auto& Preset : Candidate.Presets) if (!LooksValid(Preset.Values)) return Fail(TEXT("Invalid preset values."));
	for (const auto& Cue : Candidate.Cues)
		if (!LooksValid(Cue.Values) || !FMath::IsFinite(Cue.FadeSeconds) || !FMath::IsFinite(Cue.DelaySeconds) || !FMath::IsFinite(Cue.FollowSeconds)
			|| Cue.FadeSeconds < 0 || Cue.DelaySeconds < 0 || Cue.FollowSeconds < -1) return Fail(TEXT("Invalid cue values or timing."));
	for (const auto& Effect : Candidate.Effects)
	{
		if (Effect.Attribute.IsNone() || !FMath::IsFinite(Effect.PeriodSeconds) || Effect.PeriodSeconds < 0.01f
			|| !FMath::IsFinite(Effect.Minimum) || !FMath::IsFinite(Effect.Maximum) || Effect.Minimum < 0 || Effect.Maximum > 1 || Effect.Minimum > Effect.Maximum
			|| !FMath::IsFinite(Effect.SpreadCycles) || static_cast<uint8>(Effect.Wave) > static_cast<uint8>(ETSAVLightingWave::Saw)) return Fail(TEXT("Invalid effect parameters."));
		if (!IdsValid(Effect.Fixtures)) return Fail(TEXT("Effect fixture IDs must be valid and unique."));
	}
	int64 SampleCount = 0;
	for (const auto& Recording : Candidate.Recordings)
	{
		float Previous = -1;
		if (Recording.Frames.IsEmpty() || Recording.Frames.Num() > 100000 || Recording.Frames[0].Seconds != 0) return Fail(TEXT("Invalid recording frames."));
		for (const auto& Frame : Recording.Frames)
		{
			if (!FMath::IsFinite(Frame.Seconds) || Frame.Seconds <= Previous || Frame.Seconds > 86400 || !LooksValid(Frame.Values)) return Fail(TEXT("Invalid recording time or values."));
			Previous = Frame.Seconds;
			for (const auto& Look : Frame.Values) SampleCount += Look.Attributes.Num();
			if (SampleCount > 2000000) return Fail(TEXT("Show exceeds the recording capacity."));
		}
	}
	float PreviousEvent = -1;
	for (const auto& Event : Candidate.Timecode)
	{
		if (!FMath::IsFinite(Event.Seconds) || Event.Seconds <= PreviousEvent || Event.Seconds > 86400
			|| !Candidate.Cues.ContainsByPredicate([&](const auto& Cue) { return Cue.Number == Event.CueNumber; })) return Fail(TEXT("Invalid timeline event or missing cue."));
		PreviousEvent = Event.Seconds;
	}
	return true;
}

FString ATSAVLightingShow::CaptureTSAVState() const
{
	FTSAVLightingShowData Snapshot=Data; Snapshot.bLoop=bLoop;
	if (bRecording && !RecordingBuffer.Frames.IsEmpty()) TSAVShow::Put(Snapshot.Recordings,RecordingBuffer);
	FString Json; FJsonObjectConverter::UStructToJsonObjectString(Snapshot,Json); return Json;
}

bool ATSAVLightingShow::RestoreTSAVState(const FString& State)
{
	FTSAVLightingShowData Candidate; FString Error;
	TSharedPtr<FJsonObject> Root; int32 Version=0;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(State),Root) || !Root || !Root->TryGetNumberField(TEXT("version"),Version) || Version!=1
		|| !FJsonObjectConverter::JsonObjectToUStruct(Root.ToSharedRef(),&Candidate) || !Validate(Candidate, Error)) return false;
	const auto Normalize=[](TArray<FTSAVFixtureLook>& Looks) { for (auto& Look : Looks) { TMap<FName,float> Values; for (const auto& Pair : Look.Attributes) { const FName Key=CanonicalAttribute(Pair.Key); if (const float* Existing=Values.Find(Key)) { if (*Existing!=Pair.Value) return false; } Values.Add(Key,Pair.Value); } Look.Attributes=MoveTemp(Values); } return true; };
	for (auto& Preset : Candidate.Presets) if (!Normalize(Preset.Values)) return false;
	for (auto& Cue : Candidate.Cues) if (!Normalize(Cue.Values)) return false;
	for (auto& Recording : Candidate.Recordings) for (auto& Frame : Recording.Frames) if (!Normalize(Frame.Values)) return false;
	Modify(); bRecording = false; RecordingBuffer = {}; Stop(); ClearProgrammer(); Baseline.Reset(); LastOutput.Reset(); Data = MoveTemp(Candidate);
	bLoop=Data.bLoop;
	const auto Sort = [](const auto& A, const auto& B) { return A.Number < B.Number; };
	Data.Groups.Sort(Sort); Data.Presets.Sort(Sort); Data.Cues.Sort(Sort); Data.Effects.Sort(Sort); Data.Recordings.Sort(Sort);
	Changed(); return true;
}

bool ATSAVLightingShow::SaveShow(const FString& Path) const
{
	if (Path.IsEmpty()) return false;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	const FString Temp = Path + TEXT(".tmp");
	return FFileHelper::SaveStringToFile(CaptureTSAVState(), *Temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
		&& IFileManager::Get().Move(*Path, *Temp, true, true);
}
bool ATSAVLightingShow::LoadShow(const FString& Path) { FString Text; return FFileHelper::LoadFileToString(Text, *Path) && RestoreTSAVState(Text); }

bool ATSAVLightingShow::BeginRecording(int32 Number, const FString& Name, const TArray<ATSAVDMXFixture*>& Fixtures, int32 SamplesPerSecond)
{
	if (bRecording || Number < 1 || Fixtures.IsEmpty() || SamplesPerSecond < 1 || SamplesPerSecond > 60) return false;
	RecordingFixtures.Reset();
	for (auto* Fixture : Fixtures) { const FGuid Id=FixtureId(Fixture); if (Id.IsValid()) RecordingFixtures.AddUnique(Id); }
	if (RecordingFixtures.IsEmpty()) return false;
	RecordingBuffer = {}; RecordingBuffer.Number=Number; RecordingBuffer.Name=Name;
	RecordedValues=0;
	for (const auto& Take : Data.Recordings) if (Take.Number != Number)
		for (const auto& Frame : Take.Frames) for (const auto& Look : Frame.Values) RecordedValues += Look.Attributes.Num();
	if (RecordedValues >= 2000000) return false;
	RecordingRate=SamplesPerSecond; RecordingTime=0; bRecording=true;
	CaptureRecordingFrame(); return bRecording;
}

void ATSAVLightingShow::CaptureRecordingFrame()
{
	FTSAVLightingFrame Frame; Frame.Seconds=RecordingTime;
	int32 Count=0;
	for (const FGuid& Id : RecordingFixtures) if (auto* Fixture=ResolveFixture(Id))
	{
		// Capture show output before grand master and blackout so playback applies those once.
		const auto* Cached=LastOutput.Find(Id);
		Frame.Values.Add(Cached ? *Cached : FTSAVFixtureLook{Id,TSAVShow::Read(Fixture)});
		Count += Frame.Values.Last().Attributes.Num();
	}
	if (Frame.Values.IsEmpty() || RecordedValues + Count > 2000000 || RecordingBuffer.Frames.Num() >= 100000 || RecordingTime > 86400) { EndRecording(); return; }
	RecordedValues += Count; RecordingBuffer.Frames.Add(MoveTemp(Frame));
}

void ATSAVLightingShow::EndRecording()
{
	if (!bRecording) return;
	bRecording=false;
	if (!RecordingBuffer.Frames.IsEmpty() && RecordingTime > RecordingBuffer.Frames.Last().Seconds) CaptureRecordingFrame();
	if (!RecordingBuffer.Frames.IsEmpty()) { Modify(); TSAVShow::Put(Data.Recordings, RecordingBuffer); Changed(); }
	RecordingBuffer={}; RecordingFixtures.Reset();
}

bool ATSAVLightingShow::PlayRecording(int32 Number)
{
	const auto* Take=Data.Recordings.FindByPredicate([&](const auto& Item) { return Item.Number==Number; });
	if (!Take || Take->Frames.IsEmpty()) return false;
	PlayingRecording=Number; RecordingPlaybackTime=0; bRecordingPlayback=true; bPaused=false;
	EvaluateRecording(); Flush(); return true;
}

void ATSAVLightingShow::StopRecordingPlayback() { bRecordingPlayback=false; PlayingRecording=INDEX_NONE; RecordedPlayback.Reset(); Flush(); }
void ATSAVLightingShow::SeekRecording(float Seconds)
{
	if (!FMath::IsFinite(Seconds)) return;
	RecordingPlaybackTime=FMath::Max(0.0f,Seconds); EvaluateRecording(); Flush();
}

void ATSAVLightingShow::EvaluateRecording()
{
	const auto* Take=Data.Recordings.FindByPredicate([&](const auto& Item) { return Item.Number==PlayingRecording; });
	if (!Take || Take->Frames.IsEmpty()) return;
	const float Duration=Take->Frames.Last().Seconds;
	if (RecordingPlaybackTime > Duration) {
		if (bLoop && Duration>0) RecordingPlaybackTime=FMath::Fmod(RecordingPlaybackTime,Duration);
		else { RecordingPlaybackTime=Duration; bRecordingPlayback=false; }
	}
	int32 Low=0, High=Take->Frames.Num();
	while (Low<High) { const int32 Middle=(Low+High)/2; if (Take->Frames[Middle].Seconds<=RecordingPlaybackTime) Low=Middle+1; else High=Middle; }
	RecordedPlayback.Reset();
	for (const auto& Look : Take->Frames[FMath::Max(0,Low-1)].Values) if (auto* Fixture=ResolveFixture(Look.FixtureId)) { Touch(Fixture); RecordedPlayback.Add(Look.FixtureId,Look); }
}

bool ATSAVLightingShow::StoreTimecodeEvent(float Seconds, int32 CueNumber)
{
	if (!FMath::IsFinite(Seconds) || Seconds<0 || Seconds>86400 || !Data.Cues.ContainsByPredicate([&](const auto& C) { return C.Number==CueNumber; })) return false;
	Modify();
	if (auto* Existing=Data.Timecode.FindByPredicate([&](const auto& E) { return FMath::IsNearlyEqual(E.Seconds,Seconds); })) Existing->CueNumber=CueNumber;
	else Data.Timecode.Add({Seconds,CueNumber});
	Data.Timecode.Sort([](const auto& A,const auto& B) { return A.Seconds<B.Seconds; }); Changed(); return true;
}
bool ATSAVLightingShow::DeleteTimecodeEvent(float Seconds)
{
	Modify(); const int32 Count=Data.Timecode.RemoveAll([&](const auto& Event) { return FMath::IsNearlyEqual(Event.Seconds,Seconds); });
	if (Count) Changed(); return Count>0;
}
void ATSAVLightingShow::StartTimeline() { SeekTimeline(0); bTimeline=!Data.Timecode.IsEmpty(); bPaused=false; }
void ATSAVLightingShow::SeekTimeline(float Seconds)
{
	if (!FMath::IsFinite(Seconds) || Seconds<0) return;
	const bool bWasRunning=bTimeline;
	Stop(); TimelineTime=0;
	for (const auto& Event : Data.Timecode) {
		if (Event.Seconds>Seconds) break;
		CueTime += Event.Seconds-TimelineTime; EvaluateCue();
		Go(Event.CueNumber); TimelineTime=Event.Seconds;
	}
	CueTime += Seconds-TimelineTime; TimelineTime=Seconds; EvaluateCue(); Flush(); bTimeline=bWasRunning;
}
void ATSAVLightingShow::AdvanceTimeline(float DeltaSeconds)
{
	const float End=TimelineTime+DeltaSeconds;
	for (const auto& Event : Data.Timecode) {
		if (Event.Seconds<=TimelineTime || Event.Seconds>End) continue;
		CueTime += Event.Seconds-TimelineTime; EvaluateCue(); Go(Event.CueNumber); TimelineTime=Event.Seconds;
	}
	CueTime += End-TimelineTime; TimelineTime=End; EvaluateCue();
}
#if WITH_EDITOR
void ATSAVLightingShow::PostEditUndo() { Super::PostEditUndo(); Stop(); ClearProgrammer(); ++Revision; }
#endif
