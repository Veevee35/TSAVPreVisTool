// Copyright TSAV. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "Lighting/TSAVRigExchange.h"
#include "Lighting/TSAVLightingShow.h"
#include "UI/TSAVRigWidget.h"
#include "TSAVDMXFixture.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Components/SpotLightComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "Widgets/SWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTSAVRigExchangeTest,"TSAV.NativeRig.Exchange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTSAVRigExchangeTest::RunTest(const FString&)
{
	UWorld* World=GEditor->GetEditorWorldContext().World();
	if (!FApp::IsUnattended() || !World || World->GetOutermost()->GetName()!=TEXT("/Engine/Maps/Entry")) return false;
	const FString Path=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("NativeLightingReview/OriginalRig.mvr")));
	TArray<ATSAVDMXFixture*> Actors; auto* A=World->SpawnActor<ATSAVDMXFixture>(); Actors.Add(A);
	ON_SCOPE_EXIT { for (auto* Actor : Actors) if (IsValid(Actor)) Actor->Destroy(); IFileManager::Get().Delete(*Path); };
	FDMXFixtureMode Mode; Mode.ModeName=TEXT("Native RGB 16-bit pan"); Mode.bAutoChannelSpan=false; Mode.ChannelSpan=7;
	int32 Channel=1;
	for (FName Name : {TEXT("Pan"),TEXT("Dimmer"),TEXT("ColorAdd_R"),TEXT("ColorAdd_G"),TEXT("ColorAdd_B"),TEXT("Tilt")}) {
		FDMXFixtureFunction Function; Function.Attribute=FDMXAttributeName(Name); Function.FunctionName=Name.ToString(); Function.Channel=Channel;
		if (Name==TEXT("Pan")) { Function.DataType=EDMXFixtureSignalFormat::E16Bit; Function.bUseLSBMode=true; Function.DefaultValue=32768; }
		else Function.DefaultValue=128;
		Channel+=Function.GetNumChannels(); Mode.Functions.Add(Function);
	}
	TestTrue(TEXT("Runtime mode and standalone patch"),A->SetStandaloneMode(Mode,4,100));
	const FTransform Transform(FRotator(20,30,40),FVector(123,-456,789),FVector(1.2,1.2,1.2)); A->SetActorTransform(Transform);
	FTransform ReadTransform; TestTrue(TEXT("Matrix parses"),TSAVRigExchange::ParseMatrix(TSAVRigExchange::WriteMatrix(Transform),ReadTransform));
	TestTrue(TEXT("Matrix units, axes and rotation round trip"),ReadTransform.Equals(Transform,0.0001));
	TestTrue(TEXT("Known MVR translation parses"),TSAVRigExchange::ParseMatrix(TEXT("{1,0,0}{0,1,0}{0,0,1}{1000,2000,3000}"),ReadTransform));
	TestTrue(TEXT("Millimeters and handedness converted"),ReadTransform.GetLocation().Equals(FVector(100,-200,300)));
	TestFalse(TEXT("Shear is not silently discarded"),TSAVRigExchange::ParseMatrix(TEXT("{1,1,0}{0,1,0}{0,0,1}{0,0,0}"),ReadTransform));
	FString Error; TArray64<uint8> Bytes;
	TestTrue(TEXT("Original mode generates a standard GDTF"),TSAVRigExchange::GenerateGDTF(A,Bytes,Error));
	FTSAVGDTFProfile Profile;
	if (!TestTrue(TEXT("Generated GDTF parses through Unreal GDTF"),TSAVRigExchange::ReadGDTF(Bytes,TEXT("Original.gdtf"),Profile,Error))) { AddError(Error); return false; }
	TestEqual(TEXT("Mode footprint preserved"),Profile.Modes[0].DMX.ChannelSpan,7);
	TestTrue(TEXT("16-bit little-endian attribute preserved"),Profile.Modes[0].DMX.Functions[0].bUseLSBMode && Profile.Modes[0].DMX.Functions[0].GetNumChannels()==2);
	TestEqual(TEXT("Default value preserved"),Profile.Modes[0].DMX.Functions[0].DefaultValue,static_cast<int64>(32768));
	TArray<ATSAVDMXFixture*> Grid;
	if (TestTrue(TEXT("Grid with independent patches"),TSAVRigExchange::PlaceGrid(World,A,nullptr,0,FIntPoint(2,2),FVector2D(100,200),FTransform(FVector(0,0,500)),8,505,Grid,Error))) {
		Actors.Append(Grid); TestEqual(TEXT("Grid fixture count"),Grid.Num(),4);
		TestEqual(TEXT("Last seven-channel slot fits"),Grid[0]->GetFixturePatch()->GetStartingChannel(),505);
		TestEqual(TEXT("Following fixture rolls to next universe"),Grid[1]->GetFixturePatch()->GetUniverseID(),9);
		TestTrue(TEXT("Row and column positions"),Grid[3]->GetActorLocation().Equals(FVector(100,200,500)));
		TestTrue(TEXT("Patches owned independently"),Grid[0]->GetFixturePatch()!=Grid[1]->GetFixturePatch());
	}
	TArray<ATSAVDMXFixture*> Rejected;
	TestFalse(TEXT("Occupied patch rejects whole new grid"),TSAVRigExchange::PlaceGrid(World,A,nullptr,0,FIntPoint(2,1),FVector2D(100,100),FTransform::Identity,4,100,Rejected,Error));
	TestTrue(TEXT("Failed grid creates no fixtures"),Rejected.IsEmpty());
	const FGuid Id=ATSAVLightingShow::FixtureId(A);
	if (!TestTrue(TEXT("MVR fixture rig export"),TSAVRigExchange::ExportMVR(Path,{A},Error))) { AddError(Error); return false; }
	FTSAVRigDocument Document;
	if (!TestTrue(TEXT("MVR fixture rig import preflight"),TSAVRigExchange::ReadMVR(Path,Document,Error))) { AddError(Error); return false; }
	TestEqual(TEXT("Persistent MVR identity"),Document.Fixtures[0].Id,Id);
	TestTrue(TEXT("MVR preserves placement"),Document.Fixtures[0].Transform.Equals(Transform,0.0001));
	TestEqual(TEXT("MVR universe"),Document.Fixtures[0].Universe,4); TestEqual(TEXT("MVR address"),Document.Fixtures[0].Address,100);
	TestFalse(TEXT("Duplicate rig import is rejected"),TSAVRigExchange::ImportRig(World,Document,Rejected,Error));
	A->Destroy();
	TArray<ATSAVDMXFixture*> Imported;
	if (TestTrue(TEXT("MVR rig instantiation"),TSAVRigExchange::ImportRig(World,Document,Imported,Error))) {
		Actors.Append(Imported); TestEqual(TEXT("Imported fixture identity"),ATSAVLightingShow::FixtureId(Imported[0]),Id);
		TestTrue(TEXT("GDTF physical pan range applied"),FMath::IsNearlyEqual(Imported[0]->PanMaxDegrees,270.0f));
		TestTrue(TEXT("Embedded GDTF retained with project state"),Imported[0]->CaptureTSAVState().Contains(TEXT("ExchangeGDTFBase64")));
		TestTrue(TEXT("Imported fixture emits native light"),Imported[0]->GetBeamLightComponent()->Intensity>0);
	}
	TestTrue(TEXT("Shared rig panel constructs"),MakeTSAVRigPanel(World,FSimpleDelegate())->GetChildren()->Num()>0);
	return true;
}
#endif
