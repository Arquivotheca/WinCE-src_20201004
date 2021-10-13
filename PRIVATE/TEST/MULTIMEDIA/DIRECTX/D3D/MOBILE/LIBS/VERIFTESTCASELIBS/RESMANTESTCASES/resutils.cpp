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
#include <windows.h>
#include <d3dm.h>
#include <tchar.h>
#include <math.h>
#include "CreationArgs.h"
#include "ResUtils.h"
#include "DebugOutput.h"

//
// Individual resources should not exceed 100k.  This means that even with a small amount of available memory,
// perhaps 1MB, this test can still create several textures for use with the resource manager
//
#define D3DQA_DESIRED_MAX_PER_RESOURCE_SIZE (100000)

// 
// GetValidTextureArgs
// 
//   Generate a set of arguments for CreateTexture used with the managed pool, that are valid even
//   under the most constraining conditions.  Constraints:
//
//     * D3DMPTEXTURECAPS_SQUAREONLY
//     * MaxTextureAspectRatio
//     * MaxTextureWidth   
//     * MaxTextureHeight   
//     * D3DMPTEXTURECAPS_POW2   
// 
// Arguments:
// 
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying Direct3D device
//   CREATETEXTURE_ARGS *pCreateTexArgs:  Proposed arguments for resource creation
//   SIZE_T *pEstimatedSize:  Approximate amount of memory consumption, per resource creation
// 
// Return Value:
//
//   HRESULT indicates success or failure
// 
HRESULT GetValidTextureArgs(LPDIRECT3DMOBILEDEVICE pDevice, CREATETEXTURE_ARGS *pCreateTexArgs, SIZE_T *pEstimatedSize)
{
    HRESULT hr;
    
	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Used in computation of desired/expected texture sizes
	//
	CONST UINT uiTotalSizeMax = D3DQA_DESIRED_MAX_PER_RESOURCE_SIZE;
	UINT uiBytesPerPixel;
	UINT uiMaxSquareExtent;

	//
	// Mask for finding leading one
	//
	UINT uiMask = 0x10000000;

	//
	// Get device capabilities
	//
	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x)", hr);
		return hr;
	}

	//
	// Find a format suitable for textures
	// 
	if (FAILED(hr = GetValidTextureFormat(pDevice,                     // LPDIRECT3DMOBILEDEVICE pDevice
	                                      &(pCreateTexArgs->Format)))) // D3DMFORMAT *pFormat
	{
		DebugOut(DO_ERROR, L"GetValidTextureFormat failed. (hr = 0x%08x)", hr);
		return hr;
	}

	//
	// Find the byte size of the chosen format
	//
	if (FAILED(hr = GetBytesPerPixel(pCreateTexArgs->Format, // D3DMFORMAT Format
	                                 &uiBytesPerPixel)))     // UINT *puiBytesPerPixel)
	{
		DebugOut(DO_ERROR, L"GetBytesPerPixel failed. (hr = 0x%08x)", hr);
		return hr;
	}

	//
	// Square textures are used to ensure that D3DMPTEXTURECAPS_SQUAREONLY and MaxTextureAspectRatio
	// drivers are able to run this test, by choosing square texture extents
	//
	uiMaxSquareExtent = (UINT)sqrt(((float)uiTotalSizeMax / (float)(uiBytesPerPixel)));

	//
	// Ensure that drivers with (MaxTextureWidth, MaxTextureHeight) are able to run this test.
	//
	if (uiMaxSquareExtent > Caps.MaxTextureWidth)
		uiMaxSquareExtent = Caps.MaxTextureWidth;
	if (uiMaxSquareExtent > Caps.MaxTextureHeight)
		uiMaxSquareExtent = Caps.MaxTextureHeight;

	//
	// Ensure that drivers with D3DMPTEXTURECAPS_POW2 are able to run this test.
	//
	while(!(uiMaxSquareExtent & uiMask))
		uiMask>>=1;
	uiMaxSquareExtent = uiMaxSquareExtent & uiMask;

	// pCreateTexArgs->Format is already set
	pCreateTexArgs->Height = uiMaxSquareExtent;
	pCreateTexArgs->Levels = 1;
	pCreateTexArgs->Pool = D3DMPOOL_MANAGED;
	pCreateTexArgs->Usage = 0;
	pCreateTexArgs->Width = uiMaxSquareExtent;
	*pEstimatedSize = uiBytesPerPixel*uiMaxSquareExtent*uiMaxSquareExtent;

	return S_OK;
}

