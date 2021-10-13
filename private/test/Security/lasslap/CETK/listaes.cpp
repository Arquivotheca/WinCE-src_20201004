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

TESTPROCAPI ListAEsTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD dwRetVal = TPR_FAIL;
    LONG lRet = 0;
    HKEY hKey = NULL;
    int i = 0;
    TCHAR szKeyName[MAX_PATH];
    DWORD cbKeyName = MAX_PATH * sizeof(TCHAR);
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
    case LSTA_ALL:
        break;
        
    
    default:
        g_pKato->Log(LOG_FAIL, TEXT("Unknown Test Case %d (Should never be here)\n"), lpFTE->dwUserData);
        goto Error;
    }
    

    lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, HKEY_AE_ROOT, 0, 0, &hKey);
    if (lRet != ERROR_SUCCESS)
    {
        LOG(TEXT("Can not open Key %s"), HKEY_AE_ROOT);
        goto Error;
    }

    // Enumerate all installed AEs
    lRet = ERROR_SUCCESS;
    while (lRet == ERROR_SUCCESS)
    {
        cbKeyName = MAX_PATH * sizeof(TCHAR);;
        lRet = RegEnumKeyEx(hKey, i, szKeyName, &cbKeyName, 0, 0, 0, &ftLastWriteTime);
        if (lRet == ERROR_SUCCESS)
        {
            LOG(TEXT("Found AE [%i]: %s"), i, szKeyName);
            HKEY hSubKey = NULL;
            LONG lSubRet = 0;
            TCHAR szSubValueName[MAX_PATH];
            DWORD cbSubValueName = MAX_PATH * sizeof(TCHAR);
            TCHAR szSubDataName[MAX_PATH];
            DWORD cbSubDataName = MAX_PATH * sizeof(TCHAR);
            DWORD dwType = 0;
            
            int j = 0;
            lSubRet = RegOpenKeyEx(hKey, szKeyName, 0, 0, &hSubKey);
            // Within each AE, print out all values
            while(1)
            {
                cbSubDataName = MAX_PATH * sizeof(TCHAR);
                cbSubValueName = MAX_PATH * sizeof(TCHAR);
                lSubRet = RegEnumValue(hSubKey, j, szSubValueName, &cbSubValueName, 0, &dwType, (BYTE*)szSubDataName, &cbSubDataName);
                if (lSubRet != ERROR_SUCCESS)
                    break;
                switch (dwType)
                {
                case REG_SZ:
                    LOG(TEXT("\t%s: %s"), szSubValueName, szSubDataName);
                    break;
                case REG_DWORD:
                    LOG(TEXT("\t%s: %d"), szSubValueName, (DWORD)szSubDataName[0]);
                    break;
                default:
                    LOG(TEXT("\t%s: <not type of REG_SZ or REG_DWORD>"), szSubValueName);
                    break;

                }
                j++;                
            }
            RegCloseKey(hSubKey);
        }
        
        i++;
    }

      
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

