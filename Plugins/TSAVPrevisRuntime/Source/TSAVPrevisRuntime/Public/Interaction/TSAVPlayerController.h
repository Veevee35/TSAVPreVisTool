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
class ATSAVTransformGizmoActor;
enum class ETSAVTransformMode : uint8;

/** Runtime controller that owns TSAV input contexts and viewport interaction. */
UCLASS()
class TSAVPREVISRUNTIME_API ATSAVPlayerController final : public APlayerController
{
	GENERATED_BODY()

public:
	ATSAVPlayerController();
	virtual void PlayerTick(float DeltaTime) override;

	/** Menu-callable transform and viewport controls. */
	void SetTransformTool(ETSAVTransformMode NewMode);
	void ToggleTransformCoordinateSpace();
	void FrameSelection();
	void ViewThroughCamera(AActor* CameraActor);
	void ReturnToEditorCamera();

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
	void EndPrimaryInteraction(const FInputActionValue& Value);
	void ReturnToEditMode(const FInputActionValue& Value);
	void SetTranslateMode(const FInputActionValue& Value);
	void SetRotateMode(const FInputActionValue& Value);
	void SetScaleMode(const FInputActionValue& Value);
	void ToggleCoordinateSpace(const FInputActionValue& Value);
	void DeleteSelection(const FInputActionValue& Value);
	void DuplicateSelection(const FInputActionValue& Value);
	void UndoCommand(const FInputActionValue& Value);
	void RedoCommand(const FInputActionValue& Value);
	void SaveProject(const FInputActionValue& Value);
	void LoadProject(const FInputActionValue& Value);
	void SpawnCube(const FInputActionValue& Value);
	bool BeginGizmoDrag(const FVector2D& ScreenPosition);
	void UpdateGizmoDrag();
	void EndGizmoDrag(bool bCancel);
	bool GetCursorRay(FVector& OutOrigin, FVector& OutDirection, FVector2D* OutScreenPosition = nullptr) const;
	static bool GetAxisRayParameter(const FVector& RayOrigin, const FVector& RayDirection, const FVector& AxisOrigin, const FVector& AxisDirection, double& OutAxisParameter);
	bool IsControlDown() const;

	UFUNCTION()
	void HandleSelectionChanged(AActor* SelectedActor);

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
	TObjectPtr<UInputAction> TranslateModeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RotateModeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ScaleModeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ToggleCoordinateSpaceAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DeleteAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DuplicateAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> UndoAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RedoAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SaveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SpawnCubeAction;

	UPROPERTY(Transient)
	TObjectPtr<UTSAVMainWidget> MainWidget;

	UPROPERTY(Transient)
	TObjectPtr<ATSAVTransformGizmoActor> TransformGizmo;

	TWeakObjectPtr<AActor> DragActor;
	FTransform DragStartTransform;
	FVector DragAxisOrigin = FVector::ZeroVector;
	FVector DragWorldAxis = FVector::XAxisVector;
	FVector2D DragStartScreenPosition = FVector2D::ZeroVector;
	double DragStartAxisParameter = 0.0;
	int32 DragAxisIndex = 0;
	bool bDraggingGizmo = false;
	float TranslationGridSize = 10.0f;
	float RotationGridDegrees = 15.0f;
	float ScaleGridSize = 0.1f;
};
