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

socket.c

FILE HISTORY:
	OmarM     14-Sep-2000

*/


#include "winsock2p.h"
#include <cxport.h>

extern void SetErrorCode(int Code, LPINT pErr);

//
//  Private constants.
//


//
//  Private types.
//


//
//  Private globals.
//


//
//  Private prototypes.
//

#if defined(WIN32) && !defined(UNDER_CE)
BOOL APIENTRY SetHandleContext(HANDLE s, DWORD context);    // in kernel32.dll
#endif	

//
//  Public functions.
//


WSPUPCALLTABLE UpCalls = {
	WPUCloseEvent, 
	WPUCloseSocketHandle,
	WPUCreateEvent,
	WPUCreateSocketHandle,
	WPUFDIsSet,
	WPUGetProviderPath,
	WPUModifyIFSHandle,
	WPUPostMessage,
	WPUQueryBlockingCallback,
	WPUQuerySocketHandleContext,
	WPUQueueApc,
	WPUResetEvent,
	WPUSetEvent,
	WPUOpenCurrentThread,
	WPUCloseThread
};

DEFINE_LOCK_STRUCTURE(s_ProviderListLock);




#define MAX_SOCKETS		256
#define SOCK_HASH_SIZE	64

WsSocket	*v_apWsSocks[SOCK_HASH_SIZE];

WsProvider	*s_pProviderList;

int			v_fClosingAll;

extern int	v_fProcessDetached;
extern DWORD	s_cDllRefs;

// owner must lock...
SOCKET NewSocketHandle(WsSocket *pNewSock) {
	static SOCKET	LastUsed;
    SOCKET			i;
    WsSocket		*pSock;
    unsigned int	Max = MAX_SOCKETS;
	
	for (i = 1; i <= Max; i++) {
		if (INVALID_SOCKET == LastUsed + i) {
			Max++;
			continue;
		}
		pSock = v_apWsSocks[(LastUsed + i) % SOCK_HASH_SIZE];
		
		for (; pSock; pSock = pSock->pNext) {
			if (pSock->hSock == (LastUsed + i))
				break;
		}	// for ()

		if (!pSock)
			break;
	}

	if (i > Max)
		return INVALID_SOCKET;

	LastUsed += i;
	
	pNewSock->hSock = LastUsed;
	pNewSock->pNext = v_apWsSocks[LastUsed % SOCK_HASH_SIZE];
	v_apWsSocks[LastUsed % SOCK_HASH_SIZE] = pNewSock;
	
	return LastUsed;
}	// NewSocketHandle()


WsSocket **_FindSocketHandle(SOCKET hSock) {
	WsSocket	**ppSock;

	ppSock = &v_apWsSocks[hSock % SOCK_HASH_SIZE];

	while (*ppSock) {
		if ((*ppSock)->hSock == hSock)
			break;
		ppSock = &(*ppSock)->pNext;
	}

	return ppSock;

}	// _FindSocketHandle()


int RefSocketHandle(SOCKET hSock, WsSocket **ppSock) {
	WsSocket	*pSock;
	int			Err;

	if (! v_fProcessDetached) {
		CTEGetLock(&v_DllCS, 0);
		if (s_cDllRefs > 0) {

			pSock = v_apWsSocks[hSock % SOCK_HASH_SIZE];

			while (pSock) {
				if (hSock == pSock->hSock)
					break;
				pSock = pSock->pNext;
			}

			if (pSock && !(pSock->Flags & WS_SOCK_FL_CLOSED)) {
				pSock->cRefs++;
				Err = 0;
			} else {
				Err = WSAENOTSOCK;
			}

		} else {
			Err = WSANOTINITIALISED;
		}

		CTEFreeLock(&v_DllCS, 0);
		if (! Err) {
			*ppSock = pSock;
		}
	} else {
		Err = WSAENETDOWN;
	}

	return Err;
	
}	// RefSocketHandle()


int DerefProvider(GUID *pId, WsProvider *pDelProv);


