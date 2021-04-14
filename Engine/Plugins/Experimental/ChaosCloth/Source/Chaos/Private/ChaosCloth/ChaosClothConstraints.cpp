// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChaosCloth/ChaosClothConstraints.h"
#include "Chaos/PBDSpringConstraints.h"
#include "Chaos/XPBDSpringConstraints.h"
#include "Chaos/PBDBendingConstraints.h"
#include "Chaos/PBDAxialSpringConstraints.h"
#include "Chaos/XPBDAxialSpringConstraints.h"
#include "Chaos/PBDVolumeConstraint.h"
#include "Chaos/XPBDLongRangeConstraints.h"
#include "Chaos/PBDSphericalConstraint.h"
#include "Chaos/PBDAnimDriveConstraint.h"
#include "Chaos/PBDShapeConstraints.h"
#include "Chaos/PBDCollisionSpringConstraints.h"
#include "Chaos/PBDEvolution.h"

using namespace Chaos;

FClothConstraints::FClothConstraints()
	: Evolution(nullptr)
	, AnimationPositions(nullptr)
	, OldAnimationPositions(nullptr)
	, AnimationNormals(nullptr)
	, ParticleOffset(0)
	, NumParticles(0)
	, ConstraintInitOffset(INDEX_NONE)
	, ConstraintRuleOffset(INDEX_NONE)
	, NumConstraintInits(0)
	, NumConstraintRules(0)
{
}

FClothConstraints::~FClothConstraints()
{
}

void FClothConstraints::Initialize(
	TPBDEvolution<float, 3>* InEvolution,
	const TArray<TVector<float, 3>>& InAnimationPositions,
	const TArray<TVector<float, 3>>& InOldAnimationPositions,
	const TArray<TVector<float, 3>>& InAnimationNormals,
	int32 InParticleOffset,
	int32 InNumParticles)
{
	Evolution = InEvolution;
	AnimationPositions = &InAnimationPositions;
	OldAnimationPositions = &InOldAnimationPositions;
	AnimationNormals = &InAnimationNormals;
	ParticleOffset = InParticleOffset;
	NumParticles = InNumParticles;
}

void FClothConstraints::Enable(bool bEnable)
{
	check(Evolution);
	if (ConstraintInitOffset != INDEX_NONE)
	{
		Evolution->ActivateConstraintInitRange(ConstraintInitOffset, bEnable);
	}
	if (ConstraintRuleOffset != INDEX_NONE)
	{
		Evolution->ActivateConstraintRuleRange(ConstraintRuleOffset, bEnable);
	}
}

