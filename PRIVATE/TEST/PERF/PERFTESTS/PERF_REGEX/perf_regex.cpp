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
#include "perf_regEx.h"
#include "reghlpr.h"

// For CPU and Memory Timer
DWORD g_dwPoolIntervalMs;
DWORD g_dwCPUCalibrationMs;


///////////////////////////////////////////////////////////////////////////////
//  
// Function: L_Measure_CreateKey
//
// Purpose: 
//			Create a bunch of keys as specified and measure the performance.
//
// Params:
//			dwFlags:	PERF_BIT_LONG_NAME
//					PERF_BIT_SHORT_NAME
///////////////////////////////////////////////////////////////////////////////
BOOL L_Measure_CreateKey(HKEY hRoot, LPCTSTR pszDesc, DWORD dwFlags, DWORD dwNumKeys)
{
	BOOL fRetVal=FALSE;
	LONG lRet=0;
	HKEY hKeyParent=0;
	HKEY hKeyChild=0;
	DWORD dwDispo=0;
	DWORD i=0, k=0;
	TCHAR rgszKeyName[256]={0};
	TCHAR rgszTemp[MAX_PATH]={0};
	TCHAR rgszDesc[1024]={0};
	DWORD dwKeyFound, dwSubKeys, dwMaxSubKeyLen, dwMaxClassLen, dwNumVals, dwMaxValNameLen, dwMaxValLen;
	HKEY *phKey=NULL;
	DWORD dwOpenKeys=PERF_OPEN_KEY_LIST_LEN;

	ASSERT((HKEY_LOCAL_MACHINE == hRoot) || (HKEY_CURRENT_USER == hRoot));
	ASSERT(dwNumKeys && dwFlags);

	Hlp_HKeyToTLA(hRoot, rgszTemp, MAX_PATH);
	_stprintf(rgszDesc, _T("%s - %s"), pszDesc, rgszTemp);

	// dwFlags can either be PERF_BIT_ONE_KEY, PERF_BIT_MULTIPLE_KEY or PERF_BIT_SIMILAR_NAMES
	if (dwFlags & PERF_BIT_ONE_KEY)
		_stprintf(rgszDesc, _T("%s - singleKey"), rgszDesc);

	else if (dwFlags & PERF_BIT_MULTIPLE_KEYS)
		_stprintf(rgszDesc, _T("%s - MultipleKeys"), rgszDesc);

	else if (dwFlags & PERF_BIT_SIMILAR_NAMES)
		_stprintf(rgszDesc, _T("%s - SimilarNames"), rgszDesc);
	
	else if (dwFlags & PERF_BIT_OPENLIST)
	{
		// Open HKLM\Init say 1000 times. 
		_stprintf(rgszDesc, _T("%s - Large OpenList"), rgszDesc);
		phKey = (HKEY*)LocalAlloc(LMEM_ZEROINIT, PERF_OPEN_KEY_LIST_LEN*sizeof(HKEY));
		CHECK_ALLOC(phKey);

		for (k=0; k<PERF_OPEN_KEY_LIST_LEN; k++)
		{
			if (lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("init"), 0, NULL, &(phKey[k])))
				REG_FAIL(RegOpenKeyEx, lRet);
		}
	}
	
	// dwFlags can either have PERF_BIT_LONG_NAME or PERF_BIT_SHORT_NAME set. 
	if (dwFlags & PERF_BIT_LONG_NAME)
		_stprintf(rgszDesc, _T("%s - LongNames - %d Keys"), rgszDesc, dwNumKeys);
	
	else if (dwFlags & PERF_BIT_SHORT_NAME)
		_stprintf(rgszDesc, _T("%s - ShortNames - %d Keys"), rgszDesc, dwNumKeys);
	
	// Add the AVG macro for parsing
	_stprintf(rgszDesc, _T("%s,time=%%AVG%%"), rgszDesc);

	// Only register markers that we actually use at a time	
	if (!Perf_RegisterMark(MARK_KEY_CREATE, _T("Test=RegCreateKey - %s"), rgszDesc)) 
		GENERIC_FAIL(Perf_RegisterMark);
	
	if (!Perf_RegisterMark(MARK_KEY_OPEN, _T("Test=RegOpenKey - %s"), rgszDesc)) 
		GENERIC_FAIL(Perf_RegisterMark);
	
	if (!Perf_RegisterMark(MARK_KEY_CLOSE, _T("Test=RegCloseKey - %s"), rgszDesc)) 
		GENERIC_FAIL(Perf_RegisterMark);
	
	if (!Perf_RegisterMark(MARK_KEY_DELETE, _T("Test=RegDeleteKey - %s"), rgszDesc)) 
		GENERIC_FAIL(Perf_RegisterMark);	

	if (dwFlags & PERF_BIT_MULTIPLE_KEYS || dwFlags & PERF_BIT_SIMILAR_NAMES)
	{
		if (!Perf_RegisterMark(MARK_KEY_QUERYINFO, _T("Test=RegQueryInfoKey - %s"), rgszDesc)) 
			GENERIC_FAIL(Perf_RegisterMark);
	}

	// Create the parent Key
	RegDeleteKey(hRoot, PERF_KEY);
	if (lRet=RegCreateKeyEx(hRoot, PERF_KEY, 0, NULL, 0, 0, NULL, &hKeyParent, &dwDispo))
		REG_FAIL(RegCreateKeyEx, lRet);

	// Start monitoring CPU and memory
	Perf_StartSysMonitor(PERF_APP_NAME, SYS_MON_CPU | SYS_MON_MEM | SYS_MON_LOG, 
							g_dwPoolIntervalMs, g_dwCPUCalibrationMs);
	
	//  ------------------
	//  Start creating all the keys
	//  ------------------
	for (i=0; i<dwNumKeys; i++)
	{			
		if ((dwFlags & PERF_BIT_ONE_KEY) ||
			(dwFlags & PERF_BIT_OPENLIST))
		{
			//  --------------
			//  Measure Createkey
			//  --------------
			if (dwFlags & PERF_BIT_LONG_NAME)
				Hlp_GenStringData(rgszKeyName, PERF_LONG_NAME_LEN, TST_FLAG_ALPHA_NUM);

			else if (dwFlags & PERF_BIT_SHORT_NAME)
				Hlp_GenStringData(rgszKeyName, PERF_SHORT_NAME_LEN, TST_FLAG_ALPHA_NUM);

			Perf_MarkBegin(MARK_KEY_CREATE);
			lRet=RegCreateKeyEx(hKeyParent, rgszKeyName, 0, NULL, 0, 0, NULL, &hKeyChild, &dwDispo);
			Perf_MarkEnd(MARK_KEY_CREATE);

			if ((ERROR_SUCCESS != lRet) || (REG_CREATED_NEW_KEY != dwDispo))
				REG_FAIL(RegCreateKeyEx, lRet);
			
			REG_CLOSE_KEY(hKeyChild);

			//  ----------------
			//  Measure OpenKey
			//  ----------------
			Perf_MarkBegin(MARK_KEY_OPEN);
			lRet = RegOpenKeyEx(hKeyParent, rgszKeyName, 0, NULL, &hKeyChild);
			Perf_MarkEnd(MARK_KEY_OPEN);
			
			if (ERROR_SUCCESS != lRet) 
				REG_FAIL(RegOpenKeyEx, lRet);

			//  ----------------
			//  Measure CloseKey
			//  ----------------
			Perf_MarkBegin(MARK_KEY_CLOSE);
			REG_CLOSE_KEY(hKeyChild);
			Perf_MarkEnd(MARK_KEY_CLOSE);

			//  --------------
			//  Measure Deletekey 
			//  --------------
			Perf_MarkBegin(MARK_KEY_DELETE);
			lRet=RegDeleteKey(hKeyParent, rgszKeyName);
			Perf_MarkEnd(MARK_KEY_DELETE);

			if (ERROR_SUCCESS != lRet)
				REG_FAIL(RegDeleteKey, lRet);
		}

		else if (dwFlags & PERF_BIT_MULTIPLE_KEYS)
		{
			// Create a set keys
			for (k=0; k<PERF_NUM_KEYS_IN_SET; k++)
			{
				if (dwFlags & PERF_BIT_LONG_NAME)
					Hlp_GenStringData(rgszKeyName, PERF_LONG_NAME_LEN, TST_FLAG_ALPHA_NUM);

				else if (dwFlags & PERF_BIT_SHORT_NAME)
					Hlp_GenStringData(rgszKeyName, PERF_SHORT_NAME_LEN, TST_FLAG_ALPHA_NUM);

				//  --------------
				//  Measure CreateKey
				//  --------------
				Perf_MarkBegin(MARK_KEY_CREATE);
				lRet=RegCreateKeyEx(hKeyParent, rgszKeyName, 0, NULL, 0, 0, NULL, &hKeyChild, &dwDispo);
				Perf_MarkEnd(MARK_KEY_CREATE);

				if ((ERROR_SUCCESS != lRet) || (REG_CREATED_NEW_KEY != dwDispo))
					REG_FAIL(RegCreateKeyEx, lRet);

				REG_CLOSE_KEY(hKeyChild);

				//  ----------------
				//  Measure OpenKey
				//  ----------------
				Perf_MarkBegin(MARK_KEY_OPEN);
				lRet = RegOpenKeyEx(hKeyParent, rgszKeyName, 0, NULL, &hKeyChild);
				Perf_MarkEnd(MARK_KEY_OPEN);
				
				if (ERROR_SUCCESS != lRet) 
					REG_FAIL(RegOpenKeyEx, lRet);
				
				//  ----------------
				//  Measure CloseKey
				//  ----------------
				Perf_MarkBegin(MARK_KEY_CLOSE);
				REG_CLOSE_KEY(hKeyChild);
				Perf_MarkEnd(MARK_KEY_CLOSE);

				//  --------------
				//  Measure RegQueryInfo
				//  --------------
				Perf_MarkBegin(MARK_KEY_QUERYINFO);
				lRet = RegQueryInfoKey(hKeyParent, NULL, NULL, NULL, 
										&dwSubKeys, &dwMaxSubKeyLen, &dwMaxClassLen, &dwNumVals, &dwMaxValNameLen, 
										&dwMaxValLen, NULL, NULL);
				Perf_MarkEnd(MARK_KEY_QUERYINFO);
				
				if (ERROR_SUCCESS != lRet)
					REG_FAIL(RegQueryInfoKey, lRet);
				
			}

			// Now delete the set of keys. 
			dwKeyFound=0;
			dwDispo = dwDispo=sizeof(rgszKeyName)/sizeof(TCHAR);
			while (ERROR_SUCCESS == RegEnumKeyEx(hKeyParent, 0, rgszKeyName, &dwDispo, 
												NULL, NULL, NULL, NULL))
			{
				dwKeyFound++;
				//  --------------
				//  Measure DeleteKey
				//  --------------
				Perf_MarkBegin(MARK_KEY_DELETE);
				lRet=RegDeleteKey(hKeyParent, rgszKeyName);
				Perf_MarkEnd(MARK_KEY_DELETE);

				if (ERROR_SUCCESS != lRet)
					REG_FAIL(RegDeleteKey, lRet);
				dwDispo=dwDispo=sizeof(rgszKeyName)/sizeof(TCHAR);;
			}
			ASSERT(PERF_NUM_KEYS_IN_SET==dwKeyFound);
		}

		else if (dwFlags & PERF_BIT_SIMILAR_NAMES)
		{
			// Create 100 keys
			for (k=0; k<PERF_NUM_KEYS_IN_SET; k++)
			{
				_stprintf(rgszKeyName, _T("Perf_Registry_key_number_%d"), k);
				//  --------------
				//  Measure CreateKey
				//  --------------
				Perf_MarkBegin(MARK_KEY_CREATE);
				lRet=RegCreateKeyEx(hKeyParent, rgszKeyName, 0, NULL, 0, 0, NULL, &hKeyChild, &dwDispo);
				Perf_MarkEnd(MARK_KEY_CREATE);

				if ((ERROR_SUCCESS != lRet) || (REG_CREATED_NEW_KEY != dwDispo))
					REG_FAIL(RegCreateKeyEx, lRet);
				REG_CLOSE_KEY(hKeyChild);

				//  ----------------
				//  Measure OpenKey
				//  ----------------
				Perf_MarkBegin(MARK_KEY_OPEN);
				lRet = RegOpenKeyEx(hKeyParent, rgszKeyName, 0, NULL, &hKeyChild);
				Perf_MarkEnd(MARK_KEY_OPEN);
				
				if (ERROR_SUCCESS != lRet) 
					REG_FAIL(RegOpenKeyEx, lRet);
				
				//  ----------------
				//  Measure CloseKey
				//  ----------------
				Perf_MarkBegin(MARK_KEY_CLOSE);
				REG_CLOSE_KEY(hKeyChild);
				Perf_MarkEnd(MARK_KEY_CLOSE);

				//  --------------
				//  Measure RegQueryInfo
				//  --------------
				Perf_MarkBegin(MARK_KEY_QUERYINFO);
				lRet = RegQueryInfoKey(hKeyParent, NULL, NULL, NULL, 
										&dwSubKeys, &dwMaxSubKeyLen, &dwMaxClassLen, &dwNumVals, &dwMaxValNameLen, 
										&dwMaxValLen, NULL, NULL);
				Perf_MarkEnd(MARK_KEY_QUERYINFO);
				
				if (ERROR_SUCCESS != lRet)
					REG_FAIL(RegQueryInfoKey, lRet);
				
			}

			// Now delete all the similar keys that were created. 
			dwKeyFound=0;
			dwDispo=sizeof(rgszKeyName)/sizeof(TCHAR);
			while (ERROR_SUCCESS == RegEnumKeyEx(hKeyParent, 0, rgszKeyName, &dwDispo, 
												NULL, NULL, NULL, NULL))
			{
				dwKeyFound++;
				//  --------------
				//  Measure DeleteKey
				//  --------------
				Perf_MarkBegin(MARK_KEY_DELETE);
				lRet=RegDeleteKey(hKeyParent, rgszKeyName);
				Perf_MarkEnd(MARK_KEY_DELETE);

				if (ERROR_SUCCESS != lRet)
					REG_FAIL(RegDeleteKey, lRet);
				dwDispo=sizeof(rgszKeyName)/sizeof(TCHAR);
			}
			ASSERT(PERF_NUM_KEYS_IN_SET==dwKeyFound);
			
		}
	}

	// Close the open key list before leaving. 
	if (dwFlags & PERF_BIT_OPENLIST)
	{
		for (k=0; k<PERF_OPEN_KEY_LIST_LEN; k++)
		{
			lRet = RegCloseKey(phKey[k]);
			  
			if (ERROR_SUCCESS != lRet)
				REG_FAIL(RegCloseKey, lRet);
			  
			phKey[k]=(HKEY)0;
		}
	}

	fRetVal=TRUE;

