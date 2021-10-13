//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include "fathlp.h"
#include "diskio.h"
#include "tuxmain.h"
#include "legacy.h"

#ifndef __MFS_H__
#define __MFS_H__

#define _TEST_FILE_PATH_TEXT                TEXT("\\MULTIVOL")
#define _COPY_FILE_PATH_TEXT                TEXT("\\COPYTO")
#define _MOVE_FILE_PATH_TEXT                TEXT("\\MOVETO")

#define _DEF_CLUSTER_SIZE                   4096
#define _DEF_ROOT_DIR_ENTIRES               256

#define WRITE_BUFFER_SIZE                   1024

// some partition definitions
#define PART_DOS2_FAT           0x01    // legit DOS partition
#define PART_DOS3_FAT           0x04    // legit DOS partition
#define PART_DOS4_FAT           0x06    // legit DOS partition
#define PART_DOS32              0x0B    // legit DOS partition (FAT32)
#define PART_DOS32X13           0x0C    // Same as 0x0B only "use LBA"
#define PART_DOSX13             0x0E    // Same as 0x06 only "use LBA"
#define PART_DOSX13X            0x0F    // Same as 0x05 only "use LBA"

#define LOGFLAG(flags, match) if(flags & match) LOG(L#match)
#define LOGPARTTYPE(bPartType, type) if(bPartType == type) LOG(L#type)

//
// CMtdPart - wraps a mounted partition and provides functionality for 
// testing it.
//
class CMtdPart
{

public:

    CMtdPart(PSTOREINFO pStoreInfo, PPARTINFO pPartInfo, UINT nRootDirs, 
        UINT nClusterSize);
    virtual ~CMtdPart();

    /*
     * Member Access functions
     *
     */
     
    // accessor functions for PARTINFO struct members
    SECTORNUM GetNumSectors(VOID);
    LPTSTR GetPartitionName(LPTSTR szPartitionName);
    LPTSTR GetVolumeName(LPTSTR szVolumeName);
    LPTSTR GetFileSysName(LPTSTR szFileSysName);
    DWORD GetPartitionAttributes(VOID);
    BYTE GetPartitionType(VOID);

    // accessor functions for STOREINFO struct members
    LPTSTR GetDeviceName(LPTSTR szDeviceName);
    LPTSTR GetStoreName(LPTSTR szStoreName);
    LPTSTR GetProfileName(LPTSTR szProfileName);

    // accessor functions for DISK_INFO struct members
    DWORD GetBytesPerSector(VOID);

    DWORD GetCallerDiskFreeSpace(VOID);

    // return the unique ID for this partition (generated at
    // enumeration time)
    DWORD GetUniqueId(VOID);

    // some helper functions to speed up the multi-vol tests
    TCHAR *GetTestFilePath(LPTSTR pszPathBuf);
    TCHAR *GetCopyFilePath(LPTSTR pszPathBuf);
    TCHAR *GetMoveFilePath(LPTSTR pszPathBuf);
    TCHAR *GetOsTestFilePath(LPTSTR pszPathBuf);
    TCHAR *GetOsCopyFilePath(LPTSTR pszPathBuf);
    TCHAR *GetOsMoveFilePath(LPTSTR pszPathBuf);

    // blocks until the mount-point directory to appears in the file-system
    BOOL WaitForMount(DWORD cTimeout);

    // util functions for eating and freeing disk space
    BOOL UnFillDisk(VOID);
    BOOL FillDiskMinusXBytes(DWORD freeSpace, BOOL fQuick);

    // some file ops, there are more places in the tests that
    // should use these...
    BOOL CreateTestFile(LPCTSTR szName, DWORD fileSize);
    BOOL DeleteTestFile(LPCTSTR szName);
    BOOL CreateTestDirectory(LPCTSTR szName);
    BOOL RemoveTestDirectory(LPCTSTR szName);

    // return the count of the number of directory entries in the root
    DWORD GetNumberOfRootDirEntries(VOID);

    // FSD Specific Functions
    virtual BOOL FormatVolume(VOID);
    virtual DWORD GetClusterSize(VOID);
    virtual DWORD GetMaxRootDirEntries(VOID);
    
    static CMtdPart *Allocate(PPARTINFO pPartInfo, PSTOREINFO pStoreInfo);
    static void LogWin32StorageDeviceInfo(PSTORAGEDEVICEINFO pInfo);
    static void LogWin32StoreInfo(PSTOREINFO pInfo);
    static void LogWin32PartInfo(PPARTINFO pInfo);

protected:
    PARTINFO m_partInfo;
    STOREINFO m_storeInfo;
    DISK_INFO m_diskInfo;
    HANDLE m_hPart;
    HANDLE m_hStore;
    UINT m_nRootDirs;
    UINT m_nClusterSize;
    DWORD m_uniqueId;

private:

    BOOL FillDiskMinusXBytesWithSetEndOfFile(DWORD freeSpace);
    BOOL FillDiskMinusXBytesWithWriteFile(DWORD freeSpace);
    
    static DWORD uniqueId;

};

//
//provides FAT specific info
//
class CFATMtdPart : public CMtdPart 
{
public:
    
    CFATMtdPart(PSTOREINFO pStoreInfo, PPARTINFO pPartInfo);
    virtual ~CFATMtdPart();

    virtual BOOL FormatVolume();
    virtual DWORD GetClusterSize();
    virtual DWORD GetMaxRootDirEntries(VOID);

    static VOID DisplayFATInfo(PFATINFO pFatInfo);

private:
    BOOL GetFATInfo(PFATINFO pFatInfo);
    FATINFO m_fatInfo;
};


#endif // __MSF_H__
