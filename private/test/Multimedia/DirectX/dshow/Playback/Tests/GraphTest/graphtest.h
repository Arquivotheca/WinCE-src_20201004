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
//  GraphTest helper class
//
//
////////////////////////////////////////////////////////////////////////////////

#ifndef GRAPH_TEST_H
#define GRAPH_TEST_H

#include <tux.h>
#include "TestDesc.h"
#include "globals.h"

// Helper functions
HRESULT RandomStateChangeTest(TestGraph& testGraph, int nStateChanges, FILTER_STATE initialState, int seed, DWORD dwTimeBetwenStates);

// Test functions

// Basic Graph Scenarios

// This test CoCreates a filter graph and queries the commonly used playback interfaces.
// This test fails if the essential interfaces are not retrieved.
// IMediaControl, IMediaEvent, IMediaSeeking are considered to be essential
// Config File: no dependency
TESTPROCAPI EmptyGraphQueryInterfaceTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// This test adds a source filter for the media specified in the config file.
// The test fails if it is unable to add the source filter.
// Config File: Specify media file to be used
TESTPROCAPI AddSourceFilterTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// This test adds a source filter for the unsupported media file specified in the config file.
// This test fails if adding the source filter succeeds or if there is an exception/hang/crash.
// Config File: Specify the media file to be used.
TESTPROCAPI AddUnsupportedSourceFilterTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// This test builds a filter graph using RenderFile for the media specified in the config file.
// This test fails if RenderFile returns an error
// Config File: Specify the media file to be used and the filters to be expected to be present in the filter graph.
TESTPROCAPI BuildGraphTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// This test builds and destryos a filter graph using RenderFile multiple times for the media file specified in the config file.
// This test fails if any of the RenderFile calls return an error.
// Config File: Specify the media file to be used.
TESTPROCAPI BuildGraphMultipleTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// This test builds a graph using RenderFile and queries the essential interfaces for the media file specified in the config file.
// This test fails if either the RenderFile or the QueryInterface fails.
// Config File: Specify the media file to be used.
TESTPROCAPI BuildGraphQueryInterfaceTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// This test tries to build a filter graph using RenderFile for a media type that is unsupported.
// This test fails if RenderFile succeeds or if the test/hangs/crashes
// Config File: Specify the unsupported media file
TESTPROCAPI BuildGraphUnsupportedMediaTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// This test tries to build multiple independent filter graphs using RenderFile
// This test fails if any of the RenderFile calls fail
// Config File: Specify the media files to be used
TESTPROCAPI BuildMultipleGraphTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// This test adds a source filter for the media file specified and then calls Render on each unconnected output pin in the graph.
// This test fails if any the calls to Render fails.
// Config File: Specify the media file to be used.
TESTPROCAPI RenderPinTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);


// This test adds the specified source and renderer filter and connects them using intelligent connect
// This test fails if the connect fails.
// Config File: Specify the source filter and the renderer filter and the media to be used.
TESTPROCAPI ConnectSourceFilterToRendererTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// This test builds a graph for the specified media after pre-loading a filter into the graph.
// This test fails if either adding the filter fails, if building the graph fails or if the pre-loaded filter is not found to be connected if it is specified to be essential.
// Config File: Specify the filter to be preloaded, the media file and if the pre-loaded filter is essential for the graph.
TESTPROCAPI BuildGraphPreLoadFilterTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// This test builds a graph manually
// This test fails if any of the operations in building the graph fail.
// Config File: Specify the graph to be built along with the media file.
TESTPROCAPI ManualGraphBuildTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// This test plays back the multiple media files specified and minimally verifies the playback.
// This test fails if either the graph build fails or if the playback verification fails.
// Config File: Specify the media files to be played. Set the verification(s) required, set the start/stop.
TESTPROCAPI MultiplePlaybackTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);


// This test plays back the media specified and queries the manual tester whether the playback was alright using a dialog box.
// This test fails if either the graph build fails or if the tester decides that playback had issues.
// Config File: Specify the media file to be played.
TESTPROCAPI ManualPlaybackTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// This test plays back the media specified and verifies that playback occurs correctly.
// This test fails if either the graph build fails or if the playback verification fails.
// Config File: Specify the media file to be played. Set the verification(s) required, set the start/stop.
TESTPROCAPI PlaybackTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// This test plays back the media specified for say 1000 times and verifies if there is any memory leak
// This test fails if either memory leak is found out or if any dshow api throws error
// Config File: Specify the media file to be played.
TESTPROCAPI PlaybackLeakTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

