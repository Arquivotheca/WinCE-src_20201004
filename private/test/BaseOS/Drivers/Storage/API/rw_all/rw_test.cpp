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
     
    rw_test.cpp  
        
Abstract:

    This file contains the TestProc() function for 

Functions:

    WritePattern
    ReadPattern
    CountDevices
    WriteAllSectors

Notes:

-------------------------------------------------------------------------------
*/

#include "main.h"
#include "globals.h"
#include "utility.h"

DWORD WritePattern ( HANDLE, DWORD, DWORD, DWORD );
DWORD ReadPattern ( HANDLE, DWORD, DWORD, DWORD );

#define INVALID_TIME        0xFFFFFFFF
#define WRITE_FAILED        0xFFFFFFFF
#define READ_FAILED         0xFFFFFFFF


// Global Variables
BOOL g_fOldIoctls = FALSE;

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  Function:
        
    FindNextDevice

  Description:

    If DSKx: is a known valid device, this function will search for another
    device, DSK(>x):. This is useful in the following situation:
        
        1. Insert a card (this is DSK1:)
        2. Insert another card (this is DSK2:)
        3. Remove the first card. Now the only card in the system is DSK2:.
        4. Hard-coding "DSK1:" into the test program will cause an error
           in this situation.

  Arguments:

    DWORD i: This is the highest known device in the system, DSKi:. If you are
             looking for the first device, send in 0. The routine will start
             its search at DSK(i+1):.

    DWORD j: This is the index of the next DSKx: device in the system.

  Return Value:

    HANDLE: This is the disk handle for the open device

	Notes:
		The suite has changed to permit the "targeting" of one device, see comments 
		for GetSpecificHandle()
			- ericfowl

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

