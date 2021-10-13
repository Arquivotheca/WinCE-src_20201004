//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

BOOL ReadWritePerf(HANDLE hDisk, DWORD startSector, DWORD cSectors, DWORD cBytes)
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

    if(g_pWritePerfLog) g_pWritePerfLog->StartTick();
    if(!Dsk_WriteSectors(hDisk, startSector, cSectors, pWriteBuffer)) {
        if(g_pWritePerfLog) g_pWritePerfLog->EndTick();
        ERRFAIL("Dsk_WriteSectors()");
        goto done;
    }
    if(g_pWritePerfLog) g_pWritePerfLog->EndTick();

    if(g_pReadPerfLog) g_pReadPerfLog->StartTick();
    if(!Dsk_ReadSectors(hDisk, startSector, cSectors, pReadBuffer)) {
        if(g_pReadPerfLog) g_pReadPerfLog->EndTick();    
        ERRFAIL("Dsk_ReadSectors()");
        goto done;
    }
    if(g_pReadPerfLog) g_pReadPerfLog->EndTick();

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

    HANDLE hDisk = INVALID_HANDLE_VALUE;
    DISK_INFO di;
    DWORD sector = 0;
    DWORD bytesToWrite;
    DWORD writeCount;
    DWORD totalWrites;
    DWORD percent = 0;    

    LOG(L"opening a handle to storage device \"%s\"", g_szDisk);
    LOG(L"CreateFile(\"%s\", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL", g_szDisk);
    hDisk = CreateFile(g_szDisk, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE_VALUE == hDisk) {
        ERRFAIL("CreateFile()");
        goto done;
    }

    // low level format should put all blocks on the free list and 
    // get rid of any dirty blocks
    //
    LOG(L"formatting the media...");
    if(!Dsk_FormatMedia(hDisk)) {
        ERRFAIL("Dsk_FormatMedia()");
        goto done;
    }

    LOG(L"querrying disk information");
    if(!Dsk_GetDiskInfo(hDisk, &di)) {
        ERRFAIL("Dsk_GetDiskInfo()");
        goto done;
    }

    bytesToWrite = di.di_bytes_per_sect * g_cSectors;
    
    LOG(L"disk contains %u total sectors", di.di_total_sectors);
    LOG(L"will perform %u writes per sector", g_cRepeat, bytesToWrite);
    LOG(L"write size is %u sectors (%u bytes)", g_cSectors, bytesToWrite); 
    totalWrites = g_cRepeat * di.di_total_sectors;
    LOG(L"total writes to perform is %u", totalWrites);


    // repeatedly write the same sector
    //
    sector = 0;

    LOG(L"%s : write/read %u bytes", lpFTE->lpDescription, bytesToWrite);
    if(g_pReadPerfLog) g_pReadPerfLog->InitTest(L"%s : read %u bytes", lpFTE->lpDescription, bytesToWrite); 
    if(g_pWritePerfLog) g_pWritePerfLog->InitTest(L"%s : write %u bytes", lpFTE->lpDescription, bytesToWrite); 

    percent = 0xFFFFFFFF;
    for(writeCount = 0; writeCount < totalWrites; writeCount++) {
        if(percent != (100*writeCount) / (totalWrites)) {
            percent = (100*writeCount) / (totalWrites);
            LOG(L"%02u%% complete", percent);
        }

        if(!ReadWritePerf(hDisk, sector, g_cSectors, bytesToWrite)) {
            FAIL("ReadWritePerf()");
            goto done;
        }
        
    }

