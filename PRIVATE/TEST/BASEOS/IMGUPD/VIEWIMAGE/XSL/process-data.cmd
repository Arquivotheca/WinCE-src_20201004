@echo off
rem process-data.cmd

REM
REM Copyright (c) Microsoft Corporation.  All rights reserved.
REM
REM
REM This source code is licensed under Microsoft Shared Source License
REM Version 1.0 for Windows CE.
REM For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
REM

setlocal

set DATA_FILE_DIR=%_FLATRELEASEDIR%
set OD=%_FLATRELEASEDIR%\viewimage-data

if "%1" == "clean" goto CLEAN
goto PROCESS_DATA

:CLEAN

call compare-image-output.cmd clean desktop device %OD%

echo Removing %OD%...
if exist %OD% rmdir %OD%

goto END

:PROCESS_DATA

compare-image-output.cmd desktop %DATA_FILE_DIR%\desktop.xml device %DATA_FILE_DIR%\device.xml %OD%

goto END

:ERRMISSINGFILE

echo *** Couldn't find file: %AFILE%

:END

endlocal
