@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd %~dp0

set source="https://scontent.oculuscdn.com/v/t64.5771-25/10000000_262987555171346_6729143540330336339_n.tar/sho_20210118_121146.tar?_nc_cat=102&ccb=2&_nc_sid=f4d450&_nc_ohc=VbxWwVpqySoAX9zXgIU&_nc_ht=scontent.oculuscdn.com&_nc_rmd=260&oh=5d7041ac54543450effd7aedae74e96e&oe=602D70CE"
set archive=%TMP%\archive_%date:~10,4%%date:~4,2%%date:~7,2%_%time:~0,2%%time:~3,2%%time:~6,2%.tar

where curl.exe > nul 2> nul && where tar.exe > nul 2> nul
if errorlevel 1 (
  echo We depend on curl.exe and tar.exe to download the following URI:
  echo %source%
  echo Either do the operation manually using other commands or update Windows.
  exit /b 1
)

echo Downloading Content...
curl %source% --output %archive%
if errorlevel 1 (
  echo Failed to download content!
  exit /b 1
)

echo.
echo Extracting Content...
tar -xvf %archive%
if errorlevel 1 (
  echo Failed to extract Content!
  exit /b 1
)
del /q %archive% > nul 2> nul

echo.
echo Success!
endlocal