void FClothConstraints::CreateRules()
{
	check(Evolution);
	check(ConstraintInitOffset == INDEX_NONE)
	if (NumConstraintInits)
	{
		ConstraintInitOffset = Evolution->AddConstraintInitRange(NumConstraintInits, false);
	}
	check(ConstraintRuleOffset == INDEX_NONE)
	if (NumConstraintRules)
	{
		ConstraintRuleOffset = Evolution->AddConstraintRuleRange(NumConstraintRules, false);
	}

	TFunction<void(const TPBDParticles<float, 3>&, const float)>* const ConstraintInits = Evolution->ConstraintInits().GetData() + ConstraintInitOffset;
	TFunction<void(TPBDParticles<float, 3>&, const float)>* const ConstraintRules = Evolution->ConstraintRules().GetData() + ConstraintRuleOffset;

	int32 ConstraintInitIndex = 0;
	int32 ConstraintRuleIndex = 0;

	if (XEdgeConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](const TPBDParticles<float, 3>& /*Particles*/, const float /*Dt*/)
			{
				XEdgeConstraints->Init();
			};

		ConstraintRules[ConstraintRuleIndex++] = 
			[this](TPBDParticles<float, 3>& Particles, const float Dt)
			{
				XEdgeConstraints->Apply(Particles, Dt);
			};
	}
	if (EdgeConstraints)
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](TPBDParticles<float, 3>& Particles, const float Dt)
			{
				EdgeConstraints->Apply(Particles, Dt);
			};
	}
	if (XBendingConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](const TPBDParticles<float, 3>& /*Particles*/, const float /*Dt*/)
			{
				XBendingConstraints->Init();
			};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](TPBDParticles<float, 3>& Particles, const float Dt)
			{
				XBendingConstraints->Apply(Particles, Dt);
			};
	}
	if (BendingConstraints)
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](TPBDParticles<float, 3>& Particles, const float Dt)
			{
				BendingConstraints->Apply(Particles, Dt);
			};
	}
	if (BendingElementConstraints)
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](TPBDParticles<float, 3>& Particles, const float Dt)
			{
				BendingElementConstraints->Apply(Particles, Dt);
			};
	}
	if (XAreaConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](const TPBDParticles<float, 3>& /*Particles*/, const float /*Dt*/)
			{
				XAreaConstraints->Init();
			};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](TPBDParticles<float, 3>& Particles, const float Dt)
			{
				XAreaConstraints->Apply(Particles, Dt);
			};
	}
	if (AreaConstraints)
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](TPBDParticles<float, 3>& Particles, const float Dt)
			{
				AreaConstraints->Apply(Particles, Dt);
			};
	}
	if (ThinShellVolumeConstraints)
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](TPBDParticles<float, 3>& Particles, const float Dt)
			{
				ThinShellVolumeConstraints->Apply(Particles, Dt);
			};
	}
	if (VolumeConstraints)
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](TPBDParticles<float, 3>& Particles, const float Dt)
			{
				VolumeConstraints->Apply(Particles, Dt);
			};
	}
	if (XLongRangeConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](const TPBDParticles<float, 3>& /*Particles*/, const float Dt)
			{
				XLongRangeConstraints->Init();
				XLongRangeConstraints->ApplyProperties(Dt, Evolution->GetIterations());
			};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](TPBDParticles<float, 3>& Particles, const float Dt)
			{
				XLongRangeConstraints->Apply(Particles, Dt);
			};
	}
	if (LongRangeConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](const TPBDParticles<float, 3>& /*Particles*/, const float Dt)
			{
				LongRangeConstraints->ApplyProperties(Dt, Evolution->GetIterations());
			};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](TPBDParticles<float, 3>& Particles, const float Dt)
			{
				LongRangeConstraints->Apply(Particles, Dt);
			};
	}
	if (MaximumDistanceConstraints)
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](TPBDParticles<float, 3>& Particles, const float Dt)
			{
				MaximumDistanceConstraints->Apply(Particles, Dt);
			};
	}
	if (BackstopConstraints)
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](TPBDParticles<float, 3>& Particles, const float Dt)
			{
				BackstopConstraints->Apply(Particles, Dt);
			};
	}
	if (AnimDriveConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](const TPBDParticles<float, 3>& /*Particles*/, const float Dt)
			{
				AnimDriveConstraints->ApplyProperties(Dt, Evolution->GetIterations());
			};

		ConstraintRules[ConstraintRuleIndex++] =
			[this](TPBDParticles<float, 3>& Particles, const float Dt)
			{
				AnimDriveConstraints->Apply(Particles, Dt);
			};
	}
	if (ShapeConstraints)
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](TPBDParticles<float, 3>& Particles, const float Dt)
			{
				ShapeConstraints->Apply(Particles, Dt);
			};
	}
	if (SelfCollisionConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](const TPBDParticles<float, 3>& Particles, const float /*Dt*/)
			{
				SelfCollisionConstraints->Init(Particles);
			};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](TPBDParticles<float, 3>& Particles, const float Dt)
			{
				SelfCollisionConstraints->Apply(Particles, Dt);
			};
	}
	check(ConstraintInitIndex == NumConstraintInits);
	check(ConstraintRuleIndex == NumConstraintRules);
}

void FClothConstraints::SetEdgeConstraints(const TArray<TVector<int32, 3>>& SurfaceElements, float EdgeStiffness, bool bUseXPBDConstraints)
{
	check(Evolution);
	check(EdgeStiffness > 0.f && EdgeStiffness <= 1.f);

	if (bUseXPBDConstraints)
	{
		XEdgeConstraints = MakeShared<TXPBDSpringConstraints<float, 3>>(Evolution->Particles(), SurfaceElements, EdgeStiffness, /*bStripKinematicConstraints =*/ true);
		++NumConstraintInits;
	}
	else
	{
		EdgeConstraints = MakeShared<FPBDSpringConstraints>(Evolution->Particles(), SurfaceElements, EdgeStiffness, /*bStripKinematicConstraints =*/ true);
	}
	++NumConstraintRules;
}

void FClothConstraints::SetBendingConstraints(TArray<TVector<int32, 2>>&& Edges, float BendingStiffness, bool bUseXPBDConstraints)
{
	check(Evolution);

	if (bUseXPBDConstraints)
	{
		XBendingConstraints = MakeShared<TXPBDSpringConstraints<float, 3>>(Evolution->Particles(), MoveTemp(Edges), BendingStiffness, /*bStripKinematicConstraints =*/ true);
		++NumConstraintInits;
	}
	else
	{
		BendingConstraints = MakeShared<FPBDSpringConstraints>(Evolution->Particles(), MoveTemp(Edges), BendingStiffness, /*bStripKinematicConstraints =*/ true);
	}
	++NumConstraintRules;
}

