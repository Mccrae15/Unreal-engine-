// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusXRSceneActor.h"
#include "OculusXRSceneModule.h"
#include "OculusXRHMDModule.h"
#include "OculusXRAnchorManager.h"
#include "OculusXRAnchorTypes.h"
#include "OculusXRAnchorBPFunctionLibrary.h"
#include "OculusXRDelegates.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

#define LOCTEXT_NAMESPACE "OculusXRSceneActor"

//////////////////////////////////////////////////////////////////////////
// ASceneActor

AOculusXRSceneActor::AOculusXRSceneActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ResetStates();

	// Create required components
	RoomLayoutManagerComponent = CreateDefaultSubobject<UOculusXRRoomLayoutManagerComponent>(TEXT("OculusXRRoomLayoutManagerComponent"));

	// Following are the semantic labels we want to support default properties for.  User can always add new ones through the properties panel if needed.
	const FString default2DSemanticClassifications[] = {
		TEXT("WALL_FACE"),
		TEXT("CEILING"),
		TEXT("FLOOR"),
		TEXT("COUCH"),
		TEXT("DESK"),
		TEXT("DOOR_FRAME"),
		TEXT("WINDOW_FRAME"),
		TEXT("STORAGE"),
		TEXT("OTHER")
	};

	const FString default3DSemanticClassifications[] = {
		TEXT("COUCH"),
		TEXT("DESK"),
		TEXT("SCREEN"),
		TEXT("BED"),
		TEXT("LAMP"),
		TEXT("PLANT"),
		TEXT("STORAGE"),
		TEXT("OTHER")
	};

	FOculusXRSpawnedSceneAnchorProperties spawnedAnchorProps;
	spawnedAnchorProps.ActorComponent = nullptr;
	spawnedAnchorProps.StaticMesh = nullptr;

	// Setup initial scene plane and volume properties
	for (auto& semanticLabel2D : default2DSemanticClassifications)
	{
		FOculusXRSpawnedSceneAnchorProperties& props = ScenePlaneSpawnedSceneAnchorProperties.Add(semanticLabel2D, spawnedAnchorProps);
		props.ForceParallelToFloor = (semanticLabel2D != "WALL_FACE");
	}

	for (auto& semanticLabel3D : default3DSemanticClassifications)
	{
		FOculusXRSpawnedSceneAnchorProperties& props = SceneVolumeSpawnedSceneAnchorProperties.Add(semanticLabel3D, spawnedAnchorProps);
		props.ForceParallelToFloor = true;
	}
}

void AOculusXRSceneActor::ResetStates()
{		
	bCaptureFlowWasLaunched = false;
	ClearScene();
}

void AOculusXRSceneActor::BeginPlay()
{
	Super::BeginPlay();

	// Create a scene component as root so we can attach spawned actors to it
	USceneComponent* rootSceneComponent = NewObject<USceneComponent>(this, USceneComponent::StaticClass());
	rootSceneComponent->SetMobility(EComponentMobility::Static);
	rootSceneComponent->RegisterComponent();
	SetRootComponent(rootSceneComponent);

	// Register delegates
	RoomLayoutManagerComponent->OculusXRRoomLayoutSceneCaptureCompleteNative.AddUObject(this, &AOculusXRSceneActor::SceneCaptureComplete_Handler);

	// Make an initial request to query for the room layout if bPopulateSceneOnBeginPlay was set to true
	if (bPopulateSceneOnBeginPlay)
	{
		PopulateScene();
	}
}

void AOculusXRSceneActor::EndPlay(EEndPlayReason::Type Reason)
{
	// Unregister delegates
	RoomLayoutManagerComponent->OculusXRRoomLayoutSceneCaptureCompleteNative.RemoveAll(this);

	// Calling ResetStates will reset member variables to their default values (including the request IDs).
	ResetStates();

	Super::EndPlay(Reason);
}

void AOculusXRSceneActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool AOculusXRSceneActor::IsValidUuid(const FOculusXRUUID& Uuid)
{
	return Uuid.UUIDBytes != nullptr;
}

