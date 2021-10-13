//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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


