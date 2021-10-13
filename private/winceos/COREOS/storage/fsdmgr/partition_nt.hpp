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

#ifndef UNDER_CE

#ifndef __PARTITION_H__
#define __PARTITION_H__

#include <partdrv.h>
#include "detector.hpp"
#include <dlist.h>

// The lower 16bits of the MountFlags are used by filesys
#define MOUNTFLAGS_TYPE_HIDDEN      AFS_FLAG_HIDDEN     // 1  - Hidden file system
#define MOUNTFLAGS_TYPE_BOOTABLE    AFS_FLAG_BOOTABLE   // 2  - May contain system registry
#define MOUNTFLAGS_TYPE_ROOTFS      AFS_FLAG_ROOTFS     // 4  - Mount as root of file system, "\"
#define MOUNTFLAGS_TYPE_HIDEROM     AFS_FLAG_HIDEROM    // 8  - Hide ROM when mounting FS root; use with FLAG_ROOTFS
#define MOUNTFLAGS_TYPE_MOUNTROM    AFS_FLAG_MOUNTROM   // 16 - Mount the new filesystem as an additional ROM filesystem

#define PARTITION_STATE_DISABLED        0x00000002  //  Partition is Disabled
#define PARTITION_STATE_MEDIA_DETACHED  0x00000004  //  Media is detached from this partition
#define PARTITION_STATE_MOUNTED         0x00000008  //  Partition is Mounted

typedef DWORD (* POPENSTORE)(HANDLE hDisk, PDWORD pdwStoreId);
typedef void  (* PCLOSESTORE)(DWORD dwStoreId);
typedef DWORD (* PFORMATSTORE)(DWORD dwStoreId);
typedef DWORD (* PISSTOREFORMATTED)(DWORD dwStoreId);
typedef DWORD (* PGETSTOREINFO)(DWORD dwStoreId, PD_STOREINFO *info);
typedef DWORD (* PCREATEPARTITION)(DWORD dwStoreId, LPCTSTR partName, BYTE bPartType, SECTORNUM numSectors, BOOL bAuto);
typedef DWORD (* PDELETEPARTITION)(DWORD dwStoreId, LPCTSTR partName);
typedef DWORD (* PRENAMEPARTITION)(DWORD dwStoreId, LPCTSTR oldName, LPCTSTR newName);
typedef DWORD (* PSETPARTITIONATTRS)(DWORD dwStoreId, LPCTSTR partName, DWORD attrs);
typedef DWORD (* PSETPARTITIONSIZE)(DWORD dwStoreId, LPCTSTR partName, SECTORNUM numSectors);
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
typedef DWORD (* PMEDIACHANGEEVENT)(DWORD StoreId, STORAGE_MEDIA_CHANGE_EVENT_TYPE EventId, STORAGE_MEDIA_ATTACH_RESULT* pResult);
typedef DWORD (* PSTOREIOCONTROL)(DWORD StoreId, DWORD Ioctl, LPVOID pInBuf, DWORD nInBufSize, LPVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned, LPOVERLAPPED pOverlapped);

class PartitionDriver_t
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
    PSETPARTITIONSIZE   m_pSetPartitionSize;
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
    PMEDIACHANGEEVENT   m_pMediaChangeEvent;
    PSTOREIOCONTROL     m_pStoreIoControl;
