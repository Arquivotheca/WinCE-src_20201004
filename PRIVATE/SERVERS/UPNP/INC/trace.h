//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

#define TraceError(sz, hr)                      if (FAILED(hr)) TraceTag(ttidError,"%hs(%d): %hs: Error(0x%x)\n",__FILE__, __LINE__, sz, hr);
#define TraceResult(sz, f)                      if (!f) TraceTag(ttidError,"%hs(%d): %hs: LastError(0x%x)\n",__FILE__, __LINE__, sz, GetLastError());




#else   // !ENABLETRACE

#define TraceError(_sz, _hr)
#define TraceResult(_sz, _f)
#define TraceTag                                    NOP_FUNCTION


#endif  // ENABLETRACE

