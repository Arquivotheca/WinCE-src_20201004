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
/******************************************************************************/
/**                            Microsoft Windows                             **/
/******************************************************************************/

/*

calls.c

pass-through calls from the CE WSP to the PM (afd) layer


FILE HISTORY:
	OmarM     13-Sep-2000
	

*/

#include "wspmp.h"
#include <mswsock.h>
#include <cenet.h>

#define STACK_ALLOC_BUFS	6

unsigned long MyInetAddrW (const WCHAR * cp) {
	unsigned long	Addr[4], RetAddr = INADDR_NONE;
	unsigned int	Base, i;
	WCHAR			c;

	if (! cp)
		goto Exit;

	memset(Addr, 0, sizeof(Addr));
	for (i = 0; i < 4 && *cp; i++) {
		// first figure out the base
		if (TEXT('0') == *cp) {
			cp++;
			if (TEXT('X') == *cp || TEXT('x') == *cp) {
				cp++;
				Base = 16;
			} else
				Base = 8;
		} else
			Base = 10;

		while (c = *cp) {
			// must have some number now...
			if (c >= TEXT('0') && c <= TEXT('9'))
				c -= TEXT('0');
			else if (c >= TEXT('A') && c <= TEXT('F'))
				c +=  10 - TEXT('A');
			else if (c >= TEXT('a') && c <= TEXT('f'))
				c += 10 - TEXT('a');
			else
				break;

			if (c > Base)
				goto Exit;

			Addr[i] = Addr[i] * Base + c;
			cp++;
		}	// while (c = *cp)

		if (TEXT('.') == *cp) {
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

}	// MyInetAddrW()


int MyInetNtoaW(WCHAR *pszwAddr, DWORD *pcAddr, struct in_addr Addr) {
	int						Err, i;
	register unsigned char	*p;
	volatile WCHAR			*pBuf;
	DWORD					cLen;
	WCHAR					wszTemp[18];					

	pBuf = pszwAddr;
    p = (unsigned char *)&Addr;

	i = Err = cLen = 0;
	
	for (i = 3; i >= 0; i--) {
		do {
			ASSERT(cLen < 17);
			PREFAST_SUPPRESS(394, "a byte can at most be div by 10 3 times")
			wszTemp[cLen++] = p[i] % 10 + L'0';
		} while (p[i] /= 10);

		wszTemp[cLen++] = L'.';
	}

	if (cLen <= *pcAddr) {
		*pcAddr = cLen--;
		while (cLen) {
			*pBuf++ = wszTemp[--cLen];
		}
		*pBuf = L'\0';
	} else {
		*pcAddr = cLen;
		Err = WSA_NOT_ENOUGH_MEMORY;
	}

	return Err;
	
}	// MyInetNtoaW()


// returns length of string in *pcStr
int MyShortToStr(ushort cNum, TCHAR *pStr, int *pcStr) {
	int		Err , cLen;
	TCHAR	tszTemp[6];

	cLen = Err = 0;
	do {
		ASSERT(cLen < 6);
		PREFAST_SUPPRESS(394, "a ushort can't be divided by 10 > 6 times")
		tszTemp[cLen++] = TEXT('0') + (cNum % 10);
	} while (cNum /= 10);

	if (cLen < *pcStr) {
		*pcStr = cLen + 1;
		while (cLen) {
			*pStr++ = tszTemp[--cLen];
		}
		*pStr = TEXT('\0');
	} else {
		*pcStr = cLen + 1;
		Err = WSA_NOT_ENOUGH_MEMORY;
	}

	return Err;

} // MyShortToStr()


int OvCompletionRtn(LPWSAOVERLAPPED pOv) {
	LPWSAOVERLAPPED_COMPLETION_ROUTINE pCompRtn;
	DWORD			dwError;
	DWORD			cbTransferred;
	DWORD			dwFlags;

	ASSERT(WSS_OPERATION_IN_PROGRESS != pOv->Internal);
	pCompRtn = (LPWSAOVERLAPPED_COMPLETION_ROUTINE)(pOv->Internal);
	dwError = pOv->OffsetHigh;
	cbTransferred = pOv->InternalHigh;
	dwFlags = pOv->Offset;

	pCompRtn(dwError, cbTransferred, pOv, dwFlags);

	return TRUE;
}	// OvCompletionRtn


void
MatchCurrentThreadPriority(
    HANDLE hThd
    )
{
    CeSetThreadPriority(hThd, CeGetThreadPriority(GetCurrentThread()));
}

HANDLE
StartOvCompletionThd(
	LPWSAOVERLAPPED pOv
    )
{
	DWORD	ThrdId = 0;
	HANDLE	hThd = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)OvCompletionRtn,
			pOv, CREATE_SUSPENDED, &ThrdId);
    if (hThd) {
        MatchCurrentThreadPriority(hThd);
		CloseHandle(hThd);
    }
    RETAILMSG(1, (L"WSPM:StartOvCompletionThd - Overlapped completion routine specified!\n"));
    RETAILMSG(1, (L"WSPM:StartOvCompletionThd - Event notification for overlapped I/O is more efficient!\n"));
    return (HANDLE)ThrdId;
}


