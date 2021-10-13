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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include <windows.h>
#include <imaging.h>
#include "logging.h"
#include "CaptureFramework.h"
#include "bufferfill.h"
#include <pnp.h>
#include <winbase.h>
#include <tchar.h>
#include "clparse.h"
#include <GdiplusEnums.h>
#include <bldver.h>
#include <mmreg.h>
#include "eventsink.h"

//#define SAVE_BITMAPS

// global media types to use as the defaults.
AM_MEDIA_TYPE g_mt;
GSM610WAVEFORMAT g_pwfx;
#ifndef WAVE_FORMAT_GSM610
#define WAVE_FORMAT_GSM610 0x31
#endif

CCaptureFramework::CCaptureFramework( )
{
    _tcsncpy(m_tszStillCaptureName, STILL_IMAGE_FILE_NAME, MAX_PATH);
    _tcsncpy(m_tszVideoCaptureName, VIDEO_CAPTURE_FILE_NAME, MAX_PATH);

    m_hwndOwner = NULL;
    memset(&m_rcLocation, 0, sizeof(m_rcLocation));
    m_lDefaultWidth = 0;
    m_lDefaultHeight = 0;
    m_dwComponentsInUse = 0;
    m_lPictureNumber = -1;
    m_lCaptureNumber = -1;

    m_bCoInitialized = FALSE;
    m_bInitialized = FALSE;
    m_pCaptureGraphBuilder = NULL;
    m_pGraph = NULL;
    m_pColorConverter = NULL;
    m_pVideoRenderer = NULL;
    m_pSmartTeeFilter = NULL;
    m_pVideoRendererMode = NULL;
    m_pVideoRendererBasicVideo = NULL;
    m_pVideoWindow = NULL;
    m_pVideoCapture = NULL;
    m_pAudioCapture = NULL;
    m_pVideoControl = NULL;
    m_pCameraControl = NULL;
    m_pDroppedFrames = NULL;
    m_pVideoProcAmp = NULL;
    m_pKsPropertySet = NULL;
    m_pStillStreamConfig = NULL;
    m_pCaptureStreamConfig = NULL;
    m_pPreviewStreamConfig = NULL;
    m_pAudioStreamConfig = NULL;
    m_pStillFileSink = NULL;
    m_pMuxFilter = NULL;
    m_pVideoFileSink = NULL;
    m_pVideoPropertyBag = NULL;
    m_pAudioPropertyBag = NULL;
    m_pMediaControl = NULL;
    m_pMediaEvent = NULL;
    m_pStillSink = NULL;
    m_pStillPin = NULL;
    m_pHeaderInfo = NULL;
    m_pVideoDMOWrapperFilter = NULL;
    m_pVideoEncoderDMO = NULL;
    m_pAudioDMOWrapperFilter = NULL;
    m_pAudioEncoderDMO = NULL;

    m_pAudioBuffering = NULL;
    m_pVideoBuffering = NULL;

    m_bForceColorConversion = FALSE;

    m_pImageSinkFilter  = NULL;

    m_nSelectedCameraDevice = -1;
    m_bUsingNULLDriver = FALSE;

    m_dwOptionsInUse = 0;

    m_nPreviewVerificationMode = VERIFICATION_OFF;
    m_nStillVerificationMode = VERIFICATION_OFF;

    memset(m_tszVideoFileLibraryPath, 0, sizeof(m_tszVideoFileLibraryPath));
    m_hVerifyVideoFile = NULL;
    m_pfnVerifyVideoFile = NULL;

    m_Lock.Lock();
    m_bOutputKeyEvents = FALSE;
    m_bOutputAllEvents = FALSE;
    m_lVideoFramesProcessed = 0;
    m_lAudioFramesProcessed = 0;
    m_bDiskWriteErrorOccured = FALSE;
    m_lVideoFramesDropped = 0;
    m_lAudioFramesDropped = 0;
    m_bBufferDeliveryFailed = FALSE;
    m_Lock.Unlock();

    m_uiPinCount = 0;

    m_nQuality = -1;
    m_nKeyFrameDistance = -1;
    m_nComplexity = -1;

    m_pamtPreviewStream = NULL;
    m_pamtStillStream = NULL;
    m_pamtCaptureStream = NULL;
    m_pamtAudioStream = NULL;

    m_bUseGSM = FALSE;

    m_bOutputFilterInformation = FALSE;
    m_bInsertPreviewDiagnostics = FALSE;
    m_bInsertStillDiagnostics = FALSE;
    m_bInsertCaptureDiagnostics = FALSE;

    m_clsidVideoEncoder = CLSID_CWMV9EncMediaObject;
    m_clsidAudioEncoder = GUID_NULL;
    m_clsidMux = CLSID_ASFWriter;


    // intializtion of global media types.
    // this is necessary for the GSM encoder information.
    g_mt.majortype = MEDIATYPE_Audio;
    g_mt.bFixedSizeSamples = true;
    g_mt.bTemporalCompression=false;
    g_mt.lSampleSize=4096;
    g_mt.formattype=FORMAT_WaveFormatEx;
    g_mt.pUnk=NULL;

    g_pwfx.wfx.wFormatTag = WAVE_FORMAT_GSM610;
    g_pwfx.wfx.nChannels = 0x0001;
    g_pwfx.wfx.nSamplesPerSec = 0x1f40;
    g_pwfx.wfx.nAvgBytesPerSec = 0x00000659;
    g_pwfx.wfx.nBlockAlign = 0x0041;
    g_pwfx.wfx.wBitsPerSample = 0x0000;
    g_pwfx.wfx.cbSize = 0x0002;
    g_pwfx.wSamplesPerBlock = 0x140;

    g_mt.cbFormat = sizeof( GSM610WAVEFORMAT );
    g_mt.pbFormat = (BYTE *) &g_pwfx;

    // to speed up the loading and unloading of the tests, we'll pre load all of the
    // libraries the test suite depends on, so we don't have to wait for them to load and
    // unload over and over while the test is running. Once they're loaded they stay loaded
    // until we exit, then we'll free them in the destructor.
    for(int i = 0; i < countof(g_LibrariesToAccelerateLoad); i++)
    {
        if(!g_LibrariesToAccelerateLoad[i].hLibrary)
            g_LibrariesToAccelerateLoad[i].hLibrary = LoadLibrary(g_LibrariesToAccelerateLoad[i].tszLibraryName);
    }
}

CCaptureFramework::~CCaptureFramework( )
{
    Cleanup();

    for(int i = 0; i < countof(g_LibrariesToAccelerateLoad); i++)
    {
        if(g_LibrariesToAccelerateLoad[i].hLibrary)
            FreeLibrary(g_LibrariesToAccelerateLoad[i].hLibrary);
        g_LibrariesToAccelerateLoad[i].hLibrary = NULL;
    }
}

HRESULT
CCaptureFramework::Cleanup()
{
    HRESULT hr = S_OK;

    if(m_bCoInitialized)
    {
        // if the video window is available, clear out it's window handle before cleanup
        // to prevent invalid use of the window after cleanup.
        if(m_pVideoWindow)
        {
            // Clear the owner window for the video window
            // this may fail depending on the owners and such, so ignore any failures.
            m_pVideoWindow->put_Owner((OAHWND) NULL);
        }

        PauseGraph();
        StopGraph();

        // we must be initialized to clear the media events.
        m_EventSink.Cleanup();

        // release all of the components
        m_pCaptureGraphBuilder = NULL;
        m_pGraph = NULL;
        m_pColorConverter = NULL;
        m_pVideoRenderer = NULL;
        m_pSmartTeeFilter = NULL;
        m_pVideoRendererMode = NULL;
        m_pVideoRendererBasicVideo = NULL;
        m_pVideoWindow = NULL;
        m_pVideoCapture = NULL;
        m_pAudioCapture = NULL;
        m_pVideoControl = NULL;
        m_pCameraControl = NULL;
        m_pDroppedFrames = NULL;
        m_pVideoProcAmp = NULL;
        m_pKsPropertySet = NULL;
        m_pStillStreamConfig = NULL;
        m_pCaptureStreamConfig = NULL;
        m_pPreviewStreamConfig = NULL;
        m_pAudioStreamConfig = NULL;
        m_pStillFileSink = NULL;
        m_pVideoFileSink = NULL;
        m_pMuxFilter = NULL;
        m_pVideoPropertyBag = NULL;
        m_pAudioPropertyBag = NULL;
        m_pMediaControl = NULL;
        m_pMediaEvent = NULL;
        m_pStillSink = NULL;
        m_pStillPin = NULL;
        m_pHeaderInfo = NULL;
        m_pVideoDMOWrapperFilter = NULL;
        m_pVideoEncoderDMO = NULL;
        m_pImageSinkFilter = NULL;
        m_pAudioDMOWrapperFilter = NULL;
        m_pAudioEncoderDMO = NULL;
        m_pAudioBuffering = NULL;
        m_pVideoBuffering = NULL;

        // uninitialize
        CoUninitialize();

        m_bCoInitialized = FALSE;
    }

    // clear out our stored stream configurations
    if(m_pamtPreviewStream)
    {
        DeleteMediaType(m_pamtPreviewStream);
        m_pamtPreviewStream = NULL;
    }

    if(m_pamtStillStream)
    {
        DeleteMediaType(m_pamtStillStream);
        m_pamtStillStream = NULL;
    }

    if(m_pamtCaptureStream)
    {
        DeleteMediaType(m_pamtCaptureStream);
        m_pamtCaptureStream = NULL;
    }

    if(m_pamtAudioStream)
    {
        DeleteMediaType(m_pamtAudioStream);
        m_pamtAudioStream = NULL;
    }

    // clean up some of our local variables related to graph state.
    // NOTE: variables like parameters from parsing the command line,
    // current picture number, current capture number, and camera
    // driver selected do not need to be reset as they're system state and not graph state.
    m_hwndOwner = NULL;
    memset(&m_rcLocation, 0, sizeof(m_rcLocation));
    m_lDefaultWidth = 0;
    m_lDefaultHeight = 0;
    m_dwComponentsInUse = 0;
    m_dwOptionsInUse = 0;

    m_Lock.Lock();
    m_lVideoFramesProcessed = 0;
    m_lAudioFramesProcessed = 0;
    m_bDiskWriteErrorOccured = FALSE;
    m_lVideoFramesDropped = 0;
    m_lAudioFramesDropped = 0;
    m_bBufferDeliveryFailed = FALSE;
    m_Lock.Unlock();

    m_nQuality = -1;
    m_nKeyFrameDistance = -1;
    m_nComplexity = -1;

    m_bInitialized = FALSE;

    // if a library has been loaded, free it
    if(m_hVerifyVideoFile)
    {
        FreeLibrary(m_hVerifyVideoFile);
        m_hVerifyVideoFile = NULL;
        m_pfnVerifyVideoFile = NULL;
    }

    return hr;
}

void
CCaptureFramework::OutputRuntimeParameters()
{
    Log(TEXT("*** Test runtime parameteters *** "));

    Log(TEXT("    Capture framework version %d.%d.%d.%d"), CE_MAJOR_VER, CE_MINOR_VER, CE_BUILD_VER, TEST_FRAMEWORK_VERSION);

    if(m_nPreviewVerificationMode == VERIFICATION_OFF)
    {
        Log(TEXT("    Preview verification turned off."));
    }
    else if(m_nPreviewVerificationMode == VERIFICATION_FORCEAUTO)
    {
        Log(TEXT("    Preview auto verification turned on for the sample driver."));
    }
    else if(m_nPreviewVerificationMode == VERIFICATION_FORCEMANUAL)
    {
        Log(TEXT("    Preview manual verification turned on."));
    }

    if(m_nStillVerificationMode == VERIFICATION_OFF)
    {
        Log(TEXT("    Still image verification turned off."));
    }
    else if(m_nStillVerificationMode == VERIFICATION_FORCEAUTO)
    {
        Log(TEXT("    Still image auto verfication turned on for the sample driver."));
    }
    else if(m_nStillVerificationMode == VERIFICATION_FORCEMANUAL)
    {
        Log(TEXT("    Still image manual verification turned on."));
    }

    if(m_bForceColorConversion)
    {
        Log(TEXT("    Forcing the color converter into the preview pipeline."));
    }

    LPOLESTR CLSID;

    if(SUCCEEDED(StringFromCLSID(m_clsidVideoEncoder, &CLSID)))
    {
        Log(TEXT("    Using video encoder CLSID %s"), CLSID);
        CoTaskMemFree(CLSID);
    }

    if(SUCCEEDED(StringFromCLSID(m_clsidAudioEncoder, &CLSID)))
    {
        Log(TEXT("    Using audio encoder CLSID %s"), CLSID);
        CoTaskMemFree(CLSID);
    }

    if(SUCCEEDED(StringFromCLSID(m_clsidMux, &CLSID)))
    {
        Log(TEXT("    Using mux CLSID %s"), CLSID);
        CoTaskMemFree(CLSID);
    }

    if(m_pfnVerifyVideoFile)
    {
        Log(TEXT("    Using verfication library %s."), m_tszVideoFileLibraryPath);
    }

    if(m_bUseGSM)
    {
        Log(TEXT("    Using the GSM encoder through the audio capture filter"));
    }

    m_Lock.Lock();
    if(m_bOutputAllEvents)
    {
        Log(TEXT("    Outputting event data for all events received."));
    }
    else if(m_bOutputKeyEvents)
    {
        Log(TEXT("    Outputting event data for key events."));
    }
    m_Lock.Unlock();

    if(m_bOutputFilterInformation)
    {
        Log(TEXT("    Outputting filter information."));
    }

    if(m_bInsertPreviewDiagnostics)
    {
        Log(TEXT("    Outputting preview diagnostics information."));
    }

    if(m_bInsertStillDiagnostics)
    {
        Log(TEXT("    Outputting still capture diagnostics information."));
    }

    if(m_bInsertCaptureDiagnostics)
    {
        Log(TEXT("    Outputting video capture diagnostics information."));
    }
}

BOOL
CCaptureFramework::ParseCommandLine(LPCTSTR tszCommandLine)
{
    class CClParse cCLPparser(tszCommandLine);
    BOOL bReturnValue = TRUE;
    BOOL bAutoVerify = FALSE, bManualVerify = FALSE;

    // if no command line was given, then there's nothing for us to do.
    if(!tszCommandLine)
    {
        return FALSE;
    }
    else if(cCLPparser.GetOpt(TEXT("?")))
    {
        Log(TEXT(""));
        Log(TEXT("/? outputs this message"));
        Log(TEXT(""));
        Log(TEXT("Pluggable component modification"));
        Log(TEXT("    /VideoEncoderCLSID {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxx} - sets the framework to use the encoder dmo with the CLSID specified."));
        Log(TEXT("    /AudioEncoderCLSID {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxx} - sets the framework to use the encoder dmo with the CLSID specified."));
        Log(TEXT("    /MuxCLSID {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxx} - sets the framework to use the MUX with the CLSID specified."));
        Log(TEXT("    /UseGSM - sets the framework to use the GSM encoder through the audio capture filter."));
        Log(TEXT("    /ForceColorConversion - Forces the color conversion filter into the graph in front of the renderer. May be necessary depending on encoder and camera driver supported formats."));

        Log(TEXT(""));
        Log(TEXT("Captured video validation"));
        Log(TEXT("    /VideoFileConformanceLibrary <path> - this takes a path to a library (DLL) which exports a function called VerifyVideoFile which takes a single"));
        Log(TEXT("    WCHAR * parameter which indicates a file name to verify.  This function should return an HRESULT indicating whether or not the video file is conformant and playable."));
        Log(TEXT("    if no path is given and the file sink used is the default ASF file sink, asfverify.dll is used."));
        Log(TEXT(""));
        Log(TEXT("Diagnostics and verification options"));
        Log(TEXT("    Preview verification methods"));
        Log(TEXT("        /AutoVerifyPreview forces automatic verification on, regardless of renderer in use (may cause invalid failures)."));
        Log(TEXT("        /ManualVerifyPreview forces manual verification on (user will be prompted for input)."));

        Log(TEXT("    Still verification methods"));
        Log(TEXT("        /AutoVerifyStill turns off automatic verification of the still frame captured if using the sample camera driver."));
        Log(TEXT("        /ManualVerifyStill turns off automatic verification of the still frame captured."));

        Log(TEXT("    Failure diagnostics options"));
        Log(TEXT("        /OutputKeyEvents - enables the test to output debugging information for missing events, skip outputting high volume events."));
        Log(TEXT("        /OutputAllEvents - enables the test to output debugging information for missing events, outputs every event."));
        Log(TEXT("        /OutputFilterInformation - enables the test to output information about the graph topology."));
        Log(TEXT("        /OutputPreviewDiagnostics - output preview diagnostics information."));
        Log(TEXT("        /OutputStillDiagnostics - output still capture diagnostics information."));
        Log(TEXT("        /OutputCaptureDiagnostics - output video capture diagnostics information."));
        Log(TEXT("        /OutputAllDiagnostics - output video capture diagnostics information."));

        Log(TEXT(""));

        return FALSE;
    }
    else
    {
        // get the verification methods
        bAutoVerify = cCLPparser.GetOpt(TEXT("AutoVerifyPreview"));
        bManualVerify = cCLPparser.GetOpt(TEXT("ManualVerifyPreview"));

        // if none, or more than one option of the 3 is given, then we might have a problem.
        if(!(bAutoVerify ^ bManualVerify))
        {
            // if something was given, then we have multiple verification methods given, so fail.
            if(bAutoVerify || bManualVerify)
            {
                FAIL(TEXT("Invalid command line combination given for preview verification."));
                bReturnValue = FALSE;
            }
            // if nothing was given, then we revert to the default behavior.
            else
            {
                m_nPreviewVerificationMode = VERIFICATION_OFF;
            }
        }
        else if(bAutoVerify)
        {
            m_nPreviewVerificationMode = VERIFICATION_FORCEAUTO;
        }
        else if(bManualVerify)
        {
            m_nPreviewVerificationMode = VERIFICATION_FORCEMANUAL;
        }

        // get the verification methods
        bManualVerify = cCLPparser.GetOpt(TEXT("ManualVerifyStill"));
        bAutoVerify = cCLPparser.GetOpt(TEXT("AutoVerifyStill"));

        // if none, or more than one option is given, then we have an invalid combination.
        if(!(bManualVerify ^ bAutoVerify))
        {
            // if something was given, then we have multiple verification methods given, so fail.
            if(bManualVerify || bAutoVerify)
            {
                FAIL(TEXT("Invalid command line combination given for still verification."));
                bReturnValue = FALSE;
            }
            // if nothing was given, then we revert to the default behavior.
            else
            {
                m_nStillVerificationMode = VERIFICATION_OFF;
            }
        }
        else if(bAutoVerify)
        {
            m_nStillVerificationMode = VERIFICATION_FORCEAUTO;
        }
        else if(bManualVerify)
        {
            m_nStillVerificationMode = VERIFICATION_FORCEMANUAL;
        }

        // if the user requests to use GSM through the audio capture filter,
        // set the media type.
        if(cCLPparser.GetOpt(TEXT("UseGSM")))
        {
            m_bUseGSM = TRUE;
        }

        // in some scenarios, depending on what the camera driver supports,
        // it may be necessary to manually force the color conversion filter into the 
        // graph.
        if(cCLPparser.GetOpt(TEXT("ForceColorConversion")))
        {
            m_bForceColorConversion = TRUE;
        }

        m_Lock.Lock();
        if(cCLPparser.GetOpt(TEXT("OutputKeyEvents")))
        {
            m_bOutputKeyEvents = TRUE;
            m_bOutputAllEvents = FALSE;
        }

        if(cCLPparser.GetOpt(TEXT("OutputAllEvents")))
        {
            m_bOutputKeyEvents = TRUE;
            m_bOutputAllEvents = TRUE;
        }
        m_Lock.Unlock();

        if(cCLPparser.GetOpt(TEXT("OutputFilterInformation")))
        {
            m_bOutputFilterInformation = TRUE;
        }

        if(cCLPparser.GetOpt(TEXT("OutputPreviewDiagnostics")))
        {
            m_bInsertPreviewDiagnostics = TRUE;
        }

        if(cCLPparser.GetOpt(TEXT("OutputStillDiagnostics")))
        {
            m_bInsertStillDiagnostics = TRUE;
        }

        if(cCLPparser.GetOpt(TEXT("OutputCaptureDiagnostics")))
        {
            m_bInsertCaptureDiagnostics = TRUE;
        }

        if(cCLPparser.GetOpt(TEXT("OutputAllDiagnostics")))
        {
            m_bInsertCaptureDiagnostics = TRUE;
            m_bInsertStillDiagnostics = TRUE;
            m_bInsertPreviewDiagnostics = TRUE;
            m_bOutputFilterInformation = TRUE;

            m_Lock.Lock();
            m_bOutputKeyEvents = TRUE;
            m_bOutputAllEvents = TRUE;
            m_Lock.Unlock();
        }

        TCHAR *ptszCmdLineBackup;
        DWORD dwCmdLineLength = _tcslen(tszCommandLine) + 1;
        TCHAR *ptszToken;
        CLSID clsid;

        if(dwCmdLineLength > 1)
        {
            // first, backup the command line
            ptszCmdLineBackup = new TCHAR[dwCmdLineLength];
            _tcsncpy(ptszCmdLineBackup, tszCommandLine, dwCmdLineLength);

            // now tokenize it for spaces and forward slashes, if we tokenize for dashes
            // then the dashes in the clsid mess everything up.
            ptszToken = ::_tcstok(ptszCmdLineBackup, _T(" /"));
            while(NULL != ptszToken)
            {
                // if we found the option we want, then grab the value following the token
                if(0 == ::_tcsicmp(ptszToken, TEXT("VideoEncoderCLSID")))
                {
                    ptszToken = ::_tcstok(NULL, _T(" "));
                    if(NULL != ptszToken)
                    {
                        // try to convert the value to a clsid, if that fails, then we have a problem.
                        if(FAILED(CLSIDFromString(ptszToken, &clsid)))
                        {
                            FAIL(TEXT("Invalid string given for encoder CLSID."));
                            Log(TEXT("Invalid string was %s"), ptszToken);
                            bReturnValue = FALSE;
                        }
                        else
                        {
                            m_clsidVideoEncoder = clsid;
                        }

                        // we found what we were looking for, so we're done.
                        break;
                    }
                }
                ptszToken = ::_tcstok(NULL, _T(" /"));
            }

            _tcsncpy(ptszCmdLineBackup, tszCommandLine, dwCmdLineLength);

            ptszToken = ::_tcstok(ptszCmdLineBackup, _T(" /"));
            while(NULL != ptszToken)
            {
                if(0 == ::_tcsicmp(ptszToken, TEXT("MuxCLSID")))
                {
                    ptszToken = ::_tcstok(NULL, _T(" "));
                    if(NULL != ptszToken)
                    {

                        if(FAILED(CLSIDFromString(ptszToken, &clsid)))
                        {
                            FAIL(TEXT("Invalid string given for mux CLSID."));
                            Log(TEXT("Invalid string was %s"), ptszToken);
                            bReturnValue = FALSE;
                        }
                        else
                        {
                            m_clsidMux = clsid;
                        }

                        break;
                    }
                }
                ptszToken = ::_tcstok(NULL, _T(" /"));
            }

            _tcsncpy(ptszCmdLineBackup, tszCommandLine, dwCmdLineLength);

            ptszToken = ::_tcstok(ptszCmdLineBackup, _T(" /"));
            while(NULL != ptszToken)
            {
                if(0 == ::_tcsicmp(ptszToken, TEXT("AudioEncoderCLSID")))
                {
                    ptszToken = ::_tcstok(NULL, _T(" "));
                    if(NULL != ptszToken)
                    {

                        if(FAILED(CLSIDFromString(ptszToken, &clsid)))
                        {
                            FAIL(TEXT("Invalid string given for encoder CLSID."));
                            Log(TEXT("Invalid string was %s"), ptszToken);
                            bReturnValue = FALSE;
                        }
                        else
                        {
                            m_clsidAudioEncoder = clsid;
                        }

                        break;
                    }
                }
                ptszToken = ::_tcstok(NULL, _T(" /"));
            }

            if(ptszCmdLineBackup)
                delete[] ptszCmdLineBackup;
        }
    }

    // make sure our library path is empty
    memset(m_tszVideoFileLibraryPath, 0, sizeof(m_tszVideoFileLibraryPath));
    // if a library has been loaded, free it
    if(m_hVerifyVideoFile)
    {
        FreeLibrary(m_hVerifyVideoFile);
        m_hVerifyVideoFile = NULL;
        m_pfnVerifyVideoFile = NULL;
    }

    // if there's no library given on the command line
    if(!cCLPparser.GetOptString(TEXT("VideoFileConformanceLibrary"), m_tszVideoFileLibraryPath, countof(m_tszVideoFileLibraryPath)))
    {
        // and we're still using the default ASF file creator, then use our default video file verifier.
        if(m_clsidMux == CLSID_ASFWriter)
            _tcsncpy(m_tszVideoFileLibraryPath, VIDEOFILEPARSEDLL, countof(m_tszVideoFileLibraryPath));
    }

    // if there's a library file name set
    if(*m_tszVideoFileLibraryPath != '\0')
    {
        m_hVerifyVideoFile = LoadLibrary(m_tszVideoFileLibraryPath);

        if(m_hVerifyVideoFile)
        {
            m_pfnVerifyVideoFile = (PFNVERIFYVIDEOFILE) GetProcAddress(m_hVerifyVideoFile, VIDEOFILEPARSEFUNCTIONNAME);
            if(!m_pfnVerifyVideoFile)
            {
                FAIL(TEXT("Unable to retrieve the proc address for the video verfication function."));
                FreeLibrary(m_hVerifyVideoFile);
                m_hVerifyVideoFile = NULL;
            }
        }
        else
        {
            *m_tszVideoFileLibraryPath = NULL;
        }
    }

    return bReturnValue;
}

