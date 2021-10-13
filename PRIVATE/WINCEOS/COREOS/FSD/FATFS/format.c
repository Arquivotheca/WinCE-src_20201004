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

    format.c

Abstract:

    This file contains routines for formatting volumes.

Revision History:

--*/

#include "fatfs.h"


/*  FormatVolume - Format a volume
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pfv - pointer to FMTVOLREQ structure (NULL if none)
 *
 *  EXIT
 *      ERROR_SUCCESS if successful, error code if not
 *
 *  NOTES
 *      Note that some of the values calculated by this function are arbitrary
 *      (eg, 256 root directory entries for SRAM cards vs. 512 root entries for
 *      ATA cards; no backup FATs unless specifically requested by the caller; etc).
 *      Which means the resulting volume may not exactly match what an MS-DOS or
 *      Win95 FORMAT utility would have created, but the volume should still be a
 *      legitimate FAT volume, and so hopefully all MS-DOS-based or Win95-based systems
 *      will have no problem mounting/reading it.
 */
#ifndef USE_FATUTIL
DWORD FormatVolume(PVOLUME pvol, PFMTVOLREQ pfv)
{
    DWORD dwFlags;
    int iFAT, cLoops;
    BYTE cFATs, bMediaDesc;
    DWORD csecReserved, cRootEntries;
    DWORD sec, csecHidden, csecClus;
    DWORD csecFATs, csecFATsLast;
    DWORD csecRoot, csecData, cclusData;
    DEVPB devpb, volpb;
    DWORD cbitsClus, secVolBias;
    DWORD dwError = ERROR_SUCCESS;
    BYTE abSector[DEFAULT_SECTOR_SIZE];
    TCHAR szVolumeName[MAX_PATH];

    dwFlags = FMTVOL_QUICK;
    if (pfv)
    dwFlags = pfv->fv_flags;

    // All we support right now is "quick format"

#ifdef TFAT
    if (!(dwFlags & FMTVOL_QUICK) || (pvol->v_flFATFS & FATFS_DISABLE_FORMAT))
#else
    if (!(dwFlags & FMTVOL_QUICK))
#endif
        return ERROR_INVALID_FUNCTION;

       
    // Now we can proceed with the "logical" formatting process...

    QueryVolumeParameters(pvol, &volpb, TRUE);
    QueryVolumeParameters(pvol, &devpb, FALSE);

    if (devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector != sizeof(abSector)) {
    DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!FormatVolume: unsupported sector size (%d)\n"), devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector));
    return ERROR_GEN_FAILURE;
    }

    // Determine if we need to format the entire device or if the volume is already
    // sufficiently well-defined.  Don't bother with a backup FAT unless caller requests it.
    cFATs = 1;
    if (dwFlags & FMTVOL_BACKUP_FAT)
        cFATs++;    
#ifdef TFAT
    //  for TFAT, always two FAT tables
    cFATs = 2;
#endif


    cRootEntries = 256;
    if (dwFlags & FMTVOL_ROOT_ENTRIES)
    cRootEntries = pfv->fv_cRootEntries;

    csecReserved = 1;
    secVolBias = 0;
    csecHidden = devpb.DevPrm.OldDevPB.BPB.BPB_HiddenSectors = 0;

    // If there's partition information for this volume, use it;  otherwise,
    // rely on any existing volume information (for the case where we're being reformatted)

    if (pvol->v_ppi) {
    secVolBias = pvol->v_ppi->pi_secAbsolute;
    devpb.DevPrm.OldDevPB.BPB.BPB_HiddenSectors = pvol->v_ppi->pi_secAbsolute;
    if (pvol->v_ppi->pi_ppiParent)
        devpb.DevPrm.OldDevPB.BPB.BPB_HiddenSectors -= pvol->v_ppi->pi_secPartTable;
    devpb.DevPrm.OldDevPB.BPB.BPB_BigTotalSectors = pvol->v_ppi->pi_PartEntry.Part_TotalSectors;
    }
    else if (volpb.DevPrm.OldDevPB.BPB.BPB_BigTotalSectors != 0 && !(dwFlags & FMTVOL_DISK)) {
    secVolBias = pvol->v_secVolBias;
    devpb.DevPrm.OldDevPB.BPB.BPB_HiddenSectors = volpb.DevPrm.OldDevPB.BPB.BPB_HiddenSectors;
    devpb.DevPrm.OldDevPB.BPB.BPB_BigTotalSectors = volpb.DevPrm.OldDevPB.BPB.BPB_BigTotalSectors;
    }

    // Compute new volume characteristics now

    if (dwFlags & FMTVOL_VOLUME_SIZE) {
    // But you can't make the volume *bigger* than it really is...
    if (pfv->fv_csecVol > devpb.DevPrm.OldDevPB.BPB.BPB_BigTotalSectors)
        return ERROR_INVALID_PARAMETER;
    devpb.DevPrm.OldDevPB.BPB.BPB_BigTotalSectors = pfv->fv_csecVol;
    }

    // Determine if this is disk should be partitioned (and have hidden sectors)

    if (devpb.DevPrm.OldDevPB.bMediaType == MEDIA_HD) {

    // Partitioned disks typically reserve the entire first track,
    // although only the first sector is used.  Carry the tradition forward...

    if (!(dwFlags & FMTVOL_ROOT_ENTRIES))
        cRootEntries = 512;

    if (secVolBias == 0)
        csecHidden = devpb.DevPrm.OldDevPB.BPB.BPB_HiddenSectors = devpb.DevPrm.OldDevPB.BPB.BPB_SectorsPerTrack;
    }

    if (csecHidden || devpb.DevPrm.OldDevPB.BPB.BPB_HiddenSectors)
    bMediaDesc = MEDIA_HD;
    else
    bMediaDesc = MEDIA_1440;

    





    csecClus = 1;

    if (dwFlags & FMTVOL_CLUSTER_SIZE) {
    csecClus = pfv->fv_csecClus;
    // Make sure the cluster size is valid, fits in a byte, and is a power of two...
    if (csecClus == 0 || csecClus > 255 || (csecClus & (csecClus-1)))
        return ERROR_INVALID_PARAMETER;
    }

    // Compute how many clusters will fit on the media, by first estimating
    // how many sectors will be in the data area, then recomputing the number
    // of clusters until we get a consistent answer.

restart:

    do {
    cLoops = 4;
    csecFATs = csecRoot = 0;
    do {

        csecData = devpb.DevPrm.OldDevPB.BPB.BPB_BigTotalSectors -
            csecHidden - csecReserved - csecFATs - csecRoot;
        do {
        cclusData = csecData / csecClus;

        if (csecClus > (DWORD)(MAX_CLUS_SIZE/devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector)) {

            DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!FormatVolume: media too large (%d clusters at %d secs/clus)\n"), dwError, cclusData, csecClus));

            // If a volume size has been specified, then we have to bail

            if (dwFlags & FMTVOL_VOLUME_SIZE)
            return ERROR_INVALID_PARAMETER;

            



            dwFlags |= FMTVOL_VOLUME_SIZE;
#ifdef FAT32
            devpb.DevPrm.OldDevPB.BPB.BPB_BigTotalSectors = (FAT32_BAD-1)*(MAX_CLUS_SIZE/devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector);
#else
            devpb.DevPrm.OldDevPB.BPB.BPB_BigTotalSectors = (FAT16_BAD-1)*(MAX_CLUS_SIZE/devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector);
#endif
            goto restart;
        }

        if (!(dwFlags & (FMTVOL_16BIT_FAT | FMTVOL_32BIT_FAT))) {
            if (cclusData < FAT1216_THRESHOLD) {
            cbitsClus = 12; // Should use a 12-bit FAT
            break;
            }
        }

        if (!(dwFlags & (FMTVOL_12BIT_FAT | FMTVOL_32BIT_FAT))) {
            if (cclusData < FAT1632_THRESHOLD) {
            cbitsClus = 16; // Should use a 16-bit FAT
            break;
            }
        }

#ifndef FAT32
        csecClus *= 2;  // if no FAT32 support, no choice but to grow cluster size
#else
        // So, at the current cluster size, a 32-bit FAT is the only answer;
        // however, we won't settle on that as long as the cluster size is still small
        // (less than 4K) AND the cluster was not explicitly selected by the caller.

        if (!(dwFlags & (FMTVOL_CLUSTER_SIZE | FMTVOL_32BIT_FAT)) &&
            csecClus < (DWORD)(4096/devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector)) {
            

            csecClus *= 2;      // grow the cluster size
        }
        else {
            // Bite the bullet and go with a 32-bit FAT;  note that FAT32 also requires
            // more than the usual number of reserved sectors, which we need to "reserve" now.
            cbitsClus = 32;
            csecReserved = 2;   // why only 2? see "minimalist" comments below...
            break;
        }
#endif

        } while (TRUE);

        // NOTE: when computing csecFATs, we must add DATA_CLUSTER (2) to cclusData, because
        // the first two entries in a FAT aren't real entries, but we must still account for them.

        csecFATsLast = csecFATs;
        csecFATs = (((cclusData+DATA_CLUSTER) * cbitsClus + 7)/8 + devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector-1) / devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector * cFATs;
#ifdef FAT32
        if (cbitsClus == 32)        // csecRoot comes into play only on non-FAT32 volumes
        csecRoot = 0;
        else
#endif
        csecRoot = (sizeof(DIRENTRY) * cRootEntries + devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector-1) / devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector;

    } while (csecFATs != csecFATsLast && --cLoops);

    if (cLoops)
        break;

    // We couldn't get a consistent answer, so chop off one cluster

    devpb.DevPrm.OldDevPB.BPB.BPB_BigTotalSectors -= csecClus;

    } while (TRUE);

    DEBUGMSG (ZONE_INIT,(DBGTEXT ("FATFS!FormatVolume:\n\n")));
    DEBUGMSGW(ZONE_INIT,(DBGTEXTW("Disk type:             %s\n"), csecHidden || devpb.DevPrm.OldDevPB.BPB.BPB_HiddenSectors? TEXTW("fixed") : TEXTW("removable")));
    DEBUGMSG (ZONE_INIT,(DBGTEXT ("Sectors/FAT:           %d (%d FATs)\n"), csecFATs/cFATs, cFATs));
    DEBUGMSG (ZONE_INIT,(DBGTEXT ("New cluster size:      %d (%d-bit FAT)\n"), csecClus * devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector, cbitsClus));
    DEBUGMSG (ZONE_INIT,(DBGTEXT ("New total clusters:    %d\n"), cclusData));
    DEBUGMSG (ZONE_INIT,(DBGTEXT ("Total available space: %d bytes\n\n"), cclusData * csecClus * devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector));

    // Now try to lock the volume in preparation for writing

    dwError = LockVolume(pvol, VOLF_LOCKED);
    if (dwError) {
    DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!FormatVolume: cannot lock volume (%d)\n"), dwError));
    goto abort;
    }

    // We will invalidate all buffers now, whether dirty or not, since we're about to rewrite
    // the disk structure.  Note that LockVolume is already supposed to insure that any threads
    // that might have missed the locking transition have left FATFS, so there shouldn't be any
    // risk of this invalidation being undone by another thread still modifying the disk state.

    InvalidateBufferSet(pvol, TRUE);

    pvol->v_flags |= VOLF_FORMATTING;

    // Now write a new partition table if this is a partitioned disk and we're
    // reformatting the whole disk...

    if (csecHidden) {

    PPARTENTRY ppe;
    DWORD maxCyl =
        devpb.DevPrm.OldDevPB.BPB.BPB_BigTotalSectors /
       (devpb.DevPrm.OldDevPB.BPB.BPB_Heads * devpb.DevPrm.OldDevPB.BPB.BPB_SectorsPerTrack) - 1;

    // The bulk of the partition sector needs to be zeroed.

    memset(abSector, 0, devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector);
    abSector[0] = BS3BYTJMP;
    abSector[1] = 0xfd;
    abSector[2] = 0xff;

    // Point to first partition table slot (the only one we're going to fill)

    ppe = (PPARTENTRY)(abSector+PARTTABLE_OFFSET);

    ppe->Part_BootInd = PART_BOOTABLE;
    ppe->Part_FirstHead = 1;
    ppe->Part_FirstSector = 1;
    ppe->Part_FirstTrack = 0;

    if (cbitsClus == 12) {
        ppe->Part_FileSystem = PART_DOS2_FAT;       // 12-bit FAT ID
    } else {
#ifdef FAT32
        if (cbitsClus == 32)
        ppe->Part_FileSystem = PART_DOS32X13;   // 32-bit FAT ID
        else
#endif
        if (devpb.DevPrm.OldDevPB.BPB.BPB_BigTotalSectors < 65536)
        ppe->Part_FileSystem = PART_DOS3_FAT;   // 16-bit FAT ID
        else
        ppe->Part_FileSystem = PART_DOSX13;     // >32Mb 16-bit FAT ID
    }

    ppe->Part_LastHead = (BYTE)(devpb.DevPrm.OldDevPB.BPB.BPB_Heads - 1);
    ppe->Part_LastSector = (BYTE)(devpb.DevPrm.OldDevPB.BPB.BPB_SectorsPerTrack + ((maxCyl & 0x0300) >> 2));
    ppe->Part_LastTrack = (BYTE)maxCyl;
    ppe->Part_StartSector = devpb.DevPrm.OldDevPB.BPB.BPB_SectorsPerTrack;
    ppe->Part_TotalSectors = devpb.DevPrm.OldDevPB.BPB.BPB_BigTotalSectors - devpb.DevPrm.OldDevPB.BPB.BPB_SectorsPerTrack;

    // Last but not least, store a valid signature at the end
    // of the partition sector.

    *(PWORD)(abSector+DEFAULT_SECTOR_SIZE-2) = BOOTSECTRAILSIGH;

    dwError = ReadWriteDisk(pvol,
                pvol->v_pdsk->d_hdsk,
                DISK_IOCTL_WRITE,
                &pvol->v_pdsk->d_diActive,
                0,      // sector #
                1,      // number of sectors
                abSector,
                FALSE  ); // remap FATs
    if (dwError) {
        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!FormatVolume: cannot write new partition table (%d)\n"), dwError));
        goto exit;
    }
    DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!FormatVolume: New MBR successfully written\n")));

    // Zap the rest of the hidden sectors, if any

    for (sec=1; sec < csecHidden; sec++) {

        memset(abSector, 0, devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector);

        dwError = ReadWriteDisk(pvol,
                    pvol->v_pdsk->d_hdsk,
                    DISK_IOCTL_WRITE,
                    &pvol->v_pdsk->d_diActive,
                    sec,
                    1,
                    abSector, 
                    FALSE );
        if (dwError) {
        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!FormatVolume: error initializing hidden sectors (%d)\n"), dwError));
        goto exit;
        }
    }
    }

    // Now write the new BPB sector

    {
    SYSTEMTIME st;
    PBOOTSEC pbs = (PBOOTSEC)abSector;
#ifdef FAT32
    PBIGFATBOOTSEC pbgbs = (PBIGFATBOOTSEC)abSector;
#endif
    // The bulk of the BPB sector needs to be zeroed

    memset(abSector, 0, devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector);

    pbs->bsJump[0] = BS2BYTJMP;
    pbs->bsJump[1] = 0xfe;
    pbs->bsJump[2] = 0x90;
    pbs->bsBPB = devpb.DevPrm.OldDevPB.BPB;

    if (csecHidden || devpb.DevPrm.OldDevPB.BPB.BPB_HiddenSectors)
        pbs->bsDriveNumber = PART_BOOTABLE;

    pbs->bsBootSignature = EXT_BOOT_SIGNATURE;

    // Now fill in the portions of the BPB that we just calculated

    pbs->bsBPB.BPB_SectorsPerCluster = (BYTE)csecClus;
    pbs->bsBPB.BPB_ReservedSectors = (WORD)csecReserved;
    pbs->bsBPB.BPB_NumberOfFATs = cFATs;
    pbs->bsBPB.BPB_RootEntries = (WORD)cRootEntries;
    pbs->bsBPB.BPB_BigTotalSectors = devpb.DevPrm.OldDevPB.BPB.BPB_BigTotalSectors;
    if (secVolBias == 0)
        pbs->bsBPB.BPB_BigTotalSectors -= csecHidden;

    // Use the ancient BPB_TotalSectors field if the value will fit

    if (pbs->bsBPB.BPB_BigTotalSectors < 65536) {
        pbs->bsBPB.BPB_TotalSectors = (WORD)pbs->bsBPB.BPB_BigTotalSectors;
        pbs->bsBPB.BPB_BigTotalSectors = 0;
    }

    pbs->bsBPB.BPB_MediaDescriptor = bMediaDesc;
    pbs->bsBPB.BPB_SectorsPerFAT = (WORD)(csecFATs / cFATs);
    pbs->bsBPB.BPB_HiddenSectors = devpb.DevPrm.OldDevPB.BPB.BPB_HiddenSectors;

    // Create a (hopefully unique) serial number

    GetSystemTime(&st);
    pbs->bsVolumeID = ((st.wYear + st.wDayOfWeek) << 16) + (st.wMonth + st.wDay) +
              ((st.wHour + st.wSecond)    << 16) + (st.wMinute + st.wMilliseconds);

    memset(pbs->bsVolumeLabel, 0x20, sizeof(pbs->bsVolumeLabel));
    if (FSDMGR_GetDiskName((HDSK)pvol->v_pdsk->d_hdsk, szVolumeName)) {
        WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, szVolumeName, wcslen(szVolumeName), pbs->bsVolumeLabel, sizeof(pbs->bsVolumeLabel), NULL, NULL);
    }    
    

    // Initialize the file system signature to either 'FAT16' or 'FAT12'
#ifdef TFAT    
    *(DWORD UNALIGNED *)&pbs->bsFileSysType[0] = 0x54414654;
    *(DWORD UNALIGNED *)&pbs->bsFileSysType[4] = (cbitsClus == 16)? 0x20203631 : 0x20203231;
#else
    *(DWORD UNALIGNED *)&pbs->bsFileSysType[0] = 0x31544146;
    *(DWORD UNALIGNED *)&pbs->bsFileSysType[4] = (cbitsClus == 16)? 0x20202036 : 0x20202032;
#endif

#ifdef FAT32
    if (cbitsClus == 32) {

        // Transfer any fields that immediately follow the old BPB before
        // we start writing to the new (big) BPB, lest we stomp on something.

        pbgbs->bgbsDriveNumber = pbs->bsDriveNumber;
        pbgbs->bgbsReserved1 = 0;
        pbgbs->bgbsBootSignature = pbs->bsBootSignature;
        pbgbs->bgbsVolumeID = pbs->bsVolumeID;
        memset(pbgbs->bgbsVolumeLabel, 0x20, sizeof(pbgbs->bgbsVolumeLabel));
        if (FSDMGR_GetDiskName((HDSK)pvol->v_pdsk->d_hdsk, szVolumeName)) {
            WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, szVolumeName, wcslen(szVolumeName), pbgbs->bgbsVolumeLabel, sizeof(pbgbs->bgbsVolumeLabel), NULL, NULL);
        }    

        // Initialize the file system signature to 'FAT32'
#ifdef TFAT    
        *(DWORD UNALIGNED *)&pbgbs->bgbsFileSysType[0] = 0x54414654;
        *(DWORD UNALIGNED *)&pbgbs->bgbsFileSysType[4] = 0x20203233;
#else
        *(DWORD UNALIGNED *)&pbgbs->bgbsFileSysType[0] = 0x33544146;
        *(DWORD UNALIGNED *)&pbgbs->bgbsFileSysType[4] = 0x20202032;
#endif

        // Now zap the key fields in the old BPB that indicate we're dealing
        // with a new (big) BPB.  Note that Win95 (OSR2) normally reserves 32
        // sectors, with the logical sectors 0-2 containing the primary bootstrap
        // (of which sector 1 is also the FSINFO sector), sectors 6-8 containing
        // a backup copy of the bootstrap sectors, and sectors 9-31 unused.

        // NOTE: I'm not sure it's strictly necessary to zero RootEntries, but in
        // any case, the value placed there earlier is not necessarily correct, so
        // we might as well zero it.

        pbs->bsBPB.BPB_SectorsPerFAT = 0;
        pbs->bsBPB.BPB_RootEntries = 0;

        // Now start filling in the fields that are unique to the new (big) BPB.
        // In the interests of being a minimalist, I reserve only two sectors:
        // logical sector 0 for the BPB, and 1 for the FSINFO sector.  I will set
        // BGBPB_BkUpBootSec to zero to indicate there are no backup sectors.
        // Based on the documentation I have, all FAT32-aware systems should be
        // able to handle this format.  But, we'll just have to wait and see.... -JTP

        pbgbs->bgbsBPB.BGBPB_BigSectorsPerFAT = csecFATs / cFATs;
        pbgbs->bgbsBPB.BGBPB_ExtFlags = 0;
        pbgbs->bgbsBPB.BGBPB_FS_Version = FAT32_Curr_FS_Version;
        pbgbs->bgbsBPB.BGBPB_RootDirStrtClus = DATA_CLUSTER;
        pbgbs->bgbsBPB.BGBPB_FSInfoSec = 1;
        pbgbs->bgbsBPB.BGBPB_BkUpBootSec = 0;
        memset(pbgbs->bgbsBPB.BGBPB_Reserved, 0, sizeof(pbgbs->bgbsBPB.BGBPB_Reserved));
    }
