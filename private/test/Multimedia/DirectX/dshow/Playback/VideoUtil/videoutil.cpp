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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
////////////////////////////////////////////////////////////////////////////////

#ifdef UNDER_CE
#include <dshow.h>
#include <atlbase.h>
#include <ddraw.h>
#include <tux.h>
#include "VideoUtil.h"
#include "logging.h"


extern SPS_SHELL_INFO   *g_pShellInfo;
static TCHAR g_szClassName[] = TEXT("Test Application");
static CComPtr<IVMRWindowlessControl> s_pVMRWindowlessControl = NULL;

IDirectDrawSurface *
CreateDDrawSurface( int width, int height, BOOL bAlpha )
{
    HRESULT hr = S_OK;
    IDirectDraw *pDirectDraw = NULL;
    IDirectDrawSurface* pDDS = NULL;
    HINSTANCE hDDrawdll = NULL;
    typedef HRESULT (*LPDDCREATE)( GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter );
    LPDDCREATE lpDirectDrawCreate;

    hDDrawdll = LoadLibrary( TEXT("ddraw.dll") );

    if ( hDDrawdll != NULL )
    {
#ifdef UNDER_CE    
        lpDirectDrawCreate = (LPDDCREATE)GetProcAddress( hDDrawdll, TEXT("DirectDrawCreate") );
#else
        lpDirectDrawCreate = (LPDDCREATE)GetProcAddress( hDDrawdll, "DirectDrawCreate" );
#endif
        if ( !lpDirectDrawCreate )
        {   
        LOG( TEXT("%S: ERROR %d@%S - Failed to get DirectDrawCreate function pointer.."), 
            __FUNCTION__, __LINE__, __FILE__);
        FreeLibrary( hDDrawdll );       
        return pDDS;
    }

        hr = lpDirectDrawCreate( NULL, &pDirectDraw, NULL );
        if ( SUCCEEDED(hr) && pDirectDraw )
        {
            DDSURFACEDESC ddsd;

            memset (&ddsd, 0, sizeof(ddsd));
            ddsd.dwSize = sizeof(ddsd);
            ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
            ddsd.dwWidth = width;
            ddsd.dwHeight = height;
            ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);

            if (bAlpha)
            {
                ddsd.ddpfPixelFormat.dwFourCC = BI_RGB;
                ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS | DDPF_ALPHAPREMULT;
                ddsd.ddpfPixelFormat.dwRGBBitCount = 32;
                ddsd.ddpfPixelFormat.dwRGBAlphaBitMask = 0xff000000;
            }
            else
            {
                ddsd.ddpfPixelFormat.dwFourCC = BI_RGB;
                ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
                ddsd.ddpfPixelFormat.dwRGBBitCount = 32;
            }
            ddsd.ddpfPixelFormat.dwRBitMask = 0x00ff0000;
            ddsd.ddpfPixelFormat.dwGBitMask = 0x0000ff00;
            ddsd.ddpfPixelFormat.dwBBitMask = 0x000000ff;

            hr = pDirectDraw->SetCooperativeLevel(NULL, DDSCL_NORMAL);
            if (SUCCEEDED(hr))
            {
                pDirectDraw->CreateSurface(&ddsd, &pDDS, NULL);
            }
            pDirectDraw->Release();
        }
    }

    if ( hDDrawdll )
        FreeLibrary( hDDrawdll );  

    return pDDS;
}

void 
PumpMessages( DWORD dwDuration )
{
    MSG     msg;
    DWORD   dwStart = GetTickCount();

    LOG( TEXT("Pumping Msgs, now = %d"), dwStart);
    while (GetTickCount() - dwStart < dwDuration)
    {
        BOOL bMsg = FALSE;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            bMsg = TRUE;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(100);
    }
}

