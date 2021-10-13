//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <tchar.h>
#include <stressutils.h>
#include <d3dm.h>
#include "qad3dmx.h"
#include <clparse.h>   // Command-line parsing utility

// Allow floats to be assigned to D3DMVALUEs
#define _M(a) D3DM_MAKE_D3DMVALUE(a)
#define _F(a) (*(float*)&a)

/////////////////////////////////////////////////////////////////////////////////////
const int THREADCOUNT = 1;
const TCHAR gszClassName[] = _T("s2_d3d");

// Filename of driver to register
static TCHAR g_tszSoftwareDeviceFilename[MAX_PATH];
static UINT g_uiAdapterOrdinal;
static HINSTANCE g_hSWDeviceDLL = NULL;

/////////////////////////////////////////////////////////////////////////////////////
#define countof(x)      (sizeof(x)/sizeof(*(x)))
#define SAFERELEASE(x)  if(x) { (x)->Release();   (x) = NULL; }
#define SAFEDEL(x)      if(x) { delete (x);       (x) = NULL; }
#define CHK(fn)                                                             \
do {                                                                        \
    HRESULT hRet = (fn);                                                    \
    if (hRet != D3DM_OK) {                                                   \
        LogFail(_T("%s failed [%x]\n"), _T(#fn), hRet);                 \
        hr = hRet;                                                          \
    }                                                                       \
} while(0)
#define CHK_CLEANUP(fn)                                                     \
do {                                                                        \
    HRESULT hRet = (fn);                                                    \
    if (hRet != D3DM_OK) {                                                   \
        LogFail(_T("%s failed [0x%08x]\n"), _T(#fn), hRet);             \
        goto cleanup;                                                       \
    }                                                                       \
} while(0)


/////////////////////////////////////////////////////////////////////////////////////
enum kRenderCall {
    kNone       = 0,
    kTriStrip,
    kTriList,
    kTriFan,
    kLineStrip,

    kRenderCallMax
};

/////////////////////////////////////////////////////////////////////////////////////
// A structure for untransformed, lit, textured vertex types
struct LIT_VERTEX_TYPE
{
    FLOAT x, y, z;       // The untransformed position for the vertex
    D3DMCOLOR color;     // The diffuse vertex color
    D3DMCOLOR specular;  // The specular vertex color
};
// Our custom FVF, which describes a lit vertex structure
#define LIT_VERTEX_FVF (D3DMFVF_XYZ_FLOAT|D3DMFVF_DIFFUSE|D3DMFVF_SPECULAR)

/////////////////////////////////////////////////////////////////////////////////////
class THREADDATA
{
public:
    THREADDATA() { Init(); }
    ~THREADDATA() { Clear(); }
    void Init(void);
    void Clear(void);

    HWND                         m_hwnd;
    int                          m_nIterationCount;
    IDirect3DMobile             *m_pD3D;
    IDirect3DMobileDevice       *m_pD3DDev;
    IDirect3DMobileVertexBuffer *m_pVBIndexedCube,
                                *m_pVBTriList,
                                *m_pVBTriStrip1,
                                *m_pVBTriStrip2,
                                *m_pVBTriFan1,
                                *m_pVBTriFan2;
    IDirect3DMobileIndexBuffer  *m_pIBTriList,
                                *m_pIBTriStrip1,
                                *m_pIBTriStrip2,
                                *m_pIBTriFan1,
                                *m_pIBTriFan2;
    D3DMMATRIX                   m_matRot,
                                 m_matPos;
};

/////////////////////////////////////////////////////////////////////////////////////
const float PI     = 3.141592654f;
const float TWOPI  = 2 * PI;

/////////////////////////////////////////////////////////////////////////////////////
static HINSTANCE g_hInst = NULL;
static DWORD g_dwTlsIndex = 0xFFFFFFFF;
static int g_nThreadCount = 0;
static THREADDATA g_rgThreadData[THREADCOUNT];
static bool g_bActive       = true,
            g_bRefRast      = false,
            g_bAutoRot      = true,
            g_bDrawIndexed  = false,
            g_bDrawUserMem  = false,
            g_bOneCube      = true;

/////////////////////////////////////////////////////////////////////////////////////
// The cube geometry
// Cube corners
D3DMVECTOR g_rgvectCube[] = {
    {_M( 1.0f), _M( 1.0f), _M( 1.0f)},
    {_M(-1.0f), _M( 1.0f), _M( 1.0f)},
    {_M( 1.0f), _M(-1.0f), _M( 1.0f)},
    {_M(-1.0f), _M(-1.0f), _M( 1.0f)},
    {_M( 1.0f), _M( 1.0f), _M(-1.0f)},
    {_M(-1.0f), _M( 1.0f), _M(-1.0f)},
    {_M( 1.0f), _M(-1.0f), _M(-1.0f)},
    {_M(-1.0f), _M(-1.0f), _M(-1.0f)}
};

// Cube vertices
LIT_VERTEX_TYPE g_rgvertIndexedCube[] = {
    { _F(g_rgvectCube[0].x), _F(g_rgvectCube[0].y), _F(g_rgvectCube[0].z), 0x00FFFFFF, 0x00FFFFFF },
    { _F(g_rgvectCube[1].x), _F(g_rgvectCube[1].y), _F(g_rgvectCube[1].z), 0x0000FFFF, 0x0000FFFF },
    { _F(g_rgvectCube[2].x), _F(g_rgvectCube[2].y), _F(g_rgvectCube[2].z), 0x00FF00FF, 0x00FF00FF },
    { _F(g_rgvectCube[3].x), _F(g_rgvectCube[3].y), _F(g_rgvectCube[3].z), 0x000000FF, 0x000000FF },
    { _F(g_rgvectCube[4].x), _F(g_rgvectCube[4].y), _F(g_rgvectCube[4].z), 0x00FFFF00, 0x00FFFF00 },
    { _F(g_rgvectCube[5].x), _F(g_rgvectCube[5].y), _F(g_rgvectCube[5].z), 0x0000FF00, 0x0000FF00 },
    { _F(g_rgvectCube[6].x), _F(g_rgvectCube[6].y), _F(g_rgvectCube[6].z), 0x00FF0000, 0x00FF0000 },
    { _F(g_rgvectCube[7].x), _F(g_rgvectCube[7].y), _F(g_rgvectCube[7].z), 0x00000000, 0x00000000 }
};

// One Triangle List
WORD g_rgwTriList[] = { 0, 1, 3,   3, 2, 0,   5, 1, 0,   5, 0, 4,
                        7, 5, 4,   7, 4, 6,   4, 0, 2,   4, 2, 6,
                        7, 6, 2,   7, 2, 3,   5, 7, 3,   5, 3, 1 };

// Two triangle strips
WORD g_rgwTriStrip1[] = { 3, 2, 1, 0, 5, 4, 7, 6 };
WORD g_rgwTriStrip2[] = { 4, 0, 6, 2, 7, 3, 5, 1 };

// Two Trangle fans
WORD g_rgwTriFan1[] = { 0, 4, 5, 1, 3, 2, 6, 4 };
WORD g_rgwTriFan2[] = { 7, 5, 4, 6, 2, 3, 1, 5 };

// Non-indexed triangle list
LIT_VERTEX_TYPE g_rgvertTriList[countof(g_rgwTriList)];
// Two non-indexed triangle strips
LIT_VERTEX_TYPE g_rgvertTriStrip1[countof(g_rgwTriStrip1)];
LIT_VERTEX_TYPE g_rgvertTriStrip2[countof(g_rgwTriStrip2)];
// Two non-indexed triangle fans
LIT_VERTEX_TYPE g_rgvertTriFan1[countof(g_rgwTriFan1)];
LIT_VERTEX_TYPE g_rgvertTriFan2[countof(g_rgwTriFan2)];

/////////////////////////////////////////////////////////////////////////////////////
void THREADDATA::Init(void)
{
    m_hwnd               = NULL;
    m_nIterationCount    = 0;
    m_pD3D               = NULL;
    m_pD3DDev            = NULL;
    m_pVBIndexedCube     = NULL;
    m_pVBTriList         = NULL;
    m_pVBTriStrip1       = NULL;
    m_pVBTriStrip2       = NULL;
    m_pVBTriFan1         = NULL;
    m_pVBTriFan2         = NULL;
    m_pIBTriList         = NULL;
    m_pIBTriStrip1       = NULL;
    m_pIBTriStrip2       = NULL;
    m_pIBTriFan1         = NULL;
    m_pIBTriFan2         = NULL;
    
    D3DMatrixIdentity(&m_matRot);
    D3DMatrixIdentity(&m_matPos);
}

/////////////////////////////////////////////////////////////////////////////////////
void THREADDATA::Clear(void)
{
    SAFERELEASE(m_pIBTriFan2);
    SAFERELEASE(m_pIBTriFan1);
    SAFERELEASE(m_pIBTriStrip2);
    SAFERELEASE(m_pIBTriStrip1);
    SAFERELEASE(m_pIBTriList);
    SAFERELEASE(m_pVBTriFan2);
    SAFERELEASE(m_pVBTriFan1);
    SAFERELEASE(m_pVBTriStrip2);
    SAFERELEASE(m_pVBTriStrip1);
    SAFERELEASE(m_pVBTriList);
    SAFERELEASE(m_pVBIndexedCube);
    SAFERELEASE(m_pD3DDev);
    SAFERELEASE(m_pD3D);

    if (NULL != m_hwnd)
    {
        BOOL fRet = DestroyWindow(m_hwnd);
        m_hwnd = NULL;

        if (0 == fRet)
        {
            LogWarn2(_T("Unexpected error code from DestroyWindow"));
        }
        else
        {
            LogVerbose(_T("Window successfully destroyed"));
        }
    }
    
    Init();
}

/////////////////////////////////////////////////////////////////////////////////////
LONG CALLBACK WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return (DefWindowProc(hwnd, message, wParam, lParam));
}

/////////////////////////////////////////////////////////////////////////////////////
void DoMsgPump(DWORD dwMilliSeconds)
{
    // Main message loop:
    MSG msg;
    DWORD dwStart = GetTickCount();

    while (GetTickCount() < dwStart + dwMilliSeconds)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////
BOOL InitApplication (HINSTANCE hInstance)
{
    WNDCLASS  wc;
    wc.style = 0;
    wc.lpfnWndProc = (WNDPROC) WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = 0;
    wc.hCursor = 0;
    wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = gszClassName;
    return  (RegisterClass(&wc));
}


/////////////////////////////////////////////////////////////////////////////////////
HRESULT RegisterDevice(LPDIRECT3DMOBILE pD3D, TCHAR *tszSoftwareDeviceFilename)
{
	//
	// Function pointer to driver initialization function for RegisterSoftwareDevice
	//
	FARPROC pSWDeviceEntry = NULL;

	//
	// Result of function; set to failure in all error conditions
	//
	HRESULT hr = S_OK;

	//
	// This function maps the specified DLL file into the address
	// space of the calling process. 
	//
	g_hSWDeviceDLL = LoadLibrary(tszSoftwareDeviceFilename);

	//
	// NULL indicates failure
	//
	if (NULL == g_hSWDeviceDLL)
	{
		LogFail(_T("LoadLibrary failed."));
        hr = E_FAIL;
        goto cleanup;
	}

	//
	// This function returns the address of the specified exported DLL function. 
	//
	pSWDeviceEntry = GetProcAddress(g_hSWDeviceDLL,         // HMODULE hModule, 
                                    _T("D3DM_Initialize")); // LPCWSTR lpProcName
	//
	// NULL indicates failure.
	//
 	if (NULL == pSWDeviceEntry)
	{
		LogFail(_T("GetProcAddress failed."));
		hr = E_FAIL;
        goto cleanup;
	}

	//
	// This method registers pluggable software device with Direct3D Mobile
	//
	if (FAILED(pD3D->RegisterSoftwareDevice(pSWDeviceEntry)))
	{
		LogFail(_T("RegisterSoftwareDevice failed."));
		hr = E_FAIL;
        goto cleanup;
	}

cleanup:

	if (FAILED(hr))
	{
		if (g_hSWDeviceDLL)
			FreeLibrary(g_hSWDeviceDLL);
	}

	return hr;

}

/////////////////////////////////////////////////////////////////////////////////////
void SetupGeometry(void)
{
    int i;

    // Create some non-indexed data
    for (i = 0; i < countof(g_rgvertTriList); i++)
    {
        g_rgvertTriList[i] = g_rgvertIndexedCube[g_rgwTriList[i]];
    }
    for (i = 0; i < countof(g_rgvertTriStrip1); i++)
    {
        g_rgvertTriStrip1[i] = g_rgvertIndexedCube[g_rgwTriStrip1[i]];
    }
    for (i = 0; i < countof(g_rgvertTriStrip2); i++)
    {
        g_rgvertTriStrip2[i] = g_rgvertIndexedCube[g_rgwTriStrip2[i]];
    }
    for (i = 0; i < countof(g_rgvertTriFan1); i++)
    {
        g_rgvertTriFan1[i] = g_rgvertIndexedCube[g_rgwTriFan1[i]];
    }
    for (i = 0; i < countof(g_rgvertTriFan2); i++)
    {
        g_rgvertTriFan2[i] = g_rgvertIndexedCube[g_rgwTriFan2[i]];
    }
}

/////////////////////////////////////////////////////////////////////////////////////
BOOL InitializeStressModule (
                            /*[in]*/ MODULE_PARAMS* pmp, 
                            /*[out]*/ UINT* pnThreads
                            )
{
	//
	// Command-line parsing utility
	//
	CClParse CmdLineParse(pmp->tszUser);

	//
	// Is there a cmd-line specified driver?
	//
	if (CmdLineParse.GetOptString(_T("s"), g_tszSoftwareDeviceFilename, MAX_PATH))
	{
		//
		// Command-line indicates that a registered software device should be used
		//
		g_uiAdapterOrdinal = D3DMADAPTER_REGISTERED_DEVICE;
	}
	else
	{
		//
		// Default to system driver
		//
		g_tszSoftwareDeviceFilename[0] = _T('\0');
		g_uiAdapterOrdinal = D3DMADAPTER_DEFAULT;
	}


    // Set this value to indicate the number of threads
    // that you want your module to run.  The module container
    // that loads this DLL will manage the life-time of these
    // threads.  Each thread will call your DoStressIteration()
    // function in a loop.

    *pnThreads = THREADCOUNT;
    
    // !! You must call this before using any stress utils !!


    InitializeStressUtils (
                            _T("s2_d3d"),                                        // Module name to be used in logging
                            LOGZONE(SLOG_SPACE_GDI, SLOG_DRAW | SLOG_BITMAP),    // Logging zones used by default
                            pmp                                                    // Forward the Module params passed on the cmd line
                            );

    // Initialize the thread-local-storage
    g_dwTlsIndex = TlsAlloc();
    if (0xFFFFFFFF == g_dwTlsIndex)
    {
        LogWarn1(_T("Unable to initialize ThreadLocalStorage, GetLastError: %d."),GetLastError());
        return FALSE;
    }
    
    // Note on Logging Zones: 
    //
    // Logging is filtered at two different levels: by "logging space" and
    // by "logging zones".  The 16 logging spaces roughly corresponds to the major
    // feature areas in the system (including apps).  Each space has 16 sub-zones
    // for a finer filtering granularity.
    //
    // Use the LOGZONE(space, zones) macro to indicate the logging space
    // that your module will log under (only one allowed) and the zones
    // in that space that you will log under when the space is enabled
    // (may be multiple OR'd values).
    //
    // See \test\stress\stress\inc\logging.h for a list of available
    // logging spaces and the zones that are defined under each space.


    // TODO:  Any module-specific initialization here

    if (!InitApplication(g_hInst))
    {
        LogFail(_T("Unable to initialize application"));
        return FALSE;
    }

    SetupGeometry();

    TCHAR tszModuleFileName[MAX_PATH];

	
    if (!(GetModuleFileName((HINSTANCE) g_hInst, tszModuleFileName, MAX_PATH)))
	{
		//Zero indicates failure.
        LogWarn1(_T("Unable to GetModuleFileName, GetLastError: %d."), GetLastError());
		return FALSE;
	}

	//Avoid Prefast Warning #53 (Call to GetModuleFileName may not zero-terminate string)
	tszModuleFileName[MAX_PATH - 1] = 0;

    LogComment(_T("Module File Name: %s"), tszModuleFileName);
    
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////
BOOL CreateAppWindow(HINSTANCE hInstance, HWND *phWnd)
{
    RECT rcWnd;
    int nMinHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN) / 2;
    int nMinWidth  = GetSystemMetrics(SM_CXVIRTUALSCREEN) / 2;
    
    rcWnd.top     = rand() % (GetSystemMetrics(SM_CYVIRTUALSCREEN) / 2);
    rcWnd.left    = rand() % (GetSystemMetrics(SM_CXVIRTUALSCREEN) / 2);
    rcWnd.bottom= rand() % (GetSystemMetrics(SM_CYVIRTUALSCREEN) - rcWnd.top);
    rcWnd.right    = rand() % (GetSystemMetrics(SM_CXVIRTUALSCREEN) - rcWnd.left);

    if (rcWnd.bottom < nMinHeight) rcWnd.bottom = nMinHeight;
    if (rcWnd.right  < nMinWidth)  rcWnd.right  = nMinWidth;

    *phWnd = CreateWindowEx(
            0,
            gszClassName,
            gszClassName,
            WS_OVERLAPPED | WS_CAPTION | WS_BORDER | WS_VISIBLE,
            rcWnd.left, rcWnd.top, rcWnd.right, rcWnd.bottom,
            NULL,
            NULL,
            hInstance,
            NULL);
    
    LogComment(_T("Rect: { %d, %d, %d, %d }"), rcWnd.left, rcWnd.top, rcWnd.right, rcWnd.bottom);

    return (*phWnd) ? TRUE : FALSE;
}


/////////////////////////////////////////////////////////////////////////////////////
HRESULT InitDirect3D(THREADDATA *pData)
{
    HRESULT hr = S_OK;

    // Direct3D Mobile has only one device type
    D3DMDEVTYPE device = D3DMDEVTYPE_DEFAULT;

    // Valid pool for vertex buffers
    D3DMPOOL VertexBufPool;

    // Valid pool for index buffers
    D3DMPOOL IndexBufPool;

    // Get the device CAPS
    D3DMCAPS caps;

	//    
	// Create the Direct3D Mobile object
	//
    pData->m_pD3D = Direct3DMobileCreate(D3DM_SDK_VERSION);
    if (NULL == pData->m_pD3D)
    {
        LogFail(_T("Direct3DCreate Failed\n"));
        return FALSE;
    }

	//
	// If cmd-line parser has discovered a driver filename, register the device
	//
	if (D3DMADAPTER_REGISTERED_DEVICE == g_uiAdapterOrdinal)
	{
		//
		// Register the Direct3D Mobile device
		//
		if (FAILED(RegisterDevice(pData->m_pD3D,                 // LPDIRECT3DMOBILE *pD3D,
		                          g_tszSoftwareDeviceFilename))) // TCHAR *tszSoftwareDeviceFilename
		{
			LogFail(_T("RegisterDevice Failed\n"));
			return FALSE;
		}
	}

    hr = pData->m_pD3D->GetDeviceCaps(g_uiAdapterOrdinal, device, &caps);

	// Verify that expected render state settings are supported
	if (!(caps.ShadeCaps & D3DMPSHADECAPS_COLORGOURAUDRGB))
	{
        LogFail(_T("Failing; test for use on drivers that support D3DMPSHADECAPS_COLORGOURAUDRGB.\n"));
        return E_FAIL;
	}

    // Verify that some pool is available for vertex buffers
    if (D3DMSURFCAPS_SYSVERTEXBUFFER & caps.SurfaceCaps)
    {
        VertexBufPool = D3DMPOOL_SYSTEMMEM;
    }
    else if (D3DMSURFCAPS_VIDVERTEXBUFFER & caps.SurfaceCaps)
    {
        VertexBufPool = D3DMPOOL_VIDEOMEM;
    }
    else
    {
        LogFail(_T("Fatal error; no pool supported for vertex buffers in D3DMCAPS.SurfaceCaps."));
        return E_FAIL;
    }

    // Verify that some pool is available for index buffers
    if (D3DMSURFCAPS_SYSINDEXBUFFER & caps.SurfaceCaps)
    {
        IndexBufPool = D3DMPOOL_SYSTEMMEM;
    }
    else if (D3DMSURFCAPS_VIDINDEXBUFFER & caps.SurfaceCaps)
    {
        IndexBufPool = D3DMPOOL_VIDEOMEM;
    }
    else
    {
        LogFail(_T("Fatal error; no pool supported for vertex buffers in D3DMCAPS.SurfaceCaps."));
        return E_FAIL;
    }

    // Get the display info
    D3DMDISPLAYMODE d3ddm;
    if (FAILED(pData->m_pD3D->GetAdapterDisplayMode(g_uiAdapterOrdinal, &d3ddm)))
    {
        LogFail(_T("GetAdapterDisplayMode Failed\n"));
        return E_FAIL;
    }

    // Set the device parameters
    D3DMPRESENT_PARAMETERS d3dpp;
    memset(&d3dpp, 0, sizeof(d3dpp));
    d3dpp.BackBufferFormat = d3ddm.Format; //D3DMFMT_UNKNOWN;
    d3dpp.BackBufferCount = 1;
    d3dpp.SwapEffect = D3DMSWAPEFFECT_COPY; //D3DMSWAPEFFECT_FLIP;
    d3dpp.Windowed = TRUE;
    d3dpp.EnableAutoDepthStencil = FALSE;
    d3dpp.AutoDepthStencilFormat = D3DMFMT_D16;

    // Create the device
    hr = pData->m_pD3D->CreateDevice(g_uiAdapterOrdinal,
                                     device,
                                     pData->m_hwnd,
                                     D3DMCREATE_MULTITHREADED,
                                     &d3dpp,
                                     &pData->m_pD3DDev);
    if (FAILED(hr))
    {
        LogFail(_T("Unable to create device"));
        return hr;
    }

    // Alias the device for simple coding
    IDirect3DMobileDevice* &pDev = pData->m_pD3DDev;

    D3DMSURFACE_DESC d3dsd;
    IDirect3DMobileSurface *pBackBuffer = NULL;
    hr = pDev->GetBackBuffer(0, D3DMBACKBUFFER_TYPE_MONO, &pBackBuffer);
    pBackBuffer->GetDesc(&d3dsd);
    SAFERELEASE(pBackBuffer);

    // One-time device state configuration

    // Turn off culling
    CHK(pDev->SetRenderState( D3DMRS_CULLMODE, D3DMCULL_CCW ) );

    // Turn off the zbuffer
    CHK(pDev->SetRenderState( D3DMRS_ZENABLE, FALSE ) );

    // Turn off D3D Lighting
    CHK(pDev->SetRenderState( D3DMRS_LIGHTING, FALSE ) );

    // Turn on Gourad shading
    CHK(pDev->SetRenderState( D3DMRS_SHADEMODE, D3DMSHADE_GOURAUD ) );

    // Turn off dithering
    CHK(pDev->SetRenderState( D3DMRS_DITHERENABLE, FALSE ) );

    // Set up the ambient light
    //CHK(pDev->SetRenderState( D3DRS_AMBIENT, D3DCOLOR_RGBA( 0xFF, 0xFF, 0xFF, 0xFF ) ) );

    // Set the material properties
    D3DMMATERIAL mtrl;
    ZeroMemory( &mtrl, sizeof(D3DMMATERIAL) );
    mtrl.Diffuse.r = _M(0.7f);
    mtrl.Diffuse.g = _M(0.7f);
    mtrl.Diffuse.b = _M(0.7f);
    mtrl.Diffuse.a = _M(0.0f);
    mtrl.Ambient.r = _M(0.7f);
    mtrl.Ambient.g = _M(0.7f);
    mtrl.Ambient.b = _M(0.7f);
    mtrl.Ambient.a = _M(0.0f);
    mtrl.Specular.r = _M(0.1f);
    mtrl.Specular.g = _M(0.1f);
    mtrl.Specular.b = _M(0.1f);
    mtrl.Specular.a = _M(0.0f);
    CHK(pDev->SetMaterial( &mtrl, D3DMFMT_D3DMVALUE_FLOAT) );
           
    // Set up the world, view, and projection transforms
    // Get the identity matrix
    D3DMMATRIX matIdentity;
    D3DMatrixIdentity(&matIdentity);

    // World matrix
    pData->m_matRot = matIdentity;
    pData->m_matPos = matIdentity;
    CHK(pDev->SetTransform(D3DMTS_WORLD, &pData->m_matPos, D3DMFMT_D3DMVALUE_FLOAT));

    // The view (camera) matrix -- just back 10 units on z-axis
    D3DMMATRIX matView = matIdentity;
    matView._43 = _M(10.0f);
    CHK(pDev->SetTransform(D3DMTS_VIEW, &matView, D3DMFMT_D3DMVALUE_FLOAT));

    // The projection matrix
    /*    D3DMATRIX matProj = matIdentity;
    matProj._11 =  3.0f;
    matProj._22 =  3.0f;
    matProj._34 =  1.0f;
    matProj._43 = -1.0f;
    matProj._44 =  0.0f;*/

    // Generate a perspective projection matrix
    const float Zn =  5.0f,        // Near clipping plane
                Zf = 25.0f,        // Far clipping plane
                Vh =  3.0f;        // Height of near clipping plane
            
    float Vw = Vh * (float)d3dsd.Width / (float)d3dsd.Height, // Width of near clipping plane
          Q  = Zf / (Zf - Zn);

    D3DMMATRIX matProj = {
    _M(2.0f * Zn / Vw), _M(0.000000f)     , _M(0.000000f),  _M(0.000000f), 
    _M(0.000000f)     , _M(2.0f * Zn / Vh), _M(0.000000f),  _M(0.000000f), 
    _M(0.000000f)     , _M(0.000000f)     , _M(Q)        ,  _M(1.000000f), 
    _M(0.000000f)     , _M(0.000000f)     , _M(-Q * Zn)  ,  _M(0.000000f) };

    CHK(pDev->SetTransform(D3DMTS_PROJECTION, &matProj, D3DMFMT_D3DMVALUE_FLOAT));

    // Create and fill vertex and index buffers (instead of user memory)
    #define FILL_BUFFER(p, data) \
    do { \
        BYTE *pb = NULL; \
        CHK_CLEANUP(p->Lock(0, sizeof(data), (VOID**)&pb, D3DMLOCK_DISCARD)); \
        memcpy(pb, data, sizeof(data)); \
        CHK_CLEANUP(p->Unlock()); \
    } while(0)

    // Create the vertex buffers
    DWORD dwUsage = D3DMUSAGE_WRITEONLY | D3DMUSAGE_DYNAMIC;

    CHK_CLEANUP(pDev->CreateVertexBuffer(sizeof(g_rgvertIndexedCube), dwUsage, LIT_VERTEX_FVF, VertexBufPool, &pData->m_pVBIndexedCube));
    CHK_CLEANUP(pDev->CreateVertexBuffer(sizeof(g_rgvertTriList),     dwUsage, LIT_VERTEX_FVF, VertexBufPool, &pData->m_pVBTriList));
    CHK_CLEANUP(pDev->CreateVertexBuffer(sizeof(g_rgvertTriStrip1),   dwUsage, LIT_VERTEX_FVF, VertexBufPool, &pData->m_pVBTriStrip1));
    CHK_CLEANUP(pDev->CreateVertexBuffer(sizeof(g_rgvertTriStrip2),   dwUsage, LIT_VERTEX_FVF, VertexBufPool, &pData->m_pVBTriStrip2));
    CHK_CLEANUP(pDev->CreateVertexBuffer(sizeof(g_rgvertTriFan1),     dwUsage, LIT_VERTEX_FVF, VertexBufPool, &pData->m_pVBTriFan1));
    CHK_CLEANUP(pDev->CreateVertexBuffer(sizeof(g_rgvertTriFan2),     dwUsage, LIT_VERTEX_FVF, VertexBufPool, &pData->m_pVBTriFan2));

    // Fill the vertex buffers
    FILL_BUFFER(pData->m_pVBIndexedCube, g_rgvertIndexedCube);
    FILL_BUFFER(pData->m_pVBTriList,     g_rgvertTriList);
    FILL_BUFFER(pData->m_pVBTriStrip1,   g_rgvertTriStrip1);
    FILL_BUFFER(pData->m_pVBTriStrip2,   g_rgvertTriStrip2);
    FILL_BUFFER(pData->m_pVBTriFan1,     g_rgvertTriFan1);
    FILL_BUFFER(pData->m_pVBTriFan2,     g_rgvertTriFan2);

    // Create the index buffers
    dwUsage = D3DMUSAGE_WRITEONLY | D3DMUSAGE_DYNAMIC;
    CHK_CLEANUP(pDev->CreateIndexBuffer(sizeof(g_rgwTriList),    dwUsage, D3DMFMT_INDEX16, IndexBufPool, &pData->m_pIBTriList));
    CHK_CLEANUP(pDev->CreateIndexBuffer(sizeof(g_rgwTriStrip1),  dwUsage, D3DMFMT_INDEX16, IndexBufPool, &pData->m_pIBTriStrip1));
    CHK_CLEANUP(pDev->CreateIndexBuffer(sizeof(g_rgwTriStrip2),  dwUsage, D3DMFMT_INDEX16, IndexBufPool, &pData->m_pIBTriStrip2));
    CHK_CLEANUP(pDev->CreateIndexBuffer(sizeof(g_rgwTriFan1),    dwUsage, D3DMFMT_INDEX16, IndexBufPool, &pData->m_pIBTriFan1));
    CHK_CLEANUP(pDev->CreateIndexBuffer(sizeof(g_rgwTriFan2),    dwUsage, D3DMFMT_INDEX16, IndexBufPool, &pData->m_pIBTriFan2));

    // Fill the index buffers
    FILL_BUFFER(pData->m_pIBTriList,     g_rgwTriList);
    FILL_BUFFER(pData->m_pIBTriStrip1,   g_rgwTriStrip1);
    FILL_BUFFER(pData->m_pIBTriStrip2,   g_rgwTriStrip2);
    FILL_BUFFER(pData->m_pIBTriFan1,     g_rgwTriFan1);
    FILL_BUFFER(pData->m_pIBTriFan2,     g_rgwTriFan2);

    #undef FILL_BUFFER

cleanup:
    if (FAILED(hr))
    {
        pData->Clear();
    }
    
    return hr;
}

/////////////////////////////////////////////////////////////////////////////////////
BOOL InitializeStressThread(
                        /*[in]*/ HANDLE hThread, 
                        /*[in]*/ DWORD dwThreadId, 
                        /*[in,out]*/ LPVOID pv, /*unused*/
                        /*[in]*/ int nThreadIndex /*0-based thread index*/)
{
    // TODO:  Do your per thread initialization here

    LogVerbose(_T("Init for thread %d"), nThreadIndex);
    srand(GetTickCount() * (nThreadIndex + 1));

    g_rgThreadData[nThreadIndex].Init();

    if (!CreateAppWindow(g_hInst, &g_rgThreadData[nThreadIndex].m_hwnd))
    {
        LogWarn1(_T("Unable to initialize application, GetLastError: %d."),GetLastError());
        return FALSE;
    }


	//Nonzero indicates that the window was brought to the foreground.
	//Zero indicates that the window was not brought to the foreground. 
    if (!(SetForegroundWindow(g_rgThreadData[nThreadIndex].m_hwnd)))
	{
        LogWarn1(_T("Unable to SetForegroundWindow, GetLastError: %d"),GetLastError());
		return FALSE;	
	}
    

	//Nonzero indicates that the window was previously visible.
	//Zero indicates that the window was previously hidden. 
	ShowWindow(g_rgThreadData[nThreadIndex].m_hwnd, SW_SHOWNORMAL);
    

	//Nonzero indicates success. Zero indicates failure.
	if (!(InvalidateRect(g_rgThreadData[nThreadIndex].m_hwnd, NULL, TRUE)))
	{
        LogWarn1(_T("Unable to InvalidateRect, GetLastError: %d."),GetLastError());
		return FALSE;	
	}

	//Nonzero indicates success. Zero indicates failure.
    if (!(UpdateWindow(g_rgThreadData[nThreadIndex].m_hwnd)))
	{
        LogWarn1(_T("Unable to UpdateWindow, GetLastError: %d."),GetLastError());
		return FALSE;	
	}


	//D3DM_OK indicates success
    if (D3DM_OK != InitDirect3D(&g_rgThreadData[nThreadIndex]))
	{
        LogFail(_T("Unable to InitDirect3D"));
		return FALSE;	
	}

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////
HRESULT DrawCube(THREADDATA *pData, kRenderCall nRenderCall, bool bDrawIndexed)
{
    HRESULT hr = S_OK;

    // Alias the device for simple coding
    IDirect3DMobileDevice* &pDev = pData->m_pD3DDev;
    
    // Draw the cube using the chosen combination of parameters
    switch (nRenderCall)
    {
    case kTriList:
        // Full cube from one triangle list
        if (bDrawIndexed)
        {
            CHK(pDev->SetStreamSource(0, pData->m_pVBIndexedCube, 0));
            CHK(pDev->SetIndices(pData->m_pIBTriList));
            CHK(pDev->DrawIndexedPrimitive(D3DMPT_TRIANGLELIST, 0, 0, countof(g_rgvertIndexedCube), 0, countof(g_rgwTriList)/3));
        }
        else
        {
            // Use the vertex buffer that we created
            CHK(pDev->SetStreamSource(0, pData->m_pVBTriList, 0));
            CHK(pDev->DrawPrimitive(D3DMPT_TRIANGLELIST, 0, countof(g_rgvertTriList)/3));
        }
        break;

    case kTriStrip:
        // Full cube from two triangle strips
        if (bDrawIndexed)
        {
            CHK(pDev->SetStreamSource(0, pData->m_pVBIndexedCube, 0));
            CHK(pDev->SetIndices(pData->m_pIBTriStrip1));
            CHK(pDev->DrawIndexedPrimitive(D3DMPT_TRIANGLESTRIP, 0, 0, countof(g_rgvertIndexedCube), 0, countof(g_rgwTriStrip1)-2));
            CHK(pDev->SetIndices(pData->m_pIBTriStrip2));
            CHK(pDev->DrawIndexedPrimitive(D3DMPT_TRIANGLESTRIP, 0, 0, countof(g_rgvertIndexedCube), 0, countof(g_rgwTriStrip2)-2));
        }
        else
        {
            // Use the vertex buffer that we created
            CHK(pDev->SetStreamSource(0, pData->m_pVBTriStrip1, 0));
            CHK(pDev->DrawPrimitive(D3DMPT_TRIANGLESTRIP, 0, countof(g_rgvertTriStrip1)-2));
            CHK(pDev->SetStreamSource(0, pData->m_pVBTriStrip2, 0));
            CHK(pDev->DrawPrimitive(D3DMPT_TRIANGLESTRIP, 0, countof(g_rgvertTriStrip2)-2));
        }
        break;

    case kTriFan:
        // Full cube from two triangle fans
        if (bDrawIndexed)
        {
            CHK(pDev->SetStreamSource(0, pData->m_pVBIndexedCube, 0));
            CHK(pDev->SetIndices(pData->m_pIBTriFan1));
            CHK(pDev->DrawIndexedPrimitive(D3DMPT_TRIANGLEFAN, 0, 0, countof(g_rgvertIndexedCube), 0, countof(g_rgwTriFan1)-2));
            CHK(pDev->SetIndices(pData->m_pIBTriFan2));
            CHK(pDev->DrawIndexedPrimitive(D3DMPT_TRIANGLEFAN, 0, 0, countof(g_rgvertIndexedCube), 0, countof(g_rgwTriFan2)-2));
        }
        else
        {
            // Use the vertex buffer that we created
            CHK(pDev->SetStreamSource(0, pData->m_pVBTriFan1, 0));
            CHK(pDev->DrawPrimitive(D3DMPT_TRIANGLEFAN, 0, countof(g_rgvertTriFan1)-2));
            CHK(pDev->SetStreamSource(0, pData->m_pVBTriFan2, 0));
            CHK(pDev->DrawPrimitive(D3DMPT_TRIANGLEFAN, 0, countof(g_rgvertTriFan2)-2));
        }
        break;
    
    case kLineStrip:
        // Wireframe version of cube from line strip
        if (bDrawIndexed)
        {
            CHK(pDev->SetStreamSource(0, pData->m_pVBIndexedCube, 0));
            CHK(pDev->SetIndices(pData->m_pIBTriList));
            CHK(pDev->DrawIndexedPrimitive(D3DMPT_LINESTRIP, 0, 0, countof(g_rgvertIndexedCube), 0, countof(g_rgwTriList)-1));
        }
        else
        {
            // Use the vertex buffer that we created
            CHK(pDev->SetStreamSource(0, pData->m_pVBTriList, 0));
            CHK(pDev->DrawPrimitive(D3DMPT_LINESTRIP, 0, countof(g_rgvertTriList)-1));
        }
        break;
    }
    
    return hr;
}

/////////////////////////////////////////////////////////////////////////////////////
UINT DoStressIterationEX (
                        /*[in]*/ HANDLE hThread, 
                        /*[in]*/ DWORD dwThreadId, 
                        /*[in,out]*/ LPVOID pv, /*unused*/
                        /*[in]*/ int nThreadIndex /*0-based thread index*/)
{
    // TODO:  Do your actual testing here.
    //
    //        You can do whatever you want here, including calling other functions.
    //        Just keep in mind that this is supposed to represent one iteration
    //        of a stress test (with a single result), so try to do one discrete 
    //        operation each time this function is called. 

    UINT uRet = CESTRESS_PASS;
    HRESULT hr = S_OK;
    
    // Alias the device for simple coding
    THREADDATA *pData = &g_rgThreadData[nThreadIndex];
    IDirect3DMobileDevice* &pDev = pData->m_pD3DDev;

    // Pick a random frame count
    int nFrames = 100 + (rand() % 500);

    // Get some random rotation speeds;
    float fTheta1 = TWOPI/static_cast<float>(200 + (rand() % 1000)),
          fTheta2 = TWOPI/static_cast<float>(200 + (rand() % 1000));

    // Pick a type of rendering
    kRenderCall RenderType;
    bool bDrawIndexed,
         bDrawUserMem,
         bOneCube;

    // See if we're rendering one or many cubes
    bOneCube = !!(rand() & 0x01);
    
    // If we're render only one cube, choose the type of rendering
    if (bOneCube)
    {
        RenderType = static_cast<kRenderCall>(rand() % kRenderCallMax);
        bDrawIndexed = !!(rand() & 0x01);
        bDrawUserMem = !!(rand() & 0x01);
    }    

    // Render all the frames
    for (int i = 0; i < nFrames; ++i)
    {
        // Update the location and rotation matricies
        D3DMMATRIX matRot1= { _M((float)cos(fTheta1)), _M(0.0f), _M((float)(-sin(fTheta1))), _M(0.0f),
                              _M(0.0f               ), _M(1.0f), _M(0.0f                  ), _M(0.0f),
                              _M((float)sin(fTheta1)), _M(0.0f), _M((float)cos(fTheta1)   ), _M(0.0f),
                              _M(0.0f               ), _M(0.0f), _M(0.0f                  ), _M(1.0f)};

        D3DMMATRIX matRot2 ={_M((float) cos(fTheta2)), _M((float)sin(fTheta2)),   _M(0.0f), _M(0.0f),
                             _M((float)-sin(fTheta2)), _M((float)cos(fTheta2)),   _M(0.0f), _M(0.0f),
                             _M( 0.0f               ), _M(0.0f               ),   _M(1.0f), _M(0.0f),
                             _M( 0.0f               ), _M(0.0f               ),   _M(0.0f), _M(1.0f)};
        
        D3DMMATRIX matRot;

        D3DMatrixMultiply(&matRot, &matRot1, &matRot2);
        D3DMatrixMultiply(&pData->m_matRot, &pData->m_matRot, &matRot);

        // Clear the buffer
        CHK(pDev->Clear(0, NULL, D3DMCLEAR_TARGET/*|D3DMCLEAR_ZBUFFER*/, 0x00000000, 1.0f, 0L));

        // Render the scene
        hr = pDev->BeginScene();
        if (FAILED(hr))
        {
            LogFail(_T("BeginScene failed"));
            break;
        }

        if (bOneCube)
        {
            // Rotate, then translate the cube
            D3DMMATRIX matWorld;
			D3DMatrixMultiply(&matWorld, &pData->m_matRot, &pData->m_matPos);

            CHK(pDev->SetTransform(D3DMTS_WORLD, &matWorld, D3DMFMT_D3DMVALUE_FLOAT));
            CHK(DrawCube(pData, RenderType, bDrawIndexed));
        }
        else
        {
            D3DMMATRIX matWork, matShift;
            int nRenderCall;

            for (nRenderCall = kTriStrip; nRenderCall < kRenderCallMax; nRenderCall++)
            {
                for (DWORD dw = 0; dw < 4; dw++)
                {
                    RenderType = static_cast<kRenderCall>(nRenderCall);
                    bDrawIndexed = !!(dw & 0x01),
                    bDrawUserMem = !!(dw & 0x02);

                    // Scale, rotate, then translate the cube
                    D3DMatrixScaling(&matWork, 0.3f, 0.3f, 0.3f);
                    D3DMatrixTranslation(&matShift, -1.5f+dw, -2.5f+(int)nRenderCall, 0.0f);
					
                    D3DMMATRIX matConcatSP, matConcatRSP;

                    D3DMatrixMultiply(&matConcatSP, &matShift, &pData->m_matPos);
                    D3DMatrixMultiply(&matConcatRSP, &pData->m_matRot, &matConcatSP);
                    D3DMatrixMultiply(&matWork, &matWork, &matConcatRSP );

                    CHK(pDev->SetTransform(D3DMTS_WORLD, &matWork, D3DMFMT_D3DMVALUE_FLOAT));

                    CHK(DrawCube(pData, RenderType, bDrawIndexed));
                }
            }
        }

        CHK(pDev->EndScene());

        CHK(pDev->Present(NULL, NULL, NULL, NULL));
    }

    if (FAILED(hr))
        uRet = CESTRESS_FAIL;
    
    // You must return one of these values:
    //return CESTRESS_PASS;
    //return CESTRESS_FAIL;
    //return CESTRESS_WARN1;
    //return CESTRESS_WARN2;
    //return CESTRESS_ABORT;  // Use carefully.  This will cause your module to be terminated immediately.
    return uRet;
}

/////////////////////////////////////////////////////////////////////////////////////
UINT DoStressIteration (
                        /*[in]*/ HANDLE hThread, 
                        /*[in]*/ DWORD dwThreadId, 
                        /*[in,out]*/ LPVOID pv /*unused*/)
{
    // Detect if this is the first DoStressIteration call on this thread
    int nThreadIndex = reinterpret_cast<int>(TlsGetValue(g_dwTlsIndex));
    if (0 == nThreadIndex)
    {
        nThreadIndex = ++g_nThreadCount;
        if (!InitializeStressThread(hThread, dwThreadId, pv, nThreadIndex - 1))
        {
            g_nThreadCount--;
            return CESTRESS_ABORT;
        }
        TlsSetValue(g_dwTlsIndex, reinterpret_cast<void*>(nThreadIndex));
    }

    return DoStressIterationEX(hThread, dwThreadId, pv, nThreadIndex - 1);
}

/////////////////////////////////////////////////////////////////////////////////////
void CleanupStressThread(/*[in]*/ int nThreadIndex)
{
    LogVerbose(_T("Cleanup for thread %d (iterated %d times)"), nThreadIndex, g_rgThreadData[nThreadIndex].m_nIterationCount);

    g_rgThreadData[nThreadIndex].Clear();

}

/////////////////////////////////////////////////////////////////////////////////////
DWORD TerminateStressModule (void)
{
    // TODO:  Do test-specific clean-up here.

    int i;
    if (0xFFFFFFFF != g_dwTlsIndex)
    {
        for (i = 0; i < g_nThreadCount; ++i)
        {
            CleanupStressThread(i);
        }
        TlsFree(g_dwTlsIndex);
        g_dwTlsIndex = 0xFFFFFFFF;
    }

    return ((DWORD) -1);
}

///////////////////////////////////////////////////////////
BOOL WINAPI DllMain(
                    HANDLE hInstance, 
                    ULONG dwReason, 
                    LPVOID lpReserved
                    )
{
    g_hInst = static_cast<HINSTANCE>(hInstance);
    return TRUE;
}
