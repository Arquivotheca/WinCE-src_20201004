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
*   @doc EX_RAS
*   snddata.c    Send Data functions
*
*   Date: 4/28/99
*
*/


//  Include Files

#include "windows.h"
#include "ndis.h"
#include "ras.h"
#include "raserror.h"
#include "cxport.h"
#include "protocol.h"
#include "ppp.h"
#include "macp.h"
#include "cclib.h"

BOOLEAN
CopyNdisBufferChainToFlatBuffer(
    IN     PNDIS_BUFFER        pNdisBuffer,
    IN     DWORD            dwOffset,
    IN     PBYTE            pFlatBuffer,
    IN OUT PDWORD           pcbFlatBuffer)
//
//    Copy the data in an NDIS buffer chain into contiguous memory.
//
//  Return FALSE if the chain of data will not fit in the contiguous memory.
{
    BOOLEAN bSuccess = TRUE;
    PBYTE    pNdisBufferData;
    UINT    cbNdisBufferData;
    DWORD   cbFlatBuffer = *pcbFlatBuffer;
    DWORD   cbCopied = 0;

    while (TRUE)
    {
        if (NULL == pNdisBuffer)
        {
            // Copied all data from the buffer chain successfullly
            *pcbFlatBuffer = cbCopied;
            break;
        }

        cbNdisBufferData = pNdisBuffer->ByteCount;
        if (dwOffset >= cbNdisBufferData)
        {
            // This entire buffer is to be skipped
            dwOffset -= cbNdisBufferData;
        }
        else
        {
            pNdisBufferData = (PBYTE)pNdisBuffer->StartVa + pNdisBuffer->ByteOffset;
            cbNdisBufferData -= dwOffset;
            pNdisBufferData += dwOffset;
            dwOffset = 0;
            if ((cbCopied + cbNdisBufferData) > cbFlatBuffer)
            {
                // Flat buffer was not big enough to copy buffer chain
                bSuccess = FALSE;
                break;
            }
            memcpy (pFlatBuffer + cbCopied, pNdisBufferData, cbNdisBufferData);
            cbCopied += cbNdisBufferData;
        }
        NdisGetNextBuffer(pNdisBuffer, &pNdisBuffer);
    }

    return bSuccess;
}

DWORD
pppSendData(
    IN    void  *session,
    IN    USHORT ProtocolWord,
    IN    PBYTE  pSrcData,
    IN    DWORD  cbSrcData)
//
//    Send a ppp frame from a contiguous data source.
//
//    ProtocolWord is one of the constants:
//        PPP_PROTOCOL_LCP,
//        PPP_PROTOCOL_xxx, etc
//  that specifies which protocol header to prepend to the data.
//
//    pSrcData should point to a properly formatted packet of the form:
//        <code> <id> <len:2 bytes> <body:len-4 bytes>
//  The len field must be in MSB first order!
//
//    The ProtocolWord is not yet attached to the packet, because it
//    may be sent compressed.
//
{
    macCntxt_t            *pMac = ((pppSession_t *)session)->macCntxt;
    PNDIS_WAN_PACKET    pPacket;
    NDIS_STATUS            Status;
    DWORD                dwResult = NO_ERROR;

    ASSERT(cbSrcData >= 4);
    ASSERT((DWORD)((pSrcData[2] << 8) | pSrcData[3]) == cbSrcData);

    pPacket = pppMac_GetPacket (((pppSession_t *)session)->macCntxt);
    if (!pPacket)
    {
        DEBUGMSG( ZONE_ERROR, (TEXT("PPP: ERROR - pppSendData - no Tx Buffers\n")));
    }
    else
    {
        // Leave room for the protocol header
        // (2 bytes for address/control, 2 bytes for protocol id).
        pPacket->CurrentBuffer += 4;
        if (cbSrcData <= pPacket->CurrentLength - 4)
        {
            memcpy (pPacket->CurrentBuffer, pSrcData, cbSrcData);
            pPacket->CurrentLength = cbSrcData;
            Status = pppMacSndData (pMac, ProtocolWord, pPacket);
        }
        else
        {
            ASSERT(FALSE);
        }
    }

    return dwResult;
}

