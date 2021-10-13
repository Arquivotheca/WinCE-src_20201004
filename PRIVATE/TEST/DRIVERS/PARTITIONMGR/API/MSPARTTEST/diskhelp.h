//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "TuxMain.H"

#ifndef __DISKHELP_H__
#define __DISKHELP_H__

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
    HANDLE hDisk;
    STOREID storeId;
    LPWSTR szPartName;
    
} WRITE_THREAD_DATA, *PWRITE_THREAD_DATA;

// log functions
void LOG(LPWSTR szFmt, ...);
void LogStoreInfo(PD_STOREINFO *pInfo);
void LogPartInfo(PD_PARTINFO *pInfo);
void LogDiskInfo(DISK_INFO *pInfo);
UINT DumpDiskInfo(HANDLE hDisk);

// disk functions
BOOL IsAValidDisk(DWORD dwDisk);
DWORD QueryDiskCount(VOID);
HANDLE OpenDisk(DWORD dwDisk);
BOOL GetRawDiskInfo(HANDLE hDisk, DISK_INFO *diskInfo);
BOOL ReadWriteDiskRaw(HANDLE hDisk, DWORD ctlCode, SECTORNUM sector, SECTORNUM cSectors, 
            UINT cBytesPerSector, PBYTE pbBuffer);

// partition driver functios
STOREID OpenAndFormatStore(HANDLE hDisk, PD_STOREINFO *pInfo = NULL);
BOOL ReadWritePartition(PARTID partId, DWORD ctlCode, SECTORNUM sector, SECTORNUM cSectors, 
            UINT cBytesPerSector, PBYTE pbBuffer);
BOOL WritePartitionData(STOREID storeId, LPCWSTR szPartName, SECTORNUM snSkip = 1);
BOOL VerifyPartitionData(STOREID storeId, LPCWSTR szPartName, SECTORNUM snSkip = 1);
BOOL WriteAndVerifyPartition(HANDLE hDisk, STOREID storeId, LPCWSTR szPartName, SECTORNUM snSkip = 1);
BOOL CreateAndVerifyPartition(STOREID storeId, LPCWSTR szPartName, BYTE bPartType, 
            SECTORNUM numSectors, BOOL fAuto, PD_PARTINFO *pInfo = NULL);

#endif // __DISKHELP_H__
