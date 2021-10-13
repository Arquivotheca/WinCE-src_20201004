//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*******************************************************************************
 * Module: clparse.h
 *
 * Purpose: This class is used for retrieving parameters from a WinMain-style 
 * command line.
 *
 * Example:
 *
 * WCHAR szTmp[100];
 * CClParse cmdLine(lpszCmdLine);
 * 
 * if(cmdLine.GetOpt(L"foo")) {
 *     // command line contained "/foo"
 * }
 *
 * if(cmdLine.GetOptString(L"bar", szTmp, 100)) {
 *     // command line contained "/bar <string>", <string> is stored in szTmp
 *     // where szTemp is 100 characters long. The string will be NULL 
 *     // terminated even if truncated.
 * }
 *
 * if(cmdLine.GetOptVal(L"val", &dwVal)) {
 *     // command line contained "/val <dword>", 32-bit <dword> is stored in 
 *     // dwVal
 * }
 *
 * if(cmdLine.GetOptValHex(L"hex", &dwVal)) {
 *     // command line contained "/hex <hex dword>", 32-bit <hex dword> is 
 *     // stored in dwVal
 * }
 *
 ******************************************************************************/

#ifndef __CLPARSE_H__
#define __CLPARSE_H__

#include <windows.h>
#include <strsafe.h>

class CClParse
{
public:

/*******************************************************************************
 ******************************************************************************/
CClParse(LPCWSTR pszCmdLine) :
    m_cchCmdLine(0),
    m_pszCmdLine(NULL)
{
    if(pszCmdLine) {
        
        // allocate a new buffer to store a copy of the command line
        //
        m_cchCmdLine = wcslen(pszCmdLine);
        if(m_cchCmdLine) {
            m_cchCmdLine += 1; // allow for NULL terminator 
            m_pszCmdLine = new WCHAR[m_cchCmdLine];
        }
        if(m_pszCmdLine) {
            ::StringCchCopy(m_pszCmdLine, MAX_PATH, pszCmdLine);
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
BOOL GetOpt(IN LPCWSTR pszFlag)
{
    if(0 == m_cchCmdLine || NULL == m_pszCmdLine) {
        return FALSE;
    }

    LPWSTR pszToken;
    LPWSTR pszLocalCmdLine = new WCHAR[m_cchCmdLine];
    if(NULL == pszLocalCmdLine) {
        return FALSE;
    }

    ::StringCchCopy(pszLocalCmdLine, m_cchCmdLine, m_pszCmdLine);
    pszToken = ::wcstok(pszLocalCmdLine, L" /");
    while(NULL != pszToken)
    {
        if(0 == ::wcsicmp(pszToken, pszFlag))  {
            delete[] pszLocalCmdLine; 
            return TRUE;
        }
        pszToken = ::wcstok(NULL, L" /");
    }
    delete[] pszLocalCmdLine;
    return FALSE;
}

/*******************************************************************************
 ******************************************************************************/
BOOL GetOptVal(IN LPCWSTR pszFlag, OUT LPDWORD pVal)
{
    if(0 == m_cchCmdLine || NULL == m_pszCmdLine) {
        return FALSE;
    }

    LPWSTR pszTmp = new WCHAR[m_cchCmdLine];
    if(NULL == pszTmp) {
        return FALSE;
    }

    if(GetOptString(pszFlag, pszTmp, m_cchCmdLine)) {
        if(pVal) {
            *pVal = _wtol(pszTmp);
            delete[] pszTmp;
            return TRUE;
        }
    }
    delete[] pszTmp;
    return FALSE;
}

/*******************************************************************************
 ******************************************************************************/
BOOL GetOptValHex(IN LPCWSTR pszFlag, OUT LPDWORD pVal)
{
    if(0 == m_cchCmdLine || NULL == m_pszCmdLine) {
        return FALSE;
    }

    LPWSTR pszTmp = new WCHAR[m_cchCmdLine];
    if(NULL == pszTmp) {
        return FALSE;
    }

    if(GetOptString(pszFlag, pszTmp, m_cchCmdLine)) {
        if(pVal) {
            if(0 != swscanf(pszTmp, L"%X", pVal) ||
               0 != swscanf(pszTmp, L"%x", pVal) ) {
                delete[] pszTmp;
                return TRUE;
            }
        }
    }
    delete[] pszTmp;
    return FALSE;
}

/*******************************************************************************
 ******************************************************************************/
BOOL GetOptString(IN LPCWSTR pszFlag, OUT LPWSTR pszString, IN UINT cLen)
{
    if(0 == m_cchCmdLine || NULL == m_pszCmdLine) {
        return FALSE;
    }

    LPWSTR pszToken;
    LPWSTR pszLocalCmdLine = new WCHAR[m_cchCmdLine];
    if(NULL == pszLocalCmdLine) {
        return FALSE;
    }

    ::StringCchCopy(pszLocalCmdLine, m_cchCmdLine, m_pszCmdLine);
    pszToken = ::wcstok(pszLocalCmdLine, L" /");
    while(NULL != pszToken)
    {
        if(0 == ::wcsicmp(pszToken, pszFlag))  {
            pszToken = ::wcstok(NULL, L"/");
            if(pszString && cLen) {
                if(NULL != pszToken) {
                    ::StringCchCopy(pszString, cLen, pszToken);
                    Chomp(pszString);
                    return TRUE;
                } else {
                    return FALSE;
                }
            } else {
                return TRUE;
            }
        }
        pszToken = ::wcstok(NULL, L" /");
    }
    return FALSE;
}

private:

// store a copy of the command line string (and its length)
//
DWORD m_cchCmdLine;
WCHAR *m_pszCmdLine;

/*******************************************************************************
  trim trailing whitespace from a string
 ******************************************************************************/
LPTSTR Chomp(IN OUT LPTSTR psz)
{
    INT len = 0;
    
    if(psz)
    {
        while((len = ::wcslen(psz)-1) > 0)
        {
            switch(psz[len])
            {
                // match whitespace
                case L' ':
                case L'\t':
                case L'\n':
                case L'\r':
                    psz[len] = L'\0';
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