SOCKET WSPAPI WSPAccept (
	SOCKET s,
	struct sockaddr FAR * addr,
	LPINT addrlen,
	LPCONDITIONPROC lpfnCondition,
	DWORD_PTR dwCallbackData,
	LPINT lpErrno) {

	int			Err;
	int			cLocalAddr;
	SOCKHAND	hNewSock;

	if ((addr && ! addrlen) || (!addr && addrlen && *addrlen)) {
		// if there is an addr there needs to be a length
		// if there is no addr then addrlen needs to be NULL or *addrlen == 0
		Err = WSAEFAULT;

	} else {

		if (addrlen)
			cLocalAddr = *addrlen;
		else
			cLocalAddr = 0;
		
		Err = AFDAccept((SOCKHAND)s, &hNewSock, addr, cLocalAddr, addrlen, 
			lpfnCondition, dwCallbackData);
	}

	if (Err) {
		*lpErrno = Err;
		hNewSock = (SOCKHAND)INVALID_SOCKET;
	}
	return (SOCKET)hNewSock;
	
}	// WSPAccept()


int WSPAPI WSPAddressToString (
	LPSOCKADDR lpsaAddress,
	DWORD dwAddressLength,
	LPWSAPROTOCOL_INFOW lpProtocolInfo,
	LPWSTR lpszAddressString,
	LPDWORD lpdwAddressStringLength,
	LPINT lpErrno) {

	int				Err;
	SOCKADDR_IN		*pSin = (SOCKADDR_IN *)lpsaAddress;
	DWORD			cLen, cPort;
	u_short			Port;
	WCHAR			*p;

	// for now we only work for v6 if v6 is specified
	if (AF_INET6 == lpsaAddress->sa_family) {
		Err = PMAddrConvert(1, AF_INET6, lpsaAddress, dwAddressLength, 
			&dwAddressLength, lpProtocolInfo, sizeof(*lpProtocolInfo), 
			lpszAddressString, *lpdwAddressStringLength * 2, 
			lpdwAddressStringLength);
	} else {
		if (AF_UNSPEC != lpsaAddress->sa_family && 
			AF_INET != lpsaAddress->sa_family) {
			Err = WSAEINVAL;
			
		} else if (dwAddressLength < sizeof(SOCKADDR_IN)) {
			Err = WSAEINVAL;
		} else {

			cLen = *lpdwAddressStringLength;
			Err = MyInetNtoaW(lpszAddressString, &cLen, pSin->sin_addr);

			cPort = 0;

			if (Port = pSin->sin_port) {

				if (Err) {
					p = lpszAddressString;
				} else {
					ASSERT(*lpdwAddressStringLength >= cLen);
					
					cPort = *lpdwAddressStringLength - cLen;
					p = lpszAddressString + cLen;
				}
				
				Port = ((Port << 8) | (Port >> 8));
				Err = MyShortToStr(Port, p, &cPort);
				if (! Err) {
					lpszAddressString[cLen - 1] = L':';
				}
			}

			*lpdwAddressStringLength = cLen + cPort;

			// convert the error code..the specs really should return 
			// WSA_NOT_ENOUGH_MEMORY to be consistent!!!
			if (WSA_NOT_ENOUGH_MEMORY == Err)
				Err = WSAEFAULT;
			
		}
	}

	if (Err) {
		*lpErrno = Err;
		Err = SOCKET_ERROR;
	}
	
	return Err;
	
}	// WSPAddressToString()