HRESULT
CCaptureFramework::SetStillCaptureFileName(TCHAR *tszFileName)
{
    HRESULT hr = S_OK;

    if(tszFileName)
    {
        _tcsncpy(m_tszStillCaptureName, tszFileName, MAX_PATH);
        // default to -2, indicating a static file name.
        // if we find a % sign in it, then we switch to a dynamic file name.
        m_lPictureNumber = -2;
    }
    else
    {
        hr = E_FAIL;
        goto cleanup;
    }

    DWORD dwStillFlags=AM_FILE_OVERWRITE;
    for(uint i = 0; i < _tcslen(tszFileName); i++)
        if(tszFileName[i] == '%')
        {
            dwStillFlags |= AM_FILE_TEMPLATE;
            m_lPictureNumber = -1;
            break;
        }

    if(m_pStillFileSink)
    {
        hr = m_pStillFileSink->SetMode( dwStillFlags );
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Setting the file sink flags failed."));
            goto cleanup;
        }

        // set the file name
        hr = m_pStillFileSink->SetFileName( tszFileName, NULL );
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Setting the file sink file name failed."));
            goto cleanup;
        }
    }
    else
    {
        hr = E_FAIL;
        FAIL(TEXT("CCaptureFramework: No file sink available to set the file name"));
        goto cleanup;
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::SetVideoCaptureFileName(TCHAR *tszFileName)
{
    HRESULT hr = S_OK;

    if(tszFileName)
    {
        _tcsncpy(m_tszVideoCaptureName, tszFileName, MAX_PATH);
        m_lCaptureNumber = -1;
    }
    else hr = E_FAIL;

    return hr;
}

HRESULT
CCaptureFramework::PropogateFilenameToCaptureFilter()
{
    HRESULT hr = S_OK;
    TCHAR tszPictureName[MAX_PATH] = {NULL};
    int nStringLength = MAX_PATH;

    hr = GetVideoCaptureFileName(tszPictureName, &nStringLength);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Failed to retrieve the capture file name."));
        hr = E_FAIL;
        goto cleanup;
    }

    tszPictureName[MAX_PATH - 1] = NULL;

    if(m_pVideoFileSink)
    {
        hr = m_pVideoFileSink->SetFileName( tszPictureName, NULL );
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to set the capture file name."));
            goto cleanup;
        }
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::InitCameraDriver()
{
    HRESULT hr = S_OK;
    
    hr = m_CamDriverUtils.Init(&CLSID_CameraDriver);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Failed setting up the camera data."));
        goto cleanup;
    }

    if(OEM_CERTIFY_TRUST == CeGetCallerTrust())
    {
        hr = m_CamDriverUtils.SetupNULLCameraDriver();
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed setting up the NULL camera data."));
            goto cleanup;
        }
    }

    // if the null camera device is available, select it.
    // otherwise we'll default to the first entry given.
    if(m_CamDriverUtils.GetNULLCameraDriverIndex() >= 0)
    {
        hr = SelectCameraDevice(m_CamDriverUtils.GetNULLCameraDriverIndex());
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed selecting the NULL driver ."));
            goto cleanup;
        }
    }
    else hr = SelectCameraDevice(0);

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::GetDriverList(TCHAR ***tszCamDeviceName, int *nEntryCount)
{
    return m_CamDriverUtils.GetDriverList(tszCamDeviceName, nEntryCount);
}

HRESULT
CCaptureFramework::SelectCameraDevice(TCHAR *tszCamDeviceName)
{
    HRESULT hr = E_FAIL;
    int nCameraIndex;

    nCameraIndex = m_CamDriverUtils.GetDeviceIndex(tszCamDeviceName);

    if(nCameraIndex != -1)
        hr = SelectCameraDevice(nCameraIndex);

    return hr;
}

HRESULT
CCaptureFramework::SelectCameraDevice(int nCameraDevice)
{
    if(nCameraDevice >= 0 && nCameraDevice <= m_CamDriverUtils.GetMaxDeviceIndex())
    {
        m_nSelectedCameraDevice = nCameraDevice;

        // if we aren't trusted, then the fact of using the null driver is irrelevent because we cannot do
        // automatic verification (since we cannot modify the registry to adjust the drivers capabilities).
        if(OEM_CERTIFY_TRUST == CeGetCallerTrust())
        {
            m_bUsingNULLDriver = (nCameraDevice == m_CamDriverUtils.GetNULLCameraDriverIndex());
        }
        else m_bUsingNULLDriver = FALSE;

        return S_OK;
    }

    return E_FAIL;
}


HRESULT
CCaptureFramework::Init(HWND OwnerWnd, RECT *rcCoordinates, DWORD dwComponentsOptions)
{
    HRESULT hr = S_OK;

    if(!rcCoordinates)
    {
        FAIL(TEXT("CCaptureFramework: Invalid rectangle pointer given."));
        hr = E_FAIL;
        goto cleanup;
    }

    m_dwComponentsInUse = dwComponentsOptions & COMPONENTS_MASK;
    m_dwOptionsInUse = dwComponentsOptions & OPTIONS_MASK;
    m_hwndOwner = OwnerWnd;

    memcpy(&m_rcLocation, rcCoordinates, sizeof(RECT));

    // Launch COM
    if(SUCCEEDED(hr = CoInitialize( NULL )))
    {
        m_bCoInitialized = TRUE;
        // create all of the components we're going to use
        if(SUCCEEDED(hr = CoCreateComponents()))
            // adding interfaces must happen after the components are created
            if(SUCCEEDED(hr = AddInterfaces()))
                // initializing the property bag must happen after the interfaces are added.
                if(SUCCEEDED(hr = InitFilters()))
                    // adding filters needs to be done after the interfaces are added.
                    if(SUCCEEDED(hr = AddFilters()))
                        // finding interfaces on the graph must be done after the filters are added to the graph.
                        if(SUCCEEDED(hr = FindInterfaces()))
                            // if the user pre-set some stream formats for us to use, set them now before rendering
                            if(SUCCEEDED(hr = InitializeStreamFormats()))
                                // render the graph once everything is set up.
                                if(SUCCEEDED(hr = RenderGraph()))
                                    if(SUCCEEDED(hr = InsertDiagnosticsFilters()))
                                        m_bInitialized = TRUE;

        if(m_bOutputFilterInformation)
            DisplayFilters();
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::InitCore(HWND OwnerWnd, RECT *rcCoordinates, DWORD dwComponentsOptions)
{
    HRESULT hr = S_OK;

    if(!rcCoordinates)
    {
        FAIL(TEXT("CCaptureFramework: Invalid rectangle pointer given."));
        hr = E_FAIL;
        goto cleanup;
    }

    m_dwComponentsInUse = dwComponentsOptions & COMPONENTS_MASK;
    m_dwOptionsInUse = dwComponentsOptions & OPTIONS_MASK;
    m_hwndOwner = OwnerWnd;

    memcpy(&m_rcLocation, rcCoordinates, sizeof(RECT));

    // Launch COM
    if(SUCCEEDED(hr = CoInitialize( NULL )))
    {
        m_bCoInitialized = TRUE;
        // create all of the components we're going to use
        if(SUCCEEDED(hr = CoCreateComponents()))
            // adding interfaces must happen after the components are created
            if(SUCCEEDED(hr = AddInterfaces()))
                // initializing the property bag must happen after the interfaces are added.
                if(SUCCEEDED(hr = InitFilters()))
                    // adding filters needs to be done after the interfaces are added.
                    if(SUCCEEDED(hr = AddFilters()))
                        // finding interfaces on the graph must be done after the filters are added to the graph.
                        if(SUCCEEDED(hr = FindInterfaces()))
                            // we're core initialized, but not fully initialized, make sure the current state represents that.
                            m_bInitialized = FALSE;
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::FinalizeGraph()
{
    HRESULT hr = S_OK;

    // if the user pre-set some stream formats for us to use, set them now before rendering
    if(SUCCEEDED(hr = InitializeStreamFormats()))
        // complete the initialization now.
        if(SUCCEEDED(hr = RenderGraph()))
            if(SUCCEEDED(hr = InsertDiagnosticsFilters()))
                m_bInitialized = TRUE;

    if(m_bOutputFilterInformation)
        DisplayFilters();

    return hr;
}

HRESULT
CCaptureFramework::CoCreateComponents()
{
    HRESULT hr = S_OK;

    hr = CoCreateInstance( CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**) &m_pGraph );
    if(FAILED(hr) || m_pGraph == NULL)
    {
        FAIL(TEXT("CCaptureFramework: CoCreateInstance of the filter graph failed."));
        goto cleanup;
    }

    if(m_dwComponentsInUse & VIDEO_CAPTURE_FILTER)
    {
        hr = CoCreateInstance( CLSID_VideoCapture, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**) &m_pVideoCapture );
        if(FAILED(hr) || m_pVideoCapture == NULL)
        {
            FAIL(TEXT("CCaptureFramework: CoCreateInstance of the video capture failed."));
            goto cleanup;
        }
    }

    if(m_dwComponentsInUse & AUDIO_CAPTURE_FILTER)
    {
        hr = CoCreateInstance( CLSID_AudioCapture, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**) &m_pAudioCapture );
        if(FAILED(hr) || m_pAudioCapture == NULL)
        {
            FAIL(TEXT("CCaptureFramework: CoCreateInstance of the audio capture failed."));
            goto cleanup;
        }
    }

    if(m_dwComponentsInUse & STILL_IMAGE_SINK)
    {
        hr = CoCreateInstance( CLSID_IMGSinkFilter, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**) &m_pStillSink );
        if(FAILED(hr) || m_pStillSink == NULL)
        {
            FAIL(TEXT("CCaptureFramework: CoCreateInstance of the image sink failed."));
            goto cleanup;
        }
    }

    if(m_dwComponentsInUse & VIDEO_RENDERER)
    {
        hr = CoCreateInstance( CLSID_VideoRenderer, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**) &m_pVideoRenderer );
        if(FAILED(hr) || m_pVideoRenderer == NULL)
        {
            FAIL(TEXT("CCaptureFramework: CoCreateInstance of the video renderer failed."));
            goto cleanup;
        }
    }

    if(m_bForceColorConversion)
    {
        if(m_dwComponentsInUse & VIDEO_RENDERER || m_dwComponentsInUse & VIDEO_RENDERER_INTELI_CONNECT)
        {
            hr = CoCreateInstance( CLSID_Colour, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**) &m_pColorConverter );
            if(FAILED(hr) || m_pColorConverter == NULL)
            {
                DETAIL(TEXT("Creating the color converter failed."));
            }
        }
    }

    // if the user requested a video encoder and the video encoder guid has been set, then add it.
    if(m_dwComponentsInUse & VIDEO_ENCODER && m_clsidVideoEncoder != GUID_NULL)
    {
        hr = CoCreateInstance( CLSID_DMOWrapperFilter, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**) &m_pVideoDMOWrapperFilter );
        if(FAILED(hr) || m_pVideoDMOWrapperFilter == NULL)
        {
            FAIL(TEXT("CCaptureFramework: CoCreateInstance of the video encoder failed."));
            goto cleanup;
        }
    }

    // if the user requested an audio encoder and the audio encoder guid has been set, then add it.
    if(m_dwComponentsInUse & AUDIO_ENCODER  && m_clsidAudioEncoder != GUID_NULL)
    {
        hr = CoCreateInstance( CLSID_DMOWrapperFilter, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**) &m_pAudioDMOWrapperFilter );
        if(FAILED(hr) || m_pAudioDMOWrapperFilter == NULL)
        {
            FAIL(TEXT("CCaptureFramework: CoCreateInstance of the audio encoder failed."));
            goto cleanup;
        }
    }

    // Create our capture graph builder
    hr = CoCreateInstance( CLSID_CaptureGraphBuilder, NULL, CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void**) &m_pCaptureGraphBuilder );
    if(FAILED(hr) || m_pCaptureGraphBuilder == NULL)
    {
        FAIL(TEXT("CCaptureFramework: CoCreateInstance of the capture graph builder failed."));
        goto cleanup;
    }

cleanup:

    return hr;
}


HRESULT
CCaptureFramework::AddInterfaces()
{
    HRESULT hr = S_OK;

    if(m_pGraph == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Core components needed for AddInterfaces unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    // these components always exist.
    hr = m_pGraph.QueryInterface( &m_pMediaControl );
    if(FAILED(hr) || m_pMediaControl == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Querying for the IMedia control failed."));
        goto cleanup;
    }

    hr = m_pGraph.QueryInterface( &m_pMediaEvent );
    if(FAILED(hr) || m_pMediaEvent == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Querying for the IMediaEvent failed"));
        goto cleanup;
    }

    // these are all option components which may not be available.
    // if they aren't available, skip them.
    if(m_pVideoRenderer)
    {
        hr = m_pVideoRenderer.QueryInterface( &m_pVideoWindow );
        if(FAILED(hr) || m_pVideoWindow == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the IVideoWindow failed."));
            goto cleanup;
        }

        hr = m_pVideoRenderer.QueryInterface( &m_pVideoRendererMode );
        if(FAILED(hr) || m_pVideoRendererMode == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Unable to retrieve the video renderer mode."));
            goto cleanup;
        }

        hr = m_pVideoRenderer.QueryInterface( &m_pVideoRendererBasicVideo );
        if(FAILED(hr) || m_pVideoRendererBasicVideo == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Unable to retrieve the video renderer BasicVideo interface."));
            goto cleanup;
        }
    }

    if(m_pStillSink)
    {
        hr = m_pStillSink.QueryInterface( &m_pStillFileSink );
        if(FAILED(hr) || m_pStillFileSink == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the still file sink failed."));
            goto cleanup;
        }

        hr = m_pStillSink.QueryInterface( &m_pImageSinkFilter );
        if(FAILED(hr) || m_pImageSinkFilter == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the Still image filter interface failed."));
            goto cleanup;
        }
    }

    if(m_pAudioCapture)
    {
        hr = m_pAudioCapture.QueryInterface( &m_pAudioPropertyBag );
        if(FAILED(hr) || m_pAudioPropertyBag == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the audio capture property bag failed."));
            goto cleanup;
        }
    }

    if(m_pVideoCapture)
    {
        hr = m_pVideoCapture.QueryInterface( &m_pVideoControl );
        if(FAILED(hr) || m_pVideoControl == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the video control interface failed."));
            goto cleanup;
        }

        hr = m_pVideoCapture.QueryInterface( &m_pCameraControl );
        if(FAILED(hr) || m_pCameraControl == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the camera control interface failed."));
            goto cleanup;
        }

        hr = m_pVideoCapture.QueryInterface( &m_pDroppedFrames );
        if(FAILED(hr) || m_pDroppedFrames == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the dropped frames interface failed."));
            goto cleanup;
        }

        hr = m_pVideoCapture.QueryInterface( &m_pVideoProcAmp );
        if(FAILED(hr) || m_pVideoProcAmp == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the video proc amp interface failed."));
            goto cleanup;
        }

        hr = m_pVideoCapture.QueryInterface( &m_pKsPropertySet );
        if(FAILED(hr) || m_pKsPropertySet == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the property set interface failed."));
            goto cleanup;
        }

        hr = m_pVideoCapture.QueryInterface( &m_pVideoPropertyBag );
        if(FAILED(hr) || m_pVideoPropertyBag == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the video capture property bag failed."));
            goto cleanup;
        }
    }

    // if the encoder guid isn't set, then we can't setup the encoder.
    if(m_pVideoDMOWrapperFilter)
    {
        hr = m_pVideoDMOWrapperFilter.QueryInterface( &m_pVideoEncoderDMO );
        if(FAILED(hr) || m_pVideoEncoderDMO == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the video DMO Wrapper failed."));
            goto cleanup;
        }
    }

    // if the encoder guid isn't set, then we can't setup the encoder.
    if(m_pAudioDMOWrapperFilter)
    {
        hr = m_pAudioDMOWrapperFilter.QueryInterface( &m_pAudioEncoderDMO );
        if(FAILED(hr) || m_pAudioEncoderDMO == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the audio DMO Wrapper failed."));
            goto cleanup;
        }
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::InitFilters()
{
    HRESULT hr = S_OK;
    CComVariant   varCamName;
    CPropertyBag  PropBag;

    if(m_nSelectedCameraDevice >= 0)
    {
        // Initialize the video capture filter
        VariantInit(&varCamName);
        VariantClear(&varCamName);

        // use our currently selected camera device.
        varCamName = m_CamDriverUtils.GetDeviceName(m_nSelectedCameraDevice);
        hr = PropBag.Write( L"VCapName", &varCamName );   
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Writing the camera name to the property bag failed."));
            goto cleanup;
        }

        if(m_pVideoPropertyBag)
        {
            hr = m_pVideoPropertyBag->Load( &PropBag, NULL );
            if(FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: Loading the video property bag data failed."));
                goto cleanup;
            }
        }

        if(m_pAudioPropertyBag)
        {
            hr = m_pAudioPropertyBag->Load(NULL, NULL);
            if(FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: Loading the audio property bag data failed."));
                goto cleanup;
            }
        }

    }

    if(m_pVideoEncoderDMO)
    {
        hr = m_pVideoEncoderDMO->Init( m_clsidVideoEncoder, DMOCATEGORY_VIDEO_ENCODER );
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Initializing the Video DMO wrapper failed."));
            goto cleanup;
        }
    }

    if(m_pAudioEncoderDMO)
    {
        hr = m_pAudioEncoderDMO->Init( m_clsidAudioEncoder, DMOCATEGORY_AUDIO_ENCODER );
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Initializing the Audio DMO wrapper failed."));
            goto cleanup;
        }
    }

    if(m_pStillSink)
    {
        if(OEM_CERTIFY_TRUST == CeGetCallerTrust())
        {
            hr = SetupQualityRegistryKeys();
            if(FAILED(hr))
            {
                FAIL(TEXT("InitFilters: Failed to setup still image quality registry keys."));
                goto cleanup;
            }
        }
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::AddFilters()
{
    HRESULT hr = S_OK;

    if(m_pGraph == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Core components needed for AddFilters unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    if(m_pVideoCapture)
    {
        // add the various filters
        hr = m_pGraph->AddFilter(m_pVideoCapture, TEXT("VideoCapture"));
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Adding the VideoCapture filter failed."));
            goto cleanup;
        }
    }

    if(m_pAudioCapture)
    {
        // add the various filters
        hr = m_pGraph->AddFilter(m_pAudioCapture, TEXT("AudioCapture"));
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Adding the VideoCapture filter failed."));
            goto cleanup;
        }
    }

    if(m_pStillSink)
    {
        hr = m_pGraph->AddFilter( m_pStillSink, TEXT("Image Sink Filter"));
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Adding the image sink filter failed."));
            goto cleanup;
        }
    }

    if(m_pVideoRenderer)
    {
        hr = m_pGraph->AddFilter(m_pVideoRenderer, TEXT("VideoRenderer"));
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Adding the video renderer failed."));
            goto cleanup;
        }
    }

    if(m_pColorConverter)
    {
        hr = m_pGraph->AddFilter(m_pColorConverter, TEXT("Color Converter"));
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Adding the color converter failed."));
            goto cleanup;
        }
    }

    if(m_pVideoDMOWrapperFilter)
    {
        hr = m_pGraph->AddFilter(m_pVideoDMOWrapperFilter, TEXT("Video Encoder"));
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Adding the video encoder failed."));
            goto cleanup;
        }
    }

    if(m_pAudioDMOWrapperFilter)
    {
        hr = m_pGraph->AddFilter(m_pAudioDMOWrapperFilter, TEXT("Audio Encoder"));
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Adding the audio encoder failed."));
            goto cleanup;
        }
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::FindInterfaces()
{
    HRESULT hr = S_OK;

    if(m_pCaptureGraphBuilder == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Core components needed for AddInterfaces unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    // if the video capture filter is available, find all of it's stream config interfaces
    if(m_pVideoCapture)
    {
        // find the preview interface, start looking at the video capture filter.
        // if this fails, then we have a two pin driver, otherwise we have a three pin driver.
        hr = m_pCaptureGraphBuilder->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, m_pVideoCapture, IID_IAMStreamConfig, (void **) &m_pPreviewStreamConfig);
        if(FAILED(hr) || m_pPreviewStreamConfig == NULL)
        {
            m_uiPinCount = 2;
        }
        else m_uiPinCount = 3;

        // find the still interface, start looking at the video capture filter.
        hr = m_pCaptureGraphBuilder->FindInterface(&PIN_CATEGORY_STILL, &MEDIATYPE_Video, m_pVideoCapture, IID_IAMStreamConfig, (void **) &m_pStillStreamConfig);
        if(FAILED(hr) || m_pStillStreamConfig == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the still stream config failed."));
            goto cleanup;
        }

        // find the capture interface, start looking at the video capture filter.
        hr = m_pCaptureGraphBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pVideoCapture, IID_IAMStreamConfig, (void **) &m_pCaptureStreamConfig);
        if(FAILED(hr) || m_pCaptureStreamConfig == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the capture stream config failed."));
            goto cleanup;
        }
    }

    // now find the stream config interfaces for the audio capture filter.
    if(m_pAudioCapture)
    {
        // find the audio interface, start looking at the video capture filter.
        hr = m_pCaptureGraphBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, m_pAudioCapture, IID_IAMStreamConfig, (void **) &m_pAudioStreamConfig);
        if(FAILED(hr) || m_pAudioStreamConfig == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the audio stream config failed."));
            goto cleanup;
        }
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::InitializeStreamFormats()
{
    HRESULT hr = S_OK;

    if(m_pPreviewStreamConfig && m_pamtPreviewStream)
    {
        if(FAILED(hr = m_pPreviewStreamConfig->SetFormat(m_pamtPreviewStream)))
        {
            FAIL(TEXT("CCaptureFramework: Unable to initialize the preview format."));
            goto cleanup;
        }
    }

    if(m_pStillStreamConfig && m_pamtStillStream)
    {
        if(FAILED(hr = m_pStillStreamConfig->SetFormat(m_pamtStillStream)))
        {
            FAIL(TEXT("CCaptureFramework: Unable to initialize the still format."));
            goto cleanup;
        }
    }

    if(m_pCaptureStreamConfig && m_pamtCaptureStream)
    {
        if(FAILED(hr = m_pCaptureStreamConfig->SetFormat(m_pamtCaptureStream)))
        {
            FAIL(TEXT("CCaptureFramework: Unable to initialize the capture format."));
            goto cleanup;
        }
    }
    
    if(m_pAudioStreamConfig)
    {
        // if an audio format has been set, then use it
        if(m_pamtAudioStream)
        {
            if(FAILED(hr = m_pAudioStreamConfig->SetFormat(m_pamtAudioStream)))
            {
                FAIL(TEXT("CCaptureFramework: Unable to initialize the audio format."));
                goto cleanup;
            }
        }
        // otherwise, if GSM was requested, then use it instead.
        else if(m_bUseGSM)
        {
            if(FAILED(hr = m_pAudioStreamConfig->SetFormat(&g_mt)))
            {
                FAIL(TEXT("CCaptureFramework: Unable to initialize the GSM audio format."));
                goto cleanup;
            }
        }
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::ConfigureASF()
{
    HRESULT hr = S_OK;
    BYTE    pwVideoCodecInfo[4] = {0};
    BYTE    pwAudioCodecInfo[2] = {0};
    WORD    wIndex = 0;

    if(m_pMuxFilter == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Core components needed for ConfigureASF unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = m_pMuxFilter.QueryInterface( &m_pHeaderInfo );
    if(FAILED(hr) || m_pHeaderInfo == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Retrieving the WMHeaderInfo pointer failed."));
        goto cleanup;
    }

    // if the video capture filter was selected, configure for video
    if(m_pVideoCapture)
    {
        // Let's add the video codec infos
        pwVideoCodecInfo[0] = 0x57;
        pwVideoCodecInfo[1] = 0x4d;
        pwVideoCodecInfo[2] = 0x56;
        pwVideoCodecInfo[3] = 0x33;

        hr = m_pHeaderInfo->AddCodecInfo( L"Windows Media Video 9", L"Professional", WMT_CODECINFO_VIDEO, 4, pwVideoCodecInfo );
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Adding WMVideo codec failed."));
            goto cleanup;
        }
    }

    // if the audio capture filter was selected, configure for audio
    if(m_pAudioCapture)
    {
        // Let's add the audio codec infos
        pwAudioCodecInfo[0] = 0x01;
        pwAudioCodecInfo[1] = 0x00;

        hr = m_pHeaderInfo->AddCodecInfo( L"PCM", L"22.050 kHz, 8 Bit, Stereo", WMT_CODECINFO_AUDIO, 2, pwAudioCodecInfo );
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Adding WMAudio codec failed."));
            goto cleanup;
        }
    }

    hr = m_pHeaderInfo->AddAttribute( 0, L"title", &wIndex, WMT_TYPE_STRING, 0, (BYTE*) L"Title of the ASF File", 0 );
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Setting the title attribute failed."));
        goto cleanup;
    }

    hr = m_pHeaderInfo->AddAttribute( 0, L"author", &wIndex, WMT_TYPE_STRING, 0, (BYTE*) L"Author of the ASF File", 0 );
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Setting the author attribute failed."));
        goto cleanup;
    }

    hr = m_pHeaderInfo->AddAttribute( 0, L"copyright", &wIndex, WMT_TYPE_STRING, 0, (BYTE*) L"Copyright of the ASF File", 0 );
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Setting the copyright attribute failed."));
        goto cleanup;
    }

    hr = m_pHeaderInfo->AddAttribute( 0, L"description", &wIndex, WMT_TYPE_STRING, 0, (BYTE*) L"Description of the ASF File", 0 );
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Setting the description attribute failed."));
        goto cleanup;
    }

    hr = m_pHeaderInfo->AddAttribute( 0, L"rating", &wIndex, WMT_TYPE_STRING, 0, (BYTE*) L"Rating of the ASF File", 0 );
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Setting the rating attribute failed."));
        goto cleanup;
    }
        
    hr = m_pHeaderInfo->AddAttribute( 0, L"WMFSDKVersion", &wIndex, WMT_TYPE_STRING, 0, (BYTE*) L"8.00.00.4486", 2 * ( wcslen( L"8.00.00.4486" ) + 1) );
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Setting the WMFSDKVersion attribute failed."));
        goto cleanup;
    }

    hr = m_pHeaderInfo->AddAttribute( 0, L"WMFSDKNeeded", &wIndex, WMT_TYPE_STRING, 0, (BYTE*) L"0.0.0.0000", 2 * ( wcslen( L"0.0.0.0000" ) + 1) );
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Setting the WMFSDKNeeded attribute failed."));
        goto cleanup;
    }