int DerefSocket(WsSocket *pSock) {
	WsSocket	**ppSock;
	int			i;
	WsProvider	*pProv;
	GUID		Id;

	CTEGetLock(&v_DllCS, 0);
	i = --pSock->cRefs;
	ASSERT(i >= 0);
	if (i <= 0) {
		ASSERT(pSock->Flags & WS_SOCK_FL_CLOSED);
		ppSock = _FindSocketHandle(pSock->hSock);
		if (*ppSock) {
			ASSERT(*ppSock == pSock);
			*ppSock = pSock->pNext;
		}
		pProv = pSock->pProvider;
		CTEFreeLock(&v_DllCS, 0);
		LocalFree(pSock);
		if (pProv) {
			memcpy(&Id, &pProv->Id, sizeof(Id));
			DerefProvider(&Id, pProv);
		}
	} else
		CTEFreeLock(&v_DllCS, 0);

	return i;

}	// DerefSocket()


int DerefSocketHandle(SOCKET hSock) {
	WsSocket	*pSock, **ppSock;
	int			i, fLocked;
	WsProvider	*pProv;
	GUID		Id;

	CTEGetLock(&v_DllCS, 0);
	fLocked = TRUE;
	ppSock = _FindSocketHandle(hSock);
	if (pSock = *ppSock) {
		i = --pSock->cRefs;
		ASSERT(i >= 0);
		if (i <= 0) {
			ASSERT(pSock->Flags & WS_SOCK_FL_CLOSED);
			*ppSock = pSock->pNext;
			pProv = pSock->pProvider;
			LocalFree(pSock);
			CTEFreeLock(&v_DllCS, 0);
			fLocked = FALSE;
			if (pProv) {
				memcpy(&Id, &pProv->Id, sizeof(Id));
				DerefProvider(&Id, pProv);
			}
		}
		
	} else {
		i = -1;
	}
	if (fLocked)
		CTEFreeLock(&v_DllCS, 0);

	return i;
	
}	// DerefSocketHandle()


WsProvider **_FindProvider(GUID *pId) {
	WsProvider **ppProv;

	for (ppProv = &s_pProviderList; *ppProv; ppProv = &((*ppProv)->pNext))
		if (0 == memcmp(&(*ppProv)->Id, pId, sizeof(GUID)))
			break;

	return ppProv;
}


// is called without the lock held and returns with lock held!
WsProvider *FindProvider(GUID *pId) {
	WsProvider	*pProv;

	CTEGetLock(&s_ProviderListLock, 0);
	pProv = *_FindProvider(pId);
	return pProv;

}	// FindProvider();


void FreeProvider(WsProvider *pProv) {

	if (! v_fProcessDetached) {
		int	Err;

		if (SOCKET_ERROR == pProv->ProcTable.lpWSPCleanup(&Err)) {
			DEBUGMSG(ZONE_WARN, 
				(TEXT("FreeProvider: WSPCleanup Err %d\r\n"), Err));
		}
		if (pProv->hLibrary)
			FreeLibrary(pProv->hLibrary);
	}
	LocalFree(pProv);

}   // FreeProvider()


int FreeProviders(uint Flags) {
	WsProvider	*pProv, **ppProv;

	CTEGetLock(&s_ProviderListLock, 0);

	ppProv = &s_pProviderList;
	while (pProv = *ppProv) {

		ASSERT(0 <= pProv->cRefs);
		if (0 >= pProv->cRefs) {
			*ppProv = pProv->pNext;
			FreeProvider(pProv);
		} else {
			ppProv = &pProv->pNext;
		}
	}

	CTEFreeLock(&s_ProviderListLock, 0);

	// ignore errors here, not much we can do at this stage anyway
	return 0;
	
}	// FreeProviders()


