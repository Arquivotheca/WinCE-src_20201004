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
#include <profiler.h>
#include <celog.h>

class UsbClientDrv;

// --Test Harness related definitions--

extern CKato              *g_pKato;
extern SPS_SHELL_INFO     *g_pShellInfo;

// Suggested log verbosities
#define LOG_EXCEPTION        0
#define LOG_FAIL             2
#define LOG_ABORT            4
#define LOG_SKIP             6
#define LOG_NOT_IMPLEMENTED  8
#define LOG_PASS             10
#define LOG_DETAIL           12
#define LOG_COMMENT          14


// --physical memory allocation  related definitions--
#define MY_PHYS_MEM_SIZE 0x000A8000/PAGE_SIZE
typedef struct _PhysMemNode
{
    LONG    lSize;
    DWORD   dwVirtMem;
} PhysMemNode, *PPhysMemNode;


// -- Endpoint type strings --
extern __declspec(selectany) LPCTSTR g_szEPTypeStrings[5]={
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
#define TEST_REQUEST_BULKIN         1
#define TEST_REQUEST_BULKOUT        2
#define TEST_REQUEST_INTIN          3
#define TEST_REQUEST_INTOUT         4
#define TEST_REQUEST_ISOCHIN        5
#define TEST_REQUEST_ISOCHOUT       6
#define TEST_REQUEST_RECONFIG       7
#define TEST_REQUEST_DATALPBK       8
#define TEST_REQUEST_LPBKINFO       9
#define TEST_REQUEST_PAIRNUM        10
#define TEST_REQUEST_SHORTSTRESS    11
#define TEST_REQUEST_VARYLENTRAN    12

#define TEST_REQUEST_EP0TESTIN      20
#define TEST_REQUEST_EP0TESTOUT     21
#define TEST_REQUEST_EP0TESTINST    22
#define TEST_REQUEST_EP0TESTOUTST   23

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
#define CONFIG_DEFAULT          1
#define CONFIG_ALTERNATIVE      2
#define INTERFACE_DEFAULT       1
#define INTERFACE_ALTERNATIVE   2
#define SPEED_ALL_SPEED         0
#define SPEED_FULL_SPEED        1
#define SPEED_HIGH_SPEED        2

#define USB_REQUEST_CLASS       0x20

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
    USHORT  wBulkPkSize;
    USHORT  wIntrPkSize;
    USHORT  wIsochPkSize;
    USHORT  wControlPkSize;
} USB_TDEVICE_RECONFIG, * PUSB_TDEVICE_RECONFIG, * LPUSB_TDEVICE_RECONFIG;

typedef struct _USB_TDEVICE_LPBKPAIR{
    UCHAR   ucOutEp;
    UCHAR   ucInEp;
    UCHAR   ucType;
} USB_TDEVICE_LPBKPAIR, * PUSB_TDEVICE_LPBKPAIR;

typedef struct _USB_TDEVICE_DATALPBK {
    UCHAR   uOutEP;
    USHORT  wBlockSize;
    USHORT  wNumofBlocks;
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
    USHORT  uOutEP;
    USHORT  wNumofPackets;
    USHORT  pusTransLens[1];
} USB_TDEVICE_VARYLENLPBK, * PUSB_TDEVICE_VARYLENLPBK, * LPUSB_TDEVICE_VARYLENLPBK;

#include <pshpack1.h>
typedef struct _USB_TDEVICE_STALLDATAPIPE {
    UCHAR   uOutEP;
    UCHAR   uStallRepeat;
    USHORT  wBlockSize;
} USB_TDEVICE_STALLDATAPIPE, * PUSB_TDEVICE_STALLDATAPIPE, * LPUSB_TDEVICE_STALLDATAPIPE;
#include <poppack.h>

#include <pshpack1.h>
typedef struct _USB_TDEVICE_PERFLPBK {
    UCHAR   uOutEP;
    UCHAR   uDirection; // 1: IN, 0: OUT
    USHORT  usRepeat;
    DWORD   dwBlockSize;
} USB_TDEVICE_PERFLPBK, * PUSB_TDEVICE_PERFLPBK, * LPUSB_TDEVICE_PERFLPBK;
#include <poppack.h>

#define MAX_USB_CLIENT_DRIVER 4
extern PUSB_TDEVICE_LPBKINFO   g_pTstDevLpbkInfo[MAX_USB_CLIENT_DRIVER]; //we allow to connect to up to 4 test devices

