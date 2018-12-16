// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintToDocCommands.h"

#define LOCTEXT_NAMESPACE "FBlueprintToDocModule"

void FBlueprintToDocCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "BlueprintToDoc", "Execute BlueprintToDoc action", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
