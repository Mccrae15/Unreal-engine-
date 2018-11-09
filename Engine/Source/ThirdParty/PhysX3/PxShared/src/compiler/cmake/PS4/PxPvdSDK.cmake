#
# Build PxPvdSDK
#

SET(PXSHARED_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../../src)

SET(LL_SOURCE_DIR ${PXSHARED_SOURCE_DIR}/pvd)

SET(PXPVDSDK_LIBTYPE STATIC)

SET(PXPVDSDK_PLATFORM_INCLUDES
	$ENV{SCE_ORBIS_SDK_DIR}/target/include
)

# Use generator expressions to set config specific preprocessor definitions
SET(PXPVDSDK_COMPILE_DEFS 

	# Common to all configurations
	${PXSHARED_PS4_COMPILE_DEFS}

	$<$<CONFIG:debug>:${PXSHARED_PS4_DEBUG_COMPILE_DEFS}>
	$<$<CONFIG:checked>:${PXSHARED_PS4_CHECKED_COMPILE_DEFS}>
	$<$<CONFIG:profile>:${PXSHARED_PS4_PROFILE_COMPILE_DEFS}>
	$<$<CONFIG:release>:${PXSHARED_PS4_RELEASE_COMPILE_DEFS}>
)

# include PxPvdSDK common
INCLUDE(../common/PxPvdSDK.cmake)

# Add linked libraries
TARGET_LINK_LIBRARIES(PxPvdSDK PRIVATE PxFoundation)

