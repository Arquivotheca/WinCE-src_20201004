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
// Test delete registry's tree
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_DeleteTree(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
	DWORD dwRetVal=TPR_FAIL;
	HKEY hRoot=0;
	HKEY hKeyNew=0;
	HKEY rghKeyNew[TST_MAX_LEVEL]={0};
	TCHAR rgszKeyName[MAX_PATH]={0};
	LONG lRet=0;
	DWORD dwDispo=0;
	int k=0;
	int level=0;

	// The shell doesn't necessarily want us to execute the test. 
	if(uMsg != TPM_EXECUTE)
	{
		return TPR_NOT_HANDLED;
	}

	for (k=0; k<TST_NUM_ROOTS; k++)
	{
		hRoot=g_l_RegRoots[k];
		// Create the keys writing some values in them.
		for (level=0; level<TST_MAX_LEVEL-1; level++)
		{
			// Create the key
            StringCchPrintf(rgszKeyName,MAX_PATH,_T("Lev%d"), level);
			if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &(rghKeyNew[level]), &dwDispo)))
			{
				TRACE(TEXT("Failed to create key 0x%x\\%s\r\n"), hRoot, rgszKeyName);
				REG_FAIL(RegCreateKeyEx, lRet);
			}

			// Create a value in this key
			if (!L_WriteRandomVal(rghKeyNew[level]))
				goto ErrorReturn;
			
			hRoot=rghKeyNew[level];
		}
		hRoot = g_l_RegRoots[k];

		// Close all keys
		for (level=0; level<TST_MAX_LEVEL; level++)
			REG_CLOSE_KEY(rghKeyNew[level]);


		//
		// CASE: Open Key to Lev 5
		//
		StringCchCopy(rgszKeyName, MAX_PATH, _T("Lev0\\Lev1\\Lev2\\Lev3\\Lev4\\Lev5"));
		if (ERROR_SUCCESS != (lRet=RegOpenKeyEx(hRoot, rgszKeyName, 0, NULL, &hKeyNew)))
		{
			TRACE(TEXT("Failed to open key 0x%x\\%s\r\n"), hRoot, rgszKeyName);
			REG_FAIL(RegOpenKey, lRet);
		}

		//
		// CASE: Delete Lev 6 - this should work
		//
		StringCchCat(rgszKeyName, MAX_PATH, _T("\\Lev6"));
		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, rgszKeyName)))
		{
			TRACE(TEXT("TestFail : Failed to delete Key 0x%x\\%s\r\n"), hRoot, rgszKeyName);
			REG_FAIL(RegDeleteKey, lRet);
		}

		// Make sure keys Lev6 and below are deleted.
		StringCchCopy(rgszKeyName, MAX_PATH, _T("Lev0\\Lev1\\Lev2\\Lev3\\Lev4\\Lev5"));
		for (level=6; level<TST_MAX_LEVEL; level++)
		{
            StringCchPrintf(rgszKeyName,MAX_PATH,_T("%s\\Lev%d"), rgszKeyName, level);
			if (TRUE == Hlp_IsKeyPresent(hRoot, rgszKeyName))
			{
				TRACE(TEXT("TestFail : Key 0x%x\\%s should not exist since it was deleted.\r\n"), hRoot, rgszKeyName);
				DUMP_LOCN;
				goto ErrorReturn;
			}
		}

		if (!L_WriteRandomVal(hKeyNew))
		{
			TRACE(TEXT("Writing  a value to an open key failed.\r\n"));
			goto ErrorReturn;
		}

		// All keys through level 5 should exist
		StringCchCopy(rgszKeyName, MAX_PATH, _T(""));
		for (level=0; level<6; level++)
		{
            StringCchPrintf(rgszKeyName,MAX_PATH,_T("%sLev%d"), rgszKeyName, level);
			if (FALSE == Hlp_IsKeyPresent(hRoot, rgszKeyName))
			{
				TRACE(TEXT("TestFail : Failed to find key 0x%x\\%s\r\n"), hRoot, rgszKeyName);
				DUMP_LOCN;
				goto ErrorReturn;
			}
			StringCchCat(rgszKeyName, MAX_PATH, _T("\\"));
		}

		REG_CLOSE_KEY(hKeyNew);

		// Cleanup.
		lRet = RegDeleteKey(hRoot, _T("Lev0"));
		if (ERROR_SUCCESS != lRet)
			REG_FAIL(RegDeleteKey, lRet);

		// Check return value
		if (ERROR_SUCCESS == (lRet = RegDeleteKey(hRoot, _T("Lev0"))))
		{
			TRACE(TEXT("TEST_FAIL : RegDeleteKey should have failed for 0x%x\\Lev0"), hRoot);
			goto ErrorReturn;
		}
		VERIFY_REG_ERROR(RegDeleteKey, lRet, ERROR_FILE_NOT_FOUND);
	}

	dwRetVal = TPR_PASS;
	
