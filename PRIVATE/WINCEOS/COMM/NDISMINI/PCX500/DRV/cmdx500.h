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
//CmdX500.h
#ifndef __CmdX500_h__
#define __CmdX500_h__

#include "HWX500P.h"
#include "support.h"
#include "AiroOid.h"
#include "card.h"
#include "802_11.h"

typedef enum _CMD_STATUS_X500{
    CMD_STATUS_NOTCMD,
    CMD_STATUS_SUCCESS,
    CMD_STATUS_NOTASSOC,
    CMD_STATUS_FAILURE,
    CMD_STATUS_CMDERROR,
    CMD_STATUS_UKNOWNERROR,
}CMD_STATUS_X500;
                                                                            
#define FID_BUSY        0x8000
#define FID_ERR         0x4000
#define FID_RDY         0x2000
#define RID_BUSY        FID_BUSY
#define RID_ERR         FID_ERR

#define IsFidBusy(us)   ((FID_BUSY & us))
#define IsFidErr(us)    ((FID_ERR  & us))   
#define IsFidRdy(us)    ( (FID_RDY  & us) && !IsFidBusy(us) && !IsFidErr(us) )
#define IsRidBusy(us)   IsFidBusy(us)
#define IsRidErr(us)    IsFidErr(us)    
#define IsRidRdy(us)    IsFidRdy(us)
BOOLEAN         exec(PCARD card);
BOOLEAN         exec(PCARD cardm, int delay);
BOOLEAN         exec(PCARD card, int delay, USHORT p0);
BOOLEAN         WaitComplete(PCARD card, int delay = 1000000 ); // delay in us, 1 second default             
BOOLEAN         WaitAllocComplete(PCARD card, int delay = 1000 );
CMD_STATUS_X500 GetCmdStatus(PCARD card, CMD_X500 cmd = (CMD_X500)0xFFFF);
void            WriteCommand(PCARD card);
void            WriteCommand(PCARD card, USHORT p0);
void            GetResponce(PCARD card, USHORT *p0,USHORT *p1=NULL,USHORT *p2=NULL);
BOOLEAN         IsCmdReady(PCARD card);          
BOOLEAN         IsEventAllocReady(PCARD card);
BOOLEAN         IsResponceReady(PCARD card);
BOOLEAN         IsCmdComplete(PCARD card);
BOOLEAN         IsAllocComplete(PCARD card);
void            AckCommand(PCARD card);
void            AckAlloc(PCARD card);
BOOLEAN         WaitBusy(PCARD card, int us = 1000 );
BOOLEAN         WaitLastCmd(PCARD card, int delay=0 );
BOOLEAN         BapSet(PCARD card, USHORT BAPOff, FID fid, int FidOff=0 );
void            BapMoveFidOffset(USHORT BAPOff,     USHORT FidOffNew);  // = 0 

inline void     CmdDisableInterrupts(PCARD card ){
                    USHORT  usWord;
                    NdisRawReadPortUshort( card->m_IOBase+REG_INT_EN, &usWord );
                    NdisRawWritePortUshort(card->m_IOBase+REG_INT_EN, 0x00 );
                    //spb024
                    //If m_IntActive is set, then it means we got an interrupt after
                    //reading active interrupts but before we were able to disable
                    //interrupts.  Therefore interupts really should be disabled
                    //when we return from this routine. 
                    //Trust me this happens.  (Caused lost fids at Microsoft)
                    if (card->m_IntActive ) {
                        usWord=0;
                    }

                    PUSH(usWord);

                    //NdisRawWritePortUshort(card->m_IOBase+REG_INT_EN, 0x00 );
                    //PUSH(card->m_IntReg);
                    //card->m_IntReg = 0;
                                    
                }

inline void     CmdRestoreInterrupts(PCARD card){
                        //NdisRawWritePortUshort(card->m_IOBase+REG_INT_EN, card->m_CmdIntMask );
                        NdisRawWritePortUshort(card->m_IOBase+REG_INT_EN, 
                                        (USHORT)POP() );
                        //NdisRawWritePortUshort(card->m_IOBase+REG_INT_EN, 
                        //              card->m_IntReg=(USHORT)POP() );
                    }
                    
