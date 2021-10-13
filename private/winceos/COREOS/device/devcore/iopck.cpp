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
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
// 
// Module Name:  
//     IoPck.cpp
// 
// Abstract: Io Packet Handling..
// 
// Notes: 
//
#include <windows.h>
#include <types.h>
#include <Ceddk.h>
#include <devload.h>
#include <errorrep.h>

#include "devmgrp.h"
#include "iopck.hpp"
#include "fiomgr.h"
#include "devzones.h"

LONG   IoPacket::m_lgsIoNextIndex = 0 ;

IoPacket::IoPacket(IoPckManager& ioPckMamager, LPOVERLAPPED lpExternalOverlapped)
:   m_ioPckMamager(ioPckMamager)  
,   m_lpExternalOrignal(lpExternalOverlapped)
{
    m_lIoIndex = InterlockedIncrement(&m_lgsIoNextIndex);
    DEBUGMSG(ZONE_IO,(TEXT("IoPacket::constructor (ref = 0x%x )\n"),m_lIoIndex));
    m_ioPacketStatus = IO_PACKET_STATUS_UNKNOWN ;
    m_dwNumOfBytesTransferred = 0 ;
    m_dwErrorStatus = ERROR_IO_PENDING;
    m_dwObjectFlag = IOPACKET_RETIRED_FLAG;
    
    m_dwInitialProcID= GetDirectCallerProcessId ();
    m_dwInitialThreadID = GetCurrentThreadId();
    m_lpExternalOverlapped = NULL ;
    m_hCompletionEvent = NULL;

    if (m_lpExternalOrignal) {
        if (!SUCCEEDED(CeAllocAsynchronousBuffer((PVOID *)&m_lpExternalOverlapped,m_lpExternalOrignal, sizeof(OVERLAPPED),ARG_O_PTR|MARSHAL_FORCE_ALIAS))){
            m_lpExternalOrignal = NULL;
            ASSERT(FALSE);
        }
        __try {
            if (m_lpExternalOverlapped) {
                m_lpExternalOverlapped->Internal = (DWORD)ERROR_IO_PENDING;
                m_lpExternalOverlapped->InternalHigh = 0; 
            }
            
            if (m_lpExternalOverlapped && m_lpExternalOverlapped->hEvent) {
                if (!DuplicateHandle((HANDLE)m_dwInitialProcID, m_lpExternalOverlapped->hEvent,GetCurrentProcess(),&m_hCompletionEvent,
                        0,FALSE,DUPLICATE_SAME_ACCESS)) {
                    ASSERT(FALSE);
                    m_hCompletionEvent = NULL;
                }
            }
        }__except (ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
            m_hCompletionEvent = NULL;
            CeFreeAsynchronousBuffer(m_lpExternalOverlapped,m_lpExternalOrignal, sizeof(OVERLAPPED),ARG_O_PTR|MARSHAL_FORCE_ALIAS);
            m_lpExternalOverlapped = NULL;
        }    
    }
    else
        m_hCompletionEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
            
        
    m_hAPI = NULL;

    m_UserlpInBuf = m_UserlpOutBuf = NULL;
    m_nInBufSize = m_nOutBufSize = 0 ;
    m_AsyncMappedInBuffer = m_AsyncMappedOutBuffer = NULL;    
    m_dwTargetProcID = 0;
    
    m_lMemoryRefCount = m_lRefCount = 0 ;
    
    m_ioPckMamager.AddRef();
}

BOOL  IoPacket::Init( LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize )
{
    if (m_lpExternalOrignal!=NULL && m_lpExternalOrignal==NULL) {
        // We can't access external LPOVERLAPPED strcture, so we failed.
        return FALSE;
    }
    if (m_lpExternalOrignal==NULL && m_hCompletionEvent==NULL) {
        // Sync IO can't allocate Handle.
        return FALSE;
    }
    DEBUGMSG(ZONE_IO,(TEXT("IoPacket::Init (ref = 0x%x )(lpInBuf=%x, nInBufSize=%x, lpOutBuf=%x, nOutBufSize=%x )\n"),m_lIoIndex,
        lpInBuf, nInBufSize, lpOutBuf, nOutBufSize ));
        
    m_UserlpInBuf =lpInBuf;
    m_UserlpOutBuf = lpOutBuf;
    m_nInBufSize = nInBufSize;
    m_nOutBufSize = nOutBufSize;
    
    m_AsyncMappedInBuffer = m_AsyncMappedOutBuffer = NULL;    
    m_dwNumOfBytesTransferred = 0 ;
    m_lMemoryRefCount = 0 ;

    ASSERT(m_lRefCount == 0);

    m_dwInitialProcID= GetDirectCallerProcessId ();
    m_dwInitialThreadID = GetCurrentThreadId();
    
    if (m_hAPI) {
        ASSERT(FALSE);
        CloseHandle(m_hAPI);
        m_hAPI = NULL;
    }

    m_ioPacketStatus = IO_PACKET_STATUS_INITIALIZED ;
    m_dwObjectFlag = IOPACKET_ACTIVE_FLAG ;
    return TRUE;
}

