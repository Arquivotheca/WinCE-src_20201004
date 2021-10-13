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
// cardx500.cpp
//---------------------------------------------------------------------------
// Description:
//
// Revision History:
//
// Date        
//---------------------------------------------------------------------------
// 09/22/00    jbeaujon     -added BOOLEAN parameter to InitFW() to indicate
//                           that we want to use the current config info.
//                           The default value of this param is FALSE.
//
// 10/02/00     jbeaujon    -fixed throughput (transmit) problem by ensuring that 
//                           DoNextSend() is invoked whenever cbHandleInterrupt() 
//                           is invoked.
//                          -reformatted code.  
// 
// 10/11/00     jbeaujon    -changed cardname for product numbers 0x07, 0x08 from 
//                           "PC4800" to "340 Series".  Changed cardname for prodnum
//                           "0x0A" from "PC4800" to "350 Series".
// 
// 10/30/00     jbeaujon    -auto config stuff.
//
// 03/20/01     jraf        -softex interrupt problem fix, isr and InitInterrupts modified
//
// 05/04/01     spb001      -Added "fast" initialization option.  By changing
//                           the InitFW() to only reset the device on the 
//                           first call.  InitFW1() must then be called to 
//                           finish the initialization. Subsequent calls to InitFW()
//                           will work as prior to changes.
//
// 5/10/01      spb002      -Fix for interrupt problem.
//
// 5/8/01       spb003      -Fix for slow ftp throughput problem for NCSU
//                           Took 6.21 cbHandleInterrupt routine and tweaked
//                           it (Reversed processing of Rx and Tx interrupts)
//
// 05/10/01     spb004      -Work around for network card not sending an updatelinkstatus
//                           interrupt when it should
//
// 06/05/01     spb009      -Removed part of spb004 in UpdateLinkStatus because
//                           we now send a disconnect status when we disable
//                           the card.  Now UpdateLinkStatus only has to worry
//                           about the time when the card is enabled.
//
// 07/10/01     spb018      -MiniportHalt was illegally calling NdisMIndicateStatus
//
// 07/18/01     spb023      -Added firmware flashing during init for Windows XP
//
// 07/23/01     spb026      -Changed firmware flashing during init so that it is
//                           a blocking function.  Broke WZC.
//
// 0801/01      spb027      -Added check to make sure cmdStatus was successful
//
// 08/13/01     raf         -Magic Packet fixes
//---------------------------------------------------------------------------
#pragma code_seg("LCODE")
#include "NDISVER.h"

#ifndef UNDER_CE
extern "C"{
    #include    <ndis.h>
    #include <VXDWRAPS.H>
}
#else
extern "C"{
    #include    <ndis.h>    
}
#endif


#include "string.h"
//
#include "HWX500.h"
#include "CardX500.h"
#include <memory.h>
#include <AiroVxdWrap.H>
#include <AiroPCI.h>

#if NDISVER == 5
#   include "queue.h"
#endif


char *  VxdLoadName     = "VXDX500.VXD";
char *  VxdUnloadName   = "VXDX500";
ULONG fidCounter;

USHORT  FidArray[ 3 ];

BOOLEAN validateFid( USHORT & fid )
{
    static index = 0;
    int size = sizeof(FidArray)/sizeof(FidArray[0]);

    for( int i=0; i<size; i++ ){
        if( fid == FidArray[ i ] )
            return TRUE;
    }
    fid = FidArray[ index++ ];
    return FALSE;
}

//spb027 BOOLEAN IsCardInserted( PCARD card );


typedef struct
{
    USHORT      SelectedRate;
    UCHAR       SupRates[8];
    UINT        LinkSpeed;
    char        *CardName;
}STSUPRATES;

static STSUPRATES SUPPORTED_RATES[] =
{
    {1,     {1},        1*10000/2},                 
    {2,     {2},        1*10000},   
    {4,     {4},        2*10000},
    {11,    {11},       11*10000/2},        
    {22,    {22},       11*10000},
    {0,     {2,4},      2*10000,    "PC2500"},      
    {0,     {2,4},      2*10000,    "PC3500"},      
    {0,     {2,4},      2*10000,    "PC4500"},      
    {0,     {2,4,11,22},11*10000,   "PC4800"},      
    {0xFFFF}    
};  

typedef struct _RADIOTYPE
{
    USHORT      ProductNum;         // FROM capabilities STRUCT;
    //  
    char        *CardName;          //"PC3500";     
    USHORT      u16RadioType;       // FROM capabilities STRUCT;
    UINT        FrequencyType;      //1=HOPPER, 0 = DS      
    UINT        CardType;           //VXD_CARDTYPE_PCXX00;      
    USHORT      Pci_Dev_Id;         //PC3500_PCI_DEV_ID;
}RADIOTYPE;

static RADIOTYPE RadioTypesArray[] =
{
    {0x06,  "PC2500",       0x0004, 0, VXD_CARDTYPE_PC2500, PC2500_PCI_DEV_ID},
    {0x09,  "PC3100",       0x0001, 1, VXD_CARDTYPE_PC3100, PC3100_PCI_DEV_ID},
    {0x04,  "PC3500",       0x0001, 1, VXD_CARDTYPE_PC3500, PC3500_PCI_DEV_ID},
    {0x05,  "PC4500",       0x0002, 0, VXD_CARDTYPE_PC4500, PC4500_PCI_DEV_ID},
    {0x07,  "340 Series",   0x0002, 0, VXD_CARDTYPE_PC4800, PC4800_PCI_DEV_ID},
    {0x08,  "340 Series",   0x0002, 0, VXD_CARDTYPE_PC4800, PC4800_PCI_DEV_ID},
    {0x0A,  "350 series",   0x0002, 0, VXD_CARDTYPE_PC4800, PC4800_PCI_DEV_ID},
    {0,0},
};
    
