// Copyright TSAV. All Rights Reserved.

#include "TSAVDMXEditorUtils.h"

#include "Editor.h"
#include "EditorViewportClient.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "IO/DMXInputPort.h"
#include "IO/DMXOutputPort.h"
#include "IO/DMXPortManager.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXLibrary.h"
#include "LevelEditorViewport.h"
#include "Misc/AutomationTest.h"
#include "ScopedTransaction.h"
#include "TSAVDMXFixture.h"
#include "TSAVDMXFixtureCatalog.h"
#include "UObject/UnrealType.h"

namespace TSAVDMXEditorUtils::Private
{
	bool Matches(const FString& Candidate, const TArray<FString>& Aliases)
	{
		for (const FString& Alias : Aliases)
		{
			if (Candidate == Alias || Candidate.StartsWith(Alias))
			{
				return true;
			}
		}
		return false;
	}

	int32 NormalizedToFunctionValue(const FDMXFixtureFunction& Function, const float NormalizedValue)
	{
		const uint8 NumBytes = Function.GetNumChannels();
		const uint64 MaximumValue = NumBytes >= 4 ? MAX_uint32 : ((1ULL << (NumBytes * 8)) - 1ULL);
		const uint32 UnsignedValue = static_cast<uint32>(FMath::RoundToInt64(FMath::Clamp(NormalizedValue, 0.0f, 1.0f) * MaximumValue));
		return static_cast<int32>(UnsignedValue);
	}

	void AddMatchingFunctions(
		const FDMXFixtureMode& Mode,
		const TArray<FString>& Aliases,
		const float NormalizedValue,
		TMap<FDMXAttributeName, int32>& OutValues)
	{
		for (const FDMXFixtureFunction& Function : Mode.Functions)
		{
			if (Matches(CanonicalizeAttribute(Function.Attribute.Name), Aliases))
			{
				OutValues.Add(Function.Attribute, NormalizedToFunctionValue(Function, NormalizedValue));
			}
		}
	}
}

UTSAVDMXFixtureCatalog* TSAVDMXEditorUtils::LoadCatalog()
{
	UTSAVDMXFixtureCatalog* Catalog = Cast<UTSAVDMXFixtureCatalog>(UTSAVDMXFixtureCatalog::DefaultCatalogPath.TryLoad());
	if (Catalog && !Catalog->Fixtures.IsEmpty())
	{
		if (UDMXLibrary* Library = Catalog->Fixtures[0].DMXLibrary.LoadSynchronous())
		{
			EnsureLibraryPorts(*Library, true);
		}
	}
	return Catalog;
}

