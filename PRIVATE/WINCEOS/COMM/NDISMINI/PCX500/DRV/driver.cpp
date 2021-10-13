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
//  driver.cpp
//---------------------------------------------------------------------------
// Description:
//
// Revision History:
//
// Date         
//---------------------------------------------------------------------------
// 06/12/01     spb010      -If we are using the fast init option, then 
//                           we don't want to init interrupts yet because firm
//                           ware will zero out the interrupt enable
//                           register during the reset.  This caused a race 
//                           condition where the card would be up and running
//                           but not interrupts were enabled.
//
// 07/18/01     spb023      -Added flashing of firmware from cbInit.  This is
//                           for Microsoft and Windows XP
//
// 07/23/01     spb026      -Changed the way we flash firmware so it blocks
//                           the init untill we are done. Broke WZC.  Note this
//                           function can only be used at IRQL < DISPATCH_LEVEL.
//
// 08/01/01     spb027      -"Fixed" flash rev check for Subversion.  It is not
//                            BCD encoded like the softwareversion is.
//---------------------------------------------------------------------------

#pragma code_seg("LCODE")
#include "NDISVER.h"
extern "C"{
    #include    <ndis.h>
}
#include "pci.h"
#include "aironet.h"
#include "CardX500.h"
#include "keywords.h"
#include "string.h"
#include "Support.h"
#include "AiroDef.h"
#include "alloc.h"
#include "profile.h"

PCARD       AdapterArray[CARDS_MAX+1];
WINDOWSOS   OSType      = OS_WIN95;     
UINT        NdisVersion;
UINT        ProcessorType;
extern UINT DebugFlag;


extern  BOOLEAN FindPciDeviceScan   (PCARD          card);

static  void    DeRegisterAdapter   (PCARD          card);

//===========================================================================
    BOOLEAN InitDriver (PCARD               card,
                        OUT PNDIS_STATUS    OpenErrorStatus,
                        OUT PUINT           SelectedMediumIndex,
                        IN PNDIS_MEDIUM     MediumArray,
                        IN UINT             MediumArraySize,
                        IN NDIS_HANDLE      MiniportAdapterHandle,
                        IN NDIS_HANDLE      ConfigurationHandle)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//    
//      (10/27/00)
//---------------------------------------------------------------------------
{
    card->m_MAHandle        = MiniportAdapterHandle;        
    card->m_CfgHandle       = ConfigurationHandle;

    for (*SelectedMediumIndex = 0; MediumArraySize && MediumArray[*SelectedMediumIndex]; *SelectedMediumIndex++);

    if (!GetDriverConfig(card)) {
        return FALSE;
        }
    
#if 1
    NdisMSetAttributes(card->m_MAHandle, (NDIS_HANDLE)card, FALSE, card->m_BusType);
#else
    UINT timeout = 60 * 60 * 1000 / 2;
    NdisMSetAttributesEx(card->m_MAHandle, 
                         (NDIS_HANDLE)card, 
                         timeout, 
                         0,  
                         card->m_BusType);
#endif

    if ((NdisInterfacePci == card->m_BusType) && (FALSE == FindPciDeviceScan(card))) {
        return FALSE;
    }
    
    
#ifdef SOFTEX   
    BOOLEAN SoftexInitCard(PCARD card);

    if (!card->IsSoftexClient) {
#endif
        if (!RegisterAdapter(card)) {
            return FALSE;
            }

        disableHostInterrupts(card);
        if (!InitHW(card)) {
            DeRegisterAdapter( card );
            return FALSE;
            }

#ifdef SOFTEX   
        }
    else if (!SoftexInitCard(card)) {
        DeRegisterAdapter( card );
        return FALSE;
        }
#endif

    if (!RegisterInterrupt(card)) {
        DeRegisterAdapter(card);
        return FALSE;
        }

    //spb010
    if (card->initComplete) {
        InitInterrupts( card );
    }

#ifndef UNDER_CE
    LoadVxd(card);
#endif

    NdisMInitializeTimer(&card->m_CardTimer, 
                         card->m_MAHandle, 
                         (void (*)(void *,void *,void *,void *))CardTimerCallback, 
                         card);

    NdisMSetTimer(&card->m_CardTimer, 1 * 1000);
    card->m_CardTimerIsSet = TRUE;

    return TRUE;                
}

//===========================================================================
    BOOLEAN GetDriverConfig (PCARD card)
