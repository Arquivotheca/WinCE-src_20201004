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
//      Bluetooth HCI layer
// 
// 
// Module Name:
// 
//      hci.cxx
// 
// Abstract:
// 
//      This file implements Bluetooth HCI layer
// 
// 
//------------------------------------------------------------------------------
#include <windows.h>
#include <svsutil.hxx>

#include <bt_debug.h>
#include <bt_os.h>
#include <bt_buffer.h>
#include <bt_hcip.h>
#include <bt_ddi.h>

static void ConnectTransport (void);
static void ShutdownTransport (void);
int hci_disconnect_transport (int fFinalShutdown);

BD_BUFFER *AllocBuffer (int size);
BD_BUFFER *CopyBuffer (BD_BUFFER *pSource);
BD_BUFFER *FreeBuffer (BD_BUFFER *pBuffer);

#define HCI_BUFFER			(256 + 3)
#define HCI_MEM_SCALE		10
#define HCI_TIMER_SCALE		20000

#define DEFAULT_WORKER_THREAD_PRIORITY	132

struct HCI_CONTEXT : public SVSAllocClass {
	HCI_CONTEXT				*pNext;

	HCI_EVENT_INDICATION	ei;
	HCI_CALLBACKS			c;

	unsigned int			uiControl;

	BD_ADDR					ba;
	unsigned int			class_of_device;
	unsigned char			link_type;

	void			        *pUserContext;

	HCI_CONTEXT (void) {
		memset (this, 0, sizeof(*this));
	}
};

struct BasebandConnection {
	BasebandConnection			*pNext;

	unsigned short				connection_handle;
	BD_ADDR						ba;

	unsigned int				remote_cod;

	unsigned char				link_type;

	unsigned int				fEncrypted     : 1;
	unsigned int				fAuthenticated : 1;
	unsigned int                fMode          : 3;

	HCI_CONTEXT					*pOwner;

	int							cDataPacketsPending;
    int                         cAclDataPacketsPending;

	BD_BUFFER					*pAclPacket;
	int							cAclBytesComplete;

	~BasebandConnection (void) {
		if (pAclPacket && pAclPacket->pFree)
			pAclPacket->pFree (pAclPacket);
	}

	void *operator new (size_t iSize);
	void operator delete(void *ptr);
};

struct HCIPacket {
	HCIPacket		*pNext;

	HCI_TYPE		ePacketType;

	union {
		struct {
			unsigned short	hConnection;
			int				cTotal;
			int				cSubsPending;

			int				cCompleted;
		} DataPacketAcl;

		struct {
			unsigned short	hConnection;
			int				cTotal;
			int				cSubsPending;
		} DataPacketSco;

		struct {
			unsigned short  hConnection;
			int				cTotal;
			int				cSubsPending;
		} DataPacketU;

		struct {
			unsigned short      opCode;
			HCI_EVENT_CODE		eEvent;
			PacketMarker		m;

			int					iEventRef;
		} CommandPacket;
	} uPacket;

	HCI_CONTEXT		*pOwner;
	void			*pCallContext;

	BD_BUFFER		*pContents;

	HCIPacket (HCI_TYPE a_eType, HCI_CONTEXT *a_pOwner, void *a_pCallContext, unsigned int a_cSize) {
		IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"PACKET TRACK :: Created packet 0x%08x\n", this));
		memset (this, 0, sizeof(*this));
		ePacketType  = a_eType;
		pOwner       = a_pOwner;
		pCallContext = a_pCallContext;

		uPacket.CommandPacket.m.fMarker = BTH_MARKER_NONE;

		if (a_cSize)
			pContents = BufferAlloc (a_cSize);
		else
			pContents = NULL;
	}

	HCIPacket (HCI_TYPE a_eType, HCI_CONTEXT *a_pOwner, void *a_pCallContext, unsigned int a_cSize, BD_ADDR *pba) {
		SVSUTIL_ASSERT (a_eType == COMMAND_PACKET);

		HCIPacket::HCIPacket (a_eType, a_pOwner, a_pCallContext, a_cSize);
		uPacket.CommandPacket.m.fMarker = BTH_MARKER_ADDRESS;
		uPacket.CommandPacket.m.ba = *pba;
	}

	HCIPacket (HCI_TYPE a_eType, HCI_CONTEXT *a_pOwner, void *a_pCallContext, unsigned int a_cSize, unsigned short connection_handle) {
		SVSUTIL_ASSERT (a_eType == COMMAND_PACKET);

		HCIPacket::HCIPacket (a_eType, a_pOwner, a_pCallContext, a_cSize);
		uPacket.CommandPacket.m.fMarker = BTH_MARKER_CONNECTION;
		uPacket.CommandPacket.m.connection_handle  = connection_handle;
	}

	~HCIPacket (void) {
		IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"PACKET TRACK :: Destroyed packet 0x%08x\n", this));
		if (pContents)
			pContents->pFree (pContents);
	}

	void *operator new (size_t iSize);
	void operator delete(void *ptr);
};

enum HCI_LIFE_STAGE {
	JustCreated			= 0,
	Initializing,
	AlmostRunning,
	Connected,
	Disconnected,
	ShuttingDown,
	Error
};

struct InquiryResultList {
	InquiryResultList		*pNext;
	InquiryResultBuffer		irb;

	void *operator new (size_t iSize);
	void operator delete(void *ptr);
};

struct ConnReqData {
	ConnReqData		*pNext;
	BD_ADDR			ba;
	unsigned int	cod;

	void *operator new (size_t iSize);
	void operator delete(void *ptr);
};

int RetireCall (HCIPacket *pPacket, HCI_CONTEXT *pDeviceContext, void *pCallContext, int iError);

struct HCI : public SVSSynch {
	HCI_LIFE_STAGE		eStage;

	FixedMemDescr		*pfmdPackets;
	FixedMemDescr		*pfmdConnections;
	FixedMemDescr		*pfmdInquiryResults;
	FixedMemDescr		*pfmdConnReqData;

	HCIPacket			*pPackets;
	HCIPacket			*pPacketsSent;
	HCIPacket			*pPacketsPending;
	BasebandConnection	*pConnections;
	HCI_CONTEXT			*pContexts;

	HCI_CONTEXT			*pPeriodicInquiryOwner;

	InquiryResultList	*pInquiryResults;

	ConnReqData			*pConnReqData;

	HANDLE				hQueue;
	HANDLE              hWriterThread;
	HANDLE              hReaderThread;

	int                 iCallRefCount;

	int					cCommandPacketsAllowed;

    int                 cTotalAclDataPacketsPending;

	int					fHostFlow  : 1;
	int					fLoopback  : 1;
	int					fUnderTest : 1;
	int					fScoFlow   : 1;

	HCI_Buffer_Size		sHostBuffer;
	HCI_Buffer_Size		sDeviceBuffer;

	unsigned  __int64	llEventMask;

	int					iHardwareErrors;
	int					iReportedErrors;

	HCI_PARAMETERS		transport;

	HANDLE				hInitEvent;

	void ReInit (void) {
		eStage			= JustCreated;
		pfmdPackets		= NULL;
		pfmdConnections = NULL;

		pfmdInquiryResults = NULL;

		pPackets		= NULL;
		pPacketsSent    = NULL;
		pPacketsPending = NULL;
		pConnections    = NULL;
		pContexts       = NULL;

		pPeriodicInquiryOwner = NULL;

		pInquiryResults = NULL;

		pConnReqData 	= NULL;

		hQueue          = NULL;

		hWriterThread   = hReaderThread = NULL;

		iCallRefCount   = 0;

		cCommandPacketsAllowed = 1;

		memset (&sDeviceBuffer, 0, sizeof(sDeviceBuffer));
		memset (&sHostBuffer, 0, sizeof(sHostBuffer));
		memset (&transport, 0, sizeof(transport));

		fHostFlow  = FALSE;
		fLoopback  = FALSE;
		fUnderTest = FALSE;
		fScoFlow   = FALSE;

		llEventMask = 0;

		iHardwareErrors = 0;
		iReportedErrors = 0;

        cTotalAclDataPacketsPending = 0;

		hInitEvent = NULL;
	}

	HCI (void) {
		ReInit();
	}

	void AddRef (void) {
		++iCallRefCount;
	}

	void Release (void) {
		--iCallRefCount;
		SVSUTIL_ASSERT (iCallRefCount >= 0);
	}

	int InCall (void) {
		return iCallRefCount;
	}

	int IsStackRunning (void) {
		return (eStage == AlmostRunning) || (eStage == Connected) || (eStage == Disconnected) || (eStage == Error);
	}

	void Reset (void) {
		memset (&sHostBuffer, 0, sizeof(sHostBuffer));

		fHostFlow   = FALSE;
		fLoopback   = FALSE;
		fUnderTest  = FALSE;
		fScoFlow    = FALSE;
		llEventMask = 0;

		iHardwareErrors = 0;
		iReportedErrors = 0;

		while (pConnections) {
			BasebandConnection *pNext = pConnections->pNext;
			delete pConnections;
			pConnections = pNext;
		}

		while (pInquiryResults) {
			InquiryResultList *pNext = pInquiryResults->pNext;
			delete pInquiryResults;
			pInquiryResults = pNext;
		}

		while (pConnReqData) {
			ConnReqData *pNext = pConnReqData->pNext;
			delete pConnReqData;
			pConnReqData = pNext;
		}

		pPeriodicInquiryOwner = NULL;

		HCIPacket *pX = pPacketsPending;
		pPacketsPending = NULL;

		while (pX && (eStage == IsStackRunning ())) {
			HCIPacket *pNext = pX->pNext;
			RetireCall (pX, pX->pOwner, pX->pCallContext, ERROR_BUS_RESET);
			pX = pNext;
		}

		pX = pPacketsSent;
		pPacketsSent = NULL;

		while (pX && (eStage == IsStackRunning ())) {
			HCIPacket *pNext = pX->pNext;
			RetireCall (pX, pX->pOwner, pX->pCallContext, ERROR_BUS_RESET);
			pX = pNext;
		}
	}
};

static HCI *gpHCI = NULL;

struct InternalCommand {
	HANDLE			hEvent;
	int				fCompleted;
	unsigned char	status;
};

static void IncrHWErr (void) {
	IFDBG(DebugOut (DEBUG_WARN, L"[HCI] IncrHWErr : to %d\n", gpHCI->iHardwareErrors + 1));

	if ((++gpHCI->iHardwareErrors) > HCI_MAX_ERR) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[HCI] Shutting closed because of hardware errors (%d total)\n",
			gpHCI->iHardwareErrors));

		ShutdownTransport ();
	}
}

static int InitTrackReset (void *pCallContext, unsigned char status) {
	IFDBG(DebugOut (DEBUG_HCI_INIT, L"InitTrackReset:: status %02x\n", status));

	InternalCommand *pArgs = (InternalCommand *)pCallContext;
	pArgs->status     = status;
	pArgs->fCompleted = TRUE;
	SetEvent (pArgs->hEvent);

	return TRUE;
}

static int InitTrackReadBufferSize (void *pCallContext, unsigned char status,
						 unsigned short hci_acl_data_packet_length,
						 unsigned char hci_sco_data_packet_length,
						 unsigned short hci_total_num_acl_data_packet,
						 unsigned short hci_total_num_sco_data_packets) {
	IFDBG(DebugOut (DEBUG_HCI_INIT, L"InitTrackBufferRead:: status %02x acl len: %d packets %d sco len %d packets %d\n",
				status, hci_acl_data_packet_length, hci_total_num_acl_data_packet, hci_sco_data_packet_length,
				hci_total_num_sco_data_packets));

	if (status == 0) {
		SVSUTIL_ASSERT (gpHCI->sDeviceBuffer.ACL_Data_Packet_Length == hci_acl_data_packet_length);
		SVSUTIL_ASSERT (gpHCI->sDeviceBuffer.SCO_Data_Packet_Length == hci_sco_data_packet_length);
		SVSUTIL_ASSERT (gpHCI->sDeviceBuffer.Total_Num_ACL_Data_Packets == hci_total_num_acl_data_packet);
		SVSUTIL_ASSERT (gpHCI->sDeviceBuffer.Total_Num_SCO_Data_Packets == hci_total_num_sco_data_packets);
	}

	InternalCommand *pArgs = (InternalCommand *)pCallContext;
	pArgs->status     = status;
	pArgs->fCompleted = TRUE;
	SetEvent (pArgs->hEvent);

	return TRUE;
}

static int InitTrackReadLSP (void *pCallContext, unsigned char status, unsigned char features_mask[8]) {
	IFDBG(DebugOut (DEBUG_HCI_INIT, L"InitTrackReadLSP:: status %02x\n", status));

	if (status == 0) {
		if ((features_mask[0] & 0x20) == 0)
			gpHCI->transport.uiFlags |= HCI_FLAGS_NOROLESWITCH;
	}

	InternalCommand *pArgs = (InternalCommand *)pCallContext;
	pArgs->status     = status;
	pArgs->fCompleted = TRUE;
	SetEvent (pArgs->hEvent);

	return TRUE;
}

static int InitAborted (void *pCallContext, int iError) {
	InternalCommand *pArgs = (InternalCommand *)pCallContext;
	pArgs->fCompleted = FALSE;
	SetEvent (pArgs->hEvent);

	return TRUE;
}

static void FormatPacketNoArgs(HCIPacket *pP, unsigned short usOpCode) {
	SVSUTIL_ASSERT (pP->pContents->cSize == 3);
	SVSUTIL_ASSERT (pP->pContents->cStart == 0);
	SVSUTIL_ASSERT (pP->pContents->cEnd == pP->pContents->cSize);

	IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"FormatPacketNoArgs:: op %04x\n", usOpCode));

	pP->pContents->pBuffer[pP->pContents->cStart + 0] = usOpCode & 0xff;
	pP->pContents->pBuffer[pP->pContents->cStart + 1] = (usOpCode >> 8) & 0xff;
	pP->pContents->pBuffer[pP->pContents->cStart + 2] = 0;

	pP->uPacket.CommandPacket.opCode = usOpCode;
}

static void RetireInquiryData (void) {
	InquiryResultList *pInq = gpHCI->pInquiryResults;
	InquiryResultList *pParent = NULL;

	DWORD dwNow = GetTickCount ();

	while (pInq) {
		if ((DWORD)(dwNow - pInq->irb.dwTick) >= gpHCI->transport.uiDriftFactor) {
			if (! pParent)
				gpHCI->pInquiryResults = pInq->pNext;
			else
				pParent->pNext = pInq->pNext;

			delete pInq;

			if (! pParent)
				pInq = gpHCI->pInquiryResults;
			else
				pInq = pParent->pNext;
			continue;
		}

		pParent = pInq;
		pInq = pInq->pNext;
	}
}

//
//	Communicate stack event up
//
static void DispatchStackEvent (int iEvent, HCIEventContext *pEventContext) {
	HCI_CONTEXT *pContext = gpHCI->pContexts;
	while (pContext) {
		BT_LAYER_STACK_EVENT_IND pCallback = pContext->ei.hci_StackEvent;
		if (pCallback) {
			void *pUserContext = pContext->pUserContext;

			IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"Going into StackEvent notification\n"));
			gpHCI->AddRef ();
			gpHCI->Unlock ();

			__try {
				pCallback (pUserContext, iEvent, pEventContext);
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"[HCI] DispatchStackEvent: exception in hci_StackEvent!\n"));
			}

			gpHCI->Lock ();
			gpHCI->Release ();
			IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"Came back StackEvent notification\n"));
		}

		//	Note: this is legal because we don't deref HCI and disconnect will always wait for ref to drop
		pContext = pContext->pNext;
	}
}

//
//	Communicate stack event up
//
static void NotifyMonitors (unsigned char *pBuffer, int cSize) {
	HCI_CONTEXT *pContext = gpHCI->pContexts;

	HCIEventContext e;
	e.Event.cSize = cSize;
	e.Event.pData = pBuffer;

	while ((gpHCI->eStage == Connected) && pContext) {
		BT_LAYER_STACK_EVENT_IND pCallback = pContext->ei.hci_StackEvent;
		if ((pContext->uiControl & BTH_CONTROL_ROUTE_HARDWARE) && pCallback) {
			void *pUserContext = pContext->pUserContext;

			IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"Going into StackEvent monitor notification\n"));
			gpHCI->AddRef ();
			gpHCI->Unlock ();

			__try {
				pCallback (pUserContext, BTH_STACK_HCI_HARDWARE_EVENT, &e);
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"[HCI] NotifyMonitors: exception in hci_StackEvent!\n"));
			}

			gpHCI->Lock ();
			gpHCI->Release ();
			IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"Came back StackEvent notification\n"));
		}

		//	Note: this is legal because we don't deref HCI and disconnect will always wait for ref to drop
		pContext = pContext->pNext;
	}
}

//
//	Schedule packet
//
static void SchedulePacket (HCIPacket *pPacket) {
	if ((pPacket->ePacketType == DATA_PACKET_ACL) || (pPacket->ePacketType == DATA_PACKET_SCO))
		IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"SchedulePacket:: [call %08x device %08x] packet 0x%08x data len %d\n", pPacket->pCallContext, pPacket->pOwner, pPacket, BufferTotal (pPacket->pContents)));
	else {
		IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"SchedulePacket:: [call %08x device %08x] packet 0x%08x  op %04x\n", pPacket->pCallContext, pPacket->pOwner, pPacket, pPacket->uPacket.CommandPacket.opCode));
		SVSUTIL_ASSERT (pPacket->ePacketType == COMMAND_PACKET);

		SVSUTIL_ASSERT ((pPacket->uPacket.CommandPacket.eEvent != HCI_NO_EVENT) || (pPacket->uPacket.CommandPacket.opCode == HCI_Host_Number_Of_Completed_Packets));

		SVSUTIL_ASSERT (pPacket->pContents->cSize >= 3);
		SVSUTIL_ASSERT (pPacket->pContents->cStart == 0);
		SVSUTIL_ASSERT (pPacket->pContents->cEnd == pPacket->pContents->cSize);
		SVSUTIL_ASSERT (pPacket->pContents->pBuffer[2] + 3 == BufferTotal (pPacket->pContents));

	}

	SVSUTIL_ASSERT (gpHCI->IsLocked ());
	SVSUTIL_ASSERT (! pPacket->pNext);

	if (! gpHCI->pPackets)
		gpHCI->pPackets = pPacket;
	else {
		HCIPacket *pParent = gpHCI->pPackets;
		while (pParent->pNext)
			pParent = pParent->pNext;
		pParent->pNext = pPacket;
	}

	SetEvent (gpHCI->hQueue);
}

static int bt_custom_code_internal
(
HCI_CONTEXT		*pContext,
void			*pCallContext,
unsigned short	usOpCode,
unsigned short	cPacketSize,
unsigned char	*pPacketBody,
PacketMarker    *pMarker,
unsigned char	cEvent
) {
	SVSUTIL_ASSERT ((cEvent != HCI_NO_EVENT) || (usOpCode == HCI_Host_Number_Of_Completed_Packets));

	HCIPacket *pP = new HCIPacket  (COMMAND_PACKET, pContext, pCallContext, cPacketSize + 3);
	if (! (pP && pP->pContents)) {
		if (pP)
			delete pP;
		IFDBG(DebugOut (DEBUG_ERROR, L"bt_custom_code_internal returns ERROR_OUTOFMEMORY\n"));
		return ERROR_OUTOFMEMORY;
	}

	SVSUTIL_ASSERT (pP->pContents->cEnd == pP->pContents->cSize);
	SVSUTIL_ASSERT (pP->pContents->cStart == 0);
	SVSUTIL_ASSERT (! pP->pContents->fMustCopy);

	pP->uPacket.CommandPacket.opCode = usOpCode;
	pP->uPacket.CommandPacket.eEvent = (HCI_EVENT_CODE)cEvent;
	pP->pContents->pBuffer[0] = usOpCode & 0xff;
	pP->pContents->pBuffer[1] = (usOpCode >> 8) & 0xff;
	pP->pContents->pBuffer[2] = cPacketSize & 0xff;
	if (cPacketSize)
		memcpy (pP->pContents->pBuffer + 3, pPacketBody, cPacketSize);

	if (pMarker)
		pP->uPacket.CommandPacket.m = *pMarker;

	SchedulePacket (pP);

	return ERROR_SUCCESS;
}

static HCIPacket *FindCommandPacket (HCIPacket *pList, unsigned short usOpCode) {
	HCIPacket *pPacket = pList;
	while (pPacket) {
		if (pPacket->ePacketType == COMMAND_PACKET) {
			SVSUTIL_ASSERT (pPacket->pContents->cSize >= 3);
			SVSUTIL_ASSERT (pPacket->pContents->cStart == 0);
		SVSUTIL_ASSERT (pPacket->pContents->cEnd == pPacket->pContents->cSize);
			SVSUTIL_ASSERT (pPacket->pContents->pBuffer[2] + 3 == BufferTotal (pPacket->pContents));
		}

		if ((pPacket->ePacketType == COMMAND_PACKET) && (usOpCode == pPacket->uPacket.CommandPacket.opCode)) {
			SVSUTIL_ASSERT (pPacket->uPacket.CommandPacket.m.fMarker == BTH_MARKER_NONE);
			return pPacket;
		}

		pPacket = pPacket->pNext;
	}

	return NULL;
}

static HCIPacket *ExtractCommandPacketByEvent (HCIPacket **pList, unsigned char cEvent) {
	HCIPacket *pPacket = *pList;
	HCIPacket *pParent = NULL;
	while (pPacket) {
		if (pPacket->ePacketType == COMMAND_PACKET) {
			SVSUTIL_ASSERT (pPacket->pContents->cSize >= 3);
			SVSUTIL_ASSERT (pPacket->pContents->cStart == 0);
			SVSUTIL_ASSERT (pPacket->pContents->cEnd == pPacket->pContents->cSize);
			SVSUTIL_ASSERT (pPacket->pContents->pBuffer[2] + 3 == BufferTotal (pPacket->pContents));
		}

		if ((pPacket->ePacketType == COMMAND_PACKET) && (pPacket->uPacket.CommandPacket.eEvent == cEvent)) {
			if (pParent)
				pParent->pNext = pPacket->pNext;
			else {
				SVSUTIL_ASSERT (pPacket == *pList);
				*pList = (*pList)->pNext;
			}
			return pPacket;
		}

		pParent = pPacket;
		pPacket = pPacket->pNext;
	}

	return NULL;
}

static HCIPacket *ExtractCommandPacket (HCIPacket **pList, unsigned short usOpCode, int fIgnoreMarker = FALSE) {
	HCIPacket *pPacket = *pList;
	HCIPacket *pParent = NULL;
	while (pPacket) {
		if (pPacket->ePacketType == COMMAND_PACKET) {
			SVSUTIL_ASSERT (pPacket->pContents->cSize >= 3);
			SVSUTIL_ASSERT (pPacket->pContents->cStart == 0);
			SVSUTIL_ASSERT (pPacket->pContents->cEnd == pPacket->pContents->cSize);
			SVSUTIL_ASSERT (pPacket->pContents->pBuffer[2] + 3 == BufferTotal (pPacket->pContents));
		}

		if ((pPacket->ePacketType == COMMAND_PACKET) && (usOpCode == pPacket->uPacket.CommandPacket.opCode)) {
			SVSUTIL_ASSERT (fIgnoreMarker || (pPacket->uPacket.CommandPacket.m.fMarker == BTH_MARKER_NONE));
			if (pParent)
				pParent->pNext = pPacket->pNext;
			else {
				SVSUTIL_ASSERT (pPacket == *pList);
				*pList = (*pList)->pNext;
			}
			return pPacket;
		}

		pParent = pPacket;
		pPacket = pPacket->pNext;
	}

	return NULL;
}

static HCIPacket *FindCommandPacket (HCIPacket *pList, unsigned short usOpCode, unsigned connection_handle) {
	HCIPacket *pPacket = pList;
	HCIPacket *pParent = NULL;
	while (pPacket) {
		if (pPacket->ePacketType == COMMAND_PACKET) {
			SVSUTIL_ASSERT (pPacket->pContents->cSize >= 3);
			SVSUTIL_ASSERT (pPacket->pContents->cStart == 0);
			SVSUTIL_ASSERT (pPacket->pContents->cEnd == pPacket->pContents->cSize);
			SVSUTIL_ASSERT (pPacket->pContents->pBuffer[2] + 3 == BufferTotal (pPacket->pContents));
		}


		if ((pPacket->ePacketType == COMMAND_PACKET) && (usOpCode == pPacket->uPacket.CommandPacket.opCode)) {
			SVSUTIL_ASSERT (pPacket->uPacket.CommandPacket.m.fMarker == BTH_MARKER_CONNECTION);
			if (pPacket->uPacket.CommandPacket.m.connection_handle == connection_handle)
				return pPacket;
		}

		pParent = pPacket;
		pPacket = pPacket->pNext;
	}

	return NULL;
}


static HCIPacket *ExtractCommandPacket (HCIPacket **pList, unsigned short usOpCode, unsigned short connection_handle) {
	HCIPacket *pPacket = *pList;
	HCIPacket *pParent = NULL;
	while (pPacket) {
		if (pPacket->ePacketType == COMMAND_PACKET) {
			SVSUTIL_ASSERT (pPacket->pContents->cSize >= 3);
			SVSUTIL_ASSERT (pPacket->pContents->cStart == 0);
			SVSUTIL_ASSERT (pPacket->pContents->cEnd == pPacket->pContents->cSize);
			SVSUTIL_ASSERT (pPacket->pContents->pBuffer[2] + 3 == BufferTotal (pPacket->pContents));
		}

		if ((pPacket->ePacketType == COMMAND_PACKET) && (usOpCode == pPacket->uPacket.CommandPacket.opCode)) {
			SVSUTIL_ASSERT (pPacket->uPacket.CommandPacket.m.fMarker == BTH_MARKER_CONNECTION);
			if (pPacket->uPacket.CommandPacket.m.connection_handle == connection_handle) {
				if (pParent)
					pParent->pNext = pPacket->pNext;
				else {
					SVSUTIL_ASSERT (pPacket == *pList);
					*pList = (*pList)->pNext;
				}
				return pPacket;
			}
		}

		pParent = pPacket;
		pPacket = pPacket->pNext;
	}

	return NULL;
}

static HCIPacket *FindCommandPacket (HCIPacket *pList, unsigned short usOpCode, BD_ADDR *pba) {
	HCIPacket *pPacket = pList;
	HCIPacket *pParent = NULL;
	while (pPacket) {
		if (pPacket->ePacketType == COMMAND_PACKET) {
			SVSUTIL_ASSERT (pPacket->pContents->cSize >= 3);
			SVSUTIL_ASSERT (pPacket->pContents->cStart == 0);
			SVSUTIL_ASSERT (pPacket->pContents->cEnd == pPacket->pContents->cSize);
			SVSUTIL_ASSERT (pPacket->pContents->pBuffer[2] + 3 == BufferTotal (pPacket->pContents));
		}

		if ((pPacket->ePacketType == COMMAND_PACKET) && (usOpCode == pPacket->uPacket.CommandPacket.opCode)) {
			SVSUTIL_ASSERT (pPacket->uPacket.CommandPacket.m.fMarker == BTH_MARKER_ADDRESS);
			if (pPacket->uPacket.CommandPacket.m.ba == *pba)
				return pPacket;
		}

		pParent = pPacket;
		pPacket = pPacket->pNext;
	}

	return NULL;
}

static HCIPacket *ExtractCommandPacket (HCIPacket **pList, unsigned short usOpCode, BD_ADDR *pba) {
	HCIPacket *pPacket = *pList;
	HCIPacket *pParent = NULL;
	while (pPacket) {
		if (pPacket->ePacketType == COMMAND_PACKET) {
			SVSUTIL_ASSERT (pPacket->pContents->cSize >= 3);
			SVSUTIL_ASSERT (pPacket->pContents->cStart == 0);
			SVSUTIL_ASSERT (pPacket->pContents->cEnd == pPacket->pContents->cSize);
			SVSUTIL_ASSERT (pPacket->pContents->pBuffer[2] + 3 == BufferTotal (pPacket->pContents));
		}

		if ((pPacket->ePacketType == COMMAND_PACKET) && (usOpCode == pPacket->uPacket.CommandPacket.opCode)) {
			SVSUTIL_ASSERT (pPacket->uPacket.CommandPacket.m.fMarker == BTH_MARKER_ADDRESS);
			if (pPacket->uPacket.CommandPacket.m.ba == *pba) {
				if (pParent)
					pParent->pNext = pPacket->pNext;
				else {
					SVSUTIL_ASSERT (pPacket == *pList);
					*pList = (*pList)->pNext;
				}
				return pPacket;
			}
		}

		pParent = pPacket;
		pPacket = pPacket->pNext;
	}

	return NULL;
}

static BasebandConnection *FindConnection (unsigned short h) {
	BasebandConnection *pC = gpHCI->pConnections;

	while (pC && pC->connection_handle != h)
		pC = pC->pNext;

	return pC;
}

static BasebandConnection *FindConnection (BD_ADDR *pba) {
	BasebandConnection *pC = gpHCI->pConnections;

	while (pC && pC->ba != *pba)
		pC = pC->pNext;

	return pC;
}

static HCI_CONTEXT *VerifyContext (HCI_CONTEXT *pOwner) {
	HCI_CONTEXT *pRunner = gpHCI->pContexts;
	while (pRunner && (pRunner != pOwner))
		pRunner = pRunner->pNext;

	return pRunner;
}

//
//	Retire packet
//
static int RetireCall (HCIPacket *pPacket, HCI_CONTEXT *pDeviceContext, void *pCallContext, int iError) {
	SVSUTIL_ASSERT (gpHCI && gpHCI->IsLocked ());

	IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"RetireCall:: [call %08x device %08x] packet %08x\n", pCallContext, pDeviceContext, pPacket));

#if defined (DEBUG) || defined (_DEBUG)
	if (pPacket) {
		HCIPacket *pRunner = gpHCI->pPackets;
		while (pRunner) {
			SVSUTIL_ASSERT (pRunner != pPacket);
			pRunner = pRunner->pNext;
		}

		pRunner = gpHCI->pPacketsSent;
		while (pRunner) {
			SVSUTIL_ASSERT (pRunner != pPacket);
			pRunner = pRunner->pNext;
		}

		pRunner = gpHCI->pPacketsPending;
		while (pRunner) {
			SVSUTIL_ASSERT (pRunner != pPacket);
			pRunner = pRunner->pNext;
		}
	}
#endif

	HCIPacket *pParent = NULL;
	if (! pPacket) {
		pPacket = gpHCI->pPackets;
		pParent = NULL;

		while (pPacket) {
			if ((pPacket->pOwner == pDeviceContext) && (pPacket->pCallContext == pCallContext)) {
				if (! pParent)
					gpHCI->pPackets = pPacket->pNext;
				else
					pParent->pNext = pPacket->pNext;

				IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"RetireCall:: found not executed packet\n"));
				break;
			}

			pParent = pPacket;
			pPacket = pPacket->pNext;
		}
	}

	if (! pPacket) {
		pPacket = gpHCI->pPacketsPending;
		pParent = NULL;

		while (pPacket) {
			if ((pPacket->pOwner == pDeviceContext) && (pPacket->pCallContext == pCallContext)) {
				if (! pParent)
					gpHCI->pPacketsPending = pPacket->pNext;
				else
					pParent->pNext = pPacket->pNext;

				IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"RetireCall:: found pending packet\n"));
				break;
			}

			pParent = pPacket;
			pPacket = pPacket->pNext;
		}
	}

	if (! pPacket) {
		pPacket = gpHCI->pPacketsSent;
		pParent = NULL;

		while (pPacket) {
			if ((pPacket->pOwner == pDeviceContext) && (pPacket->pCallContext == pCallContext)) {
				if (! pParent)
					gpHCI->pPacketsSent = pPacket->pNext;
				else
					pParent->pNext = pPacket->pNext;

				IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"RetireCall:: found sent packet\n"));
				break;
			}

			pParent = pPacket;
			pPacket = pPacket->pNext;
		}
	}

	if (! pPacket) {
		IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"RetireCall:: ERROR_NOT_FOUND\n"));
		return ERROR_NOT_FOUND;
	}

	BT_LAYER_CALL_ABORTED pCallback = (pPacket->pOwner && VerifyContext (pPacket->pOwner)) ?
					pPacket->pOwner->c.hci_CallAborted : NULL;

	delete pPacket;

	if (pCallback) {
		gpHCI->AddRef ();
		IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"RetireCall:: going into callback\n"));
		gpHCI->Unlock ();

		__try {
			pCallback (pCallContext, iError);
		} __except (1) {
			IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"RetireCall:: exception in callback\n"));
		}

		gpHCI->Lock ();
		IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"RetireCall:: came from callback\n"));
		gpHCI->Release ();
	}

	IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"RetireCall:: ERROR_SUCCESS\n"));
	return ERROR_SUCCESS;
}

//	We don't use HCI transport offsets because data is copied anyway into the local buffer
static int FillBuffer (HCIPacket *pPacket, int cSize, unsigned char *lpBuffer) {
	if (pPacket->ePacketType == DATA_PACKET_ACL) {	// get the buffer
		// MUST pB
		int cTransfer = BufferTotal (pPacket->pContents);

		SVSUTIL_ASSERT ((cTransfer <= 0xffff) && (cTransfer > 0));
		if (cTransfer > gpHCI->sDeviceBuffer.ACL_Data_Packet_Length)
			cTransfer = gpHCI->sDeviceBuffer.ACL_Data_Packet_Length;
		if (cTransfer > cSize - 4)
			cTransfer = cSize - 4;

		*(unsigned short *)lpBuffer = pPacket->uPacket.DataPacketAcl.hConnection | ((pPacket->uPacket.DataPacketAcl.cCompleted ? 1 : 2) << 12);
		lpBuffer += 2;
		*(unsigned short *)lpBuffer = (unsigned short)cTransfer;
		lpBuffer += 2;

		int iRes = BufferGetChunk (pPacket->pContents, cTransfer, lpBuffer);
		SVSUTIL_ASSERT (iRes);

		pPacket->uPacket.DataPacketAcl.cCompleted += cTransfer;
		++pPacket->uPacket.DataPacketAcl.cSubsPending;

		return cTransfer + 4;
	} else if (pPacket->ePacketType == DATA_PACKET_SCO) {
		int cTransfer = pPacket->uPacket.DataPacketSco.cTotal;
		int cMaxScoDataLen = gpHCI->sDeviceBuffer.SCO_Data_Packet_Length;
		int cTransportMaxDataLen = gpHCI->transport.iScoWriteMaxPacketSize - 3;
		
		if ( (cTransportMaxDataLen > 0) && (cTransportMaxDataLen < cMaxScoDataLen) ) {
			cMaxScoDataLen = cTransportMaxDataLen;
		}

		SVSUTIL_ASSERT ((cTransfer <= 0xff) && (cTransfer > 0));

		if (cTransfer > cMaxScoDataLen)
			cTransfer = cMaxScoDataLen;
		if (cTransfer > cSize - 3)
			cTransfer = cSize - 3;

		SVSUTIL_ASSERT ((cTransfer <= 0xff) && (cTransfer > 0));

		*(unsigned short *)lpBuffer = pPacket->uPacket.DataPacketSco.hConnection;
		lpBuffer += 2;
		*lpBuffer++ = (unsigned char)cTransfer;

		int iRes = BufferGetChunk (pPacket->pContents, cTransfer, lpBuffer);
		SVSUTIL_ASSERT (iRes);

		pPacket->uPacket.DataPacketAcl.cSubsPending = 1;

		return cTransfer + 3;
	}

	SVSUTIL_ASSERT (pPacket->ePacketType == COMMAND_PACKET);
	SVSUTIL_ASSERT (pPacket->pContents->cStart == 0);
	SVSUTIL_ASSERT (pPacket->pContents->cEnd == pPacket->pContents->cSize);
	SVSUTIL_ASSERT (pPacket->pContents->cSize >= 3);
	SVSUTIL_ASSERT (pPacket->pContents->pBuffer[2] + 3 == BufferTotal (pPacket->pContents));

	if (BufferTotal (pPacket->pContents) > cSize) {
		SVSUTIL_ASSERT (0);
		return 0;
	}

	memcpy (lpBuffer, pPacket->pContents->pBuffer + pPacket->pContents->cStart, BufferTotal (pPacket->pContents));
	return BufferTotal (pPacket->pContents);
}

