//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

