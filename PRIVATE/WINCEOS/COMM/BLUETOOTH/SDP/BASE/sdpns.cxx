//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
// 
//      Bluetooth SDP Name Space Layer
// 
// 
// Module Name:
// 
//      sdpns.cxx
// 
// Abstract:
// 
//      This file implements SDP Name Space handling and inquiry.
// 
// 
//------------------------------------------------------------------------------



#include "common.h"
#include <winsock2.h>
#include <svsutil.hxx>
#include <bt_debug.h>

// declarations and global data
static int bthns_ServiceSearch_Out(void *pCallContext, unsigned long status, unsigned short cReturnedHandles, unsigned long *pHandles);
static int bthns_AttributeSearch_Out(void *pCallContext, unsigned long status, unsigned char *pOutBuf, unsigned long cOutBuf);
static int bthns_ServiceAttributeSearch_Out(void *pCallContext, unsigned long status, unsigned char *pOutBuf, unsigned long cOutBuf);
static int bthns_StackEvent(void *pUserContext, int iEvent, void *pEventContext);
static int bthns_Connect_Out(void *pCallContext, unsigned long status, unsigned short cid);
static int bthns_Disconnect_Out(void *pCallContext, unsigned long status);


// On searching, number of milliseconds to wait for a response before canceling search.
#define  SPD_SEARCH_DEFAULT_TIMEOUT           30000
static DWORD         g_dwSearchTimeout;
static const TCHAR   g_szSDPBaseKey[] = TEXT("Software\\Microsoft\\Bluetooth\\SDP");
static const TCHAR   g_szTimeoutVal[] = TEXT("SearchTimeOut");

struct Call {
	Call          *pNext;
	HANDLE        hEvent;

	// Return values to SDP calls.  Note: Call does NOT free buffers
	int	           iResult;
	unsigned short cid;
	unsigned char  *pClientBuf;       // client output buffer
	unsigned long  cClientBuf;        // sizeof client output buffer.
};

#define INQUIRY_PENDING		1
#define INQUIRY_INPROGRESS	2
#define INQUIRY_COMPLETED	3

struct BthNsHandle {
	// BthNsHandle internals
	BthNsHandle      *pNext;
	Call           *pCall;

	// BthNsHandle's initial values.
	DWORD                  dwFlags;
	PBTHNS_RESTRICTIONBLOB   pResBlob;
	BD_ADDR                b;

	// ServiceClassId and Range are only used when lpServiceClassId != NULL and no RestrcitionBlob set in WSALookupServiceBegin()
	GUID                   ServiceClassId;

	unsigned int           fLocal             : 1; // search to be done on local device
	unsigned int           fNetSearchComplete : 1; // has search been sent across the wire
	unsigned int           fNoMoreData        : 1; // when all data has been sent up to Winsock level.
	unsigned int           fXPCompatMode      : 1; // TRUE if we use XP bluetooth data structures, FALSE if we use legacy WinCE structs.
	unsigned int           fInquiryStatus     : 2; // PENDING/INPROGRESS/DONE
	unsigned char          *pClientBuf;            // client output buffer
	unsigned long          cClientBuf;             // sizeof client output buffer.

	unsigned long          LAP;
	unsigned int           cDuration;
	unsigned int           cMaxResp;

	// results from a device inquiry.
	BthInquiryResult       *pInquiryResults;
	unsigned int           cDevices;
	unsigned int           iDeviceIterator; // when iterating through device list in BthNsLookupServiceNext
};

enum BTHNS_STAGE {
	JustCreated			= 0,
	Initializing,
	Running,
	ShuttingDown
};

class BthNs : public SVSSynch, public SVSRefObj {
public:
	FixedMemDescr	       *pfmdBthNsHandles;
	FixedMemDescr	       *pfmdNSCAlls;
	
	BthNsHandle              *pBthNsHandles;
	Call                   *pCalls;
	

	SDP_INTERFACE          sdp_if;
	HANDLE                 hSDP;
	BTHNS_STAGE            eStage;

	void ReInit(void) {
		pfmdBthNsHandles = NULL;
		pfmdNSCAlls   = NULL;
		pBthNsHandles    = NULL;
		pCalls         = NULL;

		hSDP      = NULL;
		memset(&sdp_if,0,sizeof(sdp_if));

		eStage   = JustCreated;
	}

	BthNs(void) {
		ReInit();
	}
};

BthNs *gpBthNS = NULL;


static BOOL DeleteBthNsHandle(BthNsHandle *pBthNsHandle)  {
	SVSUTIL_ASSERT(gpBthNS->IsLocked());

	BOOL fRet = TRUE;
	
	if (pBthNsHandle == gpBthNS->pBthNsHandles)
		gpBthNS->pBthNsHandles = pBthNsHandle->pNext;
	else {
		BthNsHandle *pParent = gpBthNS->pBthNsHandles;
		while (pParent && (pParent->pNext != pBthNsHandle))
			pParent = pParent->pNext;

		if (pParent)
			pParent->pNext = pBthNsHandle->pNext;
		else
			fRet = FALSE;
	}

	if (fRet) {
		if (pBthNsHandle->pClientBuf)
			ExFreePool(pBthNsHandle->pClientBuf);

		if (pBthNsHandle->pResBlob)
			g_funcFree(pBthNsHandle->pResBlob,g_pvAllocData);

		if (pBthNsHandle->pInquiryResults)
			g_funcFree(pBthNsHandle->pInquiryResults,g_pvAllocData);

		svsutil_FreeFixed(pBthNsHandle, gpBthNS->pfmdBthNsHandles);
	}
	return fRet;
}

static void DeleteCall(Call *pCall) {
	BOOL fFound = TRUE;

	if (pCall == gpBthNS->pCalls)
		gpBthNS->pCalls = pCall->pNext;
	else {
		Call *pParent = gpBthNS->pCalls;
		while (pParent && (pParent->pNext != pCall))
			pParent = pParent->pNext;

		if (pParent)
			pParent->pNext = pCall->pNext;
		else
			fFound = FALSE;
	}

	if (fFound) {
		CloseHandle(pCall->hEvent);
		if (pCall->pClientBuf)
			g_funcFree(pCall->pClientBuf,g_pvAllocData);

		svsutil_FreeFixed(pCall, gpBthNS->pfmdNSCAlls);
	}
}

static BOOL ConvertStringToBdAddr(WCHAR *szAddress, BD_ADDR *pb) {
	DWORD          i;
	LPWSTR         p = szAddress;
	
	for (i=0; i<6 && *p; i++) {
		if (i==0 && *p==L'(') {
			p++;
		}
		
		while (*p && ((*p>=L'0' && *p<=L'9') ||
		      (*p>=L'a' && *p<=L'f') ||
		      (*p>=L'A' && *p<=L'F')))
		{
			((PUCHAR)pb)[5-i] *= 16;

			if (*p>=L'0' && *p<=L'9') {
				((PUCHAR)pb)[5-i] += *p - L'0';
			} 
			else if (*p>=L'a' && *p<=L'f') {
				((PUCHAR)pb)[5-i] += *p + 10 - L'a';
			}
			else {
				((PUCHAR)pb)[5-i] += *p + 10 - L'A';
			}
			p++;
		}

		if (*p==L'(') {
			// This can occur at the start of the string
			// But we're not at the start
			return FALSE;
		} 
		else if (*p==L')') {
			// This can occur at the end only
			if (i<5) {
				return FALSE;
			}
			p++;
		} 
		else if (*p==L'.' || *p==L':') {
			p++;
		} 
		else if (!*p) {
			if (i<5) {
				return FALSE;
			}
		} 
		else {
			return WSAEINVAL;
		}
	}
	return TRUE;
}

