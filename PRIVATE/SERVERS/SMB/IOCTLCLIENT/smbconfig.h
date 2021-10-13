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
