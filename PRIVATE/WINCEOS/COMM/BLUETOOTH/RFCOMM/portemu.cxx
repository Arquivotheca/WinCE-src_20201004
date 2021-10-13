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
//      Bluetooth Virtual Comm Port
// 
// 
// Module Name:
// 
//      portemu.cxx
// 
// Abstract:
// 
//      This file implements vCOMM port layer
// 
// 
//------------------------------------------------------------------------------
#include <windows.h>
#include <svsutil.hxx>
#include <bt_buffer.h>
#include <bt_ddi.h>
#include <bt_api.h>
#include <bt_debug.h>
#include <bt_sdp.h>
#include <bthapi.h>
#include <ceport.h>
#include <sdplib.h>

#include <bt_os.h>

#include <windev.h>
#include <pegdser.h>

#define EA_BIT				0x01

#define BIT(c) (1 << (c))

#define MSC_EA_BIT      EA_BIT
#define MSC_FC_BIT      BIT(1)      // Flow control, clear if we can receive
#define MSC_RTC_BIT     BIT(2)      // Ready to communicate, set when ready
#define MSC_RTR_BIT     BIT(3)      // Ready to receive, set when ready
#define MSC_IC_BIT      BIT(6)      // Incoming call
#define MSC_DV_BIT      BIT(7)      // Data valid

#define PORTEMU_OPEN_MASK		0x3
#define PORTEMU_OPEN_MAX		4	// Must be power of 2; x - 1 used for mask
#define PORTEMU_OPEN_ALLUSED	((1 << PORTEMU_OPEN_MAX) - 1)

#define PORTEMU_MAX_RFCOMM_CHANNEL	31

#define PORTEMU_OPEN_ATTEMPTS		5
#define PORTEMU_OPEN_TIMEOUT		1000

enum COMM_OP {
	COMM_NO_OP      = 0,
	COMM_OPEN       = 1,
	COMM_WAIT_EVENT = 2,
	COMM_READ       = 3,
	COMM_WRITE      = 4,
	COMM_RPN        = 5,
	COMM_DISC		= 6
};

struct PendingIO : public SVSAllocClass {
	PendingIO		*pNext;			// Next in list
	HANDLE			hEvent;			// Event to signal on completion
	COMM_OP			op;				// COMM_OP (open, wait event, read, write)

	unsigned char	*buffer;		// Buffer
	int				cbuffer;		// Size of buffer
	DWORD			dwPerms;		// Permissions

	int				cbuffer_used;	// Bytes used in the buffer so far
	int				iIoResult;		// IO result on completion

	DWORD			dwEvent;		// COMM events
	DWORD			dwLineError;	// COMM line errors

	int				iPortNum;		// Port number for this IO

	PendingIO (COMM_OP a_op, int a_iPortNum);
	~PendingIO (void);
};

enum RFCOMM_OP {
	NO_OP          = 0,
	RFCOMM_CONNECT = 1,
	RFCOMM_SDP     = 2,
	RFCOMM_PN      = 3,
	RFCOMM_WRITE   = 4,
	RFCOMM_RPN     = 5,
	RFCOMM_DISC    = 6
};

struct PORTEMU_CONTEXT;

struct ECall {
	ECall			*pNext;			// Next in list

	RFCOMM_OP		fWhat;			// RFCOMM_OP (connect or write)

	int				cBytes;			// Number of bytes on write

	PORTEMU_CONTEXT	*pContext;		// Owner context

	ECall (RFCOMM_OP a_fWhat, PORTEMU_CONTEXT *a_pContext) {
		fWhat    = a_fWhat;
		pContext = a_pContext;
		pNext    = NULL;
		cBytes   = 0;
	}

	void *operator new (size_t size);
	void operator delete (void *ptr);
};

struct BD_BUFFER_LIST {
	BD_BUFFER_LIST	*pNext;			// Next in list
	BD_BUFFER		*pb;			// Buffer

	void *operator new (size_t size);
	void operator delete (void *ptr);
};

//	This structure is what describes the device. Several important notes.
//	Lifecycle starts with registering device and ends with unregistering it.
//	The port is connected when it is opened, and disconnected when it's closed.
//	Ownership only commences with OPENING the device. Ownership is surrendered
//	by CLOSING device. When device is unowned, the com port can be configured
//	using the following call:
//		RFCOMMSetServerChannel (int index,  int channel, int flocal, BD_ADDR *pdevice, int iminmtu, int imaxmtu);
//		{if (flocal) ASSERT (! pdevice);}
//
struct PORTEMU_CONTEXT : public SVSAllocClass {
	PORTEMU_CONTEXT		*pNext;				// Next in list

	HANDLE				hRFCOMM;			// RFCOMM handle

	RFCOMM_INTERFACE	rfcomm_if;			// RFCOMM interface table
	int					iDeviceHead;		// Pre-alloc in front of the buffer
	int					iDeviceTrail;		// Pre-alloc at the end of the buffer

	unsigned int		channel      : 5;	// Server channel id
	unsigned int		fLocal       : 1;	// Local channel
	unsigned int		fLocalOpen   : 1;	// Port really connected
	unsigned int		fSdpQueryOn  : 1;	// SDP query is in progress
	unsigned int		fSentXon     : 1;	// Xon sent
	unsigned int		fc           : 1;	// flow, 1 = don't send
	unsigned int		fc_aggregate : 1;	// aggregate flow, 1 = don't send
	unsigned int		credit_fc    : 1;	// use credit-based flow
	unsigned int		local_dcb    : 1;	// use local DCB, do not send RPN commands
	unsigned int		fkeep_dcd    : 1;	// keep DCD up for the duration of connection
	unsigned int		freq_auth    : 1;	// require authentication
	unsigned int        freq_encrypt : 1;	// require encryption
	unsigned int		uiOpenRef    : 4;	// Open ref field
	unsigned int		uiReadRef    : 4;	// Open ref field
	unsigned int		uiWriteRef   : 4;	// Open ref field

	unsigned int		fSending     : 1;	// in SendData
	unsigned int		fClosing	 : 1;	// Closing port
	unsigned int		fHaveClient  : 1;	// client connected to server

	int					iHaveCredits;		// Have credits
	int					iGaveCredits;		// Credits outstanding

	BD_ADDR				b;					// BD_ADDR of the other side

	HANDLE				hConnection;		// Connection handle (NULL = not connected)

	int					iMTU;				// Negotiated MTU

	int					iMinMTU;			// MIN MTU for negotiation
	int					iMaxMTU;			// MAX MTU for negotiation

	int					iSendQuota;			// Send quota
	int					iSendQuotaUsed;		// Used quota for send

	int					iRecvQuota;			// Recv quota
	int					iRecvQuotaUsed;		// Used recv quota

	DWORD				adwEnabledEvents[PORTEMU_OPEN_MAX];			// Enabled COMM events (SetCommEvents)
	DWORD				adwOccuredEvents[PORTEMU_OPEN_MAX];			// Occured COMM events (WaitCommEvents)
	DWORD				adwErr[PORTEMU_OPEN_MAX];					// COMM line error
	HANDLE				hOwnerProc[PORTEMU_OPEN_MAX];				// Owner proc

	unsigned char		ucv24out;	// COMM modem status on outgoing lines
	DWORD				dwModemStatusIn;	// COMM modem status on incoming lines
	PendingIO			*pops;				// list of pending io
	BD_BUFFER_LIST		*pbl;				// Arrived data list

	ECall				*pCalls;			// pending calls


	DCB					dcb;				// DCB
	COMMTIMEOUTS		ct;					// COMMTIMEOUTS

	GUID				uuidService;		//UUID of the service
	unsigned short      sdpCid;				//channel id for sdp search

	PORTEMU_CONTEXT (void) {
		memset (this, 0, sizeof(*this));
		fLocal       = TRUE;

		local_dcb               = TRUE;

		dcb.DCBlength			= sizeof(DCB);
		dcb.BaudRate			= CBR_115200;
		dcb.fBinary				= TRUE;
		dcb.fParity				= FALSE;
		dcb.fOutxCtsFlow		= FALSE;
		dcb.fOutxDsrFlow		= FALSE;
		dcb.fDtrControl			= DTR_CONTROL_DISABLE;
		dcb.fDsrSensitivity		= FALSE;
		dcb.fTXContinueOnXoff	= TRUE;
		dcb.fOutX				= FALSE;
		dcb.fInX				= FALSE;
		dcb.fErrorChar			= FALSE;
		dcb.fNull				= FALSE;
		dcb.fRtsControl         = RTS_CONTROL_DISABLE;
		dcb.fAbortOnError       = TRUE;
		dcb.XonLim				= PORTEMU_XONLIM;
		dcb.XoffLim				= PORTEMU_XOFFLIM;
		dcb.ByteSize			= 8;
		dcb.Parity				= NOPARITY;
		dcb.StopBits			= ONESTOPBIT;

		ct.ReadIntervalTimeout         = -1;
		ct.ReadTotalTimeoutConstant    = PORTEMU_RTO;
		ct.ReadTotalTimeoutMultiplier  = 0;
		ct.WriteTotalTimeoutConstant   = PORTEMU_WTO;
		ct.WriteTotalTimeoutMultiplier = 0;
	}

	~PORTEMU_CONTEXT (void);
};

class PORTEMU : public SVSAllocClass, public SVSSynch, public SVSRefObj {
public:
	PORTEMU_CONTEXT		*pPorts;				// List of ports
	FixedMemDescr		*pfmdPBL;				// FMD for buffer list
	FixedMemDescr		*pfmdCalls;				// FMD for calls

	int					iPIOBlocksUsed;			// Ref count on PIO blocks
	int					fInitialized;			// Initialized?

	int					iDefaultMTUMin;			// Default MTU Min
	int					iDefaultMTUMax;			// Default MTU Max
	int					iDefaultSendQuota;		// Default Send Quota
	int					iDefaultRecvQuota;		// Default Recv Quota

	HANDLE				hSDP;					// SDP handle
	SDP_INTERFACE		sdp_if;					// sdp interface table

	PORTEMU (void) {
		pPorts = NULL;
		pfmdPBL = NULL;
		pfmdCalls = NULL;

		iPIOBlocksUsed = 0;
		fInitialized = FALSE;

		iDefaultMTUMin = iDefaultMTUMax = 0;
		iDefaultSendQuota = iDefaultRecvQuota = 0;

		hSDP = NULL;
		memset(&sdp_if, 0, sizeof(sdp_if));
	}
};

extern "C" BOOL COM_Close (DWORD dwData);

//
//	Static data
//
static PORTEMU	*gpPORTEMU;

PendingIO::PendingIO (COMM_OP a_op, int a_iPortNum) {
	++gpPORTEMU->iPIOBlocksUsed;
	memset (this, 0, sizeof(*this));
	op = a_op;
	iPortNum = a_iPortNum;
	hEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
}

PendingIO::~PendingIO (void) {
	--gpPORTEMU->iPIOBlocksUsed;
	CloseHandle (hEvent);

#if defined (DEBUG) || defined (_DEBUG)
	PORTEMU_CONTEXT *pCtx = gpPORTEMU->pPorts;
	while (pCtx) {
		PendingIO *pio = pCtx->pops;
		while (pio && (pio != this))
			pio = pio->pNext;

		SVSUTIL_ASSERT (! pio);
		pCtx = pCtx->pNext;
	}
#endif
}

static int IsSerialIOCTL (DWORD ioctl_code) {
	if (((ioctl_code >> 16) & 0xffff) == FILE_DEVICE_SERIAL_PORT)
		return TRUE;

	return FALSE;
}

static int byte_to_baud[9] = {CBR_2400, CBR_4800, 7200, CBR_9600, CBR_19200, CBR_38400, CBR_57600, CBR_115200, 230400};

static int BaudToByte (int baud) {
	for (int i = 0 ; i < sizeof(byte_to_baud) ; ++i) {
		if (byte_to_baud[i] == baud)
			return i;
	}

	IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] ByteToBaud : baud rate translation failed, baud = %d!\n", baud));
	return -1;
}

static int VerifyDCB (DCB *pdcb) {
	if (pdcb->DCBlength != sizeof(*pdcb))
		return FALSE;

	if (-1 == BaudToByte (pdcb->BaudRate))
		return FALSE;

	if ((pdcb->ByteSize < 5) || (pdcb->ByteSize > 8))
		return FALSE;

	if ((pdcb->StopBits != ONESTOPBIT) && (pdcb->StopBits != ONE5STOPBITS))
		return FALSE;

	if (pdcb->fParity && (pdcb->Parity > 4))
		return FALSE;

	if (! pdcb->fBinary)
		return FALSE;

	if (pdcb->fOutxCtsFlow || pdcb->fOutxDsrFlow || pdcb->fDsrSensitivity ||
			(pdcb->fDtrControl != DTR_CONTROL_DISABLE) || (pdcb->fRtsControl != RTS_CONTROL_DISABLE))
		return FALSE;

	if (pdcb->fOutX || pdcb->fInX || pdcb->fErrorChar || pdcb->fNull)
		return FALSE;

	if (pdcb->XonLim < pdcb->XoffLim)
		return FALSE;

	pdcb->EofChar = pdcb->ErrorChar = pdcb->EvtChar = pdcb->XoffChar = pdcb->XonChar = 0;

	pdcb->fAbortOnError = TRUE;
	pdcb->fTXContinueOnXoff = TRUE;

	return TRUE;
}

static ECall *NewCall (RFCOMM_OP fWhat, PORTEMU_CONTEXT *pContext) { return new ECall (fWhat, pContext); }
static PORTEMU_CONTEXT *NewPORTEMU_CONTEXT (void) { return new PORTEMU_CONTEXT; }
static PendingIO *NewPendingIO (COMM_OP op, int iPortNum) { return new PendingIO (op, iPortNum); }

static 	PORTEMU_CONTEXT *GetContext (DWORD dwData) {
	PORTEMU_CONTEXT *pContext = gpPORTEMU->pPorts;
	while (pContext && (pContext != (PORTEMU_CONTEXT *)dwData))
		pContext = pContext->pNext;

	return pContext;
}

PORTEMU_CONTEXT::~PORTEMU_CONTEXT (void) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] deleting context 0x%08x\n", this));

	while (pops) {
		PendingIO *pNext = pops->pNext;
		pops->pNext		= NULL;
		pops->iIoResult = ERROR_OPERATION_ABORTED;
		SetEvent (pops->hEvent);

		pops = pNext;
	}

	while (pCalls) {
		ECall *pNext = pCalls->pNext;
		delete pCalls;
		pCalls = pNext;
	}

	while (pbl) {
		BD_BUFFER_LIST *pbl_next = pbl->pNext;
		if (pbl->pb->pFree)
			pbl->pb->pFree (pbl->pb);

		delete pbl;

		pbl = pbl_next;
	}

	if (hRFCOMM) {
		gpPORTEMU->Unlock ();
		RFCOMM_CloseDeviceContext (hRFCOMM);
		gpPORTEMU->Lock ();
	}

}

static void ForgetPort (PORTEMU_CONTEXT *pContext, int iPort) {
	unsigned int mask = ~(1 << iPort);
	pContext->uiOpenRef &= mask;
	pContext->uiReadRef &= mask;
	pContext->uiWriteRef &= mask;
	pContext->hOwnerProc[iPort] = NULL;
}

static ECall *FindCall (void *pCallContext) {
	if (! pCallContext)
		return NULL;

	PORTEMU_CONTEXT *pContext = NULL;

	__try {
		pContext = ((ECall *)pCallContext)->pContext;
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"FindCall excepted!\n"));
	}

	if (! pContext)
		return NULL;

	PORTEMU_CONTEXT *pC = gpPORTEMU->pPorts;
	while (pC && (pC != pContext))
		pC = pC->pNext;

	if (! pC)
		return NULL;

	ECall *pCall = pC->pCalls;
	while (pCall && (pCall != pCallContext))
		pCall = pCall->pNext;

	return pCall;
}

static void DeleteCall (ECall *pCall) {
	ECall *pC = pCall->pContext->pCalls;
	ECall *pP = NULL;

	while (pC && pC != pCall) {
		pP = pC;
		pC = pC->pNext;
	}

	SVSUTIL_ASSERT (pC);

	if (pP)
		pP->pNext = pC->pNext;
	else
		pCall->pContext->pCalls = pC->pNext;

	delete pCall;
}

static void InsertIO (PORTEMU_CONTEXT *pContext, PendingIO *pio) {
	pio->pNext = NULL;
	if (! pContext->pops)
		pContext->pops = pio;
	else {
		PendingIO *pP = pContext->pops;
		while (pP->pNext)
			pP = pP->pNext;
		pP->pNext = pio;
	}
}

static void RemoveIO (PORTEMU_CONTEXT *pContext, PendingIO *pio) {
	if (GetContext ((DWORD)pContext) != pContext)
		return;

	if (pContext->pops == pio)
		pContext->pops = pio->pNext;
	else {
		PendingIO *pP = pContext->pops;
		while (pP && (pP->pNext != pio))
			pP = pP->pNext;

		if (pP)
			pP->pNext = pio->pNext;
	}

	pio->pNext = NULL;
}

static void ReinitNewConnection (PORTEMU_CONTEXT *pContext, HANDLE hConnection) {
	pContext->iRecvQuotaUsed   = pContext->iSendQuotaUsed = 0;

	memset (pContext->adwOccuredEvents, 0, sizeof(pContext->adwOccuredEvents));
	memset (pContext->adwErr, 0, sizeof(pContext->adwErr));
	pContext->dwModemStatusIn  = MS_RLSD_ON;
	pContext->ucv24out         = MSC_DV_BIT | MSC_RTC_BIT | MSC_RTR_BIT;
	pContext->fc               = 0;
	pContext->fSentXon         = FALSE;

	pContext->fSending         = FALSE;

	pContext->hConnection = hConnection;

	SVSUTIL_ASSERT (pContext->sdpCid == 0);
	SVSUTIL_ASSERT (pContext->fSdpQueryOn == 0);
	SVSUTIL_ASSERT (pContext->pbl == NULL);
}

static void ReinitClosedConnection (PORTEMU_CONTEXT *pContext) {
	pContext->fSentXon = FALSE;

	pContext->hConnection    = NULL;
	pContext->iRecvQuotaUsed = 0;
	pContext->iSendQuotaUsed = 0;

	pContext->dwModemStatusIn  = 0;
	pContext->ucv24out         = 0;
	pContext->fc               = 0;

	pContext->credit_fc        = 0;
	pContext->iGaveCredits     = 0;
	pContext->iHaveCredits     = 0;

	pContext->fSending         = FALSE;
	pContext->fHaveClient      = FALSE;
}

static void CheckCommEvent (PORTEMU_CONTEXT *pContext) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"CheckCommEvent : ctx 0x%08x\n", pContext));

	PendingIO *pParent = NULL;
	PendingIO *pIO = pContext->pops;

	while (pIO) {
		SVSUTIL_ASSERT ((pIO->iPortNum >= 0) && (pIO->iPortNum < PORTEMU_OPEN_MAX));

		DWORD dw;

		if ((pIO->op == COMM_WAIT_EVENT) &&
							(dw = pContext->adwEnabledEvents[pIO->iPortNum] & pContext->adwOccuredEvents[pIO->iPortNum])) {
			IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"CheckCommEvent : ctx 0x%08x found and executed waiter (port %d)...\n", pContext, pIO->iPortNum));

			PendingIO *pNext = pIO->pNext;

			if (pParent)
				pParent->pNext = pIO->pNext;
			else
				pContext->pops = pIO->pNext;

			pIO->pNext       = NULL;
			pIO->dwEvent     = dw;

			if (dw & EV_ERR) {
				pIO->dwLineError = pContext->adwErr[pIO->iPortNum];
				pContext->adwErr[pIO->iPortNum] = 0;
			}
			
			pIO->iIoResult = ERROR_SUCCESS;
			SetEvent (pIO->hEvent);

			pContext->adwOccuredEvents[pIO->iPortNum] = 0;

			pIO = pNext;
			continue;
		}

		pParent = pIO;
		pIO = pIO->pNext;
	}
}

