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
*access.c - access function
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file has the _access() function which checks on file accessability.
*
*Revision History:
*       06-06-89  PHG   Module created, based on asm version
*       11-10-89  JCR   Replaced DOS32QUERYFILEMODE with DOS32QUERYPATHINFO
*       03-07-90  GJF   Made calling type _CALLTYPE2 (for now), added #include
*                       <cruntime.h>, fixed copyright and fixed compiler
*                       warnings. Also, cleaned up the formatting a bit.
*       03-30-90  GJF   Now _CALLTYPE1.
*       07-24-90  SBM   Removed '32' from API names
*       09-27-90  GJF   New-style function declarator.
*       12-04-90  SRW   Changed to include <oscalls.h> instead of <doscalls.h>
*       12-06-90  SRW   Added _CRUISER_ and _WIN32 conditionals.
*       01-16-91  GJF   ANSI naming.
*       04-09-91  PNT   Added _MAC_ conditional
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       12-07-93  CFW   Rip out Cruiser, enable wide char.
*       02-08-95  JWM   Spliced _WIN32 & Mac versions.
*       07-01-96  GJF   Replaced defined(_WIN32) with !defined(_MAC). Also,
*                       detab-ed and cleaned up the format a bit.
*       05-17-99  PML   Remove all Macintosh support.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-20-03  SJ    errno shouldn't be set to 0 on success
*
*******************************************************************************/

#include <cruntime.h>
#include <io.h>
#include <oscalls.h>
#include <stdlib.h>
#include <errno.h>
#include <msdos.h>
#include <internal.h>
#include <tchar.h>
#include <malloc.h>
#include <dbgint.h>

/***
*int _access(path, amode) - check whether file can be accessed under mode
*
*Purpose:
*       Checks to see if the specified file exists and can be accessed
*       in the given mode.
*
*Entry:
*       _TSCHAR *path - pathname
*       int amode -     access mode
*                       (0 = exist only, 2 = write, 4 = read, 6 = read/write)
*
*Exit:
*       returns 0 if file has given mode
*       returns -1 and sets errno if file does not have given mode or
*       does not exist
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _taccess (
        const _TSCHAR *path,
        int amode
        )
{
    errno_t e;
    e = _taccess_s(path,amode);

    return e ? -1 : 0 ;
}

/***
*errno_t _access_s(path, amode) - check whether file can be accessed under mode
*
*Purpose:
*       Checks to see if the specified file exists and can be accessed
*       in the given mode.
*
*Entry:
*       _TSCHAR *path - pathname
*       int amode -     access mode
*                       (0 = exist only, 2 = write, 4 = read, 6 = read/write)
*
*Exit:
*       returns 0 if file has given mode
*       returns errno_t for any other errors
*
*Exceptions:
*
*******************************************************************************/
#ifndef _UNICODE

errno_t __cdecl _access_s (
        const char *path,
        int amode
        )
{
    wchar_t* pathw = NULL;
    errno_t retval;

    if (path)
    {
        if (!__copy_path_to_wide_string(path, &pathw))
            return errno;
    }

    /* call the wide-char variant */
    retval = _waccess_s(pathw, amode);

    _free_crt(pathw); /* _free_crt leaves errno alone if everything completes as expected */

    return retval;
}

#else /* _UNICODE */

errno_t __cdecl _waccess_s (
        const wchar_t *path,
        int amode
        )
{
     
        WIN32_FILE_ATTRIBUTE_DATA attr_data;

        _VALIDATE_CLEAR_OSSERR_RETURN_ERRCODE( (path != NULL), EINVAL);
        _VALIDATE_CLEAR_OSSERR_RETURN_ERRCODE( ((amode & (~6)) == 0), EINVAL);
        
        if (!GetFileAttributesExW(path, GetFileExInfoStandard, (void*) &attr_data)) {
                /* error occured -- map error code and return */
                _dosmaperr(GetLastError());
                return errno;
        }

        if(attr_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            /* All directories have read & write access */
            return 0;
        }
        
        /* no error; see if returned premission settings OK */
        if ( (attr_data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) && (amode & 2) ) {
                /* no write permission on file, return error */
                _doserrno = E_access;
                errno = EACCES;
                return errno;
        }
        else
                /* file exists and has requested permission setting */
                return 0;

}

#endif /* _UNICODE */