ErrorReturn :    

	Perf_StopSysMonitor();	
	REG_CLOSE_KEY(hKeyChild);
	REG_CLOSE_KEY(hKeyParent);
	FREE(phKey);
	return fRetVal;
}
	

///////////////////////////////////////////////////////////////////////////////
//  
// Function: L_MeasureCreateKey_byRoot
//
// Purpose: 
//			Do all the scenarios under a certain root. 
//			This function doesn't measure any performance. It calls the worker
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_MeasureCreateKey_byRoot(HKEY hRoot, LPCTSTR pszDesc)
{
	BOOL fRetVal=FALSE;
	TCHAR rgszDesc[1024]={0};
	DWORD dwNumKeys=0;
	DWORD dwFlags=0;
	
	//  --------------------------------------------------
	//  SCENARIO 1:
	//  Create - Delete 1 short key many times.
	//  --------------------------------------------------
	TRACE(TEXT("Create - Delete 1 short key many times.... \r\n"));
	dwFlags = PERF_BIT_ONE_KEY | PERF_BIT_SHORT_NAME;
	if (!L_Measure_CreateKey(hRoot, pszDesc, dwFlags, PERF_MANY_KEYS))
		goto ErrorReturn;

	//  --------------------------------------------------
	//  SCENARIO 2:
	//  Create and delete a set of short keys many times
	//  --------------------------------------------------
	TRACE(TEXT("Create and delete a set of %d short keys many times\r\n"), PERF_NUM_KEYS_IN_SET);
	dwFlags = PERF_BIT_MULTIPLE_KEYS | PERF_BIT_SHORT_NAME;
	if (!L_Measure_CreateKey(hRoot, pszDesc, dwFlags, PERF_MANY_KEYS))
		goto ErrorReturn;

	//  --------------------------------------------------
	//  SCENARIO 3:
	//  Create and delete a set of similar key names many times. 
	//  --------------------------------------------------
	TRACE(TEXT("Create and delete a set of %d similar key names many times....\r\n"), PERF_NUM_KEYS_IN_SET);
	dwFlags = PERF_BIT_SIMILAR_NAMES;
	if (!L_Measure_CreateKey(hRoot, pszDesc, dwFlags, PERF_MANY_KEYS))
		goto ErrorReturn;

	//  --------------------------------------------------
	//  SCENARIO 4: 
	//  Create and delete a set of keys with a large number
	//  of keys already open in the system. 
	//  --------------------------------------------------
	TRACE(TEXT("Create and delete a set of keys with a long open key list. ..\r\n"));
	dwFlags = PERF_BIT_OPENLIST | PERF_BIT_SHORT_NAME;
	if (!L_Measure_CreateKey(hRoot, pszDesc, dwFlags, PERF_MANY_KEYS))
		goto ErrorReturn;

	//  --------------------------------------------------
	//  SCENARIO 5:
	//  Create - Delete 1 long key many times.
	//  --------------------------------------------------
	TRACE(TEXT("Create - Delete 1 long key many times....\r\n"));
	dwFlags = PERF_BIT_ONE_KEY | PERF_BIT_LONG_NAME;
	if (!L_Measure_CreateKey(hRoot, pszDesc, dwFlags, PERF_MANY_KEYS))
		goto ErrorReturn;

	//  --------------------------------------------------
	//  SCENARIO 6:
	//  Create and delete a sets of long keys many times
	//  --------------------------------------------------
	TRACE(TEXT("Create and delete a sets of %d long keys many times...\r\n"), PERF_NUM_KEYS_IN_SET);
	dwFlags = PERF_BIT_MULTIPLE_KEYS | PERF_BIT_LONG_NAME;
	if (!L_Measure_CreateKey(hRoot, pszDesc, dwFlags, PERF_MANY_KEYS))
		goto ErrorReturn;

	//  --------------------------------------------------
	//  SCENARIO 7: 
	//  Create keys that are paths.Create and delete a set of similar key names many times.
	//  --------------------------------------------------
	TRACE(TEXT("Create keys that are paths.Create and delete a set of similar key names many times....\r\n"));
	dwFlags = PERF_BIT_OPENLIST | PERF_BIT_LONG_NAME;
	if (!L_Measure_CreateKey(hRoot, pszDesc, dwFlags, PERF_MANY_KEYS))
		goto ErrorReturn;

	fRetVal=TRUE;

ErrorReturn:
	return fRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//  
// Function: Tst_CreateKey
//
// Purpose: Call the perf worker per ROOT. 
//			Measure the performance under HKEY_LOCAL_MACHINE 
//			and then measure it under HKEY_CURRENT_USER
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_CreateKey(UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE )
{
	UNREFERENCED_PARAMETER(lpFTE);
	DWORD dwRetVal=TPR_FAIL;

	COMMON_TUX_HEADER;

	TRACE(TEXT("Measuring performace on HKLM now.... \r\n"));

	if (!L_MeasureCreateKey_byRoot(HKEY_LOCAL_MACHINE, lpFTE->lpDescription))
		goto ErrorReturn;

	TRACE(TEXT("Measuring performace on HKCU now.... \r\n"));

	if (!L_MeasureCreateKey_byRoot(HKEY_CURRENT_USER, lpFTE->lpDescription))
		goto ErrorReturn;

	dwRetVal=TPR_PASS;

ErrorReturn:
	return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//  
// Function: L_MeasureSetValue_byNameLen
//
// Purpose: Measure how much time it takes to create short and long value names. 
//			The function will create n REG_DWORDS under the same parent key. 
//
// Params:
//		hRoot		:	RegRoot under which the keys will be created. 
//		dwFlags		:	PERF_BIT_LONG_NAMES or PERF_BIT_SHORT_NAMES
//		dwNumVals	:	The number of values
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_MeasureSetValue_byNameLen(HKEY hRoot, LPCTSTR pszDesc, DWORD dwFlags, DWORD dwNumValues)
{
	BOOL fRetVal=FALSE;
	DWORD i=0,k=0;
	TCHAR rgszValName[256]={0};
	HKEY hKeyParent=0;
	DWORD dwDispo=0;
	TCHAR rgszDesc[1024]={0};
	TCHAR rgszTemp[MAX_PATH]={0};
	LONG lRet=0;
	BYTE rgbData[100]={0};
	DWORD cbData=sizeof(rgbData)/sizeof(BYTE);
	DWORD dwType=0, ccValName, dwValFound;
	
	ASSERT(hRoot && dwFlags && dwNumValues);
	ASSERT((HKEY_LOCAL_MACHINE == hRoot) || (HKEY_CURRENT_USER == hRoot));

	// Register the markers. 
	Hlp_HKeyToTLA(hRoot, rgszTemp, MAX_PATH);
	_stprintf(rgszDesc, _T("%s - %s"), pszDesc, rgszTemp);

	if (PERF_BIT_ONE_KEY & dwFlags)
		_stprintf(rgszDesc, _T("%s - One Key"), rgszDesc);
	else if (PERF_BIT_MULTIPLE_KEYS & dwFlags)
		_stprintf(rgszDesc, _T("%s - Multiple Keys"), rgszDesc);
	else if (PERF_BIT_SIMILAR_NAMES & dwFlags)
		_stprintf(rgszDesc, _T("%s - similar names - %d vals"), rgszDesc, dwNumValues);
		
	
	if (PERF_BIT_LONG_NAME & dwFlags) 
		_stprintf(rgszDesc, _T("%s - LongNames - %d vals"), rgszDesc, dwNumValues);
	else if (PERF_BIT_SHORT_NAME & dwFlags)
		_stprintf(rgszDesc, _T("%s - ShortNames - %d vals"), rgszDesc, dwNumValues);

	// Add the AVG macro for parsing
	_stprintf(rgszDesc, _T("%s,time=%%AVG%%"), rgszDesc);

	// Only register markers that we actually use at a time	
	if (!Perf_RegisterMark(MARK_VALUE_SET, _T("Test=RegSetValueEx - %s"), rgszDesc)) 		
		GENERIC_FAIL(Perf_RegisterMark);

	if (!Perf_RegisterMark(MARK_VALUE_QUERY, _T("Test=RegQueryValueEx - %s"), rgszDesc)) 		
		GENERIC_FAIL(Perf_RegisterMark);
		
	if (!Perf_RegisterMark(MARK_VALUE_DELETE, _T("Test=RegDeleteValue - %s"), rgszDesc)) 
		GENERIC_FAIL(Perf_RegisterMark);
	
	// Create the parent Key under which all the values will be made.
	RegDeleteKey(hRoot, PERF_KEY);    
	if (lRet=RegCreateKeyEx(hRoot, PERF_KEY, 0, NULL, 0, 0, NULL, &hKeyParent, &dwDispo))
		REG_FAIL(RegCreateKeyEx, lRet);
	ASSERT(REG_CREATED_NEW_KEY == dwDispo);
	if (REG_CREATED_NEW_KEY != dwDispo)
		REG_FAIL(RegCreateKeyEx, lRet);

	// Start monitoring CPU and memory
	Perf_StartSysMonitor(PERF_APP_NAME, SYS_MON_CPU | SYS_MON_MEM | SYS_MON_LOG, 
							g_dwPoolIntervalMs, g_dwCPUCalibrationMs);

	for (i=0; i<dwNumValues; i++)
	{
		if (PERF_BIT_ONE_KEY & dwFlags)
		{
			if (dwFlags & PERF_BIT_LONG_NAME)
				Hlp_GenStringData(rgszValName, PERF_LONG_NAME_LEN, TST_FLAG_ALPHA_NUM);

			else if (dwFlags & PERF_BIT_SHORT_NAME)
				Hlp_GenStringData(rgszValName, PERF_SHORT_NAME_LEN, TST_FLAG_ALPHA_NUM);

			//  ----------------------
			//  Measure RegSetValue
			//  ----------------------
			Perf_MarkBegin(MARK_VALUE_SET);
			lRet = RegSetValueEx(hKeyParent, rgszValName, 0, REG_DWORD, (const BYTE*)&i, sizeof(DWORD));
			Perf_MarkEnd(MARK_VALUE_SET);
			
			if (ERROR_SUCCESS != lRet)
				REG_FAIL(RegSetValueEx, lRet);

			//  ----------------------
			//  Measure RegQueryValue
			//  ----------------------
			dwType=0;
			cbData = sizeof(rgbData)/sizeof(BYTE);
			Perf_MarkBegin(MARK_VALUE_QUERY);
			lRet = RegQueryValueEx(hKeyParent, rgszValName, 0, &dwType, rgbData, &cbData);
			Perf_MarkEnd(MARK_VALUE_QUERY);
			
			if (ERROR_SUCCESS != lRet)
				REG_FAIL(RegQueryValueEx, lRet);
			ASSERT(REG_DWORD == dwType);

			//  ----------------------
			//  Measure RegDeleteValue
			//  ----------------------
			Perf_MarkBegin(MARK_VALUE_DELETE);
			lRet = RegDeleteValue(hKeyParent, rgszValName);
			Perf_MarkEnd(MARK_VALUE_DELETE);
			
			if (ERROR_SUCCESS != lRet)
				REG_FAIL(RegQueryValueEx, lRet);
			
		}
		else if (PERF_BIT_MULTIPLE_KEYS & dwFlags)
		{
			for (k=0; k<PERF_NUM_KEYS_IN_SET; k++)
			{
				if (dwFlags & PERF_BIT_LONG_NAME)
					Hlp_GenStringData(rgszValName, PERF_LONG_NAME_LEN, TST_FLAG_ALPHA_NUM);

				else if (dwFlags & PERF_BIT_SHORT_NAME)
					Hlp_GenStringData(rgszValName, PERF_SHORT_NAME_LEN, TST_FLAG_ALPHA_NUM);

				//  ----------------------
				//  Measure RegSetValue
				//  ----------------------
				Perf_MarkBegin(MARK_VALUE_SET);
				lRet = RegSetValueEx(hKeyParent, rgszValName, 0, REG_DWORD, (const BYTE*)&k, sizeof(DWORD));
				Perf_MarkEnd(MARK_VALUE_SET);
				
				if (ERROR_SUCCESS != lRet)
					REG_FAIL(RegSetValueEx, lRet);
			}

			// Enumerate the values
			dwValFound=0;
			ccValName = sizeof(rgszValName)/sizeof(TCHAR);
			while (ERROR_SUCCESS == RegEnumValue(hKeyParent, 
									dwValFound, rgszValName, &ccValName, NULL, NULL, NULL, NULL))
			{
				dwValFound++;
				ccValName = sizeof(rgszValName)/sizeof(TCHAR);
				
				dwType=0;
				cbData = sizeof(rgbData)/sizeof(BYTE);
				
				//  --------------------
				//  Measure RegQueryValueEx
				//  --------------------
				Perf_MarkBegin(MARK_VALUE_QUERY);
				lRet = RegQueryValueEx(hKeyParent, rgszValName, 0, &dwType, rgbData, &cbData);
				Perf_MarkEnd(MARK_VALUE_QUERY);
				
				if (ERROR_SUCCESS != lRet)
					REG_FAIL(RegQueryValueEx, lRet);
				ASSERT(REG_DWORD == dwType);    
			}
			ASSERT(PERF_NUM_KEYS_IN_SET== dwValFound);

			dwValFound=0;
			ccValName = sizeof(rgszValName)/sizeof(TCHAR);
			while (ERROR_SUCCESS == RegEnumValue(hKeyParent, 0, rgszValName, &ccValName, NULL, NULL, NULL, NULL))
			{
				dwValFound++;
				ccValName = sizeof(rgszValName)/sizeof(TCHAR);

				//  ----------------------
				//  Measure RegDeleteValue
				//  ----------------------
				Perf_MarkBegin(MARK_VALUE_DELETE);
				lRet = RegDeleteValue(hKeyParent, rgszValName);
				Perf_MarkEnd(MARK_VALUE_DELETE);
				
				if (ERROR_SUCCESS != lRet)
					REG_FAIL(RegQueryValueEx, lRet);
			}
			ASSERT(PERF_NUM_KEYS_IN_SET== dwValFound);

			// Read the values.
		}
		else if (PERF_BIT_SIMILAR_NAMES & dwFlags)
		{
			for (k=0; k<PERF_NUM_KEYS_IN_SET; k++)
			{
				_stprintf(rgszValName, _T("This_Is_A_Common_Val_%d"), k);

				//  ----------------------
				//  Measure RegSetValue
				//  ----------------------
				Perf_MarkBegin(MARK_VALUE_SET);
				lRet = RegSetValueEx(hKeyParent, rgszValName, 0, REG_DWORD, (const BYTE*)&k, sizeof(DWORD));
				Perf_MarkEnd(MARK_VALUE_SET);
				
				if (ERROR_SUCCESS != lRet)
					REG_FAIL(RegSetValueEx, lRet);

			}

			// Enumerate the values
			dwValFound=0;
			ccValName = sizeof(rgszValName)/sizeof(TCHAR);
			while (ERROR_SUCCESS == RegEnumValue(hKeyParent, dwValFound, 
											rgszValName, &ccValName, NULL, NULL, NULL, NULL))
			{
				dwValFound++;
				ccValName = sizeof(rgszValName)/sizeof(TCHAR);
				
				dwType=0;
				cbData = sizeof(rgbData)/sizeof(BYTE);
				
				//  --------------------
				//  Measure RegQueryValueEx
				//  --------------------
				Perf_MarkBegin(MARK_VALUE_QUERY);
				lRet = RegQueryValueEx(hKeyParent, rgszValName, 0, &dwType, rgbData, &cbData);
				Perf_MarkEnd(MARK_VALUE_QUERY);
				
				if (ERROR_SUCCESS != lRet)
					REG_FAIL(RegQueryValueEx, lRet);
				ASSERT(REG_DWORD == dwType);    
			}
			ASSERT(PERF_NUM_KEYS_IN_SET== dwValFound);

			dwValFound=0;
			ccValName = sizeof(rgszValName)/sizeof(TCHAR);
			while (ERROR_SUCCESS == RegEnumValue(hKeyParent, 0, rgszValName, &ccValName, NULL, NULL, NULL, NULL))
			{
				dwValFound++;
				ccValName = sizeof(rgszValName)/sizeof(TCHAR);
				//  ----------------------
				//  Measure RegDeleteValue
				//  ----------------------
				Perf_MarkBegin(MARK_VALUE_DELETE);
				lRet = RegDeleteValue(hKeyParent, rgszValName);
				Perf_MarkEnd(MARK_VALUE_DELETE);
				
				if (ERROR_SUCCESS != lRet)
					REG_FAIL(RegQueryValueEx, lRet);
			}
			ASSERT(PERF_NUM_KEYS_IN_SET== dwValFound);

		}


	}    

	fRetVal=TRUE;

ErrorReturn :

	Perf_StopSysMonitor();	
	REG_CLOSE_KEY(hKeyParent);
	return fRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//  
// Function: L_MeasureSetValue_byValSize
//
// Purpose: Measure how much time it takes to create short and long value names. 
//			The function will create n REG_DWORDS under the same parent key. 
//
// Params:
//		hRoot		:	RegRoot under which the keys will be created. 
//		dwFlags		:	PERF_BIT_LONG_NAMES or PERF_BIT_SHORT_NAMES
//		dwNumValues	:	The number of values
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_MeasureSetValue_byValSize(HKEY hRoot, LPCTSTR pszDesc, DWORD dwFlags, DWORD dwNumValues)
{
	BOOL fRetVal=FALSE;
	DWORD i=0;
	TCHAR rgszValName[256]={0};
	HKEY hKeyParent=0;
	DWORD dwDispo=0;
	TCHAR rgszDesc[1024]={0};
	TCHAR rgszTemp[MAX_PATH]={0};
	PBYTE pbData=NULL;
	DWORD cbData=0;
	DWORD cbData_Ori=0;
	LONG lRet=0;
	DWORD dwType=0;
	
	ASSERT(hRoot && dwFlags && dwNumValues);
	ASSERT((HKEY_LOCAL_MACHINE == hRoot) || (HKEY_CURRENT_USER == hRoot));
	ASSERT((dwFlags & PERF_BIT_LARGE_VALUE) || (dwFlags & PERF_BIT_SMALL_VALUE));

	Hlp_HKeyToTLA(hRoot, rgszTemp, MAX_PATH);
	if (dwFlags & PERF_BIT_LARGE_VALUE) 
	{
		_stprintf(rgszDesc, _T("%s - %s - LargeValue - %d vals"), pszDesc, rgszTemp, dwNumValues);
		cbData = PERF_LARGE_VALUE;
	}
	else if (dwFlags & PERF_BIT_SMALL_VALUE)
	{
		_stprintf(rgszDesc, _T("%s - %s - SmallValue - %d vals"), pszDesc, rgszTemp, dwNumValues);
		cbData = PERF_SMALL_VALUE;
	}

	ASSERT(cbData);
	pbData = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
	CHECK_ALLOC(pbData);
	cbData_Ori=cbData;

	// Add the AVG macro for parsing
	_stprintf(rgszDesc, _T("%s,time=%%AVG%%"), rgszDesc);
	
	// Only register markers that we actually use at a time	
	if (!Perf_RegisterMark(MARK_VALUE_SET, _T("Test=RegSetValueEx - %s"), rgszDesc)) 		
		GENERIC_FAIL(Perf_RegisterMark);

	if (!Perf_RegisterMark(MARK_VALUE_QUERY, _T("Test=RegQueryValueEx - %s"), rgszDesc)) 		
		GENERIC_FAIL(Perf_RegisterMark);
		
	if (!Perf_RegisterMark(MARK_VALUE_DELETE, _T("Test=RegDeleteValue - %s"), rgszDesc)) 
		GENERIC_FAIL(Perf_RegisterMark);
	
	// Create the parent Key under which all the values will be made.
	RegDeleteKey(hRoot, PERF_KEY);    
	if (lRet=RegCreateKeyEx(hRoot, PERF_KEY, 0, NULL, 0, 0, NULL, &hKeyParent, &dwDispo))
		REG_FAIL(RegCreateKeyEx, lRet);
	ASSERT(REG_CREATED_NEW_KEY == dwDispo);
	if (REG_CREATED_NEW_KEY != dwDispo)
		REG_FAIL(RegCreateKeyEx, lRet);

	// Start monitoring CPU and memory
	Perf_StartSysMonitor(PERF_APP_NAME, SYS_MON_CPU | SYS_MON_MEM | SYS_MON_LOG, 
							g_dwPoolIntervalMs, g_dwCPUCalibrationMs);

	// Create a bunch of values. 
	// All values will be of the same size and will be type REG_BINARY
	for (i=0; i<dwNumValues; i++)
	{
		_stprintf(rgszValName, _T("Value_%d"), i);
		Hlp_FillBuffer(pbData, cbData, HLP_FILL_RANDOM);

		Perf_MarkBegin(MARK_VALUE_SET);
		lRet = RegSetValueEx(hKeyParent, rgszValName, 0, REG_BINARY, pbData, cbData);
		Perf_MarkEnd(MARK_VALUE_SET);

		if (ERROR_SUCCESS != lRet)
			REG_FAIL(RegSetValueEx, lRet);
	}

	// Read the values. 
	// All values will be of the same size and will be type REG_BINARY
	for (i=0; i<dwNumValues; i++)
	{
		_stprintf(rgszValName, _T("Value_%d"), i);

		dwType=0;
		cbData = cbData_Ori;
		
		Perf_MarkBegin(MARK_VALUE_QUERY);
		lRet = RegQueryValueEx(hKeyParent, rgszValName, 0, &dwType, pbData, &cbData);
		Perf_MarkEnd(MARK_VALUE_QUERY);

		if (ERROR_SUCCESS != lRet)
			REG_FAIL(RegSetValueEx, lRet);
		ASSERT(REG_BINARY == dwType);
	}

	// Delete the values. 
	// All values will be of the same size and will be type REG_BINARY
	for (i=0; i<dwNumValues; i++)
	{
		_stprintf(rgszValName, _T("Value_%d"), i);

		Perf_MarkBegin(MARK_VALUE_DELETE);
		lRet = RegDeleteValue(hKeyParent, rgszValName);
		Perf_MarkEnd(MARK_VALUE_DELETE);

		if (ERROR_SUCCESS != lRet)
			REG_FAIL(RegDeleteValue, lRet);
	}

	fRetVal=TRUE;
	
ErrorReturn :
	
	Perf_StopSysMonitor();	
	FREE(pbData);
	REG_CLOSE_KEY(hKeyParent);
	return fRetVal;
}



///////////////////////////////////////////////////////////////////////////////
//  
// Function: L_MeasureSetValue_byRoot
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_MeasureSetValue_byRoot(HKEY hRoot, LPCTSTR pszDesc)
{
	BOOL fRetVal=FALSE;
	DWORD dwFlags=0;
	DWORD dwNumValues=0;

	//  --------------------------
	//  SCENARIO 1:
	//  Measure time taken for long value names
	//  --------------------------
	TRACE(TEXT("Measure time taken for long value names...\r\n"));
	dwFlags = PERF_BIT_ONE_KEY | PERF_BIT_LONG_NAME;
	if (!L_MeasureSetValue_byNameLen(hRoot, pszDesc, dwFlags, PERF_MANY_VALUES))
		goto ErrorReturn;

	//  --------------------------
	//  SCENARIO 2:
	//  Measure time taken for sets of multiple long value names
	//  --------------------------
	TRACE(TEXT("Measure time taken to RegSetValue sets of %d long value names...\r\n"), PERF_NUM_KEYS_IN_SET);
	dwFlags = PERF_BIT_MULTIPLE_KEYS | PERF_BIT_LONG_NAME;
	if (!L_MeasureSetValue_byNameLen(hRoot, pszDesc, dwFlags, PERF_MANY_VALUES))
		goto ErrorReturn;

	//  --------------------------
	//  SCENARIO 3:
	//  Measure time taken for short value names
	//  --------------------------
	TRACE(TEXT("Measure time taken for short value names ...\r\n"));
	dwFlags = PERF_BIT_ONE_KEY | PERF_BIT_SHORT_NAME;
	if (!L_MeasureSetValue_byNameLen(hRoot, pszDesc, dwFlags, PERF_MANY_VALUES))
		goto ErrorReturn;

	//  --------------------------
	//  SCENARIO 4:
	//  Measure time taken for multiple sets of short value names
	//  --------------------------
	TRACE(TEXT("Measure time taken to RegSetValue sets of %d short value names ...\r\n"), PERF_NUM_KEYS_IN_SET);
	dwFlags = PERF_BIT_MULTIPLE_KEYS | PERF_BIT_SHORT_NAME;
	if (!L_MeasureSetValue_byNameLen(hRoot, pszDesc, dwFlags, PERF_MANY_VALUES))
		goto ErrorReturn;

	//  --------------------------
	//  SCENARIO 5:
	//  Measure time taken for similar value names
	//  --------------------------
	TRACE(TEXT("Measure time taken To RegSetValue a set of %d similar value names ...\r\n"), PERF_NUM_KEYS_IN_SET);
	dwFlags = PERF_BIT_SIMILAR_NAMES ;
	if (!L_MeasureSetValue_byNameLen(hRoot, pszDesc, dwFlags, PERF_MANY_VALUES))
		goto ErrorReturn;

	//  -----------------------------
	//  SCENARIO 6: 
	//  Measure time taken for small values
	//  -----------------------------
	TRACE(TEXT("Measure time taken for small values ...\r\n"));
	dwFlags = PERF_BIT_SMALL_VALUE;
	if (!L_MeasureSetValue_byValSize(hRoot, pszDesc, dwFlags, PERF_MANY_VALUES))
		goto ErrorReturn;

	//  -----------------------------
	//  SCENARIO 7: 
	//  Measure time taken for large values
	//  -----------------------------
	TRACE(TEXT("Measure time taken for large values ...\r\n"));
	dwFlags = PERF_BIT_LARGE_VALUE;
	if (!L_MeasureSetValue_byValSize(hRoot, pszDesc, dwFlags, PERF_MANY_VALUES))
		goto ErrorReturn;

	fRetVal=TRUE;

ErrorReturn :
	return fRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//  
// Function: Tst_SetValue
//
// Purpose: Call the perf worker per ROOT. 
//			Measure the performance under HKEY_LOCAL_MACHINE 
//			and then measure it under HKEY_CURRENT_USER
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_SetValue(UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE )
{
	UNREFERENCED_PARAMETER(lpFTE);
	DWORD dwRetVal=TPR_FAIL;

	COMMON_TUX_HEADER;

	TRACE(TEXT("Measuring RegSetValue on HKLM...\r\n"));

	if (!L_MeasureSetValue_byRoot(HKEY_LOCAL_MACHINE, lpFTE->lpDescription))
		goto ErrorReturn;

	TRACE(TEXT("Measuring RegSetValue on HKCU...\r\n"));

	if (!L_MeasureSetValue_byRoot(HKEY_CURRENT_USER, lpFTE->lpDescription))
		goto ErrorReturn;

	dwRetVal=TPR_PASS;

ErrorReturn :
	return dwRetVal;
}



///////////////////////////////////////////////////////////////////////////////
//  
// Function: L_OpenKey_PathKeys
//
// Params:
//		dwFlags =	PERF_OPEN_PATHS
//					or
//					PERF_OPEN_LEAVES
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_OpenKey_PathKeys(HKEY hRoot, LPCTSTR pszDesc, DWORD dwNumKeys, DWORD dwFlags)
{
	BOOL fRetVal=FALSE;
	TCHAR rgszTemp[1024]={0};
	TCHAR rgszKeyName[MAX_PATH]={0};
	TCHAR rgszRoot[MAX_PATH]={0};
	DWORD k=0;
	DWORD dwDispo=0;
	HKEY hKeyParent=0;
	HKEY hKey=0;
	LONG lRet=0;

	ASSERT ((PERF_OPEN_LEAVES == dwFlags) || (PERF_OPEN_PATHS == dwFlags));

	// Create a bunch of keys , say 16 levels deep. 
	for (k=0; k<dwNumKeys; k++)
	{
		_stprintf(rgszKeyName, _T("%s\\DeepKey_%d"), PERF_KEY_14_LEVEL, k);
		RegDeleteKey(hRoot, rgszKeyName);
		
		if (lRet = RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, 0, 0, NULL, &hKey, &dwDispo))
			REG_FAIL(RegCreateKeyEx, lRet);
		if ((ERROR_SUCCESS != lRet) || (REG_CREATED_NEW_KEY != dwDispo))
			REG_FAIL(RegCreateKeyEx, lRet);
		
		REG_CLOSE_KEY(hKey);
	}

	Hlp_HKeyToTLA(hRoot, rgszRoot, MAX_PATH);

	// Measure opening keys with paths. 
	if (PERF_OPEN_PATHS == dwFlags)
	{
		_stprintf(rgszTemp, _T("%s - PATH keys - %s - %d keys"), pszDesc, rgszRoot, dwNumKeys);

		// Add the AVG macro for parsing
		_stprintf(rgszTemp, _T("%s,time=%%AVG%%"), rgszTemp);

		// Only register markers that we actually use at a time	
		if (!Perf_RegisterMark(MARK_KEY_OPEN, _T("Test=RegOpenKey - %s"), rgszTemp)) 
			GENERIC_FAIL(Perf_RegisterMark);

		for (k=0; k<dwNumKeys; k++)
		{
			_stprintf(rgszKeyName, _T("%s\\DeepKey_%d"), PERF_KEY_14_LEVEL, k);
			Perf_MarkBegin(MARK_KEY_OPEN);
			lRet = RegOpenKeyEx(hRoot, rgszKeyName, 0, 0, &hKey);
			Perf_MarkEnd(MARK_KEY_OPEN);

			if (ERROR_SUCCESS != lRet) 
				REG_FAIL(RegOpenKeyEx, lRet);
			REG_CLOSE_KEY(hKey);
		}
	}
	// Measure opening leaf keys of the path.
	else if (PERF_OPEN_LEAVES == dwFlags)
	{
		_stprintf(rgszTemp, _T("%s - Leaf keys - %s - %d keys"), pszDesc, rgszRoot, dwNumKeys);

		// Add the AVG macro for parsing
		_stprintf(rgszTemp, _T("%s,time=%%AVG%%"), rgszTemp);

		// Only register markers that we actually use at a time	
		if (!Perf_RegisterMark(MARK_KEY_OPEN, _T("Test=RegOpenKey - %s"), rgszTemp)) 
			GENERIC_FAIL(Perf_RegisterMark);

		lRet = RegOpenKeyEx(hRoot, PERF_KEY_14_LEVEL, 0, 0, &hKeyParent);
		if (ERROR_SUCCESS != lRet) 
			REG_FAIL(RegOpenKeyEx, lRet);

		for (k=0; k<dwNumKeys; k++)
		{
			_stprintf(rgszKeyName, _T("DeepKey_%d"), k);
			Perf_MarkBegin(MARK_KEY_OPEN);
			lRet = RegOpenKeyEx(hKeyParent, rgszKeyName, 0, 0, &hKey);
			Perf_MarkEnd(MARK_KEY_OPEN);

			if (ERROR_SUCCESS != lRet) 
				REG_FAIL(RegOpenKeyEx, lRet);
			REG_CLOSE_KEY(hKey);
		}
		REG_CLOSE_KEY(hKeyParent);
	}
	else
		goto ErrorReturn;
	
	fRetVal=TRUE;

ErrorReturn :
	REG_CLOSE_KEY(hKey);
	REG_CLOSE_KEY(hKeyParent);
	return fRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//  
// Function: L_MeasureOpenKey_byRoot
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_MeasureOpenKey_byRoot(HKEY hRoot, LPCTSTR pszDesc)
{
	BOOL fRetVal=FALSE;
	DWORD dwFlags=0;
	DWORD dwNumKeys=0;

	// Start monitoring CPU and memory
	Perf_StartSysMonitor(PERF_APP_NAME, SYS_MON_CPU | SYS_MON_MEM | SYS_MON_LOG, 
							g_dwPoolIntervalMs, g_dwCPUCalibrationMs);

	// SCENARIO 1: 
	// Open keys that are paths. 
	TRACE(TEXT("Open keys that are paths. ...\r\n"));
	dwFlags = PERF_OPEN_PATHS;
	dwNumKeys=PERF_MANY_KEYS;
	if (!L_OpenKey_PathKeys(hRoot, pszDesc, dwNumKeys, dwFlags))
		goto ErrorReturn;
	
	// SCENARIO 2:
	// Open keys at say 16 levels deep
	TRACE(TEXT("Open keys at say 16 levels deep ...\r\n"));
	dwFlags = PERF_OPEN_LEAVES; 
	dwNumKeys = PERF_MANY_KEYS;
	if (!L_OpenKey_PathKeys(hRoot, pszDesc, dwNumKeys, dwFlags))
		goto ErrorReturn;

	Perf_StopSysMonitor();	

	fRetVal=TRUE;
	
ErrorReturn :
	return fRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//  
// Function: Tst_OpenKey
//
// Purpose: Call the perf worker per ROOT. 
//			Measure the performance under HKEY_LOCAL_MACHINE 
//			and then measure it under HKEY_CURRENT_USER
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_OpenKey(UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE )
{
	UNREFERENCED_PARAMETER(lpFTE);
	DWORD dwRetVal=TPR_FAIL;

	COMMON_TUX_HEADER;

	TRACE(TEXT("Measuring OpenKey on HKLM ...\r\n"));

	if (!L_MeasureOpenKey_byRoot(HKEY_LOCAL_MACHINE, lpFTE->lpDescription))
		goto ErrorReturn;

	TRACE(TEXT("Measuring open key on HKCU ...\r\n"));

	if (!L_MeasureOpenKey_byRoot(HKEY_CURRENT_USER, lpFTE->lpDescription))
		goto ErrorReturn;

	dwRetVal=TPR_PASS;

ErrorReturn :
	return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
//  Creates specified random values under the given key. 
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_CreateRandomVals(HKEY hKey, DWORD dwNumVals)
{
	BOOL fRetVal=FALSE;
	BYTE rgbData[100]={0};
	DWORD cbData = 0;
	DWORD k=0;
	TCHAR rgszValName[MAX_PATH]={0};
	DWORD dwType=0;
	LONG lRet=0;

	ASSERT(hKey && dwNumVals);

	for (k=0; k<dwNumVals; k++)
	{
		_stprintf(rgszValName, _T("Value_%d"), k);
		dwType=0;
		cbData = sizeof(rgbData)/sizeof(BYTE);
		
		if (!Hlp_GenRandomValData(&dwType, rgbData, &cbData))
			GENERIC_FAIL(Hlp_GenRandomValData);

		if (lRet = RegSetValueEx(hKey, rgszValName, 0, dwType, rgbData, cbData)) 
			REG_FAIL(RegSetValueEx, lRet);
	}

	fRetVal=TRUE;
	
ErrorReturn :
	return fRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
//  Creates specified number of keys under the given key. 
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_CreateRandomKeys(HKEY hKey, DWORD dwNumKeys)
{
	BOOL fRetVal=FALSE;
	DWORD k=0;
	TCHAR rgszKeyName[MAX_PATH]={0};
	LONG lRet=0;
	HKEY hKeyChild=0;
	DWORD dwDispo=0;

	ASSERT(hKey && dwNumKeys);

	for (k=0; k<dwNumKeys; k++)
	{
		_stprintf(rgszKeyName, _T("Key_%d"), k);

		lRet = RegCreateKeyEx(hKey, rgszKeyName, 0, NULL, 0, NULL, NULL, &hKeyChild, &dwDispo);

		if ((ERROR_SUCCESS != lRet) || (REG_CREATED_NEW_KEY != dwDispo))
			REG_FAIL(RegCreateKeyEx, lRet);
		
		REG_CLOSE_KEY(hKeyChild);
	}

	fRetVal=TRUE;
	
ErrorReturn :
	REG_CLOSE_KEY(hKeyChild);
	return fRetVal;
}



///////////////////////////////////////////////////////////////////////////////
//  
// Function: L_CreateTree
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_CreateTree(HKEY hKey, DWORD dwMaxLevel, DWORD dwThisLevel, DWORD dwNumKeys, DWORD dwNumVals)
{
	BOOL fRetVal=FALSE;
	DWORD dwIndex=0;
	TCHAR rgszKeyName[MAX_PATH]={0};
	DWORD ccKeyName=0;
	HKEY hEnumKey=0;
	LONG lRet=0;
	
	// Create dwNumVals. 
	if (!L_CreateRandomVals(hKey, dwNumVals))
		goto ErrorReturn;

	// If we've reached the max depth then return. 
	if (dwThisLevel == dwMaxLevel)
	{
		fRetVal=TRUE;
		goto ErrorReturn;
	}

	// Create dwNumChildKeys.
	if (!L_CreateRandomKeys(hKey, dwNumKeys))
		goto ErrorReturn;
	
	// Enumerate all the child keys and send them to createTree. 
	dwIndex=0;
	ccKeyName = sizeof(rgszKeyName)/sizeof(TCHAR);
	while (ERROR_SUCCESS == (lRet = RegEnumKeyEx(hKey, dwIndex, rgszKeyName, &ccKeyName, NULL, NULL, NULL, NULL)))
	{
		if (lRet = RegOpenKeyEx(hKey, rgszKeyName, 0, NULL, &hEnumKey))
			REG_FAIL(RegOpenKeyEx, lRet);
		
		if (!L_CreateTree(hEnumKey, dwMaxLevel, dwThisLevel+1, dwNumKeys, dwNumVals))
			goto ErrorReturn;

		ccKeyName = sizeof(rgszKeyName)/sizeof(TCHAR);
		dwIndex++;
		REG_CLOSE_KEY(hEnumKey);
	}

	fRetVal=TRUE;
	
ErrorReturn :
	REG_CLOSE_KEY(hEnumKey);
	return fRetVal;
}



///////////////////////////////////////////////////////////////////////////////
//  
// Function: Tst_DeleteTree
//
// Purpose: 
//			Create a huge tree, and deletes it. 
//			Measures DeleteKey 5 times. 
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_MeasureDeleteTree_byRoot(HKEY hRoot, LPCTSTR pszDesc)
{
	BOOL fRetVal=FALSE;
	HKEY hKeyParent=0;
	DWORD dwDispo=0;
	LONG lRet=0;
	TCHAR rgszKeyName[MAX_PATH]={0};
	TCHAR rgszDesc[1024]={0};
	DWORD k=0;

	Hlp_HKeyToTLA(hRoot, rgszKeyName, MAX_PATH);
	_stprintf(rgszDesc, _T("%s - %s - 5 Deep 5 wide"), pszDesc, rgszKeyName);

	// Add the AVG macro for parsing
	_stprintf(rgszDesc, _T("%s,time=%%AVG%%"), rgszDesc);

	// Only register markers that we actually use at a time	
	if (!Perf_RegisterMark(MARK_KEY_DELETE, _T("Test=RegDeleteKey - %s"), rgszDesc)) 
		GENERIC_FAIL(Perf_RegisterMark);	

	// Start monitoring CPU and memory
	Perf_StartSysMonitor(PERF_APP_NAME, SYS_MON_CPU | SYS_MON_MEM | SYS_MON_LOG, 
							g_dwPoolIntervalMs, g_dwCPUCalibrationMs);

	for (k=0; k<PERF_TREE_ITER_COUNT; k++)
	{
		// Create the parent key. 
		RegDeleteKey(hRoot, PERF_KEY);
		if (lRet = RegCreateKeyEx(hRoot, PERF_KEY, 0, NULL, 0, 0, NULL, &hKeyParent, &dwDispo))
			REG_FAIL(RegCreateKeyEx, lRet);
		if ((ERROR_SUCCESS != lRet) || (REG_CREATED_NEW_KEY != dwDispo))
				REG_FAIL(RegCreateKeyEx, lRet);

		// This will create a tree that is 5 levels deep, 
		// Every Key in this tree will have 5 subkeys and 5 vals in the key. 
		// Leafkeys will have only 5 values, and no subkeys. 
		TRACE(TEXT("Recursively Creating a tree ...\r\n"));
		if (!L_CreateTree(hKeyParent, 5, 0, 5, 5))
			goto ErrorReturn;

		REG_CLOSE_KEY(hKeyParent);

		//  -------------------
		//  Measure DeleteKey
		//  -------------------
		Perf_MarkBegin(MARK_KEY_DELETE);
		lRet = RegDeleteKey(hRoot, PERF_KEY);
		Perf_MarkEnd(MARK_KEY_DELETE);
		
		if (ERROR_SUCCESS != lRet)
			GENERIC_FAIL(RegDeleteKey);
	}
	
	fRetVal=TRUE;

ErrorReturn :

	Perf_StopSysMonitor();		
	REG_CLOSE_KEY(hKeyParent);
	RegDeleteKey(hRoot, PERF_KEY);
	return fRetVal;
}



///////////////////////////////////////////////////////////////////////////////
//  
// Function: Tst_DeleteTree
//
// Purpose: Call the perf worker per ROOT. 
//			Measure the performance under HKEY_LOCAL_MACHINE 
//			and then measure it under HKEY_CURRENT_USER
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_DeleteTree(UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE ) 
{
	UNREFERENCED_PARAMETER(lpFTE);
	DWORD   dwRetVal=TPR_FAIL;

	COMMON_TUX_HEADER;

	TRACE(TEXT("Measuring DeleteTree on HKLM ...\r\n"));
	if (!L_MeasureDeleteTree_byRoot(HKEY_LOCAL_MACHINE, lpFTE->lpDescription))
		goto ErrorReturn;

	TRACE(TEXT("Measuring DeleteTree on HKCU ...\r\n"));
	if (!L_MeasureDeleteTree_byRoot(HKEY_CURRENT_USER, lpFTE->lpDescription))
		goto ErrorReturn;

	dwRetVal=TPR_PASS;

ErrorReturn :
	return dwRetVal;

}
