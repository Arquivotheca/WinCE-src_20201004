//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*****************************************************************************/
/**                            Microsoft Windows                            **/
/*****************************************************************************/

/*

gxy.c

get x by y functions

FILE HISTORY:
	OmarM		11-Oct-2000

	OmarM		20-May-2001
		implemented getaddrinfo getnameinfo--copied NT's architecture for these

*/


#include "winsock2p.h"
#include "ws2tcpip.h"
#include "svcguid.h"
#include "windev.h"
#include "wscntl.h"

GUID HostnameGuid = SVCID_HOSTNAME;
GUID AddressGuid = SVCID_INET_HOSTADDRBYINETSTRING;
GUID InetHostName = SVCID_INET_HOSTADDRBYNAME;
GUID IANAGuid = SVCID_INET_SERVICEBYNAME;
GUID AtmaGuid = SVCID_DNS_TYPE_ATMA;
GUID Ipv6Guid = SVCID_DNS_TYPE_AAAA;

#define DEFAULT_QUERY_SIZE	(sizeof(WSAQUERYSET) + 1024)

extern void MatchCurrentThreadPriority(HANDLE hThd);

AFPROTOCOLS AfProtocols[2] = {{AF_INET, IPPROTO_UDP}, {AF_INET, IPPROTO_TCP}};


// *ppResults must be properly (DWORD) aligned
BLOB *getxyDataEnt(
	IN OUT	CHAR		**ppResults,
	IN		DWORD		cLen,
	IN		const char	*pszName,
	IN		LPGUID		pType,
	OUT		LPWSTR		*ppName OPTIONAL) {

	int				Err, fNewBuf;
	WSAQUERYSETW	*pQuery;
	HANDLE			hQuery;
	DWORD			cQuery;
	BLOB			*pBlob;
	int				cName;
	WCHAR			*pwszName;
	int				cConverted;

	// make sure it is dword aligned
	ASSERT(0 == ((DWORD)(*ppResults) & 0x3));
	if (ppName)
		*ppName = NULL;

	pBlob = NULL;
	pQuery = (WSAQUERYSETW *)*ppResults;
	cQuery = cLen;

	cName = strlen(pszName) + 1;
	if (pwszName = LocalAlloc(LPTR, sizeof(WCHAR) * cName)) {

		cConverted = MultiByteToWideChar(CP_ACP, 0, pszName, -1, pwszName, cName);
		if (cConverted) {
			memset(pQuery, 0, sizeof(*pQuery));
			pQuery->dwSize = sizeof(*pQuery);
			pQuery->lpszServiceInstanceName = pwszName;
			pQuery->lpServiceClassId = pType;
			if (pType == &InetHostName) {
				pQuery->dwNameSpace = NS_DNS;
			} else {
				pQuery->dwNameSpace = NS_ALL;
			}
			pQuery->dwNumberOfProtocols = 2;
			pQuery->lpafpProtocols = AfProtocols;

			Err = WSALookupServiceBegin(pQuery, 
				LUP_RETURN_BLOB | LUP_RETURN_NAME, &hQuery);

			if (! Err) {
				do {
					fNewBuf = FALSE;
					Err = WSALookupServiceNext(hQuery, 0, &cLen, pQuery);

					if (Err) {
						Err = GetLastError();

						// check if buffer was too small
						if (WSAEFAULT == Err && cLen > cQuery) {
							LocalFree(*ppResults);
							*ppResults = (char *)pQuery = 
								LocalAlloc(LPTR, cLen);
							if (pQuery) {
								cQuery = cLen;
								fNewBuf = TRUE;
							} else {
								Err = WSA_NOT_ENOUGH_MEMORY;
							}
						}
					} else {
						if (pBlob = pQuery->lpBlob) {
							if (ppName)
								*ppName = pQuery->lpszServiceInstanceName;
						} else {
							if (&HostnameGuid == pType) {
								if (ppName)
									*ppName = pQuery->lpszServiceInstanceName;
							} else
								Err = WSANO_DATA;
						}
					}	// else Err = WSALookupServiceNext
					
				} while (fNewBuf);
				WSALookupServiceEnd(hQuery);
			} else {
				Err = GetLastError();   // WSALookupServiceBegin failed
			}
		} else {	// if MultiByteToWideChar
			Err = WSANO_RECOVERY;
		}
		LocalFree(pwszName);
	} else {	// if pwszName = LocalAlloc...
		Err = WSA_NOT_ENOUGH_MEMORY;
	}

	if (Err) {
		SetLastError(Err);
	}
	return pBlob;

}	// getxyDataEnt()


void OffsetToPointer(HOSTENT *pHost) {
	int		i;
	char	*p;

	// if we use ptrs instead of i (an array) it should be a little more
	// efficient, but it makes the code harder to read

	p = (char *)pHost;

	pHost->h_name = p + (int)pHost->h_name;
	if (pHost->h_aliases) {
		pHost->h_aliases = (char **)(p + (int)pHost->h_aliases);
		for (i = 0; pHost->h_aliases[i]; i++) {
			pHost->h_aliases[i] = p + (int)pHost->h_aliases[i];
		}
		
	}
	pHost->h_addr_list = (char **)(p + (int)pHost->h_addr_list);	
	for (i = 0; pHost->h_addr_list[i]; i++)
		pHost->h_addr_list[i] = p + (int)pHost->h_addr_list[i];

}	// OffsetToPointer()


