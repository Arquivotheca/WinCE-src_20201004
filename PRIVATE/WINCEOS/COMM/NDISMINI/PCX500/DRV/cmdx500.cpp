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
//---------------------------------------------------------------------------
// CmdX500.cpp
//---------------------------------------------------------------------------
// Description:
//
// Revision History:
//
// Date         
//---------------------------------------------------------------------------
// 09/22/00     jbeaujon        -modified cmdConfigSet, cmdSSIDSet, cmdAPsSet so 
//                               that if the second parameter is NULL, we use the
//                               copy stored in card.
// 11/10/00     jbeaujon        -In AckCommand: clear the EVNT_STAT_CMD bit in
//                               card->m_IntActive so we don;t ack the same 
//                               command twice.
//
// 06/05/01     spb009          -Send the disconnect status when we disable the
//                               Radio
//
// 06/12/01     spb010          -Make sure the buffer is an even number of bytes
//                               during a RidSet and RidGet 
//
//---------------------------------------------------------------------------

#pragma code_seg("LCODE")
#include "NDISVER.h"
extern "C"{
    #include    <ndis.h>
}

extern ULONG fidCounter;
#include "CmdX500.h"
#include "support.h"
#include "memory.h"
#include "Airodef.h"
#include "CardX500.h"


BOOLEAN         
exec(PCARD card)
{
    return(exec(card, 100));
}

BOOLEAN         
exec(PCARD card, int delay )

{
    if( !WaitBusy(card, 10 ) ) {
        return FALSE;
        }
            
    if ( ! WaitLastCmd( card, delay ) ) {
        return FALSE;
        }

    WriteCommand( card );
    CmdRestoreInterrupts(card);

    return TRUE;
}

BOOLEAN         
exec(PCARD card, int delay, USHORT p0 )
{
    if( !WaitBusy(card, 10 ) ) {
        return FALSE;
        }
            
    if ( ! WaitLastCmd( card, delay ) ) {
        return FALSE;
        }

    WriteCommand( card, p0 );
    CmdRestoreInterrupts(card);

    return TRUE;
}

BOOLEAN         
WaitComplete(PCARD card, int delay)// delay in us, 1 second default          
{
#ifndef CE_FLAG_USE_NDISM_SLEEP
    for( ; ! IsCmdComplete(card) && delay > 0; delay -=10 ){
        DelayUS( 10 );
    }   

#else
    //
	//	CE can't guarantee granularity of < 1 ms.
	//

	for( ; ! IsCmdComplete(card) && delay > 0; delay -=1000 ){
        DelayUS( 1000 );
    } 

#endif

    if( ! IsCmdComplete(card) ) {
        return FALSE;
        }
    
    return TRUE;
}

BOOLEAN         
WaitAllocComplete(PCARD card, int delay )
{
    while( ! IsAllocComplete(card) && --delay ){
        DelayMS( 1 );
    }
    
    if( ! IsAllocComplete(card) )
        return FALSE;
    
    return TRUE;
}

CMD_STATUS_X500 
GetCmdStatus(PCARD card, CMD_X500 cmd )
{
    USHORT  usWord;
    
    NdisRawReadPortUshort( card->m_IOBase+REG_CMD_STATUS, &usWord );

    if ( 0xFFFF != cmd && (0x3f & cmd) != (usWord & 0x3f) ){
        return CMD_STATUS_NOTCMD;
    }   
    if ( 0x0000 == (usWord & 0x7F00) )
        return CMD_STATUS_SUCCESS;

    if ( 0x0100 == (usWord & 0x7F00) )
        return CMD_STATUS_NOTASSOC;

    if ( 0x0200 == (usWord & 0x7F00) )
        return CMD_STATUS_FAILURE;

    if ( 0x3F00 == (usWord & 0x7F00) )
        return CMD_STATUS_CMDERROR;

    return CMD_STATUS_UKNOWNERROR;
}

void            
WriteCommand(PCARD card )
{
    card->m_PrevCmdTick = 1;
    card->m_PrevCmdDone = FALSE;
    NdisRawWritePortUshort( card->m_IOBase+REG_CMD, card->m_cmd );
    card->m_PrevCommand = card->m_cmd;
}

void            
WriteCommand(PCARD card, USHORT p0 )
{
    NdisRawWritePortUshort( card->m_IOBase+REG_CMD_P0, p0 );
    WriteCommand(card);
}

void            
GetResponce(PCARD card, USHORT *p0,USHORT *p1,USHORT *p2)
{
    if( p0 )    ReadRespParam( card, (REGX500)(card->m_IOBase+REG_CMD_RESP0) , p0 );                        
    if( p1 )    ReadRespParam(  card, (REGX500)(card->m_IOBase+REG_CMD_RESP1), p1 );                        
    if( p2 )    ReadRespParam(  card, (REGX500)(card->m_IOBase+REG_CMD_RESP2), p2 );                        
}

