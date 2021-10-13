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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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

