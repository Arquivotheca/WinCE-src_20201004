//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
//
// File:
//      Url.cpp
//
// Contents:
//
//      CUrl class implementation
//
//-----------------------------------------------------------------------------


#include "Headers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CUrl::CUrl()
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CUrl::CUrl()
{
    m_bstrUrl       = 0;
    m_bstrHostName  = 0;
    m_bstrUserName  = 0;
    m_bstrPassword  = 0;
    m_bstrUrlPath   = 0;
    m_bstrExtraInfo = 0;
    m_bstrExtraPath = 0;
    m_nScheme       = INTERNET_SCHEME_UNKNOWN;
    m_nPort         = INTERNET_INVALID_PORT_NUMBER;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CUrl::~CUrl()
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CUrl::~CUrl()
{
    FreeAll();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CUrl::get_Url(BSTR *pDest)
//
//  parameters:
//
//  description:
//          Returns the copy of an URL bstr
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CUrl::get_Url(BSTR *pDest)
{
    if (pDest == 0)
        return E_INVALIDARG;

    *pDest = 0;

    return ::FreeAndCopyBSTR(*pDest, m_bstrUrl);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CUrl::put_Url(WCHAR *pszUrl)
//
//  parameters:
//
//  description:
//          Stores new value for a URL (creates copy of it and cracks it to parts)
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CUrl::put_Url(WCHAR *pszUrl)
{
    BSTR    bstrUrl = 0;
    HRESULT hr      = S_OK;

    FreeAll();

    if (pszUrl)
    {
        CHK(::CopyBSTR(bstrUrl, pszUrl));
        CHK(CrackAndCache(bstrUrl));

        m_bstrUrl = bstrUrl;
        bstrUrl   = 0;
    }

Cleanup:
    ::SysFreeString(bstrUrl);
    return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CUrl::SetUrl(BSTR bstrUrl)
//
//  parameters:
//
//  description:
//          Stores new value for a URL (DOESN't create copy of it and cracks it to parts)
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CUrl::SetUrl(BSTR bstrUrl)
{
    HRESULT hr = S_OK;

    CHK(CrackAndCache(bstrUrl));

    m_bstrUrl = bstrUrl;

Cleanup:
    return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CUrl::CrackAndCache(BSTR bstrUrl)
//
//  parameters:
//
//  description:
//          Cracks URL and stores it in cache member variables
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef HTTP_LITE
    HRESULT ConvertFromASCII(BSTR &pBuff, char *data, DWORD len)
    {  
        ASSERT(pBuff == 0);        

        if(data == 0 || len <= 0)
        {            
            return S_OK;
        }
    
        HRESULT hr = E_FAIL;
        __try{
            //allocate mem off the stack
            WCHAR *pBuf = (WCHAR *)_alloca(len * 2);
        
            //do a mb to wc conversion
            if(0 == MultiByteToWideChar(CP_ACP, 0, data, len, pBuf, len*2))
                hr = E_OUTOFMEMORY;
            else {
                pBuff = ::SysAllocStringLen(pBuf, len);
           
                if(0 == pBuff)
                    hr = E_OUTOFMEMORY;
                else
                    hr = S_OK;
            }
        }
        __except(1) {
            hr = E_OUTOFMEMORY;
        }

        return hr;
    }

#endif  
HRESULT CUrl::CrackAndCache(BSTR bstrUrl)
{
    HRESULT         hr              = S_OK;
    BSTR            bstrHostName    = 0;
    BSTR            bstrUserName    = 0;
    BSTR            bstrPassword    = 0;
    BSTR            bstrUrlPath     = 0;
    BSTR            bstrExtraInfo   = 0;
    BSTR            bstrExtraPath   = 0;

#ifdef HTTP_LITE
 URL_COMPONENTSA urlParts = 
#else
 URL_COMPONENTSW     urlParts    =
#endif
    {
        sizeof(URL_COMPONENTS),     // dwStructSize
        0,                          // lpszScheme
        0,                          // dwSchemeLength
        INTERNET_SCHEME_DEFAULT,    // nScheme
        0,                          // lpszHostName
        1,                          // dwHostNameLength
        0,                          // nPort
        0,                          // lpszUserName
        1,                          // dwUserNameLength
        0,                          // lpszPassword
        1,                          // dwPasswordLength
        0,                          // lpszUrlPath
        1,                          // dwUrlPathLength
        0,                          // lpszExtraInfo
        1,                          // dwExtraInfoLength
    };
#ifdef HTTP_LITE
    BSTR a = 0, b = 0;

    __try{
        UINT uiLen = wcslen(bstrUrl);    
        char *pUrl = (char *)(_alloca(uiLen + 1));
      
        if(!pUrl)
        {
            hr = E_OUTOFMEMORY;
            goto Cleanup;
        }  
   
        if(0 == WideCharToMultiByte(CP_ACP, 0, bstrUrl, -1, pUrl, uiLen+1, NULL, NULL))
        {
            hr = E_FAIL;
            goto Cleanup;
        }

        if (::InternetCrackUrlA(pUrl, 0, 0, &urlParts) != TRUE)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            goto Cleanup;
        }
    }
    __except(1){
        hr = E_OUTOFMEMORY;
        goto Cleanup;
    }
    
    CHK(ConvertFromASCII(bstrHostName,  urlParts.lpszHostName,  urlParts.dwHostNameLength));
    CHK(ConvertFromASCII(bstrUserName,  urlParts.lpszUserName,  urlParts.dwUserNameLength));
    CHK(ConvertFromASCII(bstrPassword,  urlParts.lpszPassword,  urlParts.dwPasswordLength));
    CHK(ConvertFromASCII(bstrUrlPath,   urlParts.lpszUrlPath,   urlParts.dwUrlPathLength));
    CHK(ConvertFromASCII(bstrExtraInfo, urlParts.lpszExtraInfo, urlParts.dwExtraInfoLength));

 
    CHK(ConvertFromASCII(a, urlParts.lpszUrlPath, urlParts.dwUrlPathLength));
    CHK(ConvertFromASCII(b, urlParts.lpszExtraInfo, urlParts.dwExtraInfoLength));
    
    CHK(CatBSTRs(bstrExtraPath, a,   urlParts.dwUrlPathLength,
                                       b, urlParts.dwExtraInfoLength));

    if(a)
        ::SysFreeString(a);
    if(b)
        ::SysFreeString(b);
    
#else
    if (::InternetCrackUrlW(bstrUrl, 0, 0, &urlParts) != TRUE)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Cleanup;
    }

    CHK(CopyBSTR(bstrHostName,  urlParts.lpszHostName,  urlParts.dwHostNameLength));
    CHK(CopyBSTR(bstrUserName,  urlParts.lpszUserName,  urlParts.dwUserNameLength));
    CHK(CopyBSTR(bstrPassword,  urlParts.lpszPassword,  urlParts.dwPasswordLength));
    CHK(CopyBSTR(bstrUrlPath,   urlParts.lpszUrlPath,   urlParts.dwUrlPathLength));
    CHK(CopyBSTR(bstrExtraInfo, urlParts.lpszExtraInfo, urlParts.dwExtraInfoLength));
    CHK(CatBSTRs(bstrExtraPath, urlParts.lpszUrlPath,   urlParts.dwUrlPathLength,
                                urlParts.lpszExtraInfo, urlParts.dwExtraInfoLength));

    FreeAll();
#endif
    m_bstrHostName  = bstrHostName;  bstrHostName  = 0;
    m_bstrUserName  = bstrUserName;  bstrUserName  = 0;
    m_bstrPassword  = bstrPassword;  bstrPassword  = 0;
    m_bstrUrlPath   = bstrUrlPath;   bstrUrlPath   = 0;
    m_bstrExtraInfo = bstrExtraInfo; bstrExtraInfo = 0;
    m_bstrExtraPath = bstrExtraPath; bstrExtraPath = 0;

    m_nPort   = urlParts.nPort;
    m_nScheme = urlParts.nScheme;

Cleanup:
    ::SysFreeString(bstrHostName);
    ::SysFreeString(bstrUserName);
    ::SysFreeString(bstrPassword);
    ::SysFreeString(bstrUrlPath);
    ::SysFreeString(bstrExtraInfo);
    ::SysFreeString(bstrExtraPath);

    return hr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void CUrl::FreeAll()
//
//  parameters:
//
//  description:
// 
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUrl::FreeAll()
{
    ::FreeAndStoreBSTR(m_bstrUrl,       0);
    ::FreeAndStoreBSTR(m_bstrHostName,  0);
    ::FreeAndStoreBSTR(m_bstrUserName,  0);
    ::FreeAndStoreBSTR(m_bstrPassword,  0);
    ::FreeAndStoreBSTR(m_bstrUrlPath,   0);
    ::FreeAndStoreBSTR(m_bstrExtraInfo, 0);
    ::FreeAndStoreBSTR(m_bstrExtraPath, 0);

    m_nScheme = INTERNET_SCHEME_UNKNOWN;
    m_nPort   = INTERNET_INVALID_PORT_NUMBER;
}

