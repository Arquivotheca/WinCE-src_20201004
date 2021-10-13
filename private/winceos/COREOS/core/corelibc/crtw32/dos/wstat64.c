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
*wstat64.c - get file status (wchar_t version)
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _wstat64() - get file status
*
*Revision History:
*       06-02-98  GJF   Created.
*
*******************************************************************************/

#ifndef _POSIX_

#define WPRFLAG 1

#ifndef _UNICODE    /* CRT flag */
#define _UNICODE 1
#endif

#ifndef UNICODE     /* NT flag */
#define UNICODE 1
#endif

#undef  _MBCS       /* UNICODE not _MBCS */

#include "stat64.c"

#endif /* _POSIX_ */
