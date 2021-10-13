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

addr.c

address related winsock functions


FILE HISTORY:
	OmarM     18-Sep-2000

*/

#include "winsock2p.h"
#include "cxport.h"


extern int DerefProvider(GUID *pId, WsProvider *pDelProv);


int CheckSockaddr(const struct sockaddr *pAddr, int cAddr) {
    
    int     Err;
    u_short Family;
    
    Err = WSAEFAULT;
    if ((pAddr) && (cAddr > 1)) {
        __try {
            Family = pAddr->sa_family;
            Err = 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            Err = WSAEFAULT;
        }
    }
    
    return Err;
    
}	// CheckSockaddr()


int WSAAPI bind (
	SOCKET s,
	const struct sockaddr * name,
	int namelen) {

	int			Err, Status;
	WsSocket	*pSock;

	if (! (Err = RefSocketHandle(s, &pSock))) {

		if (! (Err = CheckSockaddr(name, namelen))) {
			Status = pSock->pProvider->ProcTable.lpWSPBind(pSock->hWSPSock, 
				name, namelen, &Err);

			if (Status && ! Err) {
				ASSERT(0);
				Err = WSASYSCALLFAILURE;
			}
		}
		DerefSocket(pSock);
	}

	if (Err) {
		SetLastError(Err);
		Status = SOCKET_ERROR;
	} else
		Status = 0;
	
	return Status;

}	// bind()


int WSAAPI getpeername (
	IN SOCKET			s,
	OUT struct sockaddr	*pName,
	IN OUT int			*pcName) {

	int			Err, Status;
	WsSocket	*pSock;

	if(!pcName)
	{
	    SetLastError(WSAEFAULT);
	    return SOCKET_ERROR;
	}
	
	if (! (Err = RefSocketHandle(s, &pSock))) {

		if (! (Err = CheckSockaddr(pName, *pcName))) {

			Status = pSock->pProvider->ProcTable.lpWSPGetPeerName(
				pSock->hWSPSock, pName, pcName, &Err);

			if (Status && ! Err) {
				ASSERT(0);
				Err = WSASYSCALLFAILURE;
			}
		}

		DerefSocket(pSock);
	}

	if (Err) {
		SetLastError(Err);
		Status = SOCKET_ERROR;
	} else
		Status = 0;
	
	return Status;

}	// getpeername()


int WSAAPI getsockname(
	IN SOCKET				s,
	OUT struct sockaddr		*pName,
	IN OUT int				*pcName) {
    
	int			Err, Status;
	WsSocket	*pSock;

    if(!pcName)
	{
	    SetLastError(WSAEFAULT);
	    return SOCKET_ERROR;
	}

	if (! (Err = RefSocketHandle(s, &pSock))) {

		if (! (Err = CheckSockaddr(pName, *pcName))) {

			Status = pSock->pProvider->ProcTable.lpWSPGetSockName(pSock->hWSPSock, 
				pName, pcName, &Err);

			if (Status && ! Err) {
				ASSERT(0);
				Err = WSASYSCALLFAILURE;
			}
		}
		DerefSocket(pSock);
	}

	if (Err) {
		SetLastError(Err);
		Status = SOCKET_ERROR;
	} else
		Status = 0;
	
	return Status;

}	// getsockname()


unsigned short SOCKAPI htons(unsigned short hosts) {
	return _byteswap_ushort(hosts);
}

unsigned long SOCKAPI htonl(unsigned long hostlong) {
	return _byteswap_ulong(hostlong);
}

unsigned short SOCKAPI ntohs(unsigned short hosts) {
	return _byteswap_ushort(hosts);
}

unsigned long SOCKAPI ntohl(unsigned long hostlong) {
	return _byteswap_ulong(hostlong);
}

