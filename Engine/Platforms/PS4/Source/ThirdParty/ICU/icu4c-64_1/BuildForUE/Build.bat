@echo off

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

REM Temporary build directories (used as working directories when running CMake)
set MAKE_PATH="%CD%\..\..\..\..\..\..\..\Extras\ThirdPartyNotUE\GNU_Make\make-3.81\bin"
set SOURCE_PATH="%CD%\..\..\..\..\..\..\..\Source\ThirdParty\ICU\icu4c-64_1\BuildForUE"
set BUILD_PATH="%CD%\..\lib\Build"
set TOOLCHAIN="%CD%\Toolchain.cmake"

REM Place make in the path so it can be found when we build the libs
set PATH=MAKE_PATH;%PATH%

call :BuildLib Debug
call :BuildLib Release

endlocal

exit /B 0

:BuildLib
	pushd

	echo Generating %~1 ICU makefile...

	REM clean the build directory
	if exist %BUILD_PATH% (rmdir %BUILD_PATH% /s/q)
	mkdir %BUILD_PATH%

	cd %BUILD_PATH%

	REM generate the nmake file
	cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE="%~1" -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN%" %SOURCE_PATH% -DCMAKE_MAKE_PROGRAM="%MAKE_PATH%\make.exe"

	REM call nmake to do the actual build
	echo Building (%~1) ICU...
	"%MAKE_PATH%\make.exe" -j 16 MAKE="%MAKE_PATH%\make.exe -j 16" icu

	REM copy libs to output directory
	mkdir "%BUILD_PATH%\..\%~1" 2> nul
	move /y "%BUILD_PATH%\..\libicu.a" "%BUILD_PATH%\..\%~1\libicu.a"
	
	REM clean up
	rmdir %BUILD_PATH% /s/q

	popd
	exit /B 0
