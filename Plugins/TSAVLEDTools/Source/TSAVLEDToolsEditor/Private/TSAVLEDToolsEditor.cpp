// Copyright TSAV. All Rights Reserved.

#include "TSAVLEDToolsEditor.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/MessageDialog.h"
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

	BuildCompleteFixtureLibraryCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("tsav.BuildCompleteFixtureLibraryAndQuit"),
		TEXT("Builds and validates all imported TSAV GDTF fixtures, then exits with an appropriate status."),
		FConsoleCommandDelegate::CreateRaw(this, &FTSAVLEDToolsEditorModule::BuildCompleteFixtureLibraryAndQuit),
		ECVF_Default);
	ValidateCompleteFixtureLibraryCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("tsav.ValidateCompleteFixtureLibraryAndQuit"),
		TEXT("Validates the generated TSAV GDTF catalog and DMX library, then exits."),
		FConsoleCommandDelegate::CreateRaw(this, &FTSAVLEDToolsEditorModule::ValidateCompleteFixtureLibraryAndQuit),
		ECVF_Default);
}

void FTSAVLEDToolsEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	if (BuildCompleteFixtureLibraryCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(BuildCompleteFixtureLibraryCommand);
		BuildCompleteFixtureLibraryCommand = nullptr;
	}
	if (ValidateCompleteFixtureLibraryCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(ValidateCompleteFixtureLibraryCommand);
		ValidateCompleteFixtureLibraryCommand = nullptr;
	}

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
	Section.AddMenuEntry(
		TEXT("BuildTSAVCompleteFixtureLibrary"),
		LOCTEXT("BuildCompleteFixtureLibraryLabel", "Build Complete GDTF Fixture Library"),
		LOCTEXT("BuildCompleteFixtureLibraryTooltip", "Generate models, fixture options, DMX types, and patches for every imported GDTF profile."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Refresh")),
		FUIAction(FExecuteAction::CreateRaw(this, &FTSAVLEDToolsEditorModule::BuildCompleteFixtureLibrary)));
}

void FTSAVLEDToolsEditorModule::OpenBuilderTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(TSAVLEDToolsEditor::BuilderTabName);
}

void FTSAVLEDToolsEditorModule::OpenFixtureBuilderTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(TSAVLEDToolsEditor::FixtureBuilderTabName);
}

void FTSAVLEDToolsEditorModule::BuildCompleteFixtureLibrary()
{
	FString Summary;
	const bool bBuilt = STSAVDMXFixtureBuilder::BuildCompleteFixtureLibrary(Summary);
	FString ValidationSummary;
	const bool bValidated = bBuilt && STSAVDMXFixtureBuilder::ValidateCompleteFixtureLibrary(ValidationSummary);
	FMessageDialog::Open(
		bBuilt && bValidated ? EAppMsgType::Ok : EAppMsgType::Ok,
		FText::FromString(Summary + TEXT("\n\n") + ValidationSummary),
		LOCTEXT("BuildCompleteFixtureLibraryResultTitle", "TSAV Complete Fixture Library"));
}

void FTSAVLEDToolsEditorModule::BuildCompleteFixtureLibraryAndQuit()
{
	FString BuildSummary;
	const bool bBuilt = STSAVDMXFixtureBuilder::BuildCompleteFixtureLibrary(BuildSummary);
	FString ValidationSummary;
	const bool bValidated = bBuilt && STSAVDMXFixtureBuilder::ValidateCompleteFixtureLibrary(ValidationSummary);
	FPlatformMisc::RequestExitWithStatus(true, bBuilt && bValidated ? 0 : 1);
}

void FTSAVLEDToolsEditorModule::ValidateCompleteFixtureLibraryAndQuit()
{
	FString Summary;
	const bool bValidated = STSAVDMXFixtureBuilder::ValidateCompleteFixtureLibrary(Summary);
	FPlatformMisc::RequestExitWithStatus(true, bValidated ? 0 : 1);
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
