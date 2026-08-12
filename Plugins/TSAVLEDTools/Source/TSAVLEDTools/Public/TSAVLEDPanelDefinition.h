// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "TSAVLEDPanelDefinition.generated.h"

/**
 * Reusable cabinet specification shared by individual panels and wall layouts.
 * Duplicate a definition asset to create a library of the LED products used by
 * the shop, then assign the definition to a panel or wall configurator.
 */
UCLASS(BlueprintType, meta = (DisplayName = "TSAV LED Panel Definition"))
class TSAVLEDTOOLS_API UTSAVLEDPanelDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Friendly manufacturer/model text shown to operators. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	FString ModelName = TEXT("Custom 500 mm Cabinet");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical", meta = (ClampMin = "1.0", Units = "cm"))
	float WidthCm = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical", meta = (ClampMin = "1.0", Units = "cm"))
	float HeightCm = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical", meta = (ClampMin = "0.1", Units = "cm"))
	float DepthCm = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical", meta = (ClampMin = "0.0", Units = "cm"))
	float BezelCm = 0.5f;

	/** Native cabinet pixel width. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pixels", meta = (ClampMin = "1"))
	int32 ResolutionX = 128;

	/** Native cabinet pixel height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pixels", meta = (ClampMin = "1"))
	int32 ResolutionY = 128;

	UFUNCTION(BlueprintPure, Category = "Pixels")
	FIntPoint GetResolution() const;

	UFUNCTION(BlueprintPure, Category = "Pixels")
	FVector2D GetPixelPitchMm() const;
};
