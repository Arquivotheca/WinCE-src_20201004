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
*wcserror.c - Contains the _wcserror C runtime.
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	The _wcserror runtime accepts an error number as input
*	and returns the corresponding error string.
*	(wide char version of strerror)
*
*Revision History:
*   11-30-99  GB    Module Created
*
*
*******************************************************************************/

#ifndef _POSIX_

#ifndef _UNICODE   /* CRT flag */
#define _UNICODE 1
#endif

#ifndef UNICODE	   /* NT flag */
#define UNICODE 1
#endif

#undef _MBCS /* UNICODE not _MBCS */

#include "strerror.c"

#endif /* _POSIX_ */
