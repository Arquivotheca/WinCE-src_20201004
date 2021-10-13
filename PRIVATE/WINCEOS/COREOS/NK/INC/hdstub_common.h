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

Module Name :

    Hdstub_common.h

Abstract:

    This module contains the hdstub event structure.  (For sharing with the desktop build)

--*/

#pragma once

#ifndef _HDSTUB_COMMON_H_
#define _HDSTUB_COMMON_H_

/* Extra HRESULTS that may be returned by HDSTUB */
#define HDSTUB_E_NOCLIENT MAKE_HRESULT(1, FACILITY_WINDOWS_CE, 0x4400)  /* This is returned when hdstub is unable to find a specific client */

// EXDI notification structure
enum
{
    HDSTUB_EVENT_EXCEPTION = 0,
    HDSTUB_EVENT_PAGEIN = 1,
    HDSTUB_EVENT_MODULE_LOAD = 2,
    HDSTUB_EVENT_MODULE_UNLOAD = 3
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


// Constants for HDStub Event Filter
enum HDSTUB_FILTER
{
    HDSTUB_FILTER_NIL       = 0,
    HDSTUB_FILTER_EXCEPTION = 0x1,
    HDSTUB_FILTER_VMPAGEIN  = 0x2,
    HDSTUB_FILTER_MODLOAD   = 0x4,
    HDSTUB_FILTER_MODUNLOAD = 0x8,
    HDSTUB_FILTER_DEFAULT   = 0xF,
    HDSTUB_FILTER_ENABLE_CACHE_FLUSH = 0x80000000,
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
} HDSTUB_SHARED_GLOBALS;


#endif
