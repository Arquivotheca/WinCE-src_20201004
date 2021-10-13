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
//  VRTest test declarations
//
//
////////////////////////////////////////////////////////////////////////////////

#ifndef VMR_TEST_H
#define VMR_TEST_H

#include <tux.h>
#include "VMRTestDesc.h"
#include "globals.h"

// VMR Interface Test functions
TESTPROCAPI 
IVMRFilterConfigTest( UINT uMsg, 
                      TPPARAM tpParam, 
                      LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI 
IVMRMixerControlTest( UINT uMsg, 
                      TPPARAM tpParam, 
                      LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI 
IVMRMixerBitmapTest( UINT uMsg, 
                      TPPARAM tpParam, 
                      LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI 
IVMRWindowlessControlTest( UINT uMsg, 
                           TPPARAM tpParam, 
                           LPFUNCTION_TABLE_ENTRY lpFTE );

//
// Build a VMR graph with multiple video streams
//
TESTPROCAPI 
VMRGraphBuildTest( UINT uMsg, 
                   TPPARAM tpParam, 
                   LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI 
VMRPlaybackTest( UINT uMsg, 
                 TPPARAM tpParam, 
                 LPFUNCTION_TABLE_ENTRY lpFTE );

// VMR E2E blending tests
TESTPROCAPI 
VMRBlendStaticBitmapTest( UINT uMsg, 
                          TPPARAM tpParam, 
                          LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI 
VMRBlendVideoStreamTest( UINT uMsg, 
                         TPPARAM tpParam, 
                         LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI 
VMRBlendUseCustomAPTest( UINT uMsg, 
                         TPPARAM tpParam, 
                         LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI 
VMRBlendUseCustomAPMultiGraphTest( UINT uMsg, 
                         TPPARAM tpParam, 
                         LPFUNCTION_TABLE_ENTRY lpFTE );

TESTPROCAPI 
VMRSimpleMixingTest( UINT uMsg, 
                     TPPARAM tpParam, 
                     LPFUNCTION_TABLE_ENTRY lpFTE );

// VMR E2E Misc tests
TESTPROCAPI 
WLDeletePlaybackWnd( UINT uMsg, 
                     TPPARAM tpParam, 
                     LPFUNCTION_TABLE_ENTRY lpFTE );

// This test builds the graph for the media specified. The graph is now in Stopped state.
// 0. Set the graph in Run state
// 1. Set the graph in Pause state
// 2. Create a window and move it to occlude portions of the video window
// 3. Ensure that the screen gets refreshed when the window (or portion) is made visible
// 4. The test fails if any of the verifications fail.
// Config File: Specify the media file to be used, the list of positions to be set and the type of verifications required.
TESTPROCAPI 
VMR_Run_Pause_OccludingWindow_Test(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI 
VMR_Run_UntilKilled_Test(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

//VMR IVideoFrameStep Test
TESTPROCAPI 
VMRVideoFrameStepTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI 
VMRVideoFrameStepNoCustomAPTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

// playback mix of drm, non drm in normal/secure chamber
TESTPROCAPI 
RestrictedGB_VMR_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

#endif
