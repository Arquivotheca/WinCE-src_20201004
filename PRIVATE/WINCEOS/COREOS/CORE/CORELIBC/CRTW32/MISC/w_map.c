//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*w_map.c - W version of LCMapString.
*
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
*       07-26-94  CFW   #14730, LCMapString goes past NULLs.
*       12-21-94  CFW   Remove invalid MB chars NT 3.1 hack.
*       12-27-94  CFW   Call direct, all OS's have stubs.
*       01-10-95  CFW   Debug CRT allocs.
*       02-15-97  RDK   For narrow mapping, try W version first so Windows NT
*                       can process nonANSI codepage correctly.
*       03-16-97  RDK   Added error flag to __crtLCMapStringA.
*       05-09-97  GJF   Split off from aw_map.c. Revised to use _alloca
*                       instead of malloc. Also, reformatted.
*
*******************************************************************************/

#include <cruntime.h>
#include <internal.h>
#include <stdlib.h>
#include <setlocal.h>
#include <awint.h>
#include <dbgint.h>

/***
*int __cdecl wcsncnt - count wide characters in a string, up to n.
*
*Purpose:
*       Internal local support function. Counts characters in string before
*       null. If null not found in n chars, then return n.
*
*Entry:
*       const wchar_t *string   - start of string
*       int n                - byte count
*
*Exit:
*       returns number of wide characaters from start of string to
*       null (exclusive), up to n.
*
*Exceptions:
*
*******************************************************************************/

static int __cdecl wcsncnt (
        const wchar_t *string,
        int cnt
        )
{
        int n = cnt;
        wchar_t *cp = (wchar_t *)string;

        while (n-- && *cp)
            cp++;

        if (!*cp)
            return cp - string;
        return cnt;
}

/***
*int __cdecl __crtLCMapStringW - Get type information about a wide string.
*
*Purpose:
*       Internal support function. Assumes info in wide string format. Tries
*       to use NLS API call LCMapStringW if available and uses LCMapStringA
*       if it must. If neither are available it fails and returns 0.
*
*Entry:
*       LCID     Locale      - locale context for the comparison.
*       DWORD    dwMapFlags  - see NT\Chicago docs
*       LPCWSTR  lpSrcStr    - pointer to string to be mapped
*       int      cchSrc      - wide char (word) count of input string 
*                              (including NULL if any)
*                              (-1 if NULL terminated) 
*       LPWSTR   lpDestStr   - pointer to memory to store mapping
*       int      cchDest     - wide char (word) count of buffer (including NULL)
*       int      code_page   - for MB/WC conversion. If 0, use __lc_codepage
*
*       NOTE:    if LCMAP_SORTKEY is specified, then cchDest refers to number 
*                of BYTES, not number of wide chars. The return string will be
*                a series of bytes with a NULL byte terminator.
*
*Exit:
*       Success: if LCMAP_SORKEY:
*                   number of bytes written to lpDestStr (including NULL byte 
*                   terminator)
*               else
*                   number of wide characters written to lpDestStr (including 
*                   NULL)
*       Failure: 0
*
*Exceptions:
*
*******************************************************************************/

int __cdecl __crtLCMapStringW(
        LCID     Locale,
        DWORD    dwMapFlags,
        LPCWSTR  lpSrcStr,
        int      cchSrc,
        LPWSTR   lpDestStr,
        int      cchDest,
        int      code_page
        )
{
  if (cchSrc > 0)
    cchSrc = wcsncnt(lpSrcStr, cchSrc);

  return LCMapStringW( Locale, dwMapFlags, lpSrcStr, cchSrc, 
                                 lpDestStr, cchDest );
}


