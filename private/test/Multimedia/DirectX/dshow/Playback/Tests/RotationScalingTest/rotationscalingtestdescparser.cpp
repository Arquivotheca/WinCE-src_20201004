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

#include "logging.h"
#include "globals.h"
#include "TestDescParser.h"
#include "BaseVRTestDescParser.h"
#include "RotationScalingTestDescParser.h"

const char* const STR_StoppedWndScaleList = "StoppedWindowScaleList";
const char* const STR_DynamicWndScaleList = "DynamicWindowScaleList";
const char* const STR_PausedWndScaleList = "PausedWindowScaleList";
const char* const STR_WndFullScreen = "VideoFullScreen";
const char* const STR_RotateConfig = "RotateConfig";
const char* const STR_StoppedRotateConfigList = "StoppedRotateConfigList";
const char* const STR_DynamicRotateConfigList = "DynamicRotateConfigList";
const char* const STR_PausedRotateConfigList = "PausedRotateConfigList";
const char* const STR_SetLog = "SetLog";
const char* const STR_NotUpdateWindow = "NotUpdateWindow";
const char* const STR_WindowSizeChange = "WindowSizeChange";

const char* const STR_PrefUpstreamRotate = "PrefUpstreamRotate";
const char* const STR_PrefUpstreamScale = "PrefUpstreamScale";
const char* const STR_StepSize = "StepSize";
const char* const STR_TestSteps = "TestSteps";

HRESULT 
ParseRotateConfigList(CXMLElement *hElement, RotateConfigList* pRotateConfigList)
{
    HRESULT hr = S_OK;
    CXMLElement * hXmlChild = 0;

    if ( !hElement || !pRotateConfigList ) 
        return E_POINTER;

    const char* szRotateConfigList = hElement->GetValue();
    if (!szRotateConfigList)
        return S_OK;
    
    char szAngle[32];
    int listmarker = 0;
    
    while(listmarker != -1)
    {
        // Read the next window scaleition
        hr = ParseStringListString(szRotateConfigList, WNDSCALE_LIST_DELIMITER, szAngle, countof(szAngle), listmarker);
        if (FAILED(hr))
            break;

        AM_ROTATION_ANGLE rotateConfig;
        
        if (!strcmp(szAngle, "0"))
            rotateConfig = AM_ROTATION_ANGLE_0;
        else if (!strcmp(szAngle, "90"))
            rotateConfig = AM_ROTATION_ANGLE_90;
        else if (!strcmp(szAngle, "180"))
            rotateConfig = AM_ROTATION_ANGLE_180;
        else if (!strcmp(szAngle, "270"))
            rotateConfig = AM_ROTATION_ANGLE_270;            
        else {
            return E_FAIL;
        }
        pRotateConfigList->push_back(rotateConfig);
    }
    
    return hr;
}


