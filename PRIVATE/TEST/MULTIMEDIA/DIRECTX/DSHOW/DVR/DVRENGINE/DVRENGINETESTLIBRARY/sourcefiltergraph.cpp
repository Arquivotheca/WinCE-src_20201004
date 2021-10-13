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
#include "sourcefiltergraph.h"
#include "Helper.h"
#include <atlbase.h>
#include "logger.h"
#include "Grabber.h"
#include "qinnullrenderer.h"

#define DVRFile L"demo_0.dvr-dat"
#define SourcePath L"\\hard disk\\"

#define SOURCE_FILTERGRAPH_NAME TEXT("Source Filter Graph")

CSourceFilterGraph::CSourceFilterGraph(void)
:    bUseMediaSeeking(FALSE),
    pIMediaSeeking(NULL),
    pIStreamBufferMediaSeeking(NULL)
{
    DVRGraphLayout |= SourceMPEGGraph | SourceVideoRenderer | SourceAudioSyncFilter;

    VideoDecoderCLSID = CLSID_MediaExcel;
    _tcscpy(pszSourceVideoFile, DVRFile);
   _tcscpy(pszSourcePath, SourcePath);
}

CSourceFilterGraph::~CSourceFilterGraph(void)
{
    LogText(TEXT("CSourceFilterGraph::~CSourceFilterGraph"));
}

BOOL CSourceFilterGraph::SetCommandLine(const TCHAR * CommandLine)
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
        LogText(TEXT("A DVREngine compatible MPEG2 decoder filter is not shipped with Windows CE. In order to use"));
        LogText(TEXT("the DVR engine CETK tests you must specify the GUID of the decoder filter via the test"));
        LogText(TEXT("command line. Please use the SourceDecoder command line option to set this GUID."));
        LogText(TEXT("NOTE: if the video decoder has an internal video renderer, then please set the DecoderIsRenderer command line option."));
        LogText(TEXT("The full DVR source graph command line options follow:"));
        bReturnValue = FALSE;
    }


    if(cCLPparser.GetOpt(TEXT("?")) || !bReturnValue)
    {
        LogText(TEXT(""));
        LogText(TEXT("/? outputs this message"));
        LogText(TEXT(""));
        LogText(TEXT("/CETKTestInformation - Output usage information for the test within the CETK."));
        LogText(TEXT("/SourceDecoder <guid> - specifies the GUID of the MPEG decoder filter used to render video"));
        LogText(TEXT("/DecoderIsRenderer - the decoder filter has an integrated audio and video renderer in it."));
        LogText(TEXT("/RenderVBI - render the VBI output pin on the decoder to the VBID renderer filter."));
        LogText(TEXT("/SourceDVRFile <file> - When tests request a prerecorded DVR file, this specifies the path and file name to use. The default is hard disk\\demo_0.dvr-dat"));
        LogText(TEXT("/SourceVideoPath <path> - this is the path to the recordings made by the dvr source filter. This defaults to hard disk2."));
        LogText(TEXT(""));
        LogText(TEXT("The following option requires special NULL renderer filters to do component level testing of the dvr engine."));
        LogText(TEXT("/SourceTestGraph - specifies playback with a NULL audio and video renderer for component testing purposes"));
        LogText(TEXT(""));

        bReturnValue = FALSE;
    }
    else if(bReturnValue)
    {
         if(cCLPparser.GetOpt(TEXT("SourceTestGraph")))
        {
            DVRGraphLayout = ((DVRGraphLayout & ~(SourceMPEGGraph | SourceASFGraph | SourceVideoRenderer | SourceOverlayMixer  | SourceAudioSyncFilter)) | (SourceTestGraph | SourceNULLRenderer));
        }
        else if(cCLPparser.GetOpt(TEXT("DecoderIsRenderer")))
        {
            // our decoder is the renderer for audio and video, so clear off the renderers from the graph layout. It's still valid to render VBI if requested in this case,
            // but most likely the rendering filter will take care of that too.
            DVRGraphLayout = ((DVRGraphLayout & ~(SourceVideoRenderer | SourceOverlayMixer | SourceAudioSyncFilter)));
        }

         if(cCLPparser.GetOpt(TEXT("RenderVBI")))
        {
            DVRGraphLayout |= SourceVBIRenderer;
        }

        TCHAR *ptszCmdLineBackup;
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
                    if(0 == ::_tcsicmp(ptszToken, TEXT("SourceDecoder")))
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
                                VideoDecoderCLSID = clsid;
                            }

                            // we found what we were looking for, so we're done.
                            break;
                        }
                    }
                    ptszToken = ::_tcstok(NULL, _T(" /"));
                }

                delete []ptszCmdLineBackup;
            }
        }


        // parse out the default recording path
        if(!cCLPparser.GetOptString(TEXT("SourceVideoPath"), pszSourcePath, MAX_PATH))
        {
           // set the graph source path to the default because we didn't get one from the user
           _tcscpy(pszSourcePath, SourcePath);
        }

        if(!cCLPparser.GetOptString(TEXT("SourceDVRFile"), pszSourceVideoFile, MAX_PATH))
        {
            // set the graph source dat file to the default because we didn't get one from the user
            _tcscpy(pszSourceVideoFile, DVRFile);
        }
    }

    return bReturnValue;
}

