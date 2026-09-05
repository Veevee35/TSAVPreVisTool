// Copyright TSAV. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TSAVStateSerializable.h"
#include "TSAVLaserPreview.generated.h"
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UMaterialInterface;

USTRUCT()
struct TSAVPREVISRUNTIME_API FTSAVLaserPoint
{
	GENERATED_BODY()
	UPROPERTY() FVector Position=FVector::ZeroVector;
	UPROPERTY() FColor Color=FColor::Green;
	UPROPERTY() bool bBlank=false;
};
USTRUCT()
struct TSAVPREVISRUNTIME_API FTSAVLaserFrame
{
	GENERATED_BODY()
	UPROPERTY() FString Name;
	UPROPERTY() int32 Projector=0;
	UPROPERTY() bool b3D=false;
	UPROPERTY() TArray<FTSAVLaserPoint> Points;
};
USTRUCT()
struct TSAVPREVISRUNTIME_API FTSAVLaserData
{
	GENERATED_BODY()
	UPROPERTY() int32 Version=1;
	UPROPERTY() TArray<FTSAVLaserFrame> Frames;
	UPROPERTY() float FramesPerSecond=30;
	UPROPERTY() int32 Projector=0;
	UPROPERTY() float WidthCm=600;
	UPROPERTY() float HeightCm=400;
	UPROPERTY() float DistanceCm=1000;
	UPROPERTY() float LineWidthCm=1;
	UPROPERTY() float Intensity=1;
	UPROPERTY() bool bShowRays=true;
	UPROPERTY() bool bLoop=true;
	UPROPERTY() bool bFollowShowTimeline=false;
	UPROPERTY() TSoftObjectPtr<UMaterialInterface> Material=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/TSAV/Materials/M_TSAVLaserPreview.M_TSAVLaserPreview")));
};

/** Original ILDA interchange implementation, independent of any vendor plugin. */
namespace TSAVILDA
{
	TSAVPREVISRUNTIME_API bool Decode(const TArray<uint8>& Bytes,TArray<FTSAVLaserFrame>& Frames,FString& Error);
	TSAVPREVISRUNTIME_API bool Encode(const TArray<FTSAVLaserFrame>& Frames,TArray<uint8>& Bytes,FString& Error,bool bIndexed=false);
	TSAVPREVISRUNTIME_API TArray<FTSAVLaserFrame> MakePattern(int32 Pattern,int32 FrameCount=60,int32 PointCount=128);
}

UCLASS(BlueprintType)
class TSAVPREVISRUNTIME_API ATSAVLaserPreview final : public AActor, public ITSAVStateSerializable
{
	GENERATED_BODY()
public:
	ATSAVLaserPreview();
	const FTSAVLaserData& GetData() const { return Data; }
	static bool Validate(const FTSAVLaserData& Candidate);
	bool Configure(const FTSAVLaserData& Candidate);
	bool ImportILDA(const FString& Path,FString& Error);
	bool ExportILDA(const FString& Path,FString& Error,bool bIndexed=false) const;
	void Play() { bPlaying=true; }
	void Pause() { bPlaying=false; }
	void Seek(float Seconds);
	bool IsPlaying() const { return bPlaying; }
	float GetTime() const { return Time; }
	int32 GetSegmentCount() const;
	virtual FString CaptureTSAVState() const override;
	virtual bool RestoreTSAVState(const FString& State) override;
protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif
private:
	void RebuildFrame();
	UPROPERTY() FTSAVLaserData Data;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Lines;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Housing;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BeamMaterial;
	float Time=0;
	int32 LastFrame=INDEX_NONE;
	bool bPlaying=false;
};
