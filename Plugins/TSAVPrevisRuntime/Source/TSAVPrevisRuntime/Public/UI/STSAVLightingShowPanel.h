// Copyright TSAV. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
class ATSAVLightingShow;
class ATSAVDMXFixture;
class SVerticalBox;
class UWorld;

/** The same native show console in Unreal Editor and the packaged application. */
class TSAVPREVISRUNTIME_API STSAVLightingShowPanel final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STSAVLightingShowPanel) {}
		SLATE_ARGUMENT(UWorld*, World)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	void SelectFixtures(const TArray<ATSAVDMXFixture*>& Fixtures);
	virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override;
private:
	ATSAVLightingShow* Show(bool bCreate = true) const;
	TArray<ATSAVDMXFixture*> Selection() const;
	void RefreshFixtures();
	void RefreshSlots();
	void RebuildRawFaders();
	void RefreshReadout();
	TSharedRef<SWidget> Fader(FName Attribute);
	TSharedRef<SWidget> SlotSection(FName Kind, const FString& Title);
	TSharedRef<SWidget> TransportSection();
	TSharedRef<SWidget> ExecutorSection();
	void Store(FName Kind);
	void Recall(FName Kind);
	void Delete(FName Kind);
	void PatchSelection();
	void Remember(ATSAVLightingShow* Target, const FString& Before, bool bSuccess);
	void UndoShow(bool bRedo);
	void Status(const FString& Message, bool bSuccess = true);
	TWeakObjectPtr<UWorld> World;
	TSet<FGuid> Selected;
	TMap<FName, float> FaderValues;
	TMap<FName, int32> SlotNumbers;
	TMap<FName, TSharedPtr<SVerticalBox>> SlotLists;
	TSharedPtr<SVerticalBox> FixtureRows, RawFaders;
	FString FixtureSignature, RawSignature, SlotName, FilePath;
	FString StatusMessage;
	bool bStatusOK = true;
	int32 LastRevision = -1;
	TWeakObjectPtr<ATSAVLightingShow> LastShow;
	int32 Universe = 1, Address = 1;
	float Fade = 1.0f, Delay = 0.0f, Follow = -1.0f, SeekSeconds = 0.0f;
	bool bTrack = true;
	FString EffectAttribute = TEXT("Dimmer");
	int32 Wave = 0;
	float Period = 2.0f, Minimum = 0.0f, Maximum = 1.0f, Spread = 1.0f;
	float RefreshTime = 0.0f;
	float TransportSeconds = 0.0f;
	int32 RecordRate = 30;
	TSharedPtr<SVerticalBox> TimelineRows;
	TArray<FString> UndoStates, RedoStates;
	FString ExecutorCues=TEXT("1"), RecordingBeforeState;
	bool bExecutorLoop=false;
	uint8 StoreScope=0;
	FString PixelMapPath;
	int32 PixelMapColumns=1;
	float TimecodeOffset=0;
};
