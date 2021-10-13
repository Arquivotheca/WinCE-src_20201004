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

#include <upnpmem.h>
#include <ncdefine.h>
#include <trace.h>
#pragma hdrstop

HANDLE g_hHeap;

BOOL WINAPI UpnpHeapCreate()
{
    if (!(g_hHeap = HeapCreate(
                                0,    // NULL on failure,serialize access
                                0x1000, // initial heap size
                                0       // max heap size (0 == growable)
                              )))
    {
        TraceTag(ttidInit | ttidError, "%s:  HeapCreate Failed[%08x]\n", __FUNCTION__, GetLastError());
        g_hHeap = GetProcessHeap();

        if (g_hHeap == NULL)
        {
            TraceTag(ttidInit | ttidError, "%s:  GetProcessHeap Failed[%08x]\n", __FUNCTION__, GetLastError());
            return FALSE;
        }
    }

    return TRUE;
}


VOID WINAPI UpnpHeapDestroy()
{
    //
    // if ghHeap is NULL, then there is no heap to destroy
    //

    if ( ( g_hHeap != NULL) && ( g_hHeap != GetProcessHeap() ) )
    {
        HeapDestroy (g_hHeap);
        g_hHeap = NULL;
    }
}
