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

#define INSTANTIATE_DEBUG
#define WAVEAPI_MODULE_NAME     TEXT("WaveAPIC")

#include <wavelib.h>
#include <debug.h>

static CWaveLib *g_pCWaveLib = 0;

static CRITICAL_SECTION sgWaveLibCreateDeleteSec;

CWaveLib::CWaveLib() :
    m_bCallbackRunning(FALSE),
    m_hqWaveCallbackRead(0),
    m_hqWaveCallbackWrite(0),
    m_hDevice(INVALID_HANDLE_VALUE),
    m_hCallbackThread(NULL),
    m_hevCallbackThreadExit(NULL)
{
    InitializeCriticalSection(&m_cs);
    InitializeListHead(&m_ProxyList);
    return;
}

CWaveLib::~CWaveLib()
{
    // Clean up resources.
    DeinitCallbackInterface();
    Deinit();

    DeleteCriticalSection(&m_cs);
    return;
}

BOOL CWaveLib::Init()
{
    // Try to open the device
    m_hDevice = CreateFile (TEXT("WAM1:"),
                            GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ|FILE_SHARE_WRITE,
                            NULL, OPEN_EXISTING, 0, 0);
    return (m_hDevice != INVALID_HANDLE_VALUE);
}

BOOL CWaveLib::Deinit()
{
    // Callback thread should not be running.
    DEBUGCHK(!m_bCallbackRunning);

    // Clean up proxy list.
    DeleteProxyList();

    // Close handle to Wave API device.
    if (m_hDevice != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hDevice);
        m_hDevice = INVALID_HANDLE_VALUE;
    }

    return TRUE;
}

// Return a pointer to the global wavelib object.
CWaveLib *GetWaveLib()
{
    EnterCriticalSection(&sgWaveLibCreateDeleteSec);
    if (NULL == g_pCWaveLib)
    {
        g_pCWaveLib = new CWaveLib;
        if (!g_pCWaveLib->Init())
        {
            delete g_pCWaveLib;
            g_pCWaveLib = NULL;
        }
    }
    LeaveCriticalSection(&sgWaveLibCreateDeleteSec);

    return g_pCWaveLib;
}

BOOL WaveLibDeinit()
{
    EnterCriticalSection(&sgWaveLibCreateDeleteSec);
    if (g_pCWaveLib != NULL)
    {
        delete g_pCWaveLib;
        g_pCWaveLib = NULL;
    }
    LeaveCriticalSection(&sgWaveLibCreateDeleteSec);

    return TRUE;
}

