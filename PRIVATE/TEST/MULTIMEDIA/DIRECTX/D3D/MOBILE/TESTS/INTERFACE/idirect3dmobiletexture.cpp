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
#include "IDirect3DMobileTexture.h"
#include "DebugOutput.h"
#include "KatoUtils.h"
#include "BufferTools.h"
#include <tux.h>
#include <tchar.h>
#include <stdio.h>

//
// GUID for invalid parameter tests
//
DEFINE_GUID(IID_D3DQAInvalidInterface, 
0x00000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

//
// Default extents for texture creation
//
#define D3DQA_DEFAULT_TEX_WIDTH 64
#define D3DQA_DEFAULT_TEX_HEIGHT 64

//
// Default extents for mip-mapped texture creation
//
#define D3DQA_DEFAULT_MIPMAP_WIDTH 64
#define D3DQA_DEFAULT_MIPMAP_HEIGHT 64
#define D3DQA_DEFAULT_MIPMAP_LEVELS 7 // # levels for above dimensions

//
// Maximum priority level tested for Set/GetPriority
//
#define D3DQA_MAX_TEX_PRIORITY 512

//
//  TextureTest
//
//    Constructor for TextureTest class; however, most initialization is deferred.
//
//  Arguments:
//
//    none
//
//  Return Value:
//
//    none
//
TextureTest::TextureTest()
{
	m_bInitSuccess = FALSE;
}

//
// TextureTest
//
//   Initialize TextureTest object
//
// Arguments:
//
//   UINT uiAdapterOrdinal: D3DM Adapter ordinal
//   WCHAR *pSoftwareDevice: Pointer to wide character string identifying a D3DM software device to instantiate
//   BOOL bDebug: Running over debug middleware?
//   UINT uiTestCase:  Test case ordinal (to be displayed in window)
//
// Return Value
//
//   HRESULT indicates success or failure
//
HRESULT TextureTest::Init(LPTEST_CASE_ARGS pTestCaseArgs, LPWINDOW_ARGS pWindowArgs, BOOL bDebug, UINT uiTestCase)
{
	HRESULT hr;

	if (FAILED(hr = D3DMInitializer::Init(D3DQA_PURPOSE_RAW_TEST,                      // D3DQA_PURPOSE Purpose
	                                 pWindowArgs,                                 // LPWINDOW_ARGS pWindowArgs
	                                 pTestCaseArgs->pwchSoftwareDeviceFilename,   // TCHAR *ptchDriver
	                                 uiTestCase)))                                // UINT uiTestCase
	{
		DebugOut(DO_ERROR, L"Aborting initialization, due to prior initialization failure. (hr = 0x%08x)", hr);
		return E_FAIL;
	}

	m_bDebugMiddleware = bDebug;

	m_bInitSuccess = TRUE;
	return S_OK;
}


//
//  IsReady
//
//    Accessor method for "initialization state" of TextureTest object.  "Half
//    initialized" states should not occur.
//  
//  Return Value:
//  
//    BOOL:  TRUE if the object is initialized; FALSE if it is not.
//
BOOL TextureTest::IsReady()
{
	if (FALSE == m_bInitSuccess)
	{
		DebugOut(DO_ERROR, L"TextureTest is not ready.");
		return FALSE;
	}

	DebugOut(L"TextureTest is ready.");
	return D3DMInitializer::IsReady();
}

// 
// ~TextureTest
//
//  Destructor for TextureTest.  Currently; there is nothing
//  to do here.
//
TextureTest::~TextureTest()
{
	return; // Success
}

//
// SupportsTextures
//
//   Verify that the device supports textures
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  HRESULT:  success indicates texture support; failure otherwise
//
HRESULT TextureTest::SupportsTextures()     
{
	HRESULT hr;

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"No active device to query.");
		return E_FAIL;
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x)", hr);
		return E_FAIL;
	}
    
	//
	// Does the device support texture mapping?
	//
	if (D3DMDEVCAPS_TEXTURE & Caps.DevCaps)
	{
		return S_OK;
	}
	else
	{
		return E_FAIL;
	}
}

//
// SupportsManagedPool
//
//   Verify that the device supports the managed pool
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  HRESULT:  success indicates texture support; failure otherwise
//
HRESULT TextureTest::SupportsManagedPool()     
{
	HRESULT hr;

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"No active device to query.");
		return E_FAIL;
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x)", hr);
		return hr;
	}
    
	//
	// Does the device support managed pool?
	//
	if (D3DMSURFCAPS_MANAGEDPOOL & Caps.SurfaceCaps)
	{
		return S_OK;
	}
	else
	{
		return E_FAIL;
	}
}


//
// SupportsLockableTexture
//
//   Verify that the device supports lockable textures
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  HRESULT:  success indicates texture support; failure otherwise
//
HRESULT TextureTest::SupportsLockableTexture()     
{
	HRESULT hr;

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"No active device to query.");
		return E_FAIL;
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x)", hr);
		return hr;
	}
    
	//
	// Does the device support lockable textures?
	//
	if (D3DMSURFCAPS_LOCKTEXTURE & Caps.SurfaceCaps)
	{
		return S_OK;
	}
	else
	{
		return E_FAIL;
	}
}

//
// CreateSimpleTexture
//
//   Creates a texture of a supported format.  Creates in either system or 
//   video memory, whichever is supported (if both are supported the texture
//   is created in system memory).  Creates the texture in dimensions that 
//   are valid even under all of the various possible driver constraints such as:
//
//      * D3DMPTEXTURECAPS_POW2
//      * D3DMPTEXTURECAPS_SQUAREONLY
//      * MaxTextureAspectRatio
//
//   This function assumes that the driver supports a reasonable MaxTextureWidth
//   and MaxTextureHeight.
//
//   Caller assumes the responsibility of checking for texture support prior to
//   calling this function.
//
// Arguments:
//  
//  BOOL bLockable:  Should texture be created as lockable?
//
// Return Value:
//
//  HRESULT:  success indicates texture created; failure otherwise
//
HRESULT TextureTest::CreateSimpleTexture(IDirect3DMobileTexture **ppTexture, BOOL bLockable)
{
	HRESULT hr;

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	//
	// Pool for texture creation, according to current iteration
	//
	D3DMPOOL TexturePool;

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x)", hr);
		return E_FAIL;
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
		DebugOut(DO_ERROR, L"D3DMCAPS.SurfaceCaps:  No valid texture pool.");
		return E_FAIL;
	}

	//
	// If texture is supposed to be set up to be lockable, ensure support
	//
	if (bLockable)
	{
		if (!(D3DMSURFCAPS_LOCKTEXTURE & Caps.SurfaceCaps))
		{
			DebugOut(DO_ERROR, L"D3DMCAPS.SurfaceCaps:  No support for D3DMSURFCAPS_LOCKTEXTURE.");
			return E_FAIL;
		}
	}

	return CreateAutoFormatTexture(ppTexture, TexturePool, bLockable);
}

//
// CreateSimpleMipMap
//
//   Creates a mip-mapped texture of a supported format.  Creates in either system 
//   or video memory, whichever is supported (if both are supported the texture
//   is created in system memory).  Creates the texture in dimensions that 
//   are valid even under all of the various possible driver constraints such as:
//
//      * D3DMPTEXTURECAPS_POW2
//      * D3DMPTEXTURECAPS_SQUAREONLY
//      * MaxTextureAspectRatio
//
//   This function assumes that the driver supports a reasonable MaxTextureWidth
//   and MaxTextureHeight.
//
//   Caller assumes the responsibility of checking for mip-mapped texture support
//   prior to calling this function.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  HRESULT:  success indicates texture created; failure otherwise
//
HRESULT TextureTest::CreateSimpleMipMap(IDirect3DMobileTexture **ppTexture, BOOL bLockable)
{
	HRESULT hr;

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	//
	// Pool for texture creation, according to current iteration
	//
	D3DMPOOL TexturePool;

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x)", hr);
		return E_FAIL;
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
		DebugOut(DO_ERROR, L"D3DMCAPS.SurfaceCaps:  No valid texture pool.");
		return E_FAIL;
	}

	return CreateAutoFormatMipMap(ppTexture, TexturePool, bLockable);
}

//
// CreateManagedTexture
//
//   Creates a texture of a supported format.  Creates in managed pool.
//   Creates the texture in dimensions that are valid even under all of
//   the various possible driver constraints such as:
//
//      * D3DMPTEXTURECAPS_POW2
//      * D3DMPTEXTURECAPS_SQUAREONLY
//      * MaxTextureAspectRatio
//
//   This function assumes that the driver supports a reasonable MaxTextureWidth
//   and MaxTextureHeight.
//
//   Caller assumes the responsibility of checking for texture support, and
//   managed pool support, prior to calling this function.
//
// Arguments:
//  
//  BOOL bLockable:  Should texture be created as lockable?
//
// Return Value:
//
//  HRESULT:  success indicates texture created; failure otherwise
//
HRESULT TextureTest::CreateManagedTexture(IDirect3DMobileTexture **ppTexture, BOOL bLockable)
{
	HRESULT hr;

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x)", hr);
		return E_FAIL;
	}

	//
	// If texture is supposed to be set up to be lockable, ensure support
	//
	if (bLockable)
	{
		if (!(D3DMSURFCAPS_LOCKTEXTURE & Caps.SurfaceCaps))
		{
			DebugOut(DO_ERROR, L"D3DMCAPS.SurfaceCaps:  No support for D3DMSURFCAPS_LOCKTEXTURE.");
			return E_FAIL;
		}
	}
	
	return CreateAutoFormatTexture(ppTexture, D3DMPOOL_MANAGED, bLockable);
}

