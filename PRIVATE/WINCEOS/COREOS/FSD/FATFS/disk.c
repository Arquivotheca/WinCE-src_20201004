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

    disk.c

Abstract:

    This file contains routines for mounting and accessing disks.

Revision History:

--*/

#include "fatfs.h"

#if defined(UNDER_WIN95) && !defined(INCLUDE_FATFS)
#include <pcmd.h>               // for interacting with ReadWriteDisk()
#endif

#ifdef DEBUG
extern DWORD csecWrite, creqWrite;
#endif


DWORD GetDiskInfo(HANDLE hdsk, PDISK_INFO pdi)
{
    DWORD cb;

    if (FSDMGR_DiskIoControl((HDSK)hdsk,
                        IOCTL_DISK_GETINFO,
                        NULL, 0,
                        pdi, sizeof(*pdi),
                        &cb, NULL)) {
        pdi->di_flags |= DISK_INFO_FLAGS_FATFS_NEW_IOCTLS;
    }
    else
    if (FSDMGR_DiskIoControl((HDSK)hdsk,
                        DISK_IOCTL_GETINFO,
                        pdi, sizeof(*pdi),
                        NULL, 0,
                        &cb, NULL)) {
        // Make sure our private flags are never set by the driver
        DEBUGMSG((pdi->di_flags & DISK_INFO_FLAGS_FATFS_NEW_IOCTLS),(DBGTEXT("FATFS!GetDiskInfo: driver is setting reserved bit(s)!\r\n")));
        pdi->di_flags &= ~DISK_INFO_FLAGS_FATFS_NEW_IOCTLS;
    }
    else {
        DWORD status = GetLastError();
        DEBUGMSG(ZONE_DISKIO || ZONE_ERRORS,
                 (DBGTEXT("FATFS!GetDiskInfo: DISK_IOCTL_GETINFO failed %d\r\n"), status));
        return status;
    }

    DEBUGMSG(ZONE_DISKIO,
             (DBGTEXT("FATFS!GetDiskInfo: DISK_IOCTL_GETINFO returned %d bytes\r\n"), cb));

    DEBUGMSG(ZONE_DISKIO,
             (DBGTEXT("FATFS!GetDiskInfo: Sectors=%d (0x%08x), BPS=%d, CHS=%d:%d:%d\r\n"),
              pdi->di_total_sectors, pdi->di_total_sectors, pdi->di_bytes_per_sect, pdi->di_cylinders, pdi->di_heads, pdi->di_sectors));

    return ERROR_SUCCESS;
}


DWORD SetDiskInfo(HANDLE hdsk, PDISK_INFO pdi)
{
    DWORD cb;
    DWORD status = ERROR_SUCCESS;

    if (!(pdi->di_flags & DISK_INFO_FLAGS_FATFS_SIMULATED)) {
        if (FSDMGR_DiskIoControl((HDSK)hdsk,
                            (pdi->di_flags & DISK_INFO_FLAGS_FATFS_NEW_IOCTLS)? IOCTL_DISK_SETINFO : DISK_IOCTL_SETINFO,
                            pdi, sizeof(*pdi),
                            NULL, 0,
                            &cb, NULL) == FALSE) {
            status = GetLastError();

            DEBUGMSG(ZONE_DISKIO || ZONE_ERRORS,
                    (DBGTEXT("FATFS!SetDiskInfo: DISK_IOCTL_SETINFO failed %d\r\n"), status));
        }
        else {
            DEBUGMSG(ZONE_DISKIO,
                    (DBGTEXT("FATFS!SetDiskInfo: DISK_IOCTL_SETINFO returned %d bytes\r\n"), cb));

            DEBUGMSG(ZONE_DISKIO,
                    (DBGTEXT("FATFS!SetDiskInfo: Sectors=%d (0x%08x), BPS=%d, CHS=%d:%d:%d\r\n"),
                    pdi->di_total_sectors, pdi->di_total_sectors, pdi->di_bytes_per_sect, pdi->di_cylinders, pdi->di_heads, pdi->di_sectors));
        }
    }
    return status;
}

#if 0
DWORD NearestPwr2(unsigned int n)
{
    DWORD l=1;
    while((l << 1) <=  n) {
        l = l << 1;
    }
    return l;
}
#endif


