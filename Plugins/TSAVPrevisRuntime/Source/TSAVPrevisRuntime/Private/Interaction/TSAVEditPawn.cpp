// Copyright TSAV. All Rights Reserved.

#include "Interaction/TSAVEditPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/FloatingPawnMovement.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVEditPawn)

ATSAVEditPawn::ATSAVEditPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewportCamera"));
	CameraComponent->SetupAttachment(SceneRoot);
	CameraComponent->bUsePawnControlRotation = true;

	MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	MovementComponent->UpdatedComponent = SceneRoot;
	MovementComponent->MaxSpeed = 2400.0f;
	MovementComponent->Acceleration = 12000.0f;
	MovementComponent->Deceleration = 16000.0f;

	bUseControllerRotationPitch = true;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;
}

void ATSAVEditPawn::AddFlyMovement(const FVector& WorldDirection, const float ScaleValue)
{
	AddMovementInput(WorldDirection, ScaleValue);
}

void ATSAVEditPawn::SetMoveSpeed(const float NewMoveSpeed)
{
	if (MovementComponent)
	{
		MovementComponent->MaxSpeed = FMath::Clamp(NewMoveSpeed, 100.0f, 20000.0f);
	}
}

float ATSAVEditPawn::GetMoveSpeed() const
{
	return MovementComponent ? MovementComponent->MaxSpeed : 0.0f;
}
