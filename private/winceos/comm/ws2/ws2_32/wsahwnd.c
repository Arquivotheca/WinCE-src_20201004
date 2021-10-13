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
// wsahwnd.c
//
// async winsock implementation of
//  WSAAsyncSelect
//  WSAAsyncGetHostByName
//  WSAAsyncGetAddrInfo
//  WSACancelAsyncRequest
//

#include <winsock2p.h>
#include <ws2tcpip.h>

int WSACancelAsyncRequest (HANDLE hAsyncTaskHandle);
HANDLE WSAAsyncGetHostByName (HWND hWnd, unsigned int wMsg, const char FAR * name, char FAR * buf, int buflen );
int WSAAsyncSelect (SOCKET s, HWND hWnd, unsigned int wMsg, long lEvent );
HANDLE WSAAsyncGetAddrInfo(HWND hWnd, unsigned int uMsg, const char * name, unsigned short port, ADDRINFO **ppai);


DWORD cpyhent(char * lpDstBuffer, const PHOSTENT lpSrcHostent, int * lpBufSize );

typedef struct _WSAASYNC_SOCKET {
    struct _WSAASYNC_SOCKET * ws_next;
    SOCKET      ws_socket;
    HWND        ws_hwnd;
    u_int       ws_wmsg;
    long        ws_levents;
    long        ws_OneTimeEvents;
    WSAEVENT    ws_hevent;
} WSAASYNC_SOCKET, * PWSAASYNC_SOCKET;

typedef struct WSAREQ {
    struct WSAREQ  *pNext;
    HWND            hWnd;
    uint            wMsg;
    char           *pBuf;
    DWORD           dwBufLen;
    char           *pName;
    DWORD           Status;
    unsigned short  port;
    ADDRINFO      **ppAI;
} WSAREQ;

BOOL g_bWSAAsyncCSInit;
CRITICAL_SECTION g_WSAAsyncCS;
PWSAASYNC_SOCKET g_WSAAsyncSelectList;
int  g_cWSAAsyncSelectSockets;
BOOL g_bWSAAsyncSelectThreadRunning;
WSAEVENT g_WSAAsyncSelectSignal;
WSAREQ * g_WSAReqList;

