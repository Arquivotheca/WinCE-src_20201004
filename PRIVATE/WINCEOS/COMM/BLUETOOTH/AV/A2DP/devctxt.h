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
// -----------------------------------------------------------------------------
//
//      THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//      ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//      THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//      PARTICULAR PURPOSE.
//
// -----------------------------------------------------------------------------

#define SECONDARYGAINCLASSMAX 4

// number of classes affected by the device gain
#define SECONDARYDEVICEGAINCLASSMAX 2

#if defined(MONO_GAIN)
#define MAX_GAIN 0xFFFF
#else
#define MAX_GAIN 0xFFFFFFFF
#endif

class DeviceContext
{
public:
    DeviceContext()
    {
        InitializeListHead(&m_StreamList);
        m_dwGain = MAX_GAIN;
        m_dwDefaultStreamGain = MAX_GAIN;
        for (int i=0;i<SECONDARYGAINCLASSMAX;i++)
        {
            m_dwSecondaryGainLimit[i]=MAX_GAIN;
        }
    }

    virtual BOOL IsSupportedFormat(LPWAVEFORMATEX lpFormat);
    PBYTE TransferBuffer(PBYTE pBuffer, PBYTE pBufferEnd, DWORD *pNumStreams);

    DWORD NewStream(StreamContext *pStreamContext);
    void DeleteStream(StreamContext *pStreamContext);

    DWORD GetGain()
    {
        return m_dwGain;
    }

    DWORD SetGain(DWORD dwGain)
    {
        m_dwGain = dwGain;
        RecalcAllGains();
        return MMSYSERR_NOERROR;
    }

    DWORD GetDefaultStreamGain()
    {
        return m_dwDefaultStreamGain;
    }

    DWORD SetDefaultStreamGain(DWORD dwGain)
    {
        m_dwDefaultStreamGain = dwGain;
        return MMSYSERR_NOERROR;
    }

    DWORD GetSecondaryGainLimit(DWORD GainClass)
    {
        return m_dwSecondaryGainLimit[GainClass];
    }

    DWORD SetSecondaryGainLimit(DWORD GainClass, DWORD Limit)
    {
        if (GainClass>=SECONDARYGAINCLASSMAX)
        {
            return MMSYSERR_ERROR;
        }
        m_dwSecondaryGainLimit[GainClass]=Limit;
        RecalcAllGains();
        return MMSYSERR_NOERROR;
    }

    void RecalcAllGains();
    BOOL StreamsRunning(void);
    BOOL StreamsStillPlaying();
    DWORD OpenStream(LPWAVEOPENDESC lpWOD, DWORD dwFlags, StreamContext **ppStreamContext);
    void ResetDeviceContext(void);
    void FlushOutputAndSetSuspend(void);

    virtual DWORD GetExtDevCaps(PVOID pCaps, DWORD dwSize)=0;
    virtual DWORD GetDevCaps(PVOID pCaps, DWORD dwSize)=0;
    virtual void StreamReadyToRender(StreamContext *pStreamContext)=0;

    virtual StreamContext *CreateStream(LPWAVEOPENDESC lpWOD)=0;

protected:
    LIST_ENTRY  m_StreamList;         // List of streams rendering to/from this device
    DWORD       m_dwGain;
    DWORD       m_dwDefaultStreamGain;
    DWORD m_dwSecondaryGainLimit[SECONDARYGAINCLASSMAX];
};
#ifdef INPUT_ON
class InputDeviceContext : public DeviceContext
{
public:
    StreamContext *CreateStream(LPWAVEOPENDESC lpWOD);
    DWORD GetExtDevCaps(PVOID pCaps, DWORD dwSize);
    DWORD GetDevCaps(PVOID pCaps, DWORD dwSize);
    void StreamReadyToRender(StreamContext *pStreamContext);
};
#endif
class OutputDeviceContext : public DeviceContext
{
public:
    BOOL IsSupportedFormat(LPWAVEFORMATEX lpFormat);
    StreamContext *CreateStream(LPWAVEOPENDESC lpWOD);
    DWORD GetExtDevCaps(PVOID pCaps, DWORD dwSize);
    DWORD GetDevCaps(PVOID pCaps, DWORD dwSize);
    void StreamReadyToRender(StreamContext *pStreamContext);
};


