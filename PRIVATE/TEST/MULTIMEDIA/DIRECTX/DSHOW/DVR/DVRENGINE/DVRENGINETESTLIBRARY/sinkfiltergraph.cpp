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
#include "StdAfx.h"
#include "sinkfiltergraph.h"
#include "Helper.h"
#include "Grabber.h"
#include "qinnullrenderer.h"
#include <atlbase.h>
#include "logger.h"
#include <bdaiface.h>
#include <dshowfix.h>

#define SINK_FILTERGRAPH_NAME TEXT("Sink Filter Graph")
#define RECORDING_PATH L"\\Hard Disk2\\"

// the information for our default source file.
#define MPGFile L"\\Hard Disk\\sample1.vob"
#define VIDEOSTREAMPID 0xE0
#define AUDIOSTREAMPID 0xBD
#define AUDIOSTREAMSUBTYPE MEDIASUBTYPE_DOLBY_AC3

// the id's and sub types for a VOB file
#define VOBVIDEOSTREAMPID 0xE0
#define VOBAUDIOSTREAMPID 0xBD
#define VOBAUDIOSTREAMSUBTYPE MEDIASUBTYPE_DOLBY_AC3


CSinkFilterGraph::CSinkFilterGraph()
{
    DVRGraphLayout |= SinkMPEGGraph | SinkASyncFileSource;
    m_UserSpecifiedSourceCLSID = GUID_NULL;
    _tcscpy(m_pszSourceVideoFile, MPGFile);
    _tcscpy(m_pszRecordingPath, RECORDING_PATH);
    m_VideoStreamPID = VIDEOSTREAMPID;
    m_AudioStreamPID = AUDIOSTREAMPID;
    m_AudioSubType = AUDIOSTREAMSUBTYPE;
}

CSinkFilterGraph::~CSinkFilterGraph(void)
{
    LogText(TEXT("CSinkFilterGraph::~CSinkFilterGraph"));
}

BOOL CSinkFilterGraph::SetCommandLine(const TCHAR * CommandLine)
{
    LPCTSTR pszFnName = TEXT("CSinkFilterGraph::SetCommandLine");
    BOOL bReturnValue = CFilterGraph::SetCommandLine(CommandLine);

    // if no command line was given, then there's nothing for us to do.
    if(!CommandLine)
    {
        return FALSE;
    }

    class CClParse cCLPparser(pszCommandLine);

    if(cCLPparser.GetOpt(TEXT("CETKTestInformation")))
    {
        LogText(TEXT(""));
        LogText(TEXT("###CETK USAGE INFORMATION###"));
        LogText(TEXT(""));
        LogText(TEXT("A DVREngine compatible MPEG2 video source filter are not shipped with Windows CE. In order to use"));
        LogText(TEXT("the DVR engine CETK tests you must specify the GUID of the tuner source via the test"));
        LogText(TEXT("command line. Please use the SinkUserSource command line option to set this GUID."));
        LogText(TEXT("The full DVR sink graph command line options follow:"));
        bReturnValue = FALSE;
    }

    if(cCLPparser.GetOpt(TEXT("?")) || !bReturnValue)
    {
        LogText(TEXT(""));
        LogText(TEXT("/? outputs this message"));
        LogText(TEXT(""));
        LogText(TEXT("/CETKTestInformation - Output usage information for the test within the CETK."));
        LogText(TEXT("/SinkUserSource <guid> - Specify a GUID for the user's MPEG source filter/tuner"));
        LogText(TEXT("/SinkRecordLocation <path> - specify the location the dvr files are recorded to (default is hard disk2)"));
        LogText(TEXT(""));
        LogText(TEXT("The following options only apply when using a file source filter to simulate a DVR stream, and require additional demultiplexer and null rendering filters to use."));
        LogText(TEXT("/SinkASyncSource /File <optional transport stream or VOB file name, omit /File to use the default>"));
        LogText(TEXT("    /VideoStreamID <ID, necessary if specifying a transport stream file>"));
        LogText(TEXT("    /AudioStreamID <ID, necessary if specifying a transport stream file>"));
        LogText(TEXT("        /AudioSubtype <PCM, AC3, or MPEG2, necessary if specifying a transport stream file>"));
        // This is to use the MPEGFileSource filter, which is a dedicated vob file reader, disabled for now as the
        // AsyncFileSource with the PSMux can accomplish the same thing.
        //LogText(TEXT("/SinkFileSource /File <optional vob file name, omit /File for default>"));
        LogText(TEXT(""));

        bReturnValue = FALSE;
    }
    else if(bReturnValue)
    {
        // If the user requested the MPEGFileSource filter
        if(cCLPparser.GetOpt(TEXT("SinkFileSource")))
        {
            DVRGraphLayout = ((DVRGraphLayout & ~(SinkASyncFileSource | SinkUserSource | SinkTunerSource)) | SinkFileSource);

            // if the user didn't specify a file to use, then use the default
            if(!cCLPparser.GetOptString(TEXT("File"), m_pszSourceVideoFile, MAX_PATH))
                _tcscpy(m_pszSourceVideoFile, MPGFile);

            // VOB's all have the same audio/video id's and audio sub type.
            m_VideoStreamPID = VOBVIDEOSTREAMPID;
            m_AudioStreamPID = VOBAUDIOSTREAMPID;
            m_AudioSubType = VOBAUDIOSTREAMSUBTYPE;
        }
        // else parse out the file source file name
        else if(cCLPparser.GetOpt(TEXT("SinkASyncSource")))
        {
            DVRGraphLayout = ((DVRGraphLayout & ~(SinkFileSource | SinkUserSource | SinkTunerSource)) | SinkASyncFileSource);

            if(!cCLPparser.GetOptString(TEXT("File"), m_pszSourceVideoFile, MAX_PATH))
            {
                // if the user didn't specify a file, then use the default file
                _tcscpy(m_pszSourceVideoFile, MPGFile);
                m_VideoStreamPID = VIDEOSTREAMPID;
                m_AudioStreamPID = AUDIOSTREAMPID;
                m_AudioSubType = AUDIOSTREAMSUBTYPE;
            }
            else
            {
                // if the file given is a VOB file, then set the default VOB source
                // id's and sub types, but allow the user to override them.
                DWORD dwVideoStreamPID = 0;
                bool IsTS = true;
                wchar_t* pExt = wcsrchr(m_pszSourceVideoFile, L'.');
                if ( pExt && 0 == wcsicmp(pExt, L".vob") )
                {
                    IsTS = false;
                    m_VideoStreamPID = VOBVIDEOSTREAMPID;
                    m_AudioStreamPID = VOBAUDIOSTREAMPID;
                    m_AudioSubType = VOBAUDIOSTREAMSUBTYPE;
                }

                if(cCLPparser.GetOptVal(TEXT("VideoStreamID"), &dwVideoStreamPID))
                {
                    m_VideoStreamPID = dwVideoStreamPID;
                }
                // If a video stream id wasn't given, then it's ok if it's a vob file
                else if(IsTS)
                {
                    LogError(__LINE__, pszFnName, TEXT("When specifying a transport stream file file for the AsyncFileSource, a video stream id is necessary."));

                    m_VideoStreamPID = 0;
                    bReturnValue = FALSE;
                }


                DWORD dwAudioStreamPID = 0;
                TCHAR tszAudioStreamType[10];

                if(cCLPparser.GetOptVal(TEXT("AudioStreamID"), &dwAudioStreamPID))
                {
                    m_AudioStreamPID = dwAudioStreamPID;

                    if(cCLPparser.GetOptString(TEXT("AudioSubtype"), tszAudioStreamType, (sizeof(tszAudioStreamType)/sizeof(*(tszAudioStreamType)))))
                    {
                        if(!_tcscmp(tszAudioStreamType, TEXT("PCM")))
                            m_AudioSubType = MEDIASUBTYPE_DVD_LPCM_AUDIO;
                        else if(!_tcscmp(tszAudioStreamType, TEXT("AC3")))
                            m_AudioSubType = MEDIASUBTYPE_DOLBY_AC3;
                        else if(!_tcscmp(tszAudioStreamType, TEXT("MPEG2")))
                            m_AudioSubType = MEDIASUBTYPE_MPEG2_AUDIO;
                        else
                        {
                            LogError(__LINE__, pszFnName, TEXT("Unrecognized audio stream type, must be PCM, AC3, or MPEG2 (case sensitive)."));
                            bReturnValue = FALSE;
                            m_AudioStreamPID = 0;
                        }
                    }
                    else if(IsTS)
                    {
                        LogError(__LINE__, pszFnName, TEXT("When specifying a audio stream id for a transport stream file, an audio stream type is necessary."));
                        bReturnValue = FALSE;
                        m_AudioStreamPID = 0;
                    }
                }
                // if an audio stream id wasn't given, then it's ok if it's a vob file
                else if(IsTS)
                {
                    LogError(__LINE__, pszFnName, TEXT("When specifying a transport stream file for the AsyncFileSource, and audio stream id is necessary."));
                    bReturnValue = FALSE;
                    m_AudioStreamPID = 0;
                }
            }
        }
        // else parse out the source filter guid
        else
        {
            TCHAR *ptszCmdLineBackup = NULL;
            DWORD dwCmdLineLength = _tcslen(pszCommandLine) + 1;
            TCHAR *ptszToken;
            CLSID clsid;

            if(dwCmdLineLength > 1)
            {
                // first, backup the command line
                ptszCmdLineBackup = new TCHAR[dwCmdLineLength];
                if(ptszCmdLineBackup)
                {
                    _tcsncpy(ptszCmdLineBackup, pszCommandLine, dwCmdLineLength);

                    // now tokenize it for spaces and forward slashes, if we tokenize for dashes
                    // then the dashes in the clsid mess everything up.
                    ptszToken = ::_tcstok(ptszCmdLineBackup, _T(" /"));
                    while(NULL != ptszToken)
                    {
                        // if we found the option we want, then grab the value following the token
                        if(0 == ::_tcsicmp(ptszToken, TEXT("SinkUserSource")))
                        {
                            ptszToken = ::_tcstok(NULL, _T(" "));
                            if(NULL != ptszToken)
                            {
                                // try to convert the value to a clsid, if that fails, then we have a problem.
                                if(FAILED(CLSIDFromString(ptszToken, &clsid)))
                                {
                                    LogError(__LINE__, pszFnName, TEXT("Invalid string given for encoder CLSID."));
                                    LogText(__LINE__, pszFnName, TEXT("Invalid string was %s"), ptszToken);
                                    bReturnValue = FALSE;
                                }
                                else
                                {
                                    m_UserSpecifiedSourceCLSID = clsid;
                                    DVRGraphLayout = ((DVRGraphLayout & ~(SinkASyncFileSource | SinkFileSource | SinkTunerSource)) | SinkUserSource);
                                }

                                // we found what we were looking for, so we're done.
                                break;
                            }
                        }
                        ptszToken = ::_tcstok(NULL, _T(" /"));
                    }
                    delete[] ptszCmdLineBackup;
                }
            }
        }

        // parse out the default recording path
        if(!cCLPparser.GetOptString(TEXT("SinkRecordLocation"), m_pszRecordingPath, MAX_PATH))
        {
           // set the graph layout to that
           _tcscpy(m_pszRecordingPath, RECORDING_PATH);
        }
    }

    return bReturnValue;
}