HANDLE g_hCoreDll;
typedef BOOL (*pfnPostMessageW)(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
pfnPostMessageW g_pfnPostMessageW;

//
// Initialize async winsocket globals
//
BOOL WS2AsyncInit(void)
{
    BOOL bRet;

    if (WAIT_OBJECT_0 != WaitForAPIReady(SH_WMGR, 0)) {
        // Window manager not available
        SetLastError(WSASYSNOTREADY);
        return FALSE;
    }

    // Init async select globals
    if (FALSE == g_bWSAAsyncCSInit) {
        g_bWSAAsyncCSInit = TRUE;
        InitializeCriticalSection(&g_WSAAsyncCS);
        EnterCriticalSection(&g_WSAAsyncCS);
        g_WSAAsyncSelectSignal = CreateEvent(NULL, FALSE, FALSE, NULL);
        g_bWSAAsyncSelectThreadRunning = FALSE;
        g_cWSAAsyncSelectSockets = 0;
    } else {
        EnterCriticalSection(&g_WSAAsyncCS);
    }

    bRet = TRUE;
    // Init PostMessage function pointer
    if (NULL == g_hCoreDll) {
        g_hCoreDll = (HINSTANCE )GetModuleHandle(L"coredll");
        if (g_hCoreDll) {
            g_pfnPostMessageW = (pfnPostMessageW)GetProcAddress(g_hCoreDll, L"PostMessageW");
            if (NULL == g_pfnPostMessageW) {
                // Window manager APIs not in this config
                FreeLibrary(g_hCoreDll);
                g_hCoreDll = NULL;
                SetLastError(WSASYSNOTREADY);
                bRet = FALSE;
            }
        } else {
            // If we can't load coredll, it is likely an out of memory condition
            SetLastError(WSAENOBUFS);
            bRet = FALSE;
        }
    }
    LeaveCriticalSection(&g_WSAAsyncCS);

    return bRet;
}   // WS2AsyncInit

#define MAX_PORT_STRLEN 16
DWORD
WS2AsyncResolverThread(
    LPVOID lpContext
    )
{
    WSAREQ  *pReq = (WSAREQ *)lpContext;
    WSAREQ  *pPrev;
    WSAREQ  *pCurr;
    HOSTENT *pHostent = NULL;
    DWORD   lParm = 0;
    DWORD   dwLen;
    DWORD   dwStatus;

    if (pReq->pName) {
        if (pReq->pBuf) {
            // gethostbyname request
            DEBUGMSG(ZONE_MISC, (L"WS2:WS2AsyncResolverThread: calling gethostbyname(%a)\n", pReq->pName));
            pHostent = gethostbyname(pReq->pName);
            if (!pHostent) {
                lParm = GetLastError();
                DEBUGMSG(ZONE_MISC, (L"WS2:WS2AsyncResolverThread: gethostbyname(%a) failed %d\n", pReq->pName, lParm));
            }
        } else {
            // getaddrinfo request
            char szPort[MAX_PORT_STRLEN];
            StringCchPrintfA(szPort, MAX_PORT_STRLEN, "%hu", pReq->port);
            DEBUGMSG(ZONE_MISC, (L"WS2:WS2AsyncResolverThread: calling getaddrinfo(%a)\n", pReq->pName));
            if (getaddrinfo(pReq->pName, szPort, NULL, pReq->ppAI)) {
                lParm = GetLastError();
                DEBUGMSG(ZONE_MISC, (L"WS2:WS2AsyncResolverThread: getaddrinfo(%a) failed %d\n", pReq->pName, lParm));
            }
        }
    } else {
        lParm = WSAEINVAL; // No name specified!
    }

    EnterCriticalSection(&g_WSAAsyncCS);
    // Remove request from list
    if (pReq == g_WSAReqList) {
        g_WSAReqList = pReq->pNext;
    } else {
        pPrev = pCurr = g_WSAReqList;
        while (pCurr) {
            if (pCurr == pReq) {
                break;
            }
            pPrev = pCurr;
            pCurr = pCurr->pNext;
        }
        ASSERT(pCurr == pReq);
        pPrev->pNext = pReq->pNext;
    }
    LeaveCriticalSection(&g_WSAAsyncCS);

    if (pReq->Status != SOCKET_ERROR) {
        if (lParm) {
            lParm <<= 16;
        } else {
            if (pReq->pBuf) {
                // gethostbyname request
                dwLen = pReq->dwBufLen;
                dwStatus = cpyhent(pReq->pBuf, pHostent, (int *)&dwLen);
                // cpyhent might fail for lack of buffer space
                // but that's not our problem - just forward the condition to 
                // the async caller
                lParm = (dwStatus << 16)|dwLen;
            }
        }
        DEBUGMSG(ZONE_MISC, (L"WS2:WS2AsyncResolverThread: Posting results for %a (%d)\n", pReq->pName, lParm));
        g_pfnPostMessageW(pReq->hWnd, pReq->wMsg, (WPARAM)pReq, lParm);
    }

    if (pReq->pName) {
        LocalFree(pReq->pName);
    }
    LocalFree(pReq);
    return 0;
}   // WS2AsyncResolverThread

//
// Try to match the specified thread's priority to the current thread's priority
//
void
MatchCurrentThreadPriority(
    HANDLE hThd
    )
{
    CeSetThreadPriority(hThd, CeGetThreadPriority(GetCurrentThread()));
}


//
// Common worker for WSAAsyncGetHostByName and WSAGetAddrInfo
//
HANDLE
WS2StartAsyncRequest(
    HWND            hWnd,
    unsigned int    wMsg,
    const char *    name,
    char *          buf,
    int             buflen,
    unsigned short  port,
    ADDRINFO **     ppAI
    )
{
    HANDLE  hThread;
    DWORD   ThreadId;
    WSAREQ  *pReq;
    DWORD   dwLen;
    DWORD   Status;

    if (!WS2AsyncInit()) {
        return NULL;
    }

    Status = WSAENOBUFS;

    pReq = (WSAREQ *)LocalAlloc(LPTR, sizeof(*pReq));
    if (pReq) {
        pReq->hWnd = hWnd;
        pReq->wMsg = wMsg;
        pReq->pBuf = buf;
        pReq->dwBufLen = (DWORD)buflen;
        pReq->port = port;
        pReq->ppAI = ppAI;
        dwLen = strlen(name) + 1;
        pReq->pName = (char *)LocalAlloc(LPTR, dwLen);
        if (pReq->pName) {
            memcpy(pReq->pName, name, dwLen);
            pReq->Status = 0;
            EnterCriticalSection(&g_WSAAsyncCS);
            hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WS2AsyncResolverThread,
                                    pReq, CREATE_SUSPENDED, &ThreadId);

            if (hThread) {
                // put it in list of pending items
                pReq->pNext = g_WSAReqList;
                g_WSAReqList = pReq;
                LeaveCriticalSection(&g_WSAAsyncCS);
                DEBUGMSG(ZONE_MISC, (L"WS2:WS2StartAsyncRequest: Started search for %a\n", pReq->pName));
                MatchCurrentThreadPriority(hThread);
                ResumeThread(hThread);
                CloseHandle(hThread);
                return (HANDLE)pReq;
            } else {
                LeaveCriticalSection(&g_WSAAsyncCS);
                LocalFree(pReq->pName);
                LocalFree(pReq);
            }
        } else {
            LocalFree(pReq);
        }
    }

    DEBUGMSG(ZONE_MISC, (L"WS2:WS2StartAsyncRequest: Starting search for %a failed %d\n", name, Status));
    SetLastError(Status);
    return NULL;
}   // WS2StartAsyncRequest


