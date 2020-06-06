@echo off

rem Change to directory where batch file is located.  We'll restore this later with "popd"
pushd %~dp0

if "%SCE_ORBIS_SDK_DIR%" neq "" (
  if not exist "%USERPROFILE%\Documents\SCE\orbis-debugger\" (
    mkdir "%USERPROFILE%\Documents\SCE\orbis-debugger\"
    if errorlevel 1 (
      echo Failed to create PS4 visualizer folder
      goto :end
    )
  )
  copy /y PS4UE4.natvis "%USERPROFILE%\Documents\SCE\orbis-debugger" >nul:
  if errorlevel 1 (
    echo Failed to copy PS4UE4.natvis file
    goto :end
  )
)

:end
popd