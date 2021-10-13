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


///////////////////////////////////////////////////////////////////////////////
//
// Build verification test for registry enumeration APIs
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_EnumKeyBVT(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0;
    HKEY hKeyParent=0, hKeyChild=0;
    TCHAR rgszKeyName[MAX_PATH]={65};
    DWORD ccKeyName=0;
    TCHAR rgszClass[MAX_PATH]={66};
    DWORD ccClass=0;
    LONG lRet=0;
    DWORD i=0, k=0;
    DWORD dwNumKeys=30;
    DWORD dwIndex=0;
    FILETIME FT_LastWrite;
    WCHAR szClassTemp[MAX_PATH];
    size_t szlen = 0;
    StringCchCopy(szClassTemp, MAX_PATH, TREGAPI_CLASS);	

    // The shell doesn't necessarily want us to execute the test. 
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    for (i=0; i<TST_NUM_ROOTS; i++)
    {
        hRoot=g_l_RegRoots[i];
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);

        WCHAR szKeyTemp[MAX_PATH];		
        StringCchCopy(szKeyTemp, MAX_PATH, _T("Test_RegEnumKey"));		
        
        lRet = RegCreateKeyEx(hRoot, szKeyTemp, NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyParent, NULL);
        if (lRet)
            REG_FAIL(RegCreateKeyEx, lRet);

        // Create 30 keys under here. 
        for (k=0; k<dwNumKeys; k++)
        {
            StringCchPrintf(rgszKeyName,MAX_PATH,_T("Key_%d"), k);

            lRet = RegCreateKeyEx(hKeyParent, rgszKeyName, NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyChild, NULL);
            if (lRet)
                REG_FAIL(RegCreateKeyEx, lRet);    
            REG_CLOSE_KEY(hKeyChild);
        }
        
        // Enumerate all the keys under here
        dwIndex=0;
        memset(rgszKeyName, 65, sizeof(rgszKeyName));
        ccKeyName=sizeof(rgszKeyName)/sizeof(TCHAR);
        memset(rgszClass, 66, sizeof(rgszClass));
        ccClass = sizeof(rgszClass)/sizeof(TCHAR);
        while (ERROR_SUCCESS == (lRet=RegEnumKeyEx(hKeyParent, dwIndex, rgszKeyName, &ccKeyName, NULL, rgszClass, &ccClass, &FT_LastWrite)))
        {
            TRACE(TEXT("Enumerated key=%s, class=%s\r\n"), rgszKeyName, rgszClass);

            // Check rgszClass
            if (_tcscmp(rgszClass, TREGAPI_CLASS))
            {
                TRACE(TEXT("TEST_FAIL: Key was created as class=%s, but enumeration returned class=%s\r\n"), TREGAPI_CLASS, rgszClass);
                goto ErrorReturn;
            }
            
            // Check ccKeyName
            StringCchLength(rgszKeyName, STRSAFE_MAX_CCH, &szlen);
            if (ccKeyName != szlen)
            {
                TRACE(TEXT("TEST_FAIL: ccKeyName=%d, should be _tcslen(rgszClass)=%d\r\n"), ccKeyName, szlen);
                goto ErrorReturn;
            }

            // Check ccClass
            szlen=0;
            StringCchLength(rgszClass, STRSAFE_MAX_CCH, &szlen);
            if (ccClass != szlen)
            {
                TRACE(TEXT("TEST_FAIL: ccClass=%d, should be _tcslen(rgszClass)=%d\r\n"), ccClass, szlen);
                goto ErrorReturn;
            }

            // Reset
            dwIndex++;            
            memset(rgszKeyName, 65, sizeof(rgszKeyName));
            ccKeyName=sizeof(rgszKeyName)/sizeof(TCHAR);
            memset(rgszClass, 66, sizeof(rgszClass));
            ccClass = sizeof(rgszClass)/sizeof(TCHAR);
        }

        // Check the num of keys enumerated.
        if (dwIndex != dwNumKeys)
        {
            TRACE(TEXT("TEST_FAIL: Created %d keys, but enumerated %d\r\n"), dwNumKeys, dwIndex);
            goto ErrorReturn;
        }

        // Check that lRet is ERROR_NO_MORE_ITEMS
        if (lRet != ERROR_NO_MORE_ITEMS)
        {
            TRACE(TEXT("TestFail : Expecting RegEnumKeyEx to end with return value ERROR_NO_MORE_ITEMS. Instead got 0x%x\r\n"), lRet);
            goto ErrorReturn;
        }
        
        REG_CLOSE_KEY(hKeyParent);        
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);
    }

    dwRetVal = TPR_PASS;
    
