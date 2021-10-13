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

//  Include Files

#include "windows.h"
#include "ndis.h"
#include "ras.h"
#include "raserror.h"
#include "cxport.h"
#include "protocol.h"
#include "ppp.h"
#include "lcp.h"
#include "macp.h"
#include "ndistapi.h"
#include "cclib.h"


typedef DWORD IPAddr;
typedef DWORD IPMask;
#include "ip_intf.h"

// ----------------------------------------------------------------
//
//  Global Data
//
// ----------------------------------------------------------------
#define PPP_NDIS_PROTOCOL_NAME  TEXT("PPP")
static WCHAR NDISPROTName[] = PPP_NDIS_PROTOCOL_NAME;


#ifndef NDIS_API
#define NDIS_API
#endif

// Some forward declarations
void NDIS_API PROTOAComplete(NDIS_HANDLE Handle, NDIS_STATUS Status,
                             NDIS_STATUS ErrorStatus);
void NDIS_API PROTCAComplete(NDIS_HANDLE Handle, NDIS_STATUS Status);
void NDIS_API PROTSendComplete(NDIS_HANDLE Handle, PNDIS_WAN_PACKET Packet,
                               NDIS_STATUS Status);
void NDIS_API PROTTDComplete(NDIS_HANDLE Handle, PNDIS_PACKET Packet,
                             NDIS_STATUS Status, uint BytesCopied);
void NDIS_API PROTResetComplete(NDIS_HANDLE Handle, NDIS_STATUS Status);
void NDIS_API PROTRequestComplete(NDIS_HANDLE Handle, PNDIS_REQUEST Request,
                                  NDIS_STATUS Status);
NDIS_STATUS NDIS_API PROTWanRcv(NDIS_HANDLE Handle, PUCHAR Packet, ULONG PacketSize);
void NDIS_API PROTWanRcvComplete(NDIS_HANDLE Handle);
void NDIS_API PROTStatus(NDIS_HANDLE Handle, NDIS_STATUS GStatus, void *Status,
                         uint StatusSize);
void NDIS_API PROTStatusComplete(NDIS_HANDLE Handle);
void NDIS_API PROTBindAdapter(PNDIS_STATUS RetStatus, NDIS_HANDLE BindContext,
                             PNDIS_STRING AdapterName, PVOID SS1, PVOID SS2);

void NDIS_API PROTUnbindAdapter(PNDIS_STATUS RetStatus,
                               NDIS_HANDLE ProtBindContext,
                               NDIS_HANDLE UnbindContext);


NDIS_PROTOCOL_CHARACTERISTICS PROTCharacteristics = {
    4,  // MAJOR Version
    0,  // MINOR Version
    0,
    PROTOAComplete,
    PROTCAComplete,
    (SEND_COMPLETE_HANDLER)PROTSendComplete,
    PROTTDComplete,
    PROTResetComplete,
    PROTRequestComplete,
    (RECEIVE_HANDLER)PROTWanRcv,
    PROTWanRcvComplete,
    PROTStatus,
    PROTStatusComplete,
    {   
        sizeof(PPP_NDIS_PROTOCOL_NAME) - sizeof(WCHAR), // Length in bytes not including terminating NULL
        sizeof(PPP_NDIS_PROTOCOL_NAME) - sizeof(WCHAR), // MaximumLength in bytes
        PPP_NDIS_PROTOCOL_NAME                          // PWSTR Buffer
    },
    NULL,   // Receive Packet Handler
    PROTBindAdapter,    // BindAdapterHandler
    PROTUnbindAdapter,  // UnbindAdapterHandler
    NULL,   // TranslateHandler
    NULL    // UnloadHandler
};

#if DEBUG
PCHAR
GetOidString(
    NDIS_OID Oid
    )
{
    PCHAR OidName;

    #define OID_CASE(oid) case (oid): OidName = #oid; break
    switch (Oid)
    {
        OID_CASE(OID_GEN_CURRENT_LOOKAHEAD);
        OID_CASE(OID_GEN_DRIVER_VERSION);
        OID_CASE(OID_GEN_HARDWARE_STATUS);
        OID_CASE(OID_GEN_LINK_SPEED);
        OID_CASE(OID_GEN_MAC_OPTIONS);
        OID_CASE(OID_GEN_MAXIMUM_LOOKAHEAD);
        OID_CASE(OID_GEN_MAXIMUM_FRAME_SIZE);
        OID_CASE(OID_GEN_MAXIMUM_TOTAL_SIZE);
        OID_CASE(OID_GEN_MEDIA_SUPPORTED);
        OID_CASE(OID_GEN_MEDIA_IN_USE);
        OID_CASE(OID_GEN_RECEIVE_BLOCK_SIZE);
        OID_CASE(OID_GEN_RECEIVE_BUFFER_SPACE);
        OID_CASE(OID_GEN_SUPPORTED_LIST);
        OID_CASE(OID_GEN_TRANSMIT_BLOCK_SIZE);
        OID_CASE(OID_GEN_TRANSMIT_BUFFER_SPACE);
        OID_CASE(OID_GEN_VENDOR_DESCRIPTION);
        OID_CASE(OID_GEN_VENDOR_ID);
        OID_CASE(OID_802_3_CURRENT_ADDRESS);
        OID_CASE(OID_TAPI_ACCEPT);
        OID_CASE(OID_TAPI_ANSWER);
        OID_CASE(OID_TAPI_CLOSE);
        OID_CASE(OID_TAPI_CLOSE_CALL);
        OID_CASE(OID_TAPI_CONDITIONAL_MEDIA_DETECTION);
        OID_CASE(OID_TAPI_CONFIG_DIALOG);
        OID_CASE(OID_TAPI_DEV_SPECIFIC);
        OID_CASE(OID_TAPI_DIAL);
        OID_CASE(OID_TAPI_DROP);
        OID_CASE(OID_TAPI_GET_ADDRESS_CAPS);
        OID_CASE(OID_TAPI_GET_ADDRESS_ID);
        OID_CASE(OID_TAPI_GET_ADDRESS_STATUS);
        OID_CASE(OID_TAPI_GET_CALL_ADDRESS_ID);
        OID_CASE(OID_TAPI_GET_CALL_INFO);
        OID_CASE(OID_TAPI_GET_CALL_STATUS);
        OID_CASE(OID_TAPI_GET_DEV_CAPS);
        OID_CASE(OID_TAPI_GET_DEV_CONFIG);
        OID_CASE(OID_TAPI_GET_EXTENSION_ID);
        OID_CASE(OID_TAPI_GET_ID);
        OID_CASE(OID_TAPI_GET_LINE_DEV_STATUS);
        OID_CASE(OID_TAPI_MAKE_CALL);
        OID_CASE(OID_TAPI_NEGOTIATE_EXT_VERSION);
        OID_CASE(OID_TAPI_OPEN);
        OID_CASE(OID_TAPI_PROVIDER_INITIALIZE);
        OID_CASE(OID_TAPI_PROVIDER_SHUTDOWN);
        OID_CASE(OID_TAPI_SECURE_CALL);
        OID_CASE(OID_TAPI_SELECT_EXT_VERSION);
        OID_CASE(OID_TAPI_SEND_USER_USER_INFO);
        OID_CASE(OID_TAPI_SET_APP_SPECIFIC);
        OID_CASE(OID_TAPI_SET_CALL_PARAMS);
        OID_CASE(OID_TAPI_SET_DEFAULT_MEDIA_DETECTION);
        OID_CASE(OID_TAPI_SET_DEV_CONFIG);
        OID_CASE(OID_TAPI_SET_MEDIA_MODE);
        OID_CASE(OID_TAPI_SET_STATUS_MESSAGES);
        OID_CASE(OID_TAPI_TRANSLATE_ADDRESS);
        OID_CASE(OID_WAN_CURRENT_ADDRESS);
        OID_CASE(OID_WAN_GET_BRIDGE_INFO);
        OID_CASE(OID_WAN_GET_COMP_INFO);
        OID_CASE(OID_WAN_GET_INFO);
        OID_CASE(OID_WAN_GET_LINK_INFO);
        OID_CASE(OID_WAN_GET_STATS_INFO);
        OID_CASE(OID_WAN_HEADER_FORMAT);
        OID_CASE(OID_WAN_LINE_COUNT);
        OID_CASE(OID_WAN_MEDIUM_SUBTYPE);
        OID_CASE(OID_WAN_PERMANENT_ADDRESS);
        OID_CASE(OID_WAN_PROTOCOL_TYPE);
        OID_CASE(OID_WAN_QUALITY_OF_SERVICE);
        OID_CASE(OID_WAN_SET_BRIDGE_INFO);
        OID_CASE(OID_WAN_SET_COMP_INFO);
        OID_CASE(OID_WAN_SET_LINK_INFO);

        default:
            OidName = "Unknown OID";
            break;
    }
    return OidName;
}
#endif

