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
#define BUILDING_FS_STUBS
#include <windows.h>
#include <credmgr.h>

static HMODULE hSecur32 = NULL;

PFNCECREDREAD pfnCeCredRead = NULL;
PFNCECREDFREE pfnCeCredFree = NULL;

extern "C"
BOOLEAN
WINAPI
xxx_GetUserNameEx(
    EXTENDED_NAME_FORMAT NameFormat,
    LPWSTR lpNameBuffer,
    PULONG nSize
    ) {
    PCECREDENTIAL pCred = NULL;
    BOOLEAN bRet = true;
    DWORD nLen;

    if (NameFormat == NameWindowsCeLocal) {
        return (GetUserInformation_Trap(lpNameBuffer, nSize, USERINFO_NAME)) ? true : false;
    }

    if (NameFormat != NameSamCompatible) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return false;
    }

    if (hSecur32 == NULL)
        hSecur32 = LoadLibrary (L"secur32.dll");

    if (hSecur32 == NULL) {
        SetLastError (ERROR_CALL_NOT_IMPLEMENTED);
        return false;
    }

    if (pfnCeCredRead == NULL)
        pfnCeCredRead = (PFNCECREDREAD)GetProcAddress (hSecur32, L"CeCredRead");

    if (pfnCeCredFree == NULL)
        pfnCeCredFree = (PFNCECREDFREE)GetProcAddress (hSecur32, L"CeCredFree");

    if (! (pfnCeCredRead && pfnCeCredFree)) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return false;
    }

    if (! pfnCeCredRead (NULL, CRED_TYPE_DOMAIN_PASSWORD, 0, &pCred))
        return false;

    nLen = wcslen (pCred->UserName) + 1;

    if (lpNameBuffer) {
        if (nLen > *nSize) {
            SetLastError (ERROR_INSUFFICIENT_BUFFER);
            bRet = false;
        } else
            memcpy (lpNameBuffer, pCred->UserName, nLen * sizeof(WCHAR));
    }

    *nSize = nLen;
    pfnCeCredFree (pCred);

    return bRet;
}

