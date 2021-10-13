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
*   memory.c    NdisWan memory utilities
*
*   Date: 4/26/99
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

#ifdef DEBUG
BOOLEAN
NodeIsOnList(
    IN  PLIST_ENTRY     pListHead,
    IN  PLIST_ENTRY     pNode,
    IN  PNDIS_SPIN_LOCK SpinLock    OPTIONAL)
//
//  Return TRUE if pNode is on the list with head pListHead.
//  Return FALSE if pNode is not on the list.
//
{
    PLIST_ENTRY pEntry;
    BOOLEAN     Found = FALSE;

    ASSERT(pListHead != NULL);
    ASSERT(pNode != NULL);

    // The list head can never be a "node" on a list. While the list head
    // has the same Flink and Blink fields that are also in a node, the list
    // head is otherwise unique and special and does not qualify as a node.
    ASSERT(pListHead != pNode);
    if (pListHead == pNode)
        goto done;

    if (SpinLock)
        NdisAcquireSpinLock(SpinLock);

    for (pEntry = pListHead->Flink; pEntry != pListHead; pEntry = pEntry->Flink)
    {
        if (pEntry == pNode)
        {
            Found = TRUE;
            break;
        }
    }

    if (SpinLock)
        NdisReleaseSpinLock(SpinLock);

done:
    return Found;
}

#define MAX_NODES   1000000
BOOLEAN
ListIsValid(
    IN  PLIST_ENTRY     pListHead,
    IN  PNDIS_SPIN_LOCK SpinLock    OPTIONAL)
{
    PLIST_ENTRY pPred, pCurr;
    DWORD       dwNumNodes = 0;
    BOOLEAN     bValid = TRUE;

    if (SpinLock)
        NdisAcquireSpinLock(SpinLock);

    pPred = pListHead;
    while (TRUE)
    {
        pCurr = pPred->Flink;
        if (pCurr->Blink != pPred)
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR List %x Node %x Blink %x != pPred %x\n"), pListHead, pCurr, pCurr->Blink, pPred));
            bValid = FALSE;
            break;
        }

        if (pCurr == pListHead)
            break;

        if (dwNumNodes++ > MAX_NODES)
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR List %x has more than %d nodes\n"), pListHead, MAX_NODES));
            bValid = FALSE;
            break;
        }

        pPred = pCurr;
    }

    if (SpinLock)
        NdisReleaseSpinLock(SpinLock);

    return bValid;
}
#endif
/*****************************************************************************
*
*
*   @func   BOOL | NdisWanAllocateSendResources
*       Allocate the required send resources
*
*   @rdesc  TRUE if successful, FALSE for error.
*
*   @parm   PMAC_CONTEXT    |   pMac    | Pointer to the Mac Context
*
*   @comm
*           This function will allocate the NDIS_WAN_PACKETs required for a
*           connection
*
*   @ex     No Example
*
*/
NDIS_STATUS
NdisWanAllocateSendResources(
    PMAC_CONTEXT    pMac)
{
    DWORD   dwBufferSize;
    DWORD   cPackets, n;
    DWORD   dwPacketMemorySize;
    PUCHAR  PacketMemory = NULL;
    PNDIS_WAN_PACKET    pWanPacket;

    //
    //  Sanity check the miniport provided configuration values.
    //
    if (pMac->WanInfo.MaxFrameSize  > 0x20000
    ||  pMac->WanInfo.HeaderPadding > 0x10000
    ||  pMac->WanInfo.TailPadding   > 0x10000)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR- NdisWanAllocateSendResources: Miniport wants packets that are too big\n")));
        return NDIS_STATUS_RESOURCES;
    }

    if (pMac->WanInfo.MaxTransmit  > 500)
    {
        DEBUGMSG(ZONE_WARN, (L"PPP: WARNING- Miniport wants %u transmit buffers, capping at 500\n", pMac->WanInfo.MaxTransmit));
        pMac->WanInfo.MaxTransmit = 500;
    }

    // Allocate enough for the send window plus a spare for compression space
    cPackets = pMac->WanInfo.MaxTransmit + 1;

    // The compression code can actually wind up expanding the data size
    // (though when the frame expands the original is sent, not the expanded one).
    // The expansion can be up to 12.5% (1 extra bit per byte), so we reserve
    // 12.5% of the max frame size bytes for this possible expansion.
    //
    // The 8 bytes cover the 5 bytes left out at the bottom as well as whatever
    // bytes are lost to get DWORD alignment.
    //
    dwBufferSize = (pMac->WanInfo.MaxFrameSize * 9 + 7) / 8 +
                   pMac->WanInfo.HeaderPadding +
                   pMac->WanInfo.TailPadding +
                   8 + 
                   sizeof(PVOID);

    // Make size DWORD aligned
    dwBufferSize &= ~(sizeof(PVOID) - 1);

    dwPacketMemorySize = (dwBufferSize + sizeof(NDIS_WAN_PACKET)) * cPackets;

    RETAILMSG( 1,
               ( TEXT( "PPP: TX PktPoolSize: %u, MaxFrameSize: %u\r\n" ),
               cPackets,  dwBufferSize ) );

    PacketMemory = pppAllocateMemory(dwPacketMemorySize);

    if (NULL == PacketMemory)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("PPP: NdisWanAllocateSendResources: Allocation failed\r\n")));
        return NDIS_STATUS_RESOURCES;
    }
    

    pMac->PacketMemory = PacketMemory;
    pMac->PacketMemorySize = dwPacketMemorySize;
    pMac->BufferSize = dwBufferSize;
    NdisInitializeListHead (&pMac->PacketQueue);

    // Now setup all of the WAN_PACKET's
    PREFAST_SUPPRESS(12009, "dwPacketMemorySize cannot overflow 32-bits thanks to checks on inputs, but prefast still complains?");
    for (n=0; n < cPackets; n++) {
        pWanPacket = (PNDIS_WAN_PACKET)PacketMemory;
        PacketMemory += sizeof(NDIS_WAN_PACKET);

        pWanPacket->StartBuffer = PacketMemory;
        PacketMemory += dwBufferSize;

        // Leave a little extra at the end.
        pWanPacket->EndBuffer = PacketMemory - 5;

        // Add it to the list.
        NdisInterlockedInsertTailList (&pMac->PacketQueue, (PLIST_ENTRY)pWanPacket,
                                       &pMac->PacketLock);
        DEBUGMSG( ZONE_MAC,
                   ( TEXT( "PPP: (%hs) - New TX PKT[%u]: 0x%08X\r\n"),
                    __FUNCTION__,
                    n,
                    pWanPacket ) );
    }

    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
