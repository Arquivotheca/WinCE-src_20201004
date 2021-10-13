//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

#endif
