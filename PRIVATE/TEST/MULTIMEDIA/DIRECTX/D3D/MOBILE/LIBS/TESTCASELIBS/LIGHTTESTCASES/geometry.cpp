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
#include "geometry.h"
#include "QAD3DMX.h"
#include <d3dmx.h>
#include "qamath.h"

//
// SetupRotatedQuad
//
//   Create a quad of W 1.0f x H 1.0f, centered at the origin and rotated
//   by the specified angle.  Colors, assumed to be used for flat shading
//   are stored as passed in the arguments.
//
// Arguments:
//
//   LPD3DM_LIGHTTEST_VERTS pVerts:  Storage for generated geometry.
//
//   float fAngleX \
//   float fAngleY   Angle of rotation
//   float fAngleZ /
//
//   D3DMCOLOR TriDiffuse1  \
//   D3DMCOLOR TriSpecular1  \  Vertex colors for each
//   D3DMCOLOR TriDiffuse2,  /  tri that makes up the quad
//   D3DMCOLOR TriSpecular2 /
//   
// Return Value:
//   
//   HRESULT indicates success or failure
//
HRESULT SetupRotatedQuad(LPD3DM_LIGHTTEST_VERTS pVerts,
                         float fAngleX, float fAngleY, float fAngleZ,
						 D3DMCOLOR TriDiffuse1, D3DMCOLOR TriSpecular1,
						 D3DMCOLOR TriDiffuse2, D3DMCOLOR TriSpecular2
						 )
{
	const float fQuadExtent = 1.0f;  // W x H == 1.0f x 1.0f
	D3DMMATRIX RotMatrix;	
	UINT uiVertIter;

	//
	// Initial position of quad vertices; prior to rotation
	//
	CONST D3DMVECTOR QuadPos[D3DQA_VERTS_PER_QUAD]={
		//     X         Y         Z
		{ _M(-1.0f),_M( 1.0f),_M( 0.0f) },
		{ _M( 1.0f),_M( 1.0f),_M( 0.0f) },
		{ _M( 1.0f),_M(-1.0f),_M( 0.0f) },
		{ _M( 1.0f),_M(-1.0f),_M( 0.0f) },
		{ _M(-1.0f),_M(-1.0f),_M( 0.0f) },
		{ _M(-1.0f),_M( 1.0f),_M( 0.0f) }
	};

	//
	// Initial vectors for vertex normals; prior to rotation
	//
	CONST D3DMVECTOR QuadNorm[D3DQA_VERTS_PER_QUAD]={
		//     X         Y         Z
		{ _M( 0.0f),_M( 0.0f),_M(-1.0f) },
		{ _M( 0.0f),_M( 0.0f),_M(-1.0f) },
		{ _M( 0.0f),_M( 0.0f),_M(-1.0f) },
		{ _M( 0.0f),_M( 0.0f),_M(-1.0f) },
		{ _M( 0.0f),_M( 0.0f),_M(-1.0f) },
		{ _M( 0.0f),_M( 0.0f),_M(-1.0f) }
	};

	//
	// Fill output vertices based on above constants; scale X and Y quad extents
	// to width and height of 1.0f
	//
	for (uiVertIter = 0; uiVertIter < D3DQA_VERTS_PER_QUAD; uiVertIter++)
	{
		pVerts[uiVertIter].x = *(float*)&QuadPos[uiVertIter].x * (fQuadExtent / 2.0f);
		pVerts[uiVertIter].y = *(float*)&QuadPos[uiVertIter].y * (fQuadExtent / 2.0f);
		pVerts[uiVertIter].z = *(float*)&QuadPos[uiVertIter].z;
		pVerts[uiVertIter].nx = *(float*)&QuadNorm[uiVertIter].x;
		pVerts[uiVertIter].ny = *(float*)&QuadNorm[uiVertIter].y;
		pVerts[uiVertIter].nz = *(float*)&QuadNorm[uiVertIter].z;
		
		//
		// 2/3 of vert colors are irrelevent for flat shading; those that matter are handled
		// after this loop.
		//
		pVerts[uiVertIter].Specular = pVerts[uiVertIter].Diffuse = D3DMCOLOR_XRGB(0,255,0);
	}

	//
	// Set unique colors for verts that are relavent to flat shading
	//
	pVerts[0].Specular = TriSpecular1;
	pVerts[0].Diffuse = TriDiffuse1;

	pVerts[3].Specular = TriSpecular2;
	pVerts[3].Diffuse = TriDiffuse2;

	//
	// Generate a rotation matrix
	//
	if (false == GetRotation(D3DQA_ROTTYPE_XYZ, // D3DQA_ROTTYPE rType,
	                         fAngleX,           // const float fXRot,
	                         fAngleY,           // const float fYRot,
	                         fAngleZ,           // const float fZRot,
	                         &RotMatrix))       // D3DMMATRIX* const RotMatrix
		return E_FAIL;

	//
	// Transform quad through rotation matrix
	//
	*(D3DMVECTOR*)(&pVerts[0].x) = TransformVector((D3DMVECTOR*)(&pVerts[0].x), &RotMatrix);
	*(D3DMVECTOR*)(&pVerts[1].x) = TransformVector((D3DMVECTOR*)(&pVerts[1].x), &RotMatrix);
	*(D3DMVECTOR*)(&pVerts[2].x) = TransformVector((D3DMVECTOR*)(&pVerts[2].x), &RotMatrix);
	*(D3DMVECTOR*)(&pVerts[3].x) = TransformVector((D3DMVECTOR*)(&pVerts[3].x), &RotMatrix);
	*(D3DMVECTOR*)(&pVerts[4].x) = TransformVector((D3DMVECTOR*)(&pVerts[4].x), &RotMatrix);
	*(D3DMVECTOR*)(&pVerts[5].x) = TransformVector((D3DMVECTOR*)(&pVerts[5].x), &RotMatrix);

	//
	// Transform quad normals through rotation matrix
	//
	*(D3DMVECTOR*)(&pVerts[0].nx) = TransformVector((D3DMVECTOR*)(&pVerts[0].nx), &RotMatrix);
	*(D3DMVECTOR*)(&pVerts[1].nx) = TransformVector((D3DMVECTOR*)(&pVerts[1].nx), &RotMatrix);
	*(D3DMVECTOR*)(&pVerts[2].nx) = TransformVector((D3DMVECTOR*)(&pVerts[2].nx), &RotMatrix);
	*(D3DMVECTOR*)(&pVerts[3].nx) = TransformVector((D3DMVECTOR*)(&pVerts[3].nx), &RotMatrix);
	*(D3DMVECTOR*)(&pVerts[4].nx) = TransformVector((D3DMVECTOR*)(&pVerts[4].nx), &RotMatrix);
	*(D3DMVECTOR*)(&pVerts[5].nx) = TransformVector((D3DMVECTOR*)(&pVerts[5].nx), &RotMatrix);

	return S_OK;
}