static LRESULT APIENTRY 
TestWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WCHAR *pMsgStr = NULL;

    switch (msg)
    {
        case WM_MOVE:
            pMsgStr = L"WM_MOVE";
            break;
        case WM_SIZE:
            pMsgStr = L"WM_SIZE";
            break;
        case WM_ACTIVATE:
            pMsgStr = L"WM_ACTIVATE";
            break;
        case WM_SETFOCUS:
            pMsgStr = L"WM_SETFOCUS";
            break;
        case WM_KILLFOCUS:
            pMsgStr = L"WM_KILLFOCUS";
            break;
        case WM_SETTEXT:
            pMsgStr = L"WM_SETTEXT";
            break;
        case WM_PAINT:
            pMsgStr = L"WM_PAINT";
            break;
        case WM_ERASEBKGND:
            pMsgStr = L"WM_ERASEBKGND";
            break;
        case WM_SYSCOLORCHANGE:
            pMsgStr = L"WM_SYSCOLORCHANGE";
            break;
        case WM_SHOWWINDOW:
            pMsgStr = L"WM_SHOWWINDOW";
            break;
        case WM_SETTINGCHANGE:
            pMsgStr = L"WM_SETTINGCHANGE";
            break;
    }
    LOG( TEXT("WndProc: msg = %s (0x%x)\n"), pMsgStr ? pMsgStr : TEXT(""), msg);

// rotation isn't supported on the desktop, so this code isn't applicable.
 switch(msg)
    {
        case WM_MOVE:
            if (s_pVMRWindowlessControl)
            {
                s_pVMRWindowlessControl->RepaintVideo(hwnd, NULL);
            }
            break;

        case WM_SETTINGCHANGE:
            if(wParam == SETTINGCHANGE_RESET)
            {
                if (s_pVMRWindowlessControl)
                {
                    s_pVMRWindowlessControl->DisplayModeChanged();
                }
            }
            break;

        case WM_PAINT:
            if (s_pVMRWindowlessControl)
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                s_pVMRWindowlessControl->RepaintVideo(hwnd, hdc);
                EndPaint(hwnd,&ps);
            }
            break;

        case WM_SIZE:
            if (s_pVMRWindowlessControl)
            {
                // TODO: The below should work, but it is returning the full screen... (?)
                // RECT TargetRect = {0,0,LOWORD(lParam),HIWORD(lParam)};
                //s_pVMRWindowlessControl->SetVideoPosition(NULL, &TargetRect);
                InvalidateRect(hwnd,NULL,TRUE);
            }
            break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

HWND 
OpenAppWindow( LONG lLeft, 
               LONG lTop, 
               LONG lVideoWidth, 
               LONG lVideoHeight, 
               RECT *pClientRect,
               CComPtr<IVMRWindowlessControl> pVMRWindowlessControl )
{
     HWND hwndApp = NULL;
    WNDCLASS wc;
    RECT Rect;
    BOOL bResult = FALSE;
    
    memset(&wc, 0, sizeof(WNDCLASS));
    wc.lpfnWndProc = (WNDPROC) TestWndProc;
    wc.hInstance = g_pShellInfo->hLib;
    wc.hbrBackground = NULL;        // do not set a background brush (windowless mode)
    wc.lpszClassName = g_szClassName;
    s_pVMRWindowlessControl = pVMRWindowlessControl;


    // RegisterClass can fail if the previous test bombed out on cleanup (due to an exception or such).
    // so, if we allow ourselves to continue even if the registering failed, 
    // we might be able to run a test which would otherwise be blocked.
    RegisterClass(&wc);

    Rect.left = 0;
    Rect.top = 0;
    Rect.right = Rect.left + lVideoWidth;
    Rect.bottom = Rect.top + lVideoHeight;

    bResult = AdjustWindowRectEx(&Rect, WS_CAPTION | WS_CLIPCHILDREN | WS_VISIBLE, FALSE, WS_EX_WINDOWEDGE);
    if(!bResult)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to AdjustWindowRectEx. (0x%x)"), 
                __FUNCTION__, __LINE__, __FILE__, HRESULT_FROM_WIN32(GetLastError()));


        return hwndApp;
    }
    
    hwndApp = CreateWindowEx(WS_EX_WINDOWEDGE,
                             g_szClassName, 
                             TEXT("Test Window"),
                             WS_CAPTION | WS_CLIPCHILDREN | WS_VISIBLE,
                             lLeft, 
                             lTop,
                             Rect.right - Rect.left, 
                             Rect.bottom - Rect.top,
                             NULL, 
                             NULL, 
                             g_pShellInfo->hLib, 
                             NULL);

    SetFocus(hwndApp);
    SetForegroundWindow(hwndApp);
    ShowWindow(hwndApp, SW_SHOW);
    UpdateWindow(hwndApp);
    PumpMessages(100);

    if (!hwndApp)
    {
        LOG( TEXT("Failed to create window!") );
    }

    if (pClientRect)
    {
        GetClientRect(hwndApp, pClientRect);
    }

    return hwndApp;
}


