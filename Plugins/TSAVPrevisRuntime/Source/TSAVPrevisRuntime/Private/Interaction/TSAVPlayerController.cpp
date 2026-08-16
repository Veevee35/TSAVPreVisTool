// Copyright TSAV. All Rights Reserved.

#include "Interaction/TSAVPlayerController.h"

#include "TSAVPrevisRuntime.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Interaction/TSAVEditPawn.h"
#include "Interaction/TSAVCommandSubsystem.h"
#include "Interaction/TSAVModeSubsystem.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "Interaction/TSAVSelectionSubsystem.h"
#include "Interaction/TSAVTransformGizmoActor.h"
#include "Project/TSAVProjectSubsystem.h"
#include "UI/TSAVMainWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVPlayerController)

ATSAVPlayerController::ATSAVPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void ATSAVPlayerController::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTSAVPrevisRuntime, Display, TEXT("Runtime player controller started (local=%s, local player=%s)."),
		IsLocalController() ? TEXT("true") : TEXT("false"),
		GetLocalPlayer() ? TEXT("valid") : TEXT("null"));

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	if (IsLocalController())
	{
		MainWidget = CreateWidget<UTSAVMainWidget>(this, UTSAVMainWidget::StaticClass());
		if (MainWidget)
		{
			MainWidget->AddToViewport(100);
			UE_LOG(LogTSAVPrevisRuntime, Display, TEXT("Main application widget created (in viewport=%s)."),
				MainWidget->IsInViewport() ? TEXT("true") : TEXT("false"));
		}
		else
		{
			UE_LOG(LogTSAVPrevisRuntime, Error, TEXT("Could not create the main application widget."));
		}
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UTSAVModeSubsystem* ModeSubsystem = GameInstance->GetSubsystem<UTSAVModeSubsystem>())
		{
			ModeSubsystem->OnModeChanged.AddUniqueDynamic(this, &ATSAVPlayerController::HandleModeChanged);
		}
	}

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UTSAVSelectionSubsystem* SelectionSubsystem = LocalPlayer->GetSubsystem<UTSAVSelectionSubsystem>())
		{
			SelectionSubsystem->OnSelectionChanged.AddUniqueDynamic(this, &ATSAVPlayerController::HandleSelectionChanged);
		}
	}

	TransformGizmo = GetWorld()->SpawnActor<ATSAVTransformGizmoActor>();
	if (TransformGizmo)
	{
		TransformGizmo->SetTargetActor(nullptr);
	}

	ApplyMappingContexts();
}

void ATSAVPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MainWidget)
	{
		MainWidget->RemoveFromParent();
		MainWidget = nullptr;
	}

	EndGizmoDrag(true);
	if (TransformGizmo)
	{
		TransformGizmo->Destroy();
		TransformGizmo = nullptr;
	}

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UTSAVSelectionSubsystem* SelectionSubsystem = LocalPlayer->GetSubsystem<UTSAVSelectionSubsystem>())
		{
			SelectionSubsystem->OnSelectionChanged.RemoveDynamic(this, &ATSAVPlayerController::HandleSelectionChanged);
		}
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UTSAVModeSubsystem* ModeSubsystem = GameInstance->GetSubsystem<UTSAVModeSubsystem>())
		{
			ModeSubsystem->OnModeChanged.RemoveDynamic(this, &ATSAVPlayerController::HandleModeChanged);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ATSAVPlayerController::PlayerTick(const float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!bDraggingGizmo && TransformGizmo && GetLocalPlayer())
	{
		AActor* SelectedActor = GetLocalPlayer()->GetSubsystem<UTSAVSelectionSubsystem>()->GetPrimarySelection();
		const UTSAVSceneObjectComponent* SceneObject = SelectedActor ? SelectedActor->FindComponentByClass<UTSAVSceneObjectComponent>() : nullptr;
		AActor* EditableActor = SceneObject && !SceneObject->bLocked && SceneObject->bVisible ? SelectedActor : nullptr;
		if (TransformGizmo->GetTargetActor() != EditableActor)
		{
			TransformGizmo->SetTargetActor(EditableActor);
		}
	}
	if (bDraggingGizmo)
	{
		UpdateGizmoDrag();
	}
}

void ATSAVPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	CreateRuntimeInputAssets();

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInputComponent->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &ATSAVPlayerController::MoveForward);
		EnhancedInputComponent->BindAction(MoveRightAction, ETriggerEvent::Triggered, this, &ATSAVPlayerController::MoveRight);
		EnhancedInputComponent->BindAction(MoveUpAction, ETriggerEvent::Triggered, this, &ATSAVPlayerController::MoveUp);
		EnhancedInputComponent->BindAction(LookYawAction, ETriggerEvent::Triggered, this, &ATSAVPlayerController::LookYaw);
		EnhancedInputComponent->BindAction(LookPitchAction, ETriggerEvent::Triggered, this, &ATSAVPlayerController::LookPitch);
		EnhancedInputComponent->BindAction(AdjustSpeedAction, ETriggerEvent::Triggered, this, &ATSAVPlayerController::AdjustMoveSpeed);
		EnhancedInputComponent->BindAction(SelectAction, ETriggerEvent::Started, this, &ATSAVPlayerController::SelectAtCursor);
		EnhancedInputComponent->BindAction(SelectAction, ETriggerEvent::Completed, this, &ATSAVPlayerController::EndPrimaryInteraction);
		EnhancedInputComponent->BindAction(ReturnToEditAction, ETriggerEvent::Started, this, &ATSAVPlayerController::ReturnToEditMode);
		EnhancedInputComponent->BindAction(TranslateModeAction, ETriggerEvent::Started, this, &ATSAVPlayerController::SetTranslateMode);
		EnhancedInputComponent->BindAction(RotateModeAction, ETriggerEvent::Started, this, &ATSAVPlayerController::SetRotateMode);
		EnhancedInputComponent->BindAction(ScaleModeAction, ETriggerEvent::Started, this, &ATSAVPlayerController::SetScaleMode);
		EnhancedInputComponent->BindAction(ToggleCoordinateSpaceAction, ETriggerEvent::Started, this, &ATSAVPlayerController::ToggleCoordinateSpace);
		EnhancedInputComponent->BindAction(DeleteAction, ETriggerEvent::Started, this, &ATSAVPlayerController::DeleteSelection);
		EnhancedInputComponent->BindAction(DuplicateAction, ETriggerEvent::Started, this, &ATSAVPlayerController::DuplicateSelection);
		EnhancedInputComponent->BindAction(UndoAction, ETriggerEvent::Started, this, &ATSAVPlayerController::UndoCommand);
		EnhancedInputComponent->BindAction(RedoAction, ETriggerEvent::Started, this, &ATSAVPlayerController::RedoCommand);
		EnhancedInputComponent->BindAction(SaveAction, ETriggerEvent::Started, this, &ATSAVPlayerController::SaveProject);
		EnhancedInputComponent->BindAction(LoadAction, ETriggerEvent::Started, this, &ATSAVPlayerController::LoadProject);
		EnhancedInputComponent->BindAction(SpawnCubeAction, ETriggerEvent::Started, this, &ATSAVPlayerController::SpawnCube);
	}
}

