REM ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

set PATH_TO_CMAKE_FILE=%CD%\..\

REM Temporary build directories (used as working directories when running CMake)
set PS4_PATH="%PATH_TO_CMAKE_FILE%\..\PS4\Build"


REM MSBuild Directory
SET _msbuild="C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Professional\\MSBuild\\15.0\\Bin"
if not exist %_msbuild%\\msbuild.exe SET _msbuild="C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\MSBuild\\15.0\\Bin"
if not exist %_msbuild%\\msbuild.exe SET _msbuild="C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Exterprise\\MSBuild\\15.0\\Bin"
if not exist %_msbuild%\\msbuild.exe goto MSBuildMissing

REM CMake global include directory
set CMAKE_GLOBAL_DIR="%PATH_TO_CMAKE_FILE%\..\..\..\CMake\PlatformScripts"

REM Build for PS4
echo Generating libstrophe solution for VS2015 (PS4)...
if exist "%PS4_PATH%" (rmdir "%PS4_PATH%" /s/q)
mkdir "%PS4_PATH%"
cd "%PS4_PATH%"
cmake -G "Visual Studio 15 2017" -DCMAKE_TOOLCHAIN_FILE="%CMAKE_GLOBAL_DIR%\\PS4\\PS4.cmake" -DSOCKET_IMPL=PS4/sock_ps4.c -DDISABLE_TLS=0 -DDISABLE_SRV_LOOKUP=1 -DOPENSSL_PATH=../../../OpenSSL/1.0.2g/include/PS4 -DCMAKE_SUPPRESS_REGENERATION=1 %PATH_TO_CMAKE_FILE%
echo Building libstrophe solution for PS4 (Debug)...
%_msbuild%\\msbuild.exe libstrophe.sln /t:build /p:Configuration=Debug /p:Platform=Orbis
echo Building libstrophe solution for PS4 (Release)...
%_msbuild%\\msbuild.exe libstrophe.sln /t:build /p:Configuration=Release /p:Platform=Orbis
cd "%PATH_TO_CMAKE_FILE%"
rmdir "%PS4_PATH%" /s/q
goto Exit

:MSBuildMissing
echo MSBuild not found. Please check your Visual Studio install and try again.
goto Exit

:Exit
endlocal
