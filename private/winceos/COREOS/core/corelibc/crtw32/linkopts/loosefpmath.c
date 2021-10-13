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
* loosefpmath.c - Set DAZ and FTZ for SSE2 architectures
*
*   Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*
*Revision History:
*   12-03-03  AC    created file
*   02-11-05  AC    Remove redundant #pragma section
*                   VSW#445138
*
*******************************************************************************/
#include <float.h>
#include <sect_attribs.h>
#include <internal.h>

int __set_loosefpmath(void);

int __set_loosefpmath(void)
{
    _ERRCHECK(_controlfp_s(NULL, _DN_FLUSH, _MCW_DN));
    return 0;
}

_CRTALLOC(".CRT$XID") static _PIFV pinit = __set_loosefpmath;
