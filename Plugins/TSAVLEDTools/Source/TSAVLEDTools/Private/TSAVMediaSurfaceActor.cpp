// Copyright TSAV. All Rights Reserved.

#include "TSAVMediaSurfaceActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MediaComponent.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVMediaSurfaceActor)

namespace TSAVLEDTools::Private
{
	const TCHAR* DefaultDisplayMaterialPath = TEXT("/TSAVLEDTools/Materials/M_TSAV_LEDCanvasVideo.M_TSAV_LEDCanvasVideo");
	const TCHAR* DefaultFrameMaterialPath = TEXT("/TSAVLEDTools/Materials/M_TSAV_LEDCanvasFrame.M_TSAV_LEDCanvasFrame");
	const TCHAR* RectangleSubpixelTexturePath = TEXT("/TSAVLEDTools/Subpixels/T_TSAV_Subpixel_RectangleRGB.T_TSAV_Subpixel_RectangleRGB");
	const TCHAR* RoundSubpixelTexturePath = TEXT("/TSAVLEDTools/Subpixels/T_TSAV_Subpixel_RoundRGB.T_TSAV_Subpixel_RoundRGB");
}

ATSAVMediaSurfaceActor::ATSAVMediaSurfaceActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	DisplaySurface = CreateGeometryComponent(TEXT("Display Surface"), false);
	MediaComponent = CreateDefaultSubobject<UMediaComponent>(TEXT("Media"));
}

void ATSAVMediaSurfaceActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyDisplayMaterial();
	UpdatePlayback(true);
}

void ATSAVMediaSurfaceActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMediaPlayer* Player = GetMediaPlayer())
	{
		Player->Close();
	}

	Super::EndPlay(EndPlayReason);
}

void ATSAVMediaSurfaceActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyDisplayMaterial();
	UpdatePlayback(false);
}

#if WITH_EDITOR
void ATSAVMediaSurfaceActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ApplyDisplayMaterial();
	UpdatePlayback(PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ATSAVMediaSurfaceActor, MediaSource));
}
#endif

void ATSAVMediaSurfaceActor::RefreshMedia()
{
	ApplyDisplayMaterial();
	UpdatePlayback(true);
}

void ATSAVMediaSurfaceActor::PlayMedia()
{
	if (UMediaPlayer* Player = GetMediaPlayer())
	{
		Player->SetLooping(bLoop);
		Player->PlayOnOpen = true;

		if (MediaSource && Player->GetUrl() != MediaSource->GetUrl())
		{
			Player->OpenSource(MediaSource);
		}
		else
		{
			Player->Play();
		}
	}
}

void ATSAVMediaSurfaceActor::PauseMedia()
{
	if (UMediaPlayer* Player = GetMediaPlayer())
	{
		Player->Pause();
	}
}

void ATSAVMediaSurfaceActor::CloseMedia()
{
	if (UMediaPlayer* Player = GetMediaPlayer())
	{
		Player->Close();
	}
}

UMediaPlayer* ATSAVMediaSurfaceActor::GetMediaPlayer() const
{
	return MediaComponent ? MediaComponent->GetMediaPlayer() : nullptr;
}

UMediaTexture* ATSAVMediaSurfaceActor::GetMediaTexture() const
{
	return MediaComponent ? MediaComponent->GetMediaTexture() : nullptr;
}

FIntPoint ATSAVMediaSurfaceActor::GetSurfaceResolutionPixels() const
{
	const FIntPoint Resolution = GetNativePixelResolution();
	return FIntPoint(FMath::Max(Resolution.X, 1), FMath::Max(Resolution.Y, 1));
}

bool ATSAVMediaSurfaceActor::IsCanvasMappingValid() const
{
	if (!bUseCanvasMapping)
	{
		return true;
	}

	const FIntPoint SurfaceResolution = GetSurfaceResolutionPixels();
	return CanvasResolution.X > 0 && CanvasResolution.Y > 0 &&
		CanvasPosition.X >= 0 && CanvasPosition.Y >= 0 &&
		CanvasPosition.X + SurfaceResolution.X <= CanvasResolution.X &&
		CanvasPosition.Y + SurfaceResolution.Y <= CanvasResolution.Y;
}

void ATSAVMediaSurfaceActor::ApplyFrameMaterial(UStaticMeshComponent* MeshComponent) const
{
	if (MeshComponent)
	{
		MeshComponent->SetMaterial(0, ResolveFrameMaterial());
	}
}

