// Copyright TSAV. All Rights Reserved.
#include "TSAVFixtureOptics.h"
#include "TSAVDMXFixture.h"
#include "Components/SceneComponent.h"
#include "Components/SpotLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

float ATSAVDMXFixture::OpticsAttribute(FName Name, float Default) const
{
	const FString Key=CanonicalizeAttribute(Name);
	for (const auto& Pair : LastAttributeValues.Map) {
		FString Candidate=CanonicalizeAttribute(Pair.Key.Name);
		if (Candidate==TEXT("color1")) Candidate=TEXT("colorwheel");
		if (Candidate==TEXT("gobo1")) Candidate=TEXT("gobo");
		if (Candidate==TEXT("gobo1pos")) Candidate=TEXT("goborotation");
		if (Candidate==TEXT("shutter1")) Candidate=TEXT("shutter");
		if (Candidate==TEXT("frost1")) Candidate=TEXT("frost");
		if (Candidate==TEXT("coloursubc") || Candidate==TEXT("colorsubc")) Candidate=TEXT("cyan");
		if (Candidate==TEXT("colorsubm")) Candidate=TEXT("magenta");
		if (Candidate==TEXT("colorsuby")) Candidate=TEXT("yellow");
		if (Candidate==Key) return Pair.Value;
	}
	return Default;
}

void ATSAVDMXFixture::SetOpticsTime(float Seconds)
{
	if (!FMath::IsFinite(Seconds)) return;
	OpticsTime=FMath::Max(0.0f,Seconds); ApplyMotionAndBeam(0,false);
}
int32 ATSAVDMXFixture::GetActivePrismBeamCount() const
{
	int32 Count=0; for (const auto& Light : PrismLights) if (Light && Light->IsVisible()) ++Count; return Count;
}

