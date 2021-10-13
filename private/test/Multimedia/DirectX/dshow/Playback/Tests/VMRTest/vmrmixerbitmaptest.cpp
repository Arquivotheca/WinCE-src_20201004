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
//  Video Mixing Renderer Interface Test: IVMRMixerBitmap interface
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
VMRMixerBitmap_GetAlphaBitmapParameters( CComPtr<IBaseFilter> pVMR, 
                                         TestState state )
{
    CComPtr<IVMRMixerBitmap>  pVMRMixerBitmap;
    CComPtr<IVMRFilterConfig> pVMRFilterConfig;
    VMRALPHABITMAP AlphaBitmap;
    HRESULT hr;

    hr = pVMR.QueryInterface(&pVMRMixerBitmap);
    CHECKHR( hr, TEXT("Query IVMRMixerBitmap interface...") );

    hr = pVMR.QueryInterface(&pVMRFilterConfig);
    CHECKHR( hr, TEXT("Query IVMRFilterConfig interface...") );

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // mixer is not loaded. GetAlphaBitmapParams should fail
        hr = pVMRMixerBitmap->GetAlphaBitmapParameters( &AlphaBitmap );
        if ( VFW_E_WRONG_STATE != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - IVMRMixerBitmap::GetAlphaBitmapParameters didn't return")
                 TEXT(" VFW_E_WRONG_STATE before the mixer is loaded. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            goto cleanup;
        }
    }

    DWORD dwStreams;
    hr = pVMRFilterConfig->GetNumberOfStreams( &dwStreams );
    if ( FAILED(hr) )
    {
        // Mixer is not loaded yet. Configure the VMR so that the mixer is loaded.
        hr = pVMRFilterConfig->SetNumberOfStreams(1);
        CHECKHR( hr, TEXT("Call IFilterConfig::SetNumberOfStreams to load the mixer...") );
    }

    // now the mixer is loaded. GetAlphaBitmapParams should succeed
    hr = pVMRMixerBitmap->GetAlphaBitmapParameters( &AlphaBitmap );
    CHECKHR( hr, TEXT("After the mixer is loaded, call GetAlphaBitmapParameters w/ valid input...") );

    // Pass NULL should return E_POINTER
    hr = pVMRMixerBitmap->GetAlphaBitmapParameters( NULL );
    if ( E_POINTER != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerBitmap::GetAlphaBitmapParameters w/ NULL should return")
                  TEXT("E_POINTER. (0x%x)" ), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        goto cleanup;
    }
    // reset hr
    hr = S_OK;

cleanup:
    return hr;
}


