// Copyright TSAV. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "Lighting/TSAVLightingShow.h"
#include "UI/STSAVLightingShowPanel.h"
#include "TSAVDMXFixture.h"
#include "Components/SpotLightComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "JsonObjectConverter.h"
#include "Library/DMXLibrary.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "RenderingThread.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTSAVLightingShowTest, "TSAV.LightingConsole.ShowPlayback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTSAVLightingShowTest::RunTest(const FString&)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!TestTrue(TEXT("Isolated Entry map"), FApp::IsUnattended() && World && World->GetOutermost()->GetName() == TEXT("/Engine/Maps/Entry"))) return false;
	UDMXLibrary* Library = NewObject<UDMXLibrary>();
	FDMXFixtureMode Mode; Mode.ModeName = TEXT("Original TSAV show test"); Mode.ChannelSpan = 7; Mode.bAutoChannelSpan = false;
	for (FName Name : {TEXT("Pan"), TEXT("Tilt"), TEXT("Dimmer"), TEXT("ColorAdd_R"), TEXT("ColorAdd_G"), TEXT("ColorAdd_B"), TEXT("Zoom")}) {
		FDMXFixtureFunction Fn; Fn.Attribute = FDMXAttributeName(Name); Fn.Channel = Mode.Functions.Num()+1; Mode.Functions.Add(Fn);
	}
	FDMXEntityFixtureTypeConstructionParams TypeParams; TypeParams.ParentDMXLibrary = Library; TypeParams.Modes.Add(Mode);
	auto* Type = UDMXEntityFixtureType::CreateFixtureTypeInLibrary(TypeParams, TEXT("TSAV Show Test"), false);
	FDMXEntityFixturePatchConstructionParams PatchParams; PatchParams.FixtureTypeRef = FDMXEntityFixtureTypeRef(Type);
	auto* Patch = UDMXEntityFixturePatch::CreateFixturePatchInLibrary(PatchParams, TEXT("Test Template"), false);
	auto* A = World->SpawnActor<ATSAVDMXFixture>(); auto* B = World->SpawnActor<ATSAVDMXFixture>();
	auto* Restored = World->SpawnActor<ATSAVDMXFixture>(); auto* Show = ATSAVLightingShow::Find(World, true);
	ON_SCOPE_EXIT { A->Destroy(); B->Destroy(); Restored->Destroy(); Show->Destroy(); };
	if (!A || !B || !Restored || !Show || !Patch) return false;
	A->SetFixturePatch(Patch); B->SetFixturePatch(Patch);
	TestTrue(TEXT("Individual runtime patch"), A->SetIndividualPatchAddress(12, 101));
	A->PanMinDegrees = -120; A->PanMaxDegrees = 120; A->MaximumIntensityLumens = 4321;
	FDMXNormalizedAttributeValueMap Alias; Alias.Map.Add(FDMXAttributeName(TEXT("Intensity")), 0.3f); A->ApplyAttributeValues(Alias, true);
	A->ApplyNormalizedDMX(0.8f, 0.2f, 0.6f, FLinearColor(0.2f,0.4f,0.8f), 0.7f, true);
	const FString FixtureState = A->CaptureTSAVState();
	TestTrue(TEXT("Fixture restores complete configuration and its own patch"), Restored->RestoreTSAVState(FixtureState));
	if (!TestNotNull(TEXT("Restored fixture patch"), Restored->GetFixturePatch())) return false;
	TestEqual(TEXT("Saved universe"), Restored->GetFixturePatch()->GetUniverseID(), 12);
	TestEqual(TEXT("Saved address"), Restored->GetFixturePatch()->GetStartingChannel(), 101);
	TestEqual(TEXT("Saved mode footprint"), Restored->GetFixturePatch()->GetChannelSpan(), 7);
	TestEqual(TEXT("Saved physical range"), Restored->PanMinDegrees, -120.0f);
	TestEqual(TEXT("Saved output uses latest value, not cached alias"), Restored->GetBeamLightComponent()->Intensity, 4321.0f*0.6f);
	const FString RestoredBefore = Restored->CaptureTSAVState();
	TestFalse(TEXT("Invalid fixture state rejected"), Restored->RestoreTSAVState(TEXT("{\"version\":99}")));
	TestEqual(TEXT("Rejected fixture state preserves existing configuration"), Restored->CaptureTSAVState(), RestoredBefore);
	A->ApplyNormalizedDMX(0.5f,0.5f,0.0f,FLinearColor::White,0,true);
	B->ApplyNormalizedDMX(0.5f,0.5f,0.0f,FLinearColor::White,0,true);
	const FGuid AId = ATSAVLightingShow::FixtureId(A); const FGuid BId = ATSAVLightingShow::FixtureId(B);
	TestTrue(TEXT("Store deduplicated group"), Show->StoreGroup(1,TEXT("Front"),{A,B,A}));
	TestEqual(TEXT("Group contains two durable fixture IDs"), Show->GetData().Groups[0].Fixtures.Num(), 2);
	A->SetActorLabel(TEXT("Renamed fixture")); TestTrue(TEXT("Rename preserves show identity"), Show->ResolveFixture(AId) == A);
	Show->SetProgrammer({A}, {{TEXT("Dimmer"),1},{TEXT("Red"),0.2f}});
	Show->StorePreset(1,TEXT("Warm"),{A}); Show->StoreCue(1,TEXT("Open"),{A},2.0f,1.0f,1.0f);
	Show->SetProgrammer({B}, {{TEXT("Dimmer"),0.8f}}); Show->StoreCue(2,TEXT("Add back"),{B},0);
	Show->StoreCue(3,TEXT("Back only"),{B},0,0,-1,false);
	Show->ClearProgrammer();
	const auto Intensity = [](ATSAVDMXFixture* F) { return F->GetBeamLightComponent()->Intensity/F->MaximumIntensityLumens; };
	TestTrue(TEXT("GO starts cue"), Show->Go(1));
	Show->Advance(0.5f); TestEqual(TEXT("Delay holds baseline"), Intensity(A),0.0f);
	Show->Seek(2); TestTrue(TEXT("Halfway fade changes actual spotlight"), FMath::IsNearlyEqual(Intensity(A),0.5f));
	Show->SetPaused(true); Show->Advance(1); TestEqual(TEXT("Pause holds cue time"), Show->GetCueTime(),2.0f);
	Show->SetPaused(false);
	Show->SetProgrammer({A},{{TEXT("Intensity"),0.9f}}); Show->Seek(2.5f);
	TestTrue(TEXT("Programmer overrides playback"), FMath::IsNearlyEqual(Intensity(A),0.9f));
	Show->ClearProgrammer(); TestTrue(TEXT("Release restores current fade"), FMath::IsNearlyEqual(Intensity(A),0.75f));
	Show->SetMaster(0.5f); TestTrue(TEXT("Grand master applies after playback"), FMath::IsNearlyEqual(Intensity(A),0.375f));
	Show->SetBlackout(true); TestEqual(TEXT("Blackout wins"), Intensity(A),0.0f);
	TestEqual(TEXT("Blackout does not overwrite stored cue"), Show->GetData().Cues[0].Values[0].Attributes.FindRef(TEXT("dimmer")),1.0f);
	Show->SetBlackout(false); Show->SetMaster(1); Show->Advance(1.5f);
	TestEqual(TEXT("Follow advances after delay, fade and hold"), Show->GetCurrentCue(),2);
	TestEqual(TEXT("Tracking retains first fixture"), Intensity(A),1.0f);
	TestTrue(TEXT("Second cue adds second fixture"), FMath::IsNearlyEqual(Intensity(B),0.8f));
	Show->Go(3); TestEqual(TEXT("Nontracking cue releases previous look to baseline"),Intensity(A),0.0f);
	Show->Stop(); Show->RecallPreset(1); TestEqual(TEXT("Preset recall applies stored look"),Intensity(A),1.0f); Show->ClearProgrammer();
	FTSAVLightingEffect Effect; Effect.Number=1; Effect.Fixtures={AId,BId}; Effect.SpreadCycles=0.5f; Effect.PeriodSeconds=2;
	TestTrue(TEXT("Store phase effect"), Show->StoreEffect(Effect)); TestTrue(TEXT("Start effect"), Show->StartEffect(1));
	TestTrue(TEXT("Phase spread produces complementary outputs"), FMath::IsNearlyEqual(Intensity(A)+Intensity(B),1.0f,0.001f));
	Show->SetBlackout(true); Show->Advance(0.25f); TestEqual(TEXT("Effects obey blackout"),Intensity(B),0.0f); Show->SetBlackout(false); Show->StopEffects();
	Show->SetProgrammer({A},{{TEXT("Dimmer"),0.25f}});
	TestTrue(TEXT("Record selected native output"),Show->BeginRecording(1,TEXT("Original take"),{A},30));
	Show->SetMaster(0.5f); Show->SetProgrammer({A},{{TEXT("Dimmer"),0.75f}}); Show->Advance(0.5f); Show->EndRecording();
	TestEqual(TEXT("Recording stores sample timestamps"),Show->GetData().Recordings[0].Frames.Last().Seconds,0.5f);
	Show->ClearProgrammer(); Show->Stop(); TestTrue(TEXT("Play recorded take"),Show->PlayRecording(1));
	TestTrue(TEXT("Recorded first frame"),FMath::IsNearlyEqual(Intensity(A),0.125f));
	Show->SeekRecording(0.5f); TestTrue(TEXT("Recording applies master once"),FMath::IsNearlyEqual(Intensity(A),0.375f));
	Show->SetBlackout(true); TestEqual(TEXT("Recording obeys blackout"),Intensity(A),0.0f);
	Show->SetBlackout(false); Show->StopRecordingPlayback(); Show->SetMaster(1);
	TestTrue(TEXT("Schedule cue at one second"),Show->StoreTimecodeEvent(1,1));
	Show->StoreTimecodeEvent(6,2); Show->StoreTimecodeEvent(7,3);
	TestFalse(TEXT("Cannot schedule nonexistent cue"),Show->StoreTimecodeEvent(9,99));
	Show->SeekTimeline(3); TestTrue(TEXT("Timeline seek reconstructs delayed fade"),FMath::IsNearlyEqual(Intensity(A),0.5f));
	Show->StartTimeline(); Show->Advance(6.5f);
	TestEqual(TEXT("Large timeline step dispatches scheduled cue"),Show->GetCurrentCue(),2);
	TestEqual(TEXT("Timeline tracking reconstructed"),Intensity(A),1.0f);
	Show->Advance(1); TestEqual(TEXT("Timeline blocking cue applied"),Intensity(A),0.0f);
	Show->StopTimeline(); const float TimelineBefore=Show->GetTimelineTime(); Show->Advance(1); TestEqual(TEXT("Stopped timeline holds position"),Show->GetTimelineTime(),TimelineBefore);
	Show->SetPaused(false);
	Show->Stop(); Show->SetProgrammer({A},{{TEXT("Dimmer"),1},{TEXT("Red"),1},{TEXT("Green"),1},{TEXT("Blue"),1},{TEXT("Shutter"),0}});
	TestEqual(TEXT("Native shutter closes actual output"),Intensity(A),0.0f);
	Show->SetProgrammer({A},{{TEXT("Shutter"),1},{TEXT("Strobe"),1}});
	A->SetOpticsTime(0.03f); TestEqual(TEXT("Native strobe off phase"),Intensity(A),0.0f);
	A->SetOpticsTime(0.01f); TestEqual(TEXT("Native strobe on phase"),Intensity(A),1.0f);
	Show->SetProgrammer({A},{{TEXT("Strobe"),0},{TEXT("Cyan"),1}});
	TestTrue(TEXT("Native subtractive color mixing"),A->GetBeamLightComponent()->GetLightColor().R<0.01f);
	Show->SetProgrammer({A},{{TEXT("Cyan"),0},{TEXT("Gobo"),0.4f},{TEXT("Iris"),0.5f}});
	TestNotNull(TEXT("Original TSAV gobo/framing material bound"),A->GetBeamLightComponent()->LightFunctionMaterial.Get());
	Show->SetProgrammer({A},{{TEXT("Prism"),1},{TEXT("Iris"),1}});
	TestEqual(TEXT("Native prism creates separate beams"),A->GetActivePrismBeamCount(),3);
	float PrismIntensity=0; TArray<USpotLightComponent*> Lights; A->GetComponents(Lights);
	for (auto* Light : Lights) if (Light->IsVisible()) PrismIntensity+=Light->Intensity;
	TestTrue(TEXT("Prism conserves total intensity"),FMath::IsNearlyEqual(PrismIntensity,A->MaximumIntensityLumens,0.01f));
	Show->ClearProgrammer(); TestEqual(TEXT("Programmer release removes prism beams"),A->GetActivePrismBeamCount(),0);
	TestTrue(TEXT("Programmer release removes gobo override"),A->GetBeamLightComponent()->LightFunctionMaterial==nullptr);
	const FString Before = Show->CaptureTSAVState();
	FTSAVLightingShowData Invalid=Show->GetData(); Invalid.Cues[0].FadeSeconds=-1;
	FString Bad; FJsonObjectConverter::UStructToJsonObjectString(Invalid,Bad);
	TestFalse(TEXT("Invalid show file rejected"),Show->RestoreTSAVState(Bad));
	TestEqual(TEXT("Rejected show preserves all stored data"),Show->CaptureTSAVState(),Before);
	const FString Path=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("NativeLightingReview/ShowRoundTrip.tsavlight"));
	TestTrue(TEXT("Save show file"),Show->SaveShow(Path)); Show->DeleteSlot(TEXT("Cue"),1);
	TestTrue(TEXT("Reload show file"),Show->LoadShow(Path)); TestEqual(TEXT("File restores complete show data"),Show->CaptureTSAVState(),Before);
	TestTrue(TEXT("Restored groups still resolve scene fixtures"), Show->ResolveFixture(Show->GetData().Groups[0].Fixtures[0])==A);
	TSharedRef<STSAVLightingShowPanel> Panel=SNew(STSAVLightingShowPanel).World(World);
	TestTrue(TEXT("Shared editor/runtime console constructs"),Panel->GetChildren()->Num()>0);
	if (FParse::Param(FCommandLine::Get(),TEXT("TSAVNativeVisuals")))
	{
		Panel->SelectFixtures({A,B});
		FWidgetRenderer Renderer(true,true);
		UTextureRenderTarget2D* Target=Renderer.DrawWidget(Panel,FVector2D(1600,1000));
		FlushRenderingCommands();
		if (TestNotNull(TEXT("Rendered native console"),Target)) {
			TArray<FColor> Pixels;
			if (TestTrue(TEXT("Read rendered console pixels"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels))) {
				for (auto& Pixel : Pixels) Pixel.A=255;
				TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(1600,1000,Pixels,PNG);
				TestTrue(TEXT("Save rendered console review"),FFileHelper::SaveArrayToFile(PNG,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("NativeLightingReview/NativeShowConsole.png"))));
			}
		}
	}
	AddInfo(TEXT("Native serialization, group identity, preset recall, delayed fades, tracking, follow, programmer priority, master, blackout, effects, file validation and shared UI verified."));
	return true;
}
#endif
