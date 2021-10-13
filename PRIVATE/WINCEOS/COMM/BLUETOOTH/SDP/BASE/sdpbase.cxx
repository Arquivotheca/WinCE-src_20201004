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
//      Bluetooth SDP Layer
// 
// 
// Module Name:
// 
//      sdpbase.cxx
// 
// Abstract:
// 
//      This file implements SDP layer.
// 
// 
//------------------------------------------------------------------------------


#include "common.h"
#include <svsutil.hxx>
#include <bt_debug.h>


#define CALL_SDP_SERVICE_SEARCH             0x0001
#define CALL_SDP_ATTRIBUTE_SEARCH           0x0002
#define CALL_SDP_SERVICE_ATTRIBUTE_SEARCH   0x0004
#define CALL_SDP_ACCEPT                     0x0008
#define CALL_SDP_CONNECT                    0x0010
#define CALL_SDP_DISCONNECT                 0x0020
#define CALL_SDP_SERVER_WORKER              0x0040

#define CALL_SDP_SEARCH                    (CALL_SDP_SERVICE_SEARCH | CALL_SDP_ATTRIBUTE_SEARCH | CALL_SDP_SERVICE_ATTRIBUTE_SEARCH)
#define CALL_SDP_ACCEPT_CONNECT            (CALL_SDP_ACCEPT | CALL_SDP_CONNECT)
#define CALL_SDP_ACCEPT_SEARCH             (CALL_SDP_SEARCH | CALL_SDP_ACCEPT)
#define CALL_SDP_SEARCH_WORKER             (CALL_SDP_SEARCH | CALL_SDP_SERVER_WORKER)

#define	NONE			0x00
#define CONNECTED		0x01
#define CONFIG_REQ_DONE 0x02
#define CONFIG_IND_DONE 0x04
#define UP              0x07
#define LINK_ERROR		0x80

#define DEFAULT_IN_MTU_SIZE   0x1000
#define MAX_MTU_SIZE          0xFFFF

#define SDP_SCALE		                10

enum SDP_STAGE {
	JustCreated			= 0,
	Initializing,
	Connected,
	Disconnected,
	ShuttingDown,
	Error
};

#if 0
// Currently no registry settings are available for SDP.
#define SDP_DEFAULT_LINK_TIMEOUT_MS     10000

static const WCHAR g_SdpBaseRegKey[] = L"software\\Microsoft\\bluetooth\\l2cap";
static const WCHAR g_SdpLinkIdle[]   = L"SdpLinkIdle";
#endif

struct SDP_CONTEXT : public SVSAllocClass, public SVSRefObj {
	SDP_CONTEXT	           *pNext;
	SDP_CALLBACKS	       c;
	SDP_EVENT_INDICATION   ei;

	void *pUserContext;

	SDP_CONTEXT(void)  {
		memset(&ei, 0, sizeof(ei));
		memset(&c, 0, sizeof(c));
		pNext = NULL;
	}
};

struct Call;
struct CallQueue { 
	struct CallQueue  *pNext;
	Call              *pCall;
};

struct Link : public SVSAllocClass, public SVSRefObj {
	Link			*pNext;

	BD_ADDR			b;
	unsigned short	psm;
	unsigned short	cid;

	unsigned int	fStage;

	unsigned int	fIncoming : 1;
	unsigned int    fCallCanceled : 1;

	unsigned short	inMTU;
	unsigned short	outMTU;

	CallQueue       *pCallQueue;
};

struct Call : public SVSAllocClass {
	Call			*pNext;
	Link            *pLink;

	unsigned int	fWhat			: 8;

	SDP_CONTEXT     *pOwner;
	void            *pCallContext;
	SdpConnection   *pSdpConnection;
};

static int sdp_ConfigInd(void *pUserContext, unsigned char id, unsigned short cid, unsigned short usOutMTU, unsigned short usInFlushTO, struct btFLOWSPEC *pInFlow, int cOptNum, struct btCONFIGEXTENSION **ppExtendedOptions);
static int sdp_ConnectInd(void *pUserContext, BD_ADDR *pba, unsigned short cid, unsigned char id, unsigned short psm);
static int sdp_DataUpInd(void *pUserContext, unsigned short cid, BD_BUFFER *pBuffer);
static int sdp_DisconnectInd(void *pUserContext, unsigned short cid, int iErr);
static int sdp_lStackEvent(void *pUserContext, int iEvent, void *pEventContext);

static int sdp_lCallAborted(void *pCallContext, int iError);
static int sdp_ConfigReq_Out(void *pCallContext, unsigned short usResult, unsigned short usInMTU, unsigned short usOutFlushTO, struct btFLOWSPEC *pOutFlow, int cOptNum, struct btCONFIGEXTENSION **ppExtendedOptions);
static int sdp_ConfigResponse_Out(void *pCallContext, unsigned short result);
static int sdp_ConnectReq_Out(void *pCallContext, unsigned short cid, unsigned short result, unsigned short status);
static int sdp_ConnectResponse_Out(void *pCallContext, unsigned short result);
static int sdp_DataDown_Out(void *pCallContext, unsigned short result);
static int sdp_Disconnect_Out(void *pCallContext, unsigned short result);

static int sdp_InquiryComplete(void *pUserContext, void *pCallContext, unsigned char status, unsigned char num_responses);
static int sdp_InquiryResult(void *pUserContext, void *pCallContext, BD_ADDR *pba, unsigned char page_scan_repetition_mode_list, unsigned char page_scan_period_mode, unsigned char page_scan_mode, unsigned int class_of_device, unsigned short clock_offset);
static int sdp_RemoteNameDone(void *pUserContext, void *pCallContext, unsigned char status, BD_ADDR *pba, unsigned char utf_name[248]);
static int sdp_hStackEvent(void *pUserContext, int iEvent, void *pEventContext);

static int sdp_hCallAborted(void *pCallContext, int iError);
static int sdp_LocalNameDone(void *pCallContext, unsigned char status);
static int sdp_Inquiry_Out(void *pCallContext, unsigned char status);
static int sdp_BDADDR_Out(void *pCallContext, unsigned char status, BD_ADDR *pba);
static int sdp_RemoteName_Out(void *pCallContext, unsigned char status);
static int sdp_WriteScan_Out(void *pCallContext, unsigned char status);

static int SdpDisconnect(Link *pLink, Call *pCall);
static void DeleteCall (Call *pCall);

static int MapStatusToErrorCode(int status);

class SDP : public SVSSynch, public SVSRefObj {
public:
	Link			*pLinks;
	Call			*pCalls;
	SDP_CONTEXT     *pContexts;
	
	HANDLE			hL2CAP;
	L2CAP_INTERFACE	l2cap_if;

	FixedMemDescr	*pfmdLinks;
	FixedMemDescr	*pfmdCalls;
	FixedMemDescr   *pfmdSdpConnections;
	FixedMemDescr   *pfmdCallQueue;
	FixedMemDescr   *pfmdBuffers;

//	DWORD           dwSdpIdle;

	SDP_STAGE       eStage;
	BD_ADDR			b;

	int             cDataHeaders;
	int             cDataTrailers;

	void ReInit(void)  {
		pLinks       = NULL;
		pCalls       = NULL;
		pContexts    = NULL;

		eStage       = JustCreated;

		hL2CAP       = NULL;
		memset(&l2cap_if, 0, sizeof(l2cap_if));

		pfmdLinks          = NULL;
		pfmdCalls          = NULL;
		pfmdSdpConnections = NULL;
		pfmdCallQueue      = NULL;
		pfmdBuffers        = NULL;

		memset (&b, 0, sizeof(b));
		cDataHeaders = cDataTrailers = 0;
	}

	SDP(void) {
		ReInit();
	}

	int IsStackRunning (void) {
		return (eStage == Connected) || (eStage == Disconnected);
	}
};

SDP           *gpSDP           = NULL;
SdpDatabase   *pSdpDB          = NULL;
unsigned char *gpSdpReadBuffer = NULL;



//******************************************************************
// Link/call/notification helper functions.
//******************************************************************

static SDP_CONTEXT * VerifyContext(SDP_CONTEXT *pContext) {
	SVSUTIL_ASSERT(gpSDP->IsLocked());

	SDP_CONTEXT *pTrav = gpSDP->pContexts;
	while (pTrav &&(pTrav != pContext))
		pTrav = pTrav->pNext;

	return pTrav;
}

static SDP *CreateNewSDP(void) {
	return new SDP;
}

static SdpDatabase *CreateNewSdpDatabase(void) {
	return new SdpDatabase;
}

static Call *VerifyCall(Call *pCall, unsigned char fWhat=0) {
	Call *p = gpSDP->pCalls;
	while (p && (p != pCall))
		p = p->pNext;

	// Make sure call type is appropriate for given context.
	if (p && fWhat && ! (p->fWhat & fWhat))  {
		SVSUTIL_ASSERT(0);
		return NULL;
	}

	return p;
}

static Link *VerifyLink(Link *pLink) {
	Link *p = gpSDP->pLinks;
	while (p && (p != pLink))
		p = p->pNext;

	return p;
}

static Call *AllocCall(int fWhat, Link *pLink, SDP_CONTEXT *pOwner, void *pCallContext) {
	Call *pCall = (Call *)svsutil_GetFixed(gpSDP->pfmdCalls);
	if (!pCall)
		return NULL;

	memset(pCall, 0, sizeof(*pCall));

	pCall->pNext = gpSDP->pCalls;
	gpSDP->pCalls = pCall;

	pCall->pLink         = pLink;
	pCall->fWhat         = fWhat;
	pCall->pOwner        = pOwner;
	pCall->pCallContext  = pCallContext;

	return pCall;
}

static Link* AllocLink(void)  {
	Link *pLink = (Link *)svsutil_GetFixed(gpSDP->pfmdLinks);
	if (!pLink)
		return NULL;

	memset(pLink, 0, sizeof(*pLink));

	pLink->AddRef();
	pLink->psm = PSM_SDP;
	return pLink;
}

static SdpConnection* AllocSdpConnection(Call *pCall)  {
	SdpConnection *pSdpConn = (SdpConnection*) svsutil_GetFixed(gpSDP->pfmdSdpConnections);
	if (!pSdpConn)
		return NULL;

	memset(pSdpConn ,0,sizeof(*pSdpConn));
	pSdpConn->pCallContext = (void *) pCall;
	if (pCall)  {
		pCall->pSdpConnection = pSdpConn;
		pCall->pSdpConnection->outMtu = pCall->pLink->outMTU;
		pCall->pSdpConnection->inMtu  = pCall->pLink->inMTU;
	}
	return pSdpConn;
}

static CallQueue *AddItemToCallQueue(Link *pLink, Call *pCall)  {
	CallQueue *pNew  = (CallQueue *) svsutil_GetFixed(gpSDP->pfmdCallQueue);
	if (!pNew)
		return NULL;

	pNew->pNext = NULL;
	pNew->pCall = pCall;

	if (!pLink->pCallQueue)
		pLink->pCallQueue = pNew;
	else {
		CallQueue *pTrav = pLink->pCallQueue;
		while (pTrav->pNext)
			pTrav = pTrav->pNext;

		pTrav->pNext = pNew;
	}

	return pNew;
}

// need a connection struct for local requests, alloc once when first needed.
static SdpConnection *gpSpdConnectionLocal = NULL;

static SdpConnection* GetLocalSDPConn() {
	if (gpSpdConnectionLocal) {
		memset((void*)gpSpdConnectionLocal,0,sizeof(*gpSpdConnectionLocal));
		return gpSpdConnectionLocal;
	}

	gpSpdConnectionLocal = AllocSdpConnection(NULL);
	return gpSpdConnectionLocal;
}

static void RemoveCallFromSendQueue(Link *pLink, Call *pCall)  {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"RemoveCallFromSendQueue (0x%08x)\r\n",pLink));

	SVSUTIL_ASSERT(gpSDP->IsLocked());
	SVSUTIL_ASSERT(VerifyLink(pLink));

	CallQueue *pTrav   = pLink->pCallQueue;
	CallQueue *pFollow = NULL;
	
	while (pTrav && (pTrav->pCall != pCall)) { 
		pFollow = pTrav;
		pTrav =   pTrav->pNext;
	}
	if (!pTrav)
		return;

	if (pFollow)
		pFollow->pNext = pTrav->pNext;
	else  {
		SVSUTIL_ASSERT(pLink->pCallQueue->pCall == pCall);
		pLink->pCallQueue = pTrav->pNext;
	}

	svsutil_FreeFixed(pTrav, gpSDP->pfmdCallQueue);
}

void NotifyConnect(SDP_CONTEXT *pOwner, void *pContext, unsigned long status, unsigned short cid)  {
	SVSUTIL_ASSERT (VerifyContext(pOwner));

	SDP_Connect_Out pCallback = pOwner->c.sdp_Connect_Out;
	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"About to enter sdp_Connect_Out, pContext=0x%08x,status=0x%08x, cid=0x%04x\r\n",pContext,status,cid));

	pOwner->AddRef();
	gpSDP->Unlock();
	__try {
		pCallback(pContext,status,cid);
	}
	__except (1) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_Connect_Out excepted, exception code = 0x%08x\r\n",GetExceptionCode()));
	}
	gpSDP->Lock();
	pOwner->DelRef();
	
	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"Finished with sdp_Connect_Out\r\n"));
}

