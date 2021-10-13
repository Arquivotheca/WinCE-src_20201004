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

rnr.c

registration and name resolution winsock functions

FILE HISTORY:
	OmarM     04-Oct-2000

*/


#include "winsock2p.h"
#include <cxport.h>

extern int v_fProcessDetached;

typedef struct NsProvider {
	struct NsProvider	*pNext;
	int					cRefs;
	HINSTANCE			hLibrary;
	GUID				Id;
	NSP_ROUTINE			ProcTable;
} NsProvider;


#define LOOKUP_SVC_BEGIN_FAILED	0x0001
#define LOOKUP_SVC_ERROR		0x0002

typedef struct LookupProvList {
	struct LookupProvList	*pNext;
	NsProvider				*pNsProv;
	int						Flags;
	HANDLE					hLookup;
} LookupProvList;

#define NAME_SPACE_ID_LEN	32

// this need to be the same as that define in pm namespc.c
typedef struct NameSpaces {
	DWORD				Flags;	
	WSANAMESPACE_INFOW	Info;	// if not used remove--we may only need GUID
	WCHAR				LibPath[MAX_PATH];
	WCHAR				szId[NAME_SPACE_ID_LEN];	// if not used remove
} NameSpaces;


typedef struct NsLookup {
	struct NsLookup	*pNext;
	LookupProvList	*pNsProvList;
	CTELock			Lock;
	int				cRefs;
} NsLookup;


DEFINE_LOCK_STRUCTURE(s_NsProvListLock);
DEFINE_LOCK_STRUCTURE(s_NsLookupsLock);

NsProvider	*s_pNsProviders;
NsLookup	*s_pNsLookups;



NsProvider *FindNsProvider(GUID *pId) {
	NsProvider	*pNsProv;

	for (pNsProv = s_pNsProviders; pNsProv; pNsProv = pNsProv->pNext)
		if (0 == memcmp(&pNsProv->Id, pId, sizeof(GUID)))
			break;

	return pNsProv;

}	// FindNsProvider()


void DeleteNsProvider(NsProvider *pNsProv) {
	int Err;

	ASSERT(0 == pNsProv->cRefs);

	// this could supposedly fail...
	if (! v_fProcessDetached) {
		Err = pNsProv->ProcTable.NSPCleanup(&pNsProv->Id);
		if (Err) {
			Err = GetLastError();
		}
		ASSERT(NO_ERROR == Err);
	}

	if (pNsProv->hLibrary && (! v_fProcessDetached))
		FreeLibrary(pNsProv->hLibrary);
	
	LocalFree(pNsProv);

}	// DeleteNsProvider()


void DerefNsProvider(NsProvider *pNsProv) {
	int		Err;
	NsProvider	**ppNsProv, *pCurProv;

	CTEGetLock(&v_DllCS, 0);
	if (v_fProcessDetached)
		Err = WSAENETDOWN;
	else
		Err = Started();
	CTEFreeLock(&v_DllCS, 0);

	// for efficiency purposes we only unload the providers
	// if we've been cleaned up.
	pCurProv = NULL;
	CTEGetLock(&s_NsProvListLock, 0);
	if (0 == --(pNsProv->cRefs)) {
		if (Err) {
			// we're going away, so we need to get rid of the provider
			ppNsProv = &s_pNsProviders;
			while (pCurProv = *ppNsProv) {
				if (pCurProv == pNsProv) {
					break;
				}
				ppNsProv = &pCurProv->pNext;
			}	// while ()
			ASSERT(pCurProv == pNsProv);
			if (pCurProv) {
				*ppNsProv = pCurProv->pNext;
				// we'll delete below, now that it is out of the list
			}
		}
	}
	ASSERT(0 <= pNsProv->cRefs);
	
	CTEFreeLock(&s_NsProvListLock, 0);

	if (pCurProv == pNsProv) {
		DeleteNsProvider(pNsProv);
	}
	
}	// DerefNsProvider()


void FreeNsProviders() {
	NsProvider	*pNsProv, **ppNsProv;
	
	CTEGetLock(&s_NsProvListLock, 0);
	ppNsProv = &s_pNsProviders;
	while (pNsProv = *ppNsProv) {
		if (pNsProv->cRefs) {
			ppNsProv = &(pNsProv->pNext);
		} else {
			*ppNsProv = pNsProv->pNext;
			CTEFreeLock(&s_NsProvListLock, 0);
			DeleteNsProvider(pNsProv);
			CTEGetLock(&s_NsProvListLock, 0);
		}
	}
	CTEFreeLock(&s_NsProvListLock, 0);

}	// FreeNsProviders()