void CopyHostentToThread(HOSTENT *pHost, SOCK_THREAD *pThread) {
	HOSTENT	*pTHost;
	char	*p;
	int		i, cRemaining, cLen;

	pTHost = &pThread->GETHOST_host;

	pTHost->h_addrtype = pHost->h_addrtype;
	pTHost->h_length = pHost->h_length;	
	
	// copy the addresses first
	p = pThread->GETHOST_hostaddr;
	pTHost->h_addr_list = pThread->GETHOST_h_addr_ptrs;
	for (i = 0; pHost->h_addr_list[i]; i++) {
		pThread->GETHOST_h_addr_ptrs[i] = p;
		memcpy(p, pHost->h_addr_list[i], 4);
		p += 4;
	}
	pThread->GETHOST_h_addr_ptrs[i] = NULL;

	// copy the name
	pTHost->h_name = pThread->GETHOST_hostbuf;
	strcpy(pTHost->h_name, pHost->h_name);
	
    // copy the aliases
    cLen = strlen(pTHost->h_name) + 1;
    cRemaining = sizeof(pThread->GETHOST_hostbuf) - cLen;
    p = pThread->GETHOST_hostbuf + cLen;
    pTHost->h_aliases = pThread->GETHOST_host_aliases;
    for (i = 0; pHost->h_aliases[i]; i++) {
        cLen = strlen(pHost->h_aliases[i]) + 1;
        if (cRemaining >= cLen) {
            strcpy(p, pHost->h_aliases[i]);
            pThread->GETHOST_host_aliases[i] = p;
        } else {
            break;
        }
        p += cLen;
        cRemaining -= cLen;
    }
    pThread->GETHOST_host_aliases[i] = NULL;
    
}	// CopyHostentToThread()






































struct servent * WSAAPI getservbyname(
	IN const char	*pszName,
	IN const char	*pszProto) {

	SetLastError(WSANO_RECOVERY);
	return NULL;

//	we don't actually implement this right now, but if we did, this is what
//	we'd have to do at the Ws2 layer.











































}	// getservbyname()


struct servent * WSAAPI getservbyport(
	IN int			port,
	IN const char	*pszProto) {

	SetLastError(WSANO_RECOVERY);
	return NULL;

//	we don't actually implement this right now, but if we did, this is what
//	we'd have to do at the Ws2 layer.





































}	// getservbyport()


struct hostent *WSAAPI gethostbyaddr (
    IN const char FAR * addr,
    IN int len,
    IN int type) {

	int				Err;
	struct hostent	*pHost;
	BLOB			*pBlob;
	char			*pResults;
	SOCK_THREAD		*pThread;
	char			aName[32];	// 16 is all that is needed for INET addresses

	Err = 0;
	pHost = NULL;
	// check if we've been started up.

	if (addr) {

		sprintf(aName, "%u.%u.%u.%u", ((unsigned)addr[0] & 0xff), 
			((unsigned)addr[1] & 0xff), ((unsigned)addr[2] & 0xff), 
			((unsigned)addr[3] & 0xff));
		
		if (GetThreadData(&pThread)) {
			if (pResults = LocalAlloc(LPTR, DEFAULT_QUERY_SIZE)) {
				pBlob = getxyDataEnt(&pResults, DEFAULT_QUERY_SIZE, aName, 
					&AddressGuid, NULL);

				if (pBlob) {
					// copy the info into TLS mem
					pHost = (HOSTENT *)pBlob->pBlobData;
					OffsetToPointer(pHost);
					CopyHostentToThread(pHost, pThread);
					pHost = &pThread->GETHOST_host;

				} else {
					if (WSASERVICE_NOT_FOUND == (Err = GetLastError()))
						Err = WSAHOST_NOT_FOUND;
				}

				LocalFree(pResults);
			} else
				Err = WSA_NOT_ENOUGH_MEMORY;
		} else
			Err = WSA_NOT_ENOUGH_MEMORY;
			
	} else	// if (addr)
		Err = WSAEINVAL;

	if (Err) {
		ASSERT(!pHost);
		SetLastError(Err);
	}
	
	return pHost;

}	// gethostbyaddr()


struct hostent *WSAAPI gethostbyname (
	IN const char	*pName) {

	int				Err;
	struct hostent	*pHost;
	BLOB			*pBlob;
	char			*pResults, *pszName;
	SOCK_THREAD		*pThread;

	Err = 0;
	pHost = NULL;
	// check if we've been started up.
	pszName = (char *)pName;
	if (GetThreadData(&pThread)) {
		if (pResults = LocalAlloc(LPTR, DEFAULT_QUERY_SIZE)) {
			pBlob = getxyDataEnt(&pResults, DEFAULT_QUERY_SIZE, pszName, 
				&InetHostName, NULL);

			if (pBlob) {
				// copy the info into TLS mem
				pHost = (HOSTENT *)pBlob->pBlobData;
				OffsetToPointer(pHost);
				CopyHostentToThread(pHost, pThread);
				pHost = &pThread->GETHOST_host;

			} else {
				if (WSASERVICE_NOT_FOUND == (Err = GetLastError()))
					Err = WSAHOST_NOT_FOUND;
			}

			LocalFree(pResults);
		} else
			Err = WSA_NOT_ENOUGH_MEMORY;
	} else
		Err = WSA_NOT_ENOUGH_MEMORY;

	if (Err) {
		ASSERT(!pHost);
		SetLastError(Err);
	}
	
	return pHost;

}	// gethostbyname()


#define MAX_NB_NAME	16
// this call is not defined in the winsock spec., we have it so that we can
// validate the name that is being assigned to the host, perhaps in the future
// we may also make some sort of notification mechanism
// note: to be consistent with gethostname cName is actually the # of bytes
// of storage used in pName and is therefore strlen(pName) + 1 and pName must
// include the terminating null
//
// This originally matched RFC 10035, now changed to allow _ (underscore) in name
//
int SOCKAPI sethostname(IN char FAR *pName, IN int cName) {
	int		Status = 0;

	DEBUGMSG(0, (TEXT("+sethostname\r\n")));

	if (!pName) {
		Status = WSAEFAULT;
	} else if (cName < 2 || cName > MAX_NB_NAME) {
		Status = WSAEINVAL;
	} else if (IsAPIReady(SH_COMM)) {
		Status = AFDControl (-1, WSCNTL_AFD_SET_MACHINE_NAME, pName, &cName, NULL, NULL);
	} else {
		Status = WSAENETDOWN;
	}

	if (Status) {
		DEBUGMSG(ZONE_WARN, 
			(TEXT("*sethostname: had error %d, make sure cName==strlen(pName)+1\r\n")
			TEXT("and that pName ends w/ NULL and 1st must be alpha, last must be\r\n")
			TEXT("alphanumeric and in between alphanum, _ or - \r\n"),
			Status));
		SetLastError(Status);
		Status = SOCKET_ERROR;
	}
	return Status;

}	// sethostname


