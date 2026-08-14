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
	constexpr int32 RoundCornerSegments = 12;
	constexpr int32 InternalCurveSegmentsPerHalf = 12;
	constexpr double RadiusDecimalScale = 10000000000.0;

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
		return FMath::Clamp(FMath::RoundToFloat(Angle * 2.0f) * 0.5f, -90.0f, 90.0f);
	}

	double SanitizeRoundRadiusMeters(double RadiusMeters)
	{
		return FMath::IsFinite(RadiusMeters)
			? FMath::Max(FMath::RoundToDouble(RadiusMeters * RadiusDecimalScale) / RadiusDecimalScale, 0.5)
			: 0.5;
	}

	double SanitizeSignedRadiusMeters(double RadiusMeters)
	{
		return FMath::IsFinite(RadiusMeters)
			? FMath::RoundToDouble(RadiusMeters * RadiusDecimalScale) / RadiusDecimalScale
			: 0.0;
	}

	float Cross2D(const FVector2D& A, const FVector2D& B)
	{
		return A.X * B.Y - A.Y * B.X;
	}

	void AddCornerSpanningArc(
		TArray<FVector2D>& Points,
		const FVector2D& Start,
		const FVector2D& End,
		const FVector2D& OppositeCorner,
		float WidthCm,
		float HeightCm,
		double RadiusMeters)
	{
		auto ToPhysical = [WidthCm, HeightCm](const FVector2D& Point)
		{
			return FVector2D(Point.X * WidthCm, Point.Y * HeightCm);
		};
		const FVector2D PhysicalStart = ToPhysical(Start);
		const FVector2D PhysicalEnd = ToPhysical(End);
		const FVector2D Midpoint = (PhysicalStart + PhysicalEnd) * 0.5f;
		const FVector2D Chord = PhysicalEnd - PhysicalStart;
		const double HalfChord = Chord.Size() * 0.5;
		const double RadiusCm = FMath::Max(SanitizeRoundRadiusMeters(RadiusMeters) * 100.0, HalfChord + 0.01);
		FVector2D BulgeDirection = ToPhysical(OppositeCorner) - Midpoint;
		if (!BulgeDirection.Normalize())
		{
			BulgeDirection = FVector2D(-Chord.Y, Chord.X).GetSafeNormal();
		}
		const double CenterDistance = FMath::Sqrt(FMath::Max(RadiusCm * RadiusCm - HalfChord * HalfChord, 0.0));
		const FVector2D Center = Midpoint - BulgeDirection * CenterDistance;
		const FVector2D StartVector = PhysicalStart - Center;
		const FVector2D EndVector = PhysicalEnd - Center;
		float SweepRadians = FMath::Atan2(Cross2D(StartVector, EndVector), FVector2D::DotProduct(StartVector, EndVector));
		if (FMath::IsNearlyZero(SweepRadians))
		{
			SweepRadians = PI;
		}

		for (int32 Step = 1; Step < RoundCornerSegments; ++Step)
		{
			const float Angle = SweepRadians * static_cast<float>(Step) / RoundCornerSegments;
			const float CosAngle = FMath::Cos(Angle);
			const float SinAngle = FMath::Sin(Angle);
			const FVector2D Rotated(
				StartVector.X * CosAngle - StartVector.Y * SinAngle,
				StartVector.X * SinAngle + StartVector.Y * CosAngle);
			const FVector2D PhysicalPoint = Center + Rotated;
			Points.Emplace(PhysicalPoint.X / WidthCm, PhysicalPoint.Y / HeightCm);
		}
	}

	TArray<FVector2D> MakePanelPolygon(ETSAVLEDPanelEdgeStyle Style, float WidthCm, float HeightCm, double RadiusMeters)
	{
		const FVector2D TopLeft(0.0f, 0.0f);
		const FVector2D TopRight(1.0f, 0.0f);
		const FVector2D BottomRight(1.0f, 1.0f);
		const FVector2D BottomLeft(0.0f, 1.0f);
		TArray<FVector2D> Points;
		switch (Style)
		{
		case ETSAVLEDPanelEdgeStyle::DiagonalTopLeft:
			return {TopLeft, TopRight, BottomLeft};
		case ETSAVLEDPanelEdgeStyle::DiagonalTopRight:
			return {TopLeft, TopRight, BottomRight};
		case ETSAVLEDPanelEdgeStyle::DiagonalBottomLeft:
			return {TopLeft, BottomRight, BottomLeft};
		case ETSAVLEDPanelEdgeStyle::DiagonalBottomRight:
			return {TopRight, BottomRight, BottomLeft};
		case ETSAVLEDPanelEdgeStyle::RoundTopLeft:
			Points = {TopLeft, TopRight};
			AddCornerSpanningArc(Points, TopRight, BottomLeft, BottomRight, WidthCm, HeightCm, RadiusMeters);
			Points.Add(BottomLeft);
			return Points;
		case ETSAVLEDPanelEdgeStyle::RoundTopRight:
			Points = {TopLeft, TopRight, BottomRight};
			AddCornerSpanningArc(Points, BottomRight, TopLeft, BottomLeft, WidthCm, HeightCm, RadiusMeters);
			return Points;
		case ETSAVLEDPanelEdgeStyle::RoundBottomLeft:
			Points = {TopLeft};
			AddCornerSpanningArc(Points, TopLeft, BottomRight, TopRight, WidthCm, HeightCm, RadiusMeters);
			Points.Append({BottomRight, BottomLeft});
			return Points;
		case ETSAVLEDPanelEdgeStyle::RoundBottomRight:
			Points = {TopRight, BottomRight, BottomLeft};
			AddCornerSpanningArc(Points, BottomLeft, TopRight, TopLeft, WidthCm, HeightCm, RadiusMeters);
			return Points;
		default:
			return {TopLeft, TopRight, BottomRight, BottomLeft};
		}
	}

	FVector ToWorldPoint(
		const FVector2D& Point,
		const FVector& Center,
		const FVector& Normal,
		const FVector& Horizontal,
		const FVector& VerticalUp,
		float Width,
		float Height,
		float DepthOffset)
	{
		return Center + Normal * DepthOffset + Horizontal * ((Point.X - 0.5f) * Width) + VerticalUp * ((0.5f - Point.Y) * Height);
	}

	FVector MapPanelPoint(
		const FVector2D& Point,
		const FVector& TopLeft,
		const FVector& TopRight,
		const FVector& BottomRight,
		const FVector& BottomLeft)
	{
		const FVector Top = FMath::Lerp(TopLeft, TopRight, Point.X);
		const FVector Bottom = FMath::Lerp(BottomLeft, BottomRight, Point.X);
		return FMath::Lerp(Top, Bottom, Point.Y);
	}

	double GetInternalCurveOffsetCm(double NormalizedX, double PanelWidthCm, double RadiusAMeters, double RadiusBMeters)
	{
		const bool bFirstHalf = NormalizedX <= 0.5;
		const double RadiusMeters = bFirstHalf ? RadiusAMeters : RadiusBMeters;
		if (FMath::IsNearlyZero(RadiusMeters))
		{
			return 0.0;
		}

		const double LocalAlpha = bFirstHalf ? NormalizedX * 2.0 : (NormalizedX - 0.5) * 2.0;
		const double ChordLengthCm = PanelWidthCm * 0.5;
		const double HalfChordCm = ChordLengthCm * 0.5;
		const double RequestedRadiusCm = FMath::Abs(RadiusMeters) * 100.0;
		const double EffectiveRadiusCm = FMath::Max(RequestedRadiusCm, HalfChordCm + 0.0001);
		const double AlongChordCm = (LocalAlpha - 0.5) * ChordLengthCm;
		const double EndpointDistanceCm = FMath::Sqrt(FMath::Max(EffectiveRadiusCm * EffectiveRadiusCm - HalfChordCm * HalfChordCm, 0.0));
		const double ArcDistanceCm = FMath::Sqrt(FMath::Max(EffectiveRadiusCm * EffectiveRadiusCm - AlongChordCm * AlongChordCm, 0.0));
		return FMath::Sign(RadiusMeters) * (ArcDistanceCm - EndpointDistanceCm);
	}

	FVector MapCurvedPanelPoint(
		const FVector2D& Point,
		const FVector& TopLeft,
		const FVector& TopRight,
		const FVector& BottomRight,
		const FVector& BottomLeft,
		bool bInternalCurveEnabled,
		double RadiusAMeters,
		double RadiusBMeters,
		double TopCurveScale,
		double BottomCurveScale,
		float PanelWidthCm)
	{
		const FVector BasePoint = MapPanelPoint(Point, TopLeft, TopRight, BottomRight, BottomLeft);
		if (!bInternalCurveEnabled)
		{
			return BasePoint;
		}

		const FVector Horizontal = FMath::Lerp(TopRight - TopLeft, BottomRight - BottomLeft, Point.Y).GetSafeNormal();
		const FVector Down = FMath::Lerp(BottomLeft - TopLeft, BottomRight - TopRight, Point.X).GetSafeNormal();
		const FVector Normal = FVector::CrossProduct(Down, Horizontal).GetSafeNormal();
		const double CurveScale = FMath::Lerp(TopCurveScale, BottomCurveScale, Point.Y);
		return BasePoint + Normal * GetInternalCurveOffsetCm(Point.X, PanelWidthCm, RadiusAMeters, RadiusBMeters) * CurveScale;
	}

	TArray<FVector2D> ClipPolygonAtX(const TArray<FVector2D>& Input, double BoundaryX, bool bKeepGreater)
	{
		TArray<FVector2D> Output;
		if (Input.IsEmpty())
		{
			return Output;
		}

		auto IsInside = [BoundaryX, bKeepGreater](const FVector2D& Point)
		{
			return bKeepGreater ? Point.X >= BoundaryX - UE_DOUBLE_SMALL_NUMBER : Point.X <= BoundaryX + UE_DOUBLE_SMALL_NUMBER;
		};
		FVector2D Previous = Input.Last();
		bool bPreviousInside = IsInside(Previous);
		for (const FVector2D& Current : Input)
		{
			const bool bCurrentInside = IsInside(Current);
			if (bCurrentInside != bPreviousInside)
			{
				const double Denominator = Current.X - Previous.X;
				const double Alpha = FMath::IsNearlyZero(Denominator) ? 0.0 : (BoundaryX - Previous.X) / Denominator;
				Output.Add(FMath::Lerp(Previous, Current, Alpha));
			}
			if (bCurrentInside)
			{
				Output.Add(Current);
			}
			Previous = Current;
			bPreviousInside = bCurrentInside;
		}
		return Output;
	}

	TArray<FVector2D> ClipPolygonToXRange(const TArray<FVector2D>& Polygon, double MinimumX, double MaximumX)
	{
		return ClipPolygonAtX(ClipPolygonAtX(Polygon, MinimumX, true), MaximumX, false);
	}

	void AppendFrontFace(
		FMeshBuffers& Mesh,
		const TArray<FVector2D>& Polygon,
		const FVector& TopLeft,
		const FVector& TopRight,
		const FVector& BottomRight,
		const FVector& BottomLeft,
		int32 Column,
		int32 Row,
		int32 Columns,
		int32 Rows,
		bool bInternalCurveEnabled,
		double RadiusAMeters,
		double RadiusBMeters,
		double TopCurveScale,
		double BottomCurveScale,
		float PanelWidthCm)
	{
		auto AppendPolygon = [&](const TArray<FVector2D>& FacePolygon)
		{
			if (FacePolygon.Num() < 3)
			{
				return;
			}
			const int32 BaseIndex = Mesh.Vertices.Num();
			for (const FVector2D& Point : FacePolygon)
			{
				const FVector2D UV(1.0f - (Column + Point.X) / Columns, (Row + Point.Y) / Rows);
				const double SampleDistance = 0.0001;
				const FVector2D LeftSample(FMath::Max(Point.X - SampleDistance, 0.0), Point.Y);
				const FVector2D RightSample(FMath::Min(Point.X + SampleDistance, 1.0), Point.Y);
				const FVector2D TopSample(Point.X, FMath::Max(Point.Y - SampleDistance, 0.0));
				const FVector2D BottomSample(Point.X, FMath::Min(Point.Y + SampleDistance, 1.0));
				const FVector Left = MapCurvedPanelPoint(LeftSample, TopLeft, TopRight, BottomRight, BottomLeft, bInternalCurveEnabled, RadiusAMeters, RadiusBMeters, TopCurveScale, BottomCurveScale, PanelWidthCm);
				const FVector Right = MapCurvedPanelPoint(RightSample, TopLeft, TopRight, BottomRight, BottomLeft, bInternalCurveEnabled, RadiusAMeters, RadiusBMeters, TopCurveScale, BottomCurveScale, PanelWidthCm);
				const FVector Top = MapCurvedPanelPoint(TopSample, TopLeft, TopRight, BottomRight, BottomLeft, bInternalCurveEnabled, RadiusAMeters, RadiusBMeters, TopCurveScale, BottomCurveScale, PanelWidthCm);
				const FVector Bottom = MapCurvedPanelPoint(BottomSample, TopLeft, TopRight, BottomRight, BottomLeft, bInternalCurveEnabled, RadiusAMeters, RadiusBMeters, TopCurveScale, BottomCurveScale, PanelWidthCm);
				const FVector Horizontal = (Right - Left).GetSafeNormal();
				const FVector Down = (Bottom - Top).GetSafeNormal();
				const FVector Normal = FVector::CrossProduct(Down, Horizontal).GetSafeNormal();
				const FVector Position = MapCurvedPanelPoint(Point, TopLeft, TopRight, BottomRight, BottomLeft, bInternalCurveEnabled, RadiusAMeters, RadiusBMeters, TopCurveScale, BottomCurveScale, PanelWidthCm);
				Mesh.AddVertex(Position, Normal, UV, -Horizontal);
			}
			for (int32 Index = 1; Index + 1 < FacePolygon.Num(); ++Index)
			{
				Mesh.AddTriangle(BaseIndex, BaseIndex + Index, BaseIndex + Index + 1);
			}
		};

		if (!bInternalCurveEnabled || (FMath::IsNearlyZero(TopCurveScale) && FMath::IsNearlyZero(BottomCurveScale)) || (FMath::IsNearlyZero(RadiusAMeters) && FMath::IsNearlyZero(RadiusBMeters)))
		{
			AppendPolygon(Polygon);
			return;
		}

		const int32 SegmentCount = InternalCurveSegmentsPerHalf * 2;
		for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
		{
			const double MinimumX = static_cast<double>(Segment) / SegmentCount;
			const double MaximumX = static_cast<double>(Segment + 1) / SegmentCount;
			AppendPolygon(ClipPolygonToXRange(Polygon, MinimumX, MaximumX));
		}
	}

	float GetMaximumPolygonInset(const TArray<FVector2D>& Polygon, float Width, float Height)
	{
		if (Polygon.Num() < 3)
		{
			return 0.0f;
		}

		TArray<FVector2D> PhysicalPoints;
		PhysicalPoints.Reserve(Polygon.Num());
		FVector2D InteriorPoint = FVector2D::ZeroVector;
		float TwiceArea = 0.0f;
		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			const FVector2D Point(Polygon[Index].X * Width, Polygon[Index].Y * Height);
			const FVector2D Next(Polygon[(Index + 1) % Polygon.Num()].X * Width, Polygon[(Index + 1) % Polygon.Num()].Y * Height);
			PhysicalPoints.Add(Point);
			InteriorPoint += Point;
			TwiceArea += Cross2D(Point, Next);
		}
		InteriorPoint /= Polygon.Num();
		const float Orientation = TwiceArea >= 0.0f ? 1.0f : -1.0f;
		float MaximumInset = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < PhysicalPoints.Num(); ++Index)
		{
			const FVector2D Point = PhysicalPoints[Index];
			const FVector2D Edge = PhysicalPoints[(Index + 1) % PhysicalPoints.Num()] - Point;
			const FVector2D InwardNormal = FVector2D(-Edge.Y * Orientation, Edge.X * Orientation).GetSafeNormal();
			MaximumInset = FMath::Min(MaximumInset, FVector2D::DotProduct(InteriorPoint - Point, InwardNormal));
		}
		return FMath::Max(MaximumInset, 0.0f);
	}

	TArray<FVector2D> MakeInsetPolygon(const TArray<FVector2D>& Polygon, float Width, float Height, float Inset)
	{
		TArray<FVector2D> PhysicalPoints;
		PhysicalPoints.Reserve(Polygon.Num());
		float TwiceArea = 0.0f;
		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			const FVector2D Point(Polygon[Index].X * Width, Polygon[Index].Y * Height);
			const FVector2D Next(Polygon[(Index + 1) % Polygon.Num()].X * Width, Polygon[(Index + 1) % Polygon.Num()].Y * Height);
			PhysicalPoints.Add(Point);
			TwiceArea += Cross2D(Point, Next);
		}
		const float Orientation = TwiceArea >= 0.0f ? 1.0f : -1.0f;

		TArray<FVector2D> InsetPolygon;
		InsetPolygon.SetNumUninitialized(Polygon.Num());
		for (int32 Index = 0; Index < PhysicalPoints.Num(); ++Index)
		{
			const FVector2D Previous = PhysicalPoints[(Index + PhysicalPoints.Num() - 1) % PhysicalPoints.Num()];
			const FVector2D Point = PhysicalPoints[Index];
			const FVector2D Next = PhysicalPoints[(Index + 1) % PhysicalPoints.Num()];
			const FVector2D PreviousDirection = (Point - Previous).GetSafeNormal();
			const FVector2D NextDirection = (Next - Point).GetSafeNormal();
			const FVector2D PreviousNormal(-PreviousDirection.Y * Orientation, PreviousDirection.X * Orientation);
			const FVector2D NextNormal(-NextDirection.Y * Orientation, NextDirection.X * Orientation);
			const FVector2D PreviousLinePoint = Point + PreviousNormal * Inset;
			const FVector2D NextLinePoint = Point + NextNormal * Inset;
			const float Denominator = Cross2D(PreviousDirection, NextDirection);
			FVector2D InsetPoint;
			if (FMath::Abs(Denominator) > KINDA_SMALL_NUMBER)
			{
				const float PreviousDistance = Cross2D(NextLinePoint - PreviousLinePoint, NextDirection) / Denominator;
				InsetPoint = PreviousLinePoint + PreviousDirection * PreviousDistance;
			}
			else
			{
				FVector2D AverageNormal = PreviousNormal + NextNormal;
				if (!AverageNormal.Normalize())
				{
					AverageNormal = PreviousNormal;
				}
				InsetPoint = Point + AverageNormal * Inset;
			}
			InsetPolygon[Index] = FVector2D(InsetPoint.X / Width, InsetPoint.Y / Height);
		}
		return InsetPolygon;
	}

	void AppendChamferedCabinetBody(
		FMeshBuffers& Mesh,
		const TArray<FVector2D>& Polygon,
		const FVector& Center,
		const FVector& Normal,
		const FVector& Horizontal,
		const FVector& VerticalUp,
		float Width,
		float Height,
		float FrontDepth,
		float RequestedBackDepth)
	{
		// The rear outline is offset from every front edge by exactly the same
		// distance that it travels backward. This makes all cabinet sides true 45
		// degree faces, including internal panel edges and shaped boundaries.
		const float RequestedInset = FMath::Max(FrontDepth - RequestedBackDepth, 0.1f);
		const float MaximumInset = GetMaximumPolygonInset(Polygon, Width, Height);
		const float AppliedInset = FMath::Min(RequestedInset, MaximumInset * 0.9f);
		const float BackDepth = FrontDepth - AppliedInset;
		const TArray<FVector2D> BackPolygon = MakeInsetPolygon(Polygon, Width, Height, AppliedInset);
		const int32 BackBase = Mesh.Vertices.Num();
		for (const FVector2D& Point : BackPolygon)
		{
			Mesh.AddVertex(ToWorldPoint(Point, Center, Normal, Horizontal, VerticalUp, Width, Height, BackDepth), -Normal, Point, -Horizontal);
		}
		for (int32 Index = 1; Index + 1 < BackPolygon.Num(); ++Index)
		{
			Mesh.AddTriangle(BackBase, BackBase + Index + 1, BackBase + Index);
		}

		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			const FVector2D A = Polygon[Index];
			const FVector2D B = Polygon[(Index + 1) % Polygon.Num()];
			const FVector2D BackA2D = BackPolygon[Index];
			const FVector2D BackB2D = BackPolygon[(Index + 1) % BackPolygon.Num()];
			const FVector FrontA = ToWorldPoint(A, Center, Normal, Horizontal, VerticalUp, Width, Height, FrontDepth);
			const FVector FrontB = ToWorldPoint(B, Center, Normal, Horizontal, VerticalUp, Width, Height, FrontDepth);
			const FVector BackA = ToWorldPoint(BackA2D, Center, Normal, Horizontal, VerticalUp, Width, Height, BackDepth);
			const FVector BackB = ToWorldPoint(BackB2D, Center, Normal, Horizontal, VerticalUp, Width, Height, BackDepth);
			const FVector SideNormal = FVector::CrossProduct(FrontB - FrontA, BackA - FrontA).GetSafeNormal();
			const FVector SideTangent = (FrontB - FrontA).GetSafeNormal();
			const int32 SideBase = Mesh.Vertices.Num();
			Mesh.AddVertex(FrontA, SideNormal, FVector2D(0.0f, 0.0f), SideTangent);
			Mesh.AddVertex(FrontB, SideNormal, FVector2D(1.0f, 0.0f), SideTangent);
			Mesh.AddVertex(BackB, SideNormal, FVector2D(1.0f, 1.0f), SideTangent);
			Mesh.AddVertex(BackA, SideNormal, FVector2D(0.0f, 1.0f), SideTangent);
			Mesh.AddTriangle(SideBase, SideBase + 2, SideBase + 1);
			Mesh.AddTriangle(SideBase, SideBase + 3, SideBase + 2);
		}
	}

	void AppendChamferedEdge(
		FMeshBuffers& Mesh,
		const FVector& FrontA,
		const FVector& FrontB,
		const FVector& SurfaceNormal,
		const FVector& InwardDirection,
		float ChamferSize)
	{
		const FVector RearA = FrontA - SurfaceNormal * ChamferSize + InwardDirection * ChamferSize;
		const FVector RearB = FrontB - SurfaceNormal * ChamferSize + InwardDirection * ChamferSize;
		const FVector Tangent = (FrontB - FrontA).GetSafeNormal();
		const FVector FaceNormal = FVector::CrossProduct(RearB - FrontA, FrontB - FrontA).GetSafeNormal();
		const int32 BaseIndex = Mesh.Vertices.Num();
		Mesh.AddVertex(FrontA, FaceNormal, FVector2D(0.0f, 0.0f), Tangent);
		Mesh.AddVertex(FrontB, FaceNormal, FVector2D(1.0f, 0.0f), Tangent);
		Mesh.AddVertex(RearB, FaceNormal, FVector2D(1.0f, 1.0f), Tangent);
		Mesh.AddVertex(RearA, FaceNormal, FVector2D(0.0f, 1.0f), Tangent);
		Mesh.AddTriangle(BaseIndex, BaseIndex + 1, BaseIndex + 2);
		Mesh.AddTriangle(BaseIndex, BaseIndex + 2, BaseIndex + 3);
	}

	void AppendChamferedEdgeVariable(
		FMeshBuffers& Mesh,
		const FVector& FrontA,
		const FVector& FrontB,
		const FVector& SurfaceNormalA,
		const FVector& SurfaceNormalB,
		const FVector& InwardDirectionA,
		const FVector& InwardDirectionB,
		float ChamferSize)
	{
		const FVector RearA = FrontA - SurfaceNormalA * ChamferSize + InwardDirectionA * ChamferSize;
		const FVector RearB = FrontB - SurfaceNormalB * ChamferSize + InwardDirectionB * ChamferSize;
		const FVector Tangent = (FrontB - FrontA).GetSafeNormal();
		const FVector FaceNormal = FVector::CrossProduct(RearB - FrontA, FrontB - FrontA).GetSafeNormal();
		const int32 BaseIndex = Mesh.Vertices.Num();
		Mesh.AddVertex(FrontA, FaceNormal, FVector2D(0.0f, 0.0f), Tangent);
		Mesh.AddVertex(FrontB, FaceNormal, FVector2D(1.0f, 0.0f), Tangent);
		Mesh.AddVertex(RearB, FaceNormal, FVector2D(1.0f, 1.0f), Tangent);
		Mesh.AddVertex(RearA, FaceNormal, FVector2D(0.0f, 1.0f), Tangent);
		Mesh.AddTriangle(BaseIndex, BaseIndex + 1, BaseIndex + 2);
		Mesh.AddTriangle(BaseIndex, BaseIndex + 2, BaseIndex + 3);
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
	return SafeColumns * GetEffectivePanelWidthCm() + 2.0f * GetEffectiveBorderCm();
}

