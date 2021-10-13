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
// Card.cpp
//---------------------------------------------------------------------------
//
// Description:
//
// Revision History:
//
// Date         
//---------------------------------------------------------------------------
// 10/10/00     jbeaujon    -changed vendor name in OID_GEN_VENDOR_DESCRIPTION
//                           from "Aironet" to "Cisco".
// 
// 10/20/00     jbeaujon    -added the following query OIDs:
//                              OID_802_11_SSID
//                              OID_802_11_INFRASTRUCTURE_MODE
//                              OID_802_11_BSSID
//                          -added the following set OIDs:
//                              OID_802_11_SSID
//                              OID_802_11_INFRASTRUCTURE_MODE
//                              OID_802_11_DISASSOCIATE
//                              OID_802_11_AUTHENTICATION_MODE
// 
// 03/26/01     jbeaujon    -added OID_802_11_RELOAD_DEFAULTS
//
// 05/04/01     spb001      -Added "fast" initialization option.  By changing
//                           the InitFW() to only reset the device on the 
//                           first call.  InitFW1() must then be called to 
//                           finish the initialization. Subsequent calls to InitFW()
//                           will work as prior to changes.
//
// 05/10/01     spb004      -Work around for network card not sending an updatelinkstatus
//                           interrupt when it should
//
// 05/31/01     spb008      -Make sure we don't update link status when we are flashing
//
// 06/05/01     spb009      -Make sure we only use the botton byte of the signal
//                           strength as an index into the dbmTable (only 256 bytes).
//
// 06/12/01     spb010      -If we are using the fast init option, then 
//                           we don't want to init interrupts yet because firm
//                           ware will zero out the interrupt enable
//                           register during the reset.  This caused a race 
//                           condition where the card would be up and running
//                           but not interrupts were enabled.
//
// 06/19/01     spb012      -Hack to stop jerkiness when in PSP.   This needs
//                           to really be done on either a per oid basis or
//                           better yet, at a lower level i.e. per command.
//
// 06/28/01     spb014      -Fixed MiniportQuery NDIS_802_11_SSID so that
//                           we will return the currently associated SSID.
//
// 07/03/01     spb017      -Added counter to cancel timer so we don't hang 
//                           in destructor forever.
//
// 07/10/01     spb018      -If you are using the Home configuration, and WZC
//                           plumbs down a WEP key, turn off Home configuration
//                           because otherwise the new WEP key won't be used
//                           since HOME key is 5 and 802_11 spec only has 1-4.
//
// 07/23/01     spb026      -Changed OID_QUERY_RADIO_STATE in oidx500.cpp since
//                           there was no reason to access the card it is ok
//                           now to check when flashing
//
// 08/01/01     spb027      -Added ability to cancel flash timer if we are NDIS5
//                           driver.
//
// 08/03/01     spb029      -Added length check to ssid query
//
// 08/13/01     raf         -Magic Packet fixes 
//---------------------------------------------------------------------------
#pragma code_seg("LCODE")

#include "NDISVER.h"
extern "C"{
    #include    <ndis.h>
}

#include "CardX500.h"
#include "string.h"
#include "aironet.h"
#include "cmdX500.h"
#include "HWX500P.h"

extern BOOLEAN  IsCardInserted(PCARD card);
extern int      GetSupOidSize();
extern int      AironetSupportedOids[];
extern PCARD    AdapterArray[];
extern WINDOWSOS    OSType;     


NDIS_STATUS getShortVal (ULONG InfBuffLen, PVOID InfBuff, PUSHORT shortVal);

#define ARRAYSIZE(s) (sizeof(s) / sizeof(s[0]))

#define show(a,b)   a=b
extern UINT DebugFlag;
PCARD   gCard;


//===========================================================================
    void baseCardDestruct (PCARD card)
//===========================================================================
// 
// Description: Pseudo-destructor for card.
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    gCard = card;

    if (card->m_CardTimerIsSet) {
        BOOLEAN ret = FALSE;
#if NDISVER == 5
//spb017 
        NdisMCancelTimer(&card->m_CardTimer, &ret );
        if (!ret) {
            //spb020
            card->cancelTimer=TRUE;
            while (card->m_CardTimerIsSet) {
                //wait for timer to fire
                NdisMSleep(1000000);
            }
        }
#else
        while (!ret) {
            NdisMCancelTimer(&card->m_CardTimer, &ret );
        }
#endif
    }

#if NDISVER == 5
    if (card->m_IsFlashing) {
        //spb027
        //We need to cancel the flash or at least wait until
        //it has finished.  Try and cancel it, but we may be 
        //at a point where we can't cancel therefore we have
        //to wait until we are done 
        card->cancelFlash = TRUE;
        while (card->m_IsFlashing) {
            //wait for timer to fire
            NdisMSleep(1000000);
        }
    }
#endif

    if (card->m_InterruptRegistered) {
        NdisMDeregisterInterrupt(&card->Interrupt);
        card->m_InterruptRegistered = FALSE;
        }

    if (card->m_AttribMemRegistered) {
        NdisMUnmapIoSpace( card->m_MAHandle, (void *)card->m_pAttribMemBase, CIS_ATTRIBMEM_SIZE );
        card->m_AttribMemRegistered = FALSE;
        }
    
    //if( card->m_RadioMemRegistered )
    //  NdisMUnmapIoSpace( card->m_MAHandle, (void *)card->m_RadioMemBase, CIS_ATTRIBMEM_SIZE );

    if (card->m_ContolRegRegistered) {
        NdisMUnmapIoSpace( card->m_MAHandle, (void *)card->m_ContolRegMemBase, CTL_ATTRIBMEM_SIZE );
        card->m_ContolRegRegistered = FALSE;
        }
    
    if (card->m_IoRegistered) {
        //NdisMUnmapIoSpace( card->m_MAHandle, (void *)card->m_IOBase, card->m_PortIOLen );
        NdisMDeregisterIoPortRange( card->m_MAHandle, card->m_InitialPort,
                card->m_PortIOLen,(void *)card->m_IOBase );
        
        if (card->m_PortIOLen8Bit) {
            //NdisMUnmapIoSpace( card->m_MAHandle, (void *)card->m_IOBase8Bit, card->m_PortIOLen8Bit );
            NdisMDeregisterIoPortRange( card->m_MAHandle, card->m_InitialPort8Bit, 
                card->m_PortIOLen8Bit, (void *)card->m_IOBase8Bit );
            }
        card->m_IoRegistered = FALSE;
        }
    
    delete [] card->m_profiles;

    if (card->dBmTable != NULL) {
        delete card->dBmTable;
        }

    if (card->bssid_list) {
        delete [] card->bssid_list;
        }

#ifndef UNDER_CE
    UnloadVxd(card);
#endif
}

//===========================================================================
    BOOLEAN baseCardConstruct (PCARD card) 
//===========================================================================
// 
// Description: Pseudo-constructor for card.
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    BOOLEAN retval              = FALSE;

    card->m_CardPresent         = FALSE;
    CQReset(card->m_Q);
    CQReset(card->fidQ);

#if NDISVER == 3
    card->m_Lookahead           = card->m_RcvBuf;
    //For NDIS 3 drivers, use the "slow initializiation" still
    card->initComplete          = TRUE;     //spb001
#else 
    card->m_Lookahead           = card->m_RcvBuf[0];
    card->initComplete          = FALSE;    //spb001
#endif

    card->m_MediaDisconnectDamper       = DEFAULT_MEDIA_DISCONNECT_DAMPER;  // 10 seconds default
    card->m_AutoConfigActiveDamper      = DEFAULT_AUTO_CONFIG_ACTIVE_DAMPER;
    card->m_AutoConfigPassiveDamper     = DEFAULT_AUTO_CONFIG_PASSIVE_DAMPER;
    card->m_AutoConfigDamperTick        = 0;

    card->m_MSenceStatus            = NDIS_STATUS_MEDIA_DISCONNECT; 
    card->m_BusType                 = NdisInterfacePcMcia; // PCMCIA - 6
    card->m_InterruptNumber         = DEFAULT_INTERRUPTNUMBER;
    card->m_MaxMulticastList        = DEFAULT_MULTICASTLISTMAX;
    card->m_AttribMemRegistered     = FALSE;
    card->m_MagicPacketMode         = (UINT)-1; 
    card->m_PowerSaveMode           = 0; // CAM
    card->IsAwake                   = TRUE;             
    card->KeepAwake                 = 0;
    card->m_UseSavedConfig          = FALSE;
    card->m_Diversity               = 0x0000;
    card->m_ResetOnHang             = FALSE;
    card->m_ResetTick               = 0;
    card->m_CardTimerIsSet          = FALSE;
    card->m_PrevCmdTick             = 0;
    NdisZeroMemory(card->m_SupportedRates, sizeof(card->m_SupportedRates));
//
    card->m_TxHead              = 0;
    card->m_TxTail              = 0;
    card->m_NumPacketsQueued    = 0;
    card->m_PortIOLen8Bit       = 0;
    card->IsAssociated          = FALSE;
    card->m_AttribMemRegistered = 0;
    //
    card->IsAMCC                = FALSE;
    card->m_ContolRegMemBase    = 0;
    card->m_ContolRegRegistered = FALSE;

    card->m_profiles            = NULL;
    card->m_numProfiles         = 0;
    card->m_activeProfile       = NULL;
    card->m_activeProfileIndex  = 0;
    card->m_currentProfile      = NULL;
    card->bssid_list            = NULL;

    card->dBmTable              = NULL;

#ifdef UNDER_CE
    card->bHalting              = FALSE;
#endif

#if NDISVER == 5
    card->mp_enabled            = MP_POWERED; // magic packet
#endif
    retval = TRUE;

    return(retval);
}

//===========================================================================
    NDIS_STATUS cbQueryInformation (NDIS_HANDLE     Context,
                                    IN NDIS_OID     Oid,
                                    IN PVOID        InfBuff,
                                    IN ULONG        InfBuffLen,
                                    OUT PULONG      BytesWritten,
                                    OUT PULONG      BytesNeeded)