void NotifyDisconnect(SDP_CONTEXT *pOwner, void *pContext, unsigned long status) {
	SVSUTIL_ASSERT (VerifyContext(pOwner));

	SDP_Disonnect_Out pCallback = pOwner->c.sdp_Disconnect_Out;

	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"About to enter sdp_Disconnect_Out, pContext=0x%08x, status=0x%08x\r\n",pContext,status));

	pOwner->AddRef();
	gpSDP->Unlock();
	__try {
		pCallback(pContext,status);
	}
	__except (1) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_Disconnect_Out excepted, exception code = 0x%08x\r\n",GetExceptionCode()));
	}
	gpSDP->Lock();
	pOwner->DelRef();

	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"Finished with sdp_Disconnect_Out\r\n"));
}

void NotifyServiceSearch(SDP_CONTEXT *pOwner, void *pContext, unsigned long status, unsigned short cReturnedHandles, unsigned char *pClientBuf)  {
	SVSUTIL_ASSERT (VerifyContext(pOwner));
	SDP_ServiceSearch_Out pCallback = pOwner->c.sdp_ServiceSearch_Out;
	unsigned long *pHandles = NULL;

	if (status == ERROR_SUCCESS)
		pHandles = (unsigned long *) pClientBuf;

	pOwner->AddRef();
	gpSDP->Unlock();
	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"About to enter sdp_ServiceSearch_Out, pContext=0x%08x\r\n",pContext));
	__try {
		pCallback(pContext,status,cReturnedHandles,pHandles);
	}
	__except (1) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_ServiceSearch_Out excepted, exception code = 0x%08x\r\n",GetExceptionCode()));
	}
	gpSDP->Lock();
	pOwner->DelRef();
	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"Finished with sdp_ServiceSearch_Out\r\n"));
}

void NotifyAttributeSearch(SDP_CONTEXT *pOwner, void *pContext, unsigned long status, unsigned char *pClientBuf, unsigned long ulClientBuf)  {
	SVSUTIL_ASSERT (VerifyContext(pOwner));
	SDP_AttributeSearch_Out pCallback = pOwner->c.sdp_AttributeSearch_Out;

	if (status != ERROR_SUCCESS) {
		pClientBuf  = NULL;
		ulClientBuf = 0;
	}

	pOwner->AddRef();
	gpSDP->Unlock();
	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"About to enter sdp_AttributeSearch_Out, pContext=0x%08x\r\n",pContext));
	__try {
		pCallback(pContext,status,pClientBuf,ulClientBuf);
	}
	__except (1) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_AttributeSearch_Out excepted, exception code = 0x%08x\r\n",GetExceptionCode()));
	}
	gpSDP->Lock();
	pOwner->DelRef();
	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"Finished with sdp_AttributeSearch_Out\r\n"));
}

void NotifyServiceAttributeSearch(SDP_CONTEXT *pOwner, void *pContext, unsigned long status, unsigned char *pClientBuf, unsigned long ulClientBuf)  {
	SVSUTIL_ASSERT (VerifyContext(pOwner));
	SDP_ServiceAttributeSearch_Out pCallback = pOwner->c.sdp_ServiceAttributeSearch_Out;

	if (status != ERROR_SUCCESS) {
		pClientBuf  = NULL;
		ulClientBuf = 0;
	}

	pOwner->AddRef();
	gpSDP->Unlock();
	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"About to enter sdp_ServiceAttributeSearch_Out, pContext=0x%08x\r\n",pContext));
	__try {
		pCallback(pContext,status,pClientBuf,ulClientBuf);
	}
	__except (1) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_ServiceAttributeSearch_Out excepted, exception code = 0x%08x\r\n",GetExceptionCode()));
	}
	gpSDP->Lock();
	pOwner->DelRef();
	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"Finished with sdp_ServiceAttributeSearch_Out\r\n"));
}

void NotifySdpClientOfCompletion(Call *pCall, unsigned long status)   {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"NotifySdpClientOfCompletion(0x%08x,0x%08x)\r\n",pCall,status));

	SVSUTIL_ASSERT(gpSDP->IsLocked());
	SVSUTIL_ASSERT(VerifyCall(pCall));
	void           *pContext = pCall->pCallContext;
	SDP_CONTEXT    *pOwner   = pCall->pOwner;

	unsigned char *pClientBuf       = (pCall->pSdpConnection  && status == ERROR_SUCCESS) ? pCall->pSdpConnection->pClientBuf : NULL;
	unsigned long  cClientBuf       = (pCall->pSdpConnection  && status == ERROR_SUCCESS) ? pCall->pSdpConnection->cClientBuf : 0;
	unsigned short cReturnedHandles = (pCall->pSdpConnection  && status == ERROR_SUCCESS) ? pCall->pSdpConnection->u.Client.ServiceSearch.totalRecordCount : 0;
	unsigned short cid              = (pCall->pLink           && status == ERROR_SUCCESS) ? pCall->pLink->cid : 0;
	unsigned int   fWhat            = pCall->fWhat;

#if defined (DEBUG) || defined (_DEBUG)
	if ((status == ERROR_SUCCESS) && (fWhat == CALL_SDP_CONNECT))
		SVSUTIL_ASSERT( VerifyLink(pCall->pLink));

	SVSUTIL_ASSERT(!pCall->pSdpConnection || (pCall->pSdpConnection != gpSpdConnectionLocal && !pCall->pSdpConnection->fClientNotified));
	if (pCall->pSdpConnection)
		pCall->pSdpConnection->fClientNotified = TRUE;
#endif

	DeleteCall(pCall);

	switch (fWhat) {
		case CALL_SDP_ACCEPT:
		case CALL_SDP_SERVER_WORKER:
			break;  // no-op, call was generated internally.

		case CALL_SDP_CONNECT:
			NotifyConnect(pOwner,pContext,status,cid);
		break;

		case CALL_SDP_DISCONNECT:
			NotifyDisconnect(pOwner,pContext,status);
		break;
		
		case CALL_SDP_SERVICE_SEARCH:
			NotifyServiceSearch(pOwner,pContext,status,cReturnedHandles,pClientBuf);
		break;

		case CALL_SDP_ATTRIBUTE_SEARCH:
			NotifyAttributeSearch(pOwner,pContext,status,pClientBuf,cClientBuf);
		break;

		case CALL_SDP_SERVICE_ATTRIBUTE_SEARCH:
			NotifyServiceAttributeSearch(pOwner,pContext,status,pClientBuf,cClientBuf);
		break;
		
		default:
			SVSUTIL_ASSERT(0);
	}
}

static void ResetSdpConnection(SdpConnection *pSdpConn)  {
	if (pSdpConn == gpSpdConnectionLocal || pSdpConn->IsClient())  {
		pSdpConn->CompleteClientRequest(0,ERROR_SUCCESS);
	}
	else {
		pSdpConn->CleanUpServerSearchResults();
	}

	pSdpConn->ConnInformation = 0;
	pSdpConn->pClientBuf      = NULL;
	pSdpConn->cClientBuf      = 0;

#if defined (DEBUG) || defined (_DEBUG)
	// If this ASSERT is hit it means we didn't notify client of completion, 
	// meaning that there's a hanging call and that we'll end up leaking the memory.
	if ((pSdpConn != gpSpdConnectionLocal) && pSdpConn->IsClient() && (!pSdpConn->fClientNotified))
		SVSUTIL_ASSERT(0);

	pSdpConn->fClientNotified = FALSE;
#endif
}

static void DeleteSdpConnection(SdpConnection *pSdpConn)  {
	ResetSdpConnection(pSdpConn);
	svsutil_FreeFixed(pSdpConn, gpSDP->pfmdSdpConnections);
}

void RemoveAllPendingCallsFromQueue(Link *pLink, unsigned long status);

// This call just frees up link and removes it from list, assumes upper layer has already disconnected.
static void DeleteLink(Link *pLink) {
	SVSUTIL_ASSERT(gpSDP->IsLocked());
	RemoveAllPendingCallsFromQueue(pLink,ERROR_CONNECTION_UNAVAIL);

	// Possible another thread has deleted link in interim.
	if (! VerifyLink(pLink))
		return;

	if (pLink == gpSDP->pLinks)
		gpSDP->pLinks = pLink->pNext;
	else {
		Link *pParent = gpSDP->pLinks;
		while (pParent && (pParent->pNext != pLink))
			pParent = pParent->pNext;

		if (pParent)
			pParent->pNext = pLink->pNext;
	}

	svsutil_FreeFixed(pLink, gpSDP->pfmdLinks);
}

static void DeleteCall (Call *pCall) {
	SVSUTIL_ASSERT(gpSDP->IsLocked());
	SVSUTIL_ASSERT(VerifyCall(pCall));
	IFDBG(DebugOut (DEBUG_SDP_TRACE,L"DeleteCall (0x%08x)\r\n",pCall));

	unsigned int fWhat  = pCall->fWhat;
	Link         *pLink = pCall->pLink;

	if (pCall->fWhat & CALL_SDP_SEARCH_WORKER && VerifyLink(pCall->pLink))
		RemoveCallFromSendQueue(pLink,pCall);

	if (pCall == gpSDP->pCalls)
		gpSDP->pCalls = pCall->pNext;
	else {
		Call *pParent = gpSDP->pCalls;
		while (pParent && (pParent->pNext != pCall))
			pParent = pParent->pNext;

		if (pParent)
			pParent->pNext = pCall->pNext;
	}

	if (pCall->pSdpConnection)
		DeleteSdpConnection(pCall->pSdpConnection);

	svsutil_FreeFixed(pCall, gpSDP->pfmdCalls);
}

void RemoveAllPendingCallsFromQueue(Link *pLink, unsigned long status=0)  {
	while (VerifyLink(pLink) && pLink->pCallQueue) {
		SVSUTIL_ASSERT(pLink->pCallQueue->pCall->fWhat & CALL_SDP_SEARCH_WORKER);
		NotifySdpClientOfCompletion(pLink->pCallQueue->pCall,status);
	}
}

static Link *FindLink (unsigned short cid) {
	Link *p = gpSDP->pLinks;
	while (p && (p->cid != cid))
		p = p->pNext;
	return p;
}

static Link *FindLink(BD_ADDR *pba, BOOL fClient)  {
	Link *pTrav = gpSDP->pLinks;
	while (pTrav)   {
		if (pTrav->b == *pba)  {
			if ( (fClient && !pTrav->fIncoming) || (!fClient && pTrav->fIncoming))
				return pTrav;
		}
		pTrav = pTrav->pNext;
	}
	return pTrav;
}

static Call *FindCall(Link *pLink, unsigned long fWhat=0xFFFFFFFF)  {
	Call *pTrav;

	for (pTrav = gpSDP->pCalls; pTrav; pTrav = pTrav->pNext)  {
		if ((pTrav->pLink == pLink) && (pTrav->fWhat & fWhat))
			return pTrav;
	}
	return NULL;
}

#if defined (DEBUG)	|| defined (_DEBUG)
static Call *FindAcceptConnectCall(Link *pLink)  {
	Call *pTrav;

	for (pTrav = gpSDP->pCalls; pTrav; pTrav = pTrav->pNext)  {
		if ((pTrav->pLink == pLink) && (pTrav->fWhat & CALL_SDP_ACCEPT_CONNECT))
			return pTrav;
	}
	return NULL;
}
#endif // DEBUG


static void CleanupSDPState(void) {
	if (gpSDP->pfmdSdpConnections)
		svsutil_ReleaseFixedNonEmpty(gpSDP->pfmdSdpConnections);
	if (gpSDP->pfmdLinks)
		svsutil_ReleaseFixedNonEmpty(gpSDP->pfmdLinks);
	if (gpSDP->pfmdCalls)
		svsutil_ReleaseFixedNonEmpty(gpSDP->pfmdCalls);
	if (gpSDP->pfmdCallQueue)
		svsutil_ReleaseFixedNonEmpty(gpSDP->pfmdCallQueue);
	if (gpSDP->pfmdBuffers)
		svsutil_ReleaseFixedNonEmpty(gpSDP->pfmdBuffers);

	gpSDP->ReInit();
}


static void GetConnectionState (void) {
	SVSUTIL_ASSERT(gpSDP->eStage == Connected || gpSDP->eStage == Disconnected);

	__try {
		int fConnected = FALSE;
		int dwRet = 0;
		gpSDP->l2cap_if.l2ca_ioctl (gpSDP->hL2CAP, BTH_STACK_IOCTL_GET_CONNECTED, 0, NULL, sizeof(fConnected), (char *)&fConnected, &dwRet);
		if ((dwRet == sizeof(fConnected)) && fConnected)
			gpSDP->eStage = Connected;
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[SDP]: exception in hci_ioctl BTH_STACK_IOCTL_GET_CONNECTED\n"));
	}
}


static void DispatchStackEvent (int iEvent) {
	SDP_CONTEXT *pContext = gpSDP->pContexts;
	while (pContext && gpSDP->IsStackRunning()) {
		BT_LAYER_STACK_EVENT_IND pCallback = pContext->ei.sdp_StackEvent;
		if (pCallback) {
			void *pUserContext = pContext->pUserContext;

			IFDBG(DebugOut (DEBUG_SDP_CALLBACK, L"Going into StackEvent notification\n"));
			pContext->AddRef ();
			gpSDP->Unlock ();

			__try {
				pCallback (pUserContext, iEvent, NULL);
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"[SDP] SDP_connect_transport: exception in SDP_StackEvent!\n"));
			}

			gpSDP->Lock ();
			pContext->DelRef ();
			IFDBG(DebugOut (DEBUG_SDP_CALLBACK, L"SDP: Came back StackEvent notification\n"));
		}
		pContext = pContext->pNext;
	}
}