// 
// GetValidVertexBufferArgs
// 
//   Generate a set of arguments for CreateVertexBuffer used with the managed pool.
// 
// Arguments:
// 
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying Direct3D device
//   CREATEVERTEXBUFFER_ARGS *pCreateVertexBufferArgs:  Proposed arguments for resource creation
//   SIZE_T *pEstimatedSize:  Approximate amount of memory consumption, per resource creation
// 
// Return Value:
//
//   HRESULT indicates success or failure
// 
HRESULT GetValidVertexBufferArgs(LPDIRECT3DMOBILEDEVICE pDevice, CREATEVERTEXBUFFER_ARGS *pCreateVertexBufferArgs, SIZE_T *pEstimatedSize)
{
	//
	// Default size based on #define
	//
	CONST UINT uiEntrySize = 4 * sizeof(float); // D3DFVF_XYZRHW
	CONST UINT uiEntryCount = D3DQA_DESIRED_MAX_PER_RESOURCE_SIZE / uiEntrySize;
	CONST UINT uiTotalSize = uiEntryCount * uiEntrySize;

	//
	// Every driver must be able to create vertex buffers of any FVF type, regardless of whether or not the driver
	// supports the components therein.  Thus, the particular choice of D3DFVF_XYZRHW is arbitrary.
	//
	pCreateVertexBufferArgs->FVF = D3DMFVF_XYZRHW_FLOAT;
	pCreateVertexBufferArgs->Length = uiTotalSize;
	pCreateVertexBufferArgs->Pool = D3DMPOOL_MANAGED;
	pCreateVertexBufferArgs->Usage = 0;
	*pEstimatedSize = uiTotalSize;
	
	return S_OK;
}


// 
// GetValidIndexBufferArgs
// 
//   Generate a set of arguments for CreateIndexBuffer used with the managed pool.
// 
// Arguments:
// 
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying Direct3D device
//   CREATEINDEXBUFFER_ARGS *pCreateIndexBufferArgs:  Proposed arguments for resource creation
//   SIZE_T *pEstimatedSize:  Approximate amount of memory consumption, per resource creation
// 
// Return Value:
//
//   HRESULT indicates success or failure
// 
HRESULT GetValidIndexBufferArgs(LPDIRECT3DMOBILEDEVICE pDevice, CREATEINDEXBUFFER_ARGS *pCreateIndexBufferArgs, SIZE_T *pEstimatedSize)
{
	//
	// Default size based on #define
	//
	CONST UINT uiEntrySize = sizeof(__int32); // D3DMFMT_INDEX32
	CONST UINT uiEntryCount = D3DQA_DESIRED_MAX_PER_RESOURCE_SIZE / uiEntrySize;
	CONST UINT uiTotalSize = uiEntryCount * uiEntrySize;

	pCreateIndexBufferArgs->Format = D3DMFMT_INDEX32;
	pCreateIndexBufferArgs->Length = uiTotalSize;
	pCreateIndexBufferArgs->Pool = D3DMPOOL_MANAGED;
	pCreateIndexBufferArgs->Usage = 0;
	*pEstimatedSize = uiTotalSize;

	return S_OK;
}