static DWORD WINAPI WriterThread (LPVOID lpvUnused) {
	IFDBG(DebugOut (DEBUG_HCI_INIT, L"WT: Started\n"));

	if (! gpHCI) {
		IFDBG(DebugOut (DEBUG_ERROR, L"WT: Totally out of sync - exiting\n"));
		return 0;
	}

	gpHCI->Lock ();

	if (gpHCI->eStage != AlmostRunning) {
		gpHCI->eStage = Error;
		ShutdownTransport ();
		IFDBG(DebugOut (DEBUG_ERROR, L"WT: Totally out of sync - exiting\n"));
		gpHCI->Unlock ();
		return 0;
	}

	HANDLE hEvent = gpHCI->hQueue;
	BD_BUFFER bPacket;
	bPacket.fMustCopy = TRUE;
	bPacket.pFree = NULL;
	bPacket.cSize = gpHCI->transport.iWriteBufferHeader + gpHCI->transport.iMaxSizeWrite + gpHCI->transport.iWriteBufferTrailer;
	bPacket.pBuffer = (unsigned char *)g_funcAlloc (bPacket.cSize, g_pvAllocData);

	if (! bPacket.pBuffer) {
		gpHCI->eStage = Error;
		ShutdownTransport ();
		IFDBG(DebugOut (DEBUG_ERROR, L"WT: No memory!\n"));
		gpHCI->Unlock ();
		return 0;
	}

	DWORD dwHciCommTimeout = gpHCI->transport.uiWriteTimeout;

	gpHCI->Unlock ();

	IFDBG(DebugOut (DEBUG_HCI_INIT, L"WT: Initialization complete - waiting for communications...\n"));

	for ( ; ; ) {
#if defined (DEBUG) || defined (_DEBUG)
		extern void L2CAPD_CheckLock (void);
		L2CAPD_CheckLock ();
#endif

		IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"Writer: go to sleep\n"));

		DWORD dwResult = WaitForSingleObject (hEvent, dwHciCommTimeout);

		IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"Writer: woke up\n"));

		if ((dwResult != WAIT_OBJECT_0) && (dwResult != WAIT_TIMEOUT))	{ // Can only be at shutdown
			g_funcFree (bPacket.pBuffer, g_pvFreeData);

			IFDBG(DebugOut (DEBUG_HCI_INIT, L"WT: Exiting - event closed at shutdown...\n"));
			return 0;
		}

		if (! gpHCI) {
			g_funcFree (bPacket.pBuffer, g_pvFreeData);

			IFDBG(DebugOut (DEBUG_HCI_INIT, L"WT: Exiting - lost global state...\n"));
			return 0;
		}

		gpHCI->Lock ();
		if ((gpHCI->eStage != AlmostRunning) && (gpHCI->eStage != Connected)) {
			g_funcFree (bPacket.pBuffer, g_pvFreeData);
			IFDBG(DebugOut (DEBUG_HCI_INIT, L"WT: Exiting - shutting down...\n"));
			gpHCI->Unlock ();
			return 0;
		}

		if (((gpHCI->cCommandPacketsAllowed <= 0) || gpHCI->pPacketsSent) && (dwResult == WAIT_TIMEOUT) && (gpHCI->eStage == Connected)) {
			gpHCI->eStage = Error;
			ShutdownTransport ();
			g_funcFree (bPacket.pBuffer, g_pvFreeData);
			IFDBG(DebugOut (DEBUG_HCI_INIT, L"WT: Exiting - command packet timeout...\n"));
			gpHCI->Unlock ();
			return 0;
		}

		if (! gpHCI->pPackets) {
			IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"Writer : no packets\n"));

			if (gpHCI->pPacketsSent || gpHCI->pPacketsPending)
				dwHciCommTimeout = gpHCI->transport.uiWriteTimeout;
			else
				dwHciCommTimeout = INFINITE;	// No activity

			gpHCI->Unlock ();
			continue;
		}

		dwHciCommTimeout = gpHCI->transport.uiWriteTimeout;

		HCIPacket *pPacket = gpHCI->pPackets;
		HCIPacket *pParent = NULL;

		while (pPacket) {	// Send the packet
			if ((pPacket->ePacketType == COMMAND_PACKET) && (gpHCI->cCommandPacketsAllowed > 0)) {
				--gpHCI->cCommandPacketsAllowed;
				break;
			} else if ((pPacket->ePacketType == DATA_PACKET_ACL) || (pPacket->ePacketType == DATA_PACKET_SCO)) {
				BasebandConnection *pC = FindConnection (pPacket->uPacket.DataPacketU.hConnection);
				if (! pC) {	// Stale, unlink
					if (pParent)
						pParent->pNext = pPacket->pNext;
					else
						gpHCI->pPackets = pPacket->pNext;

					RetireCall (pPacket, pPacket->pOwner, pPacket->pCallContext, ERROR_CONNECTION_ABORTED);
					if ((gpHCI->eStage != AlmostRunning) && (gpHCI->eStage != Connected)) {
						g_funcFree (bPacket.pBuffer, g_pvFreeData);
						gpHCI->Unlock ();
						return 0;
					}
					
					pPacket = gpHCI->pPackets;
					pParent = NULL;
					continue;
				}

				if ((pPacket->ePacketType == DATA_PACKET_ACL) && (gpHCI->cTotalAclDataPacketsPending < gpHCI->sDeviceBuffer.Total_Num_ACL_Data_Packets)) {
					++pC->cAclDataPacketsPending;
                    ++gpHCI->cTotalAclDataPacketsPending;
					break;
				}

				if (pPacket->ePacketType == DATA_PACKET_SCO) {
					SVSUTIL_ASSERT (gpHCI->transport.iScoWriteMaxPacketSize >= 0);

					if (gpHCI->fScoFlow) {
						int cLimit = gpHCI->sDeviceBuffer.Total_Num_SCO_Data_Packets;
						if (gpHCI->transport.iScoWriteNumPackets > 0) {
							cLimit = gpHCI->transport.iScoWriteNumPackets;
						}
						if (pC->cDataPacketsPending < cLimit) {
							++pC->cDataPacketsPending;
							break;
						}
					} else {
						break;
					}
				}
			}
			pParent = pPacket;
			pPacket = pPacket->pNext;
		}

		if (! pPacket) {	// Can't send anything...
			IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"Writer : can't send anything...\n"));
			gpHCI->Unlock ();
			continue;
		}			

		//
		//	Got packet, let's send... Reset bPacket
		//
		HCI_TYPE eType = pPacket->ePacketType;

		bPacket.cStart = gpHCI->transport.iWriteBufferHeader;
		bPacket.cEnd = bPacket.cStart + FillBuffer (pPacket, gpHCI->transport.iMaxSizeWrite, bPacket.pBuffer + bPacket.cStart);
	
		int fCompletedPacket = FALSE;

		if ((eType != DATA_PACKET_ACL) || (pPacket->uPacket.DataPacketAcl.cTotal == pPacket->uPacket.DataPacketAcl.cCompleted)) {
			if (! pParent)
				gpHCI->pPackets = gpHCI->pPackets->pNext;
			else
				pParent->pNext = pPacket->pNext;

			pPacket->pNext = NULL;
			if ((eType == COMMAND_PACKET) && (pPacket->uPacket.CommandPacket.eEvent == HCI_NO_EVENT))
				delete pPacket;
			else {
				if (! gpHCI->pPacketsSent)
					gpHCI->pPacketsSent = pPacket;
				else {
					pParent = gpHCI->pPacketsSent;
					while (pParent->pNext)
						pParent = pParent->pNext;
					pParent->pNext = pPacket;
				}

				fCompletedPacket = TRUE;
			}
		}

		gpHCI->Unlock ();

		if (BufferTotal (&bPacket) <= 0)
			continue;

		if (! HCI_WritePacket (eType, &bPacket)) {
			if (gpHCI) {
				gpHCI->Lock ();
				if ((gpHCI->eStage == Connected) || (gpHCI->eStage == AlmostRunning)) {
					gpHCI->eStage = Error;
					ShutdownTransport ();
				}
				gpHCI->Unlock ();
			}
			g_funcFree (bPacket.pBuffer, g_pvFreeData);
			IFDBG(DebugOut (DEBUG_HCI_INIT, L"WT: Exiting - write failed...\n"));
			return 0;
		}

		while ((eType != COMMAND_PACKET) && fCompletedPacket && gpHCI) {
			gpHCI->Lock ();
			if (gpHCI->eStage != Connected) {
				gpHCI->Unlock ();
				break;
			}

			pPacket = gpHCI->pPacketsSent;
			pParent = NULL;
			while (pPacket && (pPacket->ePacketType == COMMAND_PACKET)) {
				pParent = pPacket;
				pPacket = pPacket->pNext;
			}

			if (! pPacket) {
				gpHCI->Unlock ();
				break;
			}

			//	There's pending data packet in SentPackets.
			HCI_CONTEXT *pOwner = VerifyContext (pPacket->pOwner);
			void *pCallContext = pPacket->pCallContext;

			HCI_DataPacketDown_Out pCallback = pOwner ? pOwner->c.hci_DataPacketDown_Out : NULL;

			if (! pParent)
				gpHCI->pPacketsSent = pPacket->pNext;
			else
				pParent->pNext = pPacket->pNext;

			delete pPacket;

			if (pCallback) {
				gpHCI->AddRef ();
				IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"Data packet completed:: going into callback - 2\n"));
				gpHCI->Unlock ();

				__try {
					pCallback (pCallContext, ERROR_SUCCESS);
				} __except (1) {
					IFDBG(DebugOut (DEBUG_ERROR, L"Data packet completed:: exception in callback - 2\n"));
				}

				gpHCI->Lock ();
				IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"Data packet completed:: came from callback - 2\n"));
				gpHCI->Release ();
			}
			gpHCI->Unlock ();
		}

		SetEvent (hEvent);	// Rescan for packets...
	}

	return 0;
}

static void DataUp (BasebandConnection *pC, BD_BUFFER *pB, HCI_CONTEXT *pOwner) {
	if (! pOwner)
		pOwner = VerifyContext (pC->pOwner);

	if (! pOwner) {
		IFDBG(DebugOut (DEBUG_WARN, L"DataUp: no context for data packet!\n"));
		return;
	}

	HCI_DataPacketUp pCallback = pOwner->ei.hci_DataPacketUp;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"DataUp: no callback for data packet!\n"));
		return;
	}

	void *pUserContext = pC->pOwner->pUserContext;
	unsigned short h = pC->connection_handle;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"DataUp:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, h, pB);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"DataUp:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"DataUp:: came from callback\n"));
	gpHCI->Release ();
}

static void DataUpAclUnbuffered (BasebandConnection *pC, unsigned char boundary, unsigned char broadcast, BD_BUFFER *pB, HCI_CONTEXT *pOwner) {
	HCI_DataPacketUpAclUnbuffered pCallback = pOwner->ei.hci_DataPacketUpAclUnbuffered;

	void *pUserContext = pC->pOwner->pUserContext;
	unsigned short h = pC->connection_handle;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"DataUpAclUnbuffered:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, h, boundary, broadcast, pB);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"DataUpAclUnbuffered:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"DataUpAclUnbuffered:: came from callback\n"));
	gpHCI->Release ();
}

static void ProcessDataAcl (unsigned char *pBuffer, int cSize) {
	IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"RT: Received ACL data packet\n"));

	unsigned short	hConnection = pBuffer[0] | ((pBuffer[1] & 0xf) << 8);
	int				fStart = (pBuffer[1] >> 4) & 0x3;
	int				cLength = pBuffer[2] | (pBuffer[3] << 8);

	if ((cSize < 4) || (cLength + 4 != cSize)) {	// at least header should fit...
		IFDBG(DebugOut (DEBUG_ERROR, L"RT: Data packet too small (%d bytes)... flushed.\n", cSize));
		IncrHWErr ();
		return;
	}

	BasebandConnection *pC = FindConnection (hConnection);

	if (! pC) {
		IFDBG(DebugOut (DEBUG_WARN, L"ProcessDataAcl: Connection handle %08x does not exist... Packet flushed.\n", hConnection));
		return;
	}

	HCI_CONTEXT *pOwner = VerifyContext (pC->pOwner);

	if (pOwner && pOwner->ei.hci_DataPacketUpAclUnbuffered) {
		IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"RT: Acl packet arrived from BD_ADDR = %04x%08x - sending unbuffered...\n", pC->ba.NAP, pC->ba.SAP));
		BD_BUFFER b;

		b.fMustCopy = TRUE;
		b.pFree = BufferFree;

		b.cSize = cLength;
		b.cEnd = b.cSize;
		b.cStart = 0;

		b.pBuffer = pBuffer + 4;

		DataUpAclUnbuffered (pC, fStart, (pBuffer[1] >> 6) & 0x3, &b, pOwner);
		return;
	}

	if (fStart == 1) { // Continuation
		if (pC->pAclPacket) {
			if (cLength + pC->cAclBytesComplete > pC->pAclPacket->cSize) {	// Too many bytes...
				IFDBG(DebugOut (DEBUG_WARN, L"RT: Packet overflow from BD_ADDR = %04x%08x... Discarding everything\n", pC->ba.NAP, pC->ba.SAP));
				pC->pAclPacket->pFree (pC->pAclPacket);
				pC->pAclPacket = NULL;
				pC->cAclBytesComplete = 0;
				return;
			}

			memcpy (&pC->pAclPacket->pBuffer[pC->cAclBytesComplete], pBuffer + 4, cLength);
			pC->cAclBytesComplete += cLength;

			if (pC->cAclBytesComplete == pC->pAclPacket->cSize) { // send DataUp
				BD_BUFFER *pB = pC->pAclPacket;

				pC->cAclBytesComplete = 0;
				pC->pAclPacket = NULL;
				DataUp (pC, pB, pOwner);
			}
		} else	// ...without the beginning...
			IFDBG(DebugOut (DEBUG_WARN, L"ProcessDataAcl: Cont. arrived with no start from BD_ADDR = %04x%08x... Packet flushed\n", pC->ba.NAP, pC->ba.SAP));
		return;
	} else if (fStart != 2) {
		IFDBG(DebugOut (DEBUG_WARN, L"RT: Packet marker unknown... flushed.\n"));
		IncrHWErr ();
		return;
	}

	if (pC->pAclPacket) {	// remains are flushed
		IFDBG(DebugOut (DEBUG_WARN, L"RT: Packet overflow from BD_ADDR = %04x%08x... Discarding everything\n", pC->ba.NAP, pC->ba.SAP));
		pC->pAclPacket->pFree (pC->pAclPacket);
		pC->pAclPacket = NULL;
		pC->cAclBytesComplete = 0;
	}

					// start new packet.
	SVSUTIL_ASSERT (! pC->pAclPacket);
	SVSUTIL_ASSERT (! pC->cAclBytesComplete);

	if (cLength < 4) {
		IFDBG(DebugOut (DEBUG_WARN, L"RT: First data packet smaller (%d) than ACL header BD_ADDR = %04x%08x... Packet flushed\n", cLength, pC->ba.NAP, pC->ba.SAP));
		return;
	}

	int cAclLen = *(unsigned short *)&pBuffer[4] + 4;
	if (cAclLen == cLength) {		// Unfragmented Acl packet
		IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"RT: Acl packet (unfragmented) arrived from BD_ADDR = %04x%08x...\n", pC->ba.NAP, pC->ba.SAP));
		BD_BUFFER b;

		b.fMustCopy = TRUE;
		b.pFree = BufferFree;

		b.cSize = cLength;
		b.cEnd = b.cSize;
		b.cStart = 0;

		b.pBuffer = pBuffer + 4;

		DataUp (pC, &b, pOwner);
		return;
	}

	if (cLength > cAclLen) {	// If packet is BIGGER than ACL packet, it is really bad...
		IFDBG(DebugOut (DEBUG_ERROR, L"RT: Data packet too big (ACL: %d bytes, data: %d)... flushed.\n", cAclLen, cLength));
		IncrHWErr ();
		return;
	}

	pC->pAclPacket = BufferAlloc (cAclLen);
	if (! pC->pAclPacket) {
		IFDBG(DebugOut (DEBUG_ERROR, L"RT: OOM allocating %d bytes BD_ADDR = %04x%08x... Packet flushed\n", cAclLen, pC->ba.NAP, pC->ba.SAP));
		return;
	}

	memcpy (pC->pAclPacket->pBuffer, pBuffer + 4, cLength);
	pC->cAclBytesComplete = cLength;
}

static void ProcessDataSco (unsigned char *pBuffer, int cSize) {
	IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"RT: Received SCO data packet\n"));

	unsigned short	hConnection = pBuffer[0] | ((pBuffer[1] & 0xf) << 8);
	int				cLength = pBuffer[2];

	if ((cSize < 3) || (cLength + 3 != cSize)) {	// at least header should fit...
		IFDBG(DebugOut (DEBUG_ERROR, L"RT: Data packet too small (%d bytes)... flushed.\n", cSize));
		IncrHWErr ();
		return;
	}

	BasebandConnection *pC = FindConnection (hConnection);

	if (! pC) {
		IFDBG(DebugOut (DEBUG_WARN, L"ProcessDataSco: Connection handle %08x does not exist... Packet flushed.\n", hConnection));
		return;
	}

	IFDBG(DebugOut (DEBUG_HCI_PACKETS, L"RT: Sco packet arrived from BD_ADDR = %04x%08x...\n", pC->ba.NAP, pC->ba.SAP));

	BD_BUFFER b;

	b.fMustCopy = TRUE;
	b.pFree = BufferFree;

	b.cSize = cLength;
	b.cEnd = b.cSize;
	b.cStart = 0;

	b.pBuffer = pBuffer + 3;

	DataUp (pC, &b, NULL);
}

// Generated in response to HCI_Reject_Connection_Request,
//							HCI_Accept_Connection_Request,
//							HCI_Create_Connection,
//							HCI_Add_SCO_Connection
static void ConnectionComplete (unsigned char status, unsigned short h, BD_ADDR *pba, unsigned char link_type, unsigned char encryption_mode) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"ConnectionComplete status = %d handle = 0x%04x bd_addr = %04x%08x link = %d encrypt = %d\n", status, h, pba->NAP, pba->SAP, link_type, encryption_mode));

	HCIPacket *pPacket = gpHCI->pPacketsPending;
	HCIPacket *pParent = NULL;
	while (pPacket) {
		if (pPacket->ePacketType == COMMAND_PACKET) {
			SVSUTIL_ASSERT (pPacket->pContents->cSize >= 3);
			SVSUTIL_ASSERT (pPacket->pContents->cEnd == pPacket->pContents->cSize);
			SVSUTIL_ASSERT (pPacket->pContents->cStart == 0);
			SVSUTIL_ASSERT (pPacket->pContents->pBuffer[2] + 3 == pPacket->pContents->cSize);

			if ((pPacket->uPacket.CommandPacket.opCode == HCI_Create_Connection) ||
				(pPacket->uPacket.CommandPacket.opCode == HCI_Accept_Connection_Request) ||
				(pPacket->uPacket.CommandPacket.opCode == HCI_Reject_Connection_Request)) {
				SVSUTIL_ASSERT (pPacket->uPacket.CommandPacket.m.fMarker == BTH_MARKER_ADDRESS);
				if (*pba == pPacket->uPacket.CommandPacket.m.ba)
					break;
			} else if (pPacket->uPacket.CommandPacket.opCode == HCI_Add_SCO_Connection) {
				SVSUTIL_ASSERT (pPacket->uPacket.CommandPacket.m.fMarker == BTH_MARKER_CONNECTION);
				BasebandConnection *pC = FindConnection (pPacket->uPacket.CommandPacket.m.connection_handle);
				if (pC && (pC->ba == *pba)) // Unaligned!
					break;
			}
		}
		pParent = pPacket;
		pPacket = pPacket->pNext;
	}

	if (! pPacket) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned connection complete event (h = 0x%08x, ba = %04x%08x)\n", h, pba->NAP, pba->SAP));
		return;
	}

	unsigned int remote_cod = 0;
	
	if ((pPacket->uPacket.CommandPacket.opCode == HCI_Accept_Connection_Request) ||
		(pPacket->uPacket.CommandPacket.opCode == HCI_Reject_Connection_Request)) {
		ConnReqData *pCR = gpHCI->pConnReqData;
		ConnReqData *pPrev = NULL;
		while (pCR && (pCR->ba != *pba) && pCR->cod) {
			pPrev = pCR;
			pCR = pCR->pNext;
		}

		if (pCR) {
			remote_cod = pCR->cod;

			if (pPrev)
				pPrev->pNext = pCR->pNext;	
			else
				gpHCI->pConnReqData = pCR->pNext;

			delete pCR;
		}
	} else if (pPacket->uPacket.CommandPacket.opCode == HCI_Create_Connection) {
		InquiryResultList *pInq = gpHCI->pInquiryResults;
		while (pInq && (pInq->irb.ba != *pba))
			pInq = pInq->pNext;

		if (pInq)
			remote_cod = pInq->irb.class_of_device;
	}
	
	if (! pParent)
		gpHCI->pPacketsPending = gpHCI->pPacketsPending->pNext;
	else
		pParent->pNext = pPacket->pNext;

	HCI_CONTEXT *pOwner = VerifyContext (pPacket->pOwner);
	void *pCallContext = pPacket->pCallContext;

	delete pPacket;

	if (! pOwner) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned connection complete event.\n"));
		return;
	}

	if (status == 0) {
#if defined (DEBUG) || defined (_DEBUG)
		{
			BasebandConnection *pRunner = gpHCI->pConnections;
			while (pRunner) {
				SVSUTIL_ASSERT (pRunner->connection_handle != h);
				pRunner = pRunner->pNext;
			}
		}
#endif
		BasebandConnection *pNewC = new BasebandConnection;

		if (pNewC) {
			memset (pNewC, 0, sizeof(*pNewC));
			pNewC->ba = *pba;
			pNewC->pOwner = pOwner;
			pNewC->connection_handle = h;
			pNewC->remote_cod = remote_cod;
			pNewC->link_type = link_type;
			pNewC->pNext = gpHCI->pConnections;
			gpHCI->pConnections = pNewC;
		}
	}

	HCI_ConnectionCompleteEvent pCallback = pOwner->ei.hci_ConnectionCompleteEvent;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"ConnectionComplete:: no handler\n"));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"ConnectionComplete:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, pCallContext, status, h, pba, link_type, encryption_mode);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"ConnectionComplete:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"ConnectionComplete:: came from callback\n"));
	gpHCI->Release ();
}

static void InquiryComplete (unsigned char status, unsigned char num_responses) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"InquiryComplete status = %d num = %d\n", status, num_responses));

	HCIPacket *pPacket = ExtractCommandPacket (&gpHCI->pPacketsPending, HCI_Inquiry);

	HCI_CONTEXT *pOwner = NULL;
	void *pCallContext = NULL;
	if (pPacket) {
		pOwner = VerifyContext (pPacket->pOwner);
		pCallContext = pPacket->pCallContext;

		delete pPacket;
	} else
		pOwner = gpHCI->pPeriodicInquiryOwner;

	if (! pOwner) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned inquiry complete event (no owner).\n"));
		return;
	}

	HCI_InquiryCompleteEvent pCallback = pOwner->ei.hci_InquiryCompleteEvent;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"InquiryComplete:: no handler\n"));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"InquiryComplete:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, pCallContext, status);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"InquiryComplete:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"InquiryComplete:: came from callback\n"));
	gpHCI->Release ();
}

static void InquiryResult (unsigned char num_responses, unsigned char *pbuffer) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"InquiryResultEvent num = %d\n", num_responses));

	for ( ; num_responses != 0 ; --num_responses, pbuffer += 14) {
		HCIPacket *pPacket = FindCommandPacket (gpHCI->pPacketsPending, HCI_Inquiry);

		HCI_CONTEXT *pOwner = VerifyContext (pPacket ? pPacket->pOwner : gpHCI->pPeriodicInquiryOwner);
		void *pCallContext = pPacket ? pPacket->pCallContext : NULL;

		if (! pOwner) {
			IFDBG(DebugOut (DEBUG_WARN, L"Orphaned inquiry result event (no owner).\n"));
			break;
		}

		HCI_InquiryResultEvent pCallback = pOwner->ei.hci_InquiryResultEvent;

		if (! pCallback) {
			IFDBG(DebugOut (DEBUG_WARN, L"InquiryResult:: no handler\n"));
			break;
		}

		void *pUserContext = pOwner->pUserContext;

		BD_ADDR b;
		memcpy (&b, pbuffer, sizeof(BD_ADDR));

		unsigned int cod = pbuffer[9] | (pbuffer[10] << 8) | (pbuffer[11] << 16);
		unsigned short offset = pbuffer[12] | (pbuffer[13] << 8);

		InquiryResultList *pInq = gpHCI->pInquiryResults;
		while (pInq && (pInq->irb.ba != b))
			pInq = pInq->pNext;

		if (! pInq) {
			pInq = new InquiryResultList;
			if (pInq) {
				pInq->pNext = gpHCI->pInquiryResults;
				gpHCI->pInquiryResults = pInq;
			}
		}

		if (pInq) {
			pInq->irb.dwTick							= GetTickCount ();
			pInq->irb.ba								= b;
			pInq->irb.page_scan_repetition_mode			= pbuffer[6];
			pInq->irb.page_scan_period_mode				= pbuffer[7];
			pInq->irb.page_scan_mode					= pbuffer[8];
			pInq->irb.class_of_device					= cod;
			pInq->irb.clock_offset						= offset;
		}

#if defined (BTH_LONG_TERM_STATE)
		if ((pbuffer[6] != 0) || (pbuffer[7] != 0) || (pbuffer[8] != 0))
			btutil_PersistPeerInfo (&b, BTH_STATE_HCI_SCAN_MODES, pbuffer + 6);
#endif

		gpHCI->AddRef ();
		IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"InquiryResultEvent:: going into callback\n"));
		gpHCI->Unlock ();

		__try {
			pCallback (pUserContext, pCallContext, &b, pbuffer[6], pbuffer[7], pbuffer[8], cod, offset);
		} __except (1) {
			IFDBG(DebugOut (DEBUG_ERROR, L"InquiryResultEvent:: exception in callback\n"));
		}

		gpHCI->Lock ();
		IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"InquiryResultEvent:: came from callback\n"));
		gpHCI->Release ();

		if (gpHCI->eStage != Connected)
			break;
	}
}

static void ConnectionRequest (BD_ADDR *pba, unsigned int class_of_device, unsigned char link_type) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"ConnectionRequest bd_addr = %04x%08x cod = %06x link = %d\n", pba->NAP, pba->SAP, class_of_device, link_type));

	HCI_CONTEXT *pOwner = gpHCI->pContexts;
	while (pOwner && ((0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_BY_ADDR)) || (pOwner->ba != *pba)))
		pOwner = pOwner->pNext;

	if (! pOwner) {
		pOwner = gpHCI->pContexts;
		while (pOwner && ((0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_BY_COD)) || (pOwner->class_of_device != class_of_device)))
			pOwner = pOwner->pNext;
	}

	if (! pOwner) {
		pOwner = gpHCI->pContexts;
		while (pOwner && ((0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_BY_LINKTYPE)) || (pOwner->link_type != link_type)))
			pOwner = pOwner->pNext;
	}

	if (! pOwner) {
		pOwner = gpHCI->pContexts;
		while (pOwner && (0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_ALL)))
			pOwner = pOwner->pNext;
	}

	if (! pOwner) {
		IFDBG(DebugOut (DEBUG_WARN, L"No context for connection request - ignored\n"));
		return;
	}

	HCI_ConnectionRequestEvent pCallback = pOwner->ei.hci_ConnectionRequestEvent;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"ConnectionRequest:: no handler\n"));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	ConnReqData *pCR = new ConnReqData;
	if (pCR) {
		pCR->ba = *pba;
		pCR->cod = class_of_device;

		pCR->pNext = gpHCI->pConnReqData;		
		gpHCI->pConnReqData = pCR;
	}
	
	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"ConnectionRequest:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, NULL, pba, class_of_device, link_type);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"ConnectionRequest:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"ConnectionRequest:: came from callback\n"));
	gpHCI->Release ();
}

static void DisconnectionComplete (unsigned char status, unsigned short h, unsigned char reason) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"DisconnectionComplete status = %d handle = 0x%04x reason = 0x%02x\n", status, h, reason));

	HCIPacket *pPacket = ExtractCommandPacket (&gpHCI->pPacketsPending, HCI_Disconnect, h);

	BasebandConnection *pC = FindConnection (h);

	if ((! pPacket) && (! pC)) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned disconnection complete event (h = 0x%08x)\n", h));
		return;
	}

	HCI_CONTEXT *pOwner = NULL;
	void        *pCallContext = NULL;

	if (pPacket) {
		pOwner = VerifyContext (pPacket->pOwner);

		pCallContext = pOwner ? pPacket->pCallContext : NULL;

		delete pPacket;
	}

	if ((! pOwner) && pC)
		pOwner = VerifyContext (pC->pOwner);

	if (! pOwner) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned disconnection complete event (no owner).\n"));
		return;
	}

	if ((status == 0) && pC) {
        gpHCI->cTotalAclDataPacketsPending -= pC->cAclDataPacketsPending;
        SVSUTIL_ASSERT (gpHCI->cTotalAclDataPacketsPending >= 0);

		if (gpHCI->pConnections == pC)
			gpHCI->pConnections = gpHCI->pConnections->pNext;
		else {
			BasebandConnection *pCParent = gpHCI->pConnections;
			while (pCParent && (pCParent->pNext != pC))
				pCParent = pCParent->pNext;

			if (pCParent)
				pCParent->pNext = pC->pNext;
			else
				SVSUTIL_ASSERT (0);
		}

		delete pC;

#ifdef DEBUG
        // If no connections are left we should have no packets pending
        if (! gpHCI->pConnections) {
                SVSUTIL_ASSERT (gpHCI->cTotalAclDataPacketsPending == 0);
        }
#endif
           // Remove all packets associated with this connection           
				HCIPacket *pParentTemp = NULL;
				HCIPacket *pPacketTemp = gpHCI->pPackets;
			                  
				while (pPacketTemp) {
					HCIPacket *pNext = pPacketTemp->pNext;
					
					if (pPacketTemp->uPacket.DataPacketU.hConnection == h) {
						if (pParentTemp)
							pParentTemp->pNext = pPacketTemp->pNext;
						else
							gpHCI->pPackets = pPacketTemp->pNext;

						RetireCall (pPacketTemp, pPacketTemp->pOwner, pPacketTemp->pCallContext, ERROR_CONNECTION_ABORTED);
					} else {
						pParentTemp = pPacketTemp;
					}
    										
					pPacketTemp = pNext;
				}           

	}

	HCI_DisconnectionCompleteEvent pCallback = pOwner->ei.hci_DisconnectionCompleteEvent;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"DisconnectionComplete:: no handler\n"));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"DisconnectionComplete:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, pCallContext, status, h, reason);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"DisconnectionComplete:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"DisconnectionComplete:: came from callback\n"));
	gpHCI->Release ();
}

static void AuthenticationComplete (unsigned char status, unsigned short h) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"AuthenticationComplete status = %d handle = 0x%04x\n", status, h));

	BasebandConnection *pConnection = FindConnection (h);
	if (pConnection)
		pConnection->fAuthenticated = TRUE;

	HCIPacket *pPacket = ExtractCommandPacket (&gpHCI->pPacketsPending, HCI_Authentication_Requested, h);

	HCI_CONTEXT *pOwner = NULL;
	void *pCallContext = NULL;

	if (pPacket) {
		pOwner = VerifyContext (pPacket->pOwner);
		pCallContext = pOwner && pOwner->ei.hci_AuthenticationCompleteEvent ? pPacket->pCallContext : NULL;

		delete pPacket;
	} else if (pConnection)
		pOwner = VerifyContext (pConnection->pOwner);

	if (pOwner && (! pOwner->ei.hci_AuthenticationCompleteEvent))
		pOwner = NULL;

	if (! pOwner) {
		pOwner = gpHCI->pContexts;
		while (pOwner && (0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_SECURITY)))
			pOwner = pOwner->pNext;
	}

	if (pOwner && (! pOwner->ei.hci_AuthenticationCompleteEvent))
		pOwner = NULL;

	if (! pOwner) {
		pOwner = gpHCI->pContexts;
		while (pOwner && (0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_ALL)))
			pOwner = pOwner->pNext;
	}

	if (pOwner && (! pOwner->ei.hci_AuthenticationCompleteEvent))
		pOwner = NULL;

	if (! pOwner) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned auth complete event.\n"));
		return;
	}

	HCI_AuthenticationCompleteEvent pCallback = pOwner->ei.hci_AuthenticationCompleteEvent;
	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"AuthenticationComplete:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, pCallContext, status, h);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"AuthenticationComplete:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"AuthenticationComplete:: came from callback\n"));
	gpHCI->Release ();

}

static void RemoteNameRequestComplete (unsigned char status, BD_ADDR *pba, unsigned char *pbname) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"RemoteNameRequestComplete : status = %d bd_addr = %04x%08x\n", status, pba->NAP, pba->SAP));

	HCIPacket *pPacket = ExtractCommandPacket (&gpHCI->pPacketsPending, HCI_Remote_Name_Request, pba);

	if (! pPacket) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned remote name read complete event (ba = %04x%08x)\n", pba->NAP, pba->SAP));
		return;
	}

	HCI_CONTEXT *pOwner = VerifyContext (pPacket->pOwner);
	void *pCallContext = pPacket->pCallContext;

	delete pPacket;

	if (! pOwner) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned remote name read complete event.\n"));
		return;
	}

	HCI_RemoteNameRequestCompleteEvent pCallback = pOwner->ei.hci_RemoteNameRequestCompleteEvent;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"RemoteNameRequestComplete:: no handler\n"));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"RemoteNameRequestComplete:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, pCallContext, status, pba, pbname);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"RemoteNameRequestComplete:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"RemoteNameRequestComplete:: came from callback\n"));
	gpHCI->Release ();
}

static void EncryptionChange (unsigned char status, unsigned short connection_handle, unsigned char encryption_enable) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"EncryptionChangeEvent : status = %d h = 0x%04x, ee = %d\n", status, connection_handle, encryption_enable));

	BasebandConnection *pConnection = FindConnection (connection_handle);
	if (pConnection)
		pConnection->fEncrypted = encryption_enable ? TRUE : FALSE;

	HCIPacket *pPacket = ExtractCommandPacket (&gpHCI->pPacketsPending, HCI_Set_Connection_Encryption, connection_handle);

	HCI_CONTEXT *pOwner = NULL;
	void *pCallContext = NULL;

	if (pPacket) {
		pOwner = VerifyContext (pPacket->pOwner);
		pCallContext = pOwner && pOwner->ei.hci_EncryptionChangeEvent ? pPacket->pCallContext : NULL;

		delete pPacket;
	} else if (pConnection)
		pOwner = VerifyContext (pConnection->pOwner);

	if (pOwner && (! pOwner->ei.hci_EncryptionChangeEvent))
		pOwner = NULL;

	if (! pOwner) {
		pOwner = gpHCI->pContexts;
		while (pOwner && (0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_SECURITY)))
			pOwner = pOwner->pNext;
	}

	if (pOwner && (! pOwner->ei.hci_EncryptionChangeEvent))
		pOwner = NULL;

	if (! pOwner) {
		pOwner = gpHCI->pContexts;
		while (pOwner && (0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_ALL)))
			pOwner = pOwner->pNext;
	}

	if (pOwner && (! pOwner->ei.hci_EncryptionChangeEvent))
		pOwner = NULL;


	if (! pOwner) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned encryption change event (h = 0x%04x)\n", connection_handle));
		return;
	}

	HCI_EncryptionChangeEvent pCallback = pOwner->ei.hci_EncryptionChangeEvent;
	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"EncryptionChangeEvent:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, pCallContext, status, connection_handle, encryption_enable);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"EncryptionChangeEvent:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"EncryptionChangeEvent:: came from callback\n"));
	gpHCI->Release ();
}

