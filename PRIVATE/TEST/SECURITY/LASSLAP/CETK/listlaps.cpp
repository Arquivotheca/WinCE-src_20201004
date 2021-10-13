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

TESTPROCAPI ListLapsTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD dwRetVal = TPR_FAIL;
    LONG lRet = 0;
    HKEY hKey = NULL;
    int i = 0;
    TCHAR szKeyName[MAX_PATH];
    DWORD cbKeyName = MAX_PATH * sizeof(TCHAR);
    TCHAR szActiveLap[MAX_PATH];
    DWORD cbActiveLap = MAX_PATH * sizeof(TCHAR);
	FILETIME ftLastWriteTime;

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
    case LSTL_ALL:
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

    // Enumerate all installed LAPs
    lRet = ERROR_SUCCESS;
    while (lRet == ERROR_SUCCESS)
    {
        lRet = RegEnumKeyEx(hKey, i, szKeyName, &cbKeyName, 0, 0, 0, &ftLastWriteTime);
        if (lRet == ERROR_SUCCESS)
            LOG(TEXT("Found LAP [%i]: %s"), i, szKeyName);
        i++;
    }

    LOG(TEXT("%d LAPs Installed"), i-1);

    // Now find the active LAP
    lRet = RegQueryValueEx(hKey, ACTIVE_LAP_NAME, NULL, NULL, (BYTE*)szActiveLap, &cbActiveLap);
    if (lRet != ERROR_SUCCESS)
    {
        LOG(TEXT("Could not detect Active Lap"));
        goto Error;
    }
    LOG(TEXT("Active Lap: %s"), szActiveLap);

      
    //
    //  test passed
    //
    dwRetVal = TPR_PASS;
    
Error:
    // cleanup here
    if (hKey)
        RegCloseKey(hKey);
    return dwRetVal;
}

