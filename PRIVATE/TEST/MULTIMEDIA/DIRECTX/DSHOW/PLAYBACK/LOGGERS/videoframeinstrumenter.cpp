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
////////////////////////////////////////////////////////////////////////////////
//
//  Frame number instrumenter
//
//
////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>

#include <windows.h>
#include <winuser.h>
#include <wingdi.h>
#include <stdio.h>
#include <tchar.h>
#include <amstream.h>
#include <streams.h>
#include <ddraw.h>

#include "globals.h"
#include "logging.h"
#include "Imaging.h"
#include "utility.h"
#include "FrameRecognizer.h"

VideoFrameInstrumenter::VideoFrameInstrumenter()
{
	memset(&m_mt, 0, sizeof(m_mt));

	m_BltY = 0;

	for(int i = 0; i < NBITMAPS; i++)
	{
		m_hBitmap[i] = 0;
		m_pSrcBits[i] = 0;
		memset(&m_SrcBmi[i], 0, sizeof(m_SrcBmi[i]));
	}
}

VideoFrameInstrumenter::~VideoFrameInstrumenter()
{
	// In CE we simply get a pointer to the underlying bitmaps so this would be freed on the CloseHandle below
#ifndef UNDER_CE
	for(int i = 0; i < NBITMAPS; i++)
	{
		if (m_pSrcBits[i])
			delete m_pSrcBits[i];
	}
#endif
	for(int i = 0; i < NBITMAPS; i++)
	{
		DeleteObject(m_hBitmap[i]);
	}
}