BOOLEAN         
IsCmdReady(PCARD card)           
{
    USHORT  usWord;
    NdisRawReadPortUshort( card->m_IOBase+REG_CMD, &usWord );
    return 0 == (0x8000&usWord);
}

BOOLEAN         
IsEventAllocReady(PCARD card)
{
    USHORT  usWord;
    NdisRawReadPortUshort( card->m_IOBase+REG_INT_STAT, &usWord );
    return  0 != (0x0008&usWord);
}

BOOLEAN         
IsResponceReady(PCARD card)
{
    USHORT  usWord;
    NdisRawReadPortUshort( card->m_IOBase+REG_CMD_STATUS, &usWord );
    return  0 != (0x8000&usWord);
}

BOOLEAN         
IsCmdComplete(PCARD card)
{
    USHORT  usWord;
    if( card->m_PrevCmdDone )
        return TRUE;
    
//  if( ! card->m_IsPolling )
//      return FALSE;

    NdisRawReadPortUshort( card->m_IOBase+REG_INT_STAT, &usWord );
    
    if( EVNT_STAT_CMD & usWord ){
        AckCommand(card);
        return TRUE;
    }
    return FALSE;
}

BOOLEAN         
IsAllocComplete(PCARD card)
{
    USHORT  usWord;
    NdisRawReadPortUshort( card->m_IOBase+REG_INT_STAT, &usWord );
    return EVNT_STAT_Alloc & usWord;
}

void            
AckCommand(PCARD card)
{
    NdisRawWritePortUshort( card->m_IOBase+REG_INT_ACK, EVNT_ACK_CMD );
    // 
    // Clear the command bit in our copy in case we get here before cbHandleInterrupt()
    // gets invoked.  (jbeaujon  11/09/00)
    // 
    card->m_IntActive &= ~EVNT_STAT_CMD;

    card->m_PrevCmdDone = TRUE;
}

void            
AckAlloc(PCARD card)
{
    NdisRawWritePortUshort( card->m_IOBase+REG_INT_ACK, EVNT_ACK_Alloc );
}
/*
BOOLEAN         
WaitBusy(PCARD card, int us)
{
    USHORT  usWord = 0xFFFF;
    int delay = 0; 
    while( (usWord & 0x8000) && delay <= us){
        DelayUS(2);
        delay   += 2;
        NdisRawReadPortUshort( card->m_IOBase+REG_CMD, &usWord );
    }
    return 0 == (0x8000&usWord);
}
*/
void            
UnstickBusyCommand(PCARD card )
{
    USHORT  usWord;
    NdisRawWritePortUshort( card->m_IOBase+REG_INT_ACK, 0x4000 );
    NdisRawReadPortUshort( card->m_IOBase+REG_CMD,  &usWord );
    if( 0x7FFF & usWord && 0==(0x8000 & usWord))
        NdisRawWritePortUshort( card->m_IOBase+REG_CMD, usWord );
}

BOOLEAN         
WaitBusy(PCARD card, int us)
{
    USHORT  usWord = 0xFFFF;
    int     timeout = 1000 * 100;   // 100 milliseconds
    int delay = 0; 
    
    while( (usWord & 0x8000) && delay <= timeout){
#ifndef CE_FLAG_USE_NDISM_SLEEP
        DelayUS(10);
        delay   += 10;
#else
		//
		//	CE can't guarantee granularity of < 1 ms.
		//

		DelayUS(1000);
		delay	+= 1000;
#endif
        NdisRawReadPortUshort( card->m_IOBase+REG_CMD, &usWord );
        
        if( (0x8000 & usWord) && 0==(delay%200) ){
            UnstickBusyCommand(card);
            card->m_PrevCmdDone = TRUE;
        }
    }
    return 0 == (0x8000&usWord);
}

BOOLEAN         
WaitLastCmd(PCARD card, int delay)
{
    BOOLEAN res;    
    int tmp = 0;  

    while( !(res = IsCmdComplete(card))&& tmp <= delay ){ 
        tmp += 10;
        DelayUS( 10 );
    }   
    
    if( ! res ) {
        return FALSE;
        }

    CmdDisableInterrupts(card);
    
    tmp = delay - tmp;  
    while( !(res = IsCmdComplete(card))&& tmp <= delay ){ 
        tmp += 10;
        DelayUS( 10 );
    }

    if ( ! res ){
        CmdRestoreInterrupts(card);     
        return FALSE;
    }
    
    return  TRUE;
}
#if 1
//BOOLEAN IsBapStuck(int IOBase, USHORT BapRegSel );
//void BapStuck(int IOBase,  USHORT BapRegSel, USHORT fid, USHORT off );

