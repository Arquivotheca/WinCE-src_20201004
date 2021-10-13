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
#include <windows.h>
#include <Msgqueue.h>
#include "logging.h"
#include "ccamerastreamtest.h"
#include "camera.h"
#include <initguid.h>
#include <uuids.h>

#define MSGQUEUE_WAITTIME 100
TCHAR g_tszClassName[] = TEXT("CamTest");

CStreamTest::CStreamTest()
{
    m_hCamStream = INVALID_HANDLE_VALUE;
    m_hStreamMsgQueue = NULL;
    m_hASyncThreadHandle = NULL;
    m_hwndMain = NULL;
    m_hdcMain = NULL;
    m_pCSDataFormat = NULL;
    m_dwAllocationType = CSPROPERTY_BUFFER_CLIENT_LIMITED;
    m_dwDriverRequestedBufferCount = 3;
    m_pcsBufferNode = NULL;
    m_wBitCount = 0;
    m_lImageWidth = 0;
    m_lImageHeight = 0;
    m_csState = CSSTATE_STOP;
    memset(&m_rcClient, 0, sizeof(RECT));
    m_hMutex = NULL;
    m_pbmiBitmapDataFormat = NULL;
    m_nArtificialDelay = 0;
    m_nPictureNumber = 1;
    m_lSampleScannedNotifsReceived = 0;
    m_lPreviousPresentationTime = 0;
    m_lExpectedDuration = 0;
    m_dwExpectedDataSize = 0;
    m_fExpectZeroPresentationTime = FALSE;
    m_lASyncThreadCount = 0;
    m_sPinType = 0;
}

CStreamTest::~CStreamTest()
{
    Cleanup();
}

BOOL
CStreamTest::Cleanup()
{
    // keep state changes synchronous, set the state to stopped before cleanup,
    // make sure we're initialized before doing it though.
    if(m_hCamStream != INVALID_HANDLE_VALUE)
        SetState(CSSTATE_STOP);

    if(NULL != m_pCSDataFormat)
    {
        delete[] m_pCSDataFormat;
        m_pCSDataFormat = NULL;
    }

    ReleaseBuffers();

    SAFECLOSEFILEHANDLE(m_hCamStream);
    SAFECLOSEHANDLE(m_hStreamMsgQueue);
    SAFECLOSEHANDLE(m_hASyncThreadHandle);
    SAFECLOSEHANDLE(m_hMutex);

    if(m_hdcMain)
    {
        ReleaseDC(m_hwndMain, m_hdcMain);
    }

    if(m_hwndMain)
    {
        DestroyWindow(m_hwndMain);
        UnregisterClass(g_tszClassName, NULL);
    }
    m_hdcMain = NULL;

    return TRUE;
}

// create the message queue and give it to the driver
BOOL CStreamTest::CreateStream(ULONG ulPinId, TCHAR *szStreamName, SHORT sPinType)
{
    if(NULL == szStreamName)
        return FALSE;

    if(INVALID_HANDLE_VALUE != m_hCamStream)
        return TRUE;

    m_hCamStream = CreateFile(szStreamName,
                                GENERIC_READ|GENERIC_WRITE,
                                0,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);

    if(INVALID_HANDLE_VALUE == m_hCamStream)
    {
        return FALSE;
    }

    m_hMutex = CreateMutex(NULL, FALSE, NULL);

    if(NULL == m_hMutex)
    {
        SAFECLOSEFILEHANDLE(m_hCamStream);
        return FALSE;
    }

    MSGQUEUEOPTIONS msgQOptions;

    msgQOptions.dwSize = sizeof(MSGQUEUEOPTIONS);
    msgQOptions.dwFlags = 0;
    msgQOptions.dwMaxMessages = 10;
    msgQOptions.cbMaxMessage = sizeof(CS_MSGQUEUE_BUFFER);
    msgQOptions.bReadAccess = TRUE;

    m_hStreamMsgQueue = CreateMsgQueue(NULL, &msgQOptions);
    if(NULL == m_hStreamMsgQueue)
    {
        WARN(TEXT("GetFrameAllocationInfoForStream : TestDeviceIOControl failed. Unable to get CSPROPERTY_STREAM_ALLOCATOR  "));
        SAFECLOSEFILEHANDLE(m_hCamStream);
        SAFECLOSEHANDLE(m_hMutex);
        return FALSE;
    }

    DWORD dwBytesReturned = 0;
    CSPROPERTY_STREAMEX_S csPropStreamEx;
    
    csPropStreamEx.CsPin.Property.Set = CSPROPSETID_StreamEx;
    csPropStreamEx.CsPin.Property.Id = CSPROPERTY_STREAMEX_INIT;
    csPropStreamEx.CsPin.PinId = ulPinId;
    csPropStreamEx.hMsgQueue = m_hStreamMsgQueue;
    
    if(FALSE == TestStreamDeviceIOControl(IOCTL_STREAM_INSTANTIATE,
                                                (LPVOID)&csPropStreamEx,
                                                sizeof(CSPROPERTY_STREAMEX_S),
                                                NULL,
                                                0,
                                                &dwBytesReturned,
                                                NULL))
    {
        SAFECLOSEFILEHANDLE(m_hCamStream);
        SAFECLOSEHANDLE(m_hMutex);
        CloseMsgQueue(m_hStreamMsgQueue);
        m_hStreamMsgQueue = NULL;
        return FALSE;
    }

    m_ulPinId = ulPinId;
    m_sPinType = sPinType;
    
    return TRUE;
}

BOOL CStreamTest::TestStreamDeviceIOControl(DWORD dwIoControlCode, 
                                                LPVOID lpInBuffer, 
                                                DWORD nInBufferSize, 
                                                LPVOID lpOutBuffer,  
                                                DWORD nOutBufferSize,  
                                                LPDWORD lpBytesReturned,  
                                                LPOVERLAPPED lpOverlapped)
{
    if(INVALID_HANDLE_VALUE == m_hCamStream)
        return FALSE;

    if(!DeviceIoControl(m_hCamStream, 
                        dwIoControlCode, 
                        lpInBuffer, 
                        nInBufferSize, 
                        lpOutBuffer,  
                        nOutBufferSize,  
                        lpBytesReturned,  
                        lpOverlapped))
    {
        return FALSE;
    }

    return TRUE;
}