static BthNsHandle * AllocBthNsHandle(LPWSAQUERYSET pQuerySet, DWORD dwFlags, int *piError)  {
	SVSUTIL_ASSERT(gpBthNS->IsLocked());
	PBTHNS_RESTRICTIONBLOB pResBlob = (PBTHNS_RESTRICTIONBLOB) ( pQuerySet->lpBlob ? pQuerySet->lpBlob->pBlobData : NULL);
	BOOL fSucces = FALSE;

	BthNsHandle *pBthNsHandle = (BthNsHandle *)svsutil_GetFixed(gpBthNS->pfmdBthNsHandles);
	if (!pBthNsHandle)
		goto done;

	memset(pBthNsHandle,0,sizeof(*pBthNsHandle));


	pBthNsHandle->dwFlags = dwFlags;

	// Get the remote address
	if (dwFlags & LUP_RES_SERVICE) {
		pBthNsHandle->fLocal = TRUE;
	} 
	else if (! (dwFlags & LUP_CONTAINERS)) {
		if (pQuerySet->lpcsaBuffer) {
			// Legacy CE mode, in which lpcsaBuffer is passed.
			SOCKADDR_BTH *pSockBT = (SOCKADDR_BTH *) pQuerySet->lpcsaBuffer->RemoteAddr.lpSockaddr;
#if defined (DEBUG) || defined (_DEBUG)
			if (! __SDP_IS_ALIGNED(pSockBT, 8))
				DebugOut(DEBUG_WARN, L"SDP: Unaligned remote address in structure - FIX THE APP!\n");
#endif
			memcpy(&pBthNsHandle->b,(void *)&pSockBT->btAddr,sizeof(pBthNsHandle->b));
		}
		else {	
			// On Windows XP, address is passed as a readable string in lpszContext.  Implement for compat.
			if (! ConvertStringToBdAddr(pQuerySet->lpszContext,&pBthNsHandle->b))
				goto done;

			pBthNsHandle->fXPCompatMode = TRUE;
		}
	}

	// figure out what to query on remote device
	if (! (dwFlags & LUP_CONTAINERS)) {
		if (pResBlob) {
			if (NULL == (pBthNsHandle->pResBlob = (PBTHNS_RESTRICTIONBLOB) g_funcAlloc(pQuerySet->lpBlob->cbSize,g_pvAllocData))) 
				goto done;

			memcpy(pBthNsHandle->pResBlob,pResBlob,pQuerySet->lpBlob->cbSize);
		}
		else {
			memcpy(&pBthNsHandle->ServiceClassId,pQuerySet->lpServiceClassId,sizeof(GUID));
		}
	}

	pBthNsHandle->pNext    = gpBthNS->pBthNsHandles;
	gpBthNS->pBthNsHandles = pBthNsHandle;
	fSucces              = TRUE;
done:
	if (!fSucces) {
		if (pBthNsHandle) {
			SVSUTIL_ASSERT(pBthNsHandle != gpBthNS->pBthNsHandles);
			SVSUTIL_ASSERT(!pBthNsHandle->pResBlob);

			svsutil_FreeFixed(pBthNsHandle, gpBthNS->pfmdBthNsHandles);
			pBthNsHandle = NULL;
			*piError = WSAEINVAL;
		}
		else
			*piError = WSA_NOT_ENOUGH_MEMORY;
	}

	return pBthNsHandle;
}

static Call * AllocCall(BthNsHandle *pBthNsHandle) {
	SVSUTIL_ASSERT(gpBthNS->IsLocked());

	Call *pCall = (Call *)svsutil_GetFixed(gpBthNS->pfmdNSCAlls);
	if (!pCall)
		return NULL;

	memset(pCall,0,sizeof(*pCall));

	if (NULL == (pCall->hEvent = CreateEvent(NULL,FALSE,FALSE,NULL))) {
		svsutil_FreeFixed(pCall, gpBthNS->pfmdNSCAlls);
	}

	pCall->pNext      = gpBthNS->pCalls;
	gpBthNS->pCalls   = pCall;
	pBthNsHandle->pCall = pCall;

	return pCall;
}


static BthNsHandle * FindBthNsHandle (BthNsHandle *pBthNsHandle) {
	BthNsHandle *pTrav = gpBthNS->pBthNsHandles;
	while (pTrav && pBthNsHandle != pTrav)
		pTrav = pTrav->pNext;

	return pTrav;
}

static Call * FindCall (Call *pCall) {
	Call *pTrav = gpBthNS->pCalls;
	while (pTrav && pCall != pTrav)
		pTrav = pTrav->pNext;

	return pTrav;
}


//
// Driver Interface functions
//
int bthns_InitializeOnce(void) {
	IFDBG(DebugOut(DEBUG_SDP_INIT | DEBUG_SDP_TRACE,L"bthns_InitializeOnce()\r\n"));

	if (gpBthNS) {
		IFDBG(DebugOut(DEBUG_ERROR,L"bthns_InitializeOnce:: ERROR_ALREADY_EXISTS\r\n"));
		return ERROR_ALREADY_EXISTS;
	}

	gpBthNS = new BthNs;
	if (!gpBthNS) {
		IFDBG(DebugOut(DEBUG_ERROR,L"bthns_InitializeOnce:: ERROR_OUTOFMEMORY\r\n"));
		return ERROR_OUTOFMEMORY;
	}

	return ERROR_SUCCESS;
}

int bthns_CreateDriverInstance(void)  {
	HKEY hKey;
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"bthns_CreateDriverInstance entered\r\n"));

	if (! gpBthNS) {
		IFDBG(DebugOut(DEBUG_ERROR, L"bthns_CreateDriverInstance:: ERROR_SERVICE_DOES_NOT_EXIST\n"));
		return ERROR_SERVICE_DOES_NOT_EXIST;
	}

	gpBthNS->Lock();
	if (gpBthNS->eStage != JustCreated) {
		IFDBG(DebugOut(DEBUG_ERROR, L"bthns_CreateDriverInstance:: ERROR_SERVICE_ALREADY_RUNNING\n"));
		gpBthNS->Unlock();
		return ERROR_SERVICE_ALREADY_RUNNING;
	}
	gpBthNS->eStage = Initializing;

	if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, g_szSDPBaseKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		DWORD dwType;
		DWORD dwSize;

		if ((RegQueryValueEx (hKey, g_szTimeoutVal, 0, &dwType, (LPBYTE)&g_dwSearchTimeout, &dwSize) != ERROR_SUCCESS) ||
		    (dwType != REG_DWORD) || (dwSize != sizeof(g_dwSearchTimeout))) {
		    g_dwSearchTimeout = SPD_SEARCH_DEFAULT_TIMEOUT;
		}
		RegCloseKey (hKey);
	}
	else
		g_dwSearchTimeout = SPD_SEARCH_DEFAULT_TIMEOUT;

	IFDBG(DebugOut(DEBUG_SDP_INIT,L"SDP Default search timeout = 0x%08x milliseconds\r\n",g_dwSearchTimeout));
	SVSUTIL_ASSERT(g_dwSearchTimeout);

	SVSUTIL_ASSERT(! (gpBthNS->pfmdBthNsHandles && gpBthNS->pfmdNSCAlls && 
	                  gpBthNS->pBthNsHandles    && gpBthNS->pCalls));

	gpBthNS->pfmdBthNsHandles = svsutil_AllocFixedMemDescr(sizeof(BthNsHandle), 10);
	gpBthNS->pfmdNSCAlls   = svsutil_AllocFixedMemDescr(sizeof(Call), 10);
	
	if (!gpBthNS->pfmdBthNsHandles || !gpBthNS->pfmdNSCAlls) {
		IFDBG(DebugOut(DEBUG_ERROR, L"bthns_CreateDriverInstance:: ERROR_OUTOFMEMORY\n"));
		gpBthNS->Unlock();
		return ERROR_OUTOFMEMORY;
	}

	SDP_CALLBACKS          c;
	SDP_EVENT_INDICATION   ei;

	memset(&c,0,sizeof(c));
	c.sdp_Connect_Out                = bthns_Connect_Out;
	c.sdp_ServiceSearch_Out          = bthns_ServiceSearch_Out;
	c.sdp_AttributeSearch_Out        = bthns_AttributeSearch_Out;
	c.sdp_ServiceAttributeSearch_Out = bthns_ServiceAttributeSearch_Out;
	c.sdp_Disconnect_Out             = bthns_Disconnect_Out;
   
	ei.sdp_StackEvent                = bthns_StackEvent;

	int iErr = SDP_EstablishDeviceContext(NULL,&ei,&c,&gpBthNS->sdp_if,&gpBthNS->hSDP);
	if (iErr != ERROR_SUCCESS) {
		IFDBG(DebugOut(DEBUG_ERROR, L"bthns_CreateDriverInstance couldn't plug into SDP, err=0x%08x\n",iErr));
		gpBthNS->Unlock();
		return iErr;
	}

	SVSUTIL_ASSERT(gpBthNS->hSDP);

    // SDP lower layer determines whether or not the lower stack is running or not.
	gpBthNS->eStage = Running;
	gpBthNS->Unlock();
	
	return ERROR_SUCCESS;
}