BOOLEAN IsBapStuck(int IOBase, USHORT BapRegSel )
{
    USHORT  usWord;
    NdisRawReadPortUshort( IOBase+BapRegSel, &usWord ); 
    return 0==usWord;   
}
void BapStuck(PCARD card, USHORT BapRegSel, USHORT fid, USHORT off )
{
//  1) disable interrupts
//  2) rewrite the Selector
//  3) rewrite the Offset
//  4) read SwSupport0
//  5) write SwSupport1     // -- this step MAY not actually be necessary
//  6) reenable interrupts
//  7) wait for busy to become unstuck
//  8) restart the entire bap access
    

    USHORT  usWord;
    USHORT  usInt;
    NdisRawReadPortUshort(card->m_IOBase+REG_INT_EN, &usInt );  // read ints enable
    NdisRawWritePortUshort(card->m_IOBase+REG_INT_EN, 0 );      // disable ints
    //spb024
    //If m_IntActive is set, then it means we got an interrupt after
    //reading active interrupts but before we were able to disable
    //interrupts.  Therefore interupts really should be disabled
    //when we return from this routine. 
    //Trust me this happens.  (Caused lost fids at Microsoft)
    if (card->m_IntActive) {
        usInt=0;
    }
    
    NdisRawWritePortUshort(card->m_IOBase+BapRegSel, fid );     
    NdisRawWritePortUshort(card->m_IOBase+BapRegSel+4, off );

    NdisRawReadPortUshort(card->m_IOBase+REG_SWS0, &usWord );  
    NdisRawWritePortUshort(card->m_IOBase+REG_SWS1, 0 );
                        
    NdisRawWritePortUshort(card->m_IOBase+REG_INT_EN, usInt );      // renable

    NdisRawWritePortUshort(card->m_IOBase+BapRegSel, fid );
    NdisRawWritePortUshort(card->m_IOBase+BapRegSel+BAPOFF_OFFSET_BAPX, off );
}

#endif

BOOLEAN         
BapSet(PCARD card, USHORT BAPOff, FID fid, int FidOff)
{
    USHORT  offValue;   
    int     delay = 0;  
    int     stuck = 0;
    
    while( delay <= 3000 ) {
        
        NdisRawWritePortUshort( card->m_IOBase+BAPOff, fid );
        NdisRawWritePortUshort( card->m_IOBase+BAPOff+BAPOFF_OFFSET_BAPX, (USHORT)FidOff );

        do {
            
            DelayUS( 25 );
            delay += 25;
            NdisRawReadPortUshort( card->m_IOBase+BAPOff+BAPOFF_OFFSET_BAPX, &offValue );
            
            if( IsFidRdy(offValue) )
                return TRUE;

            if( 300 <= stuck ){
                BapStuck(card, BAPOff, fid, (USHORT)FidOff );
                stuck =25;
                continue;
            }
            stuck +=25;
            
        }while( IsFidBusy(offValue) && 1000 >= delay );
        
        if( IsFidBusy(offValue) || IsFidErr(offValue) )
            return FALSE;

        if( IsFidRdy(offValue) )
            return TRUE;
    }

    //return !(IsFidBusy(offValue) || IsFidErr(offValue));
    return TRUE;
}

void            
BapMoveFidOffset(USHORT BAPOff, USHORT FidOffNew)
{
    NdisRawWritePortUshort( BAPOff+BAPOFF_OFFSET_BAPX, FidOffNew );
}


BOOLEAN     
XmitOpen(PCARD card, UINT param)
{
    if( 0 == (card->m_fid = (FID)CQGetNextND(card->fidQ)) )
        return FALSE;
    
    if( ! BapSet(card, REG_BAPtx_SEL, card->m_fid, sizeof(TXFID_HDR) ) )
        return FALSE; 
    
    NdisRawWritePortUshort(card->m_IOBase+REG_BAPtx_DATA, (USHORT)param-sizeof(HDR_802_3));
    return TRUE;
}

BOOLEAN     
XmitClose(PCARD card, UINT param)
{
    card->m_cmd = CMD_X500_Transmit;
    if( FALSE==exec( card, 600, card->m_fid ) ){     
        return FALSE;
    }
    fidCounter--;
    CQUpdate(card->fidQ);
    return TRUE;    
}


FID 
AllocNextFid(PCARD card)
{
#define PACKET_MAX_SIZE     TX_BUF_SIZE
#define FRAME_X500_SIZE (PACKET_MAX_SIZE+sizeof(TXFID_HDR))

    if ( FALSE == WaitLastCmd( card, 500000 ) ){
        return NULL;
    }
    
    //WaitComplete();
    USHORT      us = (USHORT)FRAME_X500_SIZE;   
    card->m_cmd = CMD_X500_Allocate;
        
    WriteCommand( card, us );       

    if( ! WaitComplete(card) ) {
        CmdRestoreInterrupts(card);
        return NULL;
        }

#if 1
    CMD_STATUS_X500 status =  GetCmdStatus(card, (CMD_X500)card->m_cmd);
    if( CMD_STATUS_SUCCESS != status ){
        CmdRestoreInterrupts(card);
        return NULL;
    }
#endif
    if( ! WaitAllocComplete(card) ) {
        CmdRestoreInterrupts(card);
        return NULL;
        }

    FID         tmpFid;             
    NdisRawReadPortUshort( card->m_IOBase+REG_FID_TX_ALLOC, &tmpFid );
    
    static      TXFID_HDR   txFidHdr = {{0,0,0,0,0x26,0,0,0}};
    if( ! BapSet(card, REG_BAPtx_SEL, tmpFid, 0 ) ) {
        CmdRestoreInterrupts(card);
        return FALSE; 
        }
    
    NdisRawWritePortBufferUshort(card->m_IOBase+REG_BAPtx_DATA, &txFidHdr, sizeof( TXFID_HDR )/2 );
    CmdRestoreInterrupts(card);
    AckAlloc(card);
    return tmpFid;
}