HANDLE
FindNextDevice (
    DWORD i, DWORD& j
    ) {
    
    // Validate input parameters
    if (i > 9) 
	// Input parameter is out of range
        return INVALID_HANDLE_VALUE;

    HANDLE hDsk = INVALID_HANDLE_VALUE;
    TCHAR szTemp[16];
    DISK_INFO diskInfo;
    
    // Find the first available DSKx: device
    do {

        // 'i' is the number of the last found device. Start the search at 'i+1'
        i++;
        
        // Create the device name to try to open
        _stprintf_s(szTemp, countof(szTemp), TEXT("DSK%u:"), i );

        // Attempt to open the device
        hDsk = OpenDevice(szTemp, g_fOpenAsStore, g_fOpenAsPartition);

        if(!GetDiskInfo(hDsk, szTemp, &diskInfo, g_pKato))
            {
  	     VERIFY(CloseHandle(hDsk));
            hDsk= INVALID_HANDLE_VALUE;
            }
	else
            {
            j = i;
            }

    } while ( hDsk == INVALID_HANDLE_VALUE && i <= 9 );

    return hDsk;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  Function:

    CountCards

  Description:

    This routine will count the number of PCMCIA cards in the system.

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

DWORD
CountCards( void ) {

    //
    // This routine counts the number of cards that are inserted in the system
    // and returns the count as a DWORD.
    //

    DWORD dwNumCards = 0;
    INT i;
    HANDLE hDsk;
    TCHAR szTemp[16];
    DISK_INFO diskInfo;

    for ( i = 1; i <= 9; i++ ) {
        
        // Create device name
        _stprintf_s(szTemp, countof(szTemp), TEXT("DSK%u:"), i );

        // Attempt to open the device
        hDsk = OpenDevice(szTemp, g_fOpenAsStore, g_fOpenAsPartition);

	 if (hDsk != INVALID_HANDLE_VALUE)
	 {
	     if(!GetDiskInfo(hDsk, szTemp, &diskInfo, g_pKato))
                {
  	         hDsk= INVALID_HANDLE_VALUE;
                }
            else
	        {
                // DSKi: was successfully opened. Close the device and increment the counter.
                CloseHandle( hDsk );
                dwNumCards++;
                }
	 }
    }

    // Return with the number of cards found
    return dwNumCards;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  Function:

    rw_all

  Description:

    This routine will perform a very simple read/write test at the DSK layer.
    Every sector on the disk will be written to and read from, and the data
    will be verified. Each card inserted in the system will be assigned a thread
    that will carry out the test.

    No bad parameters are passed to the device driver; this is a very basic test.
    Most failures from this test are an indicator of a bad card.

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

TESTPROCAPI
WriteAllSectors (
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE
    ) {
    
    // Check message to see why we have been called
    if ( uMsg == TPM_QUERY_THREAD_COUNT ) 
	{
        // Check if the client has specify the device under test from the command line
	if(g_szProfile[0])
	    {
	    if(g_szDiskName[0])
	        g_pKato->Log(LOG_DETAIL,_T("WARNING:  Overriding /disk option with /profile option\n"));
	    ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
	    }
	else if (g_szDiskName[0])
	    {
	    ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
	    }
	else
	    {
	    // Set the dwThreadCount field to the number of threads (=number of cards)
	    ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = CountCards();
	    g_pKato->Log( LOG_COMMENT, TEXT("CountDevices() found %d cards."),
	        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount );
	    }
        // Returning TPR_HANDLED sets the number of threads to the value in tpParam->dwThreadCount
        return TPR_HANDLED;
        }
    // uMsg was TPM_EXECUTE...
	if(!Zorch(g_pShellInfo->szDllCmdLine))
	{
		Zorchage(g_pKato); 
		return TPR_SKIP; 
	}

    BOOL fRet;
    TCHAR szTemp[16];
    DISK_INFO sDiskInfo;
    DWORD dwBytesReturned, dwNumSectors, dwCurrentSector;
    DWORD dwMinWriteTime = INVALID_TIME, dwMaxWriteTime = INVALID_TIME, dwAveWriteTime = 0, dwTime;
    DWORD dwMinReadTime = INVALID_TIME, dwMaxReadTime = INVALID_TIME, dwAveReadTime = 0;
    DWORD dwRet = TPR_PASS;
    DWORD i = 0;
    DWORD dwThreadNumber;
    DWORD dwPercentComplete;
    HANDLE hDsk;
    
    // TUX will pass the number of sectors to read/write at once via lpFTE->dwUserData.
    dwNumSectors = lpFTE->dwUserData;
    
    // Get a handle to the card (DSK{thread number}:)
    dwThreadNumber = ((TPS_EXECUTE*)tpParam)->dwThreadNumber;

    if(g_szProfile[0])
    	{
        hDsk = OpenDiskHandleByProfile(g_pKato, g_fOpenAsStore, g_szProfile, g_fOpenAsPartition);
		if (hDsk != INVALID_HANDLE_VALUE)
            _stprintf_s( szTemp, countof(szTemp), TEXT("%s"), g_szProfile );
    	}
    else if (g_szDiskName[0])
    	{
        hDsk = OpenDiskHandleByDiskName(g_pKato, g_fOpenAsStore, g_szDiskName, g_fOpenAsPartition);
		if (hDsk != INVALID_HANDLE_VALUE)
            _stprintf_s( szTemp, countof(szTemp), TEXT("%s"), g_szDiskName );
    	}
    else 
        {
		g_pKato->Log( LOG_COMMENT, TEXT("[%d] Attempting to open %s:"), dwThreadNumber, szTemp );
			
        if ( dwThreadNumber == 1 ) 
            {
            // If there is only one thread, it will test the only DSKx: device. This could
            // be DSK1: or DSK2: (depending on the sequence of card inserts and removes).
            DWORD outIndex;
            hDsk = FindNextDevice( 0, outIndex);

            // Thread x will test DSKx:
            _stprintf_s( szTemp, countof(szTemp), TEXT("DSK%u:"), outIndex );
            }
        else 
            {        
            // Thread x will test DSKx:
            _stprintf_s( szTemp, countof(szTemp), TEXT("DSK%u:"), dwThreadNumber );

            hDsk = OpenDevice(szTemp, g_fOpenAsStore, g_fOpenAsPartition);

            if(!GetDiskInfo(hDsk, szTemp, &sDiskInfo, g_pKato))
                {
      	         VERIFY(CloseHandle(hDsk));
                hDsk= INVALID_HANDLE_VALUE;
            	  }
            }
    	}

    // Make sure we were able to find and open a device
    if ( hDsk == INVALID_HANDLE_VALUE ) {
        
        // Unable to find or open a DSKx: device. Test failed, return now.
        g_pKato->Log( LOG_FAIL, TEXT("[%d] rw_all FAILED. Unable to open a device. GetLastError()=%lu."),
            dwThreadNumber,
            GetLastError() );		
        return TPR_FAIL;
    }
    else {
        
        // Found and opened a DSKx: device.
	 g_pKato->Log( LOG_DETAIL, TEXT("[%d] rw_all successfully opened %s, hDsk=0x%08X."),
            dwThreadNumber,
            szTemp,
            hDsk );
    }

    // Clear the data structures used to return values from DISK_IOCTL_GETINFO
    memset( & sDiskInfo, 0, sizeof(sDiskInfo) );
    dwBytesReturned = 0;

    // Get information about the opened device
    // Was DISK_IOCTL_GETINFO successful?
    if ( !GetDiskInfo(hDsk, g_szDiskName, &sDiskInfo, g_pKato) ) {
        // Unable to get device information. Test failed, return now.
	g_pKato->Log(LOG_FAIL, TEXT("[%d] rw_all FAILED. %s, hDsk=0x%08X, %s failed. GetLastError()=%lu."),
		dwThreadNumber,
		szTemp,
		hDsk,
		g_fOldIoctls ? "DISK_IOCTL_GETINFO" : "IOCTL_DISK_GETINFO", 
		GetLastError() );

        VERIFY(CloseHandle(hDsk));
        return TPR_FAIL;
    }

    // Successfully opened device.

    // perform a low-level format of the disk-- may be required for flash
    fRet = DeviceIoControl( hDsk,
        g_fOldIoctls ? DISK_IOCTL_FORMAT_MEDIA : IOCTL_DISK_FORMAT_MEDIA,             // control code        
        NULL,                           // input buffer (not used)
        0,                              // input buffer size (not used)
        NULL,                           // output buffer (not used)
        0,                              // output buffer size (not used)
        & dwBytesReturned,              // address of DWORD containing number of bytes returned (ignored)
        NULL );                         // pointer to overlapped structure (not used)

    // Was DISK_IOCTL_FORMAT_MEDIA successful?
    if ( !fRet ) {
        // Unable to low-level format, but continue testing anyway
	g_pKato->Log( LOG_DETAIL, TEXT("[%d] hDsk=0x%08X, %s failed. GetLastError()=%lu. Continuing test."),
		dwThreadNumber,
		szTemp,
		hDsk,
		g_fOldIoctls ? "DISK_IOCTL_FORMAT_MEDIA" : "IOCTL_DISK_FORMAT_MEDIA", 
		GetLastError() );
    }

    g_pKato->Log( LOG_DETAIL, TEXT("[%d] rw_all: Total sectors: %d, Bytes per sector: %d."),
        dwThreadNumber,
        sDiskInfo.di_total_sectors,
        sDiskInfo.di_bytes_per_sect );

    // calculate the percentage compete
    dwPercentComplete = 0;

    // Read/write every sector, dwNumSectors at a time.
    for ( dwCurrentSector = 0; dwCurrentSector < sDiskInfo.di_total_sectors; dwCurrentSector += dwNumSectors ) {
        
        // Make sure the block of sectors is within the device parameters; we aren't conducting
        // a 'bad parameters' test.
        if ( (dwCurrentSector + dwNumSectors) > sDiskInfo.di_total_sectors ) {
            dwNumSectors = sDiskInfo.di_total_sectors - dwCurrentSector;
        }

#ifdef VERBOSE
        // Write a pattern to the disk.
        g_pKato->Log( LOG_DETAIL, TEXT("[%d] Attempting to write sectors %d through %d."),
            dwThreadNumber,
            dwCurrentSector,
            (dwCurrentSector + dwNumSectors - 1) );

#else // VERBOSE

        if(dwPercentComplete != (DWORD) (100 * ((DOUBLE)dwCurrentSector) / ((DOUBLE)sDiskInfo.di_total_sectors))) {
            dwPercentComplete = (DWORD) (100 * ((DOUBLE)dwCurrentSector) / ((DOUBLE)sDiskInfo.di_total_sectors));
            g_pKato->Log( LOG_DETAIL, TEXT("[%d] writing %u sectors @ sector %u, %u%% complete..."), 
                dwThreadNumber,
                dwNumSectors,
                dwCurrentSector,
                dwPercentComplete);
        }
        
#endif // VERBOSE        

        dwTime = WritePattern( hDsk, dwCurrentSector, dwNumSectors, sDiskInfo.di_bytes_per_sect );

        if ( WRITE_FAILED == dwTime ) {

            // Encountered an error writing to the device. Test failed. Continue with test.
            g_pKato->Log( LOG_FAIL, TEXT("[%d] rw_all FAILED. Continuing test."),
                dwThreadNumber );
            dwRet = TPR_FAIL;
        }
        else {

            // Update time statistics
            if ( dwMinWriteTime == INVALID_TIME )
                dwMinWriteTime = dwTime;

            if ( dwMaxWriteTime == INVALID_TIME )
                dwMaxWriteTime = dwTime;

            dwMinWriteTime = min(dwMinWriteTime, dwTime);
            dwMaxWriteTime = max(dwMaxWriteTime, dwTime);
            dwAveWriteTime = (dwAveWriteTime * i + dwTime) / (i + 1);
            i++;

#ifdef VERBOSE
            g_pKato->Log( LOG_DETAIL, TEXT("[%d] Successfully wrote %d sectors in %d ms."), 
                dwThreadNumber,
                dwNumSectors,
                dwTime );
#endif // VERBOSE

        }
        
        // Read that pattern back.
        if ( WRITE_FAILED == dwTime ) {

            // Write sector failed, do not try to read this sector
            g_pKato->Log( LOG_DETAIL, TEXT("[%d] Read: Skipping sectors %d through %d."),
                dwThreadNumber,
                dwCurrentSector,
                (dwCurrentSector + dwNumSectors - 1) );
        }
        else {

#ifdef VERBOSE
            g_pKato->Log( LOG_DETAIL, TEXT("[%d] Attempting to read sectors %d through %d."),
                dwThreadNumber,
                dwCurrentSector,
                (dwCurrentSector + dwNumSectors - 1) );
#endif // VERBOSE
            dwTime = ReadPattern ( hDsk, dwCurrentSector, dwNumSectors, sDiskInfo.di_bytes_per_sect );
        }

        if ( READ_FAILED == dwTime ) {

            // Encountered an error reading from the device. Test failed. Continue with test.
            g_pKato->Log( LOG_FAIL, TEXT("[%d] rw_all FAILED. Continuing test."),
                dwThreadNumber );
            dwRet = TPR_FAIL;
        }
        else {

            // Update time statistics
            if ( dwMinReadTime == INVALID_TIME )
                dwMinReadTime = dwTime;

            if ( dwMaxReadTime == INVALID_TIME )
                dwMaxReadTime = dwTime;

            dwMinReadTime = min(dwMinReadTime, dwTime);
            dwMaxReadTime = max(dwMaxReadTime, dwTime);
            dwAveReadTime = (dwAveReadTime * i + dwTime) / (i + 1);
            i++;

#ifdef VERBOSE
            g_pKato->Log( LOG_DETAIL, TEXT("[%d] Successfully read %d sectors in %d ms."), 
                dwThreadNumber,
                dwNumSectors,
                dwTime );
#endif // VERBOSE

        }
        
    } // Continue with next block of sectors

    // Test is completed, Give time summaries.
    g_pKato->Log( LOG_DETAIL, TEXT("[%d] Min Write Time: %d ms, Max Write Time: %d ms, Ave Write Time: %d ms."),
        dwThreadNumber,
        dwMinWriteTime,
        dwMaxWriteTime,
        dwAveWriteTime );
    g_pKato->Log( LOG_DETAIL, TEXT("[%d] Min Read Time: %d ms, Max Read Time: %d ms, Ave Read Time: %d ms."),
        dwThreadNumber,
        dwMinReadTime,
        dwMaxReadTime,
        dwAveReadTime );

    g_pKato->Log( LOG_COMMENT, TEXT("[%d] rw_all exiting."),
        dwThreadNumber );

    CloseHandle(hDsk);
    // Return control to TUX.
    return dwRet;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  Function:

    WritePattern

  Description:

    This routine writes a block of data to the memory card, and times the
    operation.

  Arguments:

    hDsk: Handle to the device that is to be written
    dwCurrentSector: the starting sector to be written
    dwNumSectors: the number of sectors to be written
    dwBytesPerSector: the number of bytes per sector

  Return Value:

    This function returns the time it took to write the sectors, in
    milliseconds, if the write was successful (that is, the device driver did
    not return an error code). If the write was not a success, the routine will
    return NULL.

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

DWORD
WritePattern (
    HANDLE hDsk,
    DWORD dwCurrentSector,
    DWORD dwNumSectors,
    DWORD dwBytesPerSector
    ) {

    //
    // This routine writes dwNumSectors, starting at dwCurrentSector, to the device
    // referenced by hDsk. This routine requires that the sector size (in bytes,
    // DISK_INFO.di_bytes_per_sect) be specified. WritePattern() will return the 
    // time (in milliseconds) it took to complete the write, if it was successful.
    // If the write fails, the routine will return NULL.
    //
    
    BOOL fRet;
    DWORD dwBytesReturned, dwTickCount, i;
    DWORD * p;
    HLOCAL hBuffer;
    SG_REQ sSGReq;

    DWORD dwBufferSize = 0;
    if (S_OK != DWordMult(dwNumSectors, dwBytesPerSector, &dwBufferSize))
    {
        // Integer overflow. Test fails, do not continue.
        g_pKato->Log( LOG_FAIL, TEXT("WritePattern FAILED. Integer overflow while calculating buffer size.") );
        return WRITE_FAILED;
    }

    // Populate scatter/gather structures for read and write ioctls
    sSGReq.sr_start = dwCurrentSector;  // starting sector number
    sSGReq.sr_num_sec = dwNumSectors;   // number of sectors
    sSGReq.sr_num_sg = 1;                   // number of scatter/gather buffers
    sSGReq.sr_callback = NULL;              // request completion callback function
    sSGReq.sr_sglist[0].sb_len = dwBufferSize; // length of buffer
    hBuffer = LocalAlloc( LPTR, sSGReq.sr_sglist[0].sb_len );
    sSGReq.sr_sglist[0].sb_buf = PUCHAR(hBuffer);
                                                    // pointer to buffer

    if ( hBuffer == NULL ) {
        
        // Unable to allocate heap memory. Test fails, do not continue.
        g_pKato->Log( LOG_FAIL, TEXT("WritePattern FAILED. LocalAlloc() unable to allocate %d bytes for buffer."),
            dwBufferSize );
        return WRITE_FAILED;
    }

    // Fill the scatter/gather buffer
    i = sSGReq.sr_sglist[0].sb_len / 4;
    p = (DWORD *)hBuffer;

    while (i) {
        *p = i;
        i--;
        p++;
    }

    // Get time in ms since WindowsCE was started
    dwTickCount = GetTickCount();
    
    // Write the buffer to the device
    fRet = DeviceIoControl( hDsk,       // handle to device
	 g_fOldIoctls ? DISK_IOCTL_WRITE : IOCTL_DISK_WRITE,    // control code
        & sSGReq,                           // address of DISK_INFO structure
        sizeof(SG_REQ),                 // size of DISK_INFO structure
        NULL,                                   // output buffer (not used)
        NULL,                                   // output buffer size (not used)
        & dwBytesReturned,              // address of DWORD containing number of bytes returned
        NULL );                             // pointer to overlapped structure (not used)

    // Calculate time to write
    dwTickCount = GetTickCount() - dwTickCount;
    
    // Was the write successful?
    if ( !fRet ) {

        // Encountered an error while writing. Continue testing.
        g_pKato->Log( LOG_FAIL, TEXT("WritePattern FAILED. %s was unable to write to sectors %d through %d. GetLastError()=%lu. SG_REQ.sr_status=%d."),
            g_fOldIoctls ? "DISK_IOCTL_WRITE" : "IOCTL_DISK_WRITE",
            dwCurrentSector,
            (dwCurrentSector + dwNumSectors - 1),
            GetLastError(),
            sSGReq.sr_status );
        dwTickCount = WRITE_FAILED;
    }

    // Release buffer space
    if ( LocalFree(hBuffer) != NULL ) {

        // Encountered a problem releasing memory
        g_pKato->Log( LOG_FAIL, TEXT("ReadPattern FAILED. LocalFree() was unable to release buffer.") );
        dwTickCount = WRITE_FAILED;
    }

    return dwTickCount;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  Function:

    ReadPattern

  Description:

    This routine reads and verifies a block of sectors from a memory card. It
    also times the operation.

  Arguments:

    hDsk: handle to the device that is to be read
    dwCurrentSector: starting sector to be read
    dwNumSectors: number of sectors to read
    dwBytesPerSector: number of bytes per sector

  Return Value:

    If the routine successfully reads and verifies the data, it will return the
    time, in milliseconds, of the read operation. If the device driver returns
    an error code, or the data that is read does not match what is expected, the
    function will return NULL.

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

DWORD
ReadPattern (
    HANDLE hDsk,
    DWORD dwCurrentSector,
    DWORD dwNumSectors,
    DWORD dwBytesPerSector
    ) {

    //
    // This routine reads and verifies dwNumSectors, starting at dwCurrentSector,
    // from the device referenced by hDsk. This routine requires that the sector 
    // size (in bytes, DISK_INFO.di_bytes_per_sect) be specified. ReadPattern() 
    // will return the time (in milliseconds) to took to complete the read operation,
    // if it was successful. If the read fails, the routine will return NULL.
    //
    
    BOOL fRet;
    DWORD i, dwBytesReturned, dwTickCount;
    DWORD * p;
    HLOCAL hBuffer;
    SG_REQ sSGReq;
    DWORD dwBufferSize = 0;
    if (S_OK != DWordMult(dwNumSectors, dwBytesPerSector, &dwBufferSize))
    {
        // Integer overflow. Test fails, do not continue.
        g_pKato->Log( LOG_FAIL, TEXT("ReadPattern FAILED. Integer overflow while calculating buffer size.") );
        return READ_FAILED;
    }

    // Populate scatter/gather structures for read and write ioctls
    sSGReq.sr_start = dwCurrentSector;  // starting sector number
    sSGReq.sr_num_sec = dwNumSectors;   // number of sectors
    sSGReq.sr_num_sg = 1;                   // number of scatter/gather buffers
    sSGReq.sr_callback = NULL;              // request completion callback function
    sSGReq.sr_sglist[0].sb_len = dwBufferSize; // length of buffer
    hBuffer = LocalAlloc( LPTR, sSGReq.sr_sglist[0].sb_len );
    sSGReq.sr_sglist[0].sb_buf = PUCHAR(hBuffer);
                                                    // pointer to buffer

    // Able to allocate buffer?
    if ( hBuffer == NULL ) {
        
        // Unable to allocate heap memory. Test failed, return now.
        g_pKato->Log( LOG_FAIL, TEXT("ReadPattern FAILED. LocalAlloc() unable to allocate %d bytes for buffer."),
            dwBufferSize );
        return READ_FAILED;
    }

    // Get tick count
    dwTickCount = GetTickCount();
    
    // Attempt to read from the device
    fRet = DeviceIoControl( hDsk,       // handle to device
        g_fOldIoctls ? DISK_IOCTL_READ : IOCTL_DISK_READ,                    // control code
        & sSGReq,                           // address of DISK_INFO structure
        sizeof(SG_REQ),                 // size of DISK_INFO structure
        NULL,                                   // output buffer (not used)
        NULL,                                   // output buffer size (not used)
        & dwBytesReturned,              // address of DWORD containing number of bytes returned
        NULL );                             // pointer to overlapped structure (not used)

    // Calculate time to read
    dwTickCount = GetTickCount() - dwTickCount;

    // Was the read successful?
    if ( !fRet ) {

        // Encountered an error while reading. Test failed, continue with test.
        g_pKato->Log( LOG_FAIL, TEXT("ReadPattern FAILED. DISK_IOCTL_READ was unable to read sectors %d through %d. GetLastError()=%lu, SG_REQ.sr_status=%d."),
            g_fOldIoctls ? "DISK_IOCTL_READ" : "IOCTL_DISK_READ",
            dwCurrentSector,
            (dwCurrentSector + dwNumSectors - 1),
            GetLastError(),
            sSGReq.sr_status );
        dwTickCount = READ_FAILED;
    }
    else {
        
        // Verify correct contents of the scatter/gather buffer
        i = sSGReq.sr_sglist[0].sb_len / 4;
        p = (DWORD *)hBuffer;

        while (i) {
            if ( *p != i ) {

                // Incorrect contents. Test failed, return now.
                g_pKato->Log( LOG_FAIL, TEXT("ReadPattern FAILED at sector %d, dword %d. Expected: %d, Returned: %d."),
                    (dwCurrentSector + (DWORD)((p - (DWORD *)hBuffer) / dwBytesPerSector)),
                    ((p - (DWORD *)hBuffer) % dwBytesPerSector),
                    i,
                    *p );
                dwTickCount = READ_FAILED;
            }
            i--;
            p++;
        }
        
        // Release buffer space
        if ( LocalFree(hBuffer) != NULL ) {

            // Encountered a problem releasing memory
            g_pKato->Log( LOG_FAIL, TEXT("ReadPattern FAILED. Unable to release buffer.") );
            dwTickCount = READ_FAILED;
        }
    }

    return dwTickCount;
}
