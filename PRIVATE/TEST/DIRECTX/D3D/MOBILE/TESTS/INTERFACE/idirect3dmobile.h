//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
	INT ExecuteConformsToBaseProfileTest(D3DMCAPS Caps);

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

	/////////////////////////////////////////////
	// Additional miscellaneous tests
	/////////////////////////////////////////////
	INT ExecuteMiscTest();

};

