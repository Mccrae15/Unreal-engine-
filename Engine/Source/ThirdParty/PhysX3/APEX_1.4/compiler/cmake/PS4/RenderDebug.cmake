#
# Build RenderDebug
#

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})
FIND_PACKAGE(PxShared REQUIRED)

SET(APEX_MODULE_DIR ${PROJECT_SOURCE_DIR}/../../../module)

SET(RD_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../shared/general/RenderDebug)

SET(RENDERDEBUG_PLATFORM_INCLUDES
	$ENV{SCE_ORBIS_SDK_DIR}/target/include
)

SET(RENDERDEBUG_LIBTYPE STATIC)

# Use generator expressions to set config specific preprocessor definitions
SET(RENDERDEBUG_COMPILE_DEFS 
	# Common to all configurations

	${APEX_PS4_COMPILE_DEFS};PX_FOUNDATION_DLL=0;__PS4__;PS4;

	$<$<CONFIG:debug>:${APEX_PS4_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${APEX_PS4_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${APEX_PS4_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${APEX_PS4_RELEASE_COMPILE_DEFS};>
)

# include common RenderDebug.cmake
INCLUDE(../common/RenderDebug.cmake)