void
CSinkFilterGraph::OutputCurrentCommandLineConfiguration()
{
    LPOLESTR Temp = NULL;

    LogText(TEXT("DVRGraphLayout is 0x%08x."), DVRGraphLayout);

    // if source test graph output that
    if(DVRGraphLayout & (SinkFileSource || SinkASyncFileSource))
    {
        LogText(TEXT("Using a file source filter with a file of %s, Video PID is 0x%04x, Audio PID is 0x%04x."), m_pszSourceVideoFile, m_VideoStreamPID, m_AudioStreamPID);

        // output the source decoder guid
        StringFromCLSID(m_AudioSubType, &Temp);
        LogText(TEXT("Using audio sub type %s."), Temp);
        CoTaskMemFree(Temp);
        Temp = NULL;
    }

    if(DVRGraphLayout & SinkUserSource)
    {
        // output the source decoder guid
        StringFromCLSID(m_UserSpecifiedSourceCLSID, &Temp);
        LogText(TEXT("Using a user source filter %s."), Temp);
        CoTaskMemFree(Temp);
        Temp = NULL;
    }

    // output the source video path
    LogText(TEXT("Saving DVR files to %s"), m_pszRecordingPath);

    return;
}


HRESULT CSinkFilterGraph::Initialize()
{
    HRESULT hr = CFilterGraph::Initialize();

    CFilterGraph::SetName(SINK_FILTERGRAPH_NAME);

    return hr;
}

void CSinkFilterGraph::UnInitialize()
{
    Stop();

    CFilterGraph::UnInitialize();

    CFilterGraph::SetName(NULL);
}

HRESULT CSinkFilterGraph::SetupFilters(LPCOLESTR pszSourceFileName)
{
    return SetupFilters(pszSourceFileName, FALSE);
}

HRESULT CSinkFilterGraph::SetupFilters(BOOL bUseGrabber)
{
    return SetupFilters(m_pszSourceVideoFile, bUseGrabber);
}

HRESULT CSinkFilterGraph::SetupFilters()
{
    return SetupFilters(m_pszSourceVideoFile, FALSE);
}

