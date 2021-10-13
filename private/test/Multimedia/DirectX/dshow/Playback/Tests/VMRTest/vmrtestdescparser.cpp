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
#include "VMRTestDescParser.h"

// VMR configuration
const char* const STR_VMRState = "VMRState";
const char* const STR_VMRPreConifg = "PreConfig";
const char* const STR_VMRPreConnect = "PreConnect";
const char* const STR_VMRPreAllocate = "PreAllocate";
const char* const STR_VMRPaused = "Paused";
const char* const STR_VMRRunning = "Running";
const char* const STR_VMRStopped = "Stopped";

const char* const STR_RunFirst = "RunFirst";
const char* const STR_PauseFirst = "PauseFirst";
const char* const STR_RunInMiddle= "RunInMiddle";
const char* const STR_PauseInMiddle = "PauseInMiddle";
const char* const STR_CancelStep = "CancelStep";
const char* const STR_StepSize = "StepSize";
const char* const STR_FirstStepSize = "FirstStepSize";
const char* const STR_TestSteps = "TestSteps";

HRESULT ParseVMRTestDesc(CXMLElement *hElement, TestDesc** ppTestDesc)
{
    HRESULT hr = S_OK;
    CXMLElement *hXmlChild = 0;
    DWORD dwID = 0;

    if ( !hElement || !ppTestDesc ) return E_FAIL;

    VMRTestDesc* pVMRTestDesc = new VMRTestDesc();
    if (!pVMRTestDesc)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to allocate VMRTestDesc object when parsing."), 
                    __FUNCTION__, __LINE__, __FILE__);
        hr = E_OUTOFMEMORY;
        return hr;
    }

    *ppTestDesc = pVMRTestDesc;

    hXmlChild = hElement->GetFirstChildElement();
    if (!hXmlChild)
        return S_OK;

    while ( SUCCEEDED(hr) && hXmlChild )
    {
        // First parse the common test configuration data: test id, media, verification list, ...
        hr = ParseCommonTestInfo( hXmlChild, pVMRTestDesc );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to parse the common test data."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            break;
        }
        PCSTR pName = hXmlChild->GetName();

        if ( !strcmp(STR_UseVideoRenderer, pName) )
        {
            pVMRTestDesc->SetUseVR( true );
        }

        if (!strcmp(STR_State, pName))
        {
            FILTER_STATE tmpState = State_Stopped;
            hr = ParseState(hXmlChild, &tmpState );
            if ( FAILED(hr) )
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the test state."), 
                            __FUNCTION__, __LINE__, __FILE__);
                break;
            }
            else
            {
                pVMRTestDesc->SetStatePresent( true );
                pVMRTestDesc->SetTestState( tmpState );
            }
        }

        // parse VMR state
        if ( !strcmp(STR_VMRState, pName) )
        {
            if ( !strcmp( STR_VMRPreConifg, hXmlChild->GetValue() ) )
            {
                pVMRTestDesc->SetVMRTestState( TESTSTATE_PRE_VMRCONFIG );
            }
            else if ( !strcmp( STR_VMRPreConnect, hXmlChild->GetValue() ) )
            {
                pVMRTestDesc->SetVMRTestState( TESTSTATE_PRE_CONNECTION );
            }
            else if ( !strcmp( STR_VMRPreAllocate, hXmlChild->GetValue() ) )
            {
                pVMRTestDesc->SetVMRTestState( TESTSTATE_PRE_ALLOCATION );
            }
            else if ( !strcmp( STR_VMRPaused, hXmlChild->GetValue() ) )
            {
                pVMRTestDesc->SetVMRTestState( TESTSTATE_PAUSED );
            }
            else if ( !strcmp( STR_VMRRunning, hXmlChild->GetValue() ) )
            {
                pVMRTestDesc->SetVMRTestState( TESTSTATE_RUNNING );
            }
            else if ( !strcmp( STR_VMRStopped, hXmlChild->GetValue() ) )
            {
                pVMRTestDesc->SetVMRTestState( TESTSTATE_STOPPED );
            }
        }


        // parse the state change list
        if (!strcmp(STR_StateList, pName))
        {
            hr = ParseStateList( hXmlChild, pVMRTestDesc->GetTestStateList() );
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the test state list."), 
                            __FUNCTION__, __LINE__, __FILE__);
                break;
            }
            else
                pVMRTestDesc->SetStatePresent( true );
        }

        // Parse the source rect
        if (!strcmp(STR_SrcRect, pName))
        {
            hr = ParseRect(hXmlChild, pVMRTestDesc->GetSrcDestRect() );
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the src rect."), 
                            __FUNCTION__, __LINE__, __FILE__);
                break;
            }

            else
                pVMRTestDesc->SetSrcDestRectFlag( true );
        }

        // Parse the destination rect
        if (!strcmp(STR_DstRect, pName))
        {
            hr = ParseRect(hXmlChild, pVMRTestDesc->GetSrcDestRect(false) );
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the dest rect."), 
                            __FUNCTION__, __LINE__, __FILE__);
                break;
            }
            else
                pVMRTestDesc->SetSrcDestRectFlag( true, false );
        }

        // Parse the window position list
        if (!strcmp(STR_WndPosList, pName))
        {
            hr = ParseWndPosList(hXmlChild, pVMRTestDesc->GetVWPositions() );
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the video window position list."), 
                            __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }

        // Parse the window scale list
        if (!strcmp(STR_WndScaleList, pName))
        {
            // WndScaleList has the same format as WndPosList so use the same parse method
            hr = ParseWndScaleList(hXmlChild, pVMRTestDesc->GetVWScaleLists() );
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the video window scale list."), 
                            __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }

        // Parse the window rectangle list
        if (!strcmp(STR_WndRectList, pName))
        {
            hr = ParseWndRectList(hXmlChild, pVMRTestDesc->GetTestWinPositions() );
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the test window position list."), 
                            __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }

        // Parse the source video rectangle list
        if (!strcmp(STR_SrcRectList, pName))
        {
            // srcRectList has the same format as WndRectList so use the same parse method
            hr = ParseWndRectList( hXmlChild, pVMRTestDesc->GetSrcDestRectList() );
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the srce rect list."), 
                            __FUNCTION__, __LINE__, __FILE__);
                break;
            }
            else
                pVMRTestDesc->SetSrcDestRectFlag( true );
        }

        // Parse the destination video rectangle list
        if (!strcmp(STR_DstRectList, pName))
        {
            // destRectList has the same format as WndRectList so use the same parse method
            hr = ParseWndRectList( hXmlChild, pVMRTestDesc->GetSrcDestRectList(false) );
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the dest rect list."), 
                            __FUNCTION__, __LINE__, __FILE__);
                break;
            }
            else
                pVMRTestDesc->SetSrcDestRectFlag( true, false);
        }

        // Parse VMR specific information: stream info and alpha bitmap info
        if ( !strcmp(STR_MixingMode, pName) )
        {
            pVMRTestDesc->m_bMixerMode = TRUE;
        }

        if ( !strcmp( STR_RenderingMode, pName ) )
        {
            if ( !strcmp( "Windowed", hXmlChild->GetValue() ) )
                pVMRTestDesc->m_dwRenderingMode = VMRMode_Windowed;
            else if ( !strcmp( "Windowless", hXmlChild->GetValue() ) )
                pVMRTestDesc->m_dwRenderingMode = VMRMode_Windowless;
            else if ( !strcmp( "Renderless", hXmlChild->GetValue() ) )
                pVMRTestDesc->m_dwRenderingMode = VMRMode_Renderless;
            else
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the rendering mode."), 
                            __FUNCTION__, __LINE__, __FILE__);
                hr = E_FAIL;
                break;
            }
        }

        if ( !strcmp(STR_NumOfStreams, pName) )
        {
            hr = ParseDWORD(hXmlChild, &pVMRTestDesc->m_dwStreams );
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the number of streams."), 
                            __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }
        if ( !strcmp(STR_AlphaBitmap, pName) )
        {
            hr = ParseAlphaBitmap(hXmlChild, &pVMRTestDesc->m_AlphaBitmap );
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the alpha bitmap."), 
                            __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }
        if ( !strcmp(STR_StreamInfo, pName) )
        {
            if ( dwID >= VMR_MAX_INPUT_STREAMS ) 
                LOG(TEXT("%S: ERROR %d@%S - Extra streams ignored."), 
                            __FUNCTION__, __LINE__, __FILE__);
            else
            {
                hr = ParseStreamInfo(hXmlChild, &pVMRTestDesc->m_StreamInfo[dwID] );
                if (FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - failed to parse the stream info."), 
                                __FUNCTION__, __LINE__, __FILE__);
                    break;
                }
                dwID++;
            }
        }

        //Set Render Pref Surface
        if ( !strcmp(STR_RenderingPrefsSurface, pName) )
        {
            hr = ParseRenderPrefSurface( hXmlChild, &pVMRTestDesc->m_dwRenderingPrefsSurface);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to Render Pref Surface."), 
                            __FUNCTION__, __LINE__, __FILE__ );
                break;
            }
        }

        if ( !strcmp(STR_SrcImgPath, pName) )
        {
            hr = ParseString( hXmlChild, pVMRTestDesc->szSrcImgPath, countof(pVMRTestDesc->szSrcImgPath));
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the source image path."), 
                                __FUNCTION__, __LINE__, __FILE__ );
                break;
            }
        }

        if ( !strcmp(STR_DestImgPath, pName) )
        {
            hr = ParseString( hXmlChild, pVMRTestDesc->szDestImgPath, countof(pVMRTestDesc->szDestImgPath));
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the destination image path."), 
                                __FUNCTION__, __LINE__, __FILE__ );
                break;
            }
        }

        // Parse the Pass Rate
        if (!strcmp(STR_PassRate, pName))
        {
            hr = ParseDWORD(hXmlChild, &pVMRTestDesc->m_dwPassRate);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the pass rate numbers."), __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }

        if (!strcmp(STR_TestMode, pName))
        {
            hr = ParseTestMode(hXmlChild, &pVMRTestDesc->m_vmrTestMode);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the test mode."), 
                        __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }

        if (!strcmp(STR_RunFirst, pName))
        {
            pVMRTestDesc->m_bRunFirst= true;
        }

        if (!strcmp(STR_PauseFirst, pName))
        {
            pVMRTestDesc->m_bPauseFirst= true;
        }

        if (!strcmp(STR_RunInMiddle, pName))
        {
            pVMRTestDesc->m_bRunInMiddle= true;
        }

        if (!strcmp(STR_PauseInMiddle, pName))
        {
            pVMRTestDesc->m_bPauseInMiddle= true;
        }

        if (!strcmp(STR_CancelStep, pName))
        {
            pVMRTestDesc->m_bCancelStep= true;
        }

        if ( !strcmp(STR_StepSize, pName) )
        {
            hr = ParseINT32( hXmlChild, (INT32*)(&pVMRTestDesc->m_lStepSize));
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the step size."), 
                                __FUNCTION__, __LINE__, __FILE__ );
                break;
            }
        }

        if ( !strcmp(STR_FirstStepSize, pName) )
        {
            hr = ParseINT32( hXmlChild, (INT32*)(&pVMRTestDesc->m_lFirstStepSize));
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the first step size."), 
                                __FUNCTION__, __LINE__, __FILE__ );
                break;
            }
        }

        if ( !strcmp(STR_TestSteps, pName) )
        {
            hr = ParseDWORD( hXmlChild, &pVMRTestDesc->m_dwTestSteps);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the test steps."), 
                                __FUNCTION__, __LINE__, __FILE__ );
                break;
            }
        }

        // Get next sibling element
        if ( SUCCEEDED(hr) )
            hXmlChild = hXmlChild->GetNextSiblingElement();        
    }

    if (FAILED(hr))
    {
        delete pVMRTestDesc;
        *ppTestDesc = NULL;
    }

    return hr;
}