cleanup:
    return hr;
}

HRESULT 
CCaptureFramework::SetupQualityRegistryKeys()
{
    HRESULT hr;

    hr = EnsureQualityKeyCreated(QUALITY_LOW);
    if (FAILED(hr))
    {
        Log(TEXT("Could not create or verify low quality key, hr = 0x%08x"), hr);
        return hr;
    }
    hr = EnsureQualityKeyCreated(QUALITY_MEDIUM);
    if (FAILED(hr))
    {
        Log(TEXT("Could not create or verify medium quality key, hr = 0x%08x"), hr);
        return hr;
    }
    hr = EnsureQualityKeyCreated(QUALITY_HIGH);
    if (FAILED(hr))
    {
        Log(TEXT("Could not create or verify high quality key, hr = 0x%08x"), hr);
        return hr;
    }
    return S_OK;
}

HRESULT
CCaptureFramework::EnsureQualityKeyCreated(QUALITY Quality)
{
    HRESULT hr = S_OK;
    HKEY hkSettings = NULL;
    HKEY hkQuality = NULL;
    LONG lRet;
    // In CE, the maximum length of a key name is 255
    WCHAR wszQualityKey[256];
    WCHAR wszQualityGUID[50];
    DWORD dwDisposition;
    DWORD dwTemp;
    LONG lQuality;
    
    StringCchPrintf(wszQualityKey, countof(wszQualityKey),L"SOFTWARE\\Microsoft\\DirectShow\\ImageSink\\Quality\\%d\\jpg", (DWORD)Quality);
    
    lRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, wszQualityKey, 0 , L"", REG_OPTION_VOLATILE, 0, NULL, &hkSettings, &dwDisposition);
    if (lRet != ERROR_SUCCESS)
    {
        FAIL(TEXT("Could not open or create registry key for Settings."));
        hr = HRESULT_FROM_WIN32(lRet);
        goto Cleanup;
    }

    if (REG_OPENED_EXISTING_KEY == dwDisposition)
    {
        //Log(TEXT("Settings key %d already exists, not creating"), Quality);
        goto Cleanup;
    }

    lRet = RegCreateKeyEx(hkSettings, L"Quality", 0 , L"", REG_OPTION_VOLATILE, 0, NULL, &hkQuality, &dwDisposition);
    if (lRet != ERROR_SUCCESS)
    {
        Log(TEXT("Could not open or create registry key for Quality, GetLastError returns %d"), GetLastError());
        hr = HRESULT_FROM_WIN32(lRet);
        goto Cleanup;
    }
    
    if (!StringFromGUID2(ENCODER_QUALITY, wszQualityGUID, countof(wszQualityGUID)))
    {
        Log(TEXT("Need larger GUID string"));
        hr = E_FAIL;
        goto Cleanup;
    }

    // Set "GUID", type REG_SZ
    lRet = RegSetValueEx(hkQuality, L"GUID", 0, REG_SZ, (BYTE*)wszQualityGUID, (wcslen(wszQualityGUID) + 1) * sizeof(wszQualityGUID[0]));
    if (ERROR_SUCCESS != lRet)
    {
        Log(TEXT("Unable to set GUID value, GLE = %d, GUID = %ls"), GetLastError(), wszQualityGUID);
        hr = HRESULT_FROM_WIN32(lRet);
        goto Cleanup;
    }
    
    // Set "NumberOfValues", type REG_DWORD
    dwTemp = 1;
    lRet = RegSetValueEx(hkQuality, L"NumberOfValues", 0, REG_DWORD, (BYTE*)&dwTemp, sizeof(dwTemp));
    if (ERROR_SUCCESS != lRet)
    {
        Log(TEXT("Unable to set NumberOfValues value, GLE = %d, NOV = %d"), GetLastError(), dwTemp);
        hr = HRESULT_FROM_WIN32(lRet);
        goto Cleanup;
    }
    
    // Set "Type", type REG_DWORD
    dwTemp = EncoderParameterValueTypeLong;
    lRet = RegSetValueEx(hkQuality, L"Type", 0, REG_DWORD, (BYTE*)&dwTemp, sizeof(dwTemp));
    if (ERROR_SUCCESS != lRet)
    {
        Log(TEXT("Unable to set Type value, GLE = %d, Type = %d"), GetLastError(), dwTemp);
        hr = HRESULT_FROM_WIN32(lRet);
        goto Cleanup;
    }

    // Set "Value", type REG_VALUE
    switch(Quality)
    {
    case QUALITY_LOW:
        lQuality = 25;
        break;
    case QUALITY_MEDIUM:
        lQuality = 65;
        break;
    case QUALITY_HIGH:
        lQuality = 90;
        break;
    }
    lRet = RegSetValueEx(hkQuality, L"Value", 0, REG_BINARY, (BYTE*)&lQuality, sizeof(lQuality));
    if (ERROR_SUCCESS != lRet)
    {
        Log(TEXT("Unable to set Value value, GLE = %d, Value = %d"), GetLastError(), lQuality);
        hr = HRESULT_FROM_WIN32(lRet);
        goto Cleanup;
    }
    
Cleanup:
    if (hkSettings)
    {
        RegCloseKey(hkSettings);
    }
    if (hkQuality)
    {
        RegCloseKey(hkQuality);
    }
    return hr;
}

HRESULT
CCaptureFramework::GetFramesProcessed(LONG *lVideoFramesProcessed, LONG *lAudioFramesProcessed)
{
    CAutoLock cObjectLock(&m_Lock);
    if(lVideoFramesProcessed && lAudioFramesProcessed)
    {
        *lVideoFramesProcessed = m_lVideoFramesProcessed;
        *lAudioFramesProcessed = m_lAudioFramesProcessed;
        return S_OK;
    }
    else return E_FAIL;
}

HRESULT
CCaptureFramework::GetFramesDropped(LONG *lVideoFramesDropped, LONG *lAudioFramesDropped)
{
    CAutoLock cObjectLock(&m_Lock);
    if(lVideoFramesDropped && lAudioFramesDropped)
    {
        *lVideoFramesDropped = m_lVideoFramesDropped;
        *lAudioFramesDropped = m_lAudioFramesDropped;
        return S_OK;
    }
    else return E_FAIL;
}

HRESULT
CCaptureFramework::GetPinCount(UINT *uiPinCount)
{
    if(m_uiPinCount != 0 && uiPinCount)
    {
        *uiPinCount = m_uiPinCount;
        return S_OK;
    }

    return E_FAIL;
}

HRESULT
// this renders the graph using the ICaptureGraphBuilder2 interface.
CCaptureFramework::RenderGraph()
{
    HRESULT hr = S_OK;
    WORD wCookie = 0;

    if(m_pGraph == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Core components needed for RenderGraph unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = m_pCaptureGraphBuilder->SetFiltergraph( m_pGraph );
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Setting the filter graph failed."));
        goto cleanup;
    }

    // if the video capture is in the graph, and so is the renderer, then connect up the preview pin
    if(m_pVideoCapture && m_pVideoRenderer)
    {
        // Render the streams
        hr = m_pCaptureGraphBuilder->RenderStream( &PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, m_pVideoCapture, m_pColorConverter, m_pVideoRenderer );
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Rendering the preview failed."));
            goto cleanup;
        }
    }
    else if(m_pVideoCapture && (m_dwComponentsInUse & VIDEO_RENDERER_INTELI_CONNECT))
    {
        // Render the streams
        // If Intelligent Connect, Specify NULL for renderer: this should automatically find the renderer and connect it
        hr = m_pCaptureGraphBuilder->RenderStream( &PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, m_pVideoCapture, m_pColorConverter, NULL );
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Rendering the preview failed."));
            goto cleanup;
        }

        // now propogate all of the components necessary for the video renderer, but wasn't already done earlier
        // because we just created the renderer.
        hr = m_pGraph->FindFilterByName(TEXT("Video Renderer"), &m_pVideoRenderer);
        if (FAILED(hr) || !m_pVideoRenderer)
        {
            FAIL(TEXT("CCaptureFramework: intelligent connect must have failed - the video renderer is missing."));
            goto cleanup;
        }
    
        hr = m_pVideoRenderer.QueryInterface( &m_pVideoWindow );
        if (FAILED(hr) || m_pVideoWindow == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the IVideoWindow from the filter graph failed."));
            goto cleanup;
        }

        hr = m_pVideoRenderer.QueryInterface( &m_pVideoRendererMode);
        if (FAILED(hr) || m_pVideoRendererMode == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the IAMVideoRendererMode from the filter graph failed."));
            goto cleanup;
        }

        hr = m_pVideoRenderer.QueryInterface( &m_pVideoRendererBasicVideo );
        if (FAILED(hr) || (m_pVideoRendererBasicVideo == NULL))
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the IBasicVideo from the filter graph failed."));
            goto cleanup;
        }
    }

    // if the ASF mux is requested, then configure it.
    if(m_dwComponentsInUse & FILE_WRITER)
    {
        // SetOutputFileName will attach the MUX to the file writer if they're separate entities, otherwise the mux is the file writer.
        hr = m_pCaptureGraphBuilder->SetOutputFileName( &m_clsidMux, VIDEO_CAPTURE_STATICFILE_NAME, &m_pMuxFilter, &m_pVideoFileSink );
        if(FAILED(hr) || m_pMuxFilter == NULL || m_pVideoFileSink == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Setting the output file name failed (creating the mux and file sink)."));
            goto cleanup;
        }

        // if the wm9 encoder was not selected, then it will be NULL, resulting in no encoder being used.
        if(m_pVideoCapture)
        {

            hr = m_pCaptureGraphBuilder->RenderStream( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pVideoCapture, m_pVideoDMOWrapperFilter, m_pMuxFilter);

            // if this fails, and we're using ASF then it's a problem.  If we're not using ASF, then it's perfectly valid (third party multiplexers do not necessarily
            // have to support raw audio/video or various media types.
            if(FAILED(hr) && m_clsidMux == CLSID_ASFWriter)
            {
                FAIL(TEXT("CCaptureFramework: Rendering the video capture stream to the MUX failed."));
                goto cleanup;
            }
            else if(FAILED(hr))
            {
                hr = E_GRAPH_UNSUPPORTED;
                goto cleanup;
            }

            // now that we've rendered the capture stream, find the buffering filter interfaces.
            hr = m_pCaptureGraphBuilder->FindInterface(&LOOK_DOWNSTREAM_ONLY, NULL, m_pVideoCapture, IID_IBuffering, (void **) &m_pVideoBuffering);
            if(FAILED(hr) || m_pVideoBuffering == NULL)
            {
                FAIL(TEXT("CCaptureFramework: Retrieving the video buffering interface failed."));
                goto cleanup;
            }
        }

        // we have the ASF mux, if we have the audio capture filter, then render it
        if(m_pAudioCapture)
        {
            // Audio capture has no stream type.
            hr = m_pCaptureGraphBuilder->RenderStream(NULL, &MEDIATYPE_Audio, m_pAudioCapture, m_pAudioDMOWrapperFilter, m_pMuxFilter);

            // if this fails, and we're using ASF then it's a problem.  If we're not using ASF, then it's perfectly valid (third party multiplexers do not necessarily
            // have to support raw audio/video or various media types.
            if(FAILED(hr)  && m_clsidMux == CLSID_ASFWriter)
            {
                FAIL(TEXT("CCaptureFramework: Rendering the audio capture stream to the ASF writer failed."));
                goto cleanup;
            }
            else if(FAILED(hr))
            {
                hr = E_GRAPH_UNSUPPORTED;
                goto cleanup;
            }

            // now that we've rendered the audio capture stream, find the buffering filter interfaces.
            hr = m_pCaptureGraphBuilder->FindInterface(&LOOK_DOWNSTREAM_ONLY, NULL, m_pAudioCapture, IID_IBuffering, (void **) &m_pAudioBuffering);
            if(FAILED(hr) || m_pAudioBuffering == NULL)
            {
                FAIL(TEXT("CCaptureFramework: Retrieving the audio buffering interface failed."));
                goto cleanup;
            }
        }

        // only configure the ASF data if we're using the ASF writer.
        if(m_clsidMux == CLSID_ASFWriter)
        {
            hr = ConfigureASF();
            if(FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: ASF configuration failed."));
                goto cleanup;
            }
        }

        // if we rendered the video capture to the ASF mux, then stop the video capture until it's triggered later.
        if (m_dwOptionsInUse & OPTION_SIMULT_CONTROL)
        {
            hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, NULL, NULL, 0, 0, 0, 0);
            if (FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: Failed to initialize the capture stream state."));
                goto cleanup;
            }
        }
        else
        {
            if(m_pVideoCapture)
            {
                hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pVideoCapture, 0, 0, 0, 0 );
                if(FAILED(hr))
                {
                    FAIL(TEXT("CCaptureFramework: Failed to initialize the video capture stream state."));
                    goto cleanup;
                }
            }

            if(m_pAudioCapture)
            {
                // Audio capture has no stream type.
                // if we have an ACF, then we rendered it above.  stop it intially so it's not causing a file write that wasn't requested.
                hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, m_pAudioCapture, 0, 0, 0, 0 );
                if(FAILED(hr))
                {
                    FAIL(TEXT("CCaptureFramework: Failed to initialize the audio capture stream state."));
                    goto cleanup;
                }
            }
        }
    }

    // if the still image sink was requested, render it to the VCF
    if(m_pStillSink && m_pVideoCapture)
    {
        hr = m_pCaptureGraphBuilder->RenderStream( &PIN_CATEGORY_STILL,   &MEDIATYPE_Video, m_pVideoCapture, NULL, m_pStillSink);
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Rendering the still failed."));
            goto cleanup;
        }

        hr = m_pCaptureGraphBuilder->FindPin( (IUnknown*) m_pVideoCapture, PINDIR_OUTPUT, &PIN_CATEGORY_STILL, &MEDIATYPE_Video, FALSE, 0, &m_pStillPin );
        if(FAILED(hr) && !m_pStillPin)
        {
            FAIL(TEXT("CCaptureFramework: Finding the still image pin failed."));
            goto cleanup;
        }
    }

    // if we have a video window available (we included the video renderer),
    // retrieve our default width/height and set the window location
    if(m_pVideoWindow)
    {
        hr = m_pVideoWindow->get_Width(&m_lDefaultWidth);
        // expected to fail if there's no VCF
        if(FAILED(hr) || !m_pVideoCapture)
        {
            DETAIL(TEXT("CCaptureFramework: Getting the default width failed."));
        }

        hr = m_pVideoWindow->get_Height(&m_lDefaultHeight);
        if(FAILED(hr))
        {
            DETAIL(TEXT("CCaptureFramework: Getting the default height failed."));
        }

        // if the user gave a valid owner window, then set it.  otherwise don't.
        if(m_hwndOwner)
        {
            // configure the video window
            hr = m_pVideoWindow->put_Owner((OAHWND) m_hwndOwner);
            if(FAILED(hr))
            {
                DETAIL(TEXT("CCaptureFramework: Setting the owner window failed."));
            }
        }

        // if the user gave a valid location rectangle to set, then set it, otherwise don't set it and we'll just use the default.
        if(m_rcLocation.left > 0 || m_rcLocation.top > 0 || m_rcLocation.right > 0 || m_rcLocation.bottom > 0)
        {
            hr = m_pVideoWindow->SetWindowPosition(m_rcLocation.left, m_rcLocation.top, m_rcLocation.right - m_rcLocation.left, m_rcLocation.bottom - m_rcLocation.top);
            if(FAILED(hr))
            {
                DETAIL(TEXT("CCaptureFramework: Setting the window position failed."));
            }
        }

        // update our internal location rectangle to reflect what is set in the system.
        // the position set may be different if the position requested isn't possible.
        hr = m_pVideoWindow->GetWindowPosition(&m_rcLocation.left, &m_rcLocation.top, &m_rcLocation.right, &m_rcLocation.bottom);
        if(FAILED(hr))
        {
            DETAIL(TEXT("CCaptureFramework: Setting the window position failed."));
        }
        m_rcLocation.right += m_rcLocation.left;
        m_rcLocation.bottom += m_rcLocation.top;
    }

    // if we're a two pin driver, find the smart tee filter now.
    // it could be inserted by either the capture or the preview depending on the order
    // and which components are in the graph. 
    // if we're a two pin driver but not doing video capture or preview, then the smart tee 
    // may not be in the graph.
    if(m_pVideoCapture && m_uiPinCount == 2 && (m_pVideoFileSink || m_pVideoRenderer))
    {
        // If this is a two pin driver, then we should find the smart tee filter in the graph
        hr = m_pGraph->FindFilterByName(TEXT("SmartTee"), &m_pSmartTeeFilter);
        if(FAILED(hr) || m_pSmartTeeFilter == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the smart tee filter interface failed."));
            goto cleanup;
        }
    }

    DShowEvent dse[2];
    memset(&dse, 0, sizeof(DShowEvent) * 2);

    dse[0].Code = EC_CAP_SAMPLE_PROCESSED;
    dse[0].FilterFlags = EVT_EVCODE_EQUAL | EVT_EXCLUDE;
    dse[1].Code = EC_BUFFER_FULL;
    dse[1].FilterFlags = EVT_EVCODE_EQUAL | EVT_EXCLUDE;

    // now that we're ready to go, intialize the media event sink
    hr = m_EventSink.Init(m_pMediaEvent, 2, dse);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Failed to initialize the media event sink."));
        goto cleanup;
    }

    if(FAILED(m_EventSink.SetEventCallback(&CCaptureFramework::ProcessMediaEvent, 0, NULL, this)))
    {
        FAIL(TEXT("CCaptureFramework: Failed to set the event callback handler."));
        goto cleanup;
    }

