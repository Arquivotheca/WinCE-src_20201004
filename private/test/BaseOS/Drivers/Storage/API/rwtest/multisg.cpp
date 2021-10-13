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

    multisg.cpp
 
Abstract:

    This file contains the test procedures for testing a driver's ability to
    handle multiple scatter/gather buffers.

Functions:

    TESTPROCAPI MultiSG( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )

Notes:

-------------------------------------------------------------------------------
*/

#include "main.h"
#include "globals.h"
#include "common.h"
#include "utility.h"


// ----------------------------------------------------------------------------
// TestProc()'s
// ----------------------------------------------------------------------------


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  Function:
  
    MultiSG
    
  Description:

    MultiSG exercises the device driver's ability to handle multiple scatter/
    gather structures of different sizes.
            
  Arguments:
              
    Runs as a Tux .dll.
  
  Return Value:

    TPR_PASS or TPR_FAIL
                  
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

// Defining _MEMDETAIL will give an output of every DWORD that is being verified.
// #define _MEMDETAIL

typedef struct _TEST_CASE {
    DWORD dwNumThreads;         // Number of threads to execute
    DWORD dwNumBufs;            // Number of scatter/gather buffers
    DWORD dwStartSector;        // First sector to read/write
    DWORD dwSize[1];            // Pointer to an array of sizes for the s/g buffers
} TEST_CASE, *PTEST_CASE;

