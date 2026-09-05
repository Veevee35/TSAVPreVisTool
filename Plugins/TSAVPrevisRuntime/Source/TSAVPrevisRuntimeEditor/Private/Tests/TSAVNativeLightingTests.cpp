// Copyright TSAV. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "STSAVDMXLightingConsoleTool.h"
#include "TSAVDMXFixture.h"
#include "TSAVDMXFixtureCatalog.h"
#include "Components/SpotLightComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXLibrary.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTSAVNativeLightingTest, "TSAV.LightingConsole.NativeFixtures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTSAVNativeLightingTest::RunTest(const FString&)
{
	if (!TestTrue(TEXT("Use an isolated unattended editor"), FApp::IsUnattended())) return false;
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!TestTrue(TEXT("Use Entry, never the user's scene"), World && World->GetOutermost()->GetName() == TEXT("/Engine/Maps/Entry"))) return false;
	for (const FName Module : {TEXT("SuperCore"), TEXT("SuperAssets"), TEXT("SuperDMX"), TEXT("SuperNdi"), TEXT("SuperMadrix"), TEXT("SuperLaser"), TEXT("SuperShader"), TEXT("SuperAuth"), TEXT("SuperTools"), TEXT("SuperConsole")})
		TestFalse(FString::Printf(TEXT("%s is unloaded"), *Module.ToString()), FModuleManager::Get().IsModuleLoaded(Module));

	// Original seven-channel profile; no fixture models, vendor assets or network ports.
	UDMXLibrary* Library = NewObject<UDMXLibrary>(GetTransientPackage(), NAME_None, RF_Transactional);
	FDMXFixtureMode Mode;
	Mode.ModeName = TEXT("TSAV Test RGB Moving Light");
	Mode.bAutoChannelSpan = false;
	Mode.ChannelSpan = 7;
	for (const FName Name : {TEXT("Pan"), TEXT("Tilt"), TEXT("Dimmer"), TEXT("ColorAdd_R"), TEXT("ColorAdd_G"), TEXT("ColorAdd_B"), TEXT("Zoom")})
	{
		FDMXFixtureFunction Function;
		Function.Attribute = FDMXAttributeName(Name);
		Function.FunctionName = Name.ToString();
		Function.Channel = Mode.Functions.Num() + 1;
		Mode.Functions.Add(Function);
	}
	FDMXEntityFixtureTypeConstructionParams TypeParams;
	TypeParams.ParentDMXLibrary = Library;
	TypeParams.Modes.Add(Mode);
	UDMXEntityFixtureType* Type = UDMXEntityFixtureType::CreateFixtureTypeInLibrary(TypeParams, TEXT("TSAV Test Type"), false);
	FDMXEntityFixturePatchConstructionParams PatchParams;
	PatchParams.FixtureTypeRef = FDMXEntityFixtureTypeRef(Type);
	UDMXEntityFixturePatch* Template = UDMXEntityFixturePatch::CreateFixturePatchInLibrary(PatchParams, TEXT("TSAV Test Template"), false);
	if (!TestNotNull(TEXT("Original test fixture patch"), Template)) return false;
	TestEqual(TEXT("Full mode footprint"), Template->GetChannelSpan(), 7);
	UTSAVDMXFixtureCatalog* Catalog = NewObject<UTSAVDMXFixtureCatalog>();
	FTSAVDMXFixtureDefinition Definition;
	Definition.DefinitionId = TEXT("TSAVOriginalTest");
	Definition.DisplayName = FText::FromString(TEXT("TSAV Original Test"));
	Definition.DMXLibrary = Library;
	Definition.FixturePatchId = Template->GetID();
	Definition.ChannelSpan = 7;
	Catalog->Fixtures.Add(Definition);
	FActorSpawnParameters Spawn;
	Spawn.ObjectFlags = RF_Transactional;
	ATSAVDMXFixture* A = World->SpawnActor<ATSAVDMXFixture>(ATSAVDMXFixture::StaticClass(), FTransform::Identity, Spawn);
	ATSAVDMXFixture* B = World->SpawnActor<ATSAVDMXFixture>(ATSAVDMXFixture::StaticClass(), FTransform::Identity, Spawn);
	ON_SCOPE_EXIT { if (A) A->Destroy(); if (B) B->Destroy(); };
	if (!A || !B) return false;
	A->SetActorLabel(TEXT("Native A"));
	B->SetActorLabel(TEXT("Native B"));
	A->FixtureDefinitionId = B->FixtureDefinitionId = Definition.DefinitionId;
	A->SetFixturePatch(Template);
	B->SetFixturePatch(Template);

	auto Widget = MakeShared<STSAVDMXLightingConsoleTool>();
	Widget->Catalog = Catalog;
	Widget->AttributeFaders = SNew(SVerticalBox);
	Widget->RefreshRows(false);
	Widget->SelectPlacedClicked();
	TestEqual(TEXT("Select Placed excludes the shared library template"), Widget->SelectedDefinitionIds.Num(), 2);
	Widget->EditedUniverse = 2;
	Widget->EditedAddress = 500;
	Widget->bPatchDraftDirty = true;
	Widget->ApplyPatchClicked();
	TestTrue(TEXT("A has an individual patch"), A->GetFixturePatch() != Template);
	TestTrue(TEXT("B has a different individual patch"), B->GetFixturePatch() != Template && A->GetFixturePatch() != B->GetFixturePatch());
	TestEqual(TEXT("Requested address retained"), A->GetFixturePatch()->GetStartingChannel(), 500);
	TestEqual(TEXT("Second light rolls to next universe"), B->GetFixturePatch()->GetUniverseID(), 3);
	TestEqual(TEXT("Second light starts at one"), B->GetFixturePatch()->GetStartingChannel(), 1);
	TestEqual(TEXT("Library template address unchanged"), Template->GetStartingChannel(), 1);
	TestEqual(TEXT("Library template universe unchanged"), Template->GetUniverseID(), 1);
	TestTrue(TEXT("Patch batch Undo"), GEditor->UndoTransaction());
	TestTrue(TEXT("Undo restores A's original binding"), A->GetFixturePatch() == Template);
	TestTrue(TEXT("Undo restores B's original binding"), B->GetFixturePatch() == Template);
	TestTrue(TEXT("Patch batch Redo"), GEditor->RedoTransaction());
	TestEqual(TEXT("Redo restores A's address"), A->GetFixturePatch()->GetStartingChannel(), 500);
	TestEqual(TEXT("Redo restores B's universe"), B->GetFixturePatch()->GetUniverseID(), 3);

	const FName AId(*(TEXT("TSAVScene:") + A->GetActorGuid().ToString()));
	Widget->SelectedDefinitionIds.Reset();
	Widget->RefreshRows(false);
	Widget->SetFixtureSelected(AId, ECheckBoxState::Checked);
	TestEqual(TEXT("Native scene row exposes all mode faders plus heading"), Widget->AttributeFaders->GetChildren()->Num(), 8);
	Widget->RebuildAttributeFaders();
	TestEqual(TEXT("Native scene faders survive rebuild"), Widget->AttributeFaders->GetChildren()->Num(), 8);
	Widget->EditedUniverse = 3;
	Widget->EditedAddress = 1;
	Widget->bPatchDraftDirty = true;
	Widget->ApplyPatchClicked();
	TestEqual(TEXT("Occupied scene address is rejected"), A->GetFixturePatch()->GetUniverseID(), 2);
	TestFalse(TEXT("Invalid direct address is rejected"), A->SetIndividualPatchAddress(1, 510));

	A->ApplyNormalizedDMX(0.8f, 0.2f, 0.7f, FLinearColor(0.2f, 0.3f, 0.4f), 0.5f, true);
	B->ApplyNormalizedDMX(0.5f, 0.5f, 1.0f, FLinearColor::White, 0.0f, true);
	const float PanBefore = A->CurrentPanDegrees;
	const float TiltBefore = A->CurrentTiltDegrees;
	Widget->GrandMaster = 0.5f;
	Widget->SendAttributeValue(TEXT("Dimmer"), 0.8f);
	TestEqual(TEXT("Native dimmer reaches the actual light component"), A->GetBeamLightComponent()->Intensity, A->MaximumIntensityLumens * 0.4f);
	TestEqual(TEXT("Same model at another address remains independent"), B->GetBeamLightComponent()->Intensity, B->MaximumIntensityLumens);
	TestEqual(TEXT("Partial dimmer update preserves pan"), A->CurrentPanDegrees, PanBefore);
	TestEqual(TEXT("Partial dimmer update preserves tilt"), A->CurrentTiltDegrees, TiltBefore);
	TestTrue(TEXT("Partial update preserves color"), A->GetBeamLightComponent()->GetLightColor().Equals(FLinearColor(0.2f, 0.3f, 0.4f), 0.02f));
	Widget->BlackoutClicked();
	TestEqual(TEXT("Blackout reaches actual native light"), A->GetBeamLightComponent()->Intensity, 0.0f);
	Widget->SendAttributeValue(TEXT("Dimmer"), 1.0f);
	TestEqual(TEXT("Raw faders cannot bypass blackout"), A->GetBeamLightComponent()->Intensity, 0.0f);
	TestEqual(TEXT("Blackout selection doesn't black out the other fixture"), B->GetBeamLightComponent()->Intensity, B->MaximumIntensityLumens);
	Widget->ReleaseBlackoutClicked();
	TestTrue(TEXT("Release blackout restores native light"), A->GetBeamLightComponent()->Intensity > 0.0f);
	AddInfo(TEXT("Native patch, Undo/Redo, individual fixture control, partial attributes and actual light-component output verified with SuperStage unloaded. No external DMX or rendered viewport acceptance is claimed."));
	return true;
}
#endif
