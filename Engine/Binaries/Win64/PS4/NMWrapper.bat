@echo off
REM This batch file wraps nm to allow redirecting stdout to a file.

REM %1 is the path to nm
REM %2 is the self file
REM %3 is the symbol output location

%1 --print-size -C %2 1>%3 2>NUL