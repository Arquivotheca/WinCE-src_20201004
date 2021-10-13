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

#include "tuxIoctls.h"


/***** Base to start the tests at *******/
#define IOCTL_BASE 5000

FUNCTION_TABLE_ENTRY g_lpFTE[] = {


////////////////////// DEVICE INFO IOCTL /////////////////////////////////////
  _T("Tests for IOCTL_HAL_GET_DEVICE_INFO"), 0, 0, 0, NULL,
  
  _T("Usage Message for the tests"), 1, 0, IOCTL_BASE + 0, IoctlsTestUsageMessage,
  _T("Verify output for SPI_GETPLATFORMNAME"), 1, SPI_GETPLATFORMNAME, IOCTL_BASE + 1, testIoctlDevInfoOutputValue,
  _T("Verify output for SPI_GETPROJECTNAME"), 1, SPI_GETPROJECTNAME, IOCTL_BASE + 2, testIoctlDevInfoOutputValue,
  _T("Verify output for SPI_GETBOOTMENAME"), 1, SPI_GETBOOTMENAME, IOCTL_BASE + 3, testIoctlDevInfoOutputValue,
  _T("Verify output for SPI_GETOEMINFO"), 1, SPI_GETOEMINFO, IOCTL_BASE + 4, testIoctlDevInfoOutputValue,

  _T("Verify output for SPI_GETUUID"), 1, SPI_GETUUID, IOCTL_BASE + 6, testIoctlDevInfoOutputValue,
  _T("Verify output for SPI_GETGUIDPATTERN"), 1, SPI_GETGUIDPATTERN, IOCTL_BASE + 7, testIoctlDevInfoOutputValue,
  _T("Verify output for SPI_GETPLATFORMMANUFACTURER"), 1, SPI_GETPLATFORMMANUFACTURER, IOCTL_BASE + 8, testIoctlDevInfoOutputValue,

  _T("Test handling incorrect Inbound parameters"), 1, SPI_GETPLATFORMNAME, IOCTL_BASE + 20, testIoctlDevInfoInParam,
  _T("Test handling incorrect Outbound parameters"), 1, SPI_GETPLATFORMNAME, IOCTL_BASE + 21, testIoctlDevInfoOutParam,
  _T("Test alignment and overflow of Inbound buffer"), 1, SPI_GETPLATFORMNAME, IOCTL_BASE + 22, testIoctlDevInfoInBufferAlignment,
  _T("Test alignment and overflow of Outbound buffer"), 1, SPI_GETPLATFORMNAME, IOCTL_BASE + 23, testIoctlDevInfoOutBufferAlignment,

  _T("Verify output for user specified user parameter info"), 1, 0xffffffff, IOCTL_BASE + 24, testIoctlDevInfoOutputValue,

  //////////////////////// PROCESSOR INFORMATION IOCTL ////////////////////////
  _T("Tests for IOCTL_PROCESSOR INFORMATION"), 0, 0, 0, NULL,

  _T("Usage Message for the tests"), 1, 0, IOCTL_BASE + 100, IoctlsTestUsageMessage,
  _T("Verify Output"), 1, 0, IOCTL_BASE + 101, testIoctlProcInfoOutputValue,
  _T("Test handling incorrect Inbound parameters"), 1, 0, IOCTL_BASE + 102, testIoctlProcInfoInParam,
  _T("Test handling incorrect Outbound parameters"), 1, 0, IOCTL_BASE + 103, testIoctlProcInfoOutParam,
  _T("Test alignment and overflow of Outbound buffer"), 1, 0, IOCTL_BASE + 104, testIoctlProcInfoOutBufferAlignment,


  
  //////////////////////////// ILTIMING IOCTL /////////////////////////////////
  _T("Tests for IOCTL_HAL_ILTIMING"), 0, 0, 0, NULL,
  
  _T("Usage Message for the tests"), 1, 0, IOCTL_BASE + 200, IltimingIoctlUsageMessage,
  _T("Verify if the Ioctl is supported"), 1, 0, IOCTL_BASE + 201, testIfIltimingIoctlSupported,
  _T("Verify Enabling and Disabling Iltiming multiple times"), 1, 0, IOCTL_BASE + 202, testIltimingEnableDisable,
  _T("Test handling incorrect Inbound parameters"), 1, 0, IOCTL_BASE + 203, testIltimingIoctlInParam,
  _T("Test handling incorrect Outbound parameters"), 1, 0, IOCTL_BASE + 204, testIltimingIoctlOutParam,
  
  //////////////////////////// IOCTL_HAL_GET_RANDOM_SEED IOCTL /////////////////////////////////
  _T("Tests for IOCTL_HAL_GET_RANDOM_SEED"), 0, 0, 0, NULL,

  _T("Test handling incorrect Inbound parameters"), 1, 0, IOCTL_BASE + 300, testRandomSeedIoctlInParam,
  _T("Test handling incorrect Outbound parameters"), 1, 0, IOCTL_BASE + 301, testRandomSeedIoctlOutParam,
  _T("Verify if the Ioctl is supported and returns different Seeds everytime"), 1, 0, IOCTL_BASE + 302, testRandomSeedIoctlSupported,

  //////////////////////////// IOCTL_HAL_GET_WAKE_SOURCE IOCTL /////////////////////////////////
  _T("Tests for IOCTL_HAL_GET_WAKE_SOURCE"), 0, 0, 0, NULL,
  _T("Test that IOCTL_HAL_GET_WAKE_SOURCE returns a valid wake source"), 1, 0, IOCTL_BASE + 500, testGetWakeSource,
  _T("Test that input buffers are ignored for IOCTL_HAL_GET_WAKE_SOURCE"), 1, 0, IOCTL_BASE + 501, testGetWakeSourceInParam,
  _T("Test IOCTL_HAL_GET_WAKE_SOURCE buffers fail at appropriate times"), 1, 0, IOCTL_BASE + 502, testGetWakeSourceOutParam,

  //////////////////////////// IOCTL_HAL_GET_POWER_DISPOSITION IOCTL /////////////////////////////////
  _T("Tests for IOCTL_HAL_GET_POWER_DISPOSITION"), 0, 0, 0, NULL,
  _T("Test that IOCTL_HAL_GET_POWER_DISPOSITION returns a valid power disposition"), 1, 0, IOCTL_BASE + 600, testGetPowerDisposition,
  _T("Test that input buffers are ignored for IOCTL_HAL_GET_POWER_DISPOSITION"), 1, 0, IOCTL_BASE + 601, testGetPowerDispositionInParam,
  _T("Test IOCTL_HAL_GET_POWER_DISPOSITION buffers fail at appropriate times"), 1, 0, IOCTL_BASE + 602, testGetPowerDispositionOutParam,
  
  NULL, 0, 0, 0, NULL  // marks end of list
};