ErrorReturn :
    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// Test enumerating registry key
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_EnumKey_hKey(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0;
    HKEY hKeyNew=0;
    TCHAR rgszKeyName[MAX_PATH]={0};
    DWORD ccKeyName=0;
    TCHAR rgszClass[MAX_PATH]={0};
    DWORD ccClass=0;
    LONG lRet=0;
    DWORD dwIndex=0;
    DWORD dwNumKeys=0;
    DWORD i=0;

    // The shell doesn't necessarily want us to execute the test.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }


    //
    // CASE: Make sure it works for predefined roots. 
    //
    for (i=0; i<TST_NUM_ROOTS; i++)
    {
        hRoot = g_l_RegRoots[i];
        if (!Hlp_GetNumSubKeys(hRoot, &dwNumKeys))
            goto ErrorReturn;
        
        dwIndex=0;
        ccKeyName = sizeof(rgszKeyName)/sizeof(TCHAR);
        ccClass = sizeof(rgszClass)/sizeof(TCHAR);
        while (ERROR_SUCCESS == (lRet=RegEnumKeyEx(hRoot, dwIndex, rgszKeyName, &ccKeyName, NULL, rgszClass, &ccClass, NULL)))
        {
            dwIndex++;
            ccKeyName = sizeof(rgszKeyName)/sizeof(TCHAR);
            ccClass = sizeof(rgszClass)/sizeof(TCHAR);    
        }

        VERIFY_REG_ERROR(RegEnumKeyEx, lRet, ERROR_NO_MORE_ITEMS);
        if (dwNumKeys != dwIndex)
        {
            TRACE(TEXT("TEST_FAIL : expecting %d keys, but only %d were enumerated\r\n"), dwNumKeys, dwIndex);
            goto ErrorReturn;
        }
    }


    //
    // CASE: Try an invalid hKey = -1 
    //
    TRACE(TEXT("The next test will cause exception in filesys\r\n"));
    hRoot=(HKEY)-1;
    ccKeyName=sizeof(rgszKeyName)/sizeof(TCHAR);
    ccClass=sizeof(rgszClass)/sizeof(TCHAR);
    if (ERROR_SUCCESS == (lRet=RegEnumKeyEx(hRoot, 0, rgszKeyName, &ccKeyName, NULL, rgszClass, &ccClass, NULL)))
    {
        TRACE(TEXT("TEST_FAIL : Test is expecting RegEnumKeyEx to fail because of invalid hKey, but it did not\r\n"));
        goto ErrorReturn;
    }


    //
    // CASE: Try another invalid hKey = 0
    //
    hRoot=(HKEY)0;
    dwIndex=0;
    ccKeyName=sizeof(rgszKeyName)/sizeof(TCHAR);
    ccClass=sizeof(rgszClass)/sizeof(TCHAR);
    if (ERROR_SUCCESS == (lRet=RegEnumKeyEx(hRoot, 0, rgszKeyName, &ccKeyName, NULL, rgszClass, &ccClass, NULL)))
    {
        TRACE(TEXT("TEST_FAIL : Test is expecting RegEnumKeyEx to fail because of invalid hKey, but it did not\r\n"));
        goto ErrorReturn;
    }

    dwRetVal = TPR_PASS;
    
ErrorReturn :
    REG_CLOSE_KEY(hKeyNew);
    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// Test enumerating index
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_EnumKey_index(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0;
    HKEY hKeyParent=0, hKeyChild=0;
    TCHAR rgszKeyName[MAX_PATH]={0};
    TCHAR rgszKeyName_2[MAX_PATH]={0};
    DWORD ccKeyName=0;
    TCHAR rgszClass[MAX_PATH]={0};
    DWORD ccClass=0;
    LONG lRet=0;
    DWORD dwIndex=0;
    DWORD k=0, dwRoot=0;
    static DWORD dwNumKeys=30;
    BOOL fDone=FALSE;
    BOOL fFoundKey[30]={0};
    DWORD dwFoundIndex=0;
    size_t szlen = 0;
    DWORD dwKeysExpected=0;

    // The shell doesn't necessarily want us to execute the test. 
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    for (dwRoot=0; dwRoot<TST_NUM_ROOTS; dwRoot++)
    {
        hRoot=g_l_RegRoots[dwRoot];

        WCHAR szKeyTemp[MAX_PATH];
        WCHAR szClassTemp[MAX_PATH];
        
        StringCchCopy(szKeyTemp, MAX_PATH, _T("Test_RegEnumKey"));				
        StringCchCopy(szClassTemp, MAX_PATH, TREGAPI_CLASS);	
        
        lRet = RegDeleteKey(hRoot, szKeyTemp);
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);

        StringCchCopy(szKeyTemp, MAX_PATH, _T("Test_RegEnumKey"));
        lRet = RegCreateKeyEx(hRoot, szKeyTemp, NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyParent, NULL);
        if (lRet)
            REG_FAIL(RegCreateKeyEx, lRet);

        // Create 30 keys under here. 
        dwNumKeys=30;
        for (k=0; k<dwNumKeys; k++)
        {
            StringCchPrintf(rgszKeyName,MAX_PATH,_T("Key_%d"), k);
            lRet = RegCreateKeyEx(hKeyParent, rgszKeyName, NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyChild, NULL);
            if (lRet)
                REG_FAIL(RegCreateKeyEx, lRet);    
            REG_CLOSE_KEY(hKeyChild);
            fFoundKey[k]=FALSE;    //  just reset.
        }


        //
        // CASE : Good rgszClass, bad ccClass
        //		
        ccKeyName = sizeof(rgszKeyName)/sizeof(TCHAR);
        ccClass=0;        //  bad ccClass
        if (ERROR_SUCCESS != (lRet=RegEnumKeyEx(hRoot, dwIndex, rgszKeyName, &ccKeyName, NULL, rgszClass, &ccClass, NULL)))
        {
            TRACE(TEXT("RegEnumKeyEx failed as expected\r\n"));
        }

        ccKeyName = 0; //  bad ccKeyName
        ccClass = sizeof(rgszClass)/sizeof(TCHAR);    
        if (ERROR_SUCCESS != (lRet=RegEnumKeyEx(hRoot, dwIndex, rgszKeyName, &ccKeyName, NULL, rgszClass, &ccClass, NULL)))
        {
            TRACE(TEXT("RegEnumKeyEx failed as expected\r\n"));
        }

        
        //
        // CASE: Make sure dwIndex=0 resets the enumeration
        //
        ccKeyName = sizeof(rgszKeyName)/sizeof(TCHAR);
        ccClass = sizeof(rgszClass)/sizeof(TCHAR);    
        dwIndex=0;
        fDone=FALSE;
        if (!Hlp_GetNumSubKeys(hKeyParent, &dwKeysExpected))
            goto ErrorReturn;
        while (ERROR_SUCCESS == (lRet=RegEnumKeyEx(hKeyParent, dwIndex, rgszKeyName, &ccKeyName, NULL, rgszClass, &ccClass, NULL)))
        {
            dwIndex++;
            ccKeyName = sizeof(rgszKeyName)/sizeof(TCHAR);
            ccClass = sizeof(rgszClass)/sizeof(TCHAR);  
            if (_tcsncmp(rgszKeyName, _T("Key_"), 4))
                continue;
            StringCchLength(_T("Key_"), STRSAFE_MAX_CCH, &szlen);
            dwFoundIndex = _ttoi(rgszKeyName + szlen);

            if (dwFoundIndex >= 30)
                break;
            
            fFoundKey[dwFoundIndex]=TRUE;
            if ((dwIndex>=5) && (!fDone))
            {
                dwIndex=0;
                fDone=TRUE;
                memset(fFoundKey, 0, sizeof(fFoundKey));   //  set everything to false.
            }
        }
        VERIFY_REG_ERROR(RegEnumKeyEx, lRet, ERROR_NO_MORE_ITEMS);
        if (dwKeysExpected != dwIndex)
        {
            TRACE(TEXT("TEST_FAIL : expecting %d keys, but only %d were enumerated\r\n"), dwKeysExpected, dwIndex);
            DUMP_LOCN;
            goto ErrorReturn;
        }
        // Verify that all the keys were enummerated
        for (k=0; k<dwNumKeys; k++)
        {
            if (FALSE == fFoundKey[k])
            {
                TRACE(TEXT("TEST_FAIL : Key_%d wasn't enumerated. Investigate\r\n"), k);
                DUMP_LOCN;
                goto ErrorReturn;
            }
        }


        //
        // CASE: Make sure same dwindex returns the same data
        //
        for (dwIndex=0; dwIndex<dwNumKeys; dwIndex++)
        {
            ccKeyName = sizeof(rgszKeyName)/sizeof(TCHAR);
            ccClass = sizeof(rgszClass)/sizeof(TCHAR);    
            if (ERROR_SUCCESS != (lRet=RegEnumKeyEx(hKeyParent, dwIndex, rgszKeyName, &ccKeyName, NULL, rgszClass, &ccClass, NULL)))
                REG_FAIL(RegEnumKeyEx, lRet);

            // Enumerate the same key 20 times.
            for (k=0; k<20; k++)
            {
                ccKeyName = sizeof(rgszKeyName_2)/sizeof(TCHAR);
                ccClass = sizeof(rgszClass)/sizeof(TCHAR);    
                if (ERROR_SUCCESS != (lRet=RegEnumKeyEx(hKeyParent, dwIndex, rgszKeyName_2, &ccKeyName, NULL, rgszClass, &ccClass, NULL)))
                    REG_FAIL(RegEnumKeyEx, lRet);
                if (_tcscmp(rgszKeyName, rgszKeyName_2))
                {
                    TRACE(TEXT("TEST_FAIL : Enumerating dwIndex=%d, returned 2 different KeyValues %s and %s\r\n"), 
                                    dwIndex, rgszKeyName, rgszKeyName_2);
                    DUMP_LOCN;
                    goto ErrorReturn;
                }
            }
        }


        //
        // CASE: Can I enumerate with dwIndex randomly betn 0 and dwNumKeys? Try this 100 times
        //
        for (k=0; k<100; k++)
        {
            ccKeyName = sizeof(rgszKeyName)/sizeof(TCHAR);
            ccClass = sizeof(rgszClass)/sizeof(TCHAR);    
            dwIndex=Random()%dwNumKeys;
            if (ERROR_SUCCESS != (lRet=RegEnumKeyEx(hKeyParent, dwIndex, rgszKeyName, &ccKeyName, NULL, rgszClass, &ccClass, NULL)))
            {
                TRACE(TEXT("TEST_FAIL : Failed to enumerate for dwIndex=%d (k=%d)\r\n"), dwIndex, k);
                REG_FAIL(RegEnumKeyEx, lRet);
            }
        }

        
        //
        // CASE: Make sure the call fails for dwIndex=dwNumKeys
        //
        dwIndex = dwNumKeys;
        ccKeyName = sizeof(rgszKeyName)/sizeof(TCHAR);
        ccClass = sizeof(rgszClass)/sizeof(TCHAR);    
        if (ERROR_SUCCESS == (lRet=RegEnumKeyEx(hKeyParent, dwIndex, rgszKeyName, &ccKeyName, NULL, rgszClass, &ccClass, NULL)))
        {
            TRACE(TEXT("TEST_FAIL : RegEnumKeyEx should have failed for dwIndex=%d \r\n"), dwIndex);
            REG_FAIL(RegEnumKeyEx, lRet);
        }


        //
        // CASE: Make sure I can decrement and enumerate.
        //
        ccKeyName = sizeof(rgszKeyName)/sizeof(TCHAR);
        ccClass = sizeof(rgszClass)/sizeof(TCHAR);    
        if (!Hlp_GetNumSubKeys(hKeyParent, &dwKeysExpected))
            goto ErrorReturn;
        dwIndex = dwKeysExpected-1;
        while (ERROR_SUCCESS == (lRet=RegEnumKeyEx(hKeyParent, dwIndex, rgszKeyName, &ccKeyName, NULL, rgszClass, &ccClass, NULL)))
        {
            if (-1==dwIndex)
            {
                TRACE(TEXT("RegEnumKeyEx should have failed for dwIndex=-1\r\n"));
                DUMP_LOCN;
                goto ErrorReturn;
            }
            dwIndex--;
            ccKeyName = sizeof(rgszKeyName)/sizeof(TCHAR);
            ccClass = sizeof(rgszClass)/sizeof(TCHAR);  
            if (_tcsncmp(rgszKeyName, _T("Key_"), 4))
                continue;
            szlen=0;
            StringCchLength(_T("Key_"), STRSAFE_MAX_CCH, &szlen);
            dwFoundIndex = _ttoi(rgszKeyName + szlen);

            if (dwFoundIndex >= 30)
                break;
            
            fFoundKey[dwFoundIndex]=TRUE;
        }
        VERIFY_REG_ERROR(RegEnumKeyEx, lRet, ERROR_NO_MORE_ITEMS);
        
        //  Verify that all the keys were enummerated
        for (k=0; k<dwNumKeys; k++)
        {
            if (FALSE == fFoundKey[k])
            {
                TRACE(TEXT("TEST_FAIL : Key_%d wasn't enumerated. Need to investigate\r\n"), k);
                DUMP_LOCN;
                goto ErrorReturn;
            }
        }

        REG_CLOSE_KEY(hKeyParent);
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);
    }
   

    dwRetVal = TPR_PASS;

ErrorReturn :
    REG_CLOSE_KEY(hKeyChild);    
    REG_CLOSE_KEY(hKeyParent);

    return dwRetVal;
}



