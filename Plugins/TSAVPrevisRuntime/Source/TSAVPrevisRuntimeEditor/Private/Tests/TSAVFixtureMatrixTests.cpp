// Copyright TSAV. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "Lighting/TSAVLightingShow.h"
#include "TSAVDMXFixture.h"
#include "Components/SpotLightComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXLibrary.h"
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTSAVFixtureMatrixTest,"TSAV.LightingConsole.MatrixFixtures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTSAVFixtureMatrixTest::RunTest(const FString&)
{
	UWorld* World=GEditor->GetEditorWorldContext().World();
	if (!FApp::IsUnattended() || !World || World->GetOutermost()->GetName()!=TEXT("/Engine/Maps/Entry")) return false;
	auto* Library=NewObject<UDMXLibrary>(); FDMXFixtureMode Mode;
	Mode.ModeName=TEXT("Original two-pixel RGB dimmer test"); Mode.ChannelSpan=9; Mode.bAutoChannelSpan=false; Mode.bFixtureMatrixEnabled=true;
	FDMXFixtureFunction Dimmer; Dimmer.Attribute=FDMXAttributeName(TEXT("Dimmer")); Dimmer.Channel=1; Mode.Functions.Add(Dimmer);
	Mode.FixtureMatrixConfig.XCells=2; Mode.FixtureMatrixConfig.YCells=1; Mode.FixtureMatrixConfig.FirstCellChannel=2;
	Mode.FixtureMatrixConfig.CellAttributes.Reset();
	for (FName Name : {TEXT("Dimmer"),TEXT("ColorAdd_R"),TEXT("ColorAdd_G"),TEXT("ColorAdd_B")}) {
		FDMXFixtureCellAttribute Attribute; Attribute.Attribute=FDMXAttributeName(Name); Attribute.DefaultValue=255; Mode.FixtureMatrixConfig.CellAttributes.Add(Attribute);
	}
	FDMXEntityFixtureTypeConstructionParams TypeParams; TypeParams.ParentDMXLibrary=Library; TypeParams.Modes.Add(Mode);
	auto* Type=UDMXEntityFixtureType::CreateFixtureTypeInLibrary(TypeParams,TEXT("Native matrix"),false);
	FDMXEntityFixturePatchConstructionParams PatchParams; PatchParams.FixtureTypeRef=FDMXEntityFixtureTypeRef(Type);
	auto* Patch=UDMXEntityFixturePatch::CreateFixturePatchInLibrary(PatchParams,TEXT("Matrix patch"),false);
	auto* A=World->SpawnActor<ATSAVDMXFixture>(); auto* B=World->SpawnActor<ATSAVDMXFixture>(); auto* Show=ATSAVLightingShow::Find(World,true);
	ON_SCOPE_EXIT { A->Destroy(); B->Destroy(); Show->Destroy(); };
	A->SetFixturePatch(Patch); A->ApplyNormalizedDMX(0.5f,0.5f,1,FLinearColor::White,0,true);
	TestEqual(TEXT("Native matrix dimensions"),A->GetMatrixDimensions(),FIntPoint(2,1));
	TestTrue(TEXT("Cell zero independent red"),A->SetMatrixCellAttributes(FIntPoint(0,0),{{TEXT("Red"),1},{TEXT("Green"),0},{TEXT("Blue"),0},{TEXT("Dimmer"),1}}));
	TestTrue(TEXT("Cell one independent blue"),A->SetMatrixCellAttributes(FIntPoint(1,0),{{TEXT("Red"),0},{TEXT("Green"),0},{TEXT("Blue"),1},{TEXT("Dimmer"),0.5f}}));
	TestFalse(TEXT("Reject cell outside mode"),A->SetMatrixCellAttributes(FIntPoint(2,0),{{TEXT("Dimmer"),1}}));
	auto* Left=A->GetMatrixCellLight(FIntPoint(0,0)); auto* Right=A->GetMatrixCellLight(FIntPoint(1,0));
	if (!TestNotNull(TEXT("Left light component"),Left) || !TestNotNull(TEXT("Right light component"),Right)) return false;
	TestTrue(TEXT("Different cells produce different colors"),Left->GetLightColor().Equals(FLinearColor::Red,0.01f) && Right->GetLightColor().Equals(FLinearColor::Blue,0.01f));
	TestTrue(TEXT("Independent cell intensity"),FMath::IsNearlyEqual(Right->Intensity,A->MaximumIntensityLumens*0.25f));
	TestTrue(TEXT("Cells have distinct physical positions"),!Left->GetRelativeLocation().Equals(Right->GetRelativeLocation()));
	TestEqual(TEXT("Single beam suppressed for a matrix"),A->GetBeamLightComponent()->Intensity,0.0f);
	Show->SetProgrammer({A},{{ATSAVDMXFixture::MatrixAttributeKey(FIntPoint(0,0),TEXT("ColorAdd_R")),0.2f}});
	TestTrue(TEXT("Matrix aliases reach the same cell attribute"),FMath::IsNearlyEqual(Left->GetLightColor().R,0.2f,0.02f));
	Show->StoreCue(1,TEXT("Matrix look"),{A},0); Show->ClearProgrammer();
	TestTrue(TEXT("Release restores cell baseline"),FMath::IsNearlyEqual(Left->GetLightColor().R,1.0f));
	Show->Go(1); Show->SetMaster(0.5f);
	TestTrue(TEXT("Show master scales cell output"),FMath::IsNearlyEqual(Right->Intensity,A->MaximumIntensityLumens*0.125f));
	TestTrue(TEXT("Matrix configuration and cell state restore"),B->RestoreTSAVState(A->CaptureTSAVState()));
	if (!TestNotNull(TEXT("Restored matrix cell"),B->GetMatrixCellLight(FIntPoint(1,0)))) return false;
	TestTrue(TEXT("Restored matrix matches actual output"),FMath::IsNearlyEqual(B->GetMatrixCellLight(FIntPoint(1,0))->Intensity,Right->Intensity));
	const FString BeforeInvalid=B->CaptureTSAVState();
	FDMXFixtureMode Invalid=Mode; Invalid.FixtureMatrixConfig.XCells=MAX_int32;
	TestFalse(TEXT("Reject oversized matrix before allocation"),B->SetStandaloneMode(Invalid,1,1));
	Invalid=Mode; Invalid.FixtureMatrixConfig.FirstCellChannel=1;
	TestFalse(TEXT("Reject matrix overlapping the main dimmer"),B->SetStandaloneMode(Invalid,1,1));
	TestEqual(TEXT("Invalid mode leaves the fixture intact"),B->CaptureTSAVState(),BeforeInvalid);
	Show->SetBlackout(true); TestEqual(TEXT("Blackout reaches every cell"),Right->Intensity,0.0f);
	return true;
}
#endif
