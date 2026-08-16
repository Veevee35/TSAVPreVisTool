// Copyright TSAV. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FSpawnTabArgs;
class SDockTab;

class FTSAVPrevisRuntimeEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void OpenCameraToolTab();
	void OpenCameraControllerToolTab();
	void OpenDMXPatchToolTab();
	void OpenLightingConsoleToolTab();
	void OpenScreenControlToolTab();
	void OpenVideoSwitcherToolTab();
	TSharedRef<SDockTab> SpawnCameraToolTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnCameraControllerToolTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnDMXPatchToolTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnLightingConsoleToolTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnScreenControlToolTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnVideoSwitcherToolTab(const FSpawnTabArgs& Args);
};
