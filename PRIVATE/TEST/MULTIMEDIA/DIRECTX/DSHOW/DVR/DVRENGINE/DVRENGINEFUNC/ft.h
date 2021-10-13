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
    FTH(0, "DVR functional tests")

    FTH(1,"Functional tests")
    FTE(2,"FUNCTIONAL - GetBufferBeforeStartTest",100,0,GetBufferBeforeStartTest)
    FTE(2,"FUNCTIONAL - GetCurFilePermenantRecStopTest",101,0,GetCurFilePermenantRecStopTest)
    FTE(2,"FUNCTIONAL - LoadNoExistFileTest",102,0,LoadNoExistFileTest)
    FTE(2,"FUNCTIONAL - LoadMissingIndexFileTest",103,0,LoadMissingIndexFileTest)
    FTE(2,"FUNCTIONAL - PermStartFromOriginTest",104,0,PermStartFromOriginTest)
    FTE(2,"FUNCTIONAL - LiveKeepLiveTest",105,0,LiveKeepLiveTest)
    FTE(2,"FUNCTIONAL - BufferSizeAccurateTest",106,0,BufferSizeAccurateTest)
    FTE(2,"FUNCTIONAL - StopPositionMatch",107,0,StopPositionMatch)
    FTE(2,"FUNCTIONAL - RelativeSeekingTemp",108,0,RelativeSeekingTemp)
    FTE(2,"FUNCTIONAL - BeginTemporaryRecDiffBufTest",109,0,BeginTemporaryRecDiffBufTest)
    FTE(2,"FUNCTIONAL - BeginTempPermTest",110,0,BeginTempPermTest)
// requires tuning interfaces
#if 0
    FTE(2,"FUNCTIONAL - NotAlwaysFlushAndSeektoNow",111,0,NotAlwaysFlushAndSeektoNow)
#endif
    FTE(2,"FUNCTIONAL - BeginPermRecShortTest",112,0,BeginPermRecShortTest)
    FTE(2,"FUNCTIONAL - BeginPermPermTest",113,0,BeginPermPermTest)
    FTE(2,"FUNCTIONAL - SetRecordingPathDeep",114,0,SetRecordingPathDeep)
    FTE(2,"FUNCTIONAL - SetRecordingPathNoExistTest",115,0,SetRecordingPathNoExistTest)
    FTE(2,"FUNCTIONAL - SetRecordingPathNULLTest",116,0,SetRecordingPathNULLTest)
    FTE(2,"FUNCTIONAL - GetRecordingPathUninitializedTest",117,0,GetRecordingPathUninitializedTest)

// requires IAMStreamSelect interfaces
#if 0
    // IStreamSelect Interface
    FTH(1,"IStreamSelect tests")
    FTE(2,"FUNCTIONAL - Bound To Live - IAMStreamSelect.Enable",200,0,Test_BoundToLive_IAMStreamSelect_Enable)
    FTE(2,"FUNCTIONAL - Bound To Live Perm - IAMStreamSelect.Enable",201,0,Test_BoundToLivePerm_IAMStreamSelect_Enable)
    FTE(2,"FUNCTIONAL - Bound To Perm Recording - IAMStreamSelect.Enable",202,0,Test_BoundToRecording_IAMStreamSelect_Enable)

    FTE(2,"NEGATIVE - Bound To Live - IAMStreamSelect.Enable",203,0,Test_BoundToLive_IAMStreamSelect_Enable_Negative)
    FTE(2,"NEGATIVE - Bound To Live Perm - IAMStreamSelect.Enable",204,0,Test_BoundToLivePerm_IAMStreamSelect_Enable_Negative)
    FTE(2,"NEGATIVE - Bound To Perm Recording - IAMStreamSelect.Enable",205,0,Test_BoundToRecording_IAMStreamSelect_Enable_Negative)

    FTE(2,"FUNCTIONAL - Bound To Live - IAMStreamSelect.Count",206,0,Test_BoundToLive_IAMStreamSelect_Count)
    FTE(2,"FUNCTIONAL - Bound To Live Perm - IAMStreamSelect.Count",207,0,Test_BoundToLivePerm_IAMStreamSelect_Count)
    FTE(2,"FUNCTIONAL - Bound To Recording - IAMStreamSelect.Count",208,0,Test_BoundToRecording_IAMStreamSelect_Count)

    FTE(2,"NEGATIVE - Bound To Live - IAMStreamSelect.Count",209,0,Test_BoundToLive_IAMStreamSelect_Count_Negative)
    FTE(2,"NEGATIVE - Bound To Live Perm - IAMStreamSelect.Count",210,0,Test_BoundToLivePerm_IAMStreamSelect_Count_Negative)
    FTE(2,"NEGATIVE - Bound To Recording - IAMStreamSelect.Count",211,0,Test_BoundToRecording_IAMStreamSelect_Count_Negative)

    FTE(2,"FUNCTIONAL - Bound To Live - IAMStreamSelect.Info",212,0,Test_BoundToLive_IAMStreamSelect_Info)
    FTE(2,"FUNCTIONAL - Bound To Live Perm - IAMStreamSelect.Info",213,0,Test_BoundToLivePerm_IAMStreamSelect_Info)
    FTE(2,"FUNCTIONAL - Bound To Recording - IAMStreamSelect.Info",214,0,Test_BoundToRecording_IAMStreamSelect_Info)

    FTE(2,"NEGATIVE - Bound To Live - IAMStreamSelect.Info",215,0,Test_BoundToLive_IAMStreamSelect_Info_Negative)
    FTE(2,"NEGATIVE - Bound To Live Perm - IAMStreamSelect.Info",216,0,Test_BoundToLivePerm_IAMStreamSelect_Info_Negative)
    FTE(2,"NEGATIVE - Bound To Recording - IAMStreamSelect.Info",217,0,Test_BoundToRecording_IAMStreamSelect_Info_Negative)