static void ChangeConnectionLinkKey (unsigned char status, unsigned short connection_handle) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"ChangeConnectionLinkKeyEvent : status = %d h = 0x%04x\n", status, connection_handle));

	HCIPacket *pPacket = ExtractCommandPacket (&gpHCI->pPacketsPending, HCI_Change_Connection_Link_Key, connection_handle);

	HCI_CONTEXT *pOwner = NULL;
	void *pCallContext = NULL;

	if (pPacket) {
		pOwner = VerifyContext (pPacket->pOwner);
		pCallContext = pPacket->pCallContext;

		delete pPacket;
	}

	if (! pOwner) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned change connection link key event (h = 0x%04x)\n", connection_handle));
		return;
	}

	HCI_ChangeConnectionLinkKeyCompleteEvent pCallback = pOwner->ei.hci_ChangeConnectionLinkKeyCompleteEvent;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"ChangeConnectionLinkKey:: no handler\n"));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"ChangeConnectionLinkKeyEvent:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, pCallContext, status, connection_handle);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"ChangeConnectionLinkKeyEvent:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"ChangeConnectionLinkKeyEvent:: came from callback\n"));
	gpHCI->Release ();
}

static void MasterLinkKeyComplete (unsigned char status, unsigned short h, unsigned char flag) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"MasterLinkKeyCompleteEvent : status = %d h = 0x%04x, flag = %d\n", status, h, flag));

	HCIPacket *pPacket = ExtractCommandPacket (&gpHCI->pPacketsPending, HCI_Master_Link_Key, h);

	HCI_CONTEXT *pOwner = NULL;
	void *pCallContext = NULL;

	if (pPacket) {
		pOwner = VerifyContext (pPacket->pOwner);
		pCallContext = pPacket->pCallContext;

		delete pPacket;
	} else {
		BasebandConnection *pConnection = FindConnection (h);
		if (pConnection)
			pOwner = VerifyContext (pConnection->pOwner);
	}

	if (! pOwner) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned master link key event (h = 0x%04x)\n", h));
		return;
	}

	HCI_MasterLinkKeyCompleteEvent pCallback = pOwner->ei.hci_MasterLinkKeyCompleteEvent;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"MasterLinkKeyComplete:: no handler\n"));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"MasterLinkKeyCompleteEvent:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, pCallContext, status, h, flag);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"MasterLinkKeyCompleteEvent:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"MasterLinkKeyCompleteEvent:: came from callback\n"));
	gpHCI->Release ();
}

static void ReadRemoteSupportedFeaturesComplete (unsigned char status, unsigned short h, unsigned char lmp_features[8]) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"ReadRemoteSupportedFeaturesCompleteEvent : status = %d h = 0x%04x\n", status, h));

	HCIPacket *pPacket = ExtractCommandPacket (&gpHCI->pPacketsPending, HCI_Read_Remote_Supported_Features, h);

	HCI_CONTEXT *pOwner = NULL;
	void *pCallContext = NULL;

	if (pPacket) {
		pOwner = VerifyContext (pPacket->pOwner);
		pCallContext = pPacket->pCallContext;

		delete pPacket;
	}

	if (! pOwner) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned read remote supported features complete event (h = 0x%04x)\n", h));
		return;
	}

	HCI_ReadRemoteSupportedFeaturesCompleteEvent pCallback = pOwner->ei.hci_ReadRemoteSupportedFeaturesCompleteEvent;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"ReadRemoteSupportedFeaturesComplete:: no handler\n"));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"ReadRemoteSupportedFeaturesCompleteEvent:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, pCallContext, status, h, lmp_features);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"ReadRemoteSupportedFeaturesCompleteEvent:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"ReadRemoteSupportedFeaturesCompleteEvent:: came from callback\n"));
	gpHCI->Release ();
}

static void ReadRemoteVersionInformationComplete (unsigned char status, unsigned short h, unsigned char lmp_version, unsigned short manufacturer_name, unsigned short lmp_subversion) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"ReadRemoteVersionInformationCompleteEvent : status = %d h = 0x%04x, lmp vers = 0x%02x manuf = 0x%04x sub = 0x%02x\n", status, h, lmp_version, manufacturer_name, lmp_subversion));

	HCIPacket *pPacket = ExtractCommandPacket (&gpHCI->pPacketsPending, HCI_Read_Remote_Version_Information, h);

	HCI_CONTEXT *pOwner = NULL;
	void *pCallContext = NULL;

	if (pPacket) {
		pOwner = VerifyContext (pPacket->pOwner);
		pCallContext = pPacket->pCallContext;

		delete pPacket;
	}

	if (! pOwner) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned read remote version info complete event (h = 0x%04x)\n", h));
		return;
	}

	HCI_ReadRemoteVersionInformationCompleteEvent pCallback = pOwner->ei.hci_ReadRemoteVersionInformationCompleteEvent;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"ReadRemoteVersionInformationComplete:: no handler\n"));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"ReadRemoteVersionInformationCompleteEvent:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, pCallContext, status, h, lmp_version, manufacturer_name, lmp_subversion);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"ReadRemoteVersionInformationCompleteEvent:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"ReadRemoteVersionInformationCompleteEvent:: came from callback\n"));
	gpHCI->Release ();
}

static void QoSSetupComplete (unsigned char status, unsigned short h, unsigned char flags, unsigned char service_type, unsigned int token_rate, unsigned int peak_bandwidth, unsigned int latency, unsigned int delay_variation) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"QoSSetupComplete : status = %d h = 0x%04x, flags = %d, serv = %d, rate = %d peak = %d lat = %d, var = %d\n", status, h, flags, service_type, token_rate, peak_bandwidth, latency, delay_variation));

	HCIPacket *pPacket = ExtractCommandPacket (&gpHCI->pPacketsPending, HCI_QoS_Setup, h);

	HCI_CONTEXT *pOwner = NULL;
	void *pCallContext = NULL;

	if (pPacket) {
		pOwner = VerifyContext (pPacket->pOwner);
		pCallContext = pPacket->pCallContext;

		delete pPacket;
	} else {
		BasebandConnection *pConnection = FindConnection (h);
		if (pConnection)
			pOwner = VerifyContext (pConnection->pOwner);
	}

	if (! pOwner) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned QoS Setup Complete event (h = 0x%04x)\n", h));
		return;
	}

	HCI_QoSSetupCompleteEvent pCallback = pOwner->ei.hci_QoSSetupCompleteEvent;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"QoSSetupComplete:: no handler\n"));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"QoSSetupComplete:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, pCallContext, status, h, flags, service_type, token_rate, peak_bandwidth, latency, delay_variation);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"QoSSetupComplete:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"QoSSetupComplete:: came from callback\n"));
	gpHCI->Release ();
}

typedef int (*CallbackStatus)				(void *pCallContext, unsigned char status);
typedef int (*CallbackStatusByte)			(void *pCallContext, unsigned char status, unsigned char cflag);
typedef int (*CallbackStatusShort)			(void *pCallContext, unsigned char status, unsigned short usflag);
typedef int (*CallbackStatusShortShort)		(void *pCallContext, unsigned char status, unsigned short usflag, unsigned short usflag2);
typedef int (*CallbackStatusBA)				(void *pCallContext, unsigned char status, BD_ADDR *pba);
typedef int (*CallbackStatusUI)				(void *pCallContext, unsigned char status, unsigned int);
typedef int (*CallbackStatusHandle)			(void *pCallContext, unsigned char status, unsigned short h);
typedef int (*CallbackStatusHandleByte)		(void *pCallContext, unsigned char status, unsigned short h, unsigned char ucflag);
typedef int (*CallbackStatusHandleChar)		(void *pCallContext, unsigned char status, unsigned short h, char cflag);
typedef int (*CallbackStatusHandleShort)	(void *pCallContext, unsigned char status, unsigned short h, unsigned short usflag);


static int CommandComplete (unsigned short usOpCode, int cReturnParms, unsigned char *pReturnParms) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"CommandComplete: opcode = 0x%04x\n", usOpCode));

	if (usOpCode == 0)
		return FALSE;

	unsigned char	status  = 0;
	unsigned short	h       = 0;
	unsigned char   cflag   = 0;
	unsigned short  usflag  = 0;
	unsigned short  usflag2 = 0;
	unsigned int    uiflag  = 0;
	BD_ADDR			b;

	int iStackEvent = 0;
	HCIEventContext	e;
	HCIEventContext	*pE = NULL;

	CallbackStatus				cs   = NULL;
	CallbackStatusByte			csb  = NULL;
	CallbackStatusShort			css  = NULL;
	CallbackStatusShortShort	csss = NULL;
	CallbackStatusBA			csba = NULL;
	CallbackStatusUI			csi  = NULL;
	CallbackStatusHandle		csh  = NULL;
	CallbackStatusHandleByte	cshb = NULL;
	CallbackStatusHandleChar	cshc = NULL;
	CallbackStatusHandleShort	cshs = NULL;

	HCIPacket *pPacket = NULL;

	switch (usOpCode) {
	case HCI_Inquiry_Cancel:
	case HCI_Periodic_Inquiry_Mode:
	case HCI_Exit_Periodic_Inquiry_Mode:
	case HCI_Set_Event_Mask:
	case HCI_Set_Event_Filter:
	case HCI_Write_PIN_Type:
	case HCI_Create_New_Unit_Key:
	case HCI_Change_Local_Name:
	case HCI_Write_Connection_Accept_Timeout:
	case HCI_Write_Page_Timeout:
	case HCI_Write_Scan_Enable:
	case HCI_Write_PageScan_Activity:
	case HCI_Write_InquiryScan_Activity:
	case HCI_Write_Authentication_Enable:
	case HCI_Write_Encryption_Mode:
	case HCI_Write_Class_Of_Device:
	case HCI_Write_Voice_Setting:
	case HCI_Write_SCO_Flow_Control_Enable:
	case HCI_Write_Num_Broadcast_Retransmissions:
	case HCI_Write_Hold_Mode_Activity:
	case HCI_Set_Host_Controller_To_Host_Flow_Control:
	case HCI_Host_Buffer_Size:
	case HCI_Write_Current_IAC_LAP:
	case HCI_Write_Page_Scan_Period_Mode:
	case HCI_Write_Page_Scan_Mode:
	case HCI_Write_Loopback_Mode:
	case HCI_Enable_Device_Under_Test_Mode:
		if (cReturnParms != 1)
			return TRUE;

		status = pReturnParms[0];
		pPacket = ExtractCommandPacket (&gpHCI->pPacketsSent, usOpCode);
		break;

	case HCI_Read_Scan_Enable:
	case HCI_Read_PIN_Type:
	case HCI_Write_Stored_Link_Key:
	case HCI_Read_Authentication_Enable:
	case HCI_Read_Encryption_Mode:
	case HCI_Read_Num_Broadcast_Retransmissions:
	case HCI_Read_Hold_Mode_Activity:
	case HCI_Read_Page_Scan_Period_Mode:
	case HCI_Read_Page_Scan_Mode:
	case HCI_Read_Country_Code:
	case HCI_Read_Loopback_Mode:
	case HCI_Read_Number_Of_Supported_IAC:
		if ((cReturnParms != 1) && (cReturnParms != 2))
			return TRUE;

		status = pReturnParms[0];

		if (cReturnParms == 2)
			cflag = pReturnParms[1];
		else if (status == 0)
			return TRUE;
		else
			cflag = 0;

		pPacket = ExtractCommandPacket (&gpHCI->pPacketsSent, usOpCode);
		break;

	case HCI_Write_Link_Policy_Settings:
	case HCI_Flush:
	case HCI_Write_Automatic_Flush_Timeout:
	case HCI_Write_Link_Supervision_Timeout:
	case HCI_Reset_Failed_Contact_Counter:
		if (cReturnParms != 3)
			return TRUE;

		status = pReturnParms[0];
		h = pReturnParms[1] | (pReturnParms[2] << 8);
		pPacket = ExtractCommandPacket (&gpHCI->pPacketsSent, usOpCode, h);
		break;

	case HCI_Delete_Stored_Link_Key:
	case HCI_Read_Connection_Accept_Timeout:
	case HCI_Read_Page_Timeout:
	case HCI_Read_Voice_Setting:
		if ((cReturnParms != 3) && (cReturnParms != 1))
			return TRUE;

		status = pReturnParms[0];
		if (cReturnParms == 3)
			usflag = pReturnParms[1] | (pReturnParms[2] << 8);
		else if (status == 0)
			return FALSE;
		else
			usflag = 0;

		pPacket = ExtractCommandPacket (&gpHCI->pPacketsSent, usOpCode);
		break;

	case HCI_Role_Discovery:
	case HCI_Read_Transmit_Power_Level:
	case HCI_Get_Link_Quality:
	case HCI_Read_RSSI:
		if ((cReturnParms != 4) && (cReturnParms != 3))
			return TRUE;
		status = pReturnParms[0];
		h = pReturnParms[1] | (pReturnParms[2] << 8);

		if (cReturnParms == 4)
			cflag = pReturnParms[3];
		else if (status == 0)
			return FALSE;
		else
			cflag = 0;

		pPacket = ExtractCommandPacket (&gpHCI->pPacketsSent, usOpCode, h);
		break;

	case HCI_Read_Link_Policy_Settings:
	case HCI_Read_Automatic_Flush_Timeout:
	case HCI_Read_Link_Supervision_Timeout:
	case HCI_Read_Failed_Contact_Counter:
		if ((cReturnParms != 5) && (cReturnParms != 3))
			return TRUE;

		status = pReturnParms[0];
		h = pReturnParms[1] | (pReturnParms[2] << 8);

		if (cReturnParms == 5)
			usflag = pReturnParms[3] | (pReturnParms[4] << 8);
		else if (status == 0)
			return TRUE;
		else
			usflag = 0;

		pPacket = ExtractCommandPacket (&gpHCI->pPacketsSent, usOpCode, h);
		break;

	case HCI_Read_Stored_Link_Key:
	case HCI_Read_PageScan_Activity:
	case HCI_Read_InquiryScan_Activity:
		if ((cReturnParms != 5) && (cReturnParms != 1))
			return TRUE;

		status = pReturnParms[0];

		if (cReturnParms == 5) {
			usflag = pReturnParms[1] | (pReturnParms[2] << 8);
			usflag2 = pReturnParms[3] | (pReturnParms[4] << 8);
		} else if (status == 0)
			return TRUE;
		else {
			usflag = usflag2 = 0;
		}

		pPacket = ExtractCommandPacket (&gpHCI->pPacketsSent, usOpCode);
		break;

	case HCI_Link_Key_Request_Reply:
	case HCI_Link_Key_Request_Negative_Reply:
	case HCI_PIN_Code_Request_Reply:
	case HCI_PIN_Code_Request_Negative_Reply:
		if ((cReturnParms != 7) && (status != 1))
			return TRUE;

		status = pReturnParms[0];

		if (cReturnParms == 7)
			memcpy (&b, pReturnParms + 1, sizeof(BD_ADDR));
		else if (status == 0)
			return TRUE;
		else
			memset (&b, 0, sizeof(b));

		pPacket = ExtractCommandPacket (&gpHCI->pPacketsSent, usOpCode, &b);
		break;

	case HCI_Read_BD_ADDR:
		if ((cReturnParms != 7) && (status != 1))
			return TRUE;

		status = pReturnParms[0];

		if (cReturnParms == 7)
			memcpy (&b, pReturnParms + 1, sizeof(BD_ADDR));
		else if (status == 0)
			return TRUE;
		else
			memset (&b, 0, sizeof(b));

		pPacket = ExtractCommandPacket (&gpHCI->pPacketsSent, usOpCode);
		break;

	case HCI_Read_Class_Of_Device:
		if ((cReturnParms != 4) && (cReturnParms != 1))
			return TRUE;

		status = pReturnParms[0];

		if (cReturnParms == 4)
			uiflag = pReturnParms[1] | (pReturnParms[2] << 8) | (pReturnParms[3] << 16);
		else if (status == 0)
			return TRUE;
		else
			uiflag = 0;

		pPacket = ExtractCommandPacket (&gpHCI->pPacketsSent, usOpCode);
		break;

	case HCI_Read_Current_IAC_LAP:
		if (cReturnParms == 0)
			return TRUE;

		status = pReturnParms[0];
		if ((status == 0) && (cReturnParms != (1 + 1 + 3 * pReturnParms[1])))
			return TRUE;

		pPacket = ExtractCommandPacket (&gpHCI->pPacketsSent, usOpCode);
		break;

	case HCI_Read_Local_Version_Information:
		if (cReturnParms == 0)
			return TRUE;

		status = pReturnParms[0];
		if ((status == 0) && (cReturnParms != 9))
			return TRUE;

		pPacket = ExtractCommandPacket (&gpHCI->pPacketsSent, usOpCode);
		break;

	case HCI_Read_Local_Supported_Features:
		if (cReturnParms == 0)
			return TRUE;

		status = pReturnParms[0];
		if ((status == 0) && (cReturnParms != 9))
			return TRUE;

		pPacket = ExtractCommandPacket (&gpHCI->pPacketsSent, usOpCode);
		break;

	case HCI_Read_Buffer_Size:
		if (cReturnParms == 0)
			return TRUE;

		status = pReturnParms[0];
		if ((status == 0) &&(cReturnParms != 8))
			return TRUE;

		pPacket = ExtractCommandPacket (&gpHCI->pPacketsSent, usOpCode);
		break;

	case HCI_Read_Local_Name:
		if (cReturnParms == 0)
			return TRUE;

		status = pReturnParms[0];
		if ((status == 0) && (cReturnParms != 249))
			return TRUE;

		pPacket = ExtractCommandPacket (&gpHCI->pPacketsSent, usOpCode);
		break;

	default:
		pPacket = ExtractCommandPacket (&gpHCI->pPacketsSent, usOpCode);
		break;
	}

	if (! pPacket) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned Command complete for opcode 0x%04x\n", usOpCode));
		return FALSE;
	}

	void		*pCallContext = pPacket->pCallContext;
	HCI_CONTEXT	*pOwner = VerifyContext (pPacket->pOwner);

	//
	//	Command-specific effect on a state
	//
	switch (usOpCode) {
	case HCI_Inquiry_Cancel:
		{
			if (status == 0) {
				HCIPacket *pInquiryPacket = ExtractCommandPacket (&gpHCI->pPacketsPending, HCI_Inquiry);
				if (pInquiryPacket)
					RetireCall (pInquiryPacket, pInquiryPacket->pOwner, pInquiryPacket->pCallContext, ERROR_CANCELLED);

				if (gpHCI->eStage != Connected)
					return FALSE;
			}
			break;
		}

	case HCI_Periodic_Inquiry_Mode:
		{
			if (status == 0)
				gpHCI->pPeriodicInquiryOwner = pOwner;
			break;
		}
	case HCI_Exit_Periodic_Inquiry_Mode:
		{
			if (status == 0)
				gpHCI->pPeriodicInquiryOwner = NULL;
			break;
		}

	case HCI_Set_Event_Mask:
		{
			if (status == 0) {
				SVSUTIL_ASSERT (pPacket->pContents->cStart == 0);
				SVSUTIL_ASSERT (pPacket->pContents->cEnd == pPacket->pContents->cEnd);
				memcpy (&gpHCI->llEventMask, pPacket->pContents->pBuffer + 3, sizeof(gpHCI->llEventMask));
				iStackEvent = BTH_STACK_EVENT_MASK_SET;
			}
			break;
		}

	case HCI_Reset:
		{
			if (status == 0)
				iStackEvent = BTH_STACK_RESET;
			break;
		}

	case HCI_Set_Host_Controller_To_Host_Flow_Control:
		{
			if (status == 0) {
				SVSUTIL_ASSERT (pPacket->pContents->cStart == 0);
				SVSUTIL_ASSERT (pPacket->pContents->cEnd == pPacket->pContents->cEnd);

				//	the first bit indicates whether ACL flow control is on or off
				gpHCI->fHostFlow = 1 & pPacket->pContents->pBuffer[3];

				if (gpHCI->fHostFlow)
					iStackEvent = BTH_STACK_FLOW_ON;
				else
					iStackEvent = BTH_STACK_FLOW_OFF;
			}

			break;
		}

	case HCI_Host_Buffer_Size:
		{
			if (status == 0) {
				SVSUTIL_ASSERT (pPacket->pContents->cStart == 0);
				SVSUTIL_ASSERT (pPacket->pContents->cEnd == pPacket->pContents->cEnd);

				iStackEvent = BTH_STACK_HOST_BUFFER;

				unsigned char *p = pPacket->pContents->pBuffer + 3;
				e.Host_Buffer.ACL_Data_Packet_Length = p[0] | (p[1] << 8);
				e.Host_Buffer.SCO_Data_Packet_Length = p[2];
				e.Host_Buffer.Total_Num_ACL_Data_Packets = p[3] | (p[4] << 8);
				e.Host_Buffer.Total_Num_SCO_Data_Packets = p[5];
				pE = &e;

				gpHCI->sHostBuffer = e.Host_Buffer;
			}

			break;
		}

	case HCI_Read_Buffer_Size:
		{
			if (status == 0) {
				unsigned char *p = pReturnParms + 1;
				e.Host_Buffer.ACL_Data_Packet_Length = p[0] | (p[1] << 8);
				e.Host_Buffer.SCO_Data_Packet_Length = p[2];
				e.Host_Buffer.Total_Num_ACL_Data_Packets = p[3] | (p[4] << 8);
				e.Host_Buffer.Total_Num_SCO_Data_Packets = p[5];

				gpHCI->sDeviceBuffer = e.Host_Buffer;
			}

			break;
		}

	case HCI_Enable_Device_Under_Test_Mode:
		if (status == 0) {
			gpHCI->fUnderTest = TRUE;
			iStackEvent = BTH_STACK_UNDER_TEST;
		}

		break;

	case HCI_Write_Loopback_Mode:
		if (status == 0) {
			if (pPacket->pContents->pBuffer[3] == 0) {
				gpHCI->fLoopback = FALSE;
				iStackEvent = BTH_STACK_LOOPBACK_OFF;
			} else {
				gpHCI->fLoopback = TRUE;
				iStackEvent = BTH_STACK_LOOPBACK_ON;
			}
		}
		break;

	case HCI_Write_SCO_Flow_Control_Enable:
		if (status == 0) {
			if (pPacket->pContents->pBuffer[3] == 0) {
				gpHCI->fScoFlow = FALSE;
			} else {
				gpHCI->fScoFlow = TRUE;
			}
		}
		break;

	}

	delete pPacket;

	//
	//	Callback selector
	//
	int fHaveHandler = TRUE;

	if (pOwner) {
		switch (usOpCode) {
		case HCI_Inquiry_Cancel:
			cs = pOwner->c.hci_InquiryCancel_Out;
			break;

		case HCI_Periodic_Inquiry_Mode:
			cs = pOwner->c.hci_PeriodicInquiryMode_Out;
			break;

		case HCI_Exit_Periodic_Inquiry_Mode:
			cs = pOwner->c.hci_ExitPeriodicInquiryMode_Out;
			break;

		case HCI_Link_Key_Request_Reply:
			csba = pOwner->c.hci_LinkKeyRequestReply_Out;
			break;

		case HCI_Link_Key_Request_Negative_Reply:
			csba = pOwner->c.hci_LinkKeyRequestNegativeReply_Out;
			break;

		case HCI_PIN_Code_Request_Reply:
			csba = pOwner->c.hci_PINCodeRequestReply_Out;
			break;

		case HCI_PIN_Code_Request_Negative_Reply:
			csba = pOwner->c.hci_PINCodeRequestNegativeReply_Out;
			break;

		case HCI_Role_Discovery:
			cshb = pOwner->c.hci_RoleDiscovery_Out;
			break;

		case HCI_Read_Link_Policy_Settings:
			cshs = pOwner->c.hci_ReadLinkPolicySettings_Out;
			break;

		case HCI_Write_Link_Policy_Settings:
			csh = pOwner->c.hci_WriteLinkPolicySettings_Out;
			break;

		case HCI_Set_Event_Mask:
			cs = pOwner->c.hci_SetEventMask_Out;
			break;

		case HCI_Reset:
			cs = pOwner->c.hci_Reset_Out;
			break;

		case HCI_Set_Event_Filter:
			cs = pOwner->c.hci_SetEventFilter_Out;
			break;

		case HCI_Flush:
			csh = pOwner->c.hci_Flush_Out;
			break;

		case HCI_Read_PIN_Type:
			csb = pOwner->c.hci_ReadPINType_Out;
			break;

		case HCI_Write_PIN_Type:
			cs = pOwner->c.hci_WritePINType_Out;
			break;

		case HCI_Create_New_Unit_Key:
			cs = pOwner->c.hci_CreateNewUnitKey_Out;
			break;

		case HCI_Read_Stored_Link_Key:
			csss = pOwner->c.hci_ReadStoredLinkKey_Out;
			break;

		case HCI_Write_Stored_Link_Key:
			csb = pOwner->c.hci_WriteStoredLinkKey_Out;
			break;

		case HCI_Delete_Stored_Link_Key:
			css = pOwner->c.hci_DeleteStoredLinkKey_Out;
			break;

		case HCI_Change_Local_Name:
			cs = pOwner->c.hci_ChangeLocalName_Out;
			break;

		case HCI_Read_Connection_Accept_Timeout:
			css = pOwner->c.hci_ReadConnectionAcceptTimeout_Out;
			break;

		case HCI_Write_Connection_Accept_Timeout:
			cs = pOwner->c.hci_WriteConnectionAcceptTimeout_Out;
			break;

		case HCI_Read_Page_Timeout:
			css = pOwner->c.hci_ReadPageTimeout_Out;
			break;

		case HCI_Write_Page_Timeout:
			cs = pOwner->c.hci_WritePageTimeout_Out;
			break;

		case HCI_Read_Scan_Enable:
			csb = pOwner->c.hci_ReadScanEnable_Out;
			break;

		case HCI_Write_Scan_Enable:
			cs = pOwner->c.hci_WriteScanEnable_Out;
			break;

		case HCI_Read_PageScan_Activity:
			csss = pOwner->c.hci_ReadPageScanActivity_Out;
			break;

		case HCI_Write_PageScan_Activity:
			cs = pOwner->c.hci_WritePageScanActivity_Out;
			break;

		case HCI_Read_InquiryScan_Activity:
			csss = pOwner->c.hci_ReadInquiryScanActivity_Out;
			break;

		case HCI_Write_InquiryScan_Activity:
			cs = pOwner->c.hci_WriteInquiryScanActivity_Out;
			break;

		case HCI_Read_Authentication_Enable:
			csb = pOwner->c.hci_ReadAuthenticationEnable_Out;
			break;

		case HCI_Write_Authentication_Enable:
			cs = pOwner->c.hci_WriteAuthenticationEnable_Out;
			break;

		case HCI_Read_Encryption_Mode:
			csb = pOwner->c.hci_ReadEncryptionMode_Out;
			break;

		case HCI_Write_Encryption_Mode:
			cs = pOwner->c.hci_WriteEncryptionMode_Out;
			break;

		case HCI_Read_Class_Of_Device:
			csi = pOwner->c.hci_ReadClassOfDevice_Out;
			break;

		case HCI_Write_Class_Of_Device:
			cs = pOwner->c.hci_WriteClassOfDevice_Out;
			break;

		case HCI_Read_Voice_Setting:
			css = pOwner->c.hci_ReadVoiceSetting_Out;
			break;

		case HCI_Write_Voice_Setting:
			cs = pOwner->c.hci_WriteVoiceSetting_Out;
			break;

		case HCI_Read_Automatic_Flush_Timeout:
			cshs = pOwner->c.hci_ReadAutomaticFlushTimeout_Out;
			break;

		case HCI_Write_Automatic_Flush_Timeout:
			csh = pOwner->c.hci_WriteAutomaticFlushTimeout_Out;
			break;

		case HCI_Read_Num_Broadcast_Retransmissions:
			csb = pOwner->c.hci_ReadNumBroadcastRetransmissions_Out;
			break;

		case HCI_Write_Num_Broadcast_Retransmissions:
			cs = pOwner->c.hci_WriteNumBroadcastRetransmissions_Out;
			break;

		case HCI_Read_Hold_Mode_Activity:
			csb = pOwner->c.hci_ReadHoldModeActivity_Out;
			break;

		case HCI_Write_Hold_Mode_Activity:
			cs = pOwner->c.hci_WriteHoldModeActivity_Out;
			break;

		case HCI_Read_Transmit_Power_Level:
			cshc = pOwner->c.hci_ReadTransmitPowerLevel_Out;
			break;

		case HCI_Read_SCO_Flow_Control_Enable:
			csb = pOwner->c.hci_ReadSCOFlowControlEnable_Out;
			break;

		case HCI_Write_SCO_Flow_Control_Enable:
			cs = pOwner->c.hci_WriteSCOFlowControlEnable_Out;
			break;

		case HCI_Set_Host_Controller_To_Host_Flow_Control:
			cs = pOwner->c.hci_SetHostControllerToHostFlowControl_Out;
			break;

		case HCI_Host_Buffer_Size:
			cs = pOwner->c.hci_HostBufferSize_Out;
			break;

		case HCI_Read_Link_Supervision_Timeout:
			cshs = pOwner->c.hci_ReadLinkSupervisionTimeout_Out;
			break;

		case HCI_Write_Link_Supervision_Timeout:
			csh = pOwner->c.hci_WriteLinkSupervisionTimeout_Out;
			break;

		case HCI_Write_Current_IAC_LAP:
			cs = pOwner->c.hci_WriteCurrentIACLAP_Out;
			break;

		case HCI_Write_Page_Scan_Period_Mode:
			cs = pOwner->c.hci_WritePageScanPeriodMode_Out;
			break;

		case HCI_Write_Page_Scan_Mode:
			cs = pOwner->c.hci_WritePageScanMode_Out;
			break;

		case HCI_Read_Page_Scan_Period_Mode:
			csb = pOwner->c.hci_ReadPageScanPeriodMode_Out;
			break;

		case HCI_Read_Page_Scan_Mode:
			csb = pOwner->c.hci_ReadPageScanMode_Out;
			break;

		case HCI_Read_BD_ADDR:
			csba = pOwner->c.hci_ReadBDADDR_Out;
			break;

		case HCI_Read_Country_Code:
			csb = pOwner->c.hci_ReadCountryCode_Out;
			break;

		case HCI_Read_Failed_Contact_Counter:
			cshs = pOwner->c.hci_ReadFailedContactCounter_Out;
			break;

		case HCI_Reset_Failed_Contact_Counter:
			csh = pOwner->c.hci_ResetFailedContactCounter_Out;
			break;

		case HCI_Get_Link_Quality:
			cshb = pOwner->c.hci_GetLinkQuality_Out;
			break;

		case HCI_Read_RSSI:
			cshc = pOwner->c.hci_ReadRSSI_Out;
			break;

		case HCI_Read_Loopback_Mode:
			csb = pOwner->c.hci_ReadLoopbackMode_Out;
			break;

		case HCI_Write_Loopback_Mode:
			cs = pOwner->c.hci_WriteLoopbackMode_Out;
			break;

		case HCI_Enable_Device_Under_Test_Mode:
			cs = pOwner->c.hci_EnableDeviceUnderTestMode_Out;
			break;

		case HCI_Read_Number_Of_Supported_IAC:
			csb = pOwner->c.hci_ReadNumberOfSupportedIAC_Out;
			break;

		case HCI_Read_Current_IAC_LAP:
		case HCI_Read_Local_Version_Information:
		case HCI_Read_Local_Supported_Features:
		case HCI_Read_Buffer_Size:
		case HCI_Read_Local_Name:
			// No default handler
			fHaveHandler = FALSE;
			break;

		default:
			IFDBG(DebugOut (DEBUG_WARN, L"Unknown command completion opcode 0x%04x\n", usOpCode));
			fHaveHandler = FALSE;
			// IncrHWErr (); This is not an error - custom hardware command will cause this
		}
	}
#if defined (DEBUG) || defined (_DEBUG) || defined (RETAILLOG)
	else
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned or internal packet : no callback\n"));
#endif

	//
	//	Execute the callback.
	//
	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"CommandComplete:: going into callback\n"));
	if (cs || csb || css || csss || csba || csi || csh || cshb || cshc || cshs) {
		gpHCI->Unlock ();

		__try {

			if (cs)
				cs (pCallContext, status);
			else if (csb)
				csb (pCallContext, status, cflag);
			else if (css)
				css (pCallContext, status, usflag);
			else if (csss)
				csss (pCallContext, status, usflag, usflag2);
			else if (csba)
				csba (pCallContext, status, &b);
			else if (csi)
				csi (pCallContext, status, uiflag);
			else if (csh)
				csh (pCallContext, status, h);
			else if (cshb)
				cshb (pCallContext, status, h, cflag);
			else if (cshc)
				cshc (pCallContext, status, h, (char)cflag);
			else if (cshs)
				cshs (pCallContext, status, h, usflag);
			else
				SVSUTIL_ASSERT (0);

		} __except (1) {
			IFDBG(DebugOut (DEBUG_ERROR, L"CommandComplete:: exception in callback\n"));
		}

		gpHCI->Lock ();
	} else if (pOwner && (! fHaveHandler)) {
		switch (usOpCode) {
		case HCI_Read_Current_IAC_LAP:
			{
				HCI_ReadCurrentIACLAP_Out pCallback = pOwner->c.hci_ReadCurrentIACLAP_Out;
				if (pCallback) {
					gpHCI->Unlock ();
					__try {
						pCallback (pCallContext, status, status == 0 ? pReturnParms[1] : 0, status == 0 ? pReturnParms + 2 : NULL);
					} __except (1) {
						IFDBG(DebugOut (DEBUG_ERROR, L"CommandComplete:: exception in callback\n"));
					}
					gpHCI->Lock ();
				}
				break;
			}

		case HCI_Read_Local_Version_Information:
			{
				HCI_ReadLocalVersionInformation_Out pCallback = pOwner->c.hci_ReadLocalVersionInformation_Out;
				if (pCallback) {
					gpHCI->Unlock ();
					__try {
						pCallback (pCallContext, status, pReturnParms[1], pReturnParms[2] | (pReturnParms[3] << 8), pReturnParms[4], pReturnParms[5] | (pReturnParms[6] << 8), (pReturnParms[7] | pReturnParms[8] << 8) );
					} __except (1) {
						IFDBG(DebugOut (DEBUG_ERROR, L"CommandComplete:: exception in callback\n"));
					}
					gpHCI->Lock ();
				}
				break;
			}

		case HCI_Read_Local_Supported_Features:
			{
				HCI_ReadLocalSupportedFeatures_Out pCallback = pOwner->c.hci_ReadLocalSupportedFeatures_Out;
				if (pCallback) {
					gpHCI->Unlock ();
					__try {
						pCallback (pCallContext, status, pReturnParms + 1);
					} __except (1) {
						IFDBG(DebugOut (DEBUG_ERROR, L"CommandComplete:: exception in callback\n"));
					}
					gpHCI->Lock ();
				}
				break;
			}

		case HCI_Read_Buffer_Size:
			{
				HCI_ReadBufferSize_Out pCallback = pOwner->c.hci_ReadBufferSize_Out;
				if (pCallback) {
					gpHCI->Unlock ();
					__try {
						pCallback (pCallContext, status, e.Host_Buffer.ACL_Data_Packet_Length, e.Host_Buffer.SCO_Data_Packet_Length, e.Host_Buffer.Total_Num_ACL_Data_Packets, e.Host_Buffer.Total_Num_SCO_Data_Packets);
					} __except (1) {
						IFDBG(DebugOut (DEBUG_ERROR, L"CommandComplete:: exception in callback\n"));
					}
					gpHCI->Lock ();
				}
				break;
			}

		case HCI_Read_Local_Name:
			{
				HCI_ReadLocalName_Out pCallback = pOwner->c.hci_ReadLocalName_Out;
				if (pCallback) {
					gpHCI->Unlock ();
					__try {
						pCallback (pCallContext, status, pReturnParms + 1);
					} __except (1) {
						IFDBG(DebugOut (DEBUG_ERROR, L"CommandComplete:: exception in callback\n"));
					}
					gpHCI->Lock ();
				}
				break;
			}

		default:
			{
				IFDBG(DebugOut (DEBUG_WARN, L"CommandComplete 0x%04x : unknown opcode...\n", usOpCode));
				HCI_CustomCodeEvent pCallback = pOwner->ei.hci_CustomCodeEvent;
				void *pUserContext = pOwner->pUserContext;
				if (pCallback && (cReturnParms < HCI_BUFFER - 3)) {
					unsigned char body[HCI_BUFFER];
					body[0] = 1;
					body[1] = usOpCode & 0xff;
					body[2] = (usOpCode >> 8) & 0xff;
					memcpy (body + 3, pReturnParms, cReturnParms);

					gpHCI->Unlock ();
					__try {
						pCallback (pUserContext, pCallContext, HCI_Command_Complete_Event, cReturnParms + 3, body);
					} __except (1) {
						IFDBG(DebugOut (DEBUG_ERROR, L"CommandComplete:: exception in callback\n"));
					}
					gpHCI->Lock ();
				}
			}
		}
	}

	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"CommandComplete:: came from callback\n"));
	gpHCI->Release ();

	if (gpHCI->eStage != Connected)
		return FALSE;

	// Reset HCI state; this also BUS_RESETs all pending packets
	if (iStackEvent == BTH_STACK_RESET)
		gpHCI->Reset ();

	//
	//	Propagate events up the stack...
	//
	if (iStackEvent != BTH_STACK_NONE)
		DispatchStackEvent (iStackEvent, pE);

	return FALSE;
}

static void CommandStatus (unsigned short usOpCode, unsigned char status) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"CommandStatus: op 0x%04x status %d\n", usOpCode, status));

	HCIPacket *pPacket = ExtractCommandPacket (&gpHCI->pPacketsSent, usOpCode, TRUE);

	if (! pPacket) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned Command status for opcode 0x%04x\n", usOpCode));
		return ;
	}

	void		*pCallContext = pPacket->pCallContext;
	HCI_CONTEXT	*pOwner = VerifyContext (pPacket->pOwner);

	CallbackStatus	cs   = NULL;

	if (pOwner) {
		switch (usOpCode) {
		case HCI_Inquiry:
			cs = pOwner->c.hci_Inquiry_Out;
			break;

		case HCI_Create_Connection:
			cs = pOwner->c.hci_CreateConnection_Out;
			break;

		case HCI_Disconnect:
			cs = pOwner->c.hci_Disconnect_Out;
			break;

		case HCI_Add_SCO_Connection:
			cs = pOwner->c.hci_AddSCOConnection_Out;
			break;

		case HCI_Accept_Connection_Request:
			cs = pOwner->c.hci_AcceptConnectionRequest_Out;
			break;

		case HCI_Reject_Connection_Request:
			cs = pOwner->c.hci_RejectConnectionRequest_Out;
			break;

		case HCI_Change_Connection_Packet_Type:
			cs = pOwner->c.hci_ChangeConnectionPacketType_Out;
			break;

		case HCI_Authentication_Requested:
			cs = pOwner->c.hci_AuthenticationRequested_Out;
			break;

		case HCI_Set_Connection_Encryption:
			cs = pOwner->c.hci_SetConnectionEncryption_Out;
			break;

		case HCI_Change_Connection_Link_Key:
			cs = pOwner->c.hci_ChangeConnectionLinkKey_Out;
			break;

		case HCI_Master_Link_Key:
			cs = pOwner->c.hci_MasterLinkKey_Out;
			break;

		case HCI_Remote_Name_Request:
			cs = pOwner->c.hci_RemoteNameRequest_Out;
			break;

		case HCI_Read_Remote_Supported_Features:
			cs = pOwner->c.hci_ReadRemoteSupportedFeatures_Out;
			break;

		case HCI_Read_Remote_Version_Information:
			cs = pOwner->c.hci_ReadRemoteVersionInformation_Out;
			break;

		case HCI_Read_Clock_Offset:
			cs = pOwner->c.hci_ReadClockOffset_Out;
			break;

		case HCI_Hold_Mode:
			cs = pOwner->c.hci_HoldMode_Out;
			break;

		case HCI_Sniff_Mode:
			cs = pOwner->c.hci_SniffMode_Out;
			break;

		case HCI_Exit_Sniff_Mode:
			cs = pOwner->c.hci_ExitSniffMode_Out;
			break;

		case HCI_Park_Mode:
			cs = pOwner->c.hci_ParkMode_Out;
			break;

		case HCI_Exit_Park_Mode:
			cs = pOwner->c.hci_ExitParkMode_Out;
			break;

		case HCI_QoS_Setup:
			cs = pOwner->c.hci_QoSSetup_Out;
			break;

		case HCI_Switch_Role:
			cs = pOwner->c.hci_SwitchRole_Out;
			break;

		default:
			IFDBG(DebugOut (DEBUG_ERROR, L"Unknown command status opcode 0x%04x\n", usOpCode));
		}
	}
#if defined (DEBUG) || defined (_DEBUG) || defined (RETAILLOG)
	else
		IFDBG(DebugOut (DEBUG_WARN, L"Command status for Orphaned or Internal packet, opcode 0x%04x\n", usOpCode));
#endif

	if (status == 0) {
		pPacket->pNext = NULL;

		if (! gpHCI->pPacketsPending)
			gpHCI->pPacketsPending = pPacket;
		else {
			HCIPacket *pParent = gpHCI->pPacketsPending;
			while (pParent->pNext)
				pParent = pParent->pNext;
			pParent->pNext = pPacket;
		}
	} else
		delete pPacket;

	//
	//	Process the global state
	//
	if (status == 0) {
		switch (usOpCode) {
			case HCI_Inquiry:	// Retire old inquiry data...
				{
					while (gpHCI->pInquiryResults) {
						InquiryResultList *pNext = gpHCI->pInquiryResults->pNext;
						delete gpHCI->pInquiryResults;
						gpHCI->pInquiryResults = pNext;
					}
				}
				break;
		}
	}

	//
	//	Execute the callback.
	//
	if (cs) {
		gpHCI->AddRef ();
		IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"CommandStatus:: going into callback\n"));
		gpHCI->Unlock ();

		__try {
			cs (pCallContext, status);
		} __except (1) {
			IFDBG(DebugOut (DEBUG_ERROR, L"CommandStatus:: exception in callback\n"));
		}

		gpHCI->Lock ();
		IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"CommandStatus:: came from callback\n"));
		gpHCI->Release ();
	} else if (pOwner) { // Unknown command...
		HCI_CustomCodeEvent pCallback = pOwner->ei.hci_CustomCodeEvent;
		void *pUserContext = pOwner->pUserContext;

		if (pCallback) {
			gpHCI->AddRef ();
			IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"CommandStatus:: going into callback\n"));
			gpHCI->Unlock ();

			__try {
				unsigned char body[4];
				body[0] = status;
				body[1] = 1;
				body[2] = usOpCode & 0xff;
				body[3] = (usOpCode >> 8) & 0xff;

				pCallback (pUserContext, pCallContext, HCI_Command_Status_Event, 4, body);
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"CommandStatus:: exception in callback\n"));
			}

			gpHCI->Lock ();
			IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"CommandStatus:: came from callback\n"));
			gpHCI->Release ();
		}
	}
}

static void HardwareError (unsigned char ucCode) {
	++gpHCI->iReportedErrors;

	IFDBG(DebugOut (DEBUG_ERROR, L"Hardware error code %d (%d errors so far)\n", ucCode, gpHCI->iReportedErrors));

	HCI_CONTEXT *pRunner = gpHCI->pContexts;
	while (pRunner && (gpHCI->eStage == Connected)) {
		if (pRunner->ei.hci_HardwareErrorEvent) {
			gpHCI->AddRef ();
			IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"HW error event :: going into callback\n"));
			void *pUserContext = pRunner->pUserContext;
			HCI_HardwareErrorEvent pCallback = pRunner->ei.hci_HardwareErrorEvent;

			gpHCI->Unlock ();

			__try {
				pCallback (pUserContext, NULL, ucCode);
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"HW error event :: exception in callback\n"));
			}

			gpHCI->Lock ();
			IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"HW error event :: came from callback\n"));
			gpHCI->Release ();
		}

		pRunner = pRunner->pNext;
	}
}

static void FlushOccured (unsigned short h) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"FlushOccured : h = 0x%04x\n", h));

	BasebandConnection *pConnection = FindConnection (h);
	HCI_CONTEXT *pOwner = pConnection ? VerifyContext (pConnection->pOwner) : NULL;

	HCI_FlushOccuredEvent pCallback = pOwner ? pOwner->ei.hci_FlushOccuredEvent : NULL;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned flush event (h = 0x%04x)\n", h));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"FlushOccured:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, NULL, h);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"FlushOccured:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"FlushOccured:: came from callback\n"));
	gpHCI->Release ();
}

static void RoleChanged (unsigned char status, BD_ADDR *pba, unsigned char role) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"RoleChanged : status = %d ba = %04x%08x, role = %d\n", status, pba->NAP, pba->SAP, role));

	HCIPacket *pPacket = ExtractCommandPacket (&gpHCI->pPacketsPending, HCI_Switch_Role, pba);

	HCI_CONTEXT *pOwner = NULL;
	void *pCallContext = NULL;

	if (pPacket) {
		pOwner = VerifyContext (pPacket->pOwner);
		pCallContext = pPacket->pCallContext;

		delete pPacket;
	}

	if (pOwner && pOwner->ei.hci_RoleChangeEvent) {
		HCI_RoleChangeEvent pCallback = pOwner->ei.hci_RoleChangeEvent;
		void *pUserContext = pOwner->pUserContext;

		gpHCI->AddRef ();
		IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"RoleChanged:: going into callback\n"));
		gpHCI->Unlock ();

		__try {
			pCallback (pUserContext, pCallContext, status, pba, role);
		} __except (1) {
			IFDBG(DebugOut (DEBUG_ERROR, L"RoleChanged:: exception in callback\n"));
		}

		gpHCI->Lock ();
		IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"RoleChanged:: came from callback\n"));
		gpHCI->Release ();
	}

	if (status != 0)
		return;

	HCI_CONTEXT *pRunner = gpHCI->pContexts;

	while ((gpHCI->eStage == Connected) && pRunner) {
		if (pRunner != pOwner) {
			HCI_RoleChangeEvent pCallback = pRunner->ei.hci_RoleChangeEvent;
			if (pCallback) {
				void *pUserContext = pRunner->pUserContext;

				gpHCI->AddRef ();
				IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"RoleChanged:: going into callback\n"));
				gpHCI->Unlock ();

				__try {
					pCallback (pUserContext, NULL, status, pba, role);
				} __except (1) {
					IFDBG(DebugOut (DEBUG_ERROR, L"RoleChanged:: exception in callback\n"));
				}

				gpHCI->Lock ();
				IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"RoleChanged:: came from callback\n"));
				gpHCI->Release ();
			}
		}

		pRunner = pRunner->pNext;
	}
}

static void NumberOfCompletedPackets (unsigned char cNum, unsigned char *pBuff) {
	while ((gpHCI->eStage == Connected) && (cNum != 0)) {
		unsigned short h = pBuff[0] | (pBuff[1] << 8);
		unsigned short cComplete = pBuff[2] | (pBuff[3] << 8);
		pBuff += 4;
		--cNum;

		IFDBG(DebugOut (DEBUG_HCI_TRACE | DEBUG_HCI_PACKETS, L"%d packets completed for connection 0x%04x\n", cComplete, h));

		BasebandConnection *pC = FindConnection (h);
		if (pC) {
			SetEvent (gpHCI->hQueue);

			if ( (pC->link_type == 0) && !gpHCI->fScoFlow) { // SCO=0; ACL=1
				// fScoFlow indicates that we should not be getting NumberOfCompletedPackets event
				// for SCO connections, yet we are.
				IFDBG(DebugOut (DEBUG_ERROR, L"NumberOfCompletedPackets :: h=0x%04x event occured but fScoFlow=FALSE\n", h));
			}

			pC->cAclDataPacketsPending -= cComplete;
            gpHCI->cTotalAclDataPacketsPending -= cComplete;

			if (pC->cAclDataPacketsPending < 0) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Completed packets out of sync on connection 0x%04x (%d too many)\n", h, -pC->cDataPacketsPending));
				IncrHWErr ();
				pC->cAclDataPacketsPending = 0;
			}

			if (gpHCI->cTotalAclDataPacketsPending < 0) {
				IFDBG(DebugOut (DEBUG_ERROR, L"Completed Total packets out of sync on connection 0x%04x (%d too many)\n", h, -pC->cDataPacketsPending));
				IncrHWErr ();
				gpHCI->cTotalAclDataPacketsPending = 0;
			}

			HCI_CONTEXT *pOwner = VerifyContext (pC->pOwner);
			void *pUserContext = pOwner ? pOwner->pUserContext : NULL;
			HCI_NumberOfCompletedPacketsEvent pCallback = pOwner ? pOwner->ei.hci_NumberOfCompletedPacketsEvent : NULL;

			if (pCallback) {
				gpHCI->AddRef ();
				IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"NumberOfCompletedPackets:: going into callback\n"));
				gpHCI->Unlock ();

				__try {
					pCallback (pUserContext, NULL, h, cComplete);
				} __except (1) {
					IFDBG(DebugOut (DEBUG_ERROR, L"NumberOfCompletedPackets:: exception in callback\n"));
				}

				gpHCI->Lock ();
				IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"NumberOfCompletedPackets:: came from callback\n"));
				gpHCI->Release ();
			}

		}
	}
}

static void ModeChange (unsigned char status, unsigned short h, unsigned char current_mode, unsigned short interval) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"ModeChange : status = %d connection = 0x%04x, mode = %d, interval = %d\n", status, h, current_mode, interval));

	HCIPacket *pPacket = gpHCI->pPacketsPending;
	HCIPacket *pParent = NULL;
	while (pPacket) {
		if ((pPacket->ePacketType == COMMAND_PACKET) &&
			((pPacket->uPacket.CommandPacket.opCode == HCI_Hold_Mode) ||
			(pPacket->uPacket.CommandPacket.opCode == HCI_Sniff_Mode) ||
			(pPacket->uPacket.CommandPacket.opCode == HCI_Exit_Sniff_Mode) ||
			(pPacket->uPacket.CommandPacket.opCode == HCI_Park_Mode) ||
			(pPacket->uPacket.CommandPacket.opCode == HCI_Exit_Park_Mode)) &&
			(pPacket->uPacket.CommandPacket.m.connection_handle == h)) {
			SVSUTIL_ASSERT (pPacket->uPacket.CommandPacket.m.fMarker == BTH_MARKER_CONNECTION);
			break;
		}
		pParent = pPacket;
		pPacket = pPacket->pNext;
	}

	HCI_CONTEXT *pOwner = NULL;
	void *pCallContext = NULL;

	if (pPacket) {
		pOwner = VerifyContext (pPacket->pOwner);
		pCallContext = pPacket->pCallContext;

		if ((status == 0) && (pPacket->uPacket.CommandPacket.opCode == HCI_Hold_Mode) && (pPacket->uPacket.CommandPacket.iEventRef == 0))
			++pPacket->uPacket.CommandPacket.iEventRef;
		else {
			if (pParent)
				pParent->pNext = pPacket->pNext;
			else
				gpHCI->pPacketsPending = pPacket->pNext;

			delete pPacket;
		}
	}

	BasebandConnection *pC = FindConnection (h);

	if (pC) {
		if (! pOwner)
			pOwner = VerifyContext (pC->pOwner);
		pC->fMode = current_mode;
	}

	HCI_ModeChangeEvent pCallback = pOwner ? pOwner->ei.hci_ModeChangeEvent : NULL;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned mode change event conn 0x%04x\n", h));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"ModeChange:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, pCallContext, status, h, current_mode, interval);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"ModeChange:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"ModeChange:: came from callback\n"));
	gpHCI->Release ();
}

static void ReturnLinkKeys (unsigned char cNum, unsigned char *pBuff) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"ReturnLinkKeys : %d keys\n", cNum));
	while ((cNum > 0) && (gpHCI->eStage == Connected)) {
		HCIPacket *pPacket = FindCommandPacket (gpHCI->pPacketsPending, HCI_Read_Stored_Link_Key);
		HCI_CONTEXT *pOwner = pPacket ? VerifyContext (pPacket->pOwner) : NULL;

		if (! pOwner) {
			IFDBG(DebugOut (DEBUG_WARN, L"Orphaned ReturnLinkKeys event\n"));
			return;
		}

		void *pCallContext = pPacket->pCallContext;
		void *pUserContext = pOwner->pUserContext;

		BD_ADDR b;
		memcpy (&b, pBuff, sizeof(b));
		unsigned char *pLinkKey = pBuff + 6;
		pBuff += 22;
		--cNum;

		HCI_ReturnLinkKeysEvent pCallback = pOwner->ei.hci_ReturnLinkKeysEvent;

		if (pCallback) {
			gpHCI->AddRef ();
			IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"ReturnLinkKeys:: going into callback\n"));
			gpHCI->Unlock ();

			__try {
				pCallback (pUserContext, pCallContext, &b, pLinkKey);
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"ReturnLinkKeys:: exception in callback\n"));
			}

			gpHCI->Lock ();
			IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"ReturnLinkKeys:: came from callback\n"));
			gpHCI->Release ();
		}
	}

}

static void PINRequest (BD_ADDR *pba) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"PINRequest bd_addr = %04x%08x\n", pba->NAP, pba->SAP));

	HCIPacket *pPacket = FindCommandPacket (gpHCI->pPacketsPending, HCI_Create_Connection, pba);
	HCI_CONTEXT *pOwner = pPacket ? VerifyContext (pPacket->pOwner) : NULL;

	if (pOwner && (! pOwner->ei.hci_PINCodeRequestEvent))
		pOwner = NULL;

	void *pCallContext = (pOwner && pPacket) ? pPacket->pCallContext : NULL;

	if (! pOwner) {
		pOwner = gpHCI->pContexts;
		while (pOwner && ((0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_BY_ADDR)) || (pOwner->ba != *pba)))
			pOwner = pOwner->pNext;
	}

	if (pOwner && (! pOwner->ei.hci_PINCodeRequestEvent))
		pOwner = NULL;

	if (! pOwner) {
		pOwner = gpHCI->pContexts;
		while (pOwner && (0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_SECURITY)))
			pOwner = pOwner->pNext;
	}

	if (! pOwner) {
		pOwner = gpHCI->pContexts;
		while (pOwner && (0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_ALL)))
			pOwner = pOwner->pNext;
	}

	HCI_PINCodeRequestEvent pCallback = pOwner ? pOwner->ei.hci_PINCodeRequestEvent : NULL;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"No context for PIN request - sending negative reply\n"));

		unsigned char body[sizeof(BD_ADDR)];
		memcpy (body, pba, sizeof(BD_ADDR));

		PacketMarker m;
		m.ba = *pba;
		m.fMarker = BTH_MARKER_ADDRESS;

		bt_custom_code_internal (NULL, NULL, HCI_PIN_Code_Request_Negative_Reply, sizeof(body), body, &m, HCI_Command_Complete_Event);

		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"PINRequest:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, pCallContext, pba);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"PINRequest:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"PINRequest:: came from callback\n"));
	gpHCI->Release ();
}

static void KeyRequest (BD_ADDR *pba) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"KeyRequest bd_addr = %04x%08x\n", pba->NAP, pba->SAP));

	HCIPacket *pPacket = FindCommandPacket (gpHCI->pPacketsPending, HCI_Create_Connection, pba);
	HCI_CONTEXT *pOwner = pPacket ? VerifyContext (pPacket->pOwner) : NULL;

	if (pOwner && (! pOwner->ei.hci_LinkKeyRequestEvent))
		pOwner = NULL;

	void *pCallContext = (pPacket && pOwner) ? pPacket->pCallContext : NULL;

	if (! pOwner) {
		pOwner = gpHCI->pContexts;
		while (pOwner && ((0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_BY_ADDR)) || (pOwner->ba != *pba)))
			pOwner = pOwner->pNext;
	}

	if (pOwner && (! pOwner->ei.hci_LinkKeyRequestEvent))
		pOwner = NULL;

	if (! pOwner) {
		pOwner = gpHCI->pContexts;
		while (pOwner && (0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_SECURITY)))
			pOwner = pOwner->pNext;
	}

	if (! pOwner) {
		pOwner = gpHCI->pContexts;
		while (pOwner && (0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_ALL)))
			pOwner = pOwner->pNext;
	}

	HCI_LinkKeyRequestEvent pCallback = pOwner ? pOwner->ei.hci_LinkKeyRequestEvent : NULL;
	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"No context for key request - responding with negative reply...\n"));
		unsigned char body[sizeof(BD_ADDR)];
		memcpy (body, pba, sizeof(BD_ADDR));

		PacketMarker m;
		m.ba = *pba;
		m.fMarker = BTH_MARKER_ADDRESS;

		bt_custom_code_internal (NULL, NULL, HCI_PIN_Code_Request_Negative_Reply, sizeof(body), body, &m, HCI_Command_Complete_Event);

		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"KeyRequest:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, pCallContext, pba);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"KeyRequest:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"KeyRequest:: came from callback\n"));
	gpHCI->Release ();
}

static void KeyNotification (BD_ADDR *pba, unsigned char key[16], unsigned char key_type) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"KeyNotification bd_addr = %04x%08x\n", pba->NAP, pba->SAP));

	BasebandConnection *pC = FindConnection (pba);
	HCI_CONTEXT *pOwner = pC ? VerifyContext (pC->pOwner) : NULL;

	if (pOwner && (! pOwner->ei.hci_LinkKeyNotificationEvent))
		pOwner = NULL;

	if (! pOwner) {
		pOwner = gpHCI->pContexts;
		while (pOwner && ((0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_BY_ADDR)) || (pOwner->ba != *pba)))
			pOwner = pOwner->pNext;
	}

	if (pOwner && (! pOwner->ei.hci_LinkKeyNotificationEvent))
		pOwner = NULL;

	if (! pOwner) {
		pOwner = gpHCI->pContexts;
		while (pOwner && (0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_SECURITY)))
			pOwner = pOwner->pNext;
	}

	if (! pOwner) {
		pOwner = gpHCI->pContexts;
		while (pOwner && (0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_ALL)))
			pOwner = pOwner->pNext;
	}

	HCI_LinkKeyNotificationEvent pCallback = pOwner ? pOwner->ei.hci_LinkKeyNotificationEvent : NULL;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"No context for key request - ignored\n"));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"KeyNotification:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, NULL, pba, key, key_type);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"KeyNotification:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"KeyNotification:: came from callback\n"));
	gpHCI->Release ();
}

static void Loopback (unsigned char cLen, unsigned char *pBuf) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"Loopback:: command 0x%04x\n", pBuf[0] | (pBuf[1] << 8)));
	HCI_CONTEXT *pOwner = gpHCI->pContexts;

	HCI_LoopbackCommandEvent pCallback = pOwner ? pOwner->ei.hci_LoopbackCommandEvent : NULL;
	if ((! pCallback) || (! gpHCI->fLoopback)) {
		IFDBG(DebugOut (DEBUG_ERROR, L"Unexpected loopback command\n"));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"Loopback:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, NULL, cLen, pBuf);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"Loopback:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"Loopback:: came from callback\n"));
	gpHCI->Release ();

}

static void DataOverflow (unsigned char cType) {
	IFDBG(DebugOut (DEBUG_ERROR, L"Data buffer overflow event: %d\n", cType));

	HCI_CONTEXT *pOwner = gpHCI->pContexts;
	while ((gpHCI->eStage == Connected) && pOwner && pOwner->ei.hci_DataBufferOverflowEvent) {
		HCI_DataBufferOverflowEvent pCallback = pOwner->ei.hci_DataBufferOverflowEvent;
		if (pCallback) {
			void *pUserContext = pOwner->pUserContext;

			gpHCI->AddRef ();
			IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"DataOverflow:: going into callback\n"));
			gpHCI->Unlock ();

			__try {
				pCallback (pUserContext, NULL, cType);
			} __except (1) {
				IFDBG(DebugOut (DEBUG_ERROR, L"DataOverflow:: exception in callback\n"));
			}

			gpHCI->Lock ();
			IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"DataOverflow:: came from callback\n"));
			gpHCI->Release ();
		}

		pOwner = pOwner->pNext;
	}
}

static void MaxSlotsChange (unsigned short h, unsigned char slots) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"MaxSlotsChange:: connection 0x%04x slots %d\n", h, slots));
	BasebandConnection *pC = FindConnection (h);
	HCI_CONTEXT *pOwner = pC ? VerifyContext (pC->pOwner) : NULL;

	HCI_MaxSlotsChangeEvent pCallback = pOwner ? pOwner->ei.hci_MaxSlotsChangeEvent : NULL;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"MaxSlotChange ignored - no callback!\n"));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"MaxSlotsChange:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, NULL, h, slots);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"MaxSlotsChange:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"MaxSlotsChange:: came from callback\n"));
	gpHCI->Release ();
}

static void ClockOffsetComplete (unsigned char status, unsigned short h, unsigned short offset) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"ClockOffsetComplete status %d connection 0x%04x, offset 0x%04x\n", status, h, offset));
	HCIPacket *pPacket = ExtractCommandPacket (&gpHCI->pPacketsPending, HCI_Read_Clock_Offset, h);
	HCI_CONTEXT *pOwner = NULL;
	void *pCallContext = NULL;

	if (pPacket) {
		pOwner = VerifyContext (pPacket->pOwner);
		pCallContext = pPacket->pCallContext;
		delete pPacket;
	}

	HCI_ReadClockOffsetCompleteEvent pCallback = pOwner ? pOwner->ei.hci_ReadClockOffsetCompleteEvent : NULL;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned Read Clock Offset Complete event for connection 0x%04x\n", h));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"ClockOffsetComplete:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, pCallContext, status, h, offset);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"ClockOffsetComplete:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"ClockOffsetComplete:: came from callback\n"));
	gpHCI->Release ();
}

static void ConnectionPacketTypeChanged (unsigned char status, unsigned short h, unsigned short type) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"ConnectionPacketTypeChanged status = %d, h = 0x%04x, type = %d\n", status, h, type));

	HCIPacket *pPacket = ExtractCommandPacket (&gpHCI->pPacketsPending, HCI_Change_Connection_Packet_Type, h);
	HCI_CONTEXT *pOwner = NULL;
	void *pCallContext = NULL;
	if (pPacket) {
		pOwner = VerifyContext (pPacket->pOwner);
		pCallContext = pPacket->pCallContext;
		delete pPacket;
	} else {
		BasebandConnection *pC = FindConnection (h);
		pOwner = pC ? VerifyContext (pC->pOwner) : NULL;
	}

	HCI_ConnectionPacketTypeChangedEvent pCallback = pOwner ? pOwner->ei.hci_ConnectionPacketTypeChangedEvent : NULL;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned connection packet type changed event for connection 0x%04x\n", h));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"ConnectionPacketTypeChanged:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, pCallContext, status, h, type);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"ConnectionPacketTypeChanged:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"ConnectionPacketTypeChanged:: came from callback\n"));
	gpHCI->Release ();
}

static void QoSViolation (unsigned short h) {
	IFDBG(DebugOut (DEBUG_ERROR, L"QoS Violation! h = 0x%04x\n", h));

	BasebandConnection *pC = FindConnection (h);
	HCI_CONTEXT *pOwner = pC ? VerifyContext (pC->pOwner) : NULL;

	HCI_QoSViolationEvent pCallback = pOwner ? pOwner->ei.hci_QoSViolationEvent : NULL;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"Orphaned QoS violation event for connection 0x%04x\n", h));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"Violation:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, NULL, h);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"Violation:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"Violation:: came from callback\n"));
	gpHCI->Release ();
}

static void PageScanModeChange (BD_ADDR *pba, unsigned char mode) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"PageScanModeChangeEvent for ba %04x%06x mode = %d\n", pba->NAP, pba->SAP, mode));

#if defined (BTH_LONG_TERM_STATE)
//	page_scan_repetition_mode   = modes[0];
//	page_scan_period_mode       = modes[1];
//	page_scan_mode              = modes[2];

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"PageScanModeChange: updating long-term cache\n"));

    unsigned char modes[3];

	if (! btutil_RetrievePeerInfo (pba, BTH_STATE_HCI_SCAN_MODES, modes)) {
	    IFDBG(DebugOut (DEBUG_HCI_TRACE, L"PageScanModeChange: updating long-term cache (new record)\n"));
		memset (modes, 0, sizeof(modes));
	}

    modes[2] = mode;

	btutil_PersistPeerInfo (pba, BTH_STATE_HCI_SCAN_MODES, modes);
#endif

	HCI_CONTEXT *pOwner = gpHCI->pContexts;
	while (pOwner && ((0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_BY_ADDR)) || (pOwner->ba != *pba)))
		pOwner = pOwner->pNext;

	if (! pOwner) {
		pOwner = gpHCI->pContexts;
		while (pOwner && (0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_ALL)))
			pOwner = pOwner->pNext;
	}

	HCI_PageScanModeChangeEvent pCallback = pOwner ? pOwner->ei.hci_PageScanModeChangeEvent : NULL;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"No context/callback for page scan change - ignored\n"));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"PageScanModeChangeEvent:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, NULL, pba, mode);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"PageScanModeChangeEvent:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"PageScanModeChangeEvent:: came from callback\n"));
	gpHCI->Release ();
}

static void PageScanRepModeChange (BD_ADDR *pba, unsigned char mode) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"PageScanRepModeChange for ba %04x%06x mode = %d\n", pba->NAP, pba->SAP, mode));

#if defined (BTH_LONG_TERM_STATE)
//	page_scan_repetition_mode   = modes[0];
//	page_scan_period_mode       = modes[1];
//	page_scan_mode              = modes[2];

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"PageScanRepModeChange: updating long-term cache\n"));

    unsigned char modes[3];

	if (! btutil_RetrievePeerInfo (pba, BTH_STATE_HCI_SCAN_MODES, modes)) {
	    IFDBG(DebugOut (DEBUG_HCI_TRACE, L"PageScanRepModeChange: updating long-term cache (new record)\n"));
		memset (modes, 0, sizeof(modes));
	}

    modes[0] = mode;

	btutil_PersistPeerInfo (pba, BTH_STATE_HCI_SCAN_MODES, modes);
#endif

	HCI_CONTEXT *pOwner = gpHCI->pContexts;
	while (pOwner && ((0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_BY_ADDR)) || (pOwner->ba != *pba)))
		pOwner = pOwner->pNext;

	if (! pOwner) {
		pOwner = gpHCI->pContexts;
		while (pOwner && (0 == (pOwner->uiControl & BTH_CONTROL_ROUTE_ALL)))
			pOwner = pOwner->pNext;
	}

	HCI_PageScanRepetitionModeChangeEvent pCallback = pOwner ? pOwner->ei.hci_PageScanRepetitionModeChangeEvent : NULL;

	if (! pCallback) {
		IFDBG(DebugOut (DEBUG_WARN, L"No context/callback for page scan rep mode change - ignored\n"));
		return;
	}

	void *pUserContext = pOwner->pUserContext;

	gpHCI->AddRef ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"PageScanRepModeChange:: going into callback\n"));
	gpHCI->Unlock ();

	__try {
		pCallback (pUserContext, NULL, pba, mode);
	} __except (1) {
		IFDBG(DebugOut (DEBUG_ERROR, L"PageScanRepModeChange:: exception in callback\n"));
	}

	gpHCI->Lock ();
	IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"PageScanRepModeChange:: came from callback\n"));
	gpHCI->Release ();
}

static void UnknownEvent (unsigned char cEventCode, unsigned char cLength, unsigned char *pBuffer) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"UnknownEvent code 0x%02x with %d bytes of info\n", cEventCode, cLength));
	HCIPacket *pPacket = ExtractCommandPacketByEvent (&gpHCI->pPacketsPending, cEventCode);
	HCI_CONTEXT *pOwner = pPacket ? pPacket->pOwner : NULL;
	void *pCallContext = pPacket ? pPacket->pCallContext : NULL;

	if (pPacket)
		delete pPacket;

	HCI_CONTEXT *pRunner = gpHCI->pContexts;
	while ((gpHCI->eStage == Connected) && pRunner) {
		if ((! pOwner) || (pOwner == pRunner)) {
			HCI_CustomCodeEvent pCallback = pRunner->ei.hci_CustomCodeEvent;
			if (pCallback) {
				void *pUserContext = pRunner->pUserContext;

				gpHCI->AddRef ();
				IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"UnknownEvent:: going into callback\n"));
				gpHCI->Unlock ();

				__try {
					pCallback (pUserContext, pCallContext, cEventCode, cLength, pBuffer);
				} __except (1) {
					IFDBG(DebugOut (DEBUG_ERROR, L"UnknownEvent:: exception in callback\n"));
				}

				gpHCI->Lock ();
				IFDBG(DebugOut (DEBUG_HCI_CALLBACK, L"UnknownEvent:: came from callback\n"));
				gpHCI->Release ();
			} else
				IFDBG(DebugOut (DEBUG_HCI_TRACE, L"UnknownEvent code 0x%02x with %d bytes of info : no callback, event ignored\n", cEventCode, cLength));
		}

		//	Note: this is legal because we don't deref HCI and disconnect will always wait for ref to drop
		pRunner = pRunner->pNext;
	}
}

static void ProcessEvent (unsigned char *pBuffer, int cSize) {
	unsigned char cEventCode = pBuffer[0];
	int cLength = pBuffer[1];
	unsigned char *pParms = pBuffer + 2;
	int fLengthFault = (cLength + 2) != cSize;

	if (! fLengthFault) {
		switch (cEventCode) {
		case HCI_Inquiry_Complete_Event:
			if (cLength == 2) // && (gpHCI->transport.fHardwareVersion <= HCI_HARDWARE_VERSION_V_1_0_B)
				InquiryComplete (pParms[0], pParms[1]);
			else if (cLength == 1) // && (gpHCI->transport.fHardwareVersion >= HCI_HARDWARE_VERSION_V_1_1)
				InquiryComplete (pParms[0], 0);
			else
				fLengthFault = TRUE;
			break;

		case HCI_Inquiry_Result_Event:
			if (cLength > 1) {
				if (pParms[0]) {
					if ((cLength - 1) == pParms[0] * 14)
						InquiryResult (pParms[0], pParms + 1);
					else
						fLengthFault = TRUE;
				} else
					IFDBG(DebugOut (DEBUG_WARN, L"Inquiry result with NULL data\n"));
			} else
				fLengthFault = TRUE;
			break;

		case HCI_Connection_Complete_Event:
			if (cLength == 11) {
				BD_ADDR b;
				memcpy (&b, pParms + 3, sizeof(BD_ADDR));
				ConnectionComplete (pParms[0], pParms[1] | (pParms[2] << 8), &b, pParms[9], pParms[10]);
			} else
				fLengthFault = TRUE;
			break;

		case HCI_Connection_Request_Event:
			if (cLength == 10) {
				BD_ADDR b;
				memcpy (&b, pParms, sizeof (BD_ADDR));
				ConnectionRequest (&b, pParms[6] | (pParms[7] << 8) | (pParms[8] << 16), pParms[9]);
			} else
				fLengthFault = TRUE;
			break;

		case HCI_Disconnection_Complete_Event:
			if (cLength == 4)
				DisconnectionComplete (pParms[0], pParms[1] | (pParms[2] << 8), pParms[3]);
			else
				fLengthFault = TRUE;
			break;

		case HCI_Authentication_Complete_Event:
			if (cLength == 3)
				AuthenticationComplete (pParms[0], pParms[1] | (pParms[2] << 8));
			else
				fLengthFault = TRUE;
			break;

		case HCI_Remote_Name_Request_Complete_Event:
			if (cLength >= (1 + 6)) {
				BD_ADDR b;
				memcpy (&b, pParms + 1, sizeof(BD_ADDR));
				RemoteNameRequestComplete (pParms[0], &b, pParms + 7);
			} else
				fLengthFault = TRUE;
			break;

		case HCI_Encryption_Change_Event:
			if (cLength == 4)
				EncryptionChange (pParms[0], pParms[1] | (pParms[2] << 8), pParms[3]);
			else
				fLengthFault = TRUE;
			break;

		case HCI_Change_Connection_Link_Key_Complete_Event:
			if (cLength == 3)
				ChangeConnectionLinkKey (pParms[0], pParms[1] | (pParms[2] << 8));
			else
				fLengthFault = TRUE;
			break;

		case HCI_Master_Link_Key_Complete_Event:
			if (cLength == 4)
				MasterLinkKeyComplete (pParms[0], pParms[1] | (pParms[2] << 8), pParms[3]);
			else
				fLengthFault = TRUE;
			break;

		case HCI_Read_Remote_Supported_Features_Complete_Event:
			if (cLength == (1 + 2 + 8))
				ReadRemoteSupportedFeaturesComplete (pParms[0], pParms[1] | (pParms[2] << 8), pParms + 3);
			else
				fLengthFault = TRUE;
			break;

		case HCI_Read_Remote_Version_Information_Complete_Event:
			if (cLength == (3 + 1 + 2 + 2))
				ReadRemoteVersionInformationComplete (pParms[0], pParms[1] | (pParms[2] << 8), pParms[3], pParms[4] | (pParms[5] << 8), pParms[6] | (pParms[7] << 1));
			else
				fLengthFault = TRUE;
			break;

		case HCI_QoS_Setup_Complete_Event:
			if (cLength == (3 + 1 + 1 + 4 * 4))
				QoSSetupComplete (pParms[0], pParms[1] | (pParms[2] << 8), pParms[3], pParms[4],
					pParms[5]  | (pParms[6] << 8) | (pParms[7] << 16) | (pParms[8] << 24),
					pParms[9]  | (pParms[10] << 8) | (pParms[11] << 16) | (pParms[12] << 24),
					pParms[13] | (pParms[14] << 8) | (pParms[15] << 16) | (pParms[16] << 24),
					pParms[17] | (pParms[18] << 8) | (pParms[19] << 16) | (pParms[20] << 24));
			else
				fLengthFault = TRUE;
			break;

		case HCI_Command_Complete_Event:
			if (cLength >= 3) {
				if ((gpHCI->cCommandPacketsAllowed = pParms[0]) > 0)
					SetEvent (gpHCI->hQueue);

				fLengthFault = CommandComplete (pParms[1] | (pParms[2] << 8), cLength - 3, pParms + 3);
			} else
				fLengthFault = TRUE;
			break;

		case HCI_Command_Status_Event:
			if (cLength == 4) {
				if ((gpHCI->cCommandPacketsAllowed = pParms[1]) > 0)
					SetEvent (gpHCI->hQueue);

				CommandStatus (pParms[2] | (pParms[3] << 8), pParms[0]);
			} else
				fLengthFault = TRUE;
			break;

		case HCI_Hardware_Error_Event:
			if (cLength == 1) {
				HardwareError (pParms[0]);
			} else
				fLengthFault = TRUE;
			break;

		case HCI_Flush_Occured_Event:
			if (cLength == 3)
				FlushOccured (pParms[0] | (pParms[1] << 8));
			else
				fLengthFault = TRUE;
			break;

		case HCI_Role_Change_Event:
			if (cLength == 8) {
				BD_ADDR b;
				memcpy (&b, pParms + 1, sizeof(b));
				RoleChanged (pParms[0], &b, pParms[7]);
			} else
				fLengthFault = TRUE;
			break;

		case HCI_Number_Of_Completed_Packets_Event:
			if (cLength == 1 + pParms[0] * 4)
				NumberOfCompletedPackets (pParms[0], pParms + 1);
			else
				fLengthFault = TRUE;
			break;

		case HCI_Mode_Change_Event:
			if (cLength == 6)
				ModeChange (pParms[0], pParms[1] | (pParms[2] << 8), pParms[3], pParms[4] | (pParms[5] << 8));
			else
				fLengthFault = TRUE;
			break;
			
		case HCI_Return_Link_Keys_Event:
			if (cLength == 1 + 22 * pParms[0])
				ReturnLinkKeys (pParms[0], pParms + 1);
			else
				fLengthFault = TRUE;
			break;

		case HCI_PIN_Code_Request_Event:
			if (cLength == 6) {
				BD_ADDR b;
				memcpy (&b, pParms, sizeof(b));
				PINRequest (&b);
			} else
				fLengthFault = TRUE;
			break;

		case HCI_Link_Key_Request_Event:
			if (cLength == 6) {
				BD_ADDR b;
				memcpy (&b, pParms, sizeof(b));
				KeyRequest (&b);
			} else
				fLengthFault = TRUE;
			break;

		case HCI_Link_Key_Notification_Event:
			if (cLength == 22) { // && (gpHCI->transport.fHardwareVersion <= HCI_HARDWARE_VERSION_V_1_0_B)
				BD_ADDR b;
				memcpy (&b, pParms, sizeof(b));
				KeyNotification (&b, pParms + 6, 0xff);
			} else if (cLength == 23) { // && (gpHCI->transport.fHardwareVersion >= HCI_HARDWARE_VERSION_V_1_1)
				BD_ADDR b;
				memcpy (&b, pParms, sizeof(b));
				KeyNotification (&b, pParms + 6, pParms[6 + 16]);
			} else
				fLengthFault = TRUE;
			break;

		case HCI_Loopback_Command_Event:
			if (cLength > 2)
				Loopback (cLength, pParms);
			else
				fLengthFault = TRUE;
			break;

		case HCI_Data_Buffer_Overflow_Event:
			if (cLength == 1)
				DataOverflow (pParms[0]);
			else
				fLengthFault = TRUE;
			break;

		case HCI_Max_Slots_Change_Event:
			if (cLength == 3)
				MaxSlotsChange (pParms[0] | (pParms[1] << 8), pParms[2]);
			else
				fLengthFault = TRUE;
			break;

		case HCI_Read_Clock_Offset_Complete_Event:
			if (cLength == 5)
				ClockOffsetComplete (pParms[0], pParms[1] | (pParms[2] << 8), pParms[3] | (pParms[4] << 8));
			else
				fLengthFault = TRUE;
			break;

		case HCI_Connection_Packet_Type_Changed_Event:
			if (cLength == 5)
				ConnectionPacketTypeChanged (pParms[0], pParms[1] | (pParms[2] << 8), pParms[3] | (pParms[4] << 8));
			else
				fLengthFault = TRUE;
			break;

		case HCI_QoS_Violation_Event:
			if (cLength == 2)
				QoSViolation (pParms[0] | (pParms[1] << 8));
			else
				fLengthFault = TRUE;
			break;

		case HCI_Page_Scan_Mode_Change_Event:
			if (cLength == 7) {
				BD_ADDR b;
				memcpy (&b, pParms, sizeof(b));
				PageScanModeChange (&b, pParms[6]);
			} else
				fLengthFault = TRUE;
			break;

		case HCI_Page_Scan_Repetition_Mode_Change_Event:
			if (cLength == 7) {
				BD_ADDR b;
				memcpy (&b, pParms, sizeof(b));
				PageScanRepModeChange (&b, pParms[6]);
			} else
				fLengthFault = TRUE;
			break;

		default:
			IFDBG(DebugOut (DEBUG_ERROR, L"Unknown event code 0x%02x\n", cEventCode));
			UnknownEvent (cEventCode, cLength, pParms);
		}
	}

	if (fLengthFault) {
		IFDBG(DebugOut (DEBUG_ERROR, L"Incorrect length %d for event 0x%02x\n", cLength, cEventCode));
		IncrHWErr ();
	} else
		NotifyMonitors (pBuffer, cSize);
}

static DWORD WINAPI ReaderThread (LPVOID lpvUnused) {
	IFDBG(DebugOut (DEBUG_HCI_INIT, L"RT: Started\n"));

	gpHCI->Lock ();

	if (gpHCI->eStage != AlmostRunning) {
		IFDBG(DebugOut (DEBUG_ERROR, L"RT: System is shutting down (wrong state)\n"));
		gpHCI->Unlock ();
		return 0;
	}

	BD_BUFFER bPacket;
	bPacket.fMustCopy = TRUE;
	bPacket.pFree = NULL;
	bPacket.cSize = gpHCI->transport.iReadBufferHeader + gpHCI->transport.iMaxSizeRead + gpHCI->transport.iReadBufferTrailer;
	bPacket.pBuffer = (unsigned char *)g_funcAlloc (bPacket.cSize, g_pvAllocData);

	if (! bPacket.pBuffer) {
		IFDBG(DebugOut (DEBUG_ERROR, L"RT: Could not alloc %d bytes for HCI packet buffer\n", bPacket.cSize));
		gpHCI->Unlock ();
		return 0;
	}

	IFDBG(DebugOut (DEBUG_HCI_INIT, L"RT: Reader thread properly initialized - max packet %d bytes\n", bPacket.cSize));
	gpHCI->Unlock ();

	for ( ; ; ) {
		HCI_TYPE	eType;

#if defined (DEBUG) || defined (_DEBUG)
		extern void L2CAPD_CheckLock (void);
		L2CAPD_CheckLock ();
#endif

		bPacket.cEnd = bPacket.cSize;
		bPacket.cStart = 0;

		if (! HCI_ReadPacket (&eType, &bPacket) || (bPacket.cStart & 0x3)) {
			g_funcFree (bPacket.pBuffer, g_pvFreeData);

			if (gpHCI) {
				gpHCI->Lock ();
				if ((gpHCI->eStage == Connected) || (gpHCI->eStage == AlmostRunning)) {
					gpHCI->eStage = Error;
					ShutdownTransport ();
				}

				gpHCI->Unlock ();
			}

			IFDBG(DebugOut (DEBUG_HCI_INIT, L"RT: Communication breakdown - exiting\n"));
			return 0;
		}

		if (! gpHCI) {
			g_funcFree (bPacket.pBuffer, g_pvFreeData);
			IFDBG(DebugOut (DEBUG_HCI_INIT, L"RT: Lost state - exiting\n"));
			return 0;
		}

		gpHCI->Lock ();
		if ((gpHCI->eStage != Connected) && (gpHCI->eStage != AlmostRunning)) {
			IFDBG(DebugOut (DEBUG_HCI_INIT, L"RT: Lost state - exiting (error or shutdown)\n"));
			break;
		}

		//
		//	Process the packet here...
		//
		if (eType == EVENT_PACKET)
			ProcessEvent (bPacket.pBuffer + bPacket.cStart, BufferTotal (&bPacket));
		else if (eType == DATA_PACKET_ACL)
			ProcessDataAcl (bPacket.pBuffer + bPacket.cStart, BufferTotal (&bPacket));
		else if (eType == DATA_PACKET_SCO)
			ProcessDataSco (bPacket.pBuffer + bPacket.cStart, BufferTotal (&bPacket));
		else
			SVSUTIL_ASSERT (0);

		if ((gpHCI->eStage != Connected) && (gpHCI->eStage != AlmostRunning)) {
			IFDBG(DebugOut (DEBUG_HCI_INIT, L"RT: Exiting (error or shutdown)\n"));
			break;
		}

		gpHCI->Unlock ();
	}

	g_funcFree (bPacket.pBuffer, g_pvFreeData);
	gpHCI->Unlock ();
	return 0;
}

//
//	Interface Functions
//
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
		*pcDataReturned = 0;

		memset (pOutBuffer, 0, cOutBuffer);

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

static DWORD WINAPI GetHardwareState (LPVOID lpUnused) {	// This helper is here because IOCTL can be called from PSL
	if (! gpHCI) {
		IFDBG(DebugOut (DEBUG_ERROR, L"GetHardwareState returns HCI_HARDWARE_UNKNOWN (stack not present)\n"));
		return HCI_HARDWARE_UNKNOWN;
	}

	gpHCI->Lock ();

	if (! gpHCI->IsStackRunning ()) {
		IFDBG(DebugOut (DEBUG_ERROR, L"GetHardwareState returns HCI_HARDWARE_UNKNOWN (stack not initialized)\n"));
		gpHCI->Unlock ();
		return HCI_HARDWARE_UNKNOWN;
	}

	int flag = HCI_HARDWARE_RUNNING;
	if (gpHCI->eStage != Connected) {
		if (gpHCI->eStage == AlmostRunning)
			flag = HCI_HARDWARE_INITIALIZING;
		else if (gpHCI->eStage == Error)
			flag = HCI_HARDWARE_ERROR;
		else if (gpHCI->eStage == Disconnected) {
			if (HCI_OpenConnection ()) {
				flag = HCI_HARDWARE_SHUTDOWN;
				HCI_CloseConnection ();
			} else
				flag = HCI_HARDWARE_NOT_PRESENT;
		} else {
			SVSUTIL_ASSERT (0);
			flag = HCI_HARDWARE_UNKNOWN;
		}
	}


	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"GetHardwareState returns %d\n", flag));

	gpHCI->Unlock ();

	return (DWORD)flag;
}

static int bt_io_control
(
HANDLE	hDeviceContext,
int		fSelector,
int		cInBuffer,
char	*pInBuffer,
int		cOutBuffer,
char	*pOutBuffer,
int		*pcDataReturned
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_io_control [0x%08x code 0x%08x]\n", hDeviceContext, fSelector));

	char c;
	if (! check_io_control_parms (cInBuffer, pInBuffer, cOutBuffer, pOutBuffer, pcDataReturned, &c, 1)) {
		IFDBG(DebugOut (DEBUG_ERROR, L"bt_io_control returns ERROR_INVALID_PARAMETER (exception)\n"));
		return ERROR_INVALID_PARAMETER;
	}

	if (! gpHCI) {
		IFDBG(DebugOut (DEBUG_ERROR, L"bt_io_control returns ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpHCI->Lock ();

	if (! gpHCI->IsStackRunning ()) {
		IFDBG(DebugOut (DEBUG_ERROR, L"bt_io_control returns ERROR_SERVICE_NOT_ACTIVE\n"));
		gpHCI->Unlock ();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	HCI_CONTEXT *pContext = VerifyContext ((HCI_CONTEXT *)hDeviceContext);
	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"bt_io_control returns ERROR_INVALID_HANDLE\n"));
		gpHCI->Unlock ();
		return ERROR_INVALID_HANDLE;
	}

	int iRes = ERROR_INVALID_OPERATION;

	switch (fSelector) {
	case BTH_HCI_IOCTL_GET_BD_FOR_HANDLE:
		if ((cInBuffer == 2) && (cOutBuffer == sizeof(BD_ADDR))) {
			unsigned short h = pInBuffer[0] | (pInBuffer[1] << 8);
			BasebandConnection *pC = FindConnection (h);
			if (pC) {
				iRes = ERROR_SUCCESS;
				memcpy (pOutBuffer, &pC->ba, sizeof(BD_ADDR));
				*pcDataReturned = sizeof(BD_ADDR);
			} else
				iRes = ERROR_CONNECTION_INVALID;
		} else
			iRes = ERROR_INVALID_PARAMETER;

		break;

	case BTH_HCI_IOCTL_HANDLE_AUTHENTICATED:
		if ((cInBuffer == 2) && (cOutBuffer == sizeof(int))) {
			unsigned short h = pInBuffer[0] | (pInBuffer[1] << 8);
			BasebandConnection *pC = FindConnection (h);
			if (pC) {
				iRes = ERROR_SUCCESS;
				int flag = pC->fAuthenticated;
				memcpy (pOutBuffer, &flag, sizeof(int));
				*pcDataReturned = sizeof(int);
			} else
				iRes = ERROR_CONNECTION_INVALID;
		} else
			iRes = ERROR_INVALID_PARAMETER;

		break;

	case BTH_HCI_IOCTL_HANDLE_ENCRYPTED:
		if ((cInBuffer == 2) && (cOutBuffer == sizeof(int))) {
			unsigned short h = pInBuffer[0] | (pInBuffer[1] << 8);
			BasebandConnection *pC = FindConnection (h);
			if (pC) {
				iRes = ERROR_SUCCESS;
				int flag = pC->fEncrypted;
				memcpy (pOutBuffer, &flag, sizeof(int));
				*pcDataReturned = sizeof(int);
			} else
				iRes = ERROR_CONNECTION_INVALID;
		} else
			iRes = ERROR_INVALID_PARAMETER;

		break;

	case BTH_HCI_IOCTL_GET_HANDLE_FOR_BD:
		if ((cInBuffer == sizeof(BD_ADDR) + 1) && (cOutBuffer == 2)) {
			BD_ADDR b;
			memcpy (&b, pInBuffer, sizeof(BD_ADDR));
			unsigned char counter = pInBuffer[sizeof(BD_ADDR)];
			BasebandConnection *pC = gpHCI->pConnections;
			iRes = ERROR_NOT_FOUND;
			while (pC) {
				if (pC->ba == b) {
					if (counter == 0) {
						iRes = ERROR_SUCCESS;
						pOutBuffer[0] = pC->connection_handle & 0xff;
						pOutBuffer[1] = (pC->connection_handle >> 8) & 0xff;
						*pcDataReturned = 2;
						break;
					} else
						--counter;
				}
				pC = pC->pNext;
			}
		} else
			iRes = ERROR_INVALID_PARAMETER;

		break;

	case BTH_HCI_IOCTL_GET_NUM_UNSENT:
	case BTH_HCI_IOCTL_GET_NUM_ONDEVICE:
	case BTH_HCI_IOCTL_GET_NUM_PENDING:
		{
			HCIPacket *pList;
			if (fSelector == BTH_HCI_IOCTL_GET_NUM_UNSENT)
				pList = gpHCI->pPackets;
			else if (fSelector == BTH_HCI_IOCTL_GET_NUM_ONDEVICE)
				pList = gpHCI->pPacketsSent;
			else
				pList = gpHCI->pPacketsPending;

			if ((cInBuffer == 0) && (cOutBuffer == 4)) {
				iRes = ERROR_SUCCESS;

				unsigned int iCount = 0;
				while (pList) {
					++iCount;
					pList = pList->pNext;
				}
				pOutBuffer[0] = iCount & 0xff;
				pOutBuffer[1] = (iCount >> 8) & 0xff;
				pOutBuffer[2] = (iCount >> 16) & 0xff;
				pOutBuffer[3] = (iCount >> 24) & 0xff;
				*pcDataReturned = 4;
			} else
				iRes = ERROR_INVALID_PARAMETER;
		}
		break;

	case BTH_HCI_IOCTL_GET_INQUIRY:
	case BTH_HCI_IOCTL_GET_PERIODIC_INQUIRY:
	case BTH_HCI_IOCTL_GET_LOOPBACK:
	case BTH_HCI_IOCTL_GET_ERRORS:
	case BTH_HCI_IOCTL_GET_FLOW:
	case BTH_HCI_IOCTL_GET_COMMANDSIZE:
	case BTH_STACK_IOCTL_GET_CONNECTED:
		if ((cInBuffer == 0) && (cOutBuffer == 4)) {
			iRes = ERROR_SUCCESS;

			int iCount;
			if (fSelector == BTH_STACK_IOCTL_GET_CONNECTED)
				iCount = gpHCI->eStage == Connected;
			else if (fSelector == BTH_HCI_IOCTL_GET_INQUIRY)
				iCount = NULL != FindCommandPacket (gpHCI->pPacketsPending, HCI_Inquiry);
			else if (fSelector == BTH_HCI_IOCTL_GET_PERIODIC_INQUIRY)
				iCount = NULL != gpHCI->pPeriodicInquiryOwner;
			else if (fSelector == BTH_HCI_IOCTL_GET_LOOPBACK)
				iCount = gpHCI->fLoopback;
			else if (fSelector == BTH_HCI_IOCTL_GET_UNDER_TEST)
				iCount = gpHCI->fUnderTest;
			else if (fSelector == BTH_HCI_IOCTL_GET_ERRORS)
				iCount = gpHCI->iReportedErrors + gpHCI->iHardwareErrors;
			else if (fSelector == BTH_HCI_IOCTL_GET_COMMANDSIZE) {
				if (gpHCI->eStage == Connected) {
					HCI_PARAMETERS p;
					memset (&p, 0, sizeof(p));
					HCI_ReadHciParameters (&p);
					iCount = p.iMaxSizeRead;
					if (iCount > HCI_BUFFER)
						iCount = HCI_BUFFER;
				} else {
					iRes = ERROR_DEVICE_NOT_CONNECTED;
					break;
				}
			} else
				iCount = gpHCI->fHostFlow;


			pOutBuffer[0] = iCount & 0xff;
			pOutBuffer[1] = (iCount >> 8) & 0xff;
			pOutBuffer[2] = (iCount >> 16) & 0xff;
			pOutBuffer[3] = (iCount >> 24) & 0xff;
			*pcDataReturned = 4;
		} else
			iRes = ERROR_INVALID_PARAMETER;
		break;

	case BTH_HCI_IOCTL_GET_LAST_INQUIRY_DATA:
		{
			RetireInquiryData ();
			if ((cInBuffer == sizeof(BD_ADDR)) && (cOutBuffer == sizeof(InquiryResultBuffer))) {
				BD_ADDR b = *(BD_ADDR *)pInBuffer;
				InquiryResultList *pInq = gpHCI->pInquiryResults;
				while (pInq && (pInq->irb.ba != b))
					pInq = pInq->pNext;

				InquiryResultBuffer *pirb = pInq ? &pInq->irb : NULL;

#if defined (BTH_LONG_TERM_STATE)
				InquiryResultBuffer irb;

				if (! pirb) {
					unsigned char modes[3];

					if (btutil_RetrievePeerInfo (&b, BTH_STATE_HCI_SCAN_MODES, modes)) {
						IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_io_control [BTH_HCI_IOCTL_GET_LAST_INQUIRY_DATA] satisfied from long-term cache\n"));

						memset (&irb, 0, sizeof(irb));
						irb.page_scan_repetition_mode   = modes[0];
						irb.page_scan_period_mode       = modes[1];
						irb.page_scan_mode              = modes[2];
                        irb.ba                          = b;

						pirb = &irb;
					}
				}
#endif

				if (pirb) {
					memcpy (pOutBuffer, pirb, sizeof(*pirb));
					iRes = ERROR_SUCCESS;
					*pcDataReturned = sizeof(pInq->irb);
				} else
					iRes = ERROR_NOT_FOUND;
			} else
				iRes = ERROR_INVALID_PARAMETER;
		}
		break;

	case BTH_HCI_IOCTL_GET_REMOTE_COD:
		{
			if ((cInBuffer == sizeof(BD_ADDR)) && (cOutBuffer == sizeof(unsigned int))) {
				BD_ADDR b;
				memcpy (&b, pInBuffer, sizeof(BD_ADDR));
				BasebandConnection *pC = gpHCI->pConnections;
				iRes = ERROR_NOT_FOUND;
				while (pC) {
					if ((pC->ba == b) && (pC->link_type == 1)) {
						iRes = ERROR_SUCCESS;
						memcpy (pOutBuffer, &pC->remote_cod, sizeof(unsigned int));
						*pcDataReturned = sizeof(unsigned int);
						break;
					}
					pC = pC->pNext;
				}
			} else
				iRes = ERROR_INVALID_PARAMETER;
		}
		break;

	case BTH_HCI_IOCTL_GET_BASEBAND_HANDLES:
		{
			unsigned int cHandles = 0;
			BasebandConnection *pC = gpHCI->pConnections;
			while (pC) {
				++cHandles;
				pC = pC->pNext;
			}

			if (cInBuffer == 0) {
				*pcDataReturned = cHandles * sizeof(unsigned short);

				if ((unsigned int)cOutBuffer >= cHandles * sizeof(unsigned short)) {
					iRes = ERROR_SUCCESS;

					pC = gpHCI->pConnections;
					unsigned short *pHandles = (unsigned short *)pOutBuffer;
					while (pC) {
						*pHandles++ = pC->connection_handle;
						pC = pC->pNext;
					}
				} else {
					iRes = ERROR_INSUFFICIENT_BUFFER;
				}
			} else
				iRes = ERROR_INVALID_PARAMETER;
		}
		break;

	case BTH_HCI_IOCTL_GET_HANDLE_MODE:
		if ((cInBuffer == 2) && (cOutBuffer == sizeof(unsigned char))) {
			unsigned short h = pInBuffer[0] | (pInBuffer[1] << 8);
			BasebandConnection *pC = FindConnection (h);
			if (pC) {
				iRes = ERROR_SUCCESS;
				*(unsigned char *)pOutBuffer = pC->fMode;
				*pcDataReturned = sizeof(unsigned char);
			} else
				iRes = ERROR_CONNECTION_INVALID;
		} else
			iRes = ERROR_INVALID_PARAMETER;

		break;

	case BTH_HCI_IOCTL_GET_SCO_PARAMETERS:
		if (cInBuffer == 0) {
			if (cOutBuffer >= 3 * sizeof(unsigned int)) {
				unsigned int *pData = (unsigned int*) pOutBuffer;

				*pcDataReturned = 3 * sizeof(unsigned int);
				iRes = ERROR_SUCCESS;

				if (gpHCI->transport.iScoWriteMaxPacketSize < 0) {
					*pData++ = FALSE;
					*pData++ = 0;
					*pData++ = 0;
				} else if (gpHCI->transport.iScoWriteMaxPacketSize == 0) {
					*pData++ = TRUE;
					*pData++ = (unsigned int) gpHCI->sDeviceBuffer.SCO_Data_Packet_Length;
					*pData++ = (unsigned int) gpHCI->sDeviceBuffer.Total_Num_SCO_Data_Packets;
				} else if (gpHCI->transport.iScoWriteMaxPacketSize > 3) {
					*pData++ = TRUE;
					*pData++ = (unsigned int) (gpHCI->transport.iScoWriteMaxPacketSize - 3);
					if (gpHCI->transport.iScoWriteNumPackets > 0) {
						*pData++ = (unsigned int) gpHCI->transport.iScoWriteNumPackets;
					} else {
						*pData++ = (unsigned int) gpHCI->sDeviceBuffer.Total_Num_SCO_Data_Packets;
					}
				} else {
					SVSUTIL_ASSERT(0);
					iRes = ERROR_INTERNAL_ERROR;
					*pcDataReturned = 0;
				}
			} else {
				iRes = ERROR_INSUFFICIENT_BUFFER;
			}
		} else {
			iRes = ERROR_INVALID_PARAMETER;
		}
		break;

	case BTH_HCI_IOCTL_GET_BASEBAND_CONNECTIONS:
		if (cInBuffer == 0) {
			unsigned int cHandles = 0;
			BasebandConnection *pC = gpHCI->pConnections;
			while (pC) {
				++cHandles;
				pC = pC->pNext;
			}

			*pcDataReturned = cHandles * sizeof(BASEBAND_CONNECTION_DATA);

			if ((unsigned int)cOutBuffer >= cHandles * sizeof(BASEBAND_CONNECTION_DATA)) {
				iRes = ERROR_SUCCESS;

				BASEBAND_CONNECTION_DATA *pData = (BASEBAND_CONNECTION_DATA *)pOutBuffer;
				pC = gpHCI->pConnections;
				while (pC) {
					memcpy (&pData->baAddress, &pC->ba, sizeof(BD_ADDR));
					pData->hConnection			= pC->connection_handle;
					pData->cDataPacketsPending	= pC->cAclDataPacketsPending;
					pData->fAuthenticated		= pC->fAuthenticated;
					pData->fEncrypted			= pC->fEncrypted;
					pData->fLinkType			= pC->link_type;
					pData->fMode				= pC->fMode;

					pC = pC->pNext;
					pData++;
				}
			} else {
				iRes = ERROR_INSUFFICIENT_BUFFER;
			}
		} else {
			iRes = ERROR_INVALID_PARAMETER;
		}
		break;

	case BTH_HCI_IOCTL_GET_HARDWARE_STATUS:
		if ((cInBuffer == 0) && (cOutBuffer == sizeof(int))) {
			iRes = ERROR_INTERNAL_ERROR;
			HANDLE h = CreateThread (NULL, 0, GetHardwareState, NULL, 0, NULL);
			if (h) {
				gpHCI->Unlock ();

				DWORD dwExit;

				if ((WAIT_OBJECT_0 == WaitForSingleObject (h, INFINITE)) && GetExitCodeThread (h, &dwExit)) {
					iRes = ERROR_SUCCESS;
					memcpy (pOutBuffer, &dwExit, sizeof(int));
					*pcDataReturned = sizeof(DWORD);
				}

				return iRes;
			}
		} else
			iRes = ERROR_INVALID_PARAMETER;
		break;
	}

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_io_control exits with code 0x%08x\n", iRes));

	gpHCI->Unlock ();

	return iRes;
}

static int bt_abort_call (HANDLE hDeviceContext, void *pCallContext) {
	if (! gpHCI) {
		IFDBG(DebugOut (DEBUG_ERROR, L"bt_abort_call returns ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpHCI->Lock ();

	int iRes = ERROR_INVALID_HANDLE;
	HCI_CONTEXT *pOwner = VerifyContext ((HCI_CONTEXT *)hDeviceContext);
	if (pOwner) {
		iRes = RetireCall (NULL, pOwner, pCallContext, ERROR_CANCELLED);
	}

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_abort_call returns %d\n", iRes));
	gpHCI->Unlock ();
	return iRes;
}

static int bt_custom_code_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
unsigned short	usOpCode,
unsigned short	cPacketSize,
unsigned char	*pPacketBody,
PacketMarker    *pMarker,
unsigned char	cEvent
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_custom_code_in [0x%08x/0x%08x code 0x%04x]\n", hDeviceContext, pCallContext, usOpCode));

	if (! gpHCI) {
		IFDBG(DebugOut (DEBUG_ERROR, L"bt_custom_code_in returns ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpHCI->Lock ();

	if (gpHCI->eStage != Connected) {
		IFDBG(DebugOut (DEBUG_ERROR, L"bt_custom_code_in returns ERROR_SERVICE_NOT_ACTIVE\n"));
		gpHCI->Unlock ();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	if ((cPacketSize > (gpHCI->transport.iMaxSizeWrite - 3)) || (cPacketSize > HCI_BUFFER - 3) ||
			(pMarker && (pMarker->fMarker != BTH_MARKER_CONNECTION) &&
			(pMarker->fMarker != BTH_MARKER_ADDRESS))) {
		IFDBG(DebugOut (DEBUG_ERROR, L"bt_custom_code_in returns ERROR_INVALID_PARAMETER\n"));
		gpHCI->Unlock ();
		return ERROR_INVALID_PARAMETER;
	}

	HCI_CONTEXT *pContext = VerifyContext ((HCI_CONTEXT *)hDeviceContext);
	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"bt_custom_code_in returns ERROR_INVALID_HANDLE\n"));
		gpHCI->Unlock ();
		return ERROR_INVALID_HANDLE;
	}

	int iRes = bt_custom_code_internal (pContext, pCallContext, usOpCode, cPacketSize, pPacketBody, pMarker, cEvent);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_custom_code_in returns %d\n", iRes));
	gpHCI->Unlock ();
	return iRes;
}

static int bt_create_connection_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
BD_ADDR			*pba,
unsigned short	packet_type,
unsigned char	page_scan_repetition_mode,
unsigned char	page_scan_mode,
unsigned short	clock_offset,
unsigned char	allow_role_switch
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_create_connection_in [0x%08x/0x%08x to %04x%08x]\n", hDeviceContext, pCallContext, pba->NAP, pba->SAP));

	if (gpHCI) {
		gpHCI->Lock ();

		if ((gpHCI->eStage == Connected) && (gpHCI->transport.uiFlags & HCI_FLAGS_NOROLESWITCH)) {
			IFDBG(DebugOut (DEBUG_ERROR, L"bt_create_connection_in : disabling role switch\n"));
			allow_role_switch = 0;
		}

		gpHCI->Unlock ();
	}

	unsigned char body[sizeof(BD_ADDR) + 7];
	memcpy (body, pba, sizeof(BD_ADDR));

	body[sizeof(BD_ADDR)] = packet_type & 0xff;
	body[sizeof(BD_ADDR) + 1] = (packet_type >> 8) & 0xff;
	body[sizeof(BD_ADDR) + 2] = page_scan_repetition_mode;
	body[sizeof(BD_ADDR) + 3] = page_scan_mode;
	body[sizeof(BD_ADDR) + 4] = clock_offset & 0xff;
	body[sizeof(BD_ADDR) + 5] = (clock_offset >> 8) & 0xff;
	body[sizeof(BD_ADDR) + 6] = allow_role_switch;

	PacketMarker m;
	m.ba = *pba;
	m.fMarker = BTH_MARKER_ADDRESS;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Create_Connection,
						sizeof(body),
						body, &m, HCI_Connection_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_create_connection_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_remote_name_request_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
BD_ADDR			*pba,
unsigned char	page_scan_repetition_mode,
unsigned char	page_scan_mode,
unsigned short	clock_offset
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_remote_name_request_in [0x%08x/0x%08x to %04x%08x]\n", hDeviceContext, pCallContext, pba->NAP, pba->SAP));

	unsigned char body[sizeof(BD_ADDR) + 4];
	memcpy (body, pba, sizeof(BD_ADDR));
	body[sizeof(BD_ADDR)] = page_scan_repetition_mode;
	body[sizeof(BD_ADDR) + 1] = page_scan_mode;
	body[sizeof(BD_ADDR) + 2] = clock_offset & 0xff;
	body[sizeof(BD_ADDR) + 3] = (clock_offset >> 8) & 0xff;

	PacketMarker m;
	m.ba = *pba;
	m.fMarker = BTH_MARKER_ADDRESS;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Remote_Name_Request, sizeof(body), body, &m, HCI_Remote_Name_Request_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_remote_name_request_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_accept_connection_request_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
BD_ADDR			*pba,
unsigned char	role
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_accept_connection_request_in [0x%08x/0x%08x to %04x%08x]\n", hDeviceContext, pCallContext, pba->NAP, pba->SAP));

	if (gpHCI) {
		gpHCI->Lock ();

		if ((gpHCI->eStage == Connected) && (gpHCI->transport.uiFlags & HCI_FLAGS_NOROLESWITCH)) {
			IFDBG(DebugOut (DEBUG_ERROR, L"bt_accept_connection_request_in : disabling role switch\n"));
			role = 1;
		}

		gpHCI->Unlock ();
	}

	unsigned char body[sizeof(BD_ADDR) + 1];
	memcpy (body, pba, sizeof(BD_ADDR));
	body[sizeof(BD_ADDR)] = role;

	PacketMarker m;
	m.ba = *pba;
	m.fMarker = BTH_MARKER_ADDRESS;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Accept_Connection_Request, sizeof(body), body, &m, HCI_Connection_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_accept_connection_request_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_switch_role_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
BD_ADDR			*pba,
unsigned char	role
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_switch_role_in [0x%08x/0x%08x to %04x%08x]\n", hDeviceContext, pCallContext, pba->NAP, pba->SAP));

	unsigned char body[sizeof(BD_ADDR) + 1];
	memcpy (body, pba, sizeof(BD_ADDR));
	body[sizeof(BD_ADDR)] = role;

	PacketMarker m;
	m.ba = *pba;
	m.fMarker = BTH_MARKER_ADDRESS;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Switch_Role, sizeof(body), body, &m, HCI_Role_Change_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_switch_role_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_pin_code_request_negative_reply_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
BD_ADDR			*pba
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_pin_code_request_negative_reply_in [0x%08x/0x%08x to %04x%08x]\n", hDeviceContext, pCallContext, pba->NAP, pba->SAP));

	unsigned char body[sizeof(BD_ADDR)];
	memcpy (body, pba, sizeof(BD_ADDR));

	PacketMarker m;
	m.ba = *pba;
	m.fMarker = BTH_MARKER_ADDRESS;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_PIN_Code_Request_Negative_Reply, sizeof(body), body, &m, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_pin_code_request_negative_reply_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_link_key_request_negative_reply_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
BD_ADDR			*pba
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_link_key_request_negative_reply_in [0x%08x/0x%08x to %04x%08x]\n", hDeviceContext, pCallContext, pba->NAP, pba->SAP));

	unsigned char body[sizeof(BD_ADDR)];
	memcpy (body, pba, sizeof(BD_ADDR));

	PacketMarker m;
	m.ba = *pba;
	m.fMarker = BTH_MARKER_ADDRESS;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Link_Key_Request_Negative_Reply, sizeof(body), body, &m, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_link_key_request_negative_reply_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_pin_code_request_reply_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
BD_ADDR			*pba,
unsigned char	cPinLength,
unsigned char	*ppin
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_pin_code_request_reply_in [0x%08x/0x%08x to %04x%08x]\n", hDeviceContext, pCallContext, pba->NAP, pba->SAP));

	if ((cPinLength > 16) || (cPinLength < 1)) {
		IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_pin_code_request_reply_in completed with ERROR_INVALID_PARAMETER\n"));
		return ERROR_INVALID_PARAMETER;
	}

	unsigned char body[sizeof(BD_ADDR) + 1 + 16];
	memset (body, 0, sizeof(body));
	memcpy (body, pba, sizeof(BD_ADDR));
	body[sizeof(BD_ADDR)] = cPinLength & 0xff;
	memcpy (body + sizeof(BD_ADDR) + 1, ppin, cPinLength);

	PacketMarker m;
	m.ba = *pba;
	m.fMarker = BTH_MARKER_ADDRESS;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_PIN_Code_Request_Reply, sizeof(body), body, &m, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_pin_code_request_reply_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_link_key_request_reply_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
BD_ADDR			*pba,
unsigned char	*pkey
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_link_key_request_reply_in [0x%08x/0x%08x to %04x%08x]\n", hDeviceContext, pCallContext, pba->NAP, pba->SAP));

	unsigned char body[sizeof(BD_ADDR) + 16];
	memcpy (body, pba, sizeof(BD_ADDR));
	memcpy (body + sizeof(BD_ADDR), pkey, 16);

	PacketMarker m;
	m.ba = *pba;
	m.fMarker = BTH_MARKER_ADDRESS;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Link_Key_Request_Reply, sizeof(body), body, &m, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_link_key_request_reply_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_authentication_requested_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
unsigned short	connection_handle
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_authentication_requested_in [0x%08x/0x%08x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle));

	unsigned char body[2];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle = connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Authentication_Requested, sizeof(body), body, &m, HCI_Authentication_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_authentication_requested_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_set_connection_encryption_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
unsigned short	connection_handle,
unsigned char	encryption_flag
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_set_connection_encryption_in [0x%08x/0x%08x 0x%04x %d]\n", hDeviceContext, pCallContext, connection_handle, encryption_flag));

	unsigned char body[3];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;
	body[2] = encryption_flag;

	PacketMarker m;
	m.connection_handle = connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Set_Connection_Encryption, sizeof(body), body, &m, HCI_Encryption_Change_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_authentication_requested_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_reject_connection_request_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
BD_ADDR			*pba,
unsigned char	reason
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_reject_connection_request_in [0x%08x/0x%08x to %04x%08x]\n", hDeviceContext, pCallContext, pba->NAP, pba->SAP));

	unsigned char body[sizeof(BD_ADDR) + 1];
	memcpy (body, pba, sizeof(BD_ADDR));
	body[sizeof(BD_ADDR)] = reason;

	PacketMarker m;
	m.ba = *pba;
	m.fMarker = BTH_MARKER_ADDRESS;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Reject_Connection_Request, sizeof(body), body, &m, HCI_Connection_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_reject_connection_request_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_change_local_name_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned char utf_name[248]
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_change_local_name_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	if (gpHCI) {
		gpHCI->Lock ();

		if ((gpHCI->eStage == Connected) && (gpHCI->transport.uiFlags & HCI_FLAGS_NOLOCALNAME)) {
			IFDBG(DebugOut (DEBUG_ERROR, L"bt_change_local_name_in returns ERROR_NOT_SUPPORTED\n"));
			gpHCI->Unlock ();
			return ERROR_NOT_SUPPORTED;
		}

		gpHCI->Unlock ();
	}

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Change_Local_Name, 248, utf_name, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_change_local_name_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_read_local_name_in
(
HANDLE hDeviceContext,
void *pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_local_name_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Local_Name, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_local_name_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_data_packet_down_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
unsigned short	h,
BD_BUFFER		*pBuffer
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_data_packet_down_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	if (! pBuffer) {
		IFDBG(DebugOut (DEBUG_WARN, L"bt_data_packet_down_in: No buffer\n"));
		return ERROR_INVALID_PARAMETER;
	}

	if (! gpHCI) {
		IFDBG(DebugOut (DEBUG_ERROR, L"bt_data_packet_down_in returns ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpHCI->Lock ();

	if (gpHCI->eStage != Connected) {
		IFDBG(DebugOut (DEBUG_ERROR, L"bt_data_packet_down_in returns ERROR_SERVICE_NOT_ACTIVE\n"));
		gpHCI->Unlock ();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	HCI_CONTEXT *pContext = VerifyContext ((HCI_CONTEXT *)hDeviceContext);
	BasebandConnection *pC = FindConnection (h);
	if (! (pContext && pC)) {
		IFDBG(DebugOut (DEBUG_ERROR, L"bt_data_packet_down_in returns ERROR_INVALID_HANDLE\n"));
		gpHCI->Unlock ();
		return ERROR_INVALID_HANDLE;
	}

	if ( (gpHCI->transport.iScoWriteMaxPacketSize < 0) && (pC->link_type == 0) ) {
		IFDBG(DebugOut (DEBUG_ERROR, L"bt_data_packet_down_in: returns ERROR_NOT_SUPPORTED (Transport does not support SCO data)\n"));
		gpHCI->Unlock ();
		return ERROR_NOT_SUPPORTED;
	}

	HCIPacket *pP = new HCIPacket (pC->link_type == 1 ? DATA_PACKET_ACL : DATA_PACKET_SCO,
							pContext, pCallContext, 0);
	if (! pP) {
		IFDBG(DebugOut (DEBUG_ERROR, L"bt_data_packet_down_in returns ERROR_OUTOFMEMORY\n"));
		gpHCI->Unlock ();
		return ERROR_OUTOFMEMORY;
	}

	pP->pContents = pBuffer->fMustCopy ? BufferCopy (pBuffer) : pBuffer;

	if (! pBuffer) {
		delete pP;
		IFDBG(DebugOut (DEBUG_WARN, L"bt_data_packet_down_in returns ERROR_OUTOFMEMORY\n"));
		return ERROR_OUTOFMEMORY;
	}

	pP->uPacket.DataPacketU.hConnection = h;
	pP->uPacket.DataPacketU.cTotal = BufferTotal (pBuffer);

	SchedulePacket (pP);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_data_packet_down_in returns ERROR_SUCCESS\n"));
	gpHCI->Unlock ();
	return ERROR_SUCCESS;
}

static int bt_disconnect_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
unsigned short	connection_handle,
unsigned char	reason
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_disconnect_in [0x%08x/0x%08x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle));

	unsigned char body[3];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;
	body[2] = reason;

	PacketMarker m;
	m.connection_handle = connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Disconnect, sizeof(body), body, &m, HCI_Disconnection_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_disconnect_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_add_sco_connection_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
unsigned short	connection_handle,
unsigned short	packet_type
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_add_sco_connection_in [0x%08x/0x%08x 0x%04x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle, packet_type));

	unsigned char body[4];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;
	body[2] = packet_type & 0xff;
	body[3] = (packet_type >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle = connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Add_SCO_Connection, sizeof(body), body, &m, HCI_Connection_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_add_sco_connection_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_role_discovery_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
unsigned short	connection_handle
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_role_discovery_in [0x%08x/0x%08x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle));

	unsigned char body[2];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle = connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Role_Discovery, sizeof(body), body, &m, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_role_discovery_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_inquiry_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
unsigned int	LAP,
unsigned char	length,
unsigned char	num_responses
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_inquiry_in [0x%08x/0x%08x LAP = 0x%08x length = %d num = %d]\n", hDeviceContext, pCallContext, LAP, length, num_responses));

	unsigned char body[5];
	body[0] = LAP & 0xff;
	body[1] = (LAP >> 8) & 0xff;
	body[2] = (LAP >> 16) & 0xff;
	body[3] = length;
	body[4] = num_responses;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Inquiry, sizeof(body), body, NULL, HCI_Inquiry_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_inquiry_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_inquiry_cancel_in
(
HANDLE			hDeviceContext,
void			*pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_inquiry_cancel_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Inquiry_Cancel, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_inquiry_cancel_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_periodic_inquiry_mode_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
unsigned short	max_period,
unsigned short  min_period,
unsigned int	LAP,
unsigned char	length,
unsigned char	num_responses
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_periodic_inquiry_mode_in [0x%08x/0x%08x LAP = 0x%08x length = %d num = %d]\n", hDeviceContext, pCallContext, LAP, length, num_responses));

	unsigned char body[9];
	body[0] = max_period & 0xff;
	body[1] = (max_period >> 8) & 0xff;
	body[2] = min_period & 0xff;
	body[3] = (min_period >> 8) & 0xff;
	body[4] = LAP & 0xff;
	body[5] = (LAP >> 8) & 0xff;
	body[6] = (LAP >> 24) & 0xff;
	body[7] = length;
	body[8] = num_responses;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Periodic_Inquiry_Mode, sizeof(body), body, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_periodic_inquiry_mode_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_exit_periodic_inquiry_mode_in
(
HANDLE			hDeviceContext,
void			*pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_exit_periodic_inquiry_mode_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Exit_Periodic_Inquiry_Mode, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_exit_periodic_inquiry_mode_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_read_bd_addr_in
(
HANDLE			hDeviceContext,
void			*pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_bd_addr_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_BD_ADDR, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_bd_addr_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_read_buffer_size_in
(
HANDLE			hDeviceContext,
void			*pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_buffer_size_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Buffer_Size, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_buffer_size_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_read_scan_enable_in
(
HANDLE			hDeviceContext,
void			*pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_scan_enable_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Scan_Enable, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_scan_enable_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_reset_in
(
HANDLE			hDeviceContext,
void			*pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_reset_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Reset, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_reset_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_write_scan_enable_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
unsigned char	scan_enable
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_scan_enable_in [0x%08x/0x%08x enable = %d]\n", hDeviceContext, pCallContext, scan_enable));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_Scan_Enable, 1, &scan_enable, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_scan_enable_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_write_class_of_device_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
unsigned int	class_of_device
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_class_of_device_in [0x%08x/0x%08x class = 0x%08x]\n", hDeviceContext, pCallContext, class_of_device));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_Class_Of_Device, 3, (unsigned char *)&class_of_device, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_class_of_device_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_read_class_of_device_in
(
HANDLE			hDeviceContext,
void			*pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_class_of_device_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Class_Of_Device, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_class_of_device_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_write_voice_setting_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
unsigned short voice_setting
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_voice_setting_in [0x%08x/0x%08x voicesetting = 0x%08x]\n", hDeviceContext, pCallContext, voice_setting));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_Voice_Setting, 2, (unsigned char *)&voice_setting, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_voice_setting_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_read_voice_setting_in
(
HANDLE			hDeviceContext,
void			*pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_voice_setting_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Voice_Setting, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_voice_setting_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_set_host_controller_to_host_flow_control_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
unsigned char flow_control_enable
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_set_host_controller_to_host_flow_control_in [0x%08x/0x%08x flowcontrol = 0x%08x]\n", hDeviceContext, pCallContext, flow_control_enable));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Set_Host_Controller_To_Host_Flow_Control, 1, (unsigned char *)&flow_control_enable, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_set_host_controller_to_host_flow_control_in completed with status %d\n", iRes));

	return iRes;
}


static int bt_host_buffer_size_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
unsigned short  host_acl_data_packet_length,
unsigned char   host_sco_data_packet_length,
unsigned short  host_total_num_acl_data_packets,
unsigned short  host_total_num_sco_data_packets
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_host_buffer_size_in [0x%08x/0x%08x acl len = %d, sco len = %d, acl # = %d, sco # = %d]\n", hDeviceContext, pCallContext, host_acl_data_packet_length, host_sco_data_packet_length, host_total_num_acl_data_packets, host_total_num_sco_data_packets));

	unsigned char body[7];
	body[0] = host_acl_data_packet_length & 0xff;
	body[1] = host_acl_data_packet_length >> 8;
	body[2] = host_sco_data_packet_length;
	body[3] = host_total_num_acl_data_packets & 0xff;
	body[4] = host_total_num_acl_data_packets >> 8;
	body[5] = host_total_num_sco_data_packets & 0xff;
	body[6] = host_total_num_sco_data_packets >> 8;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Host_Buffer_Size, 7, body, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_host_buffer_size_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_host_number_of_completed_packets
(
HANDLE         hDeviceContext,
void           *pCallContext,
unsigned char  number_of_handles,
unsigned short *pconnection_handle_list,
unsigned short *phost_num_of_completed_packets
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_host_number_of_completed_packets [0x%08x/0x%08x # connections = %d]\n", hDeviceContext, pCallContext, number_of_handles));

	unsigned char body[256];
	unsigned char size = 1 + 4 * number_of_handles;
	if (size > 248) {
		IFDBG(DebugOut (DEBUG_ERROR, L"bt_host_number_of_completed_packets [0x%08x/0x%08x], too many handles, does not fit\n", hDeviceContext, pCallContext));
		return ERROR_INVALID_PARAMETER;
	}

	unsigned char *pbody = body;
	*pbody++ = number_of_handles;
	for (int i = 0 ; i < number_of_handles ; ++i) {
		*pbody++ = pconnection_handle_list[i] & 0xff;
		*pbody++ = pconnection_handle_list[i] >> 8;
		*pbody++ = phost_num_of_completed_packets[i] & 0xff;
		*pbody++ = phost_num_of_completed_packets[i] >> 8;
	}

	SVSUTIL_ASSERT ((pbody - body) == size);

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Host_Number_Of_Completed_Packets, size, body, NULL, HCI_NO_EVENT);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_host_number_of_completed_packets completed with status %d\n", iRes));

	return iRes;
}

static int bt_write_sco_flow_control_enable_in
(
HANDLE			hDeviceContext,
void			*pCallContext,
unsigned char sco_flow_control_enable
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_sco_flow_control_enable_in [0x%08x/0x%08x flowcontrol = 0x%08x]\n", hDeviceContext, pCallContext, sco_flow_control_enable));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_SCO_Flow_Control_Enable, 1, (unsigned char *)&sco_flow_control_enable, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_sco_flow_control_enable_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_read_sco_flow_control_enable_in
(
HANDLE			hDeviceContext,
void			*pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_sco_flow_control_enable_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_SCO_Flow_Control_Enable, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_sco_flow_control_enable_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_read_page_timeout_in (HANDLE hDeviceContext, void *pCallContext) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_page_timeout_in [0x%08x/0x%08x\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Page_Timeout, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_page_timeout_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_write_page_timeout_in (HANDLE hDeviceContext, void *pCallContext, unsigned short page_timeout) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_page_timeout_in [0x%08x/0x%08x timeout = 0x%08x]\n", hDeviceContext, pCallContext, page_timeout));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_Page_Timeout, 2, (unsigned char *)&page_timeout, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_page_timeout_in completed with status %d\n", iRes));

	return iRes;
}

static int bt_read_number_of_supported_IAC_in
(
HANDLE hDeviceContext,
void *pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_number_of_supported_IAC_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Number_Of_Supported_IAC, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_number_of_supported_IAC_in completed with status %d\n", iRes));

	return iRes;
}

static bt_change_connection_packet_type_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle,
unsigned short packet_type
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_change_connection_packet_type_in [0x%08x/0x%08x 0x%04x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle, packet_type));

	unsigned char body[4];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;
	body[2] = packet_type & 0xff;
	body[3] = (packet_type >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Change_Connection_Packet_Type, sizeof(body), body, NULL, HCI_Connection_Packet_Type_Changed_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_change_connection_packet_type_in completed with status %d\n", iRes));

	return iRes;
}

static bt_hold_mode_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle,
unsigned short max_interval,
unsigned short min_interval
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_hold_mode_in [0x%08x/0x%08x 0x%04x 0x%04x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle, max_interval, min_interval));

	unsigned char body[6];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;
	body[2] = max_interval & 0xff;
	body[3] = (max_interval >> 8) & 0xff;
	body[4] = min_interval & 0xff;
	body[5] = (min_interval >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Hold_Mode, sizeof(body), body, &m, HCI_Mode_Change_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_hold_mode_in completed with status %d\n", iRes));

	return iRes;
}

static bt_sniff_mode_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle,
unsigned short max_interval,
unsigned short min_interval,
unsigned short attempts,
unsigned short timeout
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_sniff_mode_in [0x%08x/0x%08x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle, max_interval, min_interval, attempts, timeout));

	unsigned char body[10];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;
	body[2] = max_interval & 0xff;
	body[3] = (max_interval >> 8) & 0xff;
	body[4] = min_interval & 0xff;
	body[5] = (min_interval >> 8) & 0xff;
	body[6] = attempts & 0xff;
	body[7] = (attempts >> 8) & 0xff;
	body[8] = timeout & 0xff;
	body[9] = (timeout >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Sniff_Mode, sizeof(body), body, &m, HCI_Mode_Change_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_sniff_mode_in completed with status %d\n", iRes));

	return iRes;
}

static bt_exit_sniff_mode_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_exit_sniff_mode_in [0x%08x/0x%08x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle));

	unsigned char body[2];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Exit_Sniff_Mode, sizeof(body), body, &m, HCI_Mode_Change_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_exit_sniff_mode_in completed with status %d\n", iRes));

	return iRes;
}

static bt_park_mode_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle,
unsigned short max_interval,
unsigned short min_interval
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_park_mode_in [0x%08x/0x%08x 0x%04x 0x%04x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle, max_interval, min_interval));

	unsigned char body[6];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;
	body[2] = max_interval & 0xff;
	body[3] = (max_interval >> 8) & 0xff;
	body[4] = min_interval & 0xff;
	body[5] = (min_interval >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Park_Mode, sizeof(body), body, &m, HCI_Mode_Change_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_park_mode_in completed with status %d\n", iRes));

	return iRes;
}

static bt_exit_park_mode_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_exit_park_mode_in [0x%08x/0x%08x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle));

	unsigned char body[2];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Exit_Park_Mode, sizeof(body), body, &m, HCI_Mode_Change_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_exit_park_mode_in completed with status %d\n", iRes));

	return iRes;
}

static bt_QOS_setup_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle,
unsigned char flags,
unsigned char service_type,
unsigned int token_rate,
unsigned int peak_bandwidth,
unsigned int latency,
unsigned int delay_variation
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_QOS_setup_in [0x%08x/0x%08x 0x%04x 0x%02x 0x%02x 0x%08x 0x%08x 0x%08x]\n", hDeviceContext, pCallContext, connection_handle, flags, service_type, token_rate, peak_bandwidth, latency, delay_variation));

	unsigned char body[20];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;
	body[2] = flags;
	body[3] = service_type;
	body[4] = (unsigned char)(token_rate & 0xff);
	body[5] = (unsigned char)((token_rate >> 8) & 0xff);
	body[6] = (unsigned char)((token_rate >> 16) & 0xff);
	body[7] = (unsigned char)((token_rate >> 24) & 0xff);
	body[8] = (unsigned char)(peak_bandwidth & 0xff);
	body[9] = (unsigned char)((peak_bandwidth >> 8) & 0xff);
	body[10] = (unsigned char)((peak_bandwidth >> 16) & 0xff);
	body[11] = (unsigned char)((peak_bandwidth >> 24) & 0xff);
	body[12] = (unsigned char)(latency & 0xff);
	body[13] = (unsigned char)((latency >> 8) & 0xff);
	body[14] = (unsigned char)((latency >> 16) & 0xff);
	body[15] = (unsigned char)((latency >> 24) & 0xff);
	body[16] = (unsigned char)(delay_variation & 0xff);
	body[17] = (unsigned char)((delay_variation >> 8) & 0xff);
	body[18] = (unsigned char)((delay_variation >> 16) & 0xff);
	body[19] = (unsigned char)((delay_variation >> 24) & 0xff);

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_QoS_Setup, sizeof(body), body, &m, HCI_QoS_Setup_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_QOS_setup_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_link_policy_settings_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_link_policy_settings_in [0x%08x/0x%08x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle));

	unsigned char body[2];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Link_Policy_Settings, sizeof(body), body, &m, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_link_policy_settings_in completed with status %d\n", iRes));

	return iRes;
}

static bt_write_link_policy_settings_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle,
unsigned short link_policy_settings
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_link_policy_settings_in [0x%08x/0x%08x 0x%04x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle, link_policy_settings));

	unsigned char body[4];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;
	body[2] = link_policy_settings & 0xff;
	body[3] = (link_policy_settings >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_Link_Policy_Settings, sizeof(body), body, &m, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_link_policy_settings_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_pin_type_in
(
HANDLE hDeviceContext,
void *pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_pin_type_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_PIN_Type, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_pin_type_in completed with status %d\n", iRes));

	return iRes;
}

static bt_write_pin_type_in
(
HANDLE hDeviceContext,
void *pCallContext,
char pin_type
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_pin_type_in [0x%08x/0x%08x 0x%02x]\n", hDeviceContext, pCallContext, pin_type));

	unsigned char body[1];
	body[0] = pin_type;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_PIN_Type, sizeof(body), body, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_pin_type_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_connection_accept_timeout_in
(
HANDLE hDeviceContext,
void *pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_connection_accept_timeout_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Connection_Accept_Timeout, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_connection_accept_timeout_in completed with status %d\n", iRes));

	return iRes;
}

static bt_write_connection_accept_timeout_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short conn_accept_timeout
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_connection_accept_timeout_in [0x%08x/0x%08x 0x%04x]\n", hDeviceContext, pCallContext, conn_accept_timeout));

	unsigned char body[2];
	body[0] = conn_accept_timeout & 0xff;
	body[1] = (conn_accept_timeout >> 8) & 0xff;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_Connection_Accept_Timeout, sizeof(body), body, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_connection_accept_timeout_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_page_scan_activity_in
(
HANDLE hDeviceContext,
void *pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_page_scan_activity_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_PageScan_Activity, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_page_scan_activity_in completed with status %d\n", iRes));

	return iRes;
}

static bt_write_page_scan_activity_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short interval,
unsigned short window
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_page_scan_activity_in [0x%08x/0x%08x 0x%04x 0x%04x]\n", hDeviceContext, pCallContext, interval, window));

	unsigned char body[4];
	body[0] = interval & 0xff;
	body[1] = (interval >> 8) & 0xff;
	body[2] = window & 0xff;
	body[3] = (window >> 8) & 0xff;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_PageScan_Activity, sizeof(body), body, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_page_scan_activity_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_inquiry_scan_activity_in
(
HANDLE hDeviceContext,
void *pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_inquiry_scan_activity_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_InquiryScan_Activity, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_inquiry_scan_activity_in completed with status %d\n", iRes));

	return iRes;
}

static bt_write_inquiry_scan_activity_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short interval,
unsigned short window
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_inquiry_scan_activity_in [0x%08x/0x%08x 0x%04x 0x%04x]\n", hDeviceContext, pCallContext, interval, window));

	unsigned char body[4];
	body[0] = interval & 0xff;
	body[1] = (interval >> 8) & 0xff;
	body[2] = window & 0xff;
	body[3] = (window >> 8) & 0xff;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_InquiryScan_Activity, sizeof(body), body, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_inquiry_scan_activity_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_authentication_enable_in
(
HANDLE hDeviceContext,
void *pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_authentication_enable_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Authentication_Enable, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_authentication_enable_in completed with status %d\n", iRes));

	return iRes;
}

static bt_write_authentication_enable_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned char enable
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_authentication_enable_in [0x%08x/0x%08x 0x%02x]\n", hDeviceContext, pCallContext, enable));

	unsigned char body[1];
	body[0] = enable;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_Authentication_Enable, sizeof(body), body, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_authentication_enable_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_encryption_mode_in
(
HANDLE hDeviceContext,
void *pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_encryption_mode_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Encryption_Mode, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_encryption_mode_in completed with status %d\n", iRes));

	return iRes;
}

static bt_write_encryption_mode_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned char mode
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_encryption_mode_in [0x%08x/0x%08x 0x%02x]\n", hDeviceContext, pCallContext, mode));

	unsigned char body[1];
	body[0] = mode;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_Encryption_Mode, sizeof(body), body, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_encryption_mode_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_hold_mode_activity_in
(
HANDLE hDeviceContext,
void *pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_hold_mode_activity_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Hold_Mode_Activity, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_hold_mode_activity_in completed with status %d\n", iRes));

	return iRes;
}

static bt_write_hold_mode_activity_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned char activity
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_hold_mode_activity_in [0x%08x/0x%08x 0x%02x]\n", hDeviceContext, pCallContext, activity));

	unsigned char body[1];
	body[0] = activity;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_Hold_Mode_Activity, sizeof(body), body, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_hold_mode_activity_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_link_supervision_timeout_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_link_supervision_timeout_in [0x%08x/0x%08x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle));

	unsigned char body[2];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Link_Supervision_Timeout, sizeof(body), body, &m, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_link_supervision_timeout_in completed with status %d\n", iRes));

	return iRes;
}

static bt_write_link_supervision_timeout_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle,
unsigned short timeout
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_link_supervision_timeout_in [0x%08x/0x%08x 0x%04x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle, timeout));

	unsigned char body[4];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;
	body[2] = timeout & 0xff;
	body[3] = (timeout >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_Link_Supervision_Timeout, sizeof(body), body, &m, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_link_supervision_timeout_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_current_IAC_LAP_in
(
HANDLE hDeviceContext,
void *pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_current_IAC_LAP_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Current_IAC_LAP, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_current_IAC_LAP_in completed with status %d\n", iRes));

	return iRes;
}

static bt_write_current_IAC_LAP_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned char number_current_IAC,
unsigned char *pIAC_LAP
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_current_IAC_LAP_in [0x%08x/0x%08x 0x%02x 0x%08x]\n", hDeviceContext, pCallContext, number_current_IAC, pIAC_LAP));

	const int ciMaxIACs	= 0x40;		// From BT Core Spec 1.1, section H:1 4.7.47 (page 692)

	if (number_current_IAC > ciMaxIACs) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[HCI] bt_write_current_IAC_LAP_in : ERROR_INVALID_PARAMETER (too many IACs)\n"));
		return ERROR_INVALID_PARAMETER;
	}

	unsigned char body[1 + 3 * ciMaxIACs];
	body[0] = number_current_IAC;
	memcpy( &body[1], pIAC_LAP, 3 * number_current_IAC);

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_Current_IAC_LAP, 1 + 3 * number_current_IAC, body, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_current_IAC_LAP_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_page_scan_period_mode_in
(
HANDLE hDeviceContext,
void *pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_page_scan_period_mode_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Page_Scan_Period_Mode, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_page_scan_period_mode_in completed with status %d\n", iRes));

	return iRes;
}

static bt_write_page_scan_period_mode_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned char page_scan_period_mode 
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_page_scan_period_mode_in [0x%08x/0x%08x 0x%02x]\n", hDeviceContext, pCallContext, page_scan_period_mode));

	unsigned char body[1];
	body[0] = page_scan_period_mode;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_Page_Scan_Period_Mode, sizeof(body), body, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_page_scan_period_mode_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_page_scan_mode_in
(
HANDLE hDeviceContext,
void *pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_page_scan_mode_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Page_Scan_Mode, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_page_scan_mode_in completed with status %d\n", iRes));

	return iRes;
}

static bt_write_page_scan_mode_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned char page_scan_mode 
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_page_scan_mode_in [0x%08x/0x%08x 0x%02x]\n", hDeviceContext, pCallContext, page_scan_mode));

	unsigned char body[1];
	body[0] = page_scan_mode;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_Page_Scan_Mode, sizeof(body), body, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_page_scan_mode_in completed with status %d\n", iRes));

	return iRes;
}

static bt_change_connection_link_key_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_change_connection_link_key_in [0x%08x/0x%08x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle));

	unsigned char body[2];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Change_Connection_Link_Key, sizeof(body), body, &m, HCI_Change_Connection_Link_Key_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_change_connection_link_key_in completed with status %d\n", iRes));

	return iRes;
}

static bt_master_link_key_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned char key_flag
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_master_link_key_in [0x%08x/0x%08x 0x%02x]\n", hDeviceContext, pCallContext, key_flag));

	unsigned char body[1];
	body[0] = key_flag;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Master_Link_Key, sizeof(body), body, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_master_link_key_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_remote_supported_features_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_remote_supported_features_in [0x%08x/0x%08x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle));

	unsigned char body[2];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Remote_Supported_Features, sizeof(body), body, &m, HCI_Read_Remote_Supported_Features_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_remote_supported_features_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_remote_version_information_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_remote_version_information_in [0x%08x/0x%08x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle));

	unsigned char body[2];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Remote_Version_Information, sizeof(body), body, &m, HCI_Read_Remote_Version_Information_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_remote_version_information_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_clock_offset_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_clock_offset_in [0x%08x/0x%08x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle));

	unsigned char body[2];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Clock_Offset, sizeof(body), body, &m, HCI_Read_Clock_Offset_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_clock_offset_in completed with status %d\n", iRes));

	return iRes;
}

static bt_flush_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_flush_in [0x%08x/0x%08x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle));

	unsigned char body[2];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Flush, sizeof(body), body, &m, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_flush_in completed with status %d\n", iRes));

	return iRes;
}

static bt_create_new_unit_key_in
(
HANDLE hDeviceContext,
void *pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_create_new_unit_key_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Create_New_Unit_Key, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_create_new_unit_key_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_automatic_flush_timeout_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_automatic_flush_timeout_in [0x%08x/0x%08x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle));

	unsigned char body[2];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Automatic_Flush_Timeout, sizeof(body), body, &m, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_automatic_flush_timeout_in completed with status %d\n", iRes));

	return iRes;
}

static bt_write_automatic_flush_timeout_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle,
unsigned short flush_timeout
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_automatic_flush_timeout_in [0x%08x/0x%08x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle));

	unsigned char body[4];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;
	body[2] = flush_timeout & 0xff;
	body[3] = (flush_timeout >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_Automatic_Flush_Timeout, sizeof(body), body, &m, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_automatic_flush_timeout_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_local_version_information_in
(
HANDLE hDeviceContext,
void *pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_local_version_information_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Local_Version_Information, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_local_version_information_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_local_supported_features_in
(
HANDLE hDeviceContext,
void *pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_local_supported_features_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Local_Supported_Features, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_local_supported_features_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_country_code_in
(
HANDLE hDeviceContext,
void *pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_country_code_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Country_Code, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_country_code_in completed with status %d\n", iRes));

	return iRes;
}

static bt_get_link_quality_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_get_link_quality_in [0x%08x/0x%08x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle));

	unsigned char body[2];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Get_Link_Quality, sizeof(body), body, &m, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_get_link_quality_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_RSSI_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_RSSI_in [0x%08x/0x%08x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle));

	unsigned char body[2];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_RSSI, sizeof(body), body, &m, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_RSSI_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_transmit_power_level_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle,
unsigned char type
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_transmit_power_level_in [0x%08x/0x%08x 0x%04x 0x%02x]\n", hDeviceContext, pCallContext, connection_handle, type));

	unsigned char body[3];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;
	body[2] = type;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Transmit_Power_Level, sizeof(body), body, &m, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_transmit_power_level_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_failed_contact_counter_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_failed_contact_counter_in [0x%08x/0x%08x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle));

	unsigned char body[2];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Failed_Contact_Counter, sizeof(body), body, &m, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_failed_contact_counter_in completed with status %d\n", iRes));

	return iRes;
}

static bt_reset_failed_contact_counter_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned short connection_handle
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_reset_failed_contact_counter_in [0x%08x/0x%08x 0x%04x]\n", hDeviceContext, pCallContext, connection_handle));

	unsigned char body[2];
	body[0] = connection_handle & 0xff;
	body[1] = (connection_handle >> 8) & 0xff;

	PacketMarker m;
	m.connection_handle	= connection_handle;
	m.fMarker = BTH_MARKER_CONNECTION;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Reset_Failed_Contact_Counter, sizeof(body), body, &m, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_reset_failed_contact_counter_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_num_broadcast_retransmissions_in
(
HANDLE hDeviceContext,
void *pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_num_broadcast_retransmissions_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Num_Broadcast_Retransmissions, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_num_broadcast_retransmissions_in completed with status %d\n", iRes));

	return iRes;
}

static bt_write_num_broadcast_retransmissions_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned char retransmissions
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_num_broadcast_retransmissions_in [0x%08x/0x%08x 0x%02x]\n", hDeviceContext, pCallContext, retransmissions));

	unsigned char body[1];
	body[0] = retransmissions;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_Num_Broadcast_Retransmissions, sizeof(body), body, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_num_broadcast_retransmissions_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_loopback_mode_in
(
HANDLE hDeviceContext,
void *pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_loopback_mode_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Loopback_Mode, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_loopback_mode_in completed with status %d\n", iRes));

	return iRes;
}

static bt_write_loopback_mode_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned char loopback_mode
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_loopback_mode_in [0x%08x/0x%08x 0x%02x]\n", hDeviceContext, pCallContext, loopback_mode));

	unsigned char body[1];
	body[0] = loopback_mode;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_Loopback_Mode, sizeof(body), body, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_loopback_mode_in completed with status %d\n", iRes));

	return iRes;
};

static bt_enable_device_under_test_mode_in
(
HANDLE hDeviceContext,
void *pCallContext
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_enable_device_under_test_mode_in [0x%08x/0x%08x]\n", hDeviceContext, pCallContext));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Enable_Device_Under_Test_Mode, 0, NULL, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_enable_device_under_test_mode_in completed with status %d\n", iRes));

	return iRes;
};

static bt_set_event_mask_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned char event_mask[8]
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_set_event_mask_in [0x%08x/0x%08x 0x%02x%02x%02x%02x%02x%02x%02x%02x]\n", hDeviceContext, pCallContext, event_mask[0], event_mask[1], event_mask[2], event_mask[3], event_mask[4], event_mask[5], event_mask[6], event_mask[7]));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Set_Event_Mask, 8, event_mask, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_set_event_mask_in completed with status %d\n", iRes));

	return iRes;
};

static bt_set_event_filter_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned char filter_type,
unsigned char filter_condition_type,
unsigned char condition[7]
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_set_event_filter_in [0x%08x/0x%08x 0x%02x 0x%02x 0x%02x%02x%02x%02x%02x%02x%02x]\n", hDeviceContext, pCallContext, filter_type, filter_condition_type, condition[0], condition[1], condition[2], condition[3], condition[4], condition[5], condition[6]));

	unsigned char body[2 + 7];
	body[0] = filter_type;
	body[1] = filter_condition_type;
	memcpy (&body[2], condition, 7);

	// We now need to figure out the length of the packet
	int length	= 2;
	int fInvalidParams	= FALSE;

	switch (filter_type) {
		case 0:		// Clear All Filters
			length = 1;
			break;
		case 1:		// Inquiry Result
			switch( filter_condition_type ) {
				case 0:		// Any device responded to an inquiry
					length	+= 0;
					break;
				case 1:		// A device with specific class of device responded to an inquiry
					length	+= 6;
					break;
				case 2:		// A device with specific BD_ADDR responded to an inquiry
					length	+= 6;
					break;
				default:	// Unrecognized filter condition type.
					fInvalidParams = TRUE;
					break;
			}
			break;

		case 2:		// Connection Setup
			switch( filter_condition_type ) {
				case 0:		// Any device requested a connection
					length	+= 1;
					break;
				case 1:		// A device with specific class of device requested a connection
					length	+= 7;
					break;
				case 2:		// A device with specific BD_ADDR requested a connection
					length	+= 7;
					break;
				default:	// Unrecognized filter condition type.
					fInvalidParams = TRUE;
					break;
			}
			break;

		default:	// Unrecognized filter type.
			fInvalidParams = TRUE;
			break;
	}

	int iRes = fInvalidParams ? ERROR_INVALID_PARAMETER : bt_custom_code_in (hDeviceContext,
			pCallContext, HCI_Set_Event_Filter, length, body, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_set_event_filter_in completed with status %d\n", iRes));

	return iRes;
}

static bt_read_stored_link_key_in
(
HANDLE hDeviceContext,
void *pCallContext,
BD_ADDR *pba,
unsigned char read_all_flag 
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_stored_link_key_in [0x%08x/0x%08x %04x%08x]\n", hDeviceContext, pCallContext, pba->NAP, pba->SAP ));

	unsigned char body[7];
	memcpy(body, pba, sizeof(*pba));
	body[6] = read_all_flag;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Read_Stored_Link_Key, sizeof(body), body, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_read_stored_link_key_in completed with status %d\n", iRes));

	return iRes;
}

static bt_write_stored_link_key_in
(
HANDLE hDeviceContext,
void *pCallContext,
unsigned char number_to_write,
BD_ADDR *pba_list,
unsigned char *link_key_list_16bytes_each
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_stored_link_key_in [0x%08x/0x%08x 0x%02x 0x%08x 0x%08x]\n", hDeviceContext, pCallContext, number_to_write, pba_list, link_key_list_16bytes_each));

	const int ciMaxKeys = 0xB;	// From BT Core Spec 1.1

	if (number_to_write > ciMaxKeys) {
		IFDBG(DebugOut (DEBUG_ERROR, L"[HCI] bt_write_stored_link_key_in : ERROR_INVALID_PARAMETER (too many keys)\n"));
		return ERROR_INVALID_PARAMETER;
	}

	const int ciElementSize = sizeof(BD_ADDR) + 16;
	unsigned char body[1 + ciMaxKeys * ciElementSize];

	body[0] = number_to_write;
	unsigned char *p = &body[1];

	for (int i = 0; i < number_to_write ; ++i) {
		memcpy (p, pba_list, sizeof(BD_ADDR));
		p += sizeof(BD_ADDR);

		memcpy (p, link_key_list_16bytes_each, 16);
		p += 16;

		link_key_list_16bytes_each += 16;
		++pba_list;
	}

	SVSUTIL_ASSERT (p - body <= sizeof(body));

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Write_Stored_Link_Key, p - body, body, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_write_stored_link_key_in completed with status %d\n", iRes));

	return iRes;
}

static bt_delete_stored_link_key_in
(
HANDLE hDeviceContext,
void *pCallContext,
BD_ADDR *pba,
unsigned char delete_all_flag 
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_delete_stored_link_key_in [0x%08x/0x%08x %04x%08x]\n", hDeviceContext, pCallContext, pba->NAP, pba->SAP ));

	unsigned char body[7];
	memcpy(body, pba, sizeof(*pba));
	body[6] = delete_all_flag;

	int iRes = bt_custom_code_in (hDeviceContext, pCallContext, HCI_Delete_Stored_Link_Key, sizeof(body), body, NULL, HCI_Command_Complete_Event);

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"bt_delete_stored_link_key_in completed with status %d\n", iRes));

	return iRes;
}

//
//	Device transport state and such
//
//
//	This can ONLY be callsed from hci_connect_transport
//
static void DisconnectInternalContextAndCleanup (HCI_CONTEXT *pOwner, HANDLE hEvent, int fClosePhysical) {
	gpHCI->hInitEvent = NULL;

	if (hEvent)
		CloseHandle (hEvent);

	if (fClosePhysical) {
		HCI_CloseConnection ();

		HCI_LIFE_STAGE eSav = gpHCI->eStage;
		gpHCI->eStage = ShuttingDown;

		if (gpHCI->hQueue)
			SetEvent (gpHCI->hQueue);

		if (gpHCI->hReaderThread) {
			gpHCI->AddRef ();
			gpHCI->Unlock ();
			WaitForSingleObject (gpHCI->hReaderThread, HCI_TIMER_SCALE);
			gpHCI->Lock ();
			while (gpHCI->InCall() > 1) {
				gpHCI->Unlock ();
				Sleep (HCI_TIMER_SCALE / 20);
				gpHCI->Lock ();
			}
			gpHCI->Release ();
			TerminateThread (gpHCI->hReaderThread, 0);
			CloseHandle (gpHCI->hReaderThread);
			gpHCI->hReaderThread = NULL;
		}

		if (gpHCI->hWriterThread) {
			gpHCI->AddRef ();
			gpHCI->Unlock ();
			WaitForSingleObject (gpHCI->hWriterThread, HCI_TIMER_SCALE);
			gpHCI->Lock ();
			while (gpHCI->InCall() > 1) {
				gpHCI->Unlock ();
				Sleep (HCI_TIMER_SCALE / 20);
				gpHCI->Lock ();
			}
			gpHCI->Release ();
			TerminateThread (gpHCI->hWriterThread, 0);
			CloseHandle (gpHCI->hWriterThread);
			gpHCI->hWriterThread = NULL;
		}

		gpHCI->eStage = eSav;
	}

	HCIPacket *pP = gpHCI->pPackets;
	HCIPacket *pPP = NULL;
	while (pP) {
		if (pP->pOwner == pOwner) {
			HCIPacket *pNext = NULL;
			if (pPP)
				pNext = pPP->pNext = pP->pNext;
			else
				pNext = gpHCI->pPackets = pP->pNext;

			delete pP;
			pP = pNext;
			continue;
		}

		pPP = pP;
		pP = pP->pNext;
	}

	pP = gpHCI->pPacketsPending;
	pPP = NULL;
	while (pP) {
		if (pP->pOwner == pOwner) {
			HCIPacket *pNext = NULL;
			if (pPP)
				pNext = pPP->pNext = pP->pNext;
			else
				pNext = gpHCI->pPacketsPending = pP->pNext;

			delete pP;
			pP = pNext;
			continue;
		}

		pPP = pP;
		pP = pP->pNext;
	}

	pP = gpHCI->pPacketsSent;
	pPP = NULL;
	while (pP) {
		if (pP->pOwner == pOwner) {
			HCIPacket *pNext = NULL;
			if (pPP)
				pNext = pPP->pNext = pP->pNext;
			else
				pNext = gpHCI->pPacketsSent = pP->pNext;

			delete pP;
			pP = pNext;
			continue;
		}

		pPP = pP;
		pP = pP->pNext;
	}

	HCI_CONTEXT *pCtxP = NULL;
	HCI_CONTEXT *pCtx  = gpHCI->pContexts;

	while (pCtx && (pCtx != pOwner)) {
		pCtxP = pCtx;
		pCtx = pCtx->pNext;
	}

	if (pCtxP)
		pCtxP->pNext = pCtx->pNext;
	else
		gpHCI->pContexts = gpHCI->pContexts->pNext;

	delete pCtx;

	SVSUTIL_ASSERT (! gpHCI->pPackets);
	SVSUTIL_ASSERT (! gpHCI->pPacketsPending);
	SVSUTIL_ASSERT (! gpHCI->pPacketsSent);
	SVSUTIL_ASSERT (! gpHCI->pPeriodicInquiryOwner);
	SVSUTIL_ASSERT (! gpHCI->pConnections);
	SVSUTIL_ASSERT (! gpHCI->pInquiryResults);

	gpHCI->Reset ();

	if (gpHCI->eStage != ShuttingDown)
		gpHCI->eStage = Disconnected;
}


#define HCI_INTERFACE_VERSION_1_0			0x00010000

static int CheckCompatibleTransport (HCI_PARAMETERS *phcip) {
	if ((gpHCI->transport.uiSize == sizeof(gpHCI->transport)) &&
		(gpHCI->transport.fInterfaceVersion == HCI_INTERFACE_VERSION_1_1))
		return TRUE;

	if ((gpHCI->transport.uiSize == offsetof(HCI_PARAMETERS, iScoWriteNumPackets)) &&
		(gpHCI->transport.fInterfaceVersion == HCI_INTERFACE_VERSION_1_0))
		return TRUE;

	return FALSE;
}

static int hci_connect_transport (void) {
	IFDBG(DebugOut (DEBUG_HCI_INIT, L"hci_connect_transport:: entered\n"));

	for ( ; ; ) {
		if (! gpHCI) {
			IFDBG(DebugOut (DEBUG_ERROR, L"hci_connect_transport:: ERROR_SERVICE_DOES_NOT_EXIST\n"));
			return ERROR_SERVICE_DOES_NOT_EXIST;
		}

		gpHCI->Lock ();
		if (gpHCI->eStage != Disconnected) {
			IFDBG(DebugOut (DEBUG_ERROR, L"hci_connect_transport:: ERROR_SERVICE_ALREADY_RUNNING\n"));
			gpHCI->Unlock ();
			return ERROR_SERVICE_ALREADY_RUNNING;
		}

		if (! gpHCI->InCall ())
			break;

		gpHCI->Unlock ();
		Sleep (100);
	}

	if (! HCI_OpenConnection ()) {
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_connect_transport:: ERROR_MEDIA_UNAVAILABLE\n"));
		gpHCI->Unlock ();
		return ERROR_MEDIA_UNAVAILABLE;
	}

	memset (&gpHCI->transport, 0, sizeof(gpHCI->transport));
	gpHCI->transport.uiSize = sizeof(gpHCI->transport);
	gpHCI->transport.fInterfaceVersion = HCI_INTERFACE_VERSION_1_1;

	if ((! HCI_ReadHciParameters (&gpHCI->transport)) || (! CheckCompatibleTransport (&gpHCI->transport)) ||
		(gpHCI->transport.iMaxSizeRead <= HCI_MAX_COMMAND_LENGTH) || (gpHCI->transport.iMaxSizeWrite <= 255) ||
		(gpHCI->transport.iReadBufferHeader < 0) || (gpHCI->transport.iReadBufferTrailer < 0) ||
		(gpHCI->transport.iWriteBufferHeader < 0) || (gpHCI->transport.iWriteBufferTrailer < 0) ||
		(gpHCI->transport.iWriteBufferHeader & 0x3) || (gpHCI->transport.iReadBufferHeader & 0x3) ||
		(gpHCI->transport.fHardwareVersion < HCI_HARDWARE_VERSION_V_1_0_A) ||
		(gpHCI->transport.fHardwareVersion > HCI_HARDWARE_VERSION_V_1_1) ||
		(! gpHCI->transport.uiWriteTimeout)) {
		HCI_CloseConnection ();

		IFDBG(DebugOut (DEBUG_ERROR, L"hci_connect_transport:: incorrect transport parameters (negative or non-int-aligned numbers, or unsupported version, or bad write timeout ) : ERROR_MEDIA_UNAVAILABLE\n"));
		gpHCI->Unlock ();
		return ERROR_MEDIA_UNAVAILABLE;
	}

	int iThreadPriority = DEFAULT_WORKER_THREAD_PRIORITY;

	HKEY hKey;
	if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"\\Software\\Microsoft\\Bluetooth\\sys", 0, 0, &hKey)) {
		DWORD dwSize = sizeof(int);
		int dwData;
		if (ERROR_SUCCESS == RegQueryValueEx(hKey, L"HCIPriority256", NULL, NULL, (LPBYTE)&dwData, &dwSize)) {
			iThreadPriority = dwData;
		}
		RegCloseKey(hKey);
	}

	SVSUTIL_ASSERT ((! gpHCI->hWriterThread) && (! gpHCI->hReaderThread));

	gpHCI->hWriterThread = CreateThread (NULL, 0, WriterThread, NULL, 0, NULL);
	gpHCI->hReaderThread = CreateThread (NULL, 0, ReaderThread, NULL, 0, NULL);

	CeSetThreadPriority(gpHCI->hWriterThread, iThreadPriority);
	CeSetThreadPriority(gpHCI->hReaderThread, iThreadPriority);

	gpHCI->eStage = AlmostRunning;
	gpHCI->cCommandPacketsAllowed = 1;

	HCI_CONTEXT *pInternalOwner = new HCI_CONTEXT;
	if (! pInternalOwner) {
		HCI_CloseConnection ();
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_connect_transport:: ERROR_OUTOFMEMORY\n"));
		gpHCI->Unlock ();
		return ERROR_OUTOFMEMORY;
	}

	pInternalOwner->uiControl = BTH_CONTROL_DEVICEONLY;

	pInternalOwner->c.hci_Reset_Out                      = InitTrackReset;
	pInternalOwner->c.hci_ReadBufferSize_Out             = InitTrackReadBufferSize;
	pInternalOwner->c.hci_ReadLocalSupportedFeatures_Out = InitTrackReadLSP;
	pInternalOwner->c.hci_CallAborted                    = InitAborted;

	pInternalOwner->pNext = gpHCI->pContexts;
	gpHCI->pContexts = pInternalOwner;

	HANDLE hEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
	if (! hEvent) {
		DisconnectInternalContextAndCleanup (pInternalOwner, NULL, TRUE);
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_connect_transport:: ERROR_OUTOFMEMORY (2)\n"));
		gpHCI->Unlock ();
		return ERROR_OUTOFMEMORY;
	}

	gpHCI->hInitEvent = hEvent;

	InternalCommand Tracker;
	Tracker.hEvent     = hEvent;
	Tracker.status     = 0;
	//
	//	Send HCI_Reset
	//

	Tracker.fCompleted = FALSE;

	for (int i = 0 ; i < 3 && (! (gpHCI->transport.uiFlags & HCI_FLAGS_NORESET)); ++i) {
		HCIPacket *pPacket = new HCIPacket (COMMAND_PACKET, pInternalOwner, (void *)&Tracker, 3);
		if (! (pPacket && pPacket->pContents)) {
			if (pPacket)
				delete pPacket;
			DisconnectInternalContextAndCleanup (pInternalOwner, hEvent, TRUE);
			IFDBG(DebugOut (DEBUG_ERROR, L"hci_connect_transport:: ERROR_OUTOFMEMORY (3)\n"));
			gpHCI->Unlock ();
			return ERROR_OUTOFMEMORY;
		}

		FormatPacketNoArgs (pPacket, HCI_Reset);
		pPacket->uPacket.CommandPacket.eEvent = HCI_Command_Complete_Event;

		SchedulePacket (pPacket);
		IFDBG(DebugOut (DEBUG_HCI_INIT, L"hci_connect_transport:: waiting for reset response\n"));
		gpHCI->AddRef ();
		gpHCI->Unlock ();
		WaitForSingleObject (hEvent, HCI_TIMER_SCALE / 5);
		IFDBG(DebugOut (DEBUG_HCI_INIT, L"hci_connect_transport:: got reset response (before lock)\n"));
		gpHCI->Lock ();
		gpHCI->Release ();
		IFDBG(DebugOut (DEBUG_HCI_INIT, L"hci_connect_transport:: got reset response (in)\n"));

		if (gpHCI->eStage != AlmostRunning) {
			DisconnectInternalContextAndCleanup (pInternalOwner, hEvent, TRUE);
			IFDBG(DebugOut (DEBUG_ERROR, L"hci_connect_transport:: ERROR_DEVICE_NOT_CONNECTED (2)\n"));
			gpHCI->Unlock ();
			return ERROR_DEVICE_NOT_CONNECTED;
		}

		if (Tracker.fCompleted) {
			if (Tracker.status == 0)
				break;
		} else
			RetireCall (NULL, pInternalOwner, (void *)&Tracker, ERROR_TIMEOUT);

		ResetEvent (hEvent);
		gpHCI->cCommandPacketsAllowed = 1;
	}

	if (i == 3) {
		DisconnectInternalContextAndCleanup (pInternalOwner, hEvent, TRUE);
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_connect_transport:: ERROR_OUTOFMEMORY (2)\n"));
		gpHCI->Unlock ();
		return ERROR_DEVICE_NOT_CONNECTED;
	}

	if (gpHCI->transport.uiResetDelay)
		Sleep (gpHCI->transport.uiResetDelay);

	//
	//	Send HCI_Read_Buffer_Size
	//
	Tracker.fCompleted = FALSE;

	HCIPacket *pPacket = new HCIPacket (COMMAND_PACKET, pInternalOwner, (void *)&Tracker, 3);
	if (! (pPacket && pPacket->pContents)) {
		if (pPacket)
			delete pPacket;
		DisconnectInternalContextAndCleanup (pInternalOwner, hEvent, TRUE);
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_connect_transport:: ERROR_OUTOFMEMORY (3)\n"));
		gpHCI->Unlock ();
		return ERROR_OUTOFMEMORY;
	}

	FormatPacketNoArgs (pPacket, HCI_Read_Buffer_Size);
	pPacket->uPacket.CommandPacket.eEvent = HCI_Command_Complete_Event;

	SchedulePacket (pPacket);

	IFDBG(DebugOut (DEBUG_HCI_INIT, L"hci_connect_transport:: waiting for ReadBufferSize response\n"));
	gpHCI->AddRef ();
	gpHCI->Unlock ();
	WaitForSingleObject (hEvent, HCI_TIMER_SCALE);
	IFDBG(DebugOut (DEBUG_HCI_INIT, L"hci_connect_transport:: got ReadBufferSize response (before lock)\n"));
	gpHCI->Lock ();
	gpHCI->Release ();

	IFDBG(DebugOut (DEBUG_HCI_INIT, L"hci_connect_transport:: got ReadBufferSize response (in)\n"));

	if ((gpHCI->eStage != AlmostRunning) || (! Tracker.fCompleted) || (Tracker.status != 0)) {
		if (gpHCI->eStage == AlmostRunning)
			RetireCall (NULL, pInternalOwner, (void *)&Tracker, ERROR_TIMEOUT);

		DisconnectInternalContextAndCleanup (pInternalOwner, hEvent, TRUE);
		IFDBG(DebugOut (DEBUG_ERROR, L"hci_connect_transport:: ERROR_DEVICE_NOT_CONNECTED (2)\n"));
		gpHCI->Unlock ();
		return ERROR_DEVICE_NOT_CONNECTED;
	}

	if (gpHCI->transport.uiFlags & HCI_FLAGS_AUTOCONFIGURE) {
		//
		//	Send HCI_Read_Local_Supported_Features
		//
		Tracker.fCompleted = FALSE;

		HCIPacket *pPacket = new HCIPacket (COMMAND_PACKET, pInternalOwner, (void *)&Tracker, 3);
		if (! (pPacket && pPacket->pContents)) {
			if (pPacket)
				delete pPacket;
			DisconnectInternalContextAndCleanup (pInternalOwner, hEvent, TRUE);
			IFDBG(DebugOut (DEBUG_ERROR, L"hci_connect_transport:: ERROR_OUTOFMEMORY (3)\n"));
			gpHCI->Unlock ();
			return ERROR_OUTOFMEMORY;
		}

		FormatPacketNoArgs (pPacket, HCI_Read_Local_Supported_Features);
		pPacket->uPacket.CommandPacket.eEvent = HCI_Command_Complete_Event;

		SchedulePacket (pPacket);

		IFDBG(DebugOut (DEBUG_HCI_INIT, L"hci_connect_transport:: waiting for ReadLocalSupportedFeatures response\n"));
		gpHCI->AddRef ();
		gpHCI->Unlock ();
		WaitForSingleObject (hEvent, HCI_TIMER_SCALE);
		IFDBG(DebugOut (DEBUG_HCI_INIT, L"hci_connect_transport:: got ReadLocalSupportedFeatures response (before lock)\n"));
		gpHCI->Lock ();
		gpHCI->Release ();

		IFDBG(DebugOut (DEBUG_HCI_INIT, L"hci_connect_transport:: got ReadLocalSupportedFeatures response (in)\n"));

		if ((gpHCI->eStage != AlmostRunning) || (! Tracker.fCompleted) || (Tracker.status != 0)) {
			if (gpHCI->eStage == AlmostRunning)
				RetireCall (NULL, pInternalOwner, (void *)&Tracker, ERROR_TIMEOUT);

			DisconnectInternalContextAndCleanup (pInternalOwner, hEvent, TRUE);
			IFDBG(DebugOut (DEBUG_ERROR, L"hci_connect_transport:: ERROR_DEVICE_NOT_CONNECTED (2)\n"));
			gpHCI->Unlock ();
			return ERROR_DEVICE_NOT_CONNECTED;
		}
	}

	DisconnectInternalContextAndCleanup (pInternalOwner, hEvent, FALSE);
	gpHCI->eStage = Connected;

	DispatchStackEvent (BTH_STACK_UP, NULL);

	IFDBG(DebugOut (DEBUG_HCI_INIT, L"hci_connect_transport:: ERROR_SUCCESS\n"));
	gpHCI->Unlock ();

	return ERROR_SUCCESS;
}

static int hci_disconnect_transport (int fFinalShutdown) {
	IFDBG(DebugOut (DEBUG_HCI_INIT, L"HCI Close Driver Instance:: entered\n"));

	for ( ; ; ) {
		if (! gpHCI) {
			IFDBG(DebugOut (DEBUG_ERROR, L"HCI Close Driver Instance:: ERROR_SERVICE_DOES_NOT_EXIST\n"));
			return ERROR_SERVICE_DOES_NOT_EXIST;
		}

		gpHCI->Lock ();

		if (gpHCI->hInitEvent)
			SetEvent (gpHCI->hInitEvent);

		if ((gpHCI->eStage == JustCreated) || (gpHCI->eStage == ShuttingDown)) {
			IFDBG(DebugOut (DEBUG_ERROR, L"HCI Close Driver Instance:: ERROR_SERVICE_NOT_ACTIVE\n"));
			gpHCI->Unlock ();
			return ERROR_SERVICE_NOT_ACTIVE;
		}

		if (! gpHCI->InCall ())
			break;

		gpHCI->Unlock ();
		IFDBG(DebugOut (DEBUG_HCI_INIT, L"HCI Close Driver Instance:: sleeping...\n"));
		Sleep (100);
	}

	gpHCI->eStage = ShuttingDown;

	if (gpHCI->hQueue)
		SetEvent (gpHCI->hQueue);

	IFDBG(DebugOut (DEBUG_HCI_INIT, L"HCI Close Driver Instance:: closing connection.\n"));
	HCI_CloseConnection ();

	IFDBG(DebugOut (DEBUG_HCI_INIT, L"HCI Close Driver Instance:: shutting down reader thread.\n"));
	if (gpHCI->hReaderThread) {
		gpHCI->AddRef ();
		gpHCI->Unlock ();
		WaitForSingleObject (gpHCI->hReaderThread, HCI_TIMER_SCALE);
		gpHCI->Lock ();
		while (gpHCI->InCall() > 1) {
			gpHCI->Unlock ();
			Sleep (HCI_TIMER_SCALE / 20);
			gpHCI->Lock ();
		}
		gpHCI->Release ();
		TerminateThread (gpHCI->hReaderThread, 0);
		CloseHandle (gpHCI->hReaderThread);
		gpHCI->hReaderThread = NULL;
	}

	IFDBG(DebugOut (DEBUG_HCI_INIT, L"HCI Close Driver Instance:: shutting down writer thread.\n"));
	if (gpHCI->hWriterThread) {
		gpHCI->AddRef ();
		gpHCI->Unlock ();
		WaitForSingleObject (gpHCI->hWriterThread, HCI_TIMER_SCALE);
		gpHCI->Lock ();
		while (gpHCI->InCall() > 1) {
			gpHCI->Unlock ();
			Sleep (HCI_TIMER_SCALE / 20);
			gpHCI->Lock ();
		}
		gpHCI->Release ();
		TerminateThread (gpHCI->hWriterThread, 0);
		CloseHandle (gpHCI->hWriterThread);
		gpHCI->hWriterThread = NULL;
	}

	SVSUTIL_ASSERT (! gpHCI->InCall ());

	IFDBG(DebugOut (DEBUG_HCI_INIT, L"HCI Close Driver Instance:: retiring calls.\n"));

	gpHCI->AddRef ();
	while (gpHCI->pPacketsPending)
		RetireCall (NULL, gpHCI->pPacketsPending->pOwner, gpHCI->pPacketsPending->pCallContext, ERROR_SHUTDOWN_IN_PROGRESS);

	while (gpHCI->pPacketsSent)
		RetireCall (NULL, gpHCI->pPacketsSent->pOwner, gpHCI->pPacketsSent->pCallContext, ERROR_SHUTDOWN_IN_PROGRESS);

	while (gpHCI->pPackets)
		RetireCall (NULL, gpHCI->pPackets->pOwner, gpHCI->pPackets->pCallContext, ERROR_SHUTDOWN_IN_PROGRESS);

	gpHCI->Release ();

	SVSUTIL_ASSERT (! gpHCI->InCall ());

	IFDBG(DebugOut (DEBUG_HCI_INIT, L"HCI Close Driver Instance:: deleting connections.\n"));

	while (gpHCI->pConnections) {
		BasebandConnection *pNext = gpHCI->pConnections->pNext;
		delete gpHCI->pConnections;
		gpHCI->pConnections = pNext;
	}

	IFDBG(DebugOut (DEBUG_HCI_INIT, L"HCI Close Driver Instance:: closing outstanding inquiries.\n"));

	while (gpHCI->pInquiryResults) {
		InquiryResultList *pNext = gpHCI->pInquiryResults->pNext;
		delete gpHCI->pInquiryResults;
		gpHCI->pInquiryResults = pNext;
	}

	IFDBG(DebugOut (DEBUG_HCI_INIT, L"HCI Close Driver Instance:: dispatching shutdown event.\n"));

	gpHCI->AddRef ();
	DispatchStackEvent (fFinalShutdown ? BTH_STACK_DISCONNECT : BTH_STACK_DOWN, NULL);
	gpHCI->Release ();

	SVSUTIL_ASSERT (! gpHCI->InCall ());

	if (fFinalShutdown) {
		if (gpHCI->hQueue) {
			CloseHandle (gpHCI->hQueue);
			gpHCI->hQueue = NULL;
		}

		while (gpHCI->pContexts) {
			HCI_CONTEXT *pNext = gpHCI->pContexts->pNext;
			delete gpHCI->pContexts;
			gpHCI->pContexts = pNext;
		}

		if (gpHCI->pfmdInquiryResults) {
			svsutil_ReleaseFixedNonEmpty (gpHCI->pfmdInquiryResults);
			gpHCI->pfmdInquiryResults = NULL;
		}

		if (gpHCI->pfmdConnReqData) {
			svsutil_ReleaseFixedNonEmpty (gpHCI->pfmdConnReqData);
			gpHCI->pfmdConnReqData = NULL;
		}

		if (gpHCI->pfmdConnections) {
			svsutil_ReleaseFixedNonEmpty (gpHCI->pfmdConnections);
			gpHCI->pfmdConnections = NULL;
		}

		if (gpHCI->pfmdPackets) {
			svsutil_ReleaseFixedEmpty (gpHCI->pfmdPackets);
			gpHCI->pfmdPackets = NULL;
		}

		gpHCI->ReInit ();	// Reset to JustCreated
	} else {
		gpHCI->Reset ();
		gpHCI->eStage = Disconnected;
	}

	SVSUTIL_ASSERT (! gpHCI->pPackets);
	SVSUTIL_ASSERT (! gpHCI->pPacketsPending);
	SVSUTIL_ASSERT (! gpHCI->pPacketsSent);
	SVSUTIL_ASSERT (! gpHCI->pPeriodicInquiryOwner);
	SVSUTIL_ASSERT (! gpHCI->pConnections);
	SVSUTIL_ASSERT (! gpHCI->pInquiryResults);

	IFDBG(DebugOut (DEBUG_HCI_INIT, L"HCI Close Driver Instance:: all done.\n"));

	gpHCI->Unlock ();

	return ERROR_SUCCESS;
}

static int hci_transport_event (HCI_EVENT iEvent, void *lpEvent) {
	IFDBG(DebugOut (DEBUG_HCI_INIT, L"HCI TRANSPORT EVENT %s\n", iEvent == DEVICE_UP ? L"UP" : (iEvent == DEVICE_DOWN ? L"DOWN" : L"UNKNOWN")));

	if (iEvent == DEVICE_UP) {
		ConnectTransport ();
	} else if (iEvent == DEVICE_DOWN) {
		ShutdownTransport ();
	}

	return ERROR_SUCCESS;
}

DWORD WINAPI ConnectTransport2 (LPVOID lpVoid) {
	hci_connect_transport ();
	return 0;
}

static void ConnectTransport (void) {
	IFDBG(DebugOut (DEBUG_HCI_INIT, L"HCI CONNECT TRANSPORT\n"));
	btutil_ScheduleEvent (ConnectTransport2, NULL);
}

DWORD WINAPI ShutdownTransport2 (LPVOID lpVoid) {
	hci_disconnect_transport (FALSE);
	return 0;
}

static void ShutdownTransport (void) {
	IFDBG(DebugOut (DEBUG_HCI_INIT, L"HCI SHUTDOWN TRANSPORT\n"));
	btutil_ScheduleEvent (ShutdownTransport2, NULL);
}

//
//	Init/uninit, context establishing routines
//
int hci_InitializeOnce (void) {
	IFDBG(DebugOut (DEBUG_HCI_INIT, L"HCI init:: entered\n"));

	SVSUTIL_ASSERT (! gpHCI);

	if (gpHCI) {
		IFDBG(DebugOut (DEBUG_ERROR, L"HCI init:: ERROR_ALREADY_EXISTS\n"));
		return ERROR_ALREADY_EXISTS;
	}

	gpHCI = new HCI;

	if (gpHCI) {
		IFDBG(DebugOut (DEBUG_HCI_INIT, L"HCI init:: ERROR_SUCCESS\n"));
		return ERROR_SUCCESS;
	}

	IFDBG(DebugOut (DEBUG_ERROR, L"HCI init:: ERROR_OUTOFMEMORY\n"));
	return ERROR_OUTOFMEMORY;
}

int hci_UninitializeOnce (void) {
	IFDBG(DebugOut (DEBUG_HCI_INIT, L"HCI uninit:: entered\n"));
	SVSUTIL_ASSERT (gpHCI);

	if (! gpHCI) {
		IFDBG(DebugOut (DEBUG_ERROR, L"HCI uninit:: ERROR_SERVICE_DOES_NOT_EXIST\n"));
		return ERROR_SERVICE_DOES_NOT_EXIST;
	}

	gpHCI->Lock ();

	if (gpHCI->eStage != JustCreated) {
		IFDBG(DebugOut (DEBUG_ERROR, L"HCI uninit:: ERROR_DEVICE_IN_USE\n"));
		return ERROR_DEVICE_IN_USE;
	}

	HCI *pHCI = gpHCI;
	gpHCI = NULL;
	pHCI->Unlock ();

	delete pHCI;

	IFDBG(DebugOut (DEBUG_HCI_INIT, L"HCI uninit:: ERROR_SUCCESS\n"));
	return ERROR_SUCCESS;
}

int hci_CreateDriverInstance (void) {
	IFDBG(DebugOut (DEBUG_HCI_INIT, L"HCI Create Driver Instance:: entered\n"));
	if (! gpHCI) {
		IFDBG(DebugOut (DEBUG_ERROR, L"HCI Create Driver Instance:: ERROR_SERVICE_DOES_NOT_EXIST\n"));
		return ERROR_SERVICE_DOES_NOT_EXIST;
	}

	gpHCI->Lock ();
	if (gpHCI->eStage != JustCreated) {
		IFDBG(DebugOut (DEBUG_ERROR, L"HCI Create Driver Instance:: ERROR_SERVICE_ALREADY_RUNNING\n"));
		gpHCI->Unlock ();
		return ERROR_SERVICE_ALREADY_RUNNING;
	}

	HCI_SetCallback (hci_transport_event);

	gpHCI->eStage = Initializing;
	gpHCI->pfmdConnections = svsutil_AllocFixedMemDescr (sizeof(BasebandConnection), HCI_MEM_SCALE);
	gpHCI->pfmdPackets = svsutil_AllocFixedMemDescr (sizeof(HCIPacket), HCI_MEM_SCALE);
	gpHCI->pfmdInquiryResults = svsutil_AllocFixedMemDescr (sizeof(InquiryResultList), HCI_MEM_SCALE);
	gpHCI->pfmdConnReqData = svsutil_AllocFixedMemDescr (sizeof(ConnReqData), HCI_MEM_SCALE);
	gpHCI->hQueue = CreateEvent (NULL, FALSE, FALSE, NULL);

	if (! (gpHCI->pfmdConnections && gpHCI->pfmdPackets && gpHCI->pfmdInquiryResults && gpHCI->pfmdConnReqData && gpHCI->hQueue)) {
		if (gpHCI->pfmdConnections) {
			svsutil_ReleaseFixedNonEmpty (gpHCI->pfmdConnections);
			gpHCI->pfmdConnections = NULL;
		}

		if (gpHCI->pfmdPackets) {
			svsutil_ReleaseFixedEmpty (gpHCI->pfmdPackets);
			gpHCI->pfmdPackets = NULL;
		}

		if (gpHCI->pfmdInquiryResults) {
			svsutil_ReleaseFixedEmpty (gpHCI->pfmdInquiryResults);
			gpHCI->pfmdInquiryResults = NULL;
		}

		if (gpHCI->pfmdConnReqData) {
			svsutil_ReleaseFixedEmpty (gpHCI->pfmdConnReqData);
			gpHCI->pfmdConnReqData = NULL;
		}

		if (gpHCI->hQueue) {
			CloseHandle (gpHCI->hQueue);
			gpHCI->hQueue = NULL;
		}

		gpHCI->ReInit ();
		IFDBG(DebugOut (DEBUG_ERROR, L"HCI Create Driver Instance:: ERROR_OUTOFMEMORY (1)\n"));
		gpHCI->Unlock ();
		return ERROR_OUTOFMEMORY;
	}

	gpHCI->eStage = Disconnected;
	IFDBG(DebugOut (DEBUG_HCI_INIT, L"HCI Create Driver Instance:: ERROR_SUCCESS\n"));
	gpHCI->Unlock ();
	
	ConnectTransport ();

	return ERROR_SUCCESS;
}


