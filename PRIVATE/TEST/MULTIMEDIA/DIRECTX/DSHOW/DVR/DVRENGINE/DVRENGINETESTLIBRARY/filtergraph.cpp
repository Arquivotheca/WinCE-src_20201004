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
#include "filtergraph.h"
#include "commctrl.h"
#include "stdio.h"
#include "dvrinterfaces.h"
#include "logger.h"
#include "Helper.h"

CFilterGraph::CFilterGraph(void):
        pIGraphBuilder(NULL),
        pIMediaControl(NULL),
        pIMediaEvent(NULL),
        bLogToConsole(FALSE),
        hIMediaEventThread(NULL),
        pszName(NULL),
        OnEventCallback(NULL),
        CallbackUserData(NULL),
        DVRGraphLayout(0)
{
    memset(&pszCommandLine, 0, sizeof(pszCommandLine));
    memset(&IMediaEventThreadWaitHandles, 0, sizeof(IMediaEventThreadWaitHandles));
    memset(&szFileName, 0, MAX_PATH);

    ZeroMemory(&openFileNameData, sizeof(OPENFILENAMEW));
}

CFilterGraph::~CFilterGraph(void)
{
    LogText(TEXT("CFilterGraph::~CFilterGraph"));

    UnInitialize();
}

HRESULT
CFilterGraph::RegisterTestDLLs()
{
    HRESULT hr = S_OK;

    // many of the tests require the grabber filter to function
    if(S_OK != Helper_RegisterDllUnderCE(TEXT("dvrgrabber.dll")))
        if(S_OK != Helper_RegisterDllUnderCE(TEXT("\\hard disk\\dvrgrabber.dll")))
            if(S_OK != Helper_RegisterDllUnderCE(TEXT("\\release\\dvrgrabber.dll")))
            {
                LogText(TEXT("***Unable to register the grabber filter dll which is necessary for testing."));
                hr = E_FAIL;
            }

    // If registering this fails, the user may be using a third party decoder, so it's not an error or a warning
    // we default to trying to register it on the hard disk, then we fall back to the device, and then the release directory, for performance.
    if(S_OK != Helper_RegisterDllUnderCE(TEXT("\\hard disk\\v_dec_ts_ce.ax")))
        if(S_OK != Helper_RegisterDllUnderCE(TEXT("v_dec_ts_ce.ax")))
            Helper_RegisterDllUnderCE(TEXT("\\release\\v_dec_ts_ce.ax"));

    // these are just test filters that are used in some scenarios,
    // not necessarily needed but we should register them anyway just in case.
    Helper_RegisterDllUnderCE(TEXT("qinnullrenderer.dll"));
    Helper_RegisterDllUnderCE(TEXT("inftee.dll"));
    Helper_RegisterDllUnderCE(TEXT("mpeg2demux.dll"));
    Helper_RegisterDllUnderCE(TEXT("mp2prop.dll"));
    Helper_RegisterDllUnderCE(TEXT("pulltopush.dll"));
    Helper_RegisterDllUnderCE(TEXT("pulltopushts.dll"));
    Helper_RegisterDllUnderCE(TEXT("mpegfilesource.dll"));

    return hr;
}

BOOL CFilterGraph::SetCommandLine(const TCHAR * CommandLine)
{
    if(NULL == CommandLine)
        return FALSE;

    _tcsncpy(pszCommandLine, CommandLine, MAX_PATH);
    pszCommandLine[MAX_PATH -1] = NULL;
    return TRUE;
}

void
CFilterGraph::OutputCurrentCommandLineConfiguration()
{
    LogText(TEXT("Command line in use is %s."), pszCommandLine);
    return;
}

HRESULT CFilterGraph::Initialize()
{
    HRESULT hr = E_FAIL;
    
    if(pIGraphBuilder == NULL)
    {
        hr = CoCreateInstance(    CLSID_FilterGraph,
                                NULL,
                                CLSCTX_INPROC_SERVER,
                                IID_IGraphBuilder,
                                (void **) &pIGraphBuilder);
    }

    if(pIGraphBuilder != NULL)
    {
        hr = QueryInterface(IID_IMediaControl, (void **) &pIMediaControl);

        if(SUCCEEDED(hr))
        {
            hr = QueryInterface(IID_IMediaEvent, (void **) &pIMediaEvent);
        }
    }

    if(SUCCEEDED(hr))
    {
        IMediaEventThreadWaitHandles[TERMINATE_I_MEDIA_EVENT_THREAD_HANDLE] = CreateEvent(NULL, FALSE, FALSE, NULL);
        this->pIMediaEvent->GetEventHandle((OAEVENT*)&IMediaEventThreadWaitHandles[I_MEDIA_EVENT_HANDLE]);
        
        if(IMediaEventThreadWaitHandles[TERMINATE_I_MEDIA_EVENT_THREAD_HANDLE] != NULL)
        {
            hIMediaEventThread = CreateThread(NULL, 0, IMediaEventThread, this, 0, &IMediaEventThreadId);
        }
    }

    return hr;
}

HRESULT CFilterGraph::AddFilterByCLSID(const GUID& clsid, LPCWSTR wszName, IBaseFilter **ppIBaseFilter)
{
    HRESULT hr = E_POINTER;

    if(this->pIGraphBuilder != NULL)
    {
        IBaseFilter * pIBaseFilter = NULL;

        hr = CoCreateInstance(    clsid,
                                0,
                                CLSCTX_INPROC_SERVER,
                                IID_IBaseFilter,
                                reinterpret_cast<void**>(&pIBaseFilter));
    
        if (SUCCEEDED(hr))
        {
            hr = pIGraphBuilder->AddFilter(pIBaseFilter, wszName);

            if(SUCCEEDED(hr) && (ppIBaseFilter != NULL))
            {
                *ppIBaseFilter = pIBaseFilter;
            }
            else if(pIBaseFilter)
            {
                pIBaseFilter->Release();
            }
        }
    }

    return hr;
}

