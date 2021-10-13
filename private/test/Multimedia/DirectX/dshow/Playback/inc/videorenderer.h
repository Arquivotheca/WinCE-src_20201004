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

#ifndef VIDEO_RENDERER_H
#define VIDEO_RENDERER_H

#include <windows.h>
#include <vector>


enum VideoRendererModes
{
	VIDEO_RENDERER_MODE_GDI = 1,
	VIDEO_RENDERER_MODE_DDRAW = 2
};

typedef DWORD VideoRendererMode;

enum VRFlags
{
	ALLOW_PRIMARY_FLIPPING = 1,
	USE_SCAN_LINE = 2,
	USE_OVERLAY_STRETCH = 4
};

enum VRImageSize
{
	IMAGE_SIZE_NONE,
	MIN_IMAGE_SIZE,
	MAX_IMAGE_SIZE,
	FULL_SCREEN
};

struct WndPos
{
	long x;
	long y;
};

struct WndScale
{
	long x;
	long y;
};

typedef RECT WndRect;

typedef std::vector<WndPos> WndPosList;
typedef std::vector<WndScale> WndScaleList;
typedef std::vector<WndRect> WndRectList;

#endif