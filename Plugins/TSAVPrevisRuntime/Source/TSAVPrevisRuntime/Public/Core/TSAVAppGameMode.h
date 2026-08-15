// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "TSAVAppGameMode.generated.h"

/** Game mode for the packaged TSAV desktop application. */
UCLASS()
class TSAVPREVISRUNTIME_API ATSAVAppGameMode final : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATSAVAppGameMode();

	virtual void StartPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

private:
	bool ShouldCreateApplicationEnvironment() const;
	void CreateApplicationEnvironment();
};
