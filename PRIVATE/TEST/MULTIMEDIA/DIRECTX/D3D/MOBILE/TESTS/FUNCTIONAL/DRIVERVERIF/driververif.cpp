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
#define INITGUID 1

#include "DriverVerif.h"
#include "ImageManagement.h"
#include "KatoUtils.h"
#include "BufferTools.h"
#include "VerifTestCases.h"
#include "QAMath.h"
#include "DebugOutput.h"
#include <tux.h>
#include <tchar.h>
#include <stdio.h>

//
// Release with NULL-check before and NULL-clear after
//
#define SAFERELEASE(_o)  if(_o) { _o->Release(); _o=NULL; } else { DebugOut(DO_ERROR, L"Attempted to release a NULL object."); }

//
// Short define for value cast
//
#define _M(_F) D3DM_MAKE_D3DMVALUE(_F)

//
//  DriverVerifTest
//
//    Constructor for DriverVerifTest class; however, most initialization is deferred.
//
//  Arguments:
//
//    none
//
//  Return Value:
//
//    none
//
DriverVerifTest::DriverVerifTest()
{
	m_bInitSuccess = FALSE;
}


//
// DriverVerifTest
//
//   Initialize DriverVerifTest object
//
// Arguments:
//
//   UINT uiAdapterOrdinal: D3DM Adapter ordinal
//   WCHAR *pSoftwareDevice: Pointer to wide character string identifying a D3DM software device to instantiate
//   UINT uiTestCase:  Test case ordinal (to be displayed in window)
//
// Return Value
//
//   HRESULT indicates success or failure
//
HRESULT DriverVerifTest::Init(LPTEST_CASE_ARGS pTestCaseArgs, LPWINDOW_ARGS pWindowArgs, UINT uiTestCase)
{
    HRESULT hr;
	if (FAILED(hr = D3DMInitializer::Init(D3DQA_PURPOSE_RAW_TEST,                      // D3DQA_PURPOSE Purpose
	                                 pWindowArgs,                                 // LPWINDOW_ARGS pWindowArgs
	                                 pTestCaseArgs->pwchSoftwareDeviceFilename,   // TCHAR *ptchDriver
	                                 uiTestCase)))                                // UINT uiTestCase
	{
		DebugOut(DO_ERROR, L"Aborting initialization, due to prior initialization failure. (hr = 0x%08x)", hr);
		return hr;
	}

	m_bInitSuccess = TRUE;
	m_fTestTolerance = pTestCaseArgs->fTestTolerance;
	return S_OK;
}


//
//  IsReady
//
//    Accessor method for "initialization state" of DriverVerifTest object.  "Half
//    initialized" states should not occur.
//  
//  Return Value:
//  
//    BOOL:  TRUE if the object is initialized; FALSE if it is not.
//
BOOL DriverVerifTest::IsReady()
{
	if (FALSE == m_bInitSuccess)
	{
		DebugOut(DO_ERROR, L"DriverVerifTest is not ready.");
		return FALSE;
	}

	DebugOut(L"DriverVerifTest is ready.");
	return D3DMInitializer::IsReady();
}

// 
// ~DriverVerifTest
//
//  Destructor for DriverVerifTest.  Currently; there is nothing
//  to do here.
//
DriverVerifTest::~DriverVerifTest()
{
	return; // Success
}



//
// SetDegenerateViewAndProj
//
//   Sets viewport extents and projection matrix such that the product is an identity matrix.
//   Useful for doing isolated tests on world and view matrices.
//
// Arguments:
//
//
// Return Value
//
//   HRESULT indicates success or failure
//
HRESULT DriverVerifTest::SetDegenerateViewAndProj()
{
    HRESULT hr;
    
	//
	// Constants that cause the projection matrix and viewport transformations
	// to become degenerate
	//
	CONST D3DMMATRIX ProjMatrix = {_M( 1.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f),
	                               _M( 0.0f), _M(-1.0f), _M( 0.0f), _M( 0.0f),
	                               _M( 0.0f), _M( 0.0f), _M( 1.0f), _M( 0.0f),
	                               _M(-1.0f), _M( 1.0f), _M( 0.0f), _M( 1.0f)};
	CONST UINT ViewportWidth = 2;
	CONST UINT ViewportHeight = 2;

	
	//
	// For retrieving/resetting viewport extents
	//
	D3DMVIEWPORT Viewport;

	//
	// Retrieve viewport
	//
	if (FAILED(hr = m_pd3dDevice->GetViewport(&Viewport)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::GetViewport failed. (hr = 0x%08x)", hr);
		return hr;
	}

	//
	// Tweak viewport; set to degenerate case
	//
	Viewport.Height = ViewportHeight;
	Viewport.Width = ViewportWidth;
	if (FAILED(hr = m_pd3dDevice->SetViewport(&Viewport)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetViewport failed. (hr = 0x%08x)", hr);
		return hr;
	}

	//
	// Indicate proj matrix to D3DM
	//
	if( FAILED( hr = m_pd3dDevice->SetTransform(D3DMTS_PROJECTION,         // D3DTRANSFORMSTATETYPE State,
	                                            &ProjMatrix,               // CONST D3DMMATRIX* pMatrix
	                                            D3DMFMT_D3DMVALUE_FLOAT))) // D3DMFORMAT Format
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetTransform failed. (hr = 0x%08x)", hr);
		return hr;
	}

	return S_OK;
}

// 
// ExecuteTexTransRotate
//  
//  This is a test of texture transforms, using rotation matrices.
//
//  This function iterates through various rotation types.  These rotation types include X, Y, and Z
//  axis rotations, in various orders (e.g., rotating in XYZ order is distinct from ZYX order).
//
//  For each rotation type, the X, Y, and Z angles of rotation are selected by an exhaustive iteration
//  of a predefined angle list (currently includes 8 angles of rotation).
//
//  The result of the texture transform (acheived through ProcessVertices) is compared to the
//  "expected" outcome.  The "expected" outcome is computed by this test, through simple matrix
//  mathematics.
//
//  If a comparison of any component (expected vs. actual) is not within the specified tolerance, the
//  test will return TPR_FAIL.  However, regardless of comparison failures, every iteration of the test
//  will execute.  The return of TPR_FAIL is deferred until all iterations are complete.
//
//  Note: clipping has been turned off at initialization time.
//
// Arguments:
//  
//  fTolerance:  Specifies the allowable difference between the driver-computed result for each
//               texture component, and the test-computed result for each texture component.
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecuteTexTransRotate()
{
    HRESULT hr;
    
	//
	// FVF structures and bit masks for use with this test
	//
	#define D3DQA_TTRTEST_IN_FVF (D3DMFVF_XYZ_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE3(0))
	struct D3DQA_TTRTEST_IN
	{
		FLOAT x, y, z;
		FLOAT tu, tv, tr;    // One set of 3D texture coordinates
	};

	#define D3DQA_TTRTEST_OUT_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE3(0))
	struct D3DQA_TTRTEST_OUT
	{
		FLOAT x, y, z, rhw;
		FLOAT tu, tv, tr;    // One set of 3D texture coordinates
	};

	//
	// Input and output Vertex buffers for ProcessVertices
	//
	IDirect3DMobileVertexBuffer *pVBIn = NULL;
	IDirect3DMobileVertexBuffer *pVBOut = NULL;

	// 
	// Device Capabilities
	// 
	D3DMCAPS Caps;

	D3DMVECTOR ResultTexCoordsExpected;         // Manually-computed texture coordinates, to be compared with D3D result
	D3DMMATRIX TexTransMatrix;                  // Matrix used to transform texture coordinates
	D3DMMATRIX TexTransMatrixInverse;           // Inverse of transformation matrix; for debugging purposes
	UINT uiRotIndexX, uiRotIndexY, uiRotIndexZ; // Angles of rotation for each axis
	UINT RotType;                               // Index into D3DQA_ROTTYPE; specifies axis of rotation (1 to 3 axis)
	UINT uiIteration = 0;                       // This is a counter, used for logging/debugging purposes, of the current iteration
	D3DQA_TTRTEST_IN InputVertex;               // Untransformed vertices
	D3DQA_TTRTEST_OUT *pOutputVert = NULL;      // Pointer for resultant data (from ProcessVertices)
	BYTE *pVertices = NULL;                     // Pointer for vertex buffer locking
	UINT uiRotXFirst, uiRotXLast;               // angle iteration constraints for the X-Axis (depends on rotation-type iteration)
	UINT uiRotYFirst, uiRotYLast;               // angle iteration constraints for the Y-Axis (depends on rotation-type iteration)
	UINT uiRotZFirst, uiRotZLast;               // angle iteration constraints for the Z-Axis (depends on rotation-type iteration)

	//
	// Tux Test Case Result; all non-pass blocks set this to another value
	//
	INT Result = TPR_PASS;

	//
	// The test will return TPR_FAIL if even one result exceeds the tolerance, but to aid in
	// debugging the test will continue to execute until reaching the number of failures below.
	//
	CONST UINT uiFailureThreshhold = 25;
	UINT uiNumFailures = 0;

	DebugOut(L"Beginning ExecuteTexTransRotate.");

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Disable clipping
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_CLIPPING, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Disable lighting
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_LIGHTING, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	// 
	// Query the device's capabilities
	// 
	if( FAILED( hr = m_pd3dDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::GetDeviceCaps failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	// 
	// If the device supports directional lights, assume that it supports lighting
	// with at least one MaxActiveLights.  Otherwise, the test cannot execute.
	// 
	if (!(D3DMDEVCAPS_TEXTURE & Caps.DevCaps))
	{
		DebugOut(DO_ERROR, L"Texturing not supported. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	ZeroMemory(&InputVertex,sizeof(D3DQA_TTRTEST_IN));

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pVBOut = CreateActiveBuffer(m_pd3dDevice, 1, D3DQA_TTRTEST_OUT_FVF, sizeof(D3DQA_TTRTEST_OUT),
	                            D3DMUSAGE_DONOTCLIP);
	if (NULL == pVBOut)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pVBIn = CreateActiveBuffer(m_pd3dDevice, 1, D3DQA_TTRTEST_IN_FVF, sizeof(D3DQA_TTRTEST_IN),
	                           D3DMUSAGE_DONOTCLIP);
	if (NULL == pVBIn)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Fill the input vertex buffer
	//
	InputVertex.x = 0.0f;
	InputVertex.y = 0.0f;
	InputVertex.z = 0.0f;
	InputVertex.tu = 0.5f;
	InputVertex.tv = 0.5f; 
	InputVertex.tr = 0.5f; 

	//
	// Set up input vertices (lock, copy data into buffer, unlock)
	//
	if( FAILED( hr = pVBIn->Lock( 0, sizeof(D3DQA_TTRTEST_IN), (VOID**)&pVertices, 0 ) ) )
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	memcpy( pVertices, &InputVertex, sizeof(D3DQA_TTRTEST_IN) );

	if( FAILED( hr = pVBIn->Unlock() ) )
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Unlock failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Indicate that texture transforms are to affect 3 texture dimensions
	//
	if (FAILED(hr = m_pd3dDevice->SetTextureStageState( 0, D3DMTSS_TEXTURETRANSFORMFLAGS, D3DMTTFF_COUNT3 )))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetTextureStageState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Indicate that the texture stage is active; PV will fail otherwise
	//
	if (FAILED(hr = m_pd3dDevice->SetTextureStageState( 0, D3DMTSS_COLORARG1, D3DMTA_CURRENT )))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetTextureStageState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Iterate through a variety of texture coordinate transformations
	//
	for (RotType = D3DQA_FIRST_ROTTYPE; RotType <= D3DQA_LAST_ROTTYPE; RotType++)
	{

		DebugOut(L"");
		DebugOut(L"==============================================================");
		DebugOut(L"Now beginning iterations of rotation type: %s ", D3DQA_ROTTYPE_NAMES[RotType]);
		DebugOut(L"==============================================================");
		DebugOut(L"");

		//
		// Some of the rotation specs (e.g., YZ) do not involve a rotation about
		// the X axis; thus, only one iteration of the X-Axis loop is needed for
		// these cases.
		//
		if (UsesXRot((D3DQA_ROTTYPE)RotType))
		{
			uiRotXFirst = 0;
			uiRotXLast = D3DQA_NUM_SAMPLE_ROT_ANGLES-1; // Adjust for zero-based indexing
		}
		else
		{
			uiRotXFirst = uiRotXLast = 0;
		}

		for(uiRotIndexX = uiRotXFirst; uiRotIndexX <= uiRotXLast; uiRotIndexX++)
		{
			//
			// Retrieve the amount (angle) of X-axis rotation for this iteration
			//
			float fRotX = ROTANGLES[uiRotIndexX];

			//
			// Some of the rotation specs (e.g., XZ) do not involve a rotation about
			// the Y axis; thus, only one iteration of the Y-Axis loop is needed for
			// these cases.
			//
			if (UsesYRot((D3DQA_ROTTYPE)RotType))
			{
				uiRotYFirst = 0;
				uiRotYLast = D3DQA_NUM_SAMPLE_ROT_ANGLES-1; // Adjust for zero-based indexing
			}
			else
			{
				uiRotYFirst = uiRotYLast = 0;
			}

			for(uiRotIndexY = uiRotYFirst; uiRotIndexY <= uiRotYLast; uiRotIndexY++)
			{
				//
				// Retrieve the amount (angle) of Y-axis rotation for this iteration
				//
				float fRotY = ROTANGLES[uiRotIndexY];

				//
				// Some of the rotation specs (e.g., XY) do not involve a rotation about
				// the Z axis; thus, only one iteration of the Z-Axis loop is needed for
				// these cases.
				//
				if (UsesZRot((D3DQA_ROTTYPE)RotType))
				{
					uiRotZFirst = 0;
					uiRotZLast = D3DQA_NUM_SAMPLE_ROT_ANGLES-1; // Adjust for zero-based indexing
				}
				else
				{
					uiRotZFirst = uiRotZLast = 0;
				}

				for(uiRotIndexZ = uiRotZFirst; uiRotIndexZ <= uiRotZLast; uiRotIndexZ++)
				{
					uiIteration++;  // This is used only for logging purposes; and for ease of debugging.

					//
					// Retrieve the amount (angle) of Z-axis rotation for this iteration
					//
					float fRotZ = ROTANGLES[uiRotIndexZ];

					//
					// Determine the rotation matrix that satisfies the specifications for this
					// iteration of the nested loop.
					//
					// The inverse matrix isn't actually used here; but it is generated in case
					// it is useful in debugging.
					//
					if (!(GetRotationInverses((D3DQA_ROTTYPE)RotType,   // D3DQA_ROTTYPE rType,
					                          fRotX,                    // const float fXRot,
					                          fRotY,                    // const float fYRot,
					                          fRotZ,                    // const float fZRot,
					                          &TexTransMatrix,          // D3DMATRIX* const RotMatrix,
					                          &TexTransMatrixInverse))) // D3DMATRIX* const RotMatrixInverse)
					{
						DebugOut(DO_ERROR, L"GetRotationInverses failed. Failing.");
						Result = TPR_FAIL;
						goto cleanup;
					}

					//
					// For ease of computation, the "expected" texture coordinates will be
					// derived by storing the initial coordinates in a vector and then
					// transforming it by the same rotation matrix that has been indicated
					// to Direct3D.
					//
					ResultTexCoordsExpected.x = _M(InputVertex.tu);
					ResultTexCoordsExpected.y = _M(InputVertex.tv);
					ResultTexCoordsExpected.z = _M(InputVertex.tr);
					ResultTexCoordsExpected = TransformVector(&ResultTexCoordsExpected, &TexTransMatrix);

					//
					// Set-up the texture coordinate transform matrix for the desired transformation.
					//
					if (FAILED(hr = m_pd3dDevice->SetTransform( D3DMTS_TEXTURE0, &TexTransMatrix, D3DMFMT_D3DMVALUE_FLOAT )))
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetTransform failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Process a single vertex from the vertex buffer.
					//
					if (FAILED(hr = m_pd3dDevice->ProcessVertices(0,      // UINT SrcStartIndex: Index of first vertex to be loaded. 
					                                              0,      // UINT DestIndex: Index of first vertex in the destination vertex buffer
					                                              1,      // UINT VertexCount: Number of vertices to process. 
					                                              pVBOut, // IDirect3DMobileVertexBuffer* pDestBuffer: Pointer to an IDirect3DMobileVertexBuffer interface, the destination vertex buffer representing the stream of interleaved vertex data
					                                              0)))    // DWORD Flags: Processing options. Set this parameter to 0 for default processing.
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::ProcessVertices failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Verify output vertices (lock, examine data, unlock)
					//

					//
					// Lock output vertex buffer; so that resulting coordinate data can be examined
					//
					if( FAILED( hr = pVBOut->Lock( 0, sizeof(D3DQA_TTRTEST_OUT), (VOID**)&pOutputVert, 0 ) ) )
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Test for differences that exceed the tolerance
					//
					if ((fabs(*(float*)&ResultTexCoordsExpected.x - *(float*)&pOutputVert->tu) > m_fTestTolerance) ||
					    (fabs(*(float*)&ResultTexCoordsExpected.y - *(float*)&pOutputVert->tv) > m_fTestTolerance) ||
					    (fabs(*(float*)&ResultTexCoordsExpected.z - *(float*)&pOutputVert->tr) > m_fTestTolerance))
					{

						//
						// Log the current rotation specs.  Note that the order of the X, Y, Z angle information will
						// not be output in "rotation order".  For example, a rotation of XYZ will have the same debug
						// output order as a rotation of XZY.
						//
						DebugOut(L"Iteration #%u...  Rotation Type: %s; X Rot Angle: %f; Y Rot Angle: %f; Z Rot Angle: %f",
							  uiIteration, D3DQA_ROTTYPE_NAMES[RotType], fRotX, fRotY, fRotZ);

						//
						// The comparison demonstrated a difference that exceeded the tolerance
						//

						DebugOut(L"Texture coordinate comparison failure.  Expected coordinates: (%f,%f,%f); Actual coordinates (%f,%f,%f); Difference (%f,%f,%f); Tolerance (%f)",
							*(float*)&ResultTexCoordsExpected.x, *(float*)&ResultTexCoordsExpected.y, *(float*)&ResultTexCoordsExpected.z,
							*(float*)&pOutputVert->tu, *(float*)&pOutputVert->tv, *(float*)&pOutputVert->tr,
							fabs(*(float*)&ResultTexCoordsExpected.x - *(float*)&pOutputVert->tu),
							fabs(*(float*)&ResultTexCoordsExpected.y - *(float*)&pOutputVert->tv),
							fabs(*(float*)&ResultTexCoordsExpected.z - *(float*)&pOutputVert->tr),
							m_fTestTolerance);
						Result = TPR_FAIL;
						DebugOut(DO_ERROR, L"Failing.");

						uiNumFailures++;
						if (uiNumFailures >= uiFailureThreshhold)
						{
							DebugOut(L"Failure threshhold exceeded.");
							Result = TPR_FAIL; // Should have been set to this previously anyway
							goto cleanup;
						}
					}
	
					//
					// Unlock output vertex buffer; analysis of coordinate data therein is complete
					//
					if( FAILED( pVBOut->Unlock() ) )
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Unlock failed.");
						Result = TPR_ABORT;
						goto cleanup;
					}
				}
			}
		}
	}

cleanup:

	//
	// Unlock if not already unlocked (no op if it is already unlocked)
	//
	if (pVBIn)
		pVBIn->Unlock();
	if (pVBOut)
		pVBOut->Unlock();

	//
	// Release vertex buffers
	//
	if (pVBIn)
		pVBIn->Release();
	if (pVBOut)
		pVBOut->Release();


	//
	// Clear active vertex buffer ref-count
	//
	if (m_pd3dDevice)
		m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
		                               NULL,  // IDirect3DMobileVertexBuffer* pStreamData
		                               0);    // UINT Stride

	//
	// Return tux test case result
	//
	return Result;
}

//
// ExecuteMultTest
//  
//   This test verifies ProcessVertices transformation of vertex positions via the world and view matrices.
//   The technique used by this test is to confirm that multiplications between specific matrix components
//   is occuring as expected.
//
//   This test expects the viewport and projection to be set up to "cancel" each other out (the result of the
//   projection matrix mult with the viewport matrix should be the identity matrix).
//   
//   For simplicity, it is expected that clipping is off.
//
// Arguments:
//
//   None
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecuteMultTest()
{
    HRESULT hr;

	DebugOut(L"Beginning ExecuteMultTest.");

	//
	// This test iterates over a variety of multiplicands, each
	// generated by 2^exp, where exp is:
	//
	int iExponentOne, iExponentTwo;

	//
	// Input and output Vertex buffers for ProcessVertices
	//
	IDirect3DMobileVertexBuffer *pInVB = NULL;
	IDirect3DMobileVertexBuffer *pOutVB = NULL;

	//
	// Positions for multiplicands in world, view matrix data
	//
	UINT uiWorldRow, uiWorldCol, uiViewRow, uiViewCol;

	//
	// World and view transformation matrices for D3DM TnL
	//
	D3DMMATRIX WorldMatrix, ViewMatrix;

	//
	// FVF sizes for this test
	//
	CONST UINT uiInVertSize = sizeof(float)*3;  // X,Y,Z
	CONST UINT uiOutVertSize = sizeof(float)*4; // X,Y,Z,RHW

	//
	// Difference between expected value and actual value
	//
	double dDelta;
	double dLargestDelta = 0.0f;

	//
	// The test will return TPR_FAIL if even one result exceeds the tolerance, but to aid in
	// debugging the test will continue to execute until reaching the number of failures below.
	//
	CONST UINT uiFailureThreshhold = 25;
	UINT uiNumFailures = 0;

	//
	// Used for VB locks, to access floating point positional data (X,Y,Z,RHW)
	//
	float *pFloats = NULL;

	//
	// Tux Test Case Result; all non-pass blocks set this to another value
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Disable clipping
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_CLIPPING, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Disable lighting
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_LIGHTING, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Cause viewport and projection to cancel each other out, for isolated world/view testing
	//
	if (FAILED(hr = SetDegenerateViewAndProj()))
	{
		DebugOut(DO_ERROR, L"SetDegenerateViewAndProj failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pOutVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DMFVF_XYZRHW_FLOAT, uiOutVertSize, D3DMUSAGE_DONOTCLIP);
	if (NULL == pOutVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pInVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DMFVF_XYZ_FLOAT, uiInVertSize, 0);
	if (NULL == pInVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}


	//
	// Iterate over a variety of multiplicands
	//
	for(iExponentOne = D3DQA_PV_MINEXP; iExponentOne <= 0; iExponentOne++)
	{
		for(iExponentTwo = 0; iExponentTwo >= (D3DQA_PV_MINEXP-iExponentOne); iExponentTwo--)
		{
			UINT uiMultOpNum = 1;
			float fValOne = (float)pow( 2, iExponentOne );
			float fValTwo = (float)pow( 2, iExponentTwo );
			int iExpectedResultExp = iExponentOne+iExponentTwo;
			float fExpectedResult = fValOne * fValTwo; // Equivalent to: (float)pow( 2, iExpectedResultExp );

			for (uiViewCol = 0; uiViewCol < 3; uiViewCol++)
			{
				for (uiWorldRow = 0; uiWorldRow < 3; uiWorldRow++)
				{
					for (uiWorldCol = 0; uiWorldCol < 3; uiWorldCol++)
					{
						uiViewRow = uiWorldCol;

						float fResult;


						//
						// Lock the input vertex buffer
						//
						if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
						                             uiInVertSize,       // UINT SizeToLock,
						                             (VOID**)(&pFloats), // VOID** ppbData,
						                             0)))                // DWORD Flags
						{
							DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
							Result = TPR_ABORT;
							goto cleanup;
						}

						//
						// Fill the input vertex buffer
						//
						pFloats[0] = 0.0f;
						pFloats[1] = 0.0f;
						pFloats[2] = 0.0f;

						pFloats[uiWorldRow] = 1.0f;


						//
						// Unlock the input vertex buffer
						//
						pInVB->Unlock();


						//
						// Prepare the view and world matrices
						//
						ZeroMemory(&WorldMatrix,sizeof(D3DMMATRIX));
						ZeroMemory(&ViewMatrix,sizeof(D3DMMATRIX));

						//
						// Set the view matrix, initially, to identity
						//
						ViewMatrix.m[0][0] = ViewMatrix.m[1][1] = ViewMatrix.m[2][2] = ViewMatrix.m[3][3] = _M(1.0f);

						WorldMatrix.m[3][3] = _M(1.0f);

						//
						// Set multiplicands
						//
						WorldMatrix.m[uiWorldRow][uiWorldCol] = _M(fValOne);
						ViewMatrix.m[uiViewRow][uiViewCol] = _M(fValTwo);

						//
						// Indicate world matrix to D3DM
						//
						if( FAILED( hr = m_pd3dDevice->SetTransform(D3DMTS_WORLD,              // D3DTRANSFORMSTATETYPE State,
						                                            &WorldMatrix,              // CONST D3DMMATRIX* pMatrix
						                                            D3DMFMT_D3DMVALUE_FLOAT))) // D3DMFORMAT Format
						{
							DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetTransform WORLD failed. (hr = 0x%08x) Aborting.", hr);
							Result = TPR_ABORT;
							goto cleanup;
						}
 
						//
						// Indicate view matrix to D3DM
						//
						if( FAILED( hr = m_pd3dDevice->SetTransform(D3DMTS_VIEW,               // D3DTRANSFORMSTATETYPE State,
						                                            &ViewMatrix,               // CONST D3DMMATRIX* pMatrix
						                                            D3DMFMT_D3DMVALUE_FLOAT))) // D3DMFORMAT Format
						{
							DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetTransform VIEW failed. (hr = 0x%08x) Aborting.", hr);
							Result = TPR_ABORT;
							goto cleanup;
						}

						//
						// Perform TnL on input VB, store in output VB
						//
						if( FAILED( hr = m_pd3dDevice->ProcessVertices(0,         // UINT SrcStartIndex,
						                                               0,         // UINT DestIndex,
						                                               1,         // UINT VertexCount,
						                                               pOutVB,    // IDirect3DMobileVertexBuffer* pDestBuffer,
						                                               0)))       // DWORD Flags
						{
							DebugOut(DO_ERROR, L"IDirect3DMobileDevice::ProcessVertices failed. (hr = 0x%08x) Aborting.", hr);
							Result = TPR_ABORT;
							goto cleanup;
						}

						//
						// Lock results of ProcessVertices
						//
						if( FAILED( hr = pOutVB->Lock(0,                  // UINT OffsetToLock,
						                               uiOutVertSize,      // UINT SizeToLock,
						                              (VOID**)(&pFloats), // VOID** ppbData,
						                              0)))                // DWORD Flags
						{
							DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
							Result = TPR_ABORT;
							goto cleanup;
						}

						//
						// fResult should be the product of the multiplicands
						//
						fResult = pFloats[uiViewCol];

						//
						// Release the lock (the target value has been stored elsewhere)
						//
						pOutVB->Unlock();

						//
						// What is the difference between the expected result and the actual result, if
						// any?
						//
						dDelta = fabs(fResult - fExpectedResult);
	
						if (dLargestDelta < dDelta) dLargestDelta = dDelta;

						//
						// Is the difference unacceptable?
						//
						if (dDelta > m_fTestTolerance)
						{
							DebugOut(L"Mult operation: 2^(%li)*2^(%li) == 2^(%li) ",iExponentOne, iExponentTwo, iExpectedResultExp);

							DebugOut(L"[Op #%lu]:  Multiplicand1==World[%lu][%lu], Multiplicand2==View[%lu][%lu]",
								  uiMultOpNum, uiWorldRow, uiWorldCol, uiViewRow, uiViewCol);

							DebugOut(L"Invalid Result: 2^(%li) * 2^(%li) == %g       (expected: %g * %g == %g)", iExponentOne, iExponentTwo, fResult, fValOne, fValTwo, fExpectedResult);

							Result = TPR_FAIL;
							DebugOut(DO_ERROR, L"Failing.");

							uiNumFailures++;
							if (uiNumFailures >= uiFailureThreshhold)
							{
								DebugOut(L"Failure threshhold exceeded. Failing.");
								Result = TPR_FAIL; // Should have been set to this previously anyway
								goto cleanup;
							}
							
						}
						uiMultOpNum++;
					}
				}
			}
		}
	}

cleanup:

	DebugOut(L"Largest diff was: %g", dLargestDelta);
	DebugOut(L"Diff threshhold is: %g", m_fTestTolerance);

	//
	// Unlock if not already unlocked (no op if it is already unlocked)
	//
	if (pInVB)
		pInVB->Unlock();
	if (pOutVB)
		pOutVB->Unlock();

	//
	// Clean up the vertex buffers
	//
	if (pInVB)
		pInVB->Release();
	if (pOutVB)
		pOutVB->Release();

	//
	// Clear active vertex buffer ref-count
	//
	if (m_pd3dDevice)
		m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
									   NULL,  // IDirect3DMobileVertexBuffer* pStreamData
									   0);    // UINT Stride

	//
	// Return tux test case result
	//
	return Result;
}


//
// ExecuteAddTest
//  
//   This test verifies ProcessVertices transformation of vertex positions via the world and view matrices.
//   The technique used by this test is to confirm that addition of specific components is occuring as expected.
//
//   This test expects the viewport and projection to be set up to "cancel" each other out (the result of the
//   projection matrix mult with the viewport matrix should be the identity matrix).
//   
//   For simplicity, it is expected that clipping is off.
//
// Arguments:
//
//   None
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecuteAddTest()
{
    HRESULT hr;
	DebugOut(L"Beginning ExecuteAddTest.");

	//
	// Addends (translations) for X, Y, Z
	//
	float fAddendX, fAddendY, fAddendZ;

	//
	// Input and output Vertex buffers for ProcessVertices
	//
	IDirect3DMobileVertexBuffer *pInVB = NULL;
	IDirect3DMobileVertexBuffer *pOutVB = NULL;

	//
	// World and view transformation matrices for D3DM TnL
	//
	D3DMMATRIX WorldMatrix, ViewMatrix;

	//
	// FVF sizes for this test
	//
	CONST UINT uiInVertSize = sizeof(float)*3;  // X,Y,Z
	CONST UINT uiOutVertSize = sizeof(float)*4; // X,Y,Z,RHW

	//
	// Difference between expected value and actual value, per component
	//
	double dDeltaX, dDeltaY, dDeltaZ;
	double dLargestDelta = 0.0f;

	//
	// Should translation occur in view matrix, or world matrix?
	//
	UINT uiMatrix;

	//
	// Used for VB locks, to access floating point positional data (X,Y,Z,RHW)
	//
	float *pFloats = NULL;

	//
	// The test will return TPR_FAIL if even one result exceeds the tolerance, but to aid in
	// debugging the test will continue to execute until reaching the number of failures below.
	//
	CONST UINT uiFailureThreshhold = 25;
	UINT uiNumFailures = 0;

	//
	// Tux Test Case Result; all non-pass blocks set this to another value
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Disable clipping
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_CLIPPING, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Disable lighting
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_LIGHTING, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Cause viewport and projection to cancel each other out, for isolated world/view testing
	//
	if (FAILED(hr = SetDegenerateViewAndProj()))
	{
		DebugOut(DO_ERROR, L"SetDegenerateViewAndProj failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pOutVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DMFVF_XYZRHW_FLOAT, uiOutVertSize, D3DMUSAGE_DONOTCLIP);
	if (NULL == pOutVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pInVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DMFVF_XYZ_FLOAT, uiInVertSize, 0);
	if (NULL == pInVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}


	//
	// Lock the input vertex buffer
	//
	if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
	                             uiInVertSize,       // UINT SizeToLock,
	                             (VOID**)(&pFloats), // VOID** ppbData,
	                             0)))                // DWORD Flags
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Fill the input vertex buffer
	//
	pFloats[0] = 0.0f;
	pFloats[1] = 0.0f;
	pFloats[2] = 0.0f;

	//
	// Unlock the input vertex buffer
	//
	pInVB->Unlock();

	//
	// Iterate over a variety of addends
	//
	for(fAddendX = 0; fAddendX < D3DQA_PV_MAX_ADDEND; fAddendX+=D3DQA_PV_ADDEND_STEP)
	{
		for(fAddendY = 0; fAddendY < D3DQA_PV_MAX_ADDEND; fAddendY+=D3DQA_PV_ADDEND_STEP)
		{
			for(fAddendZ = 0; fAddendZ < D3DQA_PV_MAX_ADDEND; fAddendZ+=D3DQA_PV_ADDEND_STEP)
			{
				for(uiMatrix = 0; uiMatrix < 2; uiMatrix++)
				{
					float fResultX, fResultY, fResultZ;
					UINT uiAddOpNum = 1;

					//
					// Prepare the view and world matrices
					//
					ZeroMemory(&WorldMatrix,sizeof(D3DMMATRIX));
					ZeroMemory(&ViewMatrix,sizeof(D3DMMATRIX));

					//
					// Set the view and world matrices, initially, to identity
					//
					ViewMatrix.m[0][0] = ViewMatrix.m[1][1] = ViewMatrix.m[2][2] = ViewMatrix.m[3][3] = _M(1.0f);
					WorldMatrix.m[0][0] = WorldMatrix.m[1][1] = WorldMatrix.m[2][2] = WorldMatrix.m[3][3] = _M(1.0f);

					//
					// Initialize translation; either in the view or world matrix.
					//

					switch(uiMatrix)
					{
					case 0:
						ViewMatrix.m[3][0] = _M(fAddendX);
						ViewMatrix.m[3][1] = _M(fAddendY);
						ViewMatrix.m[3][2] = _M(fAddendZ);
						break;
					case 1:
						WorldMatrix.m[3][0] = _M(fAddendX);
						WorldMatrix.m[3][1] = _M(fAddendY);
						WorldMatrix.m[3][2] = _M(fAddendZ);
						break;
					}

					//
					// Indicate world matrix to D3DM
					//
					if( FAILED( hr = m_pd3dDevice->SetTransform(D3DMTS_WORLD,              // D3DTRANSFORMSTATETYPE State,
					                                            &WorldMatrix,              // CONST D3DMMATRIX* pMatrix
					                                            D3DMFMT_D3DMVALUE_FLOAT))) // D3DMFORMAT Format
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetTransform WORLD failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Indicate view matrix to D3DM
					//
					if( FAILED( hr = m_pd3dDevice->SetTransform(D3DMTS_VIEW,               // D3DTRANSFORMSTATETYPE State,
					                                            &ViewMatrix,               // CONST D3DMMATRIX* pMatrix
					                                            D3DMFMT_D3DMVALUE_FLOAT))) // D3DMFORMAT Format
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetTransform VIEW failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Perform TnL on input VB, store in output VB
					//
					if( FAILED( hr = m_pd3dDevice->ProcessVertices(0,         // UINT SrcStartIndex,
					                                               0,         // UINT DestIndex,
					                                               1,         // UINT VertexCount,
					                                               pOutVB,    // IDirect3DMobileVertexBuffer* pDestBuffer,
					                                               0)))       // DWORD Flags
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::ProcessVertices failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Lock results of ProcessVertices
					//
					if( FAILED( hr = pOutVB->Lock(0,                  // UINT OffsetToLock,
					                               uiOutVertSize,      // UINT SizeToLock,
					                               (VOID**)(&pFloats), // VOID** ppbData,
					                               0)))                // DWORD Flags
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Results should be translated based on addends
					//
					fResultX = pFloats[0];
					fResultY = pFloats[1];
					fResultZ = pFloats[2];

					//
					// Release the lock (the target value has been stored elsewhere)
					//
					pOutVB->Unlock();

					//
					// What is the difference between the expected result and the actual result, if
					// any?
					//
					dDeltaX = fabs(fResultX - fAddendX);
					dDeltaY = fabs(fResultY - fAddendY);
					dDeltaZ = fabs(fResultZ - fAddendZ);

					//
					// Is the difference unacceptable?
					//

					if ((dDeltaX > m_fTestTolerance) || (dDeltaY > m_fTestTolerance) || (dDeltaZ > m_fTestTolerance))
					{
						DebugOut(L"Add operation: X-Trans: %g, Y-Trans: %g, Z-Trans: %g", fAddendX, fAddendY, fAddendZ);

						if (dDeltaX > m_fTestTolerance)
						{
							if (dLargestDelta < dDeltaX) dLargestDelta = dDeltaX;
							DebugOut(L"Invalid X Result.  Expected: %g; Actual: %g",fAddendX,fResultX);
							Result = TPR_FAIL;
						}
						if (dDeltaY > m_fTestTolerance)
						{
							if (dLargestDelta < dDeltaY) dLargestDelta = dDeltaY;
							DebugOut(L"Invalid Y Result.  Expected: %g; Actual: %g",fAddendY,fResultY);
							Result = TPR_FAIL;
						}

						if (dDeltaZ > m_fTestTolerance)
						{
							if (dLargestDelta < dDeltaZ) dLargestDelta = dDeltaZ;
							DebugOut(L"Invalid Z Result.  Expected: %g; Actual: %g",fAddendZ,fResultZ);
							Result = TPR_FAIL;
						}
                        DebugOut(DO_ERROR, L"Failing.");
						uiNumFailures++;
						if (uiNumFailures >= uiFailureThreshhold)
						{
							DebugOut(L"Failure threshhold exceeded. Failing.");
							Result = TPR_FAIL; // Should have been set to this previously anyway
							goto cleanup;
						}

					}

					uiAddOpNum++;
				}
			}
		}
	}

cleanup:

	DebugOut(L"Largest diff was: %g", dLargestDelta);
	DebugOut(L"Diff threshhold is: %g", m_fTestTolerance);

	//
	// Unlock if not already unlocked (no op if it is already unlocked)
	//
	if (pInVB)
		pInVB->Unlock();
	if (pOutVB)
		pOutVB->Unlock();

	//
	// Clean up the vertex buffers
	//
	if (pInVB)
		pInVB->Release();
	if (pOutVB)
		pOutVB->Release();

	//
	// Clear active vertex buffer ref-count
	//
	if (m_pd3dDevice)
		m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
									   NULL,  // IDirect3DMobileVertexBuffer* pStreamData
									   0);    // UINT Stride

	//
	// Return tux test case result
	//
	return Result;
}