//-- test related definitions --
#define TEST_LPBK_NORMAL        0
#define TEST_LPBK_PHYMEM        1
#define TEST_LPBK_PHYMEMALIGN   2
#define TEST_LPBK_NORMALSHORT   3
#define TEST_LPBK_STRESSSHORT   5
#define TEST_LPBK_ZEROLEN       6

#define TEST_WAIT_TIME  10*60*1000 // 10 minutes
#define TEST_DATA_SIZE                  0x400
#define DATA_LOOP_TIMES                 16
#define HIGHSPEED_MULTI_PACKETS_MASK    0x1800
#define PACKETS_PART_MASK               0x7ff


#ifdef DEBUG
#define DBG_ERR        DEBUGZONE(0)
#define DBG_WARN       DEBUGZONE(1)
#define DBG_FUNC       DEBUGZONE(2)
#define DBG_DETAIL     DEBUGZONE(3)
#define DBG_INI        DEBUGZONE(4)
#define DBG_USB        DEBUGZONE(5)
#define DBG_IO         DEBUGZONE(6)
#define DBG_WEIRD      DEBUGZONE(7)
#define DBG_EVENTS     DEBUGZONE(8)
#define DBG_CONTROL    DEBUGZONE(9)
#define DBG_BULK       DEBUGZONE(10)
#define DBG_INTERRUPT  DEBUGZONE(11)
#define DBG_ISOCH      DEBUGZONE(12)
#define DBG_THREAD     DEBUGZONE(13)
#endif

//--funciton prototypes--

VOID SetDeviceName(LPTSTR Buff, USHORT uID, LPCUSB_ENDPOINT pOutEP, LPCUSB_ENDPOINT pInEP);

//--APIs about register/unregister test driver to USBD.dll--
BOOL RegLoadUSBTestDll( LPTSTR szDllName, BOOL bUnload );
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
//Pass By Reference so that the functions can set the Pointer to NULL for failure,
// or replace the old instance with a new instance.
BOOL WaitForReflectorReset(UsbClientDrv* pDriverPtr, int iUsbDriverIndex);
DWORD SetTestDeviceConfiguration(UsbClientDrv **pDriverPtr, BOOL bIsocConfig, BOOL bHighSpeed, int iUsbDriverIndex);
DWORD LongTermDataTransTest(UsbClientDrv *pDriverPtr, DWORD dwTime, PPhysMemNode PMemN, int iID);
DWORD WINAPI TimerThread (LPVOID lpParameter);

//-- Used only by full-speed tests.
DWORD ResetDevice(UsbClientDrv *pDriverPtr, BOOL bReEnum);

//--test case numbering scheme--
#define BASE                1000
#define CHAPTER9_CASE       2000
#define CONFIG_CASE         3000
#define LONGTERM_CASE       4000
#define SINGLEPIPE_CASE     7000
#define PHYSMEM_CASE        8000    // NEW
#define BULK_BLOCKING_CASE 10000
#define SPECIAL_CASE       90000

#define HOST_BLOCK      5
#define FUNCTION_BLOCK  6
#define BLOCK_NONE      7
extern __declspec(selectany) LPCTSTR g_szBlockingStrings[8]={
    _T("Not used"),
    _T("UNKNOWN"),
    _T("UNKNOWN"),
    _T("UNKNOWN"),
    _T("UNKNOWN"),
    _T("Host-side"),
    _T("Function-side"),
    _T("No blocking")
};
extern __declspec(selectany) LPCTSTR g_szBlocking = TEXT("Blocking Side");

#define PHYS_MEM_HOST   1
#define PHYS_MEM_FUNC   2
#define PHYS_MEM_ALL    3
extern __declspec(selectany) LPCTSTR g_szPhysMemStrings[4]={
    _T("None"),
    _T("Host-side"),
    _T("Function-side"),
    _T("Both")
};
extern __declspec(selectany)  LPCTSTR g_szPhysMem = TEXT("Physical Memory Usage");

#define HOST_TIMING 1
#define DEV_TIMING  2
#define SYNC_TIMING 3
#define RAW_TIMING  4
extern __declspec(selectany) LPCTSTR g_szTimingStrings[5]={
    _T("UNKNOWN"),
    _T("Host-side"),
    _T("Function-side"),
    _T("Synchronized"),
    _T("Host, no callback")
};
extern __declspec(selectany) LPCTSTR g_szTiming = TEXT("Timing Calculation");