// retrieve general property information from the driver, including frame and allocation information
BOOL CStreamTest::GetFrameAllocationInfoForStream()
{
    DWORD dwBytesReturned = 0;
    CSPROPERTY csProp;

    csProp.Set = CSPROPSETID_Connection;
    csProp.Id  = CSPROPERTY_CONNECTION_ALLOCATORFRAMING;
    csProp.Flags = CSPROPERTY_TYPE_GET;

    
    if(FALSE == TestStreamDeviceIOControl (IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof(csProp), 
                                            &m_CSAllocatorFraming, 
                                            sizeof(m_CSAllocatorFraming), 
                                            &dwBytesReturned,
                                            NULL))
    {
        FAIL(TEXT("GetFrameAllocationInfoForStream : TestDeviceIOControl failed. Unable to get CSPROPERTY_CONNECTION_ALLOCATORFRAMING  "));
        return FALSE;
    }

    m_dwDriverRequestedBufferCount = m_CSAllocatorFraming.Frames;
    m_dwAllocationType = m_CSAllocatorFraming.RequirementsFlags;

    return TRUE;
}

BOOL CStreamTest::FlushMsgQueue()
{
    CS_MSGQUEUE_BUFFER csMsgQBuffer;
    DWORD dwFlags = 0;
    DWORD dwBytesRead = 0;

    if(!m_hStreamMsgQueue)
        return FALSE;

    memset(&csMsgQBuffer, 0x0, sizeof(csMsgQBuffer));
    while(TRUE == ReadMsgQueue(m_hStreamMsgQueue, &csMsgQBuffer, sizeof(csMsgQBuffer), &dwBytesRead, 0, &dwFlags))
    {
        if(ERROR_TIMEOUT == GetLastError())
            break;
    }

    return TRUE;
}

BOOL CStreamTest::SetFormat(PCSDATAFORMAT pCSDataFormat, size_t DataFormatSize)
{
    DWORD dwBytesReturned = 0;
    DWORD dwFormatSize    = 0;
    CSPROPERTY csProp;

    // we're given DataFormatSize as the size of the buffer passed in, there's nothing much we can to about validating it
    // beyond making sure it's not 0.
    PREFAST_ASSUME(DataFormatSize);

    WaitForSingleObject(m_hMutex, INFINITE);

    if(NULL == pCSDataFormat)
    {
        FAIL(TEXT("SetFormat : NULL parameter passed."));
        ReleaseMutex(m_hMutex);
        return FALSE;
    }

    if(0 == DataFormatSize)
    {
        FAIL(TEXT("SetFormat : invalid size passed."));
        ReleaseMutex(m_hMutex);
        return FALSE;
    }

    csProp.Set = CSPROPSETID_Connection;
    csProp.Id = CSPROPERTY_CONNECTION_DATAFORMAT;
    csProp.Flags = CSPROPERTY_TYPE_SET;

    if(CSDATAFORMAT_SPECIFIER_VIDEOINFO == pCSDataFormat->Specifier)
    {
        dwFormatSize = sizeof(CS_DATARANGE_VIDEO);
    }
    else if(CSDATAFORMAT_SPECIFIER_VIDEOINFO2 == pCSDataFormat->Specifier)
    {
        dwFormatSize = sizeof(CS_DATARANGE_VIDEO2);
    }
    else
    {
        FAIL(TEXT("SetFormat : Unknown Format."));
        ReleaseMutex(m_hMutex);
        return FALSE;
    }
    
    if(FALSE == TestStreamDeviceIOControl (IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof(CSPROPERTY), 
                                            pCSDataFormat, 
                                            dwFormatSize, 
                                            &dwBytesReturned,
                                            NULL))
    {
        FAIL(TEXT("SetFormat : TestDeviceIOControl failed. Unable to get CSPROPERTY_STREAM_ALLOCATOR  "));
        ReleaseMutex(m_hMutex);
        return FALSE;
    }

    if(m_pCSDataFormat)
        delete[] m_pCSDataFormat;
    m_pCSDataFormat = NULL;

    // grab a copy of the pointer
    m_pCSDataFormat = reinterpret_cast< PCSDATAFORMAT > (new BYTE[ DataFormatSize ]);

    if(NULL == m_pCSDataFormat)
    {
        FAIL(TEXT("SetFormat : Unknown Format or out of memory."));
        ReleaseMutex(m_hMutex);
        return FALSE;
    }
    memcpy(m_pCSDataFormat, pCSDataFormat, DataFormatSize);

    if(CSDATAFORMAT_SPECIFIER_VIDEOINFO == m_pCSDataFormat->Specifier)
    {
        PCS_DATAFORMAT_VIDEOINFOHEADER pCSDataFormatVidInfoHdr = reinterpret_cast<PCS_DATAFORMAT_VIDEOINFOHEADER>(m_pCSDataFormat);
        m_wBitCount = pCSDataFormatVidInfoHdr->VideoInfoHeader.bmiHeader.biBitCount;
        m_lImageWidth = pCSDataFormatVidInfoHdr->VideoInfoHeader.bmiHeader.biWidth;
        m_lImageHeight = abs(pCSDataFormatVidInfoHdr->VideoInfoHeader.bmiHeader.biHeight);
        // the video info header has a bmih followed by color table info, so it's basically a bitmapinfo
        m_pbmiBitmapDataFormat = (BITMAPINFO *) &(pCSDataFormatVidInfoHdr->VideoInfoHeader.bmiHeader);
    }
    else if(CSDATAFORMAT_SPECIFIER_VIDEOINFO2 == m_pCSDataFormat->Specifier)
    {
        PCS_DATAFORMAT_VIDEOINFOHEADER2 pCSDataFormatVidInfoHdr2 = reinterpret_cast<PCS_DATAFORMAT_VIDEOINFOHEADER2>(m_pCSDataFormat);
        m_wBitCount = pCSDataFormatVidInfoHdr2->VideoInfoHeader2.bmiHeader.biBitCount;
        m_lImageWidth = pCSDataFormatVidInfoHdr2->VideoInfoHeader2.bmiHeader.biWidth;
        m_lImageHeight = abs(pCSDataFormatVidInfoHdr2->VideoInfoHeader2.bmiHeader.biHeight);
        // the video info header has a bmih followed by color table info, so it's basically a bitmapinfo
        m_pbmiBitmapDataFormat = (BITMAPINFO *) &(pCSDataFormatVidInfoHdr2->VideoInfoHeader2.bmiHeader);
    }

    // we successfully changes the format, so flush the message queue so we don't process frames incorrectly.
    FlushMsgQueue();

    ReleaseMutex(m_hMutex);

    return TRUE;

}