int bthns_CloseDriverInstance(void)  {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"BTHNS_CloseDriverInstance entered\r\n"));
	SDP_CloseDeviceContext(gpBthNS->hSDP);

	if (! gpBthNS)  {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_CloseDriverInstance: ERROR_SERVICE_NOT_ACTIVE\r\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}	

	gpBthNS->Lock();
	if ((gpBthNS->eStage != Running) && (gpBthNS->eStage != Initializing)) {
		IFDBG(DebugOut(DEBUG_ERROR, L"SDP Close Driver Instance:: ERROR_SERVICE_NOT_ACTIVE\n"));
		gpBthNS->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}
	gpBthNS->eStage   = ShuttingDown;

	while (gpBthNS->pCalls) {
		SetEvent(gpBthNS->pCalls->hEvent);
		gpBthNS->pCalls->iResult = ERROR_CANCELLED;
		gpBthNS->pCalls = gpBthNS->pCalls->pNext;
	}

	while (gpBthNS->pBthNsHandles) {
		DeleteBthNsHandle(gpBthNS->pBthNsHandles);
	}

	while (gpBthNS->GetRefCount() > 1) {
		IFDBG(DebugOut (DEBUG_SDP_TRACE, L"Waiting for ref count in bthns_CloseDriverInstance\n"));
		gpBthNS->Unlock ();
		Sleep (200);
		gpBthNS->Lock ();
	}

	if (gpBthNS->hSDP)
		SDP_CloseDeviceContext(gpBthNS->hSDP);

	if (gpBthNS->pfmdBthNsHandles)
		svsutil_ReleaseFixedNonEmpty(gpBthNS->pfmdBthNsHandles);
		
	if (gpBthNS->pfmdNSCAlls)
		svsutil_ReleaseFixedNonEmpty(gpBthNS->pfmdNSCAlls);

	gpBthNS->ReInit();
	gpBthNS->Unlock();
	return ERROR_SUCCESS;
}

int bthns_UninitializeOnce(void) {
	IFDBG(DebugOut(DEBUG_SDP_INIT | DEBUG_SDP_TRACE,L"bthns_UninitializeOnce()\r\n"));
	SVSUTIL_ASSERT(gpBthNS);

	if (! gpBthNS) {
		IFDBG(DebugOut(DEBUG_ERROR, L"bthns_UninitializeOnce:: ERROR_SERVICE_DOES_NOT_EXIST\n"));
		return ERROR_SERVICE_DOES_NOT_EXIST;
	}
	gpBthNS->Lock();

	if (gpBthNS->eStage != JustCreated) {
		IFDBG(DebugOut(DEBUG_ERROR, L"bthns_UninitializeOnce:: ERROR_DEVICE_IN_USE\n"));
		gpBthNS->Unlock();
		return ERROR_DEVICE_IN_USE;
	}

	BthNs *pBthNs = gpBthNS;
	gpBthNS = NULL;
	pBthNs->Unlock();
	delete pBthNs;

	IFDBG(DebugOut(DEBUG_SDP_INIT, L"bthns_UninitializeOnce:: ERROR_SUCCESS\n"));
	return ERROR_SUCCESS;
}


static DWORD WINAPI StackDisconnect (LPVOID pArg) {
	IFDBG(DebugOut (DEBUG_SDP_TRACE, L"StackDisconnect\n"));
	bthns_CloseDriverInstance ();
	return NULL;
}

static int bthns_StackEvent(void *pUserContext, int iEvent, void *pEventContext) {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"bthns_StackEvent:: event = %d\r\n"));

	// We only handle BTH_STACK_DISCONNECT at this layer.  We just BthNsHandle into SDP
	// in all cases and let it keep track of stack state beyond this.
	if (iEvent == BTH_STACK_DISCONNECT)
		btutil_ScheduleEvent (StackDisconnect, NULL);

	return ERROR_SUCCESS;
}

//
// WSA Interface helpers.
//
void SDPAbortCall(HANDLE h, Call *pCall) {
	DebugOut(DEBUG_SDP_TRACE,L"SDPAbortCall (0x%08x)\r\n",pCall);
	BT_LAYER_ABORT_CALL  pCallback = gpBthNS->sdp_if.sdp_AbortCall;

	__try {
		pCallback(h,pCall);
	}
	__except (1) {
		;
	}
}

int SDPConnectBlocking(Call *pCall, BD_ADDR *pba)  {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"SDPConnectBlocking (0x%04x 0x%08x)\r\n",pba->NAP,pba->SAP));
	int iRet;
	HANDLE hEvent = pCall->hEvent;
	HANDLE h      = gpBthNS->hSDP;

	SDP_Connect_In pCallback = gpBthNS->sdp_if.sdp_Connect_In;

	gpBthNS->AddRef();
	gpBthNS->Unlock();

	__try {
		iRet = pCallback(h,pCall,pba);
	}
	__except (1) {
		;
	}

	// We don't have a timeout here because on connection lower layers handles timeouts.
	if (ERROR_SUCCESS == iRet) {
		if (WAIT_TIMEOUT == WaitForSingleObject(hEvent,INFINITE)) {
			SVSUTIL_ASSERT(0);
			iRet = ERROR_OPERATION_ABORTED;
			SDPAbortCall(h,pCall);
		}
	}

	gpBthNS->Lock();
	gpBthNS->DelRef();
	return iRet;
}

int SDPDisconnect(Call *pCall, unsigned short cid) {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"SDPDisconnectBlocking (0x%08x,0x%04x)\r\n",pCall,cid));
	int iRet;
	HANDLE h = gpBthNS->hSDP;
	HANDLE hEvent = pCall ? pCall->hEvent : 0;

	SDP_Disconnect_In pCallback = gpBthNS->sdp_if.sdp_Disconnect_In;

	gpBthNS->AddRef();
	gpBthNS->Unlock();

	__try {
		iRet = pCallback(h,pCall,cid);
	} __except(1)  {
		;
	}

	// We don't have a timeout here because on connection lower layers handles timeouts.
	if ((ERROR_SUCCESS == iRet) && hEvent) {
		if (WAIT_OBJECT_0 != WaitForSingleObject(hEvent,INFINITE)) {
			SVSUTIL_ASSERT(0);
			iRet = ERROR_OPERATION_ABORTED;
		}
	}

	gpBthNS->Lock();
	gpBthNS->DelRef();
	return iRet;
}

int ServiceSearchBlocking(Call *pCall, unsigned short cid, SdpQueryUuid* pUUIDs, unsigned short cMaxHandles)  {
	SVSUTIL_ASSERT(gpBthNS->eStage == Running);
	HANDLE hEvent = pCall->hEvent;
	HANDLE h      = gpBthNS->hSDP;
	int iRet;

	SDP_ServiceSearch_In pCallback = gpBthNS->sdp_if.sdp_ServiceSearch_In;

	gpBthNS->AddRef();
	gpBthNS->Unlock();

	__try {
		iRet = pCallback(h,pCall,cid,pUUIDs,cMaxHandles);
	} __except(1) {
		;
	}

	if (ERROR_SUCCESS == iRet) {
		if (WAIT_TIMEOUT == WaitForSingleObject(hEvent,g_dwSearchTimeout)) {
			iRet = ERROR_OPERATION_ABORTED;
			SDPAbortCall(h,pCall);
		}
	}

	gpBthNS->Lock();
	gpBthNS->DelRef();
	return iRet;
}


int AttributeSearchBlocking(Call *pCall, unsigned short cid, unsigned long recordHandle, SdpAttributeRange *pAttribRange, int numAttributes)  {
	SVSUTIL_ASSERT(gpBthNS->eStage == Running);
	HANDLE hEvent = pCall->hEvent;
	HANDLE h      = gpBthNS->hSDP;
	int iRet;

	SDP_AttributeSearch_In pCallback = gpBthNS->sdp_if.sdp_AttributeSearch_In;
	
	gpBthNS->AddRef();
	gpBthNS->Unlock();

	__try {
		iRet = pCallback(h,pCall,cid,recordHandle,pAttribRange,numAttributes);
	} __except(1) {
		;
	}

	if (ERROR_SUCCESS == iRet) {
		if (WAIT_TIMEOUT == WaitForSingleObject(hEvent,g_dwSearchTimeout)) {
			iRet = ERROR_OPERATION_ABORTED;
			SDPAbortCall(h,pCall);
		}
	}

	gpBthNS->Lock();
	gpBthNS->DelRef();
	return iRet;
}

int ServiceAttributeSearchBlocking(Call *pCall, unsigned short cid, SdpQueryUuid* pUUIDs, SdpAttributeRange *pAttribRange, int numAttributes)  {
	SVSUTIL_ASSERT(gpBthNS->eStage == Running);
	HANDLE hEvent = pCall->hEvent;
	HANDLE h      = gpBthNS->hSDP;
	int iRet;

	SDP_ServiceAttributeSearch_In pCallback = gpBthNS->sdp_if.sdp_ServiceAttributeSearch_In;

	gpBthNS->AddRef();
	gpBthNS->Unlock();

	__try {
		iRet = pCallback(h,pCall,cid,pUUIDs,pAttribRange,numAttributes);
	} __except(1) {
		;
	}

	if (ERROR_SUCCESS == iRet) {
		if (WAIT_TIMEOUT == WaitForSingleObject(hEvent,g_dwSearchTimeout)) {
			iRet = ERROR_OPERATION_ABORTED;
			SDPAbortCall(h,pCall);
		}
	}

	gpBthNS->Lock();
	gpBthNS->DelRef();
	return iRet;
}

