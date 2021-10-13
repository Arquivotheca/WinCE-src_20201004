//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "common.h"
#include <stdio.h>
#include <tchar.h>

static void Debug(LPCTSTR szFormat, ...);

//
// Maximum allowable length for OutputDebugString text
//
#define MAX_DEBUG_OUT 256

#define countof(x)  (sizeof(x)/sizeof(*(x)))

//
// ReadFileToMemory
//
//   Given a filename, allocate adequate memory to store the contents of the file,
//   and pass back a pointer to this memory (filled with entire file contents).
//
// Arguments:  
//   
//   CONST TCHAR *ptszFilename:  Filename for input BMP
//   BYTE **ppByte:  Pointer to bitmap bits pointer
//   
// Return Value:
//  
//   HRESULT indicates success or failure 
//
HRESULT ReadFileToMemory(CONST TCHAR *ptszFilename, BYTE **ppByte)
{
	HRESULT hr = S_OK;
	BYTE *pFileBits = NULL;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	DWORD dwFileSize;
	DWORD dwBytesRead = 0;

	*ppByte = NULL;

	//
	// Open the file for read access
	//
	hFile = CreateFile(ptszFilename,   // LPCTSTR lpFileName, 
					   GENERIC_READ,   // DWORD dwDesiredAccess, 
					   FILE_SHARE_READ,// DWORD dwShareMode, 
					   NULL,           // LPSECURITY_ATTRIBUTES lpSecurityAttributes, 
					   OPEN_EXISTING,  // DWORD dwCreationDispostion, 
					   0,              // DWORD dwFlagsAndAttributes, 
					   0);             // HANDLE hTemplateFile
	if (INVALID_HANDLE_VALUE == hFile)
	{
		OutputDebugString(_T("CreateFile failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Determine file size (required to determine buffer size allocation)
	//
	dwFileSize = GetFileSize(hFile, // HANDLE hFile, 
							 NULL); // LPDWORD lpFileSizeHigh
	if (0 == dwFileSize)
	{
		OutputDebugString(_T("GetFileSize failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Allocate a buffer in which to store the file contents
	//
	pFileBits = (BYTE*)malloc(dwFileSize);
	if (NULL == pFileBits)
	{
		OutputDebugString(_T("malloc failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Read the entire file contents into the allocated buffer
	//
	if (0 == ReadFile( 
	   hFile,        // HANDLE hFile, 
	   pFileBits,    // LPVOID lpBuffer, 
	   dwFileSize,   // DWORD nNumberOfBytesToRead, 
	   &dwBytesRead, // LPDWORD lpNumberOfBytesRead, 
	   NULL))        // LPOVERLAPPED lpOverlapped
	{
		OutputDebugString(_T("ReadFile failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// If the number of bytes read is not the entire file, fail.
	//
	if (dwBytesRead != dwFileSize)
	{
		OutputDebugString(_T("ReadFile did not read entire file."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// If every operation has completed successfully, set the output pointer
	// to the allocated buffer
	//
	*ppByte = pFileBits;

cleanup:

	if (hFile != INVALID_HANDLE_VALUE)
		CloseHandle(hFile);

	if (FAILED(hr))
		free(pFileBits);

	return hr;
}

//
// ReadFileToMemoryTimed
//
//   Given a filename, allocate adequate memory to store the contents of the file,
//   and pass back a pointer to this memory (filled with entire file contents).
//
//   Times the operation; and outputs debug information indicating elapsed time.
//
// Arguments:  
//   
//   CONST TCHAR *ptszFilename:  Filename for input BMP
//   BYTE **ppByte:  Pointer to bitmap bits pointer
//   
// Return Value:
//  
//   HRESULT indicates success or failure 
//
HRESULT ReadFileToMemoryTimed(CONST TCHAR *ptszFilename, BYTE **ppByte)
{
	//
	// Result defaults to success; failure conditions are specifically set
	//
	HRESULT hr = S_OK;

	//
	// Profiling timers, for informational debug output
	//
	DWORD dwProfileStart, dwProfileEnd;

	//
	// Pointers to BMP (loaded into memory)
	//
	BYTE *pMemFile;

	//
	// Capture profiling data, for informational debug output
	//
	dwProfileStart = GetTickCount();

	//
	// Load file into memory (allocated "automatically")
	//
	if (FAILED(ReadFileToMemory(ptszFilename, // TCHAR *ptszFilename
								&pMemFile)))  // BYTE **ppByte
	{
		OutputDebugString(_T("Unable read file."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Capture profiling data, for informational debug output
	//
	dwProfileEnd = GetTickCount();

	//
	// Log profiling data for most recent operation
	//
	Debug(_T("Elapsed time; BMP load from file:  %lu ms."), dwProfileEnd - dwProfileStart);

	*ppByte = pMemFile;

cleanup:

	return hr;

}

//
// GetPixelFormat
//
//   Determines the (bit level) specifications for a particular Direct3D Format.
//
// Arguments:  
//   
//   D3DMFORMAT d3dFormat:  Format to look up
//   PIXEL_UNPACK **ppDstFormat:  Output for specification of pixel format
//   
// Return Value:
//  
//   HRESULT indicates success or failure 
//
HRESULT GetPixelFormat(D3DMFORMAT d3dFormat, PIXEL_UNPACK **ppDstFormat)
{
	//
	// Bad parameter check
	//
	if (NULL == ppDstFormat)
		return E_FAIL;

	//
	// Pick pixel format corresponding to this Direct3D Format
	//
	switch(d3dFormat)
	{
		case D3DMFMT_R8G8B8:  
			(*ppDstFormat) = &UnpackFormat[1];
			break;
		case D3DMFMT_A8R8G8B8:
			(*ppDstFormat) = &UnpackFormat[2];
			break;
		case D3DMFMT_X8R8G8B8:
			(*ppDstFormat) = &UnpackFormat[3];
			break;
		case D3DMFMT_R5G6B5:  
			(*ppDstFormat) = &UnpackFormat[4];
			break;
		case D3DMFMT_X1R5G5B5:
			(*ppDstFormat) = &UnpackFormat[5];
			break;
		case D3DMFMT_A1R5G5B5:
			(*ppDstFormat) = &UnpackFormat[6];
			break;
		case D3DMFMT_A4R4G4B4:
			(*ppDstFormat) = &UnpackFormat[7];
			break;
		case D3DMFMT_R3G3B2:  
			(*ppDstFormat) = &UnpackFormat[8];
			break;
		case D3DMFMT_A8R3G3B2:
			(*ppDstFormat) = &UnpackFormat[9];
			break;
		case D3DMFMT_X4R4G4B4:
			(*ppDstFormat) = &UnpackFormat[10];
			break;
		default:
			OutputDebugString(_T("Unknown format"));
			return E_FAIL;
	}

	return S_OK;
}



HRESULT FillLockedRect(D3DMLOCKED_RECT *pLockedRect,
					   UINT uiBitmapHeight,
					   UINT uiBitmapWidth,
					   BYTE *pImage,
					   UINT uiSrcPixelSize,
					   D3DMFORMAT d3dFormat)
{
	//
	// Pointers to image data; used during per-pixel and per-line steppings
	// for color conversion
	//
	BYTE *pSrcLine, *pDstLine;
	BYTE *pSrcPixel, *pDstPixel;

	//
	// Profiling timers, for informational debug output
	//
	DWORD dwProfileStart, dwProfileEnd;

	//
	// Iterators for walking through src/dst surfaces on a pixel-by-pixel granularity
	//
	UINT uiX, uiY;

	//
	// Bit-level description of format
	//
	PIXEL_UNPACK *pDstFormat;

	//
	// Determine bit characteristics based on Direct3D format
	//
	if (FAILED(GetPixelFormat( d3dFormat,     // D3DMFORMAT d3dFormat
	                           &pDstFormat))) // PIXEL_UNPACK **ppDstFormat
	{
		OutputDebugString(_T("GetPixelFormat failed."));
		return E_FAIL;
	}

	//
	// Required:  bottom-up DIB.  Account for this requirement here, by adjusting the starting
	// source pointer.
	//
	pSrcLine = pImage + (uiBitmapHeight - 1) * uiSrcPixelSize * uiBitmapWidth; 

	//
	// Destination top-left is first byte of locked bits
	//
	pDstLine = (BYTE*)(pLockedRect->pBits);

	//
	// Capture profiling data, for informational debug output
	//
	dwProfileStart = GetTickCount();

	//
	// Loop through image data, performing a color conversion for each pixel, and copying into the
	// destination buffer
	//
	for(uiY = 0; uiY < uiBitmapHeight; uiY++)
	{
		
		//
		// Per-pixel color conversion begins with the first pixel of the row
		//
		pSrcPixel = pSrcLine;
		pDstPixel = pDstLine;

		for(uiX = 0; uiX < uiBitmapWidth; uiX++)
		{

			//
			// Color conversion from 24BPP BMP (R8G8B8) to arbitrary destination format
			//
			ConvertPixel(&UnpackFormat[1], pDstFormat, pSrcPixel, pDstPixel);

			//
			// Increment to next pixel in row
			//
			pSrcPixel += uiSrcPixelSize;
			pDstPixel += pDstFormat->uiTotalBytes;
		}

		//
		// Decrement source pointer by one line (bottom-up DIB orientation)
		//
		pSrcLine -= uiBitmapWidth * uiSrcPixelSize;

		//
		// Increment destination pointer by one line (D3D textures are implicitly top-down in orientation)
		//
		pDstLine += pLockedRect->Pitch;
	}

	//
	// Capture profiling data, for informational debug output
	//
	dwProfileEnd = GetTickCount();

	//
	// Log profiling data for most recent operation
	//
	Debug(_T("Elapsed time; BMP data color conversion:  %lu ms."), dwProfileEnd - dwProfileStart);

	return S_OK;
}

//
// Debug
//
//   Printf-like wrapping around OutputDebugString.
//
// Arguments:  
//   
//   szFormat        Formatting string (as in printf).
//   ...             Additional arguments.
//   
// Return Value:
//  
//   void
//
void Debug(LPCTSTR szFormat, ...)
{
    static  TCHAR   szHeader[] = TEXT("D3DQA: ");
    TCHAR   szBuffer[MAX_DEBUG_OUT];

    va_list pArgs;
    va_start(pArgs, szFormat);
    _tcscpy(szBuffer, szHeader);

    _vsntprintf(
        &szBuffer[countof(szHeader)-1],
		MAX_DEBUG_OUT - countof(szHeader),
        szFormat,
        pArgs);

	szBuffer[MAX_DEBUG_OUT-1] = '\0'; // Guarantee NULL termination, in worst case

	OutputDebugString(szBuffer);

    va_end(pArgs);

}

//
// ValidBitmapForDecode
//    
//   Examines the following bitmap-related structures:
//
//     * BITMAPFILEHEADER
//     * BITMAPINFOHEADER
//   
//   If any of the members of these structures indicate a type of bitmap that is
//   not supported by this utility, failure is indicated in the return value for
//   this function.  This utility is only intended to handle a common format of BMP
//   that is simple to parse and also widely supported.  Restrictions:
//     
//       * Must be 24 BPP
//       * Must be a bottom-up DIB
//       * Must be 1 plane only
//       * Must not be compressed (i.e., BI_RGB required)
//
// Arguments:
//
//   BITMAPFILEHEADER: File header for BMP
//   BITMAPINFOHEADER: Bitmap info header for BMP
//    
// Return Value:
//    
//   HRESULT:  Indicates success or failure
//
HRESULT ValidBitmapForDecode(CONST UNALIGNED BITMAPFILEHEADER *bmfh, CONST UNALIGNED BITMAPINFOHEADER *bmi)
{
	//
	// Argument validation
	//
	if ((NULL == bmfh) ||
		(NULL == bmi))
	{
		OutputDebugString(_T("Invalid pointer"));
		return E_FAIL;
	}

	//
	// Structure validation, for BITMAPFILEHEADER.  No validation is performed
	// for the following fields:
	//
	//     * bfReserved1
	//     * bfReserved2
	//     * bfSize
	//
	if (bmfh->bfType != 0x4D42)
	{
		OutputDebugString(_T("Invalid bitmap signature"));
		return E_FAIL;
	}
	if (bmfh->bfOffBits != (sizeof(BITMAPINFOHEADER)+sizeof(BITMAPFILEHEADER)))
	{
		OutputDebugString(_T("Unknown bitmap header structure size"));
		return E_FAIL;
	}

	//
	// Structure validation, for BITMAPINFO.  No validation is needed
	// for the following fields:
	//
	//     * biWidth
	//     * biHeight
	//     * biSizeImage
	//     * biXPelsPerMeter
	//     * biYPelsPerMeter
	//     * biClrUsed
	//     * biClrImportant
	//
	if (sizeof(BITMAPINFOHEADER) != bmi->biSize)
	{
		OutputDebugString(_T("Unknown bitmap header structure size"));
		return E_FAIL;
	}


	//
	// If the height of the bitmap is positive, the bitmap is a bottom-up DIB and its origin is the lower-left corner.
	// If the height is negative, the bitmap is a top-down DIB and its origin is the upper left corner. 
	//
	// Only allow the common case, the bottom-up DIB.
	//
	if (bmi->biHeight < 0)
	{
		OutputDebugString(_T("Top-down DIBs are not supported."));
		return E_FAIL;
	}

	//
	// Enforce "1 plane" restriction
	//
	if (1 != bmi->biPlanes)
	{
		OutputDebugString(_T("Unsupported plane count."));
		return E_FAIL;
	}

	//
	// Enforce "24 BPP" restriction
	//
	if (24 != bmi->biBitCount)
	{
		OutputDebugString(_T("Unsupported bit count."));
		return E_FAIL;
	}

	//
	// Enforce "uncompressed" restriction
	//
	if (BI_RGB != bmi->biCompression)
	{
		OutputDebugString(_T("Unsupported compression type."));
		return E_FAIL;
	}

	return S_OK;
}