HRESULT CSinkFilterGraph::SetupFilters(LPCOLESTR pszSourceFileName, BOOL bUseGrabber)
{
    LPCTSTR pszFnName = TEXT("CSinkFilterGraph::SetupFilters");
    CComPtr<IBaseFilter> pIBaseFilter_Source;
    CComPtr<IBaseFilter> pIBaseFilter_dvr_sink;
    CComPtr<IFileSourceFilter> pIFileSourceFilter;
    CComPtr<IBaseFilter> pIBaseFilter_Grabber;
    CComPtr<IGrabberSample> pIGrabberSample;
    CComPtr<INullFilter> pINullFilter;

    // used for async file source test graph
    CComPtr<IBaseFilter> pIBaseFilter_MPEG2Demux;
    CComPtr<IBaseFilter> pIBaseFilter_TeeFilter;
    CComPtr<IBaseFilter> pIBaseFilter_NullRenderer;
    CComPtr<IBaseFilter> pIBaseFilter_PSMux;
    CComPtr<IBaseFilter> pIBaseFilter_Pull2Push;
    GUID VideoSourceFilter = GUID_NULL;
    HRESULT hr = E_FAIL;

    if(DVRGraphLayout & SinkMPEGGraph)
    {
        if(DVRGraphLayout & SinkTunerSource)
            VideoSourceFilter = CLSID_LSTEncoderTuner;
        else if(DVRGraphLayout & SinkUserSource)
            VideoSourceFilter = m_UserSpecifiedSourceCLSID;
        else if(DVRGraphLayout & SinkFileSource)
            VideoSourceFilter = CLSID_MpegSource;
        else if(DVRGraphLayout & SinkASyncFileSource)
            VideoSourceFilter = CLSID_AsyncReader;
    }
    else if(DVRGraphLayout & SinkASFGraph)
    {
        VideoSourceFilter = CLSID_AsfSource;
    }

    // Before building the sink graph we need to set the default audio format for the DVR splitter
    // filter. This is only necessary when using a file source, when using the tuner source it's necessary
    // for the OEM to set this value to what's needed by their system.
    if(DVRGraphLayout & (SinkFileSource || SinkASyncFileSource))
        Helper_DvrSetAudioType(m_AudioSubType);

    // if we're using the async filter, then we're using the MPEG2 demultiplexer filter.
    // set it's priorities before we try to use it.
    if(DVRGraphLayout & SinkASyncFileSource)
    {
        // now set the thread priority for the MPEG2 demultiplexer filter
        HKEY hKey;
        DWORD dwType, dwSize, dwValue;

        // if the MPEG-2 demultiplexer reg key doesn't exist, then we have bigger problems and
        // we'll fail accordingly when we try to use it.
        if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CLASSES_ROOT, 
                                          TEXT("CLSID\\{AFB6C280-2C41-11D3-8A60-0000F81E0E4A}\\Pins\\MPEG-2 Stream\\"), 
                                          0, 0, &hKey))
        {
            dwSize = sizeof (DWORD);
            dwType = REG_DWORD;
            // for CeSetThreadPriority we add 248 to the thread priority base value
            dwValue = THREAD_PRIORITY_HIGHEST + 248;
            // if setting the registry key fails, there's not much we can do.
            if(ERROR_SUCCESS != RegSetValueEx (hKey, TEXT("ThreadPriority"), NULL, dwType, LPBYTE(&dwValue), dwSize))
                LogText(__LINE__, pszFnName, TEXT("Failed to set the ThreadPriority value for the MPEG2 demultiplexer."));

            RegCloseKey (hKey);
        }
        else
        {
            LogText(__LINE__, pszFnName, TEXT("Failed to open the MPEG2 demultiplexer registry key to set it's priority."));
        }


        // if the PSMultiplexer reg key doesn't exist, then we have bigger problems and
        // we'll fail accordingly when we try to use it.
        if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CLASSES_ROOT, 
                                           TEXT("CLSID\\{E9FA2A46-8CD8-4A5C-AE52-B82378703A69}"),
                                           0, 0, &hKey))
        {
            dwSize = sizeof (DWORD);
            dwType = REG_DWORD;
            // for CeSetThreadPriority we add 248 to the thread priority base value
            dwValue = THREAD_PRIORITY_HIGHEST + 248;
            // if setting the registry key fails, there's not much we can do.
            if(ERROR_SUCCESS != RegSetValueEx (hKey, TEXT("ThreadPriority"), NULL, dwType, LPBYTE(&dwValue), dwSize))
                LogText(__LINE__, pszFnName, TEXT("Failed to set the ThreadPriority value for the PSMultiplexer."));

            RegCloseKey (hKey);
        }
        else
        {
            LogText(__LINE__, pszFnName, TEXT("Failed to open the PSMultiplexer registry key to set it's priority."));
        }
    }

    hr = this->AddFilterByCLSID(VideoSourceFilter, TEXT("Video Source Filter"), &pIBaseFilter_Source);

    if(SUCCEEDED(hr) && (DVRGraphLayout & SinkMPEGGraph) && (DVRGraphLayout & SinkASyncFileSource))
    {
        hr = this->AddFilterByCLSID(CLSID_MPEG2Demultiplexer, TEXT("MPEG2 transport stream demux"), &pIBaseFilter_MPEG2Demux);
        if(SUCCEEDED(hr))
        {
            hr = this->AddFilterByCLSID(CLSID_Tee, TEXT("Infinite tee filter"), &pIBaseFilter_TeeFilter);
            if(SUCCEEDED(hr))
            {
                hr = this->AddFilterByCLSID(CLSID_QinNullRenderer, TEXT("Null renderer filter"), &pIBaseFilter_NullRenderer);
                if(SUCCEEDED(hr))
                {
                    // As this instance of the qinnullrenderer isn't for video (it's for timing the rendering,
                    // disable it's IKSPropertySet interface to avoid DVREngine confusion from the multiple
                    // renderer's.
                    hr = pIBaseFilter_NullRenderer->QueryInterface(&pINullFilter);

                    if(SUCCEEDED(hr))
                    {
                        pINullFilter->DisableIKSPropertySet();

                        hr = this->AddFilterByCLSID(CLSID_DVRProgramStreamMux, TEXT("PSMux filter"), &pIBaseFilter_PSMux);
                        if(SUCCEEDED(hr))
                        {
                            // if this is a vob file, then use the pull2push filter,
                            // otherwise it's a transport stream file, so use the pull2pushts filter.
                            bool UseTs = true;
                            const wchar_t* pExt = wcsrchr(pszSourceFileName, L'.');
                            if ( pExt && 0 == wcsicmp(pExt, L".vob") )
                            {
                                UseTs = false;
                            }

                            hr = this->AddFilterByCLSID(
                                UseTs ? CLSID_MorphFilter : CLSID_PullToPushFilter,
                                TEXT("Pull2Push"),
                                &pIBaseFilter_Pull2Push
                                );
                            if(FAILED(hr))
                            {
                                LogText(__LINE__, pszFnName, TEXT("Failed to add the pull to push filter."));
                            }
                        } else LogText(__LINE__, pszFnName, TEXT("Failed to add the PS Mux filter."));
                    } else LogText(__LINE__, pszFnName, TEXT("Failed to retrieve the INullFilter interface"));
                } else LogText(__LINE__, pszFnName, TEXT("Failed to add the null renderer filter."));
            } else LogText(__LINE__, pszFnName, TEXT("Failed to add the infinite pin tee filter."));
        } else LogText(__LINE__, pszFnName, TEXT("Failed to add the MPEG 2 demultiplexer."));
    } 
    else if(FAILED(hr))
        LogText(__LINE__, pszFnName, TEXT("Failed to add the video source filter."));

    if(SUCCEEDED(hr) && ((DVRGraphLayout & SinkFileSource) || (DVRGraphLayout & SinkASyncFileSource)))
    {
        // file based source selected by the user, set the file name to use
        hr = pIBaseFilter_Source->QueryInterface(&pIFileSourceFilter);

        if(SUCCEEDED(hr))
        {
            hr = pIFileSourceFilter->Load(pszSourceFileName, NULL);
            if(FAILED(hr))
                 LogText(__LINE__, pszFnName, TEXT("Failed to load the source file for testing %s."), pszSourceFileName);

        } else LogText(__LINE__, pszFnName, TEXT("Failed to query for the IFileSource interface."));
    }

    if(SUCCEEDED(hr) && ((DVRGraphLayout & SinkUserSource)))
    {
        hr = this->AddFilterByCLSID(CLSID_DVRProgramStreamMux, TEXT("PSMux filter"), &pIBaseFilter_PSMux);

        if(FAILED(hr))
            LogText(__LINE__, pszFnName, TEXT("Failed to add the PSMux filter."));
    }

    // if the user selected using the tuner, then don't put in the grabber filter
    if(TRUE == bUseGrabber)
    {
        if(SUCCEEDED(hr))
        {
            hr = this->AddFilterByCLSID(CLSID_myGrabberSample, TEXT("Grabber Filter"), &pIBaseFilter_Grabber);

            if(SUCCEEDED(hr))
            {
                hr = pIBaseFilter_Grabber->QueryInterface(&pIGrabberSample);
            
                if(SUCCEEDED(hr))
                {
                    CMediaType mediaType;
                    mediaType.SetType(&MEDIATYPE_Stream);
                    mediaType.SetSubtype(&MEDIASUBTYPE_MPEG2_PROGRAM);

                    hr = pIGrabberSample->SetAcceptedMediaType(&mediaType);
                    if(FAILED(hr))
                         LogText(__LINE__, pszFnName, TEXT("Failed to set the accepted media type for the grabber."));
                } else LogText(__LINE__, pszFnName, TEXT("Failed to query for theIGrabber interface."));
            } else LogText(__LINE__, pszFnName, TEXT("Failed add the grabber filter."));
        }
    }


    // now that we have the upper portion of the graph added, put in the DVR filter
    if(SUCCEEDED(hr))
    {
        if(DVRGraphLayout & SinkMPEGGraph)
        {
            hr = this->AddFilterByCLSID(CLSID_DVRSinkFilterMPEG, TEXT("DVR MPEG Branch Sink Filter"), &pIBaseFilter_dvr_sink);
            if(FAILED(hr))
                 LogText(__LINE__, pszFnName, TEXT("Failed to add the MPEG branch sink filter."));
        }
        else if(DVRGraphLayout & SinkASFGraph)
        {
            hr = this->AddFilterByCLSID(CLSID_DVRSinkFilter, TEXT("DVR Sink Filter"), &pIBaseFilter_dvr_sink);
            if(FAILED(hr))
                 LogText(__LINE__, pszFnName, TEXT("Failed to add the ASF dvr sink filter."));
        }
    }

    if(SUCCEEDED(hr) && ((DVRGraphLayout & SinkTunerSource) || (DVRGraphLayout & SinkFileSource)))
    {
        if(TRUE == bUseGrabber)
        {
            hr = this->ConnectFilters(pIBaseFilter_Source, pIBaseFilter_Grabber);

            if(SUCCEEDED(hr))
            {
                hr = this->ConnectFilters(pIBaseFilter_Grabber, pIBaseFilter_dvr_sink);
                if(FAILED(hr))
                     LogText(__LINE__, pszFnName, TEXT("Failed to connect the grabber filter to the sink."));
            } else LogText(__LINE__, pszFnName, TEXT("Failed to connect the source filter to the grabber filter."));
        }
        else
        {
            hr = this->ConnectFilters(pIBaseFilter_Source, pIBaseFilter_dvr_sink);
            if(FAILED(hr))
                 LogText(__LINE__, pszFnName, TEXT("Failed to connect the source filter to the sink."));
        }
    }
    else if(SUCCEEDED(hr) && (DVRGraphLayout & SinkASyncFileSource))
    {
        hr = this->ConnectFilters(pIBaseFilter_Source, pIBaseFilter_Pull2Push);

        if(SUCCEEDED(hr))
        {
            hr = this->ConnectFilters(pIBaseFilter_Pull2Push, pIBaseFilter_MPEG2Demux);

            if(SUCCEEDED(hr))
            {
                // create the demux output pins
                CComPtr<IMpeg2Demultiplexer> pIMPEG2Demux;

                hr = pIBaseFilter_MPEG2Demux.QueryInterface(&pIMPEG2Demux);
                if(SUCCEEDED(hr))
                {
                    AM_MEDIA_TYPE mt;
                    CComPtr<IPin> pVidPin = NULL;
                    CComPtr<IPin> pAudPin = NULL;

                    ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
                    mt.majortype = MEDIATYPE_Video;
                    mt.subtype = MEDIASUBTYPE_MPEG2_VIDEO;

                    hr = pIMPEG2Demux->CreateOutputPin(&mt, TEXT("Video"), &pVidPin);
                    if(SUCCEEDED(hr))
                    {
                        // audio isn't necessary, so if no audio stream id is set, then skip creating the pin
                        if(m_AudioStreamPID > 0)
                        {
                            mt.majortype = MEDIATYPE_Audio;
                            mt.subtype = m_AudioSubType;

                            hr = pIMPEG2Demux->CreateOutputPin(&mt, TEXT("Audio"), &pAudPin);                    
                            if(FAILED(hr))
                                 LogText(__LINE__, pszFnName, TEXT("Failed to create the MPEG demux audio output pin."));
                        }
                    } else LogText(__LINE__, pszFnName, TEXT("Failed to create the MPEG demux video output pin."));

                    if(SUCCEEDED(hr))
                    {
                        if(pVidPin)
                        {
                            // If this is a transport stream file then the IMPEG2PIDMap interface will be available,
                            // if it's a vob file then IMPEG2StreamIdMap will be available.

                            CComPtr<IMPEG2PIDMap> pVidPinPIDMap;
                            CComPtr<IMPEG2StreamIdMap> pVidPinStreamIdMap;

                            if(SUCCEEDED(hr = pVidPin->QueryInterface(&pVidPinPIDMap)))
                            {
                                // it's a transport stream file, so map the PID accordingly
                                hr = pVidPinPIDMap->MapPID(1, &m_VideoStreamPID, MEDIA_ELEMENTARY_STREAM);
                                if(FAILED(hr))
                                     LogText(__LINE__, pszFnName, TEXT("Failed to map the video stream pid."));
                            }
                            else if(SUCCEEDED(hr = pVidPin->QueryInterface(&pVidPinStreamIdMap)))
                            {
                                // it's a vob file, so map the stream ID
                                hr = pVidPinStreamIdMap->MapStreamId(
                                    m_VideoStreamPID,
                                    MEDIA_ELEMENTARY_STREAM,
                                    0x10000000,
                                    0
                                    );
                                if(FAILED(hr))
                                    LogText(__LINE__, pszFnName, TEXT("Failed to map the video stream id."));
                            }
                            else
                                LogText(__LINE__, pszFnName, TEXT("Failed to find the interfaces for mapping the video, is this an invalid file?"));
                        }

                        if(SUCCEEDED(hr) && pAudPin)
                        {
                            CComPtr<IMPEG2PIDMap> pAudPinPIDMap;
                            CComPtr<IMPEG2StreamIdMap> pAudPinStreamIdMap;

                            if(SUCCEEDED(hr = pAudPin->QueryInterface(&pAudPinPIDMap)))
                            {
                                hr = pAudPinPIDMap->MapPID(1, &m_AudioStreamPID, MEDIA_ELEMENTARY_STREAM);
                                if(FAILED(hr))
                                     LogText(__LINE__, pszFnName, TEXT("Failed to map the audio stream pid."));
                            }
                            else if(SUCCEEDED(hr= pAudPin->QueryInterface(&pAudPinStreamIdMap)))
                            {
                                hr = pAudPinStreamIdMap->MapStreamId(
                                    m_AudioStreamPID,
                                    MEDIA_ELEMENTARY_STREAM,
                                    0x80,
                                    4
                                    );
                                if(FAILED(hr))
                                    LogText(__LINE__, pszFnName, TEXT("Failed to map the audio stream id."));
                            }
                            else
                                LogText(__LINE__, pszFnName, TEXT("Failed to find the interfaces for mapping the audio, is this an invalid file?"));
                        }
                    }
                } else LogText(__LINE__, pszFnName, TEXT("Failed to query for the IMpeg2Demultiplexer interface."));


                hr = this->ConnectFilters(pIBaseFilter_MPEG2Demux, pIBaseFilter_PSMux);
                if(SUCCEEDED(hr))
                {
                    if(TRUE == bUseGrabber)
                    {
                        hr = this->ConnectFilters(pIBaseFilter_PSMux, pIBaseFilter_Grabber);

                        if(SUCCEEDED(hr))
                        {
                            hr = this->ConnectFilters(pIBaseFilter_Grabber, pIBaseFilter_dvr_sink);
                            if(FAILED(hr))
                                 LogText(__LINE__, pszFnName, TEXT("Failed to connect the Grabber filter to the dvr sink."));
                        } else LogText(__LINE__, pszFnName, TEXT("Failed to connect the PS mux to the grabber filter."));
                    }
                    else
                    {
                        hr = this->ConnectFilters(pIBaseFilter_PSMux, pIBaseFilter_dvr_sink);
                        if(FAILED(hr))
                             LogText(__LINE__, pszFnName, TEXT("Failed to connect the PS mux ot the dvr sink."));
                    }

                    if(SUCCEEDED(hr))
                    {
                        hr = this->InsertFilter(pIBaseFilter_MPEG2Demux, pIBaseFilter_TeeFilter, MEDIATYPE_Video, TRUE);
                        if(SUCCEEDED(hr))
                        {
                            hr = this->ConnectFilters(pIBaseFilter_TeeFilter, pIBaseFilter_NullRenderer);
                            if(FAILED(hr))
                                 LogText(__LINE__, pszFnName, TEXT("Failed to connect the tee filter to the null renderer"));
                        } else LogText(__LINE__, pszFnName, TEXT("Failed to insert the tee filter."));
                    }

                } else LogText(__LINE__, pszFnName, TEXT("Failed to connect the MPEG2 Demux to the PS mux."));
            } else LogText(__LINE__, pszFnName, TEXT("Failed to connect the Pull to Push filter to the demux."));
        } else LogText(__LINE__, pszFnName, TEXT("Failed to connect the source filter to the Pull to Push filter."));
    }
    else if(SUCCEEDED(hr) && (DVRGraphLayout & SinkUserSource))
    {
        hr = this->ConnectFilters(pIBaseFilter_Source, pIBaseFilter_PSMux);
        if(SUCCEEDED(hr))
        {
            if(TRUE == bUseGrabber)
            {

                hr = this->ConnectFilters(pIBaseFilter_PSMux, pIBaseFilter_Grabber);

                if(SUCCEEDED(hr))
                {
                    hr = this->ConnectFilters(pIBaseFilter_Grabber, pIBaseFilter_dvr_sink);
                    if(FAILED(hr))
                         LogText(__LINE__, pszFnName, TEXT("Failed to connect the grabber filter to the sink."));
                } else LogText(__LINE__, pszFnName, TEXT("Failed to connect the PSMux filter to the grabber filter."));
            }
            else
            {
                hr = this->ConnectFilters(pIBaseFilter_PSMux, pIBaseFilter_dvr_sink);
                if(FAILED(hr))
                     LogText(__LINE__, pszFnName, TEXT("Failed to connect the PSMux to the sink."));
            }
        } else LogText(__LINE__, pszFnName, TEXT("Failed to connect the source filter to the PSMux filter."));
    }

    DisplayFilters();

    return hr;
}

