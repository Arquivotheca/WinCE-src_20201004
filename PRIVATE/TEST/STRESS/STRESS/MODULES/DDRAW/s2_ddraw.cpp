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
#define DDRAWSTRESS_THREADCOUNT (3)

// With the new VS8 shell, 10 seconds is not enough when outputting lots of debug spew.
// Overcompensate to account for other modules running as well.
#define DDRAWSTRESS_MAXTIMEOUT (45000)
#define DDRAWSTRESS_MUTEXNAME _T("Mutex: S2_DDRAW Stress Test")

#if 0
#define DDRAWSTRESS_PAUSE DDrawStressPause()
#else
#define DDRAWSTRESS_PAUSE
#endif

//******************************************************************************
// Global constants
const TCHAR           g_szClassName[]       = TEXT("s_DDraw");
// Global vars
HINSTANCE             g_hInst               = NULL;
HANDLE                g_MutexRunOneInstance = NULL;

void DDrawStressPause()
{
    DebugBreak();
}

class DDrawStressThread
{
public:
    DDrawStressThread();
    static BOOL InitInstance(HINSTANCE hInstance, const TCHAR * szClassName, INT nCmdShow);
    static void CleanupInstance();
    static BOOL DectectComponent();
    static void CleanupStaticComponents();
    static BOOL InitStaticComponents();
    BOOL InitComponent();
    void CleanupComponent();

    // Just increments the number of threads that have exited.
    void CleanupThread();
    HFONT CreateSimpleFont(LPCTSTR pszTypeface, LONG lSize);
    UINT EnterCleanupPen();
    UINT ReinitializeAll();
    UINT RunTestIteration();
    
private:
    static const TCHAR *         sm_szClassName;
    static HINSTANCE             sm_hInst;
    static DWORD                 sm_dwWidth,
                                 sm_dwHeight;
    static HWND                  sm_hwndMain;
    static RECT                  sm_rcWindow;
    static HINSTANCE             sm_hInstDDraw;
    static PFNDDRAWCREATE        sm_pfnDDCreate;
    static LPDIRECTDRAW          sm_pDD;
    static LPDIRECTDRAWSURFACE   sm_pDDSPrimary;
    static DDCAPS                sm_ddDriverCaps;
    static DDSURFACEDESC         sm_ddsdPrimary;

// Synchronization objects
    // As threads exit, they increment this value, which will be used to determine if a thread
    // in ReinitializeAll should return abort or pass
    static LONG sm_lThreadsCleanedUp;
    
    // CleanupTime: boolean: A thread has entered the ReinitializeAll, so other threads should go
    // to the CleanupPen.
    static LONG sm_fCleanupTime;
    // WaitingThreadCount: int: How many threads have successfully entered the CleanupPen?
    // The last thread to enter will see this as TotalThreads-1 and signal the 
    // EventAllThreadsAreWaiting. Once the ThreadsPleaseContinue event is pulsed, each thread
    // will decrement this. The thread that sees the result as 0 will signal the 
    // ThreadsDoneWaiting event.
    static LONG sm_lWaitingThreadCount;

    // MutexReinitializeAll: Upon entering the ReinitializeAll function, wait for this event with
    // no timeout. If the thread doesn't get the event (i.e. times out) return. Otherwise start
    // reinitializing everything.
    static HANDLE sm_MutexReinitializeAll;
    // EventAllThreadsAreWaiting: The last thread to enter the cleanup pen (determined by having
    // InterlockedIncrement of sm_lWaitingThreadCount be TotalThreads-1) will set this event,
    // which will tell the reinit thread it can start hacking away at pointers.
    // This is an AutoReset event
    static HANDLE sm_EventAllThreadsAreWaiting;
    // EventThreadsPleaseContinue: All non-reinit threads will finally end up waiting on this event.
    // When the reinit thread is done mucking with pointers and has a good ddraw, it first clears the
    // sm_fCleanupTime flag and then sets this event to tell the other threads to continue partying.
    // This is a manual reset event
    static HANDLE sm_EventThreadsPleaseContinue;
    // The last thread to leave the CleanupPen will signal this event, tell the reinit thread it can 
    // release the ReinitializeAll mutex and continue partying.
    // This is an AutoReset event
    static HANDLE sm_EventThreadsDoneWaiting;
    
    LPDIRECTDRAWSURFACE   m_pDDSOffScreen;
    DDSURFACEDESC         m_ddsdOffScreen;
};

