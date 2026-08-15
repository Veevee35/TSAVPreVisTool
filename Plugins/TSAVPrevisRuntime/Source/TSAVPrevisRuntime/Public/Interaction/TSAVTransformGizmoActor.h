// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TSAVTransformGizmoActor.generated.h"

class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ETSAVTransformMode : uint8
{
	Translate,
	Rotate,
	Scale,
};

UENUM(BlueprintType)
enum class ETSAVCoordinateSpace : uint8
{
	World,
	Local,
};

/** Runtime-only three-axis transform handle used by the packaged authoring viewport. */
UCLASS(NotBlueprintable, Transient)
class TSAVPREVISRUNTIME_API ATSAVTransformGizmoActor final : public AActor
{
	GENERATED_BODY()

public:
	ATSAVTransformGizmoActor();

	virtual void Tick(float DeltaSeconds) override;

	void SetTargetActor(AActor* Actor);
	AActor* GetTargetActor() const { return TargetActor.Get(); }
	void SetTransformMode(ETSAVTransformMode NewMode);
	ETSAVTransformMode GetTransformMode() const { return TransformMode; }
	void SetCoordinateSpace(ETSAVCoordinateSpace NewSpace);
	ETSAVCoordinateSpace GetCoordinateSpace() const { return CoordinateSpace; }
	bool GetAxisForComponent(const UPrimitiveComponent* Component, int32& OutAxisIndex, FVector& OutWorldAxis) const;

private:
	void UpdateFromTarget();
	void UpdateHandleAppearance();
	UStaticMeshComponent* CreateAxisHandle(const FName& Name, const FLinearColor& Color);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> AxisX;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> AxisY;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> AxisZ;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> AxisMaterials;

	TWeakObjectPtr<AActor> TargetActor;
	ETSAVTransformMode TransformMode = ETSAVTransformMode::Translate;
	ETSAVCoordinateSpace CoordinateSpace = ETSAVCoordinateSpace::World;
};