DWORD WINAPI CFilterGraph::IMediaEventThread(LPVOID lpParameter)
{
    LPCTSTR pszFnName = TEXT("CFilterGraph::IMediaEventThread");
    CFilterGraph * pFilterGraph = (CFilterGraph *) lpParameter;
    DWORD waitResult;
    long eventCode;
    long eventParam1;
    long eventParam2;

    LogText(__LINE__, pszFnName, TEXT("Starting"));

    while(TRUE)
    {
        waitResult = WaitForMultipleObjects(2, pFilterGraph->IMediaEventThreadWaitHandles, FALSE, INFINITE);

        switch(waitResult)
        {
        case WAIT_OBJECT_0: // TERMINATE_I_MEDIA_EVENT_THREAD_HANDLE
            LogText(__LINE__, pszFnName, TEXT("Ending"));
            ExitThread(0);
            break;
        case WAIT_OBJECT_0 + 1: //I_MEDIA_EVENT_HANDLE
            if(SUCCEEDED(pFilterGraph->pIMediaEvent->GetEvent(&eventCode, &eventParam1, &eventParam2, 0)))
            {
                pFilterGraph->OnEvent(eventCode, eventParam1, eventParam2);
                pFilterGraph->pIMediaEvent->FreeEventParams(eventCode, eventParam1, eventParam2);
            }
            break;
        case WAIT_FAILED:
            break;
        }
    }

    return 0;
}

