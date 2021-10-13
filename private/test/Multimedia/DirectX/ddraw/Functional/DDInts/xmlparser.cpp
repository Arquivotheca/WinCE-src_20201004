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
#include "main.h"
#include <windowsqa.h>

#include "XMLParser.h"
#include "fileutils.h"

CFileHandler::CFileHandler( CTLog *pLog)
    : m_pLog(pLog)
{
    m_bPPFS=IsPPFSAvailable();
    m_bStorage=IsStorageCardAvailable();
}

CFileHandler::~CFileHandler()
{
}

BOOL CFileHandler::LoadFileLocal(LPCTSTR pctszPath, LPSTR *ppszOutput, DWORD *pdwSize)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT hr            = S_OK;
    BOOL    br            = FALSE;
    LPSTR   pszLoadedData = NULL;
    HANDLE  hFile         = NULL;
    DWORD   dwFileSize;
    DWORD   dwRead;



    if (!ppszOutput || !pdwSize)
    {
        hr = E_INVALIDARG;
        goto Error;
    }

    hFile = CreateFile(pctszPath, GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, 0, NULL);

    // do not assert on this, we may find the file elsewhere
    CBR(hFile != INVALID_HANDLE_VALUE);

    dwFileSize = GetFileSize(hFile, NULL);

    if (dwFileSize != 0xFFFFFFFF)
    {
        // Allocate return buffer for CHAR
        pszLoadedData = (LPSTR)LocalAlloc(LMEM_FIXED, dwFileSize);
        X_CBR (pszLoadedData != NULL);

        if (ReadFile(hFile, (LPBYTE)pszLoadedData, dwFileSize, &dwRead, 0))
        {
            *ppszOutput = pszLoadedData;
            *pdwSize = dwRead;
        }
        else
        {
            hr = E_FAIL;
        }
    }
    else
    {
        hr = E_FAIL;
    }

Error:
    CloseHandle(hFile);

    return SUCCEEDED(hr);
}

BOOL CFileHandler::LoadFileRemote( LPCTSTR pctszPath, LPSTR *ppszOutput, DWORD *pdwSize)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT hr            = S_OK;
    BOOL    br            = FALSE;
    LPSTR   pszLoadedData = NULL;
    int     iFile         = 0;
    DWORD   dwFileSize;
    DWORD   dwRead;



    if(!ppszOutput || !pdwSize)
    {
        hr = E_INVALIDARG;
        goto Error;
    }

    iFile = U_ropen(pctszPath, 0x10000);  // Magic value to open for reading

    // do not assert we may find the file elsewhere
    CBR(iFile != -1);

    dwFileSize = U_rlseek(iFile, 0, SEEK_END);
    U_rlseek(iFile, 0, SEEK_SET);

    // Allocate return buffer.  Do we need a null here
    pszLoadedData = (LPSTR) LocalAlloc (LMEM_FIXED, dwFileSize);
    X_CBR(pszLoadedData != NULL);

    dwRead = U_rread(iFile, (LPBYTE)pszLoadedData, dwFileSize);

    *ppszOutput = pszLoadedData;
    *pdwSize = dwRead;

Error:

    U_rclose(iFile);

    return SUCCEEDED(hr);

}

BOOL CFileHandler::LoadFileStorage( LPCTSTR pctszPath, LPSTR *ppszOutput, DWORD *pdwSize)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    return FALSE;
}


HRESULT CFileHandler::LoadTextFile(LPCTSTR pctszPath, BSTR * pbstrOutputData)
{
    HRESULT   hr            = S_OK;
    BOOL      br            = FALSE;
    DWORD     dwFileLen     = 0;
    LPSTR     szRawFileData = NULL;
    LPSTR     szRawFileDataStartPoint = NULL;
    int       iSize         = 0;
    WCHAR   * pBuffer       = NULL;
    BOOL      fFoundFile    = FALSE;
    UINT      uiCodePage    = CP_UTF8;
    const byte c_bomUTF8[]  = {0xEF, 0xBB, 0xBF};

    if (!pbstrOutputData)
    {
        hr = E_INVALIDARG;
        goto Error;
    }

    if (m_pLog)
    {
        m_pLog->Log( FALSE,_T("Loading %s from local path."), pctszPath );
    }

    fFoundFile = LoadFileLocal(pctszPath, &szRawFileData, &dwFileLen);

    if( !fFoundFile )
    {
        if(m_bPPFS)
        {
            fFoundFile = LoadFileRemote(pctszPath, &szRawFileData, &dwFileLen);
        }
        else if (m_bStorage)
        {
            fFoundFile = LoadFileStorage(pctszPath, &szRawFileData, &dwFileLen);
        }
        else
        {
            if (m_pLog)
            {
                m_pLog->Log( FALSE,_T("Nothing found on local and no external path."));
            }
            fFoundFile = FALSE;
        }
    }

    // make sure that we've found something
    CBRA (fFoundFile);

    // Set the raw data start point to the beginning of the text stream. 
    szRawFileDataStartPoint = szRawFileData;

    // Skip past the UTF-8 byte-order mark (BOM), if present
    for (int i = 0; i < ARRAYSIZE(c_bomUTF8); i++)
    {
        if (NULL != szRawFileDataStartPoint && (char)c_bomUTF8[i] == *szRawFileDataStartPoint)
        {
            szRawFileDataStartPoint++;
        }
    }

    // Since the input file does not include a NULL terminator, then the
    // length that comes back from MultiByteToWideChar will not include a
    // NULL either.
    iSize = MultiByteToWideChar(uiCodePage, 0, szRawFileDataStartPoint, dwFileLen, NULL, 0);

    // Allocate the memory
    pBuffer = new WCHAR [iSize + 1];
    CPRA(pBuffer);

    // Convert the text to UTF-16 Unicode
    MultiByteToWideChar(uiCodePage, 0, szRawFileDataStartPoint, dwFileLen, pBuffer, iSize);

    // make sure the string is null terminated
    pBuffer[iSize] = L'\0';

    if (*pbstrOutputData)
    {
        ::SysFreeString(*pbstrOutputData);
    }

    (*pbstrOutputData) = ::SysAllocString(pBuffer);
    X_CBR(*pbstrOutputData != NULL);

Error:

    if (pBuffer)
    {
        delete [] pBuffer;
    }

    if (szRawFileData)
    {
        LocalFree(szRawFileData);
    }

    return hr;
}

