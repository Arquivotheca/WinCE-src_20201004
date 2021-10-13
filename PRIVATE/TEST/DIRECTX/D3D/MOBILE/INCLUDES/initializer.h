//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once
#include <d3dm.h>
#include "TestWindow.h"
#include "InitPurpose.h"
#include "ParseArgs.h"

class D3DMInitializer
{

private:

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
