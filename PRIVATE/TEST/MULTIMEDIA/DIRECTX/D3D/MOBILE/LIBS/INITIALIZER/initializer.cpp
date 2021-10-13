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
#include "Initializer.h"
#include "DebugOutput.h"
#include <pm.h>

#define _M(a) D3DM_MAKE_D3DMVALUE(a)
#define D3DMDLL _T("D3DM.DLL")
#define D3DMCREATE _T("Direct3DMobileCreate")
#define MAX_DEBUG_OUT 256

//
// Device creation parameters
//
#define D3DQA_DEVTYPE D3DMDEVTYPE_DEFAULT
#define D3DQA_BEHAVIOR_FLAGS D3DMCREATE_MULTITHREADED

//
// When attempting to unload D3DM, prevent even a remote possibility of infinite loop
//
#define D3DQA_MAX_UNLOAD_ATTEMPTS 10

//
// Helper functions unrelated to D3DM, but necessary for initialization of the tests.
//
HANDLE ghPowerManagement = NULL;
void
InitPowerRequirements()
{
#ifdef UNDER_CE

    HKEY hKey;
    TCHAR szBuffer[256] = {NULL};
    TCHAR szBufferPM[256] = {NULL};
    ULONG nDriverName = _countof(szBuffer);

    ghPowerManagement = NULL;

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                        TEXT("System\\GDI\\Drivers"),
                        0, // Reseved. Must == 0.
                        0, // Must == 0.
                        &hKey) == ERROR_SUCCESS)
    {
        if (RegQueryValueEx(hKey,
                            TEXT("MainDisplay"),
                            NULL,  // Reserved. Must == NULL.
                            NULL,  // Do not need type info.
                            (BYTE *)szBuffer,
                            &nDriverName) == ERROR_SUCCESS)
        {
            // the guid and device name has to be separated by a '\' and also there has to be the leading
            // '\' in front of windows, so the name is {GUID}\\Windows\ddi_xxx.dll
            StringCchPrintf(szBufferPM, _countof(szBufferPM) - 1, TEXT("%s\\\\Windows\\%s"), PMCLASS_DISPLAY, szBuffer);
            ghPowerManagement = SetPowerRequirement(szBufferPM, D0, POWER_NAME, NULL, 0);
        }
        RegCloseKey(hKey);
    }

#endif
}

void
FreePowerRequirements()
{
#ifdef UNDER_CE
    if(ghPowerManagement)
    {
        ReleasePowerRequirement(ghPowerManagement);
        ghPowerManagement = NULL;
    }
#endif
}


//
//  D3DMInitializer
//
//    Constructor for D3DMInitializer class; however, most initialization is deferred.
//
//  Arguments:
//
//    none
//
//  Return Value:
//
//    none
//
D3DMInitializer::D3DMInitializer()
{
    ClearMembers();
    m_bForceClean = TRUE;
}

HRESULT D3DMInitializer::UseCleanD3DM(BOOL ForceClean)
{
    m_bForceClean = ForceClean;
    return S_OK;
}

//
//  ForceCleanD3DM
//
//    Ensures that D3DM.DLL is freshly loaded for this process
//
//  Arguments:
//
//    none
//
//  Return Value:
//
//    HRESULT indicates success or failure
//
HRESULT D3DMInitializer::ForceCleanD3DM()
{
	HINSTANCE hLoadLibD3DMDLL;
	HMODULE hD3DMDLL;
	UINT uiAttempts = 0;

	if (!m_bForceClean)
	{
	    return S_OK;
	}

	while (hD3DMDLL = GetModuleHandle(D3DMDLL))
	{
		if (!FreeLibrary(hD3DMDLL))
    		DebugOut(DO_ERROR, _T("FreeLibrary(D3DM.DLL) failed; GetLastError: %d.\n"), GetLastError());

		if (D3DQA_MAX_UNLOAD_ATTEMPTS == ++uiAttempts)
			break;
	}

	hLoadLibD3DMDLL = LoadLibrary(D3DMDLL); 
	
	if (NULL==hLoadLibD3DMDLL)
	{
        DebugOut(_T("LoadLibrary(D3DM.DLL) failed; GetLastError: %d.\n"), GetLastError());
		DebugOut(DO_ERROR, _T("Unable to load D3DM.DLL.  SYSGEN_D3DM is required for this test."));
		return E_FAIL;
	}
	else
	{
		return S_OK;
	}

}