bool TSAVDMXEditorUtils::EnsureLibraryPorts(UDMXLibrary& Library, const bool bSaveAsset, FString* OutMessage)
{
	FStructProperty* PortReferencesProperty = FindFProperty<FStructProperty>(
		UDMXLibrary::StaticClass(), UDMXLibrary::GetPortReferencesPropertyName());
	FDMXLibraryPortReferences* PortReferences = PortReferencesProperty
		? PortReferencesProperty->ContainerPtrToValuePtr<FDMXLibraryPortReferences>(&Library)
		: nullptr;
	if (!PortReferences)
	{
		if (OutMessage)
		{
			*OutMessage = TEXT("The DMX library port-reference property could not be accessed.");
		}
		return false;
	}

	const TArray<FDMXInputPortSharedRef>& InputPorts = FDMXPortManager::Get().GetInputPorts();
	const TArray<FDMXOutputPortSharedRef>& OutputPorts = FDMXPortManager::Get().GetOutputPorts();
	if (InputPorts.IsEmpty() || OutputPorts.IsEmpty())
	{
		if (OutMessage)
		{
			*OutMessage = TEXT("No project DMX input or output port is configured.");
		}
		return false;
	}

	auto InputReferencesMatch = [&InputPorts, PortReferences]()
	{
		if (PortReferences->InputPortReferences.Num() != InputPorts.Num())
		{
			return false;
		}
		for (const FDMXInputPortSharedRef& Port : InputPorts)
		{
			if (!PortReferences->InputPortReferences.ContainsByPredicate([&Port](const FDMXInputPortReference& Reference)
				{
					return Reference.GetPortGuid() == Port->GetPortGuid() && Reference.IsEnabledFlagSet();
				}))
			{
				return false;
			}
		}
		return true;
	};
	auto OutputReferencesMatch = [&OutputPorts, PortReferences]()
	{
		if (PortReferences->OutputPortReferences.Num() != OutputPorts.Num())
		{
			return false;
		}
		for (const FDMXOutputPortSharedRef& Port : OutputPorts)
		{
			if (!PortReferences->OutputPortReferences.ContainsByPredicate([&Port](const FDMXOutputPortReference& Reference)
				{
					return Reference.GetPortGuid() == Port->GetPortGuid() && Reference.IsEnabledFlagSet();
				}))
			{
				return false;
			}
		}
		return true;
	};

	const bool bChanged = !InputReferencesMatch() || !OutputReferencesMatch();
	if (bChanged)
	{
		Library.Modify();
		PortReferences->InputPortReferences.Reset(InputPorts.Num());
		for (const FDMXInputPortSharedRef& Port : InputPorts)
		{
			PortReferences->InputPortReferences.Emplace(Port->GetPortGuid(), true);
		}
		PortReferences->OutputPortReferences.Reset(OutputPorts.Num());
		for (const FDMXOutputPortSharedRef& Port : OutputPorts)
		{
			PortReferences->OutputPortReferences.Emplace(Port->GetPortGuid(), true);
		}
		Library.MarkPackageDirty();
	}
	Library.UpdatePorts();

	if (bChanged && bSaveAsset)
	{
		TArray<UPackage*> PackagesToSave{ Library.GetOutermost() };
		if (!UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true))
		{
			if (OutMessage)
			{
				*OutMessage = TEXT("The DMX ports were connected in memory, but the library asset could not be saved.");
			}
			return false;
		}
	}

	if (OutMessage)
	{
		*OutMessage = FString::Printf(TEXT("DMX library connected to %d input and %d output port(s)."), InputPorts.Num(), OutputPorts.Num());
	}
	return true;
}

UDMXEntityFixturePatch* TSAVDMXEditorUtils::ResolvePatch(const FTSAVDMXFixtureDefinition& Definition)
{
	if (UDMXLibrary* Library = Definition.DMXLibrary.LoadSynchronous())
	{
		return Cast<UDMXEntityFixturePatch>(Library->FindEntity(Definition.FixturePatchId));
	}
	return nullptr;
}

FString TSAVDMXEditorUtils::CanonicalizeAttribute(const FName AttributeName)
{
	FString Result = AttributeName.ToString().ToLower();
	Result.ReplaceInline(TEXT("_"), TEXT(""));
	Result.ReplaceInline(TEXT("-"), TEXT(""));
	Result.ReplaceInline(TEXT(" "), TEXT(""));
	return Result;
}