DWORD
DoNDISRegisterProtocol()
{
    NDIS_STATUS Status;         // Status for NDIS calls.
    
    DEBUGMSG (ZONE_TRACE, (TEXT("+DoNDISRegisterProtocol\r\n")));
    
    NdisRegisterProtocol(&Status, &v_PROTHandle,
                         (NDIS_PROTOCOL_CHARACTERISTICS *)
                         &PROTCharacteristics, sizeof(PROTCharacteristics));

    if (Status != NDIS_STATUS_SUCCESS)
    {
        DEBUGMSG (ZONE_TRACE,
                  (TEXT(" pppMac_Initialize: Error %d from NdisRegisterProtocol\r\n"),
                   Status));
        return ERROR_EVENT_INVALID;
    }

    return SUCCESS;
}

//** PROTOAComplete - PROT Open adapter complete handler.
//
//  This routine is called by the NDIS driver when an open adapter
//  call completes. Presumably somebody is blocked waiting for this, so
//  we'll wake him up now.
//
//  Entry:
//      Handle - The binding handle we specified (really a pointer to an AI).
//      Status - Final status of command.
//      ErrorStatus - Final error status.
//
//  Exit: Nothing.
//
void NDIS_API
PROTOAComplete(NDIS_HANDLE Handle, NDIS_STATUS Status, NDIS_STATUS ErrorStatus)
{
//    PROTInterface    *ai = (PROTInterface *)Handle;   // For compiler.
    
    DEBUGMSG (ZONE_FUNCTION, (TEXT("+PROTOAComplete(0x%X, 0x%X, 0x%X)\r\n"),
                               Handle, Status, ErrorStatus));   

//    CTESignal(&ai->ai_block, (uint)Status);         // Wake him up, and return status.
    DEBUGMSG (ZONE_FUNCTION, (TEXT("-PROTOAComplete:\r\n"), Handle, Status, ErrorStatus));  

}
//** PROTCAComplete - PROT close adapter complete handler.
//
//  This routine is called by the NDIS driver when a close adapter
//  request completes.
//
//  Entry:
//      Handle - The binding handle we specified (really a pointer to an AI).
//      Status - Final status of command.
//
//  Exit: Nothing.
//
void NDIS_API
PROTCAComplete(
    IN  NDIS_HANDLE Handle,
    IN  NDIS_STATUS Status)
{
    DWORD i;

    PNDISWAN_ADAPTER    pAdapter = (PNDISWAN_ADAPTER)Handle;

    DEBUGMSG (ZONE_FUNCTION, (TEXT("PPP: +PROTCAComplete(0x%X, 0x%X)\r\n"),Handle, Status));

    DEBUGMSG(ZONE_MAC || (ZONE_ERROR && Status), (TEXT("PPP: NdisCloseAdapter %s Complete Status=%x\n"), pAdapter->szAdapterName, Status));

    if (pAdapter->hUnbindContext)
    {
        //
        //  The CloseAdapter was initiated by a call to our UnbindAdapter Handler, from
        //  which we returned NDIS_STATUS_PENDING.  We now invoke the completion handler.
        //
        DEBUGMSG(ZONE_MAC, (TEXT("PPP: Completing Unbind for adapter %s Complete Status=%x\n"), pAdapter->szAdapterName, Status));
        NdisCompleteUnbindAdapter(pAdapter->hUnbindContext, Status);
    }

    for (i = 0; i < pAdapter->dwNumDevices; i++)
    {
        LocalFree(pAdapter->pDeviceInfo[i].pwszDeviceName);
    }
    pppFreeMemory (pAdapter->pDeviceInfo,   pAdapter->dwNumDevices * sizeof(*pAdapter->pDeviceInfo));
    pppFreeMemory (pAdapter->szAdapterName, (wcslen(pAdapter->szAdapterName)+1)*sizeof(WCHAR));
    pppFreeMemory (pAdapter, sizeof (NDISWAN_ADAPTER));

    DEBUGMSG (ZONE_FUNCTION, (TEXT("-PROTCAComplete:\r\n")));   
}

void
AbortPendingPackets(
    IN  OUT PPPP_SESSION pSession)
//
//  Abort packets that are in the PendingSendPacketList
//
{
    PNDIS_PACKET          pPacket;
    PLIST_ENTRY           pNode;
    PPPP_SEND_PACKET_INFO pSendPacketInfo;

    while (TRUE)
    {
        if (IsListEmpty(&pSession->PendingSendPacketList))
            break;

        pNode = RemoveHeadList(&pSession->PendingSendPacketList);
        pPacket = (PNDIS_PACKET)CONTAINING_RECORD(pNode, NDIS_PACKET, MiniportReserved);
        pSendPacketInfo = (PPPP_SEND_PACKET_INFO)&pPacket->MiniportReserved[0];

        pppUnLock(pSession);
        VEMSendComplete(pSession->pVEMContext, pPacket, NDIS_STATUS_CLOSED);
        pppLock(pSession);
    }
}

void
SendPendingPackets(
    IN  OUT PPPP_SESSION pSession)
