//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
/*
 * Tux Function Table
 */

#define BEGIN_FTE(NAME) FUNCTION_TABLE_ENTRY NAME[] = {
#define FTH(a, b) { TEXT(b), a, 0, 0, NULL },
#define FTE(a, b, c, d) { TEXT(b), a, 0, c, d },
#define END_FTE { NULL, 0, 0, 0, NULL } };

BEGIN_FTE(g_lpFTE)
FTH(0, "DirectSound Tests")
FTE(1, "Build Verification Test", 		100,	BVT)
FTH(0, "DirectSound Playback Tests")
FTE(1, "Playback Capabilities", 		2000,	PlaybackCapabilities)
FTE(1, "Playback", 				2001,	Playback)
FTE(1, "Playback Notifications", 		2002,	PlaybackNotifications)
FTE(1, "Playback Using Extended Functions", 	2003,	PlaybackExtended)
FTE(1, "Playback Software", 			2004,	Playback_Software)
FTE(1, "Playback Hardware (Streaming)", 	2005,	Playback_Hardware_Streaming)
FTE(1, "Playback Hardware (Static)", 		2006,	Playback_Hardware_Static)
#ifndef UNDER_CE
FTE(1, "Playback Primary", 			2007,	Playback_Primary)
#endif
FTH(0, "DirectSound Capture Tests")
FTE(1, "Capture Capabilities", 			3000,	CaptureCapabilities)
FTE(1, "Capture", 				3001,	Capture)
FTE(1, "Capture Notifications", 		3002,	CaptureNotifications)
END_FTE