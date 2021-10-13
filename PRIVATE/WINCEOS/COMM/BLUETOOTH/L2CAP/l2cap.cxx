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
//      Bluetooth L2CAP Layer
// 
// 
// Module Name:
// 
//      l2cap.cxx
// 
// Abstract:
// 
//      This file implements Bluetooth L2CAP Layer
// 
// 
//------------------------------------------------------------------------------
#include <windows.h>
#include <svsutil.hxx>

#if ! defined (UNDER_CE)
#include <stddef.h>
#endif

#include <bt_debug.h>
#include <bt_os.h>
#include <bt_buffer.h>
#include <bt_hcip.h>
#include <bt_ddi.h>

#define L2CAP_TO		5000
#define L2CAP_LINK_TO	1000
#define L2CAP_SCALE		10

#define L2CAP_FIRST_CID	0x0040
#define L2CAP_FIRST_PSM	0x1001
#define L2CAP_LAST_PSM	0x1001

#define L2CAP_RTX		60
#define L2CAP_ERTX		300
#define L2CAP_PHYSIDLE	10
#define L2CAP_CONNIDLE	10
#define L2CAP_CONFIGTO	120

#define L2CAP_RECONNECT	3
#define L2CAP_MAXBAD	3

#define L2CAP_COMMAND_COMMAND_REJECT		0x01
#define L2CAP_COMMAND_CONNECTION_REQUEST	0x02
#define L2CAP_COMMAND_CONNECTION_RESPONSE	0x03
#define L2CAP_COMMAND_CONFIG_REQUEST		0x04
#define L2CAP_COMMAND_CONFIG_RESPONSE		0x05
#define L2CAP_COMMAND_DISCONNECT_REQUEST	0x06
#define L2CAP_COMMAND_DISCONNECT_RESPONSE	0x07
#define L2CAP_COMMAND_ECHO_REQUEST			0x08
#define L2CAP_COMMAND_ECHO_RESPONSE			0x09
#define L2CAP_COMMAND_INFO_REQUEST			0x0a
#define L2CAP_COMMAND_INFO_RESPONSE			0x0b

#define L2CAP_V_MAJ	1
#define L2CAP_V_MIN	1

enum CONNECTION_STAGE {
	STARTING_PHYS,			// Waiting physical link
	STARTING,				// Waiting response
	STARTING_REQUEST,
	CONFIG,
	CONFIG_LOCAL_DONE,
	CONFIG_REMOTE_DONE,
	UP,
	DISCONNECTED
};

enum CALL_OP {
	CALL_HCI_READSCAN,
	CALL_HCI_WRITESCAN,
	CALL_PHYS_CONNECT,
	CALL_PHYS_ACCEPT,
	CALL_PHYS_DISCONNECT,
	CALL_PHYS_DROP_IDLE,
	CALL_PHYS_PING,
	CALL_LOG_CONNECT_REQ,
	CALL_LOG_CONNECT_RESP,
	CALL_LOG_CONFIG_REQ,
	CALL_LOG_CONFIG_RESP,
	CALL_LOG_DISCONNECT_REQ,
	CALL_USERDATA,
};

enum CALL_CONTEXT_TYPE {
	CALL_CTX_UNINITIALIZED	= 0,
	CALL_CTX_EVENT,
	CALL_CTX_CALLBACK,
	CALL_CTX_CALLOWNER,
	CALL_CTX_INTERNAL
};

enum L2CAP_STAGE {
	JustCreated			= 0,
	Initializing,
	Connected,
	Disconnected,
	ShuttingDown,
	Error
};

class PhysLink;
class LogLink;
class CallContext;
class L2CAP_CONTEXT;
class L2CAP;

class PSMContext {
public:
	PSMContext		*pNext;
	unsigned short	usPSM;

	void *operator new (size_t iSize);
	void operator delete(void *ptr);
};

class L2CAP_CONTEXT : public SVSAllocClass, public SVSRefObj {
public:
	L2CAP_CONTEXT	*pNext;

	L2CAP_EVENT_INDICATION	ei;
	L2CAP_CALLBACKS			c;

	void			        *pUserContext;
	PSMContext				*pReservedPorts;

	int						fDefaultServer;

	unsigned short			usPacketType;

	L2CAP_CONTEXT (void) {
		pNext = NULL;
		memset (&ei, 0, sizeof(ei));
		memset (&c, 0, sizeof(c));

		pUserContext   = NULL;
		pReservedPorts = NULL;

		fDefaultServer = FALSE;
		usPacketType   = 0;
	}
};

class PhysLink {
public:
	PhysLink			*pNext;
	DWORD				dwTimeOutCookie;

	BD_ADDR				b;
	unsigned short		h;

	CONNECTION_STAGE	eStage;

	LogLink				*pLogLinks;

	unsigned int		iConnectionAttempts;
	unsigned int		iTransmissionProblems;
	unsigned int		iPingsSent;

	unsigned short		usPacketType;

	int					iLockCnt;

	PhysLink (void) {
		memset (this, 0, sizeof(PhysLink));
		eStage = STARTING;
		h      = BT_HCI_INVALID_HANDLE;
	}

	void *operator new (size_t iSize);
	void operator delete(void *ptr);
};

class LogLink {
public:
	LogLink				*pNext;
	DWORD				dwTimeOutCookie;

	PhysLink			*pPhysLink;

	unsigned short		psm;

	unsigned short		cid;
	unsigned short		cid_remote;

	CONNECTION_STAGE	eStage;

	L2CAP_CONTEXT		*pOwner;

	LogLink (PhysLink *pPhys, L2CAP_CONTEXT *a_pOwner) {
		pNext = pPhys->pLogLinks;
		pPhys->pLogLinks = this;

		dwTimeOutCookie = 0;

		pPhysLink = pPhys;

		psm = 0;

		cid = INVALID_CID;
		cid_remote = INVALID_CID;

		eStage = pPhys->eStage == UP ? STARTING : STARTING_PHYS;

		pOwner = a_pOwner;

	}

	void *operator new (size_t iSize);
	void operator delete(void *ptr);
};

typedef int (*ContextCallback) (CallContext *pContext, int iErr);

class CallContext {
public:
	CallContext			*pNext;
	DWORD				dwTimeOutCookie;

	CALL_OP				eWhat;

	union {
		void			*pLink;
		LogLink			*pLogLink;
		PhysLink		*pPhysLink;
	} u;

	unsigned int		id : 8;

	unsigned int		fForeignId : 1;
	unsigned int		fComplete : 1;
	unsigned int		fKeepOnAbort : 1;

	unsigned int		iResult;

	L2CAP_CONTEXT		*pOwner;
	void				*pContext;

	CALL_CONTEXT_TYPE	eType;

	union {
		HANDLE			hEvent;
		ContextCallback	pCallback;
	};

	int					cData;
	void				*pData;

	union {
		unsigned char	ucresult[8];
		unsigned short	usresult[4];
		unsigned int	uiresult[2];
	} r;

	void *operator new (size_t iSize);
	void operator delete(void *ptr);
};

class L2CAP : public SVSAllocClass, public SVSSynch, public SVSRefObj {
public:
	PhysLink			*pPhysLinks;
	L2CAP_CONTEXT		*pContexts;
	CallContext			*pCalls;

	L2CAP_STAGE			eStage;

	FixedMemDescr		*pfmdPhysLinks;
	FixedMemDescr		*pfmdLogLinks;
	FixedMemDescr		*pfmdCallContexts;
	FixedMemDescr		*pfmdPSM;

	HCI_INTERFACE		hci_if;
	HANDLE				hHCI;

	int					cHCIHeader;
	int					cHCITrailer;

	int					iEchoes;

	unsigned short		usCurrentPSM;
	unsigned short		usCurrentCID;
	unsigned char		ucCurrentID;

	//	Configurable parameters
	unsigned char		bRole;
	unsigned short		usLinkPolicy;
	unsigned short		usPacketType;

	unsigned int		RTX;
	unsigned int		ERTX;

	DWORD				dwPhysIdle;
	DWORD				dwConnectIdle;
	DWORD				dwConfigTO;

	unsigned int		fPicoCapable : 1;
	unsigned int		fScanModeControl : 1;

	void ReInit (void) {
		pPhysLinks		 = NULL;
		pContexts		 = NULL;
		pCalls           = NULL;

		eStage			 = JustCreated;

		pfmdPhysLinks    = NULL;
		pfmdLogLinks     = NULL;
		pfmdCallContexts = NULL;
		pfmdPSM          = NULL;

		memset (&hci_if, 0, sizeof(hci_if));

		hHCI             = NULL;

		cHCIHeader       = 0;
		cHCITrailer      = 0;

		iEchoes          = 0;

		usCurrentPSM     = L2CAP_FIRST_PSM;
		usCurrentCID     = L2CAP_FIRST_CID;
		ucCurrentID      = 0;

		bRole            = 0;
		usLinkPolicy     = 0xffff;
		usPacketType     = 0;

		RTX              = 0;
		ERTX             = 0;

		dwPhysIdle       = 0;
		dwConnectIdle    = 0;
		dwConfigTO       = 0;

		fPicoCapable     = FALSE;
		fScanModeControl = TRUE;
	}

	L2CAP (void) {
		ReInit ();
	}

	int IsStackRunning (void) {
		return (eStage == Connected) || (eStage == Disconnected);
	}
};

struct CONFIG_OPT {
	unsigned char	type;
	unsigned char	length;
	union {
		unsigned short	MTU;
		unsigned short	FlushTimeout;
		unsigned char	QoS[22];
	} u;
};

struct SignallingPacket {
	struct {
		unsigned char		code;
		unsigned char		id;
		unsigned short		length;
	} h;

	union {
		struct {
			unsigned short	reason;
			unsigned char	ucOptionalData[4];
		} COMMAND_REJECT;
		struct {
			unsigned short  psm;
			unsigned short  source_cid;
		} CONNECTION_REQUEST;
		struct {
			unsigned short	dest_cid;
			unsigned short  source_cid;
			unsigned short	result;
			unsigned short	status;
		} CONNECTION_RESPONSE;
		struct {
			unsigned short	dest_cid;
			unsigned short	flags;
			unsigned char	optbuf[sizeof(CONFIG_OPT) * 3];
		} CONFIG_REQUEST;
		struct {
			unsigned short  source_cid;
			unsigned short  flags;
			unsigned short  result;
			unsigned char	optbuf[sizeof(CONFIG_OPT) * 3];
		} CONFIG_RESPONSE;
		struct {
			unsigned short	dest_cid;
			unsigned short	source_cid;
		} DISCONNECT_REQUEST, DISCONNECT_RESPONSE;
		struct {
			unsigned char	ucData[256];
		} ECHO_REQUEST, ECHO_RESPONSE;
		struct {
			unsigned short  info_type;
		} INFO_REQUEST;
		struct {
			unsigned short  info_type;
			unsigned short  result;
			unsigned char   ucData[1];
		} INFO_RESPONSE;
	} u;
};

struct Signal {
	unsigned short		length;
	unsigned short		cid;
	SignallingPacket	packet;
};

static inline int SIGNAL_LENGTH(Signal &s) {
	return (offsetof (Signal, packet) + s.length);
}

static L2CAP *gpL2CAP = NULL;

static CallContext *AllocCallContext (CALL_CONTEXT_TYPE eType, CALL_OP eOp, void *pLink, L2CAP_CONTEXT *pOwner, void *pCallContext);
static void ResetCallContext (CallContext *pContext);
static void DeleteCallContext (CallContext *pContext);

static int CancelCall (CallContext *pCall, int iError);

static void DisconnectLogicalLink (LogLink *pLink, int iErr, int fSendDisconnect);
static int DisconnectPhysicalLink (PhysLink *pPhysLink, int fReconnect, int iErr, CallContext *pCallContext);

#if defined (DEBUG) || defined (_DEBUG)
static void DumpBuff (unsigned int cMask, BD_BUFFER *pBuff) {
	DumpBuff (cMask, pBuff->pBuffer + pBuff->cStart, BufferTotal (pBuff));
}
#else
static void DumpBuff (unsigned int cMask, BD_BUFFER *pB) {}
#endif

static void AbortCall (CallContext *pCall, int iErr);

static PhysLink *FindPhys (unsigned short h) {
	PhysLink *pP = gpL2CAP->pPhysLinks;
	while (pP && (pP->h != h))
		pP = pP->pNext;

	return pP;
}

static PhysLink *FindPhys (BD_ADDR *pba) {
	PhysLink *pP = gpL2CAP->pPhysLinks;
	while (pP && (pP->b != *pba))
		pP = pP->pNext;

	return pP;
}

static LogLink *FindLog (unsigned short cid, PhysLink *pPhys) {
	LogLink *pLog = pPhys->pLogLinks;
	while (pLog) {
		if (pLog->cid == cid)
			break;

		pLog = pLog->pNext;
	}

	return pLog;
}

static LogLink *FindLog (unsigned short cid) {
	PhysLink *pPhys = gpL2CAP->pPhysLinks;
	while (pPhys) {
		LogLink *pLog = pPhys->pLogLinks;
		while (pLog) {
			if (pLog->cid == cid)
				return pLog;

			pLog = pLog->pNext;
		}

		pPhys = pPhys->pNext;
	}

	return NULL;
}

static CallContext *FindCall (LogLink *pLog) {
	CallContext *pCall = gpL2CAP->pCalls;
	while (pCall && (pCall->u.pLogLink != pLog))
		pCall = pCall->pNext;

	return pCall;
}

static CallContext *FindCall (LogLink *pLog, CALL_OP eOp) {
	CallContext *pCall = gpL2CAP->pCalls;
	while (pCall && ((pCall->u.pLogLink != pLog) || (pCall->eWhat != eOp)))
		pCall = pCall->pNext;

	return pCall;
}

static CallContext *FindCall (LogLink *pLog, CALL_OP eOp, unsigned char id) {
	CallContext *pCall = gpL2CAP->pCalls;
	while (pCall && ((pCall->u.pLogLink != pLog) || (pCall->eWhat != eOp) || (pCall->id != id)))
		pCall = pCall->pNext;

	return pCall;
}

static CallContext *FindCall (PhysLink *pPhys) {
	CallContext *pCall = gpL2CAP->pCalls;
	while (pCall && (pCall->u.pPhysLink != pPhys))
		pCall = pCall->pNext;

	return pCall;
}

static CallContext *FindCall (PhysLink *pPhys, CALL_OP eOp) {
	CallContext *pCall = gpL2CAP->pCalls;
	while (pCall && ((pCall->u.pPhysLink != pPhys) || (pCall->eWhat != eOp)))
		pCall = pCall->pNext;

	return pCall;
}

static CallContext *FindCall (PhysLink *pPhys, CALL_OP eOp, unsigned char id) {
	CallContext *pCall = gpL2CAP->pCalls;
	while (pCall && ((pCall->u.pPhysLink != pPhys) || (pCall->eWhat != eOp) || (pCall->id != id)))
		pCall = pCall->pNext;

	return pCall;
}

static CallContext *FindCall (PhysLink *pPhys, unsigned char id) {
	CallContext *pCall = gpL2CAP->pCalls;
	while (pCall) {
		if (pCall->id == id) {
			switch (pCall->eWhat) {
			case CALL_HCI_READSCAN:
			case CALL_HCI_WRITESCAN:
			case CALL_PHYS_DROP_IDLE:		// This does NOT have physlink in it
			case CALL_PHYS_DISCONNECT:		// This does NOT have physlink in it
				break;

			case CALL_PHYS_CONNECT:
			case CALL_PHYS_ACCEPT:
			case CALL_PHYS_PING:
				if (pPhys == pCall->u.pPhysLink)
					return pCall;
				break;

			case CALL_LOG_CONNECT_RESP:
			case CALL_LOG_CONFIG_RESP:
				if (pCall->fForeignId)
					break;

			case CALL_LOG_CONNECT_REQ:
			case CALL_LOG_CONFIG_REQ:
			case CALL_USERDATA:
			case CALL_LOG_DISCONNECT_REQ:
				if (pPhys == pCall->u.pLogLink->pPhysLink)
					return pCall;
				break;

			default:
				SVSUTIL_ASSERT (0);
			}
		}
		pCall = pCall->pNext;
	}

	return NULL;
}

static CallContext *FindCall (void *pUserContext) {
	CallContext *pCall = gpL2CAP->pCalls;

	while (pCall && (pCall->pContext != pUserContext))
		pCall = pCall->pNext;

	return pCall;
}

static LogLink *FindCID (unsigned short cid, PhysLink *pHint) {
	PhysLink *pP = pHint ? pHint : gpL2CAP->pPhysLinks;
	while (pP) {
		LogLink *pL = pP->pLogLinks;
		while (pL) {
			if (pL->cid == cid)
				return pL;
			pL = pL->pNext;
		}

		pP = pHint ? NULL : pP->pNext;
	}

	return NULL;
}

static L2CAP_CONTEXT *FindContextByPSM (unsigned short psm) {
	L2CAP_CONTEXT *pCtx = gpL2CAP->pContexts;
	while (pCtx) {
		PSMContext *pPSMCtx = pCtx->pReservedPorts;
		while (pPSMCtx) {
			if (pPSMCtx->usPSM == psm)
				return pCtx;
			pPSMCtx = pPSMCtx->pNext;
		}

		pCtx = pCtx->pNext;
	}

	return NULL;
}

static PhysLink *VerifyPhys (PhysLink *pLink) {
	PhysLink *pP = gpL2CAP->pPhysLinks;
	while (pP && (pP != pLink))
		pP = pP->pNext;

	return pP;
}

static LogLink *VerifyLog (LogLink *pLink) {
	PhysLink *pP = VerifyPhys (pLink->pPhysLink);
	if (! pP)
		return NULL;

	LogLink *pL = pP->pLogLinks;

	while (pL && (pL != pLink))
		pL = pL->pNext;

	return pL;
}

static L2CAP_CONTEXT *VerifyContext (L2CAP_CONTEXT *pContext) {
	L2CAP_CONTEXT *pRunner = gpL2CAP->pContexts;
	while (pRunner && (pRunner != pContext))
		pRunner = pRunner->pNext;

	return pRunner;
}

static CallContext *VerifyCall (CallContext *pCallContext) {
	CallContext *pContext = gpL2CAP->pCalls;
	while (pContext && (pContext != pCallContext))
		pContext = pContext->pNext;
	return pContext;
}

static PhysLink *CreateNewPhysLink (void) {
	return new PhysLink;
}

static LogLink *CreateNewLogLink (PhysLink *pPhys, L2CAP_CONTEXT *pOwner) {
	return new LogLink (pPhys, pOwner);
}

static void GetConnectionState (void) {
	__try {
		int fConnected = FALSE;
		int dwRet = 0;
		gpL2CAP->hci_if.hci_ioctl (gpL2CAP->hHCI, BTH_STACK_IOCTL_GET_CONNECTED, 0, NULL, sizeof(fConnected), (char *)&fConnected, &dwRet);
		if ((dwRet == sizeof(fConnected)) && fConnected)
			gpL2CAP->eStage = Connected;
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] GetConnectionState : exception in hci_ioctl BTH_STACK_IOCTL_GET_CONNECTED\n"));
	}
}

static void SetScanEnable (void) {
	if (! gpL2CAP->fScanModeControl)
		return;

	CallContext *pCall = AllocCallContext (CALL_CTX_EVENT, CALL_HCI_READSCAN, NULL, NULL, NULL);
	pCall->fKeepOnAbort = TRUE;

	HCI_ReadScanEnable_In pCallback = gpL2CAP->hci_if.hci_ReadScanEnable_In;
	HANDLE hHCI = gpL2CAP->hHCI;
	HANDLE hEvent = pCall->hEvent;
	gpL2CAP->AddRef ();
	gpL2CAP->Unlock ();
	int iRes = ERROR_INTERNAL_ERROR;

	__try {
		iRes = pCallback (hHCI, pCall);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] SetScanEnable : EXCEPTION in hci_ReadScanEnable_In\n"));
	}

	if (iRes == ERROR_SUCCESS)
		WaitForSingleObject (hEvent, L2CAP_TO);

	gpL2CAP->Lock ();
	gpL2CAP->DelRef ();

	if ((! (pCall = VerifyCall (pCall))) || (! pCall->fComplete) || (iRes != ERROR_SUCCESS) || (pCall->iResult != ERROR_SUCCESS)) {
		if (pCall && gpL2CAP->IsStackRunning ())
			DeleteCallContext (pCall);

		IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] SetScanEnable failed read. \n"));
		return;
	}

	unsigned char cScanEnable = pCall->r.ucresult[0];
	DeleteCallContext (pCall);

	if ((cScanEnable & 0x3) != 0x3) {
		HCI_WriteScanEnable_In pCallback = gpL2CAP->hci_if.hci_WriteScanEnable_In;
		gpL2CAP->AddRef ();
		gpL2CAP->Unlock ();
		pCallback (hHCI, NULL, cScanEnable | 0x3);
		gpL2CAP->Lock ();
		gpL2CAP->DelRef ();
	}
}

static void WriteLinkPolicy (unsigned short hconnection) {
	if (gpL2CAP->usLinkPolicy == 0xffff)
		return;

	HCI_WriteLinkPolicySettings_In pCallback = gpL2CAP->hci_if.hci_WriteLinkPolicySettings_In;
	HANDLE hHCI = gpL2CAP->hHCI;

	gpL2CAP->AddRef ();
	gpL2CAP->Unlock ();
	int iRes = ERROR_INTERNAL_ERROR;

	__try {
		pCallback (hHCI, NULL, hconnection, gpL2CAP->usLinkPolicy);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] WriteLinkPolicy : EXCEPTION in hci_WriteLinkPolicySettings_In\n"));
	}

	gpL2CAP->Lock ();
	gpL2CAP->DelRef ();
}

static int WriteDataDown (unsigned short h, CallContext *pCall, BD_BUFFER *pB) {
	int iRes = ERROR_INTERNAL_ERROR;
	if (gpL2CAP->hHCI) {
		IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"WriteDataDown in\n"));
		HANDLE hHCI = gpL2CAP->hHCI;
		HCI_DataPacketDown_In pCallback = gpL2CAP->hci_if.hci_DataPacketDown_In;

		gpL2CAP->AddRef ();
		gpL2CAP->Unlock ();
		__try {
			iRes = pCallback (hHCI, pCall, h, pB);
		} __except (1) {
			IFDBG(DebugOut (DEBUG_ERROR, L"Exception in WriteDataDown\n"));
		}
		gpL2CAP->Lock ();
		gpL2CAP->DelRef ();
		IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"WriteDataDown out\n"));
	} else
		IFDBG(DebugOut (DEBUG_ERROR, L"HCI layer does not support sending data packets!\n"));

	return iRes;
}

static int WriteDataDown (unsigned short h, CallContext *pCall, int cSize, unsigned char *pBuffer) {
	if ((gpL2CAP->cHCIHeader == 0) && (gpL2CAP->cHCITrailer == 0)) {
		BD_BUFFER b;
		b.cSize = cSize;
		b.cEnd = b.cSize;
		b.cStart = 0;
		b.fMustCopy = TRUE;
		b.pFree = BufferFree;
		b.pBuffer = pBuffer;

		return WriteDataDown (h, pCall, &b);
	}

	BD_BUFFER *pB = BufferAlloc (cSize + gpL2CAP->cHCIHeader + gpL2CAP->cHCITrailer);

	if (pB) {
		pB->cEnd = pB->cSize - gpL2CAP->cHCITrailer;
		pB->cStart = gpL2CAP->cHCIHeader;
		memcpy (pB->pBuffer, pBuffer, cSize);
		return WriteDataDown (h, pCall, pB);
	}

	IFDBG(DebugOut (DEBUG_ERROR, L"OOM in WriteDataDown!\n"));
	return ERROR_OUTOFMEMORY;
}

static unsigned short GetCID (void) {
	for ( ; ; ) {
		unsigned short cid = ++gpL2CAP->usCurrentCID;
		if (cid < L2CAP_FIRST_CID) {
			gpL2CAP->usCurrentCID = L2CAP_FIRST_CID;
			continue;
		}

		if (! FindCID(cid, NULL))
			return cid;
	}
}

