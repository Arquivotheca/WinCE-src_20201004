@REM
@REM Copyright (c) Microsoft Corporation.  All rights reserved.
@REM
@REM
@REM Use of this sample source code is subject to the terms of the Microsoft
@REM license agreement under which you licensed this sample source code. If
@REM you did not accept the terms of the license agreement, you are not
@REM authorized to use this sample source code. For the terms of the license,
@REM please see the license agreement between you and Microsoft or, if applicable,
@REM see the LICENSE.RTF on your install media or the root of your tools installation.
@REM THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
@REM
@echo off

REM Build this common control skin
call build -c

REM Settins for phcommon skin's replacement
set REPLACE_ROOT=C:\Yamazaki\public\voip
set PHCOMMON_SKIN=phcommon_skin

REM Link the new phcommon.dll
call sysgen -p voip phcommon

REM Clear replace settings
set REPLACE_ROOT=
set PHCOMMON_SKIN=
