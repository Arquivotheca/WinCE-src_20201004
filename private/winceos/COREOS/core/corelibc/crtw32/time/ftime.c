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
*ftime.c - return system time
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Returns the system date/time in a structure form.
*
*Revision History:
*       03-??-84  RLB   initial version
*       05-17-86  SKS   ported to OS/2
*       03-09-87  SKS   correct Daylight Savings Time flag
*       11-18-87  SKS   Change tzset() to __tzset()
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       10-03-88  JCR   386: Change DOS calls to SYS calls
*       10-04-88  JCR   386: Removed 'far' keyword
*       10-10-88  GJF   Made API names match DOSCALLS.H
*       04-12-89  JCR   New syscall interface
*       05-25-89  JCR   386 OS/2 calls use '_syscall' calling convention
*       03-20-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h>, removed some leftover 16-bit support
*                       and fixed the copyright. Also, cleaned up the
*                       formatting a bit.
*       07-25-90  SBM   Removed '32' from API names
*       08-13-90  SBM   Compiles cleanly with -W3
*       08-20-90  SBM   Removed old incorrect, redundant tp->dstflag assignment
*       10-04-90  GJF   New-style function declarator.
*       12-04-90  SRW   Changed to include <oscalls.h> instead of <doscalls.h>
*       12-06-90  SRW   Added _CRUISER_ and _WIN32 conditionals.
*       01-21-91  GJF   ANSI naming.
*       01-23-92  GJF   Change in time zone field name for Win32, to support
*                       crtdll.dll [_WIN32_].
*       03-30-93  GJF   Revised to use mktime(). Also purged Cruiser support.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*                       Restore correct spelling of "timezone" struct member.
*       07-15-93  GJF   Call __tzset() instead of _tzset().
*       02-10-95  GJF   Appended Mac version of source file (somewhat cleaned
*                       up), with appropriate #ifdef-s.
*       01-15-98  GJF   Complete rewrite of _ftime to eliminate incorrect dst
*                       status during the fall-back hour.
*       05-17-99  PML   Remove all Macintosh support.
*       10-27-99  GB    Remove #inlcude <dostypes.h>
*       02-03-03  EAN   changed to _ftime32 as part of 64-bit time_t change 
*                       (VSWhidbey 6851)
*       03-06-03  SSM   Assertions and validations
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-20-03  SJ    errno shouldn't be set to 0 on success
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <time.h>
#include <msdos.h>
#include <dos.h>
#include <stdlib.h>
#include <windows.h>
#include <internal.h>

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

/*
 * Cache for the minutes count for with DST status was last assessed
 */
static __time32_t elapsed_minutes_cache = 0;

/*
 * Three values of dstflag_cache
 */
#define DAYLIGHT_TIME   1
#define STANDARD_TIME   0
#define UNKNOWN_TIME    -1

/*
 * Cache for the last determined DST status
 */
static int dstflag_cache = UNKNOWN_TIME;

/***
*void _ftime32(timeptr) - return DOS time in a structure
*
*Purpose:
*       returns the current DOS time in a struct timeb structure
*
*Entry:
*       struct __timeb32 *timeptr - structure to fill in with time
*
*Exit:
*       no return value -- fills in structure
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP errno_t __cdecl _ftime32_s (
        struct __timeb32 *tp
        )
{
        FT nt_time;
        __time32_t t;
        TIME_ZONE_INFORMATION tzinfo;
        DWORD tzstate;
        long timezone = 0;
        
        _VALIDATE_RETURN_ERRCODE( ( tp != NULL ), EINVAL );

        __tzset();

        _ERRCHECK(_get_timezone(&timezone));
        tp->timezone = (short)(timezone / 60);

        GetSystemTimeAsFileTime( &(nt_time.ft_struct) );

        /*
         * Obtain the current DST status. Note the status is cached and only
         * updated once per minute, if necessary.
         */
        if ( (t = (__time32_t)(nt_time.ft_scalar / 600000000i64))
             != elapsed_minutes_cache )
        {
            if ( (tzstate = GetTimeZoneInformation( &tzinfo )) != 0xFFFFFFFF ) 
            {
                /*
                 * Must be very careful in determining whether or not DST is
                 * really in effect.
                 */
                if ( (tzstate == TIME_ZONE_ID_DAYLIGHT) &&
                     (tzinfo.DaylightDate.wMonth != 0) &&
                     (tzinfo.DaylightBias != 0) )
                    dstflag_cache = DAYLIGHT_TIME;
                else
                    /*
                     * When in doubt, assume standard time
                     */
                    dstflag_cache = STANDARD_TIME;
            }
            else
                dstflag_cache = UNKNOWN_TIME;

            elapsed_minutes_cache = t;
        }

        tp->dstflag = (short)dstflag_cache;

        tp->millitm = (unsigned short)((nt_time.ft_scalar / 10000i64) % 
                      1000i64);

        tp->time = (__time32_t)((nt_time.ft_scalar - EPOCH_BIAS) / 10000000i64);
        
        return 0;
}

_CRTIMP void __cdecl _ftime32 (
        struct __timeb32 *tp
        )
{
    _ftime32_s(tp);
}

#endif  /* _POSIX_ */
