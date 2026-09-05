// Copyright TSAV. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "ToolMenus.h"
#include "TSAVSuperStageIntegration.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTSAVSuperStageIntegrationTest, "TSAV.SuperStage.EditorIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTSAVSuperStageIntegrationTest::RunTest(const FString& Parameters)
{
	const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("SuperStage"));
	if (!TestTrue(TEXT("SuperStage is installed and enabled by the launcher"), Plugin.IsValid() && Plugin->IsEnabled()))
	{
		return false;
	}
	for (const auto& Module : Plugin->GetDescriptor().Modules)
	{
		TestTrue(FString::Printf(TEXT("Vendor module loaded: %s"), *Module.Name.ToString()),
			FModuleManager::Get().IsModuleLoaded(Module.Name));
	}
	for (const TCHAR* ClassPath : {TEXT("/Script/SuperAssets.SuperTruss"), TEXT("/Script/SuperAssets.SuperStageFloor"),
		TEXT("/Script/SuperAssets.SuperScreen"), TEXT("/Script/SuperCore.SuperFixtureActor")})
	{
		TestNotNull(FString::Printf(TEXT("Vendor actor registered: %s"), ClassPath), FindObject<UClass>(nullptr, ClassPath));
	}
	UToolMenu* ToolsMenu = UToolMenus::Get()->FindMenu(TEXT("LevelEditor.MainMenu.Tools"));
	TestNotNull(TEXT("Tools menu registered"), ToolsMenu);
	if (ToolsMenu)
	{
		const auto* Section = ToolsMenu->FindSection(TEXT("TSAVPrevis"));
		TestTrue(TEXT("TSAV SuperStage submenu registered"), Section && Section->FindEntry(TEXT("TSAVSuperStage")));
	}
	FMenuBuilder Menu(true, nullptr);
	TSAVSuperStageIntegration::PopulateMenu(Menu);
	int32 VendorPanels = 0;
	for (const auto& WeakSpawner : FGlobalTabmanager::Get()->CollectSpawners())
	{
		const auto Spawner = WeakSpawner.Pin();
		if (Spawner.IsValid() && (Spawner->GetTabType().ToString().StartsWith(TEXT("Super"))
			|| Spawner->GetDisplayName().ToString().Contains(TEXT("SuperStage"))))
		{
			++VendorPanels;
			AddInfo(FString::Printf(TEXT("Available vendor panel: %s (%s)"),
				*Spawner->GetDisplayName().ToString(), *Spawner->GetTabType().ToString()));
		}
	}
	TestTrue(TEXT("Vendor panels discoverable from TSAV menu"), VendorPanels > 0);
	auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanPathsSynchronous({TEXT("/SuperStage")}, false);
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(TEXT("/SuperStage"), Assets, true);
	AddInfo(FString::Printf(TEXT("SuperStage assets registered: %d"), Assets.Num()));
	TestTrue(TEXT("SuperStage content library mounted"), Assets.Num() > 9000);
	return true;
}

#endif
