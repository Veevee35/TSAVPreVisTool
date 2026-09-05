// Copyright TSAV. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "TSAVDMXPatchPlan.h"
#include "TSAVSuperStageDMX.h"
#include "STSAVDMXLightingConsoleTool.h"
#include "TSAVDMXFixtureCatalog.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "Widgets/SBoxPanel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTSAVPatchPlanTest, "TSAV.LightingConsole.PatchPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTSAVPatchPlanTest::RunTest(const FString&)
{
	using namespace TSAVDMXPatchPlan;
	const FRange A{TEXT("A"), TEXT("A"), 1, 1, 16, 512};
	const FRange B{TEXT("B"), TEXT("B"), 1, 17, 24, 512};
	TArray<FRange> Plan;
	FString Error;
	TestTrue(TEXT("Pack to channel 512 then roll over"), Build({A, B}, {A, B}, 2, 497, Plan, Error));
	if (Plan.Num() == 2)
	{
		TestEqual(TEXT("Requested start is retained"), Plan[0].Address, 497);
		TestEqual(TEXT("Next fixture universe"), Plan[1].Universe, 3);
		TestEqual(TEXT("Next fixture starts at 1"), Plan[1].Address, 1);
	}
	TestFalse(TEXT("First fixture cannot silently move to another universe"), Build({A}, {}, 1, 498, Plan, Error));
	TestTrue(TEXT("Failed batch returns no partial plan"), Plan.IsEmpty());
	const FRange Occupied{TEXT("Other"), TEXT("Other"), 3, 24, 10};
	TestFalse(TEXT("Later fixture inclusive overlap aborts entire batch"), Build({A, B}, {Occupied}, 2, 497, Plan, Error));
	TestTrue(TEXT("Overlap names the conflicting fixture"), Error.Contains(TEXT("Other")) && Plan.IsEmpty());
	TestTrue(TEXT("Adjacent ranges are valid"), Build({A}, {B}, 1, 1, Plan, Error));
	TestTrue(TEXT("Selected old ranges can be reused"), Build({A, B}, {A, B}, 1, 1, Plan, Error));
	TestFalse(TEXT("Vendor universe overflow rejected"), Build({A, B}, {}, 512, 497, Plan, Error));
	TestFalse(TEXT("Duplicate selection identity rejected"), Build({A, A}, {}, 1, 1, Plan, Error));
	FRange Invalid = A;
	Invalid.Span = 0;
	TestFalse(TEXT("Missing mode rejected"), Build({Invalid}, {}, 1, 1, Plan, Error));
	Invalid.Span = 513;
	TestFalse(TEXT("Oversized footprint rejected"), Build({Invalid}, {}, 1, 1, Plan, Error));
	TestFalse(TEXT("Zero address rejected"), Build({A}, {}, 1, 0, Plan, Error));
	TestFalse(TEXT("Zero universe rejected"), Build({A}, {}, 0, 1, Plan, Error));
	return true;
}

