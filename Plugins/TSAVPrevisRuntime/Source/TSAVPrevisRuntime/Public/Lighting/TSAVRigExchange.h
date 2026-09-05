// Copyright TSAV. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Library/DMXEntityFixtureType.h"
class ATSAVDMXFixture;
class UWorld;

struct TSAVPREVISRUNTIME_API FTSAVGDTFMode
{
	FDMXFixtureMode DMX;
	TMap<FName,FVector2D> PhysicalRanges;
};
struct TSAVPREVISRUNTIME_API FTSAVGDTFProfile
{
	FGuid Id;
	FString Name, Manufacturer, FileName;
	TArray64<uint8> Bytes;
	TArray<FTSAVGDTFMode> Modes;
	TArray<FString> Warnings;
};
struct TSAVPREVISRUNTIME_API FTSAVRigFixture
{
	FGuid Id;
	FString Name, Profile, Mode;
	FTransform Transform;
	int32 Universe=1, Address=1;
};
struct TSAVPREVISRUNTIME_API FTSAVRigDocument
{
	TArray<FTSAVRigFixture> Fixtures;
	TMap<FString,FTSAVGDTFProfile> Profiles;
	TArray<FString> Warnings;
};

/** Standard-file rig transfer and runtime profile creation. Does not load vendor modules. */
namespace TSAVRigExchange
{
	TSAVPREVISRUNTIME_API bool ReadGDTF(const TArray64<uint8>& Bytes,const FString& FileName,FTSAVGDTFProfile& Profile,FString& Error);
	TSAVPREVISRUNTIME_API bool LoadGDTF(const FString& Path,FTSAVGDTFProfile& Profile,FString& Error);
	TSAVPREVISRUNTIME_API bool GenerateGDTF(ATSAVDMXFixture* Fixture,TArray64<uint8>& Bytes,FString& Error);
	TSAVPREVISRUNTIME_API bool ReadMVR(const FString& Path,FTSAVRigDocument& Document,FString& Error);
	TSAVPREVISRUNTIME_API bool ExportMVR(const FString& Path,const TArray<ATSAVDMXFixture*>& Fixtures,FString& Error);
	TSAVPREVISRUNTIME_API bool ApplyProfile(ATSAVDMXFixture* Fixture,const FTSAVGDTFProfile& Profile,const FString& Mode,int32 Universe,int32 Address,FString& Error);
	TSAVPREVISRUNTIME_API bool ImportRig(UWorld* World,const FTSAVRigDocument& Document,TArray<ATSAVDMXFixture*>& Created,FString& Error);
	TSAVPREVISRUNTIME_API bool PlaceGrid(UWorld* World,ATSAVDMXFixture* Template,const FTSAVGDTFProfile* Profile,int32 ModeIndex,FIntPoint Count,FVector2D Spacing,const FTransform& Origin,int32 Universe,int32 Address,TArray<ATSAVDMXFixture*>& Created,FString& Error);
	TSAVPREVISRUNTIME_API bool ParseMatrix(const FString& Text,FTransform& Transform);
	TSAVPREVISRUNTIME_API FString WriteMatrix(const FTransform& Transform);
}
