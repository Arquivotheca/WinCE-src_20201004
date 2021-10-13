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
//  Video Mixing Renderer: IVMRFilterConfig interface tests
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

//
// a dummy IVMRImageCompositor implementation
//
class CDummyCompositor : public IVMRImageCompositor
{
private:
    LONG            m_cRef;

public:
    CDummyCompositor() : m_cRef(0){}
    ~CDummyCompositor() {}

    STDMETHODIMP_(ULONG) AddRef()
    {
        return (ULONG)++m_cRef;
    }

    STDMETHODIMP_(ULONG) Release()
    {
        return (ULONG)--m_cRef;
    }

    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP InitCompositionTarget(
        IUnknown* pD3DDevice,
        LPDIRECTDRAWSURFACE pddsRenderTarget )
    {
        return S_OK;
    }
    STDMETHODIMP TermCompositionTarget(
        IUnknown* pD3DDevice,
        LPDIRECTDRAWSURFACE pddsRenderTarget
        )
    {
        return S_OK;
    }

    STDMETHODIMP SetStreamMediaType(
        DWORD dwStrmID,
        AM_MEDIA_TYPE* pmt,
        BOOL fTexture
        )
    {
        return S_OK;
    }

    STDMETHODIMP CompositeImage(
        IUnknown* pD3DDevice,
        LPDIRECTDRAWSURFACE pddsRenderTarget,
        AM_MEDIA_TYPE* pmtRenderTarget,
        REFERENCE_TIME rtStart,
        REFERENCE_TIME rtEnd,
        DWORD dwMappedClrBkgnd,
        VMRVIDEOSTREAMINFO* pVideoStreamInfo,
        UINT cStreams )
    {
        return S_OK;
    }
};


HRESULT 
VMRFilterConfig_SetGetNumberOfStreams( CComPtr<IBaseFilter> pVMR, 
                                       TestState state )
{
    HRESULT hr;
    DWORD dwNStreams;
    CComPtr<IVMRFilterConfig> pVMRFilterConfig;

    hr = pVMR.QueryInterface( &pVMRFilterConfig );
    CHECKHR( hr, TEXT("Query IVMRFilterConfig interface...") );

    hr = pVMRFilterConfig->GetNumberOfStreams( NULL );
    hr = (hr == E_POINTER) ? S_OK : E_FAIL;
    CHECKHR( hr, TEXT("Testing GetNumberOfStreams w/ NULL ...") );

    //
    // depending on the state the VMR is at, SetNumberOfStreams may
    // return E_INVALIDARG or invalid state
    //
    hr = pVMRFilterConfig->SetNumberOfStreams( 0 );
    hr = FAILED(hr) ? S_OK : E_FAIL;
    CHECKHR( hr, TEXT("Testing SetNumberOfStreams w/ 0 ...") );

    hr = pVMRFilterConfig->SetNumberOfStreams( 17 );
    hr = FAILED(hr) ? S_OK : E_FAIL;
    CHECKHR( hr, TEXT("Testing SetNumberOfStreams w/ 17 ...") );

    hr = pVMRFilterConfig->SetNumberOfStreams( 0xffffffff );
    hr = FAILED(hr) ? S_OK : E_FAIL;
    CHECKHR( hr, TEXT("Testing SetNumberOfStreams w/ 0xFFFFFFFF ...") );

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // failing case: mixer is not loaded yet
        hr = pVMRFilterConfig->GetNumberOfStreams( &dwNStreams );
        hr = FAILED(hr) ? S_OK : E_FAIL;
        CHECKHR( hr, TEXT("Testing GetNumberOfStreams before mixer is loaded ...") );

        // working case: set number of streams to 2
        hr = pVMRFilterConfig->SetNumberOfStreams( 2 );
        CHECKHR( hr, TEXT("Testing SetNumberOfStreams w/ 2 ...") );

        hr = pVMRFilterConfig->GetNumberOfStreams( &dwNStreams );
        CHECKHR( hr, TEXT("Testing GetNumberOfStreams ...") );
        if ( dwNStreams != 2 )
            hr = E_FAIL;
 
        CHECKHR( hr, TEXT("GetNumberOfStreams should return 2 ...") );

        // failing case: once is set, we can't change it...
        hr = pVMRFilterConfig->SetNumberOfStreams( 2 );
        hr = FAILED(hr) ? S_OK : E_FAIL;
        CHECKHR( hr, TEXT("SetNumberOfStream should fail once it is set ...") );
    }
    else
    {
        dwNStreams = 0xffffffff;
        hr = pVMRFilterConfig->GetNumberOfStreams( &dwNStreams );
        CHECKHR( hr, TEXT("Testing GetNumberOfStreams ...") );

        if (dwNStreams > 16)
            hr = E_FAIL;

        CHECKHR( hr, TEXT("GetNumberOfStreams ...") );

        // failing case: once is set, we can't change it...
        hr = pVMRFilterConfig->SetNumberOfStreams( 2 );
        hr = FAILED(hr) ? S_OK : E_FAIL;
        CHECKHR( hr, TEXT("SetNumberOfStreams should fail once it is set ...") );
    }

