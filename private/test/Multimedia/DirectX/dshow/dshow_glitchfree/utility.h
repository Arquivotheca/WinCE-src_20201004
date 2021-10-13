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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#pragma once

#include <qnetwork.h>
#include <dshow.h>
#include "PerfTest.h"

//Uncomment this if you want to get detailed log of the test as it runs.
//Uncommenting this will effect performance, so only do it in case of debugging

//#define ENABLE_LOG

#ifdef ENABLE_LOG

#define LOGZONES LOG

#else

#define LOGZONES 

#endif

//creates the filter graph and initializes any variables
HRESULT CreateFilterGraph(TCHAR* url[], FilterGraph *pFG, PerfTestConfig* pConfig);

//cleans up what CreateFilterGraph did
void ReleaseFilterGraph(FilterGraph *pFG);

//figure out the URL to play from based on protocol
string LocationToPlay(const TCHAR* szProtocolName,  const CMediaContent* pMediaContent);
void LocationToPlay(TCHAR* fileName, const TCHAR* szProtocolName,  const CMediaContent* pMediaContent);

//returns a reference to the filter that has the asked for interface
HRESULT FindInterfaceOnGraph( IGraphBuilder *pFilterGraph, REFIID riid, void **ppInterface, 
									IEnumFilters* pEnumFilter, BOOL begin );
//returns a reference to the filter that implements the IAMDroppedFrames interface
HRESULT FindDroppedFrames(REFIID refiid, IGraphBuilder* pGraph, IAMDroppedFrames** pDroppedFrames);
//returns a reference to the filter that implements the IQualProp interface
HRESULT FindQualProp(REFIID refiid, IGraphBuilder* pGraph, IQualProp** pQualProp);
//returns a reference to the filter that implements the IAMAudioRendererStats interface
HRESULT FindAudioGlitch(REFIID refiid, IGraphBuilder* pGraph, IAMAudioRendererStats** pDroppedFrames);
// switch = 1 enables bandwidth switching, 0 disables
bool EnableBWSwitch(bool bwswitch);

//used to find tokens while parsing the command line
TCHAR* GetNextToken(TCHAR* string, int* pos, TCHAR delimiter);

//set the max back buffers registry key
HRESULT SetMaxVRBackBuffers(int dwMaxBackBuffers);
//tells the VR what mode to use
HRESULT SetVideoRendererMode(FilterGraph *pFG, DWORD vrMode);