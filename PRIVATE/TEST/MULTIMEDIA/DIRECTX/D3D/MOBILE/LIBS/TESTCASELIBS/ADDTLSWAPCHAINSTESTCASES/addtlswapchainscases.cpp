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
#include "AddtlSwapChainsCases.h"
#include "QAD3DMX.h"
#include <SurfaceTools.h>
#include <BufferTools.h>
#include "DebugOutput.h"

extern HANDLE g_hInstance;

#define SafeRelease(x) if(x){(x)->Release(); (x) = NULL;}

namespace AddtlSwapChainsNamespace
{
    DWORD Colors[] = {
        0xFFFF0000, // default
        0xFF00FF00, // swap1
        0xFF0000FF, // swap2
        0xFFFFFF00, // swap3
        0xFFFF00FF, // swap3
        0xFF00FFFF, // swap4
        0xFF000000, // swap5
        0xFFFFFFFF, // swap6
    };
    
    D3DM_ADDTLSWAPCHAINS Geometry[VERTCOUNT] = {
        POSX1, POSY1, POSZ1, 1.0f, 0xFFFFFFF,
        POSX2, POSY2, POSZ2, 1.0f, 0xFFFFFFF,
        POSX3, POSY3, POSZ3, 1.0f, 0xFFFFFFF,
        POSX4, POSY4, POSZ4, 1.0f, 0xFFFFFFF
    };

    HRESULT PrepareMainHWnd(
        LPDIRECT3DMOBILEDEVICE  pDevice,
        HWND                    hWnd)
    {
        HRESULT hr;
        RECT ClientRect;
        // Determine current client rect
        if (!GetClientRect(hWnd, &ClientRect))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            DebugOut(DO_ERROR, L"Could not get client rect of main window. (hr = 0x%08x)", hr);
            return hr;
        }
        