HANDLE
WSAAsyncGetHostByName(
    HWND            hWnd,
    unsigned int    wMsg,
    const char *    name,
    char *          buf,
    int             buflen
    )
{
    return (HANDLE)WS2StartAsyncRequest(hWnd, wMsg, name, buf, buflen, 0, NULL);
}   // WSAAsyncGetHostByName


HANDLE
WSAAsyncGetAddrInfo(
    HWND            hWnd,
    unsigned int    uMsg,
    const char *    name,
    unsigned short  port,
    ADDRINFO **     ppAI
    )
{
    return (HANDLE)WS2StartAsyncRequest(hWnd, uMsg, name, NULL, 0, port, ppAI);
}   // WSAAsyncGetAddrInfo


int
WSACancelAsyncRequest(
    HANDLE hAsyncTaskHandle
    )
{
    WSAREQ  *pReq;
    int     Status;

    if (!WS2AsyncInit()) {
        return SOCKET_ERROR;
    }

    EnterCriticalSection(&g_WSAAsyncCS);
    pReq = g_WSAReqList;
    while (pReq) {
        if (pReq == (WSAREQ *)hAsyncTaskHandle) {
            break;
        }
        pReq = pReq->pNext;
    }

    if (pReq) {
        pReq->Status = (DWORD)SOCKET_ERROR;
        LeaveCriticalSection(&g_WSAAsyncCS);
        Status = 0;
    } else {
        LeaveCriticalSection(&g_WSAAsyncCS);
        Status = SOCKET_ERROR;
        SetLastError(WSAEINVAL);
    }

    return Status;
}   // WSACancelAsyncRequest


 
// cpyhent - copy a hostent structure to a new buffer.
// lpBuf - [out] destination buffer
// lpSrc - [in] source hostent buffer
// lpBufSize - [in/out] comes in with size of lpBuf, goes out with resulting hostent size
//
// here's what we have to copy...
//struct  hostent 
//{
//        char    * h_name;           /* official name of host */
//        char    * * h_aliases;  /* alias list */
//        short   h_addrtype;             /* host address type */
//        short   h_length;               /* length of address */
//        char    * * h_addr_list; /* list of addresses */
//};