//===========================================================================
    BOOLEAN RidGet (PCARD card, USHORT m_RID, void *st, int dataLen)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    if (!WaitLastCmd(card, 100)) {
        return FALSE;
        }

    card->m_cmd = CMD_X500_AccessRIDRead;
    WriteCommand( card, m_RID );            // access RID read

    if (!WaitComplete(card)) { 
        CmdRestoreInterrupts(card);
        if (card->m_WasMacEnabled) {
            cmdEnable(card);
            }
        return FALSE;
        }
    
    if(BapSet(card, REG_BAP1_SEL, m_RID)) {
        ULONG size = dataLen/2;
        if (dataLen&1) {        //Account for Odd number of bytes   spb010
            size+=1;
            UCHAR *buf=new UCHAR[size*2];
            NdisRawReadPortBufferUshort(card->m_IOBase+REG_BAP1_DATA, buf, size);
            memcpy(st,buf,dataLen);
            delete buf;
        }
        else {
            NdisRawReadPortBufferUshort(card->m_IOBase+REG_BAP1_DATA, st, size);
        }
    }

    GetCmdStatus(card);
    CmdRestoreInterrupts(card);
    return TRUE;
}

//===========================================================================
    BOOLEAN RidSet (PCARD   card, 
                    USHORT  m_RID, 
                    void    *st, 
                    int     dataLen, 
                    BOOLEAN macEnable)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    if (!WaitLastCmd(card, 100)) {
        return FALSE;
        }

    if (card->m_WasMacEnabled) {
        cmdDisable(card);
        }

    card->m_cmd = CMD_X500_AccessRIDRead;
    WriteCommand(card, m_RID);      // access RID read

    if (!WaitComplete(card)) {
        CmdRestoreInterrupts(card);
        if (macEnable && card->m_WasMacEnabled) {
            cmdEnable(card);
            }
        return FALSE;
        }

    if (BapSet(card, REG_BAP1_SEL, m_RID)) {
        ULONG size = dataLen/2;
        if (dataLen&1) {        //Account for Odd number of bytes   spb010
            size+=1;         
            UCHAR *buf=new UCHAR [size*2];
            memset(buf,0,size*2);
            memcpy(buf,st,dataLen);
            //spb017 Fixed this so it was a write port instead of read.
            NdisRawWritePortBufferUshort(card->m_IOBase+REG_BAP1_DATA, buf, size);
            delete buf;
        }
        else {
            NdisRawWritePortBufferUshort(card->m_IOBase+REG_BAP1_DATA, st, size);
        }

        NdisRawWritePortUshort(card->m_IOBase+REG_CMD, CMD_X500_AccessRIDWrite);
        
        card->m_PrevCmdDone = FALSE;
        card->m_PrevCommand = CMD_X500_AccessRIDWrite;
    
        if (!WaitComplete(card)) {
            CmdRestoreInterrupts(card);
            if (macEnable && card->m_WasMacEnabled) {
                cmdEnable(card);
                }
            card->m_PrevCmdDone = TRUE;
            return FALSE;
            }
        }
    GetCmdStatus(card);
    CmdRestoreInterrupts(card);
    
    if (macEnable && card->m_WasMacEnabled) {
        cmdEnable(card);
        }
    
    return TRUE;
}

BOOLEAN 
RidSetIgnoreMAC(PCARD card, USHORT m_RID, void *st, int dataLen )
{
    if( ! WaitLastCmd(card, 100) ) {
        return FALSE;
        }

    card->m_cmd = CMD_X500_AccessRIDRead;
    WriteCommand( card, m_RID );      // access RID read

    if( ! WaitComplete(card) ){
        return FALSE;
    }   
    if( BapSet(card, REG_BAP1_SEL, m_RID ) ){

        ULONG   size = dataLen/2 + (1&dataLen);
        
        NdisRawWritePortBufferUshort( card->m_IOBase+REG_BAP1_DATA, st, size );
        NdisRawWritePortUshort( card->m_IOBase+REG_CMD, CMD_X500_AccessRIDWrite );
        
        card->m_PrevCmdDone = FALSE;
        card->m_PrevCommand = CMD_X500_AccessRIDWrite;
    
        if( ! WaitComplete(card, 30 * 1000000) ){
            card->m_PrevCmdDone = TRUE;
            CmdRestoreInterrupts(card);
            return FALSE;
        }
    }
    GetCmdStatus(card);
    CmdRestoreInterrupts(card);
    
    return TRUE;
}