static DWORD WINAPI StackDisconnect(LPVOID lpVoid) {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"SDP: StackDisconnect()\r\n"));
	sdp_CloseDriverInstance();
	return ERROR_SUCCESS;
}

static DWORD WINAPI StackDown (LPVOID pArg) {
	IFDBG(DebugOut (DEBUG_SDP_TRACE, L"SDP: Disconnect stack\n"));

	for ( ; ; ) {
		if (! gpSDP) {
			IFDBG(DebugOut (DEBUG_ERROR, L"SDP: StackDown : ERROR_SERVICE_DOES_NOT_EXIST\n"));
			return ERROR_SERVICE_DOES_NOT_EXIST;
		}
		gpSDP->Lock ();

		if (gpSDP->eStage != Connected) {
			IFDBG(DebugOut (DEBUG_ERROR, L"SDP: StackDown : ERROR_SERVICE_NOT_ACTIVE\n"));
			gpSDP->Unlock ();
			return ERROR_SERVICE_NOT_ACTIVE;
		}

		if (gpSDP->GetRefCount () == 1)
			break;

		IFDBG(DebugOut (DEBUG_SDP_TRACE, L"SDP: Waiting for ref count in StackDown\n"));
		gpSDP->Unlock ();
		Sleep (100);
	}
	SVSUTIL_ASSERT(gpSDP->IsLocked());  // break out of above for loop with lock held.

	gpSDP->AddRef ();
	gpSDP->eStage = Disconnected;

	while (gpSDP->pCalls && gpSDP->IsStackRunning())
		NotifySdpClientOfCompletion (gpSDP->pCalls, ERROR_SHUTDOWN_IN_PROGRESS);

	while (gpSDP->pLinks && gpSDP->IsStackRunning()) {
		SVSUTIL_ASSERT(gpSDP->pLinks->pCallQueue == NULL);
		SdpDisconnect(gpSDP->pLinks,NULL);
	}

	DispatchStackEvent (BTH_STACK_DOWN);
	gpSDP->DelRef ();

	IFDBG(DebugOut (DEBUG_SDP_TRACE, L"SDP: -StackDown\n"));
	gpSDP->Unlock ();

	return ERROR_SUCCESS;
}

static DWORD WINAPI StackUp (LPVOID pArg) {
	IFDBG(DebugOut (DEBUG_SDP_TRACE, L"Connect stack\n"));

	for ( ; ; ) {
		if (! gpSDP) {
			IFDBG(DebugOut (DEBUG_ERROR, L"StackUp : ERROR_SERVICE_DOES_NOT_EXIST\n"));
			return ERROR_SERVICE_DOES_NOT_EXIST;
		}
		gpSDP->Lock ();

		if (gpSDP->eStage != Disconnected) {
			IFDBG(DebugOut (DEBUG_ERROR, L"StackUp : ERROR_SERVICE_NOT_ACTIVE\n"));
			gpSDP->Unlock ();
			return ERROR_SERVICE_NOT_ACTIVE;
		}
		if (gpSDP->GetRefCount () == 1)
			break;

		IFDBG(DebugOut (DEBUG_SDP_TRACE, L"Waiting for ref count in StackUp\n"));
		gpSDP->Unlock ();
		Sleep (100);
	}

	GetConnectionState ();

	if (gpSDP->eStage == Connected) {
		gpSDP->AddRef ();
		DispatchStackEvent (BTH_STACK_UP);
		gpSDP->DelRef ();
	}

	IFDBG(DebugOut (DEBUG_SDP_TRACE, L"-StackUp\n"));
	gpSDP->Unlock ();

	return ERROR_SUCCESS;
}

static DWORD WINAPI StackReset (LPVOID pArg) {
	IFDBG(DebugOut (DEBUG_SDP_TRACE, L"SDP: Reset stack\n"));
	StackDown (NULL);
	StackUp (NULL);
	return NULL;
}

static int SendData(Call *pCall, unsigned char *pData, int cData)  {
	SVSUTIL_ASSERT(gpSDP->eStage == Connected);
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"SendData(0x%08x,0x%08x,%d)\r\n",pCall,pData,cData));
	IFDBG(DumpBuff(DEBUG_SDP_PACKETS,pData,cData));

	int       iRet   = ERROR_INTERNAL_ERROR;
	BD_BUFFER *pBuf;

	L2CA_DataDown_In pCallBack = gpSDP->l2cap_if.l2ca_DataDown_In;
	HANDLE hL2CAP = gpSDP->hL2CAP;
	unsigned short usCID = pCall->pLink->cid;

	if (NULL == (pBuf = BufferAlloc(cData + gpSDP->cDataHeaders + gpSDP->cDataTrailers)))
		return ERROR_OUTOFMEMORY;
	pBuf->cStart = gpSDP->cDataHeaders;

	memcpy(pBuf->pBuffer + pBuf->cStart,pData,cData);
	pBuf->cEnd = pBuf->cStart + cData;
	SVSUTIL_ASSERT(pBuf->cEnd + gpSDP->cDataTrailers == pBuf->cSize);
	SVSUTIL_ASSERT(cData == pBuf->cSize - gpSDP->cDataHeaders - gpSDP->cDataTrailers);
	SVSUTIL_ASSERT(cData <= pCall->pLink->outMTU);
	SVSUTIL_ASSERT(!pCall->pLink->fCallCanceled);

	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"Going into l2ca_DataDown_In\r\n"));
	gpSDP->AddRef();
	gpSDP->Unlock();
	__try {
		iRet = gpSDP->l2cap_if.l2ca_DataDown_In(hL2CAP,(void*)pCall,usCID,pBuf);
	} __except(1) 	{
		IFDBG(DebugOut(DEBUG_ERROR,L"Exception in l2cap_DataDown_In\r\n"));
	}

	gpSDP->Lock();
	gpSDP->DelRef();
	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"Came from l2ca_DataDown_In\r\n"));
	return iRet;
}


static int ProcessNextCall(Link *pLink)  {
	int iErr;

	if (!pLink->pCallQueue)  {
		IFDBG(DebugOut(DEBUG_SDP_PACKETS,L"ProcessNextCall: No pending calls in wait queue for pLink->cid=0x%04x\r\n",pLink->cid));
		return ERROR_SUCCESS;
	}

	while (VerifyLink(pLink) && pLink->pCallQueue)  {
		Call *pCall = pLink->pCallQueue->pCall;

		IFDBG(DebugOut(DEBUG_SDP_PACKETS,L"ProcessNextCall: About to process call 0x%08x\r\n",pLink->pCallQueue->pCall));
		if (ERROR_SUCCESS != (iErr = pCall->pSdpConnection->SendInitialRequestToServer())) {
			if (VerifyCall(pCall))
				NotifySdpClientOfCompletion(pCall,iErr);
			continue;				
		}
		return ERROR_SUCCESS;
	}
	return iErr;
}


void CreateCallForServer(Link *pLink) {
	Call *pNewCall = AllocCall(CALL_SDP_SERVER_WORKER,pLink,NULL,NULL);
	SdpConnection *pSdpConn = pNewCall ? AllocSdpConnection(pNewCall) : NULL;

	if (!pNewCall || !pSdpConn || !AddItemToCallQueue(pLink,pNewCall)) {
		if (pNewCall)
			DeleteCall(pNewCall);

		if (VerifyLink(pLink))
			SdpDisconnect(pLink,NULL);
	}
}


//******************************************************************
// L2Cap Interface Layer.
//******************************************************************