void CloseAppWindow(HWND hwnd)
{
    DestroyWindow(hwnd);
    UnregisterClass(g_szClassName, g_pShellInfo->hLib);
    s_pVMRWindowlessControl = NULL;
}


HRESULT SetWindowPosition(IVMRWindowlessControl *pVMRControl, HWND hwnd)
{
    HRESULT hr;
    LONG width, height;
    RECT rcSrc, rcDest;

    hr = pVMRControl->SetVideoClippingWindow(hwnd);
    if ( FAILED(hr) )
        goto cleanup;

    hr = pVMRControl->GetNativeVideoSize(&width, &height, NULL, NULL);
    if ( FAILED(hr) )
        goto cleanup;

    // Get the src area
    SetRect(&rcSrc, 0, 0, width, height);

    // Get the destination area
    GetClientRect(hwnd, &rcDest);
    SetRect(&rcDest, 0, 0, rcDest.right, rcDest.bottom);

    hr = pVMRControl->SetVideoPosition(&rcSrc, &rcDest);
    if ( FAILED(hr) )
        goto cleanup;

    LOG( TEXT("Native video size (%d x %d), window client = (%d, %d)"), rcSrc.right, rcSrc.bottom, rcDest.right, rcDest.bottom);

cleanup:
    return hr;
}


void 
InitAlphaParams( VMRALPHABITMAP *pAlphaBitmap, 
                 DWORD dwFlags, 
                 HDC hdc, 
                 IDirectDrawSurface *pDDS, 
                 RECT *prcSrc, 
                 NORMALIZEDRECT *prcDest, 
                 float fAlpha, 
                 COLORREF clrKey )
{
    RECT rcSrc;
    NORMALIZEDRECT rcDest;

    if ( !pAlphaBitmap ) return;

    memset( pAlphaBitmap, 0, sizeof(*pAlphaBitmap) );
    pAlphaBitmap->dwFlags = dwFlags;
    pAlphaBitmap->hdc = hdc;
    pAlphaBitmap->pDDS = pDDS;

    if ( prcSrc )
    {
        rcSrc = *prcSrc;
    }
    else
    {
        rcSrc.left = 0;
        rcSrc.top = 0;
        rcSrc.right = ALPHA_BITMAP_SIZE;
        rcSrc.bottom = ALPHA_BITMAP_SIZE;

        if ( pDDS )
        {
            DDSURFACEDESC ddsd;
            memset( &ddsd, 0, sizeof(ddsd) );
            ddsd.dwSize = sizeof(DDSURFACEDESC);
            pDDS->GetSurfaceDesc( &ddsd );
            rcSrc.right = ddsd.dwWidth;
            rcSrc.bottom = ddsd.dwHeight;
        }
    }

    if ( prcDest )
    {
        rcDest = *prcDest;
    }
    else
    {
        rcDest.left = 0.0f;
        rcDest.top = 0.0f;
        rcDest.right = 1.0f;
        rcDest.bottom = 1.0f;
    }

    pAlphaBitmap->rSrc = rcSrc;
    pAlphaBitmap->rDest = rcDest;
    pAlphaBitmap->fAlpha = fAlpha;
    pAlphaBitmap->clrSrcKey = clrKey;
}

