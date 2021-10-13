//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 * Tux Function Table
 */

#define BEGIN_FTE(NAME) FUNCTION_TABLE_ENTRY NAME[] = {
#define FTH(a, b) { TEXT(b), a, 0, 0, NULL },
#define FTE(a, b, c, d) { TEXT(b), a, 0, c, d },
#define END_FTE { NULL, 0, 0, 0, NULL } };

BEGIN_FTE(g_lpFTE)
FTH(0, "Waveform Audio Tests")
FTE(1, "Build Verification Test", 		100,	BVT)
FTH(0, "Waveform Audio Playback Tests")
FTE(1, "Easy Playback",				1000,	EasyPlayback)
FTE(1, "Playback Capabilities", 		2000,	PlaybackCapabilities)
FTE(1, "Playback", 				2001,	Playback)
FTE(1, "Playback Notifications", 		2002,	PlaybackNotifications)
FTE(1, "Playback Using Extended Functions", 	2003,	PlaybackExtended)
FTH(0, "Waveform Audio Capture Tests")
FTE(1, "Capture Capabilities", 			3000,	CaptureCapabilities)
FTE(1, "Capture", 				3001,	Capture)
FTE(1, "Capture Notifications", 		3002,	CaptureNotifications)
FTH(0, "Volume Control Tests")
FTE(1, "Test Volume Control", 	4000, 	TestVolume)
END_FTE