int WSPAPI WSPAsyncSelect (
	SOCKET s,
	HWND hWnd,
	unsigned int wMsg,
	long lEvent,
	LPINT lpErrno) {

	*lpErrno = WSASYSCALLFAILURE;
	return SOCKET_ERROR;
	
}	// WSPAsyncSelect


int WSPAPI WSPBind (
	SOCKET s,
	const struct sockaddr FAR * name,
	int namelen,
	LPINT lpErrno) {

	int	Err;

	Err = AFDBind((SOCKHAND)s, (struct sockaddr *)name, namelen);

	if (Err) {
		*lpErrno = Err;
		Err = SOCKET_ERROR;
	}
	return Err;

}	// WSPBind()


int WSPAPI WSPCancelBlockingCall (
	LPINT lpErrno) {

	*lpErrno = WSASYSCALLFAILURE;
	return SOCKET_ERROR;

}	// WSPCancelBlockingCall()


// this is in init.c
extern int WSPAPI WSPCleanup (LPINT lpErrno);


int WSPAPI WSPCloseSocket (
	SOCKET s,
	LPINT lpErrno) {

	if (AFDCloseSocket((SOCKHAND)s)) {
		CloseHandle((HANDLE)s);
		return 0;
	} else {
		*lpErrno = GetLastError();
		return SOCKET_ERROR;
	}

}	// WSPCloseSocket()


int WSPAPI WSPConnect (
	SOCKET s,
	const struct sockaddr FAR * name,
	int namelen,
	LPWSABUF lpCallerData,
	LPWSABUF lpCalleeData,
	LPQOS lpSQOS,
	LPQOS lpGQOS,
	LPINT lpErrno) {

	INT	Err;

	Err = AFDConnect((SOCKHAND)s, (PSOCKADDR)name, namelen);

	if (Err) {
		*lpErrno = Err;
		Err = SOCKET_ERROR;
	}
	return Err;

}	// WSPConnect()


int WSPAPI WSPDuplicateSocket (
	SOCKET s,
	DWORD dwProcessId,
	LPWSAPROTOCOL_INFOW lpProtocolInfo,
	LPINT lpErrno) {

	*lpErrno = WSASYSCALLFAILURE;
	return SOCKET_ERROR;

}	// WSPDuplicateSocket()


extern int WSPAPI WSPEnumNetworkEvents (
	SOCKET s,
	WSAEVENT hEventObject,
	LPWSANETWORKEVENTS lpNetworkEvents,
	LPINT lpErrno);


extern int WSPAPI WSPEventSelect (
	SOCKET s,
	WSAEVENT hEventObject,
	long lNetworkEvents,
	LPINT lpErrno);


