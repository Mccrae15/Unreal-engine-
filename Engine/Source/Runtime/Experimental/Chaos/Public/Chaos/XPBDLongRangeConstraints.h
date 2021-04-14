// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDLongRangeConstraintsBase.h"
#include "Chaos/PBDParticles.h"
#include "ChaosStats.h"

DECLARE_CYCLE_STAT(TEXT("Chaos XPBD Long Range Constraint"), STAT_XPBD_LongRange, STATGROUP_Chaos);

namespace Chaos
{
// Stiffness is in N/CM^2, so it needs to be adjusted from the PBD stiffness ranging between [0,1]
static const double XPBDLongRangeMaxCompliance = 1.e-3;

template<class T, int d>
class TXPBDLongRangeConstraints final : public TPBDLongRangeConstraintsBase<T, d>
{
public:
	typedef TPBDLongRangeConstraintsBase<T, d> Base;
	typedef typename Base::FTether FTether;
	typedef typename Base::EMode EMode;

	TXPBDLongRangeConstraints(
		const TPBDParticles<T, d>& Particles,
		const int32 InParticleOffset,
		const int32 InParticleCount,
		const TMap<int32, TSet<int32>>& PointToNeighbors,
		const TConstArrayView<T>& StiffnessMultipliers,
		const int32 NumberOfAttachments = 4,
		const TVector<T, 2>& InStiffness = TVector<T, 2>((T)1., (T)1.),
		const T LimitScale = (T)1,
		const EMode InMode = EMode::Geodesic)
	    : TPBDLongRangeConstraintsBase<T, d>(Particles, InParticleOffset, InParticleCount, PointToNeighbors, StiffnessMultipliers, NumberOfAttachments, InStiffness, LimitScale, InMode)
	{
		Lambdas.Reserve(Tethers.Num());
	}

	~TXPBDLongRangeConstraints() {}

	void Init() const
	{
		Lambdas.Reset();
		Lambdas.AddZeroed(Tethers.Num());
	}

	void Apply(TPBDParticles<T, d>& Particles, const T Dt) const 
	{
		SCOPE_CYCLE_COUNTER(STAT_XPBD_LongRange);
		// Run particles in parallel, and ranges in sequence to avoid a race condition when updating the same particle from different tethers
		static const int32 MinParallelSize = 500;
		if (Stiffness.HasWeightMap())
		{
			TethersView.ParallelFor([this, &Particles, Dt](TArray<FTether>& /*InTethers*/, int32 Index)
				{
					const T ExpStiffnessValue = Stiffness[Index - ParticleOffset];
					Apply(Particles, Dt, Index, ExpStiffnessValue);
				}, MinParallelSize);
		}
		else
		{
			const T ExpStiffnessValue = (T)Stiffness;
			TethersView.ParallelFor([this, &Particles, Dt, ExpStiffnessValue](TArray<FTether>& /*InTethers*/, int32 Index)
				{
					Apply(Particles, Dt, Index, ExpStiffnessValue);
				}, MinParallelSize);
		}
	}

	void Apply(TPBDParticles<T, d>& Particles, const T Dt, const TArray<int32>& ConstraintIndices) const
	{
		SCOPE_CYCLE_COUNTER(STAT_XPBD_LongRange);
		if (Stiffness.HasWeightMap())
		{
			for (const int32 Index : ConstraintIndices)
			{
				const T ExpStiffnessValue = Stiffness[Index - ParticleOffset];
				Apply(Particles, Dt, Index, ExpStiffnessValue);
			}
		}
		else
		{
			const T ExpStiffnessValue = (T)Stiffness;
			for (const int32 Index : ConstraintIndices)
			{
				Apply(Particles, Dt, Index, ExpStiffnessValue);
			}
		}
	}

private:
	void Apply(TPBDParticles<T, d>& Particles, const T Dt, int32 Index, const T InStiffness) const
	{
		const FTether& Tether = Tethers[Index];

		TVector<T, d> Direction;
		T Offset;
		Tether.GetDelta(Particles, Direction, Offset);

		T& Lambda = Lambdas[Index];
		const T Alpha = (T)XPBDLongRangeMaxCompliance / (InStiffness * Dt * Dt);

		const T DLambda = (Offset - Alpha * Lambda) / ((T)1. + Alpha);
		Particles.P(Tether.End) += DLambda * Direction;
		Lambda += DLambda;
	}

private:
	using Base::Tethers;
	using Base::TethersView;
	using Base::Stiffness;
	using Base::ParticleOffset;

	mutable TArray<T> Lambdas;
};
}
