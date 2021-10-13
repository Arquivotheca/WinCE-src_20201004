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
#include "IDirect3DMobileIndexBuffer.h"
#include "DebugOutput.h"
#include "KatoUtils.h"
#include "BufferTools.h"
#include <tux.h>
#include <tchar.h>
#include <stdio.h>

//
// For testing that does not require a particular index buffer size,
// use a reasonable default
//
#define D3DQA_DEFAULT_INDEXBUF_SIZE 100

//
// Default index buffer size for IDirect3DMobileIndexBuffer::Lock test
//
#define D3DQA_IBLOCKTEST_DEFAULT_IB_SIZE 50

//
// Maximum size to attempt for testing index buffer creation
//
#define D3DQA_MAX_INDEXBUF_SIZE 400

//
// Maximum priority level tested for Set/GetPriority
//
#define D3DQA_MAX_IB_PRIORITY 512

//
// GUID for invalid parameter tests
//
DEFINE_GUID(IID_D3DQAInvalidInterface, 
0x00000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);


//
//  IndexBufferTest
//
//    Constructor for IndexBufferTest class; however, most initialization is deferred.
//
//  Arguments:
//
//    none
//
//  Return Value:
//
//    none
//
IndexBufferTest::IndexBufferTest()
{
	m_bInitSuccess = FALSE;
}


//
// IndexBufferTest
//
//   Initialize IndexBufferTest object
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
HRESULT IndexBufferTest::Init(LPTEST_CASE_ARGS pTestCaseArgs, LPWINDOW_ARGS pWindowArgs, BOOL bDebug, UINT uiTestCase)
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
//    Accessor method for "initialization state" of IndexBufferTest object.  "Half
//    initialized" states should not occur.
//  
//  Return Value:
//  
//    BOOL:  TRUE if the object is initialized; FALSE if it is not.
//
BOOL IndexBufferTest::IsReady()
{
	if (FALSE == m_bInitSuccess)
	{
		DebugOut(DO_ERROR, L"IndexBufferTest is not ready.");
		return FALSE;
	}

	DebugOut(L"IndexBufferTest is ready.");
	return D3DMInitializer::IsReady();
}

// 
// ~IndexBufferTest
//
//  Destructor for IndexBufferTest.  Currently; there is nothing
//  to do here.
//
IndexBufferTest::~IndexBufferTest()
{
	return; // Success
}