static int sdp_DataUpInd(void *pUserContext, unsigned short cid, BD_BUFFER *pBuffer) {
	int           iSize;
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_DataUpInd(0x%08x,0x%04x,0x%08x)\r\n",pUserContext,cid,pBuffer));

	if (! gpSDP) {
		if (! pBuffer->fMustCopy) 
			pBuffer->pFree(pBuffer);
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpSDP->Lock();
	if (gpSDP->eStage != Connected) {
		if (! pBuffer->fMustCopy) 
			pBuffer->pFree(pBuffer);

		IFDBG(DebugOut(DEBUG_WARN,L"sdp_DataUpInd:: ERROR_SERVICE_NOT_ACTIVE\r\n"));
		gpSDP->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	iSize = BufferTotal(pBuffer);
	BufferGetChunk(pBuffer,iSize,gpSdpReadBuffer);
	IFDBG(DumpBuff(DEBUG_SDP_PACKETS, gpSdpReadBuffer, iSize));

	if (! pBuffer->fMustCopy)
		pBuffer->pFree(pBuffer);

	Link *pLink = FindLink(cid);
	if (! pLink) {
		IFDBG(DebugOut(DEBUG_WARN,L"sdp_DataUpInd:: cid 0x%04x not found\r\n",cid));
		gpSDP->Unlock();
		return ERROR_NOT_FOUND;
	}

	Call *pCall = (!pLink->fCallCanceled) ? pLink->pCallQueue->pCall : NULL;
	if (! pCall) {
		// This will occur if the call has been canceled by the client.
		SVSUTIL_ASSERT(!pLink->fIncoming);
		IFDBG(DebugOut(DEBUG_SDP_PACKETS,L"sdp_DataUpInd:: pCall 0x%08x not found\r\n",pCall));

		// Send off next request in call queue if there is one.
		pLink->fCallCanceled = 0;
		ProcessNextCall(pLink);

		gpSDP->Unlock();
		return ERROR_NOT_FOUND;
	}
	SVSUTIL_ASSERT(pCall->fWhat & CALL_SDP_SEARCH_WORKER);
	
	SdpConnection *pSdpConn = pCall->pSdpConnection;
	unsigned long status    = pSdpConn->ReadWorkItemWorker(gpSdpReadBuffer,iSize);

	if (!pLink->fIncoming && status != STATUS_PENDING) {
		NotifySdpClientOfCompletion(pCall,MapStatusToErrorCode(status));
		if (VerifyLink(pLink))
			ProcessNextCall(pLink);
	}
	else if (pLink->fIncoming && status != ERROR_SUCCESS) {
		ResetSdpConnection(pCall->pSdpConnection);
	}

	gpSDP->Unlock();
	return ERROR_SUCCESS;
}

static int sdp_DisconnectInd(void *pUserContext, unsigned short cid, int iErr) {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_DisconnectInd(0x%08x,0x%04x)\r\n",pUserContext,cid));

	if (! gpSDP)
		return ERROR_SERVICE_NOT_ACTIVE;

	gpSDP->Lock();
	if (gpSDP->eStage != Connected) {
		gpSDP->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	Link *pLink = FindLink(cid);
	if (!pLink)  {
		IFDBG(DebugOut(DEBUG_SDP_PACKETS,L"sdp_DisconnectInd: cid=0x%04x doesn't have corresponding SDP connection\r\n",cid));
		gpSDP->Unlock();
		return ERROR_NOT_FOUND;
	}

	// The call associated with creating the link should've been deleted already.
	DeleteLink(pLink);

	gpSDP->Unlock();
	return ERROR_SUCCESS;
}

static int sdp_lStackEvent(void *pUserContext, int iEvent, void *pEventContext) {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_lStackEvent(0x%08x,%d,0x%08x)\r\n",pUserContext,iEvent,pEventContext));

	switch (iEvent) {
	case BTH_STACK_NONE:
	case BTH_STACK_HOST_BUFFER:
		IFDBG(DebugOut (DEBUG_ERROR, L"Unexpected event - none or host buffer. Doing nothing\n"));
		break;

	case BTH_STACK_RESET:
		IFDBG(DebugOut (DEBUG_SDP_INIT, L"SDP : Stack reset\n"));
		btutil_ScheduleEvent (StackReset, NULL);
		break;

	case BTH_STACK_DOWN:
		IFDBG(DebugOut (DEBUG_SDP_INIT, L"SDP : Stack down\n"));
		btutil_ScheduleEvent (StackDown, NULL);
		break;

	case BTH_STACK_UP:
		IFDBG(DebugOut (DEBUG_SDP_INIT, L"SDP : Stack up\n"));
		btutil_ScheduleEvent (StackUp, NULL);
		break;

	case BTH_STACK_DISCONNECT:
		IFDBG(DebugOut (DEBUG_SDP_INIT, L"SDP : Stack disconnect\n"));
		btutil_ScheduleEvent (StackDisconnect, NULL);
		break;

	case BTH_STACK_FLOW_ON:
		IFDBG(DebugOut (DEBUG_SDP_INIT, L"SDP: Somebody turned flow on. Not supported. Disconnecting\n"));
		btutil_ScheduleEvent (StackDown, NULL);
		break;

	case BTH_STACK_FLOW_OFF:
		IFDBG(DebugOut (DEBUG_SDP_INIT, L"SDP : HCI Flow back off. Turning stack back on.\n"));
		btutil_ScheduleEvent (StackUp, NULL);
		break;

	default:
		IFDBG(DebugOut (DEBUG_ERROR, L"SDP: unknown stack event. Disconnecting out of paranoia.\n"));
		btutil_ScheduleEvent (StackDown, NULL);
	}

	return ERROR_SUCCESS;
}

static int sdp_lCallAborted(void *pCallContext, int iError) {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_lCallAborted(0x%08x,%d)\r\n",pCallContext,iError));

	if (! gpSDP)
		return ERROR_SERVICE_NOT_ACTIVE;

	gpSDP->Lock();

	if (!gpSDP->IsStackRunning()) {
		gpSDP->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	Call *pCall = VerifyCall((Call *)pCallContext);
	if (! pCall) {
		gpSDP->Unlock();
		return ERROR_NOT_FOUND;
	}

	if (pCall->fWhat & CALL_SDP_SEARCH)  {
		SVSUTIL_ASSERT(VerifyLink (pCall->pLink));
		Link *pLink = pCall->pLink;

		// If the call is currently pending then when we get a response it'll be
		// to this call (and item that'll be on head of queue at response time).
		if (pLink->pCallQueue && pLink->pCallQueue->pCall==pCall)  {
			IFDBG(DebugOut(DEBUG_SDP_PACKETS,L"Call (0x%08x) aborted is outstanding on link (0x%08x), further sends will pend it's response\r\n",pCall,pLink));
			pLink->fCallCanceled = 1;
		}
	}

	// During connection it's possible mutliple calls are pointing to same link.  Remove all of them.
	if (pCall->fWhat == CALL_SDP_CONNECT)  {
		Call *pTrav = gpSDP->pCalls;
		Link *pLink = pCall->pLink;  // save in case memory goes away during NotifySdpClientOfCompletion release of CritSec.

		while (pTrav)  {
			if ((pTrav != pCall) && (pTrav->pLink == pLink))  {
				SVSUTIL_ASSERT(pTrav->fWhat == CALL_SDP_CONNECT);
				NotifySdpClientOfCompletion(pTrav,iError);
				// since we gave up CritSec in above call list may have changed, start @ beginning.
				pTrav = gpSDP->pCalls;
				continue;
			}
			pTrav = pTrav->pNext;
		}
	}

	if (VerifyCall(pCall)) {
		if ((pCall->fWhat & CALL_SDP_ACCEPT_CONNECT) && VerifyLink(pCall->pLink))
			SdpDisconnect(pCall->pLink,NULL);

		if (VerifyCall(pCall))
			NotifySdpClientOfCompletion(pCall,iError);
	}
	
	gpSDP->Unlock();
	return ERROR_SUCCESS;
}

static int sdp_ConfigInd(void *pUserContext, unsigned char id, unsigned short cid, unsigned short usOutMTU, unsigned short usInFlushTO, struct btFLOWSPEC *pInFlow, int cOptNum, struct btCONFIGEXTENSION **ppExtendedOptions) {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_ConfigInd(0x%08x,%c,%d,%d,%d)\r\n",pUserContext,id,cid,usOutMTU,usInFlushTO));
	if (! gpSDP)
		return ERROR_SERVICE_NOT_ACTIVE;

	gpSDP->Lock();

	if (gpSDP->eStage != Connected) {
		gpSDP->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	Link *pLink = FindLink(cid);
	Call *pCall = pLink ? FindCall(pLink) : NULL;
	if (! pCall)  {
		IFDBG(DebugOut(DEBUG_SDP_PACKETS,L"sdp_ConfigInd: can't find link 0x%04x, returns ERROR_NOT_FOUND\r\n",cid));
		gpSDP->Unlock();
		return ERROR_NOT_FOUND;
	}

	SVSUTIL_ASSERT(pLink == pCall->pLink);
	SVSUTIL_ASSERT(pCall->fWhat & CALL_SDP_ACCEPT_CONNECT);

	int fAccept = FALSE;

	if ((usInFlushTO == 0xffff) && (! pInFlow) && btutil_VerifyExtendedL2CAPOptions (cOptNum, ppExtendedOptions)) {
		pCall->pLink->fStage |= CONFIG_IND_DONE;
		pCall->pLink->outMTU = usOutMTU ? usOutMTU : L2CAP_MTU;
		fAccept = TRUE;

		if (pLink->fStage == UP) {
			IFDBG(DebugOut(DEBUG_SDP_PACKETS,L"sdp_ConfigInd:: connection complete for pCall=0x%08x\r\n",pCall));
		}
	}

	HANDLE hL2CAP = gpSDP->hL2CAP;
	L2CA_ConfigResponse_In pCallback = gpSDP->l2cap_if.l2ca_ConfigResponse_In;

	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"Going into l2ca_ConfigResponse_In\r\n"));
	gpSDP->AddRef();
	gpSDP->Unlock();

	__try {
		pCallback(hL2CAP, pCall, id, cid, fAccept ? 0 : 2, 0, 0xffff, NULL, 0, NULL);
	} __except (1) {
		IFDBG(DebugOut(DEBUG_ERROR,L"Exception in l2ca_ConfigResponse_In\r\n"));
	}

	gpSDP->Lock();
	gpSDP->DelRef();

	if (gpSDP->eStage == Connected && VerifyLink (pLink) && (pLink->fStage == UP)) {
		// There maybe multiple calls waiting for connection with this link.  Run through all of them.
		// We delete the call in NotifySdpClientOfCompletion for client case.
		if (NULL != (pCall = FindCall(pLink,CALL_SDP_ACCEPT_CONNECT))) {
			SVSUTIL_ASSERT((pCall->pLink == pLink) && VerifyCall(pCall));
			if (pLink->fIncoming)  {
				CreateCallForServer(pLink);
				DeleteCall(pCall);
			}
			else {
				NotifySdpClientOfCompletion(pCall,ERROR_SUCCESS);
			}
		}
	}
	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"Came from l2ca_ConfigResponse_In\r\n"));
	gpSDP->Unlock();
	return ERROR_SUCCESS;
}

static int sdp_ConfigReq_Out(void *pCallContext, unsigned short usResult, unsigned short usInMTU, unsigned short usOutFlushTO, struct btFLOWSPEC *pOutFlow, int cOptNum, struct btCONFIGEXTENSION **ppExtendedOptions) {
	IFDBG(DebugOut (DEBUG_SDP_TRACE,L"sdp_ConfigReq_Out(0x%08x,%d,%d,%d)\r\n",pCallContext,usResult,usInMTU,usOutFlushTO));
	if (! gpSDP)
		return ERROR_SERVICE_NOT_ACTIVE;

	gpSDP->Lock();

	if (gpSDP->eStage != Connected) {
		gpSDP->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	Call *pCall = VerifyCall((Call *)pCallContext,CALL_SDP_ACCEPT_CONNECT);
	if (! pCall)  {
		gpSDP->Unlock();
		return ERROR_NOT_FOUND;
	}

	Link *pLink = pCall->pLink;	
	SVSUTIL_ASSERT(VerifyLink(pLink));

	if (usResult == 0) {
		SVSUTIL_ASSERT(! (pLink->fStage & CONFIG_REQ_DONE));
		SVSUTIL_ASSERT(pLink->fStage & CONNECTED);
		SVSUTIL_ASSERT(pLink->cid);
		SVSUTIL_ASSERT(pLink->psm);

		pLink->fStage |= CONFIG_REQ_DONE;

		if (pLink->fStage == UP) {
			IFDBG(DebugOut(DEBUG_SDP_PACKETS,L"sdp_ConfigInd:: connection complete for pCall=0x%08x\r\n",pCall));

			// There maybe multiple calls waiting for connection with this link.  Run through all of them.
			while (NULL != (pCall = FindCall(pLink,CALL_SDP_ACCEPT_CONNECT))) {
				SVSUTIL_ASSERT(pCall->pLink == pLink && VerifyCall(pCall));
				if (pLink->fIncoming) {
					CreateCallForServer(pLink);
					DeleteCall(pCall);
				}
				else {
					NotifySdpClientOfCompletion(pCall,ERROR_SUCCESS);
				}
			}
		}
		gpSDP->Unlock();
		return ERROR_SUCCESS;
	}

	gpSDP->Unlock();
	sdp_lCallAborted(pCallContext, ERROR_CONNECTION_ABORTED);

	return ERROR_SUCCESS;
}

static int sdp_ConnectInd(void *pUserContext, BD_ADDR *pba, unsigned short cid, unsigned char id, unsigned short psm) {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_ConnectInd(0x%08x,0x%04x 0x%08x,%d,%c,%d)\r\n",pUserContext,pba->NAP,pba->SAP,cid,id,psm));

	if (! gpSDP)
		return ERROR_SERVICE_NOT_ACTIVE;

	gpSDP->Lock();
	if (gpSDP->eStage != Connected) {
		gpSDP->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	unsigned short	result;
	unsigned short	status = 0;
	unsigned short	mtu = 0;

	if (psm != PSM_SDP)  {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_ConnectInd received connect for psm = %d (not SDP PSM)\r\n",psm));
		SVSUTIL_ASSERT(0);  // 
		gpSDP->Unlock();
		return ERROR_INVALID_PARAMETER;
	}

	Call *pCall = AllocCall(CALL_SDP_ACCEPT,NULL,NULL,NULL);
	Link *pLink = pCall ? AllocLink() : NULL;

	if (pLink) {
		pCall->pLink = pLink;
		pLink->b = *pba;
		pLink->cid = cid;
		pLink->fStage = CONNECTED;
		pLink->inMTU = DEFAULT_IN_MTU_SIZE;
		pLink->outMTU = 0;
		pLink->psm = psm;
		pLink->fIncoming = TRUE;
		pLink->pNext = gpSDP->pLinks;
		gpSDP->pLinks = pLink;
		
		result = 0;
		mtu = pLink->inMTU;
	} else {
		if (pCall)
			DeleteCall(pCall);
		result = 4;
	}

	HANDLE hL2CAP = gpSDP->hL2CAP;
	L2CA_ConnectResponse_In pCallbackConnect = gpSDP->l2cap_if.l2ca_ConnectResponse_In;
	L2CA_ConfigReq_In pCallbackConfig = gpSDP->l2cap_if.l2ca_ConfigReq_In;

	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"Calling into l2ca_ConnectResponse_In\r\n"));
	gpSDP->AddRef();
	gpSDP->Unlock();

	__try {
		pCallbackConnect(hL2CAP, NULL, pba, id, cid, result, status);
	} __except (1) {
		IFDBG(DebugOut(DEBUG_ERROR,L"Exception in l2ca_ConfigReq_In\r\n"));
	}

	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"Came from l2ca_ConnectResponse_In\r\n"));

	if (result == 0) {
		int iRes = ERROR_INTERNAL_ERROR;
		IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"Calling into l2ca_ConfigReq_In\r\n"));
		
		__try {
			iRes = pCallbackConfig(hL2CAP, pCall, cid, mtu, 0xffff, NULL, 0, NULL);
		} __except (1) {
			IFDBG(DebugOut(DEBUG_ERROR,L"Exception in l2ca_ConfigReq_In\r\n"));
		}
		IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"Came from l2ca_ConfigReq_In\r\n"));
		
		if (iRes != ERROR_SUCCESS)  {
			sdp_lCallAborted(pCall, iRes);
		}
	}
	gpSDP->Lock();
	gpSDP->DelRef();
	gpSDP->Unlock();
	
	return ERROR_SUCCESS;
}

static int sdp_ConnectReq_Out(void *pCallContext, unsigned short cid, unsigned short result, unsigned short status) {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_ConnectReq_Out(0x%08x,%d,%d,%d)\r\n",cid,result,status));

	if (result)
		return sdp_lCallAborted(pCallContext,ERROR_CONNECTION_UNAVAIL);

	if (! gpSDP)
		return ERROR_SERVICE_NOT_ACTIVE;

	gpSDP->Lock();

	if (gpSDP->eStage != Connected) {
		gpSDP->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	Call *pCall = VerifyCall((Call *)pCallContext,CALL_SDP_ACCEPT_CONNECT);
	if (! pCall) {
		gpSDP->Unlock();
		return ERROR_NOT_FOUND;
	}
	SVSUTIL_ASSERT(VerifyLink(pCall->pLink));

	Link *pLink = pCall->pLink;

	SVSUTIL_ASSERT(pLink->fStage == NONE);
	SVSUTIL_ASSERT(! pLink->cid);
	SVSUTIL_ASSERT(pLink->psm);

	pLink->fStage = CONNECTED;
	pLink->cid = cid;

	unsigned short mtu = pLink->inMTU;

	HANDLE hL2CAP = gpSDP->hL2CAP;
	L2CA_ConfigReq_In pCallback = gpSDP->l2cap_if.l2ca_ConfigReq_In;

	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"Calling into l2ca_ConfigReq_In\r\n"));
	gpSDP->AddRef();
	gpSDP->Unlock();

	int iRes = ERROR_INTERNAL_ERROR;
	__try {
		iRes = pCallback(hL2CAP, pCallContext, cid, mtu, 0xffff, NULL, 0, NULL);
	} __except (1) {
		IFDBG(DebugOut(DEBUG_ERROR,L"Exception in l2ca_ConfigReq_In\r\n"));
	}

	if (iRes != ERROR_SUCCESS)
		sdp_lCallAborted(pCallContext, iRes);

	gpSDP->Lock();
	gpSDP->DelRef();

	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"Came from l2ca_ConfigReq_In\r\n"));
	gpSDP->Unlock();
	return ERROR_SUCCESS;
}

