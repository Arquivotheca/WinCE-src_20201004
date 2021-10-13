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
*a_str.c - A version of GetStringType.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Use either GetStringTypeA or GetStringTypeW depending on which is
*       unstubbed.
*
*Revision History:
*       09-14-93  CFW   Module created.
*       09-17-93  CFW   Use unsigned chars.
*       09-23-93  CFW   Correct NLS API params and comments about same.
*       10-07-93  CFW   Optimize WideCharToMultiByte, use NULL default char.
*       10-22-93  CFW   Remove bad verification test from "A" version.
*       10-22-93  CFW   Test for invalid MB chars using global preset flag.
*       11-09-93  CFW   Allow user to pass in code page.
*       11-18-93  CFW   Test for entry point function stubs.
*       02-23-94  CFW   Use W flavor whenever possible.
*       03-31-94  CFW   Include awint.h.
*       04-18-94  CFW   Use lcid value if passed in.
*       04-18-94  CFW   Use calloc and don't test the NULL.
*       10-24-94  CFW   Must verify GetStringType return.
*       12-21-94  CFW   Remove invalid MB chars NT 3.1 hack.
*       12-27-94  CFW   Call direct, all OS's have stubs.
*       01-10-95  CFW   Debug CRT allocs.
*       02-15-97  RDK   For narrow string type, try W version first so
*                       Windows NT can process nonANSI codepage correctly.
*       03-16-97  RDK   Added error flag to __crtGetStringTypeA.
*       05-12-97  GJF   Renamed and moved __crtGetStringTypeW into a separate 
*                       file. Revised to use _alloca instead of malloc. Also,
*                       removed some silly code and reformatted.
*       08-18-98  GJF   Use _malloc_crt if _alloca fails.
*       12-10-99  GB    Added support for recovery from stack overflow around 
*                       _alloca().
*       05-17-00  GB    Use ERROR_CALL_NOT_IMPLEMENTED for existance of W API
*       08-23-00  GB    Fixed bug with non Ansi CP on Win9x.
*       11-12-01  GB    Added support for new locale implementation.
*       09-25-04  JL    Replace usage of _alloca() with _alloca_s() / _freea_s()
*       04-04-05  JL    Replace _alloca_s and _freea_s with _malloca and _freea
*       04-01-05  MSL   Integer overflow protection
*
*******************************************************************************/

#include <cruntime.h>
#include <internal.h>
#include <stdlib.h>
#include <locale.h>
#include <awint.h>
#include <dbgint.h>
#include <malloc.h>
#include <awint.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*int __cdecl __crtGetStringTypeA - Get type information about an ANSI string.
*
*Purpose:
*       Internal support function. Assumes info in ANSI string format. Tries
*       to use NLS API call GetStringTypeA if available and uses GetStringTypeW
*       if it must. If neither are available it fails and returns FALSE.
*
*Entry:
*       DWORD    dwInfoType  - see NT\Chicago docs
*       LPCSTR   lpSrcStr    - char (byte) string for which character types 
*                              are requested
*       int      cchSrc      - char (byte) count of lpSrcStr (including NULL 
*                              if any)
*       LPWORD   lpCharType  - word array to receive character type information
*                              (must be twice the size of lpSrcStr)
*       int      code_page   - for MB/WC conversion. If 0, use __lc_codepage
*       BOOL     bError      - TRUE if MB_ERR_INVALID_CHARS set on call to
*                              MultiByteToWideChar when GetStringTypeW used.
*
*Exit:
*       Success: TRUE
*       Failure: FALSE
*
*Exceptions:
*
*******************************************************************************/

static BOOL __cdecl __crtGetStringTypeA_stat(
        _locale_t plocinfo,
        DWORD    dwInfoType,
        LPCSTR   lpSrcStr,
        int      cchSrc,
        LPWORD   lpCharType,
        int      code_page,
        LPWSTR   lpwTmpStr,
        BOOL     bError
        )
{
    int retval1;
    int buff_size;
    BOOL retval2 = FALSE;
    wchar_t *wbuffer = lpwTmpStr;
    _LocaleUpdate _loc_update(plocinfo);

    /*
     * Convert string and return the requested information. Note that
     * we are converting to a wide character string so there is not a
     * one-to-one correspondence between number of multibyte chars in the
     * input string and the number of wide chars in the buffer. However,
     * there had *better be* a one-to-one correspondence between the
     * number of multibyte characters and the number of WORDs in the
     * return buffer.
     */

    /*
     * Use __lc_codepage for conversion if code_page not specified
     */

    if (0 == code_page)
        code_page = plocinfo->locinfo->lc_codepage;

    /* find out how big a buffer we need */
    if ( 0 == (buff_size = MultiByteToWideChar( code_page,
                                                bError ?
                                                    MB_PRECOMPOSED |
                                                    MB_ERR_INVALID_CHARS
                                                    : MB_PRECOMPOSED,
                                                lpSrcStr,
                                                cchSrc,
                                                NULL,
                                                0 )) )
        return FALSE;
    if (wbuffer == NULL) {
#ifndef _WCE_BOOTCRT
        /* allocate enough space for wide chars */
        wbuffer = (wchar_t *)_calloca( sizeof(wchar_t), buff_size );
        if ( wbuffer == NULL ) {
            return FALSE;
        }
#else
        /* BOOTCRT cannot assume the presence of malloc/free */
        return FALSE;
#endif
    }
    (void)memset( wbuffer, 0, sizeof(wchar_t) * buff_size );

    /* do the conversion */
    if ( 0 != (retval1 = MultiByteToWideChar( code_page,
                                             MB_PRECOMPOSED,
                                             lpSrcStr,
                                             cchSrc,
                                             wbuffer,
                                             buff_size )) )
        /* obtain result */
        retval2 = GetStringTypeW( dwInfoType,
                                  wbuffer,
                                  retval1,
                                  lpCharType );
#ifndef _WCE_BOOTCRT
    /* 
     * Need to free wbuffer if it was allocated in 
     * this function.
     * For BOOTCRT, wbuffer is never allocated in this
     * function.
     */
    if (lpwTmpStr == NULL)
        _freea(wbuffer);
#endif

    return retval2;
}

extern "C" BOOL __cdecl __crtGetStringTypeA(
        _locale_t plocinfo,
        DWORD    dwInfoType,
        LPCSTR   lpSrcStr,
        int      cchSrc,
        LPWORD   lpCharType,
        int      code_page,
        BOOL     bError
        )
{

    return __crtGetStringTypeA_stat(
            plocinfo,
            dwInfoType,
            lpSrcStr,
            cchSrc,
            lpCharType,
            code_page,
            NULL,
            bError
            );
}
#ifdef _WCE_BOOTCRT
/*
 * A slight variant of __crtGetStringTypeA which provides
 * the temp buffers so that malloc/free is not needed.
 * Used in BOOTCRT where malloc/free might not exist.
 */
extern "C" BOOL __cdecl __crtGetStringTypeA2(
        _locale_t plocinfo,
        DWORD    dwInfoType,
        LPCSTR   lpSrcStr,
        int      cchSrc,
        LPWORD   lpCharType,
        int      code_page,
        LPWSTR   lpwTmpStr,
        BOOL     bError
        )
{

    return __crtGetStringTypeA_stat(
            plocinfo,
            dwInfoType,
            lpSrcStr,
            cchSrc,
            lpCharType,
            code_page,
            lpwTmpStr,
            bError
            );
}
#endif