typedef enum  {
	BTHNS_SET_SERVICE,
	BTHNS_LOOKUP_SERVICE_BEGIN,
	BTHNS_LOOKUP_SERVICE_NEXT
} BTHNS_WSA_CALL;

#define SDP_WSA_SERVICE_BEGIN_FLAGS   (LUP_RES_SERVICE | LUP_CONTAINERS)

static BOOL CheckQuerySet(LPWSAQUERYSET pQuerySet, BTHNS_WSA_CALL BthNsHandleer, DWORD dwFlags, WSAESETSERVICEOP op=RNRSERVICE_DEREGISTER)  {
	if (!pQuerySet || pQuerySet->dwSize != sizeof(WSAQUERYSETA) || pQuerySet->dwNameSpace != NS_BTH)
		return FALSE;

	switch (BthNsHandleer)  {
		case BTHNS_SET_SERVICE: {
			if (op != RNRSERVICE_REGISTER && op != RNRSERVICE_DELETE)
				return FALSE;

			if (dwFlags)
				return FALSE;

//			if (!pQuerySet->dwNumberOfCsAddrs || !pQuerySet->lpcsaBuffer)
//				return FALSE;

			PBTHNS_SETBLOB pSetBlob = (PBTHNS_SETBLOB) (pQuerySet->lpBlob ? pQuerySet->lpBlob->pBlobData : NULL);
			if (!pSetBlob || !pSetBlob->pSdpVersion || (*pSetBlob->pSdpVersion != BTH_SDP_VERSION) || !pSetBlob->pRecordHandle || ((op == RNRSERVICE_REGISTER) && !pSetBlob->ulRecordLength))
				return FALSE;
		}
		break;

		case BTHNS_LOOKUP_SERVICE_BEGIN:  {
			PBTHNS_RESTRICTIONBLOB pResBlob = (PBTHNS_RESTRICTIONBLOB) (pQuerySet->lpBlob ?  pQuerySet->lpBlob->pBlobData : NULL);
			if (dwFlags & ~SDP_WSA_SERVICE_BEGIN_FLAGS)
				return FALSE;

			if (dwFlags & LUP_CONTAINERS) {
				LPBLOB lpBlob = pQuerySet->lpBlob;
				if (lpBlob && (lpBlob->cbSize != sizeof(BTHNS_INQUIRYBLOB)))
					return FALSE;

				return TRUE; // it's OK for lpBlob to be NULL or non-NULL when doing device inquiry.
			}

			// pQuerySet->lpServiceClassId can be set on Service or ServiceAttribute requests.
			// If pResBlob is set we use it, it has more interesting info.
			if (!pQuerySet->lpServiceClassId && !pResBlob)
				return FALSE;

			if (!(dwFlags & LUP_RES_SERVICE)) {
				// lpcsaBuffer contains an AddrInfo and is supported only on WinCE, specifies remote device to query.
				// pQuerySet->lpszContext contains a string representation of a bluetooth address of remote device to query, and is for XP compat.  
				if (!pQuerySet->lpszContext && !pQuerySet->lpcsaBuffer)
					return FALSE;

				// lpszContext will be checked when we try to do conversion, just do lpcsaBuffer now.
				if (pQuerySet->lpcsaBuffer) {
					if ((!pQuerySet->lpcsaBuffer->RemoteAddr.lpSockaddr || (pQuerySet->lpcsaBuffer->RemoteAddr.iSockaddrLength != sizeof(SOCKADDR_BTH))))  {
						return FALSE;
					}

#if defined (DEBUG) || defined (_DEBUG)
					if (! __SDP_IS_ALIGNED(pQuerySet->lpcsaBuffer->RemoteAddr.lpSockaddr, 8))
						DebugOut(DEBUG_WARN, L"SDP: Unaligned remote address in structure - FIX THE APP!!\n");
#endif
				}
			}

			if (pResBlob && !(dwFlags&LUP_CONTAINERS))  {
				switch (pResBlob->type) {
					case SDP_SERVICE_SEARCH_REQUEST:			
					break;

					case SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST:
					case SDP_SERVICE_ATTRIBUTE_REQUEST:
						if (!pResBlob->numRange ||  ((pQuerySet->lpBlob->cbSize - sizeof(BTHNS_RESTRICTIONBLOB)) % sizeof(SdpAttributeRange))  ||
						   (((pQuerySet->lpBlob->cbSize - sizeof(BTHNS_RESTRICTIONBLOB)) / sizeof(SdpAttributeRange) + 1) != pResBlob->numRange))
							return FALSE;
					break;

					default:
						return FALSE;
				}
			}
		}
		break;

		case BTHNS_LOOKUP_SERVICE_NEXT:  {
			if (!pQuerySet)
				return FALSE;
		}
		break;

		default:
			SVSUTIL_ASSERT(0);
	}
	return TRUE;
}


int MapErrorCodeToWinsockErr(int iError, BOOL fFromSetService=FALSE) {
	switch (iError) {

	// either lower layer failed, SDP timed out, or BthNsLookupServiceEnd() 
	// was called on pending search thread.
	case ERROR_OPERATION_ABORTED:
	case ERROR_CONNECTION_ABORTED:
	case ERROR_SHUTDOWN_IN_PROGRESS:
	case ERROR_CANCELLED:
		return WSA_E_CANCELLED;

	case ERROR_INTERNAL_ERROR:
	case ERROR_INVALID_PARAMETER:
		return WSAEINVAL;

	case ERROR_OUTOFMEMORY:
		return WSA_NOT_ENOUGH_MEMORY;

	case ERROR_NOT_FOUND:
		return fFromSetService ? WSA_INVALID_HANDLE : WSANO_DATA;

	case ERROR_SERVICE_NOT_ACTIVE:
		return WSASERVICE_NOT_FOUND;

	case ERROR_CONNECTION_UNAVAIL:
		return WSAENOTCONN;

	default:
		SVSUTIL_ASSERT(0);
	}
	return WSAEINVAL;
}

//
//  Public interface, equivalent to WSA Set/Lookup functions.
//
//  NOTE: This is preliminary, pending official data structures from NT.
//
int BthNsSetService(LPWSAQUERYSET pSet, WSAESETSERVICEOP op, DWORD dwFlags) {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"BthNsSetService entered\r\n"));

	if (! gpBthNS) {
		IFDBG(DebugOut(DEBUG_ERROR, L"BthNsSetService:: ERROR_SERVICE_DOES_NOT_EXIST\n"));
		SetLastError(WSASERVICE_NOT_FOUND);
		return SOCKET_ERROR;
	}

	gpBthNS->Lock();
	if (gpBthNS->eStage != Running) {
		IFDBG(DebugOut(DEBUG_ERROR, L"BthNsSetService:: ERROR_SERVICE_NOT_ACTIVE\n"));
		gpBthNS->Unlock();
		SetLastError(WSASERVICE_NOT_FOUND);
		return SOCKET_ERROR;
	}

	if (!CheckQuerySet(pSet,BTHNS_SET_SERVICE,dwFlags,op))  {
		IFDBG(DebugOut(DEBUG_ERROR,L"BthNsSetService: ERROR_INVALID_PARAMETER\r\n"));
		gpBthNS->Unlock();
		SetLastError(WSAEINVAL);
		return SOCKET_ERROR;
	}

	PBTHNS_SETBLOB pSetBlob = (PBTHNS_SETBLOB) pSet->lpBlob->pBlobData;
	int iRet;

	// Since these BthNsHandles operate loBthNsHandley, they are blocking.
	if (op == RNRSERVICE_REGISTER)
		iRet = gpBthNS->sdp_if.sdp_AddRecord(gpBthNS->hSDP,pSetBlob->pRecord,pSetBlob->ulRecordLength,(HANDLE*)(pSetBlob->pRecordHandle));
	else {
		SVSUTIL_ASSERT(op == RNRSERVICE_DELETE);
		iRet = gpBthNS->sdp_if.sdp_RemoveRecord(gpBthNS->hSDP,(HANDLE)(*(pSetBlob->pRecordHandle)));
	}

	gpBthNS->Unlock();
	if (iRet != ERROR_SUCCESS) {
		iRet = MapErrorCodeToWinsockErr(iRet,TRUE);
		SetLastError(iRet);
		return SOCKET_ERROR;
	}
	
	return ERROR_SUCCESS;
}

//
// WSA Interface functions
//

#define MAX_SEARCH_HANDLES             0xFFFF
#define INQUIRY_GIAC_LAP               0x9e8b33  // unlimited inquiry access mode (GIAC)
#define INQUIRY_DURATION               0x10
#define INQUIRY_MAX_RESPONSES          0x20

