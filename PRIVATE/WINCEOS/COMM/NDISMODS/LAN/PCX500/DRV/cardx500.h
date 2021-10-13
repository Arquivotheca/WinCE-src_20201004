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
// CardX500.h
//---------------------------------------------------------------------------
//
// Description:
//
// Revision History:
//
// Date         
//---------------------------------------------------------------------------
// 
// 08/01/01     spb027      -Defined iscardinserted in header file.
//                       
//
//---------------------------------------------------------------------------
#ifndef __cardX500_h__
#define __cardX500_h__

#include "Card.h"
#include "AiroDef.h"
#include "CmdX500.h"
#include "support.h"

void            SetUserConfig       (PCARD card, CFG_X500 &cfg);

void            SetUserSSID         (PCARD card, STSSID &ssid);

void            SetUserAPs          (PCARD card, STAPLIST &aps);

NDIS_STATUS     ResetCard           (PCARD card);


inline BOOLEAN isRxInterrupt        (PCARD card)    { return INT_EN_Rx & card->m_IntActive ? TRUE : FALSE; }

inline BOOLEAN isTxInterrupt        (PCARD card)    { return INT_EN_Tx & card->m_IntActive ? TRUE : FALSE; }

inline BOOLEAN isTxExceptInterrupt  (PCARD card)    { return INT_EN_TxExc & card->m_IntActive ? TRUE : FALSE; }
    
inline BOOLEAN isCmdInterrupt       (PCARD card)    { return EVNT_ACK_CMD & card->m_IntActive ? TRUE : FALSE; }

inline BOOLEAN isAwakeInterrupt     (PCARD card)    { return !card->IsAwake && (EVNT_STAT_IsAWAKE & card->m_IntActive) ? TRUE : FALSE; }

inline BOOLEAN isAsleepInterrupt    (PCARD card)    { return EVNT_STAT_IsASLEEP & card->m_IntActive ? TRUE : FALSE; }
    
inline BOOLEAN isLinkInterrupt      (PCARD card)    { return  INT_EN_LINK & card->m_IntActive ? TRUE : FALSE; }
    
inline BOOLEAN IsInterrupt          (PCARD card)    {
                                                        USHORT  usWord;
                                                        NdisRawReadPortUshort( card->m_IOBase+REG_INT_STAT, &usWord );
                                                        return 0!=usWord;
                                                    }

inline void StoreHostActiveInterrupts (PCARD card)
{
    NdisRawReadPortUshort(card->m_IOBase+REG_INT_STAT, &card->m_IntActive);
}
    
inline USHORT GetHostInterrupts (PCARD card)
{
    USHORT  usWord;
    NdisRawReadPortUshort(card->m_IOBase+REG_INT_STAT, &usWord);
    return usWord;
}

//spb002
inline USHORT GetMaskInterrupts (PCARD card)
{
    USHORT  usWord;
    NdisRawReadPortUshort(card->m_IOBase+REG_INT_EN, &usWord);
    return usWord;
}

inline void EnableHostInterrupts (PCARD card) 
{
    if( card->KeepAwake )
        card->m_IntMask &= ~INT_EN_AWAKE;   
//  card->m_IntReg = card->m_IntMask;
    NdisRawWritePortUshort(card->m_IOBase+REG_INT_EN, card->m_IntMask);
}

inline void disableHostInterrupts (PCARD card)
{
    //NdisRawReadPortUshort( card->m_IOBase+REG_INT_EN, &card->m_IntMask );
    //card->m_IntReg = 0;
    NdisRawWritePortUshort( card->m_IOBase+REG_INT_EN, 0x00 );
}
    
//spb024 void    EnableRxInterrupt       (PCARD card, BOOLEAN enable =TRUE);
//spb024 void    EnableTxInterrupt       (PCARD card, BOOLEAN enable =TRUE);
//spb024 void    EnableTxExceptInterrupt (PCARD card, BOOLEAN enable =TRUE);
//spb024 void    EnableCmdInterrupt      (PCARD card, BOOLEAN enable =TRUE);
//spb024 void    EnableAwakeInterrupt    (PCARD card, BOOLEAN enable =TRUE); 

inline void AckRcvInterrupt (PCARD card)
{
    NdisRawWritePortUshort( card->m_IOBase+REG_INT_ACK, EVNT_ACK_Rx);
    card->m_IntActive &= ~EVNT_STAT_Rx;
}
    
inline void AckTxInterrupt (PCARD card)
{
    NdisRawWritePortUshort( card->m_IOBase+REG_INT_ACK, EVNT_ACK_Tx);
    card->m_IntActive &= ~EVNT_STAT_Tx;
}

inline void AckTxExceptInterrupt (PCARD card)
{
    NdisRawWritePortUshort( card->m_IOBase+REG_INT_ACK, EVNT_ACK_TxExc);
    card->m_IntActive &= ~EVNT_STAT_TxExc;
}

inline void AckAwakeInterrupt (PCARD card) 
{
    // do not write to register, because a write toggles thes state
    card->m_IntActive &= ~EVNT_STAT_IsAWAKE;
}

inline void AckLinkInterrupt (PCARD card)
{
    NdisRawWritePortUshort( card->m_IOBase+REG_INT_ACK, EVNT_ACK_LINK);
    card->m_IntActive &= ~EVNT_ACK_LINK;
}

inline void AckAsleepInterrupt (PCARD card)
{
    NdisRawWritePortUshort( card->m_IOBase+REG_INT_ACK, EVNT_ACK_SLEEP);
    card->m_IntActive &= ~EVNT_STAT_IsASLEEP;
}

inline void AckCmdInterrupt (PCARD card)
{
    NdisRawWritePortUshort( card->m_IOBase+REG_INT_ACK, EVNT_ACK_CMD);
    card->m_IntActive &= ~EVNT_STAT_CMD;
    card->m_PrevCmdDone = TRUE;
    card->m_PrevCmdTick = 0;
    return;
    
//spb024  just comment it out, return was there before
/*    if (CMD_X500_Transmit == card->m_PrevCommand) {
        USHORT  usWord;
        NdisRawReadPortUshort( card->m_IOBase+REG_CMD_STATUS, &usWord );
                            
        if (0x7F00 & usWord) {  // error
            int     tmp;
            NdisRawReadPortUshort( card->m_IOBase+REG_CMD_RESP0, &usWord );
            switch(usWord ){
                case 3:       //invalid fid
                    tmp = 1;
                    break;
                case 2:
                case 5:
                    NdisRawReadPortUshort( card->m_IOBase+REG_CMD_RESP1, &usWord );
                    CQStore( card->fidQ, usWord );
                    //++card->m_CanSend;
                    break;
                default:
                    break;
                }
            }
        }
*/
}

BOOLEAN     RxFidSetup              (PCARD card);
void        UpdateCounters          (PCARD card);
BOOLEAN     FindCard                (PCARD card);

NDIS_STATUS ExtendedOids            (IN NDIS_OID    Oid,
                                     PCARD          card,
                                     IN PVOID       InfBuff,
                                     IN ULONG       InfBuffLen,
                                     OUT PULONG     BytesWritten,
                                     OUT PULONG     BytesNeeded);

void        CardShutdown            (PCARD card);
void        InitStatusChangeReg     (PCARD card);
void        UpdateLinkStatus        (PCARD card);
void        CheckAutoConfig         (PCARD card);
void        UpdateLinkSpeed         (PCARD card);
void        InitInterrupts          (PCARD card);
void        SetPromiscuousMode      (PCARD, ULONG);

//spb027
BOOLEAN  IsCardInserted(PCARD card);
#endif