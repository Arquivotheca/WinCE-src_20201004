//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <diskio.h>
#include <fatutil.h>

#ifndef __FATHLP_H__
#define __FATHLP_H__

#define FAT_DLL_NAME        _T("FATFSD.DLL")

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

/*
//
// these structures are defined in header files from public\common\oak\drivers\fsd\fatutil
//

//
// Options specified by client of format disk
//
typedef struct _FORMAT_OPTIONS 
{
    DWORD   dwClusSize;        // desired cluster size in bytes (must be power of 2)
    DWORD   dwRootEntries;     // desired number of root directory entries
    DWORD   dwFatVersion;      // FAT Version.  Either 12, 16, or 32	
    DWORD   dwNumFats;         // Number of FATs to create
    BOOL    fFullFormat;       // TRUE if full format requested
    
} FORMAT_OPTIONS, *PFORMAT_OPTIONS;

//
// Options specified by client of defrag disk
//
typedef struct _DEFRAG_OPTIONS 
{
    DWORD   dwFatToUse;         // Which FAT to use for scanning before beginning defrag
    BOOL    fVerifyFix;         // Indicates where to prompt user to verify fix in scanning
    
} DEFRAG_OPTIONS, *PDEFRAG_OPTIONS;

//
// Options specified by client of scan disk
//
typedef struct _SCAN_OPTIONS 
{
    BOOL    fVerifyFix;         // TRUE if prompt user to perform a fix
    DWORD   dwFatToUse;         // Indicates which FAT to use for scanning (one-based indexing)

} SCAN_OPTIONS, *PSCAN_OPTIONS;


// Definition of callback indicating progress of scan disk
typedef VOID (*PFN_PROGRESS)(DWORD dwPercent);

// Definition of callback to display a message or to prompt user
typedef BOOL (*PFN_MESSAGE)(LPTSTR szMessage, LPTSTR szCaption, BOOL fYesNo);

// scan volume export from fatutil.dll
typedef BOOL (*PFN_SCANVOLUME)(HANDLE hFile, PDISK_INFO pdi, PSCAN_OPTIONS pso, 
    PFN_PROGRESS pfnProgress, PFN_MESSAGE pfnMessage);

// defrag volume export from fatutil.dll
typedef BOOL (*PFN_DEFRAGVOLUME)(HANDLE hVolume, PDISK_INFO pdi, PDEFRAG_OPTIONS pdo, 
    PFN_PROGRESS pfnProgress, PFN_MESSAGE pfnMessage);

// format volume export from fatutil.dll
typedef DWORD (*PFN_FORMATVOLUME)(HANDLE hFile, PDISK_INFO pdi, 
    PFORMAT_OPTIONS pfo, PFN_PROGRESS pfnProgress, PFN_MESSAGE pfnMessage);

*/
#endif // __FATUTIL_H__
