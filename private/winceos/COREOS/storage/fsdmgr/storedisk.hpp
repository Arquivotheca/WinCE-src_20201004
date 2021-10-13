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

#ifdef UNDER_CE

#ifndef __STORAGE__
#define __STORAGE__

#include <partdrv.h>
#include <cdioctl.h>

#include <map>
#include "fsdmapincludes.h"

using namespace std;

class BlockDevice_t;
class PartitionDriver_t;
class PartitionDisk_t;

#define STOREHANDLE_TYPE_SEARCH                 (1 << 0)
#define STOREHANDLE_TYPE_INTERNAL               (1 << 1)

#define STORE_STATE_MOUNTED                     (1 << 1)
#define STORE_STATE_DETACHED                    (1 << 2)
#define STORE_STATE_MEDIA_DETACHED              (1 << 3)
#define STORE_STATE_MEDIA_DETACH_COMPLETED      (1 << 4)
#define STORE_STATE_MARKED_FOR_REFRESH          (1 << 5)
#define STORE_STATE_DETACH_COMPLETED            (1 << 6)

#define STORE_FLAG_ACTIVITYTIMER                (1 << 10)
#define STORE_FLAG_DISABLE_ON_SUSPEND           (1 << 11)


#define STORE_HANDLE_SIG 0x00596164
#define PART_HANDLE_SIG  0x64615900

struct PartStoreInfo
{
    LPWSTR pszInfoName;
    WCHAR  szInfoName[STORENAMESIZE + DEVICENAMESIZE + 1];
    union 
    {
       PARTINFO  partInfo;
       STOREINFO storeInfo;
    }u;

    PartStoreInfo()
    {
        pszInfoName = &szInfoName[0];
    }

    ~PartStoreInfo()
    {
        pszInfoName = NULL;
    }
};

class  StoreHandle_t
{
private:
    DWORD m_dwSig;
    HANDLE m_hProc;
    DWORD m_dwFlags;
    DWORD m_dwAccess;

public:
    class StoreDisk_t *m_pStore;
    class PartitionDisk_t *m_pPartition;

    inline HANDLE GetProc()     { return m_hProc; }
    inline DWORD GetAccess()    { return m_dwAccess; }
    void Close();

    map <wchar_t*, PartStoreInfo*, lt_compare> *m_pPartStoreInfoMap;

    StoreHandle_t(HANDLE hProc,DWORD dwAccess, DWORD dwFlags, DWORD dwSig) :
    m_dwSig(dwSig),
    m_pStore(NULL),
    m_pPartition(NULL),
    m_dwFlags(dwFlags),
    m_hProc(hProc),
    m_dwAccess(dwAccess)
    {

        m_pPartStoreInfoMap = new map <wchar_t*, PartStoreInfo*, lt_compare>();

        //Search handles are always external
        if (m_dwFlags != STOREHANDLE_TYPE_SEARCH ) {
            if (m_hProc == reinterpret_cast<HANDLE> (GetCurrentProcessId())) {
                 m_dwFlags = STOREHANDLE_TYPE_INTERNAL;
            }
        }
    }

    ~StoreHandle_t()
    {
        if (m_pPartStoreInfoMap->size() > 0) {
            map <wchar_t*, PartStoreInfo*, lt_compare>::iterator iter = NULL;

            for (iter = m_pPartStoreInfoMap->begin(); iter != m_pPartStoreInfoMap->end(); iter++)
            {
                //Release resources; key is released when object is deleted
                delete iter->second;
            }
            m_pPartStoreInfoMap->clear();
        }

		delete m_pPartStoreInfoMap;
    }

    void AddToPartStoreInfoMap(PartStoreInfo* pPartStoreInfo)
    {
        m_pPartStoreInfoMap->insert(std::pair<wchar_t*, PartStoreInfo*>(&(*pPartStoreInfo->u.storeInfo.szDeviceName), &(*pPartStoreInfo)));
    }

    PartStoreInfo * GetPartStoreInfoFromMap(LPWSTR pInfoName)
    {
        PartStoreInfo * pPartStoreInfo = NULL;

        map <wchar_t*, PartStoreInfo*, lt_compare>::iterator iter = m_pPartStoreInfoMap->find(pInfoName);
        if (iter != m_pPartStoreInfoMap->end())
        {
            pPartStoreInfo = iter->second;
        }

        return pPartStoreInfo;
    }

    BOOL DeletePartStoreInfoFromMap(LPWSTR pInfoName)
    {
        BOOL           bRet           = FALSE;
        PartStoreInfo *pPartStoreInfo = NULL;

        map <wchar_t*, PartStoreInfo*, lt_compare>::iterator iter = 
            m_pPartStoreInfoMap->find(pInfoName);
        if (iter != m_pPartStoreInfoMap->end())
        {
            PartStoreInfo * pOldPartStoreInfo = iter->second;

            m_pPartStoreInfoMap->erase(iter);
            delete pOldPartStoreInfo;

            if (m_pPartStoreInfoMap->empty())
            {
                m_pPartStoreInfoMap->clear();
            }

            bRet = TRUE;
        }
        return bRet;
    }