HRESULT 
VMRMixerBitmap_SetAlphaBitmap( CComPtr<IBaseFilter> pVMR, 
                               TestState state )
{
    CComPtr<IVMRMixerBitmap>  pVMRMixerBitmap;
    CComPtr<IDirectDrawSurface> pDDS;

    HDC hDC = NULL;
    VMRALPHABITMAP AlphaBitmap;
    HRESULT hr;

    hr = pVMR.QueryInterface( &pVMRMixerBitmap );
    CHECKHR( hr, TEXT("Query IVMRMixerBitmap interface...") );

    pDDS = CreateDDrawSurface( ALPHA_BITMAP_SIZE, ALPHA_BITMAP_SIZE );
    if ( !pDDS )
        hr = E_FAIL;
    
    CHECKHR( hr, TEXT("Create Direct Draw surface...") );

    hDC = GetDC( NULL );
    if ( !hDC )
        hr = E_FAIL;

    CHECKHR( hr, TEXT("GetDC with NULL...") );

    memset( &AlphaBitmap, 0, sizeof(AlphaBitmap) );

    // failing case: arg is NULL
    hr = pVMRMixerBitmap->SetAlphaBitmap( NULL );
    if ( E_POINTER != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerBitmap::SetAlphaBitmap w/ NULL didn't return E_POINTER. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        goto cleanup;
    }

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // failing case: mixer is not loaded yet
        hr = pVMRMixerBitmap->SetAlphaBitmap( &AlphaBitmap );
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IVMRMixerBitmap::SetAlphaBitmap should fail before the mixer is loaded."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            goto cleanup;
        }
        // reset hr
        hr = S_OK;
    }
    else
    {
        // failing case: arg is NULL with the VMR configured
        hr = pVMRMixerBitmap->SetAlphaBitmap( NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - IVMRMixerBitmap::SetAlphaBitmap w/ NULL didn't return E_POINTER. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            goto cleanup;
        }

        // for other testing, we need surfaces to be allocated in the mixer
        if ( state >= TESTSTATE_PAUSED )
        {
            // failing case: hdc & pDDS are NULL
            memset( &AlphaBitmap, 0, sizeof(AlphaBitmap) );
            hr = pVMRMixerBitmap->SetAlphaBitmap( &AlphaBitmap );
            if ( E_INVALIDARG != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - hdc and pdds in AlphaBitmam are NULL, SetAlphaBitmap should return E_INVALIDARG. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                goto cleanup;
            }

            // failing case: invalid flags, others valid
            InitAlphaParams( &AlphaBitmap, 0xff, NULL, pDDS, NULL, NULL, 1.0f, 0 );
            hr = pVMRMixerBitmap->SetAlphaBitmap(&AlphaBitmap);
            if ( E_INVALIDARG != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - Invalid flags in AlphaBitmap.  E_INVALIDARG should be returned. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                goto cleanup;
            }

            // failing case: invalid alpha, others valid
            InitAlphaParams(&AlphaBitmap, 0, NULL, pDDS, NULL, NULL, 1.1f, 0);
            hr = pVMRMixerBitmap->SetAlphaBitmap(&AlphaBitmap);
            if ( E_INVALIDARG != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - E_INVALIDARG should be returned for invalid alpha. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                goto cleanup;
            }

            // failing case: hdc is NULL, pDDS is not null, but VMRBITMAP_HDC is set
            InitAlphaParams(&AlphaBitmap, VMRBITMAP_HDC, NULL, pDDS, NULL, NULL, 1.0f, 0);
            hr = pVMRMixerBitmap->SetAlphaBitmap(&AlphaBitmap);
            if ( E_INVALIDARG != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - E_INVALIDARG should be returned if hdc is NULL, pDDS is not NULL and flag is set for HDC. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                goto cleanup;
            }

            // failing case: hdc is not NULL, pDDS is null, VMRBITMAP_HDC is not set
            InitAlphaParams(&AlphaBitmap, 0, hDC, NULL, NULL, NULL, 1.0f, 0);
            hr = pVMRMixerBitmap->SetAlphaBitmap(&AlphaBitmap);
            if ( E_INVALIDARG != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - E_INVALIDARG should be returned if hdc not null, pDDS is null, but HDC flag is not set. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                goto cleanup;
            }

            // failing case: hdc is not NULL, pDDS is not null, VMRBITMAP_HDC is set
            InitAlphaParams(&AlphaBitmap, VMRBITMAP_HDC, hDC, pDDS, NULL, NULL, 1.0f, 0);
            hr = pVMRMixerBitmap->SetAlphaBitmap(&AlphaBitmap);
            if ( E_INVALIDARG != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - E_INVALIDARG should be returned if hdc and pDDS are NULL, but HDC flag is set. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                goto cleanup;
            }

            // failing case: it's invalid to set SRCRECT flag in SetAlphaBitmap
            InitAlphaParams(&AlphaBitmap, VMRBITMAP_SRCRECT, hDC, pDDS, NULL, NULL, 1.0f, 0);
            hr = pVMRMixerBitmap->SetAlphaBitmap(&AlphaBitmap);
            if ( E_INVALIDARG != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - E_INVALIDARG should be returned if SRCRECT is set in SetAlphaBitmap. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                goto cleanup;
            }

            // failing case: hdc set and specifies ENTIREDDS
            InitAlphaParams(&AlphaBitmap, VMRBITMAP_HDC | VMRBITMAP_ENTIREDDS, NULL, pDDS, NULL, NULL, 1.0f, 0);
            hr = pVMRMixerBitmap->SetAlphaBitmap(&AlphaBitmap);
            if ( E_INVALIDARG != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - E_INVALIDARG should be returned if hdc is set, but specify ENTIREDDS. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                goto cleanup;
            }
        }
        // reset hr
        hr = S_OK;
    }