static void RegisterCommEvent (PORTEMU_CONTEXT *pContext, DWORD e, DWORD err) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"Event 0x%04x (%s %s %s %s %s %s %s %s %s %s %s %s) %d: registering\n",
		e,
		(e & EV_RXCHAR ? L"EV_RXCHAR" : L""),
		(e & EV_RXFLAG ? L"EV_RXFLAG" : L""),
		(e & EV_TXEMPTY ? L"EV_TXEMPTY" : L""),
		(e & EV_CTS ? L"EV_CTS" : L""),
		(e & EV_DSR ? L"EV_DSR" : L""),
		(e & EV_RLSD ? L"EV_RLSD" : L""),
		(e & EV_BREAK ? L"EV_BREAK" : L""),
		(e & EV_ERR ? L"EV_ERR" : L""),
		(e & EV_RING ? L"EV_RING" : L""),
		(e & EV_RX80FULL ? L"EV_RX80FULL" : L""),
		(e & EV_EVENT1 ? L"EV_EVENT1" : L""),
		(e & EV_EVENT2 ? L"EV_EVENT2" : L""),
		err));

	for (int i = 0 ; i < PORTEMU_OPEN_MAX ; ++i) {
		pContext->adwOccuredEvents[i] = e;
		if (e == EV_ERR)
			pContext->adwErr[i] = err;
	}

	CheckCommEvent (pContext);
}

static void SetCommEventMask (PORTEMU_CONTEXT *pContext, DWORD dwMask, int iPortNum) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"SetCommEventMask Port %d 0x%04x (%s %s %s %s %s %s %s %s %s %s %s %s)\n",
		iPortNum,
		dwMask,
		(dwMask & EV_RXCHAR ? L"EV_RXCHAR" : L""),
		(dwMask & EV_RXFLAG ? L"EV_RXFLAG" : L""),
		(dwMask & EV_TXEMPTY ? L"EV_TXEMPTY" : L""),
		(dwMask & EV_CTS ? L"EV_CTS" : L""),
		(dwMask & EV_DSR ? L"EV_DSR" : L""),
		(dwMask & EV_RLSD ? L"EV_RLSD" : L""),
		(dwMask & EV_BREAK ? L"EV_BREAK" : L""),
		(dwMask & EV_ERR ? L"EV_ERR" : L""),
		(dwMask & EV_RING ? L"EV_RING" : L""),
		(dwMask & EV_RX80FULL ? L"EV_RX80FULL" : L""),
		(dwMask & EV_EVENT1 ? L"EV_EVENT1" : L""),
		(dwMask & EV_EVENT2 ? L"EV_EVENT2" : L"")));

	pContext->adwEnabledEvents[iPortNum] = dwMask;
	pContext->adwOccuredEvents[iPortNum] = 0;

	PendingIO *pParent = NULL;
	PendingIO *pIO = pContext->pops;

	while (pIO) {
		if ((pIO->op == COMM_WAIT_EVENT) && (pIO->iPortNum == iPortNum)) {
			PendingIO *pNext = pIO->pNext;

			pIO->pNext     = NULL;
			pIO->dwEvent   = 0;
			pIO->iIoResult = ERROR_SUCCESS;

			SetEvent (pIO->hEvent);
			
			if (pParent)
				pParent->pNext = pNext;
			else
				pContext->pops = pNext;
			pIO = pNext;
		} else {
			pParent = pIO;
			pIO = pIO->pNext;
		}
	}
 }

static void RFCommDisconnectIn (PORTEMU_CONTEXT *pContext) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] RFCommDisconnect 0x%08x (handle 0x%08x device %04x%08x ch 0x%02x\n",
					pContext, pContext->hConnection, pContext->b.NAP, pContext->b.SAP, pContext->channel));

	HANDLE hConn = pContext->hConnection;

	ECall *pCall = NewCall (RFCOMM_DISC, pContext);
	if (! pCall) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] RFCommDisconnect 0x%08x : Failed to create pending IO. ERROR_NO_SYSTEM_RESOURCES\n", pContext));
		return;
	}

	pCall->pNext = pContext->pCalls;
	pContext->pCalls = pCall;

	if (hConn) {	// Really close it
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] RFCommDisconnect 0x%08x : really closing connection to device\n", pContext));
		HANDLE h = pContext->hRFCOMM;
		RFCOMM_Disconnect_In pCallback = pContext->rfcomm_if.rfcomm_Disconnect_In;

		gpPORTEMU->Unlock ();
		__try {
			pCallback (h, pCall, hConn);
		} __except (1) {
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] RFCommDisconnect : exception in rfcomm_Disconnect_In\n"));
		}
		gpPORTEMU->Lock ();
	}
}

static void CloseConnection (PORTEMU_CONTEXT *pContext, int iRes, BOOL fSendDisconnect) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] CloseConnection 0x%08x handle 0x%08x device %04x%08x ch 0x%02x send disc %d\n",
					pContext, pContext->hConnection, pContext->b.NAP, pContext->b.SAP, pContext->channel, fSendDisconnect));

	ReinitClosedConnection (pContext);

	RegisterCommEvent (pContext, EV_RLSD, 0);

	PendingIO *pParent = NULL;
	PendingIO *pIO = pContext->pops;

	while (pIO) {
		if ((pIO->op == COMM_READ) || (pIO->op == COMM_WRITE) || (pIO->op == COMM_OPEN) || (pIO->op == COMM_RPN) || (pIO->op == COMM_DISC)) {
			PendingIO *pNext = pIO->pNext;

			pIO->pNext     = NULL;
			pIO->iIoResult = iRes;
			SetEvent (pIO->hEvent);

			if (pParent)
				pParent->pNext = pNext;
			else
				pContext->pops = pNext;

			pIO = pNext;
		} else {
			pParent = pIO;
			pIO = pIO->pNext;
		}
	}

	while (pContext->pbl) {
		if (pContext->pbl->pb->pFree)
			pContext->pbl->pb->pFree (pContext->pbl->pb);
		BD_BUFFER_LIST *pNext = pContext->pbl->pNext;
		delete pContext->pbl;
		pContext->pbl = pNext;
	}

	while (pContext->pCalls) {
		ECall *pNext = pContext->pCalls->pNext;
		delete pContext->pCalls;
		pContext->pCalls = pNext;
	}

	if (fSendDisconnect)
		RFCommDisconnectIn (pContext);
}

static int GetQuota (PORTEMU_CONTEXT *pContext) {
	if (pContext->credit_fc && (pContext->iRecvQuota < (pContext->iMTU * 2 * PORTEMU_CREDITS_LOWEST)))
		return pContext->iMTU * 2 * PORTEMU_CREDITS_LOWEST;

	return pContext->iRecvQuota;
}

static int GiveCredits (PORTEMU_CONTEXT *pContext) {
	if ((! pContext->credit_fc) || (pContext->iGaveCredits > PORTEMU_CREDITS_LOW))
		return 0;

	int nCredits = (GetQuota (pContext) - pContext->iRecvQuotaUsed) / pContext->iMTU - pContext->iGaveCredits;

	return nCredits > 0 ? nCredits : 0;
}

static void SendCredits (PORTEMU_CONTEXT *pContext) {
	SVSUTIL_ASSERT (pContext->credit_fc);

	if (pContext->iGaveCredits > PORTEMU_CREDITS_LOWEST)
		return;

	int nCredits = (GetQuota (pContext) - pContext->iRecvQuotaUsed) / pContext->iMTU - pContext->iGaveCredits;

	if (nCredits <= 0)
		return;

	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] SendCredits 0x%08x : sending %d credits\n", pContext, nCredits));

	BD_BUFFER *pBuffer = BufferAlloc (pContext->iDeviceHead + pContext->iDeviceTrail);

	if (! pBuffer) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] SendCredits 0x%08x :  <out of memory>\n", pContext));
		return;
	}

	pBuffer->cStart = pContext->iDeviceHead;
	pBuffer->cEnd   = pBuffer->cSize - pContext->iDeviceTrail;

	SVSUTIL_ASSERT (BufferTotal (pBuffer) == 0);

	pContext->iGaveCredits += nCredits;

	HANDLE h = pContext->hRFCOMM;
	HANDLE hConn = pContext->hConnection;
	RFCOMM_DataDown_In pCallback = pContext->rfcomm_if.rfcomm_DataDown_In;

	gpPORTEMU->Unlock ();
	__try {
		pCallback (h, NULL, hConn, pBuffer, nCredits);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] SendCredits : exception in rfcomm_DataDown_In\n"));
	}

	gpPORTEMU->Lock ();
}

static int SendData (PORTEMU_CONTEXT *pContext) {
	if (pContext->fSending)		// Guard against packets coming out of order
		return ERROR_IO_PENDING;

	for ( ; ; ) {
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] SendData to 0x%08x (%04x%08x ch 0x%02)\n",
										pContext, pContext->b.NAP, pContext->b.SAP, pContext->channel));

		// Data queued, but can't send just yet
		if ((pContext->iSendQuotaUsed >= pContext->iSendQuota) || pContext->fc_aggregate ||
			((! pContext->credit_fc) && pContext->fc) || (pContext->credit_fc && (pContext->iHaveCredits <= 0)) ||
			(! pContext->hConnection)) {
			IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] SendData 0x%08x : ERROR_IO_PENDING\n", pContext));
			return ERROR_IO_PENDING;
		}

		//	Compute buffer
		PendingIO *pIO = pContext->pops;
		int	cTotalBytes = 0;
		int cGiveCredits = GiveCredits (pContext);

		while (pIO) {
			if (pIO->op == COMM_WRITE) {
				if ((cTotalBytes + (pIO->cbuffer - pIO->cbuffer_used)) >= (pContext->iMTU - (cGiveCredits != 0))) {
					cTotalBytes = pContext->iMTU - (cGiveCredits != 0);
					break;
				} else
					cTotalBytes += (pIO->cbuffer - pIO->cbuffer_used);
			}
			pIO = pIO->pNext;
		}

		if (cTotalBytes == 0) {
			IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] SendData 0x%08x : ERROR_SUCCESS <nothing left>\n", pContext));
			RegisterCommEvent(pContext, EV_TXEMPTY, 0);
			return ERROR_SUCCESS;
		}

		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] SendData to 0x%08x : %d bytes, %d credits\n", pContext, cTotalBytes, cGiveCredits));

		BD_BUFFER *pBuffer = BufferAlloc (cTotalBytes + pContext->iDeviceHead + pContext->iDeviceTrail);

		if (! pBuffer) {
			IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] SendData 0x%08x : ERROR_IO_PENDING <out of memory>\n", pContext));
			return ERROR_IO_PENDING;
		}

		ECall *pCall = NewCall (RFCOMM_WRITE, pContext);

		if (! pCall) {
			if (pBuffer->pFree)
				pBuffer->pFree (pBuffer);
			IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] SendData 0x%08x : ERROR_IO_PENDING <out of memory: call>\n", pContext));
			return ERROR_IO_PENDING;
		}

		pCall->pNext     = pContext->pCalls;
		pContext->pCalls = pCall;
		pCall->cBytes    = cTotalBytes;

		pBuffer->cEnd = pBuffer->cSize - pContext->iDeviceTrail;
		pBuffer->cStart = pContext->iDeviceHead;

		SVSUTIL_ASSERT (BufferTotal (pBuffer) == cTotalBytes);

		// Accumulate buffer
		pIO = pContext->pops;
		PendingIO *pParent = NULL;

		int cHaveBytes = 0;

		while (pIO && (cHaveBytes < cTotalBytes)) {
			if (pIO->op == COMM_WRITE) {
				int cThisChunk = cTotalBytes - cHaveBytes;
				if (cThisChunk > (pIO->cbuffer - pIO->cbuffer_used))
					cThisChunk = pIO->cbuffer - pIO->cbuffer_used;

				SVSUTIL_ASSERT (pIO->dwPerms);
				DWORD dwCurrentPerms = SetProcPermissions (pIO->dwPerms);
				BOOL bkm = SetKMode (TRUE);
				memcpy (pBuffer->pBuffer + pContext->iDeviceHead + cHaveBytes, pIO->buffer + pIO->cbuffer_used, cThisChunk);
				SetKMode (bkm);
				SetProcPermissions (dwCurrentPerms);

				cHaveBytes += cThisChunk;
				pIO->cbuffer_used += cThisChunk;

				if (pIO->cbuffer_used == pIO->cbuffer) {	// Retire the buffer
					PendingIO *pNext = pIO->pNext;
					pIO->iIoResult = ERROR_SUCCESS;
					pIO->pNext = NULL;
					SetEvent (pIO->hEvent);

					if (pParent)
						pParent->pNext = pNext;
					else
						pContext->pops = pNext;

					pIO = pNext;
					continue;
				}

				SVSUTIL_ASSERT (cHaveBytes == cTotalBytes);
				break;
			}
			pParent = pIO;
			pIO = pIO->pNext;
		}

		SVSUTIL_ASSERT (cHaveBytes == cTotalBytes);
		pContext->iSendQuotaUsed += cTotalBytes;

		if (pContext->credit_fc) {
			--pContext->iHaveCredits;
			pContext->iGaveCredits += cGiveCredits;
		}

		SVSUTIL_ASSERT (pContext->iHaveCredits >= 0);

		//	Send the buffer down...

		HANDLE h = pContext->hRFCOMM;
		HANDLE hConn = pContext->hConnection;
		RFCOMM_DataDown_In pCallback = pContext->rfcomm_if.rfcomm_DataDown_In;

		pContext->fSending = TRUE;

		gpPORTEMU->Unlock ();
		int iRes = ERROR_INTERNAL_ERROR;
		__try {
			iRes = pCallback (h, pCall, hConn, pBuffer, cGiveCredits);
		} __except (1) {
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] SendData : exception in rfcomm_DataDown_In\n"));
		}

		gpPORTEMU->Lock ();

		if (! (gpPORTEMU->fInitialized &&
			(pContext == GetContext ((DWORD)pContext)) &&
			(pContext->hConnection == hConn))) {
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] Going through (connection?) shutdown in the middle of send...\n"));
			return ERROR_OPERATION_ABORTED;
		}

		pContext->fSending = FALSE;

		if (iRes != ERROR_SUCCESS) {	// Close the connection
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] SendData : rfcomm_DataDown_In returned %d\n", iRes));

			RegisterCommEvent (pContext, EV_ERR, CE_FRAME);
			CloseConnection (pContext, iRes, TRUE);
			return iRes;
		}
	}
}

static int SendMSC (PORTEMU_CONTEXT *pContext) {
	HANDLE h = pContext->hRFCOMM;
	HANDLE hConnection = pContext->hConnection;
	RFCOMM_MSC_In pCallback = pContext->rfcomm_if.rfcomm_MSC_In;
	unsigned char v24 = EA_BIT | pContext->ucv24out;

	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"SendMSC : (%s %s %s %s %s)\n",
					v24 & MSC_DV_BIT ? L"MS_RLSD_ON" : L"",
					v24 & MSC_IC_BIT ? L"MS_RING_ON" : L"",
					v24 & MSC_RTR_BIT ? L"MS_CTS_ON" : L"",
					v24 & MSC_RTC_BIT ? L"MS_DSR_ON" : L"",
					v24 & MSC_FC_BIT ? L"Flow_Off" : L""));

	gpPORTEMU->Unlock ();

	__try {
		pCallback (h, NULL, hConnection, v24, 0xff);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] SendMSC Exception in rfcomm_MSC_In\n"));
	}
	gpPORTEMU->Lock ();

	return ERROR_SUCCESS;
}

static DWORD WINAPI AuthenticateThread (LPVOID pUserContext) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] AuthenticateThread 0x%08x\n", pUserContext));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] AuthenticateThread 0x%08x : not initialized!\n", pUserContext));
		return 0;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] AuthenticateThread 0x%08x: not initialized!\n", pUserContext));
		gpPORTEMU->Unlock ();
		return 0;
	}

	PORTEMU_CONTEXT *pContext = GetContext ((DWORD)pUserContext);

	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] AuthenticateThread 0x%08x : context not found\n", pUserContext));
		gpPORTEMU->Unlock ();

		return ERROR_NOT_FOUND;
	}

	int fAuthenticate  = pContext->freq_auth;
	int fEncrypt       = pContext->freq_encrypt;
	BT_ADDR bt         = SET_NAP_SAP (pContext->b.NAP, pContext->b.SAP);
	HANDLE hConnection = pContext->hConnection;

	SVSUTIL_ASSERT (fAuthenticate || fEncrypt);
	SVSUTIL_ASSERT (hConnection);

	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] AuthenticateThread 0x%08x : doing authentication/encryption\n", pUserContext));

	gpPORTEMU->Unlock ();

	int iRes = ERROR_SUCCESS;

	if (fAuthenticate)
		iRes = BthAuthenticate (&bt);

	if ((iRes == ERROR_SUCCESS) && fEncrypt)
		iRes = BthSetEncryption (&bt, TRUE);

	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] AuthenticateThread 0x%08x : RE_ENTERING THE LOCK\n", pUserContext));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] AuthenticateThread 0x%08x : not initialized!\n", pUserContext));
		return 0;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] AuthenticateThread 0x%08x: not initialized!\n", pUserContext));
		gpPORTEMU->Unlock ();
		return 0;
	}

	pContext = GetContext ((DWORD)pUserContext);

	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] AuthenticateThread 0x%08x : context not found\n", pUserContext));
		gpPORTEMU->Unlock ();

		return ERROR_NOT_FOUND;
	}

	int fAllow = FALSE;

	if ((iRes == ERROR_SUCCESS) && pContext->fLocal && pContext->fLocalOpen && pContext->hConnection) {
		fAllow = TRUE;
		RegisterCommEvent (pContext, EV_RLSD, 0);
	} else
		ReinitClosedConnection (pContext);

	HANDLE hRFCOMM = pContext->hRFCOMM;
	RFCOMM_ConnectResponse_In pCallback = pContext->rfcomm_if.rfcomm_ConnectResponse_In;

	gpPORTEMU->Unlock ();

	__try {
		iRes = pCallback (hRFCOMM, NULL, hConnection, fAllow);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] AuthenticateThread 0x%08x : exception in rfcomm_ConnectResponse_In\n", pUserContext));
	}

	if (fAllow && (iRes == ERROR_SUCCESS) && gpPORTEMU) {
		gpPORTEMU->Lock ();
		if ((pContext == GetContext ((DWORD)pUserContext)) && pContext->hConnection) {
			pContext->ucv24out &= ~MSC_FC_BIT;
			SendMSC (pContext);
		}

		gpPORTEMU->Unlock ();
	}

	return iRes;
	
}