int DerefProvider(GUID *pId, WsProvider *pProv) {
	int			Status, fDelete;
	WsProvider	**ppProv;

	CTEGetLock(&v_DllCS, 0);
	if (Started() || v_fProcessDetached)
		fDelete = TRUE;
	else
		fDelete = FALSE;
	CTEFreeLock(&v_DllCS, 0);

	CTEGetLock(&s_ProviderListLock, 0);

	if (! pProv || fDelete) {
		ppProv = _FindProvider(pId);
		pProv = *ppProv;
	}

	if (pProv) {
		Status = TRUE;
		pProv->cRefs--;
		ASSERT(0 <= pProv->cRefs);
		if (fDelete && (0 == pProv->cRefs)) {
			*ppProv = pProv->pNext;
			FreeProvider(pProv);
		}
	} else
		Status = FALSE;

	CTEFreeLock(&s_ProviderListLock, 0);
	return Status;
	
}	// DerefProvider();


int GetProvider(int af, int type, int protocol, 
		LPWSAPROTOCOL_INFOW pOrigProtInfo, LPWSAPROTOCOL_INFOW pProtInfo, 
		WsProvider **ppProv) {
		
	WsProvider		*pProv;
	int				Err;
	LPWSPSTARTUP	pfnStartup;
	WSPDATA			WSPData;
	WCHAR			sDllPath[MAX_PATH];
	int				Flags;
	DWORD			CatEntryId;
	WSPUPCALLTABLE  UCTbl;      // compiler 

	Err = Flags = 0;
	if (pOrigProtInfo) {
		Flags |= 1;
		__try {
			CatEntryId = pOrigProtInfo->dwCatalogEntryId;
			memcpy(pProtInfo , pOrigProtInfo, sizeof(*pProtInfo));
		}
		__except(EXCEPTION_EXECUTE_HANDLER) {
			Err = WSAEFAULT;
		}
	}

	if ( (! Err) &&
		(! (Err = PMFindProvider(af, type, protocol, CatEntryId, Flags, 
			pProtInfo, sDllPath)))) {

		if (pProv = FindProvider(&pProtInfo->ProviderId)) {
			pProv->cRefs++;
		} else {
			if (pProv = LocalAlloc(LPTR, sizeof(*pProv))) {
				pProv->pNext = s_pProviderList;
				pProv->cRefs = 1;
				pProv->Id = pProtInfo->ProviderId;

				if (pProv->hLibrary = LoadLibrary(sDllPath)) {
					pfnStartup = (LPWSPSTARTUP)GetProcAddress(pProv->hLibrary,
						TEXT("WSPStartup"));
					// compiler 
					memcpy(&UCTbl, &UpCalls, sizeof(WSPUPCALLTABLE));
					if (!pfnStartup) {
						Err = WSAEPROVIDERFAILEDINIT;
					} else if (Err = (*pfnStartup) (WINSOCK_VERSION, &WSPData,
						pProtInfo, UCTbl, &pProv->ProcTable)) {      // compiler 
						;
					} else if (WSPData.wVersion != WINSOCK_VERSION) {
						pProv->ProcTable.lpWSPCleanup(&Err);

						ASSERT(0 == Err);
						Err = WSAEINVALIDPROVIDER;
					} else {
						// Partial SUCCESS
						s_pProviderList = pProv;
						wcscpy(pProv->sDllPath, sDllPath);
					}

				} else
					Err = WSAEPROVIDERFAILEDINIT;

				if (Err) {
					if (pProv->hLibrary)
						CloseHandle(pProv->hLibrary);
					LocalFree(pProv);
					pProv = NULL;
				}
				
			} else
				Err = WSAENOBUFS;
		}	// else (pProv)

		CTEFreeLock(&s_ProviderListLock, 0);
	}
	
	if (! Err)
		*ppProv = pProv;
	
	return Err;
	
}	// GetProvider()


