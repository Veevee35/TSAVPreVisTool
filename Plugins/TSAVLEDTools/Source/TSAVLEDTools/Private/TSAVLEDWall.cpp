// Copyright TSAV. All Rights Reserved.

#include "TSAVLEDWall.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "TSAVLEDPanelDefinition.h"
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
	return SafeColumns * GetEffectivePanelWidthCm() + FMath::Max(SafeColumns - 1, 0) * FMath::Max(PanelGapCm, 0.0f) + 2.0f * GetEffectiveBorderCm();
}

float ATSAVLEDWall::GetWallHeightCm() const
{
	const int32 SafeRows = FMath::Clamp(Rows, 1, 64);
	return SafeRows * GetEffectivePanelHeightCm() + FMath::Max(SafeRows - 1, 0) * FMath::Max(PanelGapCm, 0.0f) + 2.0f * GetEffectiveBorderCm();
}

FIntPoint ATSAVLEDWall::GetWallResolutionPixels() const
{
	return GetNativePixelResolution();
}

FVector2D ATSAVLEDWall::GetPanelPixelPitchMm() const
{
	const FIntPoint PanelResolution = GetEffectivePanelResolution();
	return FVector2D(
		GetEffectivePanelWidthCm() * 10.0f / PanelResolution.X,
		GetEffectivePanelHeightCm() * 10.0f / PanelResolution.Y);
}

void ATSAVLEDWall::RebuildPanelLayout()
{
	RerunConstructionScripts();
}

void ATSAVLEDWall::UpdateGeometry()
{
	Columns = FMath::Clamp(Columns, 1, 64);
	Rows = FMath::Clamp(Rows, 1, 64);
	PanelGapCm = FMath::Max(PanelGapCm, 0.0f);
	const float EffectivePanelWidth = GetEffectivePanelWidthCm();
	const float EffectivePanelHeight = GetEffectivePanelHeightCm();
	const float EffectiveDepth = GetEffectivePanelDepthCm();
	const float EffectiveBorder = GetEffectiveBorderCm();

	const float DisplayWidth = Columns * EffectivePanelWidth + (Columns - 1) * PanelGapCm;
	const float DisplayHeight = Rows * EffectivePanelHeight + (Rows - 1) * PanelGapCm;
	const float OuterWidth = DisplayWidth + 2.0f * EffectiveBorder;
	const float OuterHeight = DisplayHeight + 2.0f * EffectiveBorder;
	const float ScreenDepth = 0.4f;
	const float FrontX = EffectiveDepth * 0.5f;
	const float TrimDepth = 1.0f;

	SetBox(Backing, FVector(EffectiveDepth, OuterWidth, OuterHeight), FVector::ZeroVector);
	SetBox(DisplaySurface, FVector(ScreenDepth, DisplayWidth, DisplayHeight), FVector(FrontX + ScreenDepth * 0.5f, 0.0f, 0.0f));
	SetBox(TopBorder, FVector(TrimDepth, OuterWidth, EffectiveBorder), FVector(FrontX + TrimDepth * 0.5f, 0.0f, (DisplayHeight + EffectiveBorder) * 0.5f));
	SetBox(BottomBorder, FVector(TrimDepth, OuterWidth, EffectiveBorder), FVector(FrontX + TrimDepth * 0.5f, 0.0f, -(DisplayHeight + EffectiveBorder) * 0.5f));
	SetBox(LeftBorder, FVector(TrimDepth, EffectiveBorder, DisplayHeight), FVector(FrontX + TrimDepth * 0.5f, -(DisplayWidth + EffectiveBorder) * 0.5f, 0.0f));
	SetBox(RightBorder, FVector(TrimDepth, EffectiveBorder, DisplayHeight), FVector(FrontX + TrimDepth * 0.5f, (DisplayWidth + EffectiveBorder) * 0.5f, 0.0f));

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
		UpdatePanelLinks();
		return;
	}

	const float SeamDepth = 0.3f;
	const float SeamX = FrontX + ScreenDepth + SeamDepth * 0.5f;
	const float SeamWidth = FMath::Max(PanelGapCm, 0.25f);
	const float LeftEdge = -DisplayWidth * 0.5f;
	const float BottomEdge = -DisplayHeight * 0.5f;

	for (int32 Column = 1; Column < Columns; ++Column)
	{
		const float Y = LeftEdge + Column * EffectivePanelWidth + (Column - 0.5f) * PanelGapCm;
		PanelSeams->AddInstance(FTransform(FRotator::ZeroRotator, FVector(SeamX, Y, 0.0f), FVector(SeamDepth, SeamWidth, DisplayHeight) / 100.0f));
	}

	for (int32 Row = 1; Row < Rows; ++Row)
	{
		const float Z = BottomEdge + Row * EffectivePanelHeight + (Row - 0.5f) * PanelGapCm;
		PanelSeams->AddInstance(FTransform(FRotator::ZeroRotator, FVector(SeamX, 0.0f, Z), FVector(SeamDepth, DisplayWidth, SeamWidth) / 100.0f));
	}

	UpdatePanelLinks();
}

FIntPoint ATSAVLEDWall::GetNativePixelResolution() const
{
	const FIntPoint PanelResolution = GetEffectivePanelResolution();
	return FIntPoint(
		FMath::Clamp(Columns, 1, 64) * PanelResolution.X,
		FMath::Clamp(Rows, 1, 64) * PanelResolution.Y);
}

void ATSAVLEDWall::UpdatePanelLinks()
{
	PanelLinks.Reset(Columns * Rows);
	const FIntPoint PanelResolution = GetEffectivePanelResolution();
	int32 LinkIndex = 1;

	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 PositionInRow = 0; PositionInRow < Columns; ++PositionInRow)
		{
			const bool bReverseRow = LinkPattern == ETSAVLEDLinkPattern::RowsSerpentine && (Row % 2) == 1;
			const int32 Column = bReverseRow ? Columns - PositionInRow - 1 : PositionInRow;

			FTSAVLEDPanelLink& Link = PanelLinks.AddDefaulted_GetRef();
			Link.LinkIndex = LinkIndex++;
			Link.GridPosition = FIntPoint(Column, Row);
			Link.CanvasPixelPosition = CanvasPosition + FIntPoint(Column * PanelResolution.X, Row * PanelResolution.Y);
			Link.CabinetResolution = PanelResolution;
		}
	}
}

float ATSAVLEDWall::GetEffectivePanelWidthCm() const
{
	return FMath::Max(bUsePanelDefinition && PanelDefinition ? PanelDefinition->WidthCm : PanelWidthCm, 10.0f);
}

float ATSAVLEDWall::GetEffectivePanelHeightCm() const
{
	return FMath::Max(bUsePanelDefinition && PanelDefinition ? PanelDefinition->HeightCm : PanelHeightCm, 10.0f);
}

float ATSAVLEDWall::GetEffectivePanelDepthCm() const
{
	return FMath::Max(bUsePanelDefinition && PanelDefinition ? PanelDefinition->DepthCm : WallDepthCm, 1.0f);
}

float ATSAVLEDWall::GetEffectiveBorderCm() const
{
	if (bUsePanelDefinition && PanelDefinition)
	{
		return FMath::Max(PanelDefinition->BezelCm, 0.0f);
	}

	return FMath::Max(BorderCm, 0.0f);
}

FIntPoint ATSAVLEDWall::GetEffectivePanelResolution() const
{
	if (bUsePanelDefinition && PanelDefinition)
	{
		return PanelDefinition->GetResolution();
	}

	return FIntPoint(FMath::Max(PanelResolutionX, 1), FMath::Max(PanelResolutionY, 1));
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
