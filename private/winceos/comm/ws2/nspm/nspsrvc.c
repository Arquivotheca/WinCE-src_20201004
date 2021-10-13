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

nspsrvc.c

pass-through calls from the CE WSP to the PM (afd) layer


FILE HISTORY:
    OmarM     02-Oct-2000


*/

#include "nspmp.h"
#include "ws2tcpip.h"
#include <type.h>


// temporary service-list
typedef struct Services {
    char    *pName;
    int     Port;
    char    *pProtocol;
} Services;

Services s_Serv = {"ftp", 21, "tcp"};

// BUGBUG: we don't handle aliases right now, so any aliases will have to be
// written as: {"alias", portno, "protocol"} right below the original svc name
Services s_aServices[] = {
    {"ftp",     21,     "tcp"},
    {"telnet",  23,     "tcp"},
    {"tftp",    69,     "udp"},
    {"login",   513,    "tcp"}
};


GUID HostnameGuid = SVCID_HOSTNAME;
GUID AddressGuid = SVCID_INET_HOSTADDRBYINETSTRING;
GUID InetHostName = SVCID_INET_HOSTADDRBYNAME;
GUID IANAGuid = SVCID_INET_SERVICEBYNAME;
GUID AtmaGuid = SVCID_DNS_TYPE_ATMA;
GUID Ipv6Guid = SVCID_DNS_TYPE_AAAA;

#define DNS_F_END_CALLED    0x1             // generic  cancel
#define REVERSE_LOOKUP      0x2
#define LOCAL_LOOKUP        0x4
#define NEED_DOMAIN         0x8
#define IANA_LOOKUP         0x10
#define LOOP_LOOKUP         0x20
#define V6_LOOKUP           0x100
#define DONE_LOOKUP         0x1000


#define T_A     1
#define T_CNAME 5
#define T_AAAA  28
#define T_PTR   12
#define T_ALL   255


typedef struct QueryContext {
    struct QueryContext *pNext;
    GUID                Id;
    DWORD               Flags;
    DWORD               InterfaceIndex;
    int                 cRefs;
    DWORD               ControlFlags;
    WCHAR               pwszSvcName[1];
} QueryContext;

QueryContext        *v_pQueryList;
CRITICAL_SECTION    v_ServicesCS;

QueryContext **_FindQuery(void *hHandle) {
    QueryContext    **ppQuery;


    for (ppQuery = &v_pQueryList; *ppQuery; ppQuery = &((*ppQuery)->pNext))
        if (*ppQuery == (QueryContext *)hHandle)
            break;

    return ppQuery;

}   // FindQuery()


#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif

HANDLE g_hWs2;
typedef DWORD (*PINET_ADDR)(BYTE * addr);
PINET_ADDR g_pInet_Addr;

int MyInetAddrW (const WCHAR * cp, __out unsigned long *pulAddr) {
    unsigned int    i;
    char mbs[INET_ADDRSTRLEN+1];

    *pulAddr = INADDR_NONE;

    if (NULL == g_hWs2) {
        g_hWs2 = LoadLibrary(L"ws2.dll");
        if (NULL == g_hWs2) {
            return WSASYSCALLFAILURE;
        }
    }

    if (NULL == g_pInet_Addr) {
        g_pInet_Addr = (PINET_ADDR)GetProcAddress(g_hWs2, L"inet_addr");
        if (NULL == g_pInet_Addr) {
            return WSASYSCALLFAILURE;
        }
    }

    i = wcslen(cp) + 1;
    if (i <= INET_ADDRSTRLEN) {
        if (WideCharToMultiByte(CP_ACP, 0, cp, -1, (LPSTR)mbs, (int)i, NULL, NULL)) {
            *pulAddr = g_pInet_Addr((BYTE *)mbs);
        }
    }
    return 0;

}   // MyInetAddrW()


// returns TRUE if success otherwise false
int MyInetNtoaW(__out_ecount(17) WCHAR *pszwAddr, struct in_addr Addr) {

    register unsigned char  *p;
    volatile WCHAR          *pBuf;
    int                     i;
    WCHAR                   c;

    pBuf = pszwAddr;
    p = (unsigned char *)&Addr;

    for (i = 3; i >= 0; i--) {
        do {
            PREFAST_SUPPRESS(394, "a byte can at most be div by 10 3 times")
            *pBuf++ = p[i] % 10 + TEXT('0');
        } while (p[i] /= 10);
        *pBuf++ = TEXT('.');
    }
    *(--pBuf) = TEXT('\0'); // gets rid of last period & ends string
    // now reverse it...
    for (--pBuf ; pszwAddr < pBuf; pBuf--, pszwAddr++) {
        c = *pBuf;
        *pBuf = *pszwAddr;
        *pszwAddr = c;
    }
    return TRUE;

}   // MyInetNtoaW()


// returns TRUE if strings are equal except for possbile trailing dot
// ignores case
int DnsNameCompare_W(WCHAR *p1, WCHAR *p2) {
    int Ret, cLen1, cLen2;

    Ret = TRUE;

    if (p1 != p2) {
        if (! p1 || ! p2)
            Ret = FALSE;
        else {
            cLen1 = wcslen(p1);
            cLen2 = wcslen(p2);
            if (cLen1 != cLen2) {
                if (cLen1 == cLen2 + 1) {
                    if (p1[cLen1] != TEXT('.'))
                        Ret = FALSE;
                    else
                        cLen1--;

                } else if (cLen1 == cLen2 - 1) {
                    if (p2[cLen2] != TEXT('.'))
                        Ret = FALSE;
                } else
                    Ret = FALSE;
            }
            if (Ret) {
                Ret = !_wcsnicmp(p1, p2, cLen1);
            }
        }
    }

    return Ret;

}   // DnsNameCompare_W()