//===========================================================================
    BOOLEAN cmdEnable (PCARD card)
//===========================================================================
// 
// Description: Enable the radio.   Also set flag in card indicating that 
//              we're enabled.
//    
//      Inputs: card - pointer to card structure.
//    
//     Returns: TRUE if successful, false otherwise.
//---------------------------------------------------------------------------
{
    card->m_cmd = CMD_X500_EnableAll;
    exec(card, 1000); 
    return (card->m_WasMacEnabled = card->m_IsMacEnabled = WaitComplete(card));
};

//===========================================================================
    BOOLEAN cmdDisable (PCARD card)
//===========================================================================
// 
// Description: Disable the radio.  Also set flag in card indicating that 
//              we're disabled.
//    
//      Inputs: card - pointer to card structure.
//    
//     Returns: TRUE if successful, false otherwise.
//---------------------------------------------------------------------------
{
    card->m_cmd = CMD_X500_Disable;
    exec(card, 1000); 
    
    if( WaitComplete(card) ) {
        card->m_IsMacEnabled = FALSE;

        //spb009
        if (NDIS_STATUS_MEDIA_CONNECT == card->m_MSenceStatus) {
            #if DBG
            DbgPrint("Sending NDIS_STATUS_MEDIA_DISCONNECT--\n");
            #endif
            card->IsAssociated   = FALSE;             //We are No Longer Associated
            card->m_MSenceStatus = NDIS_STATUS_MEDIA_DISCONNECT;
#ifndef UNDER_CE
            NdisMIndicateStatus(card->m_MAHandle, card->m_MSenceStatus, 0, 0 );
            NdisMIndicateStatusComplete(card->m_MAHandle);
#else

            //
            //  Shouldn't indicate if we get here from HaltHandler()..
            //  Ndis ASSERT.
            //

            if (!card->bHalting)
            {
                NdisMIndicateStatus(card->m_MAHandle, card->m_MSenceStatus, 0, 0 );
                NdisMIndicateStatusComplete(card->m_MAHandle);
            }
#endif
        }
        return TRUE;
    }
    return FALSE;
}

//===========================================================================
    BOOLEAN cmdBssidListScan (PCARD card)
//===========================================================================
// 
// Description: Have the radio perform a BSSID list scan.
//    
//      Inputs: card - pointer to card structure.
//    
//     Returns: 
//    
//      (01/29/01)
//---------------------------------------------------------------------------
{
    card->m_cmd = CMD_X500_BssidListScan;
    return exec(card, 1000);
}

BOOLEAN
cmdSetOpMode( PCARD card, USHORT param )
{
    card->m_cmd = CMD_X500_SetOperationMode;
    exec(card, 1000, param ); 
    return WaitComplete(card, 100000 ); // wait up to 100 ms
};

//========================================================================
BOOLEAN
cmdConfigGet(PCARD card, CFG_X500 * cfg)
{
    return RidGet(card, RID_CONFIG, cfg, sizeof(CFG_X500));
}

BOOLEAN
cmdConfigSet(PCARD card, CFG_X500 * cfg, BOOLEAN macEnable)
{
    if( NULL == cfg ) {
        cfg = &card->m_activeProfile->zFlags.cfg;
        }
    else {
        card->m_activeProfile->zFlags.cfg = *cfg;
        }

    //spb v7.37 Per Ned.
    if (0 == (cfg->OpMode & 0x00FF)) {
        //Adhoc
        CFG_X500 tmp;
        NdisMoveMemory(&tmp,cfg,sizeof(CFG_X500));
        //Make sure the ATIM is always 0
        tmp.AtimDuration = 0;
        return RidSet(card, RID_CONFIG, &tmp, sizeof(CFG_X500), macEnable );
    }
    else { 
        return RidSet(card, RID_CONFIG, cfg, sizeof(CFG_X500), macEnable );
    }
}
//========================================================================

BOOLEAN cmdSSIDGet(PCARD card, STSSID * ssid)
{
    return RidGet(card, RID_SSID, ssid, sizeof(STSSID) );
}

BOOLEAN cmdSSIDSet(PCARD card, STSSID * ssid, BOOLEAN macEnable)
{
    if( NULL == ssid ) {
        ssid = &card->m_activeProfile->zFlags.SSIDList;
        }
    else {
        card->m_activeProfile->zFlags.SSIDList = *ssid; 
        }
    return RidSet(card, RID_SSID, ssid, sizeof(STSSID), macEnable ) ;
}
//========================================================================

