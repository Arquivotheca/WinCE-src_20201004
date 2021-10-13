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
//  TUXTEST TUX DLL
//
//  Module: I2CTest.h
//          Declares all global variables and test function prototypes EXCEPT
//          when included by globals.cpp, in which case it DEFINES global
//          variables, including the function table.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __I2CTESTCONFIGPARSERCOMMON_H__
#define __I2CTESTCONFIGPARSERCOMMON_H__
#include "I2CTestCommon.h"

DWORD FreeParams();
//globals for the config parser application

extern BYTE *BufferPtr;
extern BYTE *DeviceNamePtr;
extern I2CTRANS *gI2CData;
extern long gI2CDataSize;
extern HANDLE gI2CConfigParserHeap;
extern CRITICAL_SECTION gI2CParserCriticalSection;

//dont need critical section here, the test actually ensures only Single entry access to parser process
#define AcquireI2CTestDataLock() //EnterCriticalSection(&gI2CParserCriticalSection)
#define ReleaseI2CTestDataLock() //LeaveCriticalSection(&gI2CParserCriticalSection)


#endif //__I2CTESTCONFIGPARSERCOMMON_H__