///////////////////////////////////////////////////////////////////////////////
//
// Tests the KeyName parameters of the API 
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_EnumKey_Name(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0;
    HKEY hKeyParent=0, hKeyChild=0;
    TCHAR rgszKeyName[MAX_PATH]={0};
    DWORD ccKeyName=0;
    TCHAR rgszClass[MAX_PATH]={0};
    DWORD ccClass=0;
    LONG lRet=0;
    DWORD dwIndex=0;
    DWORD dwRoot, k;
    DWORD dwNumKeys=0;
    size_t szlen = 0;

    // The shell doesn't necessarily want us to execute the test. 
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    for (dwRoot=0; dwRoot<TST_NUM_ROOTS; dwRoot++)
    {
        hRoot=g_l_RegRoots[dwRoot];
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);

        WCHAR szClassTemp[MAX_PATH];
        StringCchCopy(szClassTemp, MAX_PATH, TREGAPI_CLASS);	
        
        lRet = RegCreateKeyEx(hRoot, _T("Test_RegEnumKey"), NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyParent, NULL);
        if (lRet)
            REG_FAIL(RegCreateKeyEx, lRet);

        //  Create 30 keys under here. 
        dwNumKeys=30;
        for (k=0; k<dwNumKeys; k++)
        {
            StringCchPrintf(rgszKeyName,MAX_PATH,_T("Key_%d"), k);
            lRet = RegCreateKeyEx(hKeyParent, rgszKeyName, NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyChild, NULL);
            if (lRet)
                REG_FAIL(RegCreateKeyEx, lRet);    
            REG_CLOSE_KEY(hKeyChild);
        }


        //
        // CASE: Make sure the buffer is correct
        //
        dwIndex = 0;
        ccKeyName=sizeof(rgszKeyName)/sizeof(TCHAR);
        ccClass=sizeof(rgszClass)/sizeof(TCHAR);
        memset(rgszKeyName, 65, sizeof(rgszKeyName));
        memset(rgszClass, 66, sizeof(rgszClass));
        while (ERROR_SUCCESS == (lRet=RegEnumKeyEx(hKeyParent, dwIndex, rgszKeyName, &ccKeyName, NULL, rgszClass, &ccClass, NULL)))
        {
            if (rgszKeyName[ccKeyName] != _T('\0')) 
            {
                TRACE(TEXT("TEST_FAIL : The returned key name is not NULL terminated\r\n"));
                DUMP_LOCN;
                goto ErrorReturn;
            }
            
            if (rgszClass[ccClass] != _T('\0')) 
            {
                TRACE(TEXT("TEST_FAIL : The returned Reg Class is not NULL terminated\r\n"));
                DUMP_LOCN;
                goto ErrorReturn;
            }
            StringCchLength(rgszKeyName, STRSAFE_MAX_CCH, &szlen);
            if (szlen != ccKeyName)
            {
                TRACE(TEXT("TEST_FAIL : values returned by RegEnumKeyEx are incorrect\r\n"));
                TRACE(TEXT("TEST_FAIL : _tcslen(KeyName)=%d is not the same as ccKeyName=%d\r\n"), szlen, ccKeyName);
                DUMP_LOCN;
                goto ErrorReturn;
            }
            
            szlen=0;
            StringCchLength(rgszClass, STRSAFE_MAX_CCH, &szlen);
            if (szlen != ccClass)
            {
                TRACE(TEXT("TEST_FAIL : values returned by RegEnumKeyEx are incorrect\r\n"));
                TRACE(TEXT("TEST_FAIL : _tcslen(rgszClass)=%d is not the same as ccClass=%d\r\n"), szlen, ccClass);
                DUMP_LOCN;
                goto ErrorReturn;
            }

            if (0 != _tcscmp(rgszClass, TREGAPI_CLASS))
            {
                TRACE(TEXT("TEST_FAIL : returned class is incorrect. Expecting %s got %s\r\n"), TREGAPI_CLASS, rgszClass);
                DUMP_LOCN;
                goto ErrorReturn;
            }

            dwIndex++;
            ccKeyName = sizeof(rgszKeyName)/sizeof(TCHAR);
            ccClass = sizeof(rgszClass)/sizeof(TCHAR);  
            memset(rgszKeyName, 65, sizeof(rgszKeyName));
            memset(rgszClass, 66, sizeof(rgszClass));
        }
        VERIFY_REG_ERROR(RegEnumKeyEx, lRet, ERROR_NO_MORE_ITEMS);


        //
        // CASE: Make sure that lpClass and ccClass can be NULL
        //
        dwIndex = 0;
        ccKeyName=sizeof(rgszKeyName)/sizeof(TCHAR);
        while (ERROR_SUCCESS == (lRet=RegEnumKeyEx(hKeyParent, dwIndex, rgszKeyName, &ccKeyName, NULL, NULL, NULL, NULL)))
        {
            if (rgszKeyName[ccKeyName] != _T('\0')) 
            {
                TRACE(TEXT("TEST_FAIL : The returned key name is not NULL terminated\r\n"));
                DUMP_LOCN;
                goto ErrorReturn;
            }
            StringCchLength(rgszKeyName, STRSAFE_MAX_CCH, &szlen);
            if (szlen != ccKeyName)
            {
                TRACE(TEXT("TEST_FAIL : values returned by RegEnumKeyEx are incorrect\r\n"));
                TRACE(TEXT("TEST_FAIL : _tcslen(KeyName)=%d is not the same as ccKeyName=%d\r\n"), szlen, ccKeyName);
                DUMP_LOCN;
                goto ErrorReturn;
            }

            dwIndex++;
            ccKeyName = sizeof(rgszKeyName)/sizeof(TCHAR);
        }
        VERIFY_REG_ERROR(RegEnumKeyEx, lRet, ERROR_NO_MORE_ITEMS);


        //
        // CASE: lpName=NULL, should fail 
        //
        dwIndex=0;
        ccKeyName=0;
        ccClass = sizeof(rgszClass)/sizeof(TCHAR);

        
        if (ERROR_SUCCESS == (lRet=RegEnumKeyEx(hKeyParent, dwIndex, NULL, &ccKeyName, NULL, rgszClass, &ccClass, NULL)))
        {
            TRACE(TEXT("TEST_FAIL : RegEnumKeyEx Did not fail for lpName=NULL \r\n"));
            DUMP_LOCN;
            goto ErrorReturn;
        }
        VERIFY_REG_ERROR(RegEnumKeyEx, lRet, ERROR_INVALID_PARAMETER);
        
        //
        // CASE: lpClass=NULL, should fail. 
        //
        dwIndex=0;
        ccKeyName=sizeof(rgszKeyName)/sizeof(TCHAR);
        ccClass=0;
        TRACE(TEXT("Note: The next test will cause an exception in filesys\r\n"));
        if (ERROR_SUCCESS == (lRet=RegEnumKeyEx(hKeyParent, dwIndex, rgszKeyName, &ccKeyName, NULL, NULL, &ccClass, NULL)))
        {
            TRACE(TEXT("TEST_FAIL: RegEnumKeyEx didn't fail for lpClass=NULL\r\n"));
            DUMP_LOCN;
            goto ErrorReturn;
        }
        VERIFY_REG_ERROR(RegEnumKeyEx, lRet, ERROR_INVALID_PARAMETER);


        //
        // CASE: lpname is too small, should fail. 
        //
        dwIndex=0;
        ccKeyName=2;   //  hardcode
        ccClass = sizeof(rgszClass)/sizeof(TCHAR);
        if (ERROR_MORE_DATA != (lRet=RegEnumKeyEx(hKeyParent, dwIndex, rgszKeyName, &ccKeyName, NULL, rgszClass, &ccClass, NULL)))
            REG_FAIL(RegEnumKeyEx, lRet);
        if (ccKeyName != 1)
        {
            TRACE(TEXT("TEST_FAIL : Potential buffer overrun. RegEnumKeyEx should not write more than 1 char in the Name buf\r\n"));
            DUMP_LOCN;
            goto ErrorReturn;
        }


        //
        // CASE: lpClass is too small, should fail. 
        //
        dwIndex=0;
        ccKeyName = sizeof(rgszKeyName)/sizeof(TCHAR);
        ccClass = 2;
        if (ERROR_SUCCESS != (lRet=RegEnumKeyEx(hKeyParent, dwIndex, rgszKeyName, &ccKeyName, NULL, rgszClass, &ccClass, NULL)))
            REG_FAIL(RegEnumKeyEx, lRet);
        if (ccClass != 1)
        {
            TRACE(TEXT("TEST_FAIL : Potential buffer overrun. RegEnumKeyEx should not write more than 1 char in the Class buf\r\n"));
            DUMP_LOCN;
            goto ErrorReturn;
        }

        REG_CLOSE_KEY(hKeyParent);
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);
    }

    dwRetVal = TPR_PASS;
    
ErrorReturn :
    REG_CLOSE_KEY(hKeyChild);
    REG_CLOSE_KEY(hKeyParent);
    
    return dwRetVal;
}



