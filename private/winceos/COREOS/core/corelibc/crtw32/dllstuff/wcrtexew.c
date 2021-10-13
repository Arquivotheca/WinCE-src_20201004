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
*wcrtexew.c - Initialization for Windows EXE using CRT DLL, wchar_t version
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This is the actual startup routine for Windows apps using wide
*       characters.  It calls the user's main routine wWinMain() after
*       performing C Run-Time Library initialization.
*
*Revision History:
*       ??-??-??  ???   Module created.
*       09-01-94  SKS   Module commented.
*
*******************************************************************************/

#ifdef  CRTDLL

#define _WINMAIN_
#include "wcrtexe.c"

#endif  /* CRTDLL */
