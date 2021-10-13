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

#ifndef _AVGlitchEvents_h
#define _AVGlitchEvents_h

#include <evcode.h>

#define AV_GLITCH_EVENT_BASE		EC_USER | 5981

// All of the events carry the following data payloads:
//
// Parameter 1:   DWORD = GetTickCount() value for when the glitch was spotted
// Parameter 2:   INT32 = measure of the severity of the issue
//					- milliseconds early/late for an extremely early/late sample
//					- clock rate ratio times 100 for an unusual clock slaving rate
//					- milliseconds duration of an unusually long read/write
//					- number of samples discarded

typedef enum AV_GLITCH_EVENT
{
	AV_GLITCH_VIDEO_DISCARD = AV_GLITCH_EVENT_BASE,
	AV_GLITCH_VIDEO_BLOCK,				// video is blocked (e.g., by setting the output rectangle to 0x0
	AV_GLITCH_VIDEO_BAD_TIMESTAMP,		// so far off as to suggest a bug, e.g., PTS rollover bug
	AV_GLITCH_VIDEO_NO_LIPSYNC,			// time-stamps vs clock off by enough to make lipsync impossible (say, 2 seconds)
	AV_GLITCH_VIDEO_LIPSYNC_DRIFT,		// time-stamps vs clock off enough that lipsync will look wierd (say, 100 ms)
	AV_GLITCH_AUDIO_DISCARD,			// discarded 1 or more audio samples
	AV_GLITCH_AUDIO_BLOCK,				// internal muting turned on/off:  1 means start, 0 means end
	AV_GLITCH_AUDIO_BAD_TIMESTAMP,		// so far off as to suggest a bug, e.g., PTS rollover bug
	AV_GLITCH_AUDIO_NO_LIPSYNC,			// time-stamps vs clock off by enough to make lipsync impossible (say, 2 seconds)
	AV_GLITCH_AUDIO_LIPSYNC_DRIFT,		// time-stamps vs clock off enough that lipsync will look wierd (say, 100 ms)
	AV_GLITCH_AUDIO_MUTE,				// externally requested audio mute:  param2 = 1 means start-muted, 0 means end-mute
	AV_GLITCH_CLOCK_SLAVING,			// unusual rate between clocks
	AV_GLITCH_LONG_DVR_WRITE,			// dangerously long sample-write in the DVR engine
	AV_GLITCH_LONG_DVR_READ,			// dangerously long sample-read
	AV_GLITCH_DVR_ERROR_HALT,			// the DVR playback halted due to an error
	AV_GLITCH_DVR_LOAD_FAILED,			// DVR playback	refused to play the requested content

	// Non-glitches but reported periodically the same way in order to allow other code to deduce
	// whether or not there is an error:

	AV_NO_GLITCH_AUDIO_IS_ARRIVING,		// audio is being received at the audio lip sync filter
	AV_NO_GLITCH_VIDEO_IS_ARRIVING,		// video is being received at the alpha overlay mixer
	AV_NO_GLITCH_AV_IS_BEING_CAPTURED,	// a/v is being captured to disk

	// Not sent periodically --- sent once -- but also not an error:
	AV_NO_GLITCH_DVR_NORMAL_SPEED,		// sent when changing between 1x and non-1x rate. Param is TRUE
										// iff switching to normal speed
	AV_NO_GLITCH_DVR_RUNNING,			// sent when DVR playback starts running
	AV_NO_GLITCH_DVR_STOPPED			// sent when DVR playback pauses or stops
} AV_GLITCH_EVENT;

#endif /* _AVGlitchEvents_h */
