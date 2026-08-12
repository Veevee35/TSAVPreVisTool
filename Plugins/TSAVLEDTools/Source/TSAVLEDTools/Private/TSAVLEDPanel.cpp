// Copyright TSAV. All Rights Reserved.

#include "TSAVLEDPanel.h"

#include "Components/StaticMeshComponent.h"
#include "TSAVLEDPanelDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVLEDPanel)

ATSAVLEDPanel::ATSAVLEDPanel()
{
	Backing = CreateGeometryComponent(TEXT("Cabinet Backing"), true);
	TopBezel = CreateGeometryComponent(TEXT("Top Bezel"), false);
	BottomBezel = CreateGeometryComponent(TEXT("Bottom Bezel"), false);
	LeftBezel = CreateGeometryComponent(TEXT("Left Bezel"), false);
	RightBezel = CreateGeometryComponent(TEXT("Right Bezel"), false);
}

void ATSAVLEDPanel::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateGeometry();
}

void ATSAVLEDPanel::UpdateGeometry()
{
	const float EffectiveWidth = GetEffectiveWidthCm();
	const float EffectiveHeight = GetEffectiveHeightCm();
	const float EffectiveDepth = GetEffectiveDepthCm();
	const float EffectiveBezel = FMath::Clamp(GetEffectiveBezelCm(), 0.0f, 0.45f * FMath::Min(EffectiveWidth, EffectiveHeight));

	const float ScreenDepth = 0.4f;
	const float FrontX = EffectiveDepth * 0.5f;
	const float ScreenWidth = FMath::Max(1.0f, EffectiveWidth - 2.0f * EffectiveBezel);
	const float ScreenHeight = FMath::Max(1.0f, EffectiveHeight - 2.0f * EffectiveBezel);
	const float BezelDepth = 0.8f;

	SetBox(Backing, FVector(EffectiveDepth, EffectiveWidth, EffectiveHeight), FVector::ZeroVector);
	SetBox(DisplaySurface, FVector(ScreenDepth, ScreenWidth, ScreenHeight), FVector(FrontX + ScreenDepth * 0.5f, 0.0f, 0.0f));

	SetBox(TopBezel, FVector(BezelDepth, EffectiveWidth, EffectiveBezel), FVector(FrontX + BezelDepth * 0.5f, 0.0f, (EffectiveHeight - EffectiveBezel) * 0.5f));
	SetBox(BottomBezel, FVector(BezelDepth, EffectiveWidth, EffectiveBezel), FVector(FrontX + BezelDepth * 0.5f, 0.0f, -(EffectiveHeight - EffectiveBezel) * 0.5f));
	SetBox(LeftBezel, FVector(BezelDepth, EffectiveBezel, ScreenHeight), FVector(FrontX + BezelDepth * 0.5f, -(EffectiveWidth - EffectiveBezel) * 0.5f, 0.0f));
	SetBox(RightBezel, FVector(BezelDepth, EffectiveBezel, ScreenHeight), FVector(FrontX + BezelDepth * 0.5f, (EffectiveWidth - EffectiveBezel) * 0.5f, 0.0f));

	ApplyFrameMaterial(Backing);
	ApplyFrameMaterial(TopBezel);
	ApplyFrameMaterial(BottomBezel);
	ApplyFrameMaterial(LeftBezel);
	ApplyFrameMaterial(RightBezel);
}

FIntPoint ATSAVLEDPanel::GetNativePixelResolution() const
{
	if (bUsePanelDefinition && PanelDefinition)
	{
		return PanelDefinition->GetResolution();
	}

	return FIntPoint(FMath::Max(ResolutionX, 1), FMath::Max(ResolutionY, 1));
}

FVector2D ATSAVLEDPanel::GetPixelPitchMm() const
{
	const FIntPoint NativeResolution = GetNativePixelResolution();
	return FVector2D(
		GetEffectiveWidthCm() * 10.0f / NativeResolution.X,
		GetEffectiveHeightCm() * 10.0f / NativeResolution.Y);
}

float ATSAVLEDPanel::GetEffectiveWidthCm() const
{
	return FMath::Max(bUsePanelDefinition && PanelDefinition ? PanelDefinition->WidthCm : WidthCm, 10.0f);
}

float ATSAVLEDPanel::GetEffectiveHeightCm() const
{
	return FMath::Max(bUsePanelDefinition && PanelDefinition ? PanelDefinition->HeightCm : HeightCm, 10.0f);
}

float ATSAVLEDPanel::GetEffectiveDepthCm() const
{
	return FMath::Max(bUsePanelDefinition && PanelDefinition ? PanelDefinition->DepthCm : DepthCm, 1.0f);
}

float ATSAVLEDPanel::GetEffectiveBezelCm() const
{
	return FMath::Max(bUsePanelDefinition && PanelDefinition ? PanelDefinition->BezelCm : BezelCm, 0.0f);
}

void ATSAVLEDPanel::SetBox(UStaticMeshComponent* Component, const FVector& SizeCm, const FVector& LocationCm) const
{
	if (!Component)
	{
		return;
	}

	Component->SetRelativeLocation(LocationCm);
	Component->SetRelativeRotation(FRotator::ZeroRotator);
	Component->SetRelativeScale3D(SizeCm / 100.0f);
}