// 
// CreateVertexBufferResources
// 
//   Create vertex buffer resources according to the specified arguments; return array of interface pointers
// 
// Arguments:
// 
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying Direct3D device
//   UINT uiResourceCount:  Number of resources to be created
//   CREATEVERTEXBUFFER_ARGS *pCreateVertexBufferArgs:  Proposed arguments for resource creation
//   LPDIRECT3DMOBILERESOURCE **pppInterfaces:  Output for pointer to array of interface pointers
// 
// Return Value:
//
//   HRESULT indicates success or failure
// 
HRESULT CreateVertexBufferResources(LPDIRECT3DMOBILEDEVICE pDevice, UINT uiResourceCount, CONST CREATEVERTEXBUFFER_ARGS *pCreateVertexBufferArgs, LPDIRECT3DMOBILERESOURCE **pppInterfaces)
{
	//
	// Iterator for manipulating entries in interface array
	//
	UINT uiResIter;

	//
	// Function result; set to failure in all error conditions
	//
	HRESULT hr = S_OK;

	//
	// Interface array
	//
	LPDIRECT3DMOBILEVERTEXBUFFER *ppVertexBuffers = NULL;

	//
	// Parameter validation
	//
	if ((NULL == pDevice) || (NULL == pppInterfaces) || (NULL == pCreateVertexBufferArgs) || (0 == uiResourceCount))
	{
		DebugOut(DO_ERROR, L"CreateVertexBufferResources:  Invalid argument(s).");
		return E_POINTER;
	}

	//
	// Allocate array of interface pointers for consumption by caller
	//
	ppVertexBuffers = (LPDIRECT3DMOBILEVERTEXBUFFER*)malloc(uiResourceCount * sizeof(LPDIRECT3DMOBILEVERTEXBUFFER*));
	if (NULL == ppVertexBuffers) {
		DebugOut(DO_ERROR, L"Out of memory");
		hr = E_OUTOFMEMORY;
		goto cleanup;
	}

	//
	// Begin by NULL-initializing array
	//
	for (uiResIter = 0; uiResIter < uiResourceCount; uiResIter++)
	{
		ppVertexBuffers[uiResIter] = NULL;
	}

	//
	// Create vertex buffers
	//
	for (uiResIter = 0; uiResIter < uiResourceCount; uiResIter++)
	{
		if (FAILED(hr = pDevice->CreateVertexBuffer(pCreateVertexBufferArgs->Length, // UINT Length,
											        pCreateVertexBufferArgs->Usage,  // DWORD Usage,
											        pCreateVertexBufferArgs->FVF,    // DWORD FVF,
											        pCreateVertexBufferArgs->Pool,   // D3DMPOOL Pool,
											        &ppVertexBuffers[uiResIter])))   // LPDIRECT3DMOBILEVERTEXBUFFER* ppVertexBuffer,
		{
			DebugOut(DO_ERROR, L"CreateVertexBuffer failed. (hr = 0x%08x)", hr);
			goto cleanup;
		}
	}

cleanup:

	if (FAILED(hr))
	{
		if (ppVertexBuffers)
		{
			for (uiResIter = 0; uiResIter < uiResourceCount; uiResIter++)
			{
				if (ppVertexBuffers[uiResIter])
				{
					ppVertexBuffers[uiResIter]->Release();
				}
			}
			free(ppVertexBuffers);
		}
		*pppInterfaces = NULL;
	}
	else
	{
		*pppInterfaces = (LPDIRECT3DMOBILERESOURCE*)ppVertexBuffers;
	}
	return hr;
}

