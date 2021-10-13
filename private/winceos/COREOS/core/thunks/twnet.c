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
#include <windows.h>
#include <winnetwk.h>

/* Note that redir may not be loaded, in which case we return ERROR_NO_NETWORK */

DWORD
xxx_WNetAddConnection3W(HWND hwndOwner,LPNETRESOURCEW lpNetResource,
                        LPCWSTR lpUserName,LPTSTR lpPassword,BOOL dwFlags)
{
    if (WAIT_OBJECT_0 != WaitForAPIReady(SH_WNET, 0))
        return ERROR_NO_NETWORK;
    
    return WNetAddConnection3W(hwndOwner,lpNetResource,sizeof(NETRESOURCEW),lpUserName,lpPassword,dwFlags);
}

DWORD
xxx_WNetCancelConnection2W(LPCWSTR lpName,DWORD dwFlags, BOOL fForce)
{
    if (WAIT_OBJECT_0 != WaitForAPIReady(SH_WNET, 0))
        return ERROR_NO_NETWORK;
    
    return WNetCancelConnection2W(lpName,dwFlags, fForce);
}

DWORD
xxx_WNetDisconnectDialog(HWND hWnd, DWORD dwType)
{
    if (WAIT_OBJECT_0 != WaitForAPIReady(SH_WNET, 0))
        return ERROR_NO_NETWORK;
    
    return WNetDisconnectDialog(hWnd,dwType);
}

DWORD
xxx_WNetConnectionDialog1W(LPCONNECTDLGSTRUCTW lpConnectDlgStruc)
{
    if (WAIT_OBJECT_0 != WaitForAPIReady(SH_WNET, 0))
        return ERROR_NO_NETWORK;
    
    return WNetConnectionDialog1W(lpConnectDlgStruc,sizeof(CONNECTDLGSTRUCTW));
}

DWORD
xxx_WNetDisconnectDialog1W(LPDISCDLGSTRUCT lpDiscDlgStruc)
{
    if (WAIT_OBJECT_0 != WaitForAPIReady(SH_WNET, 0))
        return ERROR_NO_NETWORK;
    
    return WNetDisconnectDialog1W(lpDiscDlgStruc, sizeof(DISCDLGSTRUCT));
}

DWORD
xxx_WNetGetConnectionW(LPCWSTR lpLocalName,LPWSTR  lpRemoteName, LPDWORD lpnLength)
{
    DWORD remoteNameLength = 0;

    if (WAIT_OBJECT_0 != WaitForAPIReady(SH_WNET, 0))
        return ERROR_NO_NETWORK;
    
    if(lpnLength)
        remoteNameLength = *lpnLength;

    return WNetGetConnectionW(lpLocalName,lpRemoteName,remoteNameLength,lpnLength);
}

DWORD
xxx_WNetGetUniversalNameW(LPCWSTR lpLocalPath, DWORD   dwInfoLevel,
                      LPVOID lpBuffer, LPDWORD lpBufferSize)
{
    DWORD bufferSize = 0;

    if (WAIT_OBJECT_0 != WaitForAPIReady(SH_WNET, 0))
        return ERROR_NO_NETWORK;
    
    if(lpBufferSize)
        bufferSize = *lpBufferSize;

    return WNetGetUniversalNameW(lpLocalPath,dwInfoLevel,lpBuffer,bufferSize,lpBufferSize);
}

DWORD
xxx_WNetGetUserW(LPCWSTR lpName,LPWSTR lpUserName,LPDWORD lpnLength)
{
    DWORD userNameLength = 0;

    if (WAIT_OBJECT_0 != WaitForAPIReady(SH_WNET, 0))
        return ERROR_NO_NETWORK;
    
    if(lpnLength)
        userNameLength = *lpnLength;            

    return WNetGetUserW(lpName,lpUserName,userNameLength,lpnLength);
}



DWORD
xxx_WNetOpenEnumW(DWORD dwScope, DWORD dwType, DWORD dwUsage,
                 LPNETRESOURCEW lpNetResource, LPHANDLE lphEnum)
{
    if (WAIT_OBJECT_0 != WaitForAPIReady(SH_WNET, 0))
        return ERROR_NO_NETWORK;
    
    return WNetOpenEnumW(dwScope, dwType,dwUsage,lpNetResource,sizeof(NETRESOURCEW),lphEnum);
}

DWORD
xxx_WNetEnumResourceW(HANDLE hEnum, LPDWORD pCount, LPVOID pBuf,
                      LPDWORD pBufSize)
{
    DWORD bufferSize = 0;

    if (WAIT_OBJECT_0 != WaitForAPIReady(SH_WNET, 0))
        return ERROR_NO_NETWORK;
    
    if(pBufSize)
        bufferSize = *pBufSize;         

    return WNetEnumResourceW(hEnum,pCount,pBuf,bufferSize,pBufSize);
}


DWORD
xxx_WNetCloseEnum(HANDLE hEnum)
{
    if (WAIT_OBJECT_0 != WaitForAPIReady(SH_WNET, 0))
        return ERROR_NO_NETWORK;
    
    if (CloseHandle(hEnum))
        return ERROR_SUCCESS;
    else
        return GetLastError();
}