BOOL CStreamTest::CreateCameraWindow(HWND hwnd, RECT rc)
{
    if(m_hwndMain)
        return TRUE;

    WNDCLASS wc;

    WaitForSingleObject(m_hMutex, INFINITE);

    memset(&wc, 0, sizeof(WNDCLASS));
    wc.lpfnWndProc = (WNDPROC)DefWindowProc;
    wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = g_tszClassName;

    // when there are multiple instances of this class, this register class may fail.
    RegisterClass(&wc);

    // create a window that fills the work area.
    // if we're given a parent window, then make this as a child, otherwise make it a primary
    m_hwndMain = CreateWindowEx(0, 
                                    g_tszClassName, 
                                    NULL, 
                                    (hwnd?WS_CHILD:0) | WS_BORDER, 
                                    rc.left, 
                                    rc.top, 
                                    rc.right - rc.left,
                                    rc.bottom - rc.top, 
                                    hwnd, 
                                    NULL, 
                                    NULL/*globalInst*/, 
                                    NULL);

    if(NULL == m_hwndMain)
    {
        FAIL(TEXT("CreateCameraWindow : CreateWindowEx failed."));
        UnregisterClass(g_tszClassName, NULL);
        ReleaseMutex(m_hMutex);
        return FALSE;
    }

    ShowWindow(m_hwndMain, SW_SHOWNORMAL);

    m_hdcMain = GetDC(m_hwndMain);

    if(NULL == m_hdcMain)
    {
        FAIL(TEXT("CreateCameraWindow : GetDC failed."));
        ReleaseMutex(m_hMutex);
        return FALSE;
    }

    if(!GetClientRect(m_hwndMain, &m_rcClient))
    {
        FAIL(TEXT("CreateCameraWindow : GetClientRect failed."));
        ReleaseMutex(m_hMutex);
        return FALSE;
    }

    ReleaseMutex(m_hMutex);

    return TRUE;
}

BOOL CStreamTest::SetState(CSSTATE csState)
{
    DWORD dwBytesReturned = 0;
    CSPROPERTY csProp;

    // keep state changes synchronous.
    WaitForSingleObject(m_hMutex, INFINITE);

    // if we're transitioning from the pause state, then reset time
    if(csState == CSSTATE_RUN && m_csState == CSSTATE_PAUSE)
    {
        m_lPreviousPresentationTime = 0;
        m_fExpectZeroPresentationTime = TRUE;
    }

    csProp.Set = CSPROPSETID_Connection;
    csProp.Id = CSPROPERTY_CONNECTION_STATE;
    csProp.Flags = CSPROPERTY_TYPE_SET;

    if(FALSE == TestStreamDeviceIOControl (IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof(CSPROPERTY), 
                                            &csState, 
                                            sizeof(CSSTATE), 
                                            &dwBytesReturned,
                                            NULL))
    {
        WARN(TEXT("SetState : TestDeviceIOControl failed. Unable to set CSPROPERTY_CONNECTION_STATE  "));
        ReleaseMutex(m_hMutex);
        return FALSE;
    }

//    if (m_sPinType != STREAM_STILL)
//    {
    if (csState == CSSTATE_STOP)
    {    
        if(FALSE == ReleaseBuffers())
        {
            SKIP(TEXT("SetState : ReleaseBuffers failed"));
            return FALSE;
        }
    }

    // if we're going from stop to pause, we need to allocate the buffers
    if (m_csState == CSSTATE_STOP && csState == CSSTATE_PAUSE)
    {
        if(FALSE == AllocateBuffers())
        {
            SKIP(TEXT("SetState : AllocateBuffers failed"));
            return FALSE;
        }
        Sleep(5000);
        CreateAsyncThread();
    }
//    }

    // setting the state succeeded, save off the new current state so we know whether we're running, stopped, or paused.
    // The still pin should succeed this but not change state.
    if (!(csState == CSSTATE_RUN && m_sPinType == STREAM_STILL))
    {
        m_csState = csState;
    }

    ReleaseMutex(m_hMutex);

    // Wait for the io handler thread to finish off it's operations
    Sleep(5000);
    
    return TRUE;
}

CSSTATE CStreamTest::GetState()
{
    CSSTATE csState = m_csState;

    DWORD dwBytesReturned = 0;
    CSPROPERTY csProp;

    // keep state changes synchronous.
    WaitForSingleObject(m_hMutex, INFINITE);

    // if we're transitioning to the stop state, then we want to reset out
    // verification variables for the frame count & presentation times.
    if(csState == CSSTATE_STOP)
    {
        m_nPictureNumber = 1;
        m_lPreviousPresentationTime = 0;
        m_lExpectedDuration = 0;
        m_dwExpectedDataSize = 0;
        m_lSampleScannedNotifsReceived = 0;
    }

    csProp.Set = CSPROPSETID_Connection;
    csProp.Id = CSPROPERTY_CONNECTION_STATE;
    csProp.Flags = CSPROPERTY_TYPE_GET;

    if(FALSE == TestStreamDeviceIOControl (IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof(CSPROPERTY), 
                                            &csState, 
                                            sizeof(CSSTATE), 
                                            &dwBytesReturned,
                                            NULL))
    {
        FAIL(TEXT("SetState : TestDeviceIOControl failed. Unable to get CSPROPERTY_CONNECTION_STATE  "));
        ReleaseMutex(m_hMutex);
        return m_csState;
    }

    // setting the state succeeded, save off the new current state so we know whether we're running, stopped, or paused.
    if(m_csState != csState)
    {
        FAIL(TEXT("SetState : Expected pin state is not what was retrieved. "));
    }

    ReleaseMutex(m_hMutex);

    return csState;
}

BOOL CStreamTest::SetBufferCount(DWORD dwNewBufferCount)
{
    BOOL bReturnValue = TRUE;
    CSPROPERTY csProp;
    DWORD dwBytesReturned;
    CSALLOCATOR_FRAMING csFraming;

    memcpy(&csFraming, &m_CSAllocatorFraming, sizeof(CSALLOCATOR_FRAMING));
    csFraming.Frames = dwNewBufferCount;

    csProp.Set = CSPROPSETID_Connection;
    csProp.Id  = CSPROPERTY_CONNECTION_ALLOCATORFRAMING;
    csProp.Flags = CSPROPERTY_TYPE_SET;

     // get the requested number of buffers from the driver
    if(FALSE == TestStreamDeviceIOControl (IOCTL_CS_PROPERTY, 
                                                &csProp, 
                                                sizeof(CSPROPERTY), 
                                                &csFraming, 
                                                sizeof(CSALLOCATOR_FRAMING), 
                                                &dwBytesReturned,
                                                NULL))
    {
        FAIL(TEXT("SetBufferCount : TestDeviceIOControl failed. Unable to set allocator framing. "));
        bReturnValue = FALSE;
    }

    return bReturnValue;
}

BOOL CStreamTest::CreateAsyncThread()
{
    DWORD dwThreadID = 0;

    WaitForSingleObject(m_hMutex, INFINITE);

    m_hASyncThreadHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ASyncIOThread, (LPVOID)this, 0, &dwThreadID);
    if(NULL == m_hASyncThreadHandle)
    {
        FAIL(TEXT("CreateAsyncThread : Could not create Status Thread"));
        return FALSE;
    }

    ReleaseMutex(m_hMutex);

    return TRUE;
}