void SetupDiskCache(PVOLUME pvol)
{
    DWORD dwFlags = 0;
    DWORD dwCacheSize = 0, dwDataCacheSize = 0, dwFatCacheSize = 0;
    BOOL fEnableCache = FALSE, fFatWarm = FALSE, fDataWarm = FALSE;
    BOOL fWriteBack = FALSE;
    DWORD dwStartFatCache;

    DWORD dwLen = sizeof(DWORD);

    if (!FSDMGR_GetRegistryValue((HDSK)pvol->v_pdsk->d_hdsk, L"EnableCache", &fEnableCache)) {
        fEnableCache = FALSE;
    }
    if (!FSDMGR_GetRegistryValue((HDSK)pvol->v_pdsk->d_hdsk, L"CacheSize", &dwCacheSize)) {
        dwCacheSize = -1;
    }    

    // Keep setting dwCacheSize = -1 as option for disabling cache for backwards compatibility.
    if (!fEnableCache || dwCacheSize == -1) {
        pvol->v_FATCacheId = INVALID_CACHE_ID;
        pvol->v_DataCacheId = INVALID_CACHE_ID;
        return;
    }
   
    FSDMGR_GetRegistryValue((HDSK)pvol->v_pdsk->d_hdsk, L"EnableFatCacheWarm", &fFatWarm);
    FSDMGR_GetRegistryValue((HDSK)pvol->v_pdsk->d_hdsk, L"EnableDataCacheWarm", &fDataWarm);
    if (!fFatWarm) {
        // Allow EnableCacheWarm to also be used to warm FAT cache for backwards compatibility
        FSDMGR_GetRegistryValue((HDSK)pvol->v_pdsk->d_hdsk, L"EnableCacheWarm", &fFatWarm);
    }
    
    FSDMGR_GetRegistryValue((HDSK)pvol->v_pdsk->d_hdsk, L"FatCacheSize", &dwFatCacheSize);
    FSDMGR_GetRegistryValue((HDSK)pvol->v_pdsk->d_hdsk, L"DataCacheSize", &dwDataCacheSize);
    FSDMGR_GetRegistryValue((HDSK)pvol->v_pdsk->d_hdsk, L"EnableWriteBack", &fWriteBack);

    if (!dwFatCacheSize) {
        // If the FAT cache size was set to 0, use the size of the FAT, up to 512 sectors.
        dwFatCacheSize = min( 512, pvol->v_csecFAT);
    }

    if (!dwDataCacheSize) {        
        if (dwCacheSize) {
            // For backwards compatibility, still honor CacheSize reg setting
            dwDataCacheSize = dwCacheSize;
        } else {
            dwDataCacheSize = min (1024, 2 * pvol->v_csecFAT);
        }
    }
   
    // Set up the FAT cache flags
    if (fFatWarm) {
        dwFlags |= CACHE_FLAG_WARM;
    }      
    if (fWriteBack) {
        dwFlags |= CACHE_FLAG_WRITEBACK;
    }

    // Set up the FAT cache.
    dwStartFatCache = pvol->v_secBlkBias;

#ifdef TFAT    
    // For TFAT, only cache FAT1.  FAT0 will be uncached to ensure transactions
    // are committed properly.
    if (pvol->v_fTfat)
        dwStartFatCache += pvol->v_csecFAT; 
#endif

    pvol->v_FATCacheId = FSDMGR_CreateCache ((HDSK)pvol->v_pdsk->d_hdsk, dwStartFatCache, pvol->v_secBlkBias + pvol->v_secEndAllFATs - 1,
         dwFatCacheSize, pvol->v_pdsk->d_diActive.di_bytes_per_sect, dwFlags);

    // Set up the data cache flags
    dwFlags = 0;
    if (fDataWarm) {
        dwFlags |= CACHE_FLAG_WARM;
    }      
    if (fWriteBack) {
        dwFlags |= CACHE_FLAG_WRITEBACK;
    }

    // Set up data cache
    pvol->v_DataCacheId = FSDMGR_CreateCache ((HDSK)pvol->v_pdsk->d_hdsk, pvol->v_secBlkBias + pvol->v_secEndAllFATs, 
        pvol->v_pdsk->d_diActive.di_total_sectors - 1, dwDataCacheSize, pvol->v_pdsk->d_diActive.di_bytes_per_sect, dwFlags);
        
}

