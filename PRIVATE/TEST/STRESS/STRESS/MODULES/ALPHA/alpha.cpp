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
/////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <tchar.h>
#include <stressutils.h>

/////////////////////////////////////////////////////////////////////////////////////
const int THREADCOUNT = 3;
const TCHAR gszClassName[] = TEXT("s2_alpha");

/////////////////////////////////////////////////////////////////////////////////////
static HINSTANCE g_hInst = NULL;
static DWORD g_dwTlsIndex = 0xFFFFFFFF;
static int g_nThreadCount = 0;
static HWND g_rghWnd[THREADCOUNT] = { NULL };
static int g_rgIterationCount[THREADCOUNT];

/////////////////////////////////////////////////////////////////////////////////////
void DoMsgPump(void)
{
    // Main message loop:
    MSG msg;
    for (int i=0; i<5; i++)
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
    wc.lpfnWndProc = (WNDPROC) DefWindowProc;
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
BOOL CreateAppWindow(HINSTANCE hInstance, int nThreadIndex)
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

    g_rghWnd[nThreadIndex] = CreateWindowEx(
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

    return (g_rghWnd[nThreadIndex] != NULL) ? TRUE : FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////
BOOL InitializeStressModule (
                            /*[in]*/ MODULE_PARAMS* pmp, 
                            /*[out]*/ UINT* pnThreads
                            )
{
    // Set this value to indicate the number of threads
    // that you want your module to run.  The module container
    // that loads this DLL will manage the life-time of these
    // threads.  Each thread will call your DoStressIteration()
    // function in a loop.

    *pnThreads = THREADCOUNT;
    
    // !! You must call this before using any stress utils !!


    InitializeStressUtils (
                            _T("s2_alpha"),                                        // Module name to be used in logging
                            LOGZONE(SLOG_SPACE_GDI, SLOG_DRAW | SLOG_BITMAP),    // Logging zones used by default
                            pmp                                                    // Forward the Module params passed on the cmd line
                            );

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

    srand(GetTickCount());

    if (!InitApplication(g_hInst))
    {
        LogFail(_T("Unable to initialize application"));
        return FALSE;
    }

    TCHAR tsz[MAX_PATH];

    GetModuleFileName((HINSTANCE) g_hInst, tsz, MAX_PATH);
    tsz[MAX_PATH-1] = 0;

    LogComment(_T("Module File Name: %s"), tsz);

    // Open some windows to party on
    for (int i = 0; i < THREADCOUNT; i++)
    {
        CreateAppWindow(g_hInst, i);
    }
    
    return TRUE;
}


/////////////////////////////////////////////////////////////////////////////////////
UINT InitializeTestThread (
                        /*[in]*/ HANDLE hThread, 
                        /*[in]*/ DWORD dwThreadId, 
                        /*[in]*/ int index)
{
    LogVerbose(_T("InitializeTestThread(), thread handle = 0x%x, Id = %d, index %i"), 
                    hThread, 
                    dwThreadId, 
                    index 
                    );

    srand(GetTickCount() * (index + 1));
    
    return CESTRESS_PASS;
}

/////////////////////////////////////////////////////////////////////////////////////
HBITMAP CreateAlphaDIBSection(HDC hdc, VOID **ppvBits, int nHeight, int nWidth, int nDepth)
{
    HBITMAP          hBmp = NULL;
    BITMAPINFOHEADER temp;

    // 16 & 32 bit:  for Alpha Blending
    struct {
        BITMAPINFOHEADER  bmih;
        DWORD             ct[4];  // 4 entries
    } bmi;

    temp.biSize            = sizeof(BITMAPINFOHEADER);
    temp.biWidth        = nWidth;
    temp.biHeight        = nHeight;
    temp.biPlanes        = 1;
    temp.biBitCount        = nDepth;
    temp.biCompression    = BI_ALPHABITFIELDS;  // ALPHA
    temp.biSizeImage    = 0;
    temp.biXPelsPerMeter= 0;
    temp.biYPelsPerMeter= 0;
    temp.biClrUsed        = 4;  // ALPHA
    temp.biClrImportant    = 0;

    bmi.bmih = temp;
    if (temp.biBitCount == 16){
        // 4-4-4-4 format
        bmi.ct[0] = 0x0F00;        // RED
        bmi.ct[1] = 0x00F0;        // GREEN
        bmi.ct[2] = 0x000F;        // BLUE
        bmi.ct[3] = 0xF000;        // ALPHA
    }
    else
    {
        // 8-8-8-8 format
        bmi.ct[0] = 0x00FF0000; // RED
        bmi.ct[1] = 0x0000FF00; // GREEN
        bmi.ct[2] = 0x000000FF; // BLUE
        bmi.ct[3] = 0xFF000000; // ALPHA
    }

    SetLastError(ERROR_SUCCESS);

    hBmp = CreateDIBSection(hdc, (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, ppvBits, NULL, 0);

    // error handling in the caller.

    return hBmp;
}

/////////////////////////////////////////////////////////////////////////////////////
UINT DoStressIteration (
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

    const int rgDepth[] = { 16, 32 };
    const DWORD dwColor = RGBA(0xFF, 0x80, 0x40, 0x80);

    UINT uRet = CESTRESS_PASS;
    HWND hwndTest = NULL;
    HDC hdcWnd = NULL;
    int i, j, k;
    RECT rcBlt;

    hwndTest = g_rghWnd[rand() % THREADCOUNT];
    
    hdcWnd = GetDC(hwndTest);
    if (!hdcWnd)
    {
        LogWarn1(_T("GetDC failed: %d"), GetLastError());
        return CESTRESS_WARN1;
    }
    
    if (!GetClientRect(hwndTest, &rcBlt))
    {
        LogWarn1(_T("GetClientRect failed: %d"), GetLastError());
        return CESTRESS_WARN1;
    }
    int nDIBHeight = rcBlt.bottom - rcBlt.top,
        nDIBWidth  = rcBlt.right  - rcBlt.left;

    for (i = 0; i < (sizeof(rgDepth) / sizeof(*rgDepth)); ++i)
    {
        LPVOID pvBits = NULL;
        HBITMAP hbmAlpha = CreateAlphaDIBSection(hdcWnd, &pvBits, nDIBHeight, nDIBWidth, rgDepth[i]);
        if (NULL == hbmAlpha || NULL == pvBits)
        {
            if(GetLastError() == ERROR_NOT_ENOUGH_MEMORY)
            {
                LogWarn1(_T("GetClientRect failed: %d"), GetLastError());
                uRet = CESTRESS_WARN1;
                continue;
            }
            else
            {
                LogFail(TEXT("Unable to create DIB section"));
                uRet = CESTRESS_FAIL;
                continue;
            }
        }
        else
        {
            LogVerbose(TEXT("%d bit DIBSection successfully created"), rgDepth[i]);

            HDC hdcMem = CreateCompatibleDC(hdcWnd);
            if (NULL == hdcMem)
            {
                LogFail(TEXT("Unable to create compatible dc"));
                uRet = CESTRESS_FAIL;
            }
            else
            {
                HBITMAP hbmStock = static_cast<HBITMAP>(SelectObject(hdcMem, hbmAlpha));

                // Decorate the bitmap
                if (16 == rgDepth[i])
                {
                    WORD *pData = static_cast<WORD*>(pvBits);
                    for (j = 0; j < nDIBHeight; ++j)
                    {
                        for (k = 0; k < nDIBWidth; ++k)
                        {
                            *pData = (( j      & 0xF) << 12) |
                                     (( k      & 0xF) << 8 ) |
                                     (((j + k) & 0xF) << 4 ) |
                                     (((j - k) & 0xF));
                            pData++;
                        }
                    }
                }
                else
                {
                    DWORD *pData = static_cast<DWORD*>(pvBits);
                    for (j = 0; j < nDIBHeight; ++j)
                    {
                        for (k = 0; k < nDIBWidth; ++k)
                        {
                            *pData = (( j      & 0xFF) << 24) |
                                     (( k      & 0xFF) << 16) |
                                     (((j + k) & 0xFF) << 8 ) |
                                     (((j - k) & 0xFF));
                            pData++;
                        }
                    }
                }

                // Blt the image to the window
                BitBlt(hdcWnd, 0, 0, nDIBWidth, nDIBHeight, hdcMem, 0, 0, SRCCOPY);
                
                // Can not verfy contents, since will fail if graphics chip powered down
                if (16 == rgDepth[i])
                {
                    WORD *pData = static_cast<WORD*>(pvBits);
                    for (j = 0; j < nDIBHeight; ++j)
                    {
                        for (k = 0; k < nDIBWidth; ++k)
                        {
                            WORD Data = *pData;
                            WORD Expect = static_cast<WORD>(
                                        (( j      & 0xF) << 12) |
                                        (( k      & 0xF) << 8 ) |
                                        (((j + k) & 0xF) << 4 ) |
                                        (((j - k) & 0xF))
                                    );
                            if (Data != Expect)
                            {
                                LogFail(TEXT("Incorrect Bits found at (%d, %d).  Exp:0x%08x  Got:0x%08x"), k, j, Expect, Data);
                            }
                            pData++;
                        }
                    }
                }
                else
                {
                    DWORD *pData = static_cast<DWORD*>(pvBits);
                    for (j = 0; j < nDIBHeight; ++j)
                    {
                        for (k = 0; k < nDIBWidth; ++k)
                        {
                            DWORD Data = *pData;
                            DWORD Expect = static_cast<DWORD>(
                                        (( j      & 0xFF) << 24) |
                                        (( k      & 0xFF) << 16) |
                                        (((j + k) & 0xFF) << 8 ) |
                                        (((j - k) & 0xFF))
                                    );
                            if (Data != Expect)
                            {
                                LogFail(TEXT("Incorrect Bits found at (%d, %d).  Exp:0x%08x  Got:0x%08x"), k, j, Expect,  Data);
                            }
                            pData++;
                        }
                    }
                }

                // Copy Back
                BitBlt(hdcMem, 0, 0, nDIBWidth, nDIBHeight, hdcWnd, 0, 0, SRCCOPY);
                DoMsgPump();

                // And back again, just for fun
                BitBlt(hdcWnd, 0, 0, nDIBWidth, nDIBHeight, hdcMem, 0, 0, SRCCOPY);
                DoMsgPump();

                SelectObject(hdcMem, hbmStock);
                DeleteDC(hdcMem);
            }

            // Destroy the bitmap
            DeleteObject(hbmAlpha);
        }
        DoMsgPump();
    }

    ReleaseDC(hwndTest, hdcWnd);

    // You must return one of these values:
    //return CESTRESS_PASS;
    //return CESTRESS_FAIL;
    //return CESTRESS_WARN1;
    //return CESTRESS_WARN2;
    //return CESTRESS_ABORT;  // Use carefully.  This will cause your module to be terminated immediately.
    return uRet;
}

/////////////////////////////////////////////////////////////////////////////////////
DWORD TerminateStressModule (void)
{
    // TODO:  Do test-specific clean-up here.

    int i;
    LogVerbose(_T("Terminating Stress Module"));

    // This will be used as the process exit code.
    // It is not currently used by the harness.
    for (i = 0; i < THREADCOUNT; ++i)
    {
        BOOL fRet = DestroyWindow(g_rghWnd[i]);
        g_rghWnd[i] = NULL;

        if (0 == fRet)
        {
            LogWarn2(_T("Unexpected error code from DestroyWindow"));
        }
        else
        {
            LogVerbose(_T("Window successfully destroyed"));
        }
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
