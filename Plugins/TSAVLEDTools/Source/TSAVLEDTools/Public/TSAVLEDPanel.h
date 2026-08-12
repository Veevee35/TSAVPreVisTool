// Copyright TSAV. All Rights Reserved.

#pragma once

#include "TSAVMediaSurfaceActor.h"

#include "TSAVLEDPanel.generated.h"

class UStaticMeshComponent;
class UTSAVLEDPanelDefinition;

/** A single, dimensionally editable LED cabinet with an assignable Media/NDI source. */
UCLASS(Blueprintable, meta = (DisplayName = "TSAV LED Panel"))
class TSAVLEDTOOLS_API ATSAVLEDPanel : public ATSAVMediaSurfaceActor
{
	GENERATED_BODY()

public:
	ATSAVLEDPanel();

	/** Reusable cabinet definition. Enable Use Panel Definition to drive the properties below from this asset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Panel")
	TObjectPtr<UTSAVLEDPanelDefinition> PanelDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Panel")
	bool bUsePanelDefinition = false;

	/** Overall cabinet width in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Panel", meta = (ClampMin = "10.0", UIMin = "25.0", UIMax = "200.0", EditCondition = "!bUsePanelDefinition"))
	float WidthCm = 50.0f;

	/** Overall cabinet height in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Panel", meta = (ClampMin = "10.0", UIMin = "25.0", UIMax = "200.0", EditCondition = "!bUsePanelDefinition"))
	float HeightCm = 50.0f;

	/** Cabinet depth in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Panel", meta = (ClampMin = "1.0", UIMin = "2.0", UIMax = "30.0", EditCondition = "!bUsePanelDefinition"))
	float DepthCm = 8.0f;

	/** Width of the visible cabinet bezel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Panel", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0", EditCondition = "!bUsePanelDefinition"))
	float BezelCm = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Pixels", meta = (ClampMin = "1", EditCondition = "!bUsePanelDefinition"))
	int32 ResolutionX = 128;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Pixels", meta = (ClampMin = "1", EditCondition = "!bUsePanelDefinition"))
	int32 ResolutionY = 128;

	UFUNCTION(BlueprintPure, Category = "TSAV LED|Pixels")
	FVector2D GetPixelPitchMm() const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	virtual FIntPoint GetNativePixelResolution() const override;
	float GetEffectiveWidthCm() const;
	float GetEffectiveHeightCm() const;
	float GetEffectiveDepthCm() const;
	float GetEffectiveBezelCm() const;
	void UpdateGeometry();
	void SetBox(UStaticMeshComponent* Component, const FVector& SizeCm, const FVector& LocationCm) const;

	UPROPERTY(VisibleAnywhere, Category = "TSAV LED|Components")
	TObjectPtr<UStaticMeshComponent> Backing;

	UPROPERTY(VisibleAnywhere, Category = "TSAV LED|Components")
	TObjectPtr<UStaticMeshComponent> TopBezel;

	UPROPERTY(VisibleAnywhere, Category = "TSAV LED|Components")
	TObjectPtr<UStaticMeshComponent> BottomBezel;

	UPROPERTY(VisibleAnywhere, Category = "TSAV LED|Components")
	TObjectPtr<UStaticMeshComponent> LeftBezel;

	UPROPERTY(VisibleAnywhere, Category = "TSAV LED|Components")
	TObjectPtr<UStaticMeshComponent> RightBezel;
};
