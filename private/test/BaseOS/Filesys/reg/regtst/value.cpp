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

DWORD   g_rgdwRegTypes[] = {  REG_NONE,
                            REG_SZ,
                            REG_EXPAND_SZ,
                            REG_BINARY,
                            REG_DWORD,
                            REG_DWORD_BIG_ENDIAN,
                            REG_LINK,
                            REG_MULTI_SZ,
                            REG_RESOURCE_LIST,
                            REG_FULL_RESOURCE_DESCRIPTOR,
                            REG_RESOURCE_REQUIREMENTS_LIST
                            };


#define TST_DATA_SIZE   100

BOOL L_SetAndGetValue(HKEY hKey, DWORD dwType, LPCTSTR rgszValName, const BYTE* pbData, DWORD cbData)
{
    BOOL fRetVal=FALSE;
    DWORD dwTypeGet=0;
    PBYTE pbDataGet=NULL;
    DWORD cbDataGet=0;
    LONG lRet=0;
    DWORD cbDataActual=cbData;

    ASSERT(pbData);

    // Check the type, if it's string:
    // If it's REG_SZ and only one byte, OS will add a null terminated character
    if (dwType == REG_SZ && cbData == 1)
    {
        if (pbData[0] == 0)
        {
            TRACE(TEXT("Set REG_SZ on one byte NULL string\r\n"));
            cbDataActual += 1;
        }
        else // OS will pad another byte and then null-terminated it.
        {
            TRACE(TEXT("Set REG_SZ on one byte string\r\n"));
            cbDataActual += sizeof(TCHAR) + 1;
        }
    }

    // If it's REG_SZ and non-null terminated, OS will add a null terminated character
    if (dwType == REG_SZ && cbData >= sizeof(TCHAR))
    {
        // If it's unaligned data, OS will always pad a byte before adding the null terminated character
        if (cbData %2)
        {
            cbDataActual += sizeof(TCHAR) + 1;
        }
        else
        {
            if ((TCHAR)pbData[cbData-sizeof(TCHAR)] != _T('\0'))
                cbDataActual += sizeof(TCHAR);
        }
    }

    // If it's REG_MULTI_SZ and small multi_sz
    if (dwType == REG_MULTI_SZ && cbData < 2*sizeof(TCHAR))
    {
        TRACE(TEXT("Set REG_MULTI_SZ on small string - cbData=%d\r\n"), cbData);
        if (cbData == 3)
        {
            if (pbData[2] == 0)
                cbDataActual += sizeof(TCHAR) + 1;
            else
                cbDataActual += 2*sizeof(TCHAR) + 1;
        }
        else if (cbData == 2)
        {
            if (pbData[0] == 0 && pbData[1] == 0)
                cbDataActual = 2*sizeof(TCHAR);
            else
                cbDataActual += 2*sizeof(TCHAR);
        }
        else
            cbDataActual = 0;
    }

    // If it's REG_MULTI_SZ and it's a big enough multi_sz
    if (dwType == REG_MULTI_SZ && cbData >= 2*sizeof(TCHAR))
    {
        if (cbData %2)
        {
            // If it's unaligned, OS will pad one byte and add two null-terminated chars
            TRACE(TEXT("Set REG_MULTI_SZ on unaligned chars\r\n"));
            cbDataActual += 2*sizeof(TCHAR) + 1;
        }
        else
        {
            // If it's REG_MULTI_SZ and don't have any null-terminated character,
            // OS will add two null-terminated chars
            if ((TCHAR)pbData[cbData-(2*sizeof(TCHAR))] != _T('\0') &&
                (TCHAR)pbData[cbData-sizeof(TCHAR)] != _T('\0'))
            {
                TRACE(TEXT("Set REG_MULTI_SZ on no null-terminated chars\r\n"));
                cbDataActual += 2*sizeof(TCHAR);
            }

            // If it's REG_MULTI_SZ and only have one null-terminated character,
            // OS will add one null-terminated char
            if ((TCHAR)pbData[cbData-(2*sizeof(TCHAR))] != _T('\0') &&
                (TCHAR)pbData[cbData-sizeof(TCHAR)] == _T('\0'))
            {
                TRACE(TEXT("Set REG_MULTI_SZ with one null-terminated char at the end\r\n"));
                cbDataActual += sizeof(TCHAR);
            }
        }
    }

    // Set the value
    if (ERROR_SUCCESS != (lRet=RegSetValueEx(hKey, rgszValName, 0, dwType, pbData, cbData)))
    {
        TRACE(TEXT("TEST_FAIL : Failed RegSetValueEx %s, dwType=%d\r\n"), rgszValName, dwType);
        REG_FAIL(RegSetValueEx, lRet);
    }

    // Just get the type
    if (ERROR_SUCCESS != (lRet=RegQueryValueEx(hKey, rgszValName, 0, &dwTypeGet, NULL, NULL)))
        REG_FAIL(RegQueryValueEx, lRet);

    if (dwType != dwTypeGet)
    {
        TRACE(TEXT("TEST_FAIL: dwType=%d when setting, but dwType=%d when getting\r\n"), dwType, dwTypeGet);
        TRACE(TEXT("ValName=%s, dwType=%d\r\n"), rgszValName, dwType);
        DUMP_LOCN;
        goto ErrorReturn;
    }

    // Query the buffer size.
    if (ERROR_SUCCESS != (lRet=RegQueryValueEx(hKey, rgszValName, 0, &dwTypeGet, NULL, &cbDataGet)))
        REG_FAIL(RegQueryValueEx, lRet);

    if (cbDataGet != cbDataActual)
    {
        TRACE(TEXT("TEST_FAIL: cbData=%d when setting, but cbData=%d when getting\r\n"), cbDataActual, cbDataGet);
        TRACE(TEXT("ValName=%s, dwType=%d\r\n"), rgszValName, dwType);
        DUMP_LOCN;
        goto ErrorReturn;
    }

    pbDataGet = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbDataGet);
    CHECK_ALLOC(pbDataGet);
    memset(pbDataGet, 33, cbDataGet);

    // Get the value
    if (ERROR_SUCCESS != (lRet=RegQueryValueEx(hKey, rgszValName, 0, &dwTypeGet, pbDataGet, &cbDataGet)))
        REG_FAIL(RegQueryValueEx, lRet);

    // Check results
    if ( (dwTypeGet != dwType) ||
         (0 != memcmp(pbDataGet, pbData, cbData)) )
    {
        TRACE(TEXT("TEST_FAIL : Did not get the same data that was set on value %s\r\n"), rgszValName);
        DUMP_LOCN;
        goto ErrorReturn;
    }

    if (cbDataActual != cbDataGet)
    {
        TRACE(TEXT("TEST_FAIL with setting data cbData=%d, but cbGetData=%d\r\n"), cbDataActual, cbDataGet);
        TRACE(TEXT("ValName=%s, type=%d\r\n"), rgszValName, dwType);
        DUMP_LOCN;
        goto ErrorReturn;
    }

    fRetVal = TRUE;