//
// ExecutePerspectiveProjTest
//  
//   This test exercises ProcessVertices in the user scenario of a perspective projection.
//
//   The technique used to verify this functionality is to attempt a variety of transforms
//   with various projection matrices, compute the expected results in QA code, then compare
//   the ProcessVertices result with the expected values.
//   
//   For simplicity, it is expected that clipping is off.
//
// Arguments:
//
//   None
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecutePerspectiveProjTest()
{
    HRESULT hr;
	DebugOut(L"Beginning ExecutePerspectiveProjTest.");

	//
	// FVF structures and bit masks for use with this test
	//
	#define D3DQA_PRJTEST_IN_FVF D3DMFVF_XYZ_FLOAT
	struct D3DQA_PRJTEST_IN
	{
		FLOAT x, y, z;
	};

	#define D3DQA_PRJTEST_OUT_FVF D3DMFVF_XYZRHW_FLOAT
	struct D3DQA_PRJTEST_OUT
	{
		FLOAT x, y, z, rhw;
	};

	//
	// For vertex buffer locks
	//
	D3DQA_PRJTEST_OUT *pOutPos = NULL;
	D3DQA_PRJTEST_IN *pInPos = NULL;

	//
	// Because this test is not particularly concerned with aspect ratios,
	// a simplistic default is set.
	//
	CONST float fAspect = 1.0f;

	//
	// A simple default is set, for field-of-view
	//
	CONST float fFov = ((2*D3DQA_PI)*(1.0f/6.0f));

	//
	// Iterator for verts in the per-iteration VB
	//
	UINT uiVertIter;

	//
	// Input and output Vertex buffers for ProcessVertices
	//
	IDirect3DMobileVertexBuffer *pInVB = NULL;
	IDirect3DMobileVertexBuffer *pOutVB = NULL;

	//
	// FVF sizes for this test
	//
	CONST UINT uiInVertSize = sizeof(D3DQA_PRJTEST_IN);  // X,Y,Z
	CONST UINT uiOutVertSize = sizeof(D3DQA_PRJTEST_OUT);// X,Y,Z,RHW

	//
	// Iterators for near and far planes
	//
	float fNear, fFar;

	//
	// Device viewport
	//
	D3DMVIEWPORT Viewport;

	//
	// An intermediate value for Z tranformation computation
	//
	float fTempZ;

	//
	// Iteration counter (for debug output purposes)
	//
	UINT uiIter = 1;

	//
	// Per-component values for confirming ProcessVertices success
	//
	float fExpectedX, fExpectedY, fExpectedZ, fExpectedRHW;
	double dDeltaX, dDeltaY, dDeltaZ, dDeltaRHW;
	double dLargestDelta = 0.0f;

	//
	// Projection matrix that is modified based on near/far planes during test iterations
	//
	D3DMMATRIX ProjMatrix;

	//
	// Iterator for selecting a set of points to process
	//
	UINT uiXStep, uiYStep;

	//
	// The test will return TPR_FAIL if even one result exceeds the tolerance, but to aid in
	// debugging the test will continue to execute until reaching the number of failures below.
	//
	CONST UINT uiFailureThreshhold = 25;
	UINT uiNumFailures = 0;

	//
	// Tux Test Case Result; all non-pass blocks set this to another value
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Disable clipping
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_CLIPPING, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Disable lighting
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_LIGHTING, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	if (FAILED(hr = m_pd3dDevice->GetViewport(&Viewport)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::GetViewport failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pOutVB = CreateActiveBuffer(m_pd3dDevice, D3DQA_PV_PROJ_VERTS, D3DMFVF_XYZRHW_FLOAT, uiOutVertSize, D3DMUSAGE_DONOTCLIP);
	if (NULL == pOutVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pInVB = CreateActiveBuffer(m_pd3dDevice, D3DQA_PV_PROJ_VERTS, D3DMFVF_XYZ_FLOAT, uiInVertSize, 0);
	if (NULL == pInVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Iterate through various combinations of near/far planes, tested points
	//
	for(fNear = D3DQA_PV_NEAR_STEP; fNear < D3DQA_PV_MAX_NEAR; fNear+=D3DQA_PV_NEAR_STEP)
	{
		for(fFar = fNear+D3DQA_PV_FAR_STEP; fFar < D3DQA_PV_MAX_FAR; fFar+=D3DQA_PV_FAR_STEP)
		{

			//
			// Retval is just a pointer to ProjMatrix; no need to examine it
			//
			(void)D3DMatrixPerspectiveFovLH( &ProjMatrix,// D3DMATRIX* pOut,
			                                 fFov,       // FLOAT fovy,
			                                 fAspect,    // FLOAT Aspect,
			                                 fNear,      // FLOAT zn,
			                                 fFar );     // FLOAT zf
                                   
			//
			// Set the projection matrix for transformation
			//
			if( FAILED( hr = m_pd3dDevice->SetTransform( D3DMTS_PROJECTION, &ProjMatrix, D3DMFMT_D3DMVALUE_FLOAT)))
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetTransform failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}
					
			for(uiXStep = 0; uiXStep <= D3DQA_PV_PROJ_XDIVS; uiXStep++)
			{
				//
				// Multiplier range: [-1.0f, 1.0f]
				//
				float fMultiplierX = (uiXStep/(float)D3DQA_PV_PROJ_XDIVS) * 2.0f - 1.0f;

				for(uiYStep = 0; uiYStep <= D3DQA_PV_PROJ_YDIVS; uiYStep++)
				{
					//
					// Multiplier range: [-1.0f, 1.0f]
					//
					float fMultiplierY = (uiYStep/(float)D3DQA_PV_PROJ_YDIVS) * 2.0f - 1.0f;

					//
					// Lock the input vertex buffer
					//
					if( FAILED( hr = pInVB->Lock(0,                               // UINT OffsetToLock,
					                              uiInVertSize*D3DQA_PV_PROJ_VERTS,// UINT SizeToLock,
					                              (VOID**)(&pInPos),               // VOID** ppbData,
					                              0)))                             // DWORD Flags
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Fill the input vertex buffer
					//
					for (uiVertIter=0; uiVertIter < D3DQA_PV_PROJ_VERTS; uiVertIter++)
					{
						//
						// Z
						// 
						pInPos[uiVertIter].z = fNear + (uiVertIter+1) * ((fFar-fNear)/D3DQA_PV_PROJ_VERTS);

						//
						// X
						// 
						pInPos[uiVertIter].x = fMultiplierX * (pInPos[uiVertIter].z * (float)tan (0.5 * fFov));

						//
						// Y
						// 
						pInPos[uiVertIter].y = fMultiplierY * (pInPos[uiVertIter].z * (float)tan (0.5 * fFov));
					}

					//
					// Unlock the input vertex buffer
					//
					pInVB->Unlock();



					//
					// Perform TnL on input VB, store in output VB
					//
					if( FAILED( hr = m_pd3dDevice->ProcessVertices(0,                   // UINT SrcStartIndex,
					                                                0,                   // UINT DestIndex,
					                                                D3DQA_PV_PROJ_VERTS, // UINT VertexCount,
					                                                pOutVB,              // IDirect3DMobileVertexBuffer* pDestBuffer,
					                                                0)))                 // DWORD Flags
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::ProcessVertices failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Lock results of ProcessVertices
					//
					if( FAILED( hr = pOutVB->Lock(0,                                 // UINT OffsetToLock,
					                                uiOutVertSize*D3DQA_PV_PROJ_VERTS, // UINT SizeToLock,
					                                (VOID**)(&pOutPos),                // VOID** ppbData,
					                                0)))                               // DWORD Flags
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Examine the output vertex buffer
					//
					for (uiVertIter=0; uiVertIter < D3DQA_PV_PROJ_VERTS; uiVertIter++)
					{
						//
						// Transformed X
						//
						fExpectedX = (uiXStep/(float)D3DQA_PV_PROJ_XDIVS) * Viewport.Width;
						dDeltaX = fabs(pOutPos[uiVertIter].x - fExpectedX);

						//
						// Transformed Y (note expected inversion due to projection)
						// 
						fExpectedY = Viewport.Height - (uiYStep/(float)D3DQA_PV_PROJ_YDIVS) * Viewport.Height;
						dDeltaY = fabs(pOutPos[uiVertIter].y - fExpectedY);

						//
						// Transformed Z, RHW
						//
						fTempZ = (fNear + (uiVertIter+1) * ((fFar-fNear)/D3DQA_PV_PROJ_VERTS));
						fExpectedRHW = 1.0f / fTempZ;
						fExpectedZ = fTempZ*(fFar / (fFar-fNear));
						fExpectedZ -= (fFar / (fFar-fNear)) * fNear;
						fExpectedZ = fExpectedZ * (1.0f / fTempZ);
						dDeltaZ = fabs(pOutPos[uiVertIter].z - fExpectedZ);
						dDeltaRHW = fabs(pOutPos[uiVertIter].rhw - fExpectedRHW);

						//
						// Exceeds previous largest diff?
						//
						if (dLargestDelta < dDeltaX) dLargestDelta = dDeltaX;
						if (dLargestDelta < dDeltaY) dLargestDelta = dDeltaY;
						if (dLargestDelta < dDeltaZ) dLargestDelta = dDeltaZ;
						if (dLargestDelta < dDeltaRHW) dLargestDelta = dDeltaRHW;

						if ((dDeltaX > (D3DQA_PV_ADDEND_STEP + m_fTestTolerance)) || (dDeltaY > (D3DQA_PV_ADDEND_STEP + m_fTestTolerance)) || (dDeltaZ > m_fTestTolerance) || (dDeltaRHW > m_fTestTolerance))
						{
							DebugOut(L"Clipping plane pair.  Near: %g, Far: %g.",fNear,fFar);

							DebugOut(L"[%lu] Processed Vert (Expected, Actual, Delta):  X(%g, %g, %g); Y(%g, %g, %g); Z(%g, %g, %g); RHW(%g, %g, %g)",
								  uiIter,
								  fExpectedX, pOutPos[uiVertIter].x, dDeltaX, 
								  fExpectedY, pOutPos[uiVertIter].y, dDeltaY, 
								  fExpectedZ, pOutPos[uiVertIter].z, dDeltaZ,
								  fExpectedRHW, pOutPos[uiVertIter].rhw, dDeltaRHW);


							//
							// Is the difference unacceptable?
							//
							if (dDeltaX > (D3DQA_PV_ADDEND_STEP + m_fTestTolerance))
							{
								DebugOut(L"X Delta exceeds tolerance. Failing.");
								Result = TPR_FAIL;
							}
							if (dDeltaY > (D3DQA_PV_ADDEND_STEP + m_fTestTolerance))
							{
								DebugOut(L"Y Delta exceeds tolerance. Failing.");
								Result = TPR_FAIL;
							}
							if (dDeltaZ > m_fTestTolerance)
							{
								DebugOut(L"Z Delta exceeds tolerance. Failing.");
								Result = TPR_FAIL;
							}
							if (dDeltaRHW > m_fTestTolerance)
							{
								DebugOut(L"RHW Delta exceeds tolerance. Failing.");
								Result = TPR_FAIL;
							}

							DebugOut(DO_ERROR, L"Failing.");

							uiNumFailures++;
							if (uiNumFailures >= uiFailureThreshhold)
							{
								DebugOut(L"Failure threshhold exceeded. Failing.");
								Result = TPR_FAIL; // Should have been set to this previously anyway
								goto cleanup;
							}


						}
						uiIter++;
					}

					//
					// Release the lock (the target value has been stored elsewhere)
					//
					pOutVB->Unlock();

				}
			}
		}
	}

cleanup:

	DebugOut(L"Largest diff was: %g", dLargestDelta);
	DebugOut(L"Diff threshhold is: %g", m_fTestTolerance);


	//
	// Unlock if not already unlocked (no op if it is already unlocked)
	//
	if (pInVB)
		pInVB->Unlock();
	if (pOutVB)
		pOutVB->Unlock();

	//
	// Clean up the vertex buffers
	//
	if (pInVB)
		pInVB->Release();
	if (pOutVB)
		pOutVB->Release();

	//
	// Clear active vertex buffer ref-count
	//
	if (m_pd3dDevice)
		m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
									   NULL,  // IDirect3DMobileVertexBuffer* pStreamData
									   0);    // UINT Stride

	//
	// Return tux test case result
	//
	return Result;
}

//
// ExecuteOrthoProjTest
//  
//   This test exercises ProcessVertices in the user scenario of an orthographic projection.
//
//   The technique used to verify this functionality is to attempt a variety of transforms
//   with various projection matrices, compute the expected results in QA code, then compare
//   the ProcessVertices result with the expected values.
//   
//   For simplicity, it is expected that clipping is off.
//
// Arguments:
//
//   None
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecuteOrthoProjTest()
{
    HRESULT hr;
    
	DebugOut(L"Beginning ExecuteOrthoProjTest.");

	//
	// FVF structures and bit masks for use with this test
	//
	#define D3DQA_ORTTEST_IN_FVF D3DMFVF_XYZ_FLOAT
	struct D3DQA_ORTTEST_IN
	{
		FLOAT x, y, z;
	};

	#define D3DQA_ORTTEST_OUT_FVF D3DMFVF_XYZRHW_FLOAT
	struct D3DQA_ORTTEST_OUT
	{
		FLOAT x, y, z, rhw;
	};

	//
	// Because this test is not particularly concerned with aspect ratios,
	// a simplistic default is set.
	//
	CONST float fAspect = 1.0f;

	//
	// A simple default is set, for field-of-view
	//
	CONST float fFov = ((2*D3DQA_PI)*(1.0f/6.0f));

	//
	// Iterator for verts in the per-iteration VB
	//
	UINT uiVertIter;

	//
	// Input and output Vertex buffers for ProcessVertices
	//
	IDirect3DMobileVertexBuffer *pInVB = NULL;
	IDirect3DMobileVertexBuffer *pOutVB = NULL;

	//
	// FVF sizes for this test
	//
	CONST UINT uiInVertSize = sizeof(D3DQA_ORTTEST_IN);   // X,Y,Z
	CONST UINT uiOutVertSize = sizeof(D3DQA_ORTTEST_OUT); // X,Y,Z,RHW

	//
	// Used for VB locks, to access floating point positional data (X,Y,Z,RHW)
	//
	D3DQA_ORTTEST_IN *pInPos = NULL;
	D3DQA_ORTTEST_OUT *pOutPos = NULL;

	//
	// Iterators for ortho view volume
	//
	float fNear, fFar, fWidth, fHeight;

	//
	// Device viewport
	//
	D3DMVIEWPORT Viewport;

	//
	// An intermediate value for Z tranformation computation
	//
	float fTempZ;
	
	//
	// Iteration counter (for debug output purposes)
	//
	UINT uiIter = 1;

	//
	// Per-component values for confirming ProcessVertices success
	//
	float fExpectedX, fExpectedY, fExpectedZ, fExpectedRHW;
	double dDeltaX, dDeltaY, dDeltaZ, dDeltaRHW;
	double dLargestDelta = 0.0f;

	//
	// Projection matrix that is modified based on near/far planes during test iterations
	//
	D3DMMATRIX ProjMatrix;

	//
	// Iterator for selecting a set of points to process
	//
	UINT uiXStep, uiYStep;

	//
	// The test will return TPR_FAIL if even one result exceeds the tolerance, but to aid in
	// debugging the test will continue to execute until reaching the number of failures below.
	//
	CONST UINT uiFailureThreshhold = 25;
	UINT uiNumFailures = 0;

	//
	// Tux Test Case Result; all non-pass blocks set this to another value
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Disable clipping
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_CLIPPING, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Disable lighting
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_LIGHTING, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	if (FAILED(hr = m_pd3dDevice->GetViewport(&Viewport)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::GetViewport failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pOutVB = CreateActiveBuffer(m_pd3dDevice, D3DQA_PV_PROJ_VERTS, D3DQA_ORTTEST_OUT_FVF, uiOutVertSize, D3DMUSAGE_DONOTCLIP);
	if (NULL == pOutVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pInVB = CreateActiveBuffer(m_pd3dDevice, D3DQA_PV_PROJ_VERTS, D3DQA_ORTTEST_IN_FVF, uiInVertSize, 0);
	if (NULL == pInVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Iterate through various combinations of near/far planes, tested points
	//
	for(fNear = D3DQA_PV_NEAR_STEP; fNear < D3DQA_PV_MAX_NEAR; fNear+=D3DQA_PV_NEAR_STEP)
	{
		for(fFar = fNear+D3DQA_PV_FAR_STEP; fFar < D3DQA_PV_MAX_FAR; fFar+=D3DQA_PV_FAR_STEP)
		{
			for(fWidth = D3DQA_PV_ORTH_MIN_WIDTH; fWidth <= D3DQA_PV_ORTH_MAX_WIDTH; fWidth+=D3DQA_PV_ORTH_WIDTH_STEP)
			{
				for(fHeight = D3DQA_PV_ORTH_MIN_HEIGHT; fHeight <= D3DQA_PV_ORTH_MAX_HEIGHT; fHeight+=D3DQA_PV_ORTH_HEIGHT_STEP)
				{

					//
					// Retval is just a pointer to ProjMatrix; no need to examine it
					//

					(void)D3DMatrixOrthoLH(&ProjMatrix, // D3DXMATRIX *pOut
					                        fWidth,     // float w
					                        fHeight,    // float h
					                        fNear,      // float zn
					                        fFar);      // float zf 

                               
					//
					// Set the projection matrix for transformation
					//
					if( FAILED( hr = m_pd3dDevice->SetTransform( D3DMTS_PROJECTION, &ProjMatrix, D3DMFMT_D3DMVALUE_FLOAT )))
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetTransform failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}
						
					for(uiXStep = 0; uiXStep <= D3DQA_PV_PROJ_XDIVS; uiXStep++)
					{
						//
						// Range: [-fWidth/2.0f, fWidth/2.0f]
						//
						float fX = (uiXStep/(float)D3DQA_PV_PROJ_XDIVS) * 2.0f - 1.0f;

						for(uiYStep = 0; uiYStep <= D3DQA_PV_PROJ_YDIVS; uiYStep++)
						{
							//
							// Range: [-fHeight/2.0f, fHeight/2.0f]
							//
							float fY = (uiYStep/(float)D3DQA_PV_PROJ_YDIVS) * 2.0f - 1.0f;

							//
							// Lock the input vertex buffer
							//
							if( FAILED( hr = pInVB->Lock(0,                               // UINT OffsetToLock,
							                                uiInVertSize*D3DQA_PV_PROJ_VERTS,// UINT SizeToLock,
							                                (VOID**)(&pInPos),               // VOID** ppbData,
							                                0)))                             // DWORD Flags
							{
								DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
								Result = TPR_ABORT;
								goto cleanup;
							}

							//
							// Fill the input vertex buffer
							//
							for (uiVertIter=0; uiVertIter < D3DQA_PV_PROJ_VERTS; uiVertIter++)
							{
								//
								// X
								// 
								pInPos[uiVertIter].x = fX;

								//
								// Y
								// 
								pInPos[uiVertIter].y = fY;
								
								//
								// Z
								// 
								pInPos[uiVertIter].z = fNear + (uiVertIter+1) * ((fFar-fNear)/D3DQA_PV_PROJ_VERTS);
							}

							//
							// Unlock the input vertex buffer
							//
							pInVB->Unlock();



							//
							// Perform TnL on input VB, store in output VB
							//
							if( FAILED( hr = m_pd3dDevice->ProcessVertices(0,                   // UINT SrcStartIndex,
							                                                0,                   // UINT DestIndex,
							                                                D3DQA_PV_PROJ_VERTS, // UINT VertexCount,
							                                                pOutVB,              // IDirect3DMobileVertexBuffer* pDestBuffer,
							                                                0)))                 // DWORD Flags
							{
								DebugOut(DO_ERROR, L"IDirect3DMobileDevice::ProcessVertices failed. (hr = 0x%08x) Aborting.", hr);
								Result = TPR_ABORT;
								goto cleanup;
							}

							//
							// Lock results of ProcessVertices
							//
							if( FAILED( hr = pOutVB->Lock(0,                                 // UINT OffsetToLock,
							                              uiOutVertSize*D3DQA_PV_PROJ_VERTS, // UINT SizeToLock,
							                              (VOID**)(&pOutPos),                // VOID** ppbData,
							                              0)))                               // DWORD Flags
							{
								DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
								Result = TPR_ABORT;
								goto cleanup;
							}

							//
							// Examine the output vertex buffer
							//
							for (uiVertIter=0; uiVertIter < D3DQA_PV_PROJ_VERTS; uiVertIter++)
							{
								//
								// Transformed X
								//
								fExpectedX = (1 + fX*(2.0f/fWidth)) * ((float)Viewport.Width / 2.0f);
								dDeltaX = fabs(pOutPos[uiVertIter].x - fExpectedX);

								//
								// Transformed Y (inverted due to projection)
								// 
								fExpectedY = (1 - fY*(2.0f/fHeight)) * ((float)Viewport.Height / 2.0f);;
								dDeltaY = fabs(pOutPos[uiVertIter].y - fExpectedY);

								//
								// Transformed Z
								//	
								fTempZ = (1.0f/(fFar - fNear));
								fExpectedZ = fNear + (uiVertIter+1) * ((fFar-fNear)/D3DQA_PV_PROJ_VERTS);
								fExpectedZ *= fTempZ;
								fExpectedZ -= fTempZ * fNear;
								dDeltaZ = fabs(pOutPos[uiVertIter].z - fExpectedZ);
								
								// 
								// RHW
								// 
								fExpectedRHW = 1.0f;
								dDeltaRHW = fabs(pOutPos[uiVertIter].rhw - fExpectedRHW);


								//
								// Exceeds previous largest diff?
								//
								if (dLargestDelta < dDeltaX) dLargestDelta = dDeltaX;
								if (dLargestDelta < dDeltaY) dLargestDelta = dDeltaY;
								if (dLargestDelta < dDeltaZ) dLargestDelta = dDeltaZ;
								if (dLargestDelta < dDeltaRHW) dLargestDelta = dDeltaRHW;
								
								if ((dDeltaX > (D3DQA_PV_ADDEND_STEP + m_fTestTolerance)) || (dDeltaY > (D3DQA_PV_ADDEND_STEP + m_fTestTolerance)) || (dDeltaZ > m_fTestTolerance) || (dDeltaRHW > m_fTestTolerance))
								{
									DebugOut(L"Ortho volume.  Near: %g, Far: %g, Width: %g, Height: %g.", fNear, fFar, fWidth, fHeight);

									DebugOut(L"[%lu] Processed Vert (Expected, Actual, Delta):  X(%g, %g, %g); Y(%g, %g, %g); Z(%g, %g, %g); RHW(%g, %g, %g)",
										  uiIter,
										  fExpectedX, pOutPos[uiVertIter].x, dDeltaX, 
										  fExpectedY, pOutPos[uiVertIter].y, dDeltaY, 
										  fExpectedZ, pOutPos[uiVertIter].z, dDeltaZ,
										  fExpectedRHW, pOutPos[uiVertIter].rhw, dDeltaRHW);

									//
									// Is the difference unacceptable?
									//
									if (dDeltaX > (D3DQA_PV_ADDEND_STEP + m_fTestTolerance))
									{
										DebugOut(L"X Delta exceeds tolerance. Failing.");
										Result = TPR_FAIL;
									}
									if (dDeltaY > (D3DQA_PV_ADDEND_STEP + m_fTestTolerance))
									{
										DebugOut(L"Y Delta exceeds tolerance. Failing.");
										Result = TPR_FAIL;
									}
									if (dDeltaZ > m_fTestTolerance)
									{
										DebugOut(L"Z Delta exceeds tolerance. Failing.");
										Result = TPR_FAIL;
									}
									if (dDeltaRHW > m_fTestTolerance)
									{
										DebugOut(L"RHW Delta exceeds tolerance. Failing.");
										Result = TPR_FAIL;
									}

									DebugOut(DO_ERROR, L"Failing.");

									uiNumFailures++;
									if (uiNumFailures >= uiFailureThreshhold)
									{
										DebugOut(L"Failure threshhold exceeded. Failing.");
										Result = TPR_FAIL; // Should have been set to this previously anyway
										goto cleanup;
									}

								}
								uiIter++;
							}

							//
							// Release the lock (the target value has been stored elsewhere)
							//
							pOutVB->Unlock();
						}
					}
				}
			}
		}
	}

cleanup:

	DebugOut(L"Largest diff was: %g", dLargestDelta);
	DebugOut(L"Diff threshold is: %g", m_fTestTolerance);


	//
	// Unlock if not already unlocked (no op if it is already unlocked)
	//
	if (pInVB)
		pInVB->Unlock();
	if (pOutVB)
		pOutVB->Unlock();

	//
	// Clean up the vertex buffers
	//
	if (pInVB)
		pInVB->Release();
	if (pOutVB)
		pOutVB->Release();

	//
	// Clear active vertex buffer ref-count
	//
	if (m_pd3dDevice)
		m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
									   NULL,  // IDirect3DMobileVertexBuffer* pStreamData
									   0);    // UINT Stride

	//
	// Return tux test case result
	//
	return Result;
}



//
// ExecuteViewportTest
//  
//   This test transforms vertices (ProcessVertices) with a variety of different
//   viewport sizes and offsets.
//   
//   For simplicity, it is expected that clipping is off.
//
// Arguments:
//
//   None
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecuteViewportTest()
{
    HRESULT hr;

	DebugOut(L"Beginning ExecuteViewportTest.");

	//
	// A simple default is set, for field-of-view
	//
	CONST float fFov = ((2*D3DQA_PI)*(1.0f/6.0f));

	//
	// Input and output Vertex buffers for ProcessVertices
	//
	IDirect3DMobileVertexBuffer *pInVB = NULL;
	IDirect3DMobileVertexBuffer *pOutVB = NULL;

	//
	// FVF structures and bit masks for use with this test
	//
	#define D3DQA_VWPTEST_IN_FVF D3DMFVF_XYZ_FLOAT
	struct D3DQA_VWPTEST_IN
	{
		FLOAT x, y, z;
	};

	#define D3DQA_VWPTEST_OUT_FVF D3DMFVF_XYZRHW_FLOAT
	struct D3DQA_VWPTEST_OUT
	{
		FLOAT x, y, z, rhw;
	};

	//
	// For vertex buffer locks
	//
	D3DQA_VWPTEST_IN *pInPos = NULL;
	D3DQA_VWPTEST_OUT *pOutPos = NULL;

	//
	// FVF sizes for this test
	//
	CONST UINT uiInVertSize = sizeof(D3DQA_VWPTEST_IN);   // X,Y,Z
	CONST UINT uiOutVertSize = sizeof(D3DQA_VWPTEST_OUT); // X,Y,Z,RHW

	//
	// Device viewport
	//
	D3DMVIEWPORT Viewport;
	
	//
	// Iteration counter (for debug output purposes)
	//
	UINT uiIter = 1;

	//
	// Per-component values for confirming ProcessVertices success
	//
	float fExpectedX, fExpectedY, fExpectedZ, fExpectedRHW;
	double dDeltaX, dDeltaY, dDeltaZ, dDeltaRHW;
	double dLargestDelta = 0.0f;

	//
	// Projection matrix that is modified based on near/far planes during test iterations
	//
	D3DMMATRIX ProjMatrix;

	//
	// Render target extents
	//
	UINT uiRTMaxX, uiRTMaxY;

	//
	// Render target surface
	//
	IDirect3DMobileSurface* pRenderTarget = NULL;

	//
	// Viewport offsets
	//
	UINT uiVPOffsetX, uiVPOffsetY;

	//
	// Render target surface description
	//
	D3DMSURFACE_DESC SurfDescRT;

	//
	// The test will return TPR_FAIL if even one result exceeds the tolerance, but to aid in
	// debugging the test will continue to execute until reaching the number of failures below.
	//
	CONST UINT uiFailureThreshhold = 25;
	UINT uiNumFailures = 0;

	//
	// Tux Test Case Result; all non-pass blocks set this to another value
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Disable clipping
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_CLIPPING, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Disable lighting
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_LIGHTING, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	if (FAILED(hr = m_pd3dDevice->GetViewport(&Viewport)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::GetViewport failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pOutVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DQA_VWPTEST_OUT_FVF, uiOutVertSize, D3DMUSAGE_DONOTCLIP);
	if (NULL == pOutVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pInVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DQA_VWPTEST_IN_FVF, uiInVertSize, 0);
	if (NULL == pInVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// The current display mode is needed, to determine desired format
	//
	if( FAILED( hr = m_pd3dDevice->GetRenderTarget( &pRenderTarget  ))) // IDirect3DMobileSurface** ppRenderTarget
	{
		DebugOut(DO_ERROR, L"Unable to get render target. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	if( FAILED( hr = pRenderTarget->GetDesc(&SurfDescRT))) // D3DMSURFACE_DESC*
	{
		DebugOut(DO_ERROR, L"Unable to determine current display mode. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	SAFERELEASE(pRenderTarget);

	//
	// Lock the input vertex buffer
	//
	if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
	                              uiInVertSize,       // UINT SizeToLock,
	                              (VOID**)(&pInPos),  // VOID** ppbData,
	                              0)))                // DWORD Flags
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Fill the input vertex buffer
	//
	pInPos->x = 0.0f;
	pInPos->y = 0.0f;
	pInPos->z = 0.0f;

	//
	// Unlock the input vertex buffer
	//
	pInVB->Unlock();

	//
	// Maximum extents
	//
	uiRTMaxX = SurfDescRT.Width;
	uiRTMaxY = SurfDescRT.Height;

	for (uiVPOffsetX = 0; uiVPOffsetX < uiRTMaxX; uiVPOffsetX++)
	{
		for (uiVPOffsetY = 0; uiVPOffsetY < uiRTMaxY; uiVPOffsetY++)
		{
			
			D3DMVIEWPORT ViewPort;

			//
			// Pixel coordinates of the upper-left corner of the viewport 
			//
			ViewPort.X = uiVPOffsetX;
			ViewPort.Y = uiVPOffsetY;

			//
			//Dimensions of the viewport on the render target surface, in pixels. 
			//
			ViewPort.Width = (uiRTMaxX - uiVPOffsetX);
			ViewPort.Height = (uiRTMaxY - uiVPOffsetY);

			//
			// Values describing the range of depth values into which a scene is
			// to be rendered, the minimum and maximum values of the clip volume.
			//
			ViewPort.MinZ = 0.0f;
			ViewPort.MaxZ = 1.0f;

			//
			// Set the Viewport (Direct3D will create a matrix representation of the viewport
			// based on the parameters that are set in the D3DMVIEWPORT structure)
			//
			if( FAILED( hr = m_pd3dDevice->SetViewport( &ViewPort )))
			{
				DebugOut(DO_ERROR, L"Failed at SetViewport (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}

			//
			// Retval is just a pointer to ProjMatrix; no need to examine it
			//
			(void)D3DMatrixOrthoLH( &ProjMatrix,  // D3DXMATRIX *pOut
			                        (float)ViewPort.Width,     // float w
			                        (float)ViewPort.Height,    // float h
			                        D3DQA_PV_VPTEST_CLIP_NEAR, // float zn
			                        D3DQA_PV_VPTEST_CLIP_FAR); // float zf 
           
			//
			// Set the projection matrix for transformation
			//
			if( FAILED( hr = m_pd3dDevice->SetTransform( D3DMTS_PROJECTION, &ProjMatrix, D3DMFMT_D3DMVALUE_FLOAT )))
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetTransform failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}


			//
			// Perform TnL on input VB, store in output VB
			//
			if( FAILED( hr = m_pd3dDevice->ProcessVertices(0,         // UINT SrcStartIndex,
			                                          0,         // UINT DestIndex,
			                                          1,         // UINT VertexCount,
			                                          pOutVB,    // IDirect3DMobileVertexBuffer* pDestBuffer,
			                                          0)))       // DWORD Flags
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileDevice::ProcessVertices failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}

			//
			// Lock results of ProcessVertices
			//
			if( FAILED( hr = pOutVB->Lock(0,                  // UINT OffsetToLock,
			                         uiOutVertSize,      // UINT SizeToLock,
			                         (VOID**)(&pOutPos), // VOID** ppbData,
			                         0)))                // DWORD Flags
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}

			//
			// Examine the output vertex buffer
			//
			//

			// Transformed X
			//
			fExpectedX = ViewPort.X + (ViewPort.Width / 2.0f);
			dDeltaX = fabs(pOutPos->x - fExpectedX);

			//
			// Transformed Y
			// 
			fExpectedY = ViewPort.Y + (ViewPort.Height / 2.0f);
			dDeltaY = fabs(pOutPos->y - fExpectedY);

			//
			// Transformed Z
			//	
			fExpectedZ = (-1.0f)*(D3DQA_PV_VPTEST_CLIP_NEAR)* (1.0f / (D3DQA_PV_VPTEST_CLIP_FAR - D3DQA_PV_VPTEST_CLIP_NEAR));
			dDeltaZ = fabs(pOutPos->z - fExpectedZ);
			
			// 
			// RHW
			// 
			fExpectedRHW = 1.0f;
			dDeltaRHW = fabs(pOutPos->rhw - fExpectedRHW);

			//
			// Exceeds previous largest diff?
			//
			if (dLargestDelta < dDeltaX) dLargestDelta = dDeltaX;
			if (dLargestDelta < dDeltaY) dLargestDelta = dDeltaY;
			if (dLargestDelta < dDeltaZ) dLargestDelta = dDeltaZ;
			if (dLargestDelta < dDeltaRHW) dLargestDelta = dDeltaRHW;

			if ((dDeltaX > m_fTestTolerance) || (dDeltaY > m_fTestTolerance) || (dDeltaZ > m_fTestTolerance) || (dDeltaRHW > m_fTestTolerance))
			{
				DebugOut(L"[%lu] Processed Vert (Expected, Actual, Delta):  X(%g, %g, %g); Y(%g, %g, %g); Z(%g, %g, %g); RHW(%g, %g, %g)",
					  uiIter,
					  fExpectedX, pOutPos->x, dDeltaX, 
					  fExpectedY, pOutPos->y, dDeltaY, 
					  fExpectedZ, pOutPos->z, dDeltaZ,
					  fExpectedRHW, pOutPos->rhw, dDeltaRHW);

				//
				// Is the difference unacceptable?
				//
				if (dDeltaX > m_fTestTolerance)
				{
					DebugOut(L"X Delta exceeds tolerance. Failing.");
					Result = TPR_FAIL;
				}
				if (dDeltaY > m_fTestTolerance)
				{
					DebugOut(L"Y Delta exceeds tolerance. Failing.");
					Result = TPR_FAIL;
				}
				if (dDeltaZ > m_fTestTolerance)
				{
					DebugOut(L"Z Delta exceeds tolerance. Failing.");
					Result = TPR_FAIL;
				}
				if (dDeltaRHW > m_fTestTolerance)
				{
					DebugOut(L"RHW Delta exceeds tolerance. Failing.");
					Result = TPR_FAIL;
				}

				DebugOut(DO_ERROR, L"Failing.");

				uiNumFailures++;
				if (uiNumFailures >= uiFailureThreshhold)
				{
					DebugOut(L"Failure threshhold exceeded. Failing.");
					Result = TPR_FAIL; // Should have been set to this previously anyway
					goto cleanup;
				}

			}
			uiIter++;

			//
			// Release the lock (the target value has been stored elsewhere)
			//
			pOutVB->Unlock();
		}
	}

cleanup:

	DebugOut(L"Largest diff was: %g", dLargestDelta);
	DebugOut(L"Diff threshhold is: %g", m_fTestTolerance);


	//
	// Unlock if not already unlocked (no op if it is already unlocked)
	//
	if (pInVB)
		pInVB->Unlock();
	if (pOutVB)
		pOutVB->Unlock();

	//
	// Clean up the vertex buffers
	//
	if (pInVB)
		pInVB->Release();
	if (pOutVB)
		pOutVB->Release();

	//
	// Clear active vertex buffer ref-count
	//
	if (m_pd3dDevice)
		m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
									   NULL,  // IDirect3DMobileVertexBuffer* pStreamData
									   0);    // UINT Stride

	//
	// Return tux test case result
	//
	return Result;
}



//
// ExecuteDiffuseMaterialSourceTest
//  
//   This test verifies that ProcessVertices computes diffuse lighting correctly,
//   with any of the three valid settings for the diffuse color source.  The diffuse
//   color source can be any of the following:
//
//      * The diffuse color in the vertex
//      * The specular color in the vertex
//      * The material's diffuse color value
//
//   Per iteration, this test uses a single directional light to illuminate a single
//   vertex via ProcessVertices.  The direction of the light and the normal of the
//   vertex are contrived in a manner that eliminates the angle of incidence as a
//   factor in the lighting computation.  This allows the test to most easily
//   verify that the correct diffuse color source is being used in the computation,
//   without additional complication.
//   
//   For simplicity, it is expected that clipping is off.
//
// Arguments:
//
//   None
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecuteDiffuseMaterialSourceTest()
{
    HRESULT hr;
    
	DebugOut(L"Beginning ExecuteDiffuseMaterialSourceTest.");

	//
	// FVF structures and bit masks for use with this test
	//
	#define D3DQA_DMSTEST_IN_FVF D3DMFVF_XYZ_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_NORMAL_FLOAT | D3DMFVF_SPECULAR
	struct D3DQA_DMSTEST_IN
	{
		FLOAT x, y, z;
		FLOAT nx, ny, nz;
		DWORD Diffuse;
		DWORD Specular;
	};

	#define D3DQA_DMSTEST_OUT_FVF D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_SPECULAR
	struct D3DQA_DMSTEST_OUT
	{
		FLOAT x, y, z, rhw;
		DWORD Diffuse;
		DWORD Specular;
	};

	//
	// Storage for per-component vertex lighting results.  Although
	// the values stored are intended to be integers in the range of
	// [0,255], the storage is declared in the floating point type for
	// computational convenience.
	//
	float fRedResult, fGreenResult, fBlueResult;

	//
	// Storage used to track the difference between the expected
	// per-component vertex lighting results, and the actual per-
	// component vertex lighting results.
	//
	float fRedDelta, fGreenDelta, fBlueDelta;

	//
	// Expected vertex lighting result (R,G,B)
	//
	float fExpected;

	//
	// Color value for directional light; range: [0.0f,1.0f]
	//
	float fLightColorValue;

	//
	// Color for directional light; range: [0,255]
	//
	UINT uiLightColor;
	
	//
	// Color value for material source; range: [0.0f,1.0f]
	//
	float fMaterialSourceColorValue;

	//
	// Color for material source; range: [0.0f,1.0f]
	//
	UINT uiMaterialSourceColor;


	// 
	// Iterator to select a color source among the three options; DWORD
	// for actual value to pass to SetRenderState.
	//
	UINT uiColSource;
	DWORD dwDiffuseMatSource;

	//
	// Light value, which will be used to provide the diffuse multiplier
	// in the lighting equation (multiplied with material source diffuse
	// color)
	//
	// Initialize with a directional light type and a direction as
	// previously described.
	//
	D3DMLIGHT d3dLight ={D3DMLIGHT_DIRECTIONAL,                  // D3DMLIGHTTYPE    Type;    
	                     {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)},  // D3DMCOLORVALUE   Diffuse; 
	                     {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)},  // D3DMCOLORVALUE   Specular;
	                     {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)},  // D3DMCOLORVALUE   Ambient; 
	                     {_M(0.0f),_M(0.0f),_M(0.0f)},           // D3DMVECTOR       Position;
	                     {_M(0.0f),_M(0.0f),_M(1.0f)},           // D3DMVECTOR       Direction
	                     0.0f };                                 // D3DMVALUE        Range;   

	//
	// Material to be used in the case where the material source is set
	// to material rather than a vertex component
	//
	D3DMMATERIAL Material;

	//
	// Input and output Vertex buffers for ProcessVertices
	//
	IDirect3DMobileVertexBuffer *pInVB = NULL;
	IDirect3DMobileVertexBuffer *pOutVB = NULL;

	//
	// FVF sizes for this test
	//
	CONST UINT uiInVertSize = sizeof(D3DQA_DMSTEST_IN);  
	CONST UINT uiOutVertSize = sizeof(D3DQA_DMSTEST_OUT);

	//
	// Used for VB locks, to access FVF data (position, color)
	//
	D3DQA_DMSTEST_IN *pInVert = NULL;
	D3DQA_DMSTEST_OUT *pOutVert = NULL;

	// 
	// Device Capabilities
	// 
	D3DMCAPS Caps;

	//
	// The test will return TPR_FAIL if even one result exceeds the tolerance, but to aid in
	// debugging the test will continue to execute until reaching the number of failures below.
	//
	CONST UINT uiFailureThreshhold = 25;
	UINT uiNumFailures = 0;

	//
	// Tux Test Case Result; all non-pass blocks set this to another value
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Disable clipping
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_CLIPPING, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	// 
	// Query the device's capabilities
	// 
	if( FAILED( hr = m_pd3dDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::GetDeviceCaps failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Verify ability to toggle amongst material sources
	//
	if (!(Caps.VertexProcessingCaps & D3DMVTXPCAPS_MATERIALSOURCE))
	{
		DebugOut(DO_ERROR, L"Test case requires D3DMVTXPCAPS_MATERIALSOURCE. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	// 
	// If the device supports directional lights, assume that it supports lighting
	// with at least one MaxActiveLights.  Otherwise, the test cannot execute.
	// 
	if (!(D3DMVTXPCAPS_DIRECTIONALLIGHTS & Caps.VertexProcessingCaps))
	{
		DebugOut(DO_ERROR, L"Directional lights not supported. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pOutVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DQA_DMSTEST_OUT_FVF, uiOutVertSize, D3DMUSAGE_DONOTCLIP);
	if (NULL == pOutVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pInVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DQA_DMSTEST_IN_FVF, uiInVertSize, 0);
	if (NULL == pInVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Lock the input vertex buffer
	//
	if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
	                        uiInVertSize,       // UINT SizeToLock,
	                        (VOID**)(&pInVert), // VOID** ppbData,
	                        0)))                // DWORD Flags
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Fill the input vertex buffer
	//
	pInVert->x = 0.0f;
	pInVert->y = 0.0f;
	pInVert->z = 0.0f;
	pInVert->nx = 0.0f;
	pInVert->ny = 0.0f;
	pInVert->nz =-1.0f;
	pInVert->Diffuse = D3DMCOLOR_XRGB(0x00,0x00,0x00);
	pInVert->Specular = D3DMCOLOR_XRGB(0x00,0x00,0x00);

	//
	// Unlock the input vertex buffer
	//
	pInVB->Unlock();

	//
	// Enable lighting
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_LIGHTING, TRUE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Enabling per-vertex color allows the system to include the color
	// defined for individual vertices in its lighting calculations. 
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_COLORVERTEX, TRUE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	ZeroMemory(&Material, sizeof(D3DMMATERIAL));

	//
	// Iterate through a selection of colors for the light source (directional light)
	//
	for(uiLightColor = 0; uiLightColor < 256; uiLightColor++)
	{
		// 
		// Convert to a range/format that is suitable for use in the color value structure
		// 
		fLightColorValue = (float)uiLightColor / 255.0f;

		//
		// Iterate through a selection of colors for the diffuse material source
		//
		for(uiMaterialSourceColor = 0; uiMaterialSourceColor < 256; uiMaterialSourceColor++)
		{
			// 
			// Convert to a range/format that is suitable for use in the color value structure
			// 
			fMaterialSourceColorValue = (float)uiMaterialSourceColor / 255.0f;

			// 
			// Iterate through all three valid diffuse material sources
			// 
			for(uiColSource = 0; uiColSource < D3DQA_COLOR_SOURCES; uiColSource++)
			{

				// 
				// Depending on the diffuse material color source selected for this
				// iteration, setup for ProcessVertices accordingly
				// 
				switch(uiColSource)
				{
				case 0: // Use the diffuse color from the vertex

					//
					// Lock the input vertex buffer
					//
					if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
					                        uiInVertSize,       // UINT SizeToLock,
					                        (VOID**)(&pInVert), // VOID** ppbData,
					                        0)))                // DWORD Flags
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Fill the input vertex buffer
					//
					pInVert->Diffuse = D3DMCOLOR_XRGB(uiMaterialSourceColor,uiMaterialSourceColor,uiMaterialSourceColor);
					pInVert->Specular = D3DMCOLOR_XRGB(0x00,0x00,0x00);

					//
					// Unlock the input vertex buffer
					//
					pInVB->Unlock();
					
					d3dLight.Diffuse.r = _M(fLightColorValue);
					d3dLight.Diffuse.g = _M(fLightColorValue);
					d3dLight.Diffuse.b = _M(fLightColorValue);
					d3dLight.Diffuse.a = _M(0.0f);

					if( FAILED( hr = m_pd3dDevice->SetLight(0, &d3dLight, D3DMFMT_D3DMVALUE_FLOAT)))
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetLight failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					if( FAILED( hr = m_pd3dDevice->LightEnable(0, TRUE)))
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::LightEnable failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					dwDiffuseMatSource = D3DMMCS_COLOR1;
					break;

				case 1: // Use the specular color from the vertex
					//
					// Lock the input vertex buffer
					//
					if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
					                        uiInVertSize,       // UINT SizeToLock,
					                        (VOID**)(&pInVert), // VOID** ppbData,
					                        0)))                // DWORD Flags
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Fill the input vertex buffer
					//
					pInVert->Diffuse = D3DMCOLOR_XRGB(0x00,0x00,0x00);
					pInVert->Specular = D3DMCOLOR_XRGB(uiMaterialSourceColor,uiMaterialSourceColor,uiMaterialSourceColor);

					//
					// Unlock the input vertex buffer
					//
					pInVB->Unlock();
					
					d3dLight.Diffuse.r = _M(fLightColorValue);
					d3dLight.Diffuse.g = _M(fLightColorValue);
					d3dLight.Diffuse.b = _M(fLightColorValue);
					d3dLight.Diffuse.a = _M(0.0f);

					if( FAILED( hr = m_pd3dDevice->SetLight(0, &d3dLight, D3DMFMT_D3DMVALUE_FLOAT)))
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetLight failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					if( FAILED( hr = m_pd3dDevice->LightEnable(0, TRUE)))
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::LightEnable failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					dwDiffuseMatSource = D3DMMCS_COLOR2;

					break;


				case 2: // Use the material's color
					
					d3dLight.Diffuse.r = _M(fLightColorValue);
					d3dLight.Diffuse.g = _M(fLightColorValue);
					d3dLight.Diffuse.b = _M(fLightColorValue);
					d3dLight.Diffuse.a = _M(0.0f);

					if( FAILED( hr = m_pd3dDevice->SetLight(0, &d3dLight, D3DMFMT_D3DMVALUE_FLOAT)))
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetLight failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					if( FAILED( hr = m_pd3dDevice->LightEnable(0, TRUE)))
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::LightEnable failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Lock the input vertex buffer
					//
					if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
					                        uiInVertSize,       // UINT SizeToLock,
					                        (VOID**)(&pInVert), // VOID** ppbData,
					                        0)))                // DWORD Flags
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Fill the input vertex buffer
					//
					pInVert->x = 0.0f;
					pInVert->y = 0.0f;
					pInVert->z = 0.0f;
					pInVert->Diffuse = D3DMCOLOR_XRGB(0x00,0x00,0x00);
					pInVert->Specular = D3DMCOLOR_XRGB(0x00,0x00,0x00);

					//
					// Unlock the input vertex buffer
					//
					pInVB->Unlock();
					
					Material.Diffuse.r = _M(fMaterialSourceColorValue);
					Material.Diffuse.g = _M(fMaterialSourceColorValue);
					Material.Diffuse.b = _M(fMaterialSourceColorValue);
					Material.Diffuse.a = _M(0.0f);

					if( FAILED( hr = m_pd3dDevice->SetMaterial(&Material, D3DMFMT_D3DMVALUE_FLOAT)))
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetMaterial failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}
					
					dwDiffuseMatSource = D3DMMCS_MATERIAL;

					break;
				}

				//
				// According to the diffuse material color source selected for this
				// iteration, indicate the correct enumerator, via render state
				//
				if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_DIFFUSEMATERIALSOURCE, dwDiffuseMatSource)))
				{
					DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
					Result = TPR_ABORT;
					goto cleanup;
				}

				//
				// Perform TnL on input VB, store in output VB
				//
				if( FAILED( hr = m_pd3dDevice->ProcessVertices(0,         // UINT SrcStartIndex,
				                                          0,         // UINT DestIndex,
				                                          1,         // UINT VertexCount,
				                                          pOutVB,    // IDirect3DMobileVertexBuffer* pDestBuffer,
				                                          0)))       // DWORD Flags
				{
					DebugOut(DO_ERROR, L"IDirect3DMobileDevice::ProcessVertices failed. (hr = 0x%08x) Aborting.", hr);
					Result = TPR_ABORT;
					goto cleanup;
				}

				//
				// Lock results of ProcessVertices
				//
				if( FAILED( hr = pOutVB->Lock(0,                   // UINT OffsetToLock,
				                         uiOutVertSize,       // UINT SizeToLock,
				                         (VOID**)(&pOutVert), // VOID** ppbData,
				                         0)))                 // DWORD Flags
				{
					DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
					Result = TPR_ABORT;
					goto cleanup;
				}

				//
				// Examine the output vertex buffer
				//
				// Compute the expected result, and derive the actual per-component results
				//
				fExpected = (float)((UCHAR)(fMaterialSourceColorValue * fLightColorValue*255.0f));
				fRedResult = (float)D3DQA_GETRED(pOutVert->Diffuse);
				fGreenResult = (float)D3DQA_GETGREEN(pOutVert->Diffuse);
				fBlueResult = (float)D3DQA_GETBLUE(pOutVert->Diffuse);

				// 
				// Compute the differences between the actual results and the expected results
				// 
				fRedDelta = (float)fabs(fExpected - fRedResult);
				fGreenDelta = (float)fabs(fExpected - fGreenResult);
				fBlueDelta = (float)fabs(fExpected - fBlueResult);

				//
				// Release the lock (the target value has been stored elsewhere)
				//
				pOutVB->Unlock();

				// 
				// Report any unacceptable error
				// 
				if ((fRedDelta > 1) ||
					(fGreenDelta > 1) ||
					(fBlueDelta > 1))
				{
					DebugOut(DO_ERROR, L"Error exceeds tolerance. Failing.");


					// 
					// Log the iteration's results to the debugger
					// 
					DebugOut(L"RED (Actual: %g; Expected: %g; Delta: %g)  "
						  L"BLUE (Actual: %g; Expected: %g; Delta: %g)  "
						  L"GREEN (Actual: %g; Expected: %g; Delta: %g) ",
						  fRedResult,   fExpected, fRedDelta,
						  fGreenResult, fExpected, fGreenDelta,
						  fBlueResult,  fExpected, fBlueDelta);

					Result = TPR_FAIL;

					uiNumFailures++;
					if (uiNumFailures >= uiFailureThreshhold)
					{
						DebugOut(L"Failure threshhold exceeded. Failing.");
						Result = TPR_FAIL; // Should have been set to this previously anyway
						goto cleanup;
					}

				}
			}
		}
	}

