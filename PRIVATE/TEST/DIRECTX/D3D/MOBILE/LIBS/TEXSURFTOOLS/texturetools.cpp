//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <d3dm.h>
#include "TextureTools.h"
#include "common.h"

//
// Maximum allowable length for OutputDebugString text
//
#define MAX_DEBUG_OUT 256

#define countof(x)  (sizeof(x)/sizeof(*(x)))

extern HANDLE g_hInstance;

static void Debug(LPCTSTR szFormat, ...);
HRESULT ValidateBitmapHeaders(CONST UNALIGNED BITMAPFILEHEADER *bmfh, CONST UNALIGNED BITMAPINFOHEADER *bmi);

#define D3DQA_SOLID_TEXTURE_WIDTH 4
#define D3DQA_SOLID_TEXTURE_HEIGHT 4

BOOL IsPowTwo(UINT uiValue)
{
    return (!(uiValue == 0 || (uiValue & (uiValue - 1)) != 0));
}

//
// CapsAllowCreateTexture
//
//   Inspects texture extents, determining if there is any capability bit that preclude the 
//   extents from being used.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Direct3D device
//   UINT uiWidth:  Width of texture extent
//   UINT uiHeight:  Height of texture extent
//   
// Return Value:
//
//   BOOL:  TRUE if texture dimensions are allowable; false otherwise
//
BOOL CapsAllowCreateTexture(LPDIRECT3DMOBILEDEVICE pDevice, UINT uiWidth, UINT uiHeight)
{
	float fAspect1;
	float fAspect2;

	D3DMCAPS Caps;

	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		return FALSE;
	}

	if ((uiWidth > Caps.MaxTextureWidth) ||
		(uiHeight > Caps.MaxTextureHeight))
	{
		return FALSE;
	}

	if ((Caps.TextureCaps & D3DMPTEXTURECAPS_SQUAREONLY) &&
		(uiWidth != uiHeight))
	{
		return FALSE;
	}

	if (Caps.TextureCaps & D3DMPTEXTURECAPS_POW2)
	{
		if (!(IsPowTwo(uiHeight)))
			return FALSE;

		if (!(IsPowTwo(uiWidth)))
			return FALSE;
	}

	fAspect1 = (float)uiWidth / (float)uiHeight;
	fAspect2 = (float)uiHeight / (float)uiWidth;

	//
	// If device exposes an aspect ratio limit, verify
	// that the proposed extents are satisfactory
	//
	if (Caps.MaxTextureAspectRatio)
	{
		if ((fAspect1 > Caps.MaxTextureAspectRatio) ||
			(fAspect2 > Caps.MaxTextureAspectRatio))
		{
			return FALSE;
		}
	}

	return TRUE;
}



// 
// GetMipLevelExtents
//
//   Given some specific top-level mip-map extents, and an index of a subordinate surface,
//   compute the expected extents for that particular surface.
//
// Arguments:
//  
//   UINT uiTopWidth:  Extent of top-level of mip-map
//   UINT uiTopHeight:  Extent of top-level of mip-map
//   UINT uiLevel:  Level for which to compute extents
//   UINT *puiLevelWidth:  Output of expected extent
//   UINT *puiLevelHeight:  Output of expected extent
//   
//
// Return Value:
//
//  HRESULT indicates success or failure
//
HRESULT GetMipLevelExtents(UINT uiTopWidth, UINT uiTopHeight,
                           UINT uiLevel, UINT *puiLevelWidth, UINT *puiLevelHeight)
{
	//
	// Storage for intermediate computations
	//
	UINT uiExpectedLevelWidth, uiExpectedLevelHeight;

	//
	// Iterator for computing increasingly small mip level extents
	//
	UINT uiLevelIter;

	//
	// Bad-parameter check
	//
	if ((NULL == puiLevelWidth) ||
	    (NULL == puiLevelHeight))
	{
		OutputDebugString(_T("Invalid argument(s) for GetMipLevelExtents."));
		return E_FAIL;
	}

	//
	// Initialize level width/height expectations
	//
	uiExpectedLevelWidth = uiTopWidth;
	uiExpectedLevelHeight = uiTopHeight;

	for (uiLevelIter = 0; uiLevelIter < uiLevel; uiLevelIter++)
	{
		//
		// Reduce extents by a half; truncating to floor of fractional value
		//
		uiExpectedLevelWidth /= 2;
		uiExpectedLevelHeight /= 2;

		//
		// Clamp to one if necessary; zero-width surface is not allowed
		//
		if (0 == uiExpectedLevelWidth)
			uiExpectedLevelWidth = 1;
		if (0 == uiExpectedLevelHeight)
			uiExpectedLevelHeight = 1;
	}

	*puiLevelWidth = uiExpectedLevelWidth;
	*puiLevelHeight = uiExpectedLevelHeight;

	return S_OK;
}


