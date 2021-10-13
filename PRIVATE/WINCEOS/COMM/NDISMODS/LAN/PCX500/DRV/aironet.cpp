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
/********************************************************************************
//  arlan.cpp
Module Name:    XXX500.sys

    aironet.cpp


    Ned Nassar

Environment:

Notes:

    optional-notes

Revision History:

***********************************************************************************/
//#define NDIS41_MINIPORT

#pragma code_seg("LCODE")

#include "NDISVER.h"
/*
#if NDISVER == 5
#   define NDIS50_MINIPORT_DRIVER
#   define NDIS50_MINIPORT  1 
#   define NDIS5_SUPPORT    
#elif NDISVER == 4
#   define NDIS40_MINIPORT 
#   define NDIS4_SUPPORT
#elif defined   NDIS41_MINIPORT 
#   define NDIS4_SUPPORT
#endif
*/
extern "C"{
    #include    <ndis.h>
    #include    <string.h>
}

#include "card.h"
#include "cardx500.h"
#include "aironet.h"
#include "AiroDef.h"

#ifdef SOFTEX
#include "softex.h"
#endif

VOID cbReturnPacket(NDIS_HANDLE  MiniportAdapterContext,PNDIS_PACKET Packet);
VOID FreeReceiveQueues(IN PCARD card);

extern "C"
{
    NTSTATUS
    DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );
}

#define  __FILEID__     1       // Unique file ID for error logging

//
// On debug builds tell the compiler to keep the symbols for
// internal functions, otw throw them out.
//

#if defined NDIS5_SUPPORT 
#   define DriverStruct3    DriverStruct.Ndis40Chars.Ndis30Chars
#   define DriverStruct4    DriverStruct.Ndis40Chars
#elif defined   NDIS4_SUPPORT  
#   define DriverStruct3    DriverStruct
#   define DriverStruct4    DriverStruct.Ndis40Chars
#else
#   define DriverStruct3    DriverStruct
#endif


UINT DebugFlag;

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{
    NDIS_HANDLE     NdisWrapperHandle=NULL;     // Handle for referring to the wrapper about this driver.

#if DBG 
    #ifdef WINNTBUILD
        DbgPrint("?>>> Build Date:"__DATE__" Time:"__TIME__"\n");
    
        #if 0 //#if NDISVER < 5
            DbgBreakPoint();
        #endif
    #endif

    #ifdef WIN95BUILD
            __asm {
                int 3
            }//BREAKPOINT
    #endif // WIN95BUILD
#endif //DBG

	RETAILMSG (1, (TEXT("PCX500:: Initializing..\r\n")));

    // Initialize the wrapper.    
    NdisMInitializeWrapper( &NdisWrapperHandle,     
                            DriverObject,
                            RegistryPath,
                            NULL
                            );
    // Initialize the Miniport characteristics for the call to
    NDIS_MINIPORT_CHARACTERISTICS   DriverStruct;   // Characteristics table for this driver.ver.
    NdisZeroMemory(&DriverStruct, sizeof(DriverStruct));

    DriverStruct3.MajorNdisVersion          = NDIS_MAJOR_VERSION;
    DriverStruct3.MinorNdisVersion          = NDIS_MINOR_VERSION;
    DriverStruct3.CheckForHangHandler       = NULL;  
    DriverStruct3.DisableInterruptHandler   = NULL;
    DriverStruct3.EnableInterruptHandler    = NULL;
    DriverStruct3.HandleInterruptHandler    = (void (__stdcall *)(void *))cbHandleInterrupt;
    DriverStruct3.ISRHandler                = cbIsr;                
    DriverStruct3.TransferDataHandler       = cbTransferData;       
    DriverStruct3.ReconfigureHandler        = NULL;
    DriverStruct3.HaltHandler               = cbHalt;
    DriverStruct3.InitializeHandler         = cbInitialize;
    DriverStruct3.ResetHandler              = cbResetCard;
    DriverStruct3.QueryInformationHandler   = cbQueryInformation;
    DriverStruct3.SetInformationHandler     = cbSetInformation;
    DriverStruct3.SendHandler               = cbSend;

  // register with miniport wrapper

#if NDISVER == 5
    DriverStruct4.SendPacketsHandler        = cbSendPackets;
    DriverStruct4.ReturnPacketHandler       = cbReturnPacket;

    NDIS_STATUS 
        Status = NdisMRegisterMiniport( NdisWrapperHandle,
                    &DriverStruct,
                    sizeof(DriverStruct));

    if( STATUS_SUCCESS != Status ){
        DriverStruct3.MajorNdisVersion      = 4;
        Status = NdisMRegisterMiniport( NdisWrapperHandle,
                    &DriverStruct,
                    sizeof(DriverStruct4));
    }
    if( STATUS_SUCCESS != Status ){
        DriverStruct3.MajorNdisVersion      = 3;
        Status = NdisMRegisterMiniport( NdisWrapperHandle,
                    &DriverStruct,
                    sizeof(DriverStruct3));
    }

#else 
    NDIS_STATUS     Status = 
                    NdisMRegisterMiniport(  NdisWrapperHandle,
                    &DriverStruct,
                    sizeof(DriverStruct));
#endif

    if( NDIS_STATUS_BAD_CHARACTERISTICS == Status){
        SAY("NDIS_STATUS_BAD_CHARACTERISTICS\n");
    }
    else 
    if( NDIS_STATUS_BAD_VERSION == Status ){
        SAY("NDIS_STATUS_BAD_VERSION\n");
    }
    else
    if( NDIS_STATUS_RESOURCES  == Status ){
        SAY("NDIS_STATUS_RESOURCES\n");
    }
    else
    if( NDIS_STATUS_FAILURE  == Status ){
        PRINT2("NDIS_STATUS_FAILURE, Status=%x\n", Status);
    }
    else {
        SAY("Init Wrapper OK\n");
    }   

	RETAILMSG (1, (TEXT("PCX500:: Initialization done [%s]\r\n"),
		Status == NDIS_STATUS_SUCCESS ? TEXT("Successful") : TEXT("failed")));

    return Status == NDIS_STATUS_SUCCESS ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}


