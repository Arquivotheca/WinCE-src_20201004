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
//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

/*++

Module Name:  
    usbperf.h

Abstract:  
    USB driver service global
    
Notes: 

--*/
#ifndef __USBTDRV_H_
#define __USBTDRV_H_
#include <tux.h>
#include <kato.h>
#include <ceddk.h>
#include <usbtypes.h>

class UsbClientDrv;

// --Test Harness related definitions--

extern CKato                    *g_pKato;
extern SPS_SHELL_INFO     *g_pShellInfo;

// Suggested log verbosities
#define LOG_EXCEPTION      0
#define LOG_FAIL                2
#define LOG_ABORT             4
#define LOG_SKIP                6
#define LOG_NOT_IMPLEMENTED    8
#define LOG_PASS               10
#define LOG_DETAIL            12
#define LOG_COMMENT        14


// --physical memory allocation  related definitions--
#define MY_PHYS_MEM_SIZE 0x000A8000/PAGE_SIZE
typedef struct _PhysMemNode
{
	LONG	lSize;
	DWORD	dwVirtMem;
} PhysMemNode, *PPhysMemNode;


// -- Endpoint type strings --
static LPCTSTR  g_szEPTypeStrings[5]={
    _T("USB_ENDPOINT_TYPE_CONTROL"),
    _T("USB_ENDPOINT_TYPE_ISOCH"),
    _T("USB_ENDPOINT_TYPE_BULK"),
    _T("USB_ENDPOINT_TYPE_INTERRUPT"),
    _T("USB_ENDPOINT_TYPE_UNKNOWN")
};


// -- User input (command line) --
typedef struct _USB_PARAM {
	DWORD dwSelectDevice;
} USB_PARAM,*LPUSB_PARAM;
BOOL ParseCommandLine(LPUSB_PARAM lpUsbParam, LPCTSTR lpCommandLine) ;


//-- test commands (communicate with function side) --

//test request commands
#define TEST_REQUEST_BULKIN	1
#define TEST_REQUEST_BULKOUT	2
#define TEST_REQUEST_INTIN		3
#define TEST_REQUEST_INTOUT	4
#define TEST_REQUEST_ISOCHIN	5
#define TEST_REQUEST_ISOCHOUT	6
#define TEST_REQUEST_RECONFIG	7
#define TEST_REQUEST_DATALPBK  8
#define TEST_REQUEST_LPBKINFO   9
#define TEST_REQUEST_PAIRNUM   10
#define TEST_REQUEST_SHORTSTRESS    11
#define TEST_REQUEST_VARYLENTRAN    12

#define TEST_REQUEST_EP0TESTIN    20
#define TEST_REQUEST_EP0TESTOUT    21
#define TEST_REQUEST_EP0TESTINST    22
#define TEST_REQUEST_EP0TESTOUTST    23

#define TEST_SPREQ_DATAPIPESTALL    101

#define TEST_REQUEST_PERFIN         201 
#define TEST_REQUEST_PERFOUT        202

#define TEST_REQUEST_PERFIN_HBLOCKING_HTIMING 203
#define TEST_REQUEST_PERFOUT_HBLOCKING_HTIMING 204
#define TEST_REQUEST_PERFIN_HBLOCKING_DTIMING 205
#define TEST_REQUEST_PERFOUT_HBLOCKING_DTIMING 206
#define TEST_REQUEST_PERFIN_HBLOCKING_STIMING 207
#define TEST_REQUEST_PERFOUT_HBLOCKING_STIMING 208

#define TEST_REQUEST_PERFIN_HBLOCKING_HTIMING_PDEV 209
#define TEST_REQUEST_PERFOUT_HBLOCKING_HTIMING_PDEV 210
#define TEST_REQUEST_PERFIN_HBLOCKING_DTIMING_PDEV 211
#define TEST_REQUEST_PERFOUT_HBLOCKING_DTIMING_PDEV 212
#define TEST_REQUEST_PERFIN_HBLOCKING_STIMING_PDEV 213
#define TEST_REQUEST_PERFOUT_HBLOCKING_STIMING_PDEV 214