void ATSAVDMXFixture::ApplyOptics(float DeltaSeconds, float OuterAngle)
{
	OpticsTime+=FMath::Max(0.0f,DeltaSeconds);
	for (const auto& Light : PrismLights) if (Light) Light->SetVisibility(false);
	if (!Optics.bEnabled) { BeamLight->SetLightFunctionMaterial(nullptr); ApplyMatrixOptics(); return; }
	FLinearColor Color=TargetColor;
	Color *= FLinearColor(1-OpticsAttribute(TEXT("Cyan"),0),1-OpticsAttribute(TEXT("Magenta"),0),1-OpticsAttribute(TEXT("Yellow"),0));
	const float White=OpticsAttribute(TEXT("ColorAdd_W"),0), Amber=OpticsAttribute(TEXT("ColorAdd_A"),0);
	Color.R=FMath::Clamp(Color.R+White+Amber,0.0f,1.0f); Color.G=FMath::Clamp(Color.G+White+Amber*0.45f,0.0f,1.0f); Color.B=FMath::Clamp(Color.B+White,0.0f,1.0f);
	const float CTO=OpticsAttribute(TEXT("CTO"),0);
	if (CTO>0) Color *= FLinearColor::MakeFromColorTemperature(FMath::Lerp(6500.0f,2700.0f,CTO));
	if (!Optics.ColorWheel.IsEmpty()) Color *= Optics.ColorWheel[FMath::Min(Optics.ColorWheel.Num()-1,FMath::FloorToInt(OpticsAttribute(TEXT("ColorWheel"),0)*Optics.ColorWheel.Num()))];
	float Gate=OpticsAttribute(TEXT("Shutter"),1);
	const float Strobe=OpticsAttribute(TEXT("Strobe"),0);
	if (Strobe>0) Gate *= FMath::Frac(OpticsTime*FMath::Lerp(1.0f,FMath::Clamp(Optics.MaximumStrobeHz,1.0f,30.0f),Strobe))<0.5f ? 1.0f : 0.0f;
	const float Intensity=FMath::Max(0.0f,MaximumIntensityLumens)*TargetDimmer*Gate;
	const float Frost=OpticsAttribute(TEXT("Frost"),0);
	OuterAngle=FMath::Clamp(FMath::Lerp(OuterAngle,FMath::Max(OuterAngle,MaximumBeamAngleDegrees),Frost),1.0f,89.0f);
	const float Iris=OpticsAttribute(TEXT("Iris"),1);
	const bool bProjected=Iris<1 || OpticsAttribute(TEXT("Gobo"),0)>0 || OpticsAttribute(TEXT("BladeTop"),0)>0 || OpticsAttribute(TEXT("BladeBottom"),0)>0 || OpticsAttribute(TEXT("BladeLeft"),0)>0 || OpticsAttribute(TEXT("BladeRight"),0)>0;
	// UE 5.8's stochastic path can omit this dynamic mask in scene captures.
	// Use conventional lighting for projected optics so the viewport and camera feeds agree.
	if (BeamLight->bAllowMegaLights==bProjected) { BeamLight->bAllowMegaLights=!bProjected; BeamLight->MarkRenderStateDirty(); }
	if (bProjected && !OpticalMaterial) if (auto* Source=Optics.GoboMaterial.LoadSynchronous()) OpticalMaterial=UMaterialInstanceDynamic::Create(Source,this);
	if (OpticalMaterial) {
		OpticalMaterial->SetScalarParameterValue(TEXT("Iris"),Iris);
		OpticalMaterial->SetScalarParameterValue(TEXT("Frost"),Frost);
		OpticalMaterial->SetScalarParameterValue(TEXT("GoboIndex"),FMath::FloorToFloat(OpticsAttribute(TEXT("Gobo"),0)*4.99f));
		OpticalMaterial->SetScalarParameterValue(TEXT("GoboAngle"),OpticsAttribute(TEXT("GoboRotation"),0)*2*PI + (OpticsAttribute(TEXT("GoboSpin"),0.5f)-0.5f)*2*PI*OpticsTime);
		for (FName Name : {TEXT("BladeTop"),TEXT("BladeBottom"),TEXT("BladeLeft"),TEXT("BladeRight")}) OpticalMaterial->SetScalarParameterValue(Name,OpticsAttribute(Name,0));
	}
	BeamLight->SetLightFunctionMaterial(bProjected ? OpticalMaterial.Get() : nullptr);
	if (!OpticalMaterial) OuterAngle=FMath::Max(1.0f,OuterAngle*Iris);
	BeamLight->SetOuterConeAngle(OuterAngle);
	BeamLight->SetInnerConeAngle(OuterAngle*FMath::Lerp(0.72f,0.15f,Frost));
	BeamLight->SetIntensity(Iris<=0 ? 0 : Intensity);
	BeamLight->SetLightColor(Color.GetClamped());
	BeamLight->SetCastShadows(Optics.bCastShadows);
	BeamLight->SetVolumetricScatteringIntensity(FMath::Clamp(Optics.VolumetricScattering,0.0f,10.0f));
	if (ApplyMatrixOptics()) return;
	const float Prism=OpticsAttribute(TEXT("Prism"),0);
	if (Prism<=0) return;
	const int32 Count=FMath::Clamp(Optics.PrismFacets,1,8);
	while (PrismLights.Num()<Count) {
		auto* Light=NewObject<USpotLightComponent>(this,NAME_None,RF_Transient);
		Light->SetMobility(EComponentMobility::Movable); Light->SetupAttachment(LensRoot); Light->RegisterComponent(); PrismLights.Add(Light);
	}
	const float Separation=FMath::Clamp(Optics.PrismSeparationDegrees,0.0f,30.0f)*Prism;
	const float Rotation=OpticsAttribute(TEXT("PrismRotation"),0)*2*PI;
	for (int32 I=0; I<Count; ++I) {
		auto* Light=PrismLights[I].Get(); const float Angle=Rotation+I*2*PI/Count;
		if (Light->bAllowMegaLights==bProjected) { Light->bAllowMegaLights=!bProjected; Light->MarkRenderStateDirty(); }
		Light->SetVisibility(true); Light->SetRelativeRotation(FRotator(FMath::Sin(Angle)*Separation,FMath::Cos(Angle)*Separation,0));
		Light->SetIntensity(BeamLight->Intensity/Count); Light->SetLightColor(Color.GetClamped()); Light->SetAttenuationRadius(AttenuationRadiusCm);
		Light->SetOuterConeAngle(OuterAngle); Light->SetInnerConeAngle(BeamLight->InnerConeAngle); Light->SetUseInverseSquaredFalloff(true);
		Light->SetCastShadows(Optics.bCastShadows); Light->SetVolumetricScatteringIntensity(BeamLight->VolumetricScatteringIntensity);
		Light->SetLightFunctionMaterial(bProjected ? OpticalMaterial.Get() : nullptr);
	}
	BeamLight->SetIntensity(0);
}
