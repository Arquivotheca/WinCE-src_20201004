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
//  Video Mixing Renderer Interface Test: IVMRMixerControl interface
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
VMRMixerControl_SetGetAlpha( CComPtr<IBaseFilter> pVMR,
                             TestState state )
{
    HRESULT hr;
    CComPtr<IVMRMixerControl> pVMRMixerControl;
    CComPtr<IVMRFilterConfig> pVMRFilterConfig;

    DWORD dwStreams;

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // failing case: IVMRMixerControl interface is not available before configuring the VMR
        hr = pVMR.QueryInterface( &pVMRMixerControl );
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Should fail because IVMRMixerControl interface is not available before configuring the VMR "), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_FAIL;
            goto cleanup;
        }
        hr = S_OK;
    }
    else if ( state <= TESTSTATE_PRE_CONNECTION )
    {
        // working case: interface should be available now because the calling function should
        // use SetNumberOfStreams to load the mixer
        hr = pVMR.QueryInterface( &pVMRMixerControl );
        CHECKHR( hr, TEXT("Query IVMRMixerControl interface...") );

        hr = pVMR.QueryInterface(&pVMRFilterConfig);
        CHECKHR( hr, TEXT("Query IVMRFilterConfig interface...") );

        // valid case, but since it's not connected, it should return VFW_E_NOT_CONNECTED
        FLOAT    fAlpha = 0;
        hr = pVMRMixerControl->GetAlpha( 0, &fAlpha );
        if ( VFW_E_NOT_CONNECTED != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetAlpha before pin is connected should return VFW_E_NOT_CONNECTED. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }
        hr = pVMRMixerControl->SetAlpha( 0, 0.5f );
        if ( VFW_E_NOT_CONNECTED != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetAlpha before pin is connected should return VFW_E_NOT_CONNECTED. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }
        // test passed
        hr = S_OK;
    }
    else
    {
        FLOAT fAlphaSet = 0.0f;
        FLOAT fAlphaGet = 0.0f; 

        // working case: interface should be available now
        hr = pVMR.QueryInterface( &pVMRMixerControl );
        CHECKHR( hr, TEXT("Query IVMRMixerControl interface...") );

        hr = pVMR.QueryInterface(&pVMRFilterConfig);
        CHECKHR( hr, TEXT("Query IVMRFilterConfig interface...") );

        hr = pVMRFilterConfig->GetNumberOfStreams(&dwStreams);
        CHECKHR( hr, TEXT("GetNumberOfStreams with valid para...") );

        // now that the graph is connected, we can do some more...

        // Use the first connection to test the interfaces.
        // failing case: invalid arg
        FLOAT    fAlpha = 0;
        hr = pVMRMixerControl->GetAlpha( 0, NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetAlpha with NULL should return E_POINTER. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }
        hr = pVMRMixerControl->GetAlpha( dwStreams + 1, &fAlpha );
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetAlpha with invalid stream id (%d) should return E_INVALIDARG. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, dwStreams + 1, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }

        // failing case: invalid arg
        hr = pVMRMixerControl->SetAlpha( 0, -0.1f );
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetAlpha w/ invalid arg -0.1f should return E_INVALIDARG. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }

        // failing case: invalid arg
        hr = pVMRMixerControl->SetAlpha( 0, 1.1f );
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetAlpha w/ invalid arg 1.1f should return E_INVALIDARG. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }

        // failing case: invalid stream
        hr = pVMRMixerControl->SetAlpha( dwStreams + 1, 1.0f );
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetAlpha w/ invalid stream id (%d) should return E_INVALIDARG. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, dwStreams + 1, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }

        // working case: valid alphas for each stream
        TCHAR tmpMsg[128];
        UINT randVal = 0;
        for ( DWORD i = 0; i < dwStreams; i++ )
        {
            ZeroMemory( tmpMsg, 128 );
            rand_s(&randVal);
            fAlphaSet = (FLOAT)(randVal % 100 ) / 100.0f;

            hr = pVMRMixerControl->SetAlpha( i, fAlphaSet );
            StringCchPrintf( tmpMsg, 128, TEXT("SetAlpha with %f for stream %d"), fAlphaSet, i );
            CHECKHR( hr, tmpMsg );

            hr = pVMRMixerControl->GetAlpha( i, &fAlphaGet );
            StringCchPrintf( tmpMsg, 128, TEXT("GetAlpha (%f) for stream %d"), fAlphaGet, i );
            CHECKHR( hr, tmpMsg );

            if ( fAlphaSet != fAlphaGet )
            {
                LOG( TEXT("%S: ERROR %d@%S - Alpha retrieved %f for stream %d is not %f."), 
                            __FUNCTION__, __LINE__, __FILE__, fAlphaGet, i, fAlphaSet );
                hr = SUCCEEDED(hr) ? E_FAIL : hr;
                goto cleanup;
            }

            // NULL pointers
            hr = pVMRMixerControl->GetAlpha( i, NULL );
            if ( E_POINTER != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - GetAlpha w/ NULL should return E_POINTER. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                hr = SUCCEEDED(hr) ? E_FAIL : hr;
                goto cleanup;
            }
        }

        // failing case: set alpha for a stream that was not created
        hr = pVMRMixerControl->SetAlpha( dwStreams + 1, 0.5f );
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetAlpha w/ invalid stream id (%d) should return E_INVALIDARG. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, dwStreams + 1, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }

        // failing case: get alpha for a stream that was not created
        hr = pVMRMixerControl->GetAlpha( dwStreams + 1, &fAlphaGet );
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetAlpha w/ invalid stream id (%d) should return E_INVALIDARG. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, dwStreams + 1, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }
        // test passed
        hr = S_OK;
    }

