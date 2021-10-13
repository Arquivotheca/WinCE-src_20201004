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
*strtime.c - contains the function "_strtime()" ans "_strtime_s()"
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       contains the function _strtime() and _strtime_s()
*
*Revision History:
*       06-07-89  PHG   Module created, based on asm version
*       03-20-90  GJF   Made calling type _CALLTYPE1, added #include
*                       <cruntime.h> and fixed the copyright. Also, cleaned
*                       up the formatting a bit.
*       07-25-90  SBM   Removed '32' from API names
*       10-04-90  GJF   New-style function declarator.
*       12-04-90  SRW   Changed to include <oscalls.h> instead of <doscalls.h>
*       12-06-90  SRW   Added _CRUISER_ and _WIN32 conditionals.
*       05-19-92  DJM   ifndef for POSIX build.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       11-01-93  CFW   Enable Unicode variant, rip out Cruiser.
*       02-10-95  GJF   Merged in Mac version.
*       05-17-99  PML   Remove all Macintosh support.
*       03-06-03  SSM   Assertions, validations and sercure version
*       02-26-04  SSM   Update validations to use macros and match current specs
*       03-11-04  AC    Return ERANGE when buffer is too small
*       03-29-04  AC    Slightly fixed validation.
*                       VSW#239396
*       04-25-05  AC    Added debug filling
*                       VSW#2459
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <time.h>
#include <tchar.h>
#include <oscalls.h>
#include <internal.h>
#include <internal_securecrt.h>

/***
*errno_t _strtime_s(buffer, sizeInChars) - return time in string form
*
*Purpose:
*       _strtime_s() returns a string containing the time in "HH:MM:SS" form
*
*Entry:
*       _TSCHAR *buffer = the address of a 9-byte user buffer
*       size_t  sizeInChars = size of the buffer in characters.
*                         should include space for the terminating NULL
*                         Should be >= 9
*
*Exit:
*       errno_t = 0 on success
*                 buffer contains the time in "HH:MM:SS" form
*       errno_t = correct error code on failure
*                 buffer empty NULL terminated if it is at least 1 character 
*                 in size.
*
*
*Exceptions:
*
*******************************************************************************/

errno_t __cdecl _tstrtime_s (
        _TSCHAR *buffer,
        size_t sizeInChars
        )
{
        int hours, minutes, seconds;
        SYSTEMTIME dt;                       /* Win32 time structure */
        
        _VALIDATE_RETURN_ERRCODE( ( buffer != NULL && sizeInChars > 0 ), EINVAL )
        _RESET_STRING(buffer, sizeInChars);
        _VALIDATE_RETURN_ERRCODE( ( sizeInChars >= 9 ), ERANGE )

        GetLocalTime(&dt);

        hours = dt.wHour;
        minutes = dt.wMinute;
        seconds = dt.wSecond;

        /* store the components of the time into the string */
        /* store separators */
        buffer[2] = buffer[5] = _T(':');
        /* store end of string */
        buffer[8] = _T('\0');
        /* store tens of hour */
        buffer[0] = (_TSCHAR) (hours   / 10 + _T('0'));
        /* store units of hour */
        buffer[1] = (_TSCHAR) (hours   % 10 + _T('0'));
        /* store tens of minute */
        buffer[3] = (_TSCHAR) (minutes / 10 + _T('0'));
        /* store units of minute */
        buffer[4] = (_TSCHAR) (minutes % 10 + _T('0'));
        /* store tens of second */
        buffer[6] = (_TSCHAR) (seconds / 10 + _T('0'));
        /* store units of second */
        buffer[7] = (_TSCHAR) (seconds % 10 + _T('0'));

        return 0;
}

/***
*_TSCHAR *_strtime(buffer) - return time in string form
*
*Purpose:
*       _strtime() returns a string containing the time in "HH:MM:SS" form
*
*Entry:
*       _TSCHAR *buffer = the address of a 9-byte user buffer
*
*Exit:
*       returns buffer, which contains the time in "HH:MM:SS" form
*
*Exceptions:
*
*******************************************************************************/

_TSCHAR * __cdecl _tstrtime (
        _TSCHAR *buffer
        )
{
    // This function assumes that buffer is 9 characters in size
    errno_t e = _tstrtime_s( buffer, 9 );
    if ( e != 0 )
    {
        return NULL;
    }
    return buffer;
}

#endif  /* _POSIX_ */
