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
*a_loc.c - A versions of GetLocaleInfo.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Use either GetLocaleInfoA or GetLocaleInfoW depending on which is 
*       available
*
*Revision History:
*       09-14-93  CFW   Module created.
*       09-17-93  CFW   Use unsigned chars.
*       09-23-93  CFW   Correct NLS API params and comments about same.
*       10-07-93  CFW   Optimize WideCharToMultiByte, use NULL default char.
*       11-09-93  CFW   Allow user to pass in code page.
*       11-18-93  CFW   Test for entry point function stubs.
*       03-31-94  CFW   Include awint.h.
*       12-27-94  CFW   Call direct, all OS's have stubs.
*       01-10-95  CFW   Debug CRT allocs.
*       02-15-97  RDK   For narrow locale info, try W version first so
*                       Windows NT can process nonANSI codepage correctly.
*       05-16-97  GJF   Moved W version into a separate file and renamed this 
*                       one to a_loc.c. Replaced use of _malloc_crt/_free_crt 
*                       with _alloca. Also, detab-ed and cleaned up the code.
*       08-20-98  GJF   Use _malloc_crt if _alloca fails.
*       04-28-99  GJF   Changed dwFlags arg value to 0 in WideCharToMultiByte
*                       calls to avoid problems with codepage 1258 on NT 5.0.
*       12-10-99  GB    Added support for recovery from stack overflow around 
*                       _alloca().
*       05-17-00  GB    Use ERROR_CALL_NOT_IMPLEMENTED for existance of W API
*       11-12-01  GB    Added support for new locale implementation.
*       09-25-04  JL    Replace usage of _alloca() with _alloca_s() / _freea_s()
*       04-04-05  JL    Replace _alloca_s and _freea_s with _malloca and _freea
*       04-01-05  MSL   Integer overflow protection
*
*******************************************************************************/

#include <cruntime.h>
#include <internal.h>
#include <stdlib.h>
#include <awint.h>
#include <dbgint.h>
#include <malloc.h>
#include <locale.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*int __cdecl __crtGetLocaleInfoA - Get locale info and return it as an ASCII 
*       string
*
*Purpose:
*       Internal support function. Assumes info in ANSI string format. Tries
*       to use NLS API call GetLocaleInfoA if available (Chicago) and uses 
*       GetLocaleInfoA if it must (NT). If neither are available it fails and 
*       returns 0.
*
*Entry:
*       LPCWSTR  LocaleName  - locale context for the comparison.
*       LCTYPE   LCType      - see NT\Chicago docs
*       LPSTR    lpLCData    - pointer to memory to return data
*       int      cchData     - char (byte) count of buffer (including NULL)
*                              (if 0, lpLCData is not referenced, size needed 
*                              is returned)
*       int      code_page   - for MB/WC conversion. If 0, use __lc_codepage
*
*Exit:
*       Success: the number of characters copied (including NULL).
*       Failure: 0
*
*Exceptions:
*
*******************************************************************************/

static int __cdecl __crtGetLocaleInfoA_stat(
        _locale_t plocinfo,
        const wchar_t* LocaleName,
        LCTYPE  LCType,
        LPSTR   lpLCData,
        int     cchData
        )
{
    int retval = 0;
    int code_page;
    int buff_size;
    wchar_t *wbuffer;

    /*
     * Use __lc_codepage for conversion
     */

    code_page = plocinfo->locinfo->lc_codepage;

    /* find out how big buffer needs to be */
    if (0 == (buff_size = __crtGetLocaleInfoEx(LocaleName, LCType, NULL, 0)))
        return 0;

    /* allocate buffer */
    wbuffer = (wchar_t *)_calloca( buff_size, sizeof(wchar_t) );
    if ( wbuffer == NULL ) {
        return 0;
    }

    /* get the info in wide format */
    if (0 == __crtGetLocaleInfoEx(LocaleName, LCType, wbuffer, buff_size))
        goto error_cleanup;

    /* convert from Wide Char to ANSI */
    if (0 == cchData)
    {
        /* convert into local buffer */
        retval = WideCharToMultiByte( code_page,
                                      0,
                                      wbuffer,
                                      -1,
                                      NULL,
                                      0,
                                      NULL,
                                      NULL );
    }
    else {
        /* convert into user buffer */
        retval = WideCharToMultiByte( code_page,
                                      0,
                                      wbuffer,
                                      -1,
                                      lpLCData,
                                      cchData,
                                      NULL,
                                      NULL );
    }

error_cleanup:
    _freea(wbuffer);

    return retval;
}

extern "C" int __cdecl __crtGetLocaleInfoA(
        _locale_t plocinfo,
        const wchar_t*  LocaleName,
        LCTYPE  LCType,
        LPSTR   lpLCData,
        int     cchData
        )
{
    _LocaleUpdate _loc_update(plocinfo);

    return __crtGetLocaleInfoA_stat(
            _loc_update.GetLocaleT(),
            LocaleName,
            LCType,
            lpLCData,
            cchData
            );
}
