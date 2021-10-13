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
*drivemap.c - _getdrives
*
*   Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*   defines _getdrives()
*
*Revision History:
*   08-22-91  BWM   Wrote module.
*   04-06-93  SKS   Replace _CRTAPI* with _cdecl
*   04-20-07  GM    Removed __Detect_msvcrt_and_setup().
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>
#include <direct.h>

#if !defined(_WIN32)
#error ERROR - ONLY WIN32 TARGET SUPPORTED!
#endif

/***
*void _getdrivemap(void) - Get bit map of all available drives
*
*Purpose:
*
*Entry:
*
*Exit:
*   drive map with drive A in bit 0, B in 1, etc.
*
*Exceptions:
*
*******************************************************************************/

unsigned long __cdecl _getdrives()
{
#ifdef _WIN32_WCE
    SetLastError(ERROR_NOT_SUPPORTED);
    return 0;
#else  
    return (GetLogicalDrives());
#endif // _WIN32_WCE
}
