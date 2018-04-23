#
# Build PhysXExtensions
#

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(LL_SOURCE_DIR ${PHYSX_SOURCE_DIR}/PhysXExtensions/src)

SET(PHYSXEXTENSIONS_PLATFORM_INCLUDES
	$ENV{SCE_ORBIS_SDK_DIR}/target/include
)

SET(PHYSXEXTENSIONS_PLATFORM_SRC_FILES
	${LL_SOURCE_DIR}/ps4/ExtPS4DefaultCpuDispatcher.cpp
)


# Use generator expressions to set config specific preprocessor definitions
SET(PHYSXEXTENSIONS_COMPILE_DEFS

	# NOTE: PX_BUILD_NUMBER - probably not good!
	# Common to all configurations
	PX_BUILD_NUMBER=0;${PHYSX_PS4_COMPILE_DEFS};

	$<$<CONFIG:debug>:${PHYSX_PS4_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${PHYSX_PS4_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${PHYSX_PS4_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${PHYSX_PS4_RELEASE_COMPILE_DEFS};>
)

# include common PhysXExtensions settings
INCLUDE(../common/PhysXExtensions.cmake)