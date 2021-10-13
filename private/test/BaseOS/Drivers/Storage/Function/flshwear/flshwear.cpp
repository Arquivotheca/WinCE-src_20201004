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
////////////////////////////////////////////////////////////////////////////////
//
//  FLSHWEAR TUX DLL
//
//  Module: FLSHWEAR.cpp
//          Contains the test functions.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#include "dskutil.h"

enum {
    PERF_WRITE_SAME_W = 0,
    PERF_WRITE_SAME_R,
    PERF_WRITE_SEQUENTIAL_W,
    PERF_WRITE_SEQUENTIAL_R,
    PERF_WRITE_RANDOM_W,
    PERF_WRITE_RANDOM_R,
    PERF_WRITE_FULL_W,
    PERF_WRITE_FULL_R,
    PERF_LINEAR_FILL_W,
    PERF_LINEAR_FILL_R,
    PERF_LINEAR_FREE_W,
    PERF_LINEAR_FREE_R,
    PERF_RANDOM_FILL_W,
    PERF_RANDOM_FILL_R,
    PERF_RANDOM_FREE_W,
    PERF_RANDOM_FREE_R,
    PERF_FRAGMENTED_FILL_W,
    PERF_FRAGMENTED_FILL_R,
    PERF_FRAGMENTED_FREE_W,
    PERF_FRAGMENTED_FREE_R,
};

TCHAR g_testDescription[MAX_PATH];

BOOL ReadWritePerf(HANDLE hDisk, DWORD startSector, DWORD cSectors, DWORD cBytes, DWORD wId, DWORD rId)
{
    BOOL fRet = FALSE;
    BYTE *pWriteBuffer = NULL;
    BYTE *pReadBuffer = NULL;

    pWriteBuffer = new BYTE[cBytes];
    if(NULL == pWriteBuffer) {
        ERRFAIL("new BYTE[]");
        goto done;
    }

    pReadBuffer = new BYTE[cBytes];
    if(NULL == pReadBuffer) {
        ERRFAIL("new BYTE[]");
        goto done;
    }
    
    memset(pWriteBuffer, (BYTE)Random(), cBytes);

    if(g_pWritePerfLog) g_pWritePerfLog->PerfBegin(wId);
    if(!Dsk_WriteSectors(hDisk, startSector, cSectors, pWriteBuffer)) {
        if(g_pWritePerfLog) g_pWritePerfLog->PerfEnd(wId);
        ERRFAIL("Dsk_WriteSectors()");
        goto done;
    }
    if(g_pWritePerfLog) g_pWritePerfLog->PerfEnd(wId);

    if(g_pReadPerfLog) g_pReadPerfLog->PerfBegin(rId);
    if(!Dsk_ReadSectors(hDisk, startSector, cSectors, pReadBuffer)) {
        if(g_pReadPerfLog) g_pReadPerfLog->PerfEnd(rId);
        ERRFAIL("Dsk_ReadSectors()");
        goto done;
    }
    if(g_pReadPerfLog) g_pReadPerfLog->PerfEnd(rId);

    if(0 != memcmp(pWriteBuffer, pReadBuffer, cBytes)) {
        LOG(L"bad data while reading %u sectors at sector %u", cSectors, startSector);
        ERRFAIL("data read is different from data written");
        goto done;
    }

    if(g_fDelete) {
        if(!Dsk_DeleteSectors(hDisk, startSector, cSectors)) {
            ERRFAIL("Dsk_DeleteSectors()");
            goto done;
        }
    }

    fRet = TRUE;
done:
    if(NULL != pReadBuffer) {
        delete[] pReadBuffer;
    }
    if(NULL != pWriteBuffer) {
        delete[] pWriteBuffer;
    }
    return fRet;
}