// Initialize parts of the wavelib used for handling the wave api calls
BOOL CWaveLib::InitCallbackInterface (void)
{
    BOOL bRet;

    // If already initialized, just return
    if (m_bCallbackRunning)
    {
        return TRUE;
    }

    Lock();

    // Now try again with lock taken. This avoids a remote possibility of a race condition if two threads
    // end up trying to init at the same time.
    if (m_bCallbackRunning)
    {
        bRet = TRUE;
        goto Exit;
    }

    // Create a message queue for the kernel to callback into
    MSGQUEUEOPTIONS qopts;
    qopts.dwSize = sizeof(qopts);
    qopts.dwFlags = MSGQUEUE_NOPRECOMMIT | MSGQUEUE_ALLOW_BROKEN;
    qopts.bReadAccess = TRUE;
    qopts.cbMaxMessage = sizeof(WAVECALLBACKMSG);
    qopts.dwMaxMessages = 0;

    m_hqWaveCallbackRead = CreateMsgQueue(NULL, &qopts);
    if (m_hqWaveCallbackRead == NULL)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("InitWaveInterface: unable to create callback queue\r\n")));
        bRet = FALSE;
        goto Exit;
    }

    // Get another writeable handle to the queue
    qopts.bReadAccess = FALSE;
    m_hqWaveCallbackWrite = OpenMsgQueue( (HANDLE)GetCurrentProcessId(),
                                          m_hqWaveCallbackRead,
                                          &qopts );

    if (m_hqWaveCallbackWrite == NULL)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("InitWaveInterface: unable to create callback queue\r\n")));
        bRet = FALSE;
        goto Exit;
    }

    // Tell the kernel driver about the queue
    // (We could pass either the read or write queue down, it shouldn't matter)
    const DWORD dwParams[] =
    {
        (DWORD) m_hqWaveCallbackWrite
    };

    MMRESULT mmRet = ___MmIoControl(IOCTL_WAV_SETMSGQUEUE, 1, dwParams);
    if (mmRet != MMSYSERR_NOERROR)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("InitWaveInterface: unable to pass callback queue to driver\r\n")));
        bRet = FALSE;
        goto Exit;
    }

    // Create callback thread exit event.
    m_hevCallbackThreadExit = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == m_hevCallbackThreadExit)
    {
        DEBUGMSG(ZONE_ERROR, (
            TEXT("InitWaveInterface: ")
            TEXT("Failed to create callback thread exit event\r\n")));
        bRet = FALSE;
        goto Exit;
    }

    // Start up the callback thread
    m_hCallbackThread = CreateThread(
                            NULL, 0,
                            CWaveLib::WaveCallbackThreadEntry,
                            this, 0,
                            &m_CallbackThreadId);
    if (NULL == m_hCallbackThread)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("InitWaveInterface: unable to create callback thread\r\n")));
        bRet = FALSE;
        goto Exit;
    }

    // check, if an explicit, valid thread priority is specified in the registry. If it is, use this unchanged
    DWORD dwCallbackThreadPriority = THREAD_PRIORITY_ERROR_RETURN;
    HKEY hKey;
    LONG Result;
    Result = RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"Audio\\WaveAPI\\Callback", 0, KEY_READ, &hKey);
    if (Result == ERROR_SUCCESS)
    {
        DWORD dwType;
        DWORD dwData;
        DWORD dwSize = sizeof(DWORD);
        Result = RegQueryValueExW (hKey, (LPTSTR) TEXT("Priority256"), 0, &dwType, (LPBYTE)&dwData, &dwSize);

        if ( (Result == ERROR_SUCCESS) && (dwType == REG_DWORD) )
        {
            dwCallbackThreadPriority = dwData;
        }

        RegCloseKey(hKey);
    }

    // if no explicit priority is set by now, we default back to the relative thread priority
    // This code is pulled from CE6 code base to have wavelib thread set with application thread priority.
    if (dwCallbackThreadPriority == THREAD_PRIORITY_ERROR_RETURN )
    {
        // ensure the created thread is at least the one higher than the current thread
        // but if the current thread is normal priority, then we run higher
        dwCallbackThreadPriority= CeGetThreadPriority(GetCurrentThread());

        if ( dwCallbackThreadPriority == THREAD_PRIORITY_ERROR_RETURN )
        {
            ERRORMSG(1, (L"WaveLib: CeGetThreadPriority Error (0x%x) \r\n", GetLastError()));
            bRet = FALSE;
            goto Exit;          
        }
        
        if (dwCallbackThreadPriority > 0)
        {
            dwCallbackThreadPriority -= 1; // bump up the thread priority by one, unless it's already at 0.
        }

        // but down in the application thread priority space, bump it even higher.
        const DWORD dwMinPriority = CE_THREAD_PRIO_256_HIGHEST; // i.e. THREAD_PRIORITY_HIGHEST
        if (dwCallbackThreadPriority > dwMinPriority)
        {
            dwCallbackThreadPriority = dwMinPriority;
        }
    }
   
    if (!CeSetThreadPriority( m_hCallbackThread, dwCallbackThreadPriority ))
    {
           ERRORMSG(1, (L"WaveLib: CeSetThreadPriority Error (0x%x) \r\n", GetLastError()));
           bRet = FALSE;
           goto Exit;
    }

    m_bCallbackRunning=TRUE;
    bRet = TRUE;

