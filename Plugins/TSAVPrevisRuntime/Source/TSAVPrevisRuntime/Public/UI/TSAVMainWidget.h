// Copyright TSAV. All Rights Reserved.

#pragma once

#include "Core/TSAVTypes.h"
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "UI/TSAVMenuButton.h"

#include "TSAVMainWidget.generated.h"

class AActor;
class ATSAVCameraActor;
class ATSAVLEDWall;
class ATSAVMediaSurfaceActor;
class ATSAVVideoSwitcher;
class UBorder;
class UCanvasPanel;
class UCanvasPanelSlot;
class UComboBoxString;
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
	void BuildContextTools(AActor* SelectedActor);
	void CommitContextState(AActor* Actor, const FString& BeforeState, const FText& Description);
	void RouteContextSurface(FName BusName);
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
	void HandleSwitcherRouteClicked(ATSAVVideoSwitcher* Switcher, FGuid InputId, FName BusName);

	UFUNCTION()
	void SwitcherDiscoverClicked();

	UFUNCTION()
	void SwitcherCutClicked();

	UFUNCTION()
	void SwitcherAutoClicked();

	UFUNCTION()
	void SwitcherAddUrlClicked();

	UFUNCTION()
	void SurfaceRouteProgramClicked();

	UFUNCTION()
	void SurfaceRoutePreviewClicked();

	UFUNCTION()
	void SurfaceRouteAux1Clicked();

	UFUNCTION()
	void SurfaceRouteAux2Clicked();

	UFUNCTION()
	void SurfaceRouteDirectClicked();

	UFUNCTION()
	void LEDWallApplyConfigurationClicked();

	UFUNCTION()
	void LEDWallToggleSeamsClicked();

	UFUNCTION()
	void LEDWallApplyPanelStyleClicked();

	UFUNCTION()
	void CameraTypeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void CameraLensChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void CameraFocalLengthCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void CameraApertureCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void CameraFocusCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void CameraViscaIpCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void CameraViscaPortCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void CameraToggleViscaClicked();

	UFUNCTION()
	void CameraApplyPtzClicked();

	UFUNCTION()
	void CameraViscaHomeClicked();

	UFUNCTION()
	void CameraViscaStopClicked();

	UFUNCTION()
	void CameraViewThroughClicked();

	UFUNCTION()
	void CameraReturnToEditorClicked();

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
	TObjectPtr<UVerticalBox> ContextToolPanel;

	UPROPERTY(Transient)
	TObjectPtr<AActor> ContextActor;

	UPROPERTY(Transient)
	TObjectPtr<ATSAVVideoSwitcher> ContextSwitcher;

	UPROPERTY(Transient)
	TObjectPtr<ATSAVMediaSurfaceActor> ContextMediaSurface;

	UPROPERTY(Transient)
	TObjectPtr<ATSAVLEDWall> ContextLEDWall;

	UPROPERTY(Transient)
	TObjectPtr<ATSAVCameraActor> ContextCamera;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> SwitcherInputNameField;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> SwitcherInputUrlField;

	/** Ordered runtime LED configurator fields; indexes are private to TSAVMainWidget.cpp. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UEditableTextBox>> LEDWallFields;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> LEDPanelColumnField;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> LEDPanelRowField;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> LEDPanelStyleCombo;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> LEDLinkPatternCombo;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> LEDSubpixelCombo;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> CameraTypeCombo;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> CameraLensCombo;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> CameraFocalLengthField;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> CameraApertureField;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> CameraFocusField;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> CameraViscaIpField;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> CameraViscaPortField;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> CameraPanField;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> CameraTiltField;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> CameraZoomField;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> MenuPopup;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> MenuPopupEntries;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> MenuPopupSlot;

	ETSAVTopMenu OpenMenu = ETSAVTopMenu::None;
};
