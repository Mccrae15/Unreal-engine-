rem TODO: check if this is under UE_SDKS_ROOT ?
set CurrentFolder=%~dp0

del %CurrentFolder%OutputEnvVars.txt

REM MSIEXEC can through 'errors' that we do not want to consider fatal.  So call the error handling routine.
REM Sony says TM servers are going to be backwards compatible, and reinstalling forces reboots, so avoid for now until they DO break a backwards compat...
REM call msiexec /uninstall "%CurrentFolder%NotForLicensees\Installers\TMServer-5_50_0_10.msi" /norestart /quiet
REM CALL :HANDLE_ERROR

REM unncecessary to waste time with this.  the msi installer will overwrite the pub tools, even when downgrading
REM call msiexec /uninstall "%CurrentFolder%NotForLicensees\Installers\PublishingTools-3_38_0_6862.msi" /norestart /quiet
REM CALL :HANDLE_ERROR

GOTO:EOF

:HANDLE_ERROR	
	SET SAFE_ERROR=0
	
	REM set SAFE_ERROR if we got an error that isn't fatal.
	REM 1605 'Action is only valid for products that are currently installed'.
	IF ERRORLEVEL 1605 IF NOT ERRORLEVEL 1606 SET SAFE_ERROR=1
	
	REM 3010 'requires restart'
	IF ERRORLEVEL 3010 IF NOT ERRORLEVEL 3011 SET SAFE_ERROR=1
	
	IF ERRORLEVEL 1 IF NOT %SAFE_ERROR%==1 EXIT %ERRORLEVEL%
	
	GOTO:EOF