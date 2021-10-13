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

    volume.c

Abstract:

    This file contains routines for mounting volumes, direct volume access, etc.

Revision History:

--*/

#include "fatfs.h"

// Warning if you change this then change storemgr.h
static const GUID FATFS_MOUNT_GUID = { 0x169e1941, 0x4ce, 0x4690, { 0x97, 0xac, 0x77, 0x61, 0x87, 0xeb, 0x67, 0xcc } };

#if defined(UNDER_WIN95) && !defined(INCLUDE_FATFS)
#include <pcmd.h>               // for interacting with ReadVolume/WriteVolume
#endif


ERRFALSE(offsetof(BOOTSEC, bsBPB) == offsetof(BIGFATBOOTSEC, bgbsBPB));


/*  ReadVolume
 *
 *  Like ReadWriteDisk, except it takes a PVOLUME, and it works with
 *  volume-relative BLOCKS instead of disk-relative SECTORS.
 *
 *  For now, the caller must specify a block number that starts on a sector
 *  boundary, and the number of blocks must map to a whole number of sectors.
 *
 *  Entry:
 *      pvol        address of VOLUME structure
 *      block       0-based block number
 *      cBlocks     number of blocks to read
 *      pvBuffer    address of buffer to read sectors into
 *
 *  Exit:
 *      ERROR_SUCCESS if successful, else GetLastError() from the FSDMGR_DiskIoControl call issued
 */         

DWORD ReadVolume(PVOLUME pvol, DWORD block, int cBlocks, PVOID pvBuffer)
{
#ifdef UNDER_WIN95
    if (ZONE_BUFFERS && ZONE_PROMPTS) {
        WCHAR wsBuf[10];
        DBGPRINTFW(TEXTW("Read:  block %d, total %d;  allow (Y/N)? "), block, cBlocks);
        DBGSCANFW(wsBuf, ARRAYSIZE(wsBuf));
        if (TOLOWER(wsBuf[0]) == 'n')
            return ERROR_BAD_UNIT;
    }
#endif


    // Assert that the shifted bits in both block and cBlocks are zero

    ASSERT(pvol->v_log2cblkSec<=0? 1 : (((block | cBlocks) & ((1 << pvol->v_log2cblkSec) - 1)) == 0));

    if (pvol->v_flags & VOLF_FROZEN) {
        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!ReadVolume: frozen volume, cannot read block %d!\n"), block));
        return ERROR_DEV_NOT_EXIST;
    }

    // NOTE that all sectors computed in this function are relative to
    // block 0, and that before they are passed to ReadWriteDisk, they are
    // adjusted by v_secBlkBias (the sector bias for block 0).  Thus, it is
    // impossible for this function to read from any of the volume's reserved
    // sectors (eg, the boot sector and BPB) or hidden sectors, because they
    // precede block 0.
    //
    // See the code in IOCTL_DISK_READ_SECTORS and IOCTL_DISK_WRITE_SECTORS
    // to access sectors outside the normal block range.

    return ReadWriteDisk(pvol, pvol->v_pdsk->d_hdsk, DISK_IOCTL_READ, &pvol->v_pdsk->d_diActive, 
        pvol->v_secBlkBias + (block>>pvol->v_log2cblkSec), (cBlocks>>pvol->v_log2cblkSec), pvBuffer, TRUE);
}


/*  WriteVolume
 *
 *  Like ReadWriteDisk, except it takes a PVOLUME, and it works with
 *  volume-relative BLOCKS instead of disk-relative SECTORS.
 *
 *  For now, the caller must specify a block number that starts on a sector
 *  boundary, and the number of blocks must map to a whole number of sectors.
 *
 *  Entry:
 *      pvol        address of VOLUME structure
 *      block       0-based block number
 *      cBlocks     number of blocks to write
 *      pvBuffer    address of buffer to write sectors from
 *
 *  Exit:
 *      ERROR_SUCCESS if successful, else GetLastError() from the FSDMGR_DiskIoControl call issued
 */         

DWORD WriteVolume(PVOLUME pvol, DWORD block, int cBlocks, PVOID pvBuffer)
{
    DWORD sec, csec;

#ifdef UNDER_WIN95
    if (ZONE_BUFFERS && ZONE_PROMPTS) {
        WCHAR wsBuf[10];
        DBGPRINTFW(TEXTW("Write: block %d, total %d;  allow (Y/N)? "), block, cBlocks);
        DBGSCANFW(wsBuf, ARRAYSIZE(wsBuf));
        if (TOLOWER(wsBuf[0]) == 'n')
            return ERROR_BAD_UNIT;
    }
#endif


    // Assert that the shifted bits in both block and cBlocks are zero

    ASSERT(pvol->v_log2cblkSec<=0? 1 : (((block | cBlocks) & ((1 << pvol->v_log2cblkSec) - 1)) == 0));

    if (pvol->v_flags & VOLF_FROZEN) {
        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!WriteVolume: frozen volume, cannot write block %d!\n"), block));
        return ERROR_DEV_NOT_EXIST;
    }

    // NOTE that all sectors computed in this function are relative to
    // block 0, and that before they are passed to ReadWriteDisk, they are
    // adjusted by v_secBlkBias (the sector bias for block 0).  Thus, it is
    // impossible for this function to write to any of the volume's reserved
    // sectors (eg, the boot sector and BPB) or hidden sectors, because they
    // precede block 0.
    //
    // See the code in IOCTL_DISK_READ_SECTORS and IOCTL_DISK_WRITE_SECTORS
    // to access sectors outside the normal block range.

    sec = block >> pvol->v_log2cblkSec;
    csec = cBlocks >> pvol->v_log2cblkSec;

    // If we have multiple FATs, and we're writing to the FAT region,
    // adjust and/or mirror the write appropriately.

#ifdef TFAT   //  no backup FAT support on TFAT
    if (!pvol->v_fTfat)
#endif
    {
        if ((pvol->v_flags & VOLF_BACKUP_FAT) && sec < pvol->v_secEndAllFATs) {
            DWORD secEnd, secBackup;

            secEnd = sec + csec;

            // Check for write to root directory area that starts inside
            // a backup FAT.  Backup FATs are not buffered, hence any buffered
            // data inside the range of the backup FAT could be stale and
            // must NOT be written.

            if (sec >= pvol->v_secEndFAT) {
                ASSERT(secEnd > pvol->v_secEndAllFATs);
                csec = secEnd - pvol->v_secEndAllFATs;
                sec = pvol->v_secEndAllFATs;
                goto write;
            }

            // Check for write to end of the active FAT that ends inside
            // a backup FAT.  Backup FATs are not buffered, hence any buffered
            // data inside the range of the backup FAT could be stale and
            // must NOT be written.

            secBackup = sec;

            if (secEnd > pvol->v_secEndFAT) {

                DWORD csecFATWrite;

                csecFATWrite = pvol->v_secEndFAT - sec;

                if (secEnd < pvol->v_secEndAllFATs) {
                    csec = csecFATWrite;
                }
                else {

                    DWORD cbFATWrite;
                    PBYTE pFAT, pFATBackup;

                    // The current buffer spans all the FATs, so we need to
                    // copy the FAT data to the backup FAT location(s) instead of
                    // doing extra writes.

                    pFAT = (PBYTE)pvBuffer;
                    pFATBackup = pFAT + (pvol->v_csecFAT << pvol->v_log2cbSec);
                    cbFATWrite = (csecFATWrite << pvol->v_log2cbSec);

                    


                    while ((secBackup += pvol->v_csecFAT) < pvol->v_secEndAllFATs) {
                        memcpy(pFATBackup, pFAT, cbFATWrite);
                        pFATBackup += pvol->v_csecFAT << pvol->v_log2cbSec;
                    }
                    goto write;
                }
            }

            // If we're still here, we've got a FAT write that's been restricted
            // to the just the sectors within the active FAT.  Start cycling through
            // the backup FATs now.

            


            while ((secBackup += pvol->v_csecFAT) < pvol->v_secEndAllFATs) {

                DEBUGMSG(ZONE_FATIO,(DBGTEXT("FATFS!WriteVolume: mirroring %d FAT sectors at %d to %d\n"), csec, sec, secBackup));

                ASSERT(secBackup + csec <= pvol->v_secEndAllFATs);

                // We ignore any error here, on the assumption that perhaps the
                // backup FAT has simply gone bad.

                ReadWriteDisk(pvol, pvol->v_pdsk->d_hdsk, DISK_IOCTL_WRITE, &pvol->v_pdsk->d_diActive, 
                    pvol->v_secBlkBias + secBackup, csec, pvBuffer, TRUE);
            }
        }
    }
  write:

    // TEST_BREAK
    PWR_BREAK_NOTIFY(51);
    
    return ReadWriteDisk(pvol, pvol->v_pdsk->d_hdsk, DISK_IOCTL_WRITE, &pvol->v_pdsk->d_diActive, 
        pvol->v_secBlkBias + sec, csec, pvBuffer, TRUE);
}


/*  InitVolume - Initialize a VOLUME structure
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pbgbs - pointer to PBR (partition boot record) for volume
 *
 *  EXIT
 *      TRUE if VOLUME structure successfully initialized, FALSE if not
 */

