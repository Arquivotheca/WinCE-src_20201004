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

#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <msacm.h>
#include <wavedev.h>
#include <winbase.h>
#include <msgqueue.h>
#include <linklist.h>
#include <waveapi_interface.h>
#include <waveproxy.h>

//
// The device handle is stored globally per process. The first audio function
// call per process will initialize the variable.
//
extern HINSTANCE g_hInstance;
extern DWORD g_dwThreadPrio;

class CWaveProxy;

class CWaveLib
    {
public:
    CWaveLib();
    virtual ~CWaveLib();

    BOOL Init();
    BOOL InitCallbackInterface();

    void Lock()   {EnterCriticalSection(&m_cs);}
    void Unlock() {LeaveCriticalSection(&m_cs);}

    BOOL QueueCallback(WAVECALLBACKMSG *msg);

    HWAVE AddProxy(CWaveProxy *pProxy);
    BOOL RemoveProxy(CWaveProxy *pProxy);
    CWaveProxy *ProxyFromHandle(HANDLE hWave, BOOL bOutput);

    BOOL WaitForMsgQueue(HANDLE hEvent, DWORD dwTimeout);

    BOOL MmDeviceIoControl(DWORD dwIoControlCode,
                           __inout_bcount_opt(nInBufSize)LPVOID lpInBuf,
                           DWORD nInBufSize,
                           __inout_bcount_opt(nOutBufSize) LPVOID lpOutBuf,
                           DWORD nOutBufSize,
                           __out_opt LPDWORD lpBytesReturned)
    {
        return DeviceIoControl (m_hDevice, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, NULL);
    }

    DWORD MmParamIoControl(DWORD dwCode, int nParams, const DWORD dwParams[])
    {
        DWORD dwRet = MMSYSERR_NODRIVER;
        DWORD dwTmp;

        MmDeviceIoControl (dwCode,
                           (LPVOID) dwParams,
                           nParams * sizeof(DWORD),
                           &dwRet,
                           sizeof(DWORD),
                           &dwTmp);

        return(dwRet);
    }

    DWORD CallbackThreadId() {return m_CallbackThreadId;}

protected:
    static DWORD WaveCallbackThreadEntry (PVOID pArg);
    DWORD WaveCallbackThread();

    BOOL Deinit();
    BOOL DeinitCallbackInterface();
    BOOL DeleteProxyList();

    CRITICAL_SECTION m_cs;
    HANDLE m_hqWaveCallbackRead;
    HANDLE m_hqWaveCallbackWrite;
    LIST_ENTRY m_ProxyList;
    BOOL m_bCallbackRunning;
    HANDLE m_hDevice;
    DWORD m_CallbackThreadId;
    HANDLE m_hCallbackThread;
    HANDLE m_hevCallbackThreadExit;
    };

CWaveLib *GetWaveLib();
BOOL WaveLibDeinit();

// Helper functions
DWORD   ___MmIoControl(DWORD dwCode, int nParams, __in_ecount(nParams) const DWORD dwParams[]);
LPWAVEFORMATEX CopyWaveformat(LPCWAVEFORMATEX pwfxSrc);
void PlaySound_UpdateSettings(void);


