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

        common.cpp
     
    Abstract:

        This file contains utility routines that are commong to several modules.
    
    Functions:

        HANDLE GetNextHandle( INT, TCHAR* );
        DWORD CountCards( void );
        DWORD FindNextDevice ( DWORD );
    
    Notes:

-------------------------------------------------------------------------------
*/

#include "main.h"

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

  Return Value:

    DWORD: This is the index of the next DSKx: device in the system.

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

DWORD
FindNextDevice (
    DWORD i
    ) {
    
    // Validate input parameters
    if (i > 9) {

        // Input parameter is out of range
        return NULL;
    }

    DWORD dwRet;
    HANDLE hDsk;
    TCHAR szTemp[16];
    DISK_INFO diskInfo;
    DWORD cb;
    
    // Find the first available DSKx: device
    do {

        // 'i' is the number of the last found device. Start the search at 'i+1'
        i++;
        
        // Create the device name to try to open
        _stprintf( szTemp, TEXT("DSK%u:"), i );

        // Attempt to open the device
        hDsk = CreateFile( szTemp,
            GENERIC_READ|GENERIC_WRITE,
            FILE_SHARE_READ|FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            0 );

        if(INVALID_HANDLE_VALUE == hDsk)
        {
            continue;
        }

        // this control code will tell us if the DSK is R/W media or a CD
        if(!DeviceIoControl(hDsk, DISK_IOCTL_GETINFO, &diskInfo, sizeof(diskInfo), NULL, 0, &cb, NULL)
            || 0 == diskInfo.di_total_sectors )
        {
            CloseHandle(hDsk);
            hDsk = INVALID_HANDLE_VALUE;
            continue;
        }

    } while ( hDsk == INVALID_HANDLE_VALUE && i <= 9 );

    // Was the search successful?
    if ( hDsk == INVALID_HANDLE_VALUE ) {
        
        // Did not find a device
        dwRet = NULL;
    }
    else {

        // Found a device. Close this instance, return the number of the deivce
        CloseHandle( hDsk );
        dwRet = i;
    }

    return dwRet;
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
    DWORD cb;

    for ( i = 1; i <= 9; i++ ) {
        
        // Create device name
        _stprintf( szTemp, TEXT("DSK%u:"), i );

        // Attempt to open the device
        hDsk = CreateFile(szTemp, 
            GENERIC_READ|GENERIC_WRITE, 
            FILE_SHARE_READ|FILE_SHARE_WRITE, 
            NULL, 
            OPEN_EXISTING, 
            0, 
            0);

        if(INVALID_HANDLE_VALUE == hDsk)
        {
            continue;
        }

        if(!DeviceIoControl(hDsk, DISK_IOCTL_GETINFO, &diskInfo, sizeof(diskInfo), NULL, 0, &cb, NULL)
            || 0 == diskInfo.di_total_sectors )
        {
            CloseHandle(hDsk);
            hDsk = INVALID_HANDLE_VALUE;
            continue;
        }
        
        if ( hDsk != INVALID_HANDLE_VALUE ) {
            
            // DSKi: was successfully opened. Close the device and increment the counter.
            CloseHandle( hDsk );
            dwNumCards++;
        }
    }

    // Return with the number of cards found
    return dwNumCards;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  Function:

    GetNextHandle

  Description:

    GetNextHandle searches for a memory card with an index greater than
    iLastKnownIndex.

  Arguments:

    INT iLastKnownIndex     Device index to start searching above. Pass in a 0
                            to find the first card in the system
    TCHAR* szTemp           GetNextHandle will copy the name of the device to
                            the location pointed to by szTemp

  Return Value:

    Handle to an open device, if successful
    NULL if unsuccessful

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

HANDLE GetNextHandle( INT iLastKnownIndex, TCHAR* szTemp ) {

    HANDLE hDsk, hRet;
    
    _stprintf( szTemp, TEXT("DSK%u:"), FindNextDevice( iLastKnownIndex ) );

    // Attempt to open the file
    hDsk = CreateFile( szTemp,                  // pointer to name of file
        GENERIC_READ|GENERIC_WRITE,             // access mode
        FILE_SHARE_READ|FILE_SHARE_WRITE,       // share mode
        NULL,                                   // pointer to security attributes
        OPEN_EXISTING,                          // how to create
        0,                                      // file attributes
        0);                                     // handle to file w/ attributes to copy

    if ( hDsk == INVALID_HANDLE_VALUE ) {

        // Could not find a valid DSKx: device. Return a NULL.
        hRet = INVALID_HANDLE_VALUE;
    }
    else {

        // Found a valid device. Set return code to the handle
        hRet = hDsk;
    }

    return hRet;
}
