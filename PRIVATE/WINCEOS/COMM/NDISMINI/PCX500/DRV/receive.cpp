//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// Copyright 2001, Cisco Systems, Inc.  All rights reserved.
// No part of this source, or the resulting binary files, may be reproduced,
// transmitted or redistributed in any form or by any means, electronic or
// mechanical, for any purpose, without the express written permission of Cisco.
//
// Recive.cpp
//---------------------------------------------------------------------------
// Description:
//
// Revision History:
//
// Date         
//---------------------------------------------------------------------------
// 05/30/01     spb007      Failed to Release Spin Lock which will cause 
//                          lock ups.
//
//---------------------------------------------------------------------------
#pragma code_seg("LCODE")
#include "NDISVER.h"

extern "C"{
    #include    <ndis.h>
}

#if NDISVER == 5
#include "Cardx500.h"
#include "queue.h"
#include "receive.h"

void
RcvDpc(
    PCARD   card
    )
{
    NdisAcquireSpinLock(&card->m_lock);

    PX500RFD        SwRfd       = (PX500RFD)QueueGetHead(&card->RfdList); 

    if (NULL == SwRfd ) {
        NdisReleaseSpinLock(&card->m_lock);
        return;
    } 

    PUCHAR          rcvbuf      = (PUCHAR)&SwRfd->Rfd->RxMacHeader;
    PNDIS_PACKET    PacketArray = SwRfd->ReceivePacket;
    
    card->m_PacketRxLen = GetRxHeader(card, rcvbuf );

    if( 0 == card->m_PacketRxLen ){
        AckRcvInterrupt(card);
        NdisReleaseSpinLock(&card->m_lock);        //spb007
        return;
    }

    if ((card->m_PacketFilter == 0) || 
        (!card->m_PromiscuousMode && !IsMyPacket(card, rcvbuf) && !CheckMulticast(card, rcvbuf))) {
        AckRcvInterrupt(card);
        NdisReleaseSpinLock(&card->m_lock);
        return;
    }

    QueueRemoveHead(&card->RfdList);
    
    SwRfd->FrameLength = card->m_PacketRxLen+AIRONET_HEADER_SIZE;
    CopyUpPacketUShort( card, (PUCHAR)&SwRfd->Rfd->RxBufferData, card->m_PacketRxLen );
    AckRcvInterrupt(card);
    NdisAdjustBufferLength((PNDIS_BUFFER) SwRfd->ReceiveBuffer, (UINT) SwRfd->FrameLength);
    
    NDIS_SET_PACKET_STATUS(PacketArray, NDIS_STATUS_SUCCESS );
    NdisReleaseSpinLock(&card->m_lock);
    NdisMIndicateReceivePacket(card->m_MAHandle, &PacketArray, 1 );
    NdisAcquireSpinLock(&card->m_lock);

    NDIS_STATUS ReturnStatus = NDIS_GET_PACKET_STATUS(PacketArray);
    if (ReturnStatus != NDIS_STATUS_PENDING){
        SwRfd = *(X500RFD **)(PacketArray->MiniportReserved);
        QueuePutTail(&card->RfdList, &SwRfd->Link);
    }
    
    NdisReleaseSpinLock(&card->m_lock);
}  

//-----------------------------------------------------------------------------
// cbReturnPacket
//
// PARAMETERS: IN NDIS_HANDLE MiniportcardContext
//                 - a context version of our card pointer
//             IN NDIS_PACKET Packet
//                 - the packet that is being freed
//
// DESCRIPTION: This function attempts to return to the receive free list the
//              packet passed to us by NDIS
//
// RETURNS: nothing
//
//-----------------------------------------------------------------------------