cleanup:


	//
	// Unlock if not already unlocked (no op if it is already unlocked)
	//
	if (pInVB)
		pInVB->Unlock();
	if (pOutVB)
		pOutVB->Unlock();

	//
	// Clean up the vertex buffers
	//
	if (pInVB)
		pInVB->Release();
	if (pOutVB)
		pOutVB->Release();

	//
	// Clear active vertex buffer ref-count
	//
	if (m_pd3dDevice)
		m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
									   NULL,  // IDirect3DMobileVertexBuffer* pStreamData
									   0);    // UINT Stride

	//
	// Return tux test case result
	//
	return Result;
}


//
// ExecuteSpecularMaterialSourceTest
//  
//  
//   This test verifies that, when specular lighting is enabled, the specular material
//   source is obtained from the correct source.  This is the component shown as V :
//                                                                                s
//         
//                                                           P
//          Specular     = V   *  SUM(Normal dot HalfwayVect)  * Lcs
//                  RGB     s
//
//   The specular color source can be any of the following:
//
//      * The diffuse color in the vertex
//      * The specular color in the vertex
//      * The material's specular color value
//
//   Per iteration, this test uses a single directional light to illuminate a single
//   vertex via ProcessVertices.  The direction of the light and the normal of the
//   vertex are contrived in a manner that eliminates the angle of incidence as a
//   factor in the lighting computation.  This allows the test to most easily
//   verify that the correct specular color source is being used in the computation,
//   without additional complication.
//   
//   For simplicity, it is expected that clipping is off.
//
// Arguments:
//
//   None
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecuteSpecularMaterialSourceTest()
{
    HRESULT hr;
    
	DebugOut(L"Beginning ExecuteSpecularMaterialSourceTest.");

	//
	// FVF structures and bit masks for use with this test
	//
	#define D3DQA_SMSTEST_IN_FVF D3DMFVF_XYZ_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_NORMAL_FLOAT | D3DMFVF_SPECULAR
	struct D3DQA_SMSTEST_IN
	{
		FLOAT x, y, z;
		FLOAT nx, ny, nz;
		DWORD Diffuse;
		DWORD Specular;
	};

	#define D3DQA_SMSTEST_OUT_FVF D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_SPECULAR
	struct D3DQA_SMSTEST_OUT
	{
		FLOAT x, y, z, rhw;
		DWORD Diffuse;
		DWORD Specular;
	};

	//
	// Storage for per-component vertex lighting results.  Although
	// the values stored are intended to be integers in the range of
	// [0,255], the storage is declared in the floating point type for
	// computational convenience.
	//
	float fRedResult, fGreenResult, fBlueResult;

	//
	// Storage used to track the difference between the expected
	// per-component vertex lighting results, and the actual per-
	// component vertex lighting results.
	//
	float fRedDelta, fGreenDelta, fBlueDelta;

	//
	// Expected vertex lighting result (R,G,B)
	//
	float fExpected;

	//
	// Color value for directional light; range: [0.0f,1.0f]
	//
	float fLightColorValue;

	//
	// Color for directional light; range: [0,255]
	//
	UINT uiLightColor;
	
	//
	// Color value for material source; range: [0.0f,1.0f]
	//
	float fMaterialSourceColorValue;

	//
	// Color for material source; range: [0.0f,1.0f]
	//
	UINT uiMaterialSourceColor;


	// 
	// Iterator to select a color source among the three options; DWORD
	// for actual value to pass to SetRenderState.
	//
	UINT uiColSource;
	DWORD dwSpecularMatSource;

	//
	// Light value, which will be used to provide the specular multiplier
	// in the lighting equation (multiplied with material source specular
	// color)
	//
	// Initialize with a directional light type and a direction as
	// previously described.
	//
	D3DMLIGHT d3dLight = {D3DMLIGHT_DIRECTIONAL,                 // D3DMLIGHTTYPE    Type;
	                      {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, // D3DMCOLORVALUE   Diffuse;
	                      {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, // D3DMCOLORVALUE   Specular;
	                      {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, // D3DMCOLORVALUE   Ambient;
	                      {_M(0.0f),_M(0.0f),_M(0.0f)},          // D3DMVECTOR       Position;
	                      {_M(0.0f),_M(0.0f),_M(1.0f)},          // D3DMVECTOR       Direction;
                              0.0f };                            // float            Range;

	//
	// Material to be used in the case where the material source is set
	// to material rather than a vertex component
	//
	D3DMMATERIAL Material;

	//
	// Input and output Vertex buffers for ProcessVertices
	//
	IDirect3DMobileVertexBuffer *pInVB = NULL;
	IDirect3DMobileVertexBuffer *pOutVB = NULL;

	//
	// FVF sizes for this test
	//
	CONST UINT uiInVertSize = sizeof(D3DQA_SMSTEST_IN);  
	CONST UINT uiOutVertSize = sizeof(D3DQA_SMSTEST_OUT);

	//
	// Used for VB locks, to access FVF data (position, color)
	//
	D3DQA_SMSTEST_IN *pInVert = NULL;
	D3DQA_SMSTEST_OUT *pOutVert = NULL;

    // 
    // Device Capabilities
    // 
    D3DMCAPS Caps;

	//
	// The test will return TPR_FAIL if even one result exceeds the tolerance, but to aid in
	// debugging the test will continue to execute until reaching the number of failures below.
	//
	CONST UINT uiFailureThreshhold = 25;
	UINT uiNumFailures = 0;

	//
	// Tux Test Case Result; all non-pass blocks set this to another value
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Disable clipping
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_CLIPPING, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}
    
    // 
    // Query the device's capabilities
    // 
	if( FAILED( hr = m_pd3dDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::GetDeviceCaps failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Is specular lighting supported, if required?
	//
	if (!(Caps.DevCaps & D3DMDEVCAPS_SPECULAR))
	{
		DebugOut(DO_ERROR, L"Test case requires D3DMDEVCAPS_SPECULAR. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Verify ability to toggle amongst material sources
	//
	if (!(Caps.VertexProcessingCaps & D3DMVTXPCAPS_MATERIALSOURCE))
	{
		DebugOut(DO_ERROR, L"Test case requires D3DMVTXPCAPS_MATERIALSOURCE; Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

    // 
    // If the device supports directional lights, assume that it supports lighting
    // with at least one MaxActiveLights.  Otherwise, the test cannot execute.
    // 
    if (!(D3DMVTXPCAPS_DIRECTIONALLIGHTS & Caps.VertexProcessingCaps))
    {
		DebugOut(DO_ERROR, L"Device does not support directional lights. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
    }
	
	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pOutVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DQA_SMSTEST_OUT_FVF, uiOutVertSize, D3DMUSAGE_DONOTCLIP);
	if (NULL == pOutVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pInVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DQA_SMSTEST_IN_FVF, uiInVertSize, 0);
	if (NULL == pInVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Lock the input vertex buffer
	//
	if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
							uiInVertSize,       // UINT SizeToLock,
							(VOID**)(&pInVert), // VOID** ppbData,
							0)))                // DWORD Flags
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Fill the input vertex buffer
	//
	pInVert->x = 0.0f;
	pInVert->y = 0.0f;
	pInVert->z = 0.0f;
	pInVert->nx = 0.0f;
	pInVert->ny = 0.0f;
	pInVert->nz =-1.0f;
	pInVert->Diffuse = D3DMCOLOR_XRGB(0x00,0x00,0x00);
	pInVert->Specular = D3DMCOLOR_XRGB(0x00,0x00,0x00);

	//
	// Unlock the input vertex buffer
	//
	pInVB->Unlock();
 
    //
    // Enable lighting
    //
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_LIGHTING, TRUE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}
	
    //
    // Enable specular lighting
    //
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_SPECULARENABLE, TRUE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

    //
    // Enabling per-vertex color allows the system to include the color
    // defined for individual vertices in its lighting calculations. 
    //
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_COLORVERTEX, TRUE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	ZeroMemory(&Material, sizeof(D3DMMATERIAL));

    //
    // Iterate through a selection of colors for the light source (directional light)
    //
	for(uiLightColor = 0; uiLightColor < 256; uiLightColor++)
	{
        // 
        // Convert to a range/format that is suitable for use in the color value structure
        // 
		fLightColorValue = (float)uiLightColor / 255.0f;

        //
        // Iterate through a selection of colors for the specular material source
        //
		for(uiMaterialSourceColor = 0; uiMaterialSourceColor < 256; uiMaterialSourceColor++)
		{
            // 
            // Convert to a range/format that is suitable for use in the color value structure
            // 
			fMaterialSourceColorValue = (float)uiMaterialSourceColor / 255.0f;
    
            // 
            // Iterate through all three valid specular material sources
            // 
			for(uiColSource = 0; uiColSource < D3DQA_COLOR_SOURCES; uiColSource++)
			{

                // 
                // Depending on the specular material color source selected for this
                // iteration, setup for ProcessVertices accordingly
                // 
				switch(uiColSource)
				{
				case 0: // Use the specular color from the vertex

					//
					// Lock the input vertex buffer
					//
					if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
											uiInVertSize,       // UINT SizeToLock,
											(VOID**)(&pInVert), // VOID** ppbData,
											0)))                // DWORD Flags
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Fill the input vertex buffer
					//
					pInVert->Diffuse = D3DMCOLOR_XRGB(uiMaterialSourceColor,uiMaterialSourceColor,uiMaterialSourceColor);
					pInVert->Specular = D3DMCOLOR_XRGB(0x00,0x00,0x00);

					//
					// Unlock the input vertex buffer
					//
					pInVB->Unlock();
					
					d3dLight.Specular.r = _M(fLightColorValue);
					d3dLight.Specular.g = _M(fLightColorValue);
					d3dLight.Specular.b = _M(fLightColorValue);
					d3dLight.Specular.a = _M(0.0f);

					if( FAILED( hr = m_pd3dDevice->SetLight(0, &d3dLight, D3DMFMT_D3DMVALUE_FLOAT)))
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetLight failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					if( FAILED( hr = m_pd3dDevice->LightEnable(0, TRUE)))
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::LightEnable failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					dwSpecularMatSource = D3DMMCS_COLOR1;
					break;

				case 1: // Use the specular color from the vertex
					//
					// Lock the input vertex buffer
					//
					if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
											uiInVertSize,       // UINT SizeToLock,
											(VOID**)(&pInVert), // VOID** ppbData,
											0)))                // DWORD Flags
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Fill the input vertex buffer
					//
					pInVert->Diffuse = D3DMCOLOR_XRGB(0x00,0x00,0x00);
					pInVert->Specular = D3DMCOLOR_XRGB(uiMaterialSourceColor,uiMaterialSourceColor,uiMaterialSourceColor);

					//
					// Unlock the input vertex buffer
					//
					pInVB->Unlock();
					
					d3dLight.Specular.r = _M(fLightColorValue);
					d3dLight.Specular.g = _M(fLightColorValue);
					d3dLight.Specular.b = _M(fLightColorValue);
					d3dLight.Specular.a = _M(0.0f);

					if( FAILED( hr = m_pd3dDevice->SetLight(0, &d3dLight, D3DMFMT_D3DMVALUE_FLOAT)))
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetLight failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					if( FAILED( hr = m_pd3dDevice->LightEnable(0, TRUE)))
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::LightEnable failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					dwSpecularMatSource = D3DMMCS_COLOR2;

					break;


				case 2: // Use the material's color
					
					d3dLight.Specular.r = _M(fLightColorValue);
					d3dLight.Specular.g = _M(fLightColorValue);
					d3dLight.Specular.b = _M(fLightColorValue);
					d3dLight.Specular.a = _M(0.0f);

					if( FAILED( hr = m_pd3dDevice->SetLight(0, &d3dLight, D3DMFMT_D3DMVALUE_FLOAT)))
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetLight failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					if( FAILED( hr = m_pd3dDevice->LightEnable(0, TRUE)))
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::LightEnable failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Lock the input vertex buffer
					//
					if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
											uiInVertSize,       // UINT SizeToLock,
											(VOID**)(&pInVert), // VOID** ppbData,
											0)))                // DWORD Flags
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Fill the input vertex buffer
					//
					pInVert->x = 0.0f;
					pInVert->y = 0.0f;
					pInVert->z = 0.0f;
					pInVert->Diffuse = D3DMCOLOR_XRGB(0x00,0x00,0x00);
					pInVert->Specular = D3DMCOLOR_XRGB(0x00,0x00,0x00);

					//
					// Unlock the input vertex buffer
					//
					pInVB->Unlock();
					
					Material.Specular.r = _M(fMaterialSourceColorValue);
					Material.Specular.g = _M(fMaterialSourceColorValue);
					Material.Specular.b = _M(fMaterialSourceColorValue);
					Material.Specular.a = _M(0.0f);

					if( FAILED( hr = m_pd3dDevice->SetMaterial(&Material, D3DMFMT_D3DMVALUE_FLOAT)))
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetMaterial failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}
					
					dwSpecularMatSource = D3DMMCS_MATERIAL;

					break;
				}

				//
				// According to the specular material color source selected for this
                // iteration, indicate the correct enumerator, via render state
				//
				if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_SPECULARMATERIALSOURCE, dwSpecularMatSource)))
				{
					DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
					Result = TPR_ABORT;
					goto cleanup;
				}

				//
				// Perform TnL on input VB, store in output VB
				//
				if( FAILED( hr = m_pd3dDevice->ProcessVertices(0,         // UINT SrcStartIndex,
														  0,         // UINT DestIndex,
														  1,         // UINT VertexCount,
														  pOutVB,    // IDirect3DMobileVertexBuffer* pDestBuffer,
														  0)))       // DWORD Flags
				{
					DebugOut(DO_ERROR, L"IDirect3DMobileDevice::ProcessVertices failed. (hr = 0x%08x) Aborting.", hr);
					Result = TPR_ABORT;
					goto cleanup;
				}

				//
				// Lock results of ProcessVertices
				//
				if( FAILED( hr = pOutVB->Lock(0,                  // UINT OffsetToLock,
										 uiOutVertSize,      // UINT SizeToLock,
										 (VOID**)(&pOutVert), // VOID** ppbData,
										 0)))                // DWORD Flags
				{
					DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
					Result = TPR_ABORT;
					goto cleanup;
				}

				//
				// Examine the output vertex buffer
				//
				// Compute the expected result, and derive the actual per-component results
				//
				fExpected = (float)((UCHAR)(fMaterialSourceColorValue * fLightColorValue*255.0f));
				fRedResult = (float)D3DQA_GETRED(pOutVert->Specular);
				fGreenResult = (float)D3DQA_GETGREEN(pOutVert->Specular);
				fBlueResult = (float)D3DQA_GETBLUE(pOutVert->Specular);

                // 
                // Compute the differences between the actual results and the expected results
                // 
				fRedDelta = (float)fabs(fExpected - fRedResult);
				fGreenDelta = (float)fabs(fExpected - fGreenResult);
				fBlueDelta = (float)fabs(fExpected - fBlueResult);

				//
				// Release the lock (the target value has been stored elsewhere)
				//
				pOutVB->Unlock();
				
                // 
                // Report any unacceptable error
                // 
				if ((fRedDelta > 1) ||
					(fGreenDelta > 1) ||
					(fBlueDelta > 1))
				{
					DebugOut(DO_ERROR, L"Error exceeds tolerance. Failing.");

					// 
					// Log the iteration's results to the debugger
					// 
					DebugOut(L"RED (Actual: %g; Expected: %g; Delta: %g)  "
						  L"BLUE (Actual: %g; Expected: %g; Delta: %g)  "
						  L"GREEN (Actual: %g; Expected: %g; Delta: %g) ",
						  fRedResult,   fExpected, fRedDelta,
						  fGreenResult, fExpected, fGreenDelta,
						  fBlueResult,  fExpected, fBlueDelta);

					Result = TPR_FAIL;

					uiNumFailures++;
					if (uiNumFailures >= uiFailureThreshhold)
					{
						DebugOut(L"Failure threshhold exceeded. Failing.");
						Result = TPR_FAIL; // Should have been set to this previously anyway
						goto cleanup;
					}

				}
			}
		}
	}