BOOLEAN cardConstruct(PCARD card)
{
//  card->m_CardType                = CardType; 
//  card->m_FrequencyType           = FrequencyType;
    strcpy( card->m_CardName,  CardName );       
    strcpy( card->m_DriverName, DriverName);        

    card->m_PortIOLen               = 64; // 30 words
    card->m_IsIO                    = TRUE;
    card->m_IsIO16                  = TRUE; 
    return baseCardConstruct(card);
}

void CardShutdown(PCARD card)
{
    disableHostInterrupts(card);

//  USHORT  usWord;
//  NdisRawReadPortUshort( card->m_IOBase+REG_INT_STAT, &usWord );
//  NdisRawWritePortUshort( card->m_IOBase+REG_INT_ACK, usWord );
    
    if( ! card->m_CardPresent )
            return;
    cmdAwaken(card, TRUE );

#if NDISVER == 3 || NDISVER == 41
    if( 0xFFFF != (0xFFFF & card->m_MagicPacketMode)) {
        cmdMagicPacket( card, TRUE );
    }
    else {
        //Don't send link down during shutdown.
        card->m_MSenceStatus = NDIS_STATUS_MEDIA_DISCONNECT;
        cmdDisable(card);       
    }
#else
    //Don't send link down during shutdown.
    card->m_MSenceStatus = NDIS_STATUS_MEDIA_DISCONNECT;
    cmdDisable(card);       
#endif

}

void cardDestruct(PCARD card)
{
    CardShutdown(card);
    baseCardDestruct(card);
}

#define show(a,b)   a=b
extern UINT DebugFlag;

//===========================================================================
    void cbIsr (OUT PBOOLEAN    InterruptRecognized,
                OUT PBOOLEAN    QueueDpc,
                IN NDIS_HANDLE  Context)
//===========================================================================
// 
// Description: Hardware ISR (runs at DIRQL)
//    
//      Inputs: InterruptRecognized
//              QueueDpc
//              Context
//    
//     Returns: nothing.
//---------------------------------------------------------------------------
{
    PCARD   card  = (PCARD)Context;
    USHORT  usWord;
    // 
    // This fixes PCI shared interrupt problem.
    // 
    // (11/08/00 - jbeaujon) Only do this for bus type of PCI
    // 

    //spb002

    if (0==GetMaskInterrupts(card)) {
        *InterruptRecognized    = FALSE;
        *QueueDpc               = FALSE;
        return;
        }


#ifdef SOFTEX
    if (!ADAPTER_READY(card)) {
        *InterruptRecognized    = FALSE;
        *QueueDpc               = FALSE;
        return;
        }

    if (!card->m_InterruptRegistered ) { // should never happen.....but
        *InterruptRecognized    = FALSE;
        *QueueDpc               = FALSE;
//spb024        card->m_IntActive       = 0;
        return;
        }
#endif

    //StoreHostActiveInterrupts(card);
    NdisRawReadPortUshort(card->m_IOBase+REG_INT_STAT, &usWord);

//	DbgPrint("cbIsr int %X, mask %X \n",usWord, card->m_IntMask);

    if ((usWord & 0x7FFF) && (0xFFFF != usWord)) {

        if ((INT_EN_AWAKE == usWord) && card->IsAwake) {
            *InterruptRecognized    = FALSE;
            *QueueDpc               = FALSE;
            return;
            }
    
        //You must always disable interrupts before storing status in m_IntActive
        //There are other routines (cmds) that disable interrupts and rely
        //on the m_IntActive byte and  interrupts being disabled
        disableHostInterrupts(card);
        card->m_IntActive           = usWord;
        card->m_InterruptHandled    = TRUE;
        *InterruptRecognized        = TRUE;
        *QueueDpc                   = TRUE;
        }
    else {
        *InterruptRecognized    = FALSE;
        *QueueDpc               = FALSE;
        }
}

/*
//===========================================================================
    void cbIsr (OUT PBOOLEAN    InterruptRecognized,
                OUT PBOOLEAN    QueueDpc,
                IN NDIS_HANDLE  Context)
//===========================================================================
// 
// Description: Hardware ISR (runs at DIRQL)
//    
//      Inputs: InterruptRecognized
//              QueueDpc
//              Context
//    
//     Returns: nothing.
//---------------------------------------------------------------------------
{
    PCARD   card  = (PCARD)Context;
    // 
    // This fixes PCI shared interrupt problem.
    // 
    // (11/08/00 - jbeaujon) Only do this for bus type of PCI
    // 
    if (card->m_IntActive && (card->m_BusType == NdisInterfacePci)) {
        *InterruptRecognized    = FALSE;
        *QueueDpc               = FALSE;
        card->m_IntActive       = 0;
        return;
        }

#ifdef SOFTEX
    if (!ADAPTER_READY(card)) {
        *InterruptRecognized    = FALSE;
        *QueueDpc               = FALSE;
        return;
        }
#endif
    //StoreHostActiveInterrupts(card);
    USHORT  usWord;
    NdisRawReadPortUshort(card->m_IOBase+REG_INT_STAT, &usWord);

    if ((usWord & 0x7FFF) && (0xFFFF != usWord)) {

        if ((INT_EN_AWAKE == usWord) && card->IsAwake) {
            *InterruptRecognized    = FALSE;
            *QueueDpc               = FALSE;
            return;
            }
    
        disableHostInterrupts(card);
        card->m_IntActive           = usWord;
        card->m_InterruptHandled    = TRUE;
        *InterruptRecognized        = TRUE;
        *QueueDpc                   = TRUE;
        }
    else {
        *InterruptRecognized    = FALSE;
        *QueueDpc               = FALSE;
        }
}
*/

