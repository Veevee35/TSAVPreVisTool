// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "TSAVProjectSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTSAVProjectChanged);

/** Owns the active document identity and application-wide dirty state. */
UCLASS()
class TSAVPREVISRUNTIME_API UTSAVProjectSubsystem final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Project")
	void NewProject(const FString& InProjectName = TEXT("Untitled Show"));

	UFUNCTION(BlueprintCallable, Category = "TSAV PreVis|Project")
	void MarkDirty(bool bInDirty = true);

	UFUNCTION(BlueprintPure, Category = "TSAV PreVis|Project")
	FGuid GetProjectId() const { return ProjectId; }

	UFUNCTION(BlueprintPure, Category = "TSAV PreVis|Project")
	FString GetProjectName() const { return ProjectName; }

	UFUNCTION(BlueprintPure, Category = "TSAV PreVis|Project")
	bool IsDirty() const { return bDirty; }

	UPROPERTY(BlueprintAssignable, Category = "TSAV PreVis|Project")
	FTSAVProjectChanged OnProjectChanged;

private:
	UPROPERTY(VisibleAnywhere, Category = "TSAV PreVis|Project")
	FGuid ProjectId;

	UPROPERTY(VisibleAnywhere, Category = "TSAV PreVis|Project")
	FString ProjectName;

	UPROPERTY(VisibleAnywhere, Category = "TSAV PreVis|Project")
	bool bDirty = false;
};
