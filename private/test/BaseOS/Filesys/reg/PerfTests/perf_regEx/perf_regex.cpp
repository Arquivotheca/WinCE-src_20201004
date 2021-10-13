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
#include "perf_regEx.h"
#include "reghlpr.h"
#include <ceperf.h>

// For CPU and Memory Timer
DWORD g_dwPoolIntervalMs;
DWORD g_dwCPUCalibrationMs;
// for reg cache tests
DWORD g_dwRegCache_MAXKEYS;

DWORD  g_dwNumTestIterations = DEFAULT_NUM_TEST_ITERATIONS;

CPerfHelper *g_pMARK_KEY_ENUM_5 = NULL,*g_pMARK_VALUE_ENUM_5=NULL;
CPerfHelper *g_pMARK_KEY_ENUM_10 = NULL,*g_pMARK_VALUE_ENUM_10=NULL;
CPerfHelper *g_pMARK_KEY_ENUM_50 = NULL,*g_pMARK_VALUE_ENUM_50=NULL;
CPerfHelper *g_pMARK_KEY_ENUM_100 = NULL,*g_pMARK_VALUE_ENUM_100=NULL;

// --------------------------------------------------------------------
// Creates a marker name for perf logging
// --------------------------------------------------------------------
LPTSTR CreateMarkerName(__out_ecount(cbLength) LPTSTR pszPerfMarkerName,int cbLength,
                        LPCTSTR pszTestName)
{
    //T stands for Test
    VERIFY(SUCCEEDED(StringCchPrintf(pszPerfMarkerName, cbLength, _T("T=%s"), pszTestName)));
    return pszPerfMarkerName;
}


// --------------------------------------------------------------------
// Creates a new logging object
// --------------------------------------------------------------------
CPerfHelper * MakeNewPerflogger(
    LPCTSTR pszPerfMarkerName,
    LPCTSTR pszPerfMarkerParams,
    PerfDataType dataType,
    DWORD dwDataSize,
    DWORD dwFullSize)
{
    if(pszPerfMarkerParams==NULL)
        pszPerfMarkerParams=_T("");
    CPerfHelper * pPerfObject = NULL;
    pPerfObject = new CPerfScenario(pszPerfMarkerName, pszPerfMarkerParams, dataType, dwDataSize, dwFullSize, (float)g_dwNumTestIterations);
    return pPerfObject;
}

