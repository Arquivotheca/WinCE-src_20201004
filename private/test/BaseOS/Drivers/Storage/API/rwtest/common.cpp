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
#include "globals.h"

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

	Notes:
		The suite has changed to permit the "targeting" of one device, see comments 
		for GetSpecificHandle()
			- ericfowl

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
        StringCchPrintf( szTemp, countof(szTemp), TEXT("DSK%u:"), i );

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