void PointerToOffset(HOSTENT *pHost) {
    int     i;
    char    *p;

    // if we use ptrs instead of i (an array) it should be a little more
    // efficient, but it makes the code harder to read

    p = (char *)pHost;

    pHost->h_name = (char *)(pHost->h_name - p);
    if (pHost->h_aliases) {
        for (i = 0; pHost->h_aliases[i]; i++) {
            pHost->h_aliases[i] = (char *)(pHost->h_aliases[i] - p);
        }
        pHost->h_aliases = (char **)((char *)pHost->h_aliases - p);
    }
    for (i = 0; pHost->h_addr_list[i]; i++)
        pHost->h_addr_list[i] = (char *) (pHost->h_addr_list[i] -p);
    pHost->h_addr_list = (char **)((char *)pHost->h_addr_list - p);

}   // PointerToOffset


int CopyHostentToBlob(HOSTENT *pHost, __out_bcount(cBuf) BYTE *pBuf, int cBuf, int *pcSize) {

    int         Err, i;
    int         cSize, cAddr;
    HOSTENT     *pMyHost;
    BYTE        *p, *p2, **ppAlias;

    cSize = sizeof(HOSTENT);
    if (cSize <= cBuf) {
        pMyHost = (HOSTENT *)pBuf;
        p = (BYTE *)(pMyHost + 1);
    } else {
        p = NULL;
        pMyHost = NULL;
    }

    // to keep alignment for addresses, copy the addresses first
    if (pMyHost) {
        pMyHost->h_length = pHost->h_length;
        pMyHost->h_addrtype = pHost->h_addrtype;
    }

    // count how many pointer stuff we need to store the arrays
    if (pHost->h_aliases) {

        i = 0;
        // note i will be 1 more than # of aliases -- for NULL termination
        while(pHost->h_aliases[i++])
            ;

        i <<= 2;
    } else
        i = 4;

    if (pMyHost) {
        pMyHost->h_aliases = (char **)p;
        p += i;
    }
    cSize += i;

    // now count addresses
    for (i = 0; pHost->h_addr_list[i++];)
        ;
    i <<= 2;

    if (pMyHost) {
        pMyHost->h_addr_list = (char **)p;
        p += i;
    }
    cSize += i;

    // ok now copy the arrays, start with addresses first since they are more
    // like to be nicely aligned--maybe maybe not...
    cAddr = (pHost->h_length + 3) & ~3;
    for (i = 0; pHost->h_addr_list[i]; i++) {
        int oldcSize = cSize;

        cSize += cAddr;

        if (cSize > oldcSize) {
            if (cSize <= cBuf) {
                if (pMyHost)
                    pMyHost->h_addr_list[i] = (char *)p;

                PREFAST_ASSUME(cAddr > 0);
                memcpy(p, pHost->h_addr_list[i], cAddr);
                p += cAddr;
            }
        }
    }

    if (pHost->h_name) {
        if (pMyHost)
            pMyHost->h_name = (char *)p;
        // instead of calling strlen first then strcpy
        for (p2 = (BYTE *)pHost->h_name; *p2; p2++) {
            if (++cSize <= cBuf) {
                PREFAST_SUPPRESS(394, "ptr access guarded by cSize < cBuf")
                *p++ = *p2;
            }
        }
        if (++cSize <= cBuf) {
            PREFAST_SUPPRESS(395, "ptr access guarded by cSize < cBuf")
            *p++ = '\0';
        }
    }   // if (pHost->h_name)

    // now copy the aliases
    ppAlias = (BYTE **)pHost->h_aliases;
    if (ppAlias) {

        for (i = 0; *ppAlias; ppAlias++) {
            if (pMyHost)
                pMyHost->h_aliases[i] = (char *)p;

            // instead of calling strlen first then strcpy
            for (p2 = *ppAlias; *p2; p2++) {
                if (++cSize <= cBuf) {
                    PREFAST_SUPPRESS(395, "ptr access guarded by cSize < cBuf")
                    *p++ = *p2;
                }
            }
            if (++cSize <= cBuf)
                *p++ = '\0';
        }
    }   // if (ppAlias = pHost->h_aliases)

    *pcSize = cSize;
    if (cSize > cBuf)
        Err = WSAEFAULT;
    else {
        Err = 0;
    }

    return Err;

}   // CopyHostentToBlob()


int CheckAddressFamily(LPWSAQUERYSETW pQuery) {
    uint                i;
    LPAFPROTOCOLS   pProt;

    ASSERT(pQuery->dwNumberOfProtocols);
    pProt = pQuery->lpafpProtocols;
    for (i = 0; i < pQuery->dwNumberOfProtocols; i++, pProt++) {
        if ((pProt->iAddressFamily == AF_UNSPEC) ||
            (   ((pProt->iAddressFamily == AF_INET) ||
                 (pProt->iAddressFamily == AF_INET6)) &&

                ((pProt->iProtocol == IPPROTO_IP) ||
                 (pProt->iProtocol == IPPROTO_TCP) ||
                 (pProt->iProtocol == IPPROTO_UDP))))
            return TRUE;
    }

    return FALSE;
}


