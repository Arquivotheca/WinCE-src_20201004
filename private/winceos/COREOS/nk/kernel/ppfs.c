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
 *      NK Kernel ppfs client code
 *
 *
 * Module Name:
 *
 *      ppfs.c
 *
 * Abstract:
 *
 *      This file implements the NK kernel ppfs client side interface
 *
 *
 */

#include "kernel.h"
#include <ppfs.h>

#define REG_PEGASUS_ZONE        "Pegasus\\Zones"

BOOL ReadZoneFromRemoteRegistry (LPCWSTR pszName, LPDWORD pdwZone)
{
    static HANDLE hReg = NULL;

    // We really only need to keep one open handle around that can be shared by
    // everyone. We don't need to worry about multi-threading issues because
    // relfsd serializes all i/o with a global lock.
    if (!hReg
        && KernelIoctl (IOCTL_EDBG_IS_STARTED, NULL, 0, NULL, 0, NULL)) {

        HANDLE hTmp = FSCreateFile (TEXT("\\Release\\reg:"), GENERIC_READ, FILE_SHARE_READ,
                            NULL, OPEN_EXISTING, 0, NULL );
        if (INVALID_HANDLE_VALUE != hTmp) {
            if (InterlockedCompareExchangePointer (&hReg, hTmp, NULL) != NULL) {
                HNDLCloseHandle (g_pprcNK, hTmp);
            }
        }
    }

    if (hReg) {
        CEFSINPUTSTRUCT cefsin;
        DWORD hKeyZone = 0;

        // setup cefsin for opening the registry key "\HKCU\Pegasus\Zones"
        cefsin.hKey = (DWORD)HKEY_CURRENT_USER;
        memcpy (cefsin.szName, REG_PEGASUS_ZONE, sizeof (REG_PEGASUS_ZONE));

        // open the registry key
        if (-1 != FSIoctl (hReg, IOCTL_REG_OPEN, &cefsin,  sizeof(cefsin), &hKeyZone, sizeof(DWORD), NULL, NULL)) {
            CEFSOUTPUTSTRUCT cefsout;
            
            DEBUGCHK (hKeyZone);

            // setup cefsin for reading registry value
            cefsin.hKey = hKeyZone;
            NKUnicodeToAscii (cefsin.szName, pszName, MAX_PATH);

            // read the registry value
            // IOCTL_REG_GET returns boolean
            cefsout.dwSize = sizeof(cefsout.data);
            if (FSIoctl (hReg, IOCTL_REG_GET,  &cefsin,  sizeof(cefsin), &cefsout, sizeof (cefsout), NULL, NULL)
                && (REG_DWORD == cefsout.dwType)) {
                *pdwZone = *((LPDWORD) &cefsout.data[0]);
            }

            // close the registry
            FSIoctl (hReg, IOCTL_REG_CLOSE, &hKeyZone, sizeof (DWORD), NULL, 0, NULL, NULL);
            
        }
        
    }

    return NULL != hReg;

}