//
// SetupOffsetQuad
//
//   Take existing quad data, and apply a simple translation.
//
// Arguments:
//
//   LPD3DM_LIGHTTEST_VERTS pVerts:  Vertex data to be translated.
//
//   float fX \
//   float fY   Translation values along each axis
//   float fZ /
//   
// Return Value:
//   
//   HRESULT indicates success or failure
//
HRESULT SetupOffsetQuad(LPD3DM_LIGHTTEST_VERTS pVerts,
                        float fX, float fY, float fZ)
{

	pVerts[0].x += fX;
	pVerts[0].y += fY;
	pVerts[0].z += fZ;

	pVerts[1].x += fX;
	pVerts[1].y += fY;
	pVerts[1].z += fZ;

	pVerts[2].x += fX;
	pVerts[2].y += fY;
	pVerts[2].z += fZ;

	pVerts[3].x += fX;
	pVerts[3].y += fY;
	pVerts[3].z += fZ;

	pVerts[4].x += fX;
	pVerts[4].y += fY;
	pVerts[4].z += fZ;

	pVerts[5].x += fX;
	pVerts[5].y += fY;
	pVerts[5].z += fZ;

	return S_OK;
}



//
// SetupScaleQuad
//
//   Take existing quad data, and apply a simple scaling.
//
// Arguments:
//
//   LPD3DM_LIGHTTEST_VERTS pVerts:  Vertex data to be scaled.
//
//   float fX \
//   float fY   Scale values for each axis
//   float fZ /
//   
// Return Value:
//   
//   HRESULT indicates success or failure
//
HRESULT SetupScaleQuad(LPD3DM_LIGHTTEST_VERTS pVerts,
                       float fX, float fY, float fZ)
{
    D3DMXMATRIX ScaleMatrix;
    D3DMXMATRIX InverseScale;
    D3DMXMATRIX NormalScaleMatrix;

    D3DMXMatrixIdentity(&ScaleMatrix);
    D3DMXMatrixScaling(&ScaleMatrix, fX, fY, fZ);

    D3DMXMatrixInverse(&InverseScale, NULL, &ScaleMatrix);
    D3DMXMatrixTranspose(&NormalScaleMatrix, &InverseScale);
    
	*(D3DMVECTOR*)(&pVerts[0].x) = TransformVector((D3DMVECTOR*)(&pVerts[0].x), (D3DMMATRIX*)&ScaleMatrix);
	*(D3DMVECTOR*)(&pVerts[1].x) = TransformVector((D3DMVECTOR*)(&pVerts[1].x), (D3DMMATRIX*)&ScaleMatrix);
	*(D3DMVECTOR*)(&pVerts[2].x) = TransformVector((D3DMVECTOR*)(&pVerts[2].x), (D3DMMATRIX*)&ScaleMatrix);
	*(D3DMVECTOR*)(&pVerts[3].x) = TransformVector((D3DMVECTOR*)(&pVerts[3].x), (D3DMMATRIX*)&ScaleMatrix);
	*(D3DMVECTOR*)(&pVerts[4].x) = TransformVector((D3DMVECTOR*)(&pVerts[4].x), (D3DMMATRIX*)&ScaleMatrix);
	*(D3DMVECTOR*)(&pVerts[5].x) = TransformVector((D3DMVECTOR*)(&pVerts[5].x), (D3DMMATRIX*)&ScaleMatrix);

	*(D3DMVECTOR*)(&pVerts[0].nx) = TransformVector((D3DMVECTOR*)(&pVerts[0].nx), (D3DMMATRIX*)&NormalScaleMatrix);
	*(D3DMVECTOR*)(&pVerts[1].nx) = TransformVector((D3DMVECTOR*)(&pVerts[1].nx), (D3DMMATRIX*)&NormalScaleMatrix);
	*(D3DMVECTOR*)(&pVerts[2].nx) = TransformVector((D3DMVECTOR*)(&pVerts[2].nx), (D3DMMATRIX*)&NormalScaleMatrix);
	*(D3DMVECTOR*)(&pVerts[3].nx) = TransformVector((D3DMVECTOR*)(&pVerts[3].nx), (D3DMMATRIX*)&NormalScaleMatrix);
	*(D3DMVECTOR*)(&pVerts[4].nx) = TransformVector((D3DMVECTOR*)(&pVerts[4].nx), (D3DMMATRIX*)&NormalScaleMatrix);
	*(D3DMVECTOR*)(&pVerts[5].nx) = TransformVector((D3DMVECTOR*)(&pVerts[5].nx), (D3DMMATRIX*)&NormalScaleMatrix);

	return S_OK;
}