ErrorReturn :
    FREE(pbDataGet);
    return fRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// Tests setting registry value
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RegSetValue_Functionals(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=HKEY_LOCAL_MACHINE ;
    HKEY hKeyNew=0;
    TCHAR rgszKeyName[MAX_PATH]={0};
    TCHAR rgszValName[MAX_PATH]={0};
    LONG lRet=0;
    int k=0;
    DWORD dwType=0;
    DWORD dwTypeGet=0;
    PBYTE pbData=NULL;
    DWORD cbData=TST_DATA_SIZE;    //  warning : Don't change this
    PBYTE pbDataGet=NULL;
    DWORD cbDataGet=TST_DATA_SIZE;//  warning : Don't change this
    WCHAR szTemp[MAX_PATH];

    // The shell doesn't necessarily want us to execute the test.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    pbData = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
    CHECK_ALLOC(pbData);

    pbDataGet = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbDataGet);
    CHECK_ALLOC(pbDataGet);

    StringCchCopy(rgszKeyName, MAX_PATH, _T("tRegApiKey"));
    if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyNew, NULL)))
    {
        TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, rgszKeyName);
        REG_FAIL(RegCreateKeyEx, lRet);
        goto ErrorReturn;
    }


    //
    // CASE: Set Values of all types. Read then and make sure the types are correct.
    //
    for (k=0; k<TST_NUM_VALUE_TYPES; k++)
    {
        dwType = g_rgdwRegTypes[k];
        cbData=TST_DATA_SIZE;

        //
        // CASE: Try setting and checking a value of this type
        //
        if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
            goto ErrorReturn;

        StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
        if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
            goto ErrorReturn;

        // Make sure the unnamed value of the key can also be of this type
        StringCchCopy(szTemp, MAX_PATH, _T(""));
        if (!L_SetAndGetValue(hKeyNew, dwType, szTemp, pbData, cbData))
            goto ErrorReturn;

        TRACE(TEXT("Value %s ok\r\n"), rgszValName);
    }
    REG_CLOSE_KEY(hKeyNew);
    lRet = RegDeleteKey(hRoot, rgszKeyName);
    if (lRet == ERROR_ACCESS_DENIED)
    {
        REG_FAIL(RegDeleteKey, lRet);
        goto ErrorReturn;
    }


    //
    // CASE: Make sure the NULL and "" are the same unnamed value
    //
    StringCchCopy(rgszKeyName, MAX_PATH, _T("tRegApiKey"));
    if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyNew, NULL)))
    {
        TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, rgszKeyName);
        REG_FAIL(RegCreateKeyEx, lRet);
        goto ErrorReturn;
    }

    dwType=REG_DWORD;
    cbData=TST_DATA_SIZE;
    if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
        goto ErrorReturn;

    // Set Val=NULL
    if (ERROR_SUCCESS != (lRet=RegSetValueEx(hKeyNew, NULL, 0, dwType, pbData, cbData)))
    {
        TRACE(TEXT("TEST_FAIL : Failed RegSetValueEx NULL, dwType=%d\r\n"),dwType);
        REG_FAIL(RegSetValueEx, lRet);
        goto ErrorReturn;
    }

    cbDataGet = TST_DATA_SIZE;
    // Get Val=""
    if (ERROR_SUCCESS != (lRet=RegQueryValueEx(hKeyNew, _T(""), 0, &dwTypeGet, pbDataGet, &cbDataGet)))
    {
        REG_FAIL(RegQueryValueEx, lRet);
        goto ErrorReturn;
    }
    if ( (dwTypeGet != dwType) ||
         (0 != memcmp(pbDataGet, pbData, cbData)) )
    {
        TRACE(TEXT("TestFail : NULL and \"\" might not be the same value\r\n"));
        TRACE(TEXT("TEST_FAIL : Did not get the same data that was set on value %s\r\n"), _T(""));
        goto ErrorReturn;
    }


    //
    // CASE: Try setting and getting a 4K value
    //
    FREE(pbData);
    FREE(pbDataGet);
    cbData = 4*1024;
    cbDataGet = cbData;
    pbData = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
    CHECK_ALLOC(pbData);

    dwType = REG_SZ;
    StringCchCopy(rgszValName, MAX_PATH, _T("LargeVal"));
    Hlp_GenRandomValData(&dwType, pbData, &cbData);

    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
        goto ErrorReturn;

    RegDeleteValue(hKeyNew, rgszValName);


    //
    // CASE: Try setting and getting larger than 4K value
    //
    FREE(pbData);
    FREE(pbDataGet);
    cbData = 5*1024;
    cbDataGet = cbData;
    pbData = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
    CHECK_ALLOC(pbData);

    dwType = REG_SZ;
    StringCchCopy(rgszValName, MAX_PATH, _T("LargeVal"));
    Hlp_GenRandomValData(&dwType, pbData, &cbData);
    if (ERROR_SUCCESS == (lRet = RegSetValueEx(hKeyNew, rgszValName, 0, dwType, pbData, cbData)))
    {
        TRACE(TEXT("TEST_FAIL : RegSetValueEx should have failed for a large value\r\n"));
        goto ErrorReturn;
    }

    dwRetVal=TPR_PASS;