//
//  D3DMInitializer
//
//    Initializer for this class; takes a flag that indicates the type of test that will
//    use this test, and sets up initial state accordingly.
//
//  Arguments:
//
//    D3DQA_PURPOSE:  A hint to D3DMInitializer, to indicate how to set the device up for
//                    the desired type of testing (e.g., TnL-specific testing)
//    HWND:  Window to use for Direct3D device creation
//    
//    PWCHAR pwszDriver:  DLL name of driver to register (pass NULL if using system driver)
//    
//    UINT uiTestCase:  Test case ordinal (to be displayed in window)
//
//  Return Value:
//
//    HRESULT:  Indicates success or failure
//
HRESULT D3DMInitializer::Init(D3DQA_PURPOSE Purpose, LPWINDOW_ARGS pWindowArgs, PWCHAR pwszDriver, UINT uiTestCase)
{
	LPDIRECT3DMOBILE (WINAPI* pfnDirect3DMobileCreate)(UINT SDKVersion);
	HMODULE hD3DMDLL = NULL;
	FARPROC pSWDeviceEntry = NULL;

	//
	// This HRESULT is returned only in failure cases.
	//
	HRESULT hr = E_FAIL;
	HRESULT hrTemp;

	//
	// Perhaps this object has been used previously; and is now being re-initialized for another
	// purpose.  Handle this case gracefully.
	//
	Cleanup();
	ClearMembers();

	DebugOut(_T("Creating D3DM object.\n"));

	//
	// Some args aren't currently configurable by this class
	//
	m_DevType = D3DQA_DEVTYPE;
	m_dwBehaviorFlags = D3DQA_BEHAVIOR_FLAGS;

	//
	// Create the D3D object.
	//
	if (FAILED(hrTemp = ForceCleanD3DM()))
	{
	    hr = hrTemp;
		DebugOut(DO_ERROR, _T("ForceCleanD3DM failed. hr = 0x%08x"), hr);
        goto cleanup;
	}

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
		DebugOut(DO_ERROR, _T("GetProcAddress failed for Direct3DMobileCreate. Error: 0x%08x.\n"), hr);
		goto cleanup;
	}

	if(!( m_pD3D = pfnDirect3DMobileCreate( D3DM_SDK_VERSION ) ) )
	{
	    hr = HRESULT_FROM_WIN32(GetLastError());
		DebugOut(DO_ERROR, _T("Direct3DMobileCreate failed. Error: 0x%08lx.\n"), hr);
		goto cleanup;
	}

	//
	// Create a window
	//
	m_pAppWindow = new WindowSetup(D3DQA_MSGPROC_POSTQUIT_ONDESTROY, // D3DQA_MSGPROC_STYLE MsgProcStyle,
	                               NULL,                             // WNDPROC lpfnWndProc,
	                               pWindowArgs);                     // LPWINDOW_ARGS pWindowArgs
	if (NULL == m_pAppWindow)
	{
		DebugOut(DO_ERROR, _T("WindowSetup failed."));
		goto cleanup;
	}

	if (!(m_hWnd = m_pAppWindow->GetHandle()))
	{
		DebugOut(DO_ERROR, _T("WindowSetup::GetHandle failed."));
		goto cleanup;
	}

	ShowTestCaseOrdinal(m_hWnd, uiTestCase);

	//
	// If a driver filename has been provided, honor it; otherwise, use 
	// the default adapter.
	//
	if (NULL == pwszDriver)
	{
		m_uiAdapterOrdinal = D3DMADAPTER_DEFAULT;
	}
	else
	{
		m_uiAdapterOrdinal = D3DMADAPTER_REGISTERED_DEVICE;

        DebugOut(_T("Loading driver: %ls"), pwszDriver);
		//
		// This function maps the specified DLL file into the address
		// space of the calling process. 
		//
		m_hSWDeviceDLL = LoadLibrary(pwszDriver);

		//
		// NULL indicates failure
		//
		if (NULL == m_hSWDeviceDLL)
		{
		    hr = HRESULT_FROM_WIN32(GetLastError());
		    DebugOut(DO_ERROR, _T("LoadLibrary failed. Error: 0x%08lx.\n"), hr);
            goto cleanup;
		}

		//
		// This function returns the address of the specified exported DLL function. 
		//
		pSWDeviceEntry = GetProcAddress(m_hSWDeviceDLL,         // HMODULE hModule, 
                                        _T("D3DM_Initialize")); // LPCWSTR lpProcName
		//
		// NULL indicates failure.
		//
 		if (NULL == pSWDeviceEntry)
		{
		    hr = HRESULT_FROM_WIN32(GetLastError());
			DebugOut(DO_ERROR, _T("GetProcAddress failed. Error: 0x%08x"), hr);
			goto cleanup;
		}

		//
		// This method registers pluggable software device with Direct3D Mobile
		//
		if (FAILED(hr = m_pD3D->RegisterSoftwareDevice(pSWDeviceEntry)))
		{
			DebugOut(DO_ERROR, _T("RegisterSoftwareDevice failed. hr = 0x%08x"), hr);
			goto cleanup;
		}
	}
	
	switch(Purpose)
	{
	case D3DQA_PURPOSE_WORLD_VIEW_TEST:
		if (FAILED(hr = D3DInitForWorldViewTest(pWindowArgs)))
            goto cleanup;
		break;
	case D3DQA_PURPOSE_SMALL_TOPMOST:
	case D3DQA_PURPOSE_RAW_TEST:
		if (FAILED(hr = D3DInitRawTest(pWindowArgs)))
            goto cleanup;
		break;
	case D3DQA_PURPOSE_DEPTH_STENCIL_TEST:
		if (FAILED(hr = D3DDepthStencilSupportedTest(pWindowArgs)))
            goto cleanup;
		break;
	case D3DQA_PURPOSE_TEXTURE_TEST:
	case D3DQA_PURPOSE_CLIP_STATUS_TEST:
	case D3DQA_PURPOSE_FULLPIPE_TEST:
	default:
	    hr = E_NOTIMPL;
		DebugOut(DO_ERROR, _T("D3DMInitializer: Not yet implemented.\n"));
        goto cleanup;
		break;
	}

	//
	// Now that initialization is complete, set up the remaining member variables
	//
	m_bInitSuccess = TRUE;

	return S_OK;

