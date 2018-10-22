#
# Build LowLevelParticles
#

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(LL_SOURCE_DIR ${PHYSX_SOURCE_DIR}/LowLevelParticles/src)

SET(LOWLEVELPARTICLES_PLATFORM_INCLUDES
	$ENV{SCE_ORBIS_SDK_DIR}/target/include
)


# Use generator expressions to set config specific preprocessor definitions
SET(LOWLEVELPARTICLES_COMPILE_DEFS 

	${PHYSX_PS4_COMPILE_DEFS}

	$<$<CONFIG:debug>:${PHYSX_PS4_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${PHYSX_PS4_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${PHYSX_PS4_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${PHYSX_PS4_RELEASE_COMPILE_DEFS};>
)

# include common low level particles settings
INCLUDE(../common/LowLevelParticles.cmake)