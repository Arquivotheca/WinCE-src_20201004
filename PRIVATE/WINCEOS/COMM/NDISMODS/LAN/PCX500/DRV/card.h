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
// Card.h
//---------------------------------------------------------------------------
// Description:
//
// Revision History:
//
// Date        
//---------------------------------------------------------------------------
// 10/24/00     jbeaujon        -Initial Revision
//
// 05/04/01     spb001          -Added "fast" initialization option.  By changing
//                               the InitFW() to only reset the device on the 
//                               first call.  InitFW1() must then be called to 
//                               finish the initialization. Subsequent calls to InitFW()
//                               will work as prior to changes.
//
// 05/10/01     spb004          -Work around for network card not sending an updatelinkstatus
//                               interrupt when it should
//
// 07/18/01     spb023          -Added firmware flashing during cbInit for Windows XP
//
// 07/23/01     spb026          -Changed the was firmware flashing is done during
//                               init to a blocking call because it Broke WCZ.  Note
//                               this is an NIDS5 thing only
//
// 08/01/01     spb027          -Added ability to cancel the flash
//
// 08/13/01     raf             -Magic Packet mode addition
//---------------------------------------------------------------------------
//      Card.h
#ifndef __card_h__
#define __card_h__
#include "NDISVER.h"
#include "AiroOid.h"
#include "hwx500.h"
#include "hwx500p.h"
#include "802_11.h"

#define FREQUENCY_DS        0
#define FREQUENCY_HOPPER    1

extern UINT     CardType;       
extern UINT     FrequencyType;      // DS = 0, HOPPER=1 
extern UINT     DriverMajorVersion;  
extern UINT     DriverMinorVersion;     
extern char     *Ver_FileVersionStr;
extern char     *CardName;      
extern char     *DriverName;        

extern char *   VxdLoadName;
extern char *   VxdUnloadName;  

//#define       BOOL    BOOLEAN
//#define       PBOOL   PBOOLEAN
#if DBG
    #if 1
        #define DBGOUT(a)           DbgPrint(a)
        #define DBGOUT2(a,b)        DbgPrint(a,b)
        #define DBGOUT3(a,b,c)      DbgPrint(a,b,c)             
        #define DBGOUT4(a,b,c,d)    DbgPrint(a,b,c,d)
    #endif
    #if 0
        #define PRINT(a)            DbgPrint(a)
        #define PRINT2(a,b)         DbgPrint(a,b)
        #define PRINT3(a,b,c)       DbgPrint(a,b,c)             
        #define PRINT4(a,b,c,d)     DbgPrint(a,b,c,d)

        #define SAY(a)              DbgPrint(a)
        #define SAY2(a,b)           DbgPrint(a,b)
        #define SAY3(a,b,c)         DbgPrint(a,b,c)             
        #define SAY4(a,b,c,d)       DbgPrint(a,b,c,d)

    #else
//  
        #define SAY(a)
        #define SAY2(a,b)
        #define SAY3(a,b,c)
        #define SAY4(a,b,c,d)

        #define PRINT(a)            
        #define PRINT2(a,b)         
        #define PRINT3(a,b,c)                   
        #define PRINT4(a,b,c,d) 

    #endif
#else
        #define SAY(a)
        #define SAY2(a,b)
        #define SAY3(a,b,c)
        #define SAY4(a,b,c,d)
        //      
        #define PRINT(a)            
        #define PRINT2(a,b)     
        #define PRINT3(a,b,c)                       
        #define PRINT4(a,b,c,d) 
        //
        #define DBGOUT(a)           
        #define DBGOUT2(a,b)        
        #define DBGOUT3(a,b,c)      
        #define DBGOUT4(a,b,c,d)    

#endif