cleanup:
    return hr;
}
 

HRESULT 
VMRMixerControl_SetGetBackgroundClr( CComPtr<IBaseFilter> pVMR, 
                                     TestState state )
{
    HRESULT hr;
    COLORREF colorRef;
    CComPtr<IVMRMixerControl> pVMRMixerControl;

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // failing case: IVMRMixerControl interface is not available before configuring the VMR
        hr = pVMR.QueryInterface(&pVMRMixerControl);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IVMRMixerControl is not available before configuring the VMR. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            hr = E_FAIL;
            goto cleanup;
        }
        // test passed
        hr = S_OK;
    }
    else if ( state <= TESTSTATE_PRE_CONNECTION )
    {
        // working case: interface should be available now
        hr = pVMR.QueryInterface( &pVMRMixerControl );
        CHECKHR( hr, TEXT("Query IVMRMixerControl interface...") );

        // failing case: invalid arg
        hr = pVMRMixerControl->GetBackgroundClr( NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetBackgroundClr w/ NULL should return E_POINTER. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }

        // working case: default background color
        hr = pVMRMixerControl->GetBackgroundClr(&colorRef);
        CHECKHR( hr, TEXT("GetBackgroundClr with valid para...") );

        // failing case: graph is not connected
        colorRef = RGB(0xff,0,0);
        hr = pVMRMixerControl->SetBackgroundClr( colorRef );
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetBackgroundClr should fail before the VMR is connected. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            hr = E_FAIL;
            goto cleanup;
        }
        // test passed
        hr = S_OK;
    } 
    else
    {
        // working case: interface should be available now
        hr = pVMR.QueryInterface(&pVMRMixerControl);
        CHECKHR( hr, TEXT("Query IVMRMixerControl interface...") );

        // working case: graph is connected
        colorRef = RGB(0xff,0,0);
        hr = pVMRMixerControl->SetBackgroundClr(colorRef);
        CHECKHR( hr, TEXT("SetBackgroundClr with valid para...") );

        hr = pVMRMixerControl->GetBackgroundClr(&colorRef);
        CHECKHR( hr, TEXT("GetBackgroundClr with valid para...") );

        if ( colorRef != RGB(0xff,0,0) )
        {
            LOG( TEXT("%S: ERROR %d@%S - The color retrieved (%08x) is not the color set (%08x)."), 
                        __FUNCTION__, __LINE__, __FILE__, colorRef, RGB(0xff, 0, 0) );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }
        // test passed
        hr = S_OK;
    }

