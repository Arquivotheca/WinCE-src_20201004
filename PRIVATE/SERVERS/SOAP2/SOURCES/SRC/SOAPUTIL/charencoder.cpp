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
//      charencoder.cpp
//
// Contents:
//
//      CharEncoder, Encoding object implementation
//
//----------------------------------------------------------------------------------

#include "headers.h"

#include <tchar.h>
#include <mlang.h>

#include "charencoder.h"

CAutoRefc<IMultiLanguage> g_pMultiLanguage;

HRESULT _EnsureMultiLanguage();

static CSpinlock 	mlanglock(x_MultiLang);


//
// Delegate other charsets to mlang
//
const EncodingEntry CharEncoder::s_charsetInfo [] = 
{
    { CP_1250,  _T("WINDOWS-1250"),       1, wideCharToMultiByteWin32 }, 
    { CP_1251,  _T("WINDOWS-1251"),       1, wideCharToMultiByteWin32 }, 
    { CP_1252,  _T("WINDOWS-1252"),       1, wideCharToMultiByteWin32 },
    { CP_1253,  _T("WINDOWS-1253"),       1, wideCharToMultiByteWin32 }, 
    { CP_1254,  _T("WINDOWS-1254"),       1, wideCharToMultiByteWin32 },
    { CP_1257,  _T("WINDOWS-1257"),       1, wideCharToMultiByteWin32 },
    { CP_UCS_4, _T("UCS-4"),              4, wideCharToUcs4Littleendian },
    { CP_UCS_2, _T("ISO-10646-UCS-2"),    2, wideCharToUcs2Littleendian },
    { CP_UCS_2, _T("UCS-2"),              2, wideCharToUcs2Littleendian },
    { CP_UCS_2, _T("UNICODE-2-0-UTF-16"), 2, wideCharToUcs2Littleendian },
    { CP_UCS_2, _T("UTF-16"),             2, wideCharToUcs2Littleendian },
    { CP_UTF_8, _T("UNICODE-1-1-UTF-8"),  3, wideCharToUtf8 },
    { CP_UTF_8, _T("UNICODE-2-0-UTF-8"),  3, wideCharToUtf8 },
    { CP_UTF_8, _T("UTF-8"),              3, wideCharToUtf8 }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: int CharEncoder::NeedsByteOrderMark(const WCHAR * charset, ULONG cchEncoding)
//
//  parameters:
//
//  description:
//        Determines whether byte order mark is needed
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
int CharEncoder::NeedsByteOrderMark(const WCHAR * charset, ULONG cchEncoding)
{
    for (int i = 10; i > 5; i--)
    {
        if (_tcslen(s_charsetInfo[i].m_charset) == cchEncoding &&
            _tcsnicmp(charset, s_charsetInfo[i].m_charset, cchEncoding) == 0)
        {
            return TRUE;
        }
    }
    return FALSE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: int CharEncoder::getCharsetInfo(const TCHAR * charset, CODEPAGE * pcodepage, UINT * mCharSize)
//
//  parameters:
//
//  description:
//        Returns charset information
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
int CharEncoder::getCharsetInfo(const TCHAR * charset, CODEPAGE * pcodepage, UINT * mCharSize)
{
    CPINFO cpinfo;

    for (int i = countof(s_charsetInfo) - 1; i >= 0; i--)
    {
        if (_tcsicmp(charset, s_charsetInfo[i].m_charset) == 0)
        {             
            //
            // test whether we can handle it locally or not
            //
            if (i > 5 || GetCPInfo(s_charsetInfo[i].m_codepage, &cpinfo))
            {
                *pcodepage = s_charsetInfo[i].m_codepage;
                *mCharSize = s_charsetInfo[i].m_maxCharSize;
                return i;
            }
            else
            {
                break;
            }
        }
    }

    //
    // delegate to MLANG then
    //
    MIMECSETINFO mimeCharsetInfo;
    HRESULT hr;

    hr = _EnsureMultiLanguage();
    if (hr == S_OK)
    {
        hr = g_pMultiLanguage->GetCharsetInfo((TCHAR*)charset, &mimeCharsetInfo);
        if (hr == S_OK)
        {
            *pcodepage = mimeCharsetInfo.uiInternetEncoding;
            if (GetCPInfo(*pcodepage, &cpinfo))
                *mCharSize = cpinfo.MaxCharSize;
            else // if we don't know the max size, assume a large size
                *mCharSize = 4;
            return -1;
        }
    }

    return -2;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CreateMultiLanguage(IMultiLanguage ** ppUnk)
//
//  parameters:
//
//  description:
//        Creates instance of MultiLanguage component
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CreateMultiLanguage(IMultiLanguage ** ppUnk)
{
    HRESULT hr = S_OK;

    if (*ppUnk == NULL)
    {
        CLocalSpinlock 	m_lock(&mlanglock);
        // perhaps someone took care of it already
        if (*ppUnk == NULL)
        {
            hr = CoCreateInstance(CLSID_CMultiLanguage, NULL, CLSCTX_INPROC_SERVER, 
                IID_IMultiLanguage, (void**)ppUnk);
        }
    }
    return hr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _EnsureMultiLanguage()
//
//  parameters:
//
//  description:
//        Ensures MultiLanguage component is created.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _EnsureMultiLanguage()
{
    return CreateMultiLanguage(&g_pMultiLanguage);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CharEncoder::getWideCharToMultiByteInfo
//
//  parameters:
//
//  description:
//        get information about a code page identified by <code> encoding </code>
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CharEncoder::getWideCharToMultiByteInfo(const WCHAR * charset, CODEPAGE * pcodepage, WideCharToMultiByteFunc ** pfnWideCharToMultiByte, UINT * mCharSize)
{
    HRESULT hr = S_OK;

    int i = getCharsetInfo(charset, pcodepage, mCharSize);
    if (i >= 0) // in our short list
    {
        switch (*pcodepage)
        {
        case CP_UCS_2:
            *pfnWideCharToMultiByte = wideCharToUcs2Littleendian;
            break;
        case CP_UCS_4:
            *pfnWideCharToMultiByte = wideCharToUcs4Littleendian;
            break;
        default:
            *pfnWideCharToMultiByte = s_charsetInfo[i].m_pfnWideCharToMultiByte;
            break;
        }
    }
    else if (i == -1) // delegate to MLANG
    {
        hr = g_pMultiLanguage->IsConvertible(CP_UTF_16, *pcodepage);
        if (hr == S_OK)
            *pfnWideCharToMultiByte = wideCharToMultiByteMlang;
        else
            hr = E_FAIL;
    }
    else
    {
        hr = E_FAIL;
    }

    return hr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CharEncoder::wideCharToUcs2Littleendian
//
//  parameters:
//
//  description:
//        Scans buffer and translates Unicode characters into Ucs2 little endian characters
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CharEncoder::wideCharToUcs2Littleendian(DWORD* , CODEPAGE , 
                                            TCHAR * buffer, 
                                            UINT *cch, 
                                            BYTE* bytebuffer, 
                                            UINT * cb)
{
    UINT num = (*cb) >> 1;
    if (num > *cch)
        num = *cch;
    ::memcpy(bytebuffer, buffer, num * sizeof(TCHAR));
    *cch = num;
    *cb = num << 1;
    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CharEncoder::wideCharToUcs4Littleendian(DWORD* pdwMode, CODEPAGE codepage, TCHAR * buffer, 
//
//  parameters:
//
//  description:
//        Scans buffer and translates Unicode characters into Ucs4 little endian characters
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CharEncoder::wideCharToUcs4Littleendian(DWORD* pdwMode, CODEPAGE codepage, TCHAR * buffer, 
                                              UINT *cch, BYTE* bytebuffer, UINT * cb)
{
    UINT num = (*cb) >> 2;
    if (num > *cch)
        num = *cch;

    for (UINT i = num; i > 0; i--)
    {
        *bytebuffer++ = (*buffer) & 0xFF;
        *bytebuffer++ = (*buffer) >> 8;
        *bytebuffer++ = 0;
        *bytebuffer++ = 0;
        buffer++;
    }
    *cch = num;
    *cb = num << 2;
    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CharEncoder::wideCharToUtf8(DWORD* pdwMode, CODEPAGE codepage, TCHAR * buffer, 
//
//  parameters:
//
//  description:
//        Scans buffer and translates Unicode characters into UTF8 characters
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CharEncoder::wideCharToUtf8(DWORD* pdwMode, CODEPAGE codepage, TCHAR * buffer, 
                                       UINT *cch, BYTE* bytebuffer, UINT * cb)
{
    UINT count = 0, num = *cch, m1 = *cb, m2 = m1 - 1, m3 = m2 - 1, m4 = m3 - 1;
    DWORD dw1;
    bool surrogate = false;

    for (UINT i = num; i > 0; i--)
    {
        DWORD dw = *buffer;

        if (surrogate) //  is it the second char of a surrogate pair?
        {
            if (dw >= 0xdc00 && dw <= 0xdfff)
            {
                // four bytes 0x11110xxx 0x10xxxxxx 0x10xxxxxx 0x10xxxxxx
                if (count < m4)
                    count += 4;
                else
                    break;
                ULONG ucs4 = (dw1 - 0xd800) * 0x400 + (dw - 0xdc00) + 0x10000;
                *bytebuffer++ = (byte)(( ucs4 >> 18) | 0xF0);
                *bytebuffer++ = (byte)((( ucs4 >> 12) & 0x3F) | 0x80);
                *bytebuffer++ = (byte)((( ucs4 >> 6) & 0x3F) | 0x80);
                *bytebuffer++ = (byte)(( ucs4 & 0x3F) | 0x80);
                surrogate = false;
                buffer++;
                continue;
            }
            else // Then dw1 must be a three byte character
            {
                if (count < m3)
                    count += 3;
                else
                    break;
                *bytebuffer++ = (byte)(( dw1 >> 12) | 0xE0);
                *bytebuffer++ = (byte)((( dw1 >> 6) & 0x3F) | 0x80);
                *bytebuffer++ = (byte)(( dw1 & 0x3F) | 0x80);
            }
            surrogate = false;
        }

        if (dw  < 0x80) // one byte, 0xxxxxxx
        {
            if (count < m1)
                count++;
            else
                break;
            *bytebuffer++ = (byte)dw;
        }
        else if ( dw < 0x800) // two WORDS, 110xxxxx 10xxxxxx
        {
            if (count < m2)
                count += 2;
            else
                break;
            *bytebuffer++ = (byte)((dw >> 6) | 0xC0);
            *bytebuffer++ = (byte)((dw & 0x3F) | 0x80);
        }
        else if (dw >= 0xd800 && dw <= 0xdbff) // Assume that it is the first char of surrogate pair
        {
            if (i == 1) // last wchar in buffer
                break;
            dw1 = dw;
            surrogate = true;
        }
        else // three bytes, 1110xxxx 10xxxxxx 10xxxxxx
        {
            if (count < m3)
                count += 3;
            else
                break;
            *bytebuffer++ = (byte)(( dw >> 12) | 0xE0);
            *bytebuffer++ = (byte)((( dw >> 6) & 0x3F) | 0x80);
            *bytebuffer++ = (byte)(( dw & 0x3F) | 0x80);
        }
        buffer++;
    }

    *cch = surrogate ? num - i - 1 : num - i;
    *cb = count;

    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CharEncoder::wideCharToMultiByteWin32(DWORD* pdwMode, CODEPAGE codepage, TCHAR * buffer, 
//
//  parameters:
//
//  description:
//        Scans buffer and translates Unicode characters into characters identified
//        by <code> codepage </>, using Win32 function WideCharToMultiByte for encoding 
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CharEncoder::wideCharToMultiByteWin32(DWORD* pdwMode, CODEPAGE codepage, TCHAR * buffer, 
                                              UINT *cch, BYTE* bytebuffer, UINT * cb)
{
    HRESULT hr = S_OK;
    BOOL fBadChar = false;
    *cb = ::WideCharToMultiByte(codepage, NULL, buffer, *cch, (char*)bytebuffer, *cb, NULL, &fBadChar);
    if (*cb == 0)
        hr = ::GetLastError();
    else if (fBadChar)
        hr = S_FALSE;
    return hr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CharEncoder::wideCharToMultiByteMlang(DWORD* pdwMode, CODEPAGE codepage, TCHAR * buffer, 
//
//  parameters:
//
//  description:
//        Scans buffer and translates Unicode characters into characters of charSet 
//        identified by <code> codepage </code>, using Mlang for encoding 
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CharEncoder::wideCharToMultiByteMlang(DWORD* pdwMode, CODEPAGE codepage, TCHAR * buffer, 
                                              UINT *cch, BYTE* bytebuffer, UINT * cb)
{
    HRESULT hr;
    if (FAILED(hr = _EnsureMultiLanguage()))
    {
        return hr;
    }
    if (FAILED(hr = g_pMultiLanguage->ConvertStringFromUnicode(pdwMode, codepage,
                                       buffer, cch, (char*)bytebuffer, cb )))
    {
        return hr;
    }
    return S_OK;
}