///////////////////////////////////////////////////////////////////////////////
//  
// Function: L_Measure_CreateKey
//
// Purpose: 
//            Create a bunch of keys as specified and measure the performance.
//
// Params:
//            dwFlags:    PERF_BIT_LONG_NAME
//                    PERF_BIT_SHORT_NAME
///////////////////////////////////////////////////////////////////////////////
BOOL L_Measure_CreateKey(HKEY hRoot, LPCTSTR pszDesc, DWORD dwFlags, DWORD dwNumKeys)
{
    BOOL fRetVal=FALSE;
    LONG lRet=0;
    HKEY hKeyParent=0;
    HKEY hKeyChild=0;
    DWORD dwDispo=0;
    DWORD i=0, k=0;
    TCHAR rgszKeyName[MAX_PATH]={0};
    TCHAR rgszTemp[MAX_PATH]={0};
    TCHAR rgszDesc[1024]={0};
    TCHAR tempDesc[1024]={0};
    DWORD dwKeyFound, dwSubKeys, dwMaxSubKeyLen, dwMaxClassLen, dwNumVals, dwMaxValNameLen, dwMaxValLen;
    HKEY *phKey=NULL;
    CPerfHelper *pCPerCreateKey = NULL,*pCPerOpenKey = NULL,*pCPerCloseKey = NULL,*pCPerDeleteKey = NULL,*pCPerQueryInfoKey = NULL;
    LPTSTR pszPerfMarkerName = NULL;
        
    ASSERT((HKEY_LOCAL_MACHINE == hRoot) || (HKEY_CURRENT_USER == hRoot));
    ASSERT(dwNumKeys && dwFlags);

    Hlp_HKeyToTLA(hRoot, rgszTemp, MAX_PATH);
    StringCchPrintf(rgszDesc,1024,_T("%s-%s"), pszDesc, rgszTemp);

    // dwFlags can either be PERF_BIT_ONE_KEY, PERF_BIT_MULTIPLE_KEY or PERF_BIT_SIMILAR_NAMES
    if (dwFlags & PERF_BIT_ONE_KEY)
        StringCchPrintf(rgszDesc,1024,_T("%s-SK"), rgszDesc);

    else if (dwFlags & PERF_BIT_MULTIPLE_KEYS)
        StringCchPrintf(rgszDesc,1024,_T("%s-MK"), rgszDesc);

    else if (dwFlags & PERF_BIT_SIMILAR_NAMES)
        StringCchPrintf(rgszDesc,1024,_T("%s-SN"), rgszDesc);

    else if (dwFlags & PERF_BIT_OPENLIST)
    {
        // Open HKLM\Init say 1000 times. 
        StringCchPrintf(rgszDesc,1024,_T("%s-LO"), rgszDesc);
        phKey = (HKEY*)LocalAlloc(LMEM_ZEROINIT, PERF_OPEN_KEY_LIST_LEN*sizeof(HKEY));
        CHECK_ALLOC(phKey);

        for (k=0; k<PERF_OPEN_KEY_LIST_LEN; k++)
        {
            lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("init"), 0, NULL, &(phKey[k]));
            if (lRet)
                REG_FAIL(RegOpenKeyEx, lRet);
        }
    }

    // dwFlags can either have PERF_BIT_LONG_NAME or PERF_BIT_SHORT_NAME set. 
    if (dwFlags & PERF_BIT_LONG_NAME)
        StringCchPrintf(rgszDesc,1024,_T("%s-LN-%d Keys"), rgszDesc, dwNumKeys);

    else if (dwFlags & PERF_BIT_SHORT_NAME)
       StringCchPrintf(rgszDesc,1024,_T("%s-SN-%d Keys"), rgszDesc, dwNumKeys);

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CEPERF_MAX_ITEM_NAME_LEN);
    if ((NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    // Fill the perf strings with test-specific data
    StringCchPrintf(tempDesc,1024,_T("Create-%s"), rgszDesc);
    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,tempDesc);
    pCPerCreateKey = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == pCPerCreateKey)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    StringCchPrintf(tempDesc,1024,_T("OK-%s"), rgszDesc);
    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,tempDesc);
    pCPerOpenKey = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == pCPerOpenKey)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    StringCchPrintf(tempDesc,1024,_T("CK-%s"), rgszDesc);
    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,tempDesc);
    pCPerCloseKey = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == pCPerCloseKey)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    StringCchPrintf(tempDesc,1024,_T("DK-%s"), rgszDesc);
    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,tempDesc);
    pCPerDeleteKey = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == pCPerDeleteKey)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    if ((dwFlags & PERF_BIT_MULTIPLE_KEYS) || (dwFlags & PERF_BIT_SIMILAR_NAMES))
    {
        StringCchPrintf(tempDesc,1024,_T("QIK-%s"), rgszDesc);
        CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,tempDesc);
        pCPerQueryInfoKey = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
        if (NULL == pCPerQueryInfoKey)
        {
            ERRFAIL("Out of memory!");
            goto ErrorReturn;
        }
    }
    
    // Create the parent Key
    RegDeleteKey(hRoot, PERF_KEY);
    lRet=RegCreateKeyEx(hRoot, PERF_KEY, 0, NULL, 0, 0, NULL, &hKeyParent, &dwDispo);
    if (lRet)
        REG_FAIL(RegCreateKeyEx, lRet);

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
            
            pCPerCreateKey->StartLog();
            lRet=RegCreateKeyEx(hKeyParent, rgszKeyName, 0, NULL, 0, 0, NULL, &hKeyChild, &dwDispo);
            pCPerCreateKey->EndLog();
            
            if ((ERROR_SUCCESS != lRet) || (REG_CREATED_NEW_KEY != dwDispo))
                REG_FAIL(RegCreateKeyEx, lRet);

            REG_CLOSE_KEY(hKeyChild);

            //  ----------------
            //  Measure OpenKey
            //  ----------------
            pCPerOpenKey->StartLog();
            lRet = RegOpenKeyEx(hKeyParent, rgszKeyName, 0, NULL, &hKeyChild);
            pCPerOpenKey->EndLog();

            if (ERROR_SUCCESS != lRet) 
                REG_FAIL(RegOpenKeyEx, lRet);

            //  ----------------
            //  Measure CloseKey
            //  ----------------
            pCPerCloseKey->StartLog();
            REG_CLOSE_KEY(hKeyChild);
            pCPerCloseKey->EndLog();

            //  --------------
            //  Measure Deletekey 
            //  --------------
            pCPerDeleteKey->StartLog();
            lRet=RegDeleteKey(hKeyParent, rgszKeyName);
            pCPerDeleteKey->EndLog();

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
                pCPerCreateKey->StartLog();
                dwDispo=0;
                lRet=RegCreateKeyEx(hKeyParent, rgszKeyName, 0, NULL, 0, 0, NULL, &hKeyChild, &dwDispo);
                pCPerCreateKey->EndLog();

                if ((ERROR_SUCCESS != lRet) || (REG_CREATED_NEW_KEY != dwDispo))
                    REG_FAIL(RegCreateKeyEx, lRet);

                REG_CLOSE_KEY(hKeyChild);

                //  ----------------
                //  Measure OpenKey
                //  ----------------
                pCPerOpenKey->StartLog();
                lRet = RegOpenKeyEx(hKeyParent, rgszKeyName, 0, NULL, &hKeyChild);
                pCPerOpenKey->EndLog();

                if (ERROR_SUCCESS != lRet) 
                    REG_FAIL(RegOpenKeyEx, lRet);

                //  ----------------
                //  Measure CloseKey
                //  ----------------
                pCPerCloseKey->StartLog();
                REG_CLOSE_KEY(hKeyChild);
                pCPerCloseKey->EndLog();

                //  --------------
                //  Measure RegQueryInfo
                //  --------------
                pCPerQueryInfoKey->StartLog();
                lRet = RegQueryInfoKey(hKeyParent, NULL, NULL, NULL, 
                    &dwSubKeys, &dwMaxSubKeyLen, &dwMaxClassLen, &dwNumVals, &dwMaxValNameLen, 
                    &dwMaxValLen, NULL, NULL);
                pCPerQueryInfoKey->EndLog();

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
                pCPerDeleteKey->StartLog();
                lRet=RegDeleteKey(hKeyParent, rgszKeyName);
                pCPerDeleteKey->EndLog();

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
                StringCchPrintf(rgszKeyName,MAX_PATH,_T("Perf_Registry_key_number_%d"), k);
                //  --------------
                //  Measure CreateKey
                //  --------------
                pCPerCreateKey->StartLog();
                lRet=RegCreateKeyEx(hKeyParent, rgszKeyName, 0, NULL, 0, 0, NULL, &hKeyChild, &dwDispo);
                pCPerCreateKey->EndLog();

                if ((ERROR_SUCCESS != lRet) || (REG_CREATED_NEW_KEY != dwDispo))
                    REG_FAIL(RegCreateKeyEx, lRet);
                REG_CLOSE_KEY(hKeyChild);

                //  ----------------
                //  Measure OpenKey
                //  ----------------
                pCPerOpenKey->StartLog();
                lRet = RegOpenKeyEx(hKeyParent, rgszKeyName, 0, NULL, &hKeyChild);
                pCPerOpenKey->EndLog();

                if (ERROR_SUCCESS != lRet) 
                    REG_FAIL(RegOpenKeyEx, lRet);

                //  ----------------
                //  Measure CloseKey
                //  ----------------
                pCPerCloseKey->StartLog();
                REG_CLOSE_KEY(hKeyChild);
                pCPerCloseKey->EndLog();

                //  --------------
                //  Measure RegQueryInfo
                //  --------------
                pCPerQueryInfoKey->StartLog();
                lRet = RegQueryInfoKey(hKeyParent, NULL, NULL, NULL, 
                    &dwSubKeys, &dwMaxSubKeyLen, &dwMaxClassLen, &dwNumVals, &dwMaxValNameLen, 
                    &dwMaxValLen, NULL, NULL);
                pCPerQueryInfoKey->EndLog();

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
                pCPerDeleteKey->StartLog();
                lRet=RegDeleteKey(hKeyParent, rgszKeyName);
                pCPerDeleteKey->EndLog();

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
    CHECK_END_PERF(pCPerCreateKey);
    CHECK_END_PERF(pCPerOpenKey);
    CHECK_END_PERF(pCPerCloseKey);
    CHECK_END_PERF(pCPerDeleteKey);
    if ((dwFlags & PERF_BIT_MULTIPLE_KEYS) || (dwFlags & PERF_BIT_SIMILAR_NAMES))
    {
        CHECK_END_PERF(pCPerQueryInfoKey);
    }
    CHECK_FREE(pszPerfMarkerName);
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
//            Do all the scenarios under a certain root. 
//            This function doesn't measure any performance. It calls the worker
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_MeasureCreateKey_byRoot(HKEY hRoot, LPCTSTR pszDesc)
{
    BOOL fRetVal=FALSE;
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
//            Measure the performance under HKEY_LOCAL_MACHINE 
//            and then measure it under HKEY_CURRENT_USER
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_CreateKey(UINT uMsg, TPPARAM /*tpParam*/, const LPFUNCTION_TABLE_ENTRY lpFTE )
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
//            The function will create n REG_DWORDS under the same parent key. 
//
// Params:
//        hRoot        :    RegRoot under which the keys will be created. 
//        dwFlags        :    PERF_BIT_LONG_NAMES or PERF_BIT_SHORT_NAMES
//        dwNumVals    :    The number of values
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_MeasureSetValue_byNameLen(HKEY hRoot, LPCTSTR pszDesc, DWORD dwFlags, DWORD dwNumValues)
{
    BOOL fRetVal=FALSE;
    DWORD i=0,k=0;
    TCHAR rgszValName[MAX_PATH]={0};
    HKEY hKeyParent=0;
    DWORD dwDispo=0;
    TCHAR rgszDesc[1024]={0};
    TCHAR rgszTemp[1024]={0};
    LONG lRet=0;
    BYTE rgbData[100]={0};
    DWORD cbData=sizeof(rgbData)/sizeof(BYTE);
    DWORD dwType=0, ccValName, dwValFound;
    CPerfHelper *pCPerSetValueEx = NULL,*pCPerQueryValueEx = NULL,*pCPerDeleteValue = NULL;
    LPTSTR pszPerfMarkerName = NULL;

    ASSERT(hRoot && dwFlags && dwNumValues);
    ASSERT((HKEY_LOCAL_MACHINE == hRoot) || (HKEY_CURRENT_USER == hRoot));
    
    // Register the markers. 
    Hlp_HKeyToTLA(hRoot, rgszTemp, MAX_PATH);
    StringCchPrintf(rgszDesc,1024,_T("%s-%s"), pszDesc, rgszTemp);

    if (PERF_BIT_ONE_KEY & dwFlags)
        StringCchPrintf(rgszDesc,1024,_T("%s-OK"), rgszDesc);
    else if (PERF_BIT_MULTIPLE_KEYS & dwFlags)
        StringCchPrintf(rgszDesc,1024,_T("%s-MK"), rgszDesc);
    else if (PERF_BIT_SIMILAR_NAMES & dwFlags)
        StringCchPrintf(rgszDesc,1024,_T("%s-SN-%d vals"), rgszDesc, dwNumValues);


    if (PERF_BIT_LONG_NAME & dwFlags) 
        StringCchPrintf(rgszDesc,1024,_T("%s-LN-%d vals"), rgszDesc, dwNumValues);
    else if (PERF_BIT_SHORT_NAME & dwFlags)
        StringCchPrintf(rgszDesc,1024,_T("%s-SN-%d vals"), rgszDesc, dwNumValues);

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CEPERF_MAX_ITEM_NAME_LEN);
    if ((NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    // Fill the perf strings with test-specific data
    StringCchPrintf(rgszTemp,MAX_PATH,_T("SV-%s"), rgszDesc);
    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,rgszTemp);
    pCPerSetValueEx = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == pCPerSetValueEx)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    StringCchPrintf(rgszTemp,MAX_PATH,_T("QV-%s"), rgszDesc);
    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,rgszTemp);
    pCPerQueryValueEx= MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == pCPerQueryValueEx)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    StringCchPrintf(rgszTemp,MAX_PATH,_T("DV-%s"), rgszDesc);
    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,rgszTemp);
    pCPerDeleteValue= MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == pCPerDeleteValue)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }
        
    // Create the parent Key under which all the values will be made.
    RegDeleteKey(hRoot, PERF_KEY);    
    lRet=RegCreateKeyEx(hRoot, PERF_KEY, 0, NULL, 0, 0, NULL, &hKeyParent, &dwDispo);
    if (lRet)
        REG_FAIL(RegCreateKeyEx, lRet);
    ASSERT(REG_CREATED_NEW_KEY == dwDispo);
    if (REG_CREATED_NEW_KEY != dwDispo)
        REG_FAIL(RegCreateKeyEx, lRet);

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
            pCPerSetValueEx->StartLog();
            lRet = RegSetValueEx(hKeyParent, rgszValName, 0, REG_DWORD, (const BYTE*)&i, sizeof(DWORD));
            pCPerSetValueEx->EndLog();

            if (ERROR_SUCCESS != lRet)
                REG_FAIL(RegSetValueEx, lRet);

            //  ----------------------
            //  Measure RegQueryValue
            //  ----------------------
            dwType=0;
            cbData = sizeof(rgbData)/sizeof(BYTE);
            pCPerQueryValueEx->StartLog();
            lRet = RegQueryValueEx(hKeyParent, rgszValName, 0, &dwType, rgbData, &cbData);
            pCPerQueryValueEx->EndLog();

            if (ERROR_SUCCESS != lRet)
                REG_FAIL(RegQueryValueEx, lRet);
            ASSERT(REG_DWORD == dwType);

            //  ----------------------
            //  Measure RegDeleteValue
            //  ----------------------
            pCPerDeleteValue->StartLog();
            lRet = RegDeleteValue(hKeyParent, rgszValName);
            pCPerDeleteValue->EndLog();

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
                pCPerSetValueEx->StartLog();
                lRet = RegSetValueEx(hKeyParent, rgszValName, 0, REG_DWORD, (const BYTE*)&k, sizeof(DWORD));
                pCPerSetValueEx->EndLog();

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
                pCPerQueryValueEx->StartLog();
                lRet = RegQueryValueEx(hKeyParent, rgszValName, 0, &dwType, rgbData, &cbData);
                pCPerQueryValueEx->EndLog();

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
                pCPerDeleteValue->StartLog();
                lRet = RegDeleteValue(hKeyParent, rgszValName);
                pCPerDeleteValue->EndLog();

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
                StringCchPrintf(rgszValName,MAX_PATH,_T("This_Is_A_Common_Val_%d"), k);

                //  ----------------------
                //  Measure RegSetValue
                //  ----------------------
                pCPerSetValueEx->StartLog();
                lRet = RegSetValueEx(hKeyParent, rgszValName, 0, REG_DWORD, (const BYTE*)&k, sizeof(DWORD));
                pCPerSetValueEx->EndLog();

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
                pCPerQueryValueEx->StartLog();
                lRet = RegQueryValueEx(hKeyParent, rgszValName, 0, &dwType, rgbData, &cbData);
                pCPerQueryValueEx->EndLog();

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
                pCPerDeleteValue->StartLog();
                lRet = RegDeleteValue(hKeyParent, rgszValName);
                pCPerDeleteValue->EndLog();

                if (ERROR_SUCCESS != lRet)
                    REG_FAIL(RegQueryValueEx, lRet);
            }
            ASSERT(PERF_NUM_KEYS_IN_SET== dwValFound);

        }


    }    

    fRetVal=TRUE;

ErrorReturn :
    CHECK_END_PERF(pCPerSetValueEx);
    CHECK_END_PERF(pCPerQueryValueEx);
    CHECK_END_PERF(pCPerDeleteValue);
    CHECK_FREE(pszPerfMarkerName);
    REG_CLOSE_KEY(hKeyParent);
    return fRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//  
