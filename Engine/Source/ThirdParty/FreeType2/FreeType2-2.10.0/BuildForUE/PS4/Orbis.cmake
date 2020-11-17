set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)

set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_WORKS TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)

set(SCE_ORBIS_SDK_DIR $ENV{SCE_ORBIS_SDK_DIR})
set(CONFIGURATION $ENV{configuration} CACHE STRING "Configuration type: Debug/Release")

if(NOT EXISTS ${SCE_ORBIS_SDK_DIR})
	message(FATAL_ERROR "${SCE_ORBIS_SDK_DIR} is missing. Please ensure that SCE_ORBIS_SDK_DIR is set correctly.")
endif()

set(CMAKE_C_COMPILER "${SCE_ORBIS_SDK_DIR}/host_tools/bin/orbis-clang.exe" CACHE PATH "C Compiler")
set(CMAKE_CXX_COMPILER "${SCE_ORBIS_SDK_DIR}/host_tools/bin/orbis-clang++.exe" CACHE PATH "C++ Compiler")

set(CMAKE_AS "${SCE_ORBIS_SDK_DIR}/host_tools/bin/orbis-as.exe" CACHE PATH "Archive")
set(CMAKE_AR "${SCE_ORBIS_SDK_DIR}/host_tools/bin/orbis-ar.exe" CACHE PATH "Archive")
set(CMAKE_LINKER "${SCE_ORBIS_SDK_DIR}/host_tools/bin/orbis-ld.exe" CACHE PATH "Linker")
set(CMAKE_NM "${SCE_ORBIS_SDK_DIR}/host_tools/bin/orbis-nm.exe" CACHE PATH "NM")
set(CMAKE_OBJCOPY "${SCE_ORBIS_SDK_DIR}/host_tools/bin/orbis-objcopy.exe" CACHE PATH "ObjCopy")
set(CMAKE_OBJDUMP "${SCE_ORBIS_SDK_DIR}/host_tools/bin/orbis-objdump.exe" CACHE PATH "ObjDump")

## Setup compiler flags
## Common flags
set(CMAKE_COMMON_FLAGS "-v -fdiagnostics-format=msvc -mno-omit-leaf-frame-pointer -Wdelete-non-virtual-dtor -Wall -Werror -Wno-unused-function -Wno-unused-variable -Wno-undef")

## Debug/Release flags
if(CONFIGURATION STREQUAL "Debug")
	set(CMAKE_COMMON_FLAGS "${CMAKE_COMMON_FLAGS} -DDEBUG=1 -D_DEBUG -O0 -g")
else()
	set(CMAKE_COMMON_FLAGS "${CMAKE_COMMON_FLAGS} -DNDEBUG=1 -O3 -ffast-math -funroll-loops")
endif()

## Source (language) dependent flags
set(CMAKE_C_FLAGS "-v ${CMAKE_COMMON_FLAGS} -ansi -x c")
set(CMAKE_CXX_FLAGS "-v ${CMAKE_COMMON_FLAGS} -std=c++11 -frtti -x c++")

## Compiler features - workaround for forcing compiler
set(CMAKE_CXX_COMPILE_FEATURES "cxx_std_11")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "c flags")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "c++ flags")

## Settings (hacks) specific to this project
set(CMAKE_CXX_FLAGS "-frtti ${CMAKE_CXX_FLAGS}")

add_definitions(-DU_ENABLE_DYLOAD=0 -DU_HAVE_TZNAME -DU_HAVE_TZSET -DU_HAVE_TIMEZONE)