// Copyright TSAV. All Rights Reserved.

#include "TSAVLEDWall.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "TSAVLEDPanelDefinition.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVLEDWall)

namespace TSAVLEDWall::Private
{
	constexpr float EdgeCutFraction = 0.28f;
	constexpr int32 RoundCornerSegments = 6;

	struct FMeshBuffers
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;
		TArray<FProcMeshTangent> Tangents;

		int32 AddVertex(const FVector& Position, const FVector& Normal, const FVector2D& UV, const FVector& Tangent)
		{
			const int32 Index = Vertices.Add(Position);
			Normals.Add(Normal);
			UVs.Add(UV);
			Colors.Add(FLinearColor::White);
			Tangents.Emplace(Tangent, false);
			return Index;
		}

		void AddTriangle(int32 A, int32 B, int32 C)
		{
			Triangles.Append({A, B, C});
		}
	};

	float SanitizeAngle(float Angle)
	{
		return FMath::Clamp(FMath::RoundToFloat(Angle * 2.0f) * 0.5f, -15.0f, 15.0f);
	}

	void AddArc(TArray<FVector2D>& Points, const FVector2D& Center, float StartDegrees, float EndDegrees, bool bIncludeEnd)
	{
		const int32 LastStep = bIncludeEnd ? RoundCornerSegments : RoundCornerSegments - 1;
		for (int32 Step = 1; Step <= LastStep; ++Step)
		{
			const float Alpha = static_cast<float>(Step) / RoundCornerSegments;
			const float Angle = FMath::DegreesToRadians(FMath::Lerp(StartDegrees, EndDegrees, Alpha));
			Points.Emplace(Center.X + EdgeCutFraction * FMath::Cos(Angle), Center.Y + EdgeCutFraction * FMath::Sin(Angle));
		}
	}

	TArray<FVector2D> MakePanelPolygon(ETSAVLEDPanelEdgeStyle Style)
	{
		const float Cut = EdgeCutFraction;
		TArray<FVector2D> Points;
		switch (Style)
		{
		case ETSAVLEDPanelEdgeStyle::DiagonalTopLeft:
			return {{Cut, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, Cut}};
		case ETSAVLEDPanelEdgeStyle::DiagonalTopRight:
			return {{0.0f, 0.0f}, {1.0f - Cut, 0.0f}, {1.0f, Cut}, {1.0f, 1.0f}, {0.0f, 1.0f}};
		case ETSAVLEDPanelEdgeStyle::DiagonalBottomLeft:
			return {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {Cut, 1.0f}, {0.0f, 1.0f - Cut}};
		case ETSAVLEDPanelEdgeStyle::DiagonalBottomRight:
			return {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f - Cut}, {1.0f - Cut, 1.0f}, {0.0f, 1.0f}};
		case ETSAVLEDPanelEdgeStyle::RoundTopLeft:
			Points = {{Cut, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, Cut}};
			AddArc(Points, FVector2D(Cut, Cut), 180.0f, 270.0f, false);
			return Points;
		case ETSAVLEDPanelEdgeStyle::RoundTopRight:
			Points = {{0.0f, 0.0f}, {1.0f - Cut, 0.0f}};
			AddArc(Points, FVector2D(1.0f - Cut, Cut), -90.0f, 0.0f, true);
			Points.Append({{1.0f, 1.0f}, {0.0f, 1.0f}});
			return Points;
		case ETSAVLEDPanelEdgeStyle::RoundBottomLeft:
			Points = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {Cut, 1.0f}};
			AddArc(Points, FVector2D(Cut, 1.0f - Cut), 90.0f, 180.0f, true);
			return Points;
		case ETSAVLEDPanelEdgeStyle::RoundBottomRight:
			Points = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f - Cut}};
			AddArc(Points, FVector2D(1.0f - Cut, 1.0f - Cut), 0.0f, 90.0f, true);
			Points.Add(FVector2D(0.0f, 1.0f));
			return Points;
		default:
			return {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
		}
	}

	FVector ToWorldPoint(
		const FVector2D& Point,
		const FVector& Center,
		const FVector& Normal,
		const FVector& Horizontal,
		float Width,
		float Height,
		float DepthOffset)
	{
		return Center + Normal * DepthOffset + Horizontal * ((Point.X - 0.5f) * Width) + FVector::UpVector * ((0.5f - Point.Y) * Height);
	}

	void AppendFrontFace(
		FMeshBuffers& Mesh,
		const TArray<FVector2D>& Polygon,
		const FVector& Center,
		const FVector& Normal,
		const FVector& Horizontal,
		float Width,
		float Height,
		float FrontDepth,
		int32 Column,
		int32 Row,
		int32 Columns,
		int32 Rows)
	{
		const int32 BaseIndex = Mesh.Vertices.Num();
		for (const FVector2D& Point : Polygon)
		{
			const FVector2D UV((Column + Point.X) / Columns, (Row + Point.Y) / Rows);
			Mesh.AddVertex(ToWorldPoint(Point, Center, Normal, Horizontal, Width, Height, FrontDepth), Normal, UV, Horizontal);
		}
		for (int32 Index = 1; Index + 1 < Polygon.Num(); ++Index)
		{
			Mesh.AddTriangle(BaseIndex, BaseIndex + Index + 1, BaseIndex + Index);
		}
	}

	void AppendCabinetBody(
		FMeshBuffers& Mesh,
		const TArray<FVector2D>& Polygon,
		const FVector& Center,
		const FVector& Normal,
		const FVector& Horizontal,
		float Width,
		float Height,
		float FrontDepth,
		float BackDepth,
		bool bIncludeFront)
	{
		const int32 BackBase = Mesh.Vertices.Num();
		for (const FVector2D& Point : Polygon)
		{
			Mesh.AddVertex(ToWorldPoint(Point, Center, Normal, Horizontal, Width, Height, BackDepth), -Normal, Point, -Horizontal);
		}
		for (int32 Index = 1; Index + 1 < Polygon.Num(); ++Index)
		{
			Mesh.AddTriangle(BackBase, BackBase + Index, BackBase + Index + 1);
		}

		if (bIncludeFront)
		{
			const int32 FrontBase = Mesh.Vertices.Num();
			for (const FVector2D& Point : Polygon)
			{
				Mesh.AddVertex(ToWorldPoint(Point, Center, Normal, Horizontal, Width, Height, FrontDepth), Normal, Point, Horizontal);
			}
			for (int32 Index = 1; Index + 1 < Polygon.Num(); ++Index)
			{
				Mesh.AddTriangle(FrontBase, FrontBase + Index + 1, FrontBase + Index);
			}
		}

		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			const FVector2D A = Polygon[Index];
			const FVector2D B = Polygon[(Index + 1) % Polygon.Num()];
			const FVector FrontA = ToWorldPoint(A, Center, Normal, Horizontal, Width, Height, FrontDepth);
			const FVector FrontB = ToWorldPoint(B, Center, Normal, Horizontal, Width, Height, FrontDepth);
			const FVector BackA = ToWorldPoint(A, Center, Normal, Horizontal, Width, Height, BackDepth);
			const FVector BackB = ToWorldPoint(B, Center, Normal, Horizontal, Width, Height, BackDepth);
			const FVector SideNormal = FVector::CrossProduct(FrontB - FrontA, BackA - FrontA).GetSafeNormal();
			const FVector SideTangent = (FrontB - FrontA).GetSafeNormal();
			const int32 SideBase = Mesh.Vertices.Num();
			Mesh.AddVertex(FrontA, SideNormal, FVector2D(0.0f, 0.0f), SideTangent);
			Mesh.AddVertex(FrontB, SideNormal, FVector2D(1.0f, 0.0f), SideTangent);
			Mesh.AddVertex(BackB, SideNormal, FVector2D(1.0f, 1.0f), SideTangent);
			Mesh.AddVertex(BackA, SideNormal, FVector2D(0.0f, 1.0f), SideTangent);
			Mesh.AddTriangle(SideBase, SideBase + 1, SideBase + 2);
			Mesh.AddTriangle(SideBase, SideBase + 2, SideBase + 3);
		}
	}
}

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

	ShapedWallMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Shaped Wall Mesh"));
	ShapedWallMesh->SetupAttachment(SceneRoot);
	ShapedWallMesh->SetMobility(EComponentMobility::Movable);
	ShapedWallMesh->SetGenerateOverlapEvents(false);
	ShapedWallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ShapedWallMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

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