int BthNsLookupServiceBegin(LPWSAQUERYSET pQuerySet, DWORD dwFlags, LPHANDLE lphLookup)   {
	int iRes = ERROR_INTERNAL_ERROR;
	unsigned short cid = 0;
	BthNsHandle *pBthNsHandle = NULL;

	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"BthNsLookupServiceBegin entered\r\n"));

	if (! gpBthNS) {
		IFDBG(DebugOut(DEBUG_ERROR, L"BthNsLookupServiceBegin:: WSASERVICE_NOT_FOUND\n"));
		SetLastError(WSASERVICE_NOT_FOUND);
		return SOCKET_ERROR;
	}

	gpBthNS->Lock();
	if (gpBthNS->eStage != Running) {
		IFDBG(DebugOut(DEBUG_ERROR, L"BthNsLookupServiceBegin:: WSASERVICE_NOT_FOUND\n"));
		gpBthNS->Unlock();
		SetLastError(WSASERVICE_NOT_FOUND);
		return SOCKET_ERROR;
	}

	if (!CheckQuerySet(pQuerySet,BTHNS_LOOKUP_SERVICE_BEGIN,dwFlags) || !lphLookup)  {
		IFDBG(DebugOut(DEBUG_ERROR,L"BthNsLookupServiceBegin: WSAEINVAL\r\n"));
		gpBthNS->Unlock();
		SetLastError(WSAEINVAL);
		return SOCKET_ERROR;
	}

	if (NULL == (pBthNsHandle = AllocBthNsHandle(pQuerySet, dwFlags,&iRes)))   {
		IFDBG(DebugOut(DEBUG_ERROR,L"BthNsLookupServiceBegin: ERROR_OUTOFMEMORY\r\n"));
		gpBthNS->Unlock();
		SetLastError(iRes);
		return SOCKET_ERROR;
	}

	if (dwFlags & LUP_CONTAINERS) {
		// device inquiry
		// If pInquiryBlob is NULL, then use default values.
		PBTHNS_INQUIRYBLOB pInquiryBlob = (PBTHNS_INQUIRYBLOB) ( pQuerySet->lpBlob ? pQuerySet->lpBlob->pBlobData : NULL);
		pBthNsHandle->LAP        = pInquiryBlob ? pInquiryBlob->LAP           : INQUIRY_GIAC_LAP;
		pBthNsHandle->cDuration  = pInquiryBlob ? pInquiryBlob->length        : INQUIRY_DURATION;

		// WinXP doesn't support pInquiryBlob->num_responses field.  To avoid
		// potential porting issues (i.e. an app on XP doesn't set this and on porting
		// it's a garbage value on CE) we ignore it.
	    // pBthNsHandle->cMaxResp   = pInquiryBlob ? pInquiryBlob->num_responses : INQUIRY_MAX_RESPONSES;
		pBthNsHandle->cMaxResp   = INQUIRY_MAX_RESPONSES;
		pBthNsHandle->fInquiryStatus = INQUIRY_PENDING;
	}
	gpBthNS->Unlock();

	*lphLookup = (HANDLE) pBthNsHandle;

	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"BthNsLookupServiceBegin returns ERROR_SUCCESS\r\n"));
	return ERROR_SUCCESS;
}

int PerformInquiry (BthNsHandle *pBthNsHandle) {
	unsigned long LAP = pBthNsHandle->LAP;
	unsigned int cMaxResp = pBthNsHandle->cMaxResp;
	unsigned int cDuration = pBthNsHandle->cDuration;
	unsigned int cDevices = 0;

	BthInquiryResult *pInquiryResults = (BthInquiryResult *) g_funcAlloc(cMaxResp*sizeof(BthInquiryResult),g_pvAllocData);

	if (!pInquiryResults) {
		IFDBG(DebugOut(DEBUG_ERROR,L"BthNsLookupServiceBegin: ERROR_OUTOFMEMORY\r\n"));
		gpBthNS->Unlock();
		SetLastError(WSA_NOT_ENOUGH_MEMORY);
		return SOCKET_ERROR;
	}

	memset(pInquiryResults,0,cMaxResp*sizeof(BthInquiryResult));

	pBthNsHandle->fInquiryStatus = INQUIRY_INPROGRESS;

	gpBthNS->AddRef();
	gpBthNS->Unlock();

	int iResult = BthPerformInquiry(LAP,cDuration,0,
		                            cMaxResp,&cDevices,pInquiryResults);

	gpBthNS->Lock();
	gpBthNS->DelRef();

	if (! FindBthNsHandle(pBthNsHandle)) {
		IFDBG(DebugOut(DEBUG_ERROR,L"BthNsLookupServiceBegin: handle (0x%08x) invalid after BthPerformInquiry()\r\n",pBthNsHandle));
		g_funcFree(pInquiryResults,g_pvAllocData);
		gpBthNS->Unlock();
		SetLastError(WSA_E_CANCELLED);
		return SOCKET_ERROR;
	}

	pBthNsHandle->fInquiryStatus = INQUIRY_COMPLETED;

	if ((ERROR_SUCCESS != iResult)) {
		IFDBG(DebugOut(DEBUG_ERROR,L"BthNsLookupServiceBegin: BthPerformInquiry returns 0x%08x\r\n",iResult));
		g_funcFree(pInquiryResults,g_pvAllocData);
		gpBthNS->Unlock();
		SetLastError(WSAENETDOWN);
		return SOCKET_ERROR;
	}

	SVSUTIL_ASSERT(cDevices <= cMaxResp);

	if (cDevices > cMaxResp)
		cDevices = cMaxResp;

	pBthNsHandle->cDevices        = cDevices;
	pBthNsHandle->pInquiryResults = pInquiryResults;

	return ERROR_SUCCESS;
}

// supported Contairer flags for dwFlags param in BthNsLookupServiceNext
#define BTHNS_LOOKUP_SERVICE_NEXT_CONTAINER_FLAGS     (LUP_RETURN_NAME | LUP_RETURN_ADDR | LUP_RETURN_BLOB | BTHNS_LUP_RESET_ITERATOR | BTHNS_LUP_NO_ADVANCE)

