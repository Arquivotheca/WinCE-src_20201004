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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include "StdAfx.h"
#include "utilities.h"

CUtilities::CUtilities(void)
{
}

CUtilities::~CUtilities(void)
{
}
void CUtilities::Bstr2Tchar(BSTR bstr, int lenBstr, TCHAR * pTchar, int lenTchar)
{
    int i      = 0;
    int maxLen = (lenBstr < lenTchar) ? lenBstr:lenTchar-1;

    for(i=0; i < maxLen; i++)
    {
        pTchar[i] = (TCHAR)bstr[i];
    }

    pTchar[i] = 0;
}

void CUtilities::Wstr2Tchar(LPCWSTR lpcwStr, int lenWstr, TCHAR * pTchar, int lenTchar)
{
    int i      = 0;
    int maxLen = (lenWstr < lenTchar) ? lenWstr:lenTchar-1;

    for(i=0; i < maxLen; i++)
    {
        pTchar[i] = (TCHAR)lpcwStr[i];
    }

    pTchar[i] = 0;
}

void CUtilities::Tchar2Wchar(LPCTSTR pTchar, int lenTchar, LPWSTR lpwStr, int lenWchar)
{
    int i      = 0;
    int maxLen = (lenTchar < lenWchar) ? lenTchar:lenWchar-1;

    for(i=0; i < maxLen; i++)
    {
        lpwStr[i] = (WCHAR)pTchar[i];
    }

    lpwStr[i] = 0;
}

