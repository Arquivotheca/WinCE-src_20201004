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
#include "main.h"
#include "globals.h"
#include <utility.h>

#define QUERY_DATA_SIZE    512
#define QUERY_NUM_KEYS    50
#define QUERY_NUM_VALUES    QUERY_NUM_KEYS

LONG L_GetKeyClass(HKEY hKey, __out_ecount(*pcbClass) LPTSTR pszClass, DWORD *pcbClass)
{
    return RegQueryInfoKey(hKey, pszClass, pcbClass, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

LONG L_GetKey_cSubKey(HKEY hKeyNew, DWORD *pcSubKey)
{
    return RegQueryInfoKey(hKeyNew, NULL, NULL, NULL, pcSubKey, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

LONG L_GetKey_MaxSubKeyLen(HKEY hKeyNew, DWORD *pcbMaxSubKey)
{
    return RegQueryInfoKey(hKeyNew, NULL, NULL, NULL, NULL, pcbMaxSubKey, NULL, NULL, NULL, NULL, NULL, NULL);
}

LONG L_GetKey_MaxClassLen(HKEY hKeyNew, DWORD *pcbMaxClass)
{
    return RegQueryInfoKey(hKeyNew, NULL, NULL, NULL, NULL, NULL, pcbMaxClass, NULL, NULL, NULL, NULL, NULL);
}

LONG L_GetKey_cValues(HKEY hKeyNew, DWORD *pcValues)
{
    return RegQueryInfoKey(hKeyNew, NULL, NULL, NULL, NULL, NULL, NULL, pcValues, NULL, NULL, NULL, NULL);
}

LONG L_GetKey_cbMaxValName(HKEY hKeyNew, DWORD *pcbMaxValName)
{
    return RegQueryInfoKey(hKeyNew, NULL, NULL, NULL, NULL, NULL, NULL, NULL, pcbMaxValName, NULL, NULL, NULL);
}

LONG L_GetKey_cbMaxValData(HKEY hKeyNew, DWORD *pcbMaxValData)
{
    return RegQueryInfoKey(hKeyNew, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, pcbMaxValData, NULL, NULL);
}


///////////////////////////////////////////////////////////////////////////////
//
// Test RegQueryInfo
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RegQueryInfo_Class(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0;
    HKEY hKeyNew=0, hKeyTemp=0;
    TCHAR rgszKeyName[MAX_PATH]={0};
    TCHAR rgszClass[MAX_PATH]={0};
    TCHAR rgszClassGet[MAX_PATH]={0};
    DWORD cbClassGet=0;
    LONG lRet=0;
    DWORD cChars=0;
    DWORD k=0,i=0;
    DWORD dwDispo=0;
    DWORD dwMaxClassLen=0;
    DWORD dwMaxClassLen_Get=0;
    size_t szlen = 0;
    // The shell doesn't necessarily want us to execute the test.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    hRoot=HKEY_LOCAL_MACHINE;

    // Create a key for this test to use.
    StringCchCopy(rgszKeyName, MAX_PATH, _T("tRegApiKey"));
    lRet = RegDeleteKey(hRoot, rgszKeyName);
    if (lRet == ERROR_ACCESS_DENIED)
        REG_FAIL(RegDeleteKey, lRet);
    if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyNew, &dwDispo)))
    {
        TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, rgszKeyName);
        REG_FAIL(RegCreateKeyEx, lRet);
    }
    if (REG_CREATED_NEW_KEY != dwDispo)
        REG_FAIL(RegCreateKeyEx, lRet);


    //
    // CASE: Make sure the reported max class len is 0 for no keys
    //
    lRet = L_GetKey_MaxClassLen(hKeyNew, &dwMaxClassLen_Get);
    if (lRet)
        REG_FAIL(RegQueryInfoKey, lRet);
    if (dwMaxClassLen_Get != 0)
    {
        TRACE(TEXT("TEST_FAIL : RegQueryInfoKey should report MaxClassLen=0 for no keys, but reports %d\r\n"), dwMaxClassLen_Get);
        DUMP_LOCN;
        goto ErrorReturn;
    }

    // Create a bunch of keys are verify it's class.
    for (k=0; k<QUERY_NUM_KEYS; k++)
    {
        StringCchPrintf(rgszKeyName,MAX_PATH,_T("tRegApiSubKey_%d"), k);

        UINT uNumber = 0;
        memset(rgszClass, 65, MAX_PATH);
        // Max registry length is 255, do not exceed
        rand_s(&uNumber);
        cChars= uNumber%255;
        if (cChars<2)
            cChars=10;
        if (!Hlp_GenStringData(rgszClass, cChars, TST_FLAG_ALPHA_NUM))
            GENERIC_FAIL(Hlp_GenStringData);

        StringCchLength(rgszClass, STRSAFE_MAX_CCH, &szlen);
        ASSERT(szlen < sizeof(rgszClass)/sizeof(TCHAR));

        if (dwMaxClassLen < szlen )
        {
            dwMaxClassLen = szlen;
            TRACE(TEXT("New Max Class Length = %d\r\n"), dwMaxClassLen);
        }

        // Create a key with a random class.
        lRet =RegDeleteKey(hKeyNew, rgszKeyName);
        if (lRet == ERROR_ACCESS_DENIED)
            REG_FAIL(RegDeleteKey, lRet);
        if ((ERROR_SUCCESS != (lRet=RegCreateKeyEx(hKeyNew, rgszKeyName, GetOptionsFromString(g_pShellInfo->szDllCmdLine), rgszClass, 0, NULL, NULL, &hKeyTemp, &dwDispo)) && szlen < 255))    //make sure if fails key is not too long
        {
            TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, rgszKeyName);
            REG_FAIL(RegCreateKeyEx, lRet);
        }
        if (dwDispo != REG_CREATED_NEW_KEY)
        {
            TRACE(TEXT("TEST_FAIL : RegCreateKey didn't create a new key %s\r\n"), rgszKeyName);
            REG_FAIL(RegCreateKey, lRet);
        }


        //
        // Case: Verify that the max class len is correct for the parent key.
        //
        lRet = L_GetKey_MaxClassLen(hKeyNew, &dwMaxClassLen_Get);
        if (lRet)
            REG_FAIL(RegQueryInfoKey, lRet);
        if (dwMaxClassLen_Get != dwMaxClassLen)
        {
            TRACE(TEXT("TEST_FAIL : Created a key with max class len=%d, but RegQueryInfoKey reports %d\r\n"),
                dwMaxClassLen, dwMaxClassLen_Get);
            DUMP_LOCN;
            goto ErrorReturn;
        }


        //
        // CASE: Make sure I can correctly get back the key class.
        //
        cbClassGet = sizeof(rgszClassGet)/sizeof(TCHAR);
        memset((PBYTE)rgszClassGet, 65, MAX_PATH);

        lRet = L_GetKeyClass(hKeyTemp, rgszClassGet, &cbClassGet);
        if (lRet)
            REG_FAIL(RegQueryInfoKey, lRet);

        // Verify the key class.
        StringCchLength(rgszClass, STRSAFE_MAX_CCH, &szlen);
        if ((_tcscmp(rgszClassGet, rgszClass)) ||
            (cbClassGet != szlen))
        {
            TRACE(TEXT("TEST_FAIL : Got an incorrect Class for the key. Expecting=%s, got=%s\r\n"), rgszClass, rgszClassGet);
            DUMP_LOCN;
            goto ErrorReturn;
        }


        //
        // CASE: Verify that cbClass doesn't include the terminating NULL
        //
        if ((rgszClassGet[cbClassGet] != _T('\0')) ||
            (rgszClassGet[cbClassGet-1] == _T('\0')))
        {
            TRACE(TEXT("TEST_FAIL : The returned pszclass is not NULL terminated\r\n"));
            TRACE(TEXT("rgszClass=%s, rgszClassGet=%s, cbClassGet=%d, rgszClassGet[cbClassGet]=%c\r\n"),
                rgszClass, rgszClassGet, cbClassGet, rgszClassGet[cbClassGet]);
            DUMP_LOCN;
            goto ErrorReturn;
        }


        //
        // CASE: if the class buffer is too small, it should fail with ERROR_MORE_DATA
        //
        cbClassGet = 1;
        if (ERROR_SUCCESS == (lRet = L_GetKeyClass(hKeyTemp, rgszClassGet, &cbClassGet)))
        {
            TRACE(TEXT("TEST_FAIL : Expecting RegQueryKeyInfo to fail with ERROR_MORE_DATA . rgszClassGet=%s\r\n"), rgszClassGet);
            DUMP_LOCN;
            goto ErrorReturn;
        }
        VERIFY_REG_ERROR(RegQueryInfoKey, lRet, ERROR_MORE_DATA);


        //
        // CASE: Valid pszClass, cbClass=NULL
        //
        if (ERROR_SUCCESS == (lRet = L_GetKeyClass(hKeyTemp, rgszClassGet, NULL)))
        {
            TRACE(TEXT("TEST_FAIL : Expecting RegQueryKeyInfo to fail or &cbClass=NULL\r\n"));
            DUMP_LOCN;
            goto ErrorReturn;
        }
        VERIFY_REG_ERROR(RegQueryInfoKey, lRet, ERROR_INVALID_PARAMETER);


        //
        // CASE: pszClass=NULL and &cbClass=NULL
        //
        if (ERROR_SUCCESS != (lRet = L_GetKeyClass(hKeyTemp, NULL, NULL)))
        {
            TRACE(TEXT("TEST_FAIL : RegQueryKeyInfo failed for pszClass=NULL and &cbClass=NULL \r\n"));
            REG_FAIL(RegQueryInfoKey, lRet);
        }


        //
        // CASE: pszClass=NULL and &cbClass=valid
        //
        if (ERROR_SUCCESS != (lRet = L_GetKeyClass(hKeyTemp, NULL, &cbClassGet)))
        {
            TRACE(TEXT("TEST_FAIL : RegQueryKeyInfo failed for pszClass=NULL and valid cbClass\r\n"));
            REG_FAIL(RegQueryInfoKey, lRet);
        }


        //
        // CASE: Verify that there is no BO
        //

        //  Get the size.
        if (ERROR_SUCCESS != (lRet = L_GetKeyClass(hKeyTemp, NULL, &cbClassGet)))
        {
            TRACE(TEXT("TEST_FAIL : RegQueryKeyInfo failed for pszClass=NULL and valid cbClass\r\n"));
            REG_FAIL(RegQueryInfoKey, lRet);
        }
        memset((PBYTE)rgszClassGet, 65, MAX_PATH);

        //  Get the class .
        cbClassGet++; //   for terminating NULL
        if (ERROR_SUCCESS != (lRet = L_GetKeyClass(hKeyTemp, rgszClassGet, &cbClassGet)))
        {
            TRACE(TEXT("TEST_FAIL : RegQueryKeyInfo failed for pszClass=NULL and valid cbClass\r\n"));
            REG_FAIL(RegQueryInfoKey, lRet);
        }

        // Make sure that the last character is NULL and every byte after that is 65.
        if (rgszClassGet[cbClassGet] != _T('\0'))
        {
            TRACE(TEXT("TEST_FAIL : The returned class is not NULL terminated\r\n"));
            DUMP_LOCN;
            goto ErrorReturn;
        }
        for (i=(cbClassGet+1)*sizeof(TCHAR); i<MAX_PATH; i++)
        {
            if (*(((PBYTE)rgszClassGet)+i) != 65)
            {
                TRACE(TEXT("TEST_FAIL : Potential BO. Expecting rgszClassGet[%d]=65, instead it = %d\r\n"), i, (BYTE)rgszClassGet[i]);
                DUMP_LOCN;
                goto ErrorReturn;

            }
        }
        REG_CLOSE_KEY(hKeyTemp);
    }


    //
    // CASE: If the key doesn't have a class, then it should still be a valid case
    //
    StringCchCopy(rgszKeyName, MAX_PATH, _T("KeyWithNoClass"));

    // Create a key with a random class.
    lRet = RegDeleteKey(hKeyNew, rgszKeyName);
    if (lRet == ERROR_ACCESS_DENIED)
        REG_FAIL(RegDeleteKey, lRet);
    if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hKeyNew, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyTemp, &dwDispo)))
    {
        TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hKeyNew, rgszKeyName);
        REG_FAIL(RegCreateKeyEx, lRet);
    }
    if (dwDispo != REG_CREATED_NEW_KEY)
    {
        TRACE(TEXT("TEST_FAIL : RegCreateKey didn't create a new key %s\r\n"), rgszKeyName);
        REG_FAIL(RegCreateKey, lRet);
    }

    cbClassGet = sizeof(rgszClassGet)/sizeof(TCHAR);
    if (ERROR_SUCCESS != (lRet = L_GetKeyClass(hKeyTemp, rgszClassGet, &cbClassGet)))
    {
        TRACE(TEXT("TEST_FAIL : RegQueryInfoKey fails with err=%u\r\n"), GetLastError());
        g_dwFailCount++;
        DUMP_LOCN;
        goto ErrorReturn;
    }

    dwRetVal = TPR_PASS;