cleanup:
    return hr;
}

HRESULT 
VMRFilterConfig_SetGetRenderingMode( CComPtr<IBaseFilter> pVMR, 
                                     TestState state )
{
    HRESULT hr;
    DWORD dwMode;
    CComPtr<IVMRFilterConfig> pVMRFilterConfig;

    hr = pVMR.QueryInterface(&pVMRFilterConfig);
    CHECKHR( hr, TEXT("Query IVMRFilterConfig interface ...") );

    hr = pVMRFilterConfig->GetRenderingMode(NULL);
    hr = (hr == E_POINTER) ? S_OK : E_FAIL;
    CHECKHR( hr, TEXT("Testing GetRenderingMode w/ NULL ...") );

    // failing cases: all invalid values for SetRenderingMode
    hr = pVMRFilterConfig->SetRenderingMode(VMRMode_Mask);
    hr = (hr == E_INVALIDARG) ? S_OK : E_FAIL;
    CHECKHR( hr, TEXT("Testing SetRenderingMode w/ VMRMode_Mask ...") );

    hr = pVMRFilterConfig->SetRenderingMode(0xffffffff);
    hr = (hr == E_INVALIDARG) ? S_OK : E_FAIL;
    CHECKHR( hr, TEXT("Testing SetRenderingMode w/ 0xFFFFFFFF ...") );

    hr = pVMRFilterConfig->SetRenderingMode(0);
    hr = (hr == E_INVALIDARG) ? S_OK : E_FAIL;
    CHECKHR( hr, TEXT("Testing SetRenderingMode w/ 0 ...") );

    hr = pVMRFilterConfig->SetRenderingMode(VMRMode_Windowed | VMRMode_Renderless);
    hr = (hr == E_INVALIDARG) ? S_OK : E_FAIL;
    CHECKHR( hr, TEXT("Testing SetRenderingMode w/ windowed|renderless ...") );

    hr = pVMRFilterConfig->SetRenderingMode(VMRMode_Windowed | VMRMode_Windowless);
    hr = (hr == E_INVALIDARG) ? S_OK : E_FAIL;
    CHECKHR( hr, TEXT("Testing SetRenderingMode w/ windowed|windowless ...") );

    hr = pVMRFilterConfig->SetRenderingMode(VMRMode_Windowless | VMRMode_Renderless);
    hr = (hr == E_INVALIDARG) ? S_OK : E_FAIL;
    CHECKHR( hr, TEXT("Testing SetRenderingMode w/ windowless|renderless ...") );

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // working case: default mode should be Windowed
        hr = pVMRFilterConfig->GetRenderingMode(&dwMode);
        CHECKHR( hr, TEXT("GetRenderingMode ...") );
        if (dwMode != VMRMode_Windowed) 
        {
            // compatibility mode: if no mode has been set, we're in windowed
            hr = E_FAIL;
        }
        CHECKHR( hr, TEXT("GetRenderingMode returning windowed ...") );

        //
        // While the VMR is not configured, we can switch modes
        //

        // set to windowless and verify it's in windowless
        hr = pVMRFilterConfig->SetRenderingMode(VMRMode_Windowless);
        CHECKHR( hr, TEXT("SetRenderingMode w/ windowless ...") );

        hr = pVMRFilterConfig->GetRenderingMode( &dwMode );
        CHECKHR( hr, TEXT("GetRenderingMode ...") );
        if (dwMode != VMRMode_Windowless)
        {
            hr = E_FAIL;
        }
        CHECKHR( hr, TEXT("GetRenderingMode returning windowless ...") );

        // failing case: set to windowed. It should fail because now it's already configured
        hr = pVMRFilterConfig->SetRenderingMode(VMRMode_Windowed);
        hr = (hr == VFW_E_WRONG_STATE) ? S_OK : E_FAIL;
        CHECKHR( hr, TEXT("SetRenderingMode w/ windowed after the mode is set ...") );

        // failing case: set to renderless now. It should fail because now it's already configured.
        hr = pVMRFilterConfig->SetRenderingMode(VMRMode_Renderless);
        hr = FAILED(hr) ? S_OK : E_FAIL;
        CHECKHR( hr, TEXT("SetRenderingMode w/ renderless after the mode is set ...") );

        // working case: set to windowless now. It should work since it's in windowless...
         hr = pVMRFilterConfig->SetRenderingMode(VMRMode_Windowless);
        CHECKHR( hr, TEXT("SetRenderingMode w/ windowless after the mode is set ...") );
    }
    else if ( state <= TESTSTATE_PRE_CONNECTION )
    {
        //
        // You cannot change the mode after any pin has been connected and you cannot 
        // change the mode from windowless or renderless back to windowed, even before 
        // any pins are connected
        //
        dwMode = 0xffffffff;
        hr = pVMRFilterConfig->GetRenderingMode(&dwMode);
        CHECKHR( hr, TEXT("Testing GetRenderingMode w/ NULL ...") );

        if (dwMode != VMRMode_Windowed  &&
            dwMode != VMRMode_Windowless  &&
            dwMode != VMRMode_Renderless)
        {
            hr = E_FAIL;
        }
        CHECKHR( hr, TEXT("GetRenderingMode returns the valid mode ...") );

        switch (dwMode)
        {
            case VMRMode_Windowed:
                hr = pVMRFilterConfig->SetRenderingMode(VMRMode_Windowless);
                hr = SUCCEEDED(hr) ? S_OK : E_FAIL;
                CHECKHR( hr, TEXT("SetRenderingMode w/ windowless when it's in windowed ...") );

                hr = pVMRFilterConfig->SetRenderingMode(VMRMode_Renderless);
                hr = FAILED(hr) ? S_OK : E_FAIL;
                CHECKHR( hr, TEXT("SetRenderingMode w/ renderless when it's in windowed...") );

                hr = pVMRFilterConfig->SetRenderingMode(VMRMode_Windowed);
                hr = FAILED(hr) ? S_OK : E_FAIL;
                CHECKHR( hr, TEXT("SetRenderingMode w/ windowed when it's in windowed ...") );
                break;

            case VMRMode_Windowless:
                hr = pVMRFilterConfig->SetRenderingMode(VMRMode_Renderless);
                hr = FAILED(hr) ? S_OK : E_FAIL;
                CHECKHR( hr, TEXT("SetRenderingMode w/ renderless when it's in windowless ...") );

                hr = pVMRFilterConfig->SetRenderingMode(VMRMode_Windowed);
                hr = FAILED(hr) ? S_OK : E_FAIL;
                CHECKHR( hr, TEXT("SetRenderingMode w/ windowed when it's in windowless ...") );

                hr = pVMRFilterConfig->SetRenderingMode(VMRMode_Windowless);
                CHECKHR( hr, TEXT("SetRenderingMode w/ windowless when it's in windowless ...") );
                break;

            case VMRMode_Renderless:
                hr = pVMRFilterConfig->SetRenderingMode(VMRMode_Windowless);
                hr = FAILED(hr) ? S_OK : E_FAIL;
                CHECKHR( hr, TEXT("SetRenderingMode w/ windowless when it's in renderless ...") );

                hr = pVMRFilterConfig->SetRenderingMode(VMRMode_Windowed);
                hr = FAILED(hr) ? S_OK : E_FAIL;
                CHECKHR( hr, TEXT("SetRenderingMode w/ windowed when it's in windowless ...") );

                hr = pVMRFilterConfig->SetRenderingMode(VMRMode_Renderless);
                CHECKHR( hr, TEXT("SetRenderingMode w/ renderless when it's in windowless ...") );
                break;
        }
    }

