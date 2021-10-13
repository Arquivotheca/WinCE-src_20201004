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
/*******************************************************************************
 * Module: clparse.h
 *
 * Purpose: This class is used for retrieving parameters from a WinMain-style 
 * command line.
 *
 * Example:
 *
 * TCHAR szTmp[100];
 * CClParse cmdLine(lpszCmdLine);
 * 
 * if(cmdLine.GetOpt(_T("foo"))) {
 *     // command line contained "/foo" or "-foo"
 * }
 *
 * if(cmdLine.GetOptString(_T("bar"), szTmp, 100)) {
 *     // command line contained "/bar <string>", or "-bar <string>" <string> is stored in szTmp
 *     // where szTemp is 100 characters long. The string will be NULL 
 *     // terminated even if truncated.
 * }
 *
 * if(cmdLine.GetOptVal(_T("val"), &dwVal)) {
 *     // command line contained "/val <dword>", or "-val <dword>" 32-bit <dword> is stored in 
 *     // dwVal
 * }
 *
 * if(cmdLine.GetOptValHex(_T("hex"), &dwVal)) {
 *     // command line contained "/hex <hex dword>", or "-hex <hex dword>" 32-bit <hex dword> is 
 *     // stored in dwVal
 * }
 *
 ******************************************************************************/

#ifndef __CLPARSE_H__
#define __CLPARSE_H__

#include <windows.h>

#ifndef UNDER_CE
#include <tchar.h>
#include <strsafe.h>
#endif

