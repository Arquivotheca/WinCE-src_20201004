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
HKEY g_l_RegRoots[] = { HKEY_LOCAL_MACHINE,
						HKEY_CURRENT_USER, 
						HKEY_CLASSES_ROOT, 
						HKEY_USERS };

///////////////////////////////////////////////////////////////////////////////
//
// Tests the RegCreateKey API.
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_LevelLimit(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
	DWORD dwRetVal=TPR_FAIL;
	HKEY hRoot=0;
	int level=0;
	HKEY rghKeyNew[TST_MAX_LEVEL]={0};
	TCHAR rgszKey[MAX_PATH]={0};
	DWORD dwDispo=0;
	LONG lRet=0;
    size_t szlen = 0;
	int  k=0;

	// The shell doesn't necessarily wants us to execute the test.
	if(uMsg != TPM_EXECUTE)
	{
		return TPR_NOT_HANDLED;
	}    

	for (k=0; k<TST_NUM_ROOTS; k++)
	{
		hRoot=g_l_RegRoots[k];
		for (level=0; level<TST_MAX_LEVEL; level++)
		{
			// Create the key
            StringCchPrintf(rgszKey,MAX_PATH,_T("Level%d"), level);
			if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKey, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &(rghKeyNew[level]), &dwDispo)))
			{
				if (TST_MAX_LEVEL == (level+1))
				{
					TRACE(TEXT("RegCreate correctly failed because of MAX level violation. 0x%x\r\n"), lRet);
					break;
				}
				TRACE(TEXT("Failed to create key 0x%x\\%s\r\n"), hRoot, rgszKey);
				REG_FAIL(RegCreateKeyEx, lRet);
			}
			// Check the disposition
			if (REG_CREATED_NEW_KEY != dwDispo)
			{
				TRACE(TEXT("TEST_FAIL : Did not get dwDispo=new, instead got 0x%x\r\n"), dwDispo);
				REG_FAIL(RegCreateKeyEx, 0);
			}

			// Create a value in this key
			if (!L_WriteRandomVal(rghKeyNew[level]))
				goto ErrorReturn;
			
			hRoot=rghKeyNew[level];
		}

		// Close all keys
		for (level=0; level<TST_MAX_LEVEL; level++)
			REG_CLOSE_KEY(rghKeyNew[level]);
	}

	for (k=0; k<TST_NUM_ROOTS; k++)
	{
		// Cleanup
		hRoot=g_l_RegRoots[k];
		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, _T("Level0"))))
			REG_FAIL(RegDeleteKey, lRet);
	}

	// Now try to give a key path that is 16 levels deep.
	for (k=0; k<TST_NUM_ROOTS; k++)
	{
		hRoot = g_l_RegRoots[k];
		rgszKey[0]=_T('\0');

		for (level=0; level<TST_MAX_LEVEL-1; level++)
		{
			if (0==level)
                StringCchPrintf(rgszKey,MAX_PATH,_T("Level%d"), level);
			else
                StringCchPrintf(rgszKey,MAX_PATH,_T("%s\\Level%d"), rgszKey, level);
            
            StringCchLength(rgszKey, STRSAFE_MAX_CCH, &szlen);
            ASSERT(szlen < MAX_PATH);
		}

		TRACE(TEXT("Creating subkey %s\r\n"), rgszKey);
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKey, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &(rghKeyNew[0]), &dwDispo)))
		{
			TRACE(TEXT("Failed to create key 0x%x\\%s\r\n"), hRoot, rgszKey);
			REG_FAIL(RegCreateKeyEx, lRet);
		}

		// Check the disposition
		if (REG_CREATED_NEW_KEY != dwDispo)
		{
			TRACE(TEXT("TEST_FAIL : Did not get dwDispo=new, instead got 0x%x\r\n"), dwDispo);
			REG_FAIL(RegCreateKeyEx, 0);
		}
		REG_CLOSE_KEY(rghKeyNew[0]);

		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, _T("Level0"))))
			REG_FAIL(RegDeleteKey, lRet);


		//
		// CASE: Try to exceed the depth limit.
		//
        StringCchPrintf(rgszKey,MAX_PATH,_T("%s\\Level%d"), rgszKey, TST_MAX_LEVEL-1);
		StringCchLength(rgszKey, STRSAFE_MAX_CCH, &szlen);
        ASSERT(szlen < MAX_PATH);
		
		TRACE(TEXT("Creating subkey %s\r\n"), rgszKey);
		if (ERROR_SUCCESS == (lRet=RegCreateKeyEx(hRoot, rgszKey, 0, NULL,GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &(rghKeyNew[0]), &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : Should have failed to create key 0x%x\\%s\r\n"), hRoot, rgszKey);
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		else
			TRACE(TEXT("success.\r\n"));
		
		if (TRUE == Hlp_IsKeyPresent(hRoot, _T("Level0")))
		{
			g_dwFailCount++;
			TRACE(TEXT("FAIL\r\n"));
			TRACE(TEXT("Not expecting key 0x%x\\%s to be present.\r\n"), hRoot, _T("Level0"));
			goto ErrorReturn;
		}
		
		TRACE(TEXT("Testing sublevel depth ok\r\n"));
		lRet =RegDeleteKey(hRoot, _T("Level0"));
		if (lRet == ERROR_ACCESS_DENIED)
			REG_FAIL(RegDeleteKey, lRet);
	}    

	dwRetVal = TPR_PASS;

ErrorReturn :
	return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// Test variations of subkeys
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_Subkey_Variations(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
	DWORD dwRetVal=TPR_FAIL;
	HKEY hRoot=0;
	HKEY hKeyNew=0;
	HKEY hKeyOpen=0;
	TCHAR rgszKeyName[MAX_PATH]={0};
	LONG lRet=0;
	DWORD dwDispo=0;
	int level=0;
	int k=0;
	BYTE pbData[100]={0};
	DWORD cbData;
	DWORD dwType=0;
    size_t szlen = 0;
	// The shell doesn't necessarily want us to execute the test.
	if(uMsg != TPM_EXECUTE)
	{
		return TPR_NOT_HANDLED;
	}

	for (k=0; k<TST_NUM_ROOTS; k++)
	{
		hRoot = g_l_RegRoots[k];
		lRet = RegDeleteKey(hRoot, _T("FooKey"));
		if (lRet == ERROR_ACCESS_DENIED)
			REG_FAIL(RegDeleteKey, lRet);


		//
		// CASE: Key ending with "\" - should succeed
		//
		StringCchCopy(rgszKeyName, MAX_PATH, _T("FooKey\\"));
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyNew, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for pszKey=%s\r\n"), rgszKeyName);
			REG_FAIL(RegCreateKeyEx, lRet);
		} 
		else 
		{
			RegCloseKey(hKeyNew);
		}

		// Registry code will create FooKey and then fail, so we should clean up here. 
		lRet = RegDeleteKey(hRoot, _T("FooKey"));
		if (lRet == ERROR_ACCESS_DENIED)
			REG_FAIL(RegDeleteKey, lRet);


		//
		// CASE: Key name beginning with "\" - should pass. 
		//
		StringCchCopy(rgszKeyName, MAX_PATH, _T("\\FooKey"));
		ASSERT(FALSE == Hlp_IsKeyPresent(hRoot, rgszKeyName));
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyNew, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s\r\n"), hRoot, rgszKeyName);
			REG_FAIL(RegCreateKey, lRet);
		}
		REG_CLOSE_KEY(hKeyNew);
		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, rgszKeyName)))
			REG_FAIL(RegDeleteKey, lRet);


		//
		// CASE: pszSubKey = NULL - should pass.
		// 
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, NULL, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyNew, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for pszKey=NULL\r\n"));
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		REG_CLOSE_KEY(hKeyNew);


		// 
		// CASE: RegCreateKey NULL should also work on subkeys.
		//
		StringCchCopy(rgszKeyName, MAX_PATH, _T("FooKey"));
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyNew, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for pszKey=%s\r\n"), rgszKeyName);
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hKeyNew, NULL, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyOpen, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey should have opened hKey for pszSubKey=NULL\r\n"));
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		REG_CLOSE_KEY(hKeyNew);
		REG_CLOSE_KEY(hKeyOpen);
		lRet = RegDeleteKey(hRoot, rgszKeyName);
		if (lRet == ERROR_ACCESS_DENIED)
			REG_FAIL(RegDeleteKey, lRet);

		
		//
		// CASE: pszKeyName is 255 characters long
		//
		if (!Hlp_GenStringData(rgszKeyName, 255+1, TST_FLAG_ALPHA_NUM))
			goto ErrorReturn;
        StringCchLength(rgszKeyName, STRSAFE_MAX_CCH, &szlen);
		ASSERT(255 == szlen);
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey should have failed for pszKey=NULL\r\n"));
			REG_CLOSE_KEY(hKeyNew);
			lRet = RegDeleteKey(hRoot, rgszKeyName);
			if (lRet == ERROR_ACCESS_DENIED)
				REG_FAIL(RegDeleteKey, lRet);
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		REG_CLOSE_KEY(hKeyNew);
		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, rgszKeyName)))
			REG_FAIL(RegDeleteKey, lRet);


		//
		// CASE: pszKeyName is more than 255 chars long
		//
		if (!Hlp_GenStringData(rgszKeyName, 256+1, TST_FLAG_ALPHA_NUM))
			goto ErrorReturn;
		StringCchLength(rgszKeyName, STRSAFE_MAX_CCH, &szlen);
		ASSERT(256 == szlen);
		if (ERROR_SUCCESS == (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey should have failed for pszKey=NULL\r\n"));
			REG_CLOSE_KEY(hKeyNew);
			lRet = RegDeleteKey(hRoot, rgszKeyName);
			if (lRet == ERROR_ACCESS_DENIED)
				REG_FAIL(RegDeleteKey, lRet);
			
			REG_FAIL(RegCreateKeyEx, lRet);
		}


		//
		// CASE: same key name with different classes
		//  
		StringCchCopy(rgszKeyName, MAX_PATH, _T("FooKey"));
		// Create a key
		WCHAR szClassTemp[MAX_PATH];
		StringCchCopy(szClassTemp, MAX_PATH, _T("Class1"));
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyNew, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for pszKey=%s\r\n"), rgszKeyName);
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		REG_CLOSE_KEY(hKeyNew);
		if (REG_CREATED_NEW_KEY != dwDispo)
		{
			TRACE(TEXT("Expecting new key to be created, but got dospo=0x%x\r\n"), dwDispo);
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		//  Try to create the same key with a different class - should fail.
		StringCchCopy(szClassTemp, MAX_PATH, _T("DifferentClass"));
		if (ERROR_SUCCESS == (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, szClassTemp, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL, &hKeyNew, &dwDispo)))
		{
			REG_CLOSE_KEY(hKeyNew);
			if (REG_OPENED_EXISTING_KEY != dwDispo)
			{
				TRACE(TEXT("TEST_FAIL : RegCreateKey should have failed for pszKey=%s with different class\r\n"), rgszKeyName);
				REG_FAIL(RegCreateKeyEx, lRet);
			}
		}
		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, rgszKeyName)))
			REG_FAIL(RegDeleteKey, lRet);


		//
		// CASE: case-sensitive keynames.
		//
		StringCchCopy(rgszKeyName, MAX_PATH, _T("FooKey"));
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for pszKey=%s\r\n"), rgszKeyName);
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		REG_CLOSE_KEY(hKeyNew);
        StringCchLength(rgszKeyName, STRSAFE_MAX_CCH, &szlen);
		CharUpperBuff(rgszKeyName, szlen);
		if (ERROR_SUCCESS == (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, &dwDispo)))
		{
			REG_CLOSE_KEY(hKeyNew);
			if (REG_OPENED_EXISTING_KEY != dwDispo)
			{
				TRACE(TEXT("TEST_FAIL : RegCreateKey should have failed for uppercase pszKey=%s\r\n"), rgszKeyName);
				REG_FAIL(RegCreateKeyEx, lRet);
			}
		}
		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, rgszKeyName)))
			REG_FAIL(RegDeleteKey, lRet);


		//
		// CASE: Opened hKey, same keyname
		//
		StringCchCopy(rgszKeyName, MAX_PATH, _T("Level1\\Level2"));
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for pszKey=%s\r\n"), rgszKeyName);
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		REG_CLOSE_KEY(hKeyNew);
		
		if (ERROR_SUCCESS != (lRet=RegOpenKeyEx(hRoot, _T("Level1"), 0, 0, &hKeyOpen)))
			REG_FAIL(RegOpenKeyEx, lRet);
		
		StringCchCopy(rgszKeyName, MAX_PATH, _T("Level2"));	
		if (ERROR_SUCCESS == (lRet=RegCreateKeyEx(hKeyOpen, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, &dwDispo)))
		{
			REG_CLOSE_KEY(hKeyNew);
			if (REG_OPENED_EXISTING_KEY != dwDispo)
			{
				TRACE(TEXT("TEST_FAIL : RegCreateKey should have failed for pszKey=%s\r\n"), rgszKeyName);
				REG_FAIL(RegCreateKeyEx, lRet);
			}
		}
		REG_CLOSE_KEY(hKeyOpen);
		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, _T("Level1")) ))
			REG_FAIL(RegDeleteKey, lRet);

		
		//
		// CASE: Does it create the keys in the path. 1\2\3 etc..
		//
		for (level=0; level<5; level++)
		{
			if (0==level)
                StringCchPrintf(rgszKeyName,MAX_PATH,_T("Level%d"), level);
			else
                StringCchPrintf(rgszKeyName,MAX_PATH,_T("%s\\Level%d"), rgszKeyName, level);
            StringCchLength(rgszKeyName, STRSAFE_MAX_CCH, &szlen);
			ASSERT(szlen < MAX_PATH);
		}
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for pszKey=%s\r\n"), rgszKeyName);
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		REG_CLOSE_KEY(hKeyNew);

		//  Check that all the keys in the path exist.
		for (level=0; level<5; level++)
		{
			if (0==level)
                StringCchPrintf(rgszKeyName,MAX_PATH,_T("Level%d"), level);
			else
                StringCchPrintf(rgszKeyName,MAX_PATH,_T("%s\\Level%d"), rgszKeyName, level);
            StringCchLength(rgszKeyName, STRSAFE_MAX_CCH, &szlen);
			ASSERT(szlen < MAX_PATH);
			if (FALSE == Hlp_IsKeyPresent(hRoot, rgszKeyName))
			{
				TRACE(TEXT("TEST_FAIL : Key %s doesn't exist in the path\r\n"), rgszKeyName);
				DUMP_LOCN;
				goto ErrorReturn;
			}
		}
		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, _T("Level0"))))
			REG_FAIL(RegDeleteKey, lRet);


		//
		// CASE: Creating a key "" - should behave like RegOpenKey
		//  ------------------------------------        
		StringCchCopy(rgszKeyName, MAX_PATH, _T(""));	
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for pszKey=\"\"\r\n"));
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		REG_CLOSE_KEY(hKeyNew);


		//
		// CASE: Numeric key names
		//
		if (!Hlp_GenStringData(rgszKeyName, 10, TST_FLAG_NUMERIC))
			goto ErrorReturn;
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for pszKey=%s\r\n"), rgszKeyName);
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		REG_CLOSE_KEY(hKeyNew);
		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, rgszKeyName)))
			REG_FAIL(RegDeleteKey, lRet);


		//
		// CASE: Non ALPHA_NUMERIC key names
		//
		StringCchCopy(rgszKeyName, MAX_PATH, _T("!@#$%^&*()/.,?><"));
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for pszKey=%s\r\n"), rgszKeyName);
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		REG_CLOSE_KEY(hKeyNew);
		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, rgszKeyName)))
			REG_FAIL(RegDeleteKey, lRet);


		//
		// CASE: Key "\\\\" (4 slashes). The canonicalization process will strip this, transforming it into an empty string
		// RegCreateKeyEx will just behave like RegOpenKey
		//
		StringCchCopy(rgszKeyName, MAX_PATH, _T("\\\\\\\\"));
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for pszKey=\\\\\\\\\r\n"));
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		REG_CLOSE_KEY(hKeyNew);


		//
		// CASE: If a value called Foo is present, can we still create a key called FOO. 
		//
		StringCchCopy(rgszKeyName, MAX_PATH, _T("FooKey"));
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for pszKey=%s\r\n"), rgszKeyName);
			REG_FAIL(RegCreateKeyEx, lRet);
		}

		// Set a value "FooValue"        
		cbData = sizeof(pbData)/sizeof(BYTE);
		if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
			goto ErrorReturn;
		if (ERROR_SUCCESS != (lRet=RegSetValueEx(hKeyNew, _T("FooValue"), 0, dwType, pbData, cbData)))
			REG_FAIL(RegSetValueEx, lRet);

		// Now try to create a key "FooValue"
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hKeyNew, _T("FooValue"), 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyOpen, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for pszKey=%s\r\n"), _T("FooValue"));
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		if (REG_CREATED_NEW_KEY != dwDispo)
		{
			TRACE(TEXT("TEST_FAIL : dwDisposition is not REG_CREATED_NEW (dwDispo=0x%x)\r\n"), dwDispo);
			DUMP_LOCN;
			goto ErrorReturn;
		}

		REG_CLOSE_KEY(hKeyOpen);
		REG_CLOSE_KEY(hKeyNew);        
		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, rgszKeyName)))
			REG_FAIL(RegDeleteKey, lRet);
		
	}


	//
	// CASE: Same keyname on all roots - should PASS
	//
	StringCchCopy(rgszKeyName, MAX_PATH, _T("FooKey"));
	for (k=0; k<TST_NUM_ROOTS; k++)
	{
		hRoot = g_l_RegRoots[k];
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for pszKey=%s\r\n"), rgszKeyName);
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		REG_CLOSE_KEY(hKeyNew);
	}
	for (k=0; k<TST_NUM_ROOTS; k++)
	{
		hRoot = g_l_RegRoots[k];
		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, rgszKeyName)))
			REG_FAIL(RegDeleteKey, lRet);
	}
   
	dwRetVal = TPR_PASS;