ErrorReturn :
	REG_CLOSE_KEY(hKeyNew)
	return dwRetVal;
}



///////////////////////////////////////////////////////////////////////////////
//
// Test deleting various registry keys
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_SubKeyVariations(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
	DWORD dwRetVal=TPR_FAIL;
	HKEY hRoot=0;
	HKEY hKeyNew=0;
	LONG lRet=0;
	int k=0;
	TCHAR rgszKeyName[MAX_PATH]={0};
	TCHAR rgszLongKeyName[500]={0};
	TCHAR ccLongKeyName=sizeof(rgszLongKeyName)/sizeof(TCHAR);
	DWORD dwDispo=0;
    size_t szlen = 0;

	// The shell doesn't necessarily want us to execute the test. 
	if(uMsg != TPM_EXECUTE)
	{
		return TPR_NOT_HANDLED;
	}

	for (k=0; k<TST_NUM_ROOTS; k++)
	{
		hRoot=g_l_RegRoots[k];


		//
		// CASE: Deleting hRoot, NULL - should fail.
		//
		if (ERROR_SUCCESS == (lRet = RegDeleteKey(hRoot, NULL)))
		{
			TRACE(TEXT("TEST_FAIL : RegDeleteKey 0x%x\\NULL should have failed\r\n"), hRoot);
			DUMP_LOCN;
			goto ErrorReturn;
		}
		VERIFY_REG_ERROR(RegDeleteKey, lRet, ERROR_INVALID_PARAMETER);


		//
		// CASE: delete a key "" should fail 
		//
		if (ERROR_SUCCESS == (lRet = RegDeleteKey(hRoot, _T(""))))
		{
			TRACE(TEXT("TEST_FAIL : RegDeleteKey 0x%x\\\"\" should have failed\r\n"), hRoot);
			DUMP_LOCN;
			goto ErrorReturn;
		}
		VERIFY_REG_ERROR(RegDeleteKey, lRet, ERROR_INVALID_PARAMETER);


		//
		// CASE: Check case-sensitivity
		//
		StringCchCopy(rgszKeyName, MAX_PATH, _T("fookey"));
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, rgszKeyName);
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		REG_CLOSE_KEY(hKeyNew);
		StringCchLength(rgszKeyName, STRSAFE_MAX_CCH, &szlen);
		CharUpperBuff(rgszKeyName, szlen);
		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, rgszKeyName)))
		{
			TRACE(TEXT("Fail: RegDeletekey is case sensitive\r\n"));
			TRACE(TEXT("TESTFAIL : RegDeleteKey failed to delete 0x%x\\%s\r\n"), hRoot, rgszKeyName);
			REG_FAIL(RegDeleteKey, lRet);
		}


		//
		// CASE: pszKeyName is non-existant and larger than 256 chars - should fail 
		//
		Hlp_GenStringData(rgszLongKeyName, ccLongKeyName, TST_FLAG_ALPHA);
		if (ERROR_SUCCESS == (lRet=RegDeleteKey(hRoot, rgszLongKeyName)))
		{
			TRACE(TEXT("TEST_FAIL : Should have failed for non-existant long key name \r\n"));
			DUMP_LOCN;
			goto ErrorReturn;
		}


		//
		// CASE: pszKeyName like "\Foo"
		//
		StringCchCopy(rgszKeyName, MAX_PATH, _T("\\fookey"));
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, rgszKeyName);
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		REG_CLOSE_KEY(hKeyNew);
		
		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, rgszKeyName)))
		{
			TRACE(TEXT("Fail: RegDeletekey failed for keyname starting with \\ \r\n"));
			TRACE(TEXT("TESTFAIL : RegDeleteKey failed to delete 0x%x\\%s\r\n"), hRoot, rgszKeyName);
			REG_FAIL(RegDeleteKey, lRet);
		}


		//
		// CASE: pszKeyName like "Foo\"
		//
		StringCchCopy(rgszKeyName, MAX_PATH, _T("fookey"));
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, &dwDispo)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, rgszKeyName);
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		REG_CLOSE_KEY(hKeyNew);

		StringCchCopy(rgszKeyName, MAX_PATH, _T("fookey\\"));
		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, rgszKeyName)))
		{
			TRACE(TEXT("RegDeletekey failed for keyname ending with \\ \r\n"));
			TRACE(TEXT("TESTFAIL : RegDeleteKey failed to delete 0x%x\\%s\r\n"), hRoot, rgszKeyName);
			g_dwFailCount++;
			REG_FAIL(RegDeleteKey, lRet);
		}
				
	}

	dwRetVal = TPR_PASS;
	
