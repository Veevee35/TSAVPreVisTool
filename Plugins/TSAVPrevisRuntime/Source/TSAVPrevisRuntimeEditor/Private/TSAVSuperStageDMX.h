// Copyright TSAV. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;

namespace TSAVSuperStageDMX
{
	struct FAttribute
	{
		FName Name;
		int32 Instance = 0;
		int32 Channel = 0;
		int32 Fine = 0;
		int32 Ultra = 0;
	};
	struct FFixture
	{
		TWeakObjectPtr<AActor> Actor;
		int32 FixtureId = 0;
		int32 Universe = 1;
		int32 Address = 1;
		int32 Span = 0;
		TArray<FAttribute> Attributes;
	};
	struct FValue
	{
		FName Attribute;
		float Value = 0.0f;
		int32 Instance = INDEX_NONE;
	};
	bool ReadFixture(AActor* Actor, FFixture& Out);
	TArray<FFixture> GetSceneFixtures(UWorld* World);
	bool SetPatch(AActor* Actor, int32 Universe, int32 Address);
	bool SyncConsole(bool bImportScene);
	bool IsOutputEnabled();
	bool SetOutputEnabled(bool bEnabled);
	/** Resolve the vendor console's internal ID through ActorGuid, never the scene's FixtureID. */
	int32 GetConsoleFixtureId(AActor* Actor);
	bool SendAttributes(AActor* Actor, const TArray<FValue>& Values);
	bool SendAttribute(AActor* Actor, FName Attribute, float Value, int32 Instance = INDEX_NONE);
}