HRESULT 
RotateUI (AM_ROTATION_ANGLE angle, BOOL bTest)
{
    HRESULT hr = S_OK;
    DEVMODE devMode;
    memset(&devMode,0x00,sizeof(devMode));
    devMode.dmSize=sizeof(devMode);
    
    switch (angle)
    {
        case AM_ROTATION_ANGLE_0:
            devMode.dmDisplayOrientation = DMDO_0;
            LOG(TEXT("Rotating UI by 0 degrees.\n"));
            break;

        case AM_ROTATION_ANGLE_90:
            devMode.dmDisplayOrientation = DMDO_90;
            LOG(TEXT("Rotating UI by 90 degrees.\n"));
            break;

        case AM_ROTATION_ANGLE_180:
            devMode.dmDisplayOrientation = DMDO_180;
            LOG(TEXT("Rotating UI by 180 degrees.\n"));
            break;

        case AM_ROTATION_ANGLE_270:
            devMode.dmDisplayOrientation = DMDO_270;
            LOG(TEXT("Rotating UI by 270 degrees.\n"));
            break;

        default:
            LOG(TEXT("Unknown rotation angle %d!!!\n"), angle);
            hr = E_FAIL;
    }

    if (SUCCEEDED(hr))
    {
        if (bTest)
        {
            devMode.dmFields = DM_DISPLAYQUERYORIENTATION;
            hr = ChangeDisplaySettingsEx (NULL, &devMode, NULL, CDS_TEST, NULL);
            if (hr == DISP_CHANGE_SUCCESSFUL)
            {
                if (devMode.dmDisplayOrientation == DMDO_0)
                {
                    hr = E_FAIL;
                }
            }
        }
        else
        {
            devMode.dmFields = DM_DISPLAYORIENTATION;
            hr = ChangeDisplaySettingsEx (NULL, &devMode, NULL, CDS_RESET, NULL);
        }
        if (hr == DISP_CHANGE_BADMODE)
        {
            LOG(TEXT("The specified graphics mode is not supported\n"));
        }
        else if (hr != DISP_CHANGE_SUCCESSFUL)
        {
            LOG(TEXT("ERROR: Failed rotating UI, hr = 0x%x\n"), hr);
        }
    }

    return hr;
}

HRESULT 
GetRegKey (const WCHAR *pPath, const WCHAR *pKey, DWORD *pdwVal)
{
    HKEY hKey;
    LONG lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, pPath, 0, KEY_QUERY_VALUE, &hKey);
    if (ERROR_SUCCESS == lRet)
    {
        DWORD   dwType, dwLen;

        dwLen = sizeof(DWORD);
        lRet = RegQueryValueEx(hKey, pKey, 0L, &dwType, (LPBYTE)pdwVal, &dwLen);
        RegCloseKey(hKey);
    }
    return HRESULT_FROM_WIN32(lRet);
}


HRESULT 
SetRegKey (const WCHAR *pPath, const WCHAR *pKey, DWORD dwVal)
{
    HKEY hKey;
    LONG lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, pPath, 0, KEY_WRITE, &hKey);
    if (ERROR_SUCCESS == lRet)
    {
        lRet = RegSetValueEx(hKey, pKey, 0L, REG_DWORD, (LPBYTE)&dwVal, sizeof(DWORD));
        RegCloseKey(hKey);
    }
    return HRESULT_FROM_WIN32(lRet);
}

typedef HRESULT (*PDRAWCREATE)(IID *,LPDIRECTDRAW *,LPUNKNOWN);

HRESULT 
GetDirectDraw(IDirectDraw **ppDirectDraw, HMODULE *phDirectDrawDLL)
{
    IDirectDraw *pDirectDraw = NULL;
    PDRAWCREATE pDrawCreate;
    HRESULT hr = NOERROR;

    // Make sure the library is available
    HMODULE hDirectDraw = LoadLibrary(TEXT("DDRAW.DLL"));
    if (hDirectDraw == NULL)
        hr = E_NOINTERFACE;
    
    CHECKHR(hr,TEXT("Load DDRAW.DLL ..." ));

    // Get the DLL address for the creator function
    pDrawCreate = (PDRAWCREATE)GetProcAddress(hDirectDraw, TEXT("DirectDrawCreate"));

    if (pDrawCreate == NULL)
        hr = E_NOINTERFACE;
    
    CHECKHR(hr,TEXT("Get DLL address ..." ));

    // we just use DirectDrawCreate
    if ((pDrawCreate)(NULL, &pDirectDraw, NULL) != DD_OK)
        hr = E_FAIL;
    
    CHECKHR(hr,TEXT("call DDraw Create ..." ));

    *ppDirectDraw = pDirectDraw;
    *phDirectDrawDLL = hDirectDraw;

    cleanup:

    if (FAILED(hr))
    {
        if (pDirectDraw)
            pDirectDraw->Release();

        if (hDirectDraw)
            FreeLibrary(hDirectDraw);
    }
    return hr;
}


