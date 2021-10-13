//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __PARTITION_H__
#define __PARTITION_H__

// The lower 16bits of the MountFlags are used by filesys
#define MOUNTFLAGS_TYPE_HIDDEN		AFS_FLAG_HIDDEN     // 1  - Hidden file system
#define MOUNTFLAGS_TYPE_BOOTABLE	AFS_FLAG_BOOTABLE   // 2  - May contain system registry
#define MOUNTFLAGS_TYPE_ROOTFS		AFS_FLAG_ROOTFS     // 4  - Mount as root of file system, "\"
#define MOUNTFLAGS_TYPE_HIDEROM		AFS_FLAG_HIDEROM    // 8  - Hide ROM when mounting FS root; use with FLAG_ROOTFS
#define MOUNTFLAGS_TYPE_MOUNTROM	AFS_FLAG_MOUNTROM   // 16 - Mount the new filesystem as an additional ROM filesystem

// The upper 16bits of MountFlags are used by StorageManager
#define MOUNTFLAGS_TYPE_NODISMOUNT	0x00010000			//  Do not allow dismount of this partition

#include <windows.h>
#include <storemgr.h>
#include <fsdmgrp.h>
#include <partdrv.h>

typedef DWORD (* POPENSTORE)(HANDLE hDisk, PDWORD pdwStoreId);
typedef void  (* PCLOSESTORE)(DWORD dwStoreId);
typedef DWORD (* PFORMATSTORE)(DWORD dwStoreId);
typedef DWORD (* PISSTOREFORMATTED)(DWORD dwStoreId);
typedef DWORD (* PGETSTOREINFO)(DWORD dwStoreId, PD_STOREINFO *info);
typedef DWORD (* PCREATEPARTITION)(DWORD dwStoreId, LPCTSTR partName, BYTE bPartType, SECTORNUM numSectors, BOOL bAuto);
typedef DWORD (* PDELETEPARTITION)(DWORD dwStoreId, LPCTSTR partName);
typedef DWORD (* PRENAMEPARTITION)(DWORD dwStoreId, LPCTSTR oldName, LPCTSTR newName);
typedef DWORD (* PSETPARTITIONATTRS)(DWORD dwStoreId, LPCTSTR partName, DWORD attrs);
typedef DWORD (* PFORMATPARTITION)(DWORD dwStoreId, LPCTSTR partName, BYTE bPartType, BOOL bAuto);
typedef DWORD (* PGETPARTITIONINFO)(DWORD dwStoreId, LPCTSTR partName, PPD_PARTINFO info);
typedef DWORD (* PFINDPARTITIONSTART)(DWORD dwStoreId, PDWORD pdwSearchId);
typedef DWORD (* PFINDPARTITIONNEXT)(DWORD dwSearchId, PPD_PARTINFO info);
typedef void  (* PFINDPARTITIONCLOSE)(DWORD dwSearchId);
typedef DWORD (* POPENPARTITION)(DWORD dwStoreId, LPCTSTR partName,PDWORD pdwPartitionId);
typedef void  (* PCLOSEPARTITION)(DWORD dwPartitionId);
typedef DWORD (* PREADPARTITION)(DWORD dwPartitionId, PBYTE pInBuf, DWORD nInBufSize, PBYTE pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned);
typedef DWORD (* PWRITEPARTITION)(DWORD dwPartitionId, PBYTE pInBuf, DWORD nInBufSize, PBYTE pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned);
typedef DWORD (* PPD_DEVICEIOCONTROL)(DWORD dwPartitionId, DWORD dwCode, LPVOID pInBuf, DWORD nInBufSize, LPVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned);

