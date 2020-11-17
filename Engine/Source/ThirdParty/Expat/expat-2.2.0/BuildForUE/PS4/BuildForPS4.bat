REM ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

set PATH_TO_CMAKE_FILE=%CD%\..\..

REM Temporary build directories (used as working directories when running CMake)
set PS4_PATH="%PATH_TO_CMAKE_FILE%\PS4\Build"

REM MSBuild Directory
for /f "skip=2 tokens=2,*" %%R in ('reg.exe query "HKLM\SOFTWARE\Microsoft\MSBuild\ToolsVersions\14.0" /v MSBuildToolsPath') do SET _msbuild=%%S
if not exist "%_msbuild%msbuild.exe" goto MSBuildMissing

REM CMake global include directory
set CMAKE_GLOBAL_DIR="%PATH_TO_CMAKE_FILE%\..\..\CMake\PlatformScripts"

REM Build for PS4
echo Generating Expat solution for PS4...
if exist "%PS4_PATH%" (rmdir "%PS4_PATH%" /s/q)
mkdir "%PS4_PATH%"
cd "%PS4_PATH%"
cmake -G "Visual Studio 14 2015" -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CMAKE_FILE%\BuildForUE\PS4\PS4.cmake" -DCMAKE_SUPPRESS_REGENERATION=1 -DBUILD_tools=0 -DBUILD_examples=0 -DBUILD_tests=0 -DBUILD_shared=0 %PATH_TO_CMAKE_FILE%
echo Building Expat solution for PS4 (Debug)...
"%_msbuild%msbuild.exe" expat.vcxproj /t:build /p:Configuration=Debug
echo Building Expat solution for VS2015 (64-bit, Release)...
"%_msbuild%msbuild.exe" expat.vcxproj /t:build /p:Configuration=Release
cd "%PATH_TO_CMAKE_FILE%"
copy /B/Y "%PS4_PATH%\expat.dir\Release\expat.pdb" "%PS4_PATH%\..\Release\expat.pdb"
rmdir "%PS4_PATH%" /s/q
PAUSE
goto Exit

:MSBuildMissing
echo MSBuild not found. Please check your Visual Studio install and try again.
goto Exit

:Exit
endlocal
