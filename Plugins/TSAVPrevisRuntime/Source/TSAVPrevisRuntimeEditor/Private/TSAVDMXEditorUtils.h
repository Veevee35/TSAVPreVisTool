// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ATSAVDMXFixture;
class UDMXEntityFixturePatch;
class UDMXLibrary;
class UTSAVDMXFixtureCatalog;
struct FTSAVDMXFixtureDefinition;

namespace TSAVDMXEditorUtils
{
	struct FControlValues
	{
		float Pan = 0.5f;
		float Tilt = 0.5f;
		float Dimmer = 1.0f;
		float Red = 1.0f;
		float Green = 1.0f;
		float Blue = 1.0f;
		float Zoom = 0.0f;
	};

	UTSAVDMXFixtureCatalog* LoadCatalog();
	bool EnsureLibraryPorts(UDMXLibrary& Library, bool bSaveAsset, FString* OutMessage = nullptr);
	UDMXEntityFixturePatch* ResolvePatch(const FTSAVDMXFixtureDefinition& Definition);
	FString CanonicalizeAttribute(FName AttributeName);
	bool SendControlValues(const FTSAVDMXFixtureDefinition& Definition, const FControlValues& Values, bool bSnapPreview = false);
	bool SendAttributeValue(const FTSAVDMXFixtureDefinition& Definition, FName AttributeName, float NormalizedValue);
	void PreviewMatchingActors(const FTSAVDMXFixtureDefinition& Definition, const FControlValues& Values, bool bSnapPreview);
	TArray<ATSAVDMXFixture*> FindMatchingActors(const FTSAVDMXFixtureDefinition& Definition);
	ATSAVDMXFixture* SpawnFixture(const FTSAVDMXFixtureDefinition& Definition);
	bool SelectMatchingActors(const FTSAVDMXFixtureDefinition& Definition);
	bool IsPatchRangeAvailable(
		const UTSAVDMXFixtureCatalog& Catalog,
		FName DefinitionId,
		int32 Universe,
		int32 Address,
		int32 Span,
		FString& OutConflict);
	bool UpdatePatchAddress(UTSAVDMXFixtureCatalog& Catalog, FName DefinitionId, int32 Universe, int32 Address, FString& OutMessage);
	bool RepackCatalog(UTSAVDMXFixtureCatalog& Catalog, bool bSaveAssets, FString& OutMessage);
	bool ValidateCatalog(const UTSAVDMXFixtureCatalog& Catalog, FString& OutSummary, TArray<FString>& OutErrors);
}
