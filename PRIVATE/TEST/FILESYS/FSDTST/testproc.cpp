//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "tuxmain.h"
#include "fathlp.h"
#include "mfs.h"
#include "storeutil.h"
#include "legacy.h"
#include "testproc.h"

TESTPROCAPI Tst_Format(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = GetPartitionCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    if(0 == GetPartitionCount())
    {
        retVal = TPR_SKIP;
        LOG(_T("There are no [%s] stores with [%s] partitions to test, skipping"),
            g_testOptions.szProfile, g_testOptions.szFilesystem);
        goto Error;
    }

    pMtdPart = GetPartition(((TPS_EXECUTE *)tpParam)->dwThreadNumber-1);
    if(NULL == pMtdPart)
    {
        FAIL("GetPartition()");
        goto Error;
    }

    if(!pMtdPart->FormatVolume())
    {
        FAIL("FormatVolume()");
        goto Error;
    }
        
    // success
    retVal = TPR_PASS;
    
Error:
    
    return retVal;
}

// this test is a utility to create partitions on stores before testing
// lpFTE->dwUserData is the number of partitions to create
// WARNING: all data on the disk will be lost
TESTPROCAPI Tst_Partition(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_ABORT;
    DWORD cParts = 0;
    DWORD cTotalParts = 0;
    HANDLE hStore = INVALID_HANDLE_VALUE;
    HANDLE hFindStore = INVALID_HANDLE_VALUE;
    STOREINFO storeInfo = {0};
    TCHAR szPartName[PARTITIONNAMESIZE] = _T("");
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    // destroy file system information we saved because we're going to invalidate it
    //
    DestroyFSList();

    cTotalParts = (DWORD)lpFTE->dwUserData;

    LOG(_T("Attempting to create %u partitions on each store with profile \"%s\""),
        cTotalParts, g_testOptions.szProfile);

    storeInfo.cbSize = sizeof(STOREINFO);

    hFindStore = FindFirstStore(&storeInfo);
    if(VALID_HANDLE(hFindStore))
    {        
        do
        {
            // only partition stores of type specified
            //
            if(0 != _tcsicmp(storeInfo.sdi.szProfile, g_testOptions.szProfile))
                continue;

            LOG(_T("Found store matching required storage profile [%s]"), 
                g_testOptions.szProfile);
            hStore = OpenStore(storeInfo.szDeviceName);
            if(VALID_HANDLE(hStore))
            {
                LOG(_T("Dismounting and formatting the store..."));
                if(!DismountStore(hStore))
                {
                    ERRFAIL("DismountStore()");
                    goto Error;
                }

                if(!FormatStore(hStore))
                {
                    ERRFAIL("FormatStore()");
                    goto Error;
                }
                
                // create partitions
                //
                for(cParts = 0; cParts < cTotalParts; cParts++)
                {
                    if(!GetStoreInfo(hStore, &storeInfo))
                    {
                        ERRFAIL("GetStoreInfo()");
                        goto Error;
                    }
                    _stprintf(szPartName, SZ_PART_FMT, cParts+1);
                    LOG(_T("Creating partition \"%s\" of size %u on store \"%s\""), 
                        szPartName,
                        storeInfo.snBiggestPartCreatable / (cTotalParts - cParts), 
                        storeInfo.szDeviceName);
                    if(!CreatePartition(hStore, szPartName, 
                        storeInfo.snBiggestPartCreatable / (cTotalParts - cParts)))
                    {
                        ERRFAIL("CreatePartition()");
                        goto Error;
                    }
                }
                CloseHandle(hStore);
                hStore = INVALID_HANDLE_VALUE;
            }
            
        } while(FindNextStore(hFindStore, &storeInfo));
        FindCloseStore(hFindStore);
    }

    BuildFSList();
        
    // success
    retVal = TPR_PASS;
    
Error:
    if(VALID_HANDLE(hStore))
    {
        CloseHandle(hStore);
    }
    return retVal;
}