cleanup:
    if ( hDC )
        ReleaseDC( NULL, hDC );

    return hr;
}


HRESULT 
VMRMixerBitmap_UpdateAlphaBitmapParameters( CComPtr<IBaseFilter> pVMR, 
                                            TestState state )
{
    CComPtr<IVMRMixerBitmap>  pVMRMixerBitmap;
    CComPtr<IDirectDrawSurface> pDDS;
    HDC hDC = NULL;

    VMRALPHABITMAP AlphaBitmap;
    HRESULT hr;
    RECT rcSrc;

    hr = pVMR.QueryInterface( &pVMRMixerBitmap );
    CHECKHR( hr, TEXT("Query IVMRMixerBitmap interface...") );

    pDDS = CreateDDrawSurface(16, 16);
    if ( !pDDS )
        hr = E_FAIL;

    CHECKHR( hr, TEXT("Create DDraw surface...") );

    hDC = GetDC( NULL );
    if ( !hDC )
        hr = E_FAIL;

    CHECKHR( hr, TEXT("Create device context with NULL...") );

    memset( &AlphaBitmap, 0, sizeof(AlphaBitmap) );

    // failing case: arg is NULL
    hr = pVMRMixerBitmap->UpdateAlphaBitmapParameters( NULL );
    if ( E_POINTER != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - Update with NULL should return E_POINTER. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        goto cleanup;
    }
    hr = S_OK;

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // failing case: mixer is not loaded yet
        memset(&AlphaBitmap, 0, sizeof(AlphaBitmap));
        hr = pVMRMixerBitmap->UpdateAlphaBitmapParameters( &AlphaBitmap );
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - UpdateAlphaBitmapParameters should fail if the mixer is not loaded."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            goto cleanup;
        }
        //reset hr
        hr = S_OK;
    }
    else if ( state >= TESTSTATE_PAUSED )
    {
        // For all other testing, we need surfaces to be allocated in the mixer
        //
        // Note that UpdateAlphaBitmapParameters just cares about the following fields:
        //   - dwFlags
        //   - rSrc
        //   - rDest
        //   - fAlpha
        //   - clrSrcKey

        // failing case: invalid flags, others valid
        InitAlphaParams( &AlphaBitmap, 0xff, NULL, NULL, NULL, NULL, 1.0f, 0 );
        hr = pVMRMixerBitmap->UpdateAlphaBitmapParameters( &AlphaBitmap );
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - E_INVALIDARG should be returned for the invalid flag. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            goto cleanup;
        }

        // failing case: invalid alpha
        InitAlphaParams(&AlphaBitmap, 0, NULL, NULL, NULL, NULL, 1.1f, 0);
        hr = pVMRMixerBitmap->UpdateAlphaBitmapParameters(&AlphaBitmap);
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - E_INVALIDARG should be returned for the invalid alpha. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            goto cleanup;
        }

        // failing case: rSrc invalid (no alpha bitmap set yet)
        rcSrc.left = -1;
        rcSrc.top = 0;
        rcSrc.right = 16;
        rcSrc.bottom = 16;
        InitAlphaParams(&AlphaBitmap, VMRBITMAP_SRCRECT, NULL, NULL, &rcSrc, NULL, 1.0f, 0);
        hr = pVMRMixerBitmap->UpdateAlphaBitmapParameters(&AlphaBitmap);
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - E_INVALIDARG should be returned for the invalid rSrc. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            goto cleanup;
        }

        // failing case: rSrc invalid2 (no alpha bitmap set yet)
        rcSrc.left = 0;
        rcSrc.top = 0;
        rcSrc.right = 32;
        rcSrc.bottom = 32;
        InitAlphaParams(&AlphaBitmap, VMRBITMAP_SRCRECT, NULL, NULL, &rcSrc, NULL, 1.0f, 0);
        hr = pVMRMixerBitmap->UpdateAlphaBitmapParameters(&AlphaBitmap);
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - E_INVALIDARG should be returned for the invalid rSrc. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            goto cleanup;
        }

        //
        // Now, set a valid alpha bitmap
        //
        InitAlphaParams(&AlphaBitmap, VMRBITMAP_ENTIREDDS, NULL, pDDS, NULL, NULL, 1.0f, 0);
        hr = pVMRMixerBitmap->SetAlphaBitmap( &AlphaBitmap );
        CHECKHR( hr, TEXT("Set a valid alpha bitmap...") );

        // failing case: rSrc invalid (alpha bitmap set)
        rcSrc.left = -1;
        rcSrc.top = 0;
        rcSrc.right = 16;
        rcSrc.bottom = 16;
        InitAlphaParams(&AlphaBitmap, VMRBITMAP_SRCRECT, NULL, NULL, &rcSrc, NULL, 1.0f, 0);
        hr = pVMRMixerBitmap->UpdateAlphaBitmapParameters(&AlphaBitmap);
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - E_INVALIDARG should be returned for the invalid rSrc. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            goto cleanup;
        }

        // failing case: rSrc invalid2 (alpha bitmap set)
        rcSrc.left = 0;
        rcSrc.top = 0;
        rcSrc.right = 32;
        rcSrc.bottom = 32;
        InitAlphaParams(&AlphaBitmap, VMRBITMAP_SRCRECT, NULL, NULL, &rcSrc, NULL, 1.0f, 0);
        hr = pVMRMixerBitmap->UpdateAlphaBitmapParameters(&AlphaBitmap);
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - E_INVALIDARG should be returned for the invalid rSrc. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            goto cleanup;
        }
        // reset hr 
        hr = S_OK;
    }

