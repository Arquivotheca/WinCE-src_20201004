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
/*****************************************************************************/
/**                            Microsoft Windows                            **/
/*****************************************************************************/

/*

socket.c

FILE HISTORY:
    OmarM     14-Sep-2000

*/

#define WINSOCK_API_LINKAGE

#include <windows.h>


#include <types.h>
#include <wtypes.h>

#include <winerror.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cxport.h>

#include <pc_marshaler.hxx>

#include "wss_marshal.hxx"

#include "type.h"


#define INVALID_TLS_INDEX           (DWORD)-1L



ce::ioctl_proxy<> proxy(L"WSS1:", WSS_IOCTL_INVOKE, NULL);
static DWORD s_KSockTlsSlot = INVALID_TLS_INDEX;


int __WSAFDIsSet (
    SOCKET       fd,
    FD_SET FAR * set) {

    int i = (set->fd_count & 0xFFFF);

    while (i--)
        if (set->fd_array[i] == fd)
            return 1;

    return 0;

}   // __WSAFDIsSet()


WINSOCK_API_LINKAGE
int
WSAAPI
WSAStartup(
    IN WORD wVersionRequested,
    OUT LPWSADATA lpWSAData
    )
{
    int Status;
   
    if (proxy.call(WSS_STARTUP, ce::out(&Status), wVersionRequested,
        ce::out(byte_array_dw_aligned((uchar *)lpWSAData, sizeof(*lpWSAData))) 
        ))
        {
        Status = WSAEACCES;
        }
        

    return Status;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSACleanup(
    void
    )
{
    int Status;

    if (proxy.call(WSS_CLEANUP, ce::out(&Status)))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    return Status;
}


WINSOCK_API_LINKAGE
SOCKET
WSAAPI
socket(
    int af,
    int type,
    int protocol)
{
    SOCKET s;
    
    if (proxy.call(WSS_SOCKET, ce::out(&s), af, type, protocol))
        {
        SetLastError(WSAEACCES);
        s = INVALID_SOCKET;
        }
        
    return s;
}


WINSOCK_API_LINKAGE
int
WSAAPI
bind(
    IN SOCKET s,
    IN const struct sockaddr FAR * name,
    IN int namelen
    )
{
    int Status, Err;
    
    if (proxy.call(WSS_BIND, ce::out(&Status), ce::out(&Err), s, 
        sockaddr_buf((uchar *)name, namelen), 
        namelen))
        {
        Err = WSAEACCES;
        Status = SOCKET_ERROR;
        }

    if (SOCKET_ERROR == Status)
        SetLastError(Err);

    return Status;
}


WINSOCK_API_LINKAGE
SOCKET
WSAAPI
accept(
    IN SOCKET s,
    OUT struct sockaddr FAR * addr,
    IN OUT int FAR * addrlen
    )
{
    SOCKET NewSock;
    int cAddr = addrlen ? *addrlen : 0;

    if (proxy.call(WSS_ACCEPT, ce::out(&NewSock), s,
        ce::out(sockaddr_buf((uchar *)addr, cAddr)), 
        ce::out(addrlen)))
        {
        SetLastError(WSAEACCES);
        NewSock = INVALID_SOCKET;
        }

    return NewSock;
}


WINSOCK_API_LINKAGE
int
WSAAPI
closesocket(
    IN SOCKET s
    )
{
    int Status;
    
    if (proxy.call(WSS_CLOSESOCKET, ce::out(&Status), s))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }


    return Status;
}


WINSOCK_API_LINKAGE
int
WSAAPI
connect(
    IN SOCKET s,
    IN const struct sockaddr FAR * name,
    IN int namelen
    )
{
    int Status;
    
    if (proxy.call(WSS_CONNECT, ce::out(&Status), s, 
        sockaddr_buf((uchar *)name, namelen),  namelen))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    return Status;
}


WINSOCK_API_LINKAGE
int
WSAAPI
listen(
    IN SOCKET s,
    IN int backlog
    )
{
    int Status;
    
    if (proxy.call(WSS_LISTEN, ce::out(&Status), s, backlog))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    return Status;
}


WINSOCK_API_LINKAGE
int
WSAAPI
recv(
    IN SOCKET s,
    __out_bcount(len) OUT char FAR * buf,
    IN int len,
    IN int flags
    )
{
    int cRcvd = 0;

    proxy.call(WSS_RECV, ce::out(&cRcvd), s,
        ce::out(ce::psl_buffer((uchar *)buf, len)),
        len, flags);

    return cRcvd;
}

WINSOCK_API_LINKAGE
int
WSAAPI
recvfrom(
    IN SOCKET s,
    __out_bcount(len) OUT char FAR * buf,
    IN int len,
    IN int flags,
    OUT struct sockaddr FAR * from,
    IN OUT int FAR * fromlen
    )
{
    int cFrom = fromlen ? *fromlen : 0;
    int cRcvd = 0;

    proxy.call(WSS_RECVFROM, ce::out(&cRcvd), s, 
        ce::out(ce::psl_buffer((uchar *)buf, len)),
        len, flags,
        ce::out(sockaddr_buf((uchar *)from, cFrom)),
        ce::out(fromlen));

    return cRcvd;
        
}


