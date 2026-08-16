// Copyright TSAV. All Rights Reserved.

#include "Video/TSAVCameraActor.h"

#include "Camera/CameraComponent.h"
#include "CineCameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interaction/TSAVSceneObjectComponent.h"
#include "Materials/Material.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "TSAVVideoSwitcher.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVCameraActor)

ATSAVCameraActor::ATSAVCameraActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	BaseVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Camera Base"));
	BaseVisual->SetupAttachment(SceneRoot);
	PanPivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pan Pivot"));
	PanPivot->SetupAttachment(SceneRoot);
	TiltPivot = CreateDefaultSubobject<USceneComponent>(TEXT("Tilt Pivot"));
	TiltPivot->SetupAttachment(PanPivot);
	BodyVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Camera Body"));
	BodyVisual->SetupAttachment(TiltPivot);
	LensVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Camera Lens"));
	LensVisual->SetupAttachment(TiltPivot);
	CineCamera = CreateDefaultSubobject<UCineCameraComponent>(TEXT("Cine Camera"));
	CineCamera->SetupAttachment(TiltPivot);
	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Video Output"));
	SceneCapture->SetupAttachment(TiltPivot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BaseVisual->SetStaticMesh(CubeMesh.Object);
		BodyVisual->SetStaticMesh(CubeMesh.Object);
		LensVisual->SetStaticMesh(CubeMesh.Object);
	}
	BaseVisual->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.08f));
	BaseVisual->SetRelativeLocation(FVector(0.0f, 0.0f, -22.0f));
	BodyVisual->SetRelativeScale3D(FVector(0.32f, 0.22f, 0.20f));
	LensVisual->SetRelativeScale3D(FVector(0.25f, 0.10f, 0.10f));
	LensVisual->SetRelativeLocation(FVector(38.0f, 0.0f, 0.0f));
	CineCamera->SetRelativeLocation(FVector(50.0f, 0.0f, 0.0f));
	SceneCapture->SetRelativeLocation(FVector(50.0f, 0.0f, 0.0f));
	SceneCapture->bCaptureEveryFrame = true;
	SceneCapture->bCaptureOnMovement = true;
}

void ATSAVCameraActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyCameraConfiguration();
}

void ATSAVCameraActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseViscaSocket();
	Super::EndPlay(EndPlayReason);
}

void ATSAVCameraActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!CameraId.IsValid() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		CameraId = FGuid::NewGuid();
	}
	ApplyCameraConfiguration();
	if (CameraId.IsValid() && GetWorld())
	{
		// A routed wall may have constructed before this transient capture target
		// existed. Republish affected buses once the camera output is ready.
		for (TActorIterator<ATSAVVideoSwitcher> It(GetWorld()); It; ++It)
		{
			It->RefreshOutputsForProvider(CameraId);
		}
	}
}

void ATSAVCameraActor::Destroyed()
{
	if (CameraId.IsValid() && GetWorld())
	{
		for (TActorIterator<ATSAVVideoSwitcher> It(GetWorld()); It; ++It)
		{
			It->Modify();
			It->RemoveProviderInputs(CameraId);
		}
	}
	Super::Destroyed();
}

void ATSAVCameraActor::SetCameraType(const ETSAVCameraType NewType)
{
	CameraType = NewType;
	if (NewType == ETSAVCameraType::PTZ && LensPreset != ETSAVLensPreset::PTZZoom)
	{
		LensPreset = ETSAVLensPreset::PTZZoom;
	}
	ApplyCameraConfiguration();
}

void ATSAVCameraActor::SetLensPreset(const ETSAVLensPreset NewPreset)
{
	LensPreset = NewPreset;
	if (NewPreset != ETSAVLensPreset::Custom)
	{
		FocalLengthMm = GetPresetFocalLength(NewPreset);
	}
	ApplyCameraConfiguration();
}

void ATSAVCameraActor::SetLens(const float NewFocalLengthMm, const float NewAperture, const float NewFocusDistanceCm)
{
	LensPreset = ETSAVLensPreset::Custom;
	FocalLengthMm = FMath::Clamp(NewFocalLengthMm, 1.0f, 2000.0f);
	Aperture = FMath::Clamp(NewAperture, 0.7f, 64.0f);
	FocusDistanceCm = FMath::Max(NewFocusDistanceCm, 1.0f);
	ApplyCameraConfiguration();
}

