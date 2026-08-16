// Copyright TSAV. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "TSAVStateSerializable.generated.h"

/** C++ interface for actor-specific state stored in .tsav projects and command snapshots. */
UINTERFACE(MinimalAPI)
class UTSAVStateSerializable : public UInterface
{
	GENERATED_BODY()
};

class TSAVLEDTOOLS_API ITSAVStateSerializable
{
	GENERATED_BODY()

public:
	virtual FString CaptureTSAVState() const = 0;
	virtual bool RestoreTSAVState(const FString& State) = 0;
};