ErrorReturn :
	return dwRetVal;
}



///////////////////////////////////////////////////////////////////////////////
//
// Test deleting registry with invalid parameters
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_InvalidParams(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
	DWORD dwRetVal=TPR_FAIL;
	HKEY hRoot=0;
	HKEY hKeyNew=0;
	LONG lRet=0;
	int k=0;
	TCHAR rgszKeyName[MAX_PATH]={0};

	// The shell doesn't necessarily want us to execute the test. 
	if(uMsg != TPM_EXECUTE)
	{
		return TPR_NOT_HANDLED;
	}

	for (k=0; k<TST_NUM_ROOTS; k++)
	{
		hRoot=g_l_RegRoots[k];


		//
		// CASE: Non-existant key
		//
		lRet = RegDeleteKey(hRoot, _T("NonExistantKey"));
		VERIFY_REG_ERROR(RegDeleteKey, lRet, ERROR_FILE_NOT_FOUND);


		//
		// CASE: Invalid hRoot
		//
		StringCchCopy(rgszKeyName, MAX_PATH, _T("fookey"));
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, NULL)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, rgszKeyName);
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		REG_CLOSE_KEY(hKeyNew);

		lRet = RegDeleteKey(HKEY_INVALID_TST, rgszKeyName);
		if (ERROR_SUCCESS == lRet)
		{
			TRACE(TEXT("TEST_FAIL : RegDeleteKey should have failed for an invalid hRoot\r\n"));
			DUMP_LOCN;
			goto ErrorReturn;
		}
		lRet = RegDeleteKey(hRoot, rgszKeyName);
		if (lRet == ERROR_ACCESS_DENIED)
			REG_FAIL(RegDeleteKey, lRet);
		
	}

	dwRetVal = TPR_PASS;
	