cleanup:
    if ( hDC )
        ReleaseDC(NULL, hDC);

    return hr;
}


HRESULT
VMRMixerBitmapPreConfigTest( BOOL bRemoteGB )
{
	DWORD retval = TPR_PASS;
	HRESULT hr = S_OK;
	IBaseFilter   *pVMR = 0x0;
    IVMRFilterConfig *pVMRConfig = 0x0;
 
    TestGraph testGraph;
    if ( bRemoteGB )
        testGraph.UseRemoteGB( TRUE );

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

    hr = testGraph.FindInterfaceOnGraph( IID_IVMRFilterConfig, (void **)&pVMRConfig );
	if ( !pVMRConfig )
	{
		LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the VMR from the graph" ), 
						__FUNCTION__, __LINE__, __FILE__ );
		retval = TPR_FAIL;
		goto cleanup;
	}

    hr = pVMRConfig->QueryInterface( IID_IUnknown, (void **)&pVMR );
	if ( !pVMR )
	{
		LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the VMR from the graph" ), 
						__FUNCTION__, __LINE__, __FILE__ );
		retval = TPR_FAIL;
		goto cleanup;
	}    

    // Call the interface function before the VMR is configured
    hr = VMRMixerBitmap_GetAlphaBitmapParameters( pVMR, TESTSTATE_PRE_VMRCONFIG );
    CHECKHR( hr, TEXT("Testing IVMRMixerBitmap::GetAlphaBitmapParameters before configuring VMR...") );
    hr = VMRMixerBitmap_SetAlphaBitmap( pVMR, TESTSTATE_PRE_VMRCONFIG );
    CHECKHR( hr, TEXT("Testing IVMRMixerBitmap::SetAlphaBitmap before configuring VMR...") );
    hr = VMRMixerBitmap_UpdateAlphaBitmapParameters( pVMR, TESTSTATE_PRE_VMRCONFIG );
    CHECKHR( hr, TEXT("Testing IVMRMixerBitmap::UpdateAlphaBitmapParameters before configuring VMR...") );

