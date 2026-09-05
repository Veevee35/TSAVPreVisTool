// Copyright TSAV. All Rights Reserved.
#include "Lighting/TSAVLightingShow.h"
#include "TSAVDMXFixture.h"

bool ATSAVLightingShow::StoreExecutor(const FTSAVLightingExecutor& Executor)
{
	if (Executor.Number<1 || Executor.Cues.IsEmpty()) return false;
	for (int32 Cue : Executor.Cues) if (!Data.Cues.ContainsByPredicate([&](const auto& C) { return C.Number==Cue; })) return false;
	Modify(); StopExecutor(Executor.Number);
	if (auto* Existing=Data.Executors.FindByPredicate([&](const auto& E) { return E.Number==Executor.Number; })) *Existing=Executor;
	else Data.Executors.Add(Executor);
	Data.Executors.Sort([](const auto& A,const auto& B) { return A.Number<B.Number; }); Changed(); return true;
}
bool ATSAVLightingShow::GoExecutor(int32 Number)
{
	const auto* Definition=Data.Executors.FindByPredicate([&](const auto& E) { return E.Number==Number; });
	if (!Definition || Definition->Cues.IsEmpty()) return false;
	auto& State=ExecutorPlayback.FindOrAdd(Number); int32 Index=State.Index+1;
	if (Index>=Definition->Cues.Num()) { if (!Definition->bLoop) return false; Index=0; }
	TMap<FGuid,FTSAVFixtureLook> Target;
	for (int32 I=0; I<=Index; ++I) {
		const auto* Cue=Data.Cues.FindByPredicate([&](const auto& C) { return C.Number==Definition->Cues[I]; }); if (!Cue) return false;
		if (!Cue->bTrack) Target.Reset();
		for (const auto& Look : Cue->Values) { auto& To=Target.FindOrAdd(Look.FixtureId); To.FixtureId=Look.FixtureId; for (const auto& Attr : Look.Attributes) To.Attributes.Add(Attr.Key,Attr.Value); }
	}
	State.From.Reset();
	for (const auto& Pair : Target) if (auto* Fixture=ResolveFixture(Pair.Key)) {
		Touch(Fixture); auto& From=State.From.FindOrAdd(Pair.Key); From.FixtureId=Pair.Key;
		const auto* Previous=State.Values.Find(Pair.Key); const auto* Output=LastOutput.Find(Pair.Key);
		const auto& Values=Previous?Previous->Attributes:Output?Output->Attributes:Baseline[Pair.Key].Attributes;
		for (const auto& Attr : Pair.Value.Attributes) From.Attributes.Add(Attr.Key,Values.FindRef(Attr.Key));
	}
	State.To=MoveTemp(Target); State.Index=Index; State.Time=0; State.Serial=++ExecutorSerial; bPaused=false;
	EvaluateExecutor(Number); Flush(); return true;
}
void ATSAVLightingShow::EvaluateExecutor(int32 Number)
{
	auto* State=ExecutorPlayback.Find(Number); const auto* Definition=Data.Executors.FindByPredicate([&](const auto& E) { return E.Number==Number; });
	if (!State || !Definition || !Definition->Cues.IsValidIndex(State->Index)) return;
	const auto* Cue=Data.Cues.FindByPredicate([&](const auto& C) { return C.Number==Definition->Cues[State->Index]; }); if (!Cue) return;
	const float Alpha=State->Time<Cue->DelaySeconds?0:Cue->FadeSeconds<=0?1:FMath::Clamp((State->Time-Cue->DelaySeconds)/Cue->FadeSeconds,0.0f,1.0f);
	State->Values=State->To;
	for (auto& Pair : State->Values) { const auto* From=State->From.Find(Pair.Key); for (auto& Attr : Pair.Value.Attributes) Attr.Value=FMath::Lerp(From?From->Attributes.FindRef(Attr.Key):0.0f,Attr.Value,Alpha); }
}
void ATSAVLightingShow::AdvanceExecutors(float DeltaSeconds)
{
	TArray<int32> Numbers; ExecutorPlayback.GetKeys(Numbers);
	for (int32 Number : Numbers) {
		auto& State=ExecutorPlayback[Number]; State.Time+=DeltaSeconds; EvaluateExecutor(Number);
		for (int32 Count=0; Count<128; ++Count) {
			const int32 CueNumber=GetExecutorCue(Number); const auto* Cue=Data.Cues.FindByPredicate([&](const auto& C) { return C.Number==CueNumber; });
			if (!Cue || Cue->FollowSeconds<0) break;
			const float Duration=Cue->DelaySeconds+Cue->FadeSeconds+Cue->FollowSeconds; if (State.Time<Duration) break;
			const float Remainder=State.Time-Duration; if (!GoExecutor(Number)) break; State.Time=Remainder; EvaluateExecutor(Number); if (Duration<=0) break;
		}
	}
}
void ATSAVLightingShow::StopExecutor(int32 Number) { ExecutorPlayback.Remove(Number); Flush(); }
void ATSAVLightingShow::SetExecutorLevel(int32 Number,float Level) { if (!FMath::IsFinite(Level)) return; ExecutorPlayback.FindOrAdd(Number).Level=FMath::Clamp(Level,0.0f,1.0f); Flush(); }
float ATSAVLightingShow::GetExecutorLevel(int32 Number) const { const auto* State=ExecutorPlayback.Find(Number); return State?State->Level:1; }
int32 ATSAVLightingShow::GetExecutorCue(int32 Number) const {
	const auto* State=ExecutorPlayback.Find(Number); const auto* Definition=Data.Executors.FindByPredicate([&](const auto& E) { return E.Number==Number; });
	return State && Definition && Definition->Cues.IsValidIndex(State->Index)?Definition->Cues[State->Index]:INDEX_NONE;
}
void ATSAVLightingShow::OverlayExecutors(FGuid Fixture,TMap<FName,float>& Values) const
{
	TArray<const FExecutorPlayback*> Ordered; for (const auto& Pair : ExecutorPlayback) if (Pair.Value.Index!=INDEX_NONE) Ordered.Add(&Pair.Value);
	Ordered.Sort([](const auto& A,const auto& B) { return A.Serial<B.Serial; });
	TMap<FName,float> Intensities;
	for (const auto* State : Ordered) if (const auto* Look=State->Values.Find(Fixture)) for (const auto& Attr : Look->Attributes) {
		FIntPoint Cell; FName CellAttribute; const bool bDimmer=Attr.Key==TEXT("dimmer") || (ATSAVDMXFixture::ParseMatrixAttributeKey(Attr.Key,Cell,CellAttribute) && CellAttribute==TEXT("dimmer"));
		if (bDimmer) { auto& Level=Intensities.FindOrAdd(Attr.Key); Level=FMath::Max(Level,Attr.Value*State->Level); }
		else Values.Add(Attr.Key,Attr.Value);
	}
	for (const auto& Intensity : Intensities) Values.Add(Intensity.Key,Intensity.Value);
}