BOOL WSPAPI WSPGetOverlappedResult (
	SOCKET s,
	LPWSAOVERLAPPED lpOverlapped,
	LPDWORD lpcbTransfer,
	BOOL fWait,
	LPDWORD lpdwFlags,
	LPINT lpErrno) {

	DWORD		Err, Status;

	Status = Err = 0;

	if (lpOverlapped) {
		if (WSS_OPERATION_IN_PROGRESS == lpOverlapped->Internal) {
			if (fWait) {
				if (lpOverlapped->hEvent) {
					Err = WaitForSingleObject((HANDLE)lpOverlapped->hEvent, 
						INFINITE);
					if (WAIT_FAILED == Err) {
						Err = GetLastError();
					} else {
						Err = lpOverlapped->OffsetHigh;	
					}
				} else {
					Err = WSA_INVALID_HANDLE;
				}
			} else {	// if (fWait)
				Err = WSA_IO_INCOMPLETE;
			}
		} else {
			// operation is already completed.
			Err = lpOverlapped->OffsetHigh;
		}
	} else {
		Err = WSAEINVAL;
	}

	if (!Err || (WSAEMSGSIZE == Err)) {
		__try {
			*lpdwFlags = lpOverlapped->Offset;
			*lpcbTransfer = lpOverlapped->InternalHigh;
			if (! Err)
				Status = TRUE;
		}
		__except(EXCEPTION_EXECUTE_HANDLER ) {
			Err = WSAEFAULT;
		}
	}

	*lpErrno = Err;

	return Status;
	
}	// WSPGetOverlappedResult()


int WSPAPI WSPGetPeerName (
	SOCKET s,
	struct sockaddr FAR * name,
	LPINT namelen,
	LPINT lpErrno) {

	int	Err;

	Err = AFDGetpeername((SOCKHAND)s, name, *namelen, namelen);

	if (Err) {
		*lpErrno = Err;
		Err = SOCKET_ERROR;
	}
	return Err;

}	// WSPGetPeerName()


int WSPAPI WSPGetSockName (
	SOCKET s,
	struct sockaddr FAR * name,
	LPINT namelen,
	LPINT lpErrno) {

	int Err;

	Err = AFDGetsockname((SOCKHAND)s, name, *namelen, namelen);

	if (Err) {
		*lpErrno = Err;
		Err = SOCKET_ERROR;
	}
	return Err;

}	// WSPGetSockName()


int WSPAPI WSPGetSockOpt (
	SOCKET s,
	int level,
	int optname,
	char FAR * optval,
	LPINT optlen,
	LPINT lpErrno) {

	int	Err;

	Err = AFDGetSockOpt((SOCKHAND)s, level, optname, optval, *optlen, optlen);

	if (Err) {
		*lpErrno = Err;
		Err = SOCKET_ERROR;
	}
	return Err;

}	// WSPGetSockOpt()


BOOL WSPAPI WSPGetQOSByName (
	SOCKET s,
	LPWSABUF lpQOSName,
	LPQOS lpQOS,
	LPINT lpErrno) {

	*lpErrno = WSASYSCALLFAILURE;
	return SOCKET_ERROR;
	
}	// WSPGetQOSByName()


int WSPAPI WSPRecvMsg (
	SOCKET s,
	LPWSAMSG lpMsg,
	LPDWORD lpNumberOfBytesRecvd,
	LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
	LPWSATHREADID lpThreadId,
	LPINT lpErrno) {

	int			Err;
	WSATHREADID	WsaThreadId;
	WSABUF		*pWsaBufs, aWsaBufs[STACK_ALLOC_BUFS];
	
	if (NULL == lpMsg) {
		Err = WSAEFAULT;
		goto Exit;
	}		

	if (lpMsg->dwBufferCount > STACK_ALLOC_BUFS) {
		pWsaBufs = LocalAlloc(0, sizeof(WSABUF) * lpMsg->dwBufferCount);
		if (! pWsaBufs) {
			Err = WSAENOBUFS;
			goto Exit;
		}
	} else {
		pWsaBufs = aWsaBufs;
	}

	memcpy(pWsaBufs, lpMsg->lpBuffers, sizeof(WSABUF) * lpMsg->dwBufferCount);

#ifdef DEBUG
	{
		DWORD		i;

		for (i = 0; i < lpMsg->dwBufferCount; i++) {
			ASSERT(pWsaBufs[i].len == lpMsg->lpBuffers[i].len);
			ASSERT(pWsaBufs[i].buf == lpMsg->lpBuffers[i].buf);
		}
	}
#endif

	if (lpOverlapped && lpCompletionRoutine) {
		WsaThreadId.ThreadHandle = StartOvCompletionThd(lpOverlapped);
		lpThreadId = &WsaThreadId;
	}

	Err = AFDRecvImpl((SOCKHAND)s, pWsaBufs, lpMsg->dwBufferCount, 
		lpNumberOfBytesRecvd, &lpMsg->Control, &lpMsg->dwFlags, lpMsg->name, 
		&lpMsg->namelen, lpOverlapped, lpCompletionRoutine, 
		&WsaThreadId);

	if (aWsaBufs != pWsaBufs)
		LocalFree(pWsaBufs);

Exit:	
	if (Err) {
		*lpErrno = Err;
		Err = SOCKET_ERROR;
	}
	
	return Err;

}	// WSPRecvMsg()