// Need to dword-align the sub-structures inside the HOSTENT buffer,
// because some processors (actually, most other than x86) don't like
// unaligned pointers.
// I'm not too happy about this - seems we should be able to devise
// syntax that would tell the compiler what we want, without making
// this explicit noise about DWORD alignment.
// (e.g. this code will break on a device that requires QWORD alignment)
// the difficulty is that we're implementing a miniature memory allocator
// inside the HOSTENT buffer, where we carve out chunks of memory and
// advance a byte pointer.

#define DWORDALIGN(i) (((i)+3)&~3)
#define ALNPTR(ptr) ptr=(char*)DWORDALIGN((int)(ptr))

DWORD cpyhent(char * lpDstBuffer, const PHOSTENT lpSrcHostent, int * lpBufSize )
{
    int total_size;
    PHOSTENT lpDstHostent;
    char * lpWritePointer;
    int num_aliases;
    int num_addrs;
    int addr_size;
    char ** ptr;
    char ** src, **dst;
    int i;
    int len;
    
    total_size = DWORDALIGN(sizeof(HOSTENT));
    // first, step through the sources HOSTENT and figure out how big it is
    total_size += DWORDALIGN(strlen(lpSrcHostent->h_name) + 1);
    // count the number entries in h_aliases
    num_aliases = 0;
    for (ptr = lpSrcHostent->h_aliases; ptr && *ptr != NULL; ptr++) {
        total_size += DWORDALIGN(strlen(*ptr) + 1);
        num_aliases ++;
    }
    total_size += DWORDALIGN((num_aliases + 1) * sizeof(char *));
    // count the number of entries in h_addr_list
    num_addrs = 0;
    for (ptr = lpSrcHostent->h_addr_list; *ptr != NULL; ptr++) {
        num_addrs ++;
    }
    // each addr occupies (sizeof(char*) + h_length) bytes
    total_size += DWORDALIGN(num_addrs * (sizeof(char*) + lpSrcHostent->h_length));
    // now, check to see that there's enough room in the destination buffer
    if (total_size > *lpBufSize) {
        *lpBufSize = total_size; // inform async caller how big the buffer should be.
        return WSAENOBUFS;
    }
    // now we can actually start copying stuff, safe in the knowledge it will all fit
    // the lpWritePointer pointer tracks the current write position in the destination buffer
    
    lpWritePointer = lpDstBuffer;
    lpDstHostent = (PHOSTENT) lpDstBuffer;
    // copy the absolute data first, then the strings
    lpDstHostent->h_length = lpSrcHostent->h_length;
    lpDstHostent->h_addrtype = lpSrcHostent->h_addrtype;
    
    // account for the HOSTENT structure itself
    lpWritePointer += sizeof(HOSTENT);
    ALNPTR(lpWritePointer);
    lpDstHostent->h_name = lpWritePointer;
    len =strlen(lpSrcHostent->h_name) + 1;
    lpWritePointer += len;
    ALNPTR(lpWritePointer);
    ASSERT(lpWritePointer - lpDstBuffer <= *lpBufSize);
    // copy the host name
    StringCchCopyA(lpDstHostent->h_name, len, lpSrcHostent->h_name);
    // copy the aliases table
    if (lpSrcHostent->h_aliases)
    {
        lpDstHostent->h_aliases = (char **) lpWritePointer;
        lpWritePointer += (num_aliases+1) * sizeof(char*);
        ALNPTR(lpWritePointer);
        src = lpSrcHostent->h_aliases;
        dst = lpDstHostent->h_aliases;
        for (i = 0; i < num_aliases; i++) {
            *dst = lpWritePointer;
            len = strlen(*src) + 1;
            lpWritePointer += len;
            ALNPTR(lpWritePointer);
            ASSERT(lpWritePointer - lpDstBuffer <= *lpBufSize);
            StringCchCopyA(*dst, len, *src);
            dst++;
            src++;
        }
        // null-terminate the aliases table
        *dst = NULL;
        ASSERT(*src == NULL);
        // copy the address table
    }
    else
    {
        lpDstHostent->h_aliases = NULL;
    }
    
    // now copy the address table
    lpDstHostent->h_addr_list = (char **) lpWritePointer;
    lpWritePointer += (num_addrs+1) * sizeof(char*);
    ALNPTR(lpWritePointer);
    ASSERT(lpWritePointer - lpDstBuffer <= *lpBufSize);
    src = lpSrcHostent->h_addr_list;
    dst = lpDstHostent->h_addr_list;
    addr_size = lpSrcHostent->h_length;
    for (i = 0; i < num_addrs; i++) {
        *dst = lpWritePointer;
        lpWritePointer += addr_size;
        ALNPTR(lpWritePointer);
        ASSERT(lpWritePointer - lpDstBuffer <= *lpBufSize);
        memcpy(*dst, *src, addr_size);
        dst++;
        src++;
    }
    *dst = NULL;
    ASSERT(*src == NULL);
    return ERROR_SUCCESS; 
}   // cpyhent