//===========================================================================
// 
// Description: MiniportQueryInformation.
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{ 
    PCARD           card = (PCARD)Context;
    char            tmpBuf[64];

    if (NULL == card) {
        for (int i = 0; (NULL == AdapterArray[i]) && (i < CARDS_MAX); i++);
        card = (CARDS_MAX == i) ? AdapterArray[i-1] : AdapterArray[i];                
        }
    
    for( int i=0; card != AdapterArray[i] && i<CARDS_MAX; i++ );

    if( ! card || i == CARDS_MAX ) {
        return NDIS_STATUS_INVALID_OID;
    }

//    DbgPrint("Oid Query %X\n", Oid);


#if NDISVER == 5
    if (card->mp_enabled != MP_POWERED)
    {
      if (((Oid != OID_GEN_MEDIA_CONNECT_STATUS) && (Oid != OID_TCP_TASK_OFFLOAD))  ||  
          (card->mp_enabled == MP_ENABLED))
      {  
        InitFW(card, TRUE);
        card->mp_enabled= MP_POWERED;
        InitInterrupts( card );     //spb010
      } 
    }
#endif

    //spb001 Finish Initialization if we haven't yet
    if (!card->initComplete) {
        InitFW1(card);
        InitInterrupts( card );     //spb010
    }

    NDIS_STATUS         StatusToReturn  = NDIS_STATUS_SUCCESS;

    //spb008
    if (TRUE == CheckCardFlashing(card,&StatusToReturn,Oid )) {  
        return StatusToReturn;
    }

    cmdAwaken(card, TRUE);
    
    NDIS_802_11_SSID                    ndis_ssid;
    NDIS_802_11_CONFIGURATION           ndis_config;
    NDIS_802_11_RSSI                    ndis_rssi;
    NDIS_802_11_MAC_ADDRESS             ndis_bssid;
    NDIS_802_11_NETWORK_INFRASTRUCTURE  ndis_infrastructure;
    NDIS_802_11_AUTHENTICATION_MODE     ndis_authmode;
    NDIS_802_11_NETWORK_TYPE            ndis_nettype;
    NDIS_802_11_RATES                   ndis_rates;
    NDIS_802_11_WEP_STATUS              ndis_wepstatus;
#if NDISVER == 5
        NDIS_PNP_CAPABILITIES   ndis_pnp_capablities;
#endif


    ULONG               GenericULong;
    USHORT              GenericUShort;
    UCHAR               GenericArray[6];

    UINT                MoveBytes       = sizeof(ULONG);
    PVOID               MoveSource      = (PVOID)(&GenericULong);
    *BytesWritten   = 0;
    *BytesNeeded    = 0;

    //NDIS_HARDWARE_STATUS  HardwareStatus  = NdisHardwareStatusReady;
    NDIS_MEDIUM             Medium          = NdisMedium802_3;

    ASSERT(sizeof(ULONG) == 4);
  

    switch (Oid) {
    case OID_GEN_MAC_OPTIONS:

#if NDISVER == 3
//#ifndef   PM_SUPPORT
        GenericULong    = (ULONG)(  NDIS_MAC_OPTION_TRANSFERS_NOT_PEND  |
                                    NDIS_MAC_OPTION_RECEIVE_SERIALIZED  |
                                    NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
                                    NDIS_MAC_OPTION_NO_LOOPBACK );
#else
        GenericULong    = (ULONG)(  NDIS_MAC_OPTION_TRANSFERS_NOT_PEND  |
                                    NDIS_MAC_OPTION_RECEIVE_SERIALIZED  |
                                    NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
                                    NDIS_MAC_OPTION_TRANSFERS_NOT_PEND  |
                                    NDIS_MAC_OPTION_NO_LOOPBACK );
#endif
        break;

    case OID_GEN_SUPPORTED_LIST:
        MoveSource      = (PVOID)(AironetSupportedOids);
        MoveBytes       = GetSupOidSize();
        break;

    case OID_GEN_HARDWARE_STATUS:
        GenericULong = IsCardInserted(card) ? NdisHardwareStatusReady : NdisHardwareStatusNotReady;
        break;
        //veSource      = (PVOID)(  &HardwareStatus );
        //veBytes       = sizeof( NDIS_HARDWARE_STATUS );
        //break;

    case OID_GEN_MEDIA_SUPPORTED:
    case OID_GEN_MEDIA_IN_USE:

        MoveSource      = (PVOID) (&Medium);
        MoveBytes       = sizeof(NDIS_MEDIUM);
        break;

    case OID_GEN_MAXIMUM_LOOKAHEAD:
        GenericULong    = AIRONET_MAX_LOOKAHEAD;
        break;


    case OID_GEN_MAXIMUM_FRAME_SIZE:
        GenericULong    = (ULONG)(1514 - AIRONET_HEADER_SIZE);
        break;


    case OID_GEN_MAXIMUM_TOTAL_SIZE:
        GenericULong    = (ULONG)1514;
        break;


    case OID_GEN_LINK_SPEED:
#ifdef SOFTEX
        if( ! ADAPTER_READY( card ) ){
            GenericULong = 1;
            break;
        }
#endif  
        UpdateLinkSpeed( card );
        GenericULong = card->m_LinkSpeed;
        
        if(0==GenericULong) {
            // This fixes the 98 hang problem. (v6.54 10/18/00)
            GenericULong = 11 * 10000; // This value is in units of 100 bps
        }
        break;


    case OID_GEN_TRANSMIT_BUFFER_SPACE:
        GenericULong    = (ULONG)TX_BUF_SIZE;
        break;

    case OID_GEN_RECEIVE_BUFFER_SPACE:
        GenericULong    = (ULONG)2314;
        break;

    case OID_GEN_TRANSMIT_BLOCK_SIZE:
        GenericULong    = (ULONG)TX_BUF_SIZE;
        break;

    case OID_GEN_RECEIVE_BLOCK_SIZE:
        GenericULong    = (ULONG)1514;
        break;

    case OID_GEN_VENDOR_ID:
        NdisMoveMemory( (PVOID)&GenericULong, card->m_PermanentAddress, 3 );
        GenericULong    &= 0xFFFFFF00;
        MoveSource      = (PVOID)&GenericULong;
        MoveBytes       = sizeof(GenericULong);
        break;

    case OID_GEN_VENDOR_DESCRIPTION:
        //MoveSource        = (PVOID)"Aironet Wireless LAN Adapter.";
        //MoveBytes         = 31;
        strcpy( tmpBuf, "Cisco " );
        strncpy( tmpBuf+strlen(tmpBuf), card->m_CardName, ARRAYSIZE(tmpBuf) - strlen(tmpBuf) - 1 );
		tmpBuf[ARRAYSIZE(tmpBuf) - 1] = '\0';
        strncpy( tmpBuf+strlen(tmpBuf), " Wireless LAN Adapter.", ARRAYSIZE(tmpBuf) - strlen(tmpBuf) - 1 );
		tmpBuf[ARRAYSIZE(tmpBuf) - 1] = '\0';
        MoveSource      = tmpBuf;
        MoveBytes       = strlen(tmpBuf)+1;
        break;

    case OID_GEN_DRIVER_VERSION:
        GenericUShort   = ((USHORT)NDIS_MAJOR_VERSION << 8) | NDIS_MINOR_VERSION;
        MoveSource      = (PVOID)&GenericUShort;
        MoveBytes       = sizeof(GenericUShort);
        break;

    case OID_GEN_CURRENT_LOOKAHEAD:
        GenericULong    = (ULONG)card->m_MaxLookAhead;
        break;

    case OID_802_3_PERMANENT_ADDRESS:
        NdisMoveMemory((PCHAR)GenericArray, card->m_PermanentAddress, AIRONET_LENGTH_OF_ADDRESS);
        MoveSource      = (PVOID)GenericArray;
        MoveBytes       = sizeof(card->m_PermanentAddress);
        break;

    case OID_802_3_CURRENT_ADDRESS:
        NdisMoveMemory((PCHAR)GenericArray, card->m_StationAddress, AIRONET_LENGTH_OF_ADDRESS);
        MoveSource      = (PVOID)GenericArray;
        MoveBytes       = sizeof(card->m_StationAddress);
        break;

    case OID_802_3_MAXIMUM_LIST_SIZE:
        GenericULong    = (ULONG) card->m_MaxMulticastList;
        break;

    case OID_GEN_XMIT_OK:
        UpdateCounters(card);
        GenericULong    = (UINT)card->m_FramesXmitGood;
        break;

    case OID_GEN_RCV_OK:
        UpdateCounters(card);
        GenericULong    = (UINT)card->m_FramesRcvGood;
        break;

    case OID_GEN_XMIT_ERROR:
        UpdateCounters(card);
        GenericULong    = (UINT)card->m_FramesXmitBad;
        break;

    case OID_GEN_RCV_ERROR: 
        UpdateCounters(card);
        GenericULong    = (UINT)card->m_FramesRcvBad;
        break;

    case OID_GEN_RCV_NO_BUFFER:
        UpdateCounters(card);
        GenericULong    = (UINT)card->m_FramesRcvOverRun;
        break;
    
    case OID_802_3_RCV_ERROR_ALIGNMENT:
        GenericULong    = 0;
        break;

    case OID_802_3_XMIT_ONE_COLLISION:
        UpdateCounters(card);
        GenericULong    = (UINT)card->m_FramesXmitOneCollision;
        break;

    case OID_802_3_XMIT_MORE_COLLISIONS:
        UpdateCounters(card);
        GenericULong    = (UINT)card->m_FramesXmitManyCollisions;
        break;
    
    case OID_GEN_VENDOR_DRIVER_VERSION:
        GenericULong    = (DriverMajorVersion<<16) | DriverMinorVersion;        
        break;

    case OID_GEN_MAXIMUM_SEND_PACKETS:
        GenericULong    = 1;
        break;

    case OID_GEN_MEDIA_CONNECT_STATUS:
        if (NDIS_STATUS_MEDIA_CONNECT == card->m_MSenceStatus) {
            GenericULong = NdisMediaStateConnected;
        }
        else {
            GenericULong = NdisMediaStateDisconnected;   
        }
        #if DBG
//spb026        DbgPrint("Media Connect Status is %s\n",
//spb026          GenericULong==NdisMediaStateDisconnected?"NdisMediaStateDisconnected"
//spb026                                                  :"NdisMediaStateConnected");
        #endif


        DEBUGMSG (ZONE_QUERY,
			(TEXT("PCX500:: OID_GEN_MEDIA_CONNECT_STATUS return [%s]\r\n"),
			GenericULong==NdisMediaStateDisconnected ? 
				TEXT("Disconnected"):
				TEXT("Connected")));

        break;

//***************************************************************************
// BEGIN 802.11 OIDs -- (query)
//***************************************************************************

    //-----------------------------------------------------------------------
    // This object defines the Service Set Identifier. The SSID is a string, 
    // up to 32 characters. It identifies a set of interconnected Basic Service 
    // Sets. Passing in an empty string means associate with any SSID. Setting 
    // an SSID would result in disassociating if already associated with a 
    // particular SSID, turning on the radio if the radio is currently in the 
    // off state, setting the SSID with the value specified or setting it to 
    // any SSID if SSID is not specified, and attempting to associate with the 
    // set SSID. 
    // 
    // Data type:   NDIS_802_11_SSID.
    // Query:       Returns the SSID.
    // Set:         Sets the SSID.
    // Indication:  Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_SSID: {
        MoveBytes       = sizeof(NDIS_802_11_SSID);
        MoveSource      = &ndis_ssid;
        NdisZeroMemory(&ndis_ssid,MoveBytes);

        if (NDIS_STATUS_MEDIA_CONNECT == card->m_MSenceStatus) {
            ndis_ssid.SsidLength = (card->m_SSID.SsidLength < 32)?card->m_SSID.SsidLength:32;
            NdisMoveMemory(ndis_ssid.Ssid,card->m_SSID.Ssid,ndis_ssid.SsidLength);
        }
    break;
    }


    //-----------------------------------------------------------------------
    // Query or Set how an 802.11 NIC connects to the network. Will also reset 
    // the network association algorithm.
    // 
    // Data type:  NDIS_802_11_NETWORK_INFRASTRUCTURE.
    // Query:      Returns either Infrastructure or Independent Basic Service Set 
    //             (IBSS), unknown.
    // Set:        Sets Infrastructure or IBSS, or automatic switch between the 
    //             two.
    // Indication: Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_INFRASTRUCTURE_MODE: {
        USHORT mode = card->m_activeProfile->zFlags.cfg.OpMode;
        if ((mode                   & 0x00FF) == 0) {
            ndis_infrastructure = Ndis802_11IBSS;
            }
        else if ((mode & 0x00FF) == 1) {
            ndis_infrastructure = Ndis802_11Infrastructure;
            }
        else {
            StatusToReturn = NDIS_STATUS_INVALID_DATA;
            }

        MoveSource  = &ndis_infrastructure;
        MoveBytes   = sizeof(NDIS_802_11_NETWORK_INFRASTRUCTURE);
        break;
        }


    //-----------------------------------------------------------------------
    // Sets the IEEE 802.11 authentication mode.
    // 
    // Data type:  NDIS_802_11_AUTHENTICATION_MODE.
    // Query:      Current mode.
    // Set:        Set to open or shared or auto-switch.
    // Indication: Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_AUTHENTICATION_MODE: {
        USHORT mode = card->m_activeProfile->zFlags.cfg.AuthenType;
        if (mode & AUTH_TYPE_OPEN) {
            ndis_authmode = Ndis802_11AuthModeOpen;
            }
        else if (mode & AUTH_TYPE_SHARED_KEY) {
            ndis_authmode = Ndis802_11AuthModeShared;
            }
        else {
            StatusToReturn = NDIS_STATUS_INVALID_DATA;
            }
        MoveSource  = &ndis_authmode;
        MoveBytes   = sizeof(NDIS_802_11_AUTHENTICATION_MODE);
        break;
        }


    //-----------------------------------------------------------------------
    // This object is the MAC address of the associated Access point. Setting 
    // is useful when doing a site survey.
    // 
    // Data type:   NDIS_802_11_MAC_ADDRESS.
    // Query:       Returns the current AP MAC address.
    // Set:         Sets the MAC address of the desired AP.
    // Indication:  Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_BSSID: {
        if (NDIS_STATUS_MEDIA_DISCONNECT == card->m_MSenceStatus) {
            StatusToReturn = NDIS_STATUS_ADAPTER_NOT_READY;
        }
        else {
            MoveSource  = &ndis_bssid;
            MoveBytes   = sizeof(NDIS_802_11_MAC_ADDRESS);
            NdisMoveMemory(&ndis_bssid, card->m_BSSID, MoveBytes);
        }
        break;
        }


    //-----------------------------------------------------------------------
    // Data type:  NDIS_802_11_NETWORK_TYPE.
    // Query:      Returns the current NDIS_802_11_NETWORK_TYPE used by the device.
    // Set:        Will set the network type that should be used for the driver. 
    // Indication: Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_NETWORK_TYPE_IN_USE: {
        if (card->m_activeProfile->zFlags.cfg.RadioType == 1) {
            ndis_nettype   = Ndis802_11FH;
            }
        else {
            ndis_nettype   = Ndis802_11DS;
            }
        MoveSource  = &ndis_nettype;
        MoveBytes   = sizeof(NDIS_802_11_NETWORK_TYPE);
        break;          
        }


    //-----------------------------------------------------------------------
    // A set of supported data rates in the operational rate set that the radio 
    // is capable of running at. Data rates are encoded as 8 octets where each 
    // octet describes a single supported rate in units of  0.5 Mbps. Supported 
    // rates belonging to the BSSBasicRateSet are used for frames such as control 
    // and broadcast frames. Each supported rate belonging to the BSSBasicRateSet 
    // is encoded as an octet with the msb (bit 7) set to 1 (e.g., a 1 Mbps rate 
    // belonging to the BSSBasicRateSet is encoded as 0x82). Rates not belonging 
    // to the BSSBasicRateSet are encoded with the msb set to 0 (e.g., a 2 Mbps 
    // rate not belonging to the BSSBasicRateSet is encoded as 0x04). 
    // 
    // Data type:  NDIS_802_11_RATES.
    // Query:      Returns the set of supported data rates the radio is capable 
    //             of running.
    // Set:        Not supported.
    // Indication: Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_RATES_SUPPORTED: {
        STCAPS caps;
        if (cmdCapsGet(card, &caps)) {
            NdisMoveMemory(&ndis_rates, &caps.au8SupportedRates, sizeof(NDIS_802_11_RATES));
            MoveSource  = &ndis_rates;
            MoveBytes   = sizeof(NDIS_802_11_RATES);
            }
        else {
            StatusToReturn = NDIS_STATUS_INVALID_DATA;
            }
        break;
        }


    //-----------------------------------------------------------------------
    // Configures the radio parameters.
    // 
    // Data type:  NDIS_802_11_CONFIGURATION.
    // Query:      Returns the current radio configuration.
    // Set:        Sets the radio configuration.
    // Indication: Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_CONFIGURATION: {
        STSTATUS status;
        if (cmdStatusGet(card, &status)) {
            ndis_config.Length              = sizeof(NDIS_802_11_CONFIGURATION);
            ndis_config.BeaconPeriod        = status.u16BeaconPeriod;
            ndis_config.ATIMWindow          = status.u16AtimDuration;
            ndis_config.DSConfig            = (BASE_FREQUENCY_MHZ + ((status.u16DsChannel - 1) * 5)) * 1000;
            ndis_config.FHConfig.Length     = sizeof(NDIS_802_11_CONFIGURATION_FH);
            ndis_config.FHConfig.HopPattern = status.u16HopPattern;
            // 
            // NOTE: u16DsChannel and u16HopSet are unioned in the STSTATUS structure.  
            // Since we don't support frequency hopping, we'll just set hopset to 0.
            // 
            ndis_config.FHConfig.HopSet     = 0;
            ndis_config.FHConfig.DwellTime  = status.u16HopPeriod;
            MoveSource  = &ndis_config;
            MoveBytes   = sizeof(NDIS_802_11_CONFIGURATION);
            }
        else {
            StatusToReturn = NDIS_STATUS_INVALID_DATA;
            }
        break;                  
        }


    //-----------------------------------------------------------------------
    // Returns the list of all BSSIDs detected including their attributes 
    // specified in the data structure. The list of BSSIDs returned is the cached 
    // list stored in the IEEE 802.11 NIC's database. The list of BSSIDs in the 
    // IEEE 802.11 NIC's database is the set of BSSs detected by the IEEE 802.11 NIC 
    // during the last survey of potential BSSs. A call to this OID should result in 
    // an immediate return of the list of BSSIDs in the IEEE 802.11 NIC's database. 
    // Note that if this OID is called without a preceding OID_802_11_BSSID_LIST_SCAN, 
    // and the IEEE 802.11 NIC is active, it may return a list of BSSIDs limited to 
    // those BSSIDs that the IEEE 802.11 NIC considers valid to join based on its 
    // current configuration. However, if this OID is immediately preceded by 
    // OID_802_11_BSSID_LIST_SCAN, then the list of BSSIDs should contain all the 
    // BSSIDs found during the OID_802_11_BSSID_LIST_SCAN.
    // 
    // Data type:  NDIS_802_11_BSSID_LIST.
    // Query:      Array of NDIS_802_11_BSSID_LIST structures.
    // Set:        Not supported. 
    // Indication: Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_BSSID_LIST: {

        #ifdef DEBUG_BSSID_LIST
        DbgPrint("OID_802_11_BSSID_LIST: enter\n");
        #endif

        RID_BSS ridBss;
        BOOLEAN emptyList = FALSE;
        BOOLEAN endOfList = FALSE;

        StatusToReturn  = NDIS_STATUS_SUCCESS;
        // 
        // Rebuild the list if we don't have a cached copy.
        // 
        if (card->bssid_list == NULL) {

            #ifdef DEBUG_BSSID_LIST
            DbgPrint("Rebuilding BSSID list\n");
            #endif

            NDIS_802_11_BSSID_LIST *ndis_bssid_list = NULL;
            // 
            // Get the first bss rid.
            // 
            if (cmdGetFirstBSS(card, &ridBss, &emptyList)) {
                NDIS_802_11_BSSID_LIST  *savebssid_list     = NULL;
                // 
                // Size of fixed portion of NDIS_802_11_BSSID_LIST.
                // 
                UINT fixedSize = sizeof(NDIS_802_11_BSSID_LIST) - sizeof(NDIS_WLAN_BSSID);

                #ifdef DEBUG_BSSID_LIST
                DbgPrint("sizeof(NDIS_802_11_BSSID_LIST)    = %d\n", sizeof(NDIS_802_11_BSSID_LIST));
                DbgPrint("sizeof(NDIS_WLAN_BSSID)           = %d\n", sizeof(NDIS_WLAN_BSSID));
                DbgPrint("fixedSize                         = %d\n", fixedSize);
                DbgPrint("\n");
                int loopCount = 0;
                #endif

                UINT curSize    = fixedSize;
                UINT count      = 0;
                // 
                // Loop through the rest of the list.
                // 
                do {

                    #ifdef DEBUG_BSSID_LIST
                    loopCount++;

                    char ssid[33];
                    NdisZeroMemory(ssid, 33);
                    NdisMoveMemory(ssid, (const char *)ridBss.ssid, MIN(ridBss.ssidLen),32);
                    DbgPrint("%d) BSSID = %0d:%0d:%0d:%0d:%0d:%0d, SSID = %s\n", 
                             loopCount,
                             (int)ridBss.bssid[0], 
                             (int)ridBss.bssid[1], 
                             (int)ridBss.bssid[2], 
                             (int)ridBss.bssid[3], 
                             (int)ridBss.bssid[4], 
                             (int)ridBss.bssid[5],
                             ssid);

//                    dumpMem(&ridBss, sizeof(RID_BSS), 1);
                    #endif


                    // 
                    // Make sure the radio supports 802.11
                    // 
                    if (ridBss.radioType != RADIO_TYPE_TMA_DS) {
                        count++;
                        #ifdef DEBUG_BSSID_LIST
                        DbgPrint("     count = %d\n", count);
                        #endif
                        // 
                        // Hang on to previous list.
                        // 
                        savebssid_list = ndis_bssid_list;
                        // 
                        // Allocate new list with one additional element.
                        // 
                        curSize += sizeof(NDIS_WLAN_BSSID);
                        ndis_bssid_list = (NDIS_802_11_BSSID_LIST*)(new char[curSize]);
                        if (ndis_bssid_list != NULL) {
                            NdisZeroMemory(ndis_bssid_list, curSize);
                            // 
                            // If there was a previous list, copy it into the new one.
                            // 
                            if (savebssid_list != NULL) {
                                NdisMoveMemory(ndis_bssid_list, 
                                               savebssid_list, 
                                               (curSize - sizeof(NDIS_WLAN_BSSID)));
                                delete savebssid_list;
                                savebssid_list = NULL;
                                }
                            // 
                            // Check for empty list
                            // 
                            if (emptyList) {
                                ndis_bssid_list->NumberOfItems = 0;
                                }
                            else {
                                ndis_bssid_list->NumberOfItems = count;
                                // 
                                // Just for convenience...
                                // 
                                NDIS_WLAN_BSSID *bss = &ndis_bssid_list->Bssid[count - 1];
                                // 
                                // Fill in new list element.
                                // 
                                NdisMoveMemory(bss->MacAddress, ridBss.bssid, sizeof(NDIS_802_11_MAC_ADDRESS));
                                NdisZeroMemory(&bss->Reserved, 2);

                                bss->Ssid.SsidLength                    = MIN((ULONG)ridBss.ssidLen,(ULONG)32);
                                NdisMoveMemory(bss->Ssid.Ssid, ridBss.ssid, bss->Ssid.SsidLength);

                                NdisMoveMemory(&bss->SupportedRates, &ridBss.rates, sizeof(NDIS_802_11_RATES));
                                bss->Length                             = sizeof(NDIS_WLAN_BSSID);

                                bss->Privacy                            = (ridBss.capability & CAP_PRIVACY) != 0;
                                bss->Rssi                               = ridBss.rssi;
                                bss->NetworkTypeInUse                   = (ridBss.radioType == RADIO_TYPE_FH)   ?
                                                                          Ndis802_11FH                          :
                                                                          Ndis802_11DS;
                                bss->Configuration.Length               = sizeof(NDIS_802_11_CONFIGURATION);
                                bss->Configuration.BeaconPeriod         = ridBss.beaconInterval;
                                bss->Configuration.ATIMWindow           = ridBss.atimWindow;
                                bss->Configuration.DSConfig             = (BASE_FREQUENCY_MHZ + ((ridBss.dsChannel - 1) * 5)) * 1000;
                                bss->Configuration.FHConfig.Length      = sizeof(NDIS_802_11_CONFIGURATION_FH);
                                bss->Configuration.FHConfig.HopPattern  = ridBss.fhInfo.hopPattern;
                                bss->Configuration.FHConfig.HopSet      = ridBss.fhInfo.hopSet;
                                bss->Configuration.FHConfig.DwellTime   = ridBss.fhInfo.dwellTime;
                                bss->InfrastructureMode                 = (ridBss.capability & CAP_ESS) ? 
                                                                          Ndis802_11Infrastructure      : 
                                                                          Ndis802_11IBSS;
                                }
                            }
                        else {
                            // 
                            // Memory allocation error.
                            // 
                            StatusToReturn = NDIS_STATUS_RESOURCES;
                            if (savebssid_list != NULL) {
                                delete savebssid_list;
                                }
                            }
                        }
                    else {
                        // 
                        // not 802.11 so return "not supported".
                        // 
                        #ifdef DEBUG_BSSID_LIST
                        DbgPrint("     skipped: does not support  802.11\n");
                        #endif

    //                    StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
                        }
                    } while ((StatusToReturn == NDIS_STATUS_SUCCESS) && cmdGetNextBSS(card, &ridBss, &endOfList) && !endOfList);

                if (StatusToReturn == NDIS_STATUS_SUCCESS) {
                    card->bssid_list        = ndis_bssid_list;
                    card->bssid_list_count  = count;
                    card->bssid_list_size   = curSize;
                    }
                else {
                    delete [] ndis_bssid_list;
                    }
                }
            else {
                StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
                }
            }
        #ifdef DEBUG_BSSID_LIST
        else {
            DbgPrint("Using cached BSSID list\n");
            }
        #endif


        if (StatusToReturn == NDIS_STATUS_SUCCESS) {
            MoveSource  = card->bssid_list;
            MoveBytes   = card->bssid_list_size;
            }

        #ifdef DEBUG_BSSID_LIST
        DbgPrint("OID_802_11_BSSID_LIST: leave\n");
        #endif

        break;
        }


    //-----------------------------------------------------------------------
    // Returns the Received Signal Strength Indication in dBm.
    // 
    // Data type:  NDIS_802_11_RSSI.
    // Query:      Returns the current RSSI value.
    // Set:        Not supported.
    // Indication: If an indication request is enabled, then an event is 
    //             triggered according to the value as given in the set.
    //-----------------------------------------------------------------------
    case OID_802_11_RSSI: {
        // 
        // If it's NULL then try to read again since it's possible the firmware
        // has been updated since we tried when the driver started up.
        // 
        if (card->dBmTable == NULL) {
            card->dBmTable = cmdDBMTableGet(card);
            }
        // 
        // If still NULL then this OID is not supported.
        // 
        if (card->dBmTable == NULL) {
            StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
            }
        else {
            STSTATUS status;
            if (cmdStatusGet(card, &status)) {
/*
                // percent is in the low byte -- don't need this.
                USHORT percent  = (card->dBmTable->table[status.u16SignalStrength] & 0x00FF);
*/
                USHORT rssi = (card->dBmTable->table[status.u16SignalStrength & 0x00FF] & 0xFF00) >> 8;
                ndis_rssi   = (NDIS_802_11_RSSI)rssi * -1;
                if (ndis_rssi < -90 && card->m_MSenceStatus == NDIS_STATUS_MEDIA_CONNECT) {
                    //This keeps netshell from displaying no signal when we are connect on XP.
                    ndis_rssi = -90;
                }

                MoveSource  = &ndis_rssi;
                MoveBytes   = sizeof(NDIS_802_11_RSSI);
                }
            else {
                StatusToReturn = NDIS_STATUS_INVALID_DATA;
                }
            }
        break;
        }

    //-----------------------------------------------------------------------
    // Query shall respond with the WEP status indicating whether WEP is enabled, 
    // WEP is disabled, WEP key is absent, or WEP is not supported. Query response 
    // indicating that WEP is enabled or WEP is disabled implies that WEP key is 
    // available for the NIC to encrypt data, that is, WEP key is available for 
    // MPDU transmission with encryption. Query response indicating WEP key is 
    // absent implies that WEP key is not available for the NIC to encrypt data 
    // and therefore WEP cannot be enabled or disabled. Note that this implies 
    // that while WEP key(s) may be available, the NIC does not have a WEP key 
    // for MPDU transmission with encryption. Query response indicating that WEP 
    // is not supported implies that the NIC does not support the desired WEP, 
    // that is, the NIC is not capable of encrypting data and hence WEP cannot be 
    // enabled or disabled. The NIC is permitted to set the WEP status to either 
    // enable or disable setting only. In order to be able to set the WEP status 
    // to either enable or disable setting, it is required that WEP key is 
    // available for MPDU transmission with encryption. If the set WEP status 
    // operation cannot be done, the driver shall return an NDIS_STATUS value of 
    // NDIS_STATUS_NOT_ACCEPTED.     
    // 
    // Data type:   NDIS_802_11_WEP_STATUS.
    // Query:       Returns the current WEP status.
    // Set:         Permitted to enable or disable WEP only.
    // Indication:  Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_WEP_STATUS: {
        STCAPS caps;
        if (cmdCapsGet(card, &caps)) {

            // Does card support WEP (40 bit) ?  
            //
            // (NOTE: We can check the SOFT_CAPS_128_BIT_WEP_SUPPORT bit to see if 
            //  128 bit WEP is supported).
            if (caps.u16SoftwareCapabilities & SOFT_CAPS_WEP_ENCRYPTION_SUPPORT) {
                // WEP is supported.  Now let's see if a WEP key is set.
                RID_WEP_Key keyBuf;

                // Read first WEP key RID.
                if (RidGet(card, RID_WEP_SESSION_KEY, &keyBuf, sizeof(RID_WEP_Key))) {

                    // Check key index.  0xFFFF indicates no keys, 4 indicates home key.
                    // All we're interested in are the enterprise keys (index = 0-3).
                    if ((keyBuf.keyindex == 0xFFFF) || (keyBuf.keyindex == 4)) {

                        // WEP key is not set.
                        ndis_wepstatus = Ndis802_11WEPKeyAbsent;
                        }
                    else {

                        // WEP is set, but is it enabled ?
                        if (card->m_activeProfile->zFlags.cfg.AuthenType & AUTH_TYPE_WEP) {
                            ndis_wepstatus = Ndis802_11WEPEnabled;
                            }
                        else {
                            ndis_wepstatus = Ndis802_11WEPDisabled;
                            }
                        }
                    }
                else {
                    StatusToReturn = NDIS_STATUS_INVALID_DATA;
                    }
                }
            else {
                ndis_wepstatus = Ndis802_11WEPNotSupported;
                }

            if (StatusToReturn == NDIS_STATUS_SUCCESS) {
                MoveSource  = &ndis_wepstatus;
                MoveBytes   = sizeof(NDIS_802_11_WEP_STATUS);
                }
            }
        else {
            StatusToReturn = NDIS_STATUS_INVALID_DATA;
            }
        break;
        }