float ATSAVLEDWall::GetWallHeightCm() const
{
	const int32 SafeRows = FMath::Clamp(Rows, 1, 64);
	return SafeRows * GetEffectivePanelHeightCm() + 2.0f * GetEffectiveBorderCm();
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
	TArray<FVector> ColumnCenters;
	TArray<float> ColumnYaws;
	BuildColumnTransforms(ColumnCenters, ColumnYaws);
	return ColumnYaws.IsValidIndex(Column) ? ColumnYaws[Column] : 0.0f;
}

float ATSAVLEDWall::GetRowAngleDegrees(int32 Row) const
{
	TArray<FVector> RowCenters;
	TArray<float> RowPitches;
	BuildRowTransforms(RowCenters, RowPitches);
	return RowPitches.IsValidIndex(Row) ? RowPitches[Row] : 0.0f;
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

void ATSAVLEDWall::BuildColumnTransforms(TArray<FVector>& OutFrontCenters, TArray<float>& OutYawDegrees) const
{
	const int32 SafeColumns = FMath::Clamp(Columns, 1, 64);
	const float PanelWidth = GetEffectivePanelWidthCm();

	OutFrontCenters.SetNumZeroed(SafeColumns);
	OutYawDegrees.SetNumZeroed(SafeColumns);

	// Each stored value is a hinge delta between this column and the next one.
	// Accumulating the deltas makes repeated values form a real arc instead of
	// rotating every cabinet to the same absolute heading.
	for (int32 Column = 1; Column < SafeColumns; ++Column)
	{
		const float SeamAngle = ColumnSeamAnglesDegrees.IsValidIndex(Column - 1)
			? TSAVLEDWall::Private::SanitizeAngle(ColumnSeamAnglesDegrees[Column - 1])
			: 0.0f;
		OutYawDegrees[Column] = OutYawDegrees[Column - 1] + SeamAngle;
	}

	// Center the overall wall facing on the actor so convex and concave curves do
	// not inherit an arbitrary rotation from the first column.
	const float FacingCenter = (OutYawDegrees[0] + OutYawDegrees.Last()) * 0.5f;
	for (float& Yaw : OutYawDegrees)
	{
		Yaw -= FacingCenter;
	}

	// These are centers on the video plane, not cabinet centerlines. Connecting
	// their half-width edges makes every vertical hinge watertight.
	for (int32 Column = 1; Column < SafeColumns; ++Column)
	{
		const FVector PreviousTangent = FRotator(0.0f, OutYawDegrees[Column - 1], 0.0f).RotateVector(FVector::YAxisVector);
		const FVector CurrentTangent = FRotator(0.0f, OutYawDegrees[Column], 0.0f).RotateVector(FVector::YAxisVector);
		OutFrontCenters[Column] = OutFrontCenters[Column - 1]
			+ PreviousTangent * (PanelWidth * 0.5f)
			+ CurrentTangent * (PanelWidth * 0.5f);
	}

	const FVector FirstTangent = FRotator(0.0f, OutYawDegrees[0], 0.0f).RotateVector(FVector::YAxisVector);
	const FVector LastTangent = FRotator(0.0f, OutYawDegrees.Last(), 0.0f).RotateVector(FVector::YAxisVector);
	const FVector FirstEdge = OutFrontCenters[0] - FirstTangent * (PanelWidth * 0.5f);
	const FVector LastEdge = OutFrontCenters.Last() + LastTangent * (PanelWidth * 0.5f);
	const FVector CenterOffset = (FirstEdge + LastEdge) * 0.5f;
	for (FVector& Center : OutFrontCenters)
	{
		Center -= CenterOffset;
	}
}

void ATSAVLEDWall::BuildRowTransforms(TArray<FVector>& OutFrontCenters, TArray<float>& OutPitchDegrees) const
{
	const int32 SafeRows = FMath::Clamp(Rows, 1, 64);
	const float PanelHeight = GetEffectivePanelHeightCm();

	OutFrontCenters.SetNumZeroed(SafeRows);
	OutPitchDegrees.SetNumZeroed(SafeRows);
	for (int32 Row = 1; Row < SafeRows; ++Row)
	{
		const float SeamAngle = RowSeamAnglesDegrees.IsValidIndex(Row - 1)
			? TSAVLEDWall::Private::SanitizeAngle(RowSeamAnglesDegrees[Row - 1])
			: 0.0f;
		OutPitchDegrees[Row] = OutPitchDegrees[Row - 1] + SeamAngle;
	}

	// Keep the first row on the actor's front plane. A +90/-90 seam therefore
	// folds the following row all the way under/over without tilting the main
	// wall by half the requested bend.
	for (int32 Row = 1; Row < SafeRows; ++Row)
	{
		const FVector PreviousDown = FRotator(OutPitchDegrees[Row - 1], 0.0f, 0.0f).RotateVector(-FVector::ZAxisVector);
		const FVector CurrentDown = FRotator(OutPitchDegrees[Row], 0.0f, 0.0f).RotateVector(-FVector::ZAxisVector);
		OutFrontCenters[Row] = OutFrontCenters[Row - 1]
			+ PreviousDown * (PanelHeight * 0.5f)
			+ CurrentDown * (PanelHeight * 0.5f);
	}

	const FVector FirstDown = FRotator(OutPitchDegrees[0], 0.0f, 0.0f).RotateVector(-FVector::ZAxisVector);
	const FVector LastDown = FRotator(OutPitchDegrees.Last(), 0.0f, 0.0f).RotateVector(-FVector::ZAxisVector);
	const FVector FirstEdge = OutFrontCenters[0] - FirstDown * (PanelHeight * 0.5f);
	const FVector LastEdge = OutFrontCenters.Last() + LastDown * (PanelHeight * 0.5f);
	const FVector CenterOffset = (FirstEdge + LastEdge) * 0.5f;
	for (FVector& Center : OutFrontCenters)
	{
		Center -= CenterOffset;
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
	const float CabinetWidth = FMath::Max(EffectivePanelWidth - PanelGapCm, 1.0f);
	const float CabinetHeight = FMath::Max(EffectivePanelHeight - PanelGapCm, 1.0f);
	const float CabinetFrontDepth = EffectiveDepth * 0.5f;
	const float DisplayFrontDepth = CabinetFrontDepth + 0.2f;
	const float BackDepth = -EffectiveDepth * 0.5f;

	DisplaySurface->SetVisibility(false);
	DisplaySurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	for (UStaticMeshComponent* LegacyComponent : {Backing.Get(), TopBorder.Get(), BottomBorder.Get(), LeftBorder.Get(), RightBorder.Get()})
	{
		LegacyComponent->SetVisibility(false);
		LegacyComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	PanelSeams->ClearInstances();
	PanelSeams->SetVisibility(false);

	TArray<FVector> ColumnFrontCenters;
	TArray<float> ColumnYaws;
	BuildColumnTransforms(ColumnFrontCenters, ColumnYaws);
	TArray<FVector> UnusedRowFrontCenters;
	TArray<float> RowPitches;
	BuildRowTransforms(UnusedRowFrontCenters, RowPitches);
	TArray<FVector> ColumnHorizontals;
	ColumnHorizontals.Reserve(Columns);
	for (int32 Column = 0; Column < Columns; ++Column)
	{
		ColumnHorizontals.Add(FRotator(0.0f, ColumnYaws[Column], 0.0f).RotateVector(FVector::YAxisVector));
	}

	// Build one shared front vertex grid. Each vertical grid line projects the
	// requested row direction perpendicular to its local horizontal hinge. That
	// prevents a panel from collapsing when simultaneous 90-degree row and
	// column bends would otherwise align both of its axes.
	TArray<FVector> FrontGrid;
	FrontGrid.SetNumZeroed((Columns + 1) * (Rows + 1));
	auto GridIndex = [this](int32 EdgeColumn, int32 EdgeRow)
	{
		return EdgeRow * (Columns + 1) + EdgeColumn;
	};
	FrontGrid[GridIndex(0, 0)] = ColumnFrontCenters[0] - ColumnHorizontals[0] * (EffectivePanelWidth * 0.5f);
	for (int32 EdgeColumn = 1; EdgeColumn <= Columns; ++EdgeColumn)
	{
		FrontGrid[GridIndex(EdgeColumn, 0)] = ColumnFrontCenters[EdgeColumn - 1] + ColumnHorizontals[EdgeColumn - 1] * (EffectivePanelWidth * 0.5f);
	}
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		const FVector RequestedDown = FRotator(RowPitches[Row], 0.0f, 0.0f).RotateVector(-FVector::ZAxisVector);
		for (int32 EdgeColumn = 0; EdgeColumn <= Columns; ++EdgeColumn)
		{
			FVector LocalHorizontal;
			const bool bEnabledOnLeft = EdgeColumn > 0 && IsPanelEnabled(EdgeColumn - 1, Row);
			const bool bEnabledOnRight = EdgeColumn < Columns && IsPanelEnabled(EdgeColumn, Row);
			if (bEnabledOnLeft && !bEnabledOnRight)
			{
				LocalHorizontal = ColumnHorizontals[EdgeColumn - 1];
			}
			else if (bEnabledOnRight && !bEnabledOnLeft)
			{
				LocalHorizontal = ColumnHorizontals[EdgeColumn];
			}
			else if (EdgeColumn == 0)
			{
				LocalHorizontal = ColumnHorizontals[0];
			}
			else if (EdgeColumn == Columns)
			{
				LocalHorizontal = ColumnHorizontals.Last();
			}
			else
			{
				LocalHorizontal = (ColumnHorizontals[EdgeColumn - 1] + ColumnHorizontals[EdgeColumn]).GetSafeNormal();
				if (LocalHorizontal.IsNearlyZero())
				{
					LocalHorizontal = ColumnHorizontals[EdgeColumn - 1];
				}
			}
			FVector SafeDown = RequestedDown - LocalHorizontal * FVector::DotProduct(RequestedDown, LocalHorizontal);
			if (!SafeDown.Normalize())
			{
				SafeDown = -FVector::ZAxisVector;
			}
			if (FVector::DotProduct(SafeDown, RequestedDown) < 0.0f)
			{
				SafeDown *= -1.0f;
			}
			FrontGrid[GridIndex(EdgeColumn, Row + 1)] = FrontGrid[GridIndex(EdgeColumn, Row)] + SafeDown * EffectivePanelHeight;
		}
	}
	const FVector SurfaceCenter = (
		FrontGrid[GridIndex(0, 0)] +
		FrontGrid[GridIndex(Columns, 0)] +
		FrontGrid[GridIndex(0, Rows)] +
		FrontGrid[GridIndex(Columns, Rows)]) * 0.25f;
	for (FVector& Point : FrontGrid)
	{
		Point += FVector::XAxisVector * DisplayFrontDepth - SurfaceCenter;
	}

	FMeshBuffers DisplayMesh;
	FMeshBuffers FrameMesh;
	const float ChamferSize = FMath::Min3(EffectiveBorder, EffectiveDepth, FMath::Min(EffectivePanelWidth, EffectivePanelHeight) * 0.45f);
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Column = 0; Column < Columns; ++Column)
		{
			if (!IsPanelEnabled(Column, Row))
			{
				continue;
			}

			const FVector TopLeft = FrontGrid[GridIndex(Column, Row)];
			const FVector TopRight = FrontGrid[GridIndex(Column + 1, Row)];
			const FVector BottomRight = FrontGrid[GridIndex(Column + 1, Row + 1)];
			const FVector BottomLeft = FrontGrid[GridIndex(Column, Row + 1)];
			const FVector Horizontal = ((TopRight - TopLeft) + (BottomRight - BottomLeft)).GetSafeNormal();
			const FVector Down = ((BottomLeft - TopLeft) + (BottomRight - TopRight)).GetSafeNormal();
			const FVector VerticalUp = -Down;
			const FVector Normal = FVector::CrossProduct(Down, Horizontal).GetSafeNormal();
			const FVector FrontCenter = (TopLeft + TopRight + BottomRight + BottomLeft) * 0.25f;
			const bool bIgnoreInternalCurveOnRow = RowIgnoreInternalColumnCurves.IsValidIndex(Row) && RowIgnoreInternalColumnCurves[Row];
			const bool bInternalCurveEnabled = ColumnInternalCurveEnabled.IsValidIndex(Column) && ColumnInternalCurveEnabled[Column] && !bIgnoreInternalCurveOnRow;
			const bool bPreviousRowIgnoresInternalCurve = Row > 0 && RowIgnoreInternalColumnCurves.IsValidIndex(Row - 1) && RowIgnoreInternalColumnCurves[Row - 1];
			const bool bNextRowIgnoresInternalCurve = Row + 1 < Rows && RowIgnoreInternalColumnCurves.IsValidIndex(Row + 1) && RowIgnoreInternalColumnCurves[Row + 1];
			const double TopCurveScale = bInternalCurveEnabled && !bPreviousRowIgnoresInternalCurve ? 1.0 : 0.0;
			const double BottomCurveScale = bInternalCurveEnabled && !bNextRowIgnoresInternalCurve ? 1.0 : 0.0;
			const double RadiusAMeters = ColumnInternalCurveRadiusAMeters.IsValidIndex(Column) ? ColumnInternalCurveRadiusAMeters[Column] : 0.0;
			const double RadiusBMeters = ColumnInternalCurveRadiusBMeters.IsValidIndex(Column) ? ColumnInternalCurveRadiusBMeters[Column] : 0.0;
			const double MaximumConcaveOffset = bInternalCurveEnabled
				? FMath::Max3(
					-GetInternalCurveOffsetCm(0.25, EffectivePanelWidth, RadiusAMeters, RadiusBMeters),
					-GetInternalCurveOffsetCm(0.75, EffectivePanelWidth, RadiusAMeters, RadiusBMeters),
					0.0) * FMath::Max(TopCurveScale, BottomCurveScale)
				: 0.0;
			const FVector CabinetCenter = FrontCenter - Normal * (DisplayFrontDepth + MaximumConcaveOffset);
			const TArray<FVector2D> Polygon = MakePanelPolygon(GetPanelEdgeStyle(Column, Row), EffectivePanelWidth, EffectivePanelHeight, RoundEdgeRadiusMeters);
			AppendFrontFace(DisplayMesh, Polygon, TopLeft, TopRight, BottomRight, BottomLeft, Column, Row, Columns, Rows, bInternalCurveEnabled, RadiusAMeters, RadiusBMeters, TopCurveScale, BottomCurveScale, EffectivePanelWidth);
			AppendChamferedCabinetBody(FrameMesh, Polygon, CabinetCenter, Normal, Horizontal, VerticalUp, CabinetWidth, CabinetHeight, CabinetFrontDepth, BackDepth);

			if (ChamferSize > 0.0f)
			{
				if (bInternalCurveEnabled && (!FMath::IsNearlyZero(RadiusAMeters) || !FMath::IsNearlyZero(RadiusBMeters)))
				{
					auto GetSurfaceFrame = [&](const FVector2D& Point, FVector& OutPosition, FVector& OutNormal, FVector& OutHorizontal, FVector& OutDown)
					{
						const double SampleDistance = 0.0001;
						const FVector2D LeftSample(FMath::Max(Point.X - SampleDistance, 0.0), Point.Y);
						const FVector2D RightSample(FMath::Min(Point.X + SampleDistance, 1.0), Point.Y);
						const FVector2D TopSample(Point.X, FMath::Max(Point.Y - SampleDistance, 0.0));
						const FVector2D BottomSample(Point.X, FMath::Min(Point.Y + SampleDistance, 1.0));
						OutPosition = MapCurvedPanelPoint(Point, TopLeft, TopRight, BottomRight, BottomLeft, true, RadiusAMeters, RadiusBMeters, TopCurveScale, BottomCurveScale, EffectivePanelWidth);
						OutHorizontal = (MapCurvedPanelPoint(RightSample, TopLeft, TopRight, BottomRight, BottomLeft, true, RadiusAMeters, RadiusBMeters, TopCurveScale, BottomCurveScale, EffectivePanelWidth)
							- MapCurvedPanelPoint(LeftSample, TopLeft, TopRight, BottomRight, BottomLeft, true, RadiusAMeters, RadiusBMeters, TopCurveScale, BottomCurveScale, EffectivePanelWidth)).GetSafeNormal();
						OutDown = (MapCurvedPanelPoint(BottomSample, TopLeft, TopRight, BottomRight, BottomLeft, true, RadiusAMeters, RadiusBMeters, TopCurveScale, BottomCurveScale, EffectivePanelWidth)
							- MapCurvedPanelPoint(TopSample, TopLeft, TopRight, BottomRight, BottomLeft, true, RadiusAMeters, RadiusBMeters, TopCurveScale, BottomCurveScale, EffectivePanelWidth)).GetSafeNormal();
						OutNormal = FVector::CrossProduct(OutDown, OutHorizontal).GetSafeNormal();
					};

					if (!IsPanelEnabled(Column, Row - 1) || !IsPanelEnabled(Column, Row + 1))
					{
						const int32 SegmentCount = InternalCurveSegmentsPerHalf * 2;
						for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
						{
							const double X0 = static_cast<double>(Segment) / SegmentCount;
							const double X1 = static_cast<double>(Segment + 1) / SegmentCount;
							if (!IsPanelEnabled(Column, Row - 1))
							{
								FVector PositionA, NormalA, HorizontalA, DownA;
								FVector PositionB, NormalB, HorizontalB, DownB;
								GetSurfaceFrame(FVector2D(X0, 0.0), PositionA, NormalA, HorizontalA, DownA);
								GetSurfaceFrame(FVector2D(X1, 0.0), PositionB, NormalB, HorizontalB, DownB);
								AppendChamferedEdgeVariable(FrameMesh, PositionA, PositionB, NormalA, NormalB, DownA, DownB, ChamferSize);
							}
							if (!IsPanelEnabled(Column, Row + 1))
							{
								FVector PositionA, NormalA, HorizontalA, DownA;
								FVector PositionB, NormalB, HorizontalB, DownB;
								GetSurfaceFrame(FVector2D(X0, 1.0), PositionA, NormalA, HorizontalA, DownA);
								GetSurfaceFrame(FVector2D(X1, 1.0), PositionB, NormalB, HorizontalB, DownB);
								AppendChamferedEdgeVariable(FrameMesh, PositionA, PositionB, NormalA, NormalB, -DownA, -DownB, ChamferSize);
							}
						}
					}
					if (!IsPanelEnabled(Column - 1, Row))
					{
						FVector PositionA, NormalA, HorizontalA, DownA;
						FVector PositionB, NormalB, HorizontalB, DownB;
						GetSurfaceFrame(FVector2D(0.0, 0.0), PositionA, NormalA, HorizontalA, DownA);
						GetSurfaceFrame(FVector2D(0.0, 1.0), PositionB, NormalB, HorizontalB, DownB);
						AppendChamferedEdgeVariable(FrameMesh, PositionA, PositionB, NormalA, NormalB, HorizontalA, HorizontalB, ChamferSize);
					}
					if (!IsPanelEnabled(Column + 1, Row))
					{
						FVector PositionA, NormalA, HorizontalA, DownA;
						FVector PositionB, NormalB, HorizontalB, DownB;
						GetSurfaceFrame(FVector2D(1.0, 0.0), PositionA, NormalA, HorizontalA, DownA);
						GetSurfaceFrame(FVector2D(1.0, 1.0), PositionB, NormalB, HorizontalB, DownB);
						AppendChamferedEdgeVariable(FrameMesh, PositionA, PositionB, NormalA, NormalB, -HorizontalA, -HorizontalB, ChamferSize);
					}
				}
				else
				{
					if (!IsPanelEnabled(Column, Row - 1))
					{
						AppendChamferedEdge(FrameMesh, TopLeft, TopRight, Normal, Down, ChamferSize);
					}
					if (!IsPanelEnabled(Column, Row + 1))
					{
						AppendChamferedEdge(FrameMesh, BottomLeft, BottomRight, Normal, VerticalUp, ChamferSize);
					}
					if (!IsPanelEnabled(Column - 1, Row))
					{
						AppendChamferedEdge(FrameMesh, TopLeft, BottomLeft, Normal, Horizontal, ChamferSize);
					}
					if (!IsPanelEnabled(Column + 1, Row))
					{
						AppendChamferedEdge(FrameMesh, TopRight, BottomRight, Normal, -Horizontal, ChamferSize);
					}
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
			Link.RowAngleDegrees = GetRowAngleDegrees(Row);
			Link.EdgeStyle = GetPanelEdgeStyle(Column, Row);
		}
	}
}

void ATSAVLEDWall::NormalizeShapeSettings()
{
	const int32 RequiredSeams = FMath::Max(Columns - 1, 0);
	if (ColumnSeamAnglesDegrees.IsEmpty() && !ColumnAnglesDegrees.IsEmpty())
	{
		ColumnSeamAnglesDegrees.SetNum(RequiredSeams);
		if (ColumnAnglesDegrees.Num() == Columns)
		{
			// Migrate walls saved by the initial implementation, where the legacy
			// values were absolute column headings.
			for (int32 Seam = 0; Seam < RequiredSeams; ++Seam)
			{
				ColumnSeamAnglesDegrees[Seam] = TSAVLEDWall::Private::SanitizeAngle(ColumnAnglesDegrees[Seam + 1] - ColumnAnglesDegrees[Seam]);
			}
		}
		else
		{
			// Also accept experimental assets that already stored one value per seam
			// under the legacy property name.
			for (int32 Seam = 0; Seam < RequiredSeams; ++Seam)
			{
				ColumnSeamAnglesDegrees[Seam] = ColumnAnglesDegrees.IsValidIndex(Seam) ? ColumnAnglesDegrees[Seam] : 0.0f;
			}
		}
		ColumnAnglesDegrees.Reset();
	}
	ColumnSeamAnglesDegrees.SetNum(RequiredSeams);
	for (float& Angle : ColumnSeamAnglesDegrees)
	{
		Angle = TSAVLEDWall::Private::SanitizeAngle(Angle);
	}
	RowSeamAnglesDegrees.SetNum(FMath::Max(Rows - 1, 0));
	for (float& Angle : RowSeamAnglesDegrees)
	{
		Angle = TSAVLEDWall::Private::SanitizeAngle(Angle);
	}
	RoundEdgeRadiusMeters = TSAVLEDWall::Private::SanitizeRoundRadiusMeters(RoundEdgeRadiusMeters);
	ColumnInternalCurveEnabled.SetNum(Columns);
	const int32 PreviousRadiusACount = ColumnInternalCurveRadiusAMeters.Num();
	const int32 PreviousRadiusBCount = ColumnInternalCurveRadiusBMeters.Num();
	ColumnInternalCurveRadiusAMeters.SetNum(Columns);
	ColumnInternalCurveRadiusBMeters.SetNum(Columns);
	RowIgnoreInternalColumnCurves.SetNum(Rows);
	for (int32 Column = 0; Column < Columns; ++Column)
	{
		if (Column >= PreviousRadiusACount)
		{
			ColumnInternalCurveRadiusAMeters[Column] = 1.0;
		}
		if (Column >= PreviousRadiusBCount)
		{
			ColumnInternalCurveRadiusBMeters[Column] = 1.0;
		}
		ColumnInternalCurveRadiusAMeters[Column] = TSAVLEDWall::Private::SanitizeSignedRadiusMeters(ColumnInternalCurveRadiusAMeters[Column]);
		ColumnInternalCurveRadiusBMeters[Column] = TSAVLEDWall::Private::SanitizeSignedRadiusMeters(ColumnInternalCurveRadiusBMeters[Column]);
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