inline void     ReadRespParam(PCARD card, REGX500 respOff, USHORT *param ){
                        NdisRawReadPortUshort( card->m_IOBase+respOff, param );
                }
    //

BOOLEAN         XmitOpen                (PCARD card, UINT param);
BOOLEAN         XmitClose               (PCARD card, UINT param=0);
FID             AllocNextFid            (PCARD card);
BOOLEAN         RidGet                  (PCARD card, USHORT m_RID, void *st, int dataLen);
BOOLEAN         RidSet                  (PCARD card, USHORT m_RID, void *st, int dataLen, BOOLEAN macEnable=TRUE);
BOOLEAN         RidSetIgnoreMAC         (PCARD card, USHORT m_RID, void *st, int dataLen );
BOOLEAN         cmdEnable               (PCARD card);
BOOLEAN         cmdDisable              (PCARD card);
BOOLEAN         cmdConfigGet            (PCARD card, CFG_X500 * cfg);
BOOLEAN         cmdConfigSet            (PCARD card, CFG_X500 * cfg = NULL, BOOLEAN macEnable=TRUE);
BOOLEAN         cmdSSIDGet              (PCARD card, STSSID * ssid);
BOOLEAN         cmdSSIDSet              (PCARD card, STSSID * ssid = NULL, BOOLEAN macEnable=TRUE);
BOOLEAN         cmdAPsGet               (PCARD card, STAPLIST  * aps );
BOOLEAN         cmdAPsSet               (PCARD card, STAPLIST  * aps = NULL, BOOLEAN macEnable=TRUE );
BOOLEAN         cmdCapsGet              (PCARD card,STCAPS *cap );
BOOLEAN         cmdCapsSet              (PCARD card,STCAPS *cap );
BOOLEAN         cmdStatusGet            (PCARD card,STSTATUS * status );
BOOLEAN         cmdStatusSet            (PCARD card,STSTATUS *status );
BOOLEAN         cmdStatisticsGet        (PCARD card,STSTATISTICS * stats );
BOOLEAN         cmdStatisticsClear      (PCARD card,STSTATISTICS *stats );
BOOLEAN         cmdStatistics32Get      (PCARD card,STSTATISTICS32 * stats );
BOOLEAN         cmdStatistics32Clear    (PCARD card,STSTATISTICS32 *stats );
BOOLEAN         cmdReset                (PCARD card);
BOOLEAN         cmdMagicPacket          (PCARD card, BOOLEAN enable=TRUE);
BOOLEAN         cmdSleep                (PCARD card );
BOOLEAN         cmdTrySleep             (PCARD card );
BOOLEAN         cmdAwaken               (PCARD card, BOOLEAN wait = FALSE );
BOOLEAN         EEReadConfigCmd         (PCARD card);
BOOLEAN         EEWriteConfigCmd        (PCARD card);
BOOLEAN         EEReadConfig            (PCARD card, CFG_X500 * cfg);
BOOLEAN         EEWriteConfig           (PCARD card, CFG_X500 * cfg);
BOOLEAN         EEReadSSIDS             (PCARD card, STSSID * ssid);
BOOLEAN         EEWriteSSIDS            (PCARD card, STSSID * ssid);
BOOLEAN         EEReadAPS               (PCARD card, STAPLIST  * aps);
BOOLEAN         EEWriteAPS              (PCARD card, STAPLIST  * aps);
BOOLEAN         cmdSetOpMode            (PCARD card, USHORT param );
NDIS_STATUS     cmdAddWEP               (PCARD card, NDIS_802_11_WEP *key);
NDIS_STATUS     cmdRemoveWEP            (PCARD card, USHORT index);
NDIS_STATUS     cmdRestoreWEP           (PCARD card);

DBM_TABLE*      cmdDBMTableGet          (PCARD card);
                             
BOOLEAN         cmdGetFirstBSS          (PCARD card, RID_BSS *ridBss, BOOLEAN *eol = NULL);
BOOLEAN         cmdGetNextBSS           (PCARD card, RID_BSS *ridBss, BOOLEAN *eol = NULL);
                                            
BOOLEAN         cmdBssidListScan        (PCARD card);




#endif //__CardCmd_h__

