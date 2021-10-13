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
#include "IDirect3DMobileSwapChain.h"
#include "DebugOutput.h"
#include "KatoUtils.h"
#include "BufferTools.h"
#include <tux.h>
#include <tchar.h>
#include <stdio.h>

//
// GUID for invalid parameter tests
//
DEFINE_GUID(IID_D3DQAInvalidInterface, 
0x00000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

HRESULT SwapChainTest::OpenWindow(
    RECT * pWindowRect,
    HWND * pWindow)
{
    WNDCLASS wc;
    TCHAR tszClassName[] = _T("AddtlSwapChains");
    HRESULT hr;

    if (!pWindow || !pWindowRect)
    {
        DebugOut(DO_ERROR, L"OpenWindow called with NULL pointer.");
        return E_POINTER;
    }
    
    wc.style = 0;
    wc.lpfnWndProc = (WNDPROC) DefWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = (HINSTANCE) GetModuleHandle(NULL);
    wc.hIcon = 0;
    wc.hCursor = 0;
    wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = tszClassName;

    RegisterClass(&wc);

    *pWindow = CreateWindowEx(
        WS_EX_TOPMOST,
        tszClassName,
        tszClassName,
        0,
        pWindowRect->left,
        pWindowRect->top,
        pWindowRect->right - pWindowRect->left,
        pWindowRect->bottom - pWindowRect->top,
        NULL,
        NULL,
        (HINSTANCE) GetModuleHandle(NULL),
        NULL );
    if (*pWindow == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        DebugOut(DO_ERROR, L"Could not create window. (hr = 0x%08x)", hr);
        return hr;
    }
    
    // Use SetWindowLong to clear styles (i.e. borders etc.)
    SetLastError(0);
    if (!SetWindowLong(*pWindow, GWL_STYLE, 0) && GetLastError())
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        DebugOut(DO_ERROR, L"Could not clear out styles of main window. (hr = 0x%08x)", hr);
        return hr;
    }
    ShowWindow(*pWindow, SW_SHOWNORMAL);
    UpdateWindow(*pWindow);
    return S_OK;
}

HRESULT SwapChainTest::CreateSwapChain(IDirect3DMobileSwapChain ** ppSwapChain, UINT uiBackBufferCount, D3DMPRESENT_PARAMETERS * pParams)
{
    if (!ppSwapChain || !pParams)
    {
        DebugOut(DO_ERROR, L"SwapChainTest::CreateSwapChain called with bad pointer");
        return E_POINTER;
    }
    
    // First, create window
    HRESULT hr;
    RECT rc, rcTest, rcDesktop;

    if (NULL == m_hwndSwapChain)
    {
        GetWindowRect(m_hWnd, &rcTest);
        GetWindowRect(NULL, &rcDesktop);

        OffsetRect(&rcTest, 10, 10);
        IntersectRect(&rc, &rcTest, &rcDesktop);

        hr = OpenWindow(&rc, &m_hwndSwapChain);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"Could not open window for swap chain. (hr = 0x%08x)", hr);
            return hr;
        }
    }

    GetClientRect(m_hwndSwapChain, &rc);
    
    pParams->BackBufferWidth = rc.right - rc.left;
    pParams->BackBufferHeight = rc.bottom - rc.top;
    pParams->BackBufferCount = uiBackBufferCount;

    // Third, Create the swap chain
    return m_pd3dDevice->CreateAdditionalSwapChain(pParams, ppSwapChain);
}

//
//  SwapChainTest
//
//    Constructor for SwapChainTest class; however, most initialization is deferred.
//
//  Arguments:
//
//    none
//
//  Return Value:
//
//    none
//
SwapChainTest::SwapChainTest() :
    m_bInitSuccess(FALSE),
    m_hwndSwapChain(NULL)
{
}