//
// CreateAutoFormatTexture
//
//   Creates a texture of a supported format, of dimensions that are valid even
//   under all of the various possible driver constraints such as:
//
//      * D3DMPTEXTURECAPS_POW2
//      * D3DMPTEXTURECAPS_SQUAREONLY
//      * MaxTextureAspectRatio
//
//   This function assumes that the driver supports a reasonable MaxTextureWidth
//   and MaxTextureHeight.
//
//   Caller assumes the responsibility of checking for texture support prior to
//   calling this function.
//
// Arguments:
//  
//    BOOL bLockable:  Should texture be created as lockable?
//
// Return Value:
//
//  HRESULT:  success indicates texture created; failure otherwise
//
HRESULT TextureTest::CreateAutoFormatTexture(IDirect3DMobileTexture **ppTexture, D3DMPOOL TexturePool, BOOL bLockable)
{
	HRESULT hr;

	//
	// The display mode's spatial resolution and color resolution
	//
	D3DMDISPLAYMODE Mode;

	//
	// There must be at least one valid texture format; caller assumes
	// the responsibility of verifying texture support prior to calling
	// this function.
	//
	BOOL bFoundValidFormat = FALSE;

	//
	// D3DMFORMAT iterator; to cycle through formats until a valid format
	// is found
	//
	D3DMFORMAT CurrentFormat;

	//
	// Surface usage
	// 
	DWORD dwUsage;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"No valid device.");
		return E_FAIL;
	}

	//
	// Parameter validation
	//
	if (NULL == ppTexture)
	{
		DebugOut(DO_ERROR, L"Failing due to NULL IDirect3DMobileTexture**.");
		return E_FAIL;
	}

	//
	// Retrieve current display mode
	//
	if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode))) // D3DMDISPLAYMODE* pMode
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode; function returned failure. (hr = 0x%08x)", hr);
		return E_FAIL;
	}

	//
	// Find a surface format that the device is capable of creating
	//
	for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
	{
		if (!(FAILED(hr = m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal,     // UINT Adapter
		                                       m_DevType,              // D3DMDEVTYPE DeviceType
		                                       Mode.Format,            // D3DMFORMAT AdapterFormat
		                                       D3DMUSAGE_RENDERTARGET, // ULONG Usage
		                                       D3DMRTYPE_TEXTURE,      // D3DMRESOURCETYPE RType
		                                       CurrentFormat))))       // D3DMFORMAT CheckFormat
		{
			bFoundValidFormat = TRUE;
			break;
		}
	}

	//
	// At least one texture format must be supported
	//
	if (FALSE == bFoundValidFormat)
	{
		DebugOut(DO_ERROR, L"Fatal error; CheckDeviceFormat failed for every format.");
		return E_FAIL;
	}

	//
	// Need to make this a render target, since it may be used as ColorFill target, among other things
	//
	dwUsage = D3DMUSAGE_RENDERTARGET;

	//
	// This texture, potentially, may need to be lockable
	//
	if (bLockable)
		dwUsage |= D3DMUSAGE_LOCKABLE;

	if (FAILED(hr = m_pd3dDevice->CreateTexture( D3DQA_DEFAULT_TEX_WIDTH,            // UINT Width,
	                                        D3DQA_DEFAULT_TEX_HEIGHT,           // UINT Height,
	                                        1,                                  // UINT Levels,
	                                        dwUsage,                            // DWORD Usage,
	                                        CurrentFormat,                      // D3DMFORMAT Format,
	                                        TexturePool,                        // D3DMPOOL Pool,
	                                        ppTexture)))                        // IDirect3DMobileTexture** ppTexture
	{
		DebugOut(DO_ERROR, L"CreateTexture failed. (hr = 0x%08x)", hr);
		return E_FAIL;
	}

	return S_OK;
}

//
// CreateAutoFormatMipMap
//
//   Creates a mip-mapped texture of a supported format, of dimensions 
//   that are valid even under all of the various possible driver constraints 
//   such as:
//
//      * D3DMPTEXTURECAPS_POW2
//      * D3DMPTEXTURECAPS_SQUAREONLY
//      * MaxTextureAspectRatio
//
//   This function assumes that the driver supports a reasonable MaxTextureWidth
//   and MaxTextureHeight.
//
//   Caller assumes the responsibility of checking for mip-mapped texture support
//   prior to calling this function.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  HRESULT:  success indicates texture created; failure otherwise
//
HRESULT TextureTest::CreateAutoFormatMipMap(IDirect3DMobileTexture **ppTexture, D3DMPOOL TexturePool, BOOL bLockable)
{
	HRESULT hr;

	//
	// The display mode's spatial resolution and color resolution
	//
	D3DMDISPLAYMODE Mode;

	//
	// There must be at least one valid texture format; caller assumes
	// the responsibility of verifying texture support prior to calling
	// this function.
	//
	BOOL bFoundValidFormat = FALSE;

	//
	// D3DMFORMAT iterator; to cycle through formats until a valid format
	// is found
	//
	D3DMFORMAT CurrentFormat;

	//
	// Surface usage
	// 
	DWORD dwUsage;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"No valid device.");
		return E_FAIL;
	}

	//
	// Parameter validation
	//
	if (NULL == ppTexture)
	{
		DebugOut(DO_ERROR, L"Failing due to NULL IDirect3DMobileTexture**.");
		return E_FAIL;
	}

	//
	// Retrieve current display mode
	//
	if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode))) // D3DMDISPLAYMODE* pMode
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode; function returned failure. (hr = 0x%08x)", hr);
		return hr;
	}

	//
	// Find a surface format that the device is capable of creating
	//
	for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
	{
		if (!(FAILED(hr = m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal,     // UINT Adapter
		                                       m_DevType,              // D3DMDEVTYPE DeviceType
		                                       Mode.Format,            // D3DMFORMAT AdapterFormat
		                                       D3DMUSAGE_RENDERTARGET, // ULONG Usage
		                                       D3DMRTYPE_TEXTURE,      // D3DMRESOURCETYPE RType
		                                       CurrentFormat))))       // D3DMFORMAT CheckFormat
		{
			bFoundValidFormat = TRUE;
			break;
		}
	}

	//
	// At least one texture format must be supported
	//
	if (FALSE == bFoundValidFormat)
	{
		DebugOut(DO_ERROR, L"Fatal error; CheckDeviceFormat failed for every format.");
		return E_FAIL;
	}

	//
	// Need to make this a render target, since it may be used as ColorFill target, among other things
	//
	dwUsage = D3DMUSAGE_RENDERTARGET;

	//
	// This texture, potentially, may need to be lockable
	//
	if (bLockable)
		dwUsage |= D3DMUSAGE_LOCKABLE;

	//
	// Levels == 0 implies automatically generated level count
	//
	if (FAILED(hr = m_pd3dDevice->CreateTexture( D3DQA_DEFAULT_MIPMAP_WIDTH,         // UINT Width,
	                                        D3DQA_DEFAULT_MIPMAP_HEIGHT,        // UINT Height,
	                                        0,                                  // UINT Levels,
	                                        dwUsage,                            // DWORD Usage,
	                                        CurrentFormat,                      // D3DMFORMAT Format,
	                                        TexturePool,                        // D3DMPOOL Pool,
	                                        ppTexture)))                        // IDirect3DMobileTexture** ppTexture
	{
		DebugOut(DO_ERROR, L"CreateTexture failed. (hr = 0x%08x)", hr);
		return E_FAIL;
	}

	return S_OK;
}

//
// CreateManagedMipMap
//
//   Creates a mip-mapped texture of a supported format, of dimensions 
//   that are valid even under all of the various possible driver constraints 
//   such as:
//
//      * D3DMPTEXTURECAPS_POW2
//      * D3DMPTEXTURECAPS_SQUAREONLY
//      * MaxTextureAspectRatio
//
//   This function assumes that the driver supports a reasonable MaxTextureWidth
//   and MaxTextureHeight.
//
//   Caller assumes the responsibility of checking for mip-mapped texture support
//   prior to calling this function.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  HRESULT:  success indicates texture created; failure otherwise
//
HRESULT TextureTest::CreateManagedMipMap(IDirect3DMobileTexture **ppTexture)
{
	return CreateAutoFormatMipMap(ppTexture, D3DMPOOL_MANAGED, FALSE);
}