BOOL CStreamTest::CleanupASyncThread()
{
    // make sure the async thread is stopped (it exits when it's stopped)
    SetState(CSSTATE_STOP);

    // now close out it's handle;
    SAFECLOSEHANDLE(m_hASyncThreadHandle);

    return TRUE;
}

BOOL CStreamTest::SetAritificalDelay(int nDelay)
{
    WaitForSingleObject(m_hMutex, INFINITE);
    m_nArtificialDelay = nDelay;
    ReleaseMutex(m_hMutex);
    return TRUE;
}

DWORD WINAPI CStreamTest::ASyncIOThread(LPVOID lpVoid)
{
    PCAMERASTREAMTEST pCamStreamTest = reinterpret_cast<PCAMERASTREAMTEST>(lpVoid);
    if(NULL != lpVoid)
        return pCamStreamTest->StreamIOHandler();
    else
        return THREAD_FAILURE;
}

DWORD WINAPI CStreamTest::StreamIOHandler()
{
    DWORD dwTimeOut = 0;
    DWORD dwBytesRead = 0;
    DWORD dwFlags = 0;
    int iNoOfAttempts = 0;
    CS_MSGQUEUE_BUFFER csMsgQBuffer;
    
    
    if(NULL == m_hStreamMsgQueue)
    {
        WARN(TEXT("AsyncIOThread() : "));
        return THREAD_FAILURE;
    }

    if (InterlockedIncrement(&m_lASyncThreadCount) > 1)
    {
        InterlockedDecrement(&m_lASyncThreadCount);
        return 0;
        DebugBreak();
    }

    do
    {
        // wait for something from the message queue.  If it doesn't come within the timeout
        // see if there was a state change.
        dwTimeOut = WaitForSingleObject (m_hStreamMsgQueue, MSGQUEUE_WAITTIME);

        // hold the mutex when using class state variables.
        WaitForSingleObject(m_hMutex, INFINITE);
        if(WAIT_TIMEOUT == dwTimeOut)
        {
            // if we're in the run state and the queue timed out, 
            // then output a warning that it did.  Still continue waiting though
            if(m_csState == CSSTATE_RUN)
            {
                // if we weren't told to stop, then just wait until we are.
                WARN(TEXT("ASyncIOThread : Timed Out"));
            }
            
            // if we stopped, just exit the io handler
            if(m_csState == CSSTATE_STOP)// && m_sPinType != STREAM_STILL)
            {
                // Make sure the buffer queue is completely flushed
                while(FALSE != ReadMsgQueue(m_hStreamMsgQueue, &csMsgQBuffer, sizeof(csMsgQBuffer), &dwBytesRead, MSGQUEUE_WAITTIME * 5, &dwFlags))
                {
                    Log(TEXT("ASyncIOThread : Received buffer from queue while in stop state. Throwing out."));
                }

                m_nPictureNumber = 1;
                m_lPreviousPresentationTime = 0;
                m_lExpectedDuration = 0;
                m_dwExpectedDataSize = 0;
                m_nlReceivedNotifications.clear();
                m_lSampleScannedNotifsReceived = 0;
                ReleaseMutex(m_hMutex);
                break;
            }

            ReleaseMutex(m_hMutex);
            // whether we're paused or run, continue waiting on the queue.
            continue;
        }


        // Read Msg.
        memset(&csMsgQBuffer, 0x0, sizeof(csMsgQBuffer));
        if(FALSE == ReadMsgQueue(m_hStreamMsgQueue, &csMsgQBuffer, sizeof(csMsgQBuffer), &dwBytesRead, MSGQUEUE_WAITTIME, &dwFlags))
        {
            ERRFAIL(TEXT("ASyncIOThread : ReadMsgQueue returned FALSE "));
            ReleaseMutex(m_hMutex);
            break;
        }

        PCS_MSGQUEUE_HEADER pCSMsgQHeader = NULL;
        PCS_STREAM_DESCRIPTOR pStreamDescriptor = NULL;
        PCSSTREAM_HEADER pCsStreamHeader = NULL;

        pCSMsgQHeader = &csMsgQBuffer.CsMsgQueueHeader;
        if(NULL == pCSMsgQHeader)
        {
            ERRFAIL(TEXT("ASyncIOThread : MessageQueue Header is NULL"));
            ReleaseMutex(m_hMutex);
            break;
        }
        
        if(FLAG_MSGQ_ASYNCHRONOUS_FOCUS == pCSMsgQHeader->Flags ||
            FLAG_MSGQ_SAMPLE_SCANNED == pCSMsgQHeader->Flags)
        {
            // We've received a notification, track it.
            NOTIFICATIONDATA NotifData;
            Log(TEXT("ASyncIOThread : Received notification from queue: %#x"), 
                pCSMsgQHeader->Flags);

            if (!ReadMsgQueue(
                    m_hStreamMsgQueue, 
                    &(NotifData.Context), 
                    sizeof(NotifData.Context), 
                    &dwBytesRead, 
                    MSGQUEUE_WAITTIME, 
                    &dwFlags))
            {
                FAIL(TEXT("ASyncIOThread : No Context was sent through msgq after notification header"));
                ReleaseMutex(m_hMutex);
                continue;
            }
            if (dwBytesRead != sizeof(NotifData.Context))
            {
                FAIL(TEXT("ASyncIOThread : Notification Context is incorrect size"));
                ReleaseMutex(m_hMutex);
                continue;
            }
            if (NotifData.Context.Size != sizeof(NotifData.Context))
            {
                FAIL(TEXT("ASyncIOThread : Notification Context has incorrect Size member"));
                ReleaseMutex(m_hMutex);
                continue;
            }
            if (FLAG_MSGQ_ASYNCHRONOUS_FOCUS == pCSMsgQHeader->Flags)
            {
                FAIL(TEXT("ASyncIOThread : Received ASYNCHRONOUS_FOCUS notification on Pin msgq; Should only be received on Adapter msgq"));
            }
            if (FLAG_MSGQ_SAMPLE_SCANNED == pCSMsgQHeader->Flags)
            {
                if (m_sPinType != STREAM_STILL)
                {
                    FAIL(TEXT("ASyncIOThread : Received SAMPLE_SCANNED notification on pin other than still; Should only be received from STILL pin"));
                }
                if (NotifData.Context.Data != m_nPictureNumber)
                {
                    FAIL(TEXT("ASyncIOThread : Notification Context for Sample Scanned has incorrect Data, should be number of next picture"));
                    Log(TEXT("Expected Data: %d; Actual Data: %d"), m_nPictureNumber, NotifData.Context.Data);
                }
                ++m_lSampleScannedNotifsReceived;
            }
            NotifData.TypeFlag = pCSMsgQHeader->Flags;
            NotifData.TimeReceived = GetTickCount();
            m_nlReceivedNotifications.push_back(NotifData);
            ReleaseMutex(m_hMutex);
            continue;
        }

        if(FLAG_MSGQ_FRAME_BUFFER & ~pCSMsgQHeader->Flags)
        {
            WARN(TEXT("ASyncIOThread : The buffer retrieved from Queue is of custom type for driver"));
        }

        pStreamDescriptor = csMsgQBuffer.pStreamDescriptor;
        
        if(NULL == pStreamDescriptor)
        {
            ERRFAIL(TEXT("ASyncIOThread : Stream Descriptor is NULL"));
            ReleaseMutex(m_hMutex);
            break;
        }
        
        pCsStreamHeader = &pStreamDescriptor->CsStreamHeader;
        if(NULL == pCsStreamHeader)
        {
            ERRFAIL(TEXT("ASyncIOThread : Stream Header is NULL"));
            ReleaseMutex(m_hMutex);
            break;
        }

        Log(TEXT("Frame Id%d"), pStreamDescriptor->CsFrameInfo.PictureNumber);

        if(m_csState == CSSTATE_STOP)// && m_sPinType != STREAM_STILL)
        {
            Log(TEXT("ASyncIOThread : Received buffer from queue while in stop state. Throwing out."));
            ++m_nPictureNumber;
            ReleaseMutex(m_hMutex);
            continue;
        }

        if(m_nPictureNumber == 1)
        {
            // make sure the retrieved picture number is 0
            if(pStreamDescriptor->CsFrameInfo.PictureNumber != 1)
            {
                ERRFAIL(TEXT("ASyncIOThread : Initial picture number isn't 1."));
                Log(TEXT("Initial picture number: %d"), (int)(pStreamDescriptor->CsFrameInfo.PictureNumber));
            }

            // make sure the presentation time is 0
            if(pStreamDescriptor->CsStreamHeader.PresentationTime.Time != 0 && m_sPinType != STREAM_STILL)
            {
                ERRFAIL(TEXT("ASyncIOThread : Initial presentation time isn't 0."));
                Log(TEXT("Initial presentation time: %d"), pStreamDescriptor->CsStreamHeader.PresentationTime.Time);
            }

            m_lExpectedDuration = pStreamDescriptor->CsStreamHeader.Duration;
            m_dwExpectedDataSize = pStreamDescriptor->CsStreamHeader.DataUsed;
            m_fExpectZeroPresentationTime = FALSE;
            
            // make sure a single sample scanned has been received
            if(m_sPinType == STREAM_STILL && m_lSampleScannedNotifsReceived != 1)
            {
                FAIL(TEXT("ASyncIOThread : Received a picture without first receiving a SAMPLE_SCANNED message"));
                Log(TEXT("Driver must call MDD_HandleNotification with SAMPLE_SCANNED when picture is read from sensor"));
            }
        }
        else
        {
            // verify that the picture number we think we're on is the same as we're actually on
            if(m_nPictureNumber != pStreamDescriptor->CsFrameInfo.PictureNumber)
            {
                ERRFAIL(TEXT("ASyncIOThread : Picture number didn't increase between frames."));
                Log(TEXT("Expected picture number is %d, actual picture number is %d (resetting expected)"),
                    m_nPictureNumber,
                    pStreamDescriptor->CsFrameInfo.PictureNumber);
                m_nPictureNumber = pStreamDescriptor->CsFrameInfo.PictureNumber;
            }

            // verify the new presentation time is later than the previous
            if(m_sPinType == STREAM_STILL)
            {
                // Don't verify the presentation time.
            }
            else if(!m_fExpectZeroPresentationTime && m_lPreviousPresentationTime >= pStreamDescriptor->CsStreamHeader.PresentationTime.Time)
            {
                ERRFAIL(TEXT("ASyncIOThread : Previous presentation time is before the current presentation time."));
                Log(TEXT("ASyncIOThread : Previous presentation time is 0x%08x, new presentation time is 0x%08x"), m_lPreviousPresentationTime, pStreamDescriptor->CsStreamHeader.PresentationTime.Time);
            }
            else if (m_fExpectZeroPresentationTime)
            {
                if (pStreamDescriptor->CsStreamHeader.PresentationTime.Time != 0)
                {
                    ERRFAIL(TEXT("ASyncIOThread : Expected PresentationTime == 0, found nonzero presentation time"));
                    Log(TEXT("ASyncIOThread : Expected presentation time is 0, actual presentation time is 0x%08x"), pStreamDescriptor->CsStreamHeader.PresentationTime.Time);
                }
                m_fExpectZeroPresentationTime = FALSE;
            }
            // make sure a single sample scanned has been received
            if(m_sPinType == STREAM_STILL && m_lSampleScannedNotifsReceived != m_nPictureNumber)
            {
                FAIL(TEXT("ASyncIOThread : Received a picture without first receiving a SAMPLE_SCANNED message"));
                Log(TEXT("Driver must call MDD_HandleNotification with SAMPLE_SCANNED when picture is read from sensor"));
            }
        }

        m_nPictureNumber++;
        m_lPreviousPresentationTime = pStreamDescriptor->CsStreamHeader.PresentationTime.Time;

        // verify the duration is what we'd expect
        if(m_lExpectedDuration != pStreamDescriptor->CsStreamHeader.Duration)
        {
            ERRFAIL(TEXT("ASyncIOThread : Actual duration doesn't match the expected duration."));
        }

        // verify the data size is what we'd expect
        if(m_dwExpectedDataSize != pStreamDescriptor->CsStreamHeader.DataUsed)
        {
            // it's only an error if the incoming subformat is uncompressed.
            if(m_pCSDataFormat->SubFormat == MEDIASUBTYPE_RGB8 ||
                m_pCSDataFormat->SubFormat == MEDIASUBTYPE_RGB565 ||
                m_pCSDataFormat->SubFormat == MEDIASUBTYPE_RGB555 ||
                m_pCSDataFormat->SubFormat == MEDIASUBTYPE_RGB24 ||
                m_pCSDataFormat->SubFormat == MEDIASUBTYPE_RGB32 ||
                m_pCSDataFormat->SubFormat == MEDIASUBTYPE_YUY2 ||
                m_pCSDataFormat->SubFormat == MEDIASUBTYPE_YV12 ||
                m_pCSDataFormat->SubFormat == MEDIASUBTYPE_YV16 ||
                m_pCSDataFormat->SubFormat == MEDIASUBTYPE_UYVY ||
                m_pCSDataFormat->SubFormat == MEDIASUBTYPE_YVU9 ||
                m_pCSDataFormat->SubFormat == MEDIASUBTYPE_Y41P ||
                m_pCSDataFormat->SubFormat == MEDIASUBTYPE_CLJR)
            {
                ERRFAIL(TEXT("ASyncIOThread : actual data used doesn't match the expected data used."));
            }
        }

        GUID RGB565guid = GUID_16BPP565;

        // enable this for the test to perform a color conversion from UYVY or YUV422 to RGB so the
        // images returned from the camera driver are visible.
        // BUGBUG: disabled due to a failure when creating the DIB which should be fixed
        // BUGBUG: but has not been tested.
#if 0
        GUID UYVYguid = GUID_UYVY;
        GUID YUV422guid = GUID_YUV422;

        // these algorithms handle UYVY and YUV422
        if(UYVYguid == m_pCSDataFormat->SubFormat || YUV422guid == m_pCSDataFormat->SubFormat)
        { 
            PCS_DATAFORMAT_VIDEOINFOHEADER pCsVideoInfoHdr = reinterpret_cast<PCS_DATAFORMAT_VIDEOINFOHEADER>(m_pCSDataFormat);
            BITMAPINFO *bmi = NULL;
            HBITMAP hbmp;
            WORD *pSurface = NULL;
            BYTE *pYData = (BYTE *) pCsStreamHeader->Data;
            BYTE *pUData = NULL;
            BYTE *pVData = NULL;

            bmi = (BITMAPINFO *) new BYTE[ sizeof(BITMAPINFO) + 3*sizeof(RGBQUAD) ];

            if(NULL == bmi)
            {
                ERRFAIL(TEXT("ASyncIOThread : Unable to allocate memory for a bitmapinfo structure."));
                ReleaseMutex(m_hMutex);
                // free the BMI allocated.
                delete[] bmi;
                bmi = NULL;
                break;
            }

            memset(bmi, 0, sizeof(BITMAPINFO) + 3*sizeof(RGBQUAD));
            bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi->bmiHeader.biWidth = pCsVideoInfoHdr->VideoInfoHeader.bmiHeader.biWidth;
            bmi->bmiHeader.biHeight = pCsVideoInfoHdr->VideoInfoHeader.bmiHeader.biHeight;
            bmi->bmiHeader.biBitCount = 16;
            bmi->bmiHeader.biPlanes = 1;
            // use BI_BITFIELDS because we want the bitfields defined, not the default BI_RGB format.
            bmi->bmiHeader.biCompression = BI_BITFIELDS;

            *((UINT *) &(bmi->bmiColors[2])) = 0x001F;
            *((UINT *) &(bmi->bmiColors[1])) = 0x07E0;
            *((UINT *) &(bmi->bmiColors[0])) = 0xF800;

            hbmp = CreateDIBSection(m_hdcMain, bmi, DIB_RGB_COLORS, (VOID **) &pSurface, NULL, 0);
            if(NULL == hbmp)
            {
                ERRFAIL(TEXT("ASyncIOThread : Unable to create the DIB for color conversion."));
                ReleaseMutex(m_hMutex);
                // free the BMI allocated.
                delete[] bmi;
                bmi = NULL;
                break;
            }

            // compute the stride for the YUV surface
            int SurfStride = (DIBSTRIDEBYTES(pCsVideoInfoHdr->VideoInfoHeader.bmiHeader));
            // Compute the stride in WORDs since we know this will be an RGB565 pixel format. 
            int Stride = (CS_DIBWIDTHBYTES(bmi->bmiHeader)) >> 1; 

            for (int Height = 0; Height < abs(pCsVideoInfoHdr->VideoInfoHeader.bmiHeader.biHeight); Height++) 
            { 
                for (int Width = 0; Width < pCsVideoInfoHdr->VideoInfoHeader.bmiHeader.biWidth; Width++) 
                { 
                    WORD rgb = 0; 

                    if(NULL == pUData && NULL == pVData && NULL != pYData) 
                    { 
                        BYTE y0 = pYData[Width * 2]; 
                        BYTE y1 = pYData[Width * 2 + 2]; 
                        BYTE u  = pYData[Width * 2 + 1]; 
                        BYTE v  = pYData[Width * 2 + 3]; 

                        int vv = (int)(1.371 * (v - 128)); 
                        int vu = (int)(-0.698 * (v - 128) - 0.336 * (u - 128)); 
                        int uu = (int)(1.732 * (u - 128)); 

                        int red   = (int)y0 + vv; 
                        int green = (int)y0 + vu; 
                        int blue  = (int)y0 + uu; 

                        if(red < 0) red = 0; 
                        if(green < 0) green = 0; 
                        if(blue < 0) blue = 0; 

                        if(red > 0xff) red = 0xff; 
                        if(green > 0xff) green = 0xff; 
                        if(blue > 0xff) blue = 0xff; 

                        rgb = ((red << 8) & 0xf800) | ((green << 3) & 0x07e0) | (blue >> 3); 

                        ((WORD *)(pSurface))[ Height * Stride + Width ] = rgb; 

                        Width++; 

                        red   = (int)y1 + vv; 
                        green = (int)y1 + vu; 
                        blue  = (int)y1 + uu; 

                        if(red < 0) red = 0; 
                        if(green < 0) green = 0; 
                        if(blue < 0) blue = 0; 

                        if(red > 0xff) red = 0xff; 
                        if(green > 0xff) green = 0xff; 
                        if(blue > 0xff) blue = 0xff; 

                        rgb = ((red << 8) & 0xf800) | ((green << 5) & 0x07e0) | (blue >> 3); 

                        ((WORD *)(pSurface))[ Height * Stride + Width ] = rgb; 
                    } 
                    else if(NULL != pUData && NULL != pVData && NULL != pYData) 
                    { 
                        BYTE y0 = pYData[(Height * pCsVideoInfoHdr->VideoInfoHeader.bmiHeader.biWidth + Width)]; 
                        BYTE y1 = pYData[(Height * pCsVideoInfoHdr->VideoInfoHeader.bmiHeader.biWidth + Width) + 1]; 
                        BYTE u  = pUData[(Height * pCsVideoInfoHdr->VideoInfoHeader.bmiHeader.biWidth + Width) >> 1]; 
                        BYTE v  = pVData[(Height * pCsVideoInfoHdr->VideoInfoHeader.bmiHeader.biWidth + Width) >> 1]; 

                        int red   = (int)(y0 + 1.371*(v - 128)); 
                        int green = (int)(y0 - 0.698*(v - 128) - 0.336*(u - 128)); 
                        int blue  = (int)(y0 + 1.732*(u - 128)); 

                        if(red < 0) red = 0; 
                        if(green < 0) green = 0; 
                        if(blue < 0) blue = 0; 

                        if(red > 0xff) red = 0xff; 
                        if(green > 0xff) green = 0xff; 
                        if(blue > 0xff) blue = 0xff; 

                        rgb = ((red << 8) & 0xf800) | ((green << 5) & 0x07e0) | (blue >> 3); 

                        ((WORD *)(pSurface))[ Height * Stride + Width ] = rgb; 
                    } 
                } 

                pYData += SurfStride; 
                pUData += SurfStride; 
                pVData += SurfStride; 
            } 

            if(FALSE == StretchDIBits(m_hdcMain,
                                                        m_rcClient.left,
                                                        m_rcClient.top,
                                                        m_rcClient.right - m_rcClient.left,
                                                        m_rcClient.bottom - m_rcClient.top, 
                                                        0,
                                                        0,
                                                        bmi->bmiHeader.biWidth,
                                                        bmi->bmiHeader.biHeight,
                                                        pSurface, 
                                                        bmi, 
                                                        DIB_RGB_COLORS, 
                                                        SRCCOPY))
            {
                ERRFAIL(TEXT("ASyncIOThread : StretchDIBitsFailed."));
                ReleaseMutex(m_hMutex);
                // free the BMI allocated.
                delete[] bmi;
                bmi = NULL;
                break;
            }

            // free the BMI allocated.
            delete[] bmi;
            bmi = NULL;
        } 
        else 
#endif
        if(RGB565guid == m_pCSDataFormat->SubFormat)
        {
            if(FALSE == StretchDIBits(m_hdcMain,
                                                        m_rcClient.left,
                                                        m_rcClient.top,
                                                        m_rcClient.right - m_rcClient.left,
                                                        m_rcClient.bottom - m_rcClient.top, 
                                                        0,
                                                        0,
                                                        m_lImageWidth,
                                                        m_lImageHeight,
                                                        pCsStreamHeader->Data, 
                                                        m_pbmiBitmapDataFormat, 
                                                        DIB_RGB_COLORS, 
                                                        SRCCOPY))
            {
                ERRFAIL(TEXT("ASyncIOThread : StretchDIBitsFailed."));
                ReleaseMutex(m_hMutex);
                break;
            }
        }
        else
        {
            Log(TEXT("ASyncIOThread : This is not an error; due to an unrecognized format the frame was received but not displayed."));
        }

        // insert an artificial delay if requested
        if(m_nArtificialDelay)
            Sleep(m_nArtificialDelay);

        
        if(m_csState != CSSTATE_STOP)
        {
            EnqueueBuffer(pStreamDescriptor);
        }

        ReleaseMutex(m_hMutex);
    } while (1/*We do not get Last Buffer */);

    InterlockedDecrement(&m_lASyncThreadCount);

    return THREAD_SUCCESS;
}