//
// SwapChainTest
//
//   Initialize SwapChainTest object
//
// Arguments:
//
//   UINT uiAdapterOrdinal: D3DM Adapter ordinal
//   WCHAR *pSoftwareDevice: Pointer to wide character string identifying a D3DM software device to instantiate
//   UINT uiTestCase:  Test case ordinal (to be displayed in window)
//
// Return Value
//
//   HRESULT indicates success or failure
//
HRESULT SwapChainTest::Init(LPTEST_CASE_ARGS pTestCaseArgs, LPWINDOW_ARGS pWindowArgs, BOOL bDebug, UINT uiTestCase)
{
    HRESULT hr;

    if (FAILED(hr = D3DMInitializer::Init(D3DQA_PURPOSE_RAW_TEST,                      // D3DQA_PURPOSE Purpose
                                     pWindowArgs,                                 // LPWINDOW_ARGS pWindowArgs
                                     pTestCaseArgs->pwchSoftwareDeviceFilename,   // TCHAR *ptchDriver
                                     uiTestCase)))                                // UINT uiTestCase
    {
        DebugOut(DO_ERROR, L"Aborting initialization, due to prior initialization failure. (hr = 0x%08x).", hr);
        return E_FAIL;
    }

    m_bDebugMiddleware = bDebug;

    m_bInitSuccess = TRUE;
    return S_OK;
}


//
//  IsReady
//
//    Accessor method for "initialization state" of SwapChainTest object.  "Half
//    initialized" states should not occur.
//  
//  Return Value:
//  
//    BOOL:  TRUE if the object is initialized; FALSE if it is not.
//
BOOL SwapChainTest::IsReady()
{
    if (FALSE == m_bInitSuccess)
    {
        DebugOut(DO_ERROR, L"SwapChainTest is not ready.");
        return FALSE;
    }

    DebugOut(L"SwapChainTest is ready.");
    return D3DMInitializer::IsReady();
}

// 
// ~SwapChainTest
//
//  Destructor for SwapChainTest.  Currently; there is nothing
//  to do here.
//
SwapChainTest::~SwapChainTest()
{
    if (m_hwndSwapChain)
    {
        DestroyWindow(m_hwndSwapChain);
        m_hwndSwapChain = NULL;
    }
    return; // Success
}