static int PluckPhysLink (PhysLink *pLink) {
	if (pLink == gpL2CAP->pPhysLinks)
		gpL2CAP->pPhysLinks = gpL2CAP->pPhysLinks->pNext;
	else {
		PhysLink *pParent = gpL2CAP->pPhysLinks;
		while (pParent && (pParent->pNext != pLink))
			pParent = pParent->pNext;

		if (! pParent)
			return FALSE;

		pParent->pNext = pLink->pNext;
	}

	return TRUE;
}

static int PluckLogLink (LogLink *pLink) {
	//	Take it out of the list
	if (pLink == pLink->pPhysLink->pLogLinks)
		pLink->pPhysLink->pLogLinks = pLink->pNext;
	else {
		LogLink *pParent = pLink->pPhysLink->pLogLinks;
		while (pParent && (pParent->pNext != pLink))
			pParent = pParent->pNext;

		if (! pParent)
			return FALSE;

		pParent->pNext = pLink->pNext;
	}

	return TRUE;
}
//
//	Timer
//
static DWORD WINAPI PhysLinkTimeout (LPVOID pArg) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"PhysLinkTimeout\n"));

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"PhysLinkTimeout : shutting down!\n"));
		return 0;
	}

	gpL2CAP->Lock ();

	if (gpL2CAP->eStage != Connected) {
		IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"PhysLinkTimeout : shutting down!\n"));
		gpL2CAP->Unlock ();
		return 0;
	}

	PhysLink *pLink = VerifyPhys ((PhysLink *)pArg);
	if (! pLink) {
		IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"PhysLinkTimeout : no link!\n"));
		gpL2CAP->Unlock ();
		return 0;
	}

	pLink->dwTimeOutCookie = 0;

	if (! (pLink->iPingsSent || pLink->pLogLinks || pLink->iLockCnt)) {
		IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"PhysLinkTimeout : timeouting the connection\n"));
		DisconnectPhysicalLink (pLink, FALSE, ERROR_SUCCESS, NULL);
	}

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"PhysLinkTimeout : done\n"));
	gpL2CAP->Unlock ();
	return 0;
}

static DWORD WINAPI LogLinkTimeout (LPVOID pArg) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"LogLinkTimeout\n"));

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"LogLinkTimeout : shutting down!\n"));
		return 0;
	}

	gpL2CAP->Lock ();

	if (gpL2CAP->eStage != Connected) {
		IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"LogLinkTimeout : shutting down!\n"));
		gpL2CAP->Unlock ();
		return 0;
	}

	LogLink *pLink = VerifyLog ((LogLink *)pArg);
	if (! pLink) {
		IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"LogLinkTimeout : no link!\n"));
		gpL2CAP->Unlock ();
		return 0;
	}

	if ((pLink->dwTimeOutCookie != 0) && (pLink->eStage != UP)) {
		IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"LogLinkTimeout : timeouting the connection\n"));

		pLink->dwTimeOutCookie = 0;

		DisconnectLogicalLink (pLink, ERROR_TIMEOUT, TRUE);
	}

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"LogLinkTimeout : done\n"));
	gpL2CAP->Unlock ();
	return 0;
}

static DWORD WINAPI CallTimeout (LPVOID pArg) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"CallTimeout\n"));

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"CallTimeout : shutting down!\n"));
		return 0;
	}

	gpL2CAP->Lock ();

	if (gpL2CAP->eStage != Connected) {
		IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"CallTimeout : shutting down!\n"));
		gpL2CAP->Unlock ();
		return 0;
	}

	CallContext *pCall = VerifyCall ((CallContext *)pArg);
	if (! pCall) {
		IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"CallTimeout : no call!\n"));
		gpL2CAP->Unlock ();
		return 0;
	}

	if (pCall->dwTimeOutCookie != 0) {
		IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"CallTimeout : timeouting the call\n"));

		pCall->dwTimeOutCookie = 0;

		if (pCall->eWhat == CALL_LOG_DISCONNECT_REQ) {
			SVSUTIL_ASSERT (VerifyLog (pCall->u.pLogLink));

			LogLink *pLog = pCall->u.pLogLink;

			CancelCall (pCall, ERROR_TIMEOUT);

			if (VerifyLog (pLog))
				DisconnectLogicalLink (pLog, ERROR_TIMEOUT, FALSE);
		} else if (pCall->eWhat == CALL_PHYS_PING) {
			SVSUTIL_ASSERT (VerifyPhys (pCall->u.pPhysLink));

			PhysLink *pPhys = pCall->u.pPhysLink;

			CancelCall (pCall, ERROR_TIMEOUT);

			if (VerifyPhys (pPhys))
				DisconnectPhysicalLink (pPhys, FALSE, ERROR_TIMEOUT, NULL);
		} else
			SVSUTIL_ASSERT (0);
	}

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"CallTimeout : done\n"));
	gpL2CAP->Unlock ();
	return 0;
}


static void ScheduleTimeout (PhysLink *pLink, DWORD dwTime) {
	if (pLink->dwTimeOutCookie)
		btutil_UnScheduleEvent (pLink->dwTimeOutCookie);

	pLink->dwTimeOutCookie = btutil_ScheduleEvent (PhysLinkTimeout, pLink, dwTime * 1000);
}

static void ScheduleTimeout (LogLink *pLink, DWORD dwTime) {
	if (pLink->dwTimeOutCookie)
		btutil_UnScheduleEvent (pLink->dwTimeOutCookie);

	pLink->dwTimeOutCookie = btutil_ScheduleEvent (LogLinkTimeout, pLink, dwTime * 1000);
}

static void ScheduleTimeout (CallContext *pCall, DWORD dwTime) {
	if (pCall->dwTimeOutCookie)
		btutil_UnScheduleEvent (pCall->dwTimeOutCookie);

	pCall->dwTimeOutCookie = btutil_ScheduleEvent (CallTimeout, pCall, dwTime * 1000);
}

static void UnscheduleTimeout (PhysLink *pLink) {
	if (pLink->dwTimeOutCookie)
		btutil_UnScheduleEvent (pLink->dwTimeOutCookie);
	pLink->dwTimeOutCookie = 0;
}

static void UnscheduleTimeout (LogLink *pLink) {
	if (pLink->dwTimeOutCookie)
		btutil_UnScheduleEvent (pLink->dwTimeOutCookie);
	pLink->dwTimeOutCookie = 0;
}

static void UnscheduleTimeout (CallContext *pCall) {
	if (pCall->dwTimeOutCookie)
		btutil_UnScheduleEvent (pCall->dwTimeOutCookie);
	pCall->dwTimeOutCookie = 0;
}

//
//	Communicate stack event up
//
static void DispatchStackEvent (int iEvent) {
	L2CAP_CONTEXT *pContext = gpL2CAP->pContexts;
	while (pContext && gpL2CAP->IsStackRunning ()) {
		BT_LAYER_STACK_EVENT_IND pCallback = pContext->ei.l2ca_StackEvent;
		if (pCallback) {
			void *pUserContext = pContext->pUserContext;

			IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Going into StackEvent notification\n"));
			pContext->AddRef ();
			gpL2CAP->Unlock ();

			__try {
				pCallback (pUserContext, iEvent, NULL);
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] L2CAP_connect_transport: exception in L2CAP_StackEvent!\n"));
			}

			gpL2CAP->Lock ();
			pContext->DelRef ();
			IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Came back StackEvent notification\n"));
		}
		pContext = pContext->pNext;
	}
}

static DWORD WINAPI StackDown (LPVOID pArg) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"Disconnect stack\n"));

	for ( ; ; ) {
		if (! gpL2CAP) {
			IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] DisconnectStack:: ERROR_SERVICE_DOES_NOT_EXIST\n"));
			return ERROR_SERVICE_DOES_NOT_EXIST;
		}

		gpL2CAP->Lock ();
		if (gpL2CAP->eStage != Connected) {
			IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] DisconnectStack:: ERROR_SERVICE_ALREADY_RUNNING\n"));
			gpL2CAP->Unlock ();
			return ERROR_SERVICE_ALREADY_RUNNING;
		}
		if (gpL2CAP->GetRefCount () == 1)
			break;

		IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"Waiting for ref count in StackDown\n"));
		gpL2CAP->Unlock ();
		Sleep (100);
	}

	gpL2CAP->iEchoes = 0;
	gpL2CAP->eStage = Disconnected;
	gpL2CAP->AddRef ();

	while (gpL2CAP->pPhysLinks && gpL2CAP->IsStackRunning ()) {
		gpL2CAP->pPhysLinks->eStage = DISCONNECTED;
		DisconnectPhysicalLink (gpL2CAP->pPhysLinks, FALSE, ERROR_SHUTDOWN_IN_PROGRESS, NULL);
	}

	while (gpL2CAP->pCalls && gpL2CAP->IsStackRunning ())
		AbortCall (gpL2CAP->pCalls, ERROR_SHUTDOWN_IN_PROGRESS);

	DispatchStackEvent (BTH_STACK_DOWN);

	gpL2CAP->DelRef ();

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"DisconnectStack:: ERROR_SUCCESS\n"));
	gpL2CAP->Unlock ();

	return ERROR_SUCCESS;
}

static DWORD WINAPI StackUp (LPVOID pArg) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"Connect stack\n"));

	for ( ; ; ) {
		if (! gpL2CAP) {
			IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] ConnectStack:: ERROR_SERVICE_DOES_NOT_EXIST\n"));
			return ERROR_SERVICE_DOES_NOT_EXIST;
		}

		gpL2CAP->Lock ();
		if (gpL2CAP->eStage != Disconnected) {
			IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] ConnectStack:: ERROR_SERVICE_ALREADY_RUNNING\n"));
			gpL2CAP->Unlock ();
			return ERROR_SERVICE_ALREADY_RUNNING;
		}
		if (gpL2CAP->GetRefCount () == 1)
			break;

		IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"Waiting for ref count in StackUp\n"));
		gpL2CAP->Unlock ();
		Sleep (100);
	}

	gpL2CAP->AddRef ();

	gpL2CAP->eStage = Connected;

	SetScanEnable ();

	DispatchStackEvent (BTH_STACK_UP);

	gpL2CAP->DelRef ();

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"ConnectStack:: ERROR_SUCCESS\n"));
	gpL2CAP->Unlock ();

	return ERROR_SUCCESS;
}

static DWORD WINAPI StackReset (LPVOID pArg) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"Reset stack\n"));

	StackDown (NULL);
	StackUp (NULL);

	return NULL;
}

static DWORD WINAPI StackDisconnect (LPVOID pArg) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"Disconnect stack\n"));

	l2cap_CloseDriverInstance ();

	return NULL;
}

//
//	The MEAT
//
static void AbortCall (CallContext *pCall, int iErr) {
	pCall->fComplete = TRUE;
	pCall->iResult = iErr;

	int fDel = ! pCall->fKeepOnAbort;

	if (pCall->eType == CALL_CTX_EVENT) {
		SetEvent (pCall->hEvent);
	} else if (pCall->eType == CALL_CTX_CALLBACK) {
		__try {
			pCall->pCallback (pCall, iErr);
		} __except (1) {
			IFDBG(DebugOut (DEBUG_ERROR, L"CALL_CTX_CALLBACK :: Excepted in AbortCall while processing callbacl\n"));
		}
	} else if (pCall->eType == CALL_CTX_CALLOWNER) {
		L2CAP_CONTEXT *pContext = VerifyContext (pCall->pOwner);
		if (pContext && pContext->c.l2ca_CallAborted) {
			BT_LAYER_CALL_ABORTED pCallback = pContext->c.l2ca_CallAborted;
			void *pCallContext = pCall->pContext;
			pContext->AddRef ();
			IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"AbortCall:: going in callback\n"));
			gpL2CAP->Unlock ();
			__try {
				pCallback (pCallContext, iErr);
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"CALL_CTX_CALLOWNER :: Excepted in AbortCall while processing callbacl\n"));
			}
			gpL2CAP->Lock ();
			IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"AbortCall:: back from callback\n"));
			pContext->DelRef ();
		}
	} else if (pCall->eType == CALL_CTX_INTERNAL)
		;
	else
		SVSUTIL_ASSERT (0);

	if (fDel && gpL2CAP->IsStackRunning ())
		DeleteCallContext (pCall);
}

static void DisconnectLogicalLink (LogLink *pThis, int iErr, int fSendDisconnect) {
	//	Take it out of the list
	if (! PluckLogLink (pThis)) {
		IFDBG(DebugOut (DEBUG_ERROR, L"Disconnecting non-existent link!\n"));
		return;
	}

	UnscheduleTimeout (pThis);

	if (! pThis->pPhysLink->pLogLinks)
		ScheduleTimeout (pThis->pPhysLink, gpL2CAP->dwPhysIdle);

	CallContext *pCall;
	//	Cancel all calls to it...
	while ((gpL2CAP->IsStackRunning ()) && (pCall = FindCall (pThis)))
		AbortCall (pCall, iErr);

	if (! gpL2CAP->IsStackRunning ())
		return;

	unsigned short cid = pThis->cid;
	unsigned short cid_remote = pThis->cid_remote;
	unsigned short h = (pThis->pPhysLink && (pThis->pPhysLink->eStage == UP)) ? pThis->pPhysLink->h : BT_HCI_INVALID_HANDLE;
	
	L2CAP_CONTEXT *pOwner = VerifyContext (pThis->pOwner);
	// Delete the thing.
	delete pThis;

	//	Signal disconnection, if required
	if (cid && pOwner && pOwner->ei.l2ca_DisconnectInd) {
		IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"DisconnectLogicalLink :: closing cid 0x%04x\n", cid));
		IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"DisconnectLogicalLink:: going into callback\n"));

		pOwner->AddRef ();
		gpL2CAP->Unlock ();

		__try {
			pOwner->ei.l2ca_DisconnectInd (pOwner->pUserContext, cid, iErr);
		} __except (1) {
			IFDBG(DebugOut (DEBUG_ERROR, L"DisconnectLogicalLink:: Exception in callback l2ca_DisconnectInd!\n"));
		}

		gpL2CAP->Lock ();
		pOwner->DelRef ();
		IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"DisconnectLogicalLink:: Back from callback\n"));
	}

	if ((gpL2CAP->eStage == Connected) && fSendDisconnect && (cid_remote != INVALID_CID) && (h != BT_HCI_INVALID_HANDLE)) {
		IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"Sending disconnect signal\n"));
		Signal s;
		s.cid = 1;
		s.length = sizeof(s.packet.h) + sizeof(s.packet.u.DISCONNECT_REQUEST);
		s.packet.h.code = L2CAP_COMMAND_DISCONNECT_REQUEST;
		s.packet.h.id = gpL2CAP->ucCurrentID++;
		if (! s.packet.h.id)
			s.packet.h.id = gpL2CAP->ucCurrentID++;
		s.packet.h.length = sizeof(s.packet.u.DISCONNECT_REQUEST);
		s.packet.u.DISCONNECT_REQUEST.dest_cid = cid_remote;
		s.packet.u.DISCONNECT_REQUEST.source_cid = cid;

		int iRes = WriteDataDown (h, NULL, SIGNAL_LENGTH(s), (unsigned char *)&s);

		IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"WriteDataDown returns %d\n", iRes));
	}
}

static int DisconnectPhysicalLink (PhysLink *pPhysLink, int fReconnect, int iErr, CallContext *pCallContext) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"DisconnectPhysicalLink :: %04x%08x reconnect = %d\n", pPhysLink->b.NAP, pPhysLink->b.SAP, fReconnect));

	SVSUTIL_ASSERT((! pCallContext) || (! fReconnect));

	UnscheduleTimeout (pPhysLink);

	if ((pPhysLink->iConnectionAttempts > L2CAP_RECONNECT) || (gpL2CAP->eStage == Disconnected))
		fReconnect = FALSE;

	if (! fReconnect)
		PluckPhysLink (pPhysLink);

	while (gpL2CAP->IsStackRunning () && pPhysLink->pLogLinks) {
		SVSUTIL_ASSERT (! pCallContext);
		LogLink *pThis = pPhysLink->pLogLinks;

		if (fReconnect) {
			while (pThis && ((pThis->eStage == STARTING) || (pThis->eStage == STARTING_PHYS)))
				pThis = pThis->pNext;

			if (! pThis)
				break;
		}

		DisconnectLogicalLink (pThis, iErr, FALSE);
	}

	if (! gpL2CAP->IsStackRunning ())
		return ERROR_SHUTDOWN_IN_PROGRESS;

	if (fReconnect && (! pPhysLink->pLogLinks)) {
		fReconnect = FALSE;
		PluckPhysLink (pPhysLink);
	}

	if (! fReconnect) {
		CallContext *pCall;
		while (gpL2CAP->IsStackRunning () && (pCall = FindCall (pPhysLink))) {
			SVSUTIL_ASSERT (! pCallContext);
			AbortCall (pCall, iErr);
		}

		if (gpL2CAP->eStage == Connected) {
			unsigned short h = pPhysLink->h;
			HCI_Disconnect_In pCallback = (pPhysLink->eStage == UP) ? gpL2CAP->hci_if.hci_Disconnect_In : NULL;
			HANDLE hHCI = gpL2CAP->hHCI;
			IFDBG(BD_ADDR b = pPhysLink->b);

			delete pPhysLink;

			if (pCallback) {
				IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"Taking down connection to %04x%08x handle 0x%04x\n", b.NAP, b.SAP, h));

				IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Going into hci_Disconnect_In callback\n"));
				gpL2CAP->AddRef ();
				gpL2CAP->Unlock ();

				int iRes = ERROR_INTERNAL_ERROR;
				__try {
					iRes = pCallback (hHCI, pCallContext, h, 0x13);
				} __except (1) {
					IFDBG(DebugOut (DEBUG_ERROR, L"Exception in hci_Disconnect_In\n"));
				}
				gpL2CAP->Lock ();
				gpL2CAP->DelRef ();

				IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Came from hci_Disconnect_In callback\n"));

				return gpL2CAP->eStage == Connected ? iRes : ERROR_SHUTDOWN_IN_PROGRESS;
			}

			return ERROR_SUCCESS;
		}

		return ERROR_SHUTDOWN_IN_PROGRESS;
	}

	SVSUTIL_ASSERT (! pCallContext);

	++pPhysLink->iConnectionAttempts;

	pPhysLink->eStage = STARTING;
	pPhysLink->h = BT_HCI_INVALID_HANDLE;
	pPhysLink->iPingsSent = 0;
	pPhysLink->iTransmissionProblems = 0;
	pPhysLink->dwTimeOutCookie = 0;

	LogLink *pLog = pPhysLink->pLogLinks;
	while (pLog) {
		SVSUTIL_ASSERT ((pLog->eStage == STARTING) || (pLog->eStage == STARTING_PHYS));
		pLog->cid = 0;
		pLog->eStage = STARTING_PHYS;
		pLog = pLog->pNext;
	}

	BD_ADDR b = pPhysLink->b;
	HANDLE hHCI = gpL2CAP->hHCI;
	HCI_CreateConnection_In pHCICall = gpL2CAP->hci_if.hci_CreateConnection_In;
	unsigned short usPacketType = pPhysLink->usPacketType ? pPhysLink->usPacketType : gpL2CAP->usPacketType;

	BT_LAYER_IO_CONTROL pIoctl = gpL2CAP->hci_if.hci_ioctl;

	int iRes = ERROR_INTERNAL_ERROR;

	CallContext *pCall = FindCall (pPhysLink, CALL_PHYS_CONNECT); // Must!

	if (pCall) {
		IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"going into in hci_CreateConnection_In\n"));
		gpL2CAP->AddRef ();
		gpL2CAP->Unlock ();
		__try {
			InquiryResultBuffer irb;
			int cSizeRet = 0;
			iRes = pIoctl (hHCI, BTH_HCI_IOCTL_GET_LAST_INQUIRY_DATA, sizeof(b), (char *)&b,
						sizeof(irb), (char *)&irb, &cSizeRet);

			if ((iRes != ERROR_SUCCESS) || (cSizeRet != sizeof(irb)))
				memset (&irb, 0, sizeof(irb));

			iRes = pHCICall (hHCI, pCall, &b, usPacketType, irb.page_scan_repetition_mode, irb.page_scan_mode, irb.clock_offset, 1);
		} __except (1) {
			IFDBG(DebugOut (DEBUG_ERROR, L"exception in hci_CreateConnection_In\n"));
		}
		gpL2CAP->Lock ();
		gpL2CAP->DelRef ();
		IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"came out of hci_CreateConnection_In\n"));
	} else
		IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] DisconnectPhysicalLink :: Call not matched!\n"));

	if (iRes != ERROR_SUCCESS)
		return DisconnectPhysicalLink (pPhysLink, FALSE, iRes, NULL);

	return iRes;
}

static int RequestPhysicalLink (CallContext *pCall) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"RequestPhysicalLink : %04x%08x\n", pCall->u.pPhysLink->b.NAP, pCall->u.pPhysLink->b.SAP));
	SVSUTIL_ASSERT (pCall->eWhat == CALL_PHYS_CONNECT);
	SVSUTIL_ASSERT (pCall->eType == CALL_CTX_INTERNAL);
	SVSUTIL_ASSERT (VerifyPhys (pCall->u.pPhysLink));
	SVSUTIL_ASSERT (gpL2CAP->eStage == Connected);

	HANDLE hHCI = gpL2CAP->hHCI;
	if (! hHCI) {
		IFDBG(DebugOut (DEBUG_ERROR, L"RequestPhysicalLink :: hci does not support physical links\n"));
		return ERROR_INTERNAL_ERROR;
	}

	HCI_CreateConnection_In pHCICall = gpL2CAP->hci_if.hci_CreateConnection_In;
	unsigned short usPacketType = pCall->u.pPhysLink->usPacketType ? pCall->u.pPhysLink->usPacketType : gpL2CAP->usPacketType;

	BT_LAYER_IO_CONTROL pIoctl = gpL2CAP->hci_if.hci_ioctl;

	BD_ADDR b = pCall->u.pPhysLink->b;

	gpL2CAP->AddRef ();
	gpL2CAP->Unlock ();

	int iRes = ERROR_INTERNAL_ERROR;

	__try {
		InquiryResultBuffer irb;
		int cSizeRet = 0;
		iRes = pIoctl (hHCI, BTH_HCI_IOCTL_GET_LAST_INQUIRY_DATA, sizeof(b), (char *)&b,
						sizeof(irb), (char *)&irb, &cSizeRet);

		if ((iRes != ERROR_SUCCESS) || (cSizeRet != sizeof(irb)))
			memset (&irb, 0, sizeof(irb));

		iRes = pHCICall (hHCI, pCall, &b, usPacketType, irb.page_scan_repetition_mode, irb.page_scan_mode, irb.clock_offset, 1);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] RequestPhysicalLink :: Exception in hci_CreateConnection_In\n"));
	}

	gpL2CAP->Lock ();
	gpL2CAP->DelRef ();

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"RequestPhysicalLink : %04x%08x returns %d\n", b.NAP, b.SAP, iRes));

	return iRes;
}

