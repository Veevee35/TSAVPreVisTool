// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "TSAVDMXFixtureCatalog.generated.h"

class UDMXEntityFixturePatch;
class UDMXEntityFixtureType;
class UDMXImportGDTF;
class UDMXLibrary;
class UStaticMesh;

/** A generated, runtime-safe fixture option backed by an imported GDTF profile. */
USTRUCT(BlueprintType)
struct TSAVLEDTOOLS_API FTSAVDMXFixtureDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName DefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FText Manufacturer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FString Revision;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition")
	TSoftObjectPtr<UDMXImportGDTF> GDTFSource;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition")
	FString GDTFModeName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
	TSoftObjectPtr<UDMXLibrary> DMXLibrary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
	FGuid FixtureTypeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
	FGuid FixturePatchId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
	int32 Universe = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
	int32 Address = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
	int32 ChannelSpan = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	TSoftObjectPtr<UStaticMesh> BaseMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	TSoftObjectPtr<UStaticMesh> YokeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	TSoftObjectPtr<UStaticMesh> HeadMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	TSoftObjectPtr<UStaticMesh> LensMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	float FixtureScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	FRotator ModelRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	FVector BaseMeshScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	FVector YokeMeshScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	FVector HeadMeshScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	FVector LensMeshScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	FVector BaseMeshOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	FVector YokeMeshOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	FVector HeadMeshOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion")
	FVector PanPivotOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion")
	FVector TiltPivotOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion")
	FRotator PanPivotRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion")
	FRotator TiltPivotRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion")
	float PanMinDegrees = -270.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion")
	float PanMaxDegrees = 270.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion")
	float TiltMinDegrees = -135.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion")
	float TiltMaxDegrees = 135.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion")
	float PanSpeedDegreesPerSecond = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion")
	float TiltSpeedDegreesPerSecond = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam")
	FVector LensOffset = FVector(20.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam")
	FRotator LensMeshRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam")
	FRotator BeamRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam")
	float MaximumIntensityLumens = 50000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam")
	float MinimumBeamAngleDegrees = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam")
	float MaximumBeamAngleDegrees = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam")
	float AttenuationRadiusCm = 3000.0f;

	/** True when at least one detailed embedded model resource was imported. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Validation")
	bool bUsesEmbeddedModel = false;

	/** True when missing model roles were completed from GDTF primitives or TSAV fallback geometry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Validation")
	bool bUsesPrimitiveFallback = false;

	/** True when the source profile could not be parsed and a safe generic mode/model was generated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Validation")
	bool bUsesInvalidProfileFallback = false;
};

/** Searchable fixture-option catalog generated from the project's complete GDTF collection. */
UCLASS(BlueprintType)
class TSAVLEDTOOLS_API UTSAVDMXFixtureCatalog final : public UDataAsset
{
	GENERATED_BODY()

public:
	static const FSoftObjectPath DefaultCatalogPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TSAV Fixture Catalog")
	TArray<FTSAVDMXFixtureDefinition> Fixtures;

	const FTSAVDMXFixtureDefinition* FindFixture(FName DefinitionId) const;

	UFUNCTION(BlueprintPure, Category = "TSAV Fixture Catalog")
	int32 GetFixtureCount() const { return Fixtures.Num(); }
};