//
//  Send packets that are in the PendingSendPacketList, so long
//  as WAN_PACKETs are available to send them.
//
{
    PNDIS_PACKET          pPacket;
    PLIST_ENTRY           pNode;
    PPPP_SEND_PACKET_INFO pSendPacketInfo;
    PNDIS_WAN_PACKET      pWanPacket;
    NDIS_STATUS           Status;

    while (TRUE)
    {
        if (IsListEmpty(&pSession->PendingSendPacketList))
            break;

        //
        // TODO - Only send if 2 or more packets are available
        //        in the MAC packet list. 2 are needed for compression.
        //        Otherwise, when we send with only 1 MAC packet compression
        //        will be skipped and the data will be sent uncompressed,
        //        reducing efficiency.
        //
        pWanPacket = pppMac_GetPacket (pSession->macCntxt);
        if (pWanPacket == NULL)
            break;

        pNode = RemoveHeadList(&pSession->PendingSendPacketList);
        pPacket = (PNDIS_PACKET)CONTAINING_RECORD(pNode, NDIS_PACKET, MiniportReserved);
        pSendPacketInfo = (PPPP_SEND_PACKET_INFO)&pPacket->MiniportReserved[0];

        Status = PppSendIPvXLocked(pSession, pSendPacketInfo, pWanPacket);
        pppUnLock(pSession);
        VEMSendComplete(pSession->pVEMContext, pPacket, Status);
        pppLock(pSession);
    }
}

//** PROTSendComplete - PROT send complete handler.
//
//  This routine is called by the NDIS driver when a send completes.
//  This is a pretty time critical operation, we need to get through here
//  quickly. We'll strip our buffer off and put it back, and call the upper
//  later send complete handler.
//
//  Entry:
//      Handle - The binding handle we specified (really a pointer to an AI).
//      Packet - A pointer to the packet that was sent.
//      Status - Final status of command.
//
//  Exit: Nothing.
//
void NDIS_API
PROTSendComplete(
    NDIS_HANDLE      Handle,
    PNDIS_WAN_PACKET pPacket,
    NDIS_STATUS      Status)
{
    //
    // The Handle is pretty useless since it refers to the miniport
    // adapter binding and not to the particular active session. So,
    // we need to get our pMac via the packet. In pppMac_GetPacket
    // we saved our pMac in the packet's ProtocolReserved1 field.
    //
    macCntxt_t   *pMac = (macCntxt_t *)pPacket->ProtocolReserved1;
    PPPP_SESSION  pSession;

    PPP_LOG_FN_ENTER();

    // DEBUGMSG (ZONE_MAC || (ZONE_ERROR && Status), (TEXT("PPP: ProtSendComplete: Status=%x\n"), Status));

    pSession = PPPADDREFMAC(pMac, REF_SEND_COMPLETE);
    if (pSession)
    {
        DEBUGMSG( ZONE_MAC,
                   ( TEXT( "PPP: (%hs) - Freeing TX DONE Pkt: 0x%08X, Sts: %u\r\n" ),
                   __FUNCTION__,
                   pPacket,
                   Status ) );

        NdisWanFreePacket (pMac, pPacket);

        // Freeing this send packet may have made send resources available,
        // so try to send any pending packets.
        pppLock( pSession );
        SendPendingPackets(pSession);
        pppUnLock( pSession );

        PPPDELREF(pSession, REF_SEND_COMPLETE);
    }

    else
    {
        RETAILMSG( 1,
                   ( TEXT( "PPP: (%hs) - *** WARNING *** " )
                     TEXT( "Orphaned TX Pkt: 0x%08X, Sts: %u\r\n" ),
                   __FUNCTION__,
                   pPacket,
                   Status ) );
    }

    PPP_LOG_FN_LEAVE( 0 );

}

//** PROTTDComplete - PROT transfer data complete handler.
//
//  This routine is called by the NDIS driver when a transfer data
//  call completes. Since we never transfer data ourselves, this must be
//  from the upper layer. We'll just call his routine and let him deal
//  with it.
//
//  Entry:
//      Handle - The binding handle we specified (really a pointer to an AI).
//      Packet - A pointer to the packet used for the TD.
//      Status - Final status of command.
//      BytesCopied - Count of bytes copied.
//
//  Exit: Nothing.
//
void NDIS_API
PROTTDComplete(NDIS_HANDLE Handle, PNDIS_PACKET Packet, NDIS_STATUS Status,
    uint BytesCopied)
{
//    PROTInterface    *ai = (PROTInterface *)Handle;

    DEBUGMSG (ZONE_FUNCTION, (TEXT("+PROTTDComplete(0x%X, 0x%X, 0x%X, %d)\r\n"),
                               Handle, Packet, Status, BytesCopied));
//    IPTDComplete(ai->ai_context, Packet, Status, BytesCopied);

}

//** PROTResetComplete - PROT reset complete handler.
//
//  This routine is called by the NDIS driver when a reset completes.
//
//  Entry:
//      Handle - The binding handle we specified (really a pointer to an AI).
//      Status - Final status of command.
//
//  Exit: Nothing.
//
void NDIS_API
PROTResetComplete(NDIS_HANDLE Handle, NDIS_STATUS Status)
{
    DEBUGMSG (ZONE_FUNCTION, (TEXT("+PROTResetComplete(0x%X, 0x%X)\r\n"),
                               Handle, Status));
}

VOID
PppNdisFreeRequest(
    PNDIS_REQUEST_BETTER    pRequest)
{
    // This is to be used during calls using an event, not a callback func
    ASSERT (pRequest->pCompletionFunc == NULL);

    // Free the event
    NdisFreeEvent(&pRequest->Event);

    // Free the request
    DEBUGMSG(ZONE_ALLOC, (TEXT("PPP: PppNdisFreeRequest Free pRequest=%x\n"), pRequest));
    pppFreeMemory(pRequest, sizeof(*pRequest));
}

VOID
PppNdisRequestCompleteCb(
    PNDIS_REQUEST_BETTER    pRequest,
    NDIS_STATUS             Status
    )
//
//  This function is called when an NDIS request has completed.
//  A request is completed when:
//      Synchronously:  NdisRequest returns a status other than PENDING
//      Asynchronously: NdisRequest returns PENDING and the Protocol Request Complete Handler is called
//
{
    //
    // Call the completion function if there is one.
    // Having a completion function and blocking against the
    // event are mutually exclusive,
    //

    if( pRequest->pCompletionFunc != NULL )
    {
        if (pRequest->pSession)
            pppLock(pRequest->pSession);

        (*pRequest->pCompletionFunc)(pRequest, pRequest->FuncArg, Status);

        if (pRequest->pSession)
        {
            pppUnLock(pRequest->pSession);

            // Delete the session reference taken in PppNdisIssueRequest to
            // prevent it from going away while request was in progress
            PPPDELREF(pRequest->pSession, REF_ISSUEREQUEST);
        }

        // Free the request
        DEBUGMSG(ZONE_ALLOC, (TEXT("PPP: PppNdisRequestCompleteCb Free pRequest=%x\n"), pRequest));
        pppFreeMemory(pRequest, sizeof(*pRequest));
    }
    else // Requesting thread is blocked on event
    {
        // Communicate final status to blocked caller
        pRequest->Status = Status;
        NdisSetEvent( &pRequest->Event );

        // We can't free the request since we're using it to
        // return the Status to the thread blocked on the event.
        // The waiting thread must free
        // the event and the request by calling PppNdisFreeRequest.
    }
}