cleanup:

	//
	// Unlock if not already unlocked (no op if it is already unlocked)
	//
	if (pInVB)
		pInVB->Unlock();
	if (pOutVB)
		pOutVB->Unlock();

	//
	// Clean up the vertex buffers
	//
	if (pInVB)
		pInVB->Release();
	if (pOutVB)
		pOutVB->Release();

	//
	// Clear active vertex buffer ref-count
	//
	if (m_pd3dDevice)
		m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
									   NULL,  // IDirect3DMobileVertexBuffer* pStreamData
									   0);    // UINT Stride

	//
	// Return tux test case result
	//
	return Result;
}


//
// ExecuteSpecularPowerTest
//  
//   This test verifies that, when specular lighting is enabled, the specular power
//   is computed correctly in the specular lighting equation.  The specular formula
//   is:
//         
//                                                           P
//          Specular     = V   *  SUM(Normal dot HalfwayVect)  * Lcs
//                  RGB     s
//
//   To verify that the specular power is computed correctly, Vs and Lcs components
//   are set to 1.0f to reduce the specular formula to:
//   
//                                                    P
//          Specular     = SUM(Normal dot HalfwayVect)
//                  RGB    
// 
//   To simplify the formula further, only one light is used, and the position of
//   the light and the vertex normal vector are chosen in a manner that reduces
//   the formula to:
//                               P
//          Specular     = Normal
//                  RGB    
// 
//   This allows the test to effectively isolate the affect of the specular power on
//   the specular formula.
//   
//   For simplicity, it is expected that clipping is off.
//
// Arguments:
//
//   None
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecuteSpecularPowerTest()
{
    HRESULT hr;
    
	DebugOut(L"Beginning ExecuteSpecularPowerTest.");
    //
    // FVF structures and bit masks for use with this test
    //
    #define D3DQA_SPTTEST_IN_FVF D3DMFVF_XYZ_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_NORMAL_FLOAT | D3DMFVF_SPECULAR
    struct D3DQA_SPTTEST_IN
    {
        FLOAT x, y, z;
        FLOAT nx, ny, nz;
	    DWORD Diffuse;
	    DWORD Specular;
    };

    #define D3DQA_SPTTEST_OUT_FVF D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_SPECULAR
    struct D3DQA_SPTTEST_OUT
    {
        FLOAT x, y, z, rhw;
	    DWORD Diffuse;
	    DWORD Specular;
    };

	//
	// Storage for per-component vertex lighting results.  Although
	// the values stored are intended to be integers in the range of
    // [0,255], the storage is declared in the floating point type for
    // computational convenience.
	//
	float fRedResult, fGreenResult, fBlueResult;

	//
	// Storage used to track the difference between the expected
	// per-component vertex lighting results, and the actual per-
	// component vertex lighting results.
	//
	float fRedDelta, fGreenDelta, fBlueDelta;

	//
	// Expected vertex lighting result (R,G,B)
	//
	float fExpected;

	//
	// Used in the specular formula to produce:  fX^fY
	//
	float fX,fY;
	
	//
	// Light value, which will be used to provide the specular multiplier
	// in the lighting equation (multiplied with material source specular
	// color)
	//
	// Initialize with a directional light type and a direction as
	// previously described.
	//
	D3DMLIGHT d3dLight = {D3DMLIGHT_DIRECTIONAL,                  // D3DMLIGHTTYPE    Type;
	                      {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)},  // D3DMCOLORVALUE   Diffuse;
	                      {_M(1.0f),_M(1.0f),_M(1.0f),_M(0.0f)},  // D3DMCOLORVALUE   Specular;
	                      {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)},  // D3DMCOLORVALUE   Ambient;
	                      {_M(0.0f),_M(0.0f),_M(0.0f)},           // D3DMVECTOR       Position;
	                      {_M(0.0f),_M(0.0f),_M(-1.0f)},          // D3DMVECTOR       Direction;
                              0.0f };                             // float           Range;

	//
	// Material to be used in the case where the material source is set
	// to material rather than a vertex component
	//
	D3DMMATERIAL Material;

	//
	// Input and output Vertex buffers for ProcessVertices
	//
	IDirect3DMobileVertexBuffer *pInVB = NULL;
	IDirect3DMobileVertexBuffer *pOutVB = NULL;

	//
	// FVF sizes for this test
	//
	CONST UINT uiInVertSize = sizeof(D3DQA_SPTTEST_IN);  
	CONST UINT uiOutVertSize = sizeof(D3DQA_SPTTEST_OUT);

	//
	// Used for VB locks, to access FVF data (position, color)
	//
	D3DQA_SPTTEST_IN *pInVert = NULL;
	D3DQA_SPTTEST_OUT *pOutVert = NULL;

    // 
    // Device Capabilities
    // 
    D3DMCAPS Caps;

	//
	// The test will return TPR_FAIL if even one result exceeds the tolerance, but to aid in
	// debugging the test will continue to execute until reaching the number of failures below.
	//
	CONST UINT uiFailureThreshhold = 25;
	UINT uiNumFailures = 0;

	//
	// Tux Test Case Result; all non-pass blocks set this to another value
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Disable clipping
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_CLIPPING, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}
    
    // 
    // Query the device's capabilities
    // 
	if( FAILED( hr = m_pd3dDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::GetDeviceCaps failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Is specular lighting supported, if required?
	//
	if (!(Caps.DevCaps & D3DMDEVCAPS_SPECULAR))
	{
		DebugOut(DO_ERROR, L"Test case requires D3DMDEVCAPS_SPECULAR. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Verify ability to toggle amongst material sources
	//
	if (!(Caps.VertexProcessingCaps & D3DMVTXPCAPS_MATERIALSOURCE))
	{
		DebugOut(DO_ERROR, L"Test case requires D3DMVTXPCAPS_MATERIALSOURCE. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

    // 
    // If the device supports directional lights, assume that it supports lighting
    // with at least one MaxActiveLights.  Otherwise, the test cannot execute.
    // 
    if (!(D3DMVTXPCAPS_DIRECTIONALLIGHTS & Caps.VertexProcessingCaps))
    {
		DebugOut(DO_ERROR, L"Directional lights not supported. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
    }

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pOutVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DQA_SPTTEST_OUT_FVF, uiOutVertSize, D3DMUSAGE_DONOTCLIP);
	if (NULL == pOutVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pInVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DQA_SPTTEST_IN_FVF, uiInVertSize, 0);
	if (NULL == pInVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}
 
    //
    // Enable lighting
    //
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_LIGHTING, TRUE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}
	
    //
    // Enable specular lighting
    //
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_SPECULARENABLE, TRUE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

    //
    // Enabling per-vertex color allows the system to include the color
    // defined for individual vertices in its lighting calculations. 
    //
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_COLORVERTEX, TRUE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// The specular source is the material (for convenience, since this is also where the
	// specular power is set)
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_SPECULARMATERIALSOURCE, D3DMMCS_MATERIAL)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Initialize the material:  everything should be zero-valued, except for specular
	// RGB, which will be set to full intensity.
	//
	ZeroMemory(&Material, sizeof(D3DMMATERIAL));
	Material.Specular.r = _M(1.0f);
	Material.Specular.g = _M(1.0f);
	Material.Specular.b = _M(1.0f);
	Material.Specular.a = _M(0.0f);

	//
	// Test fX^fY, via the specular lighting formula, iterating through a variety of fX and
	// fY possibilities, in such a manner that all possible specular output intensities (0/255
	// through 255/255) are produced and verified.
	//
	for (fX = 0.1f; fX < 1.0f; fX+=0.1f)
	{
		//
		// Specular output, per-component, is in the range of [0,255].
		//
		for (fExpected = 0.0f; fExpected <= 255.0f; fExpected+=1.0f)
		{

			fY = (float)(log(fExpected / 255.0f) / log(fX));

			//
			// (Debugging note) If editing, check:
			// 
			//		(float)(pow(fX,fY) * 255.0f)
			// 
			// to verify that the changes do not break intended test.
			// 

			//
			// Lock the input vertex buffer
			//
			if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
									uiInVertSize,       // UINT SizeToLock,
									(VOID**)(&pInVert), // VOID** ppbData,
									0)))                // DWORD Flags
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}

			//
			// Fill the input vertex buffer
			//
			pInVert->x = 0.0f;
			pInVert->y = 0.0f;
			pInVert->z = 0.0f;
			pInVert->nx = 0.0f;
			pInVert->ny = 0.0f;
			pInVert->nz = fX;
			pInVert->Diffuse = D3DMCOLOR_XRGB(0x00,0x00,0x00);
			pInVert->Specular = D3DMCOLOR_XRGB(0x00,0x00,0x00);

			//
			// Unlock the input vertex buffer
			//
			pInVB->Unlock();

			if( FAILED( hr = m_pd3dDevice->SetLight(0, &d3dLight, D3DMFMT_D3DMVALUE_FLOAT)))
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetLight failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}

			if( FAILED( hr = m_pd3dDevice->LightEnable(0, TRUE)))
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileDevice::LightEnable failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}

			//
			// Set the material power, for this iteration
			//
			Material.Power = fY;

			if( FAILED( hr = m_pd3dDevice->SetMaterial(&Material, D3DMFMT_D3DMVALUE_FLOAT)))
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetMaterial failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}

			//
			// Perform TnL on input VB, store in output VB
			//
			if( FAILED( hr = m_pd3dDevice->ProcessVertices(0,         // UINT SrcStartIndex,
													  0,         // UINT DestIndex,
													  1,         // UINT VertexCount,
													  pOutVB,    // IDirect3DMobileVertexBuffer* pDestBuffer,
													  0)))       // DWORD Flags
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileDevice::ProcessVertices failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}

			//
			// Lock results of ProcessVertices
			//
			if( FAILED( hr = pOutVB->Lock(0,                  // UINT OffsetToLock,
									 uiOutVertSize,      // UINT SizeToLock,
									 (VOID**)(&pOutVert), // VOID** ppbData,
									 0)))                // DWORD Flags
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}
			
			//
			// Examine the output vertex buffer
			//
			fRedResult = (float)D3DQA_GETRED(pOutVert->Specular);
			fGreenResult = (float)D3DQA_GETGREEN(pOutVert->Specular);
			fBlueResult = (float)D3DQA_GETBLUE(pOutVert->Specular);

			fRedDelta = (float)fabs(fExpected - fRedResult);
			fGreenDelta = (float)fabs(fExpected - fGreenResult);
			fBlueDelta = (float)fabs(fExpected - fBlueResult);

			//
			// Release the lock (the target value has been stored elsewhere)
			//
			pOutVB->Unlock();

			// 
			// Report any unacceptable error
			// 
			if ((fRedDelta > 1) ||
				(fGreenDelta > 1) ||
				(fBlueDelta > 1))
			{
				DebugOut(DO_ERROR, L"Error exceeds tolerance. Failing.");
				Result = TPR_FAIL;

				uiNumFailures++;
				if (uiNumFailures >= uiFailureThreshhold)
				{
					DebugOut(L"Failure threshhold exceeded. Failing.");
					Result = TPR_FAIL; // Should have been set to this previously anyway
					goto cleanup;
				}

			}
		}
	}