WINSOCK_API_LINKAGE
int
WSAAPI
select(
    IN int nfds,
    IN OUT fd_set FAR * readfds,
    IN OUT fd_set FAR * writefds,
    IN OUT fd_set FAR *exceptfds,
    IN const struct timeval FAR * timeout
    )
{
    int     Status;
    u_int   cRead, cWrite, cExcept;
    fd_set  *pMyRead, *pMyWrite, *pMyExcept;

    cRead = cWrite = cExcept = 0;
    pMyRead = pMyWrite = pMyExcept = NULL;

    if (readfds)
        {
        cRead = readfds->fd_count + 1;
        pMyRead = readfds;
        }
    if (writefds)
        {
        cWrite = writefds->fd_count + 1;
        pMyWrite = writefds;
        }
    if (exceptfds)
        {
        cExcept = exceptfds->fd_count + 1;
        pMyExcept = exceptfds;
        }

    if (proxy.call(WSS_SELECT, ce::out(&Status),
        ce::out(ce::psl_buffer((int *) pMyRead, cRead)),
        ce::out(ce::psl_buffer((int *) pMyWrite, cWrite)),
        ce::out(ce::psl_buffer((int *) pMyExcept, cExcept)),
        timeout))
        {
        //SetLastError(WSAEACCES);
        //Status = SOCKET_ERROR;
        }

    return Status;

}


WINSOCK_API_LINKAGE
int
WSAAPI
send(
    IN SOCKET s,
    __in_bcount(len) IN const char FAR * buf,
    IN int len,
    IN int flags
    )
{
    int cSent;

    proxy.call(WSS_SEND, ce::out(&cSent), s,
        ce::psl_buffer((uchar *)buf, len), len, flags);

    return cSent;
}


WINSOCK_API_LINKAGE
int
WSAAPI
sendto(
    IN SOCKET s,
    IN const char FAR * buf,
    IN int len,
    IN int flags,
    IN const struct sockaddr FAR * to,
    IN int tolen
    )
{
    int cSent;

    proxy.call(WSS_SENDTO, ce::out(&cSent), s,
        ce::psl_buffer((uchar *)buf, len), len, flags,
        sockaddr_buf((uchar *)to, tolen), tolen);

    return cSent;
}


WINSOCK_API_LINKAGE
int
WSAAPI
setsockopt(
    IN SOCKET s,
    IN int level,
    IN int optname,
    __in_bcount(optlen) IN const char FAR * optval,
    IN int optlen
    )
{
    int Status;
    
    proxy.call(WSS_SETSOCKOPT, ce::out(&Status), s, level, optname,
        ce::psl_buffer((uchar *)optval, optlen), optlen);

    return Status;
}


WINSOCK_API_LINKAGE
int
WSAAPI
shutdown(
    IN SOCKET s,
    IN int how
    )
{
    int Status;
    
    if (proxy.call(WSS_SHUTDOWN, ce::out(&Status), s, how))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    return Status;
}


WINSOCK_API_LINKAGE
u_long
WSAAPI
htonl(
    IN u_long hostlong
    )
{
    return _byteswap_ulong(hostlong);
}

WINSOCK_API_LINKAGE
u_short
WSAAPI
htons(
    IN u_short hostshort
    )
{
    return _byteswap_ushort(hostshort);
}

WINSOCK_API_LINKAGE
u_long
WSAAPI
ntohl(unsigned long hostlong)
{
    return _byteswap_ulong(hostlong);
}

WINSOCK_API_LINKAGE
u_short
WSAAPI
ntohs(unsigned short hosts)
{
    return _byteswap_ushort(hosts);
}

extern "C" unsigned long internal_inet_addr (const char * cp);

WINSOCK_API_LINKAGE
unsigned long
WSAAPI
inet_addr (const char * cp) {
    return internal_inet_addr(cp);
}


WINSOCK_API_LINKAGE
void
WSAAPI
freeaddrinfo(
    IN struct addrinfo FAR * ai
    )
{
    LocalFree(ai);
    return;
}


SOCK_THREAD *
GetSockThread()
{
    SOCK_THREAD *pThread;

    // TODO: it seems the kernel team needs to fix this!
    /*
    if (INVALID_TLS_INDEX == s_KSockTlsSlot)
        {
        s_KSockTlsSlot = TlsAlloc();
        RETAILMSG(1, (TEXT("ws2k: GetSockThread: TlsAlloc failed!!!\r\n")));
        }

    if (INVALID_TLS_INDEX == s_KSockTlsSlot)
        {
        pThread = NULL;
        }
    else
        {
        }
        */
        
        pThread = (SOCK_THREAD *)TlsGetValue(s_KSockTlsSlot);
        if (! pThread)
            {
            pThread = (SOCK_THREAD *)LocalAlloc(LPTR, sizeof(*pThread));
            TlsSetValue (s_KSockTlsSlot, (LPVOID)pThread);
            }

    return pThread;
}


void NormalizeHostent(hostent *pHost)
{
    int i;

    pHost->h_name = (char *)pHost + (DWORD)pHost->h_name;
    pHost->h_aliases = (char **)
        ((char *)pHost + (DWORD)pHost->h_aliases);
    
    pHost->h_addr_list = (char **)
        ((char *)pHost + (DWORD)pHost->h_addr_list);
    
    for (i = 0; pHost->h_addr_list[i]; i++)
        pHost->h_addr_list[i] = (char *)pHost + (DWORD)pHost->h_addr_list[i];
}