int LoadNsProviders(NameSpaces *pNs, int cNs, NsLookup *pLookup) {

	NsProvider		*pNsProv;
	int				Err, i, cLoaded;
	LPNSPSTARTUP	pfnStartup;
	LookupProvList	*pLookupProvList, **ppProvList;
	
	Err = cLoaded = 0;

	// this is so that we query the providers in order
	// so the pPrivNext of the lookup should be in the order that the
	// providers were given to us by PM
	ppProvList = &pLookup->pNsProvList;
	
	CTEGetLock(&s_NsProvListLock, 0);
	for (i = 0; i < cNs; i++, pNs++) {
		if (pLookupProvList = LocalAlloc(LPTR, sizeof(*pLookupProvList))) {
			if (pNsProv = FindNsProvider(&pNs->Info.NSProviderId)) {
				pNsProv->cRefs++;
			} else {
				if (pNsProv = LocalAlloc(LPTR, sizeof(*pNsProv))) {
					pNsProv->pNext = s_pNsProviders;
					pNsProv->cRefs = 1;
					pNsProv->Id = pNs->Info.NSProviderId;
					pNsProv->ProcTable.cbSize = sizeof(pNsProv->ProcTable);

					if (pNsProv->hLibrary = LoadLibrary(pNs->LibPath)) {
						pfnStartup = (LPNSPSTARTUP)GetProcAddress(
							pNsProv->hLibrary, TEXT("NSPStartup"));
						
						if (!pfnStartup) {
							Err = WSAEPROVIDERFAILEDINIT;
						} else if (Err = (*pfnStartup)(&pNs->Info.NSProviderId, 
							&pNsProv->ProcTable)) {

							CloseHandle(pNsProv->hLibrary);
							pNsProv->hLibrary = NULL;	// just to be safe
						} else {	// should we check version info here?
							// Partial SUCCESS
							s_pNsProviders = pNsProv;
							Err = 0;
						}
					} else
						Err = WSAEPROVIDERFAILEDINIT;

					if (Err) {
						LocalFree(pNsProv);
						pNsProv = NULL;
					}
				} else {
					Err = WSA_NOT_ENOUGH_MEMORY;
				}
			}	// else FindNsProvider()

			if (pNsProv) {
				pLookupProvList->pNsProv = pNsProv;
				*ppProvList = pLookupProvList;
				ppProvList = &pLookupProvList->pNext;
				cLoaded++;
			} else {
				// couldn't find a provider or provider failed
				LocalFree(pLookupProvList);
			}	
		} else {	// if (pLookupProvList = LocalAlloc...)
			Err = WSA_NOT_ENOUGH_MEMORY;	
		}
	}	// for (i < cNs)
	
	CTEFreeLock(&s_NsProvListLock, 0);

	return cLoaded;

}	// LoadNsProviders()


int CallNsProviders(NsLookup *pNsLookup, WSAQUERYSETW *pRest, 
	WSASERVICECLASSINFOW *pSvcClass, DWORD Flags, WSAESETSERVICEOP essOperation, BOOL fWSSetServiceCaller) {

	LookupProvList	*pLkpProvList;
	NsProvider		*pNsProv;
	int				Err=0, cOK=0;

	cOK = 0;
	pLkpProvList = pNsLookup->pNsProvList;
	while (pLkpProvList) {
		pNsProv = pLkpProvList->pNsProv;
		ASSERT(pNsProv);

		if (fWSSetServiceCaller) {
			// WSASetService
			Err = pNsProv->ProcTable.NSPSetService(&pNsProv->Id, pSvcClass, pRest, 
				  essOperation,Flags);
		}
		else {
			// WSALookupServiceBegin
			Err = pNsProv->ProcTable.NSPLookupServiceBegin(&pNsProv->Id, pRest, 
				pSvcClass, Flags, &pLkpProvList->hLookup);

			if (Err)
				pLkpProvList->Flags |= LOOKUP_SVC_BEGIN_FAILED;
			else
				cOK++;
		}

		pLkpProvList = pLkpProvList->pNext;
	}

	if (cOK)
		Err = 0;

	return Err;
}	// CallNsProviders()


