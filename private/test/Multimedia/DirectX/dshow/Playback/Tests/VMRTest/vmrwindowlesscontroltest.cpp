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
//
//  Video Mixing Renderer Interface Test: IVMRWindowlessControl interface
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <atlbase.h>
#include <tux.h>
#include "logging.h"
#include "TuxGraphTest.h"
#include "VMRTestDesc.h"
#include "TestGraph.h"
#include "VMRTest.h"
#include "VideoUtil.h"


HRESULT 
VMRWLControl_DisplayModeChanged( TestGraph* pGraph,
                                 TestState state )
{
    HRESULT hr = S_OK;
    IVMRWindowlessControl* pControl = NULL;
    IVMRFilterConfig* pConfig = NULL;
    DEVMODE devMode;
    memset( &devMode, 0, sizeof(devMode) );
    devMode.dmSize = sizeof( devMode );
    HWND    hwndApp = NULL;
    RECT rcVideoWindow;

    if ( TESTSTATE_NULL == state ) 
        return S_FALSE;

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // failing case: IVMRWindowlessControl interface is not available before configuring the VMR
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);
        
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Should fail because IVMRWindowlessControl interface is not available before configuring the VMR "), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_FAIL;
            goto cleanup;
        }
        hr = S_OK;
    }
    else
    {
        // Note: You can not change the mode after any pin has been connected and you can not change the mode 
        // from windowless or renderless back to windowed, even before any pins are connected.

        // working case: interface should be available now
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);
        CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

        hr = pGraph->FindInterfaceOnGraph(IID_IVMRFilterConfig, (void**)&pConfig);
        CHECKHR( hr, TEXT("Query IVMRFilterConfig interface...") );

        DWORD dwMode = 0;
        hr = pConfig->GetRenderingMode( &dwMode );
        CHECKHR( hr, TEXT("Get current rendering mode...") );

        if ( VMRMode_Windowed == dwMode )
        {
            // failing case: default is windowed mode.
            hr = pControl->DisplayModeChanged();
            if ( VFW_E_WRONG_STATE != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - DisplayModeChange didn't return wrong state under windowed mode. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                if ( SUCCEEDED(hr) )
                    hr = E_FAIL;
                goto cleanup;
            }
            // set the VMR into windowless mode
            hr = pConfig->SetRenderingMode( VMRMode_Windowless );  
            CHECKHR( hr, TEXT("Set windowless mode for the VMR ...") );
        }

        hwndApp = OpenAppWindow( 0, 0, 320, 240, &rcVideoWindow, pControl );
        if ( !hwndApp )
        {
            hr = E_FAIL;
            CHECKHR( hr, TEXT("Create an application window...") );
        }
        hr = pControl->SetVideoClippingWindow(hwndApp);
        CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );

        // in windowless mode
        // working case: without display change
        hr = pControl->DisplayModeChanged();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - DisplayModeChange failed under windowless mode. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            goto cleanup;
        }

        // working case: with display change
        devMode.dmFields = DM_DISPLAYORIENTATION;
        devMode.dmDisplayOrientation = DMDO_90;
        hr = ChangeDisplaySettingsEx( NULL, &devMode, NULL, 0, NULL );
        if( DISP_CHANGE_SUCCESSFUL != hr )
        {
            LOG( TEXT("%S: WARNING %d@%S - ChangeDisplaySettingsEX failed. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        }

        hr = pControl->DisplayModeChanged();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - DisplayModeChange failed under windowless mode after display changed. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            goto cleanup;
        }
        // change back to 0 degree
        devMode.dmFields = DM_DISPLAYORIENTATION;
        devMode.dmDisplayOrientation = DMDO_0;
        hr = ChangeDisplaySettingsEx( NULL, &devMode, NULL, 0, NULL );
        if( DISP_CHANGE_SUCCESSFUL != hr )
        {
            LOG( TEXT("%S: WARNING %d@%S - ChangeDisplaySettingsEX failed. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        }
    }

cleanup:
    if ( hwndApp )
        CloseAppWindow(hwndApp);

    if(pConfig)
        pConfig->Release();

    if(pControl)
        pControl->Release();

    return hr;
}

HRESULT 
VMRWLControl_GetCurrentImage( TestGraph* pGraph,
                              TestState state )
{
    HRESULT hr = S_OK;
    IVMRWindowlessControl* pControl = NULL;
    IVMRFilterConfig* pConfig = NULL;
    BYTE *lpDib = NULL;
    HWND    hwndApp = NULL;
    RECT rcVideoWindow;

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // failing case: IVMRWindowlessControl interface is not available before configuring the VMR
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Should fail because IVMRWindowlessControl interface is not available before configuring the VMR "), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_FAIL;
            goto cleanup;
        }
        hr = S_OK;
    }
    else
    {
        // working case: interface should be available now
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);
        CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

        hr = pGraph->FindInterfaceOnGraph(IID_IVMRFilterConfig, (void**)&pConfig);
        CHECKHR( hr, TEXT("Query IVMRFilterConfig interface...") );

        DWORD dwMode = 0;
        hr = pConfig->GetRenderingMode( &dwMode );
        CHECKHR( hr, TEXT("Get current rendering mode...") );

        hr = pControl->GetCurrentImage( NULL );
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetCurrentImage w/ NULL should fail"), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_FAIL;
            goto cleanup;
        }
        // reset hr
        hr = S_OK;

        if ( VMRMode_Windowed == dwMode )
        {
            // failing case: default is windowed mode.
            hr = pControl->GetCurrentImage( &lpDib );
            if ( lpDib )
                CoTaskMemFree(lpDib);
            if ( VFW_E_WRONG_STATE != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - DisplayModeChange didn't return wrong state under windowed mode. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                if ( SUCCEEDED(hr) )
                    hr = E_FAIL;
                goto cleanup;
            }
            // set the VMR into windowless mode
            hr = pConfig->SetRenderingMode( VMRMode_Windowless );  
            CHECKHR( hr, TEXT("Set windowless mode for the VMR ...") );
        }

        hwndApp = OpenAppWindow( 0, 0, 320, 240, &rcVideoWindow, pControl );
        if ( !hwndApp )
        {
            hr = E_FAIL;
            CHECKHR( hr, TEXT("Create an application window...") );
        }
        hr = pControl->SetVideoClippingWindow(hwndApp);
        CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );

        // working case: windowless mode.  Surface has to be allocated.
        if ( TESTSTATE_PRE_CONNECTION < state )
        {
            lpDib = NULL;
            hr = pControl->GetCurrentImage( &lpDib );
            if ( lpDib )
                CoTaskMemFree(lpDib);
            
            // Surface lost should not be treated as interface test failure
            if ( DDERR_SURFACELOST == hr )
            {
                LOG( TEXT("%S: WARNING %d@%S - Surface lost!"), __FUNCTION__, __LINE__, __FILE__ );
                hr = S_OK;
            }

            CHECKHR( hr, TEXT("GetCurrentImage under windowless mode ...") );
        }
    }

cleanup:
    if ( hwndApp )
        CloseAppWindow(hwndApp);
    
    if(pConfig)
        pConfig->Release();

    if(pControl)
        pControl->Release();
    
    return hr;
}

HRESULT 
VMRWLControl_GetMaxMinIdealVideoSize( TestGraph* pGraph,
                                      TestState state )
{
    HRESULT hr = S_OK;
    IVMRWindowlessControl* pControl = NULL;
    IVMRFilterConfig* pConfig = NULL;
    LONG lWidth = 0, lHeight = 0;
    HWND    hwndApp = NULL;
    RECT rcVideoWindow;

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // failing case: IVMRWindowlessControl interface is not available before configuring the VMR
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Should fail because IVMRWindowlessControl interface is not available before configuring the VMR "), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_FAIL;
            goto cleanup;
        }
        hr = S_OK;
    }
    else
    {
        // working case: interface should be available now
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);
        CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

        hr = pGraph->FindInterfaceOnGraph(IID_IVMRFilterConfig, (void**)&pConfig);
        CHECKHR( hr, TEXT("Query IVMRFilterConfig interface...") );

        DWORD dwMode = 0;
        hr = pConfig->GetRenderingMode( &dwMode );
        CHECKHR( hr, TEXT("Get current rendering mode...") );

        if ( VMRMode_Windowed == dwMode )
        {
            // failing case: default is windowed mode.
            hr = pControl->GetMaxIdealVideoSize( &lWidth, &lHeight );
            if ( VFW_E_WRONG_STATE != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetMaxIdealVideoSize didn't return wrong state under windowed mode. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                if ( SUCCEEDED(hr) )
                    hr = E_FAIL;
                goto cleanup;
            }
            hr = pControl->GetMinIdealVideoSize( &lWidth, &lHeight );
            if ( VFW_E_WRONG_STATE != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetMinIdealVideoSize didn't return wrong state under windowed mode. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                if ( SUCCEEDED(hr) )
                    hr = E_FAIL;
                goto cleanup;
            }
            
            // set the VMR into windowless mode
            hr = pConfig->SetRenderingMode( VMRMode_Windowless );  
            CHECKHR( hr, TEXT("Set windowless mode for the VMR ...") );
        }

        hwndApp = OpenAppWindow( 0, 0, 320, 240, &rcVideoWindow, pControl );
        if ( !hwndApp )
        {
            hr = E_FAIL;
            CHECKHR( hr, TEXT("Create an application window...") );
        }
        hr = pControl->SetVideoClippingWindow(hwndApp);
        CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );

        // failing case: one or both of the inputs are NULL
        hr = pControl->GetMaxIdealVideoSize( NULL, &lHeight );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetMaxIdealVideoSize with NULL didn't return E_POINTER. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            if ( SUCCEEDED(hr) )
                hr = E_FAIL;
            goto cleanup;
        }
        hr = pControl->GetMinIdealVideoSize( NULL, &lHeight );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetMinIdealVideoSize with NULL didn't return E_POINTER. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            if ( SUCCEEDED(hr) )
                hr = E_FAIL;
            goto cleanup;
        }
        hr = pControl->GetMaxIdealVideoSize( &lWidth, NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetMaxIdealVideoSize with NULL didn't return E_POINTER. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            if ( SUCCEEDED(hr) )
                hr = E_FAIL;
            goto cleanup;
        }
        hr = pControl->GetMinIdealVideoSize( &lWidth, NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetMinIdealVideoSize with NULL didn't return E_POINTER. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            if ( SUCCEEDED(hr) )
                hr = E_FAIL;
            goto cleanup;
        }
        hr = pControl->GetMaxIdealVideoSize( NULL, NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetMaxIdealVideoSize with NULL didn't return E_POINTER. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            if ( SUCCEEDED(hr) )
                hr = E_FAIL;
            goto cleanup;
        }
        hr = pControl->GetMinIdealVideoSize( NULL, NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetMinIdealVideoSize with NULL didn't return E_POINTER. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            if ( SUCCEEDED(hr) )
                hr = E_FAIL;
            goto cleanup;
        }

        // working case: windowless mode
        hr = pControl->GetMaxIdealVideoSize( &lWidth, &lHeight );
        CHECKHR( hr, TEXT("GetMaxIdealVideoSize under windowless mode ...") );
        hr = pControl->GetMinIdealVideoSize( &lWidth, &lHeight );
        CHECKHR( hr, TEXT("GetMinIdealVideoSize under windowless mode ...") );
    }

