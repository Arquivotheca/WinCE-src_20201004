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
#include <windows.h>
#include <d3dm.h>
#include "ColorConv.h"

HRESULT ReadFileToMemory(CONST TCHAR *ptszFilename, BYTE **ppByte);
HRESULT ReadFileToMemoryTimed(CONST TCHAR *ptszFilename, BYTE **ppByte);
HRESULT GetPixelFormat(D3DMFORMAT d3dFormat, PIXEL_UNPACK **ppDstFormat);
HRESULT ValidBitmapForDecode(CONST UNALIGNED BITMAPFILEHEADER *bmfh, CONST UNALIGNED BITMAPINFOHEADER *bmi);

HRESULT FillLockedRect(D3DMLOCKED_RECT *pLockedRect,
					   UINT uiBitmapHeight,
					   UINT uiBitmapWidth,
					   BYTE *pImage,
					   UINT uiSrcPixelSize,
					   D3DMFORMAT d3dFormat);