//
// ExecuteQueryInterfaceTest
//
//   Verify IDirect3DMobileVertexBuffer::QueryInterface
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT SwapChainTest::ExecuteQueryInterfaceTest()
{
    HRESULT hr;

    //
    // Presentation parameters
    //
    D3DMPRESENT_PARAMETERS d3dmpp;
    
    //
    // Interface pointer for swap chain to be created
    //
    IDirect3DMobileSwapChain *pSwapChain;

    //
    // Interface pointers to be retrieved by QueryInterface
    //
    IDirect3DMobileSwapChain *pQISwapChain;
    IUnknown *pQIUnknown;

    DebugOut(L"Beginning ExecuteQueryInterfaceTest.");

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Create a new swap chain
    //
    d3dmpp = m_d3dpp;
    hr = CreateSwapChain(&pSwapChain, 1, &d3dmpp);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not create swap chain for testing. (hr = 0x%08x) Aborting.", hr);
        return TPR_ABORT;
    }

    //     
    // Bad parameter tests
    //     

    //
    // Test case #1:  Invalid REFIID
    //
    if (SUCCEEDED(hr = pSwapChain->QueryInterface(IID_D3DQAInvalidInterface, // REFIID riid
                                             (void**)(&pQISwapChain)))) // void** ppvObj
    {
        DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        pQISwapChain->Release();
        pSwapChain->Release();
        return TPR_FAIL;
    }

    //
    // Test case #2:  Valid REFIID, invalid PPVOID
    //
    if (SUCCEEDED(hr = pSwapChain->QueryInterface(IID_IDirect3DMobileSwapChain, // REFIID riid
                                             (void**)0)))                     // void** ppvObj
    {
        DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        pSwapChain->Release();
        return TPR_FAIL;
    }

    //
    // Test case #3:  Invalid REFIID, invalid PPVOID
    //
    if (SUCCEEDED(hr = pSwapChain->QueryInterface(IID_D3DQAInvalidInterface, // REFIID riid
                                             (void**)0)))               // void** ppvObj
    {
        DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        pSwapChain->Release();
        return TPR_FAIL;
    }



    //
    // Valid parameter tests
    //

    //     
    // Perform an interface query on the interface buffer
    //     
    if (FAILED(hr = pSwapChain->QueryInterface(IID_IUnknown, // REFIID riid
                                          (void**)(&pQIUnknown))))    // void** ppvObj
    {
        DebugOut(DO_ERROR, L"QueryInterface failed. (hr = 0x%08x). Failing.", hr);
        pSwapChain->Release();
        return TPR_FAIL;
    }

    //     
    // Perform an interface query on the interface buffer
    //     
    if (FAILED(hr = pSwapChain->QueryInterface(IID_IDirect3DMobileSwapChain, // REFIID riid
                                            (void**)(&pQISwapChain))))       // void** ppvObj
    {
        DebugOut(DO_ERROR, L"QueryInterface failed. (hr = 0x%08x). Failing.", hr);
        pSwapChain->Release();
        return TPR_FAIL;
    }


    //
    // Release the original interface pointer; verify that the ref count is still non-zero
    //
    if (!(pSwapChain->Release()))
    {
        DebugOut(DO_ERROR, L"Reference count for IDirect3DMobileVertexBuffer dropped to zero; unexpected. Failing.");
        return TPR_FAIL;
    }

    //
    // Release the IDirect3DMobileVertexBuffer interface pointer; verify that the ref count is still non-zero
    //
    if (!(pQISwapChain->Release()))
    {
        DebugOut(DO_ERROR, L"Reference count for IDirect3DMobileVertexBuffer dropped to zero; unexpected. Failing.");
        return TPR_FAIL;
    }

    //
    // Release the IDirect3DMobileResource interface pointer; ref count should now drop to zero
    //
    if (pQIUnknown->Release())
    {
        DebugOut(DO_ERROR, L"Reference count for IUnknown is non-zero; unexpected. Failing.");
        return TPR_FAIL;
    }
    
    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;
}

//
// ExecuteAddRefTest
//
//   Verify IDirect3DMobileVertexBuffer::AddRef
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT SwapChainTest::ExecuteAddRefTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteAddRefTest.");

    //
    // Presentation parameters
    //
    D3DMPRESENT_PARAMETERS d3dmpp;
    
    //
    // Interface pointer for vertex buffer to be created
    //
    IDirect3DMobileSwapChain *pSwapChain;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Create a new vertex buffer
    //
    d3dmpp = m_d3dpp;
    hr = CreateSwapChain(&pSwapChain, 1, &d3dmpp);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not create test swap chain. (hr = 0x%08x) Aborting.", hr);
        return TPR_ABORT;
    }
    
    //
    // Verify ref-count values; via AddRef/Release return values
    //
    if (2 != pSwapChain->AddRef())
    {
        DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer: unexpected reference count. Failing.");
        return TPR_FAIL;
    }
    
    if (1 != pSwapChain->Release())
    {
        DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer: unexpected reference count. Failing.");
        return TPR_FAIL;
    }
    
    if (0 != pSwapChain->Release())
    {
        DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer: unexpected reference count. Failing.");
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;
}
//
// ExecuteReleaseTest
//
//   Verify IDirect3DMobileVertexBuffer::Release
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT SwapChainTest::ExecuteReleaseTest()
{
    DebugOut(L"Beginning ExecuteReleaseTest.");

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Testing for this method occurs in a closely-related test:
    //
    return ExecuteAddRefTest();
}

