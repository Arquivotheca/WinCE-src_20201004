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
#include <windows.h>
#include <strmif.h>
#include <uuids.h>
#include <control.h>
#include <stdio.h>
#include <vfwmsgs.h>
#include <evcode.h>
#include <amvideo.h>
#include <dshow.h>
#include <evcode.h>
#include <atlcomcli.h>
#pragma once

#include <windows.h>

#define PLAY_EVENT_DURATION 90000
#define CONTENT_TYPE_CLEAR TEXT("clear")
#define CONTENT_TYPE_DRM_ND  TEXT("drm-nd")
#define CONTENT_TYPE_DRM_PD  TEXT("drm-pd")
#define MEDIA_TYPE_IMAGE   TEXT("image")
#define MEDIA_TYPE_AUDIO   TEXT("audio")
#define MEDIA_TYPE_VIDEO   TEXT("video")

class CDShowUtil
{
public:  

   HRESULT AudioChannelInfo(IGraphBuilder *pGB, int *pChannel);
   HRESULT FindInterfaceOnGraph(IGraphBuilder *pGB, REFIID id, void **Interface );
   HRESULT EnumerateFilters(IGraphBuilder *pGB);
   bool DShowPlayback(BSTR URL /* URL of the media */, BSTR MediaType /*Media type ie. image, audio, video*/, BSTR ContentType /*clear, drm-nd or drm-pd*/);

};