BOOLEAN cmdAPsGet(PCARD card, STAPLIST  *aps)
{
    return RidGet(card, RID_APP, aps, sizeof(STAPLIST));
}

BOOLEAN cmdAPsSet(PCARD card, STAPLIST  *aps, BOOLEAN macEnable)
{
    if( NULL == aps ) {
        aps = &card->m_activeProfile->zFlags.APList;
        }
    else {
        card->m_activeProfile->zFlags.APList = *aps; 
        }
    return RidSet(card, RID_APP, aps, sizeof(STAPLIST), macEnable );
}

//========================================================================
BOOLEAN
cmdCapsGet(PCARD card,STCAPS *cap )
{
    return RidGet(card, RID_CAP, cap, sizeof(STCAPS) );
}

BOOLEAN
cmdCapsSet(PCARD card,STCAPS *cap )
{
    return RidSet(card, RID_CAP, cap, sizeof(STCAPS) );
}
//========================================================================
BOOLEAN
cmdStatusGet(PCARD card,STSTATUS * status )
{
    return RidGet(card, RID_STATUS, status, sizeof(STSTATUS) );
}

BOOLEAN
cmdStatusSet(PCARD card,STSTATUS *status )
{
    return RidSet(card, RID_STATUS, status, sizeof(STSTATUS) );
}

//========================================================================
BOOLEAN         
cmdStatisticsGet(PCARD card, STSTATISTICS *stats )
{                                
    return RidGet(card, RID_STATS, stats, sizeof(STSTATISTICS) );
}
BOOLEAN         
cmdStatisticsClear(PCARD card,STSTATISTICS *stats )
{
    return RidGet(card, RID_STATS_CLEAR, stats, sizeof(STSTATISTICS) );
}
//........................................................................

BOOLEAN cmdStatistics32Get (PCARD card,STSTATISTICS32 *stats)
{                                
    return RidGet(card, RID_STATS32, stats, sizeof(STSTATISTICS32) );
}

BOOLEAN cmdStatistics32Clear (PCARD card,STSTATISTICS32 *stats)
{
    return RidGet(card, RID_STATS32_CLEAR, stats, sizeof(STSTATISTICS32) );
}

//========================================================================
BOOLEAN cmdReset(PCARD card)
{
    cmdDisable( card ); 
    card->m_cmd = CMD_X500_ResetCard;
    exec(card, 1000); 
    return TRUE;
}
//========================================================================
BOOLEAN         
cmdMagicPacket(PCARD card, BOOLEAN enable)
{
    CMD_X500 modeCmd = enable ? CMD_X500_MagicPacketON : CMD_X500_MagicPacketOFF;
    card->m_cmd = modeCmd;
    return exec(card, 1000); 
}
//========================================================================
BOOLEAN         
cmdSleep(PCARD card )
{
    NdisRawWritePortUshort(card->m_IOBase+REG_INT_ACK, EVNT_DO_SLEEP );
    card->IsAwake = FALSE;              
    return TRUE;
}

//===========================================================================
    BOOLEAN cmdAwaken (PCARD card, BOOLEAN wait)
//===========================================================================
// 
// Description: Wake the card up.
//    
//      Inputs: card - pointer to card structure
//              wait - if TRUE, wait for acknowledgement from card.
//                      otherwise, don't wait and return FALSE.
// 
//       Notes: Rewrote to fix an overflow condition that caused erroneous 
//              results to be returned. (jbeaujon - 11/15/00) 
//---------------------------------------------------------------------------
{
    if (!card->IsAwake) {
        USHORT  usWord;
        // 
        // save ints state & disable ints
        // 
        NdisRawReadPortUshort(card->m_IOBase+REG_INT_EN, &usWord );
        NdisRawWritePortUshort(card->m_IOBase+REG_INT_EN, 0 );
        //spb024
        //If m_IntActive is set, then it means we got an interrupt after
        //reading active interrupts but before we were able to disable
        //interrupts.  Therefore interupts really should be disabled
        //when we return from this routine. 
        //Trust me this happens.  (Caused lost fids at Microsoft)
        if (card->m_IntActive) {
            usWord=0;
        }
        //
        // Two writes are required to wake up the card.
        // 
        NdisRawWritePortUshort( card->m_IOBase+REG_INT_ACK, EVNT_DO_WAKEUP );
        NdisRawWritePortUshort( card->m_IOBase+REG_INT_ACK, EVNT_DO_WAKEUP );
        // 
        // restore interrupts
        // 
        NdisRawWritePortUshort(card->m_IOBase+REG_INT_EN, usWord );
        // 
        // Wait for wake up indication from the card.
        // 
        if (wait) {
            int i = 0;
            while (!card->IsAwake && (i++ <= 100)) {
                NdisRawReadPortUshort(card->m_IOBase+REG_INT_STAT, &usWord);
                if (usWord & EVNT_STAT_IsAWAKE) {
                    card->IsAwake = TRUE;
                    }
                   DelayMS(1);
                }
            }
        }
    return card->IsAwake;
}

