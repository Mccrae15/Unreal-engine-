cmake_minimum_required(VERSION 3.3)
PROJECT(PxShared CXX)
include(../common/CMakeLists.txt)


IF(NOT DEFINED TARGET_BUILD_PLATFORM) # Not defined, default to ps4
	SET(TARGET_BUILD_PLATFORM "ps4")
ENDIF()

SET(PLATFORM_LIST ps4)

IF (NOT ${TARGET_BUILD_PLATFORM} IN_LIST PLATFORM_LIST)
	MESSAGE(FATAL_ERROR "Invalid platform:" ${TARGET_BUILD_PLATFORM})
ENDIF()

SET(CMAKE_CXX_FLAGS "-fdiagnostics-show-option -fno-rtti -fno-exceptions -ffast-math -ffunction-sections -fdata-sections -Werror -ferror-limit=0 -Wall -Wextra -fstrict-aliasing -Wstrict-aliasing=2 -pedantic -Weverything -Wno-documentation-deprecated-sync -Wno-documentation-unknown-command -Wno-float-equal -Wno-padded -Wno-weak-vtables -Wno-cast-align -Wno-conversion -Wno-missing-noreturn -Wno-missing-variable-declarations -Wno-shift-sign-overflow -Wno-covered-switch-default -Wno-exit-time-destructors -Wno-global-constructors -Wno-missing-prototypes -Wno-unreachable-code -Wno-unused-macros -Wno-unused-member-function -Wno-used-but-marked-unused -Wno-weak-template-vtables -Wno-deprecated")

SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
SET(CMAKE_CXX_FLAGS_CHECKED "-O2")
SET(CMAKE_CXX_FLAGS_PROFILE "-O2")
SET(CMAKE_CXX_FLAGS_RELEASE "-O2")

SET(PXSHARED_PS4_COMPILE_DEFS _LIB;_CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE)
SET(PXSHARED_PS4_DEBUG_COMPILE_DEFS _DEBUG;PX_DEBUG=1;PX_CHECKED=1)
SET(PXSHARED_PS4_CHECKED_COMPILE_DEFS NDEBUG;PX_CHECKED=1)
SET(PXSHARED_PS4_PROFILE_COMPILE_DEFS NDEBUG;PX_PROFILE=1)
SET(PXSHARED_PS4_RELEASE_COMPILE_DEFS NDEBUG)

# NOTE: PxCudaContextManager excluded on this platform

# Include project cmake files here
INCLUDE(PxFoundation.cmake)
INCLUDE(PsFastXml.cmake)
INCLUDE(PxPvdSDK.cmake)
INCLUDE(PxTask.cmake)