int WSAAPI gethostname(
	OUT char	*pName,
	IN int		cName) {

	int		Err = WSAEFAULT;
	WCHAR	aDftName[] = TEXT("WindowsCE");
	WCHAR	*p, CompName[128];
	DWORD	Type;
	int		cSize;
	HKEY	hKey;
	LONG	hRet;

	if (cName > 0 && pName) {
		p = NULL;
		if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_LOCAL_MACHINE, 
									TEXT("Ident"), 0, 0, &hKey)) {
			cSize = sizeof(CompName);
			hRet = RegQueryValueEx(hKey, TEXT("Name"), 0, &Type,
				(char *)CompName, &cSize);
			RegCloseKey (hKey);
			
			if ((hRet == ERROR_SUCCESS) && (Type == REG_SZ)) {
				p = CompName;
				cSize >>= 1;
			}
		}
		if (!p) {
			p = aDftName;
			cSize = 10;
		}
		
		if (cName >= cSize) {
			__try {
				wcstombs (pName, p, cSize);
				pName[cName-1] = '\0';
				Err = 0;
			}
			__except(EXCEPTION_EXECUTE_HANDLER) {
				;	// don't have anything to do
			}
		}
	}

	if (Err) {
		SetLastError(Err);
		Err = SOCKET_ERROR;
	}

	return Err;

}	// gethostname()


struct protoent * WSAAPI getprotobynumber(
	IN int number) {

	SetLastError(WSANO_RECOVERY);
	return NULL;

}	// getprotobynumber()


struct protoent * WSAAPI getprotobyname(
	IN const char FAR * name) {

	SetLastError(WSANO_RECOVERY);
	return NULL;

}	// getprotobyname()


// NOTE: right now we only handle IPv4 and IPV6 addrinfo stuff
struct addrinfo *GetAI(int ProtFamily, int SockType, int Proto,
	struct addrinfo ***pppPrev) {

	struct addrinfo	*pAddr;

	if (pAddr = LocalAlloc(LPTR, sizeof(*pAddr))) {

		if (PF_INET == ProtFamily) {
			pAddr->ai_addrlen = sizeof(SOCKADDR_IN);
			pAddr->ai_addr = LocalAlloc(LPTR, sizeof(SOCKADDR_IN));
		} else if (PF_INET6 == ProtFamily) {
			pAddr->ai_addrlen = sizeof(SOCKADDR_IN6);
			pAddr->ai_addr = LocalAlloc(LPTR, sizeof(SOCKADDR_IN6));
		} else {
			ASSERT(0);
		}

		if (pAddr->ai_addr) {
			pAddr->ai_family = ProtFamily;
			pAddr->ai_socktype = SockType;
			pAddr->ai_protocol = Proto;

			**pppPrev = pAddr;
			*pppPrev = &pAddr->ai_next;
			
		} else {
			LocalFree(pAddr);
			pAddr = NULL;
		}
	}

	return pAddr;

}	// GetAI


int ConvertToSockAddrList(WSAQUERYSETW *pResults, int ProtFamily, 
		SOCKET_ADDRESS_LIST **ppSAL) {

	int					Status;
	SOCKET_ADDRESS_LIST	*pSAL;
	DWORD				i, cAddrs;
	CSADDR_INFO			*pCsAddr;
	int iAddrsWithMyProto=0;

	if (0 == (cAddrs = pResults->dwNumberOfCsAddrs))
		cAddrs = 1;

	pSAL = LocalAlloc(LPTR, sizeof(*pSAL) + 
			((cAddrs - 1) * sizeof(SOCKET_ADDRESS)));

	cAddrs = pResults->dwNumberOfCsAddrs;

	if (*ppSAL = pSAL) {
		pCsAddr = pResults->lpcsaBuffer;

		for (i = 0; i < cAddrs; i++, pCsAddr++) {
			if (ProtFamily != pCsAddr->RemoteAddr.lpSockaddr->sa_family) {
				continue;
			}
			pSAL->Address[iAddrsWithMyProto] = pCsAddr->RemoteAddr;
			iAddrsWithMyProto++;
		}
		pSAL->iAddressCount = iAddrsWithMyProto;
		Status = 0;
	} else {
		Status = WSAENOBUFS;
	}
	if (0 == iAddrsWithMyProto) {
		Status = WSANO_DATA;
	}

	return Status;

}	// ConvertToSockAddrList()


int ConvertCSAddress(SOCKET_ADDRESS_LIST *pSAL, int ProtFamily, int SockType, 
	int Proto, u_short ServicePort, struct addrinfo ***pppPrev) {

	SOCKET_ADDRESS	*pSA;
	int				cAddrs, cLen, Err, i;
	struct addrinfo	*pAddr;
	SOCKADDR		*pSockaddr;
	SOCKADDR_IN		*pSaddrIn;

	Err = cAddrs = 0;
	if (PF_INET6 == ProtFamily)
		cLen = sizeof(SOCKADDR_IN6);
	else
		cLen = sizeof(SOCKADDR_IN);
	
	pSA = pSAL->Address;
	
	for (i = 0; i < pSAL->iAddressCount; i++, pSA++) {

		pSockaddr = pSA->lpSockaddr;
		ASSERT(pSockaddr);

		

		
		if ((pSockaddr) && (
			(pSA->iSockaddrLength >= cLen &&
			AF_INET6 == pSockaddr->sa_family)
				||
			(pSA->iSockaddrLength >= cLen &&
			AF_INET == pSockaddr->sa_family))) {

			if (pAddr = GetAI(ProtFamily, SockType, Proto, pppPrev)) {

				memcpy(pAddr->ai_addr, pSockaddr, cLen);

				// the port is in the same place for both v4 and v6
				pSaddrIn = (SOCKADDR_IN *)pAddr->ai_addr;
				pSaddrIn->sin_port = ServicePort;

				pAddr->ai_flags |= AI_NUMERICHOST;
				cAddrs++;

			} else {
				Err = EAI_MEMORY;
				break;
			}
		} else {

			if (pSockaddr) {
				DEBUGMSG(ZONE_WARN, 
					(TEXT("ConvertCSAddress: SAddrLen %d, family %d\r\n"),
					pSA->iSockaddrLength,
					pSockaddr->sa_family));
			}
			ASSERT(0);
		}

	}

	if (! cAddrs) {
		ASSERT(0);
		Err = EAI_NODATA;
	}

	return Err;

}


