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
////////////////////////////////////////////////////////////////////////////////
//
//  GraphTest function table
//
//
////////////////////////////////////////////////////////////////////////////////

#include <clparse.h>
#include "logging.h"
#include "TuxGraphTest.h"
#include "GraphTest.h"
#include "TestDesc.h"
#include "TestDescParser.h"
//we need to include this file so that the functions under it can be built individually for each test
//if this is not included there will be linker errors
#include "Verifier.h"

GraphFunctionTableEntry g_lpLocalFunctionTable[] = 
{
	// Basic Graph building scenarios
	{L"Graph building - empty graph query interfaces", L"EmptyGraphQueryInterfaceTest", EmptyGraphQueryInterfaceTest, ParseBuildGraphTestDesc},
	{L"Graph building - adding a source filter", L"AddSourceFilterTest", AddSourceFilterTest, ParseBuildGraphTestDesc},
    {L"Graph building - adding a source filter - unsupported media", L"AddUnsupportedSourceFilterTest", AddUnsupportedSourceFilterTest, ParseBuildGraphTestDesc},
    {L"Graph building - build graph for supported media", L"BuildGraphTest", BuildGraphTest, ParseBuildGraphTestDesc},
	{L"Graph building - build graph supported media - multiple times", L"BuildGraphMultipleTest", BuildGraphMultipleTest, ParseBuildGraphTestDesc},
	{L"Graph building - build graph for supported media & query interfaces", L"BuildGraphQueryInterfaceTest", BuildGraphQueryInterfaceTest, ParseBuildGraphTestDesc},
    {L"Graph building - build graph for unsupported media", L"BuildGraphUnsupportedMediaTest", BuildGraphUnsupportedMediaTest, ParseBuildGraphTestDesc},

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
	{L"Graph playback - measure playback time", L"PlaybackDurationTest", PlaybackDurationTest, ParsePlaybackTestDesc},
	{L"Graph playback - multiple independent graphs", L"MultiplePlaybackTest", MultiplePlaybackTest, ParsePlaybackTestDesc},

	// State change tests
	{L"Graph playback - state change test", L"StateChangeTest", StateChangeTest, ParseStateChangeTestDesc},
	{L"Graph playback - run, pause, run", L"Run_Pause_Run_Test", Run_Pause_Run_Test, ParsePlaybackTestDesc},

};
int g_nLocalTestFunctions = countof(g_lpLocalFunctionTable);