//===========================================================================
// 
// Description: Read the driver's configuration from the registry.
//    
//      Inputs: card - pointer to card structure
//    
//     Returns: TRUE if successful, FALSE otherwise.
//---------------------------------------------------------------------------
{
    BOOLEAN retval = TRUE;

    NDIS_HANDLE MiniportAdapterHandle   = card->m_MAHandle;     
    NDIS_HANDLE ConfigurationHandle     = card->m_CfgHandle;            // The handle for reading from the registry.
    NDIS_HANDLE ConfigHandle;
    
#ifndef UNDER_CE
    PNDIS_CONFIGURATION_PARAMETER   ReturnedValue;// The value read from the registry.
#endif

    NDIS_STATUS                     Status;
    
#ifndef UNDER_CE
    UCHAR                           tmpUCHAR;
#endif

    unsigned                        Length;                 // The number of bytes in the address.It should be
    char                            *NetAddressPtr;

#ifndef UNDER_CE
    char                            tmpBuf[ 256 ];
#endif
    
   // Open the configuration space.
    NdisOpenConfiguration(&Status, &ConfigHandle, ConfigurationHandle);
    if (NDIS_STATUS_SUCCESS != Status) {
        retval = FALSE;
        }
    else {
        //
        UINT Env;
        NDIS_STRING Environment = __Environment;        
        GetConfigEntry(ConfigHandle, &Environment, &Env);
        OSType = NdisEnvironmentWindows == Env ? OS_WIN95 : OS_WINNT;
        //
    
        NdisReadNetworkAddress(&Status, (void **)&NetAddressPtr, &Length, ConfigHandle);
        if ((NDIS_STATUS_SUCCESS == Status) && ((unsigned)6 == Length)) {
            if ((0 != (ULONG *)NetAddressPtr) ||  (0 != (short *)(NetAddressPtr+sizeof(LONG)))) {
                card->m_MacIdOveride = TRUE;
                NdisMoveMemory(card->m_StationAddress, NetAddressPtr, 6 );
                }
            }
    
        NDIS_STRING     BusType = __BusType;
        GetConfigEntry( ConfigHandle, &BusType, (UINT *)&card->m_BusType );

        NDIS_STRING     FormFactor = __FormFactor;
        GetConfigEntry( ConfigHandle, &FormFactor, (UCHAR *)card->m_FormFactor, sizeof(card->m_FormFactor)-1 );

        NDIS_STRING     IoBaseAddress   = __IoBaseAddress;
        //GetConfigEntry( ConfigHandle, &IoBaseAddress, (UINT *)&card->m_IOBase );
        GetConfigEntry( ConfigHandle, &IoBaseAddress, (UINT *)&card->m_InitialPort );

        NDIS_STRING     InterruptNumber     = __InterruptNumber;
        GetConfigEntry( ConfigHandle, &InterruptNumber, &card->m_InterruptNumber );

#if (NDISVER < 5) || (NDISVER == 41)    //spbMgc
        NDIS_STRING     PCCARDAttributeMemoryAddress    = __PCCARDAttributeMemoryAddress;
        GetConfigEntry( ConfigHandle, &PCCARDAttributeMemoryAddress, (UINT *)&card->m_pAttribMemBase );
#endif

        NDIS_STRING SupportedRates  = __SupportedRates;
        GetConfigEntry( ConfigHandle, &SupportedRates, &card->m_SelectedRate );
    
        NDIS_STRING InfrastructureMode      = __InfrastructureMode;
        GetConfigEntry( ConfigHandle, &InfrastructureMode, &card->m_InfrastructureMode );

        NDIS_STRING MagicPacketMode     = __MagicPacketMode;
        GetConfigEntry( ConfigHandle,   &MagicPacketMode, &card->m_MagicPacketMode );
    
        NDIS_STRING     NodeName        = __NodeName;
        GetConfigEntry( ConfigHandle, &NodeName, card->m_AdapterName, 16 );
    
        NDIS_STRING     PowerSaveMode   = __PowerSaveMode;
        GetConfigEntry( ConfigHandle, &PowerSaveMode, &card->m_PowerSaveMode );

        NDIS_STRING     SSID1   = __SSID1;
        GetConfigEntry( ConfigHandle, &SSID1, (UINT *)card->m_SystemID);
        card->m_ESS_IDLen1 = (short)GetConfigEntry( ConfigHandle, &SSID1, card->m_ESS_ID1, 32 );
    
        NDIS_STRING     SlotNumber  = __SlotNumber;
        GetConfigEntry( ConfigHandle, &SlotNumber, &card->m_SlotNumber );
    
        NDIS_STRING     MediaDisconnectDamper   = __MediaDisconnectDamper;
        GetConfigEntry( ConfigHandle, &MediaDisconnectDamper, &card->m_MediaDisconnectDamper,NdisParameterInteger  );

        NDIS_STRING     AutoConfigActiveDamper   = __AutoConfigActiveDamper;
        GetConfigEntry(ConfigHandle, &AutoConfigActiveDamper, &card->m_AutoConfigActiveDamper, NdisParameterInteger);
        if (card->m_AutoConfigActiveDamper < DEFAULT_AUTO_CONFIG_ACTIVE_DAMPER) {
            card->m_AutoConfigActiveDamper = DEFAULT_AUTO_CONFIG_ACTIVE_DAMPER;
            }

        NDIS_STRING     AutoConfigPassiveDamper  = __AutoConfigPassiveDamper;
        GetConfigEntry(ConfigHandle, &AutoConfigPassiveDamper, &card->m_AutoConfigPassiveDamper, NdisParameterInteger);
        if (card->m_AutoConfigPassiveDamper < DEFAULT_AUTO_CONFIG_PASSIVE_DAMPER) {
            card->m_AutoConfigPassiveDamper = DEFAULT_AUTO_CONFIG_PASSIVE_DAMPER;
        
        }

#ifdef MICROSOFT
        //spb026    Just incase someone changes their mind.
        NDIS_STRING     NoFirmwareCheck = __NoFirmwareCheck;
        GetConfigEntry(ConfigHandle, &NoFirmwareCheck, &card->wasFlashChecked, NdisParameterInteger);
#endif

        // 
        // Read available profiles.                 
        // 
        if (getProfiles(ConfigHandle, &card->m_profiles, &card->m_numProfiles)) {
            // 
            // If no profiles, make a default.
            // 
            if (card->m_numProfiles == 0) {
                card->m_profiles = new STPROFILE;
                NdisZeroMemory(card->m_profiles, sizeof(STPROFILE));
                card->m_readCardConfig = TRUE;
                card->m_numProfiles = 1;
                // 
                // Set default name and set flags
                // 
                strcpy(card->m_profiles->properties.name, DEFAULT_ENTERPRISE_PROFILE_NAME);
                card->m_profiles->properties.isDefault      = TRUE;
                card->m_profiles->properties.autoSelectable = TRUE;
                card->m_profiles->isValid                   = TRUE;
                }

            card->m_activeProfileIndex  = 0;
            // 
            // point to active/current profiles.
            // 
            card->m_activeProfile  = &card->m_profiles[0];
            card->m_currentProfile = card->m_activeProfile;
            // 
            // Auto config is off by default (i.e. if the registry key does not exist)
            // 
/*  spb006
            UCHAR autocfg[2];
            NDIS_STRING AutoConfig = __AutoConfig;
            GetConfigEntry( ConfigHandle, &AutoConfig, autocfg, 2);
            card->m_autoConfigEnabled = (*autocfg == '1') ? TRUE : FALSE;
*/
            card->m_autoConfigEnabled =  FALSE;  //spb006

            #ifdef DEBUG_AUTO_CONFIG
            for (int i = 0; i < card->m_numProfiles; i++) {
                dumpProfile(&card->m_profiles[i], TRUE);
                }
            #endif

            }
        else {
            retval = FALSE;
            }

        NdisCloseConfiguration(ConfigHandle);
        }

    return retval;
}

