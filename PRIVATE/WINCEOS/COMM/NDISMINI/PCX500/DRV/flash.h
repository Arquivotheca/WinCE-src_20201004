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
// flash.h
//---------------------------------------------------------------------------
// Description:
//
// Revision History:
//
// Date         
//---------------------------------------------------------------------------
//
// 08/01/01     spb027      -Added the enableinterrupts parameter to 
//                           FlashStartFlashing
//
//---------------------------------------------------------------------------

#ifndef __flash_h__
#define __flash_h__
//
#include "cmdX500.h"
//
typedef enum  {
        FLASHCMD_FAILED = -1,
        FLASHCMD_FINISHED=0,
        FLASHCMD_INPROGRESS=1,
}FLASHCMD_PROGRESS;

void    FlashInit(PCARD card);
void    FlashTimerCallback(IN PVOID ,PCARD,IN PVOID, IN PVOID);
void    FlashInitTimer(PCARD card);
int     FlashWrite1kBlock(PCARD card, char *block );
void    FlashCopyNextBlock(PCARD card, char *block );
BOOLEAN FlashGetChar( PCARD card, UCHAR byte, BOOLEAN match=TRUE, int us = 6000000 );
BOOLEAN FlashPutChar( PCARD card, UCHAR byte, int us = 500000 );
BOOLEAN FlashIsReady(PCARD card);
BOOLEAN FlashIsNewSegment(int m_block);
BOOLEAN FlashIsSegmentEnd(int m_block);
void    FlashStartFlashing(PCARD card,BOOLEAN enableInterrupts = TRUE); //spb027

#endif