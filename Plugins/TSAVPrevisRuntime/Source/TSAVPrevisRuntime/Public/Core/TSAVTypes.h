// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TSAVTypes.generated.h"

/** Top-level authoring modes that operate over one TSAV project scene. */
UENUM(BlueprintType)
enum class ETSAVAppMode : uint8
{
	Select,
	Venue,
	Stage,
	Truss,
	Lighting,
	LED,
	Camera,
	Video,
	Characters,
	Walkthrough,
};

/** Stable semantic category used by selection, persistence, and the outliner. */
UENUM(BlueprintType)
enum class ETSAVObjectType : uint8
{
	Unknown,
	Venue,
	Stage,
	Truss,
	Fixture,
	LED,
	Camera,
	Video,
	Character,
	Scenic,
};
