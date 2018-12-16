// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CoreNetTypes.h"
#include "BlueprintToDocUtil.generated.h"

/**
 * イベントグラフピンドキュメント化情報
 */
USTRUCT()
struct FEdGraphPinDocument
{
	GENERATED_BODY()
	// グラフ名
	UPROPERTY()
	FString Name;
	UPROPERTY()
	FString Type;
	// 初期値
	UPROPERTY()
	FString DefaultValue;
	// ToolTips
	UPROPERTY()
	FString ToolTips;
};

/**
 * イベントグラフドキュメント化情報
 */
USTRUCT()
struct FEdGraphDocument
{
	GENERATED_BODY()
	// グラフ名
	UPROPERTY()
	FString Name;
	// ペアレント
	UPROPERTY()
	FString Parent;
	// カテゴリ
	UPROPERTY()
	FString Category;
	// ファンクションフラグ
	uint64 Flag;
	// 入力
	UPROPERTY()
	TMap<FString,FEdGraphPinDocument> Input;
	// 出力
	UPROPERTY()
	TMap<FString,FEdGraphPinDocument> Output;
	// ToolTips
	UPROPERTY()
	FString ToolTips;
	// Todo
	UPROPERTY()
	TArray<FString> TodoList;
};

/**
 * プロパティドキュメント化情報
 */
USTRUCT()
struct FPropertyDocument
{
	GENERATED_BODY()

	// プロパティ名
	UPROPERTY()
	FString Name;
	// タイプ
	UPROPERTY()
	FString Type;
	// カテゴリ
	UPROPERTY()
	FString Category;
	// プロパティフラグ
	int64 PropertyFlg;
	// 初期値
	UPROPERTY()
	FString DefaultValue;
	// ToopTips
	UPROPERTY()
	FString ToolTips;
	// LifeTimeCondition
	UPROPERTY()
	TEnumAsByte<ELifetimeCondition> LifetimeCondition;
};

/**
 * ブループリントドキュメント化情報
 */
USTRUCT()
struct FBlueprintDocument
{
	GENERATED_BODY()

	// Blueprint名
	UPROPERTY()
	FString Name;
	// ToopTips
	UPROPERTY()
	FString ToolTips;
	// Parentクラス
	UPROPERTY()
	FString ParentName;
	// ContentPath
	UPROPERTY()
	FString ContentPath;
	// EventGraph
	UPROPERTY()
	TArray<FEdGraphDocument> Events;
	// MacroGraph
	UPROPERTY()
	TArray<FEdGraphDocument> Macros;
	// Functions
	UPROPERTY()
	TArray<FEdGraphDocument> Functions;
	// Component
	UPROPERTY()
	TArray<FEdGraphDocument> Components;
	// Property
	UPROPERTY()
	TArray<FPropertyDocument> Properties;
};

/**
 * ドキュメント化情報
 */
USTRUCT()
struct FDocument
{
	GENERATED_BODY()

	//出力するパス
	UPROPERTY()
	FString RootPath;

	// ブループリント
	UPROPERTY()
	TArray<FBlueprintDocument> Blueprints;
};

/**
 * ドキュメント化 ユティリティ
 */
UCLASS()
class BLUEPRINTTODOC_API UBlueprintToDocUtil : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "BlueprintToDoc")
	static void BlueprintToDoc();

	static void BlueprintToDoc_Exec(FDocument& Document);

private:
	// EdGraphから情報を取得する
	static void MakeEdGraphDocument(TArray<FEdGraphDocument>& OutEdGraphDocument, class UBlueprint* Blueprint, const TArray<class UEdGraph*>& EdGraphs);

	// ドキュメント情報からHTMLを出力
	static void DocumentToHTML(FDocument& Document);
	// 見出し
	static void TopicHTML(FString& OutString, int32 Level, const FString& Contents);
	// 改行の置換
	static FString ReplaceHTMLReturenCode(const FString& Source);
	// 条件による１行追加
	static void AddLine(FString& OutString, bool Result, FString AddString);
	// EdGraphをHTMLへ
	static void EdGraphTableHTML(FString& OutString, const TArray<FEdGraphDocument>& EdGraphDocuments,FDocument& Document);
	// PropertyをHTML
	static void PropertiesTableHTML(FString& OutString, const TArray<FPropertyDocument>& PropertiesDocuments);
	// カテゴリのページ作成
	static void CategoryPageHTML(FDocument& Document, FName Category);
	// ELifetimeConditionの文字列取得
	static FString LifetimeConditionToString(ELifetimeCondition Condition);

};
