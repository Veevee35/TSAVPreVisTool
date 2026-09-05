// Copyright TSAV. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "Scene/TSAVSceneGenerator.h"
#include "UI/TSAVSceneBuilderWidget.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTSAVSceneGeneratorTest,"TSAV.NativeScene.Generators",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTSAVSceneGeneratorTest::RunTest(const FString&)
{
	UWorld* World=GEditor->GetEditorWorldContext().World();
	if (!FApp::IsUnattended() || !World || World->GetOutermost()->GetName()!=TEXT("/Engine/Maps/Entry")) return false;
	auto* A=World->SpawnActor<ATSAVSceneGenerator>(); auto* B=World->SpawnActor<ATSAVSceneGenerator>();
	ON_SCOPE_EXIT { A->Destroy(); B->Destroy(); };
	for (int32 I=0; I<=static_cast<int32>(ETSAVScenicKind::Rail); ++I) {
		const auto Kind=static_cast<ETSAVScenicKind>(I); const FString Name=StaticEnum<ETSAVScenicKind>()->GetNameStringByValue(I);
		TestTrue(Name+TEXT(" preset configures"),A->Configure(ATSAVSceneGenerator::Preset(Kind)));
		TestTrue(Name+TEXT(" generates real geometry"),A->GetPartCount()>0);
		TestTrue(Name+TEXT(" state restores"),B->RestoreTSAVState(A->CaptureTSAVState()));
		TestEqual(Name+TEXT(" restored part count"),B->GetPartCount(),A->GetPartCount());
		TestEqual(Name+TEXT(" round-trip configuration"),B->CaptureTSAVState(),A->CaptureTSAVState());
	}
	A->Configure(ATSAVSceneGenerator::Preset(ETSAVScenicKind::Stage)); TestEqual(TEXT("Deck includes four supporting legs"),A->GetPartCount(),5);
	A->Configure(ATSAVSceneGenerator::Preset(ETSAVScenicKind::Lift)); A->SetMotionPosition(0.75f);
	TestTrue(TEXT("Lift moves by its configured travel"),A->GetMotionOffset().Equals(FVector(0,0,300)));
	A->Configure(ATSAVSceneGenerator::Preset(ETSAVScenicKind::Rail)); A->SetMotionPosition(1);
	TestTrue(TEXT("Rail reaches its positive endpoint"),A->GetMotionOffset().Equals(FVector(500,0,0)));
	const FString Before=A->CaptureTSAVState(); auto Bad=A->Settings; Bad.Columns=100; Bad.Rows=100;
	TestFalse(TEXT("Oversized generator rejected before mutation"),A->Configure(Bad)); TestEqual(TEXT("Invalid configuration preserves object"),A->CaptureTSAVState(),Before);
	TestTrue(TEXT("Shared scene builder constructs"),MakeTSAVSceneBuilder(World,FSimpleDelegate())->GetChildren()->Num()>0);
	return true;
}
#endif