VOID
cbReturnPacket(
    NDIS_HANDLE  MiniportAdapterContext,
    PNDIS_PACKET Packet
    )
{
    PCARD           card    = (PCARD)MiniportAdapterContext;
    PX500RFD        SwRfd;

    NdisAcquireSpinLock(&card->m_lock);

    SwRfd = *(X500RFD **)(Packet->MiniportReserved);

    NDIS_SET_PACKET_STATUS(Packet, NDIS_STATUS_SUCCESS );

   QueuePutTail(&card->RfdList, &SwRfd->Link);

    --card->UsedRfdCount;

    NdisReleaseSpinLock(&card->m_lock);
}

#define MAX_RX_DESCR                1024 // 0x400
#define NUM_BYT_PROT_RSRVD_SEC      16

BOOLEAN
SetupReceiveQueues(
     IN PCARD card
    )
{
    // init the lists
    QueueInitList(&card->RfdList);

    NDIS_STATUS AllocStatus;
    NdisAllocatePacketPool(&AllocStatus, &card->RxPktPool, MAX_RX_DESCR, NUM_BYT_PROT_RSRVD_SEC);
    NdisAllocateBufferPool(&AllocStatus, &card->RxBufPool, MAX_RX_DESCR);

    card->NumRfd        = NUMRFD;
    card->m_pSwRfd      = NdisMalloc( sizeof(X500RFD) * card->NumRfd );
    X500RFD     *SwRfd  = (X500RFD *)card->m_pSwRfd;

    NdisZeroMemory( SwRfd, sizeof(X500RFD) * card->NumRfd );

    for (UINT i = 0; i < card->NumRfd; i++, SwRfd++ ){

        SwRfd->Rfd  = (RFD_STRUC *)card->m_RcvBuf[i];
        
        NdisAllocatePacket(&AllocStatus, &SwRfd->ReceivePacket, card->RxPktPool);
        if( NDIS_STATUS_SUCCESS != AllocStatus){
            SwRfd->ReceivePacket = NULL;
            return FALSE;
        }

        NDIS_SET_PACKET_HEADER_SIZE(SwRfd->ReceivePacket, ETHERNET_HEADER_SIZE);
        
        // point our buffer for receives at this Rfd
        NdisAllocateBuffer(&AllocStatus,
                &SwRfd->ReceiveBuffer,
                card->RxBufPool,
                &SwRfd->Rfd->RxMacHeader,
                MAXIMUM_ETHERNET_PACKET_SIZE);

        if( NDIS_STATUS_SUCCESS != AllocStatus){
            SwRfd->ReceiveBuffer = NULL;
            return FALSE;
        }
        // probably should do some error handling here
        NdisChainBufferAtFront(SwRfd->ReceivePacket, SwRfd->ReceiveBuffer);
        NDIS_SET_PACKET_STATUS(SwRfd->ReceivePacket, NDIS_STATUS_SUCCESS );

        X500RFD     **TempPtr = (PX500RFD *) &SwRfd->ReceivePacket->MiniportReserved;
        *TempPtr    = SwRfd;
        QueuePutTail(&card->RfdList, &SwRfd->Link);
    }

    return TRUE;
}


VOID
FreeReceiveQueues(
     IN PCARD card
    )
{
    X500RFD *SwRfd      = (X500RFD *)card->m_pSwRfd;

    for( UINT i=0; SwRfd && i<card->NumRfd; i++, SwRfd++ ){
        
        if( SwRfd->ReceivePacket ){
        
            if( SwRfd->ReceiveBuffer )
                NdisFreeBuffer(SwRfd->ReceiveBuffer);

            NdisFreePacket(SwRfd->ReceivePacket);
        }
    }

    NdisAcquireSpinLock(&card->m_lock);
    
    if( card->m_pSwRfd )
        NdisFreeMemory(card->m_pSwRfd, sizeof(X500RFD)* card->NumRfd, 0 );
    
    NdisReleaseSpinLock(&card->m_lock);
    while(0!=NdisPacketPoolUsage(card->RxPktPool) )
        NdisMSleep(10);

    if( card->RxBufPool )
        NdisFreeBufferPool(card->RxBufPool);
    if( card->RxPktPool )
        NdisFreePacketPool(card->RxPktPool);
}

#endif