cleanup:
    if ( hwndApp )
        CloseAppWindow(hwndApp);

    if(pConfig)
        pConfig->Release();

    if(pControl)
        pControl->Release();
    
    return hr;
}

HRESULT 
VMRWLControl_GetNativeVideoSize( TestGraph* pGraph,
                                    TestState state )
{
    HRESULT hr = S_OK;
    IVMRWindowlessControl* pControl = NULL;
    IVMRFilterConfig* pConfig = NULL;
    LONG lWidth = 0, lHeight = 0;
    LONG lARWidth = 0, lARHeight = 0;
    HWND    hwndApp = NULL;
    RECT rcVideoWindow;

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // failing case: IVMRWindowlessControl interface is not available before configuring the VMR
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Should fail because IVMRWindowlessControl interface is not available before configuring the VMR "), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_FAIL;
            goto cleanup;
        }
        hr = S_OK;
    }
    else
    {
        // working case: interface should be available now
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);
        CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

        hr = pGraph->FindInterfaceOnGraph(IID_IVMRFilterConfig, (void**)&pConfig);
        CHECKHR( hr, TEXT("Query IVMRFilterConfig interface...") );

        DWORD dwMode = 0;
        hr = pConfig->GetRenderingMode( &dwMode );
        CHECKHR( hr, TEXT("Get current rendering mode...") );

        if ( VMRMode_Windowed == dwMode )
        {
            // failing case: default is windowed mode.
            hr = pControl->GetNativeVideoSize( &lWidth, &lHeight, &lARWidth, &lARHeight );
            if ( VFW_E_WRONG_STATE != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetMaxIdealVideoSize didn't return wrong state under windowed mode. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                if ( SUCCEEDED(hr) )
                    hr = E_FAIL;
                goto cleanup;
            }

            // set the VMR into windowless mode
            hr = pConfig->SetRenderingMode( VMRMode_Windowless );  
            CHECKHR( hr, TEXT("Set windowless mode for the VMR ...") );
        }

        hwndApp = OpenAppWindow( 0, 0, 320, 240, &rcVideoWindow, pControl );
        if ( !hwndApp )
        {
            hr = E_FAIL;
            CHECKHR( hr, TEXT("Create an application window...") );
        }
        hr = pControl->SetVideoClippingWindow(hwndApp);
        CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );

        // failing case: all NULL ptrs
        hr = pControl->GetNativeVideoSize( NULL, NULL, NULL, NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetMaxIdealVideoSize didn't return E_POINTER when passing all NULL ptrs. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            if ( SUCCEEDED(hr) )
                hr = E_FAIL;
            goto cleanup;
        }
        // Reset hr
        hr = S_OK;

        // working case: one or more para could be NULL and all other parameters should be reset to zero if not connected
        if ( TESTSTATE_PRE_CONNECTION == state ) 
        {
            lWidth = lHeight = lARWidth = lARHeight = 0xDEADBEEF;
            hr = pControl->GetNativeVideoSize( NULL, &lHeight, &lARWidth, &lARHeight );
            CHECKHR( hr, TEXT("GetNativeVideoSize with NULL width ptr...") );
            if ( 0 != lHeight || 0 != lARWidth || 0 != lARHeight )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetNativeVideoSize didn't reset parameters. (x %d %d %d)"), 
                            __FUNCTION__, __LINE__, __FILE__, lHeight, lARWidth, lARHeight );
                hr = E_FAIL;
                goto cleanup;
            }
            lWidth = lHeight = lARWidth = lARHeight = 0xDEADBEEF;
            hr = pControl->GetNativeVideoSize( &lWidth, &lHeight, NULL, &lARHeight );
            CHECKHR( hr, TEXT("GetNativeVideoSize with NULL ar width ptr ...") );
            if ( 0 != lWidth || 0 != lHeight || 0 != lARHeight )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetNativeVideoSize didn't reset parameters. (%d %d x %d)"), 
                            __FUNCTION__, __LINE__, __FILE__, lWidth, lHeight, lARWidth, lARHeight );
                hr = E_FAIL;
                goto cleanup;
            }
            lWidth = lHeight = lARWidth = lARHeight = 0xDEADBEEF;
            hr = pControl->GetNativeVideoSize( NULL, NULL, &lARWidth, &lARHeight );
            CHECKHR( hr, TEXT("GetNativeVideoSize with NULL width and heigth ptrs ...") );
            if ( 0 != lARWidth || 0 != lARHeight )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetNativeVideoSize didn't reset parameters. (x x %d %d)"), 
                            __FUNCTION__, __LINE__, __FILE__, lARWidth, lARHeight );
                hr = E_FAIL;
                goto cleanup;
            }
            lWidth = lHeight = lARWidth = lARHeight = 0xDEADBEEF;
            hr = pControl->GetNativeVideoSize( &lWidth, &lHeight, NULL, NULL );
            CHECKHR( hr, TEXT("GetNativeVideoSize with NULL ar width and heigth ptrs ...") );
            if ( 0 != lWidth || 0 != lHeight )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetNativeVideoSize didn't reset parameters. (%d %d x x)"), 
                            __FUNCTION__, __LINE__, __FILE__, lWidth, lHeight );
                hr = E_FAIL;
                goto cleanup;
            }
            lWidth = lHeight = lARWidth = lARHeight = 0xDEADBEEF;
            hr = pControl->GetNativeVideoSize( &lWidth, &lHeight, &lARWidth, &lARHeight );
            CHECKHR( hr, TEXT("GetNativeVideoSize under windowless mode ...") );
            if ( 0 != lWidth || 0 != lHeight || 0 != lARWidth || 0 != lARHeight )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetNativeVideoSize didn't reset parameters when not connected. (%d %d %d %d)"), 
                            __FUNCTION__, __LINE__, __FILE__, lWidth, lHeight, lARWidth, lARHeight );
                hr = E_FAIL;
                goto cleanup;
            }
        }
    }

cleanup:
    if ( hwndApp )
        CloseAppWindow(hwndApp);

    if(pConfig)
        pConfig->Release();

    if(pControl)
        pControl->Release();
    
    return hr;
}

HRESULT 
VMRWLControl_RepaintVideo( TestGraph* pGraph, 
                              TestState state )
{
    HRESULT hr = S_OK;
    IVMRWindowlessControl* pControl = NULL;
    IVMRFilterConfig* pConfig = NULL;
    HDC hdc = NULL;
    HWND    hwndApp = NULL;
    RECT rcVideoWindow;

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // failing case: IVMRWindowlessControl interface is not available before configuring the VMR
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Should fail because IVMRWindowlessControl interface is not available before configuring the VMR "), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_FAIL;
            goto cleanup;
        }
        hr = S_OK;
    }
    else if ( TESTSTATE_PRE_CONNECTION < state )
    {
        // working case: interface should be available now
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);
        CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

          hr = pGraph->FindInterfaceOnGraph(IID_IVMRFilterConfig, (void**)&pConfig);
        CHECKHR( hr, TEXT("Query IVMRFilterConfig interface...") );

        DWORD dwMode = 0;
        hr = pConfig->GetRenderingMode( &dwMode );
        CHECKHR( hr, TEXT("Get current rendering mode...") );

        if ( VMRMode_Windowed == dwMode )
        {
            // failing case: default is windowed mode.
            hr = pControl->RepaintVideo( hwndApp, hdc );
            if ( VFW_E_WRONG_STATE != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - RepaintVideo didn't return wrong state under windowed mode. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                if ( SUCCEEDED(hr) )
                    hr = E_FAIL;
                goto cleanup;
            }

            // set the VMR into windowless mode
            hr = pConfig->SetRenderingMode( VMRMode_Windowless );  
            CHECKHR( hr, TEXT("Set windowless mode for the VMR ...") );
        }

        hwndApp = OpenAppWindow( 0, 0, 320, 240, &rcVideoWindow, pControl );
        if ( !hwndApp )
        {
            hr = E_FAIL;
            CHECKHR( hr, TEXT("Create an application window...") );
        }
        hr = pControl->SetVideoClippingWindow(hwndApp);
        CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );

        hdc = GetWindowDC( hwndApp );
        if ( NULL == hdc || INVALID_HANDLE_VALUE == hdc )
        {
            LOG( TEXT("%S: ERROR %d@%S - invalid device context for the window! %08x "), 
                        __FUNCTION__, __LINE__, __FILE__, hdc );
            hr = E_FAIL;
            goto cleanup;
        }

        // working case: valid HWND and HDC
        hr = pControl->RepaintVideo( hwndApp, hdc );
        CHECKHR( hr, TEXT("RepainVideo with valid HWND and HDC under windowless mode ...") );

        // failing case: valid HWND, invalid HDC
        // VMR doesn't care HDC as long as it has the clipping window handle
        /*
        hr = pControl->RepaintVideo( hwndApp, (HDC)INVALID_HANDLE_VALUE );
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - RepainVideo with valid HWND and invalid HDC didn't return E_INVALIDARG."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            if ( SUCCEEDED(hr) )
                hr = E_FAIL;
            goto cleanup;
        }
        */

        // failing case: invalid HWND, valid HDC
        hr = pControl->RepaintVideo( (HWND)INVALID_HANDLE_VALUE, hdc );
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - RepainVideo with invalid HWND and valid HDC didn't return E_INVALIDARG."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            if ( SUCCEEDED(hr) )
                hr = E_FAIL;
            goto cleanup;
        }
        // failing case: invalid HWND, invalid HDC
        hr = pControl->RepaintVideo( (HWND)INVALID_HANDLE_VALUE, (HDC)INVALID_HANDLE_VALUE );
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - RepainVideo with invalid HWND and HDC didn't return E_INVALIDARG."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            if ( SUCCEEDED(hr) )
                hr = E_FAIL;
            goto cleanup;
        }
        // faliing case: destroyed HWND, valid HDC
        BOOL bDestroyed = DestroyWindow( hwndApp );
        hr = pControl->RepaintVideo( hwndApp, hdc );
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - RepainVideo with destroyed HWND and HDC didn't return E_INVALIDARG."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            if ( SUCCEEDED(hr) )
                hr = E_FAIL;
            goto cleanup;
        }
        // reset hr
        hr = S_OK;
    }