cleanup:
    return hr;
}

HRESULT CCaptureFramework::GrabberCallback(IMediaSample *pSample, REFERENCE_TIME *StartTime, REFERENCE_TIME *StopTime, BOOL TypeChanged, AM_MEDIA_TYPE *mtype, LPVOID lpUser)
{
    int FilterIndex = (int) lpUser;

    Log(TEXT("CaptureFramework: %s inserted %s received a sample with a time 0x%08x"), DiagnosticsData[FilterIndex][FILTER_NAME], DiagnosticsData[FilterIndex][FILTER_LOCATION], (DWORD) *StartTime);

    return S_OK;
}

HRESULT 
CCaptureFramework::InsertDiagnosticsFilters()
{
    HRESULT hr = S_OK;

    // first, if we're in diagnostics mode then we must verify that the grabber filter is available
    // and register it with the system if necessary.
    if(m_bInsertPreviewDiagnostics || m_bInsertStillDiagnostics || m_bInsertCaptureDiagnostics)
    {
        CComPtr<IBaseFilter> pGrabber;

        hr = CoCreateInstance( CLSID_CameraGrabber, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**) &pGrabber );
        if(FAILED(hr) || pGrabber == NULL)
        {
            // the grabber wasn't found, so we assume it's not registered and register it.
            if(S_OK != RegisterDll(TEXT("CameraGrabber.dll")))
                if(S_OK != RegisterDll(TEXT("\\hard disk\\CameraGrabber.dll")))
                    if(S_OK != RegisterDll(TEXT("\\release\\CameraGrabber.dll")))
                    {
                        FAIL(TEXT("CCaptureFramework: Unable to register the grabber filter dll which is necessary for diagnostics information."));
                        hr = E_FAIL;
                        goto cleanup;
                    }

            // try again now that the filter should be properly registered
            hr = CoCreateInstance( CLSID_CameraGrabber, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**) &pGrabber );
            if(FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: Failed to CoCreate the grabber filter even after registering it. Is the cameragrabber.dll available?"));
                goto cleanup;
            }
        }
        // free the grabber now that we're done with it.
        pGrabber = NULL;
    }

    // if everything succeeded above, let's get down to inserting the filters where we need them.
    if(SUCCEEDED(hr))
    {
        // if adding diagnostics to the still image pipe. only add it if the video capture filter and still image sink are in the graph.
        if(m_bInsertStillDiagnostics && m_pVideoCapture && m_pStillFileSink)
        {
            hr = InsertGrabberFilter(m_pVideoCapture, PINDIR_OUTPUT, &PIN_CATEGORY_STILL, NULL, 0, DIAGNOSTICSFILTER_STILLIMAGE);
            if(FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: Failed to insert the still image diagnostics filter."));
                goto cleanup;
            }
        }

        // if adding diagnostics to the preview pipe, intert the video renderer diagnostics filter just before the video renderer
        if(m_bInsertPreviewDiagnostics && m_pVideoCapture && m_pVideoRenderer)
        {
            hr = InsertGrabberFilter(m_pVideoRenderer, PINDIR_INPUT, NULL, NULL, 0, DIAGNOSTICSFILTER_VIDEORENDERER);
            if(FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: Failed to insert the preview diagnostics filter."));
                goto cleanup;
            }
        }

        // now for video capture diagnostics.
        if(m_bInsertCaptureDiagnostics)
        {
            // if video capture is there, then we insert the filters into the video capture
            // pipe, they go at the vcap output if there's no smart tee, or at the smart tee output if there is,
            // and at the video input of the mux
            if(m_pVideoCapture && m_pMuxFilter)
            {
                if(m_pSmartTeeFilter)
                {
                    hr = InsertGrabberFilter(m_pSmartTeeFilter, PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE, NULL, 0, DIAGNOSTICSFILTER_VIDEOCAPTURE);
                    if(FAILED(hr))
                    {
                        FAIL(TEXT("CCaptureFramework: Failed to insert the video capture diagnostics filter."));
                        goto cleanup;
                    }
                }
                else
                {
                    hr = InsertGrabberFilter(m_pVideoCapture, PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE, NULL, 0, DIAGNOSTICSFILTER_VIDEOCAPTURE);
                    if(FAILED(hr))
                    {
                        FAIL(TEXT("CCaptureFramework: Failed to insert the video capture diagnostics filter."));
                        goto cleanup;
                    }
                }

                {
                    // the media type on the video input pin of the multiplexer may be video or videobuffered,
                    // Figure out what kind of pin it is for the insertion.
                    CComPtr<IPin> pPin = NULL;
                    const GUID *pMediaType = &MEDIATYPE_Video;

                    if(SUCCEEDED(m_pCaptureGraphBuilder->FindPin(m_pMuxFilter, PINDIR_INPUT, NULL, &MEDIATYPE_VideoBuffered, FALSE, 0, &pPin)))
                        pMediaType = &MEDIATYPE_VideoBuffered;

                    pPin = NULL;

                    hr = InsertGrabberFilter(m_pMuxFilter, PINDIR_INPUT, NULL, pMediaType, 0, DIAGNOSTICSFILTER_VIDEOTOBEMUXED);
                    if(FAILED(hr))
                    {
                        FAIL(TEXT("CCaptureFramework: Failed to insert the video to be muxed diagnostics filter."));
                        goto cleanup;
                    }
                }
            }

            // if audio capture is there, then we put in the diagnostics filters
            // after the audio capture filter output, and at the multiplexer input for audio.
            if(m_pMuxFilter && m_pAudioCapture)
            {
                hr = InsertGrabberFilter(m_pAudioCapture, PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, 0, DIAGNOSTICSFILTER_AUDIOCAPTURE);
                if(FAILED(hr))
                {
                    FAIL(TEXT("CCaptureFramework: Failed to insert the audio capture diagnostics filter."));
                    goto cleanup;
                }


                {
                    // the media type on the audio input pin of the multiplexer may be audio or audiobuffered,
                    // Figure out what kind of pin it is for the insertion.
                    CComPtr<IPin> pPin = NULL;
                    const GUID *pMediaType = &MEDIATYPE_Audio;

                    if(SUCCEEDED(m_pCaptureGraphBuilder->FindPin(m_pMuxFilter, PINDIR_INPUT, NULL, &MEDIATYPE_AudioBuffered, FALSE, 0, &pPin)))
                        pMediaType = &MEDIATYPE_AudioBuffered;

                    pPin = NULL;

                    hr = InsertGrabberFilter(m_pMuxFilter, PINDIR_INPUT, NULL, pMediaType, 0, DIAGNOSTICSFILTER_AUDIOTOBEMUXED);
                    if(FAILED(hr))
                    {
                        FAIL(TEXT("CCaptureFramework: Failed to insert the audio to be muxed diagnostics filter."));
                        goto cleanup;
                    }
                }
            }
        }
    }

cleanup:
    return hr;
}


