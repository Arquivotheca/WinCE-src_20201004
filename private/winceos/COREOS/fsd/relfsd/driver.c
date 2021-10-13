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
