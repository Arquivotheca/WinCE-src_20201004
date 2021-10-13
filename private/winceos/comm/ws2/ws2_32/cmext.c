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
/*++

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  
    CmExt.c

Abstract:  
    Winsock Connection Manager Extensions
    
Author:


Environment:


Revision History:
    
    August 2007 - Original code.
   
--*/

#include "winsock2p.h"
#include <CmNet.h>

BOOL g_WsCmExtLoaded;
BOOL g_WsCmExtUnavailable;
HANDLE g_hWsCmExt;

typedef BOOL (* PFN_WSCMEXTINIT)(void);
typedef void (* PFN_WSCMEXTDEINIT)(void);

typedef BOOL (* PFN_WSCMEXTCONNECTBYLIST)(
                        WsSocket *,
                        PSOCKET_ADDRESS_LIST,
                        LPDWORD,
                        LPSOCKADDR,
                        LPDWORD,
                        LPSOCKADDR,
                        const struct timeval*,
                        LPWSAOVERLAPPED
                        );
typedef BOOL (* PFN_WSCMEXTCONNECTBYNAME)(
                        WsSocket *,
                        LPWSTR,
                        LPWSTR,
                        LPDWORD,
                        LPSOCKADDR,
                        LPDWORD,
                        LPSOCKADDR,
                        const struct timeval*,
                        LPWSAOVERLAPPED
                        );
typedef CM_SESSION_HANDLE (* PFN_WSCMEXTGETCMSESSION)(WsSocket *);

PFN_WSCMEXTINIT          g_pfnWsCmExtInit;
PFN_WSCMEXTDEINIT        g_pfnWsCmExtDeinit;
PFN_WSCMEXTCONNECTBYLIST g_pfnWsCmExtConnectByList;
PFN_WSCMEXTCONNECTBYNAME g_pfnWsCmExtConnectByName;
PFN_WSCMEXTGETCMSESSION g_pfnWsCmExtGetCmSession;

BOOL IsWsCmExtLoaded(void)
{
    // Enforce CM concepts only on user mode processes.
    if (IsKModeAddr((DWORD)IsWsCmExtLoaded)) {
        return FALSE;
    }

    if (g_WsCmExtLoaded) {
        return TRUE;
    } else {
        if (g_WsCmExtUnavailable) {
            // Avoid continually attempting LoadLibrary/GetProcAddress
            return FALSE;
        }

        EnterCriticalSection(&v_DllCS);
        // Recheck under lock
        if (g_WsCmExtLoaded) {
            LeaveCriticalSection(&v_DllCS);
            return TRUE;
        }

        if (g_WsCmExtUnavailable) {
            LeaveCriticalSection(&v_DllCS);
            return FALSE;
        }

        g_hWsCmExt = LoadLibrary(L"WsCmExt.dll");
        if (g_hWsCmExt) {
            g_pfnWsCmExtInit            = (PFN_WSCMEXTINIT)         GetProcAddress(g_hWsCmExt, L"WsCmExtInit");
            g_pfnWsCmExtDeinit          = (PFN_WSCMEXTDEINIT)       GetProcAddress(g_hWsCmExt, L"WsCmExtDeinit");
            g_pfnWsCmExtAccept          = (PFN_WSCMEXTACCEPT)       GetProcAddress(g_hWsCmExt, L"WsCmExtAccept");
            g_pfnWsCmExtCloseSocket     = (PFN_WSCMEXTCLOSESOCKET)  GetProcAddress(g_hWsCmExt, L"WsCmExtCloseSocket");
            g_pfnWsCmExtConnect         = (PFN_WSCMEXTCONNECT)      GetProcAddress(g_hWsCmExt, L"WsCmExtConnect");
            g_pfnWsCmExtConnectByList   = (PFN_WSCMEXTCONNECTBYLIST)GetProcAddress(g_hWsCmExt, L"WsCmExtConnectByList");
            g_pfnWsCmExtConnectByName   = (PFN_WSCMEXTCONNECTBYNAME)GetProcAddress(g_hWsCmExt, L"WsCmExtConnectByName");
            g_pfnWsCmExtGetCmSession    = (PFN_WSCMEXTGETCMSESSION) GetProcAddress(g_hWsCmExt, L"WsCmExtGetCmSession");

            if (   (NULL == g_pfnWsCmExtInit)
                || (NULL == g_pfnWsCmExtDeinit)
                || (NULL == g_pfnWsCmExtAccept)
                || (NULL == g_pfnWsCmExtCloseSocket)
                || (NULL == g_pfnWsCmExtConnect)
                || (NULL == g_pfnWsCmExtConnectByList)
                || (NULL == g_pfnWsCmExtConnectByName)
                || (NULL == g_pfnWsCmExtGetCmSession)
                || (FALSE == g_pfnWsCmExtInit())
                )
            {
                g_WsCmExtUnavailable = TRUE;
                FreeLibrary(g_hWsCmExt);
            } else {
                g_WsCmExtLoaded = TRUE;
            }
        } else {
            g_WsCmExtUnavailable = TRUE;
        }

        LeaveCriticalSection(&v_DllCS);
    }

    return g_WsCmExtLoaded;
}