BOOL InitVolume(PVOLUME pvol, PBIGFATBOOTSEC pbgbs)
{
    DWORD cmaxClus;
    PBIGFATBPB pbpb;
    PDSTREAM pstmFAT;
    PDSTREAM pstmRoot;

    ASSERT(OWNCRITICALSECTION(&pvol->v_cs));

    pvol->v_flags &= ~(VOLF_INVALID | VOLF_12BIT_FAT | VOLF_16BIT_FAT | VOLF_32BIT_FAT | VOLF_BACKUP_FAT);

#ifdef TFAT

    pvol->v_fTfat = FALSE;
    if ((memcmp(pbgbs->bgbsFileSysType, "TFAT", 4) == 0) ||
         (memcmp(((PBOOTSEC)pbgbs)->bsFileSysType, "TFAT", 4) == 0) ||
         ((pvol->v_flFATFS & FATFS_FORCE_TFAT) && (pbgbs->bgbsBPB.oldBPB.BPB_NumberOfFATs == 2 ||
         pbgbs->bgbsBPB.oldBPB.BPB_NumberOfFATs == 0)))
    {
        pvol->v_fTfat = TRUE;

        if (pbgbs->bgbsBPB.oldBPB.BPB_NumberOfFATs == 0)
        {
            // Last transacation was not done yet, we will sync FAT later. For now, change NOF back in order 
            // to init the volume properly
            pbgbs->bgbsBPB.oldBPB.BPB_NumberOfFATs = 2;
        }        
    }
    
#endif


    // Allocate special DSTREAMs for the FAT and the root directory,
    // using PSEUDO cluster numbers 0 and 1.  Root directories of FAT32
    // volumes do have a REAL cluster number, and if we're in a nested init
    // path, we need to use it, but otherwise we'll just use ROOT_PSEUDO_CLUSTER
    // and record the real cluster number when we figure it out, in the
    // FAT32-specific code farther down...

    pstmFAT = OpenStream(pvol, FAT_PSEUDO_CLUSTER, NULL, NULL, NULL, OPENSTREAM_CREATE);
    ASSERT(pvol->v_pstmFAT == NULL || pvol->v_pstmFAT == pstmFAT);

    pstmRoot = OpenStream(pvol, pvol->v_pstmRoot? pvol->v_pstmRoot->s_clusFirst : ROOT_PSEUDO_CLUSTER, NULL, NULL, NULL, OPENSTREAM_CREATE);

    ASSERT(pvol->v_pstmRoot == NULL || pvol->v_pstmRoot == pstmRoot);

    if (!pstmFAT || !pstmRoot)
        goto exit;

    // Add extra refs to these special streams to make them stick around;
    // we only want to do this if the current open is their only reference,
    // because if they've been resurrected, then they still have their original
    // extra refs.

    if (pstmFAT->s_refs == 1) {
        pstmFAT->s_refs++;
    }

    if (pstmRoot->s_refs == 1) {
        pstmRoot->s_refs++;
    }

    pvol->v_pstmFAT = pstmFAT;
    pvol->v_pstmRoot = pstmRoot;

    pstmRoot->s_attr = ATTR_DIRECTORY;

    // We have completed the required stream initialization for this
    // volume.  Now perform boot sector verification and BPB validation.

    if (pvol->v_pdsk->d_diActive.di_flags & DISK_INFO_FLAG_UNFORMATTED) {
        RETAILMSG(TRUE,(DBGTEXT("FATFS!InitVolume: driver has set 'unformatted' bit, marking volume invalid\n")));
        pvol->v_flags |= VOLF_INVALID;
    }

    


    if (!(pvol->v_flags & VOLF_INVALID)) 
    {
        if (pbgbs->bgbsJump[0] != BSNOP     &&
            pbgbs->bgbsJump[0] != BS2BYTJMP &&
            pbgbs->bgbsJump[0] != BS3BYTJMP) 
        {
            RETAILMSG(TRUE,(DBGTEXT("FATFS!InitVolume: sector 0 byte 0 suspicious (0x%x)\n"), pbgbs->bgbsJump[0]));
        }
        
        if (pbgbs->bgbsBPB.oldBPB.BPB_BytesPerSector < DEFAULT_SECTOR_SIZE ||
            Log2(pbgbs->bgbsBPB.oldBPB.BPB_BytesPerSector) == -1 ||
            pbgbs->bgbsBPB.oldBPB.BPB_NumberOfFATs < 1 ||
            *(PWORD)((PBYTE)pbgbs+DEFAULT_SECTOR_SIZE-2) != BOOTSECTRAILSIGH) {
            DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!InitVolume: invalid BPB, volume deemed invalid\n")));
            pvol->v_flags |= VOLF_INVALID;
        }        
    }

    pbpb = &pbgbs->bgbsBPB;

    // Preliminary FAT32 detection: if FAT32 support is disabled, then ALWAYS
    // set VOLF_INVALID;  otherwise, do it only if there is a version mismatch.

    if (!(pvol->v_flags & VOLF_INVALID) && pbpb->oldBPB.BPB_SectorsPerFAT == 0) {

#ifdef FAT32
        if (pbpb->BGBPB_FS_Version > FAT32_Curr_FS_Version) {
            RETAILMSG(TRUE,(TEXT("FATFS!InitVolume: FAT32 volume version unsupported (%d > %d)\n"),
                        pbpb->BGBPB_FS_Version, FAT32_Curr_FS_Version));
#else
            RETAILMSG(TRUE,(TEXT("FATFS!InitVolume: FAT32 volumes not supported in this build\n")));
#endif
            pvol->v_flags |= VOLF_INVALID;

#ifdef FAT32
        }

        if (!(pvol->v_flags & VOLF_INVALID)) {
            pvol->v_flags |= VOLF_32BIT_FAT;
            pvol->v_secFSInfo = pbpb->BGBPB_FSInfoSec;
        }
#endif
    }

    if (pvol->v_flags & VOLF_INVALID) {

        




        memset(&pvol->v_bMediaDesc, 0, offsetof(VOLUME, v_pstmFAT) - offsetof(VOLUME, v_bMediaDesc));
        memset(&pstmFAT->s_run, 0, sizeof(pstmFAT->s_run));
        memset(&pstmRoot->s_run, 0, sizeof(pstmRoot->s_run));
        goto exit;
    }

    // Now compute volume dimensions, etc, from BPB data

    pvol->v_bMediaDesc = pbpb->oldBPB.BPB_MediaDescriptor;

    if (pbpb->oldBPB.BPB_NumberOfFATs > 1)
        pvol->v_flags |= VOLF_BACKUP_FAT; 

    // Compute sector bias for volume (for sector-based I/O)

    pvol->v_secVolBias = pbpb->oldBPB.BPB_HiddenSectors;

    // Compute sector bias for block 0 (for block-based I/O only)

    pvol->v_secBlkBias = pbpb->oldBPB.BPB_HiddenSectors + pbpb->oldBPB.BPB_ReservedSectors;

    // Compute how many sectors are on the volume

    pvol->v_csecTotal = pbpb->oldBPB.BPB_TotalSectors;
    if (pvol->v_csecTotal == 0)
        pvol->v_csecTotal = pbpb->oldBPB.BPB_BigTotalSectors;

    if (pvol->v_csecTotal <= pbpb->oldBPB.BPB_ReservedSectors) {
        DEBUGMSGBREAK(ZONE_INIT || ZONE_ERRORS,
                 (DBGTEXT("FATFS!InitVolume: total sectors (%d) is incorrect, adjusting to match driver\n"), pvol->v_csecTotal));
        pvol->v_csecTotal = pvol->v_pdsk->d_diActive.di_total_sectors - pvol->v_secBlkBias;
    }
    else
        pvol->v_csecTotal -= pbpb->oldBPB.BPB_ReservedSectors;

#ifdef DEBUG
    







    if (!(pvol->v_pdsk->d_diActive.di_flags & DISK_INFO_FLAG_CHS_UNCERTAIN) &&
         (pvol->v_pdsk->d_diActive.di_heads != pbpb->oldBPB.BPB_Heads ||
          pvol->v_pdsk->d_diActive.di_sectors != pbpb->oldBPB.BPB_SectorsPerTrack)) {

        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,
                 (DBGTEXT("FATFS!InitVolume: driver disagrees with BPB (BPB CHS=%d:%d:%d)\n"),
                  (pvol->v_csecTotal + pvol->v_secBlkBias) / (pbpb->oldBPB.BPB_Heads * pbpb->oldBPB.BPB_SectorsPerTrack),
                  pbpb->oldBPB.BPB_Heads,
                  pbpb->oldBPB.BPB_SectorsPerTrack));
    }
#endif

    // We've seen cases where the driver thinks there are fewer sectors on
    // the disk than the BPB does, which means that any I/O near the end of
    // the volume will probably fail.  So for now, we'll try to catch these
    // cases, report them, and then adjust our values to agree with the driver.

    if (pvol->v_pdsk->d_diActive.di_total_sectors < pvol->v_csecTotal + pvol->v_secBlkBias) {

        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,
                 (DBGTEXT("FATFS!InitVolume: driver reports fewer sectors than BPB! (%d vs. %d)\n"),
                  pvol->v_pdsk->d_diActive.di_total_sectors, pvol->v_csecTotal + pvol->v_secBlkBias));

        pvol->v_csecTotal = pvol->v_pdsk->d_diActive.di_total_sectors - pvol->v_secBlkBias;
    }

    pvol->v_log2csecClus = Log2(pbpb->oldBPB.BPB_SectorsPerCluster);
    pvol->v_log2cbSec = Log2(pbpb->oldBPB.BPB_BytesPerSector);
    pvol->v_log2cblkSec = pvol->v_log2cbSec - BLOCK_LOG2;
    pvol->v_log2cblkClus = pvol->v_log2cblkSec + pvol->v_log2csecClus;
    pvol->v_cbClus = 1 << (pvol->v_log2cbSec   + pvol->v_log2csecClus);

    






#ifdef FAT32
    if (pbpb->oldBPB.BPB_SectorsPerFAT == 0) {

        pvol->v_serialID = pbgbs->bgbsVolumeID;
        memcpy(pvol->v_label, pbgbs->bgbsVolumeLabel, sizeof(pbgbs->bgbsVolumeLabel));   

        pvol->v_csecFAT = pbpb->BGBPB_BigSectorsPerFAT;

        // Set up the FAT DSTREAM structure

        pvol->v_secEndFAT = pbpb->BGBPB_BigSectorsPerFAT * (pbpb->BGBPB_ExtFlags & BGBPB_F_ActiveFATMsk);
        pvol->v_secEndAllFATs = pbpb->BGBPB_BigSectorsPerFAT * pbpb->oldBPB.BPB_NumberOfFATs;

        pstmFAT->s_size = pbpb->BGBPB_BigSectorsPerFAT << pvol->v_log2cbSec;
        ASSERT(pstmFAT->s_run.r_start == 0);
        pstmFAT->s_run.r_end = pstmFAT->s_size;
        pstmFAT->s_run.r_blk = pvol->v_secEndFAT << pvol->v_log2cblkSec;

        pvol->v_secEndFAT += pbpb->BGBPB_BigSectorsPerFAT;

        // Set up the root directory DSTREAM structure

        pstmRoot->s_size = MAX_DIRSIZE;
        pstmRoot->s_clusFirst = pbpb->BGBPB_RootDirStrtClus;
        RewindStream(pstmRoot, INVALID_POS);

        pvol->v_csecUsed = pvol->v_secEndAllFATs;

        // Compute block bias for cluster 0.  The first data cluster is 2.

        pvol->v_blkClusBias = (pvol->v_csecUsed << pvol->v_log2cblkSec) -
                                  (DATA_CLUSTER << pvol->v_log2cblkClus);

        // Set the maximum cluster # supported for this volume;  it is the
        // smaller of 1) total clusters on the media and 2) total cluster entries
        // the FAT can contain.

        cmaxClus = pstmFAT->s_size/sizeof(DWORD) - DATA_CLUSTER;
        pvol->v_clusMax = ((pvol->v_csecTotal - pvol->v_csecUsed) >> pvol->v_log2csecClus);
        if (pvol->v_clusMax > cmaxClus) {
            DEBUGMSGBREAK(ZONE_INIT || ZONE_ERRORS,
                (DBGTEXT("FATFS!InitVolume: total clusters (%d) exceeds FAT32 size (%d), reducing...\n"), pvol->v_clusMax, cmaxClus));
            pvol->v_clusMax = cmaxClus;
        }
        pvol->v_clusMax++;      // now convert total clusters to max cluster #

        pvol->v_unpack = Unpack32;
        pvol->v_pack = Pack32;
        pvol->v_clusEOF = FAT32_EOF_MIN;

    } else