void 
ReleaseDirectDraw(IDirectDraw *pDirectDraw, HMODULE hDirectDrawDLL)
{
    if (pDirectDraw)
        pDirectDraw->Release();

    if (hDirectDrawDLL)
        FreeLibrary(hDirectDrawDLL);
}


BOOL 
IsYV12HardwareSupported()
{
    //
    // The WMV codec just supports YV12 & RGB formats for rotation/scaling. If our display driver
    // doesn't support this YUV format, then we disable YUV formats so that we have a chance of getting
    // hw accelerated surfaces (RGB overlays).
    //
    IDirectDraw *pDirectDraw;
    HMODULE hDirectDrawDLL;
    BOOL bYV12Supported = FALSE;
    HRESULT hr;

    hr = GetDirectDraw(&pDirectDraw, &hDirectDrawDLL);
    if (SUCCEEDED(hr))
    {
        DDCAPS ddCaps;

        pDirectDraw->GetCaps(&ddCaps, NULL);
        if (ddCaps.dwOverlayCaps != 0)
        {
            DWORD Codes[32];
            DWORD NumCodes = 32;

            hr = pDirectDraw->GetFourCCCodes(&NumCodes, Codes);
            if (SUCCEEDED(hr))
            {
                for (DWORD i = 0; i < NumCodes; i++)
                {
                    if (Codes[i] == FOURCC_YV12)
                        bYV12Supported = TRUE;                            
                }
            }
        }
        ReleaseDirectDraw(pDirectDraw, hDirectDrawDLL);
    }
    return bYV12Supported;
}

BOOL 
IsOverlayHardwareSupported()
{
    IDirectDraw *pDirectDraw;
    HMODULE hDirectDrawDLL;
    BOOL bOverlaySupported = FALSE;
    HRESULT hr;

    hr = GetDirectDraw(&pDirectDraw, &hDirectDrawDLL);
    if (SUCCEEDED(hr))
    {
        DDCAPS ddCaps;

        pDirectDraw->GetCaps(&ddCaps, NULL);
        if (ddCaps.dwOverlayCaps != 0)
        {
           bOverlaySupported = TRUE;                            
        }
        
        ReleaseDirectDraw(pDirectDraw, hDirectDrawDLL);
    }
    return bOverlaySupported;
}

HRESULT 
ConfigMaxBackBuffers(DWORD dwNumOfBuffers, DWORD* pdwOrigNumOfBuffers)
{
    HRESULT hr = S_OK;

    hr = GetRegKey (L"Software\\Microsoft\\DirectX\\DirectShow\\VMR", L"MaxBackBuffers", pdwOrigNumOfBuffers);
    if (SUCCEEDED(hr))
        hr = SetRegKey (L"Software\\Microsoft\\DirectX\\DirectShow\\VMR", L"MaxBackBuffers", dwNumOfBuffers);

    return hr;
}

HRESULT 
EnableUpstreamRotate(bool bUpstreamPreferred, DWORD* dwOrigUpstreamRotationPreferred)
{
    HRESULT hr = S_OK;

    hr = GetRegKey(L"Software\\Microsoft\\DirectX\\DirectShow\\VMR", L"UpstreamRotationPreferred", dwOrigUpstreamRotationPreferred);

    if (SUCCEEDED(hr))
        hr = SetRegKey(L"Software\\Microsoft\\DirectX\\DirectShow\\VMR", L"UpstreamRotationPreferred", bUpstreamPreferred);
    
    return hr;
}


HRESULT 
RestoreOrigUpstreamRotate(DWORD dwOrigUpstreamRotationPreferred)
{
    HRESULT hr = S_OK;

    hr = SetRegKey(L"Software\\Microsoft\\DirectX\\DirectShow\\VMR", L"UpstreamRotationPreferred", dwOrigUpstreamRotationPreferred);

    return hr;
}

HRESULT 
EnableUpstreamScale(bool bUpstreamPreferred, DWORD* dwOrigUpstreamScalePreferred)
{
    HRESULT hr = S_OK;

    hr = GetRegKey(L"Software\\Microsoft\\DirectX\\DirectShow\\VMR", L"UpstreamScalingPreferred", dwOrigUpstreamScalePreferred);

    if (SUCCEEDED(hr))
        hr = SetRegKey(L"Software\\Microsoft\\DirectX\\DirectShow\\VMR", L"UpstreamScalingPreferred", bUpstreamPreferred);

    return hr;
}