cleanup:

	//
	// Unlock if not already unlocked (no op if it is already unlocked)
	//
	if (pInVB)
		pInVB->Unlock();
	if (pOutVB)
		pOutVB->Unlock();

	//
	// Clean up the vertex buffers
	//
	if (pInVB)
		pInVB->Release();
	if (pOutVB)
		pOutVB->Release();

	//
	// Clear active vertex buffer ref-count
	//
	if (m_pd3dDevice)
		m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
									   NULL,  // IDirect3DMobileVertexBuffer* pStreamData
									   0);    // UINT Stride

	//
	// Return tux test case result
	//
	return Result;
}




//
// ExecuteHalfwayVectorTest
//   
//   Four components are used to compute the halfway vector during TnL:
//
//      * Vertex position
//      * Vertex normal
//      * Camera position
//      * Light position/direction
//   
//   To verify that ProcessVertices is using the halfway vector to
//   compute specular lighting, the specular formula is reduced to:
//                                                 
//          Specular     = (Normal dot HalfwayVect)
//                  RGB    
//   
//   This is accomplished by:
//      
//      * Setting the source specular component values to 1.0f
//      * Using only one light (a directional light)
//      * Setting the directional light specular component values to 1.0f
//      * Setting the specular power to 1.0f via material
//   
//   The technique that this test uses to tweak the halfway vector is to
//   change the vertex normal and directional light direction.
//   
//   For simplicity, it is expected that clipping is off.
//
// Arguments:
//
//   None
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecuteHalfwayVectorTest()
{
    HRESULT hr;
    
	DebugOut(L"Beginning ExecuteHalfwayVectorTest.");
    //
    // FVF structures and bit masks for use with this test
    //
    #define D3DQA_HWVTEST_IN_FVF D3DMFVF_XYZ_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_NORMAL_FLOAT | D3DMFVF_SPECULAR
    struct D3DQA_HWVTEST_IN
    {
        FLOAT x, y, z;
        FLOAT nx, ny, nz;
	    DWORD Diffuse;
	    DWORD Specular;
    };

    #define D3DQA_HWVTEST_OUT_FVF D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_SPECULAR
    struct D3DQA_HWVTEST_OUT
    {
        FLOAT x, y, z, rhw;
	    DWORD Diffuse;
	    DWORD Specular;
    };

	//
	// Storage for per-component vertex lighting results.  Although
	// the values stored are intended to be integers in the range of
    // [0,255], the storage is declared in the floating point type for
    // computational convenience.
	//
	float fRedResult, fGreenResult, fBlueResult;

	//
	// Storage used to track the difference between the expected
	// per-component vertex lighting results, and the actual per-
	// component vertex lighting results.
	//
	float fRedDelta, fGreenDelta, fBlueDelta;

	//
	// Expected vertex lighting result (R,G,B)
	//
	float fExpected;

	//
	// Light value, which will be used to provide the specular multiplier
	// in the lighting equation (multiplied with material source specular
	// color)
	//
	// Initialize with a directional light type and a direction as
	// previously described.
	//
	D3DMLIGHT d3dLight = {D3DMLIGHT_DIRECTIONAL,                  // D3DMLIGHTTYPE    Type;
	                      {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)},  // D3DMCOLORVALUE   Diffuse;
	                      {_M(1.0f),_M(1.0f),_M(1.0f),_M(0.0f)},  // D3DMCOLORVALUE   Specular;
	                      {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)},  // D3DMCOLORVALUE   Ambient;
	                      {_M(0.0f),_M(0.0f),_M(0.0f)},           // D3DMVECTOR       Position;
	                      {_M(0.0f),_M(0.0f),_M(0.0f)},           // D3DMVECTOR       Direction;
                              0.0f };                             // float           Range;


	//
	// Material to be used in the case where the material source is set
	// to material rather than a vertex component
	//
	D3DMMATERIAL Material;

	//
	// Angles for directional lighting and vertex normal.  These values are iterated
	// over a range, to exercise a variety of settings.
	//
	float fLightAngle, fNormalAngle;

	//
	// Input and output Vertex buffers for ProcessVertices
	//
	IDirect3DMobileVertexBuffer *pInVB = NULL;
	IDirect3DMobileVertexBuffer *pOutVB = NULL;

	//
	// FVF sizes for this test
	//
	CONST UINT uiInVertSize = sizeof(D3DQA_HWVTEST_IN);  
	CONST UINT uiOutVertSize = sizeof(D3DQA_HWVTEST_OUT);

	//
	// Used for VB locks, to access FVF data (position, color)
	//
	D3DQA_HWVTEST_IN *pInVert = NULL;
	D3DQA_HWVTEST_OUT *pOutVert = NULL;

    // 
    // Device Capabilities
    // 
    D3DMCAPS Caps;

	//
	// The test will return TPR_FAIL if even one result exceeds the tolerance, but to aid in
	// debugging the test will continue to execute until reaching the number of failures below.
	//
	CONST UINT uiFailureThreshhold = 25;
	UINT uiNumFailures = 0;

	//
	// Tux Test Case Result; all non-pass blocks set this to another value
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}
    
    // 
    // Query the device's capabilities
    // 
	if( FAILED( hr = m_pd3dDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::GetDeviceCaps failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Disable clipping
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_CLIPPING, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Is specular lighting supported, if required?
	//
	if (!(Caps.DevCaps & D3DMDEVCAPS_SPECULAR))
	{
		DebugOut(DO_ERROR, L"Test case requires D3DMDEVCAPS_SPECULAR. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

    // 
    // If the device supports directional lights, assume that it supports lighting
    // with at least one MaxActiveLights.  Otherwise, the test cannot execute.
    // 
    if (!(D3DMVTXPCAPS_DIRECTIONALLIGHTS & Caps.VertexProcessingCaps))
    {
		DebugOut(DO_ERROR, L"Directional lights not supported. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
    }

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pOutVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DQA_HWVTEST_OUT_FVF, uiOutVertSize, D3DMUSAGE_DONOTCLIP);
	if (NULL == pOutVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pInVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DQA_HWVTEST_IN_FVF, uiInVertSize, 0);
	if (NULL == pInVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}
 
    //
    // Enable lighting
    //
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_LIGHTING, TRUE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}
	
    //
    // Enable specular lighting
    //
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_SPECULARENABLE, TRUE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

    //
    // Enabling per-vertex color allows the system to include the color
    // defined for individual vertices in its lighting calculations. 
    //
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_COLORVERTEX, TRUE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// The specular source is the material (for convenience, since this is also where the
	// specular power is set)
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_SPECULARMATERIALSOURCE, D3DMMCS_MATERIAL)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Turn on localviewer (no affect if it is already on)
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_LOCALVIEWER, TRUE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Initialize the material:  everything should be zero-valued, except for specular
	// RGB, which will be set to full intensity, and power, which should be set to one.
	//
	ZeroMemory(&Material, sizeof(D3DMMATERIAL));
	Material.Specular.r = _M(1.0f);
	Material.Specular.g = _M(1.0f);
	Material.Specular.b = _M(1.0f);
	Material.Specular.a = _M(0.0f);
	Material.Power = 1.0f;

	if( FAILED( hr = m_pd3dDevice->SetMaterial(&Material, D3DMFMT_D3DMVALUE_FLOAT)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetMaterial failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}


	//
	// Iterate through lighting angles.  Rotate lighting vector one full rotation.
	//
	for (fLightAngle = 0.0f; fLightAngle < (2*D3DQA_PI); fLightAngle+=((2*D3DQA_PI)/100))
	{

		//
		// Iterate through normal angles.  Rotate normal vector one full rotation.
		//
		for (fNormalAngle = 0.0f; fNormalAngle < (2*D3DQA_PI); fNormalAngle+=((2*D3DQA_PI)/100))
		{

			//
			// Normalized vector components generated based on the angles for this iteration
			//
			float fNormX, fNormY, fNormZ;
			float fLightX, fLightY, fLightZ;

			fNormX = (float)sin(fNormalAngle);
			fNormY = (float)cos(fNormalAngle);
			fNormZ = 0.0f;
			fLightX = (float)sin(fLightAngle);
			fLightY = (float)cos(fLightAngle);
			fLightZ = 0.0f;


			//
			// Lock the input vertex buffer
			//
			if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
									uiInVertSize,       // UINT SizeToLock,
									(VOID**)(&pInVert), // VOID** ppbData,
									0)))                // DWORD Flags
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}

			//
			// Fill the input vertex buffer
			//
			pInVert->x = 0.0f;
			pInVert->y = 0.0f;
			pInVert->z = 0.0f;
			pInVert->nx = fNormX;
			pInVert->ny = fNormY;
			pInVert->nz = fNormZ;
			pInVert->Diffuse = D3DMCOLOR_XRGB(0x00,0x00,0x00);
			pInVert->Specular = D3DMCOLOR_XRGB(0x00,0x00,0x00);

			//
			// Unlock the input vertex buffer
			//
			pInVB->Unlock();

			d3dLight.Direction.x = _M(fLightX);
			d3dLight.Direction.y = _M(fLightY);
			d3dLight.Direction.z = _M(fLightZ);

			if( FAILED( hr = m_pd3dDevice->SetLight(0, &d3dLight, D3DMFMT_D3DMVALUE_FLOAT)))
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetLight failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}

			if( FAILED( hr = m_pd3dDevice->LightEnable(0, TRUE)))
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileDevice::LightEnable failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}


			//
			// Perform TnL on input VB, store in output VB
			//
			if( FAILED( hr = m_pd3dDevice->ProcessVertices(0,         // UINT SrcStartIndex,
													  0,         // UINT DestIndex,
													  1,         // UINT VertexCount,
													  pOutVB,    // IDirect3DMobileVertexBuffer* pDestBuffer,
													  0)))       // DWORD Flags
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileDevice::ProcessVertices failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}

			//
			// Lock results of ProcessVertices
			//
			if( FAILED( hr = pOutVB->Lock(0,                  // UINT OffsetToLock,
									 uiOutVertSize,      // UINT SizeToLock,
									 (VOID**)(&pOutVert), // VOID** ppbData,
									 0)))                // DWORD Flags
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}

			//
			// Compute expected intensity
			//
			fExpected = 255.0f*((-1)*fNormX*fLightX + (-1)*fNormY*fLightY + (-1)*fNormZ*fLightZ);

			//
			// Negative dot product:  light is facing away from vertex normal
			//
			if (0.0f>fExpected) fExpected = 0.0f;

			//
			// Examine the output vertex buffer
			//
			fRedResult = (float)D3DQA_GETRED(pOutVert->Specular);
			fGreenResult = (float)D3DQA_GETGREEN(pOutVert->Specular);
			fBlueResult = (float)D3DQA_GETBLUE(pOutVert->Specular);

			fRedDelta = (float)fabs(fExpected - fRedResult);
			fGreenDelta = (float)fabs(fExpected - fGreenResult);
			fBlueDelta = (float)fabs(fExpected - fBlueResult);

			//
			// Release the lock (the target value has been stored elsewhere)
			//
			pOutVB->Unlock();

			// 
			// Report any unacceptable error
			// 
			if ((fRedDelta > 1) ||
				(fGreenDelta > 1) ||
				(fBlueDelta > 1))
			{
				DebugOut(DO_ERROR, L"Error exceeds tolerance. Failing.");
				Result = TPR_FAIL;

				uiNumFailures++;
				if (uiNumFailures >= uiFailureThreshhold)
				{
					DebugOut(L"Failure threshhold exceeded. Failing.");
					Result = TPR_FAIL; // Should have been set to this previously anyway
					goto cleanup;
				}

			}
		}
	}

