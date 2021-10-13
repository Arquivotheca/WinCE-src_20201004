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
// flash.cpp
//---------------------------------------------------------------------------
// Description:
//
// Revision History:
//
// Date         
//---------------------------------------------------------------------------
// spb026   7/23/01     - Changed the way the flashing is done so it
//                        Can be used without a timer.
//
// spb027   08/01/01    - Added the ability to cancel the flashing
//
//---------------------------------------------------------------------------

#pragma code_seg("LCODE")
#include "NDISVER.h"

extern "C"{
    #include    <ndis.h>
}

#include "CmdX500.h"
#include "Cardx500.h"
#include "HWX500.h"
#include "support.h"
#include "memory.h"
#include "Airodef.h"
#include "flash.h"

void InitInterrupts( PCARD card );
//#define   ESTIMATED_FLASHING_TIME  (14*1000)
#define ESTIMATED_FLASHING_TIME  (15*1000)
//
void
FlashInit(PCARD card )
{
    FLASH_STRUCT        *p = &card->FlashSt;
    STFLASH_PROGRESS    tmp = {FLASHING_PROGRESS_OPEN,0,ESTIMATED_FLASHING_TIME};

    p->m_delay          = 0;
    p->m_state          = 1;
    p->m_block          = 0;
    p->m_ImageBufOff    = 0;
    BOOLEAN             m_useTimer=FALSE;       //spb026
    p->m_FlashStateProc = (void (__stdcall *)(void))FlashStartFlashing; 
    
    card->m_FlashProgress = tmp;
    CmdDisableInterrupts(card);
}

void
FlashTimerCallback(
    IN PVOID        SystemSpecific1,        // ignored
    PCARD           card,
    IN PVOID        SystemSpecific2,        // ignored
    IN PVOID        SystemSpecific3         // ignored
    )
{
    FLASH_STRUCT    *p = &card->FlashSt;
    
    if( ! p )
        return;

    //(p->m_FlashStateProc)(card);
    FlashStartFlashing(card);
  
    if( FLASHCMD_FINISHED   == p->m_Progress ||
        FLASHCMD_FAILED     == p->m_Progress ){
        card->m_IsFlashing = FALSE;
    }
}


void                
FlashInitTimer(PCARD card)
{
    FLASH_STRUCT    *p = &card->FlashSt;

    p->m_useTimer=TRUE;     //spb026
    NdisMInitializeTimer( &p->m_timer, card->m_MAHandle, 
        (void (*)(void *,void *,void *,void *))FlashTimerCallback, card  ); 
}

    
int                 
FlashWrite1kBlock(PCARD card, char *block )
{
    FLASH_STRUCT    *p = &card->FlashSt;

    if( p->m_block >= 128 )         // 128 blocks max
        return -2;
    
    if( FlashIsNewSegment(p->m_block) ){
        if( p->m_block && !FlashIsReady(card))
            return -1;

        NdisRawWritePortUshort( card->m_IOBase+REG_AUX_PAGE, 0x100 );
        NdisRawWritePortUshort( card->m_IOBase+REG_AUX_OFF, 0x00 );
    }
    NdisRawWritePortBufferUshort(card->m_IOBase+REG_AUX_DATA, block, 512 );

    if( FlashIsSegmentEnd(p->m_block) ){
        NdisRawWritePortUshort( card->m_IOBase+REG_SWS0, 0x8000 );
    }
    ++p->m_block;

    return 0;           
}

void                
FlashCopyNextBlock(PCARD card, char *block )
{
    FLASH_STRUCT    *p = &card->FlashSt;

    if(  p->m_ImageBufOff >= 0 && p->m_ImageBufOff <= (1024*127) ){
        NdisMoveMemory( p->m_ImageBuf+p->m_ImageBufOff, block, 1024 );
        p->m_ImageBufOff += 1024;
    }
}