//spb003
void
cbHandleInterrupt(
    PCARD card
    )
{
    // FOR PCMCIA handleing
    // StoreHostActiveInterrupts must be called also here 
    // to handle some machines controllers
#ifdef SOFTEX   
    if(!ADAPTER_READY( card ))
        return;
#endif
        
//	DbgPrint("cbHandleInterrupt\n");

    if( FALSE==card->m_InterruptHandled ){
        StoreHostActiveInterrupts(card);
        disableHostInterrupts(card);
    }

    card->m_InterruptHandled = FALSE;

#ifndef UNDER_CE
    USHORT ints;
#endif

    do {
        if( isAwakeInterrupt(card ) ){
            AckAwakeInterrupt(card);
            card->IsAwake = TRUE;
            //spb003 DoNextSend(card);
        }

        if( isAsleepInterrupt(card ) ){
            AckAsleepInterrupt(card);
        }

        if(  isCmdInterrupt(card) )
            AckCmdInterrupt(card);

        while( DoNextSend(card));        
        
        if( isTxInterrupt(card) ){
            USHORT  fid;                                    // reclaim fids
            //NdisRawReadPortUshort( card->m_IOBase+REG_FID_TX_COMP, &fid );
            NdisRawReadPortUshort( card->m_IOBase+REG_FID_TX_COMP, &fid );
//            if (0 == fid ) {
//                while (1)
//                    DbgPrint("Fid is 0  ................\n");
//            }
            fidCounter++;
            CQStore( card->fidQ, fid );
            AckTxInterrupt(card);
            while( DoNextSend(card));
        }
        
        if( isTxExceptInterrupt(card) ){
            USHORT  fid;                                            // reclaim fids
            NdisRawReadPortUshort( card->m_IOBase+REG_FID_TX_COMP, &fid );
//            if (0 == fid ) {
//                while (1)
//                    DbgPrint("Fid is 0  ................\n");
//            }
            fidCounter++;
            //validateFid(fid);
            CQStore( card->fidQ, fid );
            AckTxExceptInterrupt(card);
            while( DoNextSend(card));
        }

        if( isRxInterrupt(card) ){
            if( RxFidSetup(card) )
                RcvDpc(card);
        }

        if( isLinkInterrupt(card) ){
            UpdateLinkStatus(card);
            CheckAutoConfig(card);
            AckLinkInterrupt( card );
        }

        // ACK remaining ints
        if(card->m_IntActive ) {
            NdisRawWritePortUshort(card->m_IOBase+REG_INT_ACK, card->m_IntActive & ~EVNT_DO_SLEEP);
            //Only reset m_IntActive in the defered handler routine
            card->m_IntActive = 0;  //spb024
//spb005            NdisRawWritePortUshort( card->m_IOBase+REG_INT_ACK, card->m_IntActive );
        }

//spb024        card->m_IntActive = 0;
        //
        //StoreHostActiveInterrupts(card);
        //card->m_IntActive &= ~(EVNT_DO_SLEEP | EVNT_DO_WAKEUP);

    } while(0 && card->m_IntActive && card->m_IntActive != ~(EVNT_DO_SLEEP | EVNT_DO_WAKEUP) );

    if( card->m_MaxPSP && 0==card->KeepAwake )
        cmdSleep(card);

    EnableHostInterrupts(card);     // enable
}

//------------------------------------------------------------------
void UpdateLinkSpeed (PCARD card)
{
    // 
    // Added this to stop "jerkiness" when in MaxPSP mode (jmb 11/28/00)
    // 
    if( ! card->IsAwake )
        return;

    STSTATUS    status;
    if( ! cmdStatusGet(card, &status ) )
        return;                         
//    ULONG   LinkSpeed = status.LinkSpeed & 0xFF;
    // 
    // field name changed from status.LinkSpeed to status.u16CurrentTxRate
    // 
    ULONG   LinkSpeed = status.u16CurrentTxRate & 0xFF;
    card->m_LinkSpeed   = LinkSpeed * 10000 / 2;
}   

//===========================================================================
    void CheckAutoConfig (PCARD card)
//===========================================================================
// 
// Description: Start the auto config damper tick if we're not associated.
//    
//      Inputs: card    - pointer to card structure.
//    
//     Returns: nothing.
//    
//      (10/30/00)
//---------------------------------------------------------------------------
{
    // 
    // See if we're associated.
    // 
    STSTATUS    st;
    BOOLEAN     ret         = cmdStatusGet(card, &st);
    BOOLEAN     associated  = 0x20 & st.u16OperationalMode;
    if (!associated                 && 
        card->m_autoConfigEnabled   && 
        (card->m_numProfiles > 1)) {
        setAutoConfigTimer(card, TRUE);
        }
}

//===========================================================================
    void UpdateLinkStatus (PCARD card)