HRESULT 
ParseRotationScalingTestDesc( CXMLElement *hElement, TestDesc** ppTestDesc )
{
    HRESULT hr = S_OK;
    CXMLElement *hXmlChild = 0;
    DWORD dwID = 0;

    if ( !hElement || !ppTestDesc ) 
        return E_POINTER;

    RotationScalingTestDesc* pRotationScalingTestDesc = new RotationScalingTestDesc();
    if ( !pRotationScalingTestDesc )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to allocate RotationScalingTestDesc object when parsing."), 
                        __FUNCTION__, __LINE__, __FILE__ );
        hr = E_OUTOFMEMORY;
        return hr;
    }

    hXmlChild = hElement->GetFirstChildElement();
    if ( hXmlChild == 0 )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to find first test desc element."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return E_FAIL;
    }

    PCSTR pElemName = 0x0;
    while ( SUCCEEDED(hr) && hXmlChild )
    {
        // First parse the common test configuration data: test id, media, verification list, ...
        hr = ParseCommonTestInfo( hXmlChild, pRotationScalingTestDesc );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to parse the common test data."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            break;
        }

        pElemName = hXmlChild->GetName();
        if ( !pElemName ) 
        {
            hr = E_FAIL;
            break;
        }
    
        if ( !strcmp( STR_PrefUpstreamRotate, pElemName ) )
        {
            pRotationScalingTestDesc->bPrefUpstreamRotate = true;
        }

        if ( !strcmp( STR_PrefUpstreamScale, pElemName ) )
        {
            pRotationScalingTestDesc->bPrefUpstreamScale = true;
        }

        //Set Render Pref Surface
        if ( !strcmp(STR_RenderingPrefsSurface, pElemName) )
        {
            hr = ParseRenderPrefSurface( hXmlChild, &pRotationScalingTestDesc->m_dwRenderingPrefsSurface);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to Render Pref Surface."), 
                            __FUNCTION__, __LINE__, __FILE__ );
                break;
            }
        }

        // Parse the source rect
        if (!strcmp(STR_SrcRect, pElemName))
        {
            hr = ParseRect(hXmlChild, &pRotationScalingTestDesc->srcRect);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the verification list."), __FUNCTION__, __LINE__, __FILE__);
                break;
            }
            else
                pRotationScalingTestDesc->bSrcRect = true;
        }

        // Parse the destination rect
        if (!strcmp(STR_DstRect, pElemName))
        {
            hr = ParseRect(hXmlChild, &pRotationScalingTestDesc->destRect);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the verification list."), __FUNCTION__, __LINE__, __FILE__);
                break;
            }
            else
                pRotationScalingTestDesc->bDestRect = true;
        }

        // Parse the multiple window scale list
        if (!strcmp(STR_StoppedWndScaleList, pElemName))
        {
            // WndScaleList has the same format as WndPosList so use the same parse method
            hr = ParseWndScaleList(hXmlChild, &pRotationScalingTestDesc->multiScaleList);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the verification list."), __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }

        // Parse the dynamic window scale list
        if (!strcmp(STR_DynamicWndScaleList, pElemName))
        {
            // WndScaleList has the same format as WndPosList so use the same parse method
            hr = ParseWndScaleList(hXmlChild, &pRotationScalingTestDesc->dynamicScaleList);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the verification list."), __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }

        // Parse the paused window scale list
        if (!strcmp(STR_PausedWndScaleList, pElemName))
        {
            // WndScaleList has the same format as WndPosList so use the same parse method
            hr = ParseWndScaleList(hXmlChild, &pRotationScalingTestDesc->pausedScaleList);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the verification list."), __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }

        // Parse the Multiple Rotate Configs
        if (!strcmp(STR_StoppedRotateConfigList, pElemName))
        {
            hr = ParseRotateConfigList(hXmlChild, &pRotationScalingTestDesc->multiRotateConfigList);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse ParseRotateConfigList flag."), __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }
        
        // Parse the Dynamic Rotate Configs
        if (!strcmp(STR_DynamicRotateConfigList, pElemName))
        {
            hr = ParseRotateConfigList(hXmlChild, &pRotationScalingTestDesc->dynamicRotateConfigList);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse ParseRotateConfigList flag."), __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }

        // Parse the Paused Rotate Configs
        if (!strcmp(STR_PausedRotateConfigList, pElemName))
        {
            hr = ParseRotateConfigList(hXmlChild, &pRotationScalingTestDesc->pausedRotateConfigList);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse ParseRotateConfigList flag."), __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }
        
        // Parse the SetLog flag 
        if (!strcmp(STR_SetLog, pElemName))
        {
            pRotationScalingTestDesc->bSetLog = true;
        }

        // Parse the NotUpdateWindow flag 
        if (!strcmp(STR_NotUpdateWindow, pElemName))
        {
            pRotationScalingTestDesc->m_bUpdateWindow = false;
        }

        // Parse Check Using Overlays 
        if (!strcmp(STR_CheckUsingOverlay, pElemName))
        {
            pRotationScalingTestDesc->bCheckUsingOverlay= true;
        }

        if ( !strcmp(STR_Verification, pElemName))
        {
            hr = ParseVerification(hXmlChild, &pRotationScalingTestDesc->verificationList);
            if ( FAILED(hr))
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the verification list."), 
                                __FUNCTION__, __LINE__, __FILE__ );
                break;
            }
        }

        if ( !strcmp(STR_SrcImgPath, pElemName) )
        {
            hr = ParseString( hXmlChild, pRotationScalingTestDesc->szSrcImgPath, countof(pRotationScalingTestDesc->szSrcImgPath));
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the source image path."), 
                                __FUNCTION__, __LINE__, __FILE__ );
                break;
            }
        }

        if ( !strcmp(STR_DestImgPath, pElemName) )
        {
            hr = ParseString( hXmlChild, pRotationScalingTestDesc->szDestImgPath, countof(pRotationScalingTestDesc->szDestImgPath));
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the destination image path."), 
                                __FUNCTION__, __LINE__, __FILE__ );
                break;
            }
        }

        // Parse the Pass Rate
        if (!strcmp(STR_PassRate, pElemName))
        {
            hr = ParseDWORD(hXmlChild, &pRotationScalingTestDesc->m_dwPassRate);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the pass rate numbers."), __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }

        // Parse the Window size change
        if (!strcmp(STR_WindowSizeChange, pElemName))
        {
            hr = ParseDWORD(hXmlChild, &pRotationScalingTestDesc->m_dwWindowSizeChange);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the window size change."), __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }
        // Parse the window position list
        if (!strcmp(STR_WndPosList, pElemName))
        {
            hr = ParseWndPosList(hXmlChild, pRotationScalingTestDesc->GetVWPositions() );
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the video window position list."), 
                        __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }

        // Parse VMR specific information: stream info and alpha bitmap info
        if ( !strcmp(STR_MixingMode, pElemName) )
        {
            pRotationScalingTestDesc->m_bMixerMode = TRUE;
        }

        if ( !strcmp( STR_RenderingMode, pElemName ) )
        {
            if ( !strcmp( "Windowed", hXmlChild->GetValue() ) )
                pRotationScalingTestDesc->m_dwRenderingMode = VMRMode_Windowed;
            else if ( !strcmp( "Windowless", hXmlChild->GetValue() ) )
                pRotationScalingTestDesc->m_dwRenderingMode = VMRMode_Windowless;
            else if ( !strcmp( "Renderless", hXmlChild->GetValue() ) )
                pRotationScalingTestDesc->m_dwRenderingMode = VMRMode_Renderless;
            else
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the rendering mode."), 
                        __FUNCTION__, __LINE__, __FILE__);
                hr = E_FAIL;
                break;
            }
        }

        if ( !strcmp(STR_NumOfStreams, pElemName) )
        {
            hr = ParseDWORD(hXmlChild, &pRotationScalingTestDesc->m_dwStreams );
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the number of streams."), 
                        __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }
        if ( !strcmp(STR_AlphaBitmap, pElemName) )
        {
            hr = ParseAlphaBitmap(hXmlChild, &pRotationScalingTestDesc->m_AlphaBitmap );
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the alpha bitmap."), 
                        __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }
        
        if ( !strcmp(STR_StreamInfo, pElemName) )
        {
            if ( dwID >= VMR_MAX_INPUT_STREAMS ) 
                LOG(TEXT("%S: ERROR %d@%S - Extra streams ignored."), 
                        __FUNCTION__, __LINE__, __FILE__);
            else
            {
                hr = ParseStreamInfo(hXmlChild, &pRotationScalingTestDesc->m_StreamInfo[dwID] );
                if (FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - failed to parse the stream info."), 
                            __FUNCTION__, __LINE__, __FILE__);
                    break;
                }
                dwID++;
            }
        }

        if (!strcmp(STR_TestMode, pElemName))
        {
            hr = ParseTestMode(hXmlChild, &pRotationScalingTestDesc->m_vmrTestMode);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the test mode."), 
                        __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }

        if ( !strcmp(STR_StepSize, pElemName) )
        {
            hr = ParseINT32( hXmlChild, (INT32*)(&pRotationScalingTestDesc->m_lStepSize));
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the step size."), 
                                __FUNCTION__, __LINE__, __FILE__ );
                break;
            }
        }

        if ( !strcmp(STR_TestSteps, pElemName) )
        {
            hr = ParseDWORD( hXmlChild, &pRotationScalingTestDesc->m_dwTestSteps);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the test steps."), 
                                __FUNCTION__, __LINE__, __FILE__ );
                break;
            }
        }

        if ( SUCCEEDED(hr))
            hXmlChild = hXmlChild->GetNextSiblingElement();
    }

    if ( SUCCEEDED(hr) )
        *ppTestDesc = pRotationScalingTestDesc;
    else
        delete pRotationScalingTestDesc;

    return hr;
}