static int MakePhysicalLink (L2CAP_CONTEXT *pOwner, BD_ADDR *pba, PhysLink **ppLink) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"MakePhysicalLink %04x%08x\n", pba->NAP, pba->SAP));

	if (gpL2CAP->eStage != Connected) {
		IFDBG(DebugOut (DEBUG_ERROR, L"MakePhysicalLink :: ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	*ppLink = FindPhys (pba);
	if (*ppLink) {
		IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"MakePhysicalLink :: found existing\n"));
		return ERROR_SUCCESS;
	}

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"MakePhysicalLink :: making new\n"));

	*ppLink = CreateNewPhysLink ();
	if (!  *ppLink) {
		IFDBG(DebugOut (DEBUG_ERROR, L"MakePhysicalLink :: ERROR_OUTOFMEMORY\n"));
		return ERROR_OUTOFMEMORY;
	}

	CallContext *pCall = AllocCallContext (CALL_CTX_INTERNAL, CALL_PHYS_CONNECT, *ppLink, NULL, NULL);
	if (! pCall) {
		delete *ppLink;
		*ppLink = NULL;
		IFDBG(DebugOut (DEBUG_ERROR, L"MakePhysicalLink :: ERROR_OUTOFMEMORY\n"));
		return ERROR_OUTOFMEMORY;
	}

	(*ppLink)->b = *pba;
	(*ppLink)->usPacketType = pOwner->usPacketType;
	(*ppLink)->pNext = gpL2CAP->pPhysLinks;
	gpL2CAP->pPhysLinks = *ppLink;

	int iRes = ERROR_INTERNAL_ERROR;

	if (! gpL2CAP->fPicoCapable) {
		//
		//	Note, this might look funny, but if there's a single existing "live" link - better don't mess with it
		//	and just go through with the connection.
		//
		PhysLink *pPhys = gpL2CAP->pPhysLinks;
		while (pPhys && (pPhys->eStage != UP))
			pPhys = pPhys->pNext;

		if (pPhys && (! pPhys->pLogLinks) && (! FindCall (pPhys))) {	// Timeouting link...
			IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"MakePhysicalLink :: disconnecting idle connection (to %04x%08x) first\n", pPhys->b.NAP, pPhys->b.SAP));
			iRes = DisconnectPhysicalLink (pPhys, FALSE, ERROR_SUCCESS, pCall);
		} else
			IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"MakePhysicalLink :: Idle connection not found!\n"));
	} else
		IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"MakePhysicalLink :: hardware is piconet-capable!\n"));
	
	if ((gpL2CAP->eStage == Connected) && (iRes != ERROR_SUCCESS) && VerifyCall (pCall))
		iRes = RequestPhysicalLink (pCall);

	if ((iRes != ERROR_SUCCESS) && VerifyPhys (*ppLink)) {
		DisconnectPhysicalLink (*ppLink, FALSE, iRes, NULL);
		*ppLink = NULL;
	}

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"MakePhysicalLink :: returning %d\n", iRes));

	return iRes;
}

static int SendConnectionRequest (CallContext *pCall, int fAbortCall) {
	SVSUTIL_ASSERT (VerifyLog (pCall->u.pLogLink));
	SVSUTIL_ASSERT (pCall->eWhat == CALL_LOG_CONNECT_REQ);
	SVSUTIL_ASSERT (pCall->eType == CALL_CTX_CALLOWNER);

	LogLink *pLink = pCall->u.pLogLink;

	pLink->cid = GetCID ();
	pLink->eStage = STARTING;

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"Sending connection request for psm 0x%04x (local cid = 0x%04x, id = 0x%02x)\n",
		pLink->psm, pLink->cid, pCall->id));

	if (! gpL2CAP->hHCI) {
		IFDBG(DebugOut (DEBUG_ERROR, L"HCI disconnected!\n"));

		if (fAbortCall)
			DisconnectLogicalLink (pLink, ERROR_INTERNAL_ERROR, FALSE);

		return ERROR_INTERNAL_ERROR;
	}

	Signal s;
	s.cid = 1;
	s.length = sizeof (s.packet.h) + sizeof(s.packet.u.CONNECTION_REQUEST);
	s.packet.h.code = L2CAP_COMMAND_CONNECTION_REQUEST;
	s.packet.h.id = pCall->id;
	s.packet.h.length = sizeof(s.packet.u.CONNECTION_REQUEST);
	s.packet.u.CONNECTION_REQUEST.psm = pLink->psm;
	s.packet.u.CONNECTION_REQUEST.source_cid = pLink->cid;

	int iRes = WriteDataDown (pLink->pPhysLink->h, pCall, SIGNAL_LENGTH(s), (unsigned char *)&s);

	if (fAbortCall && (iRes != ERROR_SUCCESS) && gpL2CAP->IsStackRunning () && (pLink == VerifyLog (pLink)))
		DisconnectLogicalLink (pLink, iRes, FALSE);

	return iRes;
}

static int MakeLogicalLink (L2CAP_CONTEXT *pOwner, void *pCallContext, PhysLink *pPhysLink, unsigned short psm) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"MakeLogicalLink to %04x%08x psm 0x%04x\n", pPhysLink->b.NAP, pPhysLink->b.SAP, psm));

	LogLink *pLink = CreateNewLogLink (pPhysLink, pOwner);
	if (! pLink) {
		IFDBG(DebugOut (DEBUG_ERROR, L"MakeLogicalLink : ERROR_OUTOFMEMORY\n"));
		return ERROR_OUTOFMEMORY;
	}

	CallContext *pCall = AllocCallContext (CALL_CTX_CALLOWNER, CALL_LOG_CONNECT_REQ, pLink, pOwner, pCallContext);
	if (! pCall) {
		PluckLogLink (pLink);
		IFDBG(DebugOut (DEBUG_ERROR, L"MakeLogicalLink : ERROR_OUTOFMEMORY (no call)\n"));

		return ERROR_OUTOFMEMORY;
	}

	pLink->psm = psm;

	int iRes = ERROR_SUCCESS;
	if (pPhysLink->eStage == UP)
		iRes = SendConnectionRequest (pCall, FALSE);
	else
		pLink->eStage = STARTING_PHYS;

	if (iRes != ERROR_SUCCESS) {
		IFDBG(DebugOut (DEBUG_ERROR, L"MakeLogicalLink failed (%d) - deleting link\n", iRes));
		DeleteCallContext (pCall);

		if (VerifyLog (pLink) && PluckLogLink (pLink))
			delete pLink;
	}

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"MakeLogicalLink returns %d\n", iRes));

	return iRes;
}

static void RegisterTransmissionError (PhysLink *pPhys) {
	++pPhys->iTransmissionProblems;
	if (pPhys->iTransmissionProblems > L2CAP_MAXBAD)
		DisconnectPhysicalLink (pPhys, FALSE, ERROR_INVALID_DATA, NULL);
}

static int CancelCall (CallContext *pCall, int iError) {
	int fAbort = TRUE;
	switch (pCall->eWhat) {
	case CALL_PHYS_CONNECT:			// Close physical connection, too
	case CALL_PHYS_ACCEPT:
		DisconnectPhysicalLink (pCall->u.pPhysLink, FALSE, iError, NULL);
		fAbort = FALSE;
		break;

	case CALL_LOG_CONNECT_REQ:		// Close logical connection, too
	case CALL_LOG_CONNECT_RESP:
		if (pCall->u.pLogLink)
			DisconnectLogicalLink (pCall->u.pLogLink, iError, FALSE);
		fAbort = FALSE;
		break;

	case CALL_LOG_CONFIG_REQ:
	case CALL_LOG_CONFIG_RESP:
		DisconnectLogicalLink (pCall->u.pLogLink, iError, TRUE);
		fAbort = FALSE;
		break;

	case CALL_PHYS_DISCONNECT:
	case CALL_PHYS_DROP_IDLE:
	case CALL_PHYS_PING:
	case CALL_HCI_READSCAN:
	case CALL_HCI_WRITESCAN:
	case CALL_LOG_DISCONNECT_REQ:	// Do nothing, just cancel call
	case CALL_USERDATA:
		break;

	default:
		SVSUTIL_ASSERT (0);
	}

	if (fAbort)
		AbortCall (pCall, iError);

	return ERROR_SUCCESS;
}

//
//	HCI callbacks
//
static int hci_read_scan_enable_out (void *pCallContext, unsigned char status, unsigned char scan_enable) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_read_scan_enable_out: status = %d, scan = %d\n", status, scan_enable));

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_read_scan_enable_out: ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpL2CAP->Lock ();

	if ((gpL2CAP->eStage != Connected) && (gpL2CAP->eStage != ShuttingDown)) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_read_scan_enable_out: ERROR_SERVICE_NOT_ACTIVE\n"));
		gpL2CAP->Unlock ();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	CallContext *pCall = VerifyCall ((CallContext *)pCallContext);
	if (! pCall) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_read_scan_enable_out: ERROR_NOT_FOUND\n"));
		gpL2CAP->Unlock ();
		return ERROR_NOT_FOUND;
	}

	SVSUTIL_ASSERT (pCall->eType == CALL_CTX_EVENT);
	SVSUTIL_ASSERT (pCall->eWhat == CALL_HCI_READSCAN);
	SVSUTIL_ASSERT (pCall->pOwner == NULL);
	SVSUTIL_ASSERT (pCall->fKeepOnAbort);

	pCall->r.ucresult[0] = scan_enable;

	AbortCall (pCall, StatusToError (status, ERROR_INTERNAL_ERROR));

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_read_scan_enable_out: ERROR_SUCCESS\n"));
	gpL2CAP->Unlock ();
	return ERROR_SUCCESS;
}

static int hci_disconnect_out (void *pCallContext, unsigned char status) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_disconnect_out: status = %d\n", status));

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_disconnect_out: ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpL2CAP->Lock ();

	if ((gpL2CAP->eStage != Connected) && (gpL2CAP->eStage != ShuttingDown)) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_disconnect_out: ERROR_SERVICE_NOT_ACTIVE\n"));
		gpL2CAP->Unlock ();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	CallContext *pCall = VerifyCall ((CallContext *)pCallContext);
	if (! pCall) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_disconnect_out: ERROR_NOT_FOUND\n"));
		gpL2CAP->Unlock ();
		return ERROR_NOT_FOUND;
	}

	if (status) {
		if (pCall->eWhat == CALL_PHYS_CONNECT) {
			//
			//	Note, this might look funny, but if there's a single existing "live" link - better don't mess with it
			//	and just go through with the connection.
			//
			int iRes = ERROR_INTERNAL_ERROR;

			PhysLink *pPhys = gpL2CAP->pPhysLinks;

			while (pPhys && (pPhys->eStage != UP))
				pPhys = pPhys->pNext;

			if (pPhys && (! pPhys->pLogLinks) && (! FindCall (pPhys))) {	// Timeouting link...
				IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_disconnect_out :: disconnecting idle connection (to %04x%08x) first\n", pPhys->b.NAP, pPhys->b.SAP));
				iRes = DisconnectPhysicalLink (pPhys, FALSE, ERROR_SUCCESS, pCall);
			} else
				IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_disconnect_out :: no idle connection detected, going into connect\n"));

			if ((iRes != ERROR_SUCCESS) && (gpL2CAP->eStage == Connected) && VerifyCall (pCall)) {
				pPhys = pCall->u.pPhysLink;

				SVSUTIL_ASSERT (VerifyPhys (pPhys));

				iRes = RequestPhysicalLink (pCall);

				if ((iRes != ERROR_SUCCESS) && VerifyPhys (pPhys))
					DisconnectPhysicalLink (pPhys, FALSE, iRes, NULL);
			}
		} else if (pCall->eWhat == CALL_PHYS_DROP_IDLE) {
			AbortCall (pCall, StatusToError (status, ERROR_INTERNAL_ERROR));
		} else {
			SVSUTIL_ASSERT (pCall->eType == CALL_CTX_EVENT);
			SVSUTIL_ASSERT (pCall->eWhat == CALL_PHYS_DISCONNECT);
			SVSUTIL_ASSERT (pCall->pOwner == NULL);
			SVSUTIL_ASSERT (pCall->fKeepOnAbort);

			AbortCall (pCall, StatusToError (status, ERROR_INTERNAL_ERROR));
		}
	}

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_disconnect_out: ERROR_SUCCESS\n"));
	gpL2CAP->Unlock ();
	return ERROR_SUCCESS;
}

static int hci_disconnection_complete_event (void *pUserContext, void *pCallContext, unsigned char status, unsigned short connection_handle, unsigned char reason) {
	SVSUTIL_ASSERT (pUserContext == gpL2CAP);

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_disconnection_complete_event: connection 0x%04x status %d\n", connection_handle, status));
	if ((! gpL2CAP) || (pUserContext != gpL2CAP)) {
		IFDBG(DebugOut (DEBUG_ERROR, L"Completely desynchronized\n"));
		return ERROR_INTERNAL_ERROR;
	}

	gpL2CAP->Lock ();

	CallContext *pCall = VerifyCall ((CallContext *)pCallContext);
	if (pCall) {
		if (pCall->eWhat == CALL_PHYS_CONNECT) {
			//
			//	Note, this might look funny, but if there's a single existing "live" link - better don't mess with it
			//	and just go through with the connection.
			//
			int iRes = ERROR_INTERNAL_ERROR;

			PhysLink *pPhys = gpL2CAP->pPhysLinks;

			while (pPhys && (pPhys->eStage != UP))
				pPhys = pPhys->pNext;

			if (pPhys && (! pPhys->pLogLinks) && (! FindCall (pPhys))) {	// Timeouting link...
				IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_disconnection_complete_event :: disconnecting idle connection (to %04x%08x) first\n", pPhys->b.NAP, pPhys->b.SAP));
				iRes = DisconnectPhysicalLink (pPhys, FALSE, ERROR_SUCCESS, pCall);
			} else
				IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_disconnection_complete_event :: no idle connection detected, going into connect\n"));

			if ((iRes != ERROR_SUCCESS) && (gpL2CAP->eStage == Connected) && VerifyCall (pCall)) {
				pPhys = pCall->u.pPhysLink;

				SVSUTIL_ASSERT (VerifyPhys (pPhys));

				iRes = RequestPhysicalLink (pCall);

				if ((iRes != ERROR_SUCCESS) && VerifyPhys (pPhys))
					DisconnectPhysicalLink (pPhys, FALSE, iRes, NULL);
			}
		} else if (pCall->eWhat == CALL_PHYS_DROP_IDLE) {
			IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_disconnection_complete_event :: disconnected idle connection...\n"));

			SVSUTIL_ASSERT (pCall->eType == CALL_CTX_CALLOWNER);
			SVSUTIL_ASSERT (! pCall->fKeepOnAbort);

			L2CAP_CONTEXT *pOwner = pCall->pOwner;
			BT_LAYER_STACK_EVENT_IND pCallback = pOwner->ei.l2ca_StackEvent;
			if (! pCallback) { // eat the response...
				if (pCall)
					AbortCall (pCall, ERROR_CALL_NOT_IMPLEMENTED);

				IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] hci_disconnection_complete_event : No callback, or no connection\n"));
			} else {
				void *pCallContext = pCall->pContext;
				DeleteCallContext (pCall);

				//	If we have more than one open 
				IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Going into l2ca_StackEvent\n"));
				pOwner->AddRef ();
				gpL2CAP->Unlock ();
				__try {
					pCallback (pOwner, BTH_STACK_L2CAP_DROP_COMPLETE, pCallContext);
				} __except (1) {
					IFDBG(DebugOut (DEBUG_ERROR, L"Exception in l2ca_StackEvent!!!\n"));
				}
				gpL2CAP->Lock ();
				pOwner->DelRef ();
				IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Came from l2ca_StackEvent\n"));
			}
		} else {
			SVSUTIL_ASSERT (pCall->eType == CALL_CTX_EVENT);
			SVSUTIL_ASSERT (pCall->eWhat == CALL_PHYS_DISCONNECT);
			SVSUTIL_ASSERT (pCall->pOwner == NULL);
			SVSUTIL_ASSERT (pCall->fKeepOnAbort);
			SVSUTIL_ASSERT (pCall->u.pPhysLink == FindPhys (connection_handle));

			pCall->fComplete = TRUE;
			pCall->iResult = StatusToError (status, ERROR_INTERNAL_ERROR);
	
			if (pCall->eType == CALL_CTX_EVENT)
				SetEvent (pCall->hEvent);
		}
	}

	if ((status == 0) && (gpL2CAP->eStage == Connected)) {		// Get rid of all logical connections
		PhysLink *pPhys = FindPhys (connection_handle);
		if (pPhys) {
			pPhys->eStage = DISCONNECTED;
			DisconnectPhysicalLink (pPhys, TRUE, ERROR_CONNECTION_UNAVAIL, NULL);
		}
	}

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_disconnection_complete_event: ERROR_SUCCESS\n"));
	gpL2CAP->Unlock ();
	return ERROR_SUCCESS;
}

static int hci_create_connection_out (void *pCallContext, unsigned char status) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_create_connection_out: status = %d\n", status));

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_create_connection_out: ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpL2CAP->Lock ();

	if (gpL2CAP->eStage != Connected) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_create_connection_out: ERROR_SERVICE_NOT_ACTIVE\n"));
		gpL2CAP->Unlock ();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	CallContext *pCall = VerifyCall ((CallContext *)pCallContext);

	if (! pCall) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_create_connection_out: ERROR_NOT_FOUND\n"));
		gpL2CAP->Unlock ();
		return ERROR_NOT_FOUND;
	}

	SVSUTIL_ASSERT (VerifyPhys (pCall->u.pPhysLink));
	SVSUTIL_ASSERT (pCall->eWhat == CALL_PHYS_CONNECT);
	SVSUTIL_ASSERT (pCall->eType == CALL_CTX_INTERNAL);
	SVSUTIL_ASSERT (pCall->pOwner == NULL);
	SVSUTIL_ASSERT (! pCall->fKeepOnAbort);

	PhysLink *pLink = pCall->u.pPhysLink;

	if (status) {
		if (pLink->eStage == STARTING)
			DisconnectPhysicalLink (pLink, FALSE, StatusToError (status, ERROR_INTERNAL_ERROR), NULL);
		else
			IFDBG(DebugOut (DEBUG_WARN, L"hci_create_connection_out : failed with %d but allowing link to %04x%08x exist (it's UP)\n", status, pLink->b.NAP, pLink->b.SAP));
	}

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_create_connection_out: ERROR_SUCCESS\n"));
	gpL2CAP->Unlock ();
	return ERROR_SUCCESS;
}

static int hci_connection_complete_event
(
void			*pUserContext,
void			*pCallContext,
unsigned char	status,
unsigned short	connection_handle,
BD_ADDR			*pba,
unsigned char	link_type,
unsigned char	encryption_mode
) {
	SVSUTIL_ASSERT (pUserContext == gpL2CAP);

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_connection_complete_event: status = %d\n", status));

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_connection_complete_event: ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpL2CAP->Lock ();

	if (gpL2CAP->eStage != Connected) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_connection_complete_event: ERROR_SERVICE_NOT_ACTIVE\n"));
		gpL2CAP->Unlock ();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	CallContext *pCall = VerifyCall ((CallContext *)pCallContext);

	if (! pCall) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_connection_complete_event: ERROR_NOT_FOUND\n"));
		gpL2CAP->Unlock ();
		return ERROR_NOT_FOUND;
	}

	SVSUTIL_ASSERT (VerifyPhys (pCall->u.pPhysLink));
	SVSUTIL_ASSERT ((pCall->eWhat == CALL_PHYS_CONNECT) || (pCall->eWhat == CALL_PHYS_ACCEPT));
	SVSUTIL_ASSERT (pCall->eType == CALL_CTX_INTERNAL);
	SVSUTIL_ASSERT (pCall->pOwner == NULL);
	SVSUTIL_ASSERT (! pCall->fKeepOnAbort);

	PhysLink *pLink = pCall->u.pPhysLink;

	SVSUTIL_ASSERT (pLink->b == *pba);

	if (status) {
		int iError = StatusToError (status, ERROR_INTERNAL_ERROR);
		int fRetry = FALSE;
		switch (status) {
		case BT_ERROR_NO_CONNECTION:
		case BT_ERROR_HARDWARE_FAILURE:
		case BT_ERROR_MAX_NUMBER_OF_CONNECTIONS:
		case BT_ERROR_MAX_NUMBER_OF_ACL_CONNECTIONS:
		case BT_ERROR_HOST_REJECTED_LIMITED_RESOURCES:
		case BT_ERROR_OETC_LOW_RESOURCES:
		case BT_ERROR_UNSUPPORTED_REMOTE_FEATURE:
			fRetry = TRUE;
		}

		DisconnectPhysicalLink (pLink, fRetry, iError, NULL);
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_connection_complete_event : no connection, status = %d\n", status));
		gpL2CAP->Unlock ();
		return ERROR_SUCCESS;
	}

	DeleteCallContext (pCall);

	SVSUTIL_ASSERT (link_type == 1);
	SVSUTIL_ASSERT (pLink->h == BT_HCI_INVALID_HANDLE);

	pLink->h = connection_handle;
	pLink->eStage = UP;
	pLink->iConnectionAttempts = 0;
	pLink->iPingsSent = 0;
	pLink->iTransmissionProblems = 0;
	pLink->dwTimeOutCookie = 0;

	// Now do the thing for L2CAP connections...
#if defined (DEBUG) || defined (_DEBUG)
	{
		LogLink *pLog = pLink->pLogLinks;
		while (pLog) {
			SVSUTIL_ASSERT (pLog->eStage == STARTING_PHYS);
			pLog = pLog->pNext;
		}
	}
#endif

	if (! pLink->pLogLinks)
		ScheduleTimeout (pLink, gpL2CAP->dwConnectIdle);

	// If we have link policy, enable it.
	WriteLinkPolicy (connection_handle);

	// Do logical connections
	while ((gpL2CAP->eStage == Connected) && (pLink == VerifyPhys (pLink))) {
		LogLink *pLog = pLink->pLogLinks;
		while (pLog && (pLog->eStage != STARTING_PHYS))
			pLog = pLog->pNext;

		if (! pLog)
			break;

		CallContext *pCall = FindCall (pLog, CALL_LOG_CONNECT_REQ);
		if (pCall)
			SendConnectionRequest (pCall, TRUE);
		else {
			IFDBG(DebugOut (DEBUG_ERROR, L"Orphaned logical connection in hci_connection_complete_event\n"));
			DisconnectLogicalLink (pLog, ERROR_INTERNAL_ERROR, FALSE);
		}
	}

	//	Send pings
	while ((gpL2CAP->eStage == Connected) && (pLink == VerifyPhys (pLink))) {
		CallContext *pCall = gpL2CAP->pCalls;
		while (pCall && ((pCall->eWhat != CALL_PHYS_PING) || (! pCall->pData)))
			pCall = pCall->pNext;

		if (pCall) {
			ScheduleTimeout (pCall, gpL2CAP->RTX);

			unsigned char *pBuf = (unsigned char *)pCall->pData;
			int cSize = pCall->cData;

			pCall->pData = NULL;
			pCall->cData = 0;

			int iRes = WriteDataDown (connection_handle, pCall, cSize, pBuf);

			if ((iRes != ERROR_SUCCESS) && VerifyCall (pCall))
				CancelCall (pCall, iRes);
			
			g_funcFree (pBuf, g_pvFreeData);
		} else
			break;
	}

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_connection_complete_event : ERROR_SUCCESS\n"));
	gpL2CAP->Unlock ();

	return ERROR_SUCCESS;
}

static int hci_connection_request_event
(
void			*pUserContext,
void			*pCallContext,
BD_ADDR			*pba,
unsigned int	class_of_device,
unsigned char	link_type
) {
	SVSUTIL_ASSERT (pUserContext == gpL2CAP);
	SVSUTIL_ASSERT (! pCallContext);

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_connection_request_event: %04x%08x cod = 0x%08x link = %d\n", pba->NAP, pba->SAP, class_of_device, link_type));

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_connection_request_event: ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpL2CAP->Lock ();

	if (gpL2CAP->eStage != Connected) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_connection_request_event: ERROR_SERVICE_NOT_ACTIVE\n"));
		gpL2CAP->Unlock ();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	PhysLink *pLink = NULL;
	if (link_type == 1) {
		pLink = FindPhys (pba);
		if (! pLink) {
			pLink = CreateNewPhysLink ();
			if (pLink) {
				pLink->b = *pba;
				pLink->pNext = gpL2CAP->pPhysLinks;
				gpL2CAP->pPhysLinks = pLink;
			}
		}
	}

	if (pLink) {
		int iRes = ERROR_INTERNAL_ERROR;

		CallContext *pCall = AllocCallContext (CALL_CTX_INTERNAL, CALL_PHYS_ACCEPT, pLink, NULL, NULL);
		HCI_AcceptConnectionRequest_In pHCICall = gpL2CAP->hci_if.hci_AcceptConnectionRequest_In;
		HANDLE hHCI = gpL2CAP->hHCI;

		if (hHCI && pCall) {
			IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Entering hci_AcceptConnectionRequest_In\n"));
			gpL2CAP->AddRef ();
			gpL2CAP->Unlock ();
			__try {
				iRes = pHCICall (hHCI, pCall, pba, gpL2CAP->bRole); // role == 0 => Become master...
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Exception in hci_AcceptConnectionRequest_In\n"));
			}
			gpL2CAP->Lock ();
			gpL2CAP->DelRef ();
			IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Came from hci_AcceptConnectionRequest_In\n"));
		} else if (! pCall) {
			IFDBG(DebugOut (DEBUG_ERROR, L" : ERROR_OUTOFMEMORY\n"));
			iRes = ERROR_OUTOFMEMORY;
		} else
			IFDBG(DebugOut (DEBUG_ERROR, L"HCI disconnected!\n"));

		if (iRes != ERROR_SUCCESS) {
			DisconnectPhysicalLink (pLink, TRUE, iRes, NULL);
			pLink = NULL;
		}
	}

	if (! pLink) {	// Reject for no resources...
		HCI_RejectConnectionRequest_In pHCICall = gpL2CAP->hci_if.hci_RejectConnectionRequest_In;
		HANDLE hHCI = gpL2CAP->hHCI;

		int iRes = ERROR_INTERNAL_ERROR;

		if (hHCI) {
			IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Entering hci_RejectConnectionRequest_In\n"));
			gpL2CAP->AddRef ();
			gpL2CAP->Unlock ();
			__try {
				iRes = pHCICall (hHCI, NULL, pba, BT_ERROR_HOST_REJECTED_LIMITED_RESOURCES);
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Exception in hci_RejectConnectionRequest_In\n"));
			}
			gpL2CAP->Lock ();
			gpL2CAP->DelRef ();
		} else
			IFDBG(DebugOut (DEBUG_ERROR, L"HCI disconnected!\n"));
	}

	gpL2CAP->Unlock ();

	return ERROR_SUCCESS;
}