// Function: L_MeasureSetValue_byValSize
//
// Purpose: Measure how much time it takes to create short and long value names. 
//            The function will create n REG_DWORDS under the same parent key. 
//
// Params:
//        hRoot        :    RegRoot under which the keys will be created. 
//        dwFlags        :    PERF_BIT_LONG_NAMES or PERF_BIT_SHORT_NAMES
//        dwNumValues    :    The number of values
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_MeasureSetValue_byValSize(HKEY hRoot, LPCTSTR pszDesc, DWORD dwFlags, DWORD dwNumValues)
{
    BOOL fRetVal=FALSE;
    DWORD i=0;
    TCHAR rgszValName[MAX_PATH]={0};
    HKEY hKeyParent=0;
    DWORD dwDispo=0;
    TCHAR rgszDesc[1024]={0};
    TCHAR rgszTemp[1024]={0};
    PBYTE pbData=NULL;
    DWORD cbData=0;
    DWORD cbData_Ori=0;
    LONG lRet=0;
    DWORD dwType=0;
    CPerfHelper *pCPerSetValueEx = NULL,*pCPerQueryValueEx = NULL,*pCPerDeleteValue = NULL;
    LPTSTR pszPerfMarkerName = NULL;


    ASSERT(hRoot && dwFlags && dwNumValues);
    ASSERT((HKEY_LOCAL_MACHINE == hRoot) || (HKEY_CURRENT_USER == hRoot));
    ASSERT((dwFlags & PERF_BIT_LARGE_VALUE) || (dwFlags & PERF_BIT_SMALL_VALUE));

    Hlp_HKeyToTLA(hRoot, rgszTemp, MAX_PATH);
    if (dwFlags & PERF_BIT_LARGE_VALUE) 
    {
        StringCchPrintf(rgszDesc,1024,_T("%s-%s-LV-%d vals"), pszDesc, rgszTemp, dwNumValues);
        cbData = PERF_LARGE_VALUE;
    }
    else if (dwFlags & PERF_BIT_SMALL_VALUE)
    {
        StringCchPrintf(rgszDesc,1024,_T("%s-%s-SV-%d vals"), pszDesc, rgszTemp, dwNumValues);
        cbData = PERF_SMALL_VALUE;
    }

    ASSERT(cbData);
    pbData = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbData);
    CHECK_ALLOC(pbData);
    cbData_Ori=cbData;

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CEPERF_MAX_ITEM_NAME_LEN);
    if ((NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    // Fill the perf strings with test-specific data
    StringCchPrintf(rgszTemp,1024,_T("SV-%s"), rgszDesc);
    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,rgszTemp);
    pCPerSetValueEx = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == pCPerSetValueEx)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    StringCchPrintf(rgszTemp,1024,_T("QV-%s"), rgszDesc);
    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,rgszTemp);
    pCPerQueryValueEx = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == pCPerQueryValueEx)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    StringCchPrintf(rgszTemp,1024,_T("DV-%s"), rgszDesc);
    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,rgszTemp);
    pCPerDeleteValue = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == pCPerDeleteValue)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    // Create the parent Key under which all the values will be made.
    RegDeleteKey(hRoot, PERF_KEY);    
    lRet=RegCreateKeyEx(hRoot, PERF_KEY, 0, NULL, 0, 0, NULL, &hKeyParent, &dwDispo);
    if (lRet)
        REG_FAIL(RegCreateKeyEx, lRet);
    ASSERT(REG_CREATED_NEW_KEY == dwDispo);
    if (REG_CREATED_NEW_KEY != dwDispo)
        REG_FAIL(RegCreateKeyEx, lRet);

    // Create a bunch of values. 
    // All values will be of the same size and will be type REG_BINARY
    for (i=0; i<dwNumValues; i++)
    {
        StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), i);
        Hlp_FillBuffer(pbData, cbData, HLP_FILL_RANDOM);

        pCPerSetValueEx->StartLog();
        lRet = RegSetValueEx(hKeyParent, rgszValName, 0, REG_BINARY, pbData, cbData);
        pCPerSetValueEx->EndLog();

        if (ERROR_SUCCESS != lRet)
            REG_FAIL(RegSetValueEx, lRet);
    }

    // Read the values. 
    // All values will be of the same size and will be type REG_BINARY
    for (i=0; i<dwNumValues; i++)
    {
        StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), i);
        dwType=0;
        cbData = cbData_Ori;

        pCPerQueryValueEx->StartLog();
        lRet = RegQueryValueEx(hKeyParent, rgszValName, 0, &dwType, pbData, &cbData);
        pCPerQueryValueEx->EndLog();

        if (ERROR_SUCCESS != lRet)
            REG_FAIL(RegSetValueEx, lRet);
        ASSERT(REG_BINARY == dwType);
    }

    // Delete the values. 
    // All values will be of the same size and will be type REG_BINARY
    for (i=0; i<dwNumValues; i++)
    {
        StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), i);

        pCPerDeleteValue->StartLog();
        lRet = RegDeleteValue(hKeyParent, rgszValName);
        pCPerDeleteValue->EndLog();

        if (ERROR_SUCCESS != lRet)
            REG_FAIL(RegDeleteValue, lRet);
    }

    fRetVal=TRUE;

ErrorReturn :
    CHECK_END_PERF(pCPerSetValueEx);
    CHECK_END_PERF(pCPerQueryValueEx);
    CHECK_END_PERF(pCPerDeleteValue);
    CHECK_FREE(pszPerfMarkerName);
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
//            Measure the performance under HKEY_LOCAL_MACHINE 
//            and then measure it under HKEY_CURRENT_USER
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_SetValue(UINT uMsg, TPPARAM /*tpParam*/, const LPFUNCTION_TABLE_ENTRY lpFTE )
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
//        dwFlags =    PERF_OPEN_PATHS
//                    or
//                    PERF_OPEN_LEAVES
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
    CPerfHelper *pCPerOpenKey = NULL;
    LPTSTR pszPerfMarkerName = NULL;


    ASSERT ((PERF_OPEN_LEAVES == dwFlags) || (PERF_OPEN_PATHS == dwFlags));

    // Create a bunch of keys , say 16 levels deep. 
    for (k=0; k<dwNumKeys; k++)
    {
        StringCchPrintf(rgszKeyName,MAX_PATH,_T("%s\\DeepKey_%d"), PERF_KEY_14_LEVEL, k);
        RegDeleteKey(hRoot, rgszKeyName);

        lRet = RegCreateKeyEx(hRoot, rgszKeyName, 0, NULL, 0, 0, NULL, &hKey, &dwDispo);
        if (lRet)
            REG_FAIL(RegCreateKeyEx, lRet);
        if ((ERROR_SUCCESS != lRet) || (REG_CREATED_NEW_KEY != dwDispo))
            REG_FAIL(RegCreateKeyEx, lRet);

        REG_CLOSE_KEY(hKey);
    }

    Hlp_HKeyToTLA(hRoot, rgszRoot, MAX_PATH);
    
    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CEPERF_MAX_ITEM_NAME_LEN);
    if ((NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    // Measure opening keys with paths. 
    if (PERF_OPEN_PATHS == dwFlags)
    {
        StringCchPrintf(rgszTemp,1024,_T("%s-PATH keys-%s-%d keys"), pszDesc, rgszRoot, dwNumKeys);

        CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,rgszTemp);
        pCPerOpenKey = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
        if (NULL == pCPerOpenKey)
        {
            ERRFAIL("Out of memory!");
            goto ErrorReturn;
        }


        for (k=0; k<dwNumKeys; k++)
        {
            StringCchPrintf(rgszKeyName,MAX_PATH,_T("%s\\DeepKey_%d"), PERF_KEY_14_LEVEL, k);
            pCPerOpenKey->StartLog();
            lRet = RegOpenKeyEx(hRoot, rgszKeyName, 0, 0, &hKey);
            pCPerOpenKey->EndLog();

            if (ERROR_SUCCESS != lRet) 
                REG_FAIL(RegOpenKeyEx, lRet);
            REG_CLOSE_KEY(hKey);
        }
        CHECK_END_PERF(pCPerOpenKey);
        
    }
    // Measure opening leaf keys of the path.
    else if (PERF_OPEN_LEAVES == dwFlags)
    {
        StringCchPrintf(rgszTemp,1024,_T("%s-LK-%s-%d keys"), pszDesc, rgszRoot, dwNumKeys);

        CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,rgszTemp);
        pCPerOpenKey = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
        if (NULL == pCPerOpenKey)
        {
            ERRFAIL("Out of memory!");
            goto ErrorReturn;
        }

        lRet = RegOpenKeyEx(hRoot, PERF_KEY_14_LEVEL, 0, 0, &hKeyParent);
        if (ERROR_SUCCESS != lRet) 
            REG_FAIL(RegOpenKeyEx, lRet);

        for (k=0; k<dwNumKeys; k++)
        {
            StringCchPrintf(rgszKeyName,MAX_PATH,_T("DeepKey_%d"), k);
            pCPerOpenKey->StartLog();
            lRet = RegOpenKeyEx(hKeyParent, rgszKeyName, 0, 0, &hKey);
            pCPerOpenKey->EndLog();

            if (ERROR_SUCCESS != lRet) 
                REG_FAIL(RegOpenKeyEx, lRet);
            REG_CLOSE_KEY(hKey);
        }
        REG_CLOSE_KEY(hKeyParent);
        CHECK_END_PERF(pCPerOpenKey);
    }
    else
        goto ErrorReturn;

    fRetVal=TRUE;

ErrorReturn :
    CHECK_END_PERF(pCPerOpenKey);
    CHECK_FREE(pszPerfMarkerName);
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

    fRetVal=TRUE;

ErrorReturn :
    return fRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//  
