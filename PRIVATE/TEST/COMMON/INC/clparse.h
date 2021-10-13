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
    LPTSTR pszToken;
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

    pszToken = ::_tcstok(pszLocalCmdLine, _T(" /-"));
    while(NULL != pszToken)
    {
        if(0 == ::_tcsicmp(pszToken, pszFlag))  {
            fRet = TRUE;
            break;
        }
        pszToken = ::_tcstok(NULL, _T(" /-"));
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
            if(0 != swscanf(pszTmp, _T("%X"), pVal) ||
               0 != swscanf(pszTmp, _T("%x"), pVal) ) {
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
    LPTSTR pszToken;
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

    pszToken = ::_tcstok(pszLocalCmdLine, _T(" /-"));
    while(NULL != pszToken)
    {
        if(0 == ::_tcsicmp(pszToken, pszFlag))  {
            pszToken = ::_tcstok(NULL, _T("/-"));
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
        pszToken = ::_tcstok(NULL, _T(" /-"));
    }

done:
    if(pszLocalCmdLine) delete[] pszLocalCmdLine;
    return fRet;
}

private:

// store a copy of the command line string (and its length)
//
DWORD m_cchCmdLine;
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

}; // class CClParse

#endif // __CLPARSE_H__