#endif

    // Last but not least, store a valid signature at the end
    // of the BPB sector.

    *(PWORD)(abSector+DEFAULT_SECTOR_SIZE-2) = BOOTSECTRAILSIGH;

        dwError = ReadWriteDisk(pvol,
                pvol->v_pdsk->d_hdsk,
                DISK_IOCTL_WRITE,
                &pvol->v_pdsk->d_diActive,
                secVolBias + csecHidden,
                1,
                abSector,
                FALSE);

    if (dwError) {
        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!FormatVolume: cannot write new BPB (%d)\n"), dwError));
        goto exit;
    }
    DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!FormatVolume: New BPB successfully written\n")));
    }

#ifdef FAT32
    // Now write the combination extended boot sector/FSInfo sector for FAT32 volumes

    if (cbitsClus == 32) {
    
    PBIGFATBOOTFSINFO pFSInfo;

    memset(abSector, 0, devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector);

    *(PDWORD)&abSector[0] = SECONDBOOTSECSIG;
    *(PDWORD)&abSector[OFFSETTRLSIG] = BOOTSECTRAILSIG;
    pFSInfo = (PBIGFATBOOTFSINFO)&abSector[OFFSETFSINFOFRMSECSTRT];

    pFSInfo->bfFSInf_Sig = FSINFOSIG;
    pFSInfo->bfFSInf_free_clus_cnt = UNKNOWN_CLUSTER;
    pFSInfo->bfFSInf_next_free_clus = UNKNOWN_CLUSTER;

        dwError = ReadWriteDisk(pvol,
                pvol->v_pdsk->d_hdsk,
                DISK_IOCTL_WRITE,
                &pvol->v_pdsk->d_diActive,
                secVolBias + csecHidden + 1,
                1,
                abSector,
                FALSE);

    if (dwError) {
        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!FormatVolume: cannot write new extended boot sector (%d)\n"), dwError));
        goto exit;
    }
    DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!FormatVolume: New extended boot sector successfully written\n")));
    }

