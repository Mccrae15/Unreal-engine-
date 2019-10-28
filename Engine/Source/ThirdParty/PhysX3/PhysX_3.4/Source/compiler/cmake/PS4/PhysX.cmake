#
# Build PhysX (PROJECT not SOLUTION)
#

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(PX_SOURCE_DIR ${PHYSX_SOURCE_DIR}/PhysX/src)
SET(MD_SOURCE_DIR ${PHYSX_SOURCE_DIR}/PhysXMetaData)


SET(PHYSX_PLATFORM_INCLUDES
	$ENV{SCE_ORBIS_SDK_DIR}/target/include
)

SET(PHYSX_COMPILE_DEFS

	# Common to all configurations
	${PHYSX_PS4_COMPILE_DEFS};

	$<$<CONFIG:debug>:${PHYSX_PS4_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${PHYSX_PS4_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${PHYSX_PS4_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${PHYSX_PS4_RELEASE_COMPILE_DEFS};>
)

SET(PHYSX_LIBTYPE STATIC)

# include common PhysX settings
INCLUDE(../common/PhysX.cmake)

TARGET_LINK_LIBRARIES(PhysX PUBLIC LowLevel LowLevelAABB LowLevelCloth LowLevelDynamics LowLevelParticles PhysXCommon PxFoundation PxPvdSDK PxTask SceneQuery SimulationController)
