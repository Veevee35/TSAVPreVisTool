// Copyright TSAV. All Rights Reserved.

#include "TSAVLEDPanel.h"

#include "Components/StaticMeshComponent.h"

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
	WidthCm = FMath::Max(WidthCm, 10.0f);
	HeightCm = FMath::Max(HeightCm, 10.0f);
	DepthCm = FMath::Max(DepthCm, 1.0f);
	BezelCm = FMath::Clamp(BezelCm, 0.0f, 0.45f * FMath::Min(WidthCm, HeightCm));

	const float ScreenDepth = 0.4f;
	const float FrontX = DepthCm * 0.5f;
	const float ScreenWidth = FMath::Max(1.0f, WidthCm - 2.0f * BezelCm);
	const float ScreenHeight = FMath::Max(1.0f, HeightCm - 2.0f * BezelCm);
	const float BezelDepth = 0.8f;

	SetBox(Backing, FVector(DepthCm, WidthCm, HeightCm), FVector::ZeroVector);
	SetBox(DisplaySurface, FVector(ScreenDepth, ScreenWidth, ScreenHeight), FVector(FrontX + ScreenDepth * 0.5f, 0.0f, 0.0f));

	SetBox(TopBezel, FVector(BezelDepth, WidthCm, BezelCm), FVector(FrontX + BezelDepth * 0.5f, 0.0f, (HeightCm - BezelCm) * 0.5f));
	SetBox(BottomBezel, FVector(BezelDepth, WidthCm, BezelCm), FVector(FrontX + BezelDepth * 0.5f, 0.0f, -(HeightCm - BezelCm) * 0.5f));
	SetBox(LeftBezel, FVector(BezelDepth, BezelCm, ScreenHeight), FVector(FrontX + BezelDepth * 0.5f, -(WidthCm - BezelCm) * 0.5f, 0.0f));
	SetBox(RightBezel, FVector(BezelDepth, BezelCm, ScreenHeight), FVector(FrontX + BezelDepth * 0.5f, (WidthCm - BezelCm) * 0.5f, 0.0f));

	ApplyFrameMaterial(Backing);
	ApplyFrameMaterial(TopBezel);
	ApplyFrameMaterial(BottomBezel);
	ApplyFrameMaterial(LeftBezel);
	ApplyFrameMaterial(RightBezel);
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