WINSOCK_API_LINKAGE
struct hostent FAR *
WSAAPI
gethostbyaddr(
    IN const char FAR * addr,
    IN int len,
    IN int type
    )
{
    SOCK_THREAD *pThread;
    int         Err;
    hostent     *pHost = NULL;

    pThread = GetSockThread();
    if (pThread)
        {        
        if (ERROR_SUCCESS == proxy.call(WSS_GETHOSTBYADDR, 
            ce::out(&Err), ce::psl_buffer((uchar *)addr, len), len, type,
            ce::out(byte_array_dw_aligned((uchar *)pThread, sizeof(*pThread)))))
            {
            if (Err)
                {
                pHost = NULL;
                SetLastError(Err);
                }
            else
                {
                // pHost here so we can return non-NULL
                pHost = &pThread->GETHOST_host;
                NormalizeHostent(pHost);
                }
            }
        }
    else
        {
        SetLastError(WSAENOBUFS);
        }

    return pHost;
}


struct hostent *
gethostbyname(
    IN const char FAR * name
    )
{
    SOCK_THREAD *pThread;
    int         Err;
    hostent     *pHost = NULL;

    pThread = GetSockThread();
    if (pThread)
        {
        if (ERROR_SUCCESS == proxy.call(WSS_GETHOSTBYNAME, ce::out(&Err), name, 
            ce::out(byte_array_dw_aligned((uchar *)pThread, sizeof(*pThread)))))
        
            {
            if (Err)
                {
                pHost = NULL;
                SetLastError(Err);
                }
            else
                {
                // pHost here so we can return non-NULL
                pHost = &pThread->GETHOST_host;
                NormalizeHostent(pHost);
                }
            }
        }
    else
        {
        SetLastError(WSAENOBUFS);
        }

    return pHost;
}


WINSOCK_API_LINKAGE
int
WSAAPI
gethostname(
    OUT char FAR * name,
    IN int namelen
    )
{
    int Status;

    if (proxy.call(WSS_GETHOSTNAME, ce::out(&Status),
        ce::out(ce::psl_buffer(name, namelen)),
        namelen))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    return Status;
}


WINSOCK_API_LINKAGE
int
WSAAPI
getaddrinfo(
    IN const char FAR * nodename,
    IN const char FAR * servname,
    IN const struct addrinfo FAR * hints,
    OUT struct addrinfo FAR * FAR * res
    )
{
    int     Status = ERROR_SUCCESS;
    int     cBuf = 1024;
    DWORD   BasePtr;

    *res = (struct addrinfo *)LocalAlloc(LPTR, cBuf);
    if (! (*res))
        {
        SetLastError(WSAENOBUFS);
        return SOCKET_ERROR;
        }

    BasePtr = (DWORD)*res;
    if (proxy.call(WSS_GETADDRINFO, ce::out(&Status),
            nodename, servname, 
            byte_array_dw_aligned((uchar *)hints, sizeof(*hints)),
            ce::out(byte_array_dw_aligned((uchar *)*res, 1024)), 
            ce::out(&cBuf), BasePtr))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    if(ERROR_SUCCESS != Status)
    {
        LocalFree(*res);
        *res = NULL;
    }

    return Status;

}


WINSOCK_API_LINKAGE
int
WSAAPI
getnameinfo(
    IN const struct sockaddr FAR * sa,
    IN socklen_t salen,
    __out_bcount(hostlen) OUT char FAR * host,
    IN DWORD hostlen,
    OUT char FAR * serv,
    IN DWORD servlen,
    IN int flags
    )
{
    int Status = 0;

    if (proxy.call(WSS_GETNAMEINFO, ce::out(&Status),
            sockaddr_buf((uchar *)sa, salen), salen,
            ce::out(ce::psl_buffer(host, hostlen)), hostlen, 
            ce::out(ce::psl_buffer((uchar *)serv, servlen)), servlen,
            flags))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }


    return Status;

}


WINSOCK_API_LINKAGE
int
WSAAPI
getpeername(
    IN SOCKET s,
    OUT struct sockaddr FAR * name,
    IN OUT int FAR * namelen
    )
{
    int Status;

    if (proxy.call(WSS_GETPEERNAME, ce::out(&Status), s,
        ce::out(sockaddr_buf((uchar *)name, *namelen)), ce::out(namelen)))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    return Status;
}


WINSOCK_API_LINKAGE
struct protoent FAR *
WSAAPI
getprotobynumber(
    IN int number
    )
{
    SetLastError(WSANO_RECOVERY);
    return NULL;
}


WINSOCK_API_LINKAGE
struct protoent FAR *
WSAAPI
getprotobyname(
    IN const char FAR * name
    )
{
    SetLastError(WSANO_RECOVERY);
    return NULL;
}


WINSOCK_API_LINKAGE
struct servent FAR *
WSAAPI
getservbyport(
    IN int port,
    IN const char FAR * proto
    )
{
    SetLastError(WSANO_RECOVERY);
    return NULL;
}


WINSOCK_API_LINKAGE
struct servent FAR *
WSAAPI
getservbyname(
    IN const char FAR * name,
    IN const char FAR * proto
    )
{
    SetLastError(WSANO_RECOVERY);
    return NULL;
}


WINSOCK_API_LINKAGE
int
WSAAPI
getsockname(
    IN SOCKET s,
    OUT struct sockaddr FAR * name,
    IN OUT int FAR * namelen
    )
{
    int Status;

    if (proxy.call(WSS_GETSOCKNAME, ce::out(&Status), s,
        ce::out(sockaddr_buf((uchar *)name, *namelen)), ce::out(namelen)))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    return Status;
}


