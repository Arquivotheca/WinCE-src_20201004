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

    badparms.cpp
 
Abstract:

    This routine will check for a block device driver's ability to handle
    invalid parameters in read/write ioctl calls.
        
Functions:

    TESTPROCAPI BadParms( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )

Notes:

-------------------------------------------------------------------------------
*/

#include "main.h"
#include "globals.h"
#include "common.h"


// ----------------------------------------------------------------------------
// TestProc()'s
// ----------------------------------------------------------------------------

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  Function:

    BadParms

  Description:

    This routine will check for a block device driver's ability to handle
    invalid parameters in read/write ioctl calls.

  Arguments:

    Runs as a Tux .dll

  Return Value:

    TPR_PASS or TPR_FAIL

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#define SAME 1
#define ZERO 2

TESTPROCAPI BadParms(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {

    // Check message to see why we have been called
    if ( uMsg != TPM_EXECUTE ) {

        // This test does not run on multiple threads
        return TPR_NOT_HANDLED;
    }

    // Local variable declarations
    HANDLE hDsk;
    HLOCAL hBuffer_R;
    HLOCAL hBuffer_W;
    HLOCAL hBuffer_C;
    DWORD dwBytesReturned;
    DISK_INFO sDiskInfo;
    SG_REQ sSG_R;
    SG_REQ sSG_W;
    SG_REQ sSG_C;
    DWORD dwRet = TPR_PASS;
    TCHAR szTemp[16];
    BOOL fRet;
    DWORD dwFill;
    DWORD* p;
    DWORD dwExpected;
    
    // Open device
    hDsk = GetNextHandle( 0, szTemp );

    if ( !hDsk ) {

        // Unable to open device
        g_pKato->Log( LOG_FAIL, TEXT("BadParms FAIL: Unable to open a device. GetLastError()=%lu."),
            GetLastError() );

        return TPR_FAIL;
    }

    g_pKato->Log( LOG_COMMENT, TEXT("BadParms: Opened %s, hDsk=0x%08x."),
        szTemp,
        hDsk );

    // Get device information
    fRet = DeviceIoControl( hDsk,
        DISK_IOCTL_GETINFO,
        & sDiskInfo,
        sizeof( DISK_INFO ),
        NULL,
        NULL,
        & dwBytesReturned,
        NULL );

    if ( !fRet ) {

        // Error in call to ...GETINFO
        g_pKato->Log( LOG_FAIL, TEXT("BadParms FAIL: Unable to GETINFO. GetLastError()=%lu."),
            GetLastError() );

        // Do not continue with test
        CloseHandle( hDsk );
        return TPR_FAIL;
    }
    
    // Allocate space for r/w buffer (size of one sector)
    hBuffer_R = LocalAlloc( LPTR, sDiskInfo.di_bytes_per_sect );
    hBuffer_W = LocalAlloc( LPTR, sDiskInfo.di_bytes_per_sect );
    hBuffer_C = LocalAlloc( LPTR, sDiskInfo.di_bytes_per_sect );

    // Fill write buffer with decreasing series of DWORDs
    dwFill = sDiskInfo.di_bytes_per_sect / 4;
    p = (DWORD *)hBuffer_W;

    while (dwFill) {
        *p = dwFill;
        dwFill--;
        p++;
    }
    
    // Clear read buffer
    memset( hBuffer_R, 0, sDiskInfo.di_bytes_per_sect );

    switch( lpFTE->dwUserData ) {
    case 1:

        //
        // Number of sectors = 0
        //

        // Populate scatter/gather structures
        sSG_W.sr_start = 100;
        sSG_W.sr_num_sec = 0;
        sSG_W.sr_num_sg = 1;
        sSG_W.sr_callback = NULL;
        sSG_W.sr_sglist[0].sb_buf = (PUCHAR)hBuffer_W;
        sSG_W.sr_sglist[0].sb_len = sDiskInfo.di_bytes_per_sect;

        // Populate scatter/gather structures
        sSG_R.sr_start = 100;
        sSG_R.sr_num_sec = 0;
        sSG_R.sr_num_sg = 1;
        sSG_R.sr_callback = NULL;
        sSG_R.sr_sglist[0].sb_buf = (PUCHAR)hBuffer_R;
        sSG_R.sr_sglist[0].sb_len = sDiskInfo.di_bytes_per_sect;

        dwExpected = ZERO;

        break;
    
    case 2:

        //
        // Number of scatter/gather buffers = 0
        //
        
        // Populate scatter/gather structures
        sSG_W.sr_start = 100;
        sSG_W.sr_num_sec = 1;
        sSG_W.sr_num_sg = 0;
        sSG_W.sr_callback = NULL;
        sSG_W.sr_sglist[0].sb_buf = (PUCHAR)hBuffer_W;
        sSG_W.sr_sglist[0].sb_len = sDiskInfo.di_bytes_per_sect;

        sSG_R.sr_start = 100;
        sSG_R.sr_num_sec = 1;
        sSG_R.sr_num_sg = 0;
        sSG_R.sr_callback = NULL;
        sSG_R.sr_sglist[0].sb_buf = (PUCHAR)hBuffer_R;
        sSG_R.sr_sglist[0].sb_len = sDiskInfo.di_bytes_per_sect;

        dwExpected = ZERO;

        break;
    
    case 3:

        //
        // Pointer to scatter/gather buffer = NULL
        //
        
        // Populate scatter/gather structures
        sSG_W.sr_start = 100;
        sSG_W.sr_num_sec = 1;
        sSG_W.sr_num_sg = 1;
        sSG_W.sr_callback = NULL;
        sSG_W.sr_sglist[0].sb_buf = NULL;
        sSG_W.sr_sglist[0].sb_len = sDiskInfo.di_bytes_per_sect;

        sSG_R.sr_start = 100;
        sSG_R.sr_num_sec = 1;
        sSG_R.sr_num_sg = 1;
        sSG_R.sr_callback = NULL;
        sSG_R.sr_sglist[0].sb_buf = NULL;
        sSG_R.sr_sglist[0].sb_len = sDiskInfo.di_bytes_per_sect;

        dwExpected = ZERO;

        break;
    
    case 4:

        //
        // Scatter/gather buffer length = 0
        //
        
        // Populate scatter/gather structures
        sSG_W.sr_start = 100;
        sSG_W.sr_num_sec = 1;
        sSG_W.sr_num_sg = 1;
        sSG_W.sr_callback = NULL;
        sSG_W.sr_sglist[0].sb_buf = (PUCHAR)hBuffer_W;
        sSG_W.sr_sglist[0].sb_len = 0;

        sSG_R.sr_start = 100;
        sSG_R.sr_num_sec = 1;
        sSG_R.sr_num_sg = 1;
        sSG_R.sr_callback = NULL;
        sSG_R.sr_sglist[0].sb_buf = (PUCHAR)hBuffer_R;
        sSG_R.sr_sglist[0].sb_len = 0;

        dwExpected = ZERO;

        break;
    
    default:

        g_pKato->Log( LOG_FAIL, TEXT("BadParms FAIL: Unknown lpFTE->dwUserData: %d"),
            lpFTE->dwUserData );
        
        return TPR_FAIL;
    }

    //
    // First, fill sector 100 with all DWORD 1's
    //
    sSG_C.sr_start = 100;
    sSG_C.sr_num_sg = 1;
    sSG_C.sr_num_sec = 1;
    sSG_C.sr_callback = NULL;
    sSG_C.sr_sglist[0].sb_buf = (PUCHAR)hBuffer_C;
    sSG_C.sr_sglist[0].sb_len = sDiskInfo.di_bytes_per_sect;

    dwFill = sDiskInfo.di_bytes_per_sect / 4;
    p = (DWORD*)hBuffer_C;
    while ( dwFill ) {

        *p = 1;
        p++;
        dwFill--;
    }
    
    // Submit write request
    g_pKato->Log( LOG_COMMENT, TEXT("Filling sector 100 with DWORD 1's...") );
    
    fRet = DeviceIoControl( hDsk,
        DISK_IOCTL_WRITE,
        & sSG_C,
        sizeof( SG_REQ ),
        NULL,
        NULL,
        & dwBytesReturned,
        NULL );

    if ( !fRet ) {

        // 
        g_pKato->Log( LOG_FAIL, TEXT("BadParms FAIL: ...Unable to write control sector") );

        return TPR_FAIL;        
    }
    
    //
    // Now, attempt bad parameter write
    //
    g_pKato->Log( LOG_DETAIL, TEXT("Submitting write request: sr_start: %d, sr_num_sec: %d, sr_num_sg: %d, sr_callback: 0x%08x, sb_buf: 0x%08x, sb_len: %d"),
        sSG_W.sr_start,
        sSG_W.sr_num_sec,
        sSG_W.sr_num_sg,
        sSG_W.sr_callback,
        sSG_W.sr_sglist[0].sb_buf,
        sSG_W.sr_sglist[0].sb_len );
    
    // Submit write request
    fRet = DeviceIoControl( hDsk,
        DISK_IOCTL_WRITE,
        & sSG_W,
        sizeof( SG_REQ ),
        NULL,
        NULL,
        & dwBytesReturned,
        NULL );

    if ( fRet ) {

        // Device driver did not return an error
        g_pKato->Log( LOG_FAIL, TEXT("DISK_IOCTL_WRITE accepted invalid parameter w/o indicating an error, check for data integrity.") );
    }
    else {

        // Failure in call to ...WRITE. 
        g_pKato->Log( LOG_DETAIL, TEXT("BadParms PASS: Expected error encountered in ...WRITE. GetLastError()=%lu."),
            GetLastError() );
    }

    //
    // Next, check contents of sector 100 (should be all 1's)
    //
    memset( hBuffer_C, 0, sDiskInfo.di_bytes_per_sect );
    
    // Submit write request
    fRet = DeviceIoControl( hDsk,
        DISK_IOCTL_READ,
        & sSG_C,
        sizeof( SG_REQ ),
        NULL,
        NULL,
        & dwBytesReturned,
        NULL );

    if ( !fRet ) {

        // 
        g_pKato->Log( LOG_FAIL, TEXT("BadParms FAIL: ...Unable to read control sector") );

        dwRet = TPR_FAIL;       
    }
    
    dwFill = sDiskInfo.di_bytes_per_sect / 4;
    p = (DWORD *)hBuffer_C;

    g_pKato->Log( LOG_COMMENT, TEXT("BadParms: Verifying contents of sector 100...") );

    while (dwFill) {
        
        if ( *p != 1 ) {

            // Corrupted data
            g_pKato->Log( LOG_FAIL, TEXT("BadParms FAIL: Corrupted data at DWORD %d. Expected: 1, Received: %d."),
                ( p - (DWORD*)hBuffer_C ),
                *p );

            dwRet = TPR_FAIL;
        }
        p++;
        dwFill--;
    }

    //
    // Submit read request with bad parameters
    //
    g_pKato->Log( LOG_DETAIL, TEXT("Submitting read request: sr_start: %d, sr_num_sec: %d, sr_num_sg: %d, sr_callback: 0x%08x, sb_buf: 0x%08x, sb_len: %d"),
        sSG_R.sr_start,
        sSG_R.sr_num_sec,
        sSG_R.sr_num_sg,
        sSG_R.sr_callback,
        sSG_R.sr_sglist[0].sb_buf,
        sSG_R.sr_sglist[0].sb_len );
    
    // Attempt to read
    fRet = DeviceIoControl( hDsk,
        DISK_IOCTL_READ,
        & sSG_R,
        sizeof( SG_REQ ),
        NULL,
        NULL,
        & dwBytesReturned,
        NULL );

    if ( fRet ) {

        // Device driver did not return an error
        g_pKato->Log( LOG_FAIL, TEXT("DISK_IOCTL_READ accepted invalid parameter w/o indicating error. Checking data integrity.") );
    }
    else {

        // Failure in call to ...WRITE. 
        g_pKato->Log( LOG_DETAIL, TEXT("BadParms PASS: Expected error encountered in ...READ. GetLastError()=%lu."),
            GetLastError() );
    }

    // Verify no data lost
    dwFill = sDiskInfo.di_bytes_per_sect / 4;
    p = (DWORD *)hBuffer_R;

    g_pKato->Log( LOG_COMMENT, TEXT("BadParms: Verifying contents of read buffer...") );

    while (dwFill) {
        
        if ( dwExpected == SAME ) {
            if ( *p != dwFill) {

                // Corrupted data
                g_pKato->Log( LOG_FAIL, TEXT("BadParms FAIL: Corrupted data at DWORD %d. Expected: %d, Received: %d."),
                    ( p - (DWORD*)hBuffer_R ),
                    dwFill,
                    *p );

                dwRet = TPR_FAIL;
            }
        }
        else if ( dwExpected == ZERO ) {
            if ( *p != 0 ) {

                // Corrupted data
                g_pKato->Log( LOG_FAIL, TEXT("BadParms FAIL: Corrupted data at DWORD %d. Expected: 0, Received: %d."),
                    ( p - (DWORD*)hBuffer_R ),
                    dwFill );

                dwRet = TPR_FAIL;
            }
        }
        else {

            g_pKato->Log( LOG_FAIL, TEXT("UNKNOWN TEST. dwExpected = %d."),
                dwExpected );

            dwRet = TPR_FAIL;
        }

        dwFill--;
        p++;
    }

    // Release resources
    CloseHandle( hDsk );
    LocalFree( hBuffer_R );
    LocalFree( hBuffer_W );
    LocalFree( hBuffer_C );
    
    return dwRet;
}

