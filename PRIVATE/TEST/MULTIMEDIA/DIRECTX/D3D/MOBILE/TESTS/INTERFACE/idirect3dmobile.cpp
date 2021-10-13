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
#define INITGUID

#include "IDirect3DMobile.h"
#include "DebugOutput.h"
#include "KatoUtils.h"
#include "BufferTools.h"
#include "D3DMStrings.h"
#include "D3DMHelper.h"
#include <tux.h>
#include <tchar.h>
#include <stdio.h>

#define D3DMDLL _T("D3DM.DLL")
#define D3DMCREATE _T("Direct3DMobileCreate")

//
// Allow prefast pragmas
//
#pragma warning(disable:4068)  // warning 4068: unknown pragma

INT SetResult(INT & Result, INT NewResult);

//
// GUID for invalid parameter tests
//
DEFINE_GUID(IID_D3DQAInvalidInterface, 
0x00000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

int GetModulePath(HMODULE hMod, __out_ecount(dwCchSize) TCHAR * tszPath, DWORD dwCchSize)
{
    int iLen = GetModuleFileName(hMod, tszPath, dwCchSize);

    while (iLen > 0 && tszPath[iLen - 1] != _T('\\'))
        --iLen;

    if (iLen >= 0)
    {
        tszPath[iLen] = 0;
    }
    return iLen;
}

void OutputAdapterIdentifier(TCHAR * tszDescription, const D3DMADAPTER_IDENTIFIER * pIdent)
{
    DebugOut(_T("Adapter Identifier for %s"), tszDescription);
    DebugOut(_T("   Driver:           %s"), pIdent->Driver);
    DebugOut(_T("   Description:      %s"), pIdent->Description);
    DebugOut(_T("   DriverVersion:    %d.%d.%d.%d"), 
        HIWORD(pIdent->DriverVersion.HighPart), 
        LOWORD(pIdent->DriverVersion.HighPart), 
        HIWORD(pIdent->DriverVersion.LowPart),
        LOWORD(pIdent->DriverVersion.LowPart));
    DebugOut(_T("   VendorId:         %d"), pIdent->VendorId);
    DebugOut(_T("   DeviceId:         %d"), pIdent->DeviceId);
    DebugOut(_T("   SubSysId:         %d"), pIdent->SubSysId);
    DebugOut(_T("   Revision:         %d"), pIdent->Revision);
    DebugOut(_T("   DeviceIdentifier: {%08X-%04X-%04X-%02X%02X%02X%02X%02X%02X%02X%02X}"), 
        pIdent->DeviceIdentifier.Data1,
        pIdent->DeviceIdentifier.Data2,
        pIdent->DeviceIdentifier.Data3,
        pIdent->DeviceIdentifier.Data4[0],
        pIdent->DeviceIdentifier.Data4[1],
        pIdent->DeviceIdentifier.Data4[2],
        pIdent->DeviceIdentifier.Data4[3],
        pIdent->DeviceIdentifier.Data4[4],
        pIdent->DeviceIdentifier.Data4[5],
        pIdent->DeviceIdentifier.Data4[6],
        pIdent->DeviceIdentifier.Data4[7]
        );
}

//
//  IDirect3DMobileTest
//
//    Constructor for IDirect3DMobileTest class; however, most initialization is deferred.
//
//  Arguments:
//
//    none
//
//  Return Value:
//
//    none
//
IDirect3DMobileTest::IDirect3DMobileTest()
{
	m_bInitSuccess = FALSE;
}


//
// IDirect3DMobileTest
//
//   Initialize IDirect3DMobileTest object
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
HRESULT IDirect3DMobileTest::Init(LPTEST_CASE_ARGS pTestCaseArgs, LPWINDOW_ARGS pWindowArgs, BOOL bDebug, UINT uiTestCase)
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
//    Accessor method for "initialization state" of IDirect3DMobileTest object.  "Half
//    initialized" states should not occur.
//  
//  Return Value:
//  
//    BOOL:  TRUE if the object is initialized; FALSE if it is not.
//
BOOL IDirect3DMobileTest::IsReady()
{
	if (FALSE == m_bInitSuccess)
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileTest is not ready.");
		return FALSE;
	}

	DebugOut(L"IDirect3DMobileTest is ready.");
	return D3DMInitializer::IsReady();
}


// 
// ~IDirect3DMobileTest
//
//  Destructor for IDirect3DMobileTest.  Currently; there is nothing
//  to do here.
//
IDirect3DMobileTest::~IDirect3DMobileTest()
{
	return; // Success
}



