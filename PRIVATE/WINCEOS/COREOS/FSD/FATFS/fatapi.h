//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    fatapi.h

Abstract:

    This file contains FAT-specific API definitions (ie, IOCTLs) for
    the WINCE FAT file system implementation.

Revision History:

--*/

#ifndef FATAPI_H
#define FATAPI_H


/*  Volume-level I/O request structures
 */

#define IOCTL_DISK_GET_DEVICE_PARAMETERS        CTL_CODE(IOCTL_DISK_BASE, 0x0080, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_GET_VOLUME_PARAMETERS        CTL_CODE(IOCTL_DISK_BASE, 0x0081, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_GET_FREE_SPACE               CTL_CODE(IOCTL_DISK_BASE, 0x0082, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_LOCK_VOLUME                  CTL_CODE(IOCTL_DISK_BASE, 0x0083, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_UNLOCK_VOLUME                CTL_CODE(IOCTL_DISK_BASE, 0x0084, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_READ_SECTORS                 CTL_CODE(IOCTL_DISK_BASE, 0x0085, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_WRITE_SECTORS                CTL_CODE(IOCTL_DISK_BASE, 0x0086, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_RESERVED                     /* was IOCTL_DISK_GET_OEM_UNICODE_TABLES in v1, no longer supported */

// Moved these IOCTLS to public\common\oak\inc\diskio.h to allow public access
//#define IOCTL_DISK_FORMAT_VOLUME                CTL_CODE(IOCTL_DISK_BASE, 0x0088, METHOD_BUFFERED, FILE_ANY_ACCESS)
//#define IOCTL_DISK_SCAN_VOLUME                  CTL_CODE(IOCTL_DISK_BASE, 0x0089, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DISK_SET_DEBUG_ZONES              CTL_CODE(IOCTL_DISK_BASE, 0x008A, METHOD_BUFFERED, FILE_ANY_ACCESS)


//  Structure for IOCTL_DISK_GET_FREE_SPACE request

typedef struct _FREEREQ {
    DWORD   fr_SectorsPerCluster;
    DWORD   fr_BytesPerSector;
    DWORD   fr_FreeClusters;
    DWORD   fr_Clusters;
} FREEREQ;
typedef FREEREQ *PFREEREQ;


//  Structure for IOCTL_DISK_READ_SECTORS and IOCTL_DISK_WRITE_SECTORS I/O request

#define SECIO_DISK              0x00000001      // request access to entire disk

typedef struct _SECIOREQ {
    DWORD   sr_flags;           // see SECIO_*
    DWORD   sr_sec;             // starting sector number
    DWORD   sr_csecs;           // total sectors
} SECIOREQ;
typedef SECIOREQ *PSECIOREQ;


//  Structure for IOCTL_DISK_FORMAT_VOLUME

#define FMTVOL_QUICK            0x00000001      // perform a "quick" format (eg, no surface scan)
#define FMTVOL_BACKUP_FAT       0x00000002      // create a backup FAT on the volume
#define FMTVOL_DISK             0x00000004      // reformat entire disk on which volume resides
#define FMTVOL_CLUSTER_SIZE     0x00000008      // fv_csecClus contains a desired cluster size (in sectors)
#define FMTVOL_VOLUME_SIZE      0x00000010      // fv_csecVol contains a desired volume size (in sectors)
#define FMTVOL_ROOT_ENTRIES     0x00000020      // fv_cRootEntries contains a desired number of root entries
#define FMTVOL_12BIT_FAT        0x00000040      // 12-bit FAT desired
#define FMTVOL_16BIT_FAT        0x00000080      // 16-bit FAT desired
#define FMTVOL_32BIT_FAT        0x00000100      // 32-bit FAT desired
#define FMTVOL_DISK_LOWLEVEL    0x00000200      // issue low-level format request to disk driver first

typedef struct _FMTVOLREQ {
    DWORD   fv_flags;           // see FMTVOL_*
    DWORD   fv_csecClus;        // desired cluster size (must be power of 2, and FMTVOL_CLUSTER_SIZE must be set)
    DWORD   fv_csecVol;         // desired volume size (must be <= real volume size, and FMTVOL_VOLUME_SIZE must be set)
    DWORD   fv_cRootEntries;    // desired number of root directory entries (FMTVOL_ROOT_ENTRIES must be set)
} FMTVOLREQ;
typedef FMTVOLREQ *PFMTVOLREQ;


//  Structure for IOCTL_DISK_SCAN_VOLUME

#define SCANVOL_QUICK           0x00000001      // perform a "quick" scan (eg, no surface analysis)
#define SCANVOL_UNATTENDED      0x00000002      // do not prompt the user
#define SCANVOL_REPAIRALL       0x00000004      // if unattended mode, repair all errors

#define SCANERR_DIR_BADDATA     0x00000001
#define SCANERR_DIR_BADCLUS     0x00000002
#define SCANERR_DIR_BADSIZE     0x00000004
#define SCANERR_FAT_BADINDEX    0x00000008
#define SCANERR_FAT_CROSSLINK   0x00000010
#define SCANERR_FAT_WASTED      0x00000020
#define SCANERR_FAT_TRUNCATED   0x00000040

#define SCAN_CANCELLED          0xFFFFFFFF

typedef struct _SCANVOLREQ {
    DWORD   sv_dwScanVol;       // see SCANVOL_*
} SCANVOLREQ;
typedef SCANVOLREQ *PSCANVOLREQ;

typedef struct _SCANRESULTS {
    DWORD   sr_dwScanErr;       // see SCANERR_*
} SCANRESULTS;
typedef SCANRESULTS *PSCANRESULTS;



#endif /* FATAPI_H */