int WSPAPI WSPIoctl (
	SOCKET s,
	DWORD dwIoControlCode,
	LPVOID lpvInBuffer,
	DWORD cbInBuffer,
	LPVOID lpvOutBuffer,
	DWORD cbOutBuffer,
	LPDWORD lpcbBytesReturned,
	LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
	LPWSATHREADID lpThreadId,
	LPINT lpErrno) {

	int					Err;
	WSATHREADID			WsaThreadId;

	if (SIO_RECV_MSG == dwIoControlCode) {
		if(cbInBuffer != sizeof(WSAMSG)) {
			*lpErrno = WSAEFAULT;
			return SOCKET_ERROR;
		}
		return WSPRecvMsg(s, (LPWSAMSG)lpvInBuffer, lpcbBytesReturned, 
			lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);
	}

	if (lpOverlapped && lpCompletionRoutine) {
		WsaThreadId.ThreadHandle = StartOvCompletionThd(lpOverlapped);
		lpThreadId = &WsaThreadId;
	}

	Err = AFDIoctl((SOCKHAND)s, dwIoControlCode, lpvInBuffer, cbInBuffer,
		lpvOutBuffer, cbOutBuffer, lpcbBytesReturned, lpOverlapped, 
		lpCompletionRoutine, lpThreadId);

	if (Err) {
		*lpErrno = Err;
		Err = SOCKET_ERROR;
	}
	return Err;

}	// WSPIoctl()


SOCKET WSPAPI WSPJoinLeaf (
	SOCKET s,
	const struct sockaddr FAR * name,
	int namelen,
	LPWSABUF lpCallerData,
	LPWSABUF lpCalleeData,
	LPQOS lpSQOS,
	LPQOS lpGQOS,
	DWORD dwFlags,
	LPINT lpErrno) {

	*lpErrno = WSASYSCALLFAILURE;
	return INVALID_SOCKET;
	
}	// WSPJoinLeaf()


int WSPAPI WSPListen (
	SOCKET s,
	int backlog,
	LPINT lpErrno) {

	int	Err;

	Err = AFDListen((SOCKHAND)s, backlog);

	if (Err) {
		*lpErrno = Err;
		Err = SOCKET_ERROR;
	}
	return Err;

}	// WSPListen()


int WSPAPI WSPRecv (
	SOCKET s,
	LPWSABUF lpBuffers,
	DWORD dwBufferCount,
	LPDWORD lpNumberOfBytesRecvd,
	LPDWORD lpFlags,
	LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
	LPWSATHREADID lpThreadId,
	LPINT lpErrno) {

	int			Err;
	WSATHREADID	WsaThreadId;


	if (lpOverlapped && lpCompletionRoutine) {
		WsaThreadId.ThreadHandle = StartOvCompletionThd(lpOverlapped);
		lpThreadId = &WsaThreadId;
	}

	Err = AFDRecvImpl((SOCKHAND)s, lpBuffers, dwBufferCount, 
		lpNumberOfBytesRecvd, NULL, lpFlags, NULL, NULL, lpOverlapped, 
		lpCompletionRoutine, lpThreadId);

	if (Err) {
		*lpErrno = Err;
		Err = SOCKET_ERROR;
	}
	return Err;

}	// WSPRecv()


