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
*ctime.h - constant for dates and times
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Include file used by the c time routines containing definitions of
*       various constants and macros used in determining dates and times.
*
*       [Internal]
*
*Revision History:
*       03-??-84  RLB   written
*       05-??-84  DFW   split out the constant from each routine into this file
*       07-27-89  GJF   Fixed copyright
*       10-30-89  GJF   Fixed copyright (again)
*       02-28-90  GJF   Added #ifndef _INC_CTIME stuff.
*       03-29-93  GJF   Revised constants.
*       02-14-95  CFW   Clean up Mac merge.
*       03-29-95  CFW   Add error message to internal headers.
*       12-14-95  JWM   Add "#pragma once".
*       02-24-97  GJF   Detab-ed.
*       05-07-97  GJF   Added _MAX_YEAR64 and _MAX_TIME64_T constants. Added
*                       _IS_LEAP_YEAR() and _ELAPSED_LEAP_YEAR macros. Took
*                       out unnecessary casts.
*       02-09-98  GJF   Changes for Win64: removed unnecessary typing of 
*                       constants as long, also put in larger value for 
*                       _MAX_YEAR.
*       12-10-99  GB    _MAX_YEAR for Win64 be equal to _MAX_YEAR64 
*       07-09-03  SJ    _MAX__TIME64_T value changed - VSWhidbey#108598
*       09-23-03  SSM   VSWhidbey : 140798 Change _MAX_YEAR64
*                       to be 1100 (actual year 3000) to match MSDN
*       09-01-06  JKS   add _MAX__TIME32_T, _MAX_LOCAL_TIME, _MIN_LOCAL_TIME
*
****/

#pragma once

#ifndef _INC_CTIME
#define _INC_CTIME

#ifndef _CRTBLD
/*
 * This is an internal C runtime header file. It is used when building
 * the C runtimes only. It is not to be used as a public header file.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* _CRTBLD */

/*
 * Constants
 */
#define _DAY_SEC           (24 * 60 * 60)       /* secs in a day */

#define _YEAR_SEC          (365 * _DAY_SEC)     /* secs in a year */

#define _FOUR_YEAR_SEC     (1461 * _DAY_SEC)    /* secs in a 4 year interval */

#define _DEC_SEC           315532800            /* secs in 1970-1979 */

#define _BASE_YEAR         70                   /* 1970 is the base year */

#define _BASE_DOW          4                    /* 01-01-70 was a Thursday */

#define _LEAP_YEAR_ADJUST  17                   /* Leap years 1900 - 1970 */

#ifdef  _WIN64
#define _MAX_YEAR          1100                 /* 3000 is the max year */
#else
#define _MAX_YEAR          138                  /* 2038 is the max year */
#endif

#define _MAX_LOCAL_TIME    (13 * 60 * 60)      /* max local time adjustment
                                                   GMT +13 hour
                                                   DST -0  hour             */

#define _MIN_LOCAL_TIME    (-12 * 60 * 60)      /* min local time adjustment
                                                   GMT -11 hour
                                                   DST -1  hour             */

#define _MAX_YEAR64        1100                 /* 3000 is the max year */

#define _MAX__TIME32_T     0x7fffd27f           /* number of seconds from
                                                   00:00:00, 01/01/1970 UTC to
                                                   23:59:59, 01/18/2038 UTC */

#define _MAX__TIME64_T     0x793406fffi64       /* number of seconds from 
                                                   00:00:00, 01/01/1970 UTC to
                                                   23:59:59. 12/31/3000 UTC */

/*
 * Macro to determine if a given year, expressed as the number of years since
 * 1900, is a leap year.
 */
#define _IS_LEAP_YEAR(y)        (((y % 4 == 0) && (y % 100 != 0)) || \
                                ((y + 1900) % 400 == 0))

/*
 * Number of leap years from 1970 up to, but not including, the specified year
 * (expressed as the number of years since 1900).
 */
#define _ELAPSED_LEAP_YEARS(y)  (((y - 1)/4) - ((y - 1)/100) + ((y + 299)/400) \
                                - _LEAP_YEAR_ADJUST)

#endif  /* _INC_CTIME */
