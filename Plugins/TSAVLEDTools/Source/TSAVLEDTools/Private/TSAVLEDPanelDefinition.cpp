// Copyright TSAV. All Rights Reserved.

#include "TSAVLEDPanelDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVLEDPanelDefinition)

FIntPoint UTSAVLEDPanelDefinition::GetResolution() const
{
	return FIntPoint(FMath::Max(ResolutionX, 1), FMath::Max(ResolutionY, 1));
}

FVector2D UTSAVLEDPanelDefinition::GetPixelPitchMm() const
{
	const FIntPoint Resolution = GetResolution();
	return FVector2D(
		FMath::Max(WidthCm, 1.0f) * 10.0f / Resolution.X,
		FMath::Max(HeightCm, 1.0f) * 10.0f / Resolution.Y);
}
