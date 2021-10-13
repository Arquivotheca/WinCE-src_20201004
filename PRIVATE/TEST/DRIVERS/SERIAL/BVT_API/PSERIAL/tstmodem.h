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
 
Copyright (c) 1996  Microsoft Corporation
 
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

BOOL TstModemReadPacket( HANDLE hFile, LPVOID lpBuffer,
						DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead );
HANDLE  TstModemPing( LPCOMMTHREADCTRL lpCtrl );
BOOL    TstModemSendBuffer( HANDLE hCommPort, LPCVOID lpBuffer, 
                          const DWORD nSendBytes, LPDWORD lpnSent );
BOOL    TstModemReceiveBuffer( HANDLE hCommPort, LPVOID lpBuffer, 
                             const DWORD nRecBytes, LPDWORD lpnRecieved );
BOOL    TstModemCancel( HANDLE hCommPort );
                             

#endif __TSTMODEM_H__

