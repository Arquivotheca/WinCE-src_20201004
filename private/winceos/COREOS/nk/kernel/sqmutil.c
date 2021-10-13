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
#include "kernel.h"
#include <sqmutil.h>
#include <sqmdatapoints.h>

SQMOSMARKER g_SQMOSMarkers[SQM_MARKER_LAST_ITEM];

void
InitSQMMarkers ()
{
    memset (&g_SQMOSMarkers, 0, sizeof(g_SQMOSMarkers));
    g_SQMOSMarkers[SQM_MARKER_PHYSLOWBYTES].dwValue = (DWORD) -1;
    
    g_SQMOSMarkers[SQM_MARKER_PHYSOOMCOUNT].dwId = DATAID_KERNEL_MEMORY_PHYSOOMCOUNT;
    g_SQMOSMarkers[SQM_MARKER_VMOOMCOUNT].dwId = DATAID_KERNEL_MEMORY_VMOOMCOUNT;
    g_SQMOSMarkers[SQM_MARKER_RAMBASELINEBYTES].dwId = DATAID_KERNEL_MEMORY_RAM_BASELINE_COUNT;
    g_SQMOSMarkers[SQM_MARKER_PHYSLOWBYTES].dwId = DATAID_KERNEL_MEMORY_PHYSICAL_LOW_COUNT;    
}

BOOL
GetSQMMarkers (
    LPBYTE lpOutBuf,
    DWORD cbOutBuf,
    LPDWORD lpdwBytesReturned
    )
{
    DWORD dwErr = 0;
    DWORD cbSizeRequired = sizeof(g_SQMOSMarkers);

    if (!lpdwBytesReturned) {
        dwErr = ERROR_INVALID_PARAMETER;

    } else {
        // copy the total size required
        memcpy (lpdwBytesReturned, &cbSizeRequired, sizeof(DWORD));

        if (lpOutBuf) {
            if (cbOutBuf < cbSizeRequired) {
                dwErr = ERROR_INSUFFICIENT_BUFFER;
        
            } else {
                // copy the markers and update markers which are in pages to bytes
                PSQMOSMARKER pMarkers = (PSQMOSMARKER) lpOutBuf;
                memcpy (pMarkers, &g_SQMOSMarkers, sizeof(g_SQMOSMarkers));
                pMarkers[SQM_MARKER_PHYSLOWBYTES].dwValue *= KInfoTable[KINX_PAGESIZE];
                pMarkers[SQM_MARKER_RAMBASELINEBYTES].dwValue *= KInfoTable[KINX_PAGESIZE];
            } 
        }
    }
    
    return !dwErr;
}

BOOL
ResetSQMMarkers ()
{
    g_SQMOSMarkers[SQM_MARKER_PHYSOOMCOUNT].dwValue = 0;
    g_SQMOSMarkers[SQM_MARKER_VMOOMCOUNT].dwValue = 0;
    g_SQMOSMarkers[SQM_MARKER_PHYSLOWBYTES].dwValue = (DWORD) -1;
    //g_SQMOSMarkers[SQM_MARKER_RAMBASELINEBYTES].dwValue = 0; // don't reset ram baseline

    return TRUE;
}

BOOL
UpdateSQMMarkers (
        enSQMMarker eMarker,
        DWORD dwValue
    )
{
    BOOL fRet = TRUE;
    
    switch (eMarker) {
        case SQM_MARKER_PHYSOOMCOUNT:
            InterlockedExchangeAdd ((LONG*) &(g_SQMOSMarkers[SQM_MARKER_PHYSOOMCOUNT].dwValue), dwValue);
            break;

        case SQM_MARKER_PHYSLOWBYTES:
            // called with phys lock held
            if (dwValue < g_SQMOSMarkers[SQM_MARKER_PHYSLOWBYTES].dwValue) { 
                g_SQMOSMarkers[SQM_MARKER_PHYSLOWBYTES].dwValue = dwValue;
            }
            break;

        case SQM_MARKER_RAMBASELINEBYTES:
            // called only on boot from filesys
            g_SQMOSMarkers[SQM_MARKER_RAMBASELINEBYTES].dwValue = dwValue;
            break;

        case SQM_MARKER_VMOOMCOUNT:
            InterlockedExchangeAdd ((LONG*) &(g_SQMOSMarkers[SQM_MARKER_VMOOMCOUNT].dwValue), dwValue);
            break;
            
        default:
            fRet = FALSE;
            DEBUGCHK (0);
            break;
    }
    
    return fRet;
}

