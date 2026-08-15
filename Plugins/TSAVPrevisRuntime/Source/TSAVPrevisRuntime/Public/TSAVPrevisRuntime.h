// Copyright TSAV. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTSAVPrevisRuntime, Log, All);

class FTSAVPrevisRuntimeModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
