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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#pragma once

#include <wavelib.h>
#include <msgqueue.h>
#include <linklist.h>

#define INVALID_WAVE_HANDLE_VALUE ((HWAVE)-1)

class CWaveLib;

class CWaveProxy
    {
public:
    LIST_ENTRY m_Link;

    CWaveProxy( DWORD dwCallback,
                DWORD dwInstance,
                DWORD fdwOpen,
                BOOL bIsOutput,
                CWaveLib *pCWaveLib );

    virtual ~CWaveProxy();

    LONG AddRef();
    LONG Release();

    virtual MMRESULT waveOpen(LPCWAVEFORMATEX pwfx);
    virtual MMRESULT waveClose();
    virtual MMRESULT wavePrepareHeader  ( LPWAVEHDR pwh, UINT cbwh );
    virtual MMRESULT waveUnprepareHeader( LPWAVEHDR pwh, UINT cbwh );
    virtual MMRESULT waveQueueBuffer    ( LPWAVEHDR pwh, UINT cbwh );
    virtual MMRESULT waveGetPosition    ( LPMMTIME pmmt, UINT cbmmt);
    virtual MMRESULT waveReset();

    virtual void FilterWomDone(WAVECALLBACKMSG *pmsg);
    virtual void FilterWimData(WAVECALLBACKMSG *pmsg);

    BOOL WaitForMsgQueue(DWORD dwTimeout);
    BOOL DoClientCallback(UINT  uMsg, DWORD dwParam1, DWORD dwParam2);

    HWAVE GetWaveHandle(void)       {return m_hWave;}
    BOOL IsOutput() {return m_bIsOutput;}

    UINT GetDeviceId() { return m_DeviceId; }
    void SetDeviceId(UINT DeviceId) {m_DeviceId=DeviceId; }

    DWORD GetStreamClassId() { return m_dwStreamClassId; }
    void  SetStreamClassId(DWORD dwStreamClassId) { m_dwStreamClassId = dwStreamClassId; }

    DWORD GetOpenFlags() { return m_fdwOpen; }

protected:
    LONG     m_RefCount;
    HANDLE   m_hevSync;       // event for synchronizing open/close/reset
    DWORD    m_dwCallback;    // client callback
    DWORD    m_dwInstance;    // client callback instance
    DWORD    m_fdwOpen;       // client callback flags
    BOOL     m_bIsOutput;
    HWAVE    m_hWave;         // wave handle returned from waveapi
    UINT     m_DeviceId;      // Device ID to open
    DWORD    m_dwStreamClassId;// Stream class ID
    HANDLE   m_hqClientCallbackWriteable;
    CWaveLib *m_pCWaveLib;
    };


CWaveProxy *CreateWaveProxy   (DWORD dwCallback, DWORD dwInstance, DWORD fdwOpen, BOOL bIsOutput, CWaveLib *pCWaveLib);
CWaveProxy *CreateWaveProxyACM(DWORD dwCallback, DWORD dwInstance, DWORD fdwOpen, BOOL bIsOutput, CWaveLib *pCWaveLib);

BOOL acm_Initialize(VOID);
BOOL acm_Terminate(VOID);
BOOL acmui_Initialize(VOID);
BOOL acmui_Terminate(VOID);
BOOL SPS_Init (VOID);
BOOL SPS_DeInit (VOID);