void ATSAVPlayerController::CreateRuntimeInputAssets()
{
	if (CommonMappingContext)
	{
		return;
	}

	auto CreateAction = [this](const FName Name, const EInputActionValueType ValueType)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = ValueType;
		return Action;
	};

	CommonMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_TSAV_Common"));
	EditMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_TSAV_Edit"));

	MoveForwardAction = CreateAction(TEXT("IA_TSAV_MoveForward"), EInputActionValueType::Axis1D);
	MoveRightAction = CreateAction(TEXT("IA_TSAV_MoveRight"), EInputActionValueType::Axis1D);
	MoveUpAction = CreateAction(TEXT("IA_TSAV_MoveUp"), EInputActionValueType::Axis1D);
	LookYawAction = CreateAction(TEXT("IA_TSAV_LookYaw"), EInputActionValueType::Axis1D);
	LookPitchAction = CreateAction(TEXT("IA_TSAV_LookPitch"), EInputActionValueType::Axis1D);
	AdjustSpeedAction = CreateAction(TEXT("IA_TSAV_AdjustSpeed"), EInputActionValueType::Axis1D);
	SelectAction = CreateAction(TEXT("IA_TSAV_Select"), EInputActionValueType::Boolean);
	ReturnToEditAction = CreateAction(TEXT("IA_TSAV_ReturnToEdit"), EInputActionValueType::Boolean);
	TranslateModeAction = CreateAction(TEXT("IA_TSAV_TranslateMode"), EInputActionValueType::Boolean);
	RotateModeAction = CreateAction(TEXT("IA_TSAV_RotateMode"), EInputActionValueType::Boolean);
	ScaleModeAction = CreateAction(TEXT("IA_TSAV_ScaleMode"), EInputActionValueType::Boolean);
	ToggleCoordinateSpaceAction = CreateAction(TEXT("IA_TSAV_ToggleCoordinateSpace"), EInputActionValueType::Boolean);
	DeleteAction = CreateAction(TEXT("IA_TSAV_Delete"), EInputActionValueType::Boolean);
	DuplicateAction = CreateAction(TEXT("IA_TSAV_Duplicate"), EInputActionValueType::Boolean);
	UndoAction = CreateAction(TEXT("IA_TSAV_Undo"), EInputActionValueType::Boolean);
	RedoAction = CreateAction(TEXT("IA_TSAV_Redo"), EInputActionValueType::Boolean);
	SaveAction = CreateAction(TEXT("IA_TSAV_Save"), EInputActionValueType::Boolean);
	LoadAction = CreateAction(TEXT("IA_TSAV_Load"), EInputActionValueType::Boolean);
	SpawnCubeAction = CreateAction(TEXT("IA_TSAV_SpawnCube"), EInputActionValueType::Boolean);

	EditMappingContext->MapKey(MoveForwardAction, EKeys::W);
	FEnhancedActionKeyMapping& MoveBackward = EditMappingContext->MapKey(MoveForwardAction, EKeys::S);
	MoveBackward.Modifiers.Add(NewObject<UInputModifierNegate>(this));

	EditMappingContext->MapKey(MoveRightAction, EKeys::D);
	FEnhancedActionKeyMapping& MoveLeft = EditMappingContext->MapKey(MoveRightAction, EKeys::A);
	MoveLeft.Modifiers.Add(NewObject<UInputModifierNegate>(this));

	EditMappingContext->MapKey(MoveUpAction, EKeys::E);
	FEnhancedActionKeyMapping& MoveDown = EditMappingContext->MapKey(MoveUpAction, EKeys::Q);
	MoveDown.Modifiers.Add(NewObject<UInputModifierNegate>(this));

	EditMappingContext->MapKey(LookYawAction, EKeys::MouseX);
	FEnhancedActionKeyMapping& PitchMapping = EditMappingContext->MapKey(LookPitchAction, EKeys::MouseY);
	PitchMapping.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	EditMappingContext->MapKey(AdjustSpeedAction, EKeys::MouseWheelAxis);
	EditMappingContext->MapKey(SelectAction, EKeys::LeftMouseButton);
	EditMappingContext->MapKey(TranslateModeAction, EKeys::W);
	EditMappingContext->MapKey(RotateModeAction, EKeys::E);
	EditMappingContext->MapKey(ScaleModeAction, EKeys::R);
	EditMappingContext->MapKey(ToggleCoordinateSpaceAction, EKeys::X);
	EditMappingContext->MapKey(DeleteAction, EKeys::Delete);
	EditMappingContext->MapKey(DuplicateAction, EKeys::D);
	EditMappingContext->MapKey(UndoAction, EKeys::Z);
	EditMappingContext->MapKey(RedoAction, EKeys::Y);
	EditMappingContext->MapKey(SaveAction, EKeys::S);
	EditMappingContext->MapKey(LoadAction, EKeys::O);
	EditMappingContext->MapKey(SpawnCubeAction, EKeys::Insert);

	CommonMappingContext->MapKey(ReturnToEditAction, EKeys::Escape);
	CommonMappingContext->MapKey(ReturnToEditAction, EKeys::F1);
}