//
// WSAAsyncSelect based on WSAEventSelect and WSAEnumNetworkEvents
//

//
// WS2FindWsaAsyncSocket: Find async socket structure based on either the associated
// socket handle or event handle
//
// Note: call with g_WSAAsyncCS taken
//
PWSAASYNC_SOCKET
WS2FindWsaAsyncSocket(
    SOCKET s,
    WSAEVENT e
    )
{
    PWSAASYNC_SOCKET pwsa;

    for (pwsa = g_WSAAsyncSelectList; pwsa; pwsa = pwsa->ws_next) {
        if (s) {
            if (pwsa->ws_socket == s) {
                return pwsa;
            }
        }
        if (e) {
            if (pwsa->ws_hevent == e) {
                return pwsa;
            }
        }
    }
    return NULL;
}   // WS2FindWsaAsyncSocket


// Note: call with g_WSAAsyncCS taken
void
WS2DeleteWsaAsyncSocket(
    PWSAASYNC_SOCKET pwsa
    )
{
    PWSAASYNC_SOCKET pcur, pprev;

    for (pcur = pprev = g_WSAAsyncSelectList; pcur;) {
        if (pcur == pwsa) {
            if (pcur == g_WSAAsyncSelectList) {
                g_WSAAsyncSelectList = pcur->ws_next;
            } else {
                pprev->ws_next = pcur->ws_next;
            }
            break;
        }
        pprev = pcur;
        pcur = pcur->ws_next;
    }
    if (pcur == pwsa) {
        g_cWSAAsyncSelectSockets--;
        WSAEventSelect(pwsa->ws_socket, pwsa->ws_hevent, 0);    // Cancel notifications
        CloseHandle(pwsa->ws_hevent);
        LocalFree(pwsa);
    }
}   // WS2DeleteWsaAsyncSocket


DWORD WS2AsyncSelectThread(LPVOID Nothing);

