// Copyright TSAV. All Rights Reserved.

#pragma once

#include "Components/Button.h"
#include "CoreMinimal.h"

#include "TSAVMenuButton.generated.h"

/** Commands exposed by the desktop application's top menus. */
UENUM()
enum class ETSAVMenuAction : uint8
{
	NewProject,
	SaveProject,
	LoadProject,
	Undo,
	Redo,
	DuplicateSelection,
	DeleteSelection,
	AddVenueFloor,
	AddStageDeck,
	AddScenicCube,
	AddTrussSegment,
	AddLEDWall,
	AddLEDPanel,
	AddDMXFixture,
	AddPointLight,
	AddSpotLight,
	AddRectLight,
	AddVideoSurface,
	AddVideoSwitcher,
	AddCamera,
	SelectView,
	WalkthroughView,
	TranslateTool,
	RotateTool,
	ScaleTool,
	ToggleCoordinateSpace,
	FrameSelection,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTSAVMenuActionClicked, ETSAVMenuAction, Action);

/** Runtime menu row that retains the command represented by its label. */
UCLASS()
class TSAVPREVISRUNTIME_API UTSAVMenuButton final : public UButton
{
	GENERATED_BODY()

public:
	void InitializeForAction(ETSAVMenuAction InAction);

	UPROPERTY(BlueprintAssignable)
	FTSAVMenuActionClicked OnActionClicked;

private:
	UFUNCTION()
	void HandleClicked();

	ETSAVMenuAction Action = ETSAVMenuAction::NewProject;
};
