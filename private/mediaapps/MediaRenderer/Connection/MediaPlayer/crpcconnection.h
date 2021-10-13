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

class CRpcConnection : public IConnection
{
public:
    typedef enum _MediaType
    {
        MediaType_None,
        MediaType_Music,
        MediaType_Video,
        MediaType_Photo
    }MediaType;

public:
    CRpcConnection();
    ~CRpcConnection();

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

protected:

    WCHAR*          m_pStatus;
    WCHAR*          m_pProtocolInfo;
    EMediaClass     m_MediaClass;
    ce::auto_handle m_hStopEvent;
    ce::auto_handle m_hShutdownEvent;
    unsigned short  m_Volume;
    ITransportCallback *m_pITCB;

private:
    static DWORD WINAPI EventThreadProc(LPVOID lpParameter);

private:
    CComPtr<IMediaPlayerControl>        m_remoteControl;
    CRpcConnection::MediaType           m_mediaType;
    ce::wstring                         m_playbackURI;
    ce::wstring                         m_title;
    ce::smart_ptr<IRpcConnectionImpl>   m_connImpl;
    ce::auto_handle                     m_hEventThread;
};

extern "C" HRESULT GetConnection(IConnection** ppConnection);