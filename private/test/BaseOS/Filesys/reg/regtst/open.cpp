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
// Tests the RegOpenKey API.
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RegOpenKey_hKey(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
	DWORD dwRetVal=TPR_FAIL;
	HKEY hRoot=0;
	HKEY hKeyParent=0, hKeyChild=0;
	TCHAR rgszKeyName[MAX_PATH]={65};
	LONG lRet=0;
	DWORD i=0, k=0;
	DWORD dwNumKeys=30;
	WCHAR szClassTemp[MAX_PATH];
	
	StringCchCopy(szClassTemp, MAX_PATH, TREGAPI_CLASS);	

	// The shell doesn't necessarily want us to execute the test. 
	if(uMsg != TPM_EXECUTE)
	{
		return TPR_NOT_HANDLED;
	}

	// Create a bunch of keys under hKeyParent = Test_RegEnumKey
	for (i=0; i<TST_NUM_ROOTS; i++)
	{
		hRoot=g_l_RegRoots[i];
		
		lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
		if (ERROR_ACCESS_DENIED == lRet)
			REG_FAIL(RegDeleteKey, lRet);
		
		// Create 30 keys under Root. 
		for (k=0; k<dwNumKeys; k++)
		{
            StringCchPrintf(rgszKeyName,MAX_PATH,_T("RootSubKey_%d"), k);
			
            lRet = RegCreateKeyEx(hRoot, rgszKeyName, NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyChild, NULL);
			if (lRet)
				REG_FAIL(RegCreateKeyEx, lRet);    
			
            lRet = RegCloseKey(hKeyChild);
			if (lRet)
				REG_FAIL(RegCloseKey, lRet);
		}
			
        lRet = RegCreateKeyEx(hRoot, _T("Test_RegEnumKey"), NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyParent, NULL);
		if (lRet)
			REG_FAIL(RegCreateKeyEx, lRet);

		// Create 30 keys under here. 
		for (k=0; k<dwNumKeys; k++)
		{
            StringCchPrintf(rgszKeyName,MAX_PATH,_T("Key_%d"), k);
			
            lRet = RegCreateKeyEx(hKeyParent, rgszKeyName, NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyChild, NULL);
			if (lRet)
				REG_FAIL(RegCreateKeyEx, lRet);    
			
            lRet = RegCloseKey(hKeyChild);
			if (lRet)
				REG_FAIL(RegCloseKey, lRet);
		}


		//
		// CASE: BVT
		//
		for (k=0; k<dwNumKeys; k++)
		{
            StringCchPrintf(rgszKeyName,MAX_PATH,_T("Key_%d"), k);
			if (ERROR_SUCCESS != (lRet=RegOpenKeyEx(hKeyParent, rgszKeyName, NULL, NULL, &hKeyChild)))
				REG_FAIL(RegOpenKeyEx, lRet);
            lRet = RegCloseKey(hKeyChild);
			if (lRet)
				REG_FAIL(RegClosekey, lRet);
		}
		hKeyChild=0;    


		// Open all the keys under the Root.
		for (k=0; k<dwNumKeys; k++)
		{
            StringCchPrintf(rgszKeyName,MAX_PATH,_T("RootSubKey_%d"), k);
			
			if (ERROR_SUCCESS != (lRet=RegOpenKeyEx(hRoot, rgszKeyName, NULL, NULL, &hKeyChild)))
				REG_FAIL(RegOpenKeyEx, lRet);
			
            lRet = RegCloseKey(hKeyChild);
			if (lRet)
				REG_FAIL(RegClosekey, lRet);
		}
		hKeyChild=0;    

		
		//
		// CASE: Invalid hKey
		//
		if (ERROR_SUCCESS == (lRet=RegOpenKeyEx((HKEY)0, rgszKeyName, NULL, NULL, &hKeyChild)))
		{
			TRACE(TEXT("TEST_FAIL : REgOpenKey should have failed for an invalid hKey(=0) \r\n"));
			DUMP_LOCN;
			goto ErrorReturn;
		}

		TRACE(TEXT(">> NOTE : The next test will cause an exception in filesys\r\n"));
		if (ERROR_SUCCESS == (lRet=RegOpenKeyEx((HKEY)-1, rgszKeyName, NULL, NULL, &hKeyChild)))
		{
			TRACE(TEXT("TEST_FAIL : REgOpenKey should have failed for an invalid hKey(=-1) \r\n"));
			DUMP_LOCN;
			goto ErrorReturn;
		}


		//
		// CASE: Can I open hRoot + NULL?
		//
		if (ERROR_SUCCESS != (lRet=RegOpenKeyEx(hRoot, NULL, NULL, NULL, &hKeyChild)))
		{
			TRACE(TEXT("TEST_FAIL : Could not open the root key using pszKeyName=NULL\r\n"));
			REG_FAIL(RegOpenKeyEx, lRet);
		}

        lRet = RegCloseKey(hKeyChild);
		if (lRet)
			REG_FAIL(RegCloseKey, lRet);
		hKeyChild=0;
		
		// Reset and next iteration of for. 
        lRet = RegCloseKey(hKeyParent);
		if (lRet)
			REG_FAIL(RegCloseKey, lRet);
		hKeyParent=0;
		
		lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
		if (ERROR_ACCESS_DENIED == lRet)
			REG_FAIL(RegDeleteKey, lRet);
	}

	dwRetVal=TPR_PASS;
	
ErrorReturn :
	REG_CLOSE_KEY(hKeyParent);
	REG_CLOSE_KEY(hKeyChild);
	return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// More test on RegOpenKey
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RegOpenKey_pszSubKey(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
	DWORD dwRetVal=TPR_FAIL;
	HKEY hRoot=0;
	HKEY hKeyParent=0, hKeyChild=0;
	TCHAR rgszKeyName[MAX_PATH]={65};
	LONG lRet=0;
	DWORD i=0, j=0, k=0;
	DWORD dwNumKeys=30;
	HKEY rghKeyTemp[20]={0};
	DWORD dwTemp=0;
	WCHAR szClassTemp[MAX_PATH];
	
	StringCchCopy(szClassTemp, MAX_PATH, TREGAPI_CLASS);	

	// The shell doesn't necessarily want us to execute the test. 
	if(uMsg != TPM_EXECUTE)
	{
		return TPR_NOT_HANDLED;
	}

	// Create a bunch of keys under hKeyParent = Test_RegEnumKey
	for (i=0; i<TST_NUM_ROOTS; i++)
	{
		hRoot=g_l_RegRoots[i];
		lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
		if (ERROR_ACCESS_DENIED == lRet)
			REG_FAIL(RegDeleteKey, lRet);
		
        lRet = RegCreateKeyEx(hRoot, _T("Test_RegEnumKey"), NULL, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyParent, NULL);
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
		hKeyChild=0;


		//
		// CASE: Non-existent key
		//
		StringCchCopy(rgszKeyName, MAX_PATH, _T("non-existent"));
		if (ERROR_SUCCESS == (lRet = RegOpenKeyEx(hKeyParent, rgszKeyName, NULL, NULL, &hKeyChild)))
		{
			TRACE(TEXT("TEST_FAIL : Not expecting to be able to open a non-existent key \r\n"));
			DUMP_LOCN;
			goto ErrorReturn;
		}
		VERIFY_REG_ERROR(RegOpenKeyEx, lRet, ERROR_FILE_NOT_FOUND);

		
		//
		// CASE: Empty string
		//
		if (ERROR_SUCCESS != (lRet = RegOpenKeyEx(hKeyParent, _T(""), NULL, NULL, &hKeyChild)))
		{
			TRACE(TEXT("TEST_FAIL : RegOpenKey failed for an empty string. Should have opened the same key\r\n"));
			REG_FAIL(RegOpenKeyEx, lRet);
		}
		if (!Hlp_CmpRegKeys(hKeyParent, hKeyChild))
		{
			TRACE(TEXT("TEST_FAIL : The opened key via RegOpenKeyEx(\"\")is not the same\r\n"));
			DUMP_LOCN;
			goto ErrorReturn;
		}
        lRet=RegCloseKey(hKeyChild);
		if (lRet)
			REG_FAIL(RegCloseKey, lRet);
		hKeyChild=0;

		
		//
		// CASE: NULL string
		//
		if (ERROR_SUCCESS != (lRet = RegOpenKeyEx(hKeyParent, NULL, NULL, NULL, &hKeyChild)))
		{
			TRACE(TEXT("TEST_FAIL : RegOpenKey failed for an empty string. Should have opened the same key\r\n"));
			REG_FAIL(RegOpenKeyEx, lRet);
		}
		if (!Hlp_CmpRegKeys(hKeyParent, hKeyChild))
		{
			TRACE(TEXT("TEST_FAIL : The opened key via RegOpenKeyEx(\"\")is not the same\r\n"));
			DUMP_LOCN;
			goto ErrorReturn;
		}
        lRet=RegCloseKey(hKeyChild);
		if (lRet)
			REG_FAIL(RegCloseKey, lRet);
		hKeyChild=0;

		
		//
		// CASE: Open the key names in the transposed case.
		//
		for (k=0; k<dwNumKeys; k++)
		{
            StringCchPrintf(rgszKeyName,MAX_PATH,_T("kEY_%d"), k);
			if (ERROR_SUCCESS != (lRet = RegOpenKeyEx(hKeyParent, rgszKeyName, NULL, NULL, &hKeyChild)))
			{
				TRACE(TEXT("TEST_FAIL : RegOpenKey failed for key %s. \r\n"), rgszKeyName);
				REG_FAIL(RegOpenKeyEx, lRet);
			}

            lRet=RegCloseKey(hKeyChild);
			if (lRet)
				REG_FAIL(RegCloseKey, lRet);
			hKeyChild=0;
		}
		hKeyChild=0;


		//
		// CASE: Open the same key multiple times.
		//
		for (k=0; k<dwNumKeys; k++)
		{
			dwTemp=sizeof(rghKeyTemp)/sizeof(HKEY);
            StringCchPrintf(rgszKeyName,MAX_PATH,_T("Key_%d"), k);
			// Open the same key many times.
			for (j=0; j<dwTemp; j++)
			{
				if (ERROR_SUCCESS != (lRet = RegOpenKeyEx(hKeyParent, rgszKeyName, NULL, NULL, &(rghKeyTemp[j]))))
				{
					TRACE(TEXT("TEST_FAIL : RegOpenKey failed for key %s. \r\n"), rgszKeyName);
					REG_FAIL(RegOpenKeyEx, lRet);
				}
			}

			for (j=0; j<dwTemp; j++)
			{
                lRet=RegCloseKey(rghKeyTemp[j]);
				if (lRet)
					REG_FAIL(RegCloseKey, lRet);
				rghKeyTemp[j]=0;
			}
		}


		//
		// CASE: Opening just a substring should fail
		//
        StringCchCopy(rgszKeyName,MAX_PATH, _T("Key"));
		if (ERROR_SUCCESS == (lRet = RegOpenKeyEx(hKeyParent, rgszKeyName, NULL, NULL, &hKeyChild)))
		{
			TRACE(TEXT("TEST_FAIL : RegOpenKey should have failed for key %s. \r\n"), rgszKeyName);
			DUMP_LOCN;
			goto ErrorReturn;
		}


		//
		// CASE: Should fail if hKey is NULL
		//
		if (ERROR_SUCCESS == (lRet = RegOpenKeyEx(hKeyParent, NULL, NULL, NULL, NULL)))
		{
			TRACE(TEXT("TEST_FAIL : RegOpenKey should have failed for &hkey=NULL \r\n"));
			DUMP_LOCN;
			goto ErrorReturn;
		}

        lRet=RegCloseKey(hKeyParent);
		if (lRet)
			REG_FAIL(RegCloseKey, lRet);
		hKeyParent=0;
		
		lRet = RegDeleteKey(hRoot, _T("Test_RegEnumKey"));
		if (ERROR_ACCESS_DENIED == lRet)
			REG_FAIL(RegDeleteKey, lRet);
	}

	dwRetVal=TPR_PASS;
	
ErrorReturn :
	REG_CLOSE_KEY(hKeyParent);
	REG_CLOSE_KEY(hKeyChild);
	return dwRetVal;
}