DWORD CStreamTest::GetDriverBufferCount()
{
    DWORD dwRequestedBuffer;

    WaitForSingleObject(m_hMutex, INFINITE);
    dwRequestedBuffer = m_dwDriverRequestedBufferCount;
    ReleaseMutex(m_hMutex);

    return dwRequestedBuffer;
}

DWORD CStreamTest::GetAllocationType()
{
    DWORD dwAllocationType;

    WaitForSingleObject(m_hMutex, INFINITE);
    dwAllocationType = m_dwAllocationType;
    ReleaseMutex(m_hMutex);

    return dwAllocationType;
}

BOOL CStreamTest::AllocateBuffers()
{
    BOOL bReturnValue = TRUE;
    ULONG ulBufferIndex;

    WaitForSingleObject(m_hMutex, INFINITE);

    if(m_dwAllocationType != CSPROPERTY_BUFFER_CLIENT_LIMITED &&
        m_dwAllocationType != CSPROPERTY_BUFFER_CLIENT_UNLIMITED &&
        m_dwAllocationType != CSPROPERTY_BUFFER_DRIVER)
    {
        ERRFAIL(TEXT("AllocateBuffers : Invalid buffering mode."));
        bReturnValue = FALSE;
        goto cleanup;
    }

    PSTREAM_BUFFERNODE pcsCurrentBufferNode = m_pcsBufferNode;

    // get to the end of the list.
    if(pcsCurrentBufferNode)
    {
        while(pcsCurrentBufferNode->pNext)
            pcsCurrentBufferNode = pcsCurrentBufferNode->pNext;
    }

    for(ulBufferIndex = 0; ulBufferIndex < m_dwDriverRequestedBufferCount; ulBufferIndex++)
    {
        // allocate our buffer node
        // if our current node is non-zero, then we're in the middle of the list.
        // allocate a new node, 
        if(pcsCurrentBufferNode)
        {
            pcsCurrentBufferNode->pNext = new(STREAM_BUFFERNODE);
            pcsCurrentBufferNode = pcsCurrentBufferNode->pNext;
        }
        else // we're starting a new linked list, so set the current node, and set the beginning pointer
        {
            pcsCurrentBufferNode = new(STREAM_BUFFERNODE);
            m_pcsBufferNode = pcsCurrentBufferNode;
        }

        if(!pcsCurrentBufferNode)
        {
            ERRFAIL(TEXT("AllocateBuffers : buffer node allocation failed."));
            bReturnValue = FALSE;
            goto cleanup;
        }

        // clear out the node
        memset(pcsCurrentBufferNode, 0, sizeof(STREAM_BUFFERNODE));

        // allocate the stream descriptor
        pcsCurrentBufferNode->pCsStreamDesc = new(CS_STREAM_DESCRIPTOR);
        if(!pcsCurrentBufferNode->pCsStreamDesc)
        {
            ERRFAIL(TEXT("AllocateBuffers : stream descriptor allocation failed."));
            bReturnValue = FALSE;
            goto cleanup;
        }

        memset(pcsCurrentBufferNode->pCsStreamDesc, 0, sizeof(CS_STREAM_DESCRIPTOR));

        // if software allocate frame
        if(m_dwAllocationType == CSPROPERTY_BUFFER_CLIENT_LIMITED || m_dwAllocationType == CSPROPERTY_BUFFER_CLIENT_UNLIMITED)
        {
            pcsCurrentBufferNode->pCsStreamDesc->CsStreamHeader.Data = (PVOID) new(BYTE[m_pCSDataFormat->SampleSize]);

            if(!pcsCurrentBufferNode->pCsStreamDesc->CsStreamHeader.Data)
            {
                ERRFAIL(TEXT("AllocateBuffers : buffer frame allocation failed."));
                bReturnValue = FALSE;
                goto cleanup;
            }
        }

        // pass the frame to the driver
        if(FALSE == RegisterBuffer(pcsCurrentBufferNode->pCsStreamDesc))
        {
            ERRFAIL(TEXT("AllocateBuffers : Failed to register the buffer with the driver."));
            bReturnValue = FALSE;
            goto cleanup;
        }

        // pass the frame to the driver
        if(FALSE == EnqueueBuffer(pcsCurrentBufferNode->pCsStreamDesc))
        {
            ERRFAIL(TEXT("AllocateBuffers : Failed to queue the buffer up with the driver."));
            bReturnValue = FALSE;
            goto cleanup;
        }
    }
cleanup:
    ReleaseMutex(m_hMutex);

    return bReturnValue;
}