void ATSAVPlayerController::ApplyMappingContexts()
{
	CreateRuntimeInputAssets();
	if (!GetLocalPlayer())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		InputSubsystem->RemoveMappingContext(CommonMappingContext);
		InputSubsystem->RemoveMappingContext(EditMappingContext);
		InputSubsystem->AddMappingContext(CommonMappingContext, 0);

		const UTSAVModeSubsystem* ModeSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTSAVModeSubsystem>() : nullptr;
		if (!ModeSubsystem || ModeSubsystem->GetMode() != ETSAVAppMode::Walkthrough)
		{
			InputSubsystem->AddMappingContext(EditMappingContext, 10);
		}
	}
}

void ATSAVPlayerController::MoveForward(const FInputActionValue& Value)
{
	if (IsInputKeyDown(EKeys::RightMouseButton))
	{
		if (ATSAVEditPawn* EditPawn = Cast<ATSAVEditPawn>(GetPawn()))
		{
			EditPawn->AddFlyMovement(GetControlRotation().Vector(), Value.Get<float>());
		}
	}
}

void ATSAVPlayerController::MoveRight(const FInputActionValue& Value)
{
	if (IsInputKeyDown(EKeys::RightMouseButton))
	{
		if (ATSAVEditPawn* EditPawn = Cast<ATSAVEditPawn>(GetPawn()))
		{
			const FVector Direction = FRotationMatrix(GetControlRotation()).GetUnitAxis(EAxis::Y);
			EditPawn->AddFlyMovement(Direction, Value.Get<float>());
		}
	}
}

void ATSAVPlayerController::MoveUp(const FInputActionValue& Value)
{
	if (IsInputKeyDown(EKeys::RightMouseButton))
	{
		if (ATSAVEditPawn* EditPawn = Cast<ATSAVEditPawn>(GetPawn()))
		{
			EditPawn->AddFlyMovement(FVector::UpVector, Value.Get<float>());
		}
	}
}

void ATSAVPlayerController::LookYaw(const FInputActionValue& Value)
{
	if (IsInputKeyDown(EKeys::RightMouseButton))
	{
		AddYawInput(Value.Get<float>());
	}
}

void ATSAVPlayerController::LookPitch(const FInputActionValue& Value)
{
	if (IsInputKeyDown(EKeys::RightMouseButton))
	{
		AddPitchInput(Value.Get<float>());
	}
}

void ATSAVPlayerController::AdjustMoveSpeed(const FInputActionValue& Value)
{
	if (ATSAVEditPawn* EditPawn = Cast<ATSAVEditPawn>(GetPawn()))
	{
		EditPawn->SetMoveSpeed(EditPawn->GetMoveSpeed() + Value.Get<float>() * 400.0f);
	}
}

void ATSAVPlayerController::SelectAtCursor(const FInputActionValue& Value)
{
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!GetMousePosition(MouseX, MouseY) || !GetLocalPlayer())
	{
		return;
	}
	if (BeginGizmoDrag(FVector2D(MouseX, MouseY)))
	{
		return;
	}

	if (UTSAVSelectionSubsystem* SelectionSubsystem = GetLocalPlayer()->GetSubsystem<UTSAVSelectionSubsystem>())
	{
		SelectionSubsystem->SelectFromScreenPosition(this, FVector2D(MouseX, MouseY), 1000000.0f, IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift));
	}
}

void ATSAVPlayerController::EndPrimaryInteraction(const FInputActionValue& Value)
{
	EndGizmoDrag(false);
}

