REM ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

set PATH_TO_CMAKE_FILE=%CD%\..\

REM Temporary build directories (used as working directories when running CMake)
set PS4_PATH="%PATH_TO_CMAKE_FILE%\..\PS4\Build"

REM MSBuild Directory
if "%VSWHERE%"=="" set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -version 15.0 -requires Microsoft.Component.MSBuild -property installationPath`) do SET _msbuild=%%i\MSBuild\15.0\Bin\MSBuild.exe
if not exist "%_msbuild%" goto MSBuildMissing

REM CMake global include directory
set CMAKE_GLOBAL_DIR="%PATH_TO_CMAKE_FILE%\..\..\..\CMake\PlatformScripts"

REM Build for PS4
echo Generating vivoxclientapi solution for VS2017 (PS4)...
if exist "%PS4_PATH%" (rmdir "%PS4_PATH%" /s/q)
mkdir "%PS4_PATH%"
cd "%PS4_PATH%"
cmake -G "Visual Studio 15 2017" -DCMAKE_TOOLCHAIN_FILE="%CMAKE_GLOBAL_DIR%\PS4\PS4.cmake" -DVIVOXSDK_PATH=../../vivox-sdk/Include -DCMAKE_SUPPRESS_REGENERATION=1 %PATH_TO_CMAKE_FILE% -DUSE_LOGIN_SESSION_AUDIO_SETTINGS=1 -DVALIDATE_AUDIO_DEVICE_SELECTION=0
echo Building vivoxclientapi solution for PS4 (Debug)...
"%_msbuild%" vivoxclientapi.sln /t:build /p:Configuration=Debug /p:Platform=Orbis
echo Building vivoxclientapi solution for PS4 (Release)...
"%_msbuild%" vivoxclientapi.sln /t:build /p:Configuration=Release /p:Platform=Orbis
cd "%PATH_TO_CMAKE_FILE%"
rmdir "%PS4_PATH%" /s/q
goto Exit

:MSBuildMissing
echo MSBuild not found. Please check your Visual Studio install and try again.
goto Exit

:Exit
endlocal
