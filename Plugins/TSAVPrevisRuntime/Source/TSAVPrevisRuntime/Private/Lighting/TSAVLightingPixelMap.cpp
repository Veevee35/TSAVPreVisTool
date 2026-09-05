// Copyright TSAV. All Rights Reserved.
#include "Lighting/TSAVLightingShow.h"
#include "TSAVDMXFixture.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "HAL/FileManager.h"

bool ATSAVLightingShow::ApplyPixelMap(const TArray<ATSAVDMXFixture*>& Fixtures,int32 Columns,const FString& Path,FString& Error)
{
	if (Fixtures.IsEmpty() || Columns<1 || Columns>512 || IFileManager::Get().FileSize(*Path)>64*1024*1024) { Error=TEXT("Select fixtures, a grid width, and an image up to 64 MB."); return false; }
	for (auto* Fixture : Fixtures) if (!IsValid(Fixture) || Fixture->GetWorld()!=GetWorld()) { Error=TEXT("The selection contains an unavailable fixture."); return false; }
	FImage Image;
	if (!FImageUtils::LoadImage(*Path,Image) || Image.SizeX<1 || Image.SizeY<1 || static_cast<int64>(Image.SizeX)*Image.SizeY>16*1024*1024) { Error=TEXT("Choose a readable image with at most 16 million pixels."); return false; }
	const bool bSingleMatrix=Fixtures.Num()==1 && Fixtures[0]->GetMatrixDimensions().X>0;
	const FIntPoint Grid=bSingleMatrix?Fixtures[0]->GetMatrixDimensions():FIntPoint(Columns,FMath::DivideAndRoundUp(Fixtures.Num(),Columns));
	const auto Sample=[&](int32 X,int32 Y) { return Image.GetOnePixelLinear(FMath::Clamp(FMath::FloorToInt((X+0.5)*Image.SizeX/Grid.X),0,Image.SizeX-1),FMath::Clamp(FMath::FloorToInt((Y+0.5)*Image.SizeY/Grid.Y),0,Image.SizeY-1)).GetClamped(); };
	for (int32 I=0; I<Fixtures.Num(); ++I) {
		auto* Fixture=Fixtures[I]; Touch(Fixture); const FGuid Id=FixtureId(Fixture); auto& Look=Programmer.FindOrAdd(Id); Look.FixtureId=Id;
		if (bSingleMatrix) {
			Look.Attributes.Add(TEXT("red"),1); Look.Attributes.Add(TEXT("green"),1); Look.Attributes.Add(TEXT("blue"),1); Look.Attributes.Add(TEXT("dimmer"),1);
			for (int32 Y=0; Y<Grid.Y; ++Y) for (int32 X=0; X<Grid.X; ++X) { const auto Color=Sample(X,Y); const FIntPoint Cell(X,Y);
				Look.Attributes.Add(ATSAVDMXFixture::MatrixAttributeKey(Cell,TEXT("red")),Color.R); Look.Attributes.Add(ATSAVDMXFixture::MatrixAttributeKey(Cell,TEXT("green")),Color.G);
				Look.Attributes.Add(ATSAVDMXFixture::MatrixAttributeKey(Cell,TEXT("blue")),Color.B); Look.Attributes.Add(ATSAVDMXFixture::MatrixAttributeKey(Cell,TEXT("dimmer")),Color.A);
			}
		} else { const auto Color=Sample(I%Grid.X,I/Grid.X); Look.Attributes.Add(TEXT("red"),Color.R); Look.Attributes.Add(TEXT("green"),Color.G); Look.Attributes.Add(TEXT("blue"),Color.B); Look.Attributes.Add(TEXT("dimmer"),Color.A); }
	}
	Flush(); Error.Reset(); return true;
}
