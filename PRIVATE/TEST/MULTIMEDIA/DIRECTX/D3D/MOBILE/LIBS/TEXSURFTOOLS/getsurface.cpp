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
#include "common.h"
#include <stdio.h>
#include <tchar.h>
#include "DebugOutput.h"

//
// FillSurface
//
//   Loads a BMP (that satisfies the constraints required by ValidateBitmapHeaders) into a Direct3D surface.
//
// Arguments:  
//   
//   BYTE *pBMP:  Pointer to bitmap file (loaded into memory)
//   IDirect3DMobileSurface *pSurface:  Surface to fill
//   
// Return Value:
//  
//   HRESULT indicates success or failure 
//
HRESULT FillSurface(IDirect3DMobileSurface* pSurface, BYTE *pBMP)
{
	//
	// Pointers to various significant offsets within the BMP file
	//
	UNALIGNED BITMAPFILEHEADER *pbmfh;
	UNALIGNED BITMAPINFOHEADER *pbmih;
	BYTE *pImage;

	//
	// Result defaults to success; failure conditions are specifically set
	//
	HRESULT hr = S_OK;

	//
	// For retrieving surface description
	//
	D3DMSURFACE_DESC Desc;

	//
	// Only one pixel size is accepted as input:  24BPP
	//
	CONST UINT uiSrcPixelSize = 3;

	//
	// D3D-generated structure that specifies surface pointer and pitch
	//
	D3DMLOCKED_RECT d3dLockedRect;

	//
	// Store significant BMP offsets
	//
	pbmfh = (BITMAPFILEHEADER*)(pBMP);
	pbmih = (BITMAPINFOHEADER*)(pBMP+sizeof(BITMAPFILEHEADER));
	pImage = pBMP+pbmfh->bfOffBits;

	//
	// Verify that BMP format conforms to expectations
	//
	if (FAILED(hr = ValidBitmapForDecode(pbmfh,    // CONST UNALIGNED BITMAPFILEHEADER *bmfh
									     pbmih)))  // CONST UNALIGNED BITMAPINFOHEADER *bmi
	{
		DebugOut(DO_ERROR, L"File header does not conform to requirements. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Inspect surface description
	//
	if (FAILED(hr = pSurface->GetDesc(&Desc)))
	{
		DebugOut(DO_ERROR, L"File header does not conform to requirements. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Verify that extents are identical to BMP extents
	//
	if ((Desc.Width != (UINT)pbmih->biWidth) ||
		(Desc.Height != (UINT)pbmih->biHeight))
	{
		DebugOut(DO_ERROR, 
		    L"Extent mismatch (BMP %dx%d, surface %dx%d).",
		    pbmih->biWidth,
		    pbmih->biHeight,
		    Desc.Width,
		    Desc.Height);
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Lock the entire extent, for use as a destination for the color-converted BMP data
	//
	if (FAILED(hr = pSurface->LockRect(&d3dLockedRect, // D3DMLOCKED_RECT* pLockedRect,
                                       NULL,           // CONST RECT* pRect,
                                       0           ))) // DWORD Flags
	{
		DebugOut(DO_ERROR, L"LockRect failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Put bitmap data in
	//
	if (FAILED(hr = FillLockedRect( &d3dLockedRect,  // D3DMLOCKED_RECT *pLockedRect,
	                                pbmih->biHeight, // UINT uiBitmapHeight,
	                                pbmih->biWidth,  // UINT uiBitmapWidth,
	                                pImage,          // BYTE *pImage,
	                                uiSrcPixelSize,  // UINT uiSrcPixelSize,
	                                Desc.Format)))   // D3DMFORMAT d3dFormat
	{
		DebugOut(DO_ERROR, L"FillLockedRect failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Surface manipulation is complete.
	//
	if (FAILED(hr = pSurface->UnlockRect())) // UINT Level
	{
		DebugOut(DO_ERROR, L"UnlockRect failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

cleanup:

	return hr;
}



//
// FillSurface
//
//   Overload that automatically loads the BMP into memory, then calls the FillSurface
//   function to fill the surface with the BMP contents.
//
// Arguments:  
//   
//   IDirect3DMobileSurface *pSurface:  Surface to fill
//   CONST TCHAR *pszFile:  Full filename, including path, of BMP to load
//   
// Return Value:
//  
//   HRESULT indicates success or failure 
//
HRESULT FillSurface(IDirect3DMobileSurface* pSurface, CONST TCHAR *pszFile)
{
	//
	// Result defaults to success; failure conditions are specifically set
	//
	HRESULT hr = S_OK;

	//
	// Pointers to BMP (loaded into memory)
	//
	BYTE *pBMP = NULL;

	//
	// Surface description
	//
	D3DMSURFACE_DESC SurfaceDesc;

	//
	// Unused value required for VD argument
	//
	DWORD dwValidateDevice;

	//
	// Image surface used as intermediary in case of texture surface
	//
	IDirect3DMobileSurface* pImageSurface = NULL;

	//
	// Device's interface pointer; potentially needed for image surface creation
	//
	LPDIRECT3DMOBILEDEVICE pDevice = NULL;

	//
	// Load file into memory (allocated "automatically")
	//
	if (FAILED(hr = ReadFileToMemoryTimed(pszFile, // TCHAR *ptszFilename
								          &pBMP))) // BYTE **ppByte
	{
		DebugOut(DO_ERROR, L"ReadFileToMemoryTimed failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Retrieve description of surface
	//
	if (FAILED(hr = pSurface->GetDesc(&SurfaceDesc)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileSurface::GetDesc failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Because not all drivers allow lock on texture surfaces, a different
	// path must be followed for those, to ensure validity on all texture-
	// supporting drivers
	//
	if (D3DMRTYPE_TEXTURE == SurfaceDesc.Type)
	{
		//
		// Retrieve device's interface pointer
		//
		if (FAILED(hr = pSurface->GetDevice(&pDevice))) // IDirect3DMobileDevice** ppDevice
		{
			DebugOut(DO_ERROR, L"GetDevice failed. (hr = 0x%08x)", hr);
			goto cleanup;
		}

		//
		// Create an image surface of the same extents as the texture surface level
		//
		if (FAILED(hr = pDevice->CreateImageSurface(SurfaceDesc.Width,   // UINT Width
		                                            SurfaceDesc.Height,  // UINT Height
		                                            SurfaceDesc.Format,  // D3DMFORMAT Format
		                                            &pImageSurface )))   // IDirect3DMobileSurface** ppSurface
		{
			DebugOut(DO_ERROR, L"CreateImageSurface failed. (hr = 0x%08x)", hr);
			goto cleanup;
		}

		//
		// Fill the image surface
		//
		if (FAILED(hr = FillSurface(pImageSurface, // IDirect3DMobileSurface* pSurface
		                            pBMP)))        // BYTE *pBMP
		{
			DebugOut(DO_ERROR, L"FillSurface failed. (hr = 0x%08x)", hr);
			goto cleanup;
		}

		//
		// Copy data from image surface to to texture
		//
		if (FAILED(hr = pDevice->CopyRects(pImageSurface,   // IDirect3DMobileSurface* pSourceSurface
		                                   NULL,            // CONST RECT* pSourceRectsArray
		                                   0,               // UINT cRects
		                                   pSurface,        // IDirect3DMobileSurface* pDestinationSurface
		                                   NULL)))          // CONST POINT* pDestPointsArray
		{
			DebugOut(DO_ERROR, L"CopyRects failed. (hr = 0x%08x)", hr);
			goto cleanup;
		}

		//
		// Force a flush to ensure immediate processing of CopyRects
		//
		(VOID)pDevice->ValidateDevice(&dwValidateDevice);

	}
	else if (D3DMRTYPE_SURFACE == SurfaceDesc.Type)
	{
		if (FAILED(hr = FillSurface(pSurface, // IDirect3DMobileSurface* pSurface
							        pBMP)))   // BYTE *pBMP
		{
			DebugOut(DO_ERROR, L"FillSurface failed. (hr = 0x%08x)", hr);
			goto cleanup;
		}
	}
	else
	{
		DebugOut(DO_ERROR, L"Unknown D3DMSURFACE_DESC.Type.");
		hr = E_FAIL;
		goto cleanup;
	}



cleanup:

	if (pDevice)
		pDevice->Release();

	if (pImageSurface)
		pImageSurface->Release();

	if (pBMP)
		free(pBMP);

	return hr;
}

