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
*a_map.c - A version of LCMapString.
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Use either LCMapStringA or LCMapStringW depending on which is available
*
*Revision History:
*       09-14-93  CFW   Module created.
*       09-17-93  CFW   Use unsigned chars.
*       09-23-93  CFW   Correct NLS API params and comments about same.
*       10-07-93  CFW   Optimize WideCharToMultiByte, use NULL default char.
*       10-22-93  CFW   Test for invalid MB chars using global preset flag.
*       11-09-93  CFW   Allow user to pass in code page.
*       11-18-93  CFW   Test for entry point function stubs.
*       02-23-94  CFW   Use W flavor whenever possible.
*       03-31-94  CFW   Include awint.h.
*       07-26-94  CFW   Update #14730, LCMapString goes past NULLs.
*       12-21-94  CFW   Remove invalid MB chars NT 3.1 hack.
*       12-27-94  CFW   Call direct, all OS's have stubs.
*       01-10-95  CFW   Debug CRT allocs.
*       02-15-97  RDK   For narrow mapping, try W version first so Windows NT
*                       can process nonANSI codepage correctly.
*       03-16-97  RDK   Added error flag to __crtLCMapStringA.
*       05-09-97  GJF   Renamed and moved __crtLCMapStringW into a separate 
*                       file. Revised to use _alloca instead of malloc. Also,
*                       reformatted.
*       05-27-98  GJF   Changed strncnt() so that it will never examine the
*                       (cnt + 1)-th byte of the string.
*       08-18-98  GJF   Use _malloc_crt if _alloca fails.
*       04-28-99  GJF   Changed dwFlags arg value to 0 in WideCharToMultiByte
*                       calls to avoid problems with codepage 1258 on NT 5.0.
*       12-10-99  GB    Added support for recovery from stack overflow around 
*                       _alloca().
*       05-17-00  GB    Use ERROR_CALL_NOT_IMPLEMENTED for existance of W API
*       08-23-00  GB    Fixed bug with non Ansi CP on Win9x.
*       07-07-01  BWT   Fix error path (set ret=FALSE before branching, not after)
*       11-12-01  GB    Added support for new locale implementation.
*       09-25-04  JL    Replace usage of _alloca() with _alloca_s() / _freea_s()
*       09-27-04  ESC   VSW346744, fix handling of length 0 strings.
*       03-24-05  SJ    VSW431517, Integrate Hotfix 3297
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
#include <locale.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*int __cdecl strncnt - count characters in a string, up to n.
*
*Purpose:
*       Internal local support function. Counts characters in string before
*       null. If null is not found in n chars, then return n.
*
*Entry:
*       const char *string   - start of string
*       int n                - byte count
*
*Exit:
*       returns number of bytes from start of string to
*       null (exclusive), up to n.
*
*Exceptions:
*
*******************************************************************************/

static int __cdecl strncnt (
        const char *string,
        int cnt
        )
{
        int n = cnt;
        char *cp = (char *)string;

        while (n-- && *cp)
            cp++;

        return cnt - n - 1;
}


/***
*int __cdecl __crtLCMapStringA - Get type information about an ANSI string.
*
*Purpose:
*       Internal support function. Assumes info in ANSI string format. Tries
*       to use NLS API call LCMapStringA if available and uses LCMapStringW
*       if it must. If neither are available it fails and returns 0.
*
*Entry:
*       LPCWSTR  LocaleName  - locale context for the comparison.
*       DWORD    dwMapFlags  - see NT\Chicago docs
*       LPCSTR   lpSrcStr    - pointer to string to be mapped
*       int      cchSrc      - wide char (word) count of input string 
*                              (including NULL if any)
*                              (-1 if NULL terminated) 
*       LPSTR    lpDestStr   - pointer to memory to store mapping
*       int      cchDest     - char (byte) count of buffer (including NULL)
*       int      code_page   - for MB/WC conversion. If 0, use __lc_codepage
*       BOOL     bError      - TRUE if MB_ERR_INVALID_CHARS set on call to
*                              MultiByteToWideChar when GetStringTypeW used.
*
*Exit:
*       Success: number of chars written to lpDestStr (including NULL)
*       Failure: 0
*
*Exceptions:
*
*******************************************************************************/