cleanup:

	//
	// Unlock if not already unlocked (no op if it is already unlocked)
	//
	if (pInVB)
		pInVB->Unlock();
	if (pOutVB)
		pOutVB->Unlock();

	//
	// Clean up the vertex buffers
	//
	if (pInVB)
		pInVB->Release();
	if (pOutVB)
		pOutVB->Release();

	//
	// Clear active vertex buffer ref-count
	//
	if (m_pd3dDevice)
		m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
									   NULL,  // IDirect3DMobileVertexBuffer* pStreamData
									   0);    // UINT Stride

	//
	// Return tux test case result
	//
	return Result;
}




//
// ExecuteNonLocalViewerTest
//
//   This test verifies that the non-local-viewer setting causes the expected
//   halfway vector computation (as if the viewer were at infinite Z).
//   
//   For simplicity, it is expected that clipping is off.
//
// Arguments:
//
//   None
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecuteNonLocalViewerTest()
{
    HRESULT hr;
    
	DebugOut(L"Beginning ExecuteNonLocalViewerTest.");
    //
    // FVF structures and bit masks for use with this test
    //
    #define D3DQA_NLVTEST_IN_FVF D3DMFVF_XYZ_FLOAT | D3DMFVF_NORMAL_FLOAT
    struct D3DQA_NLVTEST_IN
    {
        FLOAT x, y, z;
        FLOAT nx, ny, nz;
    };

    #define D3DQA_NLVTEST_OUT_FVF D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_SPECULAR
    struct D3DQA_NLVTEST_OUT
    {
        FLOAT x, y, z, rhw;
	    DWORD Diffuse;
	    DWORD Specular;
    };

	//
	// Storage for per-component vertex lighting results.  Although
	// the values stored are intended to be integers in the range of
    // [0,255], the storage is declared in the floating point type for
    // computational convenience.
	//
	float fRedResult, fGreenResult, fBlueResult;

	//
	// Storage used to track the difference between the expected
	// per-component vertex lighting results, and the actual per-
	// component vertex lighting results.
	//
	float fRedDelta, fGreenDelta, fBlueDelta;

	//
	// Expected vertex lighting result (R,G,B)
	//
	float fExpected;

	//
	// Light value, which will be used to provide the specular multiplier
	// in the lighting equation (multiplied with material source specular
	// color)
	//
	// Initialize with a directional light type and a direction as
	// previously described.
	//
	D3DMLIGHT d3dLight = {D3DMLIGHT_DIRECTIONAL,                  // D3DMLIGHTTYPE    Type;
	                      {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)},  // D3DMCOLORVALUE   Diffuse;
	                      {_M(1.0f),_M(1.0f),_M(1.0f),_M(0.0f)},  // D3DMCOLORVALUE   Specular;
	                      {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)},  // D3DMCOLORVALUE   Ambient;
	                      {_M(0.0f),_M(0.0f),_M(0.0f)},           // D3DMVECTOR       Position;
	                      {_M(0.0f),_M(0.0f),_M(0.0f)},           // D3DMVECTOR       Direction;
                              0.0f };                             // float           Range;

	//
	// Material to be used in the case where the material source is set
	// to material rather than a vertex component
	//
	D3DMMATERIAL Material;

	//
	// Used for vector normalization
	//
	float fMagnitude;

	//
	// Angles for directional lighting and vertex normal.  These values are iterated
	// over a range, to exercise a variety of settings.
	//
	float fLightAngle;

	//
	// Input and output Vertex buffers for ProcessVertices
	//
	IDirect3DMobileVertexBuffer *pInVB = NULL;
	IDirect3DMobileVertexBuffer *pOutVB = NULL;

	//
	// FVF sizes for this test
	//
	CONST UINT uiInVertSize = sizeof(D3DQA_NLVTEST_IN);  
	CONST UINT uiOutVertSize = sizeof(D3DQA_NLVTEST_OUT);

	//
	// Used for VB locks, to access FVF data (position, color)
	//
	D3DQA_NLVTEST_IN *pInVert = NULL;
	D3DQA_NLVTEST_OUT *pOutVert = NULL;

    // 
    // Device Capabilities
    // 
    D3DMCAPS Caps;

	//
	// The test will return TPR_FAIL if even one result exceeds the tolerance, but to aid in
	// debugging the test will continue to execute until reaching the number of failures below.
	//
	CONST UINT uiFailureThreshhold = 25;
	UINT uiNumFailures = 0;

	//
	// Tux Test Case Result; all non-pass blocks set this to another value
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}
    
    // 
    // Query the device's capabilities
    // 
	if( FAILED( hr = m_pd3dDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::GetDeviceCaps failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Is specular lighting supported, if required?
	//
	if (!(Caps.DevCaps & D3DMDEVCAPS_SPECULAR))
	{
		DebugOut(DO_ERROR, L"Test case requires D3DMDEVCAPS_SPECULAR. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Verify ability to toggle amongst material sources
	//
	if (!(Caps.VertexProcessingCaps & D3DMVTXPCAPS_MATERIALSOURCE))
	{
		DebugOut(DO_ERROR, L"Test case requires D3DMVTXPCAPS_MATERIALSOURCE. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

    // 
    // If the device supports directional lights, assume that it supports lighting
    // with at least one MaxActiveLights.  Otherwise, the test cannot execute.
    // 
    if (!(D3DMVTXPCAPS_DIRECTIONALLIGHTS & Caps.VertexProcessingCaps))
    {
		DebugOut(DO_ERROR, L"Directional lights not supported. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
    }

	//
	// Disable clipping
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_CLIPPING, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pOutVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DQA_NLVTEST_OUT_FVF, uiOutVertSize, D3DMUSAGE_DONOTCLIP);
	if (NULL == pOutVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pInVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DQA_NLVTEST_IN_FVF, uiInVertSize, 0);
	if (NULL == pInVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}
 
    //
    // Enable lighting
    //
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_LIGHTING, TRUE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}
	
    //
    // Enable specular lighting
    //
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_SPECULARENABLE, TRUE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

    //
    // Enabling per-vertex color allows the system to include the color
    // defined for individual vertices in its lighting calculations. 
    //
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_COLORVERTEX, TRUE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// The specular source is the material (for convenience, since this is also where the
	// specular power is set)
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_SPECULARMATERIALSOURCE, D3DMMCS_MATERIAL)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Turn off localviewer (no affect if it is already off)
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_LOCALVIEWER, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Initialize the material:  everything should be zero-valued, except for specular
	// RGB, which will be set to full intensity, and power, which should be set to one.
	//
	ZeroMemory(&Material, sizeof(D3DMMATERIAL));
	Material.Specular.r = _M(1.0f);
	Material.Specular.g = _M(1.0f);
	Material.Specular.b = _M(1.0f);
	Material.Specular.a = _M(0.0f);
	Material.Power = 1.0f;

	if( FAILED( hr = m_pd3dDevice->SetMaterial(&Material, D3DMFMT_D3DMVALUE_FLOAT)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetMaterial failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}



	//
	// Lock the input vertex buffer
	//
	if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
							uiInVertSize,       // UINT SizeToLock,
							(VOID**)(&pInVert), // VOID** ppbData,
							0)))                // DWORD Flags
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Fill the input vertex buffer
	//
	pInVert->x = 0.0f;
	pInVert->y = 0.0f;
	pInVert->z = 0.0f;
	pInVert->nx = 0.0f;
	pInVert->ny = 0.0f;
	pInVert->nz =-1.0f;

	//
	// Unlock the input vertex buffer
	//
	pInVB->Unlock();


	//
	// Iterate through lighting angles.  Rotate lighting vector one full rotation.
	//
	for (fLightAngle = 0.0f; fLightAngle < (2*D3DQA_PI); fLightAngle+=((2*D3DQA_PI)/100))
	{
			//
			// Normalized vector components generated based on the angles for this iteration
			//
			float fLightX, fLightY, fLightZ;

			fLightX = 0.0f;
			fLightY = (float)sin(fLightAngle);
			fLightZ = (float)cos(fLightAngle);

            //
            // If fLightZ is some very small number, there is a chance that some
            // precision limits could be exceeded inside the driver, giving transient
            // errors. For example: a driver that uses 16.16 format internally for
            // ProcessVertices might convert a number such as +1e-06 to 0.
            // This is not an important problem, so we will skip these cases.
            //
			if (fLightZ < m_fTestTolerance && fLightZ > 0)
			{
			    continue;
			}

			d3dLight.Direction.x = _M(fLightX);
			d3dLight.Direction.y = _M(fLightY);
			d3dLight.Direction.z = _M(fLightZ);

			if( FAILED( hr = m_pd3dDevice->SetLight(0, &d3dLight, D3DMFMT_D3DMVALUE_FLOAT)))
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetLight failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}

			if( FAILED( hr = m_pd3dDevice->LightEnable(0, TRUE)))
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileDevice::LightEnable failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}


			//
			// This branch protects against the case where the dot product
			// between the halfway vector and the vertex normal is less
			// than zero (which implies no specular light in processed
			// vertices)
			//
			if (fLightZ > 0.0f)
			{
				//
				// Compute expected specular light, via non-local-viewer
				// halfway vector:
				//
				//    H = norm((0,0,1) + L   ) 
				//                        dir
				//

				//
				// Non-local viewers view at infinite Z.  Normalized, this
				// view vector affects only the Z component, by adding one.
				//
				fLightZ += 1.0f; 

				//
				// Normalize the sum of the view vector and the light vector
				//
				fMagnitude = (float)sqrt(fLightY*fLightY + (fLightZ)*(fLightZ));

				//
				// Re-scale to account for format (ProcessVertices produces
				// packed DWORD colors with eight bits per component)
				//
				fExpected = 255.0f * (fLightZ / fMagnitude);
			}
			else
			{
				fExpected = 0.0f;
			}

			//
			// Perform TnL on input VB, store in output VB
			//
			if( FAILED( hr = m_pd3dDevice->ProcessVertices(0,         // UINT SrcStartIndex,
													  0,         // UINT DestIndex,
													  1,         // UINT VertexCount,
													  pOutVB,    // IDirect3DMobileVertexBuffer* pDestBuffer,
													  0)))       // DWORD Flags
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileDevice::ProcessVertices failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}

			//
			// Lock results of ProcessVertices
			//
			if( FAILED( hr = pOutVB->Lock(0,                  // UINT OffsetToLock,
									 uiOutVertSize,      // UINT SizeToLock,
									 (VOID**)(&pOutVert), // VOID** ppbData,
									 0)))                // DWORD Flags
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}


			//
			// Examine the output vertex buffer
			//
			fRedResult = (float)D3DQA_GETRED(pOutVert->Specular);
			fGreenResult = (float)D3DQA_GETGREEN(pOutVert->Specular);
			fBlueResult = (float)D3DQA_GETBLUE(pOutVert->Specular);

			fRedDelta = (float)fabs(fExpected - fRedResult);
			fGreenDelta = (float)fabs(fExpected - fGreenResult);
			fBlueDelta = (float)fabs(fExpected - fBlueResult);

			//
			// Release the lock (the target value has been stored elsewhere)
			//
			pOutVB->Unlock();

			// 
			// Report any unacceptable error
			// 
			if ((fRedDelta > 1) ||
				(fGreenDelta > 1) ||
				(fBlueDelta > 1))
			{
				DebugOut(DO_ERROR, L"Error exceeds tolerance. Failing.");
				Result = TPR_FAIL;

				uiNumFailures++;
				if (uiNumFailures >= uiFailureThreshhold)
				{
					DebugOut(L"Failure threshhold exceeded. Failing.");
					Result = TPR_FAIL; // Should have been set to this previously anyway
					goto cleanup;
				}

			}
	}