DDrawStressThread * g_ThreadContexts[DDRAWSTRESS_THREADCOUNT] = {0};
const TCHAR *         DDrawStressThread::sm_szClassName = NULL;
HINSTANCE             DDrawStressThread::sm_hInst = NULL;
DWORD                 DDrawStressThread::sm_dwWidth = 0;
DWORD                 DDrawStressThread::sm_dwHeight = 0;
HWND                  DDrawStressThread::sm_hwndMain = NULL;
RECT                  DDrawStressThread::sm_rcWindow = {0};
HINSTANCE             DDrawStressThread::sm_hInstDDraw = NULL;
PFNDDRAWCREATE        DDrawStressThread::sm_pfnDDCreate = NULL;
LPDIRECTDRAW          DDrawStressThread::sm_pDD = NULL;
LPDIRECTDRAWSURFACE   DDrawStressThread::sm_pDDSPrimary = NULL;
DDCAPS                DDrawStressThread::sm_ddDriverCaps = {0};
DDSURFACEDESC         DDrawStressThread::sm_ddsdPrimary = {0};

LONG                  DDrawStressThread::sm_lThreadsCleanedUp = 0;
LONG                  DDrawStressThread::sm_fCleanupTime = 0;
LONG                  DDrawStressThread::sm_lWaitingThreadCount = 0;
HANDLE                DDrawStressThread::sm_MutexReinitializeAll = NULL;
HANDLE                DDrawStressThread::sm_EventAllThreadsAreWaiting = NULL;
HANDLE                DDrawStressThread::sm_EventThreadsPleaseContinue = NULL;
HANDLE                DDrawStressThread::sm_EventThreadsDoneWaiting = NULL;


//******************************************************************************
// Function Implementation

//******************************************************************************
DDrawStressThread::DDrawStressThread() :
    m_pDDSOffScreen(NULL)
{
}

//******************************************************************************
BOOL DDrawStressThread::InitInstance(HINSTANCE hInstance, const TCHAR * szClassName, int nCmdShow)
{
    LogVerbose(_T("Initializing the Instance"));
    BOOL fReturnValue = TRUE;
    RECT rcPos;
    RECT rcDesktop;
    RECT rcIntersect;
    int iWidth;
    int iHeight;

    // Initialize the synchronization objects
    
    sm_MutexReinitializeAll = CreateMutex(
        NULL,  // Security
        FALSE, // Is the creating thread the initial owner? We don't want that.
        NULL); // Name
    sm_EventAllThreadsAreWaiting = CreateEvent(
        NULL,  // Security
        FALSE, // ManualReset: We want an auto reset event here
        FALSE, // InitialState: We want this to be unsignaled.
        NULL); // Name
    sm_EventThreadsPleaseContinue = CreateEvent(
        NULL,  // Security
        TRUE,  // ManualReset: We want a manual reset here, so that multiple threads can be signaled
        FALSE, // InitialState: We want this to be unsignaled.
        NULL); // Name
    sm_EventThreadsDoneWaiting = CreateEvent(
        NULL,  // Security
        FALSE, // ManualReset: We want an auto reset event here
        FALSE, // InitialState: We want this to be unsignaled.
        NULL); // Name

    if (NULL == sm_MutexReinitializeAll ||
        NULL == sm_EventAllThreadsAreWaiting ||
        NULL == sm_EventThreadsPleaseContinue ||
        NULL == sm_EventThreadsDoneWaiting)
    {
        LogAbort(_T("The synchronization objects could not be created"));
        return FALSE;
    }

    sm_hInst = hInstance;
    sm_szClassName = szClassName;

    do
    {
        if (!SystemParametersInfo(SPI_GETWORKAREA, 0, &rcDesktop, FALSE))
        {
            LogFail(_T("Could not get work area. GetLastError:%d"), GetLastError());
            fReturnValue = FALSE;
        }

        CopyRect(&rcPos, &rcDesktop);

        iWidth = rcPos.right - rcPos.left;
        iHeight = rcPos.bottom - rcPos.top;

        iWidth /= 2;
        iHeight /= 2;

        InflateRect(&rcPos, -(iWidth/4 + rand()%(iWidth/2)), -(iHeight/4 + rand()%(iHeight/2)));

        OffsetRect(&rcPos, (rand() % (iWidth * 3)) - (iWidth * 3 / 2), (rand() % (iHeight * 3)) - (iHeight * 3 / 2));

        IntersectRect(&rcIntersect, &rcDesktop, &rcPos);
    }while (IsRectEmpty(&rcIntersect));
    
    sm_hwndMain = CreateWindowEx(
        0,
        sm_szClassName,
        sm_szClassName,    // STRESS test needs this name
        WS_OVERLAPPED | WS_CAPTION | WS_BORDER | WS_VISIBLE,
        rcPos.left,
        rcPos.top,
        rcPos.right - rcPos.left,
        rcPos.bottom - rcPos.top,
        NULL,
        NULL,
        sm_hInst,
        NULL);

    if (NULL == sm_hwndMain)
    {
        LogFail(_T("Window creation failed on class %s.  GetLastError:%d"), sm_szClassName, GetLastError());
        fReturnValue = FALSE;
    }

    ShowWindow(sm_hwndMain, nCmdShow);
    if(!UpdateWindow(sm_hwndMain))
    {
        LogFail(_T("UpdateWindow call failed.  GetLastError:%d"), GetLastError());
        fReturnValue = FALSE;
    }

    GetWindowRect(sm_hwndMain, &sm_rcWindow);
    
    return fReturnValue;
}

