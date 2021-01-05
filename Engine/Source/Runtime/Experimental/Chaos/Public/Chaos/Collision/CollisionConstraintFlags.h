// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/Framework/MultiBufferResource.h"
#include "Chaos/GeometryParticlesfwd.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Chaos/PBDRigidsEvolutionFwd.h"

namespace Chaos
{
	enum class ECollisionConstraintFlags : uint32
	{
		CCF_None                       = 0x0,
		CCF_BroadPhaseIgnoreCollisions = 0x1,
		CCF_DummyFlag
	};

	class CHAOS_API FIgnoreCollisionManager
	{
	public:
		using FHandleID = FUniqueIdx;
		using FDeactivationArray = TArray<FUniqueIdx>;
		using FActiveMap = TMap<FHandleID, TArray<FHandleID> >;
		using FPendingMap = TMap<FHandleID, TArray<FHandleID> >;
		struct FStorageData
		{
			FPendingMap PendingActivations;
			FDeactivationArray PendingDeactivations;
		};

		FIgnoreCollisionManager()
		{
			BufferedData = FMultiBufferFactory<FStorageData>::CreateBuffer(EMultiBufferMode::Double);
		}

		bool ContainsHandle(FHandleID Body0);

		bool IgnoresCollision(FHandleID Body0, FHandleID Body1);

		int32 NumIgnoredCollision(FHandleID Body0);

		void AddIgnoreCollisionsFor(FHandleID Body0, FHandleID Body1);

		void RemoveIgnoreCollisionsFor(FHandleID Body0, FHandleID Body1);

		const FPendingMap& GetPendingActivationsForGameThread() const { return BufferedData->AccessProducerBuffer()->PendingActivations; }
		FPendingMap& GetPendingActivationsForGameThread() { return BufferedData->AccessProducerBuffer()->PendingActivations; }

		const FDeactivationArray& GetPendingDeactivationsForGameThread() const { return BufferedData->AccessProducerBuffer()->PendingDeactivations; }
		FDeactivationArray& GetPendingDeactivationsForGameThread() { return BufferedData->AccessProducerBuffer()->PendingDeactivations; }

		/*
		*
		*/
		void ProcessPendingQueues();

		/*
		*
		*/
		void FlipBufferPreSolve();

	private:
		FActiveMap IgnoreCollisionsList;

		FPendingMap PendingActivations;
		FDeactivationArray PendingDeactivations;
		TUniquePtr<Chaos::IBufferResource<FStorageData>> BufferedData;
	};

} // Chaos