HRESULT
CCaptureFramework::SetVideoWindowPosition(RECT *rcCoordinates)
{
    HRESULT hr = S_OK;

    if(m_pVideoWindow == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Core components needed for SetVideoWindowPosition unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    if(!rcCoordinates)
    {
        FAIL(TEXT("CCaptureFramework: Invalid rectangle pointer given."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = m_pVideoWindow->SetWindowPosition(rcCoordinates->left, rcCoordinates->top, rcCoordinates->right - rcCoordinates->left, rcCoordinates->bottom - rcCoordinates->top);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Setting the window position failed."));
        goto cleanup;
    }

    // now that we've successfully moved the preview, update our internal location rectangle
    hr = m_pVideoWindow->GetWindowPosition(&m_rcLocation.left, &m_rcLocation.top, &m_rcLocation.right, &m_rcLocation.bottom);
    if(FAILED(hr))
    {
        DETAIL(TEXT("CCaptureFramework: Setting the window position failed."));
    }
    m_rcLocation.right += m_rcLocation.left;
    m_rcLocation.bottom += m_rcLocation.top;

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::SetVideoWindowSize(long Width, long Height)
{
    HRESULT hr = S_OK;

    if(m_pVideoWindow == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Core components needed for SetVideoWindowPosition unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = m_pVideoWindow->put_Width(Width);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Setting the window width failed."));
        goto cleanup;
    }

    // some heights can be negative because of bottom up vs top down.
    // the window height is always positive.
    hr = m_pVideoWindow->put_Height(abs(Height));
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Setting the window height failed."));
        goto cleanup;
    }

    // now that we've successfully moved the preview, update our internal location rectangle
    hr = m_pVideoWindow->GetWindowPosition(&m_rcLocation.left, &m_rcLocation.top, &m_rcLocation.right, &m_rcLocation.bottom);
    if(FAILED(hr))
    {
        DETAIL(TEXT("CCaptureFramework: Setting the window position failed."));
    }
    m_rcLocation.right += m_rcLocation.left;
    m_rcLocation.bottom += m_rcLocation.top;

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::SetVideoWindowVisible(BOOL bVisible)
{
    HRESULT hr = S_OK;

    if(m_pVideoWindow == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Core components needed for SetWindowVisible unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = m_pVideoWindow->put_Visible(bVisible?OATRUE:OAFALSE);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Showing/Hiding the video window failed."));
        goto cleanup;
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::GetBuilderInterface(LPVOID* ppint, REFIID riid)
{
    if(!ppint)
        return E_FAIL;

    // as we're grabbing the interface from the ccom pointer, and returning it to a test,
    // we need to manually addref it.

    if(riid == IID_IGraphBuilder)
    {
        IGraphBuilder * igb;
        *ppint = igb = m_pGraph;
        igb->AddRef();
    }
    else if(riid == IID_ICaptureGraphBuilder2)
    {
        ICaptureGraphBuilder2 *icb2;
        *ppint = icb2 = m_pCaptureGraphBuilder;
        icb2->AddRef();
    }
    else return E_FAIL;

    return S_OK;
}

HRESULT
CCaptureFramework::GetBaseFilter(LPVOID* ppint, DWORD component)
{
    HRESULT hr = S_OK;
    IBaseFilter *ibf = NULL;

    // as we're grabbing the interface from the ccom pointer, and returning it to a test,
    // we need to manually addref it.

    if(!ppint)
        return E_FAIL;

    switch(component)
    {
        case VIDEO_CAPTURE_FILTER:
            ibf = m_pVideoCapture;
            break;
        case STILL_IMAGE_SINK:
            ibf = m_pStillSink;
            break;
        case VIDEO_RENDERER:
            ibf = m_pVideoRenderer;
            break;
        case FILE_WRITER:
            ibf = m_pMuxFilter;
            break;
        case AUDIO_CAPTURE_FILTER:
            ibf = m_pAudioCapture;
            break;
        case VIDEO_ENCODER:
            ibf = m_pVideoDMOWrapperFilter;
            break;
        case AUDIO_ENCODER:
            ibf = m_pAudioDMOWrapperFilter;
            break;
        default:
            hr = E_FAIL;
            break;
    };

    if(ibf)
    {
        *ppint = ibf;
        ibf->AddRef();
    }

    return hr;
}

HRESULT
CCaptureFramework::RunGraph()
{
    HRESULT hr = S_OK;
    OAFilterState fsBefore;
    OAFilterState fsAfter;

    if(m_pMediaControl == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Core components needed for RunGraph unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = m_pMediaControl->GetState(MEDIAEVENT_TIMEOUT, &fsBefore);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Retrieving the graph state failed."));
        goto cleanup;
    }

    // BUGBUG: 107737 - telling a running graph to run again breaks the graph time
    if(fsBefore != State_Running)
    {
        hr = m_pMediaControl->Run();
        if(FAILED(hr))
        {
            DETAIL(TEXT("CCaptureFramework: Starting the graph failed."));
            goto cleanup;
        }
    }

    hr = m_pMediaControl->GetState(MEDIAEVENT_TIMEOUT, &fsAfter);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Retrieving the graph state failed."));
        goto cleanup;
    }

    if(fsAfter != State_Running)
    {
        FAIL(TEXT("CCaptureFramework: Failed to change the filter state."));
        goto cleanup;
    }

    // if the graph state was stopped, then set the new file name.  if it was running or paused, then don't because we may still be doing
    // a capture.  A previous capture doesn't end until the graph is stopped.
    if(fsBefore == State_Stopped)
    {
        // increment the file name before the capture (so the current file name is the file being captured until a new capture starts).
        m_lCaptureNumber++;

        // send the file name to the file sink now
        if(FAILED(hr = PropogateFilenameToCaptureFilter()))
        {
            FAIL(TEXT("CCaptureFramework: Failed to set the capture file name."));
            goto cleanup;
        }
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::PauseGraph()
{
    HRESULT hr = S_OK;

    if(m_pMediaControl == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Core components needed for PauseGraph unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = m_pMediaControl->Pause();
    if(FAILED(hr))
    {
        DETAIL(TEXT("CCaptureFramework: Pausing the graph failed."));
        goto cleanup;
    }

    OAFilterState fs;
    hr = m_pMediaControl->GetState(MEDIAEVENT_TIMEOUT, &fs);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Retrieving the graph state failed."));
        goto cleanup;
    }

    if(fs != State_Paused)
    {
        FAIL(TEXT("CCaptureFramework: Failed to change the filter state."));
        goto cleanup;
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::StopGraph()
{
    HRESULT hr = S_OK;

    if(m_pMediaControl == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Core components needed for StopGraph unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    // if something fails when stopping the graph, and we weren't initialized, then it's not necessarily a failure.
    // if we were initialized, then everything should succeed.

    hr = m_pMediaControl->Stop();
    if(FAILED(hr) && m_bInitialized)
    {
        FAIL(TEXT("CCaptureFramework: Stopping the graph failed."));
        goto cleanup;
    }

    OAFilterState fs;
    hr = m_pMediaControl->GetState(MEDIAEVENT_TIMEOUT, &fs);
    if(FAILED(hr) && m_bInitialized)
    {
        FAIL(TEXT("CCaptureFramework: Retrieving the graph state failed."));
        goto cleanup;
    }

    if(fs != State_Stopped && m_bInitialized)
    {
        FAIL(TEXT("CCaptureFramework: Failed to change the filter state."));
        goto cleanup;
    }

    // reset the streams now that it's been stopped
    // if we rendered the video capture to the ASF mux, then stop the video capture until it's triggered later so it doesn't
    // cause a file write that isn't requested.
    if(m_pMuxFilter)
    {
        if (m_dwOptionsInUse & OPTION_SIMULT_CONTROL)
        {
            hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, NULL, NULL, 0, 0, 0, 0 );
            if(FAILED(hr) && m_bInitialized)
            {
                FAIL(TEXT("CCaptureFramework: Failed to initialize the capture stream state."));
                goto cleanup;
            }
        }
        else {
            if(m_pVideoCapture)
            {
                hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pVideoCapture, 0, 0, 0, 0 );
                if(FAILED(hr) && m_bInitialized)
                {
                    FAIL(TEXT("CCaptureFramework: Failed to initialize the video capture stream state."));
                    goto cleanup;
                }
            }

            if(m_pAudioCapture)
            {
                // Audio capture has no stream type.
                // if we have an ACF, then we rendered it above.  stop it intially so it's not causing a file write that wasn't requested.
                hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, m_pAudioCapture, 0, 0, 0, 0 );
                if(FAILED(hr) && m_bInitialized)
                {
                    FAIL(TEXT("CCaptureFramework: Failed to initialize the audio capture stream state."));
                goto cleanup;
                }
            }
        }
    }

    m_Lock.Lock();
    // now that the graph is stopped, reset our state.
    m_lVideoFramesProcessed = 0;
    m_lAudioFramesProcessed = 0;
    m_bDiskWriteErrorOccured = FALSE;
    m_lVideoFramesDropped = 0;
    m_lAudioFramesDropped = 0;
    m_bBufferDeliveryFailed = FALSE;
    m_Lock.Unlock();

cleanup:
    return hr;
}

// we're copying the filter's behavior here in the case of templated files
HRESULT
CCaptureFramework::GetStillCaptureFileName(TCHAR *tszFileName, int *nStringLength)
{
    HRESULT hr = S_OK;
    TCHAR tszPictureName[MAX_PATH] = {NULL};
    int nCharactersStored = 0;

    if(NULL == nStringLength)
    {
        hr = E_FAIL;
        FAIL(TEXT("CCaptureFramework: invalid pointer given for the string length."));
        goto cleanup;
    }

    // _sntprintf may not null terminate if the string length is exactly the buffer size, so make sure we're null terminated.
    nCharactersStored = _sntprintf(tszPictureName, MAX_PATH - 1, m_tszStillCaptureName, m_lPictureNumber);
    tszPictureName[MAX_PATH - 1] = NULL;

    if(nCharactersStored < 0)
    {
        hr = E_FAIL;
        FAIL(TEXT("CCaptureFramework: internal buffer insufficient for the file name to store, file is larger than the file system can handle as a path."));
        goto cleanup;
    }

    // no buffer, or a buffer too small was given, make sure there's enough room for a NULL terminator
    if(NULL == tszFileName || nCharactersStored > (*nStringLength - 1))
    {
        // tell the user how big the string needs to be
        *nStringLength = nCharactersStored;
        hr = ERROR_INSUFFICIENT_BUFFER;
        goto cleanup;
    }

    // due to the check above, verifying that the buffer size is atleast one greater than the number of characters stored
    // we're guaranteed that _tcsncpy will null terminate the string.
    _tcsncpy(tszFileName, tszPictureName, *nStringLength);
    *nStringLength = nCharactersStored;
        
cleanup:
    return hr;
}

HRESULT
CCaptureFramework::GetVideoCaptureFileName(TCHAR *tszFileName, int *nStringLength)
{
    HRESULT hr = S_OK;
    TCHAR tszPictureName[MAX_PATH] = {NULL};
    int nCharactersStored = 0;

    if(NULL == nStringLength)
    {
        hr = E_FAIL;
        FAIL(TEXT("CCaptureFramework: invalid pointer given for the string length."));
        goto cleanup;
    }

    // _sntprintf may not null terminate if the string length is exactly the buffer size, so make sure we're null terminated.
    nCharactersStored = _sntprintf(tszPictureName, MAX_PATH - 1, m_tszVideoCaptureName, m_lCaptureNumber);
    tszPictureName[MAX_PATH - 1] = NULL;

    if(nCharactersStored < 0)
    {
        hr = E_FAIL;
        FAIL(TEXT("CCaptureFramework: internal buffer insufficient for the file name to store, file is larger than the file system can handle as a path."));
        goto cleanup;
    }

    // no buffer, or a buffer too small was given, make sure there's enough room for a NULL terminator
    if(NULL == tszFileName || nCharactersStored > (*nStringLength - 1))
    {
        // tell the user how big the string needs to be
        *nStringLength = nCharactersStored;
        hr = ERROR_INSUFFICIENT_BUFFER;
        goto cleanup;
    }

    // due to the check above, verifying that the buffer size is atleast one greater than the number of characters stored
    // we're guaranteed that _tcsncpy will null terminate the string.
    _tcsncpy(tszFileName, tszPictureName, *nStringLength);
    *nStringLength = nCharactersStored;
        
cleanup:
    return hr;
}

HRESULT
CCaptureFramework::GetFrameStatistics(long *plDropped, long *plNotDropped, long *plAvgFrameSize)
{
    HRESULT hr = S_OK;

    // just retrieve all of the frame data in one call, to simplify testing.  If any of the pointers given aren't valid,
    // then skip filling it out.  The only side effect is that if none of the pointers are valid, the call succeeds
    // in doing nothing.

    if(m_pDroppedFrames == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Core components needed for GetFrameStastics unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    if(plDropped)
    {
        if(FAILED(hr = m_pDroppedFrames->GetNumDropped(plDropped)))
        {
            FAIL(TEXT("CCaptureFramework: Failed to retrieve the number of dropped frames."));
            goto cleanup;
        }
    }

    if(plNotDropped)
    {
        if(FAILED(hr = m_pDroppedFrames->GetNumNotDropped(plNotDropped)))
        {
            FAIL(TEXT("CCaptureFramework: Failed to retrieve the number of frames not dropped."));
            goto cleanup;
        }
    }

    if(plAvgFrameSize)
    {
        if(FAILED(hr = m_pDroppedFrames->GetAverageFrameSize(plAvgFrameSize)))
        {
            FAIL(TEXT("CCaptureFramework: Failed to retrieve the average frame size."));
            goto cleanup;
        }
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::GetRange(int nProperty, long lProperty, long *lpMin, long *lpMax, long *lpStepping, long *lpDefault, long *lpFlags)
{
    HRESULT hr = S_OK;

    // retrieve the range on the given property and property set.  just a basic pass through to the middleware, 
    // but combining muliple property sets into a single double property funciton for the app.  First the app requests
    // whether it wants to query the camera control or vidprocamp property, then it gives the actual property 
    // it wants from either set.

    switch(nProperty)
    {
        case PROPERTY_CAMERACONTROL:
            if(FAILED(hr = m_pCameraControl->GetRange(lProperty, lpMin, lpMax, lpStepping, lpDefault, lpFlags)))
                DETAIL(TEXT("CCaptureFramework: Failed to retrieve the CameraControl range."));
            break;
        case PROPERTY_VIDPROCAMP:
            if(FAILED(hr = m_pVideoProcAmp->GetRange(lProperty, lpMin, lpMax, lpStepping, lpDefault, lpFlags)))
                DETAIL(TEXT("CCaptureFramework: Failed to retrieve the VideoProcAmp range"));
            break;
        default:
            hr = E_FAIL;
            FAIL(TEXT("CCaptureFramework: Invalid property requested."));
            break;
    };

    return hr;
}

HRESULT
CCaptureFramework::Get(int nProperty, long lProperty, long *lpValue, long *lpFlags)
{
    HRESULT hr = S_OK;

    // similar to GetRange, only for retrieving the current value

    switch(nProperty)
    {
        case PROPERTY_CAMERACONTROL:
            if(FAILED(hr = m_pCameraControl->Get(lProperty, lpValue, lpFlags)))
                DETAIL(TEXT("CCaptureFramework: Failed to retrieve the current CameraControl value."));
            break;
        case PROPERTY_VIDPROCAMP:
            if(FAILED(hr = m_pVideoProcAmp->Get(lProperty, lpValue, lpFlags)))
                DETAIL(TEXT("CCaptureFramework: Failed to retrieve the current VideoProcAmp value."));
            break;
        default:
            hr = E_FAIL;
            FAIL(TEXT("CCaptureFramework: Invalid property requested."));
            break;
    };

    return hr;
}

HRESULT
CCaptureFramework::Set(int nProperty, long lProperty, long lValue, long lFlags)
{
    HRESULT hr = S_OK;

    // similar to Get, only for setting the property value.

    switch(nProperty)
    {
        case PROPERTY_CAMERACONTROL:
            if(FAILED(hr = m_pCameraControl->Set(lProperty, lValue, lFlags)))
                DETAIL(TEXT("CCaptureFramework: Failed to set the VideoProcAmp value."));
            break;
        case PROPERTY_VIDPROCAMP:
            if(FAILED(hr = m_pVideoProcAmp->Set(lProperty, lValue, lFlags)))
                DETAIL(TEXT("CCaptureFramework: Failed to set the VideoProcAmp value."));
            break;
        default:
            hr = E_FAIL;
            FAIL(TEXT("CCaptureFramework: Invalid property requested."));
            break;
    };

    return hr;
}

HRESULT
CCaptureFramework::GetStreamCaps(int nStream, int nIndex, AM_MEDIA_TYPE **pFormat, BYTE *pSCC)
{
    HRESULT hr = S_OK;

    // we currently support 4 different streams, so first we select which stream we want to access the properties for, 
    // and then we pass through the applications request to that stream.

    switch(nStream)
    {
        case STREAM_PREVIEW:
            // if the component isn't available, fail the GetStreamCaps call.
            if(!m_pPreviewStreamConfig)
            {
                hr = VFW_E_NO_INTERFACE;
                break;
            }
            if(FAILED(hr = m_pPreviewStreamConfig->GetStreamCaps(nIndex, pFormat, pSCC)))
                DETAIL(TEXT("CCaptureFramework: Failed to get the preview stream caps."));
            break;
        case STREAM_STILL:
            // if the component isn't available, fail the GetStreamCaps call.
            if(!m_pStillStreamConfig)
            {
                hr = VFW_E_NO_INTERFACE;
                break;
            }
            if(FAILED(hr = m_pStillStreamConfig->GetStreamCaps(nIndex, pFormat, pSCC)))
                DETAIL(TEXT("CCaptureFramework: Failed to get the still stream caps."));
            break;
        case STREAM_CAPTURE:
            // if the component isn't available, fail the GetStreamCaps call.
            if(!m_pCaptureStreamConfig)
            {
                hr = VFW_E_NO_INTERFACE;
                break;
            }
            if(FAILED(hr = m_pCaptureStreamConfig->GetStreamCaps(nIndex, pFormat, pSCC)))
                DETAIL(TEXT("CCaptureFramework: Failed to get the capture stream caps."));
            break;
        case STREAM_AUDIO:
            // if the component isn't available, fail the GetStreamCaps call.
            if(!m_pAudioStreamConfig)
            {
                hr = VFW_E_NO_INTERFACE;
                break;
            }
            if(FAILED(hr = m_pAudioStreamConfig->GetStreamCaps(nIndex, pFormat, pSCC)))
                DETAIL(TEXT("CCaptureFramework: Failed to get the audio stream caps."));
            break;
        default:
            FAIL(TEXT("CCaptureFramework: Invalid stream requested."));
            hr = E_FAIL;
            break;
    };

    return hr;
}

HRESULT
CCaptureFramework::SetFormat(int nStream, AM_MEDIA_TYPE *pFormat)
{
    HRESULT hr = S_OK;
    AM_MEDIA_TYPE *pBackupFormat = NULL;
    
    // a NULL pFormat indicates to reset the stream format back to the default.

    // we currently support 4 different streams, so first we select which stream we want to access the properties for, 
    // and then we pass through the applications request to that stream.
    IAMStreamConfig *pStreamConfig;
    AM_MEDIA_TYPE **ppSetFormat;

    switch(nStream)
    {
        case STREAM_PREVIEW:
            pStreamConfig = m_pPreviewStreamConfig;
            ppSetFormat = &m_pamtPreviewStream;
            break;
        case STREAM_STILL:
            pStreamConfig = m_pStillStreamConfig;
            ppSetFormat = &m_pamtStillStream;
            break;
        case STREAM_CAPTURE:
            pStreamConfig = m_pCaptureStreamConfig;
            ppSetFormat = &m_pamtCaptureStream;
            break;
        case STREAM_AUDIO:
            pStreamConfig = m_pAudioStreamConfig;
            ppSetFormat = &m_pamtAudioStream;
            break;
    }

    // if the component isn't available, fail the SetFormat call.
    if(m_bInitialized && !pStreamConfig)
    {
        hr = VFW_E_NO_INTERFACE;
        return hr;
    }

    // grab the backup of the current format before we try to set it.
    pBackupFormat = *ppSetFormat;
    // make sure we don't inadvertantly delete it by storing it two places at once
    *ppSetFormat = NULL;

    // if a valid format was passed in, grab a copy of it
    if(pFormat)
    {
        *ppSetFormat = CreateMediaType(pFormat);
        if(!*ppSetFormat)
        {
            DETAIL(TEXT("CCaptureFramework: Unable to grab a copy of the format to be set."));
            hr = E_FAIL;
        }
    }

    // if we're initialized, then try to set it directly now.
    if(m_bInitialized && SUCCEEDED(hr) && FAILED(hr = pStreamConfig->SetFormat(*ppSetFormat)))
    {
        DETAIL(TEXT("CCaptureFramework: Unable to set the new format."));
    }

    // everything looks good, either we succeeded in setting the format,
    // or we weren't initialized, so delete the backup if it exists.
    if(SUCCEEDED(hr) && pBackupFormat)
    {
        DeleteMediaType(pBackupFormat);
        pBackupFormat = NULL;
    }
    else if(FAILED(hr))
    {
        // setting the new format failed, so restore the old format

        // delete the copy of the new one
        if(*ppSetFormat)
        {
            DeleteMediaType(*ppSetFormat);
            *ppSetFormat = NULL;
        }

        // restore the backup, and make sure we don't inadvertantly delete it.
        *ppSetFormat = pBackupFormat;
        pBackupFormat = NULL;
    }

    return hr;
}

HRESULT
CCaptureFramework::GetFormat(int nStream, AM_MEDIA_TYPE **pFormat)
{
    HRESULT hr = S_OK;

    // we currently support 4 different streams, so first we select which stream we want to access the properties for, 
    // and then we pass through the applications request to that stream.

    switch(nStream)
    {
        case STREAM_PREVIEW:
            // if the component isn't available, fail the GetFormat call.
            if(!m_pPreviewStreamConfig)
            {
                hr = VFW_E_NO_INTERFACE;
                break;
            }
            if(FAILED(hr = m_pPreviewStreamConfig->GetFormat(pFormat)))
                FAIL(TEXT("CCaptureFramework: Failed to retrieve the preview format."));
            break;
        case STREAM_STILL:
            // if the component isn't available, fail the GetFormat call.
            if(!m_pStillStreamConfig)
            {
                hr = VFW_E_NO_INTERFACE;
                break;
            }
            if(FAILED(hr = m_pStillStreamConfig->GetFormat(pFormat)))
                FAIL(TEXT("CCaptureFramework: Failed to retrieve the still format."));
            break;
        case STREAM_CAPTURE:
            // if the component isn't available, fail the GetFormat call.
            if(!m_pCaptureStreamConfig)
            {
                hr = VFW_E_NO_INTERFACE;
                break;
            }
            if(FAILED(hr = m_pCaptureStreamConfig->GetFormat(pFormat)))
                FAIL(TEXT("CCaptureFramework: Failed to retrieve the capture format."));
            break;
        case STREAM_AUDIO:
            // if the component isn't available, fail the GetFormat call.
            if(!m_pAudioStreamConfig)
            {
                hr = VFW_E_NO_INTERFACE;
                break;
            }
            if(FAILED(hr = m_pAudioStreamConfig->GetFormat(pFormat)))
                FAIL(TEXT("CCaptureFramework: Failed to retrieve the audio format."));
            break;
        default:
            FAIL(TEXT("CCaptureFramework: Invalid stream requested."));
            hr = E_FAIL;
            break;
    };

    return hr;
}

HRESULT
CCaptureFramework::GetNumberOfCapabilities(int nStream, int *piCount, int *piSize)
{
    HRESULT hr = S_OK;

    // we currently support 4 different streams, so first we select which stream we want to access the properties for, 
    // and then we pass through the applications request to that stream.

    switch(nStream)
    {
        case STREAM_PREVIEW:
            // if the component isn't available, fail the GetNumberOfCapabilities call.
            if(!m_pPreviewStreamConfig)
            {
                hr = VFW_E_NO_INTERFACE;
                break;
            }
            if(FAILED(hr = m_pPreviewStreamConfig->GetNumberOfCapabilities(piCount, piSize)))
                FAIL(TEXT("CCaptureFramework: Unable to retrieve the number of preview capabilities."));
            break;
        case STREAM_STILL:
            // if the component isn't available, fail the GetNumberOfCapabilities call.
            if(!m_pStillStreamConfig)
            {
                hr = VFW_E_NO_INTERFACE;
                break;
            }
            if(FAILED(hr = m_pStillStreamConfig->GetNumberOfCapabilities(piCount, piSize)))
                FAIL(TEXT("CCaptureFramework: Unable to retrieve the number of still capabilities."));
            break;
        case STREAM_CAPTURE:
            // if the component isn't available, fail the GetNumberOfCapabilities call.
            if(!m_pCaptureStreamConfig)
            {
                hr = VFW_E_NO_INTERFACE;
                break;
            }
            if(FAILED(hr = m_pCaptureStreamConfig->GetNumberOfCapabilities(piCount, piSize)))
                FAIL(TEXT("CCaptureFramework: Unable to retrieve the number of capture capabilities."));
            break;
        case STREAM_AUDIO:
            // if the component isn't available, fail the GetNumberOfCapabilities call.
            if(!m_pAudioStreamConfig)
            {
                hr = VFW_E_NO_INTERFACE;
                break;
            }
            if(FAILED(hr = m_pAudioStreamConfig->GetNumberOfCapabilities(piCount, piSize)))
                FAIL(TEXT("CCaptureFramework: Unable to retrieve the number of audio capabilities."));
            break;
        default:
            FAIL(TEXT("CCaptureFramework: Invalid stream requested."));
            hr = E_FAIL;
            break;
    };

    return hr;
}

HRESULT
CCaptureFramework::GetScreenOrientation(DWORD *dwOrientation)
{
    DEVMODE dm;

    *dwOrientation = 0;

    memset(&dm, 0, sizeof(DEVMODE));
    dm.dmSize = sizeof(DEVMODE);
    dm.dmFields = DM_DISPLAYORIENTATION;

    if(DISP_CHANGE_SUCCESSFUL == ChangeDisplaySettingsEx(NULL, &dm, NULL, CDS_TEST, NULL))
    {
        *dwOrientation = dm.dmDisplayOrientation;
    }

    return S_OK;
}

HRESULT
CCaptureFramework::SetScreenOrientation(DWORD dwOrientation)
{
    DEVMODE dm;
    HRESULT hr = E_FAIL;

    memset(&dm, 0, sizeof(DEVMODE));
    dm.dmSize = sizeof(DEVMODE);
    dm.dmFields = DM_DISPLAYORIENTATION;
    dm.dmDisplayOrientation = dwOrientation;

    if(DISP_CHANGE_SUCCESSFUL == ChangeDisplaySettingsEx(NULL, &dm, NULL, 0, NULL))
    {
        hr = S_OK;
    }

    return hr;
}

HRESULT
CCaptureFramework::GetCurrentPosition(REFERENCE_TIME *rt)
{
    CComPtr<IMediaSeeking> pMediaSeeking;
    HRESULT hr = S_OK;

    if(m_pGraph)
    {
        hr = m_pGraph.QueryInterface( &pMediaSeeking );
        if(FAILED(hr) || pMediaSeeking == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the MediaSeeking interface failed."));
        }
        else
        {
            hr = pMediaSeeking->GetCurrentPosition(rt);
            if(FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: Failed to retrieve the current position."));
            }
        }
    }
    else hr = VFW_E_NO_INTERFACE;

    return hr;
}

HRESULT
CCaptureFramework::GetBufferingDepth(int nStream, REFERENCE_TIME *rt)
{
    HRESULT hr = S_OK;

    switch(nStream)
    {
        case STREAM_CAPTURE:
            if(!m_pVideoBuffering)
            {
                hr = VFW_E_NO_INTERFACE;
                break;
            }
            hr = m_pVideoBuffering->GetBufferingDepth(rt);
            break;
        case STREAM_AUDIO:
            if(!m_pAudioBuffering)
            {
                hr = VFW_E_NO_INTERFACE;
                break;
            }
            hr = m_pAudioBuffering->GetBufferingDepth(rt);
            break;
    }

    return hr;
}

HRESULT
CCaptureFramework::SetBufferingDepth(int nStream, REFERENCE_TIME *rt)
{
    HRESULT hr = S_OK;

    switch(nStream)
    {
        case STREAM_CAPTURE:
            if(!m_pVideoBuffering)
            {
                hr = VFW_E_NO_INTERFACE;
                break;
            }
            hr = m_pVideoBuffering->SetBufferingDepth(rt);
            break;
        case STREAM_AUDIO:
            if(!m_pAudioBuffering)
            {
                hr = VFW_E_NO_INTERFACE;
                break;
            }
            hr = m_pAudioBuffering->SetBufferingDepth(rt);
            break;
    }

    return hr;
}

HRESULT
CCaptureFramework::TriggerStillImage()
{
    HRESULT hr = S_OK;

    if(!m_pStillPin)
    {
        hr = E_FAIL;
        DETAIL(TEXT("No still image pin available to trigger"));
        goto cleanup;
    }

    // it's still possible to set the mode to trigger a still image, with the 
    // file sink unavailable.
    hr = m_pVideoControl->SetMode( m_pStillPin, VideoControlFlag_Trigger );
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: triggering the still image failed."));
        goto cleanup;
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::CaptureStillImage()
{
    HRESULT hr = S_OK;

    // move the file name before everything, so the current file name is the capture in progress
    // until a new one starts.
    // see if the name exists, if it exists already, then move on to the next name.
    // a picture number less than -1 indicates that the picture number isn't used
    if(m_lPictureNumber >= -1)
    {
        HANDLE hFile = INVALID_HANDLE_VALUE;

        // search for the first file name that's not in use.
        do
        {
            TCHAR tszPictureName[MAX_PATH] = {NULL};
            int nStringLength = MAX_PATH;
            WIN32_FIND_DATA fd;

            m_lPictureNumber++;
            GetStillCaptureFileName(tszPictureName, &nStringLength);
            hFile = FindFirstFile(tszPictureName, &fd);

            if(INVALID_HANDLE_VALUE != hFile)
            {
                CloseHandle(hFile);
            }
        }while(hFile != INVALID_HANDLE_VALUE);
    }

    if(m_pVideoControl == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Core components needed for CaptureStillImage unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = m_EventSink.Purge();
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Failed to cleanout media events before still trigger."));
    }

    hr = TriggerStillImage();
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Failed to trigger the still image capture."));
        goto cleanup;
    }

    DShowEvent dse;

    dse.Code = EC_CAP_FILE_COMPLETED;
    dse.FilterFlags = EVT_EVCODE_EQUAL;

    hr = m_EventSink.WaitOnEvent(&dse, MEDIAEVENT_TIMEOUT);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Failed to retrieve the event indicating the capture completed."));
        OutputMissingEventsInformation(EC_CAP_FILE_COMPLETED);
        goto cleanup;
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::CaptureStillImageBurst(int nBurstCount)
{
    HRESULT hr = S_OK;
    int nMediaEventsCounted = 0;
    DShowEvent *dse;

    if(m_pVideoControl == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Core components needed for CaptureStillImage unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = m_EventSink.Purge();
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Failed to cleanout media events before still trigger."));
    }

    dse = new DShowEvent[nBurstCount];

    if(!dse)
    {
        FAIL(TEXT("CCaptureFramework: Failed to allocate the dshow event list."));
        hr = E_FAIL;
        goto cleanup;
    }

    for(int i = 0; i < nBurstCount; i++)
    {
        hr = TriggerStillImage();
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to trigger the still image capture."));
            goto cleanup;
        }

        dse[i].Code = EC_CAP_FILE_COMPLETED;
        dse[i].FilterFlags = EVT_EVCODE_EQUAL;
    }

    hr = m_EventSink.WaitOnEvents(nBurstCount, dse, TRUE, nBurstCount * MEDIAEVENT_TIMEOUT);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Failed to retrieve the event indicating the capture completed."));
        OutputMissingEventsInformation(EC_CAP_FILE_COMPLETED);
        goto cleanup;
    }

cleanup:

    if(dse)
        delete[] dse;

    return hr;
}

HRESULT
CCaptureFramework::StartStreamCapture()
{
    HRESULT hr = S_OK;
    REFERENCE_TIME rtStart = 0;
    REFERENCE_TIME rtStop = MAX_TIME;
    WORD wVideoStartCookie = 1, wVideoStopCookie = 2;
    WORD wAudioStartCookie = 3, wAudioStopCookie = 4;
    DShowEvent dse[2];

    if(m_pVideoCapture && m_pAudioCapture)
    {
        hr = m_EventSink.Purge();
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to cleanout media events."));
        }

        if (m_dwOptionsInUse & OPTION_SIMULT_CONTROL)
        {
            // since we're in simultanious control mode, we don't have separate audio and video cookies.
            wAudioStartCookie = wVideoStartCookie;
            wAudioStopCookie = wVideoStopCookie;
            hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, NULL, NULL, &rtStart, &rtStop, wVideoStartCookie, wVideoStopCookie );
            if(FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: Failed to start stream capture."));
                goto cleanup;
            }
        }
        else {
            hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, m_pAudioCapture, &rtStart, &rtStop, wAudioStartCookie, wAudioStopCookie );
            if(FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: Failed to start the audio capture."));
                goto cleanup;
            }

            hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pVideoCapture, &rtStart, &rtStop, wVideoStartCookie, wVideoStopCookie );
            if(FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: Failed to start the video capture."));
                goto cleanup;
            }
        }

        dse[0].Code = EC_STREAM_CONTROL_STARTED;
        dse[0].Param2 = wVideoStartCookie;
        dse[0].FilterFlags = EVT_EVCODE_EQUAL | EVT_PARAM2_EQUAL;
        dse[1].Code = EC_STREAM_CONTROL_STARTED;
        dse[1].Param2 = wAudioStartCookie;
        dse[1].FilterFlags = EVT_EVCODE_EQUAL | EVT_PARAM2_EQUAL;

        hr = m_EventSink.WaitOnEvents(2, dse, TRUE, MEDIAEVENT_TIMEOUT);
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to retrieve the control stream event."));
            OutputMissingEventsInformation(EC_STREAM_CONTROL_STARTED);
            goto cleanup;
        }
    }
    else if(m_pVideoCapture)
    {
        hr = m_EventSink.Purge();
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to cleanout media events."));
        }

        hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pVideoCapture, &rtStart, &rtStop, wVideoStartCookie, wVideoStopCookie );
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to start the video capture."));
            goto cleanup;
        }

        dse[0].Code = EC_STREAM_CONTROL_STARTED;
        dse[0].Param2 = wVideoStartCookie;
        dse[0].FilterFlags = EVT_EVCODE_EQUAL | EVT_PARAM2_EQUAL;

        hr = m_EventSink.WaitOnEvent(&(dse[0]), MEDIAEVENT_TIMEOUT);
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to retrieve the control stream event."));
            OutputMissingEventsInformation(EC_STREAM_CONTROL_STARTED);
            goto cleanup;
        }

    }
    else if(m_pAudioCapture)
    {

        hr = m_EventSink.Purge();
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to cleanout media events."));
        }

        hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, m_pAudioCapture, &rtStart, &rtStop, wAudioStartCookie, wAudioStopCookie );
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to start the audio capture."));
            goto cleanup;
        }

        dse[0].Code = EC_STREAM_CONTROL_STARTED;
        dse[0].Param2 = wAudioStartCookie;
        dse[0].FilterFlags = EVT_EVCODE_EQUAL | EVT_PARAM2_EQUAL;

        hr = m_EventSink.WaitOnEvent(&(dse[0]), MEDIAEVENT_TIMEOUT);
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to retrieve the control stream event."));
            OutputMissingEventsInformation(EC_STREAM_CONTROL_STARTED);
            goto cleanup;
        }
    }

cleanup:

    // if anything above failed, stop and restart the graph so we're in a good state.
    if(FAILED(hr))
    {
        // if anything failed trying to start the capture, then stop and restart the graph
        // so we're in a good state.
        StopGraph();
        RunGraph();
    }

    // if everything is good so far, then tell the app whether or not
    // frames have been dropped already.
    if(SUCCEEDED(hr))
    {
        m_Lock.Lock();
        if(m_bDiskWriteErrorOccured)
            hr = E_WRITE_ERROR_EVENT;
        else if(m_bBufferDeliveryFailed)
            hr = E_BUFFER_DELIVERY_FAILED;
        else if(m_lVideoFramesDropped > 0 || m_lAudioFramesDropped > 0)
            hr = E_OOM_EVENT;
        m_Lock.Unlock();
    }

    return hr;
}