void
CSourceFilterGraph::OutputCurrentCommandLineConfiguration()
{
    LogText(TEXT("DVRGraphLayout is 0x%08x."), DVRGraphLayout);

    // if source test graph output that
    if(DVRGraphLayout & SourceTestGraph)
        LogText(TEXT("Using the DVR test graph."));

    // if no renderers and we're not a test graph, then the decoder is the renderer
    if(!(DVRGraphLayout & (SourceVideoRenderer | SourceOverlayMixer | SourceAudioSyncFilter | SourceTestGraph)))
        LogText(TEXT("The decoder is a renderer or rendered to nonstandard renderers."));

    // if vbi is set then output it is
    if(DVRGraphLayout & SourceVBIRenderer)
        LogText(TEXT("VBI is rendered."));

    // output the source decoder guid
    LPOLESTR decoder = NULL;
    StringFromCLSID(VideoDecoderCLSID, &decoder);
    LogText(TEXT("Using video decoder %s"), decoder);
    CoTaskMemFree(decoder);
    decoder = NULL;

    // output the source video path
    LogText(TEXT("Default DVR file is at %s%s"), pszSourcePath, pszSourceVideoFile);
    
    return;
}

HRESULT CSourceFilterGraph::Initialize()
{
    HRESULT hr = CFilterGraph::Initialize();

    CFilterGraph::SetName(SOURCE_FILTERGRAPH_NAME);

    pszDefaultSourceVideoFile[0] = NULL;
    _tcscat(pszDefaultSourceVideoFile, pszSourcePath);
    _tcscat(pszDefaultSourceVideoFile, pszSourceVideoFile);    

    return hr;
}

void CSourceFilterGraph::UnInitialize()
{
    Stop();

    CFilterGraph::UnInitialize();

    CFilterGraph::SetName(NULL);

    bUseMediaSeeking = FALSE;
    pIMediaSeeking = NULL;
    pIStreamBufferMediaSeeking = NULL;
}

void CSourceFilterGraph::UseIMediaSeeking()
{
    bUseMediaSeeking = TRUE;
}