//===========================================================================
    void DeRegisterAdapter (PCARD card)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    if (card->m_AttribMemRegistered) {
        NdisMUnmapIoSpace( card->m_MAHandle, (void *)card->m_pAttribMemBase, CIS_ATTRIBMEM_SIZE );
        }

    //if( card->m_RadioMemBase )
    //  NdisMUnmapIoSpace( card->m_MAHandle, (void *)card->m_RadioMemBase, CIS_ATTRIBMEM_SIZE );
    
    if (card->m_ContolRegRegistered) {
        NdisMUnmapIoSpace( card->m_MAHandle, (void *)card->m_ContolRegMemBase, CTL_ATTRIBMEM_SIZE );
        }
    
    if (card->m_IoRegistered) {
        //NdisMUnmapIoSpace( card->m_MAHandle, (void *)card->m_IOBase, card->m_PortIOLen );
        NdisMDeregisterIoPortRange(card->m_MAHandle, card->m_InitialPort, card->m_PortIOLen, (void *)card->m_IOBase);
        if (card->m_PortIOLen8Bit) {
            //NdisMUnmapIoSpace( card->m_MAHandle, (void *)card->m_IOBase8Bit, card->m_PortIOLen8Bit );
            NdisMDeregisterIoPortRange(card->m_MAHandle, 
                                       card->m_InitialPort8Bit, 
                                       card->m_PortIOLen8Bit, 
                                       (void *)card->m_IOBase8Bit);
            }
        }
}

