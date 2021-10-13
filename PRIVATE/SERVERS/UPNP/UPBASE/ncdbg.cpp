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
//  File:       D E B U G X . C P P
//
//  Contents:   Implementation of debug support routines.
//
//  Notes:
//
//----------------------------------------------------------------------------

#include <pch.h>
#pragma hdrstop

#ifdef ENABLETRACE

#define DBG_BUFFER_SIZE	512
//+---------------------------------------------------------------------------
//
//  Function:   TraceTag
//
//  Purpose:    Output a debug trace to one or more trace targets (ODS,
//              File, COM port, etc.). This function determines the targets
//              and performs the actual trace.
//
//  Arguments:
//      ttid    []  TraceTag to use for the debug output
//      pszaFmt []  Format of the vargs.
//
//  Returns:
//
//  Notes:
//
VOID
WINAPIV
TraceTag (
    TRACETAGID  ttid,
    PCSTR       pszaFmt,
    ...)
{
    // If this tracetag is turned off, don't do anything.
    //
	if (!DEBUGZONE(ttid))
		return;

    CHAR szaBuf [DBG_BUFFER_SIZE];

    // Build the string from the varg list
    //
    va_list valMarker;
    va_start (valMarker, pszaFmt);

    // copy as many bytes as possible
    _vsnprintf (szaBuf, sizeof(szaBuf), pszaFmt, valMarker);

    // terminate full string
    szaBuf[sizeof(szaBuf) - 1] = '\0';

    va_end (valMarker);

    NKDbgPrintfW(L"%s: %a\n",dpCurSettings.lpszName, szaBuf);
}

#endif

#ifdef DEBUG
VOID WINAPI AssertSzFn(PCSTR pszaMsg, PCSTR pszaFile, INT nLine)
{
	NKDbgPrintfW(L"(%a:%d) %a\n", pszaFile, nLine, pszaMsg);
	DebugBreak();
}
#endif


