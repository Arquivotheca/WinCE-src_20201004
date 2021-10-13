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
*snprnc.c - Version of _snprintf with the error return fix. 
*
*   Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*   The _snprintf_c() flavor  returns -1 in case there is no space
*   available for the null terminator & blanks out the buffer
*
*Revision History:
*   08-22-03   SJ   Module created for _snprintf_c
*
*******************************************************************************/

#ifndef _POSIX_

#define _COUNT_ 1
#define _SWPRINTFS_ERROR_RETURN_FIX 1

#include <wchar.h>
#include "sprintf.c"

#endif /* _POSIX_ */
