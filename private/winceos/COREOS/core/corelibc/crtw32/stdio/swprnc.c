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
*swprnc.c - Non standard version of swprintf
*
*   Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*   The _swprintf_c() flavor does take a count argument & also 
*   returns the correct error value, in case there is no space
*   available for the null terminator
*
*Revision History:
*   11-01-02   SJ   Module created for _swprintf_c
*
*******************************************************************************/

#ifndef _POSIX_

#define _COUNT_ 1
#define _SWPRINTFS_ERROR_RETURN_FIX 1

#include <wchar.h>
#include "swprintf.c"

#endif /* _POSIX_ */
