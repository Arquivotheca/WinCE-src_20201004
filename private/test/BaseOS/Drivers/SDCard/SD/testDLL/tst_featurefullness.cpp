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
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 BSQUARE Corporation. All rights reserved.
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
// SD Feature Fullness Test Group is responsible for performing the Host & Card
// Feature Fullness Tests. See the ft.h file for more detail.
//
// Function Table Entries are:
//  TestHostInterfaceFF   Tests querying the host controller for its interface mode and clock rate
//  TestCardInterfaceFF   Tests querying an SD card for its interface mode and clock rate
//

#include "main.h"
#include "globals.h"
#include <sdcard.h>

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests: TestHostInterfaceFF
// Tests querying the platform host controller for its physical nature, i.e.,
// interface mode (4-bit/1-bit) and maximum clock rate.
//
// Parameters:
//  uMsg        Message code.
//  tpParam     Additional message-dependent data.
//  lpFTE   
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
//

TESTPROCAPI TestHostInterfaceFF(UINT uMsg, TPPARAM, LPFUNCTION_TABLE_ENTRY)
{
    NO_MESSAGES;

    DWORD   length;
    LPCTSTR ioctlName = NULL;
    BOOL bRet = TRUE;

    g_pKato->Comment(0, TEXT("The bulk of the following test is contained in the test driver: \"SDTst.dll\"."));
    g_pKato->Comment(0, TEXT("The final results will be logged to Kato, but for more information and step by step descriptions it is best if you have a debugger connected."));

    if (g_hTstDriver == INVALID_HANDLE_VALUE)
    {
        g_pKato->Comment(0, TEXT("There is no client test driver, test will not run!"));
        return TPR_ABORT;
    }

    SD_CARD_INTERFACE ci;
    bRet = DeviceIoControl(g_hTstDriver, IOCTL_GET_HOST_INTERFACE, NULL, 0, &ci, sizeof(SD_CARD_INTERFACE), &length, NULL);
    if (bRet)
    {
        //succeeded - display current rate
        g_pKato->Log(LOG_DETAIL, TEXT("The SD host controller supports %s mode at a maximum clock rate of %d\n"),
            ci.InterfaceMode == SD_INTERFACE_SD_4BIT ? _T("4-bit") : _T("1-bit"),
            ci.ClockRate);

        return TPR_PASS;
    }
    else
    {
        // Diagnose why the driver failed to run the requested test
        if (GetLastError() == ERROR_INVALID_PARAMETER)
        {
            g_pKato->Log(TPR_FAIL, TEXT("You have passed in either bad test params, or an invalid size of those params"));
        }
        else if (GetLastError() == ERROR_INVALID_HANDLE)
        {
            g_pKato->Log(LOG_ABORT, TEXT("The Client Driver has unloaded, perhaps the disk has been ejected. Test Aborted."));

            return TPR_ABORT;
        }
        else
        {
            ioctlName = TranslateIOCTLS(IOCTL_GET_HOST_INTERFACE);
            if (ioctlName != NULL)
            {
                g_pKato->Comment(0, TEXT("Can't find the IOCTL: 0x%x (%s) in the driver"), IOCTL_GET_HOST_INTERFACE, ioctlName);
            }
            else
            {
                g_pKato->Log(LOG_FAIL, TEXT("You are calling an IOCTL that does not exist in either the driver or the test DLL"));
            }
        }
    }

    return TPR_FAIL;
}


//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests: TestCardInterfaceFF
// Tests querying for the the physical nature of an SD card, i.e., interface mode
// and clock rate.
//
// Parameters:
//  uMsg        Message code.
//  tpParam     Additional message-dependent data.
//  lpFTE   
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
//

TESTPROCAPI TestCardInterfaceFF(UINT uMsg, TPPARAM, LPFUNCTION_TABLE_ENTRY)
{
    NO_MESSAGES;

    DWORD   length;
    LPCTSTR ioctlName = NULL;
    BOOL bRet = TRUE;

    g_pKato->Comment(0, TEXT("The bulk of the following test is contained in the test driver: \"SDTst.dll\"."));
    g_pKato->Comment(0, TEXT("The final results will be logged to Kato, but for more information and step by step descriptions it is best if you have a debugger connected."));

    if (g_hTstDriver == INVALID_HANDLE_VALUE)
    {
        g_pKato->Comment(0, TEXT("There is no client test driver, test will not run!"));

        return TPR_ABORT;
    }

    SD_CARD_INTERFACE ci;
    bRet = DeviceIoControl(g_hTstDriver, IOCTL_GET_CARD_INTERFACE, NULL, 0, &ci, sizeof(SD_CARD_INTERFACE), &length, NULL);
    if (bRet)
    {
        //succeeded, display the current rate
        g_pKato->Log(LOG_DETAIL, TEXT("The SD card supports %s mode at a maximum clock rate of %d\n"),
            ci.InterfaceMode == SD_INTERFACE_SD_4BIT ? _T("4-bit") : _T("1-bit"),
            ci.ClockRate);

        return TPR_PASS;
    }
    else
    {
        // Diagnose why the driver failed to run the requested test
        if (GetLastError() == ERROR_INVALID_PARAMETER)
        {
            g_pKato->Log(TPR_FAIL, TEXT("You have passed in either bad test params, or an invalid size of those params"));
        }
        else if (GetLastError() == ERROR_INVALID_HANDLE)
        {
            g_pKato->Log(LOG_ABORT, TEXT("The Client Driver has unloaded, perhaps the disk has been ejected. Test Aborted."));

            return TPR_ABORT;
        }
        else
        {
            ioctlName = TranslateIOCTLS(IOCTL_GET_CARD_INTERFACE);
            if (ioctlName != NULL)
            {
                g_pKato->Comment(0, TEXT("Can't find the IOCTL: 0x%x (%s) in the driver"), IOCTL_GET_HOST_INTERFACE, ioctlName);
            }
            else
            {
                g_pKato->Log(LOG_FAIL, TEXT("You are calling an IOCTL that does not exist in either the driver or the test DLL"));
            }
        }
    }

    return TPR_FAIL;
}
