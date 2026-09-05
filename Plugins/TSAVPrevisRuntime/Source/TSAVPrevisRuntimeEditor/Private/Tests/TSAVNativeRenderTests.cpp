// Copyright TSAV. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "TSAVDMXFixture.h"
#include "Laser/TSAVLaserPreview.h"
#include "UI/TSAVLaserWidget.h"
#include "UI/TSAVRigWidget.h"
#include "UI/TSAVSceneBuilderWidget.h"
#include "UI/TSAVDMXNetworkWidget.h"
#include "Editor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "AssetCompilingManager.h"
#include "Tests/AutomationCommon.h"
#include "Components/SpotLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Widgets/SWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTSAVNativeRenderTest,"TSAV.NativeScene.RenderReview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTSAVNativeRenderTest::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(),TEXT("TSAVNativeVisuals"))) { AddInfo(TEXT("Use Test-TSAVLighting.ps1 -Render for GPU preview and panel captures.")); return true; }
	UWorld* World=GEditor->GetEditorWorldContext().World();
	if (!FApp::IsUnattended() || !World || World->GetOutermost()->GetName()!=TEXT("/Engine/Maps/Entry")) return false;
	const auto Save=[](UTextureRenderTarget2D* Target,const FString& Name) {
		if (!Target) return false; FlushRenderingCommands(); TArray<FColor> Pixels;
		if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels)) return false;
		for (auto& Pixel : Pixels) Pixel.A=255;
		TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Target->SizeX,Target->SizeY,Pixels,PNG);
		return FFileHelper::SaveArrayToFile(PNG,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("NativeLightingReview"),Name+TEXT(".png")));
	};
	auto* Fixture=World->SpawnActor<ATSAVDMXFixture>();
	auto* Wall=World->SpawnActor<AStaticMeshActor>();
	auto* Laser=World->SpawnActor<ATSAVLaserPreview>();
	auto* CaptureActor=World->SpawnActor<AActor>();
	Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
	// Plane local +Z normal is rotated to -X, facing the fixture and camera.
	Wall->SetActorTransform(FTransform(FQuat::FindBetweenNormals(FVector::UpVector,-FVector::ForwardVector),FVector(1000,0,0),FVector(12)));
	Fixture->MaximumIntensityLumens=50000; Fixture->MaximumBeamAngleDegrees=35; Fixture->MinimumBeamAngleDegrees=35;
	FDMXNormalizedAttributeValueMap Values; for (const auto& Pair : TMap<FName,float>{{TEXT("Dimmer"),1},{TEXT("Gobo"),0.4f},{TEXT("Iris"),0.9f},{TEXT("Red"),1},{TEXT("Green"),0.5f},{TEXT("Blue"),0.1f}}) Values.Map.Add(FDMXAttributeName(Pair.Key),Pair.Value);
	Fixture->ApplyAttributeValues(Values,true);
	FTSAVLaserData LaserData; LaserData.Frames=TSAVILDA::MakePattern(0,1,64); LaserData.WidthCm=750; LaserData.HeightCm=750; LaserData.DistanceCm=950; LaserData.bShowRays=false; LaserData.LineWidthCm=3;
	Laser->Configure(LaserData);
	const auto* LaserLines=Laser->FindComponentByClass<UInstancedStaticMeshComponent>();
	AddInfo(FString::Printf(TEXT("Laser instance values: channels=%d values=%d first=%s"),LaserLines->NumCustomDataFloats,LaserLines->PerInstanceSMCustomData.Num(),LaserLines->PerInstanceSMCustomData.Num()>=3?*FVector(LaserLines->PerInstanceSMCustomData[0],LaserLines->PerInstanceSMCustomData[1],LaserLines->PerInstanceSMCustomData[2]).ToString():TEXT("none")));
	auto* Capture=NewObject<USceneCaptureComponent2D>(CaptureActor); CaptureActor->SetRootComponent(Capture); Capture->RegisterComponent();
	CaptureActor->SetActorLocation(FVector(450,0,0)); CaptureActor->SetActorRotation(FRotator::ZeroRotator);
	Capture->bCaptureEveryFrame=true; Capture->bCaptureOnMovement=false; Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR; Capture->FOVAngle=95;
	Capture->PostProcessSettings.bOverride_AutoExposureMethod=true; Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
	Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true; Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
	Capture->PostProcessSettings.bOverride_AutoExposureBias=true; Capture->PostProcessSettings.AutoExposureBias=0;
	Capture->ShowFlags.SetAtmosphere(false); Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetTemporalAA(false); Capture->ShowFlags.SetAntiAliasing(false);
	Capture->ShowFlags.SetLightFunctions(true); Capture->ShowFlags.SetTranslucency(true);
	Capture->ShowFlags.SetSeparateTranslucency(true);
	Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList; Capture->ShowOnlyActors={Fixture,Wall,Laser};
	auto* Target=NewObject<UTextureRenderTarget2D>(); Target->InitCustomFormat(768,768,PF_B8G8R8A8,false); Target->TargetGamma=2.2f; Capture->TextureTarget=Target;
	// Newly loaded meshes and material permutations compile asynchronously in the editor.
	// A synchronous capture must wait for them instead of recording fallback materials.
	FAssetCompilingManager::Get().FinishAllCompilation();
	World->SendAllEndOfFrameUpdates(); FlushRenderingCommands();
	// Let instance buffers, PSOs and the per-frame light-function atlas become ready.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([=,this]() {
	ON_SCOPE_EXIT { Fixture->Destroy(); Wall->Destroy(); Laser->Destroy(); CaptureActor->Destroy(); };
	Capture->bCaptureEveryFrame=false;
	Capture->CaptureScene();
	AddInfo(FString::Printf(TEXT("Native render: fixture=%s laser=%s segments=%d gobo=%s"),*Fixture->GetActorLocation().ToString(),*Laser->GetActorLocation().ToString(),Laser->GetSegmentCount(),*GetNameSafe(Fixture->GetBeamLightComponent()->LightFunctionMaterial)));
	if (auto* Material=Cast<UMaterialInstanceDynamic>(Fixture->GetBeamLightComponent()->LightFunctionMaterial)) { float Gobo=0; Material->GetScalarParameterValue(TEXT("GoboIndex"),Gobo); AddInfo(FString::Printf(TEXT("Gobo index %.1f"),Gobo)); }
	TestTrue(TEXT("Save actual native optics and laser render"),Save(Target,TEXT("NativeOpticsAndLaser")));
	TArray<FColor> Rendered;
	if (TestTrue(TEXT("Read optical pixels"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Rendered)) && Rendered.Num()==768*768) {
		const auto MeanRed=[&](int32 Y) { double Value=0; for (int32 DY=-4; DY<=4; ++DY) for (int32 X=375; X<=393; ++X) Value+=Rendered[(Y+DY)*768+X].R; return Value/(9*19); };
		TestTrue(TEXT("Gobo projects distinct lit and masked bands"),MeanRed(350)>MeanRed(410)+15);
		int32 CyanPixels=0,MagentaPixels=0;
		for (const auto& Pixel : Rendered) { if (Pixel.B>100 && Pixel.G>100 && Pixel.B>Pixel.R+25) ++CyanPixels; if (Pixel.R>100 && Pixel.B>100 && Pixel.B>Pixel.G+25) ++MagentaPixels; }
		TestTrue(TEXT("Laser renders independently colored segments"),CyanPixels>20 && MagentaPixels>20);
	}
	FWidgetRenderer Renderer(true,true);
	TestTrue(TEXT("Render shared scenic builder"),Save(Renderer.DrawWidget(MakeTSAVSceneBuilder(World,FSimpleDelegate()),FVector2D(1200,900)),TEXT("NativeScenicBuilder")));
	TestTrue(TEXT("Render shared laser panel"),Save(Renderer.DrawWidget(MakeTSAVLaserPanel(World,FSimpleDelegate()),FVector2D(1200,900)),TEXT("NativeLaserPanel")));
	TestTrue(TEXT("Render shared rig panel"),Save(Renderer.DrawWidget(MakeTSAVRigPanel(World,FSimpleDelegate()),FVector2D(1200,900)),TEXT("NativeRigPanel")));
	TestTrue(TEXT("Render shared network panel"),Save(Renderer.DrawWidget(MakeTSAVDMXNetworkPanel(World,FSimpleDelegate()),FVector2D(1200,900)),TEXT("NativeNetworkPanel")));
	return true;
	}));
	return true;
}
#endif