// Function: Tst_OpenKey
//
// Purpose: Call the perf worker per ROOT. 
//            Measure the performance under HKEY_LOCAL_MACHINE 
//            and then measure it under HKEY_CURRENT_USER
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_OpenKey(UINT uMsg, TPPARAM /*tpParam*/, const LPFUNCTION_TABLE_ENTRY lpFTE )
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
        StringCchPrintf(rgszValName,MAX_PATH,_T("Value_%d"), k);
        dwType=0;
        cbData = sizeof(rgbData)/sizeof(BYTE);

        if (!Hlp_GenRandomValData(&dwType, rgbData, &cbData))
            GENERIC_FAIL(Hlp_GenRandomValData);

        lRet = RegSetValueEx(hKey, rgszValName, 0, dwType, rgbData, cbData);
        if (lRet) 
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
        StringCchPrintf(rgszKeyName,MAX_PATH,_T("Key_%d"), k);
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
        lRet = RegOpenKeyEx(hKey, rgszKeyName, 0, NULL, &hEnumKey);
        if (lRet)
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
// Function: L_MeasureDeleteTree_byRoot
//
// Purpose: 
//            Create a huge tree, and deletes it. 
//            Measures DeleteKey 5 times. 
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
    CPerfHelper *pCPerDeleteKey = NULL;
    LPTSTR pszPerfMarkerName = NULL;

    Hlp_HKeyToTLA(hRoot, rgszKeyName, MAX_PATH);
    StringCchPrintf(rgszDesc,1024,_T("%s-%s-5 Deep 5 wide"), pszDesc, rgszKeyName);

    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CEPERF_MAX_ITEM_NAME_LEN);
    if ((NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,rgszDesc);
    pCPerDeleteKey = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == pCPerDeleteKey)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    for (k=0; k<PERF_TREE_ITER_COUNT; k++)
    {
        // Create the parent key. 
        RegDeleteKey(hRoot, PERF_KEY);
        lRet = RegCreateKeyEx(hRoot, PERF_KEY, 0, NULL, 0, 0, NULL, &hKeyParent, &dwDispo);
        if (lRet)
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
        pCPerDeleteKey->StartLog();
        lRet = RegDeleteKey(hRoot, PERF_KEY);
        pCPerDeleteKey->EndLog();

        if (ERROR_SUCCESS != lRet)
            GENERIC_FAIL(RegDeleteKey);
    }

    fRetVal=TRUE;

ErrorReturn :
    CHECK_END_PERF(pCPerDeleteKey)
    CHECK_FREE(pszPerfMarkerName);
    REG_CLOSE_KEY(hKeyParent);
    RegDeleteKey(hRoot, PERF_KEY);
    return fRetVal;
}



///////////////////////////////////////////////////////////////////////////////
//  
// Function: Tst_DeleteTree
//
// Purpose: Call the perf worker per ROOT. 
//            Measure the performance under HKEY_LOCAL_MACHINE 
//            and then measure it under HKEY_CURRENT_USER
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_DeleteTree(UINT uMsg, TPPARAM /*tpParam*/, const LPFUNCTION_TABLE_ENTRY lpFTE ) 
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


///////////////////////////////////////////////////////////////////////////////
//  
// Function: Tst_FlushTree
//
// Purpose: 
//            Create a huge tree, and deletes it. 
//            Measures DeleteKey 5 times. 
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_MeasureFlushTree_byRoot(HKEY hRoot, LPCTSTR pszDesc)
{
    BOOL fRetVal=FALSE;
    HKEY hKeyParent=0;
    DWORD dwDispo=0;
    LONG lRet=0;
    TCHAR rgszKeyName[MAX_PATH]={0};
    TCHAR rgszValName[256]={0};
    TCHAR rgszDesc[1024]={0};
    DWORD k=0;
    CPerfHelper *pCPerFlushKey = NULL;
    LPTSTR pszPerfMarkerName = NULL;

    Hlp_HKeyToTLA(hRoot, rgszKeyName, MAX_PATH);
    
    StringCchPrintf(rgszDesc,1024,_T("%s-%s-for every %d keys"), pszDesc, rgszKeyName,PERF_FEW_KEYS);
    
    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CEPERF_MAX_ITEM_NAME_LEN);
    if ((NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    // Fill the perf strings with test-specific data
    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,rgszDesc);
    pCPerFlushKey = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == pCPerFlushKey)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    for (k=0; k<PERF_TREE_ITER_COUNT; k++)
    {

        for(int i=0;i<PERF_FEW_KEYS;i++)
        {
            // Create the parent key. 
            StringCchPrintf(rgszDesc,1024,_T("%s-%d%d"), PERF_KEY, k,i);
            RegDeleteKey(hRoot, rgszDesc);
            lRet = RegCreateKeyEx(hRoot, rgszDesc, 0, NULL, 0, 0, NULL, &hKeyParent, &dwDispo);
            if (lRet)
                REG_FAIL(RegCreateKeyEx, lRet);
            if ((ERROR_SUCCESS != lRet) || (REG_CREATED_NEW_KEY != dwDispo))
                REG_FAIL(RegCreateKeyEx, lRet);

            Hlp_GenStringData(rgszValName, PERF_SHORT_NAME_LEN, TST_FLAG_ALPHA_NUM);

            lRet = RegSetValueEx(hKeyParent, rgszValName, 0, REG_DWORD, (const BYTE*)&i, sizeof(DWORD));
            if (ERROR_SUCCESS != lRet)
                REG_FAIL(RegSetValueEx, lRet);

            REG_CLOSE_KEY(hKeyParent);
        }


    }


    //  -------------------
    //  Measure FlushKey
    //  -------------------
    pCPerFlushKey->StartLog();
    lRet = RegFlushKey(hRoot);
    pCPerFlushKey->EndLog();
    if (ERROR_SUCCESS != lRet)
        GENERIC_FAIL(RegFlushKey);

    //Delete keys
    for (k=0; k<PERF_TREE_ITER_COUNT; k++)
    {

        for(int i=0;i<PERF_FEW_KEYS;i++)
        {
            // Create the parent key. 
            StringCchPrintf(rgszDesc,1024,_T("%s-%d%d"), PERF_KEY, k,i);
            RegDeleteKey(hRoot, rgszDesc);
        }
    }

    fRetVal=TRUE;

ErrorReturn :
    CHECK_END_PERF(pCPerFlushKey);
    CHECK_FREE(pszPerfMarkerName);
    REG_CLOSE_KEY(hKeyParent);
    RegFlushKey(hRoot);
    return fRetVal;
}

///////////////////////////////////////////////////////////////////////////////
//  
// Function: Tst_FlushTree
//
// Purpose: Call the perf worker per ROOT. 
//            Measure the performance under HKEY_LOCAL_MACHINE 
//            and then measure it under HKEY_CURRENT_USER
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_FlushTree(UINT uMsg, TPPARAM /*tpParam*/, const LPFUNCTION_TABLE_ENTRY lpFTE ) 
{
    UNREFERENCED_PARAMETER(lpFTE);
    DWORD   dwRetVal=TPR_FAIL;

    COMMON_TUX_HEADER;

    TRACE(TEXT("Measuring FlushTree on HKLM ...\r\n"));
    if (!L_MeasureFlushTree_byRoot(HKEY_LOCAL_MACHINE, lpFTE->lpDescription))
        goto ErrorReturn;

    TRACE(TEXT("Measuring FlushTree on HKCU ...\r\n"));
    if (!L_MeasureFlushTree_byRoot(HKEY_CURRENT_USER, lpFTE->lpDescription))
        goto ErrorReturn;

    dwRetVal=TPR_PASS;

ErrorReturn :
    return dwRetVal;

}

BOOL PerfEnumWorkFunc(HKEY hParentKey, int uSubKeyCount)
{
    HKEY hSubKey = 0;
    int count = 0;
    TCHAR rgszKeyName[MAX_PATH] = {0};
    DWORD dwRetVal = 0;
    DWORD dwDispos = 0;
    DWORD dwSize = MAX_PATH;
    BOOL  fReturn = FALSE;
    DWORD dwData = 0;
    DWORD dwRegData = REG_DWORD;
    CPerfHelper *p_iValMarker=NULL;
    CPerfHelper *p_iKeyMarker=NULL;
    
    
    if(uSubKeyCount == 5)
    {
        p_iValMarker = g_pMARK_VALUE_ENUM_5;
        p_iKeyMarker = g_pMARK_KEY_ENUM_5;
    }
    else if(uSubKeyCount == 10)
    {
        p_iValMarker = g_pMARK_VALUE_ENUM_10;
        p_iKeyMarker = g_pMARK_KEY_ENUM_10;
    }
    else if(uSubKeyCount == 50)
    {
        p_iValMarker = g_pMARK_VALUE_ENUM_50;
        p_iKeyMarker = g_pMARK_KEY_ENUM_50;
    }
    else
    {
        p_iValMarker = g_pMARK_VALUE_ENUM_100;
        p_iKeyMarker = g_pMARK_KEY_ENUM_100;
    }
    
    // open a large number of subkeys
    for(int i = 0; i< uSubKeyCount;++i)
    {
        StringCchPrintf(&rgszKeyName[0],MAX_PATH,L"TestKey%d",i);
        dwRetVal = RegCreateKeyEx(hParentKey,rgszKeyName,0,NULL,0,0,NULL,&hSubKey,&dwDispos);
        if((dwRetVal != ERROR_SUCCESS) && (dwDispos != REG_CREATED_NEW_KEY))
        {
            REG_FAIL(RegCreateKeyEx,dwRetVal);
        }
        
        StringCchPrintf(&rgszKeyName[0],MAX_PATH,L"TestVal%d",i);
        dwSize = sizeof(DWORD);
        Hlp_GenRandomValData(&dwRegData,(PBYTE)&dwData,&dwSize);
        dwRetVal = RegSetValueEx(hParentKey,rgszKeyName,0,dwRegData,(PBYTE)&dwData,dwSize);
        if(dwRetVal != ERROR_SUCCESS)
        {
            REG_FAIL(RegSetValueEx,dwRetVal);
        }
        
        CloseHandle(hSubKey);
    }

    dwSize = MAX_PATH;
    // for loop to enumerate all keys
    while(dwRetVal != ERROR_NO_MORE_ITEMS)
    {
        // enumerate key with markers surrounding
        p_iKeyMarker->StartLog();
        dwRetVal = RegEnumKeyEx(hParentKey,count,rgszKeyName,&dwSize,NULL,NULL,NULL,NULL);
        p_iKeyMarker->EndLog();
        if((dwRetVal != ERROR_SUCCESS) && (dwRetVal != ERROR_NO_MORE_ITEMS))
        {
            REG_FAIL(RegEnumKeyEx,dwRetVal);
        }
        dwSize = MAX_PATH; // reset size so RegEnumValue doesn't fail.
        
        p_iValMarker->StartLog();
        dwRetVal = RegEnumValue(hParentKey,count,rgszKeyName,&dwSize,NULL,&dwRegData,NULL,NULL);
        p_iValMarker->EndLog();
        if((dwRetVal != ERROR_SUCCESS) && (dwRetVal != ERROR_NO_MORE_ITEMS))
        {
            REG_FAIL(RegEnumValue,dwRetVal);
        }
        
        dwSize = MAX_PATH; // reset size so RegEnumKeyEx doesn't fail.
        ++count;
    }
    
    for(int i = 0; i < uSubKeyCount; ++i)
    {
        StringCchPrintf(&rgszKeyName[0],MAX_PATH,L"TestKey%d",i);
        dwRetVal = RegDeleteKey(hParentKey,rgszKeyName);
        if(dwRetVal != ERROR_SUCCESS)
        {
            REG_FAIL(RegDeleteKey,dwRetVal);
        }
        StringCchPrintf(&rgszKeyName[0],MAX_PATH,L"TestVal%d",i);
        dwRetVal = RegDeleteValue(hParentKey,rgszKeyName);
        if(dwRetVal != ERROR_SUCCESS)
        {
            REG_FAIL(RegDeleteKey,dwRetVal);
        }
        
    }


    fReturn = TRUE;

ErrorReturn:
    if(fReturn)
        TRACE(TEXT("PASS!"));
    else
        TRACE(TEXT("FAIL!"));

    return fReturn;
}