cleanup:
    if ( hdc )
        ReleaseDC( hwndApp, hdc );
    if ( hwndApp )
        CloseAppWindow(hwndApp);
    if(pConfig)
        pConfig->Release();
    if(pControl)
        pControl->Release();
    
    return hr;
}

HRESULT 
VMRWLControl_SetVideoClippingWindow( TestGraph* pGraph,
                                     TestState state )
{
    HRESULT hr = S_OK;
    IVMRWindowlessControl* pControl = NULL;
    IVMRFilterConfig* pConfig = NULL;
    HWND    hwndApp = NULL;
    RECT rcVideoWindow;

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // failing case: IVMRWindowlessControl interface is not available before configuring the VMR
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Should fail because IVMRWindowlessControl interface is not available before configuring the VMR "), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_FAIL;
            goto cleanup;
        }
        hr = S_OK;
    }
    else
    {
        // working case: interface should be available now
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);
        CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

        hr = pGraph->FindInterfaceOnGraph(IID_IVMRFilterConfig, (void**)&pConfig);
        CHECKHR( hr, TEXT("Query IVMRFilterConfig interface...") );

        DWORD dwMode = 0;
        hr = pConfig->GetRenderingMode( &dwMode );
        CHECKHR( hr, TEXT("Get current rendering mode...") );

        if ( VMRMode_Windowed == dwMode )
        {
            // failing case: default is windowed mode.
            hr = pControl->SetVideoClippingWindow( hwndApp );
            if ( VFW_E_WRONG_STATE != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - SetVideoClippingWindow didn't return wrong state under windowed mode. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                if ( SUCCEEDED(hr) )
                    hr = E_FAIL;
                goto cleanup;
            }

            // set the VMR into windowless mode
            hr = pConfig->SetRenderingMode( VMRMode_Windowless );  
            CHECKHR( hr, TEXT("Set windowless mode for the VMR ...") );
        }

        hwndApp = OpenAppWindow( 0, 0, 320, 240, &rcVideoWindow, pControl );
        if ( !hwndApp )
        {
            hr = E_FAIL;
            CHECKHR( hr, TEXT("Create an application window...") );
        }

        // working case: valid handle
        hr = pControl->SetVideoClippingWindow( hwndApp );
        CHECKHR( hr, TEXT("SetVideoClippingWindow with valid handle under windowless mode ...") );

        // failing case: invalid handle
        hr = pControl->SetVideoClippingWindow( (HWND)INVALID_HANDLE_VALUE );
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetVideoClippingWindow with invalid handle didn't return E_INVALIDARG."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            if ( SUCCEEDED(hr) )
                hr = E_FAIL;
            goto cleanup;
        }
        // reset hr
        hr = S_OK;
    }

cleanup:
    if ( hwndApp )
        CloseAppWindow(hwndApp);

    if(pConfig)
        pConfig->Release();

    if(pControl)
        pControl->Release();
    
    return hr;
}

HRESULT 
VMRWLControl_SetGetAspectRatioMode( TestGraph* pGraph, 
                                    TestState state )
{
    HRESULT hr = S_OK;
    IVMRWindowlessControl* pControl = NULL;
    IVMRFilterConfig* pConfig = NULL;
    DWORD dwAR = VMR_ARMODE_NONE; 
    HWND    hwndApp = NULL;
    RECT rcVideoWindow;

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // failing case: IVMRWindowlessControl interface is not available before configuring the VMR
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Should fail because IVMRWindowlessControl interface is not available before configuring the VMR "), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_FAIL;
            goto cleanup;
        }
        hr = S_OK;
    }
    else
    {
        // working case: interface should be available now
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);
        CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

        hr = pGraph->FindInterfaceOnGraph(IID_IVMRFilterConfig, (void**)&pConfig);
        CHECKHR( hr, TEXT("Query IVMRFilterConfig interface...") );

        DWORD dwMode = 0;
        hr = pConfig->GetRenderingMode( &dwMode );
        CHECKHR( hr, TEXT("Get current rendering mode...") );

        if ( VMRMode_Windowed == dwMode )
        {
            // failing case: default is windowed mode.
            hr = pControl->SetAspectRatioMode( dwAR );
            if ( VFW_E_WRONG_STATE != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - SetAspectRatioMode didn't return wrong state under windowed mode. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                if ( SUCCEEDED(hr) )
                    hr = E_FAIL;
                goto cleanup;
            }
            hr = pControl->GetAspectRatioMode( &dwAR );
            if ( VFW_E_WRONG_STATE != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetAspectRatioMode didn't return wrong state under windowed mode. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                if ( SUCCEEDED(hr) )
                    hr = E_FAIL;
                goto cleanup;
            }

            // set the VMR into windowless mode
            hr = pConfig->SetRenderingMode( VMRMode_Windowless );  
            CHECKHR( hr, TEXT("Set windowless mode for the VMR ...") );
        }

        hwndApp = OpenAppWindow( 0, 0, 320, 240, &rcVideoWindow, pControl );
        if ( !hwndApp )
        {
            hr = E_FAIL;
            CHECKHR( hr, TEXT("Create an application window...") );
        }
        hr = pControl->SetVideoClippingWindow(hwndApp);
        CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );

        // failing case: invalid mode
        dwAR = 0xDEADBEEF;
        hr = pControl->SetAspectRatioMode( dwAR );
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetAspectRatioMode w/ 0xDEADBEEF didn't return E_INVALIDARG. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            if ( SUCCEEDED(hr) )
                hr = E_FAIL;
            goto cleanup;
        }

        // working case: valid mode
        dwAR = VMR_ARMODE_NONE;
        hr = pControl->SetAspectRatioMode( dwAR );
        CHECKHR( hr, TEXT("SetAspectRatioMode with VMR_ARMODE_NONE under windowless mode ...") );
        dwAR = 0xDEADBEEF;
        hr = pControl->GetAspectRatioMode( &dwAR );
        CHECKHR( hr, TEXT("GetAspectRatioMode with VMR_ARMODE_NONE under windowless mode ...") );
        if ( VMR_ARMODE_NONE != dwAR )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetAspectRatioMode didn't return VMR_ARMODE_NONE. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, dwAR );
            hr = E_FAIL;
            goto cleanup;
        }
        // failing case: call with NULL ptr.
        hr = pControl->GetAspectRatioMode( NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetAspectRatioMode w/ NULL didn't return E_POINTER. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            if ( SUCCEEDED(hr) )
                hr = E_FAIL;
            goto cleanup;
        }

        // working case: valid mode
        dwAR = VMR_ARMODE_LETTER_BOX;
        hr = pControl->SetAspectRatioMode( dwAR );
        CHECKHR( hr, TEXT("SetAspectRatioMode with VMR_ARMODE_LETTER_BOX under windowless mode ...") );
        dwAR = 0xDEADBEEF;
        hr = pControl->GetAspectRatioMode( &dwAR );
        CHECKHR( hr, TEXT("GetAspectRatioMode with VMR_ARMODE_LETTER_BOX under windowless mode ...") );
        if ( VMR_ARMODE_LETTER_BOX != dwAR )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetAspectRatioMode didn't return VMR_ARMODE_LETTER_BOX. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, dwAR );
            hr = E_FAIL;
            goto cleanup;
        }
    }

cleanup:
    if ( hwndApp )
        CloseAppWindow(hwndApp);

    if(pConfig)
        pConfig->Release();

    if(pControl)
        pControl->Release();
    
    return hr;
}
 