INT WSAAPI NSPLookupServiceBegin(
    LPGUID                  pProviderId,
    LPWSAQUERYSETW          pRestrict,
    LPWSASERVICECLASSINFOW  pServiceClass,
    DWORD                   ControlFlags,
    LPHANDLE                phLookup) {

    WCHAR           *pwszTemp = NULL;
    WCHAR           *pwszSvcName;
    GUID            *pSvcClassId;
    DWORD           Flags;
    SOCKADDR_IN     *pSAddr;
    WCHAR           pwszSockAddr[18];
    int             Err, Family, fNameLookup;
    QueryContext    *pQuery;
    int             cSvcName;
    DWORD           Protocols=0x0, cProtocols;
    int             AddrFamily, Prot;
    LPAFPROTOCOLS   pAfProtocols;

    Family = 0;
    Err = 0;
    // BUGBUG: check NSPStartup called or not

    if (pRestrict->dwSize < sizeof(WSAQUERYSET)) {
        Err = WSAEFAULT;
        goto nlsb_error;
    }

    pwszTemp = LocalAlloc(LPTR, 1024*sizeof(WCHAR));
    if (!pwszTemp) {
        Err = WSA_NOT_ENOUGH_MEMORY;
        goto nlsb_error;
    }

    // we assume if ws2_32 called us that the GUID is correct

    pSvcClassId = pRestrict->lpServiceClassId;

    if (!pSvcClassId) {
        Err = WSAEINVAL;
    } else if ((ControlFlags & LUP_CONTAINERS) && (ControlFlags & LUP_NOCONTAINERS)) {
        Err = WSAEINVAL;
    } else if (ControlFlags & LUP_CONTAINERS) {
        Err = WSANO_DATA;
    } else if (pRestrict->dwNumberOfProtocols &&
        (!CheckAddressFamily(pRestrict))) {
        Err = WSAEINVAL;
    } else if ((pRestrict->lpszContext) && (pRestrict->lpszContext[0]) &&
                wcscmp(pRestrict->lpszContext, TEXT("\\"))) {
        // if there is a context and it's not the default context then fail
        Err = WSANO_DATA;
    } else {
        // ok finally do the work...
        Flags = 0;

        if (0 == memcmp(pSvcClassId, &AddressGuid, sizeof(GUID))) {
            Flags |= REVERSE_LOOKUP;
        } else if (0 == memcmp(pSvcClassId, &IANAGuid, sizeof(GUID))) {
            Flags |= IANA_LOOKUP;
            ControlFlags &= ~LUP_RETURN_ADDR;
        } else if (0 == memcmp(pSvcClassId, &Ipv6Guid, sizeof(GUID))) {
            Flags |= V6_LOOKUP;
        }

        fNameLookup = (0 == memcmp(pSvcClassId, &HostnameGuid, sizeof(GUID)))
                        || IS_SVCID_TCP(pSvcClassId)
                        || IS_SVCID_UDP(pSvcClassId)
            || (0 == memcmp(pSvcClassId, &InetHostName, sizeof(GUID)))
            || (0 == memcmp(pSvcClassId, &Ipv6Guid, sizeof(GUID)))
            || (0 == memcmp(pSvcClassId, &AtmaGuid, sizeof(GUID)));

        pwszSvcName = pRestrict->lpszServiceInstanceName;
        if (!pwszSvcName || ! (*pwszSvcName)) {
            if (fNameLookup) {
                Flags |= LOCAL_LOOKUP;
                pwszSvcName = TEXT("");
            } else if ( (REVERSE_LOOKUP & Flags) && pRestrict->lpcsaBuffer &&
                (1 == pRestrict->dwNumberOfCsAddrs)) {

                pSAddr = (SOCKADDR_IN *)pRestrict->lpcsaBuffer->
                                                RemoteAddr.lpSockaddr;
                if (MyInetNtoaW(pwszSockAddr, pSAddr->sin_addr))
                    pwszSvcName = pwszTemp;
                else {
                    Err = WSAEINVAL;
                    // break out somehow?
                }
            } else {
                Err = WSAEINVAL;
                // break out somehow??
            }
        } else if (fNameLookup) {
            // check for special names
            if (DnsNameCompare_W(pwszSvcName, TEXT("localhost")) ||
                DnsNameCompare_W(pwszSvcName, TEXT("loopback")))
                Flags |= LOCAL_LOOKUP | LOOP_LOOKUP;
            // BUGBUG: also check computer name
            //  else if (DnsNameCompare_W(pwszSvcName, DeviceName)
            //  Flags |= LOCAL_LOOKUP;

        }

        if (! Err) {
            // compare protocols to return
            cProtocols = pRestrict->dwNumberOfProtocols;
            if (cProtocols) {
                pAfProtocols = pRestrict->lpafpProtocols;
                while (cProtocols-- && pAfProtocols) {
                    AddrFamily = pAfProtocols->iAddressFamily;
                    if (AF_UNSPEC == AddrFamily) {
                        Protocols = 0x3;
                        break;
                    } else if (AF_INET==AddrFamily || AF_INET6==AddrFamily) {
                        Prot = pRestrict->lpafpProtocols->iProtocol;
                        if (!Prot) {
                            Protocols = 0x3;
                            break;
                        } else if (IPPROTO_UDP == Prot) {
                            Protocols |= 0x1;
                        } else if (IPPROTO_TCP == Prot) {
                            Protocols |= 0x2;
                        }
                        // otherwise we don't do anything just in case someone
                        // adds a new protocol we don't recognize
                    }
                    pAfProtocols++;
                }
            } else {
                Protocols = 0x3;
            }
            // get the server and protocols if there is a string

            if (Protocols) {
                // ...

                // if everything ok get a handle
                // don't add 1 here b/c pQuery already has an array of WCHAR[1]
                cSvcName = (wcslen(pwszSvcName)) * sizeof(WCHAR);
                pQuery = LocalAlloc(LPTR, sizeof(*pQuery) + cSvcName);
                if (pQuery) {
                    pQuery->cRefs = 1;
                    pQuery->Flags = Flags;
                    pQuery->InterfaceIndex = (DWORD)pRestrict->lpszComment; // Overloaded to plumb interface index (MSDN says this field is ignored for queries)
                    pQuery->ControlFlags = ControlFlags;
                    memcpy(&pQuery->Id, pSvcClassId, sizeof(GUID));
                    StringCbCopyW(pQuery->pwszSvcName, cSvcName + sizeof(WCHAR), pwszSvcName);

                    // set other fields
                    EnterCriticalSection(&v_NspmCS);
                    pQuery->pNext = v_pQueryList;
                    v_pQueryList = pQuery;
                    LeaveCriticalSection(&v_NspmCS);
                    *phLookup = (HANDLE)pQuery;

                } else
                    Err = WSA_NOT_ENOUGH_MEMORY;

            } else
                Err = WSANO_DATA;
        }
    }

nlsb_error:
    LocalFree(pwszTemp);    // LocalFree checks for NULL.

    if (Err) {
        SetLastError(Err);
        Err = SOCKET_ERROR;
    }

    return Err;

}   // NSPLookupServiceBegin()