/*  ReadWriteDisk
 *
 *  Reads/writes a set of contiguous sectors from a disk to/from memory.
 *
 *  Entry:
 *      pvol        pointer to VOLUME, NULL if none
 *      hdsk        handle to disk
 *      cmd         READ_DISK_CMD, WRITE_DISK_CMD, or WRITETHROUGH_DISK_CMD
 *      pdi         address of DISK_INFO structure
 *      sector      0-based sector number
 *      cSectors    number of sectors to read/write
 *      pvBuffer    address of buffer to read/write sectors to/from
 *
 *  Exit:
 *      ERROR_SUCCESS if successful, else GetLastError() from the FSDMGR_DiskIoControl call issued
 */   
DWORD ReadWriteDisk(PVOLUME pvol, HANDLE hdsk, DWORD cmd, PDISK_INFO pdi, DWORD sector, int cSectors, 
                                          PVOID pvBuffer, BOOL fRemapFats)
{
    DWORD dwError = ERROR_SUCCESS;
    // DWORD dwOffset;
    // DWORD dwSector;
    BOOL  bRWFromDisk = TRUE;
    DWORD dwCacheId;
    
#ifdef TFAT
    // Here is where we are going to redirect any writes to the FAT1 from FAT0;
    if  ( pvol && pvol->v_fTfat && (pvol->v_flags & VOLF_BACKUP_FAT) &&
           IS_FAT_SECTOR(pvol, sector) && fRemapFats )
    {
        if( sector < ( pvol->v_secBlkBias + pvol->v_csecFAT )) 
        {
            // Keep track what FAT sectors are modified
            if (WRITE_DISK_CMD == cmd || WRITETHROUGH_DISK_CMD == cmd)
            {
                DWORD sec; 

                for (sec = sector - pvol->v_secBlkBias; sec < sector + cSectors - pvol->v_secBlkBias; sec++)
                {
                    ASSERT (sec < pvol->v_secEndAllFATs);
                    ASSERT ((sec >> 3) < pvol->v_DirtySectorsInFAT.dwSize);
                    pvol->v_DirtySectorsInFAT.lpBits[ sec >> 3] |= 1 << (sec & 7);
                }
            }

        	// This is a read/write to the FAT0, redirect to FAT1
            DEBUGMSGW( ZONE_TFAT && ZONE_DISKIO, ( DBGTEXTW( "FATFS!ReadWriteDisk: %s to sector %d, rerouting to sector %d\r\n" ),
                ( cmd == READ_DISK_CMD ) ? DBGTEXTW( "READ" ) : DBGTEXTW( "WRITE" ), 
                sector, sector + pvol->v_csecFAT));

            sector += pvol->v_csecFAT;
        }
        else
        {
            DEBUGMSGW( ZONE_TFAT || ZONE_ERRORS, ( DBGTEXTW( "FATFS!ReadWriteDisk: %s to sector %d -- out of range warning\r\n" ),
                ( cmd == READ_DISK_CMD ) ? DBGTEXTW( "READ" ) : DBGTEXTW( "WRITE" ), sector));
            ASSERT(FALSE);
        }
    }
#endif

    if (pvol && sector >= pvol->v_secBlkBias && pvol->v_FATCacheId != INVALID_CACHE_ID && pvol->v_DataCacheId != INVALID_CACHE_ID)
    {
        dwCacheId = pvol->v_DataCacheId;

        if (IS_FAT_SECTOR(pvol, sector))
        {
            dwCacheId = pvol->v_FATCacheId;

#ifdef TFAT
            // If this is a FAT0 sector, write directly to disk
            if (pvol->v_fTfat && sector < ( pvol->v_secBlkBias + pvol->v_csecFAT )) 
                return UncachedReadWriteDisk ((HDSK)hdsk, cmd, pdi, sector, cSectors, pvBuffer);
#endif
        }

        if (cmd == READ_DISK_CMD) 
        {
            dwError = FSDMGR_CachedRead (dwCacheId, sector, cSectors, pvBuffer, 0);
        } 
        else 
        {
            dwError = FSDMGR_CachedWrite (dwCacheId, sector, cSectors, pvBuffer, 
                (cmd == WRITETHROUGH_DISK_CMD) ? CACHE_FORCE_WRITETHROUGH : 0);
        }
    }
    else
    {
        // This is typically used for reading the boot sector, which is not cached, or if caching disabled.
        dwError = UncachedReadWriteDisk ((HDSK)hdsk, cmd, pdi, sector, cSectors, pvBuffer);
    }

    return dwError;
}    