IoPacket::~IoPacket()
{
    DEBUGMSG(ZONE_IO,(TEXT("IoPacket::deconstructor (ref = 0x%x )\n"),m_lIoIndex));
    DeInit();
    if (m_lpExternalOverlapped) {
        CeFreeAsynchronousBuffer(m_lpExternalOverlapped,m_lpExternalOrignal, sizeof(OVERLAPPED),ARG_O_PTR|MARSHAL_FORCE_ALIAS);
    }
    if (m_hCompletionEvent)  {
        SetEvent(m_hCompletionEvent);
        CloseHandle(m_hCompletionEvent);
    }
    m_ioPckMamager.DeRef();
}
BOOL IoPacket::DeInit()
{
    DEBUGMSG(ZONE_IO,(TEXT("IoPacket::DeInit (ref = 0x%x )\n"),m_lIoIndex));
    if (m_hAPI) {
        CloseHandle(m_hAPI);
        m_hAPI = NULL;
    }
    UnmapBuffer();
    m_dwInitialProcID = 0 ;
    m_dwInitialThreadID = 0;
    m_dwObjectFlag = IOPACKET_RETIRED_FLAG;
    m_ioPacketStatus = IO_PACKET_STATUS_UNKNOWN;
    ASSERT(m_lMemoryRefCount==0);
    return TRUE;
    
}
VOID IoPacket::FreeIoPacket()
{

    if (m_lpExternalOrignal) {
        delete this;
    }
    else {
        DeInit();
        m_ioPckMamager.InsertFreeSyncPacket(this);
    }
}
BOOL  IoPacket::IoControl(DWORD dwIoControlCode,PVOID lpInBuf, DWORD nInBufSize, PVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned,LPOVERLAPPED /*lpOverlapped*/)
{
    BOOL fRetun = FALSE;
    SetLastError(ERROR_INVALID_PARAMETER);
    switch (dwIoControlCode){
      case IOCTL_FIOMGR_IOUPDATE_STATUS:
        if (lpInBuf && nInBufSize >= sizeof(FIOMGR_IOUPDATE_STATUS)) {
            FIOMGR_IOUPDATE_STATUS ioupdateStatus = *(PFIOMGR_IOUPDATE_STATUS)lpInBuf;
            fRetun = UpdateCompletedBytesAndStatus(ioupdateStatus.dwBytesTransfered,ioupdateStatus.dwLastErrorStatus);
        }
        break;
      case IOCTL_FIOMGR_IOGET_TARGET_PROCID:
        if (lpOutBuf && nOutBufSize >= sizeof(DWORD)) {
            *(PDWORD)lpOutBuf = m_dwTargetProcID;
            if (lpBytesReturned) {
                *lpBytesReturned = sizeof(DWORD);
            }
            fRetun = TRUE;
        }
        break;
      default:
        break;
    }
    ASSERT(fRetun);
    return fRetun;
}