const TCHAR * CFilterGraph::GetEventCodeString(long lEventCode)
{
    switch(lEventCode)
    {
#ifndef _WIN32_WCE
    case EC_CLOCK_UNSET:// The clock provider was disconnected. 
        return TEXT("EC_CLOCK_UNSET");
    case EC_DEVICE_LOST:// A Plug and Play device was removed or has become available again. 
        return TEXT("EC_DEVICE_LOST");
    case EC_GRAPH_CHANGED:// The filter graph has changed. 
        return TEXT("EC_GRAPH_CHANGED");
    case EC_SNDDEV_IN_ERROR:// An audio device error has occurred on an input pin. 
        return TEXT("EC_SNDDEV_IN_ERROR");
    case EC_SNDDEV_OUT_ERROR:// An audio device error has occurred on an output pin. 
        return TEXT("EC_SNDDEV_OUT_ERROR");
    case EC_STATE_CHANGE:// The filter graph has changed state. 
        return TEXT("EC_STATE_CHANGE");
    case EC_STEP_COMPLETE:// A filter performing frame stepping has stepped the specified number of frames. 
        return TEXT("EC_STEP_COMPLETE");
#endif
    case EC_ACTIVATE:// A video window is being activated or deactivated.
        return TEXT("EC_ACTIVATE");
    case EC_BUFFERING_DATA:// The graph is buffering data, or has stopped buffering data. 
        return TEXT("EC_BUFFERING_DATA");
    case EC_CLOCK_CHANGED:// The reference clock has changed. 
        return TEXT("EC_CLOCK_CHANGED");
    case EC_COMPLETE:// All data from a particular stream has been rendered. 
        return TEXT("EC_COMPLETE");
    case EC_DISPLAY_CHANGED:// The display mode has changed. 
        return TEXT("EC_DISPLAY_CHANGED");
    case EC_END_OF_SEGMENT:// The end of a segment has been reached. 
        return TEXT("EC_END_OF_SEGMENT");
    case EC_ERROR_STILLPLAYING:// An asynchronous command to run the graph has failed. 
        return TEXT("EC_ERROR_STILLPLAYING");
    case EC_ERRORABORT:// An operation was aborted because of an error. 
        return TEXT("EC_ERRORABORT");
    case EC_EXTDEVICE_MODE_CHANGE:// Not supported. 
        return TEXT("EC_EXTDEVICE_MODE_CHANGE");
    case EC_FULLSCREEN_LOST:// The video renderer is switching out of full-screen mode. 
        return TEXT("EC_FULLSCREEN_LOST");
    case EC_LENGTH_CHANGED:// The length of a source has changed. 
        return TEXT("EC_LENGTH_CHANGED");
    case EC_NEED_RESTART:// A filter is requesting that the graph be restarted. 
        return TEXT("EC_NEED_RESTART");
    case EC_NOTIFY_WINDOW:// Notifies a filter of the video renderer's window. 
        return TEXT("EC_NOTIFY_WINDOW");
    case EC_OLE_EVENT:// A filter is passing a text string to the application. 
        return TEXT("EC_OLE_EVENT");
    case EC_OPENING_FILE:// The graph is opening a file, or has finished opening a file. 
        return TEXT("EC_OPENING_FILE");
    case EC_PALETTE_CHANGED:// The video palette has changed. 
        return TEXT("EC_PALETTE_CHANGED");
    case EC_PAUSED:// A pause request has completed. 
        return TEXT("EC_PAUSED");
    case EC_QUALITY_CHANGE:// The graph is dropping samples, for quality control. 
        return TEXT("EC_QUALITY_CHANGE");
    case EC_REPAINT:// A video renderer requires a repaint. 
        return TEXT("EC_REPAINT");
    case EC_SEGMENT_STARTED:// A new segment has started. 
        return TEXT("EC_SEGMENT_STARTED");
    case EC_SHUTTING_DOWN:// The filter graph is shutting down, prior to being destroyed. 
        return TEXT("EC_SHUTTING_DOWN");
    case EC_STARVATION:// A filter is not receiving enough data. 
        return TEXT("EC_STARVATION");
    case EC_STREAM_CONTROL_STARTED:// A stream-control start command has taken effect. 
        return TEXT("EC_STREAM_CONTROL_STARTED");
    case EC_STREAM_CONTROL_STOPPED:// A stream-control start command has taken effect. 
        return TEXT("EC_STREAM_CONTROL_STOPPED");
    case EC_STREAM_ERROR_STILLPLAYING:// An error has occurred in a stream. The stream is still playing. 
        return TEXT("EC_STREAM_ERROR_STILLPLAYING");
    case EC_STREAM_ERROR_STOPPED:// A stream has stopped because of an error. 
        return TEXT("EC_STREAM_ERROR_STOPPED");
    case EC_TIMECODE_AVAILABLE:// Not supported. 
        return TEXT("EC_TIMECODE_AVAILABLE");
    case EC_USERABORT:// The user has terminated playback. 
        return TEXT("EC_USERABORT");
    case EC_VIDEO_SIZE_CHANGED:// The native video size has changed. 
        return TEXT("EC_VIDEO_SIZE_CHANGED");
    case EC_WINDOW_DESTROYED:
        return TEXT("EC_WINDOW_DESTROYED");
    case EC_TIME:
        return TEXT("EC_TIME");
#ifdef _WIN32_WCE
    case EC_PLEASE_REOPEN:
        return TEXT("EC_PLEASE_REOPEN");
    case EC_STATUS:
        return TEXT("EC_STATUS");
    case EC_MARKER_HIT:
        return TEXT("EC_MARKER_HIT");
    case EC_LOADSTATUS:
        return TEXT("EC_LOADSTATUS");
    case EC_FILE_CLOSED:
        return TEXT("EC_FILE_CLOSED");
    case EC_ERRORABORTEX:
        return TEXT("EC_ERRORABORTEX");
    case EC_EOS_SOON:
        return TEXT("EC_EOS_SOON");
    case EC_CONTENTPROPERTY_CHANGED:
        return TEXT("EC_CONTENTPROPERTY_CHANGED");
    case EC_BANDWIDTHCHANGE:
        return TEXT("EC_BANDWIDTHCHANGE");
    case EC_VIDEOFRAMEREADY:
        return TEXT("EC_VIDEOFRAMEREADY");
    case EC_DRMSTATUS:
        return TEXT("EC_DRMSTATUS");
    case EC_VCD_SELECT:
        return TEXT("EC_VCD_SELECT");
    case EC_VCD_PLAY:
        return TEXT("EC_VCD_PLAY");
    case EC_VCD_END:
        return TEXT("EC_VCD_END");
    case EC_PLAY_NEXT:
        return TEXT("EC_PLAY_NEXT");
    case EC_PLAY_PREVIOUS:
        return TEXT("EC_PLAY_PREVIOUS");
    case EC_DRM_LEVEL:
        return TEXT("EC_DRM_LEVEL");
#endif // UNDER_CE
    case STREAMBUFFER_EC_TIMEHOLE:
        return TEXT("STREAMBUFFER_EC_TIMEHOLE");
    case STREAMBUFFER_EC_STALE_DATA_READ:
        return TEXT("STREAMBUFFER_EC_STALE_DATA_READ");
    case STREAMBUFFER_EC_STALE_FILE_DELETED:
        return TEXT("STREAMBUFFER_EC_STALE_FILE_DELETED");
    case STREAMBUFFER_EC_CONTENT_BECOMING_STALE:
        return TEXT("STREAMBUFFER_EC_CONTENT_BECOMING_STALE");
    case STREAMBUFFER_EC_WRITE_FAILURE:
        return TEXT("STREAMBUFFER_EC_WRITE_FAILURE");
    case STREAMBUFFER_EC_READ_FAILURE:
        return TEXT("STREAMBUFFER_EC_READ_FAILURE");
    case STREAMBUFFER_EC_RATE_CHANGED:
        return TEXT("STREAMBUFFER_EC_RATE_CHANGED");
    case DVR_SOURCE_EC_COMPLETE_PENDING:
        return TEXT("DVR_SOURCE_EC_COMPLETE_PENDING");
    case DVR_SOURCE_EC_COMPLETE_DONE:
        return TEXT("DVR_SOURCE_EC_COMPLETE_DONE");
    case DVRENGINE_EVENT_BEGINNING_OF_PAUSE_BUFFER:
        return TEXT("DVRENGINE_EVENT_BEGINNING_OF_PAUSE_BUFFER");
    case DVRENGINE_EVENT_END_OF_PAUSE_BUFFER:
        return TEXT("DVRENGINE_EVENT_END_OF_PAUSE_BUFFER");
    case DVRENGINE_EVENT_RECORDING_END_OF_STREAM:
        return TEXT("DVRENGINE_EVENT_RECORDING_END_OF_STREAM");
    case DVRENGINE_EVENT_COPYPROTECTION_CHANGE_DETECTED:
        return TEXT("DVRENGINE_EVENT_COPYPROTECTION_CHANGE_DETECTED");
    case DVRENGINE_XDSCODEC_NEWXDSPACKET:
        return TEXT("DVRENGINE_XDSCODEC_NEWXDSPACKET");
    case DVRENGINE_XDSCODEC_NEWXDSRATING:
        return TEXT("DVRENGINE_XDSCODEC_NEWXDSRATING");
    case DVRENGINE_XDSCODEC_DUPLICATEXDSRATING:
        return TEXT("DVRENGINE_XDSCODEC_DUPLICATEXDSRATING");
    case DVRENGINE_EVENT_COPYPROTECTION_DUPLICATE_RECEIVED:
        return TEXT("DVRENGINE_EVENT_COPYPROTECTION_DUPLICATE_RECEIVED");
    default:
        return TEXT("UNKNOWN");
    }
}

VOID CFilterGraph::RegisterOnEventCallback(pFnOnEventCallback pOnEventCallback, PVOID pUserData)
{
    this->OnEventCallback = pOnEventCallback;
    this->CallbackUserData = pUserData;
}

void CFilterGraph::OnEvent(long lEventCode, long lParam1, long lParam2)
{
    if(this->OnEventCallback != NULL)
    {
        OnEventCallback(lEventCode, lParam1, lParam2, CallbackUserData);
    }
}

