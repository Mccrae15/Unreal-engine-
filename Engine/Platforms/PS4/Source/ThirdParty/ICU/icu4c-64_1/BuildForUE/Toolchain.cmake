
# This isn't a Linux platform, but it's close enough to work
set(CMAKE_SYSTEM_NAME Linux)

# SDK Path setup
set(SCE_ORBIS_SDK_DIR $ENV{SCE_ORBIS_SDK_DIR})
if(NOT EXISTS ${SCE_ORBIS_SDK_DIR})
	message(FATAL_ERROR "${SCE_ORBIS_SDK_DIR} is missing. Please ensure that SCE_ORBIS_SDK_DIR is set correctly.")
endif()

set(CONFIGURATION $ENV{configuration} CACHE STRING "Configuration type: Debug/Release")

set(CMAKE_AR           "${SCE_ORBIS_SDK_DIR}/host_tools/bin/orbis-ar.exe" CACHE PATH "Archive")
set(CMAKE_C_COMPILER   "${SCE_ORBIS_SDK_DIR}/host_tools/bin/orbis-clang.exe"   CACHE PATH "C Compiler")
set(CMAKE_CXX_COMPILER "${SCE_ORBIS_SDK_DIR}/host_tools/bin/orbis-clang++.exe"   CACHE PATH "C++ Compiler")

# Enable C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable runtime type info
set(CMAKE_CXX_FLAGS "-frtti ${CMAKE_CXX_FLAGS}")

# Common flags
set(CMAKE_COMMON_FLAGS "-v -fdiagnostics-format=msvc -mno-omit-leaf-frame-pointer -Wdelete-non-virtual-dtor -Wall -Werror -Wno-unused-function -Wno-unused-variable -Wno-undef")

# Debug/Release flags
if(CONFIGURATION STREQUAL "Debug")
	set(CMAKE_COMMON_FLAGS "${CMAKE_COMMON_FLAGS} -DDEBUG=1 -D_DEBUG -O0 -g")
else()
	set(CMAKE_COMMON_FLAGS "${CMAKE_COMMON_FLAGS} -DNDEBUG=1 -O3 -ffast-math -funroll-loops")
endif()

# Definitions
add_definitions(-DU_PLATFORM=9999 -D_XOPEN_SOURCE -DU_HAVE_NL_LANGINFO_CODESET=0 -DU_HAVE_TZNAME=0 -DU_HAVE_TZSET=0 -DU_HAVE_TIMEZONE=0 -DU_PLATFORM_HAS_GETENV=0 -DHAVE_DLFCN_H=0 -DU_ENABLE_DYLOAD=0)

# Hack to resolve include issues with <signal.h>
include_directories(AFTER SYSTEM "${SCE_ORBIS_SDK_DIR}/target/include/machine")