#define     DEFAULT_MULTICASTLISTMAX            128
#define     DEFAULT_INTERRUPTNUMBER             5       // Default value for Adapter->InterruptNumber
#define     CIS_ATTRIBMEM_SIZE                  2048    
#define     CTL_ATTRIBMEM_SIZE                  128
#define     ESS_ID1_LENGTH                      33
#define     MAX_CONFIGURATIONS                  100


#define     DEFAULT_MEDIA_DISCONNECT_DAMPER     10
#define     DEFAULT_AUTO_CONFIG_PASSIVE_DAMPER  30
#define     DEFAULT_AUTO_CONFIG_ACTIVE_DAMPER   8


//#include "Core.h"
#include "support.h"
//#include "CardCmd.h"
#include "AiroDef.h"

typedef enum {
    INDICATE_OK,
    SKIPPED,
    ABORT,
    CARD_BAD
} INDICATE_STATUS;

typedef enum {   // magic packet modes
    MP_POWERED,  // running state
    MP_SENT,     // card is enabled, but some oids still need to come through
    MP_ENABLED   // next oids will be after a wakeup
} MP_MODE;


/*
NDIS_ERROR_CODE_RESOURCE_CONFLICT
NDIS_ERROR_CODE_OUT_OF_RESOURCES
NDIS_ERROR_CODE_HARDWARE_FAILURE
NDIS_ERROR_CODE_ADAPTER_NOT_FOUND
NDIS_ERROR_CODE_INTERRUPT_CONNECT
NDIS_ERROR_CODE_DRIVER_FAILURE
NDIS_ERROR_CODE_BAD_VERSION
NDIS_ERROR_CODE_TIMEOUT
NDIS_ERROR_CODE_NETWORK_ADDRESS
NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION
NDIS_ERROR_CODE_INVALID_VALUE_FROM_ADAPTER
NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER
NDIS_ERROR_CODE_BAD_IO_BASE_ADDRESS
NDIS_ERROR_CODE_RECEIVE_SPACE_SMALL
NDIS_ERROR_CODE_ADAPTER_DISABLED
*/
typedef enum {
        RESOURCE_CONFLICT               =0x01,
        OUT_OF_RESOURCES                =0x02,
        HARDWARE_FAILURE                =0x03,
        ADAPTER_NOT_FOUND               =0x04,
        INTERRUPT_CONNECT               =0x05,
        DRIVER_FAILURE                  =0x06,
        BAD_VERSION                     =0x07,
        TIMEOUT                         =0x08,
        _NETWORK_ADDRESS                =0x09,
        UNSUPPORTED_CONFIGURATION       =0x0A,
        INVALID_VALUE_FROM_ADAPTER      =0x0B,
        MISSING_CONFIGURATION_PARAMETER =0x0C,
        BAD_IO_BASE_ADDRESS             =0x0D,
        RECEIVE_SPACE_SMALL             =0x0E,
        ADAPTER_DISABLED                =0x0F,
        VXD_NOTLOADED                   =0x10,
        BAD_ATTRIBMEM_BASE_ADDRESS      =0x11,
}AIRONET_ERRORLOG_CODE;