done:
    if(INVALID_HANDLE_VALUE != hDisk) {
        VERIFY(CloseHandle(hDisk));
    }
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

    HANDLE hDisk = INVALID_HANDLE_VALUE;
    DISK_INFO di;
    DWORD sector = 0;
    DWORD bytesToWrite;
    DWORD writeCount;
    DWORD totalWrites;
    DWORD percent = 0;

    LOG(L"opening a handle to storage device \"%s\"", g_szDisk);
    LOG(L"CreateFile(\"%s\", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL", g_szDisk);
    hDisk = CreateFile(g_szDisk, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE_VALUE == hDisk) {
        ERRFAIL("CreateFile()");
        goto done;
    }

    // low level format should put all blocks on the free list and 
    // get rid of any dirty blocks
    //
    LOG(L"formatting the media...");
    if(!Dsk_FormatMedia(hDisk)) {
        ERRFAIL("Dsk_FormatMedia()");
        goto done;
    }

    LOG(L"querrying disk information");
    if(!Dsk_GetDiskInfo(hDisk, &di)) {
        ERRFAIL("Dsk_GetDiskInfo()");
        goto done;
    }

    bytesToWrite = di.di_bytes_per_sect * g_cSectors;

    LOG(L"disk contains %u total sectors", di.di_total_sectors);
    LOG(L"will perform %u writes per sector", g_cRepeat, bytesToWrite);
    LOG(L"write size is %u sectors (%u bytes)", g_cSectors, bytesToWrite); 
    totalWrites = g_cRepeat * di.di_total_sectors;
    LOG(L"total writes to perform is %u", totalWrites);

    LOG(L"%s : write/read %u bytes", lpFTE->lpDescription, bytesToWrite);
    if(g_pReadPerfLog) g_pReadPerfLog->InitTest(L"%s : read %u bytes", lpFTE->lpDescription, bytesToWrite); 
    if(g_pWritePerfLog) g_pWritePerfLog->InitTest(L"%s : write %u bytes", lpFTE->lpDescription, bytesToWrite); 

    percent = 0xFFFFFFFF;
    for(writeCount = 0; writeCount < totalWrites; writeCount++) {
        if(percent != (100*writeCount) / (totalWrites)) {
            percent = (100*writeCount) / (totalWrites);
            LOG(L"%02u%% complete", percent);
        }

        // write every sector in sequence
        sector = writeCount % (di.di_total_sectors - g_cSectors);

        if(!ReadWritePerf(hDisk, sector, g_cSectors, bytesToWrite)) {
            FAIL("ReadWritePerf()");
            goto done;
        }
        
    }

done:
    if(INVALID_HANDLE_VALUE != hDisk) {
        VERIFY(CloseHandle(hDisk));
    }
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

    HANDLE hDisk = INVALID_HANDLE_VALUE;
    DISK_INFO di;
    DWORD sector = 0;
    DWORD bytesToWrite;
    DWORD writeCount;
    DWORD totalWrites;
    DWORD percent = 0;

    LOG(L"opening a handle to storage device \"%s\"", g_szDisk);
    LOG(L"CreateFile(\"%s\", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL", g_szDisk);
    hDisk = CreateFile(g_szDisk, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE_VALUE == hDisk) {
        ERRFAIL("CreateFile()");
        goto done;
    }

    // low level format should put all blocks on the free list and 
    // get rid of any dirty blocks
    //
    LOG(L"formatting the media...");
    if(!Dsk_FormatMedia(hDisk)) {
        ERRFAIL("Dsk_FormatMedia()");
        goto done;
    }

    LOG(L"querrying disk information");
    if(!Dsk_GetDiskInfo(hDisk, &di)) {
        ERRFAIL("Dsk_GetDiskInfo()");
        goto done;
    }

    bytesToWrite = di.di_bytes_per_sect * g_cSectors;

    LOG(L"disk contains %u total sectors", di.di_total_sectors);
    LOG(L"will perform %u writes per sector", g_cRepeat, bytesToWrite);
    LOG(L"write size is %u sectors (%u bytes)", g_cSectors, bytesToWrite); 
    totalWrites = g_cRepeat * di.di_total_sectors;
    LOG(L"total writes to perform is %u", totalWrites);

    LOG(L"%s : write/read %u bytes", lpFTE->lpDescription, bytesToWrite);
    if(g_pReadPerfLog) g_pReadPerfLog->InitTest(L"%s : read %u bytes", lpFTE->lpDescription, bytesToWrite); 
    if(g_pWritePerfLog) g_pWritePerfLog->InitTest(L"%s : write %u bytes", lpFTE->lpDescription, bytesToWrite); 
    
    percent = 0xFFFFFFFF;
    for(writeCount = 0; writeCount < totalWrites; writeCount++) {
        if(percent != (100*writeCount) / (totalWrites)) {
            percent = (100*writeCount) / (totalWrites);
            LOG(L"%02u%% complete", percent);
        }

        // write a random sector every time
        sector = Random() % (di.di_total_sectors - g_cSectors);

        if(!ReadWritePerf(hDisk, sector, g_cSectors, bytesToWrite)) {
            FAIL("ReadWritePerf()");
            goto done;
        }
    }