//===========================================================================
    DBM_TABLE* cmdDBMTableGet (PCARD card)
//===========================================================================
// 
// Description: Read the dBm table from the card and return it in an allocated
//              buffer.  Note that not all cards support this feature (firmware
//              3.94 was the first).
//    
//      Inputs: card - pointer to card.
//    
//     Returns: pointer to allocated DBM_TABLE structure if successful, NULL
//              otherwise.
//    
//      (12/04/00)
//---------------------------------------------------------------------------
{
    DBM_TABLE   *table = new DBM_TABLE;
    if (table != NULL) {
        RidGet(card, RID_DBM_TABLE, table, sizeof(DBM_TABLE));
        if (table->length != sizeof(DBM_TABLE)) {
            delete table;
            table = NULL;
            }
        }
    return(table);
}

//===========================================================================
    BOOLEAN cmdGetFirstBSS (PCARD card, RID_BSS *ridBss, BOOLEAN *eol /* = NULL */)
//===========================================================================
// 
// Description: Get the first entry in the card's BSSID list.
//    
//      Inputs: card    - pointer to card structure
//              ridBss  - pointer to RID_BSS structure.
//              eol     - pointer to boolean to receive end of list status.
//                        Only meaningful if this function returns TRUE.
//                        May be NULL if don't care.
//    
//     Returns: TRUE if successful, FALSE otherwise.
//    
//      (12/05/00)
//---------------------------------------------------------------------------
{
    NdisZeroMemory(ridBss, sizeof(RID_BSS));
    BOOLEAN retval = RidGet(card, RID_GET_FIRST_BSS, ridBss, sizeof(RID_BSS));
    if (retval) {
        retval = (ridBss->length == sizeof(RID_BSS));
        if (eol) {
            *eol = (ridBss->index == 0xFFFF);
            }
/*
        retval = (ridBss->length == sizeof(RID_BSS)) && (ridBss->index != 0xFFFF); // end of list value
*/
        }
    return retval;
}

//===========================================================================
    BOOLEAN cmdGetNextBSS (PCARD card, RID_BSS *ridBss, BOOLEAN *eol /* = NULL */)
//===========================================================================
// 
// Description: Get the nexst entry in the card's BSSID list.
//    
//      Inputs: card    - pointer to card structure
//              ridBss  - pointer to RID_BSS structure.
//              eol     - pointer to boolean to receive end of list status.
//                        Only meaningful if this function returns TRUE.
//                        May be NULL if don't care.
//    
//     Returns: TRUE if successful, FALSE otherwise (end of list).
//    
//      (12/05/00)
//---------------------------------------------------------------------------
{
    NdisZeroMemory(ridBss, sizeof(RID_BSS));
    BOOLEAN retval = RidGet(card, RID_GET_NEXT_BSS, ridBss, sizeof(RID_BSS));
    if (retval) {
        retval = (ridBss->length == sizeof(RID_BSS));
        if (eol) {
            *eol = (ridBss->index == 0xFFFF);
            }
        }
    return retval;
}

//========================================================================
BOOLEAN         
EEReadConfigCmd(PCARD card)
{
    card->m_cmd = CMD_X500_EEReadConfig;
    if( ! exec(card, 1000) )
        return FALSE;
    
    int     delay = 1000 * 1000; //  1 sec
    for( int i=0; i<delay; i+=10 ){

        if( IsCmdComplete(card) )
            return TRUE;
        DelayUS(10);
    }
    return FALSE;
}

BOOLEAN         
EEWriteConfigCmd(PCARD card)
{
    card->m_cmd = CMD_X500_EEWriteConfig;
    if( ! exec(card, 1000) )
        return FALSE;
    
    int     delay = 1000 * 1000; //  1 sec
    for( int i=0; i<delay; i+=10 ){

        if( IsCmdComplete(card) )
            return TRUE;
        DelayUS(10);
    }
    return FALSE;
}
//========================================================================
BOOLEAN         
EEReadConfig(PCARD card, CFG_X500 * cfg)
{
    return EEReadConfigCmd(card) ? cmdConfigGet(card, cfg) : FALSE;
}
BOOLEAN         
EEWriteConfig(PCARD card, CFG_X500 * cfg)
{
    return  cmdConfigSet(card, cfg, FALSE) ? (EEWriteConfigCmd(card) ? cmdEnable(card) : FALSE) : FALSE;
}
//========================================================================
BOOLEAN         
EEReadSSIDS(PCARD card, STSSID * ssid)
{
    return EEReadConfigCmd(card) ? cmdSSIDGet(card, ssid) : FALSE;
}
BOOLEAN         
EEWriteSSIDS(PCARD card, STSSID * ssid)
{
    return  cmdSSIDSet(card, ssid, FALSE) ? (EEWriteConfigCmd(card) ? cmdEnable(card) : FALSE) : FALSE;
}
//========================================================================
BOOLEAN         
EEReadAPS(PCARD card, STAPLIST  * aps)
{
    return EEReadConfigCmd(card) ? cmdAPsSet(card, aps) : FALSE;
}
BOOLEAN         
EEWriteAPS(PCARD card, STAPLIST  * aps)
{
    return  cmdAPsSet(card, aps, FALSE) ? (EEWriteConfigCmd(card) ? cmdEnable(card) : FALSE) : FALSE;
}

 
//===========================================================================
    NDIS_STATUS cmdAddWEP (PCARD card, NDIS_802_11_WEP *WepKey)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