ErrorReturn :
    REG_CLOSE_KEY(hKeyNew);
    FREE(pbData);
    FREE(pbDataGet);
    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// More test on RegSetVal
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RegSetVal_NameVariations(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0;
    HKEY hKeyNew=0;
    TCHAR rgszKeyName[MAX_PATH]={0};
    TCHAR rgszValName[MAX_PATH]={0};
    LONG lRet=0;
    PBYTE pbData=NULL;
    DWORD cbData=TST_DATA_SIZE;
    PBYTE pbDataGet=NULL;
    DWORD cbDataGet=TST_DATA_SIZE;
    DWORD dwType=0;

    // The shell doesn't necessarily want us to execute the test.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    hRoot=HKEY_LOCAL_MACHINE;
    pbData = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
    CHECK_ALLOC(pbData);

    pbDataGet = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbDataGet);
    CHECK_ALLOC(pbDataGet);

    StringCchCopy(rgszKeyName, MAX_PATH, _T("tRegApiKey"));
    if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyNew, NULL)))
    {
        TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, rgszKeyName);
        REG_FAIL(RegCreateKeyEx, lRet);
        goto ErrorReturn;
    }


    //
    // CASE: Check case sensitivity
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("lowercase"));
    dwType = REG_SZ;
    Hlp_GenRandomValData(&dwType, pbData, &cbData);
    if (ERROR_SUCCESS != (lRet = RegSetValueEx(hKeyNew, rgszValName, 0, dwType, pbData, cbData)))
    {
        REG_FAIL(RegSetValueEx, lRet);
        goto ErrorReturn;
    }

    StringCchCopy(rgszValName, MAX_PATH, _T("LOWERCASE"));
    dwType = REG_DWORD;
    cbData=TST_DATA_SIZE;
    Hlp_GenRandomValData(&dwType, pbData, &cbData);
    if (ERROR_SUCCESS != (lRet = RegSetValueEx(hKeyNew, rgszValName, 0, dwType, pbData, cbData)))
    {
        REG_FAIL(RegSetValueEx, lRet);
        goto ErrorReturn;
    }

    StringCchCopy(rgszValName, MAX_PATH, _T("lowercase"));
    if (ERROR_SUCCESS != (lRet=RegQueryValueEx(hKeyNew, rgszValName, 0, &dwType, pbDataGet, &cbDataGet)))
    {
        REG_FAIL(RegQueryValueEx, lRet);
        goto ErrorReturn;
    }

    if ( (REG_DWORD != dwType) ||
         (0 != memcmp(pbDataGet, pbData, cbData)) )
    {
        TRACE(TEXT("TestFail : RegSetValue might be case sensitive\r\n"));
        TRACE(TEXT("TEST_FAIL : Did not get the same data that was set on value %s\r\n"), rgszValName);
        goto ErrorReturn;
    }


    //
    // CASE: Numeric value names
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("123456"));
    cbData=TST_DATA_SIZE;
    dwType = REG_DWORD;
    Hlp_GenRandomValData(&dwType, pbData, &cbData);
    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
    {
        TRACE(TEXT("TEST_FAIL : RegSetValue does not allow numeric value names\r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: Alpha Numeric Value name
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("1a2b3c4d"));
    cbData=TST_DATA_SIZE;
    dwType = REG_DWORD;
    Hlp_GenRandomValData(&dwType, pbData, &cbData);
    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
    {
        TRACE(TEXT("TEST_FAIL : RegSetValue does not allow alpha numeric value names\r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: Non-alpha-numeric Value names
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("#$%^"));
    cbData=TST_DATA_SIZE;
    dwType = 0;
    Hlp_GenRandomValData(&dwType, pbData, &cbData);
    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
    {
        TRACE(TEXT("TEST_FAIL : RegSetValue does not allow non-alphanumeric value names\r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: Spaces in name
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("Value with space"));
    cbData=TST_DATA_SIZE;
    dwType = 0;
    Hlp_GenRandomValData(&dwType, pbData, &cbData);
    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
    {
        TRACE(TEXT("TEST_FAIL : RegSetValue does not allow value names with spaces\r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: Value name like a path
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("Foo\\Bar"));
    cbData=TST_DATA_SIZE;
    dwType = 0;
    Hlp_GenRandomValData(&dwType, pbData, &cbData);
    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
    {
        TRACE(TEXT("TEST_FAIL : RegSetValue does not allow pathlike value names \r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: Value name starting in "\"
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("\\startslash"));
    cbData=TST_DATA_SIZE;
    dwType = 0;
    if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
        goto ErrorReturn;
    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
    {
        TRACE(TEXT("TEST_FAIL : RegSetValue does not allow value names starting with slash\r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: Value name like a path
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("EndSlash\\"));
    cbData=TST_DATA_SIZE;
    dwType = 0;
    if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
        goto ErrorReturn;
    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
    {
        TRACE(TEXT("TEST_FAIL : RegSetValue does not allow value names ending with slash\r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: Just spaces
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("    "));
    cbData=TST_DATA_SIZE;
    dwType = REG_SZ;
    if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
        goto ErrorReturn;
    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
    {
        TRACE(TEXT("TEST_FAIL : RegSetValue does not allow value names ending with slash\r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }

    StringCchCopy(rgszValName, MAX_PATH, _T("  "));
    cbData=TST_DATA_SIZE;
    dwType = REG_DWORD;
    if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
        goto ErrorReturn;
    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
    {
        TRACE(TEXT("TEST_FAIL : RegSetValue does not allow value names ending with slash\r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }

    if ((FALSE == Hlp_IsValuePresent(hKeyNew, _T("    "))) ||
        (FALSE == Hlp_IsValuePresent(hKeyNew, _T("  "))))
    {
        TRACE(TEXT("TEST_FAIL : Value with spaces does not exist\r\n"));
        goto ErrorReturn;

    }


    //
    // CASE: Value name larger than 255 chars.
    //
    if (!Hlp_GenStringData(rgszValName, 257, TST_FLAG_ALPHA))
        goto ErrorReturn;
    cbData=TST_DATA_SIZE;
    dwType = 0;
    if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
        goto ErrorReturn;

    if (ERROR_SUCCESS == (lRet=RegSetValueEx(hKeyNew, rgszValName, 0, dwType, pbData, cbData)))
    {
        TRACE(TEXT("TEST_FAIL : RegSetValue should have failed to set a key name larger than 255 chars\r\n")) ;
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: Single character name
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("A"));
    cbData=TST_DATA_SIZE;
    dwType = 0;
    if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
        goto ErrorReturn;
    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
    {
        TRACE(TEXT("TEST_FAIL : RegSetValue does not allow value names starting with slash\r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }

    dwRetVal = TPR_PASS;

ErrorReturn :
    REG_CLOSE_KEY(hKeyNew);
    RegDeleteKey(hRoot, rgszKeyName);
    FREE(pbData);
    FREE(pbDataGet);
    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// Test RegSetVal limits
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RegSetVal_Limits(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0;
    HKEY hKeyNew=0;
    TCHAR rgszKeyName[MAX_PATH]={0};
    TCHAR rgszValName[MAX_PATH]={0};
    LONG lRet=0;
    PBYTE pbData=NULL;
    DWORD cbData=TST_DATA_SIZE;
    DWORD dwType;
    int k=0;

    // The shell doesn't necessarily want us to execute the test.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    pbData = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
    CHECK_ALLOC(pbData);

    hRoot=HKEY_LOCAL_MACHINE;

    StringCchCopy(rgszKeyName, MAX_PATH, _T("tRegApiKey"));
    if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyNew, NULL)))
    {
        TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, rgszKeyName);
        REG_FAIL(RegCreateKeyEx, lRet);
        goto ErrorReturn;
    }

    // How many values can I create and delete?
    for (k=0; k<TST_LARGE_NUM_VALUES; k++)
    {
        StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
        dwType=0;
        cbData=TST_DATA_SIZE;

        if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
            goto ErrorReturn;

        if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
        {
            TRACE(TEXT("TestFailed value=%s\r\n"), rgszValName);
            goto ErrorReturn;
        }
    }
    TRACE(TEXT("Created %d keys\r\n"), k);

    for (k=0; k<TST_LARGE_NUM_VALUES; k++)
    {
        StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
        if (ERROR_SUCCESS != RegDeleteValue(hKeyNew, rgszValName))
        {
            TRACE(TEXT("Failed to delete key %s\r\n"), rgszValName);
            REG_FAIL(RegDeleteKey, lRet);
            goto ErrorReturn;
        }
     }
    TRACE(TEXT("Deleted %d values \r\n"), k);

    dwRetVal = TPR_PASS;

ErrorReturn :

    FREE(pbData);
    REG_CLOSE_KEY(hKeyNew);
    if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, rgszKeyName)))
        TRACE(TEXT("RegDeleKey could not cleanup 0x%x, file=%s, line %d\r\n"), lRet, _T(__FILE__), __LINE__);

    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// More test on RegSetVal
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RegSetVal_DataVariations(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0;
    HKEY hKeyNew=0;
    TCHAR rgszKeyName[MAX_PATH]={0};
    TCHAR rgszValName[MAX_PATH]={0};
    LONG lRet=0;
    PBYTE pbData=NULL;
    DWORD cbData=TST_DATA_SIZE;
    DWORD dwType;

    // The shell doesn't necessarily want us to execute the test.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    hRoot=HKEY_LOCAL_MACHINE;
    pbData = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
    CHECK_ALLOC(pbData);

    StringCchCopy(rgszKeyName, MAX_PATH, _T("tRegApiKey"));
    if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyNew, NULL)))
    {
        TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, rgszKeyName);
        REG_FAIL(RegCreateKeyEx, lRet);
    }


    //
    // CASE: Try setting a string without the terminating NULL character
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("NonNullTerm_String"));
    dwType = REG_SZ;
    if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
        goto ErrorReturn;
    if ((TCHAR)pbData[cbData-sizeof(TCHAR)] != _T('\0'))
        TRACE(TEXT("WARNING : returned pbData is not NULL terminated\r\n"));

    // Trample the terminating NULL
    pbData[cbData-sizeof(TCHAR)] = _T('a');
    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
    {
        TRACE(TEXT("TestFailed value=%s\r\n"), rgszValName);
        goto ErrorReturn;
    }


    //
    // CASE: Try setting a multi_sz without two terminating null chars.
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("NonNullTerm_multisz"));
    dwType=REG_MULTI_SZ;
    cbData=TST_DATA_SIZE;
    if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
        goto ErrorReturn;
    pbData[cbData-(2*sizeof(TCHAR))] = _T('1');
    pbData[cbData-sizeof(TCHAR)] = _T('2');
    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
    {
        TRACE(TEXT("TestFailed value=%s\r\n"), rgszValName);
        goto ErrorReturn;
    }


    //
    // CASE: Try setting a multi_sz with one terminating null char at the end
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("OneNullTerm_multisz"));
    dwType=REG_MULTI_SZ;
    if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
        goto ErrorReturn;
    pbData[cbData-(2*sizeof(TCHAR))] = _T('1');

    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
    {
        TRACE(TEXT("TestFailed value=%s\r\n"), rgszValName);
        goto ErrorReturn;
    }


    //
    // CASE: Try setting a one byte sz (make sure no underflow since OS will make sure
    //            any sz or multi_sz is null-terminated)
    //
    DWORD cbOneByte = 1;
    DWORD cbTemp = 4;
    StringCchCopy(rgszValName, MAX_PATH, _T("OneByte_String"));
    dwType = REG_SZ;
    if (!Hlp_GenRandomValData(&dwType, pbData, &cbTemp))
        goto ErrorReturn;

    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbOneByte))
    {
        TRACE(TEXT("TestFailed value=%s\r\n"), rgszValName);
        goto ErrorReturn;
    }


    //
    // CASE: Try setting a one byte sz with null value
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("OneByteNULL_String"));
    dwType = REG_SZ;
    pbData[0] = 0;

    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, 1))
    {
        TRACE(TEXT("TestFailed value=%s\r\n"), rgszValName);
        goto ErrorReturn;
    }


    //
    // CASE: Try setting string with unaligned WCHAR
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("UnalignedWCHAR_String"));
    dwType = REG_SZ;
    if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
        goto ErrorReturn;

    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, 15))
    {
        TRACE(TEXT("TestFailed value=%s\r\n"), rgszValName);
        goto ErrorReturn;
    }


    //
    // CASE: Try setting a multi_sz with unaligned WCHAR
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("UnalignedWCHAR_multisz"));
    dwType=REG_MULTI_SZ;
    if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
        goto ErrorReturn;

    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, 17))
    {
        TRACE(TEXT("TestFailed value=%s\r\n"), rgszValName);
        goto ErrorReturn;
    }


    //
    // CASE: Try setting a 3-byte multi_sz
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("ThreeByte_multisz"));
    dwType=REG_MULTI_SZ;

    // Set temporary data ("abc")
    pbData[0] = 97;
    pbData[1] = 98;
    pbData[2] = 99;

    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, 3))
    {
        TRACE(TEXT("TestFailed value=%s\r\n"), rgszValName);
        goto ErrorReturn;
    }


    //
    // CASE: Try setting a 3-byte multi_sz, semi null-terminated
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("ThreeByteSemiNull_multisz"));
    dwType=REG_MULTI_SZ;

    // Set temporary data ("de")
    pbData[0] = 100;
    pbData[1] = 101;
    pbData[2] = 0;

    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, 3))
    {
        TRACE(TEXT("TestFailed value=%s\r\n"), rgszValName);
        goto ErrorReturn;
    }


    //
    // CASE: Try setting a 2-byte multi_sz
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("TwoByte_multisz"));
    dwType=REG_MULTI_SZ;

    // Set temporary data ("fg")
    pbData[0] = 102;
    pbData[1] = 103;

    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, 2))
    {
        TRACE(TEXT("TestFailed value=%s\r\n"), rgszValName);
        goto ErrorReturn;
    }


    //
    // CASE: Try setting a 2-byte NULL multi_sz
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("TwoByteNULL_multisz"));
    dwType=REG_MULTI_SZ;

    pbData[0] = 0;
    pbData[1] = 0;

    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, 2))
    {
        TRACE(TEXT("TestFailed value=%s\r\n"), rgszValName);
        goto ErrorReturn;
    }


    //
    // CASE: Do NULL chars affect the way binaries are stored
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("Nully_bin"));
    dwType=REG_BINARY;
    if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
        goto ErrorReturn;
    memset(pbData+10, 0, 10);

    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
    {
        TRACE(TEXT("TestFailed value=%s\r\n"), rgszValName);
        goto ErrorReturn;
    }


    //
    // CASE: Do NULL chars affect the way REG_SZ is stored.
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("Nully_Regsz"));
    dwType=REG_SZ;
    if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
        goto ErrorReturn;
    memset(pbData+10, 0, 10);

    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
    {
        TRACE(TEXT("TestFailed value=%s\r\n"), rgszValName);
        goto ErrorReturn;
    }

    dwRetVal = TPR_PASS;

