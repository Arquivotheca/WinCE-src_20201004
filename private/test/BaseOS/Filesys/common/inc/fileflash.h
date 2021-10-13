//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef _FILE_FLASH_H_
#define _FILE_FLASH_H_

#include <windows.h>
#include <diskio.h>
#include <FlashPdd.h>
#include <pnp.h>
#include <devload.h>
#include <msgqueue.h>

// IOCTL to pass in fault behaviour
#define IOCTL_FILE_FLASH_PDD_USER_OFFSET            0x0
#define IOCTL_FILE_FLASH_PDD_SET_FAULT              IOCTL_DISK_USER(IOCTL_FILE_FLASH_PDD_USER_OFFSET)

// Flags to configure the fault injection
#define FILE_FLASH_PDD_FAULT_FAIL_ALL_WRITES        0x0
#define FILE_FLASH_PDD_FAULT_FAIL_SINGLE_WRITE      0x1
// Add more flags for new failure modes

// Structure to hold fault injection parameters
typedef struct _FILE_FLASH_PDD_FAULT_PARAMETERS
{
    DWORD dwCountDownToFailure;
    DWORD dwFlags;
} FILE_FLASH_PDD_FAULT_PARAMETERS, *PFILE_FLASH_PDD_FAULT_PARAMETERS;

// Defaults for FileFlashPDD parameters
#define MAX_REGION_COUNT            4
#define DEFAULT_REGION_COUNT        1
#define DEFAULT_BYTES_PER_SECTOR    512
#define DEFAULT_SECTORS_PER_BLOCK   32
#define DEFAULT_NUM_BLOCKS          1024
#define DEFAULT_SPARE_AREA_SIZE     8
#define MIN_SPARE_SIZE              6
#define DEFAULT_RANDOM_SEED         0

// FileFlashPDD specific values
#define MAX_REGION_COUNT    4
#define BLOCK_DEVICE_REGISTRY_KEY           _T("\\Drivers\\BlockDevice\\FileFlash")
#define PROFILE_REGISTRY_KEY                _T("\\System\\StorageManager\\Profiles\\FileFlash")
#define FATFS_FILESYS_REGISTRY_KEY          _T("\\System\\StorageManager\\Profiles\\FileFlash\\FATFS")
#define EXFAT_FILESYS_REGISTRY_KEY          _T("\\System\\StorageManager\\Profiles\\FileFlash\\EXFAT")
#define DEVICE_HANDLE_VALUE_NAME            _T("HDEVICE")
#define PATH_VALUE_NAME                     _T("Path")
#define USE_EXISTING_FILE_VALUE_NAME        _T("UseExistingFile")
#define REGION_COUNT_VALUE_NAME             _T("RegionCount")
#define RANDOM_SEED_VALUE_NAME              _T("RandomSeed")
#define BYTES_PER_SECTOR_VALUE_NAME         _T("BytesPerSector")
#define SECTORS_PER_BLOCK_VALUE_NAME        _T("SectorsPerBlock")
#define NUM_BLOCKS_VALUE_NAME               _T("NumBlocks")
#define SPARE_AREA_SIZE_VALUE_NAME          _T("SpareBytesPerSector")
#define AUTO_FORMAT_VALUE_NAME              _T("AutoFormat")
#define DISABLE_AUTO_FORMAT_VALUE_NAME		_T("DisableAutoFormat")
#define AUTO_PART_VALUE_NAME                _T("AutoPart")
#define FORMAT_TEXFAT_VALUE_NAME            _T("FormatTfat")

// Fixed values to write to the BlockDriver & StorageManager registry
#define MDD_DLL_VALUE_NAME                  _T("Dll")
#define MDD_DLL_FILE_NAME                   _T("flashmdd.dll")
#define PDD_DLL_VALUE_NAME                  _T("FlashPddDll")
#define PDD_DLL_FILE_NAME                   _T("fileflashpdd.dll")
#define FLASH_PROFILE_VALUE_NAME            _T("Profile")
#define FLASH_PROFILE_NAME                  _T("FileFlash")
#define PREFIX_VALUE_NAME                   _T("Prefix")
#define PREFIX_VALUE                        _T("DSK")
#define ICLASS_VALUE_NAME                   _T("IClass")
#define ICLASS_VALUE                        _T("{A4E7EDDA-E575-4252-9D6B-4195D48BB865}")
#define FOLDER_VALUE_NAME                   _T("Folder")
#define FOLDER_VALUE                        _T("FF Disk")
#define PARTITION_DRIVER_VALUE_NAME         _T("PartitionDriver")
#define PARTITION_DRIVER_VALUE              _T("flashpart.dll")

