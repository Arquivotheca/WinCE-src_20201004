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

#include <streams.h>
#include "RS_Util.h"
#include "logging.h"
#include "utility.h"

//Scale window
HRESULT
VMRScale(TestGraph* pTestGraph, IBaseFilter   *pVMR, WndScale wndScale, bool bRotate, BOOL bMixingMode)
{
    HRESULT hr = S_OK;
    long lDelta = 100;
    long dstWidth = 0;
    long dstHeight = 0;
    long dstLeft = 0;
    long dstTop = 0;
    long screenWidth= 0;
    long screenHeight = 0;
    RECT destRect;
    IAMVideoTransform   *pVideoTransform = NULL;
    IVMRWindowlessControl   *pVMRWindowlessControl = NULL;
    IVMRMixerControl2    *pVMRMixerControl2 = NULL;
    IGraphBuilder2          *pGraph = NULL;
    IEnumFilterInterfaces   *pEnumInterfaces = NULL;
    IUnknown                *pUnknown = NULL;

    AMScalingRatio *pRatios = NULL;
    ULONG Count = 0;
    DWORD ScalingCaps;
    bool bFound = false;
    bool bDDrawSupported = true;
    LONG lWidth, lHeight;

    pGraph = (IGraphBuilder2 *)pTestGraph->GetGraph();
    if ( !pGraph )
    {
        hr = E_FAIL;
        goto cleanup;
    }

    // FindInterfacesOnGraph can be used within either normal or secure chamber.
    hr = pGraph->FindInterfacesOnGraph( IID_IAMVideoTransform, &pEnumInterfaces );
    CHECKHR( hr, TEXT("Query IAMVideoTransform interface from the filter graph...") );

    hr = pEnumInterfaces->Reset();
    CHECKHR( hr, TEXT("IEnumFilterInterfaces::Reset() ... ") );

    WCHAR   wszInterfaceName[MAX_PATH] = {0};
    hr = pEnumInterfaces->Next( &pUnknown, MAX_PATH, wszInterfaceName );
    CHECKHR( hr, TEXT("IEnumFilterInterfaces::Next() ... ") );
    hr = pUnknown->QueryInterface( IID_IAMVideoTransform, (void **)&pVideoTransform );
    CHECKHR( hr, TEXT("Query IAMVideoTransform interface using IBaseFilter returned") );

    if ( !pVideoTransform )
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to get IAMVideoTransform interface pointer. (0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr);
        goto cleanup;
    }

    //Get codec scaling caps
    hr = pVideoTransform->GetScalingCaps(FALSE, &ScalingCaps, &pRatios, &Count);
    if ( FAILED(hr))
    {
        LOG(TEXT("%S: WARN %d@%S - failed to call GetScalingCaps. (0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr);
    }
    else if( ScalingCaps == AM_TRANSFORM_SCALING_ARBITRARY )
        LOG(TEXT("%S: INFO %d@%S - Codec supports Arbitrary scaling ratio."),
                    __FUNCTION__, __LINE__, __FILE__);
    else if(!ScalingCaps)
    {
        LOG(TEXT("%S: INFO %d@%S - Codec doesn't supports scaling."),
                    __FUNCTION__, __LINE__, __FILE__);
        goto cleanup;
    }

    //Pring scaling caps out
    for (int j = 0; j < (int)Count; j++)
    {
        LOG(TEXT("Scale ration[%d]: %d:%d"), j, pRatios[j].x, pRatios[j].y);
    }

    hr = pVMR->QueryInterface( IID_IVMRWindowlessControl, (void **)&pVMRWindowlessControl );
    CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

    if(bMixingMode)
    {
        // In mixing mode, Retrieve IVMRMixerControl2 interface
        hr = pVMR->QueryInterface( IID_IVMRMixerControl2, (void **)&pVMRMixerControl2 );
        CHECKHR( hr, TEXT("Query IVMRMixerControl2 interface using remote graph builder") );

        //always put the video with max native size on pin 0
        hr = pVMRMixerControl2->GetStreamNativeVideoSize(0, &lWidth, &lHeight);

        LOG( TEXT("GetStreamNativeVideoSize returned stream %d: (%d, %d)"), 0, lWidth, lHeight );
    }
    else
    {
        hr = pVMRWindowlessControl->GetNativeVideoSize( &lWidth, &lHeight, NULL, NULL );
        LOG( TEXT("GetNativeVideoSize returned (%d, %d)"), lWidth, lHeight);
    }

    //Get Screen width and height
    screenHeight = GetSystemMetrics (SM_CYSCREEN);
    screenWidth = GetSystemMetrics (SM_CXSCREEN);
    LOG( TEXT("Get screen size %d:%d"), screenWidth, screenHeight);

    //Get video destination rectangle
    hr = pVMRWindowlessControl->GetVideoPosition( NULL, &destRect );
    CHECKHR( hr, TEXT("GetVideoPosition ..."));

    dstWidth = lWidth;
    dstHeight = lHeight;

    //Special scale ration indicating full screen
    if (wndScale.x == -1 && wndScale.y == -1)
    {
        dstWidth = screenWidth - 6;
        dstHeight = screenHeight - 35;
    }
    else
    {
        //We could scale out of screen here, but we don't care
        dstWidth = wndScale.x*dstWidth/100;
        dstHeight = wndScale.y*dstHeight/100;
    }

    // Size the window to the new size, if new size is 0 due to integer calculation, change it to 1
    dstWidth = dstWidth == 0 ? 1 : dstWidth;
    dstHeight = dstHeight == 0 ? 1 : dstHeight;
    LOG(TEXT("%S: scaling video window to (X:%d, Y:%d)"), __FUNCTION__, dstWidth, dstHeight);

    //Update destination rectangle based on scale.
    destRect.right = destRect.left + dstWidth;
    destRect.bottom = destRect.top + dstHeight;

    //Set video destination rectangle
    hr = pVMRWindowlessControl->SetVideoPosition( NULL, &destRect );
    CHECKHR( hr, TEXT("SetVideoPosition ..."));

cleanup:
    if (pRatios)
        CoTaskMemFree (pRatios);

    if(pVideoTransform)
        pVideoTransform->Release();

    if(pVMRWindowlessControl)
        pVMRWindowlessControl->Release();

    if ( pVMRMixerControl2 )
        pVMRMixerControl2->Release();

    if ( pGraph )
        pGraph->Release();

    if ( pEnumInterfaces )
        pEnumInterfaces->Release();

    if ( pUnknown )
        pUnknown->Release();

    return hr;
}

//VMR Rotate
HRESULT
VMRRotate(TestGraph* pTestGraph, AM_ROTATION_ANGLE rotateAngle, bool* pbRotateWindow, DWORD *pAngle)
{
    HRESULT hr = S_OK;

    if(rotateAngle == AM_ROTATION_ANGLE_90 ||rotateAngle == AM_ROTATION_ANGLE_270)
    {
        *pbRotateWindow = true;
    }

    hr = RotateUI (rotateAngle);
    if (hr == DISP_CHANGE_BADMODE)
    {
        //The specified graphics mode is not supported
        goto cleanup;
    }
    CHECKHR(hr,TEXT("Rotate UI ..." ));

   switch (rotateAngle)
    {
        case AM_ROTATION_ANGLE_0:
            *pAngle = 0;
            break;

        case AM_ROTATION_ANGLE_90:
            *pAngle = 90;
            break;

        case AM_ROTATION_ANGLE_180:
            *pAngle = 180;
            break;

        case AM_ROTATION_ANGLE_270:
            *pAngle = 270;
            break;
    }

cleanup:

    return hr;

}