typedef struct _EXTENDED_PARAMS {
    u16     RxMode;
    u16     FragThreshhold;
    u16     RTSThreshhold;
    u16     ShortRetryLimit;
    u16     LongRetryLimit;
    u16     TxMSDULifetime;
    u16     RxMSDULifetime;
    u16     stationary  ;
    u16     ordering    ;
    u16     ScanMode    ;
    u16     ProbeDelay;
    u16     ProbeEnergyTO;
    u16     ProbeRespTO;
    u16     BeaconListenTO;
    u16     JoinNetTO;
    u16     AuthenTO;
    u16     AuthenType;
    u16     AssociationTO;
    u16     SpecAPTO    ;
    u16     OfflineScanInterv;
    u16     OfflineScanDuar;
    u16     LinkLossDelay;
    u16     MaxBeaconLT;
    u16     ResfreshInterv;
    u16     RcvDTIM ;
    u16     ListenInterv;
    u16     FastListenInterv;
    u16     ListenDecay;
    u16     FastListenDecay;
    u16     BeaconPeriod;
    u16     AtimDuration;
    u16     HopPeriod;
    u16     HopSet;
    u16     HopPattern;
    u16     RadioType;
    u16     Diversity;
    u16     TransmitPower;
    u16     RSSIThreshhold; // 0x76
    u8      NodeName[16];   // 0x80
    u16     ARLThreshhold;  // 0x90
    u16     ARLDecay;       // 0x92
    u16     ARLDelay;       // 0x94
    //----------------------------------------------------
    u16     LM2000Preamble; // 0x78
    u16     LM2000param;    // 0x7a
    u16     LM2000WaitTime; // 0x7c
    u16     LM2000FindRtr;  // 0x7e
    u16     RcvErrors;
    u16     AutoSave;
    u16     RadioConfig;
    u16     timing802_11;
    u16     CWminmax[2];

}EXTENDED_PARAMS;

#define IMAGE_SIZE  (1024 * 128) 

typedef struct  
{
    NDIS_MINIPORT_TIMER m_timer;
    int                 m_Prevdelay;
    int                 m_delay;
    int                 m_Progress;      // -1 = failed, 0 = finished, 1 = in progress 
    int                 m_state;
    void                (*m_FlashStateProc)();
    char                m_ImageBuf[IMAGE_SIZE];
    int                 m_block;
    int                 m_ImageBufOff;
    BOOLEAN             m_useTimer;     //spb026
}FLASH_STRUCT;

