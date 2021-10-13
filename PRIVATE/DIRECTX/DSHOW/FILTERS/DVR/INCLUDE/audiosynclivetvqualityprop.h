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
/*
**  AudioSyncLiveTVQualityProp.h:
**
**
**  Defines a property set and property used to inform the audio sync
**  filter that live tv is being captured in a mode that has the
**  normal versus unusually high jitter in the lag being outputted
**  from the (MPEG) encoder.
*/

#ifndef _AudioSyncLiveTVQualityProp_h
#define _AudioSyncLiveTVQualityProp_h

// {6F5958F6-990A-4c0b-96D9-44475F2A3754}
DEFINE_GUID(PVR_AUDIO_PROPSETID_LiveTV,
0x6f5958f6, 0x990a, 0x4c0b, 0x96, 0xd9, 0x44, 0x47, 0x5f, 0x2a, 0x37, 0x54);

typedef enum _PVR_LIVE_TV_AUDIO_PROPERTY_ID
{
	// One of the functions of a live/recorded tv lip sync filter
	// is to ensure that playback stays safely behind the point
	// at which the encoder (source) is generating data to ensure
	// that playback is not affected by the burstiness of the
	// encoder's delivery of output.  In other words, this is a
	// form of pre-roll.

	PVR_AUDIO_PROPID_LiveTVSourceJitter = 0		// DWORD as data -- holds a PVR_LIVE_TV_AUDIO_JITTER_CATEGORY

} PVR_LIVE_TV_AUDIO_PROPERTY_ID;

typedef enum _PVR_LIVE_TV_AUDIO_JITTER_CATEGORY
{
    PVR_LIVE_TV_AUDIO_JITTER_NORMAL = 0,
    PVR_LIVE_TV_AUDIO_JITTER_MODERATE = 1,
    PVR_LIVE_TV_AUDIO_JITTER_SEVERE = 2
} PVR_LIVE_TV_AUDIO_JITTER_CATEGORY;

#endif /* _AudioSyncLiveTVQualityProp_h */
