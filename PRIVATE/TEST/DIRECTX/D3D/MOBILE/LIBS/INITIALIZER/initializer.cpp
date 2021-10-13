//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "Initializer.h"
#include "DebugOutput.h"

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

	while (hD3DMDLL = GetModuleHandle(D3DMDLL))
	{
		FreeLibrary(hD3DMDLL);
		DebugOut(_T("FreeLibrary(D3DM.DLL); GetLastError: 0x%08lx.\n"), GetLastError());

		if (D3DQA_MAX_UNLOAD_ATTEMPTS == ++uiAttempts)
			break;
	}

	hLoadLibD3DMDLL = LoadLibrary(D3DMDLL); 
	DebugOut(_T("LoadLibrary(D3DM.DLL); GetLastError: 0x%08lx.\n"), GetLastError());

	if (NULL==hLoadLibD3DMDLL)
	{
		DebugOut(_T("Unable to load D3DM.DLL.  SYSGEN_D3DM is required for this test."));
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
	// Perhaps this object has been used previously; and is now being re-initialized for another
	// purpose.  Handle this case gracefully.
	//
	Cleanup();
	ClearMembers();

	OutputDebugString(_T("Creating D3DM object.\n"));

	//
	// Some args aren't currently configurable by this class
	//
	m_DevType = D3DQA_DEVTYPE;
	m_dwBehaviorFlags = D3DQA_BEHAVIOR_FLAGS;

	//
	// Create the D3D object.
	//
	if (FAILED(ForceCleanD3DM()))
	{
		OutputDebugString(_T("ForceCleanD3DM failed."));
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
		OutputDebugString(_T("GetProcAddress failed for Direct3DMobileCreate."));
		goto cleanup;
	}

	if(!( m_pD3D = pfnDirect3DMobileCreate( D3DM_SDK_VERSION ) ) )
	{
		OutputDebugString(_T("Direct3DMobileCreate failed."));
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
		OutputDebugString(_T("WindowSetup failed."));
		goto cleanup;
	}

	if (!(m_hWnd = m_pAppWindow->GetHandle()))
	{
		OutputDebugString(_T("GetHandle failed."));
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

		//
		// This function maps the specified DLL file into the address
		// space of the calling process. 
		//
		m_hSWDeviceDLL = LoadLibrary(pwszDriver);
		DebugOut(_T("LoadLibrary(pwszDriver); GetLastError: 0x%08lx.\n"), GetLastError());

		//
		// NULL indicates failure
		//
		if (NULL == m_hSWDeviceDLL)
		{
		    OutputDebugString(_T("LoadLibrary failed."));
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
			OutputDebugString(_T("GetProcAddress failed."));
			goto cleanup;
		}

		//
		// This method registers pluggable software device with Direct3D Mobile
		//
		if (FAILED(m_pD3D->RegisterSoftwareDevice(pSWDeviceEntry)))
		{
			OutputDebugString(_T("RegisterSoftwareDevice failed."));
			goto cleanup;
		}
	}
	
	switch(Purpose)
	{
	case D3DQA_PURPOSE_WORLD_VIEW_TEST:
		if (FAILED(D3DInitForWorldViewTest(pWindowArgs)))
            goto cleanup;
		break;
	case D3DQA_PURPOSE_SMALL_TOPMOST:
	case D3DQA_PURPOSE_RAW_TEST:
		if (FAILED(D3DInitRawTest(pWindowArgs)))
            goto cleanup;
		break;
	case D3DQA_PURPOSE_DEPTH_STENCIL_TEST:
		if (FAILED(D3DDepthStencilSupportedTest(pWindowArgs)))
            goto cleanup;
		break;
	case D3DQA_PURPOSE_TEXTURE_TEST:
	case D3DQA_PURPOSE_CLIP_STATUS_TEST:
	case D3DQA_PURPOSE_FULLPIPE_TEST:
	default:
		OutputDebugString(_T("D3DMInitializer: Not yet implemented.\n"));
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

	return E_FAIL;

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

	if ((!m_hWnd) || (!m_pD3D))
	{
		OutputDebugString(_T("No valid HWND available\n"));
        return E_ABORT;
	}

	OutputDebugString(_T("Getting adapter display mode\n"));

	//
    // The current display mode is needed, to determine desired format
	//
    if( FAILED( m_pD3D->GetAdapterDisplayMode( m_uiAdapterOrdinal, &Mode ) ) )
	{
		OutputDebugString(_T("Unable to determine current display mode.\n"));
		m_pD3D->Release();
		m_pD3D = NULL;
        return E_ABORT;
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(m_pD3D->GetDeviceCaps(m_uiAdapterOrdinal,  // UINT Adapter
	                                 D3DMDEVTYPE_DEFAULT, // D3DMDEVTYPE DeviceType
	                                 &Caps)))             // D3DMCAPS* pCaps
	{
		OutputDebugString(_T("GetDeviceCaps failed."));
		m_pD3D->Release();
		m_pD3D = NULL;
        return E_ABORT;
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

	OutputDebugString(_T("Creating Direct3D device\n"));

	//
    // Create the D3DDevice
	//
	// Must specify multithreaded flag because tux will call TestProc with a different thread.
	//
    if( FAILED( m_pD3D->CreateDevice( m_uiAdapterOrdinal, m_DevType, m_hWnd,
                                      m_dwBehaviorFlags, &m_d3dpp, &m_pd3dDevice ) ) )
    {
		OutputDebugString(_T("Failed at CreateDevice.\n"));
		m_pD3D->Release();
		m_pD3D = NULL;
        return E_ABORT;
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
		OutputDebugString(_T("No valid HWND available\n"));
        return E_ABORT;
	}

	OutputDebugString(_T("Getting adapter display mode\n"));

	//
    // The current display mode is needed, to determine desired format
	//
    if( FAILED( m_pD3D->GetAdapterDisplayMode( m_uiAdapterOrdinal, &Mode ) ) )
	{
		OutputDebugString(_T("Unable to determine current display mode.\n"));
		m_pD3D->Release();
		m_pD3D = NULL;
        return E_ABORT;
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(m_pD3D->GetDeviceCaps(m_uiAdapterOrdinal,  // UINT Adapter
	                                 D3DMDEVTYPE_DEFAULT, // D3DMDEVTYPE DeviceType
	                                 &Caps)))             // D3DMCAPS* pCaps
	{
		OutputDebugString(_T("GetDeviceCaps failed."));
		m_pD3D->Release();
		m_pD3D = NULL;
        return E_ABORT;
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
		m_pD3D->Release();
		m_pD3D = NULL;
        return E_ABORT;
	}

	if (Caps.SurfaceCaps & D3DMSURFCAPS_LOCKRENDERTARGET)
	{
		m_d3dpp.Flags = D3DMPRESENTFLAG_LOCKABLE_BACKBUFFER;
	}

	OutputDebugString(_T("Creating Direct3D device\n"));

	//
    // Create the D3DDevice
	//
	// Must specify multithreaded flag because tux will call TestProc with a different thread.
	//
    if( FAILED( m_pD3D->CreateDevice( m_uiAdapterOrdinal, m_DevType, m_hWnd,
                                      m_dwBehaviorFlags, &m_d3dpp, &m_pd3dDevice ) ) )
    {
		OutputDebugString(_T("Failed at CreateDevice.\n"));
		m_pD3D->Release();
		m_pD3D = NULL;
        return E_ABORT;
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
		OutputDebugString(_T("No valid HWND available\n"));
        return E_ABORT;
	}

	OutputDebugString(_T("Getting adapter display mode\n"));

	//
    // The current display mode is needed, to determine desired format
	//
    if( FAILED( m_pD3D->GetAdapterDisplayMode( m_uiAdapterOrdinal, &Mode ) ) )
	{
		OutputDebugString(_T("Unable to determine current display mode.\n"));
		m_pD3D->Release();
		m_pD3D = NULL;
        return E_ABORT;
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(m_pD3D->GetDeviceCaps(m_uiAdapterOrdinal,  // UINT Adapter
	                                 D3DMDEVTYPE_DEFAULT, // D3DMDEVTYPE DeviceType
	                                 &Caps)))             // D3DMCAPS* pCaps
	{
		OutputDebugString(_T("GetDeviceCaps failed."));
		m_pD3D->Release();
		m_pD3D = NULL;
        return E_ABORT;
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

	OutputDebugString(_T("Creating Direct3D device\n"));

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
    if( FAILED( m_pD3D->CreateDevice( m_uiAdapterOrdinal, m_DevType, m_hWnd,
                                      m_dwBehaviorFlags, &m_d3dpp, &m_pd3dDevice ) ) )
    {
		OutputDebugString(_T("Failed at CreateDevice.\n"));
		m_pD3D->Release();
		m_pD3D = NULL;
        return E_ABORT;
    }

	OutputDebugString(_T("Setting up viewport\n"));

    //
	// Set the Viewport (Direct3D will create a matrix representation of the viewport
	// based on the parameters that are set in the D3DVIEWPORT8 structure)
	//
	if( FAILED( m_pd3dDevice->SetViewport( &ViewPort )))
    {
		OutputDebugString(_T("Failed at SetViewPort\n"));
		m_pD3D->Release();
		m_pD3D = NULL;
		m_pd3dDevice->Release();
		m_pd3dDevice = NULL;
        return E_ABORT;
    }

	OutputDebugString(_T("Turning off clipping.\n"));

    //
    // Turn off clipping to make the outcome of ProcessVertices more consistent.
    //
	if( FAILED( m_pd3dDevice->SetRenderState(D3DMRS_CLIPPING, FALSE)))
    {
		OutputDebugString(_T("Failed at SetRenderState (D3DMRS_CLIPPING)\n"));
		m_pD3D->Release();
		m_pD3D = NULL;
		m_pd3dDevice->Release();
		m_pd3dDevice = NULL;
        return E_ABORT;
    }

    //
    // Set the world matrix for transformation
    //
	if( FAILED( m_pd3dDevice->SetTransform( D3DMTS_WORLD, &MatWorld, D3DMFMT_D3DMVALUE_FLOAT )))
	{
		OutputDebugString(_T("Failure at SetTransform for world matrix.\n"));
		m_pD3D->Release();
		m_pD3D = NULL;
		m_pd3dDevice->Release();
		m_pd3dDevice = NULL;
        return E_ABORT;
	}

    //
    // Set the view matrix for transformation
    //
	if( FAILED( m_pd3dDevice->SetTransform( D3DMTS_VIEW, &MatView, D3DMFMT_D3DMVALUE_FLOAT )))
	{
		OutputDebugString(_T("Failure at SetTransform for view matrix.\n"));
		m_pD3D->Release();
		m_pD3D = NULL;
		m_pd3dDevice->Release();
		m_pd3dDevice = NULL;
        return E_ABORT;
	}

    //
    // Set the projection matrix for transformation
    //
	if( FAILED( m_pd3dDevice->SetTransform( D3DMTS_PROJECTION, &MatProj, D3DMFMT_D3DMVALUE_FLOAT )))
	{
		OutputDebugString(_T("Failure at SetTransform for projection matrix.\n"));
		m_pD3D->Release();
		m_pD3D = NULL;
		m_pd3dDevice->Release();
		m_pd3dDevice = NULL;
        return E_ABORT;
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

	while (hD3DMDLL = GetModuleHandle(D3DMDLL))
	{
		FreeLibrary(hD3DMDLL);
		DebugOut(_T("FreeLibrary(D3DM.DLL); GetLastError: 0x%08lx.\n"), GetLastError());

		if (D3DQA_MAX_UNLOAD_ATTEMPTS == ++uiAttempts)
			break;
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
		OutputDebugString(_T("GetDC failed (non-fatal to test execution)."));
		return;
	}

	if (0 == GetClientRect( hWnd, &WndRect))
	{
		OutputDebugString(_T("GetWindowRect failed (non-fatal to test execution)."));
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
			OutputDebugString(_T("DrawText failed (non-fatal to test execution)."));
		}

		if (0 == DrawText(hDC,                     // HDC hDC, 
						  tszBuffer,               // LPCTSTR lpString, 
						  -1,                      // int nCount, 
						  &WndRectBottom,          // LPRECT lpRect, 
						  DT_CENTER | DT_VCENTER)) // UNIT uFormat
		{
			OutputDebugString(_T("DrawText failed (non-fatal to test execution)."));
		}
	}

	UpdateWindow(hWnd);

	ReleaseDC(hWnd, hDC);


}