typedef struct _D100_LIST_ENTRY {

    LIST_ENTRY  Link;

} D100_LIST_ENTRY, *PD100_LIST_ENTRY;


 
typedef struct  _CARD{
    USHORT              m_SlotNumber;       //PCI slot
    NDIS_MINIPORT_TIMER m_CardTimer;
    BOOLEAN             m_InterruptHandled;
    BOOLEAN             m_CardTimerIsSet;
    USHORT              m_IntActive;
    USHORT              m_IntMask;
    USHORT              m_CmdIntMask;
    USHORT              m_cmd;
    FID                 m_fid;
    FLASH_STRUCT        FlashSt;
    //-----------------------------------------------------------------------
    // 
    //-----------------------------------------------------------------------
    NDIS_HANDLE         m_MAHandle;
    BOOLEAN             m_InitDone;
    BOOLEAN             m_IsFlashing;
    STFLASH_PROGRESS    m_FlashProgress;
    IN NDIS_HANDLE      m_CfgHandle;
    char                m_ImageNameBuf[512];
    NDIS_STRING         m_ImageName;
    char                *m_VxdLoadName;
    char                *m_VxdUnloadName;
    //-----------------------------------------------------------------------
    // CARD HW RESOURCES & THEIR FLAGS
    //-----------------------------------------------------------------------
    BOOLEAN             m_IsMacEnabled; 
    BOOLEAN             m_WasMacEnabled;    
    BOOLEAN             m_IsPolling;    
    BOOLEAN             m_PrevCmdDone;
    int                 m_PrevCmdTick;
    USHORT              m_PrevCommand;
    UINT                m_FrequencyType;        
    UINT                m_CardType;     
    char                m_CardName[32];     
    char                m_DriverName[32];       
    NDIS_MINIPORT_INTERRUPT Interrupt;      // Interrupt object.
    NDIS_INTERFACE_TYPE m_BusType;
    char                m_FormFactor[20];// "PCMCIA", "ISA", "PCI"
    ULONG               m_XmitReg;
    ULONG               m_RcvReg;
    ULONG               m_IOBase;
    USHORT              m_PortIOLen;
    ULONG               m_IOBase8Bit;
    USHORT              m_PortIOLen8Bit;
    ULONG               m_InitialPort;
    ULONG               m_InitialPort8Bit;
    BOOLEAN             m_IsIO;
    BOOLEAN             m_IsIO16;
    PUCHAR              m_MemBase;
    UCHAR               *m_pAttribMemBase;
    UCHAR               m_InterruptNumber;
    UCHAR               *m_XmitMemBase;
    UCHAR               *m_RcvMemBase;
    BOOLEAN             m_InterruptRegistered;
    BOOLEAN             m_IoRegistered;
    BOOLEAN             m_AttribMemRegistered;
    //BOOLEAN               m_SharedMemRegistered;
    ULONG               m_FWVersion;
//    USHORT              m_SoftwareCapabilities; //0x0076
//---------------------- CARD CONFIG VARIABLES
    USHORT              m_RadioChannel;
    UINT                m_MagicPacketMode;
    ULONG               m_PromiscuousMode;
    USHORT              m_PowerSaveMode;
    USHORT              m_MaxPSP;               //BOOLEAN
    BOOLEAN             IsAssociated;
    BOOLEAN             IsAwake;
    int                 KeepAwake;
    USHORT              m_InfrastructureMode;
    USHORT              m_RTSRetryLimit;
    USHORT              m_DataRetryLimit;
    UCHAR               m_SystemID[sizeof(UINT)];           // FOR TMA ONLY
    UCHAR               m_ESS_ID1[ESS_ID1_LENGTH];
    short               m_ESS_IDLen1;
    BOOLEAN             m_MacIdOveride;
    UCHAR               m_AdapterName[16];
    UCHAR               m_SupportedRates[8];
    USHORT              m_SelectedRate;
    UINT                m_LinkSpeed;
    BOOLEAN             m_UseSavedConfig;   
    USHORT              m_Diversity;
    USHORT              m_TransmitPower;
// 
// Removed 11/29/00 -- not needed.
// 
//    USHORT              m_Authentication;
    BOOLEAN             m_ResetOnHang;
    int                 m_ResetTick;
//
// ---------------------- radio stats
//
    ULONG               m_FramesXmitGood;           // Good Frames Transmitted
    ULONG               m_FramesXmitBad;            // Bad Frames Transmitted
    ULONG               m_FramesRcvGood;            // Good Frames Received
    ULONG               m_FramesRcvBad;             // CRC errors counted
    ULONG               m_FramesRcvOverRun;         // missed packet counted
    ULONG               m_FrameAlignmentErrors;     // FAE errors counted
    ULONG               m_FramesXmitOneCollision;   // Frames Transmitted with one collision
    ULONG               m_FramesXmitManyCollisions; // Frames Transmitted with > 1 collision
    UINT                m_ReceivePacketCount;
//
// ---------------------- DRIVER VARIABLES
    UINT                m_MaxMulticastList;
    UCHAR               m_Addresses[DEFAULT_MULTICASTLISTMAX][AIRONET_LENGTH_OF_ADDRESS];
    UCHAR               m_PermanentAddress[AIRONET_LENGTH_OF_ADDRESS];// The ethernet address that is burned into
    UCHAR               m_StationAddress[AIRONET_LENGTH_OF_ADDRESS];// The ethernet address currently in use.
    ULONG               m_PacketFilter;
    ULONG               m_MaxLookAhead;     // The lookahead buffer size in use.
    ULONG               m_CurMulticastSize; // The current size of multicast address
    PNDIS_PACKET        m_Packet;
    BOOLEAN             m_CardStarted;
    BOOLEAN             m_CardPresent;
    int                 m_TxIntPending;
    UINT                m_PacketRxLen;      // Total length of a received packet.
    UCHAR               *m_Lookahead;   // Lookahead buffer
#if NDISVER == 3
    UCHAR               m_RcvBuf[RX_BUF_SIZE];  // Lookahead buffer
#else
#define NUMRFD          8            
    UCHAR               m_RcvBuf[NUMRFD][RX_BUF_SIZE];  // Lookahead buffer
#endif              
    BOOLEAN             m_IndicateReceiveDone;// TRUE if the driver needs to call
    BOOLEAN             m_VxdLoaded;
    BOOLEAN             m_IsInDoNextSend;
    CQ                  m_Q;        // Store NDIS packets buffers' pointers
    CQ                  fidQ;
    CSTACK              m_IntStack;
    BOOLEAN             m_CardFirmwareErr;
    // 
    // Array of configuration profiles.
    // 
    STPROFILE           *m_profiles;
    // 
    // Number of available configuration profiles.
    // 
    USHORT              m_numProfiles;
    // 
    // Index of the active profile.
    // 
    USHORT              m_activeProfileIndex;
    // 
    // Points to the "active" profile in the m_profiles array.  This is the profile that 
    // is stored in the card.
    // 
    STPROFILE           *m_activeProfile;
    // 
    // Points to the "current" profile in the m_profiles array.  This is not necessarily 
    // the profile stored in the card.
    // 
    STPROFILE           *m_currentProfile;

//    BOOLEAN             m_activeProfileSwitched;
    // 
    // Global enable/disable flag for auto profile switching.
    // 
    BOOLEAN             m_autoConfigEnabled;
    // 
    // Flag to temporarily disable auto profile switching.
    // 
    BOOLEAN             m_tempDisableAutoSwitch;

    // 
    // BSSID list cache.  Gets destroyed on a list scan, and rebuilt on the next request
    // for the BSSID list.  Subsequent requests for the list, return the cached copy.
    // 
    NDIS_802_11_BSSID_LIST  *bssid_list;
    UINT                    bssid_list_count;   // number of elements in the list.
    UINT                    bssid_list_size;    // size of the list (bytes).


    // 
    // Forces init FW to read the card's stored config and use this as the default.
    // 
    BOOLEAN             m_readCardConfig;

//-- NDIS5  
    NDIS_SPIN_LOCK      m_lock;
    PNDIS_PACKET        m_TxHead;
    PNDIS_PACKET        m_TxTail;
    UINT                m_NumPacketsQueued;

    //Currently associated information
    NDIS_802_11_MAC_ADDRESS     m_BSSID;
    NDIS_802_11_SSID            m_SSID;

    //                  - -------------- AMCC PCI card 
    BOOLEAN             IsAMCC;
    PUCHAR              m_ContolRegMemBase;
    BOOLEAN             m_ContolRegRegistered;
    //PUCHAR                m_RadioMemBase;
    //BOOLEAN               m_RadioMemRegistered;

    //                  ---- ----------- Softex stuff
    BOOLEAN             IsSoftexClient;
    USHORT              Softex_ClientHandle;
    ULONG               Softex_Flags;
    USHORT              Softex_Sktnum;
    BOOLEAN             Softex_Removed;
    int                 SoftexRegistryPathLength;
    PWSTR               SoftexRegistryPath;

//          
    UINT                m_MediaDisconnectDamper;
    UINT                m_MediaDisconnectDamperTick;
    NDIS_STATUS         m_MSenceStatus;     
    // 
    // Dampers used when switching among configuration profiles.
    // 
    UINT                m_AutoConfigActiveDamper;
    UINT                m_AutoConfigPassiveDamper;
    UINT                m_AutoConfigDamperTick;

    STCAPS              capabilities;
    DBM_TABLE           *dBmTable;

#if NDISVER == 5
    void                *m_pSwRfd;
    D100_LIST_ENTRY     RfdList;
    UINT                UsedRfdCount;       
    UINT                NumRfd; 
    NDIS_HANDLE         RxPktPool;
    NDIS_HANDLE         RxBufPool;
    MP_MODE             mp_enabled;    // magic packet command enabled
#endif
    BOOLEAN             initComplete;           //spb001
    UINT                m_UpdateLinkDamper;     //spb004
    BOOLEAN             cancelTimer;            //spb019
    BOOLEAN             wasFlashChecked;        //spb023
    BOOLEAN             flashCard;              //spb026
    BOOLEAN             cancelFlash;            //spb027

#ifdef UNDER_CE
    BOOLEAN             bHalting;
#endif

//
}CARD, *PCARD;

