// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintToDocUserSettings.generated.h"

/**
 * BlueprintToDoc Settings
 */
UCLASS(config=BlueprintToDocUserSettings)
class BLUEPRINTTODOC_API UBlueprintToDocUserSettings:public UObject
{
	GENERATED_BODY()

public:
	// 出力先
	UPROPERTY(config, EditAnywhere, Category=Path, meta=(RelativePath))
	FDirectoryPath DocumentRootPath;

	// ドキュメント化するパス
	UPROPERTY(config, EditAnywhere, Category = Path, meta = (RelativePath))
	TArray<FName> ContentPaths;

	// 抽出するプロパティのカテゴリ
	UPROPERTY(config, EditAnywhere, Category = Property)
	TArray<FName> ListupCategories;
};
