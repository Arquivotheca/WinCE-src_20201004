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
/*++

Module Name :

    Hdstub_common.h

Abstract:

    This module contains the hdstub event structure.  (For sharing with the desktop build)

--*/

#pragma once

#ifndef _HDSTUB_COMMON_H_
#define _HDSTUB_COMMON_H_

// EXDI notification structure
enum HdstubEventType
{
    HDSTUB_EVENT_EXCEPTION = 0,
    HDSTUB_EVENT_PAGEIN = 1,
    HDSTUB_EVENT_MODULE_LOAD = 2,
    HDSTUB_EVENT_MODULE_UNLOAD = 3,
    HDSTUB_EVENT_PAGEOUT = 4
};

enum
{
    HDSTUB_MODNAMELEN = 64,
    HDSTUB_PDBNAMELEN = 64,
    HDSTUB_CURRENT_VERSION = 0
};


typedef struct _HDSTUB_EVENT
{
    DWORD dwType;
    DWORD dwArg1;
    DWORD dwArg2;
    DWORD dwArg3;
    DWORD fHandled;
    DWORD rgdwReserved[11];     // Pad out the structure to 64 bytes
} HDSTUB_EVENT;

// {3215CEF7-5447-46f5-9B63-77C921E9ACC1}
#define HDSTUB_EVENT2_SIGNATURE { 0x3215cef7, 0x5447, 0x46f5, { 0x9b, 0x63, 0x77, 0xc9, 0x21, 0xe9, 0xac, 0xc1 } }


typedef struct _HDSTUB_EVENT2
{
    GUID  Signature;                    // 0x000        Signature of the structure (unique guid)
    DWORD Size;                         // 0x010        Size of the total event structure.
    DWORD SubVersion;                   // 0x014        Subversion that tracks changes to this structure.
    enum HdstubEventType Type;          // 0x018        Type code from above
    DWORD Pad0;                         // 0x01C        Pad to 64bit
    union
    {
        struct
        {
            ULONG64 pExceptionRecord;   // 0x020        Pointer to OS Exception record
            ULONG64 pContext;           // 0x028        Pointer to Exception Context
            BOOL    SecondChance;       // 0x030        Is this exception second change.
            BOOL    fHandled;           // 0x034        Debugger handled the exception?
        } Exception;
        struct
        {
            ULONG64 PageAddress;        // 0x020        VM Page Base
            ULONG64 NumPages;           // 0x028        Number of pages
            ULONG32 VmId;               // 0x030        VM id.
            BOOL    fWriteable;
        } PageIn;
        struct
        {
/*
TODO: Try this and profile load notification. May not have enough stack space.
            ULONG32 Handle;             // 0x020
            ULONG32 RefCount;           // 0x024
            ULONG64 BasePointer;        // 0x028
            ULONG64 ModuleSize;         // 0x030
            ULONG64 Address;            // 0x038
            ULONG32 TimeStamp;          // 0x040
            ULONG32 PdbFormat;          // 0x044
            GUID    PdbSignature;       // 0x054
            ULONG32 PdbAge;             // 0x058
            ULONG32 Flags;              // 0x05C
            CHAR    ModuleName[HDSTUB_MODNAMELEN];      // 0x060
            CHAR    PdbName[HDSTUB_PDBNAMELEN];         // 0x100
*/
            ULONG64 StructAddr;
        } Module;
    } Event;
} HDSTUB_EVENT2;


// Constants for HDStub Event Filter
enum HDSTUB_FILTER
{
    HDSTUB_FILTER_NIL       = 0,
    HDSTUB_FILTER_EXCEPTION = 0x1,
    HDSTUB_FILTER_VMPAGEIN  = 0x2,
    HDSTUB_FILTER_MODLOAD   = 0x4,
    HDSTUB_FILTER_MODUNLOAD = 0x8,
    HDSTUB_FILTER_DEFAULT   = 0xF,
    HDSTUB_FILTER_VMPAGEOUT  = 0x10,
    HDSTUB_FILTER_ENABLE_CACHE_FLUSH = 0x80000000,
    HDSTUB_FILTER_NONMP_PROBE = 0x40000000,
};


// Constants for HDStub Mod Load Notif
enum MOD_LOAD_NOTIF_OPTIONS
{
    MLNO_ALWAYS_OFF         = 0x0,
    MLNO_ALWAYS_ON          = 0x1,
    MLNO_OFF_UNTIL_HALT     = 0x2,
    MLNO_AUTO               = 0x4,
    MLNO_ALWAYS_ON_UNINIT   = 0x80000000,
};


// Pool of global variables in HdStub shared accross multiple debugger modules (HD, KD, OSAXS, host-side debugger)
typedef struct _HDSTUB_SHARED_GLOBALS
{
    DWORD cbSize; // expandability
    DWORD dwModLoadNotifDbg; // Notify Module load and unload to host-side debugger (non-real-time) or not (see MOD_LOAD_NOTIF_OPTIONS)
    DWORD dwAllModuleLoadsAndUnloads;  //if > 0, Notify host-side debugger of every module load and unload.
} HDSTUB_SHARED_GLOBALS;


#endif
