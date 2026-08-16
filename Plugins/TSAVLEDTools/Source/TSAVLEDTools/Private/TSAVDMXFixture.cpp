// Copyright TSAV. All Rights Reserved.

#include "TSAVDMXFixture.h"

#include "Components/SceneComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Game/DMXComponent.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXImportGDTF.h"
#include "Library/DMXLibrary.h"

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

	LensVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lens Mesh"));
	LensVisual->SetupAttachment(TiltPivot);

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
	LensVisual->SetMobility(EComponentMobility::Movable);
	// Fixtures need editor/runtime hit testing, but never simulate physics. Query
	// collision avoids Chaos mass generation for intentionally flat GDTF parts.
	BaseVisual->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	YokeVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LensVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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

bool ATSAVDMXFixture::ApplyFixtureDefinition(const FTSAVDMXFixtureDefinition& Definition, const bool bApplyDefaultPatch)
{
	FixtureDefinitionId = Definition.DefinitionId;
	GDTFSource = Definition.GDTFSource.LoadSynchronous();
	GDTFModeName = Definition.GDTFModeName;
	BaseMesh = Definition.BaseMesh.LoadSynchronous();
	YokeMesh = Definition.YokeMesh.LoadSynchronous();
	HeadMesh = Definition.HeadMesh.LoadSynchronous();
	LensMesh = Definition.LensMesh.LoadSynchronous();
	FixtureScale = FMath::Max(Definition.FixtureScale, 0.001f);
	ModelRotation = Definition.ModelRotation;
	auto SafeModelScale = [](const FVector& Scale)
	{
		return FVector(
			FMath::Clamp(FMath::Abs(Scale.X), 0.001, 1000.0),
			FMath::Clamp(FMath::Abs(Scale.Y), 0.001, 1000.0),
			FMath::Clamp(FMath::Abs(Scale.Z), 0.001, 1000.0));
	};
	BaseMeshScale = SafeModelScale(Definition.BaseMeshScale);
	YokeMeshScale = SafeModelScale(Definition.YokeMeshScale);
	HeadMeshScale = SafeModelScale(Definition.HeadMeshScale);
	LensMeshScale = SafeModelScale(Definition.LensMeshScale);
	BaseMeshOffset = Definition.BaseMeshOffset;
	YokeMeshOffset = Definition.YokeMeshOffset;
	HeadMeshOffset = Definition.HeadMeshOffset;
	PanPivotOffset = Definition.PanPivotOffset;
	TiltPivotOffset = Definition.TiltPivotOffset;
	PanPivotRotation = Definition.PanPivotRotation;
	TiltPivotRotation = Definition.TiltPivotRotation;
	PanMinDegrees = Definition.PanMinDegrees;
	PanMaxDegrees = Definition.PanMaxDegrees;
	TiltMinDegrees = Definition.TiltMinDegrees;
	TiltMaxDegrees = Definition.TiltMaxDegrees;
	PanSpeedDegreesPerSecond = FMath::Max(Definition.PanSpeedDegreesPerSecond, 0.0f);
	TiltSpeedDegreesPerSecond = FMath::Max(Definition.TiltSpeedDegreesPerSecond, 0.0f);
	LensOffset = Definition.LensOffset;
	LensMeshRotation = Definition.LensMeshRotation;
	BeamRotation = Definition.BeamRotation;
	MaximumIntensityLumens = FMath::Max(Definition.MaximumIntensityLumens, 0.0f);
	MinimumBeamAngleDegrees = Definition.MinimumBeamAngleDegrees;
	MaximumBeamAngleDegrees = Definition.MaximumBeamAngleDegrees;
	AttenuationRadiusCm = FMath::Max(Definition.AttenuationRadiusCm, 1.0f);

	if (bApplyDefaultPatch)
	{
		UDMXEntityFixturePatch* DefaultPatch = nullptr;
		if (UDMXLibrary* Library = Definition.DMXLibrary.LoadSynchronous())
		{
			DefaultPatch = Cast<UDMXEntityFixturePatch>(Library->FindEntity(Definition.FixturePatchId));
		}
		SetFixturePatch(DefaultPatch);
	}

	ApplyModelSetup();
	ApplyPreviewValues();
	return GDTFSource != nullptr && (BaseMesh != nullptr || YokeMesh != nullptr || HeadMesh != nullptr || LensMesh != nullptr);
}

void ATSAVDMXFixture::ApplyNormalizedDMX(
	const float Pan,
	const float Tilt,
	const float Dimmer,
	const FLinearColor Color,
	const float Zoom,
	const bool bSnap)
{
	SetTargetsFromNormalized(Pan, Tilt, Dimmer, Color, Zoom);
	ApplyMotionAndBeam(0.0f, bSnap);
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
	LensVisual->SetStaticMesh(LensMesh);
	BaseVisual->SetRelativeScale3D(BaseMeshScale);
	YokeVisual->SetRelativeScale3D(YokeMeshScale);
	HeadVisual->SetRelativeScale3D(HeadMeshScale);
	LensVisual->SetRelativeScale3D(LensMeshScale);
	BaseVisual->SetRelativeLocation(BaseMeshOffset);
	YokeVisual->SetRelativeLocation(YokeMeshOffset);
	HeadVisual->SetRelativeLocation(HeadMeshOffset);
	PanPivot->SetRelativeLocation(PanPivotOffset);
	TiltPivot->SetRelativeLocation(TiltPivotOffset);
	LensVisual->SetRelativeLocation(LensOffset);
	LensVisual->SetRelativeRotation(LensMeshRotation);
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

	const FQuat PanMotion(FVector::UpVector, FMath::DegreesToRadians(CurrentPanDegrees));
	const FQuat TiltMotion(FVector::ForwardVector, FMath::DegreesToRadians(CurrentTiltDegrees));
	PanPivot->SetRelativeRotation(PanPivotRotation.Quaternion() * PanMotion);
	TiltPivot->SetRelativeRotation(TiltPivotRotation.Quaternion() * TiltMotion);

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
