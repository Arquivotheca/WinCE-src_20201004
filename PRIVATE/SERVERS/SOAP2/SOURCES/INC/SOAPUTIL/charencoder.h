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
//      charencoder.h
//
// Contents:
//
//      CharEncoder, Encoding object declaration
//
//----------------------------------------------------------------------------------

#ifndef _CHARENCODER_H
#define _CHARENCODER_H

#include "codepage.h"

typedef HRESULT WideCharToMultiByteFunc(DWORD* pdwMode, CODEPAGE codepage, TCHAR * buffer, UINT *cch, BYTE * bytebuffer, UINT * cb);

struct EncodingEntry
{
    UINT    m_codepage;
    TCHAR * m_charset;
    UINT    m_maxCharSize;
    WideCharToMultiByteFunc   * m_pfnWideCharToMultiByte;
};

/**
 * 
 * An Encoder specifically for dealing with different encoding formats 
 * @version 1.0, 6/10/97
 */

class CharEncoder
{
    //
    // class CharEncoder is a utility class, makes sure no instance can be defined
    //
    private: virtual charEncoder() = 0;

public:

    static HRESULT getWideCharToMultiByteInfo(const TCHAR * charset, CODEPAGE *pcodepage,
                       WideCharToMultiByteFunc ** pfnWideCharToMultiByte, UINT *mCharSize);

    /**
     * Encoding functions: from Unicode to other encodings
     */

    static WideCharToMultiByteFunc wideCharToUcs2Littleendian;
    static WideCharToMultiByteFunc wideCharToUcs4Littleendian;
    static WideCharToMultiByteFunc wideCharToUtf8;
    static WideCharToMultiByteFunc wideCharToUtf7;
    static WideCharToMultiByteFunc wideCharToAnsiLatin1;
    static WideCharToMultiByteFunc wideCharToMultiByteWin32;
    static WideCharToMultiByteFunc wideCharToMultiByteMlang;

    static int getCharsetInfo(const TCHAR * charset, CODEPAGE * pcodepage, 
UINT * mCharSize);
    static int NeedsByteOrderMark(const WCHAR * charset, ULONG cchEncoding);


private:

    static const EncodingEntry  s_charsetInfo [];
};

#endif _CHARENCODER_H
