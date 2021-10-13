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
#include "IDirect3DMobileVertexBuffer.h"
#include "DebugOutput.h"
#include "KatoUtils.h"
#include "BufferTools.h"
#include <tux.h>
#include <tchar.h>
#include <stdio.h>

//
// For testing that does not require a particular vertex buffer size,
// use a reasonable default size and corresponding FVF
//
#define D3DQA_DEFAULT_VERTEXBUF_SIZE 96
#define D3DQA_DEFAULT_VERTEXBUF_FVF D3DMFVF_XYZ_FLOAT
#define D3DQA_DEFAULT_VERTEXBUF_FVF_VERTSIZE 12

//
// Maximum priority level tested for Set/GetPriority
//
#define D3DQA_MAX_VB_PRIORITY 512

//
// Maximum size to attempt for testing vertex buffer creation (note that this
// must be a multiple of the vertex size for the specified FVF)
//
#define D3DQA_MAX_VERTEXBUF_SIZE (D3DQA_DEFAULT_VERTEXBUF_FVF_VERTSIZE * 50)

//
// GUID for invalid parameter tests
//
DEFINE_GUID(IID_D3DQAInvalidInterface, 
0x00000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);



//
//  VertexBufferTest
//
//    Constructor for VertexBufferTest class; however, most initialization is deferred.
//
//  Arguments:
//
//    none
//
//  Return Value:
//
//    none
//
VertexBufferTest::VertexBufferTest()
{
	m_bInitSuccess = FALSE;
}


//
// VertexBufferTest
//
//   Initialize VertexBufferTest object
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
HRESULT VertexBufferTest::Init(LPTEST_CASE_ARGS pTestCaseArgs, LPWINDOW_ARGS pWindowArgs, BOOL bDebug, UINT uiTestCase)
{
	HRESULT hr;

	if (FAILED(hr = D3DMInitializer::Init(D3DQA_PURPOSE_RAW_TEST,                      // D3DQA_PURPOSE Purpose
	                                 pWindowArgs,                                 // LPWINDOW_ARGS pWindowArgs
	                                 pTestCaseArgs->pwchSoftwareDeviceFilename,   // TCHAR *ptchDriver
	                                 uiTestCase)))                                // UINT uiTestCase
    {
		DebugOut(DO_ERROR, L"Aborting initialization, due to prior initialization failure. (hr = 0x%08x).", hr);
		return E_FAIL;
    }

	m_bDebugMiddleware = bDebug;

    m_bInitSuccess = TRUE;
    return S_OK;
}


//
//  IsReady
//
//    Accessor method for "initialization state" of VertexBufferTest object.  "Half
//    initialized" states should not occur.
//  
//  Return Value:
//  
//    BOOL:  TRUE if the object is initialized; FALSE if it is not.
//
BOOL VertexBufferTest::IsReady()
{
	if (FALSE == m_bInitSuccess)
	{
		DebugOut(DO_ERROR, L"VertexBufferTest is not ready.");
		return FALSE;
	}

	DebugOut(L"VertexBufferTest is ready.");
	return D3DMInitializer::IsReady();
}

// 
// ~VertexBufferTest
//
//  Destructor for VertexBufferTest.  Currently; there is nothing
//  to do here.
//
VertexBufferTest::~VertexBufferTest()
{
	return; // Success
}


