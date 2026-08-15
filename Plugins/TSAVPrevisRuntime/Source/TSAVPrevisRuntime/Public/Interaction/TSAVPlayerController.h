// Copyright TSAV. All Rights Reserved.

#pragma once

#include "Core/TSAVTypes.h"
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"

#include "TSAVPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UTSAVMainWidget;

/** Runtime controller that owns TSAV input contexts and viewport interaction. */
UCLASS()
class TSAVPREVISRUNTIME_API ATSAVPlayerController final : public APlayerController
{
	GENERATED_BODY()

public:
	ATSAVPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

private:
	void CreateRuntimeInputAssets();
	void ApplyMappingContexts();
	void MoveForward(const FInputActionValue& Value);
	void MoveRight(const FInputActionValue& Value);
	void MoveUp(const FInputActionValue& Value);
	void LookYaw(const FInputActionValue& Value);
	void LookPitch(const FInputActionValue& Value);
	void AdjustMoveSpeed(const FInputActionValue& Value);
	void SelectAtCursor(const FInputActionValue& Value);
	void ReturnToEditMode(const FInputActionValue& Value);

	UFUNCTION()
	void HandleModeChanged(ETSAVAppMode NewMode, ETSAVAppMode PreviousMode);

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> CommonMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> EditMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveForwardAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveRightAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveUpAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookYawAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookPitchAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AdjustSpeedAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SelectAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ReturnToEditAction;

	UPROPERTY(Transient)
	TObjectPtr<UTSAVMainWidget> MainWidget;
};