//
// SupportsMipMap
//
//   Verify that the device supports mip mapping
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  HRESULT:  success indicates texture support; failure otherwise
//
HRESULT TextureTest::SupportsMipMap()     
{
	HRESULT hr;

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"No active device to query.");
		return E_FAIL;
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x)", hr);
		return E_FAIL;
	}

	//
	// Does the device support mip mapping?
	//
	if (D3DMPTEXTURECAPS_MIPMAP & Caps.TextureCaps)
	{
		return S_OK;
	}
	else
	{
		return E_FAIL;
	}
}

//
// ExecuteQueryInterfaceTest
//
//   Verify IDirect3DMobileTexture::QueryInterface
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteQueryInterfaceTest()     
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteQueryInterfaceTest.");

	//
	// Texture interface to be manipulated during this test case
	//
	IDirect3DMobileTexture *pTexture;

	//
	// Interface pointers to be retrieved by QueryInterface
	//
	IDirect3DMobileTexture *pQITexture;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Before testing texture interface, verify that device supports texture mapping
	//
	if (FAILED(hr = SupportsTextures()))
	{
		DebugOut(DO_ERROR, L"Device does not support texture mapping. (hr = 0x%08x) Skipping.", hr);
		return TPR_SKIP;
	}

	if (FAILED(hr = CreateSimpleTexture(&pTexture, FALSE))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}


	// 	
	// Bad parameter tests
	// 	

	//
	// Test case #1:  Invalid REFIID
	//
	if (SUCCEEDED(hr = pTexture->QueryInterface(IID_D3DQAInvalidInterface, // REFIID riid
	                                       (void**)(&pQITexture)))) // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
		pTexture->Release();
		return TPR_FAIL;
	}

	//
	// Test case #2:  Valid REFIID, invalid PPVOID
	//
	if (SUCCEEDED(hr = pTexture->QueryInterface(IID_IDirect3DMobileTexture, // REFIID riid
	                                       (void**)0)))              // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
		pTexture->Release();
		return TPR_FAIL;
	}

	//
	// Test case #3:  Invalid REFIID, invalid PPVOID
	//
	if (SUCCEEDED(hr = pTexture->QueryInterface(IID_D3DQAInvalidInterface, // REFIID riid
	                                       (void**)0)))               // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
		pTexture->Release();
		return TPR_FAIL;
	}


	//
	// Valid parameter tests
	//

	// 	
	// Perform an query on the surface interface
	// 	
	if (FAILED(hr = pTexture->QueryInterface(IID_IDirect3DMobileTexture,  // REFIID riid
	                                    (void**)(&pQITexture))))     // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface failed. (hr = 0x%08x). Failing.", hr);
		pTexture->Release();
		return TPR_FAIL;
	}


	//
	// Release the original interface pointer; verify that the ref count is still non-zero
	//
	if (!(pTexture->Release()))
	{
		DebugOut(DO_ERROR, L"Reference count for IDirect3DMobileTexture dropped to zero; unexpected. Failing.");
		return TPR_FAIL;
	}

	//
	// Release the IDirect3DMobileTexture interface pointer; ref count should now drop to zero
	//
	if (pQITexture->Release())
	{
		DebugOut(DO_ERROR, L"Reference count for IDirect3DMobileTexture is non-zero; unexpected. Failing.");
		return TPR_FAIL;
	}


	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteAddRefTest
//
//   Verify IDirect3DMobileTexture::AddRef
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteAddRefTest()     
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteAddRefTest.");

	//
	// Texture interface to be manipulated during this test case
	//
	IDirect3DMobileTexture *pTexture;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Before testing texture interface, verify that device supports texture mapping
	//
	if (FAILED(hr = SupportsTextures()))
	{
		DebugOut(DO_ERROR, L"Device does not support texture mapping. (hr = 0x%08x) Skipping.", hr);
		return TPR_SKIP;
	}
    
	if (FAILED(hr = CreateSimpleTexture(&pTexture, FALSE))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}
    	
	//
	// Verify ref-count values; via AddRef/Release return values
	//
	if (2 != pTexture->AddRef())
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileTexture: unexpected reference count. Failing.");
		return TPR_FAIL;
	}
	
	if (1 != pTexture->Release())
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileTexture: unexpected reference count. Failing.");
		return TPR_FAIL;
	}
	
	if (0 != pTexture->Release())
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileTexture: unexpected reference count. Failing.");
		return TPR_FAIL;
	}

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteReleaseTest
//
//   Verify IDirect3DMobileTexture::Release
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteReleaseTest()     
{
	DebugOut(L"Beginning ExecuteReleaseTest.");

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Testing for this method occurs in a closely-related test:
	//
	return ExecuteAddRefTest();
}

//
// ExecuteGetDeviceTest
//
//   Verify IDirect3DMobileTexture::GetDevice
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteGetDeviceTest()     
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteGetDeviceTest.");

	//
	// Device interface pointer to retrieve with GetDevice
	//
	IDirect3DMobileDevice* pDevice;

	//
	// Texture interface to be manipulated during this test case
	//
	IDirect3DMobileTexture *pTexture;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Before testing texture interface, verify that device supports texture mapping
	//
	if (FAILED(hr = SupportsTextures()))
	{
		DebugOut(DO_ERROR, L"Device does not support texture mapping. (hr = 0x%08x) Skipping.", hr);
		return TPR_SKIP;
	}
    
	if (FAILED(hr = CreateSimpleTexture(&pTexture, FALSE))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	//
	// Retrieve the device interface pointer
	//
	if (FAILED(hr = pTexture->GetDevice(&pDevice))) // IDirect3DMobileDevice** ppDevice
	{
		DebugOut(DO_ERROR, L"GetDevice failed. (hr = 0x%08x). Failing.", hr);
		pTexture->Release();
		return TPR_FAIL;
	}

	//
	// Verify that the device interface pointer is consistent with expectations
	//
	if (pDevice != m_pd3dDevice)
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice interface pointer mismatch. Failing.");
		pTexture->Release();
		return TPR_FAIL;
	}

	//
	// Release the device interface reference (incremented by GetDevice)
	//
	pDevice->Release();

	//
	// Release the interface
	//
	pTexture->Release();
	
	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteSetPriorityTest
//
//   Verify IDirect3DMobileTexture::SetPriority
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteSetPriorityTest()     
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteSetPriorityTest.");

	//
	// Texture interface to be manipulated during this test case
	//
	IDirect3DMobileTexture *pTexture;

	//
	// Priority iterator for values passed to SetPriority
	//
	UINT uiPriority;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Before testing texture interface, verify that device supports texture mapping
	//
	if (FAILED(hr = SupportsTextures()))
	{
		DebugOut(DO_ERROR, L"Device does not support texture mapping. (hr = 0x%08x) Skipping.", hr);
		return TPR_SKIP;
	}

	//
	// This test requires managed pool resource support
	//
	if (FAILED(hr = SupportsManagedPool()))
	{
		//
		// Skip most of test
		//
		if (FAILED(hr = CreateSimpleTexture(&pTexture, FALSE))) // IDirect3DMobileTexture **ppTexture
		{
			DebugOut(DO_ERROR, L"CreateSimpleTexture failed. (hr = 0x%08x). Failing.", hr);
			return TPR_FAIL;
		}

		pTexture->SetPriority(1);
		pTexture->GetPriority();

		pTexture->Release();

		DebugOut(DO_ERROR, L"SetPriority:  Skipping test because driver does not expose D3DMSURFCAPS_MANAGEDPOOL. Skipping.");
		return TPR_SKIP;

	}
    
	if (FAILED(hr = CreateManagedTexture(&pTexture, FALSE))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	// 
	// Verify default priority of zero; arg is arbitrary (as long as it is non-zero).
	// This caller simply verifies that SetPriority returns the previous priority, and
	// that the previous priority defaults to zero if it has never been explicitely set.
	// 
	if (0 != pTexture->SetPriority(1))
	{
		DebugOut(DO_ERROR, L"SetPriority failed. Failing.");
		pTexture->Release();
		return TPR_FAIL;
	}

	//
	// Iterate through a large set of priorities, and verify that SetPriority always
	// returns the previously-set priority
	//
	pTexture->SetPriority(0);
	for(uiPriority = 1; uiPriority < D3DQA_MAX_TEX_PRIORITY; uiPriority++)
	{
		if ((uiPriority-1) != pTexture->SetPriority(uiPriority))
		{
			DebugOut(DO_ERROR, L"SetPriority failed. Failing.");
			pTexture->Release();
			return TPR_FAIL;
		}

		if (uiPriority != pTexture->GetPriority())
		{
			DebugOut(DO_ERROR, L"GetPriority priority mismatch. Failing.");
			pTexture->Release();
			return TPR_FAIL;
		}
	
	}

	pTexture->Release();
	
	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteGetPriorityTest
//
//   Verify IDirect3DMobileTexture::GetPriority
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteGetPriorityTest()     
{
	DebugOut(L"Beginning ExecuteGetPriorityTest.");

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// GetPriority testing is in combination with SetPriority testing; redirect this test
	// case accordingly
	//
	return ExecuteSetPriorityTest();
}

//
// ExecutePreLoadTest
//
//   Verify IDirect3DMobileTexture::PreLoad
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecutePreLoadTest()     
{
	HRESULT hr;

	DebugOut(L"Beginning ExecutePreLoadTest.");

	//
	// Texture interface to be manipulated during this test case
	//
	IDirect3DMobileTexture *pTexture;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Before testing texture interface, verify that device supports texture mapping
	//
	if (FAILED(hr = SupportsTextures()))
	{
		DebugOut(DO_ERROR, L"Device does not support texture mapping. (hr = 0x%08x) Skipping.", hr);
		return TPR_SKIP;
	}
    
	//
	// This test requires managed pool resource support
	//
	if (FAILED(hr = SupportsManagedPool()))
	{
		//
		// Skip most of test
		//
		if (FAILED(hr = CreateSimpleTexture(&pTexture, FALSE))) // IDirect3DMobileTexture **ppTexture
		{
			DebugOut(DO_ERROR, L"CreateSimpleTexture failed. (hr = 0x%08x). Failing.", hr);
			return TPR_FAIL;
		}

		pTexture->PreLoad();

		pTexture->Release();

		DebugOut(DO_ERROR, L"PreLoad:  Skipping test because driver does not expose D3DMSURFCAPS_MANAGEDPOOL. Skipping.");
		return TPR_SKIP;
	}

	if (FAILED(hr = CreateManagedTexture(&pTexture, FALSE))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}
	
	pTexture->PreLoad();
	
	pTexture->Release();
	
	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteGetTypeTest
//
//   Verify IDirect3DMobileTexture::GetType
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteGetTypeTest()     
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteGetTypeTest.");

	//
	// Texture interface to be manipulated during this test case
	//
	IDirect3DMobileTexture *pTexture;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Before testing texture interface, verify that device supports texture mapping
	//
	if (FAILED(hr = SupportsTextures()))
	{
		DebugOut(DO_ERROR, L"Device does not support texture mapping. (hr = 0x%08x) Skipping.", hr);
		return TPR_SKIP;
	}
    
	if (FAILED(hr = CreateSimpleTexture(&pTexture, FALSE))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}
	

	if (D3DMRTYPE_TEXTURE != pTexture->GetType())
	{
		DebugOut(DO_ERROR, L"D3DMRESOURCETYPE mismatch; IDirect3DMobileTexture::GetType. Failing.");
		pTexture->Release();
		return TPR_FAIL;
	}

	pTexture->Release();
	
	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;	
}

//
// ExecuteSetLODTest
//
//   Verify IDirect3DMobileTexture::SetLOD
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteSetLODTest()     
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteSetLODTest.");

	//
	// Texture interface to be manipulated during this test case
	//
	IDirect3DMobileTexture *pTexture;

	//
	// Mip-Map Level Count
	//
	DWORD dwLevelCount;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Before testing texture interface, verify that device supports texture mapping
	//
	if (FAILED(hr = SupportsTextures()))
	{
		DebugOut(DO_ERROR, L"Device does not support texture mapping. (hr = 0x%08x) Skipping.", hr);
		return TPR_SKIP;
	}

	//
	// Before testing mip mapping, verify support
	//
	if (FAILED(hr = SupportsMipMap()))
	{
		DebugOut(DO_ERROR, L"Device does not support mip mapping. (hr = 0x%08x) Skipping.", hr);
		return TPR_SKIP;
	}
    
	//
	// Create a mip-map in either system or video memory, depending on device capabilities
	//
	if (FAILED(hr = CreateSimpleMipMap(&pTexture, FALSE))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}
	
	//
	// GetLOD is used for LOD control of managed textures. This method returns
	// 0 on nonmanaged textures.
	//
	if (0 != pTexture->GetLOD())
	{
		DebugOut(DO_ERROR, L"GetLOD on non-managed texture should always return 0. Failing.");
		pTexture->Release();
		return TPR_FAIL;
	}

	pTexture->SetLOD(0xFFFFFFFF);
	pTexture->SetLOD(0);

	pTexture->Release();

	//
	// Before testing mip mapping, verify support
	//
	if (FAILED(hr = SupportsManagedPool()))
	{
		DebugOut(DO_ERROR, L"Device does not support texture mapping. (hr = 0x%08x) Skipping.", hr);
		return TPR_SKIP;
	}
    
	//
	// Create a mip-map in either system or video memory, depending on device capabilities
	//
	if (FAILED(hr = CreateManagedMipMap(&pTexture))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		pTexture->Release();
		return TPR_FAIL;
	}

	dwLevelCount = pTexture->GetLevelCount();

	//
	// GetLOD returns a DWORD value, clamped to the maximum LOD value (one less than the
	// total number of levels).
	// 
	if ((dwLevelCount-1) != pTexture->GetLOD())
	{
		DebugOut(DO_ERROR, L"GetLOD: unexpected result. Failing.");
		pTexture->Release();
		return TPR_FAIL;
	}

	//
	// Verify SetLOD clamping
	// 
	if ((dwLevelCount-1) != pTexture->SetLOD(dwLevelCount+1))
	{
		DebugOut(DO_ERROR, L"SetLOD: unexpected result. Failing.");
		pTexture->Release();
		return TPR_FAIL;
	}

	//
	// Verify SetLOD clamping
	// 
	if ((dwLevelCount-1) != pTexture->SetLOD(dwLevelCount+2))
	{
		DebugOut(DO_ERROR, L"SetLOD: unexpected result. Failing.");
		pTexture->Release();
		return TPR_FAIL;
	}

	//
	// Verify SetLOD clamping
	// 
	if ((dwLevelCount-1) != pTexture->SetLOD(dwLevelCount+3))
	{
		DebugOut(DO_ERROR, L"SetLOD: unexpected result. Failing.");
		pTexture->Release();
		return TPR_FAIL;
	}

	pTexture->Release();

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteGetLODTest
//
//   Verify IDirect3DMobileTexture::GetLOD
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteGetLODTest()     
{
	DebugOut(L"Beginning ExecuteGetLODTest.");

	//
	// Testing for this method occurs in a closely-related test:
	//
	return ExecuteSetLODTest();
}

//
// ExecuteGetLevelCountTest
//
//   Verify IDirect3DMobileTexture::GetLevelCount
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteGetLevelCountTest()     
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteGetLevelCountTest.");

	//
	// Texture interface to be manipulated during this test case
	//
	IDirect3DMobileTexture *pTexture;

	//
	// Mip-Map Level Count
	//
	DWORD dwLevelCount;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Before testing texture interface, verify that device supports texture mapping
	//
	if (FAILED(hr = SupportsTextures()))
	{
		DebugOut(DO_ERROR, L"Device does not support texture mapping. (hr = 0x%08x) Skipping.", hr);
		return TPR_SKIP;
	}

	//
	// Before testing mip mapping, verify support
	//
	if (FAILED(hr = SupportsMipMap()))
	{
		DebugOut(DO_ERROR, L"Device does not support mip mapping. (hr = 0x%08x) Skipping.", hr);
		return TPR_SKIP;
	}
    
	//
	// Create a mip-map in either system or video memory, depending on device capabilities
	//
	if (FAILED(hr = CreateSimpleMipMap(&pTexture, FALSE))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	dwLevelCount = pTexture->GetLevelCount();

	pTexture->Release();

	//
	// Verify that automatic level count is "as expected"
	//
	if (dwLevelCount != D3DQA_DEFAULT_MIPMAP_LEVELS)
	{
		DebugOut(DO_ERROR, L"GetLevelCount returned unexpected value. Failing.");
		return TPR_FAIL;
	}

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;

}

//
// ExecuteGetLevelDescTest
//
//   Verify IDirect3DMobileTexture::GetLevelDesc
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteGetLevelDescTest()     
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteGetLevelDescTest.");

	//
	// Texture interface to be manipulated during this test case
	//
	IDirect3DMobileTexture *pTexture;

	//
	// Surface description to retrieve from texture
	//
	D3DMSURFACE_DESC SurfaceDesc;

	//
	// Iterator for walking through mip-map levels; inspecting surface descriptions
	//
	UINT uiLevelIter;

	//
	// Mip-Map Level Count
	//
	DWORD dwLevelCount;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Before testing texture interface, verify that device supports texture mapping
	//
	if (FAILED(hr = SupportsTextures()))
	{
		DebugOut(DO_ERROR, L"Device does not support texture mapping. (hr = 0x%08x) Skipping.", hr);
		return TPR_SKIP;
	}

	//
	// Before testing mip mapping, verify support
	//
	if (FAILED(hr = SupportsMipMap()))
	{
		DebugOut(DO_ERROR, L"Device does not support mip mapping. (hr = 0x%08x) Skipping.", hr);
		return TPR_SKIP;
	}
    
	//
	// Create a mip-map in either system or video memory, depending on device capabilities
	//
	if (FAILED(hr = CreateSimpleMipMap(&pTexture, FALSE))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	dwLevelCount = pTexture->GetLevelCount();

	//
	// Loop through each level; getting the level's description
	//
	for (uiLevelIter = 0; uiLevelIter < dwLevelCount; uiLevelIter++)
	{

		if (FAILED(hr = pTexture->GetLevelDesc(uiLevelIter, &SurfaceDesc)))
		{
			DebugOut(DO_ERROR, L"GetLevelDesc failed for a valid level. (hr = 0x%08x). Failing.", hr);
			pTexture->Release();
			return TPR_FAIL;
		}
			
		//
		// Does the format match the expectation?
		//
		if (D3DMFMT_UNKNOWN == SurfaceDesc.Format)
		{
			DebugOut(DO_ERROR, L"D3DMSURFACE_DESC.Format set to invalid value. Failing.");
			pTexture->Release();
			return TPR_FAIL;
		}

		//
		// Does the resource type match the expectation?
		//
		if (D3DMRTYPE_TEXTURE != SurfaceDesc.Type)
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMSURFACE_DESC.Type. Failing.");
			pTexture->Release();
			return TPR_FAIL;
		}

		//
		// Does the usage mask match the expectation?
		//
		if ((D3DMUSAGE_TEXTURE | D3DMUSAGE_RENDERTARGET) != SurfaceDesc.Usage)
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMSURFACE_DESC.Usage. Failing.");
			pTexture->Release();
			return TPR_FAIL;
		}

		//
		// Does the pool type match the expectation?
		//
		if (!((D3DMPOOL_SYSTEMMEM == SurfaceDesc.Pool) ||
		      (D3DMPOOL_VIDEOMEM  == SurfaceDesc.Pool)))
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMSURFACE_DESC.Pool. Failing.");
			pTexture->Release();
			return TPR_FAIL;
		}

		//
		// Is the size non-zero?
		//
		if (0 == SurfaceDesc.Size)
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMSURFACE_DESC.Size. Failing.");
			pTexture->Release();
			return TPR_FAIL;
		}

		//
		// Does the MultiSampleType match the expectation?
		//
		if (D3DMMULTISAMPLE_NONE != SurfaceDesc.MultiSampleType)
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMSURFACE_DESC.MultiSampleType. Failing.");
			pTexture->Release();
			return TPR_FAIL;
		}

		//
		// Mip-map width, for any level, could not possibly exceed top-level
		// mip-map width.
		//
		if (D3DQA_DEFAULT_MIPMAP_WIDTH < SurfaceDesc.Width)
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMSURFACE_DESC.Width. Failing.");
			pTexture->Release();
			return TPR_FAIL;
		}

		//
		// Mip-map height, for any level, could not possibly exceed top-level
		// mip-map height.
		//
		if (D3DQA_DEFAULT_MIPMAP_HEIGHT < SurfaceDesc.Height)
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMSURFACE_DESC.Height. Failing.");
			pTexture->Release();
			return TPR_FAIL;
		}

	}

	pTexture->Release();

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteGetSurfaceLevelTest
//
//   Verify IDirect3DMobileTexture::GetSurfaceLevel
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteGetSurfaceLevelTest()     
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteGetSurfaceLevelTest.");

	//
	// Interfaces to be manipulated during this test case
	//
	IDirect3DMobileTexture *pTexture;
	IDirect3DMobileSurface *pSurface;

	//
	// Surface description to retrieve from texture
	//
	D3DMSURFACE_DESC SurfaceDesc;

	//
	// Iterator for walking through mip-map levels; inspecting surface descriptions
	//
	UINT uiLevelIter;

	//
	// Mip-Map Level Count
	//
	DWORD dwLevelCount;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Before testing texture interface, verify that device supports texture mapping
	//
	if (FAILED(hr = SupportsTextures()))
	{
		DebugOut(DO_ERROR, L"Device does not support texture mapping. (hr = 0x%08x) Skipping.", hr);
		return TPR_SKIP;
	}

	//
	// Before testing mip mapping, verify support
	//
	if (FAILED(hr = SupportsMipMap()))
	{
		DebugOut(DO_ERROR, L"Device does not support mip mapping. (hr = 0x%08x) Skipping.", hr);
		return TPR_SKIP;
	}
    
	//
	// Create a mip-map in either system or video memory, depending on device capabilities
	//
	if (FAILED(hr = CreateSimpleMipMap(&pTexture, FALSE))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	dwLevelCount = pTexture->GetLevelCount();

	//
	// Loop through each level; getting the level's description
	//
	for (uiLevelIter = 0; uiLevelIter < dwLevelCount; uiLevelIter++)
	{

		//		
		// Retrieve a surface interface for one of the mip-map levels
		//		
		if (FAILED(hr = pTexture->GetSurfaceLevel(uiLevelIter, // UINT Level
		                                     &pSurface))) // IDirect3DMobileSurface** ppSurfaceLevel
		{
			DebugOut(DO_ERROR, L"GetSurfaceLevel failed for a valid level. (hr = 0x%08x). Failing.", hr);
			pTexture->Release();
			return TPR_FAIL;
		}

		//
		// Get the description for the surface
		//
		if (FAILED(hr = pSurface->GetDesc(&SurfaceDesc))) // D3DMSURFACE_DESC *pDesc
		{
			DebugOut(DO_ERROR, L"GetDesc failed for a valid surface. (hr = 0x%08x). Failing.", hr);
			pSurface->Release();
			pTexture->Release();
			return TPR_FAIL;
		}

		//
		// The surface interface is not needed for the following validation
		//
		pSurface->Release();

		//
		// Does the format match the expectation?
		//
		if (D3DMFMT_UNKNOWN == SurfaceDesc.Format)
		{
			DebugOut(DO_ERROR, L"D3DMSURFACE_DESC.Format set to invalid value. Failing.");
			pTexture->Release();
			return TPR_FAIL;
		}

		//
		// Does the resource type match the expectation?
		//
		if (D3DMRTYPE_TEXTURE != SurfaceDesc.Type)
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMSURFACE_DESC.Type. Failing.");
			pTexture->Release();
			return TPR_FAIL;
		}

		//
		// Does the usage mask match the expectation?
		//
		if ((D3DMUSAGE_TEXTURE | D3DMUSAGE_RENDERTARGET) != SurfaceDesc.Usage)
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMSURFACE_DESC.Usage. Failing.");
			pTexture->Release();
			return TPR_FAIL;
		}

		//
		// Does the pool type match the expectation?
		//
		if (!((D3DMPOOL_SYSTEMMEM == SurfaceDesc.Pool) ||
		      (D3DMPOOL_VIDEOMEM  == SurfaceDesc.Pool)))
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMSURFACE_DESC.Pool. Failing.");
			pTexture->Release();
			return TPR_FAIL;
		}

		//
		// Is the size non-zero?
		//
		if (0 == SurfaceDesc.Size)
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMSURFACE_DESC.Size. Failing.");
			pTexture->Release();
			return TPR_FAIL;
		}

		//
		// Does the MultiSampleType match the expectation?
		//
		if (D3DMMULTISAMPLE_NONE != SurfaceDesc.MultiSampleType)
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMSURFACE_DESC.MultiSampleType. Failing.");
			pTexture->Release();
			return TPR_FAIL;
		}

		//
		// Mip-map width, for any level, could not possibly exceed top-level
		// mip-map width.
		//
		if (D3DQA_DEFAULT_MIPMAP_WIDTH < SurfaceDesc.Width)
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMSURFACE_DESC.Width. Failing.");
			pTexture->Release();
			return TPR_FAIL;
		}

		//
		// Mip-map height, for any level, could not possibly exceed top-level
		// mip-map height.
		//
		if (D3DQA_DEFAULT_MIPMAP_HEIGHT < SurfaceDesc.Height)
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMSURFACE_DESC.Height. Failing.");
			pTexture->Release();
			return TPR_FAIL;
		}

	}

	pTexture->Release();

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;

}