////////////////////////////////////////////////////////////////////////////////
// Tst_RepeatWriteSame
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Tst_RepeatWriteSame(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if( TPM_QUERY_THREAD_COUNT == uMsg ) {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return SPR_HANDLED;
    }
    else if( TPM_EXECUTE != uMsg ) {
        return TPR_NOT_HANDLED;
    }

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        LOG(_T("SKIP: Global storage device handle is invalid"));
        return TPR_SKIP;
    }

    DWORD sector = 0;
    DWORD bytesToWrite;
    DWORD percent = 0;    
    DWORD perfIdW = PERF_WRITE_SAME_W, perfIdR = PERF_WRITE_SAME_R;
    
    // low level format should put all blocks on the free list and 
    // get rid of any dirty blocks
    //
    LOG(L"formatting the media...");
    if(!Dsk_FormatMedia(g_hDisk)) {
        ERRFAIL("Dsk_FormatMedia()");
        goto done;
    }

    bytesToWrite = g_diskInfo.di_bytes_per_sect * g_cSectors;
    
    LOG(L"disk contains %u total sectors", g_diskInfo.di_total_sectors);
    LOG(L"write size is %u sectors (%u bytes)", g_cSectors, bytesToWrite); 
    LOG(L"total writes to perform is %u", g_totalWrites);

    // repeatedly write the same sector
    //
    sector = 0;

    HRESULT hr = StringCchPrintfEx(g_testDescription, MAX_PATH, NULL, NULL, 
        STRSAFE_NULL_ON_FAILURE, _T("%s"), lpFTE->lpDescription);
    if (FAILED(hr))
    {
        FAIL("StringCchPrintfEx");
        goto done;
    }
    LOG(L"%s : write/read %u bytes", g_testDescription, bytesToWrite);

    if(g_pWritePerfLog) 
    {
        g_pWritePerfLog->PerfRegister(perfIdW, _T("%s: WRITE %d bytes"), g_testDescription, bytesToWrite);
        g_pWritePerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: WRITE [%u] bytes"), g_testDescription, bytesToWrite);
    }
    if(g_pReadPerfLog) 
    {
        g_pReadPerfLog->PerfRegister(perfIdR, _T("%s: READ %d bytes"), g_testDescription, bytesToWrite);
        g_pReadPerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: READ [%u] bytes"), g_testDescription, bytesToWrite);
    }
    
    percent = 0xFFFFFFFF;
    for(DWORD writeCount = 0; writeCount < g_totalWrites; writeCount++) {
        if(percent != (100*writeCount) / g_totalWrites) {
            percent = (100*writeCount) / g_totalWrites;
            LOG(L"%02u%% complete", percent);
        }

        if(!ReadWritePerf(g_hDisk, sector, g_cSectors, bytesToWrite, perfIdW, perfIdR)) {
            FAIL("ReadWritePerf()");
            goto done;
        }
    }

done:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Tst_RepeatWriteSequential
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Tst_RepeatWriteSequential(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if( TPM_QUERY_THREAD_COUNT == uMsg ) {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return SPR_HANDLED;
    }
    else if( TPM_EXECUTE != uMsg ) {
        return TPR_NOT_HANDLED;
    }

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        LOG(_T("SKIP: Global storage device handle is invalid"));
        return TPR_SKIP;
    }
    
    DWORD sector = 0;
    DWORD bytesToWrite;
    DWORD percent = 0;
    DWORD perfIdW = PERF_WRITE_SEQUENTIAL_W, perfIdR = PERF_WRITE_SEQUENTIAL_R;
    
    // low level format should put all blocks on the free list and 
    // get rid of any dirty blocks
    //
    LOG(L"formatting the media...");
    if(!Dsk_FormatMedia(g_hDisk)) {
        ERRFAIL("Dsk_FormatMedia()");
        goto done;
    }

    bytesToWrite = g_diskInfo.di_bytes_per_sect * g_cSectors;

    LOG(L"disk contains %u total sectors", g_diskInfo.di_total_sectors);
    LOG(L"write size is %u sectors (%u bytes)", g_cSectors, bytesToWrite); 
    LOG(L"total writes to perform is %u", g_totalWrites);

    HRESULT hr = StringCchPrintfEx(g_testDescription, MAX_PATH, NULL, NULL, 
        STRSAFE_NULL_ON_FAILURE, _T("%s"), lpFTE->lpDescription);
    if (FAILED(hr))
    {
        FAIL("StringCchPrintfEx");
        goto done;
    }
    LOG(L"%s : write/read %u bytes", g_testDescription, bytesToWrite);

    if(g_pWritePerfLog) 
    {
        g_pWritePerfLog->PerfRegister(perfIdW, _T("%s: WRITE %d bytes"), g_testDescription, bytesToWrite);
        g_pWritePerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: WRITE [%u] bytes"), g_testDescription, bytesToWrite);
    }
    if(g_pReadPerfLog) 
    {
        g_pReadPerfLog->PerfRegister(perfIdR, _T("%s: READ %d bytes"), g_testDescription, bytesToWrite);
        g_pReadPerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: READ [%u] bytes"), g_testDescription, bytesToWrite);
    }

    percent = 0xFFFFFFFF;
    for(DWORD writeCount = 0; writeCount < g_totalWrites; writeCount++) {
        if(percent != (100*writeCount) / g_totalWrites) {
            percent = (100*writeCount) / g_totalWrites;
            LOG(L"%02u%% complete", percent);
        }

        // write every sector in sequence
        sector = (sector + g_cSectors) % (g_diskInfo.di_total_sectors - g_cSectors);

        if(!ReadWritePerf(g_hDisk, sector, g_cSectors, bytesToWrite, perfIdW, perfIdR)) {
            FAIL("ReadWritePerf()");
            goto done;
        }
    }

done:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Tst_RepeatWriteRandom
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Tst_RepeatWriteRandom(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if( TPM_QUERY_THREAD_COUNT == uMsg ) {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return SPR_HANDLED;
    }
    else if( TPM_EXECUTE != uMsg ) {
        return TPR_NOT_HANDLED;
    }

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        LOG(_T("SKIP: Global storage device handle is invalid"));
        return TPR_SKIP;
    }
    
    DWORD sector = 0;
    DWORD bytesToWrite;
    DWORD percent = 0;
    DWORD perfIdW = PERF_WRITE_RANDOM_W, perfIdR = PERF_WRITE_RANDOM_R;
    
    // low level format should put all blocks on the free list and 
    // get rid of any dirty blocks
    //
    LOG(L"formatting the media...");
    if(!Dsk_FormatMedia(g_hDisk)) {
        ERRFAIL("Dsk_FormatMedia()");
        goto done;
    }

    bytesToWrite = g_diskInfo.di_bytes_per_sect * g_cSectors;

    LOG(L"disk contains %u total sectors", g_diskInfo.di_total_sectors);
    LOG(L"write size is %u sectors (%u bytes)", g_cSectors, bytesToWrite); 
    LOG(L"total writes to perform is %u", g_totalWrites);

    HRESULT hr = StringCchPrintfEx(g_testDescription, MAX_PATH, NULL, NULL, 
        STRSAFE_NULL_ON_FAILURE, _T("%s"), lpFTE->lpDescription);
    if (FAILED(hr))
    {
        FAIL("StringCchPrintfEx");
        goto done;
    }
    LOG(L"%s : write/read %u bytes", g_testDescription, bytesToWrite);

    if(g_pWritePerfLog) 
    {
        g_pWritePerfLog->PerfRegister(perfIdW, _T("%s: WRITE %d bytes"), g_testDescription, bytesToWrite);
        g_pWritePerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: WRITE [%u] bytes"), g_testDescription, bytesToWrite);
    }
    if(g_pReadPerfLog) 
    {
        g_pReadPerfLog->PerfRegister(perfIdR, _T("%s: READ %d bytes"), g_testDescription, bytesToWrite);
        g_pReadPerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: READ [%u] bytes"), g_testDescription, bytesToWrite);
    }
    
    percent = 0xFFFFFFFF;
    for(DWORD writeCount = 0; writeCount < g_totalWrites; writeCount++) {
        if(percent != (100*writeCount) / g_totalWrites) {
            percent = (100*writeCount) / g_totalWrites;
            LOG(L"%02u%% complete", percent);
        }

        // write a random sector every time
        sector = Random() % (g_diskInfo.di_total_sectors - g_cSectors);

        if(!ReadWritePerf(g_hDisk, sector, g_cSectors, bytesToWrite, perfIdW, perfIdR)) {
            FAIL("ReadWritePerf()");
            goto done;
        }
    }

done:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Tst_RepeatWriteFull
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Tst_RepeatWriteFull(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if( TPM_QUERY_THREAD_COUNT == uMsg ) {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return SPR_HANDLED;
    }
    else if( TPM_EXECUTE != uMsg ) {
        return TPR_NOT_HANDLED;
    }

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        LOG(_T("SKIP: Global storage device handle is invalid"));
        return TPR_SKIP;
    }

    DWORD repeat;
    DWORD bytesToWrite;
    DWORD perfIdW = PERF_WRITE_FULL_W, perfIdR = PERF_WRITE_FULL_R;
    
    // low level format should put all blocks on the free list and 
    // get rid of any dirty blocks
    //
    LOG(L"formatting the media...");
    if(!Dsk_FormatMedia(g_hDisk)) {
        ERRFAIL("Dsk_FormatMedia()");
        goto done;
    }
    
    // fill the disk the requested amount
    //
    if(!Util_FillDiskLinear(g_hDisk, 100)) {
        FAIL("Util_FillDiskLinear()");
        goto done;
    }

    bytesToWrite = g_diskInfo.di_bytes_per_sect * g_cSectors;
    
    LOG(L"disk contains %u total sectors", g_diskInfo.di_total_sectors);
    LOG(L"write size is %u sectors (%u bytes)", g_cSectors, bytesToWrite); 
    LOG(L"total writes to perform is %u", g_totalWrites);
    
    HRESULT hr = StringCchPrintfEx(g_testDescription, MAX_PATH, NULL, NULL, 
        STRSAFE_NULL_ON_FAILURE, _T("%s"), lpFTE->lpDescription);
    if (FAILED(hr))
    {
        FAIL("StringCchPrintfEx");
        goto done;
    }
    LOG(L"%s : write/read %u bytes", g_testDescription, bytesToWrite);

    if(g_pWritePerfLog) 
    {
        g_pWritePerfLog->PerfRegister(perfIdW, _T("%s: WRITE %d bytes"), g_testDescription, bytesToWrite);
        g_pWritePerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: WRITE [%u] bytes"), g_testDescription, bytesToWrite);
    }
    if(g_pReadPerfLog) 
    {
        g_pReadPerfLog->PerfRegister(perfIdR, _T("%s: READ %d bytes"), g_testDescription, bytesToWrite);
        g_pReadPerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: READ [%u] bytes"), g_testDescription, bytesToWrite);
    }
    
    // now repeatedly write to the same X disk sectors
    //
    for(repeat = 0; repeat < g_cRepeat; repeat++) {

        if(!Dsk_DeleteSectors(g_hDisk, 0, g_cSectors)) {
            ERRFAIL("Dsk_DeleteSectors()");
            goto done;
        }

        LOG(L"Writing %u sectors at sector %u...", g_cSectors, 0);
        if(!ReadWritePerf(g_hDisk, 0, g_cSectors, bytesToWrite, perfIdW, perfIdR)) {
            FAIL("ReadWritePerf()");
            goto done;
        }

        LOG(L"Waiting for idle compaction...");
        // allow idle compaction to kick in
        Sleep(1000);
    }

done:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Tst_LinearFillDiskReadWrite
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Tst_LinearFillDiskReadWrite(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if( TPM_QUERY_THREAD_COUNT == uMsg ) {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return SPR_HANDLED;
    }
    else if( TPM_EXECUTE != uMsg ) {
        return TPR_NOT_HANDLED;
    }

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        LOG(_T("SKIP: Global storage device handle is invalid"));
        return TPR_SKIP;
    }
    
    DWORD sector;
    DWORD bytesToWrite;
    DWORD repeat;
    BOOL fLinearWrite = TRUE;
    DWORD perfIdW = PERF_LINEAR_FILL_W, perfIdR = PERF_LINEAR_FILL_R;

    for (DWORD percentToFill = 0; percentToFill < countof(g_filled); percentToFill++)
    {
        if (!g_filled[percentToFill])
            continue;
        
        // fill the disk the requested amount
        //
        if(!Util_FillDiskLinear(g_hDisk, percentToFill)) {
            FAIL("Util_FillDiskLinear()");
            goto done;
        }

        // calculate the total bytes to write based on sector size and 
        // requested number of sectors per write
        //
        bytesToWrite = g_cSectors * g_diskInfo.di_bytes_per_sect;

        LOG(L"performing %u %u byte writes to empty FLASH linearly filled to %02u%% capacity", 
            g_cRepeat, bytesToWrite, percentToFill);

        HRESULT hr = StringCchPrintfEx(g_testDescription, MAX_PATH, NULL, NULL, 
            STRSAFE_NULL_ON_FAILURE, _T("%2u%% %s"), percentToFill, lpFTE->lpDescription);
        if (FAILED(hr))
        {
            FAIL("StringCchPrintfEx");
            goto done;
        }
        LOG(L"%s : write/read %u bytes", g_testDescription, bytesToWrite);

        if(g_pWritePerfLog) 
        {
            g_pWritePerfLog->PerfRegister(perfIdW, _T("%s: WRITE %d bytes"), g_testDescription, bytesToWrite);
            g_pWritePerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: WRITE [%u] bytes"), g_testDescription, bytesToWrite);
        }
        if(g_pReadPerfLog) 
        {
            g_pReadPerfLog->PerfRegister(perfIdR, _T("%s: READ %d bytes"), g_testDescription, bytesToWrite);
            g_pReadPerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: READ [%u] bytes"), g_testDescription, bytesToWrite);
        }
        
        for(repeat = 0; repeat < g_cRepeat; repeat++) 
        {
            if(fLinearWrite) 
                sector = repeat % (g_diskInfo.di_total_sectors - g_cSectors);
            else
                sector = Random() % (g_diskInfo.di_total_sectors - g_cSectors);

            if(!ReadWritePerf(g_hDisk, sector, g_cSectors, bytesToWrite, perfIdW, perfIdR)) {
                FAIL("ReadWritePerf()");
                goto done;
            }
        }
        LOG(_T(""));
    }
        
done:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Tst_LinearFreeDiskReadWrite
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Tst_LinearFreeDiskReadWrite(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if( TPM_QUERY_THREAD_COUNT == uMsg ) {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return SPR_HANDLED;
    }
    else if( TPM_EXECUTE != uMsg ) {
        return TPR_NOT_HANDLED;
    }


    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        LOG(_T("SKIP: Global storage device handle is invalid"));
        return TPR_SKIP;
    }
    
    DWORD sector;
    DWORD bytesToWrite;
    DWORD repeat;
    BOOL fLinearWrite = TRUE;
    DWORD perfIdW = PERF_LINEAR_FREE_W, perfIdR = PERF_LINEAR_FREE_R;

    for (DWORD percentToFill = 0; percentToFill < countof(g_filled); percentToFill++)
    {
        if (!g_filled[percentToFill])
            continue;
        
        // fill the disk the requested amount
        //
        if(!Util_FillDiskLinear(g_hDisk, 100)) {
            FAIL("Util_FillDiskLinear()");
            goto done;
        }

        // free the requested amount
        //
        if(!Util_FreeDiskLinear(g_hDisk, 100-percentToFill)) {
            FAIL("Util_FreeDiskRandom()");
            goto done;
        }

        LOG(L"disk is now approximately %02u%% full", percentToFill);

        // calculate the total bytes to write based on sector size and 
        // requested number of sectors per write
        //
        bytesToWrite = g_cSectors * g_diskInfo.di_bytes_per_sect;

        LOG(L"performing %u %u byte writes to full FLASH linearly freed to %02u%% capacity", 
            g_cRepeat, bytesToWrite, percentToFill);

        HRESULT hr = StringCchPrintfEx(g_testDescription, MAX_PATH, NULL, NULL, 
            STRSAFE_NULL_ON_FAILURE, _T("%2u%% %s"), percentToFill, lpFTE->lpDescription);
        if (FAILED(hr))
        {
            FAIL("StringCchPrintfEx");
            goto done;
        }
        LOG(L"%s : write/read %u bytes", g_testDescription, bytesToWrite);

        if(g_pWritePerfLog) 
        {
            g_pWritePerfLog->PerfRegister(perfIdW, _T("%s: WRITE %d bytes"), g_testDescription, bytesToWrite);
            g_pWritePerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: WRITE [%u] bytes"), g_testDescription, bytesToWrite);
        }
        if(g_pReadPerfLog) 
        {
            g_pReadPerfLog->PerfRegister(perfIdR, _T("%s: READ %d bytes"), g_testDescription, bytesToWrite);
            g_pReadPerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: READ [%u] bytes"), g_testDescription, bytesToWrite);
        }
                
        for(repeat = 0; repeat < g_cRepeat; repeat++) {

            if(fLinearWrite) {
                sector = repeat % (g_diskInfo.di_total_sectors - g_cSectors);
            } else {
                sector = Random() % (g_diskInfo.di_total_sectors - g_cSectors);
            }

            if(!ReadWritePerf(g_hDisk, sector, g_cSectors, bytesToWrite, perfIdW, perfIdR)) {
                FAIL("ReadWritePerf()");
                goto done;
            }
        }
        LOG(_T(""));
    }        
done:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Tst_RandFillDiskReadWrite
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Tst_RandFillDiskReadWrite(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if( TPM_QUERY_THREAD_COUNT == uMsg ) {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return SPR_HANDLED;
    }
    else if( TPM_EXECUTE != uMsg ) {
        return TPR_NOT_HANDLED;
    }

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        LOG(_T("SKIP: Global storage device handle is invalid"));
        return TPR_SKIP;
    }
 
    DWORD sector;
    DWORD bytesToWrite;
    DWORD repeat;
    BOOL fLinearWrite = TRUE;
    DWORD perfIdW = PERF_RANDOM_FILL_W, perfIdR = PERF_RANDOM_FILL_R;

    for (DWORD percentToFill = 0; percentToFill < countof(g_filled); percentToFill++)
    {
        if (!g_filled[percentToFill])
            continue;
        
        // fill the disk the requested amount
        //
        if(!Util_FillDiskRandom(g_hDisk, percentToFill)) {
            FAIL("Util_FillDiskRandom()");
            goto done;
        }

        // calculate the total bytes to write based on sector size and 
        // requested number of sectors per write
        //
        bytesToWrite = g_cSectors * g_diskInfo.di_bytes_per_sect;
        
        LOG(L"performing %u %u byte writes to empty FLASH randomly filled to %02u%% capacity", 
            g_cRepeat, bytesToWrite, percentToFill);
        
        HRESULT hr = StringCchPrintfEx(g_testDescription, MAX_PATH, NULL, NULL, 
            STRSAFE_NULL_ON_FAILURE, _T("%2u%% %s"), percentToFill, lpFTE->lpDescription);
        if (FAILED(hr))
        {
            FAIL("StringCchPrintfEx");
            goto done;
        }
        LOG(L"%s : write/read %u bytes", g_testDescription, bytesToWrite);

        if(g_pWritePerfLog) 
        {
            g_pWritePerfLog->PerfRegister(perfIdW, _T("%s: WRITE %d bytes"), g_testDescription, bytesToWrite);
            g_pWritePerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: WRITE [%u] bytes"), g_testDescription, bytesToWrite);
        }
        if(g_pReadPerfLog) 
        {
            g_pReadPerfLog->PerfRegister(perfIdR, _T("%s: READ %d bytes"), g_testDescription, bytesToWrite);
            g_pReadPerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: READ [%u] bytes"), g_testDescription, bytesToWrite);
        }
        
        for(repeat = 0; repeat < g_cRepeat; repeat++) {

            if(fLinearWrite) {
                sector = repeat % (g_diskInfo.di_total_sectors - g_cSectors);
            } else {
                sector = Random() % (g_diskInfo.di_total_sectors - g_cSectors);
            }

            if(!ReadWritePerf(g_hDisk, sector, g_cSectors, bytesToWrite, perfIdW, perfIdR)) {
                FAIL("ReadWritePerf()");
                goto done;
            }
        }
        LOG(_T(""));
    }           
done:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Tst_RandFreeDiskReadWrite
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Tst_RandFreeDiskReadWrite(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if( TPM_QUERY_THREAD_COUNT == uMsg ) {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return SPR_HANDLED;
    }
    else if( TPM_EXECUTE != uMsg ) {
        return TPR_NOT_HANDLED;
    }

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        LOG(_T("SKIP: Global storage device handle is invalid"));
        return TPR_SKIP;
    }
    
    DWORD sector;
    DWORD bytesToWrite;
    DWORD repeat;
    BOOL fLinearWrite = TRUE;
    DWORD perfIdW = PERF_RANDOM_FREE_W, perfIdR = PERF_RANDOM_FREE_R;

    for (DWORD percentToFill = 0; percentToFill < countof(g_filled); percentToFill++)
    {
        if (!g_filled[percentToFill])
            continue;
    
        // fill the disk the requested amount
        //
        if(!Util_FillDiskLinear(g_hDisk, 100)) {
            FAIL("Util_FillDiskLinear()");
            goto done;
        }

        // free the requested amount
        //
        if(!Util_FreeDiskRandom(g_hDisk, 100-percentToFill)) {
            FAIL("Util_FreeDiskRandom()");
            goto done;
        }

        LOG(L"disk is now approximately %02u%% full", percentToFill);

        // calculate the total bytes to write based on sector size and 
        // requested number of sectors per write
        //
        bytesToWrite = g_cSectors * g_diskInfo.di_bytes_per_sect;
        
        LOG(L"performing %u %u byte writes to full FLASH randomly freed to %02u%% capacity", 
            g_cRepeat, bytesToWrite, percentToFill);
        
        HRESULT hr = StringCchPrintfEx(g_testDescription, MAX_PATH, NULL, NULL, 
            STRSAFE_NULL_ON_FAILURE, _T("%2u%% %s"), percentToFill, lpFTE->lpDescription);
        if (FAILED(hr))
        {
            FAIL("StringCchPrintfEx");
            goto done;
        }
        LOG(L"%s : write/read %u bytes", g_testDescription, bytesToWrite);

        if(g_pWritePerfLog) 
        {
            g_pWritePerfLog->PerfRegister(perfIdW, _T("%s: WRITE %d bytes"), g_testDescription, bytesToWrite);
            g_pWritePerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: WRITE [%u] bytes"), g_testDescription, bytesToWrite);
        }
        if(g_pReadPerfLog) 
        {
            g_pReadPerfLog->PerfRegister(perfIdR, _T("%s: READ %d bytes"), g_testDescription, bytesToWrite);
            g_pReadPerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: READ [%u] bytes"), g_testDescription, bytesToWrite);
        }
        
        for(repeat = 0; repeat < g_cRepeat; repeat++) {

            if(fLinearWrite) {
                sector = repeat % (g_diskInfo.di_total_sectors - g_cSectors);
            } else {
                sector = Random() % (g_diskInfo.di_total_sectors - g_cSectors);
            }

            if(!ReadWritePerf(g_hDisk, sector, g_cSectors, bytesToWrite, perfIdW, perfIdR)) {
                FAIL("ReadWritePerf()");
                goto done;
            }
        }
        LOG(_T(""));
    }           
done:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Tst_FragmentedFillDiskReadWrite
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Tst_FragmentedFillDiskReadWrite(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if( TPM_QUERY_THREAD_COUNT == uMsg ) {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return SPR_HANDLED;
    }
    else if( TPM_EXECUTE != uMsg ) {
        return TPR_NOT_HANDLED;
    }

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        LOG(_T("SKIP: Global storage device handle is invalid"));
        return TPR_SKIP;
    }
    
    DWORD sector;
    DWORD bytesToWrite;
    DWORD repeat;
    BOOL fLinearWrite = TRUE;
    DWORD perfIdW = PERF_FRAGMENTED_FILL_W, perfIdR = PERF_FRAGMENTED_FILL_R;
    for (DWORD percentToFill = 0; percentToFill < countof(g_filled); percentToFill++)
    {
        if (!g_filled[percentToFill])
            continue;
        
        // fill the disk the requested amount
        //
        if(!Util_FillDiskMaxFragment(g_hDisk, percentToFill)) {
            FAIL("Util_FillDiskMaxFragment()");
            goto done;
        }

        // calculate the total bytes to write based on sector size and 
        // requested number of sectors per write
        //
        bytesToWrite = g_cSectors * g_diskInfo.di_bytes_per_sect;  
        
        LOG(L"performing %u %u byte writes to empty FLASH filled w/maximum fragmentation to %02u%% capacity", 
            g_cRepeat, bytesToWrite, percentToFill);

        HRESULT hr = StringCchPrintfEx(g_testDescription, MAX_PATH, NULL, NULL, 
            STRSAFE_NULL_ON_FAILURE, _T("%2u%% %s"), percentToFill, lpFTE->lpDescription);
        if (FAILED(hr))
        {
            FAIL("StringCchPrintfEx");
            goto done;
        }
        LOG(L"%s : write/read %u bytes", g_testDescription, bytesToWrite);

        if(g_pWritePerfLog) 
        {
            g_pWritePerfLog->PerfRegister(perfIdW, _T("%s: WRITE %d bytes"), g_testDescription, bytesToWrite);
            g_pWritePerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: WRITE [%u] bytes"), g_testDescription, bytesToWrite);
        }
        if(g_pReadPerfLog) 
        {
            g_pReadPerfLog->PerfRegister(perfIdR, _T("%s: READ %d bytes"), g_testDescription, bytesToWrite);
            g_pReadPerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: READ [%u] bytes"), g_testDescription, bytesToWrite);
        }
        
        for(repeat = 0; repeat < g_cRepeat; repeat++) {

            if(fLinearWrite) {
                sector = repeat % (g_diskInfo.di_total_sectors - g_cSectors);
            } else {
                sector = Random() % (g_diskInfo.di_total_sectors - g_cSectors);
            }

            if(!ReadWritePerf(g_hDisk, sector, g_cSectors, bytesToWrite, perfIdW, perfIdR)) {
                FAIL("ReadWritePerf()");
                goto done;
            }
        }
        LOG(_T(""));
    }           
done:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Tst_FragmentedFreeDiskReadWrite
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Tst_FragmentedFreeDiskReadWrite(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if( TPM_QUERY_THREAD_COUNT == uMsg ) {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return SPR_HANDLED;
    }
    else if( TPM_EXECUTE != uMsg ) {
        return TPR_NOT_HANDLED;
    }

    // validate the global disk handle
    if(INVALID_HANDLE(g_hDisk))
    {
        LOG(_T("SKIP: Global storage device handle is invalid"));
        return TPR_SKIP;
    }
    
    DWORD sector;
    DWORD bytesToWrite;
    DWORD repeat;
    BOOL fLinearWrite = TRUE;
    DWORD perfIdW = PERF_FRAGMENTED_FREE_W, perfIdR = PERF_FRAGMENTED_FREE_R;

    for (DWORD percentToFill = 0; percentToFill < countof(g_filled); percentToFill++)
    {
        if (!g_filled[percentToFill])
            continue;
        
        // fill the disk the requested amount
        //
        if(!Util_FillDiskLinear(g_hDisk, 100)) {
            FAIL("Util_FillDiskLinear()");
            goto done;
        }

        // free the requested amount
        //
        if(!Util_FreeDiskMaxFragment(g_hDisk, 100-percentToFill)) {
            FAIL("Util_FreeDiskMaxFragment()");
            goto done;
        }

        LOG(L"disk is now approximately %02u%% full", percentToFill);

        // calculate the total bytes to write based on sector size and 
        // requested number of sectors per write
        //
        bytesToWrite = g_cSectors * g_diskInfo.di_bytes_per_sect;
        
        LOG(L"performing %u %u byte writes to full FLASH freed w/maximum fragmentation to %02u%% capacity", 
            g_cRepeat, bytesToWrite, percentToFill);

        HRESULT hr = StringCchPrintfEx(g_testDescription, MAX_PATH, NULL, NULL, 
            STRSAFE_NULL_ON_FAILURE, _T("%2u%% %s"), percentToFill, lpFTE->lpDescription);
        if (FAILED(hr))
        {
            FAIL("StringCchPrintfEx");
            goto done;
        }
        LOG(L"%s : write/read %u bytes", g_testDescription, bytesToWrite);

        if(g_pWritePerfLog) 
        {
            g_pWritePerfLog->PerfRegister(perfIdW, _T("%s: WRITE %d bytes"), g_testDescription, bytesToWrite);
            g_pWritePerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: WRITE [%u] bytes"), g_testDescription, bytesToWrite);
        }
        if(g_pReadPerfLog) 
        {
            g_pReadPerfLog->PerfRegister(perfIdR, _T("%s: READ %d bytes"), g_testDescription, bytesToWrite);
            g_pReadPerfLog->PerfLogText(_T("\r\n## PERF ## [%s]: READ [%u] bytes"), g_testDescription, bytesToWrite);
        }
                
        for(repeat = 0; repeat < g_cRepeat; repeat++) {

            if(fLinearWrite) {
                sector = repeat % (g_diskInfo.di_total_sectors - g_cSectors);
            } else {
                sector = Random() % (g_diskInfo.di_total_sectors - g_cSectors);
            }
            
            if(!ReadWritePerf(g_hDisk, sector, g_cSectors, bytesToWrite, perfIdW, perfIdR)) {
                FAIL("ReadWritePerf()");
                goto done;
            }
        }
        LOG(_T(""));
    }           
done:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
