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
*drivfree.c - Get the size of a disk
*
*   Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*   This file has _getdiskfree()
*
*Revision History:
*   08-21-91  PHG   Module created for Win32
*   10-24-91  GJF   Added LPDWORD casts to make MIPS compiler happy.
*           ASSUMES THAT sizeof(unsigned) == sizeof(DWORD).
*   04-06-93  SKS   Replace _CRTAPI* with _cdecl
*   01-27-95  GJF   Removed explicit handling for case where the default
*           drive is specified and the current directory is a
*           UNC path Also, cleaned up a bit.
*   01-31-95  GJF   Further cleanup, as suggested by Richard Shupak.
*   09-11-03  SJ    Secure CRT Work - Assertions & Validations
*
*******************************************************************************/

#include <cruntime.h>
#include <direct.h>
#include <oscalls.h>
#include <internal.h>

/***
*int _getdiskfree(drivenum, diskfree)  - get size of a specified disk
*
*Purpose:
*   Gets the size of the current or specified disk drive
*
*Entry:
*   int drivenum - 0 for current drive, or drive 1-26
*
*Exit:
*   returns 0 if succeeds
*   returns system error code on error.
*
*Exceptions:
*
*******************************************************************************/

#ifndef _WIN32
#error ERROR - ONLY WIN32 TARGET SUPPORTED!
#endif

unsigned __cdecl _getdiskfree (
    unsigned uDrive,
    struct _diskfree_t * pdf
)
{

#ifdef _WIN32_WCE
    // CE only support AFS_GetDiskFreeSpace for kmode
    return (ERROR_NOT_SUPPORTED);
#else
    char   Root[4];
    char * pRoot;

    _VALIDATE_RETURN((pdf != NULL), EINVAL, ERROR_INVALID_PARAMETER);
    _VALIDATE_RETURN(( uDrive <= 26 ), EINVAL, ERROR_INVALID_PARAMETER);
    memset( pdf, 0, sizeof( struct _diskfree_t ) );
    
    if ( uDrive == 0 ) {
        pRoot = NULL;
    }
    else {
        pRoot = &Root[0];
        Root[0] = (char)uDrive + (char)('A' - 1);
        Root[1] = ':';
        Root[2] = '\\';
        Root[3] = '\0';
    }


    if ( !GetDiskFreeSpace( pRoot,
        (LPDWORD)&(pdf->sectors_per_cluster),
        (LPDWORD)&(pdf->bytes_per_sector),
        (LPDWORD)&(pdf->avail_clusters),
        (LPDWORD)&(pdf->total_clusters)) )
    {
        int err = GetLastError();
        errno = _get_errno_from_oserr(err);

        return ( err );
    }

    return ( 0 );
#endif // _WIN32_WCE
}