//===========================================================================
    BOOLEAN RegisterAdapter (PCARD card)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    NDIS_STATUS             Status;
    NDIS_PHYSICAL_ADDRESS   AttributePhysicalAddress;
//  UINT                    InitialPort = card->m_IOBase;

    Status = NdisMRegisterIoPortRange((void **)&card->m_IOBase, 
                                      card->m_MAHandle, 
                                      card->m_InitialPort,  
                                      card->m_PortIOLen);

    if (NDIS_STATUS_SUCCESS != Status) {
        LogError(card, NDIS_ERROR_CODE_BAD_IO_BASE_ADDRESS, BAD_IO_BASE_ADDRESS );
        return FALSE;
        }

    if (card->m_PortIOLen8Bit) {
        //InitialPort   = card->m_IOBase8Bit;
        Status = NdisMRegisterIoPortRange((void **)&card->m_IOBase8Bit, 
                                          card->m_MAHandle, 
                                          card->m_InitialPort8Bit,
                                          card->m_PortIOLen8Bit);

        if (NDIS_STATUS_SUCCESS !=  Status) {
            NdisMUnmapIoSpace(card->m_MAHandle, (void *)card->m_IOBase, card->m_PortIOLen);
            LogError(card, NDIS_ERROR_CODE_BAD_IO_BASE_ADDRESS, BAD_IO_BASE_ADDRESS);
            return FALSE;
            }
        }
    card->m_IoRegistered = TRUE;
    
//spbMgc
#if (NDISVER < 5) || (NDISVER == 41)
    if (card->m_pAttribMemBase) {
        NdisSetPhysicalAddressHigh(AttributePhysicalAddress, 0);
        NdisSetPhysicalAddressLow(AttributePhysicalAddress, (ULONG)card->m_pAttribMemBase);
        Status = NdisMMapIoSpace((PVOID *)&card->m_pAttribMemBase,card->m_MAHandle, AttributePhysicalAddress,CIS_ATTRIBMEM_SIZE);

        if (NDIS_STATUS_SUCCESS !=  Status) {
            LogError(card, NDIS_ERROR_CODE_BAD_IO_BASE_ADDRESS, BAD_ATTRIBMEM_BASE_ADDRESS);
            card->m_AttribMemRegistered = FALSE;
            }
        else {
            card->m_AttribMemRegistered = TRUE;
            }
        }
#endif

    //------------------------------------------
    if (card->m_ContolRegMemBase) {
        NdisSetPhysicalAddressHigh(AttributePhysicalAddress, 0);
        NdisSetPhysicalAddressLow(AttributePhysicalAddress, (ULONG)card->m_ContolRegMemBase);
        Status = NdisMMapIoSpace((PVOID *)&card->m_ContolRegMemBase, 
                                 card->m_MAHandle, 
                                 AttributePhysicalAddress,
                                 CTL_ATTRIBMEM_SIZE);

        if (NDIS_STATUS_SUCCESS != Status) {
            LogError(card, NDIS_ERROR_CODE_BAD_IO_BASE_ADDRESS, BAD_ATTRIBMEM_BASE_ADDRESS );
            }
        else {
            card->m_ContolRegRegistered = TRUE;
            }
        }
    //}
    return TRUE;
}

BOOLEAN         
RegisterInterrupt(
    PCARD   card
    )
{
    NDIS_STATUS Status = NdisMRegisterInterrupt( &card->Interrupt, card->m_MAHandle, 
                            card->m_InterruptNumber, card->m_InterruptNumber, 
                            TRUE, TRUE, NdisInterruptLevelSensitive );

    if ( Status != NDIS_STATUS_SUCCESS) 
        LogError(card, NDIS_ERROR_CODE_INTERRUPT_CONNECT, INTERRUPT_CONNECT);

    return card->m_InterruptRegistered = (NDIS_STATUS_SUCCESS == Status);   
}   


#ifndef UNDER_CE
extern "C" {
#define WANTVXDWRAPS
#include <basedef.h>        
#include <VMM.H>
#include <vxdldr.h>
#include <VXDWRAPS.H>
}

#include <AiroVxdWrap.H>

