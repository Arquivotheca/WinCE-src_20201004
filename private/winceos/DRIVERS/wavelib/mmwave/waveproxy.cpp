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
#include <wavelib.h>
#include <debug.h>

CWaveProxy *CreateWaveProxy(
    DWORD dwCallback,
    DWORD dwInstance,
    DWORD fdwOpen,
    BOOL bIsOutput,
    CWaveLib *pCWaveLib
    )
{
    return new CWaveProxy(dwCallback,dwInstance,fdwOpen,bIsOutput,pCWaveLib);
}

// Constructor for wave proxy object
CWaveProxy::CWaveProxy(
    DWORD dwCallback,
    DWORD dwInstance,
    DWORD fdwOpen,
    BOOL bIsOutput,
    CWaveLib *pCWaveLib
    ) :
    m_RefCount(1),
    m_hevSync(NULL),
    m_dwCallback(dwCallback),
    m_dwInstance(dwInstance),
    m_fdwOpen(fdwOpen),
    m_bIsOutput(bIsOutput),
    m_hWave(INVALID_WAVE_HANDLE_VALUE),
    m_DeviceId(0),
    m_dwStreamClassId(0),
    m_hqClientCallbackWriteable(NULL),
    m_pCWaveLib(pCWaveLib)
{
    return;
}

CWaveProxy::~CWaveProxy()
{
    if (m_hevSync)
        {
        CloseHandle(m_hevSync);
        m_hevSync=NULL;
        }

    if (m_hqClientCallbackWriteable)
        {
        CloseHandle(m_hqClientCallbackWriteable);
        m_hqClientCallbackWriteable=NULL;
        }

    return;
}

LONG CWaveProxy::AddRef(void)
{
    LONG RefCount;
    RefCount = InterlockedIncrement(&m_RefCount);
    return RefCount;
}

LONG CWaveProxy::Release(void)
{
    LONG RefCount;
    RefCount = InterlockedDecrement(&m_RefCount);
    if (RefCount == 0)
    {
        delete this;
    }
    return RefCount;
}

// Function to wait until the message queue is drained. Very dangerous- this will block if called from the callback thread's context.
BOOL CWaveProxy::WaitForMsgQueue(DWORD dwTimeout)
{
    return m_pCWaveLib->WaitForMsgQueue(m_hevSync,dwTimeout);
}

// Callback into the application
BOOL CWaveProxy::DoClientCallback(UINT uMsg, DWORD dwParam1, DWORD dwParam2)
{
    BOOL fRet = TRUE;

    switch (m_fdwOpen & CALLBACK_TYPEMASK)
    {
        case CALLBACK_EVENT:
        {
            // For open/close, don't signal event
            if (uMsg == WIM_DATA || uMsg == WOM_DONE)
            {
                fRet = SetEvent((HANDLE) m_dwCallback);
                if (fRet == FALSE)
                {
                    DEBUGMSG(ZONE_ERROR, (TEXT("ERROR! SetEvent fails (0x%0X)\r\n"), GetLastError()));
                }
            }
            break;
        }
        case CALLBACK_THREAD:
        {
            PostThreadMessage (m_dwCallback, uMsg, (DWORD)m_hWave, dwParam1);
            break;
        }
        case CALLBACK_WINDOW:
        {
            PostMessage ((HWND) m_dwCallback, uMsg, (DWORD)m_hWave, dwParam1);
            break;
        }
        case CALLBACK_FUNCTION:
        {
            PDRVCALLBACK pfnCallback = (PDRVCALLBACK)m_dwCallback;
            pfnCallback(m_hWave, uMsg, m_dwInstance, dwParam1, dwParam2);

            break;
        }
        case CALLBACK_MSGQUEUE:
        {
            WAVEMSG msg;
            msg.uMsg = uMsg;
            msg.hWav = (HANDLE) m_hWave;
            msg.dwInstance = m_dwInstance;
            msg.dwParam1 = dwParam1;
            msg.dwParam2 = dwParam2;
            fRet = WriteMsgQueue(m_hqClientCallbackWriteable, &msg, sizeof(msg), 0, 0);
            if (!fRet)
            {
                // It's up to the application to ensure that the queue is never full or broken.
                // We make no attempt to recover from write errors.
                DEBUGMSG(ZONE_ERROR, (TEXT("WaveLib: WriteMsgQueue failed (%08xh)\r\n"), GetLastError()));
            }
            break;
        }
    }

    switch(uMsg)
        {
        case WOM_CLOSE:
        case WIM_CLOSE:
            {
            Release();
            }
        }

    return fRet;
}