cleanup:
    return hr;
}


HRESULT 
VMRFilterConfig_SetGetRenderingPrefs( CComPtr<IBaseFilter> pVMR, 
                                      TestState state )
{
    HRESULT hr;
    CComPtr<IVMRFilterConfig> pVMRFilterConfig;

    hr = pVMR.QueryInterface(&pVMRFilterConfig);
    CHECKHR( hr, TEXT("Query IVMRFilterConfig interface ...") );

    hr = pVMRFilterConfig->GetRenderingPrefs(NULL);
    hr = (hr == E_POINTER) ? S_OK : E_FAIL;
    CHECKHR( hr, TEXT("Testing GetRenderingPrefs w/ NULL ...") );

    DWORD dwPrefs = 0xFFFFFFF & ~RenderPrefs_Mask;
    hr = pVMRFilterConfig->SetRenderingPrefs(dwPrefs);
    hr = (hr == E_INVALIDARG) ? S_OK : E_FAIL;
    CHECKHR( hr, TEXT("Testing SetRenderingPrefs w/ 0xFFFFFFF & ~RenderPrefs_Mask ...") );

    if ( state > TESTSTATE_PRE_VMRCONFIG )
    {
        DWORD dwPrefs;
        hr = pVMRFilterConfig->GetRenderingPrefs(&dwPrefs);
        CHECKHR( hr, TEXT("Testing GetRenderingPrefs ...") );

        hr = pVMRFilterConfig->SetRenderingPrefs(RenderPrefs_Mask);
        CHECKHR( hr, TEXT("Testing SetRenderingPrefs w/ RenderPrefs_Mask ...") );
    }

cleanup:
    return hr;
}

