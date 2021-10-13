//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "IDirect3DMobileSurface.h"
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
// Default extents for surface creation
//
#define D3DQA_DEFAULT_SURF_WIDTH 4
#define D3DQA_DEFAULT_SURF_HEIGHT 4


//
//  SurfaceTest
//
//    Constructor for SurfaceTest class; however, most initialization is deferred.
//
//  Arguments:
//
//    none
//
//  Return Value:
//
//    none
//
SurfaceTest::SurfaceTest()
{
	m_bInitSuccess = FALSE;
}


//
// SurfaceTest
//
//   Initialize SurfaceTest object
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
HRESULT SurfaceTest::Init(LPTEST_CASE_ARGS pTestCaseArgs, LPWINDOW_ARGS pWindowArgs, BOOL bDebug, UINT uiTestCase)
{
	if (FAILED(D3DMInitializer::Init(D3DQA_PURPOSE_RAW_TEST,                      // D3DQA_PURPOSE Purpose
	                                 pWindowArgs,                                 // LPWINDOW_ARGS pWindowArgs
	                                 pTestCaseArgs->pwchSoftwareDeviceFilename,   // TCHAR *ptchDriver
	                                 uiTestCase)))                                // UINT uiTestCase
	{
		DebugOut(_T("Aborting initialization, due to prior initialization failure.\n"));
		return E_FAIL;
	}

	m_bDebugMiddleware = bDebug;

	m_bInitSuccess = TRUE;
	return S_OK;
}


//
//  IsReady
//
//    Accessor method for "initialization state" of SurfaceTest object.  "Half
//    initialized" states should not occur.
//  
//  Return Value:
//  
//    BOOL:  True if the object is initialized; FALSE if it is not.
//
BOOL SurfaceTest::IsReady()
{
	if (FALSE == m_bInitSuccess)
	{
		DebugOut(_T("SurfaceTest is not ready.\n"));
		return FALSE;
	}

	DebugOut(_T("SurfaceTest is ready.\n"));
	return D3DMInitializer::IsReady();
}

// 
// ~SurfaceTest
//
//  Destructor for SurfaceTest.  Currently; there is nothing
//  to do here.
//
SurfaceTest::~SurfaceTest()
{
	return; // Success
}


