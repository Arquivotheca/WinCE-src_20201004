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
//------------------------------------------------------------------------------
//
//  waveproxyacm.h
//
//------------------------------------------------------------------------------
#pragma once

#include <waveproxy.h>

class CWaveProxyACM : public CWaveProxy
    {
public:
    CWaveProxyACM( DWORD dwCallback,
                DWORD dwInstance,
                DWORD fdwOpen,
                BOOL bIsOutput,
                CWaveLib *pCWaveLib );

    virtual ~CWaveProxyACM();

    virtual MMRESULT waveOpen( LPCWAVEFORMATEX pwfx );
    virtual MMRESULT waveClose();
    virtual MMRESULT wavePrepareHeader  ( LPWAVEHDR pwh, UINT cbwh );
    virtual MMRESULT waveUnprepareHeader( LPWAVEHDR pwh, UINT cbwh );
    virtual MMRESULT waveQueueBuffer    ( LPWAVEHDR pwh, UINT cbwh );
    virtual MMRESULT waveGetPosition    ( LPMMTIME pmmt, UINT cbmmt);

    virtual void FilterWomDone(WAVECALLBACKMSG *pmsg);
    virtual void FilterWimData(WAVECALLBACKMSG *pmsg);

protected:
    MMRESULT FindConverterMatch();

    static BOOL s_DriverEnumCallback(HACMDRIVERID hadid, DWORD dwInstance, DWORD fdwSupport);
    BOOL DriverEnumCallback(HACMDRIVERID hadid, DWORD fdwSupport);

    MMRESULT            m_mmrClient;

    UINT                m_uHeuristic;

    HACMDRIVER          m_had;            // handle to ACM driver we chose
    HACMSTREAM          m_has;            // handle to ACM conversion stream
    LPWAVEFORMATEX      m_pwfxSrc;        // source format when mapping
    LPWAVEFORMATEX      m_pwfxDst;        // destination format when mapping
    DWORD               m_fdwSupport;     // support required for conversion

    LPWAVEFORMATEX      m_pwfxClient;     // format of client wave data

    LPWAVEFORMATEX      m_pwfxReal;       // format of device wave data
    DWORD               m_cbwfxReal;
    };


