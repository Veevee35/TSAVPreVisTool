// Copyright TSAV. All Rights Reserved.

#include "TSAVDMXFixture.h"

#include "Components/SceneComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Game/DMXComponent.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXImportGDTF.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVDMXFixture)

ATSAVDMXFixture::ATSAVDMXFixture()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bAllowTickBeforeBeginPlay = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	ModelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Model Root"));
	ModelRoot->SetupAttachment(SceneRoot);

	BaseVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base Mesh"));
	BaseVisual->SetupAttachment(ModelRoot);

	PanPivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pan Pivot"));
	PanPivot->SetupAttachment(ModelRoot);

	YokeVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Yoke Mesh"));
	YokeVisual->SetupAttachment(PanPivot);

	TiltPivot = CreateDefaultSubobject<USceneComponent>(TEXT("Tilt Pivot"));
	TiltPivot->SetupAttachment(PanPivot);

	HeadVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head Mesh"));
	HeadVisual->SetupAttachment(TiltPivot);

	LensRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Lens Root"));
	LensRoot->SetupAttachment(TiltPivot);

	BeamLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Beam Light"));
	BeamLight->SetupAttachment(LensRoot);
	BeamLight->SetMobility(EComponentMobility::Movable);
	BeamLight->SetCastShadows(false);
	BeamLight->SetUseInverseSquaredFalloff(true);

	DMXComponent = CreateDefaultSubobject<UDMXComponent>(TEXT("DMX"));
	DMXComponent->SetReceiveDMXFromPatch(true);
	DMXComponent->OnFixturePatchReceived.AddDynamic(this, &ATSAVDMXFixture::OnFixturePatchReceived);

	BaseVisual->SetMobility(EComponentMobility::Movable);
	YokeVisual->SetMobility(EComponentMobility::Movable);
	HeadVisual->SetMobility(EComponentMobility::Movable);
	BaseVisual->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	YokeVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TargetPanDegrees = FMath::Lerp(PanMinDegrees, PanMaxDegrees, 0.5f);
	TargetTiltDegrees = FMath::Lerp(TiltMinDegrees, TiltMaxDegrees, 0.5f);
}

void ATSAVDMXFixture::BeginPlay()
{
	Super::BeginPlay();
	ApplyModelSetup();
	DMXComponent->SetReceiveDMXFromPatch(true);
}

void ATSAVDMXFixture::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyModelSetup();
	ApplyMotionAndBeam(0.0f, true);
}

void ATSAVDMXFixture::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyMotionAndBeam(DeltaSeconds, false);
}

bool ATSAVDMXFixture::ShouldTickIfViewportsOnly() const
{
	return true;
}

#if WITH_EDITOR
void ATSAVDMXFixture::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ApplyModelSetup();
}
#endif

void ATSAVDMXFixture::ApplyPreviewValues()
{
	SetTargetsFromNormalized(
		FMath::Clamp(PreviewPan, 0.0f, 1.0f),
		FMath::Clamp(PreviewTilt, 0.0f, 1.0f),
		FMath::Clamp(PreviewDimmer, 0.0f, 1.0f),
		PreviewColor,
		0.0f);
	ApplyMotionAndBeam(0.0f, true);
}

void ATSAVDMXFixture::SetFixturePatch(UDMXEntityFixturePatch* FixturePatch)
{
	if (DMXComponent)
	{
		DMXComponent->SetFixturePatch(FixturePatch);
		DMXComponent->SetReceiveDMXFromPatch(true);
	}
}

UDMXEntityFixturePatch* ATSAVDMXFixture::GetFixturePatch() const
{
	return DMXComponent ? DMXComponent->GetFixturePatch() : nullptr;
}

void ATSAVDMXFixture::OnFixturePatchReceived(UDMXEntityFixturePatch* FixturePatch, const FDMXNormalizedAttributeValueMap& ValuePerAttribute)
{
	float PanValue = 0.5f;
	float TiltValue = 0.5f;
	float DimmerValue = LastDimmerValue;
	float ZoomValue = TargetZoom;
	float RedValue = TargetColor.R;
	float GreenValue = TargetColor.G;
	float BlueValue = TargetColor.B;

	FindAttributeValue(ValuePerAttribute, PanAttribute, { TEXT("pan") }, PanValue);
	FindAttributeValue(ValuePerAttribute, TiltAttribute, { TEXT("tilt") }, TiltValue);
	FindAttributeValue(ValuePerAttribute, DimmerAttribute, { TEXT("dimmer"), TEXT("intensity"), TEXT("masterdimmer") }, DimmerValue);
	FindAttributeValue(ValuePerAttribute, ZoomAttribute, { TEXT("zoom"), TEXT("beamangle") }, ZoomValue);

	const bool bHasRed = FindAttributeValue(ValuePerAttribute, RedAttribute, { TEXT("coloraddr"), TEXT("colorrgbred"), TEXT("red") }, RedValue);
	const bool bHasGreen = FindAttributeValue(ValuePerAttribute, GreenAttribute, { TEXT("coloraddg"), TEXT("colorrgbgreen"), TEXT("green") }, GreenValue);
	const bool bHasBlue = FindAttributeValue(ValuePerAttribute, BlueAttribute, { TEXT("coloraddb"), TEXT("colorrgbblue"), TEXT("blue") }, BlueValue);
	const FLinearColor Color = bHasRed || bHasGreen || bHasBlue ? FLinearColor(RedValue, GreenValue, BlueValue) : DefaultLightColor;

	SetTargetsFromNormalized(PanValue, TiltValue, DimmerValue, Color, ZoomValue);
}

