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
//  Copyright (c) Microsoft Corporation
//
//  Module: WMFunctional_Tests_No_Media.cpp
//          Contains Camera Driver Conformance tests
//        CreateFile(...)
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////
#include <Windows.h>
#include "logging.h"
#include "Globals.h"

TESTPROCAPI CameraDetect( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    DEVMGR_DEVICE_INFORMATION di;
    GUID DriverGuid = DEVCLASS_CAMERA_GUID;

    memset(&di, 0, sizeof(DEVMGR_DEVICE_INFORMATION));
    di.dwSize = sizeof(DEVMGR_DEVICE_INFORMATION);

    HANDLE hDevice = FindFirstDevice(DeviceSearchByGuid, &DriverGuid, &di);

    if(INVALID_HANDLE_VALUE == hDevice)
    {
        FAIL(TEXT("No camera drivers of this type were found in the system."));
    }
    else
    {
        Log(TEXT("Camera driver found."));
        FindClose(hDevice);
    }

    return GetTestResult();
}

