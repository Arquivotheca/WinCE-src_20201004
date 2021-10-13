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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*****************************************************************************
* 
*
*   @doc
*   @module macp.h | PPP MAC Layer Header File
*
*   Date: 2/25/99
*
*   @comm
*/

#ifndef _MACP_H_
#define _MACP_H_
#include "mac.h"
#include "ndiswan.h"
#include "ndistapi.h"

struct _NDIS_REQUEST_BETTER;

// Completion function type for NDIS_REQUEST_BETTER
typedef VOID (*PCOMPLETION_FUNC)(struct _NDIS_REQUEST_BETTER *pRequest, PVOID FuncArg, NDIS_STATUS Status);

//
// Structure for performing NDIS requests. Can block to wait for result or
// specify a custom completion routine.
//
typedef struct _NDIS_REQUEST_BETTER
{
    NDIS_REQUEST            Request;
    NDIS_STATUS             Status;
    pppSession_t            *pSession;              // Session for the request
    NDIS_EVENT              Event;                  // Event signaled when request completes
    PCOMPLETION_FUNC        pCompletionFunc;        // Completion function
    PVOID                   FuncArg;                // Argument to completion function
    CTEEvent                CTEEvent;               // CXPORT event data
    PNDISWAN_ADAPTER        pAdapter;
} NDIS_REQUEST_BETTER, *PNDIS_REQUEST_BETTER;

typedef struct  mac_context
{
    void        *session;           // session context pntr
    PNDISWAN_ADAPTER    pAdapter;   // The adapter it's on
    DWORD       dwDeviceID;         // The deviceID
    
    // Don't really need these but why not?
    TCHAR       szDeviceName[RAS_MaxDeviceName+1];
    TCHAR       szDeviceType[RAS_MaxDeviceType+1];
    HDRV_LINE   hLine;              // The line handle
    HDRV_CALL   hCall;              // The call handle
    HANDLE      hNdisTapiEvent;     // Call Indication event
    DWORD       TapiEcode;          // Result code of dial operation
    NDIS_WAN_INFO   WanInfo;        // The current WAN Info.
    PUCHAR      PacketMemory;
    DWORD       PacketMemorySize;
    DWORD       BufferSize;
    LIST_ENTRY  PacketQueue;
    NDIS_SPIN_LOCK  PacketLock;
    BOOL        bIsPPPoE;           // TRUE if szDeviceType is RASDT_PPPoE

    // List of pending completes
    CompleteHanderInfo_t    *pPendingLineCloseCompleteList;
    CompleteHanderInfo_t    *pPendingCallDropCompleteList;
    CompleteHanderInfo_t    *pPendingCallCloseCompleteList;

    // Link specific info.  For multi-link this needs to be broken out into a separate
    // structure.  NT uses a BundleCB to contain the common info for a group of links and
    // a LinkCB for the link specific info.
    NDIS_HANDLE NdisLinkHandle;
    DWORD       LinkSpeed;
    NDIS_WAN_QUALITY    Quality;
    DWORD       SendWindow;
    DWORD       OutstandingFrames;
    DWORD       dwSendFramingBits;
    BOOL        bLineOpenInProgress;
    HANDLE      hEventLineOpenComplete;
    BOOL        bCallCloseRequested;
    BOOL        bCallClosed;
    BOOL        bLineCloseRequested;
    BOOL        bLineClosed;
    DWORD       dwLineCallState;

    //
    // Custom scripting dll support
    //
    HANDLE      hComPort;           // The COM port handle
    HANDLE      hRxEvent;           // Event to signal on RX data available
    HANDLE      hRxEventThread;     // Handle to thread watching for RX events
    PBYTE       pRxBuffer;          // Pointer to buffer to store rx data in
    DWORD       dwRxBufferSize;     // Size of Rx buffer
#define MAX_CUSTOM_SCRIPT_RX_BUFFER_SIZE    1500
    DWORD       dwRxBytesReceived;  // Bytes received into rx buffer

    //
    // If the underlying miniport supports OID_WAN_GET_STATS_INFO,
    // then bMacStatsObtained will be TRUE and the MacStats
    // structure will contain the results from the most recent
    // query of that OID. Just prior to OID_TAPI_CLOSE being issued
    // the final STATS will be retrieved and stored here.
    //
    BOOL                    bMacStatsObtained;
    NDIS_WAN_GET_STATS_INFO MacStats;

} macCntxt_t, *PMAC_CONTEXT;

