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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*
 * deviceInfoTests.h
 */

/*
 * This file includes constants, defines and functions that are specific to 
 * Device Info related IOCTLs 
 */

/**********************************  Headers  *********************************/

/* common utils */
#include "..\..\..\common\commonUtils.h"
#include "..\..\..\common\commonIoctlTests.h"

#include <windows.h>
#include <tchar.h>


////////////////////////////////////////////////////////////////////////////////
#ifndef __DEVICEINFO_TESTS_H__
#define __DEVICEINFO_TESTS_H__


/**********************************  Functions ********************************/

// This function prints the output of the Device Info IOCTL.
BOOL PrintDeviceInfoOutput(DWORD dwInputSpi);

// This function verifies invalid inputs for Device Info IOCTL.
// Possible DWORD values other than valid SPI values are tested here.
BOOL CheckDeviceInfoInvalidInputs(DWORD dwInputSpi);

// This function prints the output of the Device ID IOCTL.
BOOL PrintDeviceIDOutput(VOID);

// This function verifies invalid outbound parameters for DeviceID IOCTL.
// This DeviceID test does not use the common code because the error codes
// for this IOCTL are not consistent with the rest of them. This IOCTL will
// be deprecated so no changes will be made to it.
BOOL DevIDCheckIncorrectOutParameters(VOID);

// This function prints the output of the UUID IOCTL.
BOOL PrintUUIDOutput(VOID);

// This function prints the output of the Processor Information IOCTL.
BOOL PrintProcInfoOutput(VOID);

#endif // __DEVICEINFO_TESTS_H__
