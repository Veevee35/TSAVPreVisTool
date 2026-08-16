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
	void OpenVideoSwitcherToolTab();
	TSharedRef<SDockTab> SpawnCameraToolTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnVideoSwitcherToolTab(const FSpawnTabArgs& Args);
};
