// Copyright TSAV. All Rights Reserved.

#pragma once

#include "TSAVMediaSurfaceActor.h"

#include "TSAVLEDPanel.generated.h"

class UStaticMeshComponent;

/** A single, dimensionally editable LED cabinet with an assignable Media/NDI source. */
UCLASS(Blueprintable, meta = (DisplayName = "TSAV LED Panel"))
class TSAVLEDTOOLS_API ATSAVLEDPanel : public ATSAVMediaSurfaceActor
{
	GENERATED_BODY()

public:
	ATSAVLEDPanel();

	/** Overall cabinet width in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Panel", meta = (ClampMin = "10.0", UIMin = "25.0", UIMax = "200.0"))
	float WidthCm = 50.0f;

	/** Overall cabinet height in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Panel", meta = (ClampMin = "10.0", UIMin = "25.0", UIMax = "200.0"))
	float HeightCm = 50.0f;

	/** Cabinet depth in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Panel", meta = (ClampMin = "1.0", UIMin = "2.0", UIMax = "30.0"))
	float DepthCm = 8.0f;

	/** Width of the visible cabinet bezel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Panel", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0"))
	float BezelCm = 1.0f;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

private:
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