HANDLE  IoPacket::CreateAsyncIoHandle(DWORD dwTargetProcID, LPVOID * lpInBuf ,LPVOID * lpOutBuf)
{
    DEBUGMSG(ZONE_IO,(TEXT("IoPacket::CreateAsyncIoHandle (ref = 0x%x )(dwTargetProcID=0x%x\n"),m_lIoIndex,dwTargetProcID));
    if (m_ioPacketStatus == IO_PACKET_STATUS_INITIALIZED && m_hAPI==NULL &&
            m_dwObjectFlag == IOPACKET_ACTIVE_FLAG && 
            m_dwInitialThreadID == GetCurrentThreadId() &&
            ghAPIHandle!=NULL) {
        // Everything is correct. Let us create API Handle.
        m_hAPI = CreateAPIHandle(ghAPIHandle,this);
        if (MapToAsyncBuffer(dwTargetProcID, lpInBuf , lpOutBuf)) {
            if (m_hAPI!=NULL && m_hAPI!=INVALID_HANDLE_VALUE) {
                if (m_hCompletionEvent)
                    ResetEvent(m_hCompletionEvent);
                m_ioPacketStatus = IO_PACKET_STATUS_ASYNC ;
                VERIFY(m_ioPckMamager.InsertToActiveList(this));
                AddRef();// reference by created Handle.
                UpdateCompletedBytesAndStatus(0,ERROR_IO_PENDING);
            }
            else {
                m_hAPI = NULL;
                UnMapAsyncBuffer();
            }
        }
        DEBUGMSG(ZONE_IO && ZONE_FUNCTION,(TEXT("IoPacket::CreateAsyncIoHandle ( ref = 0x%x ) return handle 0x%x)"),m_hAPI));
        return m_hAPI;
    }
    else
        return NULL;
}
BOOL    IoPacket::AsyncCompleteIo() // This is called by close handle. So we dettach this objec from the API Handle.
{
    DEBUGMSG(ZONE_IO,(TEXT("IoPacket::AsyncCompleteIo (ref = 0x%x )\n"),m_lIoIndex));
    ASSERT(m_lRefCount>0);
    ASSERT(m_hAPI);
    m_hAPI = NULL ;
    ASSERT(m_hCompletionEvent!=NULL);
    if (m_dwErrorStatus == ERROR_IO_PENDING) { // Somehting wrong. Correct it.
        ASSERT(FALSE);
        UpdateCompletedBytesAndStatus(m_dwNumOfBytesTransferred,ERROR_SUCCESS);
    }
    m_ioPacketStatus = IO_PACKET_STATUS_COMPLETE;
    BOOL fResult= m_ioPckMamager.RemoveFromActiveList(this);
    ASSERT(fResult);
    fResult = UnMapAsyncBuffer();
    ASSERT(fResult);
    return (m_hCompletionEvent!=NULL? SetEvent(m_hCompletionEvent):FALSE);
}
BOOL    IoPacket::SyncCompleteIo(DWORD dwCompletedBytes, DWORD dwErrorsStatus )
{ 
    ASSERT(m_hAPI == NULL);
    VERIFY(UpdateCompletedBytesAndStatus(dwCompletedBytes, dwErrorsStatus));
    return (m_hCompletionEvent!=NULL? SetEvent(m_hCompletionEvent): FALSE) ; 
};

BOOL    IoPacket::UpdateCompletedBytesAndStatus(DWORD dwCompletedBytes,DWORD dwErrorStatus) 
{
    if (m_ioPacketStatus == IO_PACKET_STATUS_ASYNC || m_ioPacketStatus == IO_PACKET_STATUS_INITIALIZED) {
        if (m_dwNumOfBytesTransferred<dwCompletedBytes) {
            m_dwNumOfBytesTransferred = dwCompletedBytes;
            m_ioPckMamager.UpdateLastCompletedBytes(dwCompletedBytes);
        }
        m_dwErrorStatus = dwErrorStatus;
        if (m_lpExternalOverlapped) {
            __try{
                m_lpExternalOverlapped->InternalHigh= m_dwNumOfBytesTransferred;
                m_lpExternalOverlapped->Internal = m_dwErrorStatus;
            }__except (ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER){
            }
        }
    }
    else
        ASSERT(FALSE);
    return TRUE;
}

