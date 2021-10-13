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

////////////////////////////////////////////////////////////////////////////////
// Test_CreateFile
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CreateFile
//
//  Assertion:        
//
//  Description:    1: Test CreateFile when all the parameters are correct.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

#ifndef DEVCLASS_3PT_CAMERA_GUID
#define DEVCLASS_3PT_CAMERA_GUID { 0xcb0f3e02, 0x534f, 0x4826, {0xa0, 0x44, 0xf0, 0x8f, 0x10, 0xe6, 0x44, 0x8d }}
#endif

GUID g_CameraDevclass = DEVCLASS_CAMERA_GUID;
GUID g_CameraPinclass = DEVCLASS_PIN_GUID;
GUID g_3ptDevclass = DEVCLASS_3PT_CAMERA_GUID;


TESTPROCAPI Test_DriverName( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    DEVMGR_DEVICE_INFORMATION di;
    GUID *DriverGuid = (GUID *) lpFTE->dwUserData;

    memset(&di, 0, sizeof(DEVMGR_DEVICE_INFORMATION));
    di.dwSize = sizeof(DEVMGR_DEVICE_INFORMATION);

    if(g_CameraDevclass != *DriverGuid && g_CameraPinclass != *DriverGuid && g_3ptDevclass != *DriverGuid)
    {
        ABORT(TEXT("Invalid guid specified in test function table."));
        return TPR_ABORT;
    }

    HANDLE hDevice = FindFirstDevice(DeviceSearchByGuid, DriverGuid, &di);

    if(INVALID_HANDLE_VALUE == hDevice)
        Log(TEXT("No camera drivers of this type were found in the system."));

    if(INVALID_HANDLE_VALUE != hDevice)
    {
        do
        {
            Log(TEXT("Found device %s"), di.szLegacyName);

            if(g_CameraDevclass == *DriverGuid)
            {
                if(di.szLegacyName[0] != 'C' || di.szLegacyName[1] != 'A'  || di.szLegacyName[2] != 'M')
                {
                    FAIL(TEXT("Invalid legacy name (%s) specified for the DShow camera device."), di.szLegacyName);
                }
            }
            else if(g_CameraPinclass == *DriverGuid)
            {
                if(di.szLegacyName[0] != 'P' || di.szLegacyName[1] != 'I'  || di.szLegacyName[2] != 'N')
                {
                    FAIL(TEXT("Invalid legacy name (%s) specified for the third party camera device."), di.szLegacyName);
                }
            }
            else if(g_3ptDevclass == *DriverGuid)
            {
                if(di.szLegacyName[0] != 'C' || di.szLegacyName[1] != 'A'  || di.szLegacyName[2] != 'P')
                {
                    FAIL(TEXT("Invalid legacy name (%s) specified for the third party camera device."), di.szLegacyName);
                }
            }
        }while(FindNextDevice(hDevice, &di));

        FindClose(hDevice);
    }

    return GetTestResult();
}



