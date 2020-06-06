@echo off
REM This batch file wraps nm to allow redirecting stdout to a file.

REM %1 is the path to bin
REM %2 is the path to nm
REM %3 is the self file
REM %4 is the stripped self file
REM %5 is the symbol output location

REM Strip the debug stuff from the .self so it's < 2GB
%1 --strip-debug -i %3 -o %4 2>NUL

REM Create symbol data from the stripped .self
%2 --print-size -C %4 1>%5 2>NUL

REM Delete the stripped self
del %4 2>NUL