///////////////////////////////////////////////////////////////////////////////
//
// Misc RegEnumKeyEx tests.
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_EnumKey_Misc(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0;
    HKEY hKeyParent=0, hKeyChild=0, hKeyTemp=0;
    TCHAR rgszKeyName[MAX_PATH]={0};
    DWORD ccKeyName=0;
    TCHAR rgszClass[MAX_PATH]={0};
    DWORD ccClass=0;
    LONG lRet=0;
    DWORD dwIndex=0, dwIdxChild;
    DWORD dwRoot=0, k;
    DWORD dwNumKeys=0;
    DWORD dwTemp=0;
    FILETIME FtTemp;
    WCHAR szClassTemp[MAX_PATH];
    size_t szlen = 0;

    StringCchCopy(szClassTemp, MAX_PATH, TREGAPI_CLASS);	

    // The shell doesn't necessarily want us to execute the test. 
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    for (dwRoot=0; dwRoot<TST_NUM_ROOTS; dwRoot++)
    {
        hRoot=g_l_RegRoots[dwRoot];
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);
        
        lRet = RegCreateKeyEx(hRoot, _T("Test_RegEnumKey"), NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyParent, NULL);
        if (lRet)
            REG_FAIL(RegCreateKeyEx, lRet);

        // Create 30 keys under here. 
        dwNumKeys=30;
        for (k=0; k<dwNumKeys; k++)
        {
            StringCchPrintf(rgszKeyName,MAX_PATH,_T("Key_%d"), k);
            lRet = RegCreateKeyEx(hKeyParent, rgszKeyName, NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyChild, NULL);
            if (lRet)
                REG_FAIL(RegCreateKeyEx, lRet);    
            REG_CLOSE_KEY(hKeyChild);
        }


        //
        // CASE: If there are no subkeys to enumerate, it should correctly fail.
        //
        dwIndex = 0;
        ccKeyName=sizeof(rgszKeyName)/sizeof(TCHAR);
        while (ERROR_SUCCESS == (lRet=RegEnumKeyEx(hKeyParent, dwIndex++, rgszKeyName, &ccKeyName, NULL, NULL, NULL, NULL)))
        {
            // Open the enumerated child key. 
            if (ERROR_SUCCESS != (lRet=RegOpenKeyEx(hKeyParent, rgszKeyName, 0, 0, &hKeyChild)))
                REG_FAIL(RegOpenKey, lRet);

            // Enumeration in the child key should fail since it's empty. 
            dwIdxChild=0;
            ccClass=sizeof(rgszClass)/sizeof(TCHAR);
            if (ERROR_SUCCESS == (lRet=RegEnumKeyEx(hKeyChild, dwIdxChild, rgszClass, &ccClass, NULL, NULL, NULL, NULL)))
            {
                TRACE(TEXT("TEST_FAIL : RegEnumKey should have failed for an empty key\r\n"));
                DUMP_LOCN;
                goto ErrorReturn;
            }
            VERIFY_REG_ERROR(RegEnumKeyEx, lRet, ERROR_NO_MORE_ITEMS);
            REG_CLOSE_KEY(hKeyChild);
            
            ccKeyName = sizeof(rgszKeyName)/sizeof(TCHAR);
        }


        //
        // CASE: Should fail for out of range dwIndex. 
        //
        dwIndex=dwNumKeys+10;
        ccKeyName=sizeof(rgszKeyName)/sizeof(TCHAR);
        if (ERROR_SUCCESS == (lRet=RegEnumKeyEx(hKeyParent, dwIndex++, rgszKeyName, &ccKeyName, NULL, NULL, NULL, &FtTemp)))
        {
            TRACE(TEXT("TEST_FAIL : RegEnumKey Did not Failed for a bad dwIndex=%d. \r\n"), dwIndex);
            DUMP_LOCN; 
            goto ErrorReturn;
        }

        
        //
        // CASE: Non_NULL FT
        //
        dwIndex = 0;
        ccKeyName=sizeof(rgszKeyName)/sizeof(TCHAR);
        if (ERROR_SUCCESS != (lRet=RegEnumKeyEx(hKeyParent, dwIndex++, rgszKeyName, &ccKeyName, NULL, NULL, NULL, &FtTemp)))
        {
            TRACE(TEXT("TEST_FAIL : RegEnumKey Failed for a non-NULL FT. 0x%x\r\n"), lRet);
            REG_FAIL(RegEnumKeyEx, lRet);
        }


        //
        // CASE: Key and Value of same name, which one is enumerated
        //
        REG_CLOSE_KEY(hKeyParent);
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (lRet)
            REG_FAIL(RegDeleteKey, lRet);
        
        lRet = RegCreateKeyEx(hRoot, _T("Test_RegEnumKey"), NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyParent, NULL);
        if (lRet)
            REG_FAIL(RegCreateKeyEx, lRet);

        // Create 30 keys under here. 
        dwNumKeys=3;
        for (k=0; k<dwNumKeys; k++)
        {
            // Create a Child Key
            StringCchPrintf(rgszKeyName,MAX_PATH,_T("ChildKey_%d"), k);
            lRet = RegCreateKeyEx(hKeyParent, rgszKeyName, NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyChild, NULL);
            if (lRet)
                REG_FAIL(RegCreateKeyEx, lRet);    
            
            // Create a child Value
            StringCchCopy(rgszClass, MAX_PATH, _T("FooBar"));
            StringCchLength(rgszClass, STRSAFE_MAX_CCH, &szlen);
            lRet=RegSetValueEx(hKeyParent, rgszKeyName, 0, REG_SZ, (PBYTE)rgszClass, (szlen+1)*sizeof(TCHAR));
            if (lRet)
                REG_FAIL(RegSetValue, lRet);

            // Create a grand-child key under the child Key
            StringCchPrintf(rgszKeyName,MAX_PATH,_T("GrandChildKey_%d"), k);
            lRet = RegCreateKeyEx(hKeyChild, rgszKeyName, NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyTemp, NULL);
            if (lRet)
                REG_FAIL(RegCreateKeyEx, lRet);    
            REG_CLOSE_KEY(hKeyTemp);
            REG_CLOSE_KEY(hKeyChild);
        }
        
        dwIndex = 0;
        ccKeyName=sizeof(rgszKeyName)/sizeof(TCHAR);
        while (ERROR_SUCCESS == (lRet=RegEnumKeyEx(hKeyParent, dwIndex, rgszKeyName, &ccKeyName, NULL, NULL, NULL, NULL)))
        {
            // Make sure this key has one key. 
            if (ERROR_SUCCESS != (lRet=RegOpenKeyEx(hKeyParent, rgszKeyName, 0, 0, &hKeyChild)))
                REG_FAIL(RegOpenKey, lRet);
            if (!Hlp_GetNumSubKeys(hKeyChild, &dwTemp))
                goto ErrorReturn;
            
            if (1 != dwTemp)
            {
                TRACE(TEXT("TEST_FAIL: The key %s should have 1 subkey, Got %d instead\r\n"), rgszKeyName, dwTemp);
                DUMP_LOCN;
                goto ErrorReturn;
            }
            REG_CLOSE_KEY(hKeyChild);
            dwIndex++;
            ccKeyName=sizeof(rgszKeyName)/sizeof(TCHAR);
        }

        
        //
        // CASE: Make sure we can enumerate say 1000 keys.
        //
        REG_CLOSE_KEY(hKeyParent);
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);
        
        lRet = RegCreateKeyEx(hRoot, _T("Test_RegEnumKey"), NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyParent, NULL);
        if (lRet)
            REG_FAIL(RegCreateKeyEx, lRet);

        // Create 1000 keys under here. 
        dwNumKeys=1000;
        TRACE(TEXT("Creating %d keys .... \r\n"), dwNumKeys);
        for (k=0; k<dwNumKeys; k++)
        {
            StringCchPrintf(rgszKeyName,MAX_PATH,_T("Key_%d"), k);
            lRet = RegCreateKeyEx(hKeyParent, rgszKeyName, NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyChild, NULL);
            if (lRet)
                REG_FAIL(RegCreateKeyEx, lRet);    
            REG_CLOSE_KEY(hKeyChild);
        }

        // Enumerate expecting 1000 keys
        TRACE(TEXT("Enumerating %d keys...\r\n"), dwNumKeys);
        ccKeyName = sizeof(rgszKeyName)/sizeof(TCHAR);
        dwIndex=0;
        while (ERROR_SUCCESS == (lRet=RegEnumKeyEx(hKeyParent, dwIndex, rgszKeyName, &ccKeyName, NULL, NULL, NULL, NULL)))
        {
            // Open the enumerated child key. 
            if (ERROR_SUCCESS != (lRet=RegOpenKeyEx(hKeyParent, rgszKeyName, 0, 0, &hKeyChild)))
                REG_FAIL(RegOpenKey, lRet);

            // Enumeration in the child key should fail since it's empty. 
            dwIdxChild=0;
            ccClass=sizeof(rgszClass)/sizeof(TCHAR);
            if (ERROR_SUCCESS == (lRet=RegEnumKeyEx(hKeyChild, dwIdxChild, rgszClass, &ccClass, NULL, NULL, NULL, NULL)))
            {
                TRACE(TEXT("TEST_FAIL : RegEnumKey should have failed for an empty key\r\n"));
                DUMP_LOCN;
                goto ErrorReturn;
            }
            VERIFY_REG_ERROR(RegEnumKeyEx, lRet, ERROR_NO_MORE_ITEMS);
            REG_CLOSE_KEY(hKeyChild);

            dwIndex++;
            ccKeyName = sizeof(rgszKeyName)/sizeof(TCHAR);
        }
        if (dwIndex != dwNumKeys)
        {
            TRACE(TEXT("TEST_FAIL : Expecting %d keys But only %d were enumerated\r\n"), dwNumKeys, dwIndex);
            DUMP_LOCN;
            goto ErrorReturn;
        }
       
        REG_CLOSE_KEY(hKeyParent);
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);    
    }

    dwRetVal = TPR_PASS;
    
ErrorReturn :
    REG_CLOSE_KEY(hKeyParent);
    REG_CLOSE_KEY(hKeyChild);
    return dwRetVal;
}