    BOOL PartStoreInfoMapIsEmpty(void)
    {
        return m_pPartStoreInfoMap->empty();
    }

    PartStoreInfo * GetFirstPartStoreInfoFromMap()
    {
        PartStoreInfo *pPartStoreInfo = NULL;

        if (!m_pPartStoreInfoMap->empty())
        {
            pPartStoreInfo = m_pPartStoreInfoMap->begin()->second;
        }

        return pPartStoreInfo;
    }
};

class StoreDisk_t : public LogicalDisk_t
{
private:
    HANDLE m_hStore;
    TCHAR  m_szDeviceName[MAX_PATH];
    TCHAR  m_szStoreName[STORENAMESIZE];
    TCHAR  m_szFolderName[FOLDERNAMESIZE];
    TCHAR  m_szPartDriverName[32];
    TCHAR  m_szPartDriver[MAX_PATH];
    TCHAR  m_szRootRegKey[MAX_PATH];
    TCHAR  m_szDefaultFileSystem[MAX_PATH];
    GUID   m_DeviceGuid;
    DWORD  m_dwPartCount;
    DWORD  m_dwMountCount;
    DWORD  m_dwPowerCount;
    DWORD  m_dwFlags;
    DWORD  m_dwState;
    TCHAR  m_szActivityName[MAX_PATH];
    HANDLE m_hActivityEvent;
    DWORD  m_dwDefaultMountFlags;
    PBYTE  m_pStorageId;
    LONG   m_RefCount;
public:
    PartitionDriver_t *m_pPartDriver;
    map <TCHAR*, PartitionDisk_t*, lt_compare> *m_pPartitionMap;

    PD_STOREINFO m_si;
    CRITICAL_SECTION m_cs;
    STORAGEDEVICEINFO m_sdi;
    DISK_INFO m_di;
    BlockDevice_t *m_pBlockDevice;
    DWORD m_dwStoreId;
public:
    HANDLE m_hDisk;
    HANDLE m_hStoreDisk;

public:
   StoreDisk_t(const WCHAR *szDeviceName, const GUID* pDeviceGuid);
   StoreDisk_t();
   virtual ~StoreDisk_t();
public:
    inline void Lock()
    {
        EnterCriticalSection( &m_cs);
    }
    inline void Unlock()
    {
        LeaveCriticalSection( &m_cs);
    }

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

    inline DWORD RefCount() { return m_RefCount; }

    inline DWORD GetDefaultMountFlags( )            { return m_dwDefaultMountFlags; }

    inline const WCHAR* const GetDeviceName( )      { return m_szDeviceName; }
    inline const WCHAR* const GetRootRegKey ( )     { return m_szRootRegKey; }
    inline const WCHAR* const GetDefaultFSName( )   { return m_szDefaultFileSystem; }


    //Returns TRUE is the store is either Marked for Detach or already detached.
    inline BOOL IsDetached()                    { return ( m_dwState & STORE_STATE_DETACHED ); }
    inline void SetDetached()                   { m_dwState |= STORE_STATE_DETACHED; }
    inline void ClearDetached()                 { m_dwState &= ~STORE_STATE_DETACHED; }

    inline BOOL IsDetachCompleted()             { return ( m_dwState & STORE_STATE_DETACH_COMPLETED ); }
    inline void SetDetachCompleted()            { m_dwState |= STORE_STATE_DETACH_COMPLETED; }

    inline BOOL IsMounted()                     { return ( m_dwState & STORE_STATE_MOUNTED); }
    inline void SetMounted()                    { m_dwState = STORE_STATE_MOUNTED; }


    inline BOOL IsReadOnly()                    { return ( m_dwFlags & STORE_ATTRIBUTE_READONLY ); }
    inline BOOL IsDisableOnSuspend()            { return ( m_dwFlags & STORE_FLAG_DISABLE_ON_SUSPEND ); }
    inline BOOL IsRemovable()                   { return ( m_dwFlags & STORE_ATTRIBUTE_REMOVABLE ); }

    inline BOOL IsRefreshNeeded()               { return (m_dwState & STORE_STATE_MARKED_FOR_REFRESH); }
    inline void SetRefreshNeeded( )             { m_dwState |= STORE_STATE_MARKED_FOR_REFRESH; }
    inline void ClearRefreshNeeded( )           { m_dwState &= ~STORE_STATE_MARKED_FOR_REFRESH; }

    inline BOOL IsMediaDetached()               { return ( m_dwState & STORE_STATE_MEDIA_DETACHED ); }
    inline void SetMediaDetached()              { m_dwState |= STORE_STATE_MEDIA_DETACHED ; }
    inline void ClearMediaDetached()            { m_dwState &= ~STORE_STATE_MEDIA_DETACHED ; }

