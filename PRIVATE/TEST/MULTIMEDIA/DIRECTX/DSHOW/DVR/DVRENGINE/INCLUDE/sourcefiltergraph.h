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
#include "filtergraph.h"
#include "dvrinterfaces.h"

enum
{
    GRABBER_LOCATION_NONE,
    GRABBER_LOCATION_SOURCE,
    GRABBER_LOCATION_VIDEO,
    GRABBER_LOCATION_AUDIO
};

class CSourceFilterGraph :
    public CFilterGraph
{
public:
    CSourceFilterGraph(void);
    ~CSourceFilterGraph(void);

    virtual HRESULT Initialize();
    virtual void UnInitialize();
    virtual BOOL SetCommandLine(const TCHAR * CommandLine);
    void OutputCurrentCommandLineConfiguration();
    void UseIMediaSeeking();
    virtual HRESULT SetupFilters();
    virtual HRESULT SetupFilters(LPCOLESTR pszSourceFileName);
    virtual HRESULT SetupFilters(LPCOLESTR pszSourceFileName, BOOL bUseGrabber);
    virtual HRESULT SetupFilters(BOOL bUseGrabber);
    virtual HRESULT SetupFilters2(DWORD GrabberSource);
    virtual HRESULT SetupFilters2(LPCOLESTR pszSourceFileName, DWORD GrabberSource);
    HRESULT SetupVideoWindow(HWND hWnd, DWORD dwWindowStyle);

    //IStreamBufferMediaSeeking
    HRESULT GetCapabilities(DWORD *pCapabilities);
    HRESULT CheckCapabilities(DWORD *pCapabilities);
    HRESULT IsFormatSupported(const GUID *pFormat);
    HRESULT QueryPreferredFormat(GUID *pFormat);
    HRESULT GetTimeFormat(GUID *pFormat);
    HRESULT IsUsingTimeFormat(const GUID *pFormat);
    HRESULT SetTimeFormat(const GUID *pFormat);
    HRESULT GetDuration(LONGLONG *pDuration);
    HRESULT GetStopPosition(LONGLONG *pStop);
    HRESULT GetCurrentPosition(LONGLONG *pCurrent);
    HRESULT ConvertTimeFormat(LONGLONG *pTarget, const GUID *pTargetFormat, LONGLONG Source, const GUID *pSourceFormat);
    HRESULT SetPositions(LONGLONG *pCurrent, DWORD dwCurrentFlags, LONGLONG *pStop, DWORD dwStopFlags);
    HRESULT GetPositions(LONGLONG *pCurrent, LONGLONG *pStop);
    HRESULT GetAvailable(LONGLONG *pEarliest, LONGLONG *pLatest);
    HRESULT SetRate(double dRate);
    HRESULT GetRate(double *dRate);
    HRESULT GetPreroll(LONGLONG *pllPreroll);
    HRESULT GetDefaultRecordedFile(TCHAR *tszFile);

    //IFileSourceFilter
    HRESULT Load(LPCOLESTR pszFileName, const AM_MEDIA_TYPE *pmt);
    HRESULT GetCurFile(LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt);

protected:
    BOOL bUseMediaSeeking;
    CComPtr<IMediaSeeking> pIMediaSeeking;
    CComPtr<IStreamBufferMediaSeeking> pIStreamBufferMediaSeeking;
    GUID VideoDecoderCLSID;
    TCHAR pszSourceVideoFile[MAX_PATH];
    TCHAR pszSourcePath[MAX_PATH];
    TCHAR pszDefaultSourceVideoFile[MAX_PATH];

private:
    HRESULT SetupFiltersWMV(LPCOLESTR pszSourceFileName, DWORD GrabberLocation);
    HRESULT SetupFiltersMPEG(LPCOLESTR pszSourceFileName, DWORD GrabberLocation);
    HRESULT SetupNullSourceGraph(LPCOLESTR pszSourceFileName, DWORD GrabberLocation);
};