int QueryName(
	IN OUT	CHAR		**ppResults,
	IN		DWORD		cLen,
	IN		WCHAR		*pwszName,
	IN		LPGUID		pType,
	IN		DWORD		Flags,
	OUT		LPWSTR		*ppName,	// OPTIONAL parameter
	OUT		LPWSTR		*ppSvcInstName) {	// OPTIONAL parameter

	int				Err, fNewBuf, cSvcInstName;
	WSAQUERYSETW	*pQuery;
	HANDLE			hQuery;
	DWORD			cQuery;
	BLOB			*pBlob;

	// make sure it is dword aligned
	ASSERT(0 == ((DWORD)(*ppResults) & 0x3));
	if (ppName)
		*ppName = NULL;

	pBlob = NULL;
	pQuery = (WSAQUERYSETW *)*ppResults;
	cQuery = cLen;

	memset(pQuery, 0, sizeof(*pQuery));
	pQuery->dwSize = sizeof(*pQuery);
	pQuery->lpszServiceInstanceName = pwszName;
	pQuery->lpServiceClassId = pType;
	pQuery->dwNameSpace = NS_DNS;
//			pQuery->dwNumberOfProtocols = 2;
//			pQuery->lpafpProtocols = AfProtocols;

	Err = WSALookupServiceBegin(pQuery, Flags, &hQuery);

	if (! Err) {
		do {
			fNewBuf = FALSE;
			Err = WSALookupServiceNext(hQuery, 0, &cLen, pQuery);

			if (Err) {
				Err = GetLastError();

				// check if buffer was too small
				if (WSAEFAULT == Err && cLen > cQuery) {
					LocalFree(*ppResults);
					*ppResults = (char *)pQuery = 
						LocalAlloc(LPTR, cLen);
					if (pQuery) {
						cQuery = cLen;
						fNewBuf = TRUE;
					} else {
						Err = WSA_NOT_ENOUGH_MEMORY;
					}
				}
			} else {

				// we got a response...
				if (LUP_RETURN_ADDR & Flags) {
					if (pQuery->dwNumberOfCsAddrs) {
						DEBUGMSG(ZONE_WARN, 
							(TEXT("QueryName: good success %d addresses\r\n"),
							pQuery->dwNumberOfCsAddrs));

					} else {
						// how come addresses weren't returned?!?
						ASSERT(0);
						Err = WSANO_DATA;
					}
				}

				if (LUP_RETURN_NAME & Flags) {
					ASSERT(pQuery->lpszServiceInstanceName);
					if (ppSvcInstName && (! *ppSvcInstName) &&
						pQuery->lpszServiceInstanceName) {

						cSvcInstName = sizeof(WCHAR) * 
							(wcslen(pQuery->lpszServiceInstanceName) + 1);

						if (*ppSvcInstName = LocalAlloc(LPTR, cSvcInstName)) {
							StringCbCopy(*ppSvcInstName, cSvcInstName,
								pQuery->lpszServiceInstanceName);
						}
						// if the allocation fails, that is OK, returning
						// canonical name is not crucial, ignore error
					}
				}
				
				if (LUP_RETURN_BLOB & Flags) {
					if (pBlob = pQuery->lpBlob) {
						if (ppName)
							*ppName = pQuery->lpszServiceInstanceName;
					} else {
						if (&HostnameGuid == pType) {
							if (ppName)
								*ppName = pQuery->lpszServiceInstanceName;
						} else
							Err = WSANO_DATA;
					}
				}
			}	// else Err = WSALookupServiceNext
			
		} while (fNewBuf);
		WSALookupServiceEnd(hQuery);
	} else {
        Err = GetLastError(); // from WSALookupServiceBegin
    }

	return Err;

}	// QueryName()


// assumes base is ten
ulong MyStrToUl(char *pStr, char **ppEnd) {
	ulong	cResult = 0;
	
	// skip white space
	while(*pStr == ' ' || *pStr == '\t' || *pStr == '\n') {
		pStr++;
	}

	while (('0' <= *pStr) && ('9' >= *pStr)) {
		cResult = 10 * cResult + (*pStr - '0');
		pStr++;
	}
	
	*ppEnd = pStr;

	return cResult;

} // MyStrToL()


// assumes base is ten
int MyShortToStr(ushort cNum, char *pStr, int cStr) {
	int		Err , i;
	char	aTemp[6];

	i = Err = 0;
	do {
		ASSERT(i < 6);
		aTemp[i++] = '0' + (cNum % 10);
		cNum /= 10;
	} while (cNum);

	while (i && cStr--) {
		*pStr++ = aTemp[--i];
	}
	
	if (cStr) {
		*pStr = '\0';
	} else {
		Err = EAI_MEMORY;
	}

	return Err;

} // MyShortToStr()


// there should be a better way than this, but this is what NT does
int IsIp6Running() {
	SOCKET  s;

	s = socket(AF_INET6, SOCK_DGRAM, 0);
	if (INVALID_SOCKET != s)
		closesocket(s);

	return(INVALID_SOCKET != s);
}	// IsIp6Running()