//******************************************************************************
void DDrawStressThread::CleanupInstance(void)
{
    LogVerbose(_T("Cleanup the instance"));
    
    CloseHandle(sm_MutexReinitializeAll);
    CloseHandle(sm_EventAllThreadsAreWaiting);
    CloseHandle(sm_EventThreadsPleaseContinue);
    CloseHandle(sm_EventThreadsDoneWaiting);
    
    if (IsWindow(sm_hwndMain))
    {
        if(!DestroyWindow(sm_hwndMain))
        {
            LogFail(_T("DestroyWindow failed.  GetLastError:%d"), GetLastError());
        }
        sm_hwndMain = NULL;
    }
}
void DDrawStressThread::CleanupThread()
{
    InterlockedIncrement(&sm_lThreadsCleanedUp);
    return;
}


//******************************************************************************
void DDrawStressThread::CleanupComponent(void)
{
    LogVerbose(_T("Cleaning up the component"));
    
    SafeRelease(m_pDDSOffScreen);
}

void DDrawStressThread::CleanupStaticComponents(void)
{
    SafeRelease(sm_pDDSPrimary);
    SafeRelease(sm_pDD);
    
    if (NULL != sm_hInstDDraw)
    {
        if(!FreeLibrary(sm_hInstDDraw))
        {
            LogFail(_T("Free Library failed.  GetLastError:%d"), GetLastError);
        }
        sm_hInstDDraw = NULL;
    }
}

//******************************************************************************
BOOL DDrawStressThread::InitStaticComponents(void)
{
    LPDIRECTDRAWCLIPPER pDDC = NULL;
    BOOL fReturnValue = FALSE;
    HRESULT hr;

    // Initialize the component globals
    CleanupStaticComponents();

    LogVerbose(_T("Initializing the component"));
    
    // LoadLibrary the DDraw dll, to avoid staticly linking to it
    if (NULL == (sm_hInstDDraw = LoadLibrary(TEXT("ddraw.dll"))))
    {
        LogFail(_T("DDraw.dll no longer loadable, test cannot continue. GetLastError:%d"), GetLastError());
        goto Exit;
    }
    
    // Get the creation entry point
    sm_pfnDDCreate = (PFNDDRAWCREATE)GetProcAddress(sm_hInstDDraw, TEXT("DirectDrawCreate"));
    if (NULL == sm_pfnDDCreate)
    {
        LogFail(_T("DirectDrawCreate entry point no longer available, test cannot continue.  GetLastError:%d"), GetLastError());
        goto Exit;
    }

    // Create the DirectDraw object
    if (FAILED(hr = sm_pfnDDCreate(NULL, &sm_pDD, NULL)) || (NULL == sm_pDD))
    {
        LogFail(_T("DirectDrawCreate failed, test cannot continue. hr:%s"), ErrorName[hr]);
        goto Exit;
    }

    // Set the cooperative level
    if (FAILED(hr = sm_pDD->SetCooperativeLevel(sm_hwndMain, DDSCL_NORMAL)))
    {
        LogFail(_T("SetCooperative Level DDSCL_NORMAL failed. hr:%s"), ErrorName[hr]);
        goto Exit;
    }
    LogVerbose(_T("DirectDraw created, and cooperative level set."));

    // get the direct draw capabilities
    memset(&sm_ddDriverCaps, 0, sizeof(sm_ddDriverCaps));
    sm_ddDriverCaps.dwSize=sizeof(sm_ddDriverCaps);
    
    if (FAILED(hr = sm_pDD->GetCaps(&sm_ddDriverCaps, NULL)))
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
    if (FAILED(hr = sm_pDD->CreateSurface(&ddsd, &sm_pDDSPrimary, NULL)))
    {
        LogFail(_T("Create primary surface failed, test cannot continue. hr:%s"), ErrorName[hr]);
        goto Exit;
    }
    LogVerbose(_T("Primary surface created."));
    memset(&sm_ddsdPrimary, 0, sizeof(sm_ddsdPrimary));
    sm_ddsdPrimary.dwSize=sizeof(sm_ddsdPrimary);
    if (FAILED(hr = sm_pDDSPrimary->GetSurfaceDesc(&sm_ddsdPrimary)))
    {
        LogFail(_T("%s: GetSurfaceDesc on Primary surface failed. hr:%s"), ErrorName[hr]);
        goto Exit;
    }

    // Get the main window's position
    RECT rc;
    if(!GetWindowRect(sm_hwndMain, &rc))
    {
        LogFail(_T("GetWindowRect failed. GetLastError:%d"), GetLastError());
        goto Exit;
    }
    
    sm_dwWidth = rc.right - rc.left;
    sm_dwHeight = rc.bottom - rc.top;

    // Create a clipper
    if (FAILED(hr = sm_pDD->CreateClipper(0, &pDDC, NULL)) || NULL == pDDC)
    {
        LogFail(_T("Create clipper failed. hr:%s"), ErrorName[hr]);
        goto Exit;
    }
    LogVerbose(_T("Clipper created"));
    // Configure the clipper
    if (FAILED(hr = pDDC->SetHWnd(0, sm_hwndMain)))
    {
        LogFail(_T("SetHWnd failed. hr:%s"), ErrorName[hr]);
        goto Exit;
    }

    // Attach the clipper to the primary
    if (FAILED(hr = sm_pDDSPrimary->SetClipper(pDDC)))
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
        CleanupStaticComponents();
    }

    return fReturnValue;
}