int BthNsLookupServiceNext(HANDLE hLookup, DWORD dwFlags, LPDWORD lpdwBufferLength, LPWSAQUERYSET pResults) {
	int                   iRes = ERROR_INTERNAL_ERROR;
	unsigned short        cid  = 0;
	BOOL                  fLocal;
	DWORD                 cbRequired  = 0;
	PBTHNS_RESTRICTIONBLOB  pResBlob    = NULL;
	ULONG                 type;
	BOOL                  fDisconnect = FALSE;
	Call                  *pCall      = NULL;

	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"BthNsLookupServiceNext entered\r\n"));

	if (!lpdwBufferLength)  {
		IFDBG(DebugOut(DEBUG_ERROR,L"BthNsLookupServiceNext: params misformatted, ERROR_INVALID_PARAMETER\r\n"));
		SetLastError(WSAEINVAL);
		return SOCKET_ERROR;
	}

	if (!gpBthNS) {
		IFDBG(DebugOut(DEBUG_ERROR,L"BthNsLookupServiceNext:: ERROR_SERVICE_DOES_NOT_EXIST\r\n"));
		SetLastError(WSASERVICE_NOT_FOUND);
		return SOCKET_ERROR;
	}
	gpBthNS->Lock();

	if (gpBthNS->eStage != Running) {
		IFDBG(DebugOut(DEBUG_ERROR,L"BthNsLookupServiceNext:: ERROR_SERVICE_NOT_ACTIVE\r\n"));
		gpBthNS->Unlock();
		SetLastError(WSASERVICE_NOT_FOUND);
		return SOCKET_ERROR;
	}

	BthNsHandle *pBthNsHandle = FindBthNsHandle((BthNsHandle *) hLookup);
	if (!pBthNsHandle) { 
		IFDBG(DebugOut(DEBUG_ERROR,L"BthNsSetService: BthNsHandle not found, ERROR_INVALID_PARAMETER\r\n"));
		gpBthNS->Unlock();
		SetLastError(WSA_INVALID_HANDLE);
		return SOCKET_ERROR;
	}

	//
	// Device Inquiry
	//
	if (pBthNsHandle->dwFlags & LUP_CONTAINERS) {
		if (pBthNsHandle->fInquiryStatus == INQUIRY_PENDING) {
			if (PerformInquiry (pBthNsHandle) == SOCKET_ERROR)	// If error, it unlocks
				return SOCKET_ERROR;
		}

		if (! __SDP_IS_ALIGNED(pResults, 8))
			DebugOut(DEBUG_WARN, L"SDP: Unaligned remote address in structure - FIX THE APP!\n");

		if (dwFlags & ~(BTHNS_LOOKUP_SERVICE_NEXT_CONTAINER_FLAGS)) {
			IFDBG(DebugOut(DEBUG_ERROR,L"BthNsLookupServiceNext flags are invalid (=0x%08x)",dwFlags));
			gpBthNS->Unlock();
			SetLastError(WSAEFAULT);
			return SOCKET_ERROR;
		}

		// WinCE allows iterator list to be reset, allowing list to be traveresed again.
		if (dwFlags & BTHNS_LUP_RESET_ITERATOR) {
			pBthNsHandle->iDeviceIterator = 0;
			gpBthNS->Unlock();
			return ERROR_SUCCESS;
		}

		if (pBthNsHandle->iDeviceIterator >= pBthNsHandle->cDevices) {
			gpBthNS->Unlock();
			SetLastError(WSA_E_NO_MORE); 
			return SOCKET_ERROR;
		}

		cbRequired = sizeof(WSAQUERYSET);

		if (dwFlags & LUP_RETURN_NAME)
			cbRequired += BD_MAXNAME * sizeof(WCHAR);

		if (dwFlags & LUP_RETURN_ADDR) {
			cbRequired = __SDP_ALIGN(cbRequired, 4);
			cbRequired = __SDP_ALIGN((cbRequired + sizeof(CSADDR_INFO)), 8) + 2 * sizeof(SOCKADDR_BTH);
		}

		// Note: WinCE returns a different data structure on this call than NT, by design.
		if (dwFlags & LUP_RETURN_BLOB) {
			cbRequired = __SDP_ALIGN(cbRequired, 4);
			cbRequired = __SDP_ALIGN((cbRequired + sizeof(LPBLOB)), 8) + sizeof(BthInquiryResult);
		}

		if (cbRequired > *lpdwBufferLength) {
			IFDBG(DebugOut(DEBUG_SDP_TRACE,L"BthNsLookupServiceNext: %d bytes passed in, %d required\r\n",*lpdwBufferLength,cbRequired));
			*lpdwBufferLength = cbRequired;
			gpBthNS->Unlock();
			SetLastError(WSA_NOT_ENOUGH_MEMORY);
			return SOCKET_ERROR;
		}

		__try {
			CHAR *pBase = (CHAR*)pResults;
			unsigned int uiOffsetCounter = sizeof(WSAQUERYSET);

			memset(pResults,0,sizeof(*pResults));
			pResults->dwSize = sizeof(WSAQUERYSETW);
			pResults->lpServiceClassId = 0;
			pResults->dwNameSpace = NS_BTH;

			if (dwFlags & LUP_RETURN_NAME) {
				// we don't query for names in WSALookupBegin, so we need to do it here.
				BT_ADDR ba = pBthNsHandle->pInquiryResults[pBthNsHandle->iDeviceIterator].ba;
				unsigned int cbReceived = 0;
				WCHAR   wszDeviceName[BD_MAXNAME];

				memset (wszDeviceName, 0, sizeof(wszDeviceName));

				gpBthNS->AddRef();
				gpBthNS->Unlock();

				iRes = BthRemoteNameQuery(&ba, SVSUTIL_ARRLEN(wszDeviceName), &cbReceived, wszDeviceName);

				gpBthNS->Lock();
				gpBthNS->DelRef();

				if (! FindBthNsHandle((BthNsHandle *) hLookup)) {
					IFDBG(DebugOut(DEBUG_ERROR,L"BthNsLookupServiceNext: handle (0x%08x) invalid after BthRemoteNameQuery()\r\n",hLookup));
					gpBthNS->Unlock();
					SetLastError(WSA_E_CANCELLED);
					return SOCKET_ERROR;
				}

				// in the event of error, don't fail, just leave field NULL.
				if (iRes == ERROR_SUCCESS) {
					pResults->lpszServiceInstanceName = (WCHAR *)(pBase + uiOffsetCounter);
					wcsncpy (pResults->lpszServiceInstanceName, wszDeviceName, BD_MAXNAME);
				}
				uiOffsetCounter += BD_MAXNAME * sizeof(WCHAR);
			}

			if (dwFlags & LUP_RETURN_ADDR) {
				int uiOffsetCounterStart = uiOffsetCounter = __SDP_ALIGN(uiOffsetCounter, 4);

				PCSADDR_INFO pAddrInfo = (PCSADDR_INFO)(pBase + uiOffsetCounter);

				pResults->lpcsaBuffer = pAddrInfo;
				pResults->dwNumberOfCsAddrs = 1;

				uiOffsetCounter = __SDP_ALIGN(uiOffsetCounter + sizeof(CSADDR_INFO), 8);

				PSOCKADDR_BTH pLocal = (PSOCKADDR_BTH)(pBase + uiOffsetCounter);
				PSOCKADDR_BTH pRemote = pLocal + 1;

				uiOffsetCounter += 2 * sizeof(SOCKADDR_BTH);
				memset (pBase + uiOffsetCounterStart, 0, uiOffsetCounter - uiOffsetCounterStart);

				pAddrInfo->iSocketType = SOCK_STREAM;
				pAddrInfo->LocalAddr.lpSockaddr = (PSOCKADDR)pLocal;
				pAddrInfo->LocalAddr.iSockaddrLength = sizeof(*pLocal);
				pLocal->addressFamily = AF_BT;

				BthReadLocalAddr (&pLocal->btAddr);

				pAddrInfo->RemoteAddr.lpSockaddr = (PSOCKADDR)pRemote;
				pAddrInfo->RemoteAddr.iSockaddrLength = sizeof(*pRemote);
				pRemote->addressFamily = AF_BT;
				memcpy(&pRemote->btAddr, &pBthNsHandle->pInquiryResults[pBthNsHandle->iDeviceIterator].ba, sizeof(BT_ADDR));
			}

			if (dwFlags & LUP_RETURN_BLOB) {
				uiOffsetCounter = __SDP_ALIGN(uiOffsetCounter, 4);

				pResults->lpBlob = (LPBLOB)(pBase + uiOffsetCounter);
				pResults->lpBlob->cbSize    = sizeof(BthInquiryResult);

				uiOffsetCounter = __SDP_ALIGN(uiOffsetCounter + sizeof(BLOB), 8);

				pResults->lpBlob->pBlobData = (PBYTE) (pBase + uiOffsetCounter);
				memcpy(pResults->lpBlob->pBlobData,(BthInquiryResult *)&pBthNsHandle->pInquiryResults[pBthNsHandle->iDeviceIterator],sizeof(BthInquiryResult));
				uiOffsetCounter += sizeof(BthInquiryResult);
			}

			SVSUTIL_ASSERT (uiOffsetCounter == cbRequired);
		} __except (1) {
			IFDBG(DebugOut(DEBUG_ERROR,L"BthNsLookupServiceNext: Exception!\n"));
			gpBthNS->Unlock();
			return ERROR_INVALID_PARAMETER;
		}
		// CE offers the option of not advancing iterator, so app may query for
		// more detailed info on this BT addr if it so desires.
		if (! (dwFlags & BTHNS_LUP_NO_ADVANCE))
			pBthNsHandle->iDeviceIterator++;

		gpBthNS->Unlock();

		return ERROR_SUCCESS;
	}

	//
	// Service Search
	//
	if (pBthNsHandle->fNoMoreData) {
		IFDBG(DebugOut(DEBUG_ERROR,L"BthNsLookupServiceNext: WSA_E_NO_MORE\r\n"));
		gpBthNS->Unlock();
		SetLastError(WSA_E_NO_MORE);
		return SOCKET_ERROR;
	}

	if (! pBthNsHandle->fNetSearchComplete) {
		fLocal = pBthNsHandle->fLocal;

		if (NULL == (pCall = AllocCall(pBthNsHandle))) {
			IFDBG(DebugOut(DEBUG_ERROR,L"BthNsLookupServiceNext: WSA_NOT_ENOUGH_MEMORY\r\n"));
			gpBthNS->Unlock();
			SetLastError(WSA_NOT_ENOUGH_MEMORY);
			return SOCKET_ERROR;
		}

		// Below this point we may have to disconnect, so goto done for common exit pt.
		if (!fLocal) {
			iRes = SDPConnectBlocking(pCall,&pBthNsHandle->b);
			SVSUTIL_ASSERT(gpBthNS->IsLocked());

			if (iRes != ERROR_SUCCESS) {
				iRes = WSASERVICE_NOT_FOUND;
				goto done;
			}
			if (NULL == (pCall = FindCall(pCall)) || (pCall->iResult != ERROR_SUCCESS))   {
				iRes = pCall ? WSASERVICE_NOT_FOUND : WSA_E_CANCELLED;
				goto done;
			}
			fDisconnect = TRUE;
			cid = pCall->cid;

			if (NULL == (pBthNsHandle = FindBthNsHandle(pBthNsHandle))) {
				iRes = WSA_E_CANCELLED;
				goto done;
			}
		}
		pResBlob = pBthNsHandle->pResBlob;

		// To determine which type of inquiery to perform, if we have pResBlob do
		// what's specifed there.  If we don't we use the GUID set in ServiceClassId on
		// BthNsLookupServiceBegin call.  On CE to keep bc we assume SDP_SERVICE_SEARCH_REQUEST,
		// however if we're in XP compat mode then we do a SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST
		// with all attributes.
		if (pResBlob)
			type = pResBlob->type;
		else if (pBthNsHandle->fXPCompatMode)
			type = SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST;
		else
			type = SDP_SERVICE_SEARCH_REQUEST;

		SVSUTIL_ASSERT( (fLocal && !cid) || (!fLocal && cid));

		// If no restrictions were set, use ServiceClassId to fill out a default 
		// restriction blob.
		SdpQueryUuid UUID[2];
		SdpQueryUuid *pUUIDs;

		SdpAttributeRange Range;
		SdpAttributeRange *pRange;
		int numAttributes;

		if ((type == SDP_SERVICE_SEARCH_REQUEST) || (type == SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST)) {
			if (pResBlob) {
				pUUIDs = pResBlob->uuids;
			}
			else {
				memcpy(&UUID[0].u.uuid128,&pBthNsHandle->ServiceClassId,sizeof(GUID));
				UUID[0].uuidType = SDP_ST_UUID128;
				// This is so SdpInterface::VerifyServiceSearch knows where to stop
				memset(&UUID[1],0,sizeof(SdpQueryUuid));
				pUUIDs = UUID;
			}
		}

		if (type == SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST) {
			if (pResBlob) {
				pRange = pResBlob->pRange;
				numAttributes = pResBlob->numRange;
			}
			else {
				SVSUTIL_ASSERT(pBthNsHandle->fXPCompatMode);
				Range.minAttribute = 0;
				Range.maxAttribute = 0xffff;
				pRange = &Range;
				numAttributes = 1;
			}
		}

		switch (type) {
			case SDP_SERVICE_SEARCH_REQUEST:  {
				if (ERROR_SUCCESS != (iRes = ServiceSearchBlocking(pCall,cid, pUUIDs, MAX_SEARCH_HANDLES)))
					iRes = MapErrorCodeToWinsockErr(iRes);
			}
			break;

			case SDP_SERVICE_ATTRIBUTE_REQUEST: {
				numAttributes = pResBlob->numRange;
				if (ERROR_SUCCESS != (iRes = AttributeSearchBlocking(pCall, cid, pResBlob->serviceHandle,pResBlob->pRange,numAttributes)))
					iRes = MapErrorCodeToWinsockErr(iRes);
			}
			break;

			case SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST: {
				if (ERROR_SUCCESS != (iRes = ServiceAttributeSearchBlocking(pCall, cid, pUUIDs,pRange,numAttributes)))
					iRes = MapErrorCodeToWinsockErr(iRes);
			}
			break;

			default:
				SVSUTIL_ASSERT(0);
		}
		SVSUTIL_ASSERT(gpBthNS->IsLocked());

		if (iRes != ERROR_SUCCESS)
			goto done;

		if (NULL == (pCall = FindCall(pCall)) || pCall->iResult != ERROR_SUCCESS || 
		    NULL == (pBthNsHandle = FindBthNsHandle(pBthNsHandle)))  {
			iRes = MapErrorCodeToWinsockErr((pCall && pBthNsHandle) ? pCall->iResult : ERROR_OPERATION_ABORTED);
			goto done;
		}

		pBthNsHandle->pClientBuf = pCall->pClientBuf;
		pBthNsHandle->cClientBuf = pCall->cClientBuf;
		pBthNsHandle->fNetSearchComplete  = TRUE;
		pCall->pClientBuf      = NULL;
		pCall->cClientBuf      = 0;
	}

	if (!pBthNsHandle->cClientBuf) {
		SVSUTIL_ASSERT(!pBthNsHandle->pClientBuf);
		iRes = WSA_E_NO_MORE;
		goto done;
	}
	cbRequired = pBthNsHandle->cClientBuf + sizeof(WSAQUERYSET) + sizeof(BLOB);

	if (*lpdwBufferLength < cbRequired || !pResults) {
		IFDBG(DebugOut(DEBUG_ERROR,L"BthNsLookupServiceNext: WSA_NOT_ENOUGH_MEMORY.  *lpdwBufferLength=%d,cbRequired=%d\r\n",*lpdwBufferLength,cbRequired));
		*lpdwBufferLength = cbRequired;
		iRes =  WSA_NOT_ENOUGH_MEMORY;
		goto done;
	}

	*lpdwBufferLength = cbRequired;
	pBthNsHandle->fNoMoreData = TRUE;

