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
//    Creates a vertex buffer of the specified FVF and FVF size (automatic FVF size computation
//    is not yet included in this function), then does SetStreamSource and SetVertexShader on
//    the new vertex buffer
//
//  Arguments:
//
//    LPDIRECT3DMOBILEDEVICE pd3dDevice: A valid Direct3D device
//    UINT uiNumVerts: Number of vertices to allocate in the buffer
//    DWORD dwFVF: FVF bit-mask
//    DWORD dwFVFSize: Size of a vertex of this FVF type
//    DWORD dwUsage:  Vertex buffer usage flag(s).
//    
//  Return Value:
//
//    LPDIRECT3DMOBILEVERTEXBUFFER:  The resultant vertex buffer; or NULL if failed
//
LPDIRECT3DMOBILEVERTEXBUFFER CreateActiveBuffer(LPDIRECT3DMOBILEDEVICE pd3dDevice, UINT uiNumVerts, DWORD dwFVF,
										        DWORD dwFVFSize, DWORD dwUsage, D3DMPOOL d3dPool)
{
	LPDIRECT3DMOBILEVERTEXBUFFER pVB = NULL;
	
	//
	// Bad-parameter checks.
	//
	// Note that an FVF of 0x00000000 is valid
	//
	if ((NULL == pd3dDevice) || (0 == uiNumVerts) || (0 == dwFVFSize))
	{
		OutputDebugString(_T("CreateActiveBuffer:  Aborting due to invalid argument(s)."));
		return NULL;
	}

    if( FAILED( pd3dDevice->CreateVertexBuffer( uiNumVerts*dwFVFSize, dwUsage, dwFVF, d3dPool, &pVB ) ) )
    {
		OutputDebugString(_T("CreateActiveBuffer: Aborting due to failure in vertex buffer creation."));
		return NULL;
    }

	if (FAILED(pd3dDevice->SetStreamSource( 0, pVB, 0 )))
	{
		OutputDebugString(_T("CreateActiveBuffer: Aborting due to failure in setting stream source."));
		pVB->Release();
		return NULL;
	}

	// Success condition (all failures are handled with early returns)
	return pVB;
}



//
//  CreateActiveBuffer
//
//    Creates a vertex buffer of the specified FVF and FVF size (automatic FVF size computation
//    is not yet included in this function), then does SetStreamSource and SetVertexShader on
//    the new vertex buffer.
//
//    Stores the buffer in a supported pool, based on D3DMCAPS.
//
//  Arguments:
//
//    LPDIRECT3DMOBILEDEVICE pd3dDevice: A valid Direct3D device
//    UINT uiNumVerts: Number of vertices to allocate in the buffer
//    DWORD dwFVF: FVF bit-mask
//    DWORD dwFVFSize: Size of a vertex of this FVF type
//    DWORD dwUsage:  Vertex buffer usage flag(s).
//    
//  Return Value:
//
//    LPDIRECT3DMOBILEVERTEXBUFFER:  The resultant vertex buffer; or NULL if failed
//
LPDIRECT3DMOBILEVERTEXBUFFER CreateActiveBuffer(LPDIRECT3DMOBILEDEVICE pd3dDevice, UINT uiNumVerts, DWORD dwFVF,
										        DWORD dwFVFSize, DWORD dwUsage)
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
	if (D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps)
	{
		return CreateActiveBuffer(pd3dDevice, uiNumVerts, dwFVF,
								  dwFVFSize, dwUsage, D3DMPOOL_SYSTEMMEM);
	}
	else if (D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps)
	{
		return CreateActiveBuffer(pd3dDevice, uiNumVerts, dwFVF,
								  dwFVFSize, dwUsage, D3DMPOOL_VIDEOMEM);
	}
	else
	{
		return NULL;
	}
}


