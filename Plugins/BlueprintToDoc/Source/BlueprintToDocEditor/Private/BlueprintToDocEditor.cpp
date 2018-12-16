// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintToDocEditor.h"
#include "BlueprintToDocStyle.h"
#include "BlueprintToDocCommands.h"
#include "Misc/MessageDialog.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "LevelEditor.h"
#include "BlueprintToDocUtil.h"
#include "BlueprintToDocUserSettings.h"
#include <ISettingsModule.h>



static const FName BlueprintToDocTabName("BlueprintToDoc");

#define LOCTEXT_NAMESPACE "FBlueprintToDocModule"

void FBlueprintToDocEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FBlueprintToDocStyle::Initialize();
	FBlueprintToDocStyle::ReloadTextures();

	FBlueprintToDocCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FBlueprintToDocCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FBlueprintToDocEditorModule::PluginButtonClicked),
		FCanExecuteAction());
		
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	
	{
		TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
		MenuExtender->AddMenuExtension("WindowLayout", EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FBlueprintToDocEditorModule::AddMenuExtension));

		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
	}
	
	{
		TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
		ToolbarExtender->AddToolBarExtension("Settings", EExtensionHook::After, PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FBlueprintToDocEditorModule::AddToolbarExtension));
		
		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
	}

	RegisterSettings();
}

void FBlueprintToDocEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FBlueprintToDocStyle::Shutdown();

	FBlueprintToDocCommands::Unregister();
}

void FBlueprintToDocEditorModule::PluginButtonClicked()
{
	// ドキュメント設定
	const UBlueprintToDocUserSettings* Settings = GetDefault<UBlueprintToDocUserSettings>();
	check(Settings);

	FDocument Document;

	if(Settings->DocumentRootPath.Path.IsEmpty())
	{
		Document.RootPath = FPaths::ProjectDir() / "Doc";
	}
	else
	{
		Document.RootPath = Settings->DocumentRootPath.Path;

		// 絶対パスではない場合プロジェクトパスからの相対パスとみなす
		if(Document.RootPath.Find(TEXT(":/")) != 1)
		{
			Document.RootPath = FPaths::ProjectDir() / Settings->DocumentRootPath.Path;
		}

		FPaths::NormalizeDirectoryName(Document.RootPath);
	}

	// ドキュメント化実行
	UBlueprintToDocUtil::BlueprintToDoc_Exec(Document);

	// 終了を通知
	FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, TEXT("Finish！！ BlueprintToDoc"), TEXT("BlueprintToDoc"));
}

void FBlueprintToDocEditorModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FBlueprintToDocCommands::Get().PluginAction);
}

void FBlueprintToDocEditorModule::AddToolbarExtension(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FBlueprintToDocCommands::Get().PluginAction);
}

void FBlueprintToDocEditorModule::RegisterSettings()
{
	if(ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "BlueprintToDoc",
			LOCTEXT("RuntimeSettingsName", "BlueprintToDoc"),
			LOCTEXT("RuntimeSettingsDescription", "BlueprintToDoc"),
			GetMutableDefault<UBlueprintToDocUserSettings>());
	}
}

void FBlueprintToDocEditorModule::UnregisterSettings()
{
	if(ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "BlueprintToDoc");
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBlueprintToDocEditorModule, BlueprintToDocEditor)