void AOculusXRSceneActor::LaunchCaptureFlow()
{
	UE_LOG(LogOculusXRScene, Verbose, TEXT("Launch capture flow"));

	if (RoomLayoutManagerComponent)
	{
		UE_LOG(LogOculusXRScene, Verbose, TEXT("Launch capture flow -- RoomLayoutManagerComponent"));

		const bool bResult = RoomLayoutManagerComponent->LaunchCaptureFlow();
		if (!bResult)
		{
			UE_LOG(LogOculusXRScene, Error, TEXT("LaunchCaptureFlow() failed!"));
		}
	}
}

void AOculusXRSceneActor::LaunchCaptureFlowIfNeeded()
{
#if WITH_EDITOR
	UE_LOG(LogOculusXRScene, Display, TEXT("Scene Capture does not work over Link. Please capture a scene with the HMD in standalone mode, then access the scene model over Link."));
#else
	// Depending on LauchCaptureFlowWhenMissingScene, we might not want to launch Capture Flow
	if (LauchCaptureFlowWhenMissingScene != EOculusXRLaunchCaptureFlowWhenMissingScene::NEVER)
	{
		if (LauchCaptureFlowWhenMissingScene == EOculusXRLaunchCaptureFlowWhenMissingScene::ALWAYS ||
			(!bCaptureFlowWasLaunched && LauchCaptureFlowWhenMissingScene == EOculusXRLaunchCaptureFlowWhenMissingScene::ONCE))
		{	
			LaunchCaptureFlow();
		}
	}
#endif
}

USceneComponent* AOculusXRSceneActor::GetOrCreateRoomOriginSceneComponent(const FOculusXRUInt64& RoomSpaceID)
{
	USceneComponent** foundSceneComponent = RoomSceneComponents.Find(RoomSpaceID);

	if (foundSceneComponent != nullptr)
	{
		return *foundSceneComponent;
	}

	USceneComponent* roomSceneComponent = NewObject<USceneComponent>(this, USceneComponent::StaticClass());
	roomSceneComponent->CreationMethod = EComponentCreationMethod::Instance;
	AddOwnedComponent(roomSceneComponent);

	roomSceneComponent->SetupAttachment(RootComponent);
	roomSceneComponent->RegisterComponent();
	roomSceneComponent->InitializeComponent();
	roomSceneComponent->Activate();

	RoomSceneComponents.Add(RoomSpaceID) = roomSceneComponent;

	return roomSceneComponent;
}

