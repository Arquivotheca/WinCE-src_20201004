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
// Keyboard Peripheral and technology detector
//
//

#include "Keyboard_Detector.h"

#define __BIN_NAME__ TEXT("Keyboard_Detector.dll")

CKato *g_pKato = NULL;
SPS_SHELL_INFO *g_pShellInfo;

TESTPROCAPI KeyboardDetectorTest(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

//--------------------------------------------------------------------
// TUX Function table
//

extern "C" {
    FUNCTION_TABLE_ENTRY g_lpFTE[] = {
        {TEXT("Keyboard Peripheral and Technology Detector"), 0, 0, BVT_BASE + 0,
             KeyboardDetectorTest},
        {NULL, 0, 0, 0, NULL}
    };
}


//
// IsTechPresent
//
// Does the actual Peripheral and Technology detection
//
// Called By:
//      KeyboardDetectorTest
//
// Return Values:
//      TRUE    Tech was detected
//      FALSE   Tech is not detected
//

BOOL IsTechPresent(void)
{
    BOOL bFlag = FALSE;

    GUID keyboardGUID = DEVCLASS_KEYBOARD_GUID;
    DEVMGR_DEVICE_INFORMATION ddiKeyboard;

    memset(&ddiKeyboard, 0, sizeof(DEVMGR_DEVICE_INFORMATION));
    ddiKeyboard.dwSize = sizeof(DEVMGR_DEVICE_INFORMATION);

    HANDLE hDevice = FindFirstDevice(DeviceSearchByGuid, &keyboardGUID, &ddiKeyboard);

    if (hDevice == INVALID_HANDLE_VALUE)
    {
        bFlag = FALSE;
    }
    else
    {
        bFlag = TRUE;
    }

    if (bFlag == TRUE)
        DETAIL("Successfully detected Keyboard");
    else
        WARN("Did not detect Keyboard");

    return bFlag;
}

//
// KeyboardDetectorTest
//
// This test is called by TUX directly and so must handle the standard TUX messages.
//
// Returns:
//      TPR_PASS    if tech was detected
//      TPR_FAIL    if tech was not detected
//

TESTPROCAPI KeyboardDetectorTest(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    UINT uiStatus = TPR_FAIL;

    // skip any call to us that is not a request to execute
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    DETAIL("Detecting ...");
    if(IsTechPresent())
        uiStatus = TPR_PASS;

    return uiStatus;
}

//
// ShellProc
//
// Standard simplified shellproc for supporting calls from TUX
//

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam)
{
    switch (uMsg)
    {
        case SPM_LOAD_DLL:
        {
            ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
            // Get/Create our global logging object.
            g_pKato = (CKato*)KatoGetDefaultObject();
            return SPR_HANDLED;
        }

        case SPM_SHELL_INFO:
        {
            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
            if( g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine )
            {
                // Display our Dlls command line if we have one.
                g_pKato->Log( LOG_DETAIL, _T("Command Line: \"%s\"."), g_pShellInfo->szDllCmdLine);
            } 
            // get the global pointer to our HINSTANCE
            //g_hInstance = g_pShellInfo->hLib;
            return SPR_HANDLED;
        }

        case SPM_REGISTER:
        {
            ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
            return SPR_HANDLED | SPF_UNICODE;
        }

        case SPM_UNLOAD_DLL:
        case SPM_START_SCRIPT:
        case SPM_STOP_SCRIPT:
            return SPR_HANDLED;

        case SPM_BEGIN_GROUP:
        {
            g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: %s"), __BIN_NAME__);
            return SPR_HANDLED;
        }

        case SPM_END_GROUP:
        {
           g_pKato->EndLevel(TEXT("END GROUP: %s"), __BIN_NAME__);
           return SPR_HANDLED;
        }

        case SPM_BEGIN_TEST:
        {
           // Start our logging level.
            LPSPS_BEGIN_TEST pBT = (LPSPS_BEGIN_TEST)spParam;
            g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID,
                TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                pBT->dwRandomSeed);
           return SPR_HANDLED;
        }

        case SPM_END_TEST:
        {
            // End our logging level.
            LPSPS_END_TEST pET = (LPSPS_END_TEST)spParam;
            g_pKato->EndLevel(TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
                pET->lpFTE->lpDescription,
                pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
                pET->dwResult == TPR_PASS ? TEXT("PASSED") :
                pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
                pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);
            return SPR_HANDLED;
        }

        case SPM_EXCEPTION:
        {
            g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred in %s!"), __WFILE__);
            return SPR_HANDLED;
        }
    }
    return SPR_NOT_HANDLED;
}