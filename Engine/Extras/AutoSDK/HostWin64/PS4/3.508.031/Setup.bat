rem TODO: check if this is under UE_SDKS_ROOT ?
set CurrentFolder=%~dp0
echo %CurrentFolder%

REM delete OutputEnvVars from the old location
del "%CurrentFolder%OutputEnvVars.txt"

set OutputEnvVarsFolder=%~dp0..\
del "%OutputEnvVarsFolder%OutputEnvVars.txt"

REM Set SCE_ROOT_DIR if it doesn't exist, but otherwise don't override it.  The installer msi's have logic to install to a path that isn't solely based on SCE_ROOT_DIR,
REM so if it's already set to a non-standard location, overriding it to the standard location will break builds when the installers install to the non-standard location.
IF NOT "%SCE_ROOT_DIR%" == "" GOTO SKIPSCEROOTOVERRIDE
:SCEROOTOVERRIDE
echo Setting SCE_ROOT_DIR
set SCE_ROOT_DIR=C:\Program Files (x86)\SCE
echo SCE_ROOT_DIR=%SCE_ROOT_DIR%>"%OutputEnvVarsFolder%OutputEnvVars.txt"


:SKIPSCEROOTOVERRIDE
echo SCE_ORBIS_SDK_DIR=%CurrentFolder%NotForLicensees\3.500>>"%OutputEnvVarsFolder%OutputEnvVars.txt"
echo SCE_ORBIS_SAMPLE_DIR=%CurrentFolder%NotForLicensees\3.500\target\samples>>"%OutputEnvVarsFolder%OutputEnvVars.txt"

REM strip anything Orbis related from the process local PATH var.  We want to protect against code calling one of the SDK tools without being properly pathed through one of the environment variables.
echo STRIPPATH=Orbis>>"%OutputEnvVarsFolder%OutputEnvVars.txt"

REM specifically put TMServer back on the path so the COM ORTMAPI object can start it up succesfully.
echo ADDPATH=%SCE_ROOT_DIR%\ORBIS\Tools\Target Manager Server\bin>>"%OutputEnvVarsFolder%OutputEnvVars.txt"

REM PSFSD installer is finicky remove cached version first
rmdir /s /q "C:\ProgramData\SCE\PSFSD"
call "%CurrentFolder%NotForLicensees\Installers\PSFSD-6_0_172_4.exe" -install -silent
call "%CurrentFolder%NotForLicensees\Installers\TMServer-3_50_0_55.msi" /norestart /quiet
call "%CurrentFolder%NotForLicensees\Installers\PublishingTools-2_68_0_5609.msi" /norestart /quiet
call "%CurrentFolder%NotForLicensees\Installers\Neighborhood-3_50_0_13.msi" /norestart /quiet
rmdir /s /q "C:\ProgramData\SCE\PSFSD"

REM support old branches that expect this file in a different location.
copy "%OutputEnvVarsFolder%OutputEnvVars.txt" "%CurrentFolder%OutputEnvVars.txt"