CXMLParser::CXMLParser( CTLog *pLog)
    : m_pNodeRoot (NULL)
    , m_pNodeCurrent (NULL)
    , m_pDoc (NULL)
    , m_pLog (pLog)
    , m_bSuccess (FALSE)
    , m_lIndent (0)
    , m_fInitCOM (FALSE)
{
}

CXMLParser::~CXMLParser()
{
    m_pNodeCurrent.Release();
    m_pNodeRoot.Release();
    m_pDoc.Release();

    if (m_fInitCOM)
    {
        CoUninitialize();
    }
}


HRESULT CXMLParser::CreateXMLDoc(VOID)
{
    HRESULT                  hr        = S_OK;
    CComPtr<IXMLDOMDocument> pDocTemp;

    // intialize COM (returns S_FALSE if already initialized)
    if (m_fInitCOM == FALSE)
    {
        hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

        // may be S_FALSE at this point
        if (FAILED (hr))
        {
            goto Error;
        }

        m_fInitCOM = TRUE;
    }


    // clean up the old existing document if needed
    m_pDoc = NULL;

    hr = CoCreateInstance(__uuidof(DOMDocument), NULL, CLSCTX_INPROC_SERVER, __uuidof(IXMLDOMDocument), (VOID**)&pDocTemp);
    CHRA(hr);

    // check that we actually allocated a document
    CPRAEx (pDocTemp, E_FAIL);

    // assign to the global document
    m_pDoc = pDocTemp;

    // set the current node to the root of the document
    m_pNodeRoot    = m_pDoc;
    m_pNodeCurrent = m_pDoc;

Error:

    return hr;
}

HRESULT CXMLParser::GetCurrentNode (IXMLDOMNode ** ppNode)
{
    HRESULT hr = S_OK;
    BOOL    br = FALSE;

    // check inputs
    X_CBR ( ppNode != NULL );

    // assign the current node
    *ppNode = m_pNodeCurrent;

    // addref if needed
    if (*ppNode)
    {
        (*ppNode)->AddRef();
    }

Error:

    return hr;
}


// NOTE: GetNodeValue may return S_FALSE
HRESULT CXMLParser::GetNodeValue(IXMLDOMNode *pNodeItem, VARIANT * pvar )
{
    HRESULT              hr    = S_OK;
    BOOL                 br    = FALSE;
    CComPtr<IXMLDOMNode> pNode = NULL;

    //check inputs
    if (!pNodeItem )
    {
        return E_INVALIDARG;
    }

    //Check string token which should be the child of the action node
    hr = pNodeItem->get_firstChild(&pNode);

    // No children in this node? Our work here is done.
    if (S_FALSE == hr)
    {
        goto Error;
    }

    X_CHR(hr);

    hr = pNode->get_nodeValue(pvar);
    X_CHR(hr);

    // verify that we got a BSTR...
    X_CBR(VT_BSTR == pvar->vt);

Error:

    return hr;
}


HRESULT CXMLParser::LoadXMLFile( LPCTSTR pctszXML )
//****************************************************************************
//
//****************************************************************************
{
    HRESULT                  hr          = S_OK;
    VARIANT_BOOL             br          = FALSE;
    CComPtr<IXMLDOMDocument> pDoc        = NULL;
    CComBSTR                 bstrXMLFile;

    CFileHandler             fileHandler(m_pLog);

    X_CBR(pctszXML != NULL);

    hr = fileHandler.LoadTextFile( pctszXML, &bstrXMLFile);
    X_CHR (hr);

    // create the XML document
    hr = CreateXMLDoc();
    X_CHR (hr);

    // Load the XML string into the document, using string not the actual URL
    // since the path of the file is not fixed.
    // Now what happen to the memory for bstrXMLFile
    hr = m_pDoc->loadXML(bstrXMLFile, &br);

    // if this HR fails or is an S_FALSE the XML blob that
    // is being loaded is probably missing a tag
    X_CHR(hr);
    X_CBR(br);

    m_bSuccess = TRUE;

Error:

    return hr;
}