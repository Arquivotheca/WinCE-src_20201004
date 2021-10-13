//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <tchar.h>
#include <stressutils.h>
#include <ddraw.h>
#include <ddraw_utils.h>

//******************************************************************************
// Types
typedef HRESULT (WINAPI * PFNDDRAWCREATE)(GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter);

//******************************************************************************
// Macros
#define countof(x) (sizeof(x)/sizeof(*(x)))
#define SafeRelease(p) do { if (NULL != p) { p->Release(); p = NULL; } } while(0)


//******************************************************************************
// Global constants
const TCHAR                  g_szClassName[]   = TEXT("s_DDraw");
// Global vars
HINSTANCE                     g_hInst                  = NULL;
DWORD                          g_dwWidth            = 0,
                                     g_dwHeight           = 0;
HWND                            g_hwndMain          = NULL;
HINSTANCE                    g_hInstDDraw        = NULL;
PFNDDRAWCREATE         g_pfnDDCreate      = NULL;
LPDIRECTDRAW              g_pDD                    = NULL;
LPDIRECTDRAWSURFACE g_pDDSPrimary     = NULL,
                                     g_pDDSOffScreen  = NULL;
DDCAPS                         g_ddDriverCaps,
                                     g_ddHELCaps;
DDSURFACEDESC            g_ddsdPrimary,
                                     g_ddsdOffScreen;

//******************************************************************************
// Function Implementation

//******************************************************************************
BOOL InitApplication(HINSTANCE hInstance)
{
    LogVerbose(_T("Initializing the application"));
    BOOL fReturnValue = TRUE;
    WNDCLASS  wc;
    
    wc.style = 0;
    wc.lpfnWndProc = (WNDPROC) DefWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = 0;
    wc.hCursor = 0;
    wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = g_szClassName;

    // Seed the random number generator
    srand(GetTickCount());

    if(!RegisterClass(&wc))
    {
        LogFail(_T("Register class failed. GetLastError:%d"), GetLastError());
        fReturnValue = FALSE;
    }

    return fReturnValue;
}

//******************************************************************************
void CleanupApplication(void)
{
    LogVerbose(_T("Cleanup application"));
    if (!UnregisterClass(g_szClassName, g_hInst))
    {
        LogFail(_T("%s: UnregisterClass() failed"), g_szClassName);
    }
}

//******************************************************************************
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    LogVerbose(_T("Initializing the Instance"));
    BOOL fReturnValue = TRUE;
    RECT rcPos = { 50, 50, 400, 280 };
    UINT dx = rand() % 100,
         dy = rand() % 100;
    
    g_hInst = hInstance;
    g_hwndMain = CreateWindowEx(
                                                        0,
                                                        g_szClassName,
                                                        g_szClassName,    // STRESS test needs this name
                                                        WS_OVERLAPPED | WS_CAPTION | WS_BORDER | WS_VISIBLE,
                                                        rcPos.left  + dx, rcPos.top    + dy,
                                                        rcPos.right + dx, rcPos.bottom + dy,
                                                        NULL,
                                                        NULL,
                                                        hInstance,
                                                        NULL);

    if (NULL == g_hwndMain)
    {
        LogFail(_T("Window creation failed on class %s.  GetLastError:%d"), g_szClassName, GetLastError());
        fReturnValue = FALSE;
    }

    ShowWindow(g_hwndMain, nCmdShow);
    if(!UpdateWindow(g_hwndMain))
    {
        LogFail(_T("UpdateWindow call failed.  GetLastError:%d"), GetLastError());
        fReturnValue = FALSE;
    }
    
    return fReturnValue;
}

//******************************************************************************
void CleanupInstance(void)
{
    LogVerbose(_T("Cleanup the instance"));
    if (IsWindow(g_hwndMain))
    {
        if(!DestroyWindow(g_hwndMain))
        {
            LogFail(_T("DestroyWindow failed.  GetLastError:%d"), GetLastError());
        }
        g_hwndMain = NULL;
    }
}

//******************************************************************************
BOOL DetectComponent(void)
{
    LogVerbose(_T("Detecting the existance of DirectDraw"));
    HINSTANCE hInst = NULL;
    BOOL fReturnValue = FALSE;
    if (NULL != (hInst = LoadLibrary(TEXT("ddraw.dll"))))
    {
        fReturnValue = (NULL != GetProcAddress(hInst, TEXT("DirectDrawCreate")));
        FreeLibrary(hInst);
    }
    else
    {
       LogFail(_T("DDraw.DLL not loadable or DirectDraw unsupported."));
    }
    return (fReturnValue);
}

