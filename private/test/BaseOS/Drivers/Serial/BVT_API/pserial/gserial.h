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

     gserial.h  

Abstract:
Functions:
Notes:
--*/
/*++
 
 
Module Name:
 
	E:\wince\private\qaos\drivers\serial\pserial\GSerial.h
 
Abstract:

    General Serial:  This file contains general serial communications stuff.
 
Author:
 
	Uknown (unknown)
  
Notes:
 
--*/
#pragma once
#include <windows.h>
#include <comm.h>
#define ReadFile(x,y,a,b,c) x->ReadFile(y,a,b,c)
#define WriteFile(x,y,a,b,c) x->WriteFile(y,a,b,c)
#define ClearCommBreak(x) x->ClearCommBreak()
#define ClearCommError(x,y,z) x->ClearCommError(y,z)
#define EscapeCommFunction(x,y) x->EscapeCommFunction(y)
#define GetCommMask(x,y) x->GetCommMask(y)
#define GetCommModemStatus(x,y) x->GetCommModemStatus(y)
#define GetCommProperties(x,y) x->GetCommProperties(y)
#define GetCommState(x,y) x->GetCommState(y)
#define GetCommTimeouts(x,y) x->GetCommTimeouts(y)
#define PurgeComm(x,y) x->PurgeComm(y)
#define SetCommBreak(x) x->SetCommBreak()
#define SetCommMask(x,y) x->SetCommMask(y)
#define SetCommState(x,y) x->SetCommState(y)
#define SetCommTimeouts(x,y) x->SetCommTimeouts(y)
#define SetupComm(x,y,z) x->SetupComm(y,z)
#define TransmitCommChar(x,y) x->TransmitCommChar(y)
#define WaitCommEvent(x,y,z) x->WaitCommEvent(y,z)

/* ------------------------------------------------------------------------
    Structures	
------------------------------------------------------------------------ */
typedef struct _COMMTHREADCTRL
{
    CommPort *  hPort;
    HANDLE      hEndEvent;
    DWORD       dwData;

} COMMTHREADCTRL, *LPCOMMTHREADCTRL;

typedef struct _BAUDTABLE {
    DWORD   dwFlag;
    DWORD   dwBaud;
    LPTSTR  ptszString;
	BOOL    bValid;  //If the Baud Rate is Valid 
} BAUDTABLE, *LPBAUDTABLE;

typedef struct _PARITYSTOPBITSTABLE {
    DWORD dwFlag;
    BYTE bDCBFlag;
    LPTSTR ptszString;
} PARITYSTOPBITSTABLE, *LPPARITYSTOPBITSTABLE;

typedef struct _DATABITSTABLE {
    DWORD dwFlag;
    BYTE  bDCBFlag;
    SHORT siBits;
} DATABITSTABLE, *LPDATABITSTABLE;

typedef struct _COMMERROR {
    DWORD   dwErrNum;
    LPTSTR  ptszString;
} COMMERROR, *LPCOMMERROR;


static BAUDTABLE g_BaudTable[] = {
	{ BAUD_075,     75,         TEXT("75")	   ,FALSE },
    { BAUD_110,     CBR_110,    TEXT("110")    ,FALSE },
    { BAUD_134_5,   134,        TEXT("134.5")  ,FALSE },
	{ BAUD_150,     150,        TEXT("150")    ,FALSE },
	{ BAUD_300,     CBR_300,    TEXT("300")    ,FALSE },
	{ BAUD_600,     CBR_600,    TEXT("600")    ,FALSE },
    { BAUD_1200,    CBR_1200,   TEXT("1200")   ,FALSE },
    { BAUD_1800,    1800,       TEXT("1800")   ,FALSE },
    { BAUD_2400,    CBR_2400,   TEXT("2400")   ,FALSE },
    { BAUD_4800,    CBR_4800,   TEXT("4800")   ,FALSE },
    { BAUD_7200,    7200,       TEXT("7200")   ,FALSE },
    { BAUD_9600,    CBR_9600,   TEXT("9600")   ,FALSE },
    { BAUD_14400,   CBR_14400,  TEXT("14400")  ,FALSE },
	{ BAUD_19200,   CBR_19200,  TEXT("19200")  ,FALSE },
    { BAUD_38400,   CBR_38400,  TEXT("38400")  ,FALSE },
    { BAUD_56K,     CBR_56000,  TEXT("56000")  ,FALSE },
    { BAUD_57600,   CBR_57600,  TEXT("57600")  ,FALSE },
    { BAUD_115200,  CBR_115200, TEXT("115200") ,FALSE },
    { BAUD_128K,    CBR_128000, TEXT("128000") ,FALSE }
};


#define NUMBAUDS (sizeof( g_BaudTable ) / sizeof( BAUDTABLE ))

static PARITYSTOPBITSTABLE g_ParityStopBitsTable[] = {
    { PARITY_NONE,  NOPARITY,    TEXT("NO")         },
    { PARITY_ODD,   ODDPARITY,   TEXT("ODD")        },
    { PARITY_EVEN,  EVENPARITY,  TEXT("EVEN")       },
    { PARITY_MARK,  MARKPARITY,  TEXT("MARK")       },
    { PARITY_SPACE, SPACEPARITY, TEXT("SPACE")      },
    { STOPBITS_10,  0,        	 TEXT("1 stop bit") },
    { STOPBITS_15,	1,           TEXT("1.5 stop bits")},
    { STOPBITS_20,	2,           TEXT("2 stop bits")}

};
#define NUMPARITIESANDSTOPS (sizeof(g_ParityStopBitsTable) / sizeof( PARITYSTOPBITSTABLE ))
#define PARITYOFFSET    0
#define STOPBITSOFFSET  5

static DATABITSTABLE g_DataBitsTable[] = 
{
    { DATABITS_8, 8, 8 },
    { DATABITS_7, 7, 7 },
    { DATABITS_6, 6, 6 },
    { DATABITS_5, 5, 5 }
};

#define NUMDATABITS (sizeof( g_DataBitsTable ) / sizeof(DATABITSTABLE))

#define CAS(x) { x,TEXT(#x) }
static COMMERROR CommErrs[] = {
    CAS( CE_BREAK ),
    CAS( CE_DNS ),
    CAS( CE_FRAME ),
    CAS( CE_IOE ),
    CAS( CE_MODE ),
    CAS( CE_OOP ),
    CAS( CE_OVERRUN ),
    CAS( CE_PTO ),
    CAS( CE_RXOVER ),
    CAS( CE_RXPARITY ),
    CAS( CE_TXFULL )
};
#define NUMCOMMERRORS   (sizeof( CommErrs ) / sizeof( COMMERROR ))



/* ------------------------------------------------------------------------
	Constants
------------------------------------------------------------------------ */
#define NUL 0x00
#define SOH 0x01    // Start of Header
#define STX 0x02    // Start of Text
#define ETX 0x03    // End of Text
#define EOT 0x04
#define ENQ 0x05
#define ACK 0x06
#define BEL 0x07
#define NCK 0x15
#define SYN 0x16
#define ETB 0x17
#define CAN 0x18
#define EM  0x19
#define SUB 0x1A
#define ESC 0x1B





