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
	if (FAILED(ValidBitmapForDecode(pbmfh,    // CONST UNALIGNED BITMAPFILEHEADER *bmfh
									pbmih)))  // CONST UNALIGNED BITMAPINFOHEADER *bmi
	{
		OutputDebugString(_T("File header does not conform to requirements."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Inspect surface description
	//
	if (FAILED(pSurface->GetDesc(&Desc)))
	{
		OutputDebugString(_T("File header does not conform to requirements."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Verify that extents are identical to BMP extents
	//
	if ((Desc.Width != (UINT)pbmih->biWidth) ||
		(Desc.Height != (UINT)pbmih->biHeight))
	{
		OutputDebugString(_T("Extent mismatch (BMP, surface)."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Lock the entire extent, for use as a destination for the color-converted BMP data
	//
	if (FAILED(pSurface->LockRect(&d3dLockedRect, // D3DMLOCKED_RECT* pLockedRect,
                                  NULL,           // CONST RECT* pRect,
                                  0           ))) // DWORD Flags
	{
		OutputDebugString(_T("LockRect failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Put bitmap data in
	//
	if (FAILED(FillLockedRect( &d3dLockedRect,  // D3DMLOCKED_RECT *pLockedRect,
	                           pbmih->biHeight, // UINT uiBitmapHeight,
	                           pbmih->biWidth,  // UINT uiBitmapWidth,
	                           pImage,          // BYTE *pImage,
	                           uiSrcPixelSize,  // UINT uiSrcPixelSize,
	                           Desc.Format)))   // D3DMFORMAT d3dFormat
	{
		OutputDebugString(_T("FillLockedRect failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Surface manipulation is complete.
	//
	if (FAILED(pSurface->UnlockRect())) // UINT Level
	{
		OutputDebugString(_T("UnlockRect failed."));
		hr = E_FAIL;
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
	if (FAILED(ReadFileToMemoryTimed(pszFile, // TCHAR *ptszFilename
								     &pBMP))) // BYTE **ppByte
	{
		OutputDebugString(_T("ReadFileToMemoryTimed failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Retrieve description of surface
	//
	if (FAILED(pSurface->GetDesc(&SurfaceDesc)))
	{
		OutputDebugString(_T("IDirect3DMobileSurface::GetDesc failed."));
		hr = E_FAIL;
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
		if (FAILED(pSurface->GetDevice(&pDevice))) // IDirect3DMobileDevice** ppDevice
		{
			OutputDebugString(_T("GetDevice failed."));
			hr = E_FAIL;
			goto cleanup;
		}

		//
		// Create an image surface of the same extents as the texture surface level
		//
		if (FAILED(pDevice->CreateImageSurface(SurfaceDesc.Width,   // UINT Width
		                                       SurfaceDesc.Height,  // UINT Height
		                                       SurfaceDesc.Format,  // D3DMFORMAT Format
		                                       &pImageSurface )))   // IDirect3DMobileSurface** ppSurface
		{
			OutputDebugString(_T("CreateImageSurface failed."));
			hr = E_FAIL;
			goto cleanup;
		}

		//
		// Fill the image surface
		//
		if (FAILED(FillSurface(pImageSurface, // IDirect3DMobileSurface* pSurface
		                       pBMP)))        // BYTE *pBMP
		{
			OutputDebugString(_T("FillSurface failed."));
			hr = E_FAIL;
			goto cleanup;
		}

		//
		// Copy data from image surface to to texture
		//
		if (FAILED(pDevice->CopyRects(pImageSurface,   // IDirect3DMobileSurface* pSourceSurface
		                              NULL,            // CONST RECT* pSourceRectsArray
		                              0,               // UINT cRects
		                              pSurface,        // IDirect3DMobileSurface* pDestinationSurface
		                              NULL)))          // CONST POINT* pDestPointsArray
		{
			OutputDebugString(_T("CopyRects failed."));
			hr = E_FAIL;
			goto cleanup;
		}

		//
		// Force a flush to ensure immediate processing of CopyRects
		//
		(VOID)pDevice->ValidateDevice(&dwValidateDevice);

	}
	else if (D3DMRTYPE_SURFACE == SurfaceDesc.Type)
	{
		if (FAILED(FillSurface(pSurface, // IDirect3DMobileSurface* pSurface
							   pBMP)))   // BYTE *pBMP
		{
			OutputDebugString(_T("FillSurface failed."));
			hr = E_FAIL;
			goto cleanup;
		}
	}
	else
	{
		OutputDebugString(_T("Unknown D3DMSURFACE_DESC.Type."));
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