cleanup:
    return hr;
}


HRESULT 
VMRMixerControl_SetGetMixingPrefs( CComPtr<IBaseFilter> pVMR, 
                                   TestState state )
{
    HRESULT hr;
    DWORD dwPrefs, dwDefaultPrefs;
    CComPtr<IVMRMixerControl> pVMRMixerControl;

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // failing case: IVMRMixerControl interface is not available before configuring the VMR
        hr = pVMR.QueryInterface( &pVMRMixerControl );
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IVMRMixerControl should not be available before configuring the VMR. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            hr = E_FAIL;
            goto cleanup;
        }
        // test passed
        hr = S_OK;
    }
    else
    {
        // working case: interface should be available now
        hr = pVMR.QueryInterface( &pVMRMixerControl );
        CHECKHR( hr, TEXT("Query IVMRMixerControl interface...") );

        // failing case: invalid arg
        hr = pVMRMixerControl->GetMixingPrefs(NULL);
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetMixingPrefs w/ NULL should return E_POINTER. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }

        // working case: default preferences
        hr = pVMRMixerControl->GetMixingPrefs( &dwDefaultPrefs );
        CHECKHR( hr, TEXT("GetMixingPrefs with valid para...") );

        // failing case: invalid decimation flag
        dwPrefs = dwDefaultPrefs;
        dwPrefs &= ~MixerPref_DecimateMask;
        dwPrefs |= MixerPref_DecimationReserved;
        hr = pVMRMixerControl->SetMixingPrefs(dwPrefs);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetMixingPrefs w/ invalid decimation flag (%08x) should fail. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, dwPrefs, hr );
            hr = E_FAIL;
            goto cleanup;
        }

        dwPrefs = dwDefaultPrefs;
        dwPrefs &= ~MixerPref_DecimateMask;
        dwPrefs |= MixerPref_NoDecimation | MixerPref_DecimateOutput;
        hr = pVMRMixerControl->SetMixingPrefs(dwPrefs);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetMixingPrefs w/ invalid decimation flag (%08x) should fail. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, dwPrefs, hr );
            hr = E_FAIL;
            goto cleanup;
        }

        // failing case: invalid filter flags
        dwPrefs = dwDefaultPrefs;
        dwPrefs |= MixerPref_FilteringMask;
        hr = pVMRMixerControl->SetMixingPrefs(dwPrefs);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetMixingPrefs w/ invalid filter flag (%08x) should fail. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, dwPrefs, hr );
            hr = E_FAIL;
            goto cleanup;
        }

        dwPrefs = dwDefaultPrefs;
        dwPrefs &= ~MixerPref_FilteringMask;
        dwPrefs |= MixerPref_BiLinearFiltering | MixerPref_PointFiltering;
        hr = pVMRMixerControl->SetMixingPrefs(dwPrefs);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetMixingPrefs w/ invalid filter flag (%08x) should fail. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, dwPrefs, hr );
            hr = E_FAIL;
            goto cleanup;
        }

        // failing case: invalid render target flag
        dwPrefs = dwDefaultPrefs;
        dwPrefs &= ~MixerPref_RenderTargetMask;
        dwPrefs |= MixerPref_RenderTargetReserved;
        hr = pVMRMixerControl->SetMixingPrefs(dwPrefs);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetMixingPrefs w/ invalid render target flag (%08x) should fail. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, dwPrefs, hr );
            hr = E_FAIL;
            goto cleanup;
        }

        dwPrefs = dwDefaultPrefs;
        dwPrefs &= ~MixerPref_RenderTargetMask;
        dwPrefs |= MixerPref_RenderTargetRGB | MixerPref_RenderTargetYUV;
        hr = pVMRMixerControl->SetMixingPrefs(dwPrefs);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetMixingPrefs w/ invalid render target flag (%08x) should fail. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, dwPrefs, hr );
            hr = E_FAIL;
            goto cleanup;
        }

        // failing case: invalid dynamic flag
        dwPrefs = dwDefaultPrefs;
        dwPrefs &= ~MixerPref_DynamicMask;
        dwPrefs |= MixerPref_DynamicReserved;
        hr = pVMRMixerControl->SetMixingPrefs(dwPrefs);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetMixingPrefs w/ invalid dynamic flag (%08x) should fail. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, dwPrefs, hr );
            hr = E_FAIL;
            goto cleanup;
        }

        // try to change RenderTarget
        dwPrefs = dwDefaultPrefs;
        dwPrefs &= ~MixerPref_RenderTargetMask;
        if (dwDefaultPrefs & MixerPref_RenderTargetRGB)
        {
            dwPrefs |= MixerPref_RenderTargetYUV;
        }
        else
        {
            dwPrefs |= MixerPref_RenderTargetRGB;
        }
        hr = pVMRMixerControl->SetMixingPrefs(dwPrefs);
        if (state > TESTSTATE_PRE_CONNECTION)
        {
            // can't change render target once it's connected
            if ( SUCCEEDED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - Changing the render target after the VMR is connected should fail. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                hr = E_FAIL;
                goto cleanup;
            }
        }

        // try to change decimation mode
        dwPrefs = dwDefaultPrefs;
        dwPrefs &= ~MixerPref_DecimateMask;
        if (dwDefaultPrefs & MixerPref_DecimateOutput)
        {
            dwPrefs |= MixerPref_NoDecimation;
        }
        else
        {
            dwPrefs |= MixerPref_DecimateOutput;
        }
        hr = pVMRMixerControl->SetMixingPrefs(dwPrefs);
        if (state > TESTSTATE_PRE_CONNECTION)
        {
            // can't change render target once it's connected
            if ( SUCCEEDED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - Changing the decimation mode after the VMR is connected should fail. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                hr = E_FAIL;
                goto cleanup;
            }
        }

        // test passed
        hr = S_OK;
    } 