static void PORTEMU_ProcessExited (HANDLE hProc) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] PORTEMU_ProcessExited 0x%08x\n", hProc));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] PORTEMU_ProcessExited 0x%08x : not initialized!\n", hProc));
		return;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] PORTEMU_ProcessExited 0x%08x: not initialized!\n", hProc));
		gpPORTEMU->Unlock ();
		return;
	}

	while (gpPORTEMU->fInitialized) {
		PORTEMU_CONTEXT *pContext = gpPORTEMU->pPorts;
		int iPort = 0;
		while (pContext) {
			for (iPort = 0 ; iPort < PORTEMU_OPEN_MAX ; ++iPort) {
				if (pContext->hOwnerProc[iPort] == hProc)
					break;
			}

			if (iPort < PORTEMU_OPEN_MAX)
				break;

			pContext = pContext->pNext;
		}

		if (! pContext)
			break;

		SVSUTIL_ASSERT (iPort < PORTEMU_OPEN_MAX);

		pContext->hOwnerProc[iPort] = NULL;

		gpPORTEMU->AddRef ();
		gpPORTEMU->Unlock ();
		COM_Close (((DWORD)pContext) | iPort);
		gpPORTEMU->Lock ();
		gpPORTEMU->DelRef ();
	}

	gpPORTEMU->Unlock ();
}

static DWORD WINAPI StackDisconnect (LPVOID pArg) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"Disconnect stack\n"));
	rfcomm_CloseDriverInstance ();
	return ERROR_SUCCESS;
}

static DWORD WINAPI StackDown (LPVOID pArg) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"stack down\n"));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] StackDown 0x%08x : not initialized!\n", pArg));
		return 0;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] StackDown 0x%08x: not initialized!\n", pArg));
		gpPORTEMU->Unlock ();
		return 0;
	}

	PORTEMU_CONTEXT *pContext = GetContext ((DWORD)pArg);

	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] StackDown 0x%08x : context not found\n", pArg));
		gpPORTEMU->Unlock ();

		return ERROR_NOT_FOUND;
	}

	if (pContext->hConnection) {
		pContext->hConnection  = NULL;
		pContext->fc_aggregate = 0;

		CloseConnection (pContext, ERROR_GRACEFUL_DISCONNECT, TRUE);
	}
	gpPORTEMU->Unlock ();

	return ERROR_SUCCESS;
}

static int rfcommsdp_disconnect_out(void *pCallContext, unsigned long status) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"+rfcommsdp_disconnect_out\n"));
	return ERROR_SUCCESS;
}

static void rfcommsdp_disconnect (void *pCallContext, int iErr) {
	SVSUTIL_ASSERT(gpPORTEMU && gpPORTEMU->IsLocked());

	ECall *pCall = FindCall (pCallContext);

	if (! pCall) {
		IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] rfcommsdp_disconnect 0x%08x : context does not exist\n", pCallContext));
		return;
	}

	PORTEMU_CONTEXT *pContext = pCall->pContext;

	DeleteCall (pCall);

	PendingIO *pParent = NULL;
	PendingIO *pIO = pContext->pops;

	while (pIO) {
		if (pIO->op == COMM_OPEN) {
			PendingIO *pNext = pIO->pNext;

			pIO->pNext     = NULL;
			pIO->dwEvent   = 0;
			pIO->iIoResult = iErr;

			SetEvent (pIO->hEvent);
			
			if (pParent)
				pParent->pNext = pNext;
			else
				pContext->pops = pNext;
			break;
		}

		pParent = pIO;
		pIO = pIO->pNext;
	}

	SVSUTIL_ASSERT (pContext->pops == NULL);

	if (pContext->sdpCid) {
		HANDLE h = gpPORTEMU->hSDP;
		SDP_Disconnect_In  pCallback = gpPORTEMU->sdp_if.sdp_Disconnect_In;
		unsigned short cid = pContext->sdpCid;

		pContext->sdpCid = 0;

		SVSUTIL_ASSERT(h && pCallback && cid);

		gpPORTEMU->AddRef ();
		gpPORTEMU->Unlock ();

		__try {
			pCallback (h,NULL,cid);
		} __except (1) {
			IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] : exception in sdp_disconnect_in!\n"));
		}

		gpPORTEMU->Lock ();
		gpPORTEMU->DelRef ();
	}
}

static int rfcommsdp_connect_out (void *pCallContext, unsigned long status, unsigned short cid) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"+rfcommsdp_connect_out \n"));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] rfcommsdp_connect_out 0x%08x : not initialized!\n", pCallContext));
		return ERROR_SUCCESS;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] rfcommsdp_connect_out 0x%08x: not initialized!\n", pCallContext));
		gpPORTEMU->Unlock ();
		return ERROR_SUCCESS;
	}

	ECall *pCall = FindCall (pCallContext);
	if (! pCall) {
		IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] rfcommsdp_connect_out 0x%08x : context does not exist\n", pCallContext));
		gpPORTEMU->Unlock ();
		return ERROR_SUCCESS;	
	}

	PORTEMU_CONTEXT *pContext = pCall->pContext;

	SVSUTIL_ASSERT (pContext->channel == 0);
	SVSUTIL_ASSERT (pContext->sdpCid == 0);

	SdpQueryUuid                  uuid[2];
	SdpAttributeRange             attribRange;
	
	int iErr = ERROR_PORT_UNREACHABLE;

	if (status == ERROR_SUCCESS) {
		SVSUTIL_ASSERT(cid);
		pContext->sdpCid = cid;

		// Call ServiceAttribSearch to get protocol headers.
		SDP_ServiceAttributeSearch_In pCallback = gpPORTEMU->sdp_if.sdp_ServiceAttributeSearch_In;
		HANDLE h = gpPORTEMU->hSDP;

		SVSUTIL_ASSERT(h && pCallback);

		uuid[0].uuidType = SDP_ST_UUID128;
		memcpy(&uuid[0].u.uuid128, &pContext->uuidService, sizeof(GUID));
		memset(&uuid[1],0,sizeof(uuid[1]));

		attribRange.minAttribute = attribRange.maxAttribute = SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST;

		gpPORTEMU->AddRef ();
		gpPORTEMU->Unlock ();

		iErr = ERROR_INTERNAL_ERROR;

		__try {
			iErr = pCallback (h, pCallContext, cid, uuid, &attribRange,1);
		} __except (1) {
			IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] : exception in sdp_ServiceAttributeSearch_In!\n"));
		}

		gpPORTEMU->Lock ();
		gpPORTEMU->DelRef ();

		IFDBG(DebugOut(DEBUG_ERROR,L"rfcommsdp_connect_out: returns 0x%08x\r\n",iErr));
	}

	SVSUTIL_ASSERT(gpPORTEMU->IsLocked());

	if (iErr != ERROR_SUCCESS)
		rfcommsdp_disconnect (pCallContext, iErr);

	gpPORTEMU->Unlock ();
	return ERROR_SUCCESS;
}

static int rfcommsdp_service_attribute_search_out(void *pCallContext, unsigned long status, unsigned char *pOutBuf, unsigned long cOutBuf) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"+rfcommsdp_service_attribute_search_out\n"));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] rfcommsdp_service_attribute_search_out 0x%08x : not initialized!\n", pCallContext));
		return 0;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] rfcommsdp_service_attribute_search_out 0x%08x: not initialized!\n", pCallContext));
		gpPORTEMU->Unlock ();
		return 0;
	}

	ECall *pCall = FindCall (pCallContext);
	if (! pCall) {
		IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] rfcommsdp_service_attribute_search_out 0x%08x : context does not exist\n", pCallContext));
		gpPORTEMU->Unlock ();
		return ERROR_SUCCESS;	
	}

	PORTEMU_CONTEXT *pContext = pCall->pContext;

	SVSUTIL_ASSERT (pContext->channel == 0);
	SVSUTIL_ASSERT (pContext->sdpCid != 0);

	if ((status == ERROR_SUCCESS) && ((! pOutBuf) || (cOutBuf == 0)))
		status = ERROR_INVALID_DATA;

	unsigned long ch = 0;
	if (status == ERROR_SUCCESS)
		status = GetRfcommCidFromResponse (pOutBuf, cOutBuf, &ch);

	if (status == ERROR_SUCCESS)
		pContext->channel = ch;

	IFDBG(DebugOut(DEBUG_RFCOMM_TRACE,L"rfcommsdp_service_attribute_search_out returns %d channel=%d\r\n", status, pContext->channel));

	if (pOutBuf)
		g_funcFree(pOutBuf,g_pvAllocData);

	rfcommsdp_disconnect(pCall, status);

	gpPORTEMU->Unlock();

	return ERROR_SUCCESS;
}

static int portemu_connect_request_ind	(void *pUserContext, HANDLE hConnection, BD_ADDR *pba, unsigned char channel) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_connect_request_ind 0x%08x\n", pUserContext));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_connect_request_ind 0x%08x : not initialized!\n", pUserContext));
		return 0;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_connect_request_ind 0x%08x: not initialized!\n", pUserContext));
		gpPORTEMU->Unlock ();
		return 0;
	}

	PORTEMU_CONTEXT *pContext = GetContext ((DWORD)pUserContext);

	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_connect_request_ind 0x%08x : context not found\n", pUserContext));
		gpPORTEMU->Unlock ();

		return ERROR_NOT_FOUND;
	}

	int fAllow = FALSE;

	HANDLE hRFCOMM = pContext->hRFCOMM;
	RFCOMM_ConnectResponse_In pCallback = pContext->rfcomm_if.rfcomm_ConnectResponse_In;

	if ((pContext->channel == channel) && pContext->fLocal && pContext->fLocalOpen && !pContext->fHaveClient) {
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"Got active connection from %04x%08x (%d)\n", pba->NAP, pba->SAP, channel));

		if (pContext->freq_auth || pContext->freq_encrypt) {
			IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"Requesting authentication for connection from %04x%08x (%d)\n", pba->NAP, pba->SAP, channel));
			SVSCookie cookie = btutil_ScheduleEvent (AuthenticateThread, pContext);
			if (cookie) {
				pContext->fHaveClient = TRUE;
				pContext->b = *pba;
				ReinitNewConnection (pContext, hConnection);

				gpPORTEMU->Unlock ();
				return ERROR_SUCCESS;
			}

			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_connect_request_ind 0x%08x : could not create authentication thread\n", pUserContext));
		} else {
			fAllow = TRUE;
			pContext->fHaveClient = TRUE;
			pContext->b = *pba;

			ReinitNewConnection (pContext, hConnection);

			RegisterCommEvent (pContext, EV_RLSD, 0);
		}
	}

	gpPORTEMU->Unlock ();
		
	int iRes = ERROR_INTERNAL_ERROR;
	__try {
		iRes = pCallback (hRFCOMM, NULL, hConnection, fAllow);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_connect_request_ind 0x%08x : exception in rfcomm_ConnectResponse_In\n", pUserContext));
	}

	if (fAllow && (iRes == ERROR_SUCCESS) && gpPORTEMU) {
		gpPORTEMU->Lock ();
		if ((pContext == GetContext ((DWORD)pUserContext)) && pContext->hConnection) {
			pContext->ucv24out &= ~MSC_FC_BIT;
			SendMSC (pContext);
		}

		gpPORTEMU->Unlock ();
	}

	return iRes;
}

static int portemu_data_up_ind (void *pUserContext, HANDLE hConnection, BD_BUFFER *pb, int cAdditionalCredits) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_data_up_ind 0x%08x, new credits = %d\n", pUserContext, cAdditionalCredits));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_data_up_ind 0x%08x : not initialized!\n", pUserContext));
		return 0;
	}

	gpPORTEMU->Lock ();

	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_data_up_ind 0x%08x: not initialized!\n", pUserContext));

		if (pb->pFree)
			pb->pFree (pb);

		gpPORTEMU->Unlock ();
		return 0;
	}

	PORTEMU_CONTEXT *pContext = GetContext ((DWORD)pUserContext);

	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_data_up_ind 0x%08x : context not found\n", pUserContext));

		if (pb->pFree)
			pb->pFree (pb);

		gpPORTEMU->Unlock ();

		return ERROR_NOT_FOUND;
	}

	//	First see if we have consumers
	PendingIO *pParent = NULL;
	PendingIO *pIO = pContext->pops;

	int cHaveData = BufferTotal (pb);

	if ((cHaveData > 0) && pContext->credit_fc)
		--pContext->iGaveCredits;

	while (pIO && (cHaveData > 0)) {
		if (pIO->op == COMM_READ) {
			SVSUTIL_ASSERT (! pContext->pbl);	// else why is the request here???

			int cBytes = cHaveData > (pIO->cbuffer - pIO->cbuffer_used) ? (pIO->cbuffer - pIO->cbuffer_used) : cHaveData;
			DWORD dwCurrentPerms = SetProcPermissions (pIO->dwPerms);
			BOOL bkm = SetKMode (TRUE);
			int iRes = BufferGetChunk (pb, cBytes, pIO->buffer + pIO->cbuffer_used);
			SetKMode (bkm);
			SetProcPermissions (dwCurrentPerms);
			SVSUTIL_ASSERT (iRes);

			cHaveData -= cBytes;
			SVSUTIL_ASSERT (cHaveData == BufferTotal (pb));

			pIO->cbuffer_used += cBytes;

			IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_data_up_ind 0x%08x : sat. read (%d bytes)\n", pUserContext, cBytes));

			if (pIO->cbuffer_used == pIO->cbuffer) {
				IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_data_up_ind 0x%08x : retired read event\n", pUserContext, cBytes));
				PendingIO *pNext = pIO->pNext;

				pIO->pNext     = NULL;
				pIO->iIoResult = ERROR_SUCCESS;
				SetEvent (pIO->hEvent);

				if (pParent)
					pParent->pNext = pNext;
				else
					pContext->pops = pNext;

				pIO = pNext;
			} else {
				SVSUTIL_ASSERT (cHaveData == 0);
				break;
			}
		} else {
			pParent = pIO;
			pIO = pIO->pNext;
		}
	}

	SVSUTIL_ASSERT (cHaveData == BufferTotal (pb));

	if ((cHaveData + pContext->iRecvQuotaUsed) > GetQuota(pContext)) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] OUT OF quota receiving the buffer!\n"));
		cHaveData = 0;

		RegisterCommEvent (pContext, EV_ERR, CE_OVERRUN);
	}

	if (cHaveData > 0) {	// Queue the remainder
		if (pb->fMustCopy)
			pb = BufferCompress (pb);	// Note: this is somewhat hackish, but it does not leak. Mental exercise: why :-)?

		BD_BUFFER_LIST *pbl = pb ? new BD_BUFFER_LIST : NULL;

		if (pbl) {	// decrement quota and store the data
			IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_data_up_ind 0x%08x : queueing %d bytes\n", pUserContext, cHaveData));

			pbl->pNext = NULL;
			pbl->pb = pb;
			if (pContext->pbl) {
				BD_BUFFER_LIST *pP = pContext->pbl;
				while (pP->pNext)
					pP = pP->pNext;

				pP->pNext = pbl;
			} else
				pContext->pbl = pbl;

			pContext->iRecvQuotaUsed += cHaveData;

			RegisterCommEvent (pContext, EV_RXCHAR, 0);
		} else {
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] OUTOFMEMORY queueing the buffer!\n"));
			cHaveData = 0;
		}
	}

	if (cHaveData == 0) {	// Buffer all read or of no use anyway...
		if (pb && pb->pFree)
			pb->pFree (pb);
	}

	if ((! pContext->credit_fc) && (! pContext->fSentXon) && (pContext->iRecvQuotaUsed > pContext->dcb.XonLim)) {
		pContext->fSentXon = TRUE;
		pContext->ucv24out |= MSC_FC_BIT;
		SendMSC (pContext);
	} else if (pContext->credit_fc) {
		pContext->iHaveCredits += cAdditionalCredits;
		if (pContext->iHaveCredits - cAdditionalCredits <= 0)
			SendData (pContext);

		if (gpPORTEMU->fInitialized && (pContext == GetContext ((DWORD)pUserContext)))
			SendCredits (pContext);
	}

	gpPORTEMU->Unlock ();

	return ERROR_SUCCESS;
}

static int portemu_disconnect_ind (void *pUserContext, HANDLE hConnection) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_disconnect_ind 0x%08x\n", pUserContext));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_disconnect_ind 0x%08x : not initialized!\n", pUserContext));
		return 0;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_disconnect_ind 0x%08x: not initialized!\n", pUserContext));
		gpPORTEMU->Unlock ();
		return 0;
	}

	PORTEMU_CONTEXT *pContext = GetContext ((DWORD)pUserContext);

	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_disconnect_ind 0x%08x : context not found\n", pUserContext));
		gpPORTEMU->Unlock ();

		return ERROR_NOT_FOUND;
	}

	pContext->hConnection = NULL;
	CloseConnection (pContext, ERROR_GRACEFUL_DISCONNECT, FALSE);

	gpPORTEMU->Unlock ();

	return ERROR_SUCCESS;
}

static int portemu_pnreq_ind (void *pUserContext, HANDLE hConnection, unsigned char priority, unsigned short n1, int use_credit_fc, int initial_credits) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_pnreq_ind 0x%08x hconn = 0x%08x, pri = %d, mtu = %d, credit = %x, initial credtis = %d\n", pUserContext, hConnection, priority, n1, use_credit_fc, initial_credits));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_pnreq_ind 0x%08x : not initialized!\n", pUserContext));
		return 0;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_pnreq_ind 0x%08x: not initialized!\n", pUserContext));
		gpPORTEMU->Unlock ();
		return 0;
	}

	PORTEMU_CONTEXT *pContext = GetContext ((DWORD)pUserContext);

	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_pnreq_ind 0x%08x : context not found\n", pUserContext));
		gpPORTEMU->Unlock ();

		return ERROR_NOT_FOUND;
	}

	if (! pContext->hConnection)
		pContext->hConnection = hConnection;

	if (n1 > pContext->iMaxMTU)
		n1 = pContext->iMaxMTU;
	if (n1 < pContext->iMinMTU)
		n1 = pContext->iMinMTU;

	pContext->iMTU = n1;

	if (use_credit_fc == RFCOMM_PN_CREDIT_IN) {
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_pnreq_ind 0x%08x CREDIT-based Flow ON\n", pUserContext));

		pContext->credit_fc     = TRUE;
		pContext->iHaveCredits  = initial_credits;
		pContext->iGaveCredits  = RFCOMM_PN_CREDIT_MAX;
	} else {
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_pnreq_ind 0x%08x CREDIT-based Flow OFF\n", pUserContext));

		pContext->credit_fc     = FALSE;
		pContext->iHaveCredits  = 0;
		pContext->iGaveCredits  = 0;
	}

	HANDLE h = pContext->hRFCOMM;
	RFCOMM_PNRSP_In pCallback = pContext->rfcomm_if.rfcomm_PNRSP_In;

	gpPORTEMU->Unlock ();

	int iRes = ERROR_INTERNAL_ERROR;
	__try {
		iRes = pCallback (h, NULL, hConnection, priority, n1,
									(use_credit_fc == RFCOMM_PN_CREDIT_IN) ? RFCOMM_PN_CREDIT_OUT : 0,
									(use_credit_fc == RFCOMM_PN_CREDIT_IN) ? RFCOMM_PN_CREDIT_MAX : 0);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_pnreq_ind Exception in rfcomm_PNRSP_In\n"));
	}

#if defined (DEBUG) || defined (_DEBUG) || defined (RETAILLOG)
	if (iRes != ERROR_SUCCESS)
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_pnreq_ind rfcomm_PNRSP_In returns %d\n", iRes));
#endif

	return ERROR_SUCCESS;
}

