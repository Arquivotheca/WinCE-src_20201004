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

/*
 * tuxFunctionTable.cpp
 */

/*
 * pull in the function defs for the iltiming tests
 */
 
#include "tuxIltiming.h"


FUNCTION_TABLE_ENTRY g_lpFTE[] = {

  _T(  "Usage message for the IOCTL_HAL_ILTIMING tests"), 0, 0, 1, UsageMessage,

  _T(  "Verify if IOCTL_HAL_ILTIMING is supported"), 0, 0, 1000, testIfIltimingIoctlSupported,

  _T(  "Verify Enabling and Disabling Iltiming multiple times"), 0, 0, 1001, testEnableDisableSequence,

  _T(  "Test incorrect inbound/outbound parameters"), 0, 0, 1002, testIltimingInOutParam,

   NULL, 0, 0, 0, NULL  // marks end of list
};