HRESULT CSinkFilterGraph::Tune(TUNE_MODE tuneMode, long lTargetChannel, long lVideoSubChannel, long lAudioSubChannel)
{
    CComPtr<IAMTVTuner> pTvTuner;
    long lChannelMin;
    long lChannelMax;
    long lChannel;

    HRESULT hr = this->FindInterface(IID_IAMTVTuner, (void **) &pTvTuner);

    if(SUCCEEDED(hr))
    {
        switch(tuneMode)
        {
        case TUNE_TO:
            lChannel = lTargetChannel;
            break;
        default:
            hr = pTvTuner->ChannelMinMax(&lChannelMin, &lChannelMax);

            if(FAILED(hr))
            {
                LogHRError(hr, TEXT("pTvTuner->ChannelMinMax failed"));
            }
            else
            {
                hr = pTvTuner->get_Channel(&lChannel, &lVideoSubChannel, &lAudioSubChannel);

                if(FAILED(hr))
                {
                    LogHRError(hr, TEXT("pTvTuner->get_Channel failed"));
                }
                else
                {
                    switch(tuneMode)
                    {
                    case TUNE_UP:
                        if(lChannel == lChannelMax)
                        {
                            lChannel = lChannelMin;
                        }
                        else
                        {
                            lChannel++;
                        }
                        break;
                    case TUNE_DOWN:
                        if(lChannel == lChannelMin)
                        {
                            lChannel = lChannelMax;
                        }
                        else
                        {
                            lChannel--;
                        }
                    }
                }
            }
        }

        if(SUCCEEDED(hr))
        {
            hr = pTvTuner->put_Channel(lChannel, lVideoSubChannel, lAudioSubChannel);
        }
    }

    return hr;
}