//***************************************************************************
// END 802.11 OIDs
//***************************************************************************

#if NDISVER == 5
    case OID_PNP_CAPABILITIES:
        ndis_pnp_capablities.WakeUpCapabilities.MinMagicPacketWakeUp    = NdisDeviceStateUnspecified;
        ndis_pnp_capablities.WakeUpCapabilities.MinPatternWakeUp        = NdisDeviceStateUnspecified;
        ndis_pnp_capablities.WakeUpCapabilities.MinLinkChangeWakeUp     = NdisDeviceStateUnspecified;
                                                                                                 
        if (0xFFFF != (0xFFFF & card->m_MagicPacketMode)) {
          ndis_pnp_capablities.WakeUpCapabilities.MinMagicPacketWakeUp    = NdisDeviceStateD3;
    // this is necessary to have pattern enabled---2K + XP wont let you wake-on-lan otherwise
          ndis_pnp_capablities.WakeUpCapabilities.MinPatternWakeUp        = NdisDeviceStateD0;
                                                                                                 
          ndis_pnp_capablities.Flags = NDIS_DEVICE_WAKE_UP_ENABLE;
        }

        MoveSource      = (PVOID)&ndis_pnp_capablities; 
        MoveBytes       = sizeof(NDIS_PNP_CAPABILITIES);
        break;

    case OID_PNP_QUERY_POWER:
        MoveBytes       = 0;
        break;

    case OID_PNP_ENABLE_WAKE_UP:
        break;

    case OID_PNP_WAKE_UP_PATTERN_LIST:
        MoveBytes       = 0;
        break;

    case OID_GEN_PHYSICAL_MEDIUM:
        GenericULong    = NdisPhysicalMediumWirelessLan;
        break;

    case OID_TCP_TASK_OFFLOAD:
        if (card->mp_enabled == MP_SENT) {
            card->mp_enabled = MP_ENABLED;
        }
        StatusToReturn = NDIS_STATUS_NOT_SUPPORTED; //stub
        break;
