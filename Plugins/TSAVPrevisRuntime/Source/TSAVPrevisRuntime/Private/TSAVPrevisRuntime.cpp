// Copyright TSAV. All Rights Reserved.

#include "TSAVPrevisRuntime.h"

#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/PointLight.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/TSAVCommandSubsystem.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "Misc/CoreDelegates.h"
#include "Containers/Ticker.h"
#include "Project/TSAVProjectSubsystem.h"
#include "TSAVLEDWall.h"
#include "TSAVVideoSwitcher.h"
#include "UI/TSAVLEDWallConfiguratorWidget.h"
#include "Video/TSAVCameraActor.h"

#define LOCTEXT_NAMESPACE "FTSAVPrevisRuntimeModule"

DEFINE_LOG_CATEGORY(LogTSAVPrevisRuntime);

#if !UE_BUILD_SHIPPING
namespace TSAVPhase2Validation::Private
{
	AActor* FindById(UWorld* World, const FGuid& ObjectId)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const UTSAVSceneObjectComponent* SceneObject = It->FindComponentByClass<UTSAVSceneObjectComponent>();
			if (SceneObject && SceneObject->ObjectId == ObjectId)
			{
				return *It;
			}
		}
		return nullptr;
	}

	void Validate(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		UTSAVCommandSubsystem* Commands = GameInstance ? GameInstance->GetSubsystem<UTSAVCommandSubsystem>() : nullptr;
		UTSAVProjectSubsystem* Project = GameInstance ? GameInstance->GetSubsystem<UTSAVProjectSubsystem>() : nullptr;
		auto Require = [](const bool bCondition, const TCHAR* Message)
		{
			if (!bCondition)
			{
				UE_LOG(LogTSAVPrevisRuntime, Error, TEXT("CODEX_TSAV_PHASE2_VALIDATION_FAILURE: %s"), Message);
			}
			return bCondition;
		};

		if (!Require(World && Commands && Project, TEXT("Runtime subsystems unavailable")))
		{
			return;
		}

		Project->NewProject(TEXT("Phase 2 Validation"));
		AActor* Actor = Commands->SpawnSceneObject(World, FTransform(FRotator::ZeroRotator, FVector(10.0f, 20.0f, 30.0f)), FText::FromString(TEXT("Validation Cube")));
		UTSAVSceneObjectComponent* SceneObject = Actor ? Actor->FindComponentByClass<UTSAVSceneObjectComponent>() : nullptr;
		if (!Require(SceneObject != nullptr, TEXT("Spawn command failed"))) { return; }
		const FGuid ObjectId = SceneObject->ObjectId;

		Commands->Undo();
		if (!Require(FindById(World, ObjectId) == nullptr, TEXT("Spawn undo failed"))) { return; }
		Commands->Redo();
		Actor = FindById(World, ObjectId);
		if (!Require(Actor != nullptr, TEXT("Spawn redo failed"))) { return; }

		const FTransform StartTransform = Actor->GetActorTransform();
		FTransform MovedTransform = StartTransform;
		MovedTransform.SetLocation(FVector(410.0f, -220.0f, 130.0f));
		MovedTransform.SetRotation(FRotator(15.0f, 35.0f, 5.0f).Quaternion());
		MovedTransform.SetScale3D(FVector(1.5f, 0.8f, 2.0f));
		Commands->SetActorTransform(Actor, MovedTransform, FText::FromString(TEXT("Validation Transform")));
		Commands->Undo();
		if (!Require(Actor->GetActorTransform().Equals(StartTransform), TEXT("Transform undo failed"))) { return; }
		Commands->Redo();
		if (!Require(Actor->GetActorTransform().Equals(MovedTransform), TEXT("Transform redo failed"))) { return; }

		Commands->SetDisplayName(Actor, FText::FromString(TEXT("Renamed Validation Cube")));
		Commands->Undo();
		if (!Require(Actor->FindComponentByClass<UTSAVSceneObjectComponent>()->DisplayName.ToString() == TEXT("Validation Cube"), TEXT("Property undo failed"))) { return; }
		Commands->Redo();

		AActor* Duplicate = Commands->DuplicateActor(Actor);
		UTSAVSceneObjectComponent* DuplicateObject = Duplicate ? Duplicate->FindComponentByClass<UTSAVSceneObjectComponent>() : nullptr;
		if (!Require(DuplicateObject && DuplicateObject->ObjectId != ObjectId, TEXT("Duplicate command failed"))) { return; }
		const FGuid DuplicateId = DuplicateObject->ObjectId;
		Commands->Undo();
		if (!Require(FindById(World, DuplicateId) == nullptr, TEXT("Duplicate undo failed"))) { return; }
		Commands->Redo();
		Duplicate = FindById(World, DuplicateId);
		Commands->DeleteActor(Duplicate);
		if (!Require(FindById(World, DuplicateId) == nullptr, TEXT("Delete command failed"))) { return; }
		Commands->Undo();
		if (!Require(FindById(World, DuplicateId) != nullptr, TEXT("Delete undo failed"))) { return; }

		AActor* LEDWall = Commands->SpawnActorClass(
			World,
			ATSAVLEDWall::StaticClass(),
			FTransform(FRotator::ZeroRotator, FVector(700.0f, 0.0f, 200.0f)),
			FText::FromString(TEXT("Validation LED Wall")),
			ETSAVObjectType::LED);
		UTSAVSceneObjectComponent* LEDWallObject = LEDWall ? LEDWall->FindComponentByClass<UTSAVSceneObjectComponent>() : nullptr;
		if (!Require(LEDWallObject && LEDWallObject->ObjectType == ETSAVObjectType::LED, TEXT("Generic LED wall spawn failed"))) { return; }
		const FGuid LEDWallId = LEDWallObject->ObjectId;

		AActor* PointLight = Commands->SpawnActorClass(
			World,
			APointLight::StaticClass(),
			FTransform(FRotator::ZeroRotator, FVector(300.0f, 200.0f, 400.0f)),
			FText::FromString(TEXT("Validation Point Light")),
			ETSAVObjectType::Fixture);
		UTSAVSceneObjectComponent* LightObject = PointLight ? PointLight->FindComponentByClass<UTSAVSceneObjectComponent>() : nullptr;
		if (!Require(LightObject && LightObject->ObjectType == ETSAVObjectType::Fixture, TEXT("Generic light spawn failed"))) { return; }
		const FGuid LightId = LightObject->ObjectId;
		Commands->Undo();
		if (!Require(FindById(World, LightId) == nullptr, TEXT("Generic actor undo failed"))) { return; }
		Commands->Redo();
		if (!Require(FindById(World, LightId) != nullptr, TEXT("Generic actor redo failed"))) { return; }

		ATSAVCameraActor* CameraOne = Cast<ATSAVCameraActor>(Commands->SpawnActorClass(
			World, ATSAVCameraActor::StaticClass(), FTransform(FRotator::ZeroRotator, FVector(0.0f, -300.0f, 200.0f)),
			FText::FromString(TEXT("CAM 1")), ETSAVObjectType::Camera));
		ATSAVCameraActor* CameraTwo = Cast<ATSAVCameraActor>(Commands->SpawnActorClass(
			World, ATSAVCameraActor::StaticClass(), FTransform(FRotator::ZeroRotator, FVector(0.0f, 300.0f, 200.0f)),
			FText::FromString(TEXT("CAM 2")), ETSAVObjectType::Camera));
		if (!Require(CameraOne && CameraTwo, TEXT("Production camera spawn failed"))) { return; }
		const FString CameraTwoBefore = CameraTwo->CaptureTSAVState();
		CameraTwo->CameraLabel = FText::FromString(TEXT("CAM 2"));
		CameraTwo->SetCameraType(ETSAVCameraType::PTZ);
		CameraTwo->SetLensPreset(ETSAVLensPreset::PTZZoom);
		CameraTwo->ApplyPTZ(25.0f, 10.0f, 0.45f, false);
		Commands->CommitAppliedActorState(CameraTwo, CameraTwoBefore, FText::FromString(TEXT("Configure Validation Camera")));
		const FGuid CameraOneObjectId = CameraOne->FindComponentByClass<UTSAVSceneObjectComponent>()->ObjectId;
		const FGuid CameraTwoObjectId = CameraTwo->FindComponentByClass<UTSAVSceneObjectComponent>()->ObjectId;

		ATSAVVideoSwitcher* Switcher = Cast<ATSAVVideoSwitcher>(Commands->SpawnActorClass(
			World, ATSAVVideoSwitcher::StaticClass(), FTransform(FRotator::ZeroRotator, FVector(200.0f, 0.0f, 100.0f)),
			FText::FromString(TEXT("Validation Switcher")), ETSAVObjectType::Video));
		if (!Require(Switcher != nullptr, TEXT("Video switcher spawn failed"))) { return; }
		const FGuid NormalizedNDIInputId = Switcher->AddStreamInput(FText::FromString(TEXT("NDI Validation")), TEXT("TSAV Validation Sender"));
		const FTSAVVideoInput* NormalizedNDIInput = Switcher->Inputs.FindByPredicate([NormalizedNDIInputId](const FTSAVVideoInput& Input)
		{
			return Input.InputId == NormalizedNDIInputId;
		});
		if (!Require(NormalizedNDIInput && NormalizedNDIInput->StreamUrl == TEXT("ndi://TSAV Validation Sender"),
			TEXT("Bare NDI source name normalization failed"))) { return; }
		Switcher->RemoveInput(NormalizedNDIInputId);
		Switcher->DiscoverSources();
		const FTSAVVideoInput* CameraOneInput = Switcher->Inputs.FindByPredicate([CameraOne](const FTSAVVideoInput& Input) { return Input.ProviderId == CameraOne->CameraId; });
		const FTSAVVideoInput* CameraTwoInput = Switcher->Inputs.FindByPredicate([CameraTwo](const FTSAVVideoInput& Input) { return Input.ProviderId == CameraTwo->CameraId; });
		if (!Require(CameraOneInput && CameraTwoInput, TEXT("Camera feed discovery failed"))) { return; }
		const FGuid CameraOneInputId = CameraOneInput->InputId;
		const FGuid CameraTwoInputId = CameraTwoInput->InputId;
		Switcher->SetBusInput(TEXT("Preview"), CameraOneInputId);
		Switcher->SetBusInput(TEXT("Program"), CameraTwoInputId);
		if (!Require(Switcher->GetOutputTexture(TEXT("Program")) == CameraTwo->GetTSAVVideoTexture(), TEXT("Switcher camera route failed"))) { return; }
		ATSAVLEDWall* RoutedWall = Cast<ATSAVLEDWall>(LEDWall);
		const FString WallBefore = RoutedWall->CaptureTSAVState();
		RoutedWall->Columns = 5;
		RoutedWall->Rows = 3;
		RoutedWall->PanelResolutionX = 160;
		RoutedWall->PanelResolutionY = 120;
		RoutedWall->ColumnSeamAnglesDegrees = { 0.0f, 15.0f, 15.0f, 0.0f };
		RoutedWall->RowSeamAnglesDegrees = { 0.0f, -5.0f };
		RoutedWall->ColumnInternalCurveEnabled = { false, false, true, false, false };
		RoutedWall->ColumnInternalCurveAngleADegrees = { 0.0f, 0.0f, 30.0f, 0.0f, 0.0f };
		RoutedWall->ColumnInternalCurveAngleBDegrees = { 0.0f, 0.0f, 30.0f, 0.0f, 0.0f };
		RoutedWall->SubpixelLayout = ETSAVLEDSubpixelLayout::RoundLinear;
		RoutedWall->SubpixelStrength = 0.85f;
		RoutedWall->PanelEdgeStyles.Init(ETSAVLEDPanelEdgeStyle::Square, RoutedWall->Columns * RoutedWall->Rows);
		RoutedWall->PanelEdgeStyles[0] = ETSAVLEDPanelEdgeStyle::DiagonalTopLeft;
		RoutedWall->RebuildPanelLayout();
		Commands->CommitAppliedActorState(RoutedWall, WallBefore, FText::FromString(TEXT("Configure Validation LED Wall")));
		if (!Require(RoutedWall->GetWallResolutionPixels() == FIntPoint(800, 360), TEXT("Runtime LED configurator failed"))) { return; }
		APlayerController* ValidationController = World->GetFirstPlayerController();
		UTSAVLEDWallConfiguratorWidget* FullscreenConfigurator = ValidationController
			? CreateWidget<UTSAVLEDWallConfiguratorWidget>(ValidationController) : nullptr;
		if (!Require(FullscreenConfigurator != nullptr, TEXT("Full-screen LED configurator could not be created"))) { return; }
		FullscreenConfigurator->OpenForWall(RoutedWall);
		FullscreenConfigurator->AddToViewport(100);
		FullscreenConfigurator->OpenForWall(RoutedWall);
		if (!Require(FullscreenConfigurator->GetConfiguredWall() == RoutedWall && FullscreenConfigurator->GetPanelCellCount() == 15,
			TEXT("Full-screen LED configurator did not load the complete cabinet grid"))) { return; }
		if (!Require(FullscreenConfigurator->GetPanelDefinitionCount() > 0,
			TEXT("Cooked LED cabinet definition library is unavailable in the full-screen configurator"))) { return; }
		FullscreenConfigurator->CloseConfigurator();
		if (!Require(LoadObject<UMaterialInterface>(nullptr, TEXT("/TSAVLEDTools/Materials/M_TSAV_LEDCanvasVideo.M_TSAV_LEDCanvasVideo")) != nullptr,
			TEXT("Packaged LED video material missing"))) { return; }
		if (!Require(LoadObject<UTexture>(nullptr, TEXT("/TSAVLEDTools/Subpixels/T_TSAV_Subpixel_RectangleRGB.T_TSAV_Subpixel_RectangleRGB")) != nullptr,
			TEXT("Packaged LED subpixel texture missing"))) { return; }
		if (!Require(LoadObject<UTexture>(nullptr, TEXT("/TSAVLEDTools/Subpixels/T_TSAV_Subpixel_RoundLinear.T_TSAV_Subpixel_RoundLinear")) != nullptr,
			TEXT("Packaged Round Linear subpixel texture missing"))) { return; }
		RoutedWall->SetVideoRoute(Switcher, TEXT("Program"));
		if (!Require(RoutedWall->GetDisplayedVideoTexture() == CameraTwo->GetTSAVVideoTexture(), TEXT("LED shader did not bind routed camera texture"))) { return; }
		const FString SwitcherBeforeCut = Switcher->CaptureTSAVState();
		Switcher->Cut();
		Commands->CommitAppliedActorState(Switcher, SwitcherBeforeCut, FText::FromString(TEXT("Validation Switcher Cut")));
		if (!Require(Switcher->GetBusInputId(TEXT("Program")) == CameraOneInputId, TEXT("Switcher cut failed"))) { return; }
		if (!Require(RoutedWall->GetDisplayedVideoTexture() == CameraOne->GetTSAVVideoTexture(), TEXT("LED texture did not follow Program cut"))) { return; }
		Commands->Undo();
		if (!Require(Switcher->GetBusInputId(TEXT("Program")) == CameraTwoInputId, TEXT("Switcher cut undo failed"))) { return; }
		Commands->Redo();
		const FGuid SwitcherObjectId = Switcher->FindComponentByClass<UTSAVSceneObjectComponent>()->ObjectId;

		const FString ValidationPath = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("Phase2Validation.tsav")));
		if (!Require(Project->SaveProject(ValidationPath), TEXT(".tsav save failed"))) { return; }
		Actor->SetActorLocation(FVector(9999.0f));
		if (!Require(Project->LoadProject(ValidationPath), TEXT(".tsav load failed"))) { return; }
		Actor = FindById(World, ObjectId);
		SceneObject = Actor ? Actor->FindComponentByClass<UTSAVSceneObjectComponent>() : nullptr;
		if (!Require(Actor && SceneObject, TEXT("Persistent GUID restoration failed"))) { return; }
		if (!Require(Actor->GetActorTransform().Equals(MovedTransform), TEXT("Persistent transform restoration failed"))) { return; }
		if (!Require(SceneObject->DisplayName.ToString() == TEXT("Renamed Validation Cube"), TEXT("Persistent property restoration failed"))) { return; }
		if (!Require(FindById(World, DuplicateId) != nullptr, TEXT("Persistent duplicate restoration failed"))) { return; }
		if (!Require(Cast<ATSAVLEDWall>(FindById(World, LEDWallId)) != nullptr, TEXT("Persistent LED wall restoration failed"))) { return; }
		if (!Require(Cast<APointLight>(FindById(World, LightId)) != nullptr, TEXT("Persistent light restoration failed"))) { return; }
		CameraOne = Cast<ATSAVCameraActor>(FindById(World, CameraOneObjectId));
		CameraTwo = Cast<ATSAVCameraActor>(FindById(World, CameraTwoObjectId));
		Switcher = Cast<ATSAVVideoSwitcher>(FindById(World, SwitcherObjectId));
		RoutedWall = Cast<ATSAVLEDWall>(FindById(World, LEDWallId));
		if (!Require(CameraOne && CameraTwo && Switcher && RoutedWall, TEXT("Persistent video tool restoration failed"))) { return; }
		if (!Require(CameraTwo->CameraType == ETSAVCameraType::PTZ && FMath::IsNearlyEqual(CameraTwo->ZoomNormalized, 0.45f), TEXT("Persistent camera configuration failed"))) { return; }
		if (!Require(RoutedWall->bUseVideoSwitcher && RoutedWall->GetVideoSwitcher() == Switcher, TEXT("Persistent surface route failed"))) { return; }
		if (!Require(RoutedWall->Columns == 5 && RoutedWall->Rows == 3 && RoutedWall->ColumnInternalCurveEnabled.IsValidIndex(2) &&
			RoutedWall->ColumnInternalCurveEnabled[2] && RoutedWall->GetPanelEdgeStyle(0, 0) == ETSAVLEDPanelEdgeStyle::DiagonalTopLeft &&
			RoutedWall->SubpixelLayout == ETSAVLEDSubpixelLayout::RoundLinear && FMath::IsNearlyEqual(RoutedWall->SubpixelStrength, 0.85f),
			TEXT("Persistent LED configurator state failed"))) { return; }
		if (!Require(Switcher->GetOutputTexture(TEXT("Program")) == CameraOne->GetTSAVVideoTexture(), TEXT("Persistent switcher bus failed"))) { return; }
		if (!Require(RoutedWall->GetDisplayedVideoTexture() == CameraOne->GetTSAVVideoTexture(), TEXT("Persistent LED video texture binding failed"))) { return; }

		UE_LOG(LogTSAVPrevisRuntime, Display, TEXT("CODEX_TSAV_PHASE2_COMMAND_PERSISTENCE_SUCCESS path=%s"), *ValidationPath);
		UE_LOG(LogTSAVPrevisRuntime, Display, TEXT("CODEX_TSAV_VIDEO_SWITCHER_CAMERA_VISCA_SUCCESS inputs=%d program=%s"),
			Switcher->Inputs.Num(), *Switcher->GetBusInputLabel(TEXT("Program")).ToString());
		UE_LOG(LogTSAVPrevisRuntime, Display, TEXT("CODEX_TSAV_LED_RUNTIME_CONFIGURATOR_VIDEO_SUCCESS columns=%d rows=%d texture=%s"),
			RoutedWall->Columns, RoutedWall->Rows, *GetNameSafe(RoutedWall->GetDisplayedVideoTexture()));
		UE_LOG(LogTSAVPrevisRuntime, Display, TEXT("CODEX_TSAV_LED_FULLSCREEN_CONFIGURATOR_SUCCESS panels=%d presets=%d"),
			RoutedWall->Columns * RoutedWall->Rows, FullscreenConfigurator->GetPanelDefinitionCount());
	}

	FAutoConsoleCommandWithWorld ValidationCommand(
		TEXT("tsav.ValidatePhase2"),
		TEXT("Validate TSAV runtime commands, undo/redo, stable IDs, and .tsav persistence."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Validate));

	bool DeferredValidateAndQuit(float DeltaSeconds)
	{
		if (!GEngine)
		{
			return true;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && World->IsGameWorld() && World->HasBegunPlay() && World->GetGameInstance())
			{
				Validate(World);
				FPlatformMisc::RequestExit(false);
				return false;
			}
		}
		return true;
	}

	void QueueValidationAndQuit()
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&DeferredValidateAndQuit), 0.1f);
	}

	FAutoConsoleCommand DeferredValidationCommand(
		TEXT("tsav.ValidatePhase2AndQuit"),
		TEXT("Run Phase 2 validation after the app world begins play, then exit."),
		FConsoleCommandDelegate::CreateStatic(&QueueValidationAndQuit));
}
#endif

void FTSAVPrevisRuntimeModule::StartupModule()
{
}

void FTSAVPrevisRuntimeModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTSAVPrevisRuntimeModule, TSAVPrevisRuntime)