unsigned long inet_addr (const char * cp) {
	unsigned long	Addr[4], RetAddr = INADDR_NONE;
	unsigned int	Base, i;
	unsigned char	c;

	if (! cp)
		goto Exit;

	memset(Addr, 0, sizeof(Addr));
	for (i = 0; i < 4 && *cp; i++) {
		// first figure out the base
		if ('0' == *cp) {
			cp++;
			if ('X' == *cp || 'x' == *cp) {
				cp++;
				Base = 16;
			} else
				Base = 8;
		} else
			Base = 10;

		while (c = *cp) {
			// must have some number now...
			if (c >= '0' && c <= '9')
				c -= '0';
			else if (c >= 'A' && c <= 'F')
				c +=  10 - 'A';
			else if (c >= 'a' && c <= 'f')
				c += 10 - 'a';
			else
				break;

			if (c >= Base)
				goto Exit;

			Addr[i] = Addr[i] * Base + c;
			cp++;
		}	// while (c = *cp)

		if ('.' == *cp) {
			if (i >= 3)
				goto Exit;
			cp++;
		} else if (*cp && !isspace(*cp))
			goto Exit;

        // Don't increment i if we're done.
        if (!*cp) {
            break;
        }
	}	// for (i)

	if (0 == i)
		RetAddr = Addr[0];
	else if (1 == i) {
		if (Addr[0] <= 0xff && Addr[1] <= 0xffFFff)
			RetAddr = (Addr[0] << 24) | Addr[1];
	} else if (2 == i) {
		if (Addr[0] <= 0xff && Addr[1] <= 0xff && Addr[2] <= 0xFFff)
			RetAddr = (Addr[0] << 24) | (Addr[1] << 16) | Addr[2];
	} else if (Addr[0] <= 0xff && Addr[1] <= 0xff && Addr[2] <= 0xff && 
			Addr[3] <= 0xff)
		RetAddr = (Addr[0] << 24) | (Addr[1] << 16) | (Addr[2] << 8) | Addr[3];

Exit:
	RetAddr = htonl(RetAddr);
	return RetAddr;

}	// inet_addr()


char * WSAAPI inet_ntoa(
	IN struct in_addr in) {

	SOCK_THREAD		*pThread;
	int		i;
	register unsigned char *p;
	// gmd -make volatile to work around SA1100 optimizer 
	volatile char	*pBuf, *p2Buf; 
	char			c, *pResult;

	DEBUGMSG(ZONE_INTERFACE, ( TEXT("+WS2: inet_ntoa\r\n") ));

	pResult = NULL;
	if (GetThreadData(&pThread)) {
		pBuf = p2Buf = pResult = pThread->st_ntoa_buffer;
		p = (unsigned char *)&in;

		for (i=3; i >= 0; i--) {
			do {
				*pBuf++ = p[i] % 10 + '0';
			} while (p[i] /= 10);
			*pBuf++ = '.';
		}
		*(--pBuf) = '\0';	// gets rid of last period & ends string
		// now reverse it...
		for (--pBuf ; p2Buf < pBuf; pBuf--, p2Buf++) {
			c = *pBuf;
			*pBuf = *p2Buf;
			*p2Buf = c;
		}
	}

	DEBUGMSG(ZONE_INTERFACE, ( TEXT("-WS2: inet_ntoa Ret: %X\r\n"),
		pResult));
	
	return pResult;

}	// inet_ntoa()


int WSAHtonl (
	SOCKET s,               
	u_long hostlong,        
	u_long *lpnetlong) {

	int			Err, Status = 0;
	WsSocket	*pSock;

	if (! (Err = RefSocketHandle(s, &pSock))) {

		__try {
			if (BIGENDIAN == pSock->ProtInfo.iNetworkByteOrder) {
				*lpnetlong = htonl(hostlong);
			} else
				*lpnetlong = hostlong;

		}

		__except(EXCEPTION_EXECUTE_HANDLER) {
			Err = WSAEFAULT;
		}

		DerefSocket(pSock);
	}

	if (Err) {
		Status = SOCKET_ERROR;
		SetLastError(Err);
	}
	return Status;

}	// WSAHtonl()


int WSAHtons (
	SOCKET s,               
	u_short host,        
	u_short *lpnet) {

	int			Err, Status = 0;
	WsSocket	*pSock;

	if (! (Err = RefSocketHandle(s, &pSock))) {

		__try {
			if (BIGENDIAN == pSock->ProtInfo.iNetworkByteOrder) {
				*lpnet = htons(host);
			} else
				*lpnet = host;
		}

		__except(EXCEPTION_EXECUTE_HANDLER) {
			Err = WSAEFAULT;
		}

		DerefSocket(pSock);
	}

	if (Err) {
		Status = SOCKET_ERROR;
		SetLastError(Err);
	}
	return Status;

}	// WSAHtons()


int WSANtohl (
	SOCKET s,               
	u_long hostlong,        
	u_long *lpnetlong) {

	return WSAHtonl(s, hostlong, lpnetlong);

}	// WSANtohl


int WSANtohs (
	SOCKET s,               
	u_short host,        
	u_short *lpnet) {

	return WSAHtons(s, host, lpnet);

}	// WSANtohs