HRESULT
VMRFilterConfig_SetImageCompositor( CComPtr<IBaseFilter> pVMR, 
                                    TestState state )
{
    HRESULT hr;
    CComPtr<IVMRFilterConfig> pVMRFilterConfig;
    static CDummyCompositor Compositor;

    hr = pVMR.QueryInterface(&pVMRFilterConfig);
    CHECKHR( hr, TEXT("Query IVMRFilterConfig interface ...") );

    hr = pVMRFilterConfig->SetImageCompositor(NULL);
    hr = FAILED(hr) ? S_OK : E_FAIL;
    CHECKHR( hr, TEXT("Testing SetImageCompositor w/ NULL ...") );

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // failing case: can't set image compositor before configuring VMR to mixer mode
        hr = pVMRFilterConfig->SetImageCompositor(&Compositor);
        hr = FAILED(hr) ? S_OK : E_FAIL;
        CHECKHR( hr, TEXT("Testing SetImageCompositor before configuring the VMR to the mixer mode...") );
    }
    else if (state <= TESTSTATE_PRE_CONNECTION)
    {
        // working case: VMR is in mixing mode, and not connected yet
        hr = pVMRFilterConfig->SetImageCompositor(&Compositor);
        CHECKHR( hr, TEXT("Testing SetImageCompositor in mixing mode, but not connected yet ...") );
    }
    else
    {
        // failing case: VMR is already connected, can't set compositor
        hr = pVMRFilterConfig->SetImageCompositor(&Compositor);
        hr = FAILED(hr) ? S_OK : E_FAIL;
        CHECKHR( hr, TEXT("Testing SetImageCompositor when the VMR is already connected ...") );
    }