//This test tries to play media file from MediaCube.
// This test plays back the media specified and verifies that playback occurs correctly.
// This test fails if either the graph build fails or if the playback verification fails.
// Config File: Specify the media file to be played. Set the verification(s) required, set the start/stop.
TESTPROCAPI MediaCubePlaybackTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);


// This test plays back the media specified.
// This test fails if either the graph build fails.
// Config File: Specify the media file to be played. It will create a tmp folder under root.
TESTPROCAPI PlaybackLongPathTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI PlaybackDurationTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// This test builds the graph for the media specified and takes the graph through many state transitions.
// The test fails if either the graph build fails or if the state changes fail.
// Config File: Specify the media file to be played, the pattern of state changes, and the number of state changes to be tested.
TESTPROCAPI StateChangeTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// This test builds the graph for the media specified. The graph is now in Stopped state.
// 1. It runs the graph for an arbitrary time
// 2. Enables the verification.
// 3. Pauses the graph
// 4. Runs the graph again
// 5. It checks the verifications specified.
// 4. The test fails if any of the verifications fail.
// Config File: Specify the media file to be used, the list of positions to be set and the type of verifications required.
TESTPROCAPI Run_Pause_Run_Test(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// This test Performs IGraphBuilder interface test on single media file
TESTPROCAPI IGraphBuilderTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

// This test runs MediaControl test on media file (AVI, MPeg, Wav etc)
TESTPROCAPI IMediaControlTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

// This test tests IEnumFilters which enumerates the filters in a filter graph.
TESTPROCAPI IEnumFiltersTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

// Test the Clone function of IEnumFilters.
TESTPROCAPI IEnumFiltersCloneTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

// This test tests EnumPins which retrieves pins on a filter.
TESTPROCAPI IEnumPinsTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

// Test the Clone function of IEnumPins
TESTPROCAPI IEnumPinsCloneTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI IEnumRegFiltersTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

// This test performs the Media event interface tests for the media file specified.
TESTPROCAPI IMediaEventTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

// This test performs the Media event test for media file.
TESTPROCAPI IMediaEventExTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

// Tests graph filter manager.
TESTPROCAPI IGraphBuilder_IFilterGraph2_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

// Test IReferenceClock
TESTPROCAPI IReferenceClockTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI IReferenceClock_GetTime_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI IReferenceClock_AdviseTime_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

// Test IGraphVersion
TESTPROCAPI IGraphVersionTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

// Test IAudioControl
TESTPROCAPI IAudioControlTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
TESTPROCAPI IAudioControlTest2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// IGraphBuilder2
TESTPROCAPI
IGraphBuilder2_AddFilterByCLSID_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI
IGraphBuilder2_FindInterfacesOnGraph_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI
IGraphBuilder2_AddUnsignedFilterToGraph_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

//Test IAMMediaContent
TESTPROCAPI IAMMediaContentTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

//Test IAMMediaContentEx
TESTPROCAPI IAMMediaContentExTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

//Test IAMMediaList and IAMPlayListItem
TESTPROCAPI IAMPlayListTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

//Test IAMOpenProgress
TESTPROCAPI IAMOpenProgressTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

//Test IAMNetShowConfig
TESTPROCAPI IAMNetShowConfigTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

//Test IBasicAudio
TESTPROCAPI IBasicAudioTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

//Test IFileSourceFilter
TESTPROCAPI IFileSourceFilterTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

//Test IAMNetShowExProps
TESTPROCAPI IAMNetShowExPropsTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

//Test IGraphBuilder
TESTPROCAPI IGraphBuilderTestAbort(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

//Test IAMExtendedSeeking
TESTPROCAPI IAMExtendedSeekingTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

//Test IAMDroppedFrames
TESTPROCAPI IAMDroppedFramesTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

//Test IQualProp
TESTPROCAPI IQualPropTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

//Test IAudioRendererWaveOut
TESTPROCAPI IAudioRendererWaveOutTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

//Test IAMAudioRendererStats
TESTPROCAPI IAMAudioRendererStatsTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

//Test IAMNetworkStatus
TESTPROCAPI IAMNetworkStatusTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// Non-exposed interfaces when using restricted graph builder
TESTPROCAPI
NonProxyedInterface_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

#endif