#define NAME_SIZE   168 // make this a multiple of 4

INT WSAAPI NSPLookupServiceNext(
    HANDLE          hLookup,
    DWORD           ControlFlags,
    DWORD           *pcBuf,
    LPWSAQUERYSETW  pResults) {

    int             Err = 0;
    QueryContext    *pQuery, **ppQuery;
    WSAQUERYSETW    QuerySet = {0};
    SOCK_THREAD     *pThread;
    HOSTENT         *pHost;
    CSADDR_INFO     *pCsAddr;
    SOCKADDR        *pSAddr;
    SOCKADDR_IN     *pSAddrIn;
    SOCKADDR_IN6    *pSAddrIn6;
    BYTE            *p = NULL;
    unsigned int    cBuf = 0;
    unsigned int    cAddr;
    int             i, cSize;
    int             cName = 0;
    BLOB            *pBlob;
    WCHAR           wName[NAME_SIZE];
    DWORD           IPAddr[4];
    HKEY            hKey;
    LONG            hRet;
    DWORD           Type;
    int             fLocalLookup;
    AFD_OPTIONS     AfdOptions;
    ushort          Family;

    // BUGBUG: check NSPStartup called or not
    if(!hLookup || !pcBuf || !pResults) {
        Err = WSAEFAULT;
    } else if ((ControlFlags & LUP_CONTAINERS) && (ControlFlags & LUP_NOCONTAINERS)) {
        Err = WSAEINVAL;
    } else if (ControlFlags & LUP_CONTAINERS) {
        Err = WSANO_DATA;
    } else {
        if (! pResults && *pcBuf) {
            // the caller lied to us!
            *pcBuf = 0;
        }
        if (*pcBuf < sizeof(WSAQUERYSETW)) {
            pResults = &QuerySet;
        }

        memset(pResults, 0, sizeof(WSAQUERYSETW));
        pResults->dwNameSpace = NS_DNS;
        pResults->dwSize = sizeof(WSAQUERYSETW);

        EnterCriticalSection(&v_NspmCS);
        pQuery = *_FindQuery(hLookup);
        pQuery->cRefs++;
        LeaveCriticalSection(&v_NspmCS);

        if (pQuery) {
            if (0 == pQuery->InterfaceIndex) {
                AfdOptions.InterfaceIndex = AFD_OPTIONS_ALL_INTERFACES; // Search over all interfaces.
            } else {
                AfdOptions.InterfaceIndex = pQuery->InterfaceIndex; // Interface index was specified
            }
            if (V6_LOOKUP & pQuery->Flags) {
                AfdOptions.Type = T_AAAA;
                cAddr = sizeof(SOCKADDR_IN6);
                Family = AF_INET6;
            } else {
                AfdOptions.Type = T_A;
                cAddr = sizeof(SOCKADDR_IN);
                Family = AF_INET;
            }


            if (DONE_LOOKUP & pQuery->Flags) {
                Err = WSA_E_NO_MORE;
            } else if (IANA_LOOKUP & pQuery->Flags) {
                ASSERT(0);


            } else if (REVERSE_LOOKUP & pQuery->Flags) {

                if (AF_INET6 == Family) {
                    // we don't yet handle reverse queries for v6
                    Err = WSANO_DATA;
                    goto Done;
                } else {
                    Err = MyInetAddrW(pQuery->pwszSvcName, IPAddr);
                    if (Err != 0) {
                        goto Done;
                    }
                }
                if (INADDR_NONE != IPAddr[0]) {
                    pThread = LocalAlloc(LPTR, sizeof(*pThread));
                    if (pThread) {
                        if (AFDGetHostentByAttr(pThread, sizeof(*pThread),
                            NULL, (BYTE *)IPAddr, &AfdOptions, sizeof(AfdOptions))) {

                            pHost = &(pThread->GETHOST_host);

                            p = (BYTE *)(pResults + 1);
                            cBuf = (DWORD)(pResults + 1) - (DWORD)pResults;

                            ASSERT(!((DWORD)p & 0x3));

                            memset(&QuerySet, 0, sizeof(QuerySet));
                            // copy stuff into the query structure...
                            QuerySet.dwSize = sizeof(QuerySet);

                            // very silly: copy GUID from LookupServiceBegin
                            if (LUP_RETURN_TYPE & pQuery->ControlFlags) {
                                cSize = sizeof(GUID);
                                cBuf += cSize;
                                if (cBuf <= *pcBuf) {
                                    QuerySet.lpServiceClassId = (GUID *)p;
                                    memcpy(p, &pQuery->Id, cSize);
                                    p += cSize;

                                    ASSERT(!((DWORD)p & 0x3));
                                }
                            }
                            // seems to be not set by NT
                            QuerySet.dwNameSpace = NS_DNS;

                            cSize = sizeof(GUID);
                            cBuf += cSize;
                            if (cBuf <= *pcBuf) {
                                QuerySet.lpNSProviderId = (GUID *)p;
                                memcpy(p, &NsId, cSize);
                                p += cSize;

                                ASSERT(!((DWORD)p & 0x3));
                            }

                            ASSERT(!((DWORD)p & 0x3));

                            if (LUP_RETURN_ADDR & pQuery->ControlFlags) {
                                // we'll only add one address for now...the one
                                // that was given to us
                                i = 1;
                                // each pCsAddr will have two pointers to
                                // SOCKADDRS so calculate them in too...
                                cSize = sizeof(*pCsAddr) + (cAddr << 1);
                                cBuf += cSize;

                                ASSERT(!((DWORD)cSize & 0x3));

                                // if they're not all going to fit in, then don't
                                // return partial info...
                                if (cBuf <= *pcBuf) {
                                    QuerySet.dwNumberOfCsAddrs = i;
                                    QuerySet.lpcsaBuffer = pCsAddr =
                                        (CSADDR_INFO *)p;
                                    p += sizeof(*pCsAddr) * i;

                                    pSAddr = (SOCKADDR *)p;

                                    // now copy them in...
                                    memset(pCsAddr, 0, sizeof(*pCsAddr));
                                    // copy the non-pointer info first

                                    // BUGBUG: how do we know if it is tcp/udp
                                    pCsAddr[0].iSocketType = SOCK_STREAM;
                                    pCsAddr[0].iProtocol = IPPROTO_TCP;

                                    pCsAddr[0].LocalAddr.iSockaddrLength =
                                        pCsAddr[0].RemoteAddr.iSockaddrLength
                                        = cAddr;

                                    pCsAddr[0].LocalAddr.lpSockaddr = pSAddr;
                                    memset(pSAddr, 0, cAddr);
                                    pSAddr->sa_family = Family;
                                    // local address will be blank

                                    p += cAddr;
                                    ASSERT(!((DWORD)p & 0x3));
                                    pSAddr = (SOCKADDR *)p;

                                    pCsAddr[0].RemoteAddr.lpSockaddr = pSAddr;
                                    memset(pSAddr, 0, cAddr);
                                    // pSAddrIn->sin_port already memset to 0
                                    pSAddr->sa_family = Family;
                                    if (Family == AF_INET) {
                                        pSAddrIn = (SOCKADDR_IN *)p;
                                        pSAddrIn->sin_addr.s_addr = IPAddr[0];
                                    } else {
                                        pSAddrIn6 = (SOCKADDR_IN6 *)p;
                                        memcpy(&pSAddrIn6->sin6_addr, IPAddr,
                                            sizeof(struct in6_addr));
                                    }

                                    p += cAddr;
                                    ASSERT(!((DWORD)p & 0x3));

                                }   // if (cBuf <= *pcBuf)
                            }   // if (LUP_RETURN_ADDR & pQuery->ControlFlags

                            // if this is an alias set this to RESULT_IS_ALIAS
    //                      QuerySet.dwOutputFlags = 0;

                            if (LUP_RETURN_BLOB & pQuery->ControlFlags) {
                                QuerySet.lpBlob = pBlob = (BLOB *)p;
                                // make sure this is a SVCID_INET_HOSTADDRBYNAME
                                cSize = sizeof(BLOB);
                                cBuf += cSize;
                                if (cBuf <= *pcBuf) {
                                    p += cSize;
                                    ASSERT(!((DWORD)p & 0x3));
                                    pBlob->pBlobData = p;
                                }

                                Err = CopyHostentToBlob(pHost, p, *pcBuf - cBuf,
                                    &cSize);
                                if (! Err) {
                                    PointerToOffset((HOSTENT*)p);
                                    pBlob->cbSize = cSize;
                                    cSize = (cSize + 3) & ~3;
                                    p += cSize;
                                    cBuf += cSize;
                                } else {
                                    cSize = (cSize + 3) & ~3;
                                    cBuf += cSize;
                                }
                                ASSERT(!((DWORD)p & 0x3));
                            }

                            if (LUP_RETURN_NAME & pQuery->ControlFlags) {

                                cName = strlen(pHost->h_name) + 1;
                                cSize = ((cName *2) + 3) & (~3);
                                ASSERT(!((DWORD)cSize & 0x3));

                                cBuf += cSize;
                                if (cBuf <= *pcBuf) {
                                    QuerySet.lpszServiceInstanceName = (WCHAR *)p;
                                    MultiByteToWideChar(CP_ACP, 0, pHost->h_name, -1, (WCHAR *)p, cName);
                                    p += cSize;
                                    ASSERT(!((DWORD)p & 0x3));
                                }
                            }
                            if (cBuf > *pcBuf) {
                                Err = WSAEFAULT;
                                *pcBuf = cBuf;
                            } else
                                memcpy(pResults, &QuerySet, sizeof(QuerySet));


                        } else  // if (AFDGetHostentByAttr...)
                            Err = GetLastError();

                        LocalFree(pThread);
                    } else
                        Err = WSA_NOT_ENOUGH_MEMORY;


                } else {
                    // The client passed in something like INADDR_NONE
                    Err = WSAEINVAL;
                }

            } else {

                if (LOCAL_LOOKUP & pQuery->Flags) {
                    fLocalLookup = TRUE;

                    if (! ((LUP_RETURN_NAME | LUP_RETURN_ADDR) &
                        pQuery->ControlFlags)) {
                        Err = WSAEINVAL;
                    } else if (
                        ! ((LUP_RETURN_NAME | LUP_RETURN_ADDR |LUP_RETURN_BLOB)
                        & pQuery->ControlFlags)) {
                        Err = WSAEINVAL;
                    }
                    if (Err)
                        goto Done;

                    p = (BYTE *)(pResults + 1);
                    cBuf = (DWORD)(pResults + 1) - (DWORD)pResults;

                    memset(&QuerySet, 0, sizeof(QuerySet));

                    // copy stuff into the query structure...
                    QuerySet.dwSize = sizeof(QuerySet);

                    QuerySet.dwNameSpace = NS_DNS;
                    cSize = sizeof(GUID);
                    cBuf += cSize;
                    if (cBuf <= *pcBuf) {
                        QuerySet.lpNSProviderId = (GUID *)p;
                        memcpy(p, &NsId, cSize);
                        p += cSize;
                    }
                    ASSERT(!((DWORD)p & 0x3));

                    i = 0;
                    if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_LOCAL_MACHINE,
                                                TEXT("Ident"), 0, 0, &hKey)) {
                        cName = sizeof(wName);
                        hRet = RegQueryValueEx(hKey, TEXT("Name"), 0, &Type,
                            (LPBYTE)wName, (LPDWORD)&cName);
                        RegCloseKey (hKey);

                        if ((hRet == ERROR_SUCCESS) && (Type == REG_SZ)) {
                            i++;
                        }
                    }
                    if (! i) {
                        StringCchCopyW(wName, NAME_SIZE, TEXT("WindowsCE"));
                        cName = 20;
                    }
                    cSize = (cName + 3) & (~3);
                    ASSERT(!((DWORD)cSize & 0x3));

                    if (LUP_RETURN_NAME & pQuery->ControlFlags) {
                        cBuf += cSize;
                        if (cBuf <= *pcBuf) {
                            QuerySet.lpszServiceInstanceName = (WCHAR *)p;
                            memcpy(p, wName, cName);
                            p += cSize;
                            ASSERT(!((DWORD)p & 0x3));
                        }
                    }

                    // very silly: copy GUID from LookupServiceBegin
                    if (LUP_RETURN_TYPE & pQuery->ControlFlags) {
                        cSize = sizeof(GUID);
                        cBuf += cSize;
                        if (cBuf <= *pcBuf) {
                            QuerySet.lpServiceClassId = (GUID *)p;
                            memcpy(p, &pQuery->Id, cSize);
                            p += cSize;

                            ASSERT(!((DWORD)p & 0x3));
                        }
                    }

                    if (! ((LUP_RETURN_ADDR | LUP_RETURN_BLOB) & pQuery->ControlFlags)) {
                        if (cBuf > *pcBuf) {
                            Err = WSAEFAULT;
                            *pcBuf = cBuf;
                        } else
                            memcpy(pResults, &QuerySet, sizeof(QuerySet));

                        goto Done;
                    }

                } else {
                    fLocalLookup = FALSE;
                }

                pThread = LocalAlloc(LPTR, sizeof(*pThread));
                if (pThread) {
                    WCHAR   *wp;

                    if (fLocalLookup && (! (LOOP_LOOKUP & pQuery->Flags))) {
                        wp = wName;
                    } else {
                        wp = pQuery->pwszSvcName;
                    }

                    if (AFDGetHostentByAttr(pThread, sizeof(*pThread), wp,
                        NULL, &AfdOptions, sizeof(AfdOptions))) {
                        pHost = &(pThread->GETHOST_host);

                        if (! fLocalLookup) {
                            p = (BYTE *)(pResults + 1);
                            cBuf = (DWORD)(pResults + 1) - (DWORD)pResults;

                            ASSERT(!((DWORD)p & 0x3));

                            memset(&QuerySet, 0, sizeof(QuerySet));
                            // copy stuff into the query structure...
                            QuerySet.dwSize = sizeof(QuerySet);

                            // very silly: copy GUID from LookupServiceBegin
                            if (LUP_RETURN_TYPE & pQuery->ControlFlags) {
                                cSize = sizeof(GUID);
                                cBuf += cSize;
                                if (cBuf <= *pcBuf) {
                                    QuerySet.lpServiceClassId = (GUID *)p;
                                    memcpy(p, &pQuery->Id, cSize);
                                    p += cSize;

                                    ASSERT(!((DWORD)p & 0x3));
                                }
                            }

                            QuerySet.dwNameSpace = NS_DNS;

                            cSize = sizeof(GUID);
                            cBuf += cSize;
                            if (cBuf <= *pcBuf) {
                                QuerySet.lpNSProviderId = (GUID *)p;
                                memcpy(p, &NsId, cSize);
                                p += cSize;

                                ASSERT(!((DWORD)p & 0x3));
                            }

                            ASSERT(!((DWORD)p & 0x3));
                        }   // if (!fLocalLookup)

                        if (LUP_RETURN_ADDR & pQuery->ControlFlags) {
                            // first count the addresses
                            for (i = 0; pHost->h_addr_list[i]; i++)
                                ;

                            ASSERT(i);

                            // each pCsAddr will have two pointers to
                            // SOCKADDRS so calculate them in too...
                            cSize = sizeof(*pCsAddr) + (cAddr << 1);
                            cBuf += cSize * i;

                            ASSERT(!((DWORD)cSize & 0x3));

                            // if they're not all going to fit in, then don't
                            // return partial info...
                            if (cBuf <= *pcBuf) {
                                QuerySet.dwNumberOfCsAddrs = i;
                                QuerySet.lpcsaBuffer = pCsAddr =
                                    (CSADDR_INFO *)p;
                                p += sizeof(*pCsAddr) * i;

                                ASSERT(!((DWORD)p & 0x3));

                                pSAddr = (SOCKADDR *)p;

                                // now copy them in...
                                for (i = 0; pHost->h_addr_list[i]; i++) {
                                    PREFAST_SUPPRESS(394, "cBuf is incremented by sizeof(*pCsAddr) earlier & checked against *pcBuf")
                                    memset(&pCsAddr[i], 0, sizeof(*pCsAddr));
                                    // copy the non-pointer info first

                                    // BUGBUG: how do we know if it is tcp/udp
                                    pCsAddr[i].iSocketType = SOCK_STREAM;
                                    pCsAddr[i].iProtocol = IPPROTO_TCP;

                                    pCsAddr[i].LocalAddr.iSockaddrLength =
                                        pCsAddr[i].RemoteAddr.iSockaddrLength
                                        = cAddr;

                                    pCsAddr[i].LocalAddr.lpSockaddr = pSAddr;
                                    memset(pSAddr, 0, cAddr);
                                    pSAddr->sa_family = Family;
                                    // local address will be blank

                                    p += cAddr;
                                    ASSERT(!((DWORD)p & 0x3));
                                    pSAddr = (SOCKADDR *)p;

                                    pCsAddr[i].RemoteAddr.lpSockaddr = pSAddr;
                                    memset(pSAddr, 0, cAddr);
                                    pSAddr->sa_family = Family;
                                    // pSAddrIn->sin_port already memset to 0
                                    if (AF_INET == Family) {
                                        pSAddrIn = (SOCKADDR_IN *)pSAddr;
                                        pSAddrIn->sin_addr.s_addr =
                                            *(DWORD *)pHost->h_addr_list[i];
                                    } else {
                                        pSAddrIn6 = (SOCKADDR_IN6 *)pSAddr;
                                        memcpy(&pSAddrIn6->sin6_addr,
                                            pHost->h_addr_list[i],
                                            sizeof(struct in6_addr) + sizeof(u_long));  // IPv6 address and scope Id
                                    }

                                    p += cAddr;
                                    ASSERT(!((DWORD)p & 0x3));
                                    pSAddr = (SOCKADDR *)p;

                                }   // for (pHost->h_addr_list[i])

                            }   // if (cBuf <= *pcBuf)
                        }   // if (LUP_RETURN_ADDR & pQuery->ControlFlags

                        // if this is an alias set this to RESULT_IS_ALIAS
//                      QuerySet.dwOutputFlags = 0;

                        if (LUP_RETURN_BLOB & pQuery->ControlFlags) {
                            QuerySet.lpBlob = pBlob = (BLOB *)p;
                            // make sure this is a SVCID_INET_HOSTADDRBYNAME
                            cSize = sizeof(BLOB);
                            cBuf += cSize;
                            if (cBuf <= *pcBuf) {
                                p += cSize;
                                pBlob->pBlobData = p;

                                ASSERT(!((DWORD)p & 0x3));
                            }

                            Err = CopyHostentToBlob(pHost, p, *pcBuf - cBuf,
                                &cSize);
                            if (! Err) {
                                PointerToOffset((HOSTENT*)p);
                                pBlob->cbSize = cSize;
                                cSize = (cSize + 3) & ~3;
                                p += cSize;
                                cBuf += cSize;
                            } else {
                                cSize = (cSize + 3) & ~3;
                                cBuf += cSize;
                            }
                            ASSERT(!((DWORD)p & 0x3));
                        }

                        if ((LUP_RETURN_NAME & pQuery->ControlFlags) &&
                            ! fLocalLookup) {

                            cName = strlen(pHost->h_name) + 1;
                            cSize = cName << 1;
                            cSize = (cSize + 3) & (~3);

                            cBuf += cSize;
                            if (cBuf <= *pcBuf) {
                                QuerySet.lpszServiceInstanceName = (WCHAR *)p;
                                MultiByteToWideChar(CP_ACP, 0, pHost->h_name, -1, (WCHAR *)p, cName);
                                p += cSize;
                                ASSERT(!((DWORD)p & 0x3));
                            }
                        }
                        if (cBuf > *pcBuf) {
                            Err = WSAEFAULT;
                            *pcBuf = cBuf;
                        } else
                            memcpy(pResults, &QuerySet, sizeof(QuerySet));

                    } else  // if (AFDGetHostentByAttr...)
                        Err = GetLastError();
                    LocalFree(pThread);
                } else
                    Err = WSA_NOT_ENOUGH_MEMORY;

            }

Done:
            EnterCriticalSection(&v_NspmCS);
            ppQuery = _FindQuery(hLookup);
            ASSERT(pQuery == *ppQuery);

            pQuery = *ppQuery;
            if (pQuery) {
                if (--(pQuery->cRefs)) {
                    if (! Err)
                        pQuery->Flags |= DONE_LOOKUP;
                } else {
                    ASSERT(pQuery->Flags & DNS_F_END_CALLED);
                    // no one is using him, so remove him
                    *ppQuery = pQuery->pNext;
                    LocalFree(pQuery);
                }
            } else {
                RETAILMSG(1, (TEXT("NSPLookupServiceNext: pQuery dissappeared\r\n")));
            }
            LeaveCriticalSection(&v_NspmCS);


        } else  // if (pQuery)
            Err = WSA_INVALID_HANDLE;
    }


    if (Err) {
        SetLastError(Err);
        Err = SOCKET_ERROR;
    }

    return Err;

}   // NSPLookupServiceNext()


