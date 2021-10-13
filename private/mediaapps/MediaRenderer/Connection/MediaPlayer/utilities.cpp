//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

// Utilities: Implementation of utilities

#include "stdafx.h"

HRESULT ReadRegistry(LPCWSTR wszKey, LPWSTR pdwData, DWORD* length)
{
    HRESULT hr = S_OK;
    long retval;
    HKEY hKey = NULL;

    retval = RegOpenKeyEx(HKEY_LOCAL_MACHINE, wszKey, 0, (REGSAM)NULL, &hKey);
    CHR( HRESULT_FROM_WIN32(retval) );

    retval = RegQueryValueEx(hKey, L"", NULL, NULL, (LPBYTE)pdwData, length);
    CHR( HRESULT_FROM_WIN32(retval) );

Error:
    RegCloseKey(hKey);
    return hr;
}