SOCKET WSAAPI WSASocket (
	IN int af,
	IN int type,
	IN int protocol,
	IN LPWSAPROTOCOL_INFOW lpProtocolInfo,
	IN GROUP g,
	IN DWORD dwFlags) {

	int					cRefs, Err;
	DWORD				Flags = 0;
	WSAPROTOCOL_INFO	ProtInfo, *pProtInfo;
	WsProvider			*pProv;
	SOCKET				hSock = INVALID_SOCKET;
	WsSocket			*pSock;
	
	DEBUGMSG(ZONE_FUNCTION || ZONE_SOCKET,
		 (TEXT("+WS2!WSASocket: %d, %d, %d ProtInfo %X %d %d\r\n"),
		  af, type, protocol, lpProtocolInfo, g, dwFlags ));

//	if (Err = EnterDll()) {
//		goto Exit;
//	}
	if (v_fProcessDetached) {
		Err = WSAENETDOWN;
	} else {
		CTEGetLock(&v_DllCS, 0);
		Err = Started();
		CTEFreeLock(&v_DllCS, 0);
	}
	
	if (Err)
		goto Exit;

	if (lpProtocolInfo)
		pProtInfo = lpProtocolInfo;
	else
		pProtInfo = &ProtInfo;

	if (! (Err = GetProvider(af, type, protocol, lpProtocolInfo, 
		&ProtInfo, &pProv))) {

		// now check the flags...
		if ((WSA_FLAG_MULTIPOINT_C_LEAF | WSA_FLAG_MULTIPOINT_C_ROOT)&dwFlags){

			if (! (XP1_SUPPORT_MULTIPOINT & ProtInfo.dwServiceFlags1)) {
				Err = WSAEINVAL;
			} else if ((WSA_FLAG_MULTIPOINT_C_ROOT & dwFlags) &&
				((WSA_FLAG_MULTIPOINT_C_LEAF & dwFlags) ||
				! (XP1_MULTIPOINT_CONTROL_PLANE & ProtInfo.dwServiceFlags1)))
				Err = WSAEINVAL;

			if (WSA_FLAG_MULTIPOINT_D_ROOT & dwFlags) {
				if (! (XP1_MULTIPOINT_DATA_PLANE & ProtInfo.dwServiceFlags1) ||
					(WSA_FLAG_MULTIPOINT_D_LEAF & dwFlags))
					Err = WSAEINVAL;	
			} else if (! (WSA_FLAG_MULTIPOINT_D_LEAF & dwFlags))
				Err = WSAEINVAL;
			
		} else if ((WSA_FLAG_MULTIPOINT_D_LEAF | WSA_FLAG_MULTIPOINT_D_ROOT) & 
			dwFlags) {
			Err = WSAEINVAL;
		}

		// ok this means that the provider is up...
		if ( (! Err) && (pSock = LocalAlloc(LPTR, sizeof(*pSock)))) {
			pSock->cRefs = 1;
			CTEGetLock(&v_DllCS, 0);
			hSock = NewSocketHandle(pSock);
			CTEFreeLock(&v_DllCS, 0);
			
			if (INVALID_SOCKET != hSock) {
				pSock->hWSPSock = pProv->ProcTable.lpWSPSocket(af, type, 
					protocol, pProtInfo, g, dwFlags, &Err);

				if (INVALID_SOCKET != pSock->hWSPSock) {
					pSock->pProvider = pProv;
					// should we copy from ProtInfo or pProtInfo?
					memcpy(&pSock->ProtInfo, &ProtInfo, sizeof(ProtInfo));

					// SUCCESS !!!

				} else {
					// oh oh, close and delete everything
					pSock->Flags |= WS_SOCK_FL_CLOSED;
					cRefs = DerefSocketHandle(pSock->hSock);
					hSock = INVALID_SOCKET;
					ASSERT(cRefs == 0);
				}
			} else {	// if (INVALID_SOCKET != hSock)
				LocalFree(pSock);
				DerefProvider(&pProtInfo->ProviderId, pProv);
				Err = WSAEMFILE;
			}
		} else {	// if (! Err && pSock = LocalAlloc...)
			DerefProvider(&pProtInfo->ProviderId, pProv);
			if (! Err)
    			Err = WSAENOBUFS;
		}
	}	// if (! (Err = GetProvider(...)))

Exit:
	if (Err) {
		ASSERT(INVALID_SOCKET == hSock);
		SetLastError(Err);
	}

	DEBUGMSG(ZONE_FUNCTION || ZONE_SOCKET,
		 (TEXT("+WS2!WSASocket: %d, %d, %d ProtInfo %X Ret %u Err %u\r\n"),
		  af, type, protocol, lpProtocolInfo, hSock, Err));

	return hSock;
}	// WSASocket()


