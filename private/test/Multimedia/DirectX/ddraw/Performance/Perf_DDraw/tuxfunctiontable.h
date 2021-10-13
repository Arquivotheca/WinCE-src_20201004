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
#pragma once
#ifndef __TUXFUNCTIONTABLE_H__
#define __TUXFUNCTIONTABLE_H__

#define BEGIN_FTE FUNCTION_TABLE_ENTRY g_lpFTE[] = {
#define FTH(a, b) { TEXT(b), a, 0, 0, NULL },
#define FTE(a, b, d, e) { TEXT(b), a, 0, d, e },
#define END_FTE { NULL, 0, 0, 0, NULL } };

// External Dependencies
// ---------------------
#include "perfcases.h"

BEGIN_FTE

    TUX_FTE(0, "Blt performance test - Video to Video 1 pixel and full surface",              1,  CDDPerfBpsVidVid)
    TUX_FTE(0, "Blt performance test - System to Video 1 pixel and full surface",             2,  CDDPerfBpsSysVid)
    TUX_FTE(0, "Blt performance test - System to System 1 pixel and full surface",            3,  CDDPerfBpsSysSys)
    TUX_FTE(0, "Blt performance test - Video to Video 10x scaled and nonoverlapping 100px",   4,  CDDPerfBps2VidVid)
    TUX_FTE(0, "Blt performance test - System to Video 10x scaled and nonoverlapping 100px",  5,  CDDPerfBps2SysVid)
    TUX_FTE(0, "Blt performance test - System to System 10x scaled and nonoverlapping 100px", 6,  CDDPerfBps2SysSys)
    TUX_FTE(0, "Color Key Blit testing",                                                      7,  CDDPerfCkBlit)
    TUX_FTE(0, "Flip perf test",                                                              8,  CDDPerfFlip)
    TUX_FTE(0, "Lock peformance test - Video memory Read and Write",                          9,  CDDPerfLockVid)
    TUX_FTE(0, "Lock peformance test - System memory Read and Write",                        10,  CDDPerfLockSys)
END_FTE

#endif 
