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
//  Frame number recognizer
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
#include "Validtype.h"
#include "utility.h"
#include "FrameRecognizer.h"

extern HRESULT LockSurface(IMediaSample *pSample, bool bLock);
extern void InvertBitmap(BYTE* pBits, int width, int height, DWORD bitcount);

#define PRE_CONVERT 1
#define SAVE_BITMAP 0
#define LOG_VERBOSE 0

#define SWAPBYTE(x,y) { BYTE temp = x; x = y; y = temp; }


VideoFrameRecognizer::VideoFrameRecognizer()
{
	memset(&m_mt, 0, sizeof(m_mt));

	m_BitmapSubType = GUID_NULL;

	m_SrcY = 0;

	for(int i = 0; i < NBITMAPS; i++)
	{
		m_hBitmap[i] = 0;
		m_pSrcBits[i] = 0;
		m_pConvBits[i] = 0;
		m_pConvBmi[i] = 0;
		memset(&m_SrcBmi[i], 0, sizeof(m_SrcBmi[i]));
	}
}

VideoFrameRecognizer::~VideoFrameRecognizer()
{
	Reset();
}

void VideoFrameRecognizer::Reset()
{
	FreeMediaType(m_mt);
	memset(&m_mt, 0, sizeof(m_mt));
	
	// In CE we simply get a pointer to the underlying bitmaps so this would be freed on the CloseHandle below
	for(int i = 0; i < NBITMAPS; i++)
	{
#ifndef UNDER_CE
		if (m_pSrcBits[i])
		{
			delete m_pSrcBits[i];
			m_pSrcBits[i] = NULL;
		}
#endif
		if (m_pConvBits[i])
		{
			delete m_pConvBits[i];
			m_pConvBits[i] = NULL;
		}

		if (m_pConvBmi[i])
		{
			delete m_pConvBmi[i];
			m_pConvBmi[i] = 0;
		}

		if (m_hBitmap[i])
			CloseHandle(m_hBitmap[i]);
		
		memset(&m_SrcBmi[i], 0, sizeof(m_SrcBmi[i]));
	}
}

