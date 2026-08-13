// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DMXTypes.h"
#include "GameFramework/Actor.h"

#include "TSAVDMXFixture.generated.h"

class UDMXComponent;
class UDMXEntityFixturePatch;
class UDMXImportGDTF;
class USceneComponent;
class USpotLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Reusable, articulated DMX fixture assembled by the TSAV DMX Fixture Builder.
 *
 * The component hierarchy is Base -> Pan/Yoke -> Tilt/Head -> Lens/Beam. A
 * generated DMX fixture patch supplies normalized GDTF attributes to the actor.
 */
UCLASS(Blueprintable, meta = (DisplayName = "TSAV GDTF DMX Fixture"))
class TSAVLEDTOOLS_API ATSAVDMXFixture final : public AActor
{
	GENERATED_BODY()

public:
	ATSAVDMXFixture();

	/** Original GDTF used to create the DMX fixture type and patch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Definition")
	TObjectPtr<UDMXImportGDTF> GDTFSource;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Definition")
	FString GDTFModeName;

	/** Optional separate meshes. A single full-fixture model can be assigned as Head Mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Model")
	TObjectPtr<UStaticMesh> BaseMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Model")
	TObjectPtr<UStaticMesh> YokeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Model")
	TObjectPtr<UStaticMesh> HeadMesh;

	/** Optional lens/beam geometry from the GDTF model. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Model")
	TObjectPtr<UStaticMesh> LensMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Model", meta = (ClampMin = "0.001", UIMin = "0.01", UIMax = "10.0"))
	float FixtureScale = 1.0f;

	/** Corrects the imported model's forward/up axes before pan and tilt are applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Model")
	FRotator ModelRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Model")
	FVector BaseMeshOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Model")
	FVector YokeMeshOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Model")
	FVector HeadMeshOffset = FVector::ZeroVector;

	/** Position of the pan/yoke pivot relative to the fixture base, in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Motion")
	FVector PanPivotOffset = FVector::ZeroVector;

	/** Position of the tilt/head pivot relative to the pan pivot, in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Motion")
	FVector TiltPivotOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Motion")
	float PanMinDegrees = -270.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Motion")
	float PanMaxDegrees = 270.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Motion")
	float TiltMinDegrees = -135.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Motion")
	float TiltMaxDegrees = 135.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Motion")
	float PanOffsetDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Motion")
	float TiltOffsetDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Motion")
	bool bInvertPan = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Motion")
	bool bInvertTilt = false;

	/** Maximum pan travel per second. Set to zero to snap directly to the DMX value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Motion", meta = (ClampMin = "0.0"))
	float PanSpeedDegreesPerSecond = 360.0f;

	/** Maximum tilt travel per second. Set to zero to snap directly to the DMX value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Motion", meta = (ClampMin = "0.0"))
	float TiltSpeedDegreesPerSecond = 360.0f;

	/** Lens/beam origin relative to the head pivot, in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Beam")
	FVector LensOffset = FVector(20.0f, 0.0f, 0.0f);

	/** Orientation of the physical lens model from the GDTF beam geometry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Beam")
	FRotator LensMeshRotation = FRotator::ZeroRotator;

	/** Corrects beam direction. Unreal spot lights point along local +X. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Beam")
	FRotator BeamRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Beam", meta = (ClampMin = "0.0"))
	float MaximumIntensityLumens = 50000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Beam", meta = (ClampMin = "1.0", ClampMax = "89.0"))
	float MinimumBeamAngleDegrees = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Beam", meta = (ClampMin = "1.0", ClampMax = "89.0"))
	float MaximumBeamAngleDegrees = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Beam", meta = (ClampMin = "1.0"))
	float AttenuationRadiusCm = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Beam")
	FLinearColor DefaultLightColor = FLinearColor::White;

	/** Attribute overrides. Common GDTF aliases are detected automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|DMX Attributes")
	FName PanAttribute = TEXT("Pan");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|DMX Attributes")
	FName TiltAttribute = TEXT("Tilt");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|DMX Attributes")
	FName DimmerAttribute = TEXT("Dimmer");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|DMX Attributes")
	FName RedAttribute = TEXT("ColorAdd_R");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|DMX Attributes")
	FName GreenAttribute = TEXT("ColorAdd_G");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|DMX Attributes")
	FName BlueAttribute = TEXT("ColorAdd_B");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|DMX Attributes")
	FName ZoomAttribute = TEXT("Zoom");

	/** Convenient editor test values. Incoming DMX takes control as soon as it arrives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Preview", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PreviewPan = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Preview", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PreviewTilt = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Preview", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PreviewDimmer = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Fixture|Preview")
	FLinearColor PreviewColor = FLinearColor::White;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "TSAV Fixture|Preview")
	void ApplyPreviewValues();

	UFUNCTION(BlueprintCallable, Category = "TSAV Fixture|DMX")
	void SetFixturePatch(UDMXEntityFixturePatch* FixturePatch);

	UFUNCTION(BlueprintPure, Category = "TSAV Fixture|DMX")
	UDMXEntityFixturePatch* GetFixturePatch() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TSAV Fixture|Status")
	float CurrentPanDegrees = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TSAV Fixture|Status")
	float CurrentTiltDegrees = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TSAV Fixture|Status")
	float LastDimmerValue = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TSAV Fixture|Components")
	TObjectPtr<UDMXComponent> DMXComponent;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	UFUNCTION()
	void OnFixturePatchReceived(UDMXEntityFixturePatch* FixturePatch, const FDMXNormalizedAttributeValueMap& ValuePerAttribute);

	void ApplyModelSetup();
	void ApplyMotionAndBeam(float DeltaSeconds, bool bSnap);
	void SetTargetsFromNormalized(float Pan, float Tilt, float Dimmer, const FLinearColor& Color, float Zoom);
	static bool FindAttributeValue(const FDMXNormalizedAttributeValueMap& Values, FName PreferredName, const TArray<FString>& Aliases, float& OutValue);
	static FString CanonicalizeAttribute(FName AttributeName);

	UPROPERTY(VisibleAnywhere, Category = "TSAV Fixture|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "TSAV Fixture|Components")
	TObjectPtr<USceneComponent> ModelRoot;

	UPROPERTY(VisibleAnywhere, Category = "TSAV Fixture|Components")
	TObjectPtr<UStaticMeshComponent> BaseVisual;

	UPROPERTY(VisibleAnywhere, Category = "TSAV Fixture|Components")
	TObjectPtr<USceneComponent> PanPivot;

	UPROPERTY(VisibleAnywhere, Category = "TSAV Fixture|Components")
	TObjectPtr<UStaticMeshComponent> YokeVisual;

	UPROPERTY(VisibleAnywhere, Category = "TSAV Fixture|Components")
	TObjectPtr<USceneComponent> TiltPivot;

	UPROPERTY(VisibleAnywhere, Category = "TSAV Fixture|Components")
	TObjectPtr<UStaticMeshComponent> HeadVisual;

	UPROPERTY(VisibleAnywhere, Category = "TSAV Fixture|Components")
	TObjectPtr<USceneComponent> LensRoot;

	UPROPERTY(VisibleAnywhere, Category = "TSAV Fixture|Components")
	TObjectPtr<UStaticMeshComponent> LensVisual;

	UPROPERTY(VisibleAnywhere, Category = "TSAV Fixture|Components")
	TObjectPtr<USpotLightComponent> BeamLight;

	float TargetPanDegrees = 0.0f;
	float TargetTiltDegrees = 0.0f;
	float TargetDimmer = 0.0f;
	float TargetZoom = 0.0f;
	FLinearColor TargetColor = FLinearColor::White;
};
