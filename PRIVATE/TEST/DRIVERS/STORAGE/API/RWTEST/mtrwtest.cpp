//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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


// ----------------------------------------------------------------------------
// Globals
// ----------------------------------------------------------------------------

// Global constants (default parameters)

#define NUMTHREADS 1
#define NUMSECTORS 1
#define WRITESECTOR 100
#define NUMWRITES 1000

// Global test properties info structure

typedef struct _TEST_PROPERTIES {
    DWORD dwNumberOfThreads;
    DWORD dwNumberOfWrites;
    DWORD dwNumberOfSectors;
    DWORD dwWriteSector;
} TEST_PROPERTIES, *PTEST_PROPERTIES;

TEST_PROPERTIES g_sProperties;


// ----------------------------------------------------------------------------
// TestProcs's
// ----------------------------------------------------------------------------


#if 0
-------------------------------------------------------------------------------

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

-------------------------------------------------------------------------------
#endif

DWORD
DataVerify( HANDLE hDsk, DWORD dwBytesPerSector, DWORD dwThreadNumber ) {

    DWORD dwRet = TPR_PASS;
    DWORD* pData;
    DWORD dwFill;
    BOOL fRet;
    SG_REQ sSg;
    DWORD dwBytesReturned;

    // Populate the scatter/gather request structure
    sSg.sr_start = g_sProperties.dwWriteSector + (dwThreadNumber-1) * g_sProperties.dwNumberOfSectors;
    sSg.sr_num_sec = g_sProperties.dwNumberOfSectors;
    sSg.sr_num_sg = 1;
    sSg.sr_callback = NULL;
    sSg.sr_sglist[0].sb_len = g_sProperties.dwNumberOfSectors * dwBytesPerSector;
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
    dwFill = g_sProperties.dwNumberOfSectors * dwBytesPerSector / 4;
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


#if 0
-------------------------------------------------------------------------------

Function:

    ParseCommandLine

Description:

    This routine will parse the command line for user-defined test parameters.
    To specify test parameters, use the following command line:

        s tux -o -d mtrw -x1999 -c"aaa bbb ccc ddd"

    where aaa is the number of threads to create, bbb is the number of writes
    per thread, ccc is the number of sectors per write, and ddd is the sector
    that is to be written. Use the -x1999 switch to prevent Tux from running
    the entire set of test cases.

    All of these parameters are optional. You may specify as many or as few as
    you like. If you specify fewer than all the parameters, the program will
    use default values for the last parameters in the list.
    
    To run a test with default values, use the following command line:

        s tux -o -d mtrw -x1999 -c"d"


-------------------------------------------------------------------------------
#endif

VOID
ParseCommandLine( VOID ) {

    LPCTSTR szCmdLine = g_pShellInfo->szDllCmdLine;
    LPCTSTR szCmdLineEnd = g_pShellInfo->szDllCmdLine + _tcslen( szCmdLine );
    LONG lTemp;

    if ( szCmdLine[0] == 'd' ) {

        // Use default values
        g_pKato->Log( LOG_DETAIL, TEXT("Using default values: Threads: %d, Writes: %d, Sectors/Write: %d, Write Sector: %d"),
            NUMTHREADS,
            NUMWRITES,
            NUMSECTORS,
            WRITESECTOR );
        return;
    }

    
    // If szCmdLine points to a string that can be converted to a valid number,
    if ( lTemp = _wtol( szCmdLine ) )
        
        // Use this first parameter for dwNumberOfThreads
        g_sProperties.dwNumberOfThreads = (DWORD)lTemp;
    else
        return;

    // Advance szCmdLine past the parameter (to the space)
    while ( ' ' != *szCmdLine && szCmdLine < szCmdLineEnd ) {
        szCmdLine++;
    }
    // Advance szCmdLine to the first character of the next parameter
    szCmdLine++;

    if ( lTemp = _wtol( szCmdLine ) )
        g_sProperties.dwNumberOfWrites = (DWORD)lTemp;
    else
        return;

    while ( ' ' != *szCmdLine && szCmdLine < szCmdLineEnd ) {
        szCmdLine++;
    }
    szCmdLine++;

    if ( lTemp = _wtol( szCmdLine ) )
        g_sProperties.dwNumberOfSectors = (DWORD)lTemp;
    else
        return;

    while ( ' ' != *szCmdLine && szCmdLine < szCmdLineEnd ) {
        szCmdLine++;
    }
    szCmdLine++;

    if ( lTemp = _wtol( szCmdLine ) )
        g_sProperties.dwWriteSector = (DWORD)lTemp;
    else
        return;

}


#if 0
-------------------------------------------------------------------------------

Function:

    TestProc

Description:

    mtrw (Multi Thread Read/Write) will create numerous threads, and let each
    thread attempt to simultaneously write to the same sector on the disk many
    times.

-------------------------------------------------------------------------------
#endif

TESTPROCAPI
TestProc( UINT uMsg,
          TPPARAM tpParam,
          LPFUNCTION_TABLE_ENTRY lpFTE
          ) {

    // Check message to see why we have been called
    if ( TPM_QUERY_THREAD_COUNT == uMsg ) {
        
        // Populate the test properties structure with default values
        g_sProperties.dwNumberOfThreads = NUMTHREADS;
        g_sProperties.dwNumberOfWrites = NUMWRITES;
        g_sProperties.dwNumberOfSectors = NUMSECTORS;
        g_sProperties.dwWriteSector = WRITESECTOR;

        // Now check for user values to over-ride defaults
        if ( 999 == lpFTE->dwUserData ) {

            // Populate properties structure from command line
            ParseCommandLine();
        }
        else {

            // No command line parameters. Number of threads = dwUserData
            g_sProperties.dwNumberOfThreads = lpFTE->dwUserData;
        }

        // Create lpFTE->dwUserData separate threads.
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = g_sProperties.dwNumberOfThreads;

        return TPR_HANDLED;
    }

    // Now check for user values to over-ride defaults
    if ( 999 == lpFTE->dwUserData ) {

        if ( *g_pShellInfo->szDllCmdLine ) {
            
            // Populate properties structure from command line
            ParseCommandLine();
        }
        else {

            // Did not specify any parameters for this test, so skip
            return TPR_SKIP;
        }
    }
    else {

        // No command line parameters. Number of threads = dwUserData
        g_sProperties.dwNumberOfThreads = lpFTE->dwUserData;
    }

    // Display test properties
    g_pKato->Log( LOG_DETAIL, TEXT("Test parameters: Threads: %d, Number of writes: %d, Number of sectors/write: %d, Sector: %d"),
        g_sProperties.dwNumberOfThreads,
        g_sProperties.dwNumberOfWrites,
        g_sProperties.dwNumberOfSectors,
        g_sProperties.dwWriteSector );

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
    TCHAR szTemp[16];

    hDsk = GetNextHandle(0, szTemp);

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

        // Successfully opened DSK1:
        g_pKato->Log( LOG_DETAIL, TEXT("[%d] Opened %s:. hDsk=0x%08X"),
            dwThreadNumber,
            szTemp,
            hDsk );
    }

    // Get information about DSK1:
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
    sSg.sr_num_sg = 1;
    sSg.sr_num_sec = g_sProperties.dwNumberOfSectors;
    sSg.sr_start = g_sProperties.dwWriteSector + (dwThreadNumber - 1) * (g_sProperties.dwNumberOfSectors);
    sSg.sr_callback = NULL;
    sSg.sr_sglist[0].sb_len = sSg.sr_num_sec * dwBytesPerSector;
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