Exit:

    if (!bRet)
    {
        DeinitCallbackInterface();
    }

    Unlock();
    return bRet;
}

BOOL CWaveLib::DeinitCallbackInterface()
{
    // Wait for callback thread to exit.
    if (m_hCallbackThread != NULL)
    {
        if (m_hevCallbackThreadExit != NULL)
        {
            SetEvent(m_hevCallbackThreadExit);
            do {
                if (WAIT_OBJECT_0 == WaitForSingleObject(m_hCallbackThread, 3000))
                    break;
                DEBUGMSG(ZONE_WARNING, (TEXT("CWaveLib::DeinitCallbackInterface: Warning: Callback thread is taking a long time to exit.\r\n")));
            } while (true);
        }

        CloseHandle(m_hCallbackThread);
        m_hCallbackThread = NULL;
    }

    m_bCallbackRunning = FALSE;

    if (m_hevCallbackThreadExit != NULL)
    {
        CloseHandle(m_hevCallbackThreadExit);
        m_hevCallbackThreadExit = NULL;
    }

    //
    // Clean up remaining resources.
    //

    if (m_hqWaveCallbackRead != NULL)
    {
        CloseMsgQueue(m_hqWaveCallbackRead);
        m_hqWaveCallbackRead = NULL;
    }

    if (m_hqWaveCallbackWrite != NULL)
    {
        CloseMsgQueue(m_hqWaveCallbackWrite);
        m_hqWaveCallbackWrite = NULL;
    }

    return TRUE;
}

// Post a message to the callback thread.
BOOL CWaveLib::QueueCallback(WAVECALLBACKMSG *pMsg)
{
    return WriteMsgQueue(m_hqWaveCallbackWrite, pMsg, sizeof(WAVECALLBACKMSG), 0, 0);
}

// There are four ways the app handle<->proxy ptr might be managed:
// 1. Use the handle returned by the kernel, and keep a list/hash table locally and search the table to map
//    to the proxy object.
// 2. Use the handle returned by the kernel, and create an API into the kernel to get back the proxy given a handle.
// 3. Implement a pseudo-handle locally in the client code, and pass the pseudo-handle to the application rather than
//    the kernel handle.
// 4. Return the proxy ptr as a handle.
// For the moment, I've done #1 using a simple linked list. We should reexamine this later.

// Add a proxy to the list. Return the kernel handle.
HWAVE CWaveLib::AddProxy(CWaveProxy *pProxy)
{
    Lock();

    // Takes a ref on the proxy
    pProxy->AddRef();
    InsertHeadList(&m_ProxyList,&pProxy->m_Link);

    Unlock();

    // For now, we just return the same wave handle that the proxy has associated with it.
    return pProxy->GetWaveHandle();
}

BOOL CWaveLib::RemoveProxy(CWaveProxy *pProxy)
{
    Lock();

    // Releases a ref on the proxy
    RemoveEntryList(&pProxy->m_Link);
    pProxy->Release();
    Unlock();

    return TRUE;
}

// Remove all entries from the proxy list.
BOOL CWaveLib::DeleteProxyList()
{
    Lock();

    while (!IsListEmpty(&m_ProxyList))
    {
        PLIST_ENTRY pListEntry = RemoveHeadList(&m_ProxyList);
        CWaveProxy *pProxy = CONTAINING_RECORD(
                                    pListEntry,
                                    CWaveProxy,
                                    m_Link);

        pProxy->Release();
    }

    Unlock();

    return TRUE;
}

// Given a application wave handle, figure out which proxy object it is associated with.
// If found, takes a ref and returns a pointer to the proxy.
// Return NULL if proxy not found.
CWaveProxy *CWaveLib::ProxyFromHandle(HANDLE hWave, BOOL bOutput)
{
    CWaveProxy *pProxy = NULL;

    Lock();

    // Takes a ref on the proxy
    PLIST_ENTRY pListEntry;

    for (pListEntry = m_ProxyList.Flink;
         pListEntry != &m_ProxyList;
         pListEntry = pListEntry->Flink )
    {
        CWaveProxy *pTestProxy;

        pTestProxy = CONTAINING_RECORD(pListEntry,CWaveProxy,m_Link);

        if ( (pTestProxy->GetWaveHandle()==hWave) && (pTestProxy->IsOutput() == bOutput) )
        {
            pProxy=pTestProxy;
            pProxy->AddRef();
            break;
        }
    }

    Unlock();

    return pProxy;
}