static int portemu_rpnreq_ind (void *pUserContext, HANDLE hConnection, unsigned short mask, int baud, int data, int stop, int parity, int parity_type, unsigned char flow, unsigned char xon, unsigned char xoff) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_rpnreq_ind 0x%08x hconn = 0x%08x\n", pUserContext, hConnection));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_rpnreq_ind 0x%08x : not initialized!\n", pUserContext));
		return 0;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_rpnreq_ind 0x%08x: not initialized!\n", pUserContext));
		gpPORTEMU->Unlock ();
		return 0;
	}

	PORTEMU_CONTEXT *pContext = GetContext ((DWORD)pUserContext);

	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_rpnreq_ind 0x%08x : context not found\n", pUserContext));
		gpPORTEMU->Unlock ();

		return ERROR_NOT_FOUND;
	}

	if (mask & RFCOMM_RPN_HAVE_BAUD)
		pContext->dcb.BaudRate = baud;

	if (mask & RFCOMM_RPN_HAVE_DATA)
		pContext->dcb.ByteSize = data;

	if (mask & RFCOMM_RPN_HAVE_STOP)
		pContext->dcb.StopBits = stop;

	if (mask & RFCOMM_RPN_HAVE_PARITY)
		pContext->dcb.fParity = parity;

	if (mask & RFCOMM_RPN_HAVE_PT)
		pContext->dcb.Parity = parity_type;

	if (mask & RFCOMM_RPN_HAVE_XOFF)
		pContext->dcb.XoffChar = xoff;

	if (mask & RFCOMM_RPN_HAVE_XON)
		pContext->dcb.XonChar = xon;

	if (mask & RFCOMM_RPN_HAVE_XON_IN)
		pContext->dcb.fInX = flow & RFCOMM_RPN_XON_IN ? TRUE : FALSE;

	if (mask & RFCOMM_RPN_HAVE_XON_OUT)
		pContext->dcb.fOutX = flow & RFCOMM_RPN_XON_OUT ? TRUE : FALSE;

	if (! mask) {
		mask = RFCOMM_RPN_HAVE_BAUD | RFCOMM_RPN_HAVE_DATA | RFCOMM_RPN_HAVE_STOP | 
			RFCOMM_RPN_HAVE_PARITY | RFCOMM_RPN_HAVE_PT;
		baud = pContext->dcb.BaudRate;
		data = pContext->dcb.ByteSize;
		stop = pContext->dcb.StopBits;
		parity = pContext->dcb.fParity;
		parity_type = pContext->dcb.Parity;
		xoff = 0;
		xon = 0;
		flow = 0;
	} else
		RegisterCommEvent (pContext, EV_EVENT1, 0);

	HANDLE h = pContext->hRFCOMM;
	RFCOMM_RPNRSP_In pCallback = pContext->rfcomm_if.rfcomm_RPNRSP_In;

	gpPORTEMU->Unlock ();

	int iRes = ERROR_INTERNAL_ERROR;
	__try {
		iRes = pCallback (h, NULL, hConnection, mask, baud, data, stop, parity, parity_type, flow, xon, xoff);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_rpnreq_ind Exception in rfcomm_PNRSP_In\n"));
	}

#if defined (DEBUG) || defined (_DEBUG) || defined (RETAILLOG)
	if (iRes != ERROR_SUCCESS)
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_rpnreq_ind rfcomm_PNRSP_In returns %d\n", iRes));
#endif

	return ERROR_SUCCESS;
}

static int portemu_fc_ind (void *pUserContext, HANDLE hConnect, unsigned char fcOn) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_fc_ind 0x%08x %s\n", pUserContext, fcOn ? L"on" : L"off"));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_fc_ind 0x%08x : not initialized!\n", pUserContext));
		return 0;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_fc_ind 0x%08x: not initialized!\n", pUserContext));
		gpPORTEMU->Unlock ();
		return 0;
	}

	PORTEMU_CONTEXT *pContext = GetContext ((DWORD)pUserContext);

	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_fc_ind 0x%08x : context not found\n", pUserContext));
		gpPORTEMU->Unlock ();

		return ERROR_NOT_FOUND;
	}

	if (fcOn) {
		pContext->fc_aggregate = FALSE;
		SendData (pContext);
	} else
		pContext->fc_aggregate = TRUE;

	gpPORTEMU->Unlock ();

	return ERROR_SUCCESS;
}

static int portemu_msc_ind (void *pUserContext, HANDLE hConnect, unsigned char v24, unsigned char bs) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_msc_ind 0x%08x 0x%02x 0x%02x\n", pUserContext, v24, bs));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_msc_ind 0x%08x : not initialized!\n", pUserContext));
		return 0;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_msc_ind 0x%08x: not initialized!\n", pUserContext));
		gpPORTEMU->Unlock ();
		return 0;
	}

	PORTEMU_CONTEXT *pContext = GetContext ((DWORD)pUserContext);

	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_msc_ind 0x%08x : context not found\n", pUserContext));
		gpPORTEMU->Unlock ();

		return ERROR_NOT_FOUND;
	}

	if (pContext->fkeep_dcd)
		v24 |= MSC_DV_BIT;

	DWORD dwOldLine = pContext->dwModemStatusIn;

	if (v24 & MSC_RTC_BIT) // RTC == DSR
		pContext->dwModemStatusIn |= MS_DSR_ON;
	else
		pContext->dwModemStatusIn &= ~MS_DSR_ON;

	if (v24 & MSC_RTR_BIT) // RTR == CTS
		pContext->dwModemStatusIn |= MS_CTS_ON;
	else
		pContext->dwModemStatusIn &= ~MS_CTS_ON;

	if (v24 & MSC_IC_BIT)
		pContext->dwModemStatusIn |= MS_RING_ON;
	else
		pContext->dwModemStatusIn &= ~MS_RING_ON;

	if (v24 & MSC_DV_BIT) // DV = DCD
		pContext->dwModemStatusIn |= MS_RLSD_ON;
	else
		pContext->dwModemStatusIn &= ~MS_RLSD_ON;

	dwOldLine ^= pContext->dwModemStatusIn;

	if (v24 & MSC_FC_BIT)
		pContext->fc = TRUE;
	else {
		pContext->fc = FALSE;
		SendData (pContext);
	}

	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"MSC Indicated : %s %s %s %s\n",
			dwOldLine & MS_RING_ON ? L"EV_RING" : L"",
			dwOldLine & MS_DSR_ON ? L"EV_DSR" : L"",
			dwOldLine & MS_CTS_ON ? L"EV_CTS" : L"",
			dwOldLine & MS_RLSD_ON ? L"EV_RLSD" : L"",
			v24 & MSC_FC_BIT ? L"Flow_Off" : L""));

	// Signal status change
	if (dwOldLine && gpPORTEMU->fInitialized && (pContext == GetContext ((DWORD)pUserContext))) {
		DWORD dwEvent = 0;
		if (dwOldLine & MS_RING_ON)
			dwEvent = EV_RING;
		if (dwOldLine & MS_DSR_ON)
			dwEvent |= EV_DSR;
		if (dwOldLine & MS_CTS_ON)
			dwEvent |= EV_CTS;
		if (dwOldLine & MS_RLSD_ON)
			dwEvent |= EV_RLSD;

		RegisterCommEvent (pContext, dwEvent, 0);
	}

	gpPORTEMU->Unlock ();

	return ERROR_SUCCESS;
}

static int portemu_stack_event (void *pUserContext, int iEvent, void *pEventContext) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_stack_event 0x%08x\n", pUserContext));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_stack_event 0x%08x : not initialized!\n", pUserContext));
		return 0;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_stack_event 0x%08x: not initialized!\n", pUserContext));
		gpPORTEMU->Unlock ();
		return 0;
	}

	PORTEMU_CONTEXT *pContext = GetContext ((DWORD)pUserContext);

	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_stack_event 0x%08x : context not found\n", pUserContext));
		gpPORTEMU->Unlock ();

		return ERROR_NOT_FOUND;
	}

	if (iEvent == BTH_STACK_DOWN)
		btutil_ScheduleEvent (StackDown, pContext);
	else if (iEvent == BTH_STACK_DISCONNECT)
		btutil_ScheduleEvent (StackDisconnect, NULL);

	gpPORTEMU->Unlock ();

	return ERROR_SUCCESS;
}

static int portemu_call_aborted (void *pCallContext, int iError) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_call_aborted 0x%08x\n", pCallContext));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_call_aborted 0x%08x : not initialized!\n", pCallContext));
		return 0;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_call_aborted 0x%08x: not initialized!\n", pCallContext));
		gpPORTEMU->Unlock ();
		return 0;
	}

	ECall *pCall = FindCall (pCallContext);
	if (! pCall) {
		IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] portemu_call_aborted 0x%08x : context does not exist\n", pCallContext));
		gpPORTEMU->Unlock ();
		return ERROR_SUCCESS;	
	}

	PORTEMU_CONTEXT *pContext = pCall->pContext;
	BOOL fSendDisconnect = (pCall->fWhat != RFCOMM_DISC);
	DeleteCall (pCall);

	CloseConnection (pContext, iError, fSendDisconnect);
	gpPORTEMU->Unlock ();

	return ERROR_SUCCESS;	
}

static int portemu_disconnect_out (void *pCallContext, int iError) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_disconnect_out 0x%08x\n", pCallContext));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_disconnect_out 0x%08x : not initialized!\n", pCallContext));
		return 0;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_disconnect_out 0x%08x: not initialized!\n", pCallContext));
		gpPORTEMU->Unlock ();
		return 0;
	}

	ECall *pCall = FindCall (pCallContext);
	if (! pCall) {
		IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] portemu_disconnect_out 0x%08x : context does not exist\n", pCallContext));
		gpPORTEMU->Unlock ();
		return ERROR_SUCCESS;	
	}

	PORTEMU_CONTEXT *pContext = pCall->pContext;
	DeleteCall (pCall);

	CloseConnection (pContext, iError, FALSE);
	gpPORTEMU->Unlock ();

	return ERROR_SUCCESS;
}

static int portemu_connect_request_out (void *pCallContext, int iError, HANDLE hConnection) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_connect_request_out 0x%08x\n", pCallContext));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_connect_request_out 0x%08x : not initialized!\n", pCallContext));
		return 0;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_connect_request_out 0x%08x: not initialized!\n", pCallContext));
		gpPORTEMU->Unlock ();
		return 0;
	}

	ECall *pCall = FindCall (pCallContext);
	if (! pCall) {
		IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] portemu_connect_request_out 0x%08x : context does not exist\n", pCallContext));
		gpPORTEMU->Unlock ();
		return ERROR_SUCCESS;	
	}

	PORTEMU_CONTEXT *pContext = pCall->pContext;
	SVSUTIL_ASSERT (pCall->fWhat == RFCOMM_CONNECT);
	SVSUTIL_ASSERT (pContext->pops && (pContext->pops->op == COMM_OPEN) && (! pContext->pops->pNext));	// MUST have one connection request pops

	DeleteCall (pCall);

	if (iError != ERROR_SUCCESS)
		CloseConnection (pContext, iError, TRUE);
	else {
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_connect_request_out 0x%08x : connection established\n", pCallContext));

		ReinitNewConnection (pContext, hConnection);

		PendingIO *pIO = pContext->pops;
		pContext->pops = NULL;

		SVSUTIL_ASSERT (pIO->op == COMM_OPEN);
		SVSUTIL_ASSERT (pIO->pNext == NULL);
		SVSUTIL_ASSERT (pIO->buffer == NULL);
		SVSUTIL_ASSERT (pIO->cbuffer == 0);
		SVSUTIL_ASSERT (pIO->cbuffer_used == 0);

		pIO->iIoResult = ERROR_SUCCESS;
		SetEvent (pIO->hEvent);

		pContext->ucv24out &= ~MSC_FC_BIT;
		SendMSC (pContext);
	}

	gpPORTEMU->Unlock ();

	return ERROR_SUCCESS;	
}

static int portemu_data_down_out (void *pCallContext, int iError) {
	if (! pCallContext) {
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_data_down_out : ignored empty call!\n"));
		return 0;
	}

	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_data_down_out 0x%08x\n", pCallContext));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_data_down_out 0x%08x : not initialized!\n", pCallContext));
		return 0;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_data_down_out 0x%08x: not initialized!\n", pCallContext));
		gpPORTEMU->Unlock ();
		return 0;
	}

	ECall *pCall = FindCall (pCallContext);
	if (! pCall) {
		IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] portemu_data_down_out 0x%08x : context does not exist\n", pCallContext));
		gpPORTEMU->Unlock ();
		return ERROR_SUCCESS;	
	}

	PORTEMU_CONTEXT *pContext = pCall->pContext;
	int cb = pCall->cBytes;

	SVSUTIL_ASSERT (pCall->fWhat == RFCOMM_WRITE);

	DeleteCall (pCall);

	if (iError != ERROR_SUCCESS)
		CloseConnection (pContext, iError, TRUE);
	else {
		pContext->iSendQuotaUsed -= cb;
		if (pContext->iSendQuota < 0) {
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] quota miscalculated in portemu_data_down_out\n"));
			pContext->iSendQuota = 0;
		}

		SendData (pContext);
	}

	gpPORTEMU->Unlock ();

	return ERROR_SUCCESS;	
}

static int portemu_pnreq_out (void *pCallContext, int iError, unsigned char priority, unsigned short n1, int use_credit_fc, int initial_credits) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_pnreq_out 0x%08x mtu = %d credit fc = %x initial credits = %d\n", pCallContext, n1, use_credit_fc, initial_credits));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_pnreq_out 0x%08x : not initialized!\n", pCallContext));
		return 0;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_pnreq_out 0x%08x: not initialized!\n", pCallContext));
		gpPORTEMU->Unlock ();
		return 0;
	}

	ECall *pCall = FindCall (pCallContext);
	if (! pCall) {
		IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] portemu_pnreq_out 0x%08x : context does not exist\n", pCallContext));
		gpPORTEMU->Unlock ();
		return ERROR_SUCCESS;	
	}

	PORTEMU_CONTEXT *pContext = pCall->pContext;
	SVSUTIL_ASSERT (pCall->fWhat == RFCOMM_PN);
	SVSUTIL_ASSERT (pContext->pops && (pContext->pops->op == COMM_OPEN) && (! pContext->pops->pNext));	// MUST have one connection request pops

	DeleteCall (pCall);

	if (use_credit_fc == RFCOMM_PN_CREDIT_OUT) {
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_pnreq_ind 0x%08x CREDIT-based Flow ON\n", pContext));

		pContext->credit_fc    = TRUE;
		pContext->iHaveCredits = initial_credits;
		pContext->iGaveCredits = RFCOMM_PN_CREDIT_MAX;
	} else {
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_pnreq_ind 0x%08x CREDIT-based Flow OFF\n", pContext));

		pContext->credit_fc    = FALSE;
		pContext->iHaveCredits = 0;
		pContext->iGaveCredits = 0;
	}

	if ((n1 >= pContext->iMinMTU) && (n1 <= pContext->iMaxMTU)) {
		pContext->iMTU = n1;
		pContext->pops->iIoResult = ERROR_SUCCESS;
	} else
		pContext->pops->iIoResult = ERROR_INVALID_DATA;

	SVSUTIL_ASSERT (pContext->pops->pNext == NULL);
	SVSUTIL_ASSERT (pContext->pops->op == COMM_OPEN);
	SetEvent (pContext->pops->hEvent);

	pContext->pops = NULL;

	gpPORTEMU->Unlock ();

	return ERROR_SUCCESS;	
}

static int portemu_rpnreq_out (void *pCallContext, int iError, unsigned short mask, int baud, int data, int stop, int parity, int parity_type, unsigned char flow, unsigned char xon, unsigned char xoff) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] portemu_rpnreq_out 0x%08x\n", pCallContext));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_rpnreq_out 0x%08x : not initialized!\n", pCallContext));
		return 0;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_rpnreq_out 0x%08x: not initialized!\n", pCallContext));
		gpPORTEMU->Unlock ();
		return 0;
	}

	ECall *pCall = FindCall (pCallContext);
	if (! pCall) {
		IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] portemu_rpnreq_out 0x%08x : context does not exist\n", pCallContext));
		gpPORTEMU->Unlock ();
		return ERROR_SUCCESS;	
	}

	SVSUTIL_ASSERT (pCall->fWhat == RFCOMM_RPN);

	PORTEMU_CONTEXT *pContext = pCall->pContext;

	DeleteCall (pCall);

	if (mask & RFCOMM_RPN_HAVE_BAUD)
		pContext->dcb.BaudRate = baud;

	if (mask & RFCOMM_RPN_HAVE_DATA)
		pContext->dcb.ByteSize = data;

	if (mask & RFCOMM_RPN_HAVE_STOP)
		pContext->dcb.StopBits = stop;

	if (mask & RFCOMM_RPN_HAVE_PARITY)
		pContext->dcb.fParity = parity;

	if (mask & RFCOMM_RPN_HAVE_PT)
		pContext->dcb.Parity = parity_type;

	if (mask & RFCOMM_RPN_HAVE_XOFF)
		pContext->dcb.XoffChar = xoff;

	if (mask & RFCOMM_RPN_HAVE_XON)
		pContext->dcb.XonChar = xon;

	if (mask & RFCOMM_RPN_HAVE_XON_IN)
		pContext->dcb.fInX = flow & RFCOMM_RPN_XON_IN ? TRUE : FALSE;

	if (mask & RFCOMM_RPN_HAVE_XON_OUT)
		pContext->dcb.fOutX = flow & RFCOMM_RPN_XON_OUT ? TRUE : FALSE;

	//#error what's the deal with the rest of the flow???

	PendingIO *pio = pContext->pops;
	PendingIO *pParent = NULL;

	while (pio && (pio->op != COMM_RPN)) {
		pParent = pio;
		pio = pio->pNext;
	}

	if (pio) {
		if (pParent)
			pParent = pio->pNext;
		else
			pContext->pops = pio->pNext;

		pio->iIoResult = ERROR_SUCCESS;
		SetEvent (pio->hEvent);
	}

	gpPORTEMU->Unlock ();

	return ERROR_SUCCESS;	
}

