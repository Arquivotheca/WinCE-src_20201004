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
#pragma once
#include "wtypes.h"

class CUtilities
{
public:
    CUtilities(void);
    ~CUtilities(void);
    static void Bstr2Tchar(BSTR bstr, int lenBstr, TCHAR * pTchar, int lenTchar);
    static void Wstr2Tchar(LPCWSTR lpcwStr, int lenWstr, TCHAR * pTchar, int lenTchar);
    static void Tchar2Wchar(LPCTSTR pTchar, int lenTchar, LPWSTR lpwStr, int lenWchar);
};

