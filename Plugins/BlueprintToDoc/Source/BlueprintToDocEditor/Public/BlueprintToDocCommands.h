// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "BlueprintToDocStyle.h"

class FBlueprintToDocCommands : public TCommands<FBlueprintToDocCommands>
{
public:

	FBlueprintToDocCommands()
		: TCommands<FBlueprintToDocCommands>(TEXT("BlueprintToDoc"), NSLOCTEXT("Contexts", "BlueprintToDoc", "BlueprintToDoc Plugin"), NAME_None, FBlueprintToDocStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