DWORD UncachedReadWriteDisk (HDSK hdsk, DWORD cmd, PDISK_INFO pdi, DWORD sector, int cSectors, PVOID pvBuffer)
{
    DWORD dwError;
    
    if (cmd == READ_DISK_CMD) 
    {
        dwError = FSDMGR_ReadDisk (hdsk, sector, cSectors, pvBuffer, cSectors * pdi->di_bytes_per_sect);
    } 
    else 
    {
        dwError = FSDMGR_WriteDisk (hdsk, sector, cSectors, pvBuffer, cSectors * pdi->di_bytes_per_sect);
    }

    return dwError;
}

     

/*  FreeClusterOnDisk - Calls IOCTL_DISK_DELETE_SECTORS on sectors that are freed
 *
 *  ENTRY
 *      pvol - pointer to volume
 *      dwStartCluster - starting cluster to free
 *      dwNumClusters - number of clusters to free
 *
 *  EXIT
 *      None
 */

BOOL FreeClustersOnDisk (PVOLUME pvol, DWORD dwStartCluster, DWORD dwNumClusters)
{
    DELETE_SECTOR_INFO dsi;
    dsi.cbSize = sizeof(DELETE_SECTOR_INFO);
    dsi.numsectors = (1 << pvol->v_log2csecClus) * dwNumClusters;

    if (pvol->v_pdsk->d_flags & DSKF_SENDDELETE) {

        DWORD dwRet; 
        dsi.startsector = ((1 << pvol->v_log2csecClus) * dwStartCluster) + pvol->v_blkClusBias + pvol->v_secBlkBias;

        if (pvol->v_DataCacheId == INVALID_CACHE_ID) 
        {
            if (!FSDMGR_DiskIoControl((HDSK)pvol->v_pdsk->d_hdsk, 
                            IOCTL_DISK_DELETE_SECTORS, 
                            &dsi, sizeof(DELETE_SECTOR_INFO), 
                            NULL, 0, 
                            &dwRet, 
                            NULL))
            {
                pvol->v_pdsk->d_flags &= ~DSKF_SENDDELETE;
            }
        } 
        else 
        {
            FSDMGR_CacheIoControl(pvol->v_DataCacheId, 
                            IOCTL_DISK_DELETE_SECTORS, 
                            &dsi, sizeof(DELETE_SECTOR_INFO), 
                            NULL, 0, 
                            &dwRet, 
                            NULL);
        }
    }    

    return TRUE;

}


















PDSK FindDisk(HANDLE hdsk, PCWSTR pwsDisk, PDISK_INFO pdi)
{
    DWORD cwDisk;
    PDSK pdsk = dlDisks.pdskNext;

    ASSERT(OWNCRITICALSECTION(&csFATFS));

    cwDisk = 0;
    if (pwsDisk)
        cwDisk = wcslen(pwsDisk)+1;

    while (pdsk && (pdsk != (PDSK)&dlDisks)) {

        if (pdsk->d_hdsk == hdsk) {
            if (pdi)
                ASSERT(memcmp(&pdsk->d_diActive, pdi, sizeof(*pdi)) == 0);
            goto exit;
        }

        if (pdi && (pdsk->d_flags & DSKF_FROZEN)) {

            if (memcmp(&pdsk->d_diActive, pdi, sizeof(*pdi)) == 0) {
                pdsk->d_flags |= DSKF_REMOUNTED;
                DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!FindDisk: remounting frozen disk...\r\n")));
                goto init2;
            }
        }
        pdsk = pdsk->d_dlOpenDisks.pdskNext;
    }

    if (!pdi) {
        pdsk = NULL;
        goto exit;
    }    

    pdsk = (PDSK)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(DSK)-sizeof(pdsk->d_wsName) + cwDisk*sizeof(pwsDisk[0]));
    if (!pdsk)
        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!FindDisk: out of memory!\r\n")));
    else {
        DEBUGALLOC(sizeof(DSK)-sizeof(pdsk->d_wsName) + cwDisk*sizeof(pwsDisk[0]));
        AddItem((PDLINK)&dlDisks, (PDLINK)&pdsk->d_dlOpenDisks);
        // InitList((PDLINK)&pdsk->d_dlPartitions);
        pdsk->d_diActive = *pdi;
init2:
        pdsk->d_cwName = cwDisk;
        if (pwsDisk)
            wcsncpy(pdsk->d_wsName, pwsDisk, cwDisk);
    }

exit:
    return pdsk;
}