void FClothConstraints::SetBendingConstraints(TArray<TVector<int32, 4>>&& BendingElements, float BendingStiffness)
{
	check(Evolution);
	check(BendingStiffness > 0.f && BendingStiffness <= 1.f);

	BendingElementConstraints = MakeShared<TPBDBendingConstraints<float>>(Evolution->Particles(), MoveTemp(BendingElements), BendingStiffness);  // TODO: Strip kinematic constraints
	++NumConstraintRules;
}

void FClothConstraints::SetAreaConstraints(TArray<TVector<int32, 3>>&& SurfaceElements, float AreaStiffness, bool bUseXPBDConstraints)
{
	check(Evolution);
	check(AreaStiffness > 0.f && AreaStiffness <= 1.f);

	if (bUseXPBDConstraints)
	{
		XAreaConstraints = MakeShared<TXPBDAxialSpringConstraints<float, 3>>(Evolution->Particles(), MoveTemp(SurfaceElements), AreaStiffness);
		++NumConstraintInits;
	}
	else
	{
		AreaConstraints = MakeShared<FPBDAxialSpringConstraints>(Evolution->Particles(), MoveTemp(SurfaceElements), AreaStiffness);
	}
	++NumConstraintRules;
}

void FClothConstraints::SetVolumeConstraints(TArray<TVector<int32, 2>>&& DoubleBendingEdges, float VolumeStiffness)
{
	check(Evolution);
	check(VolumeStiffness > 0.f && VolumeStiffness <= 1.f);

	ThinShellVolumeConstraints = MakeShared<FPBDSpringConstraints>(Evolution->Particles(), MoveTemp(DoubleBendingEdges), VolumeStiffness);
	++NumConstraintRules;
}

void FClothConstraints::SetVolumeConstraints(TArray<TVector<int32, 3>>&& SurfaceElements, float VolumeStiffness)
{
	check(Evolution);
	check(VolumeStiffness > 0.f && VolumeStiffness <= 1.f);

	VolumeConstraints = MakeShared<TPBDVolumeConstraint<float>>(Evolution->Particles(), MoveTemp(SurfaceElements), VolumeStiffness);
	++NumConstraintRules;
}

void FClothConstraints::SetLongRangeConstraints(const TMap<int32, TSet<int32>>& PointToNeighborsMap, const TConstArrayView<float>& TetherStiffnessMultipliers, const TVector<float, 2>& TetherStiffness, float LimitScale, ETetherMode TetherMode, bool bUseXPBDConstraints)
{
	check(Evolution);

	static const int32 MaxNumTetherIslands = 4;  // The max number of connected neighbors per particle.

	if (bUseXPBDConstraints)
	{
		XLongRangeConstraints = MakeShared<TXPBDLongRangeConstraints<float, 3>>(
			Evolution->Particles(),
			ParticleOffset,
			NumParticles,
			PointToNeighborsMap,
			TetherStiffnessMultipliers,
			MaxNumTetherIslands,
			TetherStiffness);  // TODO: Add LimitScale to the XPBD constraint
	}
	else
	{
		LongRangeConstraints = MakeShared<TPBDLongRangeConstraints<float, 3>>(
			Evolution->Particles(),
			ParticleOffset,
			NumParticles,
			PointToNeighborsMap,
			TetherStiffnessMultipliers,
			MaxNumTetherIslands,
			TetherStiffness,
			LimitScale,
			TetherMode);
	}
	++NumConstraintInits;  // Uses init to update the property tables
	++NumConstraintRules;
}

void FClothConstraints::SetMaximumDistanceConstraints(const TConstArrayView<float>& MaxDistances)
{
	MaximumDistanceConstraints = MakeShared<TPBDSphericalConstraint<float, 3>>(
		ParticleOffset,
		NumParticles,
		*AnimationPositions,
		MaxDistances);
	++NumConstraintRules;
}

void FClothConstraints::SetBackstopConstraints(const TConstArrayView<float>& BackstopDistances, const TConstArrayView<float>& BackstopRadiuses, bool bUseLegacyBackstop)
{
	BackstopConstraints = MakeShared<TPBDSphericalBackstopConstraint<float, 3>>(
		ParticleOffset,
		NumParticles,
		*AnimationPositions,
		*AnimationNormals,
		BackstopRadiuses,
		BackstopDistances,
		bUseLegacyBackstop);
	++NumConstraintRules;
}