HRESULT
CCaptureFramework::StopStreamCapture(DWORD dwAdditionalDelay)
{
    HRESULT hr = S_OK;
    REFERENCE_TIME rtStart = 0;
    REFERENCE_TIME rtStop = 0;
    WORD wVideoStartCookie = 5, wVideoStopCookie = 6;
    WORD wAudioStartCookie = 7, wAudioStopCookie = 8;
    CComPtr<IMediaSeeking> pMediaSeeking;
    DShowEvent dse[2];

    if(m_pCaptureGraphBuilder)
    {
        hr = m_pGraph.QueryInterface( &pMediaSeeking );
        if(FAILED(hr) || pMediaSeeking == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the MediaSeeking interface failed."));
            goto cleanup;
        }

        hr = pMediaSeeking->GetCurrentPosition(&rtStop);
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to retrieve the current position."));
            goto cleanup;
        }
    }

    if(m_pVideoCapture && m_pAudioCapture)
    {
        if (m_dwOptionsInUse & OPTION_SIMULT_CONTROL)
        {
            wAudioStartCookie= wVideoStartCookie;
            wAudioStopCookie = wVideoStopCookie;
            hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, NULL, NULL, &rtStart, &rtStop, wVideoStartCookie, wVideoStopCookie);
            if(FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: Failed to stop stream capture."));
                goto cleanup;
            }
        }
        else {
            hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pVideoCapture, &rtStart, &rtStop, wVideoStartCookie, wVideoStopCookie);
            if(FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: Failed to stop the video capture."));
                goto cleanup;
            }

            hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, m_pAudioCapture, &rtStart, &rtStop, wAudioStartCookie, wAudioStopCookie);
            if(FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: Failed to stop the audio capture."));
                goto cleanup;
            }
        }

        if(m_pVideoCapture && m_pVideoRenderer)
        {
            // hide the video window now that the stream control has stopped, to focus the cpu on encoding the video clip
            hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, m_pVideoCapture, NULL, 0, 0, 0 );
            if(FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: Failed to pause the preview stream."));
            }
        }

        dse[0].Code = EC_STREAM_CONTROL_STOPPED;
        dse[0].Param2 = wVideoStopCookie;
        dse[0].FilterFlags = EVT_EVCODE_EQUAL | EVT_PARAM2_EQUAL;
        dse[1].Code = EC_STREAM_CONTROL_STOPPED;
        dse[1].Param2 = wAudioStopCookie;
        dse[1].FilterFlags = EVT_EVCODE_EQUAL | EVT_PARAM2_EQUAL;

        hr = m_EventSink.WaitOnEvents(2, dse, TRUE, MAXIMUM_MEDIAEVENT_TIMEOUT + dwAdditionalDelay);
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: StopStreamCapture - failed to retrieve the EC_STREAM_CONTROL_STOPPED events."));
            OutputMissingEventsInformation(EC_STREAM_CONTROL_STOPPED);
            goto cleanup;
        }
    }
    else if(m_pVideoCapture)
    {
        hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pVideoCapture, &rtStart, &rtStop, wVideoStartCookie, wVideoStopCookie);
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to stop the video capture."));
            goto cleanup;
        }

        if(m_pVideoCapture && m_pVideoRenderer)
        {
            // hide the video window now that the stream control has stopped, to focus the cpu on encoding the video clip
            hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, m_pVideoCapture, NULL, 0, 0, 0 );
            if(FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: Failed to pause the preview stream."));
            }
        }

        dse[0].Code = EC_STREAM_CONTROL_STOPPED;
        dse[0].Param2 = wVideoStopCookie;
        dse[0].FilterFlags = EVT_EVCODE_EQUAL | EVT_PARAM2_EQUAL;

        hr = m_EventSink.WaitOnEvent(&(dse[0]), MAXIMUM_MEDIAEVENT_TIMEOUT + dwAdditionalDelay);
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to retrieve the control stream event."));
            OutputMissingEventsInformation(EC_STREAM_CONTROL_STOPPED);
            goto cleanup;
        }
    }
    else if(m_pAudioCapture)
    {
        hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, m_pAudioCapture, &rtStart, &rtStop, wAudioStartCookie, wAudioStopCookie);
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to stop the audio capture."));
            goto cleanup;
        }

        if(m_pVideoWindow)
        {
            hr = SetVideoWindowVisible(FALSE);
            if(FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: Failed to hide the video window."));
            }
        }

        dse[0].Code = EC_STREAM_CONTROL_STOPPED;
        dse[0].Param2 = wAudioStopCookie;
        dse[0].FilterFlags = EVT_EVCODE_EQUAL | EVT_PARAM2_EQUAL;

        hr = m_EventSink.WaitOnEvent(&(dse[0]), MAXIMUM_MEDIAEVENT_TIMEOUT + dwAdditionalDelay);
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to retrieve the control stream event."));
            OutputMissingEventsInformation(EC_STREAM_CONTROL_STOPPED);
            goto cleanup;
        }
    }

    m_Lock.Lock();
    Log(TEXT("%d frames processed, %d frames dropped"), m_lVideoFramesProcessed + m_lAudioFramesProcessed, m_lVideoFramesDropped + m_lAudioFramesDropped);
    m_Lock.Unlock();

    if(m_pVideoCapture && m_pVideoRenderer)
    {
        LONGLONG llEnd = MAXLONGLONG;
        hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, m_pVideoCapture, NULL, &llEnd, 0, 0 );
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to restart the paused preview stream."));
        }
    }

    if(SUCCEEDED(hr))
    {
        m_Lock.Lock();
        if(m_bDiskWriteErrorOccured)
            hr = E_WRITE_ERROR_EVENT;
        else if(m_bBufferDeliveryFailed)
            hr = E_BUFFER_DELIVERY_FAILED;
        else if(m_lVideoFramesDropped > 0 || m_lAudioFramesDropped > 0)
            hr = E_OOM_EVENT;
        m_Lock.Unlock();
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::TimedStreamCapture(int nCaptureLength)
{
    HRESULT hr = S_OK;
    REFERENCE_TIME rtStart = 0;
    // we're taking in the time based on microseconds, to 1000 nanoseconds is one microsecond,
    // and 10000*1000=10000000ns=1second
    REFERENCE_TIME rtStop = 0;
    WORD wVideoStartCookie = 1, wVideoStopCookie = 2;
    WORD wAudioStartCookie = 3, wAudioStopCookie = 4;
    CComPtr<IMediaSeeking> pMediaSeeking;

    DShowEvent dseStart[2];
    DShowEvent dseStop[2];

    if(m_pCaptureGraphBuilder)
    {
        hr = m_pGraph.QueryInterface( &pMediaSeeking );
        if(FAILED(hr) || pMediaSeeking == NULL)
        {
            FAIL(TEXT("CCaptureFramework: Retrieving the MediaSeeking interface failed."));
            goto cleanup;
        }

        hr = pMediaSeeking->GetCurrentPosition(&rtStart);
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to retrieve the current position."));
            goto cleanup;
        }

        rtStop = rtStart;

        // calculate our stop time based on the current time.
        rtStop += (nCaptureLength * 10000);

        // set the capture 1/10th of a second into the future, to guarantee that we never receive the stream
        // completion event for one stream before we receive the stream start event for the other stream.
        rtStart += (100 * 10000) ;
        rtStop += (100 * 10000) ;
    }

    hr = m_EventSink.Purge();
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Failed to cleanout media events."));
    }

    if(m_pVideoCapture && m_pAudioCapture)
    {
        if (m_dwOptionsInUse & OPTION_SIMULT_CONTROL)
        {
            wAudioStartCookie = wVideoStartCookie;
            wAudioStopCookie = wVideoStopCookie;
            hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, NULL, NULL, &rtStart, &rtStop, wVideoStartCookie, wVideoStopCookie );
            if(FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: Failed to start stream capture."));
                goto cleanup;
            }
        }
        else {
            hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, NULL, &rtStart, &rtStop, wVideoStartCookie, wVideoStopCookie );
            if(FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: Failed to start the video capture."));
                goto cleanup;
            }

            hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, NULL, &rtStart, &rtStop, wAudioStartCookie, wAudioStopCookie );
            if(FAILED(hr))
            {
                FAIL(TEXT("CCaptureFramework: Failed to start the audio capture."));
                goto cleanup;
            }
        }

        dseStart[0].Code = EC_STREAM_CONTROL_STARTED;
        dseStart[0].Param2 = wVideoStartCookie;
        dseStart[0].FilterFlags = EVT_EVCODE_EQUAL | EVT_PARAM2_EQUAL;
        dseStart[1].Code = EC_STREAM_CONTROL_STARTED;
        dseStart[1].Param2 = wAudioStartCookie;
        dseStart[1].FilterFlags = EVT_EVCODE_EQUAL | EVT_PARAM2_EQUAL;

        hr = m_EventSink.WaitOnEvents(2, dseStart, TRUE, MEDIAEVENT_TIMEOUT);
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: StopStreamCapture - failed to retrieve the EC_STREAM_CONTROL_STARTED events."));
            OutputMissingEventsInformation(EC_STREAM_CONTROL_STARTED);
            goto cleanup;
        }
    }
    else if(m_pVideoCapture)
    {
        hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pVideoCapture, &rtStart, &rtStop, wVideoStartCookie, wVideoStopCookie );
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to start the video capture."));
            goto cleanup;
        }

        dseStart[0].Code = EC_STREAM_CONTROL_STARTED;
        dseStart[0].Param2 = wVideoStartCookie;
        dseStart[0].FilterFlags = EVT_EVCODE_EQUAL | EVT_PARAM2_EQUAL;

        hr = m_EventSink.WaitOnEvent(&(dseStart[0]), MEDIAEVENT_TIMEOUT);
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to retrieve the control stream event."));
            OutputMissingEventsInformation(EC_STREAM_CONTROL_STARTED);
            goto cleanup;
        }
    }
    else if(m_pAudioCapture)
    {
        hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, m_pAudioCapture, &rtStart, &rtStop, wAudioStartCookie, wAudioStopCookie );
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to start the audio capture."));
            goto cleanup;
        }

        dseStart[0].Code = EC_STREAM_CONTROL_STARTED;
        dseStart[0].Param2 = wAudioStartCookie;
        dseStart[0].FilterFlags = EVT_EVCODE_EQUAL | EVT_PARAM2_EQUAL;

        hr = m_EventSink.WaitOnEvent(&(dseStart[0]), MEDIAEVENT_TIMEOUT);
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to retrieve the control stream event."));
            OutputMissingEventsInformation(EC_STREAM_CONTROL_STARTED);
            goto cleanup;
        }
    }

    // if we could potentially be losing CPU to the video renderer in this test, delay for roughly the length
    // of the capture and then pause the preview stream.
    // Then, once paused we do the final wait for the stream control stopped events with the stream paused, and then restart
    // it when we're done. This prevents waiting forever because high cpu utilization for the preview blocks encoding.
    if(m_pVideoCapture && m_pVideoRenderer)
    {
        Sleep(nCaptureLength);

        // hide the video window now that the stream control has stopped, to focus the cpu on encoding the video clip
        hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, m_pVideoCapture, NULL, 0, 0, 0 );
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to pause the preview stream."));
            goto cleanup;
        }
    }

    if(m_pVideoCapture && m_pAudioCapture)
    {
        dseStop[0].Code = EC_STREAM_CONTROL_STOPPED;
        dseStop[0].Param2 = wVideoStopCookie;
        dseStop[0].FilterFlags = EVT_EVCODE_EQUAL | EVT_PARAM2_EQUAL;
        dseStop[1].Code = EC_STREAM_CONTROL_STOPPED;
        dseStop[1].Param2 = wAudioStopCookie;
        dseStop[1].FilterFlags = EVT_EVCODE_EQUAL | EVT_PARAM2_EQUAL;


        hr = m_EventSink.WaitOnEvents(2, dseStop, TRUE, MAXIMUM_MEDIAEVENT_TIMEOUT + nCaptureLength);
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: StopStreamCapture - failed to retrieve the EC_STREAM_CONTROL_STOPPED events."));
            OutputMissingEventsInformation(EC_STREAM_CONTROL_STOPPED);
            goto cleanup;
        }
    }
    else if(m_pVideoCapture)
    {
        dseStop[0].Code = EC_STREAM_CONTROL_STOPPED;
        dseStop[0].Param2 = wVideoStopCookie;
        dseStop[0].FilterFlags = EVT_EVCODE_EQUAL | EVT_PARAM2_EQUAL;

        hr = m_EventSink.WaitOnEvent(&(dseStop[0]), MAXIMUM_MEDIAEVENT_TIMEOUT + nCaptureLength);
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to retrieve the control stream event."));
            OutputMissingEventsInformation(EC_STREAM_CONTROL_STOPPED);
            goto cleanup;
        }
    }
    else if(m_pAudioCapture)
    {
        dseStop[0].Code = EC_STREAM_CONTROL_STOPPED;
        dseStop[0].Param2 = wAudioStopCookie;
        dseStop[0].FilterFlags = EVT_EVCODE_EQUAL | EVT_PARAM2_EQUAL;

        hr = m_EventSink.WaitOnEvent(&(dseStop[0]), MAXIMUM_MEDIAEVENT_TIMEOUT + nCaptureLength);
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to retrieve the control stream event."));
            OutputMissingEventsInformation(EC_STREAM_CONTROL_STOPPED);
            goto cleanup;
        }
    }

    m_Lock.Lock();
    Log(TEXT("%d frames processed, %d frames dropped"), m_lVideoFramesProcessed + m_lAudioFramesProcessed, m_lVideoFramesDropped + m_lAudioFramesDropped);
    m_Lock.Unlock();

    // now restart the preview so everyone's happy.
    if(m_pVideoCapture && m_pVideoRenderer)
    {
        LONGLONG llEnd = MAXLONGLONG;
        hr = m_pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, m_pVideoCapture, NULL, &llEnd, 0, 0 );
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Failed to restart the paused preview stream."));
            goto cleanup;
        }
    }


cleanup:

    // if anything above failed, then stop the graph and restart it so we're in a good state.
    if(FAILED(hr))
    {
        StopGraph();
        RunGraph();
    }
    else if(SUCCEEDED(hr))
    {
        m_Lock.Lock();
        if(m_bDiskWriteErrorOccured)
            hr = E_WRITE_ERROR_EVENT;
        else if(m_bBufferDeliveryFailed)
            hr = E_BUFFER_DELIVERY_FAILED;
        else if(m_lVideoFramesDropped > 0 || m_lAudioFramesDropped > 0)
            hr = E_OOM_EVENT;
        m_Lock.Unlock();
    }
    return hr;
}

HRESULT
CCaptureFramework::VerifyVideoFileCaptured(TCHAR *tszVideoFileName)
{
    HRESULT hr = S_FALSE;
    TCHAR *tszFileToVerify = NULL;
    TCHAR tszFileName[MAX_PATH];
    int nStringLength = MAX_PATH;

    // if we have no function for doing the verification, then
    // fail gracefully (appropriate errors were outputted earlier)
    if(m_pfnVerifyVideoFile)
    {
        // if no file name was given, then try to retrieve it.
        // if retrieving it fails, then we can't do verification.
        if(!tszVideoFileName)
        {
            if(FAILED(hr = GetVideoCaptureFileName(tszFileName, &nStringLength)))
            {
                FAIL(TEXT("Failed to retrieve the video capture file name."));
            }
            else tszFileToVerify = tszFileName;
        }
        else tszFileToVerify = tszVideoFileName;

        // if the video capture verification function is set
        if(tszFileToVerify)
        {
            if(FAILED(hr = m_pfnVerifyVideoFile(tszFileToVerify)))
                FAIL(TEXT("Verifying the video file failed."));
        }
        else FAIL(TEXT("Unable to verify, no file name available"));
    }

    return hr;
}

HRESULT
CCaptureFramework::VerifyPreviewWindow()
{
    HRESULT hr = S_OK;
    int nTickCountBefore = 0, nTickCountAfter = 0;;
    HDC hdc = NULL, hdcCompat = NULL;
    HBITMAP hbmp = NULL, hbmpStock = NULL;
    int nWidth = m_rcLocation.right - m_rcLocation.left;
    int nHeight = m_rcLocation.bottom - m_rcLocation.top;

    // Create a 24-bit bitmap
    BITMAPINFO bmi;
    LPBYTE lpbBits = NULL;
    DWORD dwOrientation;

    // if you're requesting auto verification, but not using the null driver, then force off verification.
    if(!m_bUsingNULLDriver && m_nPreviewVerificationMode == VERIFICATION_FORCEAUTO)
    {
        m_nPreviewVerificationMode = VERIFICATION_OFF;
        Log(TEXT("Forcing automatic verification off because the NULL camera driver is not being used."));
    }

    // if verification is off, then don't do anything.
    if(m_nPreviewVerificationMode == VERIFICATION_OFF)
    {
        goto cleanup;
    }

    // if we're forcing manual, then do manual.
    if(m_nPreviewVerificationMode == VERIFICATION_FORCEMANUAL)
    {
        int nMessageBoxResult;

        nMessageBoxResult = MessageBox(m_hwndOwner, TEXT("Is the preview image on the screen correct?"), 
                                                                TEXT("UserVerify"), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1 | MB_SETFOREGROUND);
        if(IDNO == nMessageBoxResult)
        {
            FAIL(TEXT("CCaptureFramework: User specified the preview was incorrect."));
            hr = E_FAIL;
        }
        goto cleanup;
    }

    // verify what we need to do automatic verification is available.
    if(m_pVideoRendererBasicVideo == NULL || m_pVideoWindow == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Core components required for VerifyPreviewWindow unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = m_lDefaultWidth;
    // negative height so we have a top down bitmap
    bmi.bmiHeader.biHeight      = -m_lDefaultHeight;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    // retrieve the current screen orientation just before the preview is run.
    hr = GetScreenOrientation(&dwOrientation);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Failed to retrieve the system orientation before capture."));
        goto cleanup;
    }

    nTickCountBefore = GetTickCount();
    // delay long enough so that a new image is rendered, before pausing the graph to
    // capture the screen for verification.
    Sleep(200);
    hr = PauseGraph();
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Unable to pause the basic graph."));
        goto RestartGraph;
    }
    nTickCountAfter = GetTickCount();

    hdc = GetDC(m_hwndOwner);
    if(NULL == hdc)
    {
        FAIL(TEXT("CCaptureFramework: Unable to retrieve a dc for the window."));
        hr = E_FAIL;
        goto RestartGraph;
    }

    hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (VOID**)&lpbBits, NULL, 0);
    if(NULL == hbmp)
    {
        FAIL(TEXT("CCaptureFramework: Unable to create a compatible bitmap."));
        hr = E_FAIL;
        goto RestartGraph;
    }

    hdcCompat = CreateCompatibleDC(hdc);
    if(NULL == hdcCompat)
    {
        FAIL(TEXT("CCaptureFramework: Unable to create a compatible dc."));
        hr = E_FAIL;
        goto RestartGraph;
    }

    hbmpStock = (HBITMAP) SelectObject(hdcCompat, hbmp);
    if(NULL == hbmpStock)
    {
        FAIL(TEXT("CCaptureFramework: Unable to create a compatible dc."));
        hr = E_FAIL;
        goto RestartGraph;
    }

    if(!StretchBlt(hdcCompat, 0, 0, m_lDefaultWidth, m_lDefaultHeight, hdc, m_rcLocation.left, m_rcLocation.top, nWidth, nHeight, SRCCOPY))
    {
        FAIL(TEXT("CCaptureFramework: Unable to blit from the primary to our test DC."));
        hr = E_FAIL;
        goto RestartGraph;
    }

    hr = VerifyDriverImage(hbmp, nTickCountBefore, nTickCountAfter, dwOrientation);

RestartGraph:
    HRESULT hr2 = RunGraph();
    if(FAILED(hr2))
    {
        FAIL(TEXT("CCaptureFramework: Unable to restart the graph."));
        hr = E_FAIL;
        goto cleanup;
    }

cleanup:

    if(hbmpStock)
        SelectObject(hdcCompat, hbmpStock);

    if(hdcCompat)
        DeleteDC(hdcCompat);

    if(hbmp)
        DeleteObject(hbmp);

    if(hdc)
        ReleaseDC(m_hwndOwner, hdc);

    return hr;
}

HRESULT
CCaptureFramework::VerifyStillImageCapture()
{
    HRESULT hr = S_OK;
    int nTickCountBefore = 0, nTickCountAfter = 0;;
    TCHAR tszImageName[MAX_PATH];
    int nStringLength = MAX_PATH;

    // capture a still image
    nTickCountBefore = GetTickCount();
    hr = CaptureStillImage();
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Failed to capture the still image."));
        goto cleanup;
    }
    nTickCountAfter = GetTickCount();

    hr = GetStillCaptureFileName(tszImageName, &nStringLength);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Unable to retrieve the still image file name."));
        goto cleanup;
    }

    hr = VerifyStillImageLocation(tszImageName, nTickCountBefore, nTickCountAfter);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Verifying the still image failed."));
        goto cleanup;
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::VerifyStillImageLocation(TCHAR *tszImageName, int nTickCountBefore, int nTickCountAfter)
{
    HRESULT hr = S_OK;
    CComPtr<IImagingFactory> pImagingFactory = NULL;
    CComPtr<IImage> pJPEGImage = NULL;
    RECT rc;
    ImageInfo ImageInfo;
    HDC hdcCompat = NULL;
    HDC hdcPrimary = NULL;
    HBITMAP hbmp = NULL, hbmpStock = NULL;
    BITMAPINFO bmi;
    LPBYTE lpbBits = NULL;
    DWORD dwOrientation;

    // retrieve the current screen orientation
    hr = GetScreenOrientation(&dwOrientation);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Failed to retrieve the system orientation before capture."));
        goto cleanup;
    }

    // if you're requesting auto verification, but not using the null driver, then force off verification.
    if(!m_bUsingNULLDriver && m_nStillVerificationMode == VERIFICATION_FORCEAUTO)
    {
        m_nStillVerificationMode = VERIFICATION_OFF;
        Log(TEXT("Forcing automatic verification off because the NULL camera driver is not being used."));
    }

    // if verification is turned off then cleanup
    // because there's nothing else we can do.
    if(m_nStillVerificationMode == VERIFICATION_OFF)
    {
        goto cleanup;
    }

    // retrieve the still image info
    hr = CoCreateInstance( CLSID_ImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IImagingFactory, (void**) &pImagingFactory );
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Unable to retrieve the Imaging factory."));
        goto cleanup;
    }

    hr = pImagingFactory->CreateImageFromFile(tszImageName, &pJPEGImage);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Unable to load the image file."));
        goto cleanup;
    }

    hr = pJPEGImage->GetImageInfo(&ImageInfo);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Unable to retrieve the image information."));
        goto cleanup;
    }

    if(m_bUsingNULLDriver && m_nStillVerificationMode != VERIFICATION_FORCEMANUAL)
    {

        hdcCompat = CreateCompatibleDC(NULL);
        if(NULL == hdcCompat)
        {
            FAIL(TEXT("CCaptureFramework: Unable to create a compatible dc."));
            hr = E_FAIL;
            goto cleanup;
        }

        memset(&bmi, 0, sizeof(bmi));
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = ImageInfo.Width;
        // negative height so we have a top down bitmap
        bmi.bmiHeader.biHeight      = -((int) ImageInfo.Height);
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 24;
        bmi.bmiHeader.biCompression = BI_RGB;

        hbmp = CreateDIBSection(hdcCompat, &bmi, DIB_RGB_COLORS, (VOID**)&lpbBits, NULL, 0);
        if(NULL == hbmp)
        {
            FAIL(TEXT("CCaptureFramework: Unable to create a compatible bitmap."));
            hr = E_FAIL;
            goto cleanup;
        }

        hbmpStock = (HBITMAP) SelectObject(hdcCompat, hbmp);
        if(NULL == hbmpStock)
        {
            FAIL(TEXT("CCaptureFramework: Unable to create a compatible dc."));
            hr = E_FAIL;
            goto cleanup;
        }

        rc.left = 0;
        rc.top = 0;
        rc.right = ImageInfo.Width;
        rc.bottom = ImageInfo.Height;

        hr = pJPEGImage->Draw(hdcCompat, &rc, NULL);
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Unable to copy the JPEG onto the HDC."));
            goto cleanup;
        }

        hr = VerifyDriverImage(hbmp, nTickCountBefore, nTickCountAfter, dwOrientation);
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: image verification failed."));
            goto cleanup;
        }
    }
    else
    {
        // pause the graph
        hr = PauseGraph();
        if(FAILED(hr))
        {
            FAIL(TEXT("CCaptureFramework: Unable to pause the graph for manual verification."));
            goto cleanup;
        }

        hdcPrimary = GetDC(NULL);
        if(NULL == hdcPrimary)
        {
            FAIL(TEXT("CCaptureFramework: Unable to retrieve a dc for the primary for displaying the image captured."));
            hr = E_FAIL;
            goto cleanup;
        }

        RECT rcPrimary;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rcPrimary, 0);

        // clear out the surface for the initialization
        PatBlt(hdcPrimary, rcPrimary.left, rcPrimary.top, rcPrimary.right - rcPrimary.left, rcPrimary.bottom - rcPrimary.top, WHITENESS);

        // copy up the captured image to the primary, and then ask the user if it's right
        hr = pJPEGImage->Draw(hdcPrimary, &rcPrimary, NULL);

        // ask the user
        int nMessageBoxResult;

        nMessageBoxResult = MessageBox(NULL, TEXT("Is the image on the screen the image expected to be captured?"), 
                                                                TEXT("UserVerify"), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1 );
        if(IDNO == nMessageBoxResult)
        {
            FAIL(TEXT("CCaptureFramework: User specified the still image was incorrect."));
            hr = E_FAIL;
        }

        HRESULT hr2 = RunGraph();
        if(FAILED(hr2))
        {
            FAIL(TEXT("CCaptureFramework: Unable to restart the graph."));
            hr = E_FAIL;
            goto cleanup;
        }
    }

cleanup:

    if(hdcPrimary)
        ReleaseDC(m_hwndOwner, hdcPrimary);

    if(hbmpStock)
        SelectObject(hdcCompat, hbmpStock);

    if(hdcCompat)
        DeleteDC(hdcCompat);

    if(hbmp)
        DeleteObject(hbmp);

    return hr;
}