done:
    if(INVALID_HANDLE_VALUE != hDisk) {
        VERIFY(CloseHandle(hDisk));
    }
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

    HANDLE hDisk = INVALID_HANDLE_VALUE;
    DISK_INFO di;
    DWORD sectorsToWrite = lpFTE->dwUserData;
    DWORD repeat;
    DWORD bytesToWrite;

    LOG(L"opening a handle to storage device \"%s...\"", g_szDisk);
    LOG(L"CreateFile(\"%s\", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL", g_szDisk);
    hDisk = CreateFile(g_szDisk, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE_VALUE == hDisk) {
        ERRFAIL("CreateFile()");
        goto done;
    }    

    // low level format should put all blocks on the free list and 
    // get rid of any dirty blocks
    //
    LOG(L"formatting the media...");
    if(!Dsk_FormatMedia(hDisk)) {
        ERRFAIL("Dsk_FormatMedia()");
        goto done;
    }

    // get disk info so we know the sector size
    //
    if(!Dsk_GetDiskInfo(hDisk, &di)) {
        ERRFAIL("Dsk_GetDiskInfo()");
        goto done;
    }

    // fill the disk the requested amount
    //
    if(!Util_FillDiskLinear(hDisk, 100)) {
        FAIL("Util_FillDiskLinear()");
        goto done;
    }

    bytesToWrite = di.di_bytes_per_sect * g_cSectors;

    // now repeatedly write to the same X disk sectors
    //
    for(repeat = 0; repeat < g_cRepeat; repeat++) {

        if(!Dsk_DeleteSectors(hDisk, 0, g_cSectors)) {
            ERRFAIL("Dsk_DeleteSectors()");
            goto done;
        }

        LOG(L"Writing %u sectors at sector %u...", g_cSectors, 0);
        if(!ReadWritePerf(hDisk, 0, g_cSectors, bytesToWrite)) {
            FAIL("ReadWritePerf()");
            goto done;
        }

        LOG(L"Waiting for idle compaction...");
        // allow idle compaction to kick in
        Sleep(1000);
    }

done:
    if(INVALID_HANDLE_VALUE != hDisk) {
        VERIFY(CloseHandle(hDisk));
    }
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

    HANDLE hDisk = INVALID_HANDLE_VALUE;
    DISK_INFO di;
    DWORD sector;
    DWORD percentToFill = lpFTE->dwUserData;
    DWORD bytesToWrite;
    DWORD repeat;
    BOOL fLinearWrite = TRUE;

    LOG(L"opening a handle to storage device \"%s...\"", g_szDisk);
    LOG(L"CreateFile(\"%s\", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL", g_szDisk);
    hDisk = CreateFile(g_szDisk, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE_VALUE == hDisk) {
        ERRFAIL("CreateFile()");
        goto done;
    }

    // get disk info so we know the sector size
    //
    if(!Dsk_GetDiskInfo(hDisk, &di)) {
        ERRFAIL("Dsk_GetDiskInfo()");
        goto done;
    }

    // fill the disk the requested amount
    //
    if(!Util_FillDiskLinear(hDisk, percentToFill)) {
        FAIL("Util_FillDiskLinear()");
        goto done;
    }

    // calculate the total bytes to write based on sector size and 
    // requested number of sectors per write
    //
    bytesToWrite = g_cSectors * di.di_bytes_per_sect;

    LOG(L"performing %u %u byte writes to empty FLASH linearly filled to %02u%% capacity", 
        g_cRepeat, bytesToWrite, percentToFill);

    LOG(L"%s : write/read %u bytes", lpFTE->lpDescription, bytesToWrite);
    if(g_pReadPerfLog) g_pReadPerfLog->InitTest(L"%s : read %u bytes", lpFTE->lpDescription, bytesToWrite); 
    if(g_pWritePerfLog) g_pWritePerfLog->InitTest(L"%s : write %u bytes", lpFTE->lpDescription, bytesToWrite); 

    for(repeat = 0; repeat < g_cRepeat; repeat++) {

        if(fLinearWrite) {
            sector = repeat % (di.di_total_sectors - g_cSectors);
        } else {
            sector = Random() % (di.di_total_sectors - g_cSectors);
        }

        if(!ReadWritePerf(hDisk, sector, g_cSectors, bytesToWrite)) {
            FAIL("ReadWritePerf()");
            goto done;
        }
    }
        
