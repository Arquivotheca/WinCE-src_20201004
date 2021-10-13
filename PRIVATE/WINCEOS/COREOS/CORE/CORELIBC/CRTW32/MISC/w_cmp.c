//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*w_cmp.c - W versions of CompareString.
*
*
*Purpose:
*       Use either CompareStringA or CompareStringW depending on which is 
*       available
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
*       05-09-94  CFW   Do not let CompareString compare past NULL.
*       06-03-94  CFW   Test for empty string early.
*       11/01-94  CFW   But not too early for MB strings.
*       12-21-94  CFW   Remove invalid MB chars NT 3.1 hack.
*       12-27-94  CFW   Call direct, all OS's have stubs.
*       01-10-95  CFW   Debug CRT allocs.
*       02-06-95  CFW   assert -> _ASSERTE.
*       02-15-97  RDK   For narrow comparsion, try W version first so Windows NT
*                       can process nonANSI codepage correctly.
*       05-15-97  GJF   Split off from aw_cmp.c. Replaced use of _malloc_crt
*                       and _free_crt with _alloca. Also, detab-ed and cleaned
*                       up the code a bit.
*
*******************************************************************************/

#include <corecrt.h>
#include <internal.h>
#include <dbgint.h>
#include <stdlib.h>
#include <setlocal.h>
#include <awint.h>
#include <dbgint.h>

#define USE_W   1
#define USE_A   2

/***
*int __cdecl wcsncnt - count wide characters in a string, up to n.
*
*Purpose:
*       Internal local support function. Counts characters in string before NULL.
*       If NULL not found in n chars, then return n.
*
*Entry:
*       const wchar_t *string   - start of string
*       int n                   - byte count
*
*Exit:
*       returns number of wide characaters from start of string to
*       NULL (exclusive), up to n.
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
*int __cdecl __crtCompareStringW - Get type information about a wide string.
*
*Purpose:
*  Internal support function. Assumes info in wide string format. Tries
*  to use NLS API call CompareStringW if available and uses CompareStringA
*  if it must. If neither are available it fails and returns 0.
*
*Entry:
*  LCID     Locale      - locale context for the comparison.
*  DWORD    dwCmpFlags  - see NT\Chicago docs
*  LPCWSTR  lpStringn   - wide string to be compared
*  int      cchCountn   - wide char (word) count (NOT including NULL)
*                       (-1 if NULL terminated)
*  int      code_page   - for MB/WC conversion. If 0, use __lc_codepage
*
*Exit:
*  Success: 1 - if lpString1 <  lpString2
*           2 - if lpString1 == lpString2
*           3 - if lpString1 >  lpString2
*  Failure: 0
*
*Exceptions:
*
*******************************************************************************/

int __cdecl __crtCompareStringW(
        LCID     Locale,
        DWORD    dwCmpFlags,
        LPCWSTR  lpString1,
        int      cchCount1,
        LPCWSTR  lpString2,
        int      cchCount2,
        int      code_page
        )
{
        /*
         * CompareString will compare past NULL. Must find NULL if in string
         * before cchCountn wide characters.
         */

        if (cchCount1 > 0)
            cchCount1= wcsncnt(lpString1, cchCount1);
        if (cchCount2 > 0)
            cchCount2= wcsncnt(lpString2, cchCount2);

        if (!cchCount1 || !cchCount2)
            return (cchCount1 - cchCount2 == 0) ? 2 :
                   (cchCount1 - cchCount2 < 0) ? 1 : 3;
        
	return CompareStringW( Locale, dwCmpFlags, lpString1,
			       cchCount1, lpString2, cchCount2 );
     
}


