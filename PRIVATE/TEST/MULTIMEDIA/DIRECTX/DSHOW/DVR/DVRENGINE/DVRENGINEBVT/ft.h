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
    FTH(0,"DVR BVT tests")

    FTH(1,"Basic record tests")
//    FTE(2,"BVT - ASFDemuxCanRender",100,0,ASFDemuxCanRender)
    FTE(2,"BVT - EngineCanWorkPerm",101,0,EngineCanWorkPerm)
    FTE(2,"BVT - EngineCanWorkTemp",102,0,EngineCanWorkTemp)
    FTE(2,"BVT - GetCurFileTempRecStartedTest",103,0,GetCurFileTempRecStartedTest)
    FTE(2,"BVT - GetCurFilePermRecStartedTest",104,0,GetCurFilePermRecStartedTest)
    FTE(2,"BVT - BeginTempRecTest",105,0,BeginTempRecTest)
    FTE(2,"BVT - BeginPermRecTest",106,0,BeginPermRecTest)
    FTE(2,"BVT - SwitchRecModeTest",107,0,SwitchRecModeTest)
    FTE(2,"BVT - TempRec2TempRecTest",108,0,TempRec2TempRecTest)
    FTE(2,"BVT - SetGetRecordingPathExist",109,0,SetGetRecordingPathExist)
    FTE(2,"BVT - DeleteRecordedItemTest",110,0,DeleteRecordedItemTest)
    FTE(2,"BVT - DeleteInBufferRecordTest",111,0,DeleteInBufferRecordTest)
    FTE(2,"BVT - CleanupOrphanedRecordingsTest",112,0,CleanupOrphanedRecordingsTest)

#ifdef BVT_USETEMPMETHOD
    FTH(1,"Temp tests")
    FTE(2,"BVT - Test_Temp01",1000,0,Test_Temp01)
    FTE(2,"BVT - Test_Em10Tune",1001,0,Test_Em10Tune)
    FTE(2,"BVT - Test_Em10Tune01",1002,0,Test_Em10Tune01)
    FTE(2,"BVT - Test_PauseAndPlay",1003,0,Test_PauseAndPlay)
    FTE(2,"BVT - Test_FreePlayPerm",1004,0,Test_FreePlayPerm)
    FTE(2,"BVT - CleanTempPlayback",1005,0,CleanTempPlayback)
#endif
END_FTE

#endif // !__FT_H__ || __GLOBALS_CPP__