//===========================================================================
    VOID cbHalt (IN NDIS_HANDLE Context)
//===========================================================================
// 
// Description: MiniportHalt
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{
    #ifdef DEBUG_MINIPORT_HALT
    DbgPrint("cbHalt:\n");
    #endif 

    PCARD card = (PCARD)Context;

#ifdef UNDER_CE
    card->bHalting = TRUE;
#endif

    // 
    // Since this function may be pre-empted by MiniportISR, we must
    // disable and deregister interrupts before we do anything else.
    // 
    disableHostInterrupts(card);
    if (card->m_InterruptRegistered) {
        NdisMDeregisterInterrupt(&card->Interrupt);
        card->m_InterruptRegistered = FALSE;
        }

#if NDISVER == 5
    FreeReceiveQueues(card);
    NdisFreeSpinLock(&card->m_lock);
#endif

    cardDestruct(card);
    NdisMDeregisterAdapterShutdownHandler(card->m_MAHandle);
    NdisFreeMemory(card, sizeof(CARD), 0);
}


//===========================================================================
    VOID cbShutdown (IN NDIS_HANDLE Context)
//===========================================================================
// 
// Description: MiniportShutdown
//    
//      Inputs: 
//    
//     Returns: 
//---------------------------------------------------------------------------
{ 
    extern void CardShutdown(PCARD card);

    CardShutdown((PCARD)Context);
}

extern PCARD AdapterArray[];
BOOLEAN SetupReceiveQueues(PCARD card);

NDIS_STATUS
cbInitialize(
    OUT PNDIS_STATUS    OpenErrorStatus,
    OUT PUINT           SelectedMediumIndex,
    IN  PNDIS_MEDIUM    MediumArray,
    IN  UINT            MediumArraySize,
    IN  NDIS_HANDLE     MiniportAdapterHandle,
    IN  NDIS_HANDLE     ConfigurationHandle
    )
{
    PCARD   card = (PCARD)NdisMalloc(sizeof(CARD));

    if( NULL == card )
        return NDIS_STATUS_FAILURE;
    
    NdisZeroMemory(card, sizeof(CARD) );
    if (!cardConstruct(card)) {
        return NDIS_STATUS_FAILURE;
        }

#ifdef SOFTEX
    SFTX_Open(card);
#endif

#if NDISVER == 5
    NdisAllocateSpinLock(&card->m_lock);
    
    if( ! SetupReceiveQueues( card ) ){
        FreeReceiveQueues(card);
        NdisFreeSpinLock(&card->m_lock );
        NdisFreeMemory( card, sizeof(CARD), 0);
        return NDIS_STATUS_FAILURE;
    }
#endif

    BOOLEAN res = InitDriver(card, OpenErrorStatus,SelectedMediumIndex,MediumArray,
                        MediumArraySize, MiniportAdapterHandle, ConfigurationHandle );

    if( FALSE == res ){

        LogError(card, NDIS_ERROR_CODE_HARDWARE_FAILURE, 0x5555 ); 
#if NDISVER == 5
        FreeReceiveQueues(card);
        NdisFreeSpinLock(&card->m_lock );
#endif

#ifdef SOFTEX
        void SoftexClose(PCARD  card);
        SoftexClose(card);
#endif

        NdisFreeMemory( card, sizeof(CARD), 0);
        return NDIS_STATUS_FAILURE;
    }   
    
    card->m_InitDone    = TRUE;

    for( int i=0; i<CARDS_MAX && AdapterArray[i]; i++ );
    if( i<CARDS_MAX )
        AdapterArray[i] = card;

    NdisMRegisterAdapterShutdownHandler(MiniportAdapterHandle, card, cbShutdown);

    return NDIS_STATUS_SUCCESS;
}