//===========================================================================
// 
// Description: Updates the Link status whenever a link event interrupt is
//              received.
//    
//      Inputs:  PCARD - pointer to the card structure
//
//     Caveats: Should only be called from within the Interrupt Handler.
//    
//     Returns: None
//
//---------------------------------------------------------------------------
{
    STSTATUS    st;
    BOOLEAN     IsAssociated;
    USHORT      usWord;

    //Use link event register to check for association.
    NdisRawReadPortUshort(card->m_IOBase+REG_LINKSTATUS, &usWord );

    IsAssociated = (0x8000 != (usWord & 0x8000));

    //If we are associated, then we need to get the status for the BSSID
    if (IsAssociated) {
        cmdStatusGet(card, &st);
    }


    #ifdef DEBUG_AUTO_CONFIG
    DbgPrint("Link Status=%x, Operational Mode=%x\n",usWord,st.u16OperationalMode & 0x20);
    #endif

    BOOLEAN  oldAssociation = card->IsAssociated;
    card->IsAssociated  = IsAssociated;

    //  Indicate Link event if: 
    //  a- We associated and we previously indicated to NDIS that we were disconnected
    //  b- We associated to a new BSSID
    //  c- We are disassocaited and the m_MediaDisconnectDamper has expired

    BOOLEAN doIndicate = FALSE;

    if (card->IsAssociated) {
        #ifdef DEBUG_AUTO_CONFIG
        DbgPrint("UpdateLinkStatus: Associated\n");
        #endif

        // Just became associated.  Because of damper, we need to compare current BSSID
        // with the previous BSSID. Indicate connect status if different.  This causes
        // DHCP to renew.
        UINT    result;
        ETH_COMPARE_NETWORK_ADDRESSES_EQ( st.au8CurrentBssid, card->m_BSSID, &result);
        doIndicate = (0 != result) || (NDIS_STATUS_MEDIA_DISCONNECT == card->m_MSenceStatus);

        //Save new BSSID
        //Note:  Per Doug,if we ever get a FF:FF:FF:FF for the BSSID, then there
        //is something wrong in the NIC.  The only time this is possible is after
        //we have disabled/enabled the NIC. Once associated, the status will always
        //indicate the last BSSID the NIC associated to.  Therefore it will never
        //be FFFFFFFFF.        
        NdisMoveMemory(card->m_BSSID, st.au8CurrentBssid, sizeof(MacAddr)); 
 
        card->m_SSID.SsidLength = (st.SSIDlength < 32)?st.SSIDlength:32;
        NdisMoveMemory(card->m_SSID.Ssid,st.SSID,card->m_SSID.SsidLength);
    }
    else {
        //Protect from 2 back to back media disconnects happening (they do happen)
        if (card->IsAssociated != oldAssociation) {
            // We're no longer associated.  Start media disconnect damper tick or send event.
            doIndicate = (0 == card->m_MediaDisconnectDamper);
            card->m_MediaDisconnectDamperTick = card->m_MediaDisconnectDamper;
        }

        #ifdef DEBUG_AUTO_CONFIG
        DbgPrint("UpdateLinkStatus: NOT Associated\n");
        #endif
    }

    // Indicate status based on current association state.
    if (doIndicate) {
        card->m_MSenceStatus = card->IsAssociated ? NDIS_STATUS_MEDIA_CONNECT : NDIS_STATUS_MEDIA_DISCONNECT; 
        NdisMIndicateStatus(card->m_MAHandle, card->m_MSenceStatus, 0, 0);
        NdisMIndicateStatusComplete(card->m_MAHandle);

        #ifdef DEBUG_AUTO_CONFIG
        if (card->IsAssociated)
            DbgPrint("UpdateLinkStatus: Indicating NDIS_STATUS_MEDIA_CONNECT\n");
        else 
            DbgPrint("UpdateLinkStatus: Indicating NDIS_STATUS_MEDIA_DISCONNECT\n");
        #endif
    }
}

void SetPromiscuousMode(PCARD card, ULONG mode)
{
    // called from outside of the INTDPC.
    // nothing changes, do nothing
    if( (mode && card->m_PromiscuousMode) ||
        (!mode && !card->m_PromiscuousMode) )
        return;
        
    card->m_PromiscuousMode = mode;
    
    USHORT param    = mode ? 0xFFFF : card->m_PowerSaveMode;
    
    if( ! card->m_MaxPSP ){

        cmdSetOpMode(card, param);
    }
    else{
        if( 0xFFFF != param )
            param -= 2;

        int     KeepAwake = card->KeepAwake;

        card->KeepAwake = 1;
        cmdAwaken( card, TRUE );        // wait for upto 100 ms 
        cmdSetOpMode(card, param);
        card->KeepAwake = KeepAwake;
    
        if( 0==card->KeepAwake )
            cmdSleep(card);
    }
}

//===========================================================================
    BOOLEAN InitHW (PCARD card)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    card->m_RcvReg  = card->m_IOBase+REG_BAPrx_DATA;
    card->m_XmitReg = card->m_IOBase+REG_BAPtx_DATA;

    if (FALSE == FindCard(card)) {
        LogError( card, NDIS_ERROR_CODE_ADAPTER_NOT_FOUND, 0x00010000 | ADAPTER_NOT_FOUND);
        return FALSE;
        }
    
//  if (0xFFFF != (0xFFFF&card->m_MagicPacketMode)) {
//      InitStatusChangeReg( card );
//      }
    BOOLEAN retval = InitFW(card);
    return retval ? TRUE : card->m_CardFirmwareErr; 
}

//===========================================================================
    void InitStatusChangeReg ( PCARD card )
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    int StatusRegOff        = 0x3e2; 
    int PinReplaceRegOff    = 0x3e4; 

#ifndef UNDER_CE
    UCHAR   uChar;
#endif

#if (NDISVER < 5) || (NDISVER == 41)        //spbMgc
    if (card->m_AttribMemRegistered) {
        NdisWriteRegisterUchar(card->m_pAttribMemBase+PinReplaceRegOff, 0x0F);
        DelayMS(50);
        NdisReadRegisterUchar(card->m_pAttribMemBase+StatusRegOff, &uChar);
        NdisWriteRegisterUchar(card->m_pAttribMemBase+StatusRegOff, uChar | 0x40);

#if DBG 
        NdisReadRegisterUchar(card->m_pAttribMemBase+StatusRegOff, &uChar);
#endif
    }
#else
    USHORT uShort=0x0F;
    NdisWritePcmciaAttributeMemory(card->m_MAHandle,PinReplaceRegOff/2,&uShort,1);
    DelayMS(50);
    NdisReadPcmciaAttributeMemory(card->m_MAHandle,StatusRegOff/2,&uShort,1);
    uShort|=0x40;
    NdisWritePcmciaAttributeMemory(card->m_MAHandle,StatusRegOff/2,&uShort,1);

