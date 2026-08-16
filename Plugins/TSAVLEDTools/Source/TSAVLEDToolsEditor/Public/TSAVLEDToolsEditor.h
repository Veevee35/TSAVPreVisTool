// Copyright TSAV. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FSpawnTabArgs;
class IConsoleObject;
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
	void BuildCompleteFixtureLibrary();
	void BuildCompleteFixtureLibraryAndQuit();
	void ValidateCompleteFixtureLibraryAndQuit();
	TSharedRef<SDockTab> SpawnBuilderTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnFixtureBuilderTab(const FSpawnTabArgs& Args);

	IConsoleObject* BuildCompleteFixtureLibraryCommand = nullptr;
	IConsoleObject* ValidateCompleteFixtureLibraryCommand = nullptr;
};