cleanup:
    return hr;
}


HRESULT 
VMRMixerControl_SetGetOutputRect( CComPtr<IBaseFilter> pVMR, 
                                  TestState state )
{
    HRESULT hr;
    NORMALIZEDRECT rcOutput;
    CComPtr<IVMRMixerControl> pVMRMixerControl;
    CComPtr<IVMRFilterConfig> pVMRFilterConfig;

    if ( state <= TESTSTATE_PRE_VMRCONFIG )
    {
        // failing case: IVMRMixerControl interface is not available before configuring the VMR
        hr = pVMR.QueryInterface( &pVMRMixerControl );
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IVMRMixerControl should not be available before configuring the VMR. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            hr = E_FAIL;
            goto cleanup;
        }
        // test passed
        hr = S_OK;
    }
    else if ( state <= TESTSTATE_PRE_CONNECTION )
    {
        // working case: interface should be available now because the calling function should
        // use SetNumberOfStreams to load the mixer
        hr = pVMR.QueryInterface( &pVMRMixerControl );
        CHECKHR( hr, TEXT("Query IVMRMixerControl interface...") );

        // failing case: VMR not connected
        hr = pVMRMixerControl->GetOutputRect( 0, &rcOutput );
        if ( VFW_E_NOT_CONNECTED != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetOutputRect should return VFW_E_NOT_CONNECTED before the VMR is connected. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }
        hr = pVMRMixerControl->SetOutputRect( 0, &rcOutput);
        if ( VFW_E_NOT_CONNECTED != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetOutputRect should return VFW_E_NOT_CONNECTED before the VMR is connected. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }
        // test passed
        hr = S_OK;
    }
    else
    {
        // working case: interface should be available now
        hr = pVMR.QueryInterface(&pVMRMixerControl);
        CHECKHR( hr, TEXT("Query IVMRMixerControl interface...") );

        hr = pVMR.QueryInterface(&pVMRFilterConfig);
        CHECKHR( hr, TEXT("Query IVMRFilterConfig interface...") );

        // failing case: invalid arg
        hr = pVMRMixerControl->GetOutputRect(0, NULL);
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetOutputRect w/ NULL should return E_POINTER. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }
        // failing case: invalid stream id
        hr = pVMRMixerControl->SetOutputRect( VMR_MAX_INPUT_STREAMS + 1, &rcOutput);
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetOutputRect w/ invalid stream id (%d) should return E_INVALIDARG. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, VMR_MAX_INPUT_STREAMS + 1, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }
        // failing case: invalid stream id
        hr = pVMRMixerControl->GetOutputRect( VMR_MAX_INPUT_STREAMS + 1, &rcOutput);
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetOutputRect w/ invalid stream id (%d) should return E_INVALIDARG. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, VMR_MAX_INPUT_STREAMS + 1, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }
        DWORD dwNStreams;
        hr = pVMRFilterConfig->GetNumberOfStreams(&dwNStreams);
        CHECKHR( hr, TEXT("GetNumberOfStreams with valid para...") );

        // working case: set output rect
        for (DWORD i = 0; i < dwNStreams; i++)
        {
            rcOutput.left = i + 0.5f;
            rcOutput.top = i + 0.5f;
            rcOutput.right = rcOutput.left + i * 10.0f;
            rcOutput.bottom = rcOutput.top + i * 2.0f;
            hr = pVMRMixerControl->SetOutputRect(i, &rcOutput);
            CHECKHR( hr, TEXT("SetOutputRect with valid input...") );
        }

        for (DWORD i = 0; i < dwNStreams; i++)
        {
            hr = pVMRMixerControl->GetOutputRect(i, &rcOutput);
            CHECKHR( hr, TEXT("GetOutputRect with valid para...") );
            if (rcOutput.left   != (i + 0.5f)  ||
                rcOutput.top    != (i + 0.5f) ||
                rcOutput.right  != (rcOutput.left + i * 10.0f)  ||
                rcOutput.bottom != (rcOutput.top + i * 2.0f))
            {
                LOG( TEXT("%S: ERROR %d@%S - Rect retrieved (%f,%f,%f,%f) is not rect (%f,%f,%f,%f) set"), 
                    __FUNCTION__, __LINE__, __FILE__, rcOutput.left, rcOutput.top, rcOutput.right, rcOutput.bottom,
                    i + 0.5f, i + 0.5f, rcOutput.left + i * 10.0f, rcOutput.top + i * 2.0f );
                hr = SUCCEEDED(hr) ? E_FAIL : hr;
                goto cleanup;
            }
        }

        // failing case: stream invalid
        hr = pVMRMixerControl->SetOutputRect(dwNStreams, &rcOutput);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - . (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            hr = E_FAIL;
            goto cleanup;
        }

        // failing case: stream invalid
        hr = pVMRMixerControl->GetOutputRect(dwNStreams, &rcOutput);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetOutputRect w/ invalid stream id (%d) should fail. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, dwNStreams, hr );
            hr = E_FAIL;
            goto cleanup;
        }
        // test passed
        hr = S_OK;
    } 

