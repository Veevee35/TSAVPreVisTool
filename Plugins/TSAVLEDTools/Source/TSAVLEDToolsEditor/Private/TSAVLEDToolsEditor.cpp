// Copyright TSAV. All Rights Reserved.

#include "TSAVLEDToolsEditor.h"

#include "Framework/Application/SlateApplication.h"
#include "STSAVDMXFixtureBuilder.h"
#include "STSAVLEDWallBuilder.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "TSAVLEDToolsEditor"

namespace TSAVLEDToolsEditor
{
	const FName BuilderTabName(TEXT("TSAVLEDWallBuilder"));
	const FName FixtureBuilderTabName(TEXT("TSAVDMXFixtureBuilder"));
}

void FTSAVLEDToolsEditorModule::StartupModule()
{
	const FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), TEXT("LevelEditor.Tabs.Details"));
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TSAVLEDToolsEditor::BuilderTabName,
		FOnSpawnTab::CreateRaw(this, &FTSAVLEDToolsEditorModule::SpawnBuilderTab))
		.SetDisplayName(LOCTEXT("BuilderTabTitle", "TSAV LED Wall Builder"))
		.SetTooltipText(LOCTEXT("BuilderTabTooltip", "Build an LED wall from a cabinet specification and map an NDI source."))
		.SetIcon(Icon)
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TSAVLEDToolsEditor::FixtureBuilderTabName,
		FOnSpawnTab::CreateRaw(this, &FTSAVLEDToolsEditorModule::SpawnFixtureBuilderTab))
		.SetDisplayName(LOCTEXT("FixtureBuilderTabTitle", "TSAV GDTF DMX Fixture Builder"))
		.SetTooltipText(LOCTEXT("FixtureBuilderTabTooltip", "Import a GDTF and model, configure articulation and beam behavior, and create a patched DMX fixture."))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.SpotLight")))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FTSAVLEDToolsEditorModule::RegisterMenus));
}

void FTSAVLEDToolsEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TSAVLEDToolsEditor::BuilderTabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TSAVLEDToolsEditor::FixtureBuilderTabName);
	}
}

void FTSAVLEDToolsEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
	FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("TSAVPrevis"));
	Section.AddMenuEntry(
		TEXT("OpenTSAVLEDWallBuilder"),
		LOCTEXT("OpenBuilderLabel", "TSAV LED Wall Builder"),
		LOCTEXT("OpenBuilderTooltip", "Open the guided panel, wall, canvas, and NDI configurator."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("LevelEditor.Tabs.Details")),
		FUIAction(FExecuteAction::CreateRaw(this, &FTSAVLEDToolsEditorModule::OpenBuilderTab)));
	Section.AddMenuEntry(
		TEXT("OpenTSAVDMXFixtureBuilder"),
		LOCTEXT("OpenFixtureBuilderLabel", "TSAV GDTF DMX Fixture Builder"),
		LOCTEXT("OpenFixtureBuilderTooltip", "Open the guided GDTF, model, motion, beam, and DMX patch configurator."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.SpotLight")),
		FUIAction(FExecuteAction::CreateRaw(this, &FTSAVLEDToolsEditorModule::OpenFixtureBuilderTab)));
}

void FTSAVLEDToolsEditorModule::OpenBuilderTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(TSAVLEDToolsEditor::BuilderTabName);
}

void FTSAVLEDToolsEditorModule::OpenFixtureBuilderTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(TSAVLEDToolsEditor::FixtureBuilderTabName);
}

TSharedRef<SDockTab> FTSAVLEDToolsEditorModule::SpawnBuilderTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(STSAVLEDWallBuilder)
		];
}

TSharedRef<SDockTab> FTSAVLEDToolsEditorModule::SpawnFixtureBuilderTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(STSAVDMXFixtureBuilder)
		];
}

IMPLEMENT_MODULE(FTSAVLEDToolsEditorModule, TSAVLEDToolsEditor)

#undef LOCTEXT_NAMESPACE
