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

//
// This is for callbacks from connection to transport
//
typedef enum
{
    ETransportCallbackEvent_EOS,   // Playback is done and needs a STOP
    ETransportCallbackEvent_BOS    // Playback is queued and needs a START
                                   // intended for use with DMS1.0 devices
} ETransportCallbackEvent;


class ITransportCallback
{
public:
    virtual HRESULT EventNotify(ETransportCallbackEvent event) = 0;
};


//
// This is for calls from transport to connection 
//

typedef enum 
{
   MediaClass_Unknown,
   MediaClass_Image,
   MediaClass_Audio,
   MediaClass_Video,
} EMediaClass;

class IConnection
{
public:
    virtual ~IConnection(){}
    virtual HRESULT SetURI(LPCWSTR pszURI, IMediaMetadata *pIMM, EMediaClass MediaClass) = 0;
    virtual HRESULT SetCurrentTrack(unsigned long nTrack, av::PositionInfo* pPositionInfo) = 0;

    // The media class allows for play to be skipped when not needed.
    virtual HRESULT Play() = 0;
    virtual HRESULT Stop() = 0;
    virtual HRESULT Pause() = 0;

    virtual HRESULT Cleanup() = 0;

    virtual HRESULT SetVolume(unsigned short Volume) = 0;
    virtual HRESULT GetVolume(unsigned short* pVolume) = 0;
    virtual HRESULT SetProtocolInfo(LPCWSTR pszProtocolInfo) = 0;
    virtual HRESULT GetProtocolInfo(LPWSTR* ppszProtocolInfo) = 0;
    virtual HRESULT SetStatus(LPWSTR pszStatus) = 0;
    virtual HRESULT GetStatus(LPWSTR* ppszProtocolInfo) = 0;
    virtual HRESULT GetCurrentPosition( LONGLONG *pllPosition ) = 0;
    virtual HRESULT SetPositions( LONGLONG *pllStart, DWORD dwStartFlags, LONGLONG *pllStop, DWORD dwStopFlags ) = 0;

    virtual HRESULT PrepareEvent(ITransportCallback *pICB) = 0;
};

extern "C" HRESULT GetConnection(IConnection** ppConnection);
