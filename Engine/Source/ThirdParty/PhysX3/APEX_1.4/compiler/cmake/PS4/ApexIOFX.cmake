#
# Build APEX_IOFX
#

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})
FIND_PACKAGE(PxShared REQUIRED)

SET(APEX_MODULE_DIR ${PROJECT_SOURCE_DIR}/../../../module)

SET(AM_SOURCE_DIR ${APEX_MODULE_DIR}/iofx)

SET(APEX_IOFX_PLATFORM_INCLUDES
	$ENV{SCE_ORBIS_SDK_DIR}/target/include
)

# Use generator expressions to set config specific preprocessor definitions
SET(APEX_IOFX_COMPILE_DEFS 

	# Common to all configurations
	${APEX_PS4_COMPILE_DEFS};_LIB;PX_PHYSX_STATIC_LIB;

	$<$<CONFIG:debug>:${APEX_PS4_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${APEX_PS4_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${APEX_PS4_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${APEX_PS4_RELEASE_COMPILE_DEFS};>
)

# include common ApexIOFX.cmake
INCLUDE(../common/ApexIOFX.cmake)