// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "TSAVVideoSourceProvider.generated.h"

class UTexture;

/** Implemented by runtime actors that can feed a switcher without a Media Source asset. */
UINTERFACE(MinimalAPI)
class UTSAVVideoSourceProvider : public UInterface
{
	GENERATED_BODY()
};

class TSAVLEDTOOLS_API ITSAVVideoSourceProvider
{
	GENERATED_BODY()

public:
	virtual FGuid GetTSAVVideoSourceId() const = 0;
	virtual FText GetTSAVVideoSourceName() const = 0;
	virtual UTexture* GetTSAVVideoTexture() const = 0;
};
