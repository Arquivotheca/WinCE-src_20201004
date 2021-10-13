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
//


#define BUILDING_FS_STUBS
#include <windows.h>
#include <acctid.h>

extern "C" LONG xxx_ADBCreateAccount(
    __in LPCWSTR pszName,
    DWORD dwAcctFlags,
    __reserved DWORD Reserved)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** ADBCreateAccount() called - Not supported in CE7.\r\n\r\n")));
    ASSERT(0);
    SetLastError(ERROR_NOT_SUPPORTED);
    return E_FAIL;
}

extern "C" LONG xxx_ADBDeleteAccount(__in LPCWSTR pszName)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** ADBDeleteAccount() called - Not supported in CE7.\r\n\r\n")));
    ASSERT(0);
    SetLastError(ERROR_NOT_SUPPORTED);
    return E_FAIL;
}

extern "C" LONG xxx_ADBGetAccountProperty(
    __in PCWSTR pszName,
    DWORD propertyId,
    __inout PDWORD pcbPropertyValue,
    __out_bcount_opt(*pcbPropertyValue) PVOID pValue)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** ADBGetAccountProperty() called - Not supported in CE7.\r\n\r\n")));
    ASSERT(0);
    SetLastError(ERROR_NOT_SUPPORTED);
    return E_FAIL;
}

extern "C" LONG xxx_ADBSetAccountProperties(
    __in PCWSTR pszName,
    DWORD cProperties,
    __in_ecount(cProperties) void *rgProperties)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** ADBSetAccountProperties() called - Not supported in CE7.\r\n\r\n")));
    ASSERT(0);
    SetLastError(ERROR_NOT_SUPPORTED);
    return E_FAIL;
}

extern "C" LONG xxx_ADBGetAccountSecurityInfo(
    __in PCWSTR pszName,
    __inout PDWORD pcbBuf,
    __out_bcount_opt(*pcbBuf) void *pSecurityInfo)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** ADBGetAccountSecurityInfo() called - Not supported in CE7.\r\n\r\n")));
    ASSERT(0);
    SetLastError(ERROR_NOT_SUPPORTED);
    return E_FAIL;
}

extern "C" LONG xxx_ADBAddAccountToGroup(
    __in PCWSTR pszName,
    __in PCWSTR pszGroupName)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** ADBAddAccountToGroup() called - Not supported in CE7.\r\n\r\n")));
    ASSERT(0);
    SetLastError(ERROR_NOT_SUPPORTED);
    return E_FAIL;
}

extern "C" LONG xxx_ADBRemoveAccountFromGroup(
    __in PCWSTR pszName,
    __in PCWSTR pszGroupName)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** ADBRemoveAccountFromGroup() called - Not supported in CE7.\r\n\r\n")));
    ASSERT(0);
    SetLastError(ERROR_NOT_SUPPORTED);
    return E_FAIL;
}

extern "C" LONG xxx_ADBEnumAccounts(
    DWORD dwFlags,
    __in_opt PCWSTR pszPrevName,
    __inout PDWORD pcchNextName,
    __out_ecount_opt(*pcchNextName) PWSTR pszNextName)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** ADBEnumAccounts() called - Not supported in CE7.\r\n\r\n")));
    ASSERT(0);
    SetLastError(ERROR_NOT_SUPPORTED);
    return E_FAIL;
}

extern "C" LONG xxx_ADBAccountIDFromName(__in PCWSTR pszName, __out PACCTID pAccountID) 
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** ADBAccountIDFromName() called - Not supported in CE7.\r\n\r\n")));
    ASSERT(0);
    SetLastError(ERROR_NOT_SUPPORTED);
    return E_FAIL;
}

extern "C" LONG xxx_ADBNameFromAccountID(
    __in PCACCTID pAccountID,
    __out_ecount_opt(*pcchSize) PWSTR pszName,
    __inout PDWORD pcchSize)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** ADBNameFromAccountID() called - Not supported in CE7.\r\n\r\n")));
    ASSERT(0);
    SetLastError(ERROR_NOT_SUPPORTED);
    return E_FAIL;
}

extern "C" LONG xxx_ADBEnumAccountsInGroup(
    __in PCWSTR pszGroupName,
    __in PCWSTR pszPrevName,
    __out_ecount_opt(*pcchNextName) PWSTR pszNextName,
    __in PDWORD pcchNextName)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** ADBEnumAccountsInGroup() called - Not supported in CE7.\r\n\r\n")));
    ASSERT(0);
    SetLastError(ERROR_NOT_SUPPORTED);
    return E_FAIL;
}