cleanup:

	Cleanup();
	ClearMembers();

	return hr;

}


//
// D3DInitRawTest
//
//   This function initializes Direct3D "raw" testing; meaning that the test
//   application will be left to initialize all render states, transforms, etc.
//   
// Arguments:
//   
//   LPWINDOW_ARGS pWindowArgs:  Description of desired window characteristics
//
// Return Value:
// 
//   HRESULT:  Indicates success or failure
//
HRESULT D3DMInitializer::D3DInitRawTest(LPWINDOW_ARGS pWindowArgs)
{
    D3DMDISPLAYMODE        Mode; // Current display mode
	D3DMCAPS               Caps;
    HRESULT                hr;

	if ((!m_hWnd) || (!m_pD3D))
	{
		DebugOut(DO_ERROR, _T("No valid HWND available\n"));
        return E_UNEXPECTED;
	}

	DebugOut(_T("Getting adapter display mode\n"));

	//
    // The current display mode is needed, to determine desired format
	//
    if( FAILED( hr = m_pD3D->GetAdapterDisplayMode( m_uiAdapterOrdinal, &Mode ) ) )
	{
		DebugOut(DO_ERROR, _T("Unable to determine current display mode. hr = 0x%08x\n"), hr);
		m_pD3D->Release();
		m_pD3D = NULL;
        return hr;
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = m_pD3D->GetDeviceCaps(m_uiAdapterOrdinal,  // UINT Adapter
	                                 D3DMDEVTYPE_DEFAULT, // D3DMDEVTYPE DeviceType
	                                 &Caps)))             // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, _T("GetDeviceCaps failed. hr = 0x%08x"), hr);
		m_pD3D->Release();
		m_pD3D = NULL;
        return hr;
	}

	//
	// Set up the presentation parameters that are desired for this 
	// testing scenario.
	//
    m_d3dpp.Windowed   = pWindowArgs->bPParmsWindowed;
    m_d3dpp.SwapEffect = D3DMSWAPEFFECT_DISCARD;
    m_d3dpp.BackBufferFormat = Mode.Format;
	m_d3dpp.BackBufferCount = 1;
	m_d3dpp.BackBufferWidth = pWindowArgs->uiWindowWidth; 
	m_d3dpp.BackBufferHeight = pWindowArgs->uiWindowHeight;
	m_d3dpp.MultiSampleType = D3DMMULTISAMPLE_NONE;

	if (Caps.SurfaceCaps & D3DMSURFCAPS_LOCKRENDERTARGET)
	{
		m_d3dpp.Flags = D3DMPRESENTFLAG_LOCKABLE_BACKBUFFER;
	}

	DebugOut(_T("Creating Direct3D device\n"));

	//
    // Create the D3DDevice
	//
	// Must specify multithreaded flag because tux will call TestProc with a different thread.
	//
    if( FAILED( hr = m_pD3D->CreateDevice( m_uiAdapterOrdinal, m_DevType, m_hWnd,
                                      m_dwBehaviorFlags, &m_d3dpp, &m_pd3dDevice ) ) )
    {
		DebugOut(DO_ERROR, _T("Failed at CreateDevice. hr = 0x%08x\n"), hr);
		m_pD3D->Release();
		m_pD3D = NULL;
        return hr;
    }

    return S_OK;
}