HRESULT 
VMRWLControl_SetGetBorderColor( TestGraph* pGraph, 
                                TestState state )
{
    HRESULT hr = S_OK;
    IVMRWindowlessControl* pControl = NULL;
    IVMRFilterConfig* pConfig = NULL;
    COLORREF dwClr = 0; 
    HWND    hwndApp = NULL;
    RECT rcVideoWindow;

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // failing case: IVMRWindowlessControl interface is not available before configuring the VMR
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Should fail because IVMRWindowlessControl interface is not available before configuring the VMR "), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_FAIL;
            goto cleanup;
        }
        hr = S_OK;
    }
    else
    {
        // working case: interface should be available now
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);
        CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

        hr = pGraph->FindInterfaceOnGraph(IID_IVMRFilterConfig, (void**)&pConfig);
        CHECKHR( hr, TEXT("Query IVMRFilterConfig interface...") );

        DWORD dwMode = 0;
        hr = pConfig->GetRenderingMode( &dwMode );
        CHECKHR( hr, TEXT("Get current rendering mode...") );

        if ( VMRMode_Windowed == dwMode )
        {
            // failing case: default is windowed mode.
            hr = pControl->SetBorderColor( 0x00FFFFFF );
            if ( VFW_E_WRONG_STATE != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - SetBorderColor didn't return wrong state under windowed mode. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                if ( SUCCEEDED(hr) )
                    hr = E_FAIL;
                goto cleanup;
            }
            hr = pControl->GetBorderColor( &dwClr);
            if ( VFW_E_WRONG_STATE != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetBorderColor didn't return wrong state under windowed mode. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                if ( SUCCEEDED(hr) )
                    hr = E_FAIL;
                goto cleanup;
            }

            // set the VMR into windowless mode
            hr = pConfig->SetRenderingMode( VMRMode_Windowless );  
            CHECKHR( hr, TEXT("Set windowless mode for the VMR ...") );
        }

        hwndApp = OpenAppWindow( 0, 0, 320, 240, &rcVideoWindow, pControl );
        if ( !hwndApp )
        {
            hr = E_FAIL;
            CHECKHR( hr, TEXT("Create an application window...") );
        }
        hr = pControl->SetVideoClippingWindow(hwndApp);
        CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );

        // failing case: invalid mode
        dwClr = CLR_INVALID; 
        hr = pControl->SetBorderColor( dwClr );
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetBorderColor w/ CLR_INVALID didn't return E_INVALIDARG. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            if ( SUCCEEDED(hr) )
                hr = E_FAIL;
            goto cleanup;
        }

        // working case: valid mode
        dwClr = 0xDEADBF;
        hr = pControl->SetBorderColor( dwClr );
        CHECKHR( hr, TEXT("SetBorderColor under windowless mode ...") );
        dwClr = 0;
        hr = pControl->GetBorderColor( &dwClr );
        CHECKHR( hr, TEXT("GetBorderColor under windowless mode ...") );
        if ( 0xDEADBF != dwClr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetBorderColor didn't return 0xDEADBF. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, dwClr );
            hr = E_FAIL;
            goto cleanup;
        }
        // failing case: call with NULL ptr.
        hr = pControl->GetBorderColor( NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetBorderColor w/ NULL didn't return E_POINTER. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            if ( SUCCEEDED(hr) )
                hr = E_FAIL;
            goto cleanup;
        }
        // reset hr
        hr = S_OK;
    }

cleanup:
    if ( hwndApp )
        CloseAppWindow(hwndApp);

    if(pConfig)
        pConfig->Release();

    if(pControl)
        pControl->Release();
    
    return hr;
}

HRESULT 
VMRWLControl_SetGetColorKey( TestGraph* pGraph, 
                                TestState state )
{
    HRESULT hr = S_OK;
    IVMRWindowlessControl* pControl = NULL;
    IVMRFilterConfig* pConfig = NULL;
    COLORREF dwClr = 0; 
    HWND    hwndApp = NULL;
    RECT rcVideoWindow;

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // failing case: IVMRWindowlessControl interface is not available before configuring the VMR
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);;
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Should fail because IVMRWindowlessControl interface is not available before configuring the VMR "), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_FAIL;
            goto cleanup;
        }
        hr = S_OK;
    }
    else
    {
        // working case: interface should be available now
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);
        CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

        hr = pGraph->FindInterfaceOnGraph(IID_IVMRFilterConfig, (void**)&pConfig);
        CHECKHR( hr, TEXT("Query IVMRFilterConfig interface...") );

        DWORD dwMode = 0;
        hr = pConfig->GetRenderingMode( &dwMode );
        CHECKHR( hr, TEXT("Get current rendering mode...") );

        if ( VMRMode_Windowed == dwMode )
        {
            // failing case: default is windowed mode.
            hr = pControl->SetColorKey( 0x00FFFFFF );
            if ( VFW_E_WRONG_STATE != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - SetColorKey didn't return wrong state under windowed mode. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                if ( SUCCEEDED(hr) )
                    hr = E_FAIL;
                goto cleanup;
            }
            hr = pControl->GetColorKey( &dwClr);
            if ( VFW_E_WRONG_STATE != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetColorKey didn't return wrong state under windowed mode. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                if ( SUCCEEDED(hr) )
                    hr = E_FAIL;
                goto cleanup;
            }

            // set the VMR into windowless mode
            hr = pConfig->SetRenderingMode( VMRMode_Windowless );  
            CHECKHR( hr, TEXT("Set windowless mode for the VMR ...") );
        }

        hwndApp = OpenAppWindow( 0, 0, 320, 240, &rcVideoWindow, pControl );
        if ( !hwndApp )
        {
            hr = E_FAIL;
            CHECKHR( hr, TEXT("Create an application window...") );
        }
        hr = pControl->SetVideoClippingWindow(hwndApp);
        CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );

        // For CLR_INVALID, MSDN doesn't state the behavor and return code.  It seems the VMR returns OK and
        // treat CLR_INVALID (0xFFFFFFFF) just as 0x00FFFFFF

        // working case: valid mode
        dwClr = 0xDEADBF;
        hr = pControl->SetColorKey( dwClr );
        // Surface lost should not be treated as interface test failure
        if ( DDERR_SURFACELOST == hr )
        {
            LOG( TEXT("%S: WARNING %d@%S - Surface lost when SetColorKey under windowless mode!"), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = S_OK;
        }
        else
        {
            dwClr = 0;
            hr = pControl->GetColorKey( &dwClr );
            CHECKHR( hr, TEXT("GetColorKey under windowless mode ...") );
            if ( 0xDEADBF != dwClr )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetColorKey didn't return 0xDEADBF. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, dwClr );
                hr = E_FAIL;
                goto cleanup;
            }
        }

        // failing case: call with NULL ptr.
        hr = pControl->GetColorKey( NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetColorKey w/ NULL didn't return E_POINTER. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            if ( SUCCEEDED(hr) )
                hr = E_FAIL;
            goto cleanup;
        }
        // reset hr
        hr = S_OK;
    }

cleanup:
    if ( hwndApp )
        CloseAppWindow(hwndApp);

    if(pConfig)
        pConfig->Release();

    if(pControl)
        pControl->Release();
    
    return hr;
}