public:
    PartitionDriver_t() :
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
        m_pSetPartitionSize(NULL),
        m_pFormatPartition(NULL),
        m_pGetPartitionInfo(NULL),
        m_pFindPartitionStart(NULL),
        m_pFindPartitionNext(NULL),
        m_pFindPartitionClose(NULL),
        m_pOpenPartition(NULL),
        m_pClosePartition(NULL),
        m_pReadPartition(NULL),
        m_pWritePartition(NULL),
        m_pDeviceIoControl(NULL),
        m_pMediaChangeEvent(NULL),
        m_pStoreIoControl(NULL)
    {
    }
    virtual ~PartitionDriver_t()
    {
        if (m_hPartDriver)
            FreeLibrary( m_hPartDriver);
    }
    DWORD LoadPartitionDriver(const WCHAR *szPartDriver);

    inline DWORD OpenStore(HANDLE hDisk, PDWORD pdwStoreId)
    {
        __try {
            return m_pOpenStore(hDisk,pdwStoreId);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }
    }
    inline void CloseStore(DWORD dwStoreId)
    {
        __try {
            m_pCloseStore(dwStoreId);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    /* stores */
    inline DWORD FormatStore(DWORD dwStoreId)
    {
        __try {
            return m_pFormatStore(dwStoreId);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }
    }

    inline DWORD IsStoreFormatted(DWORD dwStoreId)
    {
        __try {
            return m_pIsStoreFormatted( dwStoreId);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }
    }

    inline DWORD GetStoreInfo(DWORD dwStoreId, PD_STOREINFO *pInfo)
    {
        __try {
            return m_pGetStoreInfo(dwStoreId, pInfo);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }
    }

    /* partitions management */
    inline DWORD CreatePartition(DWORD dwStoreId, LPCTSTR partName, BYTE bPartType, SECTORNUM numSectors, BOOL bAuto)
    {
        __try {
            return m_pCreatePartition(dwStoreId, partName, bPartType, numSectors, bAuto);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }
    }
    inline DWORD DeletePartition(DWORD dwStoreId, LPCTSTR partName)
    {
        __try {
            return m_pDeletePartition(dwStoreId, partName);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }
    }

    inline DWORD RenamePartition(DWORD dwStoreId, LPCTSTR oldName, LPCTSTR newName)
    {
        __try {
            return m_pRenamePartition(dwStoreId, oldName, newName);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }
    }
    inline DWORD SetPartitionAttrs(DWORD dwStoreId, LPCTSTR partName, DWORD attrs)
    {
        __try {
            return m_pSetPartitionAttrs(dwStoreId, partName, attrs);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }
    }
    inline DWORD SetPartitionSize(DWORD dwStoreId, LPCTSTR partName, SECTORNUM snNumSectors)
    {
        __try {
            return m_pSetPartitionSize(dwStoreId, partName, snNumSectors);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }
    }

    inline DWORD FormatPartition(DWORD dwStoreId, LPCTSTR partName, BYTE bPartType, BOOL bAuto)
    {
        __try {
            return m_pFormatPartition(dwStoreId, partName, bPartType, bAuto);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }
    }

    inline DWORD GetPartitionInfo(DWORD dwStoreId, LPCTSTR partName, PD_PARTINFO *pInfo)
    {
        __try {
            return m_pGetPartitionInfo(dwStoreId, partName, pInfo);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }
    }

    inline DWORD FindPartitionStart(DWORD dwStoreId,PDWORD pdwSearchId)
    {
        __try {
            return m_pFindPartitionStart(dwStoreId, pdwSearchId);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }
    }

    inline DWORD FindPartitionNext(DWORD dwSearchId, PD_PARTINFO *pInfo)
    {
        __try {
            return m_pFindPartitionNext(dwSearchId, pInfo);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }
    }

    inline void FindPartitionClose(DWORD dwSearchId)
    {
        __try {
            m_pFindPartitionClose(dwSearchId);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
        }
    }


    /* partition I/O */
    inline DWORD OpenPartition(DWORD dwStoreId, LPCTSTR partName, PDWORD pdwPartitionId)
    {
        __try {
            return m_pOpenPartition(dwStoreId, partName, pdwPartitionId);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }
    }

    inline void ClosePartition(DWORD dwPartitionId)
    {
        __try {
            m_pClosePartition(dwPartitionId);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            DEBUGCHK (0);
        }
    }
    inline DWORD DeviceIoControl(DWORD dwPartitionId, DWORD dwCode, LPVOID pInBuf, DWORD nInBufSize, LPVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned)
    {
        __try {
            return m_pDeviceIoControl(dwPartitionId, dwCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }
    }

    inline DWORD MediaChangeEvent (DWORD StoreId, STORAGE_MEDIA_CHANGE_EVENT_TYPE EventId, STORAGE_MEDIA_ATTACH_RESULT* pResult)
    {
        if (!m_pMediaChangeEvent) {
            // The partition driver does not export the "PD_ MediaChangeEvent"
            // function.
            return ERROR_NOT_SUPPORTED;
        }

        __try {
            return m_pMediaChangeEvent (StoreId, EventId, pResult);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_GEN_FAILURE;
        }
    }

    inline DWORD StoreIoControl (DWORD StoreId, HANDLE hDisk, DWORD Ioctl, LPVOID pInBuf, DWORD nInBufSize, LPVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned, LPOVERLAPPED pOverlapped)
    {
        BOOL fRet;

        if (m_pStoreIoControl) {

            // The partition driver exports a PD_StoreIoControl function, so pass
            // it the IOCTL.

            fRet = m_pStoreIoControl (StoreId, Ioctl, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned, pOverlapped);

#ifdef UNDER_CE
        } else if ((pInBuf || pOutBuf) &&
            (!pInBuf || !::IsKModeAddr (reinterpret_cast<DWORD> (pInBuf))) &&
            (!pOutBuf || !::IsKModeAddr (reinterpret_cast<DWORD> (pOutBuf)))) {

            // All top level buffers exist in user VM. In this case, the IOCTL is
            // being forwarded, so continue the forwarding. The existing caller
            // process will be the caller process as viewed by the driver.

            fRet = ::ForwardDeviceIoControl (hDisk, Ioctl, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned, pOverlapped);
#endif // UNDER_CE

        } else {

            // At least one of the top level buffers is in kernel VM, so perform
            // a normal DeviceIoControl. This process (kernel) will be the caller
            // process as viewed by the driver.

            fRet = ::DeviceIoControl (hDisk, Ioctl, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned, pOverlapped);
        }

        return fRet;
    }

};

class PartitionDisk_t : public MountableDisk_t
{
private:
    DWORD m_dwAttrs;
    PD_PARTINFO m_pi;
    TCHAR m_szFriendlyName[MAX_PATH];
    TCHAR m_szFileSys[FILESYSNAMESIZE];
    
    HANDLE m_hPartition;
    DetectorState_t m_DetectorState;
    BOOL m_fFormatOnMount;
    DWORD m_dwState;

    LONG m_RefCount;
    TCHAR m_szFolderName[FOLDERNAMESIZE];
    TCHAR m_szPartitionName[PARTITIONNAMESIZE];

    PartitionDriver_t *m_pPartDriver; // Just a reference... StoreDisk_t takes care of the lifetime of this object !!!

public:
    
    DWORD m_dwPartitionId;
    DWORD m_dwStoreId;

    StoreDisk_t *m_pStore;

    DLIST m_link;   //Link to the next PartitionDisk

    PartitionDisk_t(StoreDisk_t *pStore, DWORD dwStoreId, DWORD dwPartitionId, PartitionDriver_t *pPartDriver, const WCHAR *szFolderName, BlockDevice_t* /* pBlockDevice */);
    PartitionDisk_t();
    virtual ~PartitionDisk_t();

public:

    inline LONG AddRef()
    {
        LONG RefCount;
        RefCount = InterlockedIncrement(&m_RefCount);

        return RefCount;
    }

    inline LONG Release()
    {
        LONG RefCount;
        RefCount = InterlockedDecrement(&m_RefCount);

        if (RefCount == 0)
        {
            delete this;
        }
        return RefCount;
    }

    inline DWORD RefCount()         { return m_RefCount; }

    inline BOOL IsMediaDetached()   { return m_dwState & PARTITION_STATE_MEDIA_DETACHED; }
    inline void SetMediaDetached()  { m_dwState |= PARTITION_STATE_MEDIA_DETACHED; }
    inline void SetMediaAttached()  { m_dwState &= ~PARTITION_STATE_MEDIA_DETACHED; }
    
    inline BOOL IsMounted()         { return m_dwState & PARTITION_STATE_MOUNTED; }
    inline void SetMounted()        { m_dwState |= PARTITION_STATE_MOUNTED; }
    inline void SetUnmounted()      { m_dwState &= ~PARTITION_STATE_MOUNTED; }

    inline BOOL IsDisabled()        { return m_dwState & PARTITION_STATE_DISABLED; }
    inline void SetDisabled()       { m_dwState |= PARTITION_STATE_DISABLED; }
    inline void SetEnabled()        { m_dwState &= ~PARTITION_STATE_DISABLED; }

    inline const WCHAR* const GetName( )  { return m_szPartitionName; }

public:
    BOOL    Init(LPCTSTR szPartitionName, LPCTSTR RootRegKey);

    BOOL    Mount(HANDLE hPartition);
    BOOL    Unmount();
    void    Enable();
    void    Disable();
    DWORD   CreateHandle(HANDLE *phHandle, HANDLE hProc, DWORD dwAccess);

    inline BOOL SetPartitionAttributes(DWORD dwAttrs)
    {
        if(ERROR_SUCCESS == m_pPartDriver->SetPartitionAttrs(m_dwStoreId, m_szPartitionName, dwAttrs))
        {
            m_dwAttrs = dwAttrs;
            return TRUE;
        }
        return FALSE;
    }
    inline BOOL SetPartitionSize(SECTORNUM snNumSectors)
    {
        if (ERROR_SUCCESS == m_pPartDriver->SetPartitionSize(m_dwStoreId, m_szPartitionName, snNumSectors)) {
            // reload partition info
            m_pPartDriver->GetPartitionInfo(m_dwStoreId, m_szPartitionName, &m_pi);
            return TRUE;
        } else {
            return FALSE;
        }
    }
    inline VOID SetFormatOnMount(BOOL fFormatOnMount)
    {
        m_fFormatOnMount = fFormatOnMount;
    }

    BOOL GetStoreInfo(STOREINFO *pInfo);
    BOOL GetPartitionInfo(PARTINFO *pInfo);
    BOOL RenamePartition(LPCTSTR szNewName);
    BOOL FormatPartition(BYTE bPartType, BOOL bAuto);

    virtual void SignalActivity ();

    virtual LRESULT GetName (WCHAR* pDiskName, DWORD NameChars);

    virtual LRESULT DiskIoControl (DWORD IoControlCode, void* pInBuf,
            DWORD InBufBytes, void* pOutBuf, DWORD OutBufBytes,
            DWORD* pBytesReturned, OVERLAPPED* pOverlapped);

    virtual LRESULT GetVolumeInfo (CE_VOLUME_INFO* pInfo);

    virtual HANDLE GetDeviceHandle ();

    virtual LRESULT GetSecurityClassName(WCHAR* pSecurityClassName, DWORD NameChars);

    LRESULT RunAllDetectors (HKEY hFsKey,
            __out_ecount(FileSystemNameChars) WCHAR* pFileSystemName,
            size_t FileSystemNameChars);

    LRESULT DetectFileSystem (__out_ecount(FileSystemNameChars) WCHAR* pFileSystemName,
        size_t FileSystemNameChars);

    BOOL ComparePartitions (PartitionDisk_t *pPartition);

    virtual BOOL IsAvailable();


#ifdef UNDER_CE
    virtual BOOL DoIOControl(DWORD IoControlCode, void* pInBuf, DWORD InBufSize, 
                    void* pOutBuf, DWORD OutBufSize, DWORD* pBytesReturned, OVERLAPPED* pOverlapped);
#endif

};

#endif //__PARTITION_H__

#endif /* !UNDER_CE */