///////////////////////////////////////////////////////////////////////////////
//
// Test enumerating values
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_EnumVal_hKey(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0, hKeyParent=0, hKeyAlt=0 ;
    TCHAR rgszValName[MAX_PATH]={0};
    DWORD ccValName=0;
    LONG lRet=0;
    DWORD dwIndex=0, dwRoot=0, dwNumVals=0, dwType=0, dwTemp=0, dwMaxcb=0, k ;
    PBYTE pbData=NULL;
    DWORD cbData=NULL;
    BOOL *pfFound=NULL;
    WCHAR szClassTemp[MAX_PATH];
    size_t szlen = 0;

    StringCchCopy(szClassTemp, MAX_PATH, TREGAPI_CLASS);	

    // The shell doesn't necessarily want us to execute the test. 
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    cbData=TST_VAL_BUF_SIZE;
    pbData = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
    CHECK_ALLOC(pbData);
    
    for (dwRoot=0; dwRoot<TST_NUM_ROOTS; dwRoot++)
    {
        hRoot=g_l_RegRoots[dwRoot];
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);
        
        lRet = RegCreateKeyEx(hRoot, _T("Test_RegEnumValue"), NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyParent, NULL);
        if (lRet)
            REG_FAIL(RegCreateKeyEx, lRet);

        dwNumVals=30;
        for (k=0; k<dwNumVals; k++)
        {
            // Create a value
            StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
            dwType=0;
            cbData = TST_VAL_BUF_SIZE;
            if (!Hlp_Write_Random_Value(hKeyParent, NULL, rgszValName, &dwType, pbData, &cbData))
                goto ErrorReturn;
            ASSERT(cbData!=0);
            if (cbData > dwMaxcb)
                dwMaxcb = cbData;
        }


        //
        // CASE: make sure I can enumerate values (BVT)
        //
        FREE(pfFound);
        pfFound = (BOOL*)LocalAlloc(LMEM_ZEROINIT, dwNumVals*sizeof(BOOL));
        CHECK_ALLOC(pfFound);
        ccValName=sizeof(rgszValName)/sizeof(TCHAR);
        cbData=TST_VAL_BUF_SIZE ;
        dwIndex=0;
        rgszValName[0]= _T('\0');
        while (ERROR_SUCCESS == (lRet=RegEnumValue(hKeyParent, dwIndex, rgszValName, &ccValName, NULL, &dwType, pbData, &cbData)))
        {
            cbData = TST_VAL_BUF_SIZE;
            dwIndex++;
            StringCchLength(_T("Value_"), STRSAFE_MAX_CCH, &szlen);
            dwTemp = _ttoi(rgszValName+ szlen);
            pfFound[dwTemp]=TRUE;
            ccValName=sizeof(rgszValName)/sizeof(TCHAR);
        }
        VERIFY_REG_ERROR(RegEnumValue, lRet, ERROR_NO_MORE_ITEMS);
        if (dwIndex != dwNumVals)
        {
            TRACE(TEXT("TEST_FAIL : Created %d Vals, but enumerated only %d Vals\r\n"), dwNumVals, dwIndex);
            DUMP_LOCN;
            goto ErrorReturn;
        }
        
        // Check to make sure that we enumerated all the values. 
        for (k=0; k<dwNumVals; k++)
        {
            if (FALSE == pfFound[k])
            {
                TRACE(TEXT("TEST_FAIL : Value %d not found during enumeration\r\n"), k);
                DUMP_LOCN;
                goto ErrorReturn;
            }
        }
        FREE(pfFound);

        
        //
        // CASE: Make sure it doesn't fail if there is another handle to the key
        //
        lRet=RegOpenKeyEx(hRoot, _T("Test_RegEnumValue"), 0, 0, &hKeyAlt);
        if (lRet)
            REG_FAIL(RegOpenKeyEx, lRet);
        cbData = TST_VAL_BUF_SIZE;
        dwIndex=0;
        ccValName=sizeof(rgszValName)/sizeof(TCHAR);
        while (ERROR_SUCCESS == (lRet=RegEnumValue(hKeyParent, dwIndex, rgszValName, &ccValName, NULL, &dwType, pbData, &cbData)))
        {
            cbData = TST_VAL_BUF_SIZE;
            dwIndex++;
            ccValName=sizeof(rgszValName)/sizeof(TCHAR);
        }
        VERIFY_REG_ERROR(RegEnumValue, lRet, ERROR_NO_MORE_ITEMS);
        if (dwIndex != dwNumVals)
        {
            TRACE(TEXT("TEST_FAIL : Created %d Vals, but enumerated only %d Vals\r\n"), dwNumVals, dwIndex);
            DUMP_LOCN;
            goto ErrorReturn;
        }
        REG_CLOSE_KEY(hKeyAlt);


        //
        // CASE: Enumerate Values at Root. 
        //
        dwNumVals=30;
        for (k=0; k<dwNumVals; k++)
        {
            // Create a value
            StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
            dwType=0;
            cbData = TST_VAL_BUF_SIZE;
            if (!Hlp_Write_Random_Value(hRoot, NULL, rgszValName, &dwType, pbData, &cbData))
                goto ErrorReturn;
            ASSERT(cbData!=0);
            if (cbData > dwMaxcb)
                dwMaxcb = cbData;
        }

        ccValName=sizeof(rgszValName)/sizeof(TCHAR);
        cbData=TST_VAL_BUF_SIZE ;
        dwIndex=0;
        rgszValName[0]= _T('\0');
        while (ERROR_SUCCESS == (lRet=RegEnumValue(hRoot, dwIndex, rgszValName, &ccValName, NULL, &dwType, pbData, &cbData)))
        {
            cbData = TST_VAL_BUF_SIZE;
            dwIndex++;
            ccValName=sizeof(rgszValName)/sizeof(TCHAR);
        }
        VERIFY_REG_ERROR(RegEnumValue, lRet, ERROR_NO_MORE_ITEMS);
        if (dwIndex < dwNumVals)
        {
            TRACE(TEXT("TEST_FAIL : Created %d Vals, but enumerated only %d Vals\r\n"), dwNumVals, dwIndex);
            DUMP_LOCN;
            goto ErrorReturn;
        }

        for (k=0; k<dwNumVals; k++)
        {
            // Create a value
            StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
            lRet=RegDeleteValue(hRoot, rgszValName);
            if (lRet)
            {
                TRACE(TEXT("TEST_FAIL : Failed cleanup at Root key. rgszValName=%s\r\n"), rgszValName);
                DUMP_LOCN;
                goto ErrorReturn;
            }
        }
        REG_CLOSE_KEY(hKeyParent);
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);

    }


    //
    // CASE: Invalid hKey
    //
    TRACE(TEXT("Note : The next test will cause an exception in filesys\r\n"));
    if (ERROR_SUCCESS == (lRet=RegEnumValue((HKEY)-1, 0, rgszValName, &ccValName, NULL, &dwType, pbData, &cbData)))
    {
        TRACE(TEXT("TEST_FAIL : RegEnumValue should have failed for an invalid hKey=-1\r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }

    if (ERROR_SUCCESS == (lRet=RegEnumValue((HKEY)0, 0, rgszValName, &ccValName, NULL, &dwType, pbData, &cbData)))
    {
        TRACE(TEXT("TEST_FAIL : RegEnumValue should have failed for an invalid hKey=0\r\n"));
        DUMP_LOCN;
        goto ErrorReturn;
    }

    dwRetVal = TPR_PASS;
    
ErrorReturn :
    FREE(pbData);
    FREE(pfFound);
    REG_CLOSE_KEY(hKeyParent);
    REG_CLOSE_KEY(hKeyAlt);
    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// Test enumerating index
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Test_EnumVal_index(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0, hKeyParent=0, hKeyAlt=0;
    TCHAR rgszValName[MAX_PATH]={0};
    DWORD ccValName=0;
    TCHAR rgszValName2[MAX_PATH]={0};
    DWORD ccValName2=0;
    LONG lRet=0;
    DWORD dwIndex=0, dwRoot=0, dwType=0, dwTemp=0, k;
    static DWORD dwNumVals=30;
    PBYTE pbData=NULL;
    DWORD cbData=NULL;
    PBYTE pbData2=NULL;
    DWORD cbData2=NULL;
    BOOL *pfFound=NULL;
    BOOL fDone=FALSE;
    WCHAR szClassTemp[MAX_PATH];
    size_t szlen = 0;
    DWORD dwMaxValues = FlagIsPresentOnCmdLine(g_pShellInfo->szDllCmdLine, L"-depth") ? 4096 : 2048;
    StringCchCopy(szClassTemp, MAX_PATH, TREGAPI_CLASS);	

    // The shell doesn't necessarily want us to execute the test. 
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    cbData=TST_VAL_BUF_SIZE;
    pbData = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
    CHECK_ALLOC(pbData);
    cbData2=TST_VAL_BUF_SIZE;
    pbData2 = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData2);
    CHECK_ALLOC(pbData2);

    // Iterate over all pre-defined roots.
    for (dwRoot=0; dwRoot<TST_NUM_ROOTS; dwRoot++)
    {
        hRoot=g_l_RegRoots[dwRoot];
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);
        
        lRet = RegCreateKeyEx(hRoot, _T("Test_RegEnumKey"), NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyParent, NULL);
        if (lRet)
            REG_FAIL(RegCreateKeyEx, lRet);

        // Create 30 values of random type in the Test Key.
        for (k=0; k<dwNumVals; k++)
        {
            //  Create a value
            StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
            dwType=0;
            cbData = TST_VAL_BUF_SIZE;
            if (!Hlp_Write_Random_Value(hKeyParent, NULL, rgszValName, &dwType, pbData, &cbData))
                goto ErrorReturn;
            ASSERT(cbData!=0);
        }


        //
        // CASE: Invalid dwIndex
        //
        dwIndex= (DWORD)-1;
        ccValName = sizeof(rgszValName)/sizeof(TCHAR);
        cbData = TST_VAL_BUF_SIZE;
        if (ERROR_SUCCESS == (lRet=RegEnumValue(hKeyParent, dwIndex, rgszValName, &ccValName, NULL, &dwType, pbData, &cbData)))
        {
            TRACE(TEXT("TEST_FAIL : RegEnumValue should have failed for an invalid dwIndex=-1\r\n"));
            DUMP_LOCN;
            goto ErrorReturn;
        }
        
        dwIndex= (DWORD)-2;
        ccValName = sizeof(rgszValName)/sizeof(TCHAR);
        cbData = TST_VAL_BUF_SIZE;
        if (ERROR_SUCCESS == (lRet=RegEnumValue(hKeyParent, dwIndex, rgszValName, &ccValName, NULL, &dwType, pbData, &cbData)))
        {
            TRACE(TEXT("TEST_FAIL : RegEnumValue should have failed for an invalid dwIndex=-2\r\n"));
            DUMP_LOCN;
            goto ErrorReturn;
        }


        //
        // CASE: Same index should result in same data.
        //
        for (dwIndex=0; dwIndex<dwNumVals; dwIndex++)
        {
            ccValName = sizeof(rgszValName)/sizeof(TCHAR);
            cbData = TST_VAL_BUF_SIZE;
            if (ERROR_SUCCESS != (lRet=RegEnumValue(hKeyParent, dwIndex, rgszValName, &ccValName, NULL, &dwType, pbData, &cbData)))
                REG_FAIL(RegEnumValue, lRet);
            
            for (k=0; k<20; k++)
            {
                ccValName2 = sizeof(rgszValName2)/sizeof(TCHAR);
                cbData2 = TST_VAL_BUF_SIZE;
                if (ERROR_SUCCESS != (lRet=RegEnumValue(hKeyParent, dwIndex, rgszValName2, &ccValName2, NULL, &dwTemp, pbData2, &cbData2)))
                    REG_FAIL(RegEnumValue, lRet);
                if ((dwTemp != dwType) ||
                    (_tcscmp(rgszValName, rgszValName2)) ||
                    (memcmp(pbData2, pbData, cbData)))
                {
                    TRACE(TEXT("The enumerated value does not compare to the previous value\r\n"));
                    DUMP_LOCN;
                    goto ErrorReturn;
                }
            }
        }


        //
        // CASE: Decrement dwIndex. 
        //
        dwIndex = dwNumVals-1;
        ccValName = sizeof(rgszValName)/sizeof(TCHAR);
        cbData = TST_VAL_BUF_SIZE;
        FREE(pfFound);
        pfFound = (BOOL*)LocalAlloc(LMEM_ZEROINIT, dwNumVals*sizeof(BOOL));
        CHECK_ALLOC(pfFound);
        while (dwIndex!= -1)
        {
            if (ERROR_SUCCESS != (lRet = RegEnumValue(hKeyParent, dwIndex, rgszValName, &ccValName, NULL, &dwType, pbData, &cbData)))
            {
                TRACE(TEXT("TEST_FAIL : Failed RegEnumValue in the reverse direction. dwIndex=%d\r\n"), dwIndex);
                DUMP_LOCN;
                goto ErrorReturn;
            }
            ccValName = sizeof(rgszValName)/sizeof(TCHAR);
            cbData = TST_VAL_BUF_SIZE;
            dwIndex--;
            StringCchLength(_T("Value_"), STRSAFE_MAX_CCH, &szlen);
            dwTemp = _ttoi(rgszValName+ szlen);
            pfFound[dwTemp]=TRUE;
        }
        // Check to make sure that we enumerated all the values. 
        for (k=0; k<dwNumVals; k++)
        {
            if (FALSE == pfFound[k])
            {
                TRACE(TEXT("TEST_FAIL : Value %d not found during enumeration\r\n"), k);
                DUMP_LOCN;
                goto ErrorReturn;
            }
        }


        //
        // CASE: make sure dwIndex=0 resets the enumeration
        //
        ccValName = sizeof(rgszValName)/sizeof(TCHAR);
        cbData = TST_VAL_BUF_SIZE;
        fDone=FALSE;
        dwIndex=0;
        while (ERROR_SUCCESS == (lRet=RegEnumValue(hKeyParent, dwIndex, rgszValName, &ccValName, NULL, &dwType, pbData, &cbData)))
        {
            dwIndex++;
            ccValName = sizeof(rgszValName)/sizeof(TCHAR);
            cbData = TST_VAL_BUF_SIZE;
            
            if (_tcsncmp(rgszValName, _T("Value_"), 6))
                continue;
            
            StringCchLength(_T("Value_"), STRSAFE_MAX_CCH, &szlen);
            dwTemp = _ttoi(rgszValName + szlen);
            pfFound[dwTemp]=TRUE;
            if ((dwIndex>=5) && (!fDone))
            {
                dwIndex=0;
                fDone=TRUE;
                memset(pfFound, 0, dwNumVals*sizeof(BOOL));   //  set everything to false.
            }
            
        }
        VERIFY_REG_ERROR(RegEnumKeyEx, lRet, ERROR_NO_MORE_ITEMS);
        
        if (dwNumVals!= dwIndex)
        {
            TRACE(TEXT("TEST_FAIL : expecting %d Values, but only %d were enumerated\r\n"), dwNumVals, dwIndex);
            DUMP_LOCN;
            goto ErrorReturn;
        }
        // Verify that all the keys were enummerated
        for (k=0; k<dwNumVals; k++)
        {
            if (FALSE == pfFound[k])
            {
                TRACE(TEXT("TEST_FAIL : Value_%d wasn't enumerated. Investigate\r\n"), k);
                DUMP_LOCN;
                goto ErrorReturn;
            }
        }


        //
        // CASE: Make sure I can randomly enumerate
        //
        for (k=0; k<100; k++)
        {
            dwIndex = Random()%dwNumVals;
            ccValName = sizeof(rgszValName)/sizeof(TCHAR);
            cbData = TST_VAL_BUF_SIZE;
            if(ERROR_SUCCESS != (lRet=RegEnumValue(hKeyParent, dwIndex, rgszValName, &ccValName, NULL, &dwType, pbData, &cbData)))
            {
                TRACE(TEXT("TEST_FAIL : RegEnumValue failed to enumerate dwIndex=%d. k=%d, err=0x%x\r\n"), dwIndex, k, lRet);
                DUMP_LOCN;
                goto ErrorReturn;
            }
        }


        //
        // CASE: Is there a limit to the num of vals that can be enumerated?
        // 			Trying dwMaxValues
        //			Closing hKeyParent here. 
        //
        TRACE(TEXT("Creating %u values..... \r\n"), dwMaxValues);
        FREE(pfFound);
        pfFound = (BOOL*)LocalAlloc(LMEM_ZEROINIT, dwMaxValues*sizeof(BOOL));
        CHECK_ALLOC(pfFound);
        memset((PBYTE)pfFound, 65, dwMaxValues*sizeof(BOOL));

        lRet = RegCloseKey(hKeyParent);
        if (lRet)
            REG_FAIL(RegCloseKey, lRet);
        hKeyParent=0;
        
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);
        
        lRet = RegCreateKeyEx(hRoot, _T("Test_RegEnum_manyvals"), NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyAlt, NULL);
        if (lRet)
            REG_FAIL(RegCreateKeyEx, lRet);

        // Create dwMaxValues values of random type in the Test Key.
        for (k=0; k<dwMaxValues; k++)
        {
            // Create a value
            StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
            dwType=0;
            cbData = TST_VAL_BUF_SIZE;
            if (!Hlp_Write_Random_Value(hKeyAlt, NULL, rgszValName, &dwType, pbData, &cbData))
                goto ErrorReturn;
            ASSERT(cbData!=0);
        }

        // Begin enumeration
        ccValName = sizeof(rgszValName)/sizeof(TCHAR);
        cbData = TST_VAL_BUF_SIZE;
        dwIndex=0;
        fDone=FALSE; 
        while (ERROR_SUCCESS == (lRet=RegEnumValue(hKeyAlt, dwIndex, rgszValName, &ccValName, NULL, &dwType, pbData, &cbData)))
        {
            dwIndex++;
            ccValName = sizeof(rgszValName)/sizeof(TCHAR);
            cbData = TST_VAL_BUF_SIZE;
            
            if (_tcsncmp(rgszValName, _T("Value_"), 6))
                continue;
            StringCchLength(_T("Value_"), STRSAFE_MAX_CCH, &szlen);
            dwTemp = _ttoi(rgszValName + szlen);
            pfFound[dwTemp]=TRUE;
        }
        VERIFY_REG_ERROR(RegEnumKeyEx, lRet, ERROR_NO_MORE_ITEMS);
        
        if (dwMaxValues != dwIndex)
        {
            TRACE(TEXT("TEST_FAIL : expecting %u values, but only %d were enumerated\r\n"), dwMaxValues, dwIndex);
            DUMP_LOCN;
            goto ErrorReturn;
        }
        // Verify that all the keys were enummerated
        for (k=0; k<dwMaxValues; k++)
        {
            if (FALSE == pfFound[k])
            {
                TRACE(TEXT("TEST_FAIL : Value_%d wasn't enumerated. Investigate\r\n"), k);
                DUMP_LOCN;
                goto ErrorReturn;
            }
        }
        FREE(pfFound);
        lRet = RegCloseKey(hKeyAlt);
        if (hKeyAlt && lRet)
            REG_FAIL(RegCloseKey, lRet);
        hKeyAlt=0;
        
        lRet=RegDeleteKey(hRoot, _T("Test_RegEnum_manyvals"));
        if (lRet)
            REG_FAIL(RegDeleteKey, lRet);
        
        lRet = RegCloseKey(hKeyParent);
        if (hKeyParent && lRet)
            REG_FAIL(RegCloseKey, lRet);
        hKeyParent=0;

        REG_CLOSE_KEY(hKeyParent);
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);
    }

    dwRetVal = TPR_PASS;
    
