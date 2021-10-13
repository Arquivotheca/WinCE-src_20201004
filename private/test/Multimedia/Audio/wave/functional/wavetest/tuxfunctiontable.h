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

/*
 * Tux Function Table
 */
#include "BadDeviceID.h"
#include "BadGUID.h"
#include "BadHandles.h"
#include "International.h"
#include "test_wavetest.h"
#include "WaveInterOp.h"
#include "InvalidBlockAlignment.h"

#define BEGIN_FTE(NAME) FUNCTION_TABLE_ENTRY NAME[] = {
#define FTH(a, b) { TEXT(b), a, 0, 0, NULL },
#define FTE(a, b, c, d) { TEXT(b), a, 0, c, d },
#define FTEX(a, b, c, d, e) { TEXT(b), a, c, d, e },
#define END_FTE { NULL, 0, 0, 0, NULL } };


#ifndef PLAYSOUND_SYNC
#define PLAYSOUND_SYNC 0
#endif 

#ifndef PLAYSOUND_ASYNC
#define PLAYSOUND_ASYNC 1
#endif

#ifndef SNDPLAYSOUND_SYNC
#define SNDPLAYSOUND_SYNC 2
#endif 

#ifndef SNDPLAYSOUND_ASYNC
#define SNDPLAYSOUND_ASYNC 3
#endif 

#ifndef WAVEOUT  
#define WAVEOUT 4
#endif 



BEGIN_FTE(g_lpFTE)
FTH(0, "Waveform Audio Tests")
FTE(1, "Build Verification Test",                 100,        BVT)
FTH(0, "Waveform Audio Playback Tests")
FTE(1, "Easy Playback",                          1000,        EasyPlayback)
FTE(1, "Playback Capabilities",                  2000,        PlaybackCapabilities)
FTE(1, "Playback",                               2001,        Playback)
FTE(1, "Playback Notifications",                 2002,        PlaybackNotifications)
FTE(1, "Playback Using Extended Functions",      2003,        PlaybackExtended)
FTE(1, "Playback Buffer Freed During Playback",  2004,        PlaybackVirtualFree)
FTE(1, "Playback Sample Rate Reporting", 2005, WaveOutTestReportedFormats)
FTEX(1, "Playback Initial Latency", (DWORD)&aliLatencyTests[0], 2006, PlaybackInitialLatency)
FTEX(1, "Playback Initial Latencies Series", (DWORD)&aliLatencyTests[0], 2007, PlaybackInitialLatencySeries )
FTE(1, "Playback Interoperability ", 2008, PlaybackInteroperability )
FTH(0, "Waveform Audio Capture Tests")
FTE(1, "Capture Capabilities",                    3000,        CaptureCapabilities)
FTE(1, "Capture",                                 3001,        Capture)
FTE(1, "Capture Notifications",                   3002,        CaptureNotifications)
FTE(1, "Capture Buffer Freed During Capture",     3003,        CaptureVirtualFree)
FTEX(1, "Capture Initial Latency", (DWORD)&aliLatencyTests[0], 3006, CaptureInitialLatency)
FTEX(1, "Capture Initial Latencies Series", (DWORD)&aliLatencyTests[0], 3007, CaptureInitialLatencySeries )
FTEX(1, "Capture Multiple Streams"        , (DWORD)&aliLatencyTests[0], 3008, CaptureMultipleStreams      )
FTH(0, "Volume Control Tests")
FTE(1, "Test Volume Control",                     4000,         TestVolume)
FTE(1, "Test waveOutSetVolume & waveOutGetVolume",4001, TestwaveOutSet_GetVolume )
FTH(0, "Power Management Tests")
FTEX(1, "Test Power Down/Up with PlaySound Synchronus ",         (DWORD)&aatTests[0], 5000,         TestPowerUpAndDown)
FTEX(1, "Test Power Down/Up with PlaySound Asynchronus",         (DWORD)&aatTests[1], 5001,         TestPowerUpAndDown)
FTEX(1, "Test Power Down/Up with sndPlaySound Synchronus",       (DWORD)&aatTests[2], 5002,         TestPowerUpAndDown)
FTEX(1, "Test Power Down/Up with sndPlaySound Asynchronus",      (DWORD)&aatTests[3], 5003,         TestPowerUpAndDown)
FTEX(1, "Test Power Down/Up with waveOutWrite",         (DWORD)&aatTests[4], 5004,         TestPowerUpAndDown)
FTH(0, "Mixing Tests")
FTE(1, "Playback Mixing", 6000, PlaybackMixing)
FTEX(1, "Capture Mixing" , (DWORD)&aliLatencyTests[0], 6001, CaptureMixing )
FTH(0, "International Tests")
FTE(1, "International Audio File Names", 7000, International )
FTH(0, "Invalid Parameter Tests")
FTE(1, "Verify Handle Validation", 8000, BadHandles )
FTE(1, "Verify Device ID Validation", 8001, BadDeviceID )
FTE(1, "Verify Invalid Block Alignment Handling", 8003, InvalidBlockAlignment )


END_FTE



