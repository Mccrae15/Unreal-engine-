@ECHO OFF

REM Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

setlocal

REM Invoke vcvarsall so that we can use NMake
call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" amd64

set PATH_TO_CMAKE_FILE=%CD%\..\

REM Temporary build directories (used as working directories when running CMake)
set PS4_BUILD_PATH="%PATH_TO_CMAKE_FILE%\..\PS4\Build"

REM Build for PS4 (Debug)
echo Generating HarfBuzz makefile for PS4 (Debug)...
if exist %PS4_BUILD_PATH% (rmdir %PS4_BUILD_PATH% /s/q)
mkdir %PS4_BUILD_PATH%
cd %PS4_BUILD_PATH%
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE="Debug" -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CMAKE_FILE%\PS4\Orbis.cmake" -DUSE_INTEL_ATOMIC_PRIMITIVES=TRUE -DUSE_STUB_GETENV=TRUE %PATH_TO_CMAKE_FILE%
echo Building HarfBuzz makefile for PS4 (Debug)...
nmake harfbuzz
mkdir "%PS4_BUILD_PATH%\..\Debug"
move /y "%PS4_BUILD_PATH%\..\libharfbuzz.a" "%PS4_BUILD_PATH%\..\Debug\libharfbuzz.a"
cd %PATH_TO_CMAKE_FILE%
rmdir %PS4_BUILD_PATH% /s/q

REM Build for PS4 (Release)
echo Generating HarfBuzz makefile for PS4 (Release)...
if exist %PS4_BUILD_PATH% (rmdir %PS4_BUILD_PATH% /s/q)
mkdir %PS4_BUILD_PATH%
cd %PS4_BUILD_PATH%
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE="Release" -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CMAKE_FILE%\PS4\Orbis.cmake" -DUSE_INTEL_ATOMIC_PRIMITIVES=TRUE -DUSE_STUB_GETENV=TRUE %PATH_TO_CMAKE_FILE%
echo Building HarfBuzz makefile for PS4 (Release)...
nmake harfbuzz
mkdir "%PS4_BUILD_PATH%\..\Release"
move /y "%PS4_BUILD_PATH%\..\libharfbuzz.a" "%PS4_BUILD_PATH%\..\Release\libharfbuzz.a"
cd %PATH_TO_CMAKE_FILE%
rmdir %PS4_BUILD_PATH% /s/q

endlocal