HRESULT 
RestoreOrigUpstreamScale(DWORD dwOrigUpstreamScalePreferred)
{
    HRESULT hr = S_OK;

    hr = SetRegKey(L"Software\\Microsoft\\DirectX\\DirectShow\\VMR", L"UpstreamScalingPreferred", dwOrigUpstreamScalePreferred);

    return hr;
}

HRESULT 
RestoreMaxBackBuffers(DWORD dwNumOfBuffers)
{
    HRESULT hr = S_OK;

    hr = SetRegKey (L"Software\\Microsoft\\DirectX\\DirectShow\\VMR", L"MaxBackBuffers", dwNumOfBuffers);

    return hr;
}

HRESULT 
DisableYUVSurfaces(bool* bDisableYUVSurfaces)
{
    HRESULT hr = S_OK;

    if (!IsYV12HardwareSupported())
    {
        // if YV12 is not amongst the YUV formats this platform supports, we need to disable YUV so
        // that we have a chance with RGB overlays
        DWORD SurfaceTypes = 0xff & ~AMDDS_YUV;
        hr = SetRegKey (L"Software\\Microsoft\\DirectX\\DirectShow\\Video Renderer", L"SurfaceTypes", SurfaceTypes);
        *bDisableYUVSurfaces = TRUE;
    }

    return hr;
}

HRESULT 
EnableYUVSurfaces(void)
{
    HRESULT hr = S_OK;

    hr = SetRegKey (L"Software\\Microsoft\\DirectX\\DirectShow\\Video Renderer", L"SurfaceTypes", 0xff);
        
    return hr;
}

HRESULT 
DisableWMVDecoderFrameDrop(void)
{
    HRESULT hr = S_OK;

    hr = SetRegKey (L"Software\\Microsoft\\DirectX\\DirectShow\\WMVDecoder", L"DoNotDropFrames", 1);

    return hr;
}

HRESULT 
EnableWMVDecoderFrameDrop(void)
{
    HRESULT hr = S_OK;

    hr = SetRegKey (L"Software\\Microsoft\\DirectX\\DirectShow\\WMVDecoder", L"DoNotDropFrames", 0);
        
    return hr;
}

HRESULT 
SetupGDIMode(void)
{
    HRESULT hr = S_OK;

    // disable all DDraw surfaces
    hr = SetRegKey (L"Software\\Microsoft\\DirectX\\DirectShow\\Video Renderer", L"SurfaceTypes", 0);

    return hr;
}


HRESULT 
RestoreRenderMode(void)
{
    HRESULT hr = S_OK;

    // enable all DDraw surfaces
    hr = SetRegKey (L"Software\\Microsoft\\DirectX\\DirectShow\\Video Renderer", L"SurfaceTypes", 0xff);

    return hr;
}

//Set up grapth log file
HRESULT 
SetLogFile(TestGraph* pTestGraph)
{
    HRESULT hr = S_OK;
    IGraphBuilder *pGraphBuilder = NULL;
    HANDLE fileHandle;

    if ( !pTestGraph )
    {
        LOG( TEXT(" NULL TestGraph pointer passed." ) );
        hr = E_FAIL;
        goto cleanup;
    }

    // Since GetGraph() addref for the pointer, we need to release it after using it.
    pGraphBuilder = pTestGraph->GetGraph();
    if ( !pGraphBuilder )
    {
        LOG( TEXT("failed to retrieve graph builder pointer") );
        hr = E_FAIL;
        goto cleanup;
    }

    fileHandle = CreateFile( TEXT("\\release\\graph.log"), GENERIC_WRITE, (DWORD)0, NULL, CREATE_ALWAYS, (DWORD)0,  NULL );

    if ( INVALID_HANDLE_VALUE == fileHandle)
    {
        LOG( TEXT("%S: ERROR %d@%S - CreateFile returned invalid handle."), __FUNCTION__, __LINE__, __FILE__ );
        hr = E_FAIL;
        goto cleanup;
    }
    
    hr = pGraphBuilder->SetLogFile( fileHandle );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetLogFile with a valid handle failed. (0x%08x)"), __FUNCTION__, __LINE__, __FILE__, hr );
        hr = E_FAIL;
        CloseHandle( fileHandle );
        goto cleanup;
    }

