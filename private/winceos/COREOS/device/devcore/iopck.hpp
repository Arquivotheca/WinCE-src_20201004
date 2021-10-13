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

//
// This header contains internal function I/O packet manager.
//

#pragma once

#include <CRefCon.h>
#include <CSync.h>

class IoPckManager;

typedef enum __IO_PACKET_STATUS {
    IO_PACKET_STATUS_UNKNOWN = 0,
    IO_PACKET_STATUS_INITIALIZED,
    IO_PACKET_STATUS_ASYNC,
    IO_PACKET_STATUS_COMPLETE
} IO_PACKET_STATUS, PIO_PACKET_STATUS;

#define IOPACKET_ACTIVE_FLAG 0xf05aac53
#define IOPACKET_RETIRED_FLAG 0xf05adeed

class IoPacket: public LIST_ENTRY  {
public:
    IoPacket(IoPckManager& ioPckMamager, LPOVERLAPPED lpExternalOverlapped);
    ~IoPacket();
    BOOL    Init( LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize );
    BOOL    DeInit();
    void    FreeIoPacket();

    BOOL    MapToAsyncBuffer(DWORD dwTargetProcID, LPVOID * lpInBuf ,LPVOID * lpOutBuf );
    BOOL    UnMapAsyncBuffer();

 
    // External Function.
    BOOL    IoControl(DWORD dwIoControlCode,PVOID lpInBuf, DWORD nInBufSize, PVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned,LPOVERLAPPED lpOverlapped);
    HANDLE  CreateAsyncIoHandle(DWORD dwTargetProcID, LPVOID * lpInBuf ,LPVOID * lpOutBuf);
    BOOL    UpdateCompletedBytesAndStatus(DWORD dwCompletedBytes, DWORD errorStatus) ;
    BOOL    AsyncCompleteIo() ;
    BOOL    SyncCompleteIo(DWORD dwCompletedBytes, DWORD dwErrorsStatus); 

    
    HANDLE  GetAPIHandle() { return m_hAPI; };
    HANDLE  GetCompleteEventHandle() { return m_hCompletionEvent; };
    DWORD   GetCallerThreadID() { return m_dwInitialThreadID; };

    IO_PACKET_STATUS GetIoPacketStatus() { return m_ioPacketStatus; };
    BOOL    IsExternalIo() { return (m_lpExternalOrignal!=0); };
    LPVOID  GetOrigInBuffer() { return (m_nInBufSize!=0? m_UserlpInBuf: NULL); };
    LPVOID  GetOrignOutBuffer() { return (m_nOutBufSize!=0? m_UserlpOutBuf: NULL); };
    LPVOID  GetAsyncInBuffer() { return (m_AsyncMappedInBuffer); };
    LPVOID  GetAsyncOutBuffer() { return (m_AsyncMappedOutBuffer); };
    DWORD   GetLastCompletedStatus() { return m_dwErrorStatus; };
    DWORD   GetNumberOfByteTransfered() { return m_dwNumOfBytesTransferred; };
    LPOVERLAPPED GetOriginalOverlapped() { return m_lpExternalOrignal; };
    LONG    AddRef( void ) {
        return InterlockedIncrement(&m_lRefCount);
    };
    LONG    DeRef( void ) {
        LONG lReturn = InterlockedDecrement(&m_lRefCount);
        if( lReturn <= 0 ) {
            FreeIoPacket();
        }
        return lReturn;
    }

protected:
    LPOVERLAPPED m_lpExternalOverlapped;
    LPOVERLAPPED m_lpExternalOrignal;
    
    IO_PACKET_STATUS    m_ioPacketStatus;
    DWORD   m_dwNumOfBytesTransferred;
    DWORD   m_dwErrorStatus;
    DWORD   m_dwObjectFlag;
    
    HANDLE  m_hCompletionEvent;
    HANDLE  m_hAPI;

    DWORD   m_dwInitialProcID;
    DWORD   m_dwInitialThreadID;
    
    LPVOID  m_UserlpInBuf;
    DWORD   m_nInBufSize;
    LPVOID  m_UserlpOutBuf;
    DWORD   m_nOutBufSize;

    DWORD   m_dwTargetProcID;
    LONG    m_lMemoryRefCount;
    LPVOID  m_AsyncMappedInBuffer;
    LPVOID  m_InKernelAddress;
    LPVOID  m_AsyncMappedOutBuffer;
    LPVOID  m_OutKernelAddress;

    
    LONG    m_lRefCount;
    IoPckManager& m_ioPckMamager;
    
    BOOL    UnmapBuffer();
    LPVOID  AllocCopyBuffer(LPVOID lpOrigBuffer, DWORD dwLength, BOOL fInput,LPVOID& KernelAddress) const;
    BOOL    FreeCopyBuffer(LPVOID lpMappedBuffer, LPVOID lpOrigBuffer,DWORD dwLength, BOOL fInput, LPVOID KernelAddress) const;
private:
    static LONG   m_lgsIoNextIndex;
    LONG       m_lIoIndex;

    IoPacket operator=(IoPacket&rhs);
};



class IoPckManager: public fsopendev_t, public CRefObject, protected CLockObject  {
public:
// Intialization
    IoPckManager();
    ~IoPckManager();
    virtual BOOL Init() { return TRUE; };
    virtual BOOL DeInit();
// Existing API Support.
    virtual BOOL DevReadFile(LPVOID buffer, DWORD nBytesToRead, LPDWORD lpNumBytesRead,LPOVERLAPPED lpOverlapped);
    virtual BOOL DevWriteFile(LPCVOID buffer, DWORD nBytesToWrite, LPDWORD lpNumBytesWritten,LPOVERLAPPED lpOverlapped);
    virtual BOOL DevDeviceIoControl(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);
// New API support    
    virtual BOOL CancelIo(BOOL fAll = FALSE );
    virtual BOOL CancelIoEx(LPOVERLAPPED lpOverlappedIo);
    
// Update Last Completed IO result.    
    DWORD   UpdateLastCompletedBytes(DWORD dwCompletedBytes) { return (m_dwLastCompletedBytes = dwCompletedBytes); };
protected:
    IoPacket * Request(LPOVERLAPPED lpOverlappedIo,LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize);
//Async List
protected:
    LIST_ENTRY  m_AsyncIoList;
public:
    BOOL    InsertToActiveList(IoPacket * pioPacket);
    BOOL    RemoveFromActiveList(IoPacket * pioPacket);
protected:    
    LIST_ENTRY  m_SyncIoPacketFreeList;
public:
    IoPacket *  GetFreeSyncPacket();
    BOOL        InsertFreeSyncPacket(IoPacket * pioPacket);

    DWORD       m_dwLastCompletedBytes;

    BOOL        m_fTerminated;
private:
    static  LONG m_lgsPckMgrIndex;
    LONG        m_lPckMgrIndex;
};


extern HANDLE ghAPIHandle ;