#if defined (UNDER_CE)
    memset(pResults,0,sizeof(WSAQUERYSETA));
	pResults->dwSize      = sizeof(WSAQUERYSETA);
	pResults->dwNameSpace = NS_BTH;
	pResults->lpBlob = (LPBLOB) ((char*)pResults + sizeof(WSAQUERYSET));
	pResults->lpBlob->cbSize    = pBthNsHandle->cClientBuf;
	pResults->lpBlob->pBlobData = (PBYTE) ((char*)pResults->lpBlob + sizeof(BLOB));

	if (pBthNsHandle->pClientBuf)
		memcpy(pResults->lpBlob->pBlobData,pBthNsHandle->pClientBuf,pBthNsHandle->cClientBuf);
#endif

	iRes = ERROR_SUCCESS;
done:

#if defined (DEBUG) || defined (_DEBUG)
	if (iRes != ERROR_SUCCESS)
		IFDBG(DebugOut(DEBUG_ERROR,L"BthNsLookupServiceNext:: fails, err=0x%08x\r\n",iRes));
#endif

	if (fDisconnect) {
		SDPDisconnect(pCall,cid);
		SVSUTIL_ASSERT(gpBthNS->IsLocked());
	}

	if (pBthNsHandle)
		pBthNsHandle->pCall = NULL;

	if (pCall)
		DeleteCall(pCall);

	if (iRes != ERROR_SUCCESS) {
		SetLastError(iRes);
		iRes = SOCKET_ERROR;
	}

	gpBthNS->Unlock();
	return iRes;
}

int BthNsLookupServiceEnd(HANDLE hLookup)  {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"BthNsLookupServiceEnd entered\r\n"));

	if (!gpBthNS) {
		IFDBG(DebugOut(DEBUG_ERROR,L"BthNsLookupServiceNext:: ERROR_SERVICE_DOES_NOT_EXIST\r\n"));
		SetLastError(WSASERVICE_NOT_FOUND);
		return SOCKET_ERROR;
	}

	gpBthNS->Lock();

	if (gpBthNS->eStage != Running) {
		IFDBG(DebugOut(DEBUG_ERROR,L"BthNsLookupServiceNext:: ERROR_SERVICE_NOT_ACTIVE\r\n"));
		gpBthNS->Unlock();
		SetLastError(WSASERVICE_NOT_FOUND);
		return SOCKET_ERROR;
	}

	int fCancelInquiry = ((DWORD)hLookup == BTHNS_ABORT_CURRENT_INQUIRY);

	if (fCancelInquiry) {
		gpBthNS->Unlock();
		BthCancelInquiry ();

		return ERROR_SUCCESS;
	}

	BthNsHandle *pBthNsHandle = FindBthNsHandle((BthNsHandle *) hLookup);

	if (pBthNsHandle) {
		if (pBthNsHandle->fInquiryStatus == INQUIRY_INPROGRESS)
			fCancelInquiry = TRUE;

		// If the call is currently executing then terminate it.
		if (pBthNsHandle->pCall)
			SetEvent(pBthNsHandle->pCall->hEvent);

		DeleteBthNsHandle(pBthNsHandle);
	}

	gpBthNS->Unlock();

	if (!pBthNsHandle) {
		SetLastError(WSA_INVALID_HANDLE);
		return SOCKET_ERROR;
	}

	if (fCancelInquiry)
		BthCancelInquiry ();

	return ERROR_SUCCESS;
}

// BthNsHandleback functions
static int bthns_Connect_Out(void *pBthNsHandleContext, unsigned long status, unsigned short cid)  {
	if (!gpBthNS) {
		IFDBG(DebugOut(DEBUG_ERROR,L"bthns_Connect_Out:: ERROR_SERVICE_DOES_NOT_EXIST\r\n"));
		return ERROR_SERVICE_DOES_NOT_EXIST;
	}
	gpBthNS->Lock();

	if (gpBthNS->eStage != Running) {
		IFDBG(DebugOut(DEBUG_ERROR,L"bthns_Connect_Out:: ERROR_SERVICE_NOT_ACTIVE\r\n"));
		gpBthNS->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}
	
	Call *pCall = FindCall ((Call*) pBthNsHandleContext);
	if (pCall) {
		pCall->iResult = status;
		pCall->cid     = cid;
		SetEvent(pCall->hEvent);
	}

	gpBthNS->Unlock();
	return ERROR_SUCCESS;
}