//
// D3DDepthStencilSupportedTest
//
//   This function initializes Direct3D for "raw" testing, with the addition of a 
//   depth/stencil buffer.  The test application will be left to initialize all
//   render states, transforms, etc.
//   
// Arguments:
//   
//   LPWINDOW_ARGS pWindowArgs:  Description of desired window characteristics
//
// Return Value:
// 
//   HRESULT:  Indicates success or failure
//
HRESULT D3DMInitializer::D3DDepthStencilSupportedTest(LPWINDOW_ARGS pWindowArgs)
{
    D3DMDISPLAYMODE        Mode; // Current display mode
	D3DMCAPS               Caps;
	HRESULT hr;

	//
	// D3DMFORMAT iterator; to cycle through formats until a valid depth/stencil
	// format is found
	//
	D3DMFORMAT CurrentFormat;

	//
	// Indicates whether the search for a valid depth/stencil format has been
	// successful or not
	//
	BOOL bFoundValidFormat = FALSE;


	if ((!m_hWnd) || (!m_pD3D))
	{
		DebugOut(DO_ERROR, _T("No valid HWND available\n"));
        return E_UNEXPECTED;
	}

	DebugOut(_T("Getting adapter display mode\n"));

	//
    // The current display mode is needed, to determine desired format
	//
    if( FAILED( hr = m_pD3D->GetAdapterDisplayMode( m_uiAdapterOrdinal, &Mode ) ) )
	{
		DebugOut(DO_ERROR, _T("Unable to determine current display mode. hr = 0x%08x\n"), hr);
		m_pD3D->Release();
		m_pD3D = NULL;
        return hr;
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = m_pD3D->GetDeviceCaps(m_uiAdapterOrdinal,  // UINT Adapter
	                                 D3DMDEVTYPE_DEFAULT, // D3DMDEVTYPE DeviceType
	                                 &Caps)))             // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, _T("GetDeviceCaps failed. hr = 0x%08x"), hr);
		m_pD3D->Release();
		m_pD3D = NULL;
        return hr;
	}

	//
	// Set up the presentation parameters that are desired for this 
	// testing scenario.
	//
    m_d3dpp.Windowed   = pWindowArgs->bPParmsWindowed;
    m_d3dpp.SwapEffect = D3DMSWAPEFFECT_DISCARD;
    m_d3dpp.BackBufferFormat = Mode.Format;
	m_d3dpp.BackBufferCount = 1;
	m_d3dpp.BackBufferWidth = pWindowArgs->uiWindowWidth; 
	m_d3dpp.BackBufferHeight = pWindowArgs->uiWindowHeight;
	m_d3dpp.MultiSampleType = D3DMMULTISAMPLE_NONE;
	m_d3dpp.EnableAutoDepthStencil = TRUE;

	//
	// Attempt to identify a valid depth/stencil format
	//
	for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
	{
		
		switch(CurrentFormat)
		{
		case D3DMFMT_D32:
		case D3DMFMT_D15S1:
		case D3DMFMT_D24S8:
		case D3DMFMT_D16:
		case D3DMFMT_D24X8:
		case D3DMFMT_D24X4S4:

			//
			// Continue iteration; this format is a D/S format
			//
			break;
		default:

			//
			// Skip iteration; this format is not a D/S format
			//
			continue;
		}



		if (SUCCEEDED(m_pD3D->CheckDepthStencilMatch(m_uiAdapterOrdinal,     // UINT Adapter
		                                             m_DevType,              // D3DMDEVTYPE DeviceType
		                                             Mode.Format,            // D3DMFORMAT AdapterFormat
		                                             Mode.Format,            // D3DMFORMAT RenderTargetFormat
		                                             CurrentFormat)))        // D3DMFORMAT DepthStencilFormat
		{
			bFoundValidFormat = TRUE;
			break;
		}
	}

	//
	// Was the search for a valid format successful?
	//
	if (TRUE == bFoundValidFormat)
	{
		m_d3dpp.AutoDepthStencilFormat = CurrentFormat;
	}
	else
	{
	    DebugOut(DO_ERROR, _T("DepthStencilSupportedTest failed, unable to find matching format"));
		m_pD3D->Release();
		m_pD3D = NULL;
        return E_ABORT;
	}

	if (Caps.SurfaceCaps & D3DMSURFCAPS_LOCKRENDERTARGET)
	{
		m_d3dpp.Flags = D3DMPRESENTFLAG_LOCKABLE_BACKBUFFER;
	}

	DebugOut(_T("Creating Direct3D device\n"));

	//
    // Create the D3DDevice
	//
	// Must specify multithreaded flag because tux will call TestProc with a different thread.
	//
    if( FAILED( hr = m_pD3D->CreateDevice( m_uiAdapterOrdinal, m_DevType, m_hWnd,
                                      m_dwBehaviorFlags, &m_d3dpp, &m_pd3dDevice ) ) )
    {
		DebugOut(DO_ERROR, _T("Failed at CreateDevice. hr = 0x%08x\n"), hr);
		m_pD3D->Release();
		m_pD3D = NULL;
        return hr;
    }

    return S_OK;
}


