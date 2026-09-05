// Copyright TSAV. All Rights Reserved.
#include "TSAVSuperStageIntegration.h"

#include "ContentBrowserModule.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformProcess.h"
#include "IContentBrowserSingleton.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "TSAVSuperStageIntegration"

namespace TSAVSuperStageIntegration
{
	static bool IsAvailable()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SuperStage"));
		return Plugin.IsValid() && Plugin->IsEnabled()
			&& FModuleManager::Get().IsModuleLoaded(TEXT("SuperTools"));
	}

	static void BrowseContent(const FString& Path)
	{
		FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"))
			.Get().SyncBrowserToFolders({Path});
	}

	void PopulateMenu(FMenuBuilder& Menu)
	{
		if (IsAvailable())
		{
			Menu.BeginSection(TEXT("SuperStagePanels"), LOCTEXT("Panels", "SuperStage panels"));
			// Use registered spawners and their own actions, preserving vendor availability
			// checks and supporting new panels without hard-coded private tab identifiers.
			auto Spawners = FGlobalTabmanager::Get()->CollectSpawners();
			Spawners.Sort([](const TWeakPtr<FTabSpawnerEntry>& A, const TWeakPtr<FTabSpawnerEntry>& B)
			{
				return A.IsValid() && B.IsValid()
					? A.Pin()->GetDisplayName().CompareTo(B.Pin()->GetDisplayName()) < 0
					: A.IsValid();
			});
			for (const auto& WeakSpawner : Spawners)
			{
				const auto Spawner = WeakSpawner.Pin();
				if (Spawner.IsValid() && (Spawner->GetTabType().ToString().StartsWith(TEXT("Super"))
					|| Spawner->GetDisplayName().ToString().Contains(TEXT("SuperStage"))))
				{
					Menu.AddMenuEntry(Spawner->GetDisplayName(), Spawner->GetTooltipText(), Spawner->GetIcon(),
						FGlobalTabmanager::Get()->GetUIActionForTabSpawnerMenuEntry(Spawner));
				}
			}
			Menu.EndSection();
		}

		Menu.BeginSection(TEXT("SuperStageContent"), LOCTEXT("Content", "SuperStage library"));
		const FCanExecuteAction CanBrowse = FCanExecuteAction::CreateStatic(&IsAvailable);
		Menu.AddMenuEntry(LOCTEXT("AllContent", "Browse all SuperStage content"),
			LOCTEXT("AllContentTip", "Open the installed SuperStage content library."), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([] { BrowseContent(TEXT("/SuperStage")); }), CanBrowse));
		Menu.AddMenuEntry(LOCTEXT("Fixtures", "Browse lighting fixture library"),
			LOCTEXT("FixturesTip", "Open SuperStage's supplied fixture definitions and models."), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([] { BrowseContent(TEXT("/SuperStage/Library/Lighting")); }), CanBrowse));
		Menu.EndSection();

		Menu.BeginSection(TEXT("SuperStageHelp"));
		Menu.AddMenuEntry(LOCTEXT("Status", "Installation and launch instructions"), FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]
			{
				const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SuperStage"));
				const FString Status = IsAvailable() ? TEXT("SuperStage is loaded in this editor session.")
					: Plugin.IsValid() ? TEXT("SuperStage is installed. Close this editor and use Start-TSAVSuperStage.cmd in the project folder to load it.")
					: TEXT("SuperStage is not installed. Run Build/Install-SuperStage.ps1 with your vendor archive, then Start-TSAVSuperStage.cmd.");
				FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Status + TEXT("\n\nUse the SuperStage toolbar or Window menu for its complete toolset. Sign in through the vendor panel to activate your licensed features.\n\nThis release contains editor binaries only. Standalone TSAV packaging requires a vendor runtime SDK. See Docs/SUPERSTAGE_INTEGRATION.md.")));
			})));
		Menu.AddMenuEntry(LOCTEXT("Docs", "SuperStage documentation"), FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([] { FPlatformProcess::LaunchURL(TEXT("https://yunsio.com/docs"), nullptr, nullptr); })));
		Menu.EndSection();
	}

	void RegisterMenus()
	{
		UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"))
			->FindOrAddSection(TEXT("TSAVPrevis")).AddSubMenu(
				TEXT("TSAVSuperStage"), LOCTEXT("SuperStage", "TSAV SuperStage"),
				LOCTEXT("SuperStageTip", "SuperStage stage design, lighting, media, effects, and fixture tools."),
				FNewMenuDelegate::CreateStatic(&PopulateMenu));
	}
}

#undef LOCTEXT_NAMESPACE
