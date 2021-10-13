//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//

#include "main.h"

#include <DDrawUty.h>
#include <QATestUty/WinApp.h>
#include <list>

#include "DDrawTK.h"

////////////////////////////////////////////////////////////////////////////////
// Macros
//  
#if CFG_TEST_INVALID_PARAMETERS
    #define INVALID_PARAM_TEST_RET(__CALL, __HREXPECTED) \
        hr = __CALL; \
        QueryForHRESULT(hr, __HREXPECTED, TEXT(#__CALL), tr |= trFail)
    #define INVALID_PARAM_TEST(__CALL) \
        INVALID_PARAM_TEST_RET(__CALL, DDERR_INVALIDPARAMS)
#endif

using namespace com_utilities;
using namespace DebugStreamUty;
using namespace DDrawUty;

namespace priv_Test_DirectDrawVideoPort
{
    HRESULT CALLBACK GetSingleCapsCallback(LPDDVIDEOPORTCAPS lpDDVideoPortCaps, LPVOID pvContext)
    {
        LPDDVIDEOPORTCAPS lpDDVideoPortCapsOut = reinterpret_cast<LPDDVIDEOPORTCAPS>(pvContext);

        CheckCondition(NULL == lpDDVideoPortCapsOut, "NULL pvContext", DDENUMRET_CANCEL);
        memcpy(lpDDVideoPortCapsOut, lpDDVideoPortCaps, sizeof(*lpDDVideoPortCaps));
        return DDENUMRET_CANCEL;
    }    
}

namespace Test_DirectDrawVideoPort
{
    eTestResult CTest_IterateVideoPortStates::TestIDDVideoPortContainer()
    {
        CDDVideoPortDesc cddvpd;
        eTestResult tr = trPass;
        HRESULT hr;

        // A list of the possible video port types
        const GUID * vpGuids[] = {
            &DDVPTYPE_BROOKTREE,
            &DDVPTYPE_PHILIPS,
            &DDVPTYPE_E_HREFH_VREFH,
            &DDVPTYPE_E_HREFH_VREFL,
            &DDVPTYPE_E_HREFL_VREFH,
            &DDVPTYPE_E_HREFL_VREFL,
            &DDVPTYPE_CCIR656 
        };
         
        // If video ports are not supported, then just skip this test
        if (!(DDCAPS2_VIDEOPORT & g_DDConfig.HALCaps().dwCaps2) &&
            !(DDCAPS2_VIDEOPORT & g_DDConfig.HELCaps().dwCaps2))
        {
            return trSkip;
        }
        
        // Set up a reasonable video port description
        cddvpd.dwFieldWidth = 640;
        cddvpd.dwFieldHeight = 240;
        cddvpd.dwVBIWidth = 0;
        cddvpd.dwMicrosecondsPerField = 1000 * 1000 / 60; // ???
        cddvpd.dwMaxPixelsPerSecond = 0xFFFFFFF7; // ????
        cddvpd.dwVideoPortID = 0;
        cddvpd.VideoPortType.dwPortWidth = 8;
        cddvpd.VideoPortType.dwFlags = DDVPCONNECT_VACT | DDVPCONNECT_INTERLACED;

        // Set up a reasonable overlay surface description
        m_cddsdOverlay.dwBackBufferCount = 0;
        m_cddsdOverlay.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
        m_cddsdOverlay.ddsCaps.dwCaps = DDSCAPS_OVERLAY | DDSCAPS_VIDEOMEMORY | DDSCAPS_VIDEOPORT;
        m_cddsdOverlay.dwWidth = 640;
        m_cddsdOverlay.dwHeight = 240;
        m_cddsdOverlay.ddpfPixelFormat.dwSize = sizeof(m_cddsdOverlay.ddpfPixelFormat);
        m_cddsdOverlay.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
        m_cddsdOverlay.ddpfPixelFormat.dwFourCC = MAKEFOURCC('Y','U','Y','V');

        // Get the caps for the video port we're goint to create into m_cddvpcaps
        CDDVideoPortCaps cddvpcaps;
        cddvpcaps.dwFlags = DDVPD_ID;
        cddvpcaps.dwVideoPortID = cddvpd.dwVideoPortID;
        m_cddvpcaps.Preset();
        hr = m_piDDVPC->EnumVideoPorts(0, &cddvpcaps, &m_cddvpcaps, priv_Test_DirectDrawVideoPort::GetSingleCapsCallback);
        CheckHRESULT(hr, "EnumVideoPorts", trAbort);
        CheckCondition(m_cddvpcaps.IsPreset(), "Failed to get Caps", trFail);
        //dbgout << "Caps = " << m_cddvpcaps << endl;
        
        // Create the video port
        // =====================
        dbgout << "Creating Port with Flags = 0" << endl;
        for(int index = 0; index < countof(vpGuids); index++)
        {
            cddvpd.VideoPortType.guidTypeID = *vpGuids[index];
            hr = m_piDDVPC->CreateVideoPort(0 /*DDVPCREATE_VIDEOONLY*/, &cddvpd, m_piDDVP.AsTestedOutParam(), NULL);
            if (SUCCEEDED(hr))
                break;
            CheckForHRESULT(hr, DDERR_INVALIDPARAMS, "CreateVideoPort", trAbort);
        }

        CheckHRESULT(hr, "CreateVideoPort", trAbort);
        CheckCondition(m_piDDVP.InvalidOutParam(), "CreateVideoPort didn't fill out param", trAbort);
        dbgout << "Testing with no surface" << indent;
        m_State = vpsNoSurface;
        tr |= RunVideoPortTest();
        dbgout << unindent << "Done no surface test" << endl;

        // Create and attach a target surface
        // ==================================
        dbgout << "Creating overlay surface" << endl;
        hr = m_piDD->CreateSurface(&m_cddsdOverlay, m_piDDSOverlay.AsTestedOutParam(), NULL);
        CheckHRESULT(hr, "Create Overlay Surface", trAbort);
        CheckCondition(m_piDDSOverlay.InvalidOutParam(), "CreateSurface didn't fill out param", trAbort);

        // Set the target surface
        dbgout << "Setting target surface" << endl;
        hr = m_piDDVP->SetTargetSurface(m_piDDSOverlay.AsInParam(), DDVPTARGET_VIDEO);
        CheckHRESULT(hr, "SetTargetSurface", trAbort);

        // Create Primary and make overlay visible
        // =======================================
        CComPointer<IDirectDrawSurface> piDDSPrimary;
        piDDSPrimary = g_DDSPrimary.GetObject();
        CheckCondition(piDDSPrimary == NULL, "Unable to get Primary", trAbort);

        RECT rc = { 0, 0, 640, 480 };
        hr = m_piDDSOverlay->UpdateOverlay(/*&rc*/ NULL, piDDSPrimary.AsInParam(), /*&rc*/ NULL, DDOVER_SHOW, NULL);
        CheckHRESULT(hr, "UpdateOverlay(DDOVER_SHOW)", trAbort);

        Sleep(1000);
        dbgout << "Testing with target surface, but no signal" << indent;
        m_State = vpsNoSignal;
        tr |= RunVideoPortTest();
        dbgout << unindent << "Done no signal test" << endl;

        // Run Test with video playing
        // ===========================
        CDDVideoPortInfo cddvpi;
        CDDPixelFormat cddpf;

        cddvpi.dwOriginX = 0;
        cddvpi.dwOriginY = 0;
        cddvpi.dwVPFlags = 0;
        cddvpi.lpddpfInputFormat = &cddpf;
        
        cddpf.dwFlags = DDPF_FOURCC;
        cddpf.dwFourCC = MAKEFOURCC('Y','U','Y','V');

        dbgout << "Starting video" << endl;
        hr = m_piDDVP->StartVideo(&cddvpi);
        Sleep(1000);
        CheckHRESULT(hr, "StartVideo", trAbort);
        m_State = vpsSignalStarted;
        dbgout << "Testing with running video" << indent;
        tr |= RunVideoPortTest();
        dbgout << unindent << "Done running video test" << endl;

        hr = m_piDDSOverlay->UpdateOverlay(/*&rc*/ NULL, piDDSPrimary.AsInParam(), /*&rc*/ NULL, DDOVER_SHOW, NULL);
        CheckHRESULT(hr, "UpdateOverlay(DDOVER_SHOW)", trAbort);

        // Run Test with video updated
        // ===========================
        dbgout << "Updating Video" << endl;
        hr = m_piDDVP->UpdateVideo(&cddvpi);
        Sleep(1000);
        CheckHRESULT(hr, "UpdateVideo", trAbort);
        dbgout << "Testing with updated video" << indent;
        tr |= RunVideoPortTest();
        dbgout << unindent << "Done updated video test" << endl;

        // Run Test with video stopped
        // ===========================
        dbgout << "Stopping Video" << endl;
        hr = m_piDDVP->StopVideo();
        Sleep(1000);
        CheckHRESULT(hr, "StopVideo", trAbort);
        m_State = vpsSignalStopped;
        dbgout << "Testing with stopped video" << indent;
        tr |= RunVideoPortTest();
        dbgout << unindent << "Done stopped video test" << endl;

        // Run Test with video updated
        // ===========================
        dbgout << "Updating Video" << endl;
        hr = m_piDDVP->UpdateVideo(&cddvpi);
        Sleep(1000);
        CheckHRESULT(hr, "UpdateVideo", trAbort);
        dbgout << "Testing with updated video" << indent;
        tr |= RunVideoPortTest();
        dbgout << unindent << "Done updated video test" << endl;

        // Unhook overlay from Primary
        hr = m_piDDSOverlay->UpdateOverlay(NULL, piDDSPrimary.AsInParam(), NULL, DDOVER_HIDE, NULL);
        CheckHRESULT(hr, "UpdateOverlay(DDOVER_HIDE)", trAbort);
        
        return tr;
    }
}

namespace Test_IDirectDrawVideoPort
{
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetBandwidthInfo::TestIDirectDrawVideoPort
    //  Tests the GetBandwidthInfo Method.
    //
    eTestResult CTest_GetBandwidthInfo::TestIDirectDrawVideoPort()
    {
        CDDVideoPortBandwidth cddvpb;
        eTestResult tr = trPass;
        HRESULT hr;

#       if CFG_TEST_INVALID_PARAMETERS
            dbgout << "Testing invalid args" << endl;
            INVALID_PARAM_TEST(m_piDDVP->GetBandwidthInfo(NULL, 0, 0, DDVPB_TYPE, &cddvpb));
            INVALID_PARAM_TEST(m_piDDVP->GetBandwidthInfo(&m_cddsdOverlay.ddpfPixelFormat, 0, 0, DDVPB_TYPE, NULL));
            INVALID_PARAM_TEST(m_piDDVP->GetBandwidthInfo(&m_cddsdOverlay.ddpfPixelFormat, 0, 0, 0, &cddvpb));
            INVALID_PARAM_TEST(m_piDDVP->GetBandwidthInfo(NULL, 0, 0, 0, NULL));
#       endif

        dbgout << "Getting bandwidth limitation type" << endl;
        hr = m_piDDVP->GetBandwidthInfo(&m_cddsdOverlay.ddpfPixelFormat, 0, 0, DDVPB_TYPE, &cddvpb);
        CheckHRESULT(hr, "GetBandwidthInfo(DDVPB_TYPE)", trFail);
        dbgout << "Got caps = " << g_VideoPortBandwidthCapsMap[cddvpb.dwCaps] << endl;

        switch(cddvpb.dwCaps)
        {
        case DDVPBCAPS_DESTINATION:
            hr = m_piDDVP->GetBandwidthInfo(&m_cddsdOverlay.ddpfPixelFormat, m_cddsdOverlay.dwWidth,
                                            m_cddsdOverlay.dwHeight, DDVPB_VIDEOPORT, &cddvpb);
            CheckHRESULT(hr, "GetBandwidthInfo(DDVPB_VIDEOPORT)", trFail);
            break;

        case DDVPBCAPS_SOURCE:
            hr = m_piDDVP->GetBandwidthInfo(&m_cddsdOverlay.ddpfPixelFormat, m_cddsdOverlay.dwWidth,
                                            m_cddsdOverlay.dwHeight, DDVPB_OVERLAY, &cddvpb);
            CheckHRESULT(hr, "GetBandwidthInfo(DDVPB_OVERLAY)", trFail);
            break;

        default:
            QueryCondition(cddvpb.dwCaps != DDVPBCAPS_DESTINATION && cddvpb.dwCaps != DDVPBCAPS_SOURCE,
                           "Unrecognized bandwidth caps", tr |= trFail);
            break;    
        }

        dbgout << "Resulting Bandwidth : " << cddvpb << endl;
        
        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetSetColorControls::TestIDirectDrawVideoPort
    //  Tests the GetColorControls Method.
    //
    eTestResult CTest_GetSetColorControls::TestIDirectDrawVideoPort()
    {
        eTestResult tr = trPass;
        CDDColorControl cddcc1,
                        cddcc2;
        HRESULT hr;

#       if CFG_TEST_INVALID_PARAMETERS
            dbgout << "Testing invalid args" << endl;
            INVALID_PARAM_TEST(m_piDDVP->GetColorControls(NULL));
            INVALID_PARAM_TEST(m_piDDVP->SetColorControls(NULL));
#       endif

        if (m_cddvpcaps.dwCaps & DDVPCAPS_COLORCONTROL)
        {
            // ColorControls are supported -- verify that the
            // call succeeded and log the color controls
            dbgout << "Getting color controls" << endl;
            hr = m_piDDVP->GetColorControls(&cddcc1);
            CheckHRESULT(hr, "GetColorControls", trFail);
            dbgout << "Original Color Controls : " << cddcc1 << endl;

            cddcc1.dwFlags |= DDCOLOR_BRIGHTNESS | DDCOLOR_COLORENABLE | DDCOLOR_CONTRAST;
            // According to the docs, here are the defaults:
            cddcc1.lBrightness = 750;
            cddcc1.lContrast = 10000;
            cddcc1.lColorEnable = 1;

            hr = m_piDDVP->SetColorControls(&cddcc1);
            CheckHRESULT(hr, "SetColorControls", trFail);
            dbgout << "Set Color Controls : " << cddcc1 << endl;

            // Get the color controls back
            hr = m_piDDVP->GetColorControls(&cddcc2);
            CheckHRESULT(hr, "GetColorControls", trFail);
            dbgout << "Modified Color Controls : " << cddcc2 << endl;

            CheckCondition(0 == memcmp(&cddcc1, &cddcc2, sizeof(cddcc1)), "Mis-matched ColorControls", trFail);
        }
        else
        {
            // ColorControls are not supported -- make sure that
            // the call fails correctly
            dbgout  << "Caps indicate that Color Controls not supported" << endl
                    << "      -- verifying that calls fail" << endl;
            hr = m_piDDVP->GetColorControls(&cddcc1);
            CheckForHRESULT(hr, DDERR_UNSUPPORTED, "GetColorControls", trFail);
            hr = m_piDDVP->SetColorControls(&cddcc1);
            CheckForHRESULT(hr, DDERR_UNSUPPORTED, "SetColorControls", trFail);
        }

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetInputOutputFormats::TestIDirectDrawVideoPort
    //  Tests the GetInputFormats and GetOutputFormats Methods.
    //
    eTestResult CTest_GetInputOutputFormats::TestIDirectDrawVideoPort()
    {
        DWORD dwFlags = DDVPFORMAT_VIDEO;
        eTestResult tr = trPass;
        CDDPixelFormat *pddpf1 = NULL,
                       *pddpf2 = NULL,
                       *pddpfOut1 = NULL,
                       *pddpfOut2 = NULL;
        DWORD dwEntriesInput,
              dwEntriesOutput,
              dwTemp;
        HRESULT hr;
        int i = 0,
            j = 0;

#       if CFG_TEST_INVALID_PARAMETERS
            dbgout << "Testing invalid args" << endl;
            INVALID_PARAM_TEST(m_piDDVP->GetInputFormats(NULL, pddpf1, dwFlags));
            INVALID_PARAM_TEST(m_piDDVP->GetInputFormats(&dwTemp, pddpf1, 0));
            INVALID_PARAM_TEST(m_piDDVP->GetInputFormats(NULL, NULL, dwFlags));
            INVALID_PARAM_TEST(m_piDDVP->GetInputFormats(NULL, NULL, 0));
#       endif

        // Get the count of formats
        dwEntriesInput = 0;
        hr = m_piDDVP->GetInputFormats(&dwEntriesInput, NULL, dwFlags);
        QueryHRESULT(hr, "GetInputFormats with NULL lpFormats", tr |= trFail);
        dbgout << "Port has " << dwEntriesInput << " input formats" << endl;

        pddpf1 = new CDDPixelFormat[dwEntriesInput+1],
        pddpf2 = new CDDPixelFormat[dwEntriesInput+1];
        QueryCondition(NULL == pddpf1 || NULL == pddpf2, "Out of Memory", goto cleanup);

        // Set the array entries to known data
        for (i = 0; i < (int)dwEntriesInput + 1; i++)
            pddpf1[i].Preset();

        // Get the connection info offering as many entries as required
        dwTemp = dwEntriesInput;
        hr = m_piDDVP->GetInputFormats(&dwTemp, pddpf1, dwFlags);
        QueryHRESULT(hr, "GetInputFormats", tr |= trFail);

        // Verify elements past end are not modified
        QueryCondition(!pddpf1[dwEntriesInput].IsPreset(), "GetInputFormats overstepped array", tr |= trFail);

        // If there are multiple formats, test an array that's too small
        if (dwEntriesInput > 1)
        {
            DWORD dwLower = dwEntriesInput - 1;

            dbgout << "Testing asking for only " << dwLower << " entries" << endl;
            
            // Set the array entries to known data
            for (i = 0; i < (int)dwEntriesInput + 1; i++)
                pddpf2[i].Preset();
                
            // Get the connection info, offering one less entry than required
            dwTemp = dwLower;
            hr = m_piDDVP->GetInputFormats(&dwTemp, pddpf2, dwFlags);
            QueryForHRESULT(hr, DDERR_MOREDATA, "GetInputFormats didn't return DDERR_MOREDATA", tr |= trFail);
            QueryCondition(dwTemp != dwEntriesInput, "GetInputFormats didn't correct count", tr |= trFail);

            // Verify that the two arrays from the two calls match
            for (i = 0; i < (int)dwLower; i++)
                QueryCondition(0 != memcmp(&pddpf1[i], &pddpf2[i], sizeof(pddpf1[i])), "Info doesn't match", tr |= trFail);
            // Verify elements past end are not modified
            for (i = dwLower; i < (int)dwEntriesInput + 1; i++)
                QueryCondition(!pddpf2[i].IsPreset(), "GetInputFormats overstepped array", tr |= trFail);
        }

        // Set the array entries to known data
        for (i = 0; i < (int)dwEntriesInput + 1; i++)
            pddpf2[i].Preset();

        // Get the connection info, offering one more slot than required
        dwTemp = dwEntriesInput + 1;
        hr = m_piDDVP->GetInputFormats(&dwTemp, pddpf2, dwFlags);
        QueryHRESULT(hr, "GetInputFormats", tr |= trFail);
        
        // Verify that the two arrays from the two calls match
        for (i = 0; i < (int)dwEntriesInput; i++)
            QueryCondition(0 != memcmp(&pddpf1[i], &pddpf2[i], sizeof(pddpf1[i])), "Info doesn't match", tr |= trFail);
        // Verify elements past end are not modified
        QueryCondition(!pddpf2[dwEntriesInput].IsPreset(), "GetInputFormats overstepped array", tr |= trFail);

        // Verify that the two arrays from the two calls match
        for (i = 0; i < (int)dwEntriesInput; i++)
        {
            QueryCondition(0 != memcmp(&pddpf1[i], &pddpf2[i], sizeof(pddpf1[i])), "Info doesn't match", tr |= trFail);
            dbgout << "Format #" << i << ": " << pddpf1[i] << endl;

            // Test getting output formats for this input format
            dbgout << "Getting output formats for this format" << indent;

            // Get the count of formats
            dwEntriesOutput = 0;
            hr = m_piDDVP->GetOutputFormats(&pddpf1[i], &dwEntriesOutput, NULL, dwFlags);
            QueryHRESULT(hr, "GetInputFormats with NULL lpFormats", tr |= trFail);
            dbgout << "Port has " << dwEntriesOutput << " output formats" << endl;

            pddpfOut1 = new CDDPixelFormat[dwEntriesOutput+1],
            pddpfOut2 = new CDDPixelFormat[dwEntriesOutput+1];
            QueryCondition(NULL == pddpfOut1 || NULL == pddpfOut2, "Out of Memory", goto cleanup);

            // Set the array entries to known data
            for (j = 0; j < (int)dwEntriesOutput + 1; j++)
                pddpfOut1[j].Preset();


            // Get the connection info offering as many entries as required
            dwTemp = dwEntriesOutput;
            hr = m_piDDVP->GetOutputFormats(&pddpf1[i], &dwTemp, pddpfOut1, dwFlags);
            QueryHRESULT(hr, "GetOutputFormats", tr |= trFail);

            // If there are multiple formats, test an array that's too small
            if (dwEntriesOutput > 1)
            {
                DWORD dwLower = dwEntriesOutput - 1;

                dbgout << "Testing asking for only " << dwLower << " entries" << endl;
                
                // Set the array entries to known data
                for (j = 0; j < (int)dwEntriesOutput + 1; j++)
                    pddpfOut2[j].Preset();
                    
                // Get the connection info, offering one less entry than required
                dwTemp = dwLower;
                hr = m_piDDVP->GetOutputFormats(&pddpf1[i], &dwTemp, pddpfOut2, dwFlags);
                QueryForHRESULT(hr, DDERR_MOREDATA, "GetOutputFormats didn't return DDERR_MOREDATA", tr |= trFail);
                QueryCondition(dwTemp != dwEntriesOutput, "GetOutputFormats didn't correct count", tr |= trFail);
                
                // Verify that the two arrays from the two calls match
                for (j = 0; j < (int)dwLower; j++)
                    QueryCondition(0 != memcmp(&pddpfOut1[j], &pddpfOut2[j], sizeof(pddpfOut1[j])), "Info doesn't match", tr |= trFail);
                // Verify elements past end are not modified
                for (j = dwLower; j < (int)dwEntriesOutput + 1; j++)
                    QueryCondition(!pddpfOut2[j].IsPreset(), "GetOutputFormats overstepped array", tr |= trFail);
            }

            // Set the array entries to known data
            for (j = 0; j < (int)dwEntriesOutput + 1; j++)
                pddpfOut2[j].Preset();

                
            // Get the connection info, offering one more slot than required
            dwTemp = dwEntriesOutput + 1;
            hr = m_piDDVP->GetOutputFormats(&pddpf1[i], &dwTemp, pddpfOut2, dwFlags);
            QueryHRESULT(hr, "GetOutputFormats", tr |= trFail);

            // Verify that the two arrays from the two calls match
            for (j = 0; j < (int)dwEntriesOutput; j++)
                QueryCondition(pddpfOut1[j] != pddpfOut2[j], "Info doesn't match", tr |= trFail);
            // Verify elements past end are not modified
            QueryCondition(!pddpfOut2[dwEntriesOutput].IsPreset(), "GetOutputFormats overstepped array", tr |= trFail);

            for (j = 0; j < (int)dwEntriesOutput; j++)
            {
                QueryCondition(pddpfOut1[j] != pddpfOut2[j], "Info doesn't match", tr |= trFail);
                dbgout << "Format #" << j << ": " << pddpfOut1[j] << endl;
            }

            dbgout << unindent << "Done with output formats" << endl;
            
            delete[] pddpfOut2;
            pddpfOut2 = NULL;
            delete[] pddpfOut1;
            pddpfOut1 = NULL;
        }


cleanup:
        delete[] pddpfOut2;
        delete[] pddpfOut1;
        delete[] pddpf2;
        delete[] pddpf1;

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetFieldPolarity::TestIDirectDrawVideoPort
    //  Tests the GetFieldPolarity Method.
    //
    eTestResult CTest_GetFieldPolarity::TestIDirectDrawVideoPort()
    {
        eTestResult tr = trPass;
        HRESULT hr;
        BOOL fPol;

        ASSERT(m_piDDVP);

#       if CFG_TEST_INVALID_PARAMETERS
            dbgout << "Testing invalid args" << endl;
            INVALID_PARAM_TEST(m_piDDVP->GetFieldPolarity(NULL));
#       endif

        if (m_cddvpcaps.dwCaps & DDVPCAPS_READBACKFIELD)
        {
            // The GetFieldPolarity method is supported
            
            // Give a 5 second timeout
            const DWORD dwTimeOut = 5 * 1000;
            DWORD dwStartTime;
            bool fTimedOut;

            fPol = FALSE;
            dwStartTime = GetTickCount();
            dbgout << "Waiting for an even field" << endl;
            do
            {
                hr = m_piDDVP->GetFieldPolarity(&fPol);
                if (hr == DDERR_VIDEONOTACTIVE)
                {
                    dbgout << "No Active Signal -- Skipping" << endl;
                    return trSkip;
                }
                else QueryHRESULT(hr, "GetFieldPolarity", tr |= trFail);
                fTimedOut = GetTickCount() - dwStartTime > dwTimeOut;
            } while (!fPol && !fTimedOut);
            QueryCondition(fTimedOut, "Timed out waiting for even field", tr |= trFail);

            fPol = TRUE;
            dwStartTime = GetTickCount();
            dbgout << "Waiting for an odd field" << endl;
            do
            {
                hr = m_piDDVP->GetFieldPolarity(&fPol);
                if (hr == DDERR_VIDEONOTACTIVE)
                {
                    dbgout << "No Active Signal -- Skipping" << endl;
                    return trSkip;
                }
                else QueryHRESULT(hr, "GetFieldPolarity", return trFail);
                fTimedOut = GetTickCount() - dwStartTime > dwTimeOut;
            } while (fPol && !fTimedOut);
            QueryCondition(fTimedOut, "Timed out waiting for odd field", tr |= trFail);
        }
        else
        {
            hr = m_piDDVP->GetFieldPolarity(&fPol);
            CheckForHRESULT(hr, DDERR_UNSUPPORTED, "GetFieldPolarity", trFail);
        }
        
        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetVideoLine::TestIDirectDrawVideoPort
    //  Tests the GetVideoLine Method.
    //
    eTestResult CTest_GetVideoLine::TestIDirectDrawVideoPort()
    {
        eTestResult tr = trPass;
        HRESULT hr;
        int i;

#       if CFG_TEST_INVALID_PARAMETERS
            dbgout << "Testing invalid args" << endl;
            INVALID_PARAM_TEST(m_piDDVP->GetVideoLine(NULL));
#       endif

        if (m_cddvpcaps.dwCaps & DDVPCAPS_READBACKFIELD)
        {
            DWORD dwLines[1024] = { 0 };
            
            for (i = 0; i < countof(dwLines); i++)
            {
                hr = m_piDDVP->GetVideoLine(&(dwLines[i]));
                if (DDERR_VIDEONOTACTIVE == hr)
                {
                    dbgout << "No Active Signal -- Skipping" << endl;
                    return trSkip;
                }
                QueryHRESULT(hr, "GetVideoLine", return trFail);
            }
            
            dbgout << "Video Lines = ";
            for (i = 0; i < countof(dwLines); i++)
            {
                if (0 == i % 20) dbgout << endl;
                dbgout << dwLines[i] << " ";
            }
            dbgout << endl;
        }
        else
        {
            DWORD dwDummy = 0;
            hr = m_piDDVP->GetVideoLine(&dwDummy);
            CheckForHRESULT(hr, DDERR_UNSUPPORTED, "GetVideoLine", trFail);
        }

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetVideoSignalStatus::TestIDirectDrawVideoPort
    //  Tests the GetVideoSignalStatus Method.
    //
    eTestResult CTest_GetVideoSignalStatus::TestIDirectDrawVideoPort()
    {
        eTestResult tr = trPass;
        DWORD dwStatus = 0,
              dwStatusOrig = 0;
        HRESULT hr;

#       if CFG_TEST_INVALID_PARAMETERS
            dbgout << "Testing invalid args" << endl;
            INVALID_PARAM_TEST(m_piDDVP->GetVideoSignalStatus(NULL));
#       endif

        for (int i = 0; i < 20; i++)
        {
            hr = m_piDDVP->GetVideoSignalStatus(&dwStatus);
            CheckHRESULT(hr, "GetVideoSignalStatus", trFail);

            if (0 == dwStatusOrig)
                dwStatusOrig = dwStatus;

            CheckCondition(DDVPSQ_NOSIGNAL != dwStatus && DDVPSQ_SIGNALOK != dwStatus, "Unrecognzed video status", trFail);
            CheckCondition(dwStatusOrig != dwStatus, "GetVideoSignalStatus returned inconsistent values", tr |= trFail);
            
            dbgout << "Status = ";
            if (DDVPSQ_NOSIGNAL == dwStatus) dbgout << "DDVPSQ_NOSIGNAL";
            else if (DDVPSQ_SIGNALOK == dwStatus) dbgout << "DDVPSQ_SIGNALOK";
            else dbgout(LOG_FAIL) << "Unrecognized signal status : " << HEX(dwStatus);
            dbgout << endl;
        }

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_SetTargetSurface::TestIDirectDrawVideoPort
    //  Tests the SetTargetSurface Method.
    //
    eTestResult CTest_SetTargetSurface::TestIDirectDrawVideoPort()
    {
        eTestResult tr = trPass;
        HRESULT hr = S_OK;
        
#       if CFG_TEST_INVALID_PARAMETERS
            dbgout << "Testing invalid args" << endl;
            hr = m_piDDVP->SetTargetSurface(NULL, DDVPTARGET_VIDEO);
            CheckForHRESULT(hr, DDERR_INVALIDOBJECT, "SetTargetSurface(NULL, DDVPTARGET_VIDEO)", tr |= trFail);
            hr = m_piDDVP->SetTargetSurface(NULL, DDVPTARGET_VBI);
            CheckForHRESULT(hr, DDERR_INVALIDOBJECT, "SetTargetSurface(NULL, DDVPTARGET_VBI)", tr |= trFail);
            hr = m_piDDVP->SetTargetSurface(NULL, DDVPTARGET_VIDEO | DDVPTARGET_VBI);
            CheckForHRESULT(hr, DDERR_INVALIDOBJECT, "SetTargetSurface(NULL, DDVPTARGET_VIDEO | DDVPTARGET_VBI)", tr |= trFail);
            hr = m_piDDVP->SetTargetSurface(m_piDDSOverlay.AsInParam(), 0);
            CheckForHRESULT(hr, ((m_State == vpsNoSurface) ? DDERR_INVALIDOBJECT : DDERR_INVALIDPARAMS), "SetTargetSurface(<valid>, 0)", tr |= trFail);
#       endif

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_StartVideo::TestIDirectDrawVideoPort
    //  Tests the StartVideo Method.
    //
    eTestResult CTest_StartVideo::TestIDirectDrawVideoPort()
    {
        eTestResult tr = trPass;
        CDDVideoPortInfo cddvpi;
        CDDPixelFormat cddpf;
        HRESULT hr = S_OK;

#       if CFG_TEST_INVALID_PARAMETERS
        {
            CDDVideoPortInfo cddvpiInvalid;
            
            dbgout << "Testing invalid args" << endl;
            INVALID_PARAM_TEST(m_piDDVP->StartVideo(NULL));
            cddvpiInvalid.dwSize = 0;
            INVALID_PARAM_TEST(m_piDDVP->StartVideo(&cddvpiInvalid));
            cddvpiInvalid.dwSize = 0xBAADF00D;
            INVALID_PARAM_TEST(m_piDDVP->StartVideo(&cddvpiInvalid));
            cddvpiInvalid.dwSize = sizeof(cddvpiInvalid);
            
            cddvpiInvalid.dwVPFlags = DDVP_CONVERT;
            cddvpiInvalid.lpddpfVBIOutputFormat = NULL;
            INVALID_PARAM_TEST(m_piDDVP->StartVideo(&cddvpiInvalid));

            cddvpiInvalid.dwVPFlags = DDVP_CROP;
            cddvpiInvalid.rCrop.top = cddvpiInvalid.rCrop.bottom = 0;
            cddvpiInvalid.rCrop.left = cddvpiInvalid.rCrop.right = 0;
            INVALID_PARAM_TEST(m_piDDVP->StartVideo(&cddvpiInvalid));
            
            cddvpiInvalid.dwVPFlags = DDVP_PRESCALE;
            cddvpiInvalid.dwPrescaleWidth = 640;
            cddvpiInvalid.dwPrescaleHeight = 0;
            INVALID_PARAM_TEST(m_piDDVP->StartVideo(&cddvpiInvalid));
            cddvpiInvalid.dwPrescaleWidth = 0;
            cddvpiInvalid.dwPrescaleHeight = 120;
            INVALID_PARAM_TEST(m_piDDVP->StartVideo(&cddvpiInvalid));

            cddvpiInvalid.dwVPFlags = DDVP_SKIPEVENFIELDS | DDVP_SKIPODDFIELDS;
            INVALID_PARAM_TEST(m_piDDVP->StartVideo(&cddvpiInvalid));
            
            cddvpiInvalid.dwVPFlags = DDVP_VBICONVERT;
            cddvpiInvalid.lpddpfVBIOutputFormat = NULL;
            INVALID_PARAM_TEST(m_piDDVP->StartVideo(&cddvpiInvalid));
            
            cddvpiInvalid.dwVPFlags = 0;
            cddvpiInvalid.dwReserved1 = 0xBAADF00D;
            INVALID_PARAM_TEST(m_piDDVP->StartVideo(&cddvpiInvalid));
            
            cddvpiInvalid.dwReserved2 = 0xBAADF00D;
            INVALID_PARAM_TEST(m_piDDVP->StartVideo(&cddvpiInvalid));
        }
#       endif

        cddvpi.dwOriginX = 0;
        cddvpi.dwOriginY = 0;
        cddvpi.dwVPFlags = 0;
        cddvpi.lpddpfInputFormat = &cddpf;
        
        cddpf.dwFlags = DDPF_FOURCC;
        cddpf.dwFourCC = MAKEFOURCC('Y','U','Y','V');

        HRESULT hrExpected = (m_State == vpsNoSurface) ? DDERR_INVALIDPARAMS : S_OK;

        dbgout << "Starting video" << endl;
        hr = m_piDDVP->StartVideo(&cddvpi);
        QueryForHRESULT(hr, hrExpected, "StartVideo", tr |= trFail);

        if (m_cddvpcaps.dwFX & DDVPFX_CROPX || m_cddvpcaps.dwFX & DDVPFX_CROPY)
        {
            cddvpi.dwVPFlags    = DDVP_CROP;
            cddvpi.rCrop.top    = 0;
            cddvpi.rCrop.bottom = m_cddsdOverlay.dwHeight;
            cddvpi.rCrop.left   = 0;
            cddvpi.rCrop.right  = m_cddsdOverlay.dwWidth;
            
            if (m_cddvpcaps.dwFX & DDVPFX_CROPX)
            {
                cddvpi.rCrop.top += 10;
                cddvpi.rCrop.bottom -= 10;
            }
            
            if (m_cddvpcaps.dwFX & DDVPFX_CROPY)
            {
                cddvpi.rCrop.left += 10;
                cddvpi.rCrop.right -= 10;
            }
            
            hr = m_piDDVP->StartVideo(&cddvpi);
            QueryForHRESULT(hr, hrExpected, "StartVideo with cropping", tr |= trFail);

            if (m_cddvpcaps.dwFX & DDVPFX_IGNOREVBIXCROP)
            {
                cddvpi.dwVPFlags |= DDVP_IGNOREVBIXCROP;
                
                hr = m_piDDVP->StartVideo(&cddvpi);
                QueryForHRESULT(hr, hrExpected, "StartVideo with no VBI cropping", tr |= trFail);
            }
        }

        if (m_cddvpcaps.dwFX & DDVPFX_MIRRORLEFTRIGHT)
        {
            cddvpi.dwVPFlags = DDVP_MIRRORLEFTRIGHT;
            hr = m_piDDVP->StartVideo(&cddvpi);
            QueryForHRESULT(hr, hrExpected, "StartVideo with left-right mirroring", tr |= trFail);
        }
        
        if (m_cddvpcaps.dwFX & DDVPFX_MIRRORUPDOWN)
        {
            cddvpi.dwVPFlags = DDVP_MIRRORUPDOWN;
            hr = m_piDDVP->StartVideo(&cddvpi);
            QueryForHRESULT(hr, hrExpected, "StartVideo with up-down mirroring", tr |= trFail);
        }
        
        if (m_cddvpcaps.dwCaps & DDVPCAPS_SKIPEVENFIELDS)
        {
            cddvpi.dwVPFlags = DDVP_SKIPEVENFIELDS;
            hr = m_piDDVP->StartVideo(&cddvpi);
            QueryForHRESULT(hr, hrExpected, "StartVideo with skipping even fields", tr |= trFail);
        }
        
        if (m_cddvpcaps.dwCaps & DDVPCAPS_SKIPODDFIELDS)
        {
            cddvpi.dwVPFlags = DDVP_SKIPODDFIELDS;
            hr = m_piDDVP->StartVideo(&cddvpi);
            QueryForHRESULT(hr, hrExpected, "StartVideo with skipping odd fields", tr |= trFail);
        }

        if (m_cddvpcaps.dwCaps & DDVPCAPS_SYNCMASTER)
        {
            cddvpi.dwVPFlags = DDVP_SYNCMASTER;
            hr = m_piDDVP->StartVideo(&cddvpi);
            QueryForHRESULT(hr, hrExpected, "StartVideo with Sync-master", tr |= trFail);
        }

        if (vpsSignalStopped == m_State)
        {
            dbgout << "Video was stopped ... Stopping again" << endl;
            hr = m_piDDVP->StopVideo();
            CheckHRESULT(hr, "StopVideo", trAbort);
        }
        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_StopVideo::TestIDirectDrawVideoPort
    //  Tests the StopVideo Method.
    //
    eTestResult CTest_StopVideo::TestIDirectDrawVideoPort()
    {
        eTestResult tr = trPass;
        HRESULT hr = S_OK;

        hr = m_piDDVP->StopVideo();
        CheckForHRESULT(hr, S_OK, "StopVideo", trFail);

        if (vpsSignalStarted == m_State)
        {
            CDDVideoPortInfo cddvpi;
            CDDPixelFormat cddpf;
            
            cddvpi.dwOriginX = 0;
            cddvpi.dwOriginY = 0;
            cddvpi.dwVPFlags = 0;
            cddvpi.lpddpfInputFormat = &cddpf;
            
            cddpf.dwFlags = DDPF_FOURCC;
            cddpf.dwFourCC = MAKEFOURCC('Y','U','Y','V');
        
            dbgout << "Video was started ... Starting again" << endl;
            hr = m_piDDVP->StartVideo(&cddvpi);
            CheckHRESULT(hr, "StartVideo", trAbort);
        }
        
        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_UpdateVideo::TestIDirectDrawVideoPort
    //  Tests the UpdateVideo Method.
    //
    eTestResult CTest_UpdateVideo::TestIDirectDrawVideoPort()
    {
        eTestResult tr = trPass;
        CDDVideoPortInfo cddvpi;
        CDDPixelFormat cddpf;
        HRESULT hr = S_OK;

#       if CFG_TEST_INVALID_PARAMETERS
        {
            CDDVideoPortInfo cddvpiInvalid;
            
            dbgout << "Testing invalid args" << endl;
            INVALID_PARAM_TEST(m_piDDVP->StartVideo(NULL));
            cddvpiInvalid.dwSize = 0;
            INVALID_PARAM_TEST(m_piDDVP->UpdateVideo(&cddvpiInvalid));
            cddvpiInvalid.dwSize = 0xBAADF00D;
            INVALID_PARAM_TEST(m_piDDVP->UpdateVideo(&cddvpiInvalid));
            cddvpiInvalid.dwSize = sizeof(cddvpiInvalid);
            
            cddvpiInvalid.dwVPFlags = DDVP_CONVERT;
            cddvpiInvalid.lpddpfVBIOutputFormat = NULL;
            INVALID_PARAM_TEST(m_piDDVP->UpdateVideo(&cddvpiInvalid));

            cddvpiInvalid.dwVPFlags = DDVP_CROP;
            cddvpiInvalid.rCrop.top = cddvpiInvalid.rCrop.bottom = 0;
            cddvpiInvalid.rCrop.left = cddvpiInvalid.rCrop.right = 0;
            INVALID_PARAM_TEST(m_piDDVP->UpdateVideo(&cddvpiInvalid));
            
            cddvpiInvalid.dwVPFlags = DDVP_PRESCALE;
            cddvpiInvalid.dwPrescaleWidth = 640;
            cddvpiInvalid.dwPrescaleHeight = 0;
            INVALID_PARAM_TEST(m_piDDVP->UpdateVideo(&cddvpiInvalid));
            cddvpiInvalid.dwPrescaleWidth = 0;
            cddvpiInvalid.dwPrescaleHeight = 120;
            INVALID_PARAM_TEST(m_piDDVP->UpdateVideo(&cddvpiInvalid));

            cddvpiInvalid.dwVPFlags = DDVP_SKIPEVENFIELDS | DDVP_SKIPODDFIELDS;
            INVALID_PARAM_TEST(m_piDDVP->UpdateVideo(&cddvpiInvalid));
            
            cddvpiInvalid.dwVPFlags = DDVP_VBICONVERT;
            cddvpiInvalid.lpddpfVBIOutputFormat = NULL;
            INVALID_PARAM_TEST(m_piDDVP->UpdateVideo(&cddvpiInvalid));
            
            cddvpiInvalid.dwVPFlags = 0;
            cddvpiInvalid.dwReserved1 = 0xBAADF00D;
            INVALID_PARAM_TEST(m_piDDVP->UpdateVideo(&cddvpiInvalid));
            
            cddvpiInvalid.dwReserved2 = 0xBAADF00D;
            INVALID_PARAM_TEST(m_piDDVP->UpdateVideo(&cddvpiInvalid));
        }
#       endif

        cddvpi.dwOriginX = 0;
        cddvpi.dwOriginY = 0;
        cddvpi.dwVPFlags = 0;
        cddvpi.lpddpfInputFormat = &cddpf;
        
        cddpf.dwFlags = DDPF_FOURCC;
        cddpf.dwFourCC = MAKEFOURCC('Y','U','Y','V');

        HRESULT hrExpected = (m_State == vpsNoSurface) ? DDERR_INVALIDPARAMS : S_OK;

        dbgout << "Updating video" << endl;
        hr = m_piDDVP->UpdateVideo(&cddvpi);
        QueryForHRESULT(hr, hrExpected, "UpdateVideo", tr |= trFail);

        if (m_cddvpcaps.dwFX & DDVPFX_CROPX || m_cddvpcaps.dwFX & DDVPFX_CROPY)
        {
            cddvpi.dwVPFlags    = DDVP_CROP;
            cddvpi.rCrop.top    = 0;
            cddvpi.rCrop.bottom = m_cddsdOverlay.dwHeight;
            cddvpi.rCrop.left   = 0;
            cddvpi.rCrop.right  = m_cddsdOverlay.dwWidth;
            
            if (m_cddvpcaps.dwFX & DDVPFX_CROPX)
            {
                cddvpi.rCrop.top += 10;
                cddvpi.rCrop.bottom -= 10;
            }
            
            if (m_cddvpcaps.dwFX & DDVPFX_CROPY)
            {
                cddvpi.rCrop.left += 10;
                cddvpi.rCrop.right -= 10;
            }
            
            hr = m_piDDVP->UpdateVideo(&cddvpi);
            QueryForHRESULT(hr, hrExpected, "UpdateVideo with cropping", tr |= trFail);

            if (m_cddvpcaps.dwFX & DDVPFX_IGNOREVBIXCROP)
            {
                cddvpi.dwVPFlags |= DDVP_IGNOREVBIXCROP;
                
                hr = m_piDDVP->UpdateVideo(&cddvpi);
                QueryForHRESULT(hr, hrExpected, "UpdateVideo with no VBI cropping", tr |= trFail);
            }
        }

        if (m_cddvpcaps.dwFX & DDVPFX_MIRRORLEFTRIGHT)
        {
            cddvpi.dwVPFlags = DDVP_MIRRORLEFTRIGHT;
            hr = m_piDDVP->UpdateVideo(&cddvpi);
            QueryForHRESULT(hr, hrExpected, "UpdateVideo with left-right mirroring", tr |= trFail);
        }
        
        if (m_cddvpcaps.dwFX & DDVPFX_MIRRORUPDOWN)
        {
            cddvpi.dwVPFlags = DDVP_MIRRORUPDOWN;
            hr = m_piDDVP->UpdateVideo(&cddvpi);
            QueryForHRESULT(hr, hrExpected, "UpdateVideo with up-down mirroring", tr |= trFail);
        }
        
        if (m_cddvpcaps.dwCaps & DDVPCAPS_SKIPEVENFIELDS)
        {
            cddvpi.dwVPFlags = DDVP_SKIPEVENFIELDS;
            hr = m_piDDVP->UpdateVideo(&cddvpi);
            QueryForHRESULT(hr, hrExpected, "UpdateVideo with skipping even fields", tr |= trFail);
        }
        
        if (m_cddvpcaps.dwCaps & DDVPCAPS_SKIPODDFIELDS)
        {
            cddvpi.dwVPFlags = DDVP_SKIPODDFIELDS;
            hr = m_piDDVP->UpdateVideo(&cddvpi);
            QueryForHRESULT(hr, hrExpected, "UpdateVideo with skipping odd fields", tr |= trFail);
        }

        if (m_cddvpcaps.dwCaps & DDVPCAPS_SYNCMASTER)
        {
            cddvpi.dwVPFlags = DDVP_SYNCMASTER;
            hr = m_piDDVP->UpdateVideo(&cddvpi);
            QueryForHRESULT(hr, hrExpected, "UpdateVideo with Sync-master", tr |= trFail);
        }

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // namespace priv_CTest_WaitForSync
    //  
    //
    namespace priv_CTest_WaitForSync
    {
        eTestResult VerifyMultipleSyncs(CComPointer<IDirectDrawVideoPort> &piDDVP,
                                        DWORD dwWaitFlags, DWORD dwLine, TCHAR *szDesc)
        {
            eTestResult tr = trPass;
            const DWORD dwTimeOut = 5 * 1000;
            const int nPasses = 10;
            DWORD dwStartTime,
                  dwStopTime;
            HRESULT hr;

            dbgout << "Testing " << szDesc << endl;
            // There are no caps bits to indicate if this should
            // work on a particular card or not, so if it returns
            // DDERR_UNSUPPORTED, we skip it -- not loging failure.
            hr = piDDVP->WaitForSync(dwWaitFlags, dwLine, dwTimeOut);
            if (DDERR_UNSUPPORTED == hr)
            {
                dbgout << "WaitForSync call not supported... Skipping" << endl;
                return trSkip;
            }
            else if (DDERR_VIDEONOTACTIVE == hr)
            {
                dbgout << "No Active Signal -- Skipping" << endl;
                return trSkip;
            }

            // The WaitForSync call at least succeeds, so make sure
            // that it's actually waiting
            dwStartTime = GetTickCount();
            for (int i = 0 ; i < nPasses; i++)
            {
                hr = piDDVP->WaitForSync(dwWaitFlags, dwLine, dwTimeOut);
                QueryHRESULT(hr, szDesc, tr |= trFail);
            }
            dwStopTime = GetTickCount();

            // We check to make sure that the average wait in the
            // above loop is at least dwTimeOut ms.
            if (dwStopTime >= dwStartTime + nPasses * dwTimeOut)
            {
                dbgout << szDesc << " Timed Out" << FILELOC << endl;
                tr |= trFail;
            }
            dbgout  << "Start = " << dwStartTime << endl
                    << "Stop  = " << dwStopTime << endl;

            // Check to make sure that we spent at least 1ms per Wait
            CheckCondition(dwStopTime - dwStartTime < nPasses, "WaitForSync didn't wait long enough", trFail);

            return tr;
        }
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_WaitForSync::TestIDirectDrawVideoPort
    //  Tests the WaitForSync Method.
    //
    eTestResult CTest_WaitForSync::TestIDirectDrawVideoPort()
    {
        using namespace priv_CTest_WaitForSync;
        eTestResult tr = trPass;

#       if CFG_TEST_INVALID_PARAMETERS
            HRESULT hr;
            
            dbgout << "Testing invalid arg : GetVideoSignalStatus(NULL)" << endl;
            INVALID_PARAM_TEST(m_piDDVP->WaitForSync(0, 0, 0));
            //INVALID_PARAM_TEST(m_piDDVP->WaitForSync(DDVPWAIT_LINE, 1000, 0));
            //INVALID_PARAM_TEST(m_piDDVP->WaitForSync(DDVPWAIT_LINE, 1000, 1000));
#       endif

        tr |= VerifyMultipleSyncs(m_piDDVP, DDVPWAIT_BEGIN, PRESET, TEXT("WaitForSync(DDVPWAIT_BEGIN)"));
        tr |= VerifyMultipleSyncs(m_piDDVP, DDVPWAIT_END, PRESET, TEXT("WaitForSync(DDVPWAIT_END)"));
        tr |= VerifyMultipleSyncs(m_piDDVP, DDVPWAIT_LINE, 60, TEXT("WaitForSync(DDVPWAIT_LINE)"));

        return tr;
    }
}