#endif
    default: {
        //spb012
        NDIS_STATUS status=ExtendedOids(card, Oid, InfBuff, InfBuffLen, BytesWritten, BytesNeeded);        
        return status;
        } 
    }

    if (StatusToReturn == NDIS_STATUS_SUCCESS) {
        if (MoveBytes > InfBuffLen) {
            // Not enough room in InfBuff, Punt
            *BytesNeeded    = MoveBytes;
            StatusToReturn  = NDIS_STATUS_BUFFER_TOO_SHORT;
        } 
        else {
            if (MoveBytes) {
                // Store result.
                NdisMoveMemory(InfBuff, MoveSource, MoveBytes); 
                *BytesWritten = MoveBytes;
            }
        }
    }

    return StatusToReturn;
}

//===========================================================================
    NDIS_STATUS cbSetInformation (NDIS_HANDLE     Context,
                                  IN NDIS_OID     Oid,
                                  IN PVOID        InfBuff,
                                  IN ULONG        InfBuffLen,
                                  OUT PULONG      BytesRead,
                                  OUT PULONG      BytesNeeded)
//===========================================================================
// 
// Description: MiniportSetInformation.
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    PCARD           card = (PCARD)Context;
    NDIS_STATUS     StatusToReturn  = NDIS_STATUS_SUCCESS;  // Status of the operation.


//    DbgPrint("Oid Set   %X\n", Oid);

    //spb001 Finish Initialization if we haven't yet
    if (!card->initComplete) {
        InitFW1(card);
        InitInterrupts( card ); //spb010
    }

    if (TRUE == CheckCardFlashing(card,&StatusToReturn,Oid )) {  
        return StatusToReturn;
    }

#if NDISVER == 5
    if (card->mp_enabled == MP_ENABLED)
    { 
      InitFW(card, TRUE);
      card->mp_enabled = MP_POWERED;
      InitInterrupts( card );     //spb010
    } 
