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
#include "Interaction/TSAVModeSubsystem.h"
#include "Interaction/TSAVSelectionSubsystem.h"
#include "UI/TSAVMainWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVPlayerController)

ATSAVPlayerController::ATSAVPlayerController()
{
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

	ApplyMappingContexts();
}

void ATSAVPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MainWidget)
	{
		MainWidget->RemoveFromParent();
		MainWidget = nullptr;
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
		EnhancedInputComponent->BindAction(ReturnToEditAction, ETriggerEvent::Started, this, &ATSAVPlayerController::ReturnToEditMode);
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
	if (ATSAVEditPawn* EditPawn = Cast<ATSAVEditPawn>(GetPawn()))
	{
		EditPawn->AddFlyMovement(GetControlRotation().Vector(), Value.Get<float>());
	}
}

void ATSAVPlayerController::MoveRight(const FInputActionValue& Value)
{
	if (ATSAVEditPawn* EditPawn = Cast<ATSAVEditPawn>(GetPawn()))
	{
		const FVector Direction = FRotationMatrix(GetControlRotation()).GetUnitAxis(EAxis::Y);
		EditPawn->AddFlyMovement(Direction, Value.Get<float>());
	}
}

void ATSAVPlayerController::MoveUp(const FInputActionValue& Value)
{
	if (ATSAVEditPawn* EditPawn = Cast<ATSAVEditPawn>(GetPawn()))
	{
		EditPawn->AddFlyMovement(FVector::UpVector, Value.Get<float>());
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

	if (UTSAVSelectionSubsystem* SelectionSubsystem = GetLocalPlayer()->GetSubsystem<UTSAVSelectionSubsystem>())
	{
		SelectionSubsystem->SelectFromScreenPosition(this, FVector2D(MouseX, MouseY));
	}
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
