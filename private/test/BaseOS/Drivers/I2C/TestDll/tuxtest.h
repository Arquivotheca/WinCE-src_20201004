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

#ifndef __TUXTEST_H__
#define __TUXTEST_H__
#include "I2CTest.h"
//global variable definitions 
I2CTRANS *gI2CTrans;
BYTE *gBufferPtrList;
BYTE *gDeviceNamePtrList;
long gI2CTransSize=0;
DWORD gdwI2CTRANSInitialized=0;
DWORD gdwInitFailed=0;

//HANDLE I2CTestMutex=INVALID_HANDLE_VALUE;
HANDLE I2CTestHeap=INVALID_HANDLE_VALUE;
HANDLE I2CTestHeapCreateEvent=INVALID_HANDLE_VALUE;
HANDLE I2CTestParserExitEvent=INVALID_HANDLE_VALUE;
HANDLE I2CTestSizeSignalEvent=INVALID_HANDLE_VALUE;
HANDLE I2CTestBufferListCreateEvent=INVALID_HANDLE_VALUE;
HANDLE I2CTestDeviceNameListCreateEvent=INVALID_HANDLE_VALUE;
HANDLE ghUserProcess= INVALID_HANDLE_VALUE;
PROCESS_INFORMATION gProcInfo;

CRITICAL_SECTION gI2CTestDataCriticalSection;
CRITICAL_SECTION gI2CTestInitCriticalSection;
#endif