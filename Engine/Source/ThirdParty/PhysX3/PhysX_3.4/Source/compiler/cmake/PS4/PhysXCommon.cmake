#
# Build PhysXCommon
#

FIND_PACKAGE(nvToolsExt REQUIRED)

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(COMMON_SRC_DIR ${PHYSX_SOURCE_DIR}/Common/src)
SET(GU_SOURCE_DIR ${PHYSX_SOURCE_DIR}/GeomUtils)

SET(PHYSXCOMMON_LIBTYPE STATIC)

SET(PXCOMMON_PLATFORM_INCLUDES
	$ENV{SCE_ORBIS_SDK_DIR}/target/include
)


# Use generator expressions to set config specific preprocessor definitions
SET(PXCOMMON_COMPILE_DEFS

	# Common to all configurations
	${PHYSX_PS4_COMPILE_DEFS};

	$<$<CONFIG:debug>:${PHYSX_PS4_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${PHYSX_PS4_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${PHYSX_PS4_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${PHYSX_PS4_RELEASE_COMPILE_DEFS};>
)

# include common PhysXCommon settings
INCLUDE(../common/PhysXCommon.cmake)

TARGET_LINK_LIBRARIES(PhysXCommon PUBLIC PxFoundation)