static int hci_accept_connection_request_out (void *pCallContext, unsigned char status) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_accept_connection_request_out: status = %d\n", status));

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_accept_connection_request_out: ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpL2CAP->Lock ();

	if (gpL2CAP->eStage != Connected) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_accept_connection_request_out: ERROR_SERVICE_NOT_ACTIVE\n"));
		gpL2CAP->Unlock ();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	CallContext *pCall = VerifyCall ((CallContext *)pCallContext);

	if (! pCall) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_connection_complete_event: ERROR_NOT_FOUND\n"));
		gpL2CAP->Unlock ();
		return ERROR_NOT_FOUND;
	}

	SVSUTIL_ASSERT (VerifyPhys (pCall->u.pPhysLink));
	SVSUTIL_ASSERT (pCall->eWhat == CALL_PHYS_ACCEPT);
	SVSUTIL_ASSERT (pCall->eType == CALL_CTX_INTERNAL);
	SVSUTIL_ASSERT (pCall->pOwner == NULL);
	SVSUTIL_ASSERT (! pCall->fKeepOnAbort);

	PhysLink *pLink = pCall->u.pPhysLink;

	if (status) {
		int iError = StatusToError (status, ERROR_INTERNAL_ERROR);
		int fRetry = FALSE;
		switch (status) {
		case BT_ERROR_NO_CONNECTION:
		case BT_ERROR_HARDWARE_FAILURE:
		case BT_ERROR_MAX_NUMBER_OF_CONNECTIONS:
		case BT_ERROR_MAX_NUMBER_OF_ACL_CONNECTIONS:
		case BT_ERROR_HOST_REJECTED_LIMITED_RESOURCES:
		case BT_ERROR_HOST_TIMEOUT:
		case BT_ERROR_OETC_LOW_RESOURCES:
		case BT_ERROR_UNSUPPORTED_REMOTE_FEATURE:
		case BT_ERROR_UNSPECIFIED_ERROR:
			fRetry = TRUE;
		}

		DisconnectPhysicalLink (pLink, fRetry, iError, NULL);
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_accept_connection_request_out - orphaned call\n"));
		gpL2CAP->Unlock ();
		return ERROR_SUCCESS;
	}

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_accept_connection_request_out : ERROR_SUCCESS\n"));
	gpL2CAP->Unlock ();

	return ERROR_SUCCESS;
}

static int hci_reject_connection_request_out (void *pNull, unsigned char status) {
	SVSUTIL_ASSERT (pNull == NULL);
	SVSUTIL_ASSERT (status == 0);
	return ERROR_SUCCESS;
}

static int hci_write_scan_enable_out (void *pNull, unsigned char status) {
	SVSUTIL_ASSERT (pNull == NULL);
	SVSUTIL_ASSERT (status == 0);
	return ERROR_SUCCESS;
}

static int hci_pin_code_request_negative_reply_out (void *pNull, unsigned char status, BD_ADDR *pba) {
	SVSUTIL_ASSERT (pNull == NULL);
	SVSUTIL_ASSERT (status == 0);
	return ERROR_SUCCESS;
}

static int hci_pin_code_request_reply_out (void *pNull, unsigned char status, BD_ADDR *pba) {
	SVSUTIL_ASSERT (pNull == NULL);
	SVSUTIL_ASSERT (status == 0);
	return ERROR_SUCCESS;
}

static int hci_link_key_request_negative_reply_out (void *pNull, unsigned char status, BD_ADDR *pba) {
	SVSUTIL_ASSERT (pNull == NULL);
	SVSUTIL_ASSERT (status == 0);
	return ERROR_SUCCESS;
}

static int hci_link_key_request_reply_out (void *pNull, unsigned char status, BD_ADDR *pba) {
	SVSUTIL_ASSERT (pNull == NULL);
	SVSUTIL_ASSERT (status == 0);
	return ERROR_SUCCESS;
}

static int hci_data_packet_up (void *pUserContext, unsigned short connection_handle, BD_BUFFER *pBuffer) {
	SVSUTIL_ASSERT (pUserContext == gpL2CAP);

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_data_packet_up: h = 0x%04x\n", connection_handle));

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_connection_request_event: ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpL2CAP->Lock ();

	IFDBG(DebugOut (DEBUG_L2CAP_PACKETS, L"Data Packet :: connection 0x%04x %d bytes\n", connection_handle, BufferTotal (pBuffer)));
	IFDBG(DumpBuff (DEBUG_L2CAP_PACKETS, pBuffer->pBuffer + pBuffer->cStart, BufferTotal (pBuffer)));

	if (gpL2CAP->eStage != Connected) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_connection_request_event: ERROR_SERVICE_NOT_ACTIVE\n"));

		if (pBuffer->pFree)
			pBuffer->pFree (pBuffer);

		gpL2CAP->Unlock ();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	// Gotta be L2CAP packet - do quick sanity check.
	PhysLink *pPhys = FindPhys (connection_handle);

	if ((! pPhys) || (pPhys->eStage != UP)) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_data_packet_up - orphaned call\n"));

		if (pBuffer->pFree)
			pBuffer->pFree (pBuffer);

		gpL2CAP->Unlock ();
		return ERROR_INTERNAL_ERROR;
	}

	unsigned short length = 0xffff;
	unsigned short cid = 0xffff;
	if ((! BufferGetShort (pBuffer, &length)) || (! BufferGetShort (pBuffer, &cid)) || (BufferTotal(pBuffer) != length) || (! length)) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_data_packet_up - malformed packet (length = 0x%04x, cid = 0x%04x, buffer bytes = 0x%04x)\n", cid, length, BufferTotal (pBuffer)));
		RegisterTransmissionError (pPhys);

		if (pBuffer->pFree)
			pBuffer->pFree (pBuffer);

		gpL2CAP->Unlock ();
		return ERROR_INVALID_DATA;
	}

	if (cid != 0x0001) {
		LogLink *pLog = FindCID (cid, pPhys);
		L2CAP_CONTEXT *pOwner = pLog ? VerifyContext (pLog->pOwner) : NULL;
		if ((! pLog) || (pLog->eStage != UP) || (! pOwner)) {
			IFDBG(DebugOut (DEBUG_ERROR, L"Wrong CID - no connection or not UP!\n"));
			if (pBuffer->pFree)
				pBuffer->pFree (pBuffer);

			gpL2CAP->Unlock ();
			return ERROR_INVALID_DATA;
		}

		IFDBG(DebugOut (DEBUG_L2CAP_PACKETS, L"Got data packet for cid 0x%04x\n", cid));

		L2CA_DataUpInd pCallback = pOwner->ei.l2ca_DataUpInd;
		void *pUserContext = pOwner->pUserContext;

		IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"hci_data_packet_up : routing packet up\n"));
		pOwner->AddRef ();
		gpL2CAP->Unlock ();
		int iRes = ERROR_INTERNAL_ERROR;
		__try {
			iRes = pCallback (pUserContext, cid, pBuffer);
		} __except(1) {
			IFDBG(DebugOut (DEBUG_ERROR, L"hci_data_packet_up :: exception while processing callback cid = 0x%04x\n", cid));
		}
		gpL2CAP->Lock ();
		pOwner->DelRef ();
		IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"hci_data_packet_up : routing packet up for cid 0x%04x finished\n", cid));

		gpL2CAP->Unlock ();

		return iRes;
	}

	//	Process signalling data

	IFDBG(DebugOut (DEBUG_L2CAP_PACKETS, L"Got signalling packet\n"));

	int cCurrentLength = 0;
	while ((cCurrentLength < length) && (gpL2CAP->eStage == Connected) && (pPhys == VerifyPhys (pPhys))) {
		SignallingPacket sp;
		if (! BufferGetChunk (pBuffer, sizeof(sp.h), (unsigned char *)&sp.h)) {
			IFDBG(DebugOut (DEBUG_ERROR, L"Signalling: Bad header!\n"));
			break;
		}

		cCurrentLength += sizeof(sp.h) + sp.h.length;

		if (sp.h.code == L2CAP_COMMAND_ECHO_REQUEST) {
			IFDBG(DebugOut (DEBUG_L2CAP_PACKETS, L"Got ECHO REQUEST on 0x%04x\n", connection_handle));
			IFDBG(DumpBuff (DEBUG_L2CAP_PACKETS, pBuffer));
			Signal echo_resp;
			echo_resp.cid = 0x0001;
			echo_resp.packet.h.code = L2CAP_COMMAND_ECHO_RESPONSE;
			echo_resp.packet.h.id = sp.h.id;
			wsprintf ((WCHAR *)echo_resp.packet.u.ECHO_RESPONSE.ucData, L"BT WinCE v%04d.%04d echo req id %03d response %d", L2CAP_V_MAJ, L2CAP_V_MIN, sp.h.id, gpL2CAP->iEchoes++);
			echo_resp.packet.h.length = sizeof(WCHAR) * (wcslen ((WCHAR *)echo_resp.packet.u.ECHO_RESPONSE.ucData) + 1);
			SVSUTIL_ASSERT (echo_resp.packet.h.length < sizeof(echo_resp.packet.u.ECHO_RESPONSE.ucData));
			echo_resp.length = echo_resp.packet.h.length + sizeof (echo_resp.packet.h);

			if (! pPhys->pLogLinks)
				ScheduleTimeout (pPhys, gpL2CAP->dwConnectIdle);

			WriteDataDown (connection_handle, NULL, SIGNAL_LENGTH(echo_resp), (unsigned char *)&echo_resp);
			continue;
		}

		if (sp.h.code == L2CAP_COMMAND_ECHO_RESPONSE) {
			IFDBG(DebugOut (DEBUG_L2CAP_PACKETS, L"Got ECHO RESPONSE on 0x%04x\n", connection_handle));
			IFDBG(DumpBuff (DEBUG_L2CAP_PACKETS, pBuffer));

			CallContext *pCall = FindCall (pPhys, CALL_PHYS_PING, sp.h.id);
			if (! pCall) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Orphaned PING packet id = %d, ignoring!\n", sp.h.id));
				continue;
			}

			L2CA_Ping_Out pCallback = pCall->pOwner->c.l2ca_Ping_Out;
			void *pContext = pCall->pContext;
			L2CAP_CONTEXT *pOwner = pCall->pOwner;
			BD_ADDR b = pPhys->b;
			unsigned char *pucB = (unsigned char *)g_funcAlloc (sp.h.length, g_pvAllocData);

			if (pucB && BufferGetChunk (pBuffer, sp.h.length, pucB) && pCallback) {
				DeleteCallContext (pCall);
				IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"going into l2ca_Ping_Out\n"));
				pOwner->AddRef ();
				gpL2CAP->Unlock ();
				__try {
					pCallback (pContext, &b, pucB, sp.h.length);
				} __except (1) {
					IFDBG(DebugOut (DEBUG_ERROR, L"Exception in l2ca_Ping_Out\n"));
				}
				gpL2CAP->Lock ();
				pOwner->DelRef ();
				IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Returned from l2ca_Ping_Out\n"));
				g_funcFree (pucB, g_pvFreeData);
			} else {
				int iRes = ERROR_OUTOFMEMORY;
				if (pucB) {
					if (pCallback)
						iRes = ERROR_INVALID_DATA;
					else
						iRes = ERROR_CALL_NOT_IMPLEMENTED;

					g_funcFree (pucB, g_pvFreeData);
				}
					
				IFDBG(DebugOut (DEBUG_ERROR, L"Signalling ping response: error %d\n", iRes));
				AbortCall (pCall, iRes);
			}

			continue;
		}

		if (sp.h.length > sizeof(sp.u)) {
			IFDBG(DebugOut (DEBUG_ERROR, L"Signalling: Way too big!\n"));
			break;
		}

		if (! BufferGetChunk (pBuffer, sp.h.length, (unsigned char *)&sp.u)) {
			IFDBG(DebugOut (DEBUG_ERROR, L"Signalling: Bad body!\n"));
			break;
		}

		if (sp.h.code == L2CAP_COMMAND_COMMAND_REJECT) {
			IFDBG(DebugOut (DEBUG_ERROR, L"Got COMMAND_REJECT on 0x%04x\n", connection_handle));
			if (sp.h.length < 2) {
				IFDBG(DebugOut (DEBUG_ERROR, L"hci_data_packet_up - malformed signalling packet\n"));
				break;
			}
			
			if (((sp.u.COMMAND_REJECT.reason == 0) && (sp.h.length != 2)) ||
				((sp.u.COMMAND_REJECT.reason == 1) && (sp.h.length != 4)) || 
				((sp.u.COMMAND_REJECT.reason == 2) && (sp.h.length != 6))) {
				IFDBG(DebugOut (DEBUG_ERROR, L"hci_data_packet_up - malformed signalling packet\n"));
				break;
			}

			CallContext *pCall = FindCall (pPhys, sp.h.id);
			if (! pCall) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Orphaned L2CAP COMMAND_REJECT call id %d\n", sp.h.id));
				continue;
			}

			int iRes = ERROR_INVALID_FUNCTION;
			if (sp.u.COMMAND_REJECT.reason == 0) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Command not understood\n"));
				iRes = ERROR_BAD_COMMAND;
			} else if (sp.u.COMMAND_REJECT.reason == 1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"MTU violation\n"));
				iRes = ERROR_BAD_LENGTH;
			} else if (sp.u.COMMAND_REJECT.reason == 2) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Wrong CID\n"));
				iRes = ERROR_CONNECTION_UNAVAIL;
			}

			CancelCall (pCall, iRes);
			continue;
		}

		if (sp.h.code == L2CAP_COMMAND_CONNECTION_RESPONSE) {
			IFDBG(DebugOut (DEBUG_L2CAP_PACKETS, L"Got connection response\n"));
			if (sp.h.length != 8) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Malformed connection response\n"));
				break;
			}

			unsigned short source_cid = sp.u.CONNECTION_RESPONSE.source_cid;
			if (! source_cid) {
				CallContext *pCall = gpL2CAP->pCalls;
				while (pCall && ((pCall->eWhat != CALL_LOG_CONNECT_REQ) ||
							    (! pCall->u.pLogLink) ||
								(pCall->u.pLogLink->pPhysLink != pPhys) ||
								(pCall->id != sp.h.id)))
					pCall = pCall->pNext;
				if (pCall && pCall->u.pLogLink)
					source_cid = pCall->u.pLogLink->cid;
				else
					IFDBG(DebugOut (DEBUG_ERROR, L"Could not match connect response to connect request!\n"));
			}

			LogLink *pLink = FindLog (source_cid, pPhys);
			if ((! pLink) || (pLink->eStage != STARTING)) {
				Signal s;
				s.cid = 1;
				s.length = sizeof(s.packet.h) + 6;
				s.packet.h.code = L2CAP_COMMAND_COMMAND_REJECT;
				s.packet.h.id   = sp.h.id;
				s.packet.h.length = 6;
				s.packet.u.COMMAND_REJECT.reason = 0x0002;		// Bad CID in request
				s.packet.u.COMMAND_REJECT.ucOptionalData[0] = sp.u.CONNECTION_RESPONSE.source_cid & 0xff;
				s.packet.u.COMMAND_REJECT.ucOptionalData[1] = (sp.u.CONNECTION_RESPONSE.source_cid >> 8) & 0xff;
				s.packet.u.COMMAND_REJECT.ucOptionalData[2] = sp.u.CONNECTION_RESPONSE.dest_cid & 0xff;
				s.packet.u.COMMAND_REJECT.ucOptionalData[3] = (sp.u.CONNECTION_RESPONSE.dest_cid >> 8) & 0xff;
				WriteDataDown (pPhys->h, NULL, SIGNAL_LENGTH(s), (unsigned char *)&s);
				continue;
			}

			CallContext *pCall = FindCall (pLink, CALL_LOG_CONNECT_REQ, sp.h.id);
			if (! pCall) {
				IFDBG(DebugOut (DEBUG_WARN, L"Silently discarding connection response with invalid id\n"));
				continue;
			}

			SVSUTIL_ASSERT (pCall->eType == CALL_CTX_CALLOWNER);
			SVSUTIL_ASSERT (pCall->eWhat == CALL_LOG_CONNECT_REQ);
			SVSUTIL_ASSERT (pCall->u.pLogLink == pLink);


			if (sp.u.CONNECTION_RESPONSE.result == 1) {	// Pending
				ScheduleTimeout (pLink, gpL2CAP->ERTX);
				continue;
			}
			
			UnscheduleTimeout (pLink);

			void *pUserContext = pCall->pContext;
			L2CAP_CONTEXT *pOwner = pCall->pOwner;

			L2CA_ConnectReq_Out pCallback = pOwner->c.l2ca_ConnectReq_Out;
			if (pCallback)
				DeleteCallContext (pCall);
			else {
				IFDBG(DebugOut (DEBUG_ERROR, L"Stack does not implement l2ca_ConnectReq_Out\n"));
				DisconnectLogicalLink (pLink, ERROR_CALL_NOT_IMPLEMENTED, FALSE);
				continue;
			}

			if (sp.u.CONNECTION_RESPONSE.result != 0) {
				int iErr = ERROR_INTERNAL_ERROR;
				if (sp.u.CONNECTION_RESPONSE.result == 2)
					iErr = ERROR_UNKNOWN_PORT;
				else if (sp.u.CONNECTION_RESPONSE.result == 3)
					iErr = ERROR_ACCESS_DENIED;
				else if (sp.u.CONNECTION_RESPONSE.result == 4)
					iErr = ERROR_REMOTE_SESSION_LIMIT_EXCEEDED;
				DisconnectLogicalLink (pLink, iErr, FALSE);
			} else {
				pLink->cid_remote = sp.u.CONNECTION_RESPONSE.dest_cid;
				pLink->eStage = CONFIG;
				ScheduleTimeout (pLink, gpL2CAP->dwConfigTO);
			}

			IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Going into l2ca_ConnectReq_Out callback\n"));
			pOwner->AddRef ();
			gpL2CAP->Unlock ();
			__try {
				pCallback (pUserContext, sp.u.CONNECTION_RESPONSE.source_cid, sp.u.CONNECTION_RESPONSE.result, sp.u.CONNECTION_RESPONSE.status);
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Exception while in l2ca_ConnectReq_Out\n"));
			}
			gpL2CAP->Lock ();
			pOwner->DelRef ();
			IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Came back from l2ca_ConnectReq_Out callback\n"));

			continue;
		}

		if (sp.h.code == L2CAP_COMMAND_CONNECTION_REQUEST) {
			IFDBG(DebugOut (DEBUG_L2CAP_PACKETS, L"Got connection request\n"));

			if (sp.h.length != 4) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Malformed connection request\n"));
				break;
			}

			L2CAP_CONTEXT *pOwner = FindContextByPSM (sp.u.CONNECTION_REQUEST.psm);
			if (! pOwner) {
				pOwner = gpL2CAP->pContexts;
				while (pOwner && (! pOwner->fDefaultServer))
					pOwner = pOwner->pNext;
			}

			LogLink *pLink = pOwner ? CreateNewLogLink (pPhys, pOwner) : NULL;
			CallContext *pCall = pLink ? AllocCallContext (CALL_CTX_INTERNAL, CALL_LOG_CONNECT_RESP, pLink, pOwner, NULL) : NULL;

			L2CA_ConnectInd pCallback = pCall ? pOwner->ei.l2ca_ConnectInd : NULL;
			
			if (! pCallback) {
				if (pLink) {
					PluckLogLink (pLink);
					delete pLink;
				}

				if (pCall)
					DeleteCallContext (pCall);

				Signal s;
				s.cid = 1;
				s.length = sizeof(s.packet.h) + sizeof(s.packet.u.CONNECTION_RESPONSE);
				s.packet.h.code = L2CAP_COMMAND_CONNECTION_RESPONSE;
				s.packet.h.id   = sp.h.id;
				s.packet.h.length = sizeof(s.packet.u.CONNECTION_RESPONSE);
				s.packet.u.CONNECTION_RESPONSE.source_cid = sp.u.CONNECTION_REQUEST.source_cid;
				s.packet.u.CONNECTION_RESPONSE.dest_cid = 0;
				s.packet.u.CONNECTION_RESPONSE.status = 0;
				s.packet.u.CONNECTION_RESPONSE.result = pCall ? 0x0004 : 0x0002;
				IFDBG(DebugOut (DEBUG_ERROR, L"Rejecting connection request : %s\n", pOwner ? L"PSM not supported" : L"OUT OF RESOURCES\n"));
				WriteDataDown (pPhys->h, NULL, SIGNAL_LENGTH(s), (unsigned char *)&s);

				continue;
			}

			UnscheduleTimeout (pPhys);

			pCall->id = sp.h.id;
			pCall->fForeignId = TRUE;

			pLink->cid = GetCID ();

			pLink->psm = sp.u.CONNECTION_REQUEST.psm;
			pLink->cid_remote = sp.u.CONNECTION_REQUEST.source_cid;
			pLink->eStage = STARTING_REQUEST;

			void *pUserContext = pOwner->pUserContext;
			BD_ADDR b = pPhys->b;
			unsigned short cid = pLink->cid;

			IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"In connection request : into the l2ca_ConnectInd!\n"));
			pOwner->AddRef ();
			gpL2CAP->Unlock ();
			__try {
				pCallback (pUserContext, &b, cid, sp.h.id, sp.u.CONNECTION_REQUEST.psm);
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"In connection request : exception in the l2ca_ConnectInd!\n"));
			}
			gpL2CAP->Lock ();
			pOwner->DelRef ();

			IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"In connection request : out of l2ca_ConnectInd!\n"));

			continue;
		}

		if (sp.h.code == L2CAP_COMMAND_CONFIG_REQUEST) {
			IFDBG(DebugOut (DEBUG_L2CAP_PACKETS, L"Got config request\n"));

			if (sp.h.length < 4) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Malformed config request\n"));
				break;
			}

			unsigned short	usOutMTU = 0;
			unsigned short	usInFlushTO = 0xffff;
			btFLOWSPEC		sInFlowSpec;
			btFLOWSPEC		*pfsInFlowSpec = NULL;

			Signal s;

			int fOptError = FALSE;
			s.cid = 1;
			s.length = sizeof(s.packet.h) + 6;
			s.packet.h.code = L2CAP_COMMAND_CONFIG_RESPONSE;
			s.packet.h.id = sp.h.id;
			s.packet.h.length = 6;
			s.packet.u.CONFIG_RESPONSE.flags = 0;
			s.packet.u.CONFIG_RESPONSE.source_cid = sp.u.CONFIG_REQUEST.dest_cid;
			s.packet.u.CONFIG_RESPONSE.result = 3;

			unsigned char *pBadOpt = s.packet.u.CONFIG_RESPONSE.optbuf;

			unsigned char *p = sp.u.CONFIG_REQUEST.optbuf;
			while (p - sp.u.CONFIG_REQUEST.optbuf < sp.h.length - 4) {
				unsigned char type = p[0] & 0x7f;
				unsigned char hint = p[0] & 0x80;
				unsigned char length = p[1];
				unsigned char length2 = 0;
				unsigned char *pParms = p + 2;
				p += 2 + length;
				if (p - sp.u.CONFIG_REQUEST.optbuf > sp.h.length - 4)
					break;

				switch (type) {
				case 1:
					usOutMTU = pParms[0] | (pParms[1] << 8);
					length2 = 2;
					if (usOutMTU < L2CAP_MTUMIN) {
						IFDBG(DebugOut (DEBUG_ERROR, L"L2CAP: Incorrect MTU=%d!\n", usOutMTU));
						fOptError = TRUE;
						if (pBadOpt - s.packet.u.CONFIG_RESPONSE.optbuf + length + 2 < sizeof (s.packet.u.CONFIG_RESPONSE.optbuf)) {
							fOptError = TRUE;
							memcpy (pBadOpt, p - length - 2, length + 2);
							s.length += length + 2;
							s.packet.h.length += length + 2;
							pBadOpt += length + 2;
						} else
							IFDBG(DebugOut (DEBUG_ERROR, L"Buffer space exhausted for CONFIG_RESPONSE\n"));
					}
					break;

				case 2:
					usInFlushTO = pParms[0] | (pParms[1] << 8);
					length2 = 2;
					break;

				case 3:
					length2 = 22;
					if (length != length2)
						break;

					pfsInFlowSpec = &sInFlowSpec;

					sInFlowSpec.flags = pParms[0];
					sInFlowSpec.service_type = pParms[1];
					sInFlowSpec.token_rate = pParms[2] | (pParms[3] << 8) | (pParms[4] << 16) | (pParms[5] << 24);
					sInFlowSpec.token_bucket_size = pParms[6] | (pParms[7] << 8) | (pParms[8] << 16) | (pParms[9] << 24);
					sInFlowSpec.peak_bandwidth = pParms[10] | (pParms[11] << 8) | (pParms[12] << 16) | (pParms[13] << 24);
					sInFlowSpec.latency = pParms[14] | (pParms[15] << 8) | (pParms[16] << 16) | (pParms[17] << 24);
					sInFlowSpec.delay_variation = pParms[18] | (pParms[19] << 8) | (pParms[20] << 16) | (pParms[21] << 24);
					break;

				default:
					length2 = length;
					if (hint)			// ignore the hint...
						break;

					IFDBG(DebugOut (DEBUG_ERROR, L"Unknown option!\n"));
					if (pBadOpt - s.packet.u.CONFIG_RESPONSE.optbuf + length + 2 < sizeof (s.packet.u.CONFIG_RESPONSE.optbuf)) {
						fOptError = TRUE;
						memcpy (pBadOpt, p - length - 2, length + 2);
						s.length += length + 2;
						s.packet.h.length += length + 2;
						pBadOpt += length + 2;
					} else
						IFDBG(DebugOut (DEBUG_ERROR, L"Buffer space exhausted for CONFIG_RESPONSE\n"));
					break;

				}

				if (length != length2)
					break;
			}

			if (p - sp.u.CONFIG_REQUEST.optbuf != sp.h.length - 4) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Malformed config option!\n"));
				break;
			}

			if (fOptError) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Option error - rejecting command\n"));
				WriteDataDown (pPhys->h, NULL, SIGNAL_LENGTH(s), (unsigned char *)&s);
				continue;
			}

			LogLink *pLog = pPhys->pLogLinks;
			while (pLog && (pLog->cid != sp.u.CONFIG_REQUEST.dest_cid))
				pLog = pLog->pNext;

			unsigned short response_cid = pLog ? pLog->cid_remote : 0;
			if (pLog && (pLog->eStage != CONFIG) && (pLog->eStage != CONFIG_LOCAL_DONE)) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Config request arrives out of sync: wrong connection stage\n"));
				pLog = NULL;
			}

			L2CAP_CONTEXT	*pOwner			= pLog ? pLog->pOwner : NULL;
			void			*pUserContext	= pOwner ? pOwner->pUserContext : NULL;
			L2CA_ConfigInd	pCallback		= pOwner ? pOwner->ei.l2ca_ConfigInd : NULL;

			CallContext		*pCall			= pCallback ? AllocCallContext (CALL_CTX_INTERNAL, CALL_LOG_CONFIG_RESP, pLog, pOwner, NULL) : NULL;

			if (! (pCall && pCallback)) {
				if (pCall)
					DeleteCallContext (pCall);

				Signal s;
				if (response_cid) {
					s.packet.h.code = L2CAP_COMMAND_CONFIG_RESPONSE;
					s.packet.h.id = sp.h.id;
					s.packet.h.length = 6;
					s.packet.u.CONFIG_RESPONSE.result = 2;
					s.packet.u.CONFIG_RESPONSE.flags = 0;
					s.packet.u.CONFIG_RESPONSE.source_cid = response_cid;
				} else {
					s.packet.h.code = L2CAP_COMMAND_COMMAND_REJECT;
					s.packet.h.id = sp.h.id;
					s.packet.h.length = 6;
					s.packet.u.COMMAND_REJECT.reason = 0x0002;		// Bad CID in request
					s.packet.u.COMMAND_REJECT.ucOptionalData[0] = sp.u.CONFIG_REQUEST.dest_cid & 0xff;
					s.packet.u.COMMAND_REJECT.ucOptionalData[1] = (sp.u.CONFIG_REQUEST.dest_cid >> 8) & 0xff;
					s.packet.u.COMMAND_REJECT.ucOptionalData[2] = 0;
					s.packet.u.COMMAND_REJECT.ucOptionalData[3] = 0;
				}

				s.cid = 1;
				s.length = sizeof(s.packet.h) + s.packet.h.length;

				IFDBG(DebugOut (DEBUG_ERROR, L"Config request rejected as unknown: %s\n", pLog ? L"no memory" : L"no link"));
				WriteDataDown (pPhys->h, NULL, SIGNAL_LENGTH (s), (unsigned char *)&s);

				continue;
			}

			unsigned short  cid = pLog->cid;

			pCall->id = sp.h.id;
			pCall->fForeignId = TRUE;

			IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Going into L2CA_ConfigInd\n"));
			pOwner->AddRef ();
			gpL2CAP->Unlock ();
			__try {
				pCallback (pUserContext, sp.h.id, cid, usOutMTU, usInFlushTO, pfsInFlowSpec, 0, NULL);
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Exception in L2CA_ConfigInd!!!\n"));
			}
			gpL2CAP->Lock ();
			pOwner->DelRef ();
			IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Came from L2CA_ConfigInd\n"));
			continue;
		}

		if (sp.h.code == L2CAP_COMMAND_CONFIG_RESPONSE) {
			IFDBG(DebugOut (DEBUG_L2CAP_PACKETS, L"Got config response\n"));

			if (sp.h.length < 6) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Malformed config response\n"));
				break;
			}

			unsigned short	usInMTU = 0;
			unsigned short	usOutFlushTO = 0xffff;
			btFLOWSPEC		fsOutFlowSpec;
			btFLOWSPEC		*pfsOutFlowSpec = NULL;

			unsigned char *p = sp.u.CONFIG_RESPONSE.optbuf;
			while (p - sp.u.CONFIG_RESPONSE.optbuf < sp.h.length - 6) {
				unsigned char type = p[0] & 0x7f;
				unsigned char hint = p[0] & 0x80;
				unsigned char length = p[1];
				unsigned char length2 = 0;
				unsigned char *pParms = p + 2;
				p += 2 + length;
				if (p - sp.u.CONFIG_RESPONSE.optbuf > sp.h.length - 6)
					break;

				switch (type) {
				case 1:
					usInMTU = pParms[0] | (pParms[1] << 8);
					length2 = 2;
					break;

				case 2:
					usOutFlushTO = pParms[0] | (pParms[1] << 8);
					length2 = 2;
					break;

				case 3:
					length2 = 22;
					if (length != length2)
						break;

					pfsOutFlowSpec = &fsOutFlowSpec;

					fsOutFlowSpec.flags = pParms[0];
					fsOutFlowSpec.service_type = pParms[1];
					fsOutFlowSpec.token_rate = pParms[2] | (pParms[3] << 8) | (pParms[4] << 16) | (pParms[5] << 24);
					fsOutFlowSpec.token_bucket_size = pParms[6] | (pParms[7] << 8) | (pParms[8] << 16) | (pParms[9] << 24);
					fsOutFlowSpec.peak_bandwidth = pParms[10] | (pParms[11] << 8) | (pParms[12] << 16) | (pParms[13] << 24);
					fsOutFlowSpec.latency = pParms[14] | (pParms[15] << 8) | (pParms[16] << 16) | (pParms[17] << 24);
					fsOutFlowSpec.delay_variation = pParms[18] | (pParms[19] << 8) | (pParms[20] << 16) | (pParms[21] << 24);
					break;

				default:
					IFDBG(DebugOut (DEBUG_ERROR, L"Ignoring unknown option %d\n", type));
				}

				if (length != length2)
					break;
			}

			if (p - sp.u.CONFIG_RESPONSE.optbuf != sp.h.length - 6) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Malformed config option!\n"));
				break;
			}

			LogLink *pLog = FindLog (sp.u.CONFIG_RESPONSE.source_cid, pPhys);
			if (pLog && (pLog->eStage != CONFIG) && (pLog->eStage != CONFIG_REMOTE_DONE)) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Config response arrives out of sync - wrong connection stage.\n"));
				pLog = NULL;
			}

			CallContext *pCall = pLog ? FindCall (pLog, CALL_LOG_CONFIG_REQ, sp.h.id) : NULL;
			L2CAP_CONTEXT *pOwner = pCall ? pCall->pOwner : NULL;
			L2CA_ConfigReq_Out pCallback = pOwner ? pOwner->c.l2ca_ConfigReq_Out : NULL;
			if (! pCallback) { // eat the response...
				if (pCall)
					AbortCall (pCall, ERROR_CALL_NOT_IMPLEMENTED);

				IFDBG(DebugOut (DEBUG_ERROR, L"No callback, or no connection\n"));
				continue;
			}

			void *pCallContext = pCall->pContext;
			DeleteCallContext (pCall);

			if (sp.u.CONFIG_RESPONSE.result == 0) {
				if (pLog->eStage == CONFIG)
					pLog->eStage = CONFIG_LOCAL_DONE;
				else {
					SVSUTIL_ASSERT (pLog->eStage == CONFIG_REMOTE_DONE);
					pLog->eStage = UP;
					UnscheduleTimeout (pLog);
				}
			}

			//	If we have more than one open 
			IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Going into L2CA_ConfigReq_Out\n"));
			pOwner->AddRef ();
			gpL2CAP->Unlock ();
			__try {
				pCallback (pCallContext, sp.u.CONFIG_RESPONSE.result, usInMTU, usOutFlushTO, pfsOutFlowSpec, 0, NULL);
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Exception in L2CA_ConfigReq_Out!!!\n"));
			}
			gpL2CAP->Lock ();
			pOwner->DelRef ();
			IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Came from L2CA_ConfigReq_Out\n"));
			continue;
		}

		if (sp.h.code == L2CAP_COMMAND_DISCONNECT_REQUEST) {
			IFDBG(DebugOut (DEBUG_L2CAP_PACKETS, L"Received disconnect request\n"));

			if (sp.h.length != 4) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Malformed L2CAP command!\n"));
				break;
			}

			LogLink *pLog = FindLog (sp.u.DISCONNECT_REQUEST.dest_cid, pPhys);
			if ((! pLog) || (pLog->cid_remote != sp.u.DISCONNECT_REQUEST.source_cid)) {
				Signal s;
				s.cid = 1;
				s.length = sizeof (sp.h) + 6;
				s.packet.h.code = L2CAP_COMMAND_COMMAND_REJECT;
				s.packet.h.id   = sp.h.id;
				s.packet.h.length = 6;
				s.packet.u.COMMAND_REJECT.reason = 2;
				s.packet.u.COMMAND_REJECT.ucOptionalData[0] = sp.u.DISCONNECT_REQUEST.dest_cid & 0xff;
				s.packet.u.COMMAND_REJECT.ucOptionalData[1] = (sp.u.DISCONNECT_REQUEST.dest_cid >> 8) & 0xff;
				s.packet.u.COMMAND_REJECT.ucOptionalData[2] = sp.u.DISCONNECT_REQUEST.source_cid & 0xff;
				s.packet.u.COMMAND_REJECT.ucOptionalData[3] = (sp.u.DISCONNECT_REQUEST.source_cid >> 8) & 0xff;
				WriteDataDown (pPhys->h, NULL, SIGNAL_LENGTH (s), (unsigned char *)&s);
				continue;
			}

			DisconnectLogicalLink (pLog, ERROR_GRACEFUL_DISCONNECT, FALSE);

			if ((gpL2CAP->eStage == Connected) && (pPhys == VerifyPhys (pPhys))) {
				Signal s;
				s.cid = 1;
				s.length = 8;
				s.packet.h.code = L2CAP_COMMAND_DISCONNECT_RESPONSE;
				s.packet.h.id = sp.h.id;
				s.packet.h.length = 4;
				s.packet.u.DISCONNECT_RESPONSE.dest_cid = sp.u.DISCONNECT_REQUEST.dest_cid;
				s.packet.u.DISCONNECT_RESPONSE.source_cid = sp.u.DISCONNECT_REQUEST.source_cid;
				WriteDataDown (pPhys->h, NULL, SIGNAL_LENGTH (s), (unsigned char *)&s);
			}

			continue;

		}

		if (sp.h.code == L2CAP_COMMAND_DISCONNECT_RESPONSE) {
			IFDBG(DebugOut (DEBUG_L2CAP_PACKETS, L"Received disconnect response\n"));

			if (sp.h.length != 4) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Malformed L2CAP command!\n"));
				break;
			}

			LogLink *pLog = FindLog (sp.u.DISCONNECT_RESPONSE.source_cid, pPhys);
			if ((! pLog) || (pLog->cid_remote != sp.u.DISCONNECT_RESPONSE.dest_cid)) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Disconnect request does not match - silently discarding!\n"));
				continue;
			}

			CallContext *pCall = FindCall (pLog, CALL_LOG_DISCONNECT_REQ, sp.h.id);
			if (pCall) {
				// Signal disconnection success
				L2CAP_CONTEXT *pOwner = pCall->pOwner;
				void *pCallContext = pCall->pContext;
				L2CA_Disconnect_Out pCallback = pOwner ? pOwner->c.l2ca_Disconnect_Out : NULL;
				DeleteCallContext (pCall);
				if (pCallback) {
					IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Going into l2ca_Disconnect_Out\n"));
					pOwner->AddRef ();
					gpL2CAP->Unlock ();

					__try {
						pCallback (pCallContext, 0);
					} __except (1) {
						IFDBG(DebugOut (DEBUG_ERROR, L"Exception in l2ca_Disconnect_Out!\n"));
					}

					gpL2CAP->Lock ();
					pOwner->DelRef ();

					IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Came from l2ca_Disconnect_Out\n"));

					if ((gpL2CAP->eStage != Connected) || (pLog != VerifyLog (pLog)))
						break;
				} else
					IFDBG(DebugOut (DEBUG_ERROR, L"No callback for disconnect out!\n"));
			}

			DisconnectLogicalLink (pLog, ERROR_GRACEFUL_DISCONNECT, FALSE);
			continue;
		}

		if (sp.h.code == L2CAP_COMMAND_INFO_REQUEST) {
			Signal s;
			s.cid = 1;
			s.length = sizeof(s.packet.h) + 4;
			s.packet.h.code = L2CAP_COMMAND_INFO_RESPONSE;
			s.packet.h.id = sp.h.id;
			s.packet.h.length = 4;
			s.packet.u.INFO_RESPONSE.info_type = sp.u.INFO_REQUEST.info_type;
			s.packet.u.INFO_RESPONSE.result = 1; // not supported
			WriteDataDown (pPhys->h, NULL, SIGNAL_LENGTH (s), (unsigned char *)&s);
			continue;
		}

		if (sp.h.code == L2CAP_COMMAND_INFO_REQUEST)
			continue;

		IFDBG(DebugOut (DEBUG_ERROR, L"Unsupported signalling command has arrived. Rejecting.\n"));

		Signal s;
		s.cid = 1;
		s.length = sizeof(s.packet.h);
		s.packet.h.code = L2CAP_COMMAND_COMMAND_REJECT;
		s.packet.h.id = sp.h.id;
		s.packet.h.length = 2;
		s.packet.u.COMMAND_REJECT.reason = 0;
		WriteDataDown (pPhys->h, NULL, SIGNAL_LENGTH (s), (unsigned char *)&s);
	}

	if (pBuffer->pFree)
		pBuffer->pFree (pBuffer);

	if ((cCurrentLength < length) && (gpL2CAP->eStage == Connected)) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_data_packet_up - malformed signalling packet\n"));
		RegisterTransmissionError (pPhys);
		gpL2CAP->Unlock ();
		return ERROR_INVALID_DATA;
	}

	gpL2CAP->Unlock ();
	return ERROR_SUCCESS;
}

