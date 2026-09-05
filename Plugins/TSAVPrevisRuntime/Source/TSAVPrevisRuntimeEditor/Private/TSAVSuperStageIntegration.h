// Copyright TSAV. All Rights Reserved.
#pragma once

class FMenuBuilder;

/** Editor bridge to the separately installed SuperStage binary distribution. */
namespace TSAVSuperStageIntegration
{
	void RegisterMenus();
	void PopulateMenu(FMenuBuilder& Menu);
}