bool AOculusXRSceneActor::SpawnSceneAnchor(const FOculusXRUInt64& Space, const FOculusXRUInt64& RoomSpaceID, const FVector& BoundedSize, const TArray<FString>& SemanticClassifications, const EOculusXRSpaceComponentType AnchorComponentType)
{
	if (Space.Value == 0)
	{
		UE_LOG(LogOculusXRScene, Error, TEXT("AOculusXRSceneActor::SpawnSceneAnchor Invalid Space handle."));
		return false;
	}

	if (!(AnchorComponentType == EOculusXRSpaceComponentType::ScenePlane || AnchorComponentType == EOculusXRSpaceComponentType::SceneVolume))
	{
		UE_LOG(LogOculusXRScene, Error, TEXT("AOculusXRSceneActor::SpawnSceneAnchor Anchor doesn't have ScenePlane or SceneVolume component active."));
		return false;
	}

	if (0 == SemanticClassifications.Num())
	{
		UE_LOG(LogOculusXRScene, Error, TEXT("AOculusXRSceneActor::SpawnSceneAnchor No semantic classification found."));
		return false;
	}

	USceneComponent* roomRoot = GetOrCreateRoomOriginSceneComponent(RoomSpaceID);

	TSoftClassPtr<UOculusXRSceneAnchorComponent>* sceneAnchorComponentClassPtrRef = nullptr;
	TSoftObjectPtr<UStaticMesh>* staticMeshObjPtrRef = nullptr;

	FOculusXRSpawnedSceneAnchorProperties* foundProperties = (AnchorComponentType == EOculusXRSpaceComponentType::ScenePlane) ? ScenePlaneSpawnedSceneAnchorProperties.Find(SemanticClassifications[0]) : SceneVolumeSpawnedSceneAnchorProperties.Find(SemanticClassifications[0]);

	if (!foundProperties)
	{
		UE_LOG(LogOculusXRScene, Warning, TEXT("AOculusXRSceneActor::SpawnSceneAnchor Scene object has an unknown semantic label.  Will not be spawned."));
		return false;
	}

	sceneAnchorComponentClassPtrRef = &foundProperties->ActorComponent;
	staticMeshObjPtrRef = &foundProperties->StaticMesh;

	UClass* sceneAnchorComponentInstanceClass = sceneAnchorComponentClassPtrRef->LoadSynchronous();
	if (!sceneAnchorComponentInstanceClass)
	{
		UE_LOG(LogOculusXRScene, Error, TEXT("AOculusXRSceneActor::SpawnSceneAnchor Scene anchor component class is invalid!  Cannot spawn actor to populate the scene."));
		return false;
	}
	
	FActorSpawnParameters actorSpawnParams;
	actorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* newActor = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, actorSpawnParams);
	newActor->GetRootComponent()->SetMobility(EComponentMobility::Movable);
	newActor->AttachToComponent(roomRoot, FAttachmentTransformRules::KeepRelativeTransform);
	
	if (staticMeshObjPtrRef->IsPending())
	{
		staticMeshObjPtrRef->LoadSynchronous();
	}
	newActor->GetStaticMeshComponent()->SetStaticMesh(staticMeshObjPtrRef->Get());
	
	UOculusXRSceneAnchorComponent* sceneAnchorComponent = NewObject<UOculusXRSceneAnchorComponent>(newActor, sceneAnchorComponentInstanceClass);
	sceneAnchorComponent->CreationMethod = EComponentCreationMethod::Instance;
	newActor->AddOwnedComponent(sceneAnchorComponent);

	sceneAnchorComponent->RegisterComponent();
	sceneAnchorComponent->InitializeComponent();
	sceneAnchorComponent->Activate();
	sceneAnchorComponent->SetHandle(Space);
	sceneAnchorComponent->SemanticClassifications = SemanticClassifications;
	sceneAnchorComponent->ForceParallelToFloor = foundProperties->ForceParallelToFloor;
	sceneAnchorComponent->AddOffset = foundProperties->AddOffset;

	EOculusXRAnchorResult::Type Result;
	OculusXRAnchors::FOculusXRAnchors::SetAnchorComponentStatus(sceneAnchorComponent, EOculusXRSpaceComponentType::Locatable, true, 0.0f, FOculusXRAnchorSetComponentStatusDelegate(), Result);

	// Setup scale based on bounded size and the actual size of the mesh
	UStaticMesh* staticMesh = newActor->GetStaticMeshComponent()->GetStaticMesh();
	FBoxSphereBounds staticMeshBounds;
	staticMeshBounds.BoxExtent = FVector{1.f, 1.f, 1.f};
	if (staticMesh)
	{
		staticMeshBounds = staticMesh->GetBounds();
	}
	const float worldToMeters = GetWorld()->GetWorldSettings()->WorldToMeters;

	newActor->SetActorScale3D(FVector(
		(BoundedSize.X / (staticMeshBounds.BoxExtent.X * 2.f)) * worldToMeters,
		(BoundedSize.Y / (staticMeshBounds.BoxExtent.Y * 2.f)) * worldToMeters,
		(BoundedSize.Z / (staticMeshBounds.BoxExtent.Z * 2.f)) * worldToMeters)
	);

	return true;
}

bool AOculusXRSceneActor::IsScenePopulated()
{
	if (!RootComponent) 
	{
		return false;
	}

	return RootComponent->GetNumChildrenComponents() > 0;
}

bool AOculusXRSceneActor::IsRoomLayoutValid()
{
	return true;
}

void AOculusXRSceneActor::PopulateScene()
{
	if (!RootComponent)
	{
		return;
	}

	const EOculusXRAnchorResult::Type result = QueryAllRooms();
	if (!UOculusXRAnchorBPFunctionLibrary::IsAnchorResultSuccess(result))
	{
		UE_LOG(LogOculusXRScene, Error, TEXT("PopulateScene Failed to query available rooms"));
	}
}

