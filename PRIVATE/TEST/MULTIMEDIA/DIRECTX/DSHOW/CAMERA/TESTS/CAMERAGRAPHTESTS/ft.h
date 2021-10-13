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
//  TUXTEST TUX DLL
//
//  Module: ft.h
//          Declares the TUX function table and test function prototypes EXCEPT
//          when included by globals.cpp, in which case it DEFINES the function
//          table.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

// needed for the camera components
#include "captureframework.h"

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
    FTH(0, "Graph building tests")
    FTE(1,     "Graph building tests everything.", 100, ALL_COMPONENTS, GraphBuild)
    FTE(1,     "Graph building tests VCF only.", 101, VIDEO_CAPTURE_FILTER, GraphBuild)
    FTE(1,     "Graph building tests Still image only.", 102, STILL_IMAGE_SINK, GraphBuild)
    FTE(1,     "Graph building tests Video Renderer only.", 103, VIDEO_RENDERER, GraphBuild)
    FTE(1,     "Graph building tests ASF mux only.", 104, FILE_WRITER, GraphBuild)
    FTE(1,     "Graph building tests ACF only.", 105, AUDIO_CAPTURE_FILTER, GraphBuild)
    FTE(1,     "Graph building tests WM9 encoder only.", 106, VIDEO_ENCODER, GraphBuild)
    FTE(1,     "Graph building tests Preview, no still.", 107, VIDEO_CAPTURE_FILTER | VIDEO_RENDERER, GraphBuild)
    FTE(1,     "Graph building tests Preview and still.", 108, VIDEO_CAPTURE_FILTER | STILL_IMAGE_SINK | VIDEO_RENDERER, GraphBuild)
    FTE(1,     "Graph building tests Still, no preview.", 109, VIDEO_CAPTURE_FILTER | STILL_IMAGE_SINK, GraphBuild)
    FTE(1,     "Graph building tests video/audio capture with encoding.", 110, VIDEO_CAPTURE_FILTER | FILE_WRITER | AUDIO_CAPTURE_FILTER | VIDEO_ENCODER | AUDIO_ENCODER, GraphBuild)
    FTE(1,     "Graph building tests video/audio capture, no encoding.", 111, VIDEO_CAPTURE_FILTER | FILE_WRITER | AUDIO_CAPTURE_FILTER, GraphBuild)
    FTE(1,     "Graph building tests video capture, no encoding.", 112, VIDEO_CAPTURE_FILTER | FILE_WRITER, GraphBuild)
    FTE(1,     "Graph building tests audio capture.", 113, AUDIO_CAPTURE_FILTER | FILE_WRITER, GraphBuild)

    FTH(0, "Graph state tests")
    FTE(1,     "Graph state tests everything.", 200, ALL_COMPONENTS, GraphState)
    FTE(1,     "Graph state tests VCF only.", 201, VIDEO_CAPTURE_FILTER, GraphState)
    FTE(1,     "Graph state tests Still image only.", 202, STILL_IMAGE_SINK, GraphState)
    FTE(1,     "Graph state tests Video Renderer only.", 203, VIDEO_RENDERER, GraphState)
    FTE(1,     "Graph state tests ASF mux only.", 204, FILE_WRITER, GraphState)
    FTE(1,     "Graph state tests ACF only.", 205, AUDIO_CAPTURE_FILTER, GraphState)
    FTE(1,     "Graph state tests video capture, encoding, and mux.", 206, VIDEO_CAPTURE_FILTER | VIDEO_ENCODER | FILE_WRITER, GraphState)
    FTE(1,     "Graph state tests Preview, no still.", 207, VIDEO_CAPTURE_FILTER | VIDEO_RENDERER, GraphState)
    FTE(1,     "Graph state tests Preview and still.", 208, VIDEO_CAPTURE_FILTER | STILL_IMAGE_SINK | VIDEO_RENDERER, GraphState)
    FTE(1,     "Graph state tests Still without preview.", 209, VIDEO_CAPTURE_FILTER | STILL_IMAGE_SINK, GraphState)
    FTE(1,     "Graph state tests video/audio capture with encoding.", 210, VIDEO_CAPTURE_FILTER | FILE_WRITER | AUDIO_CAPTURE_FILTER | VIDEO_ENCODER | AUDIO_ENCODER, GraphState)
    FTE(1,     "Graph state tests video/audio capture, no encoding.", 211, VIDEO_CAPTURE_FILTER | FILE_WRITER | AUDIO_CAPTURE_FILTER, GraphState)
    FTE(1,     "Graph state tests video capture, no encoding.", 212, VIDEO_CAPTURE_FILTER | FILE_WRITER, GraphState)
    FTE(1,     "Graph state tests audio capture.", 213, AUDIO_CAPTURE_FILTER | FILE_WRITER, GraphState)
    FTE(1,     "Graph state tests everything with simultaneous control", 214, ALL_COMPONENTS | OPTION_SIMULT_CONTROL, GraphState)
    FTE(1,     "Graph state tests everything with intelligent sink connect for video renderer", 215, ALL_COMPONENTS_INTELI_CONNECT_VIDEO, GraphState)
    FTE(1,     "Graph state tests everything with simultaneous control, and intelligent connect", 216, ALL_COMPONENTS_INTELI_CONNECT_VIDEO | OPTION_SIMULT_CONTROL, GraphState)

    FTH(0, "Preview tests")
    FTE(1,     "Preview verification tests, all components.", 300, ALL_COMPONENTS, VerifyPreview)
    FTE(1,     "Preview verification tests, no still.", 301, VIDEO_CAPTURE_FILTER | VIDEO_RENDERER, VerifyPreview)
    FTE(1,     "Preview verification tests, Preview and still.", 302, VIDEO_CAPTURE_FILTER | STILL_IMAGE_SINK | VIDEO_RENDERER, VerifyPreview)
    FTE(1,     "Preview verification tests, all components with intelligent sink connect.", 303, ALL_COMPONENTS_INTELI_CONNECT_VIDEO, VerifyPreview)
    FTE(1,     "Preview verification tests with rotation set before graph build, all components.", 304, ALL_COMPONENTS, VerifyPreviewRotationStatic)
    FTE(1,     "Preview verification tests with rotation changed during graph run, all components.", 305, ALL_COMPONENTS, VerifyPreviewRotationDynamic)
    FTE(1,     "Preview verification tests with rotation set before graph build, all components.", 306, ALL_COMPONENTS, VerifyPreviewRotationStatic_NoStretch)
    FTE(1,     "Preview verification tests with rotation changed during graph run, all components.", 307, ALL_COMPONENTS, VerifyPreviewRotationDynamic_NoStretch)
    FTE(1,     "Preview verification tests, verify all supported formats.", 308, ALL_COMPONENTS, VerifyPreviewFormats)
    FTE(1,     "Preview verification tests, verify all supported formats, no preview stretching.", 309, ALL_COMPONENTS, VerifyPreviewFormats_NoStretch)
    FTE(1,     "Preview verification tests, verify all supported formats setting post connect.", 310, ALL_COMPONENTS, VerifyPreviewFormatsPostConnect)

    FTH(0, "Still tests")
    FTE(1,     "Still verification tests, all components.", 400, ALL_COMPONENTS, VerifyStill)
    FTE(1,     "Still verification tests, still without preview.", 401, VIDEO_CAPTURE_FILTER | STILL_IMAGE_SINK, VerifyStill)
    FTE(1,     "Still verification tests, preview and still.", 402, VIDEO_CAPTURE_FILTER | STILL_IMAGE_SINK | VIDEO_RENDERER, VerifyStill)
    FTE(1,     "Still verification tests with rotation set before graph build, preview and still.", 403, ALL_COMPONENTS, VerifyStillRotationStatic)
    FTE(1,     "Still verification tests with rotation set during graph run, preview and still.", 404, ALL_COMPONENTS, VerifyStillRotationDynamic)
    FTE(1,     "Still verification tests, verify all supported formats.", 405, ALL_COMPONENTS, VerifyStillFormats)
    FTE(1,     "Still burst verification tests.", 406, ALL_COMPONENTS, VerifyStillBurst)
    FTE(1,     "Still verification tests, verify all supported formats setting post connect", 407, ALL_COMPONENTS, VerifyStillFormatsPostConnect)

    FTH(0, "ASF start and stop tests")
    FTE(1,     "ASF writing tests, all components.", 500, ALL_COMPONENTS, VerifyASFWriting)
    FTE(1,     "ASF writing tests, video/audio capture with encoding.", 501, VIDEO_CAPTURE_FILTER | FILE_WRITER | AUDIO_CAPTURE_FILTER | VIDEO_ENCODER | AUDIO_ENCODER, VerifyASFWriting)
    FTE(1,     "ASF writing tests, video/audio capture, with encoding and renderer.", 502, VIDEO_CAPTURE_FILTER | FILE_WRITER | AUDIO_CAPTURE_FILTER | VIDEO_ENCODER | VIDEO_RENDERER, VerifyASFWriting)
    FTE(1,     "ASF writing tests, video capture.", 503, VIDEO_CAPTURE_FILTER | FILE_WRITER | VIDEO_ENCODER, VerifyASFWriting)
    FTE(1,     "ASF writing tests, audio capture.", 504, AUDIO_CAPTURE_FILTER | FILE_WRITER | AUDIO_ENCODER, VerifyASFWriting)
    FTE(1,     "ASF writing tests, all components with simultaneous control.", 505, ALL_COMPONENTS | OPTION_SIMULT_CONTROL, VerifyASFWriting)
    FTE(1,     "ASF writing tests with static rotation, all components.", 506, ALL_COMPONENTS, VerifyASFStaticRotation)
    FTE(1,     "ASF writing tests with dynamic rotation, all components.", 507, ALL_COMPONENTS, VerifyASFDynamicRotation)
    FTE(1,     "ASF writing tests, testing all supported video formats.", 508, ALL_COMPONENTS, VerifyASFWritingFormats)
    FTE(1,     "ASF writing tests, testing all supported audioformats.", 509, ALL_COMPONENTS, VerifyASFWritingAudioFormats)
    FTE(1,     "ASF writing tests, frame dropping with all components.", 510, ALL_COMPONENTS, VerifyASFFrameDropping)
    FTE(1,     "ASF writing tests, ASF validation when graph stopped during capture.", 511, ALL_COMPONENTS, VerifyASFStopGraph)
    FTE(1,     "ASF writing tests, ASF validation when graph paused during capture.", 512, ALL_COMPONENTS, VerifyASFPauseGraph)
    FTE(1,     "ASF writing tests, ASF capture paused and resumed without graph paused, all components.", 513, ALL_COMPONENTS, VerifyASFContinued)
    FTE(1,     "ASF writing tests, ASF capture paused and resumed without graph paused, video only.", 514, VIDEO_CAPTURE_FILTER | FILE_WRITER | VIDEO_ENCODER, VerifyASFContinued)
    FTE(1,     "ASF writing tests, ASF capture paused and resumed without graph paused, audio only.", 515,  AUDIO_CAPTURE_FILTER | FILE_WRITER | AUDIO_ENCODER, VerifyASFContinued)
    FTE(1,     "ASF writing tests, ASF capture paused and resumed without graph paused, all components, simultanious control.", 516, ALL_COMPONENTS | OPTION_SIMULT_CONTROL, VerifyASFContinued)
    //The WMV encoder DMO does not support reconnects.
    //FTE(1,     "ASF writing tests, testing all supported video formats setting post connect.", 517, ALL_COMPONENTS, VerifyASFWritingFormatsPostConnect)

    FTH(0, "ASF timed tests")
    FTE(1,     "ASF writing tests timed stream capture, all components.", 600, ALL_COMPONENTS, VerifyASFWriting_TimedCapture)
    FTE(1,     "ASF writing tests timed stream capture, video/audio capture with encoding.", 601, VIDEO_CAPTURE_FILTER | FILE_WRITER | AUDIO_CAPTURE_FILTER | VIDEO_ENCODER | AUDIO_ENCODER, VerifyASFWriting_TimedCapture)
    FTE(1,     "ASF writing tests timed stream capture, video/audio capture, with encoding and renderer.", 602, VIDEO_CAPTURE_FILTER | FILE_WRITER | AUDIO_CAPTURE_FILTER | VIDEO_RENDERER | VIDEO_ENCODER, VerifyASFWriting_TimedCapture)
    FTE(1,     "ASF writing tests timed stream capture, video capture.", 603, VIDEO_CAPTURE_FILTER | FILE_WRITER | VIDEO_ENCODER, VerifyASFWriting_TimedCapture)
    FTE(1,     "ASF writing tests timed stream capture, audio capture.", 604, AUDIO_CAPTURE_FILTER | FILE_WRITER | AUDIO_ENCODER, VerifyASFWriting_TimedCapture)
    FTE(1,     "ASF writing tests timed stream capture, all components with simultaneous control.", 605, ALL_COMPONENTS | OPTION_SIMULT_CONTROL, VerifyASFWriting_TimedCapture)
    FTE(1,     "ASF writing tests timed stream capture with capture paused and resumed, all components.", 606, ALL_COMPONENTS, VerifyASFWriting_TimedContinuedCapture)
    FTE(1,     "ASF writing tests timed stream capture with capture paused and resumed, all components with simultanious control.", 607, ALL_COMPONENTS | OPTION_SIMULT_CONTROL, VerifyASFWriting_TimedContinuedCapture)

    FTH(0, "FindInterface tests")
    FTE(1,     "FindInterface tests all components.", 700, ALL_COMPONENTS, FindInterfaceTest)
    FTE(1,     "FindInterface tests video capture, encoding, and mux.", 701, VIDEO_CAPTURE_FILTER | VIDEO_ENCODER | FILE_WRITER, FindInterfaceTest)
    FTE(1,     "FindInterface tests Preview, no still.", 702, VIDEO_CAPTURE_FILTER | VIDEO_RENDERER, FindInterfaceTest)
    FTE(1,     "FindInterface tests Preview and still.", 703, VIDEO_CAPTURE_FILTER | STILL_IMAGE_SINK | VIDEO_RENDERER, FindInterfaceTest)
    FTE(1,     "FindInterface tests Still without preview.", 704, VIDEO_CAPTURE_FILTER | STILL_IMAGE_SINK, FindInterfaceTest)
    FTE(1,     "FindInterface tests video/audio capture with encoding.", 705, VIDEO_CAPTURE_FILTER | FILE_WRITER | AUDIO_CAPTURE_FILTER | VIDEO_ENCODER | AUDIO_ENCODER, FindInterfaceTest)
    FTE(1,     "FindInterface tests video/audio capture, no encoding.", 706, VIDEO_CAPTURE_FILTER | FILE_WRITER | AUDIO_CAPTURE_FILTER, FindInterfaceTest)
    FTE(1,     "FindInterface tests video capture, no encoding.", 707, VIDEO_CAPTURE_FILTER | FILE_WRITER, FindInterfaceTest)
    FTE(1,     "FindInterface tests audio capture, no encoding.", 708, AUDIO_CAPTURE_FILTER | FILE_WRITER, FindInterfaceTest)
    FTE(1,     "FindInterface tests all components with simultaneous control", 709, ALL_COMPONENTS | OPTION_SIMULT_CONTROL, FindInterfaceTest)
    FTE(1,     "FindInterface tests all components with intelligent sink connect for video renderer", 710, ALL_COMPONENTS_INTELI_CONNECT_VIDEO, FindInterfaceTest)
    FTE(1,     "FindInterface tests all components with simultaneous control, and intelligent connect", 711, ALL_COMPONENTS_INTELI_CONNECT_VIDEO | OPTION_SIMULT_CONTROL, FindInterfaceTest)

    FTH(0, "CameraControl property tests")
    FTE(1,     "CameraControl_Pan.", 800, CameraControl_Pan, TestCamControlProperties)
    FTE(1,     "CameraControl_Tilt.", 801, CameraControl_Tilt, TestCamControlProperties)
    FTE(1,     "CameraControl_Roll.", 802, CameraControl_Roll, TestCamControlProperties)
    FTE(1,     "CameraControl_Zoom.", 803, CameraControl_Zoom, TestCamControlProperties)
    FTE(1,     "CameraControl_Exposure.", 804, CameraControl_Exposure, TestCamControlProperties)
    FTE(1,     "CameraControl_Iris.", 805, CameraControl_Iris, TestCamControlProperties)
    FTE(1,     "CameraControl_Focus.", 806, CameraControl_Focus, TestCamControlProperties)
    FTE(1,     "CameraControl_Flash.", 807, CameraControl_Flash, TestCamControlProperties)

    FTH(0, "VideoProcAmp property tests")
    FTE(1,     "VideoProcAmp_Brightness.", 900, VideoProcAmp_Brightness, TestVidProcAmpProperties)
    FTE(1,     "VideoProcAmp_Contrast.", 901, VideoProcAmp_Contrast, TestVidProcAmpProperties)
    FTE(1,     "VideoProcAmp_Hue.", 902, VideoProcAmp_Hue, TestVidProcAmpProperties)
    FTE(1,     "VideoProcAmp_Saturation.", 903, VideoProcAmp_Saturation, TestVidProcAmpProperties)
    FTE(1,     "VideoProcAmp_Sharpness.", 904, VideoProcAmp_Sharpness, TestVidProcAmpProperties)
    FTE(1,     "VideoProcAmp_Gamma.", 905, VideoProcAmp_Gamma, TestVidProcAmpProperties)
    FTE(1,     "VideoProcAmp_ColorEnable.", 906, VideoProcAmp_ColorEnable, TestVidProcAmpProperties)
    FTE(1,     "VideoProcAmp_WhiteBalance.", 907, VideoProcAmp_WhiteBalance, TestVidProcAmpProperties)
    FTE(1,     "VideoProcAmp_BacklightCompensation.", 908, VideoProcAmp_BacklightCompensation, TestVidProcAmpProperties)

END_FTE

////////////////////////////////////////////////////////////////////////////////

#endif // !__FT_H__ || __GLOBALS_CPP__