SOCKET SOCKAPI socket(
    int af,
    int type,
    int protocol) {
    
    SOCKET hSock;

	// we may have to do some flag modifications...

	hSock = WSASocket(af, type, protocol, NULL, 0, 0);

	return hSock;

}   // socket()


int WSAAPI closesocket(
    IN SOCKET s) {

    WsSocket	*pSock;
    int			cRefs, Err = 0, Status = SOCKET_ERROR;


	if (v_fProcessDetached) {
		Err = WSAENETDOWN;
		goto Exit;
	}
		
	CTEGetLock(&v_DllCS, 0);
	if (Err = Started()) {
		CTEFreeLock(&v_DllCS, 0);
		goto Exit;
	}
	
	pSock = *_FindSocketHandle(s);
	
	if (pSock && !(pSock->Flags & WS_SOCK_FL_CLOSED)) {
		pSock->Flags |= WS_SOCK_FL_CLOSED;
		CTEFreeLock(&v_DllCS, 0);

		Status  = pSock->pProvider->ProcTable.lpWSPCloseSocket(pSock->hWSPSock,
			&Err);

		if (Status) {
			// this means the closesocket failed!
			ASSERT(SOCKET_ERROR == Status);
			pSock->Flags &= ~WS_SOCK_FL_CLOSED;
		} else {
			cRefs = DerefSocket(pSock);
			ASSERT(cRefs >= 0);
		}

	} else {
		CTEFreeLock(&v_DllCS, 0);
		Err = WSAENOTSOCK;
	}

Exit:
	if (Err) {
		SetLastError(Err);
	}
	return Status;

}	// closesocket()


SOCKET WSAAPI WSAAccept (
	SOCKET			s,
	struct sockaddr	*addr,
	LPINT			addrlen,
	LPCONDITIONPROC	lpfnCondition,
	DWORD_PTR		dwCallbackData) {

	int			Err, cRefs;
	WsSocket	*pSock, *pNewSock;
	SOCKET		hSock;

	if (! (Err = RefSocketHandle(s, &pSock))) {

		// lower layers need to verify addr, addrlen, and other parameters

		if (pNewSock = LocalAlloc(LPTR, sizeof(*pSock))) {
			pNewSock->cRefs = 1;
		
			// should increment provider ref before calling newsocket handle
			CTEGetLock(&s_ProviderListLock, 0);
			pSock->pProvider->cRefs++;
			CTEFreeLock(&s_ProviderListLock, 0);
			pNewSock->pProvider = pSock->pProvider;
			
			CTEGetLock(&v_DllCS, 0);
			hSock = NewSocketHandle(pNewSock);
			CTEFreeLock(&v_DllCS, 0);
			
			if (INVALID_SOCKET != hSock) {
				pNewSock->hWSPSock = pSock->pProvider->ProcTable.
					lpWSPAccept(pSock->hWSPSock, addr, addrlen, 
					lpfnCondition, dwCallbackData, &Err);

				if (INVALID_SOCKET != pNewSock->hWSPSock) {
					memcpy(&pNewSock->ProtInfo, &pSock->ProtInfo, 
						sizeof(pNewSock->ProtInfo));

					// SUCCESS !!!

				} else {
					if ((INVALID_SOCKET == pNewSock->hWSPSock) && ! Err) {
						ASSERT(0);
						Err = WSASYSCALLFAILURE;
					}
					// oh oh, close and delete everything
					pNewSock->Flags |= WS_SOCK_FL_CLOSED;
					cRefs = DerefSocketHandle(pNewSock->hSock);
					ASSERT(cRefs == 0);
				}
			} else {
				LocalFree(pSock);
				DerefProvider(&pSock->pProvider->Id, pSock->pProvider);
				Err = WSAEMFILE;
			}
		} else
			Err = WSA_NOT_ENOUGH_MEMORY;
			
		DerefSocket(pSock);
	} else
		Err = WSAENOTSOCK;

	if (Err) {
		SetLastError(Err);
		hSock = INVALID_SOCKET;
	}
	
	return hSock;

}	// WSAAccept()