cleanup:
	if ( pVMR ) 
		pVMR->Release();
	
    if ( pVMRConfig )
        pVMRConfig->Release();

	// Reset the testGraph
	testGraph.Reset();

    return hr;
}

HRESULT
VMRMixerBitmapPreConnectTest( VMRTestDesc *pTestDesc, DWORD dwMode )
{
    CheckPointer( pTestDesc, E_POINTER );

    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IBaseFilter   *pVMR = 0x0;
    IVMRFilterConfig        *pVMRConfig = NULL;
    IVMRWindowlessControl   *pVMRWindowlessControl= NULL;
    HWND    hwndApp = NULL;
    RECT rcVideoWindow;

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

    hr = pVMRConfig->QueryInterface( IID_IUnknown, (void **)&pVMR );
	if ( !pVMR )
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

        // Set windowless parameters
        hr = testGraph.FindInterfaceOnGraph( IID_IVMRWindowlessControl, (void **)&pVMRWindowlessControl );
        CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

        hwndApp = OpenAppWindow( 0, 0, 320, 240, &rcVideoWindow, pVMRWindowlessControl );
        if ( !hwndApp )
        {
            hr = E_FAIL;
            CHECKHR( hr, TEXT("Create an application window...") );
        }
        hr = pVMRWindowlessControl->SetVideoClippingWindow(hwndApp);
        CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );

        hr = pVMRWindowlessControl->SetBorderColor(RGB(0,0,0));
        CHECKHR( hr, TEXT("SetBorderColor as black...") );

        hr = pVMRWindowlessControl->SetVideoPosition(NULL, &rcVideoWindow);
        CHECKHR( hr, TEXT("SetVideoPosition ...") );
    }

    // Call the interface function before Renderfile is called
    hr = VMRMixerBitmap_GetAlphaBitmapParameters( pVMR, TESTSTATE_PRE_CONNECTION );
    CHECKHR( hr, TEXT("Testing IVMRMixerBitmap::GetAlphaBitmapParameters before connecting VMR...") );
    hr = VMRMixerBitmap_SetAlphaBitmap( pVMR, TESTSTATE_PRE_CONNECTION );
    CHECKHR( hr, TEXT("Testing IVMRMixerBitmap::SetAlphaBitmap before connecting VMR...") );
    hr = VMRMixerBitmap_UpdateAlphaBitmapParameters( pVMR, TESTSTATE_PRE_CONNECTION );
    CHECKHR( hr, TEXT("Testing IVMRMixerBitmap::UpdateAlphaBitmapParameters before connecting VMR...") );


cleanup:
    if ( pVMRConfig )
        pVMRConfig->Release();

    if ( pVMRWindowlessControl )
        pVMRWindowlessControl->Release();

    if ( pVMR ) 
        pVMR->Release();

    if ( hwndApp )
        CloseAppWindow(hwndApp);
    
    // Reset the testGraph
    testGraph.Reset();

    return hr;
}