        // Use SetWindowLong to clear styles (i.e. borders etc.)
        SetLastError(0);
        if (!SetWindowLong(hWnd, GWL_STYLE, 0) && GetLastError())
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            DebugOut(DO_ERROR, L"Could not clear out styles of main window. (hr = 0x%08x)", hr);
            return hr;
        }
        
        // Set the window size to be the same as the original client rect
        if (!SetWindowPos(hWnd, NULL, 0, 0, ClientRect.right - ClientRect.left, ClientRect.bottom - ClientRect.top, SWP_NOMOVE | SWP_NOZORDER))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            DebugOut(DO_ERROR, L"Could not set new window size. (hr = 0x%08x)", hr);
            return hr;
        }

        // since the window is recreated for each iteration, there is no need to save the current
        // size and location for undoing this.
        
        ShowWindow( hWnd,         // HWND hWnd, 
                          SW_SHOWNORMAL); // int nCmdShow 
        
        UpdateWindow( hWnd );
        return S_OK;
    }

    HRESULT LocateNewWindow(
        HWND OriginalWindow,
        const ESize NewWindowSize,
        const DWORD NewWindowLocation,
        RECT * pNewWindowRect)
    {
        RECT WindowRect;
        RECT TempRect;
        RECT Intersection;
        HRESULT hr;
        int InflationX;
        int InflationY;
        int OffsetX;
        int OffsetY;

        if (!GetWindowRect(OriginalWindow, &WindowRect))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            DebugOut(DO_ERROR, L"GetWindowRect failed. (hr = 0x%08x)", hr);
            return hr;
        }

        InflationX = (WindowRect.right - WindowRect.left) / 5;
        InflationY = (WindowRect.bottom - WindowRect.top) / 5;

        switch (NewWindowSize)
        {
        case Same:
            // The rect is the right size, don't do anything
            break;
        case Smaller:
            // The rect needs to be shrinked
            InflateRect(&WindowRect, -InflationX, -InflationY);
            break;
        case Larger:
            // The rect needs to be enlarged
            InflateRect(&WindowRect, InflationX, InflationY);
            break;
        }

        OffsetX = (WindowRect.right - WindowRect.left) / 2;
        OffsetY = (WindowRect.bottom - WindowRect.top) / 2;
        if (OffsetX > WindowRect.left)
        {
            OffsetX = WindowRect.left;
        }
        if (OffsetY > WindowRect.top)
        {
            OffsetY = WindowRect.top;
        }

        switch (NewWindowLocation)
        {
        case ASCLOC_ECLIPSE:
            // The two windows are aligned, so don't need to do anything here.
            break;
        case ASCLOC_UPPERLEFT:
            OffsetRect(&WindowRect, -OffsetX, -OffsetY);
            break;
        case ASCLOC_UPPERRIGHT:
            OffsetRect(&WindowRect, OffsetX, -OffsetY);
            break;
        case ASCLOC_LOWERLEFT:
            OffsetRect(&WindowRect, -OffsetX, OffsetY);
            break;
        case ASCLOC_LOWERRIGHT:
            OffsetRect(&WindowRect, OffsetX, OffsetY);
            break;
        case ASCLOC_NOOVERLAP:
            // Create a copy of the WindowRect
            CopyRect(&TempRect, &WindowRect);
            // Shift the new rect all the way to the left side of the screen
            OffsetRect(&TempRect, -WindowRect.left, 0);
            // Figure out how much the two rects intersect
            IntersectRect(&Intersection, &TempRect, &WindowRect);
            // Adjust the new rect by the difference, but only if the adjustment does not completely obscure the window
            if (TempRect.right != Intersection.right - Intersection.left)
            {
                TempRect.right -= Intersection.right - Intersection.left;
            }
            CopyRect(&WindowRect, &TempRect);
            break;
        }

        CopyRect(pNewWindowRect, &WindowRect);
        
        return S_OK;
    }

    HRESULT OpenWindow(
        RECT * pWindowRect,
        HWND * pWindow)
    {
        static bool fClassRegistered = false;
        WNDCLASS wc;
        TCHAR tszClassName[] = _T("AddtlSwapChains");
        HRESULT hr;

        if (!pWindow)
        {
            DebugOut(DO_ERROR, L"OpenWindow called with NULL pointer.");
            return E_POINTER;
        }
        
        wc.style = 0;
        wc.lpfnWndProc = (WNDPROC) DefWindowProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = (HINSTANCE) g_hInstance;
        wc.hIcon = 0;
        wc.hCursor = 0;
        wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
        wc.lpszMenuName = 0;
        wc.lpszClassName = tszClassName;

        if (!fClassRegistered)
        {
            RegisterClass(&wc);
            fClassRegistered = true;
        }

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
            (HINSTANCE) g_hInstance,
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

    HRESULT CreateSwapChains(
        LPDIRECT3DMOBILEDEVICE        pDevice, 
        HWND                          hWnd,
        DWORD                         dwTableIndex,
        LPDIRECT3DMOBILESWAPCHAIN   **prgpSwapChains,
        HWND                        **ppSwapWindows)
    {
        HRESULT hr;
        RECT DeviceClientRect;
        RECT NewWindowRect;
        HWND TempWindow;
        D3DMPRESENT_PARAMETERS PresentParams;
        const int SwapChainCount = AddtlSwapChainsCases[dwTableIndex].AddtlSwapChainsCount;
        const int DevZOrder = AddtlSwapChainsCases[dwTableIndex].DevZOrder;
        if (!prgpSwapChains || !ppSwapWindows)
        {
            DebugOut(DO_ERROR, L"CreateSwapChains called with NULL pointers.");
            return E_POINTER;
        }

        *prgpSwapChains = NULL;
        *ppSwapWindows = NULL;
        
        // Create an array for the windows
        *ppSwapWindows = new HWND[SwapChainCount];
        if (!*ppSwapWindows)
        {
            DebugOut(DO_ERROR, L"Out of memory allocating swap chain window array (with %d entries).", SwapChainCount);
            return E_OUTOFMEMORY;
        }
        memset(*ppSwapWindows, 0x00, SwapChainCount * sizeof(HWND));
        
        // Create all the windows in the proper z-order and locations
        // Determine current window location
        if (!GetClientRect(hWnd, &DeviceClientRect))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            DebugOut(DO_ERROR, L"Could not retrieve client rect of main window. (hr = 0x%08x)", hr);
            return hr;
        }
        int i;
        for (i = 0; i < SwapChainCount; ++i)
        {
            // For each new window, take its location and calculate position
            hr = LocateNewWindow(
                hWnd,
                AddtlSwapChainsCases[dwTableIndex].Size,
                ASCUNLOC(i+1, AddtlSwapChainsCases[dwTableIndex].Locations),
                &NewWindowRect);
            if (FAILED(hr))
            {
                DebugOut(DO_ERROR, L"Unable to find suitable location for window. (hr = 0x%08x)", hr);
                return hr;
            }
            // Create the window
            hr = OpenWindow(
                &NewWindowRect,
                &TempWindow);
            if (FAILED(hr))
            {
                DebugOut(DO_ERROR, L"Unable to create window. (hr = 0x%08x)", hr);
                return hr;
            }
            
            // Put it in the array
            (*ppSwapWindows)[i] = TempWindow;
        }

        // Once all are created, the main hWnd is at the bottom.
        // Call SetWindowPos to place the main hWnd correctly.
        TempWindow = HWND_TOP;
        if (DevZOrder > 0)
        {
            int InsertAfterIndex = SwapChainCount - DevZOrder;
            if (InsertAfterIndex < 0)
            {
                InsertAfterIndex = 0;
            }
            TempWindow = (*ppSwapWindows)[InsertAfterIndex];
        }
        BOOL Ret = SetWindowPos(
            hWnd,
            TempWindow,
            0, 0, 0, 0,
            SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOREPOSITION | SWP_NOSIZE);
        if (!Ret)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            DebugOut(DO_ERROR, L"Could not reposition main window. (hr = 0x%08x)", hr);
            return hr;
        }

        // Now create swap chains for all the windows.
        // Create an array for the swap chain pointers
        *prgpSwapChains = new LPDIRECT3DMOBILESWAPCHAIN[SwapChainCount];
        if (!*prgpSwapChains)
        {
            DebugOut(DO_ERROR, L"Out of memory allocating swap chain object array (with %d entries).", SwapChainCount);
            return E_OUTOFMEMORY;
        }
        memset(*prgpSwapChains, 0x00, SwapChainCount * sizeof(LPDIRECT3DMOBILESWAPCHAIN));
        
        // Set up the presentation parameters
        D3DMDISPLAYMODE DeviceMode;
        memset(&DeviceMode, 0x00, sizeof(DeviceMode));
        hr = pDevice->GetDisplayMode(&DeviceMode);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"GetDisplayMode failed. (hr = 0x%08x)", hr);
            return hr;
        }
        
        memset(&PresentParams, 0x00, sizeof(PresentParams));
        PresentParams.Windowed   = TRUE;
        PresentParams.SwapEffect = D3DMSWAPEFFECT_DISCARD;
        PresentParams.BackBufferFormat = DeviceMode.Format;
        PresentParams.BackBufferCount = 1;
        PresentParams.BackBufferWidth = NewWindowRect.right - NewWindowRect.left; 
        PresentParams.BackBufferHeight = NewWindowRect.bottom - NewWindowRect.top;
        PresentParams.MultiSampleType = D3DMMULTISAMPLE_NONE;
        // Create each swap chain
        for (i = 0; i < SwapChainCount; ++i)
        {
            LPDIRECT3DMOBILESWAPCHAIN pTempChain = NULL;
            hr = pDevice->CreateAdditionalSwapChain(
                &PresentParams,
                &pTempChain);
            if (FAILED(hr))
            {
                DebugOut(DO_ERROR, L"Unable to create additional swap chain. (hr = 0x%08x)", hr);
                if (D3DMERR_MEMORYPOOLEMPTY == hr)
                {
                    return S_FALSE;
                }
                return hr;
            }

            (*prgpSwapChains)[i] = pTempChain;
        }

        return S_OK;
    }

    HRESULT DestroySwapChains(
        DWORD                       dwTableIndex,
        LPDIRECT3DMOBILESWAPCHAIN * rgpSwapChains,
        HWND                      * pSwapWindows)
    {
        const int SwapChainCount = AddtlSwapChainsCases[dwTableIndex].AddtlSwapChainsCount;
        // Free all the swap chains
        if (rgpSwapChains)
        {
            for (int i = 0; i < SwapChainCount; ++i)
            {
                SafeRelease(rgpSwapChains[i]);
            }
            delete[] rgpSwapChains;
        }
        // Free the swap chain array
        if (pSwapWindows)
        {
            // Destroy all the windows
            for (int i = 0; i < SwapChainCount; ++i)
            {
                if (pSwapWindows[i])
                {
                    DestroyWindow(pSwapWindows[i]);
                    pSwapWindows[i] = NULL;
                }
            }
            // Free the window array
            delete[] pSwapWindows;
        }
        return S_OK;
    }
    
    HRESULT CreateAndPrepareVertexBuffers(
        LPDIRECT3DMOBILEDEVICE        pDevice, 
        HWND                          hWnd,
        DWORD                         dwTableIndex, 
        LPDIRECT3DMOBILEVERTEXBUFFER *ppVertexBuffer,
        UINT                         *pVBStride)
    {
        HRESULT hr = S_OK;
        UINT uiShift;
    	UINT uiSX;
    	UINT uiSY;
    	//
    	// Window client extents
    	//
    	RECT ClientExtents;
    	//
    	// Retrieve device capabilities
    	//
    	if (0 == GetClientRect( hWnd,            // HWND hWnd, 
    	                       &ClientExtents)) // LPRECT lpRect 
    	{
    		hr = HRESULT_FROM_WIN32(GetLastError());
    		DebugOut(DO_ERROR, _T("GetClientRect failed. (hr = 0x%08x) "), hr);
    		goto cleanup;
    	}

        //
        // Reset the inputs
        //
        *ppVertexBuffer = NULL;
    	
    	//
    	// Scale factors
    	//
    	uiSX = ClientExtents.right;
    	uiSY = ClientExtents.bottom;

        //////////////////////////////////////////////////////////////////////
        // Create the test primitives vertex buffer. These will be used as the
        // biased primitives. As the bias changes, the visibility of these will
        // vary.
        //

    	//
    	// Scale to window size
    	//
    	if (D3DMFVF_XYZRHW_FLOAT & D3DM_ADDTLSWAPCHAINS_FVF)
    	{
    		for (uiShift = 0; uiShift < VERTCOUNT; uiShift++)
    		{
    			Geometry[uiShift].x *= (float)(uiSX - 1);
    			Geometry[uiShift].y *= (float)(uiSY - 1);
    		}
    	}

    	hr = CreateFilledVertexBuffer(
    	    pDevice,                           // LPDIRECT3DMOBILEDEVICE pDevice,
    	    ppVertexBuffer,                              // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
    	    (BYTE*)Geometry,                                    // BYTE *pVertices,
    	    sizeof(D3DM_ADDTLSWAPCHAINS),   // UINT uiVertexSize,
    	    VERTCOUNT,  // UINT uiNumVertices,
    	    D3DM_ADDTLSWAPCHAINS_FVF);      // DWORD dwFVF

    	if (FAILED(hr))
    	{
    		DebugOut(DO_ERROR, _T("CreateActiveBuffer failed. (hr = 0x%08x) "), hr);
            hr = S_FALSE;
            goto cleanup;
    	}
    	
    	//
    	// "Unscale" from window size
    	//
    	if (D3DMFVF_XYZRHW_FLOAT & D3DM_ADDTLSWAPCHAINS_FVF)
    	{
    		for (uiShift = 0; uiShift < VERTCOUNT; uiShift++)
    		{
    			Geometry[uiShift].x /= (float)(uiSX - 1);
    			Geometry[uiShift].y /= (float)(uiSY - 1);
    		}
    	}
    cleanup:
        return hr;
    }

    HRESULT PrepareDeviceWithSwapChain(
        LPDIRECT3DMOBILEDEVICE pDevice,
        LPDIRECT3DMOBILESWAPCHAIN * rgpSwapChains,
        LPDIRECT3DMOBILEVERTEXBUFFER pVertexBuffer,
        DWORD CurrentSwapChain)
    {
        // Determine if we want the device or if we want an addtl swap chain
        LPDIRECT3DMOBILESURFACE pBackBuffer = NULL;
        LPDIRECT3DMOBILESURFACE pZStencil = NULL;
        D3DM_ADDTLSWAPCHAINS * pVertices = NULL;
        HRESULT hr;
        // Device is 0
        if (0 == CurrentSwapChain)
        {
            // Get backbuffer of pDevice
            hr = pDevice->GetBackBuffer(0, D3DMBACKBUFFER_TYPE_MONO, &pBackBuffer);
            if (FAILED(hr))
            {
                DebugOut(DO_ERROR, L"GetBackBuffer of main device failed. (hr = 0x%08x)", hr);
                goto cleanup;
            }
        }
        else
        {
            // if not device, subtract 1 from swap chain to get index into array
            int SwapIndex = CurrentSwapChain - 1;
            // Get backbuffer of current swap chain
            hr = rgpSwapChains[SwapIndex]->GetBackBuffer(0, D3DMBACKBUFFER_TYPE_MONO, &pBackBuffer);
            if (FAILED(hr))
            {
                DebugOut(DO_ERROR, L"GetBackBuffer of swap chain %d failed. (hr = 0x%08x)", SwapIndex, hr);
                goto cleanup;
            }
        }

        // Set the render target (don't specify depth surface, since we don't want to have to create
        // a new depth surface if the backbuffer size grows).
        hr = pDevice->SetRenderTarget(pBackBuffer, NULL);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetRenderTarget failed. (hr = 0x%08x)", hr);
            goto cleanup;
        }
        
        // lock the Vertex Buffer
        hr = pVertexBuffer->Lock(0, 0, (void**)&pVertices, 0);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"Could not lock vertex buffer to modify diffuse. (hr = 0x%08x)", hr);
            goto cleanup;
        }

        // Change the colors of all the vertices.
        for (int i = 0; i < VERTCOUNT; ++i)
        {
            pVertices[i].diffuse = Colors[CurrentSwapChain];
        }
        
        hr = pVertexBuffer->Unlock();
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"Could not unlock vertex buffer after modifying diffuse. (hr = 0x%08x)", hr);
            goto cleanup;
        }
        