BOOLEAN             
//FlashGetChar( PCARD card, UCHAR byte, BOOLEAN match=TRUE, int us = 6000000 )
FlashGetChar( PCARD card, UCHAR byte,  BOOLEAN match, int us )
{
    USHORT          usWord;
    BOOLEAN         success = FALSE;

    do{ 
        NdisRawReadPortUshort( card->m_IOBase+REG_SWS1, &usWord );  // read ints enable

        if( us && !(0x8000 & usWord) ){
            us -= 50;
            DelayUS( 50 );
            continue;
        }
        
        UCHAR   tmp = 0xFF & usWord;

        if( (!match || byte == tmp) && (0x8000 & usWord) ){
            NdisRawWritePortUshort( card->m_IOBase+REG_SWS1, 0 );
            success = TRUE;
            break;
        }
        if( 0x81==tmp || 0x82==tmp || 0x83==tmp || 0x1A==tmp || 0xFFFF == usWord ) 
            break;

        #if DBG
            DbgPrint("Flash Read %c(%x)\n",tmp,tmp);
        #endif
        NdisRawWritePortUshort( card->m_IOBase+REG_SWS1, 0 );
    }while( us > 0 );
    
    return success;
}

BOOLEAN             
//FlashPutChar( UCHAR byte, int us = 500000 )
FlashPutChar( PCARD card, UCHAR byte, int us )
{   
    USHORT  usWord;
    USHORT  usOut   = 0x8000 | byte;

    NdisRawWritePortUshort( card->m_IOBase+REG_SWS0, usOut );
    do{ 
        DelayUS( 50 );
        us -= 50;
        NdisRawReadPortUshort( card->m_IOBase+REG_SWS1, &usWord );  
    }while(  us && usWord != usOut );   
    
    NdisRawWritePortUshort( card->m_IOBase+REG_SWS1, 0 );
    return usWord == usOut;
}


BOOLEAN             
FlashIsReady(PCARD card)
{
    USHORT  usWord = 0;
    int NotTimedout     = 1000;     // 1 second timeout

    while( NotTimedout-- && 0==(usWord & 0x8000) ){
        NdisRawReadPortUshort( card->m_IOBase+REG_SWS1, &usWord );
        DelayMS( 1 );       // i MSecond delay
    }
    
    NdisRawWritePortUshort( card->m_IOBase+REG_SWS1, 0 );
    return 0x801B==usWord && NotTimedout;
}   

BOOLEAN             
FlashIsNewSegment(int m_block)
{
    return 0==m_block || 32==m_block || 64==m_block || 96==m_block; 
}   

BOOLEAN             
FlashIsSegmentEnd(int m_block)
{
    return 31==m_block || 63==m_block || 95==m_block || 127==m_block ; 
}   