static int hci_data_packet_down_out (void *pCallContext, int iError) {
	if (! pCallContext)
		return ERROR_SUCCESS;

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_data_packet_down_out\n"));

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_data_packet_down_out: ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpL2CAP->Lock ();

	if (gpL2CAP->eStage != Connected) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_data_packet_down_out: ERROR_SERVICE_NOT_ACTIVE\n"));
		gpL2CAP->Unlock ();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	CallContext *pCall = VerifyCall ((CallContext *)pCallContext);

	if ((! pCall) || ((pCall->eWhat != CALL_LOG_CONNECT_REQ) && (pCall->eWhat != CALL_LOG_CONNECT_RESP) &&
				(pCall->eWhat != CALL_LOG_CONFIG_REQ) && (pCall->eWhat != CALL_LOG_CONFIG_RESP) &&
				(pCall->eWhat != CALL_USERDATA) && (pCall->eWhat != CALL_LOG_DISCONNECT_REQ))) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_data_packet_down_out: no call or unexpected call type\n"));
		gpL2CAP->Unlock ();
		return ERROR_INTERNAL_ERROR;
	}


	if (iError != ERROR_SUCCESS) {
		if (pCall->u.pLogLink) {
			IFDBG(DebugOut (DEBUG_ERROR, L"Data transmission unsuccessful to %04x%08x\n", pCall->u.pLogLink->pPhysLink->b.NAP,
								pCall->u.pLogLink->pPhysLink->b.SAP));
			RegisterTransmissionError (pCall->u.pLogLink->pPhysLink);
			DisconnectLogicalLink (pCall->u.pLogLink, iError, TRUE);
		} else
			IFDBG(DebugOut (DEBUG_ERROR, L"Data transission error to cancelled l2cap link\n"));
	} else {
		int fDel = FALSE;

		int (*RESPONSE_CALLBACK) (void *pCallContext, unsigned short result) = NULL;
		void *pCallUserCtx = pCall->pContext;
		L2CAP_CONTEXT *pOwner = pCall->pOwner;

		switch (pCall->eWhat) {
		case CALL_LOG_CONFIG_REQ:		// Use master timeout for config
			break;

		case CALL_LOG_CONNECT_REQ:		// Turn on timeouts on calls. Need responses to actually signal user.
		case CALL_LOG_DISCONNECT_REQ:
			if (pCall->u.pLogLink)
				ScheduleTimeout (pCall->u.pLogLink, gpL2CAP->RTX);
			break;

		case CALL_LOG_CONFIG_RESP:	// signal OK to the user...
			RESPONSE_CALLBACK = pCall->pOwner->c.l2ca_ConfigResponse_Out;
			fDel = TRUE;
			break;

		case CALL_LOG_CONNECT_RESP:
			RESPONSE_CALLBACK = pCall->pOwner->c.l2ca_ConnectResponse_Out;
			fDel = TRUE;
			break;

		case CALL_USERDATA:
			RESPONSE_CALLBACK = pCall->pOwner->c.l2ca_DataDown_Out;
			fDel = TRUE;
			break;
		}

		if (fDel)	
			DeleteCallContext (pCall);

		if (RESPONSE_CALLBACK) {
			SVSUTIL_ASSERT (pOwner);

			pOwner->AddRef ();
			gpL2CAP->Unlock ();

			RESPONSE_CALLBACK (pCallUserCtx, 0);	// Success!

			gpL2CAP->Lock ();
			pOwner->DelRef ();
		}
	}
	
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_data_packet_down_out : ERROR_SUCCESS\n"));
	gpL2CAP->Unlock ();

	return ERROR_SUCCESS;
}

static int hci_call_aborted (void *pCallContext, int iError) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_call_aborted\n"));

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_call_aborted returns ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpL2CAP->Lock ();
	CallContext *pCall = VerifyCall ((CallContext *)pCallContext);
	if (! pCall) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_call_aborted returns ERROR_INVALID_PARAMETER (no context)\n"));
		gpL2CAP->Unlock ();

		return ERROR_INVALID_PARAMETER;
	}

	int iRes = CancelCall (pCall, ERROR_CANCELLED);

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"hci_call_aborted returns %d\n", iRes));
	gpL2CAP->Unlock ();
	return iRes;
}

static int hci_stack_event (void *pUserContext, int iEvent, void *pEventContext) {
	SVSUTIL_ASSERT (pUserContext == gpL2CAP);
	HCIEventContext *pEvent = (HCIEventContext *)pEventContext;

	switch (iEvent) {
	case BTH_STACK_NONE:
	case BTH_STACK_HOST_BUFFER:
		IFDBG(DebugOut (DEBUG_ERROR, L"Unexpected event - none or host buffer. Doing nothing\n"));
		break;

	case BTH_STACK_RESET:
		IFDBG(DebugOut (DEBUG_L2CAP_INIT, L"L2CAP : Stack reset\n"));
		btutil_ScheduleEvent (StackReset, NULL);
		break;

	case BTH_STACK_DOWN:
		IFDBG(DebugOut (DEBUG_L2CAP_INIT, L"L2CAP : Stack down\n"));
		btutil_ScheduleEvent (StackDown, NULL);
		break;

	case BTH_STACK_UP:
		IFDBG(DebugOut (DEBUG_L2CAP_INIT, L"L2CAP : Stack up\n"));
		btutil_ScheduleEvent (StackUp, NULL);
		break;

	case BTH_STACK_DISCONNECT:
		IFDBG(DebugOut (DEBUG_L2CAP_INIT, L"L2CAP : Stack disconnect\n"));
		btutil_ScheduleEvent (StackDisconnect, NULL);
		break;

	case BTH_STACK_FLOW_ON:
		IFDBG(DebugOut (DEBUG_L2CAP_INIT, L"L2CAP: Somebody turned flow on. Not supported. Disconnecting\n"));
		btutil_ScheduleEvent (StackDown, NULL);
		break;

	case BTH_STACK_FLOW_OFF:
		IFDBG(DebugOut (DEBUG_L2CAP_INIT, L"L2CAP : HCI Flow back off. Turning stack back on.\n"));
		btutil_ScheduleEvent (StackUp, NULL);
		break;

	default:
		IFDBG(DebugOut (DEBUG_ERROR, L"L2CAP: unknown stack event. Disconnecting out of paranoia.\n"));
		btutil_ScheduleEvent (StackDown, NULL);
	}

	return ERROR_SUCCESS;
}

static int l2ca_connect_req_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
unsigned short	psm,
BD_ADDR			*pba
) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"l2ca_connect_req_in\n"));

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_connect_req_in returns ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpL2CAP->Lock ();
	L2CAP_CONTEXT *pContext = VerifyContext ((L2CAP_CONTEXT *)hDeviceContext);
	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_connect_req_in returns ERROR_INVALID_PARAMETER (no context)\n"));
		gpL2CAP->Unlock ();

		return ERROR_INVALID_PARAMETER;
	}

	PhysLink *pPhysLink = NULL;

	int iRes = MakePhysicalLink (pContext, pba, &pPhysLink);

	if ((iRes == ERROR_SUCCESS) && pPhysLink)
		iRes = MakeLogicalLink (pContext, pCallContext, pPhysLink, psm);

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"l2ca_connect_req_in returns %d\n", iRes));
	gpL2CAP->Unlock ();
	return iRes;
}