BOOLEAN         
LoadVxd(
    PCARD   card
    )
{
    #define VXDLDR_INIT_DEVICE  0x000000001 
    
    if ( OS_WIN95!=OSType || NULL == VxdLoadName)
        return FALSE;
    
    ULONG   flags           = VXDLDR_INIT_DEVICE;
    struct  VxD_Desc_Block  *ddb    = NULL;
    struct  DeviceInfo      *handle = NULL;
    ULONG                   ret;

    if( 0 == (ret = VXDLDR_LoadDevice(  &ddb, &handle, VxdLoadName, flags )) && handle ){
         card->m_VxdLoaded = TRUE;
        STDRIVER_ENTRY ld   ={VXD_LOAD,0,(UINT)cbQueryInformation};
        ld.card             = (UINT)card;
        ld.CardType         = (VXD_CARDTYPE)card->m_CardType;

        MiniPortEntry(&ld);

    }
    else
        LogError(card, NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION, VXD_NOTLOADED );

    return 0==ret;
}

void            
UnloadVxd(
    PCARD   card
    )
{
    if( OS_WIN95!=OSType || FALSE == card->m_VxdLoaded || NULL == VxdUnloadName)
        return;

    STDRIVER_ENTRY ld ={VXD_UNLOAD,0,0};
    ld.card = (UINT)card;
    MiniPortEntry(&ld);
    VXDLDR_UnloadDevice(  (unsigned short)0xffff, VxdUnloadName);
}
#endif


//spb023
#ifdef MICROSOFT
BOOLEAN CheckRev(PCARD card)
{
    extern unsigned char imageData[];

    STCAPS  caps;

    if ( ! cmdCapsGet(card, &caps ) ) {
        return FALSE;
    }

    #if DBG
        DbgPrint("Caps Software is %x\n",caps.u16SoftwareVersion);
        DbgPrint("Caps SubSoftware is %x\n",caps.u16SoftwareSubVersion);
    #endif
  
    unsigned int myRev, thisRev, x1, x2, y1, y2;

    myRev=imageData[0x178];                 // hard coded in image file
    myRev = (myRev<<8) + imageData[0x179];
    myRev = (myRev<<16) + imageData[0x17a];

    x1= ((ULONG)caps.u16SoftwareVersion) >> 12;
    x2= (((ULONG)caps.u16SoftwareVersion) >> 8) % 16;
    y1= (((ULONG)caps.u16SoftwareVersion) >> 4) % 16;
    y2= ((ULONG)caps.u16SoftwareVersion) % 16;

    if ((x1 > 9) || (y1 > 9) || (x2 > 9) || (y2 > 9)) {
        #if DBG
            DbgPrint("Cant determine the version %x,%x,%x,%x\n",x1,y1,x2,y2);
        #endif

        return TRUE;  // can't determine what it is so flash it
    }

    thisRev = (((x1*10+x2) << 8) + (y1*10+y2)) << 16;

    x1= ((ULONG)caps.u16SoftwareSubVersion) >> 12;
    x2= (((ULONG)caps.u16SoftwareSubVersion) >> 8) % 16;
    y1= (((ULONG)caps.u16SoftwareSubVersion) >> 4) % 16;
    y2= ((ULONG)caps.u16SoftwareSubVersion) % 16;

    //spb027
    if ((x1 > 16) || (y1 > 16) || (x2 > 16) || (y2 > 16)) {
        #if DBG
            DbgPrint("Cant determine the subversion %x,%x,%x,%x\n",x1,y1,x2,y2);
        #endif

        return TRUE;  // can't determine what it is so flash it
    }

//spb027    thisRev += (x1*1000+x2*100+y1*10+y2);
    thisRev += (x1*64+x2*32+y1*16+y2);

    #if DBG
        DbgPrint("Driver Rev is %x.\n",myRev);
        DbgPrint("MAC Rev is %x.\n",thisRev);
    #endif

    if (myRev > thisRev)
        return TRUE;
    else
        return FALSE;
}

#include "flash.h"

//spb026
//Note THIS FUNCTION CAN ONLY BE CALLED AT IRQL < DISPATCH_LEVEL
void FlashImage(PCARD card)
{
    extern unsigned char imageData[];

    char *pBuffer = (char *)&imageData[16+256];
    DbgPrint("Starting to Flash\n");

    card->m_IsFlashing  = TRUE;

    //Init Flash
    FlashInit(card);
    FLASH_STRUCT        *p = &card->FlashSt;    

    //Copy flash image
    for( int i =0; i<128; i++ ) {
        FlashCopyNextBlock(card, pBuffer + i * 1024 );
    }

    //Ok Simulate the timer tick.
    do {
        if (p->m_delay) {
            //Simulate a timer tick
            #if DBG
            DbgPrint("Sleeping for %d\n",p->m_delay*1000);
            #endif
            NdisMSleep(p->m_delay*1000);
        }

        FlashStartFlashing(card,FALSE);
    } while (FLASHCMD_INPROGRESS == p->m_Progress);
}


#endif