HRESULT
CCaptureFramework::GetVideoRenderMode(DWORD *dwMode)
{
    HRESULT hr = S_OK;

    if(m_pVideoRendererMode == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Core components required for GetVideoRenderMode unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    if(NULL == dwMode)
    {
        FAIL(TEXT("CCaptureFramework: Invalid parameter."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = m_pVideoRendererMode->GetMode(dwMode);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Unable to retrieve the video renderer mode."));
        goto cleanup;
    }

cleanup:
    return hr;
}

HRESULT CCaptureFramework::SetVideoRenderMode(DWORD dwMode)
{
    HRESULT hr = S_OK;

    if (m_pVideoRendererMode == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Core components required for SetVideoRenderMode unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = m_pVideoRendererMode->SetMode(dwMode);
    if(FAILED(hr))
    {
        DETAIL(TEXT("CCaptureFramework: Unable to set the video renderer mode."));
        goto cleanup;
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::GetVideoRenderStats(VIDEORENDERER_STATS *statData)
{
    HRESULT hr = S_OK;
    REFTIME AvgTimeperFrame = 0;
    CComPtr<IQualProp> pQP = NULL;
    int iTemp;

    if(m_pVideoRendererBasicVideo == NULL)
    {
        FAIL(TEXT("GetVideoRenderStats:  VideoRendererBasicVideo is NULL"));
        hr = E_FAIL;
        goto cleanup;
    }

    if (statData)
    {
        statData->dFrameRate     = 0.0;
        statData->dActualRate    = 0.0;
        statData->lFramesDropped = 0;
        statData->lSourceWidth = 0;
        statData->lSourceHeight = 0;
        statData->lDestWidth = 0;
        statData->lDestHeight = 0;
    }
   
    hr = m_pVideoRendererBasicVideo->get_AvgTimePerFrame(&AvgTimeperFrame);
    if(FAILED(hr))
    {
        FAIL(TEXT("GetVideoRenderStats: get_AvgTimePerFrame failed."));
    }

    hr = m_pVideoRendererBasicVideo->get_SourceWidth(&statData->lSourceWidth);
    if(FAILED(hr))
    {
        FAIL(TEXT("GetVideoRenderStats: get_SourceWidth failed."));
    }

    hr = m_pVideoRendererBasicVideo->get_SourceHeight(&statData->lSourceHeight);
    if(FAILED(hr))
    {
        FAIL(TEXT("GetVideoRenderStats: get_SourceHeight failed."));
    }

    hr = m_pVideoRendererBasicVideo->get_DestinationWidth(&statData->lDestWidth);
    if(FAILED(hr))
    {
        FAIL(TEXT("GetVideoRenderStats: get_DestinationWidth failed."));
    }

    hr = m_pVideoRendererBasicVideo->get_DestinationHeight(&statData->lDestHeight);
    if(FAILED(hr))
    {
        FAIL(TEXT("GetVideoRenderStats: get_DestinationHeight failed."));
    }

    if (AvgTimeperFrame)
    {
        if (statData)
            statData->dFrameRate = (1.0/AvgTimeperFrame);
    }

    // no GUID associated in atlbase.h
    hr = m_pVideoRendererBasicVideo->QueryInterface(IID_IQualProp, (void**) &pQP);
    if(FAILED(hr))
    {
        FAIL(TEXT("GetVideoRenderStats: Querying for the IQualProp control failed."));
        goto cleanup;
    }
                
    pQP->get_FramesDroppedInRenderer(&iTemp);

    if (statData)    
        statData->lFramesDropped = iTemp;
          
    pQP->get_AvgFrameRate(&iTemp);
    if (statData)    
        statData->dActualRate = iTemp / 100.0;
    
cleanup:
    return hr;
}

HRESULT
CCaptureFramework::GetVideoEncoderInfo(LPCOLESTR tszEntryName, VARIANT *varg)
{
    HRESULT hr = S_OK;
    CComPtr<IPropertyBag> pPropertyBag = NULL;

    if(m_pVideoEncoderDMO == NULL)
    {
        hr = S_FALSE;
        goto cleanup;
    }

    hr = m_pVideoEncoderDMO.QueryInterface( &pPropertyBag);
    if(FAILED(hr))
    {
        DETAIL(TEXT("GetVideoEncoderInfo:  QueryInterface for IID_IPropertyBag failed"));
        goto cleanup;
    }

    hr = pPropertyBag->Read( tszEntryName, varg, NULL );

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::GetAudioEncoderInfo(LPCOLESTR tszEntryName, VARIANT *varg)
{
    HRESULT hr = S_OK;
    CComPtr<IPropertyBag> pPropertyBag = NULL;

    if(m_pAudioEncoderDMO == NULL)
    {
        hr = S_FALSE;
        goto cleanup;
    }

    hr = m_pAudioEncoderDMO.QueryInterface( &pPropertyBag);
    if(FAILED(hr))
    {
        DETAIL(TEXT("GetAudioEncoderInfo:  QueryInterface for IID_IPropertyBag failed"));
        goto cleanup;
    }
                
    hr = pPropertyBag->Read( tszEntryName, varg, NULL );

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::SetVideoEncoderProperty(LPCOLESTR ptzPropName,  VARIANT *pVar)
{
    HRESULT hr = S_OK;
    CComPtr<IPropertyBag> pPropertyBag = NULL;

    if(!(m_dwComponentsInUse & VIDEO_ENCODER))
        return S_FALSE;
    
    if(m_pVideoEncoderDMO == NULL)
    {
        FAIL(TEXT("SetVideoEncoderProperty:  m_pVideoEncoderDMO is NULL"));
        hr = E_FAIL;
        goto cleanup;
    }

    if(pVar == NULL)
    {
        FAIL(TEXT("SetVideoEncoderProperty:  pointer to Variant is NULL"));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = m_pVideoEncoderDMO.QueryInterface( &pPropertyBag);
    if(FAILED(hr))
    {
        FAIL(TEXT("SetVideoEncoderProperty:  QueryInterface for  IID_IPropertyBag failed"));
        hr = E_FAIL;
        goto cleanup;
    }
           
    hr = pPropertyBag->Write( ptzPropName, pVar);
    if(FAILED(hr))
    {
        FAIL(TEXT("SetVideoEncoderProperty:  Setting encoder property  failed"));
        hr = E_FAIL;
        goto cleanup;
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::SetAudioEncoderProperty(LPCOLESTR ptzPropName,  VARIANT *pVar)
{
    HRESULT hr = S_OK;
    CComPtr<IPropertyBag> pPropertyBag = NULL;

    // If there's no audio encoder, then there's nothing to set.
    if(m_pAudioEncoderDMO == NULL)
    {
        hr = S_FALSE;
        goto cleanup;
    }

    if(pVar == NULL)
    {
        FAIL(TEXT("SetAudioEncoderProperty:  pointer to Variant is NULL"));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = m_pAudioEncoderDMO.QueryInterface( &pPropertyBag);
    if(FAILED(hr))
    {
        FAIL(TEXT("SetAudioEncoderProperty:  QueryInterface for  IID_IPropertyBag failed"));
        hr = E_FAIL;
        goto cleanup;
    }
           
    hr = pPropertyBag->Write( ptzPropName, pVar);
    if(FAILED(hr))
    {
        FAIL(TEXT("SetAudioEncoderProperty:  Setting encoder property  failed"));
        hr = E_FAIL;
        goto cleanup;
    }

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::SetStillImageQuality(int n)
{
    HRESULT hr = S_OK;

    if(m_pImageSinkFilter == NULL)
    {
        FAIL(TEXT("SetStillImageQuality: m_pImageSinkFilter is NULL."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = m_pImageSinkFilter->SetQuality(n);

cleanup:
    return hr;
}

HRESULT
CCaptureFramework::SwitchVideoRenderMode()
{
    HRESULT hr = S_OK;

    LONG BufferSize = 0;
    LONG *pDIBImage = NULL;
    DWORD dwModeSet = AM_VIDEO_RENDERER_MODE_GDI, dwModeReturned;

    hr = GetVideoRenderMode(&dwModeReturned);

    if(FAILED(hr))
        goto cleanup;
    else if(dwModeReturned == AM_VIDEO_RENDERER_MODE_DDRAW)
    {
        DETAIL(TEXT("CCaptureFramework: Using ddraw, switching to GDI."));
        dwModeSet = AM_VIDEO_RENDERER_MODE_GDI;
    }
    else if(dwModeReturned == AM_VIDEO_RENDERER_MODE_GDI)
    {
        DETAIL(TEXT("CCaptureFramework: Using gdi, switching to DDraw."));
        dwModeSet = AM_VIDEO_RENDERER_MODE_DDRAW;
    }

    hr = SetVideoRenderMode(dwModeSet);
    if(FAILED(hr))
        goto cleanup;

    hr =GetVideoRenderMode(&dwModeReturned);
    if(FAILED(hr))
        goto cleanup;

    if(dwModeSet != dwModeReturned)
    {
        FAIL(TEXT("CCaptureFramework: The mode set doesn't match the mode returned after setting."));
        hr = E_FAIL;
        goto cleanup;
    }

cleanup:
    return hr;
}

HRESULT SaveBitmap(const TCHAR * tszFileName, BYTE *pPixels, INT iWidth, INT iHeight, INT iWidthBytes)
{
    DWORD dwBmpSize;
    BITMAPFILEHEADER bmfh;
    DWORD dwBytesWritten;
    BITMAPINFO bmi24;
    HANDLE hBmpFile = NULL;
    HRESULT hr;
    TCHAR tszBitmapCopy[MAX_PATH + 1];
    TCHAR * pBack;

    //
    // Argument validation
    //
    if (NULL == tszFileName)
    {
        return E_POINTER;
    }

    if (NULL == pPixels)
    {
        return E_POINTER;
    }

    if (0 >= iWidth)
    {
        return E_INVALIDARG;
    }

    if (0 == iHeight) 
    {
        return E_INVALIDARG;
    }

    if (0 >= iWidthBytes) 
    {
        return E_INVALIDARG;
    }

    // if iHeight is negative, then it's a top down dib.  Save it as it is.
    
    memset(&bmi24, 0x00, sizeof(bmi24));
    
    bmi24.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    bmi24.bmiHeader.biWidth = iWidth;
    // negative height to specify top down instead of bottom up
    bmi24.bmiHeader.biHeight = -iHeight;
    bmi24.bmiHeader.biPlanes = 1;
    bmi24.bmiHeader.biBitCount = 24;
    bmi24.bmiHeader.biCompression = BI_RGB;
    bmi24.bmiHeader.biSizeImage = bmi24.bmiHeader.biXPelsPerMeter = bmi24.bmiHeader.biYPelsPerMeter = 0;
    bmi24.bmiHeader.biClrUsed = bmi24.bmiHeader.biClrImportant = 0;
    
    memset(&bmfh, 0x00, sizeof(BITMAPFILEHEADER));
    bmfh.bfType = 'M' << 8 | 'B';
    bmfh.bfOffBits = sizeof(bmfh) + sizeof(bmi24);

    //created the directory to save the file if it doesn't exist
    wcsncpy(tszBitmapCopy, tszFileName, countof(tszBitmapCopy));
    tszBitmapCopy[countof(tszBitmapCopy) - 1] = 0;
    pBack = wcsrchr(tszBitmapCopy, TEXT('\\'));

    hBmpFile = CreateFile(
        tszFileName, 
        GENERIC_WRITE, 
        0, 
        NULL, 
        CREATE_ALWAYS, 
        FILE_ATTRIBUTE_NORMAL, 
        NULL);

    if (INVALID_HANDLE_VALUE == hBmpFile)
        goto LPError;
    
    bmi24.bmiHeader.biSizeImage = dwBmpSize = iWidthBytes * abs(iHeight);
    bmfh.bfSize = bmfh.bfOffBits + dwBmpSize;
    
    if (!WriteFile(
            hBmpFile,
            (void*)&bmfh,
            sizeof(bmfh),
            &dwBytesWritten,
            NULL))
    {
        goto LPError;
    }
    if (!WriteFile(
            hBmpFile,
            (void*)&bmi24,
            sizeof(bmi24),
            &dwBytesWritten,
            NULL))
    {
        goto LPError;
    }
    if (!WriteFile(
            hBmpFile,
            (void*)pPixels,
            dwBmpSize,
            &dwBytesWritten,
            NULL))
    {
        goto LPError;
    }
    
    CloseHandle(hBmpFile);
    return S_OK;
LPError:
    hr = HRESULT_FROM_WIN32(GetLastError());
    
    if (hBmpFile)
        CloseHandle(hBmpFile);
    return hr;
}

#define SWITCHINT(a, b) { int nTemporaryVariable = a; a=b; b = nTemporaryVariable; }

HRESULT
CCaptureFramework::VerifyDriverImage(HBITMAP hbmp, int nStartTime, int nStopTime, DWORD dwOrientation)
{
    HRESULT hr = S_OK;
    DIBSECTION ds;
    int x, y, xBox, yBox;
    int nWidth;
    int nHeight;
    // expected box size as defined in the buffer fill function
    int nBoxWidth;
    int nBoxHeight;
    DWORD dwPixelValue = 0;
    int nStep;
    int nStride;
    BYTE * pbBitmapData = NULL;

    if(NULL == hbmp)
    {
        FAIL(TEXT("CCaptureFramework: Invalid HBITMAP passed."));
        hr = E_FAIL;
        goto cleanup;
    }

    if(nStopTime < nStartTime)
    {
        FAIL(TEXT("CCaptureFramework: Stop time occured before the start time."));
        hr = E_FAIL;
        goto cleanup;
    }

    if(!GetObject(hbmp, sizeof (DIBSECTION), &ds))
    {
        FAIL(TEXT("CCaptureFramework: Failed to retrieve the DIBSection data."));
        hr = E_FAIL;
        goto cleanup;
    }

    nWidth = ds.dsBm.bmWidth;
    nHeight = ds.dsBm.bmHeight;
    // expected box size as defined in the buffer fill function
    nBoxWidth = nWidth/BOXWIDTHDIVIDER;
    nBoxHeight = nHeight/BOXHEIGHTDIVIDER;
    nStep = (ds.dsBm.bmBitsPixel / 8);
    nStride = ds.dsBm.bmWidthBytes;
    pbBitmapData = (BYTE *) ds.dsBm.bmBits;

    if(pbBitmapData == NULL)
    {
        FAIL(TEXT("CCaptureFramework: HBitmap passed not a DIBSection."));
        hr = E_FAIL;
        goto cleanup;
    }

    if(nStep <= 2)
    {
        FAIL(TEXT("CCaptureFramework: Function cannot handle <=16bpp surfaces."));
        hr = E_FAIL;
        goto cleanup;
    }

    if(nWidth <= 0 || nHeight <= 0)
    {
        FAIL(TEXT("CCaptureFramework: Invalid width/height given for verification."));
        hr = E_FAIL;
        goto cleanup;
    }

    // pre-set the pixel value, as it's the key for exiting the loop.
    dwPixelValue = 1;

    // find the upper left point of black on the surface, 0xe0e0e0 to compensate for the lossyness of jpeg.
    // low quality can be up to 0x2f off per color channel for the pixels expected to be black.
    for(y = 0; y < nHeight && dwPixelValue != 0x0; y++)
        for(x = 0; x < nWidth && dwPixelValue != 0x0; x++)
            dwPixelValue = (*((DWORD UNALIGNED *) (pbBitmapData + (y*nStride) + x*nStep))) & 0x00C0C0C0;

    if(x == nWidth || y == nHeight)
    {
        FAIL(TEXT("CCaptureFramework: No validation image found on the surface."));
        hr = E_FAIL;
        goto cleanup;
    }

    // decrement by one to compensate for the final increments of the loops above.
    x--;
    y--;

    // allow a variance to compensate for stretching.
    // because we're dealing with all four sides, allow 2x the variance per side
    for(yBox = y; yBox < y + (nBoxHeight - (LOCATIONVARIANCE * 2)); yBox++)
        for(xBox = x; xBox < x + (nBoxWidth - (LOCATIONVARIANCE * 2)); xBox++)
        {
            // 0xe0e0e0 to compensate for the lossyness of jpeg
            dwPixelValue = (*((DWORD UNALIGNED *) (pbBitmapData + (yBox*nStride) + xBox*nStep))) & 0x00C0C0C0;
            if(dwPixelValue != 0)
            {
                FAIL(TEXT("CCaptureFramework: Invalid pixel found within the sample image."));
                Log(TEXT("CCaptureFramework: coordinate at %d, %d, received 0x%08x, expect 0."), xBox, yBox, dwPixelValue);
                hr = E_FAIL;
                goto cleanup;
            }
        }

    // now we know that we have the black box, and it's from x,y(inclusive) to xBox,yBox (exclusive)
    // is it in the right place?

    // First, compenstate for the orientation of the shell as applied to the image captured,
    // the compensation is done on the detected variables, not the calculated variables.
    switch(dwOrientation)
    {
        // case 0 orientation, do nothing.
        case DMDO_0:
            break;
        case DMDO_90:
        {
            // switch x and y for everything.
            SWITCHINT(x, y);
            SWITCHINT(nBoxWidth, nBoxHeight);
            SWITCHINT(nWidth, nHeight);

            // as x and y are reversed from the rotate, the y variable
            // is compenstated for by the height.
            y = nHeight - y;

            // compensate for the box's upper/left corner change,
            // we're at 90 degrees, so we actually have the lower left corner.
            y-= nBoxHeight;
        }
            break;
        case DMDO_180:
        {

            // 180 degrees, so x and y are reversed
            y = nHeight - y;
            x = nWidth - x;

            // and we have the lower right corner, so move to the upper left
            x-=nBoxWidth;
            y-=nBoxHeight;

        }
            break;
        case DMDO_270:
        {
            // switch x and y for everything.
            SWITCHINT(x, y);
            SWITCHINT(nBoxWidth, nBoxHeight);
            SWITCHINT(nWidth, nHeight);

            // as x and y are reversed from the rotate, the x variable
            // is compenstated for by the width.
            x = nWidth - x;

            // we have the upper right corner, to move to the upper left.
            x-=nBoxWidth;
        }
            break;
        default:
        {
            FAIL(TEXT("Unknown orientation"));
        }
            break;
    }

    nStartTime = nStartTime >> SPEEDSHIFT;
    nStopTime = nStopTime >> SPEEDSHIFT;

    float fWidthStep = (float) (nWidth - nBoxWidth)/LOCATIONWIDTHMASK;
    float fHeightStep = (float) (nHeight - nBoxHeight)/LOCATIONHEIGHTMASK;

    int nDirectionStart = (nStartTime >> LOCATIONSHIFT) & 0x3;
    int nLocationXStart = (int) (fWidthStep * (nStartTime & LOCATIONWIDTHMASK));
    int nLocationYStart = (int) (fHeightStep * (nStartTime & LOCATIONHEIGHTMASK));

    int nDirectionEnd = (nStopTime >> LOCATIONSHIFT) & 0x3;
    int nLocationXEnd = (int) (fWidthStep * (nStopTime & LOCATIONWIDTHMASK));
    int nLocationYEnd = (int) (fHeightStep * (nStopTime & LOCATIONHEIGHTMASK));

    // we were at a corner, which is past the current ability of this verification.
    if(nDirectionStart != nDirectionEnd)
        goto cleanup;

    switch(nDirectionStart)
    {
        case 0:
            if(x < (nLocationXStart - LOCATIONVARIANCE) || x > (nLocationXEnd + LOCATIONVARIANCE))
            {
                FAIL(TEXT("CCaptureFramework: Direction 0: Invalid XLocation of the box."));
                Log(TEXT("CCaptureFramework: Expected between %d and %d, result is %d."), nLocationXStart, nLocationXEnd, x);
                hr = E_FAIL;
                goto cleanup;
            }

            if(y > LOCATIONVARIANCE)
            {
                FAIL(TEXT("CCaptureFramework: Direction 0: Invalid YLocation of the box."));
                Log(TEXT("CCaptureFramework: expected y 0, is %d."), y);
                hr = E_FAIL;
                goto cleanup;
            }
            break;
        case 1:
            if(y < (nLocationYStart - LOCATIONVARIANCE) || y > (nLocationYEnd + LOCATIONVARIANCE))
            {
                FAIL(TEXT("CCaptureFramework: Direction 1: Invalid YLocation of the box."));
                Log(TEXT("CCaptureFramework: Expected between %d and %d, result is %d."), nLocationYStart, nLocationYEnd, y);
                hr = E_FAIL;
                goto cleanup;
            }

            if(abs(x - (nWidth - nBoxWidth)) > LOCATIONVARIANCE)
            {
                FAIL(TEXT("CCaptureFramework: Direction 1: Invalid XLocation of the box."));
                Log(TEXT("CCaptureFramework: expected x %d, is %d."), nWidth - nBoxWidth, x);
                hr = E_FAIL;
                goto cleanup;
            }
            break;
        case 2:
            if(x > (nWidth - nLocationXStart - nBoxWidth + LOCATIONVARIANCE) || x < (nWidth - nLocationXEnd - nBoxWidth - LOCATIONVARIANCE))
            {
                FAIL(TEXT("CCaptureFramework: Direction 2: Invalid XLocation of the box."));
                Log(TEXT("CCaptureFramework: Expected between %d and %d, result is %d."), nLocationXEnd, nLocationXStart, x);
                hr = E_FAIL;
                goto cleanup;
            }

            if(abs(y - (nHeight - nBoxHeight)) > LOCATIONVARIANCE)
            {
                FAIL(TEXT("CCaptureFramework: Direction 2: Invalid YLocation of the box."));
                Log(TEXT("CCaptureFramework: expected y %d, is %d."), nWidth - nBoxWidth, y);
                hr = E_FAIL;
                goto cleanup;
            }
            break;
        case 3:
            if(y > (nHeight - nLocationYStart - nBoxHeight + LOCATIONVARIANCE)|| y < (nHeight - nLocationYEnd - nBoxHeight - LOCATIONVARIANCE))
            {
                FAIL(TEXT("CCaptureFramework: Direction 3: Invalid YLocation of the box."));
                Log(TEXT("CCaptureFramework: Expected between %d and %d, result is %d."), nLocationYEnd, nLocationYStart, y);
                hr = E_FAIL;
                goto cleanup;
            }

            if(x > LOCATIONVARIANCE)
            {
                FAIL(TEXT("CCaptureFramework: Direction 3: Invalid XLocation of the box."));
                Log(TEXT("CCaptureFramework: expected x 0, is %d."), x);
                hr = E_FAIL;
                goto cleanup;
            }
            break;
    }

cleanup:

// bitmap output for debugging.
#ifdef SAVE_BITMAPS
    if(E_FAIL == hr && pbBitmapData)
        SaveBitmap(TEXT("\\release\\CamFailure.bmp"), pbBitmapData, nWidth, nHeight, nStride);
#endif

    return hr;
}

void CCaptureFramework::ProcessMediaEvent(LONG lEventCode, LONG lParam1, LONG lParam2, LPVOID lpParameter)
{
    CCaptureFramework *pCaptureFramework = (CCaptureFramework *) lpParameter;

    CAutoLock cObjectLock(&(pCaptureFramework->m_Lock));

    // if it was a sample processed event, then increment our internal tracking count.
    if(lEventCode == EC_CAP_SAMPLE_PROCESSED)
    {
        if(lParam1 == VIDEOBUFFER)
        {
            pCaptureFramework->m_lVideoFramesProcessed++;
            if(pCaptureFramework->m_lVideoFramesProcessed > 0 && ((pCaptureFramework->m_lVideoFramesProcessed % 10) == 0))
                 Log(TEXT("Processing video frames"));
        }
        else if(lParam1 == 0)
        {
            pCaptureFramework->m_lAudioFramesProcessed++;
            if(pCaptureFramework->m_lAudioFramesProcessed > 0 && ((pCaptureFramework->m_lAudioFramesProcessed % 10) == 0))
                 Log(TEXT("Processing audio frames"));
        }
        else FAIL(TEXT("Unknown buffer type."));
    }
    // if it was an allocator OOM event, set the flag
    else if(lEventCode == EC_BUFFER_FULL)
    {
        if(lParam1 == VIDEOBUFFER)
        {
            pCaptureFramework->m_lVideoFramesDropped++;
            if(pCaptureFramework->m_lVideoFramesDropped > 0 && ((pCaptureFramework->m_lVideoFramesDropped % 20) == 0))
                Log(TEXT("Dropping video frames"));
        }
        else if(lParam1 == AUDIOBUFFER)
        {
            pCaptureFramework->m_lAudioFramesDropped++;
            if(pCaptureFramework->m_lAudioFramesDropped > 0 && ((pCaptureFramework->m_lAudioFramesDropped % 20) == 0))
                Log(TEXT("Dropping audio frames"));
        }
        else FAIL(TEXT("Unknown frame type"));
    }
    // if it was a write error event, set the flag
    else if(lEventCode == EC_CAP_FILE_WRITE_ERROR)
    {
        pCaptureFramework->m_bDiskWriteErrorOccured = TRUE;
    }
    // if it was a write error event, set the flag
    else if(lEventCode == EC_BUFFER_DOWNSTREAM_ERROR)
    {
        pCaptureFramework->m_bBufferDeliveryFailed = TRUE;
    }

    if((pCaptureFramework->m_bOutputKeyEvents && lEventCode != EC_CAP_SAMPLE_PROCESSED) || pCaptureFramework->m_bOutputAllEvents)
        pCaptureFramework->OutputEventInformation(lEventCode, lParam1, lParam2);
}


void 
CCaptureFramework::OutputEventInformation(LONG lEventCode, LONG lParam1, LONG lParam2)
{
    switch(lEventCode)
    {
        case EC_COMPLETE:
            Log(TEXT("Received a complete event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_USERABORT:
            Log(TEXT("Received a user abort event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_ERRORABORT:
            Log(TEXT("Received an error abort event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_TIME:
            Log(TEXT("Received an ec_time event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_REPAINT:
            Log(TEXT("Received a repaint event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_STREAM_ERROR_STOPPED:
            Log(TEXT("Received a stream error stopped event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_STREAM_ERROR_STILLPLAYING:
            Log(TEXT("Received a stream error still playing event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_PALETTE_CHANGED:
            Log(TEXT("Received a palette changed event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_VIDEO_SIZE_CHANGED:
            Log(TEXT("Received a video size changed event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_QUALITY_CHANGE:
            Log(TEXT("Received a quality changed event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_SHUTTING_DOWN:
            Log(TEXT("Received a shutting down event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_CLOCK_CHANGED:
            Log(TEXT("Received a clock changed event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_PAUSED:
            Log(TEXT("Received a paused event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_OPENING_FILE:
            Log(TEXT("Received an opening file event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_BUFFERING_DATA:
            Log(TEXT("Received a buffering data event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_FULLSCREEN_LOST:
            Log(TEXT("Received a fullscreen lost event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_ACTIVATE:
            Log(TEXT("Received an activate event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_NEED_RESTART:
            Log(TEXT("Received a need restart event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_WINDOW_DESTROYED:
            Log(TEXT("Received a window destroyed event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_DISPLAY_CHANGED:
            Log(TEXT("Received a display changed event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_STARVATION:
            Log(TEXT("Received a starvation event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_OLE_EVENT:
            Log(TEXT("Received an ole event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_NOTIFY_WINDOW:
            Log(TEXT("Received a notify window event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_STREAM_CONTROL_STARTED:
        case EC_STREAM_CONTROL_STOPPED:
            {
                TCHAR tszFilterInfo[256] = TEXT("an unknown filter");
                IPin *pPin = (IPin *) lParam1;
                PIN_INFO pPinInfo;
                FILTER_INFO pFilterInfo;

                if(pPin)
                {
                    if(SUCCEEDED(pPin->QueryPinInfo(&pPinInfo)))
                    {
                        if(pPinInfo.pFilter)
                        {
                            if(SUCCEEDED(pPinInfo.pFilter->QueryFilterInfo(&pFilterInfo)))
                            {
                                _sntprintf(tszFilterInfo, countof(tszFilterInfo), TEXT("an %s pin named %s on filter %s"), pPinInfo.dir == PINDIR_INPUT?TEXT("input"):TEXT("output"), pPinInfo.achName, pFilterInfo.achName);
                                if(pFilterInfo.pGraph)
                                    pFilterInfo.pGraph->Release();
                            }
                            pPinInfo.pFilter->Release();

                        }
                        else _sntprintf(tszFilterInfo, countof(tszFilterInfo), TEXT("an %s pin named %s"), pPinInfo.dir == PINDIR_INPUT?TEXT("input"):TEXT("output"), pPinInfo.achName);
                    }
                }

                if(lEventCode == EC_STREAM_CONTROL_STOPPED)
                    Log(TEXT("Received a stream control stopped event, from %s with a cookie 0x%08x."), tszFilterInfo, lParam2);
                else if(lEventCode == EC_STREAM_CONTROL_STARTED)
                    Log(TEXT("Received a stream control started event, from %s with a cookie 0x%08x."), tszFilterInfo, lParam2);
            }
            break;
        case EC_END_OF_SEGMENT:
            Log(TEXT("Received an end of segment event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_SEGMENT_STARTED:
            Log(TEXT("Received a segment started event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_LENGTH_CHANGED:
            Log(TEXT("Received a length changed event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_STEP_COMPLETE:
            Log(TEXT("Received a step complete event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_TIMECODE_AVAILABLE:
            Log(TEXT("Received a timecode available event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_EXTDEVICE_MODE_CHANGE:
            Log(TEXT("Received an extdevice moe change event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
#ifdef VMR_ENABLE
        case EC_VMR_RENDERDEVICE_SET:
            Log(TEXT("Received a vmr render device set event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_VMR_SURFACE_FLIPPED:
            Log(TEXT("Received a vmr surface flipped event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_VMR_RECONNECTION_FAILED:
            Log(TEXT("Received a vmr reconnection failed event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
#endif
        case EC_PLEASE_REOPEN:
            Log(TEXT("Received a please reopen event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_STATUS:
            Log(TEXT("Received a status event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_MARKER_HIT:
            Log(TEXT("Received a marker hit event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_LOADSTATUS:
            Log(TEXT("Received a loadstatus event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_FILE_CLOSED:
            Log(TEXT("Received a file closed event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_ERRORABORTEX:
            Log(TEXT("Received an errorabortex event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_EOS_SOON:
            Log(TEXT("Received an eos soon event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_CONTENTPROPERTY_CHANGED:
            Log(TEXT("Received a content property changed event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_BANDWIDTHCHANGE:
            Log(TEXT("Received a bandwidth changed event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_VIDEOFRAMEREADY:
            Log(TEXT("Received a video frame ready event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_DRMSTATUS:
            Log(TEXT("Received a drmstatus event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_VCD_SELECT:
            Log(TEXT("Received a vcd select event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_VCD_PLAY:
            Log(TEXT("Received a vcd play event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_VCD_END:
            Log(TEXT("Received a vcd end event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_PLAY_NEXT:
            Log(TEXT("Received a play next event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_PLAY_PREVIOUS:
            Log(TEXT("Received a play previous event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_DRM_LEVEL:
            Log(TEXT("Received a drm level event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_STREAM_ERROR_RECOVERED:
            Log(TEXT("Received a stream error recovered event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        case EC_CAP_FILE_COMPLETED:
            if(lParam1)
                Log(TEXT("Received a file completed event for a successful capture with the file name %s."), (TCHAR *) lParam2);
            else if(lParam2)
                Log(TEXT("Received a file completed event for a failed capture with the file name %s."), (TCHAR *) lParam2);
            else
                Log(TEXT("Received a file completed event for a failed capture with an invalid file name."));                
            break;
        case EC_CAP_SAMPLE_PROCESSED:
            Log(TEXT("Received a %s sample processed event with a timestamp of 0x%08x."), lParam1?TEXT("video"):TEXT("audio"), lParam2);
            break;
        case EC_BUFFER_FULL:
            Log(TEXT("Received a %s buffer full event due to %s."), lParam1==1?TEXT("video"):TEXT("audio"), lParam2?TEXT("user requested limit"): TEXT("OOM"));
            break;
        case EC_CAP_FILE_WRITE_ERROR:
            Log(TEXT("Received a file write error event, param1 0x%08x, HRESULT 0x%08x."), lParam1, lParam2);
            break;
        case EC_BUFFER_DOWNSTREAM_ERROR:
            Log(TEXT("Received a downstream error event, 0x%08x, 0x%08x."), lParam1, lParam2);
            break;
        default:
            Log(TEXT("Received unrecognized media event 0x%08x, param1 0x%08x, param2 0x%08x."), lEventCode, lParam1, lParam2);
            break;
    }
}

void 
CCaptureFramework::OutputMissingEventsInformation(DWORD dwEvent)
{
    if(dwEvent == EC_STREAM_CONTROL_STARTED)
    {
        FAIL(TEXT("CCaptureFramework: Failed to retrieve the control stream event for starting audio or video."));
        FAIL(TEXT("CCaptureFramework: This event indicates to the application that the stream started flowing and samples were received by DirectShow."));
        FAIL(TEXT("CCaptureFramework: One possible cause for this is the camera driver or audio driver is  failing to deliver media or delivering samples"));
        FAIL(TEXT("CCaptureFramework: with incorrect timestamps. Another possible cause of this is a high frame rate or capture size resulting in"));
        FAIL(TEXT("CCaptureFramework: 100% cpu utilization to render the preview and starving the encoding threads."));
        FAIL(TEXT("CCaptureFramework: Add /OutputAllEvents or /InsertDiagnosticsFilters to the command line to get more information."));
    }
    else if(dwEvent == EC_STREAM_CONTROL_STOPPED)
    {
        FAIL(TEXT("CCaptureFramework: Failed to retrieve the control stream event for stopping audio or video. This event indicates to the application that the"));
        FAIL(TEXT("CCaptureFramework: last sample was received by the multiplexer. One possible cause for this is the camera driver or audio driver is failing"));
        FAIL(TEXT("CCaptureFramework: to deliver media or delivering samples with incorrect timestamps. Another possible cause of this is a high frame rate"));
        FAIL(TEXT("CCaptureFramework: or capture size resulting in 100% cpu utilization to render the preview and starving the encoding threads."));
        FAIL(TEXT("CCaptureFramework: Add /OutputAllEvents or /InsertDiagnosticsFilters to the command line to get more information."));
    }
    if(dwEvent == EC_CAP_FILE_COMPLETED)
    {
        FAIL(TEXT("CCaptureFramework: Failed to retrieve the captured event, did your camera driver deliver the still image frame to DirectShow?"));
        FAIL(TEXT("CCaptureFramework: Add /OutputAllEvents or /InsertDiagnosticsFilters to the command line to get more information."));
    }

    DShowEvent dse[3];
    DShowEvent *dseReceived;

    dse[0].Code = EC_STREAM_CONTROL_STARTED;
    dse[0].FilterFlags = EVT_EVCODE_EQUAL;
    dse[1].Code = EC_STREAM_CONTROL_STOPPED;
    dse[1].FilterFlags = EVT_EVCODE_EQUAL;
    dse[2].Code = EC_CAP_FILE_COMPLETED;
    dse[2].FilterFlags = EVT_EVCODE_EQUAL;

    Log(TEXT("The following video and still capture control events were recieved in the following order:"));

    dseReceived = m_EventSink.FindFirstEvent(3, dse);
    do
    {
        if(dseReceived)
            OutputEventInformation(dseReceived->Code, dseReceived->Param1, dseReceived->Param2);

        dseReceived = m_EventSink.FindNextEvent();
    } while(dseReceived);

    Log(TEXT("End of event list."));
}

HRESULT
CCaptureFramework::RegisterDll(TCHAR * tszDllName)
{
    HRESULT hr = E_FAIL;
    HINSTANCE hInst = NULL;
    pFuncpfv pfv = NULL;

    hInst = LoadLibrary(tszDllName);
    if(hInst)
    {
        pfv = (pFuncpfv)GetProcAddress(hInst, TEXT("DllRegisterServer"));
        if(pfv)
        {
            (*pfv)();
            hr = S_OK;
        }
        FreeLibrary(hInst);
    }

    return hr;
}


void 
CCaptureFramework::DisplayPinsOfFilter(IBaseFilter *pFilter)
{
    CComPtr<IEnumPins> pEnum = NULL;
    CComPtr<IPin> pPin = NULL;
    CComPtr<IPin> pConnectedToPin = NULL;
    PIN_INFO   pinInfo;

    HRESULT hr = pFilter->EnumPins(&pEnum);

    if (SUCCEEDED(hr))
    {
        while(pEnum->Next(1, &pPin, 0) == S_OK)
        {
            if(SUCCEEDED(pPin->QueryPinInfo(&pinInfo)))
            {
                pPin->ConnectedTo(&pConnectedToPin);
            
                Log(TEXT("    Pin(%s) %s - %s"), pinInfo.dir == PINDIR_INPUT? TEXT("INPUT") : TEXT("OUTPUT"), pinInfo.achName, pConnectedToPin==NULL ? TEXT("Unconnected") : TEXT("Connected"));
                
                if(pConnectedToPin != NULL)
                {
                    pConnectedToPin = NULL;
                }

                if(pinInfo.pFilter != NULL)
                {
                    pinInfo.pFilter->Release();
                    pinInfo.pFilter = NULL;
                }
            }

            pPin = NULL;
        }

        pEnum = NULL;
    }
}

HRESULT
CCaptureFramework::DisplayFilters()
{
    CComPtr<IEnumFilters> pIEnumFilters;
    CComPtr<IBaseFilter> pIBaseFilter;
    ULONG cFetched;
    FILTER_INFO filterInfo;

    HRESULT hr = m_pGraph->EnumFilters(&pIEnumFilters);

    if(SUCCEEDED(hr))
    {
        hr = E_FAIL;

        while(pIEnumFilters->Next(1, &pIBaseFilter, &cFetched) == S_OK)
        {
            hr = pIBaseFilter->QueryFilterInfo(&filterInfo);

            Log(TEXT("Filter %s"), filterInfo.achName);

            DisplayPinsOfFilter(pIBaseFilter);

            if(filterInfo.pGraph != NULL)
            {
                filterInfo.pGraph->Release();
                filterInfo.pGraph = NULL;
            }

            pIBaseFilter = NULL;
        }

        pIEnumFilters = NULL;
    }

    return hr;
}

HRESULT
CCaptureFramework::InsertFilter(IBaseFilter *pFilterBase, PIN_DIRECTION pindir, const GUID *pCategory, const GUID *pType, int index, IBaseFilter *pFilterToInsert)
{
    HRESULT hr = S_OK;
    CComPtr<IEnumPins> pEnum = NULL;
    CComPtr<IPin> pPinUpstreamOut = NULL;
    CComPtr<IPin> pPinDownstreamIn = NULL;
    CComPtr<IPin> pPinInsertIn = NULL;
    CComPtr<IPin> pPinInsertOut = NULL;
    CComPtr<IPin> pConnectedToPin = NULL;
    PIN_INFO   pinInfo;
    AM_MEDIA_TYPE mtNegotiated;

    memset(&mtNegotiated, 0, sizeof(AM_MEDIA_TYPE));

    // From the base filter we find the pin in question
    // if we're looking for an input filter, then the base is the downstream in.
    // if we're looking for an output filter, then the base is the upstream output.
    if(pindir == PINDIR_INPUT)
    {
        hr = m_pCaptureGraphBuilder->FindPin(pFilterBase, pindir, pCategory, pType, FALSE, index, &pPinDownstreamIn);
        if(SUCCEEDED(hr))
        {
            hr = pPinDownstreamIn->ConnectedTo(&pPinUpstreamOut);
            if(FAILED(hr))
                FAIL(TEXT("CCaptureFramework::InsertFilter: The requested pin was found, but not connected to anything."));
        }
        else FAIL(TEXT("CCaptureFramework::InsertFilter: Finding the requested pin failed."));        
    }
    else
    {
        hr = m_pCaptureGraphBuilder->FindPin(pFilterBase, pindir, pCategory, pType, FALSE, index, &pPinUpstreamOut);
        if(SUCCEEDED(hr))
        {
            hr = pPinUpstreamOut->ConnectedTo(&pPinDownstreamIn);
            if(FAILED(hr))
                FAIL(TEXT("CCaptureFramework::InsertFilter: The requested pin was found, but not connected to anything."));
        }
        else FAIL(TEXT("CCaptureFramework::InsertFilter: Finding the requested pin failed."));
    }

    // if we found both the inputs and outputs, insert the intermediate filter
    if(SUCCEEDED(hr))
    {
        // retrieve the current negotiated media type to reuse.
        hr = pPinUpstreamOut->ConnectionMediaType(&mtNegotiated);
        if(SUCCEEDED(hr))
        {
            // disconnect the two pins and insert the filter requested
            // if we grabbed the pins we need from above, start with the input pin
            // for the filter being inserted
            if(SUCCEEDED(hr = pFilterToInsert->EnumPins(&pEnum)))
            {
                while((hr = pEnum->Next(1, &pPinInsertIn, 0)) == S_OK)
                {
                    if(SUCCEEDED(hr = pPinInsertIn->QueryPinInfo(&pinInfo)))
                    {
                        if(pinInfo.pFilter)
                        {
                            pinInfo.pFilter->Release();
                            pinInfo.pFilter = NULL;
                        }

                        pPinInsertIn->ConnectedTo(&pConnectedToPin);
                        if(pConnectedToPin)
                            pConnectedToPin = NULL;
                        // as long as we're not connected to anything, the direction is right, and this pin will accept the media type we need,
                        // then it's the one we're looking for.  Break out of the loop leaving the input pin interface available.
                        else if(pinInfo.dir == PINDIR_INPUT && SUCCEEDED(pPinInsertIn->QueryAccept(&mtNegotiated)))
                            break;
                    }

                    pPinInsertIn = NULL;
                }
                pEnum=NULL;
            }
            else FAIL(TEXT("CCaptureFramework::InsertFilter: Failed to enumerate the intermediate input pins."));

            if(pPinInsertIn == NULL)
                FAIL(TEXT("CCaptureFramework::InsertFilter: Failed to retrieve the intermediate input pin."));

            // if we have the intermediate input pin, now find the intermediate output pin and insert it
            if(SUCCEEDED(hr))
            {
                // now lets find the insertion filter output pin
                if (SUCCEEDED(hr = pFilterToInsert->EnumPins(&pEnum)))
                {
                    while((hr = pEnum->Next(1, &pPinInsertOut, 0)) == S_OK)
                    {
                        if(SUCCEEDED(hr = pPinInsertOut->QueryPinInfo(&pinInfo)))
                        {
                            if(pinInfo.pFilter)
                            {
                                pinInfo.pFilter->Release();
                                pinInfo.pFilter = NULL;
                            }

                            pPinInsertOut->ConnectedTo(&pConnectedToPin);
                            if(pConnectedToPin)
                                pConnectedToPin = NULL;
                            // as long as it's not connected, it's an output, and it'll accept the negotiated format, then it's the one we're looking for,
                            // break out and leave it available.
                            else if(pinInfo.dir == PINDIR_OUTPUT && SUCCEEDED(pPinInsertOut->QueryAccept(&mtNegotiated)))
                                break;
                        }

                        pPinInsertOut = NULL;
                    }
                    pEnum=NULL;
                }
                else FAIL(TEXT("CCaptureFramework::InsertFilter: Failed to enumerate the intermediate output pins."));

                if(pPinInsertOut == NULL)
                    FAIL(TEXT("CCaptureFramework::InsertFilter: Failed to retrieve the intermediate output pin."));

                // if everything is good so far, we have the insertion point and insertion pins, disconnect the upstream and downstream pins, and
                // insert the intermediate
                if(SUCCEEDED(hr))
                {
                    if(SUCCEEDED(hr = m_pGraph->Disconnect(pPinUpstreamOut)))
                    {
                        if(SUCCEEDED(hr = m_pGraph->Disconnect(pPinDownstreamIn)))
                        {
                            if(SUCCEEDED(hr = m_pGraph->ConnectDirect(pPinUpstreamOut, pPinInsertIn, &mtNegotiated)))
                            {
                                hr = m_pGraph->ConnectDirect(pPinInsertOut, pPinDownstreamIn, &mtNegotiated);
                                if(FAILED(hr))
                                    FAIL(TEXT("CCaptureFramework::InsertFilter: Failed to connect the downstream input pin to the intermediate output."));
                            }
                            else FAIL(TEXT("CCaptureFramework::InsertFilter: Failed to connect the upstream output pin to the intermediate input."));

                        }
                        else FAIL(TEXT("CCaptureFramework::InsertFilter: Failed to disconnect the downstream input pin for insertion."));
                    }
                    else FAIL(TEXT("CCaptureFramework::InsertFilter: Failed to disconnect the upstream output pin for insertion."));
                }
            }
        }
        else FAIL(TEXT("CCaptureFramework::InsertFilter: Failed retrieving the connection media type."));
    }

    return hr;
}

HRESULT
CCaptureFramework::InsertGrabberFilter(IBaseFilter *pFilterBase, PIN_DIRECTION pindir, const GUID *pCategory, const GUID *pType, int index, DIAGNOSTICS_FILTER_INDEX FilterIndex)
{
    HRESULT hr = S_OK;
    CComPtr<IBaseFilter> pGrabber;
    CComPtr<ICameraGrabber> pGrabberSample;

    // cocreate the grabber
    hr = CoCreateInstance( CLSID_CameraGrabber, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**) &pGrabber );
    if(FAILED(hr) || pGrabber == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Failed to cocreate the camera grabber filter."));
        goto cleanup;
    }

    // add it to the graph
    hr = m_pGraph->AddFilter(pGrabber, DiagnosticsData[FilterIndex][FILTER_NAME]);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Adding the grabber failed."));
        goto cleanup;
    }

    // insert it after the vcap filter
    hr = InsertFilter(pFilterBase, pindir, pCategory, pType, index, pGrabber);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Failed to insert grabber filter."));
        Log(TEXT("CCaptureFramework: Failed to insert %s %s."), DiagnosticsData[FilterIndex][FILTER_NAME], DiagnosticsData[FilterIndex][FILTER_LOCATION] );
        goto cleanup;
    }

    // set up the callback

    // query for IGrabberSample
    hr = pGrabber.QueryInterface(&pGrabberSample);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Failed to find the still grabber interface."));
        goto cleanup;
    }

    // Set the callback
    hr = pGrabberSample->SetCallback(&CCaptureFramework::GrabberCallback, 
                                                         (LPVOID) FilterIndex);
    if(FAILED(hr))
    {
        FAIL(TEXT("CCaptureFramework: Failed to set the still grabber callback."));
        goto cleanup;
    }

cleanup:
    return hr;
}