// Note: call with g_WSAAsyncCS taken
PWSAASYNC_SOCKET
WS2CreateWsaAsyncSocket(
    SOCKET  s,
    long    lEvents,
    LPDWORD pError
    )
{
    PWSAASYNC_SOCKET pwsa;
    HANDLE hThd;

    *pError = WSAENOBUFS;

    // Start thread if needed
    if (FALSE == g_bWSAAsyncSelectThreadRunning) {
        g_bWSAAsyncSelectThreadRunning = TRUE;
        hThd = CreateThread(NULL, 0, WS2AsyncSelectThread, NULL, 0, NULL);
        if (hThd) {
            MatchCurrentThreadPriority(hThd);
            CloseHandle(hThd);
        } else {
            g_bWSAAsyncSelectThreadRunning = FALSE;
            return NULL;
        }
    }

    if ((MAXIMUM_WAIT_OBJECTS-2) < g_cWSAAsyncSelectSockets) {
        return NULL;
    }

    pwsa = LocalAlloc(0, sizeof(WSAASYNC_SOCKET));
    if (pwsa) {
        if (WSA_INVALID_EVENT != (pwsa->ws_hevent = WSACreateEvent())) {
            if (0 == WSAEventSelect(s, pwsa->ws_hevent, lEvents|FD_CLOSE)) {
                g_cWSAAsyncSelectSockets++;
                *pError = 0;
                pwsa->ws_OneTimeEvents = 0xFFFFFFFF;
                return pwsa;
            } else {
                *pError = GetLastError();
                CloseHandle(pwsa->ws_hevent);
            }
        }
        LocalFree(pwsa);
    }
    return NULL;
}   // WS2CreateWsaAsyncSocket


//
// Process signalled network events by posting windows messages as appropriate
//
void
WS2ProcessWsaAsyncSocket(
    WSAEVENT hEvent
    )
{
    PWSAASYNC_SOCKET pwsa;
    WSANETWORKEVENTS NetworkEvents;
    long EventMask;
    DWORD lParam;
    int i;

    if (NULL == (pwsa = WS2FindWsaAsyncSocket(0, hEvent))) {
        //
        // The socket must have been closed and had WSAAsyncSelect(s,0,0,0) called
        // on it before this thread could process any of its events.
        //
        return;
    }

    if (SOCKET_ERROR == WSAEnumNetworkEvents(pwsa->ws_socket, pwsa->ws_hevent, &NetworkEvents)) {
        WS2DeleteWsaAsyncSocket(pwsa);
        return;
    }

    for (i = 0, EventMask = FD_READ; i < FD_MAX_EVENTS; i++, EventMask <<= 1) {
        if (EventMask & NetworkEvents.lNetworkEvents & pwsa->ws_levents & pwsa->ws_OneTimeEvents) {
            // Filter out any WSAEWOULDBLOCK notifications. The user would have already
            // seen WSAEWOULDBLOCK from a direct API function call.
            if (WSAEWOULDBLOCK == NetworkEvents.iErrorCode[i]) {
                continue;
            }

            if (EventMask == FD_CONNECT) {
                pwsa->ws_OneTimeEvents &= ~FD_CONNECT;
            }

            lParam = WSAMAKESELECTREPLY(EventMask, NetworkEvents.iErrorCode[i]);
            g_pfnPostMessageW(pwsa->ws_hwnd, pwsa->ws_wmsg, pwsa->ws_socket, lParam);
        }
    }

    if (FD_CLOSE & NetworkEvents.lNetworkEvents) {
        WS2DeleteWsaAsyncSocket(pwsa);
    }

}   // WS2ProcessWsaAsyncSocket


