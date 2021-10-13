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
@REM

@if "%_ECHOON%" == "" echo off

set CMD1=%1
set CMD2=%2
set CMD3=%3
set CMD4=%4
set CMD5=%5

if "%_TGTCPU%" == "NT" call :buildostesttree

pushd %_PRIVATEROOT%\Test
for /f %%f in (%_PRIVATEROOT%\Test\testbuild.dirs) do if EXIST %%f call :buildtesttree %%f
goto :EOF


:buildtesttree

if "%1"=="PlatformBuilderTests" (if EXIST %1 (call %_WINCEROOT%\private\test\PlatformBuilderTests\BuildScripts\CopyPBDrop.bat))

pushd %_PRIVATEROOT%\test\%1
build.exe %CMD1% %CMD2% %CMD3% %CMD4% %CMD5%
popd

goto :eof

:buildostesttree

if not EXIST %_WINCEROOT%\private\ostest goto :eof

md %_PROJECTOAKROOT%\oak\lib\%_TGTCPU%\%_WINCEDEBUG%
md %_PROJECTOAKROOT%\oak\target\%_TGTCPU%\%_WINCEDEBUG%

pushd %_PRIVATEROOT%\ostest
build.exe %CMD1% %CMD2% %CMD3% %CMD4% %CMD5%
popd

goto :eof

:EOF