static int PORTEMUSetPortParams
(
PORTEMU_CONTEXT		*pContext,
int					channel,
int					flocal,
BD_ADDR				*pdevice,
int					imtu,
int					iminmtu,
int					imaxmtu,
int					isendquota,
int					irecvquota,
GUID				uuidService,
int					uiportflags
) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] PORTEMUSetPortParams channel %d local: %s device %04x%08x mtu %d minmtu %d maxmtu %d Quotas: send %d recv %d\n",
		channel, flocal ? L"yes" : L"no", pdevice ? pdevice->NAP : 0, pdevice ? pdevice->SAP : 0, imtu, iminmtu, imaxmtu, isendquota, irecvquota));

	if (flocal && pdevice) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] channel can't be local and have destination! ERROR_INVALID_PARAMETER\n"));
		return ERROR_INVALID_PARAMETER;
	}

	if (((iminmtu != 0) && (iminmtu < PORTEMU_MTUMIN)) ||
		 (imaxmtu > PORTEMU_MTUMAX) ||
		 (imtu && ((imtu < PORTEMU_MTUMIN) || (imtu > PORTEMU_MTUMAX)))) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] mtu out of range ERROR_INVALID_PARAMETER\n"));
		return ERROR_INVALID_PARAMETER;
	}

	SVSUTIL_ASSERT (pContext->hConnection == NULL);
	SVSUTIL_ASSERT (pContext->hRFCOMM == NULL);

	RFCOMM_EVENT_INDICATION EventInd;
	memset (&EventInd, 0, sizeof(EventInd));

	EventInd.rfcomm_ConnectRequest_Ind	= portemu_connect_request_ind;
	EventInd.rfcomm_DataUp_Ind			= portemu_data_up_ind;
	EventInd.rfcomm_Disconnect_Ind		= portemu_disconnect_ind;
	EventInd.rfcomm_StackEvent			= portemu_stack_event;
	EventInd.rfcomm_PNREQ_Ind           = portemu_pnreq_ind;
	EventInd.rfcomm_FC_Ind              = portemu_fc_ind;
	EventInd.rfcomm_MSC_Ind             = portemu_msc_ind;
	EventInd.rfcomm_RPNREQ_Ind          = portemu_rpnreq_ind;

	RFCOMM_CALLBACKS Callbacks;
	memset (&Callbacks, 0, sizeof(Callbacks));

	Callbacks.rfcomm_CallAborted		= portemu_call_aborted;
	Callbacks.rfcomm_ConnectRequest_Out = portemu_connect_request_out;
	Callbacks.rfcomm_Disconnect_Out		= portemu_disconnect_out;
	Callbacks.rfcomm_DataDown_Out		= portemu_data_down_out;
	Callbacks.rfcomm_PNREQ_Out          = portemu_pnreq_out;
	Callbacks.rfcomm_RPNREQ_Out         = portemu_rpnreq_out;

	int iErr;

	if (ERROR_SUCCESS !=
		(iErr = RFCOMM_EstablishDeviceContext (pContext, flocal ? channel : RFCOMM_CHANNEL_CLIENT_ONLY,
						&EventInd, &Callbacks, &pContext->rfcomm_if, &pContext->iDeviceHead,
						&pContext->iDeviceTrail, &pContext->hRFCOMM))) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] PORTEMUSetPortParams : RFCOMM_EstablishDeviceContext returns %d\n", iErr));
		return iErr;
	}

	if (channel == RFCOMM_CHANNEL_MULTIPLE) {
		BT_LAYER_IO_CONTROL pCallback = pContext->rfcomm_if.rfcomm_ioctl;
		iErr = ERROR_INTERNAL_ERROR;
		unsigned char cPort = 0;
		int iSize = 0;
		__try {
			unsigned char cZero = 0;
			iErr = pCallback (pContext->hRFCOMM, BTH_STACK_IOCTL_RESERVE_PORT, sizeof(cZero), (char *)&cZero, sizeof(cPort), (char *)&cPort, &iSize);
		} __except (1) {
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] PORTEMUSetPortParams : BTH_STACK_IOCTL_RESERVE_PORT excepted\n"));
		}

		if (iErr != ERROR_SUCCESS) {
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] PORTEMUSetPortParams : BTH_STACK_IOCTL_RESERVE_PORT returns %d : closing down\n", iErr));
			RFCOMM_CloseDeviceContext (pContext->hRFCOMM);
			pContext->hRFCOMM = NULL;
			return iErr;
		}

		channel = cPort;
	}

	pContext->iRecvQuota = irecvquota ? irecvquota : gpPORTEMU->iDefaultRecvQuota;
	pContext->iSendQuota = isendquota ? isendquota : gpPORTEMU->iDefaultSendQuota;

	pContext->iMTU = imtu ? imtu : PORTEMU_MTU;

	pContext->iMaxMTU = imaxmtu ? imaxmtu : gpPORTEMU->iDefaultMTUMax;
	pContext->iMinMTU = iminmtu ? iminmtu : gpPORTEMU->iDefaultMTUMin;

	pContext->channel = channel;
	pContext->uuidService = uuidService;
	if (pdevice)
		pContext->b = *pdevice;
	pContext->fLocal = flocal;
	if (uiportflags & RFCOMM_PORT_FLAGS_REMOTE_DCB)
		pContext->local_dcb = FALSE;
	else
		pContext->local_dcb = TRUE;

	if (uiportflags & RFCOMM_PORT_FLAGS_KEEP_DCD)
		pContext->fkeep_dcd = TRUE;
	else
		pContext->fkeep_dcd = FALSE;

	if (uiportflags & RFCOMM_PORT_FLAGS_AUTHENTICATE)
		pContext->freq_auth = TRUE;
	else
		pContext->freq_auth = FALSE;

	if (uiportflags & RFCOMM_PORT_FLAGS_ENCRYPT)
		pContext->freq_encrypt = TRUE;
	else
		pContext->freq_encrypt = FALSE;

	return ERROR_SUCCESS;
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//		EXECUTION THREAD: Client-application!
//			These functions are only executed on the caller's thread
// 			i.e. the thread belongs to the client application
//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//	@func PVOID | COM_Init | Device initialization routine
//  @parm DWORD | dwInfo | Info passed to RegisterDevice
//  @rdesc	Returns a DWORD which will be passed to Open & Deinit or NULL if
//			unable to initialize the device.
//	@remark	Routine exported by a device driver.  "PRF" is the string passed
//			in as lpszType in RegisterDevice
extern "C" DWORD COM_Init (DWORD dwInfo) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] COM_Init 0x%08x\n", dwInfo));

	PORTEMUPortParams *pParams = (PORTEMUPortParams *)MapPtrToProcess (((void *)dwInfo), GetOwnerProcess ());
	PORTEMUPortParams pp;

	__try {
		pp = *pParams;
	} __except (1) {
		pParams = NULL;
	}

	GUID uuidZero;
	memset(&uuidZero, 0, sizeof(GUID));
	if ((! pParams) || ((pp.iminmtu != 0) && (pp.iminmtu < PORTEMU_MTUMIN)) ||
		 (pp.imaxmtu > PORTEMU_MTUMAX) ||
		 (pp.imtu && ((pp.imtu < PORTEMU_MTUMIN) || (pp.imtu > PORTEMU_MTUMAX))) ||
		 ((pp.channel == 0) && (!memcmp(&pp.uuidService, &uuidZero, sizeof(GUID))) ) ) {
		SetLastError (ERROR_INVALID_PARAMETER);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Init 0x%08x : ERROR_INVALID_PARAMETER\n", dwInfo));
		return FALSE;
	}

	if (! gpPORTEMU) {
		SetLastError (ERROR_SERVICE_NOT_ACTIVE);
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Init 0x%08x : not initialized!\n", dwInfo));
		return 0;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		SetLastError (ERROR_SERVICE_NOT_ACTIVE);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Init 0x%08x : not initialized!\n", dwInfo));
		gpPORTEMU->Unlock ();
		return 0;
	}

	PORTEMU_CONTEXT *pContext = NewPORTEMU_CONTEXT ();

	if (pContext) {
		int iRes = PORTEMUSetPortParams (pContext, pp.channel, pp.flocal, pp.flocal ? NULL : (BD_ADDR *)&pp.device, pp.imtu, pp.iminmtu, pp.imaxmtu, pp.isendquota, pp.irecvquota, pp.uuidService, pp.uiportflags);
		if (iRes == ERROR_SUCCESS) {
			pContext->pNext = gpPORTEMU->pPorts;
			gpPORTEMU->pPorts = pContext;
		} else {
			SetLastError (iRes);
			delete pContext;
			pContext = NULL;
		}
	} else
		SetLastError (ERROR_OUTOFMEMORY);

	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] COM_Init 0x%08x : created context 0x%08x\n", dwInfo, pContext));

	gpPORTEMU->Unlock ();
	return (DWORD)pContext;
}

//	@func PVOID | COM_Deinit | Device deinitialization routine
//  @parm DWORD | dwData | value returned from CON_Init call
//  @rdesc	Returns TRUE for success, FALSE for failure.
//	@remark	Routine exported by a device driver.  "PRF" is the string
//			passed in as lpszType in RegisterDevice
extern "C" BOOL COM_Deinit(DWORD dwData) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] COM_Deinit 0x%08x\n", dwData));

	if (! gpPORTEMU) {
		SetLastError (ERROR_SERVICE_NOT_ACTIVE);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Deinit 0x%08x : not initialized\n", dwData));
		return FALSE;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		SetLastError (ERROR_SERVICE_NOT_ACTIVE);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Deinit 0x%08x : not initialized\n", dwData));
		gpPORTEMU->Unlock ();
		return FALSE;
	}

	PORTEMU_CONTEXT *pContext = gpPORTEMU->pPorts;
	PORTEMU_CONTEXT *pParent = NULL;

	while (pContext && (pContext != (PORTEMU_CONTEXT *)dwData)) {
		pParent = pContext;
		pContext = pContext->pNext;
	}

	if (! pContext) {
		SetLastError (ERROR_NOT_FOUND);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Deinit 0x%08x : not found\n", dwData));
		gpPORTEMU->Unlock ();
		return FALSE;
	}

	if (! pParent)
		gpPORTEMU->pPorts = pContext->pNext;
	else
		pParent->pNext = pContext->pNext;

	delete pContext;

	gpPORTEMU->Unlock ();

	return TRUE;
}

//	@func PVOID | COM_Open		| Device open routine
//  @parm DWORD | dwData		| value returned from CON_Init call
//  @parm DWORD | dwAccess		| requested access (combination of GENERIC_READ
//								  and GENERIC_WRITE)
//  @parm DWORD | dwShareMode	| requested share mode (combination of
//								  FILE_SHARE_READ and FILE_SHARE_WRITE)
//  @rdesc	Returns a DWORD which will be passed to Read, Write, etc or NULL if
//			unable to open device.
//	@remark	Routine exported by a device driver.  "PRF" is the string passed
//			in as lpszType in RegisterDevice
extern "C" DWORD COM_Open (DWORD dwData, DWORD dwAccess, DWORD dwShareMode) {
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] COM_Open 0x%08x\n", dwData));

	if (! gpPORTEMU) {
		SetLastError (ERROR_SERVICE_NOT_ACTIVE);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Open 0x%08x : not initialized\n", dwData));
		return FALSE;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		SetLastError (ERROR_BUSY);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Open 0x%08x : not initialized\n", dwData));
		gpPORTEMU->Unlock ();
		return FALSE;
	}

	PORTEMU_CONTEXT *pContext = GetContext (dwData);

	// Do not allow open while we are closing
	for (int i = 0 ; i < PORTEMU_OPEN_ATTEMPTS ; ++i) {
		if (! pContext->fClosing) {
			break;
		}
		gpPORTEMU->Unlock ();
		Sleep (PORTEMU_OPEN_TIMEOUT);
		gpPORTEMU->Lock ();
	}

	if (pContext->fClosing) {
		SetLastError (ERROR_BUSY);
		
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Open 0x%08x : cannot open while closing\n", dwData));
		gpPORTEMU->Unlock ();
		return FALSE;
	}		

	if ((! pContext) || (! pContext->hRFCOMM) || (pContext->uiOpenRef == PORTEMU_OPEN_ALLUSED)) {
		SetLastError (pContext ? (pContext->hRFCOMM ? ERROR_ALREADY_INITIALIZED : ERROR_NOT_READY) : ERROR_NOT_FOUND);

		if (! pContext)
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Open 0x%08x : not found\n", dwData));
		else if (! pContext->hRFCOMM)
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Open 0x%08x : not setup\n", dwData));
		else
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Open 0x%08x : no resources\n", dwData));
		gpPORTEMU->Unlock ();

		return FALSE;
	}

	for (int iPortNum = 0 ; iPortNum < PORTEMU_OPEN_MAX ; ++iPortNum) {
		if (((1 << iPortNum) & pContext->uiOpenRef) == 0)
			break;
	}

	SVSUTIL_ASSERT (iPortNum < PORTEMU_OPEN_MAX);
	SVSUTIL_ASSERT ((iPortNum & dwData) == 0);

	pContext->uiOpenRef |= (1 << iPortNum);
	pContext->hOwnerProc[iPortNum] = GetOwnerProcess ();

	if (dwAccess & GENERIC_READ)
		pContext->uiReadRef |= (1 << iPortNum);

	if (dwAccess & GENERIC_WRITE)
		pContext->uiWriteRef |= (1 << iPortNum);

	if (pContext->fSdpQueryOn || pContext->hConnection || pContext->fLocalOpen || (dwAccess == 0)) {
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] COM_Open 0x%08x : already open - just incrementing the ref\n", dwData));
		gpPORTEMU->Unlock ();
		return dwData | iPortNum;
	}

	if (pContext->fLocal) {
		pContext->fLocalOpen = TRUE;
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] COM_Open 0x%08x : opening as local context\n", dwData));
		gpPORTEMU->Unlock ();
		return dwData | iPortNum;
	}

	int fAuthenticate = pContext->freq_auth;
	int fEncrypt      = pContext->freq_encrypt;
	BT_ADDR bt        = SET_NAP_SAP (pContext->b.NAP, pContext->b.SAP);

	SVSUTIL_ASSERT (pContext->fSdpQueryOn == FALSE);

	PendingIO *pio = NewPendingIO (COMM_OPEN, iPortNum);

	if ((! pio) || (! pio->hEvent)) {
		if (pio)
			delete pio;

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Open 0x%08x : Failed to create pending IO. ERROR_NOT_ENOUGH_MEMORY\n", dwData));

		ForgetPort (pContext, iPortNum);

		SetLastError(ERROR_NO_SYSTEM_RESOURCES);
		gpPORTEMU->Unlock ();
		return FALSE;
	}

	HANDLE hConnect = NULL;
	HANDLE h = pContext->hRFCOMM;

	int iRes = ERROR_SUCCESS;

	GUID uuidZero;
	memset(&uuidZero, 0, sizeof(GUID));

	if (0 != memcmp(&pContext->uuidService, &uuidZero, sizeof(GUID))) {	//perform inquiry if you have the uuid
		SVSUTIL_ASSERT (! pContext->pops);
		SVSUTIL_ASSERT (! pio->pNext);
		pContext->pops = pio;

		pContext->channel = 0;

		ECall *pCall = NewCall (RFCOMM_SDP, pContext);
		if (! pCall) {
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Open 0x%08x : out of memory\n", dwData));
			iRes = ERROR_OUTOFMEMORY;
		} else {
			pCall->pNext     = pContext->pCalls;
			pContext->pCalls = pCall;

			pContext->fSdpQueryOn = TRUE;

			HANDLE hSDP					= gpPORTEMU->hSDP;
			SDP_Connect_In pCallback	= gpPORTEMU->sdp_if.sdp_Connect_In;
			BD_ADDR b = pContext->b;

			SVSUTIL_ASSERT(hSDP && pCallback && b.NAP && b.SAP);

			HANDLE hEvent = pio->hEvent;
		
			gpPORTEMU->Unlock();

			iRes = ERROR_INTERNAL_ERROR;

			__try {
				iRes = pCallback (hSDP, pCall, &b);
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] : exception in COM_Open 0x%08x!\n", dwData));
			}

			if (iRes == ERROR_SUCCESS)
				WaitForSingleObject(hEvent, INFINITE);

			if (! gpPORTEMU) {
				SetLastError (ERROR_INTERNAL_ERROR);
				return 0;
			}

			gpPORTEMU->Lock();

			if (iRes == ERROR_SUCCESS)
				iRes = (gpPORTEMU->fInitialized && (pContext == GetContext (dwData))) ? pio->iIoResult : ERROR_OPERATION_ABORTED;
			else if (FindCall (pCall))
				DeleteCall (pCall);

			if (gpPORTEMU->fInitialized && (pContext == GetContext (dwData)))
				pContext->fSdpQueryOn = FALSE;
		}
	}

	SVSUTIL_ASSERT ((pContext != GetContext (dwData)) || (pContext->fSdpQueryOn == FALSE));

	if (iRes == ERROR_SUCCESS) {
		if ((pContext->channel < 1) || (pContext->channel > PORTEMU_MAX_RFCOMM_CHANNEL)) {
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Open 0x%08x : could not retrieve remote port for device:%04x%08x\n", dwData, pContext->b.NAP, pContext->b.SAP));
			iRes = ERROR_PORT_UNREACHABLE;
		} else {
			iRes = pContext->rfcomm_if.rfcomm_CreateChannel (pContext->hRFCOMM, &pContext->b, pContext->channel, &hConnect);
			if (iRes == ERROR_SUCCESS)
				pContext->hConnection = hConnect;
		}
	}

	if (iRes == ERROR_SUCCESS) {	// Gotta PN first...
		SVSUTIL_ASSERT (! pContext->pops);
		SVSUTIL_ASSERT (! pio->pNext);
		pContext->pops = pio;

		ECall *pCall = NewCall (RFCOMM_PN, pContext);
		if (! pCall) {
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Open 0x%08x : out of memory\n", dwData));
			iRes = ERROR_OUTOFMEMORY;
		} else {
			pCall->pNext = pContext->pCalls;
			pContext->pCalls = pCall;

			RFCOMM_PNREQ_In pCallback = pContext->rfcomm_if.rfcomm_PNREQ_In;
			iRes = ERROR_INTERNAL_ERROR;
			int n1 = pContext->iMTU;
			gpPORTEMU->Unlock ();
			__try {
				iRes = pCallback (h, pCall, hConnect, PORTEMU_PRI, n1, RFCOMM_PN_CREDIT_IN, RFCOMM_PN_CREDIT_MAX);
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Open: exception in rfcomm_PNREQ_In\n"));
			}

			if (iRes == ERROR_SUCCESS)	// Wait
				WaitForSingleObject (pio->hEvent, INFINITE);

			if (! gpPORTEMU) {
				SetLastError (ERROR_INTERNAL_ERROR);
				return 0;
			}

			gpPORTEMU->Lock ();

			if (iRes == ERROR_SUCCESS)
				iRes = (gpPORTEMU->fInitialized && (pContext == GetContext (dwData))) ? pio->iIoResult : ERROR_OPERATION_ABORTED;
			else if (FindCall (pCall))
				DeleteCall (pCall);
		}
	}

	if (iRes == ERROR_SUCCESS) {	// Connect
		SVSUTIL_ASSERT (! pContext->pops);
		SVSUTIL_ASSERT (! pio->pNext);
		pContext->pops = pio;

		ECall *pCall = NewCall (RFCOMM_CONNECT, pContext);
		if (! pCall) {
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Open 0x%08x : out of memory\n", dwData));
			iRes = ERROR_OUTOFMEMORY;
		} else {
			pCall->pNext = pContext->pCalls;
			pContext->pCalls = pCall;

			IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] COM_Open 0x%08x : connecting the channel\n", dwData));
			RFCOMM_ConnectRequest_In pCallback = pContext->rfcomm_if.rfcomm_ConnectRequest_In;

			iRes = ERROR_INTERNAL_ERROR;
			gpPORTEMU->Unlock ();
			__try {
				iRes = pCallback (h, pCall, hConnect);
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Open: exception in rfcomm_ConnectRequest_In\n"));
			}

			if (iRes == ERROR_SUCCESS)
				WaitForSingleObject (pio->hEvent, INFINITE);

			if (! gpPORTEMU) {
				SetLastError (ERROR_INTERNAL_ERROR);
				return 0;
			}

			gpPORTEMU->Lock ();
			if (iRes == ERROR_SUCCESS)
				iRes = (gpPORTEMU->fInitialized && (pContext == GetContext (dwData))) ? pio->iIoResult : ERROR_OPERATION_ABORTED;
			else if (FindCall (pCall))
				DeleteCall (pCall);
		}
	}

	RemoveIO (pContext, pio);
	delete pio;

	gpPORTEMU->Unlock ();

	if ((iRes == ERROR_SUCCESS) && fAuthenticate)
		iRes = BthAuthenticate (&bt);

	if ((iRes == ERROR_SUCCESS) && fEncrypt)
		iRes = BthSetEncryption (&bt, TRUE);

	gpPORTEMU->Lock ();

	if (iRes != ERROR_SUCCESS) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Open 0x%08x : error %d\n", dwData, iRes));
		if (gpPORTEMU->fInitialized && (pContext == GetContext (dwData))) {
			pContext->hConnection = NULL;
			
			ForgetPort (pContext, iPortNum);

			RFCOMM_Disconnect_In pCallback = pContext->rfcomm_if.rfcomm_Disconnect_In;

			gpPORTEMU->Unlock ();
			__try {
				pCallback (h, NULL, hConnect);
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Open : Exception in rfcomm_Disconnect_In\n"));
			}
			gpPORTEMU->Lock ();
		}
		SetLastError (iRes);
	}

	IFDBG(pContext = GetContext (dwData); DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] COM_Open 0x%08x : connected  to %04x%08x (%d), status %d\n", dwData, pContext ? pContext->b.NAP : 0, pContext ? pContext->b.SAP : 0, pContext ? pContext->channel : 0, iRes));
	gpPORTEMU->Unlock ();

	return iRes == ERROR_SUCCESS ? (dwData | iPortNum) : 0;
}

