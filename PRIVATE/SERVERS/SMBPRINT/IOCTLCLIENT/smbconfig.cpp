//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <windows.h>
#include <SMBIOCTL.h>
#include <psl_marshaler.hxx>

ce::psl_proxy<> proxy(L"SMB0:", SMB_IOCTL_INVOKE, NULL);




HRESULT Change_ACL(const WCHAR *pShareName, const WCHAR *pACL, const WCHAR *pROACL)
{
    proxy.call(IOCTL_CHANGE_ACL, pShareName, pACL, pROACL);
    return GetLastError();
}


HRESULT Add_Share(const WCHAR *pName, DWORD dwType, const WCHAR *pPath, const WCHAR *pACL, const WCHAR *pROACL, const WCHAR *pDriver, const WCHAR *pComment)
{
      proxy.call(IOCTL_ADD_SHARE, pName, dwType, pPath, pACL, pROACL, pDriver, pComment);
      return GetLastError();
}


HRESULT Del_Share(const WCHAR *pName)
{
    proxy.call(IOCTL_DEL_SHARE, pName);
    return GetLastError();
}


HRESULT List_Connected_Users(WCHAR *pBuffer, UINT *puiLen)
{
    proxy.call(IOCTL_LIST_USERS_CONNECTED, pBuffer, puiLen);
    return GetLastError();
}

HRESULT QueryAmountTransfered(LARGE_INTEGER *pRead, LARGE_INTEGER *pWritten)
{
    proxy.call(IOCTL_QUERY_AMOUNT_TRANSFERED, pRead, pWritten);
    return GetLastError();
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