// Useful Macros
#define Debug NKDbgPrintfW
#ifndef VALID_HANDLE
#define VALID_HANDLE(X) (X != INVALID_HANDLE_VALUE && X != NULL)
#endif
#ifndef INVALID_HANDLE
#define INVALID_HANDLE(X) (X == INVALID_HANDLE_VALUE || X == NULL)
#endif
#ifndef VALID_POINTER
#define VALID_POINTER(X) (X != NULL && 0xcccccccc != (int) X)
#endif
#ifndef CHECK_CLOSE_KEY
#define CHECK_CLOSE_KEY(X) if (VALID_POINTER(X)) { RegCloseKey(X); X = NULL; }
#endif
#ifndef CHECK_LOCAL_FREE
#define CHECK_LOCAL_FREE(X) if (VALID_POINTER(X)) { LocalFree(X); X = NULL; }
#endif
#ifndef CHECK_FREE
#define CHECK_FREE(X) if (VALID_POINTER(X)) { free(X); X = NULL; }
#endif
#ifndef CHECK_DELETE
#define CHECK_DELETE(X) if (VALID_POINTER(X)) { delete (X); X = NULL; }
#endif
#ifndef CHECK_CLOSE_HANDLE
#define CHECK_CLOSE_HANDLE(X) if (VALID_HANDLE(X)) { CloseHandle(X); X = INVALID_HANDLE_VALUE; }
#endif
#ifndef CHECK_FLUSH_BUFFERS
#define CHECK_FLUSH_BUFFERS(X) if (VALID_HANDLE(X)) FlushFileBuffers(X)
#endif
#ifndef CHECK_STOP_DEVICE_NOTIFICATIONS
#define CHECK_STOP_DEVICE_NOTIFICATIONS(X) if (VALID_HANDLE(X)) { StopDeviceNotifications(X); X = INVALID_HANDLE_VALUE; }
#endif
#ifndef LOG_WTT_STATUS
#define LOG_WTT_STATUS(X) ((X)? Debug(_T("WTTMobile Task Return Code = 1")): Debug(_T("WTTMobile Task Return Code = -1")))
#endif

// Macros for Message Queueing
#define MAX_DEVNAME_LEN         100
#define QUEUE_ITEM_SIZE         (sizeof(DEVDETAIL) + MAX_DEVNAME_LEN)

class FileFlashWrapper
{
public:
    FileFlashWrapper(LPCTSTR pszPath);
    BOOL Init(BOOL fPerformActivation = TRUE);
    BOOL DeInit();
    BOOL AddRegion(DWORD dwBytesPerSector, DWORD dwSectorsPerBlock, DWORD dwNumBlocks, DWORD dwSpareAreaSize);
    void SetAutoFormat(BOOL fAutoFormat);
    void SetFormatTexFAT(BOOL fFormatTexFAT);
    void SetUseExistingFile(BOOL fUseExistingFile);
    void SetRandomSeed(DWORD dwRandomSeed);
    void SetWaitForMount(BOOL fWaitForMount);
    static BOOL SetWriteFault(DWORD dwNumWrites, BOOL fFailAllWrites);
    static BOOL ActivateDevice(BOOL fUseExistingFile, BOOL fWaitForMount);
    static BOOL ActivateDevice(BOOL fWaitForMount);
    static BOOL DeactivateDevice();
protected:
    BOOL InitRegistry();
    BOOL InitDevice();
    BOOL WaitForMount();
    static HKEY OpenRegistryKey(LPCTSTR pszKeyName);
    static BOOL ReadRegistryDwordValue(LPCTSTR pszKeyName, LPCTSTR pszValueName, IN OUT LPDWORD pdwValue);
    static BOOL WriteRegistryDwordValue(LPCTSTR pszKeyName, LPCTSTR pszValueName, IN LPDWORD pdwValue);
    static BOOL WriteRegistryStringValue(LPCTSTR pszKeyName, LPCTSTR pszValueName, LPCTSTR pszStringValue);
    static BOOL WriteRegistryMultiStringValue(LPCTSTR pszKeyName, LPCTSTR pszValueName, LPCTSTR pszStringValue);
    static BOOL DeleteRegistryValue(LPCTSTR pszKeyName, LPCTSTR pszValueName);
private:
    TCHAR m_szPath[MAX_PATH];
    FLASH_REGION_INFO m_pRegionInfo[MAX_REGION_COUNT];
    DWORD m_dwRegionCount;
    BOOL m_fAutoFormat;
    BOOL m_fFormatTexFAT;
    BOOL m_fUseExistingFile;
    DWORD m_dwRandomSeed;
    HANDLE m_hDevice;
    BOOL m_fActive;
    BOOL m_fWaitForMount;
};

#endif // _FILE_FLASH_H_