void ATSAVPlayerController::ReturnToEditMode(const FInputActionValue& Value)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UTSAVModeSubsystem* ModeSubsystem = GameInstance->GetSubsystem<UTSAVModeSubsystem>())
		{
			ModeSubsystem->SetMode(ETSAVAppMode::Select);
		}
	}
}

void ATSAVPlayerController::HandleModeChanged(const ETSAVAppMode NewMode, const ETSAVAppMode PreviousMode)
{
	ApplyMappingContexts();
}

void ATSAVPlayerController::SetTranslateMode(const FInputActionValue& Value)
{
	if (!IsInputKeyDown(EKeys::RightMouseButton))
	{
		SetTransformTool(ETSAVTransformMode::Translate);
	}
}

void ATSAVPlayerController::SetRotateMode(const FInputActionValue& Value)
{
	if (!IsInputKeyDown(EKeys::RightMouseButton))
	{
		SetTransformTool(ETSAVTransformMode::Rotate);
	}
}

void ATSAVPlayerController::SetScaleMode(const FInputActionValue& Value)
{
	if (!IsInputKeyDown(EKeys::RightMouseButton))
	{
		SetTransformTool(ETSAVTransformMode::Scale);
	}
}

void ATSAVPlayerController::ToggleCoordinateSpace(const FInputActionValue& Value)
{
	ToggleTransformCoordinateSpace();
}

void ATSAVPlayerController::SetTransformTool(const ETSAVTransformMode NewMode)
{
	if (TransformGizmo)
	{
		TransformGizmo->SetTransformMode(NewMode);
	}
}

void ATSAVPlayerController::ToggleTransformCoordinateSpace()
{
	if (TransformGizmo)
	{
		TransformGizmo->SetCoordinateSpace(TransformGizmo->GetCoordinateSpace() == ETSAVCoordinateSpace::World
			? ETSAVCoordinateSpace::Local : ETSAVCoordinateSpace::World);
	}
}

void ATSAVPlayerController::FrameSelection()
{
	const UTSAVSelectionSubsystem* Selection = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UTSAVSelectionSubsystem>() : nullptr;
	AActor* SelectedActor = Selection ? Selection->GetPrimarySelection() : nullptr;
	APawn* ViewPawn = GetPawn();
	if (!SelectedActor || !ViewPawn)
	{
		return;
	}

	FVector BoundsOrigin;
	FVector BoundsExtent;
	SelectedActor->GetActorBounds(true, BoundsOrigin, BoundsExtent);
	const FVector ViewDirection = PlayerCameraManager ? PlayerCameraManager->GetCameraRotation().Vector() : GetControlRotation().Vector();
	const float Distance = FMath::Max(BoundsExtent.GetMax() * 3.5f, 300.0f);
	ViewPawn->SetActorLocation(BoundsOrigin - ViewDirection * Distance);
	SetControlRotation((BoundsOrigin - ViewPawn->GetActorLocation()).Rotation());
}

void ATSAVPlayerController::DeleteSelection(const FInputActionValue& Value)
{
	if (!GetLocalPlayer() || !GetGameInstance())
	{
		return;
	}
	UTSAVSelectionSubsystem* Selection = GetLocalPlayer()->GetSubsystem<UTSAVSelectionSubsystem>();
	AActor* Actor = Selection ? Selection->GetPrimarySelection() : nullptr;
	if (Actor)
	{
		Selection->ClearSelection();
		GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>()->DeleteActor(Actor);
	}
}

void ATSAVPlayerController::DuplicateSelection(const FInputActionValue& Value)
{
	if (!IsControlDown() || !GetLocalPlayer() || !GetGameInstance())
	{
		return;
	}
	UTSAVSelectionSubsystem* Selection = GetLocalPlayer()->GetSubsystem<UTSAVSelectionSubsystem>();
	if (AActor* Duplicate = GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>()->DuplicateActor(Selection ? Selection->GetPrimarySelection() : nullptr))
	{
		Selection->SelectActor(Duplicate);
	}
}