HRESULT 
VMRWLControl_SetGetVideoPosition( TestGraph* pGraph,
                                  TestState state )
{
    HRESULT hr = S_OK;
    IVMRWindowlessControl* pControl = NULL;
    IVMRFilterConfig* pConfig = NULL;
    TCHAR tmpMsg[128] = {0};
    RECT rectSrc, rectDest, rectWnd, rectTemp; 
    LONG lWidth = 0, lHeight = 0; 
    int screenWidth = 0, screenHeight = 0;
    HWND    hwndApp = NULL;
    RECT rcVideoWindow;

    screenWidth = GetSystemMetrics( SM_CXSCREEN );
    screenHeight = GetSystemMetrics( SM_CYSCREEN );

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // failing case: IVMRWindowlessControl interface is not available before configuring the VMR
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Should fail because IVMRWindowlessControl interface is not available before configuring the VMR "), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_FAIL;
            goto cleanup;
        }
        hr = S_OK;
    }
    else
    {
        // working case: interface should be available now
        hr = pGraph->FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&pControl);
        CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

        hr = pGraph->FindInterfaceOnGraph(IID_IVMRFilterConfig, (void**)&pConfig);
        CHECKHR( hr, TEXT("Query IVMRFilterConfig interface...") );

        DWORD dwMode = 0;
        hr = pConfig->GetRenderingMode( &dwMode );
        CHECKHR( hr, TEXT("Get current rendering mode...") );

        if ( VMRMode_Windowed == dwMode )
        {
            // failing case: default is windowed mode.
            hr = pControl->SetVideoPosition( &rectSrc, &rectDest );
            if ( VFW_E_WRONG_STATE != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - SetVideoPosition didn't return wrong state under windowed mode. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                if ( SUCCEEDED(hr) )
                    hr = E_FAIL;
                goto cleanup;
            }
            hr = pControl->GetVideoPosition( &rectSrc, &rectDest );
            if ( VFW_E_WRONG_STATE != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetVideoPosition didn't return wrong state under windowed mode. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                if ( SUCCEEDED(hr) )
                    hr = E_FAIL;
                goto cleanup;
            }

            // set the VMR into windowless mode
            hr = pConfig->SetRenderingMode( VMRMode_Windowless );  
            CHECKHR( hr, TEXT("Set windowless mode for the VMR ...") );
        }

        hwndApp = OpenAppWindow( 0, 0, 320, 240, &rcVideoWindow, pControl );
        if ( !hwndApp )
        {
            hr = E_FAIL;
            CHECKHR( hr, TEXT("Create an application window...") );
        }
        hr = pControl->SetVideoClippingWindow(hwndApp);
        CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );

        if( !GetClientRect( hwndApp, &rectWnd ) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Fail to get the client area for the window "), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_FAIL;
            goto cleanup;
        }

        // failing case: NULL ptrs
        hr = pControl->SetVideoPosition( NULL, NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetVideoPosition w/ NULL ptrs didn't return E_POINTER. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            if ( SUCCEEDED(hr) )
                hr = E_FAIL;
            goto cleanup;
        }
        // failing case: GetVideoPosition with NULL ptr.
        hr = pControl->GetVideoPosition( NULL, NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetVideoPosition w/ NULL ptrs didn't return E_POINTER. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            if ( SUCCEEDED(hr) )
                hr = E_FAIL;
            goto cleanup;
        }

        // working cases: NULL src, valid dest (0, 1x1, native size, larger than the native size )
        SetRect( &rectDest, 0, 0, 0, 0 );
        hr = pControl->SetVideoPosition( NULL, &rectDest );
        CHECKHR( hr, TEXT("SetVideoPosition w/ (NULL, {0,0,0,0} under windowless mode ...") );
        hr = pControl->GetVideoPosition( NULL, &rectTemp );
        CHECKHR( hr, TEXT("GetVideoPosition under windowless mode ...") );
        if ( rectDest.left != rectTemp.left || rectDest.top != rectTemp.top || 
            rectDest.right != rectTemp.right || rectDest.bottom != rectTemp.bottom )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetVideoPosition didn't return right rect. (%d %d %d %d)."), 
                __FUNCTION__, __LINE__, __FILE__, rectTemp.left, rectTemp.top, rectTemp.right, rectTemp.bottom );
            hr = E_FAIL;
            goto cleanup;
        }

        SetRect( &rectDest, rectWnd.left, rectWnd.top, rectWnd.left + 1, rectWnd.top + 1 );
        hr = pControl->SetVideoPosition( NULL, &rectDest );
        CHECKHR( hr, TEXT("SetVideoPosition w/ (NULL, {0,0,1,1} under windowless mode ...") );
        hr = pControl->GetVideoPosition( NULL, &rectTemp );
        CHECKHR( hr, TEXT("GetVideoPosition under windowless mode ...") );
        if ( rectDest.left != rectTemp.left || rectDest.top != rectTemp.top || 
            rectDest.right != rectTemp.right || rectDest.bottom != rectTemp.bottom )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetVideoPosition didn't return right rect. (%d %d %d %d)."), 
                __FUNCTION__, __LINE__, __FILE__, rectTemp.left, rectTemp.top, rectTemp.right, rectTemp.bottom );
            hr = E_FAIL;
            goto cleanup;
        }
    
        // the native video size can only be retrieved after the graph is built
        if ( TESTSTATE_PRE_ALLOCATION <= state )
        {
            hr = pControl->GetNativeVideoSize( &lWidth, &lHeight, NULL, NULL );
            CHECKHR( hr, TEXT("Retrieve the native video size ...") );

            //      native video size
            SetRect( &rectDest, rectWnd.left, rectWnd.top, rectWnd.left + lWidth, rectWnd.top + lHeight );
            hr = pControl->SetVideoPosition( NULL, &rectDest );
            StringCchPrintf( tmpMsg,
                             128,
                             TEXT("SetVideoPosition w/ native video size (%d %d %d %d) under windowless mode ..."), 
                             rectDest.left, rectDest.top, rectDest.right, rectDest.bottom );
            CHECKHR( hr, tmpMsg );
            hr = pControl->GetVideoPosition( NULL, &rectTemp );
            CHECKHR( hr, TEXT("GetVideoPosition under windowless mode ...") );
            if ( rectDest.left != rectTemp.left || rectDest.top != rectTemp.top || 
                rectDest.right != rectTemp.right || rectDest.bottom != rectTemp.bottom )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetVideoPosition didn't return right rect. (%d %d %d %d)."), 
                    __FUNCTION__, __LINE__, __FILE__, rectTemp.left, rectTemp.top, rectTemp.right, rectTemp.bottom );
                hr = E_FAIL;
                goto cleanup;
            }

            UINT randVal1, randVal2;
            rand_s(&randVal1);
            rand_s(&randVal2);
            //      larger than the native video size
            SetRect( &rectDest, rectWnd.left, rectWnd.top, rectWnd.left + lWidth + randVal1 % 100, rectWnd.top + lHeight + randVal2 % 100 );
            hr = pControl->SetVideoPosition( NULL, &rectDest );
            StringCchPrintf( tmpMsg,
                             128,
                             TEXT("SetVideoPosition w/ larger video size (%d %d %d %d) under windowless mode ..."), 
                             rectDest.left, rectDest.top, rectDest.right, rectDest.bottom );
            CHECKHR( hr, tmpMsg );
            hr = pControl->GetVideoPosition( NULL, &rectTemp );
            CHECKHR( hr, TEXT("GetVideoPosition under windowless mode ...") );
            if ( rectDest.left != rectTemp.left || rectDest.top != rectTemp.top || 
                rectDest.right != rectTemp.right || rectDest.bottom != rectTemp.bottom )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetVideoPosition didn't return right rect. (%d %d %d %d)."), 
                    __FUNCTION__, __LINE__, __FILE__, rectTemp.left, rectTemp.top, rectTemp.right, rectTemp.bottom );
                hr = E_FAIL;
                goto cleanup;
            }

            rand_s(&randVal1);
            rand_s(&randVal2);
            //      smaller than the native video size
            SetRect( &rectDest, rectWnd.left, rectWnd.top, rectWnd.left + randVal1 % lWidth, rectWnd.top + randVal2 % lHeight );
            hr = pControl->SetVideoPosition( NULL, &rectDest );
            StringCchPrintf( tmpMsg,
                             128,
                             TEXT("SetVideoPosition w/ smaller video size (%d %d %d %d) under windowless mode ..."), 
                             rectDest.left, rectDest.top, rectDest.right, rectDest.bottom );
            CHECKHR( hr, tmpMsg );
            hr = pControl->GetVideoPosition( NULL, &rectTemp );
            CHECKHR( hr, TEXT("GetVideoPosition under windowless mode ...") );
            if ( rectDest.left != rectTemp.left || rectDest.top != rectTemp.top || 
                rectDest.right != rectTemp.right || rectDest.bottom != rectTemp.bottom )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetVideoPosition didn't return right rect. (%d %d %d %d)."), 
                    __FUNCTION__, __LINE__, __FILE__, rectTemp.left, rectTemp.top, rectTemp.right, rectTemp.bottom );
                hr = E_FAIL;
                goto cleanup;
            }        

            //      rect off screen
            SetRect( &rectDest, -1 * lWidth, 0, 0, lHeight );
            hr = pControl->SetVideoPosition( NULL, &rectDest );
            StringCchPrintf( tmpMsg,
                             128,
                             TEXT("SetVideoPosition w/ smaller video size (%i %i %i %i) under windowless mode ..."), 
                             rectDest.left, rectDest.top, rectDest.right, rectDest.bottom );
            CHECKHR( hr, tmpMsg );
            hr = pControl->GetVideoPosition( NULL, &rectTemp );
            CHECKHR( hr, TEXT("GetVideoPosition under windowless mode ...") );
            if ( rectDest.left != rectTemp.left || rectDest.top != rectTemp.top || 
                rectDest.right != rectTemp.right || rectDest.bottom != rectTemp.bottom )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetVideoPosition didn't return right rect. (%d %d %d %d)."), 
                    __FUNCTION__, __LINE__, __FILE__, rectTemp.left, rectTemp.top, rectTemp.right, rectTemp.bottom );
                hr = E_FAIL;
                goto cleanup;
            }

            //      top of the rect off screen
            SetRect( &rectDest, 0, -1 * lHeight / 2, lWidth, lHeight/2 );
            hr = pControl->SetVideoPosition( NULL, &rectDest );
            StringCchPrintf( tmpMsg,
                             128,
                             TEXT("SetVideoPosition w/ smaller video size (%i %i %i %i) under windowless mode ..."), 
                             rectDest.left, rectDest.top, rectDest.right, rectDest.bottom );
            CHECKHR( hr, tmpMsg );
            hr = pControl->GetVideoPosition( NULL, &rectTemp );
            CHECKHR( hr, TEXT("GetVideoPosition under windowless mode ...") );
            if ( rectDest.left != rectTemp.left || rectDest.top != rectTemp.top || 
                rectDest.right != rectTemp.right || rectDest.bottom != rectTemp.bottom )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetVideoPosition didn't return right rect. (%d %d %d %d)."), 
                    __FUNCTION__, __LINE__, __FILE__, rectTemp.left, rectTemp.top, rectTemp.right, rectTemp.bottom );
                hr = E_FAIL;
                goto cleanup;
            }


            //      bottom of the rect off screen
            SetRect( &rectDest, 0, screenHeight - lHeight / 2, lWidth,  screenHeight + lHeight / 2 );
            hr = pControl->SetVideoPosition( NULL, &rectDest );
            StringCchPrintf( tmpMsg,
                             128,
                             TEXT("SetVideoPosition w/ smaller video size (%i %i %i %i) under windowless mode ..."), 
                             rectDest.left, rectDest.top, rectDest.right, rectDest.bottom );
            CHECKHR( hr, tmpMsg );
            hr = pControl->GetVideoPosition( NULL, &rectTemp );
            CHECKHR( hr, TEXT("GetVideoPosition under windowless mode ...") );
            if ( rectDest.left != rectTemp.left || rectDest.top != rectTemp.top || 
                rectDest.right != rectTemp.right || rectDest.bottom != rectTemp.bottom )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetVideoPosition didn't return right rect. (%d %d %d %d)."), 
                    __FUNCTION__, __LINE__, __FILE__, rectTemp.left, rectTemp.top, rectTemp.right, rectTemp.bottom );
                hr = E_FAIL;
                goto cleanup;
            }

            //      left of the rect off screen
            SetRect( &rectDest, -1 * lWidth / 2, 0, lWidth / 2, lHeight );
            hr = pControl->SetVideoPosition( NULL, &rectDest );
            StringCchPrintf( tmpMsg,
                             128,
                             TEXT("SetVideoPosition w/ smaller video size (%i %i %i %i) under windowless mode ..."), 
                             rectDest.left, rectDest.top, rectDest.right, rectDest.bottom );
            CHECKHR( hr, tmpMsg );
            hr = pControl->GetVideoPosition( NULL, &rectTemp );
            CHECKHR( hr, TEXT("GetVideoPosition under windowless mode ...") );
            if ( rectDest.left != rectTemp.left || rectDest.top != rectTemp.top || 
                rectDest.right != rectTemp.right || rectDest.bottom != rectTemp.bottom )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetVideoPosition didn't return right rect. (%d %d %d %d)."), 
                    __FUNCTION__, __LINE__, __FILE__, rectTemp.left, rectTemp.top, rectTemp.right, rectTemp.bottom );
                hr = E_FAIL;
                goto cleanup;
            }

            //      right of the rect off screen
            SetRect( &rectDest, screenWidth - lWidth / 2, 0, screenWidth + lWidth / 2, lHeight );
            hr = pControl->SetVideoPosition( NULL, &rectDest );
            StringCchPrintf( tmpMsg,
                             128,
                             TEXT("SetVideoPosition w/ smaller video size (%i %i %i %i) under windowless mode ..."), 
                             rectDest.left, rectDest.top, rectDest.right, rectDest.bottom );
            CHECKHR( hr, tmpMsg );
            //      rect not within the window??
            SetRect( &rectDest, screenWidth - lWidth / 2, 0, screenWidth + lWidth / 2, lHeight );
            hr = pControl->SetVideoPosition( NULL, &rectDest );
            StringCchPrintf( tmpMsg,
                             128,
                             TEXT("SetVideoPosition w/ smaller video size (%i %i %i %i) under windowless mode ..."), 
                             rectDest.left, rectDest.top, rectDest.right, rectDest.bottom );
            CHECKHR( hr, tmpMsg );
            hr = pControl->GetVideoPosition( NULL, &rectTemp );
            CHECKHR( hr, TEXT("GetVideoPosition under windowless mode ...") );
            if ( rectDest.left != rectTemp.left || rectDest.top != rectTemp.top || 
                rectDest.right != rectTemp.right || rectDest.bottom != rectTemp.bottom )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetVideoPosition didn't return right rect. (%d %d %d %d)."), 
                    __FUNCTION__, __LINE__, __FILE__, rectTemp.left, rectTemp.top, rectTemp.right, rectTemp.bottom );
                hr = E_FAIL;
                goto cleanup;
            }
        }
    }