HRESULT 
VMRMixerBitmapPreAllocateTest( VMRTestDesc *pTestDesc, DWORD dwMode )
{
    CheckPointer( pTestDesc, E_POINTER );
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IBaseFilter   *pVMR = 0x0;
    IVMRFilterConfig        *pVMRConfig = NULL;
    IVMRWindowlessControl    *pVMRWindowlessControl = NULL;
    HWND    hwndApp = NULL;
    RECT rcVideoWindow;
 
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

    hr = pVMRConfig->QueryInterface( IID_IUnknown, (void **)&pVMR );
	if ( !pVMR )
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

        // Set windowless parameters
        hr = testGraph.FindInterfaceOnGraph( IID_IVMRWindowlessControl, (void **)&pVMRWindowlessControl );
        CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

        hwndApp = OpenAppWindow( 0, 0, 320, 240, &rcVideoWindow, pVMRWindowlessControl );
        if ( !hwndApp )
        {
            hr = E_FAIL;
            CHECKHR( hr, TEXT("Create an application window...") );
        }
        hr = pVMRWindowlessControl->SetVideoClippingWindow(hwndApp);
        CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );

        hr = pVMRWindowlessControl->SetBorderColor(RGB(0,0,0));
        CHECKHR( hr, TEXT("SetBorderColor as black...") );

        hr = pVMRWindowlessControl->SetVideoPosition(NULL, &rcVideoWindow);
        CHECKHR( hr, TEXT("SetVideoPosition ...") );
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
    
    // Call the interface function before pause is called
    hr = VMRMixerBitmap_GetAlphaBitmapParameters( pVMR, TESTSTATE_PRE_ALLOCATION );
    CHECKHR( hr, TEXT("Testing IVMRMixerBitmap::GetAlphaBitmapParameters before surface allocation...") );
    hr = VMRMixerBitmap_SetAlphaBitmap( pVMR, TESTSTATE_PRE_ALLOCATION );
    CHECKHR( hr, TEXT("Testing IVMRMixerBitmap::SetAlphaBitmap before surface allocation...") );
    hr = VMRMixerBitmap_UpdateAlphaBitmapParameters( pVMR, TESTSTATE_PRE_ALLOCATION );
    CHECKHR( hr, TEXT("Testing IVMRMixerBitmap::UpdateAlphaBitmapParameters before surface allocation...") );

cleanup:
    if ( pVMRConfig )
        pVMRConfig->Release();

    if ( pVMRWindowlessControl )
        pVMRWindowlessControl->Release();

    if ( pVMR ) 
        pVMR->Release();

    if ( hwndApp )
        CloseAppWindow(hwndApp);
    
    // Reset the testGraph
    testGraph.Reset();

    return hr;
}


HRESULT 
VMRMixerBitmapPauseTest( VMRTestDesc *pTestDesc, DWORD dwMode )
{
    CheckPointer( pTestDesc, E_POINTER );
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IBaseFilter   *pVMR = 0x0;
    IVMRFilterConfig        *pVMRConfig = NULL;
    IVMRWindowlessControl    *pVMRWindowlessControl = NULL;
    HWND    hwndApp = NULL;
    RECT rcVideoWindow;
 
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

    hr = pVMRConfig->QueryInterface( IID_IUnknown, (void **)&pVMR );
	if ( !pVMR )
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

        // Set windowless parameters
        hr = testGraph.FindInterfaceOnGraph( IID_IVMRWindowlessControl, (void **)&pVMRWindowlessControl );
        CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

        hwndApp = OpenAppWindow( 0, 0, 320, 240, &rcVideoWindow, pVMRWindowlessControl );
        if ( !hwndApp )
        {
            hr = E_FAIL;
            CHECKHR( hr, TEXT("Create an application window...") );
        }
        hr = pVMRWindowlessControl->SetVideoClippingWindow(hwndApp);
        CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );

        hr = pVMRWindowlessControl->SetBorderColor(RGB(0,0,0));
        CHECKHR( hr, TEXT("SetBorderColor as black...") );

        hr = pVMRWindowlessControl->SetVideoPosition(NULL, &rcVideoWindow);
        CHECKHR( hr, TEXT("SetVideoPosition ...") );
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

    hr = VMRMixerBitmap_GetAlphaBitmapParameters( pVMR, TESTSTATE_PAUSED );
    CHECKHR( hr, TEXT("Testing IVMRMixerBitmap::GetAlphaBitmapParameters when paused...") );
    hr = VMRMixerBitmap_SetAlphaBitmap( pVMR, TESTSTATE_PAUSED );
    CHECKHR( hr, TEXT("Testing IVMRMixerBitmap::SetAlphaBitmap when paused...") );
    hr = VMRMixerBitmap_UpdateAlphaBitmapParameters( pVMR, TESTSTATE_PAUSED );
    CHECKHR( hr, TEXT("Testing IVMRMixerBitmap::UpdateAlphaBitmapParameters when paused...") );

