REM @ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

REM Invoke vcvarsall so that we can use NMake
call "%VS140COMNTOOLS%..\..\VC\vcvarsall.bat" amd64

set PATH_TO_CMAKE_FILE=%CD%\..\..

REM Internal dependencies
set PATH_TO_ZLIB=%CD%\..\..\..\..\zlib\zlib-1.2.5
set PATH_TO_ZLIB_INCLUDES=%PATH_TO_ZLIB%\Inc
set PATH_TO_ZLIB_DUMMY_LIB=%PATH_TO_ZLIB%\lib\PS4

set PATH_TO_PNG=%CD%\..\..\..\..\libPNG\libPNG-1.5.2
set PATH_TO_PNG_SRC=%PATH_TO_PNG%
set PATH_TO_PNG_DUMMY_LIB=%PATH_TO_PNG%\lib\PS4

REM Temporary build directories (used as working directories when running CMake)
set PS4_OUTPUT_PATH="%PATH_TO_CMAKE_FILE%\lib\PS4"
set PS4_BUILD_PATH="%PATH_TO_CMAKE_FILE%\lib\PS4\Build"

REM Build for PS4 (Debug)
echo Generating FreeType2 makefile for PS4 (Debug)...
if exist %PS4_BUILD_PATH% (rmdir %PS4_BUILD_PATH% /s/q)
mkdir %PS4_BUILD_PATH%
cd %PS4_BUILD_PATH%
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE="Debug" -DCMAKE_PREFIX_PATH="%PATH_TO_ZLIB_INCLUDES%;%PATH_TO_ZLIB_DUMMY_LIB%;%PATH_TO_PNG_SRC%;%PATH_TO_PNG_DUMMY_LIB%" -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CMAKE_FILE%\BuildForUE\PS4\Orbis.cmake" %PATH_TO_CMAKE_FILE%
echo Building FreeType2 for PS4 (Debug)...
nmake freetype
mkdir "%PS4_OUTPUT_PATH%\Debug" 2> nul
move /y "%PS4_BUILD_PATH%\libfreetyped.a" "%PS4_OUTPUT_PATH%\Debug\libfreetype.a"
cd %PATH_TO_CMAKE_FILE%
rmdir %PS4_BUILD_PATH% /s/q

REM Build for Switch (Release)
echo Generating FreeType2 makefile for PS4 (Release)...
if exist %PS4_BUILD_PATH% (rmdir %PS4_BUILD_PATH% /s/q)
mkdir %PS4_BUILD_PATH%
cd %PS4_BUILD_PATH%
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE="Release" -DCMAKE_PREFIX_PATH="%PATH_TO_ZLIB_INCLUDES%;%PATH_TO_ZLIB_DUMMY_LIB%;%PATH_TO_PNG_SRC%;%PATH_TO_PNG_DUMMY_LIB%" -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CMAKE_FILE%\BuildForUE\PS4\Orbis.cmake" %PATH_TO_CMAKE_FILE%
echo Building FreeType2 for PS4 (Release)...
nmake freetype
mkdir "%PS4_OUTPUT_PATH%\Release" 2> nul
move /y "%PS4_BUILD_PATH%\libfreetype.a" "%PS4_OUTPUT_PATH%\Release\libfreetype.a"
cd %PATH_TO_CMAKE_FILE%

rmdir %PS4_BUILD_PATH% /s/q

endlocal