SOCKET WSAAPI accept (
	SOCKET	s,
	struct sockaddr	*addr,
	int		*addrlen) {

	return WSAAccept(s, addr, addrlen, NULL, 0);

}	// accept()


BOOL WSAAPI WSAGetOverlappedResult (
	IN SOCKET s,
	IN LPWSAOVERLAPPED lpOverlapped,
	OUT LPDWORD lpcbTransfer,
	IN BOOL fWait,
	OUT LPDWORD lpdwFlags) {

	int			Err;
	WsSocket	*pSock;
	BOOL		Status;	// note status is a bool and used differently

	if (! lpOverlapped || ! lpcbTransfer || ! lpdwFlags) {
		Err = WSAEFAULT;
	} else if (! (Err = RefSocketHandle(s, &pSock))) {

		Status = pSock->pProvider->ProcTable.lpWSPGetOverlappedResult(
			pSock->hWSPSock, lpOverlapped, lpcbTransfer, fWait, 
			lpdwFlags, &Err);

		if (!Status && !Err) {
			ASSERT(0);
			Err = WSASYSCALLFAILURE;
		}

		DerefSocket(pSock);
	}

	if (Err) {
		SetLastError(Err);
		Status = FALSE;
	}
	
	return Status;

}	// WSAGetOverlappedResult()


int WPUCloseSocketHandle (
	SOCKET s,
	LPINT lpErrno) {

	SOCKET				hSock = INVALID_SOCKET;
	WsSocket			*pSock, **ppSock;
	int					Ret;


	CTEGetLock(&v_DllCS, 0);
	ppSock = _FindSocketHandle(s);
	if (pSock = *ppSock) {
		pSock->Flags |= WS_SOCK_FL_CLOSED;
		DerefSocketHandle(pSock->hSock);
		Ret = 0;
	} else {
		SetErrorCode(WSAENOTSOCK, lpErrno);
		Ret = SOCKET_ERROR;
	}
	CTEFreeLock(&v_DllCS, 0);

	return Ret;

}	// WPUCloseSocketHandle


SOCKET WPUCreateSocketHandle(
	DWORD dwCatalogEntryId,
	DWORD dwContext,
	LPINT lpErrno) {

	SOCKET				hSock = INVALID_SOCKET;
	WsSocket			*pSock;
	int					Err;
	WCHAR				sDllPath[MAX_PATH];
	WSAPROTOCOL_INFOW	ProtInfo;
	WsProvider			*pProv;
	LPWSPSTARTUP		pfnStartup;
	WSPDATA				WSPData;
    WSPUPCALLTABLE  UCTbl;      // compiler 

	if (pSock = LocalAlloc(LPTR, sizeof(*pSock))) {

		Err = PMFindProvider(0, 0, 0, dwCatalogEntryId, 1, 
			&ProtInfo, sDllPath);

		

		if (! Err) {
			if (pProv = FindProvider(&ProtInfo.ProviderId)) {
				pProv->cRefs++;
			} else {
				// ok so I was in rush... we should really reuse the code
				// in GetProvider...
				DEBUGMSG(ZONE_MISC, 
					(TEXT("WPUCreateSocketHandle: loading provider from WPUCreateSocketHandle\r\n")));
					
				if (pProv = LocalAlloc(LPTR, sizeof(*pProv))) {
					pProv->pNext = s_pProviderList;
					pProv->cRefs = 1;
					pProv->Id = ProtInfo.ProviderId;

					if (pProv->hLibrary = LoadLibrary(sDllPath)) {
						pfnStartup = (LPWSPSTARTUP)GetProcAddress(pProv->hLibrary,
							TEXT("WSPStartup"));
                        memcpy(&UCTbl, &UpCalls, sizeof(WSPUPCALLTABLE));      // compiler 
						if (!pfnStartup) {
							Err = WSAEPROVIDERFAILEDINIT;
						} else if (Err = (*pfnStartup) (WINSOCK_VERSION, &WSPData,
							&ProtInfo, UCTbl, &pProv->ProcTable)) {      // compiler 
							;
						} else if (WSPData.wVersion != WINSOCK_VERSION) {
							pProv->ProcTable.lpWSPCleanup(&Err);

							ASSERT(0 == Err);
							Err = WSAEINVALIDPROVIDER;
						} else {
							// Partial SUCCESS
							s_pProviderList = pProv;
							wcscpy(pProv->sDllPath, sDllPath);
						}

					} else
						Err = WSAEPROVIDERFAILEDINIT;

					if (Err) {
						if (pProv->hLibrary)
							CloseHandle(pProv->hLibrary);
						LocalFree(pProv);
						pProv = NULL;
					}
					
				} else
					Err = WSAENOBUFS;
			}	// else (pProv = FindProvider(...)

			CTEFreeLock(&s_ProviderListLock, 0);
		} 

		if (!Err) {
			pSock->cRefs = 1;
			CTEGetLock(&v_DllCS, 0);
			hSock = NewSocketHandle(pSock);
			CTEFreeLock(&v_DllCS, 0);
			if (INVALID_SOCKET != hSock) {
				// Success!
				
				pSock->pProvider = pProv;
				// should we copy from ProtInfo or pProtInfo?
				memcpy(&pSock->ProtInfo, &ProtInfo, sizeof(ProtInfo));
				pSock->hWSPSock = (SOCKET)dwContext;
			} else {
				LocalFree(pSock);
				DerefProvider(&ProtInfo.ProviderId, pProv);
				Err = WSAEMFILE;
			}
			
		} else {
			LocalFree(pSock);
		}
		
	} else {
		Err = WSAENOBUFS;
	}

	if (Err) {
		SetErrorCode(Err, lpErrno);
		hSock = INVALID_SOCKET;
	}
	
	return hSock;

}	// WPUCreateSocketHandle()