cleanup:
    return hr;
}


HRESULT 
VMRMixerControl_SetGetZOrder( CComPtr<IBaseFilter> pVMR, 
                              TestState state )
{
    HRESULT hr;
    DWORD dwZOrder;
    CComPtr<IVMRMixerControl> pVMRMixerControl;
    CComPtr<IVMRFilterConfig> pVMRFilterConfig;

    if (state <= TESTSTATE_PRE_VMRCONFIG)
    {
        // failing case: IVMRMixerControl interface is not available before configuring the VMR
        hr = pVMR.QueryInterface(&pVMRMixerControl);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IVMRMixerControl should not be available before configuring the VMR. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            hr = E_FAIL;
            goto cleanup;
        }    
        // test passed
        hr = S_OK;
    }
    else if ( state <= TESTSTATE_PRE_CONNECTION )
    {
        // working case: interface should be available now because the calling function should
        // use SetNumberOfStreams to load the mixer
        hr = pVMR.QueryInterface( &pVMRMixerControl );
        CHECKHR( hr, TEXT("Query IVMRMixerControl interface...") );

        // failing case: VMR not connected
        hr = pVMRMixerControl->GetZOrder( 0, &dwZOrder );
        if ( VFW_E_NOT_CONNECTED != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetZOrder should return VFW_E_NOT_CONNECTED before pin is connected. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }
        hr = pVMRMixerControl->SetZOrder( 0, 1000 );
        if ( VFW_E_NOT_CONNECTED != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetZOrder should return VFW_E_NOT_CONNECTED before pin is connected. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }
        // test passed
        hr = S_OK;
    }
    else
    {
        // working case: interface should be available now
        hr = pVMR.QueryInterface(&pVMRMixerControl);
        CHECKHR( hr, TEXT("Query IVMRMixerControl interface...") );

        hr = pVMR.QueryInterface(&pVMRFilterConfig);
        CHECKHR( hr, TEXT("Query IVMRFilterConfig interface...") );

        // failing case: invalid arg
        hr = pVMRMixerControl->GetZOrder(0, NULL);
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetZOrder w/ NULL should return E_POINTER. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }

        // failing case: invalid stream id
        hr = pVMRMixerControl->SetZOrder(VMR_MAX_INPUT_STREAMS + 1, 1000);
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetZOrder w/ invalid stream id (%d) should return E_INVALIDARG. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, VMR_MAX_INPUT_STREAMS + 1, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }

        // failing case: invalid stream id
        hr = pVMRMixerControl->GetZOrder(VMR_MAX_INPUT_STREAMS + 1, &dwZOrder);
        if ( E_INVALIDARG != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetZOrder w/ invalid stream id (%d) should return E_INVALIDARG. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, VMR_MAX_INPUT_STREAMS + 1, hr );
            hr = SUCCEEDED(hr) ? E_FAIL : hr;
            goto cleanup;
        }

        DWORD dwNStreams;
        hr = pVMRFilterConfig->GetNumberOfStreams(&dwNStreams);
        CHECKHR( hr, TEXT("GetNumberOfStreams with valid para...") );

        // working case: set zorder
        for (DWORD i = 0; i < dwNStreams; i++)
        {
            hr = pVMRMixerControl->SetZOrder(i, i);
            CHECKHR( hr, TEXT("SetZOrder with valid para...") );
        }

        for (DWORD i = 0; i < dwNStreams; i++)
        {
            hr = pVMRMixerControl->GetZOrder(i, &dwZOrder);
            CHECKHR( hr, TEXT("GetZOrder with valid para...") );
            if (dwZOrder != i)
            {
                LOG( TEXT("%S: ERROR %d@%S - Z order retrieved (%d) is not z order (%d) set."), 
                            __FUNCTION__, __LINE__, __FILE__, dwZOrder, i );
                hr = SUCCEEDED(hr) ? E_FAIL : hr;
                goto cleanup;
            }
        }

        // failing case: stream invalid
        hr = pVMRMixerControl->SetZOrder(dwNStreams, 1);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetZOrder w/ invalid stream id (%d) should fail."), 
                        __FUNCTION__, __LINE__, __FILE__, dwNStreams );
            hr = E_FAIL;
            goto cleanup;
        }

        // failing case: stream invalid
        hr = pVMRMixerControl->GetZOrder(dwNStreams, &dwZOrder);
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetZOrder w/ invalid stream id (%d) should fail"), 
                        __FUNCTION__, __LINE__, __FILE__, dwNStreams );
            hr = E_FAIL;
            goto cleanup;
        }    
        // test passed
        hr = S_OK;
    } 