cleanup:

    return hr;
}


HRESULT
VMRFilterConfigPreConfigTest( BOOL bRemoteGB )
{
	DWORD retval = TPR_PASS;
	HRESULT hr = S_OK;
    IVMRFilterConfig *pVMRConfig = 0x0;
	IBaseFilter   *pVMR = 0x0;
 
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
	hr = VMRFilterConfig_SetGetRenderingMode( pVMR, TESTSTATE_PRE_VMRCONFIG );
	CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetRenderingMode before configuring the VMR") );
	hr = VMRFilterConfig_SetGetRenderingPrefs( pVMR, TESTSTATE_PRE_VMRCONFIG );
	CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetRenderingPrefs before configuring the VMR") );
    if ( !bRemoteGB )
    {
	    hr = VMRFilterConfig_SetImageCompositor( pVMR, TESTSTATE_PRE_VMRCONFIG );
	    CHECKHR( hr, TEXT("Testing IVMRMixerControl::SetImageCompositor before configuring the VMR") );
    }
    // it will change the VMR status inside because of SetNumberOfStreams
    hr = VMRFilterConfig_SetGetNumberOfStreams( pVMR, TESTSTATE_PRE_VMRCONFIG );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetNumberOfStreams before configuring the VMR") );

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
VMRFilterConfigPreConnectTest( VMRTestDesc *pTestDesc, DWORD dwMode )
{
    CheckPointer( pTestDesc, E_POINTER );

    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IBaseFilter   *pVMR = 0x0;
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

    hr = pVMRConfig->QueryInterface( IID_IUnknown, (void **)&pVMR );
	if ( !pVMR )
	{
		LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the VMR from the graph" ), 
						__FUNCTION__, __LINE__, __FILE__ );
		retval = TPR_FAIL;
		goto cleanup;
	}    
    CHECKHR( hr, TEXT("Query IVMRFilterConfig interface") );

	TCHAR tmp[128] = {0};
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();
    hr = pVMRConfig->SetNumberOfStreams( dwStreams );
    StringCchPrintf( tmp, 128, TEXT("SetNumberofStreams with %d"), dwStreams );
    CHECKHR( hr, tmp );

    // Call the interface function before Renderfile is called
	hr = VMRFilterConfig_SetGetNumberOfStreams( pVMR, TESTSTATE_PRE_CONNECTION );
	CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetNumberOfStreams before connecting the VMR") );
	hr = VMRFilterConfig_SetGetRenderingMode( pVMR, TESTSTATE_PRE_CONNECTION );
	CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetRenderingMode before connecting the VMR") );
	hr = VMRFilterConfig_SetGetRenderingPrefs( pVMR, TESTSTATE_PRE_CONNECTION );
	CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetRenderingPrefs before connecting the VMR") );
    if ( !pTestDesc->RemoteGB() )
    {
	    hr = VMRFilterConfig_SetImageCompositor( pVMR, TESTSTATE_PRE_CONNECTION );
	    CHECKHR( hr, TEXT("Testing IVMRMixerControl::SetImageCompositor before connecting the VMR") );
    }

cleanup:
    if ( pVMRConfig )
        pVMRConfig->Release();

    if ( pVMR ) 
        pVMR->Release();
    
    // Reset the testGraph
    testGraph.Reset();

    return hr;
}

