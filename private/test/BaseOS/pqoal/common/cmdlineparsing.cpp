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
/*
 * cmdLineParsing.cpp
 */

#include "commonUtils.h"


/*******************************************************************************
  trim trailing whitespace from a string
 ******************************************************************************/
LPTSTR Chomp(__inout __nullterminated LPTSTR psz)
{
    INT len = 0;

    if(psz)
    {
        while((len = ::_tcslen(psz)-1) > 0)
        {
            switch(psz[len])
            {
                // match whitespace
                case _T(' '):
                case _T('\t'):
                case _T('\n'):
                case _T('\r'):
                    psz[len] = _T('\0');
                    break;

                // anything else and we're done
                default:
                    return psz;
            }
        }
    }

    return psz;
}

BOOL GetArgument(LPCTSTR pszCmdLine, IN LPCTSTR pszArgName, __inout_ecount_opt(*pdwLen) LPTSTR pszArgVal, PDWORD pdwLen )
{
    BOOL fRet = FALSE;
    LPTSTR pszToken;
    LPTSTR pszLocalCmdLine = NULL;
    TCHAR *szNextTok = NULL;

    /* got null */
    if ( !pszCmdLine || !pdwLen )
    {
        goto done;
    }

    DWORD cchCmdLine = wcslen(pszCmdLine);
    if(cchCmdLine)
    {
        cchCmdLine += 1; // allow for NULL terminator
        pszLocalCmdLine = (LPTSTR) malloc (cchCmdLine * sizeof (TCHAR));
    }
    else
    {
        goto done;
    }

    if(pszLocalCmdLine)
    {
        ::StringCchCopyEx(pszLocalCmdLine, MAX_PATH, pszCmdLine, NULL, NULL, STRSAFE_NULL_ON_FAILURE);
    }
    else
    {
        goto done;
    }

    szNextTok = NULL;
    pszToken = ::_tcstok_s(pszLocalCmdLine, _T(" /-"), &szNextTok);
    while(NULL != pszToken)
    {
        if(0 == ::_tcsicmp(pszToken, pszArgName))
        {
            pszToken = ::_tcstok_s(NULL, _T(" "), &szNextTok);
            if(NULL == pszToken)
            {
                // couldn't find the token - EOS
                goto done;
            }

            DWORD cchNeeded = wcslen( pszToken ) + 1;

            if( cchNeeded > *pdwLen )
            {
                // Buffer not big enough
                *pdwLen = cchNeeded;
                if(pszArgVal)
                {
                    SetLastError( ERROR_INSUFFICIENT_BUFFER );
                    fRet = FALSE;
                    goto done;
                }
                else
                {
                    // with null pszArgVal, just querying the size needed
                    fRet = TRUE;
                    break;
                }
            }
            else if( pszArgVal )
            {
                ::StringCchCopyEx(pszArgVal, *pdwLen, pszToken, NULL, NULL, STRSAFE_NULL_ON_FAILURE);
                Chomp(pszArgVal);
                fRet = TRUE;
                break;
            }
            else
            {
                break;
            }
        }
        pszToken = ::_tcstok_s(NULL, _T(" /-"), &szNextTok);
    }

done:
    if(pszLocalCmdLine)
    {
        free(pszLocalCmdLine);
    }
    return fRet;
}