VOID
PROTRequestComplete(
    IN  NDIS_HANDLE         ProtocolBindingContext,
    IN  PNDIS_REQUEST       NdisRequest,
    IN  NDIS_STATUS         Status
    )
/*++

Routine Description:

    Completion handler for the previously posted request.

    This routine is called by the NDIS driver when a general request
    completes. For synchronous requests, wake up the thread blocked.
    For asynchronous requests, call the completion handler.

Arguments:

    ProtocolBindingContext  Pointer to the binding structure

    NdisRequest             The posted request (this should actually
                            be a pointer to an NDIS_REQUEST_BETTER
                            structure)

    Status                  Completion status

Return Value:

    None

--*/
{
    PNDISWAN_ADAPTER        pAdapter = (PNDISWAN_ADAPTER)ProtocolBindingContext;
    PNDIS_REQUEST_BETTER    pRequest = (PNDIS_REQUEST_BETTER)NdisRequest;

    DEBUGMSG(ZONE_MAC, (TEXT("PPP: PROTRequestComplete %hs pRequest=%x for adapter %s\n"),
        GetOidString(pRequest->Request.DATA.SET_INFORMATION.Oid), pRequest, pAdapter->szAdapterName));

    if (AdapterAddRef(pAdapter))
    {
        PppNdisRequestCompleteCb(pRequest, Status);
        AdapterDelRef(pAdapter);
    }
}

VOID
PppNdisIssueRequestWorker(
    IN      struct CTEEvent *pEvent,
    IN  OUT PVOID            pvarg)
//
//  This function is called by a cxport worker thread to pass the request
//  created by PppNdisIssueRequest to NdisRequest.
//
{
    PNDIS_REQUEST_BETTER  pRequest = (PNDIS_REQUEST_BETTER)pvarg;
    NDIS_STATUS           Status;

    NdisRequest (&Status, pRequest->pAdapter->hAdapter, &pRequest->Request);

    //
    //  If the request completed synchronously, call the completion handler
    //
    if (Status != NDIS_STATUS_PENDING)
        PppNdisRequestCompleteCb(pRequest, Status);
}

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
    OPTIONAL    OUT PNDIS_REQUEST_BETTER    *ppRequest)
//
//  Send an NDIS Request to the miniport.
//
//  This function supports two different request completion methods: Callback or Event.
//  One and only one method must be chosen by the caller.
//
//  For Callback mode, the pCompletionFunc parameter points to the callback
//  function and ppRequest is NULL. When the Ndis Request is completed,
//  pCompletionFunc will be called.
//
//  For Event mode, pCompletionFunc is NULL and ppRequest points to a location
//  where a pointer to the allocated request will be returned. The Event within
//  this structure will be signalled when the request is completed, and the
//  Status field can be checked for the result. The caller should call NdisWaitEvent
//  to wait for the event to be signalled, retrieve the Status, and then call
//  PppNdisFreeRequest to release the request.
//
//  Note: In Event mode if the function fails to allocate the request structure
//  then it will be NULL on return.
//
{
    PNDIS_REQUEST_BETTER    pRequest;
    NDIS_STATUS             Status;
    pppSession_t            *s_p;
    BOOL                    WorkerEventQueued;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +PppNdisIssueRequest - Type=%x Oid=%hs (%x)\n"), Type, GetOidString(Oid), Oid));

    pRequest = pppAllocateMemory(sizeof(*pRequest));
    if (pRequest)
    {
        PNDIS_REQUEST   pNdisRequest = &pRequest->Request;

        DEBUGMSG(ZONE_ALLOC, (TEXT("PPP: PppNdisIssueRequest %x %hs pRequest=%x to adapter %s\n"),
            Type, GetOidString(Oid), pRequest, pAdapter->szAdapterName));
        pNdisRequest->RequestType = Type;
        if (Type == NdisRequestQueryInformation)
        {
            pNdisRequest->DATA.SET_INFORMATION.Oid = Oid;
            pNdisRequest->DATA.SET_INFORMATION.InformationBuffer = InformationBuffer;
            pNdisRequest->DATA.SET_INFORMATION.InformationBufferLength = InformationBufferLength;
        }
        else
        {
            pNdisRequest->DATA.QUERY_INFORMATION.Oid = Oid;
            pNdisRequest->DATA.QUERY_INFORMATION.InformationBuffer = InformationBuffer;
            pNdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength = InformationBufferLength;
        }

        //
        // Maintain a reference to the session while the request is
        // in progress for asynchronous (non-blocking) requests.
        // PppNdisRequestCompleteCb will dereference the session when
        // the request is completed.
        //

        s_p = NULL;
        if (pMac && pCompletionFunc)
        {
            s_p = PPPADDREFMAC(pMac, REF_ISSUEREQUEST);
            ASSERT(s_p);
        }

        pRequest->pSession = s_p;
        pRequest->pCompletionFunc = pCompletionFunc;
        pRequest->FuncArg = FuncArg;

        if (pCompletionFunc == NULL)
            NdisInitializeEvent(&pRequest->Event);

        // Queue the request to be issued to NDIS on a separate thread,
        // so we don't block the current thread.
        CTEInitEvent(&pRequest->CTEEvent, PppNdisIssueRequestWorker);
        pRequest->pAdapter = pAdapter;

        WorkerEventQueued = CTEScheduleEvent(&pRequest->CTEEvent, (PVOID)pRequest);
        if (FALSE == WorkerEventQueued)
        {
            // Unable to issue the request
            PppNdisFreeRequest(pRequest);
            Status = NDIS_STATUS_RESOURCES;
        }
        else
        {
            // Request is queued, will be completed by another thread
            Status = NDIS_STATUS_PENDING;
        }
    }
    else
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR - Unable to allocate memory for NDIS request\n")));
        Status = NDIS_STATUS_RESOURCES;
    }

    if (ppRequest)
        *ppRequest = pRequest;
    *pStatus = Status;
    
    DEBUGMSG(ZONE_FUNCTION || ZONE_ERROR && (*pStatus != NDIS_STATUS_SUCCESS) && (*pStatus != NDIS_STATUS_PENDING), 
        (L"PPP: -PppNdisIssueRequest - Type=%x Oid=%hs (%x) Status=%x\n",  Type, GetOidString(Oid), Oid, Status));
}

VOID
PppNdisDoSyncRequest(
                OUT NDIS_STATUS             *pStatus,
                IN  PNDISWAN_ADAPTER        pAdapter,
                IN  DWORD                   Type,   // NdisRequestSetInformation or NdisRequestQueryInformation
                IN  DWORD                   Oid,
                IN  PVOID                   InformationBuffer,
                IN  DWORD                   InformationBufferLength)