BOOL CStreamTest::ReleaseBuffers()
{
    // free the data structures before closing the file handles, as we need to
    // make calls into the driver.
    // repeat until the linked list is empty.
    while(m_pcsBufferNode)
    {
        // backup the current node
        PSTREAM_BUFFERNODE csTempBufferNode = m_pcsBufferNode;

        // set the current node to the next node, for the next iteration
        m_pcsBufferNode = m_pcsBufferNode->pNext;

        // if the stream descriptor was allocated
        if(csTempBufferNode->pCsStreamDesc)
        {
            // The IOCTL_CS_BUFFERS call to deallocate sets the Data value to NULL.
            //   (this occurs even for client buffers - see CPinDevice::DeallocateBuffer
            //    in public\common\oak\drivers\capture\camera\layered\mdd\pindevice.cpp)
            // DShow seems to handle this appropriately, but I've been unable to determine
            // where the buffer is allocated and freed in DShow. For now, keep track
            // of this pointer so we can delete it.
            BYTE* pTempData = (BYTE*)csTempBufferNode->pCsStreamDesc->CsStreamHeader.Data;

            // now release the temp buffer's pointers.
            ReleaseBuffer(csTempBufferNode->pCsStreamDesc);

            // free the data pointer ifwe're in software mode
            if(m_dwAllocationType == CSPROPERTY_BUFFER_CLIENT_LIMITED || m_dwAllocationType == CSPROPERTY_BUFFER_CLIENT_UNLIMITED)
            {
                delete pTempData; //csTempBufferNode->pCsStreamDesc->CsStreamHeader.Data;
                csTempBufferNode->pCsStreamDesc->CsStreamHeader.Data = NULL;
            }

            // free the stream descrioptor allocated
            delete csTempBufferNode->pCsStreamDesc;
            csTempBufferNode->pCsStreamDesc = NULL;
        }

        // finally, delete the actual node
        delete csTempBufferNode;
        csTempBufferNode = NULL;
    };

    return TRUE;
}

