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
//  HWX500P.h
#ifndef __HWX500P_H__
#define __HWX500P_H__
//#include "support.h"
// 
//
#include "hwX500.h"
//
#define MPA_STAUSCHANGE     0x0001
#define MPA_PASSTHROUGH     0x0002
#define MPM_NOMULTICAST     0x0100
#define MPM_NOBROADCAST     0x0200
#define MPM_CAM             0x0400
//
typedef unsigned short      FID; 
#define BAPOFF_OFFSET_BAPX  4
//====================================================
//  X500 Int, Ack, status registers
#define     EVNT_STAT_Rx        (0x01<<0)       // 0x0001
#define     EVNT_STAT_Tx        (0x01<<1)       // 0x0002
#define     EVNT_STAT_TxExc     (0x01<<2)       // 0x0004
#define     EVNT_STAT_Alloc     (0x01<<3)       // 0x0008
#define     EVNT_STAT_CMD       (0x01<<4)       // 0x0010
#define     EVNT_STAT_TAL       (0x01<<6)       // 0x0040
#define     EVNT_STAT_LINK      (0x01<<7)       // 0x0080
#define     EVNT_STAT_IsAWAKE   (0x01<<8)       // 0x0100
#define     EVNT_STAT_IsASLEEP  (0x01<<13)      // 0x2000
#define     EVNT_STAT_TICK      (0x01<<15)      // 0x8000
//
#define     INT_EN_Rx           (0x01<<0)       // 0x0001
#define     INT_EN_Tx           (0x01<<1)       // 0x0002
#define     INT_EN_TxExc        (0x01<<2)       // 0x0004
#define     INT_EN_Alloc        (0x01<<3)       // 0x0008
#define     INT_EN_CMD          (0x01<<4)       // 0x0010
#define     INT_EN_TAL          (0x01<<6)       // 0x0040
#define     INT_EN_LINK         (0x01<<7)       // 0x0080
#define     INT_EN_AWAKE        (0x01<<8)       // 0x0100
#define     INT_EN_SLEEP        (0x01<<13)      // 0x2000
#define     INT_EN_TICK         (0x01<<15)      // 0x8000
//
#define     EVNT_ACK_Rx         (0x01<<0)       // 0x0001
#define     EVNT_ACK_Tx         (0x01<<1)       // 0x0002
#define     EVNT_ACK_TxExc      (0x01<<2)       // 0x0004
#define     EVNT_ACK_Alloc      (0x01<<3)       // 0x0008
#define     EVNT_ACK_CMD        (0x01<<4)       // 0x0010
#define     EVNT_ACK_TAL        (0x01<<6)       // 0x0040
#define     EVNT_ACK_LINK       (0x01<<7)       // 0x0080
#define     EVNT_DO_SLEEP       (0x01<<8)       // 0x0100
#define     EVNT_DO_WAKEUP      (0x01<<13)      // 0x2000
#define     EVNT_ACK_SLEEP      EVNT_DO_WAKEUP  // 0x2000
#define     EVNT_ACK_TICK       (0x01<<15)      // 0x8000


// 
// Scan modes.
// 
#define     SCANMODE_ACTIVE         0
#define     SCANMODE_PASSIVE        1
#define     SCANMODE_AIRO_ACTIVE    2


//
//====================================================
//      X500 commands
typedef enum {
    CMD_X500_NOP                    = 0x0000,
    CMD_X500_NOP10                  = 0x0010,
    CMD_X500_Initialize             = 0x0000,
    CMD_X500_Enable                 = 0x0001,
    CMD_X500_EnableMAC              = 0x0001,
    CMD_X500_DisableMAC             = 0x0002,
    CMD_X500_EnableRcv              = 0x0201,
    CMD_X500_EnableEvents           = 0x0401,
    CMD_X500_EnableAll              = 0x0701,
    CMD_X500_Disable                = 0x0002,
    CMD_X500_Diagnose               = 0x0003,
    CMD_X500_Allocate               = 0x000a,
    CMD_X500_Transmit               = 0x000b,
    CMD_X500_Dellocate              = 0x000c,
    CMD_X500_AccessRIDRead          = 0x0021,
    CMD_X500_AccessRIDWrite         = 0x0121,
    CMD_X500_EEReadConfig           = 0x0008,
    CMD_X500_EEWriteConfig          = 0x0108,
    CMD_X500_Preserve               = 0x0000,
    CMD_X500_Program                = 0x0000,
    CMD_X500_ReadMIF                = 0x0000,
    CMD_X500_WriteMIF               = 0x0000,
    CMD_X500_Configure              = 0x0000,
    CMD_X500_SMO                    = 0x0000,
    CMD_X500_GMO                    = 0x0000,
    CMD_X500_Validate               = 0x0000,
    CMD_X500_UpdateStatistics       = 0x0000,
    CMD_X500_ResetStatistics        = 0x0000,
    CMD_X500_RadioTransmitterTests  = 0x0000,
    CMD_X500_GotoSleep              = 0x0000,
    CMD_X500_SyncToBSSID            = 0x0000,
    CMD_X500_AssocedToAP            = 0x0000,
    CMD_X500_ResetCard              = 0x0004,           // (Go to download mode) 
    CMD_X500_SiteSurveyMode         = 0x0000,
    CMD_X500_SLEEP                  = 0x0005,
    CMD_X500_MagicPacketON          = 0x0086,
    CMD_X500_MagicPacketOFF         = 0x0186,
    CMD_X500_SetOperationMode       = 0x0009,       // CAM, PSP, ..
    CMD_X500_BssidListScan          = 0x0103,
}CMD_X500;

