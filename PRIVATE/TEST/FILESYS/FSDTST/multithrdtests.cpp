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

TESTPROCAPI Tst_MultiThrd1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;

    CMtdPart *pMtdPart = NULL;

    TCHAR szPathBuf[MAX_PATH], szDirName[MAX_PATH];
    DWORD dwThreadNumber;
    
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 
            GetPartitionCount() * g_testOptions.nThreadsPerVol;
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

    // partition number div threads-per-volume
    dwThreadNumber = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;
    pMtdPart = GetPartition((dwThreadNumber-1) / g_testOptions.nThreadsPerVol);
    if(NULL == pMtdPart)
    {
        retVal = TPR_PASS;
        LOG(_T("Unable to get partition for thread %u"), dwThreadNumber);
        goto Error;
    }

    LOG(_T("Thread #%u testing volume %s"), dwThreadNumber, pMtdPart->GetVolumeName(szPathBuf));

    _stprintf(szDirName, _T("%s%04.04u"), pMtdPart->GetTestFilePath(szPathBuf),
        dwThreadNumber);   

    if(!CreateFilesToTest(szDirName, FILE_ATTRIBUTE_NORMAL, 10, 10))
    {
        FAIL("CreateFilesToTest()");
        LOG(_T("Thread #%u failed the test!"), dwThreadNumber);
        goto Error;
    }
        
    // success
    LOG(_T("Thread #%u completed test successfully"), dwThreadNumber);
    retVal = TPR_PASS;
    
Error:

    if(pMtdPart && !DeleteTree(szDirName, FALSE))
    {
        LOG(_T("Unable to remove \"%s\""), szDirName);
        FAIL("DeleteTree()");
        retVal = TPR_FAIL;
    }
    
    return retVal;
}

