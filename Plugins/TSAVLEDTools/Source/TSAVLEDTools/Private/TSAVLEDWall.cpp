// Copyright TSAV. All Rights Reserved.

#include "TSAVLEDWall.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVLEDWall)

ATSAVLEDWall::ATSAVLEDWall()
{
	Backing = CreateGeometryComponent(TEXT("Wall Backing"), true);
	TopBorder = CreateGeometryComponent(TEXT("Top Border"), false);
	BottomBorder = CreateGeometryComponent(TEXT("Bottom Border"), false);
	LeftBorder = CreateGeometryComponent(TEXT("Left Border"), false);
	RightBorder = CreateGeometryComponent(TEXT("Right Border"), false);

	PanelSeams = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Panel Seams"));
	PanelSeams->SetupAttachment(SceneRoot);
	PanelSeams->SetMobility(EComponentMobility::Movable);
	PanelSeams->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PanelSeams->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PanelSeams->SetStaticMesh(CubeMesh.Object);
	}
}

void ATSAVLEDWall::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateGeometry();
}

float ATSAVLEDWall::GetWallWidthCm() const
{
	const int32 SafeColumns = FMath::Clamp(Columns, 1, 64);
	return SafeColumns * FMath::Max(PanelWidthCm, 10.0f) + FMath::Max(SafeColumns - 1, 0) * FMath::Max(PanelGapCm, 0.0f) + 2.0f * FMath::Max(BorderCm, 0.0f);
}

float ATSAVLEDWall::GetWallHeightCm() const
{
	const int32 SafeRows = FMath::Clamp(Rows, 1, 64);
	return SafeRows * FMath::Max(PanelHeightCm, 10.0f) + FMath::Max(SafeRows - 1, 0) * FMath::Max(PanelGapCm, 0.0f) + 2.0f * FMath::Max(BorderCm, 0.0f);
}

void ATSAVLEDWall::UpdateGeometry()
{
	Columns = FMath::Clamp(Columns, 1, 64);
	Rows = FMath::Clamp(Rows, 1, 64);
	PanelWidthCm = FMath::Max(PanelWidthCm, 10.0f);
	PanelHeightCm = FMath::Max(PanelHeightCm, 10.0f);
	PanelGapCm = FMath::Max(PanelGapCm, 0.0f);
	WallDepthCm = FMath::Max(WallDepthCm, 1.0f);
	BorderCm = FMath::Max(BorderCm, 0.0f);

	const float DisplayWidth = Columns * PanelWidthCm + (Columns - 1) * PanelGapCm;
	const float DisplayHeight = Rows * PanelHeightCm + (Rows - 1) * PanelGapCm;
	const float OuterWidth = DisplayWidth + 2.0f * BorderCm;
	const float OuterHeight = DisplayHeight + 2.0f * BorderCm;
	const float ScreenDepth = 0.4f;
	const float FrontX = WallDepthCm * 0.5f;
	const float TrimDepth = 1.0f;

	SetBox(Backing, FVector(WallDepthCm, OuterWidth, OuterHeight), FVector::ZeroVector);
	SetBox(DisplaySurface, FVector(ScreenDepth, DisplayWidth, DisplayHeight), FVector(FrontX + ScreenDepth * 0.5f, 0.0f, 0.0f));
	SetBox(TopBorder, FVector(TrimDepth, OuterWidth, BorderCm), FVector(FrontX + TrimDepth * 0.5f, 0.0f, (DisplayHeight + BorderCm) * 0.5f));
	SetBox(BottomBorder, FVector(TrimDepth, OuterWidth, BorderCm), FVector(FrontX + TrimDepth * 0.5f, 0.0f, -(DisplayHeight + BorderCm) * 0.5f));
	SetBox(LeftBorder, FVector(TrimDepth, BorderCm, DisplayHeight), FVector(FrontX + TrimDepth * 0.5f, -(DisplayWidth + BorderCm) * 0.5f, 0.0f));
	SetBox(RightBorder, FVector(TrimDepth, BorderCm, DisplayHeight), FVector(FrontX + TrimDepth * 0.5f, (DisplayWidth + BorderCm) * 0.5f, 0.0f));

	ApplyFrameMaterial(Backing);
	ApplyFrameMaterial(TopBorder);
	ApplyFrameMaterial(BottomBorder);
	ApplyFrameMaterial(LeftBorder);
	ApplyFrameMaterial(RightBorder);

	PanelSeams->ClearInstances();
	PanelSeams->SetVisibility(bShowPanelSeams);
	PanelSeams->SetMaterial(0, ResolveFrameMaterial());

	if (!bShowPanelSeams)
	{
		return;
	}

	const float SeamDepth = 0.3f;
	const float SeamX = FrontX + ScreenDepth + SeamDepth * 0.5f;
	const float SeamWidth = FMath::Max(PanelGapCm, 0.25f);
	const float LeftEdge = -DisplayWidth * 0.5f;
	const float BottomEdge = -DisplayHeight * 0.5f;

	for (int32 Column = 1; Column < Columns; ++Column)
	{
		const float Y = LeftEdge + Column * PanelWidthCm + (Column - 0.5f) * PanelGapCm;
		PanelSeams->AddInstance(FTransform(FRotator::ZeroRotator, FVector(SeamX, Y, 0.0f), FVector(SeamDepth, SeamWidth, DisplayHeight) / 100.0f));
	}

	for (int32 Row = 1; Row < Rows; ++Row)
	{
		const float Z = BottomEdge + Row * PanelHeightCm + (Row - 0.5f) * PanelGapCm;
		PanelSeams->AddInstance(FTransform(FRotator::ZeroRotator, FVector(SeamX, 0.0f, Z), FVector(SeamDepth, DisplayWidth, SeamWidth) / 100.0f));
	}
}

void ATSAVLEDWall::SetBox(UStaticMeshComponent* Component, const FVector& SizeCm, const FVector& LocationCm) const
{
	if (!Component)
	{
		return;
	}

	Component->SetRelativeLocation(LocationCm);
	Component->SetRelativeRotation(FRotator::ZeroRotator);
	Component->SetRelativeScale3D(SizeCm / 100.0f);
}
