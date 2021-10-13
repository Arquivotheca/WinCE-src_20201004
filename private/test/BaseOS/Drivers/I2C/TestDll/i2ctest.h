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
////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//
//  Module: I2CTest.h
//          Header for all files in the project.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __I2CTEST_H__
#define __I2CTEST_H__

////////////////////////////////////////////////////////////////////////////////
// Included files


#include "I2CTestCommon.h"


extern CRITICAL_SECTION gI2CTestDataCriticalSection;
extern CRITICAL_SECTION gI2CTestInitCriticalSection;
#define AcquireI2CTestDataLock()  EnterCriticalSection(&gI2CTestDataCriticalSection)
#define ReleaseI2CTestDataLock()  LeaveCriticalSection(&gI2CTestDataCriticalSection)
#define AcquireI2CTestInitLock()  EnterCriticalSection(&gI2CTestInitCriticalSection)
#define ReleaseI2CTestInitLock()  LeaveCriticalSection(&gI2CTestInitCriticalSection)


extern I2CHANDLELOOKUP I2CHandleTable[];
extern DWORD dwI2CHandleTableLen;
extern DWORD gdwI2CTRANSInitialized;
extern DWORD gdwInitFailed;
extern I2CTRANS *gI2CTrans;
extern BYTE *gBufferPtrList;
extern BYTE *gDeviceNamePtrList;
extern long gI2CTransSize;
extern HANDLE I2CTestHeap;

extern HANDLE I2CTestHeapCreateEvent;
extern HANDLE I2CTestParserExitEvent;
extern HANDLE I2CTestSizeSignalEvent;
extern HANDLE I2CTestBufferListCreateEvent;
extern HANDLE I2CTestDeviceNameListCreateEvent;
extern HANDLE ghUserProcess;
extern PROCESS_INFORMATION gProcInfo;

#define I2C_DEVICENAME L"I2C1:"
#define SUBORDINATE_DEVICE_CAMERA_ADDR 0x30
#define DEFAULT_STRESS_TIME 10*60*1000 //ms
#define DEFAULT_I2C_CONFIG_FILE L"\\Release\\I2C_Test_Config.xml"

#define SUBORDINATE_ADDR_SIZE_VALID_COUNT 2
#define SUBORDINATE_ADDR_SIZE_INVALID_COUNT 3

#define SPEED_VALID_COUNT 6
#define TIMEOUT_VALID_COUNT 3
#define TIMEOUT_INVALID_COUNT 2

static const DWORD g_adwValidSubordinateAddrSize[] = {7, 10};
static const DWORD g_adwInvalidSubordinateAddrSize[] = {6, 8, 11};

static const DWORD g_adwValidTransSpeed[] = {I2C_STANDARD_SPEED/2, 
                                             I2C_STANDARD_SPEED, 
                                             (I2C_STANDARD_SPEED + I2C_FAST_SPEED)/2 , 
                                             I2C_FAST_SPEED, 
                                             (I2C_FAST_SPEED + I2C_HIGH_SPEED)/2, 
                                             I2C_HIGH_SPEED};
static const DWORD g_adwValidTimeOut[] = {1, 10, 2000};
static const DWORD g_adwInvalidTimeOut[] = {0, 3000};


//HANDLE hBusHandle[I2C_MAX_NUM_BUSES];
DWORD getParams(TCHAR* szFileName );
DWORD FreeParams();
DWORD TestDeinit();
DWORD TestInit();
void PrintUsage();
void PrintI2CDataList();
void PrintI2CData(I2CTRANS *I2CTestData, long Index);

DWORD Hlp_OpenI2CHandle(I2CTRANS *I2CTestData, long Index);
DWORD Hlp_CloseI2CHandle(I2CTRANS *I2CTestData,long Index);
DWORD Hlp_SetI2CSubordinateAddrSize(I2CTRANS *I2CTestData,long Index);
DWORD Hlp_SetI2CSubordinate(I2CTRANS *I2CTestData,long Index);
DWORD Hlp_SetI2CRegAddrSize(I2CTRANS *I2CTestData,long Index);
DWORD Hlp_SetI2CSpeed(I2CTRANS *I2CTestData,long Index);
DWORD Hlp_SetI2CSpeed(HANDLE hI2C, DWORD dwSpeed);

