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
// keywords.h
//---------------------------------------------------------------------------
// Description:
//
// Revision History:
//
// Date        
//---------------------------------------------------------------------------
// 10/25/00    jbeaujon       -auto config stuff.
//
// 07/23/0     spb026         -Added way to turn off FirmwareChecking just
//                             incase.
//
// 
//---------------------------------------------------------------------------

#ifndef __KEYWORDS_H
#define __KEYWORDS_H


/****************************************************************************\
|* Copyright (c) 1996  Aironet                                      *|
|*                                                                          *|
\****************************************************************************/

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Module Name:

    keywords.h



    Ned Nassar

Environment:

    This driver is expected to work in Win95 and NT at the kernal mode.
    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Notes:

    optional-notes

Revision History:

--*/

//---------------------------------------------------------------------------
// ANSI equivalents to unicode __zFlagsN strings below.
//---------------------------------------------------------------------------
#define NUM_ZFLAG_KEYS                  6
#define NUM_REQUIRED_ZFLAG_KEYS         5

#define __zFlagsRegKeyBaseName          "zFlags"

#define __Environment                   NDIS_STRING_CONST("Environment");
#define __BusType                       NDIS_STRING_CONST("BusType");
#define __FormFactor                    NDIS_STRING_CONST("FormFactor");
#define __SupportedRates                NDIS_STRING_CONST("SupportedRates");
#define __InfrastructureMode            NDIS_STRING_CONST("InfrastructureMode");
#define __MagicPacketMode               NDIS_STRING_CONST("MagicPacketMode");
#define __NodeName                      NDIS_STRING_CONST("NodeName");
#define __PowerSaveMode                 NDIS_STRING_CONST("PowerSaveMode");
#define __SSID1                         NDIS_STRING_CONST("SSID1");
#define __TransmitPower                 NDIS_STRING_CONST("TransmitPower");
#define __SlotNumber                    NDIS_STRING_CONST("SlotNumber");
#define __PCCARDAttributeMemoryAddress  NDIS_STRING_CONST("PCCARDAttributeMemoryAddress");
#define __IoBaseAddress                 NDIS_STRING_CONST("IoBaseAddress");
#define __InterruptNumber               NDIS_STRING_CONST("InterruptNumber");
#define __MediaDisconnectDamper         NDIS_STRING_CONST("MediaDisconnectDamper");
// 
// Auto config dampers for active and passive scan modes.
// 
#define __AutoConfig                    NDIS_STRING_CONST("AutoConfig");
#define __AutoConfigActiveDamper        NDIS_STRING_CONST("AutoCfgActiveScanDamper");
#define __AutoConfigPassiveDamper       NDIS_STRING_CONST("AutoCfgPassiveScanDamper");

//spb026
#define __NoFirmwareCheck               NDIS_STRING_CONST("NoFirmwareCheck");

#endif // __KEYWORDS_H
