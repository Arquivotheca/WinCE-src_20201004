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
/*
-------------------------------------------------------------------------------

Module Name:

    mtrwtest.cpp

Abstract:

    This file contains the test procedures for mtwr (Multi Thread Read/
    Write).

Functions:

    DWORD DataVerify( HANDLE hDsk, DWORD dwBytesPerSector )
    VOID ParseCommandLine( VOID )
    TESTPROCAPI TestProc( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )

Notes:

-------------------------------------------------------------------------------
*/

#include "main.h"
#include "globals.h"
#include "common.h"
#include "utility.h"


// ----------------------------------------------------------------------------
// Globals
// ----------------------------------------------------------------------------

// Global constants (default parameters)



TEST_PROPERTIES g_sProperties;


// ----------------------------------------------------------------------------
// TestProcs's
// ----------------------------------------------------------------------------


/*-------------------------------------------------------------------------------

Function:

    DataVerify

Description:

    This routine verifies that the sector(s) that were written to during this
    test contain valid data.

Arguments:

    This routine accepts three arguments:

    HANDLE hDsk: A handle to DSK1:. Rather than opening another handle, this
        routine will use the handle of the last thread to complete its writing.

    DWORD dwBytesPerSector: sDiskInfo.di_bytes_per_sect

    DWORD dwThreadNumber: Number of the calling thread (1-xxx...)

Return Value:

    TPR_PASS if the data is valid.
    TPR_FAIL if the data is invalid.

------------------------------------------------------------------------------- */


DWORD
DataVerify( HANDLE hDsk, DWORD dwBytesPerSector, DWORD dwThreadNumber ) {

    const DWORD* pData;
    DWORD dwFill;
    BOOL fRet;
    SG_REQ sSg;
    DWORD dwBytesReturned;
    DWORD dwBufferSize = 0;

    if (S_OK != DWordMult(g_sProperties.dwNumberOfSectors, dwBytesPerSector, &dwBufferSize))
    {
        // Integer overflow
        g_pKato->Log( LOG_FAIL, TEXT("[%d] FAILED. Integer overflow occured when calculating write buffer size"), dwThreadNumber);
        return TPR_FAIL;
    }

    // Populate the scatter/gather request structure
    sSg.sr_start = g_sProperties.dwWriteSector + (dwThreadNumber-1) * g_sProperties.dwNumberOfSectors;
    sSg.sr_num_sec = g_sProperties.dwNumberOfSectors;
    sSg.sr_num_sg = 1;
    sSg.sr_callback = NULL;
    sSg.sr_sglist[0].sb_len = dwBufferSize;
    sSg.sr_sglist[0].sb_buf = (PUCHAR)LocalAlloc( LPTR, sSg.sr_sglist[0].sb_len );

    // Successful memory allocation?
    if ( NULL == sSg.sr_sglist[0].sb_buf ) {

        // Could not allocate buffer
        g_pKato->Log( LOG_FAIL, TEXT("[%d] FAIL. Could not allocate r/w buffer. GetLastError()=%lu."),
            dwThreadNumber,
            GetLastError() );

        // Release handle. Do not continue with test.
        CloseHandle( hDsk );
        return TPR_FAIL;
    }

#ifdef DETAIL
    g_pKato->Log( LOG_DETAIL, TEXT("Allocated buffer: 0x%08X - 0x%08X"),
        sSg.sr_sglist[0].sb_buf,
        sSg.sr_sglist[0].sb_buf + dwBytesPerSector - 1 );
#endif

    // Verify the contents
    g_pKato->Log( LOG_COMMENT, TEXT("[%d] Verifying sectors %d - %d..."),
        dwThreadNumber,
        sSg.sr_start,
        sSg.sr_start + sSg.sr_num_sec - 1 );

    // Read in a sector
    fRet = DeviceIoControl( hDsk,
        DISK_IOCTL_READ,
        & sSg,
        sizeof( SG_REQ ),
        NULL,
        NULL,
        & dwBytesReturned,
        NULL );

    if ( !fRet ) {

        // Encountered an error during the read
        g_pKato->Log( LOG_FAIL, TEXT("FAIL: Verify(...) unable to read from DSK1: GetLastError()=%lu."),
            GetLastError() );

        // Do not continue test
        return TPR_FAIL;
    }

    // Validate buffer contents.
    dwFill = dwBufferSize / 4;
    pData = (DWORD*)sSg.sr_sglist[0].sb_buf;
    while ( dwFill ) {

        if ( (*pData) != dwFill ) {

            // Invalid data
            g_pKato->Log( LOG_FAIL, TEXT("[%d] FAIL: Invalid data at 0x%08X. Expected: %d, Actual: %d"),
                dwThreadNumber,
                pData - (DWORD*)sSg.sr_sglist[0].sb_buf,
                dwFill,
                *pData );

            // Do not continue testing
            return TPR_FAIL;
        }
        pData++;
        dwFill--;
    }

    return TPR_PASS;
}





