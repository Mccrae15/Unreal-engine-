// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Declares.h"
#include "Chaos/ChaosDebugDrawDeclares.h"
#include "Chaos/KinematicGeometryParticles.h"
#include "Chaos/PBDRigidParticles.h"
#include "Chaos/Vector.h"

namespace Chaos
{
	namespace DebugDraw
	{
#if CHAOS_DEBUG_DRAW

		struct CHAOS_API FChaosDebugDrawSettings
		{
		public:
			FChaosDebugDrawSettings(
				FReal InArrowSize,
				FReal InBodyAxisLen,
				FReal InContactLen,
				FReal InContactWidth,
				FReal InContactPhiWidth,
				FReal InContactOwnerWidth,
				FReal InConstraintAxisLen,
				FReal InJointComSize,
				FReal InLineThickness,
				FReal InDrawScale,
				FReal InFontHeight,
				FReal InFontScale,
				FReal InShapeThicknesScale,
				FReal InPointSize,
				FReal InVelScale,
				FReal InAngVelScale,
				FReal InImpulseScale,
				int InDrawPriority,
				bool bInShowSimpleCollision,
				bool bInShowComplexCollision,
				bool bInShowLevelSetCollision
				)
				: ArrowSize(InArrowSize)
				, BodyAxisLen(InBodyAxisLen)
				, ContactLen(InContactLen)
				, ContactWidth(InContactWidth)
				, ContactPhiWidth(InContactPhiWidth)
				, ContactOwnerWidth(InContactOwnerWidth)
				, ConstraintAxisLen(InConstraintAxisLen)
				, JointComSize(InJointComSize)
				, LineThickness(InLineThickness)
				, DrawScale(InDrawScale)
				, FontHeight(InFontHeight)
				, FontScale(InFontScale)
				, ShapeThicknesScale(InShapeThicknesScale)
				, PointSize(InPointSize)
				, VelScale(InVelScale)
				, AngVelScale(InAngVelScale)
				, ImpulseScale(InImpulseScale)
				, DrawPriority(InDrawPriority)
				, bShowSimpleCollision(bInShowSimpleCollision)
				, bShowComplexCollision(bInShowComplexCollision)
				, bShowLevelSetCollision(bInShowLevelSetCollision)
			{}

			FReal ArrowSize;
			FReal BodyAxisLen;
			FReal ContactLen;
			FReal ContactWidth;
			FReal ContactPhiWidth;
			FReal ContactOwnerWidth;
			FReal ConstraintAxisLen;
			FReal JointComSize;
			FReal LineThickness;
			FReal DrawScale;
			FReal FontHeight;
			FReal FontScale;
			FReal ShapeThicknesScale;
			FReal PointSize;
			FReal VelScale;
			FReal AngVelScale;
			FReal ImpulseScale;
			int DrawPriority;
			bool bShowSimpleCollision;
			bool bShowComplexCollision;
			bool bShowLevelSetCollision;
		};

		// A bitmask of features to show when drawing joints
		class FChaosDebugDrawJointFeatures
		{
		public:
			FChaosDebugDrawJointFeatures()
				: bCoMConnector(false)
				, bActorConnector(false)
				, bStretch(false)
				, bAxes(false)
				, bLevel(false)
				, bIndex(false)
				, bColor(false)
				, bBatch(false)
				, bIsland(false)
			{}

			static FChaosDebugDrawJointFeatures MakeEmpty()
			{
				return FChaosDebugDrawJointFeatures();
			}

			static FChaosDebugDrawJointFeatures MakeDefault()
			{
				FChaosDebugDrawJointFeatures Features = FChaosDebugDrawJointFeatures();
				Features.bActorConnector = true;
				Features.bStretch = true;
				return Features;
			}

			bool bCoMConnector;
			bool bActorConnector;
			bool bStretch;
			bool bAxes;
			bool bLevel;
			bool bIndex;
			bool bColor;
			bool bBatch;
			bool bIsland;
		};

