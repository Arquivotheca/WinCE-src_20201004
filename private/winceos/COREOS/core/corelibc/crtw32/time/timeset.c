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
*timeset.c - contains defaults for timezone setting
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Contains the timezone values for default timezone.
*       Also contains month and day name three letter abbreviations.
*
*Revision History:
*       12-03-86  JMB   added Japanese defaults and module header
*       09-22-87  SKS   fixed declarations, include <time.h>
*       02-21-88  SKS   Clean up ifdef-ing, change IBMC20 to IBMC2
*       07-05-89  PHG   Remove _NEAR_ for 386
*       08-15-89  GJF   Fixed copyright, indents. Got rid of _NEAR.
*       03-20-90  GJF   Added #include <cruntime.h> and fixed the copyright.
*       05-18-90  GJF   Added _VARTYPE1 to publics to match declaration in
*                       time.h (picky 6.0 front-end!).
*       01-21-91  GJF   ANSI naming.
*       08-10-92  PBS   Posix support(TZ stuff).
*       04-06-93  SKS   Remove _VARTYPE1
*       06-08-93  KRS   Tie JAPAN switch to _KANJI switch.
*       09-13-93  GJF   Merged NT SDK and Cuda versions.
*       04-08-94  GJF   Made non-POSIX definitions of _timezone, _daylight
*                       and _tzname conditional on ndef DLL_FOR_WIN32S.
*       10-04-94  CFW   Removed #ifdef _KANJI
*       02-13-95  GJF   Fixed def of tzname[] so that string constants would
*                       not get overwritten (dangerous if string-pooling is
*                       turned on in build). Made it big enough to hold
*                       timezone names which work with Posix. Also, made
*                       __mnames[] and __dnames[] const.
*       03-01-95  BWT   Fix POSIX by including limits.h
*       08-30-95  GJF   Added _dstbias for Win32. Also increased limit on
*                       timezone strings to 63.
*       05-13-99  PML   Remove Win32s
*       04-07-04  MSL   Added accessors
*       04-17-04  AC    Added accessors for pure mode
*                       VSW#216331
*       04-19-04  AC    Changed _get_tzname to copy the string instead of returning a pointer
*                       to the internal buffers.
*                       VSW#287265
*
*******************************************************************************/

#include <cruntime.h>
#include <stdlib.h>
#include <time.h>
#include <internal.h>

#undef _daylight
#undef _dstbias
#undef _timezone
#undef _tzname

#ifndef _POSIX_

long _timezone = 8 * 3600L; /* Pacific Time Zone */
int _daylight = 1;          /* Daylight Saving Time (DST) in timezone */
long _dstbias = -3600L;     /* DST offset in seconds */

/* note that NT Posix's TZNAME_MAX is only 10 */

static char tzstd[_TZ_STRINGS_SIZE] = { "PST" };
static char tzdst[_TZ_STRINGS_SIZE] = { "PDT" };

char *_tzname[2] = { tzstd, tzdst };

_CRTIMP errno_t __cdecl _get_daylight(int * _Daylight)
{
    _VALIDATE_RETURN_ERRCODE((_Daylight != NULL), EINVAL);

    /* This variable is correctly inited at startup, so no need to check if CRT init finished */
    *_Daylight = _daylight;
    return 0;
}

_CRTIMP errno_t __cdecl _get_dstbias(long * _Daylight_savings_bias)
{
    _VALIDATE_RETURN_ERRCODE((_Daylight_savings_bias != NULL), EINVAL);

    /* This variable is correctly inited at startup, so no need to check if CRT init finished */
    *_Daylight_savings_bias = _dstbias;
    return 0;
}

_CRTIMP errno_t __cdecl _get_timezone(long * _Timezone)
{
    _VALIDATE_RETURN_ERRCODE((_Timezone != NULL), EINVAL);

    /* This variable is correctly inited at startup, so no need to check if CRT init finished */
    *_Timezone = _timezone;
    return 0;
}

_CRTIMP errno_t __cdecl _get_tzname(size_t *_ReturnValue, char *_Buffer, size_t _SizeInBytes, int _Index)
{
    _VALIDATE_RETURN_ERRCODE((_Buffer != NULL && _SizeInBytes > 0) || (_Buffer == NULL && _SizeInBytes == 0), EINVAL);
    if (_Buffer != NULL)
    {
        _Buffer[0] = '\0';
    }
    _VALIDATE_RETURN_ERRCODE(_ReturnValue != NULL, EINVAL);
    _VALIDATE_RETURN_ERRCODE(_Index == 0 || _Index == 1, EINVAL);

    /* _tzname is correctly inited at startup, so no need to check if CRT init finished */
    *_ReturnValue = strlen(_tzname[_Index]) + 1;
    if (_Buffer == NULL)
    {
        /* the user is interested only in the size of the buffer */
        return 0;
    }
    if (*_ReturnValue > _SizeInBytes)
    {
        return ERANGE;
    }
    return strcpy_s(_Buffer, _SizeInBytes, _tzname[_Index]);
}


#else   /* _POSIX_ */

#include <limits.h>

long _timezone = 8*3600L;   /* Pacific Time */
int _daylight = 1;          /* Daylight Savings Time */
                            /* when appropriate */

static char tzstd[TZNAME_MAX + 1] = { "PST" };
static char tzdst[TZNAME_MAX + 1] = { "PDT" };

char *tzname[2] = { tzstd, tzdst };

char *_rule;
long _dstoffset = 3600L;

#endif  /* _POSIX_ */

/*  Day names must be Three character abbreviations strung together */

const char __dnames[] = {
        "SunMonTueWedThuFriSat"
};

/*  Month names must be Three character abbreviations strung together */

const char __mnames[] = {
        "JanFebMarAprMayJunJulAugSepOctNovDec"
};

/***
*int * __daylight()                                 - return pointer to _daylight
*long * __dstbias()                                 - return pointer to _dstbias
*long * __timezone()                                - return pointer to __timezone
*char ** __tzname()                                 - return _tzname
*
*Purpose:
*       Returns former global variables 
*
*Entry:
*       None.
*
*Exit:
*       See above.
*
*Exceptions:
*
*******************************************************************************/
int * __cdecl __daylight(void)
{
    return &(_daylight);
}

long * __cdecl __dstbias(void)
{
    return &(_dstbias);
}

long * __cdecl __timezone(void)
{
    return &(_timezone);
}

char ** __cdecl __tzname(void)
{
    return (_tzname);
}