class CClParse
{
public:

/*******************************************************************************
 ******************************************************************************/
CClParse(LPCTSTR pszCmdLine) :
    m_cchCmdLine(0),
    m_pszCmdLine(NULL)
{
    if(pszCmdLine) {
        
        // allocate a new buffer to store a copy of the command line
        //

        m_cchCmdLine = wcslen(pszCmdLine);

        if(m_cchCmdLine) {
            m_cchCmdLine += 1; // allow for NULL terminator 
            m_pszCmdLine = new TCHAR[m_cchCmdLine];
        }
        if(m_pszCmdLine) {
            ::StringCchCopyEx(m_pszCmdLine, MAX_PATH, pszCmdLine, NULL, NULL,
                STRSAFE_NULL_ON_FAILURE);
        }
    }
}

/*******************************************************************************
 ******************************************************************************/
~CClParse()
{
    if(m_pszCmdLine) delete[] m_pszCmdLine;
}

/*******************************************************************************
 ******************************************************************************/
BOOL GetOpt(IN LPCTSTR pszFlag)
{
    BOOL fRet = FALSE;
    LPTSTR pszToken = NULL;
    LPTSTR pszNextTok = NULL;
    LPTSTR pszLocalCmdLine = NULL;

    if(!IsValid()) {
        goto done;
    }
    
    pszLocalCmdLine = new TCHAR[m_cchCmdLine];
    if(NULL == pszLocalCmdLine) {
        goto done;
    }

    ::StringCchCopyEx(pszLocalCmdLine, m_cchCmdLine, m_pszCmdLine, NULL, NULL,
        STRSAFE_NULL_ON_FAILURE);

    pszToken = _tcstok_s(pszLocalCmdLine, _T(" /-"), &pszNextTok);
    while(NULL != pszToken)
    {
        if(0 == ::_tcsicmp(pszToken, pszFlag))  {
            fRet = TRUE;
            break;
        }
        pszToken = _tcstok_s(NULL, _T(" /-"), &pszNextTok);
    }

done:
    if(pszLocalCmdLine) delete[] pszLocalCmdLine;
    return fRet;
}

/*******************************************************************************
 ******************************************************************************/
BOOL GetOptVal(IN LPCTSTR pszFlag, OUT LPDWORD pVal)
{
    BOOL fRet = FALSE;
    LPTSTR pszTmp = NULL;
    
    if(!IsValid() || NULL == pVal) {
        goto done;
    }
    
    pszTmp = new TCHAR[m_cchCmdLine];
    if(NULL == pszTmp) {
        goto done;
    }

    if(GetOptString(pszFlag, pszTmp, m_cchCmdLine)) {
        if(pVal) {
            *pVal = _wtol(pszTmp);
            fRet = TRUE;
        }
    }

done:    
    if(pszTmp) delete[] pszTmp;
    return fRet;
}

/*******************************************************************************
 ******************************************************************************/
BOOL GetOptValHex(IN LPCTSTR pszFlag, OUT LPDWORD pVal)
{
    BOOL fRet = FALSE;
    LPTSTR pszTmp = NULL;
    
    if(!IsValid() || NULL == pVal) {
        goto done;
    }
        
    pszTmp = new TCHAR[m_cchCmdLine];
    if(NULL == pszTmp) {
        goto done;
    }

    if(GetOptString(pszFlag, pszTmp, m_cchCmdLine)) {
        if(pVal) {
            if(0 != swscanf_s(pszTmp, _T("%X"), pVal, sizeof(DWORD)) ||
               0 != swscanf_s(pszTmp, _T("%x"), pVal, sizeof(DWORD)) ) {
                fRet = TRUE;
            }
        }
    }

done:
    if(pszTmp) delete[] pszTmp;
    return fRet;
}

/*******************************************************************************
 ******************************************************************************/
BOOL GetOptString(IN LPCTSTR pszFlag, OUT LPTSTR pszString, IN UINT cLen)
{
    BOOL fRet = FALSE;
    LPTSTR pszToken = NULL;
    LPTSTR pszNextTok = NULL;
    LPTSTR pszLocalCmdLine = NULL;
    
    if(!IsValid() || NULL == pszString) {
        goto done;
    }
    
    pszLocalCmdLine = new TCHAR[m_cchCmdLine];
    if(NULL == pszLocalCmdLine) {
        goto done;
    }

    ::StringCchCopyEx(pszLocalCmdLine, m_cchCmdLine, m_pszCmdLine, NULL, NULL,
            STRSAFE_NULL_ON_FAILURE);

    pszToken = _tcstok_s(pszLocalCmdLine, _T(" /-"), &pszNextTok);
    while(NULL != pszToken)
    {
        if(0 == ::_tcsicmp(pszToken, pszFlag))  {
            pszToken = _tcstok_s(NULL, _T("/-"),&pszNextTok);
            if(pszString && cLen) {
                if(NULL != pszToken) {
                    ::StringCchCopyEx(pszString, cLen, pszToken, NULL, NULL,
                            STRSAFE_NULL_ON_FAILURE);
                    Chomp(pszString);
                    fRet = TRUE;
                    break;
                } else {
                    // reached the end of the string
                    goto done;
                }
            } else {
                // no output string
                goto done;
            }
        }
        pszToken = _tcstok_s(NULL, _T(" /-"),&pszNextTok);
    }

done:
    if(pszLocalCmdLine) delete[] pszLocalCmdLine;
    return fRet;
}


/*******************************************************************************
* Extended version of GetOptString with support of literals enclosed with ""
 ******************************************************************************/
BOOL GetOptStringEx(IN LPCTSTR pszFlag, OUT LPTSTR pszString, IN UINT cLen)
{
    BOOL fRet = FALSE;
    LPTSTR pszToken = NULL;
    LPTSTR pszTokenTmp = NULL;
    LPTSTR pszNextTok = NULL;
    LPTSTR pszLocalCmdLine = NULL;
    LPTSTR pszLocalCmdLineTmp = NULL;
    
    if(!IsValid() || NULL == pszString) {
        goto done;
    }
    
    pszLocalCmdLine = new TCHAR[m_cchCmdLine];
    if(NULL == pszLocalCmdLine) {
        goto done;
    }

    pszLocalCmdLineTmp = new TCHAR[m_cchCmdLine];
    if(NULL == pszLocalCmdLineTmp) {
        goto done;
    }

    ::StringCchCopyEx(pszLocalCmdLine, m_cchCmdLine, m_pszCmdLine, NULL, NULL,
            STRSAFE_NULL_ON_FAILURE);

    ::StringCchCopyEx(pszLocalCmdLineTmp, m_cchCmdLine, m_pszCmdLine, NULL, NULL,
            STRSAFE_NULL_ON_FAILURE);

    pszToken = _tcstok_s(pszLocalCmdLine, _T(" /-"), &pszNextTok);
    while(NULL != pszToken)
    {
        if(0 == ::_tcsicmp(pszToken, pszFlag))  {
            pszToken = _tcstok_s(NULL, _T(" \t"),&pszNextTok);

            // Check for double-quotes (for literal string).
            if (pszToken[0] == '"') {
                pszTokenTmp = ::_tcsstr(pszLocalCmdLineTmp, pszToken);

                if (!pszTokenTmp)
                    goto done;
                pszToken = _tcstok_s(pszTokenTmp, _T("\""),&pszNextTok);
            }
            else
                pszToken = _tcstok_s(pszToken, _T("/-"),&pszNextTok);

            if(pszString && cLen) {
                if(NULL != pszToken) {
                    ::StringCchCopyEx(pszString, cLen, pszToken, NULL, NULL,
                    STRSAFE_NULL_ON_FAILURE);
                    fRet = TRUE;
                    break;
                } else {
                    // reached the end of the string
                    goto done;
                }
            } else {
                // no output string
                goto done;
            }
        }
        pszToken = _tcstok_s(NULL, _T(" /-"),&pszNextTok);
    }

done:
    if(pszLocalCmdLine) delete[] pszLocalCmdLine;
    if(pszLocalCmdLineTmp) delete[] pszLocalCmdLineTmp;
    return fRet;
}

private:

// store a copy of the command line string (and its length)
//
size_t m_cchCmdLine;
TCHAR *m_pszCmdLine;


/*******************************************************************************
 ******************************************************************************/
inline BOOL IsValid()
{
    return (NULL != m_pszCmdLine) && (0 != m_cchCmdLine);
}

/*******************************************************************************
  trim trailing whitespace from a string
 ******************************************************************************/
LPTSTR Chomp(IN OUT LPTSTR psz)
{
    size_t len = 0;
    
    if(psz)
    {
        HRESULT hr = StringCchLength(psz, STRSAFE_MAX_CCH, &len);
        while(SUCCEEDED(hr) && (len > 0))
         {
            switch(psz[--len])
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

}; // class CClParse

#endif // __CLPARSE_H__