HRESULT CSinkFilterGraph::TuneUp()
{
    return Tune(TUNE_UP, 0, AMTUNER_SUBCHAN_DEFAULT, AMTUNER_SUBCHAN_DEFAULT);
}

HRESULT CSinkFilterGraph::TuneDown()
{
    return Tune(TUNE_DOWN, 0, AMTUNER_SUBCHAN_DEFAULT, AMTUNER_SUBCHAN_DEFAULT);
}

HRESULT CSinkFilterGraph::TuneTo(long lTargetChannel, long lVideoSubChannel, long lAudioSubChannel)
{
    return Tune(TUNE_TO, lTargetChannel, lVideoSubChannel, lAudioSubChannel);
}

//typedef enum {
//    TunerInputCable,
//    TunerInputAntenna
//} TunerInputType;
HRESULT get_InputType(long lIndex, TunerInputType *pInputType);
HRESULT put_InputType(long lIndex, TunerInputType InputType);
HRESULT get_ConnectInput(long *plIndex);
HRESULT put_ConnectInput(long lIndex);

// IAMCrossbar
HRESULT CSinkFilterGraph::CanRoute(long outputPinIndex, long inputPinIndex)
{
    CComPtr<IAMCrossbar> pIAMCrossbar;

    HRESULT hr = this->FindInterface(IID_IAMCrossbar, (void **) &pIAMCrossbar);

    if(SUCCEEDED(hr))
    {
        hr = pIAMCrossbar->CanRoute(outputPinIndex, inputPinIndex);
    }

    return hr;
}

HRESULT CSinkFilterGraph::get_CrossbarPinInfo(BOOL bIsInputPin, long pinIndex, long *pPinIndexRelated, long *pPhysicalType)
{
    CComPtr<IAMCrossbar> pIAMCrossbar;

    HRESULT hr = this->FindInterface(IID_IAMCrossbar, (void **) &pIAMCrossbar);

    if(SUCCEEDED(hr))
    {
        hr = pIAMCrossbar->get_CrossbarPinInfo(bIsInputPin, pinIndex, pPinIndexRelated, pPhysicalType);
    }

    return hr;
}

HRESULT CSinkFilterGraph::get_IsRoutedTo(long outputPinIndex, long *pInputPinIndex)
{
    CComPtr<IAMCrossbar> pIAMCrossbar;

    HRESULT hr = this->FindInterface(IID_IAMCrossbar, (void **) &pIAMCrossbar);

    if(SUCCEEDED(hr))
    {
        hr = pIAMCrossbar->get_IsRoutedTo(outputPinIndex, pInputPinIndex);
    }

    return hr;
}

HRESULT CSinkFilterGraph::get_PinCounts(long *pOutputPinCount, long *pInputPinCount)
{
    CComPtr<IAMCrossbar> pIAMCrossbar;

    HRESULT hr = this->FindInterface(IID_IAMCrossbar, (void **) &pIAMCrossbar);

    if(SUCCEEDED(hr))
    {
        hr = pIAMCrossbar->get_PinCounts(pOutputPinCount, pInputPinCount);
    }

    return hr;
}