namespace TSAVPatchTest
{
	template<typename T> T* RequireProperty(const UStruct* Type, FName Name)
	{
		T* Property = FindFProperty<T>(Type, Name);
		checkf(Property, TEXT("Missing reflected test property %s"), *Name.ToString());
		return Property;
	}
	void SetInt(const UStruct* Type, void* Data, FName Name, int32 Value)
	{
		RequireProperty<FIntProperty>(Type, Name)->SetPropertyValue_InContainer(Data, Value);
	}
	int32 GetInt(const UStruct* Type, const void* Data, FName Name)
	{
		return RequireProperty<FIntProperty>(Type, Name)->GetPropertyValue_InContainer(Data);
	}
	UObject* CallObject(UObject* Object, FName Name)
	{
		UFunction* Function = Object ? Object->FindFunction(Name) : nullptr;
		if (!Function) return nullptr;
		FStructOnScope Params(Function);
		Object->ProcessEvent(Function, Params.GetStructMemory());
		return RequireProperty<FObjectPropertyBase>(Function, TEXT("ReturnValue"))->GetObjectPropertyValue_InContainer(Params.GetStructMemory());
	}
	AActor* MakeFixture(UWorld* World, UClass* ActorClass, UObject* Library, int32 Id)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transactional;
		AActor* Actor = World->SpawnActor<AActor>(ActorClass, FTransform::Identity, Params);
		if (auto* DefinitionProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Definition")))
		{
			UObject* Definition = LoadObject<UObject>(nullptr, TEXT("/SuperStage/Library/Lighting/Elation/SOL_I_Blinder/SOL_I_Blinder.SOL_I_Blinder"));
			DefinitionProperty->SetObjectPropertyValue_InContainer(Actor, Definition);
			auto* Modes = RequireProperty<FArrayProperty>(Definition->GetClass(), TEXT("Modes"));
			auto* ModeType = CastFieldChecked<FStructProperty>(Modes->Inner);
			FScriptArrayHelper ModeArray(Modes, Modes->ContainerPtrToValuePtr<void>(Definition));
			for (int32 I = 0; I < ModeArray.Num(); ++I)
			{
				void* Mode = ModeArray.GetRawPtr(I);
				if (RequireProperty<FObjectPropertyBase>(ModeType->Struct, TEXT("Channels"))->GetObjectPropertyValue_InContainer(Mode) != Library) continue;
				const FName Name = RequireProperty<FNameProperty>(ModeType->Struct, TEXT("ModeName"))->GetPropertyValue_InContainer(Mode);
				RequireProperty<FNameProperty>(ActorClass, TEXT("ActiveMode"))->SetPropertyValue_InContainer(Actor, Name);
			}
			FPropertyChangedEvent Event(DefinitionProperty);
			Actor->PostEditChangeProperty(Event);
		}
		RequireProperty<FObjectPropertyBase>(ActorClass, TEXT("FixtureLibrary"))->SetObjectPropertyValue_InContainer(Actor, Library);
		auto* ControlMode = RequireProperty<FEnumProperty>(ActorClass, TEXT("ControlMode"));
		const int64 DMXMode = ControlMode->GetEnum()->GetValueByNameString(TEXT("DMX"));
		ControlMode->GetUnderlyingProperty()->SetIntPropertyValue(ControlMode->ContainerPtrToValuePtr<void>(Actor), DMXMode);
		FPropertyChangedEvent ControlEvent(ControlMode);
		Actor->PostEditChangeProperty(ControlEvent);
		auto* Patch = RequireProperty<FStructProperty>(ActorClass, TEXT("SuperDMXFixture"));
		void* Data = Patch->ContainerPtrToValuePtr<void>(Actor);
		SetInt(Patch->Struct, Data, TEXT("FixtureID"), Id);
		SetInt(Patch->Struct, Data, TEXT("Universe"), 400);
		SetInt(Patch->Struct, Data, TEXT("StartAddress"), 1);
		return Actor;
	}
	float FinalValue(UObject* DMX, int32 Id)
	{
		UFunction* Fn = DMX->FindFunctionChecked(TEXT("GetFinalValue"));
		FStructOnScope Params(Fn);
		SetInt(Fn, Params.GetStructMemory(), TEXT("FixtureID"), Id);
		SetInt(Fn, Params.GetStructMemory(), TEXT("InstanceIndex"), 0);
		RequireProperty<FNameProperty>(Fn, TEXT("AttributeName"))->SetPropertyValue_InContainer(Params.GetStructMemory(), TEXT("Dimmer"));
		DMX->ProcessEvent(Fn, Params.GetStructMemory());
		return RequireProperty<FFloatProperty>(Fn, TEXT("ReturnValue"))->GetPropertyValue_InContainer(Params.GetStructMemory());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTSAVSuperStagePatchTest, "TSAV.SuperStage.SharedPatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTSAVSuperStagePatchTest::RunTest(const FString&)
{
	using namespace TSAVPatchTest;
	// Run only in the dedicated unattended process: never replace an interactive user's map.
	if (!TestTrue(TEXT("Run SharedPatch in the isolated unattended editor"), FApp::IsUnattended())) return false;
	UClass* ActorClass = FindObject<UClass>(nullptr, TEXT("/Script/SuperCore.SuperFixtureActor"));
	UClass* LibraryClass = FindObject<UClass>(nullptr, TEXT("/Script/SuperCore.SuperFixtureLibrary"));
	UClass* ConsoleClass = FindObject<UClass>(nullptr, TEXT("/Script/SuperConsole.SuperConsoleProSubsystem"));
	if (!TestNotNull(TEXT("SuperStage actor loaded"), ActorClass) || !LibraryClass || !ConsoleClass) return false;
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!TestTrue(TEXT("Use the isolated Entry startup map"), World && World->GetOutermost()->GetName() == TEXT("/Engine/Maps/Entry"))) return false;
	UObject* Library = LoadObject<UObject>(nullptr, TEXT("/SuperStage/Library/Lighting/Elation/SOL_I_Blinder/CL_SOL_I_Blinder_Dimmer_1CH.CL_SOL_I_Blinder_Dimmer_1CH"));
	if (!TestNotNull(TEXT("Supplied single-channel fixture mode loads"), Library)) return false;
	AActor* Actor = MakeFixture(World, ActorClass, Library, 91001);
	ON_SCOPE_EXIT { Actor->Destroy(); TSAVSuperStageDMX::SyncConsole(false); };
	TSAVSuperStageDMX::FFixture Fixture;
	TestTrue(TEXT("Read vendor's canonical patch"), TSAVSuperStageDMX::ReadFixture(Actor, Fixture));
	TestTrue(TEXT("Mode footprint covers channel data"), Fixture.Span > 0);
	TestEqual(TEXT("Mode attributes"), Fixture.Attributes.Num(), 1);
	TestTrue(TEXT("Set vendor universe/address"), TSAVSuperStageDMX::SetPatch(Actor, 401, 101));
	TestTrue(TEXT("Import and sync via vendor public API"), TSAVSuperStageDMX::SyncConsole(true));
	UObject* Console = GEngine->GetEngineSubsystemBase(ConsoleClass);
	UClass* DMXEngineClass = FindObject<UClass>(nullptr, TEXT("/Script/SuperDMX.SuperDMXSubsystem"));
	TestNotNull(TEXT("SuperDMX engine subsystem exists in this editor process"), DMXEngineClass ? GEngine->GetEngineSubsystemBase(DMXEngineClass) : nullptr);
	UObject* Patch = CallObject(Console, TEXT("GetPatchSubsystem"));
	UObject* DMX = CallObject(Console, TEXT("GetConsoleDMXSubsystem"));
	if (!TestNotNull(TEXT("Vendor patch subsystem available"), Patch) || !TestNotNull(TEXT("Vendor DMX subsystem available"), DMX)) return false;
	UFunction* Imported = Patch->FindFunctionChecked(TEXT("GetImportedFixtures"));
	FStructOnScope ImportedParams(Imported);
	Patch->ProcessEvent(Imported, ImportedParams.GetStructMemory());
	auto* ImportedArray = RequireProperty<FArrayProperty>(Imported, TEXT("ReturnValue"));
	auto* ImportedType = CastFieldChecked<FStructProperty>(ImportedArray->Inner);
	FScriptArrayHelper ImportedRows(ImportedArray, ImportedArray->ContainerPtrToValuePtr<void>(ImportedParams.GetStructMemory()));
	bool bFound = false;
	int32 ConsoleId = INDEX_NONE;
	for (int32 I = 0; I < ImportedRows.Num(); ++I)
	{
		void* Row = ImportedRows.GetRawPtr(I);
		if (GetInt(ImportedType->Struct, Row, TEXT("FixtureID")) != 91001) continue;
		bFound = true;
		ConsoleId = GetInt(ImportedType->Struct, Row, TEXT("Id"));
		TestEqual(TEXT("SuperStage console mirrors universe"), GetInt(ImportedType->Struct, Row, TEXT("DMXUniverse")), 401);
		TestEqual(TEXT("SuperStage console mirrors address"), GetInt(ImportedType->Struct, Row, TEXT("DMXAddress")), 101);
	}
	TestTrue(TEXT("Test fixture is in SuperStage imported patch"), bFound);
	AddInfo(FString::Printf(TEXT("Scene FixtureID 91001 resolves to console ID %d"), ConsoleId));
	TestEqual(TEXT("Actor GUID resolves console internal ID"), TSAVSuperStageDMX::GetConsoleFixtureId(Actor), ConsoleId);
	const bool bWasEnabled = TSAVSuperStageDMX::IsOutputEnabled();
	ON_SCOPE_EXIT { TSAVSuperStageDMX::SetOutputEnabled(bWasEnabled); };
	TestTrue(TEXT("Enable vendor output"), TSAVSuperStageDMX::SetOutputEnabled(true));
	TestTrue(TEXT("TSAV dimmer enters vendor output"), TSAVSuperStageDMX::SendAttribute(Actor, TEXT("Dimmer"), 0.5f));
	TestEqual(TEXT("Vendor final dimmer value"), FinalValue(DMX, ConsoleId), 0.5f);
	FEditorScriptExecutionGuard ScriptGuard;
	UFunction* Resolve = Actor->FindFunctionChecked(TEXT("ResolveAttributeAddressesByIndex"));
	FStructOnScope ResolveParams(Resolve);
	RequireProperty<FNameProperty>(Resolve, TEXT("AttribName"))->SetPropertyValue_InContainer(ResolveParams.GetStructMemory(), TEXT("Dimmer"));
	Actor->ProcessEvent(Resolve, ResolveParams.GetStructMemory());
	TestTrue(TEXT("Vendor resolves the fixture's dimmer channel"), RequireProperty<FBoolProperty>(Resolve, TEXT("ReturnValue"))->GetPropertyValue_InContainer(ResolveParams.GetStructMemory()));
	TestEqual(TEXT("Resolved dimmer address"), GetInt(Resolve, ResolveParams.GetStructMemory(), TEXT("OutCoarseAbs")), 101);

	// Exercise console selection, fader rebuilding and patching without loading/saving a catalog.
	auto Widget = MakeShared<STSAVDMXLightingConsoleTool>();
	Widget->AttributeFaders = SNew(SVerticalBox);
	Widget->RefreshRows(false);
	const FName Id(*(TEXT("SuperStage:") + Actor->GetActorGuid().ToString()));
	Widget->SetFixtureSelected(Id, ECheckBoxState::Checked);
	TestEqual(TEXT("Fader appears with heading"), Widget->AttributeFaders->GetChildren()->Num(), 2);
	Widget->RebuildAttributeFaders();
	TestEqual(TEXT("Cached fader survives rebuilding"), Widget->AttributeFaders->GetChildren()->Num(), 2);
	Widget->EditedUniverse = 402;
	Widget->EditedAddress = 201;
	Widget->bPatchDraftDirty = true;
	Widget->ApplyPatchClicked();
	TSAVSuperStageDMX::ReadFixture(Actor, Fixture);
	TestEqual(TEXT("Console patches actual actor universe"), Fixture.Universe, 402);
	TestEqual(TEXT("Console patches actual actor address"), Fixture.Address, 201);
	TestTrue(TEXT("Patch operation is undoable"), GEditor->UndoTransaction());
	Widget->RefreshRows(false);
	TSAVSuperStageDMX::ReadFixture(Actor, Fixture);
	TestEqual(TEXT("Undo restores vendor address"), Fixture.Address, 101);
	TestEqual(TEXT("Undo restores vendor universe"), Fixture.Universe, 401);
	TSAVSuperStageDMX::SetPatch(Actor, 403, 301);
	Widget->RefreshRows(false);
	TestEqual(TEXT("External scene change appears in TSAV"), Widget->GetPrimaryRow()->Universe, 403);
	TestEqual(TEXT("Address inputs follow external change"), Widget->EditedAddress, 301);
	Widget->GrandMaster = 0.5f;
	Widget->SendAttributeValue(TEXT("Dimmer"), 0.8f);
	TestEqual(TEXT("Raw dimmer respects grand master"), FinalValue(DMX, ConsoleId), 0.4f);
	Widget->BlackoutClicked();
	Widget->SendAttributeValue(TEXT("Dimmer"), 1.0f);
	TestEqual(TEXT("Raw dimmer cannot bypass blackout"), FinalValue(DMX, ConsoleId), 0.0f);
	// Read the shipped native catalog without invoking the port auto-save helper.
	Widget->Catalog = Cast<UTSAVDMXFixtureCatalog>(UTSAVDMXFixtureCatalog::DefaultCatalogPath.TryLoad());
	if (TestNotNull(TEXT("Native fixture catalog loads"), Widget->Catalog) && !Widget->Catalog->Fixtures.IsEmpty())
	{
		Widget->SelectedDefinitionIds.Reset();
		Widget->RefreshRows(false);
		Widget->SetFixtureSelected(Widget->Catalog->Fixtures[0].DefinitionId, ECheckBoxState::Checked);
		const int32 FaderCount = Widget->AttributeFaders->GetChildren()->Num();
		TestTrue(TEXT("Native primary mode exposes raw faders"), FaderCount > 1);
		Widget->RebuildAttributeFaders();
		TestEqual(TEXT("Native raw faders survive cached values and selection rebuild"), Widget->AttributeFaders->GetChildren()->Num(), FaderCount);
	}
	TSAVSuperStageDMX::SetOutputEnabled(false);
	TestFalse(TEXT("Disabled output does not report a sent value"), TSAVSuperStageDMX::SendAttribute(Actor, TEXT("Dimmer"), 1.0f));
	AddInfo(TEXT("Shared patch and console-value checks complete. Fixture evaluation and rendered lighting are separate checks."));
	return true;
}

// Deliberately separate from the patch contract tests. Requires a working vendor
// session; a loaded DLL or accepted programmer value does not establish output.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTSAVSuperStageOutputProbe, "TSAV.SuperStage.FixtureOutputProbe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTSAVSuperStageOutputProbe::RunTest(const FString&)
{
	using namespace TSAVPatchTest;
	if (!TestTrue(TEXT("Run output probe in the isolated unattended editor"), FApp::IsUnattended())) return false;
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!TestTrue(TEXT("Use the isolated Entry startup map"), World && World->GetOutermost()->GetName() == TEXT("/Engine/Maps/Entry"))) return false;
	UClass* ActorClass = FindObject<UClass>(nullptr, TEXT("/Script/SuperCore.SuperFixtureActor"));
	UObject* Library = LoadObject<UObject>(nullptr, TEXT("/SuperStage/Library/Lighting/Elation/SOL_I_Blinder/CL_SOL_I_Blinder_Dimmer_1CH.CL_SOL_I_Blinder_Dimmer_1CH"));
	if (!TestNotNull(TEXT("Vendor actor class"), ActorClass) || !TestNotNull(TEXT("Vendor fixture mode"), Library)) return false;
	AActor* Actor = MakeFixture(World, ActorClass, Library, 91002);
	const bool bWasEnabled = TSAVSuperStageDMX::IsOutputEnabled();
	TSAVSuperStageDMX::SyncConsole(true);
	TSAVSuperStageDMX::SetOutputEnabled(true);
	TestTrue(TEXT("Submit half dimmer to the vendor console"), TSAVSuperStageDMX::SendAttribute(Actor, TEXT("Dimmer"), 0.5f));
	const double ReadbackStart = FPlatformTime::Seconds();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Actor, bWasEnabled, ReadbackStart, BlackoutStart = 0.0]() mutable
	{
		if (FPlatformTime::Seconds() - ReadbackStart < 2.0) return false;
		FEditorScriptExecutionGuard ReadbackGuard;
		Actor->ProcessEvent(Actor->FindFunctionChecked(TEXT("ForceRefreshDMX")), nullptr);
		UFunction* Fn = Actor->FindFunctionChecked(TEXT("GetAttributeRaw8ByIndex"));
		FStructOnScope Params(Fn);
		SetInt(Fn, Params.GetStructMemory(), TEXT("DefaultValue"), -1);
		RequireProperty<FNameProperty>(Fn, TEXT("AttribName"))->SetPropertyValue_InContainer(Params.GetStructMemory(), TEXT("Dimmer"));
		Actor->ProcessEvent(Fn, Params.GetStructMemory());
		const int32 Value = GetInt(Fn, Params.GetStructMemory(), TEXT("ReturnValue"));
		if (BlackoutStart == 0.0)
		{
			UClass* EvaluatorClass = FindObject<UClass>(nullptr, TEXT("/Script/SuperCore.SuperFixtureWorldSubsystem"));
			UWorldSubsystem* Evaluator = EvaluatorClass ? Actor->GetWorld()->GetSubsystemBase(EvaluatorClass) : nullptr;
			if (TestNotNull(TEXT("Fixture evaluator exists"), Evaluator))
			{
				for (FName Name : {FName(TEXT("GetNumFixtures")), FName(TEXT("GetLastEvaluatedCount"))})
				{
					UFunction* GetCount = Evaluator->FindFunctionChecked(Name);
					FStructOnScope CountParams(GetCount);
					Evaluator->ProcessEvent(GetCount, CountParams.GetStructMemory());
					const int32 Count = GetInt(GetCount, CountParams.GetStructMemory(), TEXT("ReturnValue"));
					AddInfo(FString::Printf(TEXT("%s: %d"), *Name.ToString(), Count));
					TestTrue(FString::Printf(TEXT("Vendor %s is positive"), *Name.ToString()), Count > 0);
				}
			}
			BlackoutStart = FPlatformTime::Seconds();
			AddInfo(FString::Printf(TEXT("Fixture half-dimmer raw readback: %d"), Value));
			TestTrue(TEXT("Fixture receives half dimmer on its patched channel"), Value >= 127 && Value <= 128);
			TSAVSuperStageDMX::SendAttribute(Actor, TEXT("Dimmer"), 0.0f);
			return false;
		}
		if (FPlatformTime::Seconds() - BlackoutStart < 0.5) return false;
		AddInfo(FString::Printf(TEXT("Fixture blackout raw readback: %d"), Value));
		TestEqual(TEXT("Fixture receives blackout on its patched channel"), Value, 0);
		Actor->Destroy();
		TSAVSuperStageDMX::SyncConsole(false);
		TSAVSuperStageDMX::SetOutputEnabled(bWasEnabled);
		return true;
	}));
	return true;
}
#endif