//
//  Issue a request to the adapter and do not return until the request is completed.
//
//  Note: Use with caution.  This function should not be called by any NDIS callback
//  thread, as doing so can result in deadlock.
//
{
    NDIS_REQUEST_BETTER *pRequest = NULL;

    DEBUGMSG(ZONE_FUNCTION, 
        (TEXT("PPP: +PppNdisDoSyncRequest - Type=%x Oid=%hs (%x)\n"), 
        Type, GetOidString(Oid), Oid));

    //
    //  Note that a synchronous request has no need to take a reference
    //  to the session, since the calling thread will block and is
    //  presumed to already have a reference.
    //
    PppNdisIssueRequest(pStatus,
                        pAdapter,
                        Type,
                        Oid,
                        InformationBuffer,
                        InformationBufferLength,
                        NULL,
                        NULL,
                        NULL,
                        &pRequest);

    if (pRequest)
    {
        NdisWaitEvent(&pRequest->Event, 0);
        *pStatus = pRequest->Status;

        PppNdisFreeRequest(pRequest);
    }
    
    DEBUGMSG(ZONE_FUNCTION || ZONE_ERROR && (*pStatus != NDIS_STATUS_SUCCESS), 
        (TEXT("PPP: -PppNdisDoSyncRequest - Type=%x Oid=%hs (%x) Status=%x\n"), 
        Type, GetOidString(Oid), Oid, *pStatus));
}

//** PROTWanRcv - PROT receive data handler.
//
//  This routine is called when data arrives from the NDIS driver.
//
//  Entry:
//      Handle - The binding handle we specified (really a pointer to an AI).
//      Context - NDIS context to be used for TD.
//      Header - Pointer to header
//      HeaderSize - Size of header
//      Data - Pointer to buffer of received data
//      Size - Byte count of data in buffer.
//      TotalSize - Byte count of total packet size.
//
//  Exit: Status indicating whether or not we took the packet.
//
NDIS_STATUS NDIS_API
PROTWanRcv(
    NDIS_HANDLE Handle,
    PUCHAR      Packet,
    ULONG       PacketSize)
{
    pppMsg_t         pppMsg;
    macCntxt_t      *pMac = (macCntxt_t *)Handle;
    pppSession_t    *pSession;  

    PPP_LOG_FN_ENTER();
        
    //DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +PROTRcv(0x%X, 0x%X, %d)\r\n"), Handle, Packet, PacketSize));

    DEBUGMSG( ZONE_PPP,
              ( TEXT( "PPP: (%hs) - RX Pkt: 0x%08X, len: %d\r\n" ),
              __FUNCTION__, Packet, PacketSize ) );

#ifdef DEBUG
        //DumpMem (Packet, PacketSize);
#endif

    pSession = PPPADDREFMAC( pMac, REF_WANRCV );
    if (pSession)
    {
        pppMsg.len  = PacketSize;
        pppMsg.data = Packet;
        pppMsg.cbMACPacket = 0; // RxPacketHandler will fill in size after framing/protocol bytes

        pppLock( pSession );
        pSession->RxPacketHandler( pSession, &pppMsg);
        pppUnLock( pSession );

        PPPDELREF( pSession, REF_WANRCV );
    }

    
    PPP_LOG_FN_LEAVE( 0 );

    return NDIS_STATUS_SUCCESS;
}

//** PROTWanRcvComplete - PROT receive complete handler.
//
//  This routine is called by the NDIS driver after some number of
//  receives. In some sense, it indicates 'idle time'.
//
//  Entry:
//      Handle - The binding handle we specified (really a pointer to an AI).
//
//  Exit: Nothing.
//
void NDIS_API
PROTWanRcvComplete(NDIS_HANDLE Handle)
{
    macCntxt_t      *pMac = (macCntxt_t *)Handle;
}

void
pppLine_GetSLIPLinkInfoCompleteCallback(
    IN  pppSession_t            *s_p,
    IN  NDIS_WAN_GET_LINK_INFO *pInfo,
    IN  NDIS_STATUS             Status
    )
{
    NDIS_WAN_SET_LINK_INFO *pSetInfo = (NDIS_WAN_SET_LINK_INFO *)pInfo;

    if (Status != NDIS_STATUS_SUCCESS)
    {
        DEBUGMSG( ZONE_ERROR, (TEXT( "PPP: Unable to change miniport to SLIP framing\n" )));
        pppChangeOfState( s_p, RASCS_Disconnected, 0 );
    }
    else
    {
        pSetInfo->SendFramingBits &= ~(PPP_FRAMING | PPP_MULTILINK_FRAMING);
        pSetInfo->SendFramingBits |= SLIP_FRAMING;
        pSetInfo->RecvFramingBits &= ~(PPP_FRAMING | PPP_MULTILINK_FRAMING);
        pSetInfo->RecvFramingBits |= SLIP_FRAMING;

        pppMac_SetLink( s_p->macCntxt, pSetInfo );

        // SLIP is now up, let IP and RAS know.

        PPPVEMIPV4InterfaceUp(s_p);
        pppChangeOfState( s_p, RASCS_Connected, 0 );
    }

}

void
PppHandleLineConnectedEvent(
    pppSession_t *s_p)
//
//  This function is called after the line is connected to the peer.
//  That is, the basic ability to send/receive bytes is present and
//  we can proceed on to PPP protocol negotiations.
//
{
    switch( s_p->Mode )
    {
    case PPPMODE_PPP:

        DEBUGMSG( ZONE_PPP, (TEXT( "PPP:MacOpen,LCP Up\r\n" )));

        //
        // IP address assignment done during IPCP negotiation
        //

        pppLcp_LowerLayerUp(s_p->lcpCntxt);
        break;

    case PPPMODE_SLIP:
    case PPPMODE_CSLIP:

        DEBUGMSG( ZONE_PPP, (TEXT( "PPP:SLIP:MacOpen\r\n" )));

        //
        // Change the MAC framing mode to SLIP (default is PPP)
        //
        pppMac_GetLinkInfo(s_p->macCntxt, pppLine_GetSLIPLinkInfoCompleteCallback, s_p);

        break;

    default:
        ASSERT( 0 );
        break;
    }
}

void
PppHandleLineDisconnectedEvent(
    IN OUT PPPP_SESSION   pSession,
    IN OUT macCntxt_t    *pMac)
//
//  This function is called after the line is disconnected from the peer.
//  That is, the basic ability to send/receive bytes has been lost.
//
{
    DWORD ErrorCode;

    switch( pSession->Mode )
    {
    case PPPMODE_PPP:
        DEBUGMSG( ZONE_PPP, (TEXT( "PPP:MacClosed\r\n" )));
        // Tell LCP that the MAC layer is down, LCP will tear down higher layers
        pppLcp_LowerLayerDown(pSession->lcpCntxt);
        break;

    case PPPMODE_SLIP:
    case PPPMODE_CSLIP:
        DEBUGMSG( ZONE_PPP, (TEXT( "PPP:SLIP:MacClosed\r\n" )));
        // Tear down the IP interface directly, since there is no IPCP when running SLIP
        PPPVEMIPV4InterfaceDown(pSession);
        break;

    default:
        ASSERT( FALSE );
        break;
    }

    // Complete any pending call drop requests
    pppExecuteCompleteCallbacks(&pMac->pPendingCallDropCompleteList);

    if (pSession->HangUpReq)
        ErrorCode = NO_ERROR;
    else if (pSession->bDisconnectDueToUnresponsivePeer)
        ErrorCode = ERROR_IDLE_DISCONNECTED;
    else if (pSession->bUseRasErrorFromAuthFailure)
        ErrorCode = pSession->RasError;
    else
        ErrorCode = pMac->TapiEcode;

    // Indicate the Disconnected state
    pppChangeOfState( pSession, RASCS_Disconnected, ErrorCode);

    SetEvent(pMac->hNdisTapiEvent);
}