WINSOCK_API_LINKAGE
int
WSAAPI
getsockopt(
    IN SOCKET s,
    IN int level,
    IN int optname,
    __out_bcount(*optlen) OUT char FAR * optval,
    IN OUT int FAR * optlen
    )
{
    int Status;

    if (proxy.call(WSS_GETSOCKOPT, ce::out(&Status), s,
        level, optname, 
        ce::out(ce::psl_buffer((uchar *)optval, *optlen)), ce::out(optlen)))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    return Status;
}


WINSOCK_API_LINKAGE
char FAR *
WSAAPI
inet_ntoa(
    IN struct in_addr in
    )
{
    SOCK_THREAD        *pThread;
    int        i;
    register unsigned char *p;
    // gmd -make volatile to work around SA1100 optimizer bug
    volatile char    *pBuf, *p2Buf; 
    char            c, *pResult;

    pResult = NULL;

    pThread = GetSockThread();
    if (pThread) {
        pBuf = p2Buf = pResult = pThread->st_ntoa_buffer;
        p = (unsigned char *)&in;

        for (i=3; i >= 0; i--) {
            do {
                *pBuf++ = p[i] % 10 + '0';
            } while (p[i] /= 10);
            *pBuf++ = '.';
        }
        *(--pBuf) = '\0';    // gets rid of last period & ends string
        // now reverse it...
        for (--pBuf ; p2Buf < pBuf; pBuf--, p2Buf++) {
            c = *pBuf;
            *pBuf = *p2Buf;
            *p2Buf = c;
        }
    }
    return pResult;
}


WINSOCK_API_LINKAGE
int
WSAAPI
ioctlsocket(
    IN SOCKET s,
    IN long cmd,
    IN OUT u_long FAR * argp
    )
{
    int Status;
    
    if ((FIONBIO != cmd) && (FIONREAD != cmd))
        {
        SetLastError(WSAENOPROTOOPT);
        Status = SOCKET_ERROR;
        }
    else
        {
        // just to catch silly bugs do assignment
        volatile DWORD Arg = *argp;

        if (proxy.call(WSS_IOCTLSOCKET, ce::out(&Status), s, cmd,
            ce::out(argp)))
            {
            SetLastError(WSAEACCES);
            Status = SOCKET_ERROR;
            }
        }

    return Status;

}


WINSOCK_API_LINKAGE
int
WSAAPI
sethostname(
    IN char *pName,
    IN int cName)
{
    int Status;

    if (proxy.call(WSS_SETHOSTNAME, ce::out(&Status),
        ce::psl_buffer((char *)pName, cName), cName))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    return Status;
}


WINSOCK_API_LINKAGE
SOCKET
WSAAPI
WSASocketW(
    IN int af,
    IN int type,
    IN int protocol,
    IN LPWSAPROTOCOL_INFOW lpProtInfo,
    IN GROUP g,
    IN DWORD dwFlags
    )
{
    
    SOCKET s;
    
    proxy.call(WSS_WSASOCKET, ce::out(&s), af, type, protocol,
        ce::out(byte_array_dw_aligned((uchar *)lpProtInfo, sizeof(*lpProtInfo))),
        g, dwFlags);
    
    return s;

}


WINSOCK_API_LINKAGE
SOCKET
WSAAPI
WSAAccept(
    IN SOCKET s,
    OUT struct sockaddr FAR * addr,
    IN OUT LPINT addrlen,
    IN LPCONDITIONPROC lpfnCondition,
    IN DWORD_PTR dwCallbackData
    )
{
    SOCKET NewSock;

    //ASSERT(0);

    if (lpfnCondition)
        {
        SetLastError(WSAEOPNOTSUPP);
        NewSock = INVALID_SOCKET;
        }
    else
        {
        NewSock = accept(s, addr, addrlen);
        }

    return NewSock;

}


extern "C"
WINSOCK_API_LINKAGE
int
WSAAPI
WSAAsyncSelect(
    IN SOCKET s,
    IN HWND hWnd,
    IN u_int wMsg,
    IN long lEvent
    )
{
    SetLastError(WSAEOPNOTSUPP);
    return SOCKET_ERROR;
}


