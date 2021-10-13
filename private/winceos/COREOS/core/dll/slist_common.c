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
#include "windows.h"


#if defined (_X86_)
// In x86, the Interlocked functions are defined as using FASTCALL, and ARM is defined
// as using CDECL.  To avoid complicating the header, we wrap the FASTCALL x86
// functions (prefixed with Rtlp) with an interface identical to ARM (CDECL).  

extern 
WINBASEAPI 
PSLIST_ENTRY
__fastcall
RtlpInterlockedPopEntrySList (
    __inout PSLIST_HEADER ListHead
    );

extern 
WINBASEAPI 
PSLIST_ENTRY
__fastcall
RtlpInterlockedPushEntrySList (
    __inout PSLIST_HEADER ListHead,
    __inout PSLIST_ENTRY ListEntry
    );

extern
WINBASEAPI
PSLIST_ENTRY
__fastcall
RtlpInterlockedFlushSList (
    __inout PSLIST_HEADER ListHead
    );

WINBASEAPI
PSLIST_ENTRY
WINAPI
RtlInterlockedPopEntrySList (
    __inout PSLIST_HEADER ListHead
    )
{
    return RtlpInterlockedPopEntrySList(ListHead);
}

WINBASEAPI
PSLIST_ENTRY
WINAPI
RtlInterlockedPushEntrySList (
    __inout PSLIST_HEADER ListHead,
    __inout PSLIST_ENTRY ListEntry
    )
{
    return RtlpInterlockedPushEntrySList(ListHead, ListEntry);
}

WINBASEAPI
PSLIST_ENTRY
WINAPI
RtlInterlockedFlushSList (
    __inout PSLIST_HEADER ListHead
    )
{
    return RtlpInterlockedFlushSList(ListHead);
}
#endif

WINBASEAPI
VOID
WINAPI
RtlInitializeSListHead (
    __out PSLIST_HEADER ListHead
    )
{
    ListHead->Next.Next = NULL;
    ListHead->Depth = 0;
    ListHead->Sequence = 0;
}

WINBASEAPI
USHORT
WINAPI
RtlQueryDepthSList (
    __in PSLIST_HEADER ListHead
    )
{
    return ListHead->Depth;
}