void AOculusXRSceneActor::ClearScene()
{
	for (auto& roomIterator : RoomSceneComponents)
	{
		TArray<USceneComponent*> childrenComponents = roomIterator.Value->GetAttachChildren();	
		for (USceneComponent* sceneComponent : childrenComponents)
		{
			Cast<AActor>(sceneComponent->GetOuter())->Destroy();
		}

		roomIterator.Value->DestroyComponent();
	}

	RoomSceneComponents.Empty();
	bFoundCapturedScene = false;
}

void AOculusXRSceneActor::SetVisibilityToAllSceneAnchors(const bool bIsVisible)
{
	for (auto& roomIterator : RoomSceneComponents)
	{
		TArray<USceneComponent*> childrenComponents = roomIterator.Value->GetAttachChildren();
		for (USceneComponent* sceneComponent : childrenComponents)
		{
			sceneComponent->SetVisibility(bIsVisible);
		}
	}
}

void AOculusXRSceneActor::SetVisibilityToSceneAnchorsBySemanticLabel(const FString SemanticLabel, const bool bIsVisible)
{
	for (auto& roomIterator : RoomSceneComponents)
	{
		TArray<USceneComponent*> childrenComponents = roomIterator.Value->GetAttachChildren();
		for (USceneComponent* sceneComponent : childrenComponents)
		{
			AActor* outerActor = Cast<AActor>(sceneComponent->GetOuter());
			if (!outerActor)
			{
				continue;
			}

			UActorComponent* sceneAnchorComponent = outerActor->GetComponentByClass(UOculusXRSceneAnchorComponent::StaticClass());
			if (!sceneAnchorComponent)
			{
				continue;
			}

			if (Cast<UOculusXRSceneAnchorComponent>(sceneAnchorComponent)->SemanticClassifications.Contains(SemanticLabel))
			{
				sceneComponent->SetVisibility(bIsVisible);
			}
		}
	}
}

TArray<AActor*> AOculusXRSceneActor::GetActorsBySemanticLabel(const FString SemanticLabel)
{
	TArray<AActor*> actors;

	for (auto& roomIterator : RoomSceneComponents)
	{
		TArray<USceneComponent*> childrenComponents = roomIterator.Value->GetAttachChildren();
		for (USceneComponent* sceneComponent : childrenComponents)
		{
			AActor* outerActor = Cast<AActor>(sceneComponent->GetOuter());
			if (!outerActor)
			{
				continue;
			}

			UActorComponent* sceneAnchorComponent = outerActor->GetComponentByClass(UOculusXRSceneAnchorComponent::StaticClass());
			if (!sceneAnchorComponent)
			{
				continue;
			}

			if (Cast<UOculusXRSceneAnchorComponent>(sceneAnchorComponent)->SemanticClassifications.Contains(SemanticLabel))
			{
				actors.Add(outerActor);
			}
		}
	}

	return actors;
}

EOculusXRAnchorResult::Type AOculusXRSceneActor::QueryAllRooms()
{
	FOculusXRSpaceQueryInfo queryInfo;
	queryInfo.MaxQuerySpaces = MaxQueries;
	queryInfo.FilterType = EOculusXRSpaceQueryFilterType::FilterByComponentType;
	queryInfo.ComponentFilter.Add(EOculusXRSpaceComponentType::RoomLayout);

	EOculusXRAnchorResult::Type anchorQueryResult;
	OculusXRAnchors::FOculusXRAnchors::QueryAnchorsAdvanced(queryInfo, 
		FOculusXRAnchorQueryDelegate::CreateUObject(this, &AOculusXRSceneActor::RoomLayoutQueryComplete), anchorQueryResult);

	return anchorQueryResult;
}

