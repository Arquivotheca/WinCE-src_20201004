//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#pragma once


class CConnection : public IConnection
{
public:
    CConnection();
    ~CConnection();

    HRESULT SetURI( LPCWSTR pszURI, IMediaMetadata *pIMM, EMediaClass MediaClass );
    HRESULT SetCurrentTrack( unsigned long nTrack, av::PositionInfo* pPositionInfo );

    HRESULT Play();
    HRESULT Stop();
    HRESULT Pause();

    HRESULT Cleanup();
   
    HRESULT SetVolume(unsigned short Volume);
    HRESULT GetVolume(unsigned short* pVolume);
    HRESULT SetProtocolInfo(LPCWSTR pszProtocolInfo);
    HRESULT GetProtocolInfo(LPWSTR* ppszProtocolInfo);
    HRESULT SetStatus(LPWSTR pszStatus);
    HRESULT GetStatus(LPWSTR* ppszProtocolInfo);
    HRESULT GetCurrentPosition( LONGLONG *pllPosition );
    HRESULT SetPositions( LONGLONG *pllStart, DWORD dwStartFlags, LONGLONG *pllStop, DWORD dwStopFlags );

    HRESULT PrepareEvent(ITransportCallback *pITCB);

    friend class CAVTransport;
    friend class CRenderingControl;

private:
    HRESULT SetVideoWindow( IMediaMetadata *pIMM );
    static DWORD WINAPI EventThreadProc(LPVOID lpParameter);

protected:

    EMediaClass     m_MediaClass;
    WCHAR*          m_pStatus;
    WCHAR*          m_pProtocolInfo;
    unsigned short  m_Volume;
    ITransportCallback*     m_pITCB;

    // DShow interfaces
    CComPtr<IGraphBuilder>  m_pTrackGraph;
    CComPtr<IGraphBuilder>  m_pPlayListGraph;
    CComPtr<IMediaControl>  m_pMediaControl;
    CComPtr<IMediaEvent>    m_pMediaEvent;
    CComPtr<IMediaSeeking>  m_pMediaSeeking;
    CComPtr<IBasicAudio>    m_pBasicAudio;
    CComPtr<IBasicVideo>    m_pBasicVideo;
    CComPtr<IVideoWindow>   m_pVideoWindow;
    CComPtr<IPersistMediaMetadata> m_pPersistMediaMetadata;

    // Synchronization
    ce::auto_handle         m_hEventThread;
    ce::auto_handle         m_hShutdownEvent;
    ce::auto_handle         m_hStartEvent;
    ce::auto_handle         m_hStopEvent;
    ce::critical_section    m_cs;
    
private:
    HRESULT AcquireInterfaces();
    void ReleaseInterfaces(void);

};

extern "C" HRESULT GetConnection(IConnection** ppConnection);