HRESULT CSourceFilterGraph::SetupFiltersMPEG(LPCOLESTR pszSourceFileName, DWORD GrabberLocation)
{
    LPCTSTR pszFnName = TEXT("CSourceFilterGraph::SetupFiltersMPEG");
    CComPtr<IBaseFilter> pIBaseFilter_dvr_source;
    CComPtr<IBaseFilter> pIBaseFilterDvrNav2;
    CComPtr<IBaseFilter> pIBaseFilterVideoDecoder;
    CComPtr<IFileSourceFilter> pIFileSourceFilter;
    CComPtr<IBaseFilter> pIBaseFilter_Grabber;
    CComPtr<IBaseFilter> pIBaseFilter_AudioSync;
    CComPtr<IBaseFilter> pIBaseFilter_VideoRenderer;
    CComPtr<IBaseFilter> pIBaseFilter_OverlayMixer;
    CComPtr<IBaseFilter> pIBaseFilter_VBIRenderer;

    LogText(__LINE__, pszFnName, TEXT("Starting"));

    HRESULT hr = this->AddFilterByCLSID(CLSID_DVRSourceFilterMPEG, TEXT("DVR MPEG Branch Source Filter"), &pIBaseFilter_dvr_source);

    if(SUCCEEDED(hr))
    {
        LogText(__LINE__, pszFnName, TEXT("Added DVR MPEG Branch Source Filter"));

        hr = pIBaseFilter_dvr_source->QueryInterface(IID_IFileSourceFilter, (void **) &pIFileSourceFilter);

        if(SUCCEEDED(hr))
        {
            hr = pIFileSourceFilter->Load(pszSourceFileName, NULL);
            if(FAILED(hr))
                LogText(__LINE__, pszFnName, TEXT("Failed to load the source file into the dvr filter."));
        } else LogText(__LINE__, pszFnName, TEXT("Failed to query for the IFileSourceFilter interface."));
    } else LogText(__LINE__, pszFnName, TEXT("Failed to add the dvr source filter."));

    if(SUCCEEDED(hr))
    {
        hr = this->AddFilterByCLSID(CLSID_DVDNav, TEXT("DVR Nav2 Filter"), &pIBaseFilterDvrNav2);
        if(FAILED(hr))
            LogText(__LINE__, pszFnName, TEXT("Failed to add the DVR nav filter."));
    }

    if(SUCCEEDED(hr))
    {
        LogText(__LINE__, pszFnName, TEXT("Added DVR Nav2 Filter"));

        if(SUCCEEDED(hr) && GrabberLocation == GRABBER_LOCATION_SOURCE)
        {
            hr = this->AddFilterByCLSID(CLSID_myGrabberSample, TEXT("Grabber Filter"), &pIBaseFilter_Grabber);

            if(SUCCEEDED(hr))
            {
                hr = this->ConnectFilters(pIBaseFilter_dvr_source, pIBaseFilter_Grabber);
        
                if(SUCCEEDED(hr))
                {
                    hr = this->ConnectFilters(pIBaseFilter_Grabber, pIBaseFilterDvrNav2);
                    if(FAILED(hr))
                        LogText(__LINE__, pszFnName, TEXT("Failed to connect the grabber filter to the nav 2 filter."));
                } else LogText(__LINE__, pszFnName, TEXT("Failed to connect the dvr source filter to the grabber filter."));
            } else LogText(__LINE__, pszFnName, TEXT("Failed to add the grabber filter."));
        }
        else
        {
            hr = this->ConnectFilters(pIBaseFilter_dvr_source, pIBaseFilterDvrNav2);
            if(FAILED(hr))
                LogText(__LINE__, pszFnName, TEXT("Failed to connect the dvr source to the dvr nav filter."));
        }
    }

    if(SUCCEEDED(hr))
    {
        hr = this->AddFilterByCLSID(VideoDecoderCLSID, TEXT("Video Decoder"), &pIBaseFilterVideoDecoder);

        if(SUCCEEDED(hr))
        {
            LogText(__LINE__, pszFnName, TEXT("Added decoder filter"));

            hr = this->ConnectFilters(pIBaseFilterDvrNav2, pIBaseFilterVideoDecoder);
            if(FAILED(hr))
                LogText(__LINE__, pszFnName, TEXT("Failed to connect the dvr nav filter to the decoder."));
        }
    } else LogText(__LINE__, pszFnName, TEXT("Failed to add the video decoder filter."));

    // render the VBI pin if we have it, do this first because this pin can cause interference when attempting to
    // have DShow render the remaining pins on the filter. RenderFilter(pIBaseFilterVideoDecoder) will fail because
    // this is not an optional pin, and the vbi renderer has a merit of do not use.
    if(SUCCEEDED(hr) && (DVRGraphLayout & SourceVBIRenderer))
    {
        hr = this->AddFilterByCLSID(CLSID_VBIDataRenderer, TEXT("VBIRenderer"), &pIBaseFilter_VBIRenderer);

        if(SUCCEEDED(hr))
        {
            LogText(__LINE__, pszFnName, TEXT("VBI renderer"));

            hr = this->ConnectFilters(pIBaseFilterVideoDecoder, pIBaseFilter_VBIRenderer);
            if(FAILED(hr))
                LogText(__LINE__, pszFnName, TEXT("Failed to connect the video decoder to the vbi renderer."));
        } else LogText(__LINE__, pszFnName, TEXT("Failed to add the vbi data renderer."));
    }

    if(SUCCEEDED(hr) && (DVRGraphLayout & SourceVideoRenderer))
    {
        hr = this->AddFilterByCLSID(CLSID_VideoRenderer, TEXT("Video Renderer"), &pIBaseFilter_VideoRenderer);
        if(SUCCEEDED(hr))
        {
            LogText(__LINE__, pszFnName, TEXT("Added Video Renderer Filter"));
            hr = this->ConnectFilters(pIBaseFilterVideoDecoder, pIBaseFilter_VideoRenderer);

            if(SUCCEEDED(hr))
            {
                if(GrabberLocation == GRABBER_LOCATION_VIDEO)
                {
                    hr = this->AddFilterByCLSID(CLSID_myGrabberSample, TEXT("Grabber Filter"), &pIBaseFilter_Grabber);

                    if(SUCCEEDED(hr))
                    {
                        hr = this->InsertFilter(pIBaseFilterVideoDecoder, pIBaseFilter_Grabber, MEDIATYPE_Video, TRUE);
                        if(SUCCEEDED(hr))
                            LogText(__LINE__, pszFnName, TEXT("Inserted grabber filter"));
                        else LogText(__LINE__, pszFnName, TEXT("Failed to insert the grabber filter."));
                    } else LogText(__LINE__, pszFnName, TEXT("Failed to add the grabber filter."));
                }
            } else LogText(__LINE__, pszFnName, TEXT("Failed to connect the decoder to the video renderer."));
        } else LogText(TEXT("Failed to add the video renderer filter."));
    }
    else if(SUCCEEDED(hr) && (DVRGraphLayout & SourceOverlayMixer))
    {
        hr = this->AddFilterByCLSID(CLSID_OverlayMixer, TEXT("Overlay Mixer"), &pIBaseFilter_OverlayMixer);

        if(SUCCEEDED(hr))
        {
            LogText(__LINE__, pszFnName, TEXT("Added Overlay Mixer Filter"));

            if(GrabberLocation == GRABBER_LOCATION_VIDEO)
            {
                if(SUCCEEDED(hr))
                {
                    hr = this->ConnectFilters(pIBaseFilterVideoDecoder, pIBaseFilter_OverlayMixer);
                    if(SUCCEEDED(hr))
                    {
                        hr = this->RenderFilter(pIBaseFilter_OverlayMixer);
                        if(SUCCEEDED(hr))
                        {
                            hr = this->AddFilterByCLSID(CLSID_myGrabberSample, TEXT("Grabber Filter"), &pIBaseFilter_Grabber);

                            if(SUCCEEDED(hr))
                            {
                                hr = this->InsertFilter(pIBaseFilterVideoDecoder, pIBaseFilter_Grabber, MEDIATYPE_Video, TRUE);
                                if(SUCCEEDED(hr))
                                    LogText(__LINE__, pszFnName, TEXT("Inserted grabber filter"));
                                 else LogText(__LINE__, pszFnName, TEXT("Failed to insert the grabber filter."));
                            } else LogText(__LINE__, pszFnName, TEXT("Failed to add the grabber filter."));
                        } else LogText(__LINE__, pszFnName, TEXT("Failed to render the overlay mixer."));
                    } else LogText(__LINE__, pszFnName, TEXT("Failed to connect the video decoder to the overlay mixer"));
                }
            }
        } else LogText(__LINE__, pszFnName, TEXT("Failed to add the overlay mixer filter."));
    }

    if(SUCCEEDED(hr) && (DVRGraphLayout & SourceAudioSyncFilter))
    {
        hr = this->AddFilterByCLSID(CLSID_AudioSync, TEXT("AudioSync"), &pIBaseFilter_AudioSync);

        if(SUCCEEDED(hr))
        {
            LogText(__LINE__, pszFnName, TEXT("Audio sync filter"));

            hr = this->ConnectFilters(pIBaseFilterVideoDecoder, pIBaseFilter_AudioSync);

            if(SUCCEEDED(hr) && GrabberLocation == GRABBER_LOCATION_AUDIO)
            {
                hr = this->ConnectFilters(pIBaseFilter_AudioSync, pIBaseFilter_Grabber);
                if(SUCCEEDED(hr))
                {
                    hr = this->RenderFilter(pIBaseFilter_Grabber);
                    if(FAILED(hr))
                        LogText(__LINE__, pszFnName, TEXT("Failed to render the grabber filter."));
                } else LogText(__LINE__, pszFnName, TEXT("Failed to connect the audio sync filter to the grabber filter."));
            }
            else if(SUCCEEDED(hr))
            {
                hr = this->RenderFilter(pIBaseFilter_AudioSync);
                if(FAILED(hr))
                    LogText(__LINE__, pszFnName, TEXT("Failed to render the audio sync filter."));
            } else LogText(__LINE__, pszFnName, TEXT("Failed to connect the video decoder to the audio sync filter."));
        } else LogText(__LINE__, pszFnName, TEXT("Failed to add the audio sync filter."));
    }
    // if we're not putting in the audio sync filter, then just render the last pins of the decoder (if there are any).
    else if(SUCCEEDED(hr))
        hr = this->RenderFilter(pIBaseFilterVideoDecoder);

    LogText(__LINE__, pszFnName, TEXT("Done"));

    return hr;
}

