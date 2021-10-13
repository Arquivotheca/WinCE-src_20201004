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
#if (!defined(__FT_H__) || defined(__GLOBALS_CPP__))
#ifndef __FT_H__
#define __FT_H__
#endif

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

BEGIN_FTE
    FTH(0,"DVR scenario tests")
    FTH(1,"Scenario tests")
    FTE(2,"Scenario - ConvertLongPermPartOutBuffer",100,0,ConvertLongPermPartOutBuffer)
    FTE(2,"Scenario - PauseBufferLenAccurate",101,0,PauseBufferLenAccurate)
    FTE(2,"Scenario - EndEventTest",102,0,EndEventTest)
    FTE(2,"Scenario - RepeatBuildPlaybackGraphLive",103,0,RepeatBuildPlaybackGraphLive)
    FTE(2,"Scenario - OrphanedRecording",104,0,OrphanedRecording)
    FTE(2,"Scenario - TemporaryOrphanedRecording",105,0,TempOrphanedRecording)

END_FTE

#endif // !__FT_H__ || __GLOBALS_CPP__