static int sdp_DataDown_Out(void *pCallContext, unsigned short result) {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_DataDown_Out(0x%08x,%d)\r\n",pCallContext,result));
	if (! gpSDP)
		return ERROR_SERVICE_NOT_ACTIVE;

	gpSDP->Lock();

	if (gpSDP->eStage != Connected) {
		gpSDP->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	Call *pCall = VerifyCall((Call *) pCallContext,CALL_SDP_SEARCH_WORKER);
	if (! pCall) {
		gpSDP->Unlock();
		return ERROR_NOT_FOUND;
	}

	SVSUTIL_ASSERT(VerifyLink(pCall->pLink));

	gpSDP->Unlock();
	return ERROR_SUCCESS;
}

static int sdp_Disconnect_Out(void *pCallContext, unsigned short result) {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_Disconnect_Out(0x%08x,%d)\r\n",pCallContext,result));
	if (! gpSDP)
		return ERROR_SERVICE_NOT_ACTIVE;

	gpSDP->Lock();

	Call *pCall = pCallContext ? VerifyCall((Call*) pCallContext,CALL_SDP_DISCONNECT) : NULL;
	if (pCall) 
		NotifySdpClientOfCompletion(pCall,result);

	gpSDP->Unlock();
	return ERROR_SUCCESS;
}

// These are just stubs - they do nothing
static int sdp_ConfigResponse_Out(void *pCallContext, unsigned short result) {
	return ERROR_SUCCESS;
}

static int sdp_ConnectResponse_Out(void *pCallContext, unsigned short result) {
	return ERROR_SUCCESS;
}


//**************************************************************
//	Device Driver init and deinit functions
//**************************************************************
int sdp_InitializeOnce(void)  {
	IFDBG(DebugOut(DEBUG_SDP_INIT | DEBUG_SDP_TRACE,L"sdp_InitializeOnce()\r\n"));
	SVSUTIL_ASSERT(! gpSDP && !gpSdpReadBuffer);

	if (gpSDP)  {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_InitializeOnce:: ERROR_ALREADY_EXISTS\r\n"));
		return ERROR_ALREADY_EXISTS;
	}	

	gpSDP = CreateNewSDP();
	if (gpSDP)  {
		gpSdpReadBuffer = (unsigned char*) g_funcAlloc(MAX_MTU_SIZE,g_pvAllocData);
		if (gpSdpReadBuffer) {
			IFDBG(DebugOut(DEBUG_SDP_INIT, L"sdp_InitializeOnce:: ERROR_SUCCESS\n"));
			return ERROR_SUCCESS;
		}
		delete gpSDP;
		gpSDP = NULL;
	}

	IFDBG(DebugOut(DEBUG_ERROR, L"sdp_InitializeOnce:: ERROR_OUTOFMEMORY\n"));
	return ERROR_OUTOFMEMORY;
}


int sdp_CreateDriverInstance(void)  {
	int iErr;
	IFDBG(DebugOut(DEBUG_SDP_INIT | DEBUG_SDP_TRACE,L"sdp_CreateDriverInstance()\r\n"));

	if (! gpSDP) {
		IFDBG(DebugOut(DEBUG_ERROR, L"sdp_CreateDriverInstance:: ERROR_SERVICE_DOES_NOT_EXIST\n"));
		return ERROR_SERVICE_DOES_NOT_EXIST;
	}

	gpSDP->Lock();
	if (gpSDP->eStage != JustCreated) {
		IFDBG(DebugOut(DEBUG_ERROR, L"sdp_CreateDriverInstance:: ERROR_SERVICE_ALREADY_RUNNING\n"));
		gpSDP->Unlock();
		return ERROR_SERVICE_ALREADY_RUNNING;
	}
	SVSUTIL_ASSERT(!gpSDP->pfmdLinks && !gpSDP->pfmdSdpConnections && 
	               !gpSDP->pfmdCallQueue && !gpSDP->pfmdBuffers);

	gpSDP->eStage = Initializing;

	// Setup gpSDP.
	gpSDP->pfmdLinks          = svsutil_AllocFixedMemDescr(sizeof(Link), SDP_SCALE);
	gpSDP->pfmdCalls          = svsutil_AllocFixedMemDescr(sizeof(Call), SDP_SCALE);
	gpSDP->pfmdSdpConnections = svsutil_AllocFixedMemDescr(sizeof(SdpConnection), SDP_SCALE);
	gpSDP->pfmdCallQueue      = svsutil_AllocFixedMemDescr(sizeof(CallQueue), SDP_SCALE);
	gpSDP->pfmdBuffers        = svsutil_AllocFixedMemDescr(sizeof(BD_BUFFER), SDP_SCALE);

	if (!pSdpDB) {
		pSdpDB = CreateNewSdpDatabase();
		if (pSdpDB && (STATUS_SUCCESS != pSdpDB->Init())) {
			delete pSdpDB;
			pSdpDB = NULL;
		}
	}

	if (! (gpSDP->pfmdLinks     && gpSDP->pfmdCalls   && gpSDP->pfmdSdpConnections &&
	       gpSDP->pfmdCallQueue && gpSDP->pfmdBuffers && gpSDP->pfmdBuffers &&
	       pSdpDB))   {

		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_CreateDriverInstance: ERROR_OUTOFMEMORY\r\n"));
		CleanupSDPState();
		gpSDP->Unlock();
		return ERROR_OUTOFMEMORY;
	}

	// Configure L2CAP 
	L2CAP_EVENT_INDICATION lei;
	memset(&lei, 0, sizeof(lei));

	lei.l2ca_ConfigInd     = sdp_ConfigInd;
	lei.l2ca_ConnectInd    = sdp_ConnectInd;
	lei.l2ca_DataUpInd     = sdp_DataUpInd;
	lei.l2ca_DisconnectInd = sdp_DisconnectInd;
	lei.l2ca_StackEvent    = sdp_lStackEvent;

	L2CAP_CALLBACKS lc;
	memset(&lc, 0, sizeof(lc));

	lc.l2ca_ConfigReq_Out        = sdp_ConfigReq_Out;
	lc.l2ca_ConfigResponse_Out   = sdp_ConfigResponse_Out;
	lc.l2ca_ConnectReq_Out       = sdp_ConnectReq_Out;
	lc.l2ca_ConnectResponse_Out  = sdp_ConnectResponse_Out;
	lc.l2ca_DataDown_Out         = sdp_DataDown_Out;
	lc.l2ca_Disconnect_Out       = sdp_Disconnect_Out;
	lc.l2ca_CallAborted          = sdp_lCallAborted;
	
	if (ERROR_SUCCESS != (iErr = L2CAP_EstablishDeviceContext (gpSDP, PSM_SDP, &lei, &lc, &gpSDP->l2cap_if,&gpSDP->cDataHeaders,&gpSDP->cDataTrailers,&gpSDP->hL2CAP))) {
		IFDBG((DEBUG_ERROR,L"sdp_CreateDriverInstance could not plug into L2CAP, exiting\r\n"));
		CleanupSDPState();
		gpSDP->Unlock();		
		return iErr;
	}	

	if (! (gpSDP->l2cap_if.l2ca_ConnectReq_In      && gpSDP->l2cap_if.l2ca_ConfigReq_In   &&
	       gpSDP->l2cap_if.l2ca_ConfigResponse_In  && gpSDP->l2cap_if.l2ca_Disconnect_In  &&
	       gpSDP->l2cap_if.l2ca_ConnectResponse_In && gpSDP->l2cap_if.l2ca_AbortCall      &&
	       gpSDP->l2cap_if.l2ca_DataDown_In))  {

		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_CreateDriverInstance fails because not all functions required by SDP are exposed by the L2CAP layer\r\n"));

		L2CAP_CloseDeviceContext (gpSDP->hL2CAP);
		CleanupSDPState();
		gpSDP->Unlock();
		return iErr;
	}

#if 0  // Currently this reg param is unused
	HKEY hk;
	if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_BASE, g_SdpBaseRegKey, 0, KEY_READ, &hk)) {
		DWORD dwSize = sizeof(gpSDP->dwSdpIdle);
		DWORD dwType = REG_DWORD;

		RegQueryValueEx(hk, g_SdpLinkIdle, NULL, &dwType, (LPBYTE)&gpSDP->dwSdpIdle, &dwSize);
		if ((dwSize != sizeof(gpSDP->dwSdpIdle)) || (dwType != REG_DWORD))
			gpSDP->dwSdpIdle = SDP_DEFAULT_LINK_TIMEOUT_MS;

		IFDBG(DebugOut(DEBUG_SDP_INIT,L"SdpLinkIdle time = %d ms\r\n",gpSDP->dwSdpIdle));
		RegCloseKey(hk);
	}
#endif

	gpSDP->eStage = Disconnected;
	GetConnectionState();

	IFDBG(DebugOut(DEBUG_SDP_INIT,L"sdp_CreateDriverInstance returns ERROR_SUCCESS\r\n"));
	gpSDP->Unlock();
	return ERROR_SUCCESS;
}


int sdp_CloseDriverInstance (void) {
	IFDBG(DebugOut(DEBUG_SDP_INIT | DEBUG_SDP_TRACE,L"sdp_CloseDriverInstance()\r\n"));
	
	if (! gpSDP)  {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_CloseDriverInstance: ERROR_SERVICE_NOT_ACTIVE\r\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}	

	gpSDP->Lock();
	if ((gpSDP->eStage == JustCreated) || (gpSDP->eStage == ShuttingDown)) {
		IFDBG(DebugOut(DEBUG_ERROR, L"SDP Close Driver Instance:: ERROR_SERVICE_NOT_ACTIVE\n"));
		gpSDP->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpSDP->eStage = ShuttingDown;

	while (gpSDP->pCalls) {
		NotifySdpClientOfCompletion(gpSDP->pCalls,ERROR_CANCELLED);
	}

	while (gpSDP->pLinks) {
		SVSUTIL_ASSERT(gpSDP->pLinks->pCallQueue == NULL);
		SdpDisconnect(gpSDP->pLinks,NULL);
	}

	if (gpSDP->hL2CAP)  {
		while (gpSDP->GetRefCount() > 1) {
			IFDBG(DebugOut (DEBUG_SDP_TRACE, L"Waiting for ref count in sdp_CloseDriverInstance\n"));
			gpSDP->Unlock();
			Sleep(200);
			gpSDP->Lock();
		}
		L2CAP_CloseDeviceContext(gpSDP->hL2CAP);
	}

	while (gpSDP->pContexts)  {
		SDP_CONTEXT *pThis = gpSDP->pContexts;
		gpSDP->pContexts = pThis->pNext;

		if (pThis->ei.sdp_StackEvent) {
			BT_LAYER_STACK_EVENT_IND pCallback = pThis->ei.sdp_StackEvent;
			void *pUserContext = pThis->pUserContext;

			IFDBG(DebugOut(DEBUG_SDP_TRACE, L"Going into StackEvent notification(stack down)\r\n"));
			pThis->AddRef();
			gpSDP->Unlock();
			__try {
				pCallback(pUserContext, BTH_STACK_DOWN, NULL);
			} __except(1) {
				IFDBG(DebugOut(DEBUG_ERROR, L"Exception in higher layer code\r\n"));
			}
			gpSDP->Lock();
			pThis->DelRef();
			IFDBG(DebugOut(DEBUG_SDP_TRACE, L"Came back StackEvent notification\r\n"));
		}

		while (pThis->GetRefCount() > 1) {
			IFDBG(DebugOut (DEBUG_SDP_TRACE, L"Waiting for ref count in sdp_CloseDriverInstance\n"));
			gpSDP->Unlock();
			Sleep(100);
			gpSDP->Lock();
		}
		delete pThis;
	}

	SVSUTIL_ASSERT(gpSDP->eStage == ShuttingDown);
	CleanupSDPState();

	gpSDP->Unlock();
	return ERROR_SUCCESS;
}

int sdp_UninitializeOnce(void) {
	IFDBG(DebugOut(DEBUG_SDP_INIT | DEBUG_SDP_TRACE,L"sdp_UninitializeOnce()\r\n"));
	SVSUTIL_ASSERT(gpSDP);

	if (! gpSDP) {
		IFDBG(DebugOut(DEBUG_ERROR, L"SDP uninit:: ERROR_SERVICE_DOES_NOT_EXIST\n"));
		return ERROR_SERVICE_DOES_NOT_EXIST;
	}
	gpSDP->Lock();

	if (gpSDP->eStage != JustCreated) {
		IFDBG(DebugOut(DEBUG_ERROR, L"SDP uninit:: ERROR_DEVICE_IN_USE\n"));
		gpSDP->Unlock();
		return ERROR_DEVICE_IN_USE;
	}

	if (pSdpDB) {
		delete pSdpDB;
		pSdpDB = NULL;
	}

	if (gpSdpReadBuffer) {
		g_funcFree(gpSdpReadBuffer,g_pvAllocData);
		gpSdpReadBuffer	= NULL;
	}

	if (gpSpdConnectionLocal) {
		DeleteSdpConnection(gpSpdConnectionLocal);
		gpSpdConnectionLocal = NULL;
	}

	SDP *pSDP = gpSDP;
	gpSDP = NULL;
	pSDP->Unlock();
	delete pSDP;

	IFDBG(DebugOut(DEBUG_SDP_INIT, L"SDP uninit:: ERROR_SUCCESS\n");)	
	return ERROR_SUCCESS;
}

static int SdpCreateConnection(Call *pCall, BD_ADDR *pba) {
	SVSUTIL_ASSERT(gpSDP->IsLocked());
	SVSUTIL_ASSERT(!pCall->pLink);

	Link *pLink = AllocLink();
	if (!pLink) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_Connect:: ERROR_OUTOFMEMORY\r\n"));
		return ERROR_OUTOFMEMORY;
	}
	
	pLink->pNext = gpSDP->pLinks;
	gpSDP->pLinks = pLink;
	pCall->pLink = pLink;

	pLink->cid = 0;
	pLink->b = *pba;
	pLink->fStage = NONE;
	pLink->inMTU = DEFAULT_IN_MTU_SIZE;
	pLink->fIncoming = FALSE;

	HANDLE hL2CAP = gpSDP->hL2CAP;
	L2CA_ConnectReq_In pCallbackConnect = gpSDP->l2cap_if.l2ca_ConnectReq_In;
	int iRes = ERROR_INTERNAL_ERROR;

	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"Calling into l2ca_ConnectReq_In\r\n"));
	gpSDP->AddRef();
	gpSDP->Unlock();
	__try {
		iRes = pCallbackConnect(hL2CAP, pCall, PSM_SDP, pba);
	} __except(1) {
		IFDBG(DebugOut(DEBUG_ERROR,L"Exception in l2ca_ConnectReq_In!!!\r\n"));
	}
	gpSDP->Lock();
	gpSDP->DelRef();
	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"came from l2ca_ConnectReq_In\r\n"));

	return iRes;
}

