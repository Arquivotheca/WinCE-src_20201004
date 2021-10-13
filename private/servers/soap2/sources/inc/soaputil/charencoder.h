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
    private: virtual int charEncoder() = 0;

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