WINSOCK_API_LINKAGE
BOOL
WSAAPI
WSACloseEvent(
    IN WSAEVENT hEvent
    )
{
    // CloseHandle already sets last error if there is an error
    return CloseHandle(hEvent);
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSAConnect(
    IN SOCKET s,
    IN const struct sockaddr FAR * name,
    IN int namelen,
    IN LPWSABUF lpCallerData,
    OUT LPWSABUF lpCalleeData,
    IN LPQOS lpSQOS,
    IN LPQOS lpGQOS
    )
{
    int Status;

    //ASSERT(0);

    if (lpCallerData || lpCalleeData || lpSQOS || lpGQOS)
        {
        SetLastError(WSAEOPNOTSUPP);
        Status = SOCKET_ERROR;
        }
    else
        {
        Status = connect(s, name, namelen);
        }

    return Status;

}


WINSOCK_API_LINKAGE
WSAEVENT
WSAAPI
WSACreateEvent(
    void
    )
{
    // CreateEvent already sets last error if there is an error
    return CreateEvent(NULL, TRUE, FALSE, NULL);
}


void NormalizeNSP(DWORD cNsps, LPWSANAMESPACE_INFOW pNsp)
{
    while (cNsps-- > 0)
        {
        pNsp->lpszIdentifier = (LPWSTR)((DWORD)pNsp + 
            (DWORD)pNsp->lpszIdentifier);
        pNsp++;
        }
}


WINSOCK_API_LINKAGE
INT
WSAAPI
WSAEnumNameSpaceProvidersW(
    IN OUT LPDWORD              lpdwBufferLength,
    OUT    LPWSANAMESPACE_INFOW lpnspBuffer
    )
{
    int     Status;
    DWORD   BufLen = *lpdwBufferLength;
        
    if (proxy.call(WSS_ENUMNAMESPACE, ce::out(&Status), 
        ce::out(lpdwBufferLength),
        ce::out(byte_array_dw_aligned((uchar *)lpnspBuffer, 
            BufLen))))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    if (SOCKET_ERROR != Status)
        {
        ASSERT(Status >= 0);

        NormalizeNSP(Status, lpnspBuffer);
        }

    return Status;

}


WINSOCK_API_LINKAGE
int
WSAAPI
WSAEnumNetworkEvents(
    IN SOCKET s,
    IN WSAEVENT hEventObject,
    OUT LPWSANETWORKEVENTS lpNetworkEvents
    )
{
    int Status;
    DWORD ProcId = GetCurrentProcessId();

    if (proxy.call(WSS_ENUMNETWORKEVENTS, ce::out(&Status), s, 
        (int)hEventObject, ProcId,
        ce::out(byte_array_dw_aligned((uchar *)lpNetworkEvents, 
        sizeof(*lpNetworkEvents)))))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    return Status;

}


WINSOCK_API_LINKAGE
int
WSAAPI
WSAEnumProtocols(
    IN LPINT lpiProtocols,
    OUT LPWSAPROTOCOL_INFOW lpProtocolBuffer,
    IN OUT LPDWORD lpdwBufferLength
    )
{
    int     Status;
    int     cProtocols, i;
    DWORD   BufLen = *lpdwBufferLength;

    cProtocols = 0;
    if (lpiProtocols)
        {
        cProtocols = 1;
        for (i = 0; lpiProtocols[i]; i++)
            cProtocols++;
        }
        
    if (proxy.call(WSS_ENUMPROTOCOLS, ce::out(&Status), 
        ce::psl_buffer(lpiProtocols, cProtocols),
        ce::out(byte_array_dw_aligned((uchar *)lpProtocolBuffer, 
            BufLen)),
        ce::out(lpdwBufferLength)))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    return Status;

}


WINSOCK_API_LINKAGE
int
WSAAPI
WSAEventSelect(
    IN SOCKET s,
    IN WSAEVENT hEventObject,
    IN long lNetworkEvents
    )
{
    int Status;
    DWORD ProcId = GetCurrentProcessId();

    if (proxy.call(WSS_EVENTSELECT, ce::out(&Status), 
        s, (int)hEventObject, ProcId, lNetworkEvents))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    return Status;
}


int
WSAAPI
WSAGetLastError(
    void
    )
{
    return GetLastError();

}


WINSOCK_API_LINKAGE
BOOL
WSAAPI
WSAGetOverlappedResult(
    IN SOCKET s,
    IN LPWSAOVERLAPPED lpOverlapped,
    OUT LPDWORD lpcbTransfer,
    IN BOOL fWait,
    OUT LPDWORD lpdwFlags
    )
{
    SetLastError(WSAEOPNOTSUPP);
    return FALSE;
/*
    int Status;

    ASSERT(0);
    // BUGBUG: wsaoverlapped
    if (proxy.call(WSS_GETOVERLAPPEDRESULT, ce::out(&Status), s, 
        ce::psl_buffer((uchar *)lpOverlapped, sizeof(*lpOverlapped)),
        ce::out(lpcbTransfer), fWait, ce::out(lpdwFlags)))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    return Status;
*/
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSAHtonl(
    IN SOCKET s,
    IN u_long hostlong,
    OUT u_long FAR * lpnetlong
    )
{
    int Status;

    if (proxy.call(WSS_WSAHTONL, ce::out(&Status), 
        s, hostlong, ce::out(lpnetlong)))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    return Status;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSAHtons(
    IN SOCKET s,
    IN u_short hostshort,
    OUT u_short FAR * lpnetshort
    )
{
    int Status;

    // BUGBUG: why does ce::out(lpnetshort) complain!!!
    if (proxy.call(WSS_WSAHTONS, ce::out(&Status), 
        s, hostshort, ce::out((short *)lpnetshort)))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    return Status;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSAIoctl(
    IN SOCKET s,
    IN DWORD dwIoControlCode,
    IN LPVOID lpvInBuffer,
    IN DWORD cbInBuffer,
    __out_bcount_opt(cbOutBuffer) OUT LPVOID lpvOutBuffer,
    IN DWORD cbOutBuffer,
    OUT LPDWORD lpcbBytesReturned,
    IN LPWSAOVERLAPPED lpOverlapped,
    IN LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
    )
{
    int     Status;
    DWORD   ProcId = GetCurrentProcessId();
    DWORD   LastError;

    if (lpCompletionRoutine)
        {
        SetLastError(WSAEOPNOTSUPP);
        Status = SOCKET_ERROR;
        }
    else if (lpOverlapped && (SIO_ADDRESS_LIST_CHANGE != dwIoControlCode) &&
        (SIO_ROUTING_INTERFACE_CHANGE != dwIoControlCode))
        {
        SetLastError(WSAEOPNOTSUPP);
        Status = SOCKET_ERROR;
        }
    else 
        {
        if (proxy.call(WSS_WSAIOCTL, ce::out(&Status), ce::out(&LastError),
                s, dwIoControlCode, 
                byte_array_dw_aligned((uchar *)lpvInBuffer, cbInBuffer),
                ce::out(byte_array_dw_aligned((uchar *)lpvOutBuffer, cbOutBuffer)),
                ce::out(lpcbBytesReturned),
                ce::out(byte_array_dw_aligned((uchar *)lpOverlapped, 
                    sizeof(*lpOverlapped))),
                ProcId))
            {
            SetLastError(WSAEACCES);
            Status = SOCKET_ERROR;
            }
        else if (SOCKET_ERROR == Status)
            {
            SetLastError(LastError);
            }
        }

    return Status;
}


WINSOCK_API_LINKAGE
SOCKET
WSAAPI
WSAJoinLeaf(
    IN SOCKET s,
    IN const struct sockaddr FAR * name,
    IN int namelen,
    IN LPWSABUF lpCallerData,
    OUT LPWSABUF lpCalleeData,
    IN LPQOS lpSQOS,
    IN LPQOS lpGQOS,
    IN DWORD dwFlags
    )
{
    SetLastError(WSAEOPNOTSUPP);
    return INVALID_SOCKET;
}


WINSOCK_API_LINKAGE
INT
WSAAPI
WSANSPIoctl(
    IN  HANDLE           hLookup,
    IN  DWORD            dwControlCode,
    IN  LPVOID           lpvInBuffer,
    IN  DWORD            cbInBuffer,
    OUT LPVOID           lpvOutBuffer,
    IN  DWORD            cbOutBuffer,
    OUT LPDWORD          lpcbBytesReturned,
    IN  LPWSACOMPLETION  lpCompletion
    )
{
    ASSERT(0);
    // BUGBUG:
    return 0;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSANtohl(
    IN SOCKET s,
    IN u_long netlong,
    OUT u_long FAR * lphostlong
    )
{
    int Status;

    if (proxy.call(WSS_WSAHTONL, ce::out(&Status), 
        s, netlong, ce::out(lphostlong)))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    return Status;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSANtohs(
    IN SOCKET s,
    IN u_short netshort,
    OUT u_short FAR * lphostshort
    )
{
    int Status;

    if (proxy.call(WSS_WSAHTONS, ce::out(&Status), 
        s, netshort, ce::out((short *)lphostshort)))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    return Status;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSARecv(
    IN SOCKET s,
    IN OUT LPWSABUF lpBuffers,
    IN DWORD dwBufferCount,
    OUT LPDWORD lpNumberOfBytesRecvd,
    IN OUT LPDWORD lpFlags,
    IN LPWSAOVERLAPPED lpOverlapped,
    IN LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
    )
{
    int Status;

    if (lpOverlapped || lpCompletionRoutine)
        {
        SetLastError(WSAEOPNOTSUPP);
        Status = SOCKET_ERROR;
        }
    else if (dwBufferCount > MaxWsaBufs)
        {
        SetLastError(WSAENOBUFS);
        Status = SOCKET_ERROR;
        }
    else
        {
        char    *aBufs[MaxWsaBufs];
        u_long  aLens[MaxWsaBufs];
        DWORD   i;

        memset(aBufs, 0, sizeof(aBufs));
        memset(aLens, 0, sizeof(aLens));

        for (i = 0; i < dwBufferCount; i++)
            {
            aBufs[i] = lpBuffers[i].buf;
            aLens[i] = lpBuffers[i].len;
            }

        
        if (proxy.call(WSS_WSARECV, ce::out(&Status), 
            s,
            ce::out(ce::psl_buffer((uchar *)aBufs[0], aLens[0])),
            ce::out(ce::psl_buffer((uchar *)aBufs[1], aLens[1])),
            ce::out(ce::psl_buffer((uchar *)aBufs[2], aLens[2])),
            ce::out(ce::psl_buffer((uchar *)aBufs[3], aLens[3])),
            ce::out(ce::psl_buffer((uchar *)aBufs[4], aLens[4])),
            dwBufferCount,
            ce::out(lpNumberOfBytesRecvd), ce::out(lpFlags)))
            {
            SetLastError(WSAEACCES);
            Status = SOCKET_ERROR;
            }

        }

    return Status;

}


WINSOCK_API_LINKAGE
int
WSAAPI
WSARecvFrom(
    IN SOCKET s,
    IN OUT LPWSABUF lpBuffers,
    IN DWORD dwBufferCount,
    OUT LPDWORD lpNumberOfBytesRecvd,
    IN OUT LPDWORD lpFlags,
    OUT struct sockaddr FAR * lpFrom,
    IN OUT LPINT lpFromlen,
    IN LPWSAOVERLAPPED lpOverlapped,
    IN LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
    )
{
    int Status;

    if (lpOverlapped || lpCompletionRoutine)
        {
        SetLastError(WSAEOPNOTSUPP);
        Status = SOCKET_ERROR;
        }
    else if (dwBufferCount > MaxWsaBufs)
        {
        SetLastError(WSAENOBUFS);
        Status = SOCKET_ERROR;
        }
    else
        {
        char    *aBufs[MaxWsaBufs];
        u_long  aLens[MaxWsaBufs];
        DWORD   i;
        int     cFromLen;

        if (lpFrom && lpFromlen)
            {
            cFromLen = *lpFromlen;
            }
        else
            cFromLen = 0;

        memset(aBufs, 0, sizeof(aBufs));
        memset(aLens, 0, sizeof(aLens));

        for (i = 0; i < dwBufferCount; i++)
            {
            aBufs[i] = lpBuffers[i].buf;
            aLens[i] = lpBuffers[i].len;
            }

        
        if (proxy.call(WSS_WSARECVFROM, ce::out(&Status), 
            s,
            ce::out(ce::psl_buffer((uchar *)aBufs[0], aLens[0])),
            ce::out(ce::psl_buffer((uchar *)aBufs[1], aLens[1])),
            ce::out(ce::psl_buffer((uchar *)aBufs[2], aLens[2])),
            ce::out(ce::psl_buffer((uchar *)aBufs[3], aLens[3])),
            ce::out(ce::psl_buffer((uchar *)aBufs[4], aLens[4])),
            dwBufferCount,
            ce::out(lpNumberOfBytesRecvd), ce::out(lpFlags),
            ce::out(sockaddr_buf((uchar *)lpFrom, cFromLen)),
            ce::out(lpFromlen)))
            {
            SetLastError(WSAEACCES);
            Status = SOCKET_ERROR;
            }

        }

    return Status;

}


WINSOCK_API_LINKAGE
BOOL
WSAAPI
WSAResetEvent(
    IN WSAEVENT hEvent
    )
{
    return ResetEvent(hEvent);
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSASend(
    IN SOCKET s,
    IN LPWSABUF lpBuffers,
    IN DWORD dwBufferCount,
    OUT LPDWORD lpNumberOfBytesSent,
    IN DWORD dwFlags,
    IN LPWSAOVERLAPPED lpOverlapped,
    IN LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
    )
{
    int Status;

    if (lpOverlapped || lpCompletionRoutine)
        {
        SetLastError(WSAEOPNOTSUPP);
        Status = SOCKET_ERROR;
        }
    else if (dwBufferCount > MaxWsaBufs)
        {
        SetLastError(WSAENOBUFS);
        Status = SOCKET_ERROR;
        }
    else
        {
        char    *aBufs[MaxWsaBufs];
        u_long  aLens[MaxWsaBufs];
        DWORD   i;

        memset(aBufs, 0, sizeof(aBufs));
        memset(aLens, 0, sizeof(aLens));

        for (i = 0; i < dwBufferCount; i++)
            {
            aBufs[i] = lpBuffers[i].buf;
            aLens[i] = lpBuffers[i].len;
            }
        
        if (proxy.call(WSS_WSASEND, ce::out(&Status), 
            s,
            ce::psl_buffer((uchar *)aBufs[0], aLens[0]),
            ce::psl_buffer((uchar *)aBufs[1], aLens[1]),
            ce::psl_buffer((uchar *)aBufs[2], aLens[2]),
            ce::psl_buffer((uchar *)aBufs[3], aLens[3]),
            ce::psl_buffer((uchar *)aBufs[4], aLens[4]),
            dwBufferCount,
            ce::out(lpNumberOfBytesSent), dwFlags))
            {
            SetLastError(WSAEACCES);
            Status = SOCKET_ERROR;
            }

        }

    return Status;

}


WINSOCK_API_LINKAGE
int
WSAAPI
WSASendTo(
    IN SOCKET s,
    IN LPWSABUF lpBuffers,
    IN DWORD dwBufferCount,
    OUT LPDWORD lpNumberOfBytesSent,
    IN DWORD dwFlags,
    IN const struct sockaddr FAR * lpTo,
    IN int iTolen,
    IN LPWSAOVERLAPPED lpOverlapped,
    IN LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
    )
{
    int Status;

    if (lpOverlapped || lpCompletionRoutine)
        {
        SetLastError(WSAEOPNOTSUPP);
        Status = SOCKET_ERROR;
        }
    else if (dwBufferCount > MaxWsaBufs)
        {
        SetLastError(WSAENOBUFS);
        Status = SOCKET_ERROR;
        }
    else
        {
        char    *aBufs[MaxWsaBufs];
        u_long  aLens[MaxWsaBufs];
        DWORD   i;

        if (! lpTo)
            iTolen = 0;

        memset(aBufs, 0, sizeof(aBufs));
        memset(aLens, 0, sizeof(aLens));

        for (i = 0; i < dwBufferCount; i++)
            {
            aBufs[i] = lpBuffers[i].buf;
            aLens[i] = lpBuffers[i].len;
            }

        if (proxy.call(WSS_WSASENDTO, ce::out(&Status), 
            s,
            ce::psl_buffer((uchar *)aBufs[0], aLens[0]),
            ce::psl_buffer((uchar *)aBufs[1], aLens[1]),
            ce::psl_buffer((uchar *)aBufs[2], aLens[2]),
            ce::psl_buffer((uchar *)aBufs[3], aLens[3]),
            ce::psl_buffer((uchar *)aBufs[4], aLens[4]),
            dwBufferCount,
            ce::out(lpNumberOfBytesSent), dwFlags,
            sockaddr_buf((uchar *)lpTo, iTolen),
            iTolen))
            {
            SetLastError(WSAEACCES);
            Status = SOCKET_ERROR;
            }

        }

    return Status;

}


WINSOCK_API_LINKAGE
BOOL
WSAAPI
WSASetEvent(
    IN WSAEVENT hEvent
    )
{
    return SetEvent(hEvent);
}


WINSOCK_API_LINKAGE
void
WSAAPI
WSASetLastError(
    IN int iError
    )
{
    SetLastError(iError);
}


WINSOCK_API_LINKAGE
INT
WSAAPI
WSASetServiceW(
    IN LPWSAQUERYSETW lpqsRegInfo,
    IN WSAESETSERVICEOP essoperation,
    IN DWORD dwControlFlags
    )
{
    SetLastError(WSAEOPNOTSUPP);
    return SOCKET_ERROR;
}


WINSOCK_API_LINKAGE
INT
WSAAPI
WSAAddressToStringW(
    IN     LPSOCKADDR          lpsaAddress,
    IN     DWORD               dwAddressLength,
    IN     LPWSAPROTOCOL_INFOW lpProtocolInfo,
    IN OUT LPWSTR             lpszAddressString,
    IN OUT LPDWORD             lpdwAddressStringLength
    )
{
    int     Status;
    DWORD   cAddrString = *lpdwAddressStringLength;

    if (proxy.call(WSS_WSAADDRTOSTRING, ce::out(&Status),
        sockaddr_buf((uchar *)lpsaAddress, dwAddressLength),
        dwAddressLength,
        byte_array_dw_aligned((uchar *)lpProtocolInfo, sizeof(*lpProtocolInfo)),
        ce::out(ce::psl_buffer(lpszAddressString, cAddrString)),
        ce::out(lpdwAddressStringLength)))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    return Status;
    
}


WINSOCK_API_LINKAGE
INT
WSAAPI
WSAStringToAddressW(
    IN     LPWSTR              AddressString,
    IN     INT                 AddressFamily,
    IN     LPWSAPROTOCOL_INFOW lpProtocolInfo,
    OUT    LPSOCKADDR          lpAddress,
    IN OUT LPINT               lpAddressLength
    )
{
    int     Status;
    DWORD   cAddr = *lpAddressLength;

    if (proxy.call(WSS_WSASTRINGTOADDR, ce::out(&Status),
        (const WCHAR *)AddressString, AddressFamily,
        byte_array_dw_aligned((uchar *)lpProtocolInfo, sizeof(*lpProtocolInfo)),
        ce::out(sockaddr_buf((uchar *)lpAddress, cAddr)),
        ce::out(lpAddressLength)))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }

    return Status;
}


WINSOCK_API_LINKAGE
DWORD
WSAAPI
WSAWaitForMultipleEvents(
    IN DWORD cEvents,
    IN const WSAEVENT FAR * lphEvents,
    IN BOOL fWaitAll,
    IN DWORD dwTimeout,
    IN BOOL fAlertable
    )
{
    return WaitForMultipleObjects(cEvents, lphEvents, fWaitAll, dwTimeout);
}

DWORD
WSAControl(
    DWORD   Protocol,
    DWORD   Action,
    LPVOID  InputBuffer,
    LPDWORD InputBufferLength,
    LPVOID  OutputBuffer,
    LPDWORD OutputBufferLength
    )
{
    int     Status;
    uchar   *pInBuf, *pOutBuf;
    DWORD   cInBuf, cOutBuf, LastError;

    if (InputBufferLength && InputBuffer)
        {
        cInBuf = *InputBufferLength;
        pInBuf = (uchar *)InputBuffer;
        }
    else
        {
        cInBuf = 0;
        pInBuf = NULL;
        }

    if (OutputBufferLength && OutputBuffer)
        {
        cOutBuf = *OutputBufferLength;
        pOutBuf = (uchar *)OutputBuffer;
        }
    else
        {
        cOutBuf = 0;
        pOutBuf = NULL;
        }

    if (proxy.call(WSS_WSACONTROL, ce::out(&Status), ce::out(&LastError),
            Protocol, Action, 
            byte_array_dw_aligned(pInBuf, cInBuf),
            ce::out(&cInBuf),
            ce::out(byte_array_dw_aligned((uchar *)pOutBuf, cOutBuf)),
            ce::out(&cOutBuf)))
        {
        SetLastError(WSAEACCES);
        Status = SOCKET_ERROR;
        }
    else
        {
        if (InputBufferLength)
            *InputBufferLength = cInBuf;

        if (OutputBufferLength)
            *OutputBufferLength = cOutBuf;
        
        if (SOCKET_ERROR == Status)
            {
            SetLastError(LastError);
            }
        }

    return Status;

}



extern "C" {
BOOL __stdcall
DllEntry (HANDLE  hinstDLL, DWORD Op, LPVOID  lpvReserved) {

    switch (Op)
        {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls ((HMODULE)hinstDLL);
            s_KSockTlsSlot = TlsAlloc();
            if (INVALID_TLS_INDEX == s_KSockTlsSlot)
            {
                RETAILMSG(1, (TEXT("ws2k: DllEntry: TlsAlloc failed!!!\r\n")));
                return TRUE;
            }
            break;

        case DLL_PROCESS_DETACH:
            break;

        default:
            break;
        }
    return TRUE;
    
}    // dllentry()
}

