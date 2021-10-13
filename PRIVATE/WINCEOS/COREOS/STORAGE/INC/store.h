//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __STORAGE__
#define __STORAGE__

#include <windows.h>
#include <storemgr.h>
#include <partition.h>
#include <blockdev.h>

#define STOREHANDLE_TYPE_SEARCH     (1 << 0)
#define STOREHANDLE_TYPE_CURRENT    (1 << 1)
#define STOREHANDLE_TYPE_INTERNAL   (1 << 2)

#define STORE_FLAG_MOUNTED          (1 << 31)
#define STORE_FLAG_MEDIACHANGED     (1 << 30)
#define STORE_FLAG_DETACHED         (1 << 29)
#define STORE_FLAG_REFRESHED        (1 << 28)
#define STORE_FLAG_ACTIVITYTIMER    (1 << 27)

#define STORE_HANDLE_SIG 0x00596164
#define PART_HANDLE_SIG  0x64615900

class  CStoreHandle
{
public:
    DWORD dwSig;
    class CStore *pStore;
    class CPartition *pPartition;
    HANDLE hProc;
    DWORD dwFlags;
    CStoreHandle *pNext;
public:
    CStoreHandle() :
    dwSig(STORE_HANDLE_SIG),
    pStore(NULL),
    pPartition(NULL),
    hProc(NULL),
    dwFlags(0),
    pNext(NULL)
    {
    }
};     

typedef CStoreHandle STOREHANDLE, *PSTOREHANDLE, SEARCHSTORE, *PSEARCHSTORE, SEARCHPARTITION, *PSEARCHPARTITION;

class CStore
{
public:
    HANDLE m_hDisk;
    TCHAR  m_szDeviceName[DEVICENAMESIZE];
    TCHAR  m_szOldDeviceName[DEVICENAMESIZE];
    TCHAR  m_szStoreName[STORENAMESIZE];
    TCHAR  m_szFolderName[FOLDERNAMESIZE];
    TCHAR  m_szPartDriver[MAX_PATH];
    TCHAR  m_szRootRegKey[MAX_PATH];
    TCHAR  m_szDefaultFileSystem[MAX_PATH];
    GUID   m_DeviceGuid;
    DWORD  m_dwPartCount;
    DWORD  m_dwMountCount;
    DWORD  m_dwFlags;
    TCHAR  m_szActivityName[MAX_PATH];
    HANDLE m_hActivityEvent;
    DWORD  m_dwMountFlags;
    DWORD  m_dwRefCount;
    PBYTE  m_pStorageId;
public:
    CPartDriver *m_pPartDriver;
    CPartition *m_pPartitionList;
    PD_STOREINFO m_si;
    CRITICAL_SECTION m_cs;
    STORAGEDEVICEINFO m_sdi;
    DISK_INFO m_di;
    CBlockDevice *m_pBlockDevice;
    DWORD m_dwStoreId;
public:    
    CStore *m_pNextStore;
    PSTOREHANDLE m_pRootHandle;
public:
   CStore(TCHAR *szDeviceName, GUID DeviceGuid);
   CStore(TCHAR *szDriverName);
   virtual ~CStore();
public:
    inline void Lock()
    {
        EnterCriticalSection( &m_cs);
    }
    inline void Unlock()
    {
        LeaveCriticalSection( &m_cs);
    }    
    inline void SetBlockDevice(TCHAR *szDriverName) 
    {
        m_pBlockDevice = new CBlockDevice(szDriverName);
    }
        

public:   
    BOOL GetPartitionDriver(HKEY hKeyStore, HKEY hKeyDevice);
    BOOL GetStoreSettings();
    DWORD MountStore(BOOL bMount=TRUE);
    BOOL UnmountStore(BOOL bDelete = TRUE);
    DWORD OpenDisk();
    void GetPartitionCount();
    void LoadExistingPartitions(BOOL bMount);
    void AddPartition(CPartition *pPartition);
    void DeletePartition(CPartition *pPartition);
    CPartition *FindPartition(LPCTSTR szPartitionName);
    BOOL LoadPartition(LPCTSTR szPartitionName, BOOL bMount);
    DWORD FormatStore();
    DWORD CreatePartition(LPCTSTR szPartitionName, BYTE bPartType, SECTORNUM snNumSectors, BOOL bAuto);
    DWORD DeletePartition(LPCTSTR szPartitionName);
    DWORD OpenPartition(LPCTSTR szPartitionName, HANDLE *pbHandle, HANDLE hProc);
    DWORD MountPartition(CPartition *pPartition);
    BOOL IsValidPartition(CPartition *pPartition);
    DWORD UnmountPartition(CPartition *pPartition);
    DWORD RenameParttion(CPartition *pPartition, LPCTSTR szNewName);
    DWORD SetPartitionAttributes(CPartition *pPartition, DWORD dwAttrs);
    BOOL GetStoreInfo(STOREINFO *pInfo);
    DWORD GetPartitionInfo(CPartition *pPartition, PPARTINFO info);
    DWORD FormatPartition(CPartition *pPartition, BYTE bPartType, BOOL bAuto);
    DWORD FindFirstPartition(PPARTINFO pInfo, HANDLE *pHandle, HANDLE hProc);
    DWORD FindNextPartition(PSEARCHPARTITION pSearch, PPARTINFO pInfo);
    DWORD FindClosePartition(PSEARCHPARTITION pSearch);
    DWORD DeviceIoControl(CPartition *pPartition, DWORD dwIoControlCode, PVOID pInBuf, DWORD nInBufSize, PVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned);
};


#endif /* __STORAGEMGR__ */

