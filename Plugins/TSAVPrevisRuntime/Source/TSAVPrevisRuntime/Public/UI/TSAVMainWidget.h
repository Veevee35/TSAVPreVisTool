// Copyright TSAV. All Rights Reserved.

#pragma once

#include "Core/TSAVTypes.h"
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "UI/TSAVMenuButton.h"

#include "TSAVMainWidget.generated.h"

class AActor;
class UBorder;
class UCanvasPanel;
class UCanvasPanelSlot;
class UEditableTextBox;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

enum class ETSAVTopMenu : uint8
{
	None,
	File,
	Edit,
	Build,
	LED,
	Lighting,
	Video,
	Camera,
	View,
};

/** Asset-independent runtime application chrome for the first packaged shell. */
UCLASS()
class TSAVPREVISRUNTIME_API UTSAVMainWidget final : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildLayout();
	void SetAppMode(ETSAVAppMode NewMode);
	void UpdateInspector(AActor* SelectedActor);
	void RefreshOutliner();
	void RefreshProjectStatus();
	void CommitTransformValue(int32 GroupIndex, int32 AxisIndex, const FText& Text);
	void ToggleMenu(ETSAVTopMenu Menu, float LeftPosition);
	void HideMenu();
	void AddMenuEntry(const FText& Label, ETSAVMenuAction Action, bool bEnabled = true);
	void ExecuteMenuAction(ETSAVMenuAction Action);
	FTransform MakePlacementTransform(const FVector& Scale = FVector::OneVector, float Distance = 500.0f) const;
	AActor* SpawnAndSelect(TSubclassOf<AActor> ActorClass, const FTransform& Transform, const FText& DisplayName, ETSAVObjectType ObjectType);
	static FText GetModeText(ETSAVAppMode Mode);

	UFUNCTION()
	void HandleSelectionChanged(AActor* SelectedActor);

	UFUNCTION()
	void HandleModeChanged(ETSAVAppMode NewMode, ETSAVAppMode PreviousMode);

	UFUNCTION()
	void HandleCommandObjectChanged(AActor* Actor);

	UFUNCTION()
	void HandleCommandHistoryChanged();

	UFUNCTION()
	void HandleProjectChanged();

	UFUNCTION()
	void HandleOutlinerActorClicked(AActor* Actor);

	UFUNCTION()
	void HandleMenuActionClicked(ETSAVMenuAction Action);

	UFUNCTION()
	void FileMenuClicked();

	UFUNCTION()
	void EditMenuClicked();

	UFUNCTION()
	void BuildMenuClicked();

	UFUNCTION()
	void LEDMenuClicked();

	UFUNCTION()
	void LightingMenuClicked();

	UFUNCTION()
	void VideoMenuClicked();

	UFUNCTION()
	void CameraMenuClicked();

	UFUNCTION()
	void ViewMenuClicked();

	UFUNCTION()
	void NewProjectClicked();

	UFUNCTION()
	void SaveProjectClicked();

	UFUNCTION()
	void LoadProjectClicked();

	UFUNCTION()
	void AddCubeClicked();

	UFUNCTION()
	void UndoClicked();

	UFUNCTION()
	void RedoClicked();

	UFUNCTION()
	void DeleteClicked();

	UFUNCTION()
	void DuplicateClicked();

	UFUNCTION()
	void ToggleLockedClicked();

	UFUNCTION()
	void ToggleVisibleClicked();

	UFUNCTION()
	void NameCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void LocationXCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION()
	void LocationYCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION()
	void LocationZCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION()
	void RotationPitchCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION()
	void RotationYawCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION()
	void RotationRollCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION()
	void ScaleXCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION()
	void ScaleYCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION()
	void ScaleZCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void SelectModeClicked();

	UFUNCTION()
	void VenueModeClicked();

	UFUNCTION()
	void StageModeClicked();

	UFUNCTION()
	void TrussModeClicked();

	UFUNCTION()
	void LightingModeClicked();

	UFUNCTION()
	void LEDModeClicked();

	UFUNCTION()
	void CameraModeClicked();

	UFUNCTION()
	void VideoModeClicked();

	UFUNCTION()
	void CharactersModeClicked();

	UFUNCTION()
	void WalkthroughModeClicked();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OutlinerSelectionText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> OutlinerEntries;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InspectorTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InspectorBodyText;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> NameField;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UEditableTextBox>> LocationFields;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UEditableTextBox>> RotationFields;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UEditableTextBox>> ScaleFields;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ModeStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProjectStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> UndoStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> MenuPopup;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> MenuPopupEntries;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> MenuPopupSlot;

	ETSAVTopMenu OpenMenu = ETSAVTopMenu::None;
};