//
// ExecuteLockRectTest
//
//   Verify IDirect3DMobileTexture::LockRect
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteLockRectTest()     
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteLockRectTest.");

	//
	// Texture interface to be manipulated during this test case
	//
	IDirect3DMobileTexture *pTexture;

	//
	// Description of locked surface
	//
	D3DMLOCKED_RECT LockedRect;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Before testing texture interface, verify that device supports texture mapping
	//
	if (FAILED(hr = SupportsTextures()))
	{
		DebugOut(DO_ERROR, L"Device does not support texture mapping. (hr = 0x%08x) Skipping.", hr);
		return TPR_SKIP;
	}

	//
	// Verify lockable texture support
	//
	if (FAILED(hr = SupportsLockableTexture()))
	{
		//
		// Skip majority of test
		//
		DebugOut(DO_ERROR, L"Device does not support lockable textures. Skipping.");
    
		if (FAILED(hr = CreateSimpleTexture(&pTexture, FALSE))) // IDirect3DMobileTexture **ppTexture
		{
			DebugOut(DO_ERROR, L"CreateSimpleTexture failed. (hr = 0x%08x). Failing.", hr);
			return TPR_FAIL;
		}
		
		//
		// Bad parameter test #1:  Invalid flags
		//
		if (SUCCEEDED(hr = pTexture->LockRect(0,           // UINT Level
		                                 &LockedRect, // D3DMLOCKED_RECT* pLockedRect,
		                                 NULL,        // CONST RECT* pRect
		                                 0xFFFFFFFF)))// DWORD Flags
		{
			DebugOut(DO_ERROR, L"LockRect succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
			pTexture->Release();
			return TPR_FAIL;
		}   	
		
		//
		// Bad parameter test #2:  Attempt to lock despite lack of driver support
		//
		if (SUCCEEDED(hr = pTexture->LockRect(0,           // UINT Level
		                                 &LockedRect, // D3DMLOCKED_RECT* pLockedRect,
		                                 NULL,        // CONST RECT* pRect
		                                 0)))         // DWORD Flags
		{
			DebugOut(DO_ERROR, L"LockRect failed. (hr = 0x%08x). Failing.", hr);
			pTexture->Release();
			return TPR_FAIL;
		}   	
		
		//
		// Bad parmeter test #3:  Invalid flag combo (D3DMLOCK_READONLY with D3DMLOCK_DISCARD)
		//
		if (SUCCEEDED(hr = pTexture->LockRect(0,                                      // UINT Level
		                                 &LockedRect,                            // D3DMLOCKED_RECT* pLockedRect,
		                                 NULL,                                   // CONST RECT* pRect
		                                 D3DMLOCK_READONLY | D3DMLOCK_DISCARD))) // DWORD Flags
		{
			DebugOut(DO_ERROR, L"LockRect succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
			pTexture->Release();
			return TPR_FAIL;
		}   	
		
		//
		// Bad parmeter test #4:  Invalid flags (D3DMLOCK_DISCARD only valid with D3DMUSAGE_DYNAMIC)
		//
		if (SUCCEEDED(hr = pTexture->LockRect(0,                  // UINT Level
		                                 &LockedRect,        // D3DMLOCKED_RECT* pLockedRect,
		                                 NULL,               // CONST RECT* pRect
		                                 D3DMLOCK_DISCARD))) // DWORD Flags
		{
			DebugOut(DO_ERROR, L"LockRect succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
			pTexture->Release();
			return TPR_FAIL;
		}   	
		
		//
		// Bad parmeter test #5:  Invalid flags (D3DMLOCK_NOOVERWRITE only valid with D3DMUSAGE_DYNAMIC)
		//
		if (SUCCEEDED(hr = pTexture->LockRect(0,                      // UINT Level
		                                 &LockedRect,            // D3DMLOCKED_RECT* pLockedRect,
		                                 NULL,                   // CONST RECT* pRect
		                                 D3DMLOCK_NOOVERWRITE))) // DWORD Flags
		{
			DebugOut(DO_ERROR, L"LockRect succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
			pTexture->Release();
			return TPR_FAIL;
		}   	

		pTexture->Release();
		return TPR_SKIP;
	}
    
	if (FAILED(hr = CreateSimpleTexture(&pTexture, TRUE))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}
	
	//
	// Bad parmeter test #6:  Invalid flags
	//
	if (SUCCEEDED(hr = pTexture->LockRect(0,           // UINT Level
	                                 &LockedRect, // D3DMLOCKED_RECT* pLockedRect,
	                                 NULL,        // CONST RECT* pRect
	                                 0xFFFFFFFF)))// DWORD Flags
	{
		DebugOut(DO_ERROR, L"LockRect succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
		pTexture->Release();
		return TPR_FAIL;
	}   	

	//
	// Bad parmeter test #7:  Invalid flag combo (D3DMLOCK_READONLY with D3DMLOCK_DISCARD)
	//
	if (SUCCEEDED(hr = pTexture->LockRect(0,                                      // UINT Level
	                                 &LockedRect,                            // D3DMLOCKED_RECT* pLockedRect,
	                                 NULL,                                   // CONST RECT* pRect
	                                 D3DMLOCK_READONLY | D3DMLOCK_DISCARD))) // DWORD Flags
	{
		DebugOut(DO_ERROR, L"LockRect succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
		pTexture->Release();
		return TPR_FAIL;
	}   	
	
	//
	// Bad parmeter test #8:  Invalid flags (D3DMLOCK_DISCARD only valid with D3DMUSAGE_DYNAMIC)
	//
	if (SUCCEEDED(hr = pTexture->LockRect(0,                  // UINT Level
	                                 &LockedRect,        // D3DMLOCKED_RECT* pLockedRect,
	                                 NULL,               // CONST RECT* pRect
	                                 D3DMLOCK_DISCARD))) // DWORD Flags
	{
		DebugOut(DO_ERROR, L"LockRect succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
		pTexture->Release();
		return TPR_FAIL;
	}   	
	
	//
	// Bad parmeter test #9:  Invalid flags (D3DMLOCK_NOOVERWRITE only valid with D3DMUSAGE_DYNAMIC)
	//
	if (SUCCEEDED(hr = pTexture->LockRect(0,                      // UINT Level
	                                 &LockedRect,            // D3DMLOCKED_RECT* pLockedRect,
	                                 NULL,                   // CONST RECT* pRect
	                                 D3DMLOCK_NOOVERWRITE))) // DWORD Flags
	{
		DebugOut(DO_ERROR, L"LockRect succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
		pTexture->Release();
		return TPR_FAIL;
	}   	
	
	//
	// Lock the entire texture
	//
	if (FAILED(hr = pTexture->LockRect(0,           // UINT Level
	                              &LockedRect, // D3DMLOCKED_RECT* pLockedRect,
	                              NULL,        // CONST RECT* pRect
	                              0)))         // DWORD Flags
	{
		DebugOut(DO_ERROR, L"LockRect failed. (hr = 0x%08x). Failing.", hr);
		pTexture->Release();
		return TPR_FAIL;
	}   	
	
	//
	// Unlock the entire texture
	//
	if (FAILED(hr = pTexture->UnlockRect(0)))
	{
		DebugOut(DO_ERROR, L"LockRect failed. (hr = 0x%08x). Failing.", hr);
		pTexture->Release();
		return TPR_FAIL;
	}   	
		
	//
	// Texture interface is no longer needed
	//
	pTexture->Release();

	//
	// Before optional testing of mip mapping, verify support
	//
	if (SUCCEEDED(hr = SupportsMipMap()))
	{
		//
		// Create a mip-map in either system or video memory, depending on device capabilities
		//
		if (FAILED(hr = CreateSimpleMipMap(&pTexture, TRUE))) // IDirect3DMobileTexture **ppTexture
		{
			DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
			return TPR_FAIL;
		}

		//
		// Lock the entire texture
		//
		if (FAILED(hr = pTexture->LockRect(0,           // UINT Level
		                              &LockedRect, // D3DMLOCKED_RECT* pLockedRect,
		                              NULL,        // CONST RECT* pRect
		                              0)))         // DWORD Flags
		{
			DebugOut(DO_ERROR, L"LockRect failed. (hr = 0x%08x). Failing.", hr);
			pTexture->Release();
			return TPR_FAIL;
		}   	

		//
		// Unlock the entire texture
		//
		if (FAILED(hr = pTexture->UnlockRect(0)))
		{
			DebugOut(DO_ERROR, L"LockRect failed. (hr = 0x%08x). Failing.", hr);
			pTexture->Release();
			return TPR_FAIL;
		}   	

		pTexture->Release();
	}

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;

}

//
// ExecuteUnlockRectTest
//
//   Verify IDirect3DMobileTexture::UnlockRect
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteUnlockRectTest()     
{
	DebugOut(L"Beginning ExecuteUnlockRectTest.");
    
	//
	// Testing for this method occurs in a closely-related test:
	//
	return ExecuteLockRectTest();
}

//
// ExecuteAddDirtyRectTest
//
//   Verify IDirect3DMobileTexture::AddDirtyRect
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteAddDirtyRectTest()     
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteAddDirtyRectTest.");

	//
	// Texture interface to be manipulated during this test case
	//
	IDirect3DMobileTexture *pTexture;

	//
	// Sample extent data to use for dirty rect calls
	//
	CONST UINT uiNumDirtyRects = 10;
	CONST RECT rDirtyRects[uiNumDirtyRects] = 
	{
	//  | LEFT | TOP  | RIGHT | BOTTOM | 
	//  +------+------+-------+--------+
		{    0 ,    0 ,     1 ,      1 },
		{    1 ,    1 ,     2 ,      2 },
		{    2 ,    2 ,     3 ,      3 },
		{    0 ,    0 ,    32 ,     32 }, // Dirty rect that covers some previously-specified rects
		{   63 ,   62 ,    62 ,     63 }, // One-pixel dirty rect
		{   62 ,   62 ,    63 ,     63 }, // Dirty rect that is identical to a previous dirty rect
		{    0 ,    0 ,     1 ,      1 }, // "Corner case"
		{    0 ,   33 ,    63 ,     32 },
		{    0 ,   32 ,    63 ,     33 },
		{    0 ,    0 ,    63 ,     63 }  // Fully covering dirty rect
	};		
			
	// 
	// Iterator for testing the set of dirty rect extents
	// 
	UINT uiDirtyRectIter;


	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Before testing texture interface, verify that device supports texture mapping
	//
	if (FAILED(hr = SupportsTextures()))
	{
		DebugOut(DO_ERROR, L"Device does not support texture mapping. (hr = 0x%08x) Skipping.", hr);
		return TPR_SKIP;
	}

    
	if (FAILED(hr = CreateSimpleTexture(&pTexture, FALSE))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}


	for (uiDirtyRectIter = 0; uiDirtyRectIter < uiNumDirtyRects; uiDirtyRectIter++)
	{
		if (FAILED(hr = pTexture->AddDirtyRect(&rDirtyRects[uiDirtyRectIter]))) // CONST RECT* pDirtyRect
		{
			DebugOut(DO_ERROR, L"AddDirtyRect failed. (hr = 0x%08x). Failing.", hr);
			pTexture->Release();
			return TPR_FAIL;
		}
	}

	//
	// The NULL case implies that the entire texture should be dirtied.
	//
	if (FAILED(hr = pTexture->AddDirtyRect(NULL))) // CONST RECT* pDirtyRect
	{
		DebugOut(DO_ERROR, L"AddDirtyRect failed. (hr = 0x%08x). Failing.", hr);
		pTexture->Release();
		return TPR_FAIL;
	}

	//
	// Texture interface is no longer needed
	//
	pTexture->Release();

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}


//
// ExecuteTexSurfQueryInterfaceTest
//
//   Using a surface obtained from a mip level, verify IDirect3DMobileSurface::QueryInterface
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteTexSurfQueryInterfaceTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteTexSurfQueryInterfaceTest.");
        
	//
	// Surface level from texture
	//
	LPDIRECT3DMOBILESURFACE pSurface = NULL;

	//
	// Surface interface for QueryInterface
	//
	LPDIRECT3DMOBILESURFACE pQISurface = NULL;

	//
	// Texture interface to be manipulated during this test case
	//
	LPDIRECT3DMOBILETEXTURE pTexture = NULL;

	//
	// Test result
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Before testing texture interface, verify that device supports texture mapping
	//
	if (FAILED(hr = SupportsTextures()))
	{
		DebugOut(DO_ERROR, L"Device does not support texture mapping. (hr = 0x%08x) Skipping.", hr);
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Create a non-lockable texture
	//
	if (FAILED(hr = CreateSimpleTexture(&pTexture, FALSE))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Retrieve a surface interface pointer
	//
	if (FAILED(hr = pTexture->GetSurfaceLevel(0,           // UINT Level
	                                     &pSurface))) // IDirect3DMobileSurface** ppSurfaceLevel
	{
		DebugOut(DO_ERROR, L"GetSurfaceLevel failed. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Test case #1:  Invalid REFIID
	//
	if (SUCCEEDED(hr = pSurface->QueryInterface(IID_D3DQAInvalidInterface, // REFIID riid
	                                       (void**)(&pQISurface))))   // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Test case #2:  Valid REFIID, invalid PPVOID
	//
	if (SUCCEEDED(hr = pSurface->QueryInterface(IID_IDirect3DMobileSurface, // REFIID riid
	                                       (void**)0)))                // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Test case #3:  Invalid REFIID, invalid PPVOID
	//
	if (SUCCEEDED(hr = pSurface->QueryInterface(IID_D3DQAInvalidInterface, // REFIID riid
	                                       (void**)0)))               // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}


	//
	// Valid parameter tests
	//

	// 	
	// Perform an query on the surface interface
	// 	
	if (FAILED(hr = pSurface->QueryInterface(IID_IDirect3DMobileSurface,  // REFIID riid
						  		          (void**)(&pQISurface))))     // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface failed. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

cleanup:

	if (pSurface)
		pSurface->Release();

	if (pTexture)
		pTexture->Release();

	if (pQISurface)
		pQISurface->Release();

	return Result;
}


//
// ExecuteTexSurfAddRefTest
//
//   Using a surface obtained from a mip level, verify IDirect3DMobileSurface::AddRef
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteTexSurfAddRefTest()
{

	return TPR_SKIP;
}


//
// ExecuteTexSurfReleaseTest
//
//   Using a surface obtained from a mip level, verify IDirect3DMobileSurface::Release
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteTexSurfReleaseTest()
{

	return TPR_SKIP;
}


//
// ExecuteTexSurfGetDeviceTest
//
//   Using a surface obtained from a mip level, verify IDirect3DMobileSurface::GetDevice
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteTexSurfGetDeviceTest()
{

	return TPR_SKIP;
}


//
// ExecuteTexSurfGetContainerTest
//
//   Using a surface obtained from a mip level, verify IDirect3DMobileSurface::GetContainer
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteTexSurfGetContainerTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteTexSurfGetContainerTest.");

	//
	// Container for surfaces
	//
	LPDIRECT3DMOBILETEXTURE pTextureContainer = NULL;
        
	//
	// Surface level from texture
	//
	LPDIRECT3DMOBILESURFACE pSurface = NULL;

	//
	// Texture interface to be manipulated during this test case
	//
	LPDIRECT3DMOBILETEXTURE pTexture = NULL;

	//
	// Test result
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Before testing texture interface, verify that device supports texture mapping
	//
	if (FAILED(hr = SupportsTextures()))
	{
		DebugOut(DO_ERROR, L"Device does not support texture mapping. (hr = 0x%08x) Skipping.", hr);
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Create a non-lockable texture
	//
	if (FAILED(hr = CreateSimpleTexture(&pTexture, FALSE))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Retrieve a surface interface pointer
	//
	if (FAILED(hr = pTexture->GetSurfaceLevel(0,           // UINT Level
	                                     &pSurface))) // IDirect3DMobileSurface** ppSurfaceLevel
	{
		DebugOut(DO_ERROR, L"GetSurfaceLevel failed. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Retrieve container
	//
	if (FAILED(hr = pSurface->GetContainer(IID_IDirect3DMobileTexture,     // REFIID riid
	                                  (void**)(&pTextureContainer)))) // void** ppvObj
	{
		DebugOut(DO_ERROR, L"GetContainer failed. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Does container match expectations
	//
	if (pTextureContainer != pTexture)
	{
		DebugOut(DO_ERROR, L"GetContainer:  unexpected result. Failing.");
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Release container
	//
	pTextureContainer->Release();
	pTextureContainer=NULL;


	// 	
	// Bad parameter tests
	// 	

	//
	// Test case #1:  Invalid REFIID
	//
	if (SUCCEEDED(hr = pSurface->GetContainer(IID_D3DQAInvalidInterface,      // REFIID riid
	                                     (void**)(&pTextureContainer)))) // void** ppvObj
	{
		DebugOut(DO_ERROR, L"GetContainer succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Test case #2:  Invalid PPVOID
	//
	if (SUCCEEDED(hr = pSurface->GetContainer(IID_IDirect3DMobileTexture,     // REFIID riid
	                                     (void**)0)))                    // void** ppvObj
	{
		DebugOut(DO_ERROR, L"GetContainer succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Test case #3:  Invalid REFIID, Invalid PPVOID
	//
	if (SUCCEEDED(hr = pSurface->GetContainer(IID_D3DQAInvalidInterface,      // REFIID riid
	                                     (void**)0)))                    // void** ppvObj
	{
		DebugOut(DO_ERROR, L"GetContainer succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

cleanup:

	if (pSurface)
		pSurface->Release();

	if (pTexture)
		pTexture->Release();

	if (pTextureContainer)
		pTextureContainer->Release();

	return Result;
}


//
// ExecuteTexSurfGetDescTest
//
//   Using a surface obtained from a mip level, verify IDirect3DMobileSurface::GetDesc
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteTexSurfGetDescTest()
{

	return TPR_SKIP;
}


//
// ExecuteTexSurfLockRectTest
//
//   Using a surface obtained from a mip level, verify IDirect3DMobileSurface::LockRect
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteTexSurfLockRectTest()
{

	return TPR_SKIP;
}


//
// ExecuteTexSurfUnlockRectTest
//
//   Using a surface obtained from a mip level, verify IDirect3DMobileSurface::UnlockRect
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteTexSurfUnlockRectTest()
{

	return TPR_SKIP;
}


//
// ExecuteTexSurfGetDCTest
//
//   Using a surface obtained from a mip level, verify IDirect3DMobileSurface::GetDC
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteTexSurfGetDCTest()     
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteTexSurfGetDCTest.");
        
	//
	// Surface level from texture
	//
	IDirect3DMobileSurface* pSurface = NULL;

	//
	// Texture interface to be manipulated during this test case
	//
	IDirect3DMobileTexture *pTexture = NULL;

	//
	// Device context for use with GDI
	//
	HDC hDC;

	//
	// Test result
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Before testing texture interface, verify that device supports texture mapping
	//
	if (FAILED(hr = SupportsTextures()))
	{
		DebugOut(DO_ERROR, L"Device does not support texture mapping. (hr = 0x%08x) Skipping.", hr);
		Result = TPR_SKIP;
		goto cleanup;
	}


	//
	// Verify lockable texture support
	//
	if (FAILED(hr = SupportsLockableTexture()))
	{
		DebugOut(DO_ERROR, L"Device does not support lockable textures.");

		//
		// Skip most of the test, but verify that GetDC fails
		//

		//
		// Create a non-lockable texture
		//
		if (FAILED(hr = CreateSimpleTexture(&pTexture, FALSE))) // IDirect3DMobileTexture **ppTexture
		{
			DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
			Result = TPR_FAIL;
			goto cleanup;
		}

		//
		// Retrieve a surface interface pointer
		//
		if (FAILED(hr = pTexture->GetSurfaceLevel(0,           // UINT Level
											 &pSurface))) // IDirect3DMobileSurface** ppSurfaceLevel
		{
			DebugOut(DO_ERROR, L"GetSurfaceLevel failed. (hr = 0x%08x). Failing.", hr);
			Result = TPR_FAIL;
			goto cleanup;
		}

		//
		// Get the device context
		//
		if (SUCCEEDED(hr = pSurface->GetDC(&hDC))) // HDC* phdc
		{
			DebugOut(DO_ERROR, L"GetDC succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
			Result = TPR_FAIL;
			goto cleanup;
		}
    
		//
		// Bad parameter test:  NULL arg for ReleaseDC
		//
		if (SUCCEEDED(hr = pSurface->ReleaseDC((HDC)0xFFFFFFFF))) // HDC hdc
		{
			DebugOut(DO_ERROR, L"ReleaseDC succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
			Result = TPR_FAIL;
			goto cleanup;
		}
    
		Result = TPR_SKIP;

		goto cleanup;
	}


	//
	// Create a non-lockable texture
	//
	if (FAILED(hr = CreateSimpleTexture(&pTexture, TRUE))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Retrieve a surface interface pointer
	//
	if (FAILED(hr = pTexture->GetSurfaceLevel(0,           // UINT Level
	                                     &pSurface))) // IDirect3DMobileSurface** ppSurfaceLevel
	{
		DebugOut(DO_ERROR, L"GetSurfaceLevel failed. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}
	
	//
	// Get the device context
	//
	if (FAILED(hr = pSurface->GetDC(&hDC))) // HDC* phdc
	{
		DebugOut(DO_ERROR, L"GetDC failed. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}
    
	//
	// NULL handles are invalid
	//
	if (NULL == hDC)
	{
		DebugOut(DO_ERROR, L"GetDC returned NULL device context. Failing.");
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Bad parameter test #1:  NULL arg
	//
	if (SUCCEEDED(hr = pSurface->GetDC(NULL))) // HDC* phdc
	{
		DebugOut(DO_ERROR, L"GetDC succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Bad parameter test #2:  NULL arg for ReleaseDC
	//
	if (SUCCEEDED(hr = pSurface->ReleaseDC((HDC)0xFFFFFFFF))) // HDC hdc
	{
		DebugOut(DO_ERROR, L"ReleaseDC succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Release the device context
	//
	if (FAILED(hr = pSurface->ReleaseDC(hDC))) // HDC hdc
	{
		DebugOut(DO_ERROR, L"ReleaseDC failed. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

cleanup:

	if (pSurface)
		pSurface->Release();

	if (pTexture)
		pTexture->Release();

	return Result;

}


//
// ExecuteTexSurfReleaseDCTest
//
//   Using a surface obtained from a mip level, verify IDirect3DMobileSurface::ReleaseDC
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteTexSurfReleaseDCTest()
{
	DebugOut(L"Beginning ExecuteTexSurfReleaseDCTest.");
        
	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Testing for this method occurs in a closely-related test:
	//
	return ExecuteTexSurfGetDCTest();
}



//
// ExecuteTexSurfStretchRectTest
//
//   Using a surfaces obtained from a mip levels, verify IDirect3DMobileDevice::StretchRect.
//   Because source and dest surfaces are same format in these test cases;
//   IDirect3DMobile::CheckDeviceFormatConversion is not a prerequisite to the StretchRect calls.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteTexSurfStretchRectTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteTexSurfStretchRectTest.");

	//
	// Surface levels from texture
	//
	LPDIRECT3DMOBILESURFACE pSurface1 = NULL, pSurface2 = NULL;

	//
	// Texture interfaces to be manipulated during this test case
	//
	LPDIRECT3DMOBILETEXTURE pTexture1 = NULL, pTexture2 = NULL;

	//
	// Test result
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Before testing texture interface, verify that device supports texture mapping
	//
	if (FAILED(hr = SupportsTextures()))
	{
		DebugOut(DO_ERROR, L"Device does not support texture mapping. (hr = 0x%08x) Skipping.", hr);
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Create a non-lockable texture
	//
	if (FAILED(hr = CreateSimpleTexture(&pTexture1, FALSE))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Create a non-lockable texture
	//
	if (FAILED(hr = CreateSimpleTexture(&pTexture2, FALSE))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Retrieve a surface interface pointer
	//
	if (FAILED(hr = pTexture1->GetSurfaceLevel(0,           // UINT Level
	                                     &pSurface1))) // IDirect3DMobileSurface** ppSurfaceLevel
	{
		DebugOut(DO_ERROR, L"GetSurfaceLevel failed. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Retrieve a surface interface pointer
	//
	if (FAILED(hr = pTexture2->GetSurfaceLevel(0,           // UINT Level
	                                     &pSurface2))) // IDirect3DMobileSurface** ppSurfaceLevel
	{
		DebugOut(DO_ERROR, L"GetSurfaceLevel failed. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// StretchRect from one texture's surface to another texture's surface
	//
	if (FAILED(hr = m_pd3dDevice->StretchRect(pSurface1,       // IDirect3DMobileSurface* pSourceSurface
		                                 NULL,            // CONST RECT* pSourceRect
		                                 pSurface2,       // IDirect3DMobileSurface* pDestSurface
		                                 NULL,            // CONST RECT* pDestRect
		                                 D3DMTEXF_POINT)))// D3DMTEXTUREFILTERTYPE Filter
	{
		DebugOut(DO_ERROR, L"StretchRect failed. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}


cleanup:

	if (pSurface1)
		pSurface1->Release();

	if (pTexture1)
		pTexture1->Release();

	if (pSurface2)
		pSurface2->Release();

	if (pTexture2)
		pTexture2->Release();

	return Result;


}


//
// ExecuteTexSurfColorFillTest
//
//   Using a surface obtained from a mip level, verify IDirect3DMobileDevice::ColorFill
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT TextureTest::ExecuteTexSurfColorFillTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteTexSurfStretchRectTest.");

	//
	// Surface level from texture
	//
	LPDIRECT3DMOBILESURFACE pSurface = NULL;

	//
	// Texture interface to be manipulated during this test case
	//
	LPDIRECT3DMOBILETEXTURE pTexture = NULL;

	//
	// Test result
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Before testing texture interface, verify that device supports texture mapping
	//
	if (FAILED(hr = SupportsTextures()))
	{
		DebugOut(DO_ERROR, L"Device does not support texture mapping. (hr = 0x%08x) Skipping.", hr);
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Create a non-lockable texture
	//
	if (FAILED(hr = CreateSimpleTexture(&pTexture, FALSE))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Retrieve a surface interface pointer
	//
	if (FAILED(hr = pTexture->GetSurfaceLevel(0,          // UINT Level
	                                    &pSurface))) // IDirect3DMobileSurface** ppSurfaceLevel
	{
		DebugOut(DO_ERROR, L"GetSurfaceLevel failed. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}


	//
	// Colorfill the surface belonging to the texture
	//
	if (FAILED(hr = m_pd3dDevice->ColorFill(pSurface,                 // IDirect3DMobileSurface* pSurface
	                                   NULL,                     // CONST RECT* pRect
	                                   D3DMCOLOR_XRGB(255,0,0))))// D3DMCOLOR Color
	{
		DebugOut(DO_ERROR, L"ColorFill failed. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}


cleanup:

	if (pSurface)
		pSurface->Release();

	if (pTexture)
		pTexture->Release();


	return Result;


}



/*

	//
	// Surface level from texture
	//
	LPDIRECT3DMOBILESURFACE pSurface = NULL;

	//
	// Texture interface to be manipulated during this test case
	//
	LPDIRECT3DMOBILETEXTURE pTexture = NULL;

	//
	// Test result
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Before testing texture interface, verify that device supports texture mapping
	//
	if (FAILED(hr = SupportsTextures()))
	{
		DebugOut(DO_ERROR, L"Device does not support texture mapping. (hr = 0x%08x) Skipping.", hr);
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Create a non-lockable texture
	//
	if (FAILED(hr = CreateSimpleTexture(&pTexture, FALSE))) // IDirect3DMobileTexture **ppTexture
	{
		DebugOut(DO_ERROR, L"Unable to create a texture; although device supports texture mapping. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Retrieve a surface interface pointer
	//
	if (FAILED(hr = pTexture->GetSurfaceLevel(0,           // UINT Level
	                                     &pSurface))) // IDirect3DMobileSurface** ppSurfaceLevel
	{
		DebugOut(DO_ERROR, L"GetSurfaceLevel failed. (hr = 0x%08x). Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}


	// 	
	// Bad parameter test #
	// 	


cleanup:

	if (pSurface)
		pSurface->Release();

	if (pTexture)
		pTexture->Release();

	return Result;

  */