HRESULT CSourceFilterGraph::SetupFiltersWMV(LPCOLESTR pszSourceFileName, DWORD GrabberLocation)
{
    LPCTSTR pszFnName = TEXT("CSourceFilterGraph::SetupFiltersWMV");
    CComPtr<IBaseFilter> pIBaseFilter_dvr_source;
    CComPtr<IBaseFilter> pIBaseFilter_asf_demux;
    CComPtr<IFileSourceFilter> pIFileSourceFilter;
    CComPtr<IBaseFilter> pIBaseFilter_Grabber;

    HRESULT hr = this->AddFilterByCLSID(CLSID_DVRSourceFilter, TEXT("DVR Source Filter"), &pIBaseFilter_dvr_source);

    if(SUCCEEDED(hr))
    {
        hr = pIBaseFilter_dvr_source->QueryInterface(IID_IFileSourceFilter, (void **) &pIFileSourceFilter);

        if(SUCCEEDED(hr))
        {
            hr = pIFileSourceFilter->Load(pszSourceFileName, NULL);
            if(FAILED(hr))
                LogText(__LINE__, pszFnName, TEXT("Failed to load the source file into the dvr source."));
        } else LogText(__LINE__, pszFnName, TEXT("Failed to query for the IFileSourceFilter interface."));
    } else LogText(__LINE__, pszFnName, TEXT("Failed to add the WMV DVR source filter"));

    if(SUCCEEDED(hr) && GrabberLocation != GRABBER_LOCATION_NONE)
    {
        hr = this->AddFilterByCLSID(CLSID_myGrabberSample, TEXT("Grabber Filter"), &pIBaseFilter_Grabber);
        if(FAILED(hr))
            LogText(__LINE__, pszFnName, TEXT("Failed to the grabber filter."));
    }

    if(SUCCEEDED(hr))
    {
        hr = this->AddFilterByCLSID(CLSID_ASF_DEMUX_FILTER, TEXT("ASF Demux Filter"), &pIBaseFilter_asf_demux);
        if(FAILED(hr))
            LogText(__LINE__, pszFnName, TEXT("Failed to add the ASF demux filter."));
    }

    if(SUCCEEDED(hr))
    {
        if(GrabberLocation == GRABBER_LOCATION_SOURCE)
        {
            hr = this->ConnectFilters(pIBaseFilter_dvr_source, pIBaseFilter_Grabber);
    
            if(SUCCEEDED(hr))
            {
                hr = this->ConnectFilters(pIBaseFilter_Grabber, pIBaseFilter_asf_demux);
                if(FAILED(hr))
                    LogText(__LINE__, pszFnName, TEXT("Failed to connect the grabber filter to the demux."));
            } else LogText(__LINE__, pszFnName, TEXT("Failed to connect the DVR source to the grabber filter."));
        }
        else
        {
            hr = this->ConnectFilters(pIBaseFilter_dvr_source, pIBaseFilter_asf_demux);
            if(FAILED(hr))
                LogText(__LINE__, pszFnName, TEXT("Failed to connect the dvr source to the demux."));
        }
    }

    if(SUCCEEDED(hr) && (DVRGraphLayout & SourceVideoRenderer))
    {
        hr = this->AddFilterByCLSID(CLSID_VideoRenderer, TEXT("Video Renderer"), NULL);
        if(SUCCEEDED(hr))
            LogText(__LINE__, pszFnName, TEXT("Added Video Renderer Filter."));
         else LogText(__LINE__, pszFnName, TEXT("Failed to add the video renderer filter."));
    }
    else if(SUCCEEDED(hr) && (DVRGraphLayout & SourceOverlayMixer))
    {
        hr = this->AddFilterByCLSID(CLSID_OverlayMixer, TEXT("Overlay Mixer"), NULL);
    
        if(SUCCEEDED(hr))
            LogText(__LINE__, pszFnName, TEXT("Added Overlay Mixer Filter."));
         else LogText(__LINE__, pszFnName, TEXT("Failed to add the overlay mixer filter."));
    }
    else if(SUCCEEDED(hr) && (DVRGraphLayout & SourceNULLRenderer))
    {
        hr = this->AddFilterByCLSID(CLSID_QinNullRenderer, TEXT("Null Renderer"), NULL);

        if(SUCCEEDED(hr))
            LogText(__LINE__, pszFnName, TEXT("Added NULL renderer Filter."));
         else LogText(__LINE__, pszFnName, TEXT("Failed to add the null renderer filter."));
    }

    if(SUCCEEDED(hr))
    {
        hr = this->RenderFilter(pIBaseFilter_asf_demux);
        if(FAILED(hr))
            LogText(__LINE__, pszFnName, TEXT("Failed to render the demux filter."));
    }

    return hr;
}