void ATSAVDMXFixture::ApplyModelSetup()
{
	if (!ModelRoot)
	{
		return;
	}

	ModelRoot->SetRelativeScale3D(FVector(FMath::Max(FixtureScale, 0.001f)));
	ModelRoot->SetRelativeRotation(ModelRotation);
	BaseVisual->SetStaticMesh(BaseMesh);
	YokeVisual->SetStaticMesh(YokeMesh);
	HeadVisual->SetStaticMesh(HeadMesh);
	BaseVisual->SetRelativeLocation(BaseMeshOffset);
	YokeVisual->SetRelativeLocation(YokeMeshOffset);
	HeadVisual->SetRelativeLocation(HeadMeshOffset);
	PanPivot->SetRelativeLocation(PanPivotOffset);
	TiltPivot->SetRelativeLocation(TiltPivotOffset);
	LensRoot->SetRelativeLocation(LensOffset);
	LensRoot->SetRelativeRotation(BeamRotation);

	BeamLight->SetAttenuationRadius(FMath::Max(AttenuationRadiusCm, 1.0f));
	BeamLight->SetInnerConeAngle(FMath::Max(1.0f, MinimumBeamAngleDegrees * 0.7f));
	BeamLight->SetOuterConeAngle(FMath::Clamp(MaximumBeamAngleDegrees, 1.0f, 89.0f));
}

void ATSAVDMXFixture::ApplyMotionAndBeam(float DeltaSeconds, bool bSnap)
{
	const bool bSnapPan = bSnap || PanSpeedDegreesPerSecond <= 0.0f;
	const bool bSnapTilt = bSnap || TiltSpeedDegreesPerSecond <= 0.0f;
	CurrentPanDegrees = bSnapPan
		? TargetPanDegrees
		: FMath::FInterpConstantTo(CurrentPanDegrees, TargetPanDegrees, DeltaSeconds, PanSpeedDegreesPerSecond);
	CurrentTiltDegrees = bSnapTilt
		? TargetTiltDegrees
		: FMath::FInterpConstantTo(CurrentTiltDegrees, TargetTiltDegrees, DeltaSeconds, TiltSpeedDegreesPerSecond);

	PanPivot->SetRelativeRotation(FRotator(0.0f, CurrentPanDegrees, 0.0f));
	TiltPivot->SetRelativeRotation(FRotator(CurrentTiltDegrees, 0.0f, 0.0f));

	const float NarrowAngle = FMath::Clamp(MinimumBeamAngleDegrees, 1.0f, 89.0f);
	const float WideAngle = FMath::Clamp(FMath::Max(MaximumBeamAngleDegrees, NarrowAngle), 1.0f, 89.0f);
	const float OuterAngle = FMath::Lerp(WideAngle, NarrowAngle, FMath::Clamp(TargetZoom, 0.0f, 1.0f));
	BeamLight->SetOuterConeAngle(OuterAngle);
	BeamLight->SetInnerConeAngle(FMath::Clamp(OuterAngle * 0.72f, 0.0f, OuterAngle));
	BeamLight->SetIntensity(FMath::Max(MaximumIntensityLumens, 0.0f) * FMath::Clamp(TargetDimmer, 0.0f, 1.0f));
	BeamLight->SetLightColor(TargetColor.GetClamped());
	LastDimmerValue = TargetDimmer;
}

void ATSAVDMXFixture::SetTargetsFromNormalized(float Pan, float Tilt, float Dimmer, const FLinearColor& Color, float Zoom)
{
	const float SafePan = bInvertPan ? 1.0f - FMath::Clamp(Pan, 0.0f, 1.0f) : FMath::Clamp(Pan, 0.0f, 1.0f);
	const float SafeTilt = bInvertTilt ? 1.0f - FMath::Clamp(Tilt, 0.0f, 1.0f) : FMath::Clamp(Tilt, 0.0f, 1.0f);
	TargetPanDegrees = FMath::Lerp(PanMinDegrees, PanMaxDegrees, SafePan) + PanOffsetDegrees;
	TargetTiltDegrees = FMath::Lerp(TiltMinDegrees, TiltMaxDegrees, SafeTilt) + TiltOffsetDegrees;
	TargetDimmer = FMath::Clamp(Dimmer, 0.0f, 1.0f);
	TargetColor = Color.GetClamped();
	TargetZoom = FMath::Clamp(Zoom, 0.0f, 1.0f);
}

bool ATSAVDMXFixture::FindAttributeValue(const FDMXNormalizedAttributeValueMap& Values, FName PreferredName, const TArray<FString>& Aliases, float& OutValue)
{
	if (!PreferredName.IsNone())
	{
		if (const float* ExactValue = Values.Map.Find(FDMXAttributeName(PreferredName)))
		{
			OutValue = *ExactValue;
			return true;
		}
	}

	for (const TPair<FDMXAttributeName, float>& Pair : Values.Map)
	{
		const FString Candidate = CanonicalizeAttribute(Pair.Key.Name);
		for (const FString& Alias : Aliases)
		{
			if (Candidate == Alias || Candidate.StartsWith(Alias))
			{
				OutValue = Pair.Value;
				return true;
			}
		}
	}

	return false;
}

FString ATSAVDMXFixture::CanonicalizeAttribute(FName AttributeName)
{
	FString Result = AttributeName.ToString().ToLower();
	Result.ReplaceInline(TEXT("_"), TEXT(""));
	Result.ReplaceInline(TEXT("-"), TEXT(""));
	Result.ReplaceInline(TEXT(" "), TEXT(""));
	return Result;
}