cleanup:
    return hr;
}


HRESULT
VMRMixerControlPreConfigTest( BOOL bRemoteGB )
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
    hr = VMRMixerControl_SetGetAlpha( pVMR, TESTSTATE_PRE_VMRCONFIG );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::SetAlpha/GetAlpha before configuring the VMR") );
    hr = VMRMixerControl_SetGetBackgroundClr( pVMR, TESTSTATE_PRE_VMRCONFIG );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetBackgroundClr before configuring the VMR") );
    hr = VMRMixerControl_SetGetMixingPrefs( pVMR, TESTSTATE_PRE_VMRCONFIG );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetMixingPrefs before configuring the VMR") );
    hr = VMRMixerControl_SetGetOutputRect( pVMR, TESTSTATE_PRE_VMRCONFIG );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetOutputRect before configuring the VMR") );
    hr = VMRMixerControl_SetGetZOrder( pVMR, TESTSTATE_PRE_VMRCONFIG );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetZOrder before configuring the VMR") );

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
VMRMixerControlPreConnectTest( VMRTestDesc *pTestDesc, DWORD dwMode )
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

    // Call the interface function before Renderfile is called
    hr = VMRMixerControl_SetGetAlpha( pVMR, TESTSTATE_PRE_CONNECTION );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetAlphar before connecting the VMR") );
    hr = VMRMixerControl_SetGetBackgroundClr( pVMR, TESTSTATE_PRE_CONNECTION );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetBackgroundClr before connecting the VMR") );
    hr = VMRMixerControl_SetGetMixingPrefs( pVMR, TESTSTATE_PRE_CONNECTION );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetMixingPrefs before connecting the VMR") );
    hr = VMRMixerControl_SetGetOutputRect( pVMR, TESTSTATE_PRE_CONNECTION );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetOutputRect before connecting the VMR") );
    hr = VMRMixerControl_SetGetZOrder( pVMR, TESTSTATE_PRE_CONNECTION );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetZOrder before connecting the VMR") );


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
VMRMixerControlPreAllocateTest( VMRTestDesc *pTestDesc, DWORD dwMode )
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
    hr = VMRMixerControl_SetGetAlpha( pVMR, TESTSTATE_PRE_ALLOCATION );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetAlpha before surface allocation") );
    hr = VMRMixerControl_SetGetBackgroundClr( pVMR, TESTSTATE_PRE_ALLOCATION );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetBackgroundClr before surface allocation") );
    hr = VMRMixerControl_SetGetMixingPrefs( pVMR, TESTSTATE_PRE_ALLOCATION );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetMixingPrefs before surface allocation") );
    hr = VMRMixerControl_SetGetOutputRect( pVMR, TESTSTATE_PRE_ALLOCATION );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetOutputRect before surface allocation") );
    hr = VMRMixerControl_SetGetZOrder( pVMR, TESTSTATE_PRE_ALLOCATION );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetZOrder before surface allocation") );

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
VMRMixerControlPauseTest( VMRTestDesc *pTestDesc, DWORD dwMode )
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

    hr = VMRMixerControl_SetGetAlpha( pVMR, TESTSTATE_PAUSED );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetAlpha when paused") );
    hr = VMRMixerControl_SetGetBackgroundClr( pVMR, TESTSTATE_PAUSED );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetBackgroundClr when paused") );
    hr = VMRMixerControl_SetGetMixingPrefs( pVMR, TESTSTATE_PAUSED );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetMixingPrefs when paused") );
    hr = VMRMixerControl_SetGetOutputRect( pVMR, TESTSTATE_PAUSED );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetOutputRect when paused") );
    hr = VMRMixerControl_SetGetZOrder( pVMR, TESTSTATE_PAUSED );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetZOrder when paused") );

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
VMRMixerControlRunTest( VMRTestDesc *pTestDesc, DWORD dwMode )
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

        hwndApp = OpenAppWindow( 0, 0, 320, 240, &rcVideoWindow );
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
                    hr = VMRMixerControl_SetGetAlpha( pVMR, TESTSTATE_RUNNING );
                    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetAlpha when running") );
                    hr = VMRMixerControl_SetGetBackgroundClr( pVMR, TESTSTATE_RUNNING );
                    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetBackgroundClr when running") );
                    hr = VMRMixerControl_SetGetMixingPrefs( pVMR, TESTSTATE_RUNNING );
                    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetMixingPrefs when running") );
                    hr = VMRMixerControl_SetGetOutputRect( pVMR, TESTSTATE_RUNNING );
                    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetOutputRect when running") );
                    hr = VMRMixerControl_SetGetZOrder( pVMR, TESTSTATE_RUNNING );
                    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetZOrder when running") );

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
VMRMixerControlStopAfterRunTest( VMRTestDesc *pTestDesc, DWORD dwMode )
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
    hr = VMRMixerControl_SetGetAlpha( pVMR, TESTSTATE_STOPPED );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetAlpha when stopped") );
    hr = VMRMixerControl_SetGetBackgroundClr( pVMR, TESTSTATE_STOPPED );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetBackgroundClr when stopped") );
    hr = VMRMixerControl_SetGetMixingPrefs( pVMR, TESTSTATE_STOPPED );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetMixingPrefs when stopped") );
    hr = VMRMixerControl_SetGetOutputRect( pVMR, TESTSTATE_STOPPED );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetOutputRect when stopped") );
    hr = VMRMixerControl_SetGetZOrder( pVMR, TESTSTATE_STOPPED );
    CHECKHR( hr, TEXT("Testing IVMRMixerControl::Set/GetZOrder when stopped") );

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
IVMRMixerControlTest( UINT uMsg, 
                      TPPARAM tpParam, 
                      LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    // save temp results
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
    tmphr = VMRMixerControlPreConfigTest( pTestDesc->RemoteGB() );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerControl test failed in windowed mode pre config. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }

    tmphr = VMRMixerControlPreConnectTest( pTestDesc, VMRMode_Windowed );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerControl test failed in windowed mode pre connection. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }
    tmphr = VMRMixerControlPreAllocateTest( pTestDesc, VMRMode_Windowed );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerControl test failed in windowed mode pre allocation. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }
    tmphr = VMRMixerControlPauseTest( pTestDesc, VMRMode_Windowed );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerControl test failed in windowed mode under pause. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }
    tmphr = VMRMixerControlRunTest( pTestDesc, VMRMode_Windowed );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerControl test failed in windowed mode when running. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }
    tmphr = VMRMixerControlStopAfterRunTest( pTestDesc, VMRMode_Windowed );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerControl test failed in windowed mode when stopped. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }

    // In windowless mode
    tmphr = VMRMixerControlPreConfigTest( pTestDesc->RemoteGB() );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerControl test failed in windowless mode pre config. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }

    tmphr = VMRMixerControlPreConnectTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerControl test failed in windowless mode pre connection. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }
    tmphr = VMRMixerControlPreAllocateTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerControl test failed in windowless mode pre allocation. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }
    tmphr = VMRMixerControlPauseTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerControl test failed in windowless mode under pause. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }
    tmphr = VMRMixerControlRunTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerControl test failed in windowless mode when running. (0x%x)"), 
                                __FUNCTION__, __LINE__, __FILE__, tmphr );
        hr = tmphr;
    }
    tmphr = VMRMixerControlStopAfterRunTest( pTestDesc, VMRMode_Windowless );
    if ( FAILED(tmphr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IVMRMixerControl test failed in windowless mode when stopped. (0x%x)"), 
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
