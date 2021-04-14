// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDParticles.h"
#include "Chaos/PBDActiveView.h"
#include "Chaos/PBDStiffness.h"
#include "Containers/Map.h"
#include "Containers/Set.h"

namespace Chaos
{
template<class T, int d>
class CHAOS_API TPBDLongRangeConstraintsBase
{
public:
	enum class EMode : uint8
	{
		Euclidean,
		Geodesic,

		// Deprecated modes
		FastTetherFastLength = Euclidean,
		AccurateTetherFastLength = Geodesic,
		AccurateTetherAccurateLength = Geodesic
	};

	struct FTether
	{
		int32 Start;
		int32 End;
		T RefLength;

		FTether(int32 InStart, int32 InEnd, T InRefLength): Start(InStart), End(InEnd), RefLength(InRefLength) {}

		inline TVector<T, d> GetDelta(const TPBDParticles<T, d>& Particles) const
		{
			checkSlow(Particles.InvM(Start) == (T)0.);
			checkSlow(Particles.InvM(End) > (T)0.);
			TVector<T, d> Direction = Particles.P(Start) - Particles.P(End);
			const T Length = Direction.SafeNormalize();
			const T Offset = Length - RefLength;
			return Offset < (T)0. ? TVector<T, d>((T)0.) : Offset * Direction;
		};

		inline void GetDelta(const TPBDParticles<T, d>& Particles, TVector<T, d>& OutDirection, T& OutOffset) const
		{
			checkSlow(Particles.InvM(Start) == (T)0.);
			checkSlow(Particles.InvM(End) > (T)0.);
			OutDirection = Particles.P(Start) - Particles.P(End);
			const T Length = OutDirection.SafeNormalize();
			OutOffset = FMath::Max((T)0., Length - RefLength);
		};
	};

	TPBDLongRangeConstraintsBase(
		const TPBDParticles<T, d>& Particles,
		const int32 InParticleOffset,
		const int32 InParticleCount,
		const TMap<int32, TSet<int32>>& PointToNeighbors,
		const TConstArrayView<T>& StiffnessMultipliers,
		const int32 MaxNumTetherIslands = 4,
		const TVector<T, 2>& InStiffness = TVector<T, 2>((T)0., (T)1.),
		const T LimitScale = (T)1,
		const EMode InMode = EMode::AccurateTetherFastLength);

	virtual ~TPBDLongRangeConstraintsBase() {}

	EMode GetMode() const { return Mode; }

	// Return the stiffness input values used by the constraint
	TVector<T, 2> GetStiffness() const { return Stiffness.GetWeightedValue(); }

	// Set the stiffness input values used by the constraint
	void SetStiffness(const TVector<T, 2>& InStiffness) { Stiffness.SetWeightedValue(InStiffness); }

	// Set stiffness offset and range, as well as the simulation stiffness exponent
	void ApplyProperties(const T Dt, const int32 NumIterations) { Stiffness.ApplyValues(Dt, NumIterations); }

	const TArray<FTether>& GetTethers() const { return Tethers; }

	static TArray<TArray<int32>> ComputeIslands(const TMap<int32, TSet<int32>>& PointToNeighbors, const TArray<int32>& KinematicParticles);

protected:
	void ComputeEuclideanConstraints(const TPBDParticles<T, d>& Particles, const TMap<int32, TSet<int32>>& PointToNeighbors, const int32 NumberOfAttachments);
	void ComputeGeodesicConstraints(const TPBDParticles<T, d>& Particles, const TMap<int32, TSet<int32>>& PointToNeighbors, const int32 NumberOfAttachments);

protected:
	TArray<FTether> Tethers;
	TPBDActiveView<TArray<FTether>> TethersView;
	TPBDStiffness<T, d> Stiffness;
	const EMode Mode;
	const int32 ParticleOffset;
	const int32 ParticleCount;
};
}