cleanup:
    if ( hwndApp )
        CloseAppWindow(hwndApp);

    if(pConfig)
        pConfig->Release();

    if(pControl)
        pControl->Release();
    
    return hr;
}

HRESULT
VMRWLControlPreConfigTest( BOOL bRemoteGB )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
 
    TestGraph testGraph;
    if ( bRemoteGB )
        testGraph.UseRemoteGB( TRUE );

    // Build an empty graph
    hr = testGraph.BuildEmptyGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Add the VMR manually
    LOG( TEXT("%S: - adding the video mixing renderer to the graph"), __FUNCTION__ );
    hr = testGraph.AddFilterByCLSID( CLSID_VideoMixingRenderer, L"VideoMixingRenderer" );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to add the video mixing renderer (0x%x)" ), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Call the interface function before the VMR is configured
    hr = VMRWLControl_DisplayModeChanged( &testGraph, TESTSTATE_PRE_VMRCONFIG );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::DisplayModeChanged pre config the VMR") );
    if ( !bRemoteGB )
    {
        hr = VMRWLControl_GetCurrentImage( &testGraph, TESTSTATE_PRE_VMRCONFIG );
        CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::GetCurrentImage pre config the VMR") );
    }
    hr = VMRWLControl_GetMaxMinIdealVideoSize( &testGraph, TESTSTATE_PRE_VMRCONFIG );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::GetMaxMinIdealVideoSize pre config the VMR") );
    hr = VMRWLControl_GetNativeVideoSize( &testGraph, TESTSTATE_PRE_VMRCONFIG );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::GetNativeVideoSize pre config the VMR") );
    hr = VMRWLControl_RepaintVideo( &testGraph, TESTSTATE_PRE_VMRCONFIG );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::RepaintVideo pre config the VMR") );
    hr = VMRWLControl_SetVideoClippingWindow( &testGraph, TESTSTATE_PRE_VMRCONFIG );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetVideoClippingWindow pre config the VMR") );
    hr = VMRWLControl_SetGetAspectRatioMode( &testGraph, TESTSTATE_PRE_VMRCONFIG );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetAspectRatioMode pre config the VMR") );
    hr = VMRWLControl_SetGetBorderColor( &testGraph, TESTSTATE_PRE_VMRCONFIG );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetBorderColor pre config the VMR") );
    hr = VMRWLControl_SetGetColorKey( &testGraph, TESTSTATE_PRE_VMRCONFIG );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetColorKey pre config the VMR") );
    hr = VMRWLControl_SetGetVideoPosition( &testGraph, TESTSTATE_PRE_VMRCONFIG );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetVideoPosition pre config the VMR") );

cleanup:
    
    // Reset the testGraph
    testGraph.Reset();

    return hr;
}

HRESULT
VMRWLControlPreConnectTest( VMRTestDesc *pTestDesc, DWORD dwMode )
{
    CheckPointer( pTestDesc, E_POINTER );

    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IVMRFilterConfig        *pVMRConfig = NULL;
 
    TestGraph testGraph;
    if ( pTestDesc->MixingMode() )
        testGraph.UseVMR( TRUE );

    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    // Build an empty graph
    hr = testGraph.BuildEmptyGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Add the VMR manually
    LOG( TEXT("%S: - adding the video mixing renderer to the graph"), __FUNCTION__ );
    hr = testGraph.AddFilterByCLSID( CLSID_VideoMixingRenderer, L"VideoMixingRenderer" );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to add the video mixing renderer (0x%x)" ), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = testGraph.FindInterfaceOnGraph( IID_IVMRFilterConfig, (void **)&pVMRConfig );
    if ( !pVMRConfig )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the VMR from the graph" ), 
                        __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }   

    TCHAR tmp[128] = {0};
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();
    hr = pVMRConfig->SetNumberOfStreams( dwStreams );
    StringCchPrintf( tmp, 128, TEXT("SetNumberofStreams with %d"), dwStreams );
    CHECKHR( hr, tmp );

    if ( VMRMode_Windowless == dwMode )
    {
        hr = pVMRConfig->SetRenderingMode( VMRMode_Windowless );
        CHECKHR( hr, TEXT("Set rendering mode as windowless...") );
    }

    // Call the interface function before Renderfile is called
    hr = VMRWLControl_DisplayModeChanged( &testGraph, TESTSTATE_PRE_CONNECTION );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::DisplayModeChanged pre connect the VMR") );
    if ( !pTestDesc->RemoteGB() )
    {
        hr = VMRWLControl_GetCurrentImage( &testGraph, TESTSTATE_PRE_CONNECTION );
        CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::GetCurrentImage pre connect the VMR") );
    }
    hr = VMRWLControl_GetMaxMinIdealVideoSize( &testGraph, TESTSTATE_PRE_CONNECTION );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::GetMaxMinIdealVideoSize pre connect the VMR") );
    hr = VMRWLControl_GetNativeVideoSize( &testGraph, TESTSTATE_PRE_CONNECTION );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::GetNativeVideoSize pre connect the VMR") );
    hr = VMRWLControl_RepaintVideo( &testGraph, TESTSTATE_PRE_CONNECTION );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::RepaintVideo pre connect the VMR") );
    hr = VMRWLControl_SetVideoClippingWindow( &testGraph, TESTSTATE_PRE_CONNECTION );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetVideoClippingWindow pre connect the VMR") );
    hr = VMRWLControl_SetGetAspectRatioMode( &testGraph, TESTSTATE_PRE_CONNECTION );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetAspectRatioMode pre connect the VMR") );
    hr = VMRWLControl_SetGetBorderColor( &testGraph, TESTSTATE_PRE_CONNECTION );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetBorderColor pre connect the VMR") );
    hr = VMRWLControl_SetGetColorKey( &testGraph, TESTSTATE_PRE_CONNECTION );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetColorKey pre connect the VMR") );
    hr = VMRWLControl_SetGetVideoPosition( &testGraph, TESTSTATE_PRE_CONNECTION );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetVideoPosition pre connect the VMR") );

cleanup:
    if ( pVMRConfig )
        pVMRConfig->Release();

    // Reset the testGraph
    testGraph.Reset();

    return hr;
}

HRESULT 
VMRWLControlPreAllocateTest( VMRTestDesc *pTestDesc, DWORD dwMode )
{
    CheckPointer( pTestDesc, E_POINTER );
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IVMRFilterConfig        *pVMRConfig = NULL;
 
    TestGraph testGraph;

    if ( pTestDesc->MixingMode() )
        testGraph.UseVMR( TRUE );

    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    // Build an empty graph
    hr = testGraph.BuildEmptyGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Add the VMR manually
    LOG( TEXT("%S: - adding the video mixing renderer to the graph"), __FUNCTION__ );
    hr = testGraph.AddFilterByCLSID( CLSID_VideoMixingRenderer, L"VideoMixingRenderer" );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to add the video mixing renderer (0x%x)" ), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = testGraph.FindInterfaceOnGraph( IID_IVMRFilterConfig, (void **)&pVMRConfig );
    if ( !pVMRConfig )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the VMR from the graph" ), 
                        __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    } 

    TCHAR tmp[128] = {0};
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();
    hr = pVMRConfig->SetNumberOfStreams( dwStreams );
    StringCchPrintf( tmp, 128, TEXT("SetNumberofStreams with %d"), dwStreams );
    CHECKHR( hr, tmp );

    if ( VMRMode_Windowless == dwMode )
    {
        hr = pVMRConfig->SetRenderingMode( VMRMode_Windowless );
        CHECKHR( hr, TEXT("Set rendering mode as windowless...") );
    }

    for ( DWORD i = 0; i < dwStreams; i++ )
    {
        testGraph.SetMedia( pTestDesc->GetMedia(i) );
        hr = testGraph.BuildGraph();
        CHECKHR( hr, TEXT("Rendering the file ...") );
    }
    
    // At some point trying to render the streams failed, so bail out.
    if(retval == TPR_FAIL)
        goto cleanup;
    
    // Reacquire needed interfaces for the test functions below



    // Call the interface function before pause is called
    hr = VMRWLControl_DisplayModeChanged( &testGraph, TESTSTATE_PRE_ALLOCATION );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::DisplayModeChanged pre allocation") );
    if ( !pTestDesc->RemoteGB() )
    {
        hr = VMRWLControl_GetCurrentImage( &testGraph, TESTSTATE_PRE_ALLOCATION );
        CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::GetCurrentImage pre allocation") );
    }
    hr = VMRWLControl_GetMaxMinIdealVideoSize( &testGraph, TESTSTATE_PRE_ALLOCATION );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::GetMaxMinIdealVideoSize pre allocation") );
    hr = VMRWLControl_GetNativeVideoSize( &testGraph, TESTSTATE_PRE_ALLOCATION );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::GetNativeVideoSize pre allocation") );
    hr = VMRWLControl_RepaintVideo( &testGraph, TESTSTATE_PRE_ALLOCATION );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::RepaintVideo pre allocation") );
    hr = VMRWLControl_SetVideoClippingWindow( &testGraph, TESTSTATE_PRE_ALLOCATION );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetVideoClippingWindow pre allocation") );
    hr = VMRWLControl_SetGetAspectRatioMode( &testGraph, TESTSTATE_PRE_ALLOCATION );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetAspectRatioMode pre allocation") );
    hr = VMRWLControl_SetGetBorderColor( &testGraph, TESTSTATE_PRE_ALLOCATION );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetBorderColor pre allocation") );
    hr = VMRWLControl_SetGetColorKey( &testGraph, TESTSTATE_PRE_ALLOCATION );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetColorKey pre allocation") );
    hr = VMRWLControl_SetGetVideoPosition( &testGraph, TESTSTATE_PRE_ALLOCATION );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetVideoPosition pre allocation") );

