// Copyright TSAV. All Rights Reserved.

#include "Interaction/TSAVSceneObjectActor.h"

#include "Components/StaticMeshComponent.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVSceneObjectActor)

ATSAVSceneObjectActor::ATSAVSceneObjectActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMesh.Object);
	}

	SceneObjectComponent = CreateDefaultSubobject<UTSAVSceneObjectComponent>(TEXT("SceneObject"));
	SceneObjectComponent->DisplayName = NSLOCTEXT("TSAVPreVis", "DefaultSceneObjectName", "Scene Object");
	SceneObjectComponent->ObjectType = ETSAVObjectType::Scenic;
}

bool ATSAVSceneObjectActor::CanSelect_Implementation() const
{
	return SceneObjectComponent && !SceneObjectComponent->bLocked && SceneObjectComponent->bVisible;
}

void ATSAVSceneObjectActor::OnSelectionChanged_Implementation(const bool bSelected)
{
	if (SceneObjectComponent)
	{
		SceneObjectComponent->SetSelected(bSelected);
	}
}
