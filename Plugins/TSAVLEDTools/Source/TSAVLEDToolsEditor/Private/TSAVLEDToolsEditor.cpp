// Copyright TSAV. All Rights Reserved.

#include "TSAVLEDToolsEditor.h"

#include "STSAVLEDWallBuilder.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "TSAVLEDToolsEditor"

namespace TSAVLEDToolsEditor
{
	const FName BuilderTabName(TEXT("TSAVLEDWallBuilder"));
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
}

void FTSAVLEDToolsEditorModule::OpenBuilderTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(TSAVLEDToolsEditor::BuilderTabName);
}

TSharedRef<SDockTab> FTSAVLEDToolsEditorModule::SpawnBuilderTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(STSAVLEDWallBuilder)
		];
}

IMPLEMENT_MODULE(FTSAVLEDToolsEditorModule, TSAVLEDToolsEditor)

#undef LOCTEXT_NAMESPACE