//	@func BOOL | COM_Close | Device close routine
//  @parm DWORD | dwOpenData | value returned from BTD_Open call
//  @rdesc	Returns TRUE for success, FALSE for failure
//	@remark	Routine exported by a device driver.  "PRF" is the string passed
//			in as lpszType in RegisterDevice
extern "C" BOOL COM_Close (DWORD dwData)  {
	int iPortNum = dwData & PORTEMU_OPEN_MASK;
	dwData &= ~PORTEMU_OPEN_MASK;

	if ((iPortNum < 0) || (iPortNum >= PORTEMU_OPEN_MAX)) {
		SetLastError (ERROR_INVALID_PARAMETER);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Close 0x%08x : bad port %d\n", dwData, iPortNum));
		return FALSE;
	}


	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] COM_Close 0x%08x %d\n", dwData, iPortNum));

	if (! gpPORTEMU) {
		SetLastError (ERROR_SERVICE_NOT_ACTIVE);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Close 0x%08x : not initialized\n", dwData));
		return FALSE;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		SetLastError (ERROR_SERVICE_NOT_ACTIVE);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Close 0x%08x : not initialized\n", dwData));
		gpPORTEMU->Unlock ();
		return FALSE;
	}

	PORTEMU_CONTEXT *pContext = GetContext (dwData);

	if ((! pContext) || (! pContext->hRFCOMM)) {
		SetLastError (pContext ? (pContext->hRFCOMM ? ERROR_NOT_CONNECTED : ERROR_NOT_READY) : ERROR_INVALID_HANDLE);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Close 0x%08x : not found, or not setup\n", dwData));
		gpPORTEMU->Unlock ();
		return FALSE;
	}

	pContext->fClosing = TRUE;

	ForgetPort (pContext, iPortNum);

	// Unblock IO
	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"COM_Close : unblocking UI for ctx 0x%08x\n", pContext));

	PendingIO *pParent = NULL;
	PendingIO *pIO = pContext->pops;

	while (pIO) {
		SVSUTIL_ASSERT ((pIO->iPortNum >= 0) && (pIO->iPortNum < PORTEMU_OPEN_MAX));

		if (pIO->iPortNum == iPortNum) {
			IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"COM_Close : ctx 0x%08x found and executed waiter (port %d)...\n", pContext, pIO->iPortNum));

			PendingIO *pNext = pIO->pNext;

			if (pParent)
				pParent->pNext = pIO->pNext;
			else
				pContext->pops = pIO->pNext;

			pIO->pNext = NULL;
			
			pIO->iIoResult = ERROR_CTX_CLOSE_PENDING;
			SetEvent (pIO->hEvent);

			pIO = pNext;
			continue;
		}

		pParent = pIO;
		pIO = pIO->pNext;
	}

	if (! (pContext->uiReadRef || pContext->uiWriteRef)) {
		if (pContext->hConnection) {
			IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] Actually closing 0x%08x\n", dwData));
			pContext->fLocalOpen = FALSE;

			PendingIO *pio = NewPendingIO (COMM_DISC, iPortNum);
			if (! pio) {
				SetLastError (ERROR_NO_SYSTEM_RESOURCES);
				pContext->fClosing = FALSE;
				gpPORTEMU->Unlock ();
				return FALSE;			
			}

			InsertIO (pContext, pio);
			RFCommDisconnectIn (pContext);

			gpPORTEMU->Unlock ();
			DWORD dwRes = WaitForSingleObject (pio->hEvent, INFINITE);
			gpPORTEMU->Lock ();

			delete pio;

			if (! gpPORTEMU) {
				SetLastError (ERROR_INTERNAL_ERROR);
				pContext->fClosing = FALSE;
				gpPORTEMU->Unlock ();
				return FALSE;
			}		
		}else
			CloseConnection (pContext, ERROR_GRACEFUL_DISCONNECT, FALSE);
	} else
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] Just decrementing ref count on 0x%08x\n", dwData));

	pContext->fClosing = FALSE;
	gpPORTEMU->Unlock ();

	return TRUE;
}

//	@func DWORD | COM_Write | Device write routine
//  @parm DWORD | dwOpenData | value returned from CON_Open call
//  @parm LPCVOID | pBuf | buffer containing data
//  @parm DWORD | len | maximum length to write [IN BYTES, NOT WORDS!!!]
//  @rdesc	Returns -1 for error, otherwise the number of bytes written.  The
//			length returned is guaranteed to be the length requested unless an
//			error condition occurs.
//	@remark	Routine exported by a device driver.  "PRF" is the string passed
//			in as lpszType in RegisterDevice
//
extern "C" DWORD COM_Write (DWORD dwData, LPCVOID pInBuf, DWORD dwInLen) {
	int iPortNum = dwData & PORTEMU_OPEN_MASK;
	dwData &= ~PORTEMU_OPEN_MASK;

	if ((iPortNum < 0) || (iPortNum >= PORTEMU_OPEN_MAX)) {
		SetLastError (ERROR_INVALID_PARAMETER);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Write 0x%08x : bad port %d\n", dwData, iPortNum));
		return -1;
	}

	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] COM_Write 0x%08x %d bytes\n", dwData, dwInLen));

	if (! gpPORTEMU) {
		SetLastError (ERROR_SERVICE_NOT_ACTIVE);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Write 0x%08x : not initialized\n", dwData));
		return -1;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		SetLastError (ERROR_SERVICE_NOT_ACTIVE);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Write 0x%08x : not initialized\n", dwData));
		gpPORTEMU->Unlock ();
		return -1;
	}

	PORTEMU_CONTEXT *pContext = GetContext (dwData);

	if ((! pContext) || (! pContext->hRFCOMM) || 
				(! (pContext->uiOpenRef & (1 << iPortNum))) ||
				(! (pContext->uiWriteRef & (1 << iPortNum)))) {
		SetLastError (pContext ? (pContext->hRFCOMM ? ERROR_NOT_CONNECTED : ERROR_NOT_READY) : ERROR_INVALID_HANDLE);

#if defined (DEBUG) || defined (_DEBUG)
		if (! pContext)
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Write 0x%08x : not found\n", dwData));
		else if (! pContext->hRFCOMM)
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Write 0x%08x : not setup\n", dwData));
		else if (! pContext->hConnection)
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Write 0x%08x : not connected\n", dwData));
		else if (! (pContext->uiOpenRef & (1 << iPortNum)))
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Write 0x%08x : not opened as port %d\n", dwData, iPortNum));
		else if (! (pContext->uiWriteRef & (1 << iPortNum)))
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Write 0x%08x : not opened with write access\n", dwData));
#endif

		gpPORTEMU->Unlock ();
		return -1;
	}

	PendingIO *pio = NewPendingIO (COMM_WRITE, iPortNum);

	if ((! pio) || (! pio->hEvent)) {
		if (pio)
			delete pio;

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Write 0x%08x : Failed to create pending IO. ERROR_NO_SYSTEM_RESOURCES\n", dwData));
		SetLastError(ERROR_NO_SYSTEM_RESOURCES);
		gpPORTEMU->Unlock ();
		return -1;
	}

	pio->buffer  = (unsigned char *)pInBuf;
	pio->cbuffer = dwInLen;
	pio->dwPerms = GetCurrentPermissions ();

	InsertIO (pContext, pio);

	int iTimeout = pContext->ct.WriteTotalTimeoutConstant;
	if (pContext->ct.WriteTotalTimeoutMultiplier != MAXDWORD)
		iTimeout += pContext->ct.WriteTotalTimeoutMultiplier * dwInLen;

	int iRes = SendData (pContext);

	if (iRes == ERROR_IO_PENDING) {
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] COM_Write 0x%08x %d bytes pending...\n", dwData, dwInLen));
		gpPORTEMU->Unlock ();
		DWORD dwRes = WaitForSingleObject (pio->hEvent, iTimeout);

		if (! gpPORTEMU) {
			SetLastError (ERROR_INTERNAL_ERROR);
			return -1;
		}

		gpPORTEMU->Lock ();
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] COM_Write 0x%08x %d bytes pending (out)...\n", dwData, dwInLen));
		iRes = pio->iIoResult;
	}

	RemoveIO (pContext, pio);
	int iobytes = pio->cbuffer_used;

	delete pio;

	// If we are the server and were disconnected, then it actually makes sense to just act as 
	// though we were not connected from the beginning (as a write would do if it were called 
	// without being connected from the beginning).
	if ((iRes == ERROR_GRACEFUL_DISCONNECT) && pContext->fLocal) {
		iRes = ERROR_SUCCESS;
	}

	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] COM_Write 0x%08x %d bytes completed, %d bytes written, status %d\n", dwData, dwInLen, iobytes, iRes));

	gpPORTEMU->Unlock ();

	if (iRes != ERROR_SUCCESS) {
		SetLastError (iRes);
		return -1;
	} else {
#if defined (DEBUG_DUMP_RFCOMM_STREAM)
		IFDBG(DumpBuffPfx (DEBUG_RFCOMM_STREAM, L"IN>", (unsigned char *)pInBuf, iobytes));
#endif
	}

	return iobytes;
}

//	@func DWORD | COM_Read | Device read routine
//  @parm DWORD | dwOpenData | value returned from CON_Open call
//  @parm LPVOID | pBuf | buffer to receive data
//  @parm DWORD | len | maximum length to read [IN BYTES, not WORDS!!]
//  @rdesc	Returns 0 for end of file, -1 for error, otherwise the number of
//			bytes read.  The length returned is guaranteed to be the length
//			requested unless end of file or an error condition occurs.
//	@remark	Routine exported by a device driver.  "PRF" is the string passed
//			in as lpszType in RegisterDevice
//
extern "C" DWORD COM_Read (DWORD dwData, LPVOID pBuf, DWORD Len)
{
	int iPortNum = dwData & PORTEMU_OPEN_MASK;
	dwData &= ~PORTEMU_OPEN_MASK;

	if ((iPortNum < 0) || (iPortNum >= PORTEMU_OPEN_MAX)) {
		SetLastError (ERROR_INVALID_PARAMETER);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Read 0x%08x : bad port %d\n", dwData, iPortNum));
		return -1;
	}

	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] COM_Read 0x%08x\n", dwData));

	if (! gpPORTEMU) {
		SetLastError (ERROR_SERVICE_NOT_ACTIVE);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Read 0x%08x : not initialized\n", dwData));
		return -1;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		SetLastError (ERROR_SERVICE_NOT_ACTIVE);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Read 0x%08x : not initialized\n", dwData));
		gpPORTEMU->Unlock ();
		return -1;
	}

	PORTEMU_CONTEXT *pContext = GetContext (dwData);

	if ((! pContext) || (! pContext->hRFCOMM) ||  
				(! (pContext->uiOpenRef & (1 << iPortNum))) ||
				(! (pContext->uiReadRef & (1 << iPortNum)))) {
		SetLastError (pContext ? (pContext->hRFCOMM ? ERROR_NOT_CONNECTED : ERROR_NOT_READY) : ERROR_INVALID_HANDLE);

#if defined (DEBUG) || defined (_DEBUG) || defined (RETAILLOG)
		if (! pContext)
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Read 0x%08x : not found\n", dwData));
		else if (! pContext->hRFCOMM)
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Read 0x%08x : not setup\n", dwData));
		else if (! (pContext->uiOpenRef & (1 << iPortNum)))
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Read 0x%08x : not opened as port %d\n", dwData, iPortNum));
		else if (! (pContext->uiWriteRef & (1 << iPortNum)))
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Read 0x%08x : not opened with write access\n", dwData));
#endif

		gpPORTEMU->Unlock ();
		return -1;
	}

	int iTotalTimeout = pContext->ct.ReadTotalTimeoutConstant;
	if (pContext->ct.ReadTotalTimeoutMultiplier != MAXDWORD)
		iTotalTimeout += pContext->ct.ReadTotalTimeoutMultiplier * Len;

	int iIntervalTimeout = pContext->ct.ReadIntervalTimeout;


	DWORD cDataRead = 0;

	int iRes = ERROR_SUCCESS;

	while (pContext->pbl && (cDataRead < Len)) {
		int cBytes = BufferTotal (pContext->pbl->pb);
		if ((DWORD)cBytes > Len - cDataRead)
			cBytes = Len - cDataRead;

		BufferGetChunk (pContext->pbl->pb, cBytes, ((unsigned char *)pBuf) + cDataRead);
		cDataRead += cBytes;

		if (BufferTotal (pContext->pbl->pb) == 0) {
			BD_BUFFER_LIST *pNext = pContext->pbl->pNext;
			if (pContext->pbl->pb->pFree)
				pContext->pbl->pb->pFree (pContext->pbl->pb);
			delete pContext->pbl;
			pContext->pbl = pNext;
		} else
			SVSUTIL_ASSERT (cDataRead == Len);
	}

	pContext->iRecvQuotaUsed -= cDataRead;
	SVSUTIL_ASSERT (pContext->iRecvQuotaUsed >= 0);

	if ((! pContext->credit_fc) && pContext->fSentXon && (pContext->iRecvQuotaUsed <= pContext->dcb.XoffLim)) {
		pContext->fSentXon = FALSE;
		pContext->ucv24out &= ~MSC_FC_BIT;
		SendMSC (pContext);
	} else if (pContext->credit_fc)
		SendCredits (pContext);

	if ((! gpPORTEMU->fInitialized) || (pContext != GetContext (dwData))) {
		SetLastError (ERROR_OPERATION_ABORTED);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Read 0x%08x : ERROR_OPERATION_ABORTED\n", dwData));
		gpPORTEMU->Unlock ();

		return -1;
	}

	if (cDataRead < Len) {
		PendingIO *pio = NewPendingIO (COMM_READ, iPortNum);

		if ((! pio) || (! pio->hEvent)) {
			if (pio)
				delete pio;

			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_Read 0x%08x : Failed to create pending IO. ERROR_NO_SYSTEM_RESOURCES\n", dwData));
			SetLastError(ERROR_NO_SYSTEM_RESOURCES);
			gpPORTEMU->Unlock ();
			return -1;
		}

		pio->buffer = (unsigned char *)pBuf;
		pio->cbuffer = Len;
		pio->dwPerms = GetCurrentPermissions ();
		pio->cbuffer_used = cDataRead;
		InsertIO (pContext, pio);

		int iStartTime = GetTickCount ();

		for ( ; ; ) {
			int iobytes = pio->cbuffer_used;

			int iTimeout = ((iIntervalTimeout != 0) && (iIntervalTimeout != MAXDWORD)) ? iIntervalTimeout : iTotalTimeout - (GetTickCount () - iStartTime);

			if (iTimeout > (iTotalTimeout - ((int)GetTickCount () - iStartTime)))
				iTimeout = iTotalTimeout - ((int)GetTickCount () - iStartTime);

			if (iTimeout < 0)
				break;

			gpPORTEMU->Unlock ();
			DWORD dwRes = WaitForSingleObject (pio->hEvent, iTimeout);

			if (! gpPORTEMU) {
				SetLastError (ERROR_INTERNAL_ERROR);
				return -1;
			}

			gpPORTEMU->Lock ();
			if (dwRes != WAIT_TIMEOUT)
				break;

			if ((iobytes == pio->cbuffer_used) && pio->cbuffer_used)
				break;
		}
		iRes = pio->iIoResult;

		cDataRead = pio->cbuffer_used;

		RemoveIO (pContext, pio);

		delete pio;

		// If we are the server and were disconnected, then it actually makes sense to just act as 
		// though we were not connected (as a read would do if it were called without being connected 
		// from the beginning).
		if ((iRes == ERROR_GRACEFUL_DISCONNECT) && pContext->fLocal) {
			iRes = ERROR_SUCCESS;
		}
	}

	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] COM_Read 0x%08x %d bytes completed, %d bytes read, status %d\n", dwData, Len, cDataRead, iRes));
	gpPORTEMU->Unlock ();

	if (iRes != ERROR_SUCCESS) {
		SetLastError (iRes);
		return -1;
	} else {
#if defined (DEBUG_DUMP_RFCOMM_STREAM)
		IFDBG(DumpBuffPfx (DEBUG_RFCOMM_STREAM, L"OUT>", (unsigned char *)pBuf, cDataRead));
#endif
	}

	return cDataRead;
}

//	@func DWORD | COM_Seek | Device seek routine
//  @parm DWORD | dwOpenData | value returned from CON_Open call
//  @parm long | pos | position to seek to (relative to type)
//  @parm DWORD | type | FILE_BEGIN, FILE_CURRENT, or FILE_END
//  @rdesc	Returns current position relative to start of file, or -1 on error
//	@remark	Routine exported by a device driver.  "PRF" is the string passed
//		 in as lpszType in RegisterDevice

extern "C" DWORD COM_Seek (DWORD dwData, long pos, DWORD type)
{
	SetLastError (ERROR_NOT_SUPPORTED);

	return (DWORD)-1;
}

//	@func void | COM_PowerUp | Device powerup routine
//	@comm	Called to restore device from suspend mode.  You cannot call any
//			routines aside from those in your dll in this call.
extern "C" void COM_PowerUp(void)
{
	return;
}
//	@func void | COM_PowerDown | Device powerdown routine
//	@comm	Called to suspend device.  You cannot call any routines aside from
//			those in your dll in this call.
extern "C" void COM_PowerDown(void)
{
	return;
}

//	@func BOOL | COM_IOControl | Device IO control routine
//  @parm DWORD | dwOpenData | value returned from CON_Open call
//  @parm DWORD | dwCode | io control code to be performed
//  @parm PBYTE | pBufIn | input data to the device
//  @parm DWORD | dwLenIn | number of bytes being passed in
//  @parm PBYTE | pBufOut | output data from the device
//  @parm DWORD | dwLenOut |maximum number of bytes to receive from device
//  @parm PDWORD | pdwActualOut | actual number of bytes received from device
//  @rdesc	Returns TRUE for success, FALSE for failure
//	@remark	Routine exported by a device driver.  "PRF" is the string passed
//		in as lpszType in RegisterDevice
extern "C" BOOL COM_IOControl (DWORD dwData,
									DWORD dwIoControlCode,
									LPVOID lpInBuf,
									DWORD nInBufSize,
									LPVOID lpOutBuf,
									DWORD nOutBufSize,
									LPDWORD lpBytesReturned) {

#if (! defined (SDK_BUILD)) && defined (UNDER_CE)
	if (dwIoControlCode == IOCTL_PSL_NOTIFY) {
		PDEVICE_PSL_NOTIFY pPslPacket = (PDEVICE_PSL_NOTIFY)lpInBuf;
		if ((pPslPacket->dwSize == sizeof(DEVICE_PSL_NOTIFY)) && (pPslPacket->dwFlags == DLL_PROCESS_EXITING)){
			IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] COM_DeviceIoControl 0x%08x : PROCESS SHUTDOWN 0x%08X\n", dwData, pPslPacket->hProc));
			PORTEMU_ProcessExited ((HANDLE)pPslPacket->hProc);
		}

		return TRUE;
	}
