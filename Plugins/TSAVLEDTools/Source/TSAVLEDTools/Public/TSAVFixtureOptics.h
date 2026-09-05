// Copyright TSAV. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "TSAVFixtureOptics.generated.h"
class UMaterialInterface;

/** Original TSAV generic optics profile. Physical channel ranges remain in the fixture's DMX mode. */
USTRUCT(BlueprintType)
struct TSAVLEDTOOLS_API FTSAVFixtureOptics
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Optics") bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Optics") TSoftObjectPtr<UMaterialInterface> GoboMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/TSAV/Materials/M_TSAVNativeGobo.M_TSAVNativeGobo")));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Optics", meta=(ClampMin="1",ClampMax="8")) int32 PrismFacets = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Optics", meta=(ClampMin="0",ClampMax="30")) float PrismSeparationDegrees = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Optics", meta=(ClampMin="1",ClampMax="30")) float MaximumStrobeHz = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Optics", meta=(ClampMin="0",ClampMax="10")) float VolumetricScattering = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Optics") bool bCastShadows = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Optics") TArray<FLinearColor> ColorWheel = {FLinearColor::White,FLinearColor::Red,FLinearColor::Green,FLinearColor::Blue,FLinearColor(1,0.5f,0),FLinearColor(0,1,1),FLinearColor(1,0,1)};
};