HRESULT CSourceFilterGraph::SetupFilters()
{
    return SetupFilters(pszDefaultSourceVideoFile, FALSE);
}

HRESULT CSourceFilterGraph::SetupFilters(LPCOLESTR pszSourceFileName)
{
    return SetupFilters(pszSourceFileName, FALSE);
}

HRESULT CSourceFilterGraph::SetupFilters(BOOL bUseGrabber)
{
    return SetupFilters2(pszDefaultSourceVideoFile, bUseGrabber?GRABBER_LOCATION_SOURCE:GRABBER_LOCATION_NONE);
}

HRESULT CSourceFilterGraph::SetupFilters(LPCOLESTR pszSourceFileName, BOOL bUseGrabber)
{
    return SetupFilters2(pszSourceFileName, bUseGrabber?GRABBER_LOCATION_SOURCE:GRABBER_LOCATION_NONE);
}

HRESULT CSourceFilterGraph::SetupFilters2(DWORD GrabberLocation)
{
    return SetupFilters2(pszDefaultSourceVideoFile, GrabberLocation);
}

HRESULT CSourceFilterGraph::SetupFilters2(LPCOLESTR pszSourceFileName, DWORD GrabberLocation)
{
    HRESULT hr  = E_UNEXPECTED;
    HRESULT hr2 = E_UNEXPECTED;

    if(DVRGraphLayout & SourceMPEGGraph)
    {
        hr = SetupFiltersMPEG(pszSourceFileName, GrabberLocation);
    }
    else if(DVRGraphLayout & SourceASFGraph)
    {
        hr = SetupFiltersWMV(pszSourceFileName, GrabberLocation);
    }
    else if(DVRGraphLayout & SourceTestGraph)
    {
        hr = SetupNullSourceGraph(pszSourceFileName, GrabberLocation);
    }

    if(bUseMediaSeeking)
    {
        hr2 = pIGraphBuilder->QueryInterface(/*IID_IMediaSeeking*/__uuidof(IMediaSeeking), (void **) &pIMediaSeeking);
    }
    else
    {
        hr2 = FindInterface(/*IID_IStreamBufferMediaSeeking*/__uuidof(IStreamBufferMediaSeeking), (void **) &pIStreamBufferMediaSeeking);
    }

    DisplayFilters();

    if(SUCCEEDED(hr))
    {
        hr = hr2;
    }

    return hr;
}