//
// D3DInitForWorldViewTest
//
//   This function initializes Direct3D for use in testing world/view matrices.
//   The proj matrix and viewport are setup such that they "multiply out" to
//   identity.
//   
// Arguments:
//   
//   LPWINDOW_ARGS pWindowArgs:  Description of desired window characteristics
//
// Return Value:
// 
//   HRESULT:  Indicates success or failure
//
HRESULT D3DMInitializer::D3DInitForWorldViewTest(LPWINDOW_ARGS pWindowArgs)
{

    D3DMDISPLAYMODE        Mode; // Current display mode
	D3DMCAPS               Caps;
	HRESULT hr;

	//
	// Initial World, View, and Projection Matrices
	//
	// These are setup so that the initial transformation pipeline is essentially
	// a multiplication by the identity matrix.
	//
	// Note that a multiplication of the ViewPort and Projection matrices is
	// the identity matrix.
	//
	D3DMMATRIX MatWorld ={_M( 1.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f),
		                  _M( 0.0f), _M( 1.0f), _M( 0.0f), _M( 0.0f),
		    	 		  _M( 0.0f), _M( 0.0f), _M( 1.0f), _M( 0.0f),
	 				      _M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 1.0f)};
	D3DMMATRIX MatView = {_M( 1.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f),
		                  _M( 0.0f), _M( 1.0f), _M( 0.0f), _M( 0.0f),
		    	 		  _M( 0.0f), _M( 0.0f), _M( 1.0f), _M( 0.0f),
		   		          _M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 1.0f)};
	D3DMMATRIX MatProj = {_M( 1.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f),
		                  _M( 0.0f), _M(-1.0f), _M( 0.0f), _M( 0.0f),
					      _M( 0.0f), _M( 0.0f), _M( 1.0f), _M( 0.0f),
					      _M(-1.0f), _M( 1.0f), _M( 0.0f), _M( 1.0f)};
	D3DMVIEWPORT ViewPort = {0,0,2,2,0.0f,1.0f};
	
	if ((!m_hWnd) || (!m_pD3D))
	{
		DebugOut(DO_ERROR, _T("No valid HWND available\n"));
        return E_UNEXPECTED;
	}

	DebugOut(_T("Getting adapter display mode\n"));

	//
    // The current display mode is needed, to determine desired format
	//
    if( FAILED( hr = m_pD3D->GetAdapterDisplayMode( m_uiAdapterOrdinal, &Mode ) ) )
	{
		DebugOut(DO_ERROR, _T("Unable to determine current display mode. hr = %08x\n"), hr);
		m_pD3D->Release();
		m_pD3D = NULL;
        return hr;
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = m_pD3D->GetDeviceCaps(m_uiAdapterOrdinal,  // UINT Adapter
	                                 D3DMDEVTYPE_DEFAULT, // D3DMDEVTYPE DeviceType
	                                 &Caps)))             // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, _T("GetDeviceCaps failed. hr = %08x"), hr);
		m_pD3D->Release();
		m_pD3D = NULL;
        return hr;
	}

	//
	// Set up the presentation parameters that are desired for this 
	// testing scenario.
	//
    m_d3dpp.Windowed   = pWindowArgs->bPParmsWindowed;
    m_d3dpp.SwapEffect = D3DMSWAPEFFECT_DISCARD;
    m_d3dpp.BackBufferFormat = Mode.Format;
	m_d3dpp.BackBufferCount = 1;
	m_d3dpp.BackBufferWidth = pWindowArgs->uiWindowWidth; 
	m_d3dpp.BackBufferHeight = pWindowArgs->uiWindowHeight;

	if (Caps.SurfaceCaps & D3DMSURFCAPS_LOCKRENDERTARGET)
	{
		m_d3dpp.Flags = D3DMPRESENTFLAG_LOCKABLE_BACKBUFFER;
	}

	DebugOut(_T("Creating Direct3D device\n"));

	//
    // Create the D3DDevice
	//
	// Must specify multithreaded flag because tux will call TestProc with a different thread.
	//
	//
    // Create the D3DDevice
	//
	// Must specify multithreaded flag because tux will call TestProc with a different thread.
	//
    if( FAILED( hr = m_pD3D->CreateDevice( m_uiAdapterOrdinal, m_DevType, m_hWnd,
                                      m_dwBehaviorFlags, &m_d3dpp, &m_pd3dDevice ) ) )
    {
		DebugOut(DO_ERROR, _T("Failed at CreateDevice. hr = %08x\n"), hr);
		m_pD3D->Release();
		m_pD3D = NULL;
        return hr;
    }

	DebugOut(_T("Setting up viewport\n"));

    //
	// Set the Viewport (Direct3D will create a matrix representation of the viewport
	// based on the parameters that are set in the D3DVIEWPORT8 structure)
	//
	if( FAILED( hr = m_pd3dDevice->SetViewport( &ViewPort )))
    {
		DebugOut(DO_ERROR, _T("Failed at SetViewPort, hr = 08x\n"), hr);
		m_pD3D->Release();
		m_pD3D = NULL;
		m_pd3dDevice->Release();
		m_pd3dDevice = NULL;
        return hr;
    }

	DebugOut(_T("Turning off clipping.\n"));

    //
    // Turn off clipping to make the outcome of ProcessVertices more consistent.
    //
	if( FAILED( hr = m_pd3dDevice->SetRenderState(D3DMRS_CLIPPING, FALSE)))
    {
		DebugOut(DO_ERROR, _T("Failed at SetRenderState (D3DMRS_CLIPPING), hr = %08x\n"), hr);
		m_pD3D->Release();
		m_pD3D = NULL;
		m_pd3dDevice->Release();
		m_pd3dDevice = NULL;
        return hr;
    }

    //
    // Set the world matrix for transformation
    //
	if( FAILED( hr = m_pd3dDevice->SetTransform( D3DMTS_WORLD, &MatWorld, D3DMFMT_D3DMVALUE_FLOAT )))
	{
		DebugOut(DO_ERROR, _T("Failure at SetTransform for world matrix. hr = %08x\n"), hr);
		m_pD3D->Release();
		m_pD3D = NULL;
		m_pd3dDevice->Release();
		m_pd3dDevice = NULL;
        return hr;
	}

    //
    // Set the view matrix for transformation
    //
	if( FAILED( hr = m_pd3dDevice->SetTransform( D3DMTS_VIEW, &MatView, D3DMFMT_D3DMVALUE_FLOAT )))
	{
		DebugOut(DO_ERROR, _T("Failure at SetTransform for view matrix. hr = 0x%08x\n"), hr);
		m_pD3D->Release();
		m_pD3D = NULL;
		m_pd3dDevice->Release();
		m_pd3dDevice = NULL;
        return hr;
	}

    //
    // Set the projection matrix for transformation
    //
	if( FAILED( hr = m_pd3dDevice->SetTransform( D3DMTS_PROJECTION, &MatProj, D3DMFMT_D3DMVALUE_FLOAT )))
	{
		DebugOut(DO_ERROR, _T("Failure at SetTransform for projection matrix. hr = %08x\n"), hr);
		m_pD3D->Release();
		m_pD3D = NULL;
		m_pd3dDevice->Release();
		m_pd3dDevice = NULL;
        return hr;
	}

    return S_OK;
}