ErrorReturn :
    REG_CLOSE_KEY(hKeyNew);
    RegDeleteKey(hRoot, rgszKeyName);
    FREE(pbData);

    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// Test bad parameters
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RegSetVal_BadParams(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0;
    HKEY hKeyNew=0;
    TCHAR rgszKeyName[MAX_PATH]={0};
    TCHAR rgszValName[MAX_PATH]={0};
    LONG lRet=0;
    PBYTE pbData=NULL;
    DWORD cbData=TST_DATA_SIZE;
    DWORD dwType;
    DWORD dwTypeGet=0;
    PBYTE pbDataGet=NULL;
    DWORD cbDataGet=0;

    // The shell doesn't necessarily want us to execute the test.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    hRoot=HKEY_LOCAL_MACHINE;
    pbData = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
    CHECK_ALLOC(pbData);

    StringCchCopy(rgszKeyName, MAX_PATH, _T("tRegApiKey"));
    if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyNew, NULL)))
    {
        TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, rgszKeyName);
        REG_FAIL(RegCreateKeyEx, lRet);
        goto ErrorReturn;
    }

    StringCchCopy(rgszValName, MAX_PATH, _T("InvalidParam_Val"));
    dwType=REG_BINARY;
    if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
        goto ErrorReturn;


    //
    // CASE: Invalid hKey. This might cause an exception
    //
    TRACE(TEXT("This next test will cause an exception in filesys.exe....\r\n"));
    if (ERROR_SUCCESS == (lRet=RegSetValueEx((HKEY)-1, rgszValName, 0, dwType, pbData, cbData)))
    {
        TRACE(TEXT("TEST_FAIL : Should have failed for an invalid hKey\r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: Invalid dwType
    //
    if (ERROR_SUCCESS != (lRet=RegSetValueEx(hKeyNew, rgszValName, 0, 0xBAAD, pbData, cbData)))
    {
        TRACE(TEXT("Test_Fail : RegSetValue failed for an invalid dwType. It would pass in the past\r\n"));
        REG_FAIL(RegSetValueEx, lRet);
        goto ErrorReturn;
    }

    if (ERROR_SUCCESS != (lRet=RegQueryValueEx(hKeyNew, rgszValName, 0, &dwTypeGet, NULL, NULL)))
        REG_FAIL(RegQueryValueEx, lRet);
    if (0xbaad != dwTypeGet)
    {
        TRACE(TEXT("Test_FAIL : Set type 0xbaad, but got type 0x%x\r\n"), dwTypeGet);
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: pbData=NULL
    //

    // Set a valid value
    TRACE(TEXT("This test will cause an exception ....\r\n"));
    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
        goto ErrorReturn;
    TRACE(TEXT("Done creating a temp value\r\n"));

    dwTypeGet=0;
    if (ERROR_SUCCESS == (lRet=RegSetValueEx(hKeyNew, rgszValName, 0, dwType, NULL, cbData)))
    {
        g_dwFailCount++;
        TRACE(TEXT("TEST_FAIL : Should have failed for an invalid hKey\r\n"));
        if (ERROR_SUCCESS != (lRet=RegQueryValueEx(hKeyNew, rgszValName, 0, &dwTypeGet, NULL, &cbDataGet)))
            REG_FAIL(RegQueryValue, lRet);
        pbDataGet = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbDataGet);
        CHECK_ALLOC(pbDataGet);
        if (ERROR_SUCCESS != (lRet=RegQueryValueEx(hKeyNew, rgszValName, 0, &dwTypeGet, pbDataGet, &cbDataGet)))
            REG_FAIL(RegQueryValue, lRet);
        if ( (cbDataGet == cbData) &&
             (0==memcmp(pbDataGet, pbData, cbData)) )
            TRACE(TEXT("The value is unchanged\r\n"));
        else
            TRACE(TEXT("The value is changed\r\n"));

        DUMP_LOCN;
        goto ErrorReturn;
    }

    TRACE(TEXT("Trying it with the value deleted\r\n"));
    RegDeleteValue(hKeyNew, rgszValName);
    dwTypeGet=0;
    TRACE(TEXT("Test Test will cause exception...\r\n"));
    if (ERROR_SUCCESS == (lRet=RegSetValueEx(hKeyNew, rgszValName, 0, dwType, NULL, cbData)))
    {
        TRACE(TEXT("TEST_FAIL : Should have failed for pbData=NULL\r\n"));
        if (ERROR_SUCCESS == (lRet=RegQueryValueEx(hKeyNew, rgszValName, 0, &dwTypeGet, NULL, NULL)))
            TRACE(TEXT("RegQueryValue returned a type 0x%x\r\n"), dwTypeGet);
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: cbData=0, pbData=Valid.
    //
    RegDeleteValue(hKeyNew, rgszValName);
    if (ERROR_SUCCESS != (lRet=RegSetValueEx(hKeyNew, rgszValName, 0, dwType, pbData, 0)))
    {
        TRACE(TEXT("RegSetValueEx failed for valid pbData and cbData=0\r\n"));
        REG_FAIL(RegSetValueEx, lRet);
        goto ErrorReturn;
    }
    FREE(pbDataGet);
    cbDataGet=100;
    pbDataGet=(PBYTE)LocalAlloc(LMEM_ZEROINIT, cbDataGet);
    CHECK_ALLOC(pbDataGet);
    memset(pbDataGet, 33, cbDataGet);
    if (ERROR_SUCCESS != (lRet=RegQueryValueEx(hKeyNew, rgszValName, 0, &dwTypeGet, pbDataGet, &cbDataGet)))
    {
        REG_FAIL(RegQueryValue, lRet);
        goto ErrorReturn;
    }

    if (0 != cbDataGet)
    {
        TRACE(TEXT("TEST_FAIL : cbData set was 0, but cbDataGet =%d\r\n"), cbDataGet);
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: pbData=NULL and cbData=0, should leave the data untouched
    //
    RegDeleteValue(hKeyNew, rgszValName);
    if (ERROR_SUCCESS != (lRet=RegSetValueEx(hKeyNew, rgszValName, 0, dwType, NULL, 0)))
    {
        TRACE(TEXT("RegSetValueEx failed for valid pbData and cbData=0\r\n"));
        REG_FAIL(RegSetValueEx, lRet);
        goto ErrorReturn;
    }
    // Read the value and check what's there.
    if (ERROR_SUCCESS != (lRet=RegQueryValueEx(hKeyNew, rgszValName, 0, &dwTypeGet, NULL, &cbDataGet)))
    {
        REG_FAIL(RegQueryValue, lRet);
        goto ErrorReturn;
    }

    if (0 != cbDataGet)
    {
        TRACE(TEXT("TEST_FAIL : cbData set was 0, but cbDataGet =%d\r\n"), cbDataGet);
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: cbData=1, REG_MULTI_SZ, should return ERROR_MORE_DATA
    //
    TRACE(TEXT("Try querying REG_MULTI_SZ with one byte buffer size....\r\n"));
    dwType=REG_MULTI_SZ;
    RegDeleteValue(hKeyNew, rgszValName);
    if (ERROR_SUCCESS != (lRet=RegSetValueEx(hKeyNew, rgszValName, 0, dwType, pbData, cbData)))
    {
        TRACE(TEXT("RegSetValueEx failed for valid pbData and cbData=0\r\n"));
        REG_FAIL(RegSetValueEx, lRet);
        goto ErrorReturn;
    }
    FREE(pbDataGet);
    cbDataGet=1;
    pbDataGet=(PBYTE)LocalAlloc(LMEM_ZEROINIT, cbDataGet);
    CHECK_ALLOC(pbDataGet);
    memset(pbDataGet, 33, cbDataGet);

    if (ERROR_MORE_DATA != (lRet=RegQueryValueEx(hKeyNew, rgszValName, 0, &dwTypeGet, pbDataGet, &cbDataGet)))
    {
        REG_FAIL(RegQueryValue, lRet);
        goto ErrorReturn;
    }

    dwRetVal = TPR_PASS;

ErrorReturn :
    REG_CLOSE_KEY(hKeyNew);
    RegDeleteKey(hRoot, rgszKeyName);
    FREE(pbData);
    FREE(pbDataGet);

    return dwRetVal;
}


BOOL L_SetRandValue(HKEY hKey, DWORD dwType, LPCTSTR rgszValName)
{
    BOOL fRetVal=FALSE;
    PBYTE pbData=NULL;
    DWORD cbData=100;
    LONG lRet=0;

    pbData = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
    CHECK_ALLOC(pbData);

    if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
        goto ErrorReturn;

    // Set the value
    if (ERROR_SUCCESS != (lRet=RegSetValueEx(hKey, rgszValName, 0, dwType, pbData, cbData)))
    {
        TRACE(TEXT("TEST_FAIL : Failed RegSetValueEx %s, dwType=%d\r\n"), rgszValName, dwType);
        REG_FAIL(RegSetValueEx, lRet);
    }

    fRetVal = TRUE;

ErrorReturn :
    FREE(pbData);
    return fRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// More test on RegSetVal
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RegDeleteVal_Functional(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0;
    HKEY hKeyNew=0;
    TCHAR rgszKeyName[MAX_PATH]={0};
    TCHAR rgszValName[MAX_PATH]={0};
    DWORD ccrgValName=sizeof(rgszValName)/sizeof(TCHAR);
    LONG lRet=0;
    DWORD dwType=0;
    DWORD dwIndex=0;
    int k=0;
    DWORD dwCounter=0;

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
    // CASE: Should not be able to delete all types of values
    //            Should not be able to delete value twice
    //
    for (k=0;k<TST_NUM_VALUE_TYPES; k++)
    {
        dwType=g_rgdwRegTypes[k];
        StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
        if (!L_SetRandValue(hKeyNew, dwType, rgszValName))
        {
            DUMP_LOCN;
            goto ErrorReturn;
        }
    }
    TRACE(TEXT("Set %d values\r\n"), k);
    for (k=0;k<TST_NUM_VALUE_TYPES; k++)
    {
        StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
        if (ERROR_SUCCESS != (lRet=RegDeleteValue(hKeyNew, rgszValName)))
        {
            TRACE(TEXT("TEST_FAIL : Failed to delete the value %s\r\n"), rgszValName);
            REG_FAIL(RegDeleteKey, lRet);
        }
        if (ERROR_SUCCESS == (lRet=RegDeleteValue(hKeyNew, rgszValName)))
        {
            TRACE(TEXT("TEST_FAIL : Should not be able to delete the value %s for the second time\r\n"), rgszValName);
            DUMP_LOCN;
            goto ErrorReturn;
        }
        else
            VERIFY_REG_ERROR(RegDeleteValue, lRet, ERROR_FILE_NOT_FOUND);
    }
    TRACE(TEXT("Deleted %d values\r\n"), k);


    //
    // CASE: Can I delete the unnamed value? - should work
    //
    dwType=0;
    StringCchCopy(rgszValName, MAX_PATH, _T(""));
    if (!L_SetRandValue(hKeyNew, dwType, rgszValName))
    {
        DUMP_LOCN;
        goto ErrorReturn;
    }
    if (ERROR_SUCCESS != (lRet=RegDeleteValue(hKeyNew, rgszValName)))
    {
        TRACE(TEXT("TEST_FAIL : RegDeleteKey failed to remove the unnamed value\r\n"));
        REG_FAIL(REGDeleteValue, lRet);
    }
    if (ERROR_SUCCESS == (lRet=RegDeleteValue(hKeyNew, rgszValName)))
    {
        TRACE(TEXT("TEST_FAIL : RegDeleteKey should have failed to remove the unnamed value for the second time\r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: Delete values via enumeration
    //
    for (k=0;k<TST_NUM_VALUE_TYPES; k++)
    {
        dwType=g_rgdwRegTypes[k];
        StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
        if (!L_SetRandValue(hKeyNew, dwType, rgszValName))
        {
            DUMP_LOCN;
            goto ErrorReturn;
        }
    }
    TRACE(TEXT("Set %d values\r\n"), k);

    dwIndex=0;
    while (ERROR_SUCCESS == (lRet=RegEnumValue(hKeyNew, dwIndex, rgszValName, &ccrgValName, NULL, NULL, NULL, NULL)))
    {
        if (ERROR_SUCCESS != (lRet=RegDeleteValue(hKeyNew, rgszValName)))
        {
            TRACE(TEXT("TEST_FAIL : Failed to delete value %s\r\n"), rgszValName);
            REG_FAIL(RegDeleteValue, lRet);
        }
        ccrgValName = sizeof(rgszValName)/sizeof(TCHAR);
        dwCounter++;
    }
    if (dwCounter != (DWORD)k)
    {
        TRACE(TEXT("TEST_FAIL : Created %d values, but deleted only %d\r\n"), k, dwIndex);
        DUMP_LOCN;
        goto ErrorReturn;
    }

    dwRetVal = TPR_PASS;

ErrorReturn :
    REG_CLOSE_KEY(hKeyNew);
    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// More test on RegSetVal
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RegDeleteVal_NameVariations(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0;
    HKEY hKeyNew=0;
    TCHAR rgszKeyName[MAX_PATH]={0};
    TCHAR rgszValName[MAX_PATH]={0};
    LONG lRet=0;
    DWORD dwType=0;
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
    // CASE: Case sensitivity
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("UPPERCASE"));
    dwType=0;
    if (!L_SetRandValue(hKeyNew, dwType, rgszValName))
        goto ErrorReturn;

    StringCchLength(rgszValName, STRSAFE_MAX_CCH, &szlen);
    CharLowerBuff(rgszValName,szlen);
    if (ERROR_SUCCESS != (lRet=RegDeleteValue(hKeyNew, rgszValName)))
    {
        TRACE(TEXT("TEST_FAIL : Failed to delete value %s\r\n"), rgszValName);
        REG_FAIL(RegDeleteValue, lRet);
    }
    szlen=0;
    StringCchLength(rgszValName, STRSAFE_MAX_CCH, &szlen);
    CharUpperBuff(rgszValName,szlen);
    if (ERROR_SUCCESS == (lRet=RegDeleteValue(hKeyNew, rgszValName)))
    {
        TRACE(TEXT("TEST_FAIL : Should have Failed to delete value %s\r\n"), rgszValName);
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: Space value name
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("    "));
    dwType=0;
    if (!L_SetRandValue(hKeyNew, dwType, rgszValName))
        goto ErrorReturn;

    StringCchCopy(rgszValName, MAX_PATH, _T("     "));
    if (ERROR_SUCCESS == (lRet=RegDeleteValue(hKeyNew, rgszValName)))
    {
        TRACE(TEXT("TEST_FAIL : Should have failed to delete value %s (5 spaces) \r\n"), rgszValName);
        DUMP_LOCN;
        goto ErrorReturn;
    }
    StringCchCopy(rgszValName, MAX_PATH, _T("    "));
    if (ERROR_SUCCESS != (lRet=RegDeleteValue(hKeyNew, rgszValName)))
    {
        TRACE(TEXT("TEST_FAIL : Failed to delete value %s (4 spaces)\r\n"), rgszValName);
        REG_FAIL(RegDeleteValue, lRet);
    }


    //
    // CASE: Value name ending in a "\"
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("Foo\\"));
    dwType=0;
    if (!L_SetRandValue(hKeyNew, dwType, rgszValName))
        goto ErrorReturn;

    if (ERROR_SUCCESS != (lRet=RegDeleteValue(hKeyNew, rgszValName)))
    {
        TRACE(TEXT("TEST_FAIL : Failed to delete value %s \r\n"), rgszValName);
        REG_FAIL(RegDeleteValue, lRet);
    }


    //
    // CASE: Value name beginning with a "\"
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("\\Foo"));
    dwType=0;
    if (!L_SetRandValue(hKeyNew, dwType, rgszValName))
        goto ErrorReturn;

    if (ERROR_SUCCESS != (lRet=RegDeleteValue(hKeyNew, rgszValName)))
    {
        TRACE(TEXT("TEST_FAIL : Failed to delete value %s \r\n"), rgszValName);
        REG_FAIL(RegDeleteValue, lRet);
    }


    //
    // CASE: Numeric value name
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("123456"));
    dwType=0;
    if (!L_SetRandValue(hKeyNew, dwType, rgszValName))
        goto ErrorReturn;

    if (ERROR_SUCCESS != (lRet=RegDeleteValue(hKeyNew, rgszValName)))
    {
        TRACE(TEXT("TEST_FAIL : Failed to delete value %s \r\n"), rgszValName);
        REG_FAIL(RegDeleteValue, lRet);
    }


    //
    // CASE: Alpha-Numeric value name
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("1a2b3c4d"));
    dwType=0;
    if (!L_SetRandValue(hKeyNew, dwType, rgszValName))
        goto ErrorReturn;

    if (ERROR_SUCCESS != (lRet=RegDeleteValue(hKeyNew, rgszValName)))
    {
        TRACE(TEXT("TEST_FAIL : Failed to delete value %s \r\n"), rgszValName);
        REG_FAIL(RegDeleteValue, lRet);
    }


    //
    // CASE: Non Alpha-Numeric value name
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("@#$%^&*"));
    dwType=0;
    if (!L_SetRandValue(hKeyNew, dwType, rgszValName))
        goto ErrorReturn;

    if (ERROR_SUCCESS != (lRet=RegDeleteValue(hKeyNew, rgszValName)))
    {
        TRACE(TEXT("TEST_FAIL : Failed to delete value %s \r\n"), rgszValName);
        REG_FAIL(RegDeleteValue, lRet);
    }


    //
    // CASE: Special names (like COM1:)
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("COM1:"));
    dwType=0;
    if (!L_SetRandValue(hKeyNew, dwType, rgszValName))
        goto ErrorReturn;

    if (ERROR_SUCCESS != (lRet=RegDeleteValue(hKeyNew, rgszValName)))
    {
        TRACE(TEXT("TEST_FAIL : Failed to delete value %s \r\n"), rgszValName);
        REG_FAIL(RegDeleteValue, lRet);
    }

    dwRetVal = TPR_PASS;

ErrorReturn :
    REG_CLOSE_KEY(hKeyNew);
    RegDeleteKey(hRoot, rgszKeyName);
    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// Stress test
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RegDeleteVal_Stress(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0;
    HKEY hKeyNew=0;
    TCHAR rgszKeyName[MAX_PATH]={0};
    TCHAR rgszValName[MAX_PATH]={0};
    LONG lRet=0;
    int k=0;
    PBYTE pbData=NULL;
    DWORD cbData=2*1024;
    DWORD dwType=0;

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

    pbData = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
    CHECK_ALLOC(pbData);

    StringCchCopy(rgszValName, MAX_PATH, _T("StressVal"));
    dwType=REG_BINARY;
    if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
        goto ErrorReturn;
    TRACE(TEXT("Creating and deleteing 2000 values.......\r\n"));

    TRACE(TEXT("Phy Mem before = %d\r\n"), Hlp_GetAvailMem_Physical());
    TRACE(TEXT("Vir Mem Before = %d\r\n"), Hlp_GetAvailMem_Virtual());

    for (k=0; k<8000; k++)
    {
        if (ERROR_SUCCESS != (lRet=RegSetValueEx(hKeyNew, rgszValName, 0, dwType, pbData, cbData)))
        {
            TRACE(TEXT("TEST_FAIL : Failed RegSetValueEx %s, dwType=%d\r\n"), rgszValName, dwType);
            REG_FAIL(RegSetValueEx, lRet);
        }

        if (ERROR_SUCCESS != (lRet=RegDeleteValue(hKeyNew, rgszValName)))
        {
            TRACE(TEXT("TEST_FAIL : Failed to delete value %s \r\n"), rgszValName);
            REG_FAIL(RegDeleteValue, lRet);
        }
    }

    TRACE(TEXT("Phy Mem after = %d\r\n"), Hlp_GetAvailMem_Physical());
    TRACE(TEXT("Vir Mem after = %d\r\n"), Hlp_GetAvailMem_Virtual());

    dwRetVal = TPR_PASS;

ErrorReturn :
    REG_CLOSE_KEY(hKeyNew);
    RegDeleteKey(hRoot, rgszKeyName);
    FREE(pbData);
    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// Test bad parameters
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RegDeleteVal_BadParams(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0;
    HKEY hKeyNew=0;
    TCHAR rgszKeyName[MAX_PATH]={0};
    TCHAR rgszValName[MAX_PATH]={0};
    LONG lRet=0;

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

    StringCchCopy(rgszValName, MAX_PATH, _T("RandomValue"));
    if (!L_SetRandValue(hKeyNew, 0, rgszValName))
        goto ErrorReturn;


    //
    // CASE: Invalid Hkey
    //
    TRACE(TEXT("The next test will cause an exception in filesys..... \r\n"));
    if (ERROR_SUCCESS == (lRet=RegDeleteValue((HKEY)-1, rgszValName)))
    {
        TRACE(TEXT("TEST_FAIL : Should have failed RegDeleteKey for an invalid HKey \r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }

    dwRetVal = TPR_PASS;

ErrorReturn :
    REG_CLOSE_KEY(hKeyNew);
    RegDeleteKey(hRoot, rgszKeyName);
    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// More test on RegQueryVal
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RegQueryValue_hKey(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
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
    DWORD cbData=512;

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
        StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
        if (!L_SetRandValue(hKeyNew, dwType, rgszValName))
        {
            DUMP_LOCN;
            goto ErrorReturn;
        }
    }
    TRACE(TEXT("Set %d values\r\n"), k);


    //
    // CASE: Should just work for a valid hKey
    //
    for (k=0;k<TST_NUM_VALUE_TYPES; k++)
    {
        StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
        cbData=512;
        if (ERROR_SUCCESS != (lRet=RegQueryValueEx(hKeyNew, rgszValName, NULL, &dwType, pbData, &cbData)))
        {
            TRACE(TEXT("TEST_FAIL : RegQueryValueEx failed valName=%s, error=0x%x\r\n"), rgszValName, lRet);
            goto ErrorReturn;
        }
    }


    //
    // CASE: Invalid hKey - should fail.
    //
    cbData=512;
    if (ERROR_SUCCESS == (lRet=RegQueryValueEx((HKEY)0, rgszValName, NULL, &dwType, pbData, &cbData)))
    {
        TRACE(TEXT("TEST_FAIL : RegQueryValueEx should have failed for an invalid hKey\r\n"));
        goto ErrorReturn;
    }

    TRACE(TEXT("The next test will cause an exception\r\n"));
    if (ERROR_SUCCESS == (lRet=RegQueryValueEx((HKEY)1, rgszValName, NULL, &dwType, pbData, &cbData)))
    {
        TRACE(TEXT("TEST_FAIL : RegQueryValueEx should have failed for an invalid hKey\r\n"));
        goto ErrorReturn;
    }

    TRACE(TEXT("The next test will cause an exception\r\n"));
    if (ERROR_SUCCESS == (lRet=RegQueryValueEx((HKEY)-1, rgszValName, NULL, &dwType, pbData, &cbData)))
    {
        TRACE(TEXT("TEST_FAIL : RegQueryValueEx should have failed for an invalid hKey\r\n"));
        goto ErrorReturn;
    }


    //
    // CASE: Should fail for a hKey that's been closed.
    //
    lRet=RegCloseKey(hKeyNew);
    if (lRet)
        REG_FAIL(RegCloseKey, lRet);

    if (ERROR_SUCCESS == (lRet=RegQueryValueEx(hKeyNew, rgszValName, NULL, &dwType, pbData, &cbData)))
    {
        TRACE(TEXT("TEST_FAIL : RegQueryValueEx should have failed for hKey that's been closed\r\n"));
        goto ErrorReturn;
    }
    hKeyNew=0;

    dwRetVal = TPR_PASS;

ErrorReturn :
    REG_CLOSE_KEY(hKeyNew);
    FREE(pbData);
    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// More test on RegQueryVal
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RegQueryValue_ValName(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
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
    DWORD cbData=512;
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

    // Create all the values, one per type
    for (k=0;k<TST_NUM_VALUE_TYPES; k++)
    {
        dwType=g_rgdwRegTypes[k];
        StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
        if (!L_SetRandValue(hKeyNew, dwType, rgszValName))
        {
            DUMP_LOCN;
            goto ErrorReturn;
        }
    }
    TRACE(TEXT("Set %d values\r\n"), k);

    // Set the default value
    StringCchCopy(rgszValName, MAX_PATH, _T("Default_Value"));
    StringCchLength(rgszValName, STRSAFE_MAX_CCH, &szlen);
    lRet=RegSetValueEx(hKeyNew, _T(""), 0, REG_SZ, (PBYTE)rgszValName, (szlen+1)*sizeof(TCHAR));
    if (lRet)
    {
        TRACE(TEXT("TEST_FAIL : Failed to set default value \r\n"));
        REG_FAIL(RegSetValueEx, lRet);
    }


    //
    // CASE: Should work for a correct value name
    //
    // Done in the Tst_RegQueryValue_hKey

    //
    // CASE: if pszValue is NULL, should return the default value
    //
    cbData=512;
    if (ERROR_SUCCESS != (lRet=RegQueryValueEx(hKeyNew, NULL, NULL, &dwType, pbData, &cbData)))
    {
        TRACE(TEXT("TEST_FAIL : RegQueryValueEx failed to get the default value\r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }
    if ((REG_SZ != dwType) ||
        (_tcscmp((LPCTSTR)pbData, _T("Default_Value"))))
    {
        TRACE(TEXT("TEST_FAIL : Got the default value, but it's not what was expected. dwType=%d, val=%s\r\n"), dwType, (LPTSTR)pbData);
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: Should fail for non-existant value
    //
    cbData=512;
    StringCchCopy(rgszValName, MAX_PATH, _T("non_existant"));
    if (ERROR_SUCCESS == (lRet=RegQueryValueEx(hKeyNew, rgszValName, NULL, &dwType, pbData, &cbData)))
    {
        TRACE(TEXT("TEST_FAIL : RegQueryValueEx should have failed to get a non-existant value=%s\r\n"), rgszValName);
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: Case-sensitive test
    //
    for (k=0;k<TST_NUM_VALUE_TYPES; k++)
    {
        StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
        cbData=512;
        if (ERROR_SUCCESS != (lRet=RegQueryValueEx(hKeyNew, rgszValName, NULL, &dwType, pbData, &cbData)))
        {
            TRACE(TEXT("TEST_FAIL : RegQueryValueEx failed to get uppercase value=%s\r\n"), rgszValName);
            REG_FAIL(RegQueryValueEx, lRet);
        }

        StringCchPrintf(rgszValName,MAX_PATH,_T("VALUE_%d"), k);
        cbData=512;
        if (ERROR_SUCCESS != (lRet=RegQueryValueEx(hKeyNew, rgszValName, NULL, &dwType, pbData, &cbData)))
        {
            TRACE(TEXT("TEST_FAIL : RegQueryValueEx failed to get uppercase value=%s\r\n"), rgszValName);
            REG_FAIL(RegQueryValueEx, lRet);
        }
        if (REG_NONE == g_rgdwRegTypes[k])
            continue;
        if (dwType != g_rgdwRegTypes[k])
        {
            TRACE(TEXT("TEST_FAIL : Did not get the expected type. Expecting %d, got %d\r\n"), g_rgdwRegTypes[k], dwType);
            DUMP_LOCN;
            goto ErrorReturn;
        }
    }


    //
    // CASE: if pszValue is "", should return the default value
    //
    cbData=512;
    StringCchCopy(rgszValName, MAX_PATH, _T(""));
    if (ERROR_SUCCESS != (lRet=RegQueryValueEx(hKeyNew, rgszValName, NULL, &dwType, pbData, &cbData)))
    {
        TRACE(TEXT("TEST_FAIL : RegQueryValueEx failed to get the default value\r\n"));
        REG_FAIL(RegQueryValueEx, lRet);
    }
    if ((REG_SZ != dwType) ||
        (_tcscmp((LPCTSTR)pbData, _T("Default_Value"))))
    {
        TRACE(TEXT("TEST_FAIL : Got the default value, but it's not what was expected. dwType=%d, val=%s\r\n"), dwType, (LPTSTR)pbData);
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: Should fail if there is no default value
    //
    lRet=RegDeleteValue(hKeyNew, _T(""));
    if (lRet)
    {
        TRACE(TEXT("Test_FAIL : Failed to delete the default value\r\n"));
        REG_FAIL(RegDeleteValue, lRet);
    }
    if (ERROR_SUCCESS == (lRet=RegQueryValueEx(hKeyNew, rgszValName, NULL, &dwType, pbData, &cbData)))
    {
        TRACE(TEXT("TEST_FAIL : RegQueryValueEx should have failed to get a non-existant default value\r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: Non-Null terminated value name
    //
    memset(rgszValName, 65, sizeof(rgszValName));
    StringCchPrintf(rgszValName,MAX_PATH,_T("VALUE_%d"), 0);
    StringCchLength(rgszValName, STRSAFE_MAX_CCH, &szlen);
    rgszValName[szlen] = _T('A');
    if (ERROR_SUCCESS == (lRet=RegQueryValueEx(hKeyNew, rgszValName, NULL, &dwType, pbData, &cbData)))
    {
        TRACE(TEXT("TEST_FAIL : RegQueryValueEx should have failed for a non-NULL terminated val names\r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }


    //
    // CASE: lpReserved should be ignored
    //
    StringCchPrintf(rgszValName,MAX_PATH,_T("VALUE_%d"), 0);
    k=5959;
    if (ERROR_SUCCESS == (lRet=RegQueryValueEx(hKeyNew, rgszValName, &k, &dwType, pbData, &cbData)))
    {
        TRACE(TEXT("TEST_FAIL : RegQueryValueEx should have failed for a non-NULL terminated val names\r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }
    if (k!= 5959)
    {
        TRACE(TEXT("TEST_FAIL : The lpReserved value should have been ignored. It changed from 5959 to %d\r\n"), k);
        DUMP_LOCN;
        goto ErrorReturn;
    }

    dwRetVal = TPR_PASS;

ErrorReturn :
    FREE(pbData);
    REG_CLOSE_KEY(hKeyNew);
    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// More test on RegQueryVal
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RegQueryValue_Type(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
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
    DWORD cbData=512;

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
        StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);

        if (!L_SetRandValue(hKeyNew, dwType, rgszValName))
        {
            DUMP_LOCN;
            goto ErrorReturn;
        }
    }
    TRACE(TEXT("Set %d values\r\n"), k);


    //
    // CASE: Non regular types
    //
    dwType = REG_BINARY;
    StringCchCopy(rgszValName, MAX_PATH, _T("UserTypeVal"));
    cbData=512;
    Hlp_GenRandomValData(&dwType, pbData, &cbData);

    dwType = 999;
    if (!L_SetAndGetValue(hKeyNew, dwType, rgszValName, pbData, cbData))
        goto ErrorReturn;


    //
    // CASE: Does it work if &dwType=NULL?
    //
    StringCchCopy(rgszValName, MAX_PATH, _T("Value_1"));
    cbData=512;
    if (ERROR_SUCCESS != (lRet=RegQueryValueEx(hKeyNew, rgszValName, NULL, NULL, pbData, &cbData)))
    {
        TRACE(TEXT("TEST_FAIL : RegQueryValueEx failed for dwType=NULL\r\n"));
        REG_FAIL(RegQueryValueEx, lRet);
    }

    dwRetVal = TPR_PASS;

ErrorReturn :
    FREE(pbData);
    REG_CLOSE_KEY(hKeyNew);
    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// More test on RegQueryVal
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RegQueryValue_pbcb(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
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
    DWORD cbData=512;

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
        StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
        if (!L_SetRandValue(hKeyNew, dwType, rgszValName))
        {
            DUMP_LOCN;
            goto ErrorReturn;
        }
    }
    TRACE(TEXT("Set %d values\r\n"), k);


    //
    // CASE: pbData=NULL, cbData=0;
    //
    for (k=0;k<TST_NUM_VALUE_TYPES; k++)
    {
        StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
        cbData=0;
        if (ERROR_SUCCESS != (lRet=RegQueryValueEx(hKeyNew, rgszValName, NULL, &dwType, NULL, &cbData)))
        {
            TRACE(TEXT("TEST_FAIL : RegQueryValueEx failed for pbData=NULL\r\n"));
            REG_FAIL(RegQueryValueEx, lRet);
        }
    }


    //
    // CASE: pbDAta and cbData are smaller. Should fail with ERROR_MORE_DATA
    //
    for (k=0;k<TST_NUM_VALUE_TYPES; k++)
    {
        StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
        cbData=0;
        if (ERROR_SUCCESS != (lRet=RegQueryValueEx(hKeyNew, rgszValName, NULL, &dwType, NULL, &cbData)))
        {
            TRACE(TEXT("TEST_FAIL : RegQueryValueEx failed for pbData=NULL\r\n"));
            REG_FAIL(RegQueryValueEx, lRet);
        }

        memset(pbData, 65, 512);
        cbData -= 2;
        if (ERROR_SUCCESS == (lRet=RegQueryValueEx(hKeyNew, rgszValName, NULL, &dwType, pbData, &cbData)))
        {
            TRACE(TEXT("TEST_FAIL : RegQueryValueEx should have failed for for a small pbData\r\n"));
            REG_FAIL(RegQueryValueEx, lRet);
        }
        VERIFY_REG_ERROR(RegQueryValueEx, lRet, ERROR_MORE_DATA);

        //  Checking BO
        if ((65 != pbData[cbData-1]) &&
            (65 != pbData[cbData-2]))
        {
            TRACE(TEXT("TEST_FAIL : Potential buffer overrun. Please investigate\r\n"));
            TRACE(TEXT("Expecting the last 2 bytes of pbData to be 65, instead got 0x%x and 0x%x\r\n"), pbData[cbData-1], pbData[cbData-2]);
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

