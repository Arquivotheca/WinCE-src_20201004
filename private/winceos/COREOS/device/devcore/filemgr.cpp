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
//     FileMgr.cpp
// 
// Abstract: Extended File Manager Function.
// 
// Notes: 
//
#include <windows.h>
#include <types.h>
#include <Ceddk.h>
#include <devload.h>
#include <Linklist.h>
#include <errorrep.h>

#include "devmgrp.h"
#include "fiomgr.h"
#include "iopck.hpp"
#include "devzones.h"
#include "filter.h"

// Device Manager File Handleing Entry Extension.
extern "C" 
fsopendev_t * CreateAsyncFileObject()
{
    IoPckManager * pReturn = new IoPckManager();
    if (pReturn && pReturn->Init()) {
        pReturn->AddRef();
        return pReturn;
    }
    else {
        if (pReturn)
            delete pReturn;
        return NULL;
    }
    
}
extern "C"
BOOL DeleteAsncyFileObject(fsopendev_t * pfsopened)
{
    if (pfsopened) {
        IoPckManager * pReturn = (IoPckManager *) pfsopened;
        pReturn->DeInit();
        fsopendevaccess_t * pAccessIt = pfsopened->pOpenDevAccess;
        while (pAccessIt) {
            fsopendevaccess_t * pAccessNext = pAccessIt->nextptr;
            free(pAccessIt);
            pAccessIt = pAccessNext;
        }
        pReturn->DeRef();
    }
    return TRUE;
}
extern "C"
BOOL DevReadFile(fsopendev_t *fsodev, LPVOID buffer, DWORD nBytesToRead, LPDWORD lpNumBytesRead, LPOVERLAPPED lpOverlapped)
{
    BOOL fResult = FALSE; 
    if (fsodev) {
        IoPckManager * pIoManager = (IoPckManager *) fsodev;
        fResult = pIoManager->DevReadFile(buffer,nBytesToRead, lpNumBytesRead,lpOverlapped);
    }
    else {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return fResult;
}
extern "C"
BOOL DevWriteFile(fsopendev_t *fsodev, LPCVOID buffer, DWORD nBytesToWrite, LPDWORD lpNumBytesWritten,LPOVERLAPPED lpOverlapped)
{
    BOOL fResult = FALSE; 
    if (fsodev) {
        IoPckManager * pIoManager = (IoPckManager *) fsodev;
        fResult = pIoManager->DevWriteFile(buffer, nBytesToWrite, lpNumBytesWritten, lpOverlapped);
    }
    else {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return fResult;
}
extern "C"
BOOL DevDeviceIoControl(fsopendev_t *fsodev, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
    BOOL fResult = FALSE; 
    if (fsodev) {
        IoPckManager * pIoManager = (IoPckManager *) fsodev;
        fResult = pIoManager->DevDeviceIoControl(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped);
    }
    else {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return fResult;
}
extern "C"
BOOL DevCancelIo(fsopendev_t *fsodev)
{
    BOOL fResult = FALSE; 
    if (fsodev) {
        IoPckManager * pIoManager = (IoPckManager *) fsodev;
        fResult = pIoManager->CancelIo();
    }
    else {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return fResult;
}
extern "C"
BOOL DevCancelIoEx(fsopendev_t *fsodev, LPOVERLAPPED lpOverlapped )
{
    BOOL fResult = FALSE; 
    if (fsodev) {
        IoPckManager * pIoManager = (IoPckManager *) fsodev;
        fResult = pIoManager->CancelIoEx(lpOverlapped);
    }
    else {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return fResult;
}


extern "C"
HANDLE WINAPI DM_CreateAsyncIoHandle(HANDLE hIoRef, LPVOID * lpInBuf ,LPVOID * lpOutBuf)
{
    HANDLE  retval = NULL;
    BOOL fKernel = CeSetDirectCall(TRUE); // Direct calling from kernel.
    if (hIoRef) {
        IoPacket * pioPacket = (IoPacket *)hIoRef;
        __try {
            retval = pioPacket->CreateAsyncIoHandle(GetDirectCallerProcessId(),lpInBuf,lpOutBuf);
        } __except (ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);
            retval = NULL;
        }
    }
    else {
        SetLastError(ERROR_INVALID_PARAMETER);
        retval = NULL;
    }
    CeSetDirectCall(fKernel);
    return retval;
}
extern "C"
BOOL WINAPI EX_DM_SetIoProgress(HANDLE ioHandle, DWORD dwBytesTransfered)
{
    BOOL fChecked = TRUE;
    if (GetDirectCallerProcessId() != GetCurrentProcessId()) { // External, Security check.
        DWORD dwProcID = 0;
        BOOL fRet = DeviceIoControl(ioHandle,IOCTL_FIOMGR_IOGET_TARGET_PROCID, NULL,0,&dwProcID,sizeof(dwProcID),NULL,NULL);
        if (!(fRet && (dwProcID == GetDirectCallerProcessId()))) {
            fChecked = FALSE;
            SetLastError(ERROR_ACCESS_DENIED);
        }
    }
    if (fChecked) {
        FIOMGR_IOUPDATE_STATUS ioupdateStatus = {
            dwBytesTransfered,ERROR_IO_PENDING
        };
        fChecked = DeviceIoControl(ioHandle,IOCTL_FIOMGR_IOUPDATE_STATUS,&ioupdateStatus,sizeof(ioupdateStatus), NULL,0,NULL,NULL);
    }
    return fChecked ;
}
extern "C"
BOOL WINAPI DM_SetIoProgress(HANDLE ioHandle, DWORD dwBytesTransfered)
{
    BOOL fKernel = CeSetDirectCall(TRUE); // Direct calling from kernel.
    BOOL fReturn = EX_DM_SetIoProgress(ioHandle,dwBytesTransfered);
    CeSetDirectCall(fKernel);
    return fReturn;    
}

extern "C"
BOOL WINAPI EX_DM_CompleteAsyncIo(HANDLE ioHandle, DWORD dwCompletedByte,DWORD dwFinalStatus)
{
    BOOL fChecked = TRUE;
    if (GetDirectCallerProcessId() != GetCurrentProcessId()) { // External, Security check.
        DWORD dwProcID = 0;
        BOOL fRet = DeviceIoControl(ioHandle,IOCTL_FIOMGR_IOGET_TARGET_PROCID, NULL,0,&dwProcID,sizeof(dwProcID),NULL,NULL);
        if (!(fRet && (dwProcID == GetDirectCallerProcessId()))) {
            fChecked = FALSE;
            SetLastError(ERROR_ACCESS_DENIED);
        }
    }
    if (fChecked) {
        ASSERT(dwFinalStatus!=ERROR_IO_PENDING);
        FIOMGR_IOUPDATE_STATUS ioupdateStatus = {
            dwCompletedByte,(dwFinalStatus!=ERROR_IO_PENDING? dwFinalStatus: ERROR_SUCCESS)
        };
        fChecked = DeviceIoControl(ioHandle,IOCTL_FIOMGR_IOUPDATE_STATUS,&ioupdateStatus,sizeof(ioupdateStatus), NULL,0,NULL,NULL);
        if (fChecked) {
            fChecked = CloseHandle(ioHandle);
        }
    }
    return fChecked ;

}
extern "C"
BOOL WINAPI DM_CompleteAsyncIo(HANDLE ioHandle, DWORD dwCompletedByte,DWORD dwFinalStatus)
{
    BOOL fKernel = CeSetDirectCall(TRUE); // Direct calling from kernel.
    BOOL fReturn = EX_DM_CompleteAsyncIo(ioHandle, dwCompletedByte, dwFinalStatus);
    CeSetDirectCall(fKernel);
    return fReturn;
}
static BOOL IoCloseFileHandle(IoPacket * pioPocked)
{
    BOOL fRet = pioPocked->AsyncCompleteIo();
    pioPocked->DeRef();
    return fRet;
};
static BOOL IoDeviceIoControl(IoPacket *pioPocket, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) 
{
    return pioPocket->IoControl(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize,lpBytesReturned, lpOverlapped);
};




const PFNVOID IoServApiMethods[] = {
    (PFNVOID)IoCloseFileHandle,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)IoDeviceIoControl,
};

#define NUM_SERV_APIS _countof(IoServApiMethods)

const ULONGLONG IoServApiSigs[NUM_SERV_APIS] = {
    FNSIG1(DW),                 // CloseFileHandle
    FNSIG0(),
    FNSIG5(DW,I_PTR,DW,O_PDW,IO_PDW),   // ReadFile
    FNSIG5(DW,O_PTR,DW,O_PDW,IO_PDW),   // WriteFile
    FNSIG2(DW,O_PDW),                   // GetFileSize
    FNSIG4(DW,DW,O_PDW,DW),             // SetFilePointer
    FNSIG2(DW,O_PDW),                   // GetDeviceInformationByFileHandle
    FNSIG1(DW),                         // FlushFileBuffers
    FNSIG4(DW,O_PDW,O_PDW,O_PDW),       // GetFileTime
    FNSIG4(DW,IO_PDW,IO_PDW,IO_PDW),    // SetFileTime
    FNSIG1(DW),                         // SetEndOfFile,
    FNSIG8(DW, DW, I_PTR, DW, IO_PTR, DW, O_PDW, IO_PDW), // DeviceIoControl
};
HANDLE ghAPIHandle = NULL;
extern "C"
HANDLE CreateIoPacketHandle()
{
    if (ghAPIHandle == NULL ) {
        HANDLE localAPIHandle = CreateAPISet("IOPK", NUM_SERV_APIS, IoServApiMethods, IoServApiSigs );
        if (localAPIHandle!=INVALID_HANDLE_VALUE) {
            BOOL bReturn =RegisterAPISet(localAPIHandle, HT_FILE | REGISTER_APISET_TYPE);
            ASSERT(localAPIHandle!=INVALID_HANDLE_VALUE && bReturn) ;
            if (bReturn) {
                ghAPIHandle = localAPIHandle;
            }
            else
                CloseHandle(localAPIHandle);
        }
    }
    ASSERT(ghAPIHandle!=NULL);
    return ghAPIHandle;
};
extern "C"
BOOL  DeleteIoPacketHandle()
{
    if (ghAPIHandle) {
        CloseHandle(ghAPIHandle);
        ghAPIHandle = NULL;
    }
    return (ghAPIHandle==NULL);
}

LONG IoPckManager::m_lgsPckMgrIndex = 0; 
IoPckManager::IoPckManager()
{
    m_lPckMgrIndex = InterlockedIncrement(&m_lgsPckMgrIndex);
    DEBUGMSG(ZONE_FILE,(TEXT("IoPckManager::constructor (ref = 0x%x )\n"),m_lPckMgrIndex));
    memset((fsopendev_t *)this,0,sizeof(fsopendev_t));
    m_fTerminated = FALSE ;
    InitializeListHead(&m_AsyncIoList);
    InitializeListHead(&m_SyncIoPacketFreeList);
    m_dwLastCompletedBytes = 0 ;
}
IoPckManager::~IoPckManager()
{
    ASSERT(IsListEmpty(&m_AsyncIoList));
    ASSERT(IsListEmpty(&m_SyncIoPacketFreeList));
    DEBUGMSG(ZONE_FILE,(TEXT("IoPckManager::~deconstructor (ref = 0x%x )\n"),m_lPckMgrIndex));
}
#define MAX_SYNC_ITEM 0x1000
BOOL IoPckManager::DeInit()
{
    DEBUGMSG(ZONE_FILE,(TEXT("IoPckManager::Deinit (ref = 0x%x )\n"),m_lPckMgrIndex));
    m_fTerminated = TRUE;
    Lock();
    PLIST_ENTRY pCur= m_AsyncIoList.Flink;
    for (DWORD dwIndex=0; dwIndex<MAX_SYNC_ITEM; dwIndex++) {
        if (pCur != &m_AsyncIoList && pCur!=NULL) {
            PLIST_ENTRY pNext = pCur->Flink;
            CloseHandle(((IoPacket *)pCur)->GetAPIHandle()); // Complete This Trasaction.
            pCur = pNext;
        }
        else
            break;
    }
    Unlock();
    ASSERT(dwIndex<MAX_SYNC_ITEM);
    Lock();
    while (!IsListEmpty(&m_SyncIoPacketFreeList)) {
        IoPacket * pCur = (IoPacket *) RemoveHeadList(&m_SyncIoPacketFreeList);
        delete pCur;
    }
    Unlock();
    return TRUE;
}

IoPacket * IoPckManager::Request(LPOVERLAPPED lpOverlappedIo,LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize )
{
    DEBUGMSG(ZONE_FILE,(TEXT("IoPckManager::Request (ref = 0x%x ) (lpOverlappedIo=%x,lpInBuf=%x,nInBufSize=%x,lpOutBuf=%x,nOutBufSize=%x)\n"),m_lPckMgrIndex,
        lpOverlappedIo, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize));
    IoPacket * pioPacket = NULL;
    if (!m_fTerminated) {
        if (lpOverlappedIo)  // This is async
            pioPacket = new IoPacket(*this,lpOverlappedIo);
        else 
            pioPacket = GetFreeSyncPacket();

        if ( pioPacket  &&  pioPacket->Init(lpInBuf,nInBufSize, lpOutBuf, nOutBufSize ) ) {
            ASSERT((lpOverlappedIo!=0 == pioPacket->IsExternalIo()));
            pioPacket->AddRef();
        }
        else if (pioPacket){
            pioPacket->FreeIoPacket();
            pioPacket = NULL;
        }        
    }
    return pioPacket;
}
BOOL IoPckManager::DevDeviceIoControl(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
    BOOL    retval = FALSE;
    IoPacket * pioPacket =  Request(lpOverlapped,lpInBuf,nInBufSize,lpOutBuf,nOutBufSize);
    DWORD   dwCompleted = 0;
    if (pioPacket ) {
        ASSERT( pioPacket->GetIoPacketStatus() == IO_PACKET_STATUS_INITIALIZED );
        __try {
            retval = DriverControl(lpDev,dwOpenData,dwIoControlCode,(LPBYTE)lpInBuf,nInBufSize,(LPBYTE)lpOutBuf,nOutBufSize,&dwCompleted,pioPacket);
            if (lpBytesReturned)
                *lpBytesReturned = dwCompleted;
        }__except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);
            retval = FALSE;
        }
        if (!retval) {
            if (pioPacket->GetAPIHandle()!=NULL) { // Somehow, Driver mark is assync then indicate error as well.
                DEBUGMSG(ZONE_ERROR, (L"Device:DevDeviceIoControl Driver %s make this as async and return error!!!\r\n",lpDev->pszDeviceName));
                CloseHandle(pioPacket->GetAPIHandle());
            }
        }
        else if (pioPacket->GetIoPacketStatus()==IO_PACKET_STATUS_ASYNC) {
            if (pioPacket->IsExternalIo()) { // Async to Async. Set IO Pending.
                SetLastError(ERROR_IO_PENDING);
                retval = FALSE;
            }
            else { // Async Driver, Sync call.
                DWORD dwRet= WaitForSingleObject( pioPacket->GetCompleteEventHandle(),INFINITE);
                retval =(dwRet == WAIT_OBJECT_0);
                if (retval && pioPacket->GetLastCompletedStatus()!=ERROR_SUCCESS) {
                    retval = FALSE;
                    SetLastError(pioPacket->GetLastCompletedStatus());
                }
                if (retval) {
                    if (lpBytesReturned) {
                        __try {
                            *lpBytesReturned = pioPacket->GetNumberOfByteTransfered();
                        }__except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
                            SetLastError(ERROR_INVALID_PARAMETER);
                            retval = FALSE;
                        }
                    }
                }
                else
                    ASSERT(FALSE);
            }
        } if (pioPacket->IsExternalIo() && pioPacket->GetIoPacketStatus()== IO_PACKET_STATUS_INITIALIZED) {// Sync Driver, Async call
            pioPacket->SyncCompleteIo(dwCompleted, ERROR_SUCCESS);
        } 
        pioPacket->DeRef();
    }
    else {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return retval;
};
BOOL IoPckManager::DevReadFile(LPVOID buffer, DWORD nBytesToRead, LPDWORD lpNumBytesRead,LPOVERLAPPED lpOverlapped)
{
    BOOL    retval = FALSE;
    IoPacket * pioPacket =Request(lpOverlapped,NULL,0,buffer,nBytesToRead);
    
    if (pioPacket ) {
        ASSERT( pioPacket->GetIoPacketStatus() == IO_PACKET_STATUS_INITIALIZED );
        DWORD dwResult = (DWORD)-1;
        __try {
            dwResult = DriverRead(lpDev, dwOpenData,buffer,nBytesToRead,pioPacket);
            // set lpNumBytesRead if DriverRead succeeded 
            if (lpNumBytesRead && (dwResult != -1))
                *lpNumBytesRead = dwResult;
        }__except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);
            dwResult = (DWORD)-1;
        }
        retval = (dwResult!=(DWORD)-1);
        if (!retval) {
            if (pioPacket->GetAPIHandle()!=NULL) { // Somehow, Driver mark is assync then indicate error as well.
                DEBUGMSG(ZONE_ERROR, (L"Device:DevReadFile Driver %s make this as async and return error!!!\r\n",lpDev->pszDeviceName));
                CloseHandle(pioPacket->GetAPIHandle());
            }
        }
        else if (pioPacket->GetIoPacketStatus()==IO_PACKET_STATUS_ASYNC) {
            if (pioPacket->IsExternalIo()) { // Async to Async. Set IO Pending.
                SetLastError(ERROR_IO_PENDING);
                retval = FALSE;
            }
            else { // Async Driver, Sync call.
                DWORD dwRet= WaitForSingleObject( pioPacket->GetCompleteEventHandle(),INFINITE);
                retval =(dwRet == WAIT_OBJECT_0);
                if (retval && pioPacket->GetLastCompletedStatus()!=ERROR_SUCCESS) {
                    retval = FALSE;
                    SetLastError(pioPacket->GetLastCompletedStatus());
                }
                if (retval) {
                    if (lpNumBytesRead) {
                        __try {
                            *lpNumBytesRead = pioPacket->GetNumberOfByteTransfered();
                        }__except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
                            SetLastError(ERROR_INVALID_PARAMETER);
                            retval = FALSE;
                        }
                    }
                }
                else
                    ASSERT(FALSE);
            }
        } if (pioPacket->IsExternalIo() && pioPacket->GetIoPacketStatus()== IO_PACKET_STATUS_INITIALIZED) {// Sync Driver, Async call
            pioPacket->SyncCompleteIo(dwResult, ERROR_SUCCESS);
        } 
        pioPacket->DeRef();
    }
    else {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return retval;

}
BOOL IoPckManager::DevWriteFile(LPCVOID buffer, DWORD nBytesToWrite, LPDWORD lpNumBytesWritten,LPOVERLAPPED lpOverlapped)
{
    BOOL    retval = FALSE;
    
    if (!lpNumBytesWritten) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    IoPacket * pioPacket = Request(lpOverlapped,(LPVOID)buffer,nBytesToWrite,NULL,0);
    
    if (pioPacket ) {
        ASSERT( pioPacket->GetIoPacketStatus() == IO_PACKET_STATUS_INITIALIZED );
        DWORD dwResult = (DWORD)-1;
        __try {
            dwResult = DriverWrite(lpDev, dwOpenData,buffer,nBytesToWrite,pioPacket);
            if (lpNumBytesWritten)
                *lpNumBytesWritten = dwResult;
        }__except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);
            dwResult = (DWORD)-1;
            retval = FALSE;
        }
        retval = (dwResult!=(DWORD)-1);
        if (!retval) {
            if (pioPacket->GetAPIHandle()!=NULL) { // Somehow, Driver mark is assync then indicate error as well.
                DEBUGMSG(ZONE_ERROR, (L"Device:DevWriteFile Driver %s make this as async and return error!!!\r\n",lpDev->pszDeviceName));
                CloseHandle(pioPacket->GetAPIHandle());
            }
        }
        else if (pioPacket->GetIoPacketStatus()==IO_PACKET_STATUS_ASYNC) {
            if (pioPacket->IsExternalIo()) { // Async to Async. Set IO Pending.
                SetLastError(ERROR_IO_PENDING);
                retval = FALSE;
            }
            else { // Async Driver, Sync call.
                DWORD dwRet= WaitForSingleObject( pioPacket->GetCompleteEventHandle(),INFINITE);
                retval =(dwRet == WAIT_OBJECT_0);
                if (retval && pioPacket->GetLastCompletedStatus()!=ERROR_SUCCESS) {
                    retval = FALSE;
                    SetLastError(pioPacket->GetLastCompletedStatus());
                }
                if (retval) {
                    __try {
                        *lpNumBytesWritten = pioPacket->GetNumberOfByteTransfered();
                    }__except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
                        SetLastError(ERROR_INVALID_PARAMETER);
                        retval = FALSE;
                    }
                }
                else
                    ASSERT(FALSE);
            }
        } if (pioPacket->IsExternalIo() && pioPacket->GetIoPacketStatus()== IO_PACKET_STATUS_INITIALIZED) {// Sync Driver, Async call
            pioPacket->SyncCompleteIo(dwResult, ERROR_SUCCESS );
        } 
        pioPacket->DeRef();
    }
    else {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return retval;

}
BOOL    IoPckManager::InsertToActiveList(IoPacket * pioPacket)
{
    if (pioPacket && !m_fTerminated) {
        Lock();
        InsertTailList(&m_AsyncIoList,(PLIST_ENTRY)pioPacket);
        Unlock();
        return TRUE;
    }
    else
        return FALSE;
}
BOOL    IoPckManager::RemoveFromActiveList(IoPacket * pioPacket)
{
    if (pioPacket) {
        Lock();
        RemoveEntryList((PLIST_ENTRY)pioPacket);
        Unlock();
        return TRUE;
    }
    else
        return FALSE;
}
IoPacket *  IoPckManager::GetFreeSyncPacket()
{
    IoPacket * pReturn = NULL;
    if (!m_fTerminated) {
        Lock();
        if (IsListEmpty(&m_SyncIoPacketFreeList)) {
            pReturn = new IoPacket(*this,NULL);
        }
        else {
            pReturn = (IoPacket *)RemoveHeadList(&m_SyncIoPacketFreeList);
        }
        Unlock();
    }
    ASSERT(pReturn);
    return pReturn;
}
BOOL   IoPckManager::InsertFreeSyncPacket(IoPacket * pioPacket)
{
    if (pioPacket) {
        Lock();
        InsertHeadList(&m_SyncIoPacketFreeList,(PLIST_ENTRY) pioPacket );
        Unlock();
        return TRUE;
    }
    else
        return FALSE;
}
#define STATIC_IO_HANDLE 0x10
BOOL IoPckManager::CancelIo(BOOL fAll)
{
    BOOL retval = TRUE;
    if (lpDev->fnCancelIo) {
        DWORD dwNumOfIo = 0;
        HANDLE *hAsync = NULL;
        DWORD dwInitialThreadID = GetCurrentThreadId();
        Lock();
        PLIST_ENTRY pCur= m_AsyncIoList.Flink;
        for (DWORD dwIndex=0; dwIndex<MAX_SYNC_ITEM; dwIndex++) {
            if (pCur != &m_AsyncIoList && pCur!=NULL) {
                PLIST_ENTRY pNext = pCur->Flink;
                IoPacket * pioPacket = (IoPacket *)pCur;
                if (fAll || pioPacket->GetCallerThreadID() == dwInitialThreadID ) {
                    dwNumOfIo++;
                }
                pCur = pNext;
            }
            else
                break;
        }
        ASSERT(dwIndex<MAX_SYNC_ITEM);
        DWORD dwCurIndex = 0; 
        HANDLE h_10[STATIC_IO_HANDLE];
        if (dwIndex!=0 && dwIndex<MAX_SYNC_ITEM) {
            hAsync = (dwNumOfIo<=STATIC_IO_HANDLE?h_10:  new HANDLE[dwNumOfIo]);
            if (hAsync) {
                PLIST_ENTRY pCur= m_AsyncIoList.Flink;
                for (dwIndex=0; dwIndex<dwNumOfIo; dwIndex++) {
                    if (pCur != &m_AsyncIoList && pCur!=NULL) {
                        PLIST_ENTRY pNext = pCur->Flink;
                        IoPacket * pioPacket = (IoPacket *)pCur;
                        if (fAll || pioPacket->GetCallerThreadID() == dwInitialThreadID) {
                            hAsync [dwCurIndex++] = pioPacket->GetAPIHandle();
                        }
                        pCur = pNext;
                    }
                    else
                        break;
                }
                    
            }
        }    
        Unlock();
        
        // We have to do it after unlock the structure because driver is going to cancel or complete it.
        if (dwCurIndex && hAsync) {
            for (dwIndex=0; dwIndex < dwCurIndex; dwIndex++) {
                __try {
                    DriverCancelIo(lpDev, dwOpenData,hAsync[dwIndex]);
                } __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
                    SetLastError(ERROR_INVALID_PARAMETER);
                    retval = FALSE;
                }
            }
        }
        if (hAsync!=NULL && hAsync!=h_10)
            delete [] hAsync;
    }
    return retval;
}
BOOL IoPckManager::CancelIoEx(LPOVERLAPPED lpOverlappedIo)
{
    BOOL retval = FALSE;
    if (lpDev->fnCancelIo) {
        HANDLE hCancelingHandle = NULL;
        Lock();
        PLIST_ENTRY pCur= m_AsyncIoList.Flink;
        for (DWORD dwIndex=0; dwIndex<MAX_SYNC_ITEM; dwIndex++) {
            if (pCur != &m_AsyncIoList && pCur!=NULL) {
                PLIST_ENTRY pNext = pCur->Flink;
                IoPacket * pioPacket = (IoPacket *)pCur;
                if (pioPacket->GetOriginalOverlapped() == lpOverlappedIo ){
                    hCancelingHandle = pioPacket->GetAPIHandle();
                    break;
                }
                pCur = pNext;
            }
            else
                break;
        }
        ASSERT(dwIndex<MAX_SYNC_ITEM);
        Unlock();
        
        if (hCancelingHandle  ) {
            __try {
                retval = DriverCancelIo(lpDev,dwOpenData,hCancelingHandle);
            }__except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_INVALID_PARAMETER);
                retval = FALSE;
            }
        }
    }
    return retval;
}