cleanup:
    if ( pVMRConfig )
        pVMRConfig->Release();

    if ( pVMRWindowlessControl )
        pVMRWindowlessControl->Release();

    if ( pVMR ) 
        pVMR->Release();

    if ( hwndApp )
        CloseAppWindow(hwndApp);
    
    // Reset the testGraph
    testGraph.Reset();

    return hr;
}

HRESULT 
VMRMixerBitmapRunTest( VMRTestDesc *pTestDesc, DWORD dwMode )
{
    CheckPointer( pTestDesc, E_POINTER );
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IBaseFilter   *pVMR = 0x0;
    IMediaEvent      *pEvent = 0x0;
    IVMRFilterConfig        *pVMRConfig = NULL;
    IVMRWindowlessControl    *pVMRWindowlessControl = NULL;
    HWND    hwndApp = NULL;
    RECT rcVideoWindow;
 
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

    hr = pVMRConfig->QueryInterface( IID_IUnknown, (void **)&pVMR );
	if ( !pVMR )
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

        // Set windowless parameters
        hr = testGraph.FindInterfaceOnGraph( IID_IVMRWindowlessControl, (void **)&pVMRWindowlessControl );
        CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

        hwndApp = OpenAppWindow( 0, 0, 320, 240, &rcVideoWindow, pVMRWindowlessControl );
        if ( !hwndApp )
        {
            hr = E_FAIL;
            CHECKHR( hr, TEXT("Create an application window...") );
        }
        hr = pVMRWindowlessControl->SetVideoClippingWindow(hwndApp);
        CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );

        hr = pVMRWindowlessControl->SetBorderColor(RGB(0,0,0));
        CHECKHR( hr, TEXT("SetBorderColor as black...") );

        hr = pVMRWindowlessControl->SetVideoPosition(NULL, &rcVideoWindow);
        CHECKHR( hr, TEXT("SetVideoPosition ...") );
    }

	for ( DWORD i = 0; i < dwStreams; i++ )
	{
        testGraph.SetMedia( pTestDesc->GetMedia(i) );
        if(FAILED(hr = testGraph.BuildGraph()))
        {
            LOG( TEXT("%S: ERROR %d@%S - error code: 0x%08x "),
                    __FUNCTION__, __LINE__, __FILE__, hr);
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
                    hr = VMRMixerBitmap_GetAlphaBitmapParameters( pVMR, TESTSTATE_RUNNING );
                    CHECKHR( hr, TEXT("Testing IVMRMixerBitmap::GetAlphaBitmapParameters when running...") );
                    hr = VMRMixerBitmap_SetAlphaBitmap( pVMR, TESTSTATE_RUNNING );
                    CHECKHR( hr, TEXT("Testing IVMRMixerBitmap::SetAlphaBitmap when running...") );
                    hr = VMRMixerBitmap_UpdateAlphaBitmapParameters( pVMR, TESTSTATE_RUNNING );
                    CHECKHR( hr, TEXT("Testing IVMRMixerBitmap::UpdateAlphaBitmapParameters when running...") );

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

    if ( pVMRWindowlessControl )
        pVMRWindowlessControl->Release();

    if ( pVMR ) 
        pVMR->Release();

    if ( hwndApp )
        CloseAppWindow(hwndApp);
    
    // Reset the testGraph
    testGraph.Reset();

    return hr;
}

HRESULT 
VMRMixerBitmapStopAfterRunTest( VMRTestDesc *pTestDesc, DWORD dwMode )
{
    CheckPointer( pTestDesc, E_POINTER );
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IBaseFilter   *pVMR = 0x0;
    IMediaEvent      *pEvent = 0x0;
    IVMRFilterConfig        *pVMRConfig = NULL;
    IVMRWindowlessControl    *pVMRWindowlessControl = NULL;
    HWND    hwndApp = NULL;
    RECT rcVideoWindow;
 
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

    hr = pVMRConfig->QueryInterface( IID_IUnknown, (void **)&pVMR );
	if ( !pVMR )
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

        // Set windowless parameters
        hr = testGraph.FindInterfaceOnGraph( IID_IVMRWindowlessControl, (void **)&pVMRWindowlessControl );
        CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );
        hwndApp = OpenAppWindow( 0, 0, 320, 240, &rcVideoWindow, pVMRWindowlessControl );
        if ( !hwndApp )
        {
            hr = E_FAIL;
            CHECKHR( hr, TEXT("Create an application window...") );
        }

        hr = pVMRWindowlessControl->SetVideoClippingWindow(hwndApp);
        CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );

        hr = pVMRWindowlessControl->SetBorderColor(RGB(0,0,0));
        CHECKHR( hr, TEXT("SetBorderColor as black...") );

        hr = pVMRWindowlessControl->SetVideoPosition(NULL, &rcVideoWindow);
        CHECKHR( hr, TEXT("SetVideoPosition ...") );
    }

	for ( DWORD i = 0; i < dwStreams; i++ )
	{
        testGraph.SetMedia( pTestDesc->GetMedia(i) );
		hr = testGraph.BuildGraph();
		CHECKHR( hr, TEXT("Rendering the file ...") );
	}
 
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
    // Test interfaces when stopped.
    hr = VMRMixerBitmap_GetAlphaBitmapParameters( pVMR, TESTSTATE_STOPPED );
    CHECKHR( hr, TEXT("Testing IVMRMixerBitmap::GetAlphaBitmapParameters when stopped...") );
    hr = VMRMixerBitmap_SetAlphaBitmap( pVMR, TESTSTATE_STOPPED );
    CHECKHR( hr, TEXT("Testing IVMRMixerBitmap::SetAlphaBitmap when stopped...") );
    hr = VMRMixerBitmap_UpdateAlphaBitmapParameters( pVMR, TESTSTATE_STOPPED );
    CHECKHR( hr, TEXT("Testing IVMRMixerBitmap::UpdateAlphaBitmapParameters when stopped...") );