void CFilterGraph::UnInitialize(void)
{
    if(hIMediaEventThread != NULL)
    {
        DWORD exitCode = STILL_ACTIVE;

        SetEvent(IMediaEventThreadWaitHandles[TERMINATE_I_MEDIA_EVENT_THREAD_HANDLE]);

        while(exitCode == STILL_ACTIVE)
        {
            if(GetExitCodeThread(hIMediaEventThread, &exitCode))
            {
            }

            Sleep(1000);
        }

        CloseHandle(hIMediaEventThread);

        hIMediaEventThread = NULL;
        IMediaEventThreadId = NULL;
    }

    if(IMediaEventThreadWaitHandles[TERMINATE_I_MEDIA_EVENT_THREAD_HANDLE])
    {
        CloseHandle(IMediaEventThreadWaitHandles[TERMINATE_I_MEDIA_EVENT_THREAD_HANDLE]);
        IMediaEventThreadWaitHandles[TERMINATE_I_MEDIA_EVENT_THREAD_HANDLE] = NULL;
    }

    memset(&pszCommandLine, 0, sizeof(pszCommandLine));
    memset(&IMediaEventThreadWaitHandles, 0, sizeof(IMediaEventThreadWaitHandles));
    memset(&szFileName, 0, MAX_PATH);
    ZeroMemory(&openFileNameData, sizeof(OPENFILENAMEW));

    pIMediaEvent = NULL;
    pIMediaControl = NULL;
    pIGraphBuilder = NULL;
    bLogToConsole=FALSE;
    pszName=NULL;
    OnEventCallback=NULL;
    CallbackUserData=NULL;
}

DWORD CFilterGraph::GetDVRGraphLayout()
{
    return DVRGraphLayout;
}

DWORD CFilterGraph::SetDVRGraphLayout(DWORD dwLayout)
{
    DWORD dwOldLayout = DVRGraphLayout;
    DVRGraphLayout = dwLayout;
    return dwOldLayout;
}

HRESULT CFilterGraph::Run(void)
{
    return LogHRError(pIMediaControl->Run(), TEXT("CFilterGraph::Run"));
}

HRESULT CFilterGraph::Pause(void)
{
    return LogHRError(pIMediaControl->Pause(), TEXT("CFilterGraph::Pause"));
}

HRESULT CFilterGraph::Stop(void)
{
    // Just in case, we sometimes call Stop multiple times, even though it's already stopped.
    if(pIMediaControl)
        return LogHRError(pIMediaControl->Stop(), TEXT("CFilterGraph::Stop"));
    else return S_OK;
}

void CFilterGraph::LogToConsole(BOOL bConsole)
{
    bLogToConsole = bConsole;
}

HRESULT CFilterGraph::LogHRError(HRESULT hr, LPCTSTR lpFormat, ...)
{
    LPCTSTR pszFnName = TEXT("CFilterGraph::LogHRError");

    if (FAILED(hr))
    {
        TCHAR szText[512];
        TCHAR szErr[256];
        LPTSTR pszErrDescription;
        size_t bufferSizeForErrDescription;

        va_list args;

        va_start(args, lpFormat);

        StringCchVPrintf(szText, 512, lpFormat, args);

        StringCchPrintfEx(szErr, 256, &pszErrDescription, &bufferSizeForErrDescription, STRSAFE_IGNORE_NULLS, TEXT(" - Error 0x%08x - "), hr);

        DWORD res = AMGetErrorText(hr, pszErrDescription, bufferSizeForErrDescription);


        if(bLogToConsole == TRUE)
        {
            StringCchCat(szText, 512, szErr);
            LogError(__LINE__, pszFnName, szText);
#if DEBUG
#else
#endif
        }
        else
        {
            MessageBox(0, szText, szErr, MB_OK | MB_ICONERROR);
        }
    }

    return hr;
}

//Remarks on OleCreatePropertyFrame:
//The property pages to be displayed are identified with pPageClsID, which is an 
//array of cPages CLSID values. The objects that are affected by this property 
//sheet are identified in ppUnk, an array of size cObjects containing IUnknown pointers.
//
//This function always creates a modal dialogbox and does not return until the dialog box is closed.
//
//Requirements 
//Runs on Versions Defined in Include Link to 
//Windows CE OS 2.0 and later Olectl.h   Oleaut32.lib 
//
//Note   This API is part of the complete Windows CE OS package as provided by Microsoft. 
//The functionality of a particular platform is determined by the original 
//equipment manufacturer (OEM) and some devices may not support this API.
// So disable this until our CE support

HRESULT CFilterGraph::ShowFilterPropertyPages(IBaseFilter *pFilter)
{
#ifdef _WIN32_WCE
    return E_NOTIMPL;
#else
    ISpecifyPropertyPages *pProp;
    HRESULT hr = pFilter->QueryInterface(IID_ISpecifyPropertyPages, (void **)&pProp);
    if (SUCCEEDED(hr)) 
    {
        // Get the filter's name and IUnknown pointer.
        FILTER_INFO FilterInfo;
        hr = pFilter->QueryFilterInfo(&FilterInfo); 
        IUnknown *pFilterUnk;
        pFilter->QueryInterface(IID_IUnknown, (void **)&pFilterUnk);

        // Show the page. 
        CAUUID caGUID;
        pProp->GetPages(&caGUID);
        pProp->Release();
        OleCreatePropertyFrame(
            NULL,                   // Parent window
            0, 0,                   // Reserved
            FilterInfo.achName,     // Caption for the dialog box
            1,                      // Number of objects (just the filter)
            &pFilterUnk,            // Array of object pointers. 
            caGUID.cElems,          // Number of property pages
            caGUID.pElems,          // Array of property page CLSIDs
            0,                      // Locale identifier
            0, NULL                 // Reserved
        );

        // Clean up.
        pFilterUnk->Release();
        FilterInfo.pGraph->Release(); 
        CoTaskMemFree(caGUID.pElems);
    }
    return hr;
#endif
}