#define     PUSH(a)         push(card->m_IntStack, a)
#define     POP()           pop(card->m_IntStack)   
#define     POPPEAK         PopPeak(card->m_IntStack, a)
#define     POPPEAK         PopPeak(card->m_IntStack, a)
#define     RESETSTACK()    ResetStack(card->m_IntStack)    

#ifdef INVALIDPACKETS
    void Reader80211();
#endif

BOOLEAN baseCardConstruct(PCARD card);
void baseCardDestruct(PCARD card);

extern NDIS_STATUS  cbResetCard(PBOOLEAN, NDIS_HANDLE );
extern ULONG    GetRxHeader(PCARD   card, PUCHAR);
extern void     UpdateCounters(PCARD    card);
extern void     cbIsr( PBOOLEAN ,PBOOLEAN, NDIS_HANDLE);    
extern void     cbHandleInterrupt( PCARD card);
extern BOOLEAN  cardConstruct(PCARD card);
extern void     cardDestruct(PCARD card);
NDIS_STATUS     cbSend( NDIS_HANDLE,PNDIS_PACKET Packet, UINT Flags);
VOID            cbSendPackets(NDIS_HANDLE, PPNDIS_PACKET, UINT);
void            ReleaseSendQ(PCARD card);

NDIS_STATUS         
cbTransferData( 
    PNDIS_PACKET    Pkt,
    PUINT           BytesTransferred,
    NDIS_HANDLE     Context,
    NDIS_HANDLE     MiniportReceiveContext,
    UINT            ByteOffset,
    UINT            BytesToTransfer 
    );

