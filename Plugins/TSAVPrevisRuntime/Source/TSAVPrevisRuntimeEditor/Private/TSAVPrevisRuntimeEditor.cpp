// Copyright TSAV. All Rights Reserved.

#include "TSAVPrevisRuntimeEditor.h"

#include "Framework/Application/SlateApplication.h"
#include "STSAVCameraControllerTool.h"
#include "STSAVCameraTool.h"
#include "STSAVDMXLightingConsoleTool.h"
#include "STSAVDMXPatchTool.h"
#include "STSAVScreenControlTool.h"
#include "STSAVVideoSwitcherTool.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "TSAVPrevisRuntimeEditor"

namespace TSAVPrevisRuntimeEditor
{
	const FName CameraToolTabName(TEXT("TSAVCameraTool"));
	const FName CameraControllerToolTabName(TEXT("TSAVCameraControllerTool"));
	const FName DMXPatchToolTabName(TEXT("TSAVDMXPatchTool"));
	const FName LightingConsoleToolTabName(TEXT("TSAVLightingConsoleTool"));
	const FName ScreenControlToolTabName(TEXT("TSAVScreenControlTool"));
	const FName VideoSwitcherToolTabName(TEXT("TSAVVideoSwitcherTool"));
}

void FTSAVPrevisRuntimeEditorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TSAVPrevisRuntimeEditor::CameraToolTabName,
		FOnSpawnTab::CreateRaw(this, &FTSAVPrevisRuntimeEditorModule::SpawnCameraToolTab))
		.SetDisplayName(LOCTEXT("CameraToolTabTitle", "TSAV Camera Tool"))
		.SetTooltipText(LOCTEXT("CameraToolTabTooltip", "Create and configure production, cinema, virtual, and VISCA PTZ cameras."))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.CameraActor")))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TSAVPrevisRuntimeEditor::CameraControllerToolTabName,
		FOnSpawnTab::CreateRaw(this, &FTSAVPrevisRuntimeEditorModule::SpawnCameraControllerToolTab))
		.SetDisplayName(LOCTEXT("CameraControllerToolTabTitle", "TSAV Camera Controller"))
		.SetTooltipText(LOCTEXT("CameraControllerToolTabTooltip", "Control every TSAV camera and configure VISCA-over-IP PTZ connections."))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.CineCameraActor")))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TSAVPrevisRuntimeEditor::DMXPatchToolTabName,
		FOnSpawnTab::CreateRaw(this, &FTSAVPrevisRuntimeEditorModule::SpawnDMXPatchToolTab))
		.SetDisplayName(LOCTEXT("DMXPatchToolTabTitle", "TSAV DMX Patch & Test"))
		.SetTooltipText(LOCTEXT("DMXPatchToolTabTooltip", "Search, address, validate, spawn, and test all 607 generated fixture patches."))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.Light")))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TSAVPrevisRuntimeEditor::LightingConsoleToolTabName,
		FOnSpawnTab::CreateRaw(this, &FTSAVPrevisRuntimeEditorModule::SpawnLightingConsoleToolTab))
		.SetDisplayName(LOCTEXT("LightingConsoleToolTabTitle", "TSAV Lighting Console"))
		.SetTooltipText(LOCTEXT("LightingConsoleToolTabTooltip", "Live programmer and raw attribute faders for generated DMX fixture patches."))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.SpotLight")))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TSAVPrevisRuntimeEditor::ScreenControlToolTabName,
		FOnSpawnTab::CreateRaw(this, &FTSAVPrevisRuntimeEditorModule::SpawnScreenControlToolTab))
		.SetDisplayName(LOCTEXT("ScreenControlToolTabTitle", "TSAV Screen Control"))
		.SetTooltipText(LOCTEXT("ScreenControlToolTabTooltip", "Edit screen names, brightness, canvas origins, locations, and rotations."))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.TextureRenderTarget2D")))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TSAVPrevisRuntimeEditor::VideoSwitcherToolTabName,
		FOnSpawnTab::CreateRaw(this, &FTSAVPrevisRuntimeEditorModule::SpawnVideoSwitcherToolTab))
		.SetDisplayName(LOCTEXT("VideoSwitcherToolTabTitle", "TSAV Video Switcher"))
		.SetTooltipText(LOCTEXT("VideoSwitcherToolTabTooltip", "Discover camera, media, and NDI sources and route Program, Preview, and Aux buses."))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.MediaPlayer")))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FTSAVPrevisRuntimeEditorModule::RegisterMenus));
}

void FTSAVPrevisRuntimeEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TSAVPrevisRuntimeEditor::CameraToolTabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TSAVPrevisRuntimeEditor::CameraControllerToolTabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TSAVPrevisRuntimeEditor::DMXPatchToolTabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TSAVPrevisRuntimeEditor::LightingConsoleToolTabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TSAVPrevisRuntimeEditor::ScreenControlToolTabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TSAVPrevisRuntimeEditor::VideoSwitcherToolTabName);
	}
}

void FTSAVPrevisRuntimeEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
	FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("TSAVPrevis"));
	Section.AddMenuEntry(
		TEXT("OpenTSAVCameraTool"),
		LOCTEXT("OpenCameraToolLabel", "TSAV Camera Tool"),
		LOCTEXT("OpenCameraToolTooltip", "Create cameras from the editor view and configure lens, output, PTZ, and VISCA settings."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.CameraActor")),
		FUIAction(FExecuteAction::CreateRaw(this, &FTSAVPrevisRuntimeEditorModule::OpenCameraToolTab)));
	Section.AddMenuEntry(
		TEXT("OpenTSAVCameraControllerTool"),
		LOCTEXT("OpenCameraControllerToolLabel", "TSAV Camera Controller"),
		LOCTEXT("OpenCameraControllerToolTooltip", "Control all cameras, image settings, positions, and VISCA-over-IP PTZ connections from one panel."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.CineCameraActor")),
		FUIAction(FExecuteAction::CreateRaw(this, &FTSAVPrevisRuntimeEditorModule::OpenCameraControllerToolTab)));
	Section.AddMenuEntry(
		TEXT("OpenTSAVDMXPatchTool"),
		LOCTEXT("OpenDMXPatchToolLabel", "TSAV DMX Patch & Fixture Test"),
		LOCTEXT("OpenDMXPatchToolTooltip", "Search all 607 generated fixtures, safely edit patches, spawn actors, and run live fixture tests."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.Light")),
		FUIAction(FExecuteAction::CreateRaw(this, &FTSAVPrevisRuntimeEditorModule::OpenDMXPatchToolTab)));
	Section.AddMenuEntry(
		TEXT("OpenTSAVLightingConsoleTool"),
		LOCTEXT("OpenLightingConsoleToolLabel", "TSAV Lighting Console"),
		LOCTEXT("OpenLightingConsoleToolTooltip", "Select any generated patches and control their common or raw DMX attributes with live faders."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.SpotLight")),
		FUIAction(FExecuteAction::CreateRaw(this, &FTSAVPrevisRuntimeEditorModule::OpenLightingConsoleToolTab)));
	Section.AddMenuEntry(
		TEXT("OpenTSAVScreenControlTool"),
		LOCTEXT("OpenScreenControlToolLabel", "TSAV Screen Control"),
		LOCTEXT("OpenScreenControlToolTooltip", "Edit every LED wall or panel's name, brightness, canvas start, physical location, and rotation."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.TextureRenderTarget2D")),
		FUIAction(FExecuteAction::CreateRaw(this, &FTSAVPrevisRuntimeEditorModule::OpenScreenControlToolTab)));
	Section.AddMenuEntry(
		TEXT("OpenTSAVVideoSwitcherTool"),
		LOCTEXT("OpenVideoSwitcherToolLabel", "TSAV Video Switcher"),
		LOCTEXT("OpenVideoSwitcherToolTooltip", "Create a switcher, refresh visible inputs, and route Program, Preview, and Aux buses."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.MediaPlayer")),
		FUIAction(FExecuteAction::CreateRaw(this, &FTSAVPrevisRuntimeEditorModule::OpenVideoSwitcherToolTab)));
}

void FTSAVPrevisRuntimeEditorModule::OpenCameraToolTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(TSAVPrevisRuntimeEditor::CameraToolTabName);
}

void FTSAVPrevisRuntimeEditorModule::OpenCameraControllerToolTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(TSAVPrevisRuntimeEditor::CameraControllerToolTabName);
}

void FTSAVPrevisRuntimeEditorModule::OpenScreenControlToolTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(TSAVPrevisRuntimeEditor::ScreenControlToolTabName);
}

void FTSAVPrevisRuntimeEditorModule::OpenDMXPatchToolTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(TSAVPrevisRuntimeEditor::DMXPatchToolTabName);
}

void FTSAVPrevisRuntimeEditorModule::OpenLightingConsoleToolTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(TSAVPrevisRuntimeEditor::LightingConsoleToolTabName);
}

void FTSAVPrevisRuntimeEditorModule::OpenVideoSwitcherToolTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(TSAVPrevisRuntimeEditor::VideoSwitcherToolTabName);
}

TSharedRef<SDockTab> FTSAVPrevisRuntimeEditorModule::SpawnCameraToolTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(STSAVCameraTool)
		];
}

TSharedRef<SDockTab> FTSAVPrevisRuntimeEditorModule::SpawnCameraControllerToolTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(STSAVCameraControllerTool)
		];
}

TSharedRef<SDockTab> FTSAVPrevisRuntimeEditorModule::SpawnScreenControlToolTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(STSAVScreenControlTool)
		];
}

TSharedRef<SDockTab> FTSAVPrevisRuntimeEditorModule::SpawnDMXPatchToolTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(STSAVDMXPatchTool)
		];
}

TSharedRef<SDockTab> FTSAVPrevisRuntimeEditorModule::SpawnLightingConsoleToolTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(STSAVDMXLightingConsoleTool)
		];
}

TSharedRef<SDockTab> FTSAVPrevisRuntimeEditorModule::SpawnVideoSwitcherToolTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(STSAVVideoSwitcherTool)
		];
}

IMPLEMENT_MODULE(FTSAVPrevisRuntimeEditorModule, TSAVPrevisRuntimeEditor)

#undef LOCTEXT_NAMESPACE
