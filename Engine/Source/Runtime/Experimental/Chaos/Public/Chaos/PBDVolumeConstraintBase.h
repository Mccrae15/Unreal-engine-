// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Array.h"
#include "Chaos/PBDParticles.h"
#include "Chaos/ParticleRule.h"

namespace Chaos
{
template<class T>
class TPBDVolumeConstraintBase
{
public:
	TPBDVolumeConstraintBase(const TDynamicParticles<T, 3>& InParticles, TArray<TVector<int32, 3>>&& InConstraints, const T InStiffness = (T)1.)
	    : Constraints(InConstraints), Stiffness(InStiffness)
	{
		TVector<T, 3> Com = TVector<T, 3>(0, 0, 0);
		for (uint32 i = 0; i < InParticles.Size(); ++i)
		{
			Com += InParticles.X(i);
		}
		Com /= InParticles.Size();
		RefVolume = 0;
		for (const TVector<int32, 3>& Constraint : Constraints)
		{
			const TVector<T, 3>& P1 = InParticles.X(Constraint[0]);
			const TVector<T, 3>& P2 = InParticles.X(Constraint[1]);
			const TVector<T, 3>& P3 = InParticles.X(Constraint[2]);
			RefVolume += GetVolume(P1, P2, P3, Com);
		}
		RefVolume /= (T)9.;
	}
	virtual ~TPBDVolumeConstraintBase() {}

	TArray<T> GetWeights(const TPBDParticles<T, 3>& InParticles, const T Alpha) const
	{
		TArray<T> W;
		W.SetNum(InParticles.Size());
		T oneminusAlpha = 1 - Alpha;
		T Wg = (T)1 / (T)InParticles.Size();
		T WlDenom = 0;
		for (uint32 i = 0; i < InParticles.Size(); ++i)
		{
			WlDenom += (InParticles.P(i) - InParticles.X(i)).Size();
		}
		for (uint32 i = 0; i < InParticles.Size(); ++i)
		{
			T Wl = (InParticles.P(i) - InParticles.X(i)).Size() / WlDenom;
			W[i] = oneminusAlpha * Wl + Alpha * Wg;
		}
		return W;
	}

	TArray<TVector<T, 3>> GetGradients(const TPBDParticles<T, 3>& InParticles) const
	{
		TVector<T, 3> Com = TVector<T, 3>(0, 0, 0);
		for (uint32 i = 0; i < InParticles.Size(); ++i)
		{
			Com += InParticles.P(i);
		}
		Com /= InParticles.Size();
		TArray<TVector<T, 3>> Grads;
		Grads.SetNum(InParticles.Size());
		for (TVector<T, 3>& Elem : Grads)
		{
			Elem = TVector<T, 3>(0, 0, 0);
		}
		for (int32 i = 0; i < Constraints.Num(); ++i)
		{
			const TVector<int32, 3>& Constraint = Constraints[i];
			const int32 i1 = Constraint[0];
			const int32 i2 = Constraint[1];
			const int32 i3 = Constraint[2];
			const TVector<T, 3>& P1 = InParticles.P(i1);
			const TVector<T, 3>& P2 = InParticles.P(i2);
			const TVector<T, 3>& P3 = InParticles.P(i3);
			const T Area = GetArea(P1, P2, P3);
			const TVector<T, 3> Normal = GetNormal(P1, P2, P3, Com);
			Grads[i1] += Area * Normal;
			Grads[i2] += Area * Normal;
			Grads[i3] += Area * Normal;
		}
		for (TVector<T, 3>& Elem : Grads)
		{
			Elem *= (T)1 / (T)3;
		}
		return Grads;
	}

	T GetScalingFactor(const TPBDParticles<T, 3>& InParticles, const TArray<TVector<T, 3>>& Grads, const TArray<T>& W) const
	{
		TVector<T, 3> Com = TVector<T, 3>(0, 0, 0);
		for (uint32 i = 0; i < InParticles.Size(); ++i)
		{
			Com += InParticles.P(i);
		}
		Com /= InParticles.Size();
		T Volume = 0;
		for (const TVector<int32, 3>& Constraint : Constraints)
		{
			const TVector<T, 3>& P1 = InParticles.P(Constraint[0]);
			const TVector<T, 3>& P2 = InParticles.P(Constraint[1]);
			const TVector<T, 3>& P3 = InParticles.P(Constraint[2]);
			Volume += GetVolume(P1, P2, P3, Com);
		}
		Volume /= (T)9;
		T Denom = 0;
		for (uint32 i = 0; i < InParticles.Size(); ++i)
		{
			Denom += W[i] * Grads[i].SizeSquared();
		}
		T S = (Volume - RefVolume) / Denom;
		return Stiffness * S;
	}

	void SetStiffness(T InStiffness) { Stiffness = FMath::Clamp(InStiffness, (T)0., (T)1.); }

protected:
	TArray<TVector<int32, 3>> Constraints;

private:
	// Utility functions for the triangle concept
	TVector<T, 3> GetNormal(const TVector<T, 3> P1, const TVector<T, 3>& P2, const TVector<T, 3>& P3, const TVector<T, 3>& Com) const
	{
		const TVector<T, 3> Normal = TVector<T, 3>::CrossProduct(P2 - P1, P3 - P1).GetSafeNormal();
		if (TVector<T, 3>::DotProduct((P1 + P2 + P3) / (T)3. - Com, Normal) < 0)
			return -Normal;
		return Normal;
	}

	T GetArea(const TVector<T, 3>& P1, const TVector<T, 3>& P2, const TVector<T, 3>& P3) const
	{
		TVector<T, 3> B = (P2 - P1).GetSafeNormal();
		TVector<T, 3> H = TVector<T, 3>::DotProduct(B, P3 - P1) * B + P1;
		return (T)0.5 * (P2 - P1).Size() * (P3 - H).Size();
	}

	T GetVolume(const TVector<T, 3>& P1, const TVector<T, 3>& P2, const TVector<T, 3>& P3, const TVector<T, 3>& Com) const
	{
		return GetArea(P1, P2, P3) * TVector<T, 3>::DotProduct(P1 + P2 + P3, GetNormal(P1, P2, P3, Com));
	}

	T RefVolume;
	T Stiffness;
};
}
