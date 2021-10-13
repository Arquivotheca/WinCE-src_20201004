@REM
@REM Copyright (c) Microsoft Corporation.  All rights reserved.
@REM
@REM
@REM Use of this source code is subject to the terms of the Microsoft shared
@REM source or premium shared source license agreement under which you licensed
@REM this source code. If you did not accept the terms of the license agreement,
@REM you are not authorized to use this source code. For the terms of the license,
@REM please see the license agreement between you and Microsoft or, if applicable,
@REM see the SOURCE.RTF on your install media or the root of your tools installation.
@REM THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
@REM
@if "%_ECHOON%" == "" echo off

if "%_TESTCALLED%"=="1" goto reinvoked
set _TESTCALLED=1

REM Test code doesn't build multiproc right now
set BUILD_MULTIPROCESSOR=1

REM set the list of directories to build
for /f %%f in (%_PRIVATEROOT%\Test\testbuild.dirs) do call :AddTestDir %%f

REM remove any existing sysgen variables
if exist "%_PUBLICROOT%\common\oak\misc\clearsysgenvars.bat" call %_PUBLICROOT%\common\oak\misc\clearsysgenvars.bat >nul 2>nul

REM For CE builds, we'll pick up the ostest bits from the buildlab.
REM For NT builds, we also want to build the ostest tree, since this is not built by the buildlab.
if "%_TGTOS%"=="NT" (
	md %_PROJECTOAKROOT%\oak\lib\%_TGTCPU%\%_WINCEDEBUG%
	md %_PROJECTOAKROOT%\oak\target\%_TGTCPU%\%_WINCEDEBUG%

	set _DEPTREES=ostest
	ren %_PUBLICROOT%\ostest\oak\oakver.bat oakver.bat.sav
)

REM set all sysgen settings from each of the top level directories
for %%i in (%DIRS_CETEST%) do if exist %_winceroot%\private\test\%%i\sysgen_settings.bat call %_winceroot%\private\test\%%i\sysgen_settings.bat

REM Always set OSTEST
set SYSGEN_OSTEST=1

REM for the PlatformBuilder Tests we need to copy over their test binaries after they are done building.
REM we also need to copy over the platform BUilder deliverables so the test may link against them.
REM NOTE: setting _COPYPBDROP=1 will overwrite everything in the $(_WINCEROOT)\Tools\Drop directory!!
if NOT "%_COPYPBTESTBINS%"=="1" (set _COPYPBTESTBINS=1)
if NOT "%_COPYPBDROP%"=="1" (set _COPYPBDROP=1)
if "%PBQA_ROOT%"==""  (set PBQA_ROOT=%_PRIVATEROOT%\Test\PlatformBuilderTests)

if "%_NOPBDROP%"=="" (call %PBQA_ROOT%\BuildScripts\CopyPBDrop.bat -clean)

REM Extend dependency tree so that private\test is built by cebuild
for %%i in (%DIRS_CETEST%) do call :AddDepTree test\%%i

goto :EOF

:AddDepTree
    if EXIST %_WINCEROOT%\private\%1 set _DEPTREES=%_DEPTREES% %1
    goto :EOF 

:AddTestDir
    set DIRS_CETEST=%DIRS_CETEST% %1
    goto :EOF

:reinvoked
   echo.
   echo.
   echo ERROR: test_config.bat has already been run.  Exit this window and start a new
   echo command shell to invoke with new parameters.
   echo.
   echo Current Environment was invoked via "wince %_TGTCPUFAMILY%%_TGTCPUISA%%_TGTCPUOPTIONS% %_TGTPROJ% %_TGTPLAT%"
   echo.
   echo.