//
//  GetObject
//
//    Accessor method for Direct3D object, which is typically initialized in the constructor,
//    but might be NULL in the case of failed initialization.
//
//  Return Value:
//
//    LPDIRECT3D8:  Direct3D Object to be used for testing.
//
LPDIRECT3DMOBILE D3DMInitializer::GetObject()
{
	return m_pD3D;
}

//
//  GetDevice
//
//    Accessor method for Direct3D device, which is typically initialized in the constructor,
//    but might be NULL in the case of failed initialization.
//
//  Return Value:
//
//    LPDIRECT3DDEVICE8:  Direct3D Device to be used for testing.
//
LPDIRECT3DMOBILEDEVICE D3DMInitializer::GetDevice()
{
	return m_pd3dDevice;
}

//
//  IsReady
//
//    Accessor method for "initialization state" of this object.
//
//  Return Value:
//
//    BOOL:  True if the object is in an initialized state; FALSE otherwise
//
BOOL D3DMInitializer::IsReady()
{
   return m_bInitSuccess;
}

//
//  Cleanup
//
//    Releases & cleans up members.
//
void D3DMInitializer::Cleanup()
{
	HMODULE hD3DMDLL;
	UINT uiAttempts = 0;

	if (m_pD3D)
		m_pD3D->Release();

	if (m_pd3dDevice)
		m_pd3dDevice->Release();

	if (m_pAppWindow)
        delete m_pAppWindow;

	if (m_hSWDeviceDLL)
		FreeLibrary(m_hSWDeviceDLL);

    if (m_bForceClean)
    {
    	while (hD3DMDLL = GetModuleHandle(D3DMDLL))
    	{
    		if (!FreeLibrary(hD3DMDLL))
    		    DebugOut(DO_ERROR, _T("FreeLibrary(D3DM.DLL) failed; GetLastError: %d.\n"), GetLastError());

    		if (D3DQA_MAX_UNLOAD_ATTEMPTS == ++uiAttempts)
    			break;
    	}
    }

    ClearMembers();
}