static int l2ca_connect_response_in (HANDLE hDeviceContext, void *pCallContext, BD_ADDR *pba, unsigned char id, unsigned short cid, unsigned short response, unsigned short status) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"l2ca_connect_response_in\n"));

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_connect_response_in returns ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpL2CAP->Lock ();
	L2CAP_CONTEXT *pContext = VerifyContext ((L2CAP_CONTEXT *)hDeviceContext);
	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_connect_response_in returns ERROR_INVALID_PARAMETER (no context)\n"));
		gpL2CAP->Unlock ();

		return ERROR_INVALID_PARAMETER;
	}

	PhysLink *pPhys = FindPhys (pba);
	LogLink *pLog = pPhys ? FindLog (cid, pPhys) : NULL;
	CallContext *pCall = pLog && (pLog->eStage == STARTING_REQUEST) ? FindCall (pLog, CALL_LOG_CONNECT_RESP, id) : NULL;

	if (! pCall) {
		IFDBG(DebugOut (DEBUG_WARN, L"Requested connection request not found!\n"));
		gpL2CAP->Unlock ();
		return ERROR_NOT_FOUND;
	}

	pCall->eType = CALL_CTX_CALLOWNER;
	pCall->pOwner = pContext;
	pCall->pContext = pCallContext;
	unsigned short h = pPhys->h;

	SVSUTIL_ASSERT (id == pCall->id);

	unsigned short source_cid = 0;

	if (response == 0) {
		source_cid = pLog->cid_remote;
		pLog->eStage = CONFIG;
		ScheduleTimeout (pLog, gpL2CAP->dwConfigTO);
	} else if (response != 1) {
		pCall->u.pLogLink = NULL;		// Or else it will find it...
		DisconnectLogicalLink (pLog, ERROR_SUCCESS, FALSE);
		if (gpL2CAP->eStage != Connected) {
			IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"l2ca_connect_response_in : shutdown in progress\n"));
			gpL2CAP->Unlock ();
			return ERROR_SERVICE_NOT_ACTIVE;
		}
	}

	Signal s;

	s.cid             = 1;
	s.length          = sizeof (s.packet.h) + sizeof(s.packet.u.CONNECTION_RESPONSE);
	s.packet.h.code   = L2CAP_COMMAND_CONNECTION_RESPONSE;
	s.packet.h.id     = id;
	s.packet.h.length = sizeof (s.packet.u.CONNECTION_RESPONSE);
	s.packet.u.CONNECTION_RESPONSE.dest_cid = (response == 0) ? cid : 0;
	s.packet.u.CONNECTION_RESPONSE.source_cid = source_cid;
	s.packet.u.CONNECTION_RESPONSE.result = response;
	s.packet.u.CONNECTION_RESPONSE.status = status;

	int iRes = WriteDataDown (h, pCall, SIGNAL_LENGTH(s), (unsigned char *)&s);

	if (iRes != ERROR_SUCCESS)
		DeleteCallContext (pCall);

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"l2ca_connect_response_in returns %d\n", iRes));
	gpL2CAP->Unlock ();
	return iRes;
}

static int l2ca_config_req_in (HANDLE hDeviceContext, void *pCallContext, unsigned short cid, unsigned short usInMTU, unsigned short usOutFlushTO, struct btFLOWSPEC *pOutFlow, int cOptNum, struct btCONFIGEXTENSION **ppExtendedOptions) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"l2ca_config_req_in\n"));

	if (cOptNum || ppExtendedOptions) {
		IFDBG(DebugOut (DEBUG_ERROR, L"L2CAP CONFIX extended options not supported - l2ca_config_req_in returns ERROR_INVALID_PARAMETER\n"));
		return ERROR_INVALID_PARAMETER;
	}

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_config_req_in returns ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpL2CAP->Lock ();
	L2CAP_CONTEXT *pContext = VerifyContext ((L2CAP_CONTEXT *)hDeviceContext);
	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_config_req_in returns ERROR_INVALID_PARAMETER (no context)\n"));
		gpL2CAP->Unlock ();

		return ERROR_INVALID_PARAMETER;
	}

	LogLink *pLog = FindLog (cid);
	CallContext *pCall = pLog && ((pLog->eStage == CONFIG) || (pLog->eStage == CONFIG_REMOTE_DONE)) ? 
					AllocCallContext (CALL_CTX_CALLOWNER, CALL_LOG_CONFIG_REQ, pLog, pContext, pCallContext): NULL;

	int iRes = ERROR_INTERNAL_ERROR;

	if (! pCall) {
		if (! pLog) {
			iRes = ERROR_NOT_FOUND;
			IFDBG(DebugOut (DEBUG_WARN, L"Requested connection not found!\n"));
		} else {
			iRes = ERROR_NOT_FOUND;
			IFDBG(DebugOut (DEBUG_WARN, L"ERROR_OUTOFMEMORY!\n"));
		}

		gpL2CAP->Unlock ();
		return iRes;
	}

	unsigned short source_cid = 0;

	Signal s;

	s.packet.h.code   = L2CAP_COMMAND_CONFIG_REQUEST;
	s.packet.h.id     = pCall->id;
	s.packet.h.length = 4;

	s.packet.u.CONFIG_REQUEST.dest_cid = pLog->cid_remote;
	s.packet.u.CONFIG_REQUEST.flags = 0;

	unsigned char *p = s.packet.u.CONFIG_REQUEST.optbuf;
	if (usInMTU != 0) {
		*p++ = 1;
		*p++ = 2;
		*p++ = usInMTU & 0xff;
		*p++ = (usInMTU >> 8) & 0xff;
		s.packet.h.length += 4;
	}

	if (usOutFlushTO != 0xffff) {
		*p++ = 2;
		*p++ = 2;
		*p++ = usOutFlushTO & 0xff;
		*p++ = (usOutFlushTO >> 8) & 0xff;
		s.packet.h.length += 4;
	}

	if (pOutFlow) {
		*p++ = 3;
		*p++ = 22;
		*p++ = pOutFlow->flags;
		*p++ = pOutFlow->service_type;
		*p++ = pOutFlow->token_rate & 0xff;
		*p++ = (pOutFlow->token_rate >> 8) & 0xff;
		*p++ = (pOutFlow->token_rate >> 16) & 0xff;
		*p++ = (pOutFlow->token_rate >> 24) & 0xff;
		*p++ = pOutFlow->token_bucket_size & 0xff;
		*p++ = (pOutFlow->token_bucket_size >> 8) & 0xff;
		*p++ = (pOutFlow->token_bucket_size >> 16) & 0xff;
		*p++ = (pOutFlow->token_bucket_size >> 24) & 0xff;
		*p++ = pOutFlow->peak_bandwidth & 0xff;
		*p++ = (pOutFlow->peak_bandwidth >> 8) & 0xff;
		*p++ = (pOutFlow->peak_bandwidth >> 16) & 0xff;
		*p++ = (pOutFlow->peak_bandwidth >> 24) & 0xff;
		*p++ = pOutFlow->latency & 0xff;
		*p++ = (pOutFlow->latency >> 8) & 0xff;
		*p++ = (pOutFlow->latency >> 16) & 0xff;
		*p++ = (pOutFlow->latency >> 24) & 0xff;
		*p++ = pOutFlow->delay_variation & 0xff;
		*p++ = (pOutFlow->delay_variation >> 8) & 0xff;
		*p++ = (pOutFlow->delay_variation >> 16) & 0xff;
		*p++ = (pOutFlow->delay_variation >> 24) & 0xff;

		s.packet.h.length += 24;
	}

	s.cid             = 1;
	s.length          = sizeof (s.packet.h) + s.packet.h.length;

	iRes = WriteDataDown (pLog->pPhysLink->h, pCall, SIGNAL_LENGTH(s), (unsigned char *)&s);

	if (iRes != ERROR_SUCCESS)
		DeleteCallContext (pCall);

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"l2ca_config_req_in returns %d\n", iRes));
	gpL2CAP->Unlock ();
	return iRes;
}

static int l2ca_config_response_in (HANDLE hDeviceContext, void *pCallContext, unsigned char id, unsigned short cid, unsigned short result, unsigned short usOutMTU, unsigned short usInFlushTO, struct btFLOWSPEC *pInFlow, int cOptNum, struct btCONFIGEXTENSION **ppExtendedOptions) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"l2ca_config_response_in\n"));

	if (cOptNum || ppExtendedOptions) {
		IFDBG(DebugOut (DEBUG_ERROR, L"L2CAP CONFIX extended options not supported - l2ca_config_response_in returns ERROR_INVALID_PARAMETER\n"));
		return ERROR_INVALID_PARAMETER;
	}

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_config_response_in returns ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpL2CAP->Lock ();
	L2CAP_CONTEXT *pContext = VerifyContext ((L2CAP_CONTEXT *)hDeviceContext);
	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_config_response_in returns ERROR_INVALID_PARAMETER (no context)\n"));
		gpL2CAP->Unlock ();

		return ERROR_INVALID_PARAMETER;
	}

	LogLink *pLog = FindLog (cid);
	CallContext *pCall = pLog && ((pLog->eStage == CONFIG) || (pLog->eStage == CONFIG_LOCAL_DONE)) ?
						FindCall (pLog, CALL_LOG_CONFIG_RESP, id) : NULL;
	if (! pCall) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_config_response_in returns ERROR_NOT_FOUND (no connection)\n"));
		gpL2CAP->Unlock ();

		return ERROR_NOT_FOUND;
	}

	pCall->pContext = pCallContext;

	Signal s;

	s.packet.h.code   = L2CAP_COMMAND_CONFIG_RESPONSE;
	s.packet.h.id     = pCall->id;
	s.packet.h.length = 6;

	s.packet.u.CONFIG_RESPONSE.source_cid = pLog->cid_remote;
	s.packet.u.CONFIG_RESPONSE.result     = result;
	s.packet.u.CONFIG_RESPONSE.flags      = 0;
	if (result == 1) {
		unsigned char *p = s.packet.u.CONFIG_RESPONSE.optbuf;
		if (usOutMTU != 0) {
			*p++ = 1;
			*p++ = 2;
			*p++ = usOutMTU & 0xff;
			*p++ = (usOutMTU >> 8) & 0xff;
			s.packet.h.length += 4;
		}

		if (usInFlushTO != 0) {
			*p++ = 2;
			*p++ = 2;
			*p++ = usInFlushTO & 0xff;
			*p++ = (usInFlushTO >> 8) & 0xff;
			s.packet.h.length += 4;
		}

		if (pInFlow) {
			*p++ = 3;
			*p++ = 22;
			*p++ = pInFlow->flags;
			*p++ = pInFlow->service_type;
			*p++ = pInFlow->token_rate & 0xff;
			*p++ = (pInFlow->token_rate >> 8) & 0xff;
			*p++ = (pInFlow->token_rate >> 16) & 0xff;
			*p++ = (pInFlow->token_rate >> 24) & 0xff;
			*p++ = pInFlow->token_bucket_size & 0xff;
			*p++ = (pInFlow->token_bucket_size >> 8) & 0xff;
			*p++ = (pInFlow->token_bucket_size >> 16) & 0xff;
			*p++ = (pInFlow->token_bucket_size >> 24) & 0xff;
			*p++ = pInFlow->peak_bandwidth & 0xff;
			*p++ = (pInFlow->peak_bandwidth >> 8) & 0xff;
			*p++ = (pInFlow->peak_bandwidth >> 16) & 0xff;
			*p++ = (pInFlow->peak_bandwidth >> 24) & 0xff;
			*p++ = pInFlow->latency & 0xff;
			*p++ = (pInFlow->latency >> 8) & 0xff;
			*p++ = (pInFlow->latency >> 16) & 0xff;
			*p++ = (pInFlow->latency >> 24) & 0xff;
			*p++ = pInFlow->delay_variation & 0xff;
			*p++ = (pInFlow->delay_variation >> 8) & 0xff;
			*p++ = (pInFlow->delay_variation >> 16) & 0xff;
			*p++ = (pInFlow->delay_variation >> 24) & 0xff;

			s.packet.h.length += 24;
		}
	} else if (result == 0) {
		if (pLog->eStage == CONFIG)
			pLog->eStage = CONFIG_REMOTE_DONE;
		else if (pLog->eStage == CONFIG_LOCAL_DONE) {
			UnscheduleTimeout (pLog);
			pLog->eStage = UP;
		} else
			SVSUTIL_ASSERT (0);
	}

	s.cid             = 1;
	s.length          = sizeof (s.packet.h) + s.packet.h.length;

	int iRes = WriteDataDown (pLog->pPhysLink->h, pCall, SIGNAL_LENGTH(s), (unsigned char *)&s);

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"l2ca_config_response_in returns %d\n", iRes));
	gpL2CAP->Unlock ();
	return iRes;
}

static int l2ca_ping_in (HANDLE hDeviceContext, void *pCallContext, BD_ADDR *pba, unsigned char *pInBuffer, unsigned short length) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"l2ca_ping_in\n"));

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_ping_in returns ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpL2CAP->Lock ();
	L2CAP_CONTEXT *pContext = VerifyContext ((L2CAP_CONTEXT *)hDeviceContext);
	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_ping_in returns ERROR_INVALID_PARAMETER (no context)\n"));
		gpL2CAP->Unlock ();

		return ERROR_INVALID_PARAMETER;
	}

	PhysLink *pPhys = NULL;
	int iRes = MakePhysicalLink (pContext, pba, &pPhys);

	if (iRes != ERROR_SUCCESS) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_ping_in : Can't make physlink to device, error %d\n", iRes));
		gpL2CAP->Unlock ();

		return iRes;
	}

	CallContext *pCall = AllocCallContext (CALL_CTX_CALLOWNER, CALL_PHYS_PING, pPhys, pContext, pCallContext);

	if (! pCall) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_ping_in returns ERROR_OUTOFMEMORY)\n"));
		gpL2CAP->Unlock ();

		return ERROR_OUTOFMEMORY;
	}

	Signal s;
	s.packet.h.code   = L2CAP_COMMAND_ECHO_REQUEST;
	s.packet.h.id     = pCall->id;
	s.packet.h.length = length > sizeof(s.packet.u.ECHO_REQUEST.ucData) ? sizeof(s.packet.u.ECHO_REQUEST.ucData) : length;

	memcpy (s.packet.u.ECHO_REQUEST.ucData, pInBuffer, s.packet.h.length);

	s.cid = 1;
	s.length = s.packet.h.length + 4;

	if (pPhys->eStage == UP) {// Just shove it in...
		ScheduleTimeout (pCall, gpL2CAP->RTX);

		iRes = WriteDataDown (pPhys->h, pCall, SIGNAL_LENGTH(s), (unsigned char *)&s);
	} else {
		pCall->pData = g_funcAlloc (SIGNAL_LENGTH(s), g_pvAllocData);
		if (pCall->pData) {
			pCall->cData = SIGNAL_LENGTH(s);
			memcpy (pCall->pData, &s, pCall->cData);
		} else 
			iRes = ERROR_OUTOFMEMORY;
	}

	if (iRes != ERROR_SUCCESS)
		DeleteCallContext (pCall);

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"l2ca_ping_in returns %d\n", iRes));
	gpL2CAP->Unlock ();
	return iRes;
}

static int l2ca_data_down_in (HANDLE hDeviceContext, void *pCallContext, unsigned short cid, BD_BUFFER *pBuffer) {
	if ((! pBuffer) || (pBuffer->cSize < pBuffer->cEnd - pBuffer->cStart) || (! pBuffer->pBuffer)) {
		IFDBG(DebugOut (DEBUG_WARN, L"l2ca_data_down_in returns ERROR_INVALID_PARAMETER\n"));
		return ERROR_INVALID_PARAMETER;
	}

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"l2ca_data_down_in\n"));

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_data_down_in returns ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpL2CAP->Lock ();

	L2CAP_CONTEXT *pContext = ((pBuffer->cStart < 4 + gpL2CAP->cHCIHeader) ||
			(pBuffer->cSize - pBuffer->cEnd < gpL2CAP->cHCITrailer)) ? NULL : VerifyContext ((L2CAP_CONTEXT *)hDeviceContext);

	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_data_down_in returns ERROR_INVALID_PARAMETER (no context or space for headers in buffer)\n"));
		gpL2CAP->Unlock ();

		return ERROR_INVALID_PARAMETER;
	}

	int iRes;

	LogLink *pLog = FindLog (cid);
	CallContext *pCall = (pLog && (pLog->eStage == UP)) ? AllocCallContext (CALL_CTX_CALLOWNER, CALL_USERDATA, pLog, pContext, pCallContext) : NULL;
	if (! pCall) {
		if (pLog && (pLog->eStage == UP))
			iRes = ERROR_OUTOFMEMORY;
		else
			iRes = ERROR_NOT_FOUND;

		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_data_down_in returns %d\n", iRes));
		gpL2CAP->Unlock ();

		return iRes;
	}

	//	Put in L2CAP header
	int cSize = BufferTotal (pBuffer);
	pBuffer->cStart -= 4;
	
	SVSUTIL_ASSERT (pBuffer->cStart >= gpL2CAP->cHCIHeader);

	unsigned char *p = pBuffer->pBuffer + pBuffer->cStart;
	p[0] = cSize & 0xff;
	p[1] = (cSize >> 8) & 0xff;
	p[2] = pLog->cid_remote & 0xff;
	p[3] = (pLog->cid_remote >> 8) & 0xff;

	iRes = ERROR_INTERNAL_ERROR;
	HCI_DataPacketDown_In pCallback = gpL2CAP->hci_if.hci_DataPacketDown_In;
	HANDLE hHCI = gpL2CAP->hHCI;

	if (hHCI) {
		IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Going into hci_DataPacketDown_In\n"));
		unsigned short h = pLog->pPhysLink->h;
		gpL2CAP->AddRef ();
		gpL2CAP->Unlock ();
		__try {
			iRes = pCallback (gpL2CAP->hHCI, pCall, h, pBuffer);
		} __except (1) {
			IFDBG(DebugOut (DEBUG_ERROR, L"Exception in hci_DataPacketDown_In\n"));
		}

		gpL2CAP->Lock ();
		gpL2CAP->DelRef ();
		IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Came from hci_DataPacketDown_In\n"));
	} else
		IFDBG(DebugOut (DEBUG_ERROR, L"HCI disconnected!\n"));

	if (iRes != ERROR_SUCCESS)
		DeleteCallContext (pCall);

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"l2ca_data_down_in returns %d\n", iRes));
	gpL2CAP->Unlock ();
	return iRes;
}

static int l2ca_disconnect_in (HANDLE hDeviceContext, void *pCallContext, unsigned short cid) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"l2ca_disconnect_in (0x%04x)\n", cid));

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_disconnect_in returns ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpL2CAP->Lock ();
	L2CAP_CONTEXT *pContext = VerifyContext ((L2CAP_CONTEXT *)hDeviceContext);
	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_disconnect_in returns ERROR_INVALID_PARAMETER (no context)\n"));
		gpL2CAP->Unlock ();

		return ERROR_INVALID_PARAMETER;
	}

	int iRes;

	LogLink *pLog = FindLog (cid);
	CallContext *pCall = (pLog && (pLog->eStage >= CONFIG) && (pLog->eStage <= UP)) ? AllocCallContext (CALL_CTX_CALLOWNER, CALL_LOG_DISCONNECT_REQ, pLog, pContext, pCallContext) : NULL;
	if (! pCall) {
		if (pLog && (pLog->eStage == UP))
			iRes = ERROR_OUTOFMEMORY;
		else
			iRes = ERROR_NOT_FOUND;

		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_disconnect_in returns %d\n", iRes));
		gpL2CAP->Unlock ();

		return iRes;
	}

	ScheduleTimeout (pCall, gpL2CAP->RTX);

	Signal s;
	s.cid = 1;
	s.length = sizeof(s.packet.h) + sizeof(s.packet.u.DISCONNECT_REQUEST);
	s.packet.h.code = L2CAP_COMMAND_DISCONNECT_REQUEST;
	s.packet.h.id = pCall->id;
	s.packet.h.length = sizeof(s.packet.u.DISCONNECT_REQUEST);
	s.packet.u.DISCONNECT_REQUEST.dest_cid = pLog->cid_remote;
	s.packet.u.DISCONNECT_REQUEST.source_cid = pLog->cid;

	iRes = WriteDataDown (pLog->pPhysLink->h, pCall, SIGNAL_LENGTH(s), (unsigned char *)&s);

	if (iRes != ERROR_SUCCESS)
		DeleteCallContext (pCall);

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"l2ca_data_down_in returns %d\n", iRes));
	gpL2CAP->Unlock ();
	return iRes;
}

static int l2ca_abort_call (HANDLE hDeviceContext, void *pCallContext) {
	return hci_call_aborted (FindCall (pCallContext), ERROR_CANCELLED);
}