//
// GetTexture
//
//   Loads a BMP (that satisfies the constraints required by ValidateBitmapHeaders) into a D3D texture.
//
// Arguments:  
//   
//   BYTE *pBMP:  Pointer to bitmap file (loaded into memory)
//   PIXEL_UNPACK *pDstFormat:  Format of destination buffer
//   D3DMFORMAT d3dFormat:  Format of destination buffer
//   D3DMPOOL d3dPool:  Pool for destination buffer
//   IDirect3DMobileTexture** ppTexture
//   IDirect3DMobileDevice* pd3dDevice
//   
// Return Value:
//  
//   HRESULT indicates success or failure 
//
HRESULT GetTexture(BYTE *pBMP, PIXEL_UNPACK *pDstFormat, D3DMFORMAT d3dFormat, D3DMPOOL d3dPool, IDirect3DMobileTexture** ppTexture, IDirect3DMobileDevice* pd3dDevice)
{
	//
	// Pointers to various significant offsets within the BMP file
	//
	UNALIGNED BITMAPFILEHEADER *pbmfh;
	UNALIGNED BITMAPINFOHEADER *pbmih;
	BYTE *pImage;

	//
	// Intermediate surface for data
	//
	LPDIRECT3DMOBILESURFACE pSurface = NULL;
	LPDIRECT3DMOBILESURFACE pTexSurface = NULL;

	//
	// Result defaults to success; failure conditions are specifically set
	//
	HRESULT hr = S_OK;

	//
	// Only one pixel size is accepted as input:  24BPP
	//
	CONST UINT uiSrcPixelSize = 3;

	//
	// Iterators for walking through src/dst surfaces on a pixel-by-pixel granularity
	//
	UINT uiX, uiY;

	//
	// Number of bytes of color data, in input BMP
	//
	UINT uiExpectedByteSize;

	//
	// Profiling timers, for informational debug output
	//
	DWORD dwProfileStart, dwProfileEnd;

	//
	// Pointers to image data; used during per-pixel and per-line steppings
	// for color conversion
	//
	BYTE *pSrcLine, *pDstLine;
	BYTE *pSrcPixel, *pDstPixel;

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
	if (FAILED(ValidateBitmapHeaders(pbmfh,   // BITMAPFILEHEADER *bmfh,
									 pbmih))) // BITMAPINFOHEADER *bmih
	{
		OutputDebugString(_T("File header does not conform to requirements."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Compute the number of bytes of image data expected in this bitmap
	//
	uiExpectedByteSize = pbmih->biHeight * pbmih->biWidth * pbmih->biBitCount / 8;

	//
	// Create a D3D texture according to the extents reported in the bitmap
	//
	if (FAILED(pd3dDevice->CreateImageSurface((UINT)pbmih->biWidth,   // UINT Width
	                                          (UINT)pbmih->biHeight,  // UINT Height
	                                          d3dFormat,              // D3DMFORMAT Format
	                                          &pSurface )))           // IDirect3DMobileSurface** ppSurface
	{
		OutputDebugString(_T("CreateImageSurface failed."));
		hr = E_FAIL;
		goto cleanup;
	}


	//
	// Create a D3D texture according to the extents reported in the bitmap
	//
	if (FAILED(pd3dDevice->CreateTexture(
							 (UINT)pbmih->biWidth,  // UINT Width,
							 (UINT)pbmih->biHeight, // UINT Height,
							 1,                     // UINT Levels,
							 0,                     // DWORD Usage,
							 d3dFormat,             // D3DMFORMAT Format,
							 d3dPool,               // D3DMPOOL Pool,
							 ppTexture)))           // IDirect3DMobileTexture** ppTexture
	{
		OutputDebugString(_T("CreateTexture failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Lock the entire texture, for use as a destination for the color-converted BMP data
	//
	if (FAILED(pSurface->LockRect(&d3dLockedRect, // D3DMLOCKED_RECT* pLockedRect,
	                              NULL,           // CONST RECT* pRect,
	                              0           ))) // DWORD Flags
	{
		OutputDebugString(_T("LockRect failed."));
		(*ppTexture)->Release();
		(*ppTexture) = NULL;
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Required:  bottom-up DIB.  Account for this requirement here, by adjusting the starting
	// source pointer.
	//
	pSrcLine = pImage + ((UINT)pbmih->biHeight - 1) * uiSrcPixelSize * (UINT)pbmih->biWidth; 

	//
	// Destination top-left is first byte of locked bits
	//
	pDstLine = (BYTE*)(d3dLockedRect.pBits);

	//
	// Capture profiling data, for informational debug output
	//
	dwProfileStart = GetTickCount();

	//
	// Loop through image data, performing a color conversion for each pixel, and copying into the
	// destination buffer
	//
	for(uiY = 0; uiY < (UINT)pbmih->biHeight; uiY++)
	{
		
		//
		// Per-pixel color conversion begins with the first pixel of the row
		//
		pSrcPixel = pSrcLine;
		pDstPixel = pDstLine;

		for(uiX = 0; uiX < (UINT)pbmih->biWidth; uiX++)
		{

			//
			// Color conversion from 24BPP BMP (R8G8B8) to arbitrary destination format
			//
			ConvertPixel(&UnpackFormat[D3DMFMT_R8G8B8], pDstFormat, pSrcPixel, pDstPixel);

			//
			// Increment to next pixel in row
			//
			pSrcPixel += uiSrcPixelSize;
			pDstPixel += pDstFormat->uiTotalBytes;
		}

		//
		// Decrement source pointer by one line (bottom-up DIB orientation)
		//
		pSrcLine -= (UINT)pbmih->biWidth * uiSrcPixelSize;

		//
		// Increment destination pointer by one line (D3D textures are implicitly top-down in orientation)
		//
		pDstLine += d3dLockedRect.Pitch;
	}

	//
	// Capture profiling data, for informational debug output
	//
	dwProfileEnd = GetTickCount();

	//
	// Log profiling data for most recent operation
	//
	Debug(_T("Elapsed time; BMP data color conversion:  %lu ms."), dwProfileEnd - dwProfileStart);

	//
	// Texture manipulation is complete.
	//
	if (FAILED(pSurface->UnlockRect())) // UINT Level
	{
		OutputDebugString(_T("UnlockRect failed."));
		(*ppTexture)->Release();
		(*ppTexture) = NULL;
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Retrieve a surface interface pointer
	//
	if (FAILED((*ppTexture)->GetSurfaceLevel(0,              // UINT Level
										 &pTexSurface))) // IDirect3DMobileSurface** ppSurfaceLevel
	{
		OutputDebugString(_T("GetSurfaceLevel failed."));
		(*ppTexture)->Release();
		(*ppTexture) = NULL;
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Copy data to texture
	//
	if (FAILED(pd3dDevice->CopyRects(pSurface,    // IDirect3DMobileSurface* pSourceSurface
                                     NULL,        // CONST RECT* pSourceRectsArray
                                     0,           // UINT cRects
                                     pTexSurface, // IDirect3DMobileSurface* pDestinationSurface
                                     NULL)))      // CONST POINT* pDestPointsArray
	{
		OutputDebugString(_T("CopyRects failed."));
		(*ppTexture)->Release();
		(*ppTexture) = NULL;
		hr = E_FAIL;
		goto cleanup;
	}


cleanup:

	if (pSurface)
		pSurface->Release();

	if (pTexSurface)
		pTexSurface->Release();

	return hr;
}


//
// GetTextureFromFile
//
//   Loads a BMP (that satisfies the constraints required by ValidateBitmapHeaders) into a D3D texture.
//
// Arguments:  
//   
//   CONST TCHAR *pszFile:  Filename for input BMP
//   PIXEL_UNPACK *pDstFormat:  Format of destination buffer
//   D3DMFORMAT d3dFormat:  Format of destination buffer
//   D3DMPOOL d3dPool:  Pool for destination buffer
//   IDirect3DMobileTexture** ppTexture
//   IDirect3DMobileDevice* pd3dDevice
//   
// Return Value:
//  
//   HRESULT indicates success or failure 
//
HRESULT GetTextureFromFile(CONST TCHAR *pszFile, PIXEL_UNPACK *pDstFormat, D3DMFORMAT d3dFormat, D3DMPOOL d3dPool, IDirect3DMobileTexture** ppTexture, IDirect3DMobileDevice* pd3dDevice)
{
	//
	// Result defaults to success; failure conditions are specifically set
	//
	HRESULT hr = S_OK;

	//
	// Pointers to BMP (loaded into memory)
	//
	BYTE *pBMP;

	//
	// Load file into memory (allocated "automatically")
	//
	if (FAILED(ReadFileToMemoryTimed(pszFile, // TCHAR *ptszFilename
								     &pBMP))) // BYTE **ppByte
	{
		OutputDebugString(_T("Unable read file."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// From the bitmap bits, create a texture
	//
	if (FAILED(GetTexture(pBMP,         // BYTE *pBMP
                          pDstFormat,   // PIXEL_UNPACK *pDstFormat
                          d3dFormat,    // D3DMFORMAT d3dFormat
                          d3dPool,      // D3DMPOOL d3dPool
                          ppTexture,    // IDirect3DMobileTexture** ppTexture
                          pd3dDevice )))// IDirect3DMobileDevice* pd3dDevice
	{
		OutputDebugString(_T("GetTexture failed."));
		hr = E_FAIL;
		goto cleanup;
	}

cleanup:

	if (pBMP)
		free(pBMP);

	return hr;
}

//
// GetTextureFromResource
//
//   Loads a BMP (that satisfies the constraints required by ValidateBitmapHeaders) into a D3D texture.
//
// Arguments:  
//   
//   CONST TCHAR *pszFile:  Filename for input BMP
//   PIXEL_UNPACK *pDstFormat:  Format of destination buffer
//   D3DMFORMAT d3dFormat:  Format of destination buffer
//   D3DMPOOL d3dPool:  Pool for destination buffer
//   IDirect3DMobileTexture** ppTexture
//   IDirect3DMobileDevice* pd3dDevice
//   
// Return Value:
//  
//   HRESULT indicates success or failure 
//
HRESULT GetTextureFromResource(CONST TCHAR *pszName, HMODULE hMod, PIXEL_UNPACK *pDstFormat, D3DMFORMAT d3dFormat, D3DMPOOL d3dPool, IDirect3DMobileTexture** ppTexture, IDirect3DMobileDevice* pd3dDevice)
{
	//
	// Result defaults to success; failure conditions are specifically set
	//
	HRESULT hr = S_OK;

	//
	// Handle to the resource
	//
	HRSRC hResource = NULL;

    //
    // Handle to the memory of the resource. Get to the pointer with LockResource.
    //
    HGLOBAL hgBitmap = NULL;

	//
	// Pointers to BMP (loaded into memory)
	//
	BYTE *pBMP;

	//
	// Get a handle to the resource based on the name. Use resource type
	// RCDATA so that the bitmap file format is conserved.
	//
	hResource = FindResource(hMod, pszName, RT_RCDATA);
	if (NULL == hResource)
	{
	    hr = HRESULT_FROM_WIN32(GetLastError());
	    OutputDebugString(_T("Could not load texture resource (FindResource failed)."));
	    goto cleanup;
	}

	//
	// Use the resource's handle to load the resource into memory
	//
	hgBitmap = LoadResource(hMod, hResource);
	if (NULL == hgBitmap)
	{
	    hr = HRESULT_FROM_WIN32(GetLastError());
	    OutputDebugString(_T("Could not load texture resource (LoadResource failed)."));
	    goto cleanup;
	}

	//
	// Get a pointer to the resource memory. The memory does not need to be freed.
	//
	pBMP = (BYTE*) LockResource(hgBitmap);
	if (NULL == hgBitmap)
	{
	    hr = HRESULT_FROM_WIN32(GetLastError());
	    OutputDebugString(_T("Could not load texture resource (LockResource failed)."));
	    goto cleanup;
	}

	//
	// From the bitmap bits, create a texture
	//
	if (FAILED(GetTexture(pBMP,         // BYTE *pBMP
                          pDstFormat,   // PIXEL_UNPACK *pDstFormat
                          d3dFormat,    // D3DMFORMAT d3dFormat
                          d3dPool,      // D3DMPOOL d3dPool
                          ppTexture,    // IDirect3DMobileTexture** ppTexture
                          pd3dDevice )))// IDirect3DMobileDevice* pd3dDevice
	{
		OutputDebugString(_T("GetTexture failed."));
		hr = E_FAIL;
		goto cleanup;
	}

cleanup:
    // There is no way to clean up an RCDATA resource that's been loaded.

	return hr;
}

//
// GetTextureFromMem
//
//   Loads a memory-loaded BMP (that satisfies the constraints required by ValidateBitmapHeaders)
//   into a D3D texture.
//
// Arguments:  
//   
//   BYTE *pBMP:  Pointer to BMP file (loaded into memory)
//   PIXEL_UNPACK *pDstFormat:  Format of destination buffer
//   D3DMFORMAT d3dFormat:  Format of destination buffer
//   D3DMPOOL d3dPool:  Pool for destination buffer
//   IDirect3DMobileTexture** ppTexture
//   IDirect3DMobileDevice* pd3dDevice
//   
// Return Value:
//  
//   HRESULT indicates success or failure 
//
HRESULT GetTextureFromMem(BYTE *pBMP, PIXEL_UNPACK *pDstFormat, D3DMFORMAT d3dFormat, D3DMPOOL d3dPool, IDirect3DMobileTexture** ppTexture, IDirect3DMobileDevice* pd3dDevice)
{
	//
	// From the bitmap bits, create a texture
	//
	if (FAILED(GetTexture(pBMP,         // BYTE *pBMP
                          pDstFormat,   // PIXEL_UNPACK *pDstFormat
                          d3dFormat,    // D3DMFORMAT d3dFormat
                          d3dPool,      // D3DMPOOL d3dPool
                          ppTexture,    // IDirect3DMobileTexture** ppTexture
                          pd3dDevice )))// IDirect3DMobileDevice* pd3dDevice
	{
		OutputDebugString(_T("GetTexture failed."));
		return E_FAIL;
	}
	else
	{
		return S_OK;
	}
}


//
// GetValidRGBTextureFormat
//
//   Identify a valid RGB texture format, if any, including the specified usage.
//
// Arguments:  
//   
//   LPDIRECT3DMOBILEDEVICE pDevice:  Direct3D Mobile device
//   DWORD dwUsage:  Usage to include in CheckDeviceFormat call
//   D3DMFORMAT *pFormat:  Resultant texture format
//   
// Return Value:
//  
//   HRESULT indicates success or failure 
//
HRESULT GetValidRGBTextureFormat(LPDIRECT3DMOBILEDEVICE pDevice, DWORD dwUsage, D3DMFORMAT *pFormat)
{
	//
	// For iteration of CheckDeviceFormat calls
	//
	CONST UINT uiNumRGBFormats = 10;
	UINT uiFormatIter;
	D3DMFORMAT TextureFormat;

	//
	// All failure conditions specifically set this to an error
	//
	HRESULT hr = S_OK;

	//
	// Object interface needed for CheckDeviceFormat calls
	//
	LPDIRECT3DMOBILE pD3D = NULL;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Indicates whether the search for a valid texture format has been
	// successful or not
	//
	BOOL bFoundValidFormat = FALSE;

	//
	// Display mode, used to determine a format to use
	//
	D3DMDISPLAYMODE Mode;

	//
	// Retrieve the object interface pointer
	//
	if (FAILED(pDevice->GetDirect3D(&pD3D)))
	{
		OutputDebugString(L"GetDirect3D failed.");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Retrieve the device capabilities
	//
	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(L"GetDeviceCaps failed.");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Retrieve current display mode
	//
	if (FAILED(pDevice->GetDisplayMode(&Mode))) // D3DMDISPLAYMODE* pMode
	{
		OutputDebugString(L"GetDisplayMode failed.");
		hr = E_FAIL;
		goto cleanup;
	}
	
	//
	// Find a valid format for textures
	//
	for (uiFormatIter = 0; uiFormatIter < uiNumRGBFormats; uiFormatIter++)
	{
		switch(uiFormatIter)
		{
		case 0:
			TextureFormat = D3DMFMT_R8G8B8;
			break;
		case 1:
			TextureFormat = D3DMFMT_A8R8G8B8;
			break;
		case 2:
			TextureFormat = D3DMFMT_X8R8G8B8;
			break;
		case 3:
			TextureFormat = D3DMFMT_R5G6B5;
			break;
		case 4:
			TextureFormat = D3DMFMT_X1R5G5B5;
			break;
		case 5:
			TextureFormat = D3DMFMT_A1R5G5B5;
			break;
		case 6:
			TextureFormat = D3DMFMT_A4R4G4B4;
			break;
		case 7:
			TextureFormat = D3DMFMT_R3G3B2;
			break;
		case 8:
			TextureFormat = D3DMFMT_A8R3G3B2;
			break;
		case 9:
			TextureFormat = D3DMFMT_X4R4G4B4;
			break;
		default:
			OutputDebugString(L"GetValidRGBTextureFormat failed.");
			hr = E_FAIL;
			goto cleanup;
		};


		if (SUCCEEDED(pD3D->CheckDeviceFormat(Caps.AdapterOrdinal,// UINT Adapter
		                                      Caps.DeviceType,    // D3DMDEVTYPE DeviceType
		                                      Mode.Format,        // D3DMFORMAT AdapterFormat
		                                      dwUsage,            // ULONG Usage
		                                      D3DMRTYPE_TEXTURE,  // D3DMRESOURCETYPE RType
		                                      TextureFormat)))    // D3DMFORMAT CheckFormat
		{
			bFoundValidFormat = TRUE;
			break;
		}
	}

	if (FALSE == bFoundValidFormat)
	{
		Debug(_T("CheckDeviceFormat indicates no valid texture formats."));
		hr = E_FAIL;
		goto cleanup;
	}

	*pFormat = TextureFormat;

cleanup:

	if (pD3D)
		pD3D->Release();

	return hr;
}


//
// GetSolidTexture
//
//   Creates a single level texture in a supported RGB format; and fills it with the
//   specified color.
//
// Arguments:  
//   
//   LPDIRECT3DMOBILEDEVICE pDevice:  Direct3D Mobile device
//   D3DMCOLOR Color:  Color to fill surface with
//   LPDIRECT3DMOBILETEXTURE *ppTexture:  Resultant texture
//   
// Return Value:
//  
//   HRESULT indicates success or failure 
//
HRESULT GetSolidTexture(LPDIRECT3DMOBILEDEVICE pDevice, D3DMCOLOR Color, LPDIRECT3DMOBILETEXTURE *ppTexture)
{
	//
	// Result defaults to success; failure conditions are specifically set
	//
	HRESULT hr = S_OK;

	//
	// Surface level; must obtain for ColorFill
	//
	LPDIRECT3DMOBILESURFACE pSurface = NULL;

	//
	// Format of texture; detected as valid via CheckDeviceFormat
	//
	D3DMFORMAT TextureFormat;

	//
	// Pool for texture creation
	//
	D3DMPOOL TexturePool;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Bad parameter check
	//
	if (NULL == ppTexture)
	{
		OutputDebugString(_T("GetSolidTexture:  failing due to invalid argument(s)."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Initialize texture to NULL to ensure that cleanup block won't AV
	//
	*ppTexture = NULL;

	//
	// Get device capabilities to check for mip-mapping support
	//
	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(_T("GetDeviceCaps failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Find a valid texture pool
	//
	if (D3DMSURFCAPS_SYSTEXTURE & Caps.SurfaceCaps)
	{
		TexturePool = D3DMPOOL_SYSTEMMEM;
	}
	else if (D3DMSURFCAPS_VIDTEXTURE & Caps.SurfaceCaps)
	{
		TexturePool = D3DMPOOL_VIDEOMEM;
	}
	else
	{
		OutputDebugString(_T("D3DMCAPS.SurfaceCaps:  No valid texture pool."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Search for a suitable texture format
	//
	if (FAILED(GetValidRGBTextureFormat( pDevice,                // LPDIRECT3DMOBILEDEVICE pDevice,
	                                     D3DMUSAGE_RENDERTARGET, // DWORD dwUsage,
	                                     &TextureFormat)))       // D3DMFORMAT *pFormat
	{
		OutputDebugString(_T("GetValidRGBTextureFormat failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Create a D3D texture according to the extents reported in the bitmap
	//
	if (FAILED(pDevice->CreateTexture(
							 D3DQA_SOLID_TEXTURE_WIDTH,  // UINT Width,
							 D3DQA_SOLID_TEXTURE_HEIGHT, // UINT Height,
							 1,                          // UINT Levels,
							 D3DMUSAGE_RENDERTARGET,     // DWORD Usage,
							 TextureFormat,              // D3DMFORMAT Format,
							 TexturePool,                // D3DMPOOL Pool,
							 ppTexture)))                // IDirect3DMobileTexture** ppTexture
	{
		OutputDebugString(_T("CreateTexture failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Obtain the sole surface level of the texture
	//
	if (FAILED((*ppTexture)->GetSurfaceLevel(0, &pSurface)))
	{
		OutputDebugString(_T("GetSurfaceLevel failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Fill the texture with the specified color
	//
	if (FAILED(pDevice->ColorFill(pSurface,NULL,Color)))
	{
		OutputDebugString(_T("CreateTexture failed."));
		hr = E_FAIL;
		goto cleanup;
	}

cleanup:

	if (FAILED(hr))
		if (ppTexture)
			if (*ppTexture)
				(*ppTexture)->Release();

	if (pSurface)
		pSurface->Release();

	return hr;
}



//
// GetBitmapResourcePointer
//
//   Retrieves a pointer to a bitmap resource from this executable.  Assumes
//   resource type RT_RCDATA.
//
// Arguments:
//  
//   INT iResourceID:  Resource ID for use with FindResource
//   PBYTE *ppByte:  Storage to write pointer to bitmap into
//   
// Return Value:
//
//   HRESULT indicates success or failure
//
HRESULT GetBitmapResourcePointer(INT iResourceID, PBYTE *ppByte)
{
	HRESULT hr = S_OK;
	HRSRC hRsrc;
	HGLOBAL hGlobal;
	BYTE *pBMP;

	//
	// Parameter validation
	//
	if ((NULL == ppByte) || (0 == iResourceID))
	{
		OutputDebugString(_T("GetBitmapResourcePointer: Invalid argument."));
		hr = E_FAIL;
		goto cleanup;
	}


	//
	// A handle to the specified resource's info block indicates success.
	// NULL indicates failure.
	//
	// Note: The HINSTANCE of a DLL is the same as the HMODULE of the DLL
	//
	hRsrc = FindResource((HMODULE)g_hInstance,         // HMODULE hModule, 
	                     MAKEINTRESOURCE(iResourceID), // LPCWSTR lpName, 
	                     RT_RCDATA);                   // LPCWSTR lpType 
	if (NULL == hRsrc)
	{
		OutputDebugString(_T("FindResource failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// A handle to the data associated with the resource indicates
	// success. NULL indicates failure.
	//
	// Note: The HINSTANCE of a DLL is the same as the HMODULE of the DLL
	//
	hGlobal = LoadResource((HMODULE)g_hInstance,  // HMODULE hModule, 
	                       hRsrc);                // HRSRC hResInfo 
	if (NULL == hGlobal)
	{
		OutputDebugString(_T("LoadResource failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// If the loaded resource is locked, the return value is a pointer to
	// the first byte of the resource; otherwise, it is NULL. 
	// 
	pBMP = (PBYTE)LockResource(hGlobal); // HGLOBAL hResData 

	if (NULL == pBMP)
	{
		OutputDebugString(_T("LockResource failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	*ppByte = pBMP;

cleanup:

	return hr;

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
static void Debug(LPCTSTR szFormat, ...)
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