done:
    if(INVALID_HANDLE_VALUE != hDisk) {
        VERIFY(CloseHandle(hDisk));
    }
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

    HANDLE hDisk = INVALID_HANDLE_VALUE;
    DISK_INFO di;
    DWORD sector;
    DWORD percentToFill = lpFTE->dwUserData;
    DWORD bytesToWrite;
    DWORD repeat;
    BOOL fLinearWrite = TRUE;


    LOG(L"opening a handle to storage device \"%s...\"", g_szDisk);
    LOG(L"CreateFile(\"%s\", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL", g_szDisk);
    hDisk = CreateFile(g_szDisk, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE_VALUE == hDisk) {
        ERRFAIL("CreateFile()");
        goto done;
    }    

    // get disk info so we know the sector size
    //
    if(!Dsk_GetDiskInfo(hDisk, &di)) {
        ERRFAIL("Dsk_GetDiskInfo()");
        goto done;
    }

    // fill the disk the requested amount
    //
    if(!Util_FillDiskLinear(hDisk, 100)) {
        FAIL("Util_FillDiskLinear()");
        goto done;
    }

    // free the requested amount
    //
    if(!Util_FreeDiskLinear(hDisk, 100-percentToFill)) {
        FAIL("Util_FreeDiskRandom()");
        goto done;
    }

    LOG(L"disk is now approximately %02u%% full", percentToFill);

    // calculate the total bytes to write based on sector size and 
    // requested number of sectors per write
    //
    bytesToWrite = g_cSectors * di.di_bytes_per_sect;
    
    LOG(L"performing %u %u byte writes to full FLASH linearly freed to %02u%% capacity", 
        g_cRepeat, bytesToWrite, percentToFill);
    
    LOG(L"%s : write/read %u bytes", lpFTE->lpDescription, bytesToWrite);
    if(g_pReadPerfLog) g_pReadPerfLog->InitTest(L"%s : read %u bytes", lpFTE->lpDescription, bytesToWrite); 
    if(g_pWritePerfLog) g_pWritePerfLog->InitTest(L"%s : write %u bytes", lpFTE->lpDescription, bytesToWrite); 
        
    for(repeat = 0; repeat < g_cRepeat; repeat++) {

        if(fLinearWrite) {
            sector = repeat % (di.di_total_sectors - g_cSectors);
        } else {
            sector = Random() % (di.di_total_sectors - g_cSectors);
        }

        if(!ReadWritePerf(hDisk, sector, g_cSectors, bytesToWrite)) {
            FAIL("ReadWritePerf()");
            goto done;
        }
    }
        
done:
    if(INVALID_HANDLE_VALUE != hDisk) {
        VERIFY(CloseHandle(hDisk));
    }
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

    HANDLE hDisk = INVALID_HANDLE_VALUE;
    DISK_INFO di;
    DWORD sector;
    DWORD percentToFill = lpFTE->dwUserData;
    DWORD bytesToWrite;
    DWORD repeat;
    BOOL fLinearWrite = TRUE;


    LOG(L"opening a handle to storage device \"%s...\"", g_szDisk);
    LOG(L"CreateFile(\"%s\", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL", g_szDisk);
    hDisk = CreateFile(g_szDisk, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE_VALUE == hDisk) {
        ERRFAIL("CreateFile()");
        goto done;
    }    

    // get disk info so we know the sector size
    //
    if(!Dsk_GetDiskInfo(hDisk, &di)) {
        ERRFAIL("Dsk_GetDiskInfo()");
        goto done;
    }

    // fill the disk the requested amount
    //
    if(!Util_FillDiskRandom(hDisk, percentToFill)) {
        FAIL("Util_FillDiskRandom()");
        goto done;
    }

    // calculate the total bytes to write based on sector size and 
    // requested number of sectors per write
    //
    bytesToWrite = g_cSectors * di.di_bytes_per_sect;
    
    LOG(L"performing %u %u byte writes to empty FLASH randomly filled to %02u%% capacity", 
        g_cRepeat, bytesToWrite, percentToFill);
    
    LOG(L"%s : write/read %u bytes", lpFTE->lpDescription, bytesToWrite);
    if(g_pReadPerfLog) g_pReadPerfLog->InitTest(L"%s : read %u bytes", lpFTE->lpDescription, bytesToWrite); 
    if(g_pWritePerfLog) g_pWritePerfLog->InitTest(L"%s : write %u bytes", lpFTE->lpDescription, bytesToWrite); 

    for(repeat = 0; repeat < g_cRepeat; repeat++) {

        if(fLinearWrite) {
            sector = repeat % (di.di_total_sectors - g_cSectors);
        } else {
            sector = Random() % (di.di_total_sectors - g_cSectors);
        }

        if(!ReadWritePerf(hDisk, sector, g_cSectors, bytesToWrite)) {
            FAIL("ReadWritePerf()");
            goto done;
        }
    }
        
