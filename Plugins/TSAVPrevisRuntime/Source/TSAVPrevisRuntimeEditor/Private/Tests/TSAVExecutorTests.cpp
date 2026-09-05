// Copyright TSAV. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "Lighting/TSAVLightingShow.h"
#include "UI/TSAVDMXNetworkWidget.h"
#include "TSAVDMXFixture.h"
#include "Components/SpotLightComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "JsonObjectConverter.h"
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Widgets/SWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTSAVExecutorTest,"TSAV.LightingConsole.Executors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTSAVExecutorTest::RunTest(const FString&)
{
	UWorld* World=GEditor->GetEditorWorldContext().World();
	if (!FApp::IsUnattended() || !World || World->GetOutermost()->GetName()!=TEXT("/Engine/Maps/Entry")) return false;
	auto* A=World->SpawnActor<ATSAVDMXFixture>(); auto* Show=ATSAVLightingShow::Find(World,true);
	ON_SCOPE_EXIT { A->Destroy(); Show->Destroy(); };
	const FGuid Id=ATSAVLightingShow::FixtureId(A);
	FTSAVLightingShowData Data;
	FTSAVLightingCue Red; Red.Number=1; Red.FadeSeconds=0; Red.Values.Add({Id,{{TEXT("dimmer"),0.4f},{TEXT("red"),1},{TEXT("blue"),0}}});
	FTSAVLightingCue Blue=Red; Blue.Number=2; Blue.Values[0].Attributes={{TEXT("dimmer"),0.8f},{TEXT("red"),0},{TEXT("blue"),1}};
	FTSAVLightingCue Fade=Red; Fade.Number=3; Fade.FadeSeconds=2; Fade.Values[0].Attributes={{TEXT("dimmer"),1}};
	Data.Cues={Red,Blue,Fade}; FString Json; FJsonObjectConverter::UStructToJsonObjectString(Data,Json); TestTrue(TEXT("Sparse cues load"),Show->RestoreTSAVState(Json));
	FTSAVLightingExecutor E; E.Number=1; E.Name=TEXT("Red then fade"); E.Cues={1,3}; TestTrue(TEXT("Store first executor"),Show->StoreExecutor(E));
	E.Number=2; E.Cues={2}; TestTrue(TEXT("Store independent executor"),Show->StoreExecutor(E));
	TestTrue(TEXT("First executor starts"),Show->GoExecutor(1)); TestTrue(TEXT("Second executor starts"),Show->GoExecutor(2));
	const auto Dimmer=[&] { return A->GetBeamLightComponent()->Intensity/A->MaximumIntensityLumens; };
	TestTrue(TEXT("Executor intensities merge highest"),FMath::IsNearlyEqual(Dimmer(),0.8f));
	TestTrue(TEXT("Latest GO wins color"),A->GetBeamLightComponent()->GetLightColor().B>0.99f);
	Show->SetExecutorLevel(2,0.25f); TestTrue(TEXT("Executor faders participate in HTP merge"),FMath::IsNearlyEqual(Dimmer(),0.4f));
	Show->SetProgrammer({A},{{TEXT("Dimmer"),0.1f}}); TestTrue(TEXT("Programmer overrides executor"),FMath::IsNearlyEqual(Dimmer(),0.1f));
	Show->ClearProgrammer(); Show->StopExecutor(2); TestTrue(TEXT("Executor release restores other executor color"),A->GetBeamLightComponent()->GetLightColor().R>0.99f);
	TestTrue(TEXT("Executor GO advances its own sequence"),Show->GoExecutor(1)); Show->Advance(1);
	TestTrue(TEXT("Executor fade interpolates"),FMath::IsNearlyEqual(Dimmer(),0.7f,0.001f));
	Show->SetMaster(0.5f); TestTrue(TEXT("Global master applied once after merge"),FMath::IsNearlyEqual(Dimmer(),0.35f,0.001f));
	Show->SetBlackout(true); TestEqual(TEXT("Global blackout includes executors"),Dimmer(),0.0f);
	Show->SetBlackout(false); Show->SetMaster(1); Show->Stop(); TestEqual(TEXT("Global stop releases executors"),Show->GetExecutorCue(1),INDEX_NONE);
	const FString Saved=Show->CaptureTSAVState(); TestFalse(TEXT("Unversioned JSON rejected"),Show->RestoreTSAVState(TEXT("{}"))); TestEqual(TEXT("Invalid show does not clear slots"),Show->CaptureTSAVState(),Saved);
	TestTrue(TEXT("Show file restores executor assignments"),Show->RestoreTSAVState(Saved)); TestEqual(TEXT("Executor count retained"),Show->GetData().Executors.Num(),2);
	TestTrue(TEXT("Deleting a referenced cue succeeds"),Show->DeleteSlot(TEXT("Cue"),2)); TestEqual(TEXT("Empty executor removed with its cue"),Show->GetData().Executors.Num(),1);
	Show->SetProgrammer({A},{{TEXT("Red"),0.25f},{TEXT("Dimmer"),0.75f}});
	TestTrue(TEXT("Store color-only preset"),Show->StorePreset(5,TEXT("Color only"),{A},ETSAVLightingStoreScope::Color));
	const auto* ColorPreset=Show->GetData().Presets.FindByPredicate([](const auto& P) { return P.Number==5; });
	TestTrue(TEXT("Scoped preset excludes dimmer and movement"),ColorPreset && !ColorPreset->Values[0].Attributes.Contains(TEXT("dimmer")) && !ColorPreset->Values[0].Attributes.Contains(TEXT("pan")));
	Show->ClearProgrammer(); TestFalse(TEXT("Programmer scope rejects an empty programmer"),Show->StoreCue(10,TEXT("Empty"),{A},0,0,-1,true,ETSAVLightingStoreScope::Programmer));
	Show->StoreTimecodeEvent(0,1); Show->StoreTimecodeEvent(2,3); Show->SetExternalTimecode(true,-3600); Show->StartTimeline(); Show->FollowExternalTimecode(3603);
	TestTrue(TEXT("External timecode applies offset and reconstructs cue fade"),FMath::IsNearlyEqual(Show->GetTimelineTime(),3) && Show->GetCurrentCue()==3);
	Show->FollowExternalTimecode(3600.25); TestTrue(TEXT("Backward timecode seek reconstructs the earlier cue"),Show->GetCurrentCue()==1 && FMath::IsNearlyEqual(Show->GetTimelineTime(),0.25f));
	Show->Stop(); Show->SetExternalTimecode(false,0);
	const FString ImagePath=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("NativeLightingReview/PixelMapTest.png")));
	TArray<FColor> Pixels={FColor::Red}; TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(1,1,Pixels,PNG); FFileHelper::SaveArrayToFile(PNG,*ImagePath);
	FString PixelError; TestTrue(TEXT("Image pixels enter the programmer"),Show->ApplyPixelMap({A},1,ImagePath,PixelError));
	TestTrue(TEXT("Image changes native RGB output"),A->GetBeamLightComponent()->GetLightColor().Equals(FLinearColor::Red,0.01f)); IFileManager::Get().Delete(*ImagePath);
	FTSAVDMXConnection C; FString Error; TestTrue(TEXT("Valid Art-Net mapping"),TSAVDMXNetwork::Validate(C,Error));
	C.ExternalUniverse=32767; C.Universes=2; TestFalse(TEXT("Art-Net range overflow rejected"),TSAVDMXNetwork::Validate(C,Error));
	C.bSACN=true; C.ExternalUniverse=0; TestFalse(TEXT("sACN universe zero rejected"),TSAVDMXNetwork::Validate(C,Error));
	C.ExternalUniverse=1; C.Adapter=TEXT("bad address"); TestFalse(TEXT("Invalid adapter rejected before port mutation"),TSAVDMXNetwork::Validate(C,Error));
	TestTrue(TEXT("Shared DMX connection panel constructs"),MakeTSAVDMXNetworkPanel(World,FSimpleDelegate())->GetChildren()->Num()>0);
	return true;
}
#endif