// 
// CreateIndexBufferResources
// 
//   Create index buffer resources according to the specified arguments; return array of interface pointers
// 
// Arguments:
// 
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying Direct3D device
//   UINT uiResourceCount:  Number of resources to be created
//   CREATEINDEXBUFFER_ARGS *pCreateIndexBufferArgs:  Proposed arguments for resource creation
//   LPDIRECT3DMOBILERESOURCE **pppInterfaces:  Output for pointer to array of interface pointers
// 
// Return Value:
//
//   HRESULT indicates success or failure
// 
HRESULT CreateIndexBufferResources(LPDIRECT3DMOBILEDEVICE pDevice, UINT uiResourceCount, CONST CREATEINDEXBUFFER_ARGS *pCreateIndexBufferArgs, LPDIRECT3DMOBILERESOURCE **pppInterfaces)
{
	//
	// Iterator for manipulating entries in interface array
	//
	UINT uiResIter;

	//
	// Function result; set to failure in all error conditions
	//
	HRESULT hr = S_OK;

	//
	// Interface array
	//
	LPDIRECT3DMOBILEINDEXBUFFER *ppIndexBuffers = NULL;

	//
	// Parameter validation
	//
	if ((NULL == pDevice) || (NULL == pppInterfaces) || (NULL == pCreateIndexBufferArgs) || (0 == uiResourceCount))
	{
		DebugOut(DO_ERROR, L"CreateIndexBufferResources:  Invalid argument(s).");
		return E_INVALIDARG;
	}

	//
	// Allocate array of interface pointers for consumption by caller
	//
	ppIndexBuffers = (LPDIRECT3DMOBILEINDEXBUFFER *)malloc(uiResourceCount * sizeof(LPDIRECT3DMOBILEINDEXBUFFER *));
	if (NULL == ppIndexBuffers) {
		DebugOut(DO_ERROR, L"Out of memory");
		hr = E_OUTOFMEMORY;
		goto cleanup;
	}

	//
	// Begin by NULL-initializing array
	//
	for (uiResIter = 0; uiResIter < uiResourceCount; uiResIter++)
	{
		ppIndexBuffers[uiResIter] = NULL;
	}

	//
	// Create index buffers
	//
	for (uiResIter = 0; uiResIter < uiResourceCount; uiResIter++)
	{
		if (FAILED(hr = pDevice->CreateIndexBuffer(pCreateIndexBufferArgs->Length,// UINT Length,
											       pCreateIndexBufferArgs->Usage,   //	DWORD Usage,
										           pCreateIndexBufferArgs->Format,  //	D3DMFORMAT Format,
										    	   pCreateIndexBufferArgs->Pool,    //	D3DMPOOL Pool,
										    	   &ppIndexBuffers[uiResIter])))    // LPDIRECT3DMOBILEINDEXBUFFER *ppIndexBuffer,
		{
			DebugOut(DO_ERROR, L"CreateIndexBuffer failed. (hr = 0x%08x)", hr);
			goto cleanup;
		}
	}

cleanup:

	if (FAILED(hr))
	{
		if (ppIndexBuffers)
		{
			for (uiResIter = 0; uiResIter < uiResourceCount; uiResIter++)
			{
				if (ppIndexBuffers[uiResIter])
				{
					ppIndexBuffers[uiResIter]->Release();
				}
			}
			free(ppIndexBuffers);
		}
		*pppInterfaces = NULL;
	}
	else
	{
		*pppInterfaces = (LPDIRECT3DMOBILERESOURCE*)ppIndexBuffers;
	}
	return hr;
}