//
// Called from error paths in WSASetOrLookupBeginService
//
void FreeNsLookup(NsLookup * pLookup)
{
    LookupProvList	*pProvList;
    LookupProvList	*pProvList2;

    pProvList = pLookup->pNsProvList;
    while (pProvList) {
         pProvList2 = pProvList;
         
         DerefNsProvider(pProvList2->pNsProv);
         
         pProvList = pProvList->pNext;
         LocalFree(pProvList2);
    }
    LocalFree(pLookup);
}


// Since the code paths are nearly identical for WSASetService and WSALookupServiceBegin,
// have them go through one function.

INT WSAAPI WSASetOrLookupBeginService(
	LPWSAQUERYSET pRestrictions, 
	WSAESETSERVICEOP essOperation, 
	DWORD dwControlFlags,
	OUT	LPHANDLE phLookup,
	BOOL fWSSetServiceCaller) {

	int			Err, cNs, cLoaded, fNewBuf;
	int			cLen, cBuf;
	NsLookup	*pLookup;
	NameSpaces	*pNs;

	pNs = NULL;
	// check if we were successfully started up
	CTEGetLock(&v_DllCS, 0);
	if (!(Err = Started())) {
		CTEFreeLock(&v_DllCS, 0);

		if (pRestrictions && (fWSSetServiceCaller || phLookup)) {
			// let us find the providers...
			cBuf = sizeof(*pNs) * 10;
			do {
				fNewBuf = FALSE;
				if (pNs = LocalAlloc(LPTR, cBuf)) {

					cLen = cBuf;
					cNs = PMFindNameSpaces(pRestrictions, pNs, &cLen, &Err);
					if (SOCKET_ERROR == cNs && 
						WSAEFAULT == Err && cLen > cBuf) {

						LocalFree(pNs);
						cBuf = cLen;
						fNewBuf = TRUE;
						Err = 0;
					}
				} else
					Err = WSA_NOT_ENOUGH_MEMORY;
			} while (fNewBuf);
		} else {
			Err = WSAEFAULT;
		}
	} else {
		CTEFreeLock(&v_DllCS, 0);
	}
	if (Err)
		cNs = SOCKET_ERROR;

	if (SOCKET_ERROR != cNs) {
		if (pLookup = LocalAlloc(LPTR, sizeof(*pLookup))) {
			if (cNs && (cLoaded = LoadNsProviders(pNs, cNs, pLookup))) {
				// GetServiceClassInfo here? NT has some code to do it,
				// but it is not compiled--it is if 0'ed out

				if(SOCKET_ERROR == CallNsProviders(pLookup, pRestrictions, 
					NULL, dwControlFlags,essOperation,fWSSetServiceCaller))
					Err = GetLastError();
					
				if (Err || fWSSetServiceCaller) {
					// oh, oh we failed. (or we're a SetService, which won't 
					// require the pointer later anyway)
					// probably should delete the Lookup stuff here
					FreeNsLookup(pLookup);
				} else {
					__try {
						*phLookup = (HANDLE)pLookup;
					}
					__except (EXCEPTION_EXECUTE_HANDLER) {
						Err = WSAEFAULT;
					}

					if (Err) {
						FreeNsLookup(pLookup);
					} else {
						CTEInitLock(&pLookup->Lock);
						pLookup->cRefs = 1;

						// insert in list
						CTEGetLock(&s_NsLookupsLock, 0);
						pLookup->pNext = s_pNsLookups;
						s_pNsLookups = pLookup;
						CTEFreeLock(&s_NsLookupsLock, 0);
					}
				}
			} else {
				FreeNsLookup(pLookup);
				Err = WSASERVICE_NOT_FOUND;
			}
		} else
			Err = WSA_NOT_ENOUGH_MEMORY;
	}

	if (pNs)
		LocalFree(pNs);

	if (Err) {
		SetLastError(Err);
		Err = SOCKET_ERROR;
	}
	
	return Err;
	
}


INT WSAAPI WSASetService(
	LPWSAQUERYSET lpqsRegInfo, 
	WSAESETSERVICEOP essOperation, 
	DWORD dwControlFlags
	) {
	return WSASetOrLookupBeginService(lpqsRegInfo,essOperation,dwControlFlags,NULL,TRUE);
}

