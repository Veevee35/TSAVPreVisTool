// Copyright TSAV. All Rights Reserved.
#include "TSAVDMXFixture.h"
#include "Components/SceneComponent.h"
#include "Components/SpotLightComponent.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Misc/DefaultValueHelper.h"

FIntPoint ATSAVDMXFixture::GetMatrixDimensions() const
{
	const auto* Patch=GetFixturePatch(); const auto* Mode=Patch ? Patch->GetActiveMode() : nullptr;
	if (!Mode || !Mode->bFixtureMatrixEnabled) return FIntPoint::ZeroValue;
	const auto& Matrix=Mode->FixtureMatrixConfig;
	if (Matrix.XCells<1 || Matrix.YCells<1 || static_cast<int64>(Matrix.XCells)*Matrix.YCells>512) return FIntPoint::ZeroValue;
	return FIntPoint(Matrix.XCells,Matrix.YCells);
}
FName ATSAVDMXFixture::MatrixAttributeKey(FIntPoint Cell, FName Attribute)
{
	return FName(*FString::Printf(TEXT("cell[%d,%d].%s"),Cell.X,Cell.Y,*CanonicalizeAttribute(Attribute)));
}
bool ATSAVDMXFixture::ParseMatrixAttributeKey(FName Key, FIntPoint& Cell, FName& Attribute)
{
	FString Text=Key.ToString();
	if (!Text.RemoveFromStart(TEXT("cell["),ESearchCase::IgnoreCase)) return false;
	FString Coordinate, Name, X, Y;
	if (!Text.Split(TEXT("]."),&Coordinate,&Name) || Name.IsEmpty() || !Coordinate.Split(TEXT(","),&X,&Y)
		|| !FDefaultValueHelper::ParseInt(X,Cell.X) || !FDefaultValueHelper::ParseInt(Y,Cell.Y) || Cell.X<0 || Cell.Y<0) return false;
	Attribute=FName(*CanonicalizeAttribute(FName(*Name))); return !Attribute.IsNone();
}
bool ATSAVDMXFixture::SetMatrixCellAttributes(FIntPoint Cell,const TMap<FName,float>& Attributes)
{
	const FIntPoint Dimensions=GetMatrixDimensions();
	if (Cell.X<0 || Cell.Y<0 || Cell.X>=Dimensions.X || Cell.Y>=Dimensions.Y) return false;
	FDMXNormalizedAttributeValueMap Values;
	for (const auto& Pair : Attributes) if (!Pair.Key.IsNone() && FMath::IsFinite(Pair.Value)) Values.Map.Add(FDMXAttributeName(MatrixAttributeKey(Cell,Pair.Key)),Pair.Value);
	ApplyAttributeValues(Values); return true;
}
USpotLightComponent* ATSAVDMXFixture::GetMatrixCellLight(FIntPoint Cell) const
{
	const FIntPoint Dimensions=GetMatrixDimensions(); const int32 Index=Cell.Y*Dimensions.X+Cell.X;
	return Cell.X>=0 && Cell.X<Dimensions.X && Cell.Y>=0 && Cell.Y<Dimensions.Y && MatrixLights.IsValidIndex(Index) ? MatrixLights[Index].Get() : nullptr;
}
bool ATSAVDMXFixture::ApplyMatrixOptics()
{
	const FIntPoint Dimensions=GetMatrixDimensions(); const int32 Count=Dimensions.X*Dimensions.Y;
	for (const auto& Light : MatrixLights) if (Light) Light->SetVisibility(false);
	if (Count<1) return false;
	while (MatrixLights.Num()<Count) {
		auto* Light=NewObject<USpotLightComponent>(this,NAME_None,RF_Transient);
		Light->SetMobility(EComponentMobility::Movable); Light->SetupAttachment(LensRoot); Light->RegisterComponent(); MatrixLights.Add(Light);
	}
	const auto& Attributes=GetFixturePatch()->GetActiveMode()->FixtureMatrixConfig.CellAttributes;
	const auto CellValue=[&](FIntPoint Cell,FName Attribute,float Default) {
		if (const float* Value=LastAttributeValues.Map.Find(FDMXAttributeName(MatrixAttributeKey(Cell,Attribute)))) return *Value;
		for (const auto& Entry : Attributes) if (CanonicalizeAttribute(Entry.Attribute.Name)==CanonicalizeAttribute(Attribute)) {
			const uint64 Max=Entry.GetNumChannels()>=4 ? MAX_uint32 : (1ULL<<(8*Entry.GetNumChannels()))-1;
			return FMath::Clamp(static_cast<float>(static_cast<double>(Entry.DefaultValue)/Max),0.0f,1.0f);
		}
		return Default;
	};
	for (int32 Y=0; Y<Dimensions.Y; ++Y) for (int32 X=0; X<Dimensions.X; ++X) {
		const FIntPoint Cell(X,Y); auto* Light=MatrixLights[Y*Dimensions.X+X].Get();
		Light->SetVisibility(true); Light->SetRelativeLocation(FVector(0,(X-(Dimensions.X-1)*0.5f)*CellSpacingCm.X,((Dimensions.Y-1)*0.5f-Y)*CellSpacingCm.Y));
		Light->SetRelativeRotation(FRotator::ZeroRotator);
		Light->SetIntensity(BeamLight->Intensity*CellValue(Cell,TEXT("Dimmer"),1)/Count);
		Light->SetLightColor(BeamLight->GetLightColor()*FLinearColor(CellValue(Cell,TEXT("Red"),1),CellValue(Cell,TEXT("Green"),1),CellValue(Cell,TEXT("Blue"),1)));
		Light->SetAttenuationRadius(AttenuationRadiusCm); Light->SetOuterConeAngle(BeamLight->OuterConeAngle); Light->SetInnerConeAngle(BeamLight->InnerConeAngle);
		Light->SetCastShadows(Optics.bCastShadows); Light->SetUseInverseSquaredFalloff(true); Light->SetVolumetricScatteringIntensity(BeamLight->VolumetricScatteringIntensity);
		Light->SetLightFunctionMaterial(BeamLight->LightFunctionMaterial);
	}
	BeamLight->SetIntensity(0); return true;
}
