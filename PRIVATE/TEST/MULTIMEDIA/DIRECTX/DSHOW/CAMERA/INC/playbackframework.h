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
#include "logging.h"

// for ccom pointers
#include <atlbase.h>
// for various filters and streams
#include <streams.h>
#include <dmodshow.h>
#include <dmoreg.h>

#ifndef PLAYBACKFRAMEWORK_H
#define PLAYBACKFRAMEWORK_H

#define PLAYBACK_FRAMEWORK_VERSION 1

#define PLAYBACKMEDIAEVENT_TIMEOUT 4000


#ifndef countof
#define countof(x)  (sizeof(x)/sizeof(*(x)))
#endif

typedef struct _PLAYBACKVIDEORENDERER_STATS
{
    double dFrameRate;
    double dActualRate;
    long   lFramesDropped;
    long lAvgSyncOffset;
    long lDevSyncOffset;
    long lFramesDrawn;
    long lJitter;
    long lSourceWidth;
    long lSourceHeight;
    long lDestWidth;
    long lDestHeight;
} PLAYBACKVIDEORENDERER_STATS;


typedef class CPlaybackFramework
{
    public:
        CPlaybackFramework();
        ~CPlaybackFramework();

        HRESULT Cleanup();

        void OutputRuntimeParameters();
        BOOL ParseCommandLine(LPCTSTR  tszCommandLine);

        HRESULT Init(HWND OwnerWnd, RECT *rcCoordinates, TCHAR *tszFileName);

        HRESULT RunGraph();
        HRESULT PauseGraph();
        HRESULT StopGraph();
        HRESULT SetRate(double dRate);

        HRESULT GetMediaEvent(LONG *lEventCode, LONG *lParam1, LONG *lParam2, long lEventTimeout);
        HRESULT FreeMediaEvent(LONG lEventCode, LONG lParam1, LONG lParam2);
        HRESULT ClearMediaEvents();
        HRESULT WaitForCompletion(long lEventTimeout, long *lEvCode);
        HRESULT FindFilterByName(TCHAR *Name, IBaseFilter **ppFilter);

        HRESULT SetVideoWindowPosition(RECT *rcCoordinates);
        HRESULT GetVideoWindowPosition(RECT *rcCoordinates);
        HRESULT GetVideoRenderMode(DWORD *dwMode);
        HRESULT SetVideoRenderMode(DWORD dwMode);
        HRESULT GetVideoRenderStats(PLAYBACKVIDEORENDERER_STATS *statData);
        HRESULT SwitchVideoRenderMode();

        HRESULT GetScreenOrientation(DWORD *dwOrientation);
        HRESULT SetScreenOrientation(DWORD dwOrientation);

    private:
        BOOL m_bCoInitialized;
        BOOL m_bInitialized;

        CComPtr<IGraphBuilder> m_pGraph;
        CComPtr<IMediaControl> m_pMediaControl;
        CComPtr<IMediaSeeking> m_pMediaSeeking;
        CComPtr<IMediaEventEx> m_pMediaEvent;
        CComPtr<IBaseFilter> m_pVideoRenderer;
        CComPtr<IAMVideoRendererMode> m_pVideoRendererMode;
        CComPtr<IVideoWindow> m_pVideoWindow;
        CComPtr<IBasicVideo> m_pVideoRendererBasicVideo;

}PLAYBACKFRAMEWORK, *PPLAYBACKFRAMEWORK;

#endif
