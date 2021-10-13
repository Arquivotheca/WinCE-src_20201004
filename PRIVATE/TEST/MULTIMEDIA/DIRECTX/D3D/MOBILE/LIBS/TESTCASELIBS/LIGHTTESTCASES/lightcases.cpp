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
#include "LightCases.h"
#include "Geometry.h"
#include "BufferTools.h"
#include "QAD3DMX.h"
#include "DebugOutput.h"

//
// SupportsLightingTableIndex
//
//   Function prototype must conform to SUPPORTS_TABLE_INDEX.
//
//   Determines if a test permutation is expected to be supported by the specified
//   Direct3D Mobile device.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Direct3D Mobile device
//   DWORD dwTableIndex:  Index into table of test cases
//
// Return Value:
//
//   S_OK if test case is supported on this device; E_FAIL otherwise.
//
HRESULT SupportsLightingTableIndex(LPDIRECT3DMOBILEDEVICE pDevice, DWORD dwTableIndex, BOOL bAlphaBlend)
{
	//
	// Function result
	//
	HRESULT hr = S_OK;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// If this test case uses lighting, confirm support for any required light types
	//
	if (LightTestCases[dwTableIndex].Lighting)
	{
		//
		// Are directional lights supported if required?
		//
		if (!(Caps.VertexProcessingCaps & D3DMVTXPCAPS_DIRECTIONALLIGHTS))
		{
			if (D3DMLIGHT_DIRECTIONAL == LightTestCases[dwTableIndex].Light.Type)
			{
				DebugOut(DO_ERROR, L"D3DMVTXPCAPS_DIRECTIONALLIGHTS required for this test case; skipping.");
				hr = E_FAIL;
				goto cleanup;
			}
		}

		//
		// Are point lights supported if required?
		//
		if (!(Caps.VertexProcessingCaps & D3DMVTXPCAPS_POSITIONALLIGHTS))
		{
			if (D3DMLIGHT_POINT == LightTestCases[dwTableIndex].Light.Type)
			{
				DebugOut(DO_ERROR, L"D3DMVTXPCAPS_POINTLIGHTS required for this test case; skipping.");
				hr = E_FAIL;
				goto cleanup;
			}
		}

		//
		// Is the local viewer option supported, if required?
		//
		if (!(Caps.VertexProcessingCaps & D3DMVTXPCAPS_LOCALVIEWER))
		{
			if (LightTestCases[dwTableIndex].LocalViewer)
			{
				DebugOut(DO_ERROR, L"D3DMVTXPCAPS_LOCALVIEWER required for this test case; skipping.");
				hr = E_FAIL;
				goto cleanup;
			}
		}

		//
		// Is specular lighting supported, if required?
		//
		if (!(Caps.DevCaps & D3DMDEVCAPS_SPECULAR))
		{
			if (LightTestCases[dwTableIndex].SpecularEnable)
			{
				DebugOut(DO_ERROR, L"Test case requires D3DMDEVCAPS_SPECULAR; skipping.");
				hr = E_FAIL;
				goto cleanup;
			}
		}


	}

	//
	// If the test case calls for D3DMRS_COLORVERTEX==FALSE; diffuse, ambient, 
	// and specular sources are the D3DMMATERIAL fields.  *Every* D3DM driver 
	// can do this.
	//
	// If the test case calls for D3DMRS_COLORVERTEX==TRUE; by default the 
	// color sources are:
	//
	//          * Diffuse:  Vertex Color 1
	//          * Specular: Vertex Color 2
	//          * Ambient:  Material
	//
	// All drivers accomodate the above defaults.  For drivers that do not expose
	// D3DMVTXPCAPS_MATERIALSOURCE, the above defaults are the only ones that are
	// expected to work (e.g., no granular configurability via D3DMRS_*MATERIALSOURCE).
	// However, for drivers that *do* expose D3DMVTXPCAPS_MATERIALSOURCE, when
	// D3DMRS_COLORVERTEX==TRUE the application can granularly configure each
	// color source individually.
	//
	// Thus, if D3DMRS_COLORVERTEX==TRUE for this test case, and the specified
	// color sources are non-defaults (requiring D3DMRS_*MATERIALSOURCE tweaking),
	// the test case is exclusively for use on drivers that expose 
	// D3DMVTXPCAPS_MATERIALSOURCE.
	// 
	//
	if (!(Caps.VertexProcessingCaps & D3DMVTXPCAPS_MATERIALSOURCE))
	{
		if (LightTestCases[dwTableIndex].ColorVertex)
		{
			if ((LightTestCases[dwTableIndex].DiffuseMatSrc  != D3DMMCS_COLOR1  ) || 
				(LightTestCases[dwTableIndex].SpecularMatSrc != D3DMMCS_COLOR2  ) || 
				(LightTestCases[dwTableIndex].AmbientMatSrc  != D3DMMCS_MATERIAL))
			{
				DebugOut(DO_ERROR, L"Test case requires D3DMVTXPCAPS_MATERIALSOURCE; skipping.");
				hr = E_FAIL;
				goto cleanup;
			}
		}
	}

	//
	// If this frame is using Alpha, make sure the driver supports alpha blending.
	// (Using source and invsource)
	//D3DMPBLENDCAPS_SRCALPHA
	if (!(Caps.SrcBlendCaps & D3DMPBLENDCAPS_SRCALPHA) || !(Caps.SrcBlendCaps & D3DMPBLENDCAPS_INVSRCALPHA))
	{
	    if (bAlphaBlend)
	    {
	        DebugOut(DO_ERROR, L"Test case frame requires alpha blending. Skipping frame.");
	        hr = E_FAIL;
	        goto cleanup;
	    }
	}


cleanup:
	return hr;

}