HRESULT CSinkFilterGraph::Route(long outputPinIndex, long inputPinIndex)
{
    CComPtr<IAMCrossbar> pIAMCrossbar;

    HRESULT hr = this->FindInterface(IID_IAMCrossbar, (void **) &pIAMCrossbar);

    if(SUCCEEDED(hr))
    {
        hr = pIAMCrossbar->Route(outputPinIndex, inputPinIndex);
    }

    return hr;
}


//typedef enum
//{
//    PhysConn_Video_Tuner = 1,
//    PhysConn_Video_Composite,
//    PhysConn_Video_SVideo,
//    PhysConn_Video_RGB,
//    PhysConn_Video_YRYBY,
//    PhysConn_Video_SerialDigital,
//    PhysConn_Video_ParallelDigital,
//    PhysConn_Video_SCSI,
//    PhysConn_Video_AUX,
//    PhysConn_Video_1394,
//    PhysConn_Video_USB,
//    PhysConn_Video_VideoDecoder,
//    PhysConn_Video_VideoEncoder,
//    PhysConn_Video_SCART,
//
//    PhysConn_Audio_Tuner = 4096,
//    PhysConn_Audio_Line,
//    PhysConn_Audio_Mic,
//    PhysConn_Audio_AESDigital,
//    PhysConn_Audio_SPDIFDigital,
//    PhysConn_Audio_SCSI,
//    PhysConn_Audio_AUX,
//    PhysConn_Audio_1394,
//    PhysConn_Audio_USB,
//    PhysConn_Audio_AudioDecoder,
//} PhysicalConnectorType;

long CSinkFilterGraph::GetCrossbarPinIndex(PhysicalConnectorType physicalConnectorType, BOOL bInputPin)
{
    HRESULT hr = E_FAIL;

    long outputPinCount;
    long inputPinCount;
    long pinIndexRelated;
    long physicalType;
    long index = -1;
    long pinCount;

    hr = get_PinCounts(&outputPinCount, &inputPinCount);

    if(SUCCEEDED(hr))
    {
        if(bInputPin)
        {
            pinCount = inputPinCount;
        }
        else
        {
            pinCount = outputPinCount;
        }

        for(long pinIndex=0; pinIndex<pinCount; pinIndex++)
        {
            hr = get_CrossbarPinInfo(bInputPin, pinIndex, &pinIndexRelated, &physicalType);

            if(SUCCEEDED(hr))
            {
                if(physicalType == physicalConnectorType)
                {
                    index = pinIndex;
                    break;
                }
            }
            else
            {
                break;
            }
        }
    }

    return index;
}

HRESULT CSinkFilterGraph::SelectInput(PhysicalConnectorType physicalVideoConnectorType, PhysicalConnectorType physicalAudioConnectorType)
{
    HRESULT hrVideo = E_FAIL;
    HRESULT hrAudio = E_FAIL;

    long videoInputPinIndex  = GetCrossbarPinIndex(physicalVideoConnectorType, TRUE);
    long videoOutputPinIndex = GetCrossbarPinIndex(PhysConn_Video_VideoEncoder, FALSE);;
    long audioInputPinIndex  = GetCrossbarPinIndex(physicalAudioConnectorType, TRUE);
    long audioOutputPinIndex = GetCrossbarPinIndex(PhysConn_Audio_AudioDecoder, FALSE);;

    if( (-1 != videoInputPinIndex) && (-1 != videoOutputPinIndex))
    {
        hrVideo = CanRoute(videoOutputPinIndex, videoInputPinIndex);

        if(hrVideo == S_OK)
        {
            hrVideo = Route(videoOutputPinIndex, videoInputPinIndex);
        }
    }

    if( (-1 != audioInputPinIndex) && (-1 != audioOutputPinIndex))
    {
        hrAudio = CanRoute(audioOutputPinIndex, audioInputPinIndex);

        if(hrAudio == S_OK)
        {
            hrAudio = Route(audioOutputPinIndex, audioInputPinIndex);
        }
    }

    if(FAILED(hrVideo))
    {
        return LogHRError(hrVideo, TEXT("CSinkFilterGraph::SelectInput failed; Video Failed"));
    }
    else
    {
        return LogHRError(hrAudio, TEXT("CSinkFilterGraph::SelectInput failed; Video OK; Audio Failed"));
    }

}

HRESULT CSinkFilterGraph::SelectInput(INPUT_TYPE inputType)
{
    HRESULT hr = E_FAIL;

    switch(inputType)
    {
    case INPUT_TYPE_SVIDEO:
        hr = SelectInput(PhysConn_Video_SVideo, PhysConn_Audio_Line);
        break;
    case INPUT_TYPE_COMPOSITE_VIDEO:
        hr = SelectInput(PhysConn_Video_Composite, PhysConn_Audio_Line);
        break;
    case INPUT_TYPE_TUNER:
        hr = SelectInput(PhysConn_Video_Tuner, PhysConn_Audio_Tuner);
        break;
    default:
        hr = E_INVALIDARG;
        break;
    }

    return hr;
}

//IAMVideoCompression
HRESULT CSinkFilterGraph::GetVideoCompressionInfo(WCHAR *pszVersion, int *pcbVersion, LPWSTR pszDescription, int *pcbDescription, long *pDefaultKeyFrameRate, long *pDefaultPFramesPerKey, double *pDefaultQuality, long *pCapabilities)
{
    CComPtr<IAMVideoCompression> pIAMVideoCompression;

    HRESULT hr = this->FindInterface(IID_IAMVideoCompression, (void **) &pIAMVideoCompression);

    if(SUCCEEDED(hr))
    {
        hr = pIAMVideoCompression->GetInfo(pszVersion, pcbVersion, pszDescription, pcbDescription, pDefaultKeyFrameRate, pDefaultPFramesPerKey, pDefaultQuality, pCapabilities);
    }

    return hr;
}

HRESULT CSinkFilterGraph::put_KeyFrameRate(long keyFrameRate)
{
    CComPtr<IAMVideoCompression> pIAMVideoCompression;

    HRESULT hr = this->FindInterface(IID_IAMVideoCompression, (void **) &pIAMVideoCompression);

    if(SUCCEEDED(hr))
    {
        hr = pIAMVideoCompression->put_KeyFrameRate(keyFrameRate);
    }

    return hr;
}
HRESULT CSinkFilterGraph::get_KeyFrameRate(long *pKeyFrameRate)
{
    CComPtr<IAMVideoCompression> pIAMVideoCompression;

    HRESULT hr = this->FindInterface(IID_IAMVideoCompression, (void **) &pIAMVideoCompression);

    if(SUCCEEDED(hr))
    {
        hr = pIAMVideoCompression->get_KeyFrameRate(pKeyFrameRate);
    }

    return hr;
}

HRESULT CSinkFilterGraph::put_PFramesPerKeyFrame(long lPFramesPerKeyFrame)
{
    CComPtr<IAMVideoCompression> pIAMVideoCompression;

    HRESULT hr = this->FindInterface(IID_IAMVideoCompression, (void **) &pIAMVideoCompression);

    if(SUCCEEDED(hr))
    {
        hr = pIAMVideoCompression->put_PFramesPerKeyFrame(lPFramesPerKeyFrame);
    }

    return hr;
}

HRESULT CSinkFilterGraph::get_PFramesPerKeyFrame(long *pPFramesPerKeyFrame)
{
    CComPtr<IAMVideoCompression> pIAMVideoCompression;

    HRESULT hr = this->FindInterface(IID_IAMVideoCompression, (void **) &pIAMVideoCompression);

    if(SUCCEEDED(hr))
    {
        hr = pIAMVideoCompression->get_PFramesPerKeyFrame(pPFramesPerKeyFrame);
    }

    return hr;
}