class CPartDriver
{
private:
    HMODULE             m_hPartDriver;
    POPENSTORE          m_pOpenStore;
    PCLOSESTORE         m_pCloseStore;
    PFORMATSTORE        m_pFormatStore;
    PISSTOREFORMATTED   m_pIsStoreFormatted;
    PGETSTOREINFO       m_pGetStoreInfo;
    PCREATEPARTITION    m_pCreatePartition;
    PDELETEPARTITION    m_pDeletePartition;
    PRENAMEPARTITION    m_pRenamePartition;
    PSETPARTITIONATTRS  m_pSetPartitionAttrs;
    PFORMATPARTITION    m_pFormatPartition;
    PGETPARTITIONINFO   m_pGetPartitionInfo;
    PFINDPARTITIONSTART m_pFindPartitionStart;
    PFINDPARTITIONNEXT  m_pFindPartitionNext;
    PFINDPARTITIONCLOSE m_pFindPartitionClose;
    POPENPARTITION      m_pOpenPartition;
    PCLOSEPARTITION     m_pClosePartition;
    PREADPARTITION      m_pReadPartition;
    PWRITEPARTITION     m_pWritePartition;
    PPD_DEVICEIOCONTROL m_pDeviceIoControl;
public:    
    CPartDriver() :
        m_hPartDriver(NULL),
        m_pOpenStore(NULL),
        m_pCloseStore(NULL),
        m_pFormatStore(NULL),
        m_pIsStoreFormatted(NULL),
        m_pGetStoreInfo(NULL),
        m_pCreatePartition(NULL),
        m_pDeletePartition(NULL),
        m_pRenamePartition(NULL),
        m_pSetPartitionAttrs(NULL),
        m_pFormatPartition(NULL),
        m_pGetPartitionInfo(NULL),
        m_pFindPartitionStart(NULL),
        m_pFindPartitionNext(NULL),
        m_pFindPartitionClose(NULL),
        m_pOpenPartition(NULL),
        m_pClosePartition(NULL),
        m_pReadPartition(NULL),
        m_pWritePartition(NULL),
        m_pDeviceIoControl(NULL)
    {
    }
    virtual ~CPartDriver()
    {
        if (m_hPartDriver)
            FreeLibrary( m_hPartDriver);
    }        
    DWORD LoadPartitionDriver(TCHAR *szPartDriver);
    
    inline DWORD OpenStore(HANDLE hDisk, PDWORD pdwStoreId)
    {
        __try {
            return m_pOpenStore(hDisk,pdwStoreId);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }    
    }
    inline void CloseStore(DWORD dwStoreId)
    {
        __try {
            m_pCloseStore(dwStoreId);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
        }    
    }    

    /* stores */
    inline DWORD FormatStore(DWORD dwStoreId)
    {
        __try {
            return m_pFormatStore(dwStoreId);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }    
    }
    
    inline DWORD IsStoreFormatted(DWORD dwStoreId)
    {
        __try {
            return m_pIsStoreFormatted( dwStoreId);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }    
    }
    
    inline DWORD GetStoreInfo(DWORD dwStoreId, PD_STOREINFO *pInfo)
    {
        __try {
            return m_pGetStoreInfo(dwStoreId, pInfo);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }    
    }

    /* partitions management */
    inline DWORD CreatePartition(DWORD dwStoreId, LPCTSTR partName, BYTE bPartType, SECTORNUM numSectors, BOOL bAuto)
    {
        __try {
            return m_pCreatePartition(dwStoreId, partName, bPartType, numSectors, bAuto);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }    
    }
    inline DWORD DeletePartition(DWORD dwStoreId, LPCTSTR partName)
    {
        __try {
            return m_pDeletePartition(dwStoreId, partName);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }    
    }
    
    inline DWORD RenamePartition(DWORD dwStoreId, LPCTSTR oldName, LPCTSTR newName)
    {
        __try {
            return m_pRenamePartition(dwStoreId, oldName, newName);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }    
    }
    inline DWORD SetPartitionAttrs(DWORD dwStoreId, LPCTSTR partName, DWORD attrs)
    {
        __try {
            return m_pSetPartitionAttrs(dwStoreId, partName, attrs);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }    
    }
    
    inline DWORD FormatPartition(DWORD dwStoreId, LPCTSTR partName, BYTE bPartType, BOOL bAuto)
    {
        __try {
            return m_pFormatPartition(dwStoreId, partName, bPartType, bAuto);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }    
    }
    
    inline DWORD GetPartitionInfo(DWORD dwStoreId, LPCTSTR partName, PD_PARTINFO *pInfo)
    {
        __try {
            return m_pGetPartitionInfo(dwStoreId, partName, pInfo);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }    
    }
    
    inline DWORD FindPartitionStart(DWORD dwStoreId,PDWORD pdwSearchId)
    {
        __try {
            return m_pFindPartitionStart(dwStoreId, pdwSearchId);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }    
    }
    
    inline DWORD FindPartitionNext(DWORD dwSearchId, PD_PARTINFO *pInfo)
    {
        __try {
            return m_pFindPartitionNext(dwSearchId, pInfo);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }    
    }
    