void ATSAVCameraActor::ApplyCameraConfiguration()
{
	if (!CineCamera || !SceneCapture)
	{
		return;
	}
	FCameraFilmbackSettings Filmback;
	switch (CameraType)
	{
	case ETSAVCameraType::Broadcast: Filmback.SensorWidth = 9.6f; Filmback.SensorHeight = 5.4f; break;
	case ETSAVCameraType::Cinema: Filmback.SensorWidth = 24.89f; Filmback.SensorHeight = 14.0f; break;
	case ETSAVCameraType::FullFrame: Filmback.SensorWidth = 36.0f; Filmback.SensorHeight = 24.0f; break;
	case ETSAVCameraType::PTZ: Filmback.SensorWidth = 6.4f; Filmback.SensorHeight = 3.6f; break;
	case ETSAVCameraType::Virtual: Filmback.SensorWidth = 36.0f; Filmback.SensorHeight = 20.25f; break;
	}
	Filmback.SensorAspectRatio = Filmback.SensorWidth / FMath::Max(Filmback.SensorHeight, 0.01f);
	CineCamera->SetFilmback(Filmback);
	if (LensPreset != ETSAVLensPreset::Custom)
	{
		FocalLengthMm = GetPresetFocalLength(LensPreset);
	}
	if (CameraType == ETSAVCameraType::PTZ || LensPreset == ETSAVLensPreset::PTZZoom || LensPreset == ETSAVLensPreset::BroadcastZoom)
	{
		const float MinFocal = CameraType == ETSAVCameraType::PTZ || LensPreset == ETSAVLensPreset::PTZZoom ? 4.0f : 8.0f;
		const float MaxFocal = CameraType == ETSAVCameraType::PTZ || LensPreset == ETSAVLensPreset::PTZZoom ? 80.0f : 120.0f;
		FocalLengthMm = FMath::Lerp(MinFocal, MaxFocal, FMath::Clamp(ZoomNormalized, 0.0f, 1.0f));
	}
	CineCamera->SetCurrentFocalLength(FocalLengthMm);
	CineCamera->SetCurrentAperture(Aperture);
	FCameraFocusSettings Focus = CineCamera->FocusSettings;
	Focus.FocusMethod = ECameraFocusMethod::Manual;
	Focus.ManualFocusDistance = FocusDistanceCm;
	CineCamera->SetFocusSettings(Focus);
	SceneCapture->FOVAngle = CineCamera->GetHorizontalFieldOfView();
	SceneCapture->SetVisibility(bEnableVideoOutput);
	SceneCapture->SetComponentTickEnabled(bEnableVideoOutput);
	UpdateRenderTarget();
	ApplyPTZPreview();
}

void ATSAVCameraActor::UpdateRenderTarget()
{
	EnsureRenderTarget();
	const int32 Width = FMath::Clamp(OutputResolution.X, 160, 3840);
	const int32 Height = FMath::Clamp(OutputResolution.Y, 90, 2160);
	if (VideoRenderTarget->SizeX != Width || VideoRenderTarget->SizeY != Height)
	{
		VideoRenderTarget->ResizeTarget(Width, Height);
	}
	if (SceneCapture)
	{
		SceneCapture->TextureTarget = VideoRenderTarget;
	}
}

void ATSAVCameraActor::EnsureRenderTarget()
{
	// UTextureRenderTarget2D owns editor-only AssetImportData. Making it a
	// default subobject causes level instances to retain a private CDO reference,
	// which Unreal correctly rejects when saving the map. A transient object is
	// the canonical scene-capture target and is recreated whenever the actor is
	// constructed or loaded.
	if (!VideoRenderTarget || !VideoRenderTarget->HasAnyFlags(RF_Transient) || VideoRenderTarget->GetOuter() != this)
	{
		VideoRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("TSAV Camera Video Render Target"), RF_Transient);
		VideoRenderTarget->RenderTargetFormat = RTF_RGBA8;
		VideoRenderTarget->ClearColor = FLinearColor::Black;
		const int32 Width = FMath::Clamp(OutputResolution.X, 160, 3840);
		const int32 Height = FMath::Clamp(OutputResolution.Y, 90, 2160);
		VideoRenderTarget->InitAutoFormat(Width, Height);
	}
}

void ATSAVCameraActor::ApplyPTZ(const float NewPanDegrees, const float NewTiltDegrees, const float NewZoomNormalized, const bool bSendVisca)
{
	PanDegrees = FMath::Clamp(NewPanDegrees, -170.0f, 170.0f);
	TiltDegrees = FMath::Clamp(NewTiltDegrees, -30.0f, 90.0f);
	ZoomNormalized = FMath::Clamp(NewZoomNormalized, 0.0f, 1.0f);
	ApplyCameraConfiguration();
	if (bSendVisca && bEnableViscaOverIp)
	{
		SendViscaPanTilt();
		SendViscaZoom();
	}
}

