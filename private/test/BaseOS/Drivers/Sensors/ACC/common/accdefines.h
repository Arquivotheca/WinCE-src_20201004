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

#pragma once

#include <windows.h>
#include <types.h>
#include <ceddk.h>
#include <regmani.h>
#include <accapi.h>

#include <tux.h>




DEFINE_GUID(guidAccelerometer3dType_BAD,    0x5D85E461, 0x0C36, 0x4ef4, 0x99, 0x2e, 0x8d, 0x3d, 0x2c, 0x1e, 0xb9, 0x40);

//DEFINE_GUID(g_guid3dAccelerometer,          0xC2FB0F5F, 0xE2D2, 0x4C78, 0xBC, 0xD0, 0x35, 0x2A, 0x95, 0x82, 0x81, 0x9D);









//------------------------------------------------------------------------------
// Test Suite Constants
//
static const int device_index = 1;
static const int max_threads = 5;

#define ACC_MIN_SAMPLE_INTERVAL (20) //ms (as defined by Chassis 1 spec)

#define ACC_DRIVER_DEVICE_NAME _T("ACC1:")
#define ACC_INVALID_HANDLE_VALUE ((HSENSOR)INVALID_HANDLE_VALUE)
#define ACC_TEST_SAMPLE_SIZE (10)

#define TEST_PARAM_MAP_NAME L"ACC_TST_MAP"


#define MAKE_IOCTL(c) CTL_CODE (0x8000, (c + 2048), METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_CONFIGURE     MAKE_IOCTL(1)
#define IOCTL_RUN_TEST      MAKE_IOCTL(2)
#define IOCTL_STOP_TEST     MAKE_IOCTL(3)

#define CALLBACK_CONTEXT_SIG    ((LPVOID)0xDEADBEEF)



// We do not REQUIRE developers to use the following keys.
static const TCHAR* pszAccKeyMdd = _T("Drivers\\BuiltIn\\ACC");

static const TCHAR* pszAccKeyPdd = _T("Drivers\\BuiltIn\\ACC\\PDD");




static const BOOL  Q_NOM_ACCESS = TRUE;
static const DWORD Q_NOM_SIZE = sizeof(MSGQUEUEOPTIONS);
static const DWORD Q_NOM_FLAG = MSGQUEUE_ALLOW_BROKEN;
static const DWORD Q_NOM_MAXMSG = 100;
static const DWORD Q_NOM_MAXMSG_SIZE = sizeof( ACC_DATA );

static MSGQUEUEOPTIONS nominalQOpt = 
{
    Q_NOM_SIZE,
    Q_NOM_FLAG,
    Q_NOM_MAXMSG, 
    Q_NOM_MAXMSG_SIZE, 
    Q_NOM_ACCESS,
};








//------------------------------------------------------------------------------
// Test Structures
//
struct ACC_VECTOR
{
    double x;
    double y;
    double z;
};

struct TestEntry
{
    DWORD (*pTestFunction)();
    const TCHAR* szTestDesc;
};

struct TestCommand
{
    DWORD dwTestCnt;
    DWORD dwSize;
    DWORD dwTestIds[1];
};

struct TestCommandIo
{
    DWORD dwIoCode;
    TestCommand in;
    DWORD* pOutBuf;
};

struct AccRegMdd
{
    TCHAR szDll[MAX_PATH];
    TCHAR szPrefix[MAX_PATH];
    TCHAR szIClass[1024];
    DWORD dwOrder;
};

struct AccRegPdd
{
    TCHAR szDll[MAX_PATH];
};

struct AccRegEntry
{
    AccRegMdd mdd;
    AccRegPdd pdd;
};


struct AccTestParam
{
    ACC_VECTOR accHome;    
    BOOL  bUserMode;
    BOOL  bFakePdd;
    DWORD dwNumberOfClients;    
    DWORD dwNumberOfSamples;
    DWORD dwTimeErrorMargin;
    DWORD dwDataErrorMargin;
    AccRegEntry accReg;
};

struct AccSampleInfo
{
    BOOL       bInRange;
    DWORD      dwTimmeStampDev;

};

