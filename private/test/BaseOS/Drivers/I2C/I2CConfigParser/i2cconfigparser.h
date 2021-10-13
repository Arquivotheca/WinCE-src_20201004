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

#ifndef __I2CTESTCONFIGPARSER_H__
#define __I2CTESTCONFIGPARSER_H__
#include "I2CConfigParserCommon.h"


//globals for the config parser application
//definition of global variables

BYTE *BufferPtr=NULL;
BYTE *DeviceNamePtr=NULL;
I2CTRANS *gI2CData=NULL;
HANDLE gI2CConfigParserHeap=NULL;
long gI2CDataSize=0;
HANDLE I2CTestParserExitEvent=INVALID_HANDLE_VALUE;
HANDLE I2CTestParserHeapCreateEvent=INVALID_HANDLE_VALUE;
HANDLE I2CTestParserSizeSignalEvent=INVALID_HANDLE_VALUE;
HANDLE I2CTestParserBufferListCreateEvent =INVALID_HANDLE_VALUE;
HANDLE I2CTestParserDeviceNameListCreateEvent=INVALID_HANDLE_VALUE;
CRITICAL_SECTION gI2CParserCriticalSection;


#endif //__I2CTESTCONFIGPARSER_H__