static int check_io_control_parms
(
int cInBuffer,
char *pInBuffer,
int cOutBuffer,
char *pOutBuffer,
int *pcDataReturned,
char *pSpace,
int cSpace
) {
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


static int l2ca_ioctl
(
HANDLE	hDeviceContext,
int		fSelector,
int		cInBuffer,
char	*pInBuffer,
int		cOutBuffer,
char	*pOutBuffer,
int		*pcDataReturned
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"l2ca_ioctl [0x%08x code 0x%08x]\n"));

	char c;
	if (! check_io_control_parms (cInBuffer, pInBuffer, cOutBuffer, pOutBuffer, pcDataReturned, &c, 1)) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_ioctl returns ERROR_INVALID_PARAMETER (exception)\n"));
		return ERROR_INVALID_PARAMETER;
	}

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_ioctl returns ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}


	gpL2CAP->Lock ();

	if (! gpL2CAP->IsStackRunning ()) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_ioctl returns ERROR_SERVICE_NOT_ACTIVE\n"));
		gpL2CAP->Unlock ();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	L2CAP_CONTEXT *pContext = VerifyContext ((L2CAP_CONTEXT *)hDeviceContext);
	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"l2ca_ioctl returns ERROR_INVALID_HANDLE\n"));
		gpL2CAP->Unlock ();
		return ERROR_INVALID_HANDLE;
	}

	int iRes = ERROR_INVALID_OPERATION;

	switch (fSelector) {
	case BTH_L2CAP_IOCTL_SET_PACKET_TYPE:
		{
			if ((cInBuffer != sizeof(unsigned short)) || pOutBuffer || (cOutBuffer != 0)) {
				IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] BTH_STACK_IOCTL_SET_PACKET_TYPE incorrect data size : l2ca_ioctl returns ERROR_INVALID_PARAMETER\n"));
				iRes = ERROR_INVALID_PARAMETER;
				break;
			}
			pContext->usPacketType = pInBuffer[0] | (pInBuffer[1] << 8);
			iRes = ERROR_SUCCESS;
		}
		break;

	case BTH_STACK_IOCTL_FREE_PORT:
		{
			if ((cInBuffer != sizeof(unsigned short)) || pOutBuffer || (cOutBuffer != 0)) {
				IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] BTH_STACK_IOCTL_FREE_PORT incorrect data size : l2ca_ioctl returns ERROR_INVALID_PARAMETER\n"));
				iRes = ERROR_INVALID_PARAMETER;
				break;
			}
			unsigned short psm = pInBuffer[0] | (pInBuffer[1] << 8);
			if ((psm & 1) == 0) {
				IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] illegal : l2ca_ioctl returns ERROR_INVALID_PARAMETER\n"));
				iRes = ERROR_INVALID_PARAMETER;
				break;
			}

			PSMContext *pPSMCtx = pContext->pReservedPorts;
			PSMContext *pParent = NULL;
			while (pPSMCtx && (pPSMCtx->usPSM != psm)) {
				pParent = pPSMCtx;
				pPSMCtx = pPSMCtx->pNext;
			}

			if (pPSMCtx) {
				if (pParent)
					pParent->pNext = pPSMCtx->pNext;
				else
					pContext->pReservedPorts = pPSMCtx->pNext;
				delete pPSMCtx;
				IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"[L2CAP] PSM 0x%04x successfully freed for context 0x%08x\n", psm, pContext));
				iRes = ERROR_SUCCESS;
			} else {
				IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] PSM not found : l2ca_ioctl returns ERROR_NOT_FOUND\n"));
				iRes = ERROR_NOT_FOUND;
			}
		}
		break;

	case BTH_STACK_IOCTL_RESERVE_PORT:
		{
			if ((cInBuffer != sizeof(unsigned short)) || (pOutBuffer && (cOutBuffer != sizeof(unsigned short)))) {
				IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] BTH_STACK_IOCTL_RESERVE_PORT incorrect data size : l2ca_ioctl returns ERROR_INVALID_PARAMETER\n"));
				iRes = ERROR_INVALID_PARAMETER;
				break;
			}
			unsigned short psm = pInBuffer[0] | (pInBuffer[1] << 8);
			if (psm == 0) {	// Allocate one
				SVSUTIL_ASSERT (gpL2CAP->usCurrentPSM & 1);
				psm = gpL2CAP->usCurrentPSM;
				do {
					psm += 2;
					if (psm < L2CAP_FIRST_PSM)
						psm = L2CAP_FIRST_PSM;
				} while (FindContextByPSM(psm));

				gpL2CAP->usCurrentPSM = psm;
			} else if (((psm & 1) == 0) || FindContextByPSM (psm)) {
				IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] illegal PSM or PSM already reserved : l2ca_ioctl returns ERROR_INVALID_PARAMETER\n"));
				iRes = ERROR_INVALID_PARAMETER;
				break;
			}

			PSMContext *pPSMCtx = new PSMContext;
			if (pPSMCtx) {
				pPSMCtx->usPSM = psm;
				pPSMCtx->pNext = pContext->pReservedPorts;
				pContext->pReservedPorts = pPSMCtx;
				if (pOutBuffer) {
					pOutBuffer[0] = psm & 0xff;
					pOutBuffer[1] = psm >> 8;
					*pcDataReturned = sizeof(psm);
				}
				IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"[L2CAP] PSM 0x%04x successfully reserved for context 0x%08x\n", psm, pContext));
				iRes = ERROR_SUCCESS;
			} else {
				IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] l2ca_ioctl returns ERROR_OUTOFMEMORY\n"));
				iRes = ERROR_OUTOFMEMORY;
			}
		}
		break;

	case BTH_STACK_IOCTL_GET_CONNECTED:
		if ((cInBuffer == 0) && (cOutBuffer == 4)) {
			iRes = ERROR_SUCCESS;

			int iCount = gpL2CAP->eStage == Connected;

			pOutBuffer[0] = iCount & 0xff;
			pOutBuffer[1] = (iCount >> 8) & 0xff;
			pOutBuffer[2] = (iCount >> 16) & 0xff;
			pOutBuffer[3] = (iCount >> 24) & 0xff;
			*pcDataReturned = 4;
		} else
			iRes = ERROR_INVALID_PARAMETER;
		break;

	case BTH_L2CAP_IOCTL_LOCK_BASEBAND:
	case BTH_L2CAP_IOCTL_UNLOCK_BASEBAND:
		if ((cInBuffer == sizeof(BD_ADDR)) && (cOutBuffer == 0)) {
			*pcDataReturned = 0;
			BD_ADDR b;
			memcpy (&b, pInBuffer, sizeof(b));

			PhysLink *pLink = FindPhys (&b);
			if (pLink) {
				if (fSelector == BTH_L2CAP_IOCTL_LOCK_BASEBAND)
					pLink->iLockCnt++;
				else if (pLink->iLockCnt > 0) {
					pLink->iLockCnt--;
					if ((! pLink->pLogLinks) && (pLink->iLockCnt == 0))
						ScheduleTimeout (pLink, gpL2CAP->dwPhysIdle);
				}
				iRes = ERROR_SUCCESS;
			} else
				iRes = ERROR_NOT_FOUND;
		} else
			iRes = ERROR_INVALID_PARAMETER;
		break;

	case BTH_L2CAP_IOCTL_DROP_IDLE:
		if ((cInBuffer == sizeof(void *)) && (cOutBuffer == sizeof(int))) {
			*pcDataReturned = sizeof(int);
			memset (pOutBuffer, 0, sizeof(int));

			void *pCallContext;
			memcpy (&pCallContext, pInBuffer, sizeof(void *));

			iRes = ERROR_SUCCESS;

			if (! gpL2CAP->fPicoCapable) {
				//
				//	Note, this might look funny, but if there's a single existing "live" link - better don't mess with it
				//	and just go through with the connection.
				//
				PhysLink *pPhys = gpL2CAP->pPhysLinks;
				while (pPhys && (pPhys->eStage != UP))
					pPhys = pPhys->pNext;

				if (pPhys && (pPhys->iLockCnt <= 0) && (! pPhys->pLogLinks) && (! FindCall (pPhys))) {	// Timeouting link...
					IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"BTH_STACK_IOCTL_DROP_UNUSED :: disconnecting idle connection (to %04x%08x) first\n", pPhys->b.NAP, pPhys->b.SAP));
					CallContext *pCall = AllocCallContext (CALL_CTX_CALLOWNER, CALL_PHYS_DROP_IDLE, NULL, pContext, pCallContext);
					if (pCall) {
						iRes = DisconnectPhysicalLink (pPhys, FALSE, ERROR_SUCCESS, pCall);
						if (iRes != ERROR_SUCCESS)
							DeleteCallContext (pCall);
						else {
							int c = TRUE;
							memcpy (pOutBuffer, &c, sizeof(c));
						}
					} else
						iRes = ERROR_OUTOFMEMORY;
				} else
					IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"BTH_STACK_IOCTL_DROP_UNUSED :: Idle connection not found!\n"));
			}
		} else
			iRes = ERROR_INVALID_PARAMETER;
		break;
	}

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"l2ca_ioctl exits with code 0x%08x\n", iRes));

	gpL2CAP->Unlock ();

	return iRes;
}


//
//	Public APIs
//
int l2cap_InitializeOnce (void) {
	IFDBG(DebugOut (DEBUG_L2CAP_INIT, L"L2CAP init:: entered\n"));

	SVSUTIL_ASSERT (! gpL2CAP);

	if (gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"L2CAP init:: ERROR_ALREADY_EXISTS\n"));
		return ERROR_ALREADY_EXISTS;
	}

	gpL2CAP = new L2CAP;

	if (gpL2CAP) {
		IFDBG(DebugOut (DEBUG_L2CAP_INIT, L"L2CAP init:: ERROR_SUCCESS\n"));
		return ERROR_SUCCESS;
	}

	IFDBG(DebugOut (DEBUG_ERROR, L"L2CAP init:: ERROR_OUTOFMEMORY\n"));
	return ERROR_OUTOFMEMORY;
}

int l2cap_UninitializeOnce (void) {
	IFDBG(DebugOut (DEBUG_L2CAP_INIT, L"L2CAP uninit:: entered\n"));
	SVSUTIL_ASSERT (gpL2CAP);

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"L2CAP uninit:: ERROR_SERVICE_DOES_NOT_EXIST\n"));
		return ERROR_SERVICE_DOES_NOT_EXIST;
	}

	gpL2CAP->Lock ();

	if (gpL2CAP->eStage != JustCreated) {
		gpL2CAP->Unlock ();
		IFDBG(DebugOut (DEBUG_ERROR, L"L2CAP uninit:: ERROR_DEVICE_IN_USE\n"));
		return ERROR_DEVICE_IN_USE;
	}

	L2CAP *pL2CAP = gpL2CAP;
	gpL2CAP = NULL;
	pL2CAP->Unlock ();

	delete pL2CAP;

	IFDBG(DebugOut (DEBUG_L2CAP_INIT, L"L2CAP uninit:: ERROR_SUCCESS\n"));
	return ERROR_SUCCESS;
}

int l2cap_CreateDriverInstance (void) {
	IFDBG(DebugOut (DEBUG_L2CAP_INIT, L"L2CAP Create Driver Instance:: entered\n"));
	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"L2CAP Create Driver Instance:: ERROR_SERVICE_DOES_NOT_EXIST\n"));
		return ERROR_SERVICE_DOES_NOT_EXIST;
	}

	gpL2CAP->Lock ();
	if (gpL2CAP->eStage != JustCreated) {
		IFDBG(DebugOut (DEBUG_ERROR, L"L2CAP Create Driver Instance:: ERROR_SERVICE_ALREADY_RUNNING\n"));
		gpL2CAP->Unlock ();
		return ERROR_SERVICE_ALREADY_RUNNING;
	}

	gpL2CAP->eStage = Initializing;

	HCI_EVENT_INDICATION ei;
	memset (&ei, 0, sizeof(ei));

	ei.hci_ConnectionCompleteEvent = hci_connection_complete_event;
	ei.hci_ConnectionRequestEvent = hci_connection_request_event;
	ei.hci_DataPacketUp = hci_data_packet_up;
	ei.hci_DisconnectionCompleteEvent = hci_disconnection_complete_event;
	ei.hci_PINCodeRequestEvent = NULL;
	ei.hci_LinkKeyRequestEvent = NULL;

	ei.hci_StackEvent = hci_stack_event;

	HCI_CALLBACKS c;
	memset (&c, 0, sizeof(c));

	c.hci_AcceptConnectionRequest_Out = hci_accept_connection_request_out;
	c.hci_CreateConnection_Out = hci_create_connection_out;
	c.hci_CallAborted = hci_call_aborted;
	c.hci_DataPacketDown_Out = hci_data_packet_down_out;
	c.hci_Disconnect_Out = hci_disconnect_out;
	c.hci_RejectConnectionRequest_Out = hci_reject_connection_request_out;	// should stay NULL

	c.hci_ReadScanEnable_Out = hci_read_scan_enable_out;
	c.hci_WriteScanEnable_Out = hci_write_scan_enable_out;

	c.hci_PINCodeRequestNegativeReply_Out = hci_pin_code_request_negative_reply_out;
	c.hci_PINCodeRequestReply_Out = hci_pin_code_request_reply_out;
	c.hci_LinkKeyRequestNegativeReply_Out = hci_link_key_request_negative_reply_out;
	c.hci_LinkKeyRequestReply_Out = hci_link_key_request_reply_out;

	int iErr;

	if (ERROR_SUCCESS != (iErr = HCI_EstablishDeviceContext(gpL2CAP, BTH_CONTROL_ROUTE_ALL, NULL, 0, 0, &ei, &c, &gpL2CAP->hci_if, &gpL2CAP->cHCIHeader, &gpL2CAP->cHCITrailer, &gpL2CAP->hHCI))) {
		IFDBG(DebugOut (DEBUG_ERROR, L"L2CAP Create Driver Instance: could not plug into HCI -- exiting\n"));
		gpL2CAP->eStage = Error;
		gpL2CAP->Unlock ();

		l2cap_CloseDriverInstance ();
		return iErr;
	}

	//	Then check HCI for adequacy:
	if (! (gpL2CAP->hci_if.hci_AbortCall && gpL2CAP->hci_if.hci_AcceptConnectionRequest_In &&
			gpL2CAP->hci_if.hci_RejectConnectionRequest_In &&
			gpL2CAP->hci_if.hci_CreateConnection_In && gpL2CAP->hci_if.hci_DataPacketDown_In &&
			gpL2CAP->hci_if.hci_Disconnect_In && gpL2CAP->hci_if.hci_ReadScanEnable_In &&
			gpL2CAP->hci_if.hci_WriteScanEnable_In && gpL2CAP->hHCI)) {
		IFDBG(DebugOut (DEBUG_ERROR, L"L2CAP Create Driver Instance: hci does not support necessary features -- exiting\n"));
		gpL2CAP->eStage = Error;
		gpL2CAP->Unlock ();

		l2cap_CloseDriverInstance ();
		return iErr;
	}

	gpL2CAP->pfmdCallContexts = svsutil_AllocFixedMemDescr (sizeof(CallContext), L2CAP_SCALE);
	gpL2CAP->pfmdLogLinks     = svsutil_AllocFixedMemDescr (sizeof(LogLink), L2CAP_SCALE);
	gpL2CAP->pfmdPhysLinks    = svsutil_AllocFixedMemDescr (sizeof(PhysLink), L2CAP_SCALE);
	gpL2CAP->pfmdPSM          = svsutil_AllocFixedMemDescr (sizeof(PSMContext), L2CAP_SCALE);

	if (! (gpL2CAP->pfmdCallContexts && gpL2CAP->pfmdLogLinks && gpL2CAP->pfmdPhysLinks && gpL2CAP->pfmdPSM)) {
		IFDBG(DebugOut (DEBUG_ERROR, L"L2CAP Create Driver Instance: ERROR_OUTOFMEMORY\n"));
		gpL2CAP->eStage = Error;
		gpL2CAP->Unlock ();
		return ERROR_OUTOFMEMORY;
	}

	gpL2CAP->RTX			= L2CAP_RTX;
	gpL2CAP->ERTX			= L2CAP_ERTX;
	gpL2CAP->dwPhysIdle		= L2CAP_PHYSIDLE;
	gpL2CAP->dwConnectIdle	= L2CAP_CONNIDLE;
	gpL2CAP->dwConfigTO		= L2CAP_CONFIGTO;
	gpL2CAP->fPicoCapable   = FALSE;
	gpL2CAP->bRole			= 0;
	gpL2CAP->usLinkPolicy   = 0xffff;
	gpL2CAP->usPacketType   = BT_PACKET_TYPE_DM1 | BT_PACKET_TYPE_DM3 | BT_PACKET_TYPE_DM5;

	gpL2CAP->fScanModeControl = TRUE;

	HKEY hk;

	if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_BASE, L"software\\Microsoft\\bluetooth\\l2cap", 0, KEY_READ, &hk)) {
		DWORD dw = L2CAP_RTX;
		DWORD dwSize = sizeof(dw);
		DWORD dwType = REG_DWORD;
		RegQueryValueEx (hk, L"RTX", NULL, &dwType, (LPBYTE)&dw, &dwSize);
		if ((dwSize != sizeof(dw)) || (dwType != REG_DWORD))
			dw = L2CAP_RTX;

		gpL2CAP->RTX = dw;

		dw = L2CAP_ERTX;
		dwSize = sizeof(dw);
		dwType = REG_DWORD;
		RegQueryValueEx (hk, L"ERTX", NULL, &dwType, (LPBYTE)&dw, &dwSize);
		if ((dwSize != sizeof(dw)) || (dwType != REG_DWORD))
			dw = L2CAP_ERTX;

		gpL2CAP->ERTX = dw;

		dw = L2CAP_PHYSIDLE;
		dwSize = sizeof(dw);
		dwType = REG_DWORD;
		RegQueryValueEx (hk, L"IdlePhys", NULL, &dwType, (LPBYTE)&dw, &dwSize);
		if ((dwSize != sizeof(dw)) || (dwType != REG_DWORD))
			dw = L2CAP_PHYSIDLE;

		gpL2CAP->dwPhysIdle = dw;

		dw = L2CAP_CONNIDLE;
		dwSize = sizeof(dw);
		dwType = REG_DWORD;
		RegQueryValueEx (hk, L"IdleConnect", NULL, &dwType, (LPBYTE)&dw, &dwSize);
		if ((dwSize != sizeof(dw)) || (dwType != REG_DWORD))
			dw = L2CAP_CONNIDLE;

		gpL2CAP->dwConnectIdle = dw;

		dw = L2CAP_CONFIGTO;
		dwSize = sizeof(dw);
		dwType = REG_DWORD;
		RegQueryValueEx (hk, L"ConfigTimeout", NULL, &dwType, (LPBYTE)&dw, &dwSize);
		if ((dwSize != sizeof(dw)) || (dwType != REG_DWORD))
			dw = L2CAP_CONFIGTO;

		gpL2CAP->dwConfigTO = dw;

		dw = 0;
		dwSize = sizeof(dw);
		dwType = REG_DWORD;

		if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"NoRoleSwitch", NULL, &dwType, (LPBYTE)&dw, &dwSize)) &&
			(dwSize == sizeof(dw)) && (dwType == REG_DWORD) && dw)
			gpL2CAP->bRole = 1;

		dw = 0xffff;
		dwSize = sizeof(dw);
		dwType = REG_DWORD;

		if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"LinkPolicy", NULL, &dwType, (LPBYTE)&dw, &dwSize)) &&
			(dwSize == sizeof(dw)) && (dwType == REG_DWORD))
			gpL2CAP->usLinkPolicy = (unsigned short)dw;

		dw = 0;
		dwSize = sizeof(dw);
		dwType = REG_DWORD;

		if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"PicoCapable", NULL, &dwType, (LPBYTE)&dw, &dwSize)) &&
			(dwSize == sizeof(dw)) && (dwType == REG_DWORD) && dw)
			gpL2CAP->fPicoCapable = TRUE;

		dw = BT_PACKET_TYPE_DM1 | BT_PACKET_TYPE_DM3 | BT_PACKET_TYPE_DM5;
		dwSize = sizeof(dw);
		dwType = REG_DWORD;

		if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"ConnectPacketType", NULL, &dwType, (LPBYTE)&dw, &dwSize)) &&
			(dwSize == sizeof(dw)) && (dwType == REG_DWORD) && dw)
			gpL2CAP->usPacketType = (unsigned short)dw;

		dw = 1;
		dwSize = sizeof(dw);
		dwType = REG_DWORD;

		if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"ScanModeControl", NULL, &dwType, (LPBYTE)&dw, &dwSize)) &&
			(dwSize == sizeof(dw)) && (dwType == REG_DWORD))
			gpL2CAP->fScanModeControl = dw ? TRUE : FALSE;

		RegCloseKey (hk);
	}

	gpL2CAP->eStage = Disconnected;

	GetConnectionState ();
	if (gpL2CAP->eStage == Connected)
		SetScanEnable ();

	gpL2CAP->Unlock ();

	return ERROR_SUCCESS;
}

int l2cap_CloseDriverInstance (void) {
	IFDBG(DebugOut (DEBUG_L2CAP_INIT, L"L2CAP Close Driver Instance:: entered\n"));
	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"L2CAP Close Driver Instance:: ERROR_SERVICE_DOES_NOT_EXIST\n"));
		return ERROR_SERVICE_DOES_NOT_EXIST;
	}

	gpL2CAP->Lock ();
	if ((gpL2CAP->eStage == JustCreated) || (gpL2CAP->eStage == ShuttingDown)) {
		IFDBG(DebugOut (DEBUG_ERROR, L"L2CAP Close Driver Instance:: ERROR_SERVICE_NOT_ACTIVE\n"));
		gpL2CAP->Unlock ();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpL2CAP->eStage = ShuttingDown;

	while (gpL2CAP->pCalls)
		AbortCall (gpL2CAP->pCalls, ERROR_OPERATION_ABORTED);

	if (gpL2CAP->hHCI) {
		CallContext *pCall = NULL;

		while (gpL2CAP->pPhysLinks) {
			if (! pCall) {
				pCall = AllocCallContext (CALL_CTX_EVENT, CALL_PHYS_DISCONNECT, NULL, NULL, NULL);
				pCall->fKeepOnAbort = TRUE;
			}

			PhysLink *pNext = gpL2CAP->pPhysLinks->pNext;
			if (pCall) {
				gpL2CAP->Unlock ();
				__try {
					if (ERROR_SUCCESS == gpL2CAP->hci_if.hci_Disconnect_In (gpL2CAP->hHCI, pCall, gpL2CAP->pPhysLinks->h, 0x15))
						WaitForSingleObject (pCall->hEvent, L2CAP_TO);
				} __except (1) {
					IFDBG(DebugOut (DEBUG_ERROR, L"[L2CAP] Disconnect excepted!\n"));
				}
				gpL2CAP->Lock ();

				if (! pCall->fComplete) {
					__try {
						gpL2CAP->hci_if.hci_AbortCall (gpL2CAP->hHCI, pCall);
					} __except (1) {
						IFDBG(DebugOut (DEBUG_ERROR, L"AbortCall excepted!\n"));
					}
				}

				ResetCallContext (pCall);
			}

			delete gpL2CAP->pPhysLinks;
			gpL2CAP->pPhysLinks = pNext;
		}

		if (pCall)
			DeleteCallContext (pCall);

		while (gpL2CAP->GetRefCount () > 1) {
			IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"Waiting for ref count in l2cap_CloseDriverInstance\n"));
			gpL2CAP->Unlock ();
			Sleep (100);
			gpL2CAP->Lock ();
		}

		HCI_CloseDeviceContext (gpL2CAP->hHCI);
		gpL2CAP->hHCI = NULL;
	}

	while (gpL2CAP->pContexts) {
		L2CAP_CONTEXT *pThis = gpL2CAP->pContexts;
		gpL2CAP->pContexts = pThis->pNext;

		pThis->pReservedPorts = NULL;
		pThis->fDefaultServer = FALSE;

		if (pThis->ei.l2ca_StackEvent) {
			BT_LAYER_STACK_EVENT_IND pCallback = pThis->ei.l2ca_StackEvent;
			void *pUserContext = pThis->pUserContext;
			IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Going into StackEvent notification (stack down)\n"));
			pThis->AddRef ();
			gpL2CAP->Unlock ();
			__try {
				pCallback (pUserContext, BTH_STACK_DISCONNECT, NULL);
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Exception in higher layer code\n"));
			}
			gpL2CAP->Lock ();
			pThis->DelRef ();
			IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Came back StackEvent notification\n"));
		}

		while (pThis->GetRefCount () > 1) {
			IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"Waiting for ref count in l2cap_CloseDriverInstance (ctx)\n"));
			gpL2CAP->Unlock ();
			Sleep (100);
			gpL2CAP->Lock ();
		}

		delete pThis;
	}

	gpL2CAP->pPhysLinks = NULL;

	if (gpL2CAP->pfmdPhysLinks) {
		svsutil_ReleaseFixedNonEmpty (gpL2CAP->pfmdPhysLinks);
		gpL2CAP->pfmdPhysLinks = NULL;
	}

	if (gpL2CAP->pfmdLogLinks) {
		svsutil_ReleaseFixedNonEmpty (gpL2CAP->pfmdLogLinks);
		gpL2CAP->pfmdLogLinks = NULL;
	}

	if (gpL2CAP->pfmdCallContexts) {
		svsutil_ReleaseFixedNonEmpty (gpL2CAP->pfmdCallContexts);
		gpL2CAP->pfmdCallContexts = NULL;
	}

	if (gpL2CAP->pfmdPSM) {
		svsutil_ReleaseFixedNonEmpty (gpL2CAP->pfmdPSM);
		gpL2CAP->pfmdPSM = NULL;
	}

	gpL2CAP->ReInit ();	// Reset to JustCreated

	gpL2CAP->Unlock ();

	return ERROR_SUCCESS;
}

static int check_L2CAP_EstablishDeviceContext_parms
(
L2CAP_EVENT_INDICATION	*pInd,				/* IN */
L2CAP_CALLBACKS			*pCall,				/* IN */
L2CAP_INTERFACE			*pInt,				/* OUT */
int						*pcDataHeaders,		/* OUT */
int						*pcDataTrailers,	/* OUT */
HANDLE					*phDeviceContext	/* OUT */
)  {
	__try {
		memset (pInt, 0, sizeof(*pInt));
		*phDeviceContext = NULL;

		L2CAP_EVENT_INDICATION ei;
		memcpy (&ei, pInd, sizeof(ei));

		L2CAP_CALLBACKS c;
		memcpy (&c, pCall, sizeof(c));

		*pcDataHeaders = *pcDataTrailers = 0;
	} __except (1) {
		return FALSE;
	}

	return TRUE;
}

