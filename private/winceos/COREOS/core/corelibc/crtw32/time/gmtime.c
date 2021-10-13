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
*gmtime.c - breaks down a time value into GMT date/time info
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _gmtime32() - breaks the clock value down into GMT time/date
*       information; returns pointer to structure with the data.
*
*Revision History:
*       01-??-84  RLB   Module created
*       05-??-84  DCW   Split off from rest off ctime routines.
*       02-18-87  JCR   For MS C, gmtime now returns NULL for out of range
*                       time/date.      (This is for ANSI compatibility.)
*       04-10-87  JCR   Changed long declaration to time_t and added const
*       05-21-87  SKS   Declare "struct tm tb" as NEAR data
*       11-10-87  SKS   Removed IBMC20 switch
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-24-88  PHG   Merge DLL and regular versions
*       06-06-89  JCR   386 mthread support
*       11-06-89  KRS   Add (unsigned) to handle years 2040-2099 correctly
*       03-20-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h>, removed #include <register.h> and
*                       fixed the copyright. Also, cleaned up the formatting
*                       a bit.
*       10-04-90  GJF   New-style function declarator.
*       07-17-91  GJF   Multi-thread support for Win32 [_WIN32_].
*       02-17-93  GJF   Changed for new _getptd().
*       03-24-93  GJF   Propagated changes from 16-bit tree.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       01-10-95  CFW   Debug CRT allocs.
*       02-07-98  GJF   Changes for Win64: replaced long with time_t.
*       10-19-01  BWT   Return NULL on malloc failure in MT case instead of 
*                       using static buffer.
*       12-11-01  BWT   return null if getptd fails instead of exiting program
*       02-03-03  EAN   changed to _gmtime32 as part of 64-bit time_t change 
*                       (VSWhidbey 6851)
*       03-06-03  SSM   Assertions, validations and secure version
*       09-07-06  WL    NULL initialize ptb in __getgmtimebuf
*                       (DEVDIV 20323)
*       09-27-06  JKS   Modify Assertions to allow GMT and UTC adjustment
*
*******************************************************************************/

#include <cruntime.h>
#include <time.h>
#include <ctime.h>
#include <stddef.h>
#include <memory.h>
#include <internal.h>
#include <mtdll.h>
#include <malloc.h>
#include <stddef.h>
#include <dbgint.h>

/***
*errno_t _gmtime32_s(ptm, timp) - convert *timp to a structure (UTC)
*
*Purpose:
*       Converts the calendar time value, in 32 bit internal format, to
*       broken-down time (tm structure) with the corresponding UTC time.
*
*Entry:
*       const time_t *timp - pointer to time_t value to convert
*
*Exit:
*       errno_t = 0 success
*                 tm members filled-in
*       errno_t = non zero
*                 tm members initialized to -1 if ptm != NULL
*
*Exceptions:
*
*******************************************************************************/