#define NORMAL_PRIO 0
#define LOW_PRIO    1
#define HIGH_PRIO   2
#define CRIT_PRIO   3
extern __declspec(selectany) LPCTSTR g_szPriorityStrings[4]={
    _T("Normal Priority"),
    _T("High Priority"),
    _T("Low Priority"),
    _T("Critical Priority"),
};
extern __declspec(selectany) LPCTSTR g_szPriority = TEXT("Thread Priority");

extern __declspec(selectany) LPCTSTR g_szMfrFreqStrings[4]={
    _T("none"),
    _T("x2"),
    _T("x4"),
    _T("x8"),
};
extern __declspec(selectany) LPCTSTR g_szMfrFreq = TEXT("Microframes");

extern __declspec(selectany) LPCTSTR g_szPacketSize        = TEXT("Packet Size");
extern __declspec(selectany) LPCTSTR g_szBlockSize         = TEXT("Block Size");
extern __declspec(selectany) LPCTSTR g_szTransferType      = TEXT("Transfer Type");
extern __declspec(selectany) LPCTSTR g_szDirection         = TEXT("Direction");
extern __declspec(selectany) LPCTSTR g_szLargeTransferSize = TEXT("Large Transfer Size");
extern __declspec(selectany) LPCTSTR g_szSpeed             = TEXT("Speed");


#define ASYNC_TEST           10
#define MEM_TEST            100
#define ALIGN_TEST          200
#define SHORT_TEST          300
#define ADD_TEST            400
#define SHORTSTRESS_TEST    500
#define ZEROLEN_TEST        600


//-- PerfScenario function pointers --
enum PERFSCENARIO_STATUS {PERFSCEN_UNINITIALIZED, PERFSCEN_INITIALIZED, PERFSCEN_ABORT};
typedef HRESULT (WINAPI* PFN_PERFSCEN_OPENSESSION)(LPCTSTR lpszSessionName, BOOL fStartRecordingNow);
typedef HRESULT (WINAPI* PFN_PERFSCEN_CLOSESESSION)(LPCTSTR lpszSessionName);
typedef HRESULT (WINAPI* PFN_PERFSCEN_ADDAUXDATA)(LPCTSTR lpszLabel, LPCTSTR lpszValue);
typedef HRESULT (WINAPI* PFN_PERFSCEN_FLUSHMETRICS)(BOOL fCloseAllSessions, GUID* scenarioGuid, LPCTSTR lpszScenarioNamespace, LPCTSTR lpszScenarioName, LPCTSTR lpszLogFileName, LPCTSTR lpszTTrackerFileName, GUID* instanceGuid);

#define PERF_SCENARIO_NAMESPACE L"USB"
#define PERF_OUTFILE            L"USBPerf.xml"

// Enum is created so that block sizes can be used in a switch statement.
// This new enumeration does not affect how block sizes are used in the array.
enum {  BLOCK_SIZE_16     = 16,
        BLOCK_SIZE_64     = 64,
        BLOCK_SIZE_128    = 128,
        BLOCK_SIZE_256    = 256,
        BLOCK_SIZE_512    = 512,
        BLOCK_SIZE_1024   = 1024,
        BLOCK_SIZE_1536   = 1536,
        BLOCK_SIZE_2048   = 1024*2,
        BLOCK_SIZE_4096   = 1024*4,
        BLOCK_SIZE_8192   = 1024*8,
        BLOCK_SIZE_16384  = 1024*16,
        BLOCK_SIZE_24567  = 1024*24,
        BLOCK_SIZE_30720  = 1024*30,
        BLOCK_SIZE_32768  = 1024*32,
        BLOCK_SIZE_36864  = 1024*36,
        BLOCK_SIZE_40960  = 1024*40,
        BLOCK_SIZE_49152  = 1024*48,
        BLOCK_SIZE_61440  = 1024*60,
        BLOCK_SIZE_64000  = 512*125,
        BLOCK_SIZE_65536  = 1024*64,
        BLOCK_SIZE_LAST   = 0 };

extern const __declspec(selectany) DWORD BLOCK_SIZES[] = {
        BLOCK_SIZE_64,
        BLOCK_SIZE_128,
        BLOCK_SIZE_512,
        BLOCK_SIZE_1024,
        BLOCK_SIZE_1536,
        BLOCK_SIZE_4096,
        BLOCK_SIZE_8192,
        BLOCK_SIZE_16384,
        BLOCK_SIZE_32768,
        BLOCK_SIZE_65536,
};
#define NUM_BLOCK_SIZES  (sizeof(BLOCK_SIZES)/sizeof(DWORD))


#endif