cleanup:
        SafeRelease(pBackBuffer);
        SafeRelease(pZStencil);
        return hr;
    }


    HRESULT SetupTextureStages(
        LPDIRECT3DMOBILEDEVICE pDevice, 
        DWORD                  dwTableIndex)
    {
        //
        // Give args a default state
        //
        pDevice->SetTextureStageState( 0, D3DMTSS_COLORARG1, D3DMTA_TEXTURE );
        pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAARG1, D3DMTA_TEXTURE );
        pDevice->SetTextureStageState( 1, D3DMTSS_COLORARG1, D3DMTA_TEXTURE );
        pDevice->SetTextureStageState( 1, D3DMTSS_ALPHAARG1, D3DMTA_TEXTURE );
        
        pDevice->SetTextureStageState( 2, D3DMTSS_COLORARG1, D3DMTA_TEXTURE );
        pDevice->SetTextureStageState( 2, D3DMTSS_ALPHAARG1, D3DMTA_TEXTURE );
        pDevice->SetTextureStageState( 3, D3DMTSS_COLORARG1, D3DMTA_TEXTURE );
        pDevice->SetTextureStageState( 3, D3DMTSS_ALPHAARG1, D3DMTA_TEXTURE );
        
        pDevice->SetTextureStageState( 0, D3DMTSS_COLORARG2, D3DMTA_CURRENT );
        pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAARG2, D3DMTA_CURRENT );
        pDevice->SetTextureStageState( 1, D3DMTSS_COLORARG2, D3DMTA_CURRENT );
        pDevice->SetTextureStageState( 1, D3DMTSS_ALPHAARG2, D3DMTA_CURRENT );
        
        pDevice->SetTextureStageState( 2, D3DMTSS_COLORARG2, D3DMTA_CURRENT );
        pDevice->SetTextureStageState( 2, D3DMTSS_ALPHAARG2, D3DMTA_CURRENT );
        pDevice->SetTextureStageState( 3, D3DMTSS_COLORARG2, D3DMTA_CURRENT );
        pDevice->SetTextureStageState( 3, D3DMTSS_ALPHAARG2, D3DMTA_CURRENT );
        
        //
        // Disable output of texture stages
        //
        pDevice->SetTextureStageState( 0, D3DMTSS_COLOROP, D3DMTOP_DISABLE );
        pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAOP, D3DMTOP_DISABLE );
        pDevice->SetTextureStageState( 1, D3DMTSS_COLOROP, D3DMTOP_DISABLE );
        pDevice->SetTextureStageState( 1, D3DMTSS_ALPHAOP, D3DMTOP_DISABLE );
        pDevice->SetTextureStageState( 2, D3DMTSS_COLOROP, D3DMTOP_DISABLE );
        pDevice->SetTextureStageState( 2, D3DMTSS_ALPHAOP, D3DMTOP_DISABLE );
        pDevice->SetTextureStageState( 3, D3DMTSS_COLOROP, D3DMTOP_DISABLE );
        pDevice->SetTextureStageState( 3, D3DMTSS_ALPHAOP, D3DMTOP_DISABLE );

    	if (
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_COLOROP,   D3DMTOP_SELECTARG1 ))) ||
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_COLORARG1, D3DMTA_DIFFUSE     ))) ||
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_COLORARG2, D3DMTA_CURRENT     ))) ||
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAOP,   D3DMTOP_SELECTARG1 ))) ||
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAARG1, D3DMTA_DIFFUSE     ))) ||
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAARG2, D3DMTA_CURRENT     ))) 
    		)
    	{
    		DebugOut(DO_ERROR, _T("SetTextureStageState failed."));
    		return E_FAIL;
    	}

    	return S_OK;

    }

    HRESULT SetupRenderStates(
        LPDIRECT3DMOBILEDEVICE pDevice, 
        DWORD                  dwTableIndex)
    {
        HRESULT hr;
        D3DMCAPS Caps;

        pDevice->GetDeviceCaps(&Caps);

    	//
    	// Set up the shade mode
    	//
    	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_SHADEMODE, D3DMSHADE_FLAT)))
    	{
    		DebugOut(DO_ERROR, _T("SetRenderState(D3DMRS_SHADEMODE) failed. (hr = 0x%08x) "), hr);
    		return hr;
        }

        //
        // Disable depth writing
        //
        if (FAILED(hr = pDevice->SetRenderState(D3DMRS_ZWRITEENABLE, FALSE)))
    	{
    		DebugOut(DO_ERROR, _T("SetRenderState(D3DMRS_ZWRITEENABLE, FALSE) failed. (hr = 0x%08x) "), hr);
    		return hr;
        }

        return S_OK;
    }

};