HRESULT CSinkFilterGraph::put_Quality(double Quality)
{
    CComPtr<IAMVideoCompression> pIAMVideoCompression;

    HRESULT hr = this->FindInterface(IID_IAMVideoCompression, (void **) &pIAMVideoCompression);

    if(SUCCEEDED(hr))
    {
        hr = pIAMVideoCompression->put_Quality(Quality);
    }

    return hr;
}

HRESULT CSinkFilterGraph::get_Quality(double *pQuality)
{
    CComPtr<IAMVideoCompression> pIAMVideoCompression;

    HRESULT hr = this->FindInterface(IID_IAMVideoCompression, (void **) &pIAMVideoCompression);

    if(SUCCEEDED(hr))
    {
        hr = pIAMVideoCompression->get_Quality(pQuality);
    }

    return hr;
}

HRESULT CSinkFilterGraph::put_WindowSize(DWORDLONG WindowSize)
{
    CComPtr<IAMVideoCompression> pIAMVideoCompression;

    HRESULT hr = this->FindInterface(IID_IAMVideoCompression, (void **) &pIAMVideoCompression);

    if(SUCCEEDED(hr))
    {
        hr = pIAMVideoCompression->put_WindowSize(WindowSize);
    }

    return hr;
}

HRESULT CSinkFilterGraph::get_WindowSize(DWORDLONG *pWindowSize)
{
    CComPtr<IAMVideoCompression> pIAMVideoCompression;

    HRESULT hr = this->FindInterface(IID_IAMVideoCompression, (void **) &pIAMVideoCompression);

    if(SUCCEEDED(hr))
    {
        hr = pIAMVideoCompression->get_WindowSize(pWindowSize);
    }

    return hr;
}

HRESULT CSinkFilterGraph::OverrideKeyFrame(long FrameNumber)
{
    CComPtr<IAMVideoCompression> pIAMVideoCompression;

    HRESULT hr = this->FindInterface(IID_IAMVideoCompression, (void **) &pIAMVideoCompression);

    if(SUCCEEDED(hr))
    {
        hr = pIAMVideoCompression->OverrideKeyFrame(FrameNumber);
    }

    return hr;
}

HRESULT CSinkFilterGraph::OverrideFrameSize(long FrameNumber, long Size)
{
    CComPtr<IAMVideoCompression> pIAMVideoCompression;

    HRESULT hr = this->FindInterface(IID_IAMVideoCompression, (void **) &pIAMVideoCompression);

    if(SUCCEEDED(hr))
    {
        hr = pIAMVideoCompression->OverrideFrameSize(FrameNumber, Size);
    }

    return hr;
}

//typedef enum tagTVAudioMode {
//    AMTVAUDIO_MODE_MONO   = 0x0001,
//    AMTVAUDIO_MODE_STEREO = 0x0002,
//    AMTVAUDIO_MODE_LANG_A = 0x0010,
//    AMTVAUDIO_MODE_LANG_B = 0x0020,
//    AMTVAUDIO_MODE_LANG_C = 0x0040,
//} TVAudioMode;

#ifndef _WIN32_WCE
//IAMTVAudio
HRESULT CSinkFilterGraph::get_TVAudioMode(long *plModes)
{
    CComPtr<IAMTVAudio> pIAMTVAudio;

    HRESULT hr = this->FindInterface(IID_IAMTVAudio, (void **) &pIAMTVAudio);

    if(SUCCEEDED(hr))
    {
        hr = pIAMTVAudio->get_TVAudioMode(plModes);
    }

    return hr;
}

HRESULT CSinkFilterGraph::GetAvailableTVAudioModes(long *plModes)
{
    CComPtr<IAMTVAudio> pIAMTVAudio;

    HRESULT hr = this->FindInterface(IID_IAMTVAudio, (void **) &pIAMTVAudio);

    if(SUCCEEDED(hr))
    {
        hr = pIAMTVAudio->GetAvailableTVAudioModes(plModes);
    }

    return hr;
}

HRESULT CSinkFilterGraph::GetHardwareSupportedTVAudioModes(long *plModes)
{
    CComPtr<IAMTVAudio> pIAMTVAudio;

    HRESULT hr = this->FindInterface(IID_IAMTVAudio, (void **) &pIAMTVAudio);

    if(SUCCEEDED(hr))
    {
        hr = pIAMTVAudio->GetHardwareSupportedTVAudioModes(plModes);
    }

    return hr;
}

HRESULT CSinkFilterGraph::put_TVAudioMode(long plModes)
{
    CComPtr<IAMTVAudio> pIAMTVAudio;

    HRESULT hr = this->FindInterface(IID_IAMTVAudio, (void **) &pIAMTVAudio);

    if(SUCCEEDED(hr))
    {
        hr = pIAMTVAudio->put_TVAudioMode(plModes);
    }

    return hr;
}
#endif

HRESULT CSinkFilterGraph::GetCaptureMode(STRMBUF_CAPTURE_MODE *pMode, LONGLONG * pllMaxBufferMilliseconds)
{
    CComPtr<IStreamBufferCapture> pIStreamBufferCapture;

    HRESULT hr = this->FindInterface(__uuidof(IStreamBufferCapture), (void **) &pIStreamBufferCapture);

    if(SUCCEEDED(hr))
    {
        hr = pIStreamBufferCapture->GetCaptureMode(pMode, pllMaxBufferMilliseconds);
    }

    return hr;
}

HRESULT CSinkFilterGraph::BeginTemporaryRecording(LONGLONG llBufferSizeInMilliSeconds)
{
    CComPtr<IStreamBufferCapture> pIStreamBufferCapture;

    HRESULT hr = this->FindInterface(__uuidof(IStreamBufferCapture), (void **) &pIStreamBufferCapture);

    if(SUCCEEDED(hr))
    {
        hr = pIStreamBufferCapture->BeginTemporaryRecording(llBufferSizeInMilliSeconds);
    }

    return hr;
}

HRESULT CSinkFilterGraph::BeginPermanentRecording(LONGLONG llBufferSizeInMilliSeconds)
{
    CComPtr<IStreamBufferCapture> pIStreamBufferCapture;

    HRESULT hr = this->FindInterface(__uuidof(IStreamBufferCapture), (void **) &pIStreamBufferCapture);

    if(SUCCEEDED(hr))
    {
        LONGLONG llTemp;
        hr = pIStreamBufferCapture->BeginPermanentRecording(llBufferSizeInMilliSeconds, &llTemp);
    }

    return hr;
}

HRESULT CSinkFilterGraph::ConvertToTemporaryRecording(LPCOLESTR pszFileName)
{
    CComPtr<IStreamBufferCapture> pIStreamBufferCapture;

    HRESULT hr = this->FindInterface(__uuidof(IStreamBufferCapture), (void **) &pIStreamBufferCapture);

    if(SUCCEEDED(hr))
    {
        hr = pIStreamBufferCapture->ConvertToTemporaryRecording(pszFileName);
    }

    return hr;
}

HRESULT CSinkFilterGraph::SetRecordingPath()
{
    return SetRecordingPath(m_pszRecordingPath);
}

HRESULT CSinkFilterGraph::SetRecordingPath(LPCOLESTR pszPath)
{
    CComPtr<IStreamBufferCapture> pIStreamBufferCapture;

    HRESULT hr = this->FindInterface(__uuidof(IStreamBufferCapture), (void **) &pIStreamBufferCapture);

    if(SUCCEEDED(hr))
    {
        hr = pIStreamBufferCapture->SetRecordingPath(pszPath);
    }

    return hr;
}