#endif

	int iPortNum = dwData & PORTEMU_OPEN_MASK;
	dwData &= ~PORTEMU_OPEN_MASK;

	if ((iPortNum < 0) || (iPortNum >= PORTEMU_OPEN_MAX)) {
		SetLastError (ERROR_INVALID_PARAMETER);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_IOControl 0x%08x : bad port %d\n", dwData, iPortNum));
		return FALSE;
	}


	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] COM_IOControl 0x%08x\n", dwData));

	if (! gpPORTEMU) {
		SetLastError (ERROR_SERVICE_NOT_ACTIVE);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_DeviceIoControl 0x%08x : not initialized\n", dwData));
		return FALSE;
	}

	gpPORTEMU->Lock ();
	if  (! gpPORTEMU->fInitialized) {
		SetLastError (ERROR_SERVICE_NOT_ACTIVE);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_DeviceIoControl 0x%08x : not initialized\n", dwData));
		gpPORTEMU->Unlock ();
		return FALSE;
	}

	PORTEMU_CONTEXT *pContext = GetContext (dwData);

	if ((! pContext) || (! pContext->hRFCOMM) || (! (pContext->uiOpenRef & (1 << iPortNum)))) {
		SetLastError (pContext ? (pContext->hRFCOMM ? ERROR_NOT_CONNECTED : ERROR_NOT_READY) : ERROR_INVALID_HANDLE);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_DeviceIoControl 0x%08x : port not found or not opened\n", dwData));
		gpPORTEMU->Unlock ();
		return FALSE;
	}

	if ((
		(dwIoControlCode == IOCTL_SERIAL_GET_WAIT_MASK) ||
		(dwIoControlCode == IOCTL_SERIAL_SET_WAIT_MASK) ||
		(dwIoControlCode == IOCTL_SERIAL_WAIT_ON_MASK) ||
		(dwIoControlCode == IOCTL_SERIAL_GET_PROPERTIES) ||
		(dwIoControlCode == IOCTL_BLUETOOTH_GET_RFCOMM_CHANNEL) ||
		(dwIoControlCode == IOCTL_SERIAL_GET_MODEMSTATUS) ||
		(dwIoControlCode == IOCTL_SERIAL_SET_TIMEOUTS) ||
		(dwIoControlCode == IOCTL_SERIAL_GET_TIMEOUTS) ||
		((dwIoControlCode == IOCTL_SERIAL_GET_DCB) && pContext->local_dcb) ||
		((dwIoControlCode == IOCTL_SERIAL_SET_DCB) && pContext->local_dcb)
		) && pContext->fLocal) {
#if defined (DEBUG) || defined (_DEBUG) || defined (RETAILLOG)
		if (! pContext->hConnection)
			IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] performing one of select allowed operations on UNCONNECTED! port.\n"));
#endif
	} else if (! pContext->hConnection) {
		SetLastError (ERROR_NOT_CONNECTED);

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] COM_DeviceIoControl 0x%08x : port not connected\n", dwData));

		gpPORTEMU->Unlock ();

		return FALSE;
	}
	
	int iRes = ERROR_NOT_SUPPORTED;

	switch (dwIoControlCode) {
	case IOCTL_SERIAL_SET_BREAK_ON:
		IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_SET_BREAK_ON (unsupported)\n"));
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_SERIAL_SET_BREAK_OFF:
		IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_SET_BREAK_OFF (unsupported)\n"));
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_SERIAL_SET_DTR:	// == DSR
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] +IOCTL_SERIAL_SET_DTR\n"));
		pContext->ucv24out |= (MSC_RTC_BIT | MSC_DV_BIT);
		SendMSC (pContext);
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] -IOCTL_SERIAL_SET_DTR\n"));
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_SERIAL_CLR_DTR:	// == DSR
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] +IOCTL_SERIAL_CLR_DTR\n"));
		pContext->ucv24out &= ~(MSC_RTC_BIT | MSC_DV_BIT);
		SendMSC (pContext);
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] -IOCTL_SERIAL_CLR_DTR\n"));
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_SERIAL_SET_RTS: // == CTS
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] +IOCTL_SERIAL_SET_RTS\n"));
		pContext->ucv24out |= MSC_RTR_BIT;
		SendMSC (pContext);
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] -IOCTL_SERIAL_SET_RTS\n"));
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_SERIAL_CLR_RTS: // == CTS
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] +IOCTL_SERIAL_CLR_RTS\n"));
		pContext->ucv24out &= ~MSC_RTR_BIT;
		SendMSC (pContext);
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] -IOCTL_SERIAL_CLR_RTS\n"));
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_SERIAL_SET_XOFF:
		IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_SET_XOFF (unsupported)\n"));
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_SERIAL_SET_XON:
		IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_SET_XON (unsupported)\n"));
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_SERIAL_GET_WAIT_MASK:
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] IOCTL_SERIAL_GET_WAIT_MASK\n"));
		if ((nOutBufSize < sizeof(DWORD)) || (NULL == lpOutBuf) || (NULL == lpBytesReturned)) {
			IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_GET_WAIT_MASK : ERROR_INVALID_PARAMETER\n"));
			iRes = ERROR_INVALID_PARAMETER;
			break;
		}
        
		// Set The Wait Mask
		*(DWORD *)lpOutBuf = pContext->adwEnabledEvents[iPortNum];
        
		// Return the size
		*lpBytesReturned = sizeof(DWORD);
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_SERIAL_SET_WAIT_MASK:
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] IOCTL_SERIAL_SET_WAIT_MASK\n"));
		if ((nInBufSize < sizeof(DWORD)) || (NULL == lpInBuf)) {
			IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_SET_WAIT_MASK : ERROR_INVALID_PARAMETER\n"));
			iRes = ERROR_INVALID_PARAMETER;
			break;
		}

		SetCommEventMask (pContext, *(DWORD *)lpInBuf, iPortNum);
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_SERIAL_WAIT_ON_MASK:
		{
			IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] IOCTL_SERIAL_WAIT_ON_MASK\n"));
			if ((nOutBufSize < sizeof(DWORD)) || (NULL == lpOutBuf) || (NULL == lpBytesReturned)) {
				IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_WAIT_ON_MASK : ERROR_INVALID_PARAMETER\n"));
				iRes = ERROR_INVALID_PARAMETER;
				break;
			}

			// Return right away if not waiting for any events
			if (0 == pContext->adwEnabledEvents[iPortNum]) {
				*(DWORD *)lpOutBuf = 0;
				*lpBytesReturned = sizeof(DWORD);
				iRes = ERROR_INVALID_PARAMETER;
				IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] IOCTL_SERIAL_WAIT_ON_MASK (0) : ERROR_INVALID_PARAMETER\n"));
				break;
			}

			DWORD dw;
			if (dw = pContext->adwOccuredEvents[iPortNum] & pContext->adwEnabledEvents[iPortNum]) {
				*(DWORD *)lpOutBuf = dw;
				*lpBytesReturned = sizeof(DWORD);
				pContext->adwOccuredEvents[iPortNum] = 0;
				iRes = ERROR_SUCCESS;
				IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] IOCTL_SERIAL_WAIT_ON_MASK (0x%04x) : ERROR_SUCCESS\n", *(DWORD *)lpOutBuf));
				break;
			}

			PendingIO *pio = NewPendingIO (COMM_WAIT_EVENT, iPortNum);

			if ((! pio) || (! pio->hEvent)) {
				if (pio)
					delete pio;

				IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] IOCTL_SERIAL_WAIT_ON_MASK 0x%08x : Failed to create pending IO. ERROR_NO_SYSTEM_RESOURCES\n", dwData));
				iRes = ERROR_NO_SYSTEM_RESOURCES;
				break;
			}

			InsertIO (pContext, pio);

			gpPORTEMU->Unlock ();
			WaitForSingleObject (pio->hEvent, INFINITE);

			if (! gpPORTEMU) {
				SetLastError (ERROR_INTERNAL_ERROR);
				return 0;
			}

			gpPORTEMU->Lock ();
			iRes = pio->iIoResult;

			RemoveIO (pContext, pio);
	
			if (iRes == ERROR_SUCCESS) {
				*(DWORD *)lpOutBuf = pio->dwEvent;
				*lpBytesReturned = sizeof(DWORD);
			}

			delete pio;

			IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] IOCTL_SERIAL_WAIT_ON_MASK (0x%04x) : %d\n", *(DWORD *)lpOutBuf, iRes));
		}
		break;

	case IOCTL_SERIAL_GET_COMMSTATUS:
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] IOCTL_SERIAL_GET_COMMSTATUS\n"));

		if ((nOutBufSize < sizeof(SERIAL_DEV_STATUS)) || (NULL == lpOutBuf) || (NULL == lpBytesReturned)) {
			IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_GET_COMMSTATUS : ERROR_INVALID_PARAMETER\n"));
			iRes = ERROR_INVALID_PARAMETER;
			break;
		}

		memset (lpOutBuf, 0, sizeof(SERIAL_DEV_STATUS));

		{
			PendingIO *pIO = pContext->pops;
			int	cTotalBytes = 0;

			while (pIO) {
				if (pIO->op == COMM_WRITE)
					cTotalBytes += (pIO->cbuffer - pIO->cbuffer_used);

				pIO = pIO->pNext;
			}

			((SERIAL_DEV_STATUS *)lpOutBuf)->Errors = pContext->adwErr[iPortNum];
			pContext->adwErr[iPortNum] = 0;

			((SERIAL_DEV_STATUS *)lpOutBuf)->ComStat.cbInQue = pContext->iRecvQuotaUsed;
			((SERIAL_DEV_STATUS *)lpOutBuf)->ComStat.cbOutQue = cTotalBytes;
		}

		*lpBytesReturned = sizeof(SERIAL_DEV_STATUS);
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_SERIAL_GET_MODEMSTATUS:
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] IOCTL_SERIAL_GET_MODEMSTATUS\n"));

		if ((nOutBufSize < sizeof(DWORD)) || (NULL == lpOutBuf) || (NULL == lpBytesReturned)) {
			IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_GET_MODEMSTATUS : ERROR_INVALID_PARAMETER\n"));
			iRes = ERROR_INVALID_PARAMETER;
			break;
		}

		*(DWORD *)lpOutBuf = pContext->dwModemStatusIn;
		*lpBytesReturned = sizeof(DWORD);
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_SERIAL_GET_PROPERTIES:
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] IOCTL_SERIAL_GET_PROPERTIES\n"));

		if ((nOutBufSize < sizeof(COMMPROP)) || (NULL == lpOutBuf) || (NULL == lpBytesReturned)) {
			IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_GET_PROPERTIES : ERROR_INVALID_PARAMETER\n"));
			iRes = ERROR_INVALID_PARAMETER;
			break;
		}

		{
			COMMPROP *pcp = (COMMPROP *)lpOutBuf;
			memset (pcp, 0, sizeof(*pcp));

			pcp->wPacketLength      = 0xffff;
			pcp->wPacketVersion     = 0xffff;
			pcp->dwServiceMask      = SP_SERIALCOMM;
			pcp->dwReserved1        = 0;
			pcp->dwMaxTxQueue       = 16;
			pcp->dwMaxRxQueue       = 16;
			pcp->dwMaxBaud			= BAUD_115200;
			pcp->dwProvSubType      = PST_RS232;
			pcp->dwProvCapabilities = PCF_RLSD | PCF_RTSCTS | PCF_INTTIMEOUTS | PCF_TOTALTIMEOUTS;
			pcp->dwSettableBaud     = BAUD_075 | BAUD_110 | BAUD_150 | BAUD_300 | BAUD_600 |
										BAUD_1200 | BAUD_1800 | BAUD_2400 | BAUD_4800 |
										BAUD_7200 | BAUD_9600 | BAUD_14400 |
										BAUD_19200 | BAUD_38400 | BAUD_56K | BAUD_128K |
										BAUD_115200 | BAUD_57600 | BAUD_USER;
			pcp->dwSettableParams   = SP_BAUD | SP_RLSD;
			pcp->wSettableData      = DATABITS_8;
			pcp->wSettableStopParity= STOPBITS_10 | PARITY_NONE;
		}
		*lpBytesReturned = sizeof(COMMPROP);
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_SERIAL_SET_TIMEOUTS:
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] IOCTL_SERIAL_SET_TIMEOUTS\n"));

		if ((nInBufSize < sizeof(pContext->ct)) || (NULL == lpInBuf)) {
			IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_SET_TIMEOUTS : ERROR_INVALID_PARAMETER\n"));
			iRes = ERROR_INVALID_PARAMETER;
			break;
		}
		memcpy (&pContext->ct, lpInBuf, sizeof(pContext->ct));
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_SERIAL_GET_TIMEOUTS:
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] IOCTL_SERIAL_GET_TIMEOUTS\n"));

		if ((nOutBufSize < sizeof(pContext->ct)) || (NULL == lpOutBuf) || (NULL == lpBytesReturned)) {
			IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_GET_TIMEOUTS : ERROR_INVALID_PARAMETER\n"));
			iRes = ERROR_INVALID_PARAMETER;
			break;
		}
		memcpy (lpOutBuf, &pContext->ct, sizeof(pContext->ct));
		*lpBytesReturned = sizeof(pContext->ct);
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_SERIAL_PURGE:
		IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_PURGE (unsupported)\n"));
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_SERIAL_SET_QUEUE_SIZE:
		IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_SET_QUEUE_SIZE (unsupported)\n"));
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_SERIAL_IMMEDIATE_CHAR:
		IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_IMMEDIATE_CHAR (unsupported)\n"));
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_SERIAL_GET_DCB:
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] IOCTL_SERIAL_GET_DCB\n"));
		if ((nOutBufSize < sizeof(pContext->dcb)) || (NULL == lpOutBuf) || (NULL == lpBytesReturned)) {
			IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_GET_DCB : ERROR_INVALID_PARAMETER\n"));
			iRes = ERROR_INVALID_PARAMETER;
			break;
		}

		if (pContext->local_dcb) {
			memcpy (lpOutBuf, &pContext->dcb, sizeof(pContext->dcb));
			*lpBytesReturned = sizeof(pContext->dcb);
			iRes = ERROR_SUCCESS;

			break;
		}

		{
		*lpBytesReturned = 0;

		ECall *pCall = NewCall (RFCOMM_RPN, pContext);

		if (! pCall) {
			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] IOCTL_SERIAL_GET_DCB 0x%08x : Failed to create pending IO. ERROR_NO_SYSTEM_RESOURCES\n", dwData));
			iRes = ERROR_NO_SYSTEM_RESOURCES;
			break;
		}

		pCall->pNext     = pContext->pCalls;
		pContext->pCalls = pCall;

		PendingIO *pio = NewPendingIO (COMM_RPN, iPortNum);

		if ((! pio) || (! pio->hEvent)) {
			if (pio)
				delete pio;

			DeleteCall (pCall);

			iRes = ERROR_NO_SYSTEM_RESOURCES;

			IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] IOCTL_SERIAL_GET_DCB 0x%08x : Failed to create pending IO. ERROR_NO_SYSTEM_RESOURCES\n", dwData));
			break;
		}

		InsertIO (pContext, pio);

		RFCOMM_RPNREQ_In pCallback = pContext->rfcomm_if.rfcomm_RPNREQ_In;
		HANDLE hRFCOMM = pContext->hRFCOMM;
		HANDLE hConnection = pContext->hConnection;

		unsigned short mask = 0;

		gpPORTEMU->Unlock ();

		iRes = ERROR_INTERNAL_ERROR;
		__try {
			iRes = pCallback (hRFCOMM, pCall, hConnection, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		} __except(1) {
			IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_GET_DCB : exception in rfcomm_RPNREQ_In\n"));
		}

		if (iRes == ERROR_SUCCESS)
			WaitForSingleObject (pio->hEvent, INFINITE);

		if (! gpPORTEMU) {
			SetLastError (ERROR_INTERNAL_ERROR);
			return 0;
		}

		gpPORTEMU->Lock ();
		iRes = pio->iIoResult;

		RemoveIO (pContext, pio);
		
		delete pio;

		if (iRes == ERROR_SUCCESS) {
			memcpy (lpOutBuf, &pContext->dcb, sizeof(pContext->dcb));
			*lpBytesReturned = sizeof(pContext->dcb);
		}

		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] IOCTL_SERIAL_GET_DCB : %d\n", iRes));
		}
		break;

	case IOCTL_SERIAL_SET_DCB:
		{
			if ((nInBufSize != sizeof(DCB)) || (! lpInBuf)) {
				IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_SET_DCB : ERROR_INVALID_PARAMETER\n"));
				iRes = ERROR_INVALID_PARAMETER;
				break;
			}

			iRes = ERROR_INVALID_PARAMETER;

			DCB dcb;

			__try {
				dcb = *(DCB *)lpInBuf;
				if (pContext->local_dcb) {
					if (dcb.DCBlength == sizeof(dcb)) {
						pContext->dcb = dcb;
						iRes = ERROR_SUCCESS;
					}
				} else {
					if (VerifyDCB(&dcb))
						iRes = ERROR_SUCCESS;
				}
			} __except (1) {
				IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_SET_DCB : exception reading parameters\n"));
			}

			if (iRes != ERROR_SUCCESS)
				break;

			if (pContext->local_dcb)
				break;

			ECall *pCall = NewCall (RFCOMM_RPN, pContext);

			if (! pCall) {
				IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] IOCTL_SERIAL_SET_DCB 0x%08x : Failed to create pending IO. ERROR_NO_SYSTEM_RESOURCES\n", dwData));
				iRes = ERROR_NO_SYSTEM_RESOURCES;
				break;
			}

			pCall->pNext     = pContext->pCalls;
			pContext->pCalls = pCall;

			PendingIO *pio = NewPendingIO (COMM_RPN, iPortNum);

			if ((! pio) || (! pio->hEvent)) {
				if (pio)
					delete pio;

				DeleteCall (pCall);

				iRes = ERROR_NO_SYSTEM_RESOURCES;

				IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] IOCTL_SERIAL_SET_DCB 0x%08x : Failed to create pending IO. ERROR_NO_SYSTEM_RESOURCES\n", dwData));
				break;
			}

			InsertIO (pContext, pio);

			RFCOMM_RPNREQ_In pCallback = pContext->rfcomm_if.rfcomm_RPNREQ_In;
			HANDLE hRFCOMM = pContext->hRFCOMM;
			HANDLE hConnection = pContext->hConnection;

			unsigned short mask = RFCOMM_RPN_HAVE_BAUD | RFCOMM_RPN_HAVE_DATA | RFCOMM_RPN_HAVE_STOP |
												RFCOMM_RPN_HAVE_PARITY | RFCOMM_RPN_HAVE_PT;
			int baud = dcb.BaudRate;
			int data = dcb.ByteSize;
			int stop = dcb.StopBits;
			int parity = dcb.fParity;
			int parity_type = dcb.Parity;
			unsigned char flow = 0;
			unsigned char xon = 0;
			unsigned char xoff = 0;

			pContext->dcb = dcb;

			gpPORTEMU->Unlock ();

			iRes = ERROR_INTERNAL_ERROR;
			__try {
				iRes = pCallback (hRFCOMM, pCall, hConnection, mask, baud, data, stop, parity, parity_type, flow, xon, xoff);
			} __except(1) {
				IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_SET_DCB : exception in rfcomm_RPNREQ_In\n"));
			}

			if (iRes == ERROR_SUCCESS)
				WaitForSingleObject (pio->hEvent, INFINITE);

			if (! gpPORTEMU) {
				SetLastError (ERROR_INTERNAL_ERROR);
				return 0;
			}

			gpPORTEMU->Lock ();
			iRes = pio->iIoResult;

			RemoveIO (pContext, pio);
		
			delete pio;

			IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] IOCTL_SERIAL_SET_DCB : %d\n", iRes));
		}
		break;

	case IOCTL_SERIAL_ENABLE_IR:
		IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_ENABLE_IR (unsupported)\n"));
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_SERIAL_DISABLE_IR:
		IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_DISABLE_IR (unsupported)\n"));
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_BLUETOOTH_GET_RFCOMM_CHANNEL:
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] IOCTL_SERIAL_GET_RFCOMM_CHANNEL\n"));
		if ((nOutBufSize != sizeof(DWORD)) || (NULL == lpOutBuf) || (NULL == lpBytesReturned)) {
			IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_SERIAL_GET_RFCOMM_CHANNEL : ERROR_INVALID_PARAMETER\n"));
			iRes = ERROR_INVALID_PARAMETER;
			break;
		}
		*(DWORD *)lpOutBuf = pContext->channel;
		*lpBytesReturned = sizeof(DWORD);
		iRes = ERROR_SUCCESS;
		break;

	case IOCTL_BLUETOOTH_GET_PEER_DEVICE:
		IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] IOCTL_BLUETOOTH_GET_PEER_DEVICE\n"));
		if ((nOutBufSize != sizeof(BT_ADDR)) || (NULL == lpOutBuf) || (NULL == lpBytesReturned)) {
			IFDBG(DebugOut (DEBUG_WARN, L"[PORTEMU] IOCTL_BLUETOOTH_GET_PEER_DEVICE : ERROR_INVALID_PARAMETER\n"));
			iRes = ERROR_INVALID_PARAMETER;
			break;
		}
		*(BT_ADDR *)lpOutBuf = SET_NAP_SAP (pContext->b.NAP, pContext->b.SAP);
		*lpBytesReturned = sizeof(BT_ADDR);
		iRes = ERROR_SUCCESS;
		break;
	}

	IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[PORTEMU] COM_IOControl 0x%08x : result %d\n", dwData, iRes));

	gpPORTEMU->Unlock ();

	if (iRes != ERROR_SUCCESS) {
		SetLastError (iRes);
		return FALSE;
	}

	return TRUE;
}