done:
    if(INVALID_HANDLE_VALUE != hDisk) {
        VERIFY(CloseHandle(hDisk));
    }
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

    HANDLE hDisk = INVALID_HANDLE_VALUE;
    DISK_INFO di;
    DWORD sector;
    DWORD percentToFill = lpFTE->dwUserData;
    DWORD bytesToWrite;
    DWORD repeat;
    BOOL fLinearWrite = TRUE;


    LOG(L"opening a handle to storage device \"%s...\"", g_szDisk);
    LOG(L"CreateFile(\"%s\", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL", g_szDisk);
    hDisk = CreateFile(g_szDisk, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE_VALUE == hDisk) {
        ERRFAIL("CreateFile()");
        goto done;
    }    

    // get disk info so we know the sector size
    //
    if(!Dsk_GetDiskInfo(hDisk, &di)) {
        ERRFAIL("Dsk_GetDiskInfo()");
        goto done;
    }

    // fill the disk the requested amount
    //
    if(!Util_FillDiskLinear(hDisk, 100)) {
        FAIL("Util_FillDiskLinear()");
        goto done;
    }

    // free the requested amount
    //
    if(!Util_FreeDiskRandom(hDisk, 100-percentToFill)) {
        FAIL("Util_FreeDiskRandom()");
        goto done;
    }

    LOG(L"disk is now approximately %02u%% full", percentToFill);

    // calculate the total bytes to write based on sector size and 
    // requested number of sectors per write
    //
    bytesToWrite = g_cSectors * di.di_bytes_per_sect;
    
    LOG(L"performing %u %u byte writes to full FLASH randomly freed to %02u%% capacity", 
        g_cRepeat, bytesToWrite, percentToFill);
    
    LOG(L"%s : write/read %u bytes", lpFTE->lpDescription, bytesToWrite);
    if(g_pReadPerfLog) g_pReadPerfLog->InitTest(L"%s : read %u bytes", lpFTE->lpDescription, bytesToWrite); 
    if(g_pWritePerfLog) g_pWritePerfLog->InitTest(L"%s : write %u bytes", lpFTE->lpDescription, bytesToWrite); 
        
    for(repeat = 0; repeat < g_cRepeat; repeat++) {

        if(fLinearWrite) {
            sector = repeat % (di.di_total_sectors - g_cSectors);
        } else {
            sector = Random() % (di.di_total_sectors - g_cSectors);
        }

        if(!ReadWritePerf(hDisk, sector, g_cSectors, bytesToWrite)) {
            FAIL("ReadWritePerf()");
            goto done;
        }
    }
        
