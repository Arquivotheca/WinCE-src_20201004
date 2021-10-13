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
*drive.c - get and change current drive
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file has the _getdrive() and _chdrive() functions
*
*Revision History:
*       06-06-89  PHG   Module created, based on asm version
*       03-07-90  GJF   Made calling type _CALLTYPE1, added #include
*                       <cruntime.h> and fixed copyright. Also, cleaned up
*                       the formatting a bit.
*       07-24-90  SBM   Removed '32' from API names
*       09-27-90  GJF   New-style function declarators.
*       12-04-90  SRW   Changed to include <oscalls.h> instead of <doscalls.h>
*       12-06-90  SRW   Added _CRUISER_ and _WIN32 conditionals.
*       05-10-91  GJF   Fixed off-by-1 error in Win32 version and updated the
*                       function descriptions a bit [_WIN32_].
*       05-19-92  GJF   Revised to use the 'current directory' environment
*                       variables of Win32/NT.
*       06-09-92  GJF   Use _putenv instead of Win32 API call. Also, defer
*                       adding env var until after the successful call to
*                       change the dir/drive.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       11-24-93  CFW   Rip out Cruiser.
*       11-24-93  CFW   No longer store current drive in CRT env strings.
*       02-08-95  JWM   Spliced _WIN32 & Mac versions.
*       07-01-96  GJF   Replaced defined(_WIN32) with !defined(_MAC). Also,
*                       detab-ed and cleaned up the format a bit.
*       12-17-97  GJF   Exception-safe locking.
*       05-17-99  PML   Remove all Macintosh support.
*       04-26-02  GB    fixed bug in _getdrive if path is greater then
*                       MAX_PATH, i.e. "\\?\" prepended to path.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-18-04  MSL   Fix getdrive validation for network drives
*       04-01-05  MSL   Integer overflow fix during allocation 
*       02-01-07  AC    Small fix to _chdrive validation
*                       DDB#11469
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>
#include <mtdll.h>
#include <internal.h>
#include <msdos.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <dbgint.h>


/***
*int _getdrive() - get current drive (1=A:, 2=B:, etc.)
*
*Purpose:
*       Returns the current disk drive
*
*Entry:
*       No parameters.
*
*Exit:
*       returns 1 for A:, 2 for B:, 3 for C:, etc.
*       returns 0 if current drive cannot be determined.
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _getdrive (
        void
        )
{
#ifdef _WIN32_WCE // No Support in CE
    errno = EPERM;
    return 0;
#else
    ULONG drivenum=0;
    wchar_t curdirstr[_MAX_PATH+1];
    wchar_t *cdirstr = curdirstr;
    int memfree=0,r=0;

    r = GetCurrentDirectoryW(MAX_PATH+1, cdirstr);
    if (r> MAX_PATH) {
        if ((cdirstr= (wchar_t *) _calloc_crt((r+1), sizeof(wchar_t))) == NULL) {
            errno = ENOMEM;
            r = 0;
        } else {
            memfree = 1;
        }

        if (r)
        {
            r = GetCurrentDirectoryW(r+1, cdirstr);
        }
    }

    drivenum = 0;

    if (r)
    {
        if (cdirstr[1] == L':')
        {
            drivenum = __ascii_towupper(cdirstr[0]) - L'A' + 1;
        }
    }
    else
    {
        errno=ENOMEM;
    }

    if (memfree)
    {
        _free_crt(cdirstr);
    }

    return drivenum;
#endif // _WIN32_WCE
}


/***
*int _chdrive(int drive) - set the current drive (1=A:, 2=B:, etc.)
*
*Purpose:
*       Allows the user to change the current disk drive
*
*Entry:
*       drive - the number of drive which should become the current drive
*
*Exit:
*       returns 0 if successful, else -1
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _chdrive (
        int drive
        )
{
#ifdef _WIN32_WCE // No Support in CE
    _dosmaperr(ERROR_NOT_SUPPORTED);
    return -1;
#else
    int retval;
    wchar_t newdrive[3];

    if (drive < 1 || drive > 26) {
        _doserrno = ERROR_INVALID_DRIVE;
        _VALIDATE_RETURN(("Invalid Drive Index",0), EACCES, -1);
    }

    newdrive[0] = (wchar_t) (L'A' + drive - 1);
    newdrive[1] = L':';
    newdrive[2] = L'\0';

    /*
        * Set new drive. If current directory on new drive exists, it
        * will become the cwd. Otherwise defaults to root directory.
        */

    if (SetCurrentDirectoryW(newdrive))
        retval = 0;
    else {
        _dosmaperr(GetLastError());
        retval = -1;
    }

    return retval;
#endif // _WIN32_WCE
}
