@REM
@REM Copyright (c) Microsoft Corporation.  All rights reserved.
@REM
@REM
@REM 
@REM This file contains the sysgen settings required to 
@REM build the gwes subtree
@REM
@if "%_ECHOON%" == "" echo off

rem 
rem This file contains the sysgen settings required to 
rem build this subtree.  This tree requires components
rem in test\common and test\tools in order to build.
rem

set SYSGEN_CPP_EH_AND_RTTI=1
set SYSGEN_PRINTING=1
set SYSGEN_SOFTKB=1
set SYSGEN_STANDARDSHELL=1
set SYSGEN_TOUCHGESTURE=1


set SYSGEN_AYGSHELL=1

set SYSGEN_COMPOSITION=1