NDIS_STATUS
pppMacSndData(
    macCntxt_t        *pMac,
    USHORT             wProtocol,
    PNDIS_WAN_PACKET pPacket)
{
    NDIS_STATUS Status;
    PUCHAR        pData;

    PPP_LOG_FN_ENTER();

    // DEBUGMSG (ZONE_FUNCTION, (TEXT("PPP: +pppMacSndData (0x%X, 0x%X, 0x%X)\r\n"),
    //                            pMac, wProtocol, pPacket));

    if (PPP_SLIP_PROTOCOL != wProtocol)
    {
        pData = pPacket->CurrentBuffer;

        //
        //    Prepend the protocol field, which can be compressed down from
        //    2 bytes to 1 byte if Protocol field compression has been
        //    negotiated and the upper byte of the protocol is 0x00.
        //
        //    Also, per IETF STD51 (PPP) section 6.5:
        //        "The Protocol field is never compressed when sending any LCP
        //         packet."
        //    However, since the protocol ID for LCP is 0xC021, with the high
        //    byte non-zero, it will never be compressed per the normal
        //  compression rules, so a special case is unnecessary..
        //

        // Lower byte of protocol always sent
        *--pData = (BYTE)wProtocol;

        if ((wProtocol & 0xFF00)
        ||  !(pMac->dwSendFramingBits & PPP_COMPRESS_PROTOCOL_FIELD))
        {
            // Upper byte of protocol must be sent - do not compress
            *--pData = (BYTE)((wProtocol & 0xFF00) >> 8);
        }

        //
        //    Prepend the address and control bytes (FF 03) if not compressed.
        //
        //    Also, per IETF STD51 (PPP) section 6.6:
        //        "The Address and Control fields MUST NOT be compressed
        //        when sending any LCP packet."
        //
        //  Also, per PPPoE RFC, the Address and Control fields are never sent
        //  for any PPPoE frame.
        //
        if (!pMac->bIsPPPoE
        &&  ((PPP_PROTOCOL_LCP == wProtocol) ||  !(pMac->dwSendFramingBits & PPP_COMPRESS_ADDRESS_CONTROL)))
        {
            *--pData = PPP_UI;
            *--pData = PPP_ALL_STATIONS;
        }
        
        //
        //    The current pointer must always be between the absolute
        //    start and end of the packet buffer memory.
        //
        ASSERT(pPacket->StartBuffer <= pData && pData < pPacket->EndBuffer);

        //
        //    We must not have intruded into the packet header space reserved for
        //    the MAC layer.
        //
        ASSERT(pPacket->StartBuffer + pMac->WanInfo.HeaderPadding <= pData);

        // Adjust packet size to reflect prepended header
        pPacket->CurrentLength += pPacket->CurrentBuffer - pData;
        pPacket->CurrentBuffer  = pData;

        //
        //  Must not intrude into packet trailer space reserved for MAC.
        //
        ASSERT(pData + pPacket->CurrentLength <= pPacket->EndBuffer - pMac->WanInfo.TailPadding);
    }
    
#ifdef DEBUG
    if (ZONE_TRACE) {
        DEBUGMSG (1, (TEXT("PPP: pppMacSndData: About to send packet (%d):\r\n"), pPacket->CurrentLength));
        DumpMem (pPacket->CurrentBuffer, pPacket->CurrentLength);
    }
#endif

    pppUnLock(pMac->session);

    DEBUGMSG ( ZONE_MAC, 
               (TEXT("PPP: (%hs) - Sending Pkt: 0x%08X, Len: %u\r\n"), 
               __FUNCTION__,
               pPacket,
               pPacket->CurrentLength));

    WanMiniportSend (&Status, pMac->pAdapter->hAdapter, (NDIS_HANDLE)pMac->hCall, pPacket);
    pppLock(pMac->session);

    // If they returned PENDING then the SendComplete handler will free the packet.
    if (NDIS_STATUS_SUCCESS == Status)
    {
        // DEBUGMSG (ZONE_MAC, (TEXT("PPP: pppMacSndData: Success from WanMiniportSend\r\n")));
        NdisWanFreePacket (pMac, pPacket);
    }
    else if (NDIS_STATUS_PENDING != Status)
    {
        DEBUGMSG (ZONE_ERROR, (TEXT("PPP: pppMacSndData: Error 0x%X from WanMiniportSend\r\n"), Status));
        NdisWanFreePacket (pMac, pPacket);
    }
    else
    {
        // DEBUGMSG (ZONE_MAC, (TEXT("PPP: pppMacSndData: Status Pending from WanMiniportSend, pPacket=0x%X\r\n"), pPacket));
        Status = NDIS_STATUS_SUCCESS;
    }

    // DEBUGMSG (ZONE_FUNCTION, (TEXT("PPP: -pppMacSndData: Status=0x%X\r\n"), Status));

    PPP_LOG_FN_LEAVE( Status );

    return Status;
}

typedef struct
{
    void    (*pCallbackFunc)(PVOID, NDIS_WAN_GET_LINK_INFO *, NDIS_STATUS);
    PVOID    pCallbackData;
} GetLinkInfoCallbackData;

void
PppNdisGetLinkInfoCompleteCallback(
    NDIS_REQUEST_BETTER        *pRequest,
    GetLinkInfoCallbackData    *pGetLinkInfoCallbackData,
    NDIS_STATUS                Status)
