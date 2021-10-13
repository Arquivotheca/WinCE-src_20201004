//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <d3dm.h>
#include <tchar.h>


//
//  CreateActiveBuffer
//
//    Creates a index buffer of the specified index size and index count; sets to active.
//
//  Arguments:
//
//    LPDIRECT3DMOBILEDEVICE pd3dDevice: A valid Direct3D device
//    UINT uiNumIndices: Number of indices to allocate in the buffer
//    DWORD uiIndexSize: Size of a index
//    DWORD dwUsage:  Vertex buffer usage flag(s).
//    D3DMPOOL D3DMPOOL:  Pool to create buffer in
//    
//  Return Value:
//
//    LPDIRECT3DMOBILEINDEXBUFFER:  The resultant index buffer; or NULL if failed
//
LPDIRECT3DMOBILEINDEXBUFFER CreateActiveBuffer(LPDIRECT3DMOBILEDEVICE pd3dDevice,
                                               UINT uiNumIndices,
                                               UINT uiIndexSize,
                                               DWORD dwUsage,
                                               D3DMPOOL D3DMPOOL)
{
	LPDIRECT3DMOBILEINDEXBUFFER pIB = NULL;
	D3DMFORMAT IndexFormat;
	
	if ((NULL == pd3dDevice) || (0 == uiNumIndices) || (0 == uiIndexSize))
	{
		OutputDebugString(_T("CreateActiveBuffer:  Aborting due to invalid argument(s)."));
		return NULL;
	}

	switch(uiIndexSize)
	{
	case sizeof(WORD): 
		IndexFormat = D3DMFMT_INDEX16;
		break;
	case sizeof(DWORD): 
		IndexFormat = D3DMFMT_INDEX32;
		break;
	default:
		return NULL;
	}

    if( FAILED( pd3dDevice->CreateIndexBuffer( uiNumIndices*uiIndexSize, // UINT Length,
	                                           dwUsage,                  // DWORD Usage,
	                                           IndexFormat,              // D3DMFORMAT Format,
	                                           D3DMPOOL,                 // D3DMPOOL Pool,
	                                           &pIB)))                   // LPDIRECT3DMOBILEINDEXBUFFER* ppIndexBuffer,
    {
		OutputDebugString(_T("CreateActiveBuffer: Aborting due to failure in index buffer creation."));
		return NULL;
    }

	if (FAILED(pd3dDevice->SetIndices( pIB )))
	{
		OutputDebugString(_T("SetIndices failed."));
		pIB->Release();
		return NULL;
	}

	// Success condition (all failures are handled with early returns)
	return pIB;
}

//
//  CreateActiveBuffer
//
//    Creates a index buffer of the specified index size and index count; sets to active.
//
//    Stores the buffer in a supported pool, based on D3DMCAPS.
//
//  Arguments:
//
//    LPDIRECT3DMOBILEDEVICE pd3dDevice: A valid Direct3D device
//    UINT uiNumIndices: Number of indices to allocate in the buffer
//    UINT uiIndexSize:  Per-index size
//    DWORD dwUsage:  Vertex buffer usage flag(s).
//    
//  Return Value:
//
//    LPDIRECT3DMOBILEINDEXBUFFER:  The resultant index buffer; or NULL if failed
//
LPDIRECT3DMOBILEINDEXBUFFER CreateActiveBuffer(LPDIRECT3DMOBILEDEVICE pd3dDevice,
                                               UINT uiNumIndices,
                                               UINT uiIndexSize,
                                               DWORD dwUsage)
{

	// 
    // Device Capabilities
    // 
    D3DMCAPS Caps;
	
    // 
    // Query the device's capabilities
    // 
	if( FAILED( pd3dDevice->GetDeviceCaps(&Caps)))
	{
		return NULL;
	}

	//
	// Branch based on pool
	//
	if (D3DMSURFCAPS_SYSINDEXBUFFER & Caps.SurfaceCaps)
	{
		return CreateActiveBuffer(pd3dDevice, uiNumIndices, uiIndexSize, dwUsage, D3DMPOOL_SYSTEMMEM);
	}
	else if (D3DMSURFCAPS_VIDINDEXBUFFER & Caps.SurfaceCaps)
	{
		return CreateActiveBuffer(pd3dDevice, uiNumIndices, uiIndexSize, dwUsage, D3DMPOOL_VIDEOMEM);
	}
	else
	{
		return NULL;
	}

}



//
// CreateFilledIndexBuffer
//   
//   Given data, and specs for a index buffer, creates the index buffer
//   and fills it with the data.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying device, with which to create a IB
//   LPDIRECT3DMOBILEINDEXBUFFER *ppIB:  Output for IB interface pointer
//   BYTE *pIndices:  Index data to fill VB with
//   UINT uiIndexSize:  Per-index size
//   UINT uiNumIndices:  Number of indices in input data
//   
// Return Value
// 
//   HRESULT indicates success or failure
//   
HRESULT CreateFilledIndexBuffer(LPDIRECT3DMOBILEDEVICE pDevice,
                                LPDIRECT3DMOBILEINDEXBUFFER *ppIB,
                                BYTE *pIndices,
                                UINT uiIndexSize,
                                UINT uiNumIndices)
{
	//
	// All failure conditions specifically set this to an error
	//
	HRESULT hr = S_OK;

	//
	// Pointer for buffer locks
	// 
	BYTE *pByte;

	//
	// Create a vertex buffer; pool detected automatically, SetStreamSource called
	// automatically
	//
	(*ppIB) = CreateActiveBuffer(pDevice,       // LPDIRECT3DMOBILEDEVICE pd3dDevice
	                             uiNumIndices,  // UINT uiNumIndices,
	                             uiIndexSize,   // UINT uiIndexSize,
	                             0);            // DWORD dwUsage

	if (NULL == (*ppIB))
	{
		OutputDebugString(_T("CreateActiveBuffer failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Fill the vertex buffer with data that has already been generated
	//
	if (FAILED((*ppIB)->Lock(0, uiNumIndices*uiIndexSize, (VOID**)&pByte, 0)))
	{
		OutputDebugString(_T("Lock failed."));
		hr = E_FAIL;
		goto cleanup;
	}
	memcpy(pByte, pIndices, uiNumIndices*uiIndexSize);
	if (FAILED((*ppIB)->Unlock()))
	{
		OutputDebugString(_T("Unlock failed."));
		hr = E_FAIL;
		goto cleanup;
	}

cleanup:

	if ((FAILED(hr)) && (NULL != *ppIB))
		(*ppIB)->Release();

	return hr;	
}