void ATSAVPlayerController::UndoCommand(const FInputActionValue& Value)
{
	if (IsControlDown() && GetGameInstance())
	{
		GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>()->Undo();
	}
}

void ATSAVPlayerController::RedoCommand(const FInputActionValue& Value)
{
	if (IsControlDown() && GetGameInstance())
	{
		GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>()->Redo();
	}
}

void ATSAVPlayerController::SaveProject(const FInputActionValue& Value)
{
	if (IsControlDown() && GetGameInstance())
	{
		GetGameInstance()->GetSubsystem<UTSAVProjectSubsystem>()->SaveProject();
	}
}

void ATSAVPlayerController::LoadProject(const FInputActionValue& Value)
{
	if (IsControlDown() && GetGameInstance())
	{
		if (UTSAVSelectionSubsystem* Selection = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UTSAVSelectionSubsystem>() : nullptr)
		{
			Selection->ClearSelection();
		}
		GetGameInstance()->GetSubsystem<UTSAVProjectSubsystem>()->LoadProject();
	}
}

void ATSAVPlayerController::SpawnCube(const FInputActionValue& Value)
{
	if (!GetGameInstance() || !GetWorld())
	{
		return;
	}
	const FVector CubeSpawnLocation = PlayerCameraManager
		? PlayerCameraManager->GetCameraLocation() + PlayerCameraManager->GetCameraRotation().Vector() * 500.0f
		: FVector::ZeroVector;
	AActor* Actor = GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>()->SpawnSceneObject(
		GetWorld(), FTransform(FRotator::ZeroRotator, CubeSpawnLocation, FVector::OneVector), NSLOCTEXT("TSAVPreVis", "InsertedCube", "Scene Cube"));
	if (Actor)
	{
		if (UTSAVSelectionSubsystem* Selection = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UTSAVSelectionSubsystem>() : nullptr)
		{
			Selection->SelectActor(Actor);
		}
	}
}

bool ATSAVPlayerController::BeginGizmoDrag(const FVector2D& ScreenPosition)
{
	if (!TransformGizmo || !TransformGizmo->GetTargetActor() || !GetWorld())
	{
		return false;
	}
	FVector RayOrigin;
	FVector RayDirection;
	if (!GetCursorRay(RayOrigin, RayDirection))
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TSAVGizmoHit), true);
	QueryParams.AddIgnoredActor(TransformGizmo->GetTargetActor());
	if (!GetWorld()->LineTraceSingleByChannel(Hit, RayOrigin, RayOrigin + RayDirection * 1000000.0f, ECC_Visibility, QueryParams))
	{
		return false;
	}
	if (!TransformGizmo->GetAxisForComponent(Hit.GetComponent(), DragAxisIndex, DragWorldAxis))
	{
		return false;
	}

	DragActor = TransformGizmo->GetTargetActor();
	DragStartTransform = DragActor->GetActorTransform();
	DragAxisOrigin = TransformGizmo->GetActorLocation();
	DragStartScreenPosition = ScreenPosition;
	if (!GetAxisRayParameter(RayOrigin, RayDirection, DragAxisOrigin, DragWorldAxis, DragStartAxisParameter))
	{
		DragStartAxisParameter = 0.0;
	}
	bDraggingGizmo = true;
	GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>()->BeginTransformTransaction(DragActor.Get());
	return true;
}

