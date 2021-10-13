//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef SMBCONFIG_H
#define SMBCONFIG_H

HRESULT Change_ACL(const WCHAR *pShareName, const WCHAR *pACL, const WCHAR *pROACL);
HRESULT Add_Share(const WCHAR *pName, DWORD dwType, const WCHAR *pPath, const WCHAR *pACL, const WCHAR *pROACL, const WCHAR *pDriver, const WCHAR *pComment);
HRESULT Del_Share(const WCHAR *pName);
HRESULT List_Connected_Users(WCHAR *pBuffer, UINT *puiLen);
HRESULT QueryAmountTransfered(LARGE_INTEGER *pRead, LARGE_INTEGER *pWritten);
HRESULT NetUpdateAliases(const WCHAR *pBuffer, UINT uiLen);
HRESULT NetUpdateIpAddress(const WCHAR *pBuffer, UINT uiLen);

typedef HRESULT  (*PFNChange_ACL)(const WCHAR *pShareName, const WCHAR *pACL, const WCHAR *pROACL);
typedef HRESULT  (*PFNAdd_Share)(const WCHAR *pName, DWORD dwType, const WCHAR *pPath, const WCHAR *pACL, const WCHAR *pROACL, const WCHAR *pDriver, const WCHAR *pComment);
typedef HRESULT  (*PFNDel_Share)(const WCHAR *pName);
typedef HRESULT  (*PFNList_Connected_Users)(WCHAR *pBuffer, UINT *puiLen);
typedef HRESULT  (*PFNQueryAmountTransfered)(LARGE_INTEGER *pRead, LARGE_INTEGER *pWritten);
typedef HRESULT  (*PFNNetUpdateAliases)(const WCHAR *pBuffer, UINT uiLen);
typedef HRESULT  (*PFNNetUpdateIpAddress)(const WCHAR *pBuffer, UINT uiLen);

#endif
