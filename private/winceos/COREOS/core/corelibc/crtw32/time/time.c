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
*time.c - get current system time
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _time32() - gets the current system time and converts it to
*       internal (__time32_t) format time.
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
*       03-30-93  GJF   Replaced dtoxtime() reference by __gmtotime_t. Also
*                       purged Cruiser support.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       07-21-93  GJF   Converted from using __gmtotime_t and GetSystemTime,
*                       to using __loctotime_t and GetLocalTime.
*       02-13-95  GJF   Merged in Mac version.
*       09-22-95  GJF   Obtain and use Win32's DST flag.
*       10-24-95  GJF   GetTimeZoneInformation is *EXPENSIVE* on NT. Use a
*                       cache to minimize calls to this API.
*       12-13-95  GJF   Optimization above wasn't working because I had 
*                       switched gmt and gmt_cache (thanks PhilipLu!)
*       10-11-96  GJF   More elaborate test needed to determine if current
*                       time is a DST time.
*       05-20-98  GJF   Get UTC time directly from the system.
*       05-17-99  PML   Remove all Macintosh support.
*       02-03-03  EAN   changed to _time32 as part of 64-bit time_t change 
*                       (VSWhidbey 6851)
*       09-01-06  JKS   return (time_t)-1 on error according to C99 standard
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <time.h>
#include <ctime.h>
#include <internal.h>
#include <windows.h>

/*
 * Number of 100 nanosecond units from 1/1/1601 to 1/1/1970
 */
#define EPOCH_BIAS  116444736000000000i64

/*
 * Union to facilitate converting from FILETIME to unsigned __int64
 */
typedef union {
        unsigned __int64 ft_scalar;
        FILETIME ft_struct;
        } FT;


/***
*__time32_t _time32(timeptr) - Get current system time and convert to a
*       __time32_t value.
*
*Purpose:
*       Gets the current date and time and stores it in internal (__time32_t)
*       format. The time is returned and stored via the pointer passed in
*       timeptr. If timeptr == NULL, the time is only returned, not stored in
*       *timeptr. The internal (__time32_t) format is the number of seconds since
*       00:00:00, Jan 1 1970 (UTC).
*
*       Note: We cannot use GetSystemTime since its return is ambiguous. In
*       Windows NT, in return UTC. In Win32S, probably also Win32C, it
*       returns local time.
*
*Entry:
*       __time32_t *timeptr - pointer to long to store time in.
*
*Exit:
*       returns the current time.
*
*Exceptions:
*
*******************************************************************************/

__time32_t __cdecl _time32 (
        __time32_t *timeptr
        )
{
        __time64_t tim;
        FT nt_time;

        GetSystemTimeAsFileTime( &(nt_time.ft_struct) );

        tim = (__time64_t)((nt_time.ft_scalar - EPOCH_BIAS) / 10000000i64);

        if (tim > (__time64_t)(_MAX__TIME32_T))
                tim = (__time64_t)(-1);

        if (timeptr)
                *timeptr = (__time32_t)(tim);         /* store time if requested */

        return (__time32_t)(tim);
}

#endif  /* _POSIX_ */
