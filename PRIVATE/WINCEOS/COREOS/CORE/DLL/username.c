//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#define BUILDING_FS_STUBS
#include <windows.h>
#include <credmgr.h>

static HANDLE hSecur32 = NULL;

PFNCECREDREAD pfnCeCredRead = NULL;
PFNCECREDFREE pfnCeCredFree = NULL;

BOOLEAN
WINAPI
xxx_GetUserNameEx(
    EXTENDED_NAME_FORMAT NameFormat,
    LPWSTR lpNameBuffer,
    PULONG nSize
    ) {
    PCECREDENTIAL pCred = NULL;
    BOOL bRet = TRUE;
    DWORD nLen;

    if (NameFormat == NameWindowsCeLocal)
        return GetUserInformation(lpNameBuffer, nSize, USERINFO_NAME);

    if (NameFormat != NameSamCompatible) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (hSecur32 == NULL)
        hSecur32 = LoadLibrary (L"secur32.dll");

    if (hSecur32 == NULL) {
        SetLastError (ERROR_CALL_NOT_IMPLEMENTED);
        return FALSE;
    }

    if (pfnCeCredRead == NULL)
        pfnCeCredRead = (PFNCECREDREAD)GetProcAddress (hSecur32, L"CeCredRead");

    if (pfnCeCredFree == NULL)
        pfnCeCredFree = (PFNCECREDFREE)GetProcAddress (hSecur32, L"CeCredFree");

    if (! (pfnCeCredRead && pfnCeCredFree)) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (! pfnCeCredRead (NULL, CRED_TYPE_DOMAIN_PASSWORD, 0, &pCred))
        return FALSE;

    nLen = wcslen (pCred->UserName) + 1;

    if (lpNameBuffer) {
        if (nLen > *nSize) {
            SetLastError (ERROR_INSUFFICIENT_BUFFER);
            bRet = FALSE;
        } else
            memcpy (lpNameBuffer, pCred->UserName, nLen * sizeof(WCHAR));
    }

    *nSize = nLen;
    pfnCeCredFree (pCred);

    return bRet;
}