void FClothConstraints::SetAnimDriveConstraints(const TConstArrayView<float>& AnimDriveStiffnessMultipliers, const TConstArrayView<float>& AnimDriveDampingMultipliers)
{
	AnimDriveConstraints = MakeShared<TPBDAnimDriveConstraint<float, 3>>(
		ParticleOffset,
		NumParticles,
		*AnimationPositions,
		*OldAnimationPositions,
		AnimDriveStiffnessMultipliers,
		AnimDriveDampingMultipliers);
	++NumConstraintInits;  // Uses init to update the property tables
	++NumConstraintRules;
}

void FClothConstraints::SetShapeTargetConstraints(float ShapeTargetStiffness)
{
	// TODO: Review this constraint. Currently does nothing more than the anim drive with less controls
	check(ShapeTargetStiffness > 0.f && ShapeTargetStiffness <= 1.f);

	ShapeConstraints = MakeShared<TPBDShapeConstraints<float, 3>>(
		ParticleOffset,
		NumParticles,
		*AnimationPositions,
		*AnimationPositions,
		ShapeTargetStiffness);
	++NumConstraintRules;
}

void FClothConstraints::SetSelfCollisionConstraints(const TArray<TVector<int32, 3>>& SurfaceElements, TSet<TVector<int32, 2>>&& DisabledCollisionElements, float SelfCollisionThickness)
{
	SelfCollisionConstraints = MakeShared<TPBDCollisionSpringConstraints<float, 3>>(
		ParticleOffset,
		NumParticles,
		SurfaceElements,
		MoveTemp(DisabledCollisionElements),
		SelfCollisionThickness,
		/*Stiffness =*/ 1.f);
	++NumConstraintInits;  // Self collision has an init
	++NumConstraintRules;  // and a rule
}

void FClothConstraints::SetEdgeProperties(float EdgeStiffness)
{
	if (EdgeConstraints)
	{
		EdgeConstraints->SetStiffness(EdgeStiffness);
	}
	if (XEdgeConstraints)
	{
		XEdgeConstraints->SetStiffness(EdgeStiffness);
	}
}

void FClothConstraints::SetBendingProperties(float BendingStiffness)
{
	if (BendingConstraints)
	{
		BendingConstraints->SetStiffness(BendingStiffness);
	}
	if (XBendingConstraints)
	{
		XBendingConstraints->SetStiffness(BendingStiffness);
	}
}

void FClothConstraints::SetAreaProperties(float AreaStiffness)
{
	if (AreaConstraints)
	{
		AreaConstraints->SetStiffness(AreaStiffness);
	}
	if (XAreaConstraints)
	{
		XAreaConstraints->SetStiffness(AreaStiffness);
	}
}

void FClothConstraints::SetThinShellVolumeProperties(float VolumeStiffness)
{
	if (ThinShellVolumeConstraints)
	{
		ThinShellVolumeConstraints->SetStiffness(VolumeStiffness);
	}
}

void FClothConstraints::SetVolumeProperties(float VolumeStiffness)
{
	if (VolumeConstraints)
	{
		VolumeConstraints->SetStiffness(VolumeStiffness);
	}
}

void FClothConstraints::SetLongRangeAttachmentProperties(const TVector<float, 2>& TetherStiffness)
{
	if (LongRangeConstraints)
	{
		LongRangeConstraints->SetStiffness(TetherStiffness);
	}
	if (XLongRangeConstraints)
	{
		XLongRangeConstraints->SetStiffness(TetherStiffness);
	}
}

void FClothConstraints::SetMaximumDistanceProperties(float MaxDistancesMultiplier)
{
	if (MaximumDistanceConstraints)
	{
		MaximumDistanceConstraints->SetSphereRadiiMultiplier(MaxDistancesMultiplier);
	}
}

void FClothConstraints::SetAnimDriveProperties(const TVector<float, 2>& AnimDriveStiffness, const TVector<float, 2>& AnimDriveDamping)
{
	if (AnimDriveConstraints)
	{
		AnimDriveConstraints->SetProperties(AnimDriveStiffness, AnimDriveDamping);
	}
}

void FClothConstraints::SetSelfCollisionProperties(float SelfCollisionThickness)
{
	if (SelfCollisionConstraints)
	{
		SelfCollisionConstraints->SetThickness(SelfCollisionThickness);
	}
}