#define TEST_REQUEST_PERFIN_DBLOCKING_HTIMING 215
#define TEST_REQUEST_PERFOUT_DBLOCKING_HTIMING 216
#define TEST_REQUEST_PERFIN_DBLOCKING_DTIMING 217
#define TEST_REQUEST_PERFOUT_DBLOCKING_DTIMING 218
#define TEST_REQUEST_PERFIN_DBLOCKING_STIMING 219
#define TEST_REQUEST_PERFOUT_DBLOCKING_STIMING 220

#define TEST_REQUEST_PERFIN_DBLOCKING_HTIMING_PDEV 221
#define TEST_REQUEST_PERFOUT_DBLOCKING_HTIMING_PDEV 222
#define TEST_REQUEST_PERFIN_DBLOCKING_DTIMING_PDEV 223
#define TEST_REQUEST_PERFOUT_DBLOCKING_DTIMING_PDEV 224
#define TEST_REQUEST_PERFIN_DBLOCKING_STIMING_PDEV 225
#define TEST_REQUEST_PERFOUT_DBLOCKING_STIMING_PDEV 226

//choices of test device configuration/interface/speed
#define CONFIG_DEFAULT  1
#define CONFIG_ALTERNATIVE  2
#define INTERFACE_DEFAULT 1
#define INTERFACE_ALTERNATIVE 2
#define SPEED_ALL_SPEED		0
#define SPEED_FULL_SPEED		1
#define SPEED_HIGH_SPEED		2

#define USB_REQUEST_CLASS               0x20

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

typedef struct _USB_TDEVICE_RECONFIG {
    UCHAR   ucConfig;
    UCHAR   ucInterface;
    UCHAR   ucSpeed;
    USHORT wBulkPkSize;
    USHORT wIntrPkSize;
    USHORT wIsochPkSize;
    USHORT wControlPkSize;
} USB_TDEVICE_RECONFIG, * PUSB_TDEVICE_RECONFIG, * LPUSB_TDEVICE_RECONFIG;

typedef struct _USB_TDEVICE_LPBKPAIR{
    UCHAR   ucOutEp;
    UCHAR   ucInEp;
    UCHAR   ucType;
} USB_TDEVICE_LPBKPAIR, * PUSB_TDEVICE_LPBKPAIR;

typedef struct _USB_TDEVICE_DATALPBK {
    UCHAR   uOutEP;
    USHORT wBlockSize;
    USHORT wNumofBlocks;
    USHORT  usAlignment;
    DWORD   dwStartVal;
} USB_TDEVICE_DATALPBK, * PUSB_TDEVICE_DATALPBK, * LPUSB_TDEVICE_DATALPBK;
#include <poppack.h>

//--test device information--
typedef struct _USB_TDEVICE_LPBKINFO{
    UCHAR   uNumOfPipePairs;
    UCHAR   uNumofSinglePipes;
    USB_TDEVICE_LPBKPAIR   pLpbkPairs[1];
} USB_TDEVICE_LPBKINFO, * PUSB_TDEVICE_LPBKINFO;

typedef struct _USB_TDEVICE_VARYLENLPBK {
    USHORT   uOutEP;
    USHORT wNumofPackets;
    USHORT  pusTransLens[1];
} USB_TDEVICE_VARYLENLPBK, * PUSB_TDEVICE_VARYLENLPBK, * LPUSB_TDEVICE_VARYLENLPBK;

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

extern PUSB_TDEVICE_LPBKINFO   g_pTstDevLpbkInfo[4];//we allow to connect to up to 4 test devices 

