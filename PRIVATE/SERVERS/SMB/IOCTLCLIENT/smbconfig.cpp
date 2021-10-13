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

#include <windows.h>
#include <SMBIOCTL.h>
#include <psl_marshaler.hxx>

ce::psl_proxy<> proxy(L"SMB0:", SMB_IOCTL_INVOKE, NULL);




HRESULT Change_ACL(const WCHAR *pShareName, const WCHAR *pACL, const WCHAR *pROACL)
{
    return proxy.call(IOCTL_CHANGE_ACL, pShareName, pACL, pROACL);
}


HRESULT Add_Share(const WCHAR *pName, DWORD dwType, const WCHAR *pPath, const WCHAR *pACL, const WCHAR *pROACL, const WCHAR *pDriver, const WCHAR *pComment)
{
      return proxy.call(IOCTL_ADD_SHARE, pName, dwType, pPath, pACL, pROACL, pDriver, pComment);
}


HRESULT Del_Share(const WCHAR *pName)
{
    return proxy.call(IOCTL_DEL_SHARE, pName);
}


HRESULT List_Connected_Users(WCHAR *pBuffer, UINT *puiLen)
{
    return proxy.call(IOCTL_LIST_USERS_CONNECTED, ce::psl_buffer(pBuffer, *puiLen / sizeof(WCHAR)),puiLen);
}

HRESULT QueryAmountTransfered(LARGE_INTEGER *pRead, LARGE_INTEGER *pWritten)
{
    return proxy.call(IOCTL_QUERY_AMOUNT_TRANSFERED, pRead, pWritten);
}

HRESULT NetUpdateAliases(const WCHAR *pBuffer, UINT uiLen)
{
    return proxy.call(IOCTL_NET_UPDATE_ALIASES, pBuffer, uiLen);
}

HRESULT NetUpdateIpAddress(const WCHAR *pBuffer, UINT uiLen)
{
    return proxy.call(IOCTL_NET_UPDATE_IP_ADDRESS, pBuffer, uiLen);
}

BOOL WINAPI DllMain(HANDLE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    switch(dwReason) {
        case DLL_PROCESS_ATTACH: {       
            break;
        }
        case DLL_PROCESS_DETACH: {
            break;
        }
        default:
            break;
    }
    return TRUE;
}