#endif

    // Now write the new FAT(s)

    sec = csecHidden + csecReserved;

    for (iFAT=0; iFAT < cFATs; iFAT++) {

    DWORD secFAT;
    DWORD csecFAT = csecFATs/cFATs;

    for (secFAT=0; secFAT < csecFAT; sec++,secFAT++) {

        // The bulk of each new FAT sector needs to be zeroed.

        memset(abSector, 0, devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector);

        if (secFAT == 0) {

        // The first sector of each FAT needs special EOF
        // marks in the slots corresponding to clusters 0 and 1
        // (which, as you know, are non-existent clusters).

        abSector[0] = bMediaDesc;
        abSector[1] = 0xFF;
        abSector[2] = 0xFF;
        if (cbitsClus >= 16)
            abSector[3] = 0xFF;
#ifdef FAT32
        if (cbitsClus == 32) {
            *(PDWORD)&abSector[4] = FAT32_EOF_MAX;  // this is for cluster 1
            *(PDWORD)&abSector[8] = FAT32_EOF_MAX;  // and this is for cluster 2 (the first -- and initially only -- cluster in the root directory)
        }
#endif
        }

        dwError = ReadWriteDisk(pvol,
                    pvol->v_pdsk->d_hdsk,
                    DISK_IOCTL_WRITE,
                    &pvol->v_pdsk->d_diActive,
                    secVolBias + sec,
                    1,
                    abSector,
                    FALSE);

        if (dwError) {
        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!FormatVolume: cannot write new FAT sector %d (%d)\n"), sec, dwError));
        goto exit;
        }
    }
    DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!FormatVolume: New FAT #%d successfully written\n"), iFAT));
    }

    // Now write the new root directory