void ATSAVPlayerController::UpdateGizmoDrag()
{
	AActor* Actor = DragActor.Get();
	if (!Actor || !TransformGizmo || !GetGameInstance())
	{
		EndGizmoDrag(true);
		return;
	}

	FVector RayOrigin;
	FVector RayDirection;
	FVector2D ScreenPosition;
	if (!GetCursorRay(RayOrigin, RayDirection, &ScreenPosition))
	{
		return;
	}

	FTransform NewTransform = DragStartTransform;
	if (TransformGizmo->GetTransformMode() == ETSAVTransformMode::Rotate)
	{
		const FVector2D PixelDelta = ScreenPosition - DragStartScreenPosition;
		float AngleDegrees = (PixelDelta.X - PixelDelta.Y) * 0.25f;
		if (RotationGridDegrees > 0.0f)
		{
			AngleDegrees = FMath::GridSnap(AngleDegrees, RotationGridDegrees);
		}
		const FQuat DeltaRotation(DragWorldAxis, FMath::DegreesToRadians(AngleDegrees));
		NewTransform.SetRotation(DeltaRotation * DragStartTransform.GetRotation());
	}
	else
	{
		double AxisParameter = DragStartAxisParameter;
		if (!GetAxisRayParameter(RayOrigin, RayDirection, DragAxisOrigin, DragWorldAxis, AxisParameter))
		{
			return;
		}
		const float AxisDelta = static_cast<float>(AxisParameter - DragStartAxisParameter);
		if (TransformGizmo->GetTransformMode() == ETSAVTransformMode::Translate)
		{
			const float SnappedDelta = TranslationGridSize > 0.0f ? FMath::GridSnap(AxisDelta, TranslationGridSize) : AxisDelta;
			NewTransform.SetLocation(DragStartTransform.GetLocation() + DragWorldAxis * SnappedDelta);
		}
		else
		{
			FVector Scale = DragStartTransform.GetScale3D();
			float AxisScale = Scale[DragAxisIndex] + AxisDelta / 100.0f;
			AxisScale = ScaleGridSize > 0.0f ? FMath::GridSnap(AxisScale, ScaleGridSize) : AxisScale;
			Scale[DragAxisIndex] = FMath::Max(AxisScale, 0.05f);
			NewTransform.SetScale3D(Scale);
		}
	}

	GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>()->UpdateTransformTransaction(Actor, NewTransform);
}

void ATSAVPlayerController::EndGizmoDrag(const bool bCancel)
{
	if (!bDraggingGizmo || !GetGameInstance())
	{
		return;
	}
	UTSAVCommandSubsystem* Commands = GetGameInstance()->GetSubsystem<UTSAVCommandSubsystem>();
	if (bCancel)
	{
		Commands->CancelTransformTransaction();
	}
	else
	{
		Commands->EndTransformTransaction(DragActor.Get(), NSLOCTEXT("TSAVPreVis", "GizmoTransformCommand", "Transform Object"));
	}
	bDraggingGizmo = false;
	DragActor = nullptr;
}

bool ATSAVPlayerController::GetCursorRay(FVector& OutOrigin, FVector& OutDirection, FVector2D* OutScreenPosition) const
{
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!GetMousePosition(MouseX, MouseY) || !DeprojectScreenPositionToWorld(MouseX, MouseY, OutOrigin, OutDirection))
	{
		return false;
	}
	if (OutScreenPosition)
	{
		*OutScreenPosition = FVector2D(MouseX, MouseY);
	}
	return true;
}

bool ATSAVPlayerController::GetAxisRayParameter(
	const FVector& RayOrigin,
	const FVector& RayDirection,
	const FVector& AxisOrigin,
	const FVector& AxisDirection,
	double& OutAxisParameter)
{
	const FVector Offset = RayOrigin - AxisOrigin;
	const double RayAxisDot = FVector::DotProduct(RayDirection, AxisDirection);
	const double Denominator = 1.0 - RayAxisDot * RayAxisDot;
	if (FMath::Abs(Denominator) < 0.0001)
	{
		return false;
	}
	OutAxisParameter = (FVector::DotProduct(Offset, AxisDirection) - FVector::DotProduct(Offset, RayDirection) * RayAxisDot) / Denominator;
	return true;
}

bool ATSAVPlayerController::IsControlDown() const
{
	return IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
}

void ATSAVPlayerController::HandleSelectionChanged(AActor* SelectedActor)
{
	EndGizmoDrag(true);
	if (TransformGizmo)
	{
		TransformGizmo->SetTargetActor(SelectedActor);
	}
}