TESTPROCAPI Tst_RegEnumPerf(UINT uMsg, TPPARAM /*tpParam*/, const LPFUNCTION_TABLE_ENTRY /*lpFTE*/ ) 
{
    HKEY hParentKey;
    TCHAR rgszKeyName[MAX_PATH] = {0};
    DWORD dwRetVal = 0;
    DWORD dwDispos = 0;
    BOOL  fPass = TRUE;

    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }
    
    LPTSTR pszPerfMarkerName = NULL;
    // Allocate string buffers for perf parameters
    pszPerfMarkerName = (TCHAR*) malloc(sizeof(TCHAR) * CEPERF_MAX_ITEM_NAME_LEN);
    if ((NULL == pszPerfMarkerName))
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }
    
    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,_T("EK-5 keys"));
    g_pMARK_KEY_ENUM_5 = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == g_pMARK_KEY_ENUM_5)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,_T("EV-5 values"));
    g_pMARK_VALUE_ENUM_5 = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == g_pMARK_VALUE_ENUM_5)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }
    
    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,_T("EK-10 keys"));
    g_pMARK_KEY_ENUM_10 = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == g_pMARK_KEY_ENUM_10)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,_T("EV-10 values"));
    g_pMARK_VALUE_ENUM_10 = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == g_pMARK_VALUE_ENUM_10)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }
    
    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,_T("EK-50 keys"));
    g_pMARK_KEY_ENUM_50 = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == g_pMARK_KEY_ENUM_50)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }
    
    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,_T("EV-50 values"));
    g_pMARK_VALUE_ENUM_50 = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == g_pMARK_VALUE_ENUM_50)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }
    
    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,_T("EK-100 keys"));
    g_pMARK_KEY_ENUM_100 = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == g_pMARK_KEY_ENUM_100)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }
    
    CreateMarkerName(pszPerfMarkerName,CEPERF_MAX_ITEM_NAME_LEN,_T("EV-100 values"));
    g_pMARK_VALUE_ENUM_100 = MakeNewPerflogger(pszPerfMarkerName, NULL, PERF_DATA_DURATION, 0, 0);
    if (NULL == g_pMARK_VALUE_ENUM_100)
    {
        ERRFAIL("Out of memory!");
        goto ErrorReturn;
    }

    StringCchPrintf(&rgszKeyName[0],MAX_PATH,L"Start");
    
    dwRetVal = RegCreateKeyEx(HKEY_CURRENT_USER,rgszKeyName,0,NULL,0,0,NULL,&hParentKey,&dwDispos);
    if((dwRetVal != ERROR_SUCCESS) || (dwDispos != REG_OPENED_EXISTING_KEY))
    {
        REG_FAIL(RegCreateKeyEx,dwRetVal);
    }

    TRACE(TEXT("Testing Perf of RegEnumKeyEx & RegEnumValue with 5 keys: "));
    fPass &= PerfEnumWorkFunc(hParentKey,5);


    TRACE(TEXT("Testing Perf of RegEnumKeyEx & RegEnumValue with 10 keys: "));
    fPass &= PerfEnumWorkFunc(hParentKey,10);


    TRACE(TEXT("Testing Perf of RegEnumKeyEx & RegEnumValue with 50 keys: "));
    fPass &= PerfEnumWorkFunc(hParentKey,50);

    TRACE(TEXT("Testing Perf of RegEnumKeyEx & RegEnumValue with 100 keys: "));
    fPass &= PerfEnumWorkFunc(hParentKey,100);

    CloseHandle(hParentKey);    

ErrorReturn:
    CHECK_END_PERF(g_pMARK_KEY_ENUM_5);
    CHECK_END_PERF(g_pMARK_VALUE_ENUM_5);
    CHECK_END_PERF(g_pMARK_KEY_ENUM_10);
    CHECK_END_PERF(g_pMARK_VALUE_ENUM_10);
    CHECK_END_PERF(g_pMARK_KEY_ENUM_50);
    CHECK_END_PERF(g_pMARK_VALUE_ENUM_50);
    CHECK_END_PERF(g_pMARK_KEY_ENUM_100);
    CHECK_END_PERF(g_pMARK_VALUE_ENUM_100);
    CHECK_FREE(pszPerfMarkerName);

    if(!fPass)
        return TPR_FAIL;
    else
        return TPR_PASS;
}

///////////////////////////////////////////////////////////////////////////////
//  
// Function: initRegValues
//
// Parameters: UNIT nNumberOfValues, UINT nSizeOfValues
//
// Purpose: Deletes test keys from previous tests
//          Creates 'nNumberOfValues' binary values of size 'nSizeOfValues'
//          Binary value is randomly generated.  Value names corresponds to 
//          'g_szarrRegValues'
//          Then calls RegQueryValueEx on each key so it's loaded into the in-proc cache
//          
//
///////////////////////////////////////////////////////////////////////////////
BOOL initRegValues(UINT nNumberOfValues, UINT nSizeOfValues)
{
    BOOL bRetVal = TRUE;
    HKEY hKey;
    DWORD dwValueSize = nSizeOfValues;
    DWORD dwDisposition;
    BYTE * pBuffer = NULL;
    LONG lResult = ERROR_SUCCESS;
    
    pBuffer = (BYTE *)LocalAlloc(LHND, dwValueSize + 10);
    if(pBuffer == NULL){
        g_pKato->Log(LOG_FAIL, TEXT("Failed to init pBuffer in initReg!"));
        return FALSE;
    }

    //recreate test node
    lResult = RegCreateKeyEx(HKEY_LOCAL_MACHINE, MULTITHREAD_REG_TEST_PATH, 0, L"REG_BINARY", 0, 0, NULL, &hKey, &dwDisposition);
    if(lResult != ERROR_SUCCESS){
        bRetVal = FALSE;
        goto done;
    }

    //populate with nNumberOfValues
    for(UINT i = 0; i < nNumberOfValues; i++){
        //fill buffer with values
        HKEY hSubKey = NULL;
        for(UINT index = 0; index < (dwValueSize); index++){
            pBuffer[i] = (i+1) % 8;
        }
        lResult = RegCreateKeyEx(hKey, g_szarrRegKeys[i], 0, L"REG_BINARY", 0, 0, NULL, &hSubKey, &dwDisposition);
        if(lResult != ERROR_SUCCESS){
            bRetVal = FALSE;
            goto done;
        }
        //add data as value
        lResult = RegSetValueEx(hSubKey, g_szarrRegValue, 0, REG_BINARY, pBuffer, dwValueSize);
        if(lResult != ERROR_SUCCESS){
            bRetVal = FALSE;
            goto done;
        }
        RegCloseKey(hSubKey);
    }

done:
    LocalFree(pBuffer);
    RegCloseKey(hKey);
    Sleep(100);
    return bRetVal;
}

///////////////////////////////////////////////////////////////////////////////
//  
// Function: initRegKeys
//
// Parameters: UNIT nNumberOfKeys
//
// Purpose: Deletes test keys from previous tests
//          Then creates 'nNumberOfKeys' empty keys.  Key names pulled from 'g_szarrRegKeys'
//          Then calls RegOpenKeyEx on each key so it's loaded into the in-proc cache
//          
//
///////////////////////////////////////////////////////////////////////////////
BOOL initRegKeys(UINT nNumberOfKeys)
{
    BOOL bRetVal = TRUE;
    HKEY hKey=NULL;
    DWORD dwDisposition;
    LONG lResult = ERROR_SUCCESS;

    
    //delete all test keys to insure consistent enumeration time
    lResult = RegDeleteKey(HKEY_LOCAL_MACHINE, MULTITHREAD_REG_TEST_PATH);
    if(lResult != ERROR_SUCCESS && lResult != ERROR_FILE_NOT_FOUND){
        bRetVal = FALSE;
        goto done;
    }
    
    //recreate test node
    RegCreateKeyEx(HKEY_LOCAL_MACHINE, MULTITHREAD_REG_TEST_PATH, 0, L"REG_BINARY", 0, 0, NULL, &hKey, &dwDisposition);
    if(lResult != ERROR_SUCCESS){
        bRetVal = FALSE;
        goto done;
    }

    //populate with nNumberOfKeys subkeys.  Keys will be cached automatically
    for(UINT i = 0; i < nNumberOfKeys; i++){
        HKEY hSubKey;
        lResult = RegCreateKeyEx(hKey, g_szarrRegKeys[i], 0, L"REG_BINARY", 0, 0, NULL, &hSubKey, &dwDisposition);
        if(lResult != ERROR_SUCCESS){
            bRetVal = FALSE;
            goto done;
        }
        else{
            RegCloseKey(hSubKey);
        }
    }

done:
    Sleep(1000);
    RegCloseKey(hKey);
    return bRetVal;
}