//******************************************************************************
void CleanupComponent(void)
{
    LogVerbose(_T("Cleaning up the component"));
    
    SafeRelease(g_pDDSOffScreen);
    SafeRelease(g_pDDSPrimary);
    SafeRelease(g_pDD);
    
    if (NULL != g_hInstDDraw)
    {
        if(!FreeLibrary(g_hInstDDraw))
        {
            LogFail(_T("Free Library failed.  GetLastError:%d"), GetLastError);
        }
        g_hInstDDraw = NULL;
    }
}

//******************************************************************************
BOOL InitComponent(void)
{
    LPDIRECTDRAWCLIPPER pDDC = NULL;
    BOOL fReturnValue = FALSE;
    HRESULT hr;

    // Initialize the component globals
    CleanupComponent();

    LogVerbose(_T("Initializing the component"));
    
    // LoadLibrary the DDraw dll, to avoid staticly linking to it
    if (NULL == (g_hInstDDraw = LoadLibrary(TEXT("ddraw.dll"))))
    {
        LogFail(_T("DDraw.dll no longer loadable, test cannot continue. GetLastError:%d"), GetLastError());
        goto Exit;
    }
    
    // Get the creation entry point
    g_pfnDDCreate = (PFNDDRAWCREATE)GetProcAddress(g_hInstDDraw, TEXT("DirectDrawCreate"));
    if (NULL == g_pfnDDCreate)
    {
        LogFail(_T("DirectDrawCreate entry point no longer available, test cannot continue.  GetLastError:%d"), GetLastError());
        goto Exit;
    }

    // Create the DirectDraw object
    if (FAILED(hr = g_pfnDDCreate(NULL, &g_pDD, NULL)) && (NULL != g_pDD))
    {
        LogFail(_T("DirectDrawCreate failed, test cannot continue. hr:%s"), ErrorName[hr]);
        goto Exit;
    }

    // Set the cooperative level
    if (FAILED(hr = g_pDD->SetCooperativeLevel(g_hwndMain, DDSCL_NORMAL)))
    {
        LogFail(_T("SetCooperative Level DDSCL_NORMAL failed. hr:%s"), ErrorName[hr]);
        goto Exit;
    }
    LogVerbose(_T("DirectDraw created, and cooperative level set."));

    // get the direct draw capabilities
    memset(&g_ddDriverCaps, 0, sizeof(g_ddDriverCaps));
    memset(&g_ddHELCaps, 0, sizeof(g_ddHELCaps));
    g_ddDriverCaps.dwSize=sizeof(g_ddDriverCaps);
    g_ddHELCaps.dwSize=sizeof(g_ddHELCaps);
    
    if (FAILED(hr = g_pDD->GetCaps(&g_ddDriverCaps, &g_ddHELCaps)))
    {
        LogFail(_T("GetCaps failed. hr:%s"), ErrorName[hr]);
        goto Exit;
    }
    
    // Create the primary surface
    DDSURFACEDESC ddsd;
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    if (FAILED(hr = g_pDD->CreateSurface(&ddsd, &g_pDDSPrimary, NULL)))
    {
        LogFail(_T("Create primary surface failed, test cannot continue. hr:%s"), ErrorName[hr]);
        goto Exit;
    }
    LogVerbose(_T("Primary surface created."));
    memset(&g_ddsdPrimary, 0, sizeof(g_ddsdPrimary));
    g_ddsdPrimary.dwSize=sizeof(g_ddsdPrimary);
    if (FAILED(hr = g_pDDSPrimary->GetSurfaceDesc(&g_ddsdPrimary)))
    {
        LogFail(_T("%s: GetSurfaceDesc on Primary surface failed. hr:%s"), ErrorName[hr]);
        goto Exit;
    }

    // Get the main window's position
    RECT rc;
    if(!GetWindowRect(g_hwndMain, &rc))
    {
        LogFail(_T("GetWindowRect failed. GetLastError:%d"), GetLastError());
        goto Exit;
    }
    
    g_dwWidth = rc.right - rc.left;
    g_dwHeight = rc.bottom - rc.top;
    
    // Create an offscreen surface
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
    ddsd.dwHeight = g_dwHeight;
    ddsd.dwWidth = g_dwWidth;
    if (FAILED(hr = g_pDD->CreateSurface(&ddsd, &g_pDDSOffScreen, NULL)))
    {
        LogFail(_T("Creation of offscreen surface failed, test cannot continue. hr:%s"), ErrorName[hr]);
        goto Exit;
    }
    LogVerbose(_T("Offscreen surface created"));
    
    memset(&g_ddsdOffScreen, 0, sizeof(g_ddsdOffScreen));
    g_ddsdOffScreen.dwSize=sizeof(g_ddsdOffScreen);
    if (FAILED(hr = g_pDDSOffScreen->GetSurfaceDesc(&g_ddsdOffScreen)))
    {
        LogFail(_T("GetSurfaceDesc on OffScreen surface failed. hr:%s"), ErrorName[hr]);
        goto Exit;
    }

    // Clear the offscreen surface
    DDBLTFX ddbltfx;
    int i;
    memset(&ddbltfx, 0, sizeof(ddbltfx));
    ddbltfx.dwSize = sizeof(ddbltfx);
    ddbltfx.dwFillColor = rand() << 16 | rand();

    for(i = 0; i <= TRIES; i++)
    {   
        hr = g_pDDSOffScreen->Blt(NULL, NULL, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &ddbltfx);
        if(SUCCEEDED(hr))
            break;
        if(hr != DDERR_SURFACEBUSY || i == TRIES)
        {
            LogFail(_T("Color-Filling Blt to offscreen surface failed. hr:%s"), ErrorName[hr]);
            fReturnValue = FALSE;
        }
    }
        
    LogVerbose(_T("Offscreen filled with a random color."));
    // Create a clipper
    if (FAILED(hr = g_pDD->CreateClipper(0, &pDDC, NULL)) || NULL == pDDC)
    {
        LogFail(_T("Create clipper failed. hr:%s"), ErrorName[hr]);
        goto Exit;
    }
    LogVerbose(_T("Clipper created"));
    // Configure the clipper
    if (FAILED(hr = pDDC->SetHWnd(0, g_hwndMain)))
    {
        LogFail(_T("SetHWnd failed. hr:%s"), ErrorName[hr]);
        goto Exit;
    }

    // Attach the clipper to the primary
    if (FAILED(hr = g_pDDSPrimary->SetClipper(pDDC)))
    {
        LogFail(_T("SetClipper failed.  %s"), ErrorName[hr]);
        goto Exit;
    }
    LogVerbose(_T("Initialization completed with no errors."));
    // if none of the calls failed, then we succeeded.
    fReturnValue = TRUE;
    
Exit:
    // Always release the clipper, since we've either failed or we've already
    // attached it to the primary surface.
    SafeRelease(pDDC);

    // if any of the calls failed, then cleanup the compenent and exit.
    if (!fReturnValue)
    {
        CleanupComponent();
    }

    return fReturnValue;
}

