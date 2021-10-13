//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
 

 
	Uknown (unknown)
 
Notes:
 
--*/
#ifndef __TSTMODEM_H__
#define __TSTMODEM_H__

#include<windows.h>
#include"GSerial.h"


#define TSTMODEM_RETRY      15
#define TSTMODEM_DATASIZE  128

BOOL TstModemReadPacket( CommPort * hFile, LPVOID lpBuffer,
						DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead );
HANDLE  TstModemPing( LPCOMMTHREADCTRL lpCtrl );
BOOL    TstModemSendBuffer( CommPort * hCommPort, LPCVOID lpBuffer, 
                          const DWORD nSendBytes, LPDWORD lpnSent );
BOOL    TstModemReceiveBuffer( CommPort * hCommPort, LPVOID lpBuffer, 
                             const DWORD nRecBytes, LPDWORD lpnReceived );
BOOL    TstModemCancel( CommPort * hCommPort );
                             

#endif __TSTMODEM_H__