HRESULT VideoFrameInstrumenter::Init(AM_MEDIA_TYPE* pMediaType)
{
	HRESULT hr = S_OK;

	// Save the media type
	CopyMediaType(&m_mt, pMediaType);

	// Load the bitmaps
	TCHAR szBitmapFile[128];
	for(int i = 0; i < NBITMAPS; i++)
	{
#ifdef UNDER_CE
		_stprintf(szBitmapFile, TEXT("\\release\\bitmap%d.bmp"), i);
		m_hBitmap[i] = SHLoadDIBitmap(szBitmapFile);
#else
		_stprintf(szBitmapFile, TEXT("bitmap%d.bmp"), i);
		m_hBitmap[i] = (HBITMAP)LoadImage(NULL, szBitmapFile, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
#endif
		if (!m_hBitmap[i])
		{
			LOG(TEXT("Failed to load bitmap %s"), szBitmapFile);
			return E_FAIL;
		}
	}

	// Get the height and width from the first bitmap: assume it's the same for the remaining
	BITMAP bitmap;
	hr = GetBitmapInfo(m_hBitmap[0], &bitmap);
	if (FAILED(hr))
		return hr;

	// Determine if we need to invert the bitmap
	CMediaType mt(m_mt);
	BITMAPINFO* pImageBmi = GetBitmapInfo(&mt);

	if (!pImageBmi)
	{
		LOG(TEXT("Invalid BitmapInfo"));
		return E_FAIL;
	}

	BITMAPINFOHEADER* pImageBmih = &pImageBmi->bmiHeader;
	bool bInvert = false;
	if (IsRGBImage(m_mt.subtype))
	{
		// If this is a RGB image that has a +ve height, then it is upside down
		if (pImageBmih->biHeight >= 0)
		{
			m_BltY = pImageBmih->biHeight - bitmap.bmHeight;
			// BUGBUG: shouldn't this be actually false
			bInvert = true;
		}
	}
	else {
		// If height is -ve then the image is upside down
		if (pImageBmih->biHeight < 0)
		{
			// BUGBUG: is this right?
			m_BltY = 0;
			//m_BltY = -pImageBmih->biHeight - bitmap.bmHeight;
			//bInvert = true;
		}
	}

	// Allocate memory for all the bitmaps
	for(int i = 0; i < NBITMAPS; i++)
	{
		BITMAPINFOHEADER* pSrcBmih = &m_SrcBmi[i].bmiHeader;

		hr = GetBitmapInfo(m_hBitmap[i], &bitmap);
#ifndef UNDER_CE
		m_pSrcBits[i] = new BYTE[bitmap.bmWidthBytes*bitmap.bmHeight];
		GetBitmapBits(m_hBitmap[i], bitmap.bmWidthBytes*bitmap.bmHeight, m_pSrcBits[i]);
#else
		m_pSrcBits[i] = (BYTE*)bitmap.bmBits;
#endif
		if (bInvert)
			InvertBitmap(m_pSrcBits[i], bitmap.bmWidth, bitmap.bmHeight, bitmap.bmBitsPixel);
		pSrcBmih->biBitCount = bitmap.bmBitsPixel;
		pSrcBmih->biClrImportant = 0;
		pSrcBmih->biClrUsed = 0;
		pSrcBmih->biCompression = BI_RGB;
		pSrcBmih->biHeight = bitmap.bmHeight;
		pSrcBmih->biWidth = bitmap.bmWidth;
		pSrcBmih->biPlanes = bitmap.bmPlanes;
		pSrcBmih->biSize = sizeof(BITMAPINFOHEADER);
		pSrcBmih->biSizeImage = bitmap.bmWidthBytes*bitmap.bmHeight;
		pSrcBmih->biXPelsPerMeter = 0;
		pSrcBmih->biYPelsPerMeter = 0;
	}

	// Finally, check if we support the media type
	// BUGBUG: can we do this check earlier?
	if (!CanConvertImage(&m_SrcBmi[0], MEDIASUBTYPE_RGB32, pImageBmi, m_mt.subtype))
	{
		LOG(TEXT("%S: ERROR %d@%S - Cannot convert from RGB32 to destination subtype \n"), __FUNCTION__, __LINE__, __FILE__, hr);
        return E_FAIL;
	}

	return hr;
}

HRESULT VideoFrameInstrumenter::Instrument(IMediaSample* pMediaSample, DWORD framenum)
{
	HRESULT hr = S_OK;

	// Try locking if this is a DDraw surface
	bool bUnlock = false;
	hr = LockSurface(pMediaSample, true);
	if ((hr == E_NOINTERFACE) || (hr == DDERR_SURFACEBUSY))
	{
		hr = S_OK;
	}
	else if (SUCCEEDED(hr))
	{
		bUnlock = true;
	}
	else if ((hr != E_NOINTERFACE) && FAILED_F(hr))
	{
		LOG(TEXT("Unable to lock DDraw surface (0x%x)\n"), hr);
		return hr;
	}
	
	CMediaType mt(m_mt);
	BITMAPINFO* pDstBmi = GetBitmapInfo(&mt);
	BITMAPINFOHEADER* pDstBmih = &pDstBmi->bmiHeader;
	BYTE* pImage = NULL;
	pMediaSample->GetPointer(&pImage);

	// First digit
	int srcWidth = m_SrcBmi[0].bmiHeader.biWidth;

	for (int i = 0; i < NDIGITS; i++)
	{
		int digit = framenum%NBITMAPS;
		ConvertImage(m_pSrcBits[digit], &m_SrcBmi[digit], MEDIASUBTYPE_RGB32, 0, 0, m_SrcBmi[digit].bmiHeader.biWidth, m_SrcBmi[digit].bmiHeader.biHeight,
					pImage, pDstBmi, m_mt.subtype, (NDIGITS - i - 1)*srcWidth, m_BltY);
		framenum = framenum/NBITMAPS;
	}

	if (bUnlock)
	{
		hr = LockSurface(pMediaSample, false);
		if (FAILED(hr))
			LOG(TEXT("Unable to unlock DDraw surface (0x%x)\n"), hr);
	}

	return hr;
}

#if 0
		// Secret MGDI function prototype...
extern "C" {
WINGDIAPI HBITMAP WINAPI
CreateBitmapFromPointer(
    CONST BITMAPINFO *pbmi,
    int                   iStride,
    PVOID             pvBits
);
}

#ifdef UNDER_CE
			TCHAR szBitmapFile[] = TEXT("\\release\\bitmap1.bmp");
			m_hBitmap = SHLoadDIBitmap(szBitmap);
		#else
			TCHAR szBitmapFile[] = TEXT("bitmap1.bmp");
			m_hBitmap = LoadImage(NULL, szBitmap, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
		#endif

		// Create a memory DC compatible with the current screen
		HDC hdcVideo = CreateCompatibleDC(NULL);
		HBITMAP hVideoBitmap = 0;
		HDC hdcOverlay = CreateCompatibleDC(NULL);
		HDGDIOBJ oldOverlay = SelectObject(hdcOverlay, m_hBitmap);

		int iStride = ((pBmih->biWidth * pBmih->biBitCount / 8) + 3) & ~3;

		// Create the bitmap object
#ifndef UNDER_CE
		hVideoBitmap = CreateDIBitmap(hdcVideo, pBmih, CBM_INIT, pImage, (BITMAPINFO*)pBmih, DIB_RGB_COLORS);
#else
		hVideoBitmap = CreateBitmapFromPointer((BITMAPINFO*)pBmih, iStride, pImage);
#endif
		// Select the video bitmap into the DC
		HGDIOBJ oldVideo = SelectObject(hdcVideo, hBitmap);

		// Blit into the dst DC
		BitBlt(hdcVideo, 0, 0, pBmih->biWidth, pBmih->biHeight, hdcOverlay, 0, 0, SRCCOPY);

		// Select the older object back into the DC
		SelectObject(hdc, objOld);

		// Delete the bitmap object created
		DeleteObject(hBitmap);
		DeleteDC(hdc);
#if 0
	case 24:
		for(int x = 0; x < width; x++)
		for(int y = 0; y < height; y++)
		{
			// Re-arrange into r, g and b instead of b, g and r
			BYTE* ptr = pBits + y*width*3 + x*3;
			BYTE temp = *(ptr + 2);
			*(ptr + 2) = *ptr;
			*ptr = temp;
		}
#endif

#endif