typedef enum _RID_X500{
    RID_DBM_TABLE       = 0xFF04,
    RID_CONFIG          = 0xFF10,
    RID_SSID            = 0xFF11,
    RID_APP             = 0xFF12,
    RID_WEBKEYS,
    RID_CAP             = 0xFF00,
    RID_STATUS          = 0xFF50,
    RID_STATS           = 0xFF61,
    RID_STATS_CLEAR     = 0xFF62,
    RID_STATS32         = 0xFF69,
    RID_STATS32_CLEAR   = 0xFF6A,
    RID_WEP_SESSION_KEY = 0xFF15,
    RID_GET_FIRST_BSS   = 0xFF72,
    RID_GET_NEXT_BSS    = 0xFF73,

}RID_X500;
//
//=======================================================
//  IO Offsets
typedef enum {
    REG_CMD         = 0x00,
    REG_CMD_P0      = 0x02,
    REG_CMD_P1      = 0x04,
    REG_CMD_P2      = 0x06,
    REG_CMD_STATUS  = 0x08,
    REG_CMD_RESP0   = 0x0A,
    REG_CMD_RESP1   = 0x0C,
    REG_CMD_RESP2   = 0x0E,
    REG_FID_RX      = 0x20,
    REG_FID_TX_ALLOC= 0x22,
    REG_FID_TX_COMP = 0x24,
//
    REG_SPARE       = 0x16,

//
    REG_BAP0        = 0x18,
    REG_BAP0_SEL    = 0x18,
    REG_BAP0_OFF    = 0x1C,
    REG_BAP0_DATA   = 0x36,
//
    REG_BAPrx       = REG_BAP0,
    REG_BAPrx_SEL   = REG_BAP0_SEL,
    REG_BAPrx_OFF   = REG_BAP0_OFF,
    REG_BAPrx_DATA  = REG_BAP0_DATA,
//
    REG_BAP1_SEL    = 0x1A,
    REG_BAP1        = 0x1A,
    REG_BAP1_OFF    = 0x1E,
    REG_BAP1_DATA   = 0x38,
//
    REG_BAPtx       = REG_BAP1,
    REG_BAPtx_SEL   = REG_BAP1_SEL,
    REG_BAPtx_OFF   = REG_BAP1_OFF,
    REG_BAPtx_DATA  = REG_BAP1_DATA,
//
    REG_INT_STAT    = 0x30,
    REG_INT_EN      = 0x32,
    REG_INT_ACK     = 0x34,
    REG_LINKSTATUS  = 0x10,
    REG_CTRL        = 0x14,
    REG_SWS0        = 0x28,
    REG_SWS1        = 0x2A,
    REG_SWS2        = 0x2C,
    REG_SWS3        = 0x2E,
    REG_AUX_PAGE    = 0x3A,
    REG_AUX_OFF     = 0x3C,
    REG_AUX_DATA    = 0x3E
}REGX500; 

//
//==========================================================
//  card structures
#pragma pack( push, struct_pack1 )
#pragma pack( 1 )

typedef struct _TXHDR_CTRL {
    u16     SWSupport0; //0
    u16     SWSupport1; //2
    u16     status;     //4
    u16     DataLen;    //6
    u16     TxCtrl;     //8
    u16     AID;        //10
    u16     TxRetries;  //12
    u16     res1;       //14    
}TXHDR_CTRL;
//
//.......................
//
typedef struct _RXHDR_CTRL {
    u16     SWSupport0; //0
    u16     SWSupport1; //2
    u16     status;     //4
    u16     DataLen;    //6
    u8      silence;    //8
    u8      signal;     //9
    u8      rate;       //10
    u8      reserved;   //11
    u16     unused0;    //12
    u16     unused1;    //14
}RXHDR_CTRL;
//
//.......................
//
typedef struct _HDR_802_11 {
    u16         FrmCtrl;
    u16         durtation;
    MacAddr     Addr1;
    MacAddr     Addr2;
    MacAddr     Addr3;
    u16         sequence;
    MacAddr     Addr4;
}HDR_802_11;
//
//.......................
//
typedef struct _HDR_802_3 {
    MacAddr     destAddr;
    MacAddr     srcAddr;
}HDR_802_3;
//
//.......................
//
typedef struct _TXFID_HDR{
    TXHDR_CTRL  txHdr;
    u16         Plcp0;
    u16         Plcp1;
    HDR_802_11  hdr_802_11;
    u16         GepLen;
    u16         status;
}TXFID_HDR;
//
//.......................
//
typedef struct _RXFID_HDR{
    TXHDR_CTRL  rxHdr;
    u16         Plcp0;
    u16         Plcp1;
    HDR_802_11  hdr_802_11;
    u16         GepLen;
}RXFID_HDR;