INT WSAAPI NSPLookupServiceEnd(
    HANDLE hLookup) {

    QueryContext    **ppQuery, *pQuery;
    int             Err = 0;

    EnterCriticalSection(&v_NspmCS);
    ppQuery = _FindQuery(hLookup);
    pQuery = *ppQuery;
    if (pQuery) {
        if (--(pQuery->cRefs)) {
            pQuery->Flags |= DNS_F_END_CALLED;
            pQuery = NULL;
        } else {
            // no one is using him, so remove him
            *ppQuery = pQuery->pNext;
        }
    } else
        Err = WSA_INVALID_HANDLE;

    LeaveCriticalSection(&v_NspmCS);
    if (pQuery)
        LocalFree(pQuery);

    if (Err) {
        SetLastError(Err);
        Err = SOCKET_ERROR;
    }

    return Err;

}   // NSPLookupServiceNext()


INT WSAAPI NSPSetService(
    LPGUID lpProviderId,
    LPWSASERVICECLASSINFOW lpServiceClassInfo,
    LPWSAQUERYSETW lpqsRegInfo,
    WSAESETSERVICEOP essOperation,
    DWORD dwControlFlags) {

    // this operation is not supported for DNS
    // too bad the spec doesn't really say which error code to return

    SetLastError(WSAEOPNOTSUPP);
    return SOCKET_ERROR;

}   // NSPSetService()


