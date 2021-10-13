//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      UrlUtil.cpp
//
// Contents:
//
//      Url related utilities
//
//----------------------------------------------------------------------------------

#include "Headers.h"

char s_ToHex[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT EscapeUrl(const BSTR unescaped, BSTR *escaped)
//
//  parameters:
//
//  description:
//        Escapes URL given as BSTR. First converts into utf-8, then escapes and returns as BSTR. 
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT EscapeUrl(const BSTR unescaped, BSTR *escaped)
{
    CHK_ARG(escaped);

    HRESULT hr  = S_OK;
    BSTR    esc = 0;

    *escaped = 0;

    if (unescaped)
    {
        CHAR *unesc  = 0;
        DWORD dwSize = 0;

        __try
        {
            CONVERT_WIDECHAR_TO_UTF8(unescaped, unesc);
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            hr = E_OUTOFMEMORY;
            goto Cleanup;
        }

        dwSize = SizeToEscapeA(unesc);

        if (dwSize)
        {
            esc = ::SysAllocStringLen(0, dwSize - 1);
            CHK_MEM(esc);

            CHK((DoEscapeUrl<CHAR, WCHAR>(unesc, esc)));

            *escaped = esc;
            esc      = 0;
        }
    }

Cleanup:
    ::SysFreeString(esc);
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT UnescapeUrl(const BSTR escaped, BSTR *unescaped)
//
//  parameters:
//
//  description:
//        Unescapes URL stored in BSTR, then converts from UTF-8 into UTF-16 returning another BSTR
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT UnescapeUrl(const BSTR escaped, BSTR *unescaped)
{
    CHK_ARG(unescaped);

    HRESULT hr     = S_OK;
    BSTR    unesc  = 0;

    *unescaped = 0;

    if (escaped)
    {
        CHAR *un    = 0;
        DWORD dwSize = SizeToUnescapeW(escaped);

        __try
        {
            un = reinterpret_cast<CHAR *>(_alloca(dwSize));
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            hr = E_OUTOFMEMORY;
            goto Cleanup;
        }

        CHK((DoUnescapeUrl<WCHAR, CHAR>(escaped, un)));

        {
#ifndef UNDER_CE
            int size = ::MultiByteToWideChar(CP_UTF8, 0, un, -1, 0, 0);
            if(! size)
            {
                hr = E_INVALIDARG;
                goto Cleanup;
            }
            unesc = ::SysAllocStringLen(0, size);
            CHK_MEM(unesc);
            ::MultiByteToWideChar(CP_UTF8, 0, un, -1, unesc, size);
            unesc[size] = 0;
#else
            UINT uiCodePage = CP_UTF8;
            int size = ::MultiByteToWideChar(uiCodePage, 0, un, -1, 0, 0);
        
            
            if(! size) //if this fails, try using ACP -- loc code might
                       // not be present 
            {
                uiCodePage = CP_ACP;
                size = ::MultiByteToWideChar(uiCodePage, 0, un, -1, 0, 0);       
            }
                
            if(! size) 
            {
                hr = E_INVALIDARG;
                goto Cleanup;
            }
            unesc = ::SysAllocStringLen(0, size);
            CHK_MEM(unesc);
            ::MultiByteToWideChar(uiCodePage, 0, un, -1, unesc, size);
            unesc[size] = 0;
#endif 
        }

        *unescaped = unesc;
        unesc = 0;
    }

Cleanup:
    ::SysFreeString(unesc);
    return hr;
}


