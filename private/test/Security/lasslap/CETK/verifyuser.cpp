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
#include "laptest.h"

TESTPROCAPI VerifyUserTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD dwRetVal = TPR_FAIL;
    LONG lRet = 0;
    HKEY hKey = NULL;
    TCHAR szActiveLap[MAX_PATH];
    DWORD cbActiveLap = MAX_PATH * sizeof(TCHAR);
    typedef BOOL (*MyInitLAP) (InitLap*);
    typedef BOOL (*MyDeinitLAP) ();
    typedef BOOL (*MyVerifyUser) (GUID*, LPCWSTR, HWND, DWORD, PVOID);
    MyInitLAP InitLapProc;
    MyDeinitLAP DeinitLapProc;
    MyVerifyUser VerifyUserProc;
    BOOL fProcRet = FALSE;
    InitLap hInit;
    GUID aeKey = AE_EVERY_TIME;

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
    case VUS_VUS:
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
    VerifyUserProc = (MyVerifyUser) GetProcAddress(hLapLib, TEXT("VerifyUser"));

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

    if (VerifyUserProc == NULL)
    {
        LOG(TEXT("Function VerifyUser could not be found in current LAP"));
        goto Error;
    }
   
    hInit.size = sizeof(InitLap);
    hInit.capabilities =0;
 
    fProcRet = InitLapProc(&hInit);
    LOG(TEXT("Call to InitLAP %s"), fProcRet ? TEXT("PASSED") : TEXT("FAILED"));

    fProcRet = VerifyUserProc( &aeKey, TEXT("LAP TEST"), NULL, VU_UNTIL_SUCCESS, 0);
    LOG(TEXT("VerufyUser returned %s"), fProcRet ? TEXT("TRUE") : TEXT("FALSE"));

    DeinitLapProc();
    LOG(TEXT("Called DeinitLAP"));

    // If VerifyUser fails, report FAILURE
    if (fProcRet == FALSE)
        goto Error;
      
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