// 
// CreateTextureResources
// 
//   Create texture resources according to the specified arguments; return array of interface pointers
// 
// Arguments:
// 
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying Direct3D device
//   UINT uiResourceCount:  Number of resources to be created
//   CREATETEXTURE_ARGS *pCreateTextureArgs:  Proposed arguments for resource creation
//   LPDIRECT3DMOBILERESOURCE **pppInterfaces:  Output for pointer to array of interface pointers
// 
// Return Value:
//
//   HRESULT indicates success or failure
// 
HRESULT CreateTextureResources(LPDIRECT3DMOBILEDEVICE pDevice, UINT uiResourceCount, CONST CREATETEXTURE_ARGS *pCreateTextureArgs, LPDIRECT3DMOBILERESOURCE **pppInterfaces)
{
	//
	// Iterator for manipulating entries in interface array
	//
	UINT uiResIter;

	//
	// Function result; set to failure in all error conditions
	//
	HRESULT hr = S_OK;

	//
	// Interface array
	//
	LPDIRECT3DMOBILETEXTURE *ppTextures = NULL;

	//
	// Parameter validation
	//
	if ((NULL == pDevice) || (NULL == pppInterfaces) || (NULL == pCreateTextureArgs) || (0 == uiResourceCount))
	{
		DebugOut(DO_ERROR, L"CreateTextureResources:  Invalid argument(s).");
		return E_INVALIDARG;
	}

	//
	// Allocate array of interface pointers for consumption by caller
	//
	ppTextures = (LPDIRECT3DMOBILETEXTURE *)malloc(uiResourceCount * sizeof(LPDIRECT3DMOBILETEXTURE *));
	if (NULL == ppTextures) {
		DebugOut(DO_ERROR, L"Out of memory");
		hr = E_OUTOFMEMORY;
		goto cleanup;
	}

	//
	// Begin by NULL-initializing array
	//
	for (uiResIter = 0; uiResIter < uiResourceCount; uiResIter++)
	{
		ppTextures[uiResIter] = NULL;
	}

	//
	// Create textures
	//
	for (uiResIter = 0; uiResIter < uiResourceCount; uiResIter++)
	{
		if (FAILED(hr = pDevice->CreateTexture(pCreateTextureArgs->Width, // UINT Width,
										       pCreateTextureArgs->Height, // UINT Height,
										       pCreateTextureArgs->Levels, // UINT Levels,
										       pCreateTextureArgs->Usage,  // DWORD Usage,
										       pCreateTextureArgs->Format, // D3DMFORMAT Format,
										       pCreateTextureArgs->Pool,   // D3DMPOOL Pool,
										       &ppTextures[uiResIter])))   // LPDIRECT3DMOBILETEXTURE* ppTexture,
		{
			DebugOut(DO_ERROR, L"CreateTexture failed. (hr = 0x%08x)", hr);
			goto cleanup;
		}
	}

cleanup:

	if (FAILED(hr))
	{
		if (ppTextures)
		{
			for (uiResIter = 0; uiResIter < uiResourceCount; uiResIter++)
			{
				if (ppTextures[uiResIter])
				{
					ppTextures[uiResIter]->Release();
				}
			}
			free(ppTextures);
		}
		*pppInterfaces = NULL;
	}
	else
	{
		*pppInterfaces = (LPDIRECT3DMOBILERESOURCE*)ppTextures;
	}
	return hr;
}