ErrorReturn :
    REG_CLOSE_KEY(hKeyNew);
    REG_CLOSE_KEY(hKeyTemp);
    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// More test on RegQueryInfo
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RegQueryInfo_cSubKey(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0;
    HKEY hKeyNew=0, hKeyTemp=0;
    TCHAR rgszKeyName[MAX_PATH]={0};
    TCHAR rgszValName[MAX_PATH]={0};
    LONG lRet=0;
    DWORD dwType=0, dwDispo=0;
    DWORD k=0;
    DWORD cSubKey=0, dwTemp;
    PBYTE pbData=NULL;
    DWORD cbData=QUERY_DATA_SIZE;

    // The shell doesn't necessarily want us to execute the test.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    hRoot=HKEY_LOCAL_MACHINE;
    pbData = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
    CHECK_ALLOC(pbData);

    StringCchCopy(rgszKeyName, MAX_PATH, _T("tRegApiKey"));
    lRet = RegDeleteKey(hRoot, rgszKeyName);
    if (lRet == ERROR_ACCESS_DENIED)
        REG_FAIL(RegDeleteKey, lRet);
    if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyNew, NULL)))
    {
        TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, rgszKeyName);
        REG_FAIL(RegCreateKeyEx, lRet);
    }

    // Create all the values, one per type
    for (k=0;k<TST_NUM_VALUE_TYPES; k++)
    {
        dwType=g_rgdwRegTypes[k];
        cbData=QUERY_DATA_SIZE;
        StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
        if (!Hlp_Write_Random_Value(hKeyNew, NULL, rgszValName, &dwType, pbData, &cbData))
            goto ErrorReturn;
    }

    TRACE(TEXT("Set %d values\r\n"), k);

    // Create a bunch of keys are verify it's class.
    for (k=0; k<QUERY_NUM_KEYS; k++)
    {
        StringCchPrintf(rgszKeyName,MAX_PATH,_T("tRegApiSubKey_%d"), k);

        if (ERROR_SUCCESS != (lRet = L_GetKey_cSubKey(hKeyNew, &cSubKey)))
            REG_FAIL(RegQueryInfoKey, lRet);
        if (cSubKey != k)
        {
            TRACE(TEXT("TEST_FAIL : Created %d keys, but RegQueryInfoKey says only %d exist\r\n"), k, cSubKey);
            DUMP_LOCN;
            goto ErrorReturn;
        }

        lRet = RegDeleteKey(hKeyNew, rgszKeyName);
        if (lRet == ERROR_ACCESS_DENIED)
            REG_FAIL(RegDeleteKey, lRet);
        if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hKeyNew, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyTemp, &dwDispo)))
        {
            TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, rgszKeyName);
            REG_FAIL(RegCreateKeyEx, lRet);
        }
        if (dwDispo != REG_CREATED_NEW_KEY)
        {
            TRACE(TEXT("TEST_FAIL : RegCreateKey didn't create a new key %s\r\n"), rgszKeyName);
            REG_FAIL(RegCreateKey, lRet);
        }
        REG_CLOSE_KEY(hKeyTemp);
    }


    //
    // CASE: Query keys before and after delete
    //
    for (k=0; k<QUERY_NUM_KEYS; k++)
    {
        StringCchPrintf(rgszKeyName,MAX_PATH,_T("tRegApiSubKey_%d"), k);
        if (ERROR_SUCCESS != (lRet = L_GetKey_cSubKey(hKeyNew, &cSubKey)))
            REG_FAIL(RegQueryInfoKey, lRet);

        lRet = RegDeleteKey(hKeyNew, rgszKeyName);
        if (lRet)
        {
            TRACE(TEXT("TEST_FAIL : Failed RegDeleteKey on key %s\r\n"), rgszKeyName);
            REG_FAIL(RegDeleteKey, lRet);
        }
        if (ERROR_SUCCESS != (lRet = L_GetKey_cSubKey(hKeyNew, &dwTemp)))
            REG_FAIL(RegQueryInfoKey, lRet);

        if (dwTemp+1 != cSubKey)
        {
            TRACE(TEXT("TEST_FAIL : Before deleting a key the count was %d, after deleting key it was %d\r\n"), cSubKey, dwTemp);
            DUMP_LOCN;
            goto ErrorReturn;
        }
    }
    if (ERROR_SUCCESS != (lRet = L_GetKey_cSubKey(hKeyNew, &cSubKey)))
        REG_FAIL(RegQueryInfoKey, lRet);
    // Make sure hte subkey count is 0 at the end of all this.
    if (cSubKey != 0)
    {
        TRACE(TEXT("TEST_FAIL : Expecting subkey count = 0, got %d instead\r\n"), cSubKey);
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: Closed hKey
    //
    lRet = RegCloseKey(hKeyNew);
    if (lRet)
        REG_FAIL(RegCloseKey, lRet);
    if (ERROR_SUCCESS == (lRet = L_GetKey_cSubKey(hKeyNew, &cSubKey)))
    {
        TRACE(TEXT("TEST_FAIL : RegQueryInfo Key should have failed for a closed hKey\r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }
    hKeyNew=0;

    dwRetVal = TPR_PASS;
ErrorReturn :
    FREE(pbData);
    REG_CLOSE_KEY(hKeyNew);
    REG_CLOSE_KEY(hKeyTemp);
    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// More test on RegQueryInfo
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RegQueryInfo_cbMaxSubKey(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0;
    HKEY hKeyNew=0, hKeyTemp=0;
    TCHAR rgszKeyName[MAX_PATH]={0};
    LONG lRet=0;
    DWORD dwDispo=0;
    DWORD k=0;
    DWORD dwMaxSubKey=0;
    DWORD dwMaxSubKey_Get=0;
    DWORD cChars=0;
    size_t szlen = 0;

    // The shell doesn't necessarily want us to execute the test.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    hRoot=HKEY_LOCAL_MACHINE;

    StringCchCopy(rgszKeyName, MAX_PATH, _T("tRegApiKey"));
    lRet = RegDeleteKey(hRoot, rgszKeyName);
    if (lRet == ERROR_ACCESS_DENIED)
        REG_FAIL(RegDeleteKey, lRet);

    if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyNew, NULL)))
    {
        TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, rgszKeyName);
        REG_FAIL(RegCreateKeyEx, lRet);
    }


    //
    // CASE: No subkey case
    //
    if (ERROR_SUCCESS != (lRet=L_GetKey_MaxSubKeyLen(hKeyNew, &dwMaxSubKey_Get)))
        REG_FAIL(RegQueryInfoKey, lRet);

    if (0 != dwMaxSubKey_Get)
    {
        TRACE(TEXT("TEST_FAIL : RegQueryInfoKey returned%d for cbMaxSubKeyLen when there are no keys\r\n"), dwMaxSubKey_Get);
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: Create a bunch of keys of different key name sizes and verify the max sizes
    //
    for (k=0; k<QUERY_NUM_KEYS; k++)
    {
        UINT uNumber = 0;
        memset(rgszKeyName, 65, sizeof(rgszKeyName));
        rand_s(&uNumber);
        cChars = uNumber%255; //    max key name is 255 chars.
        if (cChars<2)
            cChars = 10;
        if (!Hlp_GenStringData(rgszKeyName, cChars, TST_FLAG_ALPHA_NUM))
            GENERIC_FAIL(Hlp_GenStringData);
        StringCchLength(rgszKeyName, STRSAFE_MAX_CCH, &szlen);
        ASSERT(szlen+1 < sizeof(rgszKeyName)/sizeof(TCHAR));
        if (szlen> dwMaxSubKey)
        {
            dwMaxSubKey = szlen;
            TRACE(TEXT("New max subkey len = %d\r\n"), dwMaxSubKey);
        }

        // Create a key with a random key name.
        lRet = RegDeleteKey(hKeyNew, rgszKeyName);
        if (lRet == ERROR_ACCESS_DENIED)
            REG_FAIL(RegDeleteKey, lRet);
        if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hKeyNew, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyTemp, &dwDispo)))
        {
            TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, rgszKeyName);
            REG_FAIL(RegCreateKeyEx, lRet);
        }
        if (dwDispo != REG_CREATED_NEW_KEY)
        {
            TRACE(TEXT("TEST_FAIL : RegCreateKey didn't create a new key %s\r\n"), rgszKeyName);
            REG_FAIL(RegCreateKey, lRet);
        }

        REG_CLOSE_KEY(hKeyTemp);

        if (ERROR_SUCCESS != (lRet=L_GetKey_MaxSubKeyLen(hKeyNew, &dwMaxSubKey_Get)))
            REG_FAIL(RegQueryInfoKey, lRet);

        // Verify the max subkey size.
        if (dwMaxSubKey_Get != dwMaxSubKey)
        {
            TRACE(TEXT("TEST_FAIL : The max key created was %d, but RegQueryInfoKey says %d\r\n"), dwMaxSubKey, dwMaxSubKey_Get);
            DUMP_LOCN;
            goto ErrorReturn;
        }
    }


    //
    // CASE: Closed hKey
    //
    lRet = RegCloseKey(hKeyNew);
    if (lRet)
        REG_FAIL(RegCloseKey, lRet);
    if (ERROR_SUCCESS == (lRet=L_GetKey_MaxSubKeyLen(hKeyNew, &dwMaxSubKey_Get)))
    {
        TRACE(TEXT("TEST_FAIL : RegQueryInfoKey should have failed for a closed hKey\r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }
    hKeyNew=0;

    dwRetVal = TPR_PASS;

ErrorReturn :
    REG_CLOSE_KEY(hKeyNew);
    REG_CLOSE_KEY(hKeyTemp);
    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// More test on RegQueryInfo
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RegQueryInfo_cValues(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0;
    HKEY hKeyNew=0;
    TCHAR rgszKeyName[MAX_PATH]={0};
    TCHAR rgszValName[MAX_PATH]={0};
    LONG lRet=0;
    DWORD dwType=0;
    DWORD k=0;
    PBYTE pbData=NULL;
    DWORD cbData=QUERY_DATA_SIZE;
    DWORD cVals=0;
    DWORD dwMaxValName=0;
    DWORD dwMaxValName_Get=0;
    DWORD dwMaxValData=0;
    DWORD dwMaxValData_Get=0;
    DWORD cChars=0;
    size_t szlen = 0;

    // The shell doesn't necessarily want us to execute the test.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    hRoot=HKEY_LOCAL_MACHINE;
    pbData = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
    CHECK_ALLOC(pbData);

    StringCchCopy(rgszKeyName, MAX_PATH, _T("tRegApiKey"));
    lRet = RegDeleteKey(hRoot, rgszKeyName);
    if (lRet == ERROR_ACCESS_DENIED)
        REG_FAIL(RegDeleteKey, lRet);

    if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyNew, NULL)))
    {
        TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, rgszKeyName);
        REG_FAIL(RegCreateKeyEx, lRet);
    }

    // Create a number of values
    for (k=0;k<QUERY_NUM_VALUES; k++)
    {
        if (ERROR_SUCCESS != (lRet=L_GetKey_cValues(hKeyNew, &cVals)))
            REG_FAIL(RegQueryInfoKey, lRet);


        //
        // CASE: Check that the val count is correct.
        //
        if (cVals != k)
        {
            TRACE(TEXT("TEST_FAIL : Wrote %d values, but RegQueryInfoKey says %d \r\n"), k, cVals);
            DUMP_LOCN;
            goto ErrorReturn;
        }

        UINT uNumber = 0;
        // Write a value with random sized data and random sized name length
        memset(rgszValName, 65, sizeof(rgszKeyName));
        rand_s(&uNumber);
        cChars = uNumber % 255;                                   //  Names can be max 255 chars.
        if (cChars<2) cChars=10;

        if (!Hlp_GenStringData(rgszValName, cChars, TST_FLAG_ALPHA_NUM))
            GENERIC_FAIL(Hlp_GenStringData);

        StringCchLength(rgszValName, STRSAFE_MAX_CCH, &szlen);
        ASSERT(szlen+1 < 255);
        if (szlen+1 > 255)
            TRACE(TEXT(">>>>>>>>> Value name is too big <<<<<<<<<<\r\n"));

        // Track the max val name
        if (szlen > dwMaxValName)
        {
            dwMaxValName = szlen;
            TRACE(TEXT("New Max Val Name = %d\r\n"), dwMaxValName);
        }

        dwType=0;
        cbData=QUERY_DATA_SIZE;
        // Generate some value data depending on the dwtype
        if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
            goto ErrorReturn;

        if ( (REG_BINARY == dwType) ||
            (REG_NONE == dwType) ||
            (REG_DWORD == dwType) ||
            (REG_LINK == dwType) ||
            (REG_RESOURCE_LIST == dwType) ||
            (REG_RESOURCE_REQUIREMENTS_LIST == dwType) ||
            (REG_FULL_RESOURCE_DESCRIPTOR == dwType))
        {
            rand_s(&uNumber);
            cbData =  uNumber % cbData;
        }
        if (!cbData) cbData+=5;

        // Track the max val data
        if (cbData > dwMaxValData)
        {
            TRACE(TEXT("New Max Val Data =%d\r\n"), cbData);
            dwMaxValData = cbData;
        }

        lRet = RegSetValueEx(hKeyNew, rgszValName, 0, dwType, pbData, cbData);
        if (lRet)
        {
            DebugBreak();
            TRACE(TEXT("rgszValName=%s, dwType=%d, cbData=%d, k=%d \r\n"), rgszValName, dwType, cbData, k);
            REG_FAIL(RegSetValueEx, lRet);
        }


        //
        // CASE: Check that the max val data is correct.
        //
        lRet= L_GetKey_cbMaxValData(hKeyNew, &dwMaxValData_Get);
        if (lRet)
            REG_FAIL(RegQueryInfoKey, lRet);
        if (dwMaxValData_Get != dwMaxValData)
        {
            TRACE(TEXT("TEST_FAIL : Wrote max Val data=%d, but RegQueryInfoKey reports %d\r\n"), dwMaxValData, dwMaxValData_Get);
            DUMP_LOCN;
            goto ErrorReturn;
        }


        //
        // CASE: Check that the val name is correct.
        //
        lRet= L_GetKey_cbMaxValName(hKeyNew, &dwMaxValName_Get);
        if (lRet)
            REG_FAIL(RegQueryInfoKey, lRet);
        if (dwMaxValName_Get != dwMaxValName)
        {
            TRACE(TEXT("TEST_FAIL : Wrote max Val Name=%d, but RegQueryInfoKey reports %d\r\n"), dwMaxValName, dwMaxValName_Get);
            DUMP_LOCN;
            goto ErrorReturn;
        }

    }

    dwRetVal = TPR_PASS;

