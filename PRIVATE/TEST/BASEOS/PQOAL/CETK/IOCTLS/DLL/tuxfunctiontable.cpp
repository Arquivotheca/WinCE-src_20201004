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
 * pull in function defs for the Ioctl tests
 */

#include "..\code\tuxIoctls.h"


/***** Base to start the tests at *******/
#define IOCTL_BASE 5000

FUNCTION_TABLE_ENTRY g_lpFTE[] = {

  _T("IOCTL Tests Usage Message"), 0, 0, 1, IoctlsTestUsageMessage,


////////////////////// DEVICE INFO IOCTL /////////////////////////////////////
  _T("Tests for IOCTL_HAL_GET_DEVICE_INFO"), 0, 0, 0, NULL,
  
  _T("Tests for IOCTL_HAL_GET_DEVICE_INFO :: SPI_GETPLATFORMNAME"), 1, 0, 0, NULL,
  _T("Test Device Info - Platform Name - Verify Output value"), 2, SPI_GETPLATFORMNAME, IOCTL_BASE + 0, testIoctlDevInfoOutputValue,
  _T("Test Device Info - Platform Name - Check incorrect Inbound parameters"), 2, SPI_GETPLATFORMNAME, IOCTL_BASE + 1, testIoctlDevInfoInParam,
  _T("Test Device Info - Platform Name - Check incorrect Outbound parameters"), 2, SPI_GETPLATFORMNAME, IOCTL_BASE + 2, testIoctlDevInfoOutParam,
  _T("Test Device Info - Platform Name - Check alignment and overflow of Inbound buffer"), 2, SPI_GETPLATFORMNAME, IOCTL_BASE + 3, testIoctlDevInfoInBufferAlignment,
  _T("Test Device Info - Platform Name - Check alignment and overflow of Outbound buffer"), 2, SPI_GETPLATFORMNAME, IOCTL_BASE + 4, testIoctlDevInfoOutBufferAlignment,

  _T("Tests for IOCTL_HAL_GET_DEVICE_INFO :: SPI_GETPROJECTNAME"), 1, 0, 0, NULL,
  _T("Test Device Info - Project Name - Verify Output value"), 2, SPI_GETPROJECTNAME, IOCTL_BASE + 10, testIoctlDevInfoOutputValue,
  _T("Test Device Info - Project Name - Check incorrect Inbound parameters"), 2, SPI_GETPROJECTNAME, IOCTL_BASE + 11, testIoctlDevInfoInParam,
  _T("Test Device Info - Project Name - Check incorrect Outbound parameters"), 2, SPI_GETPROJECTNAME, IOCTL_BASE + 12, testIoctlDevInfoOutParam,
  _T("Test Device Info - Project Name - Check alignment and overflow of Inbound buffer"), 2, SPI_GETPROJECTNAME, IOCTL_BASE + 13, testIoctlDevInfoInBufferAlignment,
  _T("Test Device Info - Project Name - Check alignment and overflow of Outbound buffer"), 2, SPI_GETPROJECTNAME, IOCTL_BASE + 14, testIoctlDevInfoOutBufferAlignment,

  _T("Tests for IOCTL_HAL_GET_DEVICE_INFO :: SPI_GETPLATFORMTYPE"), 1, 0, 0, NULL,
  _T("Test Device Info - Platform Type - Verify Output value"), 2, SPI_GETPLATFORMTYPE, IOCTL_BASE + 20, testIoctlDevInfoOutputValue,
  _T("Test Device Info - Platform Type - Check incorrect Inbound parameters"), 2, SPI_GETPLATFORMTYPE, IOCTL_BASE + 21, testIoctlDevInfoInParam,
  _T("Test Device Info - Platform Type - Check incorrect Outbound parameters"), 2, SPI_GETPLATFORMTYPE, IOCTL_BASE + 22, testIoctlDevInfoOutParam,
  _T("Test Device Info - Platform Type - Check alignment and overflow of Inbound buffer"), 2, SPI_GETPLATFORMTYPE, IOCTL_BASE + 23, testIoctlDevInfoInBufferAlignment,
  _T("Test Device Info - Platform Type - Check alignment and overflow of Outbound buffer"), 2, SPI_GETPLATFORMTYPE, IOCTL_BASE + 24, testIoctlDevInfoOutBufferAlignment,

  _T("Tests for IOCTL_HAL_GET_DEVICE_INFO :: SPI_GETBOOTMENAME"), 1, 0, 0, NULL,
  _T("Test Device Info - BootMe Name - Verify Output value"), 2, SPI_GETBOOTMENAME, IOCTL_BASE + 30, testIoctlDevInfoOutputValue,
  _T("Test Device Info - BootMe Name - Check incorrect Inbound parameters"), 2, SPI_GETBOOTMENAME, IOCTL_BASE + 31, testIoctlDevInfoInParam,
  _T("Test Device Info - BootMe Name - Check incorrect Outbound parameters"), 2, SPI_GETBOOTMENAME, IOCTL_BASE + 32, testIoctlDevInfoOutParam,
  _T("Test Device Info - BootMe Name - Check alignment and overflow of Inbound buffer"), 2, SPI_GETBOOTMENAME, IOCTL_BASE + 33, testIoctlDevInfoInBufferAlignment,
  _T("Test Device Info - BootMe Name - Check alignment and overflow of Outbound buffer"), 2, SPI_GETBOOTMENAME, IOCTL_BASE + 34, testIoctlDevInfoOutBufferAlignment,

  _T("Tests for IOCTL_HAL_GET_DEVICE_INFO :: SPI_GETOEMINFO"), 1, 0, 0, NULL,
  _T("Test Device Info - OEM Info - Verify Output value"), 2, SPI_GETOEMINFO, IOCTL_BASE + 40, testIoctlDevInfoOutputValue,
  _T("Test Device Info - OEM Info - Check incorrect Inbound parameters"), 2, SPI_GETOEMINFO, IOCTL_BASE + 41, testIoctlDevInfoInParam,
  _T("Test Device Info - OEM Info - Check incorrect Outbound parameters"), 2, SPI_GETOEMINFO, IOCTL_BASE + 42, testIoctlDevInfoOutParam,
  _T("Test Device Info - OEM Info - Check alignment and overflow of Inbound buffer"), 2, SPI_GETOEMINFO, IOCTL_BASE + 43, testIoctlDevInfoInBufferAlignment,
  _T("Test Device Info - OEM Info - Check alignment and overflow of Outbound buffer"), 2, SPI_GETOEMINFO, IOCTL_BASE + 44, testIoctlDevInfoOutBufferAlignment,

  _T("Tests for IOCTL_HAL_GET_DEVICE_INFO :: SPI_GETPLATFORMVERSION"), 1, 0, 0, NULL,
  _T("Test Device Info - Platform Version - Verify Output Value"), 2, SPI_GETPLATFORMVERSION, IOCTL_BASE + 50, testIoctlDevInfoOutputValue,
  _T("Test Device Info - Platform Version - Check incorrect Inbound parameters"), 2, SPI_GETPLATFORMVERSION, IOCTL_BASE + 51, testIoctlDevInfoInParam,
  _T("Test Device Info - Platform Version - Check incorrect Outbound parameters"), 2, SPI_GETPLATFORMVERSION, IOCTL_BASE + 52, testIoctlDevInfoOutParam,
  _T("Test Device Info - Platform Version - Check aligned Inbound parameters"), 2, SPI_GETPLATFORMVERSION, IOCTL_BASE + 53, testIoctlDevInfoInBufferAlignment,
  _T("Test Device Info - Platform Version - Check aligned Outbound parameters"), 2, SPI_GETPLATFORMVERSION, IOCTL_BASE + 54, testIoctlDevInfoOutBufferAlignment,

  _T("Tests for IOCTL_HAL_GET_DEVICE_INFO :: SPI_GETUUID"), 1, 0, 0, NULL,
  _T("Test Device Info - UUID - Verify Output value"), 2, SPI_GETUUID, IOCTL_BASE + 60, testIoctlDevInfoOutputValue,
  _T("Test Device Info - UUID - Check incorrect Inbound parameters"), 2, SPI_GETUUID, IOCTL_BASE + 61, testIoctlDevInfoInParam,
  _T("Test Device Info - UUID - Check incorrect Outbound parameters"), 2, SPI_GETUUID, IOCTL_BASE + 62, testIoctlDevInfoOutParam,
  _T("Test Device Info - UUID - Check alignment and overflow of Inbound buffer"), 2, SPI_GETUUID, IOCTL_BASE + 63, testIoctlDevInfoInBufferAlignment,
  _T("Test Device Info - UUID - Check alignment and overflow of Outbound buffer"), 2, SPI_GETUUID, IOCTL_BASE + 64, testIoctlDevInfoOutBufferAlignment,

  _T("Tests for IOCTL_HAL_GET_DEVICE_INFO :: SPI_GETGUIDPATTERN"), 1, 0, 0, NULL,
  _T("Test Device Info - GUID Pattern - Verify Output value"), 2, SPI_GETGUIDPATTERN, IOCTL_BASE + 70, testIoctlDevInfoOutputValue,
  _T("Test Device Info - GUID Pattern - Check incorrect Inbound parameters"), 2, SPI_GETGUIDPATTERN, IOCTL_BASE + 71, testIoctlDevInfoInParam,
  _T("Test Device Info - GUID Pattern - Check incorrect Outbound parameters"), 2, SPI_GETGUIDPATTERN, IOCTL_BASE + 72, testIoctlDevInfoOutParam,
  _T("Test Device Info - GUID Pattern - Check alignment and overflow of Inbound buffer"), 2, SPI_GETGUIDPATTERN, IOCTL_BASE + 73, testIoctlDevInfoInBufferAlignment,
  _T("Test Device Info - GUID Pattern - Check alignment and overflow of Outbound buffer"), 2, SPI_GETGUIDPATTERN, IOCTL_BASE + 74, testIoctlDevInfoOutBufferAlignment,

  
////////////////////// DEVICE ID IOCTL ////////////////////////////////////////
  _T("Tests for IOCTL_HAL_GET_DEVICEID"), 0, 0, 0, NULL,
  _T("Test Deprecated Device ID ioctl - Verify Output value"), 1, 0, IOCTL_BASE + 200, testIoctlDevIDOutputValue,
  _T("Test Deprecated Device ID ioctl - Check incorrect Inbound parameters"), 1, 0, IOCTL_BASE + 201, testIoctlDevIDInParam,
  _T("Test Deprecated Device ID ioctl - Check incorrect Outbound parameters"), 1, 0, IOCTL_BASE + 202, testIoctlDevIDOutParam,
  _T("Test Deprecated Device ID ioctl - Check alignment and Overflow of Outbound buffer"), 1, 0, IOCTL_BASE + 203, testIoctlDevIDOutBufferAlignment,


/////////////////////////// UUID ID IOCTL //////////////////////////////////////
  _T("Tests for IOCTL_HAL_GET_UUID"), 0, 0, 0, NULL,
  _T("Test Deprecated UUID ioctl - Verify Output value"), 1, 0, IOCTL_BASE + 300, testIoctlUUIDOutputValue,
  _T("Test Deprecated UUID ioctl - Check incorrect Inbound parameters"), 1, 0, IOCTL_BASE + 301, testIoctlUUIDInParam,
  _T("Test Deprecated UUID ioctl - Check incorrect Outbound parameters"), 1, 0, IOCTL_BASE + 302, testIoctlUUIDOutParam,
  _T("Test Deprecated UUID ioctl - Check alignment and Overflow of Outbound buffer"), 1, 0, IOCTL_BASE + 303, testIoctlUUIDOutBufferAlignment,


  //////////////////////// PROCESSOR INFORMATION IOCTL ////////////////////////
  _T("Tests for IOCTL_PROCESSOR INFORMATION"), 0, 0, 0, NULL,
  _T("Test Processor Information ioctl - Verify Output value"), 1, 0, IOCTL_BASE + 400, testIoctlProcInfoOutputValue,
  _T("Test Processor Information ioctl - Check incorrect Inbound parameters"), 1, 0, IOCTL_BASE + 401, testIoctlProcInfoInParam,
  _T("Test Processor Information ioctl - Check incorrect Outbound parameters"), 1, 0, IOCTL_BASE + 402, testIoctlProcInfoOutParam,
  _T("Test Processor Information ioctl - Check alignment and Overflow of Outbound buffer"), 1, 0, IOCTL_BASE + 403, testIoctlProcInfoOutBufferAlignment,
  

  NULL, 0, 0, 0, NULL  // marks end of list
};