//******************************************************************************
HFONT CreateSimpleFont(LPTSTR pszTypeface, LONG lSize)
{
    LogVerbose(_T("Creating a font."));
    LOGFONT lf;

    // Setup the font
    lf.lfHeight         = lSize;
    lf.lfWidth          = 0;
    lf.lfEscapement     = 0;
    lf.lfOrientation    = 0;
    lf.lfWeight         = FW_NORMAL;
    lf.lfItalic         = false;
    lf.lfUnderline      = false;
    lf.lfStrikeOut      = false;
    lf.lfCharSet        = ANSI_CHARSET;
    lf.lfOutPrecision   = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
    lf.lfQuality        = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = VARIABLE_PITCH;
    if(pszTypeface)
        _tcscpy(lf.lfFaceName, pszTypeface);

    return CreateFontIndirect(&lf);
}

//******************************************************************************
UINT RunTestIteration(void)
{
    LogVerbose(_T("RunTestIteration."));
    
    HRESULT hr;
    UINT Result = CESTRESS_PASS;
    BOOL fTmp;
    // Set up a random color fill (in case we need it).
    DDBLTFX ddbltfx;
    memset(&ddbltfx, 0, sizeof(ddbltfx));
    ddbltfx.dwSize = sizeof(ddbltfx);
    ddbltfx.dwFillColor = rand() << 16 | rand();

    // Grab the primary window's position on screen
    RECT rc;
    if(!GetWindowRect(g_hwndMain, &rc))
    {
        LogFail(_T("Failed to retrieve window rectangle for this iteration. GetLastError:%d"), GetLastError());
        Result = CESTRESS_FAIL;
        goto Exit;
    }
            
    // Pick one of the test scenarios
    switch(rand() % 10)
    {
        case 0:
        {
            // Simple color-filling Blt to the primary
            LogVerbose(_T("Color-filling primary surface."));
            for(int i = 0; i <= TRIES; i++)
            {   
                hr = g_pDDSPrimary->Blt(NULL, NULL, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &ddbltfx);
                if(SUCCEEDED(hr))
                    break;
                if(hr != DDERR_SURFACEBUSY || i == TRIES)
                {
                    LogFail(_T("Color-Filling Blt to Primary surface failed. hr:%s"), ErrorName[hr]);
                    Result = CESTRESS_FAIL;
                }
            }
        }
        break;

        case 1:
        {
            // Simple color-filling Blt to the offscreen surface
            LogVerbose(_T("Color-filling offscreen surface."));
            for(int i = 0; i <= TRIES; i++)
            {   
                hr = g_pDDSOffScreen->Blt(NULL, NULL, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &ddbltfx);
                if(SUCCEEDED(hr))
                    break;
                if(hr != DDERR_SURFACEBUSY || i == TRIES)
                {
                    LogFail(_T("Color-Filling Blt to offscreen surface failed. hr:%s"), ErrorName[hr]);
                    Result = CESTRESS_FAIL;
                }
            }
        }
        break;
        
        case 2:
        {
            // Use GDI to write to the primary surface
            HDC hdc = NULL;
            LogVerbose(_T("GDI write to primary surface."));

            for(int i = 0; i <= TRIES; i++)
            {
                hr = g_pDDSPrimary->GetDC(&hdc);
                if(SUCCEEDED(hr))
                    break;
                
                if((hr == DDERR_SURFACEBUSY || hr == DDERR_CANTCREATEDC) && i == TRIES)
                {
                    LogWarn2(_T("Unable retrieve the DC for the primary after %d tries."), TRIES);
                    Result = CESTRESS_WARN2;
                }
                else if(FAILED(hr) && !(hr == DDERR_SURFACEBUSY || hr == DDERR_CANTCREATEDC))
                {
                    LogFail(_T("GetDC on the primary failed hr:%s"), ErrorName[hr]);
                    Result = CESTRESS_FAIL;
                }
                else Sleep(100);
            }
                        
            if (SUCCEEDED(hr))
            {
                HBRUSH hbrushNew = CreateSolidBrush(RGB(rand(), 0x00, rand()/2));
                if (NULL != hbrushNew)
                {
                    // Start using our new brush            
                    HBRUSH hbrushOld;

                    if(NULL == (hbrushOld= (HBRUSH)SelectObject(hdc, hbrushNew)))
                    {
                        LogFail(_T("SelectObject on the new brush failed. GetLastError:%d"), GetLastError());
                        Result = CESTRESS_FAIL;
                    }
                    LogVerbose(_T("Drawing an ellipse."));
                    // Draw some stuff
                    if(!Ellipse(hdc, rc.left+(rand() % 10), rc.top+(rand() % 10), rc.right-(rand() % 10), rc.bottom-(rand() % 10)))
                    {
                        LogFail(_T("Ellipse on DC of primary failed. GetLastError:%d"), GetLastError());
                        Result = CESTRESS_FAIL;
                    }
                    
                    // Restore original brush
                    if(NULL == SelectObject(hdc, hbrushOld))
                    {
                        LogFail(_T("SelectObject failed when reselecting the old brush. GetLastError:%d"), GetLastError());
                        Result = CESTRESS_FAIL;
                    }

                    // Delete the brush we created
                    if(!DeleteObject(hbrushNew))
                    {
                        LogFail(_T("Deletion of the new brush failed. GetLastError:%d"), GetLastError());
                        Result = CESTRESS_FAIL;
                    }
                }
                else
                {
                    LogFail(_T("CreateSolidBrush failed. GetLastError:%d"), GetLastError());
                    Result = CESTRESS_FAIL;
                }
                if(FAILED(g_pDDSPrimary->ReleaseDC(hdc)))
                {
                    LogFail(_T("ReleaseDC on the primary failed. GetLastError:%d"), GetLastError());
                    Result = CESTRESS_FAIL;
                }

                hdc = NULL;
            }
        }
        break;
        
        case 3:
        {
            // Use GDI to write to the offscreen surface
            const TCHAR szMessage[] = TEXT("DDraw GDI write to offscreen surface");
            HDC hdc = NULL;
            
            LogVerbose(_T("GDI write to offscreen surface."));

            for(int i = 0; i <= TRIES; i++)
            {
                hr = g_pDDSOffScreen->GetDC(&hdc);
                if(SUCCEEDED(hr))
                    break;
                if((hr == DDERR_SURFACEBUSY || hr == DDERR_CANTCREATEDC) && i == TRIES)
                {
                    LogWarn2(_T("Unable retrieve the DC for the offscreen after %d tries."), TRIES);
                    Result = CESTRESS_WARN2;
                }
                else if(FAILED(hr) && !(hr == DDERR_SURFACEBUSY || hr == DDERR_CANTCREATEDC))
                {
                    LogFail(_T("GetDC on the offscreen failed hr:%s"), ErrorName[hr]);
                    Result = CESTRESS_FAIL;
                }
                else Sleep(100);
            }

            if (SUCCEEDED(hr))
            {
                HFONT hfontNew = CreateSimpleFont(TEXT("Arial"), 12);
                if (NULL != hfontNew)
                {
                    // Start using our new brush            
                    HFONT hfontOld;
                    if(NULL == (hfontOld = (HFONT)SelectObject(hdc, hfontNew)))
                    {
                        LogFail(_T("Failed to select font into the dc. GetLastError:%d"), GetLastError());
                        Result = CESTRESS_FAIL;
                    }

                    // Draw some stuff
                    LogVerbose(_T("Drawing text message on offscreen"));
                    if(!ExtTextOut(hdc, 0, g_dwHeight/2, 0, NULL, szMessage, countof(szMessage)-1, NULL))
                    {
                        LogFail(_T("Failed to write text to the dc. GetLastError:%d"), GetLastError());
                        Result = CESTRESS_FAIL;
                    }
                    
                    // Restore original font
                    if(NULL == SelectObject(hdc, hfontOld))
                    {
                        LogFail(_T("Failed to select old font into the DC. GetLastError:%d"), GetLastError());
                        Result = CESTRESS_FAIL;
                    }

                    // Delete the font we created
                    if(!DeleteObject(hfontNew))
                    {
                        LogFail(_T("Failed to delete font. GetLastError:%d"), GetLastError());
                        Result = CESTRESS_FAIL;
                    }
                }
                else
                {
                    LogFail(_T("CreateSimpleFont failed. GetLastError:%d"), GetLastError());
                    Result = CESTRESS_FAIL;
                }
                if(FAILED(g_pDDSOffScreen->ReleaseDC(hdc)))
                {
                    LogFail(_T("ReleaseDC on the offscreen failed. GetLastError:%d"), GetLastError());
                    Result = CESTRESS_FAIL;
                }
                
                hdc = NULL;
            }
        }
        break;

        case 4:
        {
            // Simple offscreen to primary blt
            LogVerbose(_T("Blt from offscreen surface to primary."));
            for(int i = 0; i <= TRIES; i++)
            {
                hr = g_pDDSPrimary->Blt(&rc, g_pDDSOffScreen, NULL, DDBLT_WAIT, NULL);
                if(SUCCEEDED(hr))
                    break;
                if(hr != DDERR_SURFACEBUSY || i == TRIES)
                {
                    LogFail(_T("Blt from offscreen to primary Failed. hr:%s"), ErrorName[hr]);
                    Result = CESTRESS_FAIL;
                }
            }
        }
        break;
            
        case 5:
        {   
            // simple offscreen to primary blit with random rectangles

            LogVerbose(_T("Offscreen surface to primary with random rectangles."));
            RECT rcSrc, rcDst;
            GenRandRect(&rc, &rcDst);
            GenRandRect(g_dwWidth, g_dwHeight, &rcSrc);

            // Offscreen to primary blt with stretching, shrinking and random locations.
            for(int i = 0; i <= TRIES; i++)
            {
                hr = g_pDDSPrimary->Blt(&rcDst, g_pDDSOffScreen, &rcSrc, DDBLT_WAIT, NULL);
                if(SUCCEEDED(hr))
                    break;
                if(hr != DDERR_SURFACEBUSY || i == TRIES)
                {
                    LogFail(_T("Offscreen to primary with random source/dest failed. hr:%s"), ErrorName[hr]);
                    Result = CESTRESS_FAIL;
                }
            }
        }
        break;
        
        case 6:
        {
            // lock and random fill

            LogVerbose(_T("Locking the primary and filling with random data."));

            DDSURFACEDESC ddsdPrimary;
            DWORD *dest;
            RECT rcLoc=rc;

            for(int i = 0; i <= TRIES; i++)
            {
                hr = g_pDDSPrimary->Lock(NULL, &ddsdPrimary, NULL, NULL);
                if(SUCCEEDED(hr))
                    break;

                if((hr == DDERR_SURFACEBUSY || hr == DDERR_WASSTILLDRAWING) && i == TRIES)
                {
                    LogWarn2(_T("Unable to lock primary after %d tries."), TRIES);
                    Result = CESTRESS_WARN2;
                }
                else if(FAILED(hr) && !(hr == DDERR_SURFACEBUSY || hr == DDERR_WASSTILLDRAWING))
                {
                    LogFail(_T("Locking the primary failed hr:%s"), ErrorName[hr]);
                    Result = CESTRESS_FAIL;
                }
                else Sleep(100);
            }

            if(SUCCEEDED(hr))
            {
                dest=(DWORD*) ddsdPrimary.lpSurface;
                double dwPPW = 32./ddsdPrimary.ddpfPixelFormat.dwRGBBitCount;

                // set the starting x location based on the window position.  if the left edge is off the screen, then x is 0.
                int iXStart;
                if(rcLoc.left < 0)
                    rcLoc.left=0;
                // if the starting left edge is fractional because we're doing more than one pixel per write, then start at the
                // next pixel, this prevents us from writing outside the window.
                if((rcLoc.left/dwPPW-(int)(rcLoc.left/dwPPW))>0)
                    iXStart=(int)(rcLoc.left/dwPPW)+2;
                // we start one from the left for the window border, so it still looks good, but not required.
                else iXStart=(int)(rcLoc.left/dwPPW)+1;

                // handle the cases where the rest of the surface goes off the screen.
                if(rcLoc.top < 0)
                    rcLoc.top=0;
                if(rcLoc.bottom>(int)g_ddsdPrimary.dwHeight)
                    rcLoc.bottom=g_ddsdPrimary.dwHeight;
                if(rcLoc.right > (int)g_ddsdPrimary.dwWidth)
                    rcLoc.right = g_ddsdPrimary.dwWidth;
                    

                // step through all of the rows we need to write to
                for(int y=(rcLoc.top+24); y<(int) (rcLoc.bottom-1) && y < (int) ddsdPrimary.dwHeight; y++)
                {   
                    DWORD dwColor;
                    // if we're 32 bit color, then create a random 32 bit color
                    if(ddsdPrimary.ddpfPixelFormat.dwRGBBitCount==32)
                        dwColor=rand()%0xFF << 24 | rand()%0xFF << 16 | rand()%0xFF << 8 | rand()%0xFF;
                    // if we're 16 bit color, then we really should have the top 16 be the same as the bottom 16 for a solid colored line.
                    else if(ddsdPrimary.ddpfPixelFormat.dwRGBBitCount==16)
                    {
                        dwColor=  rand()%0xFF << 8 | rand()%0xFF;
                        dwColor |= dwColor<<16;
                        // fill in the first pixel just in case we're on a half pixel boundry.  this is so the left edge is at the 
                        // window edge
                        WORD * tmp=(WORD*) dest;
                        (tmp+(ddsdPrimary.lPitch/2)*y)[rcLoc.left+1]=(WORD)dwColor;
                    }
                    // it's 8 or 24, in which case we just fill it with 4 8 bit values.  if it's 24, then the line will won't be a solid color
                    else
                    {
                        dwColor=  rand()%0xFF;
                        dwColor |= dwColor<<8;
                        dwColor |= dwColor<<16;

                        // same as the 16 bit case, just in case we're at a half, quarter pixel boundry.
                        BYTE * tmp=(BYTE*) dest;
                        (tmp+(ddsdPrimary.lPitch)*y)[rcLoc.left+1]=(BYTE)dwColor;
                        (tmp+(ddsdPrimary.lPitch)*y)[rcLoc.left+2]=(BYTE)dwColor;
                        (tmp+(ddsdPrimary.lPitch)*y)[rcLoc.left+3]=(BYTE)dwColor;
                    }

                    // for the whole row, fill in the pixel's
                    for(int x=iXStart; x<(int)(rcLoc.right/dwPPW-1) && x < (int) ddsdPrimary.dwWidth/dwPPW; x++)
                    {
                        (dest+(ddsdPrimary.lPitch/4)*y)[x]=dwColor;
                    }
                    // done filling in the row, clean up the right edge if we aren't at the right edge. (same as up above with the left edge)
                    if(((int)rcLoc.right < (int)ddsdPrimary.dwWidth))
                    {
                        if(ddsdPrimary.ddpfPixelFormat.dwRGBBitCount==16)
                        {
                            WORD * tmp=(WORD*) dest;
                            (tmp+(ddsdPrimary.lPitch/2)*y)[rcLoc.right-2]=(WORD)dwColor;
                        }
                        else if(ddsdPrimary.ddpfPixelFormat.dwRGBBitCount!=32)
                        {
                            BYTE * tmp=(BYTE*) dest;
                            (tmp+(ddsdPrimary.lPitch)*y)[rcLoc.right-2]=(BYTE)dwColor;
                            (tmp+(ddsdPrimary.lPitch)*y)[rcLoc.right-3]=(BYTE)dwColor;
                        }
                    }
                }

                if(FAILED(hr = g_pDDSPrimary->Unlock(NULL)))
                {
                    LogFail(_T("Unable to unlock primary. hr:%s"), ErrorName[hr]);
                    Result = CESTRESS_FAIL;
                }
            }
        }
        break;
        case 7:
        {
            // lock and random fill, but this time do the offscreen surface.

            LogVerbose(_T("Locking the Offscreen and filling with random data."));
            
            DDSURFACEDESC ddsd;
            DWORD *dest;

            for(int i = 0; i <= TRIES; i++)
            {
                hr = g_pDDSOffScreen->Lock(NULL, &ddsd, NULL, NULL);

                if(SUCCEEDED(hr))
                    break;

                if((hr == DDERR_SURFACEBUSY  || hr == DDERR_WASSTILLDRAWING) && i == TRIES)
                {
                    LogWarn2(_T("Unable to lock the offscreen surface after %d tries."), TRIES);
                    Result = CESTRESS_WARN2;
                }
                else if(FAILED(hr) && !(hr == DDERR_SURFACEBUSY || hr == DDERR_WASSTILLDRAWING))
                {
                    LogFail(_T("Locking the offscreen surface failed hr:%s"), ErrorName[hr]);
                    Result = CESTRESS_FAIL;
                }
                else Sleep(100);
            }

            if(SUCCEEDED(hr))
            {
                RECT rcLoc= {0, 0, ddsd.dwWidth, ddsd.dwHeight};
                
                dest=(DWORD*) ddsd.lpSurface;
                double dwPPW = 32./ddsd.ddpfPixelFormat.dwRGBBitCount;

                for(int y=(rcLoc.top); y<(int) (rcLoc.bottom); y++)
                {   
                    DWORD dwColor;

                    // if we're 32 bit color, create a random color
                    if(ddsd.ddpfPixelFormat.dwRGBBitCount==32)
                        dwColor=rand()%0xFF << 24 | rand()%0xFF << 16 | rand()%0xFF << 8 | rand()%0xFF;
                    // 16 bit color, create one 16 bit color and copy it to the top 16 bits.
                    else if(ddsd.ddpfPixelFormat.dwRGBBitCount==16)
                    {
                        dwColor=  rand()%0xFF << 8 | rand()%0xFF;
                        dwColor |= dwColor<<16;
                    }
                    // fill in the 4 8 bit color's
                    else // it's 8 or 24, in which case we just fill it with 4 8 bit values.
                    {
                        dwColor=  rand()%0xFF;
                        dwColor |= dwColor<<8;
                        dwColor |= dwColor<<16;  
                    }
                    // step through the whole row and fill it in.
                    for(int x=rcLoc.left; x<(int)(rcLoc.right/dwPPW); x++)
                    {
                        (dest+(ddsd.lPitch/4)*y)[x]=dwColor;
                    }

                }

                if(FAILED(hr = g_pDDSOffScreen->Unlock(NULL)))
                {
                    LogFail(_T("Unable to unlock offscreen. hr:%s"), ErrorName[hr]);
                    Result = CESTRESS_FAIL;
                }
            }
        }
        break;
        case 8:
        {
            // test out colorkeying.
            LogVerbose(_T("Source Colorkeying."));
            if(g_ddDriverCaps.dwCKeyCaps & DDCKEYCAPS_SRCBLT || g_ddHELCaps.dwCKeyCaps & DDCKEYCAPS_SRCBLT)   
            {
                // source colorkeying to the primary or the offscreen
                DWORD dwColor1=rand()%0xFF << 24 | rand()%0xFF << 16 | rand()%0xFF << 8 | rand()%0xFF,
                      dwColor2=rand()%0xFF << 24 | rand()%0xFF << 16 | rand()%0xFF << 8 | rand()%0xFF,
                      dwColor3=rand()%0xFF << 24 | rand()%0xFF << 16 | rand()%0xFF << 8 | rand()%0xFF,
                      dwColor4=rand()%0xFF << 24 | rand()%0xFF << 16 | rand()%0xFF << 8 | rand()%0xFF;
                // fill in the primary surface with data, horizontal/vertical, whatever...
                if(rand() %2)
                    fTmp = FillSurfaceHorizontal(g_pDDSPrimary, dwColor1, dwColor2);
                else
                    fTmp = FillSurfaceVertical(g_pDDSPrimary, dwColor1, dwColor2);
                if(!fTmp)
                {
                    LogFail(_T("Fill surface on primary failed"));
                    Result = CESTRESS_FAIL;
                }
                // fill up the offscreen with some data.
                if(rand() %2)
                    fTmp = FillSurfaceHorizontal(g_pDDSOffScreen, dwColor3, dwColor4);
                else
                    fTmp = FillSurfaceVertical(g_pDDSOffScreen, dwColor3, dwColor4);
                
                if(!fTmp)
                {
                    LogFail(_T("Fill surface on offscreen failed"));
                    Result = CESTRESS_FAIL;
                }

                DDCOLORKEY ddck;
                memset(&ddck, 0, sizeof(ddck));
                
                // color key with the primary as the dest
                if(rand()%2)
                {   
                    // set the source colorkey, one of the colors we filled the offscreen up above with
                    ddck.dwColorSpaceLowValue=ddck.dwColorSpaceHighValue=(rand()%2) ? dwColor3 : dwColor4;
                    if(FAILED(hr=g_pDDSOffScreen->SetColorKey(DDCKEY_SRCBLT, &ddck)))
                    {
                        LogFail(_T("Unable to set color key on offscreen surface. hr:%s"), ErrorName[hr]);
                        Result = CESTRESS_FAIL;
                    }
                    
                    for(int i = 0; i <= TRIES; i++)
                    {
                        hr=g_pDDSPrimary->Blt(NULL, g_pDDSOffScreen, NULL, DDBLT_WAIT | DDBLT_KEYSRC, NULL);
                        if(SUCCEEDED(hr))
                            break;
                        if(hr != DDERR_SURFACEBUSY || i == TRIES)
                        {
                            LogFail(_T("Blt to primary surface failed. hr:%s"), ErrorName[hr]);
                            Result = CESTRESS_FAIL;
                        }
                    }
                }
                // colorkey with the offscreen as the dest
                else
                {
                    RECT rcSrc=rc;
                    
                    // rc is set from getwindowrect which will return the position of the window as points off the screen.  eg, on a 
                    // 640x480 surface it can return the top of the window as -20 or the bottom as 500.
                    if(rcSrc.left < 0)
                        rcSrc.left=0;
                    if(rcSrc.right > (int)g_ddsdPrimary.dwWidth)
                        rcSrc.right=g_ddsdPrimary.dwWidth;
                    if(rcSrc.top < 0)
                        rcSrc.top=0;
                    if(rcSrc.bottom > (int)g_ddsdPrimary.dwHeight)
                        rcSrc.bottom=g_ddsdPrimary.dwHeight;

                    RECT rcDst={0, 0, rcSrc.right-rcSrc.left, rcSrc.bottom-rcSrc.top};
                    
                    // set the source colorkey, one of the colors we filled the primary with up above.
                    ddck.dwColorSpaceLowValue=ddck.dwColorSpaceHighValue=(rand()%2) ? dwColor1 : dwColor2;
                    if(FAILED(hr=g_pDDSPrimary->SetColorKey(DDCKEY_SRCBLT, &ddck)))
                    {
                        LogFail(_T("Unable to set color key on primary surface. hr:%s"), ErrorName[hr]);
                        Result = CESTRESS_FAIL;
                    }

                    for(int i = 0; i <= TRIES; i++)
                    {
                        hr=g_pDDSOffScreen->Blt(&rcDst, g_pDDSPrimary, &rcSrc, DDBLT_WAIT | DDBLT_KEYSRC, NULL);
                        if(SUCCEEDED(hr))
                            break;
                        if(hr != DDERR_SURFACEBUSY || i == TRIES)
                        {
                            LogFail(_T("Blt to offscreen surface failed. hr:%s"), ErrorName[hr]);
                            Result = CESTRESS_FAIL;
                        }
                    }
                }
            }
        }
        break;
        case 9:
        {
            LogVerbose(_T("Releasing and recreating surfaces."));
            // if we cannot reinitialize, then the module must be terminated.
            // initcomponent takes care of releasing everything.
            if(!InitComponent())
                Result = CESTRESS_ABORT;
        }
        break;
    }
Exit:
    return Result;
}

