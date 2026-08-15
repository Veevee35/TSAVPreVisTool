// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "TSAVEditPawn.generated.h"

class UCameraComponent;
class UFloatingPawnMovement;
class USceneComponent;

/** Free-flying design viewport pawn for the standalone authoring shell. */
UCLASS()
class TSAVPREVISRUNTIME_API ATSAVEditPawn final : public APawn
{
	GENERATED_BODY()

public:
	ATSAVEditPawn();

	void AddFlyMovement(const FVector& WorldDirection, float ScaleValue);

	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Navigation")
	void SetMoveSpeed(float NewMoveSpeed);

	UFUNCTION(BlueprintPure, Category = "TSAV PreVis|Navigation")
	float GetMoveSpeed() const;

	UCameraComponent* GetCameraComponent() const { return CameraComponent; }

private:
	UPROPERTY(VisibleAnywhere, Category = "TSAV PreVis|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "TSAV PreVis|Components")
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(VisibleAnywhere, Category = "TSAV PreVis|Components")
	TObjectPtr<UFloatingPawnMovement> MovementComponent;
};
