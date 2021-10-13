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
#include <tchar.h>
#include "utils.h"
#include "TextureTools.h"

HRESULT GetPixelFormat(D3DMFORMAT d3dFormat, PIXEL_UNPACK **ppDstFormat);


//
// ResizeTarget
//
//   Resets a device, with a new render target and viewport size
//
// Argument:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying device
//   HWND hWnd:  Target window
//   D3DMPRESENT_PARAMETERS* pPresentParms:  Parameters used to create this device originally
//   UINT uiRenderExtentX:  New extent
//   UINT uiRenderExtentY:  New extent
//
// Return Value:
// 
//   HRESULT indicates success or failure
//
HRESULT ResizeTarget(LPDIRECT3DMOBILEDEVICE pDevice,
                     HWND hWnd,
                     D3DMPRESENT_PARAMETERS* pPresentParms,
                     UINT uiRenderExtentX,
                     UINT uiRenderExtentY)
{
	//
	// Render target region to use for output
	//
	D3DMVIEWPORT Viewport;

	//
	// Result of this function
	//
	HRESULT hr = S_OK;

	//
	// Retrieve current viewport; z settings will be retained, others
	// will be overwritten
	//
	if (FAILED(pDevice->GetViewport(&Viewport)))
	{
		OutputDebugString(_T("GetViewport failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Image capture utility operates on window extents; so adjust window to the size that
	// is intended to be used in this test
	//
	// Nonzero indicates success. Zero indicates failure.
	//
	if (0 == SetWindowPos(hWnd,           // HWND hWnd, 
						  HWND_TOPMOST,   // HWND hWndInsertAfter, 
						  0,              // int X, 
						  0,              // int Y, 
						  uiRenderExtentX,// int cx, 
						  uiRenderExtentY,// int cy, 
						  SWP_NOMOVE))    // UINT uFlags 
	{
		OutputDebugString(_T("SetWindowPos failed."));
		hr = E_FAIL;
		goto cleanup;
	}
	if (0 == UpdateWindow(hWnd))
	{
		OutputDebugString(_T("UpdateWindow failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Reset should re-detect these
	//
	pPresentParms->BackBufferWidth = 0;
	pPresentParms->BackBufferHeight = 0;
	
	//
	// Reset the device, to handle the resized window; all presentation parameters are identical
	// to initial device instance
	// 
	if (FAILED(pDevice->Reset(pPresentParms)))
	{
		OutputDebugString(_T("Reset failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Overwrite old viewport settings with updated settings
	//
	Viewport.X = 0;
	Viewport.Y = 0;
	Viewport.Width = uiRenderExtentX;
	Viewport.Height = uiRenderExtentY;
	if (FAILED(pDevice->SetViewport(&Viewport)))
	{
		OutputDebugString(_T("SetViewport failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Clear the backbuffer and the zbuffer; often important
	// due to frame capture tests
	//
	pDevice->Clear( 0, NULL, D3DMCLEAR_TARGET|D3DMCLEAR_ZBUFFER,
						D3DMCOLOR_XRGB(0,0,0), 1.0f, 0 );


cleanup:

	return hr;
}



//
// SetDefaultTextureStates
//
//   Initializes texture cascade with reasonable defaults
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying Direct3D Mobile device
//
// Return value:
//
//   HRESULT indicates success or failure
//
HRESULT SetDefaultTextureStates(LPDIRECT3DMOBILEDEVICE pDevice)
{

	//
	// Give args a default state
	//
	pDevice->SetTextureStageState( 0, D3DMTSS_COLORARG1, D3DMTA_TEXTURE );
	pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAARG1, D3DMTA_TEXTURE );
	pDevice->SetTextureStageState( 1, D3DMTSS_COLORARG1, D3DMTA_TEXTURE );
	pDevice->SetTextureStageState( 1, D3DMTSS_ALPHAARG1, D3DMTA_TEXTURE );

	pDevice->SetTextureStageState( 2, D3DMTSS_COLORARG1, D3DMTA_TEXTURE );
	pDevice->SetTextureStageState( 2, D3DMTSS_ALPHAARG1, D3DMTA_TEXTURE );
	pDevice->SetTextureStageState( 3, D3DMTSS_COLORARG1, D3DMTA_TEXTURE );
	pDevice->SetTextureStageState( 3, D3DMTSS_ALPHAARG1, D3DMTA_TEXTURE );

	pDevice->SetTextureStageState( 0, D3DMTSS_COLORARG2, D3DMTA_CURRENT );
	pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAARG2, D3DMTA_CURRENT );
	pDevice->SetTextureStageState( 1, D3DMTSS_COLORARG2, D3DMTA_CURRENT );
	pDevice->SetTextureStageState( 1, D3DMTSS_ALPHAARG2, D3DMTA_CURRENT );

	pDevice->SetTextureStageState( 2, D3DMTSS_COLORARG2, D3DMTA_CURRENT );
	pDevice->SetTextureStageState( 2, D3DMTSS_ALPHAARG2, D3DMTA_CURRENT );
	pDevice->SetTextureStageState( 3, D3DMTSS_COLORARG2, D3DMTA_CURRENT );
	pDevice->SetTextureStageState( 3, D3DMTSS_ALPHAARG2, D3DMTA_CURRENT );

	//
	// Disable output of texture stages
	//
	pDevice->SetTextureStageState( 0, D3DMTSS_COLOROP, D3DMTOP_DISABLE );
	pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAOP, D3DMTOP_DISABLE );
	pDevice->SetTextureStageState( 1, D3DMTSS_COLOROP, D3DMTOP_DISABLE );
	pDevice->SetTextureStageState( 1, D3DMTSS_ALPHAOP, D3DMTOP_DISABLE );
	pDevice->SetTextureStageState( 2, D3DMTSS_COLOROP, D3DMTOP_DISABLE );
	pDevice->SetTextureStageState( 2, D3DMTSS_ALPHAOP, D3DMTOP_DISABLE );
	pDevice->SetTextureStageState( 3, D3DMTSS_COLOROP, D3DMTOP_DISABLE );
	pDevice->SetTextureStageState( 3, D3DMTSS_ALPHAOP, D3DMTOP_DISABLE );

	return S_OK;
}




//
// SetTestTextureStates
//
//   Set texture stage states up in preparation for testing.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying Direct3D Mobile device
//   TEX_STAGE_SETTINGS *pTexStageSettings:  Texture cascade configuration for this stage
//   UINT uiStageIndex:  Stage index to configure
//   
// Return value:
//
//   HRESULT indicates success or failure
//
HRESULT SetTestTextureStates(LPDIRECT3DMOBILEDEVICE pDevice,
                             TEX_STAGE_SETTINGS *pTexStageSettings,
                             UINT uiStageIndex)
{
	if (
		(FAILED(pDevice->SetTextureStageState( uiStageIndex, D3DMTSS_COLOROP,   pTexStageSettings->ColorOp       ))) ||
		(FAILED(pDevice->SetTextureStageState( uiStageIndex, D3DMTSS_COLORARG1, pTexStageSettings->ColorTexArg1  ))) ||
		(FAILED(pDevice->SetTextureStageState( uiStageIndex, D3DMTSS_COLORARG2, pTexStageSettings->ColorTexArg2  ))) ||
		(FAILED(pDevice->SetTextureStageState( uiStageIndex, D3DMTSS_ALPHAOP,   pTexStageSettings->AlphaOp       ))) ||
		(FAILED(pDevice->SetTextureStageState( uiStageIndex, D3DMTSS_ALPHAARG1, pTexStageSettings->AlphaTexArg1  ))) ||
		(FAILED(pDevice->SetTextureStageState( uiStageIndex, D3DMTSS_ALPHAARG2, pTexStageSettings->AlphaTexArg2  )))
		)
	{
		OutputDebugString(_T("SetTextureStageState failed."));
		return E_FAIL;
	}

	return S_OK;
}


//
// SetupTexture
//
//   Creates a texture from the specified bitmap, and sets the texture for use in a particular
//   stage.
//
// Arguments:
//  
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying Direct3D Mobile device
//   TCHAR *ptchSrcFile:  Filename of bitmap to load into texture
//   LPDIRECT3DMOBILETEXTURE *ppTexture:  Output for texture interface pointer
//   UINT uiTextureStage:  Stage to associate texture with
//   
// Return Value:
//
//   HRESULT indicates success or failure
//
HRESULT SetupTexture(LPDIRECT3DMOBILEDEVICE pDevice,
                     TCHAR *ptchSrcFile,
                     LPDIRECT3DMOBILETEXTURE *ppTexture,
                     UINT uiTextureStage)
{
	//
	// Create a texture from file
	//

	//
	// Pool for texture creation
	//
	D3DMPOOL TexturePool;

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
	// Pixel format for texture
	//
	D3DMFORMAT DstFormat;
	PIXEL_UNPACK *pDstFormat;

	//
	// Display mode, used to determine a format to use
	//
	D3DMDISPLAYMODE Mode;

	//
	// Direct3D Mobile object
	//
	LPDIRECT3DMOBILE pD3D = NULL;

	//
	// All failure conditions specifically set this to an error
	//
	HRESULT Result = S_OK;

	*ppTexture = NULL;


	if (FAILED(pDevice->GetDirect3D(&pD3D)))
	{
		OutputDebugString(_T("GetDirect3D failed."));
		Result = E_FAIL;
		goto cleanup;
	}

	//
	// Get device capabilities to check for mip-mapping support
	//
	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(_T("GetDeviceCaps failed."));
		Result = E_FAIL;
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
		Result = E_FAIL;
		goto cleanup;
	}


	//
	// Retrieve current display mode
	//
	if (FAILED(pDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
	{
		OutputDebugString(_T("Fatal error at GetDisplayMode."));
		Result = E_FAIL;
		goto cleanup;
	}

	//
	// Attempt to identify a valid Texture format
	//
	for (DstFormat = (D3DMFORMAT)0; DstFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&DstFormat))++)
	{
		
		switch(DstFormat)
		{
		
		case D3DMFMT_R8G8B8:  
		case D3DMFMT_A8R8G8B8:
		case D3DMFMT_X8R8G8B8:
		case D3DMFMT_R5G6B5:  
		case D3DMFMT_X1R5G5B5:
		case D3DMFMT_A1R5G5B5:
		case D3DMFMT_A4R4G4B4:
		case D3DMFMT_R3G3B2:  
		case D3DMFMT_A8R3G3B2:
		case D3DMFMT_X4R4G4B4:

			//
			// Continue iteration; check validity of this format
			//
			break;

		default:

			//
			// Skip iteration for all other formats
			//
			continue;
		}



		if (SUCCEEDED(pD3D->CheckDeviceFormat(Caps.AdapterOrdinal,    // UINT Adapter
		                                      Caps.DeviceType,        // D3DMDEVTYPE DeviceType
		                                      Mode.Format,            // D3DMFORMAT AdapterFormat
		                                      0,                      // ULONG Usage
		                                      D3DMRTYPE_TEXTURE,      // D3DMRESOURCETYPE RType
		                                      DstFormat)))            // D3DMFORMAT CheckFormat
		{
			bFoundValidFormat = TRUE;
			break;
		}
	}

	//
	// Is there a valid texture format?
	//
	if (FALSE == bFoundValidFormat)
	{
		OutputDebugString(_T("Unable to identify a valid D3DMRTYPE_TEXTURE format."));
		Result = E_FAIL;
		goto cleanup;
	}

	//
	// Look up format's characteristics
	//
	if (FAILED(GetPixelFormat(DstFormat, &pDstFormat)))
	{
		OutputDebugString(_T("GetPixelFormat failed."));
		Result = E_FAIL;
		goto cleanup;
	}

	//
	// Retrieve texture from file
	//
	if (FAILED(GetTextureFromFile(ptchSrcFile,     // CONST TCHAR *pszFile,
	                              pDstFormat,      // PIXEL_UNPACK *pDstFormat,
	                              DstFormat,       // D3DMFORMAT d3dFormat,
	                              TexturePool,     // D3DMPOOL d3dPool,
	                              ppTexture,       // LPDIRECT3DMOBILETEXTURE* ppTexture,
	                              pDevice)))       // LPDIRECT3DMOBILEDEVICE pd3dDevice
	{
		OutputDebugString(_T("GetTextureFromFile failed."));
		Result = E_FAIL;
		goto cleanup;
	}

	//
	// Attach the texture to a stage
	//
	if (FAILED(pDevice->SetTexture(uiTextureStage,*ppTexture)))
	{
		OutputDebugString(_T("SetTexture failed."));
		Result = E_FAIL;
		goto cleanup;
	}

cleanup:

	//
	// SetTexture will cause texture interface to retain a reference count as long as it is active
	//
	if (*ppTexture)
	{
		(*ppTexture)->Release();
		(*ppTexture)=NULL;
	}

	//
	// If failed, free remaining texture reference count
	//
	if (FAILED(Result))
		pDevice->SetTexture(uiTextureStage,NULL);

	//
	// If Direct3D Mobile object has been used release the ref count
	//
	if (pD3D)
		pD3D->Release();

	return Result;
}


//
// SetupTexture
//
//   Creates a texture from the specified bitmap, and sets the texture for use in a particular
//   stage.
//
// Arguments:
//  
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying Direct3D Mobile device
//   INT iResourceID:  Resource ID for use with FindResource
//   LPDIRECT3DMOBILETEXTURE *ppTexture:  Output for texture interface pointer
//   UINT uiTextureStage:  Stage to associate texture with
//   
// Return Value:
//
//   HRESULT indicates success or failure
//
HRESULT SetupTexture(LPDIRECT3DMOBILEDEVICE pDevice,
					 INT iResourceID,
                     LPDIRECT3DMOBILETEXTURE *ppTexture,
                     UINT uiTextureStage)
{
	//
	// Pool for texture creation
	//
	D3DMPOOL TexturePool;

	//
	// Pointer to bitmap resource
	//
	BYTE *pBMP;

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
	// Pixel format for texture
	//
	D3DMFORMAT DstFormat;
	PIXEL_UNPACK *pDstFormat;

	//
	// Display mode, used to determine a format to use
	//
	D3DMDISPLAYMODE Mode;

	//
	// Direct3D Mobile object
	//
	LPDIRECT3DMOBILE pD3D = NULL;

	//
	// All failure conditions specifically set this to an error
	//
	HRESULT Result = S_OK;

	*ppTexture = NULL;


	if (FAILED(pDevice->GetDirect3D(&pD3D)))
	{
		OutputDebugString(_T("GetDirect3D failed."));
		Result = E_FAIL;
		goto cleanup;
	}

	//
	// Get device capabilities to check for mip-mapping support
	//
	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(_T("GetDeviceCaps failed."));
		Result = E_FAIL;
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
		Result = E_FAIL;
		goto cleanup;
	}


	//
	// Retrieve current display mode
	//
	if (FAILED(pDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
	{
		OutputDebugString(_T("Fatal error at GetDisplayMode."));
		Result = E_FAIL;
		goto cleanup;
	}

	//
	// Attempt to identify a valid Texture format
	//
	for (DstFormat = (D3DMFORMAT)0; DstFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&DstFormat))++)
	{
		
		switch(DstFormat)
		{
		
		case D3DMFMT_R8G8B8:  
		case D3DMFMT_A8R8G8B8:
		case D3DMFMT_X8R8G8B8:
		case D3DMFMT_R5G6B5:  
		case D3DMFMT_X1R5G5B5:
		case D3DMFMT_A1R5G5B5:
		case D3DMFMT_A4R4G4B4:
		case D3DMFMT_R3G3B2:  
		case D3DMFMT_A8R3G3B2:
		case D3DMFMT_X4R4G4B4:

			//
			// Continue iteration; check validity of this format
			//
			break;

		default:

			//
			// Skip iteration for all other formats
			//
			continue;
		}



		if (SUCCEEDED(pD3D->CheckDeviceFormat(Caps.AdapterOrdinal,    // UINT Adapter
		                                      Caps.DeviceType,        // D3DMDEVTYPE DeviceType
		                                      Mode.Format,            // D3DMFORMAT AdapterFormat
		                                      0,                      // ULONG Usage
		                                      D3DMRTYPE_TEXTURE,      // D3DMRESOURCETYPE RType
		                                      DstFormat)))            // D3DMFORMAT CheckFormat
		{
			bFoundValidFormat = TRUE;
			break;
		}
	}

	//
	// Is there a valid texture format?
	//
	if (FALSE == bFoundValidFormat)
	{
		OutputDebugString(_T("Unable to identify a valid D3DMRTYPE_TEXTURE format."));
		Result = E_FAIL;
		goto cleanup;
	}

	//
	// Look up format's characteristics
	//
	if (FAILED(GetPixelFormat(DstFormat, &pDstFormat)))
	{
		OutputDebugString(_T("GetPixelFormat failed."));
		Result = E_FAIL;
		goto cleanup;
	}

	//
	// Retrieve pointer to bitmap resource
	//
	if (FAILED(GetBitmapResourcePointer(iResourceID, // INT iResourceID,
	                                    &pBMP)))     // PBYTE *ppByte
	{
		OutputDebugString(_T("GetBitmapResourcePointer failed."));
		Result = E_FAIL;
		goto cleanup;
	}

	//
	// Retrieve texture from memory
	//
	if (FAILED(GetTextureFromMem(pBMP,         // BYTE *pBMP,
	                             pDstFormat,   // PIXEL_UNPACK *pDstFormat,
	                             DstFormat,    // D3DMFORMAT d3dFormat,
	                             TexturePool,  // D3DMPOOL d3dPool,
	                             ppTexture,    // IDirect3DMobileTexture** ppTexture,
	                             pDevice)))    // IDirect3DMobileDevice* pd3dDevice
	{
		OutputDebugString(_T("GetTextureFromFile failed."));
		Result = E_FAIL;
		goto cleanup;
	}

	//
	// Attach the texture to a stage
	//
	if (FAILED(pDevice->SetTexture(uiTextureStage,*ppTexture)))
	{
		OutputDebugString(_T("SetTexture failed."));
		Result = E_FAIL;
		goto cleanup;
	}

cleanup:

	//
	// SetTexture will cause texture interface to retain a reference count as long as it is active
	//
	if (*ppTexture)
	{
		(*ppTexture)->Release();
		(*ppTexture)=NULL;
	}

	//
	// If failed, free remaining texture reference count
	//
	if (FAILED(Result))
		pDevice->SetTexture(uiTextureStage,NULL);

	//
	// If Direct3D Mobile object has been used release the ref count
	//
	if (pD3D)
		pD3D->Release();

	return Result;
}

