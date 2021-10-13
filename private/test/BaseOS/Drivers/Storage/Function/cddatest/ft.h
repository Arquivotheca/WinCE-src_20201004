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
////////////////////////////////////////////////////////////////////////////////
//
//  CDDATEST TUX DLL
//
//  Module: ft.h
//          Declares the TUX function table and test function prototypes EXCEPT
//          when included by globals.cpp, in which case it DEFINES the function
//          table.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#if (!defined(__FT_H__) || defined(__GLOBALS_CPP__))
#ifndef __FT_H__
#define __FT_H__
#endif

////////////////////////////////////////////////////////////////////////////////
// Local macros

#ifdef __DEFINE_FTE__
#undef BEGIN_FTE
#undef FTE
#undef FTH
#undef END_FTE
#define BEGIN_FTE FUNCTION_TABLE_ENTRY g_lpFTE[] = {
#define FTH(a, b) { TEXT(b), a, 0, 0, NULL },
#define FTE(a, b, c, d, e) { TEXT(b), a, d, c, e },
#define END_FTE { NULL, 0, 0, 0, NULL } };
#else // __DEFINE_FTE__
#ifdef __GLOBALS_CPP__
#define BEGIN_FTE
#else // __GLOBALS_CPP__
#define BEGIN_FTE extern FUNCTION_TABLE_ENTRY g_lpFTE[];
#endif // __GLOBALS_CPP__
#define FTH(a, b)
#define FTE(a, b, c, d, e) TESTPROCAPI e(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
#define END_FTE
#endif // __DEFINE_FTE__

////////////////////////////////////////////////////////////////////////////////
// TUX Function table
//  To create the function table and function prototypes, two macros are
//  available:
//
//      FTH(level, description)
//          (Function Table Header) Used for entries that don't have functions,
//          entered only as headers (or comments) into the function table.
//
//      FTE(level, description, code, param, function)
//          (Function Table Entry) Used for all functions. DON'T use this macro
//          if the "function" field is NULL. In that case, use the FTH macro.
//
//  You must not use the TEXT or _T macros here. This is done by the FTH and FTE
//  macros.
//
//  In addition, the table must be enclosed by the BEGIN_FTE and END_FTE macros.

BEGIN_FTE
    FTH(0, "audio CD (CDDA) tests for CD/DVD block drivers")
    FTE(1, "raw read & compare 1st sector of each track", 1001, 0, Tst_AudioCDBVT)
    FTE(1, "raw read twice & compare all CD data 32 sectors/read", 1002, 32, Tst_AudioCDDataChk)
    FTE(1, "raw read and measure throughput using 32 sectors/read", 1003, 32, Tst_AudioCDDataThroughput)

    FTH(0, "audio CD (CDDA) tests with .wav file output")
    FTE(1, "make summary .wav file w/first 500 sectors of each track", 2001, 500, Tst_TrackSummary)
    FTE(1, "convert entire track to .wav file, 32 sectors/read", 2002, 32, Tst_TrackToWavFile)    

	FTH(0, "audio IOCTL tests")
	FTE(1, "play 10s at each track", 3001, 10000, Tst_PlayTracksInOrder)
	FTE(1, "play 10s at each track in reverse order", 3002, 10000, Tst_PlayTracksReverseOrder)
	FTE(1, "seek to the start of each track", 3003, 0, Tst_SeekTrackInOrder)
	FTE(1, "seek to the start of each track in reverse order", 3004, 0, Tst_SeekTrackReverseOrder)
	FTE(1, "play, pause, and resume each track", 3005, 10000, Tst_PlayAndPauseTracksInOrder)
	
END_FTE

////////////////////////////////////////////////////////////////////////////////

#endif // !__FT_H__ || __GLOBALS_CPP__