BOOL CStreamTest::EnqueueBuffer(PCS_STREAM_DESCRIPTOR pStreamDescriptor)
{
    // return the buffer back to the driver
    DWORD dwBytesReturned = 0;
    BOOL bReturnValue = TRUE;
    CSBUFFER_INFO BufferInfo;
    BufferInfo.dwCommand = CS_ENQUEUE;
    BufferInfo.pStreamDescriptor = pStreamDescriptor;

    if(FALSE == TestStreamDeviceIOControl (IOCTL_CS_BUFFERS, 
                                            &BufferInfo, 
                                            sizeof(CSBUFFER_INFO), 
                                            pStreamDescriptor, 
                                            sizeof(CS_STREAM_DESCRIPTOR), 
                                            &dwBytesReturned,
                                            NULL))
    {
        ERRFAIL(TEXT("EnqueueBuffer : Call to enqueue the buffer failed."));
        bReturnValue = FALSE;
    }

    return bReturnValue;
}

BOOL CStreamTest::RegisterBuffer(PCS_STREAM_DESCRIPTOR pStreamDescriptor)
{
    // return the buffer back to the driver
    DWORD dwBytesReturned = 0;
    BOOL bReturnValue = TRUE;
    CSBUFFER_INFO BufferInfo;
    BufferInfo.dwCommand = CS_ALLOCATE;
    BufferInfo.pStreamDescriptor = pStreamDescriptor;

    if(FALSE == TestStreamDeviceIOControl (IOCTL_CS_BUFFERS, 
                                            &BufferInfo, 
                                            sizeof(CSBUFFER_INFO), 
                                            pStreamDescriptor, 
                                            sizeof(CS_STREAM_DESCRIPTOR), 
                                            &dwBytesReturned,
                                            NULL))
    {
        ERRFAIL(TEXT("RegisterBuffer : Call to allocate the buffer failed."));
        bReturnValue = FALSE;
    }

    return bReturnValue;
}


