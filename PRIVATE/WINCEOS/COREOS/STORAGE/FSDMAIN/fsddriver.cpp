//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED \"AS IS\" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.
--*/

#include <windows.h>
#include <types.h>
#include <winerror.h>
//#include <devload.h>
#include <diskio.h>
#include <fsdmgrp.h>
#include <storemain.h>

/*****************************************************************************/


/* This driver exists only to cause the FSDMGR to get loaded into the
 * system. 
 */


/*****************************************************************************/


DWORD Init(DWORD dwContext)
{
    DEBUGMSG( ZONE_INIT, (L"FSDMGR!FSD_Init Key=%08X\r\n", dwContext));
    if (!InitStorageManager(dwContext)) {
        DEBUGMSG( ZONE_INIT, (L"FSDMGR!FSD_Init - Failed initializing storage manager...Aborting\r\n"));
        return FALSE;
    }    
    return 1;
}

DWORD Deinit()
{
    // We never ever expect this to be called;
    ASSERT(0);
    return FALSE;
}

