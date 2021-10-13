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
#include <devload.h>
#include <diskio.h>
#include <relfsd.h>

/*****************************************************************************/


/* This driver exists only to cause the RELFSD to get loaded into the
 * system. It's silly that there's no other way to get an FSD loaded.
 */


/*****************************************************************************/


DWORD DSK_Init(DWORD dwContext)
{
    return 1;
}


/*****************************************************************************/


BOOL DSK_DeinitClose(DWORD dwContext)
{
    return TRUE;
}


/*****************************************************************************/


DWORD DSK_Dud(DWORD dwData,DWORD dwAccess,DWORD dwShareMode)
{
    return dwData;
}


/*****************************************************************************/


BOOL DSK_IOControl(DWORD Handle, DWORD dwIoControlCode,
                   PBYTE pInBuf, DWORD nInBufSize,
                   PBYTE pOutBuf, DWORD nOutBufSize,
                   PDWORD pBytesReturned)
{
    return FALSE;
}


/*****************************************************************************/


void DSK_Power(void)
{
}
