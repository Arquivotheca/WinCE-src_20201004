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
#ifndef __I2CTESTCOMMON_H__
#define __I2CTESTCOMMON_H__

////////////////////////////////////////////////////////////////////////////////
// Included files

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <tux.h>
#include <kato.h>
#include <Pm.h>
#include <i2c.h>
#include <MDPGPerf.h>




// GLOBALS TYPEDEFS FOR I2C TEST//

typedef enum { I2C_OP_READ=0,I2C_OP_WRITE, I2C_OP_LOOPBACK} I2C_OP;

  
typedef struct __I2CTRANS
{
    int HandleId;
    DWORD dwTransactionID;
    TCHAR *pwszDeviceName;
    I2C_OP Operation;
    DWORD dwSubordinateAddress;
    DWORD dwRegisterAddress;
    DWORD dwLoopbackAddress;
    DWORD dwNumOfBytes;
    BYTE *BufferPtr;
    DWORD dwSpeedOfTransaction;
    DWORD dwTimeOut;
    DWORD dwSubordinateAddressSize;
    DWORD dwRegisterAddressSize;
    
} I2CTRANS, *PI2CTRANS;

typedef struct __I2CHANDLELOOKUP
{
    HANDLE hI2C;
    TCHAR pwszDeviceName[MAX_PATH];
}I2CHANDLELOOKUP, *PI2CHANDLELOOKUP;

static const TCHAR g_astrI2COp[][10] = {L"READ",
                                          L"WRITE",
                                          L"LOOPBACK"};


static const LPCTSTR pszIDAttribute    = L"ID";

DWORD getParams(TCHAR* szFileName );
void PrintI2CDataList();

#define I2CTEST_NAMED_EVENT_HEAP_CREATE             L"I2CTestHeapCreateEvent"
#define I2CTEST_NAMED_EVENT_PARSER_EXIT             L"I2CTestParserExitEvent"
#define I2CTEST_NAMED_EVENT_SIZE_SIGNAL             L"I2CTestSizeSignalEvent"  
#define I2CTEST_NAMED_EVENT_BUFFER_LIST_CREATE             L"I2CTestBufferListCreateEvent"
#define I2CTEST_NAMED_EVENT_DEVICE_NAME_LIST_CREATE           L"I2CTestDeviceNameListCreateEvent"
//GLOBAL MACRO DEFINITIONS for I2C TEST

#define I2CTIMEOUT  10000
#ifndef SUCCESS
#define SUCCESS    1
#endif

#ifndef FAILURE
#define FAILURE    0
#endif

#ifndef ZeroMemory
#define ZeroMemory(Destination,Length) memset(Destination, 0, Length)
#endif

#ifndef COUNTOF
#define    COUNTOF(x) ((sizeof(x)/sizeof(x[0])))
#endif

#ifndef MANUAL_RELEASE
#define MANUAL_RELEASE(p)       do{ if ((p)!=NULL) {delete p; p=NULL;} }while(0)
#endif

#ifndef SAFE_CLOSE
#define SAFE_CLOSE(x)           do{if(INVALID_HANDLE_VALUE!=(x)) {CloseHandle(x);} }while(0)
#endif

#define MINS_ADJUST     60000
#define MAX_I2C_BUFFER_SIZE     2048
#define DEFAULT_I2C_OPERATION I2C_OP_READ
#define DEFAULT_REGISTER_ADDRESS 0
#define DEFAULT_SUBORDINATE_ADDRESS_SIZE 7
#define DEFAULT_REG_ADDRESS_SIZE 1
#define DEFAULT_I2C_SPEED 100000
#define DEFAULT_TIMEOUT 10

#endif // __I2CTESTCOMMON_H__