ErrorReturn :
    FREE(pbData);
    REG_CLOSE_KEY(hKeyNew);
    return dwRetVal;
}




struct RegInfoStruct
{
    HKEY m_hkey;
    CE_REGISTRY_INFO m_regInfo;
};

TESTPROCAPI Tst_CeRegGetInfo_Funct(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    HKEY hRoot = 0;
    size_t szlen = 0;
    DWORD dwStructLength[10];
    DWORD dwRetVal = TPR_FAIL;
    LPWSTR szStructStringName[10];
    CE_REGISTRY_INFO regInfo = {0};
    RegInfoStruct infoStruct[10] = {0};

    LPWSTR szKeyName, szTempKey1,szTempKey2,szTempKey3;
    szKeyName = szTempKey1 = szTempKey2 = szTempKey3 = NULL;
    LPCWSTR Whack = L"\\";
    // Set up arrays of "random" path names
    LPWSTR KeyList1[10] = { L"Temp", L"Microsoft",L"Test", L"KeyWord", L"Check", L"Path",
        L"Application",L"Data", L"Text",L"Solution"};
    LPWSTR KeyList2[10] = { L"Tools", L"Settings", L"Documents",L"Private",L"Business",
        L"User", L"Admin", L"System", L"Archive", L"Tools"};
    LPWSTR KeyList3[10] = { L"Info", L"Details", L"Output", L"Input", L"Result",
        L"Stats", L"Path", L"Trash", L"Log", L"Final"};
    // Set up array of possible "random" Roots
    HKEY hRootList[4] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE , HKEY_USERS, HKEY_CLASSES_ROOT };

    // Seed the random with the provided Random Seed
    srand(((TPS_EXECUTE*)tpParam)->dwRandomSeed);

    UINT uNumber = 0;
    DWORD dwActionTaken = 0;
    // Loop over struct and open handles to 10 random keys and fill in their Info Structs by Hand
    for(int i = 0; i < 10; ++i)
    {
        // get the random values

        rand_s(&uNumber);
        int rootKey = uNumber % 4;

        rand_s(&uNumber);
        int nameKey1 = uNumber % 10;
        rand_s(&uNumber);
        int nameKey2 = uNumber % 10;
        rand_s(&uNumber);
        int nameKey3 = uNumber % 10;

        // get random data
        hRoot = hRootList[rootKey];

        szTempKey1 = KeyList1[nameKey1];
        szTempKey2 = KeyList2[nameKey2];
        szTempKey3 = KeyList3[nameKey3];
        HRESULT hr = S_OK;
        unsigned int tempsize = 0, size = 0;
        hr = FAILED(hr) ? hr : StringCchLength(szTempKey1,STRSAFE_MAX_CCH,&tempsize);
        size += tempsize; // add size of TempKey1
        hr = FAILED(hr) ? hr : StringCchLength(szTempKey2,STRSAFE_MAX_CCH,&tempsize);
        size += tempsize; // add size of TempKey2
        hr = FAILED(hr) ? hr : StringCchLength(szTempKey3,STRSAFE_MAX_CCH,&tempsize);
        size += tempsize; // add size of TempKey3
        hr = FAILED(hr) ? hr : StringCchLength(Whack,STRSAFE_MAX_CCH,&tempsize);
        size += (tempsize *3); // add space for all 3 "whacks"
        ++size; // add space for null termination
        if(size == 0 || FAILED(hr))
        {
            FAIL("StringCchLength failed when getting length of the registry key name.");
            goto Error;
        }
        hr = S_OK;
        //int size = wcslen(szTempKey1) + wcslen(szTempKey2) + wcslen(szTempKey3) + (3 * wcslen(Whack)) + 1;
        szKeyName = new TCHAR[size];
        if(szKeyName != NULL)
            szKeyName[0] = L'\0';
        else
        {
            FAIL("New failed to create buffer for registry key name.");
            goto Error;
        }
        // concatenate the random key names with \ between each

        hr = FAILED(hr) ? hr :StringCchCatW(szKeyName,size,Whack);
        hr = FAILED(hr) ? hr :StringCchCatW(szKeyName,size,szTempKey1);
        hr = FAILED(hr) ? hr :StringCchCatW(szKeyName,size,Whack);
        hr = FAILED(hr) ? hr :StringCchCatW(szKeyName,size,szTempKey2);
        hr = FAILED(hr) ? hr :StringCchCatW(szKeyName,size,Whack);
        hr = FAILED(hr) ? hr :StringCchCatW(szKeyName,size,szTempKey3);
        if(FAILED(hr))
        {
            delete[] szKeyName;
            FAIL("StringCchCatW failed to concatenate registry key name.");
            goto Error;
        }

        bool done = false;
        bool duplicate = false;
        // check to make sure we didn't create a duplicate name
        while(!done)
        {
            for(int j = 0; j < i; ++j)
            {
                HRESULT hr = S_OK;
                size_t sizecheck = 0;
                hr = StringCchLength(szStructStringName[j],STRSAFE_MAX_CCH,&sizecheck);
                if(wcsncmp(szKeyName,szStructStringName[j],sizecheck)== 0)
                {
                    duplicate = true;
                    break;
                }
            }
            if(duplicate)
            {   // if we did, re-randomize
                delete[] szKeyName;

                rand_s(&uNumber);
                int nameKey1 = uNumber %10;
                rand_s(&uNumber);
                int nameKey2 = (uNumber %10);
                rand_s(&uNumber);
                int nameKey3 = uNumber %10;

                szTempKey1 = KeyList1[nameKey1];
                szTempKey2 = KeyList2[nameKey2];
                szTempKey3 = KeyList3[nameKey3];

                HRESULT hr = S_OK;
                unsigned int tempsize = 0, size = 0;
                hr = FAILED(hr) ? hr : StringCchLength(szTempKey1,STRSAFE_MAX_CCH,&tempsize);
                size += tempsize; // add size of TempKey1
                hr = FAILED(hr) ? hr : StringCchLength(szTempKey2,STRSAFE_MAX_CCH,&tempsize);
                size += tempsize; // add size of TempKey2
                hr = FAILED(hr) ? hr : StringCchLength(szTempKey3,STRSAFE_MAX_CCH,&tempsize);
                size += tempsize; // add size of TempKey3
                hr = FAILED(hr) ? hr : StringCchLength(Whack,STRSAFE_MAX_CCH,&tempsize);
                size += (tempsize *3); // add space for all 3 "whacks"
                ++size; // add space for null termination
                if(size == 0 || FAILED(hr))
                {
                    FAIL("StringCchLength failed to get length of new registry key name to replace duplicate.");
                    goto Error;
                }

                szKeyName = new wchar_t[size];

                if(szKeyName != NULL)
                    szKeyName[0] = L'\0';
                else
                    goto Error;
                hr = S_OK;
                hr = FAILED(hr) ? hr :StringCchCatW(szKeyName,size,Whack);
                hr = FAILED(hr) ? hr :StringCchCatW(szKeyName,size,szTempKey1);
                hr = FAILED(hr) ? hr :StringCchCatW(szKeyName,size,Whack);
                hr = FAILED(hr) ? hr :StringCchCatW(szKeyName,size,szTempKey2);
                hr = FAILED(hr) ? hr :StringCchCatW(szKeyName,size,Whack);
                hr = FAILED(hr) ? hr :StringCchCatW(szKeyName,size,szTempKey3);
                if(FAILED(hr))
                {
                    delete[] szKeyName;
                    FAIL("StringCchCatW failed to concatenate registry key name to replace duplicate.");
                    goto Error;
                }
                else
                    duplicate = false;
            }
            else
                done = true;

        }
        if(ERROR_SUCCESS != RegCreateKeyEx(hRoot,szKeyName,0,NULL,GetOptionsFromString(g_pShellInfo->szDllCmdLine),
            0,NULL,&infoStruct[i].m_hkey,&dwActionTaken))
        {
            FAIL("RegCreateKeyEx failed to create new registry key.");
            goto Error;
        }
        StringCchLength(szKeyName, STRSAFE_MAX_CCH, &szlen);
        dwStructLength[i] = szlen + 1;
        szStructStringName[i] = szKeyName;
        infoStruct[i].m_regInfo.cbSize = sizeof(CE_REGISTRY_INFO);
        infoStruct[i].m_regInfo.pdwKeyNameLen = &dwStructLength[i];
        infoStruct[i].m_regInfo.pszFullKeyName = szStructStringName[i];
        infoStruct[i].m_regInfo.hRootKey = hRoot;
        infoStruct[i].m_regInfo.dwFlags = 0;

    }

    LPWSTR szTempKeyName;
    DWORD dwSize;
    for(int i = 0; i < 10;++i)
    {
        dwSize = 500;
        szTempKeyName = new wchar_t[dwSize];
        if(szTempKeyName == NULL)
        {
            FAIL("New failed to create buffer for temp registry key name.");
            goto Error;
        }
        regInfo.cbSize = sizeof(regInfo);
        regInfo.pdwKeyNameLen = &dwSize;
        regInfo.pszFullKeyName = szTempKeyName;

        size_t KeySize = 0;
        HRESULT hr = StringCchLength(infoStruct[i].m_regInfo.pszFullKeyName,STRSAFE_MAX_CCH,&KeySize);
        if(FAILED(hr))
        {
            FAIL("StringCchLength failed to get length of registry key name.");
            goto Error;
        }

        DWORD err = CeRegGetInfo(infoStruct[i].m_hkey,&regInfo);
        if(err != ERROR_SUCCESS)
        {
            delete[] szTempKeyName;
            FAIL("CeRegGetInfo failed to retrieve registry key info.");
            goto Error;
        }
        else if(regInfo.hRootKey != infoStruct[i].m_regInfo.hRootKey)
        {
            delete[] szTempKeyName;
            FAIL("CeRegGetInfo did not return correct Root Key.");
            goto Error;
        }
        else if(*regInfo.pdwKeyNameLen != *infoStruct[i].m_regInfo.pdwKeyNameLen)
        {
            delete[] szTempKeyName;
            FAIL("CeRegGetInfo did not return the correct registry key name length.");
            goto Error;
        }
        else if(_wcsicmp(regInfo.pszFullKeyName,infoStruct[i].m_regInfo.pszFullKeyName) != 0)
        {
            delete[] szTempKeyName;
            FAIL("CeRegGetInfo did not return the correct registry key name.");
            goto Error;
        }

        delete[] szTempKeyName;

    }

    dwRetVal = TPR_PASS;