ErrorReturn :
    FREE(pbData);
    FREE(pbData2);
    FREE(pfFound);
    REG_CLOSE_KEY(hKeyParent);
    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// More enumeration test
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_EnumVal_ValName(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0, hKeyParent=0 ;
    TCHAR rgszValName[MAX_PATH]={0};
    DWORD ccValName=0;
    LONG lRet=0;
    DWORD dwIndex=0, dwRoot=0, dwNumVals=0, dwType=0, dwTemp=0, dwMaxcb=0, k ;
    PBYTE pbData=NULL;
    DWORD cbData=NULL;
    WCHAR szClassTemp[MAX_PATH];
    size_t szlen = 0;
    StringCchCopy(szClassTemp, MAX_PATH, TREGAPI_CLASS);	

    // The shell doesn't necessarily want us to execute the test. 
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    cbData=TST_VAL_BUF_SIZE;
    pbData = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
    CHECK_ALLOC(pbData);
    
    // Iterate over all pre-defined roots.
    for (dwRoot=0; dwRoot<TST_NUM_ROOTS; dwRoot++)
    {
        hRoot=g_l_RegRoots[dwRoot];
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);
        
        lRet = RegCreateKeyEx(hRoot, _T("Test_RegEnumKey"), NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyParent, NULL);
        if (lRet)
            REG_FAIL(RegCreateKeyEx, lRet);

        // Create 30 values of random type in the Test Key.
        dwNumVals=30;
        for (k=0; k<dwNumVals; k++)
        {
            // Create a value
            StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
            dwType=0;
            cbData = TST_VAL_BUF_SIZE;
            if (!Hlp_Write_Random_Value(hKeyParent, NULL, rgszValName, &dwType, pbData, &cbData))
                goto ErrorReturn;
            ASSERT(cbData!=0);
        }


        //
        // CASE: ccValName is small
        //
        if (-1 == (dwTemp = Hlp_GetMaxValueNameChars(hKeyParent)))
            goto ErrorReturn;
        dwIndex=0;
        cbData = TST_VAL_BUF_SIZE;
        dwTemp -= 3;
        ccValName=dwTemp;
        while (ERROR_SUCCESS == (lRet=RegEnumValue(hKeyParent, dwIndex, rgszValName, &ccValName, NULL, &dwType, pbData, &cbData)))
        {
            StringCchLength(rgszValName, STRSAFE_MAX_CCH, &szlen);
            if (ccValName != szlen)
            {
                TRACE(TEXT("TEST_FAIL : returned ccValName (=%d) does not equal _tcslen(rgszValName)=%d\r\n"), ccValName, szlen);
                DUMP_LOCN;
                goto ErrorReturn;
            }
            if (rgszValName[ccValName] != _T('\0'))
            {
                TRACE(TEXT("TEST_FAIL : Enumerated value (=%s)name is not NULL terminated\r\n"), rgszValName);
                DUMP_LOCN;
                goto ErrorReturn;
            }
            ccValName=dwTemp;
            cbData = TST_VAL_BUF_SIZE;
            dwIndex++;            
        }

        
        //
        // CASE:  lpValName is NULL.
        //
        ccValName=0;
        cbData = TST_VAL_BUF_SIZE;
        if (ERROR_SUCCESS == (lRet=RegEnumValue(hKeyParent, 0, NULL, &ccValName, NULL, &dwType, pbData, &cbData)))
        {
            TRACE(TEXT("TEST_FAIL : RegEnumValue should have failed for pszValName=NULL\r\n"));
            DUMP_LOCN;
            goto ErrorReturn;
        }
        else
            TRACE(TEXT("RegEnumValue failed correctly with error 0x%x\r\n"), lRet);

        
        //
        // CASE: ccValName=0, but valid pszValName
        //
        ccValName=0;
        cbData = TST_VAL_BUF_SIZE;
        if (ERROR_SUCCESS == (lRet=RegEnumValue(hKeyParent, 0, rgszValName, &ccValName, NULL, &dwType, pbData, &cbData)))
        {
            TRACE(TEXT("TEST_FAIL : RegEnumValue should have failed for pszValName=NULL\r\n"));
            DUMP_LOCN;
            goto ErrorReturn;
        }
        else
            TRACE(TEXT("RegEnumValue failed correctly with error 0x%x\r\n"), lRet);


        //
        // CASE: Make sure that there is no BO and that the returned pszValName is null terminated and 
        //			ccValName is correct.
        //
        if (-1 == (dwTemp = Hlp_GetMaxValueNameChars(hKeyParent)))
            goto ErrorReturn;
        dwMaxcb=0;
        
        dwIndex=0;
        cbData = TST_VAL_BUF_SIZE;
        ccValName=sizeof(rgszValName)/sizeof(TCHAR);
        memset(rgszValName, 65, sizeof(rgszValName));
        while (ERROR_SUCCESS == (lRet=RegEnumValue(hKeyParent, dwIndex, rgszValName, &ccValName, NULL, &dwType, pbData, &cbData)))
        {
            StringCchLength(rgszValName, STRSAFE_MAX_CCH, &szlen);
            if (ccValName != szlen)
            {
                TRACE(TEXT("TEST_FAIL : returned ccValName (=%d) does not equal _tcslen(rgszValName)=%d\r\n"), ccValName, szlen);
                DUMP_LOCN;
                goto ErrorReturn;
            }
            if (rgszValName[ccValName] != _T('\0'))
            {
                TRACE(TEXT("TEST_FAIL : Enumerated value (=%s)name is not NULL terminated\r\n"), rgszValName);
                DUMP_LOCN;
                goto ErrorReturn;
            }
            
            if (ccValName > dwMaxcb)
                dwMaxcb=ccValName;
            
            ccValName=sizeof(rgszValName)/sizeof(TCHAR);
            cbData = TST_VAL_BUF_SIZE;
            memset(rgszValName, 65, sizeof(rgszValName));
            dwIndex++;            
        }
        VERIFY_REG_ERROR(RegEnumValue, lRet, ERROR_NO_MORE_ITEMS);
        
        if (dwMaxcb > dwTemp)
        {
            TRACE(TEXT("TEST_FAIL : RegQueryInfoKey said that the max value name size =%d, but the max found was %d\r\n"), dwTemp, dwMaxcb);
            DUMP_LOCN;
            goto ErrorReturn;
        }

        REG_CLOSE_KEY(hKeyParent);
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);
    }

    dwRetVal = TPR_PASS;
    
