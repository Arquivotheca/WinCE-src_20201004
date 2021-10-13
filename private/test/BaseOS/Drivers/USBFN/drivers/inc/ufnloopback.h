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

#pragma once

#include <windows.h>
#include "usbfntypes.h"

#define dims(x) (sizeof(x)/sizeof((x)[0]))

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
#define TEST_REQUEST_BULKIN         1
#define TEST_REQUEST_BULKOUT        2
#define TEST_REQUEST_INTIN          3
#define TEST_REQUEST_INTOUT         4
#define TEST_REQUEST_ISOCHIN        5
#define TEST_REQUEST_ISOCHOUT       6
#define TEST_REQUEST_RESET          7
#define TEST_REQUEST_DATALPBK       8
#define TEST_REQUEST_LPBKINFO       9
#define TEST_REQUEST_PAIRNUM        10
#define TEST_REQUEST_SHORTSTRESS    11
#define TEST_REQUEST_VARYLENTRAN    12

#define TEST_REQUEST_EP0TESTIN      20
#define TEST_REQUEST_EP0TESTOUT     21
#define TEST_REQUEST_EP0TESTINST    22
#define TEST_REQUEST_EP0TESTOUTST   23
#define TEST_REQUEST_EP0ZEROINLEN   24

#define TEST_REQUEST_QUIT           30

#define TEST_SPREQ_DATAPIPESTALL    101
#define TEST_SPREQ_DATAPIPESHORT    102

#define TEST_REQUEST_PERFIN         201
#define TEST_REQUEST_PERFOUT        202

#define TEST_REQUEST_PERFIN_HBLOCKING_HTIMING       203
#define TEST_REQUEST_PERFOUT_HBLOCKING_HTIMING      204
#define TEST_REQUEST_PERFIN_HBLOCKING_DTIMING       205
#define TEST_REQUEST_PERFOUT_HBLOCKING_DTIMING      206
#define TEST_REQUEST_PERFIN_HBLOCKING_STIMING       207
#define TEST_REQUEST_PERFOUT_HBLOCKING_STIMING      208

#define TEST_REQUEST_PERFIN_HBLOCKING_HTIMING_PDEV  209
#define TEST_REQUEST_PERFOUT_HBLOCKING_HTIMING_PDEV 210
#define TEST_REQUEST_PERFIN_HBLOCKING_DTIMING_PDEV  211
#define TEST_REQUEST_PERFOUT_HBLOCKING_DTIMING_PDEV 212
#define TEST_REQUEST_PERFIN_HBLOCKING_STIMING_PDEV  213
#define TEST_REQUEST_PERFOUT_HBLOCKING_STIMING_PDEV 214

#define TEST_REQUEST_PERFIN_DBLOCKING_HTIMING       215
#define TEST_REQUEST_PERFOUT_DBLOCKING_HTIMING      216
#define TEST_REQUEST_PERFIN_DBLOCKING_DTIMING       217
#define TEST_REQUEST_PERFOUT_DBLOCKING_DTIMING      218
#define TEST_REQUEST_PERFIN_DBLOCKING_STIMING       219
#define TEST_REQUEST_PERFOUT_DBLOCKING_STIMING      220

#define TEST_REQUEST_PERFIN_DBLOCKING_HTIMING_PDEV  221
#define TEST_REQUEST_PERFOUT_DBLOCKING_HTIMING_PDEV 222
#define TEST_REQUEST_PERFIN_DBLOCKING_DTIMING_PDEV  223
#define TEST_REQUEST_PERFOUT_DBLOCKING_DTIMING_PDEV 224
#define TEST_REQUEST_PERFIN_DBLOCKING_STIMING_PDEV  225
#define TEST_REQUEST_PERFOUT_DBLOCKING_STIMING_PDEV 226


#define dims(x) (sizeof(x)/sizeof((x)[0]))

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

#include <pshpack1.h>
typedef struct _USB_TDEVICE_STALLDATAPIPE {
    UCHAR   uOutEP;
    UCHAR   uStallRepeat;
    USHORT wBlockSize;
} USB_TDEVICE_STALLDATAPIPE, * PUSB_TDEVICE_STALLDATAPIPE, * LPUSB_TDEVICE_STALLDATAPIPE;
#include <poppack.h>

#include <pshpack1.h>
typedef struct _USB_TDEVICE_PERFLPBK {
    UCHAR   uOutEP;
    UCHAR   uDirection; // 1: IN, 0: OUT
    USHORT usRepeat;
    DWORD   dwBlockSize;
} USB_TDEVICE_PERFLPBK, * PUSB_TDEVICE_PERFLPBK, * LPUSB_TDEVICE_PERFLPBK;
#include <poppack.h>

typedef struct _USB_TDEVICE_VARYLENLPBK {
    USHORT uOutEP;
    USHORT wNumofPackets;
    USHORT pusTransLens[1];
} USB_TDEVICE_VARYLENLPBK, * PUSB_TDEVICE_VARYLENLPBK, * LPUSB_TDEVICE_VARYLENLPBK;


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
    BYTE    bStartVal;
    USHORT cbBufSize;
    UFN_TRANSFER hTransfer;
    BYTE pBuffer[1024];  // TODO: either dyn alloc or make it bigger
}TCOMMAND_INFO, *PTCOMMAND_INFO;

typedef struct _SPTHREADSINFO{
    HANDLE hThread;
    BOOL    fRunning;
}SPTHREADSINFO, *PSPTREHADINFO;
#define NUM_OF_SPTHEADS     8

class CPipePairManager;

typedef struct _UFL_CONTEXT{
    PUFN_FUNCTIONS               pUfnFuncs;
    UFN_HANDLE                   hDevice;
    PVOID                        pvInterface;
    TCHAR                        szActiveKey[MAX_PATH];
    UFN_PIPE                     hDefaultPipe;

    USB_TDEVICE_REQUEST          udr;
    PTCOMMAND_INFO               tCommand;
    BOOL                         fInRequestThread;
    BOOL                         fResetting;
    CRITICAL_SECTION             cs;
    CRITICAL_SECTION             Ep0Callbackcs;
    CRITICAL_SECTION             Resetcs;

    HANDLE                       hResetThread;
    HANDLE                       hRequestThread;
    HANDLE                       hGotContentEvent;
    HANDLE                       hRequestCompleteEvent;

    CPipePairManager*            pPairMan;
    UCHAR                        uNumOfPipePairs;

    PUSB_DEVICE_DESCRIPTOR       pHighSpeedDeviceDesc;
    PUSB_DEVICE_DESCRIPTOR       pFullSpeedDeviceDesc;
    PUFN_CONFIGURATION           pHighSpeedConfig;
    PUFN_CONFIGURATION           pFullSpeedConfig;

    USB_TDEVICE_RECONFIG         utrc;
    BOOL                         fInLoop;
    SPTHREADSINFO                spThreads[NUM_OF_SPTHEADS];
}UFL_CONTEXT, *PUFL_CONTEXT;

typedef struct _EP0CALLBACK_CONTEXT{
    PUFL_CONTEXT pContext;
    PTCOMMAND_INFO pInfo;
}EP0CALLBACK_CONTEXT, *PEP0CALLBACK_CONTEXT;

#define SPEED_ALL_SPEED     0
#define SPEED_FULL_SPEED    1
#define SPEED_HIGH_SPEED    2





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

VOID
SendSpecialCasesCommand(PUFL_CONTEXT pContext, PTCOMMAND_INFO pInfo);