void UnloadWsCmExt(void)
{
    EnterCriticalSection(&v_DllCS);
    if (g_WsCmExtLoaded) {
        g_pfnWsCmExtDeinit();
        FreeLibrary(g_hWsCmExt);
        g_WsCmExtLoaded = FALSE;
        g_hWsCmExt = NULL;
        g_pfnWsCmExtCloseSocket = NULL;
    }
    LeaveCriticalSection(&v_DllCS);
}


BOOL
WSAConnectByName(
    SOCKET s,
    LPWSTR nodename,
    LPWSTR servicename,
    LPDWORD LocalAddressLength,
    LPSOCKADDR LocalAddress,
    LPDWORD RemoteAddressLength,
    LPSOCKADDR RemoteAddress,
    const struct timeval* timeout,
    LPWSAOVERLAPPED Reserved
    )
{
    WsSocket *pSock;
    int Err;
    BOOL Ret = FALSE;

    if (IsWsCmExtLoaded()) {
        Err = RefSocketHandle(s, &pSock);
        if (0 == Err) {
            Ret = g_pfnWsCmExtConnectByName(
                        pSock,
                        nodename,
                        servicename,
                        LocalAddressLength,
                        LocalAddress,
                        RemoteAddressLength,
                        RemoteAddress,
                        timeout,
                        Reserved
                        );
            DerefSocket(pSock);
        } else {
            SetLastError(Err);
        }
    } else {
        // attempted operation is not supported...
        SetLastError(WSAEOPNOTSUPP);
    }
    return Ret;
}

BOOL
WSAConnectByList(
    SOCKET s,
    PSOCKET_ADDRESS_LIST SocketAddressList,
    LPDWORD LocalAddressLength,
    LPSOCKADDR LocalAddress,
    LPDWORD RemoteAddressLength,
    LPSOCKADDR RemoteAddress,
    const struct timeval* timeout,
    LPWSAOVERLAPPED Reserved
    )
{
    WsSocket *pSock;
    int Err;
    BOOL Ret = FALSE;

    if (IsWsCmExtLoaded()) {
        Err = RefSocketHandle(s, &pSock);
        if (0 == Err) {
            Ret = g_pfnWsCmExtConnectByList(
                        pSock,
                        SocketAddressList,
                        LocalAddressLength,
                        LocalAddress,
                        RemoteAddressLength,
                        RemoteAddress,
                        timeout,
                        Reserved
                        );
            DerefSocket(pSock);
        } else {
            SetLastError(Err);
        }
    } else {
        // attempted operation is not supported...
        SetLastError(WSAEOPNOTSUPP);
    }
    return Ret;
}

CM_SESSION_HANDLE
WSAGetCmSession(
    SOCKET s
    )
{
    WsSocket *pSock;
    int Err;
    CM_SESSION_HANDLE CmSess;

    if (IsWsCmExtLoaded()) {
        Err = RefSocketHandle(s, &pSock);
        if (0 == Err) {
            CmSess = g_pfnWsCmExtGetCmSession(pSock);
            DerefSocket(pSock);
            if (NULL == CmSess) {
                SetLastError(WSA_NOT_ENOUGH_MEMORY);
            }
            return CmSess;
        } else {
            SetLastError(Err);
        }
    } else {
        SetLastError(WSAEOPNOTSUPP);
    }
    return NULL;
}