extern BOOLEAN  InitHW(PCARD    card);

extern BOOLEAN  InitFW(PCARD    card, BOOLEAN useCurrentConfig = FALSE);
extern BOOLEAN  InitFW1(PCARD   card, BOOLEAN useCurrentConfig = FALSE);     //spb001

NDIS_STATUS 
cbQueryInformation(
    NDIS_HANDLE     Context,
    IN NDIS_OID     Oid,
    IN PVOID        InfBuff,
    IN ULONG        InfBuffLen,
    OUT PULONG      BytesWritten,
    OUT PULONG      BytesNeeded
    );

NDIS_STATUS 
cbSetInformation(   
    NDIS_HANDLE     Context,
    IN NDIS_OID     Oid,
    IN PVOID        InfBuff,
    IN ULONG        InfBuffLen,
    OUT PULONG      BytesRead,
    OUT PULONG      BytesNeeded
    );

extern NDIS_STATUS  
ExtendedOids(
    PCARD           card,
    IN NDIS_OID     Oid ,
    IN PVOID        InfBuff,
    IN ULONG        InfBuffLen,
    OUT PULONG      BytesWritten,
    OUT PULONG      BytesNeeded
    ); 


inline
BOOLEAN             
IsValidPacket(
    PCARD   card,
    UCHAR   Lookahead
    )
{
    return  *(ULONG *)(Lookahead+6) || *(USHORT *)(card->m_Lookahead+10);
}

void                
XmitDpc(
    PCARD   card
    );

void                
RcvDpc(
    PCARD   card
    );

INDICATE_STATUS     
IndicatePacket(
    PCARD   card
    );


BOOLEAN             
DoNextSend(
    PCARD   card,
    PNDIS_PACKET    Packet=NULL
    );

BOOLEAN             
CopyDownPacketPortUShort(
    PCARD   card,
    PNDIS_PACKET    Packet 
    );

BOOLEAN             
CopyDownPacketPortUChar(
    PCARD   card,
    PNDIS_PACKET    Packet 
    );

