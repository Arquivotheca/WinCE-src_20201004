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
#include <tchar.h>
#include "Geometry.h"
#include "BufferTools.h"
#include "DebugOutput.h"

//
// Vertex colors to use during primitive generation
//
#define D3DQA_DIFFUSE_COLOR1 0xFFFF0000
#define D3DQA_DIFFUSE_COLOR2 0xFF0000FF
#define D3DQA_DIFFUSE_COLOR3 0xFF00FF00
#define D3DQA_SPECULAR_COLOR1 0xFF0000FF
#define D3DQA_SPECULAR_COLOR2 0xFF00FF00
#define D3DQA_SPECULAR_COLOR3 0xFFFF0000

#define D3DQA_PI    ((FLOAT)  3.141592654f)

//
// Number of lines to generate (single lines are not conducive to thoroughly
// validating rendered output via image comparison)
//
#define D3DQA_NUM_LINES 100

//
// SetupClipGeometry
//     
//   Generates primitives that force clipping to the specified frustum plane
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying Direct3D device
//   PERSPECTIVE_PROJECTION_SPECS *pProjection:  Specifications of persp proj transform
//   UINT uiMaxIter:  Number of iterations that the caller is executing, for the current test case
//   UINT uiCurIter:  Current iteration number that the caller is executing
//   PRIM_DIRECTION PrimDir:  Direction to "point" the primitives (offers variety in test cases)
//   TRANSLATE_DIR TranslateDir:  Indicates frustum clip plane to violate
//   GEOMETRY_DESC *pDesc:  Description of generated geometry
//   D3DMPRIMITIVETYPE PrimType:  Indicates whether the caller would like lines or triangles
//   
// Return Value
// 
//   HRESULT indicates success or failure
//   
HRESULT SetupClipGeometry(LPDIRECT3DMOBILEDEVICE pDevice,
                          PERSPECTIVE_PROJECTION_SPECS *pProjection,
                          UINT uiMaxIter,
                          UINT uiCurIter,
                          PRIM_DIRECTION PrimDir,
                          TRANSLATE_DIR TranslateDir,
                          GEOMETRY_DESC *pDesc,
                          D3DMPRIMITIVETYPE PrimType)
{

	//
	// Plane extents for an XY plane that is halfway between the front view frustum plane
	// and the back view frustum plane.
	//
	// Note that computations are simplified by the assumption of an aspect ratio of 1.0f
	//
	struct _PlaneExtentsXY {
		float fDepth;
		float fLeft; 
		float fTop; 
		float fRight; 
		float fBottom; 
	} PlaneExtentsXY = 
	{
	  pProjection->fNearZ + ((pProjection->fFarZ - pProjection->fNearZ) * 0.5f), // PlaneExtentsXY.fDepth 
	 -PlaneExtentsXY.fDepth * (float)tan (pProjection->fFOV / 2.0f),             // PlaneExtentsXY.fLeft  
	  PlaneExtentsXY.fDepth * (float)tan (pProjection->fFOV / 2.0f),             // PlaneExtentsXY.fTop    
	  PlaneExtentsXY.fDepth * (float)tan (pProjection->fFOV / 2.0f),             // PlaneExtentsXY.fRight 
	 -PlaneExtentsXY.fDepth * (float)tan (pProjection->fFOV / 2.0f)              // PlaneExtentsXY.fBottom
	};

	//
	// Frustum extents on the YZ plane that is vertically level with the bottom of the front view plane
	//
	// Note that computations are simplified by the assumption of an aspect ratio of 1.0f
	//
	struct _FrustumExtentsYZ {
		float fY;
		float fFrontLeftX;  
		float fFrontRightX; 
		float fBackLeftX;  
		float fBackRightX; 
		float fFrontZ;  
		float fBackZ; 
	} PlaneExtentsYZ = 
	{
	 -pProjection->fNearZ * (float)tan (pProjection->fFOV / 2.0f), // float fY;
     -pProjection->fNearZ * (float)tan (pProjection->fFOV / 2.0f), // float fFrontLeftX;  
	  pProjection->fNearZ * (float)tan (pProjection->fFOV / 2.0f), // float fFrontRightX; 
	 -pProjection->fFarZ  * (float)tan (pProjection->fFOV / 2.0f), // float fBackLeftX;  
	  pProjection->fFarZ  * (float)tan (pProjection->fFOV / 2.0f), // float fBackRightX; 
	  pProjection->fNearZ,                                         // float fFrontZ;  
	  pProjection->fFarZ                                           // float fBackZ; 
	};


	//
	// Pointer for generated vertex data
	// 
	CUSTOM_VERTEX * pVertices = NULL;

	//
	// All failure conditions specifically set this to an error
	//
	HRESULT hr = S_OK;

	//
	// Calculations for generated geometry
	//
	float fIterPercent = ((float)uiCurIter / (float)uiMaxIter);
	float fTranslate;
	float fTemp;
	UINT uiLineIter;
	float fLineInterp;

	//
	// Bad-parameter check
	//
	if ((NULL == pDesc) || (NULL == pDevice))
		return E_FAIL;

	*(pDesc->ppVB) = NULL;

	//
	// Fill caller's variables with description of vertex buffer
	//
	switch(PrimType)
	{
	case D3DMPT_LINELIST:
		pDesc->uiVertCount = D3DQA_NUM_LINES * 2;
		pDesc->uiPrimCount = D3DQA_NUM_LINES;
		break;
	case D3DMPT_TRIANGLELIST:
		pDesc->uiVertCount = 3;
		pDesc->uiPrimCount = 1;
		break;
	default:			
		DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown type."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Allocate memory for vertices
	//
	pVertices = (CUSTOM_VERTEX *)malloc(sizeof(CUSTOM_VERTEX) * (pDesc->uiVertCount));
	if (NULL == pVertices)
	{
		DebugOut(DO_ERROR, _T("SetupClipGeometry: malloc failed"));
		hr = E_FAIL;
		goto cleanup;
	}

	switch(TranslateDir)
	{
	case TRANSLATE_TO_NEGX:

		//
		// The extent to which the triangle is outside of the left frustum plane is directly related to
		// iteration number.
		//
		fTranslate = -(PlaneExtentsXY.fRight - PlaneExtentsXY.fLeft) * (fIterPercent);
		PlaneExtentsXY.fLeft  += fTranslate;
		PlaneExtentsXY.fRight += fTranslate;

		switch(PrimType)
		{
		case D3DMPT_LINELIST:
			switch(PrimDir)
			{
			case PRIM_DIR1:
				for(uiLineIter = 0; uiLineIter < D3DQA_NUM_LINES; uiLineIter++)
				{
					fLineInterp = (float)uiLineIter / ((float)D3DQA_NUM_LINES-1.0f);
					fTemp = PlaneExtentsXY.fBottom + (PlaneExtentsXY.fTop - PlaneExtentsXY.fBottom)*fLineInterp;
					FillUntransformedVertex(&pVertices[uiLineIter*2  ], PlaneExtentsXY.fLeft,      0, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR1, D3DQA_SPECULAR_COLOR1);
					FillUntransformedVertex(&pVertices[uiLineIter*2+1],
											PlaneExtentsXY.fRight,
											fTemp,
											PlaneExtentsXY.fDepth,
											InterpColor(fLineInterp, D3DQA_DIFFUSE_COLOR2, D3DQA_DIFFUSE_COLOR3),
											InterpColor(fLineInterp, D3DQA_SPECULAR_COLOR2, D3DQA_SPECULAR_COLOR3));
				}
				break;
			case PRIM_DIR2:
				for(uiLineIter = 0; uiLineIter < D3DQA_NUM_LINES; uiLineIter++)
				{
					fLineInterp = (float)uiLineIter / ((float)D3DQA_NUM_LINES-1.0f);
					fTemp = PlaneExtentsXY.fBottom + (PlaneExtentsXY.fTop - PlaneExtentsXY.fBottom)*fLineInterp;
					FillUntransformedVertex(&pVertices[uiLineIter*2  ],
					                        PlaneExtentsXY.fLeft,
					                        fTemp,
					                        PlaneExtentsXY.fDepth,
					                        D3DQA_DIFFUSE_COLOR1,
					                        D3DQA_SPECULAR_COLOR1);
					FillUntransformedVertex(&pVertices[uiLineIter*2+1],
											PlaneExtentsXY.fRight,
											0,
											PlaneExtentsXY.fDepth,
											InterpColor(fLineInterp, D3DQA_DIFFUSE_COLOR2, D3DQA_DIFFUSE_COLOR3),
											InterpColor(fLineInterp, D3DQA_SPECULAR_COLOR2, D3DQA_SPECULAR_COLOR3));
				}
				break;
			default:			
				DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown style."));
				hr = E_FAIL;
				goto cleanup;
			}
			break;
		case D3DMPT_TRIANGLELIST:
			switch(PrimDir)
			{
			case PRIM_DIR1: // Left-pointing triangle
				FillUntransformedVertex(&pVertices[0], PlaneExtentsXY.fLeft,                       0, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR1, D3DQA_SPECULAR_COLOR1);
				FillUntransformedVertex(&pVertices[1], PlaneExtentsXY.fRight,    PlaneExtentsXY.fTop, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR2, D3DQA_SPECULAR_COLOR2);
				FillUntransformedVertex(&pVertices[2], PlaneExtentsXY.fRight, PlaneExtentsXY.fBottom, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR3, D3DQA_SPECULAR_COLOR3);
				break;
			case PRIM_DIR2: // Right-pointing triangle
				FillUntransformedVertex(&pVertices[0],  PlaneExtentsXY.fLeft,    PlaneExtentsXY.fTop, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR1, D3DQA_SPECULAR_COLOR1);
				FillUntransformedVertex(&pVertices[1], PlaneExtentsXY.fRight,                      0, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR2, D3DQA_SPECULAR_COLOR2);
				FillUntransformedVertex(&pVertices[2],  PlaneExtentsXY.fLeft, PlaneExtentsXY.fBottom, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR3, D3DQA_SPECULAR_COLOR3);
				break;
			default:			
				DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown style."));
				hr = E_FAIL;
				goto cleanup;
			}
			break;
		default:			
			DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown type."));
			hr = E_FAIL;
			goto cleanup;
		}
		break;
	case TRANSLATE_TO_POSX:

		//
		// The extent to which the triangle is outside of the right frustum plane is directly related to
		// iteration number.
		//
		fTranslate = (PlaneExtentsXY.fRight - PlaneExtentsXY.fLeft) * (fIterPercent);
		PlaneExtentsXY.fLeft  += fTranslate;
		PlaneExtentsXY.fRight += fTranslate;

		switch(PrimType)
		{
		case D3DMPT_LINELIST:
			switch(PrimDir)
			{
			case PRIM_DIR1:
				for(uiLineIter = 0; uiLineIter < D3DQA_NUM_LINES; uiLineIter++)
				{
					fLineInterp = (float)uiLineIter / ((float)D3DQA_NUM_LINES-1.0f);
					fTemp = PlaneExtentsXY.fBottom + (PlaneExtentsXY.fTop - PlaneExtentsXY.fBottom)*fLineInterp;
					FillUntransformedVertex(&pVertices[uiLineIter*2],
											PlaneExtentsXY.fLeft,
											fTemp,
											PlaneExtentsXY.fDepth,
											InterpColor(fLineInterp, D3DQA_DIFFUSE_COLOR2, D3DQA_DIFFUSE_COLOR3),
											InterpColor(fLineInterp, D3DQA_SPECULAR_COLOR2, D3DQA_SPECULAR_COLOR3));
					FillUntransformedVertex(&pVertices[uiLineIter*2+1],
					                        PlaneExtentsXY.fRight,
					                        0,
					                        PlaneExtentsXY.fDepth,
					                        D3DQA_DIFFUSE_COLOR1,
					                        D3DQA_SPECULAR_COLOR1);
				}
				break;
			case PRIM_DIR2:
				for(uiLineIter = 0; uiLineIter < D3DQA_NUM_LINES; uiLineIter++)
				{
					fLineInterp = (float)uiLineIter / ((float)D3DQA_NUM_LINES-1.0f);
					fTemp = PlaneExtentsXY.fBottom + (PlaneExtentsXY.fTop - PlaneExtentsXY.fBottom)*fLineInterp;
					FillUntransformedVertex(&pVertices[uiLineIter*2],
											PlaneExtentsXY.fLeft,
											0,
											PlaneExtentsXY.fDepth,
											InterpColor(fLineInterp, D3DQA_DIFFUSE_COLOR2, D3DQA_DIFFUSE_COLOR3),
											InterpColor(fLineInterp, D3DQA_SPECULAR_COLOR2, D3DQA_SPECULAR_COLOR3));
					FillUntransformedVertex(&pVertices[uiLineIter*2+1],
					                        PlaneExtentsXY.fRight,      
					                        fTemp, 
					                        PlaneExtentsXY.fDepth,
					                        D3DQA_DIFFUSE_COLOR1,
					                        D3DQA_SPECULAR_COLOR1);
				}
				break;
			default:			
				DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown style."));
				hr = E_FAIL;
				goto cleanup;
			}
			break;
		case D3DMPT_TRIANGLELIST:
			switch(PrimDir)
			{
			case PRIM_DIR1: // Right-pointing triangle
				FillUntransformedVertex(&pVertices[0],  PlaneExtentsXY.fLeft,    PlaneExtentsXY.fTop, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR1, D3DQA_SPECULAR_COLOR1);
				FillUntransformedVertex(&pVertices[1], PlaneExtentsXY.fRight,                      0, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR2, D3DQA_SPECULAR_COLOR2);
				FillUntransformedVertex(&pVertices[2],  PlaneExtentsXY.fLeft, PlaneExtentsXY.fBottom, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR3, D3DQA_SPECULAR_COLOR3);
				break;
			case PRIM_DIR2: // Left-pointing triangle
				FillUntransformedVertex(&pVertices[0], PlaneExtentsXY.fLeft,                       0, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR1, D3DQA_SPECULAR_COLOR1);
				FillUntransformedVertex(&pVertices[1], PlaneExtentsXY.fRight,    PlaneExtentsXY.fTop, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR2, D3DQA_SPECULAR_COLOR2);
				FillUntransformedVertex(&pVertices[2], PlaneExtentsXY.fRight, PlaneExtentsXY.fBottom, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR3, D3DQA_SPECULAR_COLOR3);
				break;
			default:			
				DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown style."));
				hr = E_FAIL;
				goto cleanup;
			}
			break;
		default:			
			DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown type."));
			hr = E_FAIL;
			goto cleanup;
		}

		break;
	case TRANSLATE_TO_POSY:

		//
		// The extent to which the triangle is outside of the top frustum plane is directly related to
		// iteration number.
		//
		fTranslate = (PlaneExtentsXY.fTop - PlaneExtentsXY.fBottom) * (fIterPercent);
		PlaneExtentsXY.fTop  += fTranslate;
		PlaneExtentsXY.fBottom += fTranslate;

		switch(PrimType)
		{
		case D3DMPT_LINELIST:
			switch(PrimDir)
			{
			case PRIM_DIR1:
				for(uiLineIter = 0; uiLineIter < D3DQA_NUM_LINES; uiLineIter++)
				{


					fLineInterp = (float)uiLineIter / ((float)D3DQA_NUM_LINES-1.0f);
					fTemp = PlaneExtentsXY.fLeft + (PlaneExtentsXY.fRight - PlaneExtentsXY.fLeft)*fLineInterp;
					FillUntransformedVertex(&pVertices[uiLineIter*2],
											fTemp,
											PlaneExtentsXY.fBottom,
											PlaneExtentsXY.fDepth,
											InterpColor(fLineInterp, D3DQA_DIFFUSE_COLOR1, D3DQA_DIFFUSE_COLOR2),
											InterpColor(fLineInterp, D3DQA_SPECULAR_COLOR1, D3DQA_SPECULAR_COLOR2));
					FillUntransformedVertex(&pVertices[uiLineIter*2+1],
											0,
											PlaneExtentsXY.fTop,
											PlaneExtentsXY.fDepth,
											D3DQA_DIFFUSE_COLOR3,
											D3DQA_SPECULAR_COLOR3);
				}
				break;
			case PRIM_DIR2:
				for(uiLineIter = 0; uiLineIter < D3DQA_NUM_LINES; uiLineIter++)
				{


					fLineInterp = (float)uiLineIter / ((float)D3DQA_NUM_LINES-1.0f);
					fTemp = PlaneExtentsXY.fLeft + (PlaneExtentsXY.fRight - PlaneExtentsXY.fLeft)*fLineInterp;
					FillUntransformedVertex(&pVertices[uiLineIter*2],
											0,
											PlaneExtentsXY.fBottom,
											PlaneExtentsXY.fDepth,
											InterpColor(fLineInterp, D3DQA_DIFFUSE_COLOR1, D3DQA_DIFFUSE_COLOR2),
											InterpColor(fLineInterp, D3DQA_SPECULAR_COLOR1, D3DQA_SPECULAR_COLOR2));
					FillUntransformedVertex(&pVertices[uiLineIter*2+1],
											fTemp,
											PlaneExtentsXY.fTop,
											PlaneExtentsXY.fDepth,
											D3DQA_DIFFUSE_COLOR3,
											D3DQA_SPECULAR_COLOR3);
				}
				break;
			default:			
				DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown style."));
				hr = E_FAIL;
				goto cleanup;
			}

			break;
		case D3DMPT_TRIANGLELIST:
			switch(PrimDir)
			{
			case PRIM_DIR1: // Up-pointing triangle
				FillUntransformedVertex(&pVertices[0], PlaneExtentsXY.fRight, PlaneExtentsXY.fBottom, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR1, D3DQA_SPECULAR_COLOR1);
				FillUntransformedVertex(&pVertices[1],  PlaneExtentsXY.fLeft, PlaneExtentsXY.fBottom, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR2, D3DQA_SPECULAR_COLOR2);
				FillUntransformedVertex(&pVertices[2],                     0,    PlaneExtentsXY.fTop, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR3, D3DQA_SPECULAR_COLOR3);
				break;
			case PRIM_DIR2: // Down-pointing triangle
				FillUntransformedVertex(&pVertices[0],  PlaneExtentsXY.fLeft,    PlaneExtentsXY.fTop, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR1, D3DQA_SPECULAR_COLOR1);
				FillUntransformedVertex(&pVertices[1], PlaneExtentsXY.fRight,    PlaneExtentsXY.fTop, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR2, D3DQA_SPECULAR_COLOR2);
				FillUntransformedVertex(&pVertices[2],                     0, PlaneExtentsXY.fBottom, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR3, D3DQA_SPECULAR_COLOR3);
				break;
			default:			
				DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown style."));
				hr = E_FAIL;
				goto cleanup;
			}
			break;
		default:			
			DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown type."));
			hr = E_FAIL;
			goto cleanup;
		}
		
		break;
	case TRANSLATE_TO_NEGY:

		//
		// The extent to which the triangle is outside of the bottom frustum plane is directly related to
		// iteration number.
		//
		fTranslate = -(PlaneExtentsXY.fTop - PlaneExtentsXY.fBottom) * (fIterPercent);
		PlaneExtentsXY.fTop  += fTranslate;
		PlaneExtentsXY.fBottom += fTranslate;

		switch(PrimType)
		{
		case D3DMPT_LINELIST:
			switch(PrimDir)
			{
			case PRIM_DIR1:
				for(uiLineIter = 0; uiLineIter < D3DQA_NUM_LINES; uiLineIter++)
				{
					fLineInterp = (float)uiLineIter / ((float)D3DQA_NUM_LINES-1.0f);
					fTemp = PlaneExtentsXY.fLeft + (PlaneExtentsXY.fRight - PlaneExtentsXY.fLeft)*fLineInterp;
					FillUntransformedVertex(&pVertices[uiLineIter*2],
											fTemp,
											PlaneExtentsXY.fTop,
											PlaneExtentsXY.fDepth,
											InterpColor(fLineInterp, D3DQA_DIFFUSE_COLOR1, D3DQA_DIFFUSE_COLOR2),
											InterpColor(fLineInterp, D3DQA_SPECULAR_COLOR1, D3DQA_SPECULAR_COLOR2));
					FillUntransformedVertex(&pVertices[uiLineIter*2+1],
											0,
											PlaneExtentsXY.fBottom,
											PlaneExtentsXY.fDepth,
											D3DQA_DIFFUSE_COLOR3,
											D3DQA_SPECULAR_COLOR3);
				}
				break;
			case PRIM_DIR2:
				for(uiLineIter = 0; uiLineIter < D3DQA_NUM_LINES; uiLineIter++)
				{
					fLineInterp = (float)uiLineIter / ((float)D3DQA_NUM_LINES-1.0f);
					fTemp = PlaneExtentsXY.fLeft + (PlaneExtentsXY.fRight - PlaneExtentsXY.fLeft)*fLineInterp;
					FillUntransformedVertex(&pVertices[uiLineIter*2],
											0,
											PlaneExtentsXY.fTop,
											PlaneExtentsXY.fDepth,
											InterpColor(fLineInterp, D3DQA_DIFFUSE_COLOR1, D3DQA_DIFFUSE_COLOR2),
											InterpColor(fLineInterp, D3DQA_SPECULAR_COLOR1, D3DQA_SPECULAR_COLOR2));
					FillUntransformedVertex(&pVertices[uiLineIter*2+1],
											fTemp,
											PlaneExtentsXY.fBottom,
											PlaneExtentsXY.fDepth,
											D3DQA_DIFFUSE_COLOR3,
											D3DQA_SPECULAR_COLOR3);
				}
				break;
			default:			
				DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown style."));
				hr = E_FAIL;
				goto cleanup;
			}
			break;
		case D3DMPT_TRIANGLELIST:
			switch(PrimDir)
			{
			case PRIM_DIR1: // Down-pointing triangle
				FillUntransformedVertex(&pVertices[0], PlaneExtentsXY.fRight,    PlaneExtentsXY.fTop, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR1, D3DQA_SPECULAR_COLOR1);
				FillUntransformedVertex(&pVertices[1],                     0, PlaneExtentsXY.fBottom, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR2, D3DQA_SPECULAR_COLOR2);
				FillUntransformedVertex(&pVertices[2],  PlaneExtentsXY.fLeft,    PlaneExtentsXY.fTop, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR3, D3DQA_SPECULAR_COLOR3);
				break;
			case PRIM_DIR2: // Up-pointing triangle
				FillUntransformedVertex(&pVertices[0],  PlaneExtentsXY.fLeft, PlaneExtentsXY.fBottom, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR1, D3DQA_SPECULAR_COLOR1);
				FillUntransformedVertex(&pVertices[1],                     0,    PlaneExtentsXY.fTop, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR2, D3DQA_SPECULAR_COLOR2);
				FillUntransformedVertex(&pVertices[2], PlaneExtentsXY.fRight, PlaneExtentsXY.fBottom, PlaneExtentsXY.fDepth, D3DQA_DIFFUSE_COLOR3, D3DQA_SPECULAR_COLOR3);
				break;
			default:			
				DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown style."));
				hr = E_FAIL;
				goto cleanup;
			}
			break;
		default:			
			DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown type."));
			hr = E_FAIL;
			goto cleanup;
		}
		
		break;
	case TRANSLATE_TO_NEGZ:

		//
		// The extent to which the triangle is outside of the near frustum plane is directly related to
		// iteration number.
		//
		fTranslate = -(PlaneExtentsYZ.fBackZ - PlaneExtentsYZ.fFrontZ) * (fIterPercent);
		PlaneExtentsYZ.fFrontZ  += fTranslate;
		PlaneExtentsYZ.fBackZ   += fTranslate;

		switch(PrimType)
		{
		case D3DMPT_LINELIST:
			switch(PrimDir)
			{
			case PRIM_DIR1:
				for(uiLineIter = 0; uiLineIter < D3DQA_NUM_LINES; uiLineIter++)
				{
					fLineInterp = (float)uiLineIter / ((float)D3DQA_NUM_LINES-1.0f);
					fTemp = PlaneExtentsYZ.fFrontLeftX + (PlaneExtentsYZ.fFrontRightX - PlaneExtentsYZ.fFrontLeftX)*fLineInterp;
					FillUntransformedVertex(&pVertices[uiLineIter*2],
											fTemp,
											PlaneExtentsYZ.fY,
											PlaneExtentsYZ.fBackZ,
											InterpColor(fLineInterp, D3DQA_DIFFUSE_COLOR2, D3DQA_DIFFUSE_COLOR3),
											InterpColor(fLineInterp, D3DQA_SPECULAR_COLOR2, D3DQA_SPECULAR_COLOR3));
					FillUntransformedVertex(&pVertices[uiLineIter*2+1],
											0,
											PlaneExtentsYZ.fY,
											PlaneExtentsYZ.fFrontZ,
											D3DQA_DIFFUSE_COLOR1,
											D3DQA_SPECULAR_COLOR1);
				}
				break;
			case PRIM_DIR2:
				for(uiLineIter = 0; uiLineIter < D3DQA_NUM_LINES; uiLineIter++)
				{
					fLineInterp = (float)uiLineIter / ((float)D3DQA_NUM_LINES-1.0f);
					fTemp = PlaneExtentsYZ.fFrontLeftX + (PlaneExtentsYZ.fFrontRightX - PlaneExtentsYZ.fFrontLeftX)*fLineInterp;
					FillUntransformedVertex(&pVertices[uiLineIter*2],
											0,
											PlaneExtentsYZ.fY,
											PlaneExtentsYZ.fBackZ,
											InterpColor(fLineInterp, D3DQA_DIFFUSE_COLOR2, D3DQA_DIFFUSE_COLOR3),
											InterpColor(fLineInterp, D3DQA_SPECULAR_COLOR2, D3DQA_SPECULAR_COLOR3));
					FillUntransformedVertex(&pVertices[uiLineIter*2+1],
											fTemp,
											PlaneExtentsYZ.fY,
											PlaneExtentsYZ.fFrontZ,
											D3DQA_DIFFUSE_COLOR1,
											D3DQA_SPECULAR_COLOR1);
				}
				break;
			default:			
				DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown style."));
				hr = E_FAIL;
				goto cleanup;
			}

			break;
		case D3DMPT_TRIANGLELIST:
			switch(PrimDir)
			{
			case PRIM_DIR1: // 

				FillUntransformedVertex(&pVertices[0], PlaneExtentsYZ.fFrontRightX, PlaneExtentsYZ.fY,  PlaneExtentsYZ.fBackZ, D3DQA_DIFFUSE_COLOR1, D3DQA_SPECULAR_COLOR1);
				FillUntransformedVertex(&pVertices[1],                           0, PlaneExtentsYZ.fY, PlaneExtentsYZ.fFrontZ, D3DQA_DIFFUSE_COLOR2, D3DQA_SPECULAR_COLOR2);
				FillUntransformedVertex(&pVertices[2],  PlaneExtentsYZ.fFrontLeftX, PlaneExtentsYZ.fY,  PlaneExtentsYZ.fBackZ, D3DQA_DIFFUSE_COLOR3, D3DQA_SPECULAR_COLOR3);
				break;
			case PRIM_DIR2: // 

				FillUntransformedVertex(&pVertices[0],  PlaneExtentsYZ.fFrontLeftX, PlaneExtentsYZ.fY, PlaneExtentsYZ.fFrontZ, D3DQA_DIFFUSE_COLOR1, D3DQA_SPECULAR_COLOR1);
				FillUntransformedVertex(&pVertices[1],                           0, PlaneExtentsYZ.fY,  PlaneExtentsYZ.fBackZ, D3DQA_DIFFUSE_COLOR2, D3DQA_SPECULAR_COLOR2);
				FillUntransformedVertex(&pVertices[2], PlaneExtentsYZ.fFrontRightX, PlaneExtentsYZ.fY, PlaneExtentsYZ.fFrontZ, D3DQA_DIFFUSE_COLOR3, D3DQA_SPECULAR_COLOR3);
				break;
			default:			
				DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown style."));
				hr = E_FAIL;
				goto cleanup;
			}
			break;
		default:			
			DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown type."));
			hr = E_FAIL;
			goto cleanup;
		}

		break;
	case TRANSLATE_TO_POSZ:

		//
		// The extent to which the triangle is outside of the far frustum plane is directly related to
		// iteration number.
		//
		fTranslate = (PlaneExtentsYZ.fBackZ - PlaneExtentsYZ.fFrontZ) * (fIterPercent);
		PlaneExtentsYZ.fFrontZ  += fTranslate;
		PlaneExtentsYZ.fBackZ   += fTranslate;

		switch(PrimType)
		{
		case D3DMPT_LINELIST:

			switch(PrimDir)
			{
			case PRIM_DIR1:
				for(uiLineIter = 0; uiLineIter < D3DQA_NUM_LINES; uiLineIter++)
				{
					fLineInterp = (float)uiLineIter / ((float)D3DQA_NUM_LINES-1.0f);
					fTemp = PlaneExtentsYZ.fFrontLeftX + (PlaneExtentsYZ.fFrontRightX - PlaneExtentsYZ.fFrontLeftX)*fLineInterp;
					FillUntransformedVertex(&pVertices[uiLineIter*2],
											fTemp,
											PlaneExtentsYZ.fY,
											PlaneExtentsYZ.fFrontZ,
											InterpColor(fLineInterp, D3DQA_DIFFUSE_COLOR2, D3DQA_DIFFUSE_COLOR3),
											InterpColor(fLineInterp, D3DQA_SPECULAR_COLOR2, D3DQA_SPECULAR_COLOR3));
					FillUntransformedVertex(&pVertices[uiLineIter*2+1],
											0,
											PlaneExtentsYZ.fY,
											PlaneExtentsYZ.fBackZ,
											D3DQA_DIFFUSE_COLOR1,
											D3DQA_SPECULAR_COLOR1);
				}
				break;
			case PRIM_DIR2:

				for(uiLineIter = 0; uiLineIter < D3DQA_NUM_LINES; uiLineIter++)
				{
					fLineInterp = (float)uiLineIter / ((float)D3DQA_NUM_LINES-1.0f);
					fTemp = PlaneExtentsYZ.fBackLeftX + (PlaneExtentsYZ.fBackRightX - PlaneExtentsYZ.fBackLeftX)*fLineInterp;
					FillUntransformedVertex(&pVertices[uiLineIter*2],
											0,
											PlaneExtentsYZ.fY,
											PlaneExtentsYZ.fFrontZ,
											InterpColor(fLineInterp, D3DQA_DIFFUSE_COLOR2, D3DQA_DIFFUSE_COLOR3),
											InterpColor(fLineInterp, D3DQA_SPECULAR_COLOR2, D3DQA_SPECULAR_COLOR3));
					FillUntransformedVertex(&pVertices[uiLineIter*2+1],
											fTemp,
											PlaneExtentsYZ.fY,
											PlaneExtentsYZ.fBackZ,
											D3DQA_DIFFUSE_COLOR1,
											D3DQA_SPECULAR_COLOR1);
				}
				break;
			default:			
				DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown style."));
				hr = E_FAIL;
				goto cleanup;
			}

			break;
		case D3DMPT_TRIANGLELIST:
			switch(PrimDir)
			{
			case PRIM_DIR1: // 
				FillUntransformedVertex(&pVertices[0],  PlaneExtentsYZ.fFrontLeftX, PlaneExtentsYZ.fY, PlaneExtentsYZ.fFrontZ, D3DQA_DIFFUSE_COLOR1, D3DQA_SPECULAR_COLOR1);
				FillUntransformedVertex(&pVertices[1],                           0, PlaneExtentsYZ.fY,  PlaneExtentsYZ.fBackZ, D3DQA_DIFFUSE_COLOR2, D3DQA_SPECULAR_COLOR2);
				FillUntransformedVertex(&pVertices[2], PlaneExtentsYZ.fFrontRightX, PlaneExtentsYZ.fY, PlaneExtentsYZ.fFrontZ, D3DQA_DIFFUSE_COLOR3, D3DQA_SPECULAR_COLOR3);
				break;
			case PRIM_DIR2: // 
				FillUntransformedVertex(&pVertices[0], PlaneExtentsYZ.fBackRightX, PlaneExtentsYZ.fY,  PlaneExtentsYZ.fBackZ, D3DQA_DIFFUSE_COLOR1, D3DQA_SPECULAR_COLOR1);
				FillUntransformedVertex(&pVertices[1],                          0, PlaneExtentsYZ.fY, PlaneExtentsYZ.fFrontZ, D3DQA_DIFFUSE_COLOR2, D3DQA_SPECULAR_COLOR2);
				FillUntransformedVertex(&pVertices[2],  PlaneExtentsYZ.fBackLeftX, PlaneExtentsYZ.fY,  PlaneExtentsYZ.fBackZ, D3DQA_DIFFUSE_COLOR3, D3DQA_SPECULAR_COLOR3);
				break;
			default:			
				DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown style."));
				hr = E_FAIL;
				goto cleanup;
			}
			break;
		default:			
			DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown type."));
			hr = E_FAIL;
			goto cleanup;
		}
		
		break;
	default:			
		DebugOut(DO_ERROR, _T("SetupClipGeometry: Unknown value."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Create and fill a vertex buffer with the generated geometry
	//
	if (FAILED(hr = CreateFilledVertexBuffer( pDevice,               // LPDIRECT3DMOBILEDEVICE pDevice,
	                                           pDesc->ppVB,           // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
	                                           (BYTE*)pVertices,      // BYTE *pVertices,
	                                           sizeof(CUSTOM_VERTEX), // UINT uiVertexSize,
	                                           pDesc->uiVertCount,    // UINT uiNumVertices,
	                                           CUSTOM_VERTEX_FVF)))   // DWORD dwFVF
	{
		DebugOut(DO_ERROR, _T("CreateFilledVertex failed. hr = 0x%08x"), hr);
		goto cleanup;
	}

cleanup:

	if (pVertices)
		free(pVertices);

	if ((FAILED(hr)) && (NULL != *(pDesc->ppVB)))
		(*(pDesc->ppVB))->Release();

	return hr;	
}
