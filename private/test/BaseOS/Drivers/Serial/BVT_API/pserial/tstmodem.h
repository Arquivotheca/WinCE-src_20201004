//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997  Microsoft Corporation.  All Rights Reserved.

Module Name:

     tstmodem.h  

Abstract:
Functions:
Notes:
--*/
/*++
 
 
Module Name:
 
    E:\wince\private\qaos\drivers\serial\pserial\TstModem.h
 
Abstract:
 
    This file is the header file for the TstModem transfer functions.
 
Author:
 
    Uknown (unknown)
 
Notes:
 
--*/
#pragma once

#include<windows.h>
#include"GSerial.h"


#define TSTMODEM_RETRY      15
#define TSTMODEM_DATASIZE  128


/* ------------------------------------------------------------------------
    Structures
------------------------------------------------------------------------ */
// Receive Packet
typedef struct _TSTMODEM_RPACKET 
{
    BYTE        nPacket;
    BYTE        nCompPacket;
    BYTE        abData[TSTMODEM_DATASIZE];
    BYTE        nChecksum;
} TSTMODEM_RPACKET, *LPTSTMODEM_RPACKET;

// Send Packet
typedef struct _TSTMODEM_SPACKET 
{
    BYTE        bHeader;
    BYTE        nPacket;
    BYTE        nCompPacket;
    BYTE        abData[TSTMODEM_DATASIZE];
    BYTE        nChecksum;
} TSTMODEM_SPACKET, *LPTSTMODEM_SPACKET;



BOOL TstModemReadPacket( CommPort * hFile, LPVOID lpBuffer,
                        DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead );
HANDLE  TstModemPing( LPCOMMTHREADCTRL lpCtrl );
BOOL    TstModemSendBuffer( CommPort * hCommPort, LPCVOID lpBuffer, 
                          const DWORD nSendBytes, LPDWORD lpnSent );
BOOL    TstModemReceiveBuffer( CommPort * hCommPort, LPVOID lpBuffer, 
                             const DWORD nRecBytes, LPDWORD lpnReceived );
BOOL    TstModemCancel( CommPort * hCommPort );
                             






