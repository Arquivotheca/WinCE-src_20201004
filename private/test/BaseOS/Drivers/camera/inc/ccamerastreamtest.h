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
#include <camera.h>
#include "logging.h"
#include <list>


#ifndef mmioFOURCC    
#define mmioFOURCC(ch0, ch1, ch2, ch3) \
     ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |\
     ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24))
#endif  

//
// FourCC of the YUV formats
// For information about FourCC, go to:
//     http://www.webartz.com/fourcc/indexyuv.htm
//     http://www.fourcc.org
//

#define FOURCC_Y444 mmioFOURCC('Y', '4', '4', '4')  // TIYUV: 1394 conferencing camera 4:4:4 mode 0
#define FOURCC_UYVY mmioFOURCC('U', 'Y', 'V', 'Y')  // MSYUV: 1394 conferencing camera 4:4:4 mode 1 and 3
#define FOURCC_Y411 mmioFOURCC('Y', '4', '1', '1')  // TIYUV: 1394 conferencing camera 4:1:1 mode 2
#define FOURCC_Y800 mmioFOURCC('Y', '8', '0', '0')  // TIYUV: 1394 conferencing camera 4:1:1 mode 5
#define FOURCC_YUV422 mmioFOURCC('U', 'Y', 'V', 'Y')

#define GUID_16BPP565 {0xe436eb7b, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}
#define GUID_Y444 {FOURCC_Y444, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define GUID_UYVY {FOURCC_UYVY, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define GUID_Y411 {FOURCC_Y411, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define GUID_Y800 {FOURCC_Y800, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define GUID_YUV422 {FOURCC_YUV422, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}

#define SAFECLOSEHANDLE(handle) if (NULL != handle){\
                                            CloseHandle(handle); \
                                            handle = NULL;}

#define SAFECLOSEFILEHANDLE(handle) if (INVALID_HANDLE_VALUE != handle){\
                                            CloseHandle(handle); \
                                            handle = INVALID_HANDLE_VALUE;}

#define SAFEDELETE(lpVoid) if (NULL != lpVoid){\
                                delete(lpVoid); \
                                lpVoid = NULL;}

#define SAFEARRAYDELETE(lpVoid) if (NULL != lpVoid){\
                                delete[](lpVoid); \
                                lpVoid = NULL;}

#define dim(x) (sizeof(x) / sizeof(x[0]))

#define DIBSTRIDEBYTES(bi) ((DWORD)(bi).biSizeImage / (DWORD)(bi).biHeight)

#define MAX_STREAMS 3
enum
{
    STREAM_ADAPTER = -1,
    STREAM_PREVIEW = 0,
    STREAM_CAPTURE,
    STREAM_STILL
};

typedef struct stValueData
{
    int Type;
    LPVOID lpValue;
} VALUEDATA, *PVALUEDATA;

typedef struct stNotificationData
{
    DWORD TypeFlag;
    CAM_NOTIFICATION_CONTEXT Context;
    DWORD TimeReceived;
} NOTIFICATIONDATA, *PNOTIFICATIONDATA;

typedef std::list<NOTIFICATIONDATA> NotifList;

enum
{
    MEMBER_VALUES = 0,
    MEMBER_STEPPEDRANGES_LONG,
    MEMBER_STEPPEDRANGES_LONGLONG,
    MEMBER_RANGES_LONG,
    MEMBER_RANGES_LONGLONG
};

#define THREAD_SUCCESS 0x00000001
#define THREAD_FAILURE 0x00000000

typedef class CStreamTest
{

public :
    CStreamTest();
    ~CStreamTest();
    BOOL Cleanup();
    BOOL CreateStream(ULONG ulStreamType, TCHAR *szStreamName, SHORT sPinType);
    BOOL TestStreamDeviceIOControl(DWORD dwIoControlCode,
                                            LPVOID lpInBuffer,
                                            DWORD nInBufferSize,
                                            LPVOID lpOutBuffer,
                                            DWORD nOutBufferSize,
                                            LPDWORD lpBytesReturned,
                                            LPOVERLAPPED lpOverlapped);
    BOOL GetFrameAllocationInfoForStream();
    BOOL CreateAsyncThread();
    BOOL CleanupASyncThread();
    BOOL SetAritificalDelay(int nDelay);
    BOOL SetState(CSSTATE sState);
    CSSTATE GetState();
    BOOL SetBufferCount (DWORD dwNewBufferCount);

    BOOL SetFormat( PCSDATAFORMAT pCSDataFormat, size_t DataFormatSize);
    BOOL CreateCameraWindow(HWND hwnd, RECT rc);

    DWORD GetDriverBufferCount();
    BOOL AllocateBuffers();
    BOOL ReleaseBuffers();

    static DWORD WINAPI ASyncIOThread(LPVOID lpVoid);


    int GetNumberOfFramesProcessed();

    // If NotifFlag == 0, removes the first notification from our list and returns it
    // If NotifFlag != 0, removes the first notif that matches the flag
    BOOL GetNextReceivedNotification(DWORD NotifFlag, PNOTIFICATIONDATA pNotifData, BOOL bRemove);

private :
    BOOL FlushMsgQueue();
    DWORD WINAPI StreamIOHandler();
    DWORD GetAllocationType();
    BOOL ReleaseBuffer(PCS_STREAM_DESCRIPTOR pStreamDescriptor);
    BOOL EnqueueBuffer(PCS_STREAM_DESCRIPTOR pStreamDescriptor);
    BOOL RegisterBuffer(PCS_STREAM_DESCRIPTOR pStreamDescriptor);

    DWORD m_dwAllocationType;
    DWORD m_dwDriverRequestedBufferCount;
    PCSDATAFORMAT m_pCSDataFormat ;
    CSALLOCATOR_FRAMING m_CSAllocatorFraming;
    PSTREAM_BUFFERNODE m_pcsBufferNode;
    HANDLE m_hStreamMsgQueue;

    WORD m_wBitCount;
    LONG m_lImageWidth;
    LONG m_lImageHeight;
    ULONG m_ulPinId;
    SHORT m_sPinType;
    HWND m_hwndMain;
    HDC m_hdcMain;
    RECT m_rcClient;
    HANDLE m_hCamStream;
    HANDLE m_hASyncThreadHandle;
    HANDLE m_hMutex;
    CSSTATE m_csState;
    BITMAPINFO *m_pbmiBitmapDataFormat;
    int m_nArtificialDelay;

    int m_nPictureNumber;
    LONGLONG m_lPreviousPresentationTime;
    LONGLONG m_lExpectedDuration;
    DWORD m_dwExpectedDataSize;
    BOOL m_fExpectZeroPresentationTime;

    LONG m_lASyncThreadCount;

    LONG m_lSampleScannedNotifsReceived;
    NotifList m_nlReceivedNotifications;
} CAMERASTREAMTEST, *PCAMERASTREAMTEST;