static int bthns_Disconnect_Out(void *pBthNsHandleContext, unsigned long status) {
	if (!gpBthNS) {
		IFDBG(DebugOut(DEBUG_ERROR,L"bthns_Connect_Out:: ERROR_SERVICE_DOES_NOT_EXIST\r\n"));
		return ERROR_SERVICE_DOES_NOT_EXIST;
	}
	gpBthNS->Lock();

	if (gpBthNS->eStage != Running) {
		IFDBG(DebugOut(DEBUG_ERROR,L"bthns_Connect_Out:: ERROR_SERVICE_NOT_ACTIVE\r\n"));
		gpBthNS->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}
	
	Call *pCall = FindCall ((Call*) pBthNsHandleContext);
	if (pCall)
		SetEvent(pCall->hEvent);

	gpBthNS->Unlock();
	return ERROR_SUCCESS;
}

static int bthns_ServiceSearch_Out(void *pBthNsHandleContext, unsigned long status, unsigned short cReturnedHandles, unsigned long *pHandles)  {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"bthns_ServiceSearch_Out pContext=0x%08x, status=%d, cRetHandles=%d, pHandles=0x%08x\r\n",
	                              pBthNsHandleContext, status, cReturnedHandles, pHandles));

	int iRet = ERROR_INTERNAL_ERROR;
	BOOL fFreeLock = FALSE;
	Call *pCall;

#if defined (DEBUG) || defined (_DEBUG)
	if (pHandles && cReturnedHandles)  {
		int i;

		for (i = 0; i < cReturnedHandles; i++)
			IFDBG(DebugOut(DEBUG_SDP_PACKETS,L"SDP Handle[%d] = 0x%08x\r\n",i,pHandles[i]));
	}
#endif

	if (!gpBthNS) {
		iRet = ERROR_SERVICE_DOES_NOT_EXIST;
		goto done;
	}

	gpBthNS->Lock();
	fFreeLock = TRUE;

	if (gpBthNS->eStage != Running) {
		iRet = ERROR_SERVICE_NOT_ACTIVE;
		goto done;
	}

	if (NULL == (pCall = FindCall ((Call*) pBthNsHandleContext)))  {
		iRet = ERROR_INVALID_PARAMETER;
		goto done;
	}
	pCall->iResult    = status;
	pCall->cClientBuf = cReturnedHandles * sizeof (DWORD);
	pCall->pClientBuf = (unsigned char*) pHandles;
	SetEvent(pCall->hEvent);
	iRet = ERROR_SUCCESS;
done:

#if defined (DEBUG) || defined (_DEBUG)
	if (iRet != ERROR_SUCCESS)
		IFDBG(DebugOut(DEBUG_ERROR,L"bthns_ServiceSearch_Out: 0x%08x\r\n",iRet));
#endif

	if (iRet != ERROR_SUCCESS && pHandles) {
		ExFreePool(pHandles);
	}
	if (fFreeLock)
		gpBthNS->Unlock();
	
	return iRet;
}

static int bthns_AttributeSearch_Out(void *pBthNsHandleContext, unsigned long status, unsigned char *pOutBuf, unsigned long cOutBuf) {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"bthns_AttributeSearch_Out pContext=0x%08x, status=%d, pOutBuf=0x%08x, cOutBuf=0x%08x\r\n",
	                              pBthNsHandleContext, status, pOutBuf, cOutBuf));


	int iRet = ERROR_INTERNAL_ERROR;
	BOOL fFreeLock = FALSE;
	Call *pCall;

	if (!gpBthNS) {
		iRet = ERROR_SERVICE_DOES_NOT_EXIST;
		goto done;
	}
	gpBthNS->Lock();
	fFreeLock = TRUE;

	if (gpBthNS->eStage != Running) {
		iRet = ERROR_SERVICE_NOT_ACTIVE;
		goto done;
	}

	if (NULL == (pCall = FindCall((Call*) pBthNsHandleContext)))  {
		iRet = ERROR_INVALID_PARAMETER;
		goto done;
	}
	pCall->iResult    = status;
	pCall->pClientBuf = pOutBuf;
	pCall->cClientBuf = cOutBuf;
	SetEvent(pCall->hEvent);
	iRet = ERROR_SUCCESS;
done:

#if defined (DEBUG) || defined (_DEBUG)
	if (iRet != ERROR_SUCCESS)
		IFDBG(DebugOut(DEBUG_ERROR,L"bthns_AttributeSearch_Out: 0x%08x\r\n",iRet));
#endif

	if (iRet != ERROR_SUCCESS && pOutBuf) {
		ExFreePool(pOutBuf);
	}
	if (fFreeLock)
		gpBthNS->Unlock();	
	
	return iRet;
}

static int bthns_ServiceAttributeSearch_Out(void *pBthNsHandleContext, unsigned long status, unsigned char *pOutBuf, unsigned long cOutBuf) {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"bthns_ServiceAttributeSearch_Out pContext=0x%08x, status=%d, pOutBuf=0x%08x, cOutBuf=0x%08x\r\n",
	                              pBthNsHandleContext, status, pOutBuf, cOutBuf));

	int iRet = ERROR_INTERNAL_ERROR;
	BOOL fFreeLock = FALSE;
	Call *pCall;

	if (!gpBthNS) {
		iRet = ERROR_SERVICE_DOES_NOT_EXIST;
		goto done;
	}
	gpBthNS->Lock();
	fFreeLock = TRUE;

	if (gpBthNS->eStage != Running) {
		iRet = ERROR_SERVICE_NOT_ACTIVE;
		goto done;
	}
	if (NULL == (pCall = FindCall((Call*) pBthNsHandleContext)))  {
		iRet = ERROR_INVALID_PARAMETER;
		goto done;
	}
	pCall->iResult    = status;
	pCall->pClientBuf = pOutBuf;
	pCall->cClientBuf = cOutBuf;
	SetEvent(pCall->hEvent);
	iRet = ERROR_SUCCESS;
done:

#if defined (DEBUG) || defined (_DEBUG)
	if (iRet != ERROR_SUCCESS)
		IFDBG(DebugOut(DEBUG_ERROR,L"bthns_AttributeSearch_Out: 0x%08x\r\n",iRet));
#endif

	if (iRet != ERROR_SUCCESS && pOutBuf) {
		ExFreePool(pOutBuf);
	}

	if (fFreeLock)
		gpBthNS->Unlock();
	
	return iRet;
}

#if defined (UNDER_CE)

BOOL       g_fNameSpaceInstalled = FALSE;
HINSTANCE  g_hWS2Lib             = NULL;
typedef int (WINAPI *PFN_WSCINSTALLNAMESPACE) (LPWSTR lpszIdentifier, LPWSTR lpszPathName, DWORD dwNameSpace, DWORD dwVersion, LPGUID lpProviderId);
typedef int (WINAPI *PFN_WSCUNINSTALLNAMESPACE) (LPGUID lpProviderId);


// {371AE22A-2144-4b26-94D8-90C47DD240D0}
static const GUID NsId = {0x371ae22a, 0x2144, 0x4b26, {0x94, 0xd8, 0x90, 0xc4, 0x7d, 0xd2, 0x40, 0xd0}};

// to install in the 1st place, WCSInstallNameSpace().  Only if ws2.dll is installed.
WCHAR szProviderName[] = L"Windows CE Bluetooth Name Space Provider";
WCHAR szProviderPath[] = L"btdrt.dll";

// Installs BTHNS as a Winsock 2 name space provider if Winsock 2 is installed.
void sdp_LoadWSANameSpaceProvider(void) {
	if (g_fNameSpaceInstalled)
		return;

	// wether we suceeed or fail, never run through this code again.
	g_fNameSpaceInstalled = TRUE;

	g_hWS2Lib = LoadLibrary(L"ws2.dll");
	if (!g_hWS2Lib)
		return;

	PFN_WSCINSTALLNAMESPACE pfnInstall = (PFN_WSCINSTALLNAMESPACE) GetProcAddress(g_hWS2Lib,L"WSCInstallNameSpace");
	if (!pfnInstall) {
		SVSUTIL_ASSERT(0);
		return;
	}

	pfnInstall(szProviderName,szProviderPath,NS_BTH,0,(GUID*)&NsId);
}

void sdp_UnLoadWSANameSpaceProvider(void) {
	if (!g_hWS2Lib)
		return;

	PFN_WSCUNINSTALLNAMESPACE pfnUnInstall = (PFN_WSCUNINSTALLNAMESPACE) GetProcAddress(g_hWS2Lib,L"WSCUninstallNameSpace");
	if (!pfnUnInstall) {
		SVSUTIL_ASSERT(0);
		return;
	}

	pfnUnInstall((GUID*)&NsId);
	FreeLibrary(g_hWS2Lib);

	g_hWS2Lib                = NULL;
	g_fNameSpaceInstalled    = FALSE;
}
#endif // UNDER_CE
