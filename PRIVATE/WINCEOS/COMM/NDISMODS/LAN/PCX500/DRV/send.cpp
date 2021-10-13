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
//---------------------------------------------------------------------------
//  Send.cpp
//---------------------------------------------------------------------------
// Description:
//
// Revision History:
//
// Date         
//---------------------------------------------------------------------------
// 05/04/01     spb001      -Added "fast" initialization option.  By changing
//                           the InitFW() to only reset the device on the 
//                           first call.  InitFW1() must then be called to 
//                           finish the initialization. Subsequent calls to InitFW()
//                           will work as prior to changes.
//
// 06/12/01     spb010      -If we are using the fast init option, then 
//                           we don't want to init interrupts yet because firm
//                           ware will zero out the interrupt enable
//                           register during the reset.  This caused a race 
//                           condition where the card would be up and running
//                           but not interrupts were enabled.
//
//---------------------------------------------------------------------------

#pragma code_seg("LCODE")
#include "NDISVER.h"

extern "C"{
    #include    <ndis.h>
}

#include "aironet.h"
#include "queue.h"
#include "CardX500.h"
//
#if NDISVER == 5


VOID
cbSendPackets(
    NDIS_HANDLE     Context,
    PPNDIS_PACKET   PacketArray,
    UINT            NumberOfPackets
    )
{
    PCARD   card        = (PCARD)Context;
    UINT    count       = 0;

#if NDISVER == 5
    if (card->mp_enabled != MP_POWERED)
      return;
#endif

    //spb001 Finish Initialization if we haven't yet
    if (!card->initComplete) {
        InitFW1(card);
        InitInterrupts( card );     //spb010
    }

    NdisAcquireSpinLock(&card->m_lock);


    if( NULL == card->m_TxHead ){

        for( ; count<NumberOfPackets && DoNextSend(card, PacketArray[count] ); count++ )
            NDIS_SET_PACKET_STATUS(PacketArray[count], NDIS_STATUS_SUCCESS);
    
    }
    
    for( ;count<NumberOfPackets; count++ ){
        EnqueuePacket(card->m_TxHead, card->m_TxTail, PacketArray[count] );
        NDIS_SET_PACKET_STATUS(PacketArray[count], NDIS_STATUS_PENDING);
    }

//  while( DoNextSend(card ) );
    NdisReleaseSpinLock(&card->m_lock);
}


//BOOLEAN validateFid( USHORT & fid );
BOOLEAN
DoNextSend(
    PCARD   card,
    PNDIS_PACKET    Packet
    )
{
    USHORT  fid = (USHORT)CQGetNextND(card->fidQ);

    if( card->m_IsInDoNextSend      ||
        card->m_IsFlashing          ||
        0==fid                      ||
        !card->m_CardStarted        ||
        !card->m_PrevCmdDone )
        return FALSE;

    card->m_IsInDoNextSend = TRUE;

    if ( Packet ){
        card->m_Packet = Packet;
    }
    else
    if ( NULL == (card->m_Packet=card->m_TxHead) ){
        card->m_IsInDoNextSend = FALSE;
        return FALSE;
    }
    else
        NdisAcquireSpinLock(&card->m_lock);

    if( FALSE == CopyDownPacketPortUShort( card, card->m_Packet ) ){
        card->m_IsInDoNextSend = FALSE;

        if( NULL == Packet )
            NdisReleaseSpinLock(&card->m_lock);
        return FALSE;
    }
    if( NULL == Packet ){
    // CAME froM ISR then acke it
        DequeuePacket(card->m_TxHead, card->m_TxTail);

         //spb011 According to MicroSoft, we should never have a lock held
        //before we call NdisMSendComplete and we are a serialized driver
        NdisReleaseSpinLock(&card->m_lock);
        NdisMSendComplete(  card->m_MAHandle, card->m_Packet, NDIS_STATUS_SUCCESS );

    }
    card->m_IsInDoNextSend = FALSE;
    return TRUE;
}

void
AckPendingPackets(
    PCARD   card
    )
{
    PNDIS_PACKET    Packet;
    NdisAcquireSpinLock(&card->m_lock);
    while( Packet = (PNDIS_PACKET)card->m_TxHead) {
        DequeuePacket(card->m_TxHead, card->m_TxTail);
         //spb011 According to MicroSoft, we should never have a lock held
        //before we call NdisMSendComplete and we are a serialized driver
        NdisReleaseSpinLock(&card->m_lock);
        NdisMSendComplete(  card->m_MAHandle, Packet, NDIS_STATUS_SUCCESS );
        NdisAcquireSpinLock(&card->m_lock);
    }
    NdisReleaseSpinLock(&card->m_lock);
}

#endif