#if DBG 
    USHORT tmp=0;
    NdisReadPcmciaAttributeMemory(card->m_MAHandle,StatusRegOff/2,&tmp,1);
#endif
#endif
}

//===========================================================================
    BOOLEAN InitFW (PCARD card, BOOLEAN useCurrentConfig)
//===========================================================================
// 
// Description: Initialize the radio firmware
//    
//      Inputs: card              - pointer to card structure
//              useCurrentConfig  - set true use re-use the current config info (as in after 
//                                  hibernating). This forces use of the existing config info 
//                                  contained in the card structure rather re-initializing 
//                                  from the registry.  (default = false).
//    
//     Returns: TRUE if successful, FALSE otherwise.
//---------------------------------------------------------------------------
{

/*
: reset elapsed time = 1051 ms
: AckCmdInterrupt elapsed time = 2093 ms
: config elapsed time = 2423 ms
: command elapsed time = 4836 ms
: elapsed time = 4846 ms
: elapsed time = 5187 ms
*/
    card->m_PrevCmdDone     = TRUE;
    card->m_IsPolling       = TRUE;
    card->IsAwake           = TRUE;             
    
    CQReset(card->m_Q);
    CQReset(card->fidQ);
    RESETSTACK();

    //spb001 We need to do this now because of the loadVXD function needs this info
    cmdCapsGet(card, &card->capabilities);
    
    if (0 == strcmp("????????", card->m_CardName)) { 
        RADIOTYPE   *pRt    = RadioTypesArray;
    
        for ( ;pRt->CardName; pRt++) {
            extern USHORT Pci_Dev_Id;
            if (pRt->ProductNum != card->capabilities.ProuctNum) {
                continue;
                }
            strcpy(card->m_CardName, pRt->CardName);
            card->m_FrequencyType   = pRt->FrequencyType;
            card->m_CardType        = pRt->CardType;
            Pci_Dev_Id              = pRt->Pci_Dev_Id;
            break;
            }
        }
        
    if (0 == strcmp(card->m_CardName, "PC2500")) {
        memcpy( card->m_ESS_ID1, card->m_SystemID, sizeof(UINT)); 
        card->m_ESS_IDLen1 = sizeof(UINT);
        }

#ifdef MICROSOFT
    //spb026
    if (!card->wasFlashChecked) {
        card->wasFlashChecked = TRUE;
        if (CheckRev(card)) {
            #if DBG
                DbgPrint("Firmware is old.\n");
            #endif
            card->flashCard=TRUE;
            card->initComplete=TRUE;    //so we fall through
        }
    }
#endif

    //--------------------------------------------------------------------------
    // reset the firmware
    cmdReset(card);

    //This will cause a Link Event Status to be sent up the stack 
    //when we reassociate
    NdisZeroMemory(card->m_BSSID, sizeof(MacAddr)); 

    //This will make sure we send a link down if we don't reassociate
    //MediaDisconnectDamperTick must be greater than 0 for this to happen, hence the check.
    card->m_MediaDisconnectDamperTick = MAX((ULONG)card->m_MediaDisconnectDamper,(ULONG)2);
    card->IsAssociated                = FALSE;

    //spb001 Initialization just started
    if (!card->initComplete) {
        return TRUE;
    }

    return InitFW1(card,useCurrentConfig);
}

//===========================================================================
    BOOLEAN InitFW1 (PCARD card, BOOLEAN useCurrentConfig)