//
// SetLightingStates
//
//   Prepare render state settings for test case execution.  This function is intended
//   to be called after verifying that the driver is expected to support this test case.
//
//   Thus, any failure here is inconsistent with the driver's reported capability
//   bits.
//
//   Function prototype must conform to SET_STATES.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Direct3D Mobile device
//   DWORD dwTableIndex:  Index into table of vertex fog test cases
//
// Return Value:
//
//   S_OK if state settings succeeded; E_FAIL otherwise.
//
HRESULT SetLightingStates(LPDIRECT3DMOBILEDEVICE pDevice, DWORD dwTableIndex, BOOL bAlphaBlend, BYTE DiffuseAlpha)
{
	//
	// Function result
	//
	HRESULT hr = S_OK;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Storage for view matrix
	//
	D3DMMATRIX View;

	// 
	// Always use the same up-vector, regardless of test case
	// 
	CONST D3DMVECTOR vecUp  = { _M(0.0f), _M(1.0f), _M(0.0f) };


	// 
	// Look at the center of the geometry
	// 
	CONST D3DMVECTOR vecAt  = { _M(WORLD_X_CENTER), // Middle of X World
								_M(WORLD_Y_CENTER), // Geometry is vertically centered WRT Y axis
								_M(WORLD_Z_CENTER)};// Middle of Z World


	//
	// Projection matrix
	//
	D3DMMATRIX ProjMatrix;

	//
	// Material
	//
	D3DMMATERIAL Material = LightTestCases[dwTableIndex].Material;

	//
	// Projection matrix specifications
	//
	// These are carefully chosen, such that the projection allows the test
	// cases to view the geometry suitably.  Particularly sensative are the
	// near plane and aspect ratio; haphazard changes would likely render
	// the test cases less useful.
	//
	CONST float fFov    = (D3DQA_PI / 4.0f);
	CONST float fAspect = 1.0f;
	CONST float fNear   = 1.0f;
	CONST float fFar    = 101.0f;

	//
	// Bad parameter check
	//
	if (NULL == pDevice)
	{
		DebugOut(DO_ERROR, L"Aborting due to NULL LPDIRECT3DMOBILEDEVICE.");
		hr = E_POINTER;
		goto cleanup;
	}

	//
	// Bad parameter check
	//
	if (dwTableIndex >= D3DMQA_LIGHTTEST_COUNT)
	{
		DebugOut(DO_ERROR, L"Aborting due to invalid table index.");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// View matrix created by looking at center of geometry, from
	// eye-point specified on a per-test-case basis.  The up vector, for
	// the purposes of this test, is always the typical Y=1 case.
	//
	D3DMatrixLookAtLH( &View,                                    // *pOut
					   &LightTestCases[dwTableIndex].vecEye,     // *pEye,
					   &vecAt,                                   // *pAt,
					   &vecUp );                                 // *pUp

	//
	// Set the view matrix
	//
	if (FAILED(hr = pDevice->SetTransform( D3DMTS_VIEW, &View, D3DMFMT_D3DMVALUE_FLOAT )))
	{
		DebugOut(DO_ERROR, L"SetTransform( D3DMTS_VIEW,,) failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Generate projection matrix
	//
	// Retval is just a pointer to ProjMatrix; no need to examine it
	//
	(void)D3DMatrixPerspectiveFovLH( &ProjMatrix, // D3DMATRIX* pOut,
	                                  fFov,       // FLOAT fovy,
	                                  fAspect,    // FLOAT Aspect,
	                                  fNear,      // FLOAT zn,
	                                  fFar );     // FLOAT zf
	                        

	//
	// Set the projection matrix for transformation
	//
	if( FAILED( hr = pDevice->SetTransform( D3DMTS_PROJECTION, &ProjMatrix, D3DMFMT_D3DMVALUE_FLOAT)))
	{
		DebugOut(DO_ERROR, L"SetTransform PROJECTION failed.(hr = 0x%08x)", hr);
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// A driver that does not expose D3DMVTXPCAPS_MATERIALSOURCE; and thus does not listen to D3DMRS_*MATERIALSOURCE
	// should be "hard-wired" to take inputs from these locations anyway (in the D3DMRS_COLORVERTEX==TRUE case).
	//
	// Note:  In the D3DMRS_COLORVERTEX==FALSE case, all drivers (D3DMVTXPCAPS_MATERIALSOURCE-exposing or not) will
	//        take all color inputs (diffuse, specular, ambient) from the material.
	//
	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_DIFFUSEMATERIALSOURCE,  LightTestCases[dwTableIndex].DiffuseMatSrc     )) ||
		FAILED(hr = pDevice->SetRenderState(D3DMRS_SPECULARMATERIALSOURCE, LightTestCases[dwTableIndex].SpecularMatSrc    )) ||
		FAILED(hr = pDevice->SetRenderState(D3DMRS_AMBIENTMATERIALSOURCE,  LightTestCases[dwTableIndex].AmbientMatSrc     )))
	{

		//
		// Failure above is OK if the driver does not expose D3DMVTXPCAPS_MATERIALSOURCE;
		// however, if it does expose D3DMVTXPCAPS_MATERIALSOURCE, the failure is unexpected.
		//
		if (Caps.VertexProcessingCaps & D3DMVTXPCAPS_MATERIALSOURCE)
		{
			DebugOut(
			    DO_ERROR, 
			    L"SetRenderState(D3DMRS_*MATERIALSOURCE, D3DMMCS_*) failed even though driver supports D3DMVTXPCAPS_MATERIALSOURCE. (hr = 0x%08x)", 
			    hr);
			goto cleanup;
		}
	}

	//
	// Set up render states required for this test case
	//
	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_CULLMODE,               D3DMCULL_NONE                                   )) ||
		FAILED(hr = pDevice->SetRenderState(D3DMRS_CLIPPING,               TRUE                                            )) ||
		FAILED(hr = pDevice->SetRenderState(D3DMRS_SHADEMODE,              D3DMSHADE_FLAT                                  )) ||
		FAILED(hr = pDevice->SetRenderState(D3DMRS_NORMALIZENORMALS,       LightTestCases[dwTableIndex].NormalizeNormals )) ||
		FAILED(hr = pDevice->SetRenderState(D3DMRS_LIGHTING,               LightTestCases[dwTableIndex].Lighting         )) ||
		FAILED(hr = pDevice->SetRenderState(D3DMRS_SPECULARENABLE,         LightTestCases[dwTableIndex].SpecularEnable   )) ||
		FAILED(hr = pDevice->SetRenderState(D3DMRS_LOCALVIEWER,            LightTestCases[dwTableIndex].LocalViewer      )) ||
		FAILED(hr = pDevice->SetRenderState(D3DMRS_COLORVERTEX,            LightTestCases[dwTableIndex].ColorVertex      )) ||
		FAILED(hr = pDevice->SetRenderState(D3DMRS_AMBIENT,                LightTestCases[dwTableIndex].GlobalAmbient    )))
	{
		DebugOut(DO_ERROR, L"SetRenderState failed for lighting settings. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Set up Alpha Blend states
	//
	if (bAlphaBlend)
	{
	    if (FAILED(hr = pDevice->SetRenderState(D3DMRS_SRCBLEND, D3DMBLEND_SRCALPHA)) ||
	        FAILED(hr = pDevice->SetRenderState(D3DMRS_DESTBLEND, D3DMBLEND_INVSRCALPHA)) ||
	        FAILED(hr = pDevice->SetRenderState(D3DMRS_ALPHABLENDENABLE, TRUE)) ||
	        FAILED(hr = pDevice->SetRenderState(D3DMRS_BLENDOP, D3DMBLENDOP_ADD)))
	    {
            DebugOut(DO_ERROR, L"SetRenderState failed for alpha blend settings. (hr = 0x%08x)", hr);
            goto cleanup;
	    }
	}
	else
	{
	    if (FAILED(hr = pDevice->SetRenderState(D3DMRS_SRCBLEND, D3DMBLEND_ONE)) ||
	        FAILED(hr = pDevice->SetRenderState(D3DMRS_DESTBLEND, D3DMBLEND_ZERO)) ||
	        FAILED(hr = pDevice->SetRenderState(D3DMRS_ALPHABLENDENABLE, FALSE)) ||
	        FAILED(hr = pDevice->SetRenderState(D3DMRS_BLENDOP, D3DMBLENDOP_ADD)))
	    {
            DebugOut(DO_ERROR, L"SetRenderState failed clearing alpha blend settings. (hr = 0x%08x)", hr);
            goto cleanup;
	    }
	}


	//
	// If this test case uses lighting, set up a light
	//
	if (LightTestCases[dwTableIndex].Lighting)
	{
		//
		// Set, and enable, a light
		//
		if (FAILED(hr = pDevice->SetLight(0,                                     // DWORD Index,
									      &LightTestCases[dwTableIndex].Light, // CONST D3DMLIGHT* pLight,
									      D3DMFMT_D3DMVALUE_FLOAT)))             // D3DMFORMAT Format)
		{
			DebugOut(DO_ERROR, L"SetLight failed. (hr = 0x%08x)", hr);
			goto cleanup;
		}
		if (FAILED(hr = pDevice->LightEnable(0,       // DWORD Index,
										     TRUE ))) // BOOL Enable
		{
			DebugOut(DO_ERROR, L"LightEnable failed. (hr = 0x%08x)", hr);
			goto cleanup;
		}

	}

	//
	// Set the material
	//
	float GivenAlpha = *reinterpret_cast<float *>(&Material.Diffuse.a);
	Material.Diffuse.a = _M(GivenAlpha * DiffuseAlpha / 255.0f);
	if (FAILED(hr = pDevice->SetMaterial(&Material, D3DMFMT_D3DMVALUE_FLOAT)))
	{
		DebugOut(DO_ERROR, L"SetMaterial failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

cleanup:

	return hr;
}


//
// SetupLightingGeometry
//  
//   Prepare geometry, in the form of a vertex buffer, for test execution.
//
//   The generated geometry is a "grid" of quads, with each quad rotated "in
//   place" about its X, Y, or Z axis.  Colors assigned to the vertices are
//   various.
//   
//   Function prototype must conform to SETUP_GEOMETRY.
//   
// Arguments: 
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying device
//   HWND hWnd:  Target window
//   LPDIRECT3DMOBILEVERTEXBUFFER *ppVB:  Resultant vertex buffer
//
// Return Value:
//
//   HRESULT indicates success or failure
//
HRESULT SetupLightingGeometry(LPDIRECT3DMOBILEDEVICE pDevice,
                                 HWND hWnd, 
                                 BYTE DiffuseAlpha,
                                 LPDIRECT3DMOBILEVERTEXBUFFER *ppVB)

{

	//
	// Pre-defined colors for geometry.  Carefully selected to expose
	// a wide variety of testing permutations.
	//
	#define D3DQA_PRIM_COLOR_COUNT 17
	DWORD dwColorPool[D3DQA_PRIM_COLOR_COUNT] = 
	{

		D3DMCOLOR_XRGB(255,  0,  0) ,
		D3DMCOLOR_XRGB(  0,255,  0) ,
		D3DMCOLOR_XRGB(  0,  0,255) ,
		D3DMCOLOR_XRGB(  0,  0,  0) ,
		D3DMCOLOR_XRGB( 64, 64, 64) ,
		D3DMCOLOR_XRGB(128,128,128) ,
		D3DMCOLOR_XRGB(192,192,192) ,
		D3DMCOLOR_XRGB(255,255,255) ,
		D3DMCOLOR_XRGB( 64, 64,  0) ,
		D3DMCOLOR_XRGB(  0, 64, 64) ,
		D3DMCOLOR_XRGB( 64,  0, 64) ,
		D3DMCOLOR_XRGB(128,128,  0) ,
		D3DMCOLOR_XRGB(  0,128,128) ,
		D3DMCOLOR_XRGB(128,  0,128) ,
		D3DMCOLOR_XRGB(192,192,  0) ,
		D3DMCOLOR_XRGB(  0,192,192) ,
		D3DMCOLOR_XRGB(192,  0,192)
	};


	#define D3DQA_PRIM_SPEC_COLOR_COUNT 8
	DWORD dwSpecColorPool[D3DQA_PRIM_COLOR_COUNT] = 
	{
		D3DMCOLOR_XRGB(255,  0,  0) ,
		D3DMCOLOR_XRGB(  0,255,  0) ,
		D3DMCOLOR_XRGB(  0,  0,255) ,
		D3DMCOLOR_XRGB(192,192,192) ,
		D3DMCOLOR_XRGB(255,255,255) ,
		D3DMCOLOR_XRGB(192,192,  0) ,
		D3DMCOLOR_XRGB(  0,192,192) ,
		D3DMCOLOR_XRGB(192,  0,192)

	};

	//
	// Each quad has a "top" and "bottom" tri; colored differently.
	// The colors for both top and bottom are fetched iteratively
	// from the above tables; however, to cause the "top" and "bottom"
	// triangles to have distinct colors, one set's initial lookup
	// index is offset to some non-zero index.
	//
	#define D3DQA_PRIM_COLOR_OFFSET1  0
	#define D3DQA_PRIM_COLOR_OFFSET2  4
	#define D3DQA_PRIM_SPEC_COLOR_OFFSET1  0
	#define D3DQA_PRIM_SPEC_COLOR_OFFSET2  4


	//
	// Pointer for generated vertex data
	// 
	LPD3DM_LIGHTTEST_VERTS pVertices = NULL, pCurrentVertices = NULL;

	//
	// All failure conditions specifically set this to an error
	//
	HRESULT hr = S_OK;

	//
	// Each quad is rotated "in place", by an angle that is generated
	// per-iteration
	//
	float fAngleX, fAngleY, fAngleZ;

	//
	// Quad offset, generated per-iteration
	//
	float fOffsetX;
	float fOffsetY;
	

	//
	// Bad-parameter check
	//
	if ((NULL == ppVB) || (NULL == pDevice))
	{
		DebugOut(DO_ERROR, L"malloc failed");
		hr = E_OUTOFMEMORY;
		goto cleanup;
	}


	//
	// Allocate memory for vertices
	//
	pCurrentVertices = pVertices = (LPD3DM_LIGHTTEST_VERTS)malloc(sizeof(D3DM_LIGHTTEST_VERTS) * D3DQA_NUMVERTS);
	if (NULL == pVertices)
	{
		DebugOut(DO_ERROR, L"malloc failed");
		hr = E_OUTOFMEMORY;
		goto cleanup;
	}

	//
	// Extents for the XY plane that serves as a bounding rectangle
	// for generated geometry.
	//
	const float fRectBorderPercent = 0.10f;
	float fRectLeft   = WORLD_X_MIN * (1.0f-fRectBorderPercent);   
	float fRectRight  = WORLD_X_MAX * (1.0f-fRectBorderPercent); 
	float fRectBottom = WORLD_Y_MIN * (1.0f-fRectBorderPercent);
	float fRectTop    = WORLD_Y_MAX * (1.0f-fRectBorderPercent);   
	float fRectWidth  = (fRectRight - fRectLeft);
	float fRectHeight = (fRectTop - fRectBottom);

	//
	// The geometry, within the XY plane described above, is centered
	// around the following Z.
	// 
	float fRectZ = WORLD_Z_CENTER;

	//
	// Extents of individual quads
	//
	float fQuadWidth  = ( fRectWidth / (float)D3DQA_QUADS_X);  
	float fQuadHeight = (fRectHeight / (float)D3DQA_QUADS_Y); 

	//
	// To allow for quads to be rotated in place, without overlapping
	// one another "much" in the common case, scale each RECT's
	// allotted world space down via a "border percentage"
	//
	const float fQuadBorderPercent = 0.20f;

	//
	// Iterator for generating quads
	//
	UINT uiQuad;

	//
	// Number of rotation steps of equal size (radians) that should
	// equate to approximately one full rotation
	//
	UINT uiRotSteps;

	//
	// Number of rotation steps to apply to quad; computed per-iteration
	//
	UINT uiRotIter; 

	//
	// Diffuse and specular primitive colors, fetched from table per-iteration
	//
	D3DMCOLOR dwDiffuse1, dwDiffuse2;
	D3DMCOLOR dwSpecular1, dwSpecular2;

	//
	// Iterate through the entire range of quads to be generated
	//
	for (uiQuad = 0; uiQuad < D3DQA_NUMQUADS; uiQuad++)
	{

		//
		// Quads are either rotated in place about their X-axis, Y-axis, or Z-axis.
		// Approximately 1/3 of quads are reserved for each axis of rotation
		//
		if (uiQuad < (D3DQA_NUMQUADS / 3))
		{
			uiRotSteps = (D3DQA_NUMQUADS / 3);
			uiRotIter  = uiQuad;

			//
			// First 1/3 of quads
			//

			fAngleY = 0.0f;
			fAngleZ = 0.0f;

			//
			// Linear interpolation of angle, in range of [0,2*Pi]
			//
			fAngleX = (2.0f * D3DQA_PI)*((float)uiRotIter/(float)uiRotSteps);

		}
		else if (uiQuad < (2*(D3DQA_NUMQUADS / 3)))
		{
			uiRotSteps = (D3DQA_NUMQUADS / 3);
			uiRotIter  = uiQuad - uiRotSteps;

			//
			// Second 1/3 of quads
			//

			fAngleX = 0.0f;
			fAngleZ = 0.0f;

			//
			// Linear interpolation of angle, in range of [0,2*Pi]
			//
			fAngleY = (2.0f * D3DQA_PI)*((float)uiRotIter/(float)uiRotSteps);

		}
		else
		{
			uiRotSteps = (D3DQA_NUMQUADS / 3);
			uiRotIter  = uiQuad - 2*uiRotSteps;

			//
			// Third 1/3 of quads
			//

			fAngleX = 0.0f;
			fAngleY = 0.0f;

			//
			// Linear interpolation of angle, in range of [0,2*Pi]
			//
			fAngleZ = (2.0f * D3DQA_PI)*((float)uiRotIter/(float)uiRotSteps);

		}

		// 
		// Lookup diffuse and specular colors, by iterating through the color tables based on quad
		// index.  An offset is applied to force the "top" and "bottom" tris to be distinct colors.
		// 
		dwDiffuse1  =     dwColorPool[((uiQuad+     D3DQA_PRIM_COLOR_OFFSET1) % D3DQA_PRIM_COLOR_COUNT)];
		dwDiffuse1  = (dwDiffuse1 & (D3DMCOLOR_ARGB(DiffuseAlpha, 0xff, 0xff, 0xff)));
		dwSpecular1 = dwSpecColorPool[((uiQuad+D3DQA_PRIM_SPEC_COLOR_OFFSET1) % D3DQA_PRIM_SPEC_COLOR_COUNT)];

		dwDiffuse2  =     dwColorPool[((uiQuad+     D3DQA_PRIM_COLOR_OFFSET2) % D3DQA_PRIM_COLOR_COUNT)];
		dwDiffuse2  = (dwDiffuse2 & (D3DMCOLOR_ARGB(DiffuseAlpha, 0xff, 0xff, 0xff)));
		dwSpecular2 = dwSpecColorPool[((uiQuad+D3DQA_PRIM_SPEC_COLOR_OFFSET2) % D3DQA_PRIM_SPEC_COLOR_COUNT)];

		//
		// Linear interpolation for top-left rect offsets in the
		// range of [fRectLeft,fRectRight) and [fRectTop,fRectBottom)
		//
		fOffsetX =   fRectLeft + (float)(uiQuad % D3DQA_QUADS_X) * fQuadWidth;
		fOffsetY = fRectBottom + (float)(uiQuad / D3DQA_QUADS_X) * fQuadHeight;


		//
		// Create a quad centered at the origin, rotated in place by the specified angles
		//
		if (FAILED(hr = SetupRotatedQuad(pCurrentVertices,          // LPD3DM_LIGHTTEST_VERTS pVerts,
									     fAngleX, fAngleY, fAngleZ, // Angles of rotation
									     dwDiffuse1,                // >
									     dwSpecular1,               //  >  Vertex colors; unique for
									     dwDiffuse2,                //  >  "top" and "bottom" tri
									     dwSpecular2                // >
									     )))
		{
			DebugOut(DO_ERROR, L"SetupRotatedQuad failed. (hr = 0x%08x)", hr);
			goto cleanup;
		}

		//
		// Scale the quad to the desired size
		//
		if (FAILED(hr = SetupScaleQuad( pCurrentVertices,                      // LPD3DM_LIGHTTEST_VERTS pVerts,
						    	        fQuadWidth * (1.0f-fQuadBorderPercent), // float fX,
								        fQuadHeight* (1.0f-fQuadBorderPercent), // float fY,
								        1.0f)))                                 // float fZ
		{
			DebugOut(DO_ERROR, L"SetupScaleQuad failed. (hr = 0x%08x)", hr);
			goto cleanup;
		}

		//
		// Translate quad to desired location within "grid" of quads
		//
		// To account for the fact that computed quad is centered at origin, and that
		// computed (desired) offsets are for top/left, add half of width/height to offsets
		//
		if (FAILED(hr = SetupOffsetQuad(pCurrentVertices,               // LPD3DM_LIGHTTEST_VERTS pVerts,
								         fOffsetX + (fQuadWidth / 2.0f), // float fX,
								         fOffsetY + (fQuadWidth / 2.0f), // float fY,
								         fRectZ)))                       // float fZ)
		{
			DebugOut(DO_ERROR, L"SetupOffsetQuad failed. (hr = 0x%08x)", hr);
			goto cleanup;
		}

		//
		// Increment pointer the length of an entire quad's vertices
		//
		pCurrentVertices = &pCurrentVertices[D3DQA_VERTS_PER_QUAD];
	}

	//
	// Create and fill a vertex buffer with the generated geometry
	//
	if (FAILED(hr = CreateFilledVertexBuffer( pDevice,                       // LPDIRECT3DMOBILEDEVICE pDevice,
	                                          ppVB,                          // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
	                                          (BYTE*)pVertices,              // BYTE *pVertices,
	                                          sizeof(D3DM_LIGHTTEST_VERTS),  // UINT uiVertexSize,
	                                          D3DQA_NUMVERTS,                // UINT uiNumVertices,
	                                          D3DM_LIGHTTEST_FVF)))          // DWORD dwFVF
	{
		DebugOut(DO_ERROR, L"CreateFilledVertexBuffer failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

cleanup:

	if (pVertices)
		free(pVertices);

	if ((FAILED(hr)) && (NULL != ppVB) && (NULL != (*ppVB)))
		(*ppVB)->Release();

	return hr;	
	

}