// now that we've finished with the buffer, give it back to the driver for future use.
BOOL CStreamTest::ReleaseBuffer(PCS_STREAM_DESCRIPTOR    pStreamDescriptor)
{
    BOOL bReturnValue = TRUE;

    // free the buffer (IOCTL_CS_BUFFERS, CS_DEALLOCATE)
    DWORD dwBytesReturned = 0;
    CSBUFFER_INFO BufferInfo;
    BufferInfo.dwCommand = CS_DEALLOCATE;
    BufferInfo.pStreamDescriptor = pStreamDescriptor;

    if(FALSE == TestStreamDeviceIOControl (IOCTL_CS_BUFFERS, 
                                            &BufferInfo, 
                                            sizeof(CSBUFFER_INFO), 
                                            pStreamDescriptor, 
                                            sizeof(CS_STREAM_DESCRIPTOR), 
                                            &dwBytesReturned,
                                            NULL))
    {
        ERRFAIL(TEXT("ReleaseBuffer : Call to release the buffer failed."));
        bReturnValue = FALSE;
    }

    return bReturnValue;
}

BOOL CStreamTest::GetNumberOfFramesProcessed()
{
    int nPictureCount = 0;
    // keep state changes synchronous.
    WaitForSingleObject(m_hMutex, INFINITE);
    nPictureCount = m_nPictureNumber;
    ReleaseMutex(m_hMutex);

    return nPictureCount;
}

BOOL CStreamTest::GetNextReceivedNotification(DWORD NotifFlag, PNOTIFICATIONDATA pNotifData, BOOL bRemove)
{
    BOOL bReturnValue = FALSE;
    NotifList::iterator iterNotif;
    if (!pNotifData)
    {
        return FALSE;
    }
    
    WaitForSingleObject(m_hMutex, INFINITE);
    if (0 == NotifFlag)
    {
        iterNotif = m_nlReceivedNotifications.begin();
        if (iterNotif == m_nlReceivedNotifications.end())
        {
            bReturnValue =  FALSE;
            goto cleanup;
        }
        memcpy(pNotifData, &(*iterNotif), sizeof(*pNotifData));
        if (bRemove)
        {
            m_nlReceivedNotifications.erase(iterNotif);
        }
        bReturnValue =  TRUE;
        goto cleanup;
    }

    for (iterNotif = m_nlReceivedNotifications.begin();
        iterNotif != m_nlReceivedNotifications.end();
        ++iterNotif)
    {
        if ((*iterNotif).TypeFlag == NotifFlag)
        {
            memcpy(pNotifData, &(*iterNotif), sizeof(*pNotifData));
            if (bRemove)
            {
                m_nlReceivedNotifications.erase(iterNotif);
            }
            bReturnValue =  TRUE;
            goto cleanup;
        }
    }
cleanup:
    ReleaseMutex(m_hMutex);
    return bReturnValue;
}