//** PROTStatus - PROT status handler.
//
//  Called by the NDIS driver when some sort of status change occurs.
//  We take action depending on the type of status.
//
//  Entry:
//      Handle - The binding handle we specified (really a pointer to an AI).
//      GStatus - General type of status that caused the call.
//      Status - Pointer to a buffer of status specific information.
//      StatusSize - Size of the status buffer.
//
//  Exit: Nothing.
//
void NDIS_API
PROTStatus(
    NDIS_HANDLE     Handle, 
    NDIS_STATUS     GStatus, 
    void            *Status,    
    uint            StatusSize)
{
    PNDISWAN_ADAPTER    pAdapter = (PNDISWAN_ADAPTER)Handle;
    PNDIS_TAPI_EVENT    pTapiEvent;
    macCntxt_t          *pMac;
    PNDIS_MAC_LINE_UP   pMacLineUp;
    PNDIS_MAC_LINE_DOWN pMacLineDown;
    pppSession_t        *s_p;
    
    DEBUGMSG (ZONE_FUNCTION, (TEXT("PPP: +PROTStatus(0x%X, 0x%X, 0x%X, %d)\r\n"),
                               Handle, GStatus, Status, StatusSize));
    switch (GStatus)
    {
    case NDIS_STATUS_TAPI_INDICATION:
        pTapiEvent = (PNDIS_TAPI_EVENT)Status;
        ASSERT (StatusSize >= sizeof(NDIS_TAPI_EVENT));

        DEBUGMSG (ZONE_MAC, (TEXT("PPP: NDIS_STATUS_TAPI_INDICATION %d from MAC\n"), pTapiEvent->ulMsg));
        
        switch (pTapiEvent->ulMsg)
        {
        case LINE_CALLINFO :
            // Information about the call has changed.
            if (pTapiEvent->ulParam1 & LINECALLINFOSTATE_RATE )
            {
                pMac = (macCntxt_t *)pTapiEvent->htLine;
                s_p = PPPADDREFMAC(pMac, REF_LINE_CALLSTATE);
                if (s_p)
                {
                    // Data rate has changed.
                    pMac->LinkSpeed = pTapiEvent->ulParam2;

                    DEBUGMSG(ZONE_MAC, (L"PPP: Link speed changed to %u Kbps\n", pMac->LinkSpeed / 10));

                    if (RAS_NOTIFIERTYPE_CE_MSGQ == s_p->notifierType)
                    {
                        BOOL WriteSuccessful;
                        RASEventMessage Msg;

                        Msg.dwSize = sizeof(Msg);
                        Msg.dwEventId = RASEVENTID_SPEEDCHANGE;
                        Msg.u.SpeedChange.LinkBps = pMac->LinkSpeed * 100;
                        WriteSuccessful = WriteMsgQueue((HANDLE)s_p->hMsgQRAS, &Msg, sizeof(Msg), 0, 0);
                        DEBUGMSG(ZONE_ERROR && !WriteSuccessful, (L"PPP: Error-WriteMsgQueue failed %u\n", GetLastError()));
                    }
                    PPPDELREF(s_p, REF_LINE_CALLSTATE);
                }
            }
            break;

        case LINE_CREATE :
            if (AdapterAddRef(pAdapter))
            {
                if ((pTapiEvent->ulParam1+1) > pAdapter->dwNumDevices)
                {
                    NdisTapiSetNumLineDevs(pAdapter, pTapiEvent->ulParam1 + 1);
                    g_dwTotalLineCount++;
                }
                AdapterDelRef(pAdapter);
            } else {
                DEBUGMSG (ZONE_ERROR, (TEXT("PROTStatus: Invalid pAdapter?\r\n")));
                ASSERT (0);
            }
            break;

        case LINE_NEWCALL:
            DEBUGMSG (ZONE_PPP, (TEXT("PPP: LINE_NEWCALL\n")));
            pMac = (macCntxt_t *)pTapiEvent->htLine;

            if (s_p = PPPADDREFMAC(pMac, REF_LINE_NEWCALL))
            {
                //
                // Store the miniport driver's call handle, to be passed
                // on calls into the driver.
                //
                pMac->hCall = pTapiEvent->ulParam1;
                pMac->bCallClosed = FALSE;

                //
                // Return a transport driver call handle to the miniport for it
                // to use in subsequent indications for this call.
                //
                pTapiEvent->ulParam2 = (HTAPI_CALL)pMac;

                //
                // A LINE_CALLSTATE of OFFERING should follow soon.
                //
                
                PPPDELREF(s_p, REF_LINE_NEWCALL);
            }
            break;

        case LINE_CALLSTATE :
            pMac = (macCntxt_t *)pTapiEvent->htLine;

            DEBUGMSG (ZONE_MAC, (TEXT("PPP: LINE_CALLSTATE %d indication from MAC layer\n"), pTapiEvent->ulParam1));

            if (s_p = PPPADDREFMAC(pMac, REF_LINE_CALLSTATE))
            {
                pppLock(s_p);

                pMac->dwLineCallState = pTapiEvent->ulParam1;

                switch (pTapiEvent->ulParam1)
                {
                case LINECALLSTATE_CONNECTED :
                    DEBUGMSG (ZONE_MAC, (TEXT("PPP: Callstate CONNECTED....\r\n")));

                    if (s_p->bIsServer)
                    {
                        (void)NdisTapiGetDeviceIdAsync(pMac, LINECALLSELECT_CALL, TEXT("ndis"));
                        PppHandleLineConnectedEvent(s_p);
                    }
                    else
                    {
                        pMac->TapiEcode = SUCCESS;
                        SetEvent (pMac->hNdisTapiEvent);
                    }
                    break;

                case LINECALLSTATE_DISCONNECTED:
                    //
                    // The remote party has disconnected from the call.
                    // ulParam2 contains additional info about the cause of
                    // the disconnection as a LINEDISCONNECTMODE_XXX value.
                    //
                    DEBUGMSG (ZONE_MAC, (TEXT("PPP: Callstate DISCONNECTED %x\n"), pTapiEvent->ulParam2));
                    switch (pTapiEvent->ulParam2)
                    {
                    case LINEDISCONNECTMODE_NORMAL:
                        // This gets reported for a "NO CARRIER" during a connect attempt
                        // Also when a connection is closed because we initiated it this
                        // error occurs.
                        if (pMac->TapiEcode == 0)
                            pMac->TapiEcode = ERROR_NO_CARRIER;
                        break;

                    case LINEDISCONNECTMODE_BUSY:
                        // The remote user's station is busy. 
                        pMac->TapiEcode = ERROR_LINE_BUSY;
                        break;

                    case LINEDISCONNECTMODE_NOANSWER:
                        // The remote user's station does not answer. 
                        pMac->TapiEcode = ERROR_NO_ANSWER;
                        break;

                    case LINEDISCONNECTMODE_NODIALTONE:
                        // A dial tone was not detected
                        pMac->TapiEcode = ERROR_NO_DIALTONE;
                        break;

                    case LINEDISCONNECTMODE_BADADDRESS:
                        // The destination address is invalid.
                        pMac->TapiEcode = ERROR_BAD_PHONE_NUMBER;
                        break;

                    case LINEDISCONNECTMODE_REJECT:
                        // The remote user has rejected the call.
                        pMac->TapiEcode = ERROR_REMOTE_DISCONNECTION;
                        break;

                    case LINEDISCONNECTMODE_UNREACHABLE:
                        // The remote user could not be reached.
                        pMac->TapiEcode = ERROR_NO_CARRIER;
                        break;

                    case LINEDISCONNECTMODE_UNAVAIL:
                        // The port is in use by another process
                        pMac->TapiEcode = ERROR_PORT_NOT_AVAILABLE;
                        break;

                    case LINEDISCONNECTMODE_UNKNOWN:
                    case LINEDISCONNECTMODE_PICKUP:
                    case LINEDISCONNECTMODE_FORWARDED:
                    case LINEDISCONNECTMODE_CONGESTION:
                    case LINEDISCONNECTMODE_INCOMPATIBLE:
                    default:
                        pMac->TapiEcode = ERROR_PORT_DISCONNECTED;
                        break;
                    }
                    PppHandleLineDisconnectedEvent(s_p, pMac);
                    break;

                case LINECALLSTATE_IDLE:
                    //
                    // The call exists but has not been connected.
                    // No activity exists on the call, which means that no call is currently active.
                    // A call can never transition out of the idle state. 
                    // 
                    DEBUGMSG (ZONE_MAC, (TEXT("PPP: Callstate IDLE....\n")));
                    pMac->TapiEcode = ERROR_PORT_DISCONNECTED;
                    PppHandleLineDisconnectedEvent(s_p, pMac);
                    break;

                case LINECALLSTATE_DIALING :
                case LINECALLSTATE_DIALTONE :
                case LINECALLSTATE_PROCEEDING :
                    DEBUGMSG (ZONE_MAC, (TEXT("PPP: Callstate DIAL....\r\n")));
                    pppChangeOfState( pMac->session, RASCS_PortOpened, 0 );
                    break;

                case LINECALLSTATE_BUSY :
                    DEBUGMSG (ZONE_MAC, (TEXT("PPP: Callstate BUSY....\r\n")));
                    pMac->TapiEcode = ERROR_LINE_BUSY;
                    SetEvent (pMac->hNdisTapiEvent);
                    break;

                case LINECALLSTATE_OFFERING :
                    //
                    // Answer the call
                    //
                    // This will typically result in a LINECALLSTATE_CONNECTED indication
                    // from the miniport shortly.
                    //
                    DEBUGMSG (ZONE_PPP, (TEXT("PPP: ANSWERING\n")));
                    if (s_p->bIsServer)
                        (void)NdisTapiLineAnswer(pMac, NULL, 0);
                    break;

                case LINECALLSTATE_UNKNOWN :
                    // This is the initial state of a new call
                    break;

                default:
                    /*
                    case LINECALLSTATE_ACCEPTED :
                    case LINECALLSTATE_RINGBACK :
                    case LINECALLSTATE_SPECIALINFO :
                    case LINECALLSTATE_ONHOLD :
                    case LINECALLSTATE_CONFERENCED :
                    case LINECALLSTATE_ONHOLDPENDCONF :
                    */
                    DEBUGMSG (ZONE_MAC, (TEXT("PPP: Unhandled Callstate %d...\r\n"),
                                          pTapiEvent->ulParam1));
                    ASSERT (0);
                    break;
                }

                pppUnLock(s_p);

                PPPDELREF(s_p, REF_LINE_CALLSTATE);
            }
            break;
        case LINE_REPLY:
            pMac = (macCntxt_t *)pTapiEvent->htLine;

            if (s_p = PPPADDREFMAC(pMac, REF_LINE_REPLY))
            {
                DEBUGMSG (ZONE_MAC, (TEXT("PPP: LINE_REPLY: ulParam1=%d ulParam2=%d ulParam3=%d\r\n"),
                              pTapiEvent->ulParam1, pTapiEvent->ulParam2,
                              pTapiEvent->ulParam3));
                if (pTapiEvent->ulParam2 && (0 == pMac->TapiEcode)) {
                    DEBUGMSG (ZONE_MAC, (TEXT("PPP: Got LineReply with error 0x%X and hadn't got previous LINECALLSTATE error\r\n"),
                                  pTapiEvent->ulParam2));
                    pMac->TapiEcode = ERROR_NO_ANSWER;
                    SetEvent (pMac->hNdisTapiEvent);
                }
                PPPDELREF(s_p, REF_LINE_REPLY);
            }
            break;
                
        default :
            DEBUGMSG (ZONE_MAC, (TEXT("PPP: PROTStatus: Unhandled NDIS_STATUS_TAPI_INDICATION %d(0x%X)\r\n"),
                          pTapiEvent->ulMsg, pTapiEvent->ulMsg));
            break;
        }
        break;

    case NDIS_STATUS_WAN_LINE_UP :
        pMacLineUp = (PNDIS_MAC_LINE_UP)Status;
        ASSERT (StatusSize >= sizeof(NDIS_MAC_LINE_UP));
        DEBUGMSG (ZONE_MAC, (TEXT("PPP: PROTStatus: WAN Line Up\r\n")));
        DEBUGMSG (ZONE_MAC, (TEXT("  LinkSpeed = %d\r\n"), pMacLineUp->LinkSpeed));
        DEBUGMSG (ZONE_MAC, (TEXT("  Quality = %d (0=Raw, 1=ErrorControl, 2=Reliable)\r\n"), pMacLineUp->Quality));
        DEBUGMSG (ZONE_MAC, (TEXT("  SendWindow = %d\r\n"), pMacLineUp->SendWindow));
        DEBUGMSG (ZONE_MAC, (TEXT("  ConnectionWrapperID = 0x%X\r\n"), pMacLineUp->ConnectionWrapperID));
        DEBUGMSG (ZONE_MAC, (TEXT("  NdisLinkHandle = 0x%X\r\n"), pMacLineUp->NdisLinkHandle));
        
        pMac = (macCntxt_t *)pMacLineUp->ConnectionWrapperID;
        s_p = PPPADDREFMAC(pMac, REF_WAN_LINE_UP);
        if (s_p)
        {
            pMacLineUp->NdisLinkContext = pMac; // For multi-link this will have to be different

            // Save info away.
            pMac->NdisLinkHandle = pMacLineUp->NdisLinkHandle;
            if (pMacLineUp->LinkSpeed) {
                pMac->LinkSpeed = pMacLineUp->LinkSpeed;
            } else {
                pMac->LinkSpeed = 288;  // Default speed
            }
            pMac->Quality = pMacLineUp->Quality;
            pMac->SendWindow = pMacLineUp->SendWindow;

            PPPDELREF(s_p, REF_WAN_LINE_UP);
        }
        else
        {
            DEBUGMSG(ZONE_WARN, (L"PPP: WARNING- NDIS_STATUS_WAN_LINE_UP received for invalid pMac %x\n", pMac));
            pMacLineUp->NdisLinkContext = 0;
        }
        break;

    case NDIS_STATUS_WAN_LINE_DOWN :
        pMacLineDown = (PNDIS_MAC_LINE_DOWN)Status;
        ASSERT (StatusSize >= sizeof(NDIS_MAC_LINE_DOWN));
        DEBUGMSG (ZONE_MAC, (TEXT("PPP: PROTStatus: WAN Line Down\r\n")));
        
        pMac = (macCntxt_t *)pMacLineDown->NdisLinkContext;

        if (s_p = PPPADDREFMAC(pMac, REF_WAN_LINE_DOWN))
        {
            pppLock(s_p);
            pMac->TapiEcode = ERROR_PORT_DISCONNECTED;
            pppLcp_LowerLayerDown(s_p->lcpCntxt);
            pppUnLock(s_p);

            PPPDELREF(s_p, REF_WAN_LINE_DOWN);
        }
        break;

    case NDIS_STATUS_RESET_START:
        break;

    case NDIS_STATUS_RESET_END:
        break;
        
    default :
        DEBUGMSG (ZONE_WARN, (TEXT("PPP: WARNING - PROTStatus: Unhandled GStatus=%X\n"), GStatus));
        break;
        
    }
    DEBUGMSG (ZONE_FUNCTION, (TEXT("PPP: -PROTStatus:\r\n")));

}

