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
//+----------------------------------------------------------------------------
//
//
// File:
//      Url.h
//
// Contents:
//
//      CUrl class declaration
//
//-----------------------------------------------------------------------------

#ifndef __URL_H_INCLUDED__
#define __URL_H_INCLUDED__

class CUrl
{
private:
    // The whole
    BSTR            m_bstrUrl;

    // Cached parts
    BSTR            m_bstrHostName;
    BSTR            m_bstrUserName;
    BSTR            m_bstrPassword;
    BSTR            m_bstrUrlPath;
    BSTR            m_bstrExtraInfo;
    BSTR            m_bstrExtraPath;
    INTERNET_SCHEME m_nScheme;
    INTERNET_PORT   m_nPort;

public:
    CUrl();
    ~CUrl();

    HRESULT get_Url(BSTR  *pDest);
    HRESULT put_Url(WCHAR *pszUrl);

    HRESULT SetUrl(BSTR bstrUrl);

    BSTR Url();
    BSTR HostName();
    BSTR UserName();
    BSTR Password();
    BSTR UrlPath();
    BSTR ExtraInfo();
    BSTR ExtraPath();

    INTERNET_SCHEME Scheme();
    INTERNET_PORT Port();

    bool IsEmpty();

private:
    HRESULT CrackAndCache(BSTR bstrUrl);
    void FreeAll();
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  CUrl's Inline methods
////////////////////////////////////////////////////////////////////////////////////////////////////
inline BSTR CUrl::Url()
{
    return m_bstrUrl;
}

inline BSTR CUrl::HostName()
{
    return m_bstrHostName;
}

inline BSTR CUrl::UserName()
{
    return m_bstrUserName;
}

inline BSTR CUrl::Password()
{
    return m_bstrPassword;
}

inline BSTR CUrl::UrlPath()
{
    return m_bstrUrlPath;
}

inline BSTR CUrl::ExtraInfo()
{
    return m_bstrExtraInfo;
}

inline BSTR CUrl::ExtraPath()
{
    return m_bstrExtraPath;
}

inline INTERNET_SCHEME CUrl::Scheme()
{
    return m_nScheme;
}

inline INTERNET_PORT CUrl::Port()
{
    return m_nPort;
}

inline bool CUrl::IsEmpty()
{
    return !! m_bstrUrl;
}

#endif //__URL_H_INCLUDED__