BOOL DDrawStressThread::InitComponent()
{
    BOOL fReturnValue = FALSE;
    HRESULT hr;

    CleanupComponent();
    
    // Create an offscreen surface
    DDSURFACEDESC ddsd;
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_HEIGHT | DDSD_WIDTH;
    ddsd.dwHeight = sm_dwHeight;
    ddsd.dwWidth = sm_dwWidth;
    if (FAILED(hr = sm_pDD->CreateSurface(&ddsd, &m_pDDSOffScreen, NULL)))
    {
        LogFail(_T("Creation of offscreen surface failed, test cannot continue. hr:%s"), ErrorName[hr]);
        goto Exit;
    }
    LogVerbose(_T("Offscreen surface created"));
    
    memset(&m_ddsdOffScreen, 0, sizeof(m_ddsdOffScreen));
    m_ddsdOffScreen.dwSize=sizeof(m_ddsdOffScreen);
    if (FAILED(hr = m_pDDSOffScreen->GetSurfaceDesc(&m_ddsdOffScreen)))
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
        hr = m_pDDSOffScreen->Blt(NULL, m_pDDSOffScreen, NULL, DDBLT_WAITNOTBUSY | DDBLT_COLORFILL, &ddbltfx);
        if(SUCCEEDED(hr))
            break;
        if(hr != DDERR_SURFACEBUSY || i == TRIES)
        {
            LogFail(_T("Color-Filling Blt to offscreen surface failed. hr:%s"), ErrorName[hr]);
            fReturnValue = FALSE;
            break;
        }
    }
        
    LogVerbose(_T("Offscreen filled with a random color."));
    fReturnValue = TRUE;
    
Exit:

    // if any of the calls failed, then cleanup the compenent and exit.
    if (!fReturnValue)
    {
        CleanupComponent();
    }

    return fReturnValue;

}
//******************************************************************************
HFONT DDrawStressThread::CreateSimpleFont(LPCTSTR pszTypeface, LONG lSize)
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
        StringCchCopy(lf.lfFaceName, _countof(lf.lfFaceName), pszTypeface);

    return CreateFontIndirect(&lf);
}

UINT DDrawStressThread::EnterCleanupPen()
{
    CleanupComponent();

    if (DDRAWSTRESS_THREADCOUNT - 1 == InterlockedIncrement(&sm_lWaitingThreadCount))
    {
        if (!SetEvent(sm_EventAllThreadsAreWaiting))
        {
            LogAbort(_T("SetEvent for AllThreadsAreWaiting failed, GLE = %d"), GetLastError());
            return CESTRESS_ABORT;    
        }
    }
    DWORD dwWFSOResult = WaitForSingleObject(sm_EventThreadsPleaseContinue, DDRAWSTRESS_MAXTIMEOUT);
    if (WAIT_TIMEOUT == dwWFSOResult)
    {
        if (0 == sm_lThreadsCleanedUp)
        {
            LogAbort(_T("WaitForSingleObject for ThreadsPleaseContinue timed out!"));
            DDRAWSTRESS_PAUSE;
            return CESTRESS_ABORT;
        }
        else
        {
            LogWarn2(_T("Some threads cleaned up before we could initialize all"));
            return CESTRESS_WARN2;
        }
    }
    if (0 == InterlockedDecrement(&sm_lWaitingThreadCount))
    {
        SetEvent(sm_EventThreadsDoneWaiting);
    }
    if (!InitComponent())
    {
        return CESTRESS_ABORT;
    }
    return CESTRESS_PASS;
}

