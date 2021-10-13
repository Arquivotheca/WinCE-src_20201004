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
*wcstod.c - convert wide char string to floating point number
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Convert character string to floating point number
*
*Revision History:
*       06-15-92  KRS   Created from strtod.c.
*       11-06-92  KRS   Fix bugs in wctomb() loop.
*       04-06-93  SKS   Replace _CRTAPI* with _cdecl
*       02-07-94  CFW   POSIXify.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       01-10-95  CFW   Debug CRT allocs.
*       04-01-96  BWT   POSIX work.
*       02-19-01  GB    added _alloca and Check for return value of _malloc_crt
*       08-19-03  AC    Removed alloca, use the new _wfltin functions, validate input parameters
*       04-07-04  MSL   Changes to support locale-specific strgtold12
*                       VSW 247190
*       05-18-05  PML   Don't give underflow error on denormals (vsw#490868)
*
*******************************************************************************/

#include <cruntime.h>
#include <internal.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <mbctype.h>
#include <errno.h>
#include <math.h>
#include <dbgint.h>
#include <stdlib.h>
#include <malloc.h>
#include <fltintrn.h>
#include <locale.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*double wcstod(nptr, endptr) - convert wide string to double
*
*Purpose:
*       wcstod recognizes an optional string of tabs and spaces,
*       then an optional sign, then a string of digits optionally
*       containing a decimal point, then an optional e or E followed
*       by an optionally signed integer, and converts all this to
*       to a floating point number.  The first unrecognized
*       character ends the string, and is pointed to by endptr.
*
*Entry:
*       nptr - pointer to wide string to convert
*
*Exit:
*       returns value of wide character string
*       wchar_t **endptr - if not NULL, points to character which stopped
*               the scan
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function. 
*
*******************************************************************************/

extern "C" double __cdecl _wcstod_l (
        const wchar_t *nptr,
        wchar_t **endptr,
        _locale_t plocinfo
        )
{

        struct _flt answerstruct;
        FLT      answer;
        double       tmp;
        unsigned int flags;
        wchar_t *ptr = (wchar_t *) nptr;
        _LocaleUpdate _loc_update(plocinfo);

        /* validation section */
        if (endptr != NULL)
        {
            /* store beginning of string in endptr */
            *endptr = (wchar_t *)nptr;
        }
        _VALIDATE_RETURN(nptr != NULL, EINVAL, 0.0);

        /* scan past leading space/tab characters */

        while ( iswspace(*ptr) )
                ptr++;

        /* let _fltin routine do the rest of the work */

        /* ok to take address of stack variable here; fltin2 knows to use ss */
        answer = _wfltin2( &answerstruct, ptr, _loc_update.GetLocaleT());

        if ( endptr != NULL )
                *endptr = (wchar_t *) ptr + answer->nbytes;

        flags = answer->flags;
        if ( flags & (512 | 64)) {
                /* no digits found or invalid format:
                   ANSI says return 0.0, and *endptr = nptr */
                tmp = 0.0;
                if ( endptr != NULL )
                        *endptr = (wchar_t *) nptr;
        }
        else if ( flags & (128 | 1) ) {
                if ( *ptr == '-' )
                        tmp = -HUGE_VAL;        /* negative overflow */
                else
                        tmp = HUGE_VAL;         /* positive overflow */
#ifndef _WCE_BOOTCRT
                errno = ERANGE;
#endif
        }
        else if ( (flags & 256) && answer->dval == 0.0 ) {
                tmp = 0.0;                      /* underflow (denormals OK) */
#ifndef _WCE_BOOTCRT
                errno = ERANGE;
#endif
        }
        else
                tmp = answer->dval;

        return(tmp);
}

extern "C" double __cdecl wcstod (
        const wchar_t *nptr,
        wchar_t **endptr
        )
{
    return _wcstod_l(nptr, endptr, NULL);
}