int WSPAPI WSPRecvDisconnect (
	SOCKET s,
	LPWSABUF lpInboundDisconnectData,
	LPINT lpErrno) {

	*lpErrno = WSASYSCALLFAILURE;
	return SOCKET_ERROR;
	
}	// WSPRecvDisconnect()


int WSPAPI WSPRecvFrom (
	SOCKET s,
	LPWSABUF lpBuffers,
	DWORD dwBufferCount,
	LPDWORD lpNumberOfBytesRecvd,
	LPDWORD lpFlags,
	struct sockaddr FAR * lpFrom,
	LPINT lpFromlen,
	LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
	LPWSATHREADID lpThreadId,
	LPINT lpErrno) {

	int			Err;
	WSATHREADID	WsaThreadId;
	

	if (lpOverlapped && lpCompletionRoutine) {
		WsaThreadId.ThreadHandle = StartOvCompletionThd(lpOverlapped);
		lpThreadId = &WsaThreadId;
	}

	Err = AFDRecvImpl((SOCKHAND)s, lpBuffers, dwBufferCount, 
		lpNumberOfBytesRecvd, NULL, lpFlags, lpFrom, lpFromlen, lpOverlapped, 
		lpCompletionRoutine, lpThreadId);

	if (Err) {
		*lpErrno = Err;
		Err = SOCKET_ERROR;
	}
	return Err;

}	// WSPRecvFrom()


extern int WSPAPI WSPSelect (
	int nfds,
	fd_set FAR * readfds,
	fd_set FAR * writefds,
	fd_set FAR * exceptfds,
	const struct timeval FAR * timeout,
	LPINT lpErrno);


int WSPAPI WSPSend (
	SOCKET s,
	LPWSABUF lpBuffers,
	DWORD dwBufferCount,
	LPDWORD lpNumberOfBytesSent,
	DWORD dwFlags,
	LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
	LPWSATHREADID lpThreadId,
	LPINT lpErrno) {

	int			Err;
	WSATHREADID	WsaThreadId;

	if (lpOverlapped && lpCompletionRoutine) {
		WsaThreadId.ThreadHandle = StartOvCompletionThd(lpOverlapped);
		lpThreadId = &WsaThreadId;
	}

	Err = AFDSend((SOCKHAND)s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, 
		dwFlags, NULL, 0, lpOverlapped, lpCompletionRoutine, 
		lpThreadId);

	if (Err) {
		*lpErrno = Err;
		Err = SOCKET_ERROR;
	}
	return Err;

}	// WSPSend()


int WSPAPI WSPSendDisconnect (
	SOCKET s,
	LPWSABUF lpOutboundDisconnectData,
	LPINT lpErrno) {

	*lpErrno = WSASYSCALLFAILURE;
	return SOCKET_ERROR;
	
}	// WSPSendDisconnect()


int WSPAPI WSPSendTo (
	SOCKET s,
	LPWSABUF lpBuffers,
	DWORD dwBufferCount,
	LPDWORD lpNumberOfBytesSent,
	DWORD dwFlags,
	const struct sockaddr FAR * lpTo,
	int iTolen,
	LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
	LPWSATHREADID lpThreadId,
	LPINT lpErrno) {

	int			Err;
	WSATHREADID	WsaThreadId;


	if (lpOverlapped && lpCompletionRoutine) {
		WsaThreadId.ThreadHandle = StartOvCompletionThd(lpOverlapped);
		lpThreadId = &WsaThreadId;
	}

	Err = AFDSend((SOCKHAND)s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, 
		dwFlags, (struct sockaddr *)lpTo, iTolen, lpOverlapped, 
		lpCompletionRoutine, lpThreadId);

	if (Err) {
		*lpErrno = Err;
		Err = SOCKET_ERROR;
	}
	return Err;

}	// WSPSendTo()


