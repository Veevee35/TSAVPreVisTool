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
	const TCHAR* DefaultDisplayMaterialPath = TEXT("/TSAVLEDTools/Materials/M_TSAV_LEDVideo.M_TSAV_LEDVideo");
	const TCHAR* DefaultFrameMaterialPath = TEXT("/TSAVLEDTools/Materials/M_TSAV_LEDFrame.M_TSAV_LEDFrame");
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
		return;
	}

	DisplayMaterialInstance->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveStrength);
	if (UMediaTexture* Texture = GetMediaTexture())
	{
		DisplayMaterialInstance->SetTextureParameterValue(TEXT("MediaTexture"), Texture);
	}

	DisplaySurface->SetMaterial(0, DisplayMaterialInstance);
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