#endif

    //spb012
    cmdAwaken(card, TRUE);

    switch (Oid) {
    case OID_802_3_MULTICAST_LIST:
        if (InfBuffLen % AIRONET_LENGTH_OF_ADDRESS) {
            *BytesRead      = 0;
            *BytesNeeded    = 0;
            StatusToReturn  = NDIS_STATUS_INVALID_LENGTH;
            break;
            }
        card->m_CurMulticastSize = InfBuffLen / AIRONET_LENGTH_OF_ADDRESS;
        if (DEFAULT_MULTICASTLISTMAX < card->m_CurMulticastSize) {
            card->m_CurMulticastSize =  DEFAULT_MULTICASTLISTMAX;
            }

        NdisMoveMemory(card->m_Addresses, InfBuff, InfBuffLen);
        StatusToReturn = NDIS_STATUS_SUCCESS;
        break;

    case OID_GEN_CURRENT_PACKET_FILTER:

        if (InfBuffLen != 4) {
            *BytesRead      = 0;
            *BytesNeeded    = 0;
            StatusToReturn  = NDIS_STATUS_INVALID_LENGTH;
            break;
            }

        NdisMoveMemory(&card->m_PacketFilter, InfBuff, 4);

        if (card->m_PacketFilter & (NDIS_PACKET_TYPE_SOURCE_ROUTING
                                    | NDIS_PACKET_TYPE_SMT 
                                    | NDIS_PACKET_TYPE_MAC_FRAME 
                                    | NDIS_PACKET_TYPE_FUNCTIONAL 
                                    | NDIS_PACKET_TYPE_ALL_FUNCTIONAL 
                                    | NDIS_PACKET_TYPE_GROUP 
                                    | NDIS_PACKET_TYPE_PROMISCUOUS)) {
            *BytesRead      = 4;
            *BytesNeeded    = 0;
            StatusToReturn  = NDIS_STATUS_NOT_SUPPORTED;
            break;
            }
        #if 0
        SetPromiscuousMode(card, card->m_PacketFilter & NDIS_PACKET_TYPE_PROMISCUOUS );
        #endif
        break;

    case OID_GEN_CURRENT_LOOKAHEAD:

        if (InfBuffLen != 4) {
            *BytesRead      = 0;
            *BytesNeeded    = 0;
            StatusToReturn  = NDIS_STATUS_INVALID_LENGTH;
            break;
            }

        if (card->m_MaxLookAhead > AIRONET_MAX_LOOKAHEAD) {
            StatusToReturn = NDIS_STATUS_INVALID_LENGTH;
            }
        else {
            NdisMoveMemory(&card->m_MaxLookAhead, InfBuff, 4);
            }
        break;


//***************************************************************************
// BEGIN 802.11 OIDs (set)
//***************************************************************************

    //-----------------------------------------------------------------------
    // This object defines the Service Set Identifier. The SSID is a string, 
    // up to 32 characters. It identifies a set of interconnected Basic Service 
    // Sets. Passing in an empty string means associate with any SSID. Setting 
    // an SSID would result in disassociating if already associated with a 
    // particular SSID, turning on the radio if the radio is currently in the 
    // off state, setting the SSID with the value specified or setting it to 
    // any SSID if SSID is not specified, and attempting to associate with the 
    // set SSID. 
    // 
    // Data type:   NDIS_802_11_SSID.
    // Query:       Returns the SSID.
    // Set:         Sets the SSID.
    // Indication:  Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_SSID: {
        if (!cmdAwaken(card, TRUE)) {
            StatusToReturn = -1000;
            }
        else {
            STSSID ssid;
            if (cmdSSIDGet(card, &ssid)) {
                NDIS_802_11_SSID *ndis_ssid = (NDIS_802_11_SSID*)InfBuff;

                ssid.Len1 = (USHORT)ndis_ssid->SsidLength;
                NdisZeroMemory(ssid.ID1, 32);
                NdisMoveMemory(ssid.ID1, ndis_ssid->Ssid, ndis_ssid->SsidLength);

                if (cmdSSIDSet(card, &ssid, TRUE)) {
                    NdisZeroMemory(card->m_ESS_ID1, 32);
                    NdisMoveMemory(card->m_ESS_ID1, ndis_ssid->Ssid, ndis_ssid->SsidLength);
                    }
                else {
                    StatusToReturn = NDIS_STATUS_INVALID_DATA;
                    }
                }
            else {
                StatusToReturn = NDIS_STATUS_INVALID_DATA;
                }
            }
        break;
        }


    //-----------------------------------------------------------------------
    // Query or Set how an 802.11 NIC connects to the network. Will also reset 
    // the network association algorithm.
    // 
    // Data type:  NDIS_802_11_NETWORK_INFRASTRUCTURE.
    // Query:      Returns either Infrastructure or Independent Basic Service Set 
    //             (IBSS), unknown.
    // Set:        Sets Infrastructure or IBSS, or automatic switch between the 
    //             two.
    // Indication: Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_INFRASTRUCTURE_MODE: {
        if (!cmdAwaken(card, TRUE)) {
            StatusToReturn = -1000;
            }
        else {
            USHORT mode;
            StatusToReturn = getShortVal(InfBuffLen, InfBuff, &mode);
            if (StatusToReturn == NDIS_STATUS_SUCCESS) {
                // 
                // Our card only supports adhoc and infrastructure modes.
                // 
                // NOTE: The MS document, "IEEE 802.11 MIN Requirements", specifies 
                //       an additional mode of Ndis802_11AutoUnknown.  Since the card 
                //       doesn't support it, we don't support it.
                // 
                if ((mode == Ndis802_11IBSS) || (mode == Ndis802_11Infrastructure)) {
                    CFG_X500 cfg;
                    if (cmdConfigGet(card, &cfg)) {
                        // 
                        // NOTE: preserve the high byte as it contains a bit mask.
                        // 
                        cfg.OpMode = (cfg.OpMode & 0xFF00) | mode;
                        if (!cmdConfigSet(card, &cfg, TRUE)) {
                            StatusToReturn = NDIS_STATUS_INVALID_DATA;
                            }
                        }
                    else {
                        StatusToReturn = NDIS_STATUS_INVALID_DATA;
                        }
                    }
                else {
                    StatusToReturn = NDIS_STATUS_INVALID_DATA;
                    }
                }
            }
        break;
        }


    //-----------------------------------------------------------------------
    // The WEP key should not be held in permanent storage but should be lost 
    // as soon as the card disassociates with all BSSIDs or if shared key 
    // authentication using the WEP key fails. Calling twice for the same index 
    // should overwrite the previous value.
    // 
    // Data type:  NDIS_802_11_WEP.
    // Query:      Not supported.
    // Set:        Sets the desired WEP key.
    // Indication: Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_ADD_WEP: {
        //spb018  added if statement
        
        if (card->m_activeProfile->zFlags.cfg.HomeConfiguration) {
            card->m_activeProfile->zFlags.cfg.HomeConfiguration=0;
            cmdConfigSet(card,&card->m_activeProfile->zFlags.cfg,TRUE);
        }

        StatusToReturn = cmdAddWEP(card, (NDIS_802_11_WEP *)InfBuff);
        break;
        }


    //-----------------------------------------------------------------------
    // The WEP key should not be held in permanent storage but should be lost 
    // as soon as the card disassociates with all BSSIDs.
    // 
    // Data type:  NDIS_802_11_KEY_INDEX.
    // Query:      Not supported.
    // Set:        Removes the desired WEP key.
    // Indication: Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_REMOVE_WEP: {            
        USHORT index;
        StatusToReturn = getShortVal(InfBuffLen, InfBuff, &index);
        StatusToReturn = cmdRemoveWEP(card, index);
        break;
        }


    //-----------------------------------------------------------------------
    // Query shall respond with the WEP status indicating whether WEP is enabled, 
    // WEP is disabled, WEP key is absent, or WEP is not supported. Query response 
    // indicating that WEP is enabled or WEP is disabled implies that WEP key is 
    // available for the NIC to encrypt data, that is, WEP key is available for 
    // MPDU transmission with encryption. Query response indicating WEP key is 
    // absent implies that WEP key is not available for the NIC to encrypt data 
    // and therefore WEP cannot be enabled or disabled. Note that this implies 
    // that while WEP key(s) may be available, the NIC does not have a WEP key 
    // for MPDU transmission with encryption. Query response indicating that WEP 
    // is not supported implies that the NIC does not support the desired WEP, 
    // that is, the NIC is not capable of encrypting data and hence WEP cannot be 
    // enabled or disabled. The NIC is permitted to set the WEP status to either 
    // enable or disable setting only. In order to be able to set the WEP status 
    // to either enable or disable setting, it is required that WEP key is 
    // available for MPDU transmission with encryption. If the set WEP status 
    // operation cannot be done, the driver shall return an NDIS_STATUS value of 
    // NDIS_STATUS_NOT_ACCEPTED.     
    // 
    // Data type:   NDIS_802_11_WEP_STATUS.
    // Query:       Returns the current WEP status.
    // Set:         Permitted to enable or disable WEP only.
    // Indication:  Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_WEP_STATUS: {
        if (!cmdAwaken(card, TRUE)) {
            StatusToReturn = -1000;
            }
        else {
            USHORT wepState;
            StatusToReturn = getShortVal(InfBuffLen, InfBuff, &wepState);
            if ((wepState == Ndis802_11WEPDisabled) || (wepState == Ndis802_11WEPEnabled)) {
                STCAPS caps;
                StatusToReturn = NDIS_STATUS_INVALID_DATA;
                if (cmdCapsGet(card, &caps)) {

                    // Does card support WEP (40 bit) ?  

                    // (NOTE: We can check the SOFT_CAPS_128_BIT_WEP_SUPPORT bit to see if 
                    //  128 bit WEP is supported).
                    if (caps.u16SoftwareCapabilities & SOFT_CAPS_WEP_ENCRYPTION_SUPPORT) {

                        // WEP is supported.  Now let's see if a WEP key is set.
                        RID_WEP_Key keyBuf;

                        // Read first WEP key RID.
                        if (RidGet(card, RID_WEP_SESSION_KEY, &keyBuf, sizeof(RID_WEP_Key))) {

                            // Check key index.  0xFFFF indicates no keys, 4 indicates home key.
                            // All we're interested in are the enterprise keys (index = 0-3).
#ifndef UNDER_CE                            
                            if ((keyBuf.keyindex == 0xFFFF) || (keyBuf.keyindex == 4)) {
#else
                            if ((wepState == Ndis802_11WEPEnabled) && 
                                ((keyBuf.keyindex == 0xFFFF) || (keyBuf.keyindex == 4))){
#endif

                                // WEP key is not set.
                                StatusToReturn = NDIS_STATUS_NOT_ACCEPTED;
                                }
                            else {
                                StatusToReturn = NDIS_STATUS_SUCCESS;
                                CFG_X500 cfg;
                                if (cmdConfigGet(card, &cfg)) {
                                    if (wepState == Ndis802_11WEPEnabled) {
                                        cfg.AuthenType |= AUTH_TYPE_WEP;
                                        }
                                    else {
                                        cfg.AuthenType &= ~AUTH_TYPE_WEP;
                                        }
                                    
                                    if (!cmdConfigSet(card, &cfg, TRUE)) {
                                        StatusToReturn = NDIS_STATUS_INVALID_DATA;
                                        }
                                    }
                                else {
                                    StatusToReturn = NDIS_STATUS_INVALID_DATA;
                                    }
                                }
                            }
                        else {
                            StatusToReturn = NDIS_STATUS_INVALID_DATA;
                            }
                        }
                    else {
                        StatusToReturn = NDIS_STATUS_NOT_ACCEPTED;
                        }
                    }
                }
            else {
                StatusToReturn = NDIS_STATUS_NOT_ACCEPTED;
                }
            }
        break;
        }

    //-----------------------------------------------------------------------
    // Disassociate with current SSID and turn the radio off.
    // 
    // Data type:  No data is associated with this Set.
    // Query:      Not supported.
    // Set:        Disassociates with current SSID and turn the radio off.
    // Indication: Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_DISASSOCIATE: {
        // 
        // Use OID_RADIO_OFF to disassociate.
        // 
        if (card->IsAssociated) {
            StatusToReturn = ExtendedOids(card, OID_RADIO_OFF, InfBuff, InfBuffLen, BytesRead, BytesNeeded);
            }
        else {
            StatusToReturn = NDIS_STATUS_ADAPTER_NOT_READY;
            }
        break;
        }


    //-----------------------------------------------------------------------
    // Sets the IEEE 802.11 authentication mode.
    // 
    // Data type:  NDIS_802_11_AUTHENTICATION_MODE.
    // Query:      Current mode.
    // Set:        Set to open or shared or auto-switch.
    // Indication: Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_AUTHENTICATION_MODE: {
        if (!cmdAwaken(card, TRUE)) {
            StatusToReturn = -1000;
            }
        else {
            USHORT mode;
            StatusToReturn = getShortVal(InfBuffLen, InfBuff, &mode);
            if (StatusToReturn == NDIS_STATUS_SUCCESS) {
                // 
                // Our card only supports open and shared key authentication.  
                // 
                // NOTE: The MS document, "IEEE 802.11 MIN Requirements", specifies 
                //       an additional authentication mode of Ndis802_11AuthModeAutoSwitch.
                //       Since the card doesn't support it, we don't support it.
                // 
                if ((mode == Ndis802_11AuthModeOpen) || (mode == Ndis802_11AuthModeShared)) {
                    CFG_X500 cfg;
                    if (cmdConfigGet(card, &cfg)) {
                        // 
                        // NOTE: preserve the high byte as it contains a bit mask.
                        // 
                        cfg.AuthenType = (mode == Ndis802_11AuthModeOpen)               ?
                                         (cfg.AuthenType & 0xFF00) | AUTH_TYPE_OPEN     :
                                         (cfg.AuthenType & 0xFF00) | AUTH_TYPE_SHARED_KEY;
                        if (!cmdConfigSet(card, &cfg, TRUE)) {
                            StatusToReturn = NDIS_STATUS_INVALID_DATA;
                            }
                        }
                    else {
                        StatusToReturn = NDIS_STATUS_INVALID_DATA;
                        }
                    }
                else {
                    StatusToReturn = NDIS_STATUS_INVALID_DATA;
                    }
                }
            }
        break;
        }


    //-----------------------------------------------------------------------
    // Data type:  NDIS_802_11_NETWORK_TYPE.
    // Query:      Returns the current NDIS_802_11_NETWORK_TYPE used by the device.
    // Set:        Will set the network type that should be used for the driver. 
    // Indication: Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_NETWORK_TYPE_IN_USE: {
        if (!cmdAwaken(card, TRUE)) {
            StatusToReturn = -1000;
            }
        else {
            USHORT type;
            StatusToReturn = getShortVal(InfBuffLen, InfBuff, &type);
            if (StatusToReturn == NDIS_STATUS_SUCCESS) {
                if ((type == Ndis802_11FH) || (type == Ndis802_11DS)) {
                    CFG_X500 cfg;
                    if (cmdConfigGet(card, &cfg)) {
                        cfg.RadioType = (type == Ndis802_11FH) ? (USHORT)RADIO_TYPE_FH : (USHORT)RADIO_TYPE_DS;
                        if (!cmdConfigSet(card, &cfg, TRUE)) {
                            StatusToReturn = NDIS_STATUS_INVALID_DATA;
                            }
                        }
                    else {
                        StatusToReturn = NDIS_STATUS_INVALID_DATA;
                        }
                    }
                else {
                    StatusToReturn = NDIS_STATUS_INVALID_DATA;
                    }
                }
            }
        break;          
        }


    //-----------------------------------------------------------------------
    // This object is the MAC address of the associated Access point. Setting 
    // is useful when doing a site survey.
    // 
    // Data type:   NDIS_802_11_MAC_ADDRESS.
    // Query:       Returns the current AP MAC address.
    // Set:         Sets the MAC address of the desired AP.
    // Indication:  Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_BSSID: {
        StatusToReturn =  NDIS_STATUS_ADAPTER_NOT_READY;
        if (card->IsAssociated) {
            if (!cmdAwaken(card, TRUE)) {
                StatusToReturn = -1000;
                }
            else {
                STAPLIST aplist;
                NDIS_802_11_MAC_ADDRESS  *ndis_bssid = (NDIS_802_11_MAC_ADDRESS*)InfBuff;;
                if (cmdAPsGet(card, &aplist)) {
                    NdisMoveMemory(aplist.AP1, ndis_bssid, sizeof(NDIS_802_11_MAC_ADDRESS));
                    if (cmdAPsSet(card, &aplist)) {
                        NdisMoveMemory(card->m_BSSID, ndis_bssid, sizeof(NDIS_802_11_MAC_ADDRESS));
                        }
                    else {
                        StatusToReturn = NDIS_STATUS_INVALID_DATA;
                        }
                    }
                else {
                    StatusToReturn = NDIS_STATUS_INVALID_DATA;
                    }
                }
        }
        break;                                
        }


    //-----------------------------------------------------------------------
    // Configures the radio parameters.
    // 
    // Data type:  NDIS_802_11_CONFIGURATION.
    // Query:      Returns the current radio configuration.
    // Set:        Sets the radio configuration.
    // Indication: Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_CONFIGURATION: {
        if (!cmdAwaken(card, TRUE)) {
            StatusToReturn = -1000;
            }
        else {
            NDIS_802_11_CONFIGURATION *ndis_cfg = (NDIS_802_11_CONFIGURATION*)InfBuff;
            CFG_X500 cfg;
            if (cmdConfigGet(card, &cfg)) {
                cfg.BeaconPeriod    = (USHORT)ndis_cfg->BeaconPeriod;
                cfg.AtimDuration    = (USHORT)ndis_cfg->ATIMWindow;
                cfg.DSChannel       = (USHORT)((((ndis_cfg->DSConfig / 1000) - BASE_FREQUENCY_MHZ) / 5) + 1);
    /*
                Ignore HopSet since we don't support hopping and this value is unioned with
                cfg.DSChannel.

                cfg.HopSet          = ndis_cfg->FHConfig.HopSet;
    */
                cfg.HopPattern      = (USHORT)ndis_cfg->FHConfig.HopPattern;
                cfg.HopPeriod       = (USHORT)ndis_cfg->FHConfig.DwellTime;

                if (!cmdConfigSet(card, &cfg, TRUE)) {
                    StatusToReturn = NDIS_STATUS_INVALID_DATA;
                    }
                }
            else {
                StatusToReturn = NDIS_STATUS_INVALID_DATA;
                }
            }
        break;
        }


    //-----------------------------------------------------------------------
    // A call to this OID will cause the IEEE 802.11 NIC to request a survey 
    // of potential BSSs using the following primitive parameters: BSSType = both 
    // infrastructure BSS and independent BSS, BSSID = broadcast BSSID, SSID = 
    // broadcast SSID, ScanType = either active, passive, or a combination of both 
    // passive and active scanning approaches, ChannelList = all permitted 
    // frequency channels. On receiving a call to this OID the IEEE 802.11 NIC may 
    // perform a scan using either active, passive, or a combination of both 
    // passive and active scanning approaches and refresh the list of BSSIDs in 
    // the IEEE 802.11 NIC's database. The IEEE 802.11 NIC should minimize the 
    // duration to complete a call to this OID and hence active scanning is 
    // preferred whenever deemed appropriate. Note that if the IEEE 802.11 NIC is 
    // currently associated with a particular BSSID and SSID that is not contained 
    // in the list of BSSIDs generated by this scan, the BSSDescription of the 
    // currently associated BSSID and SSID should be appended to the list of BSSIDs 
    // in the IEEE 802.11 NIC's database. Responses from all BSSs on frequency 
    // channels that are permitted in the particular region where the IEEE 802.11 
    // NIC is currently operating should be included in the list of BSSIDs in the 
    // IEEE 802.11 NIC's database.      
    // 
    // Data type:  No data is associated with this Set.
    // Query:      Not supported.
    // Set:        Performs a survey of potential BSSs.
    // Indication: Not supported.
    //-----------------------------------------------------------------------
    case OID_802_11_BSSID_LIST_SCAN: {
        if (!cmdAwaken(card, TRUE)) {
            StatusToReturn = -1000;
            }
        else {
            if (!cmdBssidListScan(card)) {
                StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
                }
            else {
                // 
                // Blow away our BSSID list cache after a rescan.
                // 
                delete [] card->bssid_list;
                card->bssid_list        = NULL;
                card->bssid_list_count  = 0;
                card->bssid_list_size   = 0;
                }
            }
        break;                                
        }


    //-----------------------------------------------------------------------
    // A call to this OID will cause the IEEE 802.11 NIC Driver/Firmware to 
    // reload the available default settings for the specified type field. For 
    // example, when reload defaults is set to WEP keys, the IEEE 802.11 NIC 
    // Driver/Firmware shall reload the available default WEP key settings from 
    // the permanent storage, i.e., registry key or flash settings, into the 
    // running configuration of the IEEE 802.11 NIC that is not held in the 
    // permanent storage. Therefore, a call to this OID with the reload defaults 
    // set to WEP keys ensures that the WEP key settings in the IEEE 802.11 NIC 
    // running configuration are returned to the configuration following the 
    // enabling of the interface from a disabled state, i.e., when the IEEE 
    // 802.11 Driver is loaded upon enabling the IEEE 802.11 NIC.   
    // 
    // Data type:   NDIS_802_11_RELOAD_DEFAULTS.
    // Query:       Not supported.
    // Set:         Set results in the reloading of the available defaults for 
    //              specified type field.
    // Indication:  Not supported. 
    //-----------------------------------------------------------------------
    case OID_802_11_RELOAD_DEFAULTS: {
        if (!cmdAwaken(card, TRUE)) {
            StatusToReturn = -1000;
            }
        else {
            USHORT whichDefaults;
            StatusToReturn = getShortVal(InfBuffLen, InfBuff, &whichDefaults);
            if (StatusToReturn == NDIS_STATUS_SUCCESS) {
                if (whichDefaults == Ndis802_11ReloadWEPKeys) {
                    StatusToReturn = cmdRestoreWEP(card);
                    }
                else {
                    StatusToReturn = NDIS_STATUS_INVALID_DATA;
                    }
                }
            }
        break;
        }

