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


#include "stdafx.h"

HANDLE g_startEvent[] = 
{
    CreateEvent(NULL, TRUE, FALSE, MPLAYER_START_EVENT),
    CreateEvent(NULL, TRUE, FALSE, VPLAYER_START_EVENT),
    CreateEvent(NULL, TRUE, FALSE, PVIEWER_START_EVENT)
};

CRpcConnection::CRpcConnection() : 
    m_pProtocolInfo(NULL),
    m_pStatus(NULL),
    m_pITCB(NULL),
    m_mediaType(CRpcConnection::MediaType_None)
{
    m_Volume = MAX_VOLUME_RANGE * 2 / 3;
}

CRpcConnection::~CRpcConnection()
{
    if( m_pProtocolInfo )
    {
        delete[] m_pProtocolInfo;
    }

    if ( m_hShutdownEvent.valid() )
    {
        SetEvent(m_hShutdownEvent);
    }

    MediaPlayerProxyDeInit((INT)m_mediaType - 1);
    m_mediaType = CRpcConnection::MediaType_None;
}

// Start a dshow graph for the indicated media.
// The URI is required and has Host, Port, and Identifier.
// It also has file suffix and seek denial indicator as appropriate.
HRESULT 
CRpcConnection::SetURI( LPCWSTR pszURI, IMediaMetadata *pIMM, EMediaClass MediaClass )
{
    HRESULT hr = S_OK;
    BSTR key = NULL;
    BSTR val = NULL;

    // check input
    CBREx(pszURI != NULL, E_POINTER);
    CBREx(pIMM != NULL, E_POINTER);

    //Here we store the playback URL into a member variable. and after the media type is detected, the playback URL will be translated into 
    //a command and sent to media player.
    m_MediaClass = MediaClass;
    m_playbackURI = pszURI;
    m_title.clear();

    // Collect the title
    key = SysAllocString(L"dc:title");
    CBREx(key != NULL, E_OUTOFMEMORY);
    CHREx(pIMM->Get(key, &val), S_OK);
    CBREx(m_title.append(val), E_OUTOFMEMORY);

Error:
    SysFreeString(key);
    SysFreeString(val);
    return hr;
}


HRESULT 
CRpcConnection::SetCurrentTrack(unsigned long nTrack, av::PositionInfo* pPositionInfo)
{
    if( nTrack == 1 )
    {
        return S_OK;
    }

    return E_FAIL;
}


