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
//  Non proxyed interface tests: ensure the interfaces not exposed can't be accessed.
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <tux.h>
#include "TuxGraphTest.h"
#include "logging.h"
#include "StringConversions.h"
#include "GraphTestDesc.h"
#include "TestGraph.h"
#include "GraphTest.h"

static IID NonProxyedInterfaces[] = { 
    IID_IPin, IID_IEnumPins, IID_IEnumMediaTypes, IID_IEnumFilters, IID_IMediaFilter,
    IID_IBaseFilter, IID_IReferenceClock, /*IID_IRefernceClock2,*/ IID_IMediaSample, IID_IMediaSample2,
    IID_IMemAllocator, IID_IMemAllocator2, IID_IMemInputPin, IID_IAMovieSetup, IID_IAudioRenderer, 
    IID_IEnumRegFilters, IID_IFilterMapper, IID_IQualityControl, IID_IOverlayNotify, IID_IMediaEventSink, 
    IID_IFileSinkFilter, IID_IFileSinkFilter2, IID_IFileAsyncIO, IID_ICaptureGraphBuilder, IID_IAMCopyCaptureFileProgress, 
    IID_ICaptureGraphBuilder2, IID_IStreamBuilder, IID_IAsyncReader, IID_IGraphVersion, IID_IResourceConsumer, 
    IID_IResourceManager, IID_IDistributorNotify, IID_IAMStreamControl, IID_ISeekingPassThru, IID_IAMStreamConfig,
    IID_IConfigInterleaving, IID_IConfigAviMux, IID_IAMVideoCompression, IID_IAMBufferNegotiation, IID_IAMVideoProcAmp, 
    IID_IAMCameraControl, IID_IAMVideoControl, IID_IAMCrossbar, IID_IMediaPropertyBag, IID_IPersistMediaPropertyBag,
    IID_IAMPhysicalPinInfo, IID_IAMExtDevice, IID_IAMExtTransport, IID_IAMTimecodeReader, IID_IAMTimecodeGenerator,
    IID_IAMTimecodeDisplay, IID_IAMDevMemoryAllocator, IID_IAMDevMemoryControl, IID_IAMStreamSelect, IID_IAMResourceControl,
    IID_IAMClockAdjust, IID_IAMFilterMiscFlags, IID_IDecimateVideoImage, IID_IAMVideoDecimationProperties, /* IID_IAMVideoRendererNotificationCallback,
    IID_IAMVideoRendererNotification, */ IID_IAMVideoRendererMode, IID_IImageSinkFilter, IID_ISmartTee, IID_IBuffering,
    IID_IAMCertifiedOutputProtection, IID_IVideoFrameStep, IID_IAMVideoTransformComponent, IID_ICaptureMetadata, IID_IImageMetadata,
    IID_IDvdControl, IID_IDvdInfo, IID_IDvdCmd, IID_IDvdState, IID_IDvdControl2,
    IID_IDvdInfo2, IID_IDvdGraphBuilder, IID_IDDrawExclModeVideo, IID_IDDrawExclModeVideoCallback, IID_IVMRImagePresenter, 
    IID_IVMRSurfaceAllocator, IID_IVMRSurfaceAllocatorNotify, IID_IVMRMonitorConfig, /*IID_IVMRDeinterlaceControl,*/ IID_IVMRImageCompositor, 
    IID_IVMRImageCompositor2, IID_IVMRVideoStreamControl, IID_IVMRSurface, IID_IVMRImagePresenterConfig, IID_IVMRImagePresenterExclModeConfig
};

static DWORD NUM_OF_INTERFACES = countof( NonProxyedInterfaces );

TESTPROCAPI 
NonProxyedInterface_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
	IUnknown *pUnk = NULL;
    FilterInfo  tmpFilterInfo;

    // Handle tux messages
    if ( SPR_HANDLED == HandleTuxMessages(uMsg, tpParam) )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    BuildGraphTestDesc* pTestDesc = (BuildGraphTestDesc*)lpFTE->dwUserData;

    // Test Graph
    TestGraph testGraph;
    if ( !pTestDesc->RemoteGB() )
        return TPR_SKIP;
    else
        testGraph.UseRemoteGB( TRUE );

    // From the config, determine the media files to be used
    PlaybackMedia *pMedia = NULL;
    pMedia = pTestDesc->GetMedia( 0 );
    if ( !pMedia )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the media object"), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return TPR_FAIL;
    }

    testGraph.SetMedia( pMedia );

    // Build the graph
    hr = testGraph.BuildGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to build graph for media %s (0x%x)"), 
                    __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    else 
    {
        LOG( TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl() );
    }

    for ( DWORD i = 0; i < NUM_OF_INTERFACES; i++ )
    {
		hr = testGraph.FindInterfaceOnGraph( NonProxyedInterfaces[i], (void **)&pUnk );
        if ( E_NOTIMPL != hr && E_NOINTERFACE != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - FindInterfacesOnGraph didn't return E_NOTIMPL/E_NOINTERFACE. (0x%08x)"), 
				__FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            goto cleanup;
        }
		if ( pUnk )
			pUnk->Release();
		pUnk = NULL;
    }


cleanup:
    if ( pUnk )
        pUnk->Release();

    testGraph.Reset();
    pTestDesc->Reset();

    return retval;    
}