DWORD
WS2AsyncSelectThread(
    LPVOID Nothing
    )
{
    DWORD ret;
    HANDLE * pList = NULL;
    HANDLE * pHnd;
    PWSAASYNC_SOCKET pwsa;
    int cAlloc = 0;
    int cCurr = 1;

    EnterCriticalSection(&g_WSAAsyncCS);
    while (cCurr) {
        //
        // Count async sockets and allocate handle array if needed (include 1 extra for signal)
        //
        for (cCurr = 1, pwsa = g_WSAAsyncSelectList; pwsa; pwsa = pwsa->ws_next) {
            cCurr++;
        }

        if (1 == cCurr) {
            // No sockets to process
            goto wast_exit;
        }

        if (cCurr > cAlloc) {
            cAlloc = cCurr;
            if (pList) {
                LocalFree(pList);
            }
            pList = LocalAlloc(0, sizeof(pList) * cCurr);
            ASSERT(pList);
            if (NULL == pList) {
                goto wast_exit;
            }
        }

        pHnd = pList;
        *pHnd = g_WSAAsyncSelectSignal; // Signal event is first in the list
        pHnd++;

        //
        // Build array of event handles
        //
        for (pwsa = g_WSAAsyncSelectList; pwsa; pHnd++, pwsa = pwsa->ws_next) {
            *pHnd = pwsa->ws_hevent;
        }
        LeaveCriticalSection(&g_WSAAsyncCS);

        ret = WaitForMultipleObjects(cCurr, pList, FALSE, INFINITE);

        EnterCriticalSection(&g_WSAAsyncCS);
        if (WAIT_OBJECT_0 == ret) {
            // We got signalled by WSAAsyncSelect. Continuing will cause rebuild of event array
            continue;
        } else {
            if (ret < (DWORD)cCurr) {
                WS2ProcessWsaAsyncSocket(pList[ret]);
            } else {
                DEBUGMSG(1, (L"WS2AsyncSelectThread - WaitForMultipleObjects returned %d, error %d\n",
                             ret, GetLastError()));
                goto wast_exit;
            }
        }
    }

wast_exit:
    if (pList) {
        LocalFree(pList);
    }
    g_bWSAAsyncSelectThreadRunning = FALSE;
    LeaveCriticalSection(&g_WSAAsyncCS);
    return 0;
}   // WS2AsyncSelectThread


//
// Enable/disable notification of networks events via windows messages
//
int
WSAAPI
WSAAsyncSelect(
    IN SOCKET s,
    IN HWND hWnd,
    IN u_int wMsg,
    IN long lEvent
    )
{
    PWSAASYNC_SOCKET pwsa;
    int ret;
    DWORD dwLastError = 0;

    if (!WS2AsyncInit()) {
        return SOCKET_ERROR;
    }

    EnterCriticalSection(&g_WSAAsyncCS);

    pwsa = WS2FindWsaAsyncSocket(s, NULL);

    if (0 == lEvent) {
        // Remove socket from async system
        if (pwsa) {
            WS2DeleteWsaAsyncSocket(pwsa);
        } else {
            dwLastError = WSAEINVAL;
        }
        goto was_exit;
    }

    if (NULL == pwsa) {
        pwsa = WS2CreateWsaAsyncSocket(s, lEvent, &dwLastError);
        if (pwsa) {
            //
            // set socket to non-blocking
            //
            ret = 1;
            if (SOCKET_ERROR == ioctlsocket(s, FIONBIO, (u_long *)&ret)) {
                dwLastError = GetLastError();
                WS2DeleteWsaAsyncSocket(pwsa);
                goto was_exit;
            }

            pwsa->ws_socket = s;
            pwsa->ws_next   = g_WSAAsyncSelectList;
            g_WSAAsyncSelectList = pwsa;
        } else {
            goto was_exit;
        }
    } else {
        // Update the events if needed
        if (pwsa->ws_levents != lEvent) {
            dwLastError = WSAEventSelect(s, pwsa->ws_hevent, lEvent|FD_CLOSE);
            if (dwLastError) {
                goto was_exit;
            }
        }
    }

    pwsa->ws_hwnd   = hWnd;
    pwsa->ws_wmsg   = wMsg;
    pwsa->ws_levents = lEvent;

was_exit:
    if (dwLastError) {
        SetLastError(dwLastError);
        ret = SOCKET_ERROR;
    } else {
        SetEvent(g_WSAAsyncSelectSignal);
        ret = 0;
    }
    LeaveCriticalSection(&g_WSAAsyncCS);
    return ret;
}   // WSAAsyncSelect

