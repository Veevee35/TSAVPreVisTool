// Copyright TSAV. All Rights Reserved.

#pragma once

#include "TSAVMediaSurfaceActor.h"

#include "TSAVLEDWall.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMeshComponent;

/**
 * Parametric LED wall builder. The video surface remains continuous across the
 * whole wall while physical panel seams, cabinet backing, and border geometry
 * are generated from the row and column settings.
 */
UCLASS(Blueprintable, meta = (DisplayName = "TSAV LED Wall Builder"))
class TSAVLEDTOOLS_API ATSAVLEDWall : public ATSAVMediaSurfaceActor
{
	GENERATED_BODY()

public:
	ATSAVLEDWall();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Wall", meta = (ClampMin = "1", ClampMax = "64", UIMin = "1", UIMax = "24"))
	int32 Columns = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Wall", meta = (ClampMin = "1", ClampMax = "64", UIMin = "1", UIMax = "16"))
	int32 Rows = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Wall", meta = (ClampMin = "10.0", UIMin = "25.0", UIMax = "200.0"))
	float PanelWidthCm = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Wall", meta = (ClampMin = "10.0", UIMin = "25.0", UIMax = "200.0"))
	float PanelHeightCm = 50.0f;

	/** Visible space/seam between adjacent cabinets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Wall", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float PanelGapCm = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Wall", meta = (ClampMin = "1.0", UIMin = "2.0", UIMax = "40.0"))
	float WallDepthCm = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Wall", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "15.0"))
	float BorderCm = 2.0f;

	/** Show the generated cabinet seams on top of the continuous video surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Wall")
	bool bShowPanelSeams = true;

	UFUNCTION(BlueprintPure, Category = "TSAV LED|Wall")
	float GetWallWidthCm() const;

	UFUNCTION(BlueprintPure, Category = "TSAV LED|Wall")
	float GetWallHeightCm() const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void UpdateGeometry();
	void SetBox(UStaticMeshComponent* Component, const FVector& SizeCm, const FVector& LocationCm) const;

	UPROPERTY(VisibleAnywhere, Category = "TSAV LED|Components")
	TObjectPtr<UStaticMeshComponent> Backing;

	UPROPERTY(VisibleAnywhere, Category = "TSAV LED|Components")
	TObjectPtr<UStaticMeshComponent> TopBorder;

	UPROPERTY(VisibleAnywhere, Category = "TSAV LED|Components")
	TObjectPtr<UStaticMeshComponent> BottomBorder;

	UPROPERTY(VisibleAnywhere, Category = "TSAV LED|Components")
	TObjectPtr<UStaticMeshComponent> LeftBorder;

	UPROPERTY(VisibleAnywhere, Category = "TSAV LED|Components")
	TObjectPtr<UStaticMeshComponent> RightBorder;

	UPROPERTY(VisibleAnywhere, Category = "TSAV LED|Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PanelSeams;
};
