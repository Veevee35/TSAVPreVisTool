// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TSAVStateSerializable.h"

#include "TSAVVideoSwitcher.generated.h"

class UMediaSource;
class USceneComponent;
class UStaticMeshComponent;
class UStreamMediaSource;
class UTexture;

UENUM(BlueprintType)
enum class ETSAVVideoInputKind : uint8
{
	MediaAsset UMETA(DisplayName = "Media Source Asset"),
	StreamUrl UMETA(DisplayName = "Stream / NDI URL"),
	CameraFeed UMETA(DisplayName = "Camera Feed"),
};

USTRUCT(BlueprintType)
struct TSAVLEDTOOLS_API FTSAVVideoInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video Input")
	FGuid InputId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video Input")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video Input")
	ETSAVVideoInputKind Kind = ETSAVVideoInputKind::MediaAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video Input")
	TObjectPtr<UMediaSource> MediaSource;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video Input")
	FString StreamUrl;

	/** Stable provider ID used to reconnect camera feeds after loading a project. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video Input")
	FGuid ProviderId;

	UPROPERTY(Transient)
	TObjectPtr<AActor> ProviderActor;
};

USTRUCT(BlueprintType)
struct TSAVLEDTOOLS_API FTSAVVideoBus
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video Bus")
	FName Name = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video Bus")
	FGuid SelectedInputId;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTSAVVideoBusChanged, FName, BusName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTSAVVideoInputsChanged);

/** Runtime video router with discoverable inputs and Program/Preview/Aux buses. */
UCLASS(Blueprintable, meta = (DisplayName = "TSAV Video Switcher"))
class TSAVLEDTOOLS_API ATSAVVideoSwitcher final : public AActor, public ITSAVStateSerializable
{
	GENERATED_BODY()

public:
	ATSAVVideoSwitcher();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TSAV Video|Identity")
	FGuid SwitcherId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TSAV Video|Discovery")
	bool bAutoDiscoverSources = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TSAV Video|Inputs")
	TArray<FTSAVVideoInput> Inputs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TSAV Video|Buses")
	TArray<FTSAVVideoBus> Buses;

	UFUNCTION(BlueprintCallable, Category = "TSAV Video|Inputs")
	int32 DiscoverSources();

	UFUNCTION(BlueprintCallable, Category = "TSAV Video|Inputs")
	FGuid AddStreamInput(const FText& Label, const FString& StreamUrl);

	UFUNCTION(BlueprintCallable, Category = "TSAV Video|Inputs")
	bool RemoveInput(FGuid InputId);

	UFUNCTION(BlueprintCallable, Category = "TSAV Video|Routing")
	bool SetBusInput(FName BusName, FGuid InputId);

	UFUNCTION(BlueprintCallable, Category = "TSAV Video|Routing")
	void Cut();

	UFUNCTION(BlueprintCallable, Category = "TSAV Video|Routing")
	void AutoTransition();

	UFUNCTION(BlueprintPure, Category = "TSAV Video|Routing")
	FGuid GetBusInputId(FName BusName) const;

	UFUNCTION(BlueprintPure, Category = "TSAV Video|Routing")
	FText GetBusInputLabel(FName BusName) const;

	UMediaSource* GetOutputMediaSource(FName BusName);
	UTexture* GetOutputTexture(FName BusName);

	virtual FString CaptureTSAVState() const override;
	virtual bool RestoreTSAVState(const FString& State) override;

	UPROPERTY(BlueprintAssignable, Category = "TSAV Video|Routing")
	FTSAVVideoBusChanged OnBusChanged;

	UPROPERTY(BlueprintAssignable, Category = "TSAV Video|Inputs")
	FTSAVVideoInputsChanged OnInputsChanged;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void NormalizeConfiguration();
	FTSAVVideoInput* FindInput(FGuid InputId);
	const FTSAVVideoInput* FindInput(FGuid InputId) const;
	FTSAVVideoBus* FindBus(FName BusName);
	const FTSAVVideoBus* FindBus(FName BusName) const;
	AActor* ResolveProvider(FTSAVVideoInput& Input);
	UMediaSource* ResolveMediaSource(FTSAVVideoInput& Input);

	UPROPERTY(VisibleAnywhere, Category = "TSAV Video|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "TSAV Video|Components")
	TObjectPtr<UStaticMeshComponent> ConsoleBody;

	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UStreamMediaSource>> RuntimeStreamSources;
};