//
// Verify IDirect3DMobileVertexBuffer::Release
//
INT SwapChainTest::ExecuteGetBackBufferTest()
{
    //
    // Result for any calls
    //
    HRESULT hr;

    //
    // The number of expected backbuffers
    //
    UINT uiBackBufferCount;

    //
    // The number of backbuffers specified in the call to CreateAdditionalSwapChain
    // (If 0, uiBackBufferCount will be set to 1, since D3DM always creates at least one)
    //
    UINT uiBackBufferCreation;

    //
    // The swap chain that we'll use for testing
    //
    IDirect3DMobileSwapChain * pSwapChain = NULL;

    D3DMCAPS Caps;
    m_pd3dDevice->GetDeviceCaps(&Caps);
    const UINT uiMAXBACKBUFFERCOUNT = 10;
    const UINT uiDRIVERBACKBUFFERCOUNT = Caps.MaxBackBuffer > uiMAXBACKBUFFERCOUNT ? uiMAXBACKBUFFERCOUNT : Caps.MaxBackBuffer;
    
    //
    // These will be used for reference count checking and to verify that different calls return
    // the same thing.
    //
    IDirect3DMobileSurface * rgpBackBuffers[uiMAXBACKBUFFERCOUNT] = {NULL};

    //
    // A temporary backbuffer pointer
    //
    IDirect3DMobileSurface * pBackBuffer = NULL;

    //
    // The parameters we'll be using
    //
    D3DMPRESENT_PARAMETERS SwapChainPresentParams = {0};
    INT Result = TPR_PASS;

    UINT i;

    
    //
    // Cycle through tests with between 0 and 3 or 4
    //

    for (uiBackBufferCreation = 0; uiBackBufferCreation <= uiDRIVERBACKBUFFERCOUNT; ++uiBackBufferCreation)
    {
        DebugOut(L"GetBackBufferTest with %d backbuffers specified on creation", uiBackBufferCreation);
        if (0 == uiBackBufferCreation)
        {
            // if 0 is passed in to CreateAddtlSwapChain, D3DM changes this to 1.
            uiBackBufferCount = 1;
        }
        else
        {
            uiBackBufferCount = uiBackBufferCreation;
        }
        //
        // Create the swap chain
        //
        SwapChainPresentParams = m_d3dpp;
        hr = CreateSwapChain(&pSwapChain, uiBackBufferCreation, &SwapChainPresentParams);
        if (E_OUTOFMEMORY == hr)
        {
            DebugOut(DO_ERROR,
                L"Driver out of memory creating swap chain with %d backbuffers. (hr = 0x%08x) Continuing.",
                uiBackBufferCreation,
                hr);
            continue;
        }
        else if (FAILED(hr))
        {
            DebugOut(DO_ERROR, 
                L"Unable to create swap chain with %d backbuffers.  (hr = 0x%08x) Aborting.",
                uiBackBufferCreation,
                hr);
            return TPR_ABORT;
        }

        //     
        // Bad parameter tests
        //     

        //
        // Test case #1:  Invalid backbuffer numbers (backbuffercount, -1, 10 random numbers)
        //
        for (i = 0; i < 12; ++i)
        {
            UINT uiDesiredBB;
            do
            {
                uiDesiredBB = rand() | (rand() << 15) | (rand() << 30);
            } while(uiDesiredBB < uiBackBufferCount);
            if (0 == i)
            {
                uiDesiredBB = uiBackBufferCount;
            }
            else if (1 == i)
            {
                uiDesiredBB = -1;
            }

            hr = pSwapChain->GetBackBuffer(uiDesiredBB, D3DMBACKBUFFER_TYPE_MONO, &pBackBuffer);
            if (SUCCEEDED(hr))
            {
                DebugOut(DO_ERROR,
                    L"GetBackBuffer with desired back buffer %d succeeded on swap chain with only %d backbuffers. Failing.",
                    uiDesiredBB,
                    uiBackBufferCount);
                if (Result < TPR_FAIL)
                {
                    Result = TPR_FAIL;
                }
                pBackBuffer->Release();
                pBackBuffer = NULL;
            }
        }
        
        //
        // Test case #2:  Invalid backbuffer types (FORCE_DWORD)
        //
        hr = pSwapChain->GetBackBuffer(0, D3DMBACKBUFFER_TYPE_FORCE_DWORD, &pBackBuffer);
        if (SUCCEEDED(hr))
        {
            DebugOut(DO_ERROR,
                L"GetBackBuffer with specified Type of D3DMBACKBUFFER_TYPE_FORCE_DWORD succeeded. Failing.");
            if (Result < TPR_FAIL)
            {
                Result = TPR_FAIL;
            }
            pBackBuffer->Release();
            pBackBuffer = NULL;
        }

        //
        // Test case #3:  null pointer
        //
        hr = pSwapChain->GetBackBuffer(0, D3DMBACKBUFFER_TYPE_MONO, NULL);
        if (SUCCEEDED(hr))
        {
            DebugOut(DO_ERROR,
                L"GetBackBuffer with NULL back buffer pointer succeeded. Failing.");
            if (Result < TPR_FAIL)
            {
                Result = TPR_FAIL;
            }
        }

        //
        // Test case #4:  Bad backbuffer and type
        //
        hr = pSwapChain->GetBackBuffer(uiBackBufferCount, D3DMBACKBUFFER_TYPE_FORCE_DWORD, &pBackBuffer);
        if (SUCCEEDED(hr))
        {
            DebugOut(DO_ERROR,
                L"GetBackBuffer with bad back buffer index and Type succeeded. Failing.");
            if (Result < TPR_FAIL)
            {
                Result = TPR_FAIL;
            }
            pBackBuffer->Release();
            pBackBuffer = NULL;
        }

        //
        // Valid test cases
        //

        //
        // Test case #1:  Get each of the backbuffers, verify they are valid surfaces.
        // Store in array for later use.
        //
        for (i = 0; i < uiBackBufferCount; ++i)
        {
            D3DMSURFACE_DESC bbDesc;
            hr = pSwapChain->GetBackBuffer(i, D3DMBACKBUFFER_TYPE_MONO, rgpBackBuffers + i);
            if (FAILED(hr))
            {
                DebugOut(DO_ERROR,
                    L"GetBackBuffer failed with valid index (%d). (hr = 0x%08x) Failing.",
                    i,
                    hr);
                if (Result < TPR_FAIL)
                {
                    Result = TPR_FAIL;
                }
                continue;
            }
            hr = rgpBackBuffers[i]->GetDesc(&bbDesc);
            if (FAILED(hr))
            {
                DebugOut(DO_ERROR,
                    L"Unable to get surface description for backbuffer %d. (hr = 0x%08x) Failing.",
                    i,
                    hr);
                if (Result < TPR_FAIL)
                {
                    Result = TPR_FAIL;
                }
                continue;
            }
            if (bbDesc.Format != SwapChainPresentParams.BackBufferFormat ||
                bbDesc.Width != SwapChainPresentParams.BackBufferWidth ||
                bbDesc.Height != SwapChainPresentParams.BackBufferHeight)
            {
                DebugOut(L"Surface description for backbuffer %d doesn't match Present Parameters used to create swap chain.", i);
                DebugOut(DO_ERROR,
                    L"(Format,Width,Height): PresentParams: (0x%08x, %d, %d); SurfaceDesc: (0x%08x, %d, %d)",
                    bbDesc.Format, 
                    bbDesc.Width, 
                    bbDesc.Height,
                    SwapChainPresentParams.BackBufferFormat,
                    SwapChainPresentParams.BackBufferWidth,
                    SwapChainPresentParams.BackBufferHeight);
                if (Result < TPR_FAIL)
                {
                    Result = TPR_FAIL;
                }
            }
        }

        //
        // Test case #2:  Get backbuffers a second time, make sure they are the same object.
        // 
        for (i = 0; i < uiBackBufferCount; ++i)
        {
            hr = pSwapChain->GetBackBuffer(i, D3DMBACKBUFFER_TYPE_MONO, &pBackBuffer);
            if (FAILED(hr))
            {
                DebugOut(DO_ERROR,
                    L"GetBackBuffer failed with valid index (%d). (hr = 0x%08x) Failing.",
                    i,
                    hr);
                if (Result < TPR_FAIL)
                {
                    Result = TPR_FAIL;
                }
                continue;
            }
            // Need to compare these surfaces to previous surfaces
            if (pBackBuffer != rgpBackBuffers[i])
            {
                DebugOut(DO_ERROR,
                    L"Backbuffer returned by second call to GetBackBuffer is different from first one returned. (%p versus %p) Failing.",
                    pBackBuffer,
                    rgpBackBuffers[i]);
                if (Result < TPR_FAIL)
                {
                    Result = TPR_FAIL;
                }
            }
            if (0 == pBackBuffer->Release())
            {
                DebugOut(DO_ERROR, L"Backbuffer released has 0 ref count, should be at least 1. Failing");
                if (Result < TPR_FAIL)
                {
                    Result = TPR_FAIL;
                }
            }
            pBackBuffer = NULL;
        }

        //
        // Test #3:  Do some Present calls, verify that the backbuffer locations in the swap chain change.
        //
        

        for (i = 0; i < uiBackBufferCount; ++i)
        {
            if (rgpBackBuffers[i])
            {
                rgpBackBuffers[i]->Release();
                rgpBackBuffers[i] = NULL;
            }
        }
    }
    return Result;
}