UINT DDrawStressThread::ReinitializeAll()
{
    DWORD dwWaitResult;

    dwWaitResult = WaitForSingleObject(sm_MutexReinitializeAll, 0);
    if (WAIT_TIMEOUT == dwWaitResult)
    {
        // Someone else is doing this. We need to go and enter the CleanupPen
        return CESTRESS_PASS;
    }

    CleanupComponent();

    ResetEvent(sm_EventThreadsPleaseContinue);

    if (1 != InterlockedIncrement(&sm_fCleanupTime))
    {
        LogAbort(_T("CleanupTime had unexpected value after increment! (%d)"), sm_fCleanupTime);
        return CESTRESS_ABORT;
    }

    dwWaitResult = WaitForSingleObject(sm_EventAllThreadsAreWaiting, DDRAWSTRESS_MAXTIMEOUT);
    if (WAIT_TIMEOUT == dwWaitResult)
    {
        if (0 == sm_lThreadsCleanedUp)
        {
            LogAbort(_T("ReinitializeAll timed out waiting for other threads to enter CleanupPen!"));
            DDRAWSTRESS_PAUSE;
            return CESTRESS_ABORT;
        }
        else
        {
            LogWarn2(_T("Some threads cleaned up before we could initialize all"));
            return CESTRESS_WARN2;
        }
    }

    if (!InitStaticComponents())
    {
        LogAbort(_T("Could not reinitialize static components!"));
        return CESTRESS_ABORT;
    }

    InterlockedDecrement(&sm_fCleanupTime);

    SetEvent(sm_EventThreadsPleaseContinue);

    dwWaitResult = WaitForSingleObject(sm_EventThreadsDoneWaiting, DDRAWSTRESS_MAXTIMEOUT);
    if (WAIT_TIMEOUT == dwWaitResult)
    {
        LogAbort(_T("ReinitializeAll timed out waiting for other threads to leave CleanupPen!"));
        DDRAWSTRESS_PAUSE;
        return CESTRESS_ABORT;
    }

    if (!InitComponent())
    {
        return CESTRESS_ABORT;
    }
    
    ReleaseMutex(sm_MutexReinitializeAll);
    return CESTRESS_PASS;
}