HRESULT CSourceFilterGraph::SetupNullSourceGraph(LPCOLESTR pszSourceFileName, DWORD GrabberLocation)
{
    LPCTSTR pszFnName = TEXT("CSourceFilterGraph::SetupNullSourceGraph");
    CComPtr<IBaseFilter> pIBaseFilter_dvr_source;
    CComPtr<IBaseFilter> pIBaseFilterGrabber;
    CComPtr<IBaseFilter> pIBaseDVRVideoTestSinkFilter;
    CComPtr<IBaseFilter> pIBaseDVRAudioTestSinkFilter;
    CComPtr<IBaseFilter> pIBaseFilterDvrNav2;
    CComPtr<IFileSourceFilter> pIFileSourceFilter;
    CComPtr<INullFilter> pINullFilter;

    HRESULT hr = this->AddFilterByCLSID(CLSID_DVRSourceFilterMPEG, TEXT("DVR Source Filter MPEG"), &pIBaseFilter_dvr_source);

    if(SUCCEEDED(hr))
    {
        hr = pIBaseFilter_dvr_source->QueryInterface(IID_IFileSourceFilter, (void **) &pIFileSourceFilter);

        if(SUCCEEDED(hr))
        {
            hr = pIFileSourceFilter->Load(pszSourceFileName, NULL);
            if(FAILED(hr))
                LogText(__LINE__, pszFnName, TEXT("Failed to load the file (%s) into the dvr source filter, is it missing or corrupt?"), pszSourceFileName);
        } else LogText(__LINE__, pszFnName, TEXT("Failed to retreive the IFileSourceFilter interface."));
    } else LogText(__LINE__, pszFnName, TEXT("Failed to add the DVR source filter."));


    if(SUCCEEDED(hr))
    {
        hr = this->AddFilterByCLSID(CLSID_DVDNav, TEXT("DVR Nav2 Filter"), &pIBaseFilterDvrNav2);
        if(SUCCEEDED(hr))
            LogText(__LINE__, pszFnName, TEXT("Added DVR Nav2 Filter"));
        else LogText(__LINE__, pszFnName, TEXT("Failed to add the dvr nav filter."));
    }

    if(SUCCEEDED(hr) && GrabberLocation != GRABBER_LOCATION_NONE)
    {
        hr = this->AddFilterByCLSID(CLSID_myGrabberSample, TEXT("Grabber Filter"), &pIBaseFilterGrabber);
        if(SUCCEEDED(hr))
        {
            LogText(__LINE__, pszFnName, TEXT("Added sample grabber filter"));

            hr = this->ConnectFilters(pIBaseFilter_dvr_source, pIBaseFilterGrabber);
            if(SUCCEEDED(hr))
            {
                hr = this->ConnectFilters(pIBaseFilterGrabber, pIBaseFilterDvrNav2);
            } else LogText(__LINE__, pszFnName, TEXT("Failed to connect the dvr source to the grabber filter."));
        } else LogText(__LINE__, pszFnName, TEXT("Failed to add the sample grabber filter."));
    }
    else if(SUCCEEDED(hr))
    {
        hr = this->ConnectFilters(pIBaseFilter_dvr_source, pIBaseFilterDvrNav2);
        if(FAILED(hr))
            LogText(__LINE__, pszFnName, TEXT("Failed to connect the dvr source filter to the dvr nav filter."));
    }

    if(SUCCEEDED(hr) && (DVRGraphLayout & SourceNULLRenderer))
    {
        hr = this->AddFilterByCLSID(CLSID_QinNullRenderer, TEXT("DVR video test sink filter"), &pIBaseDVRVideoTestSinkFilter);
        if(SUCCEEDED(hr))
            LogText(__LINE__, pszFnName, TEXT("Added DVR test sink filter for video"));
         else LogText(__LINE__, pszFnName, TEXT("Failed to add the dvr test sink filter for video."));
    }

    if(SUCCEEDED(hr) && (DVRGraphLayout & SourceNULLRenderer))
    {
        hr = this->AddFilterByCLSID(CLSID_QinNullRenderer, TEXT("DVR audio test sink filter"), &pIBaseDVRAudioTestSinkFilter);
        if(SUCCEEDED(hr))
        {
            // As this instance of the qinnullrenderer isn't for video, disable it's IKSPropertySet interface
            // to avoid DVREngine confusion.
            hr = pIBaseDVRAudioTestSinkFilter->QueryInterface(&pINullFilter);
            if(SUCCEEDED(hr))
                pINullFilter->DisableIKSPropertySet();
            else LogText(__LINE__, pszFnName, TEXT("Failed to Query the INullFilter interface."));

            LogText(__LINE__, pszFnName, TEXT("Added DVR test sink filter for audio"));
        }
        else LogText(__LINE__, pszFnName, TEXT("Failed to add the dvr test sink filter for audio."));
    }

    // now connect up the nav filter to the test sink
    if(SUCCEEDED(hr))
    {
        hr = this->ConnectFilters(pIBaseFilterDvrNav2, pIBaseDVRVideoTestSinkFilter);
        if(FAILED(hr))
            LogText(__LINE__, pszFnName, TEXT("Failed to connect the dvr nav filter to the dvr test sink filter."));
    }

    if(SUCCEEDED(hr))
    {
        hr = this->ConnectFilters(pIBaseFilterDvrNav2, pIBaseDVRAudioTestSinkFilter);
        if(FAILED(hr))
            LogText(__LINE__, pszFnName, TEXT("Failed to connect the dvr nav filter to the dvr test sink filter."));
    }

    return hr;
}

HRESULT CSourceFilterGraph::SetupVideoWindow(HWND hWnd, DWORD dwWindowStyle)
{
    CComPtr<IVideoWindow> pIVideoWindow;
    RECT rect;
    HRESULT hr = E_POINTER;

    memset(&rect, 0, sizeof(RECT));

    hr = FindInterface(IID_IVideoWindow, (void **) &pIVideoWindow);
    if(pIVideoWindow)
    {
            if(SUCCEEDED(hr = pIVideoWindow->put_Owner((OAHWND)hWnd)))
            {
                if(SUCCEEDED(hr = pIVideoWindow->put_WindowStyle(dwWindowStyle)))
                {
                    GetClientRect(hWnd, &rect);

                    hr = pIVideoWindow->SetWindowPosition(0, 0, rect.right - rect.left, rect.bottom - rect.top);
                }
            }
    }

    return hr;
}


