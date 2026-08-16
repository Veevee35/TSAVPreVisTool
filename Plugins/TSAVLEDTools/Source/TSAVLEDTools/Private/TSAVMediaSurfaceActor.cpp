// Copyright TSAV. All Rights Reserved.

#include "TSAVMediaSurfaceActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/CollisionProfile.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MediaComponent.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TSAVVideoSwitcher.h"
#include "EngineUtils.h"
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

	// Constructor helpers create native hard references that Unreal's cooker can
	// follow without forcing every asset in every enabled plugin into the build.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DisplayMaterialAsset(TSAVLEDTools::Private::DefaultDisplayMaterialPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FrameMaterialAsset(TSAVLEDTools::Private::DefaultFrameMaterialPath);
	static ConstructorHelpers::FObjectFinder<UTexture> RectangleSubpixelAsset(TSAVLEDTools::Private::RectangleSubpixelTexturePath);
	static ConstructorHelpers::FObjectFinder<UTexture> RoundSubpixelAsset(TSAVLEDTools::Private::RoundSubpixelTexturePath);
	if (DisplayMaterialAsset.Succeeded()) { DisplayMaterial = DisplayMaterialAsset.Object; }
	if (FrameMaterialAsset.Succeeded()) { FrameMaterial = FrameMaterialAsset.Object; }
	if (RectangleSubpixelAsset.Succeeded()) { DefaultRectangleSubpixelTexture = RectangleSubpixelAsset.Object; }
	if (RoundSubpixelAsset.Succeeded()) { DefaultRoundSubpixelTexture = RoundSubpixelAsset.Object; }
}

void ATSAVMediaSurfaceActor::BeginPlay()
{
	Super::BeginPlay();
	ResolveVideoSwitcher();
	ApplyDisplayMaterial();
	UpdatePlayback(true);
}

void ATSAVMediaSurfaceActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (VideoSwitcher)
	{
		VideoSwitcher->OnBusChanged.RemoveDynamic(this, &ATSAVMediaSurfaceActor::HandleSwitcherBusChanged);
	}
	if (UMediaPlayer* Player = GetMediaPlayer())
	{
		Player->Close();
	}

	Super::EndPlay(EndPlayReason);
}

void ATSAVMediaSurfaceActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ResolveVideoSwitcher();
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

		UMediaSource* ActiveSource = ResolveActiveMediaSource();
		if (ActiveSource && Player->GetUrl() != ActiveSource->GetUrl())
		{
			Player->OpenSource(ActiveSource);
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

void ATSAVMediaSurfaceActor::SetVideoRoute(ATSAVVideoSwitcher* Switcher, const FName BusName)
{
	if (VideoSwitcher && VideoSwitcher != Switcher)
	{
		VideoSwitcher->OnBusChanged.RemoveDynamic(this, &ATSAVMediaSurfaceActor::HandleSwitcherBusChanged);
	}
	VideoSwitcher = Switcher;
	VideoSwitcherId = Switcher ? Switcher->SwitcherId : FGuid();
	VideoBusName = BusName.IsNone() ? FName(TEXT("Program")) : BusName;
	bUseVideoSwitcher = Switcher != nullptr;
	if (VideoSwitcher)
	{
		VideoSwitcher->OnBusChanged.AddUniqueDynamic(this, &ATSAVMediaSurfaceActor::HandleSwitcherBusChanged);
	}
	RefreshMedia();
}

void ATSAVMediaSurfaceActor::ClearVideoRoute()
{
	if (VideoSwitcher)
	{
		VideoSwitcher->OnBusChanged.RemoveDynamic(this, &ATSAVMediaSurfaceActor::HandleSwitcherBusChanged);
	}
	VideoSwitcher = nullptr;
	VideoSwitcherId.Invalidate();
	bUseVideoSwitcher = false;
	RefreshMedia();
}

void ATSAVMediaSurfaceActor::HandleSwitcherBusChanged(const FName BusName)
{
	if (bUseVideoSwitcher && BusName.IsEqual(VideoBusName))
	{
		RefreshMedia();
	}
}

ATSAVVideoSwitcher* ATSAVMediaSurfaceActor::ResolveVideoSwitcher()
{
	if (!bUseVideoSwitcher)
	{
		return nullptr;
	}
	if (!IsValid(VideoSwitcher) && VideoSwitcherId.IsValid() && GetWorld())
	{
		for (TActorIterator<ATSAVVideoSwitcher> It(GetWorld()); It; ++It)
		{
			if (It->SwitcherId == VideoSwitcherId)
			{
				VideoSwitcher = *It;
				break;
			}
		}
	}
	if (VideoSwitcher)
	{
		VideoSwitcherId = VideoSwitcher->SwitcherId;
		VideoSwitcher->OnBusChanged.AddUniqueDynamic(this, &ATSAVMediaSurfaceActor::HandleSwitcherBusChanged);
	}
	return VideoSwitcher;
}

UMediaSource* ATSAVMediaSurfaceActor::ResolveActiveMediaSource()
{
	if (bUseVideoSwitcher)
	{
		if (ATSAVVideoSwitcher* Switcher = ResolveVideoSwitcher())
		{
			return Switcher->GetOutputMediaSource(VideoBusName);
		}
		return nullptr;
	}
	return MediaSource;
}

UTexture* ATSAVMediaSurfaceActor::ResolveRoutedTexture()
{
	if (bUseVideoSwitcher)
	{
		if (ATSAVVideoSwitcher* Switcher = ResolveVideoSwitcher())
		{
			return Switcher->GetOutputTexture(VideoBusName);
		}
	}
	return nullptr;
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

	UTexture* SubpixelTexture = SubpixelLayout == ETSAVLEDSubpixelLayout::RoundRGB
		? DefaultRoundSubpixelTexture.Get()
		: DefaultRectangleSubpixelTexture.Get();
	if (SubpixelTexture)
	{
		DisplayMaterialInstance->SetTextureParameterValue(TEXT("SubpixelTexture"), SubpixelTexture);
	}
	UTexture* ActiveTexture = ResolveRoutedTexture();
	if (!ActiveTexture)
	{
		ActiveTexture = GetMediaTexture();
	}
	DisplayedVideoTexture = ActiveTexture;
	if (ActiveTexture)
	{
		DisplayMaterialInstance->SetTextureParameterValue(TEXT("MediaTexture"), ActiveTexture);
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

	UMediaSource* ActiveSource = ResolveActiveMediaSource();
	if (!ActiveSource)
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

	if (bForceReopen || Player->GetUrl() != ActiveSource->GetUrl())
	{
		Player->OpenSource(ActiveSource);
	}
	else if (bAutoPlay && !Player->IsPlaying())
	{
		Player->Play();
	}
}

FString ATSAVMediaSurfaceActor::CaptureTSAVState() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("mediaSource"), MediaSource ? MediaSource->GetPathName() : FString());
	Root->SetBoolField(TEXT("useSwitcher"), bUseVideoSwitcher);
	Root->SetStringField(TEXT("switcherId"), VideoSwitcherId.ToString(EGuidFormats::DigitsWithHyphens));
	Root->SetStringField(TEXT("bus"), VideoBusName.ToString());
	Root->SetBoolField(TEXT("autoPlay"), bAutoPlay);
	Root->SetBoolField(TEXT("loop"), bLoop);
	Root->SetNumberField(TEXT("emissive"), EmissiveStrength);
	Root->SetNumberField(TEXT("canvasWidth"), CanvasResolution.X);
	Root->SetNumberField(TEXT("canvasHeight"), CanvasResolution.Y);
	Root->SetNumberField(TEXT("canvasX"), CanvasPosition.X);
	Root->SetNumberField(TEXT("canvasY"), CanvasPosition.Y);
	Root->SetBoolField(TEXT("canvasMapping"), bUseCanvasMapping);
	FString Result;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Result));
	return Result;
}

bool ATSAVMediaSurfaceActor::RestoreTSAVState(const FString& State)
{
	TSharedPtr<FJsonObject> Root;
	if (State.IsEmpty() || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(State), Root) || !Root)
	{
		return false;
	}
	const FString SourcePath = Root->GetStringField(TEXT("mediaSource"));
	MediaSource = SourcePath.IsEmpty() ? nullptr : LoadObject<UMediaSource>(nullptr, *SourcePath);
	bUseVideoSwitcher = Root->GetBoolField(TEXT("useSwitcher"));
	FGuid::Parse(Root->GetStringField(TEXT("switcherId")), VideoSwitcherId);
	VideoBusName = FName(*Root->GetStringField(TEXT("bus")));
	bAutoPlay = Root->GetBoolField(TEXT("autoPlay"));
	bLoop = Root->GetBoolField(TEXT("loop"));
	EmissiveStrength = Root->GetNumberField(TEXT("emissive"));
	CanvasResolution.X = Root->GetIntegerField(TEXT("canvasWidth"));
	CanvasResolution.Y = Root->GetIntegerField(TEXT("canvasHeight"));
	CanvasPosition.X = Root->GetIntegerField(TEXT("canvasX"));
	CanvasPosition.Y = Root->GetIntegerField(TEXT("canvasY"));
	bUseCanvasMapping = Root->GetBoolField(TEXT("canvasMapping"));
	VideoSwitcher = nullptr;
	ResolveVideoSwitcher();
	RefreshMedia();
	return true;
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
