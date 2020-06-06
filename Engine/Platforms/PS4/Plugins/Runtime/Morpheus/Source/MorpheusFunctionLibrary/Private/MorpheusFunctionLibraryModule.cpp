// Copyright Epic Games, Inc. All Rights Reserved.

#include "MorpheusFunctionLibraryModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "MorpheusFunctionLibrary"

//////////////////////////////////////////////////////////////////////////
// FMorpheusFunctionLibraryModule

class FMorpheusFunctionLibraryModule : public IMorpheusFunctionLibraryModule
{
};

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FMorpheusFunctionLibraryModule, MorpheusFunctionLibrary);

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE