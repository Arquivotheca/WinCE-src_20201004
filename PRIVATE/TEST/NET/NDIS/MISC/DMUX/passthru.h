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
/*++


Module Name:
   passthru.h

Abstract:
   The Ndis DMUX Intermediate Miniport driver sample. This is based on passthru driver 
   which doesn't touch packets at all.

Author:
   The original code was a NT/XP sample passthru driver.
   KarelD - add CE specific code and reformat text.

Environment:
   Windows CE .Net

Revision History:
   None.
 
--*/

//------------------------------------------------------------------------------

#ifdef NDIS51_MINIPORT
#define DMUXMINI_MAJOR_NDIS_VERSION       5
#define DMUXMINI_MINOR_NDIS_VERSION       1
#else
#define DMUXMINI_MAJOR_NDIS_VERSION       4
#define DMUXMINI_MINOR_NDIS_VERSION       0
#endif

#ifdef NDIS51
#define DMUXMINI_PROT_MAJOR_NDIS_VERSION  5
#define DMUXMINI_PROT_MINOR_NDIS_VERSION  0
#else
#define DMUXMINI_PROT_MAJOR_NDIS_VERSION  4
#define DMUXMINI_PROT_MINOR_NDIS_VERSION  0
#endif

//------------------------------------------------------------------------------

#define DMUXMINI_MEMORY_TAG            'ImDm'

//------------------------------------------------------------------------------

#ifndef UNDER_CE
// To support IOCTLs from user-mode
#define PASSTHRU_LINK_NAME             L"\\DosDevices\\Passthru"
#define PASSTHRU_DEVICE_NAME           L"\\Device\\Passthru"
#else
#define DMUXMINI_REGISTRY_PATH         L"Comm\\DMUX"
#define DMUXMINI_PROTOCOL_NAME         L"DMUXMINI"
#define DMUXMINI_PROTOCOL_NAME_SIZE    8
#define DMUXMINI_MINIPORT_PREFIX       L"DMUX\\"
#define DMUXMINI_MINIPORT_PREFIX_SIZE  5
#endif

//------------------------------------------------------------------------------

// Advance declaration
typedef struct _BINDING BINDING, *PBINDING;

//------------------------------------------------------------------------------

extern
NTSTATUS
DriverEntry(
   IN PDRIVER_OBJECT pDriverObject,
   IN PUNICODE_STRING psRegistryPath
);

extern
VOID
DriverUnload(
   IN PDRIVER_OBJECT pDriverObject
);

//------------------------------------------------------------------------------

#ifndef UNDER_CE

NTSTATUS
Dispatch(
   IN PDEVICE_OBJECT pDeviceObject,
   IN PIRP pIrp
);

NDIS_STATUS
RegisterDevice(
   VOID
);

NDIS_STATUS
DeregisterDevice(
   VOID
);

#endif

//------------------------------------------------------------------------------
//
// Protocol prototypes
//
extern
VOID
ProtocolOpenAdapterComplete(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN NDIS_STATUS status,
   IN NDIS_STATUS openStatus
);

extern
VOID
ProtocolCloseAdapterComplete(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN NDIS_STATUS status
);

extern
VOID
ProtocolResetComplete(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN NDIS_STATUS status
);

extern
VOID
ProtocolRequestComplete(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN PNDIS_REQUEST hNdisRequest,
   IN NDIS_STATUS status
);

extern
VOID
ProtocolStatus(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN NDIS_STATUS status,
   IN PVOID pvStatusBuffer,
   IN UINT uiStatusBufferSize
);

extern
VOID
ProtocolStatusComplete(
   IN NDIS_HANDLE hProtocolBindingContext
);

extern
VOID
ProtocolSendComplete(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN PNDIS_PACKET pPacket,
   IN NDIS_STATUS status
);

extern
VOID
ProtocolTransferDataComplete(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN PNDIS_PACKET pPacket,
   IN NDIS_STATUS status,
   IN UINT uiBytesTransferred
);

extern
NDIS_STATUS
ProtocolReceive(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN NDIS_HANDLE hMacReceiveContext,
   IN PVOID pvHeaderBuffer,
   IN UINT uiHeaderBufferSize,
   IN PVOID pvLookAheadBuffer,
   IN UINT uiLookaheadBufferSize,
   IN UINT uiPacketSize
);

extern
VOID
ProtocolReceiveComplete(
   IN NDIS_HANDLE hProtocolBindingContext
);

extern
INT
ProtocolReceivePacket(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN PNDIS_PACKET pPacket
);

extern
VOID
ProtocolBindAdapter(
   OUT PNDIS_STATUS pStatus,
   IN NDIS_HANDLE hBindContext,
   IN PNDIS_STRING sAdapterName,
   IN PVOID pvSystemSpecific1,
   IN PVOID pvSystemSpecific2
);

extern
VOID
ProtocolUnbindAdapter(
   OUT PNDIS_STATUS pStatus,
   IN NDIS_HANDLE hProtocolBindingContext,
   IN NDIS_HANDLE hUnbindContext
);
   
extern 
NDIS_STATUS
ProtocolPNPHandler(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN PNET_PNP_EVENT pNetPnPEvent
);

extern
VOID
ProtocolUnload(
   VOID
);

//------------------------------------------------------------------------------
//
// Miniport prototypes
//

NDIS_STATUS
MiniportInitialize(
   OUT PNDIS_STATUS pOpenStatus,
   OUT PUINT puiSelectedMediumIndex,
   IN PNDIS_MEDIUM aMediumArray,
   IN UINT uiMediumArraySize,
   IN NDIS_HANDLE hMiniportAdapterHandle,
   IN NDIS_HANDLE hWrapperConfigurationContext
);

VOID
MiniportSendPackets(
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN PPNDIS_PACKET apPacketArray,
   IN UINT uiNumberOfPackets
);

NDIS_STATUS
MiniportSend(
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN PNDIS_PACKET pPacket,
   IN UINT uiFlags
);

NDIS_STATUS
MiniportQueryInformation(
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN NDIS_OID Oid,
   IN PVOID pvInformationBuffer,
   IN ULONG ulInformationBufferLength,
   OUT PULONG pulBytesWritten,
   OUT PULONG pulBytesNeeded
);

NDIS_STATUS
MiniportSetInformation(
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN NDIS_OID Oid,
   IN PVOID pvInformationBuffer,
   IN ULONG ulInformationBufferLength,
   OUT PULONG pulBytesRead,
   OUT PULONG pulBytesNeeded
);

VOID
MiniportReturnPacket(
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN PNDIS_PACKET pPacket
);

NDIS_STATUS
MiniportTransferData(
   OUT PNDIS_PACKET pPacket,
   OUT PUINT puiBytesTransferred,
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN NDIS_HANDLE hMiniportReceiveContext,
   IN UINT uiByteOffset,
   IN UINT uiBytesToTransfer
);

VOID
MiniportHalt(
   IN NDIS_HANDLE hMiniportAdapterContext
);

NDIS_STATUS
MiniportReset(
   OUT PBOOLEAN pbAddressingReset,
   IN NDIS_HANDLE hMiniportAdapterContext
);

#ifdef NDIS51_MINIPORT

VOID
MiniportCancelSendPackets(
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN PVOID pvCancelId
);

VOID
MiniportAdapterShutdown(
   IN NDIS_HANDLE hMiniportAdapterContext
);

VOID
MiniportDevicePnPEvent(
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN NDIS_DEVICE_PNP_EVENT devicePnPEvent,
   IN PVOID pvInformationBuffer,
   IN ULONG ulInformationBufferLength
);

#endif // NDIS51_MINIPORT


//------------------------------------------------------------------------------
//
// There should be no DbgPrint's in the Free version of the driver
//
#if DBG
#define DBGPRINT(Fmt)            \
   {                             \
      DbgPrint Fmt;              \
   }
#else // if DBG
#define DBGPRINT(Fmt)                                 
#endif // if DBG 

//------------------------------------------------------------------------------
//
// Protocol reserved part of a sent packet that is allocated by us.
//
typedef struct _PASSTHRU_PR_SEND
{
   PNDIS_PACKET pOriginalPacket;
} PASSTHRU_PR_SEND, *PPASSTHRU_PR_SEND;

C_ASSERT(sizeof(PASSTHRU_PR_SEND) <= sizeof(((PNDIS_PACKET)0)->MiniportReserved));

//
// Miniport reserved part of a received packet that is allocated by
// us. Note that this should fit into the MiniportReserved space
// in an NDIS_PACKET.
//
typedef struct _PASSTHRU_PR_RECV
{
   PNDIS_PACKET pOriginalPacket;
} PASSTHRU_PR_RECV, *PPASSTHRU_PR_RECV;

C_ASSERT(sizeof(PASSTHRU_PR_RECV) <= sizeof(((PNDIS_PACKET)0)->MiniportReserved));


//------------------------------------------------------------------------------

#define MAX_DMUX_INTERFACES (16)

typedef struct _sDMUX
{
   NDIS_REQUEST   request;                // Pending request store
   PBINDING pBinding;					  // a handle back to BINDING.
   ULONG	ulFilter;
   NDIS_HANDLE    hMPBinding;             // Handler for miniport side binding
   NDIS_STATUS    statusLastIndicated;    // The last indicated media status
   NDIS_STATUS    statusLatestUnIndicate; // The latest suppressed media status
   NDIS_DEVICE_POWER_STATE MPDeviceState; // Miniport's Device State 
   PULONG         pulBytesNeeded;         // Ptr to result value for pending request
   PULONG         pulBytesUsed;           // Dtto

   // In DMUX we need to indicate some thing to all our dmux interfaces. Here we can't rely on the fact that
   // we have virtual miniport handle & our MiniportInitialze has returned SUCCESS. We'll wait till Protocol
   // driver has set filter on virtual miniport driver then only we'll indicate the packets.
   DWORD		  dwState;				  // =0 --> uninitialized, =1 --> (initialzed, Queried & readyto send/receive)
   BOOLEAN        bIndicateRecvComplete;  // We should call ReceiveComplete
   BOOLEAN        bOutstandingRequests;   // Request is pending at the miniport below
   BOOLEAN        bQueuedRequest;         // Request is queued at this IM miniport
   BOOLEAN        bStandingBy;            // True - When the Virtual Miniport
                                          // is transitioning from a ON (D0)
                                          // to Standby (>D0) State, cleared after
                                          // a transition to D0

   NDIS_STRING    sDeviceName;            // Device name

   BYTE pbMacAddr[6];					  // Mac addr of DMUX interface.
   WCHAR pwzcMacName[61];				  // WCHAR Name of DMUX interface.
} sDMUX, *PsDMUX;

typedef struct _sDMUXSet
{
	PsDMUX ArrayDMUX[MAX_DMUX_INTERFACES];
	DWORD dwDMUXnos;
} sDMUXSet;

//------------------------------------------------------------------------------
//
// Structure used by both the miniport as well as the protocol part of 
// the intermediate driver to represent an adapter and its lower bindings
//
typedef struct _BINDING
{
   BINDING*       pNext;

   LONG           nRef;                   // Number of references to structure
   
   NDIS_HANDLE    hPTBinding;             // Handler for protocol side binding
   NDIS_HANDLE    hMPBinding;             // Handler for miniport side binding
   NDIS_HANDLE    hSendPacketPool;        // Packet pool for send packets   
   NDIS_HANDLE    hRecvPacketPool;        // Packet pool for received packets
   NDIS_STATUS    status;                 // Pending operation result status
   NDIS_EVENT     hEvent;                 // Event used for waiting
   NDIS_MEDIUM    medium;                 // Medium for protocol side
   NDIS_REQUEST   request;                // Pending request store
   PULONG         pulBytesNeeded;         // Ptr to result value for pending request
   PULONG         pulBytesUsed;           // Dtto
   BOOLEAN        bOutstandingRequests;   // Request is pending at the miniport below
   BOOLEAN        bQueuedRequest;         // Not used. Request is queued at this IM miniport

   BOOLEAN        bIndicateRecvComplete;  // We should call ReceiveComplete
   BOOLEAN        bStandingBy;            // True - When the protocol
                                          // is transitioning from a ON (D0)
                                          // to Standby (>D0) State, cleared after
                                          // a transition to D0

   NDIS_DEVICE_POWER_STATE MPDeviceState; // Miniport's Device State 
   NDIS_DEVICE_POWER_STATE PTDeviceState; // Protocol's Device State 
   
   NDIS_STRING    sDeviceName;            // Device name

   BOOLEAN        bMiniportInitPending;   // TRUE iff IMInit in progress
   NDIS_STATUS    statusLastIndicated;    // The last indicated media status
   NDIS_STATUS    statusLatestUnIndicate; // The latest suppressed media status

   NDIS_MINIPORT_TIMER timer;
   sDMUXSet 	  sPASSDMUXSet;             // Reserved for DMUX Set.
   DWORD		  dwDMUXIfIndex;
   //NDIS_TIMER timer;

} BINDING, *PBINDING;

//------------------------------------------------------------------------------

extern NDIS_HANDLE      g_hNdisProtocol;
extern NDIS_HANDLE      g_hNdisMiniport;
extern NDIS_MEDIUM      g_aNdisMedium[3];
extern BINDING*         g_pBindingList;
extern UINT             g_uiBindings;
extern NDIS_SPIN_LOCK   g_spinLock;

//------------------------------------------------------------------------------