    inline BOOL IsMediaDetachCompleted()        { return ( m_dwState & STORE_STATE_MEDIA_DETACH_COMPLETED ); }
    inline void SetMediaDetachCompleted( )      { m_dwState |= STORE_STATE_MEDIA_DETACH_COMPLETED ; }
    inline void ClearMediaDetachCompleted()     { m_dwState &= ~STORE_STATE_MEDIA_DETACH_COMPLETED ; }

    inline BOOL IsAutoMount()                   { return ( m_dwFlags & STORE_ATTRIBUTE_AUTOMOUNT ); }

public:
    // Required LogicalDisk_t virtual functions.
    virtual LRESULT DiskIoControl (DWORD IoControlCode, void* pInBuf,
            DWORD InBufBytes, void* pOutBuf, DWORD nOutBufBytes,
            DWORD* pBytesReturned, OVERLAPPED* pOverlapped);
    // Sets an activity timer event associated with the logical disk, if any.
    virtual void SignalActivity ();
    // Query the name of the disk.
    virtual LRESULT GetName (WCHAR* pDiskName, DWORD NameChars);
    // Return the true device handle for this disk (if there is one).
    virtual HANDLE GetDeviceHandle ();

private:
    void LoadExistingPartitions(BOOL bMount, BOOL bFormat);
    PartitionDisk_t *FindPartition(LPCTSTR szPartitionName);
    BOOL AddPartition(PartitionDisk_t *pPartition);
    void GetPartitionCount();
    void DeletePartDriver();
    DWORD StoreIoControl(PartitionDisk_t *pPartition, DWORD dwIoControlCode, PVOID pInBuf, DWORD nInBufSize, PVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned);
public:
    DWORD Mount(HANDLE hStore, BOOL bMount=TRUE);
    BOOL  Unmount(BOOL bRollback=FALSE);
    DWORD OpenDisk();
    HANDLE CreateHandle(HANDLE hProc, DWORD dwAccess);

    BOOL  UnmountPartitions(BOOL bDelete);
    DWORD MountPartitions(HANDLE hStore, BOOL bMount);


    void MediaDetachFromStore ();
    void MediaAttachToStore ();
    void MediaAttachToPartition (PartitionDisk_t* pPartition);
    void MediaDetachFromPartition (PartitionDisk_t* pPartition);

    void Enable();
    void Disable();
    DWORD Format();

    void PowerOff();
    void PowerOn();

    BOOL SetBlockDevice(const TCHAR *szDriverName);
    BOOL GetStoreInfo(STOREINFO *pInfo);
    BOOL InitStoreSettings();
    DWORD GetStorageId(PVOID pOutBuf, DWORD nOutBufSize, DWORD *pBytesReturned);
    DWORD GetDeviceInfo(STORAGEDEVICEINFO *pDeviceInfo, DWORD *pBytesReturned);
    BOOL GetPartitionDriver(HKEY hKeyStore, HKEY hKeyDevice);

    BOOL Swap (StoreDisk_t* pStoreNew);
    BOOL Compare (StoreDisk_t *pStore);

    DWORD CreatePartition(LPCTSTR szPartitionName, BYTE bPartType, SECTORNUM snNumSectors, BOOL bAuto);
    DWORD DeletePartition(LPCTSTR szPartitionName);
    DWORD OpenPartitionEx(LPCTSTR szPartitionName, HANDLE *pbHandle, HANDLE hProc, DWORD dwAccess);
    BOOL LoadPartition(LPCTSTR szPartitionName, BOOL bMount, BOOL bFormat);

    DWORD DeletePartition(PartitionDisk_t *pPartition);
    DWORD MountPartition(PartitionDisk_t *pPartition);
    DWORD UnmountPartition(PartitionDisk_t *pPartition);
    DWORD RenamePartition(PartitionDisk_t *pPartition, LPCTSTR szNewName);
    DWORD SetPartitionAttributes(PartitionDisk_t *pPartition, DWORD dwAttrs);
    DWORD SetPartitionSize(PartitionDisk_t *pPartition, SECTORNUM snNumSectors);
    DWORD GetPartitionInfo(PartitionDisk_t *pPartition, PPARTINFO info);
    DWORD FormatPartition(PartitionDisk_t *pPartition, BYTE bPartType, BOOL bAuto);

    DWORD FindFirstPartition(PPARTINFO pInfo, HANDLE *pHandle, HANDLE hProc);
    DWORD DeviceIoControl(PartitionDisk_t *pPartition, DWORD dwIoControlCode, PVOID pInBuf, DWORD nInBufSize, PVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned) ;
};


#endif /* __STORAGE__ */

#endif /* UNDER_CE */