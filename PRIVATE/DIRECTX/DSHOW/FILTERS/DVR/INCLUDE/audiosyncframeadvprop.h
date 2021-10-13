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
**  AudioSyncFrameAdvProp.h:
**
**
**  Defines a property set and property used to negotiate what
**  to do rendering-wise after 
*/

#ifndef _AudioSyncFrameAdvProp_h
#define _AudioSyncFrameAdvProp_h

// {89A97479-9E5D-4719-BFC1-744FD7749610}
DEFINE_GUID(PVR_AUDIO_PROPSETID_LipSyncNegotiation, 
0x89a97479, 0x9e5d, 0x4719, 0xbf, 0xc1, 0x74, 0x4f, 0xd7, 0x74, 0x96, 0x10);

typedef enum _PVR_AUDIO_LIP_SYNC_PROPERTY_ID
{
	// A video renderer wanting to ignore the stream clock when rendering 
	// to carry out frame-advance [the current non-DirectShow-standard
	// implementation we are using because we are on an old version of
	// DirectShow where the standard frame-advance framework does not exist]
	// must first confirm that audio knows what to do. The video renderer
	// must inform the audio renderer of the start of frame-advance, of
	// each sample rendered as part of the frame-advance, and the end of
	// frame-advance. When reporting the rendering of a sample, the video
	// renderer must report the [original] stream start time of the sample.

	PVR_AUDIO_PROPID_BeginFrameAdvance = 0,				// no data
	PVR_AUDIO_PROPID_RenderedFrameWithOffset = 1,		// LONGLONG as data -- original stream start time of sample
	PVR_AUDIO_PROPID_EndFrameAdvance = 2				// no data
} PVR_AUDIO_LIP_SYNC_PROPERTY_ID;

#endif /* _AudioSyncFrameAdvProp_h */