HRESULT 
CRpcConnection::Play()
{
    HRESULT hr = E_FAIL;
    WCHAR* pszQueueName;
    STARTUPINFO si;
    DWORD dwRet = 0;
    BOOL bSuccess = FALSE;
    PROCESS_INFORMATION pi;
    WCHAR* buf = NULL;
    WCHAR* cmdLine = NULL;
    DWORD length = MAX_PATH;

    pszQueueName = new WCHAR[length];
    ZeroMemory(pszQueueName, sizeof(WCHAR) * MAX_PATH);

    buf = new WCHAR[length];
    CPR(buf);
    ZeroMemory(buf, sizeof(WCHAR) * length);

    cmdLine = new WCHAR[length];
    CPR(cmdLine);
    ZeroMemory(cmdLine, sizeof(WCHAR) * length);

    CBREx(m_mediaType != CRpcConnection::MediaType_None, E_FAIL);
    CPREx(m_connImpl, E_FAIL);
    CHR(m_connImpl->GetPlayerName(cmdLine, &length));
    memset(&si, 0, sizeof(STARTUPINFO));
    bSuccess = CreateProcess(cmdLine, DMR_TOKEN, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if(bSuccess)
    {
        //wait for the process start.
        dwRet = WaitForSingleObject(g_startEvent[m_mediaType - 1], APPLICATION_START_TIMEOUT);
        CBREx(dwRet != WAIT_TIMEOUT, E_FAIL);

        length = MAX_PATH;
        CHR(m_connImpl->GetPlayerEndpoint(pszQueueName, &length));
        CHR(MediaPlayerProxyInit((INT)m_mediaType - 1, pszQueueName));
        CHR(CreateMediaPlayerProxy((INT)m_mediaType - 1, &m_remoteControl));
        CPREx(m_remoteControl, E_OUTOFMEMORY);

        //if media type is video or photo, remove the query string.
        if(m_mediaType ==  CRpcConnection::MediaType_Video || m_mediaType == CRpcConnection::MediaType_Photo)
        {
            m_playbackURI.erase(m_playbackURI.find(L"?"));
        }
        CHR(m_remoteControl->SetMetadata((INT)Metadata_Title, m_title));
        CHR(m_remoteControl->Play(m_playbackURI.get_buffer()));
    }
Error:
    if(buf)
    {
        delete[] buf;
        buf = NULL;
    }
    if(cmdLine)
    {
        delete[] cmdLine;
        cmdLine = NULL;
    }
    if(pszQueueName)
    {
        delete[] pszQueueName;
        pszQueueName = NULL;
    }
    return hr;
}

HRESULT
CRpcConnection::Pause()
{
    HRESULT hr = S_OK;
    if(m_remoteControl)
    {
        CHR(m_remoteControl->Pause());
    }
Error:
    return hr;
}

HRESULT 
CRpcConnection::Stop()
{
    HRESULT hr = S_OK;
    if(m_remoteControl)
    {
        CHR(m_remoteControl->Stop());
    }
Error:
    return hr;
}


HRESULT 
CRpcConnection::Cleanup()
{
    MediaPlayerProxyDeInit((INT)m_mediaType - 1);
    m_remoteControl.Release();
    return S_OK;
}

HRESULT 
CRpcConnection::SetVolume( unsigned short Volume )
{
    HRESULT hr = S_OK;
    if(m_remoteControl)
    {
        CHR(m_remoteControl->SetVolume(Volume));
    }
Error:
    return hr;
}


HRESULT 
CRpcConnection::GetVolume( unsigned short* pVolume )
{
    HRESULT hr = S_OK;
    CPP(pVolume);
    if(m_remoteControl)
    {
        CHR(m_remoteControl->GetVolume(pVolume));
    }
    else
    {
        // if remoteControl has not been created yet, set to default
        *pVolume = (MIN_VOLUME_RANGE + MAX_VOLUME_RANGE) / 2;
    }
Error:
    return hr;
}


HRESULT 
CRpcConnection::SetProtocolInfo( LPCWSTR pszProtocolInfo )
{
    HRESULT hr = S_OK;
    DWORD dwChars = 0;
    ce::wstring tmpProtocol;

    ChkBool( pszProtocolInfo != NULL, E_INVALIDARG );
    Chk( StringCchLength( pszProtocolInfo, STRSAFE_MAX_CCH, (size_t*) &dwChars ));

    if(m_pProtocolInfo)
    {
        delete[] m_pProtocolInfo;
        m_pProtocolInfo = NULL;
    }

    m_pProtocolInfo = new WCHAR[ ++dwChars ];
    ChkBool( m_pProtocolInfo != NULL, E_OUTOFMEMORY );
    memcpy( m_pProtocolInfo, pszProtocolInfo, dwChars * sizeof( WCHAR ) );

    //assign approporate media type to the m_mediaType;
    tmpProtocol = m_pProtocolInfo;
    if(tmpProtocol.find(AUDIO_PROTOCOL_PATTERN) != ce::wstring::npos) //this is a audio file
    {
        m_mediaType = CRpcConnection::MediaType_Music;
        m_connImpl = new MusicRpcConnection;
    }
    else if(tmpProtocol.find(VIDEO_PROTOCOL_PATTERN) != ce::wstring::npos)
    {
        m_mediaType = CRpcConnection::MediaType_Video;
        m_connImpl = new VideoRpcConnection;
    }
    else if(tmpProtocol.find(IMAGE_PROTOCOL_PATTERN) != ce::wstring::npos)
    {
        m_mediaType = CRpcConnection::MediaType_Photo;
        m_connImpl = new PhotoRpcConnection;
    }
    CPR(m_connImpl);
Cleanup:
Error:
    return hr;
}


HRESULT 
CRpcConnection::GetProtocolInfo( LPWSTR* ppszProtocolInfo )
{
    HRESULT hr = E_FAIL;

    ChkBool( ppszProtocolInfo != NULL, E_INVALIDARG );
    *ppszProtocolInfo = L"";

    if( m_pProtocolInfo)
    {
        // BUGBUG: This is trash. We should be copyying the string
        *ppszProtocolInfo = m_pProtocolInfo;
        hr = S_OK;
    }

Cleanup:
    return hr;
}


HRESULT
CRpcConnection::SetStatus( LPWSTR pszStatus )
{
    HRESULT hr = S_OK;
    DWORD dwChars = 0;
    
    ChkBool( pszStatus != NULL, E_INVALIDARG );
    Chk( StringCchLength( pszStatus, STRSAFE_MAX_CCH, (size_t*)&dwChars ));

    if( m_pStatus )
    {
        delete[] m_pStatus;
        m_pStatus = NULL;
    }

    m_pStatus = new WCHAR[ ++dwChars ];
    memcpy( m_pStatus, pszStatus, dwChars * sizeof( WCHAR ) );

Cleanup:
    return hr;
}


HRESULT 
CRpcConnection::GetStatus( LPWSTR* ppszStatus )
{
    HRESULT hr = E_FAIL;

    ChkBool( ppszStatus != NULL, E_INVALIDARG );
    *ppszStatus = NULL;

    if( m_pStatus )
    {
        // BUGBUG: This is trash. We should be copyying the string
        *ppszStatus = m_pStatus;
        hr = S_OK;
    }

Cleanup:
    return hr;
}

HRESULT 
CRpcConnection::SetPositions( LONGLONG *pllStart, DWORD dwStartFlags, LONGLONG *pllStop, DWORD dwStoptFlags )
{
    HRESULT hr = S_OK;
    LONGLONG llStartLocal = 0;
    LONGLONG llStopLocal = 0;

    // Convert from 1ms to 100ns
    if (pllStart != NULL)
    {
        llStartLocal = *pllStart * 10000;
    }
    if (pllStop != NULL)
    {
        llStopLocal = *pllStop * 10000;
    }
    if(m_remoteControl)
    {
        CHR(m_remoteControl->SetPositions(&llStartLocal, dwStartFlags, &llStopLocal, dwStoptFlags));
    }
Error:
    return hr;
}


HRESULT
CRpcConnection::GetCurrentPosition( LONGLONG *pllPosition )
{
    HRESULT hr = S_OK;
    ChkBool(pllPosition != NULL, E_POINTER);
    ChkBool(m_remoteControl != NULL, E_UNEXPECTED);
    Chk(m_remoteControl->GetCurrentPosition(pllPosition));
    *pllPosition /= 10000; // convert from 100ns to 1ms
Cleanup:
    return hr;
}

HRESULT
CRpcConnection::PrepareEvent(ITransportCallback *pITCB)
{
    HRESULT hr = S_OK;
    ChkBool(pITCB != 0, E_POINTER);
    m_pITCB = pITCB;
    m_hEventThread = CreateThread(NULL, 0, EventThreadProc, this, 0, NULL);
    ChkBool( m_hEventThread.valid(), E_FAIL );

Cleanup:
    return hr;
}

DWORD WINAPI CRpcConnection::EventThreadProc(LPVOID lpParameter)
{
    HRESULT         hr = S_OK;
    const int       cWaitEvents = 2;
    HANDLE          rghEvents[cWaitEvents];
    DWORD           dwResult = 0;
    BOOL            fContinue = TRUE;
    CRpcConnection* pThis = reinterpret_cast<CRpcConnection*>(lpParameter);
    CPREx(pThis, E_FAIL);

    // Prepare the shutdown event if necessary
    if (!pThis->m_hShutdownEvent.valid())
    {
        pThis->m_hShutdownEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        CBREx(pThis->m_hShutdownEvent.valid(), HRESULT_FROM_WIN32(GetLastError()));
    }

    // Prepare the stop event if necessary
    if(!pThis->m_hStopEvent.valid())
    {
        pThis->m_hStopEvent = CreateEvent(NULL, TRUE, FALSE, MEDIA_PLAYER_STOP_EVENT);
        CBREx(pThis->m_hStopEvent.valid(), HRESULT_FROM_WIN32(GetLastError()));
    }

    rghEvents[0] = pThis->m_hShutdownEvent;
    rghEvents[1] = pThis->m_hStopEvent;

    while (fContinue)
    {
        dwResult = WaitForMultipleObjects(cWaitEvents, rghEvents, FALSE, INFINITE);

        // Process the events
        switch(dwResult)
        {
        case WAIT_OBJECT_0+0:
            {
                // We are shutting down
                fContinue = FALSE;

                break;
            }

        case WAIT_OBJECT_0+1:
            {
                // The play is COMPLETE, fire the EOS event
                if (pThis->m_pITCB != NULL)
                {
                    pThis->m_pITCB->EventNotify(ETransportCallbackEvent_EOS);
                }

                // Reset the state to nonsignaled
                CBREx(ResetEvent(pThis->m_hStopEvent), HRESULT_FROM_WIN32(GetLastError()));

                break;
            }

        default:
            {
                break;
            }
        }
    }

Error:
    return 0;
}

extern "C" HRESULT GetConnection(IConnection** ppConnection)
{
    HRESULT hr = S_OK;
    CPREx(ppConnection, E_FAIL);
    *ppConnection = new CRpcConnection;
Error:
    return hr;
}
