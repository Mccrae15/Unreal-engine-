rem Change to directory where batch file is located.  We'll restore this later with "popd"
pushd %~dp0

if "%SCE_ORBIS_SDK_DIR%" neq "" (
  mkdir "%USERPROFILE%\Documents\SCE\orbis-debugger"
  copy PS4UE4.natvis "%USERPROFILE%\Documents\SCE\orbis-debugger"
  echo Installed Visualizers for Orbis
)

popd