		CHAOS_API void DrawParticleShapes(const FRigidTransform3& SpaceTransform, const TParticleView<TGeometryParticles<float, 3>>& ParticlesView, const FColor& Color, const FChaosDebugDrawSettings* Settings = nullptr);
		CHAOS_API void DrawParticleShapes(const FRigidTransform3& SpaceTransform, const TParticleView<TKinematicGeometryParticles<float, 3>>& ParticlesView, const FColor& Color, const FChaosDebugDrawSettings* Settings = nullptr);
		CHAOS_API void DrawParticleShapes(const FRigidTransform3& SpaceTransform, const TParticleView<TPBDRigidParticles<float, 3>>& ParticlesView, const FColor& Color, const FChaosDebugDrawSettings* Settings = nullptr);
		CHAOS_API void DrawParticleShapes(const FRigidTransform3& SpaceTransform, const TGeometryParticleHandle<float, 3>* Particle, const FColor& Color, const FChaosDebugDrawSettings* Settings = nullptr);
		CHAOS_API void DrawParticleShapes(const FRigidTransform3& SpaceTransform, const TGeometryParticle<float, 3>* Particle, const FColor& Color, const FChaosDebugDrawSettings* Settings = nullptr);
		CHAOS_API void DrawParticleBounds(const FRigidTransform3& SpaceTransform, const TParticleView<TGeometryParticles<float, 3>>& ParticlesView, const FReal Dt, const FReal BoundsThickness, const FReal BoundsThicknessVelocityInflation, const FChaosDebugDrawSettings* Settings = nullptr);
		CHAOS_API void DrawParticleBounds(const FRigidTransform3& SpaceTransform, const TParticleView<TKinematicGeometryParticles<float, 3>>& ParticlesView, const FReal Dt, const FReal BoundsThickness, const FReal BoundsThicknessVelocityInflation, const FChaosDebugDrawSettings* Settings = nullptr);
		CHAOS_API void DrawParticleBounds(const FRigidTransform3& SpaceTransform, const TParticleView<TPBDRigidParticles<float, 3>>& ParticlesView, const FReal Dt, const FReal BoundsThickness, const FReal BoundsThicknessVelocityInflation, const FChaosDebugDrawSettings* Settings = nullptr);
		CHAOS_API void DrawParticleTransforms(const FRigidTransform3& SpaceTransform, const TParticleView<TGeometryParticles<float, 3>>& ParticlesView, const FChaosDebugDrawSettings* Settings = nullptr);
		CHAOS_API void DrawParticleTransforms(const FRigidTransform3& SpaceTransform, const TParticleView<TKinematicGeometryParticles<float, 3>>& ParticlesView, const FChaosDebugDrawSettings* Settings = nullptr);
		CHAOS_API void DrawParticleTransforms(const FRigidTransform3& SpaceTransform, const TParticleView<TPBDRigidParticles<float, 3>>& ParticlesView, const FChaosDebugDrawSettings* Settings = nullptr);
		CHAOS_API void DrawParticleCollisions(const FRigidTransform3& SpaceTransform, const TGeometryParticleHandle<float, 3>* Particle, const FPBDCollisionConstraints& Collisions, const FChaosDebugDrawSettings* Settings = nullptr);
		CHAOS_API void DrawCollisions(const FRigidTransform3& SpaceTransform, const FPBDCollisionConstraints& Collisions, FReal ColorScale, const FChaosDebugDrawSettings* Settings = nullptr);
		CHAOS_API void DrawCollisions(const FRigidTransform3& SpaceTransform, const TArray<FPBDCollisionConstraintHandle*>& ConstraintHandles, FReal ColorScale, const FChaosDebugDrawSettings* Settings = nullptr);
		CHAOS_API void DrawJointConstraints(const FRigidTransform3& SpaceTransform, const TArray<FPBDJointConstraintHandle*>& ConstraintHandles, FReal ColorScale, const FChaosDebugDrawJointFeatures& FeatureMask = FChaosDebugDrawJointFeatures::MakeDefault(), const FChaosDebugDrawSettings* Settings = nullptr);
		CHAOS_API void DrawJointConstraints(const FRigidTransform3& SpaceTransform, const FPBDJointConstraints& Constraints, float ColorScale, const FChaosDebugDrawJointFeatures& FeatureMask = FChaosDebugDrawJointFeatures::MakeDefault(), const FChaosDebugDrawSettings* Settings = nullptr);
		CHAOS_API void DrawSimulationSpace(const FSimulationSpace& SimSpace, const FChaosDebugDrawSettings* Settings = nullptr);
		CHAOS_API void DrawShape(const FRigidTransform3& ShapeTransform, const FImplicitObject* Shape, const FColor& Color, const FChaosDebugDrawSettings* Settings = nullptr);
		CHAOS_API void DrawConstraintGraph(const FRigidTransform3& ShapeTransform, const FPBDConstraintColor& Graph, const FChaosDebugDrawSettings* Settings = nullptr);
#endif
	}
}
