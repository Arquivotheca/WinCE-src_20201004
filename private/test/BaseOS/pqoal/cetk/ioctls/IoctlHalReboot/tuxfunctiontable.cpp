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
 * pull in function defs for the Ioctl tests
 */

#include "tuxIoctlReboot.h"


FUNCTION_TABLE_ENTRY g_lpFTE[] = {


  //////////////////////////// IOCTL HAL REBOOT ///////////////////////////////
  _T("Tests for IOCTL_HAL_REBOOT"), 0, 0, 0, NULL,
  
  _T("Usage Message for the tests"), 1, 0, 1000, IoctlHalRebootUsageMessage,
  _T("Verify if Ioctl is working correctly"), 1, 0, 1001, testIoctlHalReboot,



  NULL, 0, 0, 0, NULL  // marks end of list
};