//** PROTStatusComplete - PROT status complete handler.
//
//  A routine called by the NDIS driver so that we can do postprocessing
//  after a status event.
//
//  Entry:
//      Handle - The binding handle we specified (really a pointer to an AI).
//
//  Exit: Nothing.
//
void NDIS_API
PROTStatusComplete(NDIS_HANDLE Handle)
{
    DEBUGMSG (ZONE_FUNCTION, (TEXT("PPP: +PROTStatusComplete(0x%X)\r\n"),
                               Handle));

}

//*     PROTBindAdapter - Bind and initialize an adapter.
//
//      Called in a PNP environment to initialize and bind an adapter. We open
//      the adapter and get it running, and then we call up to IP to tell him
//      about it. IP will initialize, and if all goes well call us back to start
//      receiving.
//
//      Input:  RetStatus               - Where to return the status of this call.
//              BindContext             - Handle to use for calling BindAdapterComplete.
//              AdapterName             - Pointer to name of adapter.
//              SS1                     - System specific 1 parameter.
//              SS2                     - System specific 2 parameter.
//
//      Returns: Nothing.
//  
//      OmarM - modified for WinCE
void NDIS_API
PROTBindAdapter(
        OUT PNDIS_STATUS    RetStatus,
    IN      NDIS_HANDLE     BindContext,
    IN      PNDIS_STRING    AdapterName,
    IN      PVOID           SS1,
    IN      PVOID           SS2)
{
    DEBUGMSG (ZONE_MAC, (L"PPP: +PROTBindAdapter(%s)\n", AdapterName->Buffer));

    *RetStatus = AddAdapter(AdapterName);

    DEBUGMSG(ZONE_MAC, (L"PPP: -PROTBindAdapter %s Status=%x\n", AdapterName->Buffer, *RetStatus));
}

