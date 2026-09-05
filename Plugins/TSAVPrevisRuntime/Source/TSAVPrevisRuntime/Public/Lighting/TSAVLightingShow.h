// Copyright TSAV. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TSAVStateSerializable.h"
#include "TSAVLightingShow.generated.h"

class ATSAVDMXFixture;

USTRUCT(BlueprintType)
struct TSAVPREVISRUNTIME_API FTSAVFixtureLook
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid FixtureId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, float> Attributes;
};

USTRUCT(BlueprintType)
struct TSAVPREVISRUNTIME_API FTSAVLightingGroup
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Number = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGuid> Fixtures;
};

USTRUCT(BlueprintType)
struct TSAVPREVISRUNTIME_API FTSAVLightingPreset
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Number = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTSAVFixtureLook> Values;
};

USTRUCT(BlueprintType)
struct TSAVPREVISRUNTIME_API FTSAVLightingCue
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Number = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTSAVFixtureLook> Values;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FadeSeconds = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DelaySeconds = 0.0f;
	/** Negative means manual GO. Otherwise follow after the fade plus this hold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FollowSeconds = -1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTrack = true;
};

UENUM(BlueprintType)
enum class ETSAVLightingWave : uint8 { Sine, Triangle, Square, Saw };
UENUM(BlueprintType)
enum class ETSAVLightingStoreScope : uint8 { All, Programmer, Dimmer, Color, Position, Beam };

USTRUCT(BlueprintType)
struct TSAVPREVISRUNTIME_API FTSAVLightingEffect
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Number = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGuid> Fixtures;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Attribute = TEXT("Dimmer");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ETSAVLightingWave Wave = ETSAVLightingWave::Sine;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PeriodSeconds = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Minimum = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Maximum = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SpreadCycles = 1.0f;
};

USTRUCT(BlueprintType)
struct TSAVPREVISRUNTIME_API FTSAVLightingFrame
{
	GENERATED_BODY()
	UPROPERTY() float Seconds = 0;
	UPROPERTY() TArray<FTSAVFixtureLook> Values;
};

USTRUCT(BlueprintType)
struct TSAVPREVISRUNTIME_API FTSAVLightingRecording
{
	GENERATED_BODY()
	UPROPERTY() int32 Number = 1;
	UPROPERTY() FString Name;
	UPROPERTY() TArray<FTSAVLightingFrame> Frames;
};

USTRUCT(BlueprintType)
struct TSAVPREVISRUNTIME_API FTSAVLightingTimecodeEvent
{
	GENERATED_BODY()
	UPROPERTY() float Seconds = 0;
	UPROPERTY() int32 CueNumber = 1;
};

USTRUCT(BlueprintType)
struct TSAVPREVISRUNTIME_API FTSAVLightingExecutor
{
	GENERATED_BODY()
	UPROPERTY() int32 Number=1;
	UPROPERTY() FString Name;
	UPROPERTY() TArray<int32> Cues;
	UPROPERTY() bool bLoop=false;
};

USTRUCT(BlueprintType)
struct TSAVPREVISRUNTIME_API FTSAVLightingShowData
{
	GENERATED_BODY()
	UPROPERTY() int32 Version = 1;
	UPROPERTY() bool bLoop = false;
	UPROPERTY() bool bFollowEngineTimecode=false;
	UPROPERTY() double TimecodeOffsetSeconds=0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTSAVLightingGroup> Groups;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTSAVLightingPreset> Presets;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTSAVLightingCue> Cues;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTSAVLightingEffect> Effects;
	UPROPERTY() TArray<FTSAVLightingRecording> Recordings;
	UPROPERTY() TArray<FTSAVLightingTimecodeEvent> Timecode;
	UPROPERTY() TArray<FTSAVLightingExecutor> Executors;
};

/** Original, runtime-safe TSAV show engine. Scene metadata supplies durable fixture IDs. */
UCLASS(BlueprintType)
class TSAVPREVISRUNTIME_API ATSAVLightingShow final : public AActor, public ITSAVStateSerializable
{
	GENERATED_BODY()
public:
	ATSAVLightingShow();
	static ATSAVLightingShow* Find(UWorld* World, bool bCreate = false);
	static FGuid FixtureId(ATSAVDMXFixture* Fixture, bool bCreate = true);
	static FName CanonicalAttribute(FName Name);
	static bool Validate(const FTSAVLightingShowData& Candidate, FString& Error);
	ATSAVDMXFixture* ResolveFixture(FGuid Id) const;
	TArray<FTSAVFixtureLook> Capture(const TArray<ATSAVDMXFixture*>& Fixtures) const;
	void SetProgrammer(const TArray<ATSAVDMXFixture*>& Fixtures, const TMap<FName, float>& Attributes);
	bool ApplyPixelMap(const TArray<ATSAVDMXFixture*>& Fixtures,int32 Columns,const FString& ImagePath,FString& Error);
	void ClearProgrammer();
	bool StoreGroup(int32 Number, const FString& Name, const TArray<ATSAVDMXFixture*>& Fixtures);
	bool StorePreset(int32 Number, const FString& Name, const TArray<ATSAVDMXFixture*>& Fixtures, ETSAVLightingStoreScope Scope=ETSAVLightingStoreScope::All);
	bool RecallPreset(int32 Number);
	bool StoreCue(int32 Number, const FString& Name, const TArray<ATSAVDMXFixture*>& Fixtures, float Fade, float Delay = 0.0f, float Follow = -1.0f, bool bTrack = true, ETSAVLightingStoreScope Scope=ETSAVLightingStoreScope::All);
	bool StoreEffect(const FTSAVLightingEffect& Effect);
	bool DeleteSlot(FName Kind, int32 Number);
	bool Go(int32 Number);
	bool Next();
	bool Previous();
	void Stop();
	void SetPaused(bool bValue) { bPaused = bValue; }
	void SetMaster(float Value);
	void SetBlackout(bool bValue);
	bool StartEffect(int32 Number);
	void StopEffects();
	void Seek(float Seconds);
	void Advance(float DeltaSeconds);
	void Flush();
	bool BeginRecording(int32 Number, const FString& Name, const TArray<ATSAVDMXFixture*>& Fixtures, int32 SamplesPerSecond = 30);
	void EndRecording();
	bool PlayRecording(int32 Number);
	void StopRecordingPlayback();
	void SeekRecording(float Seconds);
	bool StoreTimecodeEvent(float Seconds, int32 CueNumber);
	bool DeleteTimecodeEvent(float Seconds);
	bool StoreExecutor(const FTSAVLightingExecutor& Executor);
	bool GoExecutor(int32 Number);
	void StopExecutor(int32 Number);
	void SetExecutorLevel(int32 Number,float Level);
	float GetExecutorLevel(int32 Number) const;
	int32 GetExecutorCue(int32 Number) const;
	void StartTimeline();
	void SetExternalTimecode(bool bEnabled,double OffsetSeconds);
	void FollowExternalTimecode(double Seconds);
	bool IsExternalTimecodeReady() const;
	void StopTimeline() { bTimeline = false; bPaused = true; }
	void SeekTimeline(float Seconds);
	bool IsRecording() const { return bRecording; }
	bool IsPlayingRecording() const { return bRecordingPlayback; }
	bool IsTimelineRunning() const { return bTimeline; }
	float GetRecordingTime() const { return bRecording ? RecordingTime : RecordingPlaybackTime; }
	float GetTimelineTime() const { return TimelineTime; }
	/** Number of scalar attribute samples still available before automatic recording stop. */
	int32 GetRecordingCapacity() const { return FMath::Max(0, 2000000 - RecordedValues); }
	virtual FString CaptureTSAVState() const override;
	virtual bool RestoreTSAVState(const FString& State) override;
	bool SaveShow(const FString& Path) const;
	bool LoadShow(const FString& Path);
	const FTSAVLightingShowData& GetData() const { return Data; }
	int32 GetCurrentCue() const { return CurrentCue; }
	float GetCueTime() const { return CueTime; }
	bool IsPaused() const { return bPaused; }
	float GetMaster() const { return Master; }
	bool IsBlackout() const { return bBlackout; }
	int32 GetRevision() const { return Revision; }
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TSAV Lighting") bool bNetworkOutput = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TSAV Lighting") bool bLoop = false;
protected:
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif
private:
	void Changed();
	void Touch(ATSAVDMXFixture* Fixture);
	void EvaluateCue();
	void CaptureRecordingFrame();
	void EvaluateRecording();
	void AdvanceTimeline(float DeltaSeconds);
	void AdvanceExecutors(float DeltaSeconds);
	void EvaluateExecutor(int32 Number);
	void OverlayExecutors(FGuid Fixture,TMap<FName,float>& Values) const;
	TMap<FName, float> CurrentBase(ATSAVDMXFixture* Fixture) const;
	TArray<FTSAVFixtureLook> CaptureForStore(const TArray<ATSAVDMXFixture*>& Fixtures,ETSAVLightingStoreScope Scope) const;
	UPROPERTY() FTSAVLightingShowData Data;
	TMap<FGuid, FTSAVFixtureLook> Baseline, Programmer, Playback, FadeFrom, FadeTo;
	TMap<FGuid, FTSAVFixtureLook> LastOutput, RecordedPlayback;
	FTSAVLightingRecording RecordingBuffer;
	TArray<FGuid> RecordingFixtures;
	int32 RecordingRate = 30, RecordedValues = 0, PlayingRecording = INDEX_NONE;
	float RecordingTime = 0, RecordingPlaybackTime = 0, TimelineTime = 0;
	bool bRecording = false, bRecordingPlayback = false, bTimeline = false;
	TArray<int32> ActiveEffects;
	int32 CurrentCue = INDEX_NONE;
	int32 Revision = 0;
	float CueTime = 0.0f;
	double EffectTime = 0.0;
	float Master = 1.0f;
	bool bBlackout = false;
	bool bPaused = false;
	struct FExecutorPlayback {
		int32 Index=INDEX_NONE;
		float Time=0,Level=1;
		uint64 Serial=0;
		TMap<FGuid,FTSAVFixtureLook> From,To,Values;
	};
	TMap<int32,FExecutorPlayback> ExecutorPlayback;
	uint64 ExecutorSerial=0;
};
