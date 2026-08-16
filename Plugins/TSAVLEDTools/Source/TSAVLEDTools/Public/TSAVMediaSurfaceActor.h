// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TSAVStateSerializable.h"

#include "TSAVMediaSurfaceActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMediaComponent;
class UMediaPlayer;
class UMediaSource;
class UMediaTexture;
class USceneComponent;
class UStaticMeshComponent;
class UTexture;
class ATSAVVideoSwitcher;

/** RGB emitter geometry repeated once per native LED pixel. */
UENUM(BlueprintType)
enum class ETSAVLEDSubpixelLayout : uint8
{
	None UMETA(DisplayName = "Off (Solid Video)"),
	RectangleRGB UMETA(DisplayName = "Rectangle RGB"),
	RoundRGB UMETA(DisplayName = "Round RGB"),
	RoundLinear UMETA(DisplayName = "Round Linear"),
};

/**
 * Shared media playback and display surface used by the TSAV LED actors.
 *
 * MediaSource accepts every Unreal Media Source type, including NDI Media
 * Source assets. Assign a source in the Details panel and either preview it in
 * the editor or let the actor open it automatically during play.
 */
UCLASS(Abstract, Blueprintable)
class TSAVLEDTOOLS_API ATSAVMediaSurfaceActor : public AActor, public ITSAVStateSerializable
{
	GENERATED_BODY()

public:
	ATSAVMediaSurfaceActor();

	/** Source shown on this LED surface. NDI Media Source assets are supported. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Media")
	TObjectPtr<UMediaSource> MediaSource;

	/** Follow a switcher bus instead of locking this surface to one preassigned source. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Media Routing")
	bool bUseVideoSwitcher = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Media Routing", meta = (EditCondition = "bUseVideoSwitcher"))
	TObjectPtr<ATSAVVideoSwitcher> VideoSwitcher;

	/** Stable switcher identity used when loading .tsav projects. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TSAV LED|Media Routing")
	FGuid VideoSwitcherId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Media Routing", meta = (EditCondition = "bUseVideoSwitcher"))
	FName VideoBusName = TEXT("Program");

	/** Open and play MediaSource automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Media")
	bool bAutoPlay = true;

	/** Loop sources that support looping. Live NDI feeds ignore this setting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Media")
	bool bLoop = true;

	/** Allow the assigned source to run while editing the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Media")
	bool bPlayInEditor = true;

	/** Optional material override. It must expose MediaTexture and EmissiveStrength parameters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Look")
	TObjectPtr<UMaterialInterface> DisplayMaterial;

	/** Optional frame material override. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Look")
	TObjectPtr<UMaterialInterface> FrameMaterial;

	/** Brightness multiplier passed to the LED material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Look", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "20.0"))
	float EmissiveStrength = 3.0f;

	/** Simulates the physical RGB emitters visible when an LED wall is viewed close-up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Look")
	ETSAVLEDSubpixelLayout SubpixelLayout = ETSAVLEDSubpixelLayout::None;

	/** Strength of the physical subpixel mask. Zero shows solid video; one shows the full RGB layout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Look", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", EditCondition = "SubpixelLayout != ETSAVLEDSubpixelLayout::None"))
	float SubpixelStrength = 1.0f;

	/** Resolution of the processor canvas carried by the assigned Media Source. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Canvas")
	FIntPoint CanvasResolution = FIntPoint(4096, 2160);

	/** Top-left pixel coordinate of this panel/screen on the processor canvas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Canvas")
	FIntPoint CanvasPosition = FIntPoint::ZeroValue;

	/** Crop the Media Source to this surface's native-resolution rectangle on the canvas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV LED|Canvas")
	bool bUseCanvasMapping = true;

	/** Rebuild the dynamic display material and reopen the assigned source. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "TSAV LED|Media")
	void RefreshMedia();

	/** Start or resume the assigned Media Source. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "TSAV LED|Media")
	void PlayMedia();

	/** Pause the currently open source. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "TSAV LED|Media")
	void PauseMedia();

	/** Close the currently open source. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "TSAV LED|Media")
	void CloseMedia();

	UFUNCTION(BlueprintPure, Category = "TSAV LED|Media")
	UMediaPlayer* GetMediaPlayer() const;

	UFUNCTION(BlueprintPure, Category = "TSAV LED|Media")
	UMediaTexture* GetMediaTexture() const;

	/** Texture currently bound to the LED shader after direct/switcher routing resolves. */
	UFUNCTION(BlueprintPure, Category = "TSAV LED|Media")
	UTexture* GetDisplayedVideoTexture() const { return DisplayedVideoTexture; }

	UFUNCTION(BlueprintCallable, Category = "TSAV LED|Media Routing")
	void SetVideoRoute(ATSAVVideoSwitcher* Switcher, FName BusName);

	UFUNCTION(BlueprintCallable, Category = "TSAV LED|Media Routing")
	void ClearVideoRoute();

	UFUNCTION(BlueprintPure, Category = "TSAV LED|Media Routing")
	ATSAVVideoSwitcher* GetVideoSwitcher() const { return VideoSwitcher; }

	/** Reconnect this surface before its switcher broadcasts a routed bus change. */
	bool EnsureVideoRouteBinding(ATSAVVideoSwitcher* Switcher, FName BusName);

	virtual FString CaptureTSAVState() const override;
	virtual bool RestoreTSAVState(const FString& State) override;

	/** Native pixel resolution calculated by the panel or wall configurator. */
	UFUNCTION(BlueprintPure, Category = "TSAV LED|Canvas")
	FIntPoint GetSurfaceResolutionPixels() const;

	/** True when the full surface rectangle is inside the configured canvas. */
	UFUNCTION(BlueprintPure, Category = "TSAV LED|Canvas")
	bool IsCanvasMappingValid() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Applies the bundled frame material to a mesh component. */
	void ApplyFrameMaterial(UStaticMeshComponent* MeshComponent) const;

	/** Creates a cube component configured for editable previs geometry. */
	UStaticMeshComponent* CreateGeometryComponent(FName ComponentName, bool bEnableCollision);

	UMaterialInterface* ResolveFrameMaterial() const;

	/** Lets derived surfaces reuse the dynamic media material on generated geometry. */
	virtual void OnDisplayMaterialUpdated(UMaterialInterface* AppliedMaterial);

	virtual FIntPoint GetNativePixelResolution() const PURE_VIRTUAL(ATSAVMediaSurfaceActor::GetNativePixelResolution, return FIntPoint(1, 1););

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TSAV LED|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TSAV LED|Components")
	TObjectPtr<UStaticMeshComponent> DisplaySurface;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TSAV LED|Components")
	TObjectPtr<UMediaComponent> MediaComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DisplayMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UTexture> DisplayedVideoTexture;

	/** Native hard references keep the runtime LED shader library in cooked builds. */
	UPROPERTY()
	TObjectPtr<UTexture> DefaultRectangleSubpixelTexture;

	UPROPERTY()
	TObjectPtr<UTexture> DefaultRoundSubpixelTexture;

	UPROPERTY()
	TObjectPtr<UTexture> DefaultRoundLinearSubpixelTexture;

private:
	UFUNCTION()
	void HandleSwitcherBusChanged(FName BusName);

	ATSAVVideoSwitcher* ResolveVideoSwitcher();
	UMediaSource* ResolveActiveMediaSource();
	UTexture* ResolveRoutedTexture();
	void ApplyDisplayMaterial();
	void UpdatePlayback(bool bForceReopen);
	UMaterialInterface* ResolveDisplayMaterial() const;
};
