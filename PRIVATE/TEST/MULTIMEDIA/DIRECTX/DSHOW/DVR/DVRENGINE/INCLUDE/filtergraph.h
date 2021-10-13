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
#pragma once
#include <dshow.h>
#include <atlbase.h>
#include "clparse.h"

#ifdef _WIN32_WCE
#include <Commdlg.h>
#endif

typedef enum
{
    GetFileName_ToOpen,
    GetFileName_ToSave
} GetFileNameModes;

enum
{
    SinkMPEGGraph = 0x1,
    SinkASFGraph = 0x2,

    SinkFileSource = 0x4,
    SinkASyncFileSource = 0x8,
    SinkTunerSource = 0x10,
    SinkUserSource = 0x20,

    SourceMPEGGraph = 0x100,
    SourceASFGraph = 0x200,
    SourceTestGraph = 0x400, // source test graph requires NULL renderer

    SourceAudioSyncFilter = 0x800,
    SourceNULLRenderer = 0x1000,
    SourceVideoRenderer = 0x2000,
    SourceOverlayMixer = 0x4000,
    SourceVBIRenderer = 0x8000,
};

typedef VOID(*pFnOnEventCallback) (long lEventCode, long lParam1, long lParam2, PVOID pUserData);

class CFilterGraph
{
public:
    CFilterGraph(void);
    ~CFilterGraph(void);
    HRESULT RegisterTestDLLs();
    virtual BOOL SetCommandLine(const TCHAR * CommandLine);
    virtual void OutputCurrentCommandLineConfiguration();
    void LogToConsole(BOOL bConsole);
    virtual HRESULT Initialize();
    virtual void UnInitialize(void);
    HRESULT AddFilterByCLSID(const GUID& clsid, LPCWSTR wszName, IBaseFilter **ppIBaseFilter);
    HRESULT ShowFilterPropertyPages(IBaseFilter *pFilter);
    HRESULT Run(void);
    HRESULT Pause(void);
    HRESULT Stop(void);
    void    SetName(LPCWSTR pszNameToUse);
    LPCWSTR GetName();
    HRESULT SelectGraphFile(HWND hwnd);
    HRESULT LoadGraphFile(const WCHAR* wszName);
    HRESULT SaveToGraphFile(WCHAR *wszPath);
    HRESULT SaveToGraphFile(HWND hWnd);
    HRESULT FindInterface(const IID & riid, void ** ppvObject);
    HRESULT QueryInterface(const IID & riid, void ** ppvObject);
    HRESULT FindFilter(LPCWSTR pszFilterName, IBaseFilter ** ppIBaseFilter);
    BOOL    GetFileName(HWND hWnd, LPTSTR pszTitle, LPTSTR pszFileName, DWORD dwFileNameLength, LPCTSTR pszFilters, LPCTSTR pszDefExt, GetFileNameModes getFileNameMode);
    HRESULT    DisplayFilters();
    void    DisplayPinsOfFilter(IBaseFilter *pFilter);
    IPin *  GetPinByDir(IBaseFilter *pFilter, PIN_DIRECTION PinDir);
    IPin *  GetPinByName(IBaseFilter *pFilter, LPCWSTR wszPinName);
    HRESULT RenderFilter(IBaseFilter * pIBaseFilter);
    HRESULT RenderPin(IPin * pPin);
    HRESULT CFilterGraph::ConnectOutputPinToFilter(IPin * pIPinOutput, IBaseFilter * pIBaseFilterInput);
    HRESULT ConnectFilters(IBaseFilter * pIBaseFilterOutput, IBaseFilter * pIBaseFilterInput, UINT32 uiNumberOfPinsToConnect);
    HRESULT ConnectFilters(IBaseFilter * pIBaseFilterOutput, IBaseFilter * pIBaseFilterInput);
    HRESULT InsertFilter(IBaseFilter *pUpstreamFilter, IBaseFilter *pFilterToInsert, GUID majortype, BOOL ConnectDirect);
    static const TCHAR * GetEventCodeString(long lEventCode);
    VOID    RegisterOnEventCallback(pFnOnEventCallback pOnEventCallback, PVOID pUserData);
    DWORD SetDVRGraphLayout(DWORD dwLayout);
    DWORD GetDVRGraphLayout();

protected:
    CComPtr<IGraphBuilder> pIGraphBuilder; // Graph builder object
    CComPtr<IMediaControl> pIMediaControl; // Media control object
    CComPtr<IMediaEvent>   pIMediaEvent; // Media event   object

    HRESULT LogHRError(HRESULT hr, LPCTSTR lpFormat, ...);
    BOOL    GetFileName(HWND hwnd, LPTSTR pszTitle, LPCTSTR pszFilters, LPCTSTR pszDefExt, GetFileNameModes getFileNameMode);
    virtual void OnEvent(long lEventCode, long lParam1, long lParam2);
    WCHAR szFileName[MAX_PATH];       // buffer for file name
    DWORD DVRGraphLayout;
    TCHAR pszCommandLine[MAX_PATH];

private:
    pFnOnEventCallback OnEventCallback;
    PVOID              CallbackUserData;

    HANDLE hIMediaEventThread;
#define TERMINATE_I_MEDIA_EVENT_THREAD_HANDLE 0
#define I_MEDIA_EVENT_HANDLE 1
    HANDLE IMediaEventThreadWaitHandles[2];
    DWORD  IMediaEventThreadId;
    BOOL bLogToConsole;
    LPCWSTR pszName;
    OPENFILENAMEW openFileNameData;       // common dialog box structure
    static DWORD WINAPI IMediaEventThread(LPVOID lpParameter);
};