#ifdef FAT32
    // Note that if this is a FAT32 volume, then csecRoot wasn't used, and there
    // is no root directory in the traditional sense.  However, we DID automatically
    // reserve the first cluster (DATA_CLUSTER) for the use of the root directory,
    // so we need to zero all the sectors in that first cluster.

    if (cbitsClus == 32) {
        ASSERT(csecRoot == 0);
        csecRoot = csecClus;
    }
#endif

    {
        DWORD secRoot;

        // Each new root directory sector needs to be zeroed.

#ifdef TFAT

        if (cbitsClus != 32)
        {
            memset(abSector, 0, devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector);
        }
        else
        {
            //  FAT32 needs to fill the first root dir with volume labels
            DIRENTRY    adeVolume;
            DWORD       offset;
            int               iVolumeNo;

            CreateDirEntry( NULL, &adeVolume, NULL, ATTR_VOLUME_ID, UNKNOWN_CLUSTER );
            // the first one is volume label
            memcpy(adeVolume.de_name, "TFAT       ", sizeof (adeVolume.de_name));
            memcpy(abSector, &adeVolume, sizeof(adeVolume));

            memcpy(adeVolume.de_name, "DONT_DEL000", sizeof (adeVolume.de_name));
            
            ASSERT(devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector==DEFAULT_SECTOR_SIZE);

            for (offset = sizeof(adeVolume), iVolumeNo = 1; offset < DEFAULT_SECTOR_SIZE; offset += sizeof(adeVolume), iVolumeNo++)
            {
                    adeVolume.de_name[8]  = '0' + (iVolumeNo / 100);
                    adeVolume.de_name[9]  = '0' + ((iVolumeNo / 10) % 10);
                    adeVolume.de_name[10] = '0' + (iVolumeNo % 10);
            
                    memcpy(abSector+offset, &adeVolume, sizeof(adeVolume));
            }
        }
#else
        memset(abSector, 0, devpb.DevPrm.OldDevPB.BPB.BPB_BytesPerSector);
#endif


        for (secRoot=0; secRoot < csecRoot; secRoot++) {

            dwError = ReadWriteDisk(pvol,
                        pvol->v_pdsk->d_hdsk,
                        DISK_IOCTL_WRITE,
                        &pvol->v_pdsk->d_diActive,
                        secVolBias + csecHidden + csecReserved + csecFATs + secRoot,
                        1,
                        abSector,
                        FALSE);

            if (dwError) {
            DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!FormatVolume: cannot write new root directory sector %d (%d)\n"), csecHidden + csecReserved + csecFATs + secRoot, dwError));
            goto exit;
            }
        }
        DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!FormatVolume: New root directory successfully written\n")));
    }

    RETAILMSG(1,(DBGTEXT("FATFS!FormatVolume complete (%d)\n"), dwError));

  exit:
    UnlockVolume(pvol);
    pvol->v_flags &= ~VOLF_FORMATTING;

  abort:
    return dwError;
}

#endif