HRESULT CSourceFilterGraph::GetCapabilities(DWORD *pCapabilities)
{
    HRESULT hr = E_POINTER;

    if(bUseMediaSeeking)
    {
        if(pIMediaSeeking != NULL)
        {
            hr = pIMediaSeeking->GetCapabilities(pCapabilities);
        }
    }
    else
    {
        if(pIStreamBufferMediaSeeking != NULL)
        {
            hr = pIStreamBufferMediaSeeking->GetCapabilities(pCapabilities);
        }
    }

    return hr;
}

HRESULT CSourceFilterGraph::CheckCapabilities(DWORD *pCapabilities)
{
    HRESULT hr = E_POINTER;

    if(bUseMediaSeeking)
    {
        if(pIMediaSeeking != NULL)
        {
            hr = pIMediaSeeking->CheckCapabilities(pCapabilities);
        }
    }
    else
    {
        if(pIStreamBufferMediaSeeking != NULL)
        {
            hr = pIStreamBufferMediaSeeking->CheckCapabilities(pCapabilities);
        }
    }

    return hr;
}

HRESULT CSourceFilterGraph::IsFormatSupported(const GUID *pFormat)
{
    HRESULT hr = E_POINTER;

    if(bUseMediaSeeking)
    {
        if(pIMediaSeeking != NULL)
        {
            hr = pIMediaSeeking->IsFormatSupported(pFormat);
        }
    }
    else
    {
        if(pIStreamBufferMediaSeeking != NULL)
        {
            hr = pIStreamBufferMediaSeeking->IsFormatSupported(pFormat);
        }
    }

    return hr;
}

HRESULT CSourceFilterGraph::QueryPreferredFormat(GUID *pFormat)
{
    HRESULT hr = E_POINTER;

    if(bUseMediaSeeking)
    {
        if(pIMediaSeeking != NULL)
        {
            hr = pIMediaSeeking->QueryPreferredFormat(pFormat);
        }
    }
    else
    {
        if(pIStreamBufferMediaSeeking != NULL)
        {
            hr = pIStreamBufferMediaSeeking->QueryPreferredFormat(pFormat);
        }
    }

    return hr;
}

HRESULT CSourceFilterGraph::GetTimeFormat(GUID *pFormat)
{
    HRESULT hr = E_POINTER;

    if(bUseMediaSeeking)
    {
        if(pIMediaSeeking != NULL)
        {
            hr = pIMediaSeeking->GetTimeFormat(pFormat);
        }
    }
    else
    {
        if(pIStreamBufferMediaSeeking != NULL)
        {
            hr = pIStreamBufferMediaSeeking->GetTimeFormat(pFormat);
        }
    }

    return hr;
}

HRESULT CSourceFilterGraph::IsUsingTimeFormat(const GUID *pFormat)
{
    HRESULT hr = E_POINTER;

    if(bUseMediaSeeking)
    {
        if(pIMediaSeeking != NULL)
        {
            hr = pIMediaSeeking->IsUsingTimeFormat(pFormat);
        }
    }
    else
    {
        if(pIStreamBufferMediaSeeking != NULL)
        {
            hr = pIStreamBufferMediaSeeking->IsUsingTimeFormat(pFormat);
        }
    }

    return hr;
}

HRESULT CSourceFilterGraph::SetTimeFormat(const GUID *pFormat)
{
    HRESULT hr = E_POINTER;

    if(bUseMediaSeeking)
    {
        if(pIMediaSeeking != NULL)
        {
            hr = pIMediaSeeking->SetTimeFormat(pFormat);
        }
    }
    else
    {
        if(pIStreamBufferMediaSeeking != NULL)
        {
            hr = pIStreamBufferMediaSeeking->SetTimeFormat(pFormat);
        }
    }

    return hr;
}

HRESULT CSourceFilterGraph::GetDuration(LONGLONG *pDuration)
{
    HRESULT hr = E_POINTER;

    if(bUseMediaSeeking)
    {
        if(pIMediaSeeking != NULL)
        {
            hr = pIMediaSeeking->GetDuration(pDuration);
        }
    }
    else
    {
        if(pIStreamBufferMediaSeeking != NULL)
        {
            hr = pIStreamBufferMediaSeeking->GetDuration(pDuration);
        }
    }

    return hr;
}

HRESULT CSourceFilterGraph::GetStopPosition(LONGLONG *pStop)
{
    HRESULT hr = E_POINTER;

    if(bUseMediaSeeking)
    {
        if(pIMediaSeeking != NULL)
        {
            hr = pIMediaSeeking->GetStopPosition(pStop);
        }
    }
    else
    {
        if(pIStreamBufferMediaSeeking != NULL)
        {
            hr = pIStreamBufferMediaSeeking->GetStopPosition(pStop);
        }
    }

    return hr;
}

HRESULT CSourceFilterGraph::GetCurrentPosition(LONGLONG *pCurrent)
{
    HRESULT hr = E_POINTER;

    if(bUseMediaSeeking)
    {
        if(pIMediaSeeking != NULL)
        {
            hr = pIMediaSeeking->GetCurrentPosition(pCurrent);
        }
    }
    else
    {
        if(pIStreamBufferMediaSeeking != NULL)
        {
            hr = pIStreamBufferMediaSeeking->GetCurrentPosition(pCurrent);
        }
    }

    return hr;
}