HRESULT VideoFrameRecognizer::Init(AM_MEDIA_TYPE* pMediaType)
{
	HRESULT hr = S_OK;
	
	if (!pMediaType)
		return E_INVALIDARG;

	// Reset our members
	Reset();

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
			LOG(TEXT("%S: ERROR %d@%S - Failed to load bitmap %s"), __FUNCTION__, __LINE__, __FILE__, szBitmapFile);
			return E_FAIL;
		}
	}

	// Get the height and width from the first bitmap: assume it's the same for the remaining
	BITMAP bitmap;
	if (SUCCEEDED(hr))
	{
		hr = GetBitmapInfo(m_hBitmap[0], &bitmap);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to get bitmap info for the digit bitmap"), __FUNCTION__, __LINE__, __FILE__);
			return hr;
		}
	}

	// Get the bitmap info for the current media type
	CMediaType mt(m_mt);
	BITMAPINFO* pImageBmi = NULL;
	if (SUCCEEDED(hr))
	{
		pImageBmi = GetBitmapInfo(&mt);
		if (!pImageBmi)
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to get bitmap info from media type"), __FUNCTION__, __LINE__, __FILE__);
			return E_FAIL;
		}
	}

	// Determine if we need to invert the bitmap
	BITMAPINFOHEADER* pImageBmih = NULL;
	pImageBmih = &pImageBmi->bmiHeader;
	bool bInvert = false;
	if (SUCCEEDED(hr))
	{
		if (IsRGBImage(m_mt.subtype))
		{
			// If this is a RGB image that has a +ve height, then it is upside down
			if (pImageBmih->biHeight >= 0)
			{
				// The origin is at the bottom left of the 2 dimensional array of bytes in memory
				m_SrcY = pImageBmih->biHeight - bitmap.bmHeight;

#ifndef UNDER_CE
				// BUGBUG: why is this true on the desktop and not on CE?
				bInvert = true;
#endif
			}
		}
		else {
			// If height is -ve then the image is upside down
			if (pImageBmih->biHeight < 0)
			{
				// BUGUBG: is this right?
				//m_SrcY = -pImageBmih->biHeight - bitmap.bmHeight;
				m_SrcY = 0;
				//bInvert = true;
			}
		}
	}

	// Allocate memory for all the bitmaps
	if (SUCCEEDED(hr))
	{
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

	#if 0
			//BUGBUG: why are the bytes swapped on CE?
			SwapRedGreen(m_pSrcBits[i], bitmap.bmWidth, bitmap.bmHeight, bitmap.bmBitsPixel);
	#endif

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
	}

	// BUGBUG: hardcoding subtype
	// BUGBUG: the bitmaps appear differently on CE and desktop
	if (SUCCEEDED(hr))
	{
		if (bitmap.bmBitsPixel == 24)
			m_BitmapSubType = MEDIASUBTYPE_RGB24;
		else if(bitmap.bmBitsPixel == 32)
			m_BitmapSubType = MEDIASUBTYPE_RGB32;
		else
		{
			LOG(TEXT("ERROR: Input bitmap not RGB 24 or RGB 32. \n"));
			hr = E_FAIL;
		}
	}

	// Get the color table size of the video images
	VideoTypeDesc* pVideoTypeDesc = GetVideoTypeDesc(pImageBmih->biCompression, pImageBmih->biBitCount);
	DWORD colorTableSize = pVideoTypeDesc->colorTableSize;

	// Now that we have loaded the bitmap convert it into the format used by actual video passing through
	if (SUCCEEDED(hr))
	{
		for(int i = 0; i < NBITMAPS; i++)
		{
			BITMAPINFO* pConvBmi = (BITMAPINFO*) new BYTE[sizeof(BITMAPINFOHEADER) + colorTableSize];
			if (!pConvBmi)
			{
				hr = E_OUTOFMEMORY;
				break;
			}
			BITMAPINFOHEADER* pConvBmih = (BITMAPINFOHEADER*)pConvBmi;
			pConvBmih->biBitCount = pImageBmih->biBitCount;
			pConvBmih->biClrImportant = pImageBmih->biClrImportant;
			pConvBmih->biClrUsed = pImageBmih->biClrUsed;
			pConvBmih->biCompression = pImageBmih->biCompression;
			pConvBmih->biHeight = bitmap.bmHeight;
			pConvBmih->biWidth = bitmap.bmWidth;
			pConvBmih->biPlanes = pImageBmih->biPlanes;
			pConvBmih->biSize = sizeof(BITMAPINFOHEADER);
			// Calculate size of the converted bitmap image and allocate memory for the converted image
			pConvBmih->biSizeImage = AllocateBitmapBuffer(bitmap.bmWidth, bitmap.bmHeight, pImageBmih->biBitCount, pImageBmih->biCompression, &m_pConvBits[i]);
			if (!m_pConvBits[i])
			{
				hr = E_OUTOFMEMORY;
				break;
			}
			pConvBmih->biXPelsPerMeter = pImageBmih->biXPelsPerMeter;
			pConvBmih->biYPelsPerMeter = pImageBmih->biYPelsPerMeter;
			// Copy the color table if any
			if (colorTableSize)
				memcpy(&pConvBmi->bmiColors[0],&pImageBmi->bmiColors[0],colorTableSize);

			// Convert the bits from the bitmap native bits to the type used by the video
			hr = ConvertImage(m_pSrcBits[i], &m_SrcBmi[i], m_BitmapSubType, 0, 0, m_SrcBmi[i].bmiHeader.biWidth, m_SrcBmi[i].bmiHeader.biHeight,
		  					m_pConvBits[i], pConvBmi, m_mt.subtype, 0, 0);
			if (FAILED(hr))
			{
				LOG(TEXT("%S: ERROR %d@%S - failed to convert bitmap to video type (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
				break;
			}

			// Otherwise store the bitmapinfo object created
			m_pConvBmi[i] = pConvBmi;

#if SAVE_BITMAP
			// Save the input sample 
			TCHAR szfile[64] = TEXT("");
#ifdef UNDER_CE
			_stprintf(szfile, TEXT("\\release\\in%d.bmp"), i);
#else
			_stprintf(szfile, TEXT("in%d.bmp"), i);
#endif

			// BUGBUG: source bitmaps should have been 32 or 24 and don't need a color table
			SaveBitmap(szfile, (BITMAPINFOHEADER*)&m_SrcBmi[i], m_pSrcBits[i]);
			
			// Save the output sample after converting back to RGB32
			ConvertImage(m_pConvBits[i], &m_pConvBmi[i]->bmiHeader, m_mt.subtype, 0, 0, m_pConvBmi[i]->bmiHeader.biWidth, m_pConvBmi[i]->bmiHeader.biHeight,
						m_pSrcBits[i], &m_SrcBmi[i].bmiHeader, m_BitmapSubType);

#ifdef UNDER_CE
			_stprintf(szfile, TEXT("\\release\\out%d.bmp"), i);
#else
			_stprintf(szfile, TEXT("out%d.bmp"), i);
#endif
			// BUGBUG: source bitmaps should have been 32 or 24 and don't need a color table
			SaveBitmap(szfile, (BITMAPINFOHEADER*)&m_SrcBmi[i], m_pSrcBits[i]);
#endif
		}
	}

	return hr;
}

BOOL RecursiveCreateDirectory(TCHAR * tszPath)
{
    if (!CreateDirectory(tszPath, NULL) && 
        (GetLastError() == ERROR_PATH_NOT_FOUND || GetLastError() == ERROR_FILE_NOT_FOUND))
    {
        TCHAR * pBack;
        pBack = wcsrchr(tszPath, TEXT('\\'));
        if (pBack)
        {
            *pBack = 0;
            RecursiveCreateDirectory(tszPath);
            *pBack = TEXT('\\');
        }
        else
            return false;
    }
    if (CreateDirectory(tszPath, NULL) 
        || (GetLastError() == ERROR_FILE_EXISTS
            || GetLastError() == ERROR_ALREADY_EXISTS)
        )
    {
        return true;
    }
    return false;
}


HRESULT SaveRGBBitmap(const TCHAR * tszFileName, BYTE *pPixels, INT iWidth, INT iHeight, INT iWidthBytes)
{
    DWORD dwBmpSize;
    BITMAPFILEHEADER bmfh;
    DWORD dwBytesWritten;
    BITMAPINFO bmi24;
    HANDLE hBmpFile = NULL;
    HRESULT hr;
    TCHAR tszBitmapCopy[MAX_PATH + 1];
    TCHAR * pBack;

    //
    // Argument validation
    //
    if (NULL == tszFileName)
    {
        return E_POINTER;
    }

    if (NULL == pPixels)
    {
        return E_POINTER;
    }

    if (0 >= iWidth)
    {
        return E_INVALIDARG;
    }

    if (0 == iHeight) 
    {
        return E_INVALIDARG;
    }

    if (0 >= iWidthBytes) 
    {
        return E_INVALIDARG;
    }

    // if iHeight is negative, then it's a top down dib.  Save it as it is.
    
    memset(&bmi24, 0x00, sizeof(bmi24));
    
    bmi24.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    bmi24.bmiHeader.biWidth = iWidth;
    bmi24.bmiHeader.biHeight = iHeight;
    bmi24.bmiHeader.biPlanes = 1;
    bmi24.bmiHeader.biBitCount = 24;
    bmi24.bmiHeader.biCompression = BI_RGB;
    bmi24.bmiHeader.biSizeImage = bmi24.bmiHeader.biXPelsPerMeter = bmi24.bmiHeader.biYPelsPerMeter = 0;
    bmi24.bmiHeader.biClrUsed = bmi24.bmiHeader.biClrImportant = 0;
    
    memset(&bmfh, 0x00, sizeof(BITMAPFILEHEADER));
    bmfh.bfType = 'M' << 8 | 'B';
    bmfh.bfOffBits = sizeof(bmfh) + sizeof(bmi24);

    //created the directory to save the file if it doesn't exist
    wcsncpy(tszBitmapCopy, tszFileName, countof(tszBitmapCopy));
    tszBitmapCopy[countof(tszBitmapCopy) - 1] = 0;
    pBack = wcsrchr(tszBitmapCopy, TEXT('\\'));
    if (pBack)
    {
        *pBack = 0;
        RecursiveCreateDirectory(tszBitmapCopy);
        *pBack = TEXT('\\');
    }

    hBmpFile = CreateFile(
        tszFileName, 
        GENERIC_WRITE, 
        0, 
        NULL, 
        CREATE_ALWAYS, 
        FILE_ATTRIBUTE_NORMAL, 
        NULL);
    if (INVALID_HANDLE_VALUE == hBmpFile)
        goto LPError;
    
    bmi24.bmiHeader.biSizeImage = dwBmpSize = iWidthBytes * abs(iHeight);
    bmfh.bfSize = bmfh.bfOffBits + dwBmpSize;
    
    if (!WriteFile(
            hBmpFile,
            (void*)&bmfh,
            sizeof(bmfh),
            &dwBytesWritten,
            NULL))
    {
        goto LPError;
    }
    if (!WriteFile(
            hBmpFile,
            (void*)&bmi24,
            sizeof(bmi24),
            &dwBytesWritten,
            NULL))
    {
        goto LPError;
    }
    if (!WriteFile(
            hBmpFile,
            (void*)pPixels,
            dwBmpSize,
            &dwBytesWritten,
            NULL))
    {
        goto LPError;
    }
    
    CloseHandle(hBmpFile);
    return S_OK;
LPError:
    hr = HRESULT_FROM_WIN32(GetLastError());

    if (hBmpFile)
        CloseHandle(hBmpFile);
    return hr;
}


HRESULT VideoFrameRecognizer::Recognize(IMediaSample* pMediaSample, DWORD* pFrameNum)
{
	HRESULT hr = S_OK;
	
	if (!pMediaSample)
		return E_INVALIDARG;

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
	
	// Get a pointer to the image and its info
	CMediaType mt(m_mt);
	BITMAPINFO* pImageBmi = GetBitmapInfo(&mt);

	if(!pImageBmi)
	{
		LOG(TEXT("ERROR: Invalid bitmapinfo pointer.\n"));
		return E_FAIL;
	}

	BITMAPINFOHEADER* pImageBmih = &pImageBmi->bmiHeader;

	if(!pImageBmih)
	{
		LOG(TEXT("ERROR: Invalid bitmapinfoheader pointer.\n"));
		return E_FAIL;
	}

	BYTE* pImage = NULL;
	pMediaSample->GetPointer(&pImage);

	if (!pImage)
	{
		LOG(TEXT("ERROR: unable to obtain image pointer.\n"));
		if (bUnlock)
			LockSurface(pMediaSample, false);
		return E_FAIL;
	}

#if SAVE_BITMAP
	static int nSamples = 0;
	
	if (1 /*0 == (nSamples%20)*/)
	{
		// Save the video frame as a bitmap
		TCHAR szfile[64];
		#ifdef UNDER_CE
		_sntprintf(szfile, countof(szfile), TEXT("\\release\\vout%d.bmp"), nSamples);
		#else
		_sntprintf(szfile, countof(szfile), TEXT("vout%d.bmp"), nSamples);
		#endif
		VideoTypeDesc* pVideoTypeDesc = GetVideoTypeDesc(pImageBmi->bmiHeader.biCompression, pImageBmi->bmiHeader.biBitCount);
		SaveBitmap(szfile, (BITMAPINFOHEADER*)pImageBmi, pImage, pVideoTypeDesc->colorTableSize);
	}
	nSamples++;
#endif

	BITMAPINFOHEADER* pSrcBmih = &m_SrcBmi[0].bmiHeader;
	int digitWidth = pSrcBmih->biWidth;
	int digitHeight = pSrcBmih->biHeight;
	BYTE* pDigitBits = 0;
#if PRE_CONVERT
	int alloc = AllocateBitmapBuffer(digitWidth, digitHeight, pImageBmih->biBitCount, pImageBmih->biCompression, &pDigitBits);
#else
	int alloc = AllocateBitmapBuffer(digitWidth, digitHeight, pSrcBmih->biBitCount, pSrcBmih->biCompression, &pDigitBits);
#endif

	// Set the height as positive
	if (pImageBmih->biHeight < 0)
	{
		pImageBmih->biHeight = -pImageBmih->biHeight;
		LOG(TEXT("Negative height encountered. \n"));
	}

#define SAVERGB_BITMAP

#ifdef SAVERGB_BITMAP
    BITMAPINFO bmi;
    BYTE *pRGBBits = NULL;

    memset(&bmi, 0, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = pImageBmih->biWidth;
    bmi.bmiHeader.biHeight = pImageBmih->biHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biXPelsPerMeter = bmi.bmiHeader.biYPelsPerMeter = 0;
    bmi.bmiHeader.biClrUsed = bmi.bmiHeader.biClrImportant = 0;
    bmi.bmiHeader.biSizeImage = bmi.bmiHeader.biWidth * abs(bmi.bmiHeader.biHeight);

    int alloc2 = AllocateBitmapBuffer(bmi.bmiHeader.biWidth, bmi.bmiHeader.biHeight, bmi.bmiHeader.biBitCount, bmi.bmiHeader.biCompression, &pRGBBits);
    hr = ConvertImage(pImage, pImageBmi, m_mt.subtype, 0, 0, bmi.bmiHeader.biWidth, bmi.bmiHeader.biHeight, 
                                    pRGBBits, &bmi, MEDIASUBTYPE_RGB24, 0, 0);

    static int id = 0;
    TCHAR szDigitFile[64];
    _stprintf(szDigitFile, TEXT("\\release\\digit%d.bmp"), id);

    SaveRGBBitmap(szDigitFile, pRGBBits, bmi.bmiHeader.biWidth, bmi.bmiHeader.biHeight, bmi.bmiHeader.biWidth * 3);
    id++;

    delete[] pRGBBits;
#endif

	// Go through each digit and recognize it and concat to the frame no
	int framenum = 0;
	TCHAR szFrameNo[8] = TEXT("");
	for (int i = 0; i < NDIGITS; i++)
	{
#if LOG_VERBOSE
		LOG(TEXT("Digit %d diff: "), i);
#endif

#if PRE_CONVERT
		// Extract and convert from the image to the digit bitmap
		hr = ConvertImage(pImage, pImageBmi, m_mt.subtype, i*digitWidth, m_SrcY, digitWidth, digitHeight,
						  pDigitBits, m_pConvBmi[0], m_mt.subtype, 0, 0);
		if (FAILED_F(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to extract bitmap from image (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			break;
		}

#if SAVE_BITMAP
		if (1)
		{
			TCHAR szDigitFile[64];
			_stprintf(szDigitFile, TEXT("\\release\\digit%d.bmp"), i);
			VideoTypeDesc* pVideoTypeDesc = GetVideoTypeDesc(m_pConvBmi[0]->bmiHeader.biCompression, m_pConvBmi[0]->bmiHeader.biBitCount);
			SaveBitmap(szDigitFile, &m_pConvBmi[0], pDigitBits, pVideoTypeDesc->colorTableSize);
		}
#endif

#else
		// Extract and convert from the image to the digit bitmap
		hr = ConvertImage(pImage, pImageBmi, m_mt.subtype, i*digitWidth, m_SrcY, digitWidth, digitHeight,
					pDigitBits, &m_SrcBmi[0], m_BitmapSubType);
		if (FAILED_F(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to convert image to bitmap type (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			break;
		}
#if SAVE_BITMAP
		if (1)
		{
			TCHAR szDigitFile[64];
			_stprintf(szDigitFile, TEXT("\\release\\digit%d.bmp"), i);
			VideoTypeDesc* pVideoTypeDesc = GetVideoTypeDesc(pSrcBmih->biCompression, pSrcBmih->biBitCount);
			SaveBitmap(szDigitFile, &m_SrcBmi[0], pDigitBits, pVideoTypeDesc->colorTableSize);
		}
#endif

#endif
		int match = -1;
		float diff = (float)INFINITE;
		float mindiff = (float)INFINITE;
		float nextmin = (float)INFINITE;
		TCHAR szDigit[2];
		for(int bitmap = 0; bitmap < NBITMAPS; bitmap++)
		{
#if PRE_CONVERT
			// Diff the bitmap and image bits
			diff = DiffImage(pDigitBits, m_pConvBits[bitmap], m_pConvBmi[bitmap], m_mt.subtype);
#else
			// Diff the bitmap image bits
			diff = DiffImage(pDigitBits, m_pSrcBits[bitmap], &m_SrcBmi[bitmap], m_BitmapSubType);
#endif

			if (diff == -1)
			{
				LOG(TEXT("ERROR: Failed to find a differ for the given image.\n"));
				hr = E_FAIL;
				break;
			}
#if LOG_VERBOSE
			LOG(TEXT("%f, "), diff);
#endif
			if (diff < mindiff)
			{
				mindiff = diff;
				match = bitmap;
			}
			// BUGBUG: this is wrong
			else if (diff < nextmin)
			{
				nextmin = diff;
			}
		}

		if (FAILED(hr))
			break;

#if LOG_VERBOSE
		LOG(TEXT("digit: %d, mindiff: %f, nextmin: %f\n"), match, mindiff, nextmin);
#endif

		if (match != -1)
		{
			_stprintf(szDigit, TEXT("%d"), match);
			_tcscat(szFrameNo, szDigit);
			framenum = match + NBITMAPS*framenum;
		}
		else {
			LOG(TEXT("ERROR: failed to find match for digit %d. \n"), i);
		}
	}

	if (pDigitBits)
	{
		delete pDigitBits;
		pDigitBits = NULL;
	}

	if (bUnlock)
	{
		hr = LockSurface(pMediaSample, false);
		if (FAILED(hr))
			LOG(TEXT("Unable to unlock DDraw surface (0x%x)\n"), hr);
	}

	if (pFrameNum && SUCCEEDED_F(hr))
		*pFrameNum = framenum;
	
	return hr;
}