INT WSAAPI WSALookupServiceBegin (
	IN	LPWSAQUERYSETW	pRestrictions,
	IN	DWORD			dwControlFlags,
	OUT	LPHANDLE		phLookup) {
	return WSASetOrLookupBeginService(pRestrictions,0,dwControlFlags,phLookup,FALSE);
}

// Find the lookup and remove from global list or add ref
NsLookup *FindNsLookup(HANDLE hLookup, uint fDelete) {
	NsLookup	**ppLookup, *pLookup;

	CTEGetLock(&s_NsLookupsLock, 0);
	ppLookup = &s_pNsLookups;
	while (pLookup = *ppLookup) {
		if ((HANDLE)pLookup == hLookup) {
			if (fDelete) {
				*ppLookup = pLookup->pNext;
			} else {
				CTEGetLock(&pLookup->Lock, 0);
				pLookup->cRefs++;
				CTEFreeLock(&pLookup->Lock, 0);
			}
			
			break;
		}
		ppLookup = &pLookup->pNext;
	}
	CTEFreeLock(&s_NsLookupsLock, 0);
	
	return pLookup;

}	// FindNsLookup()


void DerefLookup(NsLookup *pLookup) {
	LookupProvList	*pNsProvList;

	CTEGetLock(&pLookup->Lock, 0);
	ASSERT(0 < pLookup->cRefs);
	if (0 == --pLookup->cRefs) {
		// ok time to cleanup
		pNsProvList = pLookup->pNsProvList;
	} else
		pNsProvList = NULL;
		
	CTEFreeLock(&pLookup->Lock, 0);

	if (pNsProvList) {
		// we only set this if our ref count has reached 0

		CTEDeleteLock(&pLookup->Lock);
		
		FreeNsLookup(pLookup);
	}	// if (pNsProvList)

}	// DerefLookup()


void FreeLookups() {
	NsLookup	*pLookup;

	CTEGetLock(&s_NsLookupsLock, 0);
	while (pLookup = s_pNsLookups) {
		s_pNsLookups = pLookup->pNext;
		CTEFreeLock(&s_NsLookupsLock, 0);
		DerefLookup(pLookup);
		CTEGetLock(&s_NsLookupsLock, 0);
	}
	CTEFreeLock(&s_NsLookupsLock, 0);

}	// FreeLookups()


INT WSAAPI WSALookupServiceNext (
	IN     HANDLE           hLookup,
	IN     DWORD            dwControlFlags,
	IN OUT LPDWORD          lpdwBufferLength,
	OUT    LPWSAQUERYSETW   lpqsResults) {

	int				Err = 0;
	NsLookup		*pLookup;
	LookupProvList	*pLkpProv;

	CTEGetLock(&v_DllCS, 0);
	Err = Started();
	CTEFreeLock(&v_DllCS, 0);

	//
    // Verify that pointers are valid
    //

    if ( IsBadReadPtr(lpdwBufferLength, sizeof(*lpdwBufferLength) ) ||
         ( *lpdwBufferLength != 0  &&
           IsBadWritePtr(lpqsResults, *lpdwBufferLength) ) )
    {
        Err = WSAEFAULT;
    }
	
	if (Err)
		goto Exit;

	if (pLookup = FindNsLookup(hLookup, FALSE)) {
		pLkpProv = pLookup->pNsProvList; 
		Err = WSA_E_NO_MORE;
		for ( ;pLkpProv; pLkpProv = pLkpProv->pNext) {
			if (! (LOOKUP_SVC_BEGIN_FAILED & pLkpProv->Flags) &&
				! (LOOKUP_SVC_ERROR & pLkpProv->Flags)) {

				Err = pLkpProv->pNsProv->ProcTable.NSPLookupServiceNext(
					pLkpProv->hLookup, dwControlFlags, 
					lpdwBufferLength, lpqsResults);

				if (Err) {
					Err = GetLastError();
					if (WSAEFAULT == Err)
						break;
					if (WSA_E_NO_MORE != Err)
					    pLkpProv->Flags |= LOOKUP_SVC_ERROR;
				} else {
					break;
				}
			}
		}	// for (pLkpProv)
		
		DerefLookup(pLookup);

	} else
		Err = WSA_INVALID_HANDLE;
	
Exit:
	if (Err) {
		SetLastError(Err);
		Err = SOCKET_ERROR;
	}
	return Err;

}	// WSALookupServiceNext()