///////////////////////////////////////////////////////////////////////////////
//  
// Function: RegQueryValueExThread
//
// Parameters: PVOID pThreadParams
//              {nIterations, nPerfMarker, nTestRange, nSizeOfValues}
// Purpose: This function is intended to be called as a thread
//          It Queries 'nTestRange' different values from the registry 'nIterations' times
//          Uses 'nPerfMarker' to measure the time spent in each call to RegQueryValueEx
//
//          Size is the only verification of the value returned by RegQueryValueEx 
//          
//
///////////////////////////////////////////////////////////////////////////////
DWORD RegQueryValueExThread(PVOID pThreadParams)
{
    BYTE * pBuff = NULL;
    BOOL dwRetVal = ERROR_SUCCESS;
    DWORD dwDisposition;
    DWORD dwType = 0;
    DWORD dwSizeInBytes = 0;
    HKEY hKey;
    HKEY hSubKey;
    LONG lResult = ERROR_SUCCESS;

    dwSizeInBytes = (((THREAD_PARAMS*)pThreadParams)->nSizeOfValues );
    pBuff = (BYTE*)malloc(dwSizeInBytes);
    if(pBuff == NULL){
        g_pKato->Log(LOG_FAIL, L"Thread%d failed to allocated buffer", ((THREAD_PARAMS*)pThreadParams)->nThreadIndex);
        dwRetVal = 1;
        return 1;
    }
    
    lResult = RegCreateKeyEx(HKEY_LOCAL_MACHINE, MULTITHREAD_REG_TEST_PATH, 0, L"REG_BINARY", 0, 0, NULL, &hKey, &dwDisposition);
    if(lResult != ERROR_SUCCESS){
        g_pKato->Log(LOG_FAIL, L"Thread%d got unexpected return when opening handle to test keys", ((THREAD_PARAMS*)pThreadParams)->nThreadIndex);
        dwRetVal = 1;
        goto done;
    }
    

    g_pKato->Log(LOG_DETAIL, L"Thread%d about to query", ((THREAD_PARAMS*)pThreadParams)->nThreadIndex);
    Perf_MarkBegin(((THREAD_PARAMS*)pThreadParams)->nPerfMarker );

    for(UINT i = 0; i < ((THREAD_PARAMS*)pThreadParams)->nIterations; i++){
        lResult = RegCreateKeyEx(hKey, g_szarrRegKeys[i%((THREAD_PARAMS*)pThreadParams)->nTestRange], 0, L"REG_BINARY", 0, 0, NULL, &hSubKey, &dwDisposition);
        if(lResult != ERROR_SUCCESS){
            g_pKato->Log(LOG_FAIL, L"Thread%d got unexpected return when opening handle to sub key", ((THREAD_PARAMS*)pThreadParams)->nThreadIndex);
            dwRetVal =1;
        }
        lResult = RegQueryValueEx(hSubKey, g_szarrRegValue, NULL, &dwType, pBuff, &dwSizeInBytes);
        if(lResult != ERROR_SUCCESS){
            g_pKato->Log(LOG_FAIL, L"Thread%d got unexpected return from RegQueryValueEx", ((THREAD_PARAMS*)pThreadParams)->nThreadIndex);
            dwRetVal = 1;
        }
        RegCloseKey(hSubKey);
    }
    Perf_MarkEnd(((THREAD_PARAMS*)pThreadParams)->nPerfMarker );
done:
    RegCloseKey(hKey);
    free(pBuff);
    return dwRetVal;
}

///////////////////////////////////////////////////////////////////////////////
//  
// Function: RegQueryValueExThread_hacky
//
// Parameters: PVOID pThreadParams
//              {nIterations, nPerfMarker, nTestRange, nSizeOfValues}
// Purpose: This function is intended to be called as a thread
//          It Queries 'nTestRange' different values from the registry 'nIterations' times
//          Uses 'nPerfMarker' to measure the time spent in each call to RegQueryValueEx
//          'dwReserved' "trick" is used when querying values.  The sub key
//          name is passed using the 'dwReserved' parameter, which causes
//          the key to be opened and closed for each query.  This is expected to show 
//          significant speed up with the in-proc cache.
//
//          Size is the only verification of the value returned by RegQueryValueEx 
//          
//
///////////////////////////////////////////////////////////////////////////////
DWORD RegQueryValueExThread_hacky(PVOID pThreadParams)
{
    BYTE * pBuff = NULL;
    BOOL dwRetVal = ERROR_SUCCESS;
    DWORD dwType = 0;
    DWORD dwSizeInBytes = 0;
    LONG lResult = ERROR_SUCCESS;

    dwSizeInBytes = (((THREAD_PARAMS*)pThreadParams)->nSizeOfValues );
    pBuff = (BYTE*)malloc(dwSizeInBytes);
    if(pBuff == NULL){
        g_pKato->Log(LOG_FAIL, L"Thread%d failed to allocated buffer", ((THREAD_PARAMS*)pThreadParams)->nThreadIndex);
        dwRetVal = 1;
        return 1;
    }
    
    g_pKato->Log(LOG_DETAIL, L"Thread%d about to query", ((THREAD_PARAMS*)pThreadParams)->nThreadIndex);
    Perf_MarkBegin(((THREAD_PARAMS*)pThreadParams)->nPerfMarker );
    for(UINT i = 0; i < ((THREAD_PARAMS*)pThreadParams)->nIterations; i++){
        
        lResult = RegQueryValueEx(
                                    HKEY_LOCAL_MACHINE,
                                    g_szarrRegValue, 
                                    (LPDWORD)g_szarrRegKeys_FullPath[i % ((THREAD_PARAMS*)pThreadParams)->nTestRange], 
                                    &dwType, 
                                    pBuff, 
                                    &dwSizeInBytes
                                  );
                
        if(lResult != ERROR_SUCCESS){
            g_pKato->Log(LOG_FAIL, L"Thread%d got unexpected return from RegQueryValueEx", ((THREAD_PARAMS*)pThreadParams)->nThreadIndex);
            dwRetVal = 1;
        }
    }
    Perf_MarkEnd(((THREAD_PARAMS*)pThreadParams)->nPerfMarker );

    free(pBuff);
    return dwRetVal;
}

///////////////////////////////////////////////////////////////////////////////
//  
// Function: RegOpenKeyExThread
//
// Parameters: PVOID pThreadParams
//              {nIterations, nPerfMarker, nTestRange}
// Purpose: This function is intended to be called as a thread
//          It opens handles to 'nTestRange' different keys from the registry 'nIterations' times
//          Uses 'nPerfMarker' to measure the time spent in each call to RegQueryValueEx
//
//          No verification of key is performed
//
///////////////////////////////////////////////////////////////////////////////
DWORD RegOpenKeyExThread(PVOID pThreadParams)
{
    BOOL dwRetVal = ERROR_SUCCESS;
    DWORD dwDisposition;
    HKEY hKey = NULL;
    LONG lResult = RegCreateKeyEx(HKEY_LOCAL_MACHINE, MULTITHREAD_REG_TEST_PATH, 0, L"REG_BINARY", 0, 0, NULL, &hKey, &dwDisposition);
    if(lResult != ERROR_SUCCESS){
        g_pKato->Log(LOG_FAIL, L"Thread%d failed to open handle to test key, error: %d", ((THREAD_PARAMS*)pThreadParams)->nThreadIndex, lResult);
        dwRetVal = 1;
        return dwRetVal;
    }
    g_pKato->Log(LOG_DETAIL, L"Thread%d about to open %d keys, %d iterations", ((THREAD_PARAMS*)pThreadParams)->nThreadIndex, ((THREAD_PARAMS*)pThreadParams)->nTestRange, ((THREAD_PARAMS*)pThreadParams)->nIterations);
    Perf_MarkBegin(((THREAD_PARAMS*)pThreadParams)->nPerfMarker );

    for(UINT i = 0; i < ((THREAD_PARAMS*)pThreadParams)->nIterations; i++){
        HKEY hSubKey;
        
        lResult = RegOpenKeyEx(hKey, g_szarrRegKeys[i % ((THREAD_PARAMS*)pThreadParams)->nTestRange], 0, 0, &hSubKey);
        
        if(lResult != ERROR_SUCCESS){
            g_pKato->Log(LOG_FAIL, L"Thread%d failed to Open Key, error: %d", ((THREAD_PARAMS*)pThreadParams)->nThreadIndex, lResult);
            dwRetVal = 1;
            goto done;
        }
        RegCloseKey(hSubKey);
    }
    Perf_MarkEnd(((THREAD_PARAMS*)pThreadParams)->nPerfMarker );

done:
    RegCloseKey(hKey);
    return dwRetVal;
}