HINSTANCE g_hInstance = NULL;     // instance handle

BOOL WINAPI DllMain(HANDLE hInstance, DWORD dwReason, LPVOID lpvReserved)
{
    switch (dwReason)
        {
        case DLL_PROCESS_ATTACH :
            g_hInstance = (HINSTANCE)hInstance;
            DEBUGREGISTER((HINSTANCE)hInstance);

            InitializeCriticalSection(&sgWaveLibCreateDeleteSec);

            acm_Initialize();
            acmui_Initialize();
            SPS_Init();

            DisableThreadLibraryCalls((HMODULE) hInstance);
            break;

        case DLL_PROCESS_DETACH :
            SPS_DeInit();
            acmui_Terminate();
            acm_Terminate();

            // Deinit in this order because WaveLibDeinit has dependency
            // on the Audio Routing Manager.
            WaveLibDeinit();

            DeleteCriticalSection(&sgWaveLibCreateDeleteSec);

            break;

        default :
            break;
        }
    return TRUE;
}

//------------------------------------------------------------------------------
// CopyWaveformat
//
// Makes a safe and proper copy of a process waveformat
//------------------------------------------------------------------------------
LPWAVEFORMATEX CopyWaveformat(LPCWAVEFORMATEX pwfxSrc)
{
    PWAVEFORMATEX pwfxNew=NULL;

    if (!pwfxSrc)
        {
        goto Exit;
        }

    __try
    {
        DWORD cbCopySize;
        DWORD cbAllocSize;
        WORD cbExtraSize;

        // There's a slight hack here, not sure why, it was always there.
        // Basically, a PCM format only uses a PCMWAVEFORMAT structure, which
        // doesn't include the extra cbSize field that WAVEFORMATEX has.
        // A side effect of the copy code below converts all structures to
        // WAVEFORMATEX by always allocating space for the cbSize field and
        // setting it to 0 in the case of PCM formats.
        // That's why the Alloc and Copy sizes are different for PCM, and why
        // we set the cbSize field near the end of the function.

        if (pwfxSrc->wFormatTag == WAVE_FORMAT_PCM)
            {
            cbExtraSize = 0;

            cbAllocSize = sizeof (WAVEFORMATEX);
            cbCopySize = sizeof (PCMWAVEFORMAT);
            }
        else
            {
            // If we're not PCM format, then we need to be able to access
            // a WAVEFORMATEX struct instead, along with any extra custom bytes
            cbExtraSize = pwfxSrc->cbSize;

            cbAllocSize = sizeof(WAVEFORMATEX) + cbExtraSize;
            cbCopySize = cbAllocSize;
            }

        // Set an arbitrary limit on the size of wave headers
        if (cbAllocSize > 4096)
            {
            goto Exit;
            }

        pwfxNew = (PWAVEFORMATEX) LocalAlloc(LMEM_FIXED, cbAllocSize);
        if (!pwfxNew)
            {
            goto Exit;
            }

        // Don't need CeSafeCopyMemory here, we're already in a try/except block
        memcpy (pwfxNew, pwfxSrc, cbCopySize);

        pwfxNew->cbSize = cbExtraSize;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        if (pwfxNew)
            {
            LocalFree(pwfxNew);
            pwfxNew=NULL;
            }
    }

    Exit:
    return pwfxNew;
}