static int __cdecl __crtLCMapStringA_stat(
        _locale_t plocinfo,
        LPCWSTR  LocaleName,
        DWORD    dwMapFlags,
        LPCSTR   lpSrcStr,
        int      cchSrc,
        LPSTR    lpDestStr,
        int      cchDest,
        int      code_page,
        LPWSTR   lpwTmpInBuf,
        LPWSTR   lpwTmpOutBuf,
        BOOL     bError
        )
{
    _LocaleUpdate _loc_update(plocinfo);

    /*
     * LCMapString will map past NULL. Must find NULL if in string
     * before cchSrc characters.
     */
    if (cchSrc > 0) {
        int cchSrcCnt = strncnt(lpSrcStr, cchSrc);
        /*
         * Include NULL in cchSrc if lpSrcStr terminated within cchSrc bytes.
         */
        if (cchSrcCnt < cchSrc) {
            cchSrc = cchSrcCnt + 1;
        } else {
            cchSrc = cchSrcCnt;
        }
    }

    int retval = 0;
    int inbuff_size;
    int outbuff_size;
    wchar_t *inwbuffer = lpwTmpInBuf;
    wchar_t *outwbuffer = lpwTmpOutBuf;  

    /*
     * Convert string and return the requested information. Note that
     * we are converting to a wide string so there is not a
     * one-to-one correspondence between number of wide chars in the
     * input string and the number of *bytes* in the buffer. However,
     * there had *better be* a one-to-one correspondence between the
     * number of wide characters and the number of multibyte characters
     * or the resulting mapped string will be worthless to the user.
     */

    /*
     * Use __lc_codepage for conversion if code_page not specified
     */

    if (0 == code_page)
        code_page = plocinfo->locinfo->lc_codepage;

    /* find out how big a buffer we need (includes NULL if any) */
    if ( 0 == (inbuff_size =
               MultiByteToWideChar( code_page,
                                    bError ? MB_PRECOMPOSED |
                                        MB_ERR_INVALID_CHARS :
                                        MB_PRECOMPOSED,
                                    lpSrcStr,
                                    cchSrc,
                                    NULL,
                                    0 )) )
        return 0;
    if (inwbuffer == NULL) {
#ifndef _WCE_BOOTCRT
        /* allocate enough space for wide chars */
        inwbuffer = (wchar_t *)_calloca( inbuff_size, sizeof(wchar_t) );
        if ( inwbuffer == NULL ) {
            return 0;
        }
#else
        /* BOOTCRT cannot assume the presence of malloc/free */
        return 0;
#endif
    }

    /* do the conversion */
    if ( 0 == MultiByteToWideChar( code_page,
                                   MB_PRECOMPOSED,
                                   lpSrcStr,
                                   cchSrc,
                                   inwbuffer,
                                   inbuff_size) )
        goto error_cleanup;

    /* get size required for string mapping */
    if ( 0 == (retval = LCMapStringEx( LocaleName,
                                      dwMapFlags,
                                      inwbuffer,
                                      inbuff_size,
                                      NULL,
                                      0,
                                      NULL /* Reserved; must be NULL */, 
                                      NULL /* Reserved; must be NULL */, 
                                      0 /* Reserved; must be 0 */ )) )
        goto error_cleanup;

    if (dwMapFlags & LCMAP_SORTKEY) {
        /* retval is size in BYTES */

        if (0 != cchDest) {

            if (retval > cchDest)
                goto error_cleanup;

            /* do string mapping */
            if ( 0 == LCMapStringEx( LocaleName,
                                    dwMapFlags,
                                    inwbuffer,
                                    inbuff_size,
                                    (LPWSTR)lpDestStr,
                                    cchDest,
                                    NULL /* Reserved; must be NULL */, 
                                    NULL /* Reserved; must be NULL */, 
                                    0 /* Reserved; must be 0 */ ) )
                goto error_cleanup;
        }
    }
    else {
        /* retval is size in wide chars */

        outbuff_size = retval;

        if (outwbuffer == NULL) {
#ifndef _WCE_BOOTCRT
            /* allocate enough space for wide chars (includes NULL if any) */
            outwbuffer = (wchar_t *)_calloca( outbuff_size, sizeof(wchar_t) );
            if ( outwbuffer == NULL ) {
                goto error_cleanup;
            }
#else
            goto error_cleanup;
#endif
        }

        /* do string mapping */
        if ( 0 == LCMapStringEx( LocaleName,
                                dwMapFlags,
                                inwbuffer,
                                inbuff_size,
                                outwbuffer,
                                outbuff_size,
                                NULL /* Reserved; must be NULL */, 
                                NULL /* Reserved; must be NULL */, 
                                0 /* Reserved; must be 0 */) )
            goto error_cleanup;

        if (0 == cchDest) {
            /* get size required */
            if ( 0 == (retval =
                       WideCharToMultiByte( code_page,
                                            0,
                                            outwbuffer,
                                            outbuff_size,
                                            NULL,
                                            0,
                                            NULL,
                                            NULL )) )
                goto error_cleanup;
        }
        else {
            /* convert mapping */
            if ( 0 == (retval =
                       WideCharToMultiByte( code_page,
                                            0,
                                            outwbuffer,
                                            outbuff_size,
                                            lpDestStr,
                                            cchDest,
                                            NULL,
                                            NULL )) )
                goto error_cleanup;
        }
    }

error_cleanup:
#ifndef _WCE_BOOTCRT
    /* 
     * Need to free outwbuffer and inwbuffer if it was allocated in 
     * this function.
     * For BOOTCRT, the variables are never allocated in this
     * function.
     */
    if ( (lpwTmpOutBuf == NULL) && (outwbuffer != NULL) )
        _freea(outwbuffer);

    if (lpwTmpInBuf == NULL)
        _freea(inwbuffer);
#endif

    return retval;
}