TESTPROCAPI
MultiSG( UINT uMsg,
         TPPARAM tpParam,
         LPFUNCTION_TABLE_ENTRY lpFTE ) {
    
    // Check message to see why we have been called
    switch ( uMsg ) {
    case TPM_EXECUTE:
        
        break;
        
    case TPM_QUERY_THREAD_COUNT:
        
        // Do something about threads
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 1;
        return TPR_HANDLED;
        
    default:
        
        // Program should never be here
        return TPR_NOT_HANDLED;
    }


	if(!Zorch(g_pShellInfo->szDllCmdLine))
	{
		Zorchage(g_pKato); 
		return TPR_SKIP; 
	}

    
    // Variable declarations
    HANDLE hDsk;
    DWORD dwRet = TPR_PASS;
    BOOL fRet;
    DISK_INFO sDiskInfo;
    PSG_REQ pSGReq;
    DWORD* p;
    PTEST_CASE lpTestCase;
    DWORD i;
    DWORD dwBytesReturned;
    DWORD dwNumBytesToRead;
    DWORD dwFill;
    DWORD dwTestNumber;    

    // Get a handle to a device by Profile or diskname
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


   
    if ( hDsk != INVALID_HANDLE_VALUE ) 
    {
        // Successfully opened device
        g_pKato->Log( LOG_COMMENT, TEXT("Opened %s, hDsk=0x%08X."),
			g_szDiskName[0] ? g_szDiskName : TEXT( "disk"), hDsk );

        // Get information about the disk
        fRet = DeviceIoControl( hDsk,
            DISK_IOCTL_GETINFO,
            & sDiskInfo,
            sizeof(DISK_INFO),
            NULL,
            NULL,
            &dwBytesReturned,
            NULL );
            
        // Able to get disk information?
        if ( !fRet ) 
        {        
            // Get Info failed
            g_pKato->Log( LOG_FAIL, TEXT("FAIL: Unable to get info, %s, hDsk=0x%08X, GetLastError()=%lu."),
                g_szDiskName[0] ? g_szDiskName : TEXT( "disk"),
                hDsk,
                GetLastError() );
            CloseHandle( hDsk );
            return TPR_FAIL;
        }
        else 
        {
            // Get Info successful; display basic information
            g_pKato->Log( LOG_DETAIL, TEXT("DISK INFORMATION FROM IOCTL") );
            g_pKato->Log( LOG_DETAIL, TEXT("...Total sectors:.......(%d)"), sDiskInfo.di_total_sectors );
            g_pKato->Log( LOG_DETAIL, TEXT("...Bytes Per Sector:....(%d)"), sDiskInfo.di_bytes_per_sect );
            g_pKato->Log( LOG_DETAIL, TEXT("...Cylinders:...........(%d)"), sDiskInfo.di_cylinders );
            g_pKato->Log( LOG_DETAIL, TEXT("...Heads:...............(%d)"), sDiskInfo.di_heads );
            g_pKato->Log( LOG_DETAIL, TEXT("...Sectors:.............(%d)"), sDiskInfo.di_sectors );
            g_pKato->Log( LOG_DETAIL, TEXT("...Total Bytes:.........(%d)"), sDiskInfo.di_total_sectors * sDiskInfo.di_bytes_per_sect);
            g_pKato->Log( LOG_DETAIL, TEXT("...Flags:...............(0x%08X)"), sDiskInfo.di_flags );
        }
    }
    else
    {
        // Failed to open a device
        g_pKato->Log( LOG_FAIL, TEXT("FAIL: Unable to open a device. GetLastError()=%lu."),
            GetLastError() );        
        return TPR_FAIL;
    }


    // Build set of all test cases
    // First element of the array is the number of buffers
    // Following elements are the size of the individual buffers
    DWORD Test1[] = {   1, 
                        2 * sDiskInfo.di_bytes_per_sect };  
    
    DWORD Test2[] = {   2, 
                        2 * sDiskInfo.di_bytes_per_sect, 
                        2 * sDiskInfo.di_bytes_per_sect };
    
    DWORD Test3[] = {   4,
                        2 * sDiskInfo.di_bytes_per_sect,
                        4 * sDiskInfo.di_bytes_per_sect,
                        8 * sDiskInfo.di_bytes_per_sect,
                        16 * sDiskInfo.di_bytes_per_sect };
    
    DWORD Test4[] = {   1, 
                        32 * sDiskInfo.di_bytes_per_sect };

    DWORD* AllTests[] = { Test1, Test2, Test3, Test4 };

    // Which test to run?
    // Test 1 is AllTests[0].....
    dwTestNumber = lpFTE->dwUserData - 1;
    
    // Populate the data structure for the specific test to run.
    DWORD dwTemp = 0;
    DWORD dwBufferSize = 0;
    if (!((S_OK == DWordMult((AllTests[dwTestNumber][0] - 1), sizeof(DWORD), &dwTemp)) &&
       (S_OK == DWordAdd(dwTemp, sizeof(TEST_CASE), &dwBufferSize))))
    {
        // Integer overflow
        g_pKato->Log( LOG_FAIL, TEXT("FAIL: Integer overflow while calculating buffer size") );
        return TPR_FAIL;
    }
    lpTestCase = (PTEST_CASE)LocalAlloc( LPTR, dwBufferSize );
    lpTestCase->dwNumBufs = AllTests[dwTestNumber][0];
    lpTestCase->dwNumThreads = 1;
    lpTestCase->dwStartSector = g_dwWriteSector;
    for ( i = 0; i < lpTestCase->dwNumBufs; i++ ) 
    {
        lpTestCase->dwSize[i] = AllTests[dwTestNumber][i+1];
    }

       
    
    // Allocate memory for the scatter/gather structures
    pSGReq = (PSG_REQ)LocalAlloc ( LPTR, sizeof(SG_REQ) + ( lpTestCase->dwNumBufs - 1 ) * sizeof(SG_BUF) );
    g_pKato->Log( LOG_DETAIL, TEXT("pSGReq: 0x%08X"), pSGReq );

    // Populate the scatter/gather structures with the appropriate test case
    pSGReq->sr_start = lpTestCase->dwStartSector;
    pSGReq->sr_num_sg = lpTestCase->dwNumBufs;
    pSGReq->sr_status = NULL;
    pSGReq->sr_callback = NULL;
    
    // Allocate memory for and fill the individual buffer structures
    dwNumBytesToRead = 0;
    for ( i = 0; i < lpTestCase->dwNumBufs; i++ ) 
    {
        pSGReq->sr_sglist[i].sb_buf = (PUCHAR)LocalAlloc( LPTR, lpTestCase->dwSize[i] );
        pSGReq->sr_sglist[i].sb_len = lpTestCase->dwSize[i];
        
        g_pKato->Log( LOG_DETAIL, TEXT("sglist[%d]: %d bytes @ 0x%08X"), 
            i, pSGReq->sr_sglist[i].sb_len, pSGReq->sr_sglist[i].sb_buf );

        
        // Fill the scatter/gather buffers.
        p = (DWORD*)pSGReq->sr_sglist[i].sb_buf;
        if ( pSGReq->sr_sglist[i].sb_len % 4 == 0 ) 
        {
            dwFill = pSGReq->sr_sglist[i].sb_len / 4;
        }
        else 
        {
            dwFill = pSGReq->sr_sglist[i].sb_len / 4 + 1;
        }

        g_pKato->Log( LOG_DETAIL, TEXT("Filling: %d"), dwFill );
        while (dwFill) 
        {
            *p = dwFill;
            dwFill--;
            p++;
        }

        // Keep track of the number of bytes to read
        dwNumBytesToRead += lpTestCase->dwSize[i];
    }
    
    // How many sectors to read?
    pSGReq->sr_num_sec = (dwNumBytesToRead + sDiskInfo.di_bytes_per_sect - 1) / sDiskInfo.di_bytes_per_sect;
    g_pKato->Log( LOG_DETAIL, TEXT("Number of sectors to read: %d"), pSGReq->sr_num_sec );
    
    // Display buffer info
    for ( i = 0; i < lpTestCase->dwNumBufs; i++ ) 
    {
        g_pKato->Log( LOG_DETAIL, TEXT("Buffer %d: %d bytes, 0x%08X - 0x%08X."),
            i,
            lpTestCase->dwSize[i],
            pSGReq->sr_sglist[i].sb_buf,
            pSGReq->sr_sglist[i].sb_buf + lpTestCase->dwSize[i] - 1 );
    }
    
    // Submit write request
    g_pKato->Log( LOG_COMMENT, TEXT("Submitting write request with SG_REQ:") );
    g_pKato->Log( LOG_DETAIL, TEXT("...Start Sector......................(%d)"), pSGReq->sr_start );
    g_pKato->Log( LOG_DETAIL, TEXT("...Number of Sectors.................(%d)"), pSGReq->sr_num_sec );
    g_pKato->Log( LOG_DETAIL, TEXT("...Number of Scatter/Gather Buffers..(%d)"), pSGReq->sr_num_sg );
    g_pKato->Log( LOG_DETAIL, TEXT("...Request Status....................(0x%08X)"), pSGReq->sr_status );
    g_pKato->Log( LOG_DETAIL, TEXT("...Callback..........................(0x%08X)"), pSGReq->sr_callback );
    for ( i = 0; i < lpTestCase->dwNumBufs; i++ ) 
    {
        g_pKato->Log( LOG_DETAIL, TEXT("...Scatter/Gather Buffer[%d]..........(0x%08X)"),i, pSGReq->sr_sglist[i].sb_buf);
        g_pKato->Log( LOG_DETAIL, TEXT("...Scatter/Gather Buffer[%d] Length...(%d)"),i, pSGReq->sr_sglist[i].sb_len );
    }
    

    fRet = DeviceIoControl( 
        hDsk,
        IOCTL_DISK_WRITE,
        pSGReq,
        sizeof(SG_REQ) + ( lpTestCase->dwNumBufs - 1 ) * sizeof(SG_BUF),
        NULL,
        NULL,
        &dwBytesReturned,
        NULL );
    
    // Was the write successful?
    if ( !fRet ) {
                
        // Encountered an error writing to the device
        
        // Was the error expected?
        if ( lpTestCase->dwNumBufs > 8 ) 
        {

            // Maximum number of s/g buffers allowed is 8; this should fail
            g_pKato->Log( LOG_COMMENT, TEXT("Write failed as expected. Exceeded max s/g buffers. GetLastError()=%lu."),
                GetLastError() );
        }
        else 
        {
            
            // Unexpected error writing to device
            g_pKato->Log( LOG_FAIL, TEXT("FAIL: Encountered an error while writing to %s, hDsk=0x%08X, GetLastError()=%lu. %d"),
                g_szDiskName[0] ? g_szDiskName : TEXT( "disk"),
                hDsk,
                GetLastError(), dwBytesReturned );
            dwRet = TPR_FAIL;
        }
    }
    
    // Clear the buffers
    for ( i = 0; i < lpTestCase->dwNumBufs; i++ ) {
        p = (DWORD*)pSGReq->sr_sglist[i].sb_buf;
        if ( pSGReq->sr_sglist[i].sb_len % 4 == 0 ) {
            dwFill = pSGReq->sr_sglist[i].sb_len / 4;
        }
        else {
            dwFill = pSGReq->sr_sglist[i].sb_len / 4 + 1;
        }
         while (dwFill) {
            *p = dwFill;
            dwFill--;
            p++;
         }
    }

    // Submit read request
    g_pKato->Log( LOG_COMMENT, TEXT("Submitting read request.") );
    fRet = DeviceIoControl( hDsk,
        DISK_IOCTL_READ,
        pSGReq,
        sizeof(SG_REQ) + ( lpTestCase->dwNumBufs - 1 ) * sizeof(SG_BUF),
        NULL,
        NULL,
        & dwBytesReturned,
        NULL );
    
    // Was the read successful?
    if ( !fRet ) {
        
        // Was the error expected?
        if ( lpTestCase->dwNumBufs > 8 ) {

            // Maximum number of s/g buffers allowed is 8; this should fail
            g_pKato->Log( LOG_COMMENT, TEXT("Read failed as expected. Exceeded max s/g buffers. GetLastError()=%lu."),
                GetLastError() );
        }
        else {
            
            // Unexpected error reading from device
            g_pKato->Log( LOG_FAIL, TEXT("FAIL: Encountered an error while reading to %s, hDsk=0x%08X, GetLastError()=%lu."),
                g_szDiskName[0] ? g_szDiskName : TEXT( "disk"),
                hDsk,
                GetLastError() );
            dwRet = TPR_FAIL;
        }
    }
    
    // Verify the read/write pair
    g_pKato->Log( LOG_COMMENT, TEXT("Verifying...") );
    for ( i = 0; i < lpTestCase->dwNumBufs; i++ ) {
        p = (DWORD*)pSGReq->sr_sglist[i].sb_buf;
        if ( pSGReq->sr_sglist[i].sb_len % 4 == 0 ) {
            dwFill = pSGReq->sr_sglist[i].sb_len / 4;
        }
        else {
            dwFill = pSGReq->sr_sglist[i].sb_len / 4 + 1;
        }
        while (dwFill) {
            if ( *p != dwFill) {
                g_pKato->Log( LOG_DETAIL, TEXT("Incorrect data at 0x%08X. Expected: 0x%08X, Received: 0x%08X."),
                    p,          // Address of incorrect data
                    dwFill,     // Expected data
                    *p );       // Actual data
                if ( lpTestCase->dwNumBufs <= 8 )
                    dwRet = TPR_FAIL;
            }

#ifdef _MEMDETAIL
            else {
                g_pKato->Log( LOG_DETAIL, TEXT("Address: 0x%08X  Expected: %d, Actual: %d"),
                    p,
                    dwFill,
                    *p );

            }
#endif

            dwFill--;
            p++;
        }
    }
    
    // Free memory
    g_pKato->Log( LOG_COMMENT, TEXT("Freeing memory.") );
    for ( i = 0; i < lpTestCase->dwNumBufs; i++ ) {
        LocalFree( pSGReq->sr_sglist[i].sb_buf );
    }
    LocalFree( pSGReq );
    LocalFree( lpTestCase );

    // Close the device
    CloseHandle( hDsk );
    return dwRet;
}