// Send a message to the callback queue and wait for it to be processed.
// This enforces a synchronization/rendevous with the callback messages and ensures
// that any pending callbacks are processed before we return
BOOL CWaveLib::WaitForMsgQueue(HANDLE hEvent, DWORD dwTimeout)
{
    // Make sure we're not running _on_ the callback thread.
    // This helps prevent deadlocks if callbacks reenter us
    if (GetCurrentThreadId()==m_CallbackThreadId)
    {
        DEBUGMSG(ZONE_WARNING, (TEXT("CWaveLib::WaitForMsgQueue: Warning: Called on callback thread, skipping wait\r\n")));
        return FALSE;
    }

    BOOL bRet;
    WAVECALLBACKMSG msg;
    msg.dwInstance = (DWORD) hEvent;
    msg.uMsg = WAVECALLBACK_MSG_SYNC;

    bRet = QueueCallback(&msg);

    if (bRet)
    {
        DWORD dwResult = WaitForSingleObject(hEvent, dwTimeout);
        if (dwResult == WAIT_TIMEOUT )
        {
            DEBUGMSG(ZONE_WARNING, (TEXT("CWaveLib::WaitForMsgQueue: Timed out waiting to synchronize with wave callback\r\n")));
        }
    }
    else
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("WaitForMsgQueue: WriteMsgQueue failed\r\n")));
    }

    return bRet;
}

// Entry point for callback proxy thread
DWORD
CWaveLib::WaveCallbackThreadEntry (PVOID pArg)
{
    CWaveLib *pWaveLib = (CWaveLib *)pArg;
    pWaveLib->WaveCallbackThread();
    return 0;
}

// Helper class for handling device ID change callback.
class DeviceIdChangeCallbackInfo
{
public:

    DeviceIdChangeCallbackInfo(
        CWaveProxy *pProxy,
        UINT uMsg,
        DWORD dwParam1,
        DWORD dwParam2);

    virtual ~DeviceIdChangeCallbackInfo();

    VOID DoClientCallback();

    LIST_ENTRY m_Link;      // Link

private:

    CWaveProxy *m_pProxy;   // Proxy object
    UINT m_uMsg;            // Callback message
    DWORD m_dwParam1;       // Param1
    DWORD m_dwParam2;       // Param2
};

DeviceIdChangeCallbackInfo::DeviceIdChangeCallbackInfo(
    CWaveProxy *pProxy,
    UINT uMsg,
    DWORD dwParam1,
    DWORD dwParam2
    ) :
    m_uMsg(uMsg),
    m_dwParam1(dwParam1),
    m_dwParam2(dwParam2)
{
    DEBUGCHK(pProxy != NULL);
    m_pProxy = pProxy;
    m_pProxy->AddRef();

    InitializeListHead(&m_Link);
}

DeviceIdChangeCallbackInfo::~DeviceIdChangeCallbackInfo()
{
    m_pProxy->Release();
}

VOID
DeviceIdChangeCallbackInfo::DoClientCallback()
{
    m_pProxy->DoClientCallback(m_uMsg, m_dwParam1, m_dwParam2);
}