INT WSAAPI WSALookupServiceEnd (
	IN HANDLE  hLookup) {

	int				Err = 0;
	NsLookup		*pLookup;
	LookupProvList	*pLkpProv;

	CTEGetLock(&v_DllCS, 0);
	Err = Started();
	CTEFreeLock(&v_DllCS, 0);
	
	if (Err)
		goto Exit;

	if (pLookup = FindNsLookup(hLookup, TRUE)) {
		pLkpProv = pLookup->pNsProvList; 
		for ( ;pLkpProv; pLkpProv = pLkpProv->pNext) {
			if (! (LOOKUP_SVC_BEGIN_FAILED & pLkpProv->Flags)) {

				// there is not much to do if it fails
				pLkpProv->pNsProv->ProcTable.NSPLookupServiceEnd(
					pLkpProv->hLookup);

			}
		}	// for (pLkpProv)

		DerefLookup(pLookup);

	} else
		Err = WSA_INVALID_HANDLE;

Exit:
	if (Err) {
		SetLastError(Err);
		Err = SOCKET_ERROR;
	}
	
	return Err;

}	// WSALookupServiceEnd()


INT WSAAPI WSANSPIoctl (
	IN  HANDLE           hLookup,
	IN  DWORD            dwControlCode,
	IN  LPVOID           lpvInBuffer,
	IN  DWORD            cbInBuffer,
	OUT LPVOID           lpvOutBuffer,
	IN  DWORD            cbOutBuffer,
	OUT LPDWORD          lpcbBytesReturned,
	IN  LPWSACOMPLETION  lpCompletion) {

	NsLookup		*pLookup = NULL;
	int			ErrorCode;
    
	CTEGetLock(&v_DllCS, 0);
	ErrorCode = Started();
	CTEFreeLock(&v_DllCS, 0);
	
	if (ERROR_SUCCESS != ErrorCode) {
		goto Error;
	}

	//
	// Verify that the completion structure is readable if given.
	//
	if ((lpCompletion) && IsBadReadPtr(lpCompletion, sizeof(*lpCompletion))) {
		ErrorCode = WSAEINVAL;
		goto Error;
	}

	//
	// Verify lpcbBytesReturned.
	//
	if ((lpcbBytesReturned == NULL) ||
		IsBadWritePtr(lpcbBytesReturned, sizeof(*lpcbBytesReturned))) {
		ErrorCode = WSAEINVAL;
		goto Error;
	}

	//
	// Verify that the query handle is valid.
	//
	if (!hLookup) {
		ErrorCode = WSA_INVALID_HANDLE;
		goto Error;
	}
	
	if (pLookup = FindNsLookup(hLookup, FALSE)) {
		//
		// Make sure there is at least one and only one namespace
		// in the query that supports this operation.
		//
		unsigned int	numProviders = 0;
	  	LookupProvList	*provider, *providerIoctl = NULL;
		
		for (provider = pLookup->pNsProvList ;provider; provider = provider->pNext) {
			if (NULL != provider->pNsProv->ProcTable.NSPIoctl) {
				if(++numProviders > 1)
					break;
					
				providerIoctl = provider;
			}
		}
		
		if (numProviders > 1) {
			ErrorCode = WSAEINVAL;
			goto Error;
		}

		if (NULL == providerIoctl) {
			ErrorCode = WSAEOPNOTSUPP;
			goto Error;
		}
	
		//
		// Perform the IOCTL.
		//
		ErrorCode = providerIoctl->pNsProv->ProcTable.NSPIoctl(
										   providerIoctl->hLookup,
										   dwControlCode, lpvInBuffer, cbInBuffer,
										   lpvOutBuffer, cbOutBuffer, lpcbBytesReturned,
										   lpCompletion, NULL);
		
		DerefLookup(pLookup);
		return ErrorCode;
	} else {
		ErrorCode = WSA_INVALID_HANDLE;
	}

Error:
	if (pLookup)
		DerefLookup(pLookup);
	
	ASSERT(ERROR_SUCCESS != ErrorCode);
	SetLastError(ErrorCode);
	return SOCKET_ERROR;
	
}	// WSANSPIoctl()



