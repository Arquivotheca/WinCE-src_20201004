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

#include "TuxGraphTest.h"
#include "logging.h"
#include "RotationScalingTestDesc.h"

// BuildGraphTestDesc methods
RotationScalingTestDesc::RotationScalingTestDesc()
{
    // Init the source and dest rectangles
    bSrcRect = false;
    bDestRect = false;
    bSetLog = false;
    bPrefUpstreamRotate = false;
    bPrefUpstreamScale = false;    
    bCheckUsingOverlay = false;
    m_dwPassRate = 0;
    m_bMixerMode = FALSE;
    m_bUpdateWindow = TRUE; //by default, always update window
    m_dwWindowSizeChange = 0; //by default, window size always match video size
    m_vmrTestMode = NORMAL_MODE;
    m_dwRenderingPrefsSurface = 0;
    m_lStepSize = 0;
    m_dwTestSteps = 0;    
    memset(&srcRect, 0, sizeof(RECT));
    memset(&destRect, 0, sizeof(RECT));
}

RotationScalingTestDesc::~RotationScalingTestDesc()
{
    multiRotateConfigList.clear();
    multiScaleList.clear();
    dynamicRotateConfigList.clear();
    dynamicScaleList.clear();
    pausedRotateConfigList.clear();
    pausedScaleList.clear();
    m_lstSrcRects.clear();
    m_lstDestRects.clear();

}

STREAMINFO *
RotationScalingTestDesc::GetStreamInfo( DWORD dwStreamID )
{
    if ( dwStreamID >= VMR_MAX_INPUT_STREAMS )
        return NULL;
    else
        return &m_StreamInfo[dwStreamID];
}

PlaybackMedia* 
RotationScalingTestDesc::GetMedia( int i )
{
    HRESULT hr = S_OK;

    if ( !m_bMixerMode ) 
        return TestDesc::GetMedia( i );

    TCHAR* szMediaProtocol = m_StreamInfo[i].szMedia;
    TCHAR szMedia[MEDIA_NAME_LENGTH] = {0};
    TCHAR szProtocol[PROTOCOL_NAME_LENGTH] = {0};

    // Get the well known name from the string containing media name and protocol
    hr = ::GetMediaName(szMediaProtocol, szMedia, countof(szMedia));
    if ( FAILED(hr) )
        return NULL;

    // Get the media object corresponding to the well known name
    PlaybackMedia* pMedia = FindPlaybackMedia( GraphTest::m_pTestConfig->GetMediaList(), 
                                                                            szMedia, 
                                                                            countof(szMedia) );

    if ( !pMedia )
        return NULL;

    // Get the protocol from the string containing media name and protocol
    hr = ::GetProtocolName(szMediaProtocol, szProtocol, countof(szProtocol));
    if (FAILED(hr))
        return NULL;

    // Set the protocol in the media object
    hr = pMedia->SetProtocol(szProtocol);
    if (FAILED(hr))
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to set protocol %s."), __FUNCTION__, __LINE__, __FILE__, szProtocol );
        return NULL;
    }


    // Set the download location in the media if needed
    if ( _tcscmp( m_StreamInfo[i].szDownloadLocation, TEXT("")) )
    hr = pMedia->SetDownloadLocation( m_StreamInfo[i].szDownloadLocation );

    return ( FAILED(hr) ) ? NULL : pMedia;
}

BOOL
SetStreamParameters( IVMRMixerControl *pVMRMixerControl, RotationScalingTestDesc *pTestDesc )
{
    DWORD dwPrefs;
    
    if ( !pTestDesc || !pVMRMixerControl )
        return FALSE;

    HRESULT hr = S_OK;
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();

    for ( DWORD i = 0; i < dwStreams; i++ )
    {
        if ( pTestDesc->m_StreamInfo[i].fAlpha >= 0.0f )
        {
            hr = pVMRMixerControl->SetAlpha( i, pTestDesc->m_StreamInfo[i].fAlpha );
            LOG( TEXT("SetAlpha for stream %d with %f..."), i, pTestDesc->m_StreamInfo[i].fAlpha );
            CHECKHR( hr, TEXT("SetAlpha...") );
        }
        
        if ( pTestDesc->m_StreamInfo[i].iZOrder >= 0 )
        {
            hr = pVMRMixerControl->SetZOrder( i, (DWORD)(pTestDesc->m_StreamInfo[i].iZOrder) );
            LOG( TEXT("SetZorder for stream %d with %d..."), i, pTestDesc->m_StreamInfo[i].iZOrder );
            CHECKHR( hr, TEXT("SetZOrder...") );
        }
        
        if ( pTestDesc->m_StreamInfo[i].nrOutput.left >= 0.0f ||
            pTestDesc->m_StreamInfo[i].nrOutput.top >= 0.0f || 
            pTestDesc->m_StreamInfo[i].nrOutput.right >= 0.0f ||
            pTestDesc->m_StreamInfo[i].nrOutput.bottom >= 0.0f )
        {
            hr = pVMRMixerControl->SetOutputRect( i, &(pTestDesc->m_StreamInfo[i].nrOutput) );
            LOG( TEXT("SetZorder for stream %d with (%f,%f,%f,%f)..."), 
                        i, pTestDesc->m_StreamInfo[i].nrOutput.left, pTestDesc->m_StreamInfo[i].nrOutput.top,
                        pTestDesc->m_StreamInfo[i].nrOutput.right, pTestDesc->m_StreamInfo[i].nrOutput.bottom );
            CHECKHR( hr, TEXT("SetOutputRect...") );
        }
    }

    //If we are in mixing mode, set mixing preference to RGB 
    hr = pVMRMixerControl->GetMixingPrefs(&dwPrefs);  
    CHECKHR( hr, TEXT("GetMixingPrefs ...") );

    // Remove the current render target flag.
    dwPrefs &= ~MixerPref_RenderTargetMask; 

    // Add the render target flag that we want.
    dwPrefs |= MixerPref_RenderTargetRGB;

    // Set the new flags.
    hr = pVMRMixerControl->SetMixingPrefs( dwPrefs );
    CHECKHR( hr, TEXT("SetMixingPrefs ...") );

cleanup:

    return FAILED(hr) ? FALSE : TRUE;
}