void AOculusXRSceneActor::RoomLayoutQueryComplete(EOculusXRAnchorResult::Type AnchorResult, const TArray<FOculusXRSpaceQueryResult>& QueryResults)
{
	UE_LOG(LogOculusXRScene, Verbose, TEXT("RoomLayoutQueryComplete (Result = %d"), AnchorResult);

	for (auto& QueryElement : QueryResults)
	{
		UE_LOG(LogOculusXRScene, Verbose, TEXT("RoomLayoutQueryComplete -- Query Element (space = %llu, uuid = %s"), QueryElement.Space.Value, *BytesToHex(QueryElement.UUID.UUIDBytes, OCULUSXR_UUID_SIZE));

		FOculusXRRoomLayout roomLayout;
		const bool bGetRoomLayoutResult = RoomLayoutManagerComponent->GetRoomLayout(QueryElement.Space.Value, roomLayout, MaxQueries);
		if (!bGetRoomLayoutResult)
		{
			UE_LOG(LogOculusXRScene, Error, TEXT("RoomLayoutQueryComplete -- Failed to get room layout for space (space = %llu, uuid = %s"), 
				QueryElement.Space.Value, *BytesToHex(QueryElement.UUID.UUIDBytes, OCULUSXR_UUID_SIZE));
			continue;
		}
		
		EOculusXRAnchorResult::Type queryResult = QueryRoomUUIDs(QueryElement.Space.Value, roomLayout.RoomObjectUUIDs);
		if (UOculusXRAnchorBPFunctionLibrary::IsAnchorResultSuccess(queryResult))
		{
			RoomLayouts.Add(QueryElement.Space.Value, std::move(roomLayout));
		}

	}
}

EOculusXRAnchorResult::Type AOculusXRSceneActor::QueryRoomUUIDs(const FOculusXRUInt64 RoomSpaceID, const TArray<FOculusXRUUID>& RoomUUIDs)
{
	EOculusXRAnchorResult::Type anchorQueryResult;
	OculusXRAnchors::FOculusXRAnchors::QueryAnchors(
		RoomUUIDs, 
		EOculusXRSpaceStorageLocation::Local, 
		FOculusXRAnchorQueryDelegate::CreateUObject(this, &AOculusXRSceneActor::SceneRoomQueryComplete, RoomSpaceID), 
		anchorQueryResult);
	
	return anchorQueryResult;
}