NdisWanAllocatePacket (
    IN  OUT PMAC_CONTEXT         pMac,
        OUT PNDIS_WAN_PACKET    *ppPacket)
//
//  Get a free packet from the PacketQueue.
//
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    ASSERT(ListIsValid(&pMac->PacketQueue, &pMac->PacketLock));
    *ppPacket = (PNDIS_WAN_PACKET)NdisInterlockedRemoveHeadList(&pMac->PacketQueue, &pMac->PacketLock);
    ASSERT(ListIsValid(&pMac->PacketQueue, &pMac->PacketLock));

    if (NULL == *ppPacket)
    {
        // If there are no transmit buffers left in the buffer pool maintained
        // by PPP, the a deadlock can occur if the thread that is responsible
        // for returning buffers to the pool blocks here.
        //
        // Currently this is known to happen when tcpstk "steals" the PPTP PacketWorkingThread
        // to send some unrelated packets which are destined for the PPTP adapter.
        //
        // So, we don't want to take a chance blocking sending to the PPTP adapter.
        //
        DEBUGMSG(ZONE_ERROR, (TEXT("PPP: All Xmit Packets in use\n")));
        Status = NDIS_STATUS_RESOURCES;
    }

    return Status;
}

NDIS_STATUS
NdisWanFreePacket (PMAC_CONTEXT pMac, PNDIS_WAN_PACKET pPacket)
//
//  Return pPacket to the list of free packets.
//
{
    ASSERT(ListIsValid(&pMac->PacketQueue, &pMac->PacketLock));
    // Add it to the list.
#ifdef DEBUG
    // Some WAN Miniport drivers accidentally try to free send packets twice,
    // once by returning NDIS_STATUS_SUCCESS from the SendHandler and then a
    // second time by calling NdisSendComplete (which should only be called
    // when the Miniport returned NDIS_STATUS_PENDING from the SendHandler.
    //
    // This code detects that error and logs a debug message.
    if (NodeIsOnList(&pMac->PacketQueue, (PLIST_ENTRY)pPacket, &pMac->PacketLock))
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR - NdisWanFreePacket pPacket %x when it is already on free list, Miniport freeing packet twice?\n"), pPacket));
        return NDIS_STATUS_FAILURE;
    }
#endif
    NdisInterlockedInsertTailList(&pMac->PacketQueue, (PLIST_ENTRY)pPacket, &pMac->PacketLock);
    ASSERT(ListIsValid(&pMac->PacketQueue, &pMac->PacketLock));
    return NDIS_STATUS_SUCCESS;
}


/*****************************************************************************
*
*
*   @func   PNDIS_WAN_PACKET | pppMac_GetPacket
*       Get a packet to use internally
*
*   @rdesc  Pointer to the packet or NULL if error
*
*   @parm   PMAC_CONTEXT    |   pMac    | Pointer to the Mac Context
*
*   @comm
*           This function will allocate the NDIS_WAN_PACKET's required for a
*           connection
*
*   @ex     No Example
*
*/
PNDIS_WAN_PACKET 
pppMac_GetPacket (PMAC_CONTEXT  pMac)
{
    PNDIS_WAN_PACKET    pPacket;
    NDIS_STATUS         Status;

    PPP_LOG_FN_ENTER();

    Status = NdisWanAllocatePacket(pMac, &pPacket);
    if (NDIS_STATUS_SUCCESS != Status)
    {
        DEBUGMSG( ZONE_ERROR,
                  ( TEXT( "PPP: - (%hs) - * * * WARNING * * *" )
                    TEXT( "PKT ALLOC FAILED. STS: 0x%08X\r\n" ),
                    __FUNCTION__,
                    Status ) );
        return NULL;
    }

    // Initialize the packet.
    pPacket->CurrentBuffer = pPacket->StartBuffer + pMac->WanInfo.HeaderPadding;
    pPacket->CurrentLength = pPacket->EndBuffer - pPacket->CurrentBuffer -
                             pMac->WanInfo.TailPadding;

#if 0
    DEBUGMSG (ZONE_MAC, (TEXT("pppMac_GetPacket: Packet=0x%08X Start=0x%X End=0x%X(%d) ")
                  TEXT("CBuf=0x%X CLen=%d HPad=%d TPad=%d\r\n"),
                  pPacket,
                  pPacket->StartBuffer, pPacket->EndBuffer,
                  pPacket->EndBuffer - pPacket->StartBuffer,
                  pPacket->CurrentBuffer, pPacket->CurrentLength,
                  pMac->WanInfo.HeaderPadding, pMac->WanInfo.TailPadding));
#endif
    
    pPacket->ProtocolReserved1 = pMac;
    
    pPacket->ProtocolReserved2 = pPacket->ProtocolReserved3 = pPacket->ProtocolReserved4 = NULL;

    PPP_LOG_FN_LEAVE( 0 );

    return pPacket;     
}