cleanup:

    if ( pVMRConfig )
        pVMRConfig->Release();

    if ( pVMRWindowlessControl )
        pVMRWindowlessControl->Release();

    if ( pVMR ) 
        pVMR->Release();

    if ( hwndApp )
        CloseAppWindow(hwndApp);
    
    // Reset the testGraph
    testGraph.Reset();

    return hr;
}


TESTPROCAPI 
IVMRMixerBitmapTest( UINT uMsg, 
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
    tmphr = VMRMixerBitmapPreConfigTest( pTestDesc->RemoteGB() );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerBitmap test failed in windowed mode pre config. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }

    tmphr = VMRMixerBitmapPreConnectTest( pTestDesc, VMRMode_Windowed );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerBitmap test failed in windowed mode pre connection. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }
    tmphr = VMRMixerBitmapPreAllocateTest( pTestDesc, VMRMode_Windowed );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerBitmap test failed in windowed mode pre allocation. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }
    tmphr = VMRMixerBitmapPauseTest( pTestDesc, VMRMode_Windowed );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerBitmap test failed in windowed mode under pause. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }
    tmphr = VMRMixerBitmapRunTest( pTestDesc, VMRMode_Windowed );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerBitmap test failed in windowed mode when running. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }
    tmphr = VMRMixerBitmapStopAfterRunTest( pTestDesc, VMRMode_Windowed );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerBitmap test failed in windowed mode when stopped. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }

    // In windowless mode
    tmphr = VMRMixerBitmapPreConfigTest( pTestDesc->RemoteGB() );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerBitmap test failed in windowless mode pre config. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }

    tmphr = VMRMixerBitmapPreConnectTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerBitmap test failed in windowless mode pre connection. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }

    tmphr = VMRMixerBitmapPreAllocateTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerBitmap test failed in windowless mode pre allocation. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }
    tmphr = VMRMixerBitmapPauseTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerBitmap test failed in windowless mode under pause. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }
    tmphr = VMRMixerBitmapRunTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerBitmap test failed in windowless mode when running. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }
    tmphr = VMRMixerBitmapStopAfterRunTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerBitmap test failed in windowless mode when stopped. (0x%x)"), 
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
