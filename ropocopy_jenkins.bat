set SOURCE= %1
set DESTINATION= %2

robocopy /MIR %SOURCE% %DESTINATION%
@echo robocopy exit code: %ERRORLEVEL%
@if %ERRORLEVEL% GTR 3 ( echo robocopy ERROR )
@if %ERRORLEVEL% GTR 3 ( exit %ERRORLEVEL% )
@set ERRORLEVEL=0