//
//    This function is called when the miniport driver completes
//    an OID_WAN_GET_LINK_INFO request.
//
{
    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +PppNdisGetLinkInfoCompleteCallback Status=%x\n"), Status));

    //
    // Execute the callback
    //
    pGetLinkInfoCallbackData->pCallbackFunc(
        pGetLinkInfoCallbackData->pCallbackData,
        pRequest->Request.DATA.QUERY_INFORMATION.InformationBuffer,
        Status);
    
    // Free the memory allocated for the request.
    pppFreeMemory(pRequest->Request.DATA.QUERY_INFORMATION.InformationBuffer, sizeof(NDIS_WAN_GET_LINK_INFO));

    // Free the memory allocated for the callback info.
    pppFreeMemory(pGetLinkInfoCallbackData, sizeof(*pGetLinkInfoCallbackData));

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -PppNdisGetLinkInfoCompleteCallback\n")));
}

void
pppMac_GetLinkInfo(
    IN    PVOID            pvArg,
    IN    void            (*pGetLinkInfoCompleteCallback)(PVOID, NDIS_WAN_GET_LINK_INFO *, NDIS_STATUS),
    IN    PVOID            pCallbackData)
{
    macCntxt_t                 *pMac = pvArg;
    NDIS_STATUS                 Status;
    PNDIS_WAN_GET_LINK_INFO  pLinkInfo;
    GetLinkInfoCallbackData  *pGetLinkInfoCallbackData;

    DEBUGMSG (ZONE_FUNCTION, (TEXT("PPP: +pppMac_GetLinkInfo\n")));

    if ((NDIS_HANDLE)pMac->hCall == INVALID_HANDLE_VALUE)
    {
        //
        //    If the call just got closed by NdisTapiHangup, don't try to
        //    talk to the adapter.
        //
        pGetLinkInfoCompleteCallback(pCallbackData, NULL, NDIS_STATUS_CLOSING);
    }
    else
    {
        pGetLinkInfoCallbackData = pppAllocateMemory(sizeof(*pGetLinkInfoCallbackData));
        if (pGetLinkInfoCallbackData == NULL)
        {
            // Unable to queue the callback
            pGetLinkInfoCompleteCallback(pCallbackData, NULL, NDIS_STATUS_RESOURCES);
        }
        else
        {
            pGetLinkInfoCallbackData->pCallbackFunc = pGetLinkInfoCompleteCallback;
            pGetLinkInfoCallbackData->pCallbackData = pCallbackData;

            //
            //    Issue a new get link info request
            //

            Status = NDIS_STATUS_RESOURCES;

            pLinkInfo = pppAllocateMemory(sizeof(*pLinkInfo));
            if (pLinkInfo)
            {
                pLinkInfo->NdisLinkHandle = (NDIS_HANDLE)pMac->hCall;

                pppUnLock(pMac->session);
                PppNdisIssueRequest(&Status, 
                                    pMac->pAdapter,
                                    NdisRequestQueryInformation,
                                    OID_WAN_GET_LINK_INFO,
                                    pLinkInfo,
                                    sizeof(*pLinkInfo),
                                    pMac,
                                    PppNdisGetLinkInfoCompleteCallback,
                                    pGetLinkInfoCallbackData,
                                    NULL);
                pppLock(pMac->session);
            }
            else
            {
                pppFreeMemory(pGetLinkInfoCallbackData, sizeof(*pGetLinkInfoCallbackData));
                pGetLinkInfoCompleteCallback(pCallbackData, NULL, NDIS_STATUS_RESOURCES);
            }
        }
    }

    DEBUGMSG (ZONE_FUNCTION, (TEXT("PPP: -pppMac_GetLinkInfo\n")));
}

void
PppNdisSetLinkInfoCompleteCallback(
    NDIS_REQUEST_BETTER        *pRequest,
    GetLinkInfoCallbackData    *pGetLinkInfoCallbackData,
    NDIS_STATUS                Status)
//
//    This function is called when the miniport driver completes
//    an OID_WAN_SET_LINK_INFO request.
//
{
    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +PppNdisSetLinkInfoCompleteCallback Status=%x\n"), Status));
    
    // Free the memory allocated for the request.
    pppFreeMemory(pRequest->Request.DATA.SET_INFORMATION.InformationBuffer, sizeof(NDIS_WAN_SET_LINK_INFO));

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -PppNdisSetLinkInfoCompleteCallback\n")));
}