#if DBG 
    DbgPrint( "Key Index field = %0X\n", WepKey->KeyIndex );
    DbgPrint( "Key length field = %0X\n", WepKey->KeyLength );

//    DbgPrint( "Key = ");
//    for( USHORT i=0; i<WepKey->KeyLength && i<16; i++ ) 
//        DbgPrint( "%02X ", WepKey->KeyMaterial[i] );
//    DbgPrint( "\n");
#endif

    RID_WEP_Key     RKey = {sizeof(RID_WEP_Key),0,{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 }};

    if( (0x7FFFFFFF & WepKey->KeyIndex) > 3 )
        return NDIS_STATUS_INVALID_DATA;
        
        
    RKey.keyindex   = (USHORT)(0xF & WepKey->KeyIndex);
    RKey.keylen     = (USHORT)WepKey->KeyLength;    

    NdisMoveMemory(RKey.key, WepKey->KeyMaterial, WepKey->KeyLength );  
    
    if( ! RidSetIgnoreMAC(card, RID_WEP_SESSION_KEY, &RKey, WepKey->Length ) )
        return NDIS_STATUS_NOT_SUPPORTED ;

    if( 0==(0x80000000  & WepKey->KeyIndex) )
        return NDIS_STATUS_SUCCESS;

    RKey.keyindex   = 0xFFFF;
    RKey.keylen     = (USHORT)WepKey->KeyLength;    
        
    NdisZeroMemory(RKey.macaddr, 6 );
    RKey.macaddr[0] = (UCHAR)(0x7F & WepKey->KeyIndex);

    return  RidSetIgnoreMAC(card, RID_WEP_SESSION_KEY, &RKey, WepKey->Length ) 
            ? NDIS_STATUS_SUCCESS : NDIS_STATUS_NOT_ACCEPTED;
}

//===========================================================================
    NDIS_STATUS cmdRemoveWEP (PCARD card, USHORT index)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    RID_WEP_Key     RKey    = {sizeof(RID_WEP_Key),0,{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 }};
    NDIS_STATUS     retval  = NDIS_STATUS_SUCCESS;

    if ((index >= 0) && (index <= MAX_ENTERPRISE_WEP_KEY_INDEX)) {
        RKey.keyindex   = index;
        RKey.keylen     = 0;    
        NdisMoveMemory(RKey.macaddr, "\1\0\0\0\0\0", 6);

        if (!RidSetIgnoreMAC(card, RID_WEP_SESSION_KEY, &RKey, RKey.ridlen)) {
            retval = NDIS_STATUS_NOT_SUPPORTED;
            }
        }
    else {
        retval = NDIS_STATUS_INVALID_DATA;
        }

    return retval;
}

//===========================================================================
    NDIS_STATUS cmdRestoreWEP (PCARD card)
//===========================================================================
// 
// Description: Restore WEP key from flash.
//    
//      Inputs: card - pointer to card structure.
//    
//     Returns: NDIS_STATUS_SUCCESS if successful, error code otherwise.
//    
//      (03/26/01)
//---------------------------------------------------------------------------
{
    RID_WEP_Key     RKey    = {sizeof(RID_WEP_Key),0,{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 }};
    NDIS_STATUS     retval  = NDIS_STATUS_SUCCESS;
    // 
    // Ley length of -1 indicates reload WEP keys from flash.
    // 
    RKey.keylen = (USHORT)-1;
    if (!RidSetIgnoreMAC(card, RID_WEP_SESSION_KEY, &RKey, RKey.ridlen)) {
        retval = NDIS_STATUS_NOT_SUPPORTED;
        }
    return retval;
}

/*
NDIS_STATUS 
cmdRemoveWEP( PCARD card)
{
    RID_WEP_Key     RKey = {sizeof(RID_WEP_Key),0,{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 }};
    
    for( int i = 0; i<4; i++ ){
    
        RKey.keyindex   = (USHORT)i;
        RKey.keylen     = 0;    
        NdisMoveMemory(RKey.macaddr, "\1\0\0\0\0\0", 6 );

        if( ! RidSetIgnoreMAC(card, RID_WEP_SESSION_KEY, &RKey, RKey.ridlen ) )
            return NDIS_STATUS_NOT_SUPPORTED;
    }

    return NDIS_STATUS_SUCCESS ;
}
*/


