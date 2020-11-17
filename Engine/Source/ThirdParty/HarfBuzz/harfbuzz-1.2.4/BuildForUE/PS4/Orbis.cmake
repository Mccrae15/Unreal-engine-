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

set(CMAKE_CXX_FLAGS "-v -std=c++11 -fdiagnostics-format=msvc -Wall -Werror -Wno-unused-variable -Wno-unused-function -Wno-switch -Wno-constant-logical-operand -Wno-tautological-compare -Wno-unused-private-field -Wno-invalid-offsetof -x c++")
set(CMAKE_C_FLAGS "-v -ansi -fdiagnostics-format=msvc -Wall -Werror -Wno-unused-variable -Wno-unused-function -Wno-switch -Wno-constant-logical-operand -Wno-tautological-compare -Wno-unused-private-field -Wno-invalid-offsetof -x c")

## Debug/Release flags
if(CONFIGURATION STREQUAL "Debug")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG=1 -D_DEBUG -O0 -g")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDEBUG=1 -D_DEBUG -O0 -g")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDEBUG=1 -O3 -ffast-math -funroll-loops")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDEBUG=1 -O3 -ffast-math -funroll-loops")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "c++ flags")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "c flags")