cleanup:
    if ( pGraphBuilder )
        pGraphBuilder->Release();

    return hr;
}

//Center video output window
HRESULT 
CenterWindow(TestGraph* pTestGraph, bool bRotate)
{
    HRESULT hr = S_OK;
    long dstWidth = 0;
    long dstHeight = 0;
    long dstLeft = 0;
    long dstTop = 0;
    long x =0, y=0;
    long screenWidth, screenHeight;
    IBasicVideo         *pBasicVideo = NULL;

    hr = pTestGraph->FindInterfaceOnFilter(IID_IBasicVideo, (void**)&pBasicVideo);
    if (FAILED(hr) || (hr == S_FALSE) || !pBasicVideo)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to get IID_IBasicVideo interface. (0x%x)"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        goto cleanup;
    }
    hr = pBasicVideo->GetDestinationPosition(&dstLeft, &dstTop, &dstWidth, &dstHeight);
    CHECKHR(hr,TEXT("Get video destination position ..." ));
        
    screenWidth = GetSystemMetrics (SM_CXSCREEN);
    screenHeight = GetSystemMetrics (SM_CYSCREEN);

    if(bRotate)
    {
        x = (screenWidth - dstHeight) / 2;
        y = (screenHeight - dstWidth) / 2;
    }else{
        x = (screenWidth - dstWidth) / 2;
        y = (screenHeight - dstHeight) / 2;
    }

    x = x < 0 ? 0 : x;
    y = y < 0 ? 0 : y;

    if(bRotate)
        hr = pTestGraph->SetVideoWindowPosition (x, y, dstHeight, dstWidth);
    else
        hr = pTestGraph->SetVideoWindowPosition (x, y, dstWidth, dstHeight);
    
    CHECKHR(hr,TEXT("SetWindowPosition ..." ));

cleanup:
     if(pBasicVideo)
         pBasicVideo->Release();

    return hr;
}

HRESULT 
VMRUpdateWindow (IBaseFilter *pVMR, HWND hWnd, bool bRotate)
{
    HRESULT hr = S_OK;
    long dstWidth = 0;
    long dstHeight = 0;
    long x =0, y=0;
    long screenWidth, screenHeight;
    RECT destRect;
    BOOL bResult = FALSE;
    IVMRWindowlessControl   *pVMRWindowlessControl = NULL;

    hr = pVMR->QueryInterface( IID_IVMRWindowlessControl, (void **)&pVMRWindowlessControl );
    CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

    hr = pVMRWindowlessControl->GetVideoPosition(NULL, &destRect);
    CHECKHR(hr,TEXT("Get video destination position ..." ));
        
    screenWidth = GetSystemMetrics (SM_CXSCREEN);
    screenHeight = GetSystemMetrics (SM_CYSCREEN);

    bResult = AdjustWindowRectEx(&destRect, WS_CAPTION | WS_CLIPCHILDREN | WS_VISIBLE, FALSE, WS_EX_WINDOWEDGE);
    if(!bResult)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        LOG(TEXT("%S: ERROR %d@%S -AdjustWindowRectEx. (0x%x)"), 
                __FUNCTION__, __LINE__, __FILE__, hr);


        goto cleanup;
    }

    //Get adjusted window size
    dstWidth = destRect.right - destRect.left;
    dstHeight = destRect.bottom - destRect.top;
    LOG(TEXT("Adjusted Windows szie is %d:%d"), dstWidth, dstHeight);

    //Get x and y
    x = (screenWidth - dstWidth) / 2;
    y = (screenHeight - dstHeight) / 2;

    LOG(TEXT("Move Windows szie is %d:%d"), dstWidth, dstHeight);
    bResult = MoveWindow (hWnd, x, y, dstWidth, dstHeight, TRUE);

    //Move Window Failed
    if(!bResult)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        LOG(TEXT("%S: ERROR %d@%S - failed to move Window (0x%x)!!"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        goto cleanup;
    }

cleanup:
     if(pVMRWindowlessControl)
         pVMRWindowlessControl->Release();

    return hr;
}

#endif