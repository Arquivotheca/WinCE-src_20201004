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
#include <d3dm.h>
#include "TestWindow.h"
#include "InitPurpose.h"
#include "ParseArgs.h"

void InitPowerRequirements();
void FreePowerRequirements();

class D3DMInitializer
{

protected:

    //
    // Disables ForceCleanD3DM
    //
    HRESULT UseCleanD3DM(BOOL ForceClean);

	//
	// Ensures that D3DM.DLL is freshly loaded
	//
	HRESULT ForceCleanD3DM();

	// Creates Direct3D in a convenient state for testing
	// world and view matrices (view / projection are set to multiply
	// out to identity matrix)
	HRESULT D3DInitForWorldViewTest(LPWINDOW_ARGS pWindowArgs);

	// This function initializes Direct3D "raw" testing; meaning that
	// the test application will be left to initialize all render
	// states, transforms, etc.
	HRESULT D3DInitRawTest(LPWINDOW_ARGS pWindowArgs);

	// This function initializes Direct3D "raw" testing, with the addition
	// of a depth/stencil
	HRESULT D3DDepthStencilSupportedTest(LPWINDOW_ARGS pWindowArgs);

	//
	// Display test case ordinal to be executed
	//
	void ShowTestCaseOrdinal(HWND hWnd, UINT uiTestCase);

protected:
    
	LPDIRECT3DMOBILE m_pD3D;
	LPDIRECT3DMOBILEDEVICE m_pd3dDevice;
	D3DMPRESENT_PARAMETERS m_d3dpp;
	HWND m_hWnd;
	WindowSetup *m_pAppWindow;
	UINT m_uiAdapterOrdinal;
	D3DMDEVTYPE m_DevType;
	DWORD m_dwBehaviorFlags;
	HINSTANCE m_hSWDeviceDLL;
	BOOL m_bInitSuccess;
	BOOL m_bForceClean;

	HRESULT Init(D3DQA_PURPOSE Purpose, LPWINDOW_ARGS pWindowArgs, PWCHAR pwszDriver, UINT uiTestCase);
	void Cleanup();
	void ClearMembers();

public:
	D3DMInitializer();
	~D3DMInitializer();

	LPDIRECT3DMOBILE GetObject();
	LPDIRECT3DMOBILEDEVICE GetDevice();
	HWND GetWindowHandle() { return m_hWnd; } 

	BOOL IsReady();
};
