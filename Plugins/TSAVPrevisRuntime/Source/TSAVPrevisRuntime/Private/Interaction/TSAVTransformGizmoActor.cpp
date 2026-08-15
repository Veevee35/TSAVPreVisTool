// Copyright TSAV. All Rights Reserved.

#include "Interaction/TSAVTransformGizmoActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVTransformGizmoActor)

namespace TSAVTransformGizmo::Private
{
	const FName GizmoHandleTag(TEXT("TSAVGizmoHandle"));
	const FName ColorParameter(TEXT("Color"));
}

ATSAVTransformGizmoActor::ATSAVTransformGizmoActor()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GizmoRoot"));
	SetRootComponent(SceneRoot);

	AxisX = CreateAxisHandle(TEXT("AxisX"), FLinearColor(0.95f, 0.08f, 0.06f));
	AxisY = CreateAxisHandle(TEXT("AxisY"), FLinearColor(0.08f, 0.8f, 0.16f));
	AxisZ = CreateAxisHandle(TEXT("AxisZ"), FLinearColor(0.08f, 0.35f, 1.0f));

	AxisX->SetupAttachment(SceneRoot);
	AxisY->SetupAttachment(SceneRoot);
	AxisZ->SetupAttachment(SceneRoot);
	UpdateHandleAppearance();
	SetActorHiddenInGame(true);
}

void ATSAVTransformGizmoActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateFromTarget();
}

void ATSAVTransformGizmoActor::SetTargetActor(AActor* Actor)
{
	TargetActor = Actor;
	SetActorHiddenInGame(!IsValid(Actor));
	SetActorEnableCollision(IsValid(Actor));
	UpdateFromTarget();
}

void ATSAVTransformGizmoActor::SetTransformMode(const ETSAVTransformMode NewMode)
{
	TransformMode = NewMode;
	UpdateHandleAppearance();
}

void ATSAVTransformGizmoActor::SetCoordinateSpace(const ETSAVCoordinateSpace NewSpace)
{
	CoordinateSpace = NewSpace;
	UpdateFromTarget();
}

bool ATSAVTransformGizmoActor::GetAxisForComponent(const UPrimitiveComponent* Component, int32& OutAxisIndex, FVector& OutWorldAxis) const
{
	if (Component == AxisX)
	{
		OutAxisIndex = 0;
		OutWorldAxis = GetActorTransform().TransformVectorNoScale(FVector::XAxisVector).GetSafeNormal();
		return true;
	}
	if (Component == AxisY)
	{
		OutAxisIndex = 1;
		OutWorldAxis = GetActorTransform().TransformVectorNoScale(FVector::YAxisVector).GetSafeNormal();
		return true;
	}
	if (Component == AxisZ)
	{
		OutAxisIndex = 2;
		OutWorldAxis = GetActorTransform().TransformVectorNoScale(FVector::ZAxisVector).GetSafeNormal();
		return true;
	}
	return false;
}

void ATSAVTransformGizmoActor::UpdateFromTarget()
{
	AActor* Target = TargetActor.Get();
	if (!IsValid(Target))
	{
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
		return;
	}

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	const FRotator GizmoRotation = CoordinateSpace == ETSAVCoordinateSpace::Local ? Target->GetActorRotation() : FRotator::ZeroRotator;
	SetActorLocationAndRotation(Target->GetActorLocation(), GizmoRotation, false, nullptr, ETeleportType::TeleportPhysics);
}

void ATSAVTransformGizmoActor::UpdateHandleAppearance()
{
	const FVector ShaftScale = TransformMode == ETSAVTransformMode::Rotate ? FVector(0.065f, 0.065f, 0.75f) : FVector(0.08f, 0.08f, 1.0f);
	AxisX->SetRelativeLocation(FVector(50.0f, 0.0f, 0.0f));
	AxisX->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	AxisX->SetRelativeScale3D(ShaftScale);
	AxisY->SetRelativeLocation(FVector(0.0f, 50.0f, 0.0f));
	AxisY->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	AxisY->SetRelativeScale3D(ShaftScale);
	AxisZ->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	AxisZ->SetRelativeRotation(FRotator::ZeroRotator);
	AxisZ->SetRelativeScale3D(ShaftScale);
}

UStaticMeshComponent* ATSAVTransformGizmoActor::CreateAxisHandle(const FName& Name, const FLinearColor& Color)
{
	UStaticMeshComponent* Handle = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Handle->ComponentTags.Add(TSAVTransformGizmo::Private::GizmoHandleTag);
	Handle->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Handle->SetCollisionObjectType(ECC_WorldDynamic);
	Handle->SetCollisionResponseToAllChannels(ECR_Ignore);
	Handle->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Handle->SetGenerateOverlapEvents(false);
	Handle->SetCastShadow(false);
	Handle->SetRenderCustomDepth(true);
	Handle->SetCustomDepthStencilValue(251);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Handle->SetStaticMesh(CylinderMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterial.Succeeded())
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BasicMaterial.Object, this);
		Material->SetVectorParameterValue(TSAVTransformGizmo::Private::ColorParameter, Color);
		Handle->SetMaterial(0, Material);
		AxisMaterials.Add(Material);
	}
	return Handle;
}