cleanup:

	//
	// Unlock if not already unlocked (no op if it is already unlocked)
	//
	if (pInVB)
		pInVB->Unlock();
	if (pOutVB)
		pOutVB->Unlock();

	//
	// Clean up the vertex buffers
	//
	if (pInVB)
		pInVB->Release();
	if (pOutVB)
		pOutVB->Release();

	//
	// Clear active vertex buffer ref-count
	//
	if (m_pd3dDevice)
		m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
									   NULL,  // IDirect3DMobileVertexBuffer* pStreamData
									   0);    // UINT Stride

	//
	// Return tux test case result
	//
	return Result;
}




//
// ExecuteGlobalAmbientTest
//  
//   This test verifies that ProcessVertices computes global ambient lighting
//   correctly, as a part of the diffuse light formula:
//
//         D   =Va*La + sumi(Vd*(N dot Ldi)*Lci+Va*Lcai)
//          RGB
//   
//   For simplicity, it is expected that clipping is off.
//
// Arguments:
//
//   None
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecuteGlobalAmbientTest()
{
    HRESULT hr;
    
	DebugOut(L"Beginning ExecuteGlobalAmbientTest.");

    //
    // FVF structures and bit masks for use with this test
    //
    #define D3DQA_GATTEST_IN_FVF D3DMFVF_XYZ_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_SPECULAR
    struct D3DQA_GATTEST_IN
    {
        FLOAT x, y, z;
	    DWORD Diffuse;
	    DWORD Specular;
    };

    #define D3DQA_GATTEST_OUT_FVF D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_SPECULAR
    struct D3DQA_GATTEST_OUT
    {
        FLOAT x, y, z, rhw;
	    DWORD Diffuse;
	    DWORD Specular;
    };

	//
	// Storage for per-component vertex lighting results.  Although
	// the values stored are intended to be integers in the range of
    // [0,255], the storage is declared in the floating point type for
    // computational convenience.
	//
	float fRedResult, fGreenResult, fBlueResult;

	//
	// Color value for material source; range: [0.0f,1.0f]
	//
	float fMaterialSourceColorValue;

	//
	// Color for material source; range: [0.0f,1.0f]
	//
	UINT uiMaterialSourceColor;

	//
	// Material to be used in the case where the material source is set
	// to material rather than a vertex component
	//
	D3DMMATERIAL Material;

	// 
	// Iterator to select a color source among the three options; DWORD
	// for actual value to pass to SetRenderState.
	//
	UINT uiColSource;
	DWORD dwAmbientMatSource;

	//
	// Storage used to track the difference between the expected
	// per-component vertex lighting results, and the actual per-
	// component vertex lighting results.
	//
	float fRedDelta, fGreenDelta, fBlueDelta;

	//
	// Expected vertex lighting result (R,G,B)
	//
	float fExpected;

	//
	// Color value for ambient light; range: [0.0f,1.0f]
	//
	float fAmbientLightColorValue;

	//
	// Color for ambient light; range: [0,255]
	//
	UINT uiAmbientLightColor;

	//
	// Input and output Vertex buffers for ProcessVertices
	//
	IDirect3DMobileVertexBuffer *pInVB = NULL;
	IDirect3DMobileVertexBuffer *pOutVB = NULL;

	//
	// FVF sizes for this test
	//
	CONST UINT uiInVertSize = sizeof(D3DQA_GATTEST_IN);  
	CONST UINT uiOutVertSize = sizeof(D3DQA_GATTEST_OUT);

	//
	// Used for VB locks, to access FVF data (position, color)
	//
	D3DQA_GATTEST_IN *pInVert = NULL;
	D3DQA_GATTEST_OUT *pOutVert = NULL;

	// 
	// Device Capabilities
	// 
	D3DMCAPS Caps;

	//
	// The test will return TPR_FAIL if even one result exceeds the tolerance, but to aid in
	// debugging the test will continue to execute until reaching the number of failures below.
	//
	CONST UINT uiFailureThreshhold = 25;
	UINT uiNumFailures = 0;

	//
	// Tux Test Case Result; all non-pass blocks set this to another value
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}
    
	// 
	// Query the device's capabilities
	// 
	if( FAILED( hr = m_pd3dDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::GetDeviceCaps failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Verify ability to toggle amongst material sources
	//
	if (!(Caps.VertexProcessingCaps & D3DMVTXPCAPS_MATERIALSOURCE))
	{
		DebugOut(DO_ERROR, L"Test case requires D3DMVTXPCAPS_MATERIALSOURCE. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Disable clipping
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_CLIPPING, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pOutVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DQA_GATTEST_OUT_FVF, uiOutVertSize, D3DMUSAGE_DONOTCLIP);
	if (NULL == pOutVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pInVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DQA_GATTEST_IN_FVF, uiInVertSize, 0);
	if (NULL == pInVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Lock the input vertex buffer
	//
	if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
							uiInVertSize,       // UINT SizeToLock,
							(VOID**)(&pInVert), // VOID** ppbData,
							0)))                // DWORD Flags
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Fill the input vertex buffer
	//
	pInVert->x = 0.0f;
	pInVert->y = 0.0f;
	pInVert->z = 0.0f;
	pInVert->Diffuse = D3DMCOLOR_XRGB(0x00,0x00,0x00);
	pInVert->Specular = D3DMCOLOR_XRGB(0x00,0x00,0x00);

	//
	// Unlock the input vertex buffer
	//
	pInVB->Unlock();
 
    //
    // Enable lighting
    //
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_LIGHTING, TRUE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}
	
    //
    // Enabling per-vertex color allows the system to include the color
    // defined for individual vertices in its lighting calculations. 
    //
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_COLORVERTEX, TRUE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	ZeroMemory(&Material, sizeof(D3DMMATERIAL));

    //
    // Iterate through a selection of colors for the light source (directional light)
    //
	for(uiAmbientLightColor = 0; uiAmbientLightColor < 256; uiAmbientLightColor++)
	{
        // 
        // Convert to a range/format that is suitable for use in the color value structure
        // 
		fAmbientLightColorValue = (float)uiAmbientLightColor / 255.0f;

        //
        // Iterate through a selection of colors for the ambient material source
        //
		for(uiMaterialSourceColor = 0; uiMaterialSourceColor < 256; uiMaterialSourceColor++)
		{
            // 
            // Convert to a range/format that is suitable for use in the color value structure
            // 
			fMaterialSourceColorValue = (float)uiMaterialSourceColor / 255.0f;
    
            // 
            // Iterate through all three valid ambient material sources
            // 
			for(uiColSource = 0; uiColSource < D3DQA_COLOR_SOURCES; uiColSource++)
			{


				if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_AMBIENT,
						                                 D3DMCOLOR_XRGB(uiAmbientLightColor, uiAmbientLightColor, uiAmbientLightColor))))
				{
					DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
					Result = TPR_ABORT;
					goto cleanup;
				}


                // 
                // Depending on the ambient material color source selected for this
                // iteration, setup for ProcessVertices accordingly
                // 
				switch(uiColSource)
				{
				case 0: // Use the diffuse color from the vertex

					//
					// Lock the input vertex buffer
					//
					if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
											uiInVertSize,       // UINT SizeToLock,
											(VOID**)(&pInVert), // VOID** ppbData,
											0)))                // DWORD Flags
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Fill the input vertex buffer
					//
					pInVert->Diffuse = D3DMCOLOR_XRGB(uiMaterialSourceColor,uiMaterialSourceColor,uiMaterialSourceColor);
					pInVert->Specular = D3DMCOLOR_XRGB(0x00,0x00,0x00);

					//
					// Unlock the input vertex buffer
					//
					pInVB->Unlock();

					dwAmbientMatSource = D3DMMCS_COLOR1;
					break;

				case 1: // Use the specular color from the vertex
					//
					// Lock the input vertex buffer
					//
					if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
											uiInVertSize,       // UINT SizeToLock,
											(VOID**)(&pInVert), // VOID** ppbData,
											0)))                // DWORD Flags
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Fill the input vertex buffer
					//
					pInVert->Diffuse = D3DMCOLOR_XRGB(0x00,0x00,0x00);
					pInVert->Specular = D3DMCOLOR_XRGB(uiMaterialSourceColor,uiMaterialSourceColor,uiMaterialSourceColor);

					//
					// Unlock the input vertex buffer
					//
					pInVB->Unlock();

					dwAmbientMatSource = D3DMMCS_COLOR2;

					break;


				case 2: // Use the material's color
					
					Material.Ambient.r = _M(fMaterialSourceColorValue);
					Material.Ambient.g = _M(fMaterialSourceColorValue);
					Material.Ambient.b = _M(fMaterialSourceColorValue);
					Material.Ambient.a = _M(0.0f);

					if( FAILED( hr = m_pd3dDevice->SetMaterial(&Material, D3DMFMT_D3DMVALUE_FLOAT)))
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetMaterial failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}
					
					dwAmbientMatSource = D3DMMCS_MATERIAL;

					break;
				}

				//
				// According to the ambient material color source selected for this
                // iteration, indicate the correct enumerator, via render state
				//
				if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_AMBIENTMATERIALSOURCE, dwAmbientMatSource)))
				{
					DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
					Result = TPR_ABORT;
					goto cleanup;
				}

				//
				// Perform TnL on input VB, store in output VB
				//
				if( FAILED( hr = m_pd3dDevice->ProcessVertices(0,         // UINT SrcStartIndex,
														  0,         // UINT DestIndex,
														  1,         // UINT VertexCount,
														  pOutVB,    // IDirect3DMobileVertexBuffer* pDestBuffer,
														  0)))       // DWORD Flags
				{
					DebugOut(DO_ERROR, L"IDirect3DMobileDevice::ProcessVertices failed. (hr = 0x%08x) Aborting.", hr);
					Result = TPR_ABORT;
					goto cleanup;
				}

				//
				// Lock results of ProcessVertices
				//
				if( FAILED( hr = pOutVB->Lock(0,                  // UINT OffsetToLock,
										 uiOutVertSize,      // UINT SizeToLock,
										 (VOID**)(&pOutVert), // VOID** ppbData,
										 0)))                // DWORD Flags
				{
					DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
					Result = TPR_ABORT;
					goto cleanup;
				}

				//
				// Examine the output vertex buffer
				//
				// Compute the expected result, and derive the actual per-component results
				//
				fExpected = (float)((UCHAR)(fMaterialSourceColorValue * fAmbientLightColorValue*255.0f));
				fRedResult = (float)D3DQA_GETRED(pOutVert->Diffuse);
				fGreenResult = (float)D3DQA_GETGREEN(pOutVert->Diffuse);
				fBlueResult = (float)D3DQA_GETBLUE(pOutVert->Diffuse);

                // 
                // Compute the differences between the actual results and the expected results
                // 
				fRedDelta = (float)fabs(fExpected - fRedResult);
				fGreenDelta = (float)fabs(fExpected - fGreenResult);
				fBlueDelta = (float)fabs(fExpected - fBlueResult);

				//
				// Release the lock (the target value has been stored elsewhere)
				//
				pOutVB->Unlock();

                // 
                // Report any unacceptable error
                // 
				if ((fRedDelta > 1) ||
					(fGreenDelta > 1) ||
					(fBlueDelta > 1))
				{
					DebugOut(DO_ERROR, L"Error exceeds tolerance. Failing.");

					// 
					// Log the iteration's results to the debugger
					// 
					DebugOut(L"RED (Actual: %g; Expected: %g; Delta: %g)  "
						  L"BLUE (Actual: %g; Expected: %g; Delta: %g)  "
						  L"GREEN (Actual: %g; Expected: %g; Delta: %g) ",
						  fRedResult,   fExpected, fRedDelta,
						  fGreenResult, fExpected, fGreenDelta,
						  fBlueResult,  fExpected, fBlueDelta);

					Result = TPR_FAIL;

					uiNumFailures++;
					if (uiNumFailures >= uiFailureThreshhold)
					{
						DebugOut(L"Failure threshhold exceeded. Failing.");
						Result = TPR_FAIL; // Should have been set to this previously anyway
						goto cleanup;
					}
				}
			}
		}
	}

cleanup:

	//
	// Unlock if not already unlocked (no op if it is already unlocked)
	//
	if (pInVB)
		pInVB->Unlock();
	if (pOutVB)
		pOutVB->Unlock();

	//
	// Clean up the vertex buffers
	//
	if (pInVB)
		pInVB->Release();
	if (pOutVB)
		pOutVB->Release();

	//
	// Clear active vertex buffer ref-count
	//
	if (m_pd3dDevice)
		m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
									   NULL,  // IDirect3DMobileVertexBuffer* pStreamData
									   0);    // UINT Stride

	//
	// Return tux test case result
	//
	return Result;
}


//
// ExecuteLocalAmbientTest
//  
//   This test verifies that ProcessVertices computes local ambient lighting
//   correctly, as a part of the diffuse light formula:
//
//         D   =Va*La + sumi(Vd*(N dot Ldi)*Lci+Va*Lcai)
//          RGB
//   
//   For simplicity, it is expected that clipping is off.
//
// Arguments:
//
//   None
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecuteLocalAmbientTest()
{
    HRESULT hr;
    
	DebugOut(L"Beginning ExecuteLocalAmbientTest.");

    //
    // FVF structures and bit masks for use with this test
    //
    #define D3DQA_LATTEST_IN_FVF D3DMFVF_XYZ_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_SPECULAR
    struct D3DQA_LATTEST_IN
    {
        FLOAT x, y, z;
	    DWORD Diffuse;
	    DWORD Specular;
    };

    #define D3DQA_LATTEST_OUT_FVF D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_SPECULAR
    struct D3DQA_LATTEST_OUT
    {
        FLOAT x, y, z, rhw;
	    DWORD Diffuse;
	    DWORD Specular;
    };

	//
	// Storage for per-component vertex lighting results.  Although
	// the values stored are intended to be integers in the range of
    // [0,255], the storage is declared in the floating point type for
    // computational convenience.
	//
	float fRedResult, fGreenResult, fBlueResult;

	//
	// Color value for material source; range: [0.0f,1.0f]
	//
	float fMaterialSourceColorValue;

	//
	// Color for material source; range: [0.0f,1.0f]
	//
	UINT uiMaterialSourceColor;

	//
	// Material to be used in the case where the material source is set
	// to material rather than a vertex component
	//
	D3DMMATERIAL Material;

	// 
	// Iterator to select a color source among the three options; DWORD
	// for actual value to pass to SetRenderState.
	//
	UINT uiColSource;
	DWORD dwAmbientMatSource;

	//
	// Storage used to track the difference between the expected
	// per-component vertex lighting results, and the actual per-
	// component vertex lighting results.
	//
	float fRedDelta, fGreenDelta, fBlueDelta;

	//
	// Light value, which will be used to provide the ambient multiplier
	// in the lighting equation (multiplied with material source ambient
	// color)
	//
	// Initialize with a directional light type and a direction.
	//
	D3DMLIGHT d3dLight = {D3DMLIGHT_DIRECTIONAL,                 // D3DMLIGHTTYPE    Type;
	                      {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, // D3DMCOLORVALUE   Diffuse;
	                      {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, // D3DMCOLORVALUE   Specular;
	                      {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, // D3DMCOLORVALUE   Ambient;
	                      {_M(0.0f),_M(0.0f),_M(0.0f)},          // D3DMVECTOR       Position;
	                      {_M(0.0f),_M(0.0f),_M(1.0f)},          // D3DMVECTOR       Direction;
                             0.0f };                             // float           Range;

	//
	// Expected vertex lighting result (R,G,B)
	//
	float fExpected;

	//
	// Color value for ambient light; range: [0.0f,1.0f]
	//
	float fAmbientLightColorValue;

	//
	// Color for ambient light; range: [0,255]
	//
	UINT uiAmbientLightColor;

	//
	// Input and output Vertex buffers for ProcessVertices
	//
	IDirect3DMobileVertexBuffer *pInVB = NULL;
	IDirect3DMobileVertexBuffer *pOutVB = NULL;

	//
	// FVF sizes for this test
	//
	CONST UINT uiInVertSize = sizeof(D3DQA_LATTEST_IN);  
	CONST UINT uiOutVertSize = sizeof(D3DQA_LATTEST_OUT);

	//
	// Used for VB locks, to access FVF data (position, color)
	//
	D3DQA_LATTEST_IN *pInVert = NULL;
	D3DQA_LATTEST_OUT *pOutVert = NULL;

    // 
    // Device Capabilities
    // 
    D3DMCAPS Caps;

	//
	// The test will return TPR_FAIL if even one result exceeds the tolerance, but to aid in
	// debugging the test will continue to execute until reaching the number of failures below.
	//
	CONST UINT uiFailureThreshhold = 25;
	UINT uiNumFailures = 0;

	//
	// Tux Test Case Result; all non-pass blocks set this to another value
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}
    
    // 
    // Query the device's capabilities
    // 
	if( FAILED( hr = m_pd3dDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::GetDeviceCaps failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

    // 
    // If the device supports directional lights, assume that it supports lighting
    // with at least one MaxActiveLights.  Otherwise, the test cannot execute.
    // 
    if (!(D3DMVTXPCAPS_DIRECTIONALLIGHTS & Caps.VertexProcessingCaps))
    {
		DebugOut(DO_ERROR, L"Directional lights not supported. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
    }

	//
	// Disable clipping
	//
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_CLIPPING, FALSE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pOutVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DQA_LATTEST_OUT_FVF, uiOutVertSize, D3DMUSAGE_DONOTCLIP);
	if (NULL == pOutVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Creates a Vertex Buffer; sets stream source and vertex shader type (FVF)
	//
	pInVB = CreateActiveBuffer(m_pd3dDevice, 1, D3DQA_LATTEST_IN_FVF, uiInVertSize, 0);
	if (NULL == pInVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Lock the input vertex buffer
	//
	if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
							uiInVertSize,       // UINT SizeToLock,
							(VOID**)(&pInVert), // VOID** ppbData,
							0)))                // DWORD Flags
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Fill the input vertex buffer
	//
	pInVert->x = 0.0f;
	pInVert->y = 0.0f;
	pInVert->z = 0.0f;
	pInVert->Diffuse = D3DMCOLOR_XRGB(0x00,0x00,0x00);
	pInVert->Specular = D3DMCOLOR_XRGB(0x00,0x00,0x00);

	//
	// Unlock the input vertex buffer
	//
	pInVB->Unlock();
 
    //
    // Enable lighting
    //
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_LIGHTING, TRUE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}
	
    //
    // Enabling per-vertex color allows the system to include the color
    // defined for individual vertices in its lighting calculations. 
    //
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_COLORVERTEX, TRUE)))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	ZeroMemory(&Material, sizeof(D3DMMATERIAL));

    //
    // Iterate through a selection of colors for the light source (directional light)
    //
	for(uiAmbientLightColor = 0; uiAmbientLightColor < 256; uiAmbientLightColor++)
	{
        // 
        // Convert to a range/format that is suitable for use in the color value structure
        // 
		fAmbientLightColorValue = (float)uiAmbientLightColor / 255.0f;

        //
        // Iterate through a selection of colors for the ambient material source
        //
		for(uiMaterialSourceColor = 0; uiMaterialSourceColor < 256; uiMaterialSourceColor++)
		{
            // 
            // Convert to a range/format that is suitable for use in the color value structure
            // 
			fMaterialSourceColorValue = (float)uiMaterialSourceColor / 255.0f;
    
            // 
            // Iterate through all three valid ambient material sources
            // 
			for(uiColSource = 0; uiColSource < D3DQA_COLOR_SOURCES; uiColSource++)
			{

				d3dLight.Ambient.r = _M(fAmbientLightColorValue);
				d3dLight.Ambient.g = _M(fAmbientLightColorValue);
				d3dLight.Ambient.b = _M(fAmbientLightColorValue);
				d3dLight.Ambient.a = _M(0.0f);

				if( FAILED( hr = m_pd3dDevice->SetLight(0, &d3dLight, D3DMFMT_D3DMVALUE_FLOAT)))
				{
					DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetLight failed. (hr = 0x%08x) Aborting.", hr);
					Result = TPR_ABORT;
					goto cleanup;
				}

				if( FAILED( hr = m_pd3dDevice->LightEnable(0, TRUE)))
				{
					DebugOut(DO_ERROR, L"IDirect3DMobileDevice::LightEnable failed. (hr = 0x%08x) Aborting.", hr);
					Result = TPR_ABORT;
					goto cleanup;
				}

                // 
                // Depending on the ambient material color source selected for this
                // iteration, setup for ProcessVertices accordingly
                // 
				switch(uiColSource)
				{
				case 0: // Use the diffuse color from the vertex

					//
					// Lock the input vertex buffer
					//
					if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
											uiInVertSize,       // UINT SizeToLock,
											(VOID**)(&pInVert), // VOID** ppbData,
											0)))                // DWORD Flags
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Fill the input vertex buffer
					//
					pInVert->Diffuse = D3DMCOLOR_XRGB(uiMaterialSourceColor,uiMaterialSourceColor,uiMaterialSourceColor);
					pInVert->Specular = D3DMCOLOR_XRGB(0x00,0x00,0x00);

					//
					// Unlock the input vertex buffer
					//
					pInVB->Unlock();

					dwAmbientMatSource = D3DMMCS_COLOR1;
					break;

				case 1: // Use the specular color from the vertex
					//
					// Lock the input vertex buffer
					//
					if( FAILED( hr = pInVB->Lock(0,                  // UINT OffsetToLock,
											uiInVertSize,       // UINT SizeToLock,
											(VOID**)(&pInVert), // VOID** ppbData,
											0)))                // DWORD Flags
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}

					//
					// Fill the input vertex buffer
					//
					pInVert->Diffuse = D3DMCOLOR_XRGB(0x00,0x00,0x00);
					pInVert->Specular = D3DMCOLOR_XRGB(uiMaterialSourceColor,uiMaterialSourceColor,uiMaterialSourceColor);

					//
					// Unlock the input vertex buffer
					//
					pInVB->Unlock();

					dwAmbientMatSource = D3DMMCS_COLOR2;

					break;


				case 2: // Use the material's color
					
					Material.Ambient.r = _M(fMaterialSourceColorValue);
					Material.Ambient.g = _M(fMaterialSourceColorValue);
					Material.Ambient.b = _M(fMaterialSourceColorValue);
					Material.Ambient.a = _M(0.0f);

					if( FAILED( hr = m_pd3dDevice->SetMaterial(&Material, D3DMFMT_D3DMVALUE_FLOAT)))
					{
						DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetMaterial failed. (hr = 0x%08x) Aborting.", hr);
						Result = TPR_ABORT;
						goto cleanup;
					}
					
					dwAmbientMatSource = D3DMMCS_MATERIAL;

					break;
				}

				//
				// According to the ambient material color source selected for this
                // iteration, indicate the correct enumerator, via render state
				//
				if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_AMBIENTMATERIALSOURCE, dwAmbientMatSource)))
				{
					DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetRenderState failed. (hr = 0x%08x) Aborting.", hr);
					Result = TPR_ABORT;
					goto cleanup;
				}

				//
				// Perform TnL on input VB, store in output VB
				//
				if( FAILED(hr =  m_pd3dDevice->ProcessVertices(0,         // UINT SrcStartIndex,
														  0,         // UINT DestIndex,
														  1,         // UINT VertexCount,
														  pOutVB,    // IDirect3DMobileVertexBuffer* pDestBuffer,
														  0)))       // DWORD Flags
				{
					DebugOut(DO_ERROR, L"IDirect3DMobileDevice::ProcessVertices failed. (hr = 0x%08x) Aborting.", hr);
					Result = TPR_ABORT;
					goto cleanup;
				}

				//
				// Lock results of ProcessVertices
				//
				if( FAILED( hr = pOutVB->Lock(0,                  // UINT OffsetToLock,
										 uiOutVertSize,      // UINT SizeToLock,
										 (VOID**)(&pOutVert), // VOID** ppbData,
										 0)))                // DWORD Flags
				{
					DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
					Result = TPR_ABORT;
					goto cleanup;
				}

				//
				// Examine the output vertex buffer
				//
				// Compute the expected result, and derive the actual per-component results
				//
				fExpected = (float)((UCHAR)(fMaterialSourceColorValue * fAmbientLightColorValue*255.0f));
				fRedResult = (float)D3DQA_GETRED(pOutVert->Diffuse);
				fGreenResult = (float)D3DQA_GETGREEN(pOutVert->Diffuse);
				fBlueResult = (float)D3DQA_GETBLUE(pOutVert->Diffuse);

                // 
                // Compute the differences between the actual results and the expected results
                // 
				fRedDelta = (float)fabs(fExpected - fRedResult);
				fGreenDelta = (float)fabs(fExpected - fGreenResult);
				fBlueDelta = (float)fabs(fExpected - fBlueResult);

				//
				// Release the lock (the target value has been stored elsewhere)
				//
				pOutVB->Unlock();

                // 
                // Report any unacceptable error
                // 
				if ((fRedDelta > 1) ||
					(fGreenDelta > 1) ||
					(fBlueDelta > 1))
				{
					DebugOut(DO_ERROR, L"Error exceeds tolerance. Failing.");

					// 
					// Log the iteration's results to the debugger
					// 
					DebugOut(L"RED (Actual: %g; Expected: %g; Delta: %g)  "
						  L"BLUE (Actual: %g; Expected: %g; Delta: %g)  "
						  L"GREEN (Actual: %g; Expected: %g; Delta: %g) ",
						  fRedResult,   fExpected, fRedDelta,
						  fGreenResult, fExpected, fGreenDelta,
						  fBlueResult,  fExpected, fBlueDelta);
					
					Result = TPR_FAIL;

					uiNumFailures++;
					if (uiNumFailures >= uiFailureThreshhold)
					{
						DebugOut(L"Failure threshhold exceeded. Failing.");
						Result = TPR_FAIL; // Should have been set to this previously anyway
						goto cleanup;
					}
				}
			}
		}
	}