    inline void FindPartitionClose(DWORD dwSearchId)
    {
        __try {
            m_pFindPartitionClose(dwSearchId);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
        }    
    }
    

    /* partition I/O */
    inline DWORD OpenPartition(DWORD dwStoreId, LPCTSTR partName, PDWORD pdwPartitionId)
    {
        __try {
            return m_pOpenPartition(dwStoreId, partName, pdwPartitionId);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }    
    }
    
    inline void ClosePartition(DWORD dwPartitionId)
    {
        __try {
            m_pClosePartition(dwPartitionId);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
        }    
    }
    inline DWORD DeviceIoControl(DWORD dwPartitionId, DWORD dwCode, LPVOID pInBuf, DWORD nInBufSize, LPVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned)
    {
        __try {
            return m_pDeviceIoControl(dwPartitionId, dwCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }    
    }
};   



class CPartition
{
public:
    DWORD m_dwPartitionId;
    CPartDriver *m_pPartDriver; // Just a reference... CStore takes care of the lifetime of this object !!!
    PD_PARTINFO m_pi;
    DWORD m_dwFlags;
    TCHAR m_szPartitionName[PARTITIONNAMESIZE];
    TCHAR m_szFolderName[FOLDERNAMESIZE];
    TCHAR m_szModuleName[MAX_PATH];
    TCHAR m_szFriendlyName[MAX_PATH];
    TCHAR m_szFileSys[FILESYSNAMESIZE];
    TCHAR m_szRootKey[MAX_PATH];
    DWORD m_dwStoreId;
    HMODULE m_hFSD;
    PDSK m_pDsk;
    CPartition *m_pNextPartition;
    DWORD m_dwMountFlags;
    HANDLE m_hActivityEvent;
    HANDLE m_hPartition;
    
    
    CPartition(DWORD dwStoreId, DWORD dwPartitionId, CPartDriver *pPartDriver, TCHAR *szFolderName) :
        m_dwStoreId(dwStoreId),
        m_dwPartitionId( dwPartitionId),
        m_pPartDriver( pPartDriver),
        m_pNextPartition(NULL),
        m_hFSD(NULL),
        m_pDsk(NULL),
        m_dwMountFlags(0),
        m_hActivityEvent(NULL),
        m_hPartition(NULL),
        m_dwFlags(0)
    {
        memset( &m_pi, 0, sizeof(PD_PARTINFO));
        wcscpy( m_szPartitionName, L"");
        wcscpy( m_szFileSys, L""); 
        wcsncpy( m_szFolderName, szFolderName, FOLDERNAMESIZE-1);
        m_szFolderName[FOLDERNAMESIZE-1] = 0;
        wcscpy( m_szFriendlyName, L"");
        wcscpy( m_szModuleName, L"");
        wcscpy( m_szRootKey, L"");
    }
    virtual ~CPartition()
    {
        if (m_hFSD)
            FreeLibrary( m_hFSD);
    }
public:
    BOOL LoadPartition(LPCTSTR szPartitionName);
    BOOL MountPartition(HANDLE hPartition, LPCTSTR szRootRegKey, LPCTSTR szDefaultFileSystem, DWORD dwMountFlags, HANDLE hActivityEvent);
    inline BOOL IsPartitionMounted()
    {
        return m_dwFlags & PARTITION_ATTRIBUTE_MOUNTED;
    }
    BOOL UnmountPartition();
    inline BOOL SetPartitionAttributes(DWORD dwAttrs)
    {
        return (ERROR_SUCCESS == m_pPartDriver->SetPartitionAttrs(m_dwStoreId, m_szPartitionName, dwAttrs));
    }
    BOOL GetPartitionInfo(PARTINFO *pInfo);
    BOOL RenamePartition(LPCTSTR szNewName);
    BOOL FormatPartition(BYTE bPartType, BOOL bAuto);
    inline DWORD DeviceIoControl(DWORD dwIoControlCode, PVOID pInBuf, DWORD nInBufSize, PVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned)
    {
        return m_pPartDriver->DeviceIoControl(m_dwPartitionId, dwIoControlCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned);
    }
    BOOL GetFSDString( const TCHAR *szValueName, TCHAR *szValue, DWORD dwValueSize);
    BOOL GetFSDValue( const TCHAR *szValueName, LPDWORD pdwValue);
};

#endif //__PARTITION_H__
