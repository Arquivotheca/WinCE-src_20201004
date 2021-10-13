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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include "main.h"
#include "globals.h"
#include <utility.h>

#define QUERY_DATA_SIZE	512 
#define QUERY_NUM_KEYS	50 
#define QUERY_NUM_VALUES	QUERY_NUM_KEYS

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
TESTPROCAPI Tst_RegQueryInfo_Class(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	DWORD dwRetVal=TPR_FAIL;
	HKEY hRoot=0;
	HKEY hKeyNew=0, hKeyTemp=0;
	TCHAR rgszKeyName[MAX_PATH]={0};
	TCHAR rgszValName[MAX_PATH]={0};
	TCHAR rgszClass[MAX_PATH]={0};
	TCHAR rgszClassGet[MAX_PATH]={0};
	DWORD cbClassGet=0;
	LONG lRet=0;
	DWORD dwType=0, cChars;
	DWORD k=0,i=0;
	DWORD dwDispo=0;
	DWORD dwMaxClassLen=0;
	DWORD dwMaxClassLen_Get=0;

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
	if (lRet = L_GetKey_MaxClassLen(hKeyNew, &dwMaxClassLen_Get))
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
		_stprintf(rgszKeyName, _T("tRegApiSubKey_%d"), k);

		memset(rgszClass, 65, sizeof(rgszClass));
		// Max registry length is 255, do not exceed
		cChars=rand()%255;
		if (cChars<2)
			cChars=10;
		if (!Hlp_GenStringData(rgszClass, cChars, TST_FLAG_ALPHA_NUM))
			GENERIC_FAIL(Hlp_GenStringData);
		
		ASSERT(_tcslen(rgszClass) < sizeof(rgszClass)/sizeof(TCHAR));
		
		if (dwMaxClassLen < _tcslen(rgszClass) )
		{
			dwMaxClassLen = _tcslen(rgszClass);
			TRACE(TEXT("New Max Class Length = %d\r\n"), dwMaxClassLen);
		}

		// Create a key with a random class. 
		lRet =RegDeleteKey(hKeyNew, rgszKeyName);
		if (lRet == ERROR_ACCESS_DENIED)
			REG_FAIL(RegDeleteKey, lRet);
		if ((ERROR_SUCCESS != (lRet=RegCreateKeyEx(hKeyNew, rgszKeyName, GetOptionsFromString(g_pShellInfo->szDllCmdLine), rgszClass, 0, NULL, NULL, &hKeyTemp, &dwDispo)) && _tcslen(rgszClass) < 255))	//make sure if fails key is not too long
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
		if (lRet = L_GetKey_MaxClassLen(hKeyNew, &dwMaxClassLen_Get))
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
		memset((PBYTE)rgszClassGet, 65, sizeof(rgszClassGet));
		
		if (lRet = L_GetKeyClass(hKeyTemp, rgszClassGet, &cbClassGet))
			REG_FAIL(RegQueryInfoKey, lRet);

		// Verify the key class. 
		if ((_tcscmp(rgszClassGet, rgszClass)) || 
			(cbClassGet != _tcslen(rgszClass)))
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
		memset((PBYTE)rgszClassGet, 65, sizeof(rgszClassGet));

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
		for (i=(cbClassGet+1)*sizeof(TCHAR); i<sizeof(rgszClassGet); i++)
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
TESTPROCAPI Tst_RegQueryInfo_cSubKey(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
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
		_stprintf(rgszValName, _T("Value_%d"), k);
		if (!Hlp_Write_Random_Value(hKeyNew, NULL, rgszValName, &dwType, pbData, &cbData))
			goto ErrorReturn;
	}
	
	TRACE(TEXT("Set %d values\r\n"), k);

	// Create a bunch of keys are verify it's class. 
	for (k=0; k<QUERY_NUM_KEYS; k++)
	{
		_stprintf(rgszKeyName, _T("tRegApiSubKey_%d"), k);

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
		_stprintf(rgszKeyName, _T("tRegApiSubKey_%d"), k);
		if (ERROR_SUCCESS != (lRet = L_GetKey_cSubKey(hKeyNew, &cSubKey)))
			REG_FAIL(RegQueryInfoKey, lRet);
		
		if (lRet = RegDeleteKey(hKeyNew, rgszKeyName))
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
	if (lRet = RegCloseKey(hKeyNew))
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
TESTPROCAPI Tst_RegQueryInfo_cbMaxSubKey(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
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
		memset(rgszKeyName, 65, sizeof(rgszKeyName));
		cChars = rand()%255; //    max key name is 255 chars.
		if (cChars<2)
			cChars = 10;
		if (!Hlp_GenStringData(rgszKeyName, cChars, TST_FLAG_ALPHA_NUM))
			GENERIC_FAIL(Hlp_GenStringData);
		ASSERT(_tcslen(rgszKeyName)+1 < sizeof(rgszKeyName)/sizeof(TCHAR));
		if (_tcslen(rgszKeyName) > dwMaxSubKey)
		{
			dwMaxSubKey = _tcslen(rgszKeyName);
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
	if (lRet = RegCloseKey(hKeyNew))
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
TESTPROCAPI Tst_RegQueryInfo_cValues(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
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

		// Write a value with random sized data and random sized name length
		memset(rgszValName, 65, sizeof(rgszKeyName));
		cChars = rand()%255;                                   //  Names can be max 255 chars. 
		if (cChars<2) cChars=10;
		
		if (!Hlp_GenStringData(rgszValName, cChars, TST_FLAG_ALPHA_NUM))
			GENERIC_FAIL(Hlp_GenStringData);
		ASSERT(_tcslen(rgszValName)+1 < 255);
		if (_tcslen(rgszValName)+1 > 255)
			TRACE(TEXT(">>>>>>>>> Value name is too big <<<<<<<<<<\r\n"));

		// Track the max val name
		if (_tcslen(rgszValName) > dwMaxValName)
		{
			dwMaxValName = _tcslen(rgszValName);
			TRACE(TEXT("New Max Val Name = %d\r\n"), dwMaxValName);
		}
		
		dwType=0;
		cbData=QUERY_DATA_SIZE;
		// Generate some value data depending on the dwtype
		if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
			goto ErrorReturn;

		if ( (REG_BINARY == dwType) ||
				(REG_NONE == dwType) ||
				(REG_BINARY == dwType) ||
				(REG_LINK == dwType) ||
				(REG_RESOURCE_LIST == dwType) ||
				(REG_RESOURCE_REQUIREMENTS_LIST == dwType) ||
				(REG_FULL_RESOURCE_DESCRIPTOR == dwType))
			cbData = rand() % cbData;
		if (!cbData) cbData+=5;

		// Track the max val data
		if (cbData > dwMaxValData)
		{
			TRACE(TEXT("New Max Val Data =%d\r\n"), cbData);
			dwMaxValData = cbData;
		}
		
		if (lRet = RegSetValueEx(hKeyNew, rgszValName, 0, dwType, pbData, cbData))
		{
			DebugBreak();
			TRACE(TEXT("rgszValName=%s, dwType=%d, cbData=%d, k=%d \r\n"), rgszValName, dwType, cbData, k); 
			REG_FAIL(RegSetValueEx, lRet);
		}


		//
		// CASE: Check that the max val data is correct.
		//
		if (lRet= L_GetKey_cbMaxValData(hKeyNew, &dwMaxValData_Get))
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
		if (lRet= L_GetKey_cbMaxValName(hKeyNew, &dwMaxValName_Get))
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
