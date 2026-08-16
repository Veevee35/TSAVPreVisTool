// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TSAVStateSerializable.h"
#include "TSAVVideoSourceProvider.h"

#include "TSAVCameraActor.generated.h"

class FSocket;
class UCineCameraComponent;
class USceneCaptureComponent2D;
class USceneComponent;
class UStaticMeshComponent;
class UTexture;
class UTextureRenderTarget2D;

UENUM(BlueprintType)
enum class ETSAVCameraType : uint8
{
	Broadcast UMETA(DisplayName = "Broadcast / Studio"),
	Cinema UMETA(DisplayName = "Cinema"),
	FullFrame UMETA(DisplayName = "Full Frame / DSLR"),
	PTZ UMETA(DisplayName = "PTZ (VISCA over IP)"),
	Virtual UMETA(DisplayName = "Virtual Camera"),
};

UENUM(BlueprintType)
enum class ETSAVLensPreset : uint8
{
	UltraWide12 UMETA(DisplayName = "12mm Ultra Wide"),
	Wide18 UMETA(DisplayName = "18mm Wide"),
	Wide24 UMETA(DisplayName = "24mm Wide"),
	Standard35 UMETA(DisplayName = "35mm Standard"),
	Standard50 UMETA(DisplayName = "50mm Standard"),
	Portrait85 UMETA(DisplayName = "85mm Portrait"),
	Telephoto135 UMETA(DisplayName = "135mm Telephoto"),
	BroadcastZoom UMETA(DisplayName = "Broadcast 8-120mm Zoom"),
	PTZZoom UMETA(DisplayName = "PTZ 4-80mm Zoom"),
	Custom UMETA(DisplayName = "Custom"),
};

/** Configurable render-capable camera with optional Sony VISCA-over-IP PTZ output. */
UCLASS(Blueprintable, meta = (DisplayName = "TSAV Production Camera"))
class TSAVPREVISRUNTIME_API ATSAVCameraActor final : public AActor, public ITSAVStateSerializable, public ITSAVVideoSourceProvider
{
	GENERATED_BODY()

public:
	ATSAVCameraActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TSAV Camera|Identity")
	FGuid CameraId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Camera|Identity")
	FText CameraLabel = NSLOCTEXT("TSAVCamera", "DefaultCameraLabel", "CAM 1");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Camera|Configuration")
	ETSAVCameraType CameraType = ETSAVCameraType::Broadcast;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Camera|Lens")
	ETSAVLensPreset LensPreset = ETSAVLensPreset::BroadcastZoom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Camera|Lens", meta = (ClampMin = "1.0", ClampMax = "2000.0"))
	float FocalLengthMm = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Camera|Lens", meta = (ClampMin = "0.7", ClampMax = "64.0"))
	float Aperture = 2.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Camera|Lens", meta = (ClampMin = "1.0"))
	float FocusDistanceCm = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Camera|Video Output")
	FIntPoint OutputResolution = FIntPoint(1280, 720);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Camera|Video Output")
	bool bEnableVideoOutput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Camera|PTZ")
	float PanDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Camera|PTZ")
	float TiltDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Camera|PTZ", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ZoomNormalized = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Camera|VISCA over IP")
	bool bEnableViscaOverIp = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Camera|VISCA over IP")
	FString ViscaIpAddress = TEXT("192.168.1.100");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Camera|VISCA over IP", meta = (ClampMin = "1", ClampMax = "65535"))
	int32 ViscaPort = 52381;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Camera|VISCA over IP", meta = (ClampMin = "1", ClampMax = "24"))
	int32 ViscaPanSpeed = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Camera|VISCA over IP", meta = (ClampMin = "1", ClampMax = "20"))
	int32 ViscaTiltSpeed = 10;

	UFUNCTION(BlueprintCallable, Category = "TSAV Camera|Configuration")
	void SetCameraType(ETSAVCameraType NewType);

	UFUNCTION(BlueprintCallable, Category = "TSAV Camera|Lens")
	void SetLensPreset(ETSAVLensPreset NewPreset);

	UFUNCTION(BlueprintCallable, Category = "TSAV Camera|Lens")
	void SetLens(float NewFocalLengthMm, float NewAperture, float NewFocusDistanceCm);

	UFUNCTION(BlueprintCallable, Category = "TSAV Camera|PTZ")
	void ApplyPTZ(float NewPanDegrees, float NewTiltDegrees, float NewZoomNormalized, bool bSendVisca = true);

	UFUNCTION(BlueprintCallable, Category = "TSAV Camera|PTZ")
	bool SendViscaHome();

	UFUNCTION(BlueprintCallable, Category = "TSAV Camera|PTZ")
	bool SendViscaStop();

	UFUNCTION(BlueprintPure, Category = "TSAV Camera|Components")
	UCineCameraComponent* GetCineCameraComponent() const { return CineCamera; }

	virtual FGuid GetTSAVVideoSourceId() const override { return CameraId; }
	virtual FText GetTSAVVideoSourceName() const override;
	virtual UTexture* GetTSAVVideoTexture() const override;
	virtual FString CaptureTSAVState() const override;
	virtual bool RestoreTSAVState(const FString& State) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void ApplyCameraConfiguration();
	void ApplyPTZPreview();
	void UpdateRenderTarget();
	bool SendViscaPayload(const TArray<uint8>& Payload);
	bool SendViscaPanTilt();
	bool SendViscaZoom();
	void CloseViscaSocket();
	static float GetPresetFocalLength(ETSAVLensPreset Preset);

	UPROPERTY(VisibleAnywhere, Category = "TSAV Camera|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "TSAV Camera|Components")
	TObjectPtr<UStaticMeshComponent> BaseVisual;

	UPROPERTY(VisibleAnywhere, Category = "TSAV Camera|Components")
	TObjectPtr<USceneComponent> PanPivot;

	UPROPERTY(VisibleAnywhere, Category = "TSAV Camera|Components")
	TObjectPtr<USceneComponent> TiltPivot;

	UPROPERTY(VisibleAnywhere, Category = "TSAV Camera|Components")
	TObjectPtr<UStaticMeshComponent> BodyVisual;

	UPROPERTY(VisibleAnywhere, Category = "TSAV Camera|Components")
	TObjectPtr<UStaticMeshComponent> LensVisual;

	UPROPERTY(VisibleAnywhere, Category = "TSAV Camera|Components")
	TObjectPtr<UCineCameraComponent> CineCamera;

	UPROPERTY(VisibleAnywhere, Category = "TSAV Camera|Components")
	TObjectPtr<USceneCaptureComponent2D> SceneCapture;

	UPROPERTY(VisibleAnywhere, Category = "TSAV Camera|Components")
	TObjectPtr<UTextureRenderTarget2D> VideoRenderTarget;

	FSocket* ViscaSocket = nullptr;
	uint32 ViscaSequenceNumber = 1;
};