//
// ExecuteQueryInterfaceTest
//
//   Verify IDirect3DMobile::QueryInterface
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileTest::ExecuteQueryInterfaceTest()
{
    HRESULT hr;
    
	DebugOut(L"Beginning ExecuteQueryInterfaceTest.");

	//
	// Interface pointer to retrieve from QueryInterface
	//
	IDirect3DMobile *pQID3D;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}


	// 	
	// Bad parameter tests
	// 	

	//
	// Test case #1:  Invalid REFIID
	//
	if (SUCCEEDED(hr = m_pD3D->QueryInterface(IID_D3DQAInvalidInterface, // REFIID riid
	                                     (void**)(&pQID3D))))       // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// Test case #2:  Valid REFIID, invalid PPVOID
	//
	if (SUCCEEDED(hr = m_pD3D->QueryInterface(IID_IDirect3DMobile, // REFIID riid
	                                     (void**)0)))         // void** ppvObj
	{
		DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}


	//
	// Valid parameter tests
	//

	//
	// Retrieve IID_IDirect3DMobile
	//
	if (FAILED(hr = m_pD3D->QueryInterface(IID_IDirect3DMobile, // REFIID riid
	                                  (void**)(&pQID3D)))) // void** ppvObj
	{
		DebugOut(DO_ERROR, L"IDirect3DMobile::QueryInterface failed for IID_IDirect3DMobile. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}
	else
	{
		if (0 == pQID3D->Release())
		{
			DebugOut(DO_ERROR, L"IDirect3DMobile reference count dropped to zero unexpectedly. Failing");
			return TPR_FAIL;
		}
	}

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}


//
// ExecuteAddRefTest
//
//   Verify IDirect3DMobile::AddRef
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileTest::ExecuteAddRefTest()
{
	DebugOut(L"Beginning ExecuteAddRefTest.");

	ULONG ulRefs1, ulRefs2;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Returns the value of the new reference count
	//
	ulRefs1 = m_pD3D->AddRef();

	//
	// Returns the value of the new reference count
	//
	ulRefs2 = m_pD3D->Release();

	//
	// Verify that the reference ops incremented/decremented as expected
	//
	if ((ulRefs1 - 1) != ulRefs2)
	{
		DebugOut(DO_ERROR, L"IDirect3DMobile::AddRef / IDirect3DMobile::Release test failure. Failing.");
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
//   Verify IDirect3DMobile::Release
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileTest::ExecuteReleaseTest()
{
	DebugOut(L"Beginning ExecuteReleaseTest.");

	//
	// IDirect3DMobile::Release testing is covered in a closely-related test
	//
	return ExecuteAddRefTest();
}

//
// ExecuteRegisterSoftwareDeviceTest
//
//   Verify IDirect3DMobile::RegisterSoftwareDevice
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileTest::ExecuteRegisterSoftwareDeviceTest()
{
    HRESULT hr;
    
	DebugOut(L"Beginning ExecuteRegisterSoftwareDeviceTest.");

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// If the current device is a registered software device, release it as a prerequisite to
	// this test.
	// 
	if (D3DMADAPTER_REGISTERED_DEVICE == m_uiAdapterOrdinal)
	{
		if (0 != m_pd3dDevice->Release())
		{
			DebugOut(DO_ERROR, L"IDirect3DMobileDevice::Release returned non-zero reference count; fatal error. Failing.");
			m_pd3dDevice = NULL;
			return TPR_FAIL;
		}
		m_pd3dDevice = NULL;
	}

	//
	// Verify that RegisterSoftwareDevice can be used to clear out the current registration
	//
	if (FAILED(hr = m_pD3D->RegisterSoftwareDevice(NULL)))
	{
		DebugOut(DO_ERROR, L"Fatal error at IDirect3DMobile::RegisterSoftwareDevice. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteGetAdapterCountTest
//
//   Verify IDirect3DMobile::GetAdapterCount
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
#pragma prefast(disable: 262, "temporarily ignore stack size guideline") 
INT IDirect3DMobileTest::ExecuteGetAdapterCountTest()
{
	DebugOut(L"Beginning ExecuteGetAdapterCountTest.");

	//
	// Adapter identifiers
	//
	D3DMADAPTER_IDENTIFIER AdaptDefault, AdaptSoftware;

	//
	// API result status codes
	//
	HRESULT hr1, hr2;

	//
	// Number of adapters expected from GetAdapterCount
	//
	UINT uiNumExpectedAdapters = 0;

	//
	// Actual number of adapters reported by GetAdapterCount
	//
	UINT uiActualAdapters;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Retrieve adapter info for default device
	//
	hr1 = m_pD3D->GetAdapterIdentifier(D3DMADAPTER_DEFAULT,// UINT Adapter
	                                   0,                  // ULONG Flags
	                                  &AdaptDefault);      // D3DMADAPTER_IDENTIFIER* pIdentifier
	
	//
	// Retrieve adapter info for software device
	//
	hr2 = m_pD3D->GetAdapterIdentifier(D3DMADAPTER_REGISTERED_DEVICE,// UINT Adapter
	                                   0,                            // ULONG Flags
	                                  &AdaptSoftware);               // D3DMADAPTER_IDENTIFIER* pIdentifier

	if (SUCCEEDED(hr1))
		uiNumExpectedAdapters++;

	if (SUCCEEDED(hr2))
		uiNumExpectedAdapters++;

	uiActualAdapters = m_pD3D->GetAdapterCount();

	if (uiActualAdapters != uiNumExpectedAdapters)
	{
		DebugOut(DO_ERROR, L"Adapter count mismatch. (Expected %d; Found %d) Failing.", uiNumExpectedAdapters, uiActualAdapters);
		return TPR_FAIL;
	}

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}
#pragma prefast(pop)


//
// ExecuteGetAdapterIdentifierTest
//
//   Verify IDirect3DMobile::GetAdapterIdentifier
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
#pragma prefast(disable: 262, "temporarily ignore stack size guideline") 
INT IDirect3DMobileTest::ExecuteGetAdapterIdentifierTest()
{
	DebugOut(L"Beginning ExecuteGetAdapterIdentifierTest.");

	//
	// Adapter identifiers
	//
	D3DMADAPTER_IDENTIFIER AdaptDefault, AdaptSoftware, AdaptInvalid, AdaptEmpty;

	//
	// API result status codes
	//
	HRESULT hr1, hr2, hr;

	//
	// Test Result
	//
	INT Result = TPR_PASS;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Retrieve adapter info for default device
	//
	hr1 = m_pD3D->GetAdapterIdentifier(D3DMADAPTER_DEFAULT,// UINT Adapter
	                                   0,                  // ULONG Flags
	                                  &AdaptDefault);      // D3DMADAPTER_IDENTIFIER* pIdentifier
	
	//
	// Retrieve adapter info for software device
	//
	hr2 = m_pD3D->GetAdapterIdentifier(D3DMADAPTER_REGISTERED_DEVICE,// UINT Adapter
	                                   0,                            // ULONG Flags
	                                  &AdaptSoftware);               // D3DMADAPTER_IDENTIFIER* pIdentifier
	
	//
	// Verify that query succeeded for required device
	//
	if (FAILED(hr1))
	{
		DebugOut(DO_ERROR, L"GetAdapterIdentifier failed for default device. Default device is required. (hr = 0x%08x) Failing", hr1);
		SetResult(Result, TPR_FAIL);
	}

	//
	// Retrieve adapter info for an invalid device
	//
	if (SUCCEEDED(hr = m_pD3D->GetAdapterIdentifier(3,                    // UINT Adapter
	                                           0,                    // ULONG Flags
	                                           &AdaptInvalid    )))  // D3DMADAPTER_IDENTIFIER* pIdentifier
	{
		DebugOut(DO_ERROR, L"GetAdapterIdentifier returned success when called with invalid Adapter. (hr = 0x%08x) Failing", hr);
		SetResult(Result, TPR_FAIL);
	}

    memset(&AdaptEmpty, 0x00, sizeof(AdaptEmpty));
	if (SUCCEEDED(hr1))
	{
	    if (!memcmp(&AdaptEmpty, &AdaptDefault, sizeof(AdaptDefault)))
	    {
	        DebugOut(DO_ERROR, L"Default Adapter Identifier is completely blank. GUID must be set properly. Failing.");
	        SetResult(Result, TPR_FAIL);
	    }
	    
        if (!wcsicmp(L"d3dmref.dll", AdaptDefault.Driver) || 
            !wcsicmp(L"Microsoft Direct3D Mobile Reference Driver", AdaptDefault.Description))
        {
            DebugOut(_T("Default Adapter is set to Reference Driver (d3dmref.dll). This is not allowed!"));
            SetResult(Result, TPR_FAIL);
        }
        OutputAdapterIdentifier(_T("Default Adapter"), &AdaptDefault);
	}

	if (SUCCEEDED(hr2))
	{
	    if (!memcmp(&AdaptEmpty, &AdaptSoftware, sizeof(AdaptSoftware)))
	    {
	        DebugOut(DO_ERROR, L"Software Adapter Identifier is completely blank. GUID must be set properly. Failing.");
	        SetResult(Result, TPR_FAIL);
	    }
	    OutputAdapterIdentifier(_T("Software Adapter"), &AdaptSoftware);
	}

	//
	// All failure conditions have already returned.
	//
	return Result;
}
#pragma prefast(pop)

//
// ExecuteGetAdapterModeCountTest
//
//   Verify IDirect3DMobile::GetAdapterModeCount
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileTest::ExecuteGetAdapterModeCountTest()
{
    HRESULT hr;
    
	DebugOut(L"Beginning ExecuteGetAdapterModeCountTest.");

	//
	// Count of modes
	//
	UINT uiModeCount;

	//
	// Iterator for individual modes
	//
	UINT uiModeIter;
	
	//
	// Iterator for adapter indices
	//
	UINT uiAdapterIndex;

	//
	// Queried mode
	//
	D3DMDISPLAYMODE CurrentMode;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	for (uiAdapterIndex = D3DMADAPTER_DEFAULT; uiAdapterIndex <= D3DMADAPTER_REGISTERED_DEVICE; uiAdapterIndex++)
	{
		uiModeCount = m_pD3D->GetAdapterModeCount(uiAdapterIndex); // UINT Adapter

		for (uiModeIter = 0; uiModeIter < uiModeCount; uiModeIter++)
		{

			if (FAILED(hr = m_pD3D->EnumAdapterModes(uiAdapterIndex, // UINT Adapter,
			                                    uiModeIter,     // UINT Mode,
			                                   &CurrentMode)))  // D3DDISPLAYMODE* pMode
			{
				DebugOut(DO_ERROR, L"EnumAdapterModes failed. (hr = 0x%08x) Failing", hr);
				return TPR_FAIL;
			}
		}
	}

	//
	// Bad parameter test #1:  Invalid adapter index
	//
	if (SUCCEEDED(hr = m_pD3D->EnumAdapterModes(0xFFFFFFFF,     // UINT Adapter,
	                                       uiModeIter,     // UINT Mode,
	                                      &CurrentMode)))  // D3DDISPLAYMODE* pMode
	{
		DebugOut(DO_ERROR, L"EnumAdapterModes succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// Bad parameter test #2:  Invalid mode index
	//
	if (SUCCEEDED(hr = m_pD3D->EnumAdapterModes(D3DMADAPTER_DEFAULT, // UINT Adapter,
	                                       0xFFFFFFFF,          // UINT Mode,
	                                      &CurrentMode)))       // D3DDISPLAYMODE* pMode
	{
		DebugOut(DO_ERROR, L"EnumAdapterModes succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// Bad parameter test #3:  Invalid adapter index
	//
	if (0 != m_pD3D->GetAdapterModeCount(0xFFFFFFFF)) // UINT Adapter
	{
		DebugOut(DO_ERROR, L"GetAdapterModeCount succeeded unexpectedly. Failing.");
		return TPR_FAIL;
	}

	//
	// Be sure to cover code paths for both valid adapter indices, regardless of
	// existence of such an adapter
	//
	m_pD3D->EnumAdapterModes(D3DMADAPTER_DEFAULT,     // UINT Adapter,
	                         0,                       // UINT Mode,
	                         &CurrentMode);           // D3DDISPLAYMODE* pMode

	m_pD3D->EnumAdapterModes(D3DMADAPTER_REGISTERED_DEVICE, // UINT Adapter,
	                         0,                             // UINT Mode,
	                         &CurrentMode);                 // D3DDISPLAYMODE* pMode
	
	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteEnumAdapterModesTest
//
//   Verify IDirect3DMobile::EnumAdapterModes
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileTest::ExecuteEnumAdapterModesTest()
{
    HRESULT hr;
    
	DebugOut(L"Beginning ExecuteEnumAdapterModesTest.");

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Bad parameter tests
	//

	//
	// Test #1:  NULL D3DDISPLAYMODE pointer
	//
	if (SUCCEEDED(hr = m_pD3D->EnumAdapterModes(D3DMADAPTER_DEFAULT,// UINT Adapter,
	                                       0,                  // UINT Mode,
	                                       NULL)))             // D3DDISPLAYMODE* pMode
	{
		DebugOut(DO_ERROR, L"EnumAdapterModes succeeded; failure expected. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// More thorough testing occurs in a closely-related test:
	//
	return ExecuteGetAdapterModeCountTest();
}

//
// ExecuteGetAdapterDisplayModeTest
//
//   Verify IDirect3DMobile::GetAdapterDisplayMode
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileTest::ExecuteGetAdapterDisplayModeTest()
{
    HRESULT hr;
    
	DebugOut(L"Beginning ExecuteGetAdapterDisplayModeTest.");

	//
	// Count of modes
	//
	UINT uiModeCount;

	//
	// Iterator for individual modes
	//
	UINT uiModeIter;
	
	//
	// Iterator for adapter indices
	//
	UINT uiAdapterIndex;

	//
	// Actual number of adapters reported by GetAdapterCount
	//
	UINT uiActualAdapters;

	//
	// Queried mode
	//
	D3DMDISPLAYMODE CurrentMode;
	D3DMDISPLAYMODE AdapterDisplay;

	//
	// Indicates whether or not a match has been found for the mode
	// reported by GetAdapterDisplayMode, in EnumAdapterModes
	// 
	BOOL bFoundMatch = FALSE;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	uiActualAdapters = m_pD3D->GetAdapterCount();

	for (uiAdapterIndex = D3DMADAPTER_DEFAULT; uiAdapterIndex < uiActualAdapters; uiAdapterIndex++)
	{

		if (FAILED(hr = m_pD3D->GetAdapterDisplayMode(uiAdapterIndex,  // UINT Adapter
		                                         &AdapterDisplay)))// D3DDISPLAYMODE* pMode
		{
			DebugOut(DO_ERROR, L"GetAdapterDisplayMode failed. (hr = 0x%08x) Failing", hr);
			return TPR_FAIL;
		}

		uiModeCount = m_pD3D->GetAdapterModeCount(uiAdapterIndex); // UINT Adapter

		for (uiModeIter = 0; uiModeIter < uiModeCount; uiModeIter++)
		{
			if (FAILED(hr = m_pD3D->EnumAdapterModes(uiAdapterIndex, // UINT Adapter,
			                                    uiModeIter,     // UINT Mode,
			                                   &CurrentMode)))  // D3DMDISPLAYMODE* pMode
			{
				DebugOut(DO_ERROR, L"EnumAdapterModes failed. (hr = 0x%08x) Failing", hr);
				return TPR_FAIL;
			}

			if (0 == memcmp(&CurrentMode, &AdapterDisplay, sizeof(D3DMDISPLAYMODE)))
				bFoundMatch = TRUE;
		}

		if (FALSE == bFoundMatch)
		{
			//
			// If no match was found, for this adapter, fail the test
			//
			return TPR_FAIL;
		}
		else
		{
			//
			// Proceed with the next adapter; reset the "found match" BOOL
			//
			bFoundMatch = FALSE;
		}
	}

	//
	// Bad parameter test #1:  Invalid adapter index
	//
	if (SUCCEEDED(hr = m_pD3D->GetAdapterDisplayMode(0xFFFFFFFF,           // UINT Adapter
	                                            &AdapterDisplay)))    // D3DDISPLAYMODE* pMode
	{
		DebugOut(DO_ERROR, L"GetAdapterDisplayMode succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// Bad parameter test #2:  Invalid D3DDISPLAYMODE*
	//
	if (SUCCEEDED(hr = m_pD3D->GetAdapterDisplayMode(D3DMADAPTER_DEFAULT,  // UINT Adapter
	                                            NULL)))               // D3DDISPLAYMODE* pMode
	{
		DebugOut(DO_ERROR, L"GetAdapterDisplayMode succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}


	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteCheckDeviceTypeTest
//
//   Verify IDirect3DMobile::CheckDeviceType
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileTest::ExecuteCheckDeviceTypeTest()
{
    HRESULT hr;
    
	DebugOut(L"Beginning ExecuteCheckDeviceTypeTest.");

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	if (E_NOTIMPL != (hr = m_pD3D->CheckDeviceType(D3DMADAPTER_DEFAULT, // UINT Adapter,
	                                         D3DMDEVTYPE_DEFAULT, // D3DMDEVTYPE CheckType,
	                                         D3DMFMT_UNKNOWN,     // D3DMFORMAT DisplayFormat,
	                                         D3DMFMT_UNKNOWN,     // D3DMFORMAT BackBufferFormat,
	                                         FALSE)))              // BOOL Windowed
	{
		DebugOut(DO_ERROR, L"Unexpected result for CheckDeviceType (expecting E_NOTIMPL). (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteCheckDeviceFormatTest
//
//   Verify IDirect3DMobile::CheckDeviceFormat
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileTest::ExecuteCheckDeviceFormatTest()
{
	DebugOut(L"Beginning ExecuteCheckDeviceFormatTest.");

	//
	// Iterators for formats, resource types
	//
	D3DMFORMAT FormatIter;
	UINT ResTypeIter;
	D3DMRESOURCETYPE ResType;
	CONST UINT uiNumResTypes = 4;

	//
	// API Results
	//
	HRESULT hr;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	for (FormatIter = (D3DMFORMAT)0; FormatIter < D3DMFMT_NUMFORMAT; (*(ULONG*)(&FormatIter))++)
	{
		for (ResTypeIter = 0; ResTypeIter < uiNumResTypes; ResTypeIter++)
		{
			switch(ResTypeIter)
			{
			case 0:
				ResType = D3DMRTYPE_SURFACE;
				break;
			case 1:
				ResType = D3DMRTYPE_TEXTURE;
				break;
			case 2:
				ResType = D3DMRTYPE_VERTEXBUFFER;
				break;
			case 3:
				ResType = D3DMRTYPE_INDEXBUFFER;
				break;
			}

			hr = m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal, // UINT Adapter
			                               m_DevType,          // D3DMDEVTYPE DeviceType
			                               FormatIter,         // D3DMFORMAT AdapterFormat
			                               0,                  // ULONG Usage
			                               ResType,            // D3DMRESOURCETYPE RType
			                               FormatIter);        // D3DMFORMAT CheckFormat

			
			//
			// If CheckDeviceFormat reported support; make sure that the inputs were sensible.
			// If not, there must be an error.
			//
			if (SUCCEEDED(hr))
			{
				//
				// Verify that CheckDeviceFormat output, for D3DMRTYPE_SURFACE, is possible
				//
				if (D3DMRTYPE_SURFACE == ResType)
				{
					if (!((FormatIter > D3DMFMT_UNKNOWN) && (FormatIter < D3DMFMT_INDEX16)))
					{
						DebugOut(DO_ERROR, L"CheckDeviceFormat succeeded; failure expected. Failing.");
						return TPR_FAIL;
					}
				}

				//
				// Verify that CheckDeviceFormat output, for D3DMRTYPE_TEXTURE, is possible
				//
				if (D3DMRTYPE_TEXTURE == ResType)
				{
					if (!((FormatIter > D3DMFMT_UNKNOWN) && (FormatIter < D3DMFMT_INDEX16)))
					{
						DebugOut(DO_ERROR, L"CheckDeviceFormat succeeded; failure expected. Failing.");
						return TPR_FAIL;
					}
				}

				//
				// If middleware is debug, it screen for disallowed restypes
				//
				if (m_bDebugMiddleware)
				{
					//
					// Verify that CheckDeviceFormat output, for D3DMRTYPE_VERTEXBUFFER, is possible
					//
					if (D3DMRTYPE_VERTEXBUFFER == ResType)
					{
						//
						// CheckDeviceFormat is not for use with the vertex buffer resource type
						//
						DebugOut(DO_ERROR, L"CheckDeviceFormat succeeded; failure expected. Failing.");
						return TPR_FAIL;
					}

					//
					// Verify that CheckDeviceFormat output, for D3DMRTYPE_INDEXBUFFER, is possible
					//
					if (D3DMRTYPE_INDEXBUFFER == ResType)
					{
						//
						// CheckDeviceFormat is not for use with the index buffer resource type
						//
						DebugOut(DO_ERROR, L"CheckDeviceFormat succeeded; failure expected. Failing.");
						return TPR_FAIL;
					}
				}
			}
		}
	}
		
	//
	// Bad parameter test #1:  Invalid adapter index
	//
	if (SUCCEEDED(hr = m_pD3D->CheckDeviceFormat(0xFFFFFFFF,         // UINT Adapter
	                                        m_DevType,          // D3DMDEVTYPE DeviceType
	                                        D3DMFMT_R8G8B8,     // D3DMFORMAT AdapterFormat
	                                        0,                  // ULONG Usage
	                                        D3DMRTYPE_TEXTURE,  // D3DMRESOURCETYPE RType
	                                        D3DMFMT_R8G8B8)))   // D3DMFORMAT CheckFormat
	{
		DebugOut(DO_ERROR, L"CheckDeviceFormat succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// Bad parameter test #2:  Invalid device type index
	//
	if (SUCCEEDED(hr = m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal,      // UINT Adapter
	                                        (D3DMDEVTYPE)0xFFFFFFFF, // D3DMDEVTYPE DeviceType
	                                        D3DMFMT_R8G8B8,          // D3DMFORMAT AdapterFormat
	                                        0,                       // ULONG Usage
	                                        D3DMRTYPE_TEXTURE,       // D3DMRESOURCETYPE RType
	                                        D3DMFMT_R8G8B8)))        // D3DMFORMAT CheckFormat
	{
		DebugOut(DO_ERROR, L"CheckDeviceFormat succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// Bad parameter test #3:  Invalid usage
	//
	if (SUCCEEDED(hr = m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal, // UINT Adapter
	                                        m_DevType,          // D3DMDEVTYPE DeviceType
	                                        D3DMFMT_R8G8B8,     // D3DMFORMAT AdapterFormat
	                                        0xFFFFFFFF,         // ULONG Usage
	                                        D3DMRTYPE_TEXTURE,  // D3DMRESOURCETYPE RType
	                                        D3DMFMT_R8G8B8)))   // D3DMFORMAT CheckFormat
	{
		DebugOut(DO_ERROR, L"CheckDeviceFormat succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}


	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteCheckDeviceMultiSampleTypeTest
//
//   Verify IDirect3DMobile::CheckDeviceMultiSampleType
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileTest::ExecuteCheckDeviceMultiSampleTypeTest()
{
    HRESULT hr;
    
	DebugOut(L"Beginning ExecuteCheckDeviceMultiSampleTypeTest.");
	
	//
	// Iterator for adapter indices
	//
	UINT uiAdapterIndex;

	//
	// API result status codes
	//
	HRESULT hr1, hr2;

	//
	// Iterator for multisample types
	//
	D3DMMULTISAMPLE_TYPE MultiSampleIter;

	//
	// Display mode, used to determine a format to use
	//
	D3DMDISPLAYMODE Mode;

	//
	// Presentation parameters for device resets
	//
	D3DMPRESENT_PARAMETERS PresentParms;

	//
	// Actual number of adapters reported by GetAdapterCount
	//
	UINT uiActualAdapters;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Retrieve current display mode
	//
	if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// Bad parameter test #1: Invalid adapter
	//
	if (SUCCEEDED(hr = m_pD3D->CheckDeviceMultiSampleType(0xFFFFFFFF,            // UINT Adapter
	                                                 m_DevType,             // D3DMDEVTYPE DeviceType
	                                                 Mode.Format,           // D3DMFORMAT SurfaceFormat
	                                                 FALSE,                 // BOOL Windowed
	                                                 D3DMMULTISAMPLE_NONE)))// D3DMMULTISAMPLE_TYPE MultiSampleType
	{
		DebugOut(DO_ERROR, L"CheckDeviceMultiSampleType succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// Bad parameter test #2: Invalid device ordinal
	//
	if (SUCCEEDED(hr = m_pD3D->CheckDeviceMultiSampleType(m_uiAdapterOrdinal,      // UINT Adapter
	                                                 (D3DMDEVTYPE)0xFFFFFFFF, // D3DMDEVTYPE DeviceType
	                                                 Mode.Format,             // D3DMFORMAT SurfaceFormat
	                                                 FALSE,                   // BOOL Windowed
	                                                 D3DMMULTISAMPLE_NONE)))  // D3DMMULTISAMPLE_TYPE MultiSampleType
	{
		DebugOut(DO_ERROR, L"CheckDeviceMultiSampleType succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// Bad parameter test #3: Invalid format
	//
	if (SUCCEEDED(hr = m_pD3D->CheckDeviceMultiSampleType(m_uiAdapterOrdinal,      // UINT Adapter
	                                                 m_DevType,               // D3DMDEVTYPE DeviceType
	                                                 D3DMFMT_UNKNOWN,         // D3DMFORMAT SurfaceFormat
	                                                 FALSE,                   // BOOL Windowed
	                                                 D3DMMULTISAMPLE_NONE)))  // D3DMMULTISAMPLE_TYPE MultiSampleType
	{
		DebugOut(DO_ERROR, L"CheckDeviceMultiSampleType succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// Clear out presentation parameters structure
	//
	ZeroMemory( &PresentParms, sizeof(PresentParms) );

	//
	// Prepare presentation intervals for use with IDirect3DMobileDevice::Reset
	//
	// Note: Multisampling is supported only if the swap effect is D3DMSWAPEFFECT_DISCARD. 
	//
	PresentParms.Windowed   = FALSE;
	PresentParms.SwapEffect = D3DMSWAPEFFECT_DISCARD;
	PresentParms.BackBufferFormat = Mode.Format;
	PresentParms.BackBufferCount = 1;
	PresentParms.BackBufferWidth = Mode.Width;
	PresentParms.BackBufferHeight = Mode.Height;
	PresentParms.FullScreen_PresentationInterval = D3DMPRESENT_INTERVAL_DEFAULT;

	//
	// For this test, we must create a device specifically in fullscreen mode.  Release & create
	// to be sure.
	//
	if (0 != m_pd3dDevice->Release())
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::Release returned non-zero reference count; fatal error. Failing.");
		return TPR_FAIL;
	}

	//
	// In case of unexpected failure, ensure that device interface pointer is cleaned up
	//
	m_pd3dDevice = NULL;

	//
	// Create a device with D3DMSWAPEFFECT_DISCARD; other swap effects prevent rigorous testing
	// of IDirect3DMobile::CheckDeviceMultiSampleType with IDirect3DMobileDevice::Reset
	//
	if( FAILED(hr = m_pD3D->CreateDevice( m_uiAdapterOrdinal,// UINT Adapter,
	                                  m_DevType,         // D3DMDEVTYPE DeviceType,
	                                  m_hWnd,            // HWND hFocusWindow,
	                                  m_dwBehaviorFlags, // ULONG BehaviorFlags,
	                                 &PresentParms,      // D3DMPRESENT_PARAMETERS* pPresentationParameters,
	                                 &m_pd3dDevice )))   // IDirect3DMobileDevice** ppReturnedDeviceInterface
	{
		DebugOut(DO_ERROR, L"IDirect3DMobile::CreateDevice failed. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	uiActualAdapters = m_pD3D->GetAdapterCount();

	//
	// Verify, for each valid adapter index, that D3DMMULTISAMPLE_NONE is reported as supported
	//
	for (uiAdapterIndex = D3DMADAPTER_DEFAULT; uiAdapterIndex < uiActualAdapters; uiAdapterIndex++)
	{

		if (FAILED(hr = m_pD3D->CheckDeviceMultiSampleType(uiAdapterIndex,        // UINT Adapter
		                                              D3DMDEVTYPE_DEFAULT,   // D3DMDEVTYPE DeviceType
		                                              D3DMFMT_R8G8B8,        // UNUSED - D3DMFORMAT SurfaceFormat
		                                              FALSE,                 // UNUSED - BOOL Windowed
		                                              D3DMMULTISAMPLE_NONE)))// D3DMMULTISAMPLE_TYPE MultiSampleType
		{
			DebugOut(DO_ERROR, L"CheckDeviceMultiSampleType failed with D3DMMULTISAMPLE_NONE; fatal error. (hr = 0x%08x) Failing", hr);
			return TPR_FAIL;
		}
	}

	//
	// For currently instantiated adapter index, verify consistency between:
	//
	//    * Result of IDirect3DMobile::CheckDeviceMultiSampleType
	//    * Result of IDirect3DMobileDevice::Reset with multisample type
	//
	for (MultiSampleIter = D3DMMULTISAMPLE_2_SAMPLES; MultiSampleIter < D3DMMULTISAMPLE_16_SAMPLES; (*(ULONG*)(&MultiSampleIter))++)
	{

		hr1 = m_pD3D->CheckDeviceMultiSampleType(m_uiAdapterOrdinal, // UINT Adapter
		                                         D3DMDEVTYPE_DEFAULT,// D3DMDEVTYPE DeviceType
		                                         D3DMFMT_R8G8B8,     // UNUSED - D3DMFORMAT SurfaceFormat
		                                         FALSE,              // UNUSED - BOOL Windowed
		                                         MultiSampleIter);   // D3DMMULTISAMPLE_TYPE MultiSampleType

		//
		// Attempt a device reset with the specified multisample type
		//
		PresentParms.MultiSampleType = MultiSampleIter;

		hr2 = m_pd3dDevice->Reset(&PresentParms);

		//
		// Analyze all four of the possible outcomes
		//
		if ((FAILED(hr1)) && (FAILED(hr2)))
		{
			DebugOut(DO_ERROR, L"CheckDeviceMultiSampleType reported the multisample type as unsupported; IDirect3DMobileDevice::Reset failed as expected.");
		}
		else if ((SUCCEEDED(hr1)) && (SUCCEEDED(hr2)))
		{
			DebugOut(DO_ERROR, L"CheckDeviceMultiSampleType reported the multisample type as supported; IDirect3DMobileDevice::Reset succeeded as expected.");
		}
		else if ((FAILED(hr1)) && (SUCCEEDED(hr2)))
		{
			DebugOut(DO_ERROR, L"CheckDeviceMultiSampleType reported the multisample type as unsupported; but IDirect3DMobileDevice::Reset succeeded (unexpected). Failing");
			return TPR_FAIL;
		}
		else if ((SUCCEEDED(hr1)) && (FAILED(hr2)))
		{
			DebugOut(DO_ERROR, L"CheckDeviceMultiSampleType reported the multisample type as supported; but IDirect3DMobileDevice::Reset failed (unexpected). (hr = 0x%08x) Failing", hr);
			return TPR_FAIL;
		}

	}

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteCheckDepthStencilMatchTest
//
//   Verify IDirect3DMobile::CheckDepthStencilMatch
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileTest::ExecuteCheckDepthStencilMatchTest()
{
	DebugOut(L"Beginning ExecuteCheckDepthStencilMatchTest.");

	//
	// Actual number of adapters reported by GetAdapterCount
	//
	UINT uiActualAdapters;

	//
	// Iterator for adapter indices
	//
	UINT uiAdapterIndex;

	//
	// Display modes for adapters
	//
	D3DMDISPLAYMODE Mode1, Mode2;

	//
	// Iterators for surface formats
	//
	D3DMFORMAT FormatIterRT, FormatIterDS;

	//
	// API result status codes
	//
	HRESULT hr;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	uiActualAdapters = m_pD3D->GetAdapterCount();

	//
	// Retrieve current display mode for default adapter
	//
	if (FAILED(hr = m_pD3D->GetAdapterDisplayMode( D3DMADAPTER_DEFAULT,// UINT Adapter
	                                         &Mode1)))            // D3DDISPLAYMODE* pMode
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// Retrieve current display mode for registered device, if present
	//
	if (2 == uiActualAdapters)
	{
		if (FAILED(hr = m_pD3D->GetAdapterDisplayMode( D3DMADAPTER_REGISTERED_DEVICE,// UINT Adapter
		                                         &Mode2)))                      // D3DDISPLAYMODE* pMode
		{
			DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode. (hr = 0x%08x) Failing", hr);
			return TPR_FAIL;
		}
	}

	for (uiAdapterIndex = D3DMADAPTER_DEFAULT; uiAdapterIndex < uiActualAdapters; uiAdapterIndex++)
	{
		for (FormatIterRT = (D3DMFORMAT)0; FormatIterRT < D3DMFMT_NUMFORMAT; (*(ULONG*)(&FormatIterRT))++)
		{
			for (FormatIterDS = (D3DMFORMAT)0; FormatIterDS < D3DMFMT_NUMFORMAT; (*(ULONG*)(&FormatIterDS))++)
			{

				hr = m_pD3D->CheckDepthStencilMatch( uiAdapterIndex,                              // UINT Adapter
				                                     D3DMDEVTYPE_DEFAULT,                         // D3DMDEVTYPE DeviceType
				                                     uiAdapterIndex ? Mode2.Format : Mode1.Format,// D3DMFORMAT AdapterFormat
				                                     FormatIterRT,                                // D3DMFORMAT RenderTargetFormat
				                                     FormatIterDS);                               // D3DMFORMAT DepthStencilFormat


				//
				// If CheckDepthStencilMatch reported support; make sure that the inputs were sensible.
				// If not, there must be an error.
				//
				if (SUCCEEDED(hr))
				{
					//
					// Verify that CheckDepthStencilMatch output is possible
					//
					if (!((FormatIterRT >= D3DMFMT_R8G8B8) && (FormatIterRT <= D3DMFMT_DXT5)))
					{
						DebugOut(DO_ERROR, L"CheckDepthStencilMatch succeeded; failure expected. Failing.");
						return TPR_FAIL;
					}
					if (!((FormatIterDS >= D3DMFMT_D32) && (FormatIterDS <= D3DMFMT_D24X4S4)))
					{
						DebugOut(DO_ERROR, L"CheckDepthStencilMatch succeeded; failure expected. Failing.");
						return TPR_FAIL;
					}
				}


				//
				// Bad parameter test #1: Invalid device type
				//
				if (SUCCEEDED(hr = m_pD3D->CheckDepthStencilMatch( uiAdapterIndex,                              // UINT Adapter
				                                              (D3DMDEVTYPE)0xFFFFFFFF,                     // D3DMDEVTYPE DeviceType
				                                              uiAdapterIndex ? Mode2.Format : Mode1.Format,// D3DMFORMAT AdapterFormat
				                                              FormatIterRT,                                // D3DMFORMAT RenderTargetFormat
				                                              FormatIterDS)))                              // D3DMFORMAT DepthStencilFormat
				{
					DebugOut(DO_ERROR, L"CheckDepthStencilMatch succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
					return TPR_FAIL;
				}

				//
				// Bad parameter test #2: Invalid adapter index
				//
				if (SUCCEEDED(hr = m_pD3D->CheckDepthStencilMatch( 0xFFFFFFFF,                                  // UINT Adapter
				                                              D3DMDEVTYPE_DEFAULT,                         // D3DMDEVTYPE DeviceType
				                                              uiAdapterIndex ? Mode2.Format : Mode1.Format,// D3DMFORMAT AdapterFormat
				                                              FormatIterRT,                                // D3DMFORMAT RenderTargetFormat
				                                              FormatIterDS)))                              // D3DMFORMAT DepthStencilFormat
				{
					DebugOut(DO_ERROR, L"CheckDepthStencilMatch succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
					return TPR_FAIL;
				}

			}

		}
	}

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteCheckProfileTest
//
//   Verify IDirect3DMobile::CheckProfile
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileTest::ExecuteCheckProfileTest()
{
    HRESULT hr;
    
	DebugOut(L"Beginning ExecuteCheckProfileTest.");

	//
	// Actual number of adapters reported by GetAdapterCount
	//
	UINT uiActualAdapters;
	
	//
	// Iterator for adapter indices
	//
	UINT uiAdapterIndex;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Test result for profile conformance
	//
	INT BaseProfileTestResult;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	uiActualAdapters = m_pD3D->GetAdapterCount();

	//
	// Verify that the base profile is supported on all valid adapters; and that a non-existent
	// profile is reported as not supported.
	//
	for (uiAdapterIndex = D3DMADAPTER_DEFAULT; uiAdapterIndex < uiActualAdapters; uiAdapterIndex++)
	{

		if (FAILED(hr = m_pD3D->GetDeviceCaps( uiAdapterIndex,      // UINT Adapter,
										  D3DMDEVTYPE_DEFAULT, // D3DMDEVTYPE DeviceType
										 &Caps)))              // D3DMCAPS* pCaps
		{
			DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x) Failing", hr);
			return TPR_FAIL;
		}

		//
		// Confirm that device adheres to D3DMPROFILE_BASE requirements.
		//
		// Return value is a TPR_* code, not an HRESULT
		//
		BaseProfileTestResult = ExecuteConformsToBaseProfileTest(Caps);

		//
		// If non-passing, return
		//
		if (TPR_PASS != BaseProfileTestResult)
			return BaseProfileTestResult;

		if (FAILED(hr = m_pD3D->CheckProfile(uiAdapterIndex,       // UINT Adapter
		                                D3DMDEVTYPE_DEFAULT, // D3DMDEVTYPE DeviceType
		                                D3DMPROFILE_BASE)))  // D3DMPROFILE Profile
		{
			DebugOut(DO_ERROR, L"Device does not support base profile.  Fatal error. (hr = 0x%08x) Failing", hr);
			return TPR_FAIL;
		}

		//
		// Bad parameter test #1:  Invalid profile identifier
		//
		if (SUCCEEDED(hr = m_pD3D->CheckProfile( uiAdapterIndex,           // UINT Adapter
		                                    D3DMDEVTYPE_DEFAULT,      // D3DMDEVTYPE DeviceType
		                                   (D3DMPROFILE)0xFFFFFFFF))) // D3DMPROFILE Profile
		{
			DebugOut(DO_ERROR, L"Device claims to support non-existent profile.  Fatal error. (hr = 0x%08x) Failing", hr);
			return TPR_FAIL;
		}
	}


	//
	// Bad parameter test #2:  Invalid adapter index
	//
	if (SUCCEEDED(hr = m_pD3D->CheckProfile(0xFFFFFFFF,               // UINT Adapter
	                                   D3DMDEVTYPE_DEFAULT,      // D3DMDEVTYPE DeviceType
	                                   D3DMPROFILE_BASE)))       // D3DMPROFILE Profile
	{
		DebugOut(DO_ERROR, L"CheckProfile succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// Bad parameter test #3:  Invalid device type index
	//
	if (SUCCEEDED(hr = m_pD3D->CheckProfile( D3DMADAPTER_DEFAULT,     // UINT Adapter
	                                   (D3DMDEVTYPE)0xFFFFFFFF,  // D3DMDEVTYPE DeviceType
	                                    D3DMPROFILE_BASE)))      // D3DMPROFILE Profile
	{
		DebugOut(DO_ERROR, L"CheckProfile succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteGetDeviceCapsTest
//
//   Verify IDirect3DMobile::GetDeviceCaps
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileTest::ExecuteGetDeviceCapsTest()
{
    HRESULT hr;
    
	DebugOut(L"Beginning ExecuteGetDeviceCapsTest.");

	//
	// Device capabilities
	//
	D3DMCAPS Caps1, Caps2;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	if (FAILED(hr = m_pD3D->GetDeviceCaps( m_uiAdapterOrdinal,  // UINT Adapter,
	                                  D3DMDEVTYPE_DEFAULT, // D3DMDEVTYPE DeviceType
	                                 &Caps1)))             // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps2)))  // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	if (0 != memcmp(&Caps1, &Caps2, sizeof(D3DMCAPS)))
	{
		DebugOut(DO_ERROR, L"D3DMCAPS mismatch. Failing.");
		return TPR_FAIL;
	}

	if (D3DMDEVTYPE_DEFAULT != Caps1.DeviceType)
	{
		DebugOut(DO_ERROR, L"Invalid data: D3DMCAPS.DeviceType. Failing.");
		return TPR_FAIL;
	}

	if (m_uiAdapterOrdinal != Caps1.AdapterOrdinal)
	{
		DebugOut(DO_ERROR, L"Invalid data: D3DMCAPS.AdapterOrdinal. Failing");
		return TPR_FAIL;
	}


	//
	// Bad parameter test #1:  Invalid adapter index
	//
	if (SUCCEEDED(hr = m_pD3D->GetDeviceCaps(0xFFFFFFFF,          // UINT Adapter,
	                                    D3DMDEVTYPE_DEFAULT, // D3DMDEVTYPE DeviceType
	                                    &Caps1)))            // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// Bad parameter test #2:  Invalid device type index
	//
	if (SUCCEEDED(hr = m_pD3D->GetDeviceCaps(m_uiAdapterOrdinal,      // UINT Adapter,
	                                    (D3DMDEVTYPE)0xFFFFFFFF, // D3DMDEVTYPE DeviceType
	                                    &Caps1)))                // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}

//
// ExecuteCreateDeviceTest
//
//   Verify IDirect3DMobile::CreateDevice
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileTest::ExecuteCreateDeviceTest()
{
    HRESULT hr;
    
	DebugOut(L"Beginning ExecuteCreateDeviceTest.");

	//
	// Display mode, used to determine a format to use
	//
	D3DMDISPLAYMODE Mode;

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		return TPR_SKIP;
	}

	//
	// Retrieve current display mode
	//
	if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
	{
		DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// Release existing device to prepare for CreateDevice testing
	//
	if (0 != m_pd3dDevice->Release())
	{
		DebugOut(DO_ERROR, L"IDirect3DMobileDevice::Release returned non-zero reference count; fatal error. Failing.");
		return TPR_FAIL;
	}
	m_pd3dDevice = NULL;

	//
	// Bad parameter tests
	//

	//
	// Test #1:  Invalid IDirect3DMobileDevice**
	//
	if( SUCCEEDED( hr = m_pD3D->CreateDevice( m_uiAdapterOrdinal,// UINT Adapter,
	                                     m_DevType,         // D3DMDEVTYPE DeviceType,
	                                     m_hWnd,            // HWND hFocusWindow,
	                                     m_dwBehaviorFlags, // ULONG BehaviorFlags,
	                                    &m_d3dpp,           // D3DMPRESENT_PARAMETERS* pPresentationParameters,
	                                     NULL )))           // IDirect3DMobileDevice** ppReturnedDeviceInterface
	{
		DebugOut(DO_ERROR, L"CreateDevice succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// Test #2:  Invalid D3DMPRESENT_PARAMETERS*
	//
	if( SUCCEEDED( hr = m_pD3D->CreateDevice( m_uiAdapterOrdinal,// UINT Adapter,
	                                     m_DevType,         // D3DMDEVTYPE DeviceType,
	                                     m_hWnd,            // HWND hFocusWindow,
	                                     m_dwBehaviorFlags, // ULONG BehaviorFlags,
	                                     NULL,              // D3DMPRESENT_PARAMETERS* pPresentationParameters,
	                                    &m_pd3dDevice)))    // IDirect3DMobileDevice** ppReturnedDeviceInterface
	{
		DebugOut(DO_ERROR, L"CreateDevice succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}


	//
	// Test #3:  Invalid HWND
	//
	if( SUCCEEDED( hr = m_pD3D->CreateDevice( m_uiAdapterOrdinal,// UINT Adapter,
	                                     m_DevType,         // D3DMDEVTYPE DeviceType,
	                                     (HWND)0xFFFFFFFF,  // HWND hFocusWindow,
	                                     m_dwBehaviorFlags, // ULONG BehaviorFlags,
	                                    &m_d3dpp,           // D3DMPRESENT_PARAMETERS* pPresentationParameters,
	                                    &m_pd3dDevice)))    // IDirect3DMobileDevice** ppReturnedDeviceInterface
	{
		DebugOut(DO_ERROR, L"CreateDevice succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// Test #4:  Invalid adapter ordinal
	//
	if( SUCCEEDED( hr = m_pD3D->CreateDevice( 0xFFFFFFFF,        // UINT Adapter,
	                                     m_DevType,         // D3DMDEVTYPE DeviceType,
	                                     m_hWnd,            // HWND hFocusWindow,
	                                     m_dwBehaviorFlags, // ULONG BehaviorFlags,
	                                    &m_d3dpp,           // D3DMPRESENT_PARAMETERS* pPresentationParameters,
	                                    &m_pd3dDevice)))    // IDirect3DMobileDevice** ppReturnedDeviceInterface
	{
		DebugOut(DO_ERROR, L"CreateDevice succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// Test #5:  Invalid behavior flags
	//
	if( SUCCEEDED( hr = m_pD3D->CreateDevice( m_uiAdapterOrdinal,// UINT Adapter,
	                                     m_DevType,         // D3DMDEVTYPE DeviceType,
	                                     m_hWnd,            // HWND hFocusWindow,
	                                     0xFFFFFFFF,        // ULONG BehaviorFlags,
	                                    &m_d3dpp,           // D3DMPRESENT_PARAMETERS* pPresentationParameters,
	                                    &m_pd3dDevice)))    // IDirect3DMobileDevice** ppReturnedDeviceInterface
	{
		DebugOut(DO_ERROR, L"CreateDevice succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// Test #6:  Invalid D3DMDEVTYPE
	//
	if( SUCCEEDED( hr = m_pD3D->CreateDevice( m_uiAdapterOrdinal,      // UINT Adapter,
	                                     (D3DMDEVTYPE)0xFFFFFFFF, // D3DMDEVTYPE DeviceType,
	                                     m_hWnd,                  // HWND hFocusWindow,
	                                     m_dwBehaviorFlags,       // ULONG BehaviorFlags,
	                                    &m_d3dpp,                 // D3DMPRESENT_PARAMETERS* pPresentationParameters,
	                                    &m_pd3dDevice)))          // IDirect3DMobileDevice** ppReturnedDeviceInterface
	{
		DebugOut(DO_ERROR, L"CreateDevice succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
		return TPR_FAIL;
	}

	//
	// All failure conditions have already returned.
	//
	return TPR_PASS;
}



//
// ExecuteCheckDeviceFormatConversionTest
//
//   Verify IDirect3DMobile::CheckDeviceFormatConversion
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileTest::ExecuteCheckDeviceFormatConversionTest()
{
	HRESULT hr;

	DebugOut(L"Beginning ExecuteCheckDeviceFormatConversionTest.");

	//
	// Actual number of adapters reported by GetAdapterCount
	//
	UINT uiActualAdapters;

	//
	// Iterator for adapter indices
	//
	UINT uiAdapterIndex;

	//
	// Tux Test Result
	//
	INT Result = TPR_PASS;

	//
	// Source Format and Destination Format for CheckDeviceFormatConversion
	//
	D3DMFORMAT SourceFormat, DestFormat;

	//
	// Adapter display mode
	//
	D3DMDISPLAYMODE DisplayMode;

	//
	// Data points for test to cross-reference
	//
	BOOL bConversionSupported, bSourceSupported, bDestSupported;

	//
	// Special source format cases to track for debug output
	//
	BOOL bSrcFailReasonChkDevFmt;     
	BOOL bSrcFailReasonFmtIndex;      
	BOOL bSrcFailReasonFmtFixedFloat; 
	BOOL bSrcFailReasonFmtVertexData; 
	BOOL bSrcFailReasonFmtUnknown;    
	BOOL bSrcFailReasonFmtInvalid;    

	//
	// Special dest format cases to track for debug output
	//
	BOOL bDestFailReasonYUV;          
	BOOL bDestFailReasonChkDevFmt;    
	BOOL bDestFailReasonFmtIndex;     
	BOOL bDestFailReasonFmtFixedFloat;
	BOOL bDestFailReasonFmtVertexData;
	BOOL bDestFailReasonFmtUnknown;   
	BOOL bDestFailReasonFmtInvalid;   

	if (!IsReady())
	{
		DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	uiActualAdapters = m_pD3D->GetAdapterCount();

	for (uiAdapterIndex = D3DMADAPTER_DEFAULT; uiAdapterIndex < uiActualAdapters; uiAdapterIndex++)
	{

		DebugOut(L"");
		DebugOut(L"======================================================================================");
		DebugOut(L"Now beginning CheckDeviceFormatConversion tests for adapter index:  %s",
			(D3DMADAPTER_DEFAULT == uiAdapterIndex) ? L"D3DMADAPTER_DEFAULT" : L"D3DMADAPTER_REGISTERED_DEVICE");
		DebugOut(L"======================================================================================");
		DebugOut(L"");

		if (FAILED(hr = m_pD3D->GetAdapterDisplayMode(uiAdapterIndex, // UINT Adapter
		                                         &DisplayMode))) // D3DMDISPLAYMODE* pMode
		{
			DebugOut(DO_ERROR, L"GetAdapterDisplayMode failed. (hr = 0x%08x) Aborting.", hr);
			Result = TPR_ABORT;
			goto cleanup;
		}

		for (SourceFormat = (D3DMFORMAT)0; SourceFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&SourceFormat))++)
		{
			for (DestFormat = (D3DMFORMAT)0; DestFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&DestFormat))++)
			{
				bSrcFailReasonChkDevFmt       = FALSE;
				bSrcFailReasonFmtIndex        = FALSE;
				bSrcFailReasonFmtFixedFloat   = FALSE;
				bSrcFailReasonFmtVertexData   = FALSE;
				bSrcFailReasonFmtUnknown      = FALSE;
				bSrcFailReasonFmtInvalid      = FALSE;

				bDestFailReasonYUV            = FALSE;
				bDestFailReasonChkDevFmt      = FALSE;
				bDestFailReasonFmtIndex       = FALSE;
				bDestFailReasonFmtFixedFloat  = FALSE;
				bDestFailReasonFmtVertexData  = FALSE;
				bDestFailReasonFmtUnknown     = FALSE;
				bDestFailReasonFmtInvalid     = FALSE;

				//
				// Can the device perform the format conversion?
				//
				if (FAILED(hr = m_pD3D->CheckDeviceFormatConversion( uiAdapterIndex,                              // UINT Adapter
				                                                D3DMDEVTYPE_DEFAULT,                         // D3DMDEVTYPE DeviceType
				                                                SourceFormat,                                // D3DMFORMAT SourceFormat
				                                                DestFormat)))                                // D3DMFORMAT DestFormat
				{
					bConversionSupported = FALSE;
				}
				else
				{
					bConversionSupported = TRUE;
				}

				//
				// Does the device support creating the proposed source at all?
				//
				switch(SourceFormat)
				{
				case D3DMFMT_UNKNOWN:
					bSrcFailReasonFmtUnknown = TRUE;
					break;

				case D3DMFMT_R8G8B8:  
				case D3DMFMT_A8R8G8B8:
				case D3DMFMT_X8R8G8B8:
				case D3DMFMT_R5G6B5:  
				case D3DMFMT_X1R5G5B5:
				case D3DMFMT_A1R5G5B5:
				case D3DMFMT_A4R4G4B4:
				case D3DMFMT_R3G3B2:  
				case D3DMFMT_A8R3G3B2:
				case D3DMFMT_X4R4G4B4:
				case D3DMFMT_A8P8:    
				case D3DMFMT_P8:      
				case D3DMFMT_A8:      
				case D3DMFMT_UYVY:
				case D3DMFMT_YUY2:
				case D3DMFMT_DXT1:
				case D3DMFMT_DXT2:
				case D3DMFMT_DXT3:
				case D3DMFMT_DXT4:
				case D3DMFMT_DXT5:

					//
					// Does the device support the D3DMRTYPE_SURFACE format at all?
					//
					if (FAILED(hr = m_pD3D->CheckDeviceFormat(uiAdapterIndex,         // UINT Adapter
				                                         D3DMDEVTYPE_DEFAULT,    // D3DMDEVTYPE DeviceType
				                                         DisplayMode.Format,     // D3DMFORMAT AdapterFormat
				                                         0,                      // ULONG Usage
				                                         D3DMRTYPE_SURFACE,      // D3DMRESOURCETYPE RType
				                                         SourceFormat)))         // D3DMFORMAT CheckFormat
					{
						bSrcFailReasonChkDevFmt = TRUE;
					}

					break;

				case D3DMFMT_D32:    
				case D3DMFMT_D15S1:  
				case D3DMFMT_D24S8:  
				case D3DMFMT_D16:    
				case D3DMFMT_D24X8:
				case D3DMFMT_D24X4S4:

					//
					// Does the device support the D3DMRTYPE_DEPTHSTENCIL format at all?
					//
					if (FAILED(hr = m_pD3D->CheckDeviceFormat(uiAdapterIndex,         // UINT Adapter
				                                         D3DMDEVTYPE_DEFAULT,    // D3DMDEVTYPE DeviceType
				                                         DisplayMode.Format,     // D3DMFORMAT AdapterFormat
				                                         0,                      // ULONG Usage
				                                         D3DMRTYPE_DEPTHSTENCIL, // D3DMRESOURCETYPE RType
				                                         SourceFormat)))         // D3DMFORMAT CheckFormat
					{
						bSrcFailReasonChkDevFmt = TRUE;
					}

					break;

				case D3DMFMT_INDEX16:        
				case D3DMFMT_INDEX32:  

					bSrcFailReasonFmtIndex = TRUE;
					break;

				case D3DMFMT_VERTEXDATA:  

					bSrcFailReasonFmtVertexData = TRUE;
					break;

				case D3DMFMT_D3DMVALUE_FLOAT:
				case D3DMFMT_D3DMVALUE_FIXED:

					bSrcFailReasonFmtFixedFloat = TRUE;
					break;

				default:

					bSrcFailReasonFmtInvalid = TRUE;
					break;
				}

				//
				// Does the device support creating the proposed dest at all?
				//
				switch(DestFormat)
				{
				case D3DMFMT_UNKNOWN:
					bDestFailReasonFmtUnknown = TRUE;
					break;

				case D3DMFMT_R8G8B8:  
				case D3DMFMT_A8R8G8B8:
				case D3DMFMT_X8R8G8B8:
				case D3DMFMT_R5G6B5:  
				case D3DMFMT_X1R5G5B5:
				case D3DMFMT_A1R5G5B5:
				case D3DMFMT_A4R4G4B4:
				case D3DMFMT_R3G3B2:  
				case D3DMFMT_A8R3G3B2:
				case D3DMFMT_X4R4G4B4:
				case D3DMFMT_A8P8:    
				case D3DMFMT_P8:      
				case D3DMFMT_A8:
				case D3DMFMT_DXT1:
				case D3DMFMT_DXT2:
				case D3DMFMT_DXT3:
				case D3DMFMT_DXT4:
				case D3DMFMT_DXT5:

					//
					// Does the device support the D3DMRTYPE_SURFACE format at all?
					//
					if (FAILED(hr = m_pD3D->CheckDeviceFormat(uiAdapterIndex,         // UINT Adapter
				                                         D3DMDEVTYPE_DEFAULT,    // D3DMDEVTYPE DeviceType
				                                         DisplayMode.Format,     // D3DMFORMAT AdapterFormat
				                                         0,                      // ULONG Usage
				                                         D3DMRTYPE_SURFACE,      // D3DMRESOURCETYPE RType
				                                         DestFormat)))         // D3DMFORMAT CheckFormat
					{
						bDestFailReasonChkDevFmt = TRUE;
					}

					break;
					
				case D3DMFMT_UYVY:
				case D3DMFMT_YUY2:

					bDestFailReasonYUV = TRUE;
					break;

				case D3DMFMT_D32:    
				case D3DMFMT_D15S1:  
				case D3DMFMT_D24S8:  
				case D3DMFMT_D16:    
				case D3DMFMT_D24X8:
				case D3DMFMT_D24X4S4:

					//
					// Does the device support the D3DMRTYPE_DEPTHSTENCIL format at all?
					//
					if (FAILED(hr = m_pD3D->CheckDeviceFormat(uiAdapterIndex,         // UINT Adapter
				                                         D3DMDEVTYPE_DEFAULT,    // D3DMDEVTYPE DeviceType
				                                         DisplayMode.Format,     // D3DMFORMAT AdapterFormat
				                                         0,                      // ULONG Usage
				                                         D3DMRTYPE_DEPTHSTENCIL, // D3DMRESOURCETYPE RType
				                                         DestFormat)))           // D3DMFORMAT CheckFormat
					{
						bDestFailReasonChkDevFmt = TRUE;
					}

					break;

				case D3DMFMT_INDEX16:        
				case D3DMFMT_INDEX32:  
					bDestFailReasonFmtIndex = TRUE;
					break;

				case D3DMFMT_VERTEXDATA: 
					bDestFailReasonFmtVertexData = TRUE;
					break;

				case D3DMFMT_D3DMVALUE_FLOAT:
				case D3DMFMT_D3DMVALUE_FIXED:
					bDestFailReasonFmtFixedFloat = TRUE;
					break;

				default:
					bDestFailReasonFmtInvalid = TRUE;
					break;
				}

				//
				// Is there any constraint that disqualifies the source format from being useful for StretchRect?
				//
				if ((bSrcFailReasonChkDevFmt    ) ||
					(bSrcFailReasonFmtIndex     ) ||
					(bSrcFailReasonFmtFixedFloat) ||
					(bSrcFailReasonFmtVertexData) ||
					(bSrcFailReasonFmtUnknown   ) ||
					(bSrcFailReasonFmtInvalid   ))
					bSourceSupported = FALSE;
				else
					bSourceSupported = TRUE;

				//
				// Is there any constraint that disqualifies the dest format from being useful for StretchRect?
				//
				if ((bDestFailReasonYUV          ) ||           
					(bDestFailReasonChkDevFmt    ) ||     
					(bDestFailReasonFmtIndex     ) ||      
					(bDestFailReasonFmtFixedFloat) || 
					(bDestFailReasonFmtVertexData) || 
					(bDestFailReasonFmtUnknown   ) ||  
					(bDestFailReasonFmtInvalid   ))
					bDestSupported = FALSE;
				else
					bDestSupported = TRUE;

				//
				// Degenerate conversion should always report as valid, with CheckDeviceFormatConversion, 
				// regardless of whether or not src/dst formats are actually supported
				//
				if (DestFormat == SourceFormat)
				{
					if (FALSE == bConversionSupported)
					{
						DebugOut(L"");
						DebugOut(L"CheckDeviceFormatConversion failed unexpectedly [Source: %s; Dest: %s]  Success expected because:",
							(SourceFormat < D3DMFMT_NUMFORMAT) ? D3DQA_D3DMFMT_NAMES[SourceFormat] : L"?",
							(DestFormat   < D3DMFMT_NUMFORMAT) ? D3DQA_D3DMFMT_NAMES[DestFormat]   : L"?");

						DebugOut(L"  * CheckDeviceFormatConversion should always report success if src and dest formats are identical (regardless of whether or not formats are supported).");
						DebugOut(DO_ERROR, L"");
						//
						// Record failure for return value; but continue execution of remaining permutations
						//
						Result = TPR_FAIL;
					}
				}
				else
				{
					if (bConversionSupported)
					{
						if (FALSE == bSourceSupported)
						{
							DebugOut(L"");
							DebugOut(L"CheckDeviceFormatConversion succeeded unexpectedly [Source: %s; Dest: %s].  Failure expected because:",
								(SourceFormat < D3DMFMT_NUMFORMAT) ? D3DQA_D3DMFMT_NAMES[SourceFormat] : L"?",
								(DestFormat   < D3DMFMT_NUMFORMAT) ? D3DQA_D3DMFMT_NAMES[DestFormat]   : L"?");

							if (bSrcFailReasonChkDevFmt    )
								DebugOut(L"    * CheckDeviceFormat failed for this format.");

							if (bSrcFailReasonFmtIndex     )
								DebugOut(L"    * D3DMFMT_INDEX16 and D3DMFMT_INDEX32 are never a valid formats for use with StretchRect.");

							if (bSrcFailReasonFmtFixedFloat)
								DebugOut(L"    * D3DMFMT_D3DMVALUE_FLOAT and D3DMFMT_D3DMVALUE_FIXED are never a valid formats for use with StretchRect.");

							if (bSrcFailReasonFmtVertexData)
								DebugOut(L"    * D3DMFMT_VERTEXDATA is never a valid format for use with StretchRect.");

							if (bSrcFailReasonFmtUnknown   )
								DebugOut(L"    * D3DMFMT_UNKNOWN is never a valid format for use with StretchRect.");

							if (bSrcFailReasonFmtInvalid   )
								DebugOut(DO_ERROR, L"    * The specified destination format has no corresponding D3DMFORMAT enumerator.");

							//
							// Record failure for return value; but continue execution of remaining permutations
							//
							Result = TPR_FAIL;
							continue;
						}

						if (FALSE == bDestSupported)
						{
							DebugOut(L"");
							DebugOut(L"CheckDeviceFormatConversion succeeded unexpectedly [Source: %s; Dest: %s].  Failure expected because:",
								(SourceFormat < D3DMFMT_NUMFORMAT) ? D3DQA_D3DMFMT_NAMES[SourceFormat] : L"?",
								(DestFormat   < D3DMFMT_NUMFORMAT) ? D3DQA_D3DMFMT_NAMES[DestFormat]   : L"?");

							if (bDestFailReasonYUV          )
								DebugOut(L"    * YUV formats are invalid StretchRect destination formats.");

							if (bDestFailReasonChkDevFmt    )
								DebugOut(L"    * CheckDeviceFormat failed for this format.");

							if (bDestFailReasonFmtIndex     )
								DebugOut(L"    * D3DMFMT_INDEX16 and D3DMFMT_INDEX32 are never a valid formats for use with StretchRect.");

							if (bDestFailReasonFmtFixedFloat)
								DebugOut(L"    * D3DMFMT_D3DMVALUE_FLOAT and D3DMFMT_D3DMVALUE_FIXED are never a valid formats for use with StretchRect.");

							if (bDestFailReasonFmtVertexData)
								DebugOut(L"    * D3DMFMT_VERTEXDATA is never a valid format for use with StretchRect.");

							if (bDestFailReasonFmtUnknown   )
								DebugOut(L"    * D3DMFMT_UNKNOWN is never a valid format for use with StretchRect.");

							if (bDestFailReasonFmtInvalid   )
								DebugOut(DO_ERROR, L"    * The specified destination format has no corresponding D3DMFORMAT enumerator.");

							//
							// Record failure for return value; but continue execution of remaining permutations
							//
							Result = TPR_FAIL;
							continue;

						}
					}
				}

				//
				// Note:  Even if both formats are supported, and CheckDeviceFormatConversion succeeds, it still might not
				//        be possible to achieve a successful StretchRect call for certain situations.  Example reasons:
				//
				//            * If your app intents to stretch to a render target, an additional check is required to
				//              ensure that the format is supported for a render target
				//            * If calling StretchRect for depth buffers, source and dest must be identical sizes
				//

			}
		}
	}

cleanup:

	return Result;
}

//
// ExecuteConformsToBaseProfileTest
//
//   Verify D3DMPROFILE_BASE
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileTest::ExecuteConformsToBaseProfileTest(const D3DMCAPS & Caps)
{
	INT Result = TPR_PASS;

	DebugOut(L"Beginning ExecuteConformsToBaseProfileTest.");

	if (!(Caps.PresentationIntervals & D3DMPRESENT_INTERVAL_ONE) &&
	    !(Caps.PresentationIntervals & D3DMPRESENT_INTERVAL_IMMEDIATE))
	{
		DebugOut(DO_ERROR, L"Device does not conform to Base Profile requirement:  D3DMPRESENT_INTERVAL_ONE (PresentationIntervals) Failing.");
		Result = TPR_FAIL;
	}

	/*
	// D3DMPRESENT_INTERVAL_DEFAULT is defined as zero, and thus cannot be verified programmatically

	if (!(Caps.PresentationIntervals & D3DMPRESENT_INTERVAL_DEFAULT))
	{
		DebugOut(DO_ERROR, L"Device does not conform to Base Profile requirement:  D3DMPRESENT_INTERVAL_ONE Failing.");
		Result = TPR_FAIL;
	}
	*/

	if ((!(Caps.SurfaceCaps & D3DMSURFCAPS_SYSFRONTBUFFER)) &&
		(!(Caps.SurfaceCaps & D3DMSURFCAPS_VIDFRONTBUFFER)))
	{
		DebugOut(DO_ERROR, L"Device does not conform to Base Profile requirement:  D3DMSURFCAPS_SYSFRONTBUFFER or D3DMSURFCAPS_VIDFRONTBUFFER (SurfaceCaps). Failing.");
		Result = TPR_FAIL;
	}

	if ((!(Caps.SurfaceCaps & D3DMSURFCAPS_SYSBACKBUFFER)) &&
		(!(Caps.SurfaceCaps & D3DMSURFCAPS_VIDBACKBUFFER)))
	{
		DebugOut(DO_ERROR, L"Device does not conform to Base Profile requirement: D3DMSURFCAPS_SYSBACKBUFFER or D3DMSURFCAPS_VIDBACKBUFFER (SurfaceCaps). Failing.");
		Result = TPR_FAIL;
	}


	if ((!(Caps.SurfaceCaps & D3DMSURFCAPS_SYSDEPTHBUFFER)) &&
		(!(Caps.SurfaceCaps & D3DMSURFCAPS_VIDDEPTHBUFFER)))
	{
		DebugOut(DO_ERROR, L"Device does not conform to Base Profile requirement: D3DMSURFCAPS_SYSDEPTHBUFFER or D3DMSURFCAPS_VIDDEPTHBUFFER (SurfaceCaps). Failing.");
		Result = TPR_FAIL;
	}


	if ((!(Caps.SurfaceCaps & D3DMSURFCAPS_SYSVERTEXBUFFER)) &&
		(!(Caps.SurfaceCaps & D3DMSURFCAPS_VIDVERTEXBUFFER)))
	{
		DebugOut(DO_ERROR, L"Device does not conform to Base Profile requirement: D3DMSURFCAPS_SYSVERTEXBUFFER or D3DMSURFCAPS_VIDVERTEXBUFFER (SurfaceCaps). Failing.");
		Result = TPR_FAIL;
	}
    
	if ((!(Caps.SurfaceCaps & D3DMSURFCAPS_SYSINDEXBUFFER)) &&
		(!(Caps.SurfaceCaps & D3DMSURFCAPS_VIDINDEXBUFFER)))
	{
		DebugOut(DO_ERROR, L"Device does not conform to Base Profile requirement: D3DMSURFCAPS_SYSINDEXBUFFER or D3DMSURFCAPS_VIDINDEXBUFFER (SurfaceCaps). Failing.");
		Result = TPR_FAIL;
	}
    
	if ((!(Caps.SurfaceCaps & D3DMSURFCAPS_SYSIMAGESURFACE)) &&
		(!(Caps.SurfaceCaps & D3DMSURFCAPS_VIDIMAGESURFACE)))
	{
		DebugOut(DO_ERROR, L"Device does not conform to Base Profile requirement: D3DMSURFCAPS_SYSIMAGESURFACE or D3DMSURFCAPS_VIDIMAGESURFACE (SurfaceCaps). Failing.");
		Result = TPR_FAIL;
	}
    
	if (!(Caps.PrimitiveMiscCaps & D3DMPMISCCAPS_CULLNONE))
	{
		DebugOut(DO_ERROR, L"Device does not conform to Base Profile requirement:  D3DMPMISCCAPS_CULLNONE (PrimitiveMiscCaps). Failing.");
		Result = TPR_FAIL;
	}
    
	if (!(Caps.PrimitiveMiscCaps & D3DMPMISCCAPS_CULLCCW))
	{
		DebugOut(DO_ERROR, L"Device does not conform to Base Profile requirement:  D3DMPMISCCAPS_CULLCCW (PrimitiveMiscCaps). Failing.");
		Result = TPR_FAIL;
	}
    
	if ((Caps.RasterCaps & D3DMPRASTERCAPS_FOGTABLE) &&
	    (!(Caps.RasterCaps & D3DMPRASTERCAPS_WFOG)) &&
		(!(Caps.RasterCaps & D3DMPRASTERCAPS_ZFOG)))
	{
		DebugOut(DO_ERROR, L"Device does not conform to Base Profile requirement: D3DMPRASTERCAPS_WFOG or D3DMPRASTERCAPS_ZFOG (RasterCaps). Failing.");
		Result = TPR_FAIL;
	}
    
	if ((!(Caps.RasterCaps & D3DMPRASTERCAPS_FOGVERTEX)) &&
		(!(Caps.RasterCaps & D3DMPRASTERCAPS_FOGTABLE)))
	{
		DebugOut(DO_ERROR, L"Device does not conform to Base Profile requirement: D3DMPRASTERCAPS_FOGVERTEX or D3DMPRASTERCAPS_FOGTABLE (RasterCaps). Failing.");
		Result = TPR_FAIL;
	}

	if (!(Caps.ZCmpCaps & D3DMPCMPCAPS_NEVER))
	{
		DebugOut(DO_ERROR, L"Device does not conform to Base Profile requirement: D3DMPCMPCAPS_NEVER  (ZCmpCaps). Failing.");
		Result = TPR_FAIL;
	}
    
	if ((!(Caps.ZCmpCaps & D3DMPCMPCAPS_LESS)) &&
		(!(Caps.ZCmpCaps & D3DMPCMPCAPS_LESSEQUAL)))
	{
		DebugOut(DO_ERROR, L"Device does not conform to Base Profile requirement: D3DMPCMPCAPS_LESS or D3DMPCMPCAPS_LESSEQUAL (ZCmpCaps). Failing.");
		Result = TPR_FAIL;
	}

	if (!(Caps.ZCmpCaps & D3DMPCMPCAPS_ALWAYS))
	{
		DebugOut(DO_ERROR, L"Device does not conform to Base Profile requirement: D3DMPCMPCAPS_ALWAYS  (ZCmpCaps). Failing.");
		Result = TPR_FAIL;
	}
    
	if (!(Caps.BlendOpCaps & D3DMBLENDOPCAPS_ADD))
	{
		DebugOut(DO_ERROR, L"Device does not conform to Base Profile requirement: D3DMBLENDOPCAPS_ADD  (BlendOpCaps). Failing.");
		Result = TPR_FAIL;
	}
    
	if (!(Caps.ShadeCaps & D3DMPSHADECAPS_COLORGOURAUDRGB))
	{
		DebugOut(DO_ERROR, L"Device does not conform to Base Profile requirement: D3DMPSHADECAPS_COLORGOURAUDRGB  (ShadeCaps). Failing.");
		Result = TPR_FAIL;
	}
    
	if (!(Caps.VertexProcessingCaps & D3DMVTXPCAPS_DIRECTIONALLIGHTS))
	{
		DebugOut(DO_ERROR, L"Device does not conform to Base Profile requirement: D3DMVTXPCAPS_DIRECTIONALLIGHTS  (VertexProcessingCaps). Failing.");
		Result = TPR_FAIL;
	}
    
	if (!(Caps.VertexProcessingCaps & D3DMVTXPCAPS_POSITIONALLIGHTS))
	{
		DebugOut(DO_ERROR, L"Device does not conform to Base Profile requirement: D3DMVTXPCAPS_POSITIONALLIGHTS (VertexProcessingCaps). Failing.");
		Result = TPR_FAIL;
	}

	if (!((Caps.StretchRectFilterCaps & D3DMPTFILTERCAPS_MINFPOINT )   ||
	      (Caps.StretchRectFilterCaps & D3DMPTFILTERCAPS_MINFLINEAR)))
	{
		DebugOut(L"Device does not expose any D3DMPTFILTERCAPS_MIN* bit in StretchRectFilterCaps.  Provided that StretchRect works with");
		DebugOut(DO_ERROR, L"D3DMTEXF_NONE, this is acceptable.");
	}

	if (!((Caps.StretchRectFilterCaps & D3DMPTFILTERCAPS_MAGFPOINT )   ||
	      (Caps.StretchRectFilterCaps & D3DMPTFILTERCAPS_MAGFLINEAR)))
	{
		DebugOut(L"Device does not expose any D3DMPTFILTERCAPS_MAG* bit in StretchRectFilterCaps.  Provided that StretchRect works with");
		DebugOut(DO_ERROR, L"D3DMTEXF_NONE, this is acceptable.");
	}

	return Result;
}

//
// ExecuteCheckD3DMRefNotIncludedTest
//
//   Make sure that D3DM Reference driver is not included in image or listed in registry
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileTest::ExecuteCheckD3DMRefNotIncludedTest()
{
	INT Result = TPR_PASS;

	HKEY DriversKey;
	const INT VALUECOUNT = 2;
	WCHAR DriversName[] = L"System\\D3DM\\Drivers";
	WCHAR DriversValues[VALUECOUNT][20] = {
	    L"LocalHook",
	    L"RemoteHook"};

	TCHAR DriverFileName[] = _T("\\windows\\d3dmref.dll");
    DWORD DriverFileAttributes;
    const DWORD UnallowedAttributes = 
        FILE_ATTRIBUTE_SYSTEM |
        FILE_ATTRIBUTE_HIDDEN |
        FILE_ATTRIBUTE_READONLY |
        FILE_ATTRIBUTE_INROM;
	INT Ret = 0;

	DebugOut(_T("Beginning ExecuteCheckD3DMRefNotIncludedTest.\n"));

    //
    // Is the Reference driver listed in either LocalHook or RemoteHook?
    //

    // Open registry key HKLM\System\D3DM\Drivers
    Ret = RegOpenKeyEx(
        HKEY_LOCAL_MACHINE,
        DriversName,
        0,
        0,
        &DriversKey);
    if (ERROR_SUCCESS != Ret)
    {
        DebugOut(_T("RegOpenKeyEx could not find key for D3DM driver, skipping registry check. (Returned %d)"), Ret);
    }
    else
    {
        for (int i = 0; i < VALUECOUNT; ++i)
        {
            WCHAR Value[20];
            DWORD ValueSize = sizeof(Value);
            Ret = RegQueryValueEx(
                DriversKey,
                DriversValues[i],
                NULL,
                NULL,
                (BYTE*)Value,
                &ValueSize);
            if (ERROR_SUCCESS != Ret)
            {
                DebugOut(
                    _T("RegQueryValueEx could not find value \"%s\". Skipping check on this value. (Returned %d)"),
                    DriversValues[i],
                    Ret);
                continue;
            }
            _wcsupr(Value);
            if (wcsstr(Value, L"D3DMREF.DLL"))
            {
                DebugOut(
                    _T("Value \"%s\" is set to \"%s\", which includes the name of the reference driver (D3DMREF.DLL). This is not allowed. Failing."),
                    DriversValues[i],
                    Value);
                Result = TPR_FAIL;
            }
        }
    }
    RegCloseKey(DriversKey);
    
    //
    // Is the Reference driver included in the image?
    //

    // First check if it is present under the \windows directory
    DriverFileAttributes = GetFileAttributes(DriverFileName);

    if (0xffffffff == DriverFileAttributes && ERROR_FILE_NOT_FOUND == GetLastError())
    {
        DebugOut(
            _T("\"%s\" does not exist. This is expected."),
            DriverFileName);
    }
    else if (DriverFileAttributes & UnallowedAttributes)
    {
        DebugOut(
            _T("\"%s\" exists, and is a part of the image (Attributes: 0x%08x). This is not allowed. Failing."),
            DriverFileName,
            DriverFileAttributes);
        Result = TPR_FAIL;
    }
    else
    {
        DebugOut(
            _T("\"%s\" exists, but is not in rom. Verifying that the file is not shadowing an in-rom file"),
            DriverFileName);
            
        // If it is, copy it to the root, and delete it from the \windows directory.
        TCHAR TempFileName[MAX_PATH + 1] = {0};

        _stprintf(TempFileName, _T("\\Temp\\d3dmref%d.dll"), rand());

        Ret = MoveFile(DriverFileName, TempFileName);
        if (!Ret)
        {
            DebugOut(
                _T("MoveFile failed. Source: \"%s\"; Dest: \"%s\"; Error: %d"),
                DriverFileName,
                TempFileName,
                GetLastError());
            DebugOut(_T("Verify that the \\Temp directory exists on the device."));
            Result = TPR_ABORT;
        }
        // If it is still there, then it is included in the image, so fail.
        else
        {
            DriverFileAttributes = GetFileAttributes(DriverFileName);
            if (0xffffffff != DriverFileAttributes)
            {
                DebugOut(
                    _T("\"%s\" still exists after being moved to a different location, indicating that it is included in the runtime image. This is not allowed. Failing"),
                    DriverFileName);
                Result = TPR_FAIL;
            }
        }
        MoveFile(TempFileName, DriverFileName);
    }

    if (TPR_PASS != Result)
    {
        DebugOut(_T("To pass this test, the D3DM Reference driver (d3dmref.dll) must not be included in the image."));
        DebugOut(_T("Verify that BSP_D3DM_REF is not set, and that d3dmref.dll has not been added to your device's .bib or .reg files."));
        DebugOut(_T("If the test is still failing, verify that the test is running on a clean image."));
    }
    
	return Result;
}

//
// ExecuteMultipleInstancesTest
//
//   Verify IDirect3DMobile behaves properly when used in more than one process.
//   Uses D3DMHelper in a separate process.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileTest::ExecuteMultipleInstancesTest(
    LPWINDOW_ARGS pWindowArgs, 
    HINSTANCE hInstance, 
    const WCHAR * wszSoftwareDriver, 
    ULONG ulTestOrdinal)
{
    HRESULT hr;
    
    DebugOut(L"Beginning ExecuteCheckProfileTest.");

    //
    // Actual number of adapters reported by GetAdapterCount
    //
    UINT uiActualAdapters;
    
    //
    // Iterator for adapter indices
    //
    UINT uiAdapterIndex;

    //
    // Test result
    //
    INT Result = TPR_PASS;

    //
    // Device capabilities
    //
    D3DMCAPS Caps;

    //
    // The location of the D3DM Helper process
    //
    TCHAR tszHelperProcess[MAX_PATH + 1] = _T("");

    //
    // RemoteTester Master object, which will allow us to communicate
    // with a separate process to run tests.
    //
    RemoteMaster_t RemoteTest;
    HRESULT hrRemoteResult = S_OK;

    //
    // If no driver was specified, wszSoftwareDriver will be empty. Make it NULL
    //
    if (wszSoftwareDriver && !wszSoftwareDriver[0])
    {
        wszSoftwareDriver = NULL;
    }

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    uiActualAdapters = m_pD3D->GetAdapterCount();

    // First determine that only one D3DM object and only one D3DMDevice object can 
    // be created per process.
    DebugOut(L"Testing single process instance restraints for Direct3DMobileCreate");
    do
    {
        // Since we already have a D3DM object, this should fail
        IDirect3DMobile * pMobile = NULL;
        LPDIRECT3DMOBILE (WINAPI* pfnDirect3DMobileCreate)(UINT SDKVersion);
        HMODULE hD3DMDLL = NULL;
        FARPROC pSWDeviceEntry = NULL;
        hD3DMDLL = GetModuleHandle(D3DMDLL);
        

        //
        // The address of the DLL's exported function indicates success. 
        // NULL indicates failure. 
        //
        (FARPROC&)pfnDirect3DMobileCreate = GetProcAddress(hD3DMDLL,    // HMODULE hModule, 
                                                           D3DMCREATE); // LPCWSTR lpProcName
        if (NULL == pfnDirect3DMobileCreate)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            DebugOut(DO_ERROR, _T("GetProcAddress failed for Direct3DMobileCreate. Error: 0x%08x. Failing."), hr);
            if (TPR_PASS == Result)
            {
                Result = TPR_FAIL;
            }
            break;
        }

        pMobile = pfnDirect3DMobileCreate( D3DM_SDK_VERSION );
        if (NULL != pMobile)
        {
            DebugOut(DO_ERROR, L"Only one Direct3DMobile object is allowed per process, but Direct3DMobileCreate succeeded a second time. Failing.");
            if (TPR_PASS == Result)
            {
                Result = TPR_FAIL;
            }
            pMobile->Release();
            pMobile = NULL;
        }
    } while(false);

    DebugOut(L"Testing single process instance restraints for IDirect3DMobile::CreateDevice");
    for (uiAdapterIndex = D3DMADAPTER_DEFAULT; uiAdapterIndex < uiActualAdapters; uiAdapterIndex++)
    {
        // Since we already have a D3DM Device object, this should fail
        IDirect3DMobileDevice * pNewDev = NULL;
        // Fresh WindowArgs to prevent early class deregistration
        WINDOW_ARGS OtherWindowArgs;

        OtherWindowArgs = *pWindowArgs;
        OtherWindowArgs.lpClassName = _T("MultipleInstancesWindowClass");
        OtherWindowArgs.lpWindowName = _T("MultipleInstancesWindow");
        // Need another window
        WindowSetup newWindow(D3DQA_MSGPROC_POSTQUIT_ONDESTROY, NULL, &OtherWindowArgs);
        hr = m_pD3D->CreateDevice(uiAdapterIndex, D3DMDEVTYPE_DEFAULT, newWindow.GetHandle(), D3DMCREATE_MULTITHREADED, &m_d3dpp, &pNewDev);
        if (SUCCEEDED(hr))
        {
            DebugOut(DO_ERROR, L"Only one Direct3DMobile device is allowed per D3DM Object, but CreateDevice succeeded a second time. Failing.");
            if (TPR_PASS == Result)
            {
                Result = TPR_FAIL;
            }
            pNewDev->Release();
            pNewDev = NULL;
        }
    }

    DebugOut(L"Testing multiple process instance restraints for Direct3DMobileCreate");
    hr = RemoteTest.Initialize(hInstance, _T("D3DM_Interface_MultipleInstance"), 0);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Unable to initialize RemoteTester Master. (hr = 0x%08x) Aborting.", hr);
        // The RemoteTest destructor will clean up
        return TPR_ABORT;
    }

    GetModulePath(hInstance, tszHelperProcess, countof(tszHelperProcess));
    StringCchCat(tszHelperProcess, countof(tszHelperProcess), D3DMHELPERPROC);
    hr = RemoteTest.CreateRemoteTestProcess(tszHelperProcess);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, 
            L"Unable to create RemoteTester slave process \"%s\". (hr = 0x%08x) Aborting.",
            tszHelperProcess,
            hr);
        return TPR_ABORT;
    }
    
    // Second determine that more than one process can create a d3dm and d3dmdevice object.
    hr = RemoteTest.SendTaskSynchronous(
        D3DMHELPER_TRYCREATEOBJECT,
        0,
        NULL,
        &hrRemoteResult,
        5000);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR,
            L"Unable to remotely process command TRYCREATEOBJECT. (hr = 0x%08x) Aborting.",
            hr);
        return TPR_ABORT;
    }

    if (FAILED(hrRemoteResult))
    {
        DebugOut(DO_ERROR,
            L"Unable to create Direct3DMoblie object in other process. (hr = 0x%08x) Failing.",
            hrRemoteResult);
        if (TPR_PASS == Result)
        {
            Result = TPR_FAIL;
        }
    }

    DebugOut(L"Testing multiple process instance restraints for IDirect3DMobile::CreateDevice");
    // Try to create a device in the other process
    D3DMHCreateD3DMDeviceOptions_t CreateOptions;
    memset(&CreateOptions, 0x00, sizeof(CreateOptions));
    CreateOptions.Purpose = D3DQA_PURPOSE_RAW_TEST;
    CreateOptions.WindowArgs = *pWindowArgs;
    CreateOptions.WindowArgs.lpClassName = NULL;
    CreateOptions.WindowArgs.lpWindowName = NULL;
    if (wszSoftwareDriver)
    {
        StringCchCopy(CreateOptions.DriverName, countof(CreateOptions.DriverName), wszSoftwareDriver);
    }
    CreateOptions.TestCase = ulTestOrdinal;

    hr = RemoteTest.SendTaskSynchronous(
        D3DMHELPER_CREATEDEVICE,
        sizeof(CreateOptions),
        &CreateOptions,
        &hrRemoteResult,
        5000);
        
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR,
            L"Unable to remotely process command CREATEDEVICE. (hr = 0x%08x) Aborting.",
            hr);
        return TPR_ABORT;
    }

    if (FAILED(hrRemoteResult))
    {
        DebugOut(DO_ERROR,
            L"Unable to create IDirect3DMobileDevice object in other process. (hr = 0x%08x) Failing.",
            hrRemoteResult);
        if (TPR_PASS == Result)
        {
            Result = TPR_FAIL;
        }
    }
    else
    {
        //
        // Check on the remote device's TCL (should succeed, since we're not fullscreen)
        //
        hr = RemoteTest.SendTaskSynchronous(
            D3DMHELPER_TESTCOOPLEVEL,
            0,
            NULL,
            &hrRemoteResult,
            5000);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR,
                L"Unable to remotely process command TESTCOOPLEVEL. (hr = 0x%08x) Aborting.",
                hr);
            return TPR_ABORT;
        }
        
        if (FAILED(hrRemoteResult))
        {
            DebugOut(DO_ERROR,
                L"Remote Device reported failure from TestCooperativeLevel wehn it shouldn't have. (hr = 0x%08x) Failing.",
                hrRemoteResult);
            if (TPR_PASS == Result)
            {
                Result = TPR_FAIL;
            }
        }
        
    }


    // Third determine that only one process can create a fullscreen device
    D3DMInitializer::Cleanup();

    pWindowArgs->bPParmsWindowed = FALSE;
    hr = D3DMInitializer::Init(D3DQA_PURPOSE_RAW_TEST, pWindowArgs, CreateOptions.DriverName[0] ? CreateOptions.DriverName : NULL, ulTestOrdinal);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR,
            L"Unable to recreate device in fullscreen. (hr = 0x%08x) Aborting.",
            hr);
        return TPR_ABORT;
    }
    
    //
    // Check on the remote device's TCL (should be DEVICELOST, since we're not fullscreen)
    //
    hr = RemoteTest.SendTaskSynchronous(
        D3DMHELPER_TESTCOOPLEVEL,
        0,
        NULL,
        &hrRemoteResult,
        5000);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR,
            L"Unable to remotely process command TESTCOOPLEVEL. (hr = 0x%08x) Aborting.",
            hr);
        return TPR_ABORT;
    }
    
    if (hrRemoteResult != D3DMERR_DEVICELOST)
    {
        DebugOut(DO_ERROR,
            L"Remote Device was not lost when local went fullscreen. (hr = 0x%08x) Failing.",
            hrRemoteResult);
        if (TPR_PASS == Result)
        {
            Result = TPR_FAIL;
        }
    }
    
    hr = RemoteTest.SendTaskSynchronous(
        D3DMHELPER_RELEASEDEVICE,
        0,
        NULL,
        &hrRemoteResult,
        5000);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR,
            L"Unable to remotely process command RELEASEDEVICE. (hr = 0x%08x) Aborting.",
            hr);
        return TPR_ABORT;
    }

    if (FAILED(hrRemoteResult))
    {
        DebugOut(DO_ERROR,
            L"Unable to clean up IDirect3DMobileDevice object in other process. (hr = 0x%08x) Failing.",
            hrRemoteResult);
        if (TPR_PASS == Result)
        {
            Result = TPR_FAIL;
        }
    }

    CreateOptions.WindowArgs.bPParmsWindowed = FALSE;
    hr = RemoteTest.SendTaskSynchronous(
        D3DMHELPER_CREATEDEVICE,
        sizeof(CreateOptions),
        &CreateOptions,
        &hrRemoteResult,
        5000);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR,
            L"Unable to remotely process command CLEANUP. (hr = 0x%08x) Aborting.",
            hr);
        return TPR_ABORT;
    }

    if (SUCCEEDED(hrRemoteResult))
    {
        DebugOut(DO_ERROR,
            L"Succeeded in creating fullscreen D3DM devices in two processes. (hr = 0x%08x) Failing.",
            hrRemoteResult);
        if (TPR_PASS == Result)
        {
            Result = TPR_FAIL;
        }
    }

    //
    // Verify remote device is now ready to be reset.
    D3DMInitializer::Cleanup();
    
    hr = RemoteTest.SendTaskSynchronous(
        D3DMHELPER_TESTCOOPLEVEL,
        0,
        NULL,
        &hrRemoteResult,
        5000);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR,
            L"Unable to remotely process command TESTCOOPLEVEL. (hr = 0x%08x) Aborting.",
            hr);
        return TPR_ABORT;
    }
    
    if (hrRemoteResult != D3DMERR_DEVICENOTRESET)
    {
        DebugOut(DO_ERROR,
            L"Remote Device was not set for DEVICENOTRESET when local left fullscreen. (hr = 0x%08x) Failing.",
            hrRemoteResult);
        if (TPR_PASS == Result)
        {
            Result = TPR_FAIL;
        }
    }
    return Result;
}
