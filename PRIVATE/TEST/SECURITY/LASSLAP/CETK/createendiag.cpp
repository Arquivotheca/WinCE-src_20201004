//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#include "laptest.h"

TESTPROCAPI CreateEnDiagTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD dwRetVal = TPR_FAIL;
    LONG lRet = 0;
    HKEY hKey = NULL;
    TCHAR szActiveLap[MAX_PATH];
    DWORD cbActiveLap = MAX_PATH * sizeof(TCHAR);
    typedef BOOL (*MyInitLAP) (InitLap*);
    typedef BOOL (*MyDeinitLAP) ();
    typedef BOOL (*MyCreateEnrollmentConfigDialog) (HWND);
    MyInitLAP InitLapProc;
    MyDeinitLAP DeinitLapProc;
    MyCreateEnrollmentConfigDialog CreateEnrollmentConfigDialogProc;
    BOOL fProcRet = FALSE;
    InitLap hInit;

    // process incoming tux messages
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // return the thread count that should be used to run this test
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return TPR_HANDLED;
    }
    else if( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;
    

    // Test case specific setup here
    switch(lpFTE->dwUserData)
    {
    case INI_INI:
        break;
        
    default:
        g_pKato->Log(LOG_FAIL, TEXT("Unknown Test Case %d (Should never be here)\n"), lpFTE->dwUserData);
        goto Error;
    }
    

    lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, HKEY_LAP_ROOT, 0, 0, &hKey);
    if (lRet != ERROR_SUCCESS)
    {
        LOG(TEXT("Can not open Key %s"), HKEY_LAP_ROOT);
        goto Error;
    }

    // Now find the active LAP
    lRet = RegQueryValueEx(hKey, ACTIVE_LAP_NAME, NULL, NULL, (BYTE*)szActiveLap, &cbActiveLap);
    if (lRet != ERROR_SUCCESS)
    {
        LOG(TEXT("Could not detect Active Lap"));
        goto Error;
    }
    LOG(TEXT("Active Lap: %s"), szActiveLap);

    HMODULE hLapLib = NULL;
    TCHAR szLapDllName[MAX_PATH];
    _tcscpy (szLapDllName, RELEASE_ROOT);
    _tcsncat (szLapDllName, szActiveLap, MAX_PATH - _tcslen(szLapDllName));
    _tcsncat (szLapDllName, TEXT(".dll"), MAX_PATH - _tcslen(szLapDllName));
    LOG(TEXT("Loading %s"), szLapDllName);
    hLapLib = LoadLibrary(szLapDllName);

    if (hLapLib == NULL)
    {
        LOG(TEXT("Could not load LAP dll %s: 0x%08x"), szLapDllName, GetLastError());
        goto Error;
    }

    InitLapProc = (MyInitLAP) GetProcAddress(hLapLib, TEXT("InitLAP"));
    DeinitLapProc = (MyDeinitLAP) GetProcAddress(hLapLib, TEXT("DeinitLAP"));
    CreateEnrollmentConfigDialogProc = (MyCreateEnrollmentConfigDialog) GetProcAddress(hLapLib, TEXT("CreateEnrollmentConfigDialog"));

    if (InitLapProc == NULL)
    {
        LOG(TEXT("Function InitLap could not be found in current LAP"));
        goto Error;
    }
    if (DeinitLapProc == NULL)
    {
        LOG(TEXT("Function DeinitLap could not be found in current LAP"));
        goto Error;
    }
    if (CreateEnrollmentConfigDialogProc == NULL)
    {
        LOG(TEXT("Function CreateEnrollmentConfigDialog could not be found in current LAP"));
        goto Error;
    }
   
    fProcRet = InitLapProc(&hInit);
    LOG(TEXT("Call to InitLAP %s"), fProcRet ? TEXT("PASSED") : TEXT("FAILED"));

    fProcRet = CreateEnrollmentConfigDialogProc(NULL);
    LOG(TEXT("CreateEnrollmentConfigDialog returned %s"), fProcRet ? TEXT("TRUE") : TEXT("FALSE"));

    DeinitLapProc();
    LOG(TEXT("Called DeinitLAP"));

      
    //
    //  test passed
    //
    dwRetVal = TPR_PASS;
    
Error:
    // cleanup here
    if (hKey)
        RegCloseKey(hKey);
    if (hLapLib)
        FreeLibrary(hLapLib);
    return dwRetVal;
}