int WSPAPI WSPSetSockOpt (
	SOCKET s,
	int level,
	int optname,
	const char FAR * optval,
	int optlen,
	LPINT lpErrno) {

	int Err;

	Err = AFDSetSockOpt((SOCKHAND)s, level, optname, (char *)optval, optlen);

	if (Err) {
		*lpErrno = Err;
		Err = SOCKET_ERROR;
	}
	return Err;

}	// WSPSetSockOpt()


int WSPAPI WSPShutdown (
	SOCKET s,
	int how,
	LPINT lpErrno) {

	int	Err;

	Err = AFDShutdown((SOCKHAND)s, how);

	if (Err) {
		*lpErrno = Err;
		Err = SOCKET_ERROR;
	}
	return Err;

}	// WSPShutdown()


SOCKET WSPAPI WSPSocket (
	int af,
	int type,
	int protocol,
	LPWSAPROTOCOL_INFOW lpProtocolInfo,
	GROUP g,
	DWORD dwFlags,
	LPINT lpErrno) {

	SOCKHAND	hSock;

	if ( (~WSA_FLAG_OVERLAPPED) & dwFlags) {
		*lpErrno = WSAEINVAL;
		hSock = (SOCKHAND)INVALID_SOCKET;
	} else {

		if (lpProtocolInfo) {
			if (FROM_PROTOCOL_INFO == af)
				af = lpProtocolInfo->iAddressFamily;

			if (FROM_PROTOCOL_INFO == type)
				type = lpProtocolInfo->iSocketType;

			if (FROM_PROTOCOL_INFO == protocol)
				protocol = lpProtocolInfo->iProtocol;
		}

		hSock = AFDSocket(af, type, protocol, 
			lpProtocolInfo->dwCatalogEntryId,  &lpProtocolInfo->ProviderId);

		if (NULL == hSock) {
			*lpErrno = GetLastError();
			hSock = (SOCKHAND)INVALID_SOCKET;
		}
	}

	return (SOCKET)hSock;

}	// WSPSocket()


INT WSPAPI WSPStringToAddress (
	LPWSTR AddressString,
	INT AddressFamily,
	LPWSAPROTOCOL_INFOW lpProtocolInfo,
	LPSOCKADDR lpAddress,
	LPINT lpAddressLength,
	LPINT lpErrno) {

	int				Err;
	DWORD			cAddressString;

	// for now we only work for v6 if v6 is specified
	if (AF_INET6 == AddressFamily || AF_INET == AddressFamily || 
		AF_UNSPEC == AddressFamily) {

		cAddressString = (wcslen(AddressString) + 1) * 2;
		Err = PMAddrConvert(2, AddressFamily, lpAddress, *lpAddressLength, 
			lpAddressLength, lpProtocolInfo, sizeof(*lpProtocolInfo), 
			AddressString, cAddressString, &cAddressString);
		
	} else {
		Err = WSAEINVAL;
	}

	if (Err) {
		*lpErrno = Err;
		Err = SOCKET_ERROR;
	}
	
	return Err;
	
}	// WSPStringToAddress()


WSPPROC_TABLE	v_aWspmProcTable = {
	&WSPAccept,
	&WSPAddressToString,
	&WSPAsyncSelect,
	&WSPBind,
	&WSPCancelBlockingCall,
	&WSPCleanup,
	&WSPCloseSocket,
	&WSPConnect,
	&WSPDuplicateSocket,
	&WSPEnumNetworkEvents,
	&WSPEventSelect,
	&WSPGetOverlappedResult,
	&WSPGetPeerName,
	&WSPGetSockName,
	&WSPGetSockOpt,
	&WSPGetQOSByName,
	&WSPIoctl,
	&WSPJoinLeaf,
	&WSPListen,
	&WSPRecv,
	&WSPRecvDisconnect,
	&WSPRecvFrom,
	&WSPSelect,
	&WSPSend,
	&WSPSendDisconnect,
	&WSPSendTo,
	&WSPSetSockOpt,
	&WSPShutdown,
	&WSPSocket,
	&WSPStringToAddress
};


