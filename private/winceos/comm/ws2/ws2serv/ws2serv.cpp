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
    kspeeddrv.cpp

Abstract:  
    Kernel mode driver that starts kspeeddll.dll
    
Author:


Environment:


Revision History:
    
    November 2005:: Original code.
   
--*/

#define WINSOCK_API_LINKAGE

#include <windows.h>


#include <types.h>
#include <wtypes.h>
#include <ntcompat.h>

#include <winerror.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cxport.h>

#include <pc_marshaler.hxx>

#include "wss_marshal.hxx"

#include "type.h"
//
//    Debug Zones.
//

#ifdef DEBUG
DBGPARAM    dpCurSettings = 
{
    TEXT("Ws2Serv"), 
    {
        TEXT("Init"),        
        TEXT("Undefined"),
        TEXT("Undefined"),
        TEXT("Undefined"),
        TEXT("Undefined"),
        TEXT("Undefined"),
        TEXT("Undefined"),
        TEXT("Undefined"),
        TEXT("Undefined"),
        TEXT("Undefined"),
        TEXT("Interface"),
        TEXT("Misc"),
        TEXT("Alloc"),        
        TEXT("Function"),        
        TEXT("Warning"),
        TEXT("Error") 
    },

    0x0000C001
};

#define ZONE_INIT        DEBUGZONE(0)
#define ZONE_BIND        DEBUGZONE(1)
#define ZONE_CONNECT    DEBUGZONE(2)
#define ZONE_EVENT        DEBUGZONE(3)
#define ZONE_RECV        DEBUGZONE(4)
#define ZONE_SEND        DEBUGZONE(5)
#define ZONE_SOCKET        DEBUGZONE(6)
#define ZONE_LISTEN        DEBUGZONE(7)
#define ZONE_NAMES      DEBUGZONE(8)
#define ZONE_BUFFER        DEBUGZONE(9)
#define ZONE_INTERFACE    DEBUGZONE(10)
#define ZONE_MISC        DEBUGZONE(11)
#define ZONE_ALLOC        DEBUGZONE(12)
#define ZONE_FUNCTION    DEBUGZONE(13)
#define ZONE_WARN        DEBUGZONE(14)
#define ZONE_ERROR        DEBUGZONE(15)

#endif


extern "C" {

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT   pDriverObject,
    IN PUNICODE_STRING  pRegistryPath)
{
    return STATUS_SUCCESS;
    
}   //  DriverEntry()


BOOL
DllEntry (HANDLE hinstDLL, DWORD Op, PVOID lpvReserved)
{
    BOOL Status = TRUE;

    switch (Op) 
    {
        case DLL_PROCESS_ATTACH:
#ifdef DEBUG            
            DEBUGREGISTER((HINSTANCE)hinstDLL);
#endif
            DisableThreadLibraryCalls((HMODULE) hinstDLL);
            break;

        case DLL_PROCESS_DETACH:            
            break;

        case DLL_THREAD_DETACH :
            break;

        case DLL_THREAD_ATTACH :
            break;
            
        default :
            break;
    }
    return Status;

}    // DllEntry()


DWORD 
WSS_Init (DWORD Index) 
{
    #define        VALID_DEVICE_CONTEXT    0x12345678
    
    return (DWORD)VALID_DEVICE_CONTEXT;

}    //    KDM_Init()


BOOL 
WSS_Deinit(DWORD hDeviceContext) 
{
    DEBUGMSG (ZONE_INIT, (TEXT("WS2SERV:: KDM_Deinit()..\r\n")));

    return TRUE;    

}    //    KDM_Deinit()


DWORD 
WSS_Open (DWORD hDeviceContext , DWORD dwAccessCode , DWORD dwShareMode)
{
    //
    //  It does not matter..
    //

    return 1;    

}    //    KDM_Open()


BOOL
WSS_Close (DWORD hOpenContext)  
{    
    return TRUE;
    
}    //    KDM_Close()


 DWORD
WSS_Write (DWORD hOpenContext , LPCVOID pSourceBytes , DWORD dwNumberOfBytes) 
{
    //
    //  Not supported..
    //

    return 0;

}    //    KDM_Write()
 

DWORD 
WSS_Read (DWORD hOpenContext, LPVOID pBuffer, DWORD dwCount) 
{
    //
    //  Not supported..
    //

    return 0;


}    //    KDM_Read
 

DWORD 
WSS_Seek (DWORD hOpenContext , long lAmount, WORD wType) 
{
    //
    //  Not supported..
    //

    return (DWORD)-1;

}    //    KDM_Seek()

}   // extern "C"