//******************************************************************
// Replaced functions from NT SDP layer class.
//******************************************************************
unsigned long
SdpConnection::SendInitialRequestToServer(void)  {
	// Client data has request to send, rawParametersLength doesn't count PDU or continuation state info.
	return Write(u.Client.pRequest,u.Client.rawParametersLength + sizeof(SdpPDU) + sizeof(UCHAR));
}

// Called by functions in btsdp.cpp, this will send request if there's nothing busy or queue it otherwise.
unsigned long SdpConnection::SendOrQueueRequest()  {
	Call      *pCall   = (Call *) pCallContext;
	Link      *pLink   = pCall->pLink;
	int       iErr     = STATUS_PENDING;
	CallQueue *pCallQueue;

	SVSUTIL_ASSERT(gpSDP->IsLocked());
	SVSUTIL_ASSERT(VerifyCall(pCall,CALL_SDP_SEARCH));
	SVSUTIL_ASSERT(VerifyLink(pLink));	

	if (! (pCallQueue = AddItemToCallQueue(pLink,pCall)))  {
		return ERROR_OUTOFMEMORY;
	}

	if (pCallQueue == pLink->pCallQueue && !pLink->fCallCanceled)
		iErr = SendInitialRequestToServer();

	return iErr;
}


NTSTATUS 
SdpConnection::Write(
    void *pBuffer,
    unsigned long length
    )
{
	SVSUTIL_ASSERT(gpSDP->IsLocked());
	
	Call *pCall = (Call *) pCallContext;
	SVSUTIL_ASSERT(VerifyCall(pCall,CALL_SDP_SEARCH_WORKER));
	SVSUTIL_ASSERT(VerifyLink(pCall->pLink));

#if defined (DEBUG) || defined (_DEBUG)
	// call we're about to service better be first on send queue.
	if (pCall->fWhat & CALL_SDP_SEARCH)
		SVSUTIL_ASSERT(pCall->pLink->pCallQueue->pCall == pCall);
#endif

	return SendData(pCall,(unsigned char*)pBuffer,length);
}

BOOL SdpConnection::IsClient() {
	return ! ((Call*) pCallContext)->pLink->fIncoming;
}

BOOL SdpConnection::IsServer() {
	return ((Call*) pCallContext)->pLink->fIncoming;
}

// Code that relies on CSemaphore locking mechanism is from NT port, and is always wrapped
// in the SDP CritSec.  Put ASSERTS in to be sure
BOOLEAN CSemaphore::ReleaseSem(KPRIORITY Increment,LONG Adjustment, BOOLEAN bWait)  { 
	SVSUTIL_ASSERT(gpSDP && gpSDP->IsLocked());
	return TRUE; 
}

void CSemaphore::AcquireSem()  { 
	SVSUTIL_ASSERT(gpSDP && gpSDP->IsLocked());
}

// Required to map status codes returned by NT SDP layer
static int MapStatusToErrorCode(int status)  {
	switch (status) {
		case ERROR_SUCCESS:
			return ERROR_SUCCESS;

		case ERROR_INVALID_PARAMETER:
		case STATUS_UNSUCCESSFUL:
		case STATUS_INVALID_PARAMETER:
		case STATUS_DEVICE_PROTOCOL_ERROR:
		case STATUS_INVALID_NETWORK_RESPONSE:
			return ERROR_INVALID_PARAMETER;

		case ERROR_NOT_FOUND:
		case STATUS_NOT_FOUND:
			return ERROR_NOT_FOUND;

		case ERROR_OUTOFMEMORY:
		case STATUS_INSUFFICIENT_RESOURCES:
			return ERROR_OUTOFMEMORY;

		default:
			SVSUTIL_ASSERT(0);
	}
	return status;
}


//******************************************************************
// SDP Interface
//******************************************************************

// AddRecord and RemoveRecord run totally locally, no need
// to have them be Asyncronous.  Since they run locally we only need to make
// sure SDP stack is running since we don't use lower L2CAP layers in these calls.
static int sdp_AddRecord(HANDLE hDeviceContext, UCHAR *pStream, unsigned long streamSize, HANDLE *pRecordHandle) {
	int iRet;
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_AddRecord(0x%08x,0x%08x,%d)\r\n",hDeviceContext,pStream,streamSize));
	                         
	if (! gpSDP) {
		IFDBG(DebugOut(DEBUG_WARN,L"sdp_AddRecord returns ERROR_SERVICE_NOT_ACTIVE\r\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpSDP->Lock();
	if (!gpSDP->IsStackRunning()) {
		gpSDP->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}
	
	if (!VerifyContext((SDP_CONTEXT*) hDeviceContext) || !pStream || streamSize == 0 || !pRecordHandle)
		iRet = ERROR_INVALID_PARAMETER;
	else {
#if defined (UNDER_CE)
        // if *pRecordHandle is non NULL, then treat it as an update and not add record.
		iRet = *pRecordHandle ? pSdpDB->UpdateRecord(pStream,streamSize,*pRecordHandle) : pSdpDB->AddRecord(pStream,streamSize,pRecordHandle);
		if (iRet != ERROR_SUCCESS)
   			*pRecordHandle = 0;
#else  // WINCE_EMULATION
        iRet = pSdpDB->AddRecord(pStream,streamSize,pRecordHandle);
#endif // UNDER_CE

	}
	gpSDP->Unlock();

#if defined (UNDER_CE)
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdpAddRecord returns 0x%08x, *pRecordHandle=0x%08x\r\n",iRet,*pRecordHandle));
#endif
	return MapStatusToErrorCode(iRet);
}

static int sdp_RemoveRecord(HANDLE hDeviceContext, HANDLE hRecordHandle)  {
	int iRet;
	unsigned long iRemovedRecord;
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_RemoveRecord(0x%08x,0x%08x)\r\n",hDeviceContext,hRecordHandle));

	if (! gpSDP) {
		IFDBG(DebugOut(DEBUG_WARN,L"sdp_RemoveRecord ERROR_SERVICE_NOT_ACTIVE\r\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}
	
	gpSDP->Lock();
	if (!gpSDP->IsStackRunning()) {
		gpSDP->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	if (!VerifyContext((SDP_CONTEXT*) hDeviceContext))
		iRet = ERROR_INVALID_PARAMETER;
	else
		iRet = pSdpDB->RemoveRecord(hRecordHandle,&iRemovedRecord);
	gpSDP->Unlock();

	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdpRemoveRecord returns 0x%08x\r\n",iRet));
	return MapStatusToErrorCode(iRet);
}

static int SdpDisconnect(Link *pLink, Call *pCall)  {
	SVSUTIL_ASSERT( VerifyLink(pLink));

	HANDLE hL2CAP = gpSDP->hL2CAP;
	L2CA_Disconnect_In pCallback = gpSDP->l2cap_if.l2ca_Disconnect_In;
	unsigned short cid = pLink->cid;
	int iRes = ERROR_INTERNAL_ERROR;

	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"Entering l2ca_Disconnect_In, cid = 0x%04x\r\n",pLink->cid));
	gpSDP->AddRef();
	gpSDP->Unlock();
	__try {
		iRes = pCallback (hL2CAP, pCall, cid);
	} __except (1) {
		IFDBG(DebugOut(DEBUG_ERROR,L"Exception in l2ca_Disconnect_In\r\n"));
	}
	gpSDP->Lock();
	gpSDP->DelRef();
	IFDBG(DebugOut(DEBUG_SDP_CALLBACK,L"Finished with l2ca_Disconnect_In\r\n");)	

	DeleteLink(pLink);
	return iRes;
}

static int sdp_Connect_In(HANDLE hDeviceContext, void *pCallContext, BD_ADDR *pba)  {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_Connect_In(0x%08x,0x%08x,0x%08x 0x%04)\r\n",
	                          hDeviceContext,pCallContext,pba->NAP,pba->SAP));

	if (! gpSDP) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_Connect_In: ERROR_SERVICE_NOT_ACTIVE\r\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpSDP->Lock();
	if (gpSDP->eStage != Connected) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_Connect_In: ERROR_SERVICE_NOT_ACTIVE\r\n"));
		gpSDP->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	SDP_CONTEXT *pContext = VerifyContext((SDP_CONTEXT*) hDeviceContext);
	if (!pContext || !pba)  {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_Connect_In: ERROR_INVALID_PARAMETER\r\n"));
		gpSDP->Unlock();
		return ERROR_INVALID_PARAMETER;
	}

	Link *pLink = FindLink(pba,TRUE);
	if (pLink)  {
		IFDBG(DebugOut(DEBUG_SDP_PACKETS,L"sdp_Connect_In: already connected\r\n"));
		pLink->AddRef();

		if (pLink->fStage == UP)  {
			NotifyConnect(pContext,pCallContext,ERROR_SUCCESS,pLink->cid);
			gpSDP->Unlock();
			return ERROR_SUCCESS;
		}
		// if link hasn't been established we alloc the call but don't try to fire up connection.
	}

	Call *pCall = AllocCall(CALL_SDP_CONNECT,pLink,pContext,pCallContext);
	if (!pCall)  {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_Connect_In:  ERROR_OUTOFMEMORY\r\n"));
		if (pLink)
			pLink->DelRef();
		gpSDP->Unlock();
		return ERROR_OUTOFMEMORY;
	}
	int iRes = ERROR_SUCCESS;

	if (!pLink)  {
		iRes = SdpCreateConnection(pCall,pba);
		if (iRes != ERROR_SUCCESS)  {
			// Since we gave up CritSec in SdpCreateConnection need to make sure call is still around.
			if (VerifyCall(pCall))
				DeleteCall(pCall);
		}
	}

	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_Connect_In: iRes = 0x%08x\r\n",iRes));
	gpSDP->Unlock();
	return iRes;
}


static int sdp_Disconnect_In(HANDLE hDeviceContext, void *pCallContext, unsigned short cid)  {
	int iRes = ERROR_SUCCESS;
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_Disconnect_In(0x%08x,0x%08x,0x%04x)\r\n",
	                          hDeviceContext,pCallContext,cid));

	if (! gpSDP) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_Disconnect_In: ERROR_SERVICE_NOT_ACTIVE\r\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpSDP->Lock();
	if (gpSDP->eStage != Connected) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_Disconnect_In: ERROR_SERVICE_NOT_ACTIVE\r\n"));
		gpSDP->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	SDP_CONTEXT *pOwner = VerifyContext((SDP_CONTEXT*) hDeviceContext);
	Link *pLink = pOwner ? FindLink(cid) : NULL;

	if (!pOwner || !pLink) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_Disconnect_In: ERROR_INVALID_PARAMETER\r\n"));
		gpSDP->Unlock();
		return ERROR_INVALID_PARAMETER;
	}
	SVSUTIL_ASSERT(!pLink->fIncoming);

	pLink->DelRef();
	if (pLink->GetRefCount() > 0)  {
		IFDBG(DebugOut(DEBUG_SDP_PACKETS,L"sdp_Disconnect_In: cid 0x%04x has extra references\r\n",cid));
		NotifyDisconnect(pOwner,pCallContext,ERROR_SUCCESS);
	}
	else {
		IFDBG(DebugOut(DEBUG_SDP_PACKETS,L"sdp_Disconnect_In: cid 0x%04x has last ref removed, deleting\r\n",cid));
		Call *pCall = AllocCall(CALL_SDP_DISCONNECT,NULL, pOwner, pCallContext);

		// If are OOM we still do disconnect, but won't be able to call sdp_Disconnect_Out.
		iRes = SdpDisconnect(pLink,pCall);
		if (iRes != ERROR_SUCCESS && pCall && VerifyCall(pCall))
			DeleteCall(pCall);

		if (!pCall)
			iRes = ERROR_OUTOFMEMORY;
	}

	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_Disconnect_In: 0x%08x\r\n",iRes));
	gpSDP->Unlock();
	return iRes;
}

