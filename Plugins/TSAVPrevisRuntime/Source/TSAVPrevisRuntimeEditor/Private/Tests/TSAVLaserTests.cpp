// Copyright TSAV. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "Laser/TSAVLaserPreview.h"
#include "UI/TSAVLaserWidget.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "Widgets/SWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTSAVLaserTest,"TSAV.NativeLaser.ILDA",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTSAVLaserTest::RunTest(const FString&)
{
	UWorld* World=GEditor->GetEditorWorldContext().World();
	if (!FApp::IsUnattended() || !World || World->GetOutermost()->GetName()!=TEXT("/Engine/Maps/Entry")) return false;
	TArray<FTSAVLaserFrame> Original=TSAVILDA::MakePattern(2,2,8);
	Original[0].Name=TEXT("Test2D"); Original[0].Projector=3;
	Original[1].Name=TEXT("Test3D"); Original[1].Projector=7; Original[1].b3D=true;
	Original[1].Points[1].Position=FVector(-1,1,-0.25);
	Original[0].Points[3].bBlank=true; Original[0].Points[3].Color=FColor(10,20,30);
	FString Error;
	for (bool bIndexed : {false,true}) {
		TArray<uint8> Bytes; TArray<FTSAVLaserFrame> Read;
		if (!TestTrue(TEXT("ILDA encode succeeds"),TSAVILDA::Encode(Original,Bytes,Error,bIndexed))) return false;
		if (!TestTrue(TEXT("ILDA decode succeeds"),TSAVILDA::Decode(Bytes,Read,Error))) return false;
		TestEqual(TEXT("Both projectors preserved"),Read.Num(),2);
		for (int32 F=0; F<Read.Num(); ++F) {
			TestEqual(TEXT("Projector ID"),Read[F].Projector,Original[F].Projector);
			TestEqual(TEXT("Frame name"),Read[F].Name,Original[F].Name);
			TestEqual(TEXT("Dimensionality"),Read[F].b3D,Original[F].b3D);
			TestEqual(TEXT("Point count"),Read[F].Points.Num(),Original[F].Points.Num());
			for (int32 P=0; P<Read[F].Points.Num(); ++P) {
				TestTrue(TEXT("Signed big endian coordinates preserved"),Read[F].Points[P].Position.Equals(Original[F].Points[P].Position,0.00004));
				TestEqual(TEXT("RGB preserved including palette changes"),Read[F].Points[P].Color,Original[F].Points[P].Color);
				TestEqual(TEXT("Blanking preserved independently of color"),Read[F].Points[P].bBlank,Original[F].Points[P].bBlank);
			}
		}
		// Corruption must leave the caller's existing frames untouched.
		Bytes.SetNum(Bytes.Num()-10); const auto Before=Read[0].Points;
		TestFalse(TEXT("Truncated EOF rejected"),TSAVILDA::Decode(Bytes,Read,Error));
		TestEqual(TEXT("Failed import preserves existing data"),Read[0].Points.Num(),Before.Num());
	}
	// Hand-built format 1 record: one red default-palette point at negative X,
	// positive Y. Reserved bytes deliberately nonzero, as readers must ignore them.
	TArray<uint8> Fixture; Fixture.AddZeroed(70);
	FMemory::Memcpy(Fixture.GetData(),"ILDA",4); Fixture[4]=123; Fixture[7]=1; Fixture[25]=1;
	Fixture[32]=0x80; Fixture[34]=0x7f; Fixture[35]=0xff; Fixture[36]=0x80;
	FMemory::Memcpy(Fixture.GetData()+38,"ILDA",4); Fixture[45]=1;
	TArray<FTSAVLaserFrame> Known;
	if (TestTrue(TEXT("Independent indexed fixture accepted"),TSAVILDA::Decode(Fixture,Known,Error))) {
		TestTrue(TEXT("Coordinate endpoints decode exactly"),Known[0].Points[0].Position.Equals(FVector(-1,1,0)));
		TestEqual(TEXT("Default palette index zero is red"),Known[0].Points[0].Color,FColor::Red);
	}
	Fixture[7]=3; TestFalse(TEXT("Unsupported format rejected"),TSAVILDA::Decode(Fixture,Known,Error));
	auto* A=World->SpawnActor<ATSAVLaserPreview>(); auto* B=World->SpawnActor<ATSAVLaserPreview>();
	const FString Path=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("NativeLightingReview/LaserRoundTrip.ild")));
	ON_SCOPE_EXIT { A->Destroy(); B->Destroy(); IFileManager::Get().Delete(*Path); };
	FTSAVLaserData Data; Data.Frames=Original; Data.Projector=3; Data.bShowRays=false;
	TestTrue(TEXT("Preview configuration"),A->Configure(Data));
	TestEqual(TEXT("Blank moves do not draw segments"),A->GetSegmentCount(),7);
	TestTrue(TEXT("Original laser material exists"),Data.Material.LoadSynchronous()!=nullptr);
	TestTrue(TEXT("Project state round trip"),B->RestoreTSAVState(A->CaptureTSAVState()));
	TestEqual(TEXT("Preview restored"),B->GetSegmentCount(),A->GetSegmentCount());
	TestTrue(TEXT("ILDA file export"),A->ExportILDA(Path,Error));
	TestTrue(TEXT("ILDA file import"),B->ImportILDA(Path,Error));
	TestEqual(TEXT("File projector retained"),B->GetData().Projector,3);
	const FString Before=B->CaptureTSAVState(); auto Bad=Data; Bad.Frames[0].Points[0].Position.X=2;
	TestFalse(TEXT("Out of range points rejected"),B->Configure(Bad)); TestEqual(TEXT("Bad data preserves preview"),B->CaptureTSAVState(),Before);
	Data.Frames=TSAVILDA::MakePattern(0,2,8); Data.Projector=0; Data.bLoop=false;
	A->Configure(Data); A->Play(); A->Seek(100);
	TestFalse(TEXT("Non-looping preview stops at last frame"),A->IsPlaying());
	TestTrue(TEXT("Preview endpoint time"),FMath::IsNearlyEqual(A->GetTime(),1.0f/30));
	TestTrue(TEXT("Shared laser panel constructs"),MakeTSAVLaserPanel(World,FSimpleDelegate())->GetChildren()->Num()>0);
	return true;
}
#endif
