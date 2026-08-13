// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Library/DMXEntityFixtureType.h"
#include "Widgets/SCompoundWidget.h"

class ATSAVDMXFixture;
class SComboBoxBase;
template <typename OptionType> class SComboBox;
class UDMXEntityFixturePatch;
class UDMXImportGDTF;
class UStaticMesh;
struct FAssetData;

/** Guided GDTF, model, articulation, beam, and patch creation tool. */
class STSAVDMXFixtureBuilder final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STSAVDMXFixtureBuilder) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply ImportGDTF();
	FReply ImportModel();
	FReply CreateFixture();
	FReply UpdateFixture();
	FReply LoadSelectedFixture();
	FReply PreviewSelectedFixture();

	void OnGDTFChanged(const FAssetData& AssetData);
	void OnBaseMeshChanged(const FAssetData& AssetData);
	void OnYokeMeshChanged(const FAssetData& AssetData);
	void OnHeadMeshChanged(const FAssetData& AssetData);
	void OnLensMeshChanged(const FAssetData& AssetData);
	int32 ImportEmbeddedGDTFModels(FText& OutResultMessage);
	void RefreshGDTFModes(bool bAdoptPhysicalMotion);
	void AdoptSelectedModePhysicalProperties();
	void OnModeSelected(TSharedPtr<FString> NewMode, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> GenerateModeWidget(TSharedPtr<FString> Mode) const;

	TArray<FDMXFixtureMode> BuildFixtureModes() const;
	UDMXEntityFixturePatch* CreateDMXLibraryAndPatch(const FString& CleanFixtureName, FString& OutLibraryPath);
	void UpdateExistingPatch(UDMXEntityFixturePatch& Patch);
	void ApplySettings(ATSAVDMXFixture& Fixture, UDMXEntityFixturePatch* Patch) const;
	ATSAVDMXFixture* FindSelectedFixture() const;
	int32 GetSelectedModeIndex() const;
	int32 GetSelectedModeChannelSpan() const;
	bool CanCreateFixture() const;
	FText GetValidationText() const;
	FText GetModeSummary() const;
	FText GetSelectionStatus() const;
	void SetStatus(const FText& Message, bool bSuccess);

	FString FixtureName = TEXT("Moving Head");
	float FixtureScale = 1.0f;
	FRotator ModelRotation = FRotator::ZeroRotator;
	FVector PanPivotOffset = FVector::ZeroVector;
	FVector TiltPivotOffset = FVector(0.0f, 0.0f, 40.0f);
	FVector LensOffset = FVector(20.0f, 0.0f, 0.0f);
	FRotator LensMeshRotation = FRotator::ZeroRotator;
	FRotator BeamRotation = FRotator::ZeroRotator;
	float PanMin = -270.0f;
	float PanMax = 270.0f;
	float TiltMin = -135.0f;
	float TiltMax = 135.0f;
	float PanOffset = 0.0f;
	float TiltOffset = 0.0f;
	float PanSpeed = 360.0f;
	float TiltSpeed = 360.0f;
	bool bInvertPan = false;
	bool bInvertTilt = false;
	float PreviewPan = 0.5f;
	float PreviewTilt = 0.5f;
	float MaximumIntensity = 50000.0f;
	float MinimumBeamAngle = 5.0f;
	float MaximumBeamAngle = 35.0f;
	float AttenuationRadius = 3000.0f;
	int32 Universe = 1;
	int32 Address = 1;

	TWeakObjectPtr<UDMXImportGDTF> GDTFSource;
	TWeakObjectPtr<UStaticMesh> BaseMesh;
	TWeakObjectPtr<UStaticMesh> YokeMesh;
	TWeakObjectPtr<UStaticMesh> HeadMesh;
	TWeakObjectPtr<UStaticMesh> LensMesh;
	TWeakObjectPtr<ATSAVDMXFixture> ActiveFixture;

	TArray<FDMXFixtureMode> ParsedModes;
	TArray<TSharedPtr<FString>> ModeOptions;
	TSharedPtr<FString> SelectedMode;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> ModeCombo;
	FText StatusMessage;
	bool bStatusSuccess = true;
};