static int sdp_ServiceSearch_In(HANDLE hDeviceContext, void *pCallContext, unsigned short cid,
                                SdpQueryUuid* pUUIDs, unsigned short cMaxHandles)  {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_ServiceSearch_In(0x%08x,0x%08x,0x%08x,%d)\r\n",
	                         hDeviceContext,pCallContext,pUUIDs,cMaxHandles));

	if (! gpSDP) { 
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_ServiceSearch_In: ERROR_SERVICE_NOT_ACTIVE\r\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}	

	gpSDP->Lock();
	if (gpSDP->eStage != Connected) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_ServiceSearch_In: ERROR_SERVICE_NOT_ACTIVE\r\n"));
		gpSDP->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	SDP_CONTEXT *pContext = VerifyContext((SDP_CONTEXT*) hDeviceContext);
	Link        *pLink    = (pContext && cid) ? FindLink(cid) : NULL;

	// for VerifyCall to be legit, as soon as we take down link we must invalidate the call (or use a flag to do this,
	// let mem be freed by caller function).  Also currently this allows calls to be queued even if not connected,
	// need to make sure this case is handled both on connection and if connection fails.
	if (!pContext || (cid && (!pLink || pLink->fStage != UP))) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_ServiceSearch_In: ERROR_INVALID_PARAMETER\r\n"));
		gpSDP->Unlock();
		return ERROR_INVALID_PARAMETER;
	}

	Call             *pCall    = NULL;
	SdpConnection    *pSdpConn = NULL;
	SdpServiceSearch searchInfo;
	
	if (cid)  {
		pCall = AllocCall(CALL_SDP_SERVICE_SEARCH, pLink, pContext,pCallContext);
		if (!pCall)  {
			IFDBG(DebugOut(DEBUG_ERROR,L"sdp_ServiceSearch_In: ERROR_OUTOFMEMORY\r\n"));
			gpSDP->Unlock();
			return ERROR_OUTOFMEMORY;
		}
		pSdpConn = AllocSdpConnection(pCall);
	}
	else {
		pSdpConn = GetLocalSDPConn();
		if (!pSdpConn) {
			IFDBG(DebugOut(DEBUG_ERROR,L"sdp_ServiceSearch_In: ERROR_OUTOFMEMORY\r\n"));
			gpSDP->Unlock();
			return ERROR_OUTOFMEMORY;
		}
	}

	searchInfo.uuids       = pUUIDs;
	searchInfo.cMaxHandles = cMaxHandles;

	int iRes = SdpInterface::ServiceSearch(pSdpConn,&searchInfo,(cid == 0));
	if (iRes == STATUS_PENDING) {
		SVSUTIL_ASSERT(cid);
		iRes = ERROR_SUCCESS;
	}

	if (!cid)  {
		unsigned short cReturnedHandles;
		if (iRes == ERROR_SUCCESS)
			cReturnedHandles = pSdpConn->u.Client.ServiceSearch.totalRecordCount;
		else
			cReturnedHandles = 0;

		NotifyServiceSearch(pContext,pCallContext,iRes,cReturnedHandles,pSdpConn->pClientBuf);
		ResetSdpConnection(pSdpConn);
	}
	else if (iRes != ERROR_SUCCESS) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_ServiceSearch_In: SdpInterface::ServiceSearch fails, 0x%08x\r\n",iRes));
#if defined (DEBUG) || defined (_DEBUG)
		pCall->pSdpConnection->fClientNotified = TRUE;
#endif
		DeleteCall(pCall);
	}

	gpSDP->Unlock();
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_ServiceSearch_In: 0x%08x\r\n",iRes));
	return MapStatusToErrorCode(iRes);
}

static int sdp_AttributeSearch_In(HANDLE hDeviceContext, void *pCallContext, unsigned short cid, unsigned long recordHandle, SdpAttributeRange *pAttribRange, int numAttributes)  {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_AttributeSearch_In(0x%08x,0x%08x,0x%08x,0x%08x,0x%04x,0x%08x,%d )\r\n",
	                                                hDeviceContext,pCallContext,recordHandle,cid,pAttribRange,numAttributes));

	if (! gpSDP) { 
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_AttributeSearch_In: ERROR_SERVICE_NOT_ACTIVE\r\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}	

	gpSDP->Lock();
	if (gpSDP->eStage != Connected) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_AttributeSearch_In: ERROR_SERVICE_NOT_ACTIVE\r\n"));
		gpSDP->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	SDP_CONTEXT *pContext = VerifyContext((SDP_CONTEXT*) hDeviceContext);
	Link        *pLink = (pContext && cid) ? FindLink(cid) : NULL;	

	if (!pContext || (cid && (!pLink || pLink->fStage != UP))) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_AttributeSearch_In: ERROR_INVALID_PARAMETER\r\n"));
		gpSDP->Unlock();
		return ERROR_INVALID_PARAMETER;
	}

	Call          *pCall    = NULL;
	SdpConnection *pSdpConn = NULL;
	SdpAttributeSearch searchInfo;
	
	if (cid)  {
		pCall = AllocCall(CALL_SDP_ATTRIBUTE_SEARCH, pLink, pContext,pCallContext);
		if (!pCall)  {
			IFDBG(DebugOut(DEBUG_ERROR,L"sdp_AttributeSearch_In: ERROR_OUTOFMEMORY\r\n"));
			gpSDP->Unlock();
			return ERROR_OUTOFMEMORY;
		}
		pSdpConn = AllocSdpConnection(pCall);
	}
	else {
		pSdpConn = GetLocalSDPConn();
		if (!pSdpConn) {
			IFDBG(DebugOut(DEBUG_ERROR,L"sdp_AttributeSearch_In: ERROR_OUTOFMEMORY\r\n"));
			gpSDP->Unlock();
			return ERROR_OUTOFMEMORY;
		}
	}

	searchInfo.range         = pAttribRange;
	searchInfo.numAttributes = numAttributes;
	searchInfo.recordHandle  = recordHandle;

	int iRes = SdpInterface::AttributeSearch(pSdpConn,&searchInfo,(cid == 0));
	if (iRes == STATUS_PENDING) {
		SVSUTIL_ASSERT(cid);
		iRes = ERROR_SUCCESS;
	}


	if (!cid)  {
		NotifyAttributeSearch(pContext,pCallContext,iRes,pSdpConn->pClientBuf,pSdpConn->cClientBuf);
		ResetSdpConnection(pSdpConn);
	}
	else if (iRes != ERROR_SUCCESS) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_ServiceSearch_In: SdpInterface::ServiceSearch fails, 0x%08x\r\n",iRes));
#if defined (DEBUG) || defined (_DEBUG)
		pCall->pSdpConnection->fClientNotified = TRUE;
#endif
		DeleteCall(pCall);
	}

	gpSDP->Unlock();
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_ServiceSearch_In: 0x%08x\r\n",iRes));
	return MapStatusToErrorCode(iRes);
}

static int sdp_ServiceAttributeSearch_In(HANDLE hDeviceContext, void *pCallContext, unsigned short cid,
                                         SdpQueryUuid* pUUIDs, SdpAttributeRange *pAttribRange, int numAttributes)  {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_ServiceAttributeSearch_In(0x%08x,0x%08x,0x%04x,0x%08x,0x%08x,%08x)\r\n",
	                                                hDeviceContext,pCallContext,cid,pUUIDs,pAttribRange,numAttributes));

	if (! gpSDP) { 
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_ServiceAttributeSearch_In: ERROR_SERVICE_NOT_ACTIVE\r\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}	

	gpSDP->Lock();
	if (gpSDP->eStage != Connected) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_ServiceAttributeSearch_In: ERROR_SERVICE_NOT_ACTIVE\r\n"));
		gpSDP->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	SDP_CONTEXT *pContext = VerifyContext((SDP_CONTEXT*) hDeviceContext);
	Link        *pLink    = (pContext && cid) ? FindLink(cid) : NULL;	

	if (!pContext || (cid && (!pLink || pLink->fStage != UP))) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_ServiceAttributeSearch_In: ERROR_INVALID_PARAMETER\r\n"));
		gpSDP->Unlock();
		return ERROR_INVALID_PARAMETER;
	}

	Call          *pCall    = NULL;
	SdpConnection *pSdpConn = NULL;
	SdpServiceAttributeSearch searchInfo;
	
	if (cid)  {
		pCall = AllocCall(CALL_SDP_SERVICE_ATTRIBUTE_SEARCH, pLink, pContext,pCallContext);
		if (!pCall)  {
			IFDBG(DebugOut(DEBUG_ERROR,L"sdp_ServiceAttributeSearch_In: ERROR_OUTOFMEMORY\r\n"));
			gpSDP->Unlock();
			return ERROR_OUTOFMEMORY;
		}
		pSdpConn = AllocSdpConnection(pCall);
	}
	else {
		pSdpConn = GetLocalSDPConn();
		if (!pSdpConn) {
			IFDBG(DebugOut(DEBUG_ERROR,L"sdp_ServiceAttributeSearch_In: ERROR_OUTOFMEMORY\r\n"));
			gpSDP->Unlock();
			return ERROR_OUTOFMEMORY;
		}
	}

	searchInfo.uuids         = pUUIDs;
	searchInfo.range         = pAttribRange;
	searchInfo.numAttributes = numAttributes;
	
	int iRes = SdpInterface::ServiceAndAttributeSearch(pSdpConn,&searchInfo,(cid == 0));
	if (iRes == STATUS_PENDING) {
		SVSUTIL_ASSERT(cid);
		iRes = ERROR_SUCCESS;
	}

	if (!cid)  {
		NotifyServiceAttributeSearch(pContext,pCallContext,iRes,pSdpConn->pClientBuf,pSdpConn->cClientBuf);
		ResetSdpConnection(pSdpConn);
	}
	else if (iRes != ERROR_SUCCESS) {
		IFDBG(DebugOut(DEBUG_ERROR,L"sdp_ServiceSearch_In: SdpInterface::ServiceSearch fails, 0x%08x\r\n",iRes));
#if defined (DEBUG) || defined (_DEBUG)
		pCall->pSdpConnection->fClientNotified = TRUE;
#endif
		DeleteCall(pCall);
	}

	gpSDP->Unlock();
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_ServiceAttributeSearch_In: 0x%08x\r\n",iRes));
	return MapStatusToErrorCode(iRes);
}

static int check_io_control_parms(int cInBuffer,char *pInBuffer,int cOutBuffer,char *pOutBuffer,int *pcDataReturned,char *pSpace,int cSpace) {
	--cSpace;

	__try {
		if (pcDataReturned)  {
			*pcDataReturned = 0;
			memset (pOutBuffer, 0, cOutBuffer);
		} else if (pOutBuffer || cOutBuffer)
			return FALSE;

		int i = 0;
		while (cInBuffer > 0) {
			pSpace[i = (i < cSpace) ? i + 1 : 0] = *pInBuffer++;
			--cInBuffer;
		}
	} __except(1) {
		return FALSE;
	}

	return TRUE;
}

static int sdp_ioctl (HANDLE hDeviceContext, int fSelector, int cInBuffer, char *pInBuffer, int cOutBuffer, char *pOutBuffer, int *pcDataReturned) {
	IFDBG(DebugOut (DEBUG_SDP_TRACE, L"SDP: sdp_ioctl\n"));

	char c;
	if (! check_io_control_parms (cInBuffer, pInBuffer, cOutBuffer, pOutBuffer, pcDataReturned, &c, 1)) {
		IFDBG(DebugOut (DEBUG_ERROR, L"sdp_ioctl returns ERROR_INVALID_PARAMETER (exception)\n"));
		return ERROR_INVALID_PARAMETER;
	}

	if (! gpSDP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"sdp_ioctl returns ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpSDP->Lock();
	if (! gpSDP->IsStackRunning()) {
		IFDBG(DebugOut (DEBUG_ERROR, L"sdp_ioctl returns ERROR_SERVICE_NOT_ACTIVE\n"));
		gpSDP->Unlock ();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	SDP_CONTEXT *pContext = VerifyContext ((SDP_CONTEXT *)hDeviceContext);
	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"sdp_ioctl returns ERROR_INVALID_HANDLE\n"));
		gpSDP->Unlock ();
		return ERROR_INVALID_HANDLE;
	}

	int iRes = ERROR_INVALID_OPERATION;

	switch (fSelector) {
	case BTH_STACK_IOCTL_GET_CONNECTED:
		if ((cInBuffer == 0) && (cOutBuffer == 4)) {
			iRes = ERROR_SUCCESS;

			int iCount = (gpSDP->eStage == Connected);

			pOutBuffer[0] = iCount & 0xff;
			pOutBuffer[1] = (iCount >> 8) & 0xff;
			pOutBuffer[2] = (iCount >> 16) & 0xff;
			pOutBuffer[3] = (iCount >> 24) & 0xff;
			*pcDataReturned = 4;
		} else
			iRes = ERROR_INVALID_PARAMETER;
		break;
	}


	IFDBG(DebugOut (DEBUG_SDP_TRACE, L"sdp_ioctl exits with code 0x%08x\n", iRes));
	gpSDP->Unlock();
	return iRes;
}

