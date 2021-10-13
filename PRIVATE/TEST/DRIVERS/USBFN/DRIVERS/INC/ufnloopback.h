//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
// 
// Module Name:  
//     ufnloopback.h
// 
// Abstract:  The header file for loopback driver exposed function definitions,
//               and related data structure, defines, and globals
//     
// Notes: 
//

#ifndef __TRANSPORT_H__
#define __TRANSPORT_H__

#include <windows.h>
#include "usbfntypes.h"


enum CONTROL_RESPONSE {
    CR_SUCCESS = 0,
    CR_SUCCESS_SEND_CONTROL_HANDSHAKE, // Use if no data stage
    CR_STALL_DEFAULT_PIPE,
    CR_UNHANDLED_REQUEST,
};


//this is the definition of the request structure (mainly for a protocol to commnicate between 
//test driver on host controller side and function drive side
#include <pshpack1.h>
typedef struct _USB_TDEVICE_REQUEST {
    UCHAR   bmRequestType;
    UCHAR   bRequest;
    USHORT  wValue;
    USHORT  wIndex;
    USHORT  wLength;
} USB_TDEVICE_REQUEST, * PUSB_TDEVICE_REQUEST, * LPUSB_TDEVICE_REQUEST;
#include <poppack.h>

//test request commands
#define TEST_REQUEST_BULKIN	1
#define TEST_REQUEST_BULKOUT	2
#define TEST_REQUEST_INTIN		3
#define TEST_REQUEST_INTOUT	4
#define TEST_REQUEST_ISOCHIN	5
#define TEST_REQUEST_ISOCHOUT	6
#define TEST_REQUEST_RESET		7
#define TEST_REQUEST_DATALPBK  8
#define TEST_REQUEST_LPBKINFO  9
#define TEST_REQUEST_PAIRNUM  10
#define TEST_REQUEST_SHORTSTRESS 11
#define TEST_REQUEST_EP0TESTIN    12
#define TEST_REQUEST_EP0TESTOUT  13

#define TEST_REQUEST_QUIT   30

#include <pshpack1.h>
typedef struct _USB_TDEVICE_RECONFIG {
    UCHAR   ucConfig;
    UCHAR   ucInterface;
    UCHAR   ucSpeed;
    USHORT wBulkPkSize;
    USHORT wIntrPkSize;
    USHORT wIsochPkSize;
    USHORT wControlPkSize;
} USB_TDEVICE_RECONFIG, * PUSB_TDEVICE_RECONFIG, * LPUSB_TDEVICE_RECONFIG;
#include <poppack.h>

#include <pshpack1.h>
typedef struct _USB_TDEVICE_DATALPBK {
    UCHAR   uOutEP;
    USHORT wBlockSize;
    USHORT wNumofBlocks;
    USHORT  usAlignment;
    DWORD   dwStartVal;
} USB_TDEVICE_DATALPBK, * PUSB_TDEVICE_DATALPBK, * LPUSB_TDEVICE_DATALPBK;
#include <poppack.h>
typedef struct _USB_TDEVICE_LPBKPAIR{
    UCHAR   ucOutEp;
    UCHAR   ucInEp;
    UCHAR   ucType;
} USB_TDEVICE_LPBKPAIR, * PUSB_TDEVICE_LPBKPAIR;

typedef struct _USB_TDEVICE_LPBKINFO{
    UCHAR   uNumofPairs;
    UCHAR   uNumofSinglePipes;
    USB_TDEVICE_LPBKPAIR   pLpbkPairs[1];
} USB_TDEVICE_LPBKINFO, * PUSB_TDEVICE_LPBKINFO;

typedef struct _TCOMMAND_INFO{
    UCHAR uRequestType;
    BYTE pBuffer[256];
    USHORT cbBufSize;
    UFN_TRANSFER hTransfer;
    BYTE    bStartVal;
}TCOMMAND_INFO, *PTCOMMAND_INFO;

class CPipePairManager;

typedef struct _UFL_CONTEXT{
    PUFN_FUNCTIONS                  pUfnFuncs;
    UFN_HANDLE                         hDevice;
    PVOID                                  pvInterface;
    TCHAR                                 szActiveKey[MAX_PATH];
    UFN_PIPE                             hDefaultPipe;

    USB_TDEVICE_REQUEST          udr;
    PTCOMMAND_INFO                   tCommand;
    BOOL                                   fInRequestThread;
    CRITICAL_SECTION                cs;
    CRITICAL_SECTION                Ep0Callbackcs;

    HANDLE                                hResetThread;
    HANDLE                                hRequestThread;
    HANDLE                                hGotContentEvent;
    HANDLE                                hRequestCompleteEvent;

    CPipePairManager*	              pPairMan;
    UCHAR                                 uNumOfPipePairs;

    PUSB_DEVICE_DESCRIPTOR      pHighSpeedDeviceDesc; 
    PUSB_DEVICE_DESCRIPTOR      pFullSpeedDeviceDesc;
    PUFN_CONFIGURATION            pHighSpeedConfig;
    PUFN_CONFIGURATION            pFullSpeedConfig;

    USB_TDEVICE_RECONFIG         utrc; 
    BOOL                                   fInLoop;
}UFL_CONTEXT, *PUFL_CONTEXT;

typedef struct _EP0CALLBACK_CONTEXT{
    PUFL_CONTEXT pContext;
    PTCOMMAND_INFO pInfo;
}EP0CALLBACK_CONTEXT, *PEP0CALLBACK_CONTEXT;

#define	SPEED_ALL_SPEED		0
#define	SPEED_FULL_SPEED		1
#define	SPEED_HIGH_SPEED		2





//--------------function prototypies----------------------

BOOL
UFNLPBK_OpenInterface(
    PUFL_CONTEXT pContext
    );
static
VOID
UFNLPBK_HandleClearFeature(
    PUFL_CONTEXT pContext,
    USB_TDEVICE_REQUEST udr
    );

static
CONTROL_RESPONSE
UFNLPBK_HandleTestRequest(
    PUFL_CONTEXT pContext,
    USB_TDEVICE_REQUEST udr
    );

static
VOID
UFNLPBK_HandleRequest(
    PUFL_CONTEXT pContext,
    DWORD dwMsg,
    USB_TDEVICE_REQUEST udr
    );

extern "C"
DWORD
UFL_Init(
    LPCTSTR         pszRegKey
    );

extern "C"
BOOL
UFL_Deinit(
    DWORD               dwCtx);

extern "C"
DWORD
UFNLPBK_Close();

DWORD 
WINAPI
ResetThread(LPVOID lpParameter) ;

VOID ReConfig();

BOOL
SendLpbkInfo(BOOL bGetInfo);

DWORD 
WINAPI
HandleRequestThread(LPVOID lpParameter) ;

static
BOOL
WINAPI
UFNLPBK_DeviceNotify(
    PVOID   pvNotifyParameter,
    DWORD   dwMsg,
    DWORD   dwParam
    ); 


#endif // __TRANSPORT_H__