bool TSAVDMXEditorUtils::SendControlValues(
	const FTSAVDMXFixtureDefinition& Definition,
	const FControlValues& Values,
	const bool bSnapPreview)
{
	UDMXEntityFixturePatch* Patch = ResolvePatch(Definition);
	const FDMXFixtureMode* Mode = Patch ? Patch->GetActiveMode() : nullptr;
	if (!Patch || !Mode)
	{
		return false;
	}

	TMap<FDMXAttributeName, int32> AttributeValues;
	Private::AddMatchingFunctions(*Mode, { TEXT("pan") }, Values.Pan, AttributeValues);
	Private::AddMatchingFunctions(*Mode, { TEXT("tilt") }, Values.Tilt, AttributeValues);
	Private::AddMatchingFunctions(*Mode, { TEXT("dimmer"), TEXT("intensity"), TEXT("masterdimmer") }, Values.Dimmer, AttributeValues);
	Private::AddMatchingFunctions(*Mode, { TEXT("coloraddr"), TEXT("colorrgbred"), TEXT("red") }, Values.Red, AttributeValues);
	Private::AddMatchingFunctions(*Mode, { TEXT("coloraddg"), TEXT("colorrgbgreen"), TEXT("green") }, Values.Green, AttributeValues);
	Private::AddMatchingFunctions(*Mode, { TEXT("coloraddb"), TEXT("colorrgbblue"), TEXT("blue") }, Values.Blue, AttributeValues);
	Private::AddMatchingFunctions(*Mode, { TEXT("zoom"), TEXT("beamangle") }, Values.Zoom, AttributeValues);
	if (!AttributeValues.IsEmpty())
	{
		Patch->SendDMX(AttributeValues);
	}
	PreviewMatchingActors(Definition, Values, bSnapPreview);
	return true;
}

bool TSAVDMXEditorUtils::SendAttributeValue(
	const FTSAVDMXFixtureDefinition& Definition,
	const FName AttributeName,
	const float NormalizedValue)
{
	UDMXEntityFixturePatch* Patch = ResolvePatch(Definition);
	const FDMXFixtureMode* Mode = Patch ? Patch->GetActiveMode() : nullptr;
	if (!Patch || !Mode)
	{
		return false;
	}

	TMap<FDMXAttributeName, int32> AttributeValues;
	for (const FDMXFixtureFunction& Function : Mode->Functions)
	{
		if (Function.Attribute.Name == AttributeName)
		{
			AttributeValues.Add(Function.Attribute, Private::NormalizedToFunctionValue(Function, NormalizedValue));
		}
	}
	if (AttributeValues.IsEmpty())
	{
		return false;
	}
	Patch->SendDMX(AttributeValues);
	return true;
}

TArray<ATSAVDMXFixture*> TSAVDMXEditorUtils::FindMatchingActors(const FTSAVDMXFixtureDefinition& Definition)
{
	TArray<ATSAVDMXFixture*> Result;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UDMXEntityFixturePatch* Patch = ResolvePatch(Definition);
	if (!World)
	{
		return Result;
	}
	for (TActorIterator<ATSAVDMXFixture> It(World); It; ++It)
	{
		ATSAVDMXFixture* Fixture = *It;
		if (Fixture && (Fixture->FixtureDefinitionId == Definition.DefinitionId || Fixture->GetFixturePatch() == Patch))
		{
			Result.Add(Fixture);
		}
	}
	return Result;
}

void TSAVDMXEditorUtils::PreviewMatchingActors(
	const FTSAVDMXFixtureDefinition& Definition,
	const FControlValues& Values,
	const bool bSnapPreview)
{
	const FLinearColor Color(Values.Red, Values.Green, Values.Blue);
	for (ATSAVDMXFixture* Fixture : FindMatchingActors(Definition))
	{
		Fixture->ApplyNormalizedDMX(Values.Pan, Values.Tilt, Values.Dimmer, Color, Values.Zoom, bSnapPreview);
	}
}

