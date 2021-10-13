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

    bvt.cpp
 
Abstract:

    This file contains test procedures for verifying a driver's ability to
    handle boundary and out-of-bounds parameters.

Functions:

    TESTPROCAPI MultiSG( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )

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

    BVT

  Description:

    BVT() will write to and read from a memory card, focusing on boundary
    and out-of-bounds conditions

  Arguments:

    Runs as a Tux .dll

  Return Value:

    TPR_PASS or TPR_FAIL

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

TESTPROCAPI BVT(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
    
    //
    // This routine will test reading/writing to a disk device, concentrating on
    // boundary conditions.
    // 
    // This routine will implement the following tests:
    //    1. read/write all sectors
    //    2. read/write sectors -1, 0 and 1
    //    3. read/write di_total_sectors (-1/+0/+1)
    //    4. read/write (di_total_sectors / 2) (-1/+0/+1)
    //    5. read/write (di_total_sectors * 2) (-1/+0/+1)
    //

    // Check message to see why we have been called
    // This routine only handles the TPM_EXECUTE command
    if ( uMsg != TPM_EXECUTE )
        return TPR_NOT_HANDLED;

    TCHAR szTemp[16];
    HANDLE hDsk;
    DWORD dwRet = TPR_PASS;
    BOOL fRet;
    SG_REQ sSG;
    DISK_INFO sDiskInfo;
    DWORD dwStartSector, dwEndSector, dwNumSectors;
    PDWORD p;
    DWORD dwBytesReturned;
    HLOCAL hBuffer;
    DWORD dwFill;

    // Get handle to first available DSKx: device
    hDsk = GetNextHandle( 0, szTemp );

    if ( hDsk == INVALID_HANDLE_VALUE ) {
        // No valid DSKx: device found. Log this and then return.
        g_pKato->Log( LOG_FAIL, TEXT("No valid DSKx: devices found.") );
        return TPR_FAIL;
    }

    g_pKato->Log( LOG_COMMENT, TEXT("Successfully opened %s, hDsk=0x%08X."),
        szTemp,
        hDsk );
    
    // Get information about the opened disk
    memset( & sDiskInfo, 0, sizeof(DISK_INFO) );
    
    fRet = DeviceIoControl( hDsk,       // handle to device
        DISK_IOCTL_GETINFO,             // control code
        & sDiskInfo,                    // address of DISK_INFO structure
        sizeof(DISK_INFO),              // size of DISK_INFO structure
        NULL,                           // output buffer (not used)
        NULL,                           // output buffer size (not used)
        & dwBytesReturned,              // address of DWORD containing number of bytes returned (ignored)
        NULL );                         // pointer to overlapped structure (not used)
    
    if ( !fRet ) {
    
        // DISK_IOCTL_GETINFO failed
        g_pKato->Log( LOG_FAIL, TEXT("%s, hDsk=0x%08X, FAIL: DISK_IOCTL_GETINFO failed. GetLastError()=%lu."),
            szTemp,
            hDsk,
            GetLastError() );
        return TPR_FAIL;
    }

    g_pKato->Log( LOG_COMMENT, TEXT("GETINFO successful. %d sectors"),
        sDiskInfo.di_total_sectors );

    switch ( lpFTE->dwUserData ) {
    case 1:

        //*********************
        //* r/w first sectors *
        //*********************

        dwStartSector = 0;
        dwNumSectors = 1;
        dwEndSector = 2;

        break;

    case 2:

        //*******************
        //* r/w mid sectors *
        //*******************

        dwStartSector = sDiskInfo.di_total_sectors / 2;
        dwEndSector = dwStartSector + 2;
        dwNumSectors = 1;

        break;

    case 3:

        //********************
        //* r/w last sectors *
        //********************

        dwStartSector = sDiskInfo.di_total_sectors - 1;
        dwEndSector = sDiskInfo.di_total_sectors + 10;
        dwNumSectors = 1;
        
        break;

    case 4:

        //*****************************
        //* r/w out of bounds sectors *
        //*****************************

        dwStartSector = sDiskInfo.di_total_sectors * 2;
        dwEndSector = dwStartSector + 2;
        dwNumSectors = 1;

        break;

    default:

        // This case should never occur
        g_pKato->Log( LOG_FAIL, TEXT("ERROR: Default case called, %d"),
            lpFTE->dwUserData );

        dwRet = TPR_FAIL;
    }

    for ( DWORD dwLoopIndex = dwStartSector; dwLoopIndex <= dwEndSector; dwLoopIndex++ ) {
            
        // Populate scatter/gather structures
        sSG.sr_start = dwLoopIndex;
        sSG.sr_num_sec = dwNumSectors;
        sSG.sr_num_sg = 1;
        sSG.sr_callback = NULL;
        sSG.sr_sglist[0].sb_len = dwNumSectors * sDiskInfo.di_bytes_per_sect;

        // Allocate memory for a read/write buffer
        hBuffer = LocalAlloc( LPTR, dwNumSectors * sDiskInfo.di_bytes_per_sect );

        if ( hBuffer == NULL ) {

            // System was unable to allocate memory for the write buffer
            g_pKato->Log( LOG_FAIL, TEXT("WriteTests() FAILED. LocalAlloc() unable to allocate %d bytes for buffer."),
                ( dwNumSectors * sDiskInfo.di_bytes_per_sect ) );

            // Return now; do not continue with test
            return TPR_FAIL;
        }

        // Fill the buffer
        dwFill = sSG.sr_sglist[0].sb_len / 4;
        p = (DWORD *)hBuffer;

        while (dwFill) {
            *p = dwFill;
            dwFill--;
            p++;
        }
    
        sSG.sr_sglist[0].sb_buf = (PUCHAR)hBuffer;

        // Write the buffer to the device
        g_pKato->Log( LOG_DETAIL, TEXT("WriteTests() attempting to write sector %d. %s, hDsk=0x%08X."),
            dwLoopIndex,
            szTemp,
            hDsk );

        fRet = DeviceIoControl( hDsk,       // handle to device
            DISK_IOCTL_WRITE,               // control code
            & sSG,                          // address of DISK_INFO structure
            sizeof(SG_REQ),                 // size of DISK_INFO structure
            NULL,                           // output buffer (not used)
            NULL,                           // output buffer size (not used)
            & dwBytesReturned,              // address of DWORD containing number of bytes returned
            NULL );                         // pointer to overlapped structure (not used)

        // Was the write successful?
        if ( !fRet ) {

            // The write failed; see if it was supposed to fail (invalid sector)
            if ( dwLoopIndex < 0 || (dwLoopIndex + dwNumSectors) > sDiskInfo.di_total_sectors ) {

                // The sector number was invalid; the device driver should have returned a fail
                g_pKato->Log( LOG_PASS, TEXT("WriteTests() PASSED. DISK_IOCTL_WRITE did not write to invalid sector %d."),
                    dwLoopIndex );
            }
            else {

                // The sector number was valid, and the device driver failed
                g_pKato->Log( LOG_FAIL, TEXT("WriteTests() FAILED. DISK_IOCTL_WRITE unable to write to valid sector %d. %s, hDsk=0x%08X. GetLastError()=%lu."),
                    dwLoopIndex,
                    szTemp,
                    hDsk,
                    GetLastError() );

                // Continue with testing, but indicate a failure on return
                dwRet = TPR_FAIL;
            }
        }
        else {

            // The write succeeded; make sure it was supposed to.
            if ( dwLoopIndex < 0 || (dwLoopIndex + dwNumSectors) > sDiskInfo.di_total_sectors ) {

                // The sector number was invalid, but the driver returned a passing code
                g_pKato->Log( LOG_FAIL, TEXT("WriteTests() FAILED. DISK_IOCTL_WRITE wrote to invalid sector %d without indicating a failure. %s, hDsk=0x%08X. GetLastError()=%lu."),
                    dwLoopIndex,
                    szTemp,
                    hDsk,
                    GetLastError() );

                // Continue with testing, but indicate a failure on return
                dwRet = TPR_FAIL;
            }
            else {

                // The sector number was valid, and the device driver returned a passing code
                g_pKato->Log( LOG_PASS, TEXT("WriteTests() PASSED. DISK_IOCTL_WRITE successfully wrote to valid sector %d."),
                    dwLoopIndex );
            }
        }
    } // Go to next sector

    return dwRet;
    
} // BVT()