//
// Verify IDirect3DMobileVertexBuffer::Release
//
INT SwapChainTest::ExecutePresentTest()
{
    HRESULT hr;
    INT Result = TPR_PASS;

    DebugOut(L"Beginning ExecutePresentTest.");

    //
    // Presentation parameters for device creation
    //
    D3DMPRESENT_PARAMETERS SwapChainPresentParams;
    D3DMPRESENT_PARAMETERS PresentParms;

    //
    // Display mode, used to determine a format to use
    //
    D3DMDISPLAYMODE Mode;

    //
    // Provide a valid HWND or an invalid HWND?
    //
    UINT uiWindowValidSetting;

    //
    // Window to use to obscure the Present window
    //
    HWND hwndObscure = NULL;

    //
    // The swap chain to use for testing
    //
    IDirect3DMobileSwapChain * pSwapChain = NULL;
    
    //
    // Rect to use for present region
    //
    CONST RECT rSample = {0,0,1,1};

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }
    
    SwapChainPresentParams = m_d3dpp;
    hr = CreateSwapChain(&pSwapChain, 1, &SwapChainPresentParams);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not create swap chain. (hr = 0x%08x) Aborting", hr);
        return TPR_ABORT;
    }

    //
    // Bad parameter test #1: Invalid hDestWindowOverride
    //
    if (SUCCEEDED(hr = pSwapChain->Present(NULL,               // CONST RECT* pSourceRect
                                             NULL,               // CONST RECT* pDestRect
                                             (HWND)INVALID_HANDLE_VALUE, // HWND hDestWindowOverride
                                             NULL)))             // CONST RGNDATA* pDirtyRegion
    {
        DebugOut(DO_ERROR, L"Present succeeded unexpectedly (Invalid hDestWindowOverride). (hr = 0x%08x) Failing", hr);
        if (Result < TPR_FAIL)
        {
            Result = TPR_FAIL;
        }
    }

    //
    // Bad parameter test #2: Invalid pDirtyRegion
    //
    if (SUCCEEDED(hr = pSwapChain->Present(NULL,                   // CONST RECT* pSourceRect
                                            NULL,                   // CONST RECT* pDestRect
                                            m_hwndSwapChain,        // HWND hDestWindowOverride
                                            (RGNDATA*)0xFFFFFFFF))) // CONST RGNDATA* pDirtyRegion
    {
        DebugOut(DO_ERROR, L"Present succeeded unexpectedly (Invalid RGNDATA pointer). (hr = 0x%08x) Failing", hr);
        if (Result < TPR_FAIL)
        {
            Result = TPR_FAIL;
        }
    }

    //
    // Bad parameter tests #3,#4: Non-NULL rects with Present for D3DMSWAPEFFECT_FLIP
    //
    SwapChainPresentParams.SwapEffect = D3DMSWAPEFFECT_FLIP;
    SwapChainPresentParams.MultiSampleType = D3DMMULTISAMPLE_NONE;
    pSwapChain->Release();
    pSwapChain = NULL;
    if (FAILED(hr = CreateSwapChain(&pSwapChain, SwapChainPresentParams.BackBufferCount, &SwapChainPresentParams)))
    {
        DebugOut(DO_ERROR, L"Could not create swap chain a second time. (hr = 0x%08x) Aborting", hr);
        return TPR_ABORT;
    }
    if (SUCCEEDED(hr = pSwapChain->Present(&rSample,       // CONST RECT* pSourceRect
                                           NULL,           // CONST RECT* pDestRect
                                           m_hwndSwapChain,// HWND hDestWindowOverride
                                           NULL)))         // CONST RGNDATA* pDirtyRegion
    {
        DebugOut(DO_ERROR, L"Present succeeded unexpectedly (FLIP swap effect with source rect specified). (hr = 0x%08x) Failing", hr);
        if (Result < TPR_FAIL)
        {
            Result = TPR_FAIL;
        }
    }
    if (SUCCEEDED(hr = pSwapChain->Present(NULL,                // CONST RECT* pSourceRect
                                           &rSample,            // CONST RECT* pDestRect
                                           m_hwndSwapChain,     // HWND hDestWindowOverride
                                           NULL)))              // CONST RGNDATA* pDirtyRegion
    {
        DebugOut(DO_ERROR, L"Present succeeded unexpectedly (FLIP swap effect with dest rect specified). (hr = 0x%08x) Failing", hr);
        if (Result < TPR_FAIL)
        {
            Result = TPR_FAIL;
        }
    }

    //
    // Retrieve current display mode
    //
    if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode. (hr = 0x%08x) Failing", hr);
        if (Result < TPR_FAIL)
        {
            Result = TPR_FAIL;
        }
    }

    if (FAILED(hr = pSwapChain->Present(NULL,               // CONST RECT* pSourceRect
                                        NULL,               // CONST RECT* pDestRect
                                        m_hwndSwapChain,    // HWND hDestWindowOverride
                                        NULL)))             // CONST RGNDATA* pDirtyRegion
    {
        DebugOut(DO_ERROR, L"Fatal error; Present was expected to succeed, yet it failed. (hr = 0x%08x) Failing", hr);
        if (Result < TPR_FAIL)
        {
            Result = TPR_FAIL;
        }
    }

    if (0 != pSwapChain->Release())
    {
        DebugOut(DO_ERROR, L"Fatal error; attempting to release device to recreate in fullscreen mode; release failed. Failing");
        if (Result < TPR_FAIL)
        {
            Result = TPR_FAIL;
        }
    }


    //
    // Iterate through all possible flip-modes,
    // valid HWND and invalid HWND
    //

    D3DMSWAPEFFECT SwapStyle;

    //
    // There are four valid swap effects currently:
    //
    //   * D3DMSWAPEFFECT_DISCARD           = 1,
    //   * D3DMSWAPEFFECT_FLIP              = 2,
    //   * D3DMSWAPEFFECT_COPY              = 3,
    //   * D3DMSWAPEFFECT_COPY_VSYNC        = 4,
    //
    for (SwapStyle = D3DMSWAPEFFECT_DISCARD; SwapStyle <= D3DMSWAPEFFECT_COPY_VSYNC; (*(ULONG*)(&SwapStyle))++)
    {

        //
        // The first iteration will cause a invalid HWND to be used,
        // the second iteration will cause an valid HWND to be used.
        //
        for(uiWindowValidSetting = 0; uiWindowValidSetting < 2; uiWindowValidSetting++)
        {
            DWORD dwObscureSetting;

            //
            // There are three Obscuring settings
            //    0: The Present window is unobscured
            //    1: The Present window is fully obscured
            //    2: The Present window is partially obscured
            //
            for (dwObscureSetting = 0; dwObscureSetting < 3; dwObscureSetting++)
            {
                if (dwObscureSetting)
                {
                    RECT PresentWindowRect;
                    GetWindowRect(m_hwndSwapChain, &PresentWindowRect);
                    if (NULL == hwndObscure)
                    {
                        TCHAR lpClassName[260] = {0};
                        GetClassName(m_hwndSwapChain, lpClassName, sizeof(lpClassName)/sizeof(*lpClassName));
                        hwndObscure = CreateWindow( lpClassName,
                                                    _T("Present Obscurer"),
                                                    0,                          // dwStyle
                                                    0,                          // x
                                                    0,                          // y
                                                    1,                          // nWidth
                                                    1,                          // nHeight
                                                    NULL,                       // hWndParent
                                                    NULL,                       // hMenu
                                                    GetModuleHandle(NULL),      // hInstance
                                                    NULL );                     // lpParam
                        if (NULL == hwndObscure)
                        {
                            DebugOut(DO_ERROR, L"Unable to create window for obscuring. (GLE = %d) Aborting.", GetLastError());
                            return TPR_ABORT;
                        }
                    }

                    SetWindowPos( hwndObscure, 
                                  HWND_TOPMOST, 
                                  PresentWindowRect.left,
                                  PresentWindowRect.top,
                                  (PresentWindowRect.right - PresentWindowRect.left) / dwObscureSetting,
                                  (PresentWindowRect.bottom - PresentWindowRect.top) / dwObscureSetting,
                                  SWP_SHOWWINDOW );
                    UpdateWindow( hwndObscure );
                }
                else
                {
                    if (NULL != hwndObscure)
                    {
                        ShowWindow( hwndObscure, SW_HIDE );
                    }
                }

                //
                // Clear out presentation parameters structure
                //
                ZeroMemory( &PresentParms, sizeof(PresentParms) );

                //
                // Set up the presentation parameters that are desired for this 
                // testing scenario.
                //
                PresentParms.Windowed   = TRUE;
                PresentParms.SwapEffect = SwapStyle;
                PresentParms.BackBufferFormat = Mode.Format;
                PresentParms.BackBufferCount = 1;
                PresentParms.BackBufferWidth = Mode.Width;
                PresentParms.BackBufferHeight = Mode.Height;

                //
                // Create the D3DDevice
                //
                hr = CreateSwapChain(&pSwapChain, PresentParms.BackBufferCount, &PresentParms);
                if( FAILED(hr))
                {
                    //
                    // CreateSwapChain failed. Since no window can be specified at creation time, this is expected to succeed.
                    //
                    DebugOut(DO_ERROR, L"Failed at CreateSwapChain; success expected. (hr = 0x%08x) Failing", hr);
                    if (Result < TPR_FAIL)
                    {
                        Result = TPR_FAIL;
                    }
                    continue;
                }

                if (FAILED(hr = pSwapChain->Present(NULL,   // CONST RECT* pSourceRect
                                                    NULL,   // CONST RECT* pDestRect
                                                    uiWindowValidSetting ? m_hwndSwapChain : NULL,   // HWND hDestWindowOverride
                                                    NULL))) // CONST RGNDATA* pDirtyRegion
                {
                    //
                    // Was this failure expected? A DestWindowOverride is not required.
                    //
                
                    //
                    // No, a valid HWND was provided, thus the failure is unexpected.
                    //
                    DebugOut(DO_ERROR, L"Failed at Present; success expected. (hr = 0x%08x) Failing", hr);
                    if (Result < TPR_FAIL)
                    {
                        Result = TPR_FAIL;
                    }
                }

                if (0 != pSwapChain->Release())
                {
                    DebugOut(DO_ERROR, L"Fatal error; attempting to release device to recreate in fullscreen mode; release failed. Failing.");
                    if (Result < TPR_FAIL)
                    {
                        Result = TPR_FAIL;
                    }
                }
                else
                {
                    pSwapChain = NULL;
                }
            }
        }
    }

    //
    // All failure conditions have already returned.
    //
    if (NULL != hwndObscure)
    {
        DestroyWindow(hwndObscure);
    }
    return Result;

}