ATSAVDMXFixture* TSAVDMXEditorUtils::SpawnFixture(const FTSAVDMXFixtureDefinition& Definition)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	FVector Location(0.0, 0.0, 150.0);
	FRotator Rotation = FRotator::ZeroRotator;
	if (GCurrentLevelEditingViewportClient)
	{
		const FRotator ViewRotation = GCurrentLevelEditingViewportClient->GetViewRotation();
		Location = GCurrentLevelEditingViewportClient->GetViewLocation() + ViewRotation.Vector() * 400.0f;
		Rotation = FRotator(0.0f, ViewRotation.Yaw + 180.0f, 0.0f);
	}

	const FScopedTransaction Transaction(NSLOCTEXT("TSAVDMXEditorUtils", "SpawnFixtureTransaction", "Spawn TSAV DMX Fixture"));
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATSAVDMXFixture* Fixture = World->SpawnActor<ATSAVDMXFixture>(ATSAVDMXFixture::StaticClass(), FTransform(Rotation, Location), SpawnParameters);
	if (!Fixture || !Fixture->ApplyFixtureDefinition(Definition, true))
	{
		if (Fixture)
		{
			Fixture->Destroy();
		}
		return nullptr;
	}
	Fixture->SetActorLabel(Definition.DisplayName.ToString());
	Fixture->Modify();
	GEditor->SelectNone(false, true);
	GEditor->SelectActor(Fixture, true, true);
	return Fixture;
}

bool TSAVDMXEditorUtils::SelectMatchingActors(const FTSAVDMXFixtureDefinition& Definition)
{
	const TArray<ATSAVDMXFixture*> Fixtures = FindMatchingActors(Definition);
	if (Fixtures.IsEmpty() || !GEditor)
	{
		return false;
	}
	GEditor->SelectNone(false, true);
	for (ATSAVDMXFixture* Fixture : Fixtures)
	{
		GEditor->SelectActor(Fixture, true, false);
	}
	GEditor->NoteSelectionChange();
	return true;
}

bool TSAVDMXEditorUtils::IsPatchRangeAvailable(
	const UTSAVDMXFixtureCatalog& Catalog,
	const FName DefinitionId,
	const int32 Universe,
	const int32 Address,
	const int32 Span,
	FString& OutConflict)
{
	if (Universe < 1 || Address < 1 || Address > 512 || Span < 1 || Address + Span - 1 > 512)
	{
		OutConflict = TEXT("The patch must stay within channels 1–512 of a positive universe.");
		return false;
	}
	for (const FTSAVDMXFixtureDefinition& Other : Catalog.Fixtures)
	{
		if (Other.DefinitionId == DefinitionId)
		{
			continue;
		}
		UDMXEntityFixturePatch* OtherPatch = ResolvePatch(Other);
		const int32 OtherUniverse = OtherPatch ? OtherPatch->GetUniverseID() : Other.Universe;
		const int32 OtherAddress = OtherPatch ? OtherPatch->GetStartingChannel() : Other.Address;
		const int32 OtherSpan = OtherPatch ? FMath::Max(OtherPatch->GetChannelSpan(), 1) : FMath::Max(Other.ChannelSpan, 1);
		if (OtherUniverse == Universe && Address <= OtherAddress + OtherSpan - 1 && OtherAddress <= Address + Span - 1)
		{
			OutConflict = FString::Printf(TEXT("Overlaps %s at U%d.%03d–%03d."),
				*Other.DisplayName.ToString(), OtherUniverse, OtherAddress, OtherAddress + OtherSpan - 1);
			return false;
		}
	}
	return true;
}

