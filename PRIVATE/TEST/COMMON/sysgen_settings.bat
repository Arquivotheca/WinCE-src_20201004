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
@REM 
@REM This file contains the sysgen settings required to 
@REM build the common subtree
@REM
@if "%_ECHOON%" == "" echo off

set SYSGEN_ATL=1
set SYSGEN_COMMCTRL=1
set SYSGEN_CPP_EH_AND_RTTI=1
set SYSGEN_FMTMSG=1
set SYSGEN_TOOLHELP=1
set SYSGEN_WINSOCK=1