LPVOID  IoPacket::AllocCopyBuffer(LPVOID lpOrigBuffer, DWORD dwLength, BOOL fInput,LPVOID& KernelAddress) const
{
    LPVOID lpReturn = NULL;
    DWORD dwTargetProtection = fInput?PAGE_READONLY: PAGE_READWRITE;
    
    KernelAddress = NULL;
    if (GetCurrentProcessId()== m_dwInitialProcID && m_dwInitialProcID!= m_dwTargetProcID ) { // this is calling from kernel to user
        KernelAddress = VirtualAlloc(NULL,dwLength,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        if (KernelAddress) {
            // We do copy in for READ and READWRITE buffer.
            __try {
                memcpy(KernelAddress,lpOrigBuffer,dwLength);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
            }
            lpReturn = VirtualAllocCopyEx((HANDLE)m_dwInitialProcID,(HANDLE)m_dwTargetProcID,KernelAddress,dwLength,dwTargetProtection);
        }
    }
    else {
        lpReturn = VirtualAllocCopyEx((HANDLE)m_dwInitialProcID,(HANDLE)m_dwTargetProcID,lpOrigBuffer,dwLength,dwTargetProtection );
    }
    return lpReturn;
}
BOOL   IoPacket::FreeCopyBuffer(LPVOID lpMappedBuffer, __in_bcount(dwLength) LPVOID lpOrigBuffer,DWORD dwLength, BOOL fInput, LPVOID KernelAddress) const
{
    //Unmap the User Buffer.
    if (KernelAddress) {
        BOOL bFreeRet;
        if (lpMappedBuffer) {
            bFreeRet = VirtualFreeEx((HANDLE)m_dwTargetProcID,(LPVOID)((DWORD)lpMappedBuffer & ~VM_BLOCK_OFST_MASK),0, MEM_RELEASE) ;
            ASSERT(bFreeRet);
        }
        if (!fInput) { // Output
            // Copy Out
            __try {
                memcpy(lpOrigBuffer,KernelAddress,dwLength);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
            }
        }
        bFreeRet = VirtualFree(KernelAddress,0, MEM_RELEASE);
        ASSERT(bFreeRet);
    }
    else if ( lpMappedBuffer) {
        VERIFY(VirtualFreeEx((HANDLE)m_dwTargetProcID,(LPVOID)((DWORD)lpMappedBuffer & ~VM_BLOCK_OFST_MASK),0, MEM_RELEASE));
    }
    return TRUE;
}

BOOL    IoPacket::MapToAsyncBuffer(DWORD dwTargetProcID, LPVOID * lpInBuf ,LPVOID * lpOutBuf )
{
    BOOL fReturn = TRUE;
    if (m_dwInitialThreadID != GetCurrentThreadId()) {
        ASSERT(FALSE);
        return FALSE;
    }
    if (InterlockedIncrement(&m_lMemoryRefCount) == 1 ) { // I have to mapp it.
        ASSERT(m_AsyncMappedInBuffer == NULL);
        ASSERT(m_AsyncMappedOutBuffer == NULL);
        m_dwTargetProcID = dwTargetProcID;
        if (m_nInBufSize && m_UserlpInBuf) {
            m_AsyncMappedInBuffer = AllocCopyBuffer(m_UserlpInBuf, m_nInBufSize, TRUE ,m_InKernelAddress);
            fReturn = (m_AsyncMappedInBuffer!=NULL);
            ASSERT(m_AsyncMappedInBuffer);
        }
        if (m_UserlpOutBuf && m_nOutBufSize) {
            m_AsyncMappedOutBuffer = AllocCopyBuffer(m_UserlpOutBuf,m_nOutBufSize, FALSE, m_OutKernelAddress);
            ASSERT(m_AsyncMappedOutBuffer);
            fReturn = (m_AsyncMappedOutBuffer!=NULL);
        }
        DEBUGMSG(ZONE_IO,(TEXT("IoPacket::MapToAsyncBuffer (ref = 0x%x ) fReturn=%x\n"),m_lIoIndex,fReturn));
    }
    if (!fReturn) {
        UnMapAsyncBuffer();
    }
    ASSERT( dwTargetProcID == m_dwTargetProcID);
    if (fReturn ) {
        if (lpInBuf!=NULL)
            *lpInBuf = m_AsyncMappedInBuffer;
        if (lpOutBuf!=NULL)
            *lpOutBuf = m_AsyncMappedOutBuffer;
    }
    return fReturn;
}
BOOL    IoPacket::UnMapAsyncBuffer()
{
    BOOL fReturn = TRUE;
    if (InterlockedDecrement(&m_lMemoryRefCount)<=0) {
        fReturn = UnmapBuffer();
    }
    return fReturn;
}
BOOL  IoPacket::UnmapBuffer()
{
    ASSERT(m_lMemoryRefCount == 0);
    BOOL fResult = TRUE;
    if (m_AsyncMappedInBuffer!=NULL) {
        DEBUGMSG(ZONE_IO,(TEXT("IoPacket::UnmapBuffer free In  (ref = 0x%x ) m_AsyncMappedInBuffer=%x\n"),m_lIoIndex,m_AsyncMappedInBuffer));
        if (!FreeCopyBuffer(m_AsyncMappedInBuffer, m_UserlpInBuf,m_nInBufSize,TRUE, m_InKernelAddress)) {
            ASSERT(FALSE);
            fResult = FALSE;
        }
        m_AsyncMappedInBuffer = NULL;
    };
    if (m_AsyncMappedOutBuffer!=NULL) {
        DEBUGMSG(ZONE_IO,(TEXT("IoPacket::UnmapBuffer free Out  (ref = 0x%x ) m_AsyncMappedInBuffer=%x\n"),m_lIoIndex,m_AsyncMappedOutBuffer));
        if (!FreeCopyBuffer(m_AsyncMappedOutBuffer, m_UserlpOutBuf,m_nOutBufSize,FALSE, m_OutKernelAddress)) {
            ASSERT(FALSE);
            fResult = FALSE;
        }
        m_AsyncMappedOutBuffer = NULL;
    }
    m_lMemoryRefCount = 0 ;
    return fResult;
}


