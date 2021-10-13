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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*++


 Module Name:    ndisevnt.h

 Abstract:       This will implement the ndis required functions which
                 differ between the CE and NT implementations.

 Contents:

--*/

#ifndef _NDISEVNT_H_
#define _NDISEVNT_H_

#ifdef UNDER_CE
 
//
// Add support for NDIS_EVENTs.
//

typedef struct _NDIS_EVENT
{
    HANDLE hEvent;
}NDIS_EVENT, *PNDIS_EVENT;

__inline VOID
INdisInitializeEvent(
    IN PNDIS_EVENT Event
    )
{
    Event->hEvent = CreateEvent(
        NULL,   // Security.
        FALSE,  // Auto-reset = TRUE.
        FALSE,  // Initial state = non-signalled.
        NULL    // Not named.
        );
}

__inline VOID
INdisDeinitializeEvent(
    IN PNDIS_EVENT Event
    )
{
    CloseHandle(Event->hEvent);
}

__inline VOID
INdisSetEvent(
    IN PNDIS_EVENT Event
    )
{
    SetEvent(Event->hEvent);
}

__inline VOID
INdisResetEvent(
    IN PNDIS_EVENT Event
    )
{
    ResetEvent(Event->hEvent);
}

__inline BOOLEAN
INdisWaitEvent(
    IN PNDIS_EVENT Event,
    IN UINT        MsToWait
    )
{
    DWORD status;

    if (MsToWait == 0)
    {
        MsToWait = INFINITE;
    }

    status = WaitForSingleObject(Event->hEvent, MsToWait);

    return (status == WAIT_OBJECT_0);
}
 
#endif // UNDER_CE

#endif // _NDISEVNT_H_