/*  MountDisk - Find and mount all volumes on specified disk
 *
 *  ENTRY
 *      hdsk - handle to disk device containing one or more FAT volumes
 *      pwsDisk -> name of disk device, NULL if unknown
 *      flVol - initial volume flags (currently, only VOLF_READONLY is copied)
 *
 *  EXIT
 *      Pointer to DSK structure, NULL if error (eg, out of memory,
 *      unusable disk, etc)
 *
 *  NOTES
 *      In the 1.0 release, if the disk appeared to be a hard disk, I mounted
 *      only the active DOS partition, and if there was *no* active partition,
 *      then I mounted only the *first* DOS partition.  In the current release,
 *      I mount *all* DOS partitions, including "Extended DOS Partitions".
 */

PDSK MountDisk(HANDLE hdsk, PCWSTR pwsDisk, DWORD flVol)
{
    DWORD dwError;
    DISK_INFO di;
    PBYTE pbSector;
    PDSK pdsk = NULL;

   
    if (GetDiskInfo(hdsk, &di) != ERROR_SUCCESS || di.di_bytes_per_sect == 0) {

        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!MountDisk: no valid disk info (%d)\r\n"), GetLastError()));

        di.di_total_sectors = SetFilePointer(hdsk, 0, NULL, FILE_END);
        if (di.di_total_sectors == 0xFFFFFFFF || di.di_total_sectors < DEFAULT_SECTOR_SIZE)
            return NULL;

        di.di_total_sectors /= DEFAULT_SECTOR_SIZE;
        di.di_bytes_per_sect = DEFAULT_SECTOR_SIZE;
        di.di_cylinders = 1;
        di.di_heads = 1;
        di.di_sectors = di.di_total_sectors;
        di.di_flags = DISK_INFO_FLAG_CHS_UNCERTAIN | DISK_INFO_FLAGS_FATFS_SIMULATED;
    }
    else {
        // Make sure our special "simulated" flag is never set by the driver
        DEBUGMSG((di.di_flags & DISK_INFO_FLAGS_FATFS_SIMULATED),(DBGTEXT("FATFS!MountDisk: driver is setting reserved bit(s)!\r\n")));
        di.di_flags &= ~DISK_INFO_FLAGS_FATFS_SIMULATED;
    }
    if (di.di_total_sectors < 16) {
        return NULL;
    }    

    // We ZEROINIT the memory so that if the ReadWriteDisk call fails
    // (perhaps because the media is completely unreadable/unusable at this
    // point), MountVolume won't be spoofed by bogus data in the sector buffer;
    // even though MountVolume won't actually mount anything in that case, it
    // will at least register a volume name that can be subsequently used in
    // format requests.

    pbSector = (PBYTE)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, di.di_bytes_per_sect);
    if (!pbSector) {
        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!MountDisk: out of memory, aborting mount\r\n")));
        return NULL;
    }
    DEBUGALLOC(di.di_bytes_per_sect);

    dwError = ReadWriteDisk(NULL, hdsk, READ_DISK_CMD, &di, 0, 1, pbSector, TRUE);
    DEBUGMSG(ZONE_ERRORS && dwError,(DBGTEXT("FATFS!MountDisk: error reading MBR (%d)\r\n"), dwError));

    EnterCriticalSection(&csFATFS);

    // This is a logical point to assume that we're actually going
    // to succeed in mounting one or more volumes, so let's allocate
    // and initialize a DSK structure for the entire disk (which may
    // contain 0 or more volumes).

    pdsk = FindDisk(hdsk, pwsDisk, &di);
    if (!pdsk)
        goto exit;

    pdsk->d_hdsk = hdsk;
    pdsk->d_flags &= ~DSKF_FROZEN;


    // Since we're mounting non-partitioned media, HiddenSectors should be zero

    if (((PBIGFATBOOTSEC)pbSector)->bgbsBPB.oldBPB.BPB_HiddenSectors != 0) {
        DEBUGMSGW(ZONE_ERRORS,(DBGTEXTW("FATFS!MountDisk: BPB_HiddenSectors(%d) != 0\r\n"), ((PBIGFATBOOTSEC)pbSector)->bgbsBPB.oldBPB.BPB_HiddenSectors));
        ((PBIGFATBOOTSEC)pbSector)->bgbsBPB.oldBPB.BPB_HiddenSectors = 0;
    }
    pdsk->pVol = MountVolume(pdsk, (PBIGFATBOOTSEC *)&pbSector, flVol);

    // If we allocated a DSK structure but we never allocated any VOLUMEs
    // to go along with it, then free the DSK structure, using UnmountDisk.

  exit:
    if (pdsk && !(pdsk->d_flags & (DSKF_REMOUNTED | DSKF_RECYCLED)) && !pdsk->pVol) {

        pdsk->d_hdsk = INVALID_HANDLE_VALUE;    // prevent UnmountDisk from trying to close the disk
        UnmountDisk(pdsk, FALSE);
        pdsk = NULL;
    }

    if (pdsk) {
        // RefreshPartitionInfo(pdsk);
        pdsk->d_flags |= DSKF_SENDDELETE;
    }    

    LeaveCriticalSection(&csFATFS);

    DEBUGFREE(di.di_bytes_per_sect);
    VERIFYTRUE(HeapFree(hHeap, 0, (HLOCAL)pbSector));
    return pdsk;
}