// Proxy thread to get messages from the kernel waveapi code, do client-process processing,
// and pass the message onto the app via the app's requested callback mechanism
DWORD
CWaveLib::WaveCallbackThread ()
{
    static const DWORD c_cWaitEvents = 2;
    HANDLE rghevWait[c_cWaitEvents];
    rghevWait[0] = m_hqWaveCallbackRead;
    rghevWait[1] = m_hevCallbackThreadExit;

    BOOL fThreadExit = FALSE;
    while (!fThreadExit)
    {
        DWORD dwWaitResult = WaitForMultipleObjects(
                                    c_cWaitEvents,
                                    rghevWait,
                                    FALSE,
                                    INFINITE);

        switch (dwWaitResult)
        {
        case WAIT_OBJECT_0:         // Callback msg queue.
            {
                WAVECALLBACKMSG msg;
                DWORD dwBytesRead;
                DWORD dwFlags;

                // Read a message out of the queue
                if (!ReadMsgQueue(
                        m_hqWaveCallbackRead,
                        &msg,
                        sizeof(msg),
                        &dwBytesRead, 0, &dwFlags))
                {
                    DEBUGMSG(ZONE_ERROR, (TEXT("WaveCallbackThread: ReadMsgQueue failed\r\n")));
                    break;
                }

                if (dwBytesRead != sizeof(msg))
                {
                    DEBUGMSG(ZONE_ERROR, (TEXT("WaveCallbackThread: unexpected message size\r\n")));
                    break;
                }

                __try
                {
                    // Special handling for buffer done messages
                    // This is the main reason we moved all this code up to the client- we can't touch
                    // these fields in the kernel without remapping the memory, which is expensive and more
                    // dangerous.
                    switch (msg.uMsg)
                    {
                        // update the client hdr fields that have legitimately changed.
                        // Clear INQUEUE and set DONE bits in header
                        case WOM_DONE:
                        {
                            CWaveProxy *pProxy = (CWaveProxy *) msg.dwInstance;
                            pProxy->FilterWomDone(&msg);
                            pProxy->DoClientCallback(msg.uMsg, msg.dwParam1, msg.dwParam2);
                            break;
                        }
                        case WIM_DATA:
                        {
                            CWaveProxy *pProxy = (CWaveProxy *) msg.dwInstance;
                            pProxy->FilterWimData(&msg);
                            pProxy->DoClientCallback(msg.uMsg, msg.dwParam1, msg.dwParam2);
                            break;
                        }
                        case WOM_CLOSE:
                        case WIM_CLOSE:
                        {
                            CWaveProxy *pProxy = (CWaveProxy *) msg.dwInstance;
                            pProxy->DoClientCallback(msg.uMsg, msg.dwParam1, msg.dwParam2);
                            break;
                        }
                        case WAVECALLBACK_MSG_SYNC:
                        {
                            SetEvent((HANDLE)msg.dwInstance);
                            break;
                        }
                        case WAVECALLBACK_MSG_UPDATESETTINGS:
                        {
                            PlaySound_UpdateSettings();
                            break;
                        }
                        case MM_MIXM_LINE_CHANGE:
                        case MM_MIXM_CONTROL_CHANGE:
                        {
                            // For mixer callbacks, dwInstance will be the window handle to post the message to and dwParam3 will be the mixer handle
                            PostMessage ((HWND) msg.dwInstance, msg.uMsg, msg.dwParam3, msg.dwParam1);
                            break;
                        }
                        default:
                        {
                            DEBUGMSG(ZONE_ERROR, (TEXT("WaveAPI: callback unrecognized!\r\n")));
                            break;
                        }
                    }
                }
                __except(EXCEPTION_EXECUTE_HANDLER)
                {
                    DEBUGMSG(ZONE_ERROR, (TEXT("WaveAPI: callback exception!\r\n")));
                }
            }
            break;

        case WAIT_OBJECT_0 + 1:     // Thread exit event.
            fThreadExit = TRUE;
            break;

        default:
            // Unexpected error.
            fThreadExit = TRUE;
            DEBUGMSG(ZONE_ERROR, (
                TEXT("WaveCallbackThread: ")
                TEXT("Unexpected wait result, %d.  Exiting thread.\r\n"),
                dwWaitResult));
            DEBUGCHK(0);
            break;
        }
    }

    m_bCallbackRunning = FALSE;
    return 0;
}

// Helper functions
DWORD ___MmIoControl(DWORD dwCode, int nParams, const DWORD dwParams[])
{
    DWORD dwRet = MMSYSERR_NODRIVER;
    DWORD dwTmp;

    CWaveLib *pCWaveLib = GetWaveLib();
    if (pCWaveLib)
    {
        if (!pCWaveLib->MmDeviceIoControl (dwCode,
                           (LPVOID) dwParams,
                           nParams * sizeof(DWORD),
                           &dwRet,
                           sizeof(DWORD),
                           &dwTmp))
        {
            dwRet = MMSYSERR_ERROR;
        }
    }

    return dwRet;
}

