// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS4TargetSettings.h"

/* UPS4TargetSettings structors
 *****************************************************************************/

UPS4TargetSettings::UPS4TargetSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	bEnableRazorCPUEvents = true;
	bEnableGPUDebugger = false;
	bEnableSulphaDebugger = false;
	bEnableMemoryAnalyzer = false;
	bEnableAddressSanitizer = false;
	bEnableUndefinedBehaviorSanitizer = false;
	bEnableShaderPerfAnalysis = false;
	bEnableGnmLCUEDebug = false;
	bEnableCompanionApp = false;
	bEnableLTOPerfBuilds = false;
	bEnableLTODevBuilds = false;
	BuildIsoImage = false;
	MoveFilesToOuterEdge = false;
	bUsesDownloadZero = false;
	bEnableGnmUnsafeCommandBuffers = false;
	bEnableNewMemorySystem = false;
	bEnableGPUDefragger = false;
	GnmUnsafeCommandBufferReserveSizeInBytes = 8 * 1024;
	bUse7thCore = false;
	DeployMode = EPS4DeployMode::RunFromHost;

	StorageType = PPST_BD25;
	AppType = PPAT_Full;
}  