//***************************************************************************
// END 802.11 OIDs
//***************************************************************************

#if NDISVER == 5
    case OID_PNP_ENABLE_WAKE_UP:
        if (0xFFFF != (0xFFFF & card->m_MagicPacketMode)) {
          CmdDisableInterrupts(card);
          cmdMagicPacket( card, TRUE );  //raf
          card->mp_enabled = MP_SENT;
        }
        break;

    case OID_PNP_ADD_WAKE_UP_PATTERN:
        break;

    case OID_PNP_REMOVE_WAKE_UP_PATTERN:
        break;

    case OID_PNP_SET_POWER:
        *BytesRead = 4;
        *BytesNeeded = 0;
        switch (*(ULONG *)InfBuff) {
            case NdisDeviceStateD0:
                // 
                // Initialize firmware after hibernation 
                // 
                InitFW(card, TRUE);
                InitInterrupts(card);
                break;
        
            case NdisDeviceStateD1:
                //spbMgc check for magic packet mode.
                #if DBG
                    DbgPrint("Going to D1 state\n");
                #endif
                if (0xFFFF == (0xFFFF & card->m_MagicPacketMode))
                    cmdDisable(card);
                break;

            case NdisDeviceStateD2:
                //spbMgc check for magic packet mode.
                #if DBG
                    DbgPrint("Going to D2 state\n");
                #endif
                if (0xFFFF == (0xFFFF & card->m_MagicPacketMode))
                    cmdDisable(card);
                break;

            case NdisDeviceStateD3:
                //spbMgc check for magic packet mode.
                #if DBG
                    DbgPrint("Going to D3 state\n");
                #endif
                if (0xFFFF == (0xFFFF & card->m_MagicPacketMode))
                    cmdDisable(card);
                break;
            }
        break;
#endif

    default:
        StatusToReturn  = NDIS_STATUS_INVALID_OID;
        *BytesRead      = 0;
        *BytesNeeded    = 0;
        break;
    }


    if (StatusToReturn == NDIS_STATUS_SUCCESS) {
        if (BytesRead) {
            *BytesRead = InfBuffLen;
            }
        if (BytesNeeded) {
            *BytesNeeded = 0;
            }
    }

    return StatusToReturn ;
}


//===========================================================================
    NDIS_STATUS getShortVal (ULONG     InfBuffLen,
                             PVOID     InfBuff,
                             PUSHORT   shortVal)    
//===========================================================================
// 
// Description: Return the value pointed to by InfBuff as a USHORT.  The 
//              value pointed to may be 2 or 4 bytes
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    NDIS_STATUS retval = NDIS_STATUS_SUCCESS;
    *shortVal = 0;
    if (InfBuffLen == 4) {
        *shortVal = (USHORT)(*((ULONG*)InfBuff));
        }
    else if (InfBuffLen == 2) {
        *shortVal = *((USHORT*)InfBuff);
        }
    else {
        retval = NDIS_STATUS_INVALID_LENGTH;
        }
    return(retval);
}

void ReleaseSendQ(PCARD card)
{
    PNDIS_PACKET    Packet;

    while ( Packet = (PNDIS_PACKET)CQGetNext(card->m_Q) ){
        NdisMSendComplete(  card->m_MAHandle, Packet, NDIS_STATUS_SUCCESS );
    }
}

BOOLEAN
CopyDownPacketPortUShort(
    PCARD               card,
    PNDIS_PACKET        Packet
    )
{
    BOOLEAN         rem = FALSE;
#ifndef UNDER_CE
    UCHAR           remBuf[2];
#endif
    PUCHAR          CurBufAddress;
    PNDIS_BUFFER    CurBuffer;              // Current NDIS_BUFFER that is being copied from
    UINT            CurBufLen;              // Length of each of the above buffers
    UINT            PacketTxLen;
#ifdef UNDER_CE    
    UINT            PacketTxLenBeforePadding;
#endif


    if( FALSE == card->m_CardStarted ){
        return FALSE;
    }

    if( 0==card->KeepAwake && card->m_MaxPSP 
        && 0==(card->m_IntMask&INT_EN_AWAKE)){
        
        card->m_IntMask |= INT_EN_AWAKE;    
        EnableHostInterrupts(card);
    }

    if( FALSE == card->IsAwake ){
        cmdAwaken(card);
        return FALSE;
    }   
    // Skip 0 length copies       
    NdisQueryPacket(Packet, NULL, NULL, &CurBuffer, &PacketTxLen);
    if (0 == PacketTxLen ){
        return TRUE;
    }

#ifdef UNDER_CE
    PacketTxLenBeforePadding = PacketTxLen;
#endif

    if( PacketTxLen < 60 )
        PacketTxLen = 60;

    if( FALSE == XmitOpen( card, PacketTxLen ) ){
        return FALSE;
    }

    char    tmpBuf[ 1514 ];
    char    *ptmpPtr = tmpBuf;

#ifdef UNDER_CE
    //
    //  If it is < 60 pad it with 0x00.
    //

    if (PacketTxLenBeforePadding < 60)
    {
        memset(
            &tmpBuf[PacketTxLenBeforePadding], 
            0x00, 
            (60 - PacketTxLenBeforePadding));
    }
#endif
    
    while( (NDIS_BUFFER *)CurBuffer ){
        NdisQueryBuffer(CurBuffer, (PVOID *)&CurBufAddress, &CurBufLen);
        
        if ( 0==CurBufLen || NULL == CurBufAddress ){
            NdisGetNextBuffer(CurBuffer, &CurBuffer);
            continue;
        }
        
        memcpy(ptmpPtr, CurBufAddress, CurBufLen );
        ptmpPtr += CurBufLen;
        NdisGetNextBuffer(CurBuffer, &CurBuffer);
    }

    NdisRawWritePortBufferUshort(card->m_XmitReg, tmpBuf, (PacketTxLen+1)/2 );

    BOOLEAN ret =  XmitClose(card);

    return ret;
}

NDIS_STATUS
cbTransferData(
    OUT PNDIS_PACKET    Pkt,
    OUT PUINT           BytesTransferred,
    NDIS_HANDLE         Context,
    IN NDIS_HANDLE      MiniportReceiveContext,
    IN UINT             ByteOffset,
    IN UINT             BytesToTransfer
    )
{
    show(DebugFlag,0x900);
    PCARD               card = (PCARD)Context;
    UINT                BytesRemain;
    PUCHAR              CardPtr;
    UINT                BytesToCopy, BytesNow ;
    PNDIS_BUFFER        CurBuffer;      // Current NDIS_BUFFER to copy into
    PUCHAR              PktBuf;         // 

    // See how much data to be transferred
    *BytesTransferred = 0;

#if NDISVER == 5
	if (card->mp_enabled != MP_POWERED)
      return NDIS_STATUS_FAILURE;
#endif

    if ( card->m_PacketRxLen < ByteOffset ) 
      return NDIS_STATUS_FAILURE;
 
    BytesRemain     = card->m_PacketRxLen - ByteOffset;
    BytesToCopy     = BytesRemain > BytesToTransfer ? BytesToTransfer : BytesRemain;
    CardPtr         = card->m_Lookahead + ByteOffset + AIRONET_HEADER_SIZE;

    // Get location to copy into
    NdisQueryPacket(Pkt, NULL, NULL, &CurBuffer, NULL);
    
    // Loop, filling each buffer in the packet until all were copied.
    while ( CurBuffer && BytesToCopy > 0 ) {

        UINT            BufLen;         // Length and offset into the buffer.
        NdisQueryBuffer( CurBuffer, (PVOID *)&PktBuf, &BufLen);
        if ( 0 == BufLen ){
            NdisGetNextBuffer(CurBuffer, &CurBuffer);
            continue;
        }

            // See how much data to read into this buffer.
        BytesNow            = BufLen > BytesToCopy ? BytesToCopy : BufLen;
        NdisMoveMemory( PktBuf, CardPtr, BytesNow);
        // Update offsets and counts
        CardPtr             += BytesNow;
        BytesToCopy         -= BytesNow;
        *BytesTransferred   += BytesNow;                
        //
        NdisGetNextBuffer(CurBuffer, &CurBuffer);
    }
    if( *BytesTransferred + ByteOffset >= card->m_PacketRxLen )
        card->m_IndicateReceiveDone = TRUE;

    return  NDIS_STATUS_SUCCESS;
}

