cmake_minimum_required(VERSION 3.3)

SET(NVCLOTH_ENGINE_COMMON $ENV{GW_DEPS_ROOT}/NvCloth/compiler/cmake/common)

include(${NVCLOTH_ENGINE_COMMON}/CMakeLists.txt)

IF(NOT DEFINED TARGET_BUILD_PLATFORM) # Not defined, default to PS4
	SET(TARGET_BUILD_PLATFORM "ps4")
ENDIF()

SET(PLATFORM_LIST ps4)

IF (NOT ${TARGET_BUILD_PLATFORM} IN_LIST PLATFORM_LIST)
	MESSAGE(FATAL_ERROR "Invalid platform:" ${TARGET_BUILD_PLATFORM})
ENDIF()

SET(CMAKE_CXX_FLAGS "-fdiagnostics-show-option -fno-rtti -fno-exceptions -ffast-math -ffunction-sections -fdata-sections -Werror -ferror-limit=0 -Wall -Wextra -fstrict-aliasing -Wstrict-aliasing=2 -pedantic -Weverything -Wno-undef -Wno-unused-value -Wno-c++98-compat-pedantic -Wno-format-nonliteral -Wno-conversion -Wno-deprecated -Wno-documentation-deprecated-sync -Wno-documentation-unknown-command -Wno-exit-time-destructors -Wno-float-equal -Wno-global-constructors -Wno-missing-prototypes -Wno-missing-variable-declarations -Wno-padded -Wno-shift-sign-overflow -Wno-sign-conversion -Wno-unreachable-code -Wno-unused-macros -Wno-used-but-marked-unused -Wno-weak-template-vtables -Wno-weak-vtables -Wno-zero-as-null-pointer-constant -Wno-shadow-field -Wno-unused-template -Wno-shadow -Wno-c++2a-compat -Wno-unused-private-field -Wno-inconsistent-missing-destructor-override")

SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
SET(CMAKE_CXX_FLAGS_CHECKED "-O3 -g")
SET(CMAKE_CXX_FLAGS_PROFILE "-O3 -g")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -g")

SET(NVCLOTH_PS4_COMPILE_DEFS _LIB;_CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE;PX_NVCLOTH_STATIC_LIB;PX_EXTERNAL_PLATFORM=1;PX_PS4=1)
SET(NVCLOTH_PS4_DEBUG_COMPILE_DEFS _DEBUG;PX_DEBUG=1;PX_CHECKED=1)
SET(NVCLOTH_PS4_CHECKED_COMPILE_DEFS NDEBUG;PX_CHECKED=1)
SET(NVCLOTH_PS4_PROFILE_COMPILE_DEFS NDEBUG;PX_PROFILE=1)
SET(NVCLOTH_PS4_RELEASE_COMPILE_DEFS NDEBUG)

FIND_PACKAGE(PxShared REQUIRED)

# Include all of the projects
INCLUDE(NvCloth.cmake)