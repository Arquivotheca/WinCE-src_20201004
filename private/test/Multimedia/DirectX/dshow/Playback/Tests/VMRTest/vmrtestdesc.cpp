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
#include "VMRTestDesc.h"

VMRTestDesc::VMRTestDesc()
{
    ZeroMemory( &m_AlphaBitmap, sizeof(VMRALPHABITMAP) );
    m_dwStreams = 1;
    m_bMixerMode = FALSE;
    m_dwRenderingMode = VMRMode_Windowed;
    m_State = TESTSTATE_NULL;
    m_dwPassRate = 0;
    m_vmrTestMode = NORMAL_MODE;
    m_dwRenderingPrefsSurface = 0;
    m_bCancelStep = false;
    m_bRunFirst = false;
    m_bPauseFirst = false;
    m_bRunInMiddle = false;
    m_bPauseInMiddle = false;
    m_lStepSize = 0;
    m_lFirstStepSize = 0;    
    m_dwTestSteps = 0;
}

VMRTestDesc::~VMRTestDesc()
{
}

STREAMINFO *
VMRTestDesc::GetStreamInfo( DWORD dwStreamID )
{
    if ( dwStreamID >= VMR_MAX_INPUT_STREAMS )
        return NULL;
    else
        return &m_StreamInfo[dwStreamID];
}

PlaybackMedia* 
VMRTestDesc::GetMedia( int i )
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
        LOG( TEXT("%S: ERROR %d@%S - failed to set protocol %s."), 
                __FUNCTION__, __LINE__, __FILE__, szProtocol );
        return NULL;
    }


    // Set the download location in the media if needed
    if ( _tcscmp( m_StreamInfo[i].szDownloadLocation, TEXT("")) )
        hr = pMedia->SetDownloadLocation( m_StreamInfo[i].szDownloadLocation );
    
    return ( FAILED(hr) ) ? NULL : pMedia;
}

BOOL
VerifyVideoPosition( IVMRWindowlessControl *pVMRWLControl, VMRTestDesc *pTestDesc )
{
    if ( !pTestDesc || !pVMRWLControl )
        return FALSE;

    BOOL ret = TRUE;
    HRESULT hr = S_OK;


cleanup:
    if ( FAILED(hr) )
        ret = FALSE;

    return ret;
}

BOOL
VerifyStreamParameters( IVMRMixerControl *pVMRMixerControl, VMRTestDesc *pTestDesc )
{
    if ( !pTestDesc || !pVMRMixerControl )
        return FALSE;

    BOOL ret = TRUE;
    HRESULT hr = S_OK;
    FLOAT fAlpha;
    DWORD dwZOrder;
    NORMALIZEDRECT nrRect;
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();

    for ( DWORD i = 0; i < dwStreams; i++ )
    {
        if ( pTestDesc->m_StreamInfo[i].fAlpha >= 0.0f )
        {
            hr = pVMRMixerControl->GetAlpha( i, &fAlpha );
            CHECKHR( hr, TEXT("GetAlpha...") );
            if ( fAlpha != pTestDesc->m_StreamInfo[i].fAlpha )
            {
                LOG( TEXT("%S: ERROR %d@%S - Alpha mismatched for stream %d! (%f, %f)" ), 
                            __FUNCTION__, __LINE__, __FILE__, i, fAlpha, pTestDesc->m_StreamInfo[i].fAlpha );
                ret = FALSE;
            }
        }
        if ( pTestDesc->m_StreamInfo[i].iZOrder >= 0 )
        {
            hr = pVMRMixerControl->GetZOrder( i, &dwZOrder );
            CHECKHR( hr, TEXT(" GetZOrder ...") );
            if ( dwZOrder != (DWORD)(pTestDesc->m_StreamInfo[i].iZOrder) )
            {
                LOG( TEXT("%S: ERROR %d@%S - Z order mismatched for stream %d! (%d, %d)" ), 
                            __FUNCTION__, __LINE__, __FILE__, i, dwZOrder, pTestDesc->m_StreamInfo[i].iZOrder );
                ret = FALSE;
            }
        }
        if ( pTestDesc->m_StreamInfo[i].nrOutput.left >= 0.0f ||
            pTestDesc->m_StreamInfo[i].nrOutput.top >= 0.0f || 
            pTestDesc->m_StreamInfo[i].nrOutput.right >= 0.0f ||
            pTestDesc->m_StreamInfo[i].nrOutput.bottom >= 0.0f )
        {
            hr = pVMRMixerControl->GetOutputRect( i, &nrRect );
            CHECKHR( hr, TEXT(" GetOutputRect..." ) );
            if ( pTestDesc->m_StreamInfo[i].nrOutput.left != nrRect.left ||
                pTestDesc->m_StreamInfo[i].nrOutput.top != nrRect.top ||
                pTestDesc->m_StreamInfo[i].nrOutput.right != nrRect.right ||
                pTestDesc->m_StreamInfo[i].nrOutput.bottom != nrRect.bottom )
            {
                LOG( TEXT("%S: ERROR %d@%S - NRect mismatched for stream %d! (%f:%f:%f:%f, %f:%f:%f:%f)" ), 
                            __FUNCTION__, __LINE__, __FILE__, i, 
                            nrRect.left, nrRect.top, nrRect.right, nrRect.bottom,
                            pTestDesc->m_StreamInfo[i].nrOutput.left,
                            pTestDesc->m_StreamInfo[i].nrOutput.top,
                            pTestDesc->m_StreamInfo[i].nrOutput.right,
                            pTestDesc->m_StreamInfo[i].nrOutput.bottom );
                ret = FALSE;
            }
        }
    }

cleanup:
    if ( FAILED(hr) )
        ret = FALSE;

    return ret;
}

BOOL
SetStreamParameters( IVMRMixerControl *pVMRMixerControl, VMRTestDesc *pTestDesc )
{
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

cleanup:

    return FAILED(hr) ? FALSE : TRUE;
}