//-- test related definitions --
#define TEST_LPBK_NORMAL    0
#define TEST_LPBK_PHYMEM    1
#define TEST_LPBK_PHYMEMALIGN    2
#define TEST_LPBK_NORMALSHORT    3
#define TEST_LPBK_STRESSSHORT     5
#define TEST_LPBK_ZEROLEN            6

#define TEST_WAIT_TIME  10*60*1000 //10minutes
#define TEST_DATA_SIZE 0x400 
#define DATA_LOOP_TIMES  16
#define HIGHSPEED_MULTI_PACKETS_MASK 0x1800
#define PACKETS_PART_MASK                       0x7ff


#ifdef DEBUG
#define DBG_ERR        DEBUGZONE(0)
#define DBG_WARN       DEBUGZONE(1)
#define DBG_FUNC       DEBUGZONE(2)
#define DBG_DETAIL     DEBUGZONE(3)
#define DBG_INI        DEBUGZONE(4)
#define DBG_USB        DEBUGZONE(5)
#define DBG_IO         DEBUGZONE(6)
#define DBG_WEIRD      DEBUGZONE(7)
#define DBG_EVENTS		DEBUGZONE(8)
#define DBG_CONTROL		DEBUGZONE(9)
#define DBG_BULK		DEBUGZONE(10)
#define DBG_INTERRUPT	DEBUGZONE(11)
#define DBG_ISOCH		DEBUGZONE(12)
#define DBG_THREAD		DEBUGZONE(13)
#endif


//--funciton prototypes--

VOID SetDeviceName(LPTSTR Buff, USHORT uID, LPCUSB_ENDPOINT pOutEP, LPCUSB_ENDPOINT pInEP);

//--APIs about register/unregister test driver to USBD.dll--
BOOL    RegLoadUSBTestDll( LPTSTR szDllName, BOOL bUnload );
HWND ShowDialogBox(LPCTSTR szPromptString);
VOID DeleteDialogBox(HWND hDiagWnd);

//--APIs for automation--
VOID SetRegReady(BYTE BoardNo, BYTE PortNo);
VOID DeleteRegEntry();

//--vendor transfer for communicating with test device
BOOL SendVendorTransfer(UsbClientDrv *pDriverPtr, BOOL bIn, PUSB_TDEVICE_REQUEST pUtdr, LPVOID pBuffer);
//--get test device information--
PUSB_TDEVICE_LPBKINFO GetTestDeviceLpbkInfo(UsbClientDrv * pClientDrv);

//--reconfig test device--
DWORD  SetLowestPacketSize(UsbClientDrv *pDriverPtr, BOOL bHighSpeed);
DWORD RestoreDefaultPacketSize(UsbClientDrv *pDriverPtr, BOOL bHighSpeed);
DWORD SetHighestPacketSize(UsbClientDrv *pDriverPtr);
DWORD SetAlternativePacketSize(UsbClientDrv *pDriverPtr, BOOL bHighSpeed);
DWORD LongTermDataTransTest(UsbClientDrv *pDriverPtr, DWORD dwTime, PPhysMemNode PMemN, int iID);
DWORD WINAPI TimerThread (LPVOID lpParameter); 

//--test case numbering scheme--
#define BASE 1000
#define CHAPTER9_CASE 2000
#define CONFIG_CASE 3000
#define LONGTERM_CASE 4000
#define HOST_BLOCK 5
#define FUNCTION_BLOCK 6
#define SINGLEPIPE_CASE 7000
#define SPECIAL_CASE 90000

#define PHYS_MEM_HOST 1
#define PHYS_MEM_FUNC 2
#define PHYS_MEM_ALL 3

#define HOST_TIMING 1
#define DEV_TIMING 2
#define SYNC_TIMING 3

#define ASYNC_TEST 10
#define MEM_TEST 100
#define ALIGN_TEST 200
#define SHORT_TEST  300
#define ADD_TEST 400
#define SHORTSTRESS_TEST 500
#define ZEROLEN_TEST 600


#endif