//===========================================================================
// 
// Description: Initialize the radio firmware (Part 2)
//    
//      Inputs: card              - pointer to card structure
//              useCurrentConfig  - set true use re-use the current config info (as in after 
//                                  hibernating). This forces use of the existing config info 
//                                  contained in the card structure rather re-initializing 
//                                  from the registry.  (default = false).
//    
//     Returns: TRUE if successful, FALSE otherwise.
//---------------------------------------------------------------------------
{
    USHORT usWord = 0;

    card->initComplete=TRUE;        //spb001

    //spb023
    DelayMS(1000);
    NdisRawWritePortUshort( card->m_IOBase+REG_CMD, CMD_X500_NOP10 );

    int j;
    for (j = 1000; j && (0 == usWord); j--) {
        NdisRawReadPortUshort(card->m_IOBase+REG_INT_STAT, &usWord);
        usWord &= EVNT_STAT_CMD;
        DelayMS( 1 );       
    }

    AckCmdInterrupt(card);

    if (0xFFFF != (0xFFFF & card->m_MagicPacketMode)) {
        InitStatusChangeReg( card );
    }

    // Try to read dBm table from card (if supported).
    card->dBmTable = cmdDBMTableGet(card);

    // Use the existing config info contained in the card structure.  Otherwise, we need to 
    // read the default values from the radio, mod them with values from the registry, and 
    // stuff them back into the radio.
    if (useCurrentConfig) {
        cmdConfigSet(card);
        cmdSSIDSet(card);
        cmdAPsSet(card);
        }
    else {
        // This flag will be set on startup when there are no zflags in the registry.
        // In this case we'll read the card's stored config and use that as the default.
        // The flag is cleared so this only happens once.
        if (card->m_readCardConfig) {
            cmdConfigGet(card, &card->m_profiles[0].zFlags.cfg);
            cmdSSIDGet(card, &card->m_profiles[0].zFlags.SSIDList);
            cmdAPsGet(card, &card->m_profiles[0].zFlags.APList);
            card->m_readCardConfig = FALSE;
            }

        CFG_X500 cfg;
        if (!cmdConfigGet(card, &cfg)) {
            LogError( card, NDIS_ERROR_CODE_HARDWARE_FAILURE, 0x00020000 | HARDWARE_FAILURE);
            return FALSE;
            }
        SetUserConfig(card, cfg);                           // update it    
        if (!cmdConfigSet(card, &cfg)) {
            LogError( card, NDIS_ERROR_CODE_HARDWARE_FAILURE, 0x00030000 | HARDWARE_FAILURE);
            return FALSE;
            }

        STSSID ssid;
        if (!cmdSSIDGet(card, &ssid)) {
            LogError( card, NDIS_ERROR_CODE_HARDWARE_FAILURE, 0x00040000 |HARDWARE_FAILURE);
            return FALSE;
            }
        SetUserSSID(card, ssid);                            // update it
        if (!cmdSSIDSet(card, &ssid)) {
            LogError( card,  NDIS_ERROR_CODE_HARDWARE_FAILURE, 0x00050000 |HARDWARE_FAILURE);
            return FALSE;
            }

        STAPLIST aps;
        if (!cmdAPsGet(card, & aps )) {
            LogError( card,  NDIS_ERROR_CODE_HARDWARE_FAILURE, 0x00060000 |HARDWARE_FAILURE);
            return FALSE;
            }
        SetUserAPs(card, aps);                          // update it
        if (!cmdAPsSet(card, & aps)) {
            LogError( card,  NDIS_ERROR_CODE_HARDWARE_FAILURE, 0x00070000 |HARDWARE_FAILURE);
            return FALSE;
            }
        }

    //--------------------------------------------------------------------------
    // Set auto config timer.  In case we don't associate, this will allow us to
    // automatically switch to another profile.
    setAutoConfigTimer(card);

    if (!cmdEnable(card)) {
        return FALSE;
        }

    GetCmdStatus(card, CMD_X500_EnableAll);

    BOOLEAN Acked       = FALSE;
    BOOLEAN associated  = FALSE;
    for (int i = 0; (i < 3) && (FALSE == CQIsFull(card->fidQ)); i++) {
        FID fid = AllocNextFid(card);

        if (0 == fid) {
            LogError( card, NDIS_ERROR_CODE_HARDWARE_FAILURE, 0x00080000 |HARDWARE_FAILURE);
            return FALSE;
            }
        
        fidCounter++;
        FidArray[ i ] = fid;
        CQStore( card->fidQ, fid );                 
        }
    
#if NDISVER == 3  || NDISVER == 41
    // delay 2 seconds, waiting for a status link 
    for (i = 0; i < 2000; i++) {
        NdisRawReadPortUshort( card->m_IOBase+REG_INT_STAT, &usWord );

        if (usWord & EVNT_STAT_CMD) {
            AckCmdInterrupt(card);
        }

        if (usWord & EVNT_STAT_LINK) {
            break;
        }
    }
#endif

    card->m_IsPolling   = FALSE;
    card->m_CardStarted = TRUE;
    card->m_CardPresent = TRUE;
    card->m_PrevCmdDone = TRUE;

    if (card->m_MaxPSP) {
        cmdSleep(card);
    }

#ifdef MICROSOFT
    //spb026
    if (TRUE == card->flashCard) {
       card->flashCard=FALSE;   
       FlashImage(card);
    }
#endif

    return TRUE;
}


void InitInterrupts( PCARD card )
{
    card->m_IntMask     =  card->m_MaxPSP ?
        (USHORT)(INT_EN_Rx| INT_EN_Tx   | INT_EN_TxExc | INT_EN_CMD | INT_EN_AWAKE | INT_EN_LINK):
        (USHORT)(INT_EN_Rx | INT_EN_Tx  | INT_EN_TxExc | INT_EN_CMD | INT_EN_LINK );
  
    if (card->m_InterruptRegistered ) 
        NdisRawWritePortUshort( card->m_IOBase+REG_INT_EN, card->m_IntMask );
    else
        NdisRawWritePortUshort( card->m_IOBase+REG_INT_EN, 0 );

}


//===========================================================================
    void SetUserConfig (PCARD card, CFG_X500 &cfg)