Error:
    for(int i = 0; i < 10; ++i)
    {
        if(infoStruct[i].m_hkey != NULL)
            RegCloseKey(infoStruct[i].m_hkey);
        if(infoStruct[i].m_regInfo.pszFullKeyName != NULL)
            delete[]infoStruct[i].m_regInfo.pszFullKeyName;
    }
    return dwRetVal;
}

TESTPROCAPI Tst_CeRegGetInfo_Error(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    HKEY hTempKey = 0;
    size_t szlen = 0;
    HKEY hRoot = 0;
    CE_REGISTRY_INFO regInfo = {0};
    DWORD dwActionTaken = 0;
    DWORD dwKeySize = 0;
    DWORD dwRetVal = TPR_FAIL;
    LPWSTR szKeyName, szTempKey1,szTempKey2,szTempKey3;
    szKeyName = szTempKey1 = szTempKey2 = szTempKey3 = NULL;
    LPCWSTR Whack = L"\\";
    LPWSTR szBuffer = NULL;
    // Set up arrays of "random" path names
    LPWSTR KeyList1[10] = { L"Temp", L"Microsoft",L"Test", L"KeyWord", L"Check", L"Path",
        L"Application",L"Data", L"Text",L"Solution"};
    LPWSTR KeyList2[10] = { L"Tools", L"Settings", L"Documents",L"Private",L"Business",
        L"User", L"Admin", L"System", L"Archive", L"Tools"};
    LPWSTR KeyList3[10] = { L"Info", L"Details", L"Output", L"Input", L"Result",
        L"Stats", L"Path", L"Trash", L"Log", L"Final"};
    // Set up array of possible "random" Roots
    HKEY hRootList[4] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE , HKEY_USERS, HKEY_CLASSES_ROOT };

    // Seed the random with the provided Random Seed
    srand(((TPS_EXECUTE*)tpParam)->dwRandomSeed);

    UINT uNumber;

    // get the random values
    rand_s (&uNumber);
    int rootKey = uNumber % 4;

    rand_s (&uNumber);
    int nameKey1 = uNumber %10;
    rand_s (&uNumber);
    int nameKey2 = uNumber %10;
    rand_s (&uNumber);
    int nameKey3 = uNumber % 10;

    // get random data
    hRoot = hRootList[rootKey];

    szTempKey1 = KeyList1[nameKey1];
    szTempKey2 = KeyList2[nameKey2];
    szTempKey3 = KeyList3[nameKey3];

    HRESULT hr = S_OK;
    unsigned int tempsize = 0, size = 0;
    hr = FAILED(hr) ? hr : StringCchLength(szTempKey1,STRSAFE_MAX_CCH,&tempsize);
    size += tempsize; // add size of TempKey1
    hr = FAILED(hr) ? hr : StringCchLength(szTempKey2,STRSAFE_MAX_CCH,&tempsize);
    size += tempsize; // add size of TempKey2
    hr = FAILED(hr) ? hr : StringCchLength(szTempKey3,STRSAFE_MAX_CCH,&tempsize);
    size += tempsize; // add size of TempKey3
    hr = FAILED(hr) ? hr : StringCchLength(Whack,STRSAFE_MAX_CCH,&tempsize);
    size += (tempsize *3); // add space for all 3 "whacks"
    ++size; // add space for null termination
    if(size == 0 || FAILED(hr))
    {
        FAIL("StringCchLength failed when getting length of the registry key name.");
        goto Error;
    }

    szKeyName = new wchar_t[size];
    if(szKeyName != NULL)
        szKeyName[0] = L'\0';
    else
    {
        FAIL("New failed to create buffer for registry key name.");
        goto Error;
    }
    // concatenate the random key names with \ between each
    hr = S_OK;
    hr = FAILED(hr) ? hr :StringCchCatW(szKeyName,size,Whack);
    hr = FAILED(hr) ? hr :StringCchCatW(szKeyName,size,szTempKey1);
    hr = FAILED(hr) ? hr :StringCchCatW(szKeyName,size,Whack);
    hr = FAILED(hr) ? hr :StringCchCatW(szKeyName,size,szTempKey2);
    hr = FAILED(hr) ? hr :StringCchCatW(szKeyName,size,Whack);
    hr = FAILED(hr) ? hr :StringCchCatW(szKeyName,size,szTempKey3);
    if(FAILED(hr))
    {
        delete[] szKeyName;
        FAIL("StringCchCatW failed to concatenate registry key name.");
        return dwRetVal;
    }
    StringCchLength(szKeyName, STRSAFE_MAX_CCH, &szlen);
    dwKeySize = szlen + 1;

    if(ERROR_SUCCESS != RegCreateKeyEx(hRoot,szKeyName,0,NULL,GetOptionsFromString(g_pShellInfo->szDllCmdLine),
        0,NULL,&hTempKey,&dwActionTaken))
    {
        delete[] szKeyName;
        FAIL("RegCreateKeyEx failed to create new registry key.");
        return dwRetVal;
    }



    DWORD dwBufferSize = 500;
    szBuffer = new wchar_t[dwBufferSize];

    regInfo.cbSize = sizeof(CE_REGISTRY_INFO);
    regInfo.pdwKeyNameLen = &dwBufferSize;
    regInfo.pszFullKeyName = szBuffer;

    HKEY hDummyKey = 0;

    // pass in a null key
    if(ERROR_INVALID_HANDLE != CeRegGetInfo(hDummyKey,&regInfo))
    {
        ERRFAIL("CeRegGetInfo did not return expected error(ERROR_INVALID_HANDLE) when given a null registry key.");
        goto Error;
    }

    // pass in null struct
    if(ERROR_INVALID_HANDLE != CeRegGetInfo(hTempKey,NULL))
    {
        ERRFAIL("CeRegGetInfo did not return expected error(ERROR_INVALID_HANDLE) when given a null CE_REGISTRY_INFO struct.");
        goto Error;
    }


    regInfo.cbSize = 0;
    // pass in CE_REGISTRY_INFO struct with everything right except 0 for the size
    if(ERROR_INVALID_HANDLE != CeRegGetInfo(hTempKey,&regInfo))
    {
        ERRFAIL("CeRegGetInfo did not return expected error(ERROR_INVALID_HANDLE) when given a regInfo struct with size set to 0.");
        goto Error;
    }

    regInfo.cbSize = sizeof(CE_REGISTRY_INFO);
    regInfo.pdwKeyNameLen = NULL;
    // pass in struct with everything correct but the name len ptr is NULL.
    if(ERROR_INVALID_PARAMETER != CeRegGetInfo(hTempKey,&regInfo))
    {
        ERRFAIL("CeRegGetInfo did not return expected error(ERROR_INVALID_PARAMETER) when given a regInfo struct with null key name length pointer.");
        goto Error;
    }
    DWORD dwDummySize = 0;
    regInfo.pdwKeyNameLen = &dwDummySize;
    regInfo.pszFullKeyName = szBuffer;
    // pass in struct with everything correct but buffer size variable set to 0
    // *Note* This will pass for the same reason the next test passes.
    if(ERROR_SUCCESS != CeRegGetInfo(hTempKey,&regInfo))
    {
        ERRFAIL("CeRegGetInfo did not return expected error(ERROR_SUCCESS) when given a regInfo struct with key name length set to 0.");
        goto Error;
    }
    regInfo.pdwKeyNameLen = &dwBufferSize;
    regInfo.pszFullKeyName = NULL;
    // pass in struct with everything correct but the name ptr
    // *NOTE* This is designed to pass, to allow user to
    // find size of the key before allocating buffer, etc
    if(ERROR_SUCCESS != CeRegGetInfo(hTempKey, &regInfo))
    {
        ERRFAIL("CeRegGetInfo did not return expected error(ERROR_SUCCESS) when given a regInfo struct null for key name pointer.");
        goto Error;
    }

    delete[] szBuffer;
    dwBufferSize = dwKeySize -1;
    szBuffer = new wchar_t[dwBufferSize];
    regInfo.pszFullKeyName = szBuffer;
    regInfo.pdwKeyNameLen = &dwBufferSize;

    // pass in struct with buffer a little too small
    if(ERROR_INSUFFICIENT_BUFFER != CeRegGetInfo(hTempKey, &regInfo))
    {
        ERRFAIL("CeRegGetInfo did not return expected error(ERROR_INSUFFICENT_BUFFER) when given a regInfo struct with name length set to a number smaller than required.");
        goto Error;
    }


    dwRetVal = TPR_PASS;


Error:
    if(szBuffer)
        delete[] szBuffer;
    if(szKeyName)
        delete[] szKeyName;
    if(hTempKey)
        RegCloseKey(hTempKey);


    return dwRetVal;
}
