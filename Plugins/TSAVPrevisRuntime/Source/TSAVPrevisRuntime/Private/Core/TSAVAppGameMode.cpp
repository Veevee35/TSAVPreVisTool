// Copyright TSAV. All Rights Reserved.

#include "Core/TSAVAppGameMode.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Interaction/TSAVEditPawn.h"
#include "Interaction/TSAVPlayerController.h"
#include "Interaction/TSAVSceneObjectActor.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVAppGameMode)

ATSAVAppGameMode::ATSAVAppGameMode()
{
	DefaultPawnClass = ATSAVEditPawn::StaticClass();
	PlayerControllerClass = ATSAVPlayerController::StaticClass();
}

void ATSAVAppGameMode::StartPlay()
{
	Super::StartPlay();

	if (ShouldCreateApplicationEnvironment())
	{
		CreateApplicationEnvironment();
	}
}

void ATSAVAppGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	const FTransform StartTransform(FRotator(-18.0f, 38.0f, 0.0f), FVector(-1100.0f, -900.0f, 650.0f));
	if (NewPlayer && !NewPlayer->GetPawn())
	{
		RestartPlayerAtTransform(NewPlayer, StartTransform);
	}

	if (ATSAVEditPawn* EditPawn = NewPlayer ? Cast<ATSAVEditPawn>(NewPlayer->GetPawn()) : nullptr)
	{
		EditPawn->SetActorTransform(StartTransform);
		NewPlayer->SetControlRotation(StartTransform.Rotator());
	}
}

bool ATSAVAppGameMode::ShouldCreateApplicationEnvironment() const
{
	const UWorld* World = GetWorld();
	const FString MapName = World ? World->GetMapName() : FString();
	return MapName.Contains(TEXT("Entry")) || MapName.Contains(TEXT("L_TSAV_App"));
}

void ATSAVAppGameMode::CreateApplicationEnvironment()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bool bHasStaticMeshActor = false;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		bHasStaticMeshActor = true;
		break;
	}

	if (!bHasStaticMeshActor)
	{
		UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
		if (PlaneMesh)
		{
			AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(FVector::ZeroVector, FRotator::ZeroRotator);
			if (Floor && Floor->GetStaticMeshComponent())
			{
				Floor->GetStaticMeshComponent()->SetStaticMesh(PlaneMesh);
				Floor->GetStaticMeshComponent()->SetWorldScale3D(FVector(40.0f, 40.0f, 1.0f));
				Floor->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			}
		}
	}

	bool bHasDirectionalLight = false;
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		bHasDirectionalLight = true;
		break;
	}
	if (!bHasDirectionalLight)
	{
		if (ADirectionalLight* DirectionalLight = World->SpawnActor<ADirectionalLight>(FVector(0.0f, 0.0f, 1200.0f), FRotator(-48.0f, -32.0f, 0.0f)))
		{
			DirectionalLight->GetLightComponent()->SetIntensity(5.0f);
		}
	}

	bool bHasSkyLight = false;
	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		bHasSkyLight = true;
		break;
	}
	if (!bHasSkyLight)
	{
		if (ASkyLight* SkyLight = World->SpawnActor<ASkyLight>())
		{
			SkyLight->GetLightComponent()->SetIntensity(1.5f);
		}
	}

	const FVector Positions[] =
	{
		FVector(-250.0f, -100.0f, 75.0f),
		FVector(0.0f, 100.0f, 125.0f),
		FVector(280.0f, -80.0f, 100.0f),
	};
	const FVector Scales[] =
	{
		FVector(1.5f, 1.5f, 1.5f),
		FVector(2.0f, 1.0f, 2.5f),
		FVector(1.2f, 2.2f, 2.0f),
	};

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Positions); ++Index)
	{
		if (ATSAVSceneObjectActor* SceneObject = World->SpawnActor<ATSAVSceneObjectActor>(Positions[Index], FRotator::ZeroRotator))
		{
			SceneObject->SetActorScale3D(Scales[Index]);
			if (UTSAVSceneObjectComponent* SceneObjectData = SceneObject->GetSceneObjectComponent())
			{
				SceneObjectData->DisplayName = FText::Format(NSLOCTEXT("TSAVPreVis", "DemoObjectName", "Demo Object {0}"), FText::AsNumber(Index + 1));
			}
		}
	}
}