cleanup:

	//
	// Unlock if not already unlocked (no op if it is already unlocked)
	//
	if (pInVB)
		pInVB->Unlock();
	if (pOutVB)
		pOutVB->Unlock();

	//
	// Clean up the vertex buffers
	//
	if (pInVB)
		pInVB->Release();
	if (pOutVB)
		pOutVB->Release();

	//
	// Clear active vertex buffer ref-count
	//
	if (m_pd3dDevice)
		m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
									   NULL,  // IDirect3DMobileVertexBuffer* pStreamData
									   0);    // UINT Stride

	//
	// Return tux test case result
	//
	return Result;
}


//
// ExecuteNoStreamSourceTest
//
//   Stability test for calling ProcessVertices without a prior call
//   to SetStreamSource
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecuteNoStreamSourceTest()
{
    HRESULT hr;
    
	//
	// Source / Destination vertex buffers
	//
	IDirect3DMobileVertexBuffer *pDestBuf = NULL;
	IDirect3DMobileVertexBuffer *pSourceBuf = NULL;

	DebugOut(L"Beginning ExecuteNoStreamSourceTest.");

	VertXYZFloat *pVert = NULL;

    //
    // FVF Bits for vertex buffer creation
    //
    DWORD dwFVFBits;

	//
	// VB size to attempt to create
	//
	UINT uiSizeOneVert;

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	//
	// Valid pool for vertex buffers
	//
	D3DMPOOL VertexBufPool;

	dwFVFBits = D3DMFVF_XYZ_FLOAT;

    //
    // Determine size of a one-vert buffer, for this FVF
    //
    uiSizeOneVert = BytesPerVertex(dwFVFBits);

	//
	// Tux Test Case Result; all non-pass blocks set this to another value
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve driver capabilities
	//
	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::GetDeviceCaps failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	if (D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_SYSTEMMEM;
	}
	else if (D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_VIDEOMEM;
	}
	else
	{
		DebugOut(DO_ERROR, L"Fatal error; no pool supported for vertex buffers in D3DMCAPS.SurfaceCaps. Failing.");
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(BytesPerVertex(D3DMFVF_XYZRHW_FLOAT), // UINT Length
		                                        0,                    // DWORD Usage
						                        D3DMFVF_XYZRHW_FLOAT, // DWORD FVF
						                        VertexBufPool,        // D3DMPOOL Pool
						                       &pDestBuf)))           // IDirect3DMobileVertexBuffer** ppVertexBuffer
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::CreateVertexBuffer failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(uiSizeOneVert, // UINT Length
		                                        0,             // DWORD Usage
						                        dwFVFBits,     // DWORD FVF
						                        VertexBufPool, // D3DMPOOL Pool
						                       &pSourceBuf)))  // IDirect3DMobileVertexBuffer** ppVertexBuffer
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::CreateVertexBuffer failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = pSourceBuf->Lock(0,               // UINT OffsetToLock
		                        uiSizeOneVert,   // UINT SizeToLock
							    (VOID**)(&pVert),// VOID** ppbData
								0)))             // DWORD Flags
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// For the purposes of this test; no particular data needs to be
	// inserted into the input buffer
	//

	if (FAILED(hr = pSourceBuf->Unlock()))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Unlock failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// This is a test of stability only; no need to inspect the result of
	// the ProcessVertices call.
	//
    m_pd3dDevice->ProcessVertices(0,        // UINT SrcStartIndex
                                  0,        // UINT DestIndex
                                  1,        // UINT VertexCount
                                  pDestBuf, // IDirect3DMobileVertexBuffer* pDestBuffer
                                  0);      // DWORD Flags
cleanup:

	//
	// Unlock if not already unlocked (no op if it is already unlocked)
	//
	if (pSourceBuf)
		pSourceBuf->Unlock();
	if (pDestBuf)
		pDestBuf->Unlock();
	
	//
	// Release vertex buffers
	//
	if (pSourceBuf)
		pSourceBuf->Release();
	if (pDestBuf)
		pDestBuf->Release();

	//
	// Clear active vertex buffer ref-count
	//
	if (m_pd3dDevice)
		m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
		                               NULL,  // IDirect3DMobileVertexBuffer* pStreamData
		                               0);    // UINT Stride

	//
	// Return tux test case result
	//
	return Result;
}


//
// ExecuteNoStreamSourceTest
//
//   Stability test for calling ProcessVertices without a prior call
//   to SetStreamSource
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecuteStreamSourceTest()
{
    HRESULT hr;
    
	//
	// Source / Destination vertex buffers
	//
	IDirect3DMobileVertexBuffer *pDestBuf = NULL;
	IDirect3DMobileVertexBuffer *pSourceBuf = NULL;

	DebugOut(L"Beginning ExecuteStreamSourceTest.");

	VertXYZFloat *pVert = NULL;

	//
	// ValidateDevice result
	//
	DWORD dwScratch;

    //
    // FVF Bits for vertex buffer creation
    //
    DWORD dwFVFBits;

	//
	// VB size to attempt to create
	//
	UINT uiSizeOneVert;

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	//
	// Valid pool for vertex buffers
	//
	D3DMPOOL VertexBufPool;

	dwFVFBits = D3DMFVF_XYZ_FLOAT;

    //
    // Determine size of a one-vert buffer, for this FVF
    //
    uiSizeOneVert = BytesPerVertex(dwFVFBits);

	//
	// Tux Test Case Result; all non-pass blocks set this to another value
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve driver capabilities
	//
	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::GetDeviceCaps failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_SYSTEMMEM;
	}
	else if (D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_VIDEOMEM;
	}
	else
	{
		DebugOut(DO_ERROR, L"Fatal error; no pool supported for vertex buffers in D3DMCAPS.SurfaceCaps. Failing.");
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(2*BytesPerVertex(D3DMFVF_XYZRHW_FLOAT), // UINT Length
		                                        0,                    // DWORD Usage
						                        D3DMFVF_XYZRHW_FLOAT, // DWORD FVF
						                        VertexBufPool,        // D3DMPOOL Pool
						                       &pDestBuf)))           // IDirect3DMobileVertexBuffer** ppVertexBuffer
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::CreateVertexBuffer failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(2*uiSizeOneVert, // UINT Length
		                                        0,             // DWORD Usage
						                        dwFVFBits,     // DWORD FVF
						                        VertexBufPool, // D3DMPOOL Pool
						                       &pSourceBuf)))  // IDirect3DMobileVertexBuffer** ppVertexBuffer
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::CreateVertexBuffer failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = pSourceBuf->Lock(0,               // UINT OffsetToLock
		                        2*uiSizeOneVert,   // UINT SizeToLock
							    (VOID**)(&pVert),// VOID** ppbData
								0)))             // DWORD Flags
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// For the purposes of this test; no particular data needs to be
	// inserted into the input buffer
	//

	if (FAILED(hr = pSourceBuf->Unlock()))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Unlock failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = m_pd3dDevice->SetStreamSource( 0,          // UINT StreamNumber
		                                      pSourceBuf, // IDirect3DMobileVertexBuffer* pStreamData
											  0)))        // UINT Stride
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetStreamSource failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	m_pd3dDevice->ProcessVertices(0,        // UINT SrcStartIndex
								  0,        // UINT DestIndex
								  1,        // UINT VertexCount
								  pDestBuf, // IDirect3DMobileVertexBuffer* pDestBuffer
								  0);      // DWORD Flags

	//
	// Force the command buffer to empty
	//
	m_pd3dDevice->ValidateDevice(&dwScratch);


	if (FAILED(hr = m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
		                                      NULL,  // IDirect3DMobileVertexBuffer* pStreamData
											  0)))   // UINT Stride
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetStreamSource failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

cleanup:

	//
	// Unlock if not already unlocked (no op if it is already unlocked)
	//
	if (pSourceBuf)
		pSourceBuf->Unlock();
	if (pDestBuf)
		pDestBuf->Unlock();

	//
	// Release Vertex Buffers
	//
	if (pSourceBuf)
		pSourceBuf->Release();
	if (pDestBuf)
		pDestBuf->Release();

	//
	// Clear active vertex buffer ref-count
	//
	if (m_pd3dDevice)
		m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
		                               NULL,  // IDirect3DMobileVertexBuffer* pStreamData
		                               0);    // UINT Stride
	//
	// Return tux test case result
	//
	return Result;
}


//
// ExecuteDoNotCopyDataTest
//
//   Test for calling ProcessVertices with D3DMPV_DONOTCOPYDATA
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecuteDoNotCopyDataTest()
{
    HRESULT hr;
    
	//
	// Source / Destination vertex buffers
	//
	IDirect3DMobileVertexBuffer *pDestBuf = NULL;
	IDirect3DMobileVertexBuffer *pSourceBuf = NULL;

	DebugOut(L"Beginning ExecuteDoNotCopyDataTest.");

	VertXYZFloat *pVert = NULL;

	//
	// ValidateDevice result
	//
	DWORD dwScratch;

    //
    // FVF Bits for vertex buffer creation
    //
    DWORD dwFVFBits;

	//
	// VB size to attempt to create
	//
	UINT uiSizeOneVert;

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	//
	// Valid pool for vertex buffers
	//
	D3DMPOOL VertexBufPool;

	dwFVFBits = D3DMFVF_XYZ_FLOAT;

    //
    // Determine size of a one-vert buffer, for this FVF
    //
    uiSizeOneVert = BytesPerVertex(dwFVFBits);

	//
	// Tux Test Case Result; all non-pass blocks set this to another value
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve driver capabilities
	//
	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::GetDeviceCaps failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_SYSTEMMEM;
	}
	else if (D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_VIDEOMEM;
	}
	else
	{
		DebugOut(DO_ERROR, L"Fatal error; no pool supported for vertex buffers in D3DMCAPS.SurfaceCaps. Failing.");
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(2*BytesPerVertex(D3DMFVF_XYZRHW_FLOAT), // UINT Length
		                                        0,                    // DWORD Usage
						                        D3DMFVF_XYZRHW_FLOAT, // DWORD FVF
						                        VertexBufPool,        // D3DMPOOL Pool
						                       &pDestBuf)))           // IDirect3DMobileVertexBuffer** ppVertexBuffer
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::CreateVertexBuffer failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(2*uiSizeOneVert, // UINT Length
		                                        0,             // DWORD Usage
						                        dwFVFBits,     // DWORD FVF
						                        VertexBufPool, // D3DMPOOL Pool
						                       &pSourceBuf)))  // IDirect3DMobileVertexBuffer** ppVertexBuffer
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::CreateVertexBuffer failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = pSourceBuf->Lock(0,               // UINT OffsetToLock
		                        2*uiSizeOneVert,   // UINT SizeToLock
							    (VOID**)(&pVert),// VOID** ppbData
								0)))             // DWORD Flags
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// For the purposes of this test; no particular data needs to be
	// inserted into the input buffer
	//

	if (FAILED(hr = pSourceBuf->Unlock()))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Unlock failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = m_pd3dDevice->SetStreamSource( 0,          // UINT StreamNumber
		                                      pSourceBuf, // IDirect3DMobileVertexBuffer* pStreamData
											  0)))        // UINT Stride
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetStreamSource failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	m_pd3dDevice->ProcessVertices(0,                     // UINT SrcStartIndex
								  0,                     // UINT DestIndex
								  1,                     // UINT VertexCount
								  pDestBuf,              // IDirect3DMobileVertexBuffer* pDestBuffer
								  D3DMPV_DONOTCOPYDATA); // DWORD Flags

	//
	// Force the command buffer to empty
	//
	m_pd3dDevice->ValidateDevice(&dwScratch);


	if (FAILED(hr = m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
		                                      NULL,  // IDirect3DMobileVertexBuffer* pStreamData
											  0)))   // UINT Stride
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetStreamSource failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}
	
cleanup:

	//
	// Unlock if not already unlocked (no op if it is already unlocked)
	//
	if (pSourceBuf)
		pSourceBuf->Unlock();
	if (pDestBuf)
		pDestBuf->Unlock();

	//
	// Release Vertex Buffers
	//
	if (pSourceBuf)
		pSourceBuf->Release();
	if (pDestBuf)
		pDestBuf->Release();

	//
	// Clear active vertex buffer ref-count
	//
	if (m_pd3dDevice)
		m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
		                               NULL,  // IDirect3DMobileVertexBuffer* pStreamData
		                               0);    // UINT Stride

	//
	// Return tux test case result
	//
	return Result;
}



//
// ExecuteNotEnoughVertsTest
//
//   Test for calling ProcessVertices with less verts than required
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecuteNotEnoughVertsTest()
{
    HRESULT hr;
    
	//
	// Source / Destination vertex buffers
	//
	IDirect3DMobileVertexBuffer *pDestBuf = NULL;
	IDirect3DMobileVertexBuffer *pSourceBuf = NULL;

	DebugOut(L"Beginning ExecuteNotEnoughVertsTest.");

	VertXYZFloat *pVert = NULL;

	//
	// ValidateDevice result
	//
	DWORD dwScratch;

    //
    // FVF Bits for vertex buffer creation
    //
    DWORD dwFVFBits;

	//
	// VB size to attempt to create
	//
	UINT uiSizeOneVert;

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	//
	// Valid pool for vertex buffers
	//
	D3DMPOOL VertexBufPool;

	dwFVFBits = D3DMFVF_XYZ_FLOAT;

    //
    // Determine size of a one-vert buffer, for this FVF
    //
    uiSizeOneVert = BytesPerVertex(dwFVFBits);

	//
	// Tux Test Case Result; all non-pass blocks set this to another value
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve driver capabilities
	//
	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::GetDeviceCaps failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_SYSTEMMEM;
	}
	else if (D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_VIDEOMEM;
	}
	else
	{
		DebugOut(DO_ERROR, L"Fatal error; no pool supported for vertex buffers in D3DMCAPS.SurfaceCaps. Failing.");
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(2*BytesPerVertex(D3DMFVF_XYZRHW_FLOAT), // UINT Length
		                                        0,                    // DWORD Usage
						                        D3DMFVF_XYZRHW_FLOAT, // DWORD FVF
						                        VertexBufPool,        // D3DMPOOL Pool
						                       &pDestBuf)))           // IDirect3DMobileVertexBuffer** ppVertexBuffer
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::CreateVertexBuffer failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(2*uiSizeOneVert, // UINT Length
		                                        0,             // DWORD Usage
						                        dwFVFBits,     // DWORD FVF
						                        VertexBufPool, // D3DMPOOL Pool
						                       &pSourceBuf)))  // IDirect3DMobileVertexBuffer** ppVertexBuffer
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::CreateVertexBuffer failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = pSourceBuf->Lock(0,               // UINT OffsetToLock
		                        2*uiSizeOneVert,   // UINT SizeToLock
							    (VOID**)(&pVert),// VOID** ppbData
								0)))             // DWORD Flags
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Lock failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// For the purposes of this test; no particular data needs to be
	// inserted into the input buffer
	//

	if (FAILED(hr = pSourceBuf->Unlock()))
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Unlock failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = m_pd3dDevice->SetStreamSource( 0,          // UINT StreamNumber
		                                      pSourceBuf, // IDirect3DMobileVertexBuffer* pStreamData
											  0)))        // UINT Stride
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetStreamSource failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	m_pd3dDevice->ProcessVertices(0,         // UINT SrcStartIndex
								  0,         // UINT DestIndex
								  5,         // UINT VertexCount
								  pDestBuf,  // IDirect3DMobileVertexBuffer* pDestBuffer
								  0);        // DWORD Flags

	//
	// Force the command buffer to empty
	//
	m_pd3dDevice->ValidateDevice(&dwScratch);


	if (FAILED(hr = m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
		                                      NULL,  // IDirect3DMobileVertexBuffer* pStreamData
											  0)))   // UINT Stride
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::SetStreamSource failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

cleanup:

	//
	// Unlock if not already unlocked (no op if it is already unlocked)
	//
	if (pSourceBuf)
		pSourceBuf->Unlock();
	if (pDestBuf)
		pDestBuf->Unlock();

	//
	// Release Vertex Buffers
	//
	if (pSourceBuf)
		pSourceBuf->Release();
	if (pDestBuf)
		pDestBuf->Release();
	
	//
	// Clear active vertex buffer ref-count
	//
	if (m_pd3dDevice)
		m_pd3dDevice->SetStreamSource( 0,     // UINT StreamNumber
		                               NULL,  // IDirect3DMobileVertexBuffer* pStreamData
		                               0);    // UINT Stride

	//
	// Return tux test case result
	//
	return Result;
}



// 
// ExecuteBlendTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecuteBlendTest(DWORD dwTestIndex)
{
	DebugOut(L"Beginning ExecuteBlendTest.");

	VERIFTESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,&m_d3dpp,dwTestIndex};

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		return TPR_ABORT;
	}

	return BlendTest(&TestCaseArgs);

}



// 
// ExecuteStencilTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecuteStencilTest(DWORD dwTestIndex)
{
	DebugOut(L"Beginning ExecuteStencilTest.");

	VERIFTESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,&m_d3dpp,dwTestIndex};

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		return TPR_ABORT;
	}

	return StencilTest(&TestCaseArgs);

}



// 
// ExecuteResManTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverVerifTest::ExecuteResManTest(DWORD dwTestIndex)
{
	DebugOut(L"Beginning ExecuteResManTest.");

	VERIFTESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,&m_d3dpp,dwTestIndex};

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Aborting.");
		return TPR_ABORT;
	}

	return ResManTest(&TestCaseArgs);

}

