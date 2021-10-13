//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/***
*imp_data.c - declares all exported data (variables) forwarded by MSVCRTXX.DLL
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	This file declares all data variables exported (forwarded) by the
*	CRTL DLL MSVCRTXX.DLL so that the linker will correctly decorate
*	the names of those variables.
*
*Revision History:
*	05-14-96  SKS	Initial version
*	03-17-97  RDK	Added _mbcasemap.
*	01-27-04  SJ	Remove _fileinfo variable
*	04-28-04  AC	Readd _fileinfo variable, will remove in the next LKG.
*	07-30-04  AJS   Reremove _fileinfo variable for LKG9.
*	11-08-06  PMB   Remove support for Win9x and _osplatform and relatives.
*                       DDB#11325
*
*******************************************************************************/

void * _HUGE;
void * __argc;
void * __argv;
void * __badioinfo;
void * __initenv;
void * __mb_cur_max;
void * __pioinfo;
void * __wargv;
void * __winitenv;
void * _acmdln;
void * _crtAssertBusy;
void * _crtBreakAlloc;
void * _crtDbgFlag;
void * _daylight;
void * _dstbias;
void * _environ;
void * _mbctype;
void * _mbcasemap;
void * _pctype;
void * _pgmptr;
void * _pwctype;
void * _sys_nerr;
void * _timezone;
void * _wcmdln;
void * _wenviron;
void * _wpgmptr;