//===========================================================================
// 
// Description: Overlay "advanced" fields on top of the specified cfg structure.
//    
//      Inputs: card    - pointer to card structure.
//              cfg     - reference to config structure on which we'll overlay
//                        registry info.
//    
//     Returns: nothing
//---------------------------------------------------------------------------
{
    cfg.TxMSDULifetime  = 1500; // 1.5 second;      
    // 
    // Grab the real mac address from the cfg structure.
    // 
    memcpy(card->m_PermanentAddress, cfg.MacId, sizeof(card->m_StationAddress));
    memcpy(card->m_profiles[0].zFlags.cfg.MacId, cfg.MacId, sizeof(card->m_StationAddress));

    // 
    // Use the default zflags if they're valid.
    // 
    if (card->m_profiles[0].isValid) {
        cfg = card->m_profiles[0].zFlags.cfg;
        }
    // 
    // Overide the MAC address if necessary.
    // 
    if (card->m_MacIdOveride) {
        memcpy(cfg.MacId, card->m_StationAddress, sizeof(card->m_StationAddress));
        }
    else {
        memcpy(card->m_StationAddress, cfg.MacId, sizeof(card->m_StationAddress));
        }
    
    memcpy(cfg.NodeName, card->m_AdapterName, 16);

    cfg.OpMode          = card->m_InfrastructureMode;       
    cfg.PowerSaveMode   = card->m_PowerSaveMode;
    card->m_MaxPSP      = (card->m_PowerSaveMode >= 3) && (1 & card->capabilities.u16SoftwareCapabilities);
    
    if (2 < cfg.PowerSaveMode) {
        cfg.PowerSaveMode -= 2;
        }
    // 
    // added INT_EN_AWAKE per Ned.  (jbeaujon 11/01/00) 
    // 
    // removed INT_EN_AWAKE per Ned.  (jbeaujon 11/14/00)
    // 
    if (card->m_MaxPSP) {
        cfg.MaxPowerSave = INT_EN_CMD | INT_EN_Rx | INT_EN_Tx | INT_EN_TxExc | INT_EN_LINK;
/*
        cfg.MaxPowerSave = INT_EN_CMD | INT_EN_Rx | INT_EN_Tx | INT_EN_TxExc | INT_EN_LINK | INT_EN_AWAKE;
*/
        }
    
    if (card->m_TransmitPower) {
        cfg.TransmitPower = card->m_TransmitPower;
        }

    STSUPRATES  *pSel = SUPPORTED_RATES;
    while ((0xFFFF != pSel->SelectedRate) && (pSel->SelectedRate != card->m_SelectedRate)) {
        ++pSel;
        }
    
    if (0xFFFF == pSel->SelectedRate) {
        memcpy( card->m_SupportedRates, cfg.SupRates, sizeof(cfg.SupRates));
        card->m_LinkSpeed = pSel->LinkSpeed;
        }
    else if (pSel->SelectedRate) {
        memcpy(card->m_SupportedRates, pSel->SupRates, sizeof(cfg.SupRates));
        memcpy(cfg.SupRates, pSel->SupRates, sizeof(cfg.SupRates));
        card->m_LinkSpeed = pSel->LinkSpeed;
        }
    else {  // auto mode
        while ((0xFFFF != pSel->SelectedRate) && strcmp(card->m_CardName, pSel->CardName)) {
            ++pSel;
            }

        card->m_LinkSpeed = pSel->LinkSpeed;
        if (0xFFFF != pSel->SelectedRate) {
            memcpy( cfg.SupRates, pSel->SupRates, sizeof(cfg.SupRates));        
            }
        }
    
    switch (0xFFFF & card->m_MagicPacketMode) {
        case 0:             //Broadcast/Multicast/Unicast = ignore nothing
            cfg.MagicMode = MPA_STAUSCHANGE; 
            break;

        case 1:             //Broadcast/Unicast  = ignore multicast
            cfg.MagicMode = MPM_NOMULTICAST | MPA_STAUSCHANGE; 
            break;

        case 2:             //Unicast = ignore broadcast
            cfg.MagicMode = MPM_NOBROADCAST | MPM_NOMULTICAST | MPA_STAUSCHANGE;
            break;

        case 0xFFFF:

        default:
            break;
        }
}

//===========================================================================
    void SetUserSSID (PCARD card, STSSID &ssid)
//===========================================================================
// 
// Description: Overlay "advanced" fields on top of the specified ssid structure.
//    
//      Inputs: card    - pointer to card structure.
//              ssid    - reference to ssid structure on which we'll overlay
//                        registry info.
//    
//     Returns: nothing
//---------------------------------------------------------------------------
{
    if (card->m_profiles[0].isValid) {
        ssid = card->m_profiles[0].zFlags.SSIDList;
        }
    ssid.Len1 = card->m_ESS_IDLen1;
    if (card->m_ESS_IDLen1) {
    	// PREFIX #50063
        memcpy(ssid.ID1, card->m_ESS_ID1, min(sizeof(ssid.ID1), card->m_ESS_IDLen1));
        }
}

//===========================================================================
    void SetUserAPs (PCARD card, STAPLIST &aps)
//===========================================================================
// 
// Description: Overlay "advanced" fields on top of the specified aps structure.
//    
//      Inputs: card    - pointer to card structure.
//              aps     - reference to ap list structure on which we'll overlay
//                        registry info.
//    
//     Returns: nothing
//---------------------------------------------------------------------------
{
    if (card->m_profiles[0].isValid) {
        aps = card->m_profiles[0].zFlags.APList;
        }
}

//===========================================================================
    NDIS_STATUS cbResetCard (OUT PBOOLEAN   AddressingReset, 
                             IN NDIS_HANDLE Context)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    
    PCARD card = (PCARD)Context;

#ifndef UNDER_CE
    USHORT  usWord;
#endif

#if NDISVER == 3

    //*AddressingReset = FALSE;
    //turn  NDIS_STATUS_NOT_RESETTABLE;
    BOOLEAN IsInserted = IsCardInserted(card);

    if (IsInserted && card->m_CardPresent) {    // same ol same ol
        PRINT("RESET");
        CQReset(card->m_Q);
        *AddressingReset = FALSE;
        //

        PNDIS_PACKET    Packet;
        while (Packet = (PNDIS_PACKET)CQGetNext(card->m_Q)) {
            NdisMSendComplete(  card->m_MAHandle, Packet, NDIS_STATUS_SUCCESS );
            }
        //return    NDIS_STATUS_SUCCESS;
        }
    else if (IsInserted && ! card->m_CardPresent) {  // just inserted
        CQReset(card->fidQ);        
        CQReset(card->m_Q);
        InitHW(card );  
        InitInterrupts( card );
        }

    else if (!IsInserted && card->m_CardPresent) {   // just removed
        card->m_CardStarted = FALSE;
        card->m_CardPresent = FALSE;
        }
    else {   // !IsInserted && !card->m_CardPresent 
        }

    return  NDIS_STATUS_SUCCESS;

#elif NDISVER == 5


#ifndef UNDER_CE
    NdisAcquireSpinLock(&card->m_lock);

    while (card->m_TxHead) {
        NdisReleaseSpinLock(&card->m_lock);
        NdisMSendComplete(card->m_MAHandle, card->m_TxHead, NDIS_STATUS_SUCCESS);
        NdisAcquireSpinLock(&card->m_lock);
        DequeuePacket(card->m_TxHead, card->m_TxTail);
        }   
    NdisReleaseSpinLock(&card->m_lock);

#else
{
    NDIS_PACKET     *pNdisPacket;
    
    NdisAcquireSpinLock(&card->m_lock);
    while (pNdisPacket = card->m_TxHead) {
        DequeuePacket(card->m_TxHead, card->m_TxTail);        
        NdisReleaseSpinLock(&card->m_lock);
        NdisMSendComplete(card->m_MAHandle, pNdisPacket, NDIS_STATUS_FAILURE);
        NdisAcquireSpinLock(&card->m_lock);
        }   
    NdisReleaseSpinLock(&card->m_lock);
}    