/////////////////////////////////////////////////////////////////////////////////////
BOOL InitializeStressModule (
                            /*[in]*/ MODULE_PARAMS* pmp, 
                            /*[out]*/ UINT* pnThreads
                            )
{
    LogVerbose(_T("Initializing the stress module."));
    
    TCHAR tsz[MAX_PATH] = {0};
    BOOL fReturnVal = FALSE;
    
    *pnThreads = 1;
    
    InitializeStressUtils (
                            _T("S2_DDRAW"),                                    // Module name to be used in logging
                            LOGZONE(SLOG_SPACE_DX, SLOG_DDRAW),    // set to log to the ddraw logging zone.
                            pmp                                                // Forward the Module params passed on the cmd line
                            );

    if(DetectComponent())
    {
        if(InitApplication(g_hInst))
        {
            if(InitInstance(g_hInst, SW_SHOW))
            {
                if(InitComponent())
                {
                    fReturnVal = TRUE;
                }
                else
                {
                    // if init component failed (shouldn't because detect component says it's there
                    // cleanup the instance and application and exit.
                    CleanupInstance();
                    CleanupApplication();
                }
            }
            // if InitApplication succeeded, but InitInstance failed, cleanup the app and exit
            else CleanupApplication();
        }
    }
    
    GetModuleFileName(g_hInst, tsz, MAX_PATH-1);
    LogVerbose(_T("Module File Name: %s"), tsz);
    
    return fReturnVal;
}



/////////////////////////////////////////////////////////////////////////////////////
UINT DoStressIteration (
                        /*[in]*/ HANDLE hThread, 
                        /*[in]*/ DWORD dwThreadId, 
                        /*[in,out]*/ LPVOID pv /*unused*/)
{
    UINT nResult = CESTRESS_PASS;

    LogVerbose(_T("Doing a stress iteration."));
    nResult = RunTestIteration();

    return nResult;
}



/////////////////////////////////////////////////////////////////////////////////////
DWORD TerminateStressModule (void)
{
    LogVerbose(_T("Stress module terminated, cleaning up."));
    CleanupComponent();
    CleanupInstance();
    CleanupApplication();
    return ((DWORD) -1);
}



///////////////////////////////////////////////////////////
BOOL WINAPI DllMain(
                    HANDLE hInstance, 
                    ULONG dwReason, 
                    LPVOID lpReserved
                    )
{
    g_hInst = (HINSTANCE) hInstance;
    return TRUE;
}
