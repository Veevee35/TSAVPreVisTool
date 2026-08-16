// Copyright TSAV. All Rights Reserved.

#include "TSAVPrevisRuntime.h"

#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/TSAVCommandSubsystem.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "Misc/Paths.h"
#include "Misc/CoreDelegates.h"
#include "Containers/Ticker.h"
#include "Project/TSAVProjectSubsystem.h"
#include "TSAVLEDWall.h"

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

		const FString ValidationPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("Phase2Validation.tsav"));
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

		UE_LOG(LogTSAVPrevisRuntime, Display, TEXT("CODEX_TSAV_PHASE2_COMMAND_PERSISTENCE_SUCCESS path=%s"), *ValidationPath);
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