static int sdp_abort_call(HANDLE hDev, void *pCallContext)  {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"sdp_abort_call(0x%08x, 0x%08x)\r\n",hDev,pCallContext));
	if (! gpSDP)
		return ERROR_SERVICE_NOT_ACTIVE;

	gpSDP->Lock();

	SDP_CONTEXT *pOwner = VerifyContext((SDP_CONTEXT *)hDev);
	if (!pOwner) { 
		gpSDP->Unlock();
		return ERROR_INVALID_PARAMETER;
	}

	Call *pTrav = gpSDP->pCalls;
	while (pTrav && ((pTrav->pOwner != pOwner) || (pTrav->pCallContext != pCallContext)))
		pTrav = pTrav->pNext;

	gpSDP->Unlock();
	return (pTrav ? sdp_lCallAborted(pTrav,ERROR_OPERATION_ABORTED) : ERROR_INVALID_PARAMETER);
}


static int check_SDP_EstablishDeviceContext_parms
(
SDP_CALLBACKS           *pCall,             /* IN */
SDP_INTERFACE           *pInt,              /* IN */
HANDLE					*phDeviceContext	/* IN */
)  {
	__try {
		memset(pInt, 0, sizeof(*pInt));
		*phDeviceContext = NULL;

		SDP_CALLBACKS c;
		memcpy(&c, pCall, sizeof(c));
	} __except(1) {
		return FALSE;
	}

	return TRUE;
}

int SDP_EstablishDeviceContext
(
void					*pUserContext,		/* IN */
SDP_EVENT_INDICATION    *pInd,              /* IN */
SDP_CALLBACKS           *pCall,             /* IN */
SDP_INTERFACE           *pInt,              /* OUT */
HANDLE					*phDeviceContext	/* OUT */
)
{
	IFDBG(DebugOut(DEBUG_SDP_TRACE, L"SDP_EstablishDeviceContext()\r\n"));
	if ( ! check_SDP_EstablishDeviceContext_parms(pCall, pInt, phDeviceContext)) {
		IFDBG(DebugOut(DEBUG_WARN, L"check_SDP_EstablishDeviceContext_parms returns ERROR_INVALID_PARAMETER(exception)\r\n"));
		return ERROR_INVALID_PARAMETER;
	}

	if (! gpSDP)
		return ERROR_SERVICE_NOT_ACTIVE;

	gpSDP->Lock();
	if (!gpSDP->IsStackRunning()) {
		gpSDP->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	SDP_CONTEXT *pContext = new SDP_CONTEXT;
	if (!pContext)  {
		gpSDP->Unlock();
		IFDBG(DebugOut(DEBUG_ERROR,L"SDP_EstablishDeviceContext: ERROR_OUTOFMEMORY\r\n"));
		return ERROR_OUTOFMEMORY;
	}
	
	pContext->c  = *pCall;
	pContext->ei = *pInd;
	pContext->pNext  = gpSDP->pContexts;
	pContext->pUserContext = pUserContext;
	gpSDP->pContexts = pContext;
	*phDeviceContext = pContext;

	pInt->sdp_AddRecord                 = sdp_AddRecord;
	pInt->sdp_RemoveRecord              = sdp_RemoveRecord;
	pInt->sdp_ServiceSearch_In          = sdp_ServiceSearch_In;
	pInt->sdp_Disconnect_In             = sdp_Disconnect_In;
	pInt->sdp_Connect_In                = sdp_Connect_In;
	pInt->sdp_AttributeSearch_In        = sdp_AttributeSearch_In;
	pInt->sdp_ServiceAttributeSearch_In = sdp_ServiceAttributeSearch_In;
	pInt->sdp_ioctl                     = sdp_ioctl;
	pInt->sdp_AbortCall                 = sdp_abort_call;

	IFDBG(DebugOut(DEBUG_SDP_TRACE, L"SDP_EstablishDeviceContext: ERROR_SUCCESS\r\n"));
	gpSDP->Unlock();
	return ERROR_SUCCESS;
}

int SDP_CloseDeviceContext(HANDLE hDeviceContext) {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"SDP_CloseDeviceContext(0x%08x)\r\n",hDeviceContext));
	if (! gpSDP) {
		IFDBG(DebugOut(DEBUG_WARN, L"SDP_CloseDeviceContext returns ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpSDP->Lock();
	if (!gpSDP->IsStackRunning()) {
		IFDBG(DebugOut(DEBUG_WARN, L"SDP_CloseDeviceContext returns ERROR_SERVICE_NOT_ACTIVE\n"));
		gpSDP->Unlock();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	int iRes = ERROR_SUCCESS;
	while (gpSDP && gpSDP->IsStackRunning()) {
		SDP_CONTEXT *pOwner = VerifyContext((SDP_CONTEXT *) hDeviceContext);

		if (!pOwner) {
			iRes = ERROR_NOT_FOUND;
			break;
		}

		if (pOwner->GetRefCount() > 1)  {
			IFDBG(DebugOut (DEBUG_SDP_TRACE, L"Waiting for ref count in SDP_CloseDeviceContext\n"));
			gpSDP->Unlock();
			Sleep(100);
			gpSDP->Lock();
			continue;
		}

		Call *pCall = gpSDP->pCalls;
		while (pCall && (pCall->pOwner != pOwner))
			pCall = pCall->pNext;

		if (pCall) {
			if (VerifyLink(pCall->pLink))
				SdpDisconnect(pCall->pLink,NULL);
			continue;
		}

		if (gpSDP->pContexts == pOwner)
			gpSDP->pContexts = pOwner->pNext;
		else {
			SDP_CONTEXT *pParent = gpSDP->pContexts;
			while (pParent && (pParent->pNext != pOwner))
				pParent = pParent->pNext;
			pParent->pNext = pOwner->pNext;
		}
		delete pOwner;
		break;
	}

	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"SDP_CloseDeviceContext returns 0x%08x\r\n",iRes));
	gpSDP->Unlock();
	return iRes;
}

// Analogous to NT.  In NT this function is implemented in user mode,
// stubbed out in kernel mode.
void SdpReleaseContainer(ISdpNodeContainer *container) 
{
}

typedef struct {
    GUID    Protocol;
    BOOL    Found;
    BOOL    ParamFound;
    ULONG   Param;
} PROTOCOL_WALK_CONTEXT, *PPROTOCOL_WALK_CONTEXT;

static NTSTATUS
ProtocolListWalkFunc(
    IN      PVOID   Context,
    IN      UCHAR   DataType,
    IN      ULONG   DataSize,
    IN      PUCHAR  Data,
    IN      ULONG   DataStorageSize
    )
{
    PPROTOCOL_WALK_CONTEXT pContext = (PPROTOCOL_WALK_CONTEXT) Context;
    switch (DataType)
    {
        case SDP_TYPE_UUID:
            if (!pContext->Found)
            {
                UCHAR uuid[sizeof(GUID)];
                switch (DataSize)
                {
                    case sizeof(USHORT):
                    {
                        SdpRetrieveUint16(Data, (PUSHORT)uuid);
                        *(PUSHORT)uuid = SdpByteSwapUint16(*(PUSHORT)uuid);
                        break;
                    }
                    case sizeof(ULONG):
                    {
                        SdpRetrieveUint32(Data, (PULONG)uuid);
                        *(PULONG)uuid = SdpByteSwapUint32(*(PULONG)uuid);
                        break;
                    }
                    case sizeof(GUID):
                    {
                        SdpRetrieveUuid128(Data, (LPGUID)uuid);
                        SdpByteSwapUuid128((LPGUID)&uuid, (LPGUID)&uuid);
                        break;
                    }
                    default:
                        ASSERT(!"Bad data size for UUID\n");
                        break;
                }
                if (RtlEqualMemory(&pContext->Protocol,
                                   uuid,
                                   DataSize))
                {
                    pContext->Found = TRUE;
                }
            }
            else
            {
                // If we've found it, this is the next protocol, and we don't care.
                // Returning failure will stop the walk.
                return STATUS_UNSUCCESSFUL;
            }
            break;
        case SDP_TYPE_INT:
        case SDP_TYPE_UINT:
            if (pContext->Found)
            {
                pContext->ParamFound = TRUE;
                switch (DataSize)
                {
                    case 1:
                        pContext->Param = *(PUCHAR)Data;
                        break;
                    case 2:
                        SdpRetrieveUint16(Data, (PUSHORT)&pContext->Param);
                        pContext->Param = SdpByteSwapUint16((USHORT)pContext->Param);
                        break;
                    case 4:
                        SdpRetrieveUint32(Data, (PULONG)&pContext->Param);
                        pContext->Param = SdpByteSwapUint32(pContext->Param);
                        break;

                    default:
                        pContext->ParamFound = FALSE;  // Bad size
                        break;
                }

                return STATUS_UNSUCCESSFUL; // stop walking
            }
            break;
        default:
            break;
    }
    return STATUS_SUCCESS;
}

int GetRfcommCidFromResponse(unsigned char *pInputBuffer, DWORD cOutBuf, unsigned long *pbtChannel) {
	int iErr               = ERROR_NOT_FOUND;
	PROTOCOL_WALK_CONTEXT  protContext;
	unsigned char          *pAttrib;
	unsigned long          ulAttribLen;
	unsigned char          *pCurrentSequence;
	unsigned long          ulCurrentSequenceLen;
	unsigned char          type, sizeIndex;
    unsigned long          StorageSize;

	SVSUTIL_ASSERT((int)cOutBuf >= 0);

	if (cOutBuf == 0 || (ERROR_SUCCESS != ValidateStream(pInputBuffer,cOutBuf,NULL,NULL,NULL)))
		return ERROR_NOT_FOUND;

	memcpy(&protContext.Protocol,&RFCOMM_PROTOCOL_UUID,sizeof(GUID));	
	SdpRetrieveHeader(pInputBuffer, type, sizeIndex);
	if (type != SDP_TYPE_SEQUENCE && type != SDP_TYPE_ALTERNATIVE)
		return ERROR_NOT_FOUND;

	SdpRetrieveVariableSize(pInputBuffer,sizeIndex,&ulCurrentSequenceLen,&StorageSize);
	ulCurrentSequenceLen = StorageSize + 1;
	pCurrentSequence     = pInputBuffer;

	while (1) {
		pCurrentSequence += ulCurrentSequenceLen;
		SVSUTIL_ASSERT(pCurrentSequence - pInputBuffer <= (int)cOutBuf);
		
		if (pCurrentSequence - pInputBuffer >= (int)cOutBuf)
			break;
		
		SdpRetrieveHeader(pCurrentSequence, type, sizeIndex);
		if (type != SDP_TYPE_SEQUENCE && type != SDP_TYPE_ALTERNATIVE)
			break;

		SdpRetrieveVariableSize(pCurrentSequence+1,sizeIndex,&ulCurrentSequenceLen,&StorageSize);
		ulCurrentSequenceLen += StorageSize + 1;

		iErr = SdpFindAttributeInStream(pCurrentSequence,ulCurrentSequenceLen,SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST,&pAttrib,&ulAttribLen);

		if (ERROR_SUCCESS != iErr || !pAttrib || !ulAttribLen)
		{
			iErr = ERROR_NOT_FOUND;
			// We only output current sequence, not entire record, for succintness.
			IFDBG(DebugOut(DEBUG_WARN,L"SDP: cannot find ProtocolDescriptor in ServAttribSearch, probable bug in server.  Invalid RecordSequence follows:"));
			IFDBG(DumpBuff(DEBUG_WARN,pCurrentSequence,ulCurrentSequenceLen));
			continue;
		}

		protContext.Found      = FALSE;
		protContext.ParamFound = FALSE;
		protContext.Param      = 0;

		SdpWalkStream(pAttrib,ulAttribLen,ProtocolListWalkFunc,&protContext);

		if (protContext.ParamFound && protContext.ParamFound)
		{
			*pbtChannel = protContext.Param;
			iErr = ERROR_SUCCESS;
			break;
		}
	}

	return iErr;
}


// If a process has created SDP records and its being terminated, we receive
// a notification and will run through SDP database removing any SDP records 
// that it created.
void SdpRemoveRecordsOnProcessExit(HANDLE hDyingProc) {
	IFDBG(DebugOut(DEBUG_SDP_TRACE,L"SdpRemoveRecordsFromProcess(0x%08x)\r\n",hDyingProc));

	if (! gpSDP) {
		IFDBG(DebugOut(DEBUG_WARN,L"SdpRemoveRecordsOnProcessExit: ERROR_SERVICE_NOT_ACTIVE\r\n"));
		return;
	}

	gpSDP->Lock();
	if (!pSdpDB) {
		gpSDP->Unlock();
		IFDBG(DebugOut(DEBUG_WARN,L"SdpRemoveRecordsOnProcessExit: ERROR_SERVICE_NOT_ACTIVE\r\n"));
		return;
	}

	pSdpDB->RemoveRecordsAssociatedWithProcess(hDyingProc);

	gpSDP->Unlock();
	return;
}



#if defined(BTH_CONSOLE)
int sdp_ProcessConsoleCommand(WCHAR *pszCommand)  {
	if (! gpSDP) 
		return ERROR_SERVICE_NOT_ACTIVE;

	int iRes = ERROR_SUCCESS;
	gpSDP->Lock();

	__try {
		if (wcsicmp(pszCommand, L"help") == 0) {
			DebugOut(DEBUG_OUTPUT,L"SDP Commands:\r\n");
			DebugOut(DEBUG_OUTPUT,L"    No commands implemented yet\r\n");
		}
		else
			iRes = ERROR_NOT_FOUND;
	} 
	__except(1) {
		DebugOut(DEBUG_OUTPUT, L"ERROR! -- Exception during command processing!\n");
	}
	gpSDP->Unlock();

	return iRes;
}
#endif // BTH_CONSOLE
