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
//  VMRTest function table
//
//
////////////////////////////////////////////////////////////////////////////////

#include "VMRTest.h"
#include "TestDescParser.h"
#include "VMRTestDescParser.h"
//we need to include this file so that the functions under it can be built individually for each test
//if this is not included there will be linker errors
#include "Verifier.h"

GraphFunctionTableEntry g_lpLocalFunctionTable[] = 
{
    // Video Mixing Renderer Interface Tests
    {L"Video Mixing Renderer - IVMRFilterConfig test", L"IVMRFilterConfigTest", IVMRFilterConfigTest, ParseVMRTestDesc},
    {L"Video Mixing Renderer - IVMRMixerBitmap test", L"IVMRMixerBitmapTest", IVMRMixerBitmapTest, ParseVMRTestDesc},
    {L"Video Mixing Renderer - IVMRMixerControl test", L"IVMRMixerControlTest", IVMRMixerControlTest, ParseVMRTestDesc},
    {L"Video Mixing Renderer - IVMRWindowlessControl test", L"IVMRWindowlessControlTest", IVMRWindowlessControlTest, ParseVMRTestDesc},

    // Video Mixing Renderer Graph Build Tests
    {L"Video Mixing Renderer - Graph build test", L"VMRGraphBuildTest", VMRGraphBuildTest, ParseVMRTestDesc},
    {L"Video Mixing Renderer - E2E Mixing test", L"VMRPlaybackTest", VMRPlaybackTest, ParseVMRTestDesc},

    // Video Mixing Renderer E2E blending tests
    {L"Video Mixing Renderer - Blending test", L"VMRSimpleMixingTest", VMRSimpleMixingTest, ParseVMRTestDesc},
    {L"Video Mixing Renderer - Blend static bitmap on the video stream", L"VMRBlendStaticBitmapTest", VMRBlendStaticBitmapTest, ParseVMRTestDesc},
    {L"Video Mixing Renderer - Blending two video streams", L"VMRBlendVideoStreamTest", VMRBlendVideoStreamTest, ParseVMRTestDesc},
    {L"Video Mixing Renderer - Blending test using custom AP", L"VMRBlendUseCustomAPTest", VMRBlendUseCustomAPTest, ParseVMRTestDesc},
    {L"Video Mixing Renderer - Blending verification test using custom AP", L"VMRBlendUseCustomAPMultiGraphTest", VMRBlendUseCustomAPMultiGraphTest, ParseVMRTestDesc},

    // Video Mixing Renderer Secure playback tests
    {L"Video Mixing Renderer - mixing of DRM/non-DRM files in normal and secure chamber", L"VMRSecurePlayback", RestrictedGB_VMR_Test, ParseVMRTestDesc},

    // Video Mixing Renderer E2E Misc tests
    {L"Video Mixing Renderer - Misc test", L"WLDeletePlaybackWnd", WLDeletePlaybackWnd, ParseVMRTestDesc},

    // Window message tests
    {L"Video Renderer - run, pause, then occlude by other windows", L"VMR_Run_Pause_OccludingWindow_Test", VMR_Run_Pause_OccludingWindow_Test, ParseVMRTestDesc},
    {L"Video Renderer - run, pause, then occlude by other windows", L"VMR_Run_UntilKilled_Test", VMR_Run_UntilKilled_Test, ParseVMRTestDesc},

    // Video Mixing Renderer IVideoFramStep Test
    {L"Video Mixing Renderer - IVideoFrameStep", L"VMRVideoFrameStepTest", VMRVideoFrameStepTest, ParseVMRTestDesc},
    {L"Video Mixing Renderer - IVideoFrameStep, no custom AP", L"VMRVideoFrameStepNoAPTest", VMRVideoFrameStepNoCustomAPTest, ParseVMRTestDesc},

};
int g_nLocalTestFunctions = countof(g_lpLocalFunctionTable);