done:
    if(INVALID_HANDLE_VALUE != hDisk) {
        VERIFY(CloseHandle(hDisk));
    }
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

    HANDLE hDisk = INVALID_HANDLE_VALUE;
    DISK_INFO di;
    DWORD sector;
    DWORD percentToFill = lpFTE->dwUserData;
    DWORD bytesToWrite;
    DWORD repeat;
    BOOL fLinearWrite = TRUE;


    LOG(L"opening a handle to storage device \"%s...\"", g_szDisk);
    LOG(L"CreateFile(\"%s\", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL", g_szDisk);
    hDisk = CreateFile(g_szDisk, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE_VALUE == hDisk) {
        ERRFAIL("CreateFile()");
        goto done;
    }    

    // get disk info so we know the sector size
    //
    if(!Dsk_GetDiskInfo(hDisk, &di)) {
        ERRFAIL("Dsk_GetDiskInfo()");
        goto done;
    }

    // fill the disk the requested amount
    //
    if(!Util_FillDiskMaxFragment(hDisk, percentToFill)) {
        FAIL("Util_FillDiskMaxFragment()");
        goto done;
    }

    // calculate the total bytes to write based on sector size and 
    // requested number of sectors per write
    //
    bytesToWrite = g_cSectors * di.di_bytes_per_sect;  
    
    LOG(L"performing %u %u byte writes to empty FLASH filled w/maximum fragmentation to %02u%% capacity", 
        g_cRepeat, bytesToWrite, percentToFill);

    LOG(L"%s : write/read %u bytes", lpFTE->lpDescription, bytesToWrite);
    if(g_pReadPerfLog) g_pReadPerfLog->InitTest(L"%s : read %u bytes", lpFTE->lpDescription, bytesToWrite); 
    if(g_pWritePerfLog) g_pWritePerfLog->InitTest(L"%s : write %u bytes", lpFTE->lpDescription, bytesToWrite);         
    
    for(repeat = 0; repeat < g_cRepeat; repeat++) {

        if(fLinearWrite) {
            sector = repeat % (di.di_total_sectors - g_cSectors);
        } else {
            sector = Random() % (di.di_total_sectors - g_cSectors);
        }

        if(!ReadWritePerf(hDisk, sector, g_cSectors, bytesToWrite)) {
            FAIL("ReadWritePerf()");
            goto done;
        }
    }
        
done:  
    if(INVALID_HANDLE_VALUE != hDisk) {
        VERIFY(CloseHandle(hDisk));
    }
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

    HANDLE hDisk = INVALID_HANDLE_VALUE;
    DISK_INFO di;
    DWORD sector;
    DWORD percentToFill = lpFTE->dwUserData;
    DWORD bytesToWrite;
    DWORD repeat;
    BOOL fLinearWrite = TRUE;


    LOG(L"opening a handle to storage device \"%s...\"", g_szDisk);
    LOG(L"CreateFile(\"%s\", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL", g_szDisk);
    hDisk = CreateFile(g_szDisk, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE_VALUE == hDisk) {
        ERRFAIL("CreateFile()");
        goto done;
    }    

    // get disk info so we know the sector size
    //
    if(!Dsk_GetDiskInfo(hDisk, &di)) {
        ERRFAIL("Dsk_GetDiskInfo()");
        goto done;
    }

    // fill the disk the requested amount
    //
    if(!Util_FillDiskLinear(hDisk, 100)) {
        FAIL("Util_FillDiskLinear()");
        goto done;
    }

    // free the requested amount
    //
    if(!Util_FreeDiskMaxFragment(hDisk, 100-percentToFill)) {
        FAIL("Util_FreeDiskMaxFragment()");
        goto done;
    }

    LOG(L"disk is now approximately %02u%% full", percentToFill);

    // calculate the total bytes to write based on sector size and 
    // requested number of sectors per write
    //
    bytesToWrite = g_cSectors * di.di_bytes_per_sect;
    
    LOG(L"performing %u %u byte writes to full FLASH freed w/maximum fragmentation to %02u%% capacity", 
        g_cRepeat, bytesToWrite, percentToFill);

    LOG(L"%s : write/read %u bytes", lpFTE->lpDescription, bytesToWrite);
    if(g_pReadPerfLog) g_pReadPerfLog->InitTest(L"%s : read %u bytes", lpFTE->lpDescription, bytesToWrite); 
    if(g_pWritePerfLog) g_pWritePerfLog->InitTest(L"%s : write %u bytes", lpFTE->lpDescription, bytesToWrite); 
        
    for(repeat = 0; repeat < g_cRepeat; repeat++) {

        if(fLinearWrite) {
            sector = repeat % (di.di_total_sectors - g_cSectors);
        } else {
            sector = Random() % (di.di_total_sectors - g_cSectors);
        }
        
        if(!ReadWritePerf(hDisk, sector, g_cSectors, bytesToWrite)) {
            FAIL("ReadWritePerf()");
            goto done;
        }
    }
        
done:
    if(INVALID_HANDLE_VALUE != hDisk) {
        VERIFY(CloseHandle(hDisk));
    }
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