#endif  // FAT32
    {
        // 16-bit or 12-bit FAT volume

        PBOOTSEC pbs = (PBOOTSEC)pbgbs;

        pvol->v_serialID = pbs->bsVolumeID;
        memcpy(pvol->v_label, pbs->bsVolumeLabel, sizeof(pbs->bsVolumeLabel)); 

        pvol->v_csecFAT = pbpb->oldBPB.BPB_SectorsPerFAT;

        // Set up the FAT DSTREAM structure;  always assume that the first FAT
        // is the active one

        pvol->v_secEndFAT = pbpb->oldBPB.BPB_SectorsPerFAT;
        pvol->v_secEndAllFATs = pbpb->oldBPB.BPB_SectorsPerFAT * pbpb->oldBPB.BPB_NumberOfFATs;

        pstmFAT->s_size = pbpb->oldBPB.BPB_SectorsPerFAT << pvol->v_log2cbSec;
        ASSERT(pstmFAT->s_run.r_start == 0);
        pstmFAT->s_run.r_end = pstmFAT->s_size;
        pstmFAT->s_run.r_blk = 0;

        // Set up the root directory DSTREAM structure

        pstmRoot->s_size = pbpb->oldBPB.BPB_RootEntries * sizeof(DIRENTRY);
        pstmRoot->s_clusFirst = ROOT_PSEUDO_CLUSTER;
        pstmRoot->s_run.r_start = 0;
        pstmRoot->s_run.r_end = pstmRoot->s_size;
        pstmRoot->s_run.r_blk = pvol->v_secEndAllFATs << pvol->v_log2cblkSec;

        pvol->v_csecUsed = pvol->v_secEndAllFATs + ((pstmRoot->s_size + pbpb->oldBPB.BPB_BytesPerSector-1) >> pvol->v_log2cbSec);

        // Compute block bias for cluster 0.  The first data cluster is 2.

        pvol->v_blkClusBias = (pvol->v_csecUsed << pvol->v_log2cblkSec) -
                                  (DATA_CLUSTER << pvol->v_log2cblkClus);

        // clusMax is temporarily the total # of clusters, and cmaxClus is
        // temporarily the total number of BITS contained in the FAT (we'll divide
        // that by 12 or 16 as appropriate, and then use it to make sure clusMax is valid).

        pvol->v_clusMax = (pvol->v_csecTotal - pvol->v_csecUsed) >> pvol->v_log2csecClus;
        cmaxClus = pvol->v_csecFAT << (pvol->v_log2cbSec+3);    // add 3 to get # bits instead of # bytes

        if (pvol->v_clusMax >= FAT1216_THRESHOLD) {
            cmaxClus /= 16;
            pvol->v_flags |= VOLF_16BIT_FAT;
            pvol->v_unpack = Unpack16;
            pvol->v_pack = Pack16;
            pvol->v_clusEOF = FAT16_EOF_MIN;
        } else {
            cmaxClus /= 12;
            pvol->v_flags |= VOLF_12BIT_FAT;
            pvol->v_unpack = Unpack12;
            pvol->v_pack = Pack12;
            pvol->v_clusEOF = (pvol->v_clusMax < FAT12_EOF_SPECIAL-1)? FAT12_EOF_SPECIAL : FAT12_EOF_MIN;
        }

        // Set the maximum cluster # supported for this volume;  it is the
        // smaller of 1) total clusters on the media and 2) total cluster entries
        // the FAT can contain.

        cmaxClus -= DATA_CLUSTER;
        if (pvol->v_clusMax > cmaxClus) {
            DEBUGMSGBREAK(ZONE_INIT || ZONE_ERRORS,
                (DBGTEXT("FATFS!InitVolume: total clusters (%d) exceeds FAT size (%d), reducing...\n"), pvol->v_clusMax, cmaxClus));
            pvol->v_clusMax = cmaxClus;
        }
        pvol->v_clusMax++;      // now convert total clusters to max cluster #
    }

#ifdef TFAT
    if (pvol->v_fTfat) {
        pvol->v_clusFrozenHead = NO_CLUSTER;  

        // Setup a bit arry to record what FAT sector is changed, init to -1 in order to sync FAT1 with FAT0 when volume is mounted
        pvol->v_DirtySectorsInFAT.dwSize = (pvol->v_csecFAT + 7) >> 3;
        //  note that InitVolume can be called again, for example, at FormatVolume where the FAT size may change
        if(pvol->v_DirtySectorsInFAT.lpBits)
            LocalFree(pvol->v_DirtySectorsInFAT.lpBits);
        pvol->v_DirtySectorsInFAT.lpBits = (LPBYTE)LocalAlloc (LPTR, pvol->v_DirtySectorsInFAT.dwSize);

        // No sectors of FAT0 are cached;
        pvol->v_secFATBufferBias = NO_CLUSTER;

        //  don't alloc in the beginning, need to free it in case InitVolume is called again
        //  in case it is called again at FormatVolume, the cluster size may change
        if(pvol->v_ClusBuf)
        {
            LocalFree(pvol->v_ClusBuf);
            pvol->v_ClusBuf = NULL;
        }
    }
#endif

  exit:
    pvol->v_clusAlloc = pvol->v_cclusFree = UNKNOWN_CLUSTER;

    // If the FORMATTING bit is set and the volume is valid, then we assume that the volume
    // has just been reformatted, and therefore that the number of free clusters is the same
    // as total clusters.

    if ((pvol->v_flags & (VOLF_INVALID|VOLF_FORMATTING)) == VOLF_FORMATTING)
        pvol->v_cclusFree = pvol->v_clusMax-1;

    CloseStream(pstmRoot);
    CloseStream(pstmFAT);

#ifdef TFAT
    if (pvol->v_fTfat) {
        // Keep a copy of boot sector
        ReadWriteDisk( pvol, pvol->v_pdsk->d_hdsk, DISK_IOCTL_READ, &pvol->v_pdsk->d_diActive,
            pvol->v_secVolBias, 1, pvol->v_cbs, FALSE );
        InitFATs(pvol);
    }
#endif

#ifdef DISK_CACHING
    pvol->v_CacheSize = 0;
    pvol->v_pFATCacheBuffer = NULL;
    pvol->v_pFATCacheLookup = NULL;
    SetupDiskCache(pvol);
#endif
    
    
    return pstmFAT && pstmRoot;
}


/*  ValidateFATSector - Verify FAT sector has valid media ID
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pvSector - pointer to memory containing FAT sector
 *
 *  EXIT
 *      TRUE if given FAT sector appears to be valid, FALSE if not
 */

BOOL ValidateFATSector(PVOLUME pvol, PVOID pvSector)
{
    DWORD dwMask;

    dwMask = 0xff0;
    if (pvol->v_bMediaDesc == MEDIA_HD)
        dwMask = 0xff8;

    if (pvol->v_flags & VOLF_16BIT_FAT)
        dwMask |= 0xfff0;
#ifdef FAT32
    else if (pvol->v_flags & VOLF_32BIT_FAT)
        dwMask |= 0x0ffffff0;
#endif
    else
        ASSERT(pvol->v_flags & VOLF_12BIT_FAT);

    if ((*(PDWORD)pvSector & dwMask) < dwMask)
        return FALSE;

    // Needs to add more checking

    return TRUE;
}


















PVOLUME FindVolume(PDSK pdsk, PBIGFATBOOTSEC pbgbs)
{
    PBIGFATBPB pbpb = &pbgbs->bgbsBPB;
    PVOLUME pvol = pdsk->pVol;

    ASSERT(OWNCRITICALSECTION(&csFATFS));

    if (!pvol) {
        pvol = (PVOLUME)LocalAlloc(LPTR, sizeof(VOLUME));

        if (pvol) {
            pvol->v_hVol = 0;
        }    

    } else {
        pvol->v_flags |= VOLF_REMOUNTED;
        return pvol;
    }    
    if (!pvol)
        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!FindVolume: out of memory!\n")));
    else {
        DEBUGALLOC(sizeof(VOLUME));

        InitializeCriticalSection(&pvol->v_cs);
        DEBUGALLOC(DEBUGALLOC_CS);
        InitializeCriticalSection(&pvol->v_csStms);
        DEBUGALLOC(DEBUGALLOC_CS);

#ifdef PATH_CACHING
        InitList((PDLINK)&pvol->v_dlCaches);
        InitializeCriticalSection(&pvol->v_csCaches);
        DEBUGALLOC(DEBUGALLOC_CS);
#endif
        InitList((PDLINK)&pvol->v_dlOpenStreams);
        pvol->v_pdsk = pdsk;
        pvol->v_volID = INVALID_AFS;

#ifdef DISK_CACHING
        InitializeCriticalSection(&pvol->v_csFATCache);
        DEBUGALLOC(DEBUGALLOC_CS);
#endif
    }
    return pvol;
}


/*  TestVolume - Test a VOLUME's validity
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      ppbgbs - pointer to address of PBR (partition boot record) for volume
 *
 *  EXIT
 *      ERROR_SUCCESS, else VOLUME's INVALID bit set if any errors encountered
 */

DWORD TestVolume(PVOLUME pvol, PBIGFATBOOTSEC *ppbgbs)
{
    DWORD cbSecOrig;
    DWORD dwError = ERROR_SUCCESS;
    PDSK pdsk = pvol->v_pdsk;
    PBIGFATBOOTSEC pbgbs = *ppbgbs;


    if (pvol->v_flags & VOLF_INVALID)
        return dwError;

    // Make sure other key BPB values are in sync with the driver (ie, sector
    // size), assuming driver info was provided.

    cbSecOrig = pdsk->d_diActive.di_bytes_per_sect;

    if (cbSecOrig != pbgbs->bgbsBPB.oldBPB.BPB_BytesPerSector) {

        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,
                        (DBGTEXT("FATFS!TestVolume: forcing driver sector size (%d) to match BPB (%d)\n"),
                         cbSecOrig, pbgbs->bgbsBPB.oldBPB.BPB_BytesPerSector));

        pdsk->d_diActive.di_bytes_per_sect = pbgbs->bgbsBPB.oldBPB.BPB_BytesPerSector;

        if (SetDiskInfo(pdsk->d_hdsk, &pdsk->d_diActive) == ERROR_SUCCESS) {

            pvol->v_flags |= VOLF_MODDISKINFO;

            



            if (pdsk->d_diActive.di_bytes_per_sect > cbSecOrig) {
                PVOID pTemp;
                pTemp = LocalReAlloc((HLOCAL)pbgbs, pdsk->d_diActive.di_bytes_per_sect, LMEM_MOVEABLE);
                if (pTemp) {
                    pbgbs = pTemp;
                } else {
                    DEBUGCHK(0);
                    return ERROR_OUTOFMEMORY;
                }    
                if (pbgbs) {
                    DEBUGALLOC(pdsk->d_diActive.di_bytes_per_sect - cbSecOrig);
                    *ppbgbs = pbgbs;        // update the caller's buffer address, too
                }
                else {
                    dwError = GetLastError();
                    DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!TestVolume: out of memory, volume deemed invalid (%d)\n"), dwError));
                    pvol->v_flags |= VOLF_INVALID;
                    goto exit;
                }
            }
        }
        else {

            // If SetDiskInfo returned an error, then revert to the
            // size originally reported, but continue trying to mount.

            pdsk->d_diActive.di_bytes_per_sect = cbSecOrig;
            DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!TestVolume: SetDiskInfo call to driver failed (%d)\n"), GetLastError()));
        }
    }

    // Perform some trial reads now (first sector of first FAT, first
    // sector of second FAT if one exists, and anything else we think of).
    // We will clear VOLF_INVALID only on success.  NOTE that we can now reuse
    // the sector containing the BPB;  we've extracted everything we needed
    // from it.