void ATSAVCameraActor::ApplyPTZPreview()
{
	if (PanPivot) { PanPivot->SetRelativeRotation(FRotator(0.0f, PanDegrees, 0.0f)); }
	if (TiltPivot) { TiltPivot->SetRelativeRotation(FRotator(TiltDegrees, 0.0f, 0.0f)); }
}

bool ATSAVCameraActor::SendViscaPanTilt()
{
	const int32 PanPosition = FMath::RoundToInt(FMath::GetMappedRangeValueClamped(FVector2D(-170.0, 170.0), FVector2D(-2448.0, 2448.0), PanDegrees));
	const int32 TiltPosition = FMath::RoundToInt(FMath::GetMappedRangeValueClamped(FVector2D(-30.0, 90.0), FVector2D(-432.0, 1296.0), TiltDegrees));
	auto AppendNibbles = [](TArray<uint8>& Bytes, const int32 Value)
	{
		const uint16 Packed = static_cast<uint16>(Value);
		Bytes.Add((Packed >> 12) & 0x0f);
		Bytes.Add((Packed >> 8) & 0x0f);
		Bytes.Add((Packed >> 4) & 0x0f);
		Bytes.Add(Packed & 0x0f);
	};
	TArray<uint8> Payload = { 0x81, 0x01, 0x06, 0x02, static_cast<uint8>(FMath::Clamp(ViscaPanSpeed, 1, 24)), static_cast<uint8>(FMath::Clamp(ViscaTiltSpeed, 1, 20)) };
	AppendNibbles(Payload, PanPosition);
	AppendNibbles(Payload, TiltPosition);
	Payload.Add(0xff);
	return SendViscaPayload(Payload);
}

bool ATSAVCameraActor::SendViscaZoom()
{
	const uint16 Position = static_cast<uint16>(FMath::RoundToInt(FMath::Clamp(ZoomNormalized, 0.0f, 1.0f) * 0x4000));
	const TArray<uint8> Payload = {
		0x81, 0x01, 0x04, 0x47,
		static_cast<uint8>((Position >> 12) & 0x0f), static_cast<uint8>((Position >> 8) & 0x0f),
		static_cast<uint8>((Position >> 4) & 0x0f), static_cast<uint8>(Position & 0x0f), 0xff };
	return SendViscaPayload(Payload);
}

bool ATSAVCameraActor::SendViscaHome()
{
	return bEnableViscaOverIp && SendViscaPayload({ 0x81, 0x01, 0x06, 0x04, 0xff });
}

bool ATSAVCameraActor::SendViscaStop()
{
	return bEnableViscaOverIp && SendViscaPayload({ 0x81, 0x01, 0x06, 0x01, 0x00, 0x00, 0x03, 0x03, 0xff });
}

bool ATSAVCameraActor::SendViscaPayload(const TArray<uint8>& Payload)
{
	FIPv4Address Address;
	if (Payload.IsEmpty() || !FIPv4Address::Parse(ViscaIpAddress, Address))
	{
		return false;
	}
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		return false;
	}
	if (!ViscaSocket)
	{
		ViscaSocket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("TSAV VISCA over IP"), false);
		if (!ViscaSocket)
		{
			return false;
		}
		ViscaSocket->SetNonBlocking(true);
	}
	TArray<uint8> Packet;
	Packet.Reserve(Payload.Num() + 8);
	Packet.Add(0x01);
	Packet.Add(0x00);
	Packet.Add(static_cast<uint8>((Payload.Num() >> 8) & 0xff));
	Packet.Add(static_cast<uint8>(Payload.Num() & 0xff));
	Packet.Add(static_cast<uint8>((ViscaSequenceNumber >> 24) & 0xff));
	Packet.Add(static_cast<uint8>((ViscaSequenceNumber >> 16) & 0xff));
	Packet.Add(static_cast<uint8>((ViscaSequenceNumber >> 8) & 0xff));
	Packet.Add(static_cast<uint8>(ViscaSequenceNumber & 0xff));
	Packet.Append(Payload);
	++ViscaSequenceNumber;
	TSharedRef<FInternetAddr> Remote = SocketSubsystem->CreateInternetAddr();
	Remote->SetIp(Address.Value);
	Remote->SetPort(FMath::Clamp(ViscaPort, 1, 65535));
	int32 BytesSent = 0;
	return ViscaSocket->SendTo(Packet.GetData(), Packet.Num(), BytesSent, *Remote) && BytesSent == Packet.Num();
}

