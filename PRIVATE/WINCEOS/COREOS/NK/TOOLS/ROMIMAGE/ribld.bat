@REM
@REM Copyright (c) Microsoft Corporation.  All rights reserved.
@REM
@REM
@REM This source code is licensed under Microsoft Shared Source License
@REM Version 1.0 for Windows CE.
@REM For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
@REM
@setlocal
@cls

set WINCEDEBUG=retail

@rem pushd %_WINCEROOT%\private\winceos\coreos\nk\lzxcompress
@rem build
@rem popd

@del /s *.obj
@prefast build -c

sd edit %_WINCEROOT%\public\common\oak\Bin\i386\romimage.exe
dir %_FLATRELEASEDIR%\romimage.*
copy %_FLATRELEASEDIR%\romimage.* %_WINCEROOT%\public\common\oak\Bin\i386\
dir %_WINCEROOT%\public\common\oak\Bin\i386\romimage.exe

@if "%USERNAME%"=="damonac" copy %_FLATRELEASEDIR%\romimage.* f:\pub\romimage\

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





