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



#ifndef __EVENTLOGCOMMON_H__
#define __EVENTLOGCOMMON_H__

#define MAX_USER_DATA_SIZE 4096
#define EVENTLOG_TRACE_ID 0x147

typedef ULONG (*ControlCallback)(WMIDPREQUESTCODE, PVOID, ULONG *, PVOID);
typedef struct EventLogFilterMask
{
    ULONGLONG   ullMatchAllKeywords;
    ULONGLONG   ullMatchAnyKeywords;
    DWORD       Level;
}EventLogFilterMask;

typedef struct EventTraceCallbackData
{
    EventLogFilterMask Mask;
    DWORD EnableFlags;
    WNODE_HEADER wNode;
    HANDLE TraceHandle;
    BOOL fCloseThread;
}EventTraceCallbackData;

#endif