///////////////////////////////////////////////////////////////////////////////
//  
// Function: L_MultiThreadRegQueryValueEx
//
// Parameters: int nNumberOfThreads, int nIterations, int nNumberOfValues, 
//             int nSizeOfValues, UINT nPerfMarker, BOOL bHacky
//
// Purpose: Spawns 'nNumberOfThreads' threads which query 'nNUmberOfValues' ('nIterations'/'nNumberOfThreads') times.
//          Values are of size 'nSizeOfValues'
//          Time spent in function will be recorded against 'nPerfMarker'
//          If 'bHacky' is TRUE, 'RegQueryValueExThread_bHacky' is called to spawn threads.  
//          See description of that function for more details.
//
//          Any non-zero return indicates a failure
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_MultiThreadRegQueryValueEx(int nNumberOfThreads, int nIterations, int nNumberOfValues, int nSizeOfValues, UINT nPerfMarker, BOOL bHacky)
{
    DWORD bRetVal = TRUE;
    DWORD dwExitCode;
    DWORD dwSleepTime = 0;
    HANDLE phThreads[MAX_REGTEST_THREADS ];
    THREAD_PARAMS pThreadParams[MAX_REGTEST_THREADS ];
    if(nNumberOfThreads >MAX_REGTEST_THREADS)
        nNumberOfThreads = MAX_REGTEST_THREADS;
    for(int i = 0; i < nNumberOfThreads; i++)
    {
        pThreadParams[i].nTestRange = nNumberOfValues;
        pThreadParams[i].nIterations = nIterations;
        pThreadParams[i].nSizeOfValues = nSizeOfValues;
        pThreadParams[i].nThreadIndex = i;
        pThreadParams[i].nPerfMarker = nPerfMarker;
    }

    //load the values into the registry, this function also queries the values so 
    //they are loaded into the cache
    initRegValues(nNumberOfValues, nSizeOfValues);
    
    Perf_StartSysMonitor(PERF_APP_NAME, SYS_MON_CPU | SYS_MON_MEM | SYS_MON_LOG, 
                            g_dwPoolIntervalMs, g_dwCPUCalibrationMs);

    //spawn nNumberOfThreads with a pointer to the array of keys
    if(bHacky){
        for(int i = 0; i < nNumberOfThreads; i++)
        {
            DWORD dwThreadId;
            phThreads[i] = CreateThread(NULL, 0,&RegQueryValueExThread_hacky, (PVOID)&pThreadParams[i], 0, &dwThreadId);
        }
    }
    else{
        for(int i = 0; i < nNumberOfThreads; i++)
        {
            DWORD dwThreadId;
            phThreads[i] = CreateThread(NULL, 0,&RegQueryValueExThread, (PVOID)&pThreadParams[i], 0, &dwThreadId);
        }
    }

        TRACE(TEXT("Sleeping before getting thread exit code\r\n"));
    Sleep(5000);
    dwSleepTime += 5000;
    for(int i = 0; i < nNumberOfThreads; )
    {
        GetExitCodeThread(phThreads[i], &dwExitCode);
        if(dwExitCode == 0){
            i++;
            continue;
        }
        else if(dwExitCode == 1){
            TRACE(TEXT("Thread %d encountered a failure!\r\n"), i);
            i++;
            bRetVal = FALSE;
        }
        //If dwExitCode != 0 or 1, the thread has not yet returned
        else if(dwSleepTime > MT_MAX_SLEEP_TIME){
            TRACE(TEXT("Max wait time reached for thread %d, failure\r\n"), i);
            i++;
            bRetVal = FALSE;
        }
        else{ //don't increment i, try again after sleeping
            TRACE(TEXT("Thread %d still running, sleep and recheck\r\n"), i);
            Sleep(5000);
            dwSleepTime += 5000;
        }
    }
    Perf_StopSysMonitor();        
    //return any error
    return bRetVal;
}

///////////////////////////////////////////////////////////////////////////////
//  
// Function: L_MultiThreadRegOpenKeyEx
//
// Parameters: int nNumberOfThreads, int nIterations, int nNumberOfKeys, 
//             UINT nPerfMarker
//
// Purpose: Spawns 'nNumberOfThreads' threads which 
//          open 'nNumberOfKeys' ('nIterations'/'nNumberOfThreads') times.
//          Time spent in function will be recorded against 'nPerfMarker'
//
//          Any non-zero return indicates a failure
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_MultiThreadRegOpenKeyEx(int nNumberOfThreads, int nIterations, int nNumberOfKeys, UINT nPerfMarker)
{

    DWORD bRetVal = TRUE;
    DWORD dwExitCode;
    DWORD dwSleepTime = 0;
    HANDLE phThreads[MAX_REGTEST_THREADS ];
    THREAD_PARAMS pThreadParams[MAX_REGTEST_THREADS ];
    if(nNumberOfThreads> MAX_REGTEST_THREADS)
        nNumberOfThreads = MAX_REGTEST_THREADS;
    for(int i = 0; i < nNumberOfThreads; i++)
    {
        pThreadParams[i].nTestRange = nNumberOfKeys;
        pThreadParams[i].nIterations = nIterations;
        pThreadParams[i].nSizeOfValues = 0xBA5EBA11;
        pThreadParams[i].nThreadIndex = i;
        pThreadParams[i].nPerfMarker = nPerfMarker;
    }

    //load the values into the registry, this function also queries the values so 
    //they are loaded into the cache
    bRetVal = initRegKeys(nNumberOfKeys);
    if(!bRetVal){
        TRACE(TEXT("initRegKeys encountered a failure, failing!"));
        return bRetVal;
    }
    
    Perf_StartSysMonitor(PERF_APP_NAME, SYS_MON_CPU | SYS_MON_MEM | SYS_MON_LOG, 
                            g_dwPoolIntervalMs, g_dwCPUCalibrationMs);

    //spawn nNumberOfThreads with a pointer to the array of keys
    for(int i = 0; i < nNumberOfThreads; i++)
    {
        DWORD dwThreadId;
        phThreads[i] = CreateThread(NULL, 0,&RegOpenKeyExThread, (PVOID)&pThreadParams[i], 0, &dwThreadId);
    }

    TRACE(TEXT("Sleeping before getting thread exit code\r\n"));
    Sleep(5000);
    dwSleepTime += 5000;
    for(int i = 0; i < nNumberOfThreads; )
    {
        GetExitCodeThread(phThreads[i], &dwExitCode);
        if(dwExitCode == 0){
            i++;
            continue;
        }
        else if(dwExitCode == 1){
            TRACE(TEXT("Thread %d encountered a failure!\r\n"), i);
            i++;
            bRetVal = FALSE;
        }
        //If dwExitCode != 0 or 1, the thread has not yet returned
        else if(dwSleepTime > MT_MAX_SLEEP_TIME){
            TRACE(TEXT("Max wait time reached for thread %d, failure\r\n"), i);
            i++;
            bRetVal = FALSE;
        }
        else{ //don't increment i, try again after sleeping
            TRACE(TEXT("Thread %d still running, sleep and recheck\r\n"), i);
            Sleep(5000);
            dwSleepTime += 5000;
        }
    }
    //return any error
    Perf_StopSysMonitor();        
    return bRetVal;
}