int SortV6Addrs(SOCKET_ADDRESS_LIST *pSAL) {
	SOCKET	Sock;
	int		Status;
	DWORD	cBuf;
	int		cAddrs;

	// very, very silly--this is what NT does, if we have time
	// should change it to not have to do this!
	Sock = socket(AF_INET6, SOCK_DGRAM, 0);
	if (INVALID_SOCKET != Sock) {

		cAddrs = pSAL->iAddressCount;
		cBuf = FIELD_OFFSET(SOCKET_ADDRESS_LIST, Address[cAddrs]);
		
		Status = WSAIoctl(Sock, SIO_ADDRESS_LIST_SORT, (void *)pSAL, cBuf,
			(void *)pSAL, cBuf, &cBuf, NULL, NULL);

		closesocket(Sock);
	}

	return 0;

}	// SortV6Addrs()

//
// Thread and structure to perform parallel search over IPv4 and IPv6 for getaddrinfo(PF_UNSPEC) 
//
typedef struct _ASYNC_QUERY {
    BOOL bRunning;
    int Err;
    int	Flags;
    WCHAR *pwszNodename;
    WCHAR *pwszSvcInstName;
	PWSAQUERYSETW pResults;
    HANDLE hThd;
} ASYNC_QUERY, * PASYNC_QUERY;

DWORD
IPv6QueryThread(
    LPVOID Context
    )
{
	PASYNC_QUERY	pQuery = (PASYNC_QUERY)Context;
    WCHAR			**ppSvcInstName;

    if (pQuery->pResults =(PWSAQUERYSETW)LocalAlloc(LPTR, DEFAULT_QUERY_SIZE)){
    	
		if (AI_CANONNAME & pQuery->Flags)
			ppSvcInstName = &pQuery->pwszSvcInstName;
		else
			ppSvcInstName = NULL;

		pQuery->Err = QueryName((char **)&(pQuery->pResults), 
			DEFAULT_QUERY_SIZE, pQuery->pwszNodename, &Ipv6Guid,
			LUP_RETURN_ADDR | LUP_RETURN_NAME, NULL, ppSvcInstName);
		
#ifdef DEBUG
		if (ppSvcInstName)
			ASSERT(*ppSvcInstName == pQuery->pwszSvcInstName);
#endif

	} else {
		pQuery->Err = EAI_MEMORY;
	}
    return 0;
}

int
PerformAsyncIPv6Query(
    PASYNC_QUERY pQuery
    )
{
    pQuery->hThd = CreateThread(NULL, 0, IPv6QueryThread, pQuery, 0, NULL);
    if (pQuery->hThd) {
        pQuery->bRunning = TRUE;
        MatchCurrentThreadPriority(pQuery->hThd);
        return 0;
    } else {
        return EAI_MEMORY;
    }
}


int CanonizeNumAddress(struct addrinfo * pAddrInfo) {
	int		cAddr = INET6_ADDRSTRLEN;
	int		Err = 0;
	WCHAR 	wszAddr[INET6_ADDRSTRLEN];

	ASSERT(pAddrInfo);
	if (SOCKET_ERROR == WSAAddressToString(pAddrInfo->ai_addr, 
		pAddrInfo->ai_addrlen, NULL, wszAddr, &cAddr)) {
		Err = WSAGetLastError();
	} else {
		ASSERT(NULL == pAddrInfo->ai_canonname);

		if (pAddrInfo->ai_canonname = LocalAlloc(LPTR, cAddr + 1)) {

			if (! WideCharToMultiByte(CP_UTF8, 0, wszAddr, -1, 
				pAddrInfo->ai_canonname, cAddr, NULL, NULL)) {
				if (! WideCharToMultiByte(CP_ACP, 0, wszAddr, -1, 
					pAddrInfo->ai_canonname, cAddr, NULL, NULL)) {
					Err = EAI_MEMORY;
				}
			}

		} else {
			Err = EAI_MEMORY;
		}
    }
	return Err;

}	// CanonizeNumAddress()


int CanonizeName(struct addrinfo *pAddrInfo, WCHAR *pSvcInstName) {
	int cSvcInstName;
	int	Err = 0;

	ASSERT(pSvcInstName);
	ASSERT(pAddrInfo);
	if (pAddrInfo && (! pAddrInfo->ai_canonname)) {

		cSvcInstName = (wcslen(pSvcInstName) + 1) * 2;

		if (pAddrInfo->ai_canonname = LocalAlloc(LPTR, cSvcInstName)) {

			if (! WideCharToMultiByte(CP_UTF8, 0, pSvcInstName, -1, 
				pAddrInfo->ai_canonname, cSvcInstName, NULL, NULL)) {
				
				if (! WideCharToMultiByte(CP_ACP, 0, pSvcInstName, -1, 
					pAddrInfo->ai_canonname, cSvcInstName, NULL, NULL)) {
					
					Err = EAI_MEMORY;
				}
			}
		} else {
			Err = EAI_MEMORY;
		}
	}

	return Err;

}	// CanonizeName()