//---------------------------------------------------------------------------
// dBm table stored in card.
//---------------------------------------------------------------------------
struct DBM_TABLE {
    USHORT  length;
    USHORT  table[256];
    };

//---------------------------------------------------------------------------
// Software capabilities from capabilities RID (offset = 0x7C).
//---------------------------------------------------------------------------
#define SOFT_CAPS_AUTO_WAKE_UP_SUPPORT      0x0001
#define SOFT_CAPS_WEP_ENCRYPTION_SUPPORT    0x0002
#define SOFT_CAPS_SIGNAL_QUALITY_SUPPORT    0x0004
#define SOFT_CAPS_NORMALIZED_RSSI_SUPPORT   0x0008
#define SOFT_CAPS_SERIAL_DEEP_SLEEP_SUPPORT 0x0010
#define SOFT_CAPS_SITE_SURVEY_MODE_SUPPORT  0x0020
#define SOFT_CAPS_WEEP_AUTO_RATE_SUPPORT    0x0040
#define SOFT_CAPS_4_WEP_KEY_SUPPORT         0x0080
#define SOFT_CAPS_128_BIT_WEP_SUPPORT       0x0100

//---------------------------------------------------------------------------
// Authentication types from configuration RID (offset = 0x3E).
//---------------------------------------------------------------------------
#define AUTH_TYPE_OPEN                      0x0001
#define AUTH_TYPE_SHARED_KEY                0x0002
#define AUTH_TYPE_WEP                       0x0100
#define AUTH_TYPE_ALLOW_MIXED_CELLS         0x0200
#define AUTH_TYPE_EAP                       0x0400
#define AUTH_TYPE_EAP_SECURE                0x0800
#define AUTH_TYPE_LEAP                      0x1000

//---------------------------------------------------------------------------
// Bit masks for capability field in RID_BSS.
//---------------------------------------------------------------------------
#define CAP_ESS         (0x01<<0)   // 0x0001   infrastructure
#define CAP_IBSS        (0x01<<1)   // 0x0002   IBSS adhoc
#define CAP_PRIVACY     (0x01<<4)   // 0x0010   encrypted only cell
#define CAP_SHORTHDR    (0x01<<5)   // 0x0020   short preamble capable cell

//---------------------------------------------------------------------------
// Bit masks for radioType field in RID_BSS
//---------------------------------------------------------------------------
#define RADIO_TYPE_FH       (0x01<<0)   // 0x0001   802.11 FH
#define RADIO_TYPE_DS       (0x01<<1)   // 0x0002   802.11 DS
#define RADIO_TYPE_TMA_DS   (0x01<<2)   // 0x0004   TMA DS (old proprietary -- i.e. 2500)

//---------------------------------------------------------------------------
// Freq hop info in RID_BSS.
//---------------------------------------------------------------------------
struct RID_BSS_FH {
    u16 dwellTime;
    u8  hopSet;
    u8  hopPattern;
    u8  hopIndex;
    u8  filler;
    };

//---------------------------------------------------------------------------
// Bss Rid
//---------------------------------------------------------------------------
struct RID_BSS {
    u16         length;         // length of this RID
    u16         index;          // 0xFFFF if list ends, starts at 0 and increments
    u16         radioType;      // 1=802.11 FH, 2=802.11 DS, 4=TMA DS (old proprietary -- i.e. 2500)
    u8          bssid[6];       // mac address BSSID
    u8          zero;           // element number for SSID
    u8          ssidLen;
    u8          ssid[32];
    u16         rssi;           // will be in dBm later (0=0, 10=-10dBm, 20=-20dBm, etc...
    u16         capability;     // bit mapped
    u16         beaconInterval;    
    u8          rates[8];       // note msb of each rate is set if a basic rate
                                // so 0x16=>optional 11Mbps, 0x96=>basic/required 11Mbps
    RID_BSS_FH  fhInfo;         // FH only
    u16         dsChannel;      // DS only
    u16         atimWindow;     // Atim window size
    };

typedef struct _RID_wep_key {
    unsigned short  ridlen;     // sizeof (RID_WEP_Key)
    unsigned short  keyindex;   // 0, 1, 2, 3 for enterprise;   4 for "home wep key";  0xFFFF for Tx RID 
                                //  (or no keys present if read with RID FF15)
    unsigned char   macaddr[6]; // usually 01:00:00:00:00:00
    unsigned short  keylen;     // 5 for 40 bit; 13 for 128 bit
    unsigned char   key[13];    // all zeros for read
} RID_WEP_Key;

typedef struct _RID_tx_wep_key {
    unsigned short  ridlen;
    unsigned short  keyindex;       // 0xFFFF indicates this is the Tx index record
    unsigned short  tx_key_index;   // 0, 1, 2, or 3
    unsigned char   unused_char[4];
    unsigned short  unused_short;
    unsigned char   key[13];
} RID_TX_WEP_Key;

#pragma pack( pop, struct_pack1 )
//
#endif  //__HW3500_H__