#if NDISVER == 3
INDICATE_STATUS
IndicatePacket(
    PCARD   card
    )
{

    UINT IndicateLen;           // Length of the lookahead buffer

    //if ((card->m_PacketRxLen+AIRONET_HEADER_SIZE) > sizeof(card->m_Lookahead) ){
    if (card->m_PacketRxLen > (1514-AIRONET_HEADER_SIZE)){
        return SKIPPED;
    }

    // Lookahead amount to indicate
    IndicateLen = (card->m_PacketRxLen > card->m_MaxLookAhead) ? card->m_MaxLookAhead : card->m_PacketRxLen;

    // Indicate packet
    BOOLEAN Locked = MINIPORT_LOCK_ACQUIRED((PNDIS_MINIPORT_BLOCK)card->m_MAHandle);
    MINIPORT_LOCK_ACQUIRED((PNDIS_MINIPORT_BLOCK)card->m_MAHandle) = TRUE;
    NdisMEthIndicateReceive(
            card->m_MAHandle,
            (NDIS_HANDLE)card,
            (PCHAR)card->m_Lookahead,
            AIRONET_HEADER_SIZE,
            (PCHAR)card->m_Lookahead + AIRONET_HEADER_SIZE,
            IndicateLen,
            card->m_PacketRxLen );  

    MINIPORT_LOCK_ACQUIRED((PNDIS_MINIPORT_BLOCK)card->m_MAHandle) = Locked;
    if( IndicateLen == card->m_PacketRxLen )
        card->m_IndicateReceiveDone = TRUE;

    return INDICATE_OK;
}
#endif

void        
XmitDpc(
    PCARD   card
    )
{
    DoNextSend(card );
}

#ifdef  INVALIDPACKETS
void            
Reader80211(
    PCARD   card
    )
{
    USHORT  fid;
    USHORT  tmpLen;
    UCHAR   tmp[8];

    //int   off = sizeof(RXFID_HDR) - sizeof(u16);
    int off = 0;
    USHORT  usWord;
    NdisRawReadPortUshort( card->m_PortIO+REG_FID_RX, &fid);        // Get the FID value
    
    if( !fid )
        return;

    NdisRawWritePortUshort( card->m_PortIO+REG_BAPrx_SEL, fid );    // write the FID value to the BAP
    NdisRawWritePortUshort( card->m_PortIO+REG_BAPrx_OFF, off );    // write the FID offset to the BAP

   
    USHORT  offValue;   
    int     delay = 1;  
    do {
        NdisRawReadPortUshort( card->m_PortIO+REG_BAPrx_OFF, &offValue );
        DelayUS( delay);
    }while( (0x8000 & offValue) && 0==(0x4000&offValue) && 200 > (delay<<=1) );

    UCHAR   tmp1[ 100 ]; // 2 for etype
    NdisRawReadPortBufferUshort( card->m_PortIO+REG_BAPrx_DATA,tmp1 , 100/2 );
    SAY("\nInvalid Fid Dump Begin\n");
    for( int i=0; i<100; i++ )
        SAY2("%02X ", tmp1[i]);
    SAY("\nInvalid Fid Dump End\n");
}
#endif

ULONG           
GetRxHeader(
    PCARD   card,
    PUCHAR  RcvBuf
    )
{
    UCHAR   LenBuf[4];
    NdisRawReadPortBufferUshort( card->m_IOBase+REG_BAPrx_DATA, LenBuf , 2 );
    card->m_PacketRxLen =  *(USHORT *)(LenBuf+2) -2;    // e type not included in the count 
    
    int len = ( sizeof(HDR_802_3) + 2 )/2; 
    NdisRawReadPortBufferUshort( card->m_IOBase+REG_BAPrx_DATA, RcvBuf , len );
    
    
    if( card->m_PacketRxLen <= 0   ||  card->m_PacketRxLen > 1500 ){
        return card->m_PacketRxLen = 0;
    }
                    
    return card->m_PacketRxLen;
}

#if NDISVER == 3
void
RcvDpc(
    PCARD   card
    )
{
    INDICATE_STATUS     IndicateStatus = INDICATE_OK;

    show(DebugFlag,0x200);
    
    card->m_IndicateReceiveDone = FALSE;
    ++card->m_ReceivePacketCount;

    card->m_PacketRxLen = GetRxHeader(card, card->m_RcvBuf );

    if( 0 == card->m_PacketRxLen ){
        AckRcvInterrupt(card);
        return;
    }
    if ( card->m_PromiscuousMode            ||
        IsMyPacket(card, card->m_Lookahead) || 
        CheckMulticast(card, card->m_Lookahead )){ 

        CopyUpPacketUShort( card, card->m_Lookahead+AIRONET_HEADER_SIZE, card->m_PacketRxLen );
        AckRcvInterrupt(card);
        IndicateStatus = IndicatePacket(card);
    }
    else
        AckRcvInterrupt(card);

    if ( card->m_IndicateReceiveDone ) {
        //show(DebugFlag,0x270);
        BOOLEAN Locked = MINIPORT_LOCK_ACQUIRED((PNDIS_MINIPORT_BLOCK)card->m_MAHandle);
        MINIPORT_LOCK_ACQUIRED((PNDIS_MINIPORT_BLOCK)card->m_MAHandle) = TRUE;
        NdisMEthIndicateReceiveComplete(card->m_MAHandle);
        MINIPORT_LOCK_ACQUIRED((PNDIS_MINIPORT_BLOCK)card->m_MAHandle) = Locked;
        card->m_IndicateReceiveDone = FALSE;
    }
}  
#endif

void
CopyUpPacketUShort(
    PCARD   card,
    IN PUCHAR TargetBuffer,
    IN UINT BufferLength
    )
{
#if 1
    BufferLength        = BufferLength/2 + (1&BufferLength) ;
    NdisRawReadPortBufferUshort( card->m_RcvReg, TargetBuffer, BufferLength );
#else
    BufferLength        += (1&BufferLength) ? 1 :0;
    for( UINT i=0; i< BufferLength; i+=2) 
        NdisRawReadPortUshort( card->m_RcvReg, (USHORT *)(TargetBuffer+i));
#endif
}

//*******************************************************************
//===========================================================================
    NDIS_STATUS cbSend (NDIS_HANDLE     Context,
                        PNDIS_PACKET    Packet, 
                        UINT            Flags)
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
#ifdef SOFTEX
    if (!ADAPTER_READY(card)) {
        return NDIS_STATUS_SUCCESS;
        }
#endif  
    if (card->m_IsFlashing || !card->m_CardStarted /*|| ! IsCardInserted(card)*/) {
        return NDIS_STATUS_SUCCESS;
        }

    if (CQIsFull(card->m_Q)) {
        if (0 == CQGetNextND(card->fidQ)) {
            return NDIS_STATUS_RESOURCES;
            }
    
        if (DoNextSend(card)) {
            CQStore( card->m_Q, (UINT)Packet);
            return NDIS_STATUS_PENDING;
            }
        return NDIS_STATUS_RESOURCES;
        }
    
    CQStore( card->m_Q, (UINT)Packet);
    
    if (CQGetNextND(card->fidQ)) {
        DoNextSend(card);
        }

    return NDIS_STATUS_PENDING;
}

#if NDISVER == 3
//===========================================================================
    BOOLEAN DoNextSend (PCARD card, PNDIS_PACKET Packet)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    if( card->m_IsFlashing )
        return FALSE;

    if( card->m_IsInDoNextSend )
        return FALSE;
    
    if( !card->m_PrevCmdDone )
        return FALSE;

    card->m_IsInDoNextSend = TRUE;

    if ( NULL == ( card->m_Packet = (PNDIS_PACKET)CQGetNextND(card->m_Q)) ){
        card->m_IsInDoNextSend = FALSE;
        return FALSE;
    }

    if( FALSE == CopyDownPacketPortUShort( card, card->m_Packet ) ){
        card->m_IsInDoNextSend = FALSE;
        return FALSE;
    }
    CQUpdate(card->m_Q);
    NdisMSendComplete(  card->m_MAHandle, card->m_Packet, NDIS_STATUS_SUCCESS );
    card->m_IsInDoNextSend = FALSE;
    return TRUE;
}

//===========================================================================
    void AckPendingPackets (PCARD card)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    PNDIS_PACKET Packet;
    while (Packet = (PNDIS_PACKET)CQGetNext(card->m_Q)) {
        NdisMSendComplete(card->m_MAHandle, Packet, NDIS_STATUS_SUCCESS);
        }
}
#endif

//==========================================================================


//===========================================================================
    void LogError (PCARD                card,
                   IN  NDIS_ERROR_CODE  ErrorCode,
                   IN  const int        AiroCode)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    if (card != NULL) {
        LogError(card->m_MAHandle, ErrorCode, AiroCode);
        }
}

//===========================================================================
    void LogError (IN  NDIS_HANDLE             adapterHandle,
                   IN  NDIS_ERROR_CODE         ErrorCode,
                   IN  const int               AiroCode)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    NdisWriteErrorLogEntry(adapterHandle, ErrorCode, 1, AiroCode);
}

//===========================================================================
    void setAutoConfigTimer (PCARD card, BOOLEAN includeMediaDisconnectTimer /* = FALSE */)
//===========================================================================
// 
// Description: Set the auto config timer.  The value used is based on the  
//              card's current scan mode.  Optionally include the media 
//              disconnect timer.
//    
//      Inputs: card                        - pointer to card structure.
//              includeMediaDisconnectTimer - if true, include the media
//                                            disconnect timer.
//    
//     Returns: nothing.
//    
//      (11/17/00)
//---------------------------------------------------------------------------
{
    CFG_X500 cfg;
    cmdConfigGet(card, &cfg);
    card->m_AutoConfigDamperTick = (cfg.ScanMode == SCANMODE_ACTIVE) ? 
                                    card->m_AutoConfigActiveDamper : 
                                    card->m_AutoConfigPassiveDamper;
    if (includeMediaDisconnectTimer) {
        card->m_AutoConfigDamperTick += card->m_MediaDisconnectDamper;
        }
}

//===========================================================================
    void CardTimerCallback (IN PVOID SystemSpecific1,        // ignored
                            PCARD    card,
                            IN PVOID SystemSpecific2,        // ignored
                            IN PVOID SystemSpecific3)         // ignored
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
//	DbgPrint("Card Timer\n");

    //spb001 Finish Initialization if we haven't yet
    if (!card->initComplete) {
        InitFW1(card);
        InitInterrupts( card ); //spb010
    }

    //spb020 Added if 
    if (FALSE==card->cancelTimer)  {    
        //Reschedule timer
        NdisMSetTimer(&card->m_CardTimer, 1 * 1000);
        card->m_CardTimerIsSet=TRUE;    //spb020
    }
    else {
        //Let card shutdown know we have fired
        card->m_CardTimerIsSet=FALSE;   //spb020
    }

#ifdef SOFTEX
    if (!ADAPTER_READY(card)) {
        return;
        }
#endif  

#if NDISVER == 5
    if (card->mp_enabled != MP_POWERED) 
	  return;
