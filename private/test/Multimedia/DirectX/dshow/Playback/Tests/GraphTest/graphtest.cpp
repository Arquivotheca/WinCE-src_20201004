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
//  GraphTest function table
//
//
////////////////////////////////////////////////////////////////////////////////

#include "GraphTest.h"
#include "TestDescParser.h"
#include "GraphTestDescParser.h"
//we need to include this file so that the functions under it can be built individually for each test
//if this is not included there will be linker errors
#include "Verifier.h"

GraphFunctionTableEntry g_lpLocalFunctionTable[] = 
{
     // Graph Interface Tests
     {L"Filter Graph interface test - IGraphBuilder", L"IGraphBuilderTest", IGraphBuilderTest, ParseBuildGraphTestDesc},
     {L"Filter Graph interface test - IMediaControl", L"IMediaControlTest", IMediaControlTest, ParsePlaybackTestDesc},
     {L"Filter Graph interface test - IEnumFilters", L"IEnumFiltersTest", IEnumFiltersTest, ParsePlaybackTestDesc},
     {L"Filter Graph interface test - IEnumFilters Clone test", L"IEnumFiltersCloneTest", IEnumFiltersCloneTest, ParsePlaybackTestDesc},
     {L"Filter Graph interface test - IEnumPins", L"IEnumPinsTest", IEnumPinsTest, ParsePlaybackTestDesc},
     {L"Filter Graph interface test - IEnumPins Clone test", L"IEnumPinsCloneTest", IEnumPinsCloneTest, ParsePlaybackTestDesc},
     {L"Filter Graph interface test - IEnumRegFilters", L"IEnumRegFiltersTest", IEnumRegFiltersTest, ParsePlaybackTestDesc},
     {L"Filter Graph interface test - IMediaEvent", L"IMediaEventTest", IMediaEventTest, ParsePlaybackTestDesc},
     {L"Filter Graph interface test - IMediaEventEx", L"IMediaEventExTest", IMediaEventExTest, ParsePlaybackTestDesc},
     {L"Filter Graph interface test - Filter graph manager", L"FilterGraphMgrTest", IGraphBuilder_IFilterGraph2_Test, ParsePlaybackTestDesc},
     {L"Filter Graph interface test - IReferenceClock basic test", L"IReferenceClockTest", IReferenceClockTest, ParsePlaybackTestDesc},
     {L"Filter Graph interface test - IReferenceClock GetTime test", L"IReferenceClockGetTimeTest", IReferenceClock_GetTime_Test, ParsePlaybackTestDesc},
     {L"Filter Graph interface test - IReferenceClock AdviseTime test", L"IReferenceClockAdviseTimeTest", IReferenceClock_AdviseTime_Test, ParsePlaybackTestDesc},
     {L"Filter Graph interface test - IGraphVersion", L"IGraphVersionTest", IGraphVersionTest, ParsePlaybackTestDesc},

     // Basic Graph building scenarios
     {L"Graph building - empty graph query interfaces", L"EmptyGraphQueryInterfaceTest", EmptyGraphQueryInterfaceTest, ParseBuildGraphTestDesc},
     {L"Graph building - adding a source filter", L"AddSourceFilterTest", AddSourceFilterTest, ParseBuildGraphTestDesc},
     {L"Graph building - adding a source filter - unsupported media", L"AddUnsupportedSourceFilterTest", AddUnsupportedSourceFilterTest, ParseBuildGraphTestDesc},
     {L"Graph building - build graph for supported media", L"BuildGraphTest", BuildGraphTest, ParseBuildGraphTestDesc},
     {L"Graph building - build graph supported media - multiple times", L"BuildGraphMultipleTest", BuildGraphMultipleTest, ParseBuildGraphTestDesc},
     {L"Graph building - build graph for supported media & query interfaces", L"BuildGraphQueryInterfaceTest", BuildGraphQueryInterfaceTest, ParseBuildGraphTestDesc},
     {L"Graph building - build graph for unsupported media", L"BuildGraphUnsupportedMediaTest", BuildGraphUnsupportedMediaTest, ParseBuildGraphTestDesc},
     {L"Graph building - adding filters to the graph", L"AddFilterByCLSIDTest", IGraphBuilder2_AddFilterByCLSID_Test, ParseBuildGraphTestDesc},
     {L"Graph building - find interfaces on the graph", L"FindInterfacesOnGraphTest", IGraphBuilder2_FindInterfacesOnGraph_Test, ParseBuildGraphTestDesc},
     {L"Graph building - adding unsigned filter to the graph", L"AddUnsignedFilterToGraphTest", IGraphBuilder2_AddUnsignedFilterToGraph_Test, ParseBuildGraphTestDesc},

     // Advanced Graph building scenarios
     {L"Graph building - build multiple media graphs", L"BuildMultipleGraphTest", BuildMultipleGraphTest, ParseBuildGraphTestDesc},
     {L"Graph building - Render pin to a complete graph", L"RenderPinTest", RenderPinTest, ParseBuildGraphTestDesc},
     {L"Graph building - intelligent connect source to sink", L"ConnectSourceFilterToRendererTest", ConnectSourceFilterToRendererTest, ParseBuildGraphTestDesc},
     {L"Graph building - preferential filter loading", L"BuildGraphPreLoadFilterTest", BuildGraphPreLoadFilterTest, ParseBuildGraphTestDesc},
     {L"Graph building - manual graph building", L"ManualGraphBuildTest", ManualGraphBuildTest, ParseBuildGraphTestDesc},
     //{L"Graph building - check version change", L"GraphBuildCheckVersionTest", GraphBuildCheckVersionTest, ParseBuildGraphTestDesc},
     //{L"Graph building - build with filter reconnection", L"GraphBuildWithReconnectionTest", GraphBuildWithReconnectionTest, ParseBuildGraphTestDesc},

     // Playback tests
     {L"Graph playback - manual playback test", L"ManualPlaybackTest", ManualPlaybackTest, ParsePlaybackTestDesc},
     {L"Graph playback - end-end playback test", L"EndToEndPlaybackTest", PlaybackTest, ParsePlaybackTestDesc},
     {L"Graph playback - segmented playback test", L"SegmentedPlaybackTest", PlaybackTest, ParsePlaybackTestDesc},
     {L"Graph playback - long path playback test", L"PlaybackLongPathTest", PlaybackLongPathTest, ParsePlaybackTestDesc},
     {L"Graph playback - measure playback time", L"PlaybackDurationTest", PlaybackDurationTest, ParsePlaybackTestDesc},
     {L"Graph playback - multiple independent graphs", L"MultiplePlaybackTest", MultiplePlaybackTest, ParsePlaybackTestDesc},
     {L"Graph playback - MediaCube playback test", L"MediaCubePlaybackTest", MediaCubePlaybackTest, ParsePlaybackTestDesc},
     {L"Graph playback - end-end playback memory leak test", L"EndToEndPlaybackLeakTest", PlaybackLeakTest, ParsePlaybackTestDesc},
     
    // State change tests
     {L"Graph playback - state change test", L"StateChangeTest", StateChangeTest, ParseStateChangeTestDesc},
     {L"Graph playback - run, pause, run", L"Run_Pause_Run_Test", Run_Pause_Run_Test, ParsePlaybackTestDesc},

     // Interface tests
     {L"Interface - IAMMediaContent test", L"IAMMediaContentTest", IAMMediaContentTest, ParsePlaybackTestDesc},
     {L"Interface - IAMMediaContentEx test", L"IAMMediaContentExTest", IAMMediaContentExTest, ParsePlaybackTestDesc},
     {L"Interface - IAMPlayList test", L"IAMPlayListTest", IAMPlayListTest, ParsePlaybackTestDesc},
     {L"Interface - IAMOpenProgress test", L"IAMOpenProgressTest", IAMOpenProgressTest, ParsePlaybackTestDesc},
     {L"Interface - IBasicAudio test", L"IBasicAudioTest", IBasicAudioTest, ParseInterfaceTestDesc},
     {L"Interface - IFileSourceFilter test", L"IFileSourceFilterTest", IFileSourceFilterTest, ParseInterfaceTestDesc},
     {L"Interface - IAMNetShowExProps test", L"IAMNetShowExPropsTest", IAMNetShowExPropsTest, ParseInterfaceTestDesc},
     {L"Interface - IGraphBuilder test Abort", L"IGraphBuilderTestAbort", IGraphBuilderTestAbort, ParseInterfaceTestDesc},
     {L"Interface - IAMExtendedSeeking test", L"IAMExtendedSeekingTest", IAMExtendedSeekingTest, ParseInterfaceTestDesc},
     {L"Interface - IAMDroppedFrames test", L"IAMDroppedFramesTest", IAMDroppedFramesTest, ParseInterfaceTestDesc},
     {L"Interface - IQualProp test", L"IQualPropTest", IQualPropTest, ParseInterfaceTestDesc},
     {L"Interface - IAudioRendererWaveOut test", L"IAudioRendererWaveOutTest", IAudioRendererWaveOutTest, ParseInterfaceTestDesc},
     {L"Interface - IAMAudioRendererStats test", L"IAMAudioRendererStatsTest", IAMAudioRendererStatsTest, ParseInterfaceTestDesc},
     {L"Interface - IAMNetworkStatus test", L"IAMNetworkStatusTest", IAMNetworkStatusTest, ParseInterfaceTestDesc},
     {L"Interface - IAMNetShowConfig test", L"IAMNetShowConfigTest", IAMNetShowConfigTest, ParseInterfaceTestDesc},
     {L"Interface - Interface negative test for restricted graph builder", L"NonProxyedInterfaceTest", NonProxyedInterface_Test, ParseBuildGraphTestDesc},

     // IAudioControl test
     {L"Graph audio playback - IAudioControl test", L"IAudioControlTest", IAudioControlTest, ParseAudioControlTestDesc},
     {L"Graph audio playback - IAudioControl test", L"IAudioControlTest2", IAudioControlTest2, ParseAudioControlTestDesc},
};
int g_nLocalTestFunctions = countof(g_lpLocalFunctionTable);