cleanup:
    if ( pVMRConfig )
        pVMRConfig->Release();

    // Reset the testGraph
    testGraph.Reset();

    return hr;
}


HRESULT 
VMRWLControlPauseTest( VMRTestDesc *pTestDesc, DWORD dwMode )
{
    CheckPointer( pTestDesc, E_POINTER );
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IVMRFilterConfig        *pVMRConfig = NULL;
    TestGraph testGraph;

    if ( pTestDesc->MixingMode() )
        testGraph.UseVMR( TRUE );

    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    // Build an empty graph
    hr = testGraph.BuildEmptyGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Add the VMR manually
    LOG( TEXT("%S: - adding the video mixing renderer to the graph"), __FUNCTION__ );
    hr = testGraph.AddFilterByCLSID( CLSID_VideoMixingRenderer, L"VideoMixingRenderer" );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to add the video mixing renderer (0x%x)" ), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = testGraph.FindInterfaceOnGraph( IID_IVMRFilterConfig, (void **)&pVMRConfig );
    if ( !pVMRConfig )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the VMR from the graph" ), 
                        __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    TCHAR tmp[128] = {0};
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();
    hr = pVMRConfig->SetNumberOfStreams( dwStreams );
    StringCchPrintf( tmp, 128, TEXT("SetNumberofStreams with %d"), dwStreams );
    CHECKHR( hr, tmp );

    if ( VMRMode_Windowless == dwMode )
    {
        hr = pVMRConfig->SetRenderingMode( VMRMode_Windowless );
        CHECKHR( hr, TEXT("Set rendering mode as windowless...") );
    }

    for ( DWORD i = 0; i < dwStreams; i++ )
    {
        testGraph.SetMedia( pTestDesc->GetMedia(i) );
        hr = testGraph.BuildGraph();
        CHECKHR( hr, TEXT("Rendering the file ...") );
    }
    
    // At some point trying to render the streams failed, so bail out.
    if(retval == TPR_FAIL)
        goto cleanup;
    
    // Pause, and allocate all resources
    hr = testGraph.SetState( State_Paused, TestGraph::SYNCHRONOUS );
    CHECKHR( hr, TEXT("Pause the graph ...") );

    hr = VMRWLControl_DisplayModeChanged( &testGraph, TESTSTATE_PAUSED );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::DisplayModeChanged when paused") );
    if ( !pTestDesc->RemoteGB() )
    {
        hr = VMRWLControl_GetCurrentImage( &testGraph, TESTSTATE_PAUSED );
        CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::GetCurrentImage when paused") );
    }
    hr = VMRWLControl_GetMaxMinIdealVideoSize( &testGraph, TESTSTATE_PAUSED );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::GetMaxMinIdealVideoSize when paused") );
    hr = VMRWLControl_GetNativeVideoSize( &testGraph, TESTSTATE_PAUSED );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::GetNativeVideoSize when paused") );
    hr = VMRWLControl_RepaintVideo( &testGraph, TESTSTATE_PAUSED );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::RepaintVideo when paused") );
    hr = VMRWLControl_SetVideoClippingWindow( &testGraph, TESTSTATE_PAUSED );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetVideoClippingWindow when paused") );
    hr = VMRWLControl_SetGetAspectRatioMode( &testGraph, TESTSTATE_PAUSED );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetAspectRatioMode when paused") );
    hr = VMRWLControl_SetGetBorderColor( &testGraph, TESTSTATE_PAUSED );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetBorderColor when paused") );
    hr = VMRWLControl_SetGetColorKey( &testGraph, TESTSTATE_PAUSED );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetColorKey when paused") );
    hr = VMRWLControl_SetGetVideoPosition( &testGraph, TESTSTATE_PAUSED );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetVideoPosition when paused") );

cleanup:
    if ( pVMRConfig )
        pVMRConfig->Release();

    // Reset the testGraph
    testGraph.Reset();

    return hr;
}

HRESULT 
VMRWLControlRunTest( VMRTestDesc *pTestDesc, DWORD dwMode )
{
    CheckPointer( pTestDesc, E_POINTER );
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IMediaEvent      *pEvent = 0x0;
    IVMRFilterConfig        *pVMRConfig = NULL;
 
    TestGraph testGraph;

    if ( pTestDesc->MixingMode() )
        testGraph.UseVMR( TRUE );

    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    // Build an empty graph
    hr = testGraph.BuildEmptyGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Add the VMR manually
    LOG( TEXT("%S: - adding the video mixing renderer to the graph"), __FUNCTION__ );
    hr = testGraph.AddFilterByCLSID( CLSID_VideoMixingRenderer, L"VideoMixingRenderer" );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to add the video mixing renderer (0x%x)" ), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = testGraph.FindInterfaceOnGraph( IID_IVMRFilterConfig, (void **)&pVMRConfig );
    if ( !pVMRConfig )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the VMR from the graph" ), 
                        __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    TCHAR tmp[128] = {0};
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();
    hr = pVMRConfig->SetNumberOfStreams( dwStreams );
    StringCchPrintf( tmp, 128, TEXT("SetNumberofStreams with %d"), dwStreams );
    CHECKHR( hr, tmp );

    if ( VMRMode_Windowless == dwMode )
    {
        hr = pVMRConfig->SetRenderingMode( VMRMode_Windowless );
        CHECKHR( hr, TEXT("Set rendering mode as windowless...") );
    }

    for ( DWORD i = 0; i < dwStreams; i++ )
    {
        LOG( TEXT("Rendering media file: %s"), pTestDesc->GetMedia(i)->GetUrl() );
        testGraph.SetMedia(pTestDesc->GetMedia(i));
        if(FAILED(hr = testGraph.BuildGraph()))
        {
            LOG( TEXT("%S: ERROR %d@%S - Build graph failed. 0x%08x"),__FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;

            // If we are just trying to save the filter graph with the '/GRF' command (instead of running the test for real)
            // then BuildGraph() will fail to try and short-circuit the test. In this case,
            // we want to ALWAYS ensure that we've tried to render every stream.
            continue;
        }
        else
            LOG( TEXT("Succeeded.") ); 
    }
    
    // At some point trying to render the streams failed, so bail out.
    if(retval == TPR_FAIL)
        goto cleanup;

    // TestGraph does not addref IMediaEvent returned, so don't release at the end.
    hr = testGraph.GetMediaEvent( &pEvent );
    CHECKHR( hr, TEXT("Query IMediaEvent interface ...") );

    // Run the clip
    hr = testGraph.SetState( State_Running, TestGraph::SYNCHRONOUS );
    CHECKHR( hr, TEXT("Run the graph ...") );

    BOOL bDone = FALSE;
    while ( !bDone )
    {
        long evCode;
        LONG_PTR param1;
        LONG_PTR param2;
        HRESULT hrEvent = pEvent->GetEvent( &evCode, 
                                            &param1, 
                                            &param2, 
                                            100 );
        if ( SUCCEEDED(hrEvent) )
        {
            switch (evCode)
            {
                case EC_COMPLETE:
                    LOG( TEXT("Received EC_COMPLETE!\n") );
                    bDone = TRUE;
                    break;

                case EC_VIDEOFRAMEREADY:
                    LOG( TEXT("Received EC_VIDEOFRAMEREADY!\n") );
                    // Call the interface function now that the graph is running and it 
                    // already showed at least one frame.
                    hr = VMRWLControl_DisplayModeChanged( &testGraph, TESTSTATE_RUNNING );
                    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::DisplayModeChanged when paused") );
                    if ( !pTestDesc->RemoteGB() )
                    {
                        hr = VMRWLControl_GetCurrentImage( &testGraph, TESTSTATE_RUNNING );
                        CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::GetCurrentImage when paused") );
                    }
                    hr = VMRWLControl_GetMaxMinIdealVideoSize( &testGraph, TESTSTATE_RUNNING );
                    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::GetMaxMinIdealVideoSize when paused") );
                    hr = VMRWLControl_GetNativeVideoSize( &testGraph, TESTSTATE_RUNNING );
                    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::GetNativeVideoSize when paused") );
                    hr = VMRWLControl_RepaintVideo( &testGraph, TESTSTATE_RUNNING );
                    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::RepaintVideo when paused") );
                    hr = VMRWLControl_SetVideoClippingWindow( &testGraph, TESTSTATE_RUNNING );
                    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetVideoClippingWindow when paused") );
                    hr = VMRWLControl_SetGetAspectRatioMode( &testGraph, TESTSTATE_RUNNING );
                    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetAspectRatioMode when paused") );
                    hr = VMRWLControl_SetGetBorderColor( &testGraph, TESTSTATE_RUNNING );
                    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetBorderColor when paused") );
                    hr = VMRWLControl_SetGetColorKey( &testGraph, TESTSTATE_RUNNING );
                    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetColorKey when paused") );
                    hr = VMRWLControl_SetGetVideoPosition( &testGraph, TESTSTATE_RUNNING );
                    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetVideoPosition when paused") );

                    bDone = TRUE;
                    break;
            }
            pEvent->FreeEventParams(evCode, param1, param2);
        }
        else
        {
            LOG( TEXT( "GetEvent failed, exiting... (%08x)" ), hrEvent );
            break;
        }
    }

cleanup:
    testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );

    if ( pVMRConfig )
        pVMRConfig->Release();
    
    // Reset the testGraph
    testGraph.Reset();

    return hr;
}

