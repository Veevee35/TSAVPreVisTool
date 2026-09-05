// Copyright TSAV. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TSAVDMXNetworkWidget.generated.h"
struct TSAVPREVISRUNTIME_API FTSAVDMXConnection
{
	FGuid Id;
	FString Name=TEXT("TSAV Port"),Adapter=TEXT("127.0.0.1"),Destination=TEXT("127.0.0.1");
	bool bInput=false,bSACN=false,bUnicast=true,bLoopback=true;
	int32 LocalUniverse=1,ExternalUniverse=1,Universes=1,Priority=100;
};
namespace TSAVDMXNetwork
{
	TSAVPREVISRUNTIME_API bool Validate(const FTSAVDMXConnection& Connection,FString& Error);
	TSAVPREVISRUNTIME_API bool Apply(UWorld* World,FTSAVDMXConnection& Connection,FString& Error);
}
TSAVPREVISRUNTIME_API TSharedRef<SWidget> MakeTSAVDMXNetworkPanel(UWorld* World,FSimpleDelegate Close);
UCLASS()
class TSAVPREVISRUNTIME_API UTSAVDMXNetworkWidget final : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