VOID
pppMac_SetLink(
    IN    macCntxt_t                *pMac,
    IN    PNDIS_WAN_SET_LINK_INFO  pLinkInfo)
{
    PNDIS_WAN_SET_LINK_INFO  pNewLinkInfo;
    NDIS_STATUS                 Status;

    DEBUGMSG (ZONE_FUNCTION, (TEXT("PPP: +pppMac_SetLink\n")));

    pLinkInfo->HeaderPadding = pMac->WanInfo.HeaderPadding;
    pLinkInfo->TailPadding   = pMac->WanInfo.TailPadding;

    //
    //    If the call just got closed by NdisTapiHangup, don't try to
    //    talk to the adapter.
    //
    if ((NDIS_HANDLE)pMac->hCall != INVALID_HANDLE_VALUE)
    {

        DEBUGMSG(ZONE_MAC, (L"PPP: Set SendACCM=%08X RecvACCM=%08X\n",
                                pLinkInfo->SendACCM,
                                pLinkInfo->RecvACCM));

        //
        //    Issue a new set link info request
        //

        Status = NDIS_STATUS_RESOURCES;

        pNewLinkInfo = pppAllocateMemory(sizeof(*pNewLinkInfo));
        if (pNewLinkInfo)
        {
            pMac->dwSendFramingBits = pLinkInfo->SendFramingBits;
            memcpy(pNewLinkInfo, pLinkInfo, sizeof(*pNewLinkInfo));
            pLinkInfo->NdisLinkHandle = (NDIS_HANDLE)pMac->hCall;
            pNewLinkInfo->NdisLinkHandle = (NDIS_HANDLE)pMac->hCall;

            pppUnLock(pMac->session);
            PppNdisIssueRequest(&Status, 
                                pMac->pAdapter,
                                NdisRequestSetInformation,
                                OID_WAN_SET_LINK_INFO,
                                pNewLinkInfo,
                                sizeof(*pNewLinkInfo),
                                pMac,
                                PppNdisSetLinkInfoCompleteCallback,
                                pNewLinkInfo,
                                NULL);
            pppLock(pMac->session);

        }
    }

    DEBUGMSG (ZONE_FUNCTION, (TEXT("PPP: -pppMac_SetLink\n")));
}

PNDIS_WAN_INFO
pppMacGetWanInfo (macCntxt_t *pMac)
{
    DEBUGMSG (ZONE_FUNCTION, (TEXT("pppMacGetWanInfo(0x%X): About to return 0x%X (MaxFrame=%d)\r\n"),
                              pMac, &(pMac->WanInfo), pMac->WanInfo.MaxFrameSize));
    return &pMac->WanInfo;
}

void
pppMacIssueWanGetStatsInfoCompleteCallback(
    NDIS_REQUEST_BETTER    *pRequest,
    macCntxt_t            *pMac,
    NDIS_STATUS             Status)
{
    pMac->bMacStatsObtained = Status == NDIS_STATUS_SUCCESS;
}

void
pppMacIssueWanGetStatsInfo(
    macCntxt_t *pMac)
//
//  Send an asynchronous OID_WAN_GET_STATS_INFO request.
//  This is done just prior to closing a line in order to obtain
//  final stats data.
//
{
    NDIS_STATUS                    Status;

    if (pMac->hCall != (ULONG)INVALID_HANDLE_VALUE)
    {
        pMac->MacStats.NdisLinkHandle = (NDIS_HANDLE)pMac->hCall;

        PppNdisIssueRequest(&Status, 
                                pMac->pAdapter,
                                NdisRequestQueryInformation,
                                OID_WAN_GET_STATS_INFO,
                                &pMac->MacStats,
                                sizeof(pMac->MacStats),
                                pMac,
                                pppMacIssueWanGetStatsInfoCompleteCallback,
                                pMac,
                                NULL);
    }
}

NDIS_STATUS
pppMacGetWanStats(
    IN    PVOID                        context,
    OUT    PNDIS_WAN_GET_STATS_INFO    pStats)
{
    macCntxt_t *pMac = (macCntxt_t *)context;
    NDIS_STATUS    Status = NDIS_STATUS_SUCCESS;

    //
    // If the line is still open, retrieve the most
    // up to date statistics.
    //
    if (pMac->hCall != (ULONG)INVALID_HANDLE_VALUE)
    {
        pMac->MacStats.NdisLinkHandle = (NDIS_HANDLE)pMac->hCall;
        PppNdisDoSyncRequest(&Status,
                            pMac->pAdapter,
                            NdisRequestQueryInformation,
                            OID_WAN_GET_STATS_INFO,
                            &pMac->MacStats,
                            sizeof(pMac->MacStats));

        pMac->bMacStatsObtained = Status == NDIS_STATUS_SUCCESS;
    }
    else
    {
        // If MAC stats are supported, but the line has been closed, then
        // we can return the final stats that were retrieved just before
        // the line was closed.
        if (!pMac->bMacStatsObtained)
            Status = NDIS_STATUS_NOT_SUPPORTED;
    }

    *pStats = pMac->MacStats;

    return Status;
}
