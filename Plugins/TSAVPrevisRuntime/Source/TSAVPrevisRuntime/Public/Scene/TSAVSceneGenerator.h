// Copyright TSAV. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TSAVStateSerializable.h"
#include "TSAVSceneGenerator.generated.h"
class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;

UENUM(BlueprintType)
enum class ETSAVScenicKind : uint8 { Stage, Stairs, Truss, TrussRing, Scaffold, Drape, Barrier, Crowd, Lift, Rail };

USTRUCT(BlueprintType)
struct TSAVPREVISRUNTIME_API FTSAVScenicSettings
{
	GENERATED_BODY()
	UPROPERTY() int32 Version=1;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Geometry") ETSAVScenicKind Kind=ETSAVScenicKind::Stage;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Geometry") float Width=600;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Geometry") float Depth=300;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Geometry") float Height=100;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Geometry") float Thickness=10;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Geometry") int32 Columns=6;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Geometry") int32 Rows=3;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Motion") float Position=0;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Motion") bool bAnimate=false;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Motion") float PeriodSeconds=5;
};

/** Original dimension-driven TSAV scenery built from engine primitives and procedural cloth. Units are centimeters. */
UCLASS(BlueprintType)
class TSAVPREVISRUNTIME_API ATSAVSceneGenerator final : public AActor, public ITSAVStateSerializable
{
	GENERATED_BODY()
public:
	ATSAVSceneGenerator();
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="TSAV Builder") FTSAVScenicSettings Settings;
	static FTSAVScenicSettings Preset(ETSAVScenicKind Kind);
	static bool Validate(const FTSAVScenicSettings& Value);
	bool Configure(const FTSAVScenicSettings& Value);
	UFUNCTION(CallInEditor,BlueprintCallable,Category="TSAV Builder") void Rebuild();
	void SetMotionPosition(float Value);
	int32 GetPartCount() const;
	FVector GetMotionOffset() const;
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
	void Box(FVector Center,FVector Size,bool bMoving=false);
	void Rod(FVector From,FVector To,float Diameter);
	void UpdateMotion();
	UPROPERTY() TObjectPtr<USceneComponent> MotionRoot;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Boxes;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Rods;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Spheres;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> MovingBoxes;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Cloth;
	float MotionTime=0;
};