/*  UnmountDisk - Unmount all volumes on specified disk
 *
 *  ENTRY
 *      pdsk -> DSK structure
 *      fFrozen == TRUE to unmount frozen volumes only;
 *      this is used when we're notified that all file system devices
 *      are ON, so we no longer want to retain frozen volumes that no
 *      longer have any open files or dirty data.
 *
 *  EXIT
 *      TRUE if all volumes on disk were unmounted, FALSE if not;  if all
 *      VOLUMEs are automatically freed, then the DSK is freed as well.
 */

BOOL UnmountDisk(PDSK pdsk, BOOL fFrozen)
{
    PVOLUME pvol=pdsk->pVol;
    BOOL fSuccess = TRUE;

    DEBUGMSGW(ZONE_INIT,(DBGTEXTW("FATFS!UnmountDisk: unmounting %s volumes on disk %s\r\n"), fFrozen? TEXTW("frozen") : TEXTW("all"), pdsk->d_wsName));

    EnterCriticalSection(&csFATFS);

    if (pvol) {

        ASSERT(pvol->v_pdsk == pdsk);

        fSuccess = UnmountVolume(pvol, fFrozen);
    }

    // If we're going to free the DSK structure, or the disk
    // is going away anyway, then close the disk handle as well.

    if (fSuccess || !fFrozen) {
        if (pdsk->d_hdsk != INVALID_HANDLE_VALUE) {
            DEBUGFREE(DEBUGALLOC_HANDLE);
            pdsk->d_hdsk = INVALID_HANDLE_VALUE;
        }
        pdsk->d_flags |= DSKF_FROZEN;
        pdsk->d_flags &= ~(DSKF_REMOUNTED | DSKF_RECYCLED);
    }

    if (fSuccess) {
        // FreePartitionInfo(&pdsk->d_dlPartitions);
        RemoveItem((PDLINK)&pdsk->d_dlOpenDisks);
        DEBUGFREE(sizeof(DSK)-sizeof(pdsk->d_wsName) + pdsk->d_cwName*sizeof(pdsk->d_wsName[0]));
        VERIFYTRUE(HeapFree(hHeap, 0, (HLOCAL)pdsk));
    }

    LeaveCriticalSection(&csFATFS);

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!UnmountDisk returned %d\r\n"), fSuccess));
    return fSuccess;
}


/*  UnmountAllDisks - Unmount all disks
 *
 *  ENTRY
 *      fFrozen == TRUE to unmount frozen volumes only;  this is used when
 *      we are notified that all file system devices are ON, so we no longer
 *      want to retain frozen volumes that no longer have any open files or
 *      dirty data.
 *
 *  EXIT
 *      The number of disk devices actually unmounted.
 */

DWORD UnmountAllDisks(BOOL fFrozen)
{
    DWORD cDisks = 0;

    if (dlDisks.pdskNext != (PDSK)&dlDisks) {

        PDSK pdsk;
        EnterCriticalSection(&csFATFS);
        pdsk = dlDisks.pdskNext;

        while (pdsk != (PDSK)&dlDisks) {

            if (UnmountDisk(pdsk, fFrozen)) {

                cDisks++;

                // Since UnmountDisk was successful, the DSK was
                // removed from the list and freed, so start at the top
                // of the list again.

                pdsk = dlDisks.pdskNext;
                continue;
            }
            pdsk = pdsk->d_dlOpenDisks.pdskNext;
        }
        LeaveCriticalSection(&csFATFS);
    }
    return cDisks;
}