int WSAAPI getaddrinfo(
	IN const char * pNodename,
	IN const char * pServname,
	IN const struct addrinfo * pHints,
	OUT struct addrinfo * * ppRes) {

	int						Err;
	int						fIPv6;
	struct addrinfo			*pAddr, **ppAddr, **ppOrigAI;
	int						Proto = 0;
	u_short					ProtFamily = PF_UNSPEC;
	u_short					ServicePort = 0;
	int						SockType = 0;
	int						Flags = 0;
	struct sockaddr_in		*pSin;
	struct sockaddr_in6		*pSin6;
	int						cLen;
	struct sockaddr_in6		Sin6;
	PWSAQUERYSETW			pResults = NULL;
	WCHAR					*pwszNodename;
	GUID					*pType;
	SOCKET_ADDRESS_LIST		*pSAL;
	ASYNC_QUERY				*pAsyncQuery = NULL;
	WCHAR					**ppSvcInstName, *pSvcInstName = NULL;

	pwszNodename = NULL;
	ppSvcInstName = NULL;

	if (NULL == ppRes) {
		Err = WSAEFAULT;
		goto Exit;
	}

	Err = 0;
	*ppRes = NULL;
	ppOrigAI = ppAddr = ppRes;

	if (! pNodename && ! pServname) {
		Err = EAI_NONAME;
		goto Exit;
	}

	if (pHints) {
		if (pHints->ai_addrlen || pHints->ai_canonname || pHints->ai_addr
			|| pHints ->ai_next) {
			Err = EAI_FAIL;
		} else {

			if ((AI_CANONNAME & (Flags = pHints->ai_flags)) && !pNodename) {
				Err = EAI_BADFLAGS;
				
			} else if ((PF_UNSPEC != (ProtFamily = pHints->ai_family)) &&
				(PF_INET != ProtFamily) &&
				(PF_INET6 != ProtFamily)) {
				Err = EAI_FAMILY;
				
			} else if ((SockType = pHints->ai_socktype) &&
				(SOCK_STREAM != SockType) &&
				(SOCK_DGRAM != SockType)) {
				Err = EAI_SOCKTYPE;
			} else
				Proto = pHints->ai_protocol;
		}
	}

	if (pServname) {
		char *pEnd;
			
		// CE doesn't currently support the getservbyname functionality so
		// we'll only check to see if they gave us a port number to convert
		
		ServicePort = (u_short)MyStrToUl((char *)pServname, &pEnd);
		ServicePort = htons(ServicePort);
	}

	if (Err)
		goto Exit;

	fIPv6 = IsIp6Running();

	if (! pNodename) {

		if ((PF_UNSPEC == ProtFamily || PF_INET6 == ProtFamily) && fIPv6) {			
			if (pAddr = GetAI(PF_INET6, SockType, Proto, &ppAddr)) {
				pSin6 = (SOCKADDR_IN6 *)pAddr->ai_addr;
				
				if (AI_PASSIVE & Flags) {
					IN6ADDR_SETANY(pSin6);
				} else {
					IN6ADDR_SETLOOPBACK(pSin6);
				}
				pSin6->sin6_port = ServicePort;

			} else {
				Err = EAI_MEMORY;
				goto Exit;
			}	// else if (pAddr = GetAI...)
		}	// if (PF_UNSPEC || PF_INET6)
		
		if (PF_UNSPEC == ProtFamily || PF_INET == ProtFamily) {
			if (pAddr = GetAI(PF_INET, SockType, Proto, &ppAddr)) {
				pSin = (SOCKADDR_IN *)pAddr->ai_addr;
				pSin->sin_family = AF_INET;
				
				if (AI_PASSIVE & Flags)
					pSin->sin_addr.s_addr = htonl(INADDR_ANY);
				else
					pSin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);

				pSin->sin_port = ServicePort;

			} else {
				Err = EAI_MEMORY;
				goto Exit;
			}	// else if (pAddr = GetAI...)
		}	// if (PF_UNSPEC || PF_INET)				

		goto Exit;

	} else {

		cLen = strlen(pNodename) + 1;
		if (256 < cLen) {
			Err = EAI_FAIL;
			goto Exit;
		}
		if (pwszNodename = LocalAlloc(LPTR, sizeof(WCHAR) * cLen)) {

			if (! MultiByteToWideChar(CP_ACP, 0, pNodename, -1, pwszNodename, cLen)) {
				Err = EAI_FAIL;
			}
		} else
			Err = EAI_MEMORY;

		if (Err)
			goto Exit;

		if ((PF_UNSPEC == ProtFamily || PF_INET6 == ProtFamily) && fIPv6) {
			Sin6.sin6_family = AF_INET6;
			cLen = sizeof(Sin6);

			if (SOCKET_ERROR != WSAStringToAddress(pwszNodename, 
				AF_INET6, NULL, (SOCKADDR *)&Sin6, &cLen)) {
					
				if (pAddr = GetAI(Sin6.sin6_family, SockType, Proto,&ppAddr)){

					// WSAStringToAddress can parse the port string but we
					// need to put braces around the name and concat the port
					// string, and we've already parsed the port above
					
					Sin6.sin6_port = ServicePort;
					memcpy(pAddr->ai_addr, &Sin6, cLen);

					pAddr->ai_flags |= AI_NUMERICHOST;

					if (AI_CANONNAME & Flags) {
						ASSERT(pAddr == *ppRes);
						Err = CanonizeNumAddress(pAddr);
					}

				} else {
					Err = EAI_MEMORY;
				}	// else if (pAddr = GetAI...)	

				goto Exit;
			}
				
		}


		if (PF_UNSPEC == ProtFamily || PF_INET == ProtFamily) {
			pSin = (SOCKADDR_IN *)&Sin6;
			pSin->sin_family = AF_INET;
			cLen = sizeof(*pSin);

			if (SOCKET_ERROR != WSAStringToAddress(pwszNodename, 
				AF_INET, NULL, (SOCKADDR *)pSin, &cLen)) {
					
				if (pAddr = GetAI(PF_INET, SockType, Proto, &ppAddr)) {
					
					pSin->sin_port = ServicePort;
					memcpy(pAddr->ai_addr, pSin, min(cLen,sizeof(*pSin)));

					// do we need to clear the sin_zero???

					pAddr->ai_flags |= AI_NUMERICHOST;

					if (AI_CANONNAME & Flags) {
						ASSERT(pAddr == *ppRes);
						Err = CanonizeNumAddress(pAddr);
					}


				} else {
					Err = EAI_MEMORY;
				}	// else if (pAddr = GetAI...)	

				 goto Exit;
			}
				
		}

		if (AI_NUMERICHOST & Flags) {
			// if we're still here and this is set then we failed the
			// WSAStringToAddress calls
			Err = EAI_NONAME;
			goto Exit;
		}

		// ok we now need to query for the address:

		if (pResults = (PWSAQUERYSETW)LocalAlloc(LPTR, DEFAULT_QUERY_SIZE)) {

			if (AI_CANONNAME & Flags)
				ppSvcInstName = &pSvcInstName;

			// for PF_UNSPEC we'll do queries for both v6 and v4
			if (PF_UNSPEC == ProtFamily) {

				if (fIPv6) {
					// Use heap allocated memory for inter thread communication to avoid 
					// access problems when we are called from within a PSL.
					pAsyncQuery = (PASYNC_QUERY)LocalAlloc(0,sizeof(ASYNC_QUERY));
					if (pAsyncQuery == NULL) {
						Err = EAI_MEMORY;
						goto Exit;
					}
					pAsyncQuery->bRunning = FALSE;
					pAsyncQuery->Err = 0;
					pAsyncQuery->Flags = Flags;
					pAsyncQuery->pwszNodename = pwszNodename;
					pAsyncQuery->pwszSvcInstName = NULL;
					pAsyncQuery->hThd = NULL;
					if (EAI_MEMORY == PerformAsyncIPv6Query(pAsyncQuery)) {
						if (pAsyncQuery->hThd) {
							CloseHandle(pAsyncQuery->hThd);
							pAsyncQuery->hThd = NULL;
						}
						pType = &Ipv6Guid;

						Err = QueryName((char **)&pResults, 
							DEFAULT_QUERY_SIZE, pwszNodename, pType, 
							LUP_RETURN_ADDR | LUP_RETURN_NAME, NULL, 
							ppSvcInstName);

                        if (! Err) {
                            Err = ConvertToSockAddrList(pResults,  PF_INET6, &pSAL);

                            if (! Err) {
                                ASSERT(pSAL);
                                SortV6Addrs(pSAL);
                                //ppOrigAI = ppRes;
                                Err = ConvertCSAddress(pSAL, PF_INET6, 
                                    SockType, Proto, ServicePort, &ppRes);

                                LocalFree(pSAL);

	                            if ((AI_CANONNAME & Flags) && *ppSvcInstName) {
	                            	ASSERT(*ppSvcInstName = pSvcInstName);
									CanonizeName(*ppOrigAI, pSvcInstName);
									// only do this once to match XP even tho
									// v4 and v6 queries may return differently
									Flags &= ~AI_CANONNAME;
	                        	}
                                
                            }

                        } else {
                            

                        }
                        if (ppSvcInstName && *ppSvcInstName) {
                        	LocalFree(*ppSvcInstName);
                        	*ppSvcInstName = NULL;
                    	}
                    } 
                }

				ProtFamily = PF_INET;
				pType = &InetHostName;
			} else if ((PF_INET6 == ProtFamily) && fIPv6) {
				pType = &Ipv6Guid;
				ProtFamily = PF_INET6;
			} else if (PF_INET == ProtFamily) {
				// we only support these two Protocol families
				ProtFamily = PF_INET;
				pType = &InetHostName;
			} else {
				Err = EAI_FAMILY;
				goto Exit;
			}
			
			memset(pResults, 0, DEFAULT_QUERY_SIZE);
			Err = QueryName((char **)&pResults, DEFAULT_QUERY_SIZE, 
				pwszNodename, pType, LUP_RETURN_ADDR | LUP_RETURN_NAME, NULL,
				ppSvcInstName);

            if (pAsyncQuery && pAsyncQuery->bRunning) {

                // Wait for IPv6 search thread before processing IPv4 results
                if (pAsyncQuery->hThd) {
                    WaitForSingleObject(pAsyncQuery->hThd, INFINITE);
                    CloseHandle(pAsyncQuery->hThd);
                    if (! pAsyncQuery->Err) {
                        pAsyncQuery->Err = ConvertToSockAddrList(
                        	pAsyncQuery->pResults,  PF_INET6, &pSAL);

                        if (! pAsyncQuery->Err) {
                            ASSERT(pSAL);
                            SortV6Addrs(pSAL);
                            // ppOrigAI = ppRes;
                            pAsyncQuery->Err = ConvertCSAddress(pSAL, PF_INET6,
                                SockType, Proto, ServicePort, &ppRes);

                            LocalFree(pSAL);

                            if ((AI_CANONNAME & Flags) && 
                            	pAsyncQuery->pwszSvcInstName) {
                            	
								CanonizeName(*ppOrigAI, pAsyncQuery->pwszSvcInstName);
								// only do this once to match XP even tho
								// v4 and v6 queries may return differently
								Flags &= ~AI_CANONNAME;
                        	}
                        }

                    } else {
                        

                    }
                    if (pAsyncQuery->pwszSvcInstName) {
                    	LocalFree(pAsyncQuery->pwszSvcInstName);
                    	pAsyncQuery->pwszSvcInstName = NULL;
                	}
                    
                }
                if (pAsyncQuery->pResults) {
                    LocalFree(pAsyncQuery->pResults);
                }
            }

			if (! Err) {

				Err = ConvertToSockAddrList(pResults,  ProtFamily, &pSAL);

				if (! Err) {
					ASSERT(pSAL);
					if (fIPv6 && (PF_INET6 == ProtFamily))
						SortV6Addrs(pSAL);
					//ppOrigAI = ppRes;
					Err = ConvertCSAddress(pSAL, ProtFamily, 
						SockType, Proto, ServicePort, &ppRes);

					LocalFree(pSAL);

                    if ((AI_CANONNAME & Flags) && *ppSvcInstName) {
                    	ASSERT(*ppSvcInstName = pSvcInstName);
						CanonizeName(*ppOrigAI, pSvcInstName);
                	}
				}

			} else {
				
				
			}

            if (ppSvcInstName && *ppSvcInstName) {
            	LocalFree(*ppSvcInstName);
            	*ppSvcInstName = NULL;
        	}
			
		} else {	// if (pResults = LocalAlloc...)
			Err = EAI_MEMORY;
		}

	}// else if (! pNodename)

