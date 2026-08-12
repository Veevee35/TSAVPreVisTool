// Copyright TSAV. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FSpawnTabArgs;
class SDockTab;

class FTSAVLEDToolsEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void OpenBuilderTab();
	void OpenFixtureBuilderTab();
	TSharedRef<SDockTab> SpawnBuilderTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnFixtureBuilderTab(const FSpawnTabArgs& Args);
};
