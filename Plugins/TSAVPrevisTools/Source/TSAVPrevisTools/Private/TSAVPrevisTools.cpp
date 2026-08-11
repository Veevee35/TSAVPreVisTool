// Copyright Epic Games, Inc. All Rights Reserved.

#include "TSAVPrevisTools.h"

#include "TSAVPrevisToolset.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#define LOCTEXT_NAMESPACE "FTSAVPrevisToolsModule"

void FTSAVPrevisToolsModule::StartupModule()
{
	if (UToolsetRegistry::IsAvailable())
	{
		UToolsetRegistry::RegisterToolsetClass(UTSAVPrevisToolset::StaticClass());
	}
}

void FTSAVPrevisToolsModule::ShutdownModule()
{
	if (UToolsetRegistry::IsAvailable() &&
		UToolsetRegistry::IsToolsetClassRegistered(UTSAVPrevisToolset::StaticClass()))
	{
		UToolsetRegistry::UnregisterToolsetClass(UTSAVPrevisToolset::StaticClass());
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTSAVPrevisToolsModule, TSAVPrevisTools)
