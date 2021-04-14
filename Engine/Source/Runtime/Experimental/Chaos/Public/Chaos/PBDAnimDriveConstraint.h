// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Framework/Parallel.h"
#include "Chaos/PBDStiffness.h"
#include "ChaosStats.h"

DECLARE_CYCLE_STAT(TEXT("Chaos PBD Anim Drive Constraint"), STAT_PBD_AnimDriveConstraint, STATGROUP_Chaos);

namespace Chaos
{
	template<typename T, int d>
	class TPBDAnimDriveConstraint final
	{
	public:
		TPBDAnimDriveConstraint(
			const int32 InParticleOffset,
			const int32 InParticleCount,
			const TArray<TVector<T, d>>& InAnimationPositions,  // Use global indexation (will need adding ParticleOffset)
			const TArray<TVector<T, d>>& InOldAnimationPositions,  // Use global indexation (will need adding ParticleOffset)
			const TConstArrayView<T>& StiffnessMultipliers,  // Use local indexation
			const TConstArrayView<T>& DampingMultipliers  // Use local indexation
		)
			: AnimationPositions(InAnimationPositions)
			, OldAnimationPositions(InOldAnimationPositions)
			, ParticleOffset(InParticleOffset)
			, ParticleCount(InParticleCount)
			, Stiffness(StiffnessMultipliers, TVector<T, 2>((T)0., (T)1.), InParticleCount)
			, Damping(DampingMultipliers, TVector<T, 2>((T)0., (T)1.), InParticleCount)
		{
		}

		~TPBDAnimDriveConstraint() {}

		// Return the stiffness input values used by the constraint
		inline TVector<T, 2> GetStiffness() const { return Stiffness.GetWeightedValue(); }

		// Return the damping input values used by the constraint
		inline TVector<T, 2> GetDamping() const { return Damping.GetWeightedValue();; }

		inline void SetProperties(const TVector<T, 2>& InStiffness, const TVector<T, 2>& InDamping)
		{
			Stiffness.SetWeightedValue(InStiffness);
			Damping.SetWeightedValue(InDamping);
		}

		// Set stiffness offset and range, as well as the simulation stiffness exponent
		inline void ApplyProperties(const T Dt, const int32 NumIterations)
		{
			Stiffness.ApplyValues(Dt, NumIterations);
			Damping.ApplyValues(Dt, NumIterations);
		}

		inline void Apply(TPBDParticles<T, d>& InParticles, const T Dt) const
		{
			SCOPE_CYCLE_COUNTER(STAT_PBD_AnimDriveConstraint);

			if (Stiffness.HasWeightMap())
			{
				if (Damping.HasWeightMap())
				{
					PhysicsParallelFor(ParticleCount, [&](int32 Index)  // TODO: profile needed for these parallel loop based on particle count
					{
						const T ParticleStiffness = Stiffness[Index];
						const T ParticleDamping = Damping[Index];
						ApplyHelper(InParticles, ParticleStiffness, ParticleDamping, Dt, Index);
					});
				}
				else
				{
					const T ParticleDamping = (T)Damping;
					PhysicsParallelFor(ParticleCount, [&](int32 Index)
					{
						const T ParticleStiffness = Stiffness[Index];
						ApplyHelper(InParticles, ParticleStiffness, ParticleDamping, Dt, Index);
					});
				}
			}
			else
			{
				const T ParticleStiffness = (T)Stiffness;
				if (Damping.HasWeightMap())
				{
					PhysicsParallelFor(ParticleCount, [&](int32 Index)
					{
						const T ParticleDamping = Damping[Index];
						ApplyHelper(InParticles, ParticleStiffness, ParticleDamping, Dt, Index);
					});
				}
				else
				{
					const T ParticleDamping = (T)Damping;
					PhysicsParallelFor(ParticleCount, [&](int32 Index)
					{
						ApplyHelper(InParticles, ParticleStiffness, ParticleDamping, Dt, Index);
					});
				}
			}
		}

	private:
		inline void ApplyHelper(TPBDParticles<T, d>& Particles, const T InStiffness, const T InDamping, const T Dt, const int32 Index) const
		{
			const int32 ParticleIndex = ParticleOffset + Index;
			if (Particles.InvM(ParticleIndex) == (T)0.)
			{
				return;
			}

			TVector<T, d>& ParticlePosition = Particles.P(ParticleIndex);
			const TVector<T, d>& AnimationPosition = AnimationPositions[ParticleIndex];
			const TVector<T, d>& OldAnimationPosition = OldAnimationPositions[ParticleIndex];

			const TVector<T, d> ParticleDisplacement = ParticlePosition - Particles.X(ParticleIndex);
			const TVector<T, d> AnimationDisplacement = OldAnimationPosition - AnimationPosition;
			const TVector<T, d> RelativeDisplacement = ParticleDisplacement - AnimationDisplacement;

			ParticlePosition -= InStiffness * (ParticlePosition - AnimationPosition) + InDamping * RelativeDisplacement;
		}

	private:
		const TArray<TVector<T, d>>& AnimationPositions;  // Use global index (needs adding ParticleOffset)
		const TArray<TVector<T, d>>& OldAnimationPositions;  // Use global index (needs adding ParticleOffset)
		const int32 ParticleOffset;
		const int32 ParticleCount;

		TPBDStiffness<T, d> Stiffness;
		TPBDStiffness<T, d> Damping;
	};
}
