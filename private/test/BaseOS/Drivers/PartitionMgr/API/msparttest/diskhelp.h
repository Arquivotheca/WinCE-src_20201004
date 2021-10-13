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
#include "TuxMain.H"
#include <intsafe.h>
#pragma once

// remove these when we find where they're defined:
#define PART_UNKNOWN            0
#define INVALID                 0xFF    // invalid or not used

#define PART_DOS2_FAT           0x01    // legit DOS partition
#define PART_DOS3_FAT           0x04    // legit DOS partition
#define PART_DOS4_FAT           0x06    // legit DOS partition
#define PART_DOS32              0x0B    // legit DOS partition (FAT32)
#define PART_DOS32X13           0x0C    // Same as 0x0B only "use LBA"
#define PART_DOSX13             0x0E    // Same as 0x06 only "use LBA"
#define PART_DOSX13X            0x0F    // Same as 0x05 only "use LBA"

// used by WriteAndVerifyPartition:
//
// minimum and maximum sector counts used in multi-sector reads
#define MIN_MULTI_SECTOR        2
#define MAX_MULTI_SECTOR        25
// maximum out-of-bounds distance to test past partition
#define MAX_SECTORS_OOB         20

// for switching the block driver used by the test
enum BLOCKDRV 
{
    USE_RAMDISK,
    USE_FILEBLK,
    USE_BUILTIN
};

// struct for write/verify partition thread
typedef struct _WRITE_THREAD_DATA
{
    HANDLE hStore;
    HANDLE storeId;
    LPWSTR szPartName;
    
} WRITE_THREAD_DATA, *PWRITE_THREAD_DATA;

// log functions
void LOG(LPCWSTR szFmt, ...);
void LogStoreInfo(STOREINFO *pInfo);
void LogPartInfo(PARTINFO *pInfo);
void LogDiskInfo(DISK_INFO *pInfo);
UINT DumpDiskInfo(HANDLE hStore);

// disk functions
BOOL IsAValidStore(DWORD dwDisk);
DWORD QueryDiskCount(VOID);
HANDLE OpenStoreByIndex(DWORD dwDisk);
BOOL GetRawDiskInfo(HANDLE hStore, DISK_INFO *diskInfo);
BOOL ReadWriteDiskRaw(HANDLE hStore, DWORD ctlCode, SECTORNUM sector, SECTORNUM cSectors, 
            UINT cBytesPerSector, PBYTE pbBuffer);

// partition driver functios
HANDLE OpenAndFormatStore(DWORD dwDIskNumber, STOREINFO *pInfo = NULL);
BOOL ReadWritePartition(HANDLE hPart, DWORD ctlCode, SECTORNUM sector, SECTORNUM cSectors, 
            UINT cBytesPerSector, PBYTE pbBuffer);
BOOL WritePartitionData(HANDLE hStore, LPCWSTR szPartName, SECTORNUM snSkip = 1);
BOOL VerifyPartitionData(HANDLE hStore, LPCWSTR szPartName, SECTORNUM snSkip = 1);
BOOL WriteAndVerifyPartition(HANDLE hStore, LPCWSTR szPartName, SECTORNUM snSkip = 1);
BOOL CreateAndVerifyPartition(HANDLE hStore, LPCWSTR szPartName, BYTE bPartType, 
            SECTORNUM numSectors, BOOL fAuto, PARTINFO *pInfo = NULL);