ErrorReturn :
    FREE(pbData);
    REG_CLOSE_KEY(hKeyParent);
    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// More enumeration test
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_EnumVal_Type(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0, hKeyParent=0 ;
    TCHAR rgszValName[MAX_PATH]={0};
    DWORD ccValName=0;
    LONG lRet=0;
    DWORD dwIndex=0, dwRoot=0, dwNumVals=0, dwType=0, dwTemp=0, k =0;
    PBYTE pbData=NULL;
    DWORD cbData=NULL;
    WCHAR szClassTemp[MAX_PATH];
    size_t szlen = 0;
    StringCchCopy(szClassTemp, MAX_PATH, TREGAPI_CLASS);	
    
    // The shell doesn't necessarily want us to execute the test. 
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    cbData=TST_VAL_BUF_SIZE;
    pbData = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
    CHECK_ALLOC(pbData);
    
    // Iterate over all pre-defined roots.
    for (dwRoot=0; dwRoot<TST_NUM_ROOTS; dwRoot++)
    {
        hRoot=g_l_RegRoots[dwRoot];
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);
        
        lRet = RegCreateKeyEx(hRoot, _T("Test_RegEnumKey"), NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyParent, NULL);
        if (lRet)
            REG_FAIL(RegCreateKeyEx, lRet);

        // Create 30 values of random type in the Test Key.
        dwNumVals = TST_NUM_VALUE_TYPES;
        for (k=0; k<dwNumVals; k++)
        {
            // Create a value
            dwType=g_rgdwRegTypes[k];
            cbData = TST_VAL_BUF_SIZE;
            StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
            
            if (REG_NONE==dwType)
            {
                // Special case for REG_NONE because REG_NONE = 0. 
                // So the helper function will generate a type. 
                // We supress that by setting REG_BINARY, because
                // REG_NONE and REG_BINARY get same data generated. 
                dwType = REG_BINARY;
                if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
                    goto ErrorReturn;
                dwType=REG_NONE;
                
                lRet = RegSetValueEx(hKeyParent, rgszValName, 0, dwType, pbData, cbData);
                if (lRet)
                    REG_FAIL(RegSetValueEx, lRet);   
            }
            else
            {
                if (!Hlp_Write_Random_Value(hKeyParent, NULL, rgszValName, &dwType, pbData, &cbData))
                    goto ErrorReturn;
            }
            ASSERT(cbData!=0);
        }


        //
        // CASE: make sure we find all the different types.
        //
        dwIndex=0;
        cbData = TST_VAL_BUF_SIZE;
        ccValName=sizeof(rgszValName)/sizeof(TCHAR);
        memset(rgszValName, 65, sizeof(rgszValName));
        while (ERROR_SUCCESS == (lRet=RegEnumValue(hKeyParent, dwIndex, rgszValName, &ccValName, NULL, &dwType, pbData, &cbData)))
        {
            StringCchLength(_T("Value_"), STRSAFE_MAX_CCH, &szlen);
            dwTemp = _ttoi(rgszValName+ szlen);
            // A certain "Value_n" is linked to it's type via g_rgdwRegTypes[n]. Look at the way the values are written.
            if (dwType != g_rgdwRegTypes[dwTemp])
            {
                TRACE(TEXT("TEST_FAIL : Expecting type %d but got type %d\r\n"), g_rgdwRegTypes[dwTemp], dwType);
                DUMP_LOCN; 
                goto ErrorReturn;
            }

            ccValName=sizeof(rgszValName)/sizeof(TCHAR);
            cbData = TST_VAL_BUF_SIZE;
            memset(rgszValName, 65, sizeof(rgszValName));
            dwIndex++;            
        }
        VERIFY_REG_ERROR(RegEnumValue, lRet, ERROR_NO_MORE_ITEMS);


        //
        // CASE: Make sure the data is well formatted.
        //
        dwIndex=0;
        cbData = TST_VAL_BUF_SIZE;
        ccValName=sizeof(rgszValName)/sizeof(TCHAR);
        memset(rgszValName, 65, sizeof(rgszValName));
        while (ERROR_SUCCESS == (lRet=RegEnumValue(hKeyParent, dwIndex, rgszValName, &ccValName, NULL, &dwType, pbData, &cbData)))
        {
            switch(dwType)
            {
                case REG_NONE :
                case REG_BINARY :
                    break;
                    
                case REG_SZ :
                    // Data should be null terminated. 
                    dwTemp = cbData/sizeof(TCHAR);
                    if (((TCHAR*)pbData)[dwTemp-1] != _T('\0')) 
                    {
                        TRACE(TEXT("TEST_FAIL : Expecting pbData to be a NULL terminated string\r\n"));
                        TRACE(TEXT("Expecting pbData[%d]=NULL char. got pbData[%d]=%c\r\n"), dwTemp-1, (TCHAR*)pbData[dwTemp-1]);
                        DUMP_LOCN;
                        goto ErrorReturn;
                    }
                    break;
                    
                case REG_EXPAND_SZ :
                    break;
                                    
                case REG_DWORD :
                    break;
                    
                case REG_DWORD_BIG_ENDIAN :
                    break;
                    
                case REG_LINK :
                    break;
                    
                case REG_MULTI_SZ :
                    // Data should be terminated with 2 NULL chars.
                    dwTemp = cbData/sizeof(TCHAR);
                    if ((((TCHAR*)pbData)[dwTemp-2] != _T('\0')) ||
                        (((TCHAR*)pbData)[dwTemp-1] != _T('\0')) )
                    {   
                        TRACE(TEXT("TEST_FAIL : Expecting pbData to be a NULL terminated string\r\n"));
                        TRACE(TEXT("Expecting pbData[%d]=NULL, pbData[%d]=NULL char. got pbData[%d]=%c, pbData[%d]=%c\r\n"), 
                                            dwTemp-2, dwTemp-1, dwTemp-2, (TCHAR*)pbData[dwTemp-2], dwTemp-1, (TCHAR*)pbData[dwTemp-1]);
                        DUMP_LOCN;
                        goto ErrorReturn;
                    }
                    break;
                break;                    

                // Not sure what to verify here. 
                case REG_RESOURCE_LIST :
                case REG_FULL_RESOURCE_DESCRIPTOR :
                case REG_RESOURCE_REQUIREMENTS_LIST :
                    break;
                    
            }
            ccValName=sizeof(rgszValName)/sizeof(TCHAR);
            cbData = TST_VAL_BUF_SIZE;
            memset(rgszValName, 65, sizeof(rgszValName));
            memset(pbData, 66, cbData);
            dwIndex++;            
        }
        VERIFY_REG_ERROR(RegEnumValue, lRet, ERROR_NO_MORE_ITEMS);


        //
        // CASE: make sure it works of dwType=NULL.
        //
        REG_CLOSE_KEY(hKeyParent);    
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);

    }

    dwRetVal = TPR_PASS;
    
ErrorReturn :
    FREE(pbData);
    REG_CLOSE_KEY(hKeyParent);
    return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// More enumeration test
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_EnumVal_PbCb(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    DWORD dwRetVal=TPR_FAIL;
    HKEY hRoot=0, hKeyParent=0 ;
    TCHAR rgszValName[MAX_PATH]={0};
    DWORD ccValName=0;
    LONG lRet=0;
    DWORD dwIndex=0, dwRoot=0, dwNumVals=0, dwType=0, dwTemp=0, dwMaxcb=0, k ;
    PBYTE pbData=NULL;
    DWORD cbData=NULL;
    WCHAR szClassTemp[MAX_PATH];

    StringCchCopy(szClassTemp, MAX_PATH, TREGAPI_CLASS);	

    // The shell doesn't necessarily want us to execute the test.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    cbData=TST_VAL_BUF_SIZE;
    pbData = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
    CHECK_ALLOC(pbData);
    
    // Iterate over all pre-defined roots.
    for (dwRoot=0; dwRoot<TST_NUM_ROOTS; dwRoot++)
    {
        hRoot=g_l_RegRoots[dwRoot];
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);
        
        lRet = RegCreateKeyEx(hRoot, _T("Test_RegEnumKey"), NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyParent, NULL);
        if (lRet)
            REG_FAIL(RegCreateKeyEx, lRet);

        // Create 30 values of random type in the Test Key.
        dwNumVals=30;
        for (k=0; k<dwNumVals; k++)
        {
            // Create a value
            StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
            dwType=0;
            cbData = TST_VAL_BUF_SIZE;
            if (!Hlp_Write_Random_Value(hKeyParent, NULL, rgszValName, &dwType, pbData, &cbData))
                goto ErrorReturn;
            ASSERT(cbData!=0);
            if (cbData > dwMaxcb)
                dwMaxcb=cbData;
        }


        //
        // CASE:  Make sure RegQueryInfoKey returns the correct max cb.
        //
        if (-1 == (dwTemp = Hlp_GetMaxValueDataLen(hKeyParent)))
            goto ErrorReturn;
        if (dwTemp != dwMaxcb)
        {
            TRACE(TEXT("TEST_FAIL : RegQueryInfoKey says max data value is %d, but max should be %d\r\n"), dwTemp, dwMaxcb);
            DUMP_LOCN;
            goto ErrorReturn;
        }


        //
        // CASE: can pb and cb be NULL?
        //
        dwIndex=0;
        cbData = TST_VAL_BUF_SIZE;
        ccValName=sizeof(rgszValName)/sizeof(TCHAR);
        memset(rgszValName, 65, sizeof(rgszValName));
        while (ERROR_SUCCESS == (lRet=RegEnumValue(hKeyParent, dwIndex, rgszValName, &ccValName, NULL, &dwType, NULL, NULL)))
        {
            ccValName=sizeof(rgszValName)/sizeof(TCHAR);
            memset(rgszValName, 65, sizeof(rgszValName));
            dwIndex++;            
        }
        VERIFY_REG_ERROR(RegEnumValue, lRet, ERROR_NO_MORE_ITEMS);

        
        //
        // CASE: Can just pb=NULL?
        //
        dwIndex=0;
        ccValName=sizeof(rgszValName)/sizeof(TCHAR);
        memset(rgszValName, 65, sizeof(rgszValName));
        cbData=0;
        while (ERROR_SUCCESS == (lRet=RegEnumValue(hKeyParent, dwIndex, rgszValName, &ccValName, NULL, &dwType, NULL, &cbData)))
        {
            ccValName=sizeof(rgszValName)/sizeof(TCHAR);
            cbData=0;
            memset(rgszValName, 65, sizeof(rgszValName));
            dwIndex++;            
        }
        VERIFY_REG_ERROR(RegEnumValue, lRet, ERROR_NO_MORE_ITEMS);


        //
        // CASE: can pb, cb and dwType be NULL?
        //
        dwIndex=0;
        ccValName=sizeof(rgszValName)/sizeof(TCHAR);
        memset(rgszValName, 65, sizeof(rgszValName));
        cbData=0;
        while (ERROR_SUCCESS == (lRet=RegEnumValue(hKeyParent, dwIndex, rgszValName, &ccValName, NULL, NULL, NULL, NULL)))
        {
            ccValName=sizeof(rgszValName)/sizeof(TCHAR);
            cbData=0;
            memset(rgszValName, 65, sizeof(rgszValName));
            dwIndex++;            
        }
        VERIFY_REG_ERROR(RegEnumValue, lRet, ERROR_NO_MORE_ITEMS);

        REG_CLOSE_KEY(hKeyParent);    
        lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
        if (ERROR_ACCESS_DENIED == lRet)
            REG_FAIL(RegDeleteKey, lRet);

    }

    dwRetVal = TPR_PASS;
    
ErrorReturn :
    FREE(pbData);
    REG_CLOSE_KEY(hKeyParent);
    return dwRetVal;
}