//
//	Driver support
//
int portemu_InitializeOnce (void) {
	SVSUTIL_ASSERT (! gpPORTEMU);
	gpPORTEMU = new PORTEMU;

	if (! gpPORTEMU)
		return ERROR_OUTOFMEMORY;

	gpPORTEMU->iDefaultMTUMax = PORTEMU_MTUMAX;
	gpPORTEMU->iDefaultMTUMin = PORTEMU_MTUMIN;
	gpPORTEMU->iDefaultRecvQuota = PORTEMU_RECVMAX;
	gpPORTEMU->iDefaultSendQuota = PORTEMU_SENDMAX;

	return ERROR_SUCCESS;
}

int portemu_CreateDriverInstance (void) {
	IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"[PORTEMU] portemu_CreateDriverInstance\n"));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_CreateDriverInstance ERROR_SERVICE_DISABLED\n"));
		return ERROR_SERVICE_DISABLED;
	}

	gpPORTEMU->Lock ();
	if (gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_CreateDriverInstance ERROR_SERVICE_ALREADY_RUNNING\n"));
		gpPORTEMU->Unlock ();
		return ERROR_SERVICE_ALREADY_RUNNING;
	}

	gpPORTEMU->pfmdPBL = svsutil_AllocFixedMemDescr (sizeof(BD_BUFFER_LIST), 10);
	gpPORTEMU->pfmdCalls = svsutil_AllocFixedMemDescr (sizeof(ECall), 10);

	if ((! gpPORTEMU->pfmdPBL) || (! gpPORTEMU->pfmdCalls)) {
		if (gpPORTEMU->pfmdPBL)
			svsutil_ReleaseFixedEmpty (gpPORTEMU->pfmdPBL);

		if (gpPORTEMU->pfmdCalls)
			svsutil_ReleaseFixedEmpty (gpPORTEMU->pfmdCalls);

		gpPORTEMU->pfmdPBL = NULL;
		gpPORTEMU->pfmdCalls = NULL;

		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_CreateDriverInstance ERROR_OUTOFMEMORY\n"));
		gpPORTEMU->Unlock ();
		return ERROR_OUTOFMEMORY;
	}

	SDP_EVENT_INDICATION sdpEI;
	sdpEI.sdp_StackEvent       = NULL;


	SDP_CALLBACKS sdpC;
	memset (&sdpC, 0, sizeof(sdpC));

	sdpC.sdp_Connect_Out                 = rfcommsdp_connect_out; 
	sdpC.sdp_Disconnect_Out              = rfcommsdp_disconnect_out;
	sdpC.sdp_ServiceAttributeSearch_Out  = rfcommsdp_service_attribute_search_out;

	int iRes = SDP_EstablishDeviceContext(gpPORTEMU, &sdpEI, &sdpC, &gpPORTEMU->sdp_if,&gpPORTEMU->hSDP);

	if (!(iRes == ERROR_SUCCESS && gpPORTEMU->sdp_if.sdp_Connect_In && gpPORTEMU->sdp_if.sdp_Disconnect_In && gpPORTEMU->sdp_if.sdp_ServiceAttributeSearch_In)) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_CreateDriverInstance::Inadequate SDP support, error %d\n", iRes));
		SDP_CloseDeviceContext(gpPORTEMU->hSDP);
		gpPORTEMU->hSDP = NULL;

		svsutil_ReleaseFixedEmpty (gpPORTEMU->pfmdPBL);
		svsutil_ReleaseFixedEmpty (gpPORTEMU->pfmdCalls);
		gpPORTEMU->pfmdPBL = NULL;
		gpPORTEMU->pfmdCalls = NULL;

		gpPORTEMU->Unlock ();
		return (iRes == ERROR_SUCCESS) ? ERROR_INTERNAL_ERROR : iRes;
	}

	SVSUTIL_ASSERT(gpPORTEMU->hSDP);

	gpPORTEMU->fInitialized = TRUE;
	IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"[PORTEMU] portemu_CreateDriverInstance ERROR_SUCCESS\n"));
	gpPORTEMU->Unlock ();

	return ERROR_SUCCESS;
}

int portemu_CloseDriverInstance (void) {
	IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"[PORTEMU] portemu_CloseDriverInstance\n"));

	if (! gpPORTEMU) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_CloseDriverInstance ERROR_SERVICE_DISABLED\n"));
		return ERROR_SERVICE_DISABLED;
	}

	gpPORTEMU->Lock ();
	if (! gpPORTEMU->fInitialized) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[PORTEMU] portemu_CloseDriverInstance ERROR_SERVICE_DISABLED\n"));
		gpPORTEMU->Unlock ();
		return ERROR_SERVICE_DISABLED;
	}

	if (gpPORTEMU->hSDP) {
		SDP_CloseDeviceContext(gpPORTEMU->hSDP);
		gpPORTEMU->hSDP = NULL;
		memset(&gpPORTEMU->sdp_if, 0, sizeof(gpPORTEMU->sdp_if));
	}

	gpPORTEMU->fInitialized = FALSE;

	while (gpPORTEMU->pPorts) {
		PORTEMU_CONTEXT *pThis = gpPORTEMU->pPorts;
		gpPORTEMU->pPorts = gpPORTEMU->pPorts->pNext;

		delete pThis;
	}

	if (gpPORTEMU->pfmdPBL)
		svsutil_ReleaseFixedEmpty (gpPORTEMU->pfmdPBL);

	if (gpPORTEMU->pfmdCalls)
		svsutil_ReleaseFixedEmpty (gpPORTEMU->pfmdCalls);

	gpPORTEMU->pfmdPBL = NULL;
	gpPORTEMU->pfmdCalls = NULL;

	IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"[PORTEMU] portemu_CloseDriverInstance ERROR_SUCCESS\n"));
	gpPORTEMU->Unlock ();

	return ERROR_SUCCESS;
}

int portemu_UninitializeOnce (void) {
	SVSUTIL_ASSERT (! gpPORTEMU->pPorts);

	delete gpPORTEMU;

	return ERROR_SUCCESS;
}

void *BD_BUFFER_LIST::operator new (size_t size) {
	SVSUTIL_ASSERT (size == sizeof(BD_BUFFER_LIST));
	return svsutil_GetFixed (gpPORTEMU->pfmdPBL);
}

void BD_BUFFER_LIST::operator delete (void *ptr) {
	svsutil_FreeFixed (ptr, gpPORTEMU->pfmdPBL);
}

void *ECall::operator new (size_t size) {
	SVSUTIL_ASSERT (size == sizeof(ECall));
	return svsutil_GetFixed (gpPORTEMU->pfmdCalls);
}

void ECall::operator delete (void *ptr) {
	svsutil_FreeFixed (ptr, gpPORTEMU->pfmdCalls);
}


//
//	BT Serial Port emulation
//

extern "C" BOOL BSP_IOControl (DWORD dwData,
									DWORD dwIoControlCode,
									LPVOID lpInBuf,
									DWORD nInBufSize,
									LPVOID lpOutBuf,
									DWORD nOutBufSize,
									LPDWORD lpBytesReturned) {
	return COM_IOControl (dwData, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
}

extern "C" DWORD BSP_Init (DWORD dwInfo) {
	return COM_Init (dwInfo);
}

extern "C" BOOL BSP_Deinit (DWORD dwData) {
	return COM_Deinit (dwData);
}

extern "C" DWORD BSP_Open (DWORD dwData, DWORD dwAccess, DWORD dwShareMode) {
	return COM_Open (dwData, dwAccess, dwShareMode);
}

extern "C" BOOL BSP_Close (DWORD dwData)  {
	return COM_Close (dwData);
}

extern "C" DWORD BSP_Write (DWORD dwData, LPCVOID pInBuf, DWORD dwInLen) {
	return COM_Write (dwData, pInBuf, dwInLen);
}

extern "C" DWORD BSP_Read (DWORD dwData, LPVOID pBuf, DWORD Len) {
	return COM_Read (dwData, pBuf, Len);
}

extern "C" DWORD BSP_Seek (DWORD dwData, long pos, DWORD type) {
	return COM_Seek (dwData, pos, type);
}

extern "C" void BSP_PowerUp (void) {
	COM_PowerUp ();
}

extern "C" void BSP_PowerDown (void) {
	COM_PowerDown ();
}

	

//
//	Console output
//
#if defined (BTH_CONSOLE)

int portemu_ProcessConsoleCommand (WCHAR *pszCommand) {
	if (! gpPORTEMU) {
		DebugOut (DEBUG_OUTPUT, L"RFCOMM help : service not initialized\n");
		return ERROR_SUCCESS;
	}

	gpPORTEMU->Lock ();
	int iRes = ERROR_SUCCESS;
	__try {
		if (wcsicmp (pszCommand, L"help") == 0) {
			DebugOut (DEBUG_OUTPUT, L"PORTEMU Commands:\n");
			DebugOut (DEBUG_OUTPUT, L"    help        prints this text\n");
			DebugOut (DEBUG_OUTPUT, L"    global      dumps global state\n");
			DebugOut (DEBUG_OUTPUT, L"    ports       dumps currently installed PORTEMU clients\n");
		} else if (wcsicmp (pszCommand, L"global") == 0) {
			DebugOut (DEBUG_OUTPUT, L"Stage :             %sinitialized\n", gpPORTEMU->fInitialized ? L"" : L"un");
			DebugOut (DEBUG_OUTPUT, L"Buffer list descr : 0x%08x\n", gpPORTEMU->pfmdPBL);
			DebugOut (DEBUG_OUTPUT, L"Calls descr :       0x%08x\n", gpPORTEMU->pfmdCalls);
			DebugOut (DEBUG_OUTPUT, L"PIO blocks used :   %d\n", gpPORTEMU->iPIOBlocksUsed);
			PORTEMU_CONTEXT		*pPorts = gpPORTEMU->pPorts;
			int iCount = 0;
			while (pPorts) {
				++iCount;
				pPorts = pPorts->pNext;
			}
			DebugOut (DEBUG_OUTPUT, L"Clients installed : %d\n", iCount);
			DebugOut (DEBUG_OUTPUT, L"Default min MTU :   %d\n", gpPORTEMU->iDefaultMTUMin);
			DebugOut (DEBUG_OUTPUT, L"Default max MTU :   %d\n", gpPORTEMU->iDefaultMTUMax);
			DebugOut (DEBUG_OUTPUT, L"Default recv q :    %d\n", gpPORTEMU->iDefaultRecvQuota);
			DebugOut (DEBUG_OUTPUT, L"Default send q :    %d\n", gpPORTEMU->iDefaultSendQuota);
		} else if (wcsicmp (pszCommand, L"ports") == 0) {
			PORTEMU_CONTEXT *pPort = gpPORTEMU->pPorts;
			while (pPort) {
				DebugOut (DEBUG_OUTPUT, L"Client :                0x%08x\n", pPort);
				DebugOut (DEBUG_OUTPUT, L"Channel  :              0x%02x\n", pPort->channel);
				DebugOut (DEBUG_OUTPUT, L"Target address :        %04x%08x\n", pPort->b.NAP, pPort->b.SAP);
				DebugOut (DEBUG_OUTPUT, L"Local :                 %s\n", pPort->fLocal ? L"yes" : L"no");
				DebugOut (DEBUG_OUTPUT, L"Local Open  :           %s\n", pPort->fLocalOpen ? L"yes" : L"no");
				DebugOut (DEBUG_OUTPUT, L"SDP in progress  :      %s\n", pPort->fSdpQueryOn ? L"yes" : L"no");
				DebugOut (DEBUG_OUTPUT, L"XON Sent :              %s\n", pPort->fSentXon ? L"yes" : L"no");
				DebugOut (DEBUG_OUTPUT, L"Conn handle :           0x%08x\n", pPort->hConnection);
				DebugOut (DEBUG_OUTPUT, L"RFCOMM handle :         0x%08x\n", pPort->hRFCOMM);
				DebugOut (DEBUG_OUTPUT, L"Dev head :              %d\n", pPort->iDeviceHead);
				DebugOut (DEBUG_OUTPUT, L"Dev trail :             %d\n", pPort->iDeviceTrail);
				DebugOut (DEBUG_OUTPUT, L"MTU :                   %d\n", pPort->iMTU);
				DebugOut (DEBUG_OUTPUT, L"Min MTU :               %d\n", pPort->iMinMTU);
				DebugOut (DEBUG_OUTPUT, L"Max MTU :               %d\n", pPort->iMaxMTU);
				DebugOut (DEBUG_OUTPUT, L"Send Quota :            %d\n", pPort->iSendQuota);
				DebugOut (DEBUG_OUTPUT, L"Send Quota Used :       %d\n", pPort->iSendQuotaUsed);
				DebugOut (DEBUG_OUTPUT, L"Recv Quota :            %d\n", pPort->iRecvQuota);
				DebugOut (DEBUG_OUTPUT, L"Recv Quota Used :       %d\n", pPort->iRecvQuotaUsed);
				for (int i = 0 ; i < PORTEMU_OPEN_MAX ; ++i) {
					DebugOut (DEBUG_OUTPUT, L"Port %d :               %s\n", i, (pPort->uiOpenRef & (1 << i)) ? L"open" : L"closed");
					DebugOut (DEBUG_OUTPUT, L"EVENT mask[%d] :        0x%08x\n", i, pPort->adwEnabledEvents[i]);
					DebugOut (DEBUG_OUTPUT, L"EVENTS %d :             0x%08x\n", pPort->adwOccuredEvents[i]);
					DebugOut (DEBUG_OUTPUT, L"ERR %d :                0x%08x\n", pPort->adwErr[i]);
					DebugOut (DEBUG_OUTPUT, L"Owner :                 0x%08x\n", pPort->hOwnerProc[i]);
				}
				DebugOut (DEBUG_OUTPUT, L"MODEM IN:               0x%08x\n", pPort->dwModemStatusIn);
				DebugOut (DEBUG_OUTPUT, L"v24 OUT:                0x%02x\n", pPort->ucv24out);
				DebugOut (DEBUG_OUTPUT, L"FLOW:                   %s\n", pPort->fc ? L"off" : L"on");
				DebugOut (DEBUG_OUTPUT, L"FLOW AGGREGATE:         %s\n", pPort->fc_aggregate ? L"off" : L"on");
				DebugOut (DEBUG_OUTPUT, L"CREDIT FLOW:            %s\n", pPort->credit_fc ? L"on" : L"off");
				DebugOut (DEBUG_OUTPUT, L"HAVE CREDITS:           %s\n", pPort->iHaveCredits);
				DebugOut (DEBUG_OUTPUT, L"GAVE CREDITS:           %s\n", pPort->iGaveCredits);
				DebugOut (DEBUG_OUTPUT, L"CT/O ReadIntervalTimeout         %d\n", pPort->ct.ReadIntervalTimeout);
				DebugOut (DEBUG_OUTPUT, L"CT/O ReadTotalTimeoutConstant    %d\n", pPort->ct.ReadTotalTimeoutConstant);
				DebugOut (DEBUG_OUTPUT, L"CT/O ReadTotalTimeoutMultiplier  %d\n", pPort->ct.ReadTotalTimeoutMultiplier);
				DebugOut (DEBUG_OUTPUT, L"CT/O WriteTotalTimeoutConstant   %d\n", pPort->ct.WriteTotalTimeoutConstant);
				DebugOut (DEBUG_OUTPUT, L"CT/O WriteTotalTimeoutMultiplier %d\n", pPort->ct.WriteTotalTimeoutMultiplier);
				DebugOut (DEBUG_OUTPUT, L"Ops:\n");
				PendingIO *pio = pPort->pops;
				while (pio) {
					DebugOut (DEBUG_OUTPUT, L"    Op %s Buf size %d Buf used %d Buf 0x%08x Perms 0x%08x Evt 0x%08x Err 0x%08x\n",
						pio->op == COMM_OPEN ? L"Open" : (pio->op == COMM_WAIT_EVENT ? L"WaitEvent" : (pio->op == COMM_READ ? L"Read" : (pio->op == COMM_WRITE ? L"Write" : L"Unknown"))),
						pio->cbuffer, pio->cbuffer_used, pio->buffer, pio->dwPerms, pio->dwEvent, pio->dwLineError);
					pio = pio->pNext;
				}

				DebugOut (DEBUG_OUTPUT, L"Buffers:\n");
				BD_BUFFER_LIST *pbl = pPort->pbl;
				while (pbl) {
					DebugOut (DEBUG_OUTPUT, L"    Size %d Head %d Trail %d Total %d\n", pbl->pb->cSize, pbl->pb->cStart, pbl->pb->cEnd, BufferTotal (pbl->pb));
					pbl = pbl->pNext;
				}

				DebugOut (DEBUG_OUTPUT, L"Calls:\n");
				ECall *pCalls = pPort->pCalls;
				while (pCalls) {
					DebugOut (DEBUG_OUTPUT, L"    Op %s Owner 0x%08x Bytes %d\n",
						(pCalls->fWhat == RFCOMM_CONNECT ? L"Connect" : (pCalls->fWhat == RFCOMM_WRITE ? L"Write" : (pCalls->fWhat == RFCOMM_SDP ? L"SDP Query" : (pCalls->fWhat == RFCOMM_PN ? L"PN request" : L"Unknown")))), 
						pCalls->pContext, pCalls->cBytes);
					pCalls = pCalls->pNext;
				}

				DebugOut (DEBUG_OUTPUT, L"\n\n");
				pPort = pPort->pNext;
			}

		} else
			iRes = ERROR_UNKNOWN_FEATURE;
	} __except (1) {
		DebugOut (DEBUG_ERROR, L"Exception in PORTEMU dump!\n");
	}

	gpPORTEMU->Unlock ();

	return iRes;
}

#endif // BTH_CONSOLE
