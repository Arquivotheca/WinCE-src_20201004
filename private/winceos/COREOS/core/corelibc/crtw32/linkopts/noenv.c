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
*noenv.c - stub out CRT's environment string processing
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Stub out the environment string processing normally carried out at 
*       during startup. Note, getenv, _putenv and _environ are not supported
*       if this object is used. Nor is the third argument to main.
*
*Revision History:
*       05-05-97  GJF   Created.
*       03-27-01  PML   _[w]setenvp now returns an int (vs7#231220)
*
*******************************************************************************/

#include <stdlib.h>


#ifdef _M_CEE_PURE
#define _CALLING __clrcall
#else
#define _CALLING __cdecl
#endif

int _CALLING _setenvp(void) { return 0; }
int _CALLING _wsetenvp(void) { return 0; }

#ifndef _WIN32_WCE
void * _CALLING __crtGetEnvironmentStringsA(void) { return NULL; }
void * _CALLING __crtGetEnvironmentStringsW(void) { return NULL; }
#endif // _WIN32_WCE