#define TICK    10
void                
FlashStartFlashing(PCARD card, BOOLEAN enableInterrupts /*TRUE*/)
{
    FLASH_STRUCT    *p = &card->FlashSt;

    p->m_Progress   = FLASHCMD_INPROGRESS;

    //spb027   See if the flash is canceled or somebody pulled the card out
    if ((card->cancelFlash && ( 5 > p->m_state || 11 < p->m_state ))  ||
         FALSE == IsCardInserted(card)) {
        card->m_FlashProgress.percent_complete=100;
        if (2 > p->m_state) {
            p->m_Progress = FLASHCMD_FAILED;
        }
        else {
            p->m_Progress = FLASHCMD_FINISHED;
        }

        card->m_IsFlashing = FALSE;
        return;        
    }
    
    if (p->m_useTimer) {    //spb026
        int delay = TICK < p->m_delay ? TICK : p->m_delay <= 0 ? 1 : p->m_delay;

        card->m_FlashProgress.mseconds_remain   -= delay;
        card->m_FlashProgress.percent_complete = 
            100 - (card->m_FlashProgress.mseconds_remain * 100 /ESTIMATED_FLASHING_TIME);

        p->m_delay -= delay;

        if( p->m_delay > 0 ) {
            NdisMSetTimer(&p->m_timer, delay );
            return;
        }
    }
        
    p->m_delay = TICK;

    switch(p->m_state ) {
    case 1:
        #if DBG
            DbgPrint("Flash reset\n");
        #endif
        card->m_PrevCmdDone = TRUE;
        card->m_IsPolling   = TRUE;
        cmdReset( card );
        p->m_delay = 500;
        break;
        
    case 2:
        NdisRawWritePortUshort( card->m_IOBase+REG_SWS0, 0x7E7E );
        NdisRawWritePortUshort( card->m_IOBase+REG_SWS1, 0x7E7E );
        NdisRawWritePortUshort( card->m_IOBase+REG_CMD, CMD_X500_NOP10);
        p->m_delay = 500;//1000;//5000;
        break;

    case 3:
        #if DBG
            DbgPrint("Flash Wait\n");
        #endif
        if( ! WaitBusy(card)  ) { 
            #if DBG
                DbgPrint("Flash Wait Failed\n");
            #endif
            p->m_Progress   = FLASHCMD_FAILED;
        }

        break;
    case 4:     
        if( ! FlashGetChar( card, '*' ))
            p->m_Progress   = FLASHCMD_FAILED;

        break;
            
    case 5:
        if( ! FlashPutChar( card, 'L' ) )
            p->m_Progress   = FLASHCMD_FAILED;
        break;
        
    case 6:
        if( ! FlashPutChar( card, '\r' ) )
            p->m_Progress   = FLASHCMD_FAILED;

        p->m_delay = 7000;     //5 seconds 
        break;
        
    case 7:
        if( !FlashGetChar(card, 0x1b) ) 
            p->m_Progress   = FLASHCMD_FAILED; 
        break;
    
    case 8:
        #if DBG
            DbgPrint("Write 1k Block\n");
        #endif
        FlashWrite1kBlock( card, p->m_ImageBuf+p->m_block*1024);
        if( p->m_block < 128 ){
            --p->m_state;
        }
        else
            //p->m_delay = 1000;       // MUST BE >= 1000
            p->m_delay = 2000;     // MUST BE >= 1000
        break;
    
    case 9:
        if( !FlashGetChar(card, '*') ) 
            p->m_Progress   = FLASHCMD_FAILED;
        break;

    case 10:
        if( ! FlashPutChar( card, 'g' ) )
            p->m_Progress   = FLASHCMD_FAILED;
        break;

    case 11:
        if( ! FlashPutChar( card, '\r' ) ) {
            p->m_Progress   = FLASHCMD_FAILED;
        }
        else {
            p->m_delay = 500;     // MUST BE >= 1000
        }
        break;

    case 12:
        #if DBG
            DbgPrint("Wait Complete\n");
        #endif
        NdisRawWritePortUshort( card->m_IOBase+REG_CMD, CMD_X500_NOP10);
        //if( ! WaitComplete(card) )
        //  p->m_Progress = FLASHCMD_FAILED;        
        WaitComplete(card); 
        AckCommand(card);
//        p->m_delay = 1000;     // MUST BE >= 1000
        break;
    
    case 13:
//        WaitComplete(card);
//        AckCommand(card);
        #if DBG
            DbgPrint("InitFW \n");
        #endif
        p->m_Progress   = InitFW(card) ? FLASHCMD_FINISHED : FLASHCMD_FAILED;
        if (enableInterrupts)       //spb027
            InitInterrupts( card );
        break;
        
    }

    ++p->m_state; 

    if( FLASHCMD_INPROGRESS == p->m_Progress ){  
        if (p->m_useTimer) {    //spb026
            int delay = TICK < p->m_delay ? TICK : p->m_delay <= 0 ? 1 : p->m_delay;
            NdisMSetTimer(&p->m_timer, delay );
        }

        switch( p->m_state ){
        case 1:case 2: case 3: case 4: case 5: case 6: case 7: 
            card->m_FlashProgress.progress =  FLASHING_PROGRESS_ERASING;
            break;

        case 8:
            card->m_FlashProgress.progress =  FLASHING_PROGRESS_WRITING;
            break;
        
        default:
            card->m_FlashProgress.progress =  FLASHING_PROGRESS_REBOOTING;
            break;
        }
    }
    else {
        card->m_IsFlashing = FALSE;
        if( FLASHCMD_FAILED== p->m_Progress )
            card->m_FlashProgress.progress = FLASHING_PROGRESS_FAILED;
        else {
            card->m_FlashProgress.progress = FLASHING_PROGRESS_SUCCESS;
            #if DBG
                DbgPrint("Flash Successful\n");
            #endif
        }
    }
}