int WSAAPI WSAAddressToString(
	IN		LPSOCKADDR	lpsaAddress,
	IN		DWORD		dwAddressLength,
	IN		LPWSAPROTOCOL_INFOW	lpProtocolInfo,
	IN OUT	LPWSTR		lpszAddressString,
	IN OUT	LPDWORD		lpdwAddressStringLength) {

	int					Err;
	WSAPROTOCOL_INFO	ProtInfo, *pProtInfo;
	WsProvider			*pProv;
	SOCKET				hSock = INVALID_SOCKET;
	int					Status;
	int					AddressFamily;

	DEBUGMSG(ZONE_FUNCTION || ZONE_SOCKET,
		 (TEXT("+WS2!WSAAddressToStringW:\r\n")));

//	if (Err = EnterDll()) {
//		goto Exit;
//	}

	if (!lpsaAddress || !lpszAddressString || !lpdwAddressStringLength) {
		SetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	
	if (lpProtocolInfo) {
		pProtInfo = lpProtocolInfo;
		AddressFamily = 0;
	} else {
		pProtInfo = &ProtInfo;
		AddressFamily = lpsaAddress->sa_family;
	}

	if (! (Err = GetProvider(AddressFamily, 0, 0, lpProtocolInfo, 
		&ProtInfo, &pProv))) {
		// this means the provider is up call him...
		Status = pProv->ProcTable.lpWSPAddressToString(lpsaAddress, 
			dwAddressLength, pProtInfo, lpszAddressString, 
			lpdwAddressStringLength, &Err);
			
		if (Status && ! Err) {
			ASSERT(0);
			Err = WSASYSCALLFAILURE;
		}
		DerefProvider(&ProtInfo.ProviderId, pProv);

	}

	if (Err) {
		SetLastError(Err);
		Status = SOCKET_ERROR;
	}
	
	return Status;

}	// WSAAddressToString()


int WSAAPI WSAStringToAddress(
	IN     LPWSTR              AddressString,
	IN     INT                 AddressFamily,
	IN     LPWSAPROTOCOL_INFOW lpProtocolInfo,
	OUT    LPSOCKADDR          lpAddress,
	IN OUT LPINT               lpAddressLength) {

	int					Err;
	WSAPROTOCOL_INFO	ProtInfo, *pProtInfo;
	WsProvider			*pProv;
	SOCKET				hSock = INVALID_SOCKET;
	int					Status;

	DEBUGMSG(ZONE_FUNCTION || ZONE_SOCKET,
		 (TEXT("+WS2!WSAStringToAddress:\r\n")));

//	if (Err = EnterDll()) {
//		goto Exit;
//	}

    if (!lpAddressLength)
	{
	    SetLastError(WSAEFAULT);
	    return SOCKET_ERROR;
	}

	if (lpProtocolInfo) {
		pProtInfo = lpProtocolInfo;
	} else {
		pProtInfo = &ProtInfo;
	}

	if (! (Err = GetProvider(AddressFamily, 0, 0, lpProtocolInfo,
		&ProtInfo, &pProv))) {
		// this means the provider is up call him...
		Status = pProv->ProcTable.lpWSPStringToAddress(AddressString, 
			AddressFamily, pProtInfo, lpAddress, lpAddressLength, &Err);
			
		if (Status && ! Err) {
			ASSERT(0);
			Err = WSASYSCALLFAILURE;
		}
		DerefProvider(&ProtInfo.ProviderId, pProv);

	}

	if (Err) {
		SetLastError(Err);
		Status = SOCKET_ERROR;
	}
	
	return Status;

}	// WSAStringToAddress()


SOCKET WSAAPI WSAJoinLeaf(
	IN SOCKET s,
	IN const struct sockaddr FAR * name,
	IN int namelen,
	IN LPWSABUF lpCallerData,
	OUT LPWSABUF lpCalleeData,
	IN LPQOS lpSQOS,
	IN LPQOS lpGQOS,
	IN DWORD dwFlags) {

	int			Err;
	WsSocket	*pSock;
	SOCKET		Sock;

	DEBUGMSG(ZONE_FUNCTION || ZONE_SOCKET,
		 (TEXT("+WS2!WSAJoinLeaf:\r\n")));

	if (! (Err = RefSocketHandle(s, &pSock))) {

		Sock = pSock->pProvider->ProcTable.lpWSPJoinLeaf(
			pSock->hWSPSock, name, namelen, lpCallerData, lpCalleeData, 
			lpSQOS, lpGQOS, dwFlags, &Err);

		if ((INVALID_SOCKET == Sock) && ! Err) {
			ASSERT(0);
			Err = WSASYSCALLFAILURE;
		}

		DerefSocket(pSock);
	}

	if (Err) {
		SetLastError(Err);
		Sock = INVALID_SOCKET;
	}

	return Sock;

}	// WSAJoinLeaf()