INT WSAAPI NSPInstallServiceClass(
    LPGUID  pProviderId,
    LPWSASERVICECLASSINFOW  pServiceClass) {

    HKEY                hKey, hServiceKey;
    LPWSANSCLASSINFOW   pClassInfo;
    int                 Err, i;
    DWORD               dwDisposition;

    Err = RegCreateKeyEx(HKEY_LOCAL_MACHINE, NSP_SERVICE_KEY_NAME, 0,
                NULL, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL,
                &hKey, &dwDisposition );

    if (! Err) {
        hServiceKey = NULL;

        __try {

            Err = RegCreateKeyEx(hKey, pServiceClass->lpszServiceClassName,
                    0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE,
                    NULL, &hServiceKey, &dwDisposition);

            if (! Err) {
                Err = RegSetValueEx(hServiceKey, TEXT("GUID"), 0, REG_BINARY,
                        (BYTE *)pServiceClass->lpServiceClassId,
                        sizeof(GUID));

                if (! Err) {
                    pClassInfo = pServiceClass->lpClassInfos;
                    for (i = pServiceClass->dwCount; i && ! Err; i--) {

                        if (NS_DNS == pClassInfo->dwNameSpace) {
                            Err = RegSetValueEx(hServiceKey,
                                    pClassInfo->lpszName, 0,
                                    pClassInfo->dwValueType,
                                    (BYTE *)pClassInfo->lpValue,
                                    pClassInfo->dwValueSize);
                        }
                        pClassInfo++;
                    }   // for (i && ! Err)
                }
                RegCloseKey(hServiceKey);
            }
        }   // __try

        __except (EXCEPTION_EXECUTE_HANDLER) {
            Err = WSAEFAULT;
            if (NULL != hServiceKey)
                RegCloseKey(hServiceKey);
            ASSERT(0);
        }

        RegCloseKey(hKey);
    }

    if (Err) {
        SetLastError(Err);
        Err = SOCKET_ERROR;
    }

    return Err;

}   // NSPInstallServiceClass()