//
// ExecuteQueryInterfaceTest
//
//   Verify IDirect3DMobileVertexBuffer::QueryInterface
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT VertexBufferTest::ExecuteQueryInterfaceTest()
{
	HRESULT hr;

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;
    
    //
    // Interface pointer for vertex buffer to be created
    //
    IDirect3DMobileVertexBuffer *pVertexBuf;

    //
    // Interface pointers to be retrieved by QueryInterface
    //
    IDirect3DMobileResource *pQIResource;
    IDirect3DMobileVertexBuffer *pQIVertexBuf;

	//
	// Pool for vertex buffer creation
	//
	D3DMPOOL VertexBufPool;

	DebugOut(L"Beginning ExecuteQueryInterfaceTest.");

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Retrieve driver capabilities
	//
	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	//
	// Pick the pool that the driver supports, as reported in D3DMCAPS
	//
	if (D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_VIDEOMEM;
	}
	else if (D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_SYSTEMMEM;
	}
	else
	{
		DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSVERTEXBUFFER or D3DMSURFCAPS_VIDVERTEXBUFFER. Aborting.");
		return TPR_ABORT;
	}

    //
    // Create a new vertex buffer
    //
    if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(D3DQA_DEFAULT_VERTEXBUF_SIZE,// UINT Length
		                                        0,                           // DWORD Usage
		                                        D3DQA_DEFAULT_VERTEXBUF_FVF, // DWORD FVF
		                                        VertexBufPool,               // D3DMPOOL Pool
		                                       &pVertexBuf)))                // IDirect3DMobileVertexBuffer** ppVertexBuffer
		
	{
		DebugOut(DO_ERROR, L"CreateVertexBuffer failed. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}


    // 	
    // Bad parameter tests
    // 	

    //
    // Test case #1:  Invalid REFIID
    //
	if (SUCCEEDED(hr = pVertexBuf->QueryInterface(IID_D3DQAInvalidInterface, // REFIID riid
								             (void**)(&pQIVertexBuf)))) // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        pVertexBuf->Release();
		return TPR_FAIL;
	}

    //
    // Test case #2:  Valid REFIID, invalid PPVOID
    //
	if (SUCCEEDED(hr = pVertexBuf->QueryInterface(IID_IDirect3DMobileVertexBuffer, // REFIID riid
								             (void**)0)))                     // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        pVertexBuf->Release();
		return TPR_FAIL;
	}

    //
    // Test case #3:  Invalid REFIID, invalid PPVOID
    //
	if (SUCCEEDED(hr = pVertexBuf->QueryInterface(IID_D3DQAInvalidInterface, // REFIID riid
								             (void**)0)))               // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        pVertexBuf->Release();
		return TPR_FAIL;
	}



    //
    // Valid parameter tests
    //

    // 	
    // Perform an interface query on the interface buffer
    // 	
	if (FAILED(hr = pVertexBuf->QueryInterface(IID_IDirect3DMobileResource, // REFIID riid
								          (void**)(&pQIResource))))    // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface failed. (hr = 0x%08x). Failing.", hr);
        pVertexBuf->Release();
		return TPR_FAIL;
	}

    // 	
    // Perform an interface query on the interface buffer
    // 	
	if (FAILED(hr = pVertexBuf->QueryInterface(IID_IDirect3DMobileVertexBuffer, // REFIID riid
						  		          (void**)(&pQIVertexBuf))))       // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface failed. (hr = 0x%08x). Failing.", hr);
        pQIResource->Release();
        pVertexBuf->Release();
		return TPR_FAIL;
	}


    //
    // Release the original interface pointer; verify that the ref count is still non-zero
    //
    if (!(pVertexBuf->Release()))
    {
		DebugOut(DO_ERROR, L"Reference count for IDirect3DMobileVertexBuffer dropped to zero; unexpected. Failing.");
		return TPR_FAIL;
    }

    //
    // Release the IDirect3DMobileVertexBuffer interface pointer; verify that the ref count is still non-zero
    //
    if (!(pQIVertexBuf->Release()))
    {
		DebugOut(DO_ERROR, L"Reference count for IDirect3DMobileVertexBuffer dropped to zero; unexpected. Failing.");
		return TPR_FAIL;
    }

    //
    // Release the IDirect3DMobileResource interface pointer; ref count should now drop to zero
    //
    if (pQIResource->Release())
    {
		DebugOut(DO_ERROR, L"Reference count for IDirect3DMobileResource is non-zero; unexpected. Failing.");
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
//   Verify IDirect3DMobileVertexBuffer::AddRef
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT VertexBufferTest::ExecuteAddRefTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteAddRefTest.");

	//
	// Pool for vertex buffer creation
	//
	D3DMPOOL VertexBufPool;

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;
    
    //
    // Interface pointer for vertex buffer to be created
    //
    IDirect3DMobileVertexBuffer *pVertexBuf;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Retrieve driver capabilities
	//
	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	//
	// Pick the pool that the driver supports, as reported in D3DMCAPS
	//
	if (D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_VIDEOMEM;
	}
	else if (D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_SYSTEMMEM;
	}
	else
	{
		DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSVERTEXBUFFER or D3DMSURFCAPS_VIDVERTEXBUFFER. Aborting.");
		return TPR_ABORT;
	}

    //
    // Create a new vertex buffer
    //
    if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(D3DQA_DEFAULT_VERTEXBUF_SIZE,// UINT Length
		                                        0,                           // DWORD Usage
		                                        D3DQA_DEFAULT_VERTEXBUF_FVF, // DWORD FVF
		                                        VertexBufPool,               // D3DMPOOL Pool
		                                       &pVertexBuf)))                // IDirect3DMobileVertexBuffer** ppVertexBuffer
		
	{
		DebugOut(DO_ERROR, L"CreateVertexBuffer failed. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	
	//
	// Verify ref-count values; via AddRef/Release return values
	//
	if (2 != pVertexBuf->AddRef())
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer: unexpected reference count. Failing.");
		return TPR_FAIL;
	}
	
	if (1 != pVertexBuf->Release())
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer: unexpected reference count. Failing.");
		return TPR_FAIL;
	}
	
	if (0 != pVertexBuf->Release())
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer: unexpected reference count. Failing.");
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
//   Verify IDirect3DMobileVertexBuffer::Release
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT VertexBufferTest::ExecuteReleaseTest()
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
//   Verify IDirect3DMobileVertexBuffer::GetDevice
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT VertexBufferTest::ExecuteGetDeviceTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteGetDeviceTest.");

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;
    
	//
	// Pool for vertex buffer creation
	//
	D3DMPOOL VertexBufPool;

    //
    // Interface pointer for vertex buffer to be created
    //
    IDirect3DMobileVertexBuffer *pVertexBuf;

	//
	// Device interface pointer to retrieve with GetDevice
	//
	IDirect3DMobileDevice* pDevice;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Retrieve driver capabilities
	//
	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	//
	// Pick the pool that the driver supports, as reported in D3DMCAPS
	//
	if (D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_VIDEOMEM;
	}
	else if (D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_SYSTEMMEM;
	}
	else
	{
		DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSVERTEXBUFFER or D3DMSURFCAPS_VIDVERTEXBUFFER. Aborting.");
		return TPR_ABORT;
	}

    //
    // Create a new vertex buffer
    //
    if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(D3DQA_DEFAULT_VERTEXBUF_SIZE,// UINT Length
		                                        0,                           // DWORD Usage
		                                        D3DQA_DEFAULT_VERTEXBUF_FVF, // DWORD FVF
		                                        VertexBufPool,               // D3DMPOOL Pool
		                                       &pVertexBuf)))                // IDirect3DMobileVertexBuffer** ppVertexBuffer
		
	{
		DebugOut(DO_ERROR, L"CreateVertexBuffer failed. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	//
	// Retrieve the device interface pointer
	//
	if (FAILED(hr = pVertexBuf->GetDevice(&pDevice))) // IDirect3DMobileDevice** ppDevice
	{
		DebugOut(DO_ERROR, L"GetDevice failed. (hr = 0x%08x). Failing.", hr);
		pVertexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Verify that the device interface pointer is consistent with expectations
	//
	if (pDevice != m_pd3dDevice)
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice interface pointer mismatch. Failing.");
		pVertexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Release the device interface reference (incremented by GetDevice)
	//
	pDevice->Release();

	//
	// Release the vertex buffer
	//
	pVertexBuf->Release();
	
	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;

}
//
// ExecuteSetPriorityTest
//
//   Verify IDirect3DMobileVertexBuffer::SetPriority
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT VertexBufferTest::ExecuteSetPriorityTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteSetPriorityTest.");

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	//
	// Pool for vertex buffer if no managed support
	//
	D3DMPOOL VertexBufPool;

    //
    // Interface pointer for vertex buffer to be created
    //
    IDirect3DMobileVertexBuffer *pVertexBuf;

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
	// Retrieve driver capabilities
	//
	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	//
	// Verify support for managed pool, before proceeding with test
	//
	if (!(D3DMSURFCAPS_MANAGEDPOOL & Caps.SurfaceCaps))
	{

		//
		// If no D3DMSURFCAPS_MANAGEDPOOL, skip main test, but still call function to verify that
		// it executes to completion in the non-managed-pool case.
		//

		//
		// Pick the pool that the driver supports, as reported in D3DMCAPS
		//
		if (D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps)
		{
			VertexBufPool = D3DMPOOL_VIDEOMEM;
		}
		else if (D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps)
		{
			VertexBufPool = D3DMPOOL_SYSTEMMEM;
		}
		else
		{
			DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSVERTEXBUFFER or D3DMSURFCAPS_VIDVERTEXBUFFER. Aborting.");
			return TPR_ABORT;
		}

		//
		// Create a new vertex buffer
		//
		if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(D3DQA_DEFAULT_VERTEXBUF_SIZE,// UINT Length
													0,                           // DWORD Usage
													D3DQA_DEFAULT_VERTEXBUF_FVF, // DWORD FVF
													VertexBufPool,               // D3DMPOOL Pool
												   &pVertexBuf)))                // IDirect3DMobileVertexBuffer** ppVertexBuffer
			
		{
			DebugOut(DO_ERROR, L"CreateVertexBuffer failed. (hr = 0x%08x). Failing.", hr);
			return TPR_FAIL;
		}


		pVertexBuf->SetPriority(1);
		pVertexBuf->GetPriority();

		pVertexBuf->Release();

		DebugOut(DO_ERROR, L"SetPriority:  Skipping test because driver does not expose D3DMSURFCAPS_MANAGEDPOOL. Skipping.");
		return TPR_SKIP;
	}

    //
    // Create a new vertex buffer
    //
    if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(D3DQA_DEFAULT_VERTEXBUF_SIZE,// UINT Length
		                                        0,                           // DWORD Usage
		                                        D3DQA_DEFAULT_VERTEXBUF_FVF, // DWORD FVF
		                                        D3DMPOOL_MANAGED,            // D3DMPOOL Pool
		                                       &pVertexBuf)))                // IDirect3DMobileVertexBuffer** ppVertexBuffer
		
	{
		DebugOut(DO_ERROR, L"CreateVertexBuffer failed. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	// 
	// Verify default priority of zero; arg is arbitrary (as long as it is non-zero).
	// This caller simply verifies that SetPriority returns the previous priority, and
	// that the previous priority defaults to zero if it has never been explicitely set.
	// 
	if (0 != pVertexBuf->SetPriority(1))
	{
		DebugOut(DO_ERROR, L"SetPriority failed. Failing.");
		pVertexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Iterate through a large set of priorities, and verify that SetPriority always
	// returns the previously-set priority
	//
	pVertexBuf->SetPriority(0);
	for(uiPriority = 1; uiPriority < D3DQA_MAX_VB_PRIORITY; uiPriority++)
	{
		if ((uiPriority-1) != pVertexBuf->SetPriority(uiPriority))
		{
			DebugOut(DO_ERROR, L"SetPriority failed. Failing.");
			pVertexBuf->Release();
			return TPR_FAIL;
		}

		if (uiPriority != pVertexBuf->GetPriority())
		{
			DebugOut(DO_ERROR, L"GetPriority priority mismatch. Failing.");
			pVertexBuf->Release();
			return TPR_FAIL;
		}
	
	}

	pVertexBuf->Release();
	
	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;

}
//
// ExecuteGetPriorityTest
//
//   Verify IDirect3DMobileVertexBuffer::GetPriority
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT VertexBufferTest::ExecuteGetPriorityTest()
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
//   Call IDirect3DMobileVertexBuffer::PreLoad.  This API is essentially an optimization
//   hint to the driver; thus verifying it would require some heuristic based on speeds
//   of various operations.  Currently, we merely call the API; no further verification
//   yet exists.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT VertexBufferTest::ExecutePreLoadTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecutePreLoadTest.");

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	//
	// Pool for vertex buffer if no managed support
	//
	D3DMPOOL VertexBufPool;

    //
    // Interface pointer for vertex buffer to be created
    //
    IDirect3DMobileVertexBuffer *pVertexBuf;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Retrieve driver capabilities
	//
	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	//
	// Verify support for managed pool, before proceeding with test
	//
	if (!(D3DMSURFCAPS_MANAGEDPOOL & Caps.SurfaceCaps))
	{
		//
		// If no D3DMSURFCAPS_MANAGEDPOOL, skip main test, but still call function to verify that
		// it executes to completion in the non-managed-pool case.
		//

		//
		// Pick the pool that the driver supports, as reported in D3DMCAPS
		//
		if (D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps)
		{
			VertexBufPool = D3DMPOOL_VIDEOMEM;
		}
		else if (D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps)
		{
			VertexBufPool = D3DMPOOL_SYSTEMMEM;
		}
		else
		{
			DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSVERTEXBUFFER or D3DMSURFCAPS_VIDVERTEXBUFFER. Aborting.");
			return TPR_ABORT;
		}

		//
		// Create a new vertex buffer
		//
		if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(D3DQA_DEFAULT_VERTEXBUF_SIZE,// UINT Length
													0,                           // DWORD Usage
													D3DQA_DEFAULT_VERTEXBUF_FVF, // DWORD FVF
													VertexBufPool,               // D3DMPOOL Pool
												   &pVertexBuf)))                // IDirect3DMobileVertexBuffer** ppVertexBuffer
			
		{
			DebugOut(DO_ERROR, L"CreateVertexBuffer failed. (hr = 0x%08x). Failing.", hr);
			return TPR_FAIL;
		}

		
		pVertexBuf->PreLoad();
		
		pVertexBuf->Release();

		DebugOut(DO_ERROR, L"SetPriority:  Skipping test because driver does not expose D3DMSURFCAPS_MANAGEDPOOL. Skipping.");

		return TPR_SKIP;
	}

    //
    // Create a new vertex buffer
    //
    if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(D3DQA_DEFAULT_VERTEXBUF_SIZE,// UINT Length
		                                        0,                           // DWORD Usage
		                                        D3DQA_DEFAULT_VERTEXBUF_FVF, // DWORD FVF
		                                        D3DMPOOL_MANAGED,            // D3DMPOOL Pool
		                                       &pVertexBuf)))                // IDirect3DMobileVertexBuffer** ppVertexBuffer
		
	{
		DebugOut(DO_ERROR, L"CreateVertexBuffer failed. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	
	pVertexBuf->PreLoad();
	
	pVertexBuf->Release();
	
	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}
//
// ExecuteGetTypeTest
//
//   Verify IDirect3DMobileVertexBuffer::GetType
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT VertexBufferTest::ExecuteGetTypeTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteGetTypeTest.");

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;
    
	//
	// Pool for vertex buffer creation
	//
	D3DMPOOL VertexBufPool;

    //
    // Interface pointer for vertex buffer to be created
    //
    IDirect3DMobileVertexBuffer *pVertexBuf;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Retrieve driver capabilities
	//
	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	//
	// Pick the pool that the driver supports, as reported in D3DMCAPS
	//
	if (D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_VIDEOMEM;
	}
	else if (D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_SYSTEMMEM;
	}
	else
	{
		DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSVERTEXBUFFER or D3DMSURFCAPS_VIDVERTEXBUFFER. Aborting.");
		return TPR_ABORT;
	}

    //
    // Create a new vertex buffer
    //
    if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(D3DQA_DEFAULT_VERTEXBUF_SIZE,// UINT Length
		                                        0,                           // DWORD Usage
		                                        D3DQA_DEFAULT_VERTEXBUF_FVF, // DWORD FVF
		                                        VertexBufPool,               // D3DMPOOL Pool
		                                       &pVertexBuf)))                // IDirect3DMobileVertexBuffer** ppVertexBuffer
		
	{
		DebugOut(DO_ERROR, L"CreateVertexBuffer failed. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	if (D3DMRTYPE_VERTEXBUFFER != pVertexBuf->GetType())
	{
		DebugOut(DO_ERROR, L"D3DMRESOURCETYPE mismatch; IDirect3DMobileVertexBuffer::GetType. Failing.");
		pVertexBuf->Release();
		return TPR_FAIL;
	}

	pVertexBuf->Release();
	
	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;

}
//
// ExecuteLockTest
//
//   Verify IDirect3DMobileVertexBuffer::Lock
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT VertexBufferTest::ExecuteLockTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteLockTest.");

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;
    
	//
	// Pool for vertex buffer creation
	//
	D3DMPOOL VertexBufPool;

	//
	// Iterators for lock requests
	//
	UINT uiLockOffset, uiLockSize;

    //
    // Interface pointer for vertex buffer to be created
    //
    IDirect3DMobileVertexBuffer *pVertexBuf;

	//
	// Pointer for locked vertex buffer data
	// 
	BYTE *pVertexBufData;

	//
	// Iterator for examining individual byte locations of locked buffers
	//
	UINT uiComparePosition;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Retrieve driver capabilities
	//
	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	//
	// Pick the pool that the driver supports, as reported in D3DMCAPS
	//
	if (D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_VIDEOMEM;
	}
	else if (D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_SYSTEMMEM;
	}
	else
	{
		DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSVERTEXBUFFER or D3DMSURFCAPS_VIDVERTEXBUFFER. Aborting.");
		return TPR_ABORT;
	}

    //
    // Create a new vertex buffer
    //
    if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(D3DQA_DEFAULT_VERTEXBUF_SIZE,// UINT Length
		                                        0,                           // DWORD Usage
		                                        D3DQA_DEFAULT_VERTEXBUF_FVF, // DWORD FVF
		                                        VertexBufPool,               // D3DMPOOL Pool
		                                       &pVertexBuf)))                // IDirect3DMobileVertexBuffer** ppVertexBuffer
		
	{
		DebugOut(DO_ERROR, L"CreateVertexBuffer failed. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}


	//
	// Attempt a lock operation on all possible ranges
	//
	// Note that locking is not allowed on positions that are not aligned to the data type of
	// the vertex buffer.
	//
	for(uiLockOffset = 0; uiLockOffset < D3DQA_DEFAULT_VERTEXBUF_SIZE; uiLockOffset+=D3DQA_DEFAULT_VERTEXBUF_FVF_VERTSIZE)
	{
		for(uiLockSize = D3DQA_DEFAULT_VERTEXBUF_FVF_VERTSIZE; uiLockSize < (D3DQA_DEFAULT_VERTEXBUF_SIZE-uiLockOffset); uiLockSize+=D3DQA_DEFAULT_VERTEXBUF_FVF_VERTSIZE)
		{

			//
			// Lock the range specified by this iteration
			//
			if (FAILED(hr = pVertexBuf->Lock(uiLockOffset,   // UINT OffsetToLock
										uiLockSize,		// UINT SizeToLock
									    (VOID**)&pVertexBufData, // VOID** ppbData
										0)))            // DWORD Flags
			{
				DebugOut(DO_ERROR, L"Lock failed. (hr = 0x%08x). Failing.", hr);
				pVertexBuf->Release();
				return TPR_FAIL;
			}

			//
			// Set the contents of all bytes in this range to some known value
			//
			memset(pVertexBufData, (UCHAR)uiLockSize, uiLockSize);

			//
			// Unlock
			//
			if (FAILED(hr = pVertexBuf->Unlock()))
			{
				DebugOut(DO_ERROR, L"Fatal error; IDirect3DMobileVertexBuffer::Unlock failed (hr = 0x%08x). Failing.", hr);
				pVertexBuf->Release();
				return TPR_FAIL;
			}


			//
			// Lock the range specified by this iteration
			//
			if (FAILED(hr = pVertexBuf->Lock(uiLockOffset,   // UINT OffsetToLock
										uiLockSize,		// UINT SizeToLock
									    (VOID**)&pVertexBufData, // VOID** ppbData
										0)))            // DWORD Flags
			{
				DebugOut(DO_ERROR, L"Lock failed. (hr = 0x%08x). Failing.", hr);
				pVertexBuf->Release();
				return TPR_FAIL;
			}

			//
			// Verify that the vertex buffer contents are as-specified earlier in this
			// iteration
			//
			for(uiComparePosition = 0; uiComparePosition < uiLockSize; uiComparePosition++)
			{
				if ((UCHAR)uiLockSize != pVertexBufData[uiComparePosition])
				{
					DebugOut(DO_ERROR, L"Vertex buffer lost the data that was specified during a previous lock. Failing.");
					pVertexBuf->Release();
					return TPR_FAIL;
				}
			}

			//
			// Unlock
			//
			if (FAILED(hr = pVertexBuf->Unlock()))
			{
				DebugOut(DO_ERROR, L"Fatal error; IDirect3DMobileVertexBuffer::Unlock failed (hr = 0x%08x). Failing.", hr);
				pVertexBuf->Release();
				return TPR_FAIL;
			}
		}
	}
	
	//
	// Bad parameter test #1: Invalid double pointer
	//
	if (SUCCEEDED(hr = pVertexBuf->Lock(0,                            // UINT OffsetToLock
								   D3DQA_DEFAULT_VERTEXBUF_SIZE, // UINT SizeToLock
								   NULL,                         // VOID** ppbData
								   0)))                          // DWORD Flags
	{
		DebugOut(DO_ERROR, L"Lock succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
		pVertexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Bad parameter test #2: Offset outside of buffer
	//
	if (SUCCEEDED(hr = pVertexBuf->Lock(D3DQA_DEFAULT_VERTEXBUF_SIZE*2, // UINT OffsetToLock
								   D3DQA_DEFAULT_VERTEXBUF_SIZE,   // UINT SizeToLock
							       (VOID**)&pVertexBufData,        // VOID** ppbData
								   0)))                            // DWORD Flags
	{
		DebugOut(DO_ERROR, L"Lock succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
		pVertexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Bad parameter test #3: Lock range exceeds buffer extent
	//
	if (SUCCEEDED(hr = pVertexBuf->Lock(0,                              // UINT OffsetToLock
								   D3DQA_DEFAULT_VERTEXBUF_SIZE*2, // UINT SizeToLock
							       (VOID**)&pVertexBufData,        // VOID** ppbData
								   0)))                            // DWORD Flags
	{
		DebugOut(DO_ERROR, L"Lock succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
		pVertexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Bad parameter test #4: Invalid flags
	//
	if (SUCCEEDED(hr = pVertexBuf->Lock(0,                            // UINT OffsetToLock
								   D3DQA_DEFAULT_VERTEXBUF_SIZE, // UINT SizeToLock
								   (VOID**)&pVertexBufData,      // VOID** ppbData
								   0xFFFFFFFF)))                 // DWORD Flags
	{
		DebugOut(DO_ERROR, L"Lock succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
		pVertexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Bad parameter test #5: Flag that does not apply to this surface type
	//
	if (SUCCEEDED(hr = pVertexBuf->Lock(0,                            // UINT OffsetToLock
								   D3DQA_DEFAULT_VERTEXBUF_SIZE, // UINT SizeToLock
								   (VOID**)&pVertexBufData,      // VOID** ppbData
								   D3DMLOCK_NO_DIRTY_UPDATE)))   // DWORD Flags
	{
		DebugOut(DO_ERROR, L"Lock succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
		pVertexBuf->Release();
		return TPR_FAIL;
	}


	//
	// Bad parameter test #6: Invalid flag combo
	//
	if (SUCCEEDED(hr = pVertexBuf->Lock(0,                                      // UINT OffsetToLock
								   D3DQA_DEFAULT_VERTEXBUF_SIZE,           // UINT SizeToLock
								   (VOID**)&pVertexBufData,                // VOID** ppbData
								   D3DMLOCK_READONLY | D3DMLOCK_DISCARD))) // DWORD Flags
	{
		DebugOut(DO_ERROR, L"Lock succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
		pVertexBuf->Release();
		return TPR_FAIL;
	}


	//
	// Bad parameter test #7: D3DMLOCK_DISCARD invalid without D3DMUSAGE_DYNAMIC
	//
	if (SUCCEEDED(hr = pVertexBuf->Lock(0,                            // UINT OffsetToLock
								   D3DQA_DEFAULT_VERTEXBUF_SIZE, // UINT SizeToLock
								   (VOID**)&pVertexBufData,      // VOID** ppbData
								   D3DMLOCK_DISCARD)))           // DWORD Flags
	{
		DebugOut(DO_ERROR, L"Lock succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
		pVertexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Bad parameter test #8: D3DMLOCK_NOOVERWRITE invalid without D3DMUSAGE_DYNAMIC
	//
	if (SUCCEEDED(hr = pVertexBuf->Lock(0,                            // UINT OffsetToLock
								   D3DQA_DEFAULT_VERTEXBUF_SIZE, // UINT SizeToLock
								   (VOID**)&pVertexBufData,      // VOID** ppbData
								   D3DMLOCK_NOOVERWRITE)))       // DWORD Flags
	{
		DebugOut(DO_ERROR, L"Lock succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
		pVertexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Bad parameter test #9: Lock on non-FVF boundary
	//
	if (SUCCEEDED(hr = pVertexBuf->Lock(1,                                    // UINT OffsetToLock
								   D3DQA_DEFAULT_VERTEXBUF_FVF_VERTSIZE, // UINT SizeToLock
								   (VOID**)&pVertexBufData,              // VOID** ppbData
								   0)))                                  // DWORD Flags
	{
		//
		// Middleware allows call to go through, but warns in debug output
		//
		pVertexBuf->Unlock();
	}

	pVertexBuf->Release();
	
	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}
//
// ExecuteUnlockTest
//
//   Verify IDirect3DMobileVertexBuffer::Unlock
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT VertexBufferTest::ExecuteUnlockTest()
{
	DebugOut(L"Beginning ExecuteUnlockTest.");

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

    //
    // Unlock testing is in combination with Lock testing; redirect this test
	// case accordingly
    //
	return ExecuteLockTest();
}
//
// ExecuteGetDescTest
//
//   Verify IDirect3DMobileVertexBuffer::GetDesc
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT VertexBufferTest::ExecuteGetDescTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteGetDescTest.");

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;
    
	//
	// Iterator for attempting to create vertex buffers of various sizes
	//
	UINT uiVBSize;

	//
	// Pool for vertex buffer creation
	//
	D3DMPOOL VertexBufPool;

    //
    // Interface pointer for vertex buffer to be created
    //
    IDirect3DMobileVertexBuffer *pVertexBuf;

	//
	// Description of Vertex Buffer
	//
	D3DMVERTEXBUFFER_DESC VertexBufDesc;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Retrieve driver capabilities
	//
	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	//
	// Pick the pool that the driver supports, as reported in D3DMCAPS
	//
	if (D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_VIDEOMEM;
	}
	else if (D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_SYSTEMMEM;
	}
	else
	{
		DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSVERTEXBUFFER or D3DMSURFCAPS_VIDVERTEXBUFFER. Aborting.");
		return TPR_ABORT;
	}


	//
	// Iterate through a range of sizes, all valid for the specified FVF
	//
	for(uiVBSize = D3DQA_DEFAULT_VERTEXBUF_FVF_VERTSIZE; uiVBSize < D3DQA_MAX_VERTEXBUF_SIZE; uiVBSize+=D3DQA_DEFAULT_VERTEXBUF_FVF_VERTSIZE)
	{
		//
		// Create a new vertex buffer
		//
		if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(uiVBSize,                    // UINT Length
													0,                           // DWORD Usage
													D3DQA_DEFAULT_VERTEXBUF_FVF, // DWORD FVF
													VertexBufPool,               // D3DMPOOL Pool
												   &pVertexBuf)))                // IDirect3DMobileVertexBuffer** ppVertexBuffer
			
		{
			DebugOut(DO_ERROR, L"CreateVertexBuffer failed. (hr = 0x%08x). Failing.", hr);
			return TPR_FAIL;
		}

		//	
		// Invalid parameter test
		//	
		if (SUCCEEDED(hr = pVertexBuf->GetDesc(NULL))) // D3DMVERTEXBUFFER_DESC *pDesc
		{
			DebugOut(DO_ERROR, L"GetDesc succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
			pVertexBuf->Release();
			return TPR_FAIL;
		}

		//	
		// Retrieve the buffer description
		//	
		if (FAILED(hr = pVertexBuf->GetDesc(&VertexBufDesc))) // D3DMVERTEXBUFFER_DESC *pDesc
		{
			DebugOut(DO_ERROR, L"GetDesc failed. (hr = 0x%08x). Failing.", hr);
			pVertexBuf->Release();
			return TPR_FAIL;
		}

		//
		// For testing purposes, this vertex buffer interface pointer is not longer needed.
		// The D3DMVERTEXBUFFER_DESC can be sufficiently scrutinized without further access
		// to the interface itself.
		//
		pVertexBuf->Release();

		//
		// Does the format match the expectation?
		//
		if (D3DMFMT_VERTEXDATA != VertexBufDesc.Format)
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMVERTEXBUFFER_DESC.Format. Failing.");
			return TPR_FAIL;
		}

		//
		// Does the resource type match the expectation?
		//
		if (D3DMRTYPE_VERTEXBUFFER != VertexBufDesc.Type)
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMVERTEXBUFFER_DESC.Type. Failing.");
			return TPR_FAIL;
		}

		//
		// Does the usage mask match the expectation?
		//
		if (D3DMUSAGE_LOCKABLE != VertexBufDesc.Usage)
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMVERTEXBUFFER_DESC.Usage. Failing.");
			return TPR_FAIL;
		}

		//
		// Does the pool type match the expectation?
		//
		if (VertexBufPool != VertexBufDesc.Pool)
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMVERTEXBUFFER_DESC.Pool. Failing.");
			return TPR_FAIL;
		}

		//
		// Does the buffer size (bytes) match the expectation?
		//
		if (uiVBSize != VertexBufDesc.Size)
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMVERTEXBUFFER_DESC.Size. Failing.");
			return TPR_FAIL;
		}
	
		//
		// Does the FVF match the expectation?
		//
		if (D3DQA_DEFAULT_VERTEXBUF_FVF != VertexBufDesc.FVF)
		{
			DebugOut(DO_ERROR, L"Unexpected value in D3DMVERTEXBUFFER_DESC.FVF. Failing.");
			return TPR_FAIL;
		}
	}

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;

}