//
// ExecuteQueryInterfaceTest
//
//   Verify IDirect3DMobileSurface::QueryInterface.  Finds a supported format for
//   image surfaces, creates an image surface in that format, then attempts a variety
//   of invalid and valid QueryInterface calls from that image surface interface:
//
//       VALID:
//        
//        * Query for IID_IDirect3DMobileSurface
//
//       INVALID:
//        
//        * Invalid REFIID
//        * Invalid PPVOID
//        * Invalid REFIID, Invalid PPVOID
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT SurfaceTest::ExecuteQueryInterfaceTest()
{
	DebugOut(_T("Beginning ExecuteQueryInterfaceTest.\n"));

	//
	// There must be at least one valid format, for creating surfaces.
	// The first time a valid format is found, this BOOL is toggled.
	//
	// If the BOOL is never toggled, the test will fail after attempting
	// all possible formats.
	//
	BOOL bFoundValidFormat = FALSE;

	//
	// D3DMFORMAT iterator; to cycle through formats until a valid format
	// is found
	//
	D3DMFORMAT CurrentFormat;

	//
	// Interface pointers to be retrieved by QueryInterface
	//
	IDirect3DMobileSurface *pQISurface;

	//
	// Surface interface to be manipulated
	//
	IDirect3DMobileSurface* pSurface;

	//
	// The display mode's spatial resolution and color resolution
	//
	D3DMDISPLAYMODE Mode;

	if (!IsReady())
	{
		DebugOut(_T("Due to failed initialization, the test must be aborted.\n"));
		return TPR_SKIP;
	}

	//
	// Retrieve current display mode
	//
	if (FAILED(m_pd3dDevice->GetDisplayMode(&Mode))) // D3DMDISPLAYMODE* pMode
	{
		DebugOut(_T("Fatal error at GetDisplayMode; function returned failure.\n"));
		return TPR_FAIL;
	}

	//
	// Find a surface format that the device is capable of creating
	//
	for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
	{
		if (!(FAILED(m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal, // UINT Adapter
		                                       m_DevType,          // D3DMDEVTYPE DeviceType
		                                       Mode.Format,        // D3DMFORMAT AdapterFormat
		                                       0,                  // ULONG Usage
		                                       D3DMRTYPE_SURFACE,  // D3DMRESOURCETYPE RType
		                                       CurrentFormat))))   // D3DMFORMAT CheckFormat
		{
			bFoundValidFormat = TRUE;
			break;
		}
	}

	//
	// At least one surface format must be supported
	//
	if (FALSE == bFoundValidFormat)
	{
		DebugOut(_T("Fatal error; CheckDeviceFormat failed for every format.\n"));
		return TPR_FAIL;
	}

	//
	// Create a surface in a supported format
	//
	if (FAILED(m_pd3dDevice->CreateImageSurface(D3DQA_DEFAULT_SURF_WIDTH, // UINT Width
	                                            D3DQA_DEFAULT_SURF_HEIGHT,// UINT Height
	                                            CurrentFormat,            // D3DMFORMAT Format
	                                           &pSurface)))               // IDirect3DMobileSurface** ppSurface
	{
		DebugOut(_T("Fatal error; CreateImageSurface failed on a surface format that was approved by CheckDeviceFormat.\n"));
		return TPR_FAIL;
	}


	// 	
	// Bad parameter tests
	// 	

	//
	// Test case #1:  Invalid REFIID
	//
	if (SUCCEEDED(pSurface->QueryInterface(IID_D3DQAInvalidInterface, // REFIID riid
	                                       (void**)(&pQISurface))))   // void** ppvObj
	{
		DebugOut(_T("QueryInterface succeeded; failure expected.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}

	//
	// Test case #2:  Valid REFIID, invalid PPVOID
	//
	if (SUCCEEDED(pSurface->QueryInterface(IID_IDirect3DMobileSurface, // REFIID riid
	                                       (void**)0)))                // void** ppvObj
	{
		DebugOut(_T("QueryInterface succeeded; failure expected.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}

	//
	// Test case #3:  Invalid REFIID, invalid PPVOID
	//
	if (SUCCEEDED(pSurface->QueryInterface(IID_D3DQAInvalidInterface, // REFIID riid
	                                       (void**)0)))               // void** ppvObj
	{
		DebugOut(_T("QueryInterface succeeded; failure expected.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}


	//
	// Valid parameter tests
	//

	// 	
	// Perform an query on the surface interface
	// 	
	if (FAILED(pSurface->QueryInterface(IID_IDirect3DMobileSurface,  // REFIID riid
						  		          (void**)(&pQISurface))))     // void** ppvObj
	{
		DebugOut(_T("QueryInterface failed.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}


	//
	// Release the original interface pointer; verify that the ref count is still non-zero
	//
	if (!(pSurface->Release()))
	{
		DebugOut(_T("Reference count for IDirect3DMobileSurface dropped to zero; unexpected.\n"));
		return TPR_FAIL;
	}

	//
	// Release the IDirect3DMobileSurface interface pointer; ref count should now drop to zero
	//
	if (pQISurface->Release())
	{
		DebugOut(_T("Reference count for IDirect3DMobileSurface is non-zero; unexpected.\n"));
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
//   Verify IDirect3DMobileSurface::AddRef.  Finds a supported format for
//   image surfaces, then creates an image surface in that format.  Calls
//   AddRef and Release on through the image surface interface.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT SurfaceTest::ExecuteAddRefTest()        
{
	DebugOut(_T("Beginning ExecuteAddRefTest.\n"));

	//
	// There must be at least one valid format, for creating surfaces.
	// The first time a valid format is found, this BOOL is toggled.
	//
	// If the BOOL is never toggled, the test will fail after attempting
	// all possible formats.
	//
	BOOL bFoundValidFormat = FALSE;

	//
	// D3DMFORMAT iterator; to cycle through formats until a valid format
	// is found
	//
	D3DMFORMAT CurrentFormat;

	//
	// Surface interface to be manipulated
	//
	IDirect3DMobileSurface* pSurface;

	//
	// The display mode's spatial resolution and color resolution
	//
	D3DMDISPLAYMODE Mode;

	if (!IsReady())
	{
		DebugOut(_T("Due to failed initialization, the test must be aborted.\n"));
		return TPR_SKIP;
	}

	//
	// Retrieve current display mode
	//
	if (FAILED(m_pd3dDevice->GetDisplayMode(&Mode))) // D3DMDISPLAYMODE* pMode
	{
		DebugOut(_T("Fatal error at GetDisplayMode; function returned failure.\n"));
		return TPR_FAIL;
	}

	//
	// Find a surface format that the device is capable of creating
	//
	for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
	{
		if (!(FAILED(m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal, // UINT Adapter
		                                       m_DevType,          // D3DMDEVTYPE DeviceType
		                                       Mode.Format,        // D3DMFORMAT AdapterFormat
		                                       0,                  // ULONG Usage
		                                       D3DMRTYPE_SURFACE,  // D3DMRESOURCETYPE RType
		                                       CurrentFormat))))   // D3DMFORMAT CheckFormat
		{
			bFoundValidFormat = TRUE;
			break;
		}
	}

	//
	// At least one surface format must be supported
	//
	if (FALSE == bFoundValidFormat)
	{
		DebugOut(_T("Fatal error; CheckDeviceFormat failed for every format.\n"));
		return TPR_FAIL;
	}

	//
	// Create a surface in a supported format
	//
	if (FAILED(m_pd3dDevice->CreateImageSurface(D3DQA_DEFAULT_SURF_WIDTH, // UINT Width
	                                            D3DQA_DEFAULT_SURF_HEIGHT,// UINT Height
	                                            CurrentFormat,            // D3DMFORMAT Format
	                                           &pSurface)))               // IDirect3DMobileSurface** ppSurface
	{
		DebugOut(_T("Fatal error; CreateImageSurface failed on a surface format that was approved by CheckDeviceFormat.\n"));
		return TPR_FAIL;
	}

	
	//
	// Verify ref-count values; via AddRef/Release return values
	//
	if (2 != pSurface->AddRef())
	{
		DebugOut(_T("IDirect3DMobileSurface: unexpected reference count.\n"));
		return TPR_FAIL;
	}
	
	if (1 != pSurface->Release())
	{
		DebugOut(_T("IDirect3DMobileSurface: unexpected reference count.\n"));
		return TPR_FAIL;
	}
	
	if (0 != pSurface->Release())
	{
		DebugOut(_T("IDirect3DMobileSurface: unexpected reference count.\n"));
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
//   Verify IDirect3DMobileSurface::Release.  Test code for IDirect3DMobileSurface::AddRef
//   fully covers the test cases for IDirect3DMobileSurface::Release.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT SurfaceTest::ExecuteReleaseTest()       
{
	DebugOut(_T("Beginning ExecuteReleaseTest.\n"));

	if (!IsReady())
	{
		DebugOut(_T("Due to failed initialization, the test must be aborted.\n"));
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
//   Verify IDirect3DMobileSurface::GetDevice.  Finds a supported format for
//   image surfaces, then creates an image surface in that format.  Verifies
//   that GetDevice, called through the image surface interface pointer,
//   matches the known (existing) interface pointer for the IDirect3DMobileDevice.
//
//   Verifies that the GetDevice fails when passed a NULL paramter.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT SurfaceTest::ExecuteGetDeviceTest()     
{
	DebugOut(_T("Beginning ExecuteGetDeviceTest.\n"));

	//
	// There must be at least one valid format, for creating surfaces.
	// The first time a valid format is found, this BOOL is toggled.
	//
	// If the BOOL is never toggled, the test will fail after attempting
	// all possible formats.
	//
	BOOL bFoundValidFormat = FALSE;

	//
	// Device interface pointer to retrieve with GetDevice
	//
	IDirect3DMobileDevice* pDevice;

	//
	// D3DMFORMAT iterator; to cycle through formats until a valid format
	// is found
	//
	D3DMFORMAT CurrentFormat;

	//
	// Surface interface to be manipulated
	//
	IDirect3DMobileSurface* pSurface;

	//
	// The display mode's spatial resolution and color resolution
	//
	D3DMDISPLAYMODE Mode;

	if (!IsReady())
	{
		DebugOut(_T("Due to failed initialization, the test must be aborted.\n"));
		return TPR_SKIP;
	}

	//
	// Retrieve current display mode
	//
	if (FAILED(m_pd3dDevice->GetDisplayMode(&Mode))) // D3DMDISPLAYMODE* pMode
	{
		DebugOut(_T("Fatal error at GetDisplayMode; function returned failure.\n"));
		return TPR_FAIL;
	}

	//
	// Find a surface format that the device is capable of creating
	//
	for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
	{
		if (!(FAILED(m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal, // UINT Adapter
		                                       m_DevType,          // D3DMDEVTYPE DeviceType
		                                       Mode.Format,        // D3DMFORMAT AdapterFormat
		                                       0,                  // ULONG Usage
		                                       D3DMRTYPE_SURFACE,  // D3DMRESOURCETYPE RType
		                                       CurrentFormat))))   // D3DMFORMAT CheckFormat
		{
			bFoundValidFormat = TRUE;
			break;
		}
	}

	//
	// At least one surface format must be supported
	//
	if (FALSE == bFoundValidFormat)
	{
		DebugOut(_T("Fatal error; CheckDeviceFormat failed for every format.\n"));
		return TPR_FAIL;
	}

	//
	// Create a surface in a supported format
	//
	if (FAILED(m_pd3dDevice->CreateImageSurface(D3DQA_DEFAULT_SURF_WIDTH, // UINT Width
	                                            D3DQA_DEFAULT_SURF_HEIGHT,// UINT Height
	                                            CurrentFormat,            // D3DMFORMAT Format
	                                           &pSurface)))               // IDirect3DMobileSurface** ppSurface
	{
		DebugOut(_T("Fatal error; CreateImageSurface failed on a surface format that was approved by CheckDeviceFormat.\n"));
		return TPR_FAIL;
	}

	//
	// Retrieve the device interface pointer
	//
	if (FAILED(pSurface->GetDevice(&pDevice))) // IDirect3DMobileDevice** ppDevice
	{
		DebugOut(_T("GetDevice failed.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}

	//
	// Verify that the device interface pointer is consistent with expectations
	//
	if (pDevice != m_pd3dDevice)
	{
		DebugOut(_T("IDirect3DMobileDevice interface pointer mismatch.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}

	//
	// Release the device interface reference (incremented by GetDevice)
	//
	pDevice->Release();

	//
	// Bad parameter test #1:  NULL argument
	//
	if (SUCCEEDED(pSurface->GetDevice(NULL))) // IDirect3DMobileDevice** ppDevice
	{
		DebugOut(_T("GetDevice succeeded unexpectedly.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}

	//
	// Release the interface
	//
	pSurface->Release();
	
	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteGetContainerTest
//
//   Verify IDirect3DMobileSurface::GetContainer.  Finds a supported format for
//   image surfaces, then creates an image surface in that format.  Attempts a
//   variety of invalid and valid GetContainer calls from that image surface
//   interface:
//
//       VALID:
//        
//        * Query for IID_IDirect3DMobileDevice
//
//       INVALID:
//        
//        * Invalid REFIID
//        * Invalid PPVOID
//        * Invalid REFIID, Invalid PPVOID
//

//
//   Verifies that GetContainer fails when passed an invalid REFIID.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT SurfaceTest::ExecuteGetContainerTest()  
{
	DebugOut(_T("Beginning ExecuteGetContainerTest.\n"));
	//
	// There must be at least one valid format, for creating surfaces.
	// The first time a valid format is found, this BOOL is toggled.
	//
	// If the BOOL is never toggled, the test will fail after attempting
	// all possible formats.
	//
	BOOL bFoundValidFormat = FALSE;

	//
	// D3DMFORMAT iterator; to cycle through formats until a valid format
	// is found
	//
	D3DMFORMAT CurrentFormat;

	//
	// Surface interfaces to be manipulated
	//
	IDirect3DMobileSurface *pSurface;
	IDirect3DMobileDevice *pSurfaceContainer;

	//
	// The display mode's spatial resolution and color resolution
	//
	D3DMDISPLAYMODE Mode;

	if (!IsReady())
	{
		DebugOut(_T("Due to failed initialization, the test must be aborted.\n"));
		return TPR_SKIP;
	}

	//
	// Retrieve current display mode
	//
	if (FAILED(m_pd3dDevice->GetDisplayMode(&Mode))) // D3DMDISPLAYMODE* pMode
	{
		DebugOut(_T("Fatal error at GetDisplayMode; function returned failure.\n"));
		return TPR_FAIL;
	}

	//
	// Find a surface format that the device is capable of creating
	//
	for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
	{
		if (!(FAILED(m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal, // UINT Adapter
		                                       m_DevType,          // D3DMDEVTYPE DeviceType
		                                       Mode.Format,        // D3DMFORMAT AdapterFormat
		                                       0,                  // ULONG Usage
		                                       D3DMRTYPE_SURFACE,  // D3DMRESOURCETYPE RType
		                                       CurrentFormat))))   // D3DMFORMAT CheckFormat
		{
			bFoundValidFormat = TRUE;
			break;
		}
	}

	//
	// At least one surface format must be supported
	//
	if (FALSE == bFoundValidFormat)
	{
		DebugOut(_T("Fatal error; CheckDeviceFormat failed for every format.\n"));
		return TPR_FAIL;
	}

	//
	// Create a surface in a supported format
	//
	if (FAILED(m_pd3dDevice->CreateImageSurface(D3DQA_DEFAULT_SURF_WIDTH, // UINT Width
	                                            D3DQA_DEFAULT_SURF_HEIGHT,// UINT Height
	                                            CurrentFormat,            // D3DMFORMAT Format
	                                           &pSurface)))               // IDirect3DMobileSurface** ppSurface
	{
		DebugOut(_T("Fatal error; CreateImageSurface failed on a surface format that was approved by CheckDeviceFormat.\n"));
		return TPR_FAIL;
	}

	//
	// Retrieve container
	//
	if (FAILED(pSurface->GetContainer(IID_IDirect3DMobileDevice,      // REFIID riid
	                                  (void**)(&pSurfaceContainer)))) // void** ppvObj
	{
		DebugOut(_T("GetContainer failed.\n"));
		return TPR_FAIL;
	}

	//
	// Release container
	//
	pSurfaceContainer->Release();


	// 	
	// Bad parameter tests
	// 	

	//
	// Test case #1:  Invalid REFIID
	//
	if (SUCCEEDED(pSurface->GetContainer(IID_D3DQAInvalidInterface,      // REFIID riid
	                                     (void**)(&pSurfaceContainer)))) // void** ppvObj
	{
		DebugOut(_T("GetContainer succeeded; failure expected.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}

	//
	// Test case #2:  Invalid PPVOID
	//
	if (SUCCEEDED(pSurface->GetContainer(IID_IDirect3DMobileDevice,      // REFIID riid
	                                     (void**)0)))                    // void** ppvObj
	{
		DebugOut(_T("GetContainer succeeded; failure expected.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}

	//
	// Test case #3:  Invalid REFIID, Invalid PPVOID
	//
	if (SUCCEEDED(pSurface->GetContainer(IID_D3DQAInvalidInterface,      // REFIID riid
	                                     (void**)0)))                    // void** ppvObj
	{
		DebugOut(_T("GetContainer succeeded; failure expected.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}

	pSurface->Release();
	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteGetDescTest
//
//   Verify IDirect3DMobileSurface::GetDesc.  Finds a supported format for
//   image surfaces, then creates an image surface in that format.  Verifies
//   the D3DMSURFACE_DESC structure generated for this image surface.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT SurfaceTest::ExecuteGetDescTest()       
{
	DebugOut(_T("Beginning ExecuteGetDescTest.\n"));

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	//
	// There must be at least one valid format, for creating surfaces.
	// The first time a valid format is found, this BOOL is toggled.
	//
	// If the BOOL is never toggled, the test will fail after attempting
	// all possible formats.
	//
	BOOL bFoundValidFormat = FALSE;

	//
	// D3DMFORMAT iterator; to cycle through formats until a valid format
	// is found
	//
	D3DMFORMAT CurrentFormat;

	//
	// Surface interfaces to be manipulated
	//
	IDirect3DMobileSurface *pSurface;

	//
	// Description of Surface
	//
	D3DMSURFACE_DESC SurfaceDesc;

	//
	// Pool for surfaces
	//
	D3DMPOOL SurfacePool;

	//
	// The display mode's spatial resolution and color resolution
	//
	D3DMDISPLAYMODE Mode;

	if (!IsReady())
	{
		DebugOut(_T("Due to failed initialization, the test must be aborted.\n"));
		return TPR_SKIP;
	}

	//
	// Retrieve driver capabilities
	//
	if (FAILED(m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(_T("Fatal error at GetDeviceCaps.\n"));
		return TPR_FAIL;
	}

	//
	// Pick the pool that the driver supports, as reported in D3DMCAPS
	//
	if (D3DMSURFCAPS_VIDIMAGESURFACE & Caps.SurfaceCaps)
	{
		SurfacePool = D3DMPOOL_VIDEOMEM;
	}
	else if (D3DMSURFCAPS_SYSIMAGESURFACE & Caps.SurfaceCaps)
	{
		SurfacePool = D3DMPOOL_SYSTEMMEM;
	}
	else
	{
		DebugOut(_T("Failure: no support for either D3DMSURFCAPS_SYSIMAGESURFACE or D3DMSURFCAPS_VIDIMAGESURFACE \n"));
		return TPR_ABORT;
	}

	//
	// Retrieve current display mode
	//
	if (FAILED(m_pd3dDevice->GetDisplayMode(&Mode))) // D3DMDISPLAYMODE* pMode
	{
		DebugOut(_T("Fatal error at GetDisplayMode; function returned failure.\n"));
		return TPR_FAIL;
	}

	//
	// Find a surface format that the device is capable of creating
	//
	for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
	{
		if (!(FAILED(m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal, // UINT Adapter
		                                       m_DevType,          // D3DMDEVTYPE DeviceType
		                                       Mode.Format,        // D3DMFORMAT AdapterFormat
		                                       0,                  // ULONG Usage
		                                       D3DMRTYPE_SURFACE,  // D3DMRESOURCETYPE RType
		                                       CurrentFormat))))   // D3DMFORMAT CheckFormat
		{
			bFoundValidFormat = TRUE;
			break;
		}
	}

	//
	// At least one surface format must be supported
	//
	if (FALSE == bFoundValidFormat)
	{
		DebugOut(_T("Fatal error; CheckDeviceFormat failed for every format.\n"));
		return TPR_FAIL;
	}

	//
	// Create a surface in a supported format
	//
	if (FAILED(m_pd3dDevice->CreateImageSurface(D3DQA_DEFAULT_SURF_WIDTH, // UINT Width
	                                            D3DQA_DEFAULT_SURF_HEIGHT,// UINT Height
	                                            CurrentFormat,            // D3DMFORMAT Format
	                                           &pSurface)))               // IDirect3DMobileSurface** ppSurface
	{
		DebugOut(_T("Fatal error; CreateImageSurface failed on a surface format that was approved by CheckDeviceFormat.\n"));
		return TPR_FAIL;
	}


	//	
	// Retrieve the surface description
	//	
	if (FAILED(pSurface->GetDesc(&SurfaceDesc))) // D3DMSURFACE_DESC *pDesc
	{
		DebugOut(_T("GetDesc failed.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}


	//	
	// Bad parameter test #1:  NULL arg
	//	
	if (SUCCEEDED(pSurface->GetDesc(NULL))) // D3DMSURFACE_DESC *pDesc
	{
		DebugOut(_T("GetDesc succeeded unexpectedly.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}

	//
	// For testing purposes, this interface pointer is not longer needed.
	// The D3DMSURFACE_DESC can be sufficiently scrutinized without further access
	// to the interface itself.
	//
	pSurface->Release();

	//
	// Does the format match the expectation?
	//
	if (CurrentFormat != SurfaceDesc.Format)
	{
		DebugOut(_T("Unexpected value in D3DMSURFACE_DESC.Format\n"));
		return TPR_FAIL;
	}

	//
	// Does the resource type match the expectation?
	//
	if (D3DMRTYPE_SURFACE != SurfaceDesc.Type)
	{
		DebugOut(_T("Unexpected value in D3DMSURFACE_DESC.Type\n"));
		return TPR_FAIL;
	}

	//
	// Does the usage mask match the expectation?
	//
	if (D3DMUSAGE_LOCKABLE != SurfaceDesc.Usage)
	{
		DebugOut(_T("Unexpected value in D3DMSURFACE_DESC.Usage\n"));
		return TPR_FAIL;
	}

	//
	// Does the pool type match the expectation?
	//
	if (SurfacePool != SurfaceDesc.Pool)
	{
		DebugOut(_T("Unexpected value in D3DMSURFACE_DESC.Pool\n"));
		return TPR_FAIL;
	}

	//
	// Is the size non-zero?
	//
	if (0 == SurfaceDesc.Size)
	{
		DebugOut(_T("Unexpected value in D3DMSURFACE_DESC.Size\n"));
		return TPR_FAIL;
	}

	//
	// Does the MultiSampleType match the expectation?
	//
	if (D3DMMULTISAMPLE_NONE != SurfaceDesc.MultiSampleType)
	{
		DebugOut(_T("Unexpected value in D3DMSURFACE_DESC.MultiSampleType\n"));
		return TPR_FAIL;
	}

	//
	// Does the Width match the expectation?
	//
	if (D3DQA_DEFAULT_SURF_WIDTH != SurfaceDesc.Width)
	{
		DebugOut(_T("Unexpected value in D3DMSURFACE_DESC.Width\n"));
		return TPR_FAIL;
	}

	//
	// Does the Height match the expectation?
	//
	if (D3DQA_DEFAULT_SURF_HEIGHT != SurfaceDesc.Height)
	{
		DebugOut(_T("Unexpected value in D3DMSURFACE_DESC.Height\n"));
		return TPR_FAIL;
	}

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteLockRectTest
//
//   Verify IDirect3DMobileSurface::LockRect.  Finds a supported format for
//   image surfaces, then creates an image surface in that format.  Attempts
//   a variety of invalid and valid LockRect calls through this image surface
//   interface:
//
//       VALID:
//        
//        * Full surface lock
//        * Partial surface lock
//
//       INVALID:
//        
//        * Invalid D3DMLOCKED_RECT*
//        * Invalid Flags
//        * Specify D3DMLOCK_NO_DIRTY_UPDATE (not valid with image surfaces)
//        * Specify D3DMLOCK_DISCARD (only valid with D3DMUSAGE_DYNAMIC)
//        * Specify D3DMLOCK_NOOVERWRITE (only valid with D3DMUSAGE_DYNAMIC)
//        * Double lock
//        * Invalid RECT extents
//        
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT SurfaceTest::ExecuteLockRectTest()      
{
	DebugOut(_T("Beginning ExecuteLockRectTest.\n"));

	//
	// There must be at least one valid format, for creating surfaces.
	// The first time a valid format is found, this BOOL is toggled.
	//
	// If the BOOL is never toggled, the test will fail after attempting
	// all possible formats.
	//
	BOOL bFoundValidFormat = FALSE;

	//
	// Description of locked surface
	//
	D3DMLOCKED_RECT LockedRect;
	
	//
	// D3DMFORMAT iterator; to cycle through formats until a valid format
	// is found
	//
	D3DMFORMAT CurrentFormat;

	//
	// Surface interfaces to be manipulated
	//
	IDirect3DMobileSurface *pSurface;

	//
	// Rectangles for surface lock attempts
	//
	RECT rPartial = {0,0,1,1};
	RECT rInvalidRect = {0,0,0xFFFFFFFF,0xFFFFFFFF};

	//
	// The display mode's spatial resolution and color resolution
	//
	D3DMDISPLAYMODE Mode;

	if (!IsReady())
	{
		DebugOut(_T("Due to failed initialization, the test must be aborted.\n"));
		return TPR_SKIP;
	}

	//
	// Retrieve current display mode
	//
	if (FAILED(m_pd3dDevice->GetDisplayMode(&Mode))) // D3DMDISPLAYMODE* pMode
	{
		DebugOut(_T("Fatal error at GetDisplayMode; function returned failure.\n"));
		return TPR_FAIL;
	}

	//
	// Find a surface format that the device is capable of creating
	//
	for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
	{
		if (!(FAILED(m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal, // UINT Adapter
		                                       m_DevType,          // D3DMDEVTYPE DeviceType
		                                       Mode.Format,        // D3DMFORMAT AdapterFormat
		                                       0,                  // ULONG Usage
		                                       D3DMRTYPE_SURFACE,  // D3DMRESOURCETYPE RType
		                                       CurrentFormat))))   // D3DMFORMAT CheckFormat
		{
			bFoundValidFormat = TRUE;
			break;
		}
	}

	//
	// At least one surface format must be supported
	//
	if (FALSE == bFoundValidFormat)
	{
		DebugOut(_T("Fatal error; CheckDeviceFormat failed for every format.\n"));
		return TPR_FAIL;
	}

	//
	// Create a surface in a supported format
	//
	if (FAILED(m_pd3dDevice->CreateImageSurface(D3DQA_DEFAULT_SURF_WIDTH, // UINT Width
	                                            D3DQA_DEFAULT_SURF_HEIGHT,// UINT Height
	                                            CurrentFormat,            // D3DMFORMAT Format
	                                           &pSurface)))               // IDirect3DMobileSurface** ppSurface
	{
		DebugOut(_T("Fatal error; CreateImageSurface failed on a surface format that was approved by CheckDeviceFormat.\n"));
		return TPR_FAIL;
	}    

	//
	// Lock the entire surface
	//
	if (FAILED(pSurface->LockRect(&LockedRect, // D3DMLOCKED_RECT* pLockedRect,
	                              NULL,        // CONST RECT* pRect
	                              0)))         // DWORD Flags
	{
		DebugOut(_T("LockRect failed.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}   	
	
	//
	// Unlock the entire surface
	//
	if (FAILED(pSurface->UnlockRect()))
	{
		DebugOut(_T("LockRect failed.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}   	

	//
	// Lock part of the surface
	//
	if (FAILED(pSurface->LockRect(&LockedRect, // D3DMLOCKED_RECT* pLockedRect,
	                              &rPartial,   // CONST RECT* pRect
	                              0)))         // DWORD Flags
	{
		DebugOut(_T("LockRect failed.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}   	
	
	//
	// Unlock the partially locked surface
	//
	if (FAILED(pSurface->UnlockRect()))
	{
		DebugOut(_T("LockRect failed.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}   	


	//
	// Bad parameter test #1:  Invalid D3DMLOCKED_RECT*
	//
	if (SUCCEEDED(pSurface->LockRect(NULL,        // D3DMLOCKED_RECT* pLockedRect,
	                                 NULL,        // CONST RECT* pRect
	                                 0)))         // DWORD Flags
	{
		DebugOut(_T("LockRect succeeded unexpectedly.\n"));
		pSurface->UnlockRect();
		pSurface->Release();
		return TPR_FAIL;
	}   	
	
	//
	// Bad parameter test #2:  Invalid Flags
	//
	if (SUCCEEDED(pSurface->LockRect(&LockedRect, // D3DMLOCKED_RECT* pLockedRect,
	                                 NULL,        // CONST RECT* pRect
	                                 0xFFFFFFFF)))// DWORD Flags
	{
		DebugOut(_T("LockRect succeeded unexpectedly.\n"));
		pSurface->UnlockRect();
		pSurface->Release();
		return TPR_FAIL;
	}   	
	
	//
	// Bad parameter test #3:  D3DMLOCK_NO_DIRTY_UPDATE not valid with image surfaces
	//
	if (SUCCEEDED(pSurface->LockRect(&LockedRect,                // D3DMLOCKED_RECT* pLockedRect,
	                                 NULL,                       // CONST RECT* pRect
	                                 D3DMLOCK_NO_DIRTY_UPDATE))) // DWORD Flags
	{
		DebugOut(_T("LockRect succeeded unexpectedly.\n"));
		pSurface->UnlockRect();
		pSurface->Release();
		return TPR_FAIL;
	}   	
	
	//
	// Bad parameter test #4:  D3DMLOCK_DISCARD only valid with D3DMUSAGE_DYNAMIC
	//
	if (SUCCEEDED(pSurface->LockRect(&LockedRect,                // D3DMLOCKED_RECT* pLockedRect,
	                                 NULL,                       // CONST RECT* pRect
	                                 D3DMLOCK_DISCARD)))         // DWORD Flags
	{
		DebugOut(_T("LockRect succeeded unexpectedly.\n"));
		pSurface->UnlockRect();
		pSurface->Release();
		return TPR_FAIL;
	}   	
	
	//
	// Bad parameter test #5:  D3DMLOCK_NOOVERWRITE only valid with D3DMUSAGE_DYNAMIC
	//
	if (SUCCEEDED(pSurface->LockRect(&LockedRect,                // D3DMLOCKED_RECT* pLockedRect,
	                                 NULL,                       // CONST RECT* pRect
	                                 D3DMLOCK_NOOVERWRITE)))     // DWORD Flags
	{
		DebugOut(_T("LockRect succeeded unexpectedly.\n"));
		pSurface->UnlockRect();
		pSurface->Release();
		return TPR_FAIL;
	}   	


	//
	// Bad parameter test #6:  Double lock
	//

	//
	// Lock the entire surface
	//
	if (FAILED(pSurface->LockRect(&LockedRect, // D3DMLOCKED_RECT* pLockedRect,
	                              NULL,        // CONST RECT* pRect
	                              0)))         // DWORD Flags
	{
		DebugOut(_T("LockRect failed.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}   	
	else
	{
		//
		// Second lock; failure expected
		//
		if (SUCCEEDED(pSurface->LockRect(&LockedRect, // D3DMLOCKED_RECT* pLockedRect,
		                                 NULL,        // CONST RECT* pRect
		                                 0)))         // DWORD Flags
		{
			DebugOut(_T("LockRect succeeded unexpectedly.\n"));
			pSurface->UnlockRect();
			pSurface->Release();
			return TPR_FAIL;
		}   	

		//
		// Unlock the entire surface
		//
		if (FAILED(pSurface->UnlockRect()))
		{
			DebugOut(_T("LockRect failed.\n"));
			pSurface->Release();
			return TPR_FAIL;
		}   	
	}
	
	
	//
	// Bad parameter test #7:  Invalid RECT extents
	//
	if (SUCCEEDED(pSurface->LockRect(&LockedRect,                // D3DMLOCKED_RECT* pLockedRect,
	                                 &rInvalidRect,              // CONST RECT* pRect
	                                 D3DMLOCK_NOOVERWRITE)))     // DWORD Flags
	{
		DebugOut(_T("LockRect succeeded unexpectedly.\n"));
		pSurface->UnlockRect();
		pSurface->Release();
		return TPR_FAIL;
	}   	


	//
	// Surface interface is no longer needed
	//
	pSurface->Release();

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteUnlockRectTest
//
//   Verify IDirect3DMobileSurface::UnlockRect.  Test code for IDirect3DMobileSurface::LockRect
//   fully covers the test cases for IDirect3DMobileSurface::UnlockRect.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT SurfaceTest::ExecuteUnlockRectTest()    
{
	DebugOut(_T("Beginning ExecuteUnlockRectTest.\n"));

	if (!IsReady())
	{
		DebugOut(_T("Due to failed initialization, the test must be aborted.\n"));
		return TPR_SKIP;
	}
        
	//
	// Testing for this method occurs in a closely-related test:
	//
	return ExecuteLockRectTest();
}

//
// ExecuteGetDCTest
//
//   Verify IDirect3DMobileSurface::GetDC, ReleaseDC.  Finds a supported format
//   for image surfaces, then creates an image surface in that format.  Attempts
//   invalid and valid calls through this image surface interface:
//
//       VALID:
//        
//        * GetDC with valid HDC*
//        * ReleaseDC with valid HDC
//
//       INVALID:
//        
//        * GetDC with invalid HDC*
//        * ReleaseDC with invalid HDC
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT SurfaceTest::ExecuteGetDCTest()         
{
	DebugOut(_T("Beginning ExecuteGetDCTest.\n"));

	//
	// Device context for use with GDI
	//
	HDC hDC;

	//
	// There must be at least one valid format, for creating surfaces.
	// The first time a valid format is found, this BOOL is toggled.
	//
	// If the BOOL is never toggled, the test will fail after attempting
	// all possible formats.
	//
	BOOL bFoundValidFormat = FALSE;

	//
	// D3DMFORMAT iterator; to cycle through formats until a valid format
	// is found
	//
	D3DMFORMAT CurrentFormat;

	//
	// Surface interfaces to be manipulated
	//
	IDirect3DMobileSurface *pSurface;

	//
	// The display mode's spatial resolution and color resolution
	//
	D3DMDISPLAYMODE Mode;

	if (!IsReady())
	{
		DebugOut(_T("Due to failed initialization, the test must be aborted.\n"));
		return TPR_SKIP;
	}

	//
	// Retrieve current display mode
	//
	if (FAILED(m_pd3dDevice->GetDisplayMode(&Mode))) // D3DMDISPLAYMODE* pMode
	{
		DebugOut(_T("Fatal error at GetDisplayMode; function returned failure.\n"));
		return TPR_FAIL;
	}

	//
	// Find a surface format that the device is capable of creating
	//
	for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
	{
		if (!(FAILED(m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal, // UINT Adapter
		                                       m_DevType,          // D3DMDEVTYPE DeviceType
		                                       Mode.Format,        // D3DMFORMAT AdapterFormat
		                                       0,                  // ULONG Usage
		                                       D3DMRTYPE_SURFACE,  // D3DMRESOURCETYPE RType
		                                       CurrentFormat))))   // D3DMFORMAT CheckFormat
		{
			bFoundValidFormat = TRUE;
			break;
		}
	}

	//
	// At least one surface format must be supported
	//
	if (FALSE == bFoundValidFormat)
	{
		DebugOut(_T("Fatal error; CheckDeviceFormat failed for every format.\n"));
		return TPR_FAIL;
	}

	//
	// Create a surface in a supported format
	//
	if (FAILED(m_pd3dDevice->CreateImageSurface(D3DQA_DEFAULT_SURF_WIDTH, // UINT Width
	                                            D3DQA_DEFAULT_SURF_HEIGHT,// UINT Height
	                                            CurrentFormat,            // D3DMFORMAT Format
	                                           &pSurface)))               // IDirect3DMobileSurface** ppSurface
	{
		DebugOut(_T("Fatal error; CreateImageSurface failed on a surface format that was approved by CheckDeviceFormat.\n"));
		return TPR_FAIL;
	}

	//
	// Get the device context
	//
	if (FAILED(pSurface->GetDC(&hDC))) // HDC* phdc
	{
		DebugOut(_T("GetDC failed.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}
    
	//
	// NULL handles are invalid
	//
	if (NULL == hDC)
	{
		DebugOut(_T("GetDC returned NULL device context.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}

	//
	// Bad parameter test #1:  NULL arg
	//
	if (SUCCEEDED(pSurface->GetDC(NULL))) // HDC* phdc
	{
		DebugOut(_T("GetDC succeeded unexpectedly.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}

	//
	// Bad parameter test #2:  NULL arg for ReleaseDC
	//
	if (SUCCEEDED(pSurface->ReleaseDC((HDC)0xFFFFFFFF))) // HDC hdc
	{
		DebugOut(_T("ReleaseDC succeeded unexpectedly.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}

	//
	// Release the device context
	//
	if (FAILED(pSurface->ReleaseDC(hDC))) // HDC hdc
	{
		DebugOut(_T("ReleaseDC failed.\n"));
		pSurface->Release();
		return TPR_FAIL;
	}

	//
	// Surface interface is no longer needed
	//
	pSurface->Release();

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteReleaseDCTest
//
//   Verify IDirect3DMobileSurface::ReleaseDC.  Test code for IDirect3DMobileSurface::GetDC
//   fully covers the test cases for IDirect3DMobileSurface::ReleaseDC.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT SurfaceTest::ExecuteReleaseDCTest()     
{
	DebugOut(_T("Beginning ExecuteReleaseDCTest.\n"));

	if (!IsReady())
	{
		DebugOut(_T("Due to failed initialization, the test must be aborted.\n"));
		return TPR_SKIP;
	}

	//
	// Testing for this method occurs in a closely-related test:
	//
	return ExecuteGetDCTest();
}