#endif

    // Source Graph
    FTH(1,"Source graph tests")
    FTE(2,"FUNCTIONAL - Test_BoundToLive_SourceGraphStopAndRun",300,0,Test_BoundToLive_SourceGraphStopAndRun)
    FTE(2,"FUNCTIONAL - Test_BoundToLiveFullPB_SourceGraphStopAndRun",301,0,Test_BoundToLiveFullPB_SourceGraphStopAndRun)
    FTE(2,"FUNCTIONAL - Test_BoundToLivePerm_SourceGraphStopAndRun",302,0,Test_BoundToLivePerm_SourceGraphStopAndRun)
    FTE(2,"FUNCTIONAL - Test_BoundToRecording_SourceGraphStopAndRun",303,0,Test_BoundToRecording_SourceGraphStopAndRun)
    FTE(2,"FUNCTIONAL - Test_BoundToLive_SourceGraphRewindStopAndRun",304,0,Test_BoundToLive_SourceGraphRewindStopAndRun)
    FTE(2,"FUNCTIONAL - Test_BoundToLiveFullPB_SourceGraphRewindStopAndRun",305,0,Test_BoundToLiveFullPB_SourceGraphRewindStopAndRun)
    FTE(2,"FUNCTIONAL - Test_BoundToLivePerm_SourceGraphRewindStopAndRun",306,0,Test_BoundToLivePerm_SourceGraphRewindStopAndRun)
    FTE(2,"FUNCTIONAL - Test_BoundToRecording_SourceGraphRewindStopAndRun",307,0,Test_BoundToRecording_SourceGraphRewindStopAndRun)
    FTE(2,"FUNCTIONAL - Test_BoundToLive_SourceGraphPauseAndRun",308,0,Test_BoundToLive_SourceGraphPauseAndRun)
    FTE(2,"FUNCTIONAL - Test_BoundToLiveFullPB_SourceGraphPauseAndRun",309,0,Test_BoundToLiveFullPB_SourceGraphPauseAndRun)
    FTE(2,"FUNCTIONAL - Test_BoundToLivePerm_SourceGraphPauseAndRun",310,0,Test_BoundToLivePerm_SourceGraphPauseAndRun)
    FTE(2,"FUNCTIONAL - Test_BoundToRecording_SourceGraphPauseAndRun",311,0,Test_BoundToRecording_SourceGraphPauseAndRun)
    FTE(2,"FUNCTIONAL - Test_BoundToLivePerm_BeginMultiplePermanentRecording",312,0,Test_BoundToLive_BeginMultiplePermanentRecording)
    FTE(2,"FUNCTIONAL - Test_BoundToRecording_BeginMultiplePermanentRecording",313,0,Test_BoundToRecording_BeginMultiplePermanentRecording)

    FTH(1,"Seek tests")
    FTE(2,"FUNCTIONAL - Test_BoundToLive_SeekBackward",400,0,Test_BoundToLive_SeekBackward)
    FTE(2,"FUNCTIONAL - Test_BoundToLiveFullPB_SeekBackward",401,0,Test_BoundToLiveFullPB_SeekBackward)
    FTE(2,"FUNCTIONAL - Test_BoundToLivePerm_SeekBackward",402,0,Test_BoundToLivePerm_SeekBackward)
    FTE(2,"FUNCTIONAL - Test_BoundToRecording_SeekBackward",403,0,Test_BoundToRecording_SeekBackward)
    FTE(2,"FUNCTIONAL - Test_BoundToLive_SeekForward",404,0,Test_BoundToLive_SeekForward)
    FTE(2,"FUNCTIONAL - Test_BoundToLiveFullPB_SeekForward",405,0,Test_BoundToLiveFullPB_SeekForward)
    FTE(2,"FUNCTIONAL - Test_BoundToLivePerm_SeekForward",406,0,Test_BoundToLivePerm_SeekForward)
    FTE(2,"FUNCTIONAL - Test_BoundToRecording_SeekForward",407,0,Test_BoundToRecording_SeekForward)

    FTH(1,"RateChange tests")
    FTE(2,"FUNCTIONAL - Test_BoundToLive_RateChangedEvent",500,0,Test_BoundToLive_RateChangedEvent)
    FTE(2,"FUNCTIONAL - Test_BoundToLiveFullPB_RateChangedEvent",501,0,Test_BoundToLiveFullPB_RateChangedEvent)
    FTE(2,"FUNCTIONAL - Test_BoundToLivePerm_RateChangedEvent",502,0,Test_BoundToLivePerm_RateChangedEvent)
    FTE(2,"FUNCTIONAL - Test_BoundToRecording_RateChangedEvent",503,0,Test_BoundToRecording_RateChangedEvent)
    FTE(2,"FUNCTIONAL - Test_BoundToLive_RateAndPositioning",504,0,Test_BoundToLive_RateAndPositioning)
    FTE(2,"FUNCTIONAL - Test_BoundToLiveFullPB_RateAndPositioning",505,0,Test_BoundToLiveFullPB_RateAndPositioning)
    FTE(2,"FUNCTIONAL - Test_BoundToLivePerm_RateAndPositioning",506,0,Test_BoundToLivePerm_RateAndPositioning)
    FTE(2,"FUNCTIONAL - Test_BoundToRecording_RateAndPositioning",507,0,Test_BoundToRecording_RateAndPositioning)
    FTE(2,"FUNCTIONAL - Test_BoundToLive_RewindToBeginning",508,0,Test_BoundToLive_RewindToBeginning)
    FTE(2,"FUNCTIONAL - Test_BoundToLiveFullPB_RewindToBeginning",509,0,Test_BoundToLiveFullPB_RewindToBeginning)
    FTE(2,"FUNCTIONAL - Test_BoundToLivePerm_RewindToBeginning",510,0,Test_BoundToLivePerm_RewindToBeginning)
    FTE(2,"FUNCTIONAL - Test_BoundToRecording_RewindToBeginning",511,0,Test_BoundToRecording_RewindToBeginning)
    FTE(2,"FUNCTIONAL - Test_BoundToLive_PauseAtBeginning",512,0,Test_BoundToLive_PauseAtBeginning)
    FTE(2,"FUNCTIONAL - Test_BoundToLiveFullPB_PauseAtBeginning",513,0,Test_BoundToLiveFullPB_PauseAtBeginning)
    FTE(2,"FUNCTIONAL - Test_BoundToLivePerm_PauseAtBeginning",514,0,Test_BoundToLivePerm_PauseAtBeginning)
    FTE(2,"FUNCTIONAL - Test_BoundToRecording_PauseAtBeginning",515,0,Test_BoundToRecording_PauseAtBeginning)
    FTE(2,"FUNCTIONAL - Test_BoundToLive_SlowAtBeginning",516,0,Test_BoundToLive_SlowAtBeginning)
    FTE(2,"FUNCTIONAL - Test_BoundToLiveFullPB_SlowAtBeginning",517,0,Test_BoundToLiveFullPB_SlowAtBeginning)
    FTE(2,"FUNCTIONAL - Test_BoundToLive_FastForwardToEnd",518,0,Test_BoundToLive_FastForwardToEnd)
    FTE(2,"FUNCTIONAL - Test_BoundToLivePerm_FastForwardToEnd",519,0,Test_BoundToLivePerm_FastForwardToEnd)
    FTE(2,"FUNCTIONAL - Test_BoundToRecording_FastForwardToEnd",520,0,Test_BoundToRecording_FastForwardToEnd)
 
    FTH(1,"Helper tests")
    FTE(2,"FUNCTIONAL - Helper_GeneratePermRecording",600,0,Helper_GeneratePermRecording)
    FTE(2,"FUNCTIONAL - Helper_GeneratePermRecording2",601,0,Helper_GeneratePermRecording2)

    FTH(1,"Source and Sink interaction")
    FTE(2,"FUNCTIONAL - PreviousPlaybackDuringTemporaryRecord",700,0,PreviousPlaybackDuringTemporaryRecord)
    FTE(2,"FUNCTIONAL - PreviousPlaybackDuringPermanentRecord",701,0,PreviousPlaybackDuringPermanentRecord)
    FTE(2,"FUNCTIONAL - Test_BoundToLive_StopSinkDuringSource",702,0,Test_BoundToLive_StopSinkDuringSource)
    FTE(2,"FUNCTIONAL - Test_BoundToLiveFullPB_StopSinkDuringSource",703,0,Test_BoundToLiveFullPB_StopSinkDuringSource)
    FTE(2,"FUNCTIONAL - Test_BoundToLivePerm_StopSinkDuringSource",704,0,Test_BoundToLivePerm_StopSinkDuringSource)
    FTE(2,"FUNCTIONAL - Test_BoundToRecording_StopSinkDuringSource",705,0,Test_BoundToRecording_StopSinkDuringSource)

END_FTE

#endif // !__FT_H__ || __GLOBALS_CPP__

