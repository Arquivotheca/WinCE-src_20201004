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
#include <windows.h>

extern "C"
{

int
iswctype(
    wchar_t  c,
    wctype_t mask
    )
{
    WCHAR wc = c;
    WORD type;
    if (!GetStringTypeExW(LOCALE_SYSTEM_DEFAULT, CT_CTYPE1, &wc, 1, &type))
        {
        return 0;
        }
    return type & mask;
}

int
_isctype(
    int c,
    int mask
    )
{
    int len = IsDBCSLeadByte((BYTE)c) ? 2 : 1;
    WCHAR wc;
    if (!MultiByteToWideChar(CP_ACP, 0, (LPCSTR)&c, len, &wc, 1))
        {
        return 0;
        }
    return iswctype(wc, (wctype_t)mask);
}

wchar_t
towlower(
    wchar_t c
    )
{
    return (wchar_t)CharLowerW((WCHAR*)(DWORD)(WORD)c);
}

wchar_t
towupper(
    wchar_t c
    )
{
    return (wchar_t)CharUpperW((WCHAR*)(DWORD)(WORD)c);
}

}