float ATSAVLEDWall::GetColumnAngleDegrees(int32 Column) const
{
	return ColumnAnglesDegrees.IsValidIndex(Column) ? TSAVLEDWall::Private::SanitizeAngle(ColumnAnglesDegrees[Column]) : 0.0f;
}

ETSAVLEDPanelEdgeStyle ATSAVLEDWall::GetPanelEdgeStyle(int32 Column, int32 Row) const
{
	const int32 Index = Row * FMath::Clamp(Columns, 1, 64) + Column;
	return PanelEdgeStyles.IsValidIndex(Index) ? PanelEdgeStyles[Index] : ETSAVLEDPanelEdgeStyle::Square;
}

bool ATSAVLEDWall::IsPanelEnabled(int32 Column, int32 Row) const
{
	return Column >= 0 && Column < FMath::Clamp(Columns, 1, 64) &&
		Row >= 0 && Row < FMath::Clamp(Rows, 1, 64) &&
		GetPanelEdgeStyle(Column, Row) != ETSAVLEDPanelEdgeStyle::Disabled;
}

void ATSAVLEDWall::RebuildPanelLayout()
{
	RerunConstructionScripts();
}

void ATSAVLEDWall::OnDisplayMaterialUpdated(UMaterialInterface* AppliedMaterial)
{
	if (ShapedWallMesh)
	{
		ShapedWallMesh->SetMaterial(0, AppliedMaterial);
	}
}