extern "C" int __cdecl __crtLCMapStringA(
        _locale_t plocinfo,
        LPCWSTR  LocaleName,
        DWORD    dwMapFlags,
        LPCSTR   lpSrcStr,
        int      cchSrc,
        LPSTR    lpDestStr,
        int      cchDest,
        int      code_page,
        BOOL     bError
        )
{

    return __crtLCMapStringA_stat(
            plocinfo,
            LocaleName,
            dwMapFlags,
            lpSrcStr,
            cchSrc,
            lpDestStr,
            cchDest,
            code_page,
            NULL,
            NULL,
            bError
            );
}

#ifdef _WCE_BOOTCRT
/*
 * A slight variant of __crtLCMapStringA() which provides
 * the temp buffers so that malloc/free is not needed.
 * Used in BOOTCRT where malloc/free might not exist.
 */
extern "C" int __cdecl __crtLCMapStringA2(
        _locale_t plocinfo,
        LPCWSTR  LocaleName,
        DWORD    dwMapFlags,
        LPCSTR   lpSrcStr,
        int      cchSrc,
        LPSTR    lpDestStr,
        int      cchDest,
        int      code_page,
        LPWSTR   lpwTmpInBuf,
        LPWSTR   lpwTmpOutBuf,
        BOOL     bError
        )
{

    return __crtLCMapStringA_stat(
            plocinfo,
            LocaleName,
            dwMapFlags,
            lpSrcStr,
            cchSrc,
            lpDestStr,
            cchDest,
            code_page,
            lpwTmpInBuf,
            lpwTmpOutBuf,
            bError
            );
}
#endif