MMRESULT CWaveProxy::waveQueueBuffer( LPWAVEHDR pwh, UINT cbwh )
{
    const DWORD dwParams[] =  {
        (DWORD) m_hWave,
        (DWORD) pwh,
        (DWORD) cbwh
    };

    DWORD dwIoCtl = (m_bIsOutput ? IOCTL_WAVE_OUT_WRITE : IOCTL_WAVE_IN_ADD_BUFFER);
    return((MMRESULT) ___MmIoControl(dwIoCtl, 3, dwParams));
}

MMRESULT CWaveProxy::wavePrepareHeader( LPWAVEHDR pwh, UINT cbwh )
{
    const DWORD dwParams[] =
    {
        (DWORD) m_hWave,
        (DWORD) pwh,
        (DWORD) cbwh
    };

    DWORD dwIoCtl = (m_bIsOutput ? IOCTL_WAVE_OUT_PREPARE_HEADER : IOCTL_WAVE_IN_PREPARE_HEADER);
    return((MMRESULT) ___MmIoControl(dwIoCtl, 3, dwParams));
}

MMRESULT CWaveProxy::waveUnprepareHeader( LPWAVEHDR pwh, UINT cbwh )
{
    const DWORD dwParams[] =  {
        (DWORD) m_hWave,
        (DWORD) pwh,
        (DWORD) cbwh
    };

    DWORD dwIoCtl = (m_bIsOutput ? IOCTL_WAVE_OUT_UNPREPARE_HEADER : IOCTL_WAVE_IN_UNPREPARE_HEADER);
    return((MMRESULT) ___MmIoControl(dwIoCtl, 3, dwParams));
}


MMRESULT CWaveProxy::waveReset()
{
    MMRESULT mmRet;

    const DWORD dwParams[] =
    {
        (DWORD) m_hWave
    };

    DWORD dwIoCtl = (m_bIsOutput ? IOCTL_WAVE_OUT_RESET : IOCTL_WAVE_IN_RESET);
    mmRet = ___MmIoControl(dwIoCtl, 1, dwParams);

    // This is the one place in the waveapi where we need to synchronize API calls with callback processing.
    // This eliminates the race condition that takes place when an app calls waveXXXReset and immediately turns
    // around and calls waveOutUnprepareHeader (and worse, tries to delete the wave header memory that the callback
    // still hasn't processed.
    // When returning from the kernel call above, it is assumed that any outstanding headers have already been sent
    // to the callback thread's message queue. The call to WaitForMsgQueue below will post a synchronization message
    // to the queue and will then wait until the callback thread has processed that synchronization message. This ensures
    // that the callback has finished processing any headers that were returned due to the Reset above.
    // There are three cases where this mechanism can break down:
    // 1. If waveXXXReset was called within the context of the callback thread itself. This can only happen if an application
    // has specified CALLBACK_FUNCTION and is calling waveXXXReset from the callback, which is specifically precluded by the docs.
    // In any case, if this happens then WaitForMsgQueue will detect it and return immediately. Note that the internal implementation
    // of PlaySound may call waveOutReset from the callback thread's context, but it's been written with knowledge of this limitation.
    // 2. If the callback thread is CPU starved, or is otherwise blocked in a callback (possibly a complex deadlock), the call will
    // timeout after 100ms.
    // In either of these cases, if the calling application calls waveOutUnprepareHeader, the call will fail. In addition, if the
    // calling app frees memory allocated to wave headers, and those headers are waiting on the callback thread, there's a high
    // probability of data corruption or fault, since the callback needs to clear the INQUEUE bit in the header and it will
    // be writing to random memory at that point.
    WaitForMsgQueue(100);

    return mmRet;
}

// waveOpen is the lowest level call directly to the kernel to open a wave stream
MMRESULT CWaveProxy::waveOpen( LPCWAVEFORMATEX pwfx )
{
    MMRESULT mmRet;

    // If this is a real open, we need to setup some callback handles.
    BOOL bQuery = (0 != (WAVE_FORMAT_QUERY & m_fdwOpen));
    if (!bQuery)
        {
        // Create the event used to synchronize some operations with the callback thread
        m_hevSync = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!m_hevSync)
            {
                mmRet = MMSYSERR_NOMEM;
                goto Exit;
            }

        // holding event handle here for m_hevSync

        // If the client passed us a message queue, we need our own writeable copy
        if ( (m_fdwOpen & CALLBACK_TYPEMASK) == CALLBACK_MSGQUEUE)
            {
            MSGQUEUEOPTIONS qopts = {0};
            qopts.dwSize = sizeof(qopts);
            qopts.bReadAccess = FALSE;

            m_hqClientCallbackWriteable = OpenMsgQueue((HANDLE)GetCurrentProcessId(),
                                             (HANDLE) m_dwCallback,
                                             &qopts);
            if (!m_hqClientCallbackWriteable)
                {
                mmRet = MMSYSERR_NOMEM;
                goto Exit;
                }
            }
        }

    // Pack all the params into one structure
    const DWORD dwParams[] =
    {
        (DWORD) &m_hWave,    // ptr to where to return wave handle
        (DWORD) m_DeviceId,  // device ID
        (DWORD) pwfx,        // ptr to waveformat
        (DWORD) this,        // proxy (passed back as instance data during callbacks
        (DWORD) (m_fdwOpen & (~CALLBACK_TYPEMASK | ~WAVE_MAPPED)) | CALLBACK_MSGQUEUE,    // open flags
        (DWORD) m_dwStreamClassId,// Stream class ID
    };

    // Call the kernel
    DWORD dwIoCtl = (m_bIsOutput ? IOCTL_WAVE_OUT_OPEN : IOCTL_WAVE_IN_OPEN);
    mmRet =  (MMRESULT) ___MmIoControl(dwIoCtl, 6, dwParams);

