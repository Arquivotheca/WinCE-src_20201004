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
*wfndf64.c - C find file functions (wchar_t version)
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines _wfindfirst64() and _wfindnext64().
*
*Revision History:
*       05-29-98  GJF   Created.
*
*******************************************************************************/

#define WPRFLAG     1

#ifndef _UNICODE    /* CRT flag */
#define _UNICODE    1
#endif

#ifndef UNICODE     /* NT flag */
#define UNICODE     1
#endif

#undef  _MBCS        /* UNICODE not _MBCS */

#define _USE_INT64  1

#include "findf64.c"
