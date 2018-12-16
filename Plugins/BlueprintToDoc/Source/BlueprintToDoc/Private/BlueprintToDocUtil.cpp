// Fill out your copyright notice in the Description page of Project Settings.

#include "BlueprintToDocUtil.h"
#include "BlueprintToDoc.h"
#include "BlueprintToDocUserSettings.h"

#include "AssetRegistryModule.h"
#include "GenericPlatformFile.h"
#include "PlatformFilemanager.h"
#include "Paths.h"
#include "FileHelper.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "CoreNet.h"
#include "Animation/PreviewAssetAttachComponent.h"
#include "Class.h"
#include <EdGraphSchema_K2.h>

#include "Tests/AutomationCommon.h"
#include "Misc/AutomationTest.h"


// ソート用
struct FBlueprintSorter
{
	bool operator()(FBlueprintDocument A, FBlueprintDocument B) const
	{
		return A.ContentPath < B.ContentPath;
	}
};
struct FGraphSorter
{
	bool operator()(FEdGraphDocument A, FEdGraphDocument B) const
	{
		return A.Category < B.Category;
	}
};
struct FPropertySorter
{
	bool operator()(FPropertyDocument A, FPropertyDocument B) const
	{
		return A.Category < B.Category;
	}
};


/**
 *  テスト実行 ドキュメント化
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueprintToDocTest, "BlueprintToDoc.MakeDocTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);
bool FBlueprintToDocTest::RunTest(const FString& Parameters)
{
	const UBlueprintToDocUserSettings* Settings = GetDefault<UBlueprintToDocUserSettings>();
	check(Settings);

	// ドキュメント
	FDocument Document;

	if (Settings->DocumentRootPath.Path.IsEmpty())
	{
		Document.RootPath = FPaths::ProjectDir() / "Doc";
	}
	else
	{
		Document.RootPath = Settings->DocumentRootPath.Path;

		// 絶対パスではない場合プロジェクトパスからの相対パスとみなす
		if (Document.RootPath.Find(TEXT(":/")) != 1)
		{
			Document.RootPath = FPaths::ProjectDir() / Settings->DocumentRootPath.Path;
		}
	}
	FPaths::NormalizeDirectoryName(Document.RootPath);

	UBlueprintToDocUtil::BlueprintToDoc_Exec(Document);

	return true;
}

// ドキュメントテンプレート
static const TCHAR HTMLTemplate[] = TEXT("<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title></title><link href=\"../layout.css\" rel=\"stylesheet\" type=\"text/css\"></head><body>\n%s\n</body></html>");


void UBlueprintToDocUtil::BlueprintToDoc()
{
	const UBlueprintToDocUserSettings* Settings = GetDefault<UBlueprintToDocUserSettings>();
	check(Settings);

	// ドキュメント
	FDocument Document;

	// 出力先を決める
	if(Settings->DocumentRootPath.Path.IsEmpty())
	{
		Document.RootPath = FPaths::ProjectDir() / "Doc";
	}
	else
	{
		Document.RootPath = Settings->DocumentRootPath.Path;	
	}
	FPaths::NormalizeDirectoryName(Document.RootPath);

	BlueprintToDoc_Exec(Document);

	// 終了を通知
	FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, TEXT("Finish！！ BlueprintToDoc"), TEXT("BlueprintToDoc"));
}

void UBlueprintToDocUtil::BlueprintToDoc_Exec(FDocument& Document)
{
	const UBlueprintToDocUserSettings* Settings = GetDefault<UBlueprintToDocUserSettings>();

	// AssetRegistryモジュールの取得
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// 探すアセットのフィルタ
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Append(Settings->ContentPaths);
	Filter.bIncludeOnlyOnDiskAssets = false;
	Filter.ClassNames.Add(UBlueprint::StaticClass()->GetFName());
	Filter.bRecursiveClasses = true;

	// 条件にあったアセットを取得
	TArray<FAssetData> ShowAssetData;
	if(!AssetRegistry.GetAssets(Filter,ShowAssetData))
	{
		return;
	}

	// 取得したアセットをドキュメント化
	for(auto It = ShowAssetData.CreateConstIterator(); It; ++It)
	{
		// エンジンソースは省く
		FString BlueprintPackagePath = It->PackagePath.ToString();
		if(BlueprintPackagePath.Find(TEXT("/Engine/")) != -1)
		{
			continue;
		}

		// アセットをUBlueprintにキャスト
		UBlueprint* BluprintClass = Cast<UBlueprint>(It->GetAsset());
		if(BluprintClass == nullptr)
		{
			continue;
		}

		FBlueprintDocument BlueprintDocument;

		// BlueprintName
		BlueprintDocument.Name = It->AssetName.ToString();
		// ToolTips
		BlueprintDocument.ToolTips = BluprintClass->GeneratedClass->GetMetaData(TEXT("ToolTip"));
		// Path
		BlueprintDocument.ContentPath = BlueprintPackagePath;

		UClass* const BPClass = BluprintClass->GeneratedClass;
		if(BPClass)
		{
			// 親クラス名
			BlueprintDocument.ParentName = BPClass->GetSuperStruct()->GetName();

			// Event
			MakeEdGraphDocument(BlueprintDocument.Events, BluprintClass, BluprintClass->EventGraphs);

			// Macro
			MakeEdGraphDocument(BlueprintDocument.Macros, BluprintClass, BluprintClass->MacroGraphs);

			// Function
			MakeEdGraphDocument(BlueprintDocument.Functions, BluprintClass, BluprintClass->FunctionGraphs);

			// Propery
			for(TFieldIterator<UProperty> PropIt(BPClass); PropIt; ++PropIt)
			{
				UProperty* Property = *PropIt;

				// エディタ上で作られたものだけリストに
				if(PropIt->GetFullGroupName(true).Right(2) == TEXT("_C"))
				{
					FPropertyDocument PropertyDocument;

					if(PropertyDocument.Name.IsEmpty())
					{
						PropertyDocument.Name = Property->GetName();
					}

					//プロパティの型をキャストして識別する
					UStructProperty* StructProperty = Cast<UStructProperty>(*PropIt);
					UObjectProperty* ObjectProperty = Cast<UObjectProperty>(*PropIt);
					if(StructProperty)
					{
						PropertyDocument.Type = StructProperty->Struct->GetName();
					}
					else if(ObjectProperty)
					{
						PropertyDocument.Type = ObjectProperty->PropertyClass->GetName();
					}
					else
					{
						PropertyDocument.Type = PropIt->GetClass()->GetName().Replace(TEXT("Property"), TEXT(""));
					}

					PropertyDocument.Category = Property->GetMetaData(TEXT("Category")).Replace(TEXT("|"), TEXT(" - "));
					PropertyDocument.ToolTips = Property->GetMetaData(TEXT("ToolTip"));
					
					PropertyDocument.PropertyFlg = Property->GetPropertyFlags();
					PropertyDocument.LifetimeCondition = Property->GetBlueprintReplicationCondition();

					BlueprintDocument.Properties.Add(PropertyDocument);
				}
			}
		}
		Document.Blueprints.Add(BlueprintDocument);
	}

	//HTMLへ出力
	DocumentToHTML(Document);

	// カテゴリごとのプロパティ
	for (FName CatagoryName : Settings->ListupCategories)
	{
		CategoryPageHTML(Document, CatagoryName);
	}
}


void UBlueprintToDocUtil::MakeEdGraphDocument(TArray<FEdGraphDocument>& OutEdGraphDocument, class UBlueprint* Blueprint, const TArray<UEdGraph*>& EdGraphs)
{
	UClass* const BPClass = Blueprint->GeneratedClass;
	FString BlueprintNameString = BPClass->GetName();

	for(UEdGraph* graph : EdGraphs)
	{
		FEdGraphDocument AddEdGraphDocument;

		AddEdGraphDocument.Parent = BlueprintNameString;

		UFunction* CheckFunction = BPClass->GetDefaultObject()->FindFunction(graph->GetFName());
		if(CheckFunction != nullptr)
		{
			AddEdGraphDocument.Name = CheckFunction->GetMetaData(FBlueprintMetadata::MD_DisplayName);
			if(AddEdGraphDocument.Name.IsEmpty())
			{
				AddEdGraphDocument.Name = graph->GetName();
			}
			AddEdGraphDocument.Category = CheckFunction->GetMetaData(TEXT("Category")).Replace(TEXT("|"), TEXT(" - "));
			AddEdGraphDocument.ToolTips = CheckFunction->GetMetaData(TEXT("ToolTip"));
			AddEdGraphDocument.Flag = (uint64)CheckFunction->FunctionFlags;
		}
		else
		{
			FGraphDisplayInfo DisplayInfo;
			graph->GetSchema()->GetGraphDisplayInformation(*graph, DisplayInfo);

			AddEdGraphDocument.Name = DisplayInfo.DisplayName.ToString();
			if(AddEdGraphDocument.Name.IsEmpty())
			{
				AddEdGraphDocument.Name = graph->GetName();
			}

			AddEdGraphDocument.ToolTips = DisplayInfo.Tooltip.ToString();
		}

		// 入出力ピン
		for(UEdGraphNode* Node : graph->Nodes)
		{
			bool IsFunctionEntry = (Node->GetName().Find(TEXT("FunctionEntry")) != -1);
			bool IsFunctionResult = (Node->GetName().Find(TEXT("FunctionResult")) != -1);

			if(IsFunctionEntry || IsFunctionResult)
			{
				TArray<UEdGraphPin*> AllPins = Node->GetAllPins();
				// エントリーノードにコメントがついている場合はToolTipsに追加
				if(IsFunctionEntry && !Node->NodeComment.IsEmpty())
				{
					AddEdGraphDocument.ToolTips += Node->NodeComment;
				}

				for(UEdGraphPin* Pin : AllPins)
				{
					FEdGraphPinDocument PinDocument;
					PinDocument.Name = Pin->PinName.ToString();
					if(Pin->PinType.PinSubCategoryObject != nullptr)
					{
						PinDocument.Type = Pin->PinType.PinSubCategoryObject->GetName();
					}
					else
					{
						PinDocument.Type = Pin->PinType.PinCategory.ToString();
					}
					PinDocument.DefaultValue = Pin->GetDefaultAsString();
					PinDocument.ToolTips = Pin->PinToolTip;

					if(IsFunctionEntry)
					{
						AddEdGraphDocument.Input.Add(PinDocument.Name, PinDocument);
					}
					else if(IsFunctionResult)
					{
						AddEdGraphDocument.Output.Add(PinDocument.Name,PinDocument);
					}
				}
			}
		}
		OutEdGraphDocument.Add(AddEdGraphDocument);
	}
}

void UBlueprintToDocUtil::DocumentToHTML(FDocument& Document)
{
	const UBlueprintToDocUserSettings* Settings = GetDefault<UBlueprintToDocUserSettings>();

	//目次ページ
	FString TOCOutput;
	FString LastTOCPath;
	Document.Blueprints.Sort(FBlueprintSorter());
	
	// カテゴリ プロパティのページのリンクは先頭に
	for (FName CatagoryName : Settings->ListupCategories)
	{
		TOCOutput += FString::Printf(TEXT("<a href=\"%s.html\" target=\"document\">%s</a></br>\n"), *(CatagoryName.ToString()), *(CatagoryName.ToString()));
	}

	// ブループリント
	for(FBlueprintDocument BlueprintDocument : Document.Blueprints)
	{
		FString FileOutput;
		//ソート
		BlueprintDocument.Events.Sort(FGraphSorter());
		BlueprintDocument.Macros.Sort(FGraphSorter());
		BlueprintDocument.Functions.Sort(FGraphSorter());
		BlueprintDocument.Properties.Sort(FPropertySorter());

		//BlueprintTOC
		if(LastTOCPath != BlueprintDocument.ContentPath)
		{
			LastTOCPath = BlueprintDocument.ContentPath;
			TopicHTML(TOCOutput, 3, BlueprintDocument.ContentPath);
		}
		TOCOutput += FString::Printf(TEXT("<a href=\"%s.html\" target=\"document\">%s</a></br>\n"),*(BlueprintDocument.Name),*(BlueprintDocument.Name));

		// タイトル
		TopicHTML(FileOutput,1,BlueprintDocument.Name);
		// 説明
		FileOutput += FString::Printf(TEXT("%s</br>"), *(BlueprintDocument.ToolTips));
		// Path
		TopicHTML(FileOutput, 2, FString(TEXT("ContentPath")));
		FileOutput += FString::Printf(TEXT("%s</br>"), *(BlueprintDocument.ContentPath));
		// Parent
		TopicHTML(FileOutput, 2, FString(TEXT("Parent")));
		FileOutput += FString::Printf(TEXT("%s</br>"), *(BlueprintDocument.ParentName));

		// Event
		TopicHTML(FileOutput, 2, FString(TEXT("EventGraph")));
		EdGraphTableHTML(FileOutput, BlueprintDocument.Events,Document);

		// Macro
		TopicHTML(FileOutput, 2, FString(TEXT("MacroGraph")));
		EdGraphTableHTML(FileOutput, BlueprintDocument.Macros,Document);

		// Function
		TopicHTML(FileOutput, 2, FString(TEXT("Functions")));
		EdGraphTableHTML(FileOutput, BlueprintDocument.Functions,Document);

		// Property
		TopicHTML(FileOutput, 2, FString(TEXT("Property")));
		PropertiesTableHTML(FileOutput, BlueprintDocument.Properties);

		// HTML出力
		FString Path = Document.RootPath / "blueprint" / (BlueprintDocument.Name) + ".html";
		if(!FFileHelper::SaveStringToFile(*FString::Printf(HTMLTemplate, *FileOutput), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogBlueprintToDoc, Error, TEXT("Failed Save File:%s"), *Path);
		}
	}

	// 目次ページ
	FString Path = Document.RootPath / "blueprint\\toc.html";
	if(!FFileHelper::SaveStringToFile(*FString::Printf(HTMLTemplate, *TOCOutput), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogBlueprintToDoc,Error,TEXT("Failed Save File:%s"), *Path);
	}


	// 必要なファイルをコピー
	IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FString LaytoutTemplateDirectory;

	// プロジェクトプラグインにプラグインが入っているか確認する
	LaytoutTemplateDirectory = FPaths::ProjectPluginsDir() / "BlueprintToDoc";
	if (!PlatformFile.DirectoryExists(*LaytoutTemplateDirectory))
	{
		// プロジェクト側になければエンジンプラグインとする
		LaytoutTemplateDirectory = FPaths::EnginePluginsDir() / "BlueprintToDoc";
	}
	// index ページのコピー
	{
		Path = Document.RootPath / "index.html";
		FString LaytoutTemplatePath = LaytoutTemplateDirectory / "HTMLTemplate/index.html";
		if (!PlatformFile.CopyFile(*Path, *LaytoutTemplatePath))
		{
			UE_LOG(LogBlueprintToDoc, Error, TEXT("Failed Copy File:%s"), *LaytoutTemplatePath);
		}
	}
	// CSSファイルのコピー
	{
		Path = Document.RootPath / "Layout.css";
		FString LaytoutTemplatePath = LaytoutTemplateDirectory / "HTMLTemplate/Layout.css";
		if (!PlatformFile.CopyFile(*Path, *LaytoutTemplatePath))
		{
			UE_LOG(LogBlueprintToDoc, Error, TEXT("Failed Copy File:%s"), *LaytoutTemplatePath);
		}
	}
}

void UBlueprintToDocUtil::TopicHTML(FString& OutString, int32 Level, const FString& Contents)
{
	OutString += FString::Printf(TEXT("<h%d>%s</h%d>\n"), Level, *Contents, Level);
}


FString UBlueprintToDocUtil::ReplaceHTMLReturenCode(const FString& Source)
{
	return Source.Replace(TEXT("\n"), TEXT("</br>\n"));
}

void UBlueprintToDocUtil::AddLine(FString& OutString, bool Result, FString AddString)
{
	if(Result)
	{
		OutString += FString::Printf(TEXT("%s</br>\n"), *AddString);
	}
}

void UBlueprintToDocUtil::EdGraphTableHTML(FString& OutString, const TArray<FEdGraphDocument>& EdGraphDocuments,FDocument& Document)
{
	const FString PinTableHeader = FString(TEXT("<table>\n<tr><th>Type</th><th>Name</th><th>TootTip</th></tr>\n"));
	const FString TableFooter = FString(TEXT("</table>\n"));

	// テーブルのヘッダー部分の追加
	OutString += FString(TEXT("<table>\n"));
	OutString += FString(TEXT("<tr>"));
	OutString += FString(TEXT("<th>Category</th>"));
	OutString += FString(TEXT("<th>Access</th>"));
	OutString += FString(TEXT("<th>Name</th>"));
	OutString += FString(TEXT("<th>TootTip</th>"));
	OutString += FString(TEXT("<th>Input</th>"));
	OutString += FString(TEXT("<th>Output</th>"));
	OutString += FString(TEXT("<th>Net</th>"));
	OutString += FString(TEXT("<th>Flag</th>"));
	OutString += FString(TEXT("</tr>\n"));

	for(FEdGraphDocument EdGraph : EdGraphDocuments)
	{
		FString HTMLFileName = FString::Printf(TEXT("%s_%s.html"), *(EdGraph.Parent), *(EdGraph.Name.Replace(TEXT(" "), TEXT(""))));
		FString Path = Document.RootPath / "blueprint" / HTMLFileName;

		FString FunctionHTMLString;
		FString FunctionInputString;
		FString FunctionOutputString;
		FString AccessString;
		FString NetString;
		FString FlagString;


		TopicHTML(FunctionHTMLString, 1, EdGraph.Name);
		FunctionHTMLString += FString::Printf(TEXT("%s</br>"), *(ReplaceHTMLReturenCode(EdGraph.ToolTips)));
		TopicHTML(FunctionHTMLString, 2, TEXT("Category"));
		FunctionHTMLString += FString::Printf(TEXT("%s</br>"), *(EdGraph.Category));

		if(EdGraph.TodoList.Num() > 0)
		{
			TopicHTML(FunctionHTMLString, 2, TEXT("Todo"));
			for(FString Todo : EdGraph.TodoList)
			{
				FunctionHTMLString += FString::Printf(TEXT("%s</br>"), *(Todo));
			}
		}

		// アクセス
		AddLine(AccessString, ((EdGraph.Flag & FUNC_Public) != 0),                 FString(TEXT("Public")));
		AddLine(AccessString, ((EdGraph.Flag & FUNC_Private) != 0),                FString(TEXT("Private")));
		AddLine(AccessString, ((EdGraph.Flag & FUNC_Protected) != 0),              FString(TEXT("Protected")));
		// ネット
		AddLine(NetString,    ((EdGraph.Flag & FUNC_NetResponse) != 0),            FString(TEXT("NetResponse")));
		AddLine(NetString,    ((EdGraph.Flag & FUNC_NetServer) != 0),              FString(TEXT("NetServer")));
		AddLine(NetString,    ((EdGraph.Flag & FUNC_NetMulticast) != 0),           FString(TEXT("NetMulticast")));
		AddLine(NetString,    ((EdGraph.Flag & FUNC_NetRequest) != 0),             FString(TEXT("NetRequest")));
		AddLine(NetString,    ((EdGraph.Flag & FUNC_NetClient) != 0),              FString(TEXT("NetClient")));
		AddLine(NetString,    ((EdGraph.Flag & FUNC_NetReliable) != 0),            FString(TEXT("NetReliable")));
		// FunctionFlag
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_Final) != 0),                  FString(TEXT("Final")));
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_RequiredAPI) != 0),            FString(TEXT("RequiredAPI")));
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_BlueprintAuthorityOnly) != 0), FString(TEXT("BlueprintAuthorityOnly")));
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_RequiredAPI) != 0),            FString(TEXT("RequiredAPI")));
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_BlueprintCosmetic) != 0),      FString(TEXT("BlueprintCosmetic")));
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_Exec) != 0),                   FString(TEXT("Exec")));
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_Native) != 0),                 FString(TEXT("Native")));
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_Event) != 0),                  FString(TEXT("Event")));
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_Static) != 0),                 FString(TEXT("Static")));
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_MulticastDelegate) != 0),      FString(TEXT("MulticastDelegate")));
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_BlueprintCallable) != 0),      FString(TEXT("BlueprintCallable")));
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_BlueprintEvent) != 0),         FString(TEXT("BlueprintEvent")));
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_BlueprintPure) != 0),          FString(TEXT("BlueprintPure")));
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_EditorOnly) != 0),             FString(TEXT("EditorOnly")));
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_Const) != 0),                  FString(TEXT("Const")));
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_NetValidate) != 0),            FString(TEXT("NetValidate")));
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_Delegate) != 0),               FString(TEXT("Delegate")));
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_HasOutParms) != 0),            FString(TEXT("HasOutParms")));
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_HasDefaults) != 0),            FString(TEXT("HasDefaults")));
		AddLine(FlagString,   ((EdGraph.Flag & FUNC_DLLImport) != 0),              FString(TEXT("DLLImport")));


		if(!AccessString.IsEmpty())
		{
			TopicHTML(FunctionHTMLString, 2, TEXT("Access"));
			FunctionHTMLString += AccessString;
		}
		if(!NetString.IsEmpty())
		{
			TopicHTML(FunctionHTMLString, 2, TEXT("Net"));
			FunctionHTMLString += NetString;
		}
		if(!FlagString.IsEmpty())
		{
			TopicHTML(FunctionHTMLString, 2, TEXT("FunctionFlag"));
			FunctionHTMLString += FlagString;
		}

		// インプットピン
		TopicHTML(FunctionHTMLString, 2, TEXT("Input"));
		FunctionHTMLString += PinTableHeader;
		TArray<FString> PinKeys;
		EdGraph.Input.GetKeys(PinKeys);
		for(FString PinKey : PinKeys)
		{
			const FEdGraphPinDocument* PinDocument = EdGraph.Input.Find(PinKey);
			if(PinDocument == nullptr)
			{
				continue;
			}

			// 関数ドキュメント用
			FunctionHTMLString += FString(TEXT("<tr>"));
			FunctionHTMLString += FString::Printf(TEXT("<td>%s</td>"), *(PinDocument->Type));
			FunctionHTMLString += FString::Printf(TEXT("<td>%s</td>"), *(PinDocument->Name));
			FunctionHTMLString += FString::Printf(TEXT("<td>%s</td>"), *(PinDocument->ToolTips));
			FunctionHTMLString += FString(TEXT("</tr>"));

			// ブループリントドキュメント用
			FunctionInputString += FString::Printf(TEXT("<b>%s</b>　<i>%s</i></br>"), *(PinDocument->Type), *(PinDocument->Name));
			FunctionInputString += FString::Printf(TEXT("%s</br>"), *(PinDocument->ToolTips));
		}
		FunctionHTMLString += TableFooter;

		// アウトプットピン
		TopicHTML(FunctionHTMLString, 2, TEXT("Output"));
		FunctionHTMLString += PinTableHeader;
		EdGraph.Output.GetKeys(PinKeys);
		for(FString PinKey : PinKeys)
		{
			const FEdGraphPinDocument* PinDocument = EdGraph.Output.Find(PinKey);
			if(PinDocument == nullptr)
			{
				continue;
			}

			// 関数ドキュメント用
			FunctionHTMLString += FString(TEXT("<tr>"));
			FunctionHTMLString += FString::Printf(TEXT("<td>%s</td>"), *(PinDocument->Type));
			FunctionHTMLString += FString::Printf(TEXT("<td>%s</td>"), *(PinDocument->Name));
			FunctionHTMLString += FString::Printf(TEXT("<td>%s</td>"), *(PinDocument->ToolTips));
			FunctionHTMLString += FString(TEXT("</tr>"));

			// ブループリントドキュメント用
			FunctionOutputString += FString::Printf(TEXT("<b>%s</b>　<i>%s</i></br>"), *(PinDocument->Type), *(PinDocument->Name));
			FunctionOutputString += FString::Printf(TEXT("%s</br>"), *(PinDocument->ToolTips));
		}
		FunctionHTMLString += TableFooter;


		// 関数ドキュメント ファイル出力
		FString OutHTML = FString::Printf(HTMLTemplate, *FunctionHTMLString);
		if(!FFileHelper::SaveStringToFile(*OutHTML, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogBlueprintToDoc,Error,TEXT("Failed Save File:%s"),*Path);
		}

		OutString += FString::Printf(TEXT("<tr>"));
		OutString += FString::Printf(TEXT("<td>%s</td>"), *(EdGraph.Category));
		OutString += FString::Printf(TEXT("<td>%s</td>"), *(AccessString));
		OutString += FString::Printf(TEXT("<td><a href=\"%s\" target=\"document\">%s</a></td>\n"), *(HTMLFileName), *(EdGraph.Name));
		OutString += FString::Printf(TEXT("<td>%s</td>"), *(ReplaceHTMLReturenCode(EdGraph.ToolTips)));
		OutString += FString::Printf(TEXT("<td>%s</td>"), *FunctionInputString);
		OutString += FString::Printf(TEXT("<td>%s</td>"), *FunctionOutputString);
		OutString += FString::Printf(TEXT("<td>%s</td>"), *(NetString));
		OutString += FString::Printf(TEXT("<td>%s</td>"), *(FlagString));
		OutString += FString::Printf(TEXT("</tr>\n"));
	}
	OutString += FString::Printf(TEXT("</table>\n"));
}



void UBlueprintToDocUtil::PropertiesTableHTML(FString& OutString, const TArray<FPropertyDocument>& PropertiesDocuments)
{
	OutString += FString(TEXT("<table>\n"));
	OutString += FString(TEXT("<tr>"));
	OutString += FString(TEXT("<th>Category</th>"));
	OutString += FString(TEXT("<th>Access</th>"));
	OutString += FString(TEXT("<th>Type</th>"));
	OutString += FString(TEXT("<th>Name</th>"));
	OutString += FString(TEXT("<th>TootTip</th>"));
	OutString += FString(TEXT("<th>Net</th>"));
	OutString += FString(TEXT("<th>Flag</th>"));
	OutString += FString(TEXT("</tr>\n"));

	for(FPropertyDocument PropertyDocument : PropertiesDocuments)
	{
		FString AccessString;
		FString NetString;
		FString FlagString;

		// 各プロパティフラグの情報を文字列化
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_Edit)!=0),                  FString(TEXT("Edit")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_ConstParm)!=0),             FString(TEXT("ConstParm")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_BlueprintVisible)!=0),      FString(TEXT("BlueprintVisible")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_ExportObject)!=0),          FString(TEXT("ExportObject")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_Parm)!=0),                  FString(TEXT("Parm")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_ZeroConstructor)!=0),       FString(TEXT("ZeroConstructor")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_DisableEditOnTemplate)!=0), FString(TEXT("DisableEditOnTemplate")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_Transient)!=0),             FString(TEXT("Transient")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_Config)!=0),                FString(TEXT("Config")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_DisableEditOnInstance)!=0), FString(TEXT("DisableEditOnInstance")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_EditConst)!=0),             FString(TEXT("EditConst")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_GlobalConfig)!=0),          FString(TEXT("GlobalConfig")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_InstancedReference)!=0),    FString(TEXT("InstancedReference")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_DuplicateTransient)!=0),    FString(TEXT("DuplicateTransient")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_SubobjectReference)!=0),    FString(TEXT("SubobjectReference")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_SaveGame)!=0),              FString(TEXT("SaveGame")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_ReferenceParm)!=0),         FString(TEXT("ReferenceParm")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_Deprecated)!=0),            FString(TEXT("Deprecated")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_IsPlainOldData)!=0),        FString(TEXT("IsPlainOldData")));
		AddLine(NetString,   ((PropertyDocument.PropertyFlg&CPF_Net)!=0),                   FString(TEXT("Net")));
		AddLine(NetString,   ((PropertyDocument.PropertyFlg&CPF_RepSkip)!=0),               FString(TEXT("RepSkip")));
		AddLine(NetString,   ((PropertyDocument.PropertyFlg&CPF_RepNotify)!=0),             FString(TEXT("RepNotify")));
		AddLine(AccessString,((PropertyDocument.PropertyFlg&CPF_Protected)!=0),             FString(TEXT("Protected")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_ExposeOnSpawn)!=0),         FString(TEXT("ExposeOnSpawn")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_EditorOnly)!=0),            FString(TEXT("EditorOnly")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_Interp)!=0),                FString(TEXT("Interp")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_NonTransactional)!=0),      FString(TEXT("NonTransactional")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_BlueprintCallable)!=0),     FString(TEXT("BlueprintCallable")));
		AddLine(FlagString,  ((PropertyDocument.PropertyFlg&CPF_BlueprintAuthorityOnly)!=0),FString(TEXT("BlueprintAuthorityOnly")));

		// Lifetime
		AddLine(NetString, true, *LifetimeConditionToString(PropertyDocument.LifetimeCondition));

		OutString += FString::Printf(TEXT("<tr>"));
		OutString += FString::Printf(TEXT("<td>%s</td>"), *(PropertyDocument.Category));
		OutString += FString::Printf(TEXT("<td>%s</td>"), *(AccessString));
		OutString += FString::Printf(TEXT("<td>%s</td>"), *(PropertyDocument.Type));
		OutString += FString::Printf(TEXT("<td>%s</td>"), *(PropertyDocument.Name));
		OutString += FString::Printf(TEXT("<td>%s</td>"), *(ReplaceHTMLReturenCode(PropertyDocument.ToolTips)));
		OutString += FString::Printf(TEXT("<td>%s</td>"), *(NetString));
		OutString += FString::Printf(TEXT("<td>%s</td>"), *(FlagString));
		OutString += FString::Printf(TEXT("</tr>\n"));
	}
	OutString += FString::Printf(TEXT("</table>\n"));
}

void UBlueprintToDocUtil::CategoryPageHTML(FDocument& Document, FName Category)
{
	FString HTMLOutput;
	FString LastTOCPath;
	Document.Blueprints.Sort(FBlueprintSorter());


	// ブループリント
	for (FBlueprintDocument BlueprintDocument : Document.Blueprints)
	{
		FString OutString;
		//ソート
		BlueprintDocument.Properties.Sort(FPropertySorter());

		for (FPropertyDocument PropertyDocument : BlueprintDocument.Properties)
		{
			if (PropertyDocument.Category.Find(Category.ToString()) != INDEX_NONE)
			{
				OutString += FString::Printf(TEXT("<tr>"));
				OutString += FString::Printf(TEXT("<td>%s</td>"), *(PropertyDocument.Name));
				OutString += FString::Printf(TEXT("<td>%s</td>"), *(PropertyDocument.Type));
				OutString += FString::Printf(TEXT("<td>%s</td>"), *(PropertyDocument.Category));
				OutString += FString::Printf(TEXT("<td>%s</td>"), *(ReplaceHTMLReturenCode(PropertyDocument.ToolTips)));
				OutString += FString::Printf(TEXT("</tr>\n"));
			}
		}
		if (!OutString.IsEmpty())
		{
			TopicHTML(HTMLOutput, 2, FString::Printf(TEXT("<a href=\"%s.html\" target=\"document\">%s</a></br>\n"), *(BlueprintDocument.Name), *(BlueprintDocument.Name)));

			HTMLOutput += FString(TEXT("<table>\n"));
			HTMLOutput += FString(TEXT("<tr>"));
			HTMLOutput += FString(TEXT("<th>Name</th>"));
			HTMLOutput += FString(TEXT("<th>Type</th>"));
			HTMLOutput += FString(TEXT("<th>Category</th>"));
			HTMLOutput += FString(TEXT("<th>TootTip</th>"));
			HTMLOutput += FString(TEXT("</tr>\n"));
			HTMLOutput += OutString;
			HTMLOutput += FString(TEXT("</table>"));
		}
	}


	FString Path = Document.RootPath / "blueprint" / (Category.ToString()+".html");

	if (!FFileHelper::SaveStringToFile(*FString::Printf(HTMLTemplate, *HTMLOutput), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogBlueprintToDoc, Error, TEXT("Failed Save File:%s"), *Path);
	}

}

FString UBlueprintToDocUtil::LifetimeConditionToString(ELifetimeCondition Condition)
{
	// Lifetime
	switch (Condition)
	{
	case COND_None:							return FString(TEXT("None"));					
	case COND_InitialOnly:					return FString(TEXT("Initial Only"));					// This property will only attempt to send on the initial bunch
	case COND_OwnerOnly:					return FString(TEXT("Owner Only"));						// This property will only send to the actor's owner
	case COND_SkipOwner:					return FString(TEXT("Skip Owner"));						// This property send to every connection EXCEPT the owner
	case COND_SimulatedOnly:				return FString(TEXT("Simulated Only"));					// This property will only send to simulated actors
	case COND_AutonomousOnly:				return FString(TEXT("Autonomous Only"));					// This property will only send to autonomous actors
	case COND_SimulatedOrPhysics:			return FString(TEXT("Simulated Or Physics"));			// This property will send to simulated OR bRepPhysics actors
	case COND_InitialOrOwner:				return FString(TEXT("Initial Or Owner"));				// This property will send on the initial packet, or to the actors owner
	case COND_Custom:						return FString(TEXT("Custom"));							// This property has no particular condition, but wants the ability to toggle on/off via SetCustomIsActiveOverride
	case COND_ReplayOrOwner:				return FString(TEXT("Replay Or Owner"));					// This property will only send to the replay connection, or to the actors owner
	case COND_ReplayOnly:					return FString(TEXT("Replay Only"));						// This property will only send to the replay connection
	case COND_SimulatedOnlyNoReplay:		return FString(TEXT("Simulated Only No Replay"));		// This property will send to actors only, but not to replay connections
	case COND_SimulatedOrPhysicsNoReplay:	return FString(TEXT("Simulated Or Physics No Replay"));	// This property will send to simulated Or bRepPhysics actors, but not to replay connections
	case COND_SkipReplay:					return FString(TEXT("Skip Replay"));						// This property will not send to the replay connection
	case COND_Max:
		break;
	default:
		break;
	}
	return FString(TEXT(""));
}
