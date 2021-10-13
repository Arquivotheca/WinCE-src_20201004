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
//******************************************************************************
//***
//*** FATUTIL.H
//***
//*** These tests may use some undocumented and unsupported constants and
//*** structures. They are for current test purposes only and should not be
//*** used or relied upon.  They may change at any time without notice.
//*** Refer to the offical Microsoft Windows CE OEM Adaptation Kit for
//*** supported capabilities.
//***
//*** THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//*** ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//*** THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//*** PARTICULAR PURPOSE.
//***
//*** Copyright  1998  Microsoft Corporation.  All Rights Reserved.
//***
//******************************************************************************

#ifndef __FATUTIL_H__
#define __FATUTIL_H__


//******************************************************************************
//***** Types and structures
//******************************************************************************

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
// Structure for IOCTL_DISK_FORMAT_VOLUME
//------------------------------------------------------------------------------

#define FMTVOL_QUICK            0x00000001      // perform a "quick" format
#define FMTVOL_BACKUP_FAT       0x00000002      // create a backup FAT on the volume
#define FMTVOL_DISK             0x00000004      // reformat entire disk on which volume resides
#define FMTVOL_CLUSTER_SIZE     0x00000008      // fv_csecClus contains a desired cluster size (in sectors)
#define FMTVOL_VOLUME_SIZE      0x00000010      // fv_csecVol contains a desired volume size (in sectors)
#define FMTVOL_ROOT_ENTRIES     0x00000020      // fv_cRootEntries contains a desired number of root entries
#define FMTVOL_12BIT_FAT        0x00000040      // 12-bit FAT desired
#define FMTVOL_16BIT_FAT        0x00000080      // 16-bit FAT desired
#define FMTVOL_32BIT_FAT        0x00000100      // 32-bit FAT desired

typedef struct _FMTVOLREQ {
    DWORD   fv_flags;           // see FMTVOL_*
    DWORD   fv_csecClus;        // desired cluster size (must be power of 2, and FMTVOL_CLUSTER_SIZE must be set)
    DWORD   fv_csecVol;         // desired volume size (must be <= real volume size, and FMTVOL_VOLUME_SIZE must be set)
    DWORD   fv_cRootEntries;    // desired number of root directory entries (FMTVOL_ROOT_ENTRIES must be set)
} FMTVOLREQ;
typedef FMTVOLREQ *PFMTVOLREQ;

//******************************************************************************
//***** Global Functions
//******************************************************************************

LPTSTR FormatValue(DWORD dwValue, LPTSTR szValue);
int    FatGetVolumeCount();
LPTSTR FatBuildName(int volume, LPTSTR szCard);
BOOL   FatFormat(int volume);
BOOL   FatScan(int volume);
BOOL   FatGetInfo(int volume, FATINFO *pFI);
BOOL   FatDisplayInfo(FATINFO *pFI);

BOOL CardFormat(int volume, DWORD dwFormat, DWORD dwParam , DWORD dwsecClus );
#endif // __FATUTIL_H__