//
//  Cleanup
//
//    NULL-initializes (or zero-initializes) all member values
//
void D3DMInitializer::ClearMembers()
{
    m_hSWDeviceDLL = NULL;
    m_pD3D = NULL;
    m_pd3dDevice = NULL;
	m_hWnd = NULL;
	m_pAppWindow = NULL;
	m_uiAdapterOrdinal = 0;
	m_DevType = (D3DMDEVTYPE)0;
	m_dwBehaviorFlags = 0;
    m_bInitSuccess = FALSE;
    ZeroMemory( &m_d3dpp, sizeof(m_d3dpp) );
}

//
//  ~D3DMInitializer
//
//    Destructor for D3DMInitializer.  Releases & cleans up members.
//
D3DMInitializer::~D3DMInitializer()
{
    Cleanup();
}


//
//
// ShowTestCaseOrdinal
//
//   Draws test case ordinal number on window
//
// Arguments:
//
//   HWND hWnd:  Target window
//   UINT uiTestCase:  Test case number
//
// Return Value:
//
//   void
//
void D3DMInitializer::ShowTestCaseOrdinal(HWND hWnd, UINT uiTestCase)
{
    TCHAR tszBuffer[MAX_DEBUG_OUT];
	RECT  WndRect;
	RECT  WndRectTop;
	RECT  WndRectBottom;
	HDC   hDC;

    _sntprintf(
        tszBuffer,
		MAX_DEBUG_OUT,
        _T("#%u"),
        uiTestCase);

	tszBuffer[MAX_DEBUG_OUT-1] = '\0'; // Guarantee NULL termination, in worst case

	hDC = GetDC(hWnd);
	if (NULL == hDC)
	{
		DebugOut(DO_ERROR, _T("GetDC failed (non-fatal to test execution). Error: %d"), GetLastError());
		return;
	}

	if (0 == GetClientRect( hWnd, &WndRect))
	{
		DebugOut(DO_ERROR, _T("GetWindowRect failed (non-fatal to test execution). Error: %d"), GetLastError());
	}
	else
	{
		WndRectTop.left   = WndRect.left;
		WndRectTop.top    = WndRect.top;
		WndRectTop.right  = WndRect.right;
		WndRectTop.bottom = (WndRect.bottom - WndRect.top) / 2;

		WndRectBottom.left   = WndRect.left;
		WndRectBottom.top    = (WndRect.bottom - WndRect.top) / 2;
		WndRectBottom.right  = WndRect.right;
		WndRectBottom.bottom = WndRect.bottom;

		if (0 == DrawText(hDC,                     // HDC hDC, 
						  _T("Test Case:"), // LPCTSTR lpString, 
						  -1,                      // int nCount, 
						  &WndRectTop,             // LPRECT lpRect, 
						  DT_CENTER | DT_VCENTER)) // UNIT uFormat
		{
			DebugOut(DO_ERROR, _T("DrawText failed (non-fatal to test execution). Error: %d"), GetLastError());
		}

		if (0 == DrawText(hDC,                     // HDC hDC, 
						  tszBuffer,               // LPCTSTR lpString, 
						  -1,                      // int nCount, 
						  &WndRectBottom,          // LPRECT lpRect, 
						  DT_CENTER | DT_VCENTER)) // UNIT uFormat
		{
			DebugOut(DO_ERROR, _T("DrawText failed (non-fatal to test execution). Error: %d"), GetLastError());
		}
	}

	UpdateWindow(hWnd);

	ReleaseDC(hWnd, hDC);


}