int WPUGetProviderPath(
	LPGUID lpProviderId,
	LPWSTR lpszProviderDllPath,
	LPINT lpProviderDllPathLen,
	LPINT lpErrno) {

	WsProvider		*pProv;
	int				Ret, Err = 0;
	int				cPathLen;
	GUID			myGuid;

	myGuid = *lpProviderId;		// so we don't crash holding CS
	if (pProv = FindProvider(&myGuid)) {
		__try {
			cPathLen = wcslen(pProv->sDllPath);
			if (cPathLen <= *lpProviderDllPathLen)
				wcscpy(lpszProviderDllPath, pProv->sDllPath);
			else
				Err = WSAEFAULT;
			*lpProviderDllPathLen = cPathLen;
		}
		
		__except (EXCEPTION_EXECUTE_HANDLER) {
			Err = WSAEFAULT;
		}
		
		CTEFreeLock(&s_ProviderListLock, 0);
	} else {
		CTEFreeLock(&s_ProviderListLock, 0);
		
		// we now need to query pm
		{
			WSAPROTOCOL_INFOW	ProtInfo;

			memset(&ProtInfo, 0, sizeof(ProtInfo));
			memcpy(&ProtInfo, lpProviderId, sizeof(*lpProviderId));

			Err = PMFindProvider(0, 0, 0, 0, 2, &ProtInfo, 
				lpszProviderDllPath);

			if (! Err) {
				__try {
					*lpProviderDllPathLen = wcslen(lpszProviderDllPath);
				}
				__except (EXCEPTION_EXECUTE_HANDLER) {
					Err = WSAEFAULT;
				}
			}
		}
	}
	

	if (Err) {
		SetErrorCode(Err, lpErrno);
		Ret = SOCKET_ERROR;
	} else {
		Ret = 0;
	}
		
	return Ret;

}	// WPUGetProviderPath()


int WPUQuerySocketHandleContext(
	SOCKET s,
	PDWORD_PTR lpContext,
	LPINT lpErrno) {

	WsSocket	*pSock;
	int			Err;

	CTEGetLock(&v_DllCS, 0);

	// no longer check if Started, when we're processing WSACleanup we make
	// calls to the providers which sometimes query the SocketHandle
	// furthermore this isn't a user fn anyway--it is called by providers
	// if (!(Err = Started())) {
	Err = 0;
	pSock = *_FindSocketHandle(s);
	if (pSock) {
		__try {
			*lpContext = (DWORD)pSock->hWSPSock;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			Err = WSAEFAULT;
		}
	} else
		Err = WSAENOTSOCK;
		
	CTEFreeLock(&v_DllCS, 0);

	if (Err) {
		SetErrorCode(Err, lpErrno);
		Err = SOCKET_ERROR;
	}

	return Err;

}	// WPUQuerySocketHandleContext()