HRESULT CSourceFilterGraph::ConvertTimeFormat(LONGLONG *pTarget, const GUID *pTargetFormat, LONGLONG Source, const GUID *pSourceFormat)
{
    HRESULT hr = E_POINTER;

    if(bUseMediaSeeking)
    {
        if(pIMediaSeeking != NULL)
        {
            hr = pIMediaSeeking->ConvertTimeFormat(pTarget, pTargetFormat, Source, pSourceFormat);
        }
    }
    else
    {
        if(pIStreamBufferMediaSeeking != NULL)
        {
            hr = pIStreamBufferMediaSeeking->ConvertTimeFormat(pTarget, pTargetFormat, Source, pSourceFormat);
        }
    }

    return hr;
}

HRESULT CSourceFilterGraph::SetPositions(LONGLONG *pCurrent, DWORD dwCurrentFlags, LONGLONG *pStop, DWORD dwStopFlags)
{
    HRESULT hr = E_POINTER;

    if(bUseMediaSeeking)
    {
        if(pIMediaSeeking != NULL)
        {
            hr = pIMediaSeeking->SetPositions(pCurrent, dwCurrentFlags, pStop, dwStopFlags);
        }
    }
    else
    {
        if(pIStreamBufferMediaSeeking != NULL)
        {
            hr = pIStreamBufferMediaSeeking->SetPositions(pCurrent, dwCurrentFlags, pStop, dwStopFlags);
        }
    }

    return hr;
}

HRESULT CSourceFilterGraph::GetPositions(LONGLONG *pCurrent, LONGLONG *pStop)
{
    HRESULT hr = E_POINTER;

    if(bUseMediaSeeking)
    {
        if(pIMediaSeeking != NULL)
        {
            hr = pIMediaSeeking->GetPositions(pCurrent, pStop);
        }
    }
    else
    {
        if(pIStreamBufferMediaSeeking != NULL)
        {
            hr = pIStreamBufferMediaSeeking->GetPositions(pCurrent, pStop);
        }
    }

    return hr;
}

HRESULT CSourceFilterGraph::GetAvailable(LONGLONG *pEarliest, LONGLONG *pLatest)
{
    HRESULT hr = E_POINTER;

    if(bUseMediaSeeking)
    {
        if(pIMediaSeeking != NULL)
        {
            hr = pIMediaSeeking->GetAvailable(pEarliest, pLatest);
        }
    }
    else
    {
        if(pIStreamBufferMediaSeeking != NULL)
        {
            hr = pIStreamBufferMediaSeeking->GetAvailable(pEarliest, pLatest);
        }
    }

    return hr;
}

HRESULT CSourceFilterGraph::SetRate(double dRate)
{
    HRESULT hr = E_POINTER;

    if(bUseMediaSeeking)
    {
        if(pIMediaSeeking != NULL)
        {
            hr = pIMediaSeeking->SetRate(dRate);
        }
    }
    else
    {
        if(pIStreamBufferMediaSeeking != NULL)
        {
            hr = pIStreamBufferMediaSeeking->SetRate(dRate);
        }
    }

    return hr;
}

HRESULT CSourceFilterGraph::GetRate(double *pdRate)
{
    HRESULT hr = E_POINTER;

    if(bUseMediaSeeking)
    {
        if(pIMediaSeeking != NULL)
        {
            hr = pIMediaSeeking->GetRate(pdRate);
        }
    }
    else
    {
        if(pIStreamBufferMediaSeeking != NULL)
        {
            hr = pIStreamBufferMediaSeeking->GetRate(pdRate);
        }
    }

    return hr;
}

HRESULT CSourceFilterGraph::GetPreroll(LONGLONG *pllPreroll)
{
    HRESULT hr = E_POINTER;

    if(bUseMediaSeeking)
    {
        if(pIMediaSeeking != NULL)
        {
            hr = pIMediaSeeking->GetPreroll(pllPreroll);
        }
    }
    else
    {
        if(pIStreamBufferMediaSeeking != NULL)
        {
            hr = pIStreamBufferMediaSeeking->GetPreroll(pllPreroll);
        }
    }

    return hr;
}

//IFileSourceFilter
HRESULT CSourceFilterGraph::Load(LPCOLESTR pszFileName, const AM_MEDIA_TYPE *pmt)
{
    CComPtr<IFileSourceFilter> pIFileSourceFilter;

    HRESULT hr = this->FindInterface(IID_IFileSourceFilter, (void **) &pIFileSourceFilter);

    if(SUCCEEDED(hr))
    {
        hr = pIFileSourceFilter->Load(pszFileName, pmt);
    }

    return hr;
}

HRESULT CSourceFilterGraph::GetCurFile(LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt)
{
    CComPtr<IFileSourceFilter> pIFileSourceFilter;

    HRESULT hr = this->FindInterface(IID_IFileSourceFilter, (void **) &pIFileSourceFilter);

    if(SUCCEEDED(hr))
    {
        hr = pIFileSourceFilter->GetCurFile(ppszFileName, pmt);
    }

    return hr;
}

HRESULT CSourceFilterGraph::GetDefaultRecordedFile(TCHAR *tszFile)
{
    _tcscpy(tszFile, pszDefaultSourceVideoFile);
    return S_OK;
}