bool TSAVDMXEditorUtils::UpdatePatchAddress(
	UTSAVDMXFixtureCatalog& Catalog,
	const FName DefinitionId,
	const int32 Universe,
	const int32 Address,
	FString& OutMessage)
{
	FTSAVDMXFixtureDefinition* Definition = Catalog.Fixtures.FindByPredicate(
		[DefinitionId](const FTSAVDMXFixtureDefinition& Item) { return Item.DefinitionId == DefinitionId; });
	UDMXEntityFixturePatch* Patch = Definition ? ResolvePatch(*Definition) : nullptr;
	if (!Definition || !Patch)
	{
		OutMessage = TEXT("The selected catalog definition or fixture patch is missing.");
		return false;
	}
	const int32 Span = FMath::Max(Patch->GetChannelSpan(), Definition->ChannelSpan);
	FString Conflict;
	if (!IsPatchRangeAvailable(Catalog, DefinitionId, Universe, Address, Span, Conflict))
	{
		OutMessage = Conflict;
		return false;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("TSAVDMXEditorUtils", "PatchAddressTransaction", "Change TSAV DMX Patch Address"));
	Patch->Modify();
	Catalog.Modify();
	Patch->SetUniverseID(Universe);
	Patch->SetStartingChannel(Address);
	Definition->Universe = Universe;
	Definition->Address = Address;
	Definition->ChannelSpan = Span;
	Patch->MarkPackageDirty();
	Catalog.MarkPackageDirty();

	TArray<UPackage*> PackagesToSave;
	PackagesToSave.AddUnique(Patch->GetOutermost());
	PackagesToSave.AddUnique(Catalog.GetOutermost());
	if (!UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true))
	{
		OutMessage = TEXT("The new patch address was applied in memory, but the master library could not be saved.");
		return false;
	}
	OutMessage = FString::Printf(TEXT("Patched %s to U%d.%03d–%03d and saved the master library."),
		*Definition->DisplayName.ToString(), Universe, Address, Address + Span - 1);
	return true;
}

bool TSAVDMXEditorUtils::RepackCatalog(
	UTSAVDMXFixtureCatalog& Catalog,
	const bool bSaveAssets,
	FString& OutMessage)
{
	if (Catalog.Fixtures.IsEmpty())
	{
		OutMessage = TEXT("The fixture catalog is empty.");
		return false;
	}

	UDMXLibrary* Library = Catalog.Fixtures[0].DMXLibrary.LoadSynchronous();
	if (!Library)
	{
		OutMessage = TEXT("The master DMX library could not be loaded.");
		return false;
	}

	Library->Modify();
	Catalog.Modify();
	int32 Universe = 1;
	int32 Address = 1;
	int32 RepackedPatches = 0;
	for (FTSAVDMXFixtureDefinition& Definition : Catalog.Fixtures)
	{
		UDMXEntityFixturePatch* Patch = ResolvePatch(Definition);
		if (!Patch || !Patch->GetActiveMode())
		{
			OutMessage = FString::Printf(TEXT("Cannot repack because %s has no usable fixture patch or active mode."),
				*Definition.DisplayName.ToString());
			return false;
		}

		Patch->RebuildCache();
		const int32 Span = FMath::Clamp(Patch->GetChannelSpan(), 1, 512);
		if (Address + Span - 1 > 512)
		{
			++Universe;
			Address = 1;
		}
		Patch->Modify();
		Patch->SetUniverseID(Universe);
		Patch->SetStartingChannel(Address);
		Definition.Universe = Universe;
		Definition.Address = Address;
		Definition.ChannelSpan = Span;
		Address += Span;
		++RepackedPatches;
	}

	Library->MarkPackageDirty();
	Catalog.MarkPackageDirty();
	if (bSaveAssets)
	{
		TArray<UPackage*> PackagesToSave{ Library->GetOutermost(), Catalog.GetOutermost() };
		if (!UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true))
		{
			OutMessage = TEXT("The patches were repacked in memory, but the library and catalog assets could not be saved.");
			return false;
		}
	}

	OutMessage = FString::Printf(TEXT("Repacked and %s %d fixture patches without overlaps across %d universes."),
		bSaveAssets ? TEXT("saved") : TEXT("updated"), RepackedPatches, Universe);
	return RepackedPatches == Catalog.Fixtures.Num();
}