INT WSAAPI NSPRemoveServiceClass(
    LPGUID lpProviderId,
    LPGUID lpServiceClassId) {

    // NT doesn't implement this though they do implement install

    SetLastError(WSAEOPNOTSUPP);
    return SOCKET_ERROR;


}   // NSPRemoveServiceClass()


INT WSAAPI NSPGetServiceClassInfo(
    LPGUID lpProviderId,
    LPDWORD lpdwBufSize,
    LPWSASERVICECLASSINFOW lpServiceClass) {

    // this operation is not supported for DNS
    // too bad the spec doesn't really say which error code to return

    SetLastError(WSAEOPNOTSUPP);
    return SOCKET_ERROR;

}   // NSPGetServiceClassInfo()


INT WSAAPI NSPIoctl(
    HANDLE          hLookup,
    DWORD           dwControlCode,
    LPVOID          lpvInBuffer,
    DWORD           cbInBuffer,
    LPVOID          lpvOutBuffer,
    DWORD           cbOutBuffer,
    LPDWORD         lpcbBytesReturned,
    LPWSACOMPLETION lpCompletion,
    LPWSATHREADID   lpThreadId) {

    // unspeced function...

    SetLastError(WSAEOPNOTSUPP);
    return SOCKET_ERROR;

}   // NSPIoctl()




