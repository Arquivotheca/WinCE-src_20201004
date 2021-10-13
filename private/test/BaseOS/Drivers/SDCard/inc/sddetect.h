//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 BSQUARE Corporation. All rights reserved.
// Copyright (c) Microsoft Corporation.  All rights reserved.//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------
//
//
// SD Detector Header File
//
//

#ifndef SDDETECT_H
#define SDDETECT_H

#include <sdcardddk.h>
#include <winbase.h>
#include <auto_xxx.hxx>
#include <ceddk.h>
#include <cebus.h>

#define  SDDRV_REG_PATH               _T("Drivers\\BuiltIn\\SdBusDriver")
#define  SDHOST_ELLEN_REG_PATH        _T("Drivers\\BuiltIn\\Pci\\Instance\\Ellen1")
#define  SDHOST_ELLEN_STD_REG_PATH    _T("HKEY_LOCAL_MACHINE\\Drivers\\BuiltIn\\PCI\\Instance\\SDIOStandardHC1")
#define  SDHOST_ELLEN_STD2_REG_PATH   _T("HKEY_LOCAL_MACHINE\\Drivers\\BuiltIn\\PCI\\Instance\\SDIOStandardHCWithDMA1")
#define  SDHOST_REG_PATH              _T("Drivers\\BuiltIn\\SDHC")

#define SDDRV_PREFIX                  _T("SDC")

BOOL UnloadingDriver(LPCTSTR pszSearchPrefix, DEVMGR_DEVICE_INFORMATION& di);

BOOL LoadingDriver(DEVMGR_DEVICE_INFORMATION& di);

BOOL UnloadSDBus(DEVMGR_DEVICE_INFORMATION& di);

DWORD NumOfSDHostController();

BOOL UnloadSDHostController(DEVMGR_DEVICE_INFORMATION& di, DWORD dwHostIndex);

VOID displayAllSlot();

BOOL hasSDMemory();

BOOL hasSDHCMemory();

BOOL hasMMC();

BOOL hasMMCHC();

BOOL hasSDIO();

// Return TRUE and output Slot and Host Index for the input device type
// Return FALSE if there is no such device in the device.
BOOL getFirstDeviceSlotInfo(SDCARD_DEVICE_TYPE devType, BOOL bHighCap, DWORD& dwSlotIndex, DWORD& dwHostIndex);

#endif //SDDETECT_H