int L2CAP_EstablishDeviceContext
(
void					*pUserContext,		/* IN */
unsigned short			psm,				/* IN */
L2CAP_EVENT_INDICATION	*pInd,				/* IN */
L2CAP_CALLBACKS			*pCall,				/* IN */
L2CAP_INTERFACE			*pInt,				/* OUT */
int						*pcDataHeaders,		/* OUT */
int						*pcDataTrailers,	/* OUT */
HANDLE					*phDeviceContext	/* OUT */
) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"L2CAP_EstablishDeviceContext [0x%08x psm = 0x%04x]\n", pUserContext, psm));

	if ( ! check_L2CAP_EstablishDeviceContext_parms (pInd, pCall, pInt, pcDataHeaders, pcDataTrailers, phDeviceContext)) {
		IFDBG(DebugOut (DEBUG_WARN, L"L2CAP_EstablishDeviceContext returns ERROR_INVALID_PARAMETER (exception)\n"));
		return ERROR_INVALID_PARAMETER;
	}

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_WARN, L"L2CAP_EstablishDeviceContext returns ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpL2CAP->Lock ();
	if (! gpL2CAP->IsStackRunning ()) {
		IFDBG(DebugOut (DEBUG_WARN, L"L2CAP_EstablishDeviceContext returns ERROR_SERVICE_NOT_ACTIVE\n"));
		gpL2CAP->Unlock ();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	L2CAP_CONTEXT *pContext = ((psm != L2CAP_PSM_ALL) && (psm != L2CAP_PSM_MULTIPLE)) ? FindContextByPSM (psm) : NULL;

	if (psm == L2CAP_PSM_ALL) {
		pContext = gpL2CAP->pContexts;
		while (pContext && (! pContext->fDefaultServer))
			pContext = pContext->pNext;
	}

	if (pContext) {
		IFDBG(DebugOut (DEBUG_WARN, L"L2CAP_EstablishDeviceContext returns ERROR_SHARING_VIOLATION\n"));
		gpL2CAP->Unlock ();
		return ERROR_SHARING_VIOLATION;
	}

	PSMContext *pPSM = NULL;

	if ((psm != L2CAP_PSM_ALL) && (psm != L2CAP_PSM_MULTIPLE)) {
		pPSM = new PSMContext;
		if (! pPSM) {
			IFDBG(DebugOut (DEBUG_WARN, L"L2CAP_EstablishDeviceContext returns ERROR_OUTOFMEMORY\n"));
			gpL2CAP->Unlock ();
			return ERROR_OUTOFMEMORY;
		}
		pPSM->pNext = NULL;
		pPSM->usPSM = psm;
	}

	pContext = new L2CAP_CONTEXT;

	if (! pContext) {
		if (pPSM)
			delete pPSM;

		IFDBG(DebugOut (DEBUG_WARN, L"L2CAP_EstablishDeviceContext returns ERROR_OUTOFMEMORY\n"));
		gpL2CAP->Unlock ();
		return ERROR_OUTOFMEMORY;
	}

	pContext->pReservedPorts = pPSM;
	pContext->fDefaultServer = (psm == L2CAP_PSM_ALL);

	pContext->ei = *pInd;
	pContext->c = *pCall;
	pContext->pUserContext = pUserContext;
	SVSUTIL_ASSERT (pContext->GetRefCount() == 1);
	pContext->pNext = gpL2CAP->pContexts;
	gpL2CAP->pContexts = pContext;

	*phDeviceContext = pContext;

	pInt->l2ca_ConnectReq_In = l2ca_connect_req_in;
	pInt->l2ca_ConnectResponse_In = l2ca_connect_response_in;
	pInt->l2ca_ConfigReq_In = l2ca_config_req_in;
	pInt->l2ca_ConfigResponse_In = l2ca_config_response_in;
	pInt->l2ca_DataDown_In = l2ca_data_down_in;
	pInt->l2ca_Disconnect_In = l2ca_disconnect_in;
	pInt->l2ca_ioctl = l2ca_ioctl;
	pInt->l2ca_AbortCall = l2ca_abort_call;
	pInt->l2ca_Ping_In = l2ca_ping_in;

	*pcDataTrailers = gpL2CAP->cHCITrailer;
	*pcDataHeaders = gpL2CAP->cHCIHeader + 4;

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"L2CAP_EstablishDeviceContext returns ERROR_SUCCESS\n"));
	gpL2CAP->Unlock ();
	return ERROR_SUCCESS;
}

int L2CAP_CloseDeviceContext (HANDLE hDeviceContext) {
	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"L2CAP_CloseDeviceContext [0x%08x]\n", hDeviceContext));

	if (! gpL2CAP) {
		IFDBG(DebugOut (DEBUG_WARN, L"L2CAP_CloseDeviceContext returns ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpL2CAP->Lock ();
	if (! gpL2CAP->IsStackRunning ()) {
		IFDBG(DebugOut (DEBUG_WARN, L"L2CAP_CloseDeviceContext returns ERROR_SERVICE_NOT_ACTIVE\n"));
		gpL2CAP->Unlock ();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	int iRes = ERROR_SUCCESS;
	while (gpL2CAP && (gpL2CAP->IsStackRunning ())) {
		L2CAP_CONTEXT *pOwner = VerifyContext ((L2CAP_CONTEXT *)hDeviceContext);

		if (! pOwner) {
			iRes = ERROR_NOT_FOUND;
			break;
		}

		PSMContext *pRunningPorts = pOwner->pReservedPorts;
		pOwner->pReservedPorts = NULL;

		while (pRunningPorts) {
			PSMContext *pNext = pRunningPorts->pNext;
			delete pRunningPorts;
			pRunningPorts = pNext;
		}

		if (pOwner->GetRefCount () > 1) {
			IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"Waiting for ref count in L2CAP_CloseDeviceContext\n"));
			gpL2CAP->Unlock ();
			Sleep (100);
			gpL2CAP->Lock ();
			continue;
		}

		CallContext *pCall = gpL2CAP->pCalls;
		while (pCall && (pCall->pOwner != pOwner))
			pCall = pCall->pNext;

		if (pCall) {
			CancelCall (pCall, ERROR_CANCELLED);
			continue;
		}

		PhysLink *pPhys = gpL2CAP->pPhysLinks;
		LogLink *pLog = NULL;
		while (pPhys && (! pLog)) {
			pLog = pPhys->pLogLinks;
			pPhys = pPhys->pNext;

			while (pLog && (pLog->pOwner != pOwner))
				pLog = pLog->pNext;
		}

		if (pLog) {
			DisconnectLogicalLink (pLog, ERROR_CANCELLED, TRUE);
			continue;
		}

		if (gpL2CAP->pContexts == pOwner)
			gpL2CAP->pContexts = pOwner->pNext;
		else {
			L2CAP_CONTEXT *pParent = gpL2CAP->pContexts;
			while (pParent && (pParent->pNext != pOwner))
				pParent = pParent->pNext;
			pParent->pNext = pOwner->pNext;
		}

		delete pOwner;
		break;
	}

	IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"L2CAP_CloseDeviceContext returns %d", iRes));
	gpL2CAP->Unlock ();
	return iRes;
}

//
//	Allocations
//
void *PSMContext::operator new (size_t iSize) {
	SVSUTIL_ASSERT (iSize == sizeof(PSMContext));
	void *pRes = svsutil_GetFixed (gpL2CAP->pfmdPSM);
	SVSUTIL_ASSERT (pRes);
	return pRes;
}

void PSMContext::operator delete(void *ptr) {
	SVSUTIL_ASSERT (ptr);
	svsutil_FreeFixed (ptr, gpL2CAP->pfmdPSM);
}

void *CallContext::operator new (size_t iSize) {
	SVSUTIL_ASSERT (iSize == sizeof(CallContext));
	void *pRes = svsutil_GetFixed (gpL2CAP->pfmdCallContexts);
	SVSUTIL_ASSERT (pRes);
	return pRes;
}

void CallContext::operator delete(void *ptr) {
	SVSUTIL_ASSERT (ptr);
	svsutil_FreeFixed (ptr, gpL2CAP->pfmdCallContexts);
}

void *PhysLink::operator new (size_t iSize) {
	SVSUTIL_ASSERT (iSize == sizeof(PhysLink));
	void *pRes = svsutil_GetFixed (gpL2CAP->pfmdPhysLinks);
	SVSUTIL_ASSERT (pRes);
	return pRes;
}

void PhysLink::operator delete (void *ptr) {
	SVSUTIL_ASSERT (ptr);
	svsutil_FreeFixed (ptr, gpL2CAP->pfmdPhysLinks);
}

void *LogLink::operator new (size_t iSize) {
	SVSUTIL_ASSERT (iSize == sizeof(LogLink));
	void *pRes = svsutil_GetFixed (gpL2CAP->pfmdLogLinks);
	SVSUTIL_ASSERT (pRes);
	return pRes;
}

void LogLink::operator delete (void *ptr) {
	SVSUTIL_ASSERT (ptr);
	svsutil_FreeFixed (ptr, gpL2CAP->pfmdLogLinks);
}

static CallContext *AllocCallContext (CALL_CONTEXT_TYPE eType, CALL_OP eWhat, void *pLink, L2CAP_CONTEXT *pOwner, void *pCallContext) {
	CallContext *pContext = new CallContext;
	if (! pContext)
		return NULL;

	memset (pContext, 0, sizeof(*pContext));

	pContext->eType = eType;

	if (eType == CALL_CTX_EVENT) {
		pContext->hEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
		if (! pContext->hEvent) {
			delete pContext;
			return NULL;
		}
	}

	pContext->eWhat = eWhat;
	pContext->u.pLink = pLink;

	pContext->pContext = pCallContext;
	pContext->pOwner = pOwner;

	pContext->id = gpL2CAP->ucCurrentID++;
	if (! pContext->id)
		pContext->id = gpL2CAP->ucCurrentID++;

	pContext->pNext = gpL2CAP->pCalls;
	gpL2CAP->pCalls = pContext;

	return pContext;
}

static void DeleteCallContext (CallContext *pCall) {
	CallContext *pRunner = gpL2CAP->pCalls;
	CallContext *pParent = NULL;

	while (pRunner && (pRunner != pCall)) {
		pParent = pRunner;
		pRunner = pRunner->pNext;
	}

	if (! pRunner)
		return;

	if (pParent)
		pParent->pNext = pRunner->pNext;
	else
		gpL2CAP->pCalls = pRunner->pNext;

	if (pRunner->eType == CALL_CTX_EVENT)
		CloseHandle (pRunner->hEvent);

	if (pRunner->pData)
		g_funcFree (pRunner->pData, g_pvFreeData);

	if (pRunner->dwTimeOutCookie)
		UnscheduleTimeout (pRunner);

	delete pRunner;
}

static void ResetCallContext (CallContext *pCall) {
	pCall->fComplete = FALSE;
	pCall->iResult = -1;
}

void L2CAPD_CheckLock (void) {
	SVSUTIL_ASSERT ((! gpL2CAP) || (! gpL2CAP->IsLocked ()));
}


//
//	Console output
//
#if defined (BTH_CONSOLE)

int l2cap_ProcessConsoleCommand (WCHAR *pszCommand) {
	if (! gpL2CAP)
		return ERROR_SERVICE_NOT_ACTIVE;

	int iRes = ERROR_SUCCESS;
	gpL2CAP->Lock ();
	__try {
		if (wcsicmp (pszCommand, L"help") == 0) {
			DebugOut (DEBUG_OUTPUT, L"L2CAP Commands:\n");
			DebugOut (DEBUG_OUTPUT, L"    help        prints this text\n");
			DebugOut (DEBUG_OUTPUT, L"    global      dumps global state\n");
			DebugOut (DEBUG_OUTPUT, L"    links       dumps currently active connections\n");
			DebugOut (DEBUG_OUTPUT, L"    contexts    dumps currently installed L2CAP clients\n");
			DebugOut (DEBUG_OUTPUT, L"    security    dumps currently installed L2CAP clients\n");
			DebugOut (DEBUG_OUTPUT, L"    calls       dumps currently pending operations\n");
		} else if (wcsicmp (pszCommand, L"global") == 0) {
			DebugOut (DEBUG_OUTPUT, L"L2CAP global state                  : %s\n",
				(gpL2CAP->eStage == JustCreated) ? L"JustCreated" :
				((gpL2CAP->eStage == Initializing) ? L"Initializing" :
				((gpL2CAP->eStage == Connected) ? L"Connected" :
				((gpL2CAP->eStage == Disconnected) ? L"Disconnected" :
				((gpL2CAP->eStage == ShuttingDown) ? L"ShuttingDown" :
				((gpL2CAP->eStage == Error) ? L"Error" : L"ERROR! -- Unrecognized state!"))))));

			int iCount = 0;
			PhysLink *pPhysLinks = gpL2CAP->pPhysLinks;
			while (pPhysLinks) {
				pPhysLinks = pPhysLinks->pNext;
				++iCount;
			}

			DebugOut (DEBUG_OUTPUT, L"Baseband connections                : %d\n", iCount);
			iCount = 0;
			L2CAP_CONTEXT *pContexts = gpL2CAP->pContexts;
			while (pContexts) {
				pContexts = pContexts->pNext;
				++iCount;
			}
			DebugOut (DEBUG_OUTPUT, L"Client stacks                       : %d\n", iCount);
			iCount = 0;
			CallContext	*pCalls = gpL2CAP->pCalls;
			while (pCalls) {
				pCalls = pCalls->pNext;
				++iCount;
			}
			DebugOut (DEBUG_OUTPUT, L"Pending operations                  : %d\n", iCount);
			DebugOut (DEBUG_OUTPUT, L"PhysLinks FixedMem descriptor       : 0x%08x\n", gpL2CAP->pfmdPhysLinks);
			DebugOut (DEBUG_OUTPUT, L"LogLinks FixedMem descriptor        : 0x%08x\n", gpL2CAP->pfmdLogLinks);
			DebugOut (DEBUG_OUTPUT, L"Call FixedMem descriptor            : 0x%08x\n", gpL2CAP->pfmdCallContexts);
			DebugOut (DEBUG_OUTPUT, L"HCI handle                          : 0x%08x\n", gpL2CAP->hHCI);
			DebugOut (DEBUG_OUTPUT, L"HCI header bytes                    : %d\n", gpL2CAP->cHCIHeader);
			DebugOut (DEBUG_OUTPUT, L"HCI trailer bytes                   : %d\n", gpL2CAP->cHCITrailer);
			DebugOut (DEBUG_OUTPUT, L"Echoes counter                      : %d\n", gpL2CAP->iEchoes);
			DebugOut (DEBUG_OUTPUT, L"CID counter                         : %d\n", gpL2CAP->usCurrentCID);
			DebugOut (DEBUG_OUTPUT, L"ID counter                          : %d\n", gpL2CAP->ucCurrentID);
			DebugOut (DEBUG_OUTPUT, L"Role                                : %d\n", gpL2CAP->bRole);
			DebugOut (DEBUG_OUTPUT, L"Packet Types                        : 0x%04x\n", gpL2CAP->usPacketType);
			DebugOut (DEBUG_OUTPUT, L"Link Policy                         : 0x%04x\n", gpL2CAP->usLinkPolicy);
			DebugOut (DEBUG_OUTPUT, L"RTX                                 : %d\n", gpL2CAP->RTX);
			DebugOut (DEBUG_OUTPUT, L"ERTX                                : %d\n", gpL2CAP->ERTX);
			DebugOut (DEBUG_OUTPUT, L"Phys Idle                           : %d\n", gpL2CAP->dwPhysIdle);
			DebugOut (DEBUG_OUTPUT, L"Connect Idle                        : %d\n", gpL2CAP->dwConnectIdle);
			DebugOut (DEBUG_OUTPUT, L"Config Timeout                      : %d\n", gpL2CAP->dwConfigTO);
			DebugOut (DEBUG_OUTPUT, L"Support for piconets                : %s\n", gpL2CAP->fPicoCapable ? L"yes" : L"no");
			DebugOut (DEBUG_OUTPUT, L"Control for scan mode               : %s\n", gpL2CAP->fScanModeControl ? L"yes" : L"no");
		} else if (wcsicmp (pszCommand, L"links") == 0) {
			PhysLink *pC = gpL2CAP->pPhysLinks;
			while (pC) {
				DebugOut (DEBUG_OUTPUT, L"Baseband connection @ 0x%08x\n", pC);
				DebugOut (DEBUG_OUTPUT, L"    BD_ADDR         : %04x%08x\n", pC->b.NAP, pC->b.NAP);
				DebugOut (DEBUG_OUTPUT, L"    HANDLE          : 0x%04x\n", pC->h);
				DebugOut (DEBUG_OUTPUT, L"    Stage           : %s\n",
					(pC->eStage == STARTING_PHYS) ? L"STARTING_PHYS" :
					((pC->eStage == STARTING) ? L"STARTING" :
					((pC->eStage == STARTING_REQUEST) ? L"STARTING_REQUEST" :
					((pC->eStage == CONFIG) ? L"CONFIG" :
					((pC->eStage == CONFIG_LOCAL_DONE) ? L"CONFIG_LOCAL_DONE" :
					((pC->eStage == CONFIG_REMOTE_DONE) ? L"CONFIG_REMOTE_DONE" :
					((pC->eStage == UP) ? L"UP" :
					((pC->eStage == DISCONNECTED) ? L"DISCONNECTED" : L"ERROR! -- Undefined state"))))))));
				DebugOut (DEBUG_OUTPUT, L"    T/O handle      : 0x%08x\n", pC->dwTimeOutCookie);
				DebugOut (DEBUG_OUTPUT, L"    Reattemtps      : %d\n", pC->iConnectionAttempts);
				DebugOut (DEBUG_OUTPUT, L"    Errors          : %d\n", pC->iTransmissionProblems);
				DebugOut (DEBUG_OUTPUT, L"    Pings           : %d\n", pC->iPingsSent);
				DebugOut (DEBUG_OUTPUT, L"    Packet Types    : 0x%04x\n", pC->usPacketType);
				LogLink *pL = pC->pLogLinks;
				while (pL) {
					DebugOut (DEBUG_OUTPUT, L"Log Link @ 0x%08x Local 0x%04x Remote 0x%04x PSM 0x%04x Owner 0x%08x T/O 0x%08x Stage %s\n",
						pL, pL->cid, pL->cid_remote, pL->psm, pL->pOwner,  pL->dwTimeOutCookie,
						(pL->eStage == STARTING_PHYS) ? L"STARTING_PHYS" :
						((pL->eStage == STARTING) ? L"STARTING" :
						((pL->eStage == STARTING_REQUEST) ? L"STARTING_REQUEST" :
						((pL->eStage == CONFIG) ? L"CONFIG" :
						((pL->eStage == CONFIG_LOCAL_DONE) ? L"CONFIG_LOCAL_DONE" :
						((pL->eStage == CONFIG_REMOTE_DONE) ? L"CONFIG_REMOTE_DONE" :
						((pL->eStage == UP) ? L"UP" :
						((pL->eStage == DISCONNECTED) ? L"DISCONNECTED" : L"ERROR! -- Undefined state"))))))));
					pL = pL->pNext;
				}
				pC = pC->pNext;
			}
		} else if (wcsicmp (pszCommand, L"contexts") == 0) {
			L2CAP_CONTEXT *pC = gpL2CAP->pContexts;
			while (pC) {
				DebugOut (DEBUG_OUTPUT, L"Client Context @ 0x%08x\n", pC);
				DebugOut (DEBUG_OUTPUT, L"    User Context : 0x%08x\n", pC->pUserContext);
				DebugOut (DEBUG_OUTPUT, L"    Packet Types : 0x%04x\n", pC->usPacketType);
				PSMContext *pPSMCtx = pC->pReservedPorts;
				while (pPSMCtx) {
					DebugOut (DEBUG_OUTPUT, L"    Reserved PSM : 0x%04x\n", pPSMCtx->usPSM);
					pPSMCtx = pPSMCtx->pNext;
				}
				pC = pC->pNext;
			}
		} else if (wcsicmp (pszCommand, L"calls") == 0) {
			CallContext *pC = gpL2CAP->pCalls;
			while (pC) {
				DebugOut (DEBUG_OUTPUT, L"Call Context @ 0x%08x\n", pC);
				DebugOut (DEBUG_OUTPUT, L"    Operation  : %s\n",
					(pC->eWhat == CALL_HCI_READSCAN) ? L"CALL_HCI_READSCAN" :
					((pC->eWhat == CALL_HCI_WRITESCAN) ? L"CALL_HCI_WRITESCAN" :
					((pC->eWhat == CALL_PHYS_CONNECT) ? L"CALL_PHYS_CONNECT" :
					((pC->eWhat == CALL_PHYS_ACCEPT) ? L"CALL_PHYS_ACCEPT" :
					((pC->eWhat == CALL_PHYS_DISCONNECT) ? L"CALL_PHYS_DISCONNECT" :
					((pC->eWhat == CALL_PHYS_DROP_IDLE) ? L"CALL_PHYS_DROP_IDLE" :
					((pC->eWhat == CALL_PHYS_PING) ? L"CALL_PHYS_PING" :
					((pC->eWhat == CALL_LOG_CONNECT_REQ) ? L"CALL_LOG_CONNECT_REQ" :
					((pC->eWhat == CALL_LOG_CONNECT_RESP) ? L"CALL_LOG_CONNECT_RESP" :
					((pC->eWhat == CALL_LOG_CONFIG_REQ) ? L"CALL_LOG_CONFIG_REQ" :
					((pC->eWhat == CALL_LOG_CONFIG_RESP) ? L"CALL_LOG_CONFIG_RESP" :
					((pC->eWhat == CALL_LOG_DISCONNECT_REQ) ? L"CALL_LOG_DISCONNECT_REQ" :
					((pC->eWhat == CALL_USERDATA) ? L"CALL_USERDATA" : L"ERROR! -- Unknown call op")))))))))))));

				DebugOut (DEBUG_OUTPUT, L"    Call Type  : %s\n",
					(pC->eType == CALL_CTX_UNINITIALIZED) ? L"CALL_CTX_UNINITIALIZED" :
					((pC->eType == CALL_CTX_EVENT) ? L"CALL_CTX_EVENT" :
					((pC->eType == CALL_CTX_CALLBACK) ? L"CALL_CTX_CALLBACK" :
					((pC->eType == CALL_CTX_CALLOWNER) ? L"CALL_CTX_CALLOWNER" :
					((pC->eType == CALL_CTX_INTERNAL) ? L"CALL_CTX_INTERNAL" : L"ERROR! -- Unknwon call type")))));
				if (pC->u.pLink) {
					switch (pC->eWhat) {
						case CALL_PHYS_CONNECT:
						case CALL_PHYS_ACCEPT:
						case CALL_PHYS_DISCONNECT:
						case CALL_PHYS_PING:
							DebugOut (DEBUG_OUTPUT, L"    Phys Link  : %04x%08x\n", pC->u.pPhysLink->b.NAP, pC->u.pPhysLink->b.SAP);
						break;

						case CALL_LOG_CONNECT_RESP:
						case CALL_LOG_CONFIG_RESP:
						case CALL_LOG_CONNECT_REQ:
						case CALL_LOG_CONFIG_REQ:
						case CALL_USERDATA:
						case CALL_LOG_DISCONNECT_REQ:
							DebugOut (DEBUG_OUTPUT, L"    Log  Link  : %04x%08x loc 0x%04x rem 0x%04x\n", pC->u.pPhysLink->b.NAP, pC->u.pPhysLink->b.SAP, pC->u.pLogLink->cid, pC->u.pLogLink->cid_remote);
						break;
						default:
							DebugOut (DEBUG_OUTPUT, L"    Unkn Link  : 0x%08x\n", pC->u.pLink);
					}
				} else
					DebugOut (DEBUG_OUTPUT, L"    Link       : none\n");
				DebugOut (DEBUG_OUTPUT, L"    ID         : %d\n", pC->id);
				DebugOut (DEBUG_OUTPUT, L"    Foreign    : %s\n", pC->fForeignId ? L"yes" : L"no");
				DebugOut (DEBUG_OUTPUT, L"    Complete   : %s\n", pC->fComplete ? L"yes" : L"no");
				DebugOut (DEBUG_OUTPUT, L"    Keep       : %s\n", pC->fKeepOnAbort ? L"yes" : L"no");
				DebugOut (DEBUG_OUTPUT, L"    Result     : %d ( = 0x%08x)\n", pC->iResult, pC->iResult);
				DebugOut (DEBUG_OUTPUT, L"    Owner      : 0x%08x\n", pC->pOwner);
				DebugOut (DEBUG_OUTPUT, L"    Context    : 0x%08x\n", pC->pContext);
				DebugOut (DEBUG_OUTPUT, L"    Event/Call : 0x%08x\n", pC->hEvent);
				DebugOut (DEBUG_OUTPUT, L"    Data       : 0x%08x (%d bytes)\n", pC->pData, pC->cData);
				DebugOut (DEBUG_OUTPUT, L"    R          : 0x%08x 0x%04x 0x%02x\n", pC->r.uiresult, pC->r.usresult, pC->r.ucresult);
				pC = pC->pNext;
			}
		} else
			iRes = ERROR_NOT_FOUND;
	} __except (1) {
		DebugOut (DEBUG_OUTPUT, L"ERROR! -- Exception during command processing!\n");
	}
	gpL2CAP->Unlock ();

	return iRes;
}

#endif		// BTH_CONSOLE