//
// GetValidTextureFormat
//
//   Finds a valid format for textures, for the current device instance
//   and display mode.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Direct3D device to consider
//   D3DMFORMAT *pFormat:  Output value, valid texture format
//
// Return Value:
//
//   HRESULT indicates success or failure
//
HRESULT GetValidTextureFormat(LPDIRECT3DMOBILEDEVICE pDevice, D3DMFORMAT *pFormat)
{
	//
	// Toggled when a valid texture format is discovered
	//
	bool bFoundValidFormat = false;	

	//
	// D3DMFORMAT iterator; to cycle through formats until a valid format
	// is found.
	//
	D3DMFORMAT FormatIter;

	//
	// The display mode's spatial resolution, color resolution,
	// and refresh frequency.
	//
	D3DMDISPLAYMODE Mode;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Direct3D Object
	//
	LPDIRECT3DMOBILE pD3D = NULL;

	//
	// Result; all failure conditions set this to an error
	//
	HRESULT hr = S_OK;

	//
	// Calling GetDirect3D increases the reference count on the interface
	//
	if (FAILED(hr = pDevice->GetDirect3D(&pD3D))) // LPDIRECT3DMOBILE *ppD3D
	{
		DebugOut(DO_ERROR, L"GetDirect3D failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Retrieve current display mode
	//
	if (FAILED(hr = pDevice->GetDisplayMode(&Mode))) // D3DMDISPLAYMODE* pMode
	{
		DebugOut(DO_ERROR, L"GetDisplayMode failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Get device capabilities
	//
	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Find a format supported for textures
	//
	for (FormatIter = (D3DMFORMAT)0; FormatIter < D3DMFMT_NUMFORMAT; (*(ULONG*)(&FormatIter))++)
	{
		if (!(FAILED(pD3D->CheckDeviceFormat(Caps.AdapterOrdinal, // UINT Adapter
									         Caps.DeviceType,     // D3DMDEVTYPE DeviceType
									         Mode.Format,         // D3DMFORMAT AdapterFormat
									         0,                   // ULONG Usage
									         D3DMRTYPE_TEXTURE,   // D3DMRESOURCETYPE RType
									         FormatIter))))       // D3DMFORMAT CheckFormat
		{
			bFoundValidFormat = true;
			break;
		}
	}

	if (false == bFoundValidFormat)
	{
		DebugOut(DO_ERROR, L"No valid texture format.");
		hr = E_FAIL;
		goto cleanup;
	}


cleanup:

	if (pD3D)
		pD3D->Release();

	if (FAILED(hr))
	{
		*pFormat = D3DMFMT_UNKNOWN;
		return E_FAIL;
	}
	else
	{
		*pFormat = FormatIter;
		return S_OK;
	}
}


// 
// GetBytesPerPixel
// 
//   Indicates the number of bytes occupied by one instance of the 
//   specified pixel format.  This test case only uses the first
//   available RGB, xRGB, or ARGB format; thus, those are the only
//   formats handled by this function.
//
// Arguments:
// 
//   D3DMFORMAT Format:  Format size to query
//   UINT *puiBytesPerPixel:  Output of size for queried format
// 
// Return Value:
// 
//   HRESULT indicates success or failure
// 
HRESULT GetBytesPerPixel(D3DMFORMAT Format, UINT *puiBytesPerPixel)
{
	switch(Format)
	{
	case D3DMFMT_R8G8B8:
		*puiBytesPerPixel = 3;
		break;
	case D3DMFMT_A8R8G8B8:
		*puiBytesPerPixel = 4;
		break;
	case D3DMFMT_X8R8G8B8:
		*puiBytesPerPixel = 4;
		break;
	case D3DMFMT_R5G6B5:
		*puiBytesPerPixel = 2;
		break;
	case D3DMFMT_X1R5G5B5:
		*puiBytesPerPixel = 2;
		break;
	case D3DMFMT_A1R5G5B5:
		*puiBytesPerPixel = 2;
		break;
	case D3DMFMT_A4R4G4B4:
		*puiBytesPerPixel = 2;
		break;
	case D3DMFMT_R3G3B2:
		*puiBytesPerPixel = 1;
		break;
	case D3DMFMT_A8R3G3B2:
		*puiBytesPerPixel = 2;
		break;
	case D3DMFMT_X4R4G4B4:
		*puiBytesPerPixel = 2;
		break;
	default:
		// Unknown format
		return E_FAIL;
	}

	return S_OK;
}

//
// SupportsManagedPool
//
//   Verifies that a device supports the managed pool.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Direct3D device to inspect
//
// Return Value:
//
//   HRESULT indicates success if managed pool is supported; failure otherwise
//
HRESULT SupportsManagedPool(LPDIRECT3DMOBILEDEVICE pDevice)
{
    HRESULT hr;
    
	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Get device capabilities
	//
	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x)", hr);
		return hr;
	}

	//
	// Does device support managed pool
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
// SupportsTextures
//
//   Verifies that a device supports textures
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Direct3D device to inspect
//
// Return Value:
//
//   HRESULT indicates success if textures are supported; failure otherwise
//
HRESULT SupportsTextures(LPDIRECT3DMOBILEDEVICE pDevice)
{
    HRESULT hr;
    
	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Get device capabilities
	//
	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x)", hr);
		return hr;
	}

	//
	// Does device support textures?
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
