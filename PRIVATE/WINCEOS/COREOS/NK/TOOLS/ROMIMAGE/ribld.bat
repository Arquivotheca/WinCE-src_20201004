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
@setlocal
@cls

set WINCEDEBUG=retail

@rem pushd %_WINCEROOT%\private\winceos\coreos\nk\lzxcompress
@rem build
@rem popd

@del /s *.obj
@build -c

sd edit %_WINCEROOT%\public\common\oak\Bin\i386\romimage.exe
dir %_FLATRELEASEDIR%\romimage.*
copy %_FLATRELEASEDIR%\romimage.* %_WINCEROOT%\public\common\oak\Bin\i386\
dir %_WINCEROOT%\public\common\oak\Bin\i386\romimage.exe

@if "%USERNAME%"=="Damonac" copy %_FLATRELEASEDIR%\romimage.* \pub\romimage\

@pause

set PBTIMEBOMB=120
@del /s romimage.obj
@del /s pbrtimebomb.obj
@build -c

sd edit %_WINCEROOT%\public\common\oak\Bin\i386\pb\romimage_eval.exe
dir %_FLATRELEASEDIR%\romimage.*
copy %_FLATRELEASEDIR%\romimage.exe %_WINCEROOT%\public\common\oak\Bin\i386\pb\romimage_eval.exe
dir %_WINCEROOT%\public\common\oak\Bin\i386\pb\romimage_eval.exe

@pause

set PBTIMEBOMB=180
@del /s romimage.obj
@del /s pbrtimebomb.obj
@build -c

sd edit %_WINCEROOT%\public\common\oak\Bin\i386\pb\romimage.exe
dir %_FLATRELEASEDIR%\romimage.*
copy %_FLATRELEASEDIR%\romimage.* %_WINCEROOT%\public\common\oak\Bin\i386\pb\
dir %_WINCEROOT%\public\common\oak\Bin\i386\pb\romimage.exe