#ifdef FAT32
    if (pvol->v_flags & VOLF_32BIT_FAT) {

        PBIGFATBOOTFSINFO pFSInfo;

        dwError = ReadWriteDisk(pvol,
                                pdsk->d_hdsk,
                                DISK_IOCTL_READ,
                                &pdsk->d_diActive,
                                pvol->v_secVolBias + 1,
                                1,
                                pbgbs, 
                                TRUE);

        if (dwError != ERROR_SUCCESS
            ||
            *(PDWORD)((PBYTE)pbgbs + 0) != SECONDBOOTSECSIG
            ||
            *(PDWORD)((PBYTE)pbgbs + OFFSETTRLSIG) != BOOTSECTRAILSIG)
        {
            DEBUGMSGBREAK(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!TestVolume: error verifying extended boot sector, volume deemed invalid (%d)\n"), dwError));
            pvol->v_flags |= VOLF_INVALID;
            goto exit;
        }

        // If secFSInfo happens to be 1 (which is typical), then there's no need
        // to perform another read, since it's the same sector we just read (above).

        if (pvol->v_secFSInfo != 1) {
            dwError = ReadWriteDisk(pvol,
                                    pdsk->d_hdsk,
                                    DISK_IOCTL_READ,
                                    &pdsk->d_diActive,
                                    pvol->v_secVolBias + pvol->v_secFSInfo,
                                    1,
                                    pbgbs,
                                    TRUE);
        }

        if (dwError != ERROR_SUCCESS
            ||
            *(PDWORD)((PBYTE)pbgbs + 0) != SECONDBOOTSECSIG
            ||
            *(PDWORD)((PBYTE)pbgbs + OFFSETTRLSIG) != BOOTSECTRAILSIG
            ||
            (pFSInfo = (PBIGFATBOOTFSINFO)((PBYTE)pbgbs + OFFSETFSINFOFRMSECSTRT))->bfFSInf_Sig != FSINFOSIG)
        {
            DEBUGMSGBREAK(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!TestVolume: error verifying FS info sector, volume deemed invalid (%d)\n"), dwError));
            pvol->v_flags |= VOLF_INVALID;
            goto exit;
        }

        // Don't use the FSInfo values since we will not update them (they are legacy from Win 9x).
#if 0
        if (pFSInfo->bfFSInf_free_clus_cnt < pvol->v_clusMax &&
            pFSInfo->bfFSInf_next_free_clus <= pvol->v_clusMax) {

            pvol->v_cclusFree = pFSInfo->bfFSInf_free_clus_cnt;
            pvol->v_clusAlloc = pFSInfo->bfFSInf_next_free_clus;
        }
        else {
            // Note that if the FSInfo fields are simply UNKNOWN_CLUSTER, then my assumption
            // is that they simply haven't been initialized yet (as opposed to being truly bogus).

            DEBUGMSG((ZONE_INIT || ZONE_ERRORS) &&
                     pFSInfo->bfFSInf_free_clus_cnt != UNKNOWN_CLUSTER &&
                     pFSInfo->bfFSInf_next_free_clus != UNKNOWN_CLUSTER, (DBGTEXT("FATFS!TestVolume: FS info data suspicious (%x,%x), ignoring...\n"), pFSInfo->bfFSInf_free_clus_cnt, pFSInfo->bfFSInf_next_free_clus));
        }
#endif        
    }
#endif  // FAT32

    dwError = ReadWriteDisk(pvol,
                            pdsk->d_hdsk,
                            DISK_IOCTL_READ,
                            &pdsk->d_diActive,
                            pvol->v_secBlkBias + 0,
                            1,
                            pbgbs,
                            FALSE);

    if (dwError != ERROR_SUCCESS || !ValidateFATSector(pvol, pbgbs))
    {
            DEBUGMSGBREAK(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!TestVolume: error reading first FAT, volume deemed invalid (%d)\n"), dwError));
            pvol->v_flags |= VOLF_INVALID;
            goto exit;
    }

    if (pvol->v_flags & VOLF_BACKUP_FAT) {

        dwError = ReadWriteDisk(pvol,
                pdsk->d_hdsk,
                DISK_IOCTL_READ,
                &pdsk->d_diActive,
                pvol->v_secBlkBias + pvol->v_csecFAT,
                1,
                pbgbs,
                FALSE);

        if (dwError != ERROR_SUCCESS || !ValidateFATSector(pvol, pbgbs))
        {
                DEBUGMSGBREAK(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!TestVolume: error reading backup FAT, volume deemed invalid (%d)\n"), dwError));
                pvol->v_flags |= VOLF_INVALID;
                goto exit;
        }
    }

    // Verify we can read the last sector in the last cluster of the volume too...
#if 0
    dwError = ReadWriteDisk(pvol,
                            pdsk->d_hdsk,
                            DISK_IOCTL_READ,
                            &pdsk->d_diActive,
                            CLUSTERTOSECTOR(pvol, pvol->v_clusMax) +
                                (1 << pvol->v_log2csecClus) - 1,
                            1,
                            pbgbs);

    if (dwError != ERROR_SUCCESS)
    {
        DEBUGMSGBREAK(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!TestVolume: error reading last sector, volume deemed invalid (%d)\n"), dwError));
        pvol->v_flags |= VOLF_INVALID;
        goto exit;
    }
#endif

    // If we modified the drive parameters but the volume isn't valid anyway, restore the default parameters.

  exit:
    if ((pvol->v_flags & (VOLF_MODDISKINFO | VOLF_INVALID)) == (VOLF_MODDISKINFO | VOLF_INVALID)) {
        pvol->v_flags &= ~VOLF_MODDISKINFO;
        pdsk->d_diActive.di_bytes_per_sect = cbSecOrig;
        SetDiskInfo(pdsk->d_hdsk, &pdsk->d_diActive);
    }

    return dwError;
}


/*  RefreshVolume - Refresh a VOLUME's handles
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *
 *  EXIT
 *      Stream handles are restored, provided the volume is valid AND
 *      the streams appear to be in the same state we left them.
 */

void RefreshVolume(PVOLUME pvol)
{
    PDSTREAM pstm, pstmEnd;

    if (!(pvol->v_flags & (VOLF_REMOUNTED | VOLF_RECYCLED)))
        return;

    ASSERT(OWNCRITICALSECTION(&pvol->v_cs));

    // Walk the open stream list, regenerate each stream, then walk the
    // open handle list for each successfully regenerated stream, and clear
    // the unmounted bit for each open handle.

    // First, make sure the VISITED bit is clear in every stream currently
    // open on this volume.

    EnterCriticalSection(&pvol->v_csStms);

    pstm = pvol->v_dlOpenStreams.pstmNext;
    pstmEnd = (PDSTREAM)&pvol->v_dlOpenStreams;

    while (pstm != pstmEnd) {
        pstm->s_flags &= ~STF_VISITED;
        pstm = pstm->s_dlOpenStreams.pstmNext;
    }

    // Now find the next unvisited stream.  Note that every iteration of the
    // loop starts with the volume critical section held.

  restart:
    pstm = pvol->v_dlOpenStreams.pstmNext;
    while (pstm != pstmEnd) {

        if (pstm->s_flags & STF_VISITED) {
            pstm = pstm->s_dlOpenStreams.pstmNext;
            continue;
        }

        pstm->s_flags |= STF_VISITED;

        // Add a ref to insure that the stream can't go away once we
        // let go of the volume's critical section.

        pstm->s_refs++;
        
        LeaveCriticalSection(&pvol->v_csStms);
        EnterCriticalSection(&pstm->s_cs);

        // Find/read the block containing this stream's DIRENTRY, unless
        // it's a VOLUME-based stream, in which case we'll just automatically
        // remount it.

        ASSERT(pstm->s_pvol == pvol);

        // Volume handles are always remounted, regardless

        if (pstm->s_flags & STF_VOLUME)
            pstm->s_flags &= ~STF_UNMOUNTED;

        if ((pstm->s_flags & STF_UNMOUNTED) && !(pvol->v_flags & VOLF_INVALID)) {

            PBUF pbuf;

            ASSERT(pstm->s_blkDir != INVALID_BLOCK);

            if (FindBuffer(pvol, pstm->s_blkDir, NULL, FALSE, &pbuf) == ERROR_SUCCESS) {

                PDIRENTRY pde;

                pde = (PDIRENTRY)(pbuf->b_pdata + pstm->s_offDir);

                // Check the 8.3 name first...

                if (memcmp(pde->de_name, pstm->s_achOEM, sizeof(pde->de_name)) == 0) {

                    




                    DWORD clusEntry = GETDIRENTRYCLUSTER(pstm->s_pvol, pde);
                    if (clusEntry == NO_CLUSTER)
                        clusEntry = UNKNOWN_CLUSTER;

                    if ((pstm->s_flags & STF_DIRTY) ||
                        pde->de_attr == pstm->s_attr &&
                        pde->de_size == pstm->s_size && clusEntry == pstm->s_clusFirst)
                    {
                        pstm->s_flags &= ~STF_UNMOUNTED;
                    }
                }
                UnholdBuffer(pbuf);
            }
        }

        // If the stream is now (re)mounted, make sure all its handles are (re)mounted also.

        if (!(pstm->s_flags & STF_UNMOUNTED)) {

            PFHANDLE pfh, pfhEnd;

            pfh = pstm->s_dlOpenHandles.pfhNext;
            pfhEnd = (PFHANDLE)&pstm->s_dlOpenHandles;

            while (pfh != pfhEnd) {
                pfh->fh_flags &= ~FHF_UNMOUNTED;
                pfh = pfh->fh_dlOpenHandles.pfhNext;
            }
        }

        CloseStream(pstm);
        EnterCriticalSection(&pvol->v_csStms);
        goto restart;
    }

    LeaveCriticalSection(&pvol->v_csStms);

    // Make sure the buffer pool is clean too, now that we've finished
    // resurrecting and committing all resurrectable streams.

    if (CommitVolumeBuffers(pvol) == ERROR_SUCCESS) {

        DEBUGMSG(ZONE_INIT && (pvol->v_flags & VOLF_DIRTY),(DBGTEXT("FATFS!RefreshVolume: dirty data successfully committed\n")));
        pvol->v_flags &= ~VOLF_DIRTY;

    }
    else
        DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!RefreshVolume: unable to commit dirty data to volume 0x%08x\n"), pvol));
}


























DWORD LockVolume(PVOLUME pvol, DWORD dwFlags)
{
    DWORD dwError = ERROR_SUCCESS;
//    pvol->v_flags |= dwFlags;
//   return dwError;
    
    EnterCriticalSection(&pvol->v_cs);

    // Since VOLF_LOCKED is the concatenation of all lock flags, LockVolume
    // is disallowed if any lock of any kind is already in effect.  This
    // simplification should be fine for our purposes.

    if (!(pvol->v_flags & VOLF_LOCKED)) {

        pvol->v_flags |= dwFlags;

        if (CheckStreamHandles(pvol, NULL)) {
            pvol->v_flags &= ~dwFlags;
            dwError = ERROR_ACCESS_DENIED;
        }
    }
    else
        dwError = ERROR_DRIVE_LOCKED;

    LeaveCriticalSection(&pvol->v_cs);

    // If successful, wait until all threads are done before returning.
    // Easiest way to do this is simulate a quick power-off/power-on sequence. 
    // Since the volume's LOCKED bit is already set, we don't need to worry about
    // any new calls doing anything until the volume is unlocked.

    if (!dwError && cFATThreads > 1) {
        FAT_Notify(pvol, FSNOTIFY_POWER_OFF);
        FAT_Notify(pvol, FSNOTIFY_DEVICES_ON);
    }
    return dwError;
}


/*  UnlockVolume - Unlock a previously locked VOLUME
 *
 *  ENTRY
 *      pvol -> VOLUME structure
 *
 *  EXIT
 *      None
 */

void UnlockVolume(PVOLUME pvol)
{
//    pvol->v_flags &= ~VOLF_LOCKED;
//    return;
    // We need to clear the volume's lock flag first, because if
    // MountDisk decides to try to format it, it will need to be able
    // to (re)lock the volume.

    ASSERT(pvol->v_flags & VOLF_LOCKED);
    pvol->v_flags &= ~VOLF_LOCKED;

    if (pvol->v_flags & VOLF_MODIFIED) {

        DWORD dwOldFlags;
        HANDLE hdsk = pvol->v_pdsk->d_hdsk;

        // Reinvigorate our data structures since it appears that the disk
        // could have been significantly modified.

        EnterCriticalSection(&csFATFS);

        // Since the unmount/remount can cause the REMOUNTED or RECYCLED bits
        // to be set, and since this could be called *within* an earlier mount,
        // we don't want to lose the original bits for that earlier mount.

        dwOldFlags = pvol->v_flags;

        // We set the RETAIN bit, so that we don't have to worry about
        // UnmountVolume freeing the current VOLUME structure.  It will be
        // reset by UnmountVolume as soon as CloseVolume has completed.
        // (I used to set the FROZEN bit instead, but that doesn't get reset
        // until much later -- when the volume is reopened by OpenVolume -- and
        // it prevents legitimate ReadVolume/WriteVolume calls from working -JTP)

        pvol->v_flags |= VOLF_RETAIN;
        UnmountVolume(pvol, FALSE);

        // When UnmountVolume called CloseVolume, CloseVolume froze the volume
        // and set v_bMediaDesc to MAX_VOLUMES.  We set it to zero now to insure
        // that we will re-use the volume without delay.
        pvol->v_bMediaDesc = 0;

        if (pvol->v_flags & VOLF_FROZEN) {
            pvol->v_pdsk->pVol = pvol;
            pvol->v_flags &= ~VOLF_FROZEN;
        }    

#ifndef DEBUG
        MountDisk(hdsk, NULL, pvol->v_flags);
#else
        VERIFYTRUE(MountDisk(hdsk, NULL, pvol->v_flags));
#endif        

        // IMPORTANT NOTE: If the VOLF_FROZEN bit is *not* clear, then it's
        // possible that MountDisk (which calls MountVolume) may have recycled
        // the *wrong* volume (or allocated a completely new one), which means
        // that we may have just leaked a VOLUME;  at the very least, it probably
        // means our current pvol pointer is stale.

// TODO: Yadhug
//        ASSERT(!(pvol->v_flags & VOLF_FROZEN));

        // Restore the REMOUNTED and RECYCLED bits to their former glory

        pvol->v_flags &= ~(VOLF_REMOUNTED | VOLF_RECYCLED);
        pvol->v_flags |= dwOldFlags & (VOLF_REMOUNTED | VOLF_RECYCLED);

        LeaveCriticalSection(&csFATFS);

#if 0 






        if (pvol->v_volID > INVALID_AFS) {
            DEREGISTERAFS(pvol);
            pvol->v_volID = REGISTERAFS(pvol, pvol->v_volID);
        }
#endif        
    }
}


/*  OpenVolume - Allocate a VOLUME structure and validate volume
 *
 *  ENTRY
 *      pdsk -> DSK structure
 *      ppi -> partition info, if any
 *      ppbgbs - pointer to address of PBR (partition boot record) for volume
 *      pstmParent - pointer to parent stream (only if MiniFAT volume)
 *
 *  EXIT
 *      pointer to new VOLUME structure, or NULL if we couldn't make one
 */

PVOLUME OpenVolume(PDSK pdsk, PPARTINFO ppi, PBIGFATBOOTSEC *ppbgbs, PDSTREAM pstmParent)
{
    extern CONST WCHAR awcFlags[]; // From apis.c
    extern CONST WCHAR awcUpdateAccess[]; // From apis.c
    extern CONST WCHAR awcCodePage[]; // From apis.c
    PVOLUME pvol;

    ASSERT(pdsk->d_hdsk != INVALID_HANDLE_VALUE);

    EnterCriticalSection(&csFATFS);

    // Find a reusable VOLUME or allocate a new one.

    pvol = FindVolume(pdsk, *ppbgbs);
    if (!pvol)
        goto exit;

    EnterCriticalSection(&pvol->v_cs);

    pvol->v_ppi = ppi;  // record the associated partition info (NULL if non-partitioned)
    if (ppi)            // and associate the volume with the partition info, if any exists
        ppi->pi_pvol = pvol;

    pvol->v_flags &= ~(VOLF_FROZEN | VOLF_UNCERTAIN | VOLF_DIRTY_WARN);
    
    pvol->v_flFATFS = FATFS_REGISTRY_FLAGS;    
    FSDMGR_GetRegistryValue((HDSK)pdsk->d_hdsk, awcFlags, &pvol->v_flFATFS);  

    FSDMGR_GetRegistryFlag((HDSK)pdsk->d_hdsk, awcUpdateAccess, &pvol->v_flFATFS, FATFS_UPDATE_ACCESS);

    DEBUGMSGW(ZONE_INIT,(DBGTEXTW("FATFS!OpenVolume: access time updates %s\n"), pvol->v_flFATFS & FATFS_UPDATE_ACCESS? TEXTW("enabled") : TEXTW("disabled")));
    DEBUGMSGW(ZONE_INIT,(DBGTEXTW("FATFS!OpenVolume: event logging %s\n"), pvol->v_flFATFS & FATFS_DISABLE_LOG? TEXTW("disabled") : TEXTW("enabled")));
    DEBUGMSGW(ZONE_INIT,(DBGTEXTW("FATFS!OpenVolume: automatic scanning %s\n"), pvol->v_flFATFS & FATFS_DISABLE_AUTOSCAN? TEXTW("disabled") : TEXTW("enabled")));
    DEBUGMSGW(ZONE_INIT,(DBGTEXTW("FATFS!OpenVolume: write verify %s\n"), pvol->v_flFATFS & FATFS_VERIFY_WRITES? TEXTW("enabled") : (pvol->v_flFATFS & FATFS_DISABLE_AUTOSCAN? TEXTW("disabled") : TEXTW("enabled on first 3 writes"))));
    DEBUGMSGW(ZONE_INIT,(DBGTEXTW("FATFS!OpenVolume: extra FAT on format %s\n"), pvol->v_flFATFS & FATFS_ENABLE_BACKUP_FAT? TEXTW("enabled") : TEXTW("disabled")));
    DEBUGMSGW(ZONE_INIT,(DBGTEXTW("FATFS!OpenVolume: force write through %s\n"), pvol->v_flFATFS & FATFS_FORCE_WRITETHROUGH? TEXTW("enabled") : TEXTW("disabled")));

    if (!FSDMGR_GetRegistryValue((HDSK)pdsk->d_hdsk, awcCodePage, &pvol->v_nCodePage)) {
        pvol->v_nCodePage = CP_OEMCP;
    }
    DEBUGMSG( ZONE_INIT,(DBGTEXTW("FATFS!OpenVolume: Codepage = %ld\r\n"), pvol->v_nCodePage));

    // Get the number of path cache entries from the registry
    if (!FSDMGR_GetRegistryValue((HDSK)pdsk->d_hdsk, awcPathCacheEntries, &pvol->v_cMaxCaches)) {
        pvol->v_cMaxCaches = MAX_CACHE_PER_VOLUME;
    }
    DEBUGMSG( ZONE_INIT,(DBGTEXTW("FATFS!OpenVolume: Number of path cache entries = %ld\r\n"), pvol->v_cMaxCaches));

    // Initialize the rest of the VOLUME structure now.  This doesn't perform
    // any I/O, and will return an error only if there is a problem allocating
    // memory for either the FAT or root directory streams.

    if (!InitVolume(pvol, *ppbgbs)) {
        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!OpenVolume: InitVolume failed, volume not opened!\n")));
        CloseVolume(pvol, NULL);
        pvol = NULL;
        goto exit;
    }

    // Now make sure the volume is valid.  This will set the INVALID bit if
    // it isn't, but we will still register/mount the volume so that it can be
    // made valid (ie, formatted) later.

    TestVolume(pvol, ppbgbs);

  exit:
    LeaveCriticalSection(&csFATFS);
    return pvol;
}


/*  CloseVolume - Free a VOLUME structure (if no open handles)
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      pwsVolName - pointer to volume name buffer (assumed to be MAX_PATH)
 *
 *  EXIT
 *      TRUE if freed, FALSE if not.  If not freed, then at a minimum,
 *      the disk handle is invalidated and the volume is marked FROZEN.
 *
 *  NOTES
 *      The volume's critical section should be held by the caller,
 *      and is the caller's responsibility to unhold if this returns FALSE
 *      (ie, volume not freed).
 */

BOOL CloseVolume(PVOLUME pvol, PWSTR pwsVolName)
{
    BOOL fSuccess = TRUE;
    PDSTREAM pstm, pstmEnd;
    PFHANDLE pfh, pfhEnd;

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!CloseVolume(0x%x)\n"), pvol));

    ASSERT(pvol);

    ASSERT(OWNCRITICALSECTION(&csFATFS));
    ASSERT(OWNCRITICALSECTION(&pvol->v_cs));

#ifdef PATH_CACHING
    // We destroy the path cache first, releasing any streams
    // that it may have been holding onto, so that our assertion
    // checks below don't give us false fits.

    PathCacheDestroyAll(pvol);
#endif

    // Walk the open stream list, and for each open stream, walk
    // the open handle list, and for each open handle, mark it as
    // unmounted.  Streams with no open handles are immediately
    // closed;  the only streams that should fall into that category
    // are the special streams for the FAT and root directory.

    EnterCriticalSection(&pvol->v_csStms);

    pstm = pvol->v_dlOpenStreams.pstmNext;
    pstmEnd = (PDSTREAM)&pvol->v_dlOpenStreams;

    while (pstm != pstmEnd) {
        pstm->s_flags &= ~STF_VISITED;
        pstm = pstm->s_dlOpenStreams.pstmNext;
    }

  restart:
    pstm = pvol->v_dlOpenStreams.pstmNext;
    while (pstm != pstmEnd) {

        if (pstm->s_flags & STF_VISITED) {
            pstm = pstm->s_dlOpenStreams.pstmNext;
            continue;
        }

        pstm->s_flags |= STF_VISITED;

        // Add a ref to insure that the stream can't go away once we
        // let go of the volume's critical section.

        pstm->s_refs++;

        LeaveCriticalSection(&pvol->v_csStms);
        EnterCriticalSection(&pstm->s_cs);

        // Try to commit the stream before we unmount it, because we may
        // never be able to remount this stream again, and CommitStream
        // will refuse to commit a stream that's unmounted (and rightly so).

        CommitStream(pstm, TRUE);

        pfh = pstm->s_dlOpenHandles.pfhNext;
        pfhEnd = (PFHANDLE)&pstm->s_dlOpenHandles;

        if (pfh == pfhEnd) {

            if (pstm->s_refs == 2) {
                if (pstm == pvol->v_pstmFAT) {
                    pstm->s_refs--;
                    pvol->v_pstmFAT = NULL;
                }
                else if (pstm == pvol->v_pstmRoot) {
                    pstm->s_refs--;
                    pvol->v_pstmRoot = NULL;
                }
            }
        }
        else {

            // An open handle exists, but free the volume anyway since 
            // this is most likely due to removing removable media

            // Note that we print # of refs less 1, because one of the
            // refs is temporary (ie, it has been added by this function).

            DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!CloseVolume: stream %.11hs still open (%d refs)\n"), pstm->s_achOEM, pstm->s_refs-1));

            // Since a stream's current buffer should be neither manipulated
            // nor held after the release of its critical section, and since we
            // own that critical section now, assert that the current buffer
            // is no longer held;  this means that the current buffer pointer
            // serves only as a "cache hint" now, and since we are about to call
            // FreeBufferPool, we need to invalidate that hint now, too.

            ASSERT(!(pstm->s_flags & STF_BUFCURHELD));
            pstm->s_pbufCur = NULL;

            while (pfh != pfhEnd) {
                pfh->fh_flags |= FHF_UNMOUNTED;
                pfh = pfh->fh_dlOpenHandles.pfhNext;
            }
        }

        pstm->s_flags |= STF_UNMOUNTED;

        CloseStream(pstm);
        EnterCriticalSection(&pvol->v_csStms);
        goto restart;
    }

    LeaveCriticalSection(&pvol->v_csStms);

    // Free any other memory associated with the volume.  The only
    // memory remaining (if ANY) should be the VOLUME structure and one
    // or more open streams hanging off it.  Also, before the registered name
    // of the volume goes away, save a copy if the caller wants it (like for
    // error reporting or something....)  We start at "+1" to avoid copying
    // the leading backslash.

    if (pwsVolName) {
        *pwsVolName = '\0';
        if (pvol->v_pwsHostRoot)
            wcscpy(pwsVolName, pvol->v_pwsHostRoot+1);
    }

    if (!(pvol->v_flags & VOLF_FORMATTING) && !(pvol->v_flags & VOLF_SCANNING))
        DeregisterVolume(pvol);

    if (!FreeBufferPool(pvol))
        fSuccess = FALSE;

#ifdef TFAT
    if (pvol->v_fTfat) {
        // Free diry sector bit array
        if(pvol->v_DirtySectorsInFAT.lpBits)
        {
            LocalFree (pvol->v_DirtySectorsInFAT.lpBits);
            pvol->v_DirtySectorsInFAT.lpBits = NULL;
        }

        if(pvol->v_ClusBuf)
        {
            LocalFree(pvol->v_ClusBuf);
            pvol->v_ClusBuf = NULL;
        }

        if (pvol->v_pFrozenClusterList)
        {
            VirtualFree (pvol->v_pFrozenClusterList, 0, MEM_RELEASE);
            pvol->v_pFrozenClusterList = NULL;
        }
    }
#endif    

#ifdef DISK_CACHING
    pvol->v_CacheSize = 0;
    if (pvol->v_pFATCacheBuffer) {
        VirtualFree( pvol->v_pFATCacheBuffer, 0, MEM_RELEASE);
        pvol->v_pFATCacheBuffer = NULL; 
    }
    if (pvol->v_pFATCacheLookup) {
        HeapFree( GetProcessHeap(), 0, pvol->v_pFATCacheLookup);
        pvol->v_pFATCacheLookup = NULL;
    }    
#endif

    // If this volume is associated with partition information, kill that association now

    if (pvol->v_ppi) {
        pvol->v_ppi->pi_pvol = NULL;
        pvol->v_ppi = NULL;
    }

    if (fSuccess) {

        // If we already tried once to close this volume and failed,
        // leave it allocated but frozen until the volume is remounted
        // or recycled, lest we mess up volume bookkeeping in
        // external components.  

        fSuccess =  !(pvol->v_flags & (VOLF_FROZEN | VOLF_RETAIN));

        if (fSuccess) {
#ifdef DISK_CACHING
            DEBUGFREE(DEBUGALLOC_CS);
            DeleteCriticalSection(&pvol->v_csFATCache);
#endif
#ifdef PATH_CACHING
            DEBUGFREE(DEBUGALLOC_CS);
            DeleteCriticalSection(&pvol->v_csCaches);
#endif
            DEBUGFREE(DEBUGALLOC_CS);
            DeleteCriticalSection(&pvol->v_csStms);
            DEBUGFREE(DEBUGALLOC_CS);
            LeaveCriticalSection(&pvol->v_cs);  // DeleteCriticalSection is picky about this...
            DeleteCriticalSection(&pvol->v_cs);
            DEBUGFREE(sizeof(VOLUME));
            VERIFYNULL(LocalFree((HLOCAL)pvol));
        }
        else {
            DEBUGMSGW(ZONE_INIT,(DBGTEXTW("FATFS!CloseVolume: retaining volume 0x%08x (%s)\n"), pvol, pvol->v_flags & VOLF_FROZEN? TEXTW("previously frozen") : TEXTW("following power cycle")));
            if (!(pvol->v_flags & VOLF_FROZEN)) {
                pvol->v_bMediaDesc = MAX_VOLUMES;
                pvol->v_flags |= VOLF_FROZEN;
            }
        }
    }
    else {
        DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!CloseVolume: retaining volume 0x%08x (open files add/or dirty buffers)\n"), pvol));
        if (!(pvol->v_flags & VOLF_FROZEN)) {
            pvol->v_bMediaDesc = MAX_VOLUMES;   // overload bMediaDesc as a recycle skip count
            pvol->v_flags |= VOLF_FROZEN;
        }
    }
    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!CloseVolume returned 0x%x\n"), fSuccess));
    return fSuccess;
}


/*  QueryVolumeParameters - Query volume parameters
 *
 *  ENTRY
 *      pvol - pointer VOLUME structure
 *      pdevpb - pointer to DOS-style device parameter block
 *      fVolume - TRUE to return volume info, FALSE for device info
 *
 *  EXIT
 *      ERROR_SUCCESS if successful, error code if not
 */

void QueryVolumeParameters(PVOLUME pvol, PDEVPB pdevpb, BOOL fVolume)
{
    // Caller wants a DEVPB.  Give him a close approximation of one.

    memset(pdevpb, 0, sizeof(DEVPB));

#ifdef OLD_CODE
    // In order to allow invalid volumes inside valid partitions
    // to be reformatted, I'm going to leave fVolume alone now. -JTP

    if (pvol->v_flags & VOLF_INVALID)
        fVolume = FALSE;
#endif

    // We'll use this bMediaType field to tell the caller what
    // kind of disk this volume resides on.  Callers that want to
    // reformat volumes need to know that.

    if (pvol->v_pdsk->d_diActive.di_flags & DISK_INFO_FLAG_MBR) {
        pdevpb->DevPrm.OldDevPB.bMediaType = MEDIA_HD;
    }

    pdevpb->DevPrm.OldDevPB.BPB.BPB_BytesPerSector = (WORD)(pvol->v_pdsk->d_diActive.di_bytes_per_sect);

    if (fVolume) {
        pdevpb->DevPrm.OldDevPB.BPB.BPB_SectorsPerCluster   = (BYTE)(1 << (pvol->v_log2cblkClus - pvol->v_log2cblkSec));
        pdevpb->DevPrm.OldDevPB.BPB.BPB_HiddenSectors       = pvol->v_secVolBias;
        pdevpb->DevPrm.OldDevPB.BPB.BPB_ReservedSectors     = (WORD)(pvol->v_secBlkBias - pvol->v_secVolBias);
        if (pvol->v_csecFAT)
            pdevpb->DevPrm.OldDevPB.BPB.BPB_NumberOfFATs    = (BYTE)(pvol->v_secEndAllFATs / pvol->v_csecFAT);
        if (pvol->v_pstmRoot)
            pdevpb->DevPrm.OldDevPB.BPB.BPB_RootEntries     = (WORD)(pvol->v_pstmRoot->s_size / sizeof(DIRENTRY));
        pdevpb->DevPrm.OldDevPB.BPB.BPB_MediaDescriptor     = pvol->v_bMediaDesc;
        pdevpb->DevPrm.OldDevPB.BPB.BPB_SectorsPerFAT       = (WORD)pvol->v_csecFAT;
    }

    pdevpb->DevPrm.OldDevPB.BPB.BPB_SectorsPerTrack = (WORD)pvol->v_pdsk->d_diActive.di_sectors;
    pdevpb->DevPrm.OldDevPB.BPB.BPB_Heads = (WORD)pvol->v_pdsk->d_diActive.di_heads;
    pdevpb->DevPrm.OldDevPB.BPB.BPB_BigTotalSectors = fVolume? pvol->v_csecTotal + pdevpb->DevPrm.OldDevPB.BPB.BPB_ReservedSectors : pvol->v_pdsk->d_diActive.di_total_sectors;
}


/*  RegisterVolume - Register VOLUME in file system
 *
 *  ENTRY
 *      pvol -> VOLUME
 *
 *  EXIT
 *      TRUE if volume successfully registered, FALSE if not
 */

WCHAR awcFolder[] = TEXTW("Mounted Volume");

BOOL RegisterVolume(PVOLUME pvol)
{
    WCHAR szName[MAX_PATH];
    DWORD dwAvail;
    BOOL fSuccess = FALSE;
    DWORD dwValue;

    wcscpy( szName, L"");
    if (FSDMGR_GetRegistryValue((HDSK)pvol->v_pdsk->d_hdsk, L"MountLabel", &dwValue) && (dwValue == 1)) {
        FSDMGR_GetDiskName( (HDSK)pvol->v_pdsk->d_hdsk, szName);
    } 
    if (wcslen(szName) == 0) {
        if (!FSDMGR_DiskIoControl((HDSK)pvol->v_pdsk->d_hdsk, DISK_IOCTL_GETNAME, NULL, 0, (LPVOID)szName, sizeof(szName), &dwAvail, NULL)) { 
            wcscpy( szName, awcFolder);
        }
    }    
    
    if (pvol->v_hVol = FSDMGR_RegisterVolume((HDSK)pvol->v_pdsk->d_hdsk, szName, (DWORD)pvol)) {
        pvol->v_volID = 1;
        if (!pvol->v_pwsHostRoot) {
            pvol->v_pwsHostRoot = LocalAlloc( LMEM_FIXED, MAX_PATH * sizeof(WCHAR));
        }    
        if (pvol->v_pwsHostRoot) {
            FSDMGR_GetVolumeName(pvol->v_hVol, pvol->v_pwsHostRoot, MAX_PATH);
            pvol->v_cwsHostRoot = wcslen(pvol->v_pwsHostRoot);
        } else {
            pvol->v_cwsHostRoot = 0;
        }    
        fSuccess = TRUE;
    }    
    return fSuccess;
}


/*  DeregisterVolume - Deregister VOLUME in file system
 *
 *  ENTRY
 *      pvol -> VOLUME
 *
 *  EXIT
 *      None
 */

void DeregisterVolume(PVOLUME pvol)
{
    if (pvol->v_hVol && (pvol->v_volID != INVALID_AFS)) {
        FSDMGR_DeregisterVolume(pvol->v_hVol);
        pvol->v_hVol = 0;
        pvol->v_volID = INVALID_AFS;
    }    
    if (pvol->v_pwsHostRoot) {
        FSDMGR_AdvertiseInterface( &FATFS_MOUNT_GUID, pvol->v_pwsHostRoot, FALSE);
        LocalFree( pvol->v_pwsHostRoot);
        pvol->v_pwsHostRoot = NULL;
    }
}

/*  MountVolume - Mount volume on specified disk
 *
 *  ENTRY
 *      pdsk -> DSK structure for disk
 *      pbSector -> BIGFATBOOTSEC structure (from disk)
 *      ppi -> PI structure containing partition information
 *      flVol == initial volume flags (currently, only VOLF_READONLY is copied)
 *
 *  EXIT
 *      Pointer to VOLUME structure, NULL if error (eg, out of memory,
 *      disk error, etc)
 */

PVOLUME MountVolume(PDSK pdsk, PBIGFATBOOTSEC *ppbgbs, PPARTINFO ppi, DWORD flVol)
{
    PVOLUME pvol;

    ASSERT(OWNCRITICALSECTION(&csFATFS));

    if (pvol = OpenVolume(pdsk, ppi, ppbgbs, NULL)) {

        pvol->v_flags &= ~VOLF_READONLY;
        pvol->v_flags |= (flVol & VOLF_READONLY);
        if ((pvol->v_flags & VOLF_INVALID) && ppi != NULL) {

            // For invalid volumes, we still know something about the start
            // and extent of the volume, based on the partition table.  So we
            // will record that info in the VOLUME structure for future reformats.

            pvol->v_secVolBias = ppi->pi_secAbsolute;
            pvol->v_csecTotal = ppi->pi_PartEntry.Part_TotalSectors;
            pvol->v_secBlkBias = pvol->v_secVolBias;
        }

#ifdef LATER    

        // Although we call FreeBufferPool inside CloseVolume, we call
        // AllocBufferPool outside OpenVolume so that we can free the sector
        // buffer first and avoid spiking the system's memory requirements
        // any more than necessary.

        DEBUGFREE(pvol->v_pdsk->d_diActive.di_bytes_per_sect);
        VERIFYNULL(LocalFree((HLOCAL)pbSector));
#endif

        if (AllocBufferPool(pvol)) {

            // If this is a remounted volume, refresh as many of its open handles
            // as possible.  We do this even for INVALID volumes, because there
            // may still be VOLUME-based handles, which can always be refreshed.
            // Also, this has to be deferred until after AllocBufferPool, because we
            // can't verify what state the streams are in without buffers to work with.

            RefreshVolume(pvol);

            // We must not be holding the VOLUME's critical section around the FILESYS
            // calls made by RegisterVolume, because those calls take FILESYS' critical
            // section, and if another FILESYS thread issues a power-off notification
            // at the wrong time (which also holds FILESYS's critical section),
            // our notification function (FAT_Notify) will hang attempting to take
            // the VOLUME's critical section in order to enumerate all the file handles
            // and flush them.

            // Since we're still holding onto csFATFS, and we haven't cleared the
            // volume's UNMOUNTED bit yet, it should be OK to let go of the VOLUME's
            // critical section now.

            LeaveCriticalSection(&pvol->v_cs);

            if (pvol->v_flags & (VOLF_SCANNING|VOLF_FORMATTING|VOLF_INVALID)) 
            {
                pvol->v_flags &= ~VOLF_UNMOUNTED;

                // We set VOLF_INITCOMPLETE a little prematurely in order
                // to avoid a nested call to MountVolume redundantly taking
                // this path.  A nested call can occur in several ways;  for
                // example, if ScanVolume or CheckUnformattedVolume modified
                // the volume, they would generate a remount as soon as they
                // unlocked the volume.

                pvol->v_flags |= VOLF_INITCOMPLETE;

                // Do all the rest of our once-in-a-volume-lifetime bit
                // twiddling before diving into the first sections of code
                // that might care about them (some of them anyway).  For
                // example, turning on write verifies before we get to ScanVolume
                // and any attempts it might make to repair a bad disk is probably
                // a good idea....

                if (pvol->v_flFATFS & FATFS_UPDATE_ACCESS)
                    pvol->v_flags |= VOLF_UPDATE_ACCESS;

                if (!(pvol->v_flFATFS & FATFS_DISABLE_AUTOSCAN))
                    pvol->v_bVerifyCount = 3;       

                if (pvol->v_flFATFS & FATFS_VERIFY_WRITES)
                    pvol->v_bVerifyCount = 255;     // set magic value that is never decremented

                // Check for unformatted (or unsupported) volumes and volunteer to format them

                if ((pvol->v_flags & (VOLF_INVALID|VOLF_FORMATTING|VOLF_READONLY)) == VOLF_INVALID) {
                    if (!(pvol->v_flFATFS & FATFS_DISABLE_AUTOFORMAT)) 
                        // If the volume is invalid, read/write, and a format is not already in progress...
#ifdef USE_FATUTIL
                        FormatVolume (pvol, NULL);
#else   
                        CheckUnformattedVolume(pvol);
#endif                        
                }
                // This is a recursive call to MountVolume, so just return
                else if (pvol->v_flags & (VOLF_SCANNING|VOLF_FORMATTING)) {
                    return pvol;
                }

            }

            if (!(pvol->v_flags & (VOLF_SCANNING|VOLF_FORMATTING|VOLF_INVALID)))  {
                // Check for errors in the volume's file system, now that
                // we've taken care of any potential roll-backs.
                if (!(pvol->v_flFATFS & FATFS_DISABLE_AUTOSCAN))
                {
#ifdef USE_FATUTIL
                    ScanVolume(pvol, 0);    
#else
                    ScanVolume(pvol, SCANVOL_QUICK, ScanFixInteractive, NULL);
#endif
                }

#ifdef TFAT
                if (pvol->v_fTfat && !(pvol->v_flags & VOLF_32BIT_FAT) && !(pvol->v_flFATFS & FATFS_DISABLE_TFAT_REDIR))
                {
                    // If this is FAT12 or FAT16, is a TFAT volume, and the redirect flag has not been disabled, then create a hidden
                    // root directory, if not yet created, to redirect to so that root directory operations will be transaction safe.
                    if (FAT_CreateDirectoryW(pvol, HIDDEN_TFAT_ROOT_DIR, NULL) || (GetLastError() == ERROR_ALREADY_EXISTS))
                        pvol->v_flags |= VOLF_TFAT_REDIR_ROOT;
                }
#endif

                if (RegisterVolume(pvol)) {
                    if (pvol->v_pwsHostRoot)
                        FSDMGR_AdvertiseInterface( &FATFS_MOUNT_GUID, pvol->v_pwsHostRoot, TRUE);
                    pvol->v_flags &= ~VOLF_UNMOUNTED;
                    return pvol;
                } 
                else {
                    DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!MountVolume: unable to register file system (%d)\n"), GetLastError()));
                }
            }
        }
        else
            DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!MountVolume: unable to allocate buffer pool\n")));

        // Free the volume we just allocated if we can't allocate enough
        // buffers or there's a problem registering with the host file system.

        EnterCriticalSection(&pvol->v_cs);

        VERIFYTRUE(CloseVolume(pvol, NULL));
    }
    return NULL;
}


/*  UnmountVolume - Unmount the specified volume
 *
 *  ENTRY
 *      pvol -> VOLUME structure
 *      fFrozen == TRUE to unmount frozen volumes only
 *
 *  EXIT
 *      TRUE if volume was successfully unmounted, FALSE if not
 */

BOOL UnmountVolume(PVOLUME pvol, BOOL fFrozen)
{
    BOOL fDirty = FALSE;
    BOOL fSuccess = TRUE;
    WCHAR wsVolName[MAX_PATH];


    if (fFrozen && !(pvol->v_flags & VOLF_FROZEN)) {
        fSuccess = FALSE;
        goto exit;
    }

    EnterCriticalSection(&csFATFS);
    EnterCriticalSection(&pvol->v_cs);

    if (fFrozen) {

        // Clear the FROZEN and RETAIN flags, so that CloseVolume
        // retains the volume only if it must (ie, open files and/or
        // dirty buffers).

        pvol->v_flags &= ~(VOLF_FROZEN | VOLF_RETAIN);
    }
    else {

        pvol->v_flags |= VOLF_UNMOUNTED;
        pvol->v_flags &= ~(VOLF_REMOUNTED | VOLF_RECYCLED);

        // If we're being detached, then we must *force* closure

        if (cLoads == 0) {

            DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!UnmountVolume: volume 0x%08x being forcibly closed!\n"), pvol));
            FAT_CloseAllFiles(pvol, NULL);

            // Clear the FROZEN and RETAIN flags, and set the CLOSING flag,
            // so that CloseVolume does not retain *anything* for this volume.

            pvol->v_flags &= ~(VOLF_FROZEN | VOLF_RETAIN);
            pvol->v_flags |= VOLF_CLOSING;
        }
    }

    if (!CloseVolume(pvol, wsVolName)) {

        LeaveCriticalSection(&pvol->v_cs);

        // Since the CloseVolume wasn't successful, it should
        // have at least set the FROZEN bit.

        ASSERT(pvol->v_flags & VOLF_FROZEN);

        // If the volume has dirty data, and this unmount is
        // not the result of a power-on (ie, VOLF_RETAIN is NOT set),
        // then warn the user.  For safety's sake, we defer the actual
        // warning until we're all done (and outside all our critical
        // sections).

        if ((pvol->v_flags & (VOLF_DIRTY | VOLF_RETAIN)) == VOLF_DIRTY) {
            DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!UnmountVolume: volume 0x%08x is dirty, reinsert card or lose data!\n"), pvol));
            fDirty = TRUE;
        }

        pvol->v_flags &= ~VOLF_RETAIN;
        fSuccess = FALSE;
    }

    LeaveCriticalSection(&csFATFS);

  exit:
    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!UnmountVolume returned %d\n"), fSuccess));
    return fSuccess;
}


/*  CheckCompressedVolume - Check for compressed volume files (CVFs)
 *
 *  Checks for compressed volume files, and if any exist, advises the user
 *  that he will not be able to see any of the files on the compressed portion
 *  of the volume (because we do not currently support any types of CVFs).
 *
 *  ENTRY
 *      pvol - pointer VOLUME structure
 *
 *  EXIT
 *      None
 */

CONST WCHAR awcCVF1[] = TEXTW("DBLSPACE.0??");
CONST WCHAR awcCVF2[] = TEXTW("DRVSPACE.0??");
CONST WCHAR awcCVF3[] = TEXTW("STACVOL.*");

CONST PCWSTR apwsCVFNames[] = {awcCVF1, awcCVF2, awcCVF3};


/*  FormatVolume - Calls into FSDMGR to format volume
 *
 *
 *  ENTRY
 *      pvol - pointer VOLUME structure
 *      pfv  - pointer to format volume options, NULL if not provided
 *
 *  EXIT
 *      Error code.  ERROR_SUCCESS on success.
 */
#ifdef USE_FATUTIL
DWORD FormatVolume(PVOLUME pvol, PFMTVOLREQ pfv)
{
    DWORD dwError = ERROR_SUCCESS;
    BOOL fFormatTfat = FALSE;
    FORMAT_PARAMS fp;
    HDSK hDsk = (HDSK)pvol->v_pdsk->d_hdsk;

    if (pvol->v_flFATFS & FATFS_DISABLE_FORMAT)
        return ERROR_INVALID_FUNCTION;
    
    FSDMGR_GetRegistryValue(hDsk, L"FormatTfat", &fFormatTfat);
    fp.cbSize = sizeof(fp);
    fp.fo.dwClusSize = 0;
    fp.fo.dwRootEntries = DEFAULT_ROOT_ENTRIES;
    fp.fo.dwFatVersion = DEFAULT_FAT_VERSION;
    fp.fo.dwNumFats = (pvol->v_flFATFS & FATFS_ENABLE_BACKUP_FAT) || fFormatTfat ? 2 : 1;
    fp.fo.dwFlags = FATUTIL_DISABLE_MOUNT_CHK;
    fp.pfnProgress = NULL;
    fp.pfnMessage = NULL;
    
    if (fFormatTfat)
        fp.fo.dwFlags |= FATUTIL_FORMAT_TFAT;

    if (pfv) {
        if (pfv->fv_flags & FMTVOL_CLUSTER_SIZE)
            fp.fo.dwClusSize = pfv->fv_csecClus;
        if (pfv->fv_flags & FMTVOL_ROOT_ENTRIES)
            fp.fo.dwRootEntries = pfv->fv_cRootEntries;
        if (pfv->fv_flags & FMTVOL_12BIT_FAT)
            fp.fo.dwFatVersion = 12;
        if (pfv->fv_flags & FMTVOL_16BIT_FAT)
            fp.fo.dwFatVersion = 16;
        if (pfv->fv_flags & FMTVOL_32BIT_FAT)
            fp.fo.dwFatVersion = 32;
        fp.fo.dwNumFats = (pfv->fv_flags & FMTVOL_BACKUP_FAT) ? 2 : 1;
        if ((pfv->fv_flags & FMTVOL_QUICK) == 0)
            fp.fo.dwFlags |= FATUTIL_FULL_FORMAT;
    }

    if (LockVolume(pvol, VOLF_LOCKED) != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_LOGIO || ZONE_ERRORS,(DBGTEXT("FATFS!CheckUnformattedVolume: unable to lock volume\n")));
        return ERROR_GEN_FAILURE;
    }

    pvol->v_flags |= VOLF_MODIFIED;
    pvol->v_flags |= VOLF_FORMATTING;
    dwError = FSDMGR_FormatVolume (hDsk, (LPVOID)&fp);
    UnlockVolume(pvol);
    pvol->v_flags &= ~VOLF_FORMATTING;

    return dwError;
}
#else

/*  CheckUnformattedVolume - Check for unformatted volume
 *
 *  Checks for an unformatted volume, and asks the user if he would like us
 *  to format it for him.
 *
 *  ENTRY
 *      pvol - pointer VOLUME structure
 *
 *  EXIT
 *      TRUE if successfully reformatted, FALSE if not
 */
BOOL CheckUnformattedVolume(PVOLUME pvol)
{
    BOOL fSuccess = FALSE;
    int i;

    // If the volume is invalid, read/write, and a format is not already in progress...

    if ((pvol->v_flags & (VOLF_INVALID|VOLF_FORMATTING|VOLF_READONLY)) == VOLF_INVALID) {

        FATUIDATA fui;

        fui.dwSize = sizeof(fui);
        fui.dwFlags = FATUI_NONE;
        fui.idsEventCaption = IDS_FATUI_WARNING;
        fui.cuiParams = 1;
        fui.auiParams[0].dwType = UIPARAM_VALUE;
        fui.auiParams[0].dwValue = (DWORD)(pvol->v_pwsHostRoot+1);

        if (pvol->v_flags & VOLF_UNSUPPORTED) {

            fui.idsEvent = (pvol->v_flags & VOLF_UNCERTAIN)? IDS_FATUI_UNCERTAIN_WARNING : IDS_FATUI_UNSUPPORTED_WARNING;

#ifdef FATUI
            FATUIEvent(hFATFS, pvol->v_pwsHostRoot+1, &fui);
            if (pvol->v_flags & VOLF_UNCERTAIN) {
                DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!CheckUnformattedVolume: uncertain volume...\n")));
                return FALSE;
            }
#endif            

        }

        fui.dwFlags = FATUI_YES | FATUI_NO;
        fui.cuiParams = 2;
        fui.auiParams[1] = fui.auiParams[0];
        fui.idsEvent = IDS_FATUI_FORMATSTART_WARNING;

#ifdef FATUI
        i = FATUIEvent(hFATFS, pvol->v_pwsHostRoot+1, &fui);
#else        
        i = FATUI_YES;   
#endif        
        if (i == FATUI_YES) {
            FMTVOLREQ fv;

            DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!CheckUnformattedVolume: invalid volume, formatting...\n")));

            fv.fv_flags = FMTVOL_QUICK | ((pvol->v_flFATFS & FATFS_ENABLE_BACKUP_FAT)? FMTVOL_BACKUP_FAT : 0);

            fui.dwFlags = FATUI_NONE;
            fui.cuiParams = 2;
            fui.auiParams[1].dwType = UIPARAM_VALUE;

            // Format the volume now!

            fui.auiParams[1].dwValue = FormatVolume(pvol, &fv);

            // Paranoid refresh of FATUIDATA from the VOLUME structure after formatting...

//            ASSERT(pvol->v_pwsHostRoot && fui.auiParams[0].dwType == UIPARAM_VALUE);
            fui.auiParams[0].dwValue = (DWORD)(pvol->v_pwsHostRoot+1);

            fui.idsEvent = IDS_FATUI_FORMATERROR_WARNING;

            if (fui.auiParams[1].dwValue == ERROR_SUCCESS && !(pvol->v_flags & VOLF_INVALID)) {
                fSuccess = TRUE;
                fui.idsEvent = IDS_FATUI_FORMATDONE_WARNING;
                // Compute total number of Mb on disk using goofy computation to avoid overflow
                fui.auiParams[1].dwValue = ((pvol->v_clusMax-1) * (pvol->v_cbClus/512) + 1023) / 2048;
            }

#ifdef FATUI
            FATUIEvent(hFATFS, pvol->v_pwsHostRoot+1, &fui);
#endif            
        }
    }
    return fSuccess;
}
#endif    

#ifdef USE_FATUTIL

/*  ScanVolume - Calls into FSDMGR to scan volume
 *
 *
 *  ENTRY
 *      pvol - pointer VOLUME structure
 *      dwScanOptions  - scan options
 *
 *  EXIT
 *      Error code.  ERROR_SUCCESS on success.
 */

DWORD ScanVolume(PVOLUME pvol, DWORD dwScanOptions)
{
    DWORD dwError = ERROR_SUCCESS;
    if (!(pvol->v_flags & (VOLF_INVALID | VOLF_SCANNING | VOLF_FORMATTING)))
    {
        SCAN_PARAMS sp;
        HDSK hDsk = (HDSK)pvol->v_pdsk->d_hdsk;
        
        DEBUGMSG(1, (DBGTEXT("FATFS!ScanVolume: Beginning Scan")));

        sp.cbSize = sizeof(sp);
        sp.so.dwFlags = FATUTIL_DISABLE_MOUNT_CHK;
        sp.so.dwFatToUse = 1;
        sp.pfnProgress = NULL;
        sp.pfnMessage = NULL;

        if (dwScanOptions)
            if ((dwScanOptions & SCANVOL_UNATTENDED) == 0)
                sp.so.dwFlags |= FATUTIL_SCAN_VERIFY_FIX;

        if (LockVolume(pvol, VOLF_READLOCKED) == ERROR_SUCCESS) {
            pvol->v_flags |= VOLF_SCANNING;
            pvol->v_flags &= ~VOLF_MODIFIED;
            dwError = FSDMGR_ScanVolume (hDsk, (LPVOID)&sp);

            if (sp.sr.dwTotalErrors > 0)
                pvol->v_flags |= VOLF_MODIFIED;
            
            UnlockVolume (pvol);
            pvol->v_flags &= ~VOLF_SCANNING;
        }
    }
    return dwError;
}
#endif