#endif

    if ( card->m_IsFlashing ) {
        return;
    }
    if (card->KeepAwake) {
        --card->KeepAwake;
        }

    // 
    // Kick start previous command.  Apparently, commands will sometimes
    // get "stuck".  Sending a NOP frees things up.
    // 
    if (card->m_PrevCmdTick && (++card->m_PrevCmdTick > 5)) {
        card->m_PrevCmdTick = 0;
        card->m_PrevCommand = TRUE;
        NdisRawWritePortUshort(card->m_IOBase+REG_CMD, CMD_X500_NOP10);
        }
    // 
    // Ack any remaining packets.  If we don't, it apparently causes problems on 
    // some OS (98 I think...).
    // 
    if (!IsCardInserted(card)) {
        AckPendingPackets(card);
        }
    // 
    // Send link status (disconnect) up the stack if we've been dis-associated 
    // long enough. 
    // 
    if (!card->IsAssociated                 && 
        NDIS_STATUS_MEDIA_CONNECT == card->m_MSenceStatus &&    //Make sure we only send it once.
        (card->m_MediaDisconnectDamper > 0) &&
        card->m_MediaDisconnectDamperTick   && 
        (0 == --card->m_MediaDisconnectDamperTick)) {
        card->m_MSenceStatus = NDIS_STATUS_MEDIA_DISCONNECT;
        NdisMIndicateStatus(card->m_MAHandle, card->m_MSenceStatus, 0, 0 );
        NdisMIndicateStatusComplete(card->m_MAHandle);
    }

    // 
    // Check association state.  Auto config needs to know the real association state.
    // 
    #ifdef DEBUG_AUTO_CONFIG
    if (card->m_autoConfigEnabled   &&
        (card->m_numProfiles > 1)   &&
        !card->IsAssociated         && 
        (card->m_AutoConfigDamperTick)) {
        DbgPrint("card->m_AutoConfigDamperTick = %d\n", (int)card->m_AutoConfigDamperTick);
        }
    #endif
    // 
    // Try a new profile if:
    // 
    //      1) auto config is enabled
    //      2) we have more than one
    //      3) we're not currently associated
    //      4) we've waited sufficiently long for damper to expire.
    // 
    if (card->m_autoConfigEnabled       &&
        (card->m_numProfiles > 1)       &&
        !card->IsAssociated             &&
        card->m_IsMacEnabled            &&
        (0 == --card->m_AutoConfigDamperTick)) {

        // 
        // Delay switching if temporarily disabled or MAC is disabled.
        // 
        if (card->m_tempDisableAutoSwitch || !card->m_IsMacEnabled) {
            // 
            // Bump up count so we try again next time the timer fires.
            // 
            card->m_AutoConfigDamperTick++;
            }
        else {
            #ifdef DEBUG_AUTO_CONFIG
            DbgPrint("card->m_AutoConfigDamperTick = %s\n", NumStr(card->m_AutoConfigDamperTick, 10));
            #endif
            // 
            // Set auto config timer in case we don't associate using the new profile.
            // 
            setAutoConfigTimer(card);
            // 
            // Wrap back to first profile, if necessary.
            // 
            if (++card->m_activeProfileIndex >= card->m_numProfiles) {
                card->m_activeProfileIndex = 0;
                }
            // 
            // Point activeProfile to the new profile.
            // 
            card->m_activeProfile = &card->m_profiles[card->m_activeProfileIndex];
            // 
            // Stuff config into card.
            // 
            if (cmdAwaken(card, TRUE)) {
                cmdConfigSet(card, &card->m_activeProfile->zFlags.cfg);
                cmdSSIDSet(card, &card->m_activeProfile->zFlags.SSIDList);
                cmdAPsSet(card, &card->m_activeProfile->zFlags.APList);
                }

            #ifdef DEBUG_AUTO_CONFIG
            DbgPrint("CardTimerCallback: trying config: %s (%d)\n",
                     card->m_activeProfile->properties.name,
                     (int)card->m_activeProfileIndex);
            #endif
            }
        }
}


//===========================================================================
   BOOLEAN CheckCardFlashing  ( PCARD        card,
                                NDIS_STATUS  *status,
                                NDIS_OID     Oid )
//===========================================================================
// 
// Description: Checks for which Oids safe to use while flashing
//    
//      Inputs: 
//    
//     Returns: FALSE if safe to use otherwise TRUE
//  
//     
//             spb010  Specifically added all the oids we support
//---------------------------------------------------------------------------
{

    if (FALSE == card->m_IsFlashing) {
        return FALSE;
    }

    switch (Oid) {
    //These OIDS are from oidx500.cpp and are ok to send
    //while flashing
        case OID_X500_GET_COREDUMP: 
        case OID_X500_ISAWAKE:
        case OID_X500_AWAKEN:
        case OID_X500_SLEEP:
        case OID_X500_KEEPAWAKE:
        case OID_X500_ISMAXPOWERSAVEON:
        case OID_GETVERSION:
        case OID_X500_GET_BUSTYPE:
        case OID_X500_GET_SYSTEM_ID:
        case OID_X500_GET_ACTIVE_PROFILE:
        case OID_X500_SELECT_PROFILE:
        case OID_X500_GET_PROFILE:
        case OID_X500_ENABLE_AUTO_PROFILE_SWITCH:
        case OID_X500_DISABLE_AUTO_PROFILE_SWITCH:
        case OID_X500_GET_SIGNAL_STRENGTH:
        case OID_X500_GET_SIGNAL_PARAMS:
        case OID_X500_GOTO_SLOW_POLL:
        case OID_X500_FLASH_ADAPTER:
        case OID_X500_FLASH_PROGRESS:
        case OID_X500_FLASH_OPEN:
        case OID_X500_FLASH_WRITE:
        case OID_X500_FLASH_CLOSE:
        case OID_X500_QUERY_RADIO_TYPE:
        case OID_X500_QUERY_ADAPTER_TYPE:
        case OID_X500_GET_CAPABILITY:
        case OID_X500_ISINSERTED:
        case OID_QUERY_RADIO_STATE:     //spb026
            return FALSE;    
        break;

    //These OIDS are from oidx500.cpp and are NOT ok to send
    //while flashing
        case OID_X500_HARD_RESET:
        case OID_X500_SOFT_RESET:
        case OID_RADIO_ON:
        case OID_RADIO_OFF:
        case OID_GET_REGISTERED_ROUTER:
        case OID_X500_GET_REGISTERED_ROUTER:
        case OID_GET_REGISTRATION_STATUS:
        case OID_X500_GET_REGISTRATION_STATUS:
        case OID_X500_SET_ACTIVE_PROFILE:
        case OID_X500_SET_PROFILE:
        case OID_GET_CONFIGUATION:
        case OID_X500_GET_CONFIGURATION:
        case OID_X500_EE_GET_CONFIGURATION:
        case OID_SET_CONFIGUATION:
        case OID_X500_SET_CONFIGURATION:
        case OID_X500_EE_SET_CONFIGURATION:
        case OID_X500_GET_APS:
        case OID_X500_EE_GET_APS:
        case OID_X500_SET_APS:
        case OID_X500_EE_SET_APS:
        case OID_X500_GET_SSIDS:
        case OID_X500_EE_GET_SSIDS:
        case OID_X500_SET_SSIDS:
        case OID_X500_EE_SET_SSIDS:
        case OID_GET_STATUS:
        case OID_X500_GET_STATUS:
        case OID_GET_STATISTICS:
        case OID_X500_GET_STATISTICS:
        case OID_RESET_STATISTICS:
        case OID_X500_RESET_STATS:
        case OID_X500_GET_SIGNAL_QUALITY:
        case OID_X500_GET_CAPS:
        case OID_X500_GET_32STATISTICS:
        case OID_X500_RESET_32STATISTICS:
        case OID_X500_READ_RID:
        case OID_X500_WRITE_RID:
        case OID_X500_WRITE_RID_IGNORE_MAC:
        case OID_X500_READBUF:
        case OID_X500_WRITEBUF:
        case OID_X500_READAUXBUF:
        case OID_X500_WRITEAUXBUF:
            //spb025 Changed from NDIS_STATUS_NOT_ACCPTED do to WZC
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID (%x) NOT accepted returning NDIS_STATUS_RESOURCES.\n",Oid);
            #endif
        break;

        //These Oids are ok to use while flashing
        case OID_GEN_MAC_OPTIONS:
        case OID_GEN_SUPPORTED_LIST:
        case OID_GEN_MEDIA_SUPPORTED:
        case OID_GEN_MEDIA_IN_USE:
        case OID_GEN_MAXIMUM_LOOKAHEAD:
        case OID_GEN_MAXIMUM_FRAME_SIZE:
        case OID_GEN_MAXIMUM_TOTAL_SIZE:
        case OID_GEN_TRANSMIT_BUFFER_SPACE:
        case OID_GEN_RECEIVE_BUFFER_SPACE:
        case OID_GEN_TRANSMIT_BLOCK_SIZE:
        case OID_GEN_RECEIVE_BLOCK_SIZE:
        case OID_GEN_VENDOR_ID:
        case OID_GEN_VENDOR_DESCRIPTION:
        case OID_GEN_DRIVER_VERSION:
        case OID_GEN_CURRENT_LOOKAHEAD:
        case OID_802_3_PERMANENT_ADDRESS:
        case OID_802_3_CURRENT_ADDRESS:
        case OID_802_3_MAXIMUM_LIST_SIZE:
        case OID_GEN_VENDOR_DRIVER_VERSION:
        case OID_GEN_MAXIMUM_SEND_PACKETS:
        case OID_GEN_MEDIA_CONNECT_STATUS:
    #if NDISVER == 5
        case OID_PNP_CAPABILITIES:
        case OID_PNP_QUERY_POWER:
        case OID_GEN_PHYSICAL_MEDIUM:
        case OID_TCP_TASK_OFFLOAD:
    #endif
        case OID_802_3_MULTICAST_LIST:
        case OID_GEN_CURRENT_PACKET_FILTER:
            #if DBG
            DbgPrint("Allowing %x while flashing\n",Oid);
            #endif
            return FALSE;
        break;

        //Everything else is NOT ok to use
        //while flashing.
        case OID_GEN_HARDWARE_STATUS:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_GEN_HARDWARE_STATUS not allowed.\n");
            #endif
        break;

        case OID_GEN_LINK_SPEED:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_GEN_LINK_SPEED not allowed.\n");
            #endif
        break;

        case OID_GEN_XMIT_OK:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_GEN_XMIT_OK not allowed.\n");
            #endif
        break;

        case OID_GEN_RCV_OK:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_GEN_RCV_OK not allowed.\n");
            #endif
        break;

        case OID_GEN_XMIT_ERROR:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_GEN_XMIT_ERROR not allowed.\n");
            #endif
        break;

        case OID_GEN_RCV_ERROR: 
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_GEN_RCV_ERROR not allowed.\n");
            #endif
        break;

        case OID_GEN_RCV_NO_BUFFER:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_GET_RCV_NO_BUFFER not allowed.\n");
            #endif
        break;

        case OID_802_3_RCV_ERROR_ALIGNMENT:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_802_3_RCV_ERROR_ALIGNMENT not allowed.\n");
            #endif
        break;

        case OID_802_3_XMIT_ONE_COLLISION:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_802_3_XMIT_ONE_COLLISION not allowed.\n");
            #endif
        break;

        case OID_802_3_XMIT_MORE_COLLISIONS:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_802_3_XMIT_MORE_COLLISIONS not allowed.\n");
            #endif
        break;

        case OID_802_11_SSID:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_802_11_SSID not allowed.\n");
            #endif
        break;

        case OID_802_11_INFRASTRUCTURE_MODE:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_802_11_INFRASTRUCTURE_MODE not allowed.\n");
            #endif
        break;

        case OID_802_11_AUTHENTICATION_MODE:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_802_11_AUTHENTICATION_MODE not allowed.\n");
            #endif
        break;

        case OID_802_11_BSSID:
            *status=NDIS_STATUS_ADAPTER_NOT_READY;
            #if DBG
            DbgPrint("Flashing... OID_802_11_BSSID not allowed.\n");
            #endif
        break;

        case OID_802_11_NETWORK_TYPE_IN_USE:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_802_11_NETWOK_TYPE_IN_USE not allowed.\n");
            #endif
        break;

        case OID_802_11_RATES_SUPPORTED:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_802_11_RATES_SUPPORTED not allowed.\n");
            #endif
        break;

        case OID_802_11_CONFIGURATION:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_802_11_CONFIGURATION not allowed.\n");
            #endif
        break;

        case OID_802_11_BSSID_LIST:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_802_11_BSSID_LIST not allowed.\n");
            #endif
        break;

        case OID_802_11_RSSI:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_802_11_RSSI not allowed.\n");
            #endif
        break;

        case OID_802_11_WEP_STATUS:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_802_11_WEP_STATUS not allowed.\n");
            #endif
        break;

        case OID_802_11_ADD_WEP:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_802_11_ADD_WEP not allowed.\n");
            #endif
        break;

        case OID_802_11_REMOVE_WEP:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_802_11_REMOVE_WEP not allowed.\n");
            #endif
        break;

        case OID_802_11_DISASSOCIATE:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_802_11_DISASSOCIATE not allowed.\n");
            #endif
        break;

        case OID_802_11_BSSID_LIST_SCAN:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_802_11_BSSID_LIST_SCAN not allowed.\n");
            #endif
        break;

        case OID_802_11_RELOAD_DEFAULTS:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_802_11_RELOAD_DEFAULTS not allowed.\n");
            #endif
        break;

    #if NDISVER == 5
        case OID_PNP_SET_POWER:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_PNP_SET_POWER not allowed.\n");
            #endif
        break;

        case OID_PNP_ENABLE_WAKE_UP:
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID_PNP_ENABLE_WAKE_UP not allowed.\n");
            #endif
        break;
    #endif

        default:
            //spb025  changed from NDIS_STATUS_NOT_ACCPTED do to WZC
            *status=NDIS_STATUS_RESOURCES;
            #if DBG
            DbgPrint("Flashing... OID (%x) NOT allowed returning NDIS_STATUS_RESOURCES.\n",Oid);
            #endif
        break;
    }

    return TRUE;
}