void ATSAVLEDWall::UpdateGeometry()
{
	using namespace TSAVLEDWall::Private;

	Columns = FMath::Clamp(Columns, 1, 64);
	Rows = FMath::Clamp(Rows, 1, 64);
	PanelGapCm = FMath::Max(PanelGapCm, 0.0f);
	NormalizeShapeSettings();
	const float EffectivePanelWidth = GetEffectivePanelWidthCm();
	const float EffectivePanelHeight = GetEffectivePanelHeightCm();
	const float EffectiveDepth = GetEffectivePanelDepthCm();
	const float EffectiveBorder = GetEffectiveBorderCm();

	const float DisplayWidth = Columns * EffectivePanelWidth + (Columns - 1) * PanelGapCm;
	const float DisplayHeight = Rows * EffectivePanelHeight + (Rows - 1) * PanelGapCm;
	DisplaySurface->SetVisibility(false);
	DisplaySurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	for (UStaticMeshComponent* LegacyComponent : {Backing.Get(), TopBorder.Get(), BottomBorder.Get(), LeftBorder.Get(), RightBorder.Get()})
	{
		LegacyComponent->SetVisibility(false);
		LegacyComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	PanelSeams->ClearInstances();
	PanelSeams->SetVisibility(false);

	TArray<FVector> ColumnCenters;
	TArray<FVector> ColumnNormals;
	TArray<FVector> ColumnHorizontals;
	ColumnCenters.Reserve(Columns);
	ColumnNormals.Reserve(Columns);
	ColumnHorizontals.Reserve(Columns);
	FVector Cursor = FVector::ZeroVector;
	for (int32 Column = 0; Column < Columns; ++Column)
	{
		const float AngleRadians = FMath::DegreesToRadians(GetColumnAngleDegrees(Column));
		const FVector Normal(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f);
		const FVector Horizontal(-FMath::Sin(AngleRadians), FMath::Cos(AngleRadians), 0.0f);
		const FVector Center = Cursor + Horizontal * (EffectivePanelWidth * 0.5f);
		ColumnCenters.Add(Center);
		ColumnNormals.Add(Normal);
		ColumnHorizontals.Add(Horizontal);
		Cursor = Center + Horizontal * (EffectivePanelWidth * 0.5f + (Column + 1 < Columns ? PanelGapCm : 0.0f));
	}
	const FVector LayoutOffset = Cursor * 0.5f;
	for (FVector& Center : ColumnCenters)
	{
		Center -= LayoutOffset;
	}

	FMeshBuffers DisplayMesh;
	FMeshBuffers FrameMesh;
	const float FrontDepth = EffectiveDepth * 0.5f + 0.2f;
	const float BackDepth = -EffectiveDepth * 0.5f;
	const TArray<FVector2D> Rectangle = MakePanelPolygon(ETSAVLEDPanelEdgeStyle::Square);
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		const float Z = DisplayHeight * 0.5f - EffectivePanelHeight * 0.5f - Row * (EffectivePanelHeight + PanelGapCm);
		for (int32 Column = 0; Column < Columns; ++Column)
		{
			if (!IsPanelEnabled(Column, Row))
			{
				continue;
			}

			const FVector Center = ColumnCenters[Column] + FVector::UpVector * Z;
			const TArray<FVector2D> Polygon = MakePanelPolygon(GetPanelEdgeStyle(Column, Row));
			AppendFrontFace(DisplayMesh, Polygon, Center, ColumnNormals[Column], ColumnHorizontals[Column], EffectivePanelWidth, EffectivePanelHeight, FrontDepth, Column, Row, Columns, Rows);
			AppendCabinetBody(FrameMesh, Polygon, Center, ColumnNormals[Column], ColumnHorizontals[Column], EffectivePanelWidth, EffectivePanelHeight, FrontDepth, BackDepth, false);

			if (EffectiveBorder > 0.0f)
			{
				if (!IsPanelEnabled(Column, Row - 1))
				{
					const FVector TopCenter = Center + FVector::UpVector * ((EffectivePanelHeight + EffectiveBorder) * 0.5f);
					AppendCabinetBody(FrameMesh, Rectangle, TopCenter, ColumnNormals[Column], ColumnHorizontals[Column], EffectivePanelWidth, EffectiveBorder, FrontDepth, BackDepth, true);
				}
				if (!IsPanelEnabled(Column, Row + 1))
				{
					const FVector BottomCenter = Center - FVector::UpVector * ((EffectivePanelHeight + EffectiveBorder) * 0.5f);
					AppendCabinetBody(FrameMesh, Rectangle, BottomCenter, ColumnNormals[Column], ColumnHorizontals[Column], EffectivePanelWidth, EffectiveBorder, FrontDepth, BackDepth, true);
				}
				if (!IsPanelEnabled(Column - 1, Row))
				{
					const FVector LeftCenter = Center - ColumnHorizontals[Column] * ((EffectivePanelWidth + EffectiveBorder) * 0.5f);
					AppendCabinetBody(FrameMesh, Rectangle, LeftCenter, ColumnNormals[Column], ColumnHorizontals[Column], EffectiveBorder, EffectivePanelHeight, FrontDepth, BackDepth, true);
				}
				if (!IsPanelEnabled(Column + 1, Row))
				{
					const FVector RightCenter = Center + ColumnHorizontals[Column] * ((EffectivePanelWidth + EffectiveBorder) * 0.5f);
					AppendCabinetBody(FrameMesh, Rectangle, RightCenter, ColumnNormals[Column], ColumnHorizontals[Column], EffectiveBorder, EffectivePanelHeight, FrontDepth, BackDepth, true);
				}
			}
		}
	}

	ShapedWallMesh->ClearAllMeshSections();
	if (!DisplayMesh.Vertices.IsEmpty())
	{
		ShapedWallMesh->CreateMeshSection_LinearColor(0, DisplayMesh.Vertices, DisplayMesh.Triangles, DisplayMesh.Normals, DisplayMesh.UVs, DisplayMesh.Colors, DisplayMesh.Tangents, false);
	}
	if (!FrameMesh.Vertices.IsEmpty())
	{
		ShapedWallMesh->CreateMeshSection_LinearColor(1, FrameMesh.Vertices, FrameMesh.Triangles, FrameMesh.Normals, FrameMesh.UVs, FrameMesh.Colors, FrameMesh.Tangents, true);
	}
	ShapedWallMesh->SetMaterial(0, DisplayMaterialInstance ? DisplayMaterialInstance.Get() : DisplaySurface->GetMaterial(0));
	ShapedWallMesh->SetMaterial(1, ResolveFrameMaterial());
	ShapedWallMesh->SetVisibility(true);

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
			if (!IsPanelEnabled(Column, Row))
			{
				continue;
			}

			FTSAVLEDPanelLink& Link = PanelLinks.AddDefaulted_GetRef();
			Link.LinkIndex = LinkIndex++;
			Link.GridPosition = FIntPoint(Column, Row);
			Link.CanvasPixelPosition = CanvasPosition + FIntPoint(Column * PanelResolution.X, Row * PanelResolution.Y);
			Link.CabinetResolution = PanelResolution;
			Link.ColumnAngleDegrees = GetColumnAngleDegrees(Column);
			Link.EdgeStyle = GetPanelEdgeStyle(Column, Row);
		}
	}
}

void ATSAVLEDWall::NormalizeShapeSettings()
{
	ColumnAnglesDegrees.SetNum(Columns);
	for (float& Angle : ColumnAnglesDegrees)
	{
		Angle = TSAVLEDWall::Private::SanitizeAngle(Angle);
	}

	PanelEdgeStyles.SetNum(Columns * Rows);
	for (ETSAVLEDPanelEdgeStyle& Style : PanelEdgeStyles)
	{
		if (static_cast<uint8>(Style) > static_cast<uint8>(ETSAVLEDPanelEdgeStyle::Disabled))
		{
			Style = ETSAVLEDPanelEdgeStyle::Square;
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