HRESULT 
VMRFilterConfigPreAllocateTest( VMRTestDesc *pTestDesc, DWORD dwMode )
{
    CheckPointer( pTestDesc, E_POINTER );
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IBaseFilter   *pVMR = 0x0;
    IVMRFilterConfig        *pVMRConfig = NULL;
    IVMRWindowlessControl   *pVMRWindowlessControl = NULL;
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
	}    CHECKHR( hr, TEXT("Query IVMRFilterConfig interface") );

	TCHAR tmp[128] = {0};
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();
	hr = pVMRConfig->SetNumberOfStreams( dwStreams );
	StringCchPrintf( tmp, 128, TEXT("SetNumberofStreams with %d"), dwStreams );
    CHECKHR( hr, tmp );

    if ( VMRMode_Windowless == dwMode )
    {
        // Set windowless parameters
        hr = pVMRConfig->SetRenderingMode( VMRMode_Windowless );
        CHECKHR( hr, TEXT("Set rendering mode as windowless...") );

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
	hr = VMRFilterConfig_SetGetNumberOfStreams( pVMR, TESTSTATE_PRE_ALLOCATION );
	CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetNumberOfStreams before surface allocation") );
	hr = VMRFilterConfig_SetGetRenderingMode( pVMR, TESTSTATE_PRE_ALLOCATION );
	CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetRenderingMode before surface allocation") );
	hr = VMRFilterConfig_SetGetRenderingPrefs( pVMR, TESTSTATE_PRE_ALLOCATION );
	CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetRenderingPrefs before surface allocation") );
    if ( !pTestDesc->RemoteGB() )
    {
	    hr = VMRFilterConfig_SetImageCompositor( pVMR, TESTSTATE_PRE_ALLOCATION );
	    CHECKHR( hr, TEXT("Testing IVMRMixerControl::SetImageCompositor before surface allocation") );
    }

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
VMRFilterConfigPauseTest( VMRTestDesc *pTestDesc, DWORD dwMode )
{
    CheckPointer( pTestDesc, E_POINTER );
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IBaseFilter   *pVMR = 0x0;
    IVMRFilterConfig        *pVMRConfig = NULL;
    IVMRWindowlessControl   *pVMRWindowlessControl = NULL;
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
    hr = VMRFilterConfig_SetGetNumberOfStreams( pVMR, TESTSTATE_PAUSED );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetNumberOfStreams when paused") );
    hr = VMRFilterConfig_SetGetRenderingMode( pVMR, TESTSTATE_PAUSED );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetRenderingMode when paused") );
	hr = VMRFilterConfig_SetGetRenderingPrefs( pVMR, TESTSTATE_PAUSED );
	CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetRenderingPrefs when paused") );
    if ( !pTestDesc->RemoteGB() )
    {
	    hr = VMRFilterConfig_SetImageCompositor( pVMR, TESTSTATE_PAUSED );
	    CHECKHR( hr, TEXT("Testing IVMRMixerControl::SetImageCompositor when paused") );
    }

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
VMRFilterConfigRunTest( VMRTestDesc *pTestDesc, DWORD dwMode )
{
    CheckPointer( pTestDesc, E_POINTER );
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IBaseFilter   *pVMR = 0x0;
    IMediaEvent      *pEvent = 0x0;
    IVMRFilterConfig        *pVMRConfig = NULL;
    IVMRWindowlessControl   *pVMRWindowlessControl = NULL;
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
					hr = VMRFilterConfig_SetGetNumberOfStreams( pVMR, TESTSTATE_RUNNING );
					CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetNumberOfStreams when running") );
					hr = VMRFilterConfig_SetGetRenderingMode( pVMR, TESTSTATE_RUNNING );
					CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetRenderingMode when running") );
					hr = VMRFilterConfig_SetGetRenderingPrefs( pVMR, TESTSTATE_RUNNING );
					CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetRenderingPrefs when running") );
                    if ( !pTestDesc->RemoteGB() )
                    {
					    hr = VMRFilterConfig_SetImageCompositor( pVMR, TESTSTATE_RUNNING );
					    CHECKHR( hr, TEXT("Testing IVMRMixerControl::SetImageCompositor when running") );
                    }

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
VMRFilterConfigStopAfterRunTest( VMRTestDesc *pTestDesc, DWORD dwMode )
{
    CheckPointer( pTestDesc, E_POINTER );
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IBaseFilter   *pVMR = 0x0;
    IMediaEvent      *pEvent = 0x0;
    IVMRFilterConfig        *pVMRConfig = NULL;
    IVMRWindowlessControl   *pVMRWindowlessControl = NULL;
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
	hr = VMRFilterConfig_SetGetNumberOfStreams( pVMR, TESTSTATE_STOPPED );
	CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetNumberOfStreams when stopped") );
	hr = VMRFilterConfig_SetGetRenderingMode( pVMR, TESTSTATE_STOPPED );
	CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetRenderingMode when stopped") );
	hr = VMRFilterConfig_SetGetRenderingPrefs( pVMR, TESTSTATE_STOPPED );
	CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetRenderingPrefs when stopped") );
    if ( !pTestDesc->RemoteGB() )
    {
	    hr = VMRFilterConfig_SetImageCompositor( pVMR, TESTSTATE_STOPPED );
	    CHECKHR( hr, TEXT("Testing IVMRMixerControl::SetImageCompositor when stopped") );
    }

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
IVMRFilterConfigTest( UINT uMsg, 
                      TPPARAM tpParam, 
                      LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    HRESULT tmpHr;
 
    // Handle tux messages
    if ( HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    VMRTestDesc* pTestDesc = (VMRTestDesc*)lpFTE->dwUserData;

    // In windowed mode, after the VMR is configured, the interface calls should return
    // VFW_E_WRONG_STATE
    tmpHr = VMRFilterConfigPreConfigTest( pTestDesc->RemoteGB() );
    if ( FAILED(tmpHr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRFilterConfig test failed in windowed mode pre config. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmpHr );
        hr = tmpHr;
    }

    tmpHr = VMRFilterConfigPreConnectTest( pTestDesc, VMRMode_Windowed );
    if ( FAILED(tmpHr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRFilterConfig test failed in windowed mode pre connection. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmpHr );
        hr = tmpHr;
    }

    tmpHr = VMRFilterConfigPreAllocateTest( pTestDesc, VMRMode_Windowed );
    if ( FAILED(tmpHr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRFilterConfig test failed in windowed mode pre allocation. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmpHr );
        hr = tmpHr;
    }

    tmpHr = VMRFilterConfigPauseTest( pTestDesc, VMRMode_Windowed );
    if ( FAILED(tmpHr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRFilterConfig test failed in windowed mode under pause. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmpHr );
        hr = tmpHr;
    }

    tmpHr = VMRFilterConfigRunTest( pTestDesc, VMRMode_Windowed );
    if ( FAILED(tmpHr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRFilterConfig test failed in windowed mode when running. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmpHr );
        hr = tmpHr;
    }

    tmpHr = VMRFilterConfigStopAfterRunTest( pTestDesc, VMRMode_Windowed );
    if ( FAILED(tmpHr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRFilterConfig test failed in windowed mode when stopped. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmpHr );
        hr = tmpHr;
    }

    // In windowless mode
    tmpHr = VMRFilterConfigPreConfigTest( pTestDesc->RemoteGB() );
    if ( FAILED(tmpHr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRFilterConfig test failed in windowless mode pre config. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmpHr );
        hr = tmpHr;
    }

    tmpHr = VMRFilterConfigPreConnectTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmpHr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRFilterConfig test failed in windowless mode pre connection. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmpHr );
        hr = tmpHr;
    }

    tmpHr = VMRFilterConfigPreAllocateTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmpHr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRFilterConfig test failed in windowless mode pre allocation. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmpHr );
        hr = tmpHr;
    }

    tmpHr = VMRFilterConfigPauseTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmpHr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRFilterConfig test failed in windowless mode under pause. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmpHr );
        hr = tmpHr;
    }

    tmpHr = VMRFilterConfigRunTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmpHr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRFilterConfig test failed in windowless mode when running. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmpHr );
        hr = tmpHr;
    }
    tmpHr = VMRFilterConfigStopAfterRunTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmpHr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRFilterConfig test failed in windowless mode when stopped. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmpHr );
        hr = tmpHr;
    }


    if ( FAILED(hr) )
        retval = TPR_FAIL;

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

	// Reset the test
    pTestDesc->Reset();

    return retval;
}