int WPUCompleteOverlappedRequest(SOCKET s, LPWSAOVERLAPPED lpOverlapped, 
	DWORD dwError, DWORD cbTransferred, LPINT lpErrno) {
	WsSocket			*pSock, **ppSock;
	int					Ret;

	// since we don't yet support completion ports this routine doesn't
	// really do much other than set the values and signal the event
	if (lpOverlapped) {
        
		CTEGetLock(&v_DllCS, 0);
		ppSock = _FindSocketHandle(s);
		if (pSock = *ppSock) {
			CTEFreeLock(&v_DllCS, 0);
			lpOverlapped->InternalHigh = cbTransferred;
			lpOverlapped->Internal = WSS_OPERATION_IN_PROGRESS + 1;
			if (lpOverlapped->hEvent)
				SetEvent(lpOverlapped->hEvent);
			Ret = 0;
		} else {
			CTEFreeLock(&v_DllCS, 0);
			Ret = WSAEINVAL;
		}
	} else {
		Ret = WSA_INVALID_PARAMETER;
	}

	if (Ret) {
		SetErrorCode(Ret, lpErrno);
		Ret = SOCKET_ERROR;
	}

	return Ret;

}   // WPUCompleteOverlappedRequest


void CloseAllSockets() {
    WsSocket	*pSock;
    int			i, Err = 0, Status = SOCKET_ERROR;
	WsProvider	*pProv;
	LINGER		Linger;

	CTEGetLock(&v_DllCS, 0);
	if (v_fClosingAll) {
		goto Exit;
	}
	v_fClosingAll = TRUE;
	
	for (i = 0; i < SOCK_HASH_SIZE; i++) {

		while (pSock = v_apWsSocks[i]) {
			v_apWsSocks[i] = pSock->pNext;

			if (WS_SOCK_FL_CLOSED & pSock->Flags) {
				DEBUGMSG(ZONE_WARN, 
					(TEXT("CloseAllSockets: Socket %X already closed\r\n"),
					pSock));
				
				continue;
			}

			pProv = pSock->pProvider;
			pSock->Flags |= WS_SOCK_FL_CLOSED;
			
			if (! v_fProcessDetached) {
				if (pProv && pSock->hWSPSock) {
					// abortively close the sockets
					Linger.l_onoff  = 1;
					Linger.l_linger = 0;
					Status = pProv->ProcTable.lpWSPSetSockOpt(
						pSock->hWSPSock, SOL_SOCKET, SO_LINGER, (char *)&Linger, 
						sizeof(Linger), &Err);
					if (Status) {
						DEBUGMSG(ZONE_WARN, 
							(TEXT("CloseAllSockets: set Linger failed Sock %X Err: %d\r\n"),
							pSock, Err));
					}

					Status = pProv->ProcTable.lpWSPCloseSocket(
						pSock->hWSPSock, &Err);
					if (Status) {
						DEBUGMSG(ZONE_WARN, 
							(TEXT("CloseAllSockets: closesocket failed Sock %X Err: %d\r\n"),
							pSock, Err));
					}
				}
			}	// ! v_fProcessDetached

			--pSock->cRefs;
			ASSERT(pSock->cRefs >= 0);
			if (pSock->cRefs <= 0) {
				pSock->pProvider = NULL;
				LocalFree(pSock);
				if (pProv) {
					CTEFreeLock(&v_DllCS, 0);
					DerefProvider(&pProv->Id, pProv);
					CTEGetLock(&v_DllCS, 0);
				}
			}

		}

	}

	v_fClosingAll = FALSE;
Exit:
	CTEFreeLock(&v_DllCS, 0);

}


