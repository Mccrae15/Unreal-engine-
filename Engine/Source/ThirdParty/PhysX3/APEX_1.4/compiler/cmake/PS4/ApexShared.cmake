#
# Build ApexShared
#

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})
FIND_PACKAGE(PxShared REQUIRED)

SET(APEX_MODULE_DIR ${PROJECT_SOURCE_DIR}/../../../module)

SET(SHARED_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../shared)

#SET(AM_SOURCE_DIR ${APEX_MODULE_DIR}/{{TARGET_MODULE_DIR}})

SET(APEXSHARED_PLATFORM_INCLUDES
	$ENV{SCE_ORBIS_SDK_DIR}/target/include
	${PROJECT_SOURCE_DIR}/../../../common/include/ps4	
)

SET(APEX_SHARED_LIBTYPE STATIC)

# Use generator expressions to set config specific preprocessor definitions
SET(APEXSHARED_COMPILE_DEFS 

	# Common to all configurations
	${APEX_PS4_COMPILE_DEFS};_LIB;PX_PHYSX_STATIC_LIB;

	$<$<CONFIG:debug>:${APEX_PS4_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${APEX_PS4_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${APEX_PS4_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${APEX_PS4_RELEASE_COMPILE_DEFS};>
)

# include common ApexShared.cmake
INCLUDE(../common/ApexShared.cmake)