WINSOCK_API_LINKAGE
DWORD
WSAAPI
WSS_Startup(
    OUT int                     *pReturn, 
    IN WORD                     Version,
    OUT byte_array_dw_aligned   buf)
{
    WSAData *pWsaData;
    int     Status;

    if (buf.count() == 0)
        pWsaData = NULL;
    else
        pWsaData = (WSAData*)buf.buffer();

    Status = WSAStartup(Version, pWsaData);
    *pReturn = Status;
    
    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Cleanup(int *pReturn) {
    int Status;

    Status = WSACleanup();
    *pReturn = Status;
    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Socket(int *pSocket, int af, int type, int protocol) {
    SOCKET  s;

    DEBUGMSG(ZONE_INTERFACE, (TEXT("WSS_Socket\r\n")));
    s = socket(af, type, protocol);
    *pSocket = s;
    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
Bind(
    IN SOCKET s,
    IN const struct sockaddr FAR * name,
    IN int namelen
    )
{
    const struct sockaddr_in *pSockAddrIn = (const struct sockaddr_in *)name;
    int     Status;

    DEBUGMSG(ZONE_INTERFACE, (TEXT("Bind\r\n")));

    Status = bind(s, name, namelen);
    return Status;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Bind(
    OUT int *pStatus,
    OUT int *pErr,
    IN SOCKET s,
    IN sockaddr_buf numbers,
    IN int namelen
    )
{
    DEBUGMSG(ZONE_INTERFACE, (TEXT("+WSS_Bind\r\n")));

    *pStatus = Bind(s, (const struct sockaddr FAR *)numbers.buffer(), 
        numbers.count());

    if (SOCKET_ERROR == *pStatus)
        {
        *pErr = GetLastError();
        }

    return TRUE;
}


WINSOCK_API_LINKAGE
SOCKET
WSAAPI
Accept(
    IN SOCKET s,
    OUT struct sockaddr FAR * addr,
    IN OUT int FAR * addrlen
    )
{
    
    DEBUGMSG(ZONE_INTERFACE, (TEXT("WSS: +Accept %d\r\n"), s));
    return accept(s, addr, addrlen);
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Accept(
    OUT SOCKET      *pNewSock,
    IN SOCKET       s,
    sockaddr_buf    addr,
    int FAR         *addrlen
    )
{    
    DEBUGMSG(ZONE_INTERFACE, (TEXT("+WSS: WSS_Accept\r\n")));

    *pNewSock = (SOCKET)Accept(s, (struct sockaddr *)addr.buffer(), addrlen);
    DEBUGMSG(ZONE_INTERFACE, (TEXT("-WSS: WSS_Accept Ret: %d\r\n"), *pNewSock));
    
    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Closesocket(
    OUT int *pStatus,
    IN SOCKET s
    )
{
    DEBUGMSG(ZONE_INTERFACE, (TEXT("WSS_Closeoscket %x\r\n"), s));

    *pStatus = closesocket(s);
    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
Connect(
    IN SOCKET s,
    IN const struct sockaddr FAR * name,
    IN int namelen
    )
{
    const struct sockaddr_in *pSockAddrIn = (const struct sockaddr_in *)name;
    int     Status;

    Status = connect(s, name, namelen);
    return Status;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Connect(
    OUT int         *pStatus,
    IN SOCKET       s,
    sockaddr_buf    name,
    IN int          namelen
    )
{
    DEBUGMSG(ZONE_INTERFACE, (TEXT("WSS_Connect %x\r\n"), s));
    *pStatus = Connect(s, (const struct sockaddr *)name.buffer(),
        name.count());

    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Getpeername(
    OUT int         *pStatus,
    IN SOCKET       s,
    sockaddr_buf    name,
    IN OUT int FAR  *namelen
    )
{
    ASSERT(name.count() == (DWORD)*namelen);

    *pStatus = getpeername(s, (struct sockaddr *)name.buffer(), namelen);

    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Getsockname(
    OUT int         *pStatus,
    IN SOCKET       s,
    sockaddr_buf    name,
    IN OUT int FAR  *namelen
    )
{
    ASSERT(name.count() == (DWORD)*namelen);

    *pStatus = getsockname(s, (struct sockaddr *)name.buffer(), namelen);

    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Getsockopt(
    OUT int *pStatus,
    IN SOCKET s,
    IN int level,
    IN int optname,
    ce::psl_buffer_wrapper<uchar *> optval,
    IN OUT int FAR * optlen
    )
{
    ASSERT(optval.count() == (DWORD)*optlen);

    *pStatus = getsockopt(s, level, optname, (char *)optval.buffer(), optlen);

    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Ioctlsocket(
    OUT int *pStatus,
    IN SOCKET s,
    IN long cmd,
    IN OUT u_long FAR * argp
    )
{
    *pStatus = ioctlsocket(s, cmd, argp);

    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Listen(
    OUT int *pStatus,
    IN SOCKET s,
    IN int backlog
    )
{
    DEBUGMSG(ZONE_INTERFACE, (TEXT("WSS_Listen %x\r\n"), s));
    *pStatus = listen(s, backlog);
    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
Recv(
    IN SOCKET s,
    __out_bcount(len) OUT char FAR * buf,
    IN int len,
    IN int flags
    )
{
    int Status;

    DEBUGMSG(ZONE_INTERFACE, (TEXT("Recv %x\r\n"), s));
    Status = recv(s, buf, len, flags);
    return Status;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Recv(
    OUT int *pRcvd,
    IN SOCKET s,
    ce::psl_buffer_wrapper<const uchar *>buf,
    IN int len,
    IN int flags
    )
{
    DEBUGMSG(ZONE_INTERFACE, (TEXT("WSS_Recv\r\n")));

    *pRcvd = Recv(s, (char *)buf.buffer(), len, flags);
    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
Recvfrom(
    IN SOCKET s,
    __out_bcount(len) OUT char FAR * buf,
    IN int len,
    IN int flags,
    OUT struct sockaddr FAR * from,
    IN OUT int FAR * fromlen
    )
{
    int Status;

    DEBUGMSG(ZONE_INTERFACE, (TEXT("Recvfrom %x\r\n"), s));
    Status = recvfrom(s, buf, len, flags, from, fromlen);
    return Status;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Recvfrom(
    OUT int             *pRcvd,
    IN SOCKET           s,
    OUT ce::psl_buffer_wrapper<const uchar *> buf,
    IN int              len,
    IN int              flags,
    OUT sockaddr_buf    from,
    IN OUT int FAR      *fromlen
    )
{
    DEBUGMSG(ZONE_INTERFACE, (TEXT("WSS_Recvfrom\r\n")));


    *pRcvd = Recvfrom(s, (char *)buf.buffer(), len, flags,
        (struct sockaddr *)from.buffer(), fromlen);
    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Select(
    OUT int *pStatus,
    IN OUT  ce::psl_buffer_wrapper<int *>   readfds,
    IN OUT  ce::psl_buffer_wrapper<int *>   writefds,
    IN OUT  ce::psl_buffer_wrapper<int *>   exceptfds,
    IN const struct timeval FAR             *timeout
    )
{
    fd_set  *pReadFds, *pWriteFds, *pExceptFds;
    
    static int fStop = 1;

    pReadFds = NULL;

    pReadFds = (fd_set *)readfds.buffer();
    if (pReadFds && (0 == pReadFds->fd_count))
        pReadFds = NULL;

    pWriteFds = (fd_set *)writefds.buffer();
    if (pWriteFds && (0 == pWriteFds->fd_count))
        pWriteFds = NULL;

    pExceptFds = (fd_set *)exceptfds.buffer();
    if (pExceptFds && (0 == pExceptFds->fd_count))
        pExceptFds = NULL;

    *pStatus = select(0, pReadFds, pWriteFds, pExceptFds, timeout);

    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
Send(
    IN SOCKET s,
    __in_bcount(len) IN const char FAR * buf,
    IN int len,
    IN int flags
    )
{
    int Status;
    
    DEBUGMSG(ZONE_INTERFACE, (TEXT("Send %x\r\n"), s));

/*    for (int i = 0; i < len; i++)
        {
        char c = buf[i];
        DWORD Value = (DWORD)c;
        
        DEBUGMSG(ZONE_MISC, (L"%d", Value));
        }

    DEBUGMSG(ZONE_MISC, (L"\r\n"));
*/
    Status = send(s, buf, len, flags);
    DEBUGMSG(ZONE_INTERFACE, (L"WSK: Send: return %d\r\n", Status));

    return Status;

}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Send(
    OUT int *pSent,
    IN SOCKET s,
    ce::psl_buffer_wrapper<const uchar *>buf,
    IN int len,
    IN int flags
    )
{
    DEBUGMSG(ZONE_INTERFACE, (TEXT("WSS_Send %x\r\n"), s));

    *pSent = Send(s, (const char *)buf.buffer(), len, flags);
    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
Sendto(
    IN SOCKET s,
    IN const char FAR * buf,
    IN int len,
    IN int flags,
    IN const struct sockaddr FAR * to,
    IN int tolen
    )
{
    int Status;
    DEBUGMSG(ZONE_INTERFACE, (TEXT("Sendto %x\r\n"), s));

    Status = sendto(s, buf, len, flags, to, tolen);
    return Status;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Sendto(
    OUT int *pSent,
    IN SOCKET s,
    IN ce::psl_buffer_wrapper<const uchar *>buf,
    IN int len,
    IN int flags,
    IN ce::psl_buffer_wrapper<const uchar *>to,
    IN int tolen
    )
{
    DEBUGMSG(ZONE_INTERFACE, (TEXT("WSS_Sendto %x\r\n"), s));

    *pSent = Sendto(s, (const char *)buf.buffer(), len, flags,
        (const struct sockaddr *)to.buffer(), to.count());
    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
Setsockopt(
    IN SOCKET s,
    IN int level,
    IN int optname,
    __in_bcount(optlen) IN const char FAR * optval,
    IN int optlen
    )
{
    DEBUGMSG(ZONE_INTERFACE, (TEXT("Setsockopt %x\r\n"), s));
    return setsockopt(s, level, optname, optval, optlen);
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Setsockopt(
    OUT int *pStatus,
    IN SOCKET s,
    IN int level,
    IN int optname,
    ce::psl_buffer_wrapper<const uchar *> optval,
    IN int optlen
    )
{
    DEBUGMSG(ZONE_INTERFACE, (TEXT("WSS_Setsockopt %x\r\n"), s));

    *pStatus = Setsockopt(s, level, optname,
        (const char *)optval.buffer(), optval.count());
    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Shutdown(
    OUT int *pStatus,
    IN SOCKET s,
    IN int how
    )
{
    DEBUGMSG(ZONE_INTERFACE, (TEXT("WSS_Shutdown %x\r\n"), s));

    *pStatus = shutdown(s, how);
    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
Getaddrinfo(
    IN const char FAR * nodename,
    IN const char FAR * servname,
    IN const struct addrinfo FAR * hints,
    OUT struct addrinfo FAR * FAR * res
    )
{
    return 1;    //getaddrinfo(nodename, servname, hints, res);    
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Getaddrinfo(
    OUT int *pStatus,
    IN const char FAR           *nodename,
    IN const char FAR           *servname,
    byte_array_dw_aligned       hints,
    OUT byte_array_dw_aligned   ResBuf,
    OUT int *pcSize,
    IN  DWORD BasePtr
    )
{
//    DEBUGMSG(ZONE_INTERFACE, (TEXT("WSS_getaddrinfo\r\n")));
    struct addrinfo *pRes, *pCurInfo, *pDestInfo;
    char            *p, *pBuf;
    char            *pOrig = (char *)BasePtr;
    int             cIncrement;

    pRes = NULL;
    *pStatus = getaddrinfo(nodename, servname,
        (const struct addrinfo *)hints.buffer(),
        &pRes);

    pBuf = (char *)ResBuf.buffer();
    p = pBuf;

    for (pCurInfo = pRes; pCurInfo; pCurInfo = pCurInfo->ai_next)
    {
        pDestInfo = (struct addrinfo *)p;
        
        memcpy(p, pCurInfo, sizeof(*pCurInfo));
        p += sizeof(*pCurInfo);

        if (pCurInfo->ai_addr)
        {
            pDestInfo->ai_addr = (SOCKADDR *)((p - pBuf) + pOrig);

            memcpy(p, pCurInfo->ai_addr, pCurInfo->ai_addrlen);

            cIncrement = (pCurInfo->ai_addrlen + 3) & ~3;
            p += cIncrement;
        }

        if (pCurInfo->ai_canonname)
        {
            pDestInfo->ai_canonname = (p - pBuf) + pOrig;
            
            cIncrement = strlen(pCurInfo->ai_canonname);
            cIncrement = (cIncrement + 3) & ~3;

            StringCbCopyA(p, cIncrement, pCurInfo->ai_canonname);

            p += cIncrement;
        }

        if (pCurInfo->ai_next)
        {
            pDestInfo->ai_next = (struct addrinfo *)((p - pBuf) + pOrig);
        }

    }
    // The pRes address info is being copied into the user sent buffer ResBuf. So, free up pRes.
    if(pRes)
    {
        freeaddrinfo(pRes);
    }

    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Getnameinfo(
    OUT int *pStatus,
    IN  sockaddr_buf                            sa,
    IN  int                                     salen,
    OUT ce::psl_buffer_wrapper<const uchar *>   host,
    IN  DWORD                                   hostlen,
    OUT ce::psl_buffer_wrapper<const uchar *>   serv,
    IN  DWORD                                   servlen,
    IN  int                                     flags
    )
{
    ASSERT((sa.count() == (DWORD)salen) || (0 == salen));

    *pStatus = getnameinfo((struct sockaddr *)sa.buffer(), salen,
        (char *)host.buffer(), hostlen, (char *)serv.buffer(), servlen, flags);

    return TRUE;
}


const int HostAddrAddrLen = sizeof(SOCK_THREAD) - sizeof(int) - sizeof(int) - sizeof(int);

typedef struct HostAddrInfo {
    int len;
    int type;
    int error;
    char addr[HostAddrAddrLen];
} HostAddrInfo;


WINSOCK_API_LINKAGE
struct hostent FAR *
WSAAPI
Gethostbyaddr(
    HostAddrInfo *pHostAddrInfo
    )
{
    int     i;
    hostent *pHost, *pDestHost;
    DWORD   cOffset;

    //pHostAddrInfo checked and verified before this function is called
    __analysis_assume(pHostAddrInfo != NULL);
    pHost = gethostbyaddr(pHostAddrInfo->addr, pHostAddrInfo->len, 
        pHostAddrInfo->type);
    if (pHost)
        {
        SOCK_THREAD *pThread = (SOCK_THREAD *)pHostAddrInfo;

        pHostAddrInfo->error = 0;
        
        memset(pThread, 0, sizeof(*pThread));
        memcpy(&pThread->GETHOST_host, pHost, sizeof(*pThread) - 20);

        pDestHost = &pThread->GETHOST_host;

        cOffset = (DWORD)pDestHost - (DWORD)pHost;
        
        //pDestHost->h_name = pHost->h_name + cOffset;
        pDestHost->h_name = (char *)((DWORD)pHost->h_name - (DWORD)pHost);

        if (pHost->h_aliases)
            {
            pDestHost->h_aliases = (char **)((char *)pHost->h_aliases + cOffset);

            for (i = 0; pHost->h_aliases[0]; i++)
                {
                //pDestHost->h_aliases[i] = pHost->h_aliases[i] + cOffset;
                pDestHost->h_aliases[i] = (char *)(pHost->h_aliases[i] - (char *)pHost);
                }

            pDestHost->h_aliases = (char **)((DWORD)pHost->h_aliases - (DWORD)pHost);
            }

        ASSERT(pHost->h_addr_list);
        pDestHost->h_addr_list = (char **)((char *)pHost->h_addr_list + cOffset);

        for (i = 0; pHost->h_addr_list[i]; i++)
            {
            //pDestHost->h_addr_list[i] = pHost->h_addr_list[i] + cOffset;
            pDestHost->h_addr_list[i] = (char *)(pHost->h_addr_list[i] - (char *)pHost);
            }
        pDestHost->h_addr_list = (char **)((char *)pHost->h_addr_list - (char *)pHost);

        }
    else
        {
        // put the error in pHostAddrInfo
        pHostAddrInfo->error = GetLastError();
        RETAILMSG(1, (TEXT("gethostbyaddr failed with %d \r\n"), 
           pHostAddrInfo->error));

        if (! pHostAddrInfo->error)
            {
            ASSERT(0);
            pHostAddrInfo->error = WSAHOST_NOT_FOUND;
            }
        }

    return pHost;
}


WINSOCK_API_LINKAGE
struct hostent FAR *
WSAAPI
Gethostbyname(
    SOCK_THREAD * pThread
    )
{
    int     i;
    hostent *pHost, *pDestHost;
    DWORD   cOffset;

    pHost = gethostbyname(pThread->GETHOST_hostbuf);
    if (pHost)
        {
        memset(pThread, 0, sizeof(*pThread));
        memcpy(&pThread->GETHOST_host, pHost, sizeof(*pThread) - 20);

        pDestHost = &pThread->GETHOST_host;

        cOffset = (DWORD)pDestHost - (DWORD)pHost;
        
        //pDestHost->h_name = pHost->h_name + cOffset;
        pDestHost->h_name = (char *)((DWORD)pHost->h_name - (DWORD)pHost);

        if (pHost->h_aliases)
            {
            pDestHost->h_aliases = (char **)((char *)pHost->h_aliases + cOffset);

            for (i = 0; pHost->h_aliases[0]; i++)
                {
                //pDestHost->h_aliases[i] = pHost->h_aliases[i] + cOffset;
                pDestHost->h_aliases[i] = (char *)(pHost->h_aliases[i] - (char *)pHost);
                }

            pDestHost->h_aliases = (char **)((DWORD)pHost->h_aliases - (DWORD)pHost);
            }

        ASSERT(pHost->h_addr_list);
        pDestHost->h_addr_list = (char **)((char *)pHost->h_addr_list + cOffset);

        for (i = 0; pHost->h_addr_list[i]; i++)
            {
            //pDestHost->h_addr_list[i] = pHost->h_addr_list[i] + cOffset;
            pDestHost->h_addr_list[i] = (char *)(pHost->h_addr_list[i] - (char *)pHost);
            }
        pDestHost->h_addr_list = (char **)((char *)pHost->h_addr_list - (char *)pHost);

        }
    else
        {
        DWORD LastError = GetLastError();

        RETAILMSG(1, (TEXT("gethostbyname failed with %d \r\n"), 
            LastError));

        if (!LastError)
            {
            ASSERT(0);
            LastError = WSAHOST_NOT_FOUND;
            }

        memcpy(pThread->st_ntoa_buffer, &LastError, 4);
        }

    return pHost;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Gethostbyaddr(
    OUT int                     *pErr,
    IN  ce::psl_buffer_wrapper<const uchar *> addr,
    IN  int                     len,
    IN  int                     type,
    OUT byte_array_dw_aligned   buf
    )
{
    DWORD           Status;
    HANDLE          hThread;
    HostAddrInfo    *pHostAddrInfo;

    if (len > HostAddrAddrLen)
        {
        pHostAddrInfo = NULL;
        }
    else
        {
        pHostAddrInfo = (HostAddrInfo *)buf.buffer();
        ASSERT(buf.count() == sizeof(SOCK_THREAD));
        }

    if (pHostAddrInfo)
        {
        pHostAddrInfo->len = len;
        pHostAddrInfo->type = type;
        memcpy(&pHostAddrInfo->addr, addr.buffer(), len);
        

        hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Gethostbyaddr, 
            (LPVOID)pHostAddrInfo, 0, NULL);
        Status = WaitForSingleObject(hThread, INFINITE);
        if(WAIT_OBJECT_0 == Status)
            {
            }
        else
            {
            ASSERT(0);
            }

        *pErr = pHostAddrInfo->error;

        }
    else
        {
        SetLastError(WSAENOBUFS);
        return FALSE;
        }

    return TRUE;    

}

WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Gethostbyname(
    OUT int                     *pErr,
    IN  const char FAR          *name,
    OUT byte_array_dw_aligned   buf
    )
{
    DWORD           Status;
    HANDLE          hThread;
    SOCK_THREAD     *pThread;

    pThread = (SOCK_THREAD *)buf.buffer();
    ASSERT(buf.count() == sizeof(*pThread));
    if (pThread)
        {
        //name is checked and verified before this function is called
        __analysis_assume(name != NULL);

        StringCchCopyA(pThread->GETHOST_hostbuf, _countof(pThread->GETHOST_hostbuf), name);

        hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Gethostbyname, 
            (LPVOID)pThread, 0, NULL);
        Status = WaitForSingleObject(hThread, INFINITE);
        if(WAIT_OBJECT_0 == Status)
            {
            }
        else
            {
            ASSERT(0);
            }
        *pErr = *((DWORD*)pThread->st_ntoa_buffer);
        }
    else
        {
        SetLastError(WSAENOBUFS);
        return FALSE;
        }

    return TRUE;    

}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Gethostname(
    OUT int *pStatus,
    OUT ce::psl_buffer_wrapper<const uchar *>name,
    IN int namelen
    )
{

    ASSERT(name.count() == (DWORD)namelen);

    *pStatus = gethostname((char *)name.buffer(), namelen);

    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_Sethostname(
    OUT int *pStatus,
    IN ce::psl_buffer_wrapper<char *>name,
    IN int cName)
{

    ASSERT(name.count() == (DWORD)cName);
    
    *pStatus = sethostname((char *) name.buffer(), cName);

    return TRUE;
}


WINSOCK_API_LINKAGE
INT
WSAAPI
WSS_WSAAddressToString(
    OUT    INT                                  *pStatus,
    IN     sockaddr_buf                         psaAddr,
    IN     DWORD                                dwAddressLength,
    IN     byte_array_dw_aligned                pProtInfo,
    IN OUT ce::psl_buffer_wrapper<const WCHAR*> lpszAddressString,
    IN OUT LPDWORD                              lpdwAddressStringLength
    )
{
    ASSERT(psaAddr.count() == dwAddressLength);
    ASSERT( (pProtInfo.count() == sizeof(WSAPROTOCOL_INFOW)) || \
            ( 0 == pProtInfo.count()));
    
    *pStatus = WSAAddressToString((LPSOCKADDR)psaAddr.buffer(),
        dwAddressLength, (LPWSAPROTOCOL_INFOW)pProtInfo.buffer(),
        (LPWSTR)lpszAddressString.buffer(), lpdwAddressStringLength);

    return TRUE;
        
}


WINSOCK_API_LINKAGE
INT
WSAAPI
WSS_WSAStringToAddress(
    OUT    INT                      *pStatus,
    IN     const WCHAR *            AddressString,
    IN     INT                      AddressFamily,
    IN     byte_array_dw_aligned    lpProtocolInfo,
    OUT    sockaddr_buf             lpAddress,
    IN OUT LPINT                    lpAddressLength
    )
{
    ASSERT((lpProtocolInfo.count() == sizeof(WSAPROTOCOL_INFOW)) || \
        (NULL == lpProtocolInfo.buffer()));
    ASSERT(lpAddress.count() == (DWORD)*lpAddressLength);
    
    *pStatus = WSAStringToAddress((LPWSTR)AddressString,
        AddressFamily, (LPWSAPROTOCOL_INFOW)lpProtocolInfo.buffer(),
        (LPSOCKADDR)lpAddress.buffer(), lpAddressLength);

    return TRUE;

}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_WSASocket(
    OUT int                     *pSock,
    IN int                      af,
    IN int                      type,
    IN int                      protocol,
    IN byte_array_dw_aligned    buf,
    IN GROUP                    g,
    IN DWORD                    dwFlags
    )
{
    LPWSAPROTOCOL_INFOW lpProtInfo;
    int                 Sock;
    
    DEBUGMSG(ZONE_INTERFACE, (TEXT("WSS_WSASocket\r\n")));

    lpProtInfo = (LPWSAPROTOCOL_INFOW)buf.buffer();
    ASSERT((buf.count() == sizeof(*lpProtInfo)) || (0 == buf.count()));

    Sock = WSASocket(af, type, protocol, lpProtInfo, g, dwFlags);
    *pSock = Sock;
    return TRUE;

}


class OverlapWrapper {
public:
    OverlapWrapper(WSAOVERLAPPED Ov)
        {
        m_Ov = Ov;
        m_hEvent = m_hUserEvent = NULL;
        }

    ~OverlapWrapper()
        {
        if (m_hEvent)
            CloseHandle(m_hEvent);

        if (m_hUserEvent)
            CloseHandle(m_hUserEvent);
        }

    DWORD Initialize(DWORD ProcId)
        {
        DWORD       Status = NO_ERROR;

        if (! DuplicateHandle((HANDLE)ProcId, (HANDLE)m_Ov.hEvent, 
            (HANDLE)GetCurrentProcessId(), &m_hUserEvent,
            0, FALSE, (DUPLICATE_SAME_ACCESS)))
            {
            Status = WSAEINVAL;
            }
        else
            {
            m_Ov.hEvent = m_hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
            if (! m_hEvent)
                Status = WSAENOBUFS;
            }

        return Status;

        }

    void WaitForPendingOv()
        {
        ASSERT(m_hEvent == m_Ov.hEvent);
        WaitForSingleObject(m_hEvent, INFINITE);

        SetEvent(m_hUserEvent);
        }

    WSAOVERLAPPED *GetOv()
        {
        return &m_Ov;
        }

private:
    WSAOVERLAPPED   m_Ov;
    WSAEVENT        m_hEvent;
    WSAEVENT        m_hUserEvent;
};


DWORD WaitForPendingOvThread(class OverlapWrapper *pOvWrapper)
{
    pOvWrapper->WaitForPendingOv();
    delete pOvWrapper;

    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_WSAIoctl(
    OUT int                     *pStatus,
    OUT DWORD                   *pLastError,
    IN  SOCKET                  s,
    IN  DWORD                   dwIoControlCode,
    IN  byte_array_dw_aligned   lpvInBuffer,
    IN  byte_array_dw_aligned   lpvOutBuffer,
    OUT LPDWORD                 lpcbBytesReturned,
    IN  byte_array_dw_aligned   lpOverlapped,
    IN  DWORD                   ProcId

    )
{
    WSAOVERLAPPED   *pOv;
    OverlapWrapper  *pLocalOv = NULL;
    WSAEVENT        hEvent = NULL;
    int             Status = SOCKET_ERROR;
    DWORD           LastError = 0;
    HANDLE          hThread = NULL;

    if (lpOverlapped.count())
        pOv = (WSAOVERLAPPED *)lpOverlapped.buffer();
    else
        pOv = NULL;

    if (pOv && pOv->hEvent)
        {

        pLocalOv = new OverlapWrapper(*pOv);
        if (pLocalOv)
            {
            LastError = pLocalOv->Initialize(ProcId);
            if (! LastError)
                {
                hThread = CreateThread(NULL, 0,
                    (LPTHREAD_START_ROUTINE)WaitForPendingOvThread, 
                    (LPVOID)pLocalOv, CREATE_SUSPENDED, NULL);
                if (!hThread)
                    {
                    LastError = WSAENOBUFS;
                    }
                }

            if (LastError)
                {
                delete pLocalOv;
                pLocalOv = NULL;
                }
            }
        else
            {
            LastError = WSAENOBUFS;
            }
        }


    if (! LastError)
        {
        Status = WSAIoctl(s, dwIoControlCode, 
            (LPVOID)lpvInBuffer.buffer(), lpvInBuffer.count(), 
            (LPVOID)lpvOutBuffer.buffer(), lpvOutBuffer.count(),
            lpcbBytesReturned, pLocalOv ? pLocalOv->GetOv() : NULL, NULL);

        if (SOCKET_ERROR == Status)
            LastError = GetLastError();

        if (pLocalOv)
            {
            if ((SOCKET_ERROR == Status) && (WSA_IO_PENDING == LastError))
                {
                ASSERT(hThread);
                ResumeThread(hThread);
                }
            else
                {
                // it is safe to terminate a suspended user mode thread
                TerminateThread(hThread, 0);
                delete pLocalOv;
                }
            }
        }
    else
        {
        ASSERT(! hThread);
        ASSERT(! pLocalOv);
        }


    if (SOCKET_ERROR == Status)
        *pLastError = LastError;

    if (hThread)
        CloseHandle(hThread);
    
    *pStatus = Status;

    return TRUE;

}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_WSARecv(
    OUT int *pStatus,
    IN  SOCKET s,
    IN  ce::psl_buffer_wrapper<const uchar *>buf,
    IN  ce::psl_buffer_wrapper<const uchar *>buf1,
    IN  ce::psl_buffer_wrapper<const uchar *>buf2,
    IN  ce::psl_buffer_wrapper<const uchar *>buf3,
    IN  ce::psl_buffer_wrapper<const uchar *>buf4,
    IN  DWORD   dwBufferCount,
    OUT LPDWORD lpNumberOfBytesRecvd,
    IN OUT LPDWORD lpFlags
    )
{
    WSABUF  aWsaBufs[MaxWsaBufs];
    int     i;

    //DEBUGMSG(ZONE_INTERFACE, (TEXT("WSS_WSASend %x\r\n"), s));

    ASSERT(dwBufferCount <= MaxWsaBufs);

    i = 0;
    aWsaBufs[i].len     = buf.count();
    aWsaBufs[i++].buf   = (char *)buf.buffer();
    aWsaBufs[i].len     = buf1.count();
    aWsaBufs[i++].buf   = (char *)buf1.buffer();
    aWsaBufs[i].len     = buf2.count();
    aWsaBufs[i++].buf   = (char *)buf2.buffer();
    aWsaBufs[i].len     = buf3.count();
    aWsaBufs[i++].buf   = (char *)buf3.buffer();
    aWsaBufs[i].len     = buf4.count();
    aWsaBufs[i++].buf   = (char *)buf4.buffer();

    //dwBufferCount is checked and verified before this function is called
    __analysis_assume(dwBufferCount > 0);
    
    *pStatus = WSARecv(s, aWsaBufs, dwBufferCount, lpNumberOfBytesRecvd,
        lpFlags, NULL, NULL);

    return TRUE;

}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_WSARecvFrom(
    OUT int             *pStatus,
    IN  SOCKET          s,
    IN  ce::psl_buffer_wrapper<const uchar *>buf,
    IN  ce::psl_buffer_wrapper<const uchar *>buf1,
    IN  ce::psl_buffer_wrapper<const uchar *>buf2,
    IN  ce::psl_buffer_wrapper<const uchar *>buf3,
    IN  ce::psl_buffer_wrapper<const uchar *>buf4,
    IN  DWORD           dwBufferCount,
    OUT LPDWORD         lpNumberOfBytesRecvd,
    IN OUT LPDWORD      lpFlags,
    IN  sockaddr_buf    lpFrom,
    IN OUT LPINT        lpFromlen
    )
{
    WSABUF  aWsaBufs[MaxWsaBufs];
    int     i;

    //DEBUGMSG(ZONE_INTERFACE, (TEXT("WSS_WSASendTo %x\r\n"), s));

    ASSERT(dwBufferCount <= MaxWsaBufs);
    if (lpFromlen)
        ASSERT(lpFrom.count() == (DWORD)*lpFromlen);

    i = 0;
    aWsaBufs[i].len     = buf.count();
    aWsaBufs[i++].buf   = (char *)buf.buffer();
    aWsaBufs[i].len     = buf1.count();
    aWsaBufs[i++].buf   = (char *)buf1.buffer();
    aWsaBufs[i].len     = buf2.count();
    aWsaBufs[i++].buf   = (char *)buf2.buffer();
    aWsaBufs[i].len     = buf3.count();
    aWsaBufs[i++].buf   = (char *)buf3.buffer();
    aWsaBufs[i].len     = buf4.count();
    aWsaBufs[i++].buf   = (char *)buf4.buffer();
    
    //dwBufferCount is checked and verified before this function is called
    __analysis_assume(dwBufferCount > 0);

    *pStatus = WSARecvFrom(s, aWsaBufs, dwBufferCount, lpNumberOfBytesRecvd,
        lpFlags, (struct sockaddr  *)lpFrom.buffer(), lpFromlen, NULL, NULL);

    return TRUE;

}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_WSASend(
    OUT int *pStatus,
    IN  SOCKET s,
    IN  ce::psl_buffer_wrapper<const uchar *>buf,
    IN  ce::psl_buffer_wrapper<const uchar *>buf1,
    IN  ce::psl_buffer_wrapper<const uchar *>buf2,
    IN  ce::psl_buffer_wrapper<const uchar *>buf3,
    IN  ce::psl_buffer_wrapper<const uchar *>buf4,
    IN  DWORD   dwBufferCount,
    OUT LPDWORD lpNumberOfBytesSent,
    IN DWORD dwFlags
    )
{
    WSABUF  aWsaBufs[MaxWsaBufs];
    int     i;

    //DEBUGMSG(ZONE_INTERFACE, (TEXT("WSS_WSASend %x\r\n"), s));

    ASSERT(dwBufferCount <= MaxWsaBufs);

    i = 0;
    aWsaBufs[i].len     = buf.count();
    aWsaBufs[i++].buf   = (char *)buf.buffer();
    aWsaBufs[i].len     = buf1.count();
    aWsaBufs[i++].buf   = (char *)buf1.buffer();
    aWsaBufs[i].len     = buf2.count();
    aWsaBufs[i++].buf   = (char *)buf2.buffer();
    aWsaBufs[i].len     = buf3.count();
    aWsaBufs[i++].buf   = (char *)buf3.buffer();
    aWsaBufs[i].len     = buf4.count();
    aWsaBufs[i++].buf   = (char *)buf4.buffer();
    
    //dwBufferCount is checked and verified before this function is called
    __analysis_assume(dwBufferCount > 0);

    *pStatus = WSASend(s, aWsaBufs, dwBufferCount, lpNumberOfBytesSent,
        dwFlags, NULL, NULL);

    return TRUE;

}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_WSASendto(
    OUT int             *pStatus,
    IN  SOCKET          s,
    IN  ce::psl_buffer_wrapper<const uchar *>buf,
    IN  ce::psl_buffer_wrapper<const uchar *>buf1,
    IN  ce::psl_buffer_wrapper<const uchar *>buf2,
    IN  ce::psl_buffer_wrapper<const uchar *>buf3,
    IN  ce::psl_buffer_wrapper<const uchar *>buf4,
    IN  DWORD           dwBufferCount,
    OUT LPDWORD         lpNumberOfBytesSent,
    IN  DWORD           dwFlags,
    IN  sockaddr_buf    lpTo,
    IN  int             iTolen
    )
{
    WSABUF  aWsaBufs[MaxWsaBufs];
    int     i;

    //DEBUGMSG(ZONE_INTERFACE, (TEXT("WSS_WSASendTo %x\r\n"), s));

    ASSERT(dwBufferCount <= MaxWsaBufs);
    ASSERT(lpTo.count() == (DWORD)iTolen);

    i = 0;
    aWsaBufs[i].len     = buf.count();
    aWsaBufs[i++].buf   = (char *)buf.buffer();
    aWsaBufs[i].len     = buf1.count();
    aWsaBufs[i++].buf   = (char *)buf1.buffer();
    aWsaBufs[i].len     = buf2.count();
    aWsaBufs[i++].buf   = (char *)buf2.buffer();
    aWsaBufs[i].len     = buf3.count();
    aWsaBufs[i++].buf   = (char *)buf3.buffer();
    aWsaBufs[i].len     = buf4.count();
    aWsaBufs[i++].buf   = (char *)buf4.buffer();
    
    //dwBufferCount is checked and verified before this function is called
    __analysis_assume(dwBufferCount > 0);

    *pStatus = WSASendTo(s, aWsaBufs, dwBufferCount, lpNumberOfBytesSent,
        dwFlags, (struct sockaddr  *)lpTo.buffer(), iTolen, NULL, NULL);

    return TRUE;

}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_WSAEnumNameSpaceProviders(
    OUT int                     *pStatus,
    IN OUT LPDWORD              lpdwBufferLength,
    OUT byte_array_dw_aligned   lpnspBuffer
    )
{
    int Status;

    ASSERT(lpnspBuffer.count() == *lpdwBufferLength);

    Status = WSAEnumNameSpaceProviders(lpdwBufferLength, 
        (LPWSANAMESPACE_INFOW)lpnspBuffer.buffer());

    *pStatus = Status;

    if (SOCKET_ERROR != Status)
        {
        LPWSANAMESPACE_INFOW pNsp = (LPWSANAMESPACE_INFOW)lpnspBuffer.buffer();
        while (Status-- > 0)
            {
            pNsp->lpszIdentifier = (LPWSTR)
                ((DWORD)pNsp->lpszIdentifier - (DWORD)pNsp);
            pNsp++;
            }

        }

    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_EnumNetworkEvents(
    OUT int *pStatus,
    IN SOCKET s,
    IN int hEventObject,
    IN DWORD ProcId,
    OUT byte_array_dw_aligned   pNetEvents
    )
{
    WSAEVENT hEvent = NULL;

    ASSERT(pNetEvents.count() == sizeof(WSANETWORKEVENTS));

    if (hEventObject && (! DuplicateHandle((HANDLE)ProcId, (HANDLE)hEventObject, 
        (HANDLE)GetCurrentProcessId(), &hEvent,
        0, FALSE, (DUPLICATE_SAME_ACCESS))))
        {
        *pStatus = WSAEINVAL;
        return TRUE;
        }

    *pStatus = WSAEnumNetworkEvents(s, hEvent, 
        (LPWSANETWORKEVENTS) pNetEvents.buffer());

    if (hEvent)
        CloseHandle(hEvent);

    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_WSAEnumProtocols(
    OUT int *pStatus,
    IN ce::psl_buffer_wrapper<uchar *> lpiProtocols,
    OUT byte_array_dw_aligned   lpProtocolBuffer,
    IN OUT LPDWORD lpdwBufferLength
    )
{

    ASSERT(lpProtocolBuffer.count() == *lpdwBufferLength);

    *pStatus = WSAEnumProtocols((LPINT)lpiProtocols.buffer(), 
        (LPWSAPROTOCOL_INFOW)lpProtocolBuffer.buffer(), lpdwBufferLength);

    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_WSAEventSelect(
    OUT int *pStatus,
    IN SOCKET s,
    IN int hEventObject,
    IN DWORD ProcId,
    IN long lNetworkEvents
    )
{
    WSAEVENT hEvent = NULL;

    if (! DuplicateHandle((HANDLE)ProcId, (HANDLE)hEventObject, 
        (HANDLE)GetCurrentProcessId(), &hEvent,
        0, FALSE, (DUPLICATE_SAME_ACCESS)))
        {
        hEvent = NULL;
        }

    if (hEvent) 
        {
        *pStatus = WSAEventSelect(s, hEvent, lNetworkEvents);
        CloseHandle(hEvent);
        }
    else
        {
        *pStatus = WSAEINVAL;
        }

    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_WSAGetOverlappedResult(
    OUT int *pStatus,
    IN SOCKET s,
    IN ce::psl_buffer_wrapper<const uchar *> lpOverlapped,
    OUT LPDWORD lpcbTransfer,
    IN BOOL fWait,
    OUT LPDWORD lpdwFlags

    )
{
/*
    ASSERT(lpOverlapped.count() == sizeof(WSAOVERLAPPED));

    *pStatus = WSAGetOverlappedResult(s, 
        (WSAOVERLAPPED *)lpOverlapped.buffer(), lpcbTransfer, 
        fWait, lpdwFlags);
*/
    ASSERT(0);
    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_WSAHtonl(
    OUT int *pStatus,
    IN SOCKET s,
    IN u_long hostlong,
    OUT u_long FAR * lpnetlong
    )
{
    *pStatus = WSAHtonl(s, hostlong, lpnetlong);

    return TRUE;
}


WINSOCK_API_LINKAGE
int
WSAAPI
WSS_WSAHtons(
    OUT int *pStatus,
    IN SOCKET s,
    IN u_short hostshort,
    OUT short FAR * lpnetshort
    )
{
    *pStatus = WSAHtons(s, hostshort, (u_short *)lpnetshort);

    return TRUE;
}


DWORD
WSS_WSAControl(
    OUT int     *pStatus,
    OUT DWORD   *pLastError,
    IN DWORD    Protocol,
    IN DWORD    Action,
    IN byte_array_dw_aligned    pInBuf,
    OUT DWORD   *pcInBuf,
    IN byte_array_dw_aligned    pOutBuf,
    OUT DWORD   *pcOutBuf
    )
{

    *pStatus = WSAControl(Protocol, Action,
        (LPVOID)pInBuf.buffer(), pcInBuf, 
        (LPVOID)pOutBuf.buffer(), pcOutBuf);

    if (SOCKET_ERROR == *pStatus)
        *pLastError = GetLastError();

    return TRUE;

}


extern "C" BOOL 
WSS_IOControl(
    DWORD    hOpenContext,
    DWORD    dwCode, 
    PBYTE    pBufIn,
    DWORD    dwLenIn, 
    PBYTE    pBufOut, 
    DWORD    dwLenOut,
    PDWORD    pdwActualOut)
{    
    DWORD   dwErr = ERROR_INVALID_PARAMETER;

    switch (dwCode) {

        case WSS_IOCTL_INVOKE:
            {
            ce::pc_stub<> stub(pBufOut, dwLenOut);

            switch (stub.function())
                {
                case WSS_STARTUP:
                dwErr = stub.call(WSS_Startup);
                break;

                case WSS_CLEANUP:
                dwErr = stub.call(WSS_Cleanup);
                break;

                case WSS_SOCKET:
                return stub.call(WSS_Socket);
                break;

                case WSS_BIND:
                dwErr = stub.call(WSS_Bind);
                break;

                case WSS_ACCEPT:
                dwErr = stub.call(WSS_Accept);
                break;

                case WSS_CLOSESOCKET:
                dwErr = stub.call(WSS_Closesocket);
                break;

                case WSS_CONNECT:
                dwErr = stub.call(WSS_Connect);
                break;

                case WSS_GETHOSTBYADDR:
                dwErr = stub.call(WSS_Gethostbyaddr);
                break;

                case WSS_GETHOSTNAME:
                dwErr = stub.call(WSS_Gethostname);
                break;

                case WSS_SETHOSTNAME:
                dwErr = stub.call(WSS_Sethostname);
                break;

                case WSS_GETADDRINFO:
                dwErr = stub.call(WSS_Getaddrinfo);
                break;

                case WSS_GETNAMEINFO:
                dwErr = stub.call(WSS_Getnameinfo);
                break;

                case WSS_GETPEERNAME:
                dwErr = stub.call(WSS_Getpeername);
                break;

                case WSS_GETSOCKNAME:
                dwErr = stub.call(WSS_Getsockname);
                break;

                case WSS_GETSOCKOPT:
                dwErr = stub.call(WSS_Getsockopt);
                break;

                case WSS_IOCTLSOCKET:
                dwErr = stub.call(WSS_Ioctlsocket);
                break;

                case WSS_LISTEN:
                dwErr = stub.call(WSS_Listen);
                break;

                case WSS_RECV:
                dwErr = stub.call(WSS_Recv);
                break;

                case WSS_RECVFROM:
                dwErr = stub.call(WSS_Recvfrom);
                break;

                case WSS_SELECT:
                dwErr = stub.call(WSS_Select);
                break;

                case WSS_SEND:
                dwErr = stub.call(WSS_Send);
                break;

                case WSS_SENDTO:
                dwErr = stub.call(WSS_Sendto);
                break;

                case WSS_SETSOCKOPT:
                dwErr = stub.call(WSS_Setsockopt);
                break;

                case WSS_SHUTDOWN:
                dwErr = stub.call(WSS_Shutdown);
                break;

                case WSS_GETHOSTBYNAME:
                dwErr = stub.call(WSS_Gethostbyname);
                break;

                case WSS_WSAADDRTOSTRING:
                dwErr = stub.call(WSS_WSAAddressToString);
                break;

                case WSS_WSASTRINGTOADDR:
                dwErr = stub.call(WSS_WSAStringToAddress);
                break;

                case WSS_WSASOCKET:
                dwErr = stub.call(WSS_WSASocket);
                break;

                case WSS_WSAIOCTL:
                dwErr = stub.call(WSS_WSAIoctl);
                break;

                case WSS_WSARECV:
                dwErr = stub.call(WSS_WSARecv);
                break;

                case WSS_WSARECVFROM:
                dwErr = stub.call(WSS_WSARecvFrom);
                break;

                case WSS_WSASEND:
                dwErr = stub.call(WSS_WSASend);
                break;

                case WSS_WSASENDTO:
                dwErr = stub.call(WSS_WSASendto);
                break;

                case WSS_ENUMNAMESPACE:
                dwErr = stub.call(WSS_WSAEnumNameSpaceProviders);
                break;

                case WSS_ENUMNETWORKEVENTS:
                dwErr = stub.call(WSS_EnumNetworkEvents);
                break;

                case WSS_ENUMPROTOCOLS:
                dwErr = stub.call(WSS_WSAEnumProtocols);
                break;

                case WSS_EVENTSELECT:
                dwErr = stub.call(WSS_WSAEventSelect);
                break;

                case WSS_GETOVERLAPPEDRESULT:
                dwErr = stub.call(WSS_WSAGetOverlappedResult);
                break;

                case WSS_WSAHTONL:
                dwErr = stub.call(WSS_WSAHtonl);
                break;

                case WSS_WSAHTONS:
                dwErr = stub.call(WSS_WSAHtons);
                break;

                case WSS_WSACONTROL:
                dwErr = stub.call(WSS_WSAControl);
                break;

                default:
                ASSERT(0);
                break;

                }

            }

            break;

        case (WSS_IOCTL_INVOKE+1):
            if (pBufOut && 4 == dwLenOut)
                {
                DWORD ProcId = GetCurrentProcessId();
                HANDLE Proc = GetCurrentProcess();
                *(DWORD *)pBufOut = GetCurrentProcessId();
                dwErr = TRUE;
                }
            break;
                

        case IOCTL_PSL_NOTIFY:
            {
            break;
            }

        default:
            ASSERT(0);
            break;
    }           

    return dwErr;
}    //    KDM_IOControl()