struct AccDataAnalysis
{
    DWORD dwSampleCount;
    DWORD dwTimeDeltaDev;
    DWORD dwTimeDeltaAvg;
    ACC_DATA   acc_last;
    ACC_VECTOR acc_home;
    ACC_VECTOR acc_dev;
    ACC_VECTOR acc_avg;
    BOOL bDataInRange;
    BOOL bTimeInRange;
};

struct AccDataEnvelope
{
    ACC_VECTOR dat_upper;
    ACC_VECTOR dat_lower;
    DWORD time_upper;
    DWORD time_lower;
};

//Callback Verification Helpers
struct AccSampleCollection_t
{
    DWORD dwSampleCount;
    DWORD dwMaxSamples;
    ACC_DATA* pSamples;
};


// Test Vector Structures
struct AccOpenSensorTstData_t
{
    WCHAR*  in_name;     //Test name input
    PLUID   in_pSensorLuid;
    HSENSOR expHandle;   //Expected Handle Result
    DWORD   expError;    //Expected Last Error
};

struct AccOpenAccStartTstData_t
{
    HSENSOR hAcc;  
    HANDLE  hQueue;
    DWORD   expError;    
    BOOL    expSamples;
};

struct AccOpenAccStopTstData_t
{
    HSENSOR hAcc;  
    HANDLE  hQueue;
    DWORD   expError;    
    BOOL    expSamples;
};

struct AccCreateCallTstData_t
{
    HSENSOR hAcc;
    BOOL    bValidCallback;
    BOOL    bFakeCallback;
    BOOL    bHasContext;
    LPVOID  lpvContext;
    DWORD   expError;    
    BOOL    expSamples;
};

struct AccCancelCallTstData_t
{
    HSENSOR hAcc;
    BOOL    bValidCallback;
    DWORD   expError;    
    BOOL    expSamples;
};

struct AccSetMode_t
{
    HSENSOR hAcc;
    HANDLE  hQueue;
    ACC_CONFIG_MODE configMode;
    LPVOID reserved;
    DWORD   expError;    
    BOOL    expOSamples;
    BOOL    expSSamples;
};


struct AccIoControlTstData_t
{
     HANDLE hDevice_in;
     DWORD  dwIOCTL_in;
     LPVOID lpInBuffer_in;
     DWORD nInBufferSize_in;
     DWORD nOutBufferSize_in;
     LPVOID lpOutBuffer_out;
     LPDWORD lpBytesReturned_out;
     DWORD bytesReturned_exp;
     DWORD dwLastErr_exp;
     BOOL bIoReturn_exp;
     BOOL bValidStartStop;
};



//------------------------------------------------------------------------------
// Command Line Options
//
#define CMD_CLIENTS     _T("clients")
#define CMD_CLIENTS_MIN  1
#define CMD_CLIENTS_DEF  1
#define CMD_CLIENTS_MAX 99

#define CMD_TIME_MARGIN _T("time_margin")
#define CMD_TIME_MARGIN_MIN 1  //%
#define CMD_TIME_MARGIN_DEF 5  //%
#define CMD_TIME_MARGIN_MAX 25 //%

#define CMD_SAMPLES     _T("samples")
#define CMD_SAMPLES_MIN  1
#define CMD_SAMPLES_DEF  150
#define CMD_SAMPLES_MAX  200

#define CMD_SET_X       _T("setx")
#define CMD_SET_Y       _T("sety")
#define CMD_SET_Z       _T("setz")

#define CMD_HELP        _T("help")

#define CMD_DATA_MARGIN _T("data_margin")
#define CMD_DATA_MARGIN_DEF 5 //% 

#define CMD_KERNEL      _T("kernel")
#define CMD_USER        _T("user")




//------------------------------------------------------------------------------
// Helping Hands
//
#ifndef VALID_ACC_HANDLE
#define VALID_ACC_HANDLE(X) (X != ACC_INVALID_HANDLE_VALUE && X != NULL)
#endif
#ifndef CHECK_CLOSE_ACC_HANDLE
#define CHECK_CLOSE_ACC_HANDLE(X) if (VALID_ACC_HANDLE(X)) { CloseHandle(X); X = ACC_INVALID_HANDLE_VALUE; }
#endif