//
// ExecuteQueryInterfaceTest
//
//   Verify IDirect3DMobileIndexBuffer::QueryInterface
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IndexBufferTest::ExecuteQueryInterfaceTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteQueryInterfaceTest.");

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	//
	// Interface pointer for index buffer to be created
	//
	IDirect3DMobileIndexBuffer *pIndexBuf;

	//
	// Interface pointers to be retrieved by QueryInterface
	//
	IDirect3DMobileResource *pQIResource;
	IDirect3DMobileIndexBuffer *pQIIndexBuf;

	//
	// Pool for index buffer creation
	//
	D3DMPOOL IndexBufPool;

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
	if (D3DMSURFCAPS_VIDINDEXBUFFER & Caps.SurfaceCaps)
	{
		IndexBufPool = D3DMPOOL_VIDEOMEM;
	}
	else if (D3DMSURFCAPS_SYSINDEXBUFFER & Caps.SurfaceCaps)
	{
		IndexBufPool = D3DMPOOL_SYSTEMMEM;
	}
	else
	{
		DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSINDEXBUFFER or D3DMSURFCAPS_VIDINDEXBUFFER. Aborting.");
		return TPR_ABORT;
	}

	//
	// Create a new index buffer
	//
	if (FAILED(hr = m_pd3dDevice->CreateIndexBuffer(D3DQA_DEFAULT_INDEXBUF_SIZE, // UINT Length
	                                           0,                           // DWORD Usage
	                                           D3DMFMT_INDEX16,             // D3DMFORMAT Format
	                                           IndexBufPool,                // D3DMPOOL Pool
	                                          &pIndexBuf)))                 // IDirect3DMobileIndexBuffer** ppIndexBuffer
	{
		DebugOut(DO_ERROR, L"CreateIndexBuffer failed. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}


	// 	
	// Bad parameter tests
	// 	

	//
	// Test case #1:  Invalid REFIID
	//
	if (SUCCEEDED(hr = pIndexBuf->QueryInterface(IID_D3DQAInvalidInterface, // REFIID riid
	                                        (void**)(&pQIIndexBuf))))  // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
		pIndexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Test case #2:  Valid REFIID, invalid PPVOID
	//
	if (SUCCEEDED(hr = pIndexBuf->QueryInterface(IID_IDirect3DMobileIndexBuffer, // REFIID riid
	                                        (void**)0)))                    // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
		pIndexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Test case #3:  Invalid REFIID, invalid PPVOID
	//
	if (SUCCEEDED(hr = pIndexBuf->QueryInterface(IID_D3DQAInvalidInterface, // REFIID riid
	                                        (void**)0)))               // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
		pIndexBuf->Release();
		return TPR_FAIL;
	}


	//
	// Valid parameter tests
	//

	// 	
	// Perform an interface query on the interface buffer
	// 	
	if (FAILED(hr = pIndexBuf->QueryInterface(IID_IDirect3DMobileResource, // REFIID riid
	                                     (void**)(&pQIResource))))    // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface failed. (hr = 0x%08x). Failing.", hr);
		pIndexBuf->Release();
		return TPR_FAIL;
	}

	// 	
	// Perform an interface query on the interface buffer
	// 	
	if (FAILED(hr = pIndexBuf->QueryInterface(IID_IDirect3DMobileIndexBuffer, // REFIID riid
	                                     (void**)(&pQIIndexBuf))))       // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface failed. (hr = 0x%08x). Failing.", hr);
		pQIResource->Release();
		pIndexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Release the original interface pointer; verify that the ref count is still non-zero
	//
	if (!(pIndexBuf->Release()))
	{
		DebugOut(DO_ERROR, L"Reference count for IDirect3DMobileIndexBuffer dropped to zero; unexpected. Failing.");
		return TPR_FAIL;
	}

	//
	// Release the IDirect3DMobileIndexBuffer interface pointer; verify that the ref count is still non-zero
	//
	if (!(pQIIndexBuf->Release()))
	{
		DebugOut(DO_ERROR, L"Reference count for IDirect3DMobileIndexBuffer dropped to zero; unexpected. Failing.");
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
//   Verify IDirect3DMobileIndexBuffer::AddRef
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IndexBufferTest::ExecuteAddRefTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteAddRefTest.");

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	//
	// Interface pointer for IB
	//
	IDirect3DMobileIndexBuffer *pIndexBuf;

	//
	// Pool for index buffer creation
	//
	D3DMPOOL IndexBufPool;

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
	// Pick the index buffer pool that the driver supports, as reported in D3DMCAPS
	//
	if (D3DMSURFCAPS_VIDINDEXBUFFER & Caps.SurfaceCaps)
	{
		IndexBufPool = D3DMPOOL_VIDEOMEM;
	}
	else if (D3DMSURFCAPS_SYSINDEXBUFFER & Caps.SurfaceCaps)
	{
		IndexBufPool = D3DMPOOL_SYSTEMMEM;
	}
	else
	{
		DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSINDEXBUFFER or D3DMSURFCAPS_VIDINDEXBUFFER. Aborting.");
		return TPR_ABORT;
	}

	//
	// Create a new index buffer; the smallest IB allowable
	//
	if (FAILED(hr = m_pd3dDevice->CreateIndexBuffer(2,               // UINT Length
	                                           0,               // DWORD Usage
	                                           D3DMFMT_INDEX16, // D3DMFORMAT Format
	                                           IndexBufPool,    // D3DMPOOL Pool
	                                          &pIndexBuf)))     // IDirect3DMobileIndexBuffer** ppIndexBuffer
	{
		DebugOut(DO_ERROR, L"CreateIndexBuffer failed. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}
	
	//
	// Verify ref-count values; via AddRef/Release return values
	//
	if (2 != pIndexBuf->AddRef())
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileIndexBuffer: unexpected reference count. Failing.");
		return TPR_FAIL;
	}
	
	if (1 != pIndexBuf->Release())
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileIndexBuffer: unexpected reference count. Failing.");
		return TPR_FAIL;
	}
	
	if (0 != pIndexBuf->Release())
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileIndexBuffer: unexpected reference count. Failing.");
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
//   Verify IDirect3DMobileIndexBuffer::Release
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IndexBufferTest::ExecuteReleaseTest()
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
//   Verify IDirect3DMobileIndexBuffer::GetDevice
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IndexBufferTest::ExecuteGetDeviceTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteGetDeviceTest.");

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	//
	// Pool for index buffer creation
	//
	D3DMPOOL IndexBufPool;

	//
	// Interface pointer for index buffer to be created
	//
	IDirect3DMobileIndexBuffer *pIndexBuf;

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
	// Pick the index buffer pool that the driver supports, as reported in D3DMCAPS
	//
	if (D3DMSURFCAPS_VIDINDEXBUFFER & Caps.SurfaceCaps)
	{
		IndexBufPool = D3DMPOOL_VIDEOMEM;
	}
	else if (D3DMSURFCAPS_SYSINDEXBUFFER & Caps.SurfaceCaps)
	{
		IndexBufPool = D3DMPOOL_SYSTEMMEM;
	}
	else
	{
		DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSINDEXBUFFER or D3DMSURFCAPS_VIDINDEXBUFFER. Aborting.");
		return TPR_ABORT;
	}

	//
	// Create a new index buffer; the smallest IB allowable
	//
	if (FAILED(hr = m_pd3dDevice->CreateIndexBuffer(2,               // UINT Length
	                                           0,               // DWORD Usage
	                                           D3DMFMT_INDEX16, // D3DMFORMAT Format
	                                           IndexBufPool,    // D3DMPOOL Pool
	                                          &pIndexBuf)))     // IDirect3DMobileIndexBuffer** ppIndexBuffer
	{
		DebugOut(DO_ERROR, L"CreateIndexBuffer failed. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	//
	// Retrieve the device interface pointer
	//
	if (FAILED(hr = pIndexBuf->GetDevice(&pDevice))) // IDirect3DMobileDevice** ppDevice
	{
		DebugOut(DO_ERROR, L"GetDevice failed. (hr = 0x%08x). Failing.", hr);
		pIndexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Verify that the device interface pointer is consistent with expectations
	//
	if (pDevice != m_pd3dDevice)
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice interface pointer mismatch. Failing.");
		pIndexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Release the device interface reference (incremented by GetDevice)
	//
	pDevice->Release();

	//
	// Release the index buffer
	//
	pIndexBuf->Release();

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteSetPriorityTest
//
//   Verify IDirect3DMobileIndexBuffer::SetPriority
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IndexBufferTest::ExecuteSetPriorityTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteSetPriorityTest.");

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	//
	// Interface pointer for index buffer to be created
	//
	IDirect3DMobileIndexBuffer *pIndexBuf;

	//
	// Pool for index buffer creation
	//
	D3DMPOOL IndexBufPool;

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
		// If D3DMSURFCAPS_MANAGEDPOOL, skip main test, but still call function to verify that
		// it executes to completion in the non-managed-pool case.
		//

		//
		// Pick the pool that the driver supports, as reported in D3DMCAPS
		//
		if (D3DMSURFCAPS_VIDINDEXBUFFER & Caps.SurfaceCaps)
		{
			IndexBufPool = D3DMPOOL_VIDEOMEM;
		}
		else if (D3DMSURFCAPS_SYSINDEXBUFFER & Caps.SurfaceCaps)
		{
			IndexBufPool = D3DMPOOL_SYSTEMMEM;
		}
		else
		{
			DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSINDEXBUFFER or D3DMSURFCAPS_VIDINDEXBUFFER. Aborting.");
			return TPR_ABORT;
		}

		//
		// Create a new index buffer; the smallest IB allowable
		//
		if (FAILED(hr = m_pd3dDevice->CreateIndexBuffer(2,               // UINT Length
		                                           0,               // DWORD Usage
		                                           D3DMFMT_INDEX16, // D3DMFORMAT Format
		                                           IndexBufPool,    // D3DMPOOL Pool
		                                          &pIndexBuf)))     // IDirect3DMobileIndexBuffer** ppIndexBuffer
		{
			DebugOut(DO_ERROR, L"CreateIndexBuffer failed. (hr = 0x%08x). Failing.", hr);
			return TPR_FAIL;
		}

		pIndexBuf->SetPriority(1);
		pIndexBuf->GetPriority();

		pIndexBuf->Release();

		DebugOut(DO_ERROR, L"SetPriority:  Skipping test because driver does not expose D3DMSURFCAPS_MANAGEDPOOL. Skipping.");
		return TPR_SKIP;
	}


	//
	// Create a new index buffer; the smallest IB allowable
	//
	if (FAILED(hr = m_pd3dDevice->CreateIndexBuffer(2,               // UINT Length
	                                           0,               // DWORD Usage
	                                           D3DMFMT_INDEX16, // D3DMFORMAT Format
	                                           D3DMPOOL_MANAGED,// D3DMPOOL Pool
	                                          &pIndexBuf)))     // IDirect3DMobileIndexBuffer** ppIndexBuffer
	{
		DebugOut(DO_ERROR, L"CreateIndexBuffer failed. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}
	
	// 
	// Verify default priority of zero; arg is arbitrary (as long as it is non-zero).
	// This caller simply verifies that SetPriority returns the previous priority, and
	// that the previous priority defaults to zero if it has never been explicitely set.
	// 
	if (0 != pIndexBuf->SetPriority(1))
	{
		DebugOut(DO_ERROR, L"SetPriority failed. Failing.");
		pIndexBuf->Release();
		return TPR_FAIL;
	}


	//
	// Iterate through a large set of priorities, and verify that SetPriority always
	// returns the previously-set priority
	//
	pIndexBuf->SetPriority(0);
	for(uiPriority = 1; uiPriority < D3DQA_MAX_IB_PRIORITY; uiPriority++)
	{
		if ((uiPriority-1) != pIndexBuf->SetPriority(uiPriority))
		{
			DebugOut(DO_ERROR, L"SetPriority failed. Failing.");
			pIndexBuf->Release();
			return TPR_FAIL;
		}

		if (uiPriority != pIndexBuf->GetPriority())
		{
			DebugOut(DO_ERROR, L"GetPriority priority mismatch. Failing.");
			pIndexBuf->Release();
			return TPR_FAIL;
		}
	
	}

	pIndexBuf->Release();
	
	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteGetPriorityTest
//
//   Verify IDirect3DMobileIndexBuffer::GetPriority
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IndexBufferTest::ExecuteGetPriorityTest()
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
//   Call IDirect3DMobileIndexBuffer::PreLoad.  This API is essentially an optimization
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
INT IndexBufferTest::ExecutePreLoadTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecutePreLoadTest.");

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	//
	// Pool for index buffer creation
	//
	D3DMPOOL IndexBufPool;

	//
	// Interface pointer for index buffer to be created
	//
	IDirect3DMobileIndexBuffer *pIndexBuf;

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
		// If D3DMSURFCAPS_MANAGEDPOOL, skip main test, but still call function to verify that
		// it executes to completion in the non-managed-pool case.
		//

		//
		// Pick the pool that the driver supports, as reported in D3DMCAPS
		//
		if (D3DMSURFCAPS_VIDINDEXBUFFER & Caps.SurfaceCaps)
		{
			IndexBufPool = D3DMPOOL_VIDEOMEM;
		}
		else if (D3DMSURFCAPS_SYSINDEXBUFFER & Caps.SurfaceCaps)
		{
			IndexBufPool = D3DMPOOL_SYSTEMMEM;
		}
		else
		{
			DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSINDEXBUFFER or D3DMSURFCAPS_VIDINDEXBUFFER. Aborting.");
			return TPR_ABORT;
		}

		//
		// Create a new index buffer; the smallest IB allowable
		//
		if (FAILED(hr = m_pd3dDevice->CreateIndexBuffer(2,               // UINT Length
		                                           0,               // DWORD Usage
		                                           D3DMFMT_INDEX16, // D3DMFORMAT Format
		                                           IndexBufPool,    // D3DMPOOL Pool
		                                          &pIndexBuf)))     // IDirect3DMobileIndexBuffer** ppIndexBuffer
		{
			DebugOut(DO_ERROR, L"CreateIndexBuffer failed. (hr = 0x%08x). Failing.", hr);
			return TPR_FAIL;
		}

		pIndexBuf->PreLoad();

		pIndexBuf->Release();

		DebugOut(DO_ERROR, L"PreLoad:  Skipping test because driver does not expose D3DMSURFCAPS_MANAGEDPOOL. Skipping.");
		return TPR_SKIP;
	}

	//
	// Create a new index buffer; the smallest IB allowable
	//
	if (FAILED(hr = m_pd3dDevice->CreateIndexBuffer(2,               // UINT Length
	                                           0,               // DWORD Usage
	                                           D3DMFMT_INDEX16, // D3DMFORMAT Format
	                                           D3DMPOOL_MANAGED,// D3DMPOOL Pool
	                                          &pIndexBuf)))     // IDirect3DMobileIndexBuffer** ppIndexBuffer
	{
		DebugOut(DO_ERROR, L"CreateIndexBuffer failed. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	pIndexBuf->PreLoad();

	pIndexBuf->Release();
	
	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteGetTypeTest
//
//   Verify IDirect3DMobileIndexBuffer::GetType
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IndexBufferTest::ExecuteGetTypeTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteGetTypeTest.");

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;
    
	//
	// Pool for index buffer creation
	//
	D3DMPOOL IndexBufPool;

	//
	// Interface pointer for index buffer to be created
	//
	IDirect3DMobileIndexBuffer *pIndexBuf;

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
	// Pick the index buffer pool that the driver supports, as reported in D3DMCAPS
	//
	if (D3DMSURFCAPS_VIDINDEXBUFFER & Caps.SurfaceCaps)
	{
		IndexBufPool = D3DMPOOL_VIDEOMEM;
	}
	else if (D3DMSURFCAPS_SYSINDEXBUFFER & Caps.SurfaceCaps)
	{
		IndexBufPool = D3DMPOOL_SYSTEMMEM;
	}
	else
	{
		DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSINDEXBUFFER or D3DMSURFCAPS_VIDINDEXBUFFER. Aborting.");
		return TPR_ABORT;
	}

	//
	// Create a new index buffer; the smallest IB allowable
	//
	if (FAILED(hr = m_pd3dDevice->CreateIndexBuffer(2,               // UINT Length
	                                           0,               // DWORD Usage
	                                           D3DMFMT_INDEX16, // D3DMFORMAT Format
	                                           IndexBufPool,    // D3DMPOOL Pool
	                                          &pIndexBuf)))     // IDirect3DMobileIndexBuffer** ppIndexBuffer
	{
		DebugOut(DO_ERROR, L"CreateIndexBuffer failed. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	if (D3DMRTYPE_INDEXBUFFER != pIndexBuf->GetType())
	{
		DebugOut(DO_ERROR, L"D3DMRESOURCETYPE mismatch; IDirect3DMobileIndexBuffer::GetType. Failing.");
		pIndexBuf->Release();
		return TPR_FAIL;
	}

	pIndexBuf->Release();
	
	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteLockTest
//
//   Verify IDirect3DMobileIndexBuffer::Lock
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IndexBufferTest::ExecuteLockTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteLockTest.");

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;
    
	//
	// Pool for index buffer creation
	//
	D3DMPOOL IndexBufPool;

	//
	// Iterators for lock requests
	//
	UINT uiLockOffset, uiLockSize;

	//
	// Pointer for locked index buffer data
	// 
	BYTE *pIndexBufData;

	//
	// Iterator for examining individual byte locations of locked buffers
	//
	UINT uiComparePosition;

	//
	// Interface pointer for index buffer to be created
	//
	IDirect3DMobileIndexBuffer *pIndexBuf;

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
	// Pick the index buffer pool that the driver supports, as reported in D3DMCAPS
	//
	if (D3DMSURFCAPS_VIDINDEXBUFFER & Caps.SurfaceCaps)
	{
		IndexBufPool = D3DMPOOL_VIDEOMEM;
	}
	else if (D3DMSURFCAPS_SYSINDEXBUFFER & Caps.SurfaceCaps)
	{
		IndexBufPool = D3DMPOOL_SYSTEMMEM;
	}
	else
	{
		DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSINDEXBUFFER or D3DMSURFCAPS_VIDINDEXBUFFER. Aborting.");
		return TPR_ABORT;
	}

	//
	// Create a new index buffer
	//
	if (FAILED(hr = m_pd3dDevice->CreateIndexBuffer(D3DQA_IBLOCKTEST_DEFAULT_IB_SIZE, // UINT Length
	                                           0,                                // DWORD Usage
	                                           D3DMFMT_INDEX16,                  // D3DMFORMAT Format
	                                           IndexBufPool,                     // D3DMPOOL Pool
	                                          &pIndexBuf)))                      // IDirect3DMobileIndexBuffer** ppIndexBuffer
	{
		DebugOut(DO_ERROR, L"CreateIndexBuffer failed. (hr = 0x%08x). Failing.", hr);
		return TPR_FAIL;
	}

	//
	// Attempt a lock operation on all possible ranges
	//
	// Note that locking is not allowed on positions that are not aligned to the data type of
	// the index buffer.
	//
	for(uiLockOffset = 0; uiLockOffset < D3DQA_IBLOCKTEST_DEFAULT_IB_SIZE; uiLockOffset+=2)
	{
		for(uiLockSize = 2; uiLockSize < (D3DQA_IBLOCKTEST_DEFAULT_IB_SIZE-uiLockOffset); uiLockSize+=2)
		{

			//
			// Lock the range specified by this iteration
			//
			if (FAILED(hr = pIndexBuf->Lock( uiLockOffset,   // UINT OffsetToLock
			                            uiLockSize,	    // UINT SizeToLock
			                            (VOID**)&pIndexBufData,  // BYTE** ppbData
			                            0)))            // DWORD Flags
			{
				DebugOut(DO_ERROR, L"Lock failed. (hr = 0x%08x). Failing.", hr);
				pIndexBuf->Release();
				return TPR_FAIL;
			}

			//
			// Set the contents of all bytes in this range to some known value
			//
			memset(pIndexBufData, (UCHAR)uiLockSize, uiLockSize);

			//
			// Unlock
			//
			if (FAILED(hr = pIndexBuf->Unlock()))
			{
				DebugOut(DO_ERROR, L"Fatal error; IDirect3DMobileIndexBuffer::Unlock failed (hr = 0x%08x). Failing.", hr);
				pIndexBuf->Release();
				return TPR_FAIL;
			}


			//
			// Lock the range specified by this iteration
			//
			if (FAILED(hr = pIndexBuf->Lock( uiLockOffset,   // UINT OffsetToLock
			                            uiLockSize,	    // UINT SizeToLock
			                            (VOID**)&pIndexBufData,  // BYTE** ppbData
			                            0)))            // DWORD Flags
			{
				DebugOut(DO_ERROR, L"Lock failed. (hr = 0x%08x). Failing.", hr);
				pIndexBuf->Release();
				return TPR_FAIL;
			}

			//
			// Verify that the index buffer contents are as-specified earlier in this
			// iteration
			//
			for(uiComparePosition = 0; uiComparePosition < uiLockSize; uiComparePosition++)
			{
				if ((UCHAR)uiLockSize != pIndexBufData[uiComparePosition])
				{
					DebugOut(DO_ERROR, L"Index buffer lost the data that was specified during a previous lock. Failing.");
					pIndexBuf->Release();
					return TPR_FAIL;
				}
			}

			//
			// Unlock
			//
			if (FAILED(hr = pIndexBuf->Unlock()))
			{
				DebugOut(DO_ERROR, L"Fatal error; IDirect3DMobileIndexBuffer::Unlock failed (hr = 0x%08x). Failing.", hr);
				pIndexBuf->Release();
				return TPR_FAIL;
			}

		}
	}

	//
	// Bad parameter test #1:  invalid double pointer
	//
	if (SUCCEEDED(hr = pIndexBuf->Lock(0,              // UINT OffsetToLock
	                              2,              // UINT SizeToLock
	                              NULL,           // BYTE** ppbData
	                              0)))            // DWORD Flags
	{
		DebugOut(DO_ERROR, L"Lock succeeded unexpectedly (bad parameter test #1). (hr = 0x%08x). Failing.", hr);
		pIndexBuf->Unlock();
		pIndexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Bad parameter test #2:  lock larger than buffer
	//
	if (SUCCEEDED(hr = pIndexBuf->Lock(0,                                  // UINT OffsetToLock
	                              D3DQA_IBLOCKTEST_DEFAULT_IB_SIZE*2, // UINT SizeToLock
	                              (VOID**)&pIndexBufData,                     // BYTE** ppbData
	                              0)))                                // DWORD Flags
	{
		DebugOut(DO_ERROR, L"Lock succeeded unexpectedly (bad parameter test #2). (hr = 0x%08x). Failing.", hr);
		pIndexBuf->Unlock();
		pIndexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Bad parameter test #3:  lock beyond buffer
	//
	if (SUCCEEDED(hr = pIndexBuf->Lock(D3DQA_IBLOCKTEST_DEFAULT_IB_SIZE*2, // UINT OffsetToLock
	                              D3DQA_IBLOCKTEST_DEFAULT_IB_SIZE,   // UINT SizeToLock
	                              (VOID**)&pIndexBufData,                     // BYTE** ppbData
	                              0)))                                // DWORD Flags
	{
		DebugOut(DO_ERROR, L"Lock succeeded unexpectedly (bad parameter test #3). (hr = 0x%08x). Failing.", hr);
		pIndexBuf->Unlock();
		pIndexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Bad parameter test #4:  invalid flags
	//
	if (SUCCEEDED(hr = pIndexBuf->Lock(0,              // UINT OffsetToLock
	                              2,              // UINT SizeToLock
	                              (VOID**)&pIndexBufData, // BYTE** ppbData
	                              0xFFFFFFFF)))   // DWORD Flags
	{
		DebugOut(DO_ERROR, L"Lock succeeded unexpectedly (bad parameter test #4). (hr = 0x%08x). Failing.", hr);
		pIndexBuf->Unlock();
		pIndexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Bad parameter test #5:  flag that is invalid for this surface type
	//
	if (SUCCEEDED(hr = pIndexBuf->Lock(0,                          // UINT OffsetToLock
	                              2,                          // UINT SizeToLock
	                              (VOID**)&pIndexBufData,             // BYTE** ppbData
	                              D3DMLOCK_NO_DIRTY_UPDATE))) // DWORD Flags
	{
		DebugOut(DO_ERROR, L"Lock succeeded unexpectedly (bad parameter test #5). (hr = 0x%08x). Failing.", hr);
		pIndexBuf->Unlock();
		pIndexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Bad parameter test #6:  combination of mutually exclusive flags
	//
	if (SUCCEEDED(hr = pIndexBuf->Lock(0,                                      // UINT OffsetToLock
	                              2,                                      // UINT SizeToLock
	                              (VOID**)&pIndexBufData,                         // BYTE** ppbData
	                              D3DMLOCK_READONLY | D3DMLOCK_DISCARD))) // DWORD Flags
	{
		DebugOut(DO_ERROR, L"Lock succeeded unexpectedly (bad parameter test #6). (hr = 0x%08x). Failing.", hr);
		pIndexBuf->Unlock();
		pIndexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Bad parameter test #7:  invalid flag circumstance (D3DMLOCK_DISCARD *requires* D3DMUSAGE_DYNAMIC)
	//
	if (SUCCEEDED(hr = pIndexBuf->Lock(0,                   // UINT OffsetToLock
	                              2,                   // UINT SizeToLock
	                              (VOID**)&pIndexBufData,      // BYTE** ppbData
	                              D3DMLOCK_DISCARD)))  // DWORD Flags
	{
		DebugOut(DO_ERROR, L"Lock succeeded unexpectedly (bad parameter test #7). (hr = 0x%08x). Failing.", hr);
		pIndexBuf->Unlock();
		pIndexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Bad parameter test #8:  invalid flag circumstance (D3DMLOCK_NOOVERWRITE *requires* D3DMUSAGE_DYNAMIC)
	//
	if (SUCCEEDED(hr = pIndexBuf->Lock(0,                      // UINT OffsetToLock
	                              2,                      // UINT SizeToLock
	                              (VOID**)&pIndexBufData,         // BYTE** ppbData
	                              D3DMLOCK_NOOVERWRITE))) // DWORD Flags
	{
		DebugOut(DO_ERROR, L"Lock succeeded unexpectedly (bad parameter test #8). (hr = 0x%08x). Failing.", hr);
		pIndexBuf->Unlock();
		pIndexBuf->Release();
		return TPR_FAIL;
	}

	//
	// Bad parameter test #9:  non-index-aligned lock
	//
	if (m_bDebugMiddleware)
	{
		if (SUCCEEDED(hr = pIndexBuf->Lock(1,              // UINT OffsetToLock
									  2,              // UINT SizeToLock
									  (VOID**)&pIndexBufData, // BYTE** ppbData
									  0)))            // DWORD Flags
		{
			DebugOut(DO_ERROR, L"Lock succeeded unexpectedly (bad parameter test #9). (hr = 0x%08x). Failing.", hr);
			pIndexBuf->Unlock();
			pIndexBuf->Release();
			return TPR_FAIL;
		}
	}

	pIndexBuf->Release();
	
	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;

}

//
// ExecuteUnlockTest
//
//   Verify IDirect3DMobileIndexBuffer::Unlock
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IndexBufferTest::ExecuteUnlockTest()
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
//   Verify IDirect3DMobileIndexBuffer::GetDesc
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IndexBufferTest::ExecuteGetDescTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteGetDescTest.");

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;
    
	//
	// D3DMFORMAT iterator to attempt index buffer creation with all valid IB formats
	//
	D3DMFORMAT CurrentFormat;

	//
	// Iterator for attempting to create index buffers of various sizes
	//
	UINT uiIBSize;

	//
	// Pool for index buffer creation
	//
	D3DMPOOL IndexBufPool;

	//
	// Description of Index Buffer
	//
	D3DMINDEXBUFFER_DESC IndexBufDesc;

	//
	// Interface pointer for index buffer to be created
	//
	IDirect3DMobileIndexBuffer *pIndexBuf;

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
	// Pick the index buffer pool that the driver supports, as reported in D3DMCAPS
	//
	if (D3DMSURFCAPS_VIDINDEXBUFFER & Caps.SurfaceCaps)
	{
		IndexBufPool = D3DMPOOL_VIDEOMEM;
	}
	else if (D3DMSURFCAPS_SYSINDEXBUFFER & Caps.SurfaceCaps)
	{
		IndexBufPool = D3DMPOOL_SYSTEMMEM;
	}
	else
	{
		DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSINDEXBUFFER or D3DMSURFCAPS_VIDINDEXBUFFER. Aborting.");
		return TPR_ABORT;
	}

	//
	// Iterate through all the index buffer formats in the D3DMFORMAT enumeration
	//
	for (CurrentFormat = D3DMFMT_INDEX16; CurrentFormat <= D3DMFMT_INDEX32; (*(ULONG*)(&CurrentFormat))++)
	{

		//
		// Iterate through a range of sizes, all valid for either D3DMFMT_INDEX16 
		// or D3DMFMT_INDEX32 (every iteration is an attempt at a non-zero IB with
		// byte size divisible by 4)
		//
		for(uiIBSize = 4; uiIBSize < D3DQA_MAX_INDEXBUF_SIZE; uiIBSize+=4)
		{

			//
			// Create a new index buffer; of the size specified by this iteration
			//
			if (FAILED(hr = m_pd3dDevice->CreateIndexBuffer(uiIBSize,        // UINT Length
			                                           0,               // DWORD Usage
			                                           CurrentFormat,   // D3DMFORMAT Format
			                                           IndexBufPool,    // D3DMPOOL Pool
			                                          &pIndexBuf)))     // IDirect3DMobileIndexBuffer** ppIndexBuffer
			{
				DebugOut(DO_ERROR, L"CreateIndexBuffer failed. (hr = 0x%08x). Failing.", hr);
				return TPR_FAIL;
			}

			//	
			// Invalid parameter test
			//	
			if (SUCCEEDED(hr = pIndexBuf->GetDesc(NULL))) // D3DMINDEXBUFFER_DESC *pDesc
			{
				DebugOut(DO_ERROR, L"GetDesc succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
				pIndexBuf->Release();
				return TPR_FAIL;
			}

			//	
			// Retrieve the buffer description
			//	
			if (FAILED(hr = pIndexBuf->GetDesc(&IndexBufDesc))) // D3DMINDEXBUFFER_DESC *pDesc
			{
				DebugOut(DO_ERROR, L"GetDesc failed. (hr = 0x%08x). Failing.", hr);
				pIndexBuf->Release();
				return TPR_FAIL;
			}

			//
			// For testing purposes, this index buffer interface pointer is not longer needed.
			// The D3DMINDEXBUFFER_DESC can be sufficiently scrutinized without further access
			// to the interface itself.
			//
			pIndexBuf->Release();

			//
			// Does the format match the expectation?
			//
			if (CurrentFormat != IndexBufDesc.Format)
			{
				DebugOut(DO_ERROR, L"Unexpected value in D3DMINDEXBUFFER_DESC.Format. Failing.");
				return TPR_FAIL;
			}

			//
			// Does the resource type match the expectation?
			//
			if (D3DMRTYPE_INDEXBUFFER != IndexBufDesc.Type)
			{
				DebugOut(DO_ERROR, L"Unexpected value in D3DMINDEXBUFFER_DESC.Type. Failing.");
				return TPR_FAIL;
			}

			//
			// Does the usage mask match the expectation?
			//
			if (D3DMUSAGE_LOCKABLE != IndexBufDesc.Usage)
			{
				DebugOut(DO_ERROR, L"Unexpected value in D3DMINDEXBUFFER_DESC.Usage. Failing.");
				return TPR_FAIL;
			}

			//
			// Does the pool type match the expectation?
			//
			if (IndexBufPool != IndexBufDesc.Pool)
			{
				DebugOut(DO_ERROR, L"Unexpected value in D3DMINDEXBUFFER_DESC.Pool. Failing.");
				return TPR_FAIL;
			}

			//
			// Does the buffer size (bytes) match the expectation?
			//
			if (uiIBSize != IndexBufDesc.Size)
			{
				DebugOut(DO_ERROR, L"Unexpected value in D3DMINDEXBUFFER_DESC.Size. Failing.");
				return TPR_FAIL;
			}
		}
	}

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}