///////////////////////////////////////////////////////////////////////////////
//  
// Function: Tst_MultiThreadRegQueryValueEx
//
// Parameters: Default function table params, including-
//             lpFTE->dwUniqueID 
//
// Purpose: Uses dwUniqueID to determine appropraite perf marker.  Registers it.
//          Uses dwUniqueID to determine appropraite parameters for 
//          L_MultiThreadRegQueryValue
//
//          Returns TPR_PASS if no failures encountered
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_MultiThreadRegQueryValueEx(UINT uMsg, TPPARAM /*tpParam*/, const LPFUNCTION_TABLE_ENTRY lpFTE ) 
{
    UNREFERENCED_PARAMETER(lpFTE);
    DWORD   dwRetVal=TPR_FAIL;

    COMMON_TUX_HEADER;
    
    TRACE(TEXT("Measuring RegQueryValueEx \r\n"));
    switch(lpFTE->dwUniqueID){
        case 2010: //MARK_QUERY_5_KEYS_1_THREAD
            if (!Perf_RegisterMark(MARK_QUERY_5_KEYS_1_THREAD , _T("Test=RegQueryValueEx - MARK_QUERY_5_KEYS_1_THREAD - Test#%d, time=%%AVG%%"), lpFTE->dwUniqueID)) 
                GENERIC_FAIL(Perf_RegisterMark);

            if (!L_MultiThreadRegQueryValueEx(1, 1000, 5, 16, MARK_QUERY_5_KEYS_1_THREAD, FALSE))
                goto ErrorReturn;
            break;
        case 2011: //MARK_QUERY_5_KEYS_8_THREADS
            if (!Perf_RegisterMark(MARK_QUERY_5_KEYS_8_THREADS , _T("Test=RegQueryValueEx - MARK_QUERY_5_KEYS_8_THREADS - Test#%d, time=%%AVG%%"), lpFTE->dwUniqueID)) 
                GENERIC_FAIL(Perf_RegisterMark);

            if (!L_MultiThreadRegQueryValueEx(8, 1000, 5, 16, MARK_QUERY_5_KEYS_8_THREADS, FALSE))
                goto ErrorReturn;
            break;
        case 2012: //MARK_QUERY_MAX_KEYS_1_THREAD
            if (!Perf_RegisterMark(MARK_QUERY_MAX_KEYS_1_THREAD , _T("Test=RegQueryValueEx - MARK_QUERY_MAX_KEYS_1_THREAD - Test#%d, time=%%AVG%%"), lpFTE->dwUniqueID)) 
                GENERIC_FAIL(Perf_RegisterMark);

            if (!L_MultiThreadRegQueryValueEx(1, 1000, g_dwRegCache_MAXKEYS, 16, MARK_QUERY_MAX_KEYS_1_THREAD, FALSE))
                goto ErrorReturn;
            break;
        case 2013: //MARK_QUERY_MAX_KEYS_8_THREADS
            if (!Perf_RegisterMark(MARK_QUERY_MAX_KEYS_8_THREADS , _T("Test=RegQueryValueEx - MARK_QUERY_MAX_KEYS_8_THREADS - Test#%d, time=%%AVG%%"), lpFTE->dwUniqueID)) 
                GENERIC_FAIL(Perf_RegisterMark);

            if (!L_MultiThreadRegQueryValueEx(8, 1000, g_dwRegCache_MAXKEYS, 16, MARK_QUERY_MAX_KEYS_8_THREADS, FALSE))
                goto ErrorReturn;
            break;
        case 2014: //MARK_QUERY_OVERMAX_KEYS_1_THREAD
            if (!Perf_RegisterMark(MARK_QUERY_OVERMAX_KEYS_1_THREAD , _T("Test=RegQueryValueEx - MARK_QUERY_OVERMAX_KEYS_1_THREAD - Test#%d, time=%%AVG%%"), lpFTE->dwUniqueID)) 
                GENERIC_FAIL(Perf_RegisterMark);

            if (!L_MultiThreadRegQueryValueEx(1, 1000, g_dwRegCache_MAXKEYS+5, 16, MARK_QUERY_OVERMAX_KEYS_1_THREAD, FALSE))
                goto ErrorReturn;
            break;
        case 2015: //MARK_QUERY_OVERMAX_KEYS_8_THREAD
            if (!Perf_RegisterMark(MARK_QUERY_OVERMAX_KEYS_8_THREAD , _T("Test=RegQueryValueEx - MARK_QUERY_OVERMAX_KEYS_8_THREAD - Test#%d, time=%%AVG%%"), lpFTE->dwUniqueID)) 
                GENERIC_FAIL(Perf_RegisterMark);

            if (!L_MultiThreadRegQueryValueEx(8, 1000, g_dwRegCache_MAXKEYS+5, 16, MARK_QUERY_OVERMAX_KEYS_8_THREAD, FALSE))
                goto ErrorReturn;
            break;

            case 2020: //MARK_QUERY_5_KEYS_HACKY_1_THREAD
            if (!Perf_RegisterMark(MARK_QUERY_5_KEYS_HACKY_1_THREAD , _T("Test=RegQueryValueEx - MARK_QUERY_5_KEYS_HACKY_1_THREAD - Test#%d, time=%%AVG%%"), lpFTE->dwUniqueID)) 
                GENERIC_FAIL(Perf_RegisterMark);

            if (!L_MultiThreadRegQueryValueEx(1, 1000, 5, 16, MARK_QUERY_5_KEYS_HACKY_1_THREAD, TRUE))
                goto ErrorReturn;
            break;
        case 2021: //MARK_QUERY_5_KEYS_HACKY_8_THREADS
            if (!Perf_RegisterMark(MARK_QUERY_5_KEYS_HACKY_8_THREADS , _T("Test=RegQueryValueEx - MARK_QUERY_5_KEYS_HACKY_8_THREADS - Test#%d, time=%%AVG%%"), lpFTE->dwUniqueID)) 
                GENERIC_FAIL(Perf_RegisterMark);

            if (!L_MultiThreadRegQueryValueEx(8, 1000, 5, 16, MARK_QUERY_5_KEYS_HACKY_8_THREADS, TRUE))
                goto ErrorReturn;
            break;
        case 2022: //MARK_QUERY_MAX_KEYS_HACKY_1_THREAD
            if (!Perf_RegisterMark(MARK_QUERY_MAX_KEYS_HACKY_1_THREAD , _T("Test=RegQueryValueEx - MARK_QUERY_MAX_KEYS_HACKY_1_THREAD - Test#%d, time=%%AVG%%"), lpFTE->dwUniqueID)) 
                GENERIC_FAIL(Perf_RegisterMark);

            if (!L_MultiThreadRegQueryValueEx(1, 1000, g_dwRegCache_MAXKEYS, 16, MARK_QUERY_MAX_KEYS_HACKY_1_THREAD, TRUE))
                goto ErrorReturn;
            break;
        case 2023: //MARK_QUERY_MAX_KEYS_HACKY_8_THREADS
            if (!Perf_RegisterMark(MARK_QUERY_MAX_KEYS_HACKY_8_THREADS , _T("Test=RegQueryValueEx - MARK_QUERY_MAX_KEYS_HACKY_8_THREADS - Test#%d, time=%%AVG%%"), lpFTE->dwUniqueID)) 
                GENERIC_FAIL(Perf_RegisterMark);

            if (!L_MultiThreadRegQueryValueEx(8, 1000, g_dwRegCache_MAXKEYS, 16, MARK_QUERY_MAX_KEYS_HACKY_8_THREADS, TRUE))
                goto ErrorReturn;
            break;
        case 2024: //MARK_QUERY_OVERMAX_KEYS_HACKY_1_THREAD
            if (!Perf_RegisterMark(MARK_QUERY_OVERMAX_KEYS_HACKY_1_THREAD , _T("Test=RegQueryValueEx - MARK_QUERY_OVERMAX_KEYS_HACKY_1_THREAD - Test#%d, time=%%AVG%%"), lpFTE->dwUniqueID)) 
                GENERIC_FAIL(Perf_RegisterMark);

            if (!L_MultiThreadRegQueryValueEx(1, 1000, g_dwRegCache_MAXKEYS + 5, 16, MARK_QUERY_OVERMAX_KEYS_HACKY_1_THREAD, TRUE))
                goto ErrorReturn;
            break;
        case 2025: //MARK_QUERY_OVERMAX_KEYS_HACKY_8_THREAD
            if (!Perf_RegisterMark(MARK_QUERY_OVERMAX_KEYS_HACKY_8_THREAD , _T("Test=RegQueryValueEx - MARK_QUERY_OVERMAX_KEYS_HACKY_8_THREAD - Test#%d, time=%%AVG%%"), lpFTE->dwUniqueID)) 
                GENERIC_FAIL(Perf_RegisterMark);

            if (!L_MultiThreadRegQueryValueEx(8, 1000, g_dwRegCache_MAXKEYS + 5, 16, MARK_QUERY_OVERMAX_KEYS_HACKY_8_THREAD, TRUE))
                goto ErrorReturn;
            break;

        default:
            TRACE(TEXT("Unrecognized test number, don't know what values to use\r\n"));
            goto ErrorReturn;
    }

    dwRetVal=TPR_PASS;

ErrorReturn :
    return dwRetVal;

}

///////////////////////////////////////////////////////////////////////////////
//  
// Function: Tst_MultiThreadRegOpenKeyEx
//
// Parameters: Default function table params, including-
//             lpFTE->dwUniqueID 
//
// Purpose: Uses dwUniqueID to determine appropraite perf marker.  Registers it.
//          Uses dwUniqueID to determine appropraite parameters for 
//          L_MultiThreadRegOpenKey
//
//          Returns TPR_PASS if no failures encountered
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_MultiThreadRegOpenKeyEx(UINT uMsg, TPPARAM /*tpParam*/, const LPFUNCTION_TABLE_ENTRY lpFTE ) 
{
    UNREFERENCED_PARAMETER(lpFTE);
    DWORD   dwRetVal=TPR_FAIL;

    COMMON_TUX_HEADER;
    
    TRACE(TEXT("Measuring RegOpenKeyEx \r\n"));
    switch(lpFTE->dwUniqueID){
        case 2000: //MARK_OPEN_5_KEYS_1_THREAD
            if (!Perf_RegisterMark(MARK_OPEN_5_KEYS_1_THREAD , _T("Test=RegOpenKeyEx - MARK_OPEN_5_KEYS_1_THREAD - Test#%d, time=%%AVG%%"), lpFTE->dwUniqueID)) 
                GENERIC_FAIL(Perf_RegisterMark);

            if (!L_MultiThreadRegOpenKeyEx(1, 1000, 5, MARK_OPEN_5_KEYS_1_THREAD))
                goto ErrorReturn;
            break;
        case 2001: //MARK_OPEN_5_KEYS_8_THREADS
            if (!Perf_RegisterMark(MARK_OPEN_5_KEYS_8_THREADS , _T("Test=RegOpenKeyEx - MARK_OPEN_5_KEYS_8_THREADS - Test#%d, time=%%AVG%%"), lpFTE->dwUniqueID)) 
                GENERIC_FAIL(Perf_RegisterMark);

            if (!L_MultiThreadRegOpenKeyEx(8, 1000, 5, MARK_OPEN_5_KEYS_8_THREADS))
                goto ErrorReturn;
            break;
        case 2002: //MARK_OPEN_MAX_KEYS_1_THREAD
            if (!Perf_RegisterMark(MARK_OPEN_MAX_KEYS_1_THREAD , _T("Test=RegOpenKeyEx - MARK_OPEN_MAX_KEYS_1_THREAD - Test#%d, time=%%AVG%%"), lpFTE->dwUniqueID)) 
                GENERIC_FAIL(Perf_RegisterMark);

            if (!L_MultiThreadRegOpenKeyEx(1, 1000, g_dwRegCache_MAXKEYS, MARK_OPEN_MAX_KEYS_1_THREAD))
                goto ErrorReturn;
            break;
        case 2003: //MARK_OPEN_MAX_KEYS_8_THREADS
            if (!Perf_RegisterMark(MARK_OPEN_MAX_KEYS_8_THREADS , _T("Test=RegOpenKeyEx - MARK_OPEN_MAX_KEYS_8_THREADS - Test#%d, time=%%AVG%%"), lpFTE->dwUniqueID)) 
                GENERIC_FAIL(Perf_RegisterMark);

            if (!L_MultiThreadRegOpenKeyEx(8, 1000, g_dwRegCache_MAXKEYS, MARK_OPEN_MAX_KEYS_8_THREADS))
                goto ErrorReturn;
            break;
        case 2004: //MARK_OPEN_OVERMAX_KEYS_1_THREAD
            if (!Perf_RegisterMark(MARK_OPEN_OVERMAX_KEYS_1_THREAD , _T("Test=RegOpenKeyEx - MARK_OPEN_OVERMAX_KEYS_1_THREAD - Test#%d, time=%%AVG%%"), lpFTE->dwUniqueID)) 
                GENERIC_FAIL(Perf_RegisterMark);

            if (!L_MultiThreadRegOpenKeyEx(1, 1000, g_dwRegCache_MAXKEYS + 5, MARK_OPEN_OVERMAX_KEYS_1_THREAD))
                goto ErrorReturn;
            break;
        case 2005: //MARK_OPEN_OVERMAX_KEYS_8_THREAD
            if (!Perf_RegisterMark(MARK_OPEN_OVERMAX_KEYS_8_THREAD , _T("Test=RegOpenKeyEx - MARK_OPEN_OVERMAX_KEYS_8_THREAD - Test#%d, time=%%AVG%%"), lpFTE->dwUniqueID)) 
                GENERIC_FAIL(Perf_RegisterMark);

            if (!L_MultiThreadRegOpenKeyEx(8, 1000, g_dwRegCache_MAXKEYS + 5, MARK_OPEN_OVERMAX_KEYS_8_THREAD))
                goto ErrorReturn;
            break;

        default:
            TRACE(TEXT("Unrecognized test number, don't know what values to use\r\n"));
            goto ErrorReturn;
    }

    dwRetVal=TPR_PASS;

ErrorReturn :
    return dwRetVal;
}