void CFilterGraph::SetName(LPCWSTR pszNameToUse)
{
    this->pszName = pszNameToUse;
}

LPCWSTR CFilterGraph::GetName()
{
    return this->pszName;
}

BOOL CFilterGraph::GetFileName(HWND hWnd, LPTSTR pszTitle, LPCTSTR pszFilters, LPCTSTR pszDefExt, GetFileNameModes getFileNameMode)
{
    return GetFileName(hWnd, pszTitle, szFileName, ARRAYSIZE(szFileName), pszFilters, pszDefExt, getFileNameMode);
}

BOOL CFilterGraph::GetFileName(HWND hWnd, LPTSTR pszTitle, LPTSTR pszFileName, DWORD dwFileNameLength, LPCTSTR pszFilters, LPCTSTR pszDefExt, GetFileNameModes getFileNameMode)
{
    // Initialize OPENFILENAME
    ZeroMemory(&openFileNameData, sizeof(OPENFILENAMEW));
    PREFAST_ASSUME(dwFileNameLength);
    ZeroMemory(pszFileName, dwFileNameLength*sizeof(pszFileName[0]));

    openFileNameData.lStructSize     = sizeof(OPENFILENAMEW);
    openFileNameData.hwndOwner       = hWnd;
    openFileNameData.lpstrFile       = pszFileName;
    openFileNameData.nMaxFile        = dwFileNameLength;
    openFileNameData.lpstrFilter     = pszFilters;//L"Graph\0*.grf\0All\0*.*\0";
    openFileNameData.nFilterIndex    = 1;
    openFileNameData.lpstrFileTitle  = NULL;
    openFileNameData.nMaxFileTitle   = 0;
    openFileNameData.lpstrInitialDir = NULL;
    openFileNameData.lpstrTitle      = pszTitle;

    switch(getFileNameMode)
    {
    case GetFileName_ToOpen:
        openFileNameData.Flags       = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        // Display the Open dialog box. 
        return GetOpenFileName(&openFileNameData);
    case GetFileName_ToSave:
        openFileNameData.Flags       = 0;
        openFileNameData.lpstrDefExt = pszDefExt;
        // Display the Open dialog box. 
        return GetSaveFileName(&openFileNameData);
    }
    return false;
}

HRESULT CFilterGraph::SelectGraphFile(HWND hWnd)
{
    if (this->GetFileName(hWnd, TEXT("Select Graph File To Open"), TEXT("Graph\0*.grf\0All\0*.*\0"), TEXT("GRF"), GetFileName_ToOpen)==TRUE)
    {
        return LoadGraphFile(szFileName);
    }
    else
    {
        return E_FAIL;
    }
}

HRESULT CFilterGraph::LoadGraphFile(LPCWSTR wszGraphFileName)
{
    if(this->pIGraphBuilder == NULL)
    {
        return E_UNEXPECTED;
    }

    IStorage *pStorage = 0;

#ifndef _WIN32_WCE
    if (S_OK != StgIsStorageFile(wszGraphFileName))
    {
        return E_FAIL;
    }
#endif
    HRESULT hr = StgOpenStorage(wszGraphFileName, 0, 
        STGM_TRANSACTED | STGM_READ | STGM_SHARE_DENY_WRITE, 
        0, 0, &pStorage);

    if (FAILED(hr))
    {
        return hr;
    }

    IPersistStream *pPersistStream = 0;

    hr = QueryInterface(IID_IPersistStream, reinterpret_cast<void**>(&pPersistStream));

    if (SUCCEEDED(hr))
    {
        IStream *pStream = 0;
        hr = pStorage->OpenStream(L"ActiveMovieGraph", 0, 
            STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &pStream);
        if(SUCCEEDED(hr))
        {
            hr = pPersistStream->Load(pStream);
            pStream->Release();
        }
        pPersistStream->Release();
    }

    pStorage->Release();

    if (SUCCEEDED(hr))
    {
        SetName(wszGraphFileName);
    }

    //DisplayFilters();

    return LogHRError(hr, TEXT("CFilterGraph::LoadGraphFile"));
}

HRESULT CFilterGraph::SaveToGraphFile(WCHAR *wszPath) 
{
//#ifdef _WIN32_WCE
//    return E_NOTIMPL;
//#else
    HRESULT        hr              = E_UNEXPECTED;
    const WCHAR    wszStreamName[] = L"ActiveMovieGraph"; 
    IStorage       *pStorage       = NULL;
    IStream        *pStream        = NULL;
    IPersistStream *pPersist       = NULL;

    if(this->pIGraphBuilder != NULL)
    {
        hr = StgCreateDocfile(    wszPath,
                                STGM_CREATE | STGM_TRANSACTED | STGM_READWRITE | STGM_SHARE_EXCLUSIVE,
                                0,
                                &pStorage);

        if(SUCCEEDED(hr)) 
        {
            hr = pStorage->CreateStream(wszStreamName,
                                        STGM_WRITE | STGM_CREATE | STGM_SHARE_EXCLUSIVE,
                                        0,
                                        0,
                                        &pStream);

            if (SUCCEEDED(hr)) 
            {
                hr = QueryInterface(IID_IPersistStream, (void**)&pPersist);

                if(SUCCEEDED(hr))
                {
                    hr = pPersist->Save(pStream, TRUE);

                    if (SUCCEEDED(hr)) 
                    {
                        hr = pStorage->Commit(STGC_DEFAULT);
                    }

                    pPersist->Release();
                }

                pStream->Release();
            }

            pStorage->Release();
        }
    }

    return hr;
//#endif
}

HRESULT CFilterGraph::SaveToGraphFile(HWND hWnd)
{
    if (this->GetFileName(hWnd, TEXT("Input Graph File Name To Save"), TEXT("Graph\0*.grf\0All\0*.*\0"), TEXT("GRF"), GetFileName_ToSave)==TRUE)
    {
        return SaveToGraphFile(szFileName);
    }
    else
    {
        return E_FAIL;//CommDlgExtendedError();
    }
}

HRESULT CFilterGraph::FindInterface(const IID & riid, void ** ppvObject)
{
    CComPtr<IEnumFilters> pIEnumFilters = NULL;
    CComPtr<IBaseFilter> pIBaseFilter;
    ULONG cFetched;
    HRESULT hr = this->pIGraphBuilder->EnumFilters(&pIEnumFilters);

    if(SUCCEEDED(hr))
    {
        hr = E_FAIL;

        while(pIEnumFilters->Next(1, &pIBaseFilter, &cFetched) == S_OK)
        {
            *ppvObject = NULL;
            hr = pIBaseFilter->QueryInterface(riid, ppvObject);

            pIBaseFilter = NULL;

            if(SUCCEEDED(hr))
            {
                break;
            }
            else if(*ppvObject)
                ((IUnknown *)(*ppvObject))->Release();
        }
    }

    pIEnumFilters = NULL;
    return hr;
}

HRESULT CFilterGraph::QueryInterface(const IID & riid, void ** ppvObject)
{
    return pIGraphBuilder->QueryInterface(riid, ppvObject);
}

HRESULT CFilterGraph::FindFilter(LPCWSTR pszFilterName, IBaseFilter ** ppIBaseFilter)
{
    FILTER_INFO filterInfo;
    CComPtr<IEnumFilters> pIEnumFilters;
    ULONG cFetched;
    HRESULT hr = this->pIGraphBuilder->EnumFilters(&pIEnumFilters);

    if(SUCCEEDED(hr))
    {
        hr = E_FAIL;

        while(pIEnumFilters->Next(1, ppIBaseFilter, &cFetched) == S_OK)
        {
            if(!(*ppIBaseFilter))
            {
                hr = E_FAIL;
                break;
            }
                
            hr = (*ppIBaseFilter)->QueryFilterInfo(&filterInfo);

            if(SUCCEEDED(hr))
            {
                if(filterInfo.pGraph != NULL)
                {
                    filterInfo.pGraph->Release();
                }

                // if we found the filter we're looking for, break out of the loop, and return the interface.
                if(wcscmp(pszFilterName, filterInfo.achName) == 0)
                {
                    break;
                }
            }

            (*ppIBaseFilter)->Release();
        }

        pIEnumFilters = NULL;
    }
    return hr;
}

IPin * CFilterGraph::GetPinByDir(IBaseFilter *pFilter, PIN_DIRECTION PinDir)
{
    CComPtr<IEnumPins> pEnum = NULL;
    IPin *pPin = NULL;

    HRESULT hr = pFilter->EnumPins(&pEnum);

    if(SUCCEEDED(hr))
    {
        while(pEnum->Next(1, &pPin, 0) == S_OK)
        {
            PIN_DIRECTION PinDirThis;
            pPin->QueryDirection(&PinDirThis);

            if (PinDir == PinDirThis)
            {
                break;
            }
            else
            {
                pPin->Release();
                pPin = NULL;
            }
        }

        pEnum = NULL;;
    }

    return pPin;
}

IPin * CFilterGraph::GetPinByName(IBaseFilter *pFilter, LPCWSTR wszPinName)
{
    CComPtr<IEnumPins> pEnum = NULL;
    IPin       *pPin = NULL;
    PIN_INFO myPinInfo;

    HRESULT hr = pFilter->EnumPins(&pEnum);
    if (FAILED(hr))
    {    
        LogHRError(hr, TEXT("Failed to retrieve Pin Enumerator"));
    }
    else
    {
        while(pEnum->Next(1, &pPin, 0) == S_OK)
        {
            hr = pPin->QueryPinInfo(&myPinInfo);

            if (SUCCEEDED(hr))
            {
                if(myPinInfo.pFilter != NULL)
                {
                    myPinInfo.pFilter->Release();
                    myPinInfo.pFilter = NULL;
                }

                if (wcsstr(myPinInfo.achName, wszPinName) != NULL)
                {
                    break;
                }
            }
            // Release the pin for the next time through the loop.
            pPin->Release();
            pPin = NULL;
        }
        // No more pins. We did not find a match.
        pEnum = NULL;
    }
    return pPin;
}

HRESULT CFilterGraph::RenderFilter(IBaseFilter * pIBaseFilter)
{
    LPCTSTR pszFnName = TEXT("CFilterGraph::RenderFilter");
    CComPtr<IEnumPins> pEnum = NULL;
    CComPtr<IPin> pPin = NULL;
    CComPtr<IPin> pConnectedToPin = NULL;
    PIN_INFO   pinInfo;

    LogText(__LINE__, pszFnName, TEXT("Starting"));

    HRESULT hr = pIBaseFilter->EnumPins(&pEnum);

    if (SUCCEEDED(hr))
    {
        while(pEnum->Next(1, &pPin, 0) == S_OK)
        {
            LogText(__LINE__, pszFnName, TEXT("Inside while loop"));

            if(SUCCEEDED(pPin->QueryPinInfo(&pinInfo)))
            {
                LogText(__LINE__, pszFnName, TEXT("Pin(%s) %s"), pinInfo.dir == PINDIR_INPUT? TEXT("INPUT") : TEXT("OUTPUPT"), pinInfo.achName);
                
                if(pinInfo.dir == PINDIR_OUTPUT)
                {
                    pPin->ConnectedTo(&pConnectedToPin);
                

                    if(pConnectedToPin == NULL)
                    {
                        hr = pIGraphBuilder->Render(pPin);
                        LogHRError(hr, TEXT("pIGraphBuilder->Render %s"), pinInfo.achName);
                    }
                    else
                    {
                        pConnectedToPin = NULL;
                    }
                }

                // release our outstanding reference count to the pin's filter
                if(pinInfo.pFilter != NULL)
                {
                    pinInfo.pFilter->Release();
                    pinInfo.pFilter = NULL;
                }
            }
            else
            {
                LogText(__LINE__, pszFnName, TEXT("pPin->QueryPinInfo failed"));
            }

            pPin = NULL;

            if(FAILED(hr))
            {
                break;
            }
        }

        pEnum = NULL;
    }

    LogText(__LINE__, pszFnName, TEXT("Done"));

    return hr;
}

HRESULT CFilterGraph::RenderPin(IPin * pPin)
{
    HRESULT hr = pIGraphBuilder->Render(pPin);
    LogHRError(hr, TEXT("pIGraphBuilder->Render"));

    return hr;
}

HRESULT CFilterGraph::ConnectOutputPinToFilter(IPin * pIPinOutput, IBaseFilter * pIBaseFilterInput)
{
    LPCTSTR pszFnName = TEXT("CFilterGraph::ConnectOutputPinToFilter");
    BOOL       bConnected = FALSE;
    CComPtr<IEnumPins> pEnum = NULL;
    CComPtr<IPin> pIPinInput = NULL;
    CComPtr<IPin> pConnectedToPin = NULL;
    PIN_INFO   pinInfo;

    HRESULT hr = pIBaseFilterInput->EnumPins(&pEnum);

    if (SUCCEEDED(hr))
    {
        // we're given a pin to connect to, go through all of the base filter inputs
        // and try to connect until we're out of input pins or there's a connection.
        while( (!bConnected) && (pEnum->Next(1, &pIPinInput, 0) == S_OK) && pIPinInput)
        {
            if(SUCCEEDED(pIPinInput->QueryPinInfo(&pinInfo)))
            {
                pIPinInput->ConnectedTo(&pConnectedToPin);

                if(pConnectedToPin == NULL)
                {
                    if(pinInfo.dir == PINDIR_INPUT)
                    {
                        hr = pIGraphBuilder->Connect(pIPinOutput, pIPinInput);

                        if(SUCCEEDED(hr))
                        {
                            bConnected = TRUE;
                        }
                        else LogText(__LINE__, pszFnName, TEXT("pIGraphBuilder->Connect %s failed, hr = 0x%08x"), pinInfo.achName, hr);
                    }
                }
                else
                {
                    pConnectedToPin = NULL;
                }

                if(pinInfo.pFilter != NULL)
                {
                    pinInfo.pFilter->Release();
                    pinInfo.pFilter = NULL;
                }
            }

            pIPinInput = NULL;
        }

        pEnum= NULL;
    }

    // no pins to connect is a success, the hresult of the last pin connection attempt
    // is the return value

    return hr;
}

HRESULT CFilterGraph::ConnectFilters(IBaseFilter * pIBaseFilterOutput, IBaseFilter * pIBaseFilterInput, UINT32 uiNumberOfPinsToConnect)
{
    LPCTSTR pszFnName = TEXT("CFilterGraph::ConnectFilters");
    CComPtr<IEnumPins> pEnum = NULL;
    CComPtr<IPin> pIPinOutput = NULL;
    CComPtr<IPin> pConnectedToPin = NULL;
    PIN_INFO   pinInfo;
    UINT32     uiNumberOfPinsConnected = 0;
    BOOL       bOutputFilterHadPinsToConnect = FALSE;

    HRESULT hr = pIBaseFilterOutput->EnumPins(&pEnum);

    if (SUCCEEDED(hr))
    {
        while(pEnum->Next(1, &pIPinOutput, 0) == S_OK && pIPinOutput)
        {
            hr = pIPinOutput->QueryPinInfo(&pinInfo);

            if(SUCCEEDED(hr))
            {
                pIPinOutput->ConnectedTo(&pConnectedToPin);

                if(pConnectedToPin == NULL)
                {
                    if(pinInfo.dir == PINDIR_OUTPUT)
                    {
                        bOutputFilterHadPinsToConnect = TRUE;

                        hr = this->ConnectOutputPinToFilter(pIPinOutput, pIBaseFilterInput);

                        if(SUCCEEDED(hr))
                        {
                            uiNumberOfPinsConnected++;
                        }
                        else LogText(__LINE__, pszFnName, TEXT("pIGraphBuilder->Connect %s failed, hr = 0x%08x"), pinInfo.achName, hr);
                    }
                }
                else
                {
                    pConnectedToPin = NULL;
                }

                if(pinInfo.pFilter != NULL)
                {
                    pinInfo.pFilter->Release();
                    pinInfo.pFilter = NULL;
                }
            }

            pIPinOutput = NULL;

            if( (uiNumberOfPinsToConnect > 0) && (uiNumberOfPinsConnected == uiNumberOfPinsToConnect))
            {
                break;
            }
        }

        pEnum = NULL;
    }

    if(FALSE == bOutputFilterHadPinsToConnect)
    {
        hr = LogHRError(E_FAIL, TEXT("CFilterGraph::ConnectFilters - bOutputFilterHadPinsToConnect == FALSE"));
    }

    // if we connected something the first time, but when attempting to connect more failed, we still want to pass.
    if(uiNumberOfPinsConnected > 0)
        hr = S_OK;

    return hr;
}

HRESULT CFilterGraph::ConnectFilters(IBaseFilter * pIBaseFilterOutput, IBaseFilter * pIBaseFilterInput)
{
    return ConnectFilters(pIBaseFilterOutput, pIBaseFilterInput, 0);
}

void CFilterGraph::DisplayPinsOfFilter(IBaseFilter *pFilter)
{
    LPCTSTR pszFnName = TEXT("CFilterGraph::DisplayPinsOfFilter");
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
            
                LogText(__LINE__, pszFnName, TEXT("Pin(%s) %s - %s"), pinInfo.dir == PINDIR_INPUT? TEXT("INPUT") : TEXT("OUTPUPT"), pinInfo.achName, pConnectedToPin==NULL ? TEXT("Unconnected") : TEXT("Connected"));
                
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

    LogText(__LINE__, pszFnName, TEXT(""));
}

HRESULT    CFilterGraph::DisplayFilters()
{
    LPCTSTR pszFnName = TEXT("CFilterGraph::DisplayFilters");
    CComPtr<IEnumFilters> pIEnumFilters;
    CComPtr<IBaseFilter> pIBaseFilter;
    ULONG cFetched;
    FILTER_INFO filterInfo;

    HRESULT hr = this->pIGraphBuilder->EnumFilters(&pIEnumFilters);

    if(SUCCEEDED(hr))
    {
        hr = E_FAIL;

        while(pIEnumFilters->Next(1, &pIBaseFilter, &cFetched) == S_OK)
        {
            hr = pIBaseFilter->QueryFilterInfo(&filterInfo);

            LogText(__LINE__, pszFnName, TEXT("Filter %s"), filterInfo.achName);

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

HRESULT CFilterGraph::InsertFilter(IBaseFilter *pUpstreamFilter, IBaseFilter *pFilterToInsert, GUID majortype, BOOL ConnectDirect)
{
    HRESULT hr = S_OK;
    CComPtr<IEnumPins> pEnum = NULL;
    CComPtr<IPin> pPinUpstreamOut = NULL;
    CComPtr<IPin> pPinDownstreamIn = NULL;
    CComPtr<IPin> pPinInsertIn = NULL;
    CComPtr<IPin> pPinInsertOut = NULL;
    CComPtr<IPin> pConnectedToPin = NULL;
    PIN_INFO   pinInfo;
    AM_MEDIA_TYPE mtTemp;
    AM_MEDIA_TYPE mtNegotiated;

    if(SUCCEEDED(hr = pUpstreamFilter->EnumPins(&pEnum)))
    {
        // search through the pins, when we find the connection we're looking for,
        // leave the input and output pin interfaces, grab the format, and break out of the loop.
        while((hr = pEnum->Next(1, &pPinUpstreamOut, 0)) == S_OK)
        {
            if(SUCCEEDED(hr = pPinUpstreamOut->QueryPinInfo(&pinInfo)))
            {
                // free the pointer to the pin's base filter, we don't need it.
                if(pinInfo.pFilter)
                {
                    pinInfo.pFilter->Release();
                    pinInfo.pFilter = NULL;
                }

                // get the pin it's connected to, and grab the media type (assuming it's connected)
                pPinUpstreamOut->ConnectedTo(&pPinDownstreamIn);
                pPinUpstreamOut->ConnectionMediaType(&mtTemp);
                FreeMediaType(mtTemp);

                // if we're connected and the pin type and format match, then grab our negotiated type and break out,
                // leaving the input and output pin interfaces available.
                if(pPinDownstreamIn != NULL && pinInfo.dir == PINDIR_OUTPUT && (mtTemp.majortype == majortype || !memcmp(&mtTemp.majortype, &GUID_NULL, sizeof(GUID)) || !memcmp(&majortype, &GUID_NULL, sizeof(GUID))))
                {
                    if(ConnectDirect)
                    {
                        hr = pPinUpstreamOut->ConnectionMediaType(&mtNegotiated);
                    }
                    break;
                }

                if(pPinDownstreamIn)
                {
                    pPinDownstreamIn = NULL;
                }
            }
            pPinUpstreamOut = NULL;
        }
        pEnum = NULL;
    }

    // if we grabbed the pins we need from above, grab the input pin
    // for the filter being inserted
    if (SUCCEEDED(hr) && SUCCEEDED(hr = pFilterToInsert->EnumPins(&pEnum)))
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
                {
                    pConnectedToPin = NULL;
                }

                // as long as we're not connected to anything, the direction is right, and this pin will accept the media type we need,
                // then it's the one we're looking for.  Break out of the loop leaving the input pin interface available.
                if(pConnectedToPin == NULL && pinInfo.dir == PINDIR_INPUT && SUCCEEDED(pPinInsertIn->QueryAccept(&mtNegotiated)))
                    break;
            }

            pPinInsertIn = NULL;
        }
        pEnum=NULL;
    }

    // now lets find the insertion filter output pin
    if (SUCCEEDED(hr) && SUCCEEDED(hr = pFilterToInsert->EnumPins(&pEnum)))
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
                {
                    pConnectedToPin = NULL;
                }

                // as long as it's not connected, it's an output, and it'll accept the negotiated format, then it's the one we're looking for,
                // break out and leave it available.
                if(pConnectedToPin == NULL && pinInfo.dir == PINDIR_OUTPUT && SUCCEEDED(pPinInsertOut->QueryAccept(&mtNegotiated)))
                    break;
            }

            pPinInsertOut = NULL;
        }
        pEnum=NULL;
    }

    // if everything above succeeded, disconnect and put in the new filter
    if(SUCCEEDED(hr))
    {
        if(SUCCEEDED(hr = this->pIGraphBuilder->Disconnect(pPinUpstreamOut)))
        {
            if(SUCCEEDED(hr = this->pIGraphBuilder->Disconnect(pPinDownstreamIn)))
            {
                if(ConnectDirect)
                {
                    if(SUCCEEDED(hr = this->pIGraphBuilder->ConnectDirect(pPinUpstreamOut, pPinInsertIn, &mtNegotiated)))
                        hr = this->pIGraphBuilder->ConnectDirect(pPinInsertOut, pPinDownstreamIn, &mtNegotiated);
                }
                else
                {
                    if(SUCCEEDED(hr = this->pIGraphBuilder->Connect(pPinUpstreamOut, pPinInsertIn)))
                        hr = this->pIGraphBuilder->Connect(pPinInsertOut, pPinDownstreamIn);
                }
            }
        }
    }

    if(ConnectDirect)
        FreeMediaType(mtNegotiated);

    // now that everything's connected (or something failed), release any of the leftover pin interfaces
    // from above.
    pPinUpstreamOut = NULL;
    pPinDownstreamIn = NULL;
    pPinInsertIn = NULL;
    pPinInsertOut = NULL;

    return hr;
}