bool TSAVDMXEditorUtils::ValidateCatalog(
	const UTSAVDMXFixtureCatalog& Catalog,
	FString& OutSummary,
	TArray<FString>& OutErrors)
{
	struct FRange
	{
		int32 Start = 1;
		int32 End = 1;
		FString Name;
	};
	TMap<int32, TArray<FRange>> RangesByUniverse;
	TSet<FName> DefinitionIds;
	int32 ValidPatches = 0;
	int32 ValidModes = 0;
	int32 HighestUniverse = 0;

	for (const FTSAVDMXFixtureDefinition& Definition : Catalog.Fixtures)
	{
		if (Definition.DefinitionId.IsNone() || DefinitionIds.Contains(Definition.DefinitionId))
		{
			OutErrors.Add(FString::Printf(TEXT("Missing or duplicate definition ID: %s"), *Definition.DisplayName.ToString()));
		}
		DefinitionIds.Add(Definition.DefinitionId);
		UDMXEntityFixturePatch* Patch = ResolvePatch(Definition);
		if (!Patch)
		{
			OutErrors.Add(FString::Printf(TEXT("Missing patch: %s"), *Definition.DisplayName.ToString()));
			continue;
		}
		++ValidPatches;
		const int32 Universe = Patch->GetUniverseID();
		const int32 Address = Patch->GetStartingChannel();
		const int32 Span = Patch->GetChannelSpan();
		HighestUniverse = FMath::Max(HighestUniverse, Universe);
		if (!Patch->GetActiveMode() || Span < 1)
		{
			OutErrors.Add(FString::Printf(TEXT("No usable active mode: %s"), *Definition.DisplayName.ToString()));
			continue;
		}
		++ValidModes;
		if (Universe < 1 || Address < 1 || Address + Span - 1 > 512)
		{
			OutErrors.Add(FString::Printf(TEXT("Out-of-range patch: %s at U%d.%03d with %d channels"),
				*Definition.DisplayName.ToString(), Universe, Address, Span));
			continue;
		}
		RangesByUniverse.FindOrAdd(Universe).Add({ Address, Address + Span - 1, Definition.DisplayName.ToString() });
	}

	for (TPair<int32, TArray<FRange>>& Pair : RangesByUniverse)
	{
		Pair.Value.Sort([](const FRange& A, const FRange& B) { return A.Start < B.Start; });
		for (int32 Index = 1; Index < Pair.Value.Num(); ++Index)
		{
			if (Pair.Value[Index].Start <= Pair.Value[Index - 1].End)
			{
				OutErrors.Add(FString::Printf(TEXT("Overlap in universe %d: %s and %s"),
					Pair.Key, *Pair.Value[Index - 1].Name, *Pair.Value[Index].Name));
			}
		}
	}

	OutSummary = FString::Printf(TEXT("Catalog %d | patches %d | modes %d | universes 1–%d | errors %d"),
		Catalog.Fixtures.Num(), ValidPatches, ValidModes, HighestUniverse, OutErrors.Num());
	return Catalog.Fixtures.Num() == 607 && ValidPatches == 607 && ValidModes == 607 && OutErrors.IsEmpty();
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTSAVDMXPatchAndConsoleCatalogTest,
	"TSAV.DMX.PatchAndConsole.Catalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTSAVDMXPatchAndConsoleCatalogTest::RunTest(const FString& Parameters)
{
	UTSAVDMXFixtureCatalog* Catalog = TSAVDMXEditorUtils::LoadCatalog();
	TestNotNull(TEXT("Generated fixture catalog loads"), Catalog);
	if (!Catalog)
	{
		return false;
	}
	TestEqual(TEXT("All fixture options are present"), Catalog->Fixtures.Num(), 607);
	FString RepackSummary;
	const bool bRepacked = TSAVDMXEditorUtils::RepackCatalog(*Catalog, true, RepackSummary);
	AddInfo(RepackSummary);
	TestTrue(TEXT("All generated fixture patches can be deterministically repacked and saved"), bRepacked);
	FString Summary;
	TArray<FString> Errors;
	const bool bValid = TSAVDMXEditorUtils::ValidateCatalog(*Catalog, Summary, Errors);
	for (const FString& Error : Errors)
	{
		AddError(Error);
	}
	TestTrue(*Summary, bValid);
	return bValid;
}
#endif