DWORD Hlp_SetI2CTimeOut(I2CTRANS *I2CTestData,long Index);
DWORD Hlp_I2CWrite(I2CTRANS *I2CTestData,long Index);
DWORD Hlp_I2CWrite(HANDLE hI2C, I2CTRANS I2CTestData);
DWORD Hlp_I2CRead(I2CTRANS *I2CTestData, long Index);
DWORD Hlp_I2CRead(HANDLE hI2C, I2CTRANS I2CTestData);

DWORD Hlp_I2CLoopback(I2CTRANS *I2CTestData, long BusIndex);
DWORD Hlp_I2CLoopback(HANDLE hI2C, I2CTRANS I2CTestData);

DWORD Hlp_SetupBusData(I2CTRANS *I2CTestData, long Index);
DWORD Hlp_SetupDeviceData(I2CTRANS *I2CTestData, long Index);
DWORD Hlp_GetNextTransIndex(I2CTRANS *I2CTestData, I2C_OP OperationToFind, long *pIndex);
VOID PrintBufferData(BYTE *buffer, DWORD dwLenth);

DWORD WINAPI I2CTransactionThread (LPVOID lpParameter) ;
BOOL LoadI2CTestHook();
VOID UnloadI2CTestHook();
BOOL ApplyTestHook();
BOOL ClearTestHook();







////////////////////////////////////////////////////////////////////////////////
// Suggested log verbosities

#define LOG_EXCEPTION          0
#define LOG_FAIL               2
#define LOG_ABORT              4
#define LOG_SKIP               6
#define LOG_NOT_IMPLEMENTED    8
#define LOG_PASS              10
#define LOG_DETAIL            12
#define LOG_COMMENT           14

////////////////////////////////////////////////////////////////////////////////

//-- PerfScenario function pointers --
enum PERFSCENARIO_STATUS {PERFSCEN_UNINITIALIZED, PERFSCEN_INITIALIZED, PERFSCEN_ABORT};
typedef HRESULT (*PFN_PERFSCEN_OPENSESSION)(LPCTSTR lpszSessionName, BOOL fStartRecordingNow);
typedef HRESULT (*PFN_PERFSCEN_CLOSESESSION)(LPCTSTR lpszSessionName);
typedef HRESULT (*PFN_PERFSCEN_ADDAUXDATA)(LPCTSTR lpszLabel, LPCTSTR lpszValue);
typedef HRESULT (*PFN_PERFSCEN_FLUSHMETRICS)(BOOL fCloseAllSessions, GUID* scenarioGuid, LPCTSTR lpszScenarioNamespace, LPCTSTR lpszScenarioName, LPCTSTR lpszLogFileName, LPCTSTR lpszTTrackerFileName, GUID* instanceGuid);

// {663A54D1-69C4-4352-8332-D4171933DB90}
extern const __declspec(selectany) GUID I2C_PERF_GUID = 
{ 0x663a54d1, 0x69c4, 0x4352, { 0x83, 0x32, 0xd4, 0x17, 0x19, 0x33, 0xdb, 0x90 } };


// We'll load PerfScenario explicitly
#define PERFSCENARIO_DLL L"PerfScenario.dll"
#define MDPGPERF_I2C             TEXT("MDPG\\I2C")

static const DWORD NUM_PERF_MEASUREMENTS         = 5;   // How many performance iterations to perform.

// Define the statistics we wish to capture
// PERF_ITEMS1: Transition times
typedef enum {PERF_ITEM_DUR_READ_STAN_SPEED = 0,
      PERF_ITEM_DUR_READ_FAST_SPEED,
      PERF_ITEM_DUR_READ_HIGH_SPEED,
      PERF_ITEM_DUR_WRITE_STAN_SPEED,
      PERF_ITEM_DUR_WRITE_FAST_SPEED,
      PERF_ITEM_DUR_WRITE_HIGH_SPEED,

      NUM_PERF_ITEMS1}PERF_ITEM;




// The following functions are used to determine where to put the resulting
// performance output file.
// GetVirtualRelease is part of qa.dll, which cannot be brought into kernel-mode
// because of its dependency on shlwapi.dll (user-mode).

#define PERF_I2C_OUTFILE L"I2CPerf.xml"
#define PERF_I2C_NAMESPACE L"i2c"
#define RELEASE_PATH TEXT( "\\release\\" )

typedef struct _I2CTRANSPARAMS{
    DWORD dwThreadNo;
}I2CTRANSPARAMS, *PI2CTRANSPARAMS;

#define WAIT_THREAD_DONE    240*1000   //Thread Timeout of 240 secs
#define MAX_THREADS 100



#endif // __I2CTEST_H__