Exit:

	if (pwszNodename) {
		LocalFree(pwszNodename);
	}

    // Return success if either query succeeded
    
    if (((!pAsyncQuery || (FALSE == pAsyncQuery->bRunning)) && Err) ||
        (pAsyncQuery && (TRUE == pAsyncQuery->bRunning) && Err && pAsyncQuery->Err)) {
        if (ppRes) {
            if (*ppRes) {
                freeaddrinfo(*ppRes);
                *ppRes = NULL;
            }
        }

        // should we set last error?
        SetLastError(Err);
    } else {
        Err = 0;    // One of the protocols succeeded.
    }
#if DEBUG
	if (ppSvcInstName)
	    ASSERT(! (*ppSvcInstName));
#endif
    if (pAsyncQuery) {
        LocalFree(pAsyncQuery);
    }
    if (pResults) {
    	LocalFree(pResults);
    }
    return Err;
    
}	// getaddrinfo()


int WSAAPI getnameinfo(
	IN const struct sockaddr	*pSAddr,
	IN socklen_t				cSAddr,
	OUT char					*pHost,
	IN DWORD					cHost,
	OUT char					*pServ,
	IN DWORD					cServ,
	IN int						Flags) {

	int				Err;
	ushort			Family, ServicePort;
	WSAQUERYSETW	*pResults;
	WCHAR			awszAddress[INET6_ADDRSTRLEN+2], *pwszAddr, *p;
	DWORD			cAddr;
	
	pResults = NULL;
	
	if (!pSAddr || (cSAddr < sizeof(*pSAddr))) {
		// unfortunately the getnameinfo spec doesn't define this error code
		// however XP is returning WSAEFAULT, so we will do the same
		Err = WSAEFAULT;
	} else if ((AF_INET != (Family = pSAddr->sa_family)) && 
		AF_INET6 != pSAddr->sa_family) {
		
		Err = EAI_FAMILY;

	// no need to check AF_INET since it is same size as SOCKADDR
	} else if (AF_INET6 == Family && cSAddr < sizeof(SOCKADDR_IN6)) {
		// again no good error code defined in getnameinfo spec for this
		Err = WSAEFAULT;
	} else {

		Err = 0;

		ServicePort = ((SOCKADDR_IN *)pSAddr)->sin_port;

		if (pServ) {
			if (NI_NUMERICSERV & Flags) {
				Err = MyShortToStr(htons(ServicePort), pServ, cServ);
			} else {
				// CE doesn't support the getservbyname functionality
				Err = EAI_NODATA;
			}
		}
		
		if ((! Err) && pHost) {
			pwszAddr = awszAddress;
			cAddr = sizeof(awszAddress);

			// sin_port and sin6_port are in the same place
			if (AF_INET == pSAddr->sa_family || AF_INET6 == pSAddr->sa_family)
				((SOCKADDR_IN *)pSAddr)->sin_port = 0;
			Err = WSAAddressToString((SOCKADDR *)pSAddr, cSAddr, NULL, 
				pwszAddr, &cAddr);
			((SOCKADDR_IN *)pSAddr)->sin_port = ServicePort;

			if (Err) {
				
				

				Err = EAI_FAIL;

			} else if (! (NI_NUMERICHOST & Flags)) {
				// we don't yet do reverse ipv6 queries--our servers also
				// just redirect it anyway...
				if (AF_INET6 == pSAddr->sa_family) {
					if (IN6ADDR_ISLOOPBACK((struct sockaddr_in6 *)pSAddr)) {
						if (cHost < 10) {
							Err = EAI_MEMORY;
						} else {
							memcpy(pHost, "localhost", cHost);
						}
					} else {
						Err = EAI_NODATA;
					}
					goto Exit;
				}
			
				if (pResults = (WSAQUERYSETW *)
					LocalAlloc(LPTR, DEFAULT_QUERY_SIZE)) {
				
					if (Err = QueryName((char **)&pResults, DEFAULT_QUERY_SIZE,
						pwszAddr, &AddressGuid, LUP_RETURN_NAME, NULL, NULL)) {

						if (NI_NAMEREQD & Flags) {
							DEBUGMSG(ZONE_WARN, 
								(TEXT("getnameinfo: NameQuery failed Err = %d\r\n"),
								Err));
							
							
							Err = EAI_FAIL;
						}
						// otherwise we fall on thru to numeric case.
						
					} else {
						pwszAddr = pResults->lpszServiceInstanceName;
						// fall thru...
					}
					
				} else {	// if (pResults = LocalAlloc...)
					Err = EAI_MEMORY;
				}
			}
			
			// we're almost done
			if (! Err) {
				if ((NI_NOFQDN & Flags) && !(NI_NUMERICHOST & Flags)) {
					p = pwszAddr;
					while (TEXT('\0') != *p) {
						if (TEXT('.') == *p)
							*p = TEXT('\0');
						else
							p++;
					}
				}
				
				if (! WideCharToMultiByte(CP_UTF8, 0, pwszAddr, -1, pHost, cHost, NULL, NULL)) {
					if (! WideCharToMultiByte(CP_ACP, 0, pwszAddr, -1, pHost, cHost, NULL, NULL)) {

						DEBUGMSG(ZONE_WARN, 
							(TEXT("getnameinfo: WideCharToMB failed = %d\r\n"),
							GetLastError()));
						Err = EAI_MEMORY;
					}
				}
			}
			
			if (pResults)
				LocalFree(pResults);	
			
		}	// if (pHost)
	}

Exit:
	if (Err) {
		SetLastError(Err);
	}

	return Err;

}	// getnameinfo()


void WSAAPI freeaddrinfo(
	IN struct addrinfo	*pAi) {

	struct addrinfo *pDel;

	while (pDel = pAi) {
		pAi = pAi->ai_next;

		if (pDel->ai_addr)
			LocalFree(pDel->ai_addr);
		if (pDel->ai_canonname)
			LocalFree(pDel->ai_canonname);
		LocalFree(pDel);
	}
		

}	// freeaddrinfo()


