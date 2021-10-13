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
#pragma once

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the DVRENGINE_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// DVRENGINE_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef DVRENGINE_EXPORTS
#define DVRENGINE_API __declspec(dllexport)
#else
#define DVRENGINE_API __declspec(dllimport)
#endif

// Values that can be overridden via the registry :

extern "C" REFERENCE_TIME g_rtMinimumBufferingUpstreamOfLive;
extern "C" double g_dblMPEGReaderFullFrameRate;
extern "C" double g_dblMPEGDecoderDriverKeyFrameRatio;
extern "C" double g_dblMPEGDecoderDriverKeyFramesPerSecond;
extern "C" double g_dblMaxFullFrameRateForward;
extern "C" double g_dblMaxFullFrameRateBackward;
extern "C" double g_dblMaxSupportedSpeed;
extern "C" DWORD g_dwReaderPriority;
extern "C" DWORD g_dwWriterPriority;
extern "C" DWORD g_dwLazyReadOpenPriority;
extern "C" DWORD g_dwWritePreCreatePriority;
extern "C" DWORD g_dwLazyDeletePriority;
extern "C" DWORD g_dwStreamingThreadPri;
extern "C" ULONGLONG g_uhyMaxPermanentRecordingFileSize;
extern "C" ULONGLONG g_uhyMaxTemporaryRecordingFileSize;
extern "C" DWORD g_dwMinAcceptableClockDebug;
extern "C" DWORD g_dwMaxAcceptableClockDebug;
extern "C" DWORD g_dwMinAcceptableClockRetail;
extern "C" DWORD g_dwMaxAcceptableClockRetail;
extern "C" BOOL  g_bEnableFilePreCreation;
extern "C" DWORD g_dwExtraSampleProducerBuffers;