HRESULT CSinkFilterGraph::GetRecordingPath(LPOLESTR *ppszPath)
{
    CComPtr<IStreamBufferCapture> pIStreamBufferCapture;

    HRESULT hr = this->FindInterface(__uuidof(IStreamBufferCapture), (void **) &pIStreamBufferCapture);

    if(SUCCEEDED(hr))
    {
        hr = pIStreamBufferCapture->GetRecordingPath(ppszPath);
    }

    return hr;
}


HRESULT  CSinkFilterGraph::GetDefaultRecordingPath(TCHAR *ppszPath)
{
    _tcscpy(ppszPath, m_pszRecordingPath);
    return S_OK;
}


HRESULT CSinkFilterGraph::GetBoundToLiveToken(LPOLESTR *ppszToken)
{
    CComPtr<IStreamBufferCapture> pIStreamBufferCapture;

    HRESULT hr = this->FindInterface(__uuidof(IStreamBufferCapture), (void **) &pIStreamBufferCapture);

    if(SUCCEEDED(hr))
    {
        hr = pIStreamBufferCapture->GetBoundToLiveToken(ppszToken);
    }

    return hr;
}

HRESULT CSinkFilterGraph::GetCurrentPosition(LONGLONG  *phyCurrentPosition)
{
    CComPtr<IStreamBufferCapture> pIStreamBufferCapture;

    HRESULT hr = this->FindInterface(__uuidof(IStreamBufferCapture), (void **) &pIStreamBufferCapture);

    if(SUCCEEDED(hr))
    {
        hr = pIStreamBufferCapture->GetCurrentPosition(phyCurrentPosition);
    }

    return hr;
}

HRESULT CSinkFilterGraph::SetFileName(LPCOLESTR pszFileName, const AM_MEDIA_TYPE *pmt)
{
    CComPtr<IFileSinkFilter> pIFileSinkFilter;

    HRESULT hr = this->FindInterface(IID_IFileSinkFilter, (void **) &pIFileSinkFilter);

    if(SUCCEEDED(hr))
    {
        hr = pIFileSinkFilter->SetFileName(pszFileName, pmt);
    }

    return hr;
}

HRESULT CSinkFilterGraph::GetCurFile(LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt )
{
    CComPtr<IFileSinkFilter> pIFileSinkFilter;

    HRESULT hr = this->FindInterface(IID_IFileSinkFilter, (void **) &pIFileSinkFilter);

    if(SUCCEEDED(hr))
    {
        hr = pIFileSinkFilter->GetCurFile(ppszFileName, pmt);
    }

    return hr;
}

HRESULT CSinkFilterGraph::DeleteRecording(LPCOLESTR pszFileName)
{
    CComPtr<IDVREngineHelpers> pDVRHelper;

    HRESULT hr = this->FindInterface(__uuidof(IDVREngineHelpers), (void **) &pDVRHelper);
    
    if(SUCCEEDED(hr))
    {
        hr = pDVRHelper->DeleteRecording(pszFileName);
    }
    
    return hr;
}

HRESULT CSinkFilterGraph::CleanupOrphanedRecordings(LPCOLESTR pszDirName)
{
    CComPtr<IDVREngineHelpers> pDVRHelper;

    HRESULT hr = this->FindInterface(__uuidof(IDVREngineHelpers), (void **) &pDVRHelper);
    
    if(SUCCEEDED(hr))
    {
        hr = pDVRHelper->CleanupOrphanedRecordings(pszDirName);
    }

    return hr;
}

// Macrovision
HRESULT CSinkFilterGraph::SetMacrovisionLevel(AM_COPY_MACROVISION_LEVEL macrovisionLevel)
{
    AM_COPY_MACROVISION amCopyMacrovision;
    CComPtr<IKsPropertySet> pIKsPropertySet = NULL;
    CComPtr<IBaseFilter> pIBaseFilter_dvr_sink;
    CComPtr<IPin> pIPinInput;

    HRESULT hr = FindFilter(TEXT("DVR MPEG Branch Sink Filter"), &pIBaseFilter_dvr_sink);

    if(FAILED(hr))
    {
        LogHRError(hr, TEXT("CSinkFilterGraph::SetMacrovisionLevel - DVR MPEG Branch Sink Filter not available"));
    }
    else
    {
        pIPinInput = GetPinByDir(pIBaseFilter_dvr_sink, PINDIR_INPUT);

        if(pIPinInput == NULL)
        {
            hr = E_FAIL;
            LogHRError(hr, TEXT("CSinkFilterGraph::SetMacrovisionLevel - DVR MPEG Branch Sink Filter input pin not available"));
        }
        else
        {
            hr = pIPinInput->QueryInterface(&pIKsPropertySet);

            if(SUCCEEDED(hr))
            {
                amCopyMacrovision.MACROVISIONLevel = macrovisionLevel;

                hr = pIKsPropertySet->Set(AM_KSPROPSETID_CopyProt, AM_PROPERTY_COPY_MACROVISION, NULL, 0, &amCopyMacrovision, sizeof(AM_COPY_MACROVISION));

                if(FAILED(hr))
                {
                    LogHRError(hr, TEXT("CSinkFilterGraph::SetMacrovisionLevel - pIKsPropertySet->Set failed"));
                }
            }
            else
            {
                LogHRError(hr, TEXT("CSinkFilterGraph::SetMacrovisionLevel - IKsPropertySet not available"));
            }
        }
    }

    return hr;
}


HRESULT CSinkFilterGraph::SetEncryptionEnabled(BOOL fEncrypt)
{
#if 0
    CComPtr<IContentRestrictionCapture> pIContentRestrictionCapture;

    HRESULT hr = this->FindInterface(__uuidof(IContentRestrictionCapture), (void **) &pIContentRestrictionCapture);

    if(SUCCEEDED(hr))
    {
        hr = pIContentRestrictionCapture->SetEncryptionEnabled(fEncrypt);

        if(FAILED(hr))
        {
            LogHRError(hr, TEXT("CSinkFilterGraph::SetEncryptionEnabled - pIContentRestrictionCapture->SetEncryptionEnabled failed"));
        }
    }
    else
    {
            LogHRError(hr, TEXT("CSinkFilterGraph::SetEncryptionEnabled - pIContentRestrictionCapture not available"));
    }

    return hr;
#else
    return E_NOTIMPL;
#endif
}

HRESULT CSinkFilterGraph::GetEncryptionEnabled(BOOL *pfEncrypt)
{
#if 0
    CComPtr<IContentRestrictionCapture> pIContentRestrictionCapture;

    HRESULT hr = this->FindInterface(__uuidof(IContentRestrictionCapture), (void **) &pIContentRestrictionCapture);

    if(SUCCEEDED(hr))
    {
        hr = pIContentRestrictionCapture->GetEncryptionEnabled(pfEncrypt);
    }

    return hr;
#else
    return E_NOTIMPL;
#endif
}

HRESULT CSinkFilterGraph::ExpireVBICopyProtection(ULONG uExpiredPolicy)
{
#if 0
    CComPtr<IContentRestrictionCapture> pIContentRestrictionCapture;

    HRESULT hr = this->FindInterface(__uuidof(IContentRestrictionCapture), (void **) &pIContentRestrictionCapture);

    if(SUCCEEDED(hr))
    {
        hr = pIContentRestrictionCapture->ExpireVBICopyProtection(uExpiredPolicy);

        if(FAILED(hr))
        {
            LogHRError(hr, TEXT("CSinkFilterGraph::ExpireVBICopyProtection - pIContentRestrictionCapture->ExpireVBICopyProtection failed"));
        }
    }
    else
    {
        LogHRError(hr, TEXT("CSinkFilterGraph::ExpireVBICopyProtection - pIContentRestrictionCapture not available"));
    }

    return hr;
#else
    return E_NOTIMPL;
#endif
}