void ATSAVCameraActor::CloseViscaSocket()
{
	if (ViscaSocket)
	{
		if (ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
		{
			SocketSubsystem->DestroySocket(ViscaSocket);
		}
		ViscaSocket = nullptr;
	}
}

UTexture* ATSAVCameraActor::GetTSAVVideoTexture() const
{
	return bEnableVideoOutput ? VideoRenderTarget : nullptr;
}

FText ATSAVCameraActor::GetTSAVVideoSourceName() const
{
	if (const UTSAVSceneObjectComponent* SceneObject = FindComponentByClass<UTSAVSceneObjectComponent>())
	{
		if (!SceneObject->DisplayName.IsEmpty())
		{
			return SceneObject->DisplayName;
		}
	}
	return CameraLabel;
}

float ATSAVCameraActor::GetPresetFocalLength(const ETSAVLensPreset Preset)
{
	switch (Preset)
	{
	case ETSAVLensPreset::UltraWide12: return 12.0f;
	case ETSAVLensPreset::Wide18: return 18.0f;
	case ETSAVLensPreset::Wide24: return 24.0f;
	case ETSAVLensPreset::Standard35: return 35.0f;
	case ETSAVLensPreset::Standard50: return 50.0f;
	case ETSAVLensPreset::Portrait85: return 85.0f;
	case ETSAVLensPreset::Telephoto135: return 135.0f;
	case ETSAVLensPreset::BroadcastZoom: return 35.0f;
	case ETSAVLensPreset::PTZZoom: return 20.0f;
	case ETSAVLensPreset::Custom: return 35.0f;
	}
	return 35.0f;
}

FString ATSAVCameraActor::CaptureTSAVState() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("cameraId"), CameraId.ToString(EGuidFormats::DigitsWithHyphens));
	Root->SetStringField(TEXT("label"), CameraLabel.ToString());
	Root->SetNumberField(TEXT("type"), static_cast<uint8>(CameraType));
	Root->SetNumberField(TEXT("lensPreset"), static_cast<uint8>(LensPreset));
	Root->SetNumberField(TEXT("focalLength"), FocalLengthMm);
	Root->SetNumberField(TEXT("aperture"), Aperture);
	Root->SetNumberField(TEXT("focusDistance"), FocusDistanceCm);
	Root->SetNumberField(TEXT("outputWidth"), OutputResolution.X);
	Root->SetNumberField(TEXT("outputHeight"), OutputResolution.Y);
	Root->SetBoolField(TEXT("videoOutput"), bEnableVideoOutput);
	Root->SetNumberField(TEXT("pan"), PanDegrees);
	Root->SetNumberField(TEXT("tilt"), TiltDegrees);
	Root->SetNumberField(TEXT("zoom"), ZoomNormalized);
	Root->SetBoolField(TEXT("viscaEnabled"), bEnableViscaOverIp);
	Root->SetStringField(TEXT("viscaIp"), ViscaIpAddress);
	Root->SetNumberField(TEXT("viscaPort"), ViscaPort);
	Root->SetNumberField(TEXT("panSpeed"), ViscaPanSpeed);
	Root->SetNumberField(TEXT("tiltSpeed"), ViscaTiltSpeed);
	FString Result;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Result));
	return Result;
}

bool ATSAVCameraActor::RestoreTSAVState(const FString& State)
{
	TSharedPtr<FJsonObject> Root;
	if (State.IsEmpty() || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(State), Root) || !Root)
	{
		return false;
	}
	FGuid::Parse(Root->GetStringField(TEXT("cameraId")), CameraId);
	CameraLabel = FText::FromString(Root->GetStringField(TEXT("label")));
	CameraType = static_cast<ETSAVCameraType>(Root->GetIntegerField(TEXT("type")));
	LensPreset = static_cast<ETSAVLensPreset>(Root->GetIntegerField(TEXT("lensPreset")));
	FocalLengthMm = Root->GetNumberField(TEXT("focalLength"));
	Aperture = Root->GetNumberField(TEXT("aperture"));
	FocusDistanceCm = Root->GetNumberField(TEXT("focusDistance"));
	OutputResolution.X = Root->GetIntegerField(TEXT("outputWidth"));
	OutputResolution.Y = Root->GetIntegerField(TEXT("outputHeight"));
	bEnableVideoOutput = Root->GetBoolField(TEXT("videoOutput"));
	PanDegrees = Root->GetNumberField(TEXT("pan"));
	TiltDegrees = Root->GetNumberField(TEXT("tilt"));
	ZoomNormalized = Root->GetNumberField(TEXT("zoom"));
	bEnableViscaOverIp = Root->GetBoolField(TEXT("viscaEnabled"));
	ViscaIpAddress = Root->GetStringField(TEXT("viscaIp"));
	ViscaPort = Root->GetIntegerField(TEXT("viscaPort"));
	ViscaPanSpeed = Root->GetIntegerField(TEXT("panSpeed"));
	ViscaTiltSpeed = Root->GetIntegerField(TEXT("tiltSpeed"));
	ApplyCameraConfiguration();
	return true;
}