//******************************************************************************
UINT DDrawStressThread::RunTestIteration(void)
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

    RECT rc;
    RECT rcClient;
    POINT ptClientOrigin = {0, 0};

    if (sm_fCleanupTime)
    {
        return EnterCleanupPen();
    }

    // Grab the primary window's position on screen
    if(!GetWindowRect(sm_hwndMain, &rc))
    {
        LogFail(_T("Failed to retrieve window rectangle for this iteration. GetLastError:%d"), GetLastError());
        Result = CESTRESS_FAIL;
        goto Exit;
    }

    // Grab the rect defining the client area relative to the screen
    if (!GetClientRect(sm_hwndMain, &rcClient))
    {
        LogFail(_T("Failed to retrieve client rectangle for this iteration. GetLastError:%d"), GetLastError());
        Result = CESTRESS_FAIL;
        goto Exit;
    }
    if (!ClientToScreen(sm_hwndMain, &ptClientOrigin))
    {
        LogFail(_T("Failed to retrieve client to screen translation for this iteration. GetLastError:%d"), GetLastError());
        Result = CESTRESS_FAIL;
        goto Exit;
    }
    OffsetRect(&rcClient, ptClientOrigin.x, ptClientOrigin.y);
            
    // Pick one of the test scenarios
    switch(rand() % 11)
    {
        case 0:
        {
            // Simple color-filling Blt to the primary
            LogVerbose(_T("Color-filling primary surface."));
            for(int i = 0; i <= TRIES; i++)
            {   
                hr = sm_pDDSPrimary->Blt(NULL, NULL, NULL, DDBLT_WAITNOTBUSY | DDBLT_COLORFILL, &ddbltfx);
                if(SUCCEEDED(hr))
                    break;
                if(hr == DDERR_SURFACELOST)
                {
                    sm_pDDSPrimary->Restore();
                    continue;
                }
                if(hr != DDERR_SURFACEBUSY || i == TRIES)
                {
                    LogFail(_T("Color-Filling Blt to Primary surface failed. hr:%s"), ErrorName[hr]);
                    Result = CESTRESS_FAIL;
                    break;
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
                hr = m_pDDSOffScreen->Blt(NULL, NULL, NULL, DDBLT_WAITNOTBUSY | DDBLT_COLORFILL, &ddbltfx);
                if(SUCCEEDED(hr))
                    break;
                if(hr != DDERR_SURFACEBUSY || i == TRIES)
                {
                    LogFail(_T("Color-Filling Blt to offscreen surface failed. hr:%s"), ErrorName[hr]);
                    Result = CESTRESS_FAIL;
                    break;
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
                hr = sm_pDDSPrimary->GetDC(&hdc);
                if(SUCCEEDED(hr))
                    break;
                
                if(hr == DDERR_SURFACELOST)
                {
                    sm_pDDSPrimary->Restore();
                    continue;
                }
                if((hr == DDERR_SURFACEBUSY || hr == DDERR_CANTCREATEDC || hr == DDERR_SURFACELOST) && i == TRIES)
                {
                    LogWarn2(_T("Unable retrieve the DC for the primary after %d tries."), TRIES);
                    Result = CESTRESS_WARN2;
                }
                else if(FAILED(hr) && !(hr == DDERR_SURFACEBUSY || hr == DDERR_CANTCREATEDC))
                {
                    LogFail(_T("GetDC on the primary failed hr:%s"), ErrorName[hr]);
                    Result = CESTRESS_FAIL;
                    break;
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
                if(FAILED(sm_pDDSPrimary->ReleaseDC(hdc)))
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
                hr = m_pDDSOffScreen->GetDC(&hdc);
                if(SUCCEEDED(hr))
                    break;
                if((hr == DDERR_SURFACEBUSY || hr == DDERR_CANTCREATEDC || hr == DDERR_DCALREADYCREATED) && i == TRIES)
                {
                    LogWarn2(_T("Unable retrieve the DC for the offscreen after %d tries."), TRIES);
                    Result = CESTRESS_WARN2;
                }
                else if(FAILED(hr) && !(hr == DDERR_SURFACEBUSY || hr == DDERR_CANTCREATEDC || hr == DDERR_DCALREADYCREATED))
                {
                    LogFail(_T("GetDC on the offscreen failed hr:%s"), ErrorName[hr]);
                    Result = CESTRESS_FAIL;
                    break;
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
                    if(!ExtTextOut(hdc, 0, sm_dwHeight/2, 0, NULL, szMessage, countof(szMessage)-1, NULL))
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
                if(FAILED(m_pDDSOffScreen->ReleaseDC(hdc)))
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
                hr = sm_pDDSPrimary->Blt(&rc, m_pDDSOffScreen, NULL, DDBLT_WAITNOTBUSY, NULL);
                if(SUCCEEDED(hr))
                    break;
                if (DDERR_SURFACELOST == hr)
                {
                    sm_pDDSPrimary->Restore();
                    continue;
                }
                if(hr != DDERR_SURFACEBUSY || i == TRIES)
                {
                    LogFail(_T("Blt from offscreen to primary Failed. hr:%s"), ErrorName[hr]);
                    Result = CESTRESS_FAIL;
                    break;
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
            GenRandRect(sm_dwWidth, sm_dwHeight, &rcSrc);

            // Offscreen to primary blt with stretching, shrinking and random locations.
            for(int i = 0; i <= TRIES; i++)
            {
                hr = sm_pDDSPrimary->Blt(&rcDst, m_pDDSOffScreen, &rcSrc, DDBLT_WAITNOTBUSY, NULL);
                if(SUCCEEDED(hr))
                    break;
                if(hr == DDERR_SURFACELOST)
                {
                    sm_pDDSPrimary->Restore();
                    continue;
                }
                if(hr != DDERR_SURFACEBUSY || i == TRIES)
                {
                    LogFail(_T("Offscreen to primary with random source/dest failed. hr:%s"), ErrorName[hr]);
                    Result = CESTRESS_FAIL;
                    break;
                }
            }
        }
        break;
        
        case 6:
        {
            // lock and random fill

            LogVerbose(_T("Locking the primary and filling with random data."));

            DDSURFACEDESC ddsdPrimary;
            RECT rcLoc=rcClient;

            ddsdPrimary.dwSize = sizeof(ddsdPrimary);

            for(int i = 0; i <= TRIES; i++)
            {
                hr = sm_pDDSPrimary->Lock(NULL, &ddsdPrimary, NULL, NULL);
                if(SUCCEEDED(hr))
                    break;
                if (DDERR_SURFACELOST == hr)
                {
                    sm_pDDSPrimary->Restore();
                    continue;
                }
                if((hr == DDERR_SURFACEBUSY || hr == DDERR_WASSTILLDRAWING) && i == TRIES)
                {
                    LogWarn2(_T("Unable to lock primary after %d tries."), TRIES);
                    Result = CESTRESS_WARN2;
                }
                else if(FAILED(hr) && !(hr == DDERR_SURFACEBUSY || hr == DDERR_WASSTILLDRAWING))
                {
                    LogFail(_T("Locking the primary failed hr:%s"), ErrorName[hr]);
                    Result = CESTRESS_FAIL;
                    break;
                }
                else Sleep(100);
            }

            if(SUCCEEDED(hr))
            {
                double dwPPW = 32./ddsdPrimary.ddpfPixelFormat.dwRGBBitCount;
                DWORD dwCbPixel = (ddsdPrimary.ddpfPixelFormat.dwRGBBitCount + 7) / 8;
                BYTE * bdest = (BYTE*)ddsdPrimary.lpSurface;

                // set the starting x location based on the window position.  if the left edge is off the screen, then x is 0.
                if(rcLoc.left < 0)
                    rcLoc.left=0;

                // handle the cases where the rest of the surface goes off the screen.
                if(rcLoc.top < 0)
                    rcLoc.top=0;
                if(rcLoc.bottom>(int)ddsdPrimary.dwHeight)
                    rcLoc.bottom=ddsdPrimary.dwHeight;
                if(rcLoc.right > (int)ddsdPrimary.dwWidth)
                    rcLoc.right = ddsdPrimary.dwWidth;
                    

                for (int y = rcLoc.top; y < rcLoc.bottom; y++)
                {
                    DWORD dwColor;
                    if (4 == dwCbPixel)
                    {
                        dwColor = ((rand() & 0xFF) << 24) | ((rand() & 0xFF) << 16) | ((rand() & 0xFF) << 8) | (rand() & 0xFF);
                    }
                    else if (3 == dwCbPixel)
                    {
                        dwColor = ((rand() & 0xFF) << 16) | ((rand() & 0xFF) << 8) | (rand() & 0xFF);
                    }
                    else if (2 == dwCbPixel)
                    {
                        *((WORD*)&dwColor) = ((rand() & 0xFF) << 8) | (rand() & 0xFF);
                    }
                    else
                    {
                        *((BYTE*)&dwColor) = rand() & 0xFF;
                        dwCbPixel = 1;
                    }

                    for (int x = rcLoc.left; x < rcLoc.right; ++x)
                    {
                        memcpy((bdest + ((ddsdPrimary.lPitch) * y + (ddsdPrimary.lXPitch) * x)), &dwColor, dwCbPixel);
                    }
                }

                if(FAILED(hr = sm_pDDSPrimary->Unlock(NULL)))
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

            ddsd.dwSize = sizeof(ddsd);

            for(int i = 0; i <= TRIES; i++)
            {
                hr = m_pDDSOffScreen->Lock(NULL, &ddsd, NULL, NULL);

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
                    break;
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

                if(FAILED(hr = m_pDDSOffScreen->Unlock(NULL)))
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
            if(sm_ddDriverCaps.dwCKeyCaps & DDCKEYCAPS_SRCBLT)   
            {
                // source colorkeying to the primary or the offscreen
                DWORD dwColor1=rand()%0xFF << 24 | rand()%0xFF << 16 | rand()%0xFF << 8 | rand()%0xFF,
                      dwColor2=rand()%0xFF << 24 | rand()%0xFF << 16 | rand()%0xFF << 8 | rand()%0xFF,
                      dwColor3=rand()%0xFF << 24 | rand()%0xFF << 16 | rand()%0xFF << 8 | rand()%0xFF,
                      dwColor4=rand()%0xFF << 24 | rand()%0xFF << 16 | rand()%0xFF << 8 | rand()%0xFF;
                // fill in the primary surface with data, horizontal/vertical, whatever...
                if(rand() %2)
                    fTmp = FillSurfaceHorizontal(sm_pDDSPrimary, dwColor1, dwColor2, &sm_rcWindow);
                else
                    fTmp = FillSurfaceVertical(sm_pDDSPrimary, dwColor1, dwColor2, &sm_rcWindow);
                if(!fTmp)
                {
                    LogFail(_T("Fill surface on primary failed"));
                    Result = CESTRESS_FAIL;
                }
                // fill up the offscreen with some data.
                if(rand() %2)
                    fTmp = FillSurfaceHorizontal(m_pDDSOffScreen, dwColor3, dwColor4, &sm_rcWindow);
                else
                    fTmp = FillSurfaceVertical(m_pDDSOffScreen, dwColor3, dwColor4, &sm_rcWindow);
                
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
                    if(FAILED(hr=m_pDDSOffScreen->SetColorKey(DDCKEY_SRCBLT, &ddck)))
                    {
                        LogFail(_T("Unable to set color key on offscreen surface. hr:%s"), ErrorName[hr]);
                        Result = CESTRESS_FAIL;
                    }
                    
                    for(int i = 0; i <= TRIES; i++)
                    {
                        hr=sm_pDDSPrimary->Blt(NULL, m_pDDSOffScreen, NULL, DDBLT_WAITNOTBUSY | DDBLT_KEYSRC, NULL);
                        if(SUCCEEDED(hr))
                            break;
                        if(hr != DDERR_SURFACEBUSY || i == TRIES)
                        {
                            LogFail(_T("Blt to primary surface failed. hr:%s"), ErrorName[hr]);
                            Result = CESTRESS_FAIL;
                            break;
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
                    if(rcSrc.right > (int)sm_ddsdPrimary.dwWidth)
                        rcSrc.right=sm_ddsdPrimary.dwWidth;
                    if(rcSrc.top < 0)
                        rcSrc.top=0;
                    if(rcSrc.bottom > (int)sm_ddsdPrimary.dwHeight)
                        rcSrc.bottom=sm_ddsdPrimary.dwHeight;

                    RECT rcDst={0, 0, rcSrc.right-rcSrc.left, rcSrc.bottom-rcSrc.top};
                    
                    // set the source colorkey, one of the colors we filled the primary with up above.
                    ddck.dwColorSpaceLowValue=ddck.dwColorSpaceHighValue=(rand()%2) ? dwColor1 : dwColor2;
                    if(FAILED(hr=sm_pDDSPrimary->SetColorKey(DDCKEY_SRCBLT, &ddck)))
                    {
                        LogFail(_T("Unable to set color key on primary surface. hr:%s"), ErrorName[hr]);
                        Result = CESTRESS_FAIL;
                    }

                    for(int i = 0; i <= TRIES; i++)
                    {
                        hr=m_pDDSOffScreen->Blt(&rcDst, sm_pDDSPrimary, &rcSrc, DDBLT_WAITNOTBUSY | DDBLT_KEYSRC, NULL);
                        if(SUCCEEDED(hr))
                            break;
                        if(hr == DDERR_SURFACELOST)
                        {
                            sm_pDDSPrimary->Restore();
                        }
                        if(hr != DDERR_SURFACEBUSY || i == TRIES)
                        {
                            LogFail(_T("Blt to offscreen surface failed. hr:%s"), ErrorName[hr]);
                            Result = CESTRESS_FAIL;
                            break;
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
        case 10:
        {
            LogVerbose(_T("Reinitializing everything."));
            Result = ReinitializeAll();
        }
        break;
    }
Exit:
    return Result;
}

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

/////////////////////////////////////////////////////////////////////////////////////
BOOL InitializeStressModule (
                            /*[in]*/ MODULE_PARAMS* pmp, 
                            /*[out]*/ UINT* pnThreads
                            )
{
    LogVerbose(_T("Initializing the stress module."));
    
    TCHAR tsz[MAX_PATH] = {0};
    BOOL fReturnVal = FALSE;
    
    *pnThreads = DDRAWSTRESS_THREADCOUNT;
    
    if (!InitializeStressUtils(
        _T("S2_DDRAW"),                                    // Module name to be used in logging
        LOGZONE(SLOG_SPACE_DX, SLOG_DDRAW),    // set to log to the ddraw logging zone.
        pmp))
    {
        return FALSE;
    }
    srand(GetTickCount());

// DDraw does support more than one instance, so we don't need this.
//    SetLastError(0);
//    g_MutexRunOneInstance = CreateMutex(NULL, TRUE, DDRAWSTRESS_MUTEXNAME);
//    if (ERROR_ALREADY_EXISTS == GetLastError())
//    {
//        return FALSE;
//    }
    
    if(DetectComponent())
    {
        if(InitApplication(g_hInst))
        {
            if(DDrawStressThread::InitInstance(g_hInst, g_szClassName, SW_SHOW))
            {
                if(DDrawStressThread::InitStaticComponents())
                {
                    fReturnVal = TRUE;
                }
                else
                {
                    // if init component failed (shouldn't because detect component says it's there
                    // cleanup the instance and application and exit.
                    DDrawStressThread::CleanupInstance();
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

UINT WINAPI InitializeTestThread(HANDLE, DWORD, int index)
{
    UINT nResult = CESTRESS_PASS;
    DDrawStressThread * pThread = new DDrawStressThread();
    if (NULL == pThread)
    {
        LogAbort(_T("Could not allocate thread context structure. Thread: %d"), index);
        return CESTRESS_ABORT;
    }

    // Just GetTickCount is not guaranteed to be unique each time InitializeStressThread is called.
    // Because of this, add in a value based on the thread index. Overflow in this case doesn't matter.
    // Add 1 because the initialization thread was already initialized.
    srand(GetTickCount() + 5000 * (index + 1));

//    if (!pThread->InitInstance(SW_SHOW))
//    {
//        LogAbort(_T("InitInstance failed. Thread: %d"), index);
//        nResult = CESTRESS_ABORT;
//        goto cleanup;
//    }

    if (!pThread->InitComponent())
    {
        LogAbort(_T("InitComponent failed. Thread: %d"), index);
        nResult = CESTRESS_ABORT;
        goto cleanup;
    }
    
    g_ThreadContexts[index] = pThread;

cleanup:
    if (CESTRESS_ABORT == nResult)
    {
        delete pThread;
        pThread = NULL;
    }
    return nResult;
}

/////////////////////////////////////////////////////////////////////////////////////
UINT DoStressIteration (
                        /*[in]*/ HANDLE hThread, 
                        /*[in]*/ DWORD dwThreadId, 
                        /*[in,out]*/ ITERATION_INFO * pIterationInfo)
{
    LogVerbose(_T("Doing a stress iteration."));
    if (NULL == g_ThreadContexts[pIterationInfo->index])
    {
        LogAbort(_T("Thread context not initialized. Thread %d"), pIterationInfo->index);
        return CESTRESS_ABORT;
    }
        
    return g_ThreadContexts[pIterationInfo->index]->RunTestIteration();
}


UINT WINAPI CleanupTestThread(HANDLE, DWORD, int index)
{
    if (NULL != g_ThreadContexts[index])
    {
        g_ThreadContexts[index]->CleanupComponent();
        g_ThreadContexts[index]->CleanupThread();
        g_ThreadContexts[index] = NULL;
    }
    Sleep(100);
    return CESTRESS_PASS;
}

/////////////////////////////////////////////////////////////////////////////////////
DWORD TerminateStressModule (void)
{
    LogVerbose(_T("Stress module terminated, cleaning up."));
    DDrawStressThread::CleanupStaticComponents();
    DDrawStressThread::CleanupInstance();
    CleanupApplication();
    if (NULL != g_MutexRunOneInstance)
    {
        CloseHandle(g_MutexRunOneInstance);
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
    g_hInst = (HINSTANCE) hInstance;
    return TRUE;
}
