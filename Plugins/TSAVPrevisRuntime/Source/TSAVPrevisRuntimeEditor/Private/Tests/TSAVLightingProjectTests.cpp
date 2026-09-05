// Copyright TSAV. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "Lighting/TSAVLightingShow.h"
#include "Project/TSAVProjectSubsystem.h"
#include "TSAVDMXFixture.h"
#include "Scene/TSAVSceneGenerator.h"
#include "Video/TSAVCameraActor.h"
#include "TSAVVideoSwitcher.h"
#include "Interaction/TSAVCommandSubsystem.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTSAVLightingProjectTest,"TSAV.LightingConsole.ProjectRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTSAVLightingProjectTest::RunTest(const FString&)
{
	if (!FApp::IsUnattended()) return false;
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
	Instance->InitializeStandalone();
	UWorld* World=Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Project=Instance->GetSubsystem<UTSAVProjectSubsystem>();
	if (!TestNotNull(TEXT("Isolated runtime project subsystem"),Project)) return false;
	Project->NewProject(TEXT("Native lighting round trip"));
	auto* Scenery=World->SpawnActor<ATSAVSceneGenerator>();
	Scenery->Configure(ATSAVSceneGenerator::Preset(ETSAVScenicKind::Scaffold));
	const FGuid SceneryId=Scenery->FindComponentByClass<UTSAVSceneObjectComponent>()->ObjectId;
	const int32 PartCount=Scenery->GetPartCount();
	auto* Commands=Instance->GetSubsystem<UTSAVCommandSubsystem>();
	TestTrue(TEXT("Record newly configured scenery as one spawn command"),Commands->CommitSpawnedActor(Scenery,FText::FromString(TEXT("Create test scenery"))));
	TestTrue(TEXT("Undo scenery creation"),Commands->Undo()); TestFalse(TEXT("Undo removes scenery"),IsValid(Scenery));
	TestTrue(TEXT("Redo scenery creation"),Commands->Redo());
	Scenery=nullptr; for (TActorIterator<ATSAVSceneGenerator> It(World); It; ++It) if (It->FindComponentByClass<UTSAVSceneObjectComponent>()->ObjectId==SceneryId) Scenery=*It;
	if (!TestNotNull(TEXT("Redo restores scene object identity"),Scenery)) return false;
	TestEqual(TEXT("Redo restores configured scaffold geometry"),Scenery->GetPartCount(),PartCount);
	auto* A=World->SpawnActor<ATSAVDMXFixture>(); auto* Show=ATSAVLightingShow::Find(World,true);
	A->MaximumIntensityLumens=3456; A->SetActorLocation(FVector(12,34,56));
	const FGuid Id=ATSAVLightingShow::FixtureId(A);
	Show->SetProgrammer({A},{{TEXT("Dimmer"),0.6f}}); Show->StoreGroup(4,TEXT("Restored group"),{A}); Show->StoreCue(2,TEXT("Saved cue"),{A},0);
	auto* Camera=World->SpawnActor<ATSAVCameraActor>();
	auto* Switcher=World->SpawnActor<ATSAVVideoSwitcher>();
	UTSAVSceneObjectComponent::EnsureForActor(Camera); UTSAVSceneObjectComponent::EnsureForActor(Switcher);
	const FGuid CameraId=Camera->CameraId;
	Switcher->bAutoDiscoverSources=false;
	FTSAVVideoInput CameraInput; CameraInput.InputId=FGuid::NewGuid(); CameraInput.Kind=ETSAVVideoInputKind::CameraFeed; CameraInput.ProviderId=CameraId; CameraInput.ProviderActor=Camera;
	Switcher->Inputs.Add(CameraInput); Switcher->SetBusInput(TEXT("Program"),CameraInput.InputId);
	const FString Path=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("NativeLightingReview/LightingProject.tsav")));
	TestTrue(TEXT("Save complete runtime lighting project"),Project->SaveProject(Path));
	TestTrue(TEXT("Reload over existing camera and switcher"),Project->LoadProject(Path));
	Camera=nullptr; Switcher=nullptr;
	for (TActorIterator<ATSAVCameraActor> It(World); It; ++It) if (It->CameraId==CameraId) Camera=*It;
	for (TActorIterator<ATSAVVideoSwitcher> It(World); It; ++It) Switcher=*It;
	if (!TestNotNull(TEXT("Replacement camera exists"),Camera) || !TestNotNull(TEXT("Replacement switcher exists"),Switcher)) return false;
	TestEqual(TEXT("Retiring the old camera preserves the restored bus selection"),Switcher->GetBusInputId(TEXT("Program")),CameraInput.InputId);
	TestTrue(TEXT("Restored bus resolves the replacement camera texture"),Switcher->GetOutputTexture(TEXT("Program"))==Camera->GetTSAVVideoTexture());
	Project->NewProject(TEXT("Temporary"));
	TestTrue(TEXT("Reload runtime lighting project"),Project->LoadProject(Path));
	Scenery=nullptr; for (TActorIterator<ATSAVSceneGenerator> It(World); It; ++It) if (It->FindComponentByClass<UTSAVSceneObjectComponent>()->ObjectId==SceneryId) Scenery=*It;
	if (!TestNotNull(TEXT("Scenery restored by .tsav project"),Scenery)) return false;
	TestEqual(TEXT("Saved procedural geometry is rebuilt"),Scenery->GetPartCount(),PartCount);
	Show=ATSAVLightingShow::Find(World); if (!TestNotNull(TEXT("Lighting show actor restored"),Show)) return false;
	A=Show->ResolveFixture(Id); if (!TestNotNull(TEXT("Fixture referenced by saved cue restored"),A)) return false;
	TestEqual(TEXT("Runtime project metadata restored"),Project->GetProjectName(),FString(TEXT("Native lighting round trip")));
	TestTrue(TEXT("Fixture transform survives project round trip"),A->GetActorLocation().Equals(FVector(12,34,56)));
	TestEqual(TEXT("Fixture physical configuration survives project round trip"),A->MaximumIntensityLumens,3456.0f);
	TestTrue(TEXT("Saved cue controls restored fixture"),Show->Go(2));
	TestTrue(TEXT("Restored light output"),FMath::IsNearlyEqual(A->GetBeamLightComponent()->Intensity,3456*0.6f));
	FString Json; TSharedPtr<FJsonObject> Root;
	if (!TestTrue(TEXT("Saved project is readable JSON"),FFileHelper::LoadFileToString(Json,*Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root) && Root.IsValid())) return false;
	for (const auto& Value : Root->GetArrayField(TEXT("objects")))
		if (Value->AsObject()->GetStringField(TEXT("class"))==ATSAVDMXFixture::StaticClass()->GetPathName()) Value->AsObject()->SetStringField(TEXT("customState"),TEXT("{\"version\":99}"));
	FString Invalid; FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Invalid));
	const FString BadPath=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("NativeLightingReview/InvalidLightingProject.tsav")));
	FFileHelper::SaveStringToFile(Invalid,*BadPath);
	const FGuid OriginalProject=Project->GetProjectId();
	TestFalse(TEXT("Invalid replacement fixture rejects project load"),Project->LoadProject(BadPath));
	TestTrue(TEXT("Rejected project preserves original fixture"),IsValid(A) && Show->ResolveFixture(Id)==A);
	TestTrue(TEXT("Rejected project preserves original show"),IsValid(Show) && Show->GetData().Cues.Num()==1);
	TestEqual(TEXT("Rejected project preserves document identity"),Project->GetProjectId(),OriginalProject);
	return true;
}
#endif