typedef BOOL (WINAPI *PFN_ADPTWALK) (PNDISWAN_ADAPTER pAdapter,
                                     PVOID pContext1, PVOID pContext2);


// ----------------------------------------------------------------
//                         Global Variables
// ----------------------------------------------------------------
extern NDIS_HANDLE      v_PROTHandle;       // The NDIS Protocol Handle
extern PNDISWAN_ADAPTER v_AdapterList;      // List of known adapters
extern CRITICAL_SECTION v_AdapterCS;        // CS to protect adapter list

extern HANDLE           v_hRequestEvent;    // Request complete
extern CRITICAL_SECTION v_RequestCS;        // CS to protect request
extern NDIS_STATUS      v_RequestStatus;    // Last Request status



// ----------------------------------------------------------------
//                      Function Declarations
// ----------------------------------------------------------------

void
pppMacIssueWanGetStatsInfo(
    macCntxt_t *pMac);

// AdptLst.c
DWORD   AddAdapter (IN      PNDIS_STRING    AdapterName);
BOOL    AdapterAddRef (PNDISWAN_ADAPTER pAdapter);
BOOL    AdapterDelRef (PNDISWAN_ADAPTER pAdapter);


// NdisProt.c
VOID
PppNdisIssueRequest(
                OUT NDIS_STATUS             *pStatus,
                IN  PNDISWAN_ADAPTER        pAdapter,
                IN  DWORD                   Type,   // NdisRequestSetInformation or NdisRequestQueryInformation
                IN  DWORD                   Oid,
                IN  PVOID                   InformationBuffer,
                IN  DWORD                   InformationBufferLength,
    OPTIONAL    IN  macCntxt_t              *pMac,  // Reference session while request in progress if non-NULL
    OPTIONAL    IN  void                    (*pCompletionFunc)(PNDIS_REQUEST_BETTER, PVOID FuncArg, NDIS_STATUS),
    OPTIONAL    IN  PVOID                   FuncArg,
    OPTIONAL    OUT PNDIS_REQUEST_BETTER    *ppRequest
    );
VOID
PppNdisDoSyncRequest(
                OUT NDIS_STATUS             *pStatus,
                IN  PNDISWAN_ADAPTER        pAdapter,
                IN  DWORD                   Type,   // NdisRequestSetInformation or NdisRequestQueryInformation
                IN  DWORD                   Oid,
                IN  PVOID                   InformationBuffer,
                IN  DWORD                   InformationBufferLength
                );

DWORD   DoNDISRegisterProtocol();

// ndistapi.c

NDIS_STATUS NdisTapiSetDevConfig(macCntxt_t *pMac);
NDIS_STATUS NdisTapiLineOpen(macCntxt_t *pMac);
NDIS_STATUS NdisTapiLineMakeCall(macCntxt_t *pMac, LPCTSTR szDialStr);
NDIS_STATUS NdisTapiGetCallInfo(macCntxt_t *pMac, PNDIS_TAPI_GET_CALL_INFO pGetCallInfo);

NDIS_STATUS NdisTapiHangup(
    macCntxt_t *pMac,
    void        (*pCallCloseCompleteCallback)(PVOID),
    PVOID       pCallbackData);

NDIS_STATUS
NdisTapiLineClose(
    macCntxt_t *pMac,
    void        (*pLineCloseCompleteCallback)(PVOID),
    PVOID       pCallbackData);

NDIS_STATUS
NdisTapiGetDeviceId(
    IN  macCntxt_t  *pMac,
    IN  ULONG        ulSelect,
    IN  PTSTR        tszDeviceClass,
    OUT PVAR_STRING *pDeviceId);

NDIS_STATUS
NdisTapiGetDeviceIdAsync(
    IN  macCntxt_t  *pMac,
    IN  ULONG        ulSelect,
    IN  PTSTR        tszDeviceClass);

NDIS_STATUS NdisTapiSetDefaultMediaDetection(
    IN  macCntxt_t *pMac,
    IN  DWORD       ulMediaModes);

// memory.c
NDIS_STATUS NdisWanAllocateSendResources (PMAC_CONTEXT  pMac);
NDIS_STATUS NdisWanAllocatePacket (PMAC_CONTEXT pMac, PNDIS_WAN_PACKET  *ppPacket);


#endif // _MACP_H_