errno_t __cdecl _gmtime32_s (
        struct tm *ptm,        
        const __time32_t *timp
        )
{
        __time32_t caltim;/* = *timp; *//* calendar time to convert */
        int islpyr = 0;                 /* is-current-year-a-leap-year flag */
        int tmptim;
        int *mdays;                /* pointer to days or lpdays */
        struct tm *ptb = ptm;

        _VALIDATE_RETURN_ERRCODE( ( ptm != NULL ), EINVAL )
        memset( ptm, 0xff, sizeof( struct tm ) );

        _VALIDATE_RETURN_ERRCODE( ( timp != NULL ), EINVAL )

        caltim = *timp;
        _VALIDATE_RETURN_ERRCODE_NOEXC( ( caltim >=  _MIN_LOCAL_TIME ), EINVAL )

        /*
         * Determine years since 1970. First, identify the four-year interval
         * since this makes handling leap-years easy (note that 2000 IS a
         * leap year and 2100 is out-of-range).
         */
        tmptim = (int)(caltim / _FOUR_YEAR_SEC);
        caltim -= ((__time32_t)tmptim * _FOUR_YEAR_SEC);

        /*
         * Determine which year of the interval
         */
        tmptim = (tmptim * 4) + 70;         /* 1970, 1974, 1978,...,etc. */

        if ( caltim >= _YEAR_SEC ) {

            tmptim++;                       /* 1971, 1975, 1979,...,etc. */
            caltim -= _YEAR_SEC;

            if ( caltim >= _YEAR_SEC ) {

                tmptim++;                   /* 1972, 1976, 1980,...,etc. */
                caltim -= _YEAR_SEC;

                /*
                 * Note, it takes 366 days-worth of seconds to get past a leap
                 * year.
                 */
                if ( caltim >= (_YEAR_SEC + _DAY_SEC) ) {

                        tmptim++;           /* 1973, 1977, 1981,...,etc. */
                        caltim -= (_YEAR_SEC + _DAY_SEC);
                }
                else {
                        /*
                         * In a leap year after all, set the flag.
                         */
                        islpyr++;
                }
            }
        }

        /*
         * tmptim now holds the value for tm_year. caltim now holds the
         * number of elapsed seconds since the beginning of that year.
         */
        ptb->tm_year = tmptim;

        /*
         * Determine days since January 1 (0 - 365). This is the tm_yday value.
         * Leave caltim with number of elapsed seconds in that day.
         */
        ptb->tm_yday = (int)(caltim / _DAY_SEC);
        caltim -= (__time32_t)(ptb->tm_yday) * _DAY_SEC;

        /*
         * Determine months since January (0 - 11) and day of month (1 - 31)
         */
        if ( islpyr )
            mdays = _lpdays;
        else
            mdays = _days;


        for ( tmptim = 1 ; mdays[tmptim] < ptb->tm_yday ; tmptim++ ) ;

        ptb->tm_mon = --tmptim;

        ptb->tm_mday = ptb->tm_yday - mdays[tmptim];

        /*
         * Determine days since Sunday (0 - 6)
         */
        ptb->tm_wday = ((int)(*timp / _DAY_SEC) + _BASE_DOW) % 7;

        /*
         *  Determine hours since midnight (0 - 23), minutes after the hour
         *  (0 - 59), and seconds after the minute (0 - 59).
         */
        ptb->tm_hour = (int)(caltim / 3600);
        caltim -= (__time32_t)ptb->tm_hour * 3600L;

        ptb->tm_min = (int)(caltim / 60);
        ptb->tm_sec = (int)(caltim - (ptb->tm_min) * 60);

        ptb->tm_isdst = 0;
        return 0;

}


/***
*struct tm *__getgmtimebuf() - get the static/allocated buffer used by gmtime
*
*Purpose:
*       get the buffer used by gmtime
*
*Entry:
*
*Exit:
*       returns pointer to tm structure or NULL when allocation fails
*
*Exceptions:
*
*******************************************************************************/
struct tm * __cdecl __getgmtimebuf ()
{
        struct tm *ptb = NULL;            /* will point to gmtime buffer */
        _ptiddata ptd = _getptd_noexit();
        if (!ptd) {
            errno = ENOMEM;
            return (NULL);
        }

        /* Use per thread buffer area (malloc space, if necessary) */

        if ( (ptd->_gmtimebuf != NULL) || ((ptd->_gmtimebuf =
            _malloc_crt(sizeof(struct tm))) != NULL) )
                ptb = ptd->_gmtimebuf;
        else
        {
            errno = ENOMEM;
            return NULL;
        }
        return ptb;
}

/***
*struct tm *_gmtime32(timp) - convert *timp to a structure (UTC)
*
*Purpose:
*       Converts the calendar time value, in 32-bit internal format, to
*       broken-down time (tm structure) with the corresponding UTC time.
*
*Entry:
*       const __time32_t *timp - pointer to time_t value to convert
*
*Exit:
*       returns pointer to filled-in tm structure.
*       returns NULL if *timp < 0L
*
*Exceptions:
*
*******************************************************************************/

struct tm * __cdecl _gmtime32 (
        const __time32_t *timp
        )
{
        errno_t e;
        struct tm *ptm = __getgmtimebuf();                 /* will point to gmtime buffer */
        if ( ptm == NULL )
        {
            return NULL;
        }

        e = _gmtime32_s( ptm, timp );
        if ( e != 0 )
        {
            return NULL;
        }
        return ptm;
}
