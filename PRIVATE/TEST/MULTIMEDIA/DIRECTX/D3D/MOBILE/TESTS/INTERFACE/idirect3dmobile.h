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
#pragma once
#include "Initializer.h"

class IDirect3DMobileTest : public D3DMInitializer {
private:
	
	//
	// Indicates whether or not the object is initialized
	//
	BOOL m_bInitSuccess;
	

	//
	// Is test executing over debug middleware?
	//
	BOOL m_bDebugMiddleware;

	//
	// D3DMPROFILE_BASE Test
	//
	INT ExecuteConformsToBaseProfileTest(const D3DMCAPS & Caps);

public:

	IDirect3DMobileTest();
	HRESULT Init(LPTEST_CASE_ARGS pTestCaseArgs, LPWINDOW_ARGS pWindowArgs, BOOL bDebug, UINT uiTestCase);
	~IDirect3DMobileTest();


	//
	// A convenience function, that indicates whether the initialization
	// has completed successfully.
	//
	BOOL IsReady();


	////////////////////////////////////////////////////////////////////
	// Verification for individual methods of IDirect3DMobile
	////////////////////////////////////////////////////////////////////

	//
	// Verify IDirect3DMobile::QueryInterface
	//
	INT ExecuteQueryInterfaceTest();

	//
	// Verify IDirect3DMobile::AddRef
	//
	INT ExecuteAddRefTest();

	//
	// Verify IDirect3DMobile::Release
	//
	INT ExecuteReleaseTest();

	//
	// Verify IDirect3DMobile::RegisterSoftwareDevice
	//
	INT ExecuteRegisterSoftwareDeviceTest();

	//
	// Verify IDirect3DMobile::GetAdapterCount
	//
	INT ExecuteGetAdapterCountTest();

	//
	// Verify IDirect3DMobile::GetAdapterIdentifier
	//
	INT ExecuteGetAdapterIdentifierTest();

	//
	// Verify IDirect3DMobile::GetAdapterModeCount
	//
	INT ExecuteGetAdapterModeCountTest();

	//
	// Verify IDirect3DMobile::EnumAdapterModes
	//
	INT ExecuteEnumAdapterModesTest();

	//
	// Verify IDirect3DMobile::GetAdapterDisplayMode
	//
	INT ExecuteGetAdapterDisplayModeTest();

	//
	// Verify IDirect3DMobile::CheckDeviceType
	//
	INT ExecuteCheckDeviceTypeTest();

	//
	// Verify IDirect3DMobile::CheckDeviceFormat
	//
	INT ExecuteCheckDeviceFormatTest();

	//
	// Verify IDirect3DMobile::CheckDeviceMultiSampleType
	//
	INT ExecuteCheckDeviceMultiSampleTypeTest();

	//
	// Verify IDirect3DMobile::CheckDepthStencilMatch
	//
	INT ExecuteCheckDepthStencilMatchTest();

	//
	// Verify IDirect3DMobile::CheckProfile
	//
	INT ExecuteCheckProfileTest();

	//
	// Verify IDirect3DMobile::GetDeviceCaps
	//
	INT ExecuteGetDeviceCapsTest();

	//
	// Verify IDirect3DMobile::CreateDevice
	//
	INT ExecuteCreateDeviceTest();

	//
	// Verify IDirect3DMobile::CheckDeviceFormatConversion
	//
	INT ExecuteCheckDeviceFormatConversionTest();

    //
    // Verify the D3DM reference driver is not included in the image.
    //
	INT ExecuteCheckD3DMRefNotIncludedTest();

    //
    // Verify correct behavior with multiple instances.
    //
    INT ExecuteMultipleInstancesTest(LPWINDOW_ARGS pWindowArgs, HINSTANCE hInstance, const WCHAR * wszSoftwareDriver, ULONG ulTestOrdinal);

	/////////////////////////////////////////////
	// Additional miscellaneous tests
	/////////////////////////////////////////////
	INT ExecuteMiscTest();

};