//
// CreateFilledVertexBuffer
//   
//   Given data, and specs for a vertex buffer, creates the vertex buffer
//   and fills it with the data.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying device, with which to create a VB
//   LPDIRECT3DMOBILEVERTEXBUFFER *ppVB:  Output for VB interface pointer
//   BYTE *pVertices:  Vertex data to fill VB with
//   UINT uiVertexSize:  Per-vertex structure size
//   UINT uiNumVertices:  Number of vertices in input data
//   DWORD dwFVF:  FVF mask to associate with vertex buffer
//   
// Return Value
// 
//   HRESULT indicates success or failure
//   
HRESULT CreateFilledVertexBuffer(LPDIRECT3DMOBILEDEVICE pDevice,
                                 LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
                                 BYTE *pVertices,
                                 UINT uiVertexSize,
                                 UINT uiNumVertices,
                                 DWORD dwFVF)
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
	(*ppVB) = CreateActiveBuffer(pDevice,       // LPDIRECT3DMOBILEDEVICE pd3dDevice
	                             uiNumVertices, // UINT uiNumVerts,
	                             dwFVF,         // DWORD dwFVF,
	                             uiVertexSize,  // DWORD dwFVFSize,
	                             0);            // DWORD dwUsage

	if (NULL == (*ppVB))
	{
		OutputDebugString(_T("CreateActiveBuffer failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Fill the vertex buffer with data that has already been generated
	//
	if (FAILED((*ppVB)->Lock(0, uiNumVertices*uiVertexSize, (VOID**)&pByte, 0)))
	{
		OutputDebugString(_T("Lock failed."));
		hr = E_FAIL;
		goto cleanup;
	}
	memcpy(pByte, pVertices, uiNumVertices*uiVertexSize);
	if (FAILED((*ppVB)->Unlock()))
	{
		OutputDebugString(_T("Unlock failed."));
		hr = E_FAIL;
		goto cleanup;
	}

cleanup:

	if ((FAILED(hr)) && (NULL != *ppVB))
		(*ppVB)->Release();

	return hr;	

}

//
// BytesPerVertex
//
//   Inspect a FVF mask and determine the number of required bytes
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  ULONG:  Number of bytes for one instance of this FVF
//
ULONG BytesPerVertex(ULONG FVF)
{
    // Assume the FVF is valid.

    ULONG BytesPerVertex;

    if ((FVF & D3DMFVF_POSITION_MASK) == D3DMFVF_XYZ_FLOAT ||
        (FVF & D3DMFVF_POSITION_MASK) == D3DMFVF_XYZ_FIXED) {

        BytesPerVertex = 12;
    }
    else {

        // Must be D3DMFVF_XYZRHW_*.

        BytesPerVertex = 16;
    }

    if (FVF & D3DMFVF_NORMAL_MASK) {

        // Both normal formats add 12 bytes to the vertex.

        BytesPerVertex += 12;
    }

    // Diffuse and specular colors are 32 bit ARGB values.

    if (FVF & D3DMFVF_DIFFUSE) {

        BytesPerVertex += 4;
    }

    if (FVF & D3DMFVF_SPECULAR) {

        BytesPerVertex += 4;
    }

    // Each vertex may have up to 4 sets of coordinates.
    // Each set of coordinates may have up to 3 values.
    // The format is encoded per coordinate set.

    ULONG i                   = 0;
    ULONG TextureCount        = ((FVF & D3DMFVF_TEXCOUNT_MASK) >> D3DMFVF_TEXCOUNT_SHIFT);
    ULONG DimensionalityMask  = 0x00030000;
    ULONG DimensionalityShift = 16;
    ULONG FormatMask          = 0x03000000;
    ULONG FormatShift         = 24;
    ULONG BytesPerCoord;

    // Iterate over each coordinate set.

    while (i++ < TextureCount) {

        // Compute the format for this set.

        switch ((FVF & FormatMask) >> FormatShift) {

        case D3DMFVF_TEXCOORDFORMAT_FLOAT: 
        case D3DMFVF_TEXCOORDFORMAT_FIXED: BytesPerCoord = 4; break;
        default: BytesPerCoord = 0; break;
        }

        // Move to next potential coordinate set format bits.

        FormatMask <<= 2;
        FormatShift += 2;

        // Compute the dimensionality for this set (How many
        // coordinates in the set.) Add the appropriate number of
        // bytes to the vertex size.

        switch ((FVF & DimensionalityMask) >> DimensionalityShift) {

        case D3DMFVF_TEXCOORDCOUNT1:

            // 1D textures for this stage.

            BytesPerVertex += BytesPerCoord;
            break;

        case D3DMFVF_TEXCOORDCOUNT2:

            // 2D textures for this stage.

            BytesPerVertex += (BytesPerCoord * 2);
            break;

        case D3DMFVF_TEXCOORDCOUNT3:

            // 3D textures for this stage.

            BytesPerVertex += (BytesPerCoord * 3);
            break;
        }

        // Move to the next potential coordinate set dimensionality bits.

        DimensionalityMask <<= 2;
        DimensionalityShift += 2;
    }

    return BytesPerVertex;
}


// 
// FillTLPos
// 
//   Fills X,Y,Z,RHW for a vertex.
// 
// Arguments:
// 
//   BYTE *pVertices:  Pointer to vertex buffer data, at the beginning of a vert
//
//   float fX   \
//   float fY    \ Data with which to initialize
//   float fZ    / vertex components
//   float fRHW /
//   
// Return Value:
//   
//   VOID
// 
VOID FillTLPos(BYTE *pVertices, float fX, float fY, float fZ, float fRHW)
{
	((float*)pVertices)[0] = fX;
	((float*)pVertices)[1] = fY;
	((float*)pVertices)[2] = fZ;
	((float*)pVertices)[3] = fRHW;
}

// 
// RectangularTLTriList
// 
//   Creates a triangle list that covers a rectangular area on an XY plane; filling
//   positional vertex components only
// 
// Arguments:
// 
//   BYTE *pVertices:  Pointer to vertex buffer data, at the beginning of a vert
//   UINT uiX:  X value for corner of rectangle
//   UINT uiY:  Y value for corner of rectangle
//   UINT uiWidth:  Extent, relative to uiX, of rectangle width
//   UINT uiHeight:  Extent, relative to uiX, of rectangle width
//   float fDepth:  Z value of the XY plane for this rectangle
//   UINT uiStride:  Stride between vertices in buffer (size of vertex struct)
//   
// Return Value:
//   
//   VOID
// 
VOID RectangularTLTriList(BYTE *pVertices, UINT uiX, UINT uiY, UINT uiWidth, UINT uiHeight, float fDepth, UINT uiStride)
{
	//
	// Triangle 1
	//
	FillTLPos(pVertices, (float)uiX, (float)uiY, fDepth, 1.0f);
	pVertices+=uiStride;

	FillTLPos(pVertices, (float)(uiX+uiWidth), (float)(uiY+uiHeight), fDepth, 1.0f);
	pVertices+=uiStride;

	FillTLPos(pVertices, (float)uiX, (float)(uiY+uiHeight), fDepth, 1.0f);
	pVertices+=uiStride;

	//
	// Triangle 2
	//
	FillTLPos(pVertices, (float)uiX, (float)uiY, fDepth, 1.0f);
	pVertices+=uiStride;

	FillTLPos(pVertices, (float)(uiX+uiWidth), (float)uiY, fDepth, 1.0f);
	pVertices+=uiStride;

	FillTLPos(pVertices, (float)(uiX+uiWidth), (float)(uiY+uiHeight), fDepth, 1.0f);
	pVertices+=uiStride;
}

// 
// RegularTLTriListTex
// 
//   Fills 2D texture coordinates for triangle list of the format created by RectangularTLTriList,
// 
// Arguments:
// 
//   BYTE *pVertices:  Pointer to vertex buffer data, at the beginning of a vert
//   UINT uiOffset:  Offset into vertex structure, where tex coords can be found
//   UINT uiStride:  Stride between vertices in buffer (size of vertex struct)
//   
// Return Value:
//   
//   VOID
VOID RegularTLTriListTex(BYTE *pVertices, UINT uiOffset, UINT uiStride)
{

	// tu, tv for vertex 1
	((float*)(pVertices+uiOffset))[0] = 0.0f;
	((float*)(pVertices+uiOffset))[1] = 0.0f;
	pVertices += 2*uiStride;

	// tu, tv for vertex 2
	((float*)(pVertices+uiOffset))[0] = 1.0f;
	((float*)(pVertices+uiOffset))[1] = 1.0f;
	pVertices += 2*uiStride;

	// tu, tv for vertex 3
	((float*)(pVertices+uiOffset))[0] = 0.0f;
	((float*)(pVertices+uiOffset))[1] = 1.0f;
	pVertices += 2*uiStride;

	// tu, tv for vertex 4
	((float*)(pVertices+uiOffset))[0] = 0.0f;
	((float*)(pVertices+uiOffset))[1] = 0.0f;
	pVertices += 2*uiStride;

	// tu, tv for vertex 5
	((float*)(pVertices+uiOffset))[0] = 1.0f;
	((float*)(pVertices+uiOffset))[1] = 0.0f;
	pVertices += 2*uiStride;

	// tu, tv for vertex 6
	((float*)(pVertices+uiOffset))[0] = 1.0f;
	((float*)(pVertices+uiOffset))[1] = 1.0f;
	//pVertices += 2*uiStride;

}

// 
// ColorVertRange
// 
//   Sets up the test case, which will be put to use in the render loop.
// 
// Arguments:
// 
//   BYTE *pVertices:  Pointer to vertex buffer data, at the beginning of a vert
//   UINT uiCount:  Number of consecutive vertices to color
//   DWORD dwColor:  Color to apply to vertices
//   UINT uiStride:  Stride between vertices in buffer (size of vertex struct)
//   UINT uiOffset:  Offset into vertex, for color field to write on
//   
// Return Value:
//   
//   VOID
// 
VOID ColorVertRange(BYTE *pVertices, UINT uiCount, DWORD dwColor, UINT uiStride, UINT uiOffset)
{
	UINT uiIter;

	for(uiIter = 0; uiIter < uiCount; uiIter++)
	{
		*(DWORD*)(pVertices+uiOffset) = dwColor;
		pVertices += uiStride;
	}
}
