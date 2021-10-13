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
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>

#include <winreg.h>
#include "main.h"
#include "globals.h"
#include "thelper.h"
#include "Regtst.h"
#include <utility.h>

///////////////////////////////////////////////////////////////////////////////
//  Function    :   L_CreatePredefinedValues
//  
//  Params      :   hKey
//  
//  Description :   Creates a bunch of predefined Values under the given key
//  returns FALSE if hKey==0. 
//
//  WARNING : This code works with hard coded key names defined in g_rgszValueNames
///////////////////////////////////////////////////////////////////////////////
BOOL L_CreatePredefinedValues(HKEY hKey)
{
	BOOL    fRetVal=0;
	UINT    k=0;
	PBYTE   pbData=NULL;
	DWORD   cbData=0;
	DWORD   ccbData=100;
	DWORD   dwType=0;
	LONG    lRet=0;

	if (0==hKey)
		return FALSE;

	cbData = ccbData;
	pbData=(PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
	CHECK_ALLOC(pbData);

	for(k=0; k<TST_NUM_VALUES; k++)
	{
		dwType=0;
		if (_tcscmp(g_rgszValueNames[k], _T("StringVal")))
			dwType = REG_SZ;
		else if (_tcscmp(g_rgszValueNames[k], _T("DwordVal")))
			dwType = REG_DWORD;
		else if (_tcscmp(g_rgszValueNames[k], _T("BinVal")))
			dwType = REG_BINARY;
		else if (_tcscmp(g_rgszValueNames[k], _T("MultiSz_Val")))
			dwType = REG_MULTI_SZ;
		else if (_tcscmp(g_rgszValueNames[k], _T("BigEndian_Val")))
			dwType = REG_DWORD_BIG_ENDIAN;
		else
			TRACE(TEXT("Unknown type of Key to generate. %s %u\r\n"), _T(__FILE__), __LINE__);
		
		cbData = ccbData;
		if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
			goto ErrorReturn;

		lRet = RegSetValueEx(hKey, g_rgszValueNames[k], 0, dwType, pbData, cbData);
		if (ERROR_SUCCESS != lRet)
			REG_FAIL(RegSetValueEx, lRet);
		
		TRACE(TEXT("tregrplc : Created Value %s \r\n"), g_rgszValueNames[k]);
	}
	
	fRetVal= TRUE;
ErrorReturn :

	FREE(pbData);
	return fRetVal ;
}

///////////////////////////////////////////////////////////////////////////////
//  Function    :   L_CreatePredefinedKeys
//
//  Params      :   None
//
//  Description :   Creates a number of predefined keys in all the roots. 
//  In each key, creates a number of predfined values with random data.
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_CreatePredefinedKeys(void)
{
	BOOL    fRetVal=FALSE;
	UINT    i,j;
	HKEY    hRootKey=0;
	HKEY    hKey=0;
	LONG    lRet=0;
	DWORD   dwDisposition=0;
	TCHAR   pszTemp[MAX_PATH]={0};

	for (i=0; i<TST_NUM_REG_ROOTS; i++)
	{
		hRootKey = g_Reg_Roots[i];
		TRACE(TEXT("Creating Keys under Root %s\r\n"), Hlp_HKeyToText(hRootKey, pszTemp, MAX_PATH)) ;
		for (j=0; j<TST_NUM_KEYS; j++)
		{
			lRet = RegCreateKeyEx(  hRootKey, g_rgszKeyNames[j], 0, NULL, GetOptionsFromString(g_pShellInfo->szDllCmdLine), 0,NULL, &hKey, &dwDisposition);
			if (ERROR_SUCCESS != lRet)
				REG_FAIL(RegCreateKeyEx, lRet);

			if (!L_CreatePredefinedValues(hKey))
				goto ErrorReturn;

			REG_CLOSE_KEY(hKey);
		}
	}
	
	fRetVal=TRUE;

ErrorReturn :
	REG_CLOSE_KEY(hKey);
	return fRetVal;

}


///////////////////////////////////////////////////////////////////////////////
//
//  Function    :   L_DeletePredefinedKeys
//
//  Params      :   None
//
//  Description :   Delete the predefined keys
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_DeletePredefinedKeys(void)
{
	BOOL    fRetVal=FALSE;
	UINT    i,j;
	HKEY    hRootKey=0;
	LONG    lRet=0;

	for (i=0; i<TST_NUM_REG_ROOTS; i++)
	{
		//hRootKey = HKEY_LOCAL_MACHINE;
		hRootKey = g_Reg_Roots[i];
		for (j=0; j<TST_NUM_KEYS; j++)
		{
			lRet = RegDeleteKey(hRootKey, g_rgszKeyNames[j]);
			if (ERROR_SUCCESS != lRet)
				REG_FAIL(RegDeleteKey, lRet);
			TRACE(TEXT("tregrplc : Deleted key %s\r\n"), g_rgszKeyNames[j]);
		}
	}
	
	fRetVal=TRUE;

ErrorReturn :
	return fRetVal;

}


///////////////////////////////////////////////////////////////////////////////
//
//  Function    :   L_VerifyPredefinedValues
//
//  Params      :   None
//
//  Description :   
//
///////////////////////////////////////////////////////////////////////////////
BOOL    L_VerifyPredefinedValues(HKEY hKey) 
{
	BOOL    fRetVal=FALSE ;
	DWORD   dwType=0;
	PBYTE   pbData=NULL;
	DWORD   cbData=0;   
	DWORD   ccbData=100;
	UINT    k=0;
	LONG    lRet=0;

	if (0==hKey)
		return FALSE;
	
	cbData = ccbData;
	pbData=(PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
	CHECK_ALLOC(pbData);


	for(k=0; k<TST_NUM_VALUES; k++)
	{
		cbData=ccbData;
		lRet = RegQueryValueEx(hKey, g_rgszValueNames[k], NULL, &dwType, pbData, &cbData);
		if (ERROR_SUCCESS != lRet)
			REG_FAIL(RegQueryValueEx, lRet);
		
		if (_tcscmp(g_rgszValueNames[k], _T("StringVal")))
		{
			if (REG_SZ != dwType)
				goto ErrorReturn;
		}
		else if (_tcscmp(g_rgszValueNames[k], _T("DwordVal")))
		{
			if (dwType != REG_DWORD)
				goto ErrorReturn;
		}
		else if (_tcscmp(g_rgszValueNames[k], _T("BinVal")))
		{
			if (dwType != REG_BINARY)
				goto ErrorReturn;
		}
		else if (_tcscmp(g_rgszValueNames[k], _T("MultiSz_Val")))
		{
			if (dwType != REG_MULTI_SZ)
				goto ErrorReturn;
		}
		else if (_tcscmp(g_rgszValueNames[k], _T("BigEndian_Val")))
		{
			if (dwType != REG_DWORD_BIG_ENDIAN)
				goto ErrorReturn;
		}
		else
			TRACE(TEXT("tregrplc : Unknown type of Key to generate. %s %u\r\n"), _T(__FILE__), __LINE__);

		TRACE(TEXT("tregrplc : Verified key %s - OK\r\n"), g_rgszValueNames[k]);
	}
	fRetVal=TRUE;

ErrorReturn :
	if (FALSE == fRetVal)
		TRACE(TEXT("Failed to verify value %s\r\n"), g_rgszValueNames[k]);
	
	FREE(pbData);
	return fRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
//  Function    :   L_VerifyPredefinedKeys
//
//  Params      :   None
//
//  Description :   Verifies that the predefined keys created by 
//                  L_CreatePredefinedKeys exist and have the values
//                  that we expect.
//
///////////////////////////////////////////////////////////////////////////////
BOOL    L_VerifyPredefinedKeys(void)
{
	BOOL    fRetVal=FALSE ;
	HKEY    hRootKey=0;
	HKEY    hKey=0;
	LONG    lRet=0;
	UINT    i,j;
	TCHAR   pszTemp[MAX_PATH]={0};

	//  Verify to make sure that these keys exist. 
	//  Create a bunch of keys
	for (i=0; i<TST_NUM_REG_ROOTS; i++)
	{
		hRootKey = g_Reg_Roots[i];
		for (j=0; j<TST_NUM_KEYS; j++)
		{
			lRet = RegOpenKeyEx(hRootKey, g_rgszKeyNames[j], 0, 0, &hKey);
			if (ERROR_SUCCESS != lRet)
				REG_FAIL(RegOpenKeyEx, lRet);

			TRACE(TEXT("Verifying Keys under Root %s\r\n"), Hlp_HKeyToText(hRootKey, pszTemp, MAX_PATH)) ;

			if (!L_VerifyPredefinedValues(hKey))
				goto ErrorReturn;

			REG_CLOSE_KEY(hKey);
		}
	}

	fRetVal=TRUE;
ErrorReturn :
	REG_CLOSE_KEY(hKey);
	return fRetVal;

}


BOOL L_WriteRandomVal(HKEY hKey)
{
	BOOL    fRetVal=FALSE;
	DWORD   dwType=0;
	PBYTE   pbData;
	DWORD   cbData=100;
	LONG    lRet=0;

	pbData = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
	CHECK_ALLOC(pbData);

	if (!Hlp_GenRandomValData(&dwType, pbData, &cbData))
		goto ErrorReturn;

	if (ERROR_SUCCESS != (lRet = RegSetValueEx(hKey, _T("FooValue"), 0, dwType, pbData, cbData)))
		REG_FAIL(RegSetValueEx, lRet);
   
	fRetVal = TRUE;
ErrorReturn :
	FREE(pbData);
	return fRetVal;
}