static void
pppSessionUnbindCloseComplete(
    PVOID   pData)
{
    HANDLE          hCloseCompleteEvent = (HANDLE)pData;

    DEBUGMSG(ZONE_MAC, (L"PPP: Unbind initiated pppSessionStop has completed\n"));
}

//* PROTUnbindAdapter - Unbind from an adapter.
//
//  Called when we need to unbind from an adapter. We'll call up to IP to tell
//  him. When he's done, we'll free our memory and return.
//
//  Input:  RetStatus       - Where to return status from call.
//          ProtBindContext - The context we gave NDIS earlier - really a
//                              pointer to an PROTInterface structure.
//          UnbindContext   - Context for completeing this request.
//
//  Returns: Nothing.
//
void NDIS_API
PROTUnbindAdapter(
        OUT PNDIS_STATUS    RetStatus,
    IN      NDIS_HANDLE     ProtBindContext,
    IN      NDIS_HANDLE     UnbindContext)
{
    PNDISWAN_ADAPTER pAdapter = (PNDISWAN_ADAPTER)ProtBindContext;
    PPPP_SESSION     pSession;
    macCntxt_t      *pMac;

    DEBUGMSG (ZONE_INIT|ZONE_FUNCTION, 
        (TEXT("+PROTUnbindAdapter(0x%X, 0x%X, 0x%X)\r\n"),
                  RetStatus, ProtBindContext, UnbindContext));

    DEBUGMSG(ZONE_MAC, (L"PPP: Unbind adapter %s %x refcnt=%d\n", pAdapter->szAdapterName, pAdapter, pAdapter->dwRefCnt));

    *RetStatus = NDIS_STATUS_ADAPTER_NOT_FOUND;

    //
    //  Validate pAdapter
    //
    if (AdapterAddRef(pAdapter))
    {
        pAdapter->hUnbindContext = UnbindContext;

        //
        //  Prevent any new sessions from using the adapter
        //
        pAdapter->bClosingAdapter = TRUE;

        //
        //  Close any active sessions for the adapter
        //

        EnterCriticalSection (&v_ListCS);

        // Find the context in the global list
        for (pSession = g_PPPSessionList; pSession; pSession = pSession->Next)
        {
            if (pMac = (macCntxt_t *)(pSession->macCntxt))
            {
                if (pMac->pAdapter == pAdapter)
                {
                    //
                    // pSession is using the adapter from which
                    // we are unbinding. Close the session.
                    //
                    DEBUGMSG(ZONE_MAC, (L"PPP: Stop session %s adapter %s\n", pSession->rasEntry.szDeviceName, pAdapter->szAdapterName));
                    pppSessionStop(pSession, pppSessionUnbindCloseComplete, NULL);
                }
            }
        }

        LeaveCriticalSection (&v_ListCS);

        //
        //  Delete the reference above,
        //  
        AdapterDelRef(pAdapter);

        //
        //  Delete the original reference from PROTBindAdapter
        //
        AdapterDelRef(pAdapter);

        //
        //  When all sessions on the adapter are closed,
        //  the last reference to the session will be removed.
        //  At that point, NdisCloseAdapter will be called.
        //  When the NdisCloseAdapter completes, we will
        //  call NdisCompleteUnbindAdapter.  At this point,
        //  just return NDIS_STATUS_PENDING.
        //
        *RetStatus = NDIS_STATUS_PENDING;

        DEBUGMSG(ZONE_MAC, (L"PPP: Unbind adapter %s Status=%d\n", pAdapter->szAdapterName, *RetStatus));
    }


    DEBUGMSG (ZONE_INIT|ZONE_FUNCTION, (TEXT("-PROTUnbindAdapter:\r\n")));
}
