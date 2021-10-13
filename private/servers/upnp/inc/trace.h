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
//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  File:       T R A C E . H
//
//  Contents:   Definitions for debug tracing 
//
//  Notes:      Windows CE
//
//
//----------------------------------------------------------------------------

#pragma once

#define ttidInit            1
#define ttidPublish         1
#define ttidSsdpAnnounce    2
#define ttidSsdpNotify      3
#define ttidEvents          3
#define ttidEventServer     3
#define ttidSsdpSocket      4
#define ttidSsdpNetwork     4
#define ttidSsdpRecv        4
#define ttidSsdpSearch      5
#define ttidSsdpSearchResp  5
#define ttidSsdpParser      6
#define ttidSsdpTimer       7
#define ttidSsdpCache       8
#define ttidControl         9
#define ttidDevice          13
#define ttidTrace           14
#define ttidError           15

#ifdef DEBUG 
#define ENABLETRACE 1
#endif

#ifdef ENABLETRACE

typedef unsigned TRACETAGID;
// Trace error functions.
//
VOID
WINAPIV
TraceTag (
    TRACETAGID  ttid,
    PCSTR       pszaFmt,
    ...);

#define TraceError(sz, hr)            if (FAILED(hr)) { TraceTag(ttidError,"%hs(%d): %hs: Error(0x%x)\n",__FILE__, __LINE__, sz, hr);}
#define TraceResult(sz, f)            if (!f) { TraceTag(ttidError,"%hs(%d): %hs: LastError(0x%x)\n",__FILE__, __LINE__, sz, GetLastError()); }




#else   // !ENABLETRACE

#define TraceError(_sz, _hr)
#define TraceResult(_sz, _f)
#define TraceTag                      NOP_FUNCTION


#endif  // ENABLETRACE

