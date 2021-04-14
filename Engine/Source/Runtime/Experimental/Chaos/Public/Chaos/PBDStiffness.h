// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Framework/Parallel.h"
#include "Containers/ArrayView.h"
#include "Containers/ContainersFwd.h"
#include "ChaosStats.h"

DECLARE_CYCLE_STAT(TEXT("Chaos PBD Stiffness Apply Values"), STAT_PBD_StiffnessApplyValues, STATGROUP_Chaos);

namespace Chaos
{
	/**
	 * Stiffness class for managing real time update to the weight map and low/high value ranges
	 * and to exponentiate the stiffness value depending on the iterations and Dt.
	 */
	template<typename T, int d>
	class TPBDStiffness final
	{
	public:
		/**
		 * Weightmap constructor. 
		 */
		TPBDStiffness(
			const TConstArrayView<T>& Multipliers,
			const TVector<T, 2>& InWeightedValue,
			int32 ParticleCount,
			int32 TableSize = 16,  // Size of the lookup table, can't be more than 256 values, the larger the table the longer it takes to apply changes to the stiffness values
			T InParameterFitBase = (T)1.e3);  // Logarithm base to use in the stiffness parameter fit function

		~TPBDStiffness() {}

		/** Return the number of values stored in the weight map. */
		int32 Num() const { return Indices.Num(); }

		/** Return whether this object contains weight map values. */
		bool HasWeightMap() const { return Table.Num() > 1; }

		/**
		 * Set the low and high values of the weight map.
		 * The weight map table only gets updated after ApplyValues is called.
		 */
		void SetWeightedValue(const TVector<T, 2>& InWeightedValue) { WeightedValue = InWeightedValue.ClampAxes((T)0., (T)1.); }

		/**
		  * Return the low and high values set for this weight map.
		  * Both values will always be between 0 and 1 due to having been clamped in SetWeightedValue.
		  */
		const TVector<T, 2>& GetWeightedValue() const { return WeightedValue; }

		/** Update the weight map table with the current simulation parameters. */
		inline void ApplyValues(const T Dt, const int32 NumIterations);

		/**
		 * Lookup for the exponential weighted value at the specified weight map index.
		 * This function will assert if it is called with a non zero index on an empty weight map.
		*/
		T operator[](int32 Index) const { return Table[Indices[Index]]; }

		/** Return the exponential value at the Low weight. */
		T GetLow() const { return Table[0]; }

		/** Return the exponential value at the High weight. */
		T GetHigh() const { return Table.Last(); }

		/** Return the exponential stiffness value when the weight map is not used. */
		explicit operator T() const { return GetLow(); }

	private:
		static constexpr T ParameterFrequency = (T)120.;  // 60Hz @ 2 iterations as a root for all stiffness values TODO: Make this a global solver parameter

		TArray<uint8> Indices; // Per particle array of index to the stiffness table
		TArray<T> Table;  // Fixed lookup table of stiffness values, use uint8 indexation
		TVector<T, 2> WeightedValue;
		const T ParameterFitLogBase;
	};

	template<typename T, int d>
	TPBDStiffness<T, d>::TPBDStiffness(
		const TConstArrayView<T>& Multipliers,
		const TVector<T, 2>& InWeightedValue,
		int32 ParticleCount,
		int32 TableSize,
		T InParameterFitBase)
		: WeightedValue(InWeightedValue)
		, ParameterFitLogBase(FMath::Loge(InParameterFitBase))
	{
		check(TableSize > 0 && TableSize < 256);  // The Stiffness lookup table is restricted to uint8 sized indices

		if (Multipliers.Num() == ParticleCount)
		{
			// Convert the weight maps into an array of lookup indices to the stiffness table
			Indices.AddUninitialized(ParticleCount);

			const T TableScale = (T)(TableSize - 1);

			for (int32 Index = 0; Index < ParticleCount; ++Index)
			{
				Indices[Index] = (uint8)(FMath::Clamp(Multipliers[Index], (T)0., (T)1.) * TableScale);
			}

			// Initialize empty table until ApplyValues is called
			Table.AddZeroed(TableSize);
		}
		else
		{
			// Initialize with a one element table until ApplyValues is called
			Indices.AddZeroed(1);
			Table.AddZeroed(1);
		}
	}

	template<typename T, int d>
	void TPBDStiffness<T, d>::ApplyValues(const T Dt, const int32 NumIterations)
	{
		SCOPE_CYCLE_COUNTER(STAT_PBD_StiffnessApplyValues);

		// Calculate the simulation exponent
		const T Exponent = Dt * ParameterFrequency / (T)NumIterations;

		// Define the stiffness mapping function
		auto SimulationValue = [this, Exponent](const T InValue)->T
		{
			// Get a very steep exponential curve between the [0, 1] range to make easier to set the parameter
			// The base has been chosen empirically
			// ParameterValue = Pow(ParameterFitBase, ParameterValue - 1)
			const T ParameterFit = FMath::Exp(ParameterFitLogBase * (FMath::Clamp(InValue, (T)0., (T)1.) - (T)1.));

			// Use simulation dependent stiffness exponent to alleviate the variations in effect when Dt and NumIterations change
			// This is based on the Position-Based Simulation Methods paper (page 8),
			// but uses the delta time in addition of the number of iterations in the calculation of the error term.
			const T LogValue = FMath::Loge((T)1. - ParameterFit);
			return (T)1. - FMath::Exp(LogValue * Exponent);
		};

		const T Offset = WeightedValue[0];
		const T Range = WeightedValue[1] - WeightedValue[0];
		const int32 TableSize = Table.Num();
		const T WeightIncrement = (TableSize > 1) ? (T)1. / (T)(TableSize - 1) : (T)1.; // Must allow full range from 0 to 1 included
		for (int32 Index = 0; Index < TableSize; ++Index)
		{
			const T Weight = (T)Index * WeightIncrement;
			Table[Index] = SimulationValue(Offset + Weight * Range);
		}
	}

}