int hci_CloseDriverInstance (void) {
	HCI_SetCallback (NULL);
	return hci_disconnect_transport (TRUE);
}

static int check_EstablishDeviceContext_parms
(
unsigned int			uiControl,			/* IN */
BD_ADDR					*pba,				/* IN */
HCI_EVENT_INDICATION	*pInd,				/* IN */
HCI_CALLBACKS			*pCall,				/* IN */
HCI_INTERFACE			*pInt,				/* OUT */
int						*pcDataHeaders,		/* OUT */
int						*pcDataTrailers,	/* OUT */
HANDLE					*phDeviceContext	/* OUT */
) {
	BD_ADDR					b;
	HCI_EVENT_INDICATION	ei;
	HCI_CALLBACKS			c;

	__try {
		memset (pInt, 0, sizeof(*pInt));
		if (uiControl & BTH_CONTROL_ROUTE_BY_ADDR)
			memcpy (&b, pba, sizeof(b));
		memcpy (&ei, pInd, sizeof(ei));
		memcpy (&c, pCall, sizeof(c));
		*phDeviceContext = NULL;
		*pcDataTrailers = 0;
		*pcDataHeaders = 0;
	} __except (1) {
		return FALSE;
	}

	return TRUE;
}

int HCI_EstablishDeviceContext
(
void					*pUserContext,		/* IN */
unsigned int			uiControl,			/* IN */
BD_ADDR					*pba,				/* IN */
unsigned int			class_of_device,	/* IN */
unsigned char			link_type,			/* IN */
HCI_EVENT_INDICATION	*pInd,				/* IN */
HCI_CALLBACKS			*pCall,				/* IN */
HCI_INTERFACE			*pInt,				/* OUT */
int						*pcDataHeaders,		/* OUT */
int						*pcDataTrailers,	/* OUT */
HANDLE					*phDeviceContext	/* OUT */
) {
	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"HCI_EstablishDeviceContext user 0x%08x control 0x%08x bd_add %04x%08x cod 0x%08x\n", pUserContext, uiControl,
		pba ? pba->NAP : 0, pba ? pba->SAP : 0, class_of_device));

	if (uiControl & (~(BTH_CONTROL_ROUTE_ALL | BTH_CONTROL_ROUTE_BY_ADDR | BTH_CONTROL_ROUTE_BY_COD | BTH_CONTROL_ROUTE_BY_LINKTYPE | BTH_CONTROL_ROUTE_SECURITY | BTH_CONTROL_ROUTE_HARDWARE | BTH_CONTROL_DEVICEONLY))) {
		IFDBG(DebugOut (DEBUG_WARN, L"HCI_EstablishDeviceContext returns ERROR_INVALID_PARAMETER\n"));
		return ERROR_INVALID_PARAMETER;
	}


	if (! check_EstablishDeviceContext_parms (uiControl, pba, pInd, pCall, pInt, pcDataHeaders, pcDataTrailers, phDeviceContext)) {
		IFDBG(DebugOut (DEBUG_WARN, L"HCI_EstablishDeviceContext returns ERROR_INVALID_PARAMETER (exception)\n"));
		return ERROR_INVALID_PARAMETER;
	}

	if (! gpHCI) {
		IFDBG(DebugOut (DEBUG_ERROR, L"HCI_EstablishDeviceContext returns ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpHCI->Lock ();

	if (! gpHCI->IsStackRunning ()) {
		IFDBG(DebugOut (DEBUG_ERROR, L"HCI_EstablishDeviceContext returns ERROR_SERVICE_NOT_ACTIVE\n"));
		gpHCI->Unlock ();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	HCI_CONTEXT *pContext = gpHCI->pContexts;
	while (pContext) {
		if (uiControl & pContext->uiControl) {
			if ((uiControl & pContext->uiControl & BTH_CONTROL_ROUTE_ALL) ||
				(uiControl & pContext->uiControl & BTH_CONTROL_ROUTE_SECURITY) ||
				((uiControl & pContext->uiControl & BTH_CONTROL_ROUTE_BY_ADDR) && (*pba == pContext->ba)) ||
				((uiControl & pContext->uiControl & BTH_CONTROL_ROUTE_BY_COD) && (class_of_device == pContext->class_of_device)) ||
				((uiControl & pContext->uiControl & BTH_CONTROL_ROUTE_BY_LINKTYPE) && (link_type == pContext->link_type))) {
				IFDBG(DebugOut (DEBUG_ERROR, L"HCI_EstablishDeviceContext returns ERROR_SHARING_VIOLATION\n"));
				gpHCI->Unlock ();
				return ERROR_SHARING_VIOLATION;
			}
		}
		pContext = pContext->pNext;
	}

	pContext = new HCI_CONTEXT;

	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"HCI_EstablishDeviceContext returns ERROR_OUTOFMEMORY\n"));
		gpHCI->Unlock ();
		return ERROR_OUTOFMEMORY;
	}

	pContext->pUserContext = pUserContext;

	pContext->uiControl = uiControl;
	if (uiControl & BTH_CONTROL_ROUTE_BY_ADDR)
		pContext->ba = *pba;

	pContext->class_of_device = class_of_device;
	pContext->link_type = link_type;

	pContext->c = *pCall;
	pContext->ei = *pInd;

	pContext->pNext = gpHCI->pContexts;

	gpHCI->pContexts = pContext;

	*phDeviceContext = pContext;

	pInt->hci_AbortCall = bt_abort_call;
	pInt->hci_ioctl = bt_io_control;

	pInt->hci_DataPacketDown_In = bt_data_packet_down_in;
	pInt->hci_CustomCode_In = bt_custom_code_in;

	pInt->hci_CreateConnection_In = bt_create_connection_in;
	pInt->hci_AcceptConnectionRequest_In = bt_accept_connection_request_in;
	pInt->hci_RejectConnectionRequest_In = bt_reject_connection_request_in;
	pInt->hci_Disconnect_In = bt_disconnect_in;
	pInt->hci_AddSCOConnection_In = bt_add_sco_connection_in;
	pInt->hci_RemoteNameRequest_In = bt_remote_name_request_in;
	pInt->hci_PINCodeRequestNegativeReply_In = bt_pin_code_request_negative_reply_in;
	pInt->hci_PINCodeRequestReply_In = bt_pin_code_request_reply_in;
	pInt->hci_LinkKeyRequestNegativeReply_In = bt_link_key_request_negative_reply_in;
	pInt->hci_LinkKeyRequestReply_In = bt_link_key_request_reply_in;
	pInt->hci_AuthenticationRequested_In = bt_authentication_requested_in;
	pInt->hci_SetConnectionEncryption_In = bt_set_connection_encryption_in;

	pInt->hci_RoleDiscovery_In = bt_role_discovery_in;
	pInt->hci_SwitchRole_In = bt_switch_role_in;

	pInt->hci_Inquiry_In = bt_inquiry_in;
	pInt->hci_InquiryCancel_In = bt_inquiry_cancel_in;
	pInt->hci_PeriodicInquiryMode_In = bt_periodic_inquiry_mode_in;
	pInt->hci_ExitPeriodicInquiryMode_In = bt_exit_periodic_inquiry_mode_in;

	pInt->hci_ReadBDADDR_In = bt_read_bd_addr_in;
	pInt->hci_ReadBufferSize_In = bt_read_buffer_size_in;
	pInt->hci_Reset_In = bt_reset_in;

	pInt->hci_ChangeLocalName_In = bt_change_local_name_in;
	pInt->hci_ReadLocalName_In   = bt_read_local_name_in;

	pInt->hci_ReadClassOfDevice_In = bt_read_class_of_device_in;
	pInt->hci_WriteClassOfDevice_In = bt_write_class_of_device_in;

	pInt->hci_ReadVoiceSetting_In = bt_read_voice_setting_in;
	pInt->hci_WriteVoiceSetting_In = bt_write_voice_setting_in;

	pInt->hci_ReadSCOFlowControlEnable_In = bt_read_sco_flow_control_enable_in;
	pInt->hci_WriteSCOFlowControlEnable_In = bt_write_sco_flow_control_enable_in;

	pInt->hci_SetHostControllerToHostFlowControl_In = bt_set_host_controller_to_host_flow_control_in;
	pInt->hci_HostBufferSize_In = bt_host_buffer_size_in;
	pInt->hci_HostNumberOfCompletedPackets = bt_host_number_of_completed_packets;
	pInt->hci_ReadScanEnable_In = bt_read_scan_enable_in;
	pInt->hci_WriteScanEnable_In = bt_write_scan_enable_in;

	pInt->hci_WritePageTimeout_In = bt_write_page_timeout_in;
	pInt->hci_ReadPageTimeout_In = bt_read_page_timeout_in;

	pInt->hci_ReadNumberOfSupportedIAC_In = bt_read_number_of_supported_IAC_in;

	pInt->hci_ChangeConnectionPacketType_In	= bt_change_connection_packet_type_in;

	pInt->hci_HoldMode_In = bt_hold_mode_in;
	pInt->hci_SniffMode_In = bt_sniff_mode_in;
	pInt->hci_ExitSniffMode_In = bt_exit_sniff_mode_in;
	pInt->hci_ParkMode_In = bt_park_mode_in;
	pInt->hci_ExitParkMode_In = bt_exit_park_mode_in;

	pInt->hci_QoSSetup_In = bt_QOS_setup_in;

	pInt->hci_ReadLinkPolicySettings_In = bt_read_link_policy_settings_in;
	pInt->hci_WriteLinkPolicySettings_In = bt_write_link_policy_settings_in;

	pInt->hci_ReadPINType_In = bt_read_pin_type_in;
	pInt->hci_WritePINType_In = bt_write_pin_type_in;
	
	pInt->hci_ReadConnectionAcceptTimeout_In = bt_read_connection_accept_timeout_in;
	pInt->hci_WriteConnectionAcceptTimeout_In = bt_write_connection_accept_timeout_in;
	
	pInt->hci_ReadPageScanActivity_In = bt_read_page_scan_activity_in;
	pInt->hci_WritePageScanActivity_In = bt_write_page_scan_activity_in;
	pInt->hci_ReadInquiryScanActivity_In = bt_read_inquiry_scan_activity_in;
	pInt->hci_WriteInquiryScanActivity_In = bt_write_inquiry_scan_activity_in;

	pInt->hci_ReadAuthenticationEnable_In = bt_read_authentication_enable_in;
	pInt->hci_WriteAuthenticationEnable_In = bt_write_authentication_enable_in;
	pInt->hci_ReadEncryptionMode_In = bt_read_encryption_mode_in;
	pInt->hci_WriteEncryptionMode_In = bt_write_encryption_mode_in;
	pInt->hci_ReadHoldModeActivity_In = bt_read_hold_mode_activity_in;
	pInt->hci_WriteHoldModeActivity_In = bt_write_hold_mode_activity_in;
	
	pInt->hci_ReadLinkSupervisionTimeout_In = bt_read_link_supervision_timeout_in;
	pInt->hci_WriteLinkSupervisionTimeout_In = bt_write_link_supervision_timeout_in;
	pInt->hci_ReadCurrentIACLAP_In = bt_read_current_IAC_LAP_in;
	pInt->hci_WriteCurrentIACLAP_In = bt_write_current_IAC_LAP_in;
	pInt->hci_ReadPageScanPeriodMode_In = bt_read_page_scan_period_mode_in;
	pInt->hci_WritePageScanPeriodMode_In = bt_write_page_scan_period_mode_in;
	pInt->hci_ReadPageScanMode_In = bt_read_page_scan_mode_in;
	pInt->hci_WritePageScanMode_In = bt_write_page_scan_mode_in;

	pInt->hci_ChangeConnectionLinkKey_In = bt_change_connection_link_key_in;
	pInt->hci_MasterLinkKey_In = bt_master_link_key_in;

	pInt->hci_ReadRemoteSupportedFeatures_In = bt_read_remote_supported_features_in;
	pInt->hci_ReadRemoteVersionInformation_In = bt_read_remote_version_information_in;
	pInt->hci_ReadClockOffset_In = bt_read_clock_offset_in;

	pInt->hci_Flush_In = bt_flush_in;
	
	pInt->hci_CreateNewUnitKey_In = bt_create_new_unit_key_in;

	pInt->hci_ReadAutomaticFlushTimeout_In = bt_read_automatic_flush_timeout_in;
	pInt->hci_WriteAutomaticFlushTimeout_In = bt_write_automatic_flush_timeout_in;

	pInt->hci_ReadLocalVersionInformation_In = bt_read_local_version_information_in;
	pInt->hci_ReadLocalSupportedFeatures_In = bt_read_local_supported_features_in;
	pInt->hci_ReadCountryCode_In = bt_read_country_code_in;

	pInt->hci_GetLinkQuality_In = bt_get_link_quality_in;
	pInt->hci_ReadRSSI_In = bt_read_RSSI_in;

	pInt->hci_ReadTransmitPowerLevel_In = bt_read_transmit_power_level_in;

	pInt->hci_ReadFailedContactCounter_In = bt_read_failed_contact_counter_in;
	pInt->hci_ResetFailedContactCounter_In = bt_reset_failed_contact_counter_in;

	pInt->hci_ReadNumBroadcastRetransmissions_In = bt_read_num_broadcast_retransmissions_in;
	pInt->hci_WriteNumBroadcastRetransmissions_In = bt_write_num_broadcast_retransmissions_in;

	pInt->hci_ReadLoopbackMode_In = bt_read_loopback_mode_in;
	pInt->hci_WriteLoopbackMode_In = bt_write_loopback_mode_in;
	pInt->hci_EnableDeviceUnderTestMode_In = bt_enable_device_under_test_mode_in;
	
	pInt->hci_SetEventMask_In = bt_set_event_mask_in;
	pInt->hci_SetEventFilter_In = bt_set_event_filter_in;
	
	pInt->hci_ReadStoredLinkKey_In = bt_read_stored_link_key_in;
	pInt->hci_WriteStoredLinkKey_In = bt_write_stored_link_key_in;
	pInt->hci_DeleteStoredLinkKey_In = bt_delete_stored_link_key_in;

	*pcDataTrailers = 0;
	*pcDataHeaders = 0;

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"HCI_EstablishDeviceContext returns ERROR_SUCCESS\n"));
	gpHCI->Unlock ();

	return ERROR_SUCCESS;
}

int HCI_CloseDeviceContext
(
HANDLE					hDeviceContext		/* IN */
) {
	if (! gpHCI) {
		IFDBG(DebugOut (DEBUG_ERROR, L"HCI_CloseDeviceContext returns ERROR_SERVICE_NOT_ACTIVE\n"));
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	gpHCI->Lock ();

	if (! gpHCI->IsStackRunning ()) {
		IFDBG(DebugOut (DEBUG_ERROR, L"HCI_CloseDeviceContext returns ERROR_SERVICE_NOT_ACTIVE\n"));
		gpHCI->Unlock ();
		return ERROR_SERVICE_NOT_ACTIVE;
	}

	while (gpHCI->InCall () && gpHCI->IsStackRunning ()) {
		gpHCI->Unlock ();
		Sleep (100);
		if (! gpHCI) {
			IFDBG(DebugOut (DEBUG_ERROR, L"HCI_CloseDeviceContext returns ERROR_SERVICE_NOT_ACTIVE\n"));
			return ERROR_SERVICE_NOT_ACTIVE;
		}
		gpHCI->Lock ();
	}

	HCI_CONTEXT *pContext = gpHCI->pContexts;
	HCI_CONTEXT *pParent = NULL;

	while (pContext && pContext != hDeviceContext) {
		pParent = pContext;
		pContext = pContext->pNext;
	}

	if (! pContext) {
		IFDBG(DebugOut (DEBUG_ERROR, L"HCI_CloseDeviceContext returns ERROR_NOT_FOUND\n"));
		gpHCI->Unlock ();
		return ERROR_NOT_FOUND;
	}

	if (! pParent)
		gpHCI->pContexts = pContext->pNext;
	else
		pParent->pNext = pContext->pNext;

	if (gpHCI->pPeriodicInquiryOwner == pContext)
		gpHCI->pPeriodicInquiryOwner = NULL;

	delete pContext;
	gpHCI->Unlock ();

	IFDBG(DebugOut (DEBUG_HCI_TRACE, L"HCI_CloseDeviceContext returns ERROR_SUCCESS\n"));
	return ERROR_SUCCESS;
}



//
//	Allocation
//
void *BasebandConnection::operator new (size_t iSize) {
	SVSUTIL_ASSERT (gpHCI->eStage == Connected);
	SVSUTIL_ASSERT (gpHCI->pfmdConnections);
	SVSUTIL_ASSERT (gpHCI->IsLocked ());
	SVSUTIL_ASSERT (iSize == sizeof(BasebandConnection));

	void *pRes = svsutil_GetFixed (gpHCI->pfmdConnections);

	SVSUTIL_ASSERT (pRes);

	return pRes;
}

void BasebandConnection::operator delete(void *ptr) {
	SVSUTIL_ASSERT (gpHCI->IsLocked ());
	SVSUTIL_ASSERT (ptr);

	svsutil_FreeFixed (ptr, gpHCI->pfmdConnections);
}

void *HCIPacket::operator new (size_t iSize) {
	SVSUTIL_ASSERT ((gpHCI->eStage == Connected) || (gpHCI->eStage == AlmostRunning));
	SVSUTIL_ASSERT (gpHCI->pfmdPackets);
	SVSUTIL_ASSERT (gpHCI->IsLocked ());
	SVSUTIL_ASSERT (iSize == sizeof(HCIPacket));

	void *pRes = svsutil_GetFixed (gpHCI->pfmdPackets);

	SVSUTIL_ASSERT (pRes);

	return pRes;
}

void HCIPacket::operator delete(void *ptr) {
	SVSUTIL_ASSERT (gpHCI->IsLocked ());
	SVSUTIL_ASSERT (ptr);

	svsutil_FreeFixed (ptr, gpHCI->pfmdPackets);
}

void *InquiryResultList::operator new (size_t iSize) {
	SVSUTIL_ASSERT (gpHCI->eStage == Connected);
	SVSUTIL_ASSERT (gpHCI->pfmdInquiryResults);
	SVSUTIL_ASSERT (gpHCI->IsLocked ());
	SVSUTIL_ASSERT (iSize == sizeof(InquiryResultList));

	void *pRes = svsutil_GetFixed (gpHCI->pfmdInquiryResults);

	SVSUTIL_ASSERT (pRes);

	return pRes;
}

void InquiryResultList::operator delete(void *ptr) {
	SVSUTIL_ASSERT (gpHCI->IsLocked ());
	SVSUTIL_ASSERT (ptr);

	svsutil_FreeFixed (ptr, gpHCI->pfmdInquiryResults);
}

void *ConnReqData::operator new (size_t iSize) {
	SVSUTIL_ASSERT (gpHCI->eStage == Connected);
	SVSUTIL_ASSERT (gpHCI->pfmdConnReqData);
	SVSUTIL_ASSERT (gpHCI->IsLocked ());
	SVSUTIL_ASSERT (iSize == sizeof(ConnReqData));

	void *pRes = svsutil_GetFixed (gpHCI->pfmdConnReqData);

	SVSUTIL_ASSERT (pRes);

	return pRes;
}

void ConnReqData::operator delete(void *ptr) {
	SVSUTIL_ASSERT (gpHCI->IsLocked ());
	SVSUTIL_ASSERT (ptr);

	svsutil_FreeFixed (ptr, gpHCI->pfmdConnReqData);
}

//
//	Console output
//
#if defined (BTH_CONSOLE)

static void DumpPackets (HCIPacket *pP) {
	while (pP) {
		DebugOut (DEBUG_OUTPUT, L"Packet @ 0x%08x\n", pP);
		DebugOut (DEBUG_OUTPUT, L"    Type         : %s\n",
			pP->ePacketType == COMMAND_PACKET ? L"COMMAND_PACKET" :
			(pP->ePacketType == DATA_PACKET_ACL ? L"ACL DATA" :
			(pP->ePacketType == DATA_PACKET_SCO ? L"SCO DATA" : L"ERROR -- UNKNOWN TYPE")));
		if (pP->ePacketType == COMMAND_PACKET) {
			DebugOut (DEBUG_OUTPUT, L"    Opcode       : 0x%04x\n", pP->uPacket.CommandPacket.opCode);
			DebugOut (DEBUG_OUTPUT, L"    Event        : 0x%02x\n", pP->uPacket.CommandPacket.eEvent);
			DebugOut (DEBUG_OUTPUT, L"    Marker       : ");

			if (pP->uPacket.CommandPacket.m.fMarker == BTH_MARKER_NONE)
				DebugOut (DEBUG_OUTPUT, L"none\n");
			else if (pP->uPacket.CommandPacket.m.fMarker == BTH_MARKER_CONNECTION)
				DebugOut (DEBUG_OUTPUT, L"by connection 0x%04x\n", pP->uPacket.CommandPacket.m.connection_handle);
			else if (pP->uPacket.CommandPacket.m.fMarker == BTH_MARKER_CONNECTION)
				DebugOut (DEBUG_OUTPUT, L"by bd_addr %04x%08x\n", pP->uPacket.CommandPacket.m.ba.NAP, pP->uPacket.CommandPacket.m.ba.SAP);
			else
				DebugOut (DEBUG_OUTPUT, L"ERROR -- UNKNOWN MARKER!\n");
		} else if (pP->ePacketType == DATA_PACKET_ACL) {
			DebugOut (DEBUG_OUTPUT, L"    Handle       : 0x%04x", pP->uPacket.DataPacketU.hConnection);
			DebugOut (DEBUG_OUTPUT, L"    Subs Total   : %d", pP->uPacket.DataPacketU.cTotal);
			DebugOut (DEBUG_OUTPUT, L"    Subs Pending : %d", pP->uPacket.DataPacketU.cSubsPending);
			DebugOut (DEBUG_OUTPUT, L"    Subs Done    : %d", pP->uPacket.DataPacketAcl.cCompleted);
		} else if (pP->ePacketType == DATA_PACKET_SCO) {
			DebugOut (DEBUG_OUTPUT, L"    Handle       : 0x%04x", pP->uPacket.DataPacketU.hConnection);
			DebugOut (DEBUG_OUTPUT, L"    Subs Total   : %d", pP->uPacket.DataPacketU.cTotal);
			DebugOut (DEBUG_OUTPUT, L"    Subs Pending : %d", pP->uPacket.DataPacketU.cSubsPending);
		}

		DebugOut (DEBUG_OUTPUT, L"    Owner        : 0x%08x\n", pP->pOwner);
		DebugOut (DEBUG_OUTPUT, L"    Call Context : 0x%08x\n", pP->pCallContext);
		DebugOut (DEBUG_OUTPUT, L"    Contents     :");
		if (pP->pContents) {
			DebugOut (DEBUG_OUTPUT, L"Start %d End %d Size %d", pP->pContents->cStart, pP->pContents->cEnd, pP->pContents->cSize);
			int size = pP->pContents->cSize < 10 ? pP->pContents->cSize : 10;
			for (int i = 0 ; i < size ; ++i)
				DebugOut (DEBUG_OUTPUT, L"%02x ", pP->pContents->pBuffer[i]);
			if (size < pP->pContents->cSize)
				DebugOut (DEBUG_OUTPUT, L"...\n");
			else
				DebugOut (DEBUG_OUTPUT, L"\n");
		} else
			DebugOut (DEBUG_OUTPUT, L"None\n");

		pP = pP->pNext;
	}
}

int hci_ProcessConsoleCommand (WCHAR *pszCommand) {
	if (! gpHCI) {
		DebugOut (DEBUG_OUTPUT, L"HCI not initialized\n");
		return ERROR_SUCCESS;
	}

	gpHCI->Lock ();

	int iRes = ERROR_SUCCESS;
	__try {
		if (wcsicmp (pszCommand, L"help") == 0) {
			DebugOut (DEBUG_OUTPUT, L"HCI Commands:\n");
			DebugOut (DEBUG_OUTPUT, L"    help        prints this text\n");
			DebugOut (DEBUG_OUTPUT, L"    global      dumps global state\n");
			DebugOut (DEBUG_OUTPUT, L"    links       dumps currently active baseband connections\n");
			DebugOut (DEBUG_OUTPUT, L"    queued      dumps packets that are queued for execution\n");
			DebugOut (DEBUG_OUTPUT, L"    sent        dumps packets in execution\n");
			DebugOut (DEBUG_OUTPUT, L"    pending     dumps packets that are pending in the controller\n");
			DebugOut (DEBUG_OUTPUT, L"    contexts    dumps currently installed HCI clients\n");
			DebugOut (DEBUG_OUTPUT, L"    inquiry     dumps inquiry cache\n");
		} else if (wcsicmp (pszCommand, L"global") == 0) {
			DebugOut (DEBUG_OUTPUT, L"HCI Global State:\n");
			DebugOut (DEBUG_OUTPUT, L"    Current state                          : ");
			if (gpHCI->eStage == JustCreated)
				DebugOut (DEBUG_OUTPUT, L"Just Created\n");
			else if (gpHCI->eStage == Initializing)
				DebugOut (DEBUG_OUTPUT, L"Initializing\n");
			else if (gpHCI->eStage == AlmostRunning)
				DebugOut (DEBUG_OUTPUT, L"Almost Running\n");
			else if (gpHCI->eStage == Connected)
				DebugOut (DEBUG_OUTPUT, L"Connected\n");
			else if (gpHCI->eStage == Disconnected)
				DebugOut (DEBUG_OUTPUT, L"Disconnected\n");
			else if (gpHCI->eStage == ShuttingDown)
				DebugOut (DEBUG_OUTPUT, L"ShuttingDown\n");
			else if (gpHCI->eStage == Error)
				DebugOut (DEBUG_OUTPUT, L"Error\n");
			else
				DebugOut (DEBUG_OUTPUT, L"Error : state unknown!\n");
			DebugOut (DEBUG_OUTPUT, L"    Links fixed memory descriptor          : 0x%08x\n", gpHCI->pfmdConnections);
			DebugOut (DEBUG_OUTPUT, L"    Packets fixed memory descriptor        : 0x%08x\n", gpHCI->pfmdPackets);
		
			int iCount1 = 0;
			HCIPacket *pPacket = gpHCI->pPackets;
			while (pPacket) { iCount1++; pPacket = pPacket->pNext; }
			int iCount2 = 0;
			pPacket = gpHCI->pPacketsSent;
			while (pPacket) { iCount2++; pPacket = pPacket->pNext; }
			int iCount3 = 0;
			pPacket = gpHCI->pPacketsPending;
			while (pPacket) { iCount3++; pPacket = pPacket->pNext; }

			DebugOut (DEBUG_OUTPUT, L"    Queued packets                         : %d\n", iCount1);
			DebugOut (DEBUG_OUTPUT, L"    Sent packets                           : %d\n", iCount2);
			DebugOut (DEBUG_OUTPUT, L"    Pending packets                        : %d\n", iCount3);

			iCount1 = 0;
			BasebandConnection *pConn = gpHCI->pConnections;
			while (pConn) { iCount1++; pConn = pConn->pNext; }

			DebugOut (DEBUG_OUTPUT, L"    Baseband links                         : %d\n", iCount1);

			iCount1 = 0;
			InquiryResultList *pInq = gpHCI->pInquiryResults;
			while (pInq) { iCount1++; pInq = pInq->pNext; }

			DebugOut (DEBUG_OUTPUT, L"    Stored inquiry results                 : %d\n", iCount1);

			iCount1 = 0;
			HCI_CONTEXT *pC = gpHCI->pContexts;
			while (pC) { iCount1++; pC = pC->pNext; }
			DebugOut (DEBUG_OUTPUT, L"    Active contexts                        : %d\n", iCount1);

			DebugOut (DEBUG_OUTPUT, L"    Periodic Inquiry Owner                 : 0x%08x\n", gpHCI->pPeriodicInquiryOwner);
			DebugOut (DEBUG_OUTPUT, L"    Queue                                  : 0x%08x\n", gpHCI->hQueue);
			DebugOut (DEBUG_OUTPUT, L"    Writer Thread                          : 0x%08x\n", gpHCI->hWriterThread);
			DebugOut (DEBUG_OUTPUT, L"    Reader Thread                          : 0x%08x\n", gpHCI->hReaderThread);
			DebugOut (DEBUG_OUTPUT, L"    Call ref count                         : %d\n", gpHCI->iCallRefCount);
			DebugOut (DEBUG_OUTPUT, L"    Command packets allowed                : %d\n", gpHCI->cCommandPacketsAllowed);
			DebugOut (DEBUG_OUTPUT, L"    Host flow                              : %s\n", gpHCI->fHostFlow ? L"yes" : L"no");
			DebugOut (DEBUG_OUTPUT, L"    Loopback                               : %s\n", gpHCI->fLoopback ? L"yes" : L"no");
			DebugOut (DEBUG_OUTPUT, L"    Under Test                             : %s\n", gpHCI->fUnderTest ? L"yes" : L"no");
			DebugOut (DEBUG_OUTPUT, L"    Device's ACL packet length             : %d\n", gpHCI->sDeviceBuffer.ACL_Data_Packet_Length);
			DebugOut (DEBUG_OUTPUT, L"    Device's SCO packet length             : %d\n", gpHCI->sDeviceBuffer.SCO_Data_Packet_Length);
			DebugOut (DEBUG_OUTPUT, L"    Device's total ACL packets             : %d\n", gpHCI->sDeviceBuffer.Total_Num_ACL_Data_Packets);
			DebugOut (DEBUG_OUTPUT, L"    Device's total SCO packets             : %d\n", gpHCI->sDeviceBuffer.Total_Num_SCO_Data_Packets);
			DebugOut (DEBUG_OUTPUT, L"    Host's ACL packet length               : %d\n", gpHCI->sHostBuffer.ACL_Data_Packet_Length);
			DebugOut (DEBUG_OUTPUT, L"    Host's SCO packet length               : %d\n", gpHCI->sHostBuffer.SCO_Data_Packet_Length);
			DebugOut (DEBUG_OUTPUT, L"    Host's total ACL packets               : %d\n", gpHCI->sHostBuffer.Total_Num_ACL_Data_Packets);
			DebugOut (DEBUG_OUTPUT, L"    Host's total SCO packets               : %d\n", gpHCI->sHostBuffer.Total_Num_SCO_Data_Packets);
			DebugOut (DEBUG_OUTPUT, L"    Event mask                             : 0x%08x%08x\n", (int)((gpHCI->llEventMask >> 32) & 0xffffffff), (int)(gpHCI->llEventMask & 0xffffffff));
			DebugOut (DEBUG_OUTPUT, L"    Hardware errors/transport              : %d\n", gpHCI->iHardwareErrors);
			DebugOut (DEBUG_OUTPUT, L"    Hardware errors/card                   : %d\n", gpHCI->iReportedErrors);
		} else if (wcsicmp (pszCommand, L"links") == 0) {
			BasebandConnection *pC = gpHCI->pConnections;
			while (pC) {
				DebugOut (DEBUG_OUTPUT, L"Baseband connection @ 0x%08x\n", pC);
				DebugOut (DEBUG_OUTPUT, L"    Handle          : 0x%04x\n", pC->connection_handle);
				DebugOut (DEBUG_OUTPUT, L"    BD_ADDR         : %04x%08x\n", pC->ba.NAP, pC->ba.SAP);
				DebugOut (DEBUG_OUTPUT, L"    Link            : %d (%s)\n", pC->link_type, pC->link_type == 1 ? L"ACL" : L"SCO");
				DebugOut (DEBUG_OUTPUT, L"    Owner           : 0x%08x\n", pC->pOwner);
				DebugOut (DEBUG_OUTPUT, L"    Packets pending : %d\n", pC->cAclDataPacketsPending);
				DebugOut (DEBUG_OUTPUT, L"    Bytes complete  : %d\n", pC->cAclBytesComplete);
				if (pC->pAclPacket) {
					DebugOut (DEBUG_OUTPUT, L"    ACL Packet      : Start %d End %d Size %d ", pC->pAclPacket->cStart, pC->pAclPacket->cEnd, pC->pAclPacket->cSize);
					int size = pC->pAclPacket->cSize < 10 ? pC->pAclPacket->cSize : 10;
					for (int i = 0 ; i < size ; ++i)
						DebugOut (DEBUG_OUTPUT, L"%02x ", pC->pAclPacket->pBuffer[i]);
					if (size < pC->pAclPacket->cSize)
						DebugOut (DEBUG_OUTPUT, L"...\n");
					else
						DebugOut (DEBUG_OUTPUT, L"\n");
				}
				pC = pC->pNext;
			}
		} else if (wcsicmp (pszCommand, L"inquiry") == 0) {
			DebugOut (DEBUG_OUTPUT, L"Inquiry cache:\n");
			InquiryResultList *pInq = gpHCI->pInquiryResults;
			while (pInq) {
				DebugOut (DEBUG_OUTPUT, L"%04x%08x found=%d psm=0x%02x pspm = 0x%02x psrml = 0x%02x cod=0x%06x clock=0x%04x\n", pInq->irb.ba.NAP, pInq->irb.ba.SAP, pInq->irb.dwTick, pInq->irb.page_scan_mode, pInq->irb.page_scan_period_mode, pInq->irb.page_scan_repetition_mode, pInq->irb.class_of_device, pInq->irb.clock_offset);
				pInq = pInq->pNext;
			}
		} else if (wcsicmp (pszCommand, L"queued") == 0) {
			DebugOut (DEBUG_OUTPUT, L"Queued packets:\n");
			DumpPackets (gpHCI->pPackets);
		} else if (wcsicmp (pszCommand, L"sent") == 0) {
			DebugOut (DEBUG_OUTPUT, L"Sent packets:\n");
			DumpPackets (gpHCI->pPacketsSent);
		} else if (wcsicmp (pszCommand, L"pending") == 0) {
			DebugOut (DEBUG_OUTPUT, L"Pending packets:\n");
			DumpPackets (gpHCI->pPacketsPending);
		} else if (wcsicmp (pszCommand, L"contexts") == 0) {
			HCI_CONTEXT	*pC = gpHCI->pContexts;
			while (pC) {
				DebugOut (DEBUG_OUTPUT, L"Stack context @ 0x%08x\n", pC);
				DebugOut (DEBUG_OUTPUT, L"    Control   :");
				if (pC->uiControl & BTH_CONTROL_ROUTE_ALL)
					DebugOut (DEBUG_OUTPUT, L" All");
				if (pC->uiControl & BTH_CONTROL_ROUTE_BY_ADDR)
					DebugOut (DEBUG_OUTPUT, L" addr %04x%08x", pC->ba.NAP, pC->ba.SAP);
				if (pC->uiControl & BTH_CONTROL_ROUTE_BY_COD)
					DebugOut (DEBUG_OUTPUT, L" cod 0x%06x\n", pC->class_of_device);
				if (pC->uiControl & BTH_CONTROL_ROUTE_BY_LINKTYPE)
					DebugOut (DEBUG_OUTPUT, L" link %d\n", pC->link_type);
				if (pC->uiControl & BTH_CONTROL_ROUTE_SECURITY)
					DebugOut (DEBUG_OUTPUT, L"  security");
				if (pC->uiControl & BTH_CONTROL_ROUTE_HARDWARE)
					DebugOut (DEBUG_OUTPUT, L"  hardware");
				if (pC->uiControl & BTH_CONTROL_DEVICEONLY)
					DebugOut (DEBUG_OUTPUT, L" device only");
				DebugOut (DEBUG_OUTPUT, L"\n");
				DebugOut (DEBUG_OUTPUT, L"    User      : 0x%08x\n", pC->pUserContext);
				pC = pC->pNext;
			}
		} else
			iRes = ERROR_UNKNOWN_FEATURE;
	} __except (1) {
		DebugOut (DEBUG_ERROR, L"Exception in HCI dump!\n");
	}

	gpHCI->Unlock ();

	return iRes;
}

#endif // BTH_CONSOLE