Exit:
    if (mmRet!=MMSYSERR_NOERROR)
    {
        if (!bQuery)
        {
            if (m_hevSync)
            {
                CloseHandle(m_hevSync);
                m_hevSync = NULL;
            }
            if (m_hqClientCallbackWriteable)
            {
                CloseHandle(m_hqClientCallbackWriteable);
                m_hqClientCallbackWriteable = NULL;
            }
        }
    }
    return mmRet;
}

MMRESULT CWaveProxy::waveClose()
{
    MMRESULT mmRet;

    const DWORD dwParams[] =
    {
        (DWORD) m_hWave
    };

    DWORD dwIoCtl = (m_bIsOutput ? IOCTL_WAVE_OUT_CLOSE : IOCTL_WAVE_IN_CLOSE);
    mmRet = ((MMRESULT) ___MmIoControl(dwIoCtl, 1, dwParams));

    if (mmRet != MMSYSERR_NOERROR)
        {
        return mmRet;
        }

    // There's always a chance that we just finished playing/recording something,
    // but the message hasn't bubbled up to the callback thread yet.
    // For safety, make sure there are no pending message in the queue, since we're about
    // to release the proxy object.

    // Do an AddRef here, since the callback will do a final release when it's done
    AddRef();

    // Send callback asynchronously. This avoids hanging if someone calls waveOutClose in a function callback
    // while still guaranteeing that all other pending callbacks are processed first.
    WAVECALLBACKMSG msg;
    msg.uMsg = (m_bIsOutput ? WOM_CLOSE : WIM_CLOSE);
    msg.dwInstance = (DWORD)this;
    msg.dwParam1 = 0;
    msg.dwParam2 = 0;
    if (!m_pCWaveLib->QueueCallback(&msg))
        {
        // Should never happen, but...
        Release();
        }

    // Removed for now- this was causing some 10 second deadlocks
    // WaitForMsgQueue(10000);  // Wait up to 10 seconds for queue to drain?

    // Remove from list. This will decrement the ref count.
    m_pCWaveLib->RemoveProxy(this);

    // Note: We don't have to call Release() here.
    // RemoveProxy will decrement the ref count, and the calling stub
    // will do a final release before returning to the client.

    return mmRet;
}

MMRESULT CWaveProxy::waveGetPosition( LPMMTIME pmmt, UINT cbmmt )
{
    MMRESULT mmr;

    const DWORD dwParams[] =
        {
        (DWORD) m_hWave,
        (DWORD) pmmt,
        (DWORD) cbmmt
        };

    DWORD dwCode = m_bIsOutput ? IOCTL_WAVE_OUT_GET_POSITION : IOCTL_WAVE_IN_GET_POSITION;
    mmr = ((MMRESULT) ___MmIoControl(dwCode, 3, dwParams));

    return mmr;
}

void CWaveProxy::FilterWomDone(WAVECALLBACKMSG *pmsg)
{
    WAVEHDR UNALIGNED * pwh = (WAVEHDR UNALIGNED *)pmsg->dwParam1;
    pwh->dwFlags = (pwh->dwFlags & ~WHDR_INQUEUE) | WHDR_DONE;
}

void CWaveProxy::FilterWimData(WAVECALLBACKMSG *pmsg)
{
    WAVEHDR UNALIGNED * pwh = (WAVEHDR UNALIGNED *)pmsg->dwParam1;
    pwh->dwFlags = (pwh->dwFlags & ~WHDR_INQUEUE) | WHDR_DONE;

    // For input, also save bytes recorded
    pwh->dwBytesRecorded = pmsg->dwParam3;
}