HRESULT 
VMRWLControlStopAfterRunTest( VMRTestDesc *pTestDesc, DWORD dwMode )
{
    CheckPointer( pTestDesc, E_POINTER );
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IMediaEvent      *pEvent = 0x0;
    IVMRFilterConfig        *pVMRConfig = NULL;
 
    TestGraph testGraph;

    if ( pTestDesc->MixingMode() )
        testGraph.UseVMR( TRUE );

    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    // Build an empty graph
    hr = testGraph.BuildEmptyGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Add the VMR manually
    LOG( TEXT("%S: - adding the video mixing renderer to the graph"), __FUNCTION__ );
    hr = testGraph.AddFilterByCLSID( CLSID_VideoMixingRenderer, L"VideoMixingRenderer" );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to add the video mixing renderer (0x%x)" ), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = testGraph.FindInterfaceOnGraph( IID_IVMRFilterConfig, (void **)&pVMRConfig );
    if ( !pVMRConfig )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the VMR from the graph" ), 
                        __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    TCHAR tmp[128] = {0};
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();
    hr = pVMRConfig->SetNumberOfStreams( dwStreams );
    StringCchPrintf( tmp, 128, TEXT("SetNumberofStreams with %d"), dwStreams );
    CHECKHR( hr, tmp );

    if ( VMRMode_Windowless == dwMode )
    {
        hr = pVMRConfig->SetRenderingMode( VMRMode_Windowless );
        CHECKHR( hr, TEXT("Set rendering mode as windowless...") );
    }

    for ( DWORD i = 0; i < dwStreams; i++ )
    {
        LOG( TEXT("Rendering media file: %s"), pTestDesc->GetMedia(i)->GetUrl() );
        testGraph.SetMedia(pTestDesc->GetMedia(i));
        if(FAILED(hr = testGraph.BuildGraph()))
        {
            LOG( TEXT("%S: ERROR %d@%S - Build graph failed. 0x%08x"),__FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;

            // If we are just trying to save the filter graph with the '/GRF' command (instead of running the test for real)
            // then BuildGraph() will fail to try and short-circuit the test. In this case,
            // we want to ALWAYS ensure that we've tried to render every stream.
            continue;
        }
        else
            LOG( TEXT("Succeeded.") ); 
    }
    
    // At some point trying to render the streams failed, so bail out.
    if(retval == TPR_FAIL)
        goto cleanup;

    // TestGraph does not addref IMediaEvent returned, so don't release at the end.
    hr = testGraph.GetMediaEvent( &pEvent );
    CHECKHR( hr, TEXT("Query IMediaEvent interface ...") );

    // Run the clip
    hr = testGraph.SetState( State_Running, TestGraph::SYNCHRONOUS );
    CHECKHR( hr, TEXT("Run the graph ...") );

    BOOL bDone = FALSE;
    while ( !bDone )
    {
        long evCode;
        LONG_PTR param1;
        LONG_PTR param2;
        HRESULT hrEvent = pEvent->GetEvent( &evCode, 
                                            &param1, 
                                            &param2, 
                                            100 );
        if ( SUCCEEDED(hrEvent) )
        {
            switch (evCode)
            {
                case EC_COMPLETE:
                    LOG( TEXT("Received EC_COMPLETE!\n") );
                    bDone = TRUE;
                    break;

                case EC_VIDEOFRAMEREADY:
                    LOG( TEXT("Received EC_VIDEOFRAMEREADY!\n") );
                    bDone = TRUE;
                    break;
            }
            pEvent->FreeEventParams(evCode, param1, param2);
        }
        else
        {
            LOG( TEXT( "GetEvent failed, exiting... (%08x)" ), hrEvent );
            break;
        }
    }

    hr = testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );
    CHECKHR( hr, TEXT("Stop the graph ...") );
    // Call the interface function now that the graph is running and it 
    // already showed at least one frame.
    hr = VMRWLControl_DisplayModeChanged( &testGraph, TESTSTATE_STOPPED );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::DisplayModeChanged when paused") );
    if ( !pTestDesc->RemoteGB() )
    {
        hr = VMRWLControl_GetCurrentImage( &testGraph, TESTSTATE_STOPPED );
        CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::GetCurrentImage when paused") );
    }
    hr = VMRWLControl_GetMaxMinIdealVideoSize( &testGraph, TESTSTATE_STOPPED );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::GetMaxMinIdealVideoSize when paused") );
    hr = VMRWLControl_GetNativeVideoSize( &testGraph, TESTSTATE_STOPPED );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::GetNativeVideoSize when paused") );
    hr = VMRWLControl_RepaintVideo( &testGraph, TESTSTATE_STOPPED );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::RepaintVideo when paused") );
    hr = VMRWLControl_SetVideoClippingWindow( &testGraph, TESTSTATE_STOPPED );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetVideoClippingWindow when paused") );
    hr = VMRWLControl_SetGetAspectRatioMode( &testGraph, TESTSTATE_STOPPED );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetAspectRatioMode when paused") );
    hr = VMRWLControl_SetGetBorderColor( &testGraph, TESTSTATE_STOPPED );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetBorderColor when paused") );
    hr = VMRWLControl_SetGetColorKey( &testGraph, TESTSTATE_STOPPED );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetColorKey when paused") );
    hr = VMRWLControl_SetGetVideoPosition( &testGraph, TESTSTATE_STOPPED );
    CHECKHR( hr, TEXT("Testing IVMRWindowlessControl::SetGetVideoPosition when paused") );

cleanup:

    if ( pVMRConfig )
        pVMRConfig->Release();

    // Reset the testGraph
    testGraph.Reset();

    return hr;
}


TESTPROCAPI 
IVMRWindowlessControlTest( UINT uMsg, 
                           TPPARAM tpParam, 
                           LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    HRESULT tmphr;
 
    // Handle tux messages
    if ( HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    VMRTestDesc* pTestDesc = (VMRTestDesc*)lpFTE->dwUserData;

    // In windowed mode, after the VMR is configured, the interface calls should return
    // VFW_E_WRONG_STATE
    tmphr = VMRWLControlPreConfigTest( pTestDesc->RemoteGB() );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRWindowlessControl test failed in windowed mode pre config. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }

    tmphr = VMRWLControlPreConnectTest( pTestDesc, VMRMode_Windowed );
    if ( E_NOINTERFACE == tmphr )
        LOG( TEXT("%S: WARNING %d@%S - IVMRWindowlessControl test return no interface in windowed mode pre connection."), 
                                __FUNCTION__, __LINE__, __FILE__ );
    else if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: WARNING %d@%S - IVMRWindowlessControl test failed in windowed mode pre connection. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }

    tmphr = VMRWLControlPreAllocateTest( pTestDesc, VMRMode_Windowed );
    if ( E_NOINTERFACE == tmphr )
        LOG( TEXT("%S: WARNING %d@%S - IVMRWindowlessControl test return no interface in windowed mode pre allocation."), 
                                __FUNCTION__, __LINE__, __FILE__ );
    else if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRWindowlessControl test failed in windowed mode pre allocation. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }

    tmphr = VMRWLControlPauseTest( pTestDesc, VMRMode_Windowed );
    if ( E_NOINTERFACE == tmphr )
        LOG( TEXT("%S: WARNING %d@%S - IVMRWindowlessControl test return no interface in windowed mode under pause."), 
                                __FUNCTION__, __LINE__, __FILE__ );
    else if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRWindowlessControl test failed in windowed mode under pause. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }

    tmphr = VMRWLControlRunTest( pTestDesc, VMRMode_Windowed );
    if ( E_NOINTERFACE == tmphr )
        LOG( TEXT("%S: WARNING %d@%S - IVMRWindowlessControl test return no interface in windowed mode when running."), 
                                __FUNCTION__, __LINE__, __FILE__ );
    else if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRWindowlessControl test failed in windowed mode when running. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }

    tmphr = VMRWLControlStopAfterRunTest( pTestDesc, VMRMode_Windowed );
    if ( E_NOINTERFACE == tmphr )
        LOG( TEXT("%S: WARNING %d@%S - IVMRWindowlessControl test return no interface in windowed mode when stopped."), 
                                __FUNCTION__, __LINE__, __FILE__ );
    else if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRWindowlessControl test failed in windowed mode when stopped. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }

    // In windowless mode
    tmphr = VMRWLControlPreConfigTest( pTestDesc->RemoteGB() );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRWindowlessControl test failed in windowless mode pre config. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }

    tmphr = VMRWLControlPreConnectTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRWindowlessControl test failed in windowless mode pre connection. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }

    tmphr = VMRWLControlPreAllocateTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRWindowlessControl test failed in windowless mode pre allocation. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }

    tmphr = VMRWLControlPauseTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmphr) )
    {   
        LOG( TEXT("%S: ERROR %d@%S - IVMRWindowlessControl test failed in windowless mode under pause. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }

    tmphr = VMRWLControlRunTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRWindowlessControl test failed in windowless mode when running. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }

    tmphr = VMRWLControlStopAfterRunTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRWindowlessControl test failed in windowless mode when stopped. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }

    if ( FAILED(hr) )
        retval = TPR_FAIL;

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    // Reset the test
    pTestDesc->Reset();

    return retval;
}