/*-------------------------------------------------------------------------------

Function:

    TestProc

Description:

    mtrw (Multi Thread Read/Write) will create numerous threads, and let each
    thread attempt to simultaneously write to the same sector on the disk many
    times.

-------------------------------------------------------------------------------*/

TESTPROCAPI
TestProc( UINT uMsg,
          TPPARAM tpParam,
          LPFUNCTION_TABLE_ENTRY lpFTE
          ) {

    if(uMsg == TPM_EXECUTE)
        if(!Zorch(g_pShellInfo->szDllCmdLine))
        {
            Zorchage(g_pKato);
            return TPR_SKIP;
        }

        // Check message to see why we have been called
    if ( TPM_QUERY_THREAD_COUNT == uMsg ) {



        // Populate the test properties structure with default values
        g_sProperties.dwNumberOfThreads = g_dwNumThreads;
        g_sProperties.dwNumberOfWrites = g_dwNumWrites;
        g_sProperties.dwNumberOfSectors = g_dwNumSectors;
        g_sProperties.dwWriteSector = g_dwWriteSector;

        //use number of threads from ft.h, unless user has specified that they want a custom value
        if(g_dwNumThreads == 0)
        {
            g_sProperties.dwNumberOfThreads  =  lpFTE->dwUserData;
        }
        else
        {
          g_sProperties.dwNumberOfThreads = g_dwNumThreads;
        }
        // Create lpFTE->dwUserData separate threads.
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = g_sProperties.dwNumberOfThreads;

        return TPR_HANDLED;
    }

    //use number of threads from ft.h, unless user has specified that they want a custom value
    if(g_dwNumThreads == 0)
    {
        g_sProperties.dwNumberOfThreads  =  lpFTE->dwUserData;
    }
    else
    {
      g_sProperties.dwNumberOfThreads = g_dwNumThreads;
    }


    // Display test properties
    g_pKato->Log( LOG_DETAIL, TEXT("Test parameters: Threads: %d, Number of writes: %d, Number of sectors/write: %d, Sector: %d, Starting disk: %s"),
        g_sProperties.dwNumberOfThreads,
        g_sProperties.dwNumberOfWrites,
        g_sProperties.dwNumberOfSectors,
        g_sProperties.dwWriteSector,
       !g_szDiskName[ 0 ] ? TEXT( "Not specified" ) : g_szDiskName );

    //
    // Local variable declarations
    //
    DWORD dwThreadNumber = ((TPS_EXECUTE*)tpParam)->dwThreadNumber;
    DWORD dwRet = TPR_PASS;
    DWORD dwBytesReturned;
    HANDLE hDsk;
    DISK_INFO sDiskInfo;
    BOOL fRet;
    DWORD dwNumSectorsOnDisk;
    DWORD dwBytesPerSector;
    SG_REQ sSg;
    DWORD dwFill;
    DWORD* p;
    DWORD i;
    DWORD dwBufferSize = 0;

    if(g_szProfile[0])
    {
        if(g_szDiskName[0])
        {
            g_pKato->Log( LOG_FAIL,_T("WARNING:  Overriding /disk option with /profile option\n"));
        }
        hDsk = OpenDiskHandleByProfileRW(g_pKato,g_fOpenAsStore,g_szProfile,g_fOpenAsPartition);
    }
    else
    {
        if ( !g_szDiskName[ 0 ] )
            StringCchPrintf( g_szDiskName, MAX_PATH, TEXT("DSK%u:"), FindNextDevice( 0 ) );

        hDsk = OpenDiskHandleByDiskNameRW( g_pKato,g_fOpenAsStore, g_szDiskName,g_fOpenAsPartition);

    }



    // Able to open a disk
    if ( INVALID_HANDLE_VALUE == hDsk ) {

        // Error opening device
        g_pKato->Log( LOG_FAIL, TEXT("[%d] FAIL: Unable to open a disk GetLastError()=%lu."),
            dwThreadNumber,
            GetLastError() );

        // Do not continue with test
        return TPR_FAIL;
    }
    else {

        // Successfully opened DSKn:
        if(g_szProfile[0])
            g_pKato->Log( LOG_DETAIL, TEXT("[%d] Opened disk by profile %s:. hDsk=0x%08X"),
                          dwThreadNumber,
                          g_szProfile ,
                          hDsk );
        else
            g_pKato->Log( LOG_DETAIL, TEXT("[%d] Opened %s:. hDsk=0x%08X"),
                          dwThreadNumber,
                          g_szDiskName ,
                          hDsk );
    }

    // Get information about DSKn:
    fRet = DeviceIoControl( hDsk,
        DISK_IOCTL_GETINFO,
        & sDiskInfo,
        sizeof( DISK_INFO ),
        NULL,
        NULL,
        & dwBytesReturned,
        NULL );

    if ( fRet ) {

        // Successful call to ...GETINFO
        dwNumSectorsOnDisk = sDiskInfo.di_total_sectors;
        dwBytesPerSector = sDiskInfo.di_bytes_per_sect;

        if ( g_sProperties.dwWriteSector > dwNumSectorsOnDisk ) {

            // User passed an invalid write sector
            g_pKato->Log( LOG_SKIP, TEXT("Invalid write sector: %d > number of sectors on disk, %d. Skipping test."),
                g_sProperties.dwWriteSector,
                dwNumSectorsOnDisk );
            CloseHandle( hDsk );
            return TPR_SKIP;
        }
    }
    else {

        // Error in call to ...GETINFO
        g_pKato->Log( LOG_FAIL, TEXT("[%d] FAIL: Unable to GETINFO. GetLastError()=%lu."),
            dwThreadNumber,
            GetLastError() );

        // Do not continue with test
        CloseHandle( hDsk );
        return TPR_FAIL;
    }

    // Populate the scatter/gather request structure
    if (S_OK != DWordMult(g_sProperties.dwNumberOfSectors, dwBytesPerSector, &dwBufferSize))
    {
        // Integer overflow
        g_pKato->Log( LOG_FAIL, TEXT("[%d] FAIL. Integer overflow while calculating r/w buffer size"),
            dwThreadNumber);
        return TPR_FAIL;
    }

    sSg.sr_num_sg = 1;
    sSg.sr_num_sec = g_sProperties.dwNumberOfSectors;
    sSg.sr_start = g_sProperties.dwWriteSector + (dwThreadNumber - 1) * (g_sProperties.dwNumberOfSectors);
    sSg.sr_callback = NULL;
    sSg.sr_sglist[0].sb_len = dwBufferSize;
    sSg.sr_sglist[0].sb_buf = (PUCHAR)LocalAlloc( LPTR, sSg.sr_sglist[0].sb_len );

    // Successful memory allocation?
    if ( NULL == sSg.sr_sglist[0].sb_buf ) {

        // Could not allocate buffer
        g_pKato->Log( LOG_FAIL, TEXT("[%d] FAIL. Could not allocate r/w buffer. GetLastError()=%lu."),
            dwThreadNumber,
            GetLastError() );

        // Release handle. Do not continue with test.
        CloseHandle( hDsk );
        return TPR_FAIL;
    }

    // Fill the r/w buffer with decreasing set of DWORDS
    dwFill = sSg.sr_sglist[0].sb_len / 4;
    p = (DWORD*)sSg.sr_sglist[0].sb_buf;
    while ( dwFill ) {
        *p = dwFill;
        dwFill--;
        p++;
    }

    // Write this buffer to the disk NUMWRITES times
    g_pKato->Log( LOG_COMMENT, TEXT("[%d] Writing sectors %d - %d..."),
        dwThreadNumber,
        sSg.sr_start,
        sSg.sr_start + sSg.sr_num_sec - 1 );
    for ( i = 0; i < g_sProperties.dwNumberOfWrites; i++ ) {

        // Send the buffer to the memory card
        fRet = DeviceIoControl( hDsk,
            DISK_IOCTL_WRITE,
            & sSg,
            sizeof( SG_REQ ),
            NULL,
            NULL,
            & dwBytesReturned,
            NULL );

        if ( fRet ) {

#ifdef DETAIL
            // Successful write.
            g_pKato->Log( LOG_COMMENT, TEXT("[%d] Write # %d complete"),
                dwThreadNumber,
                i );
#endif
        }
        else {

            // Failure in call to ...WRITE.
            g_pKato->Log( LOG_FAIL, TEXT("[%d] FAIL: Error encountered in ...WRITE # %d. GetLastError()=%lu."),
                dwThreadNumber,
                i,
                GetLastError() );

            g_pKato->Log( LOG_COMMENT, TEXT("[%d] Aborting test..."),
                dwThreadNumber );

            // Continue with test.
            return TPR_FAIL;
        }
    }

    // Verify that no data corruption occurred. TPR_PASS returns a value of 3, and TPR_FAIL
    // returns a value of 4.
    if ( TPR_FAIL == dwRet || TPR_FAIL == DataVerify( hDsk, dwBytesPerSector, dwThreadNumber ) )
        dwRet = TPR_FAIL;

    // Release resources
    CloseHandle( hDsk );
    LocalFree( sSg.sr_sglist[0].sb_buf );

    // Return control to tux w/ status of test
    return dwRet;
}