#endif

    *AddressingReset = FALSE;

    return  NDIS_STATUS_SUCCESS;
#endif
}

//===========================================================================
    BOOLEAN FindCard (PCARD card)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    USHORT  usWord;
    ULONG   port[]      = {REG_CMD_P0};
    USHORT  cmp[]       = {0x0000, 0xAA55, 0x55AA};
    int     portSize    = sizeof(port) / sizeof(port[0]);
    int     cmpSize     = sizeof(cmp) / sizeof(cmp[0]);
    
    //spbMgc Make sure Attribue Memory is registered before we use it.   
    if (card->IsAMCC && card->m_AttribMemRegistered) {
        USHORT  usWord;
        // get the control register;
        NdisReadRegisterUshort((USHORT *)(card->m_ContolRegMemBase+0x38), &usWord);
        //enable the interrupt register.
        usWord |= 0x02000;
        NdisWriteRegisterUshort((USHORT *)(card->m_ContolRegMemBase+0x38), usWord);
        //
        NdisWriteRegisterUshort((USHORT *)(card->m_pAttribMemBase+0x3E0), 0x0045);
        //NdisWriteRegisterUshort( (USHORT *)(card->m_RadioMemBase+0x3E0), 0x0045 );        // only because we don't know which is which
        }
    // 
    // enable FULL Micro ISA Over ISA fulll IO range  
    // 
    NdisRawWritePortUchar(card->m_IOBase+ 6 , 0x2000);    // must be first
    NdisRawWritePortUchar(card->m_IOBase+ 7 , 0x20);      // must be second
    // 
    // enable FULL Micro ISA fulll IO range  
    // 
    if (card->m_PortIOLen8Bit) {
        NdisRawWritePortUchar(card->m_IOBase8Bit+ 7 , 0x20);
        }

    //cmdReset(card);

    for (int j = 0; j < portSize; j++) {
        for (int i = 0; i < cmpSize; i++) {
            NdisRawWritePortUshort(card->m_IOBase+port[j], cmp[i]);
            NdisRawReadPortUshort(card->m_IOBase+port[j], &usWord);
            if (cmp[i] != usWord) {
                return FALSE;
                }
            }
        }
    NdisRawWritePortUshort(card->m_IOBase+REG_AUX_PAGE, 0x0000);
    NdisRawWritePortUshort(card->m_IOBase+REG_AUX_OFF, 0x0000);
    NdisRawReadPortUshort(card->m_IOBase+REG_AUX_DATA, &usWord);

    if (0x0000 != usWord) {
        return FALSE;
        }
    
    NdisRawReadPortUshort(card->m_IOBase+REG_AUX_DATA, &usWord);
    if ((0xFFF0 & cmp[cmpSize-1]) != (0xFFF0 & usWord)) {
        return FALSE;
        }

    NdisRawWritePortUshort(card->m_IOBase+REG_SWS0, 0x0000);
    NdisRawWritePortUshort(card->m_IOBase+REG_SWS1, 0x0000);
    NdisRawWritePortUshort(card->m_IOBase+REG_CMD, 0x0000);

    usWord = 0x8000;
    USHORT usFlashCode;

    for (j = 4000; j && (usWord & 0x8000); j-- ) {
        NdisRawReadPortUshort(card->m_IOBase+REG_CMD, &usWord);
        NdisRawReadPortUshort(card->m_IOBase+REG_SWS1, &usFlashCode);
        if (0x33 == (0xff & usFlashCode)) {
            card->m_CardFirmwareErr = TRUE;
            break;
            }
        DelayMS( 1 );       
        }

    usWord = 0;
    for (j = 4000; !card->m_CardFirmwareErr && j && 0==usWord; j--) {
        NdisRawReadPortUshort(card->m_IOBase+REG_INT_STAT, &usWord);
        usWord &= EVNT_STAT_CMD;
        DelayMS( 1 );       
        }
    
    AckCmdInterrupt(card);

    return (j > 0) || card->m_CardFirmwareErr;  
}

void            
UpdateCounters(PCARD    card)
{
    if( ! card->IsAwake )
        return;

    STSTATISTICS32  st;         
    cmdStatistics32Get(card, &st );

    card->m_FramesXmitGood          = st.HostTxMc+st.HostTxBc+st.HostTxUc;      // Good Frames Transmitted
    card->m_FramesXmitBad           = st.HostTxFail;                            // Bad Frames Transmitted
    card->m_FramesRcvGood           = st.HostRxMc+st.HostRxBc+st.HostRxUc;      // Good Frames Received
    card->m_FramesRcvBad            = st.HostRxDiscard;         // CRC errors counted
    card->m_FramesRcvOverRun        = st.RxOverrunErr;                          // missed packet counted
    card->m_FramesXmitOneCollision  = st.TxSinColl;                             // Frames Transmitted with one collision
    card->m_FramesXmitManyCollisions= st.TxMulColl;                             // Frames Transmitted with > 1 collision
}

BOOLEAN
RxFidSetup(PCARD card)
{
    USHORT  fid;
#ifndef UNDER_CE
    USHORT  usWord;
#endif
    NdisRawReadPortUshort( card->m_IOBase+REG_FID_RX, &fid);        // Get the FID value
    return BapSet(card, REG_BAPrx_SEL, fid, sizeof(RXFID_HDR) ); //off* );
}


BOOLEAN 
IsCardInserted( 
    PCARD card 
    )
{
    USHORT  usWord;
    NdisRawReadPortUshort( card->m_IOBase+REG_INT_EN, &usWord);     
    return 0xFFFF != usWord; 
}

