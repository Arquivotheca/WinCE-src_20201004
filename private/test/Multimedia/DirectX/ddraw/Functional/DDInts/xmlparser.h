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
#pragma once

#include <atlbase.h>
#include <ehm.h>

#include <msxml.h>
#include <ctlog.h>


//macro for error checking. avoid dupes with product code
#define X_CHR(hResult) \
        hr = (hResult);\
        if( hr != S_OK ) { \
            if (m_pLog) {m_pLog->Log(TRUE, _T("ERROR in file %s (line %i), HRESULT code %x."), TEXT(__FILE__), __LINE__, hr); }\
            else \
            {  \
                TCHAR tszBuf[MAX_LOGBUFFSIZE];\
                wsprintf( tszBuf, _T("ERROR in file %s (line %i), HRESULT code %x."), TEXT(__FILE__), __LINE__, hr);\
                OutputDebugString( tszBuf);\
            }\
            ASSERT(0);\
            goto Error;\
        }

#define X_CBR(bResult) \
        SetLastError(0);\
        if( !(bResult)  ) { \
            if (m_pLog) {m_pLog->Log(TRUE, _T("ERROR in file %s (line %i), Last error %d."), TEXT(__FILE__), __LINE__, GetLastError()); }\
            else \
            {\
                TCHAR tszBuf[MAX_LOGBUFFSIZE];\
                wsprintf( tszBuf, _T("ERROR in file %s (line %i), Last error %d."), TEXT(__FILE__), __LINE__, GetLastError());\
                OutputDebugString( tszBuf);\
            }\
            br=FALSE; \
            hr=E_FAIL; \
            ASSERT(0);\
            goto Error;\
        }\
        else\
        {\
            hr = S_OK;\
        }\


class CFileHandler
{
public:
    HRESULT LoadTextFile (LPCTSTR pctszPath, BSTR * pbstrOutputData);

    CFileHandler(CTLog *pLog);
    ~CFileHandler();

private:
    BOOL    m_bPPFS;
    BOOL    m_bStorage;
    CTLog * m_pLog;

    BOOL LoadFileLocal(LPCTSTR ptszFile, LPSTR *ppszOut, DWORD *pdwSize); //caller needs to free the memory
    BOOL LoadFileRemote(LPCTSTR ptszFile, LPSTR *ppszOut, DWORD *pdwSize); //caller needs to free the memory
    BOOL LoadFileStorage(LPCTSTR ptszFile, LPSTR *ppszOut, DWORD *pdwSize); //caller needs to free the memory

    // private - don't use
    CFileHandler();
};

class CXMLParser
{
public:

    CXMLParser ( CTLog *pLog );
    ~CXMLParser ();

    HRESULT LoadXMLFile( LPCTSTR pctszXML );
    HRESULT CreateXMLDoc(VOID);
    HRESULT GetNodeValue(IXMLDOMNode *pNodeItem, VARIANT * pvar);
    HRESULT GetCurrentNode( IXMLDOMNode ** ppNode);
    BOOL    IsSuccessful() { return m_bSuccess; };  

protected:
    CTLog   *m_pLog;

private:

    CComPtr<IXMLDOMDocument> m_pDoc;
    CComPtr<IXMLDOMNode>     m_pNodeRoot;
    CComPtr<IXMLDOMNode>     m_pNodeCurrent;

    BOOL m_fInitCOM;
    BOOL m_bSuccess;
    LONG m_lIndent;

    // make this constructor private so nobody uses it
    CXMLParser ();
};


