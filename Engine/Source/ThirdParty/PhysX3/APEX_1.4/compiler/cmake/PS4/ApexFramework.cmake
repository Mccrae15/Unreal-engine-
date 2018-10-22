#
# Build ApexFramework
#

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})
FIND_PACKAGE(PxShared REQUIRED)

SET(APEX_MODULE_DIR ${PROJECT_SOURCE_DIR}/../../../module)

SET(FRAMEWORK_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../framework)

SET(APEXFRAMEWORK_LIBTYPE STATIC)

SET(APEXFRAMEWORK_PLATFORM_INCLUDES
	$ENV{SCE_ORBIS_SDK_DIR}/target/include
)

SET(APEXFRAMEWORK_PLATFORM_SOURCE_FILES
)


# Use generator expressions to set config specific preprocessor definitions
SET(APEXFRAMEWORK_COMPILE_DEFS 

	# Common to all configurations

	__PS4__;PS4;_CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE;NX_USE_SDK_STATICLIBS;NX_FOUNDATION_STATICLIB;NV_PARAMETERIZED_HIDE_DESCRIPTIONS=1;_LIB;

	$<$<CONFIG:debug>:${APEX_PS4_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${APEX_PS4_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${APEX_PS4_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${APEX_PS4_RELEASE_COMPILE_DEFS};>
)

# include common ApexFramework.cmake
INCLUDE(../common/ApexFramework.cmake)

# Do final direct sets after the target has been defined
TARGET_LINK_LIBRARIES(ApexFramework PUBLIC ApexCommon ApexShared NvParameterized PsFastXml PxFoundation PxPvdSDK PxTask RenderDebug
#	PhysxCommon - does it really need this?
)