void                
CopyUpPacketUShort( 
    PCARD   card,
    PUCHAR,
    UINT 
    );


BOOLEAN             
InitDriver(
    PCARD   card,
    PNDIS_STATUS    OpenErrorStatus,
    PUINT           SelectedMediumIndex,
    PNDIS_MEDIUM    MediumArray,
    UINT            MediumArraySize,
    NDIS_HANDLE     MiniportAdapterHandle,
    NDIS_HANDLE     ConfigurationHandle
    );

//spb023
BOOLEAN 
CheckRev(
    PCARD card
    );

void 
FlashImage(
    PCARD card
    );

BOOLEAN             
GetDriverConfig(
    PCARD   card
    );

BOOLEAN             
RegisterAdapter(
    PCARD   card
    );

BOOLEAN             
RegisterInterrupt(
    PCARD   card
    );

void                
LogError( 
    PCARD   card,
    NDIS_ERROR_CODE, 
    const int 
    );

void
LogError(
    IN  NDIS_HANDLE             adapterHandle,
    IN  NDIS_ERROR_CODE         ErrorCode,
    IN  const int               AiroCode
    );

BOOLEAN             
LoadVxd(
    PCARD   card
    );

void                
UnloadVxd(
    PCARD   card
    );

void                
GetExtendedRegistryParams(
    PCARD   card,
    NDIS_HANDLE,
    EXTENDED_PARAMS &
    );

void    
CardTimerCallback(
    IN PVOID    ,
    PCARD,IN PVOID, 
    IN PVOID
    );
void
AckPendingPackets(
    PCARD   card
    );

BOOLEAN CheckCardFlashing (PCARD card,
                           NDIS_STATUS *status,
                           NDIS_OID Oid );

////////////////////////////////////////////////////////////////////////
inline
BOOLEAN             
IsMyPacket(
    PCARD   card,
    PUCHAR  Lookahead
    )
{   
    UINT    result;
    //ETH_COMPARE_NETWORK_ADDRESSES_EQ( card->m_Lookahead, card->m_StationAddress, &result);
    ETH_COMPARE_NETWORK_ADDRESSES_EQ( Lookahead, card->m_StationAddress, &result);
    return  0==result;
}

inline
BOOLEAN             
CheckMulticast(
    PCARD   card,
    PUCHAR  Lookahead
    )
{
    UINT    i;
    UINT    result=1;

    // The received packet is a broadcast/multicast packet
    // It is at least a multicast address.  Check to see if
    // it is a broadcast address.
    
    if ( ETH_IS_BROADCAST( Lookahead )){
        return card->m_PacketFilter & NDIS_PACKET_TYPE_BROADCAST ? TRUE : FALSE;
    }
    else  // received a multicast packet
    if ( card->m_PacketFilter & ( NDIS_PACKET_TYPE_ALL_MULTICAST | NDIS_PACKET_TYPE_MULTICAST)){
            
        if ( card->m_PacketFilter & NDIS_PACKET_TYPE_ALL_MULTICAST )
            return TRUE;
        
        for (i = 0; result && i < card->m_CurMulticastSize ; i++){
            ETH_COMPARE_NETWORK_ADDRESSES_EQ( Lookahead, card->m_Addresses[i], &result);
            if( 0==result )
                return TRUE;
        }
    }
    return FALSE;
}

void setAutoConfigTimer (PCARD card, BOOLEAN includeMediaDisconnectTimer = FALSE);


#define F_PCMCIA 0x00000001
inline ADAPTER_READY(PCARD card)
{
#ifndef SOFTEX
    return TRUE;
#else
    BOOLEAN SoftexCardReady (PCARD  card);
    return !(card->Softex_Flags & F_PCMCIA) || SoftexCardReady(card);
#endif
}

////////////////////////////////////////////////////////////////////////
#endif
