// Copyright TSAV. All Rights Reserved.

#include "TSAVPrevisRuntimeEditor.h"
#include "Editor.h"
#include "UI/STSAVLightingShowPanel.h"
#include "UI/TSAVSceneBuilderWidget.h"
#include "UI/TSAVRigWidget.h"
#include "UI/TSAVDMXNetworkWidget.h"
#include "UI/TSAVLaserWidget.h"

#include "Framework/Application/SlateApplication.h"
#include "STSAVCameraControllerTool.h"
#include "STSAVCameraTool.h"
#include "STSAVDMXLightingConsoleTool.h"
#include "STSAVDMXPatchTool.h"
#include "STSAVScreenControlTool.h"
#include "STSAVVideoSwitcherTool.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "TSAVSuperStageIntegration.h"
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
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TEXT("TSAVRigTools"),FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&) {
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[MakeTSAVRigPanel(GEditor?GEditor->GetEditorWorldContext().World():nullptr,FSimpleDelegate())];
	})).SetDisplayName(LOCTEXT("RigToolsTitle","TSAV Fixture Arrays and Rig Exchange")).SetMenuType(ETabSpawnerMenuType::Hidden);
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TEXT("TSAVDMXConnections"),FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&) {
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[MakeTSAVDMXNetworkPanel(GEditor?GEditor->GetEditorWorldContext().World():nullptr,FSimpleDelegate())];
	})).SetDisplayName(LOCTEXT("DMXConnectionsTitle","TSAV DMX Connections and Monitor")).SetMenuType(ETabSpawnerMenuType::Hidden);
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TEXT("TSAVLaserPreview"),FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&) {
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[MakeTSAVLaserPanel(GEditor?GEditor->GetEditorWorldContext().World():nullptr,FSimpleDelegate())];
	})).SetDisplayName(LOCTEXT("LaserPreviewTitle","TSAV Laser Preview and ILDA")).SetMenuType(ETabSpawnerMenuType::Hidden);
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TEXT("TSAVSceneBuilder"), FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&) {
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[MakeTSAVSceneBuilder(GEditor ? GEditor->GetEditorWorldContext().World() : nullptr,FSimpleDelegate())];
	})).SetDisplayName(LOCTEXT("SceneBuilderTitle", "TSAV Stage and Scenic Builder")).SetMenuType(ETabSpawnerMenuType::Hidden);
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TEXT("TSAVLightingShow"), FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&) {
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(STSAVLightingShowPanel).World(GEditor ? GEditor->GetEditorWorldContext().World() : nullptr)];
	})).SetDisplayName(LOCTEXT("LightingShowTabTitle", "TSAV Lighting Show")).SetMenuType(ETabSpawnerMenuType::Hidden);
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
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("TSAVLightingShow"));
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("TSAVSceneBuilder"));
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("TSAVLaserPreview"));
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("TSAVRigTools"));
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("TSAVDMXConnections"));
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
	TSAVSuperStageIntegration::RegisterMenus();
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
	FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("TSAVPrevis"));
	Section.AddMenuEntry(TEXT("OpenTSAVRigTools"),LOCTEXT("RigToolsLabel","TSAV Fixture Arrays and Rig Exchange"),
		LOCTEXT("RigToolsTooltip","Create fixture arrays and transfer MVR rigs with embedded GDTF profiles."),FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([] { FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("TSAVRigTools"))); })));
	Section.AddMenuEntry(TEXT("OpenTSAVDMXConnections"),LOCTEXT("DMXConnectionsLabel","TSAV DMX Connections and Monitor"),
		LOCTEXT("DMXConnectionsTooltip","Configure Art-Net and sACN ports, connect native fixture libraries, and monitor channels."),FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([] { FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("TSAVDMXConnections"))); })));
	Section.AddMenuEntry(TEXT("OpenTSAVLaserPreview"),LOCTEXT("LaserPreviewLabel","TSAV Laser Preview and ILDA"),
		LOCTEXT("LaserPreviewTooltip","Create and import laser frames, configure projection and play animations."),FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([] { FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("TSAVLaserPreview"))); })));
	Section.AddMenuEntry(TEXT("OpenTSAVSceneBuilder"), LOCTEXT("SceneBuilderLabel", "TSAV Stage and Scenic Builder"),
		LOCTEXT("SceneBuilderTooltip", "Build and edit dimensioned stage, truss, scenery and machinery."), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([] { FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("TSAVSceneBuilder"))); })));
	Section.AddMenuEntry(TEXT("OpenTSAVLightingShow"), LOCTEXT("LightingShowLabel", "TSAV Lighting Show"),
		LOCTEXT("LightingShowTooltip", "Native fixture patching, groups, presets, cues and effects."), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([] { FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("TSAVLightingShow"))); })));
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