UStaticMeshComponent* ATSAVMediaSurfaceActor::CreateGeometryComponent(FName ComponentName, bool bEnableCollision)
{
	UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(ComponentName);
	Component->SetupAttachment(SceneRoot);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCollisionEnabled(bEnableCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Component->SetCollisionProfileName(bEnableCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Component->SetStaticMesh(CubeMesh.Object);
	}

	return Component;
}

UMaterialInterface* ATSAVMediaSurfaceActor::ResolveFrameMaterial() const
{
	if (FrameMaterial)
	{
		return FrameMaterial;
	}

	if (UMaterialInterface* BundledMaterial = LoadObject<UMaterialInterface>(nullptr, TSAVLEDTools::Private::DefaultFrameMaterialPath))
	{
		return BundledMaterial;
	}

	return UMaterial::GetDefaultMaterial(MD_Surface);
}

void ATSAVMediaSurfaceActor::OnDisplayMaterialUpdated(UMaterialInterface* AppliedMaterial)
{
}

void ATSAVMediaSurfaceActor::ApplyDisplayMaterial()
{
	if (!DisplaySurface)
	{
		return;
	}

	UMaterialInterface* BaseMaterial = ResolveDisplayMaterial();
	DisplayMaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	if (!DisplayMaterialInstance)
	{
		DisplaySurface->SetMaterial(0, BaseMaterial);
		OnDisplayMaterialUpdated(BaseMaterial);
		return;
	}

	DisplayMaterialInstance->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveStrength);
	DisplayMaterialInstance->SetScalarParameterValue(TEXT("SubpixelStrength"), SubpixelLayout == ETSAVLEDSubpixelLayout::None ? 0.0f : FMath::Clamp(SubpixelStrength, 0.0f, 1.0f));

	const FIntPoint SurfaceResolution = GetSurfaceResolutionPixels();
	const float SafeCanvasWidth = static_cast<float>(FMath::Max(CanvasResolution.X, 1));
	const float SafeCanvasHeight = static_cast<float>(FMath::Max(CanvasResolution.Y, 1));
	const float ScaleX = bUseCanvasMapping ? SurfaceResolution.X / SafeCanvasWidth : 1.0f;
	const float ScaleY = bUseCanvasMapping ? SurfaceResolution.Y / SafeCanvasHeight : 1.0f;
	const float OffsetX = bUseCanvasMapping ? CanvasPosition.X / SafeCanvasWidth : 0.0f;
	const float OffsetY = bUseCanvasMapping ? CanvasPosition.Y / SafeCanvasHeight : 0.0f;

	DisplayMaterialInstance->SetScalarParameterValue(TEXT("CanvasScaleX"), ScaleX);
	DisplayMaterialInstance->SetScalarParameterValue(TEXT("CanvasScaleY"), ScaleY);
	DisplayMaterialInstance->SetScalarParameterValue(TEXT("CanvasOffsetX"), OffsetX);
	DisplayMaterialInstance->SetScalarParameterValue(TEXT("CanvasOffsetY"), OffsetY);
	DisplayMaterialInstance->SetScalarParameterValue(TEXT("CanvasVisible"), IsCanvasMappingValid() ? 1.0f : 0.0f);
	DisplayMaterialInstance->SetScalarParameterValue(TEXT("SurfaceResolutionX"), static_cast<float>(SurfaceResolution.X));
	DisplayMaterialInstance->SetScalarParameterValue(TEXT("SurfaceResolutionY"), static_cast<float>(SurfaceResolution.Y));

	const TCHAR* SubpixelTexturePath = SubpixelLayout == ETSAVLEDSubpixelLayout::RoundRGB
		? TSAVLEDTools::Private::RoundSubpixelTexturePath
		: TSAVLEDTools::Private::RectangleSubpixelTexturePath;
	if (UTexture* SubpixelTexture = LoadObject<UTexture>(nullptr, SubpixelTexturePath))
	{
		DisplayMaterialInstance->SetTextureParameterValue(TEXT("SubpixelTexture"), SubpixelTexture);
	}
	if (UMediaTexture* Texture = GetMediaTexture())
	{
		DisplayMaterialInstance->SetTextureParameterValue(TEXT("MediaTexture"), Texture);
	}

	DisplaySurface->SetMaterial(0, DisplayMaterialInstance);
	OnDisplayMaterialUpdated(DisplayMaterialInstance);
}

void ATSAVMediaSurfaceActor::UpdatePlayback(bool bForceReopen)
{
	UMediaPlayer* Player = GetMediaPlayer();
	if (!Player)
	{
		return;
	}

	Player->SetLooping(bLoop);
	Player->PlayOnOpen = bAutoPlay;

	if (!MediaSource)
	{
		if (!Player->GetUrl().IsEmpty())
		{
			Player->Close();
		}
		return;
	}

	const UWorld* World = GetWorld();
	const bool bIsGameWorld = World && World->IsGameWorld();
	if (!bIsGameWorld && !bPlayInEditor)
	{
		return;
	}

	if (bForceReopen || Player->GetUrl() != MediaSource->GetUrl())
	{
		Player->OpenSource(MediaSource);
	}
	else if (bAutoPlay && !Player->IsPlaying())
	{
		Player->Play();
	}
}

UMaterialInterface* ATSAVMediaSurfaceActor::ResolveDisplayMaterial() const
{
	if (DisplayMaterial)
	{
		return DisplayMaterial;
	}

	if (UMaterialInterface* BundledMaterial = LoadObject<UMaterialInterface>(nullptr, TSAVLEDTools::Private::DefaultDisplayMaterialPath))
	{
		return BundledMaterial;
	}

	return UMaterial::GetDefaultMaterial(MD_Surface);
}
