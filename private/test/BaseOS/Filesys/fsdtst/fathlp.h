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
#include <windows.h>
#include <diskio.h>
#include <fatutil.h>

#ifndef __FATHLP_H__
#define __FATHLP_H__

#define FAT_OLD_DLL_NAME    _T("FATFSD.DLL")
#define FAT_DLL_NAME        _T("EXFAT.DLL")
// 
// FAT Specific IOCTL codes not defined. These codes are not 
// 
#ifndef IOCTL_DISK_BASE
#define IOCTL_DISK_BASE   FILE_DEVICE_DISK
#endif
#define IOCTL_DISK_GET_FREE_SPACE        CTL_CODE(IOCTL_DISK_BASE, 0x0082, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_GET_VOLUME_PARAMETERS CTL_CODE(IOCTL_DISK_BASE, 0x0081, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _FATINFO {
    DWORD dwMediaDescriptor;
    DWORD dwBytesPerSector;
    DWORD dwSectorsPerCluster;
    DWORD dwTotalSectors;
    DWORD dwReservedSecters;  
    DWORD dwHiddenSectors;
    DWORD dwSectorsPerTrack;
    DWORD dwHeads;
    DWORD dwSectorsPerFAT;
    DWORD dwNumberOfFATs;
    DWORD dwRootEntries;
    DWORD dwUsableClusters;
    DWORD dwFreeClusters;
    DWORD dwFATVersion;
    BOOL  f64BitSupport;
    BOOL  fTranscationSupport;
} FATINFO, *PFATINFO, *LPFATINFO;

//------------------------------------------------------------------------------
// Structure for IOCTL_DISK_GET_VOLUME_PARAMETERS
//------------------------------------------------------------------------------

#pragma pack(1)

typedef struct  BPB { /* */
        WORD    BPB_BytesPerSector;     /* Sector size                      */
        BYTE    BPB_SectorsPerCluster;  /* sectors per allocation unit      */
        WORD    BPB_ReservedSectors;    /* DOS reserved sectors             */
        BYTE    BPB_NumberOfFATs;       /* Number of FATS                   */
        WORD    BPB_RootEntries;        /* Number of directories            */
        WORD    BPB_TotalSectors;       /* Partition size in sectors        */
        BYTE    BPB_MediaDescriptor;    /* Media descriptor                 */
        WORD    BPB_SectorsPerFAT;      /* Fat sectors                      */
        WORD    BPB_SectorsPerTrack;    /* Sectors Per Track                */
        WORD    BPB_Heads;              /* Number heads                     */
        DWORD   BPB_HiddenSectors;      /* Hidden sectors                   */
        DWORD   BPB_BigTotalSectors;    /* Number sectors                   */
} BPB;

typedef struct  BIGFATBPB { /* */
        BPB     oldBPB;
        DWORD   BGBPB_BigSectorsPerFAT;   /* BigFAT Fat sectors              */
        WORD    BGBPB_ExtFlags;           /* Other flags                     */
        WORD    BGBPB_FS_Version;         /* File system version             */
        DWORD   BGBPB_RootDirStrtClus;    /* Starting cluster of root directory */
        WORD    BGBPB_FSInfoSec;          /* Sector number in the reserved   */
                                          /* area where the BIGFATBOOTFSINFO */
                                          /* structure is. If this is >=     */
                                          /* oldBPB.BPB_ReservedSectors or   */
                                          /* == 0 there is no FSInfoSec      */
        WORD    BGBPB_BkUpBootSec;        /* Sector number in the reserved   */
                                          /* area where there is a backup    */
                                          /* copy of all of the boot sectors */
                                          /* If this is >=                   */
                                          /* oldBPB.BPB_ReservedSectors or   */
                                          /* == 0 there is no backup copy.   */
        WORD    BGBPB_Reserved[6];        /* Reserved for future expansion   */
} BIGFATBPB;

#pragma pack()

#define MAX_SEC_PER_TRACK   64

typedef struct tagEDEVPB {
    BYTE    SplFunctions;
    BYTE    devType;
    UINT    devAtt;
    UINT    NumCyls;
    BYTE    bMediaType;         /* 0=>1.2MB and 1=>360KB */
    BIGFATBPB BPB;
    BYTE    reserved1[32];
    BYTE    TrackLayout[MAX_SEC_PER_TRACK * 4 + 2];
} EDEVPB;

typedef struct tagDEVPB {
    union   {
    EDEVPB  ExtDevPB;
    } DevPrm;
    BYTE    CallForm;
} DEVPB;

//------------------------------------------------------------------------------
// Structure for IOCTL_DISK_GET_FREE_SPACE request.
//------------------------------------------------------------------------------

typedef struct _FREEREQ {
    DWORD   fr_SectorsPerCluster;
    DWORD   fr_BytesPerSector;
    DWORD   fr_FreeClusters;
    DWORD   fr_Clusters;
} FREEREQ;


// exported function names for GetProcAddress()
#define SZ_FN_SCANVOLUME        _T("ScanVolume")
#define SZ_FN_DEGRAGVOLUME      _T("DefragVolume")
#define SZ_FN_FORMATVOLUME      _T("FormatVolume")

#endif // __FATUTIL_H__
