// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintToDoc.h"
#include "BlueprintToDocUtil.h"



static const FName BlueprintToDocTabName("BlueprintToDoc");

#define LOCTEXT_NAMESPACE "FBlueprintToDocModule"

void FBlueprintToDocModule::StartupModule()
{

}

void FBlueprintToDocModule::ShutdownModule()
{
}


#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBlueprintToDocModule, BlueprintToDoc)


