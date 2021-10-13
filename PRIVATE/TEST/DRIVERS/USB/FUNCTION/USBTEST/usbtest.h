//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
    usbtest.h

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
#define TEST_REQUEST_EP0TESTIN    12
#define TEST_REQUEST_EP0TESTOUT    13

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

extern PUSB_TDEVICE_LPBKINFO   g_pTstDevLpbkInfo[4];//we allow to connect to up to 4 test devices 

//-- test related definitions --
#define TEST_LPBK_NORMAL    0
#define TEST_LPBK_PHYMEM    1
#define TEST_LPBK_PHYMEMALIGN    2
#define TEST_LPBK_NORMALSHORT    3
#define TEST_LPBK_STRESSSHORT     5

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

//-- some additional tests--
DWORD 
ResetDevice(UsbClientDrv *pDriverPtr, BOOL bReEnum);
DWORD  SuspendDevice(UsbClientDrv *pDriverPtr);
DWORD  ResumeDevice(UsbClientDrv *pDriverPtr);
DWORD EP0Test(UsbClientDrv *pDriverPtr);

//--test case numbering scheme--
#define BASE 1000
#define CHAPTER9_CASE 2000
#define CONFIG_CASE 3000
#define LONGTERM_CASE 4000
#define SINGLEPIPE_CASE 7000
#define SPECIAL_CASE 9000

#define ASYNC_TEST 10
#define MEM_TEST 100
#define ALIGN_TEST 200
#define SHORT_TEST  300
#define ADD_TEST 400
#define SHORTSTRESS_TEST 500


#endif