ErrorReturn :
	REG_CLOSE_KEY(hKeyNew);
	REG_CLOSE_KEY(hKeyOpen);
	return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// Test RegCreateKeyEx with invalid parameters
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_Invalid_Params(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
	DWORD dwRetVal=TPR_FAIL;
	HKEY hRoot=0;
	HKEY hKeyNew=0;
	LONG lRet=0;
	DWORD dwDispo=0;

	// The shell doesn't necessarily want us to execute the test.
	if(uMsg != TPM_EXECUTE)
	{
		return TPR_NOT_HANDLED;
	}


	//
	// CASE: Invalid hRoot. This will cause an exception in debug builds
	//
	hRoot = (HKEY)-1;
	TRACE(TEXT("Next test will cause exception in debug builds....\r\n"));
	if (ERROR_SUCCESS == (lRet=RegCreateKeyEx(hRoot, _T("FooKey"), 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, &dwDispo)))
	{
		TRACE(TEXT("TEST_FAIL : RegCreateKeyEx should have failed for an invalid hRoot=0x%x\r\n"), hRoot);
		REG_FAIL(RegCreateKeyEx, lRet);
	}


	//
	// CASE: Zero hRoot
	//
	hRoot = 0;
	if (ERROR_SUCCESS == (lRet=RegCreateKeyEx(hRoot, _T("FooKey"), 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, &dwDispo)))
	{
		TRACE(TEXT("TEST_FAIL : RegCreateKeyEx should have failed for an invalid hRoot=0x%x\r\n"), hRoot);
		REG_FAIL(RegCreateKeyEx, lRet);
	}


	//
	// CASE: hKeyNew is NULL
	//
	hRoot = HKEY_LOCAL_MACHINE;
	if (ERROR_SUCCESS == (lRet=RegCreateKeyEx(hRoot, _T("FooKey"), 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  NULL, &dwDispo)))
	{
		TRACE(TEXT("TEST_FAIL : RegCreateKeyEx should have failed for an invalid NULL hKeyNew \r\n"));
		REG_FAIL(RegCreateKeyEx, lRet);
	}

	dwRetVal = TPR_PASS;
	
ErrorReturn :
	REG_CLOSE_KEY(hKeyNew);
	return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// Stress test registry creation
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_Stress(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
	DWORD dwRetVal=TPR_FAIL;
	HKEY hRoot=0;
	HKEY hKeyNew=0;
	TCHAR rgszKeyName[MAX_PATH]={0};
	LONG lRet=0;
	DWORD dwDispo=0;
	int k=0;
	DWORD dwNewKey=0;
	DWORD dwKeyDeleted=0;

	// The shell doesn't necessarily want us to execute the test. 
	if(uMsg != TPM_EXECUTE)
	{
		return TPR_NOT_HANDLED;
	}

	for (k=0; k<TST_NUM_ROOTS; k++)
	{
		hRoot=g_l_RegRoots[k];
		dwNewKey=0;

		// Create a lot of registry keys
		while (dwNewKey<TST_LARGE_NUM_KEYS)
		{
            StringCchPrintf(rgszKeyName,MAX_PATH, _T("Key%d"), dwNewKey++);
			if (ERROR_SUCCESS != (lRet=RegCreateKeyEx( hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, &dwDispo)))
				REG_FAIL(RegCreateKeyEx, lRet);
			REG_CLOSE_KEY(hKeyNew);
		}
		TRACE(TEXT("Last Key Created = %s\r\n"), rgszKeyName);

		dwKeyDeleted=0;
		while (dwKeyDeleted < 2000 )
		{
            StringCchPrintf(rgszKeyName,MAX_PATH, _T("Key%d"), dwKeyDeleted++);
			if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, rgszKeyName)))
			{
				TRACE(TEXT("Failed to delete key %s\r\n"), rgszKeyName);
				REG_FAIL(RegDeleteKey, lRet);
			}
		}
		
		if (dwNewKey != dwKeyDeleted)
		{
			TRACE(TEXT("TEST_FAIL : Created %d keys, but deleted only %d\r\n"), dwNewKey, dwKeyDeleted);
			goto ErrorReturn;
		}
		else
			TRACE(TEXT("Test Passed : created %d keys, and deleted %d keys\r\n"), dwNewKey, dwKeyDeleted);
		
	}
   
	dwRetVal = TPR_PASS;

ErrorReturn :
	return dwRetVal;
}