ErrorReturn :
	REG_CLOSE_KEY(hKeyNew);
	return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// Registry delete, miscelleneous tests
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_MiscFunctionals(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
	DWORD dwRetVal=TPR_FAIL;
	HKEY hRoot=0;
	HKEY hKeyNew=0;
	int k=0;
	TCHAR rgszKeyName[MAX_PATH]={0};
	LONG lRet=0;

	// The shell doesn't necessarily want us to execute the test. 
	if(uMsg != TPM_EXECUTE)
	{
		return TPR_NOT_HANDLED;
	}

	for (k=0; k<TST_NUM_ROOTS; k++)
	{
		hRoot = g_l_RegRoots[k];

		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, _T("Lev1\\Lev2\\Lev3"), 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hKeyNew, NULL)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, rgszKeyName);
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		REG_CLOSE_KEY(hKeyNew);

		if (ERROR_SUCCESS != (lRet=RegOpenKeyEx(hRoot, _T("Lev1"), 0, NULL, &hKeyNew)))
			REG_FAIL(RegOpenKey, lRet);

		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hKeyNew, _T("Lev2"))))
			REG_FAIL(RegDeleteKey, lRet);

		REG_CLOSE_KEY(hKeyNew);
		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, _T("Lev1"))))
			REG_FAIL(RegDeleteKey, lRet);
		
	}

	dwRetVal = TPR_PASS;
	
ErrorReturn :
	return dwRetVal;
}

///////////////////////////////////////////////////////////////////////////////
//
// Test to verify deleting invalid subkey that is open
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_DelInvalidSubKey(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
	DWORD dwRetVal=TPR_FAIL;
	HKEY hRoot=0;
	HKEY hChildKey=0;
	HKEY hChildKey2=0;
	int k=0;
	LONG lRet=0;

	// The shell doesn't necessarily want us to execute the test. 
	if(uMsg != TPM_EXECUTE)
	{
		return TPR_NOT_HANDLED;
	}

	for (k=0; k<TST_NUM_ROOTS; k++)
	{
		hRoot = g_l_RegRoots[k];

		//Create the child
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, _T("Lev1\\Lev2"), 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hChildKey, NULL)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, _T("Lev1\\Lev2"));
			REG_FAIL(RegCreateKeyEx, lRet);
		}
				
		//Delete the parent
		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, _T("Lev1"))))
			REG_FAIL(RegDeleteKey, lRet);

		//Attempt to use the child(handle is open but in valid) - it should fail
		DWORD Value=4;
		if (ERROR_SUCCESS == (lRet=RegSetValueEx(hChildKey, _T("MyValue"), 0, REG_DWORD, (BYTE*)&Value, sizeof(DWORD))))
		{
			TRACE(TEXT("TEST_FAIL : Failed RegSetValueEx %s, dwType=%d succeeded on invalid handle \r\n"), _T("MyValue"), REG_DWORD);
			REG_FAIL(RegSetValueEx, lRet);
		}

		//Recreate the child, but close the second handle
		if (ERROR_SUCCESS != (lRet=RegCreateKeyEx(hRoot, _T("Lev1\\Lev2"), 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), NULL, NULL,  &hChildKey2, NULL)))
		{
			TRACE(TEXT("TEST_FAIL : RegCreateKey failed for key 0x%x\\%s \r\n"), hRoot, _T("Lev1\\Lev2"));
			REG_FAIL(RegCreateKeyEx, lRet);
		}
		REG_CLOSE_KEY(hChildKey2);
		hChildKey2=0;

		//Attempt to delete the child(Handle is still open, but invalid) - it should succeed
		if (ERROR_SUCCESS != (lRet=RegDeleteKey(hRoot, _T("Lev1\\Lev2"))))
		{
			TRACE(TEXT("TEST_FAIL : RegDeleteKey failed for key 0x%x\\%s \r\n"), hRoot, _T("Lev1\\Lev2"));
			REG_FAIL(RegDeleteKey, lRet);
		}

	}

	dwRetVal = TPR_PASS;
	
ErrorReturn :
	if(hChildKey)
		RegCloseKey(hChildKey);
	if(hChildKey2)
		RegCloseKey(hChildKey2);
	return dwRetVal;
}