void AOculusXRSceneActor::SceneRoomQueryComplete(EOculusXRAnchorResult::Type AnchorResult, const TArray<FOculusXRSpaceQueryResult>& QueryResults, const FOculusXRUInt64 RoomSpaceID)
{
	if (!UOculusXRAnchorBPFunctionLibrary::IsAnchorResultSuccess(AnchorResult))
	{
		return;
	}

	bool bOutPending = false;

	for (auto& AnchorQueryElement : QueryResults)
	{
		// Is it a ScenePlane anchor?
		bool bIsScenePlane = false;
		bool bIsSceneVolume = false;
		EOculusXRAnchorResult::Type isPlaneResult = OculusXRAnchors::FOculusXRAnchorManager::GetSpaceComponentStatus(
			AnchorQueryElement.Space.Value, EOculusXRSpaceComponentType::ScenePlane, bIsScenePlane, bOutPending);

		EOculusXRAnchorResult::Type isVolumeResult = OculusXRAnchors::FOculusXRAnchorManager::GetSpaceComponentStatus(
			AnchorQueryElement.Space.Value, EOculusXRSpaceComponentType::SceneVolume, bIsSceneVolume, bOutPending);

		bool bIsPlaneResultSuccess = UOculusXRAnchorBPFunctionLibrary::IsAnchorResultSuccess(isPlaneResult);
		bool bIsVolumeResultSuccess = UOculusXRAnchorBPFunctionLibrary::IsAnchorResultSuccess(isVolumeResult);

		if (bIsPlaneResultSuccess && bIsScenePlane)
		{
			EOculusXRAnchorResult::Type Result;
			FVector scenePlanePos;
			FVector scenePlaneSize;
			bool ResultSuccess = OculusXRAnchors::FOculusXRAnchors::GetSpaceScenePlane(AnchorQueryElement.Space.Value, scenePlanePos, scenePlaneSize, Result);
			if (!ResultSuccess)
			{
				UE_LOG(LogOculusXRScene, Error, TEXT("SpatialAnchorQueryResult_Handler Failed to get bounds for ScenePlane space."));
				continue;
			}

			UE_LOG(LogOculusXRScene, Verbose, TEXT("SpatialAnchorQueryResult_Handler ScenePlane pos = [%.2f, %.2f, %.2f], size = [%.2f, %.2f, %.2f]."),
				scenePlanePos.X, scenePlanePos.Y, scenePlanePos.Z,
				scenePlaneSize.X, scenePlaneSize.Y, scenePlaneSize.Z);

			TArray<FString> semanticClassifications;
			GetSemanticClassifications(AnchorQueryElement.Space.Value, semanticClassifications);

			if (!SpawnSceneAnchor(AnchorQueryElement.Space, RoomSpaceID, scenePlaneSize, semanticClassifications, EOculusXRSpaceComponentType::ScenePlane))
			{
				UE_LOG(LogOculusXRScene, Error, TEXT("SpatialAnchorQueryResult_Handler Failed to spawn scene anchor."));
			}
		}
		
		if (bIsVolumeResultSuccess && bIsSceneVolume)
		{
			EOculusXRAnchorResult::Type Result;
			FVector sceneVolumePos;
			FVector sceneVolumeSize;
			bool ResultSuccess = OculusXRAnchors::FOculusXRAnchors::GetSpaceSceneVolume(AnchorQueryElement.Space.Value, sceneVolumePos, sceneVolumeSize, Result);
			if (!ResultSuccess)
			{
				UE_LOG(LogOculusXRScene, Error, TEXT("SpatialAnchorQueryResult_Handler Failed to get bounds for SceneVolume space."));
				continue;
			}

			UE_LOG(LogOculusXRScene, Display, TEXT("SpatialAnchorQueryResult_Handler SceneVolume pos = [%.2f, %.2f, %.2f], size = [%.2f, %.2f, %.2f]."),
				sceneVolumePos.X, sceneVolumePos.Y, sceneVolumePos.Z,
				sceneVolumeSize.X, sceneVolumeSize.Y, sceneVolumeSize.Z);

			TArray<FString> semanticClassifications;
			GetSemanticClassifications(AnchorQueryElement.Space.Value, semanticClassifications);

			if (!SpawnSceneAnchor(AnchorQueryElement.Space, RoomSpaceID, sceneVolumeSize, semanticClassifications, EOculusXRSpaceComponentType::SceneVolume))
			{
				UE_LOG(LogOculusXRScene, Error, TEXT("SpatialAnchorQueryResult_Handler Failed to spawn scene anchor."));
			}
		}
	}
}

void AOculusXRSceneActor::GetSemanticClassifications(uint64 Space, TArray<FString>& OutSemanticLabels) const
{
	EOculusXRAnchorResult::Type SemanticLabelAnchorResult;
	bool Result = OculusXRAnchors::FOculusXRAnchors::GetSpaceSemanticClassification(Space, OutSemanticLabels, SemanticLabelAnchorResult);
	if (Result)
	{
		UE_LOG(LogOculusXRScene, Verbose, TEXT("GetSemanticClassifications -- Space (%llu) Classifications:"), Space);
		for (FString& label : OutSemanticLabels)
		{
			UE_LOG(LogOculusXRScene, Verbose, TEXT("%s"), *label);
		}
	}
	else
	{
		UE_LOG(LogOculusXRScene, Error, TEXT("SpatialAnchorQueryResult_Handler Failed to get semantic classification space."));
	}
}

// DELEGATE HANDLERS
void AOculusXRSceneActor::SceneCaptureComplete_Handler(FOculusXRUInt64 RequestId, bool bResult)
{	
	if (!bResult)
	{
		UE_LOG(LogOculusXRScene, Error, TEXT("Scene Capture Complete failed!"));
		return;
	}
	
	// Mark that we already launched Capture Flow and try to query spatial anchors again
	bCaptureFlowWasLaunched = true;

	ClearScene();
	PopulateScene();
}

#undef LOCTEXT_NAMESPACE
