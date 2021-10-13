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

BOOL g_fEnableCache4Way = FALSE;

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
        DEBUGMSG((pdi->di_flags & DISK_INFO_FLAGS_FATFS_NEW_IOCTLS),(DBGTEXT("FATFS!GetDiskInfo: driver is setting reserved bit(s)!\n")));
        pdi->di_flags &= ~DISK_INFO_FLAGS_FATFS_NEW_IOCTLS;
    }
    else {
        DWORD status = GetLastError();
        DEBUGMSG(ZONE_DISKIO || ZONE_ERRORS,
                 (DBGTEXT("FATFS!GetDiskInfo: DISK_IOCTL_GETINFO failed %d\n"), status));
        return status;
    }

    DEBUGMSG(ZONE_DISKIO,
             (DBGTEXT("FATFS!GetDiskInfo: DISK_IOCTL_GETINFO returned %d bytes\n"), cb));

    DEBUGMSG(ZONE_DISKIO,
             (DBGTEXT("FATFS!GetDiskInfo: Sectors=%d (0x%08x), BPS=%d, CHS=%d:%d:%d\n"),
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
                    (DBGTEXT("FATFS!SetDiskInfo: DISK_IOCTL_SETINFO failed %d\n"), status));
        }
        else {
            DEBUGMSG(ZONE_DISKIO,
                    (DBGTEXT("FATFS!SetDiskInfo: DISK_IOCTL_SETINFO returned %d bytes\n"), cb));

            DEBUGMSG(ZONE_DISKIO,
                    (DBGTEXT("FATFS!SetDiskInfo: Sectors=%d (0x%08x), BPS=%d, CHS=%d:%d:%d\n"),
                    pdi->di_total_sectors, pdi->di_total_sectors, pdi->di_bytes_per_sect, pdi->di_cylinders, pdi->di_heads, pdi->di_sectors));
        }
    }
    return status;
}

#ifdef DISK_CACHING

DWORD NearestPwr2(unsigned int n)
{
    DWORD l=1;
    while((l << 1) <=  n) {
        l = l << 1;
    }
    return l;
}

DWORD GetBestCacheSize(PVOLUME pvol)
{
    DEBUGMSG( ZONE_INIT, (TEXT("FATFS: Sec/FAT = %ld!!!\r\n"), pvol->v_csecFAT));
    return min( 512, pvol->v_csecFAT);
}

void SetupDiskCache(PVOLUME pvol)
{
    DWORD dwError;
    DWORD dwStart, dwSector, dwLen;
    DWORD dwWarmCache;
    BOOL fEnableCache = FALSE;

    dwLen = sizeof(DWORD);

    if (!FSDMGR_GetRegistryValue((HDSK)pvol->v_pdsk->d_hdsk, L"CacheSize", &pvol->v_CacheSize)) {
        pvol->v_CacheSize = -1;
    }    
    if (!FSDMGR_GetRegistryValue((HDSK)pvol->v_pdsk->d_hdsk, L"MaxCachedFileSize", &pvol->v_MaxCachedFileSize)) {
        pvol->v_MaxCachedFileSize = -1;
    }    
    if (!FSDMGR_GetRegistryValue((HDSK)pvol->v_pdsk->d_hdsk, L"EnableCache4Way", &g_fEnableCache4Way)) {
        g_fEnableCache4Way = FALSE;
    }    
    if (!FSDMGR_GetRegistryValue((HDSK)pvol->v_pdsk->d_hdsk, L"EnableCache", &fEnableCache)) {
        fEnableCache = FALSE;
    }
    if (!FSDMGR_GetRegistryValue((HDSK)pvol->v_pdsk->d_hdsk, L"EnableCacheWarm", &dwWarmCache)) {
        dwWarmCache = 0;
    }    
    if (fEnableCache && pvol->v_CacheSize != -1) {
        if (!(pvol->v_CacheSize)) {
            pvol->v_CacheSize = GetBestCacheSize( pvol);
        }
        pvol->v_CacheSize = max( NearestPwr2(pvol->v_CacheSize), 16);
        DEBUGMSG( ZONE_INIT, (TEXT("FATFS: CacheSize = %ld!!!\r\n"), pvol->v_CacheSize));
        dwStart = pvol->v_secBlkBias;
        pvol->v_pFATCacheBuffer = VirtualAlloc(NULL, pvol->v_pdsk->d_diActive.di_bytes_per_sect * pvol->v_CacheSize, MEM_COMMIT, PAGE_READWRITE);
        if (!pvol->v_pFATCacheBuffer)
            goto ExitFailure;
        pvol->v_pFATCacheLookup = HeapAlloc( GetProcessHeap(), 0, pvol->v_CacheSize * sizeof(DWORD));
        if (!pvol->v_pFATCacheLookup) 
            goto ExitFailure;

        if(g_fEnableCache4Way){
            pvol->v_pFATCacheLRU = HeapAlloc( GetProcessHeap(), 0, (pvol->v_CacheSize)/4);
            if (!pvol->v_pFATCacheLRU) 
                goto ExitFailure;
            // tentative not supporting pre-warming
            memset( pvol->v_pFATCacheLRU, 0x00, pvol->v_CacheSize/4);
#ifdef ENABLE_FAT0_CACHE
            pvol->v_dwFAT0CacheLookup = 0xFFFFFFFF;
#endif // ENABLE_FAT0_CACHE
#ifdef ENABLE_FAT1_CACHE
            pvol->v_dwFAT1CacheLookup = 0xFFFFFFFF;
#endif // ENABLE_FAT1_CACHE

#ifdef DEBUG
            pvol->dwCacheHit = 0;
            pvol->dwCacheMiss = 0;
#endif
        }

        if (dwWarmCache) {
            DEBUGMSG( ZONE_INIT, (TEXT("FATFS: Warming Cache ...\r\n")));
            dwError = ReadWriteDisk2( pvol, pvol->v_pdsk->d_hdsk, DISK_IOCTL_READ, &pvol->v_pdsk->d_diActive, dwStart, pvol->v_CacheSize, pvol->v_pFATCacheBuffer);                
            if (dwError == ERROR_SUCCESS) {                
                for (dwSector=0; dwSector < pvol->v_CacheSize; dwSector++) { // Prewarm the cache
                    pvol->v_pFATCacheLookup[dwSector] = dwSector;
                }   
            } else {
                memset( pvol->v_pFATCacheLookup, 0xFF, pvol->v_CacheSize * sizeof(DWORD));
            }
        } else {
            // Invalidate all entries
            memset( pvol->v_pFATCacheLookup, 0xFF, pvol->v_CacheSize * sizeof(DWORD));
        }
    } else {
        goto ExitFailure;
    }
    return;
ExitFailure:
    DEBUGMSG( ZONE_INIT, (TEXT("No cache has been setup !!!\r\n")));
    pvol->v_CacheSize = 0;
    if (pvol->v_pFATCacheBuffer) {
        VirtualFree( pvol->v_pFATCacheBuffer, 0, MEM_RELEASE);
        pvol->v_pFATCacheBuffer = NULL;
    }
    if (pvol->v_pFATCacheLookup) {
        HeapFree( GetProcessHeap(), 0, pvol->v_pFATCacheLookup);
        pvol->v_pFATCacheLookup = NULL;
    }    
}

#include "CacheRW.c"

DWORD ReadWriteDisk(PVOLUME pvol, HANDLE hdsk, DWORD cmd, PDISK_INFO pdi, DWORD sector, int cSectors, 
                                          PVOID pvBuffer, BOOL fRemapFats)
{
    DWORD dwError = ERROR_SUCCESS;
    DWORD dwOffset;
    DWORD dwSector;
    BOOL  bRWFromDisk = TRUE;

#ifdef TFAT
    // Here is where we are going to redirect any writes to the FAT1 from FAT0;
    if  ( pvol && pvol->v_fTfat && (pvol->v_flags & VOLF_BACKUP_FAT) &&
        ( sector < ( pvol->v_secBlkBias + pvol->v_secEndAllFATs )) &&
        ( sector >= pvol->v_secBlkBias ) &&
        fRemapFats) 
    {
        if( sector < ( pvol->v_secBlkBias + pvol->v_csecFAT )) 
        {
            // Keep track what FAT sectors are modified
            if (DISK_IOCTL_WRITE == cmd)
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
            DEBUGMSGW( ZONE_TFAT && ZONE_DISKIO, ( DBGTEXTW( "FATFS!ReadWriteDisk: %s to sector %d, rerouting to sector %d\n" ),
                ( cmd == DISK_IOCTL_READ ) ? DBGTEXTW( "READ" ) : DBGTEXTW( "WRITE" ), 
                sector, sector + pvol->v_csecFAT));

            sector += pvol->v_csecFAT;
        }
        else{
            DEBUGMSGW( ZONE_TFAT || ZONE_ERRORS, ( DBGTEXTW( "FATFS!ReadWriteDisk: %s to sector %d -- out of range warning\n" ),
                ( cmd == DISK_IOCTL_READ ) ? DBGTEXTW( "READ" ) : DBGTEXTW( "WRITE" ), sector));
            ASSERT(FALSE);
        }
    }
#endif

    dwOffset = 0;

    if (!pvol || (sector < pvol->v_secBlkBias) || (pvol->v_CacheSize == 0))
        return ReadWriteDisk2( pvol, hdsk, cmd, pdi, sector, cSectors, pvBuffer);

    DEBUGMSG( ZONE_DISKIO, (TEXT("FATFS!ReadWriteDisk:Cached (%s, sector %d (0x%08x), total %d)\n"), cmd==DISK_IOCTL_READ? TEXTW("READ") : TEXTW("WRITE"), sector, sector, cSectors));

    EnterCriticalSection(&pvol->v_csFATCache);

    if(g_fEnableCache4Way){



        if (cmd == DISK_IOCTL_READ) {

            bRWFromDisk = !IsCacheHit(pvol, sector, cSectors);
            if (bRWFromDisk) {
#ifdef DEBUG
            	pvol->dwCacheMiss += cSectors;
#endif
                dwError = ReadWriteDisk2( pvol, hdsk, DISK_IOCTL_READ, pdi, sector, cSectors, pvBuffer);
    			// Fill cache
    			CacheFill(pvol, sector, cSectors, pvBuffer);
            } else {
#ifdef DEBUG
            	pvol->dwCacheHit += cSectors;
#endif
    			// Read from cache
    			CacheRead(pvol, sector, cSectors, pvBuffer);
            }
        } else {
            dwError = ReadWriteDisk2( pvol, hdsk, DISK_IOCTL_WRITE, pdi, sector, cSectors, pvBuffer);
            // Write to cache
    		CacheWrite(pvol, sector, cSectors, pvBuffer);
        }

    } else{
        if (cmd == DISK_IOCTL_READ) {
            bRWFromDisk = FALSE;
            for (dwSector = sector; dwSector < sector+cSectors; dwSector++) {
                dwOffset = dwSector - pvol->v_secBlkBias;
                if (pvol->v_pFATCacheLookup[dwOffset % pvol->v_CacheSize] != dwOffset) {
                    bRWFromDisk = TRUE;
                }
            }
            if (bRWFromDisk) {
                dwError = ReadWriteDisk2( pvol, hdsk, DISK_IOCTL_READ, pdi, sector, cSectors, pvBuffer);
            }    
        } else {
            dwError = ReadWriteDisk2( pvol, hdsk, DISK_IOCTL_WRITE, pdi, sector, cSectors, pvBuffer);
        }

        if (dwError != ERROR_SUCCESS)
            goto exit;
        
        for (dwSector = sector; dwSector < sector+cSectors; dwSector++) {
            dwOffset = dwSector - pvol->v_secBlkBias;
            DEBUGMSG( ZONE_DISKIO, (TEXT("FATFS!ReadWriteDisk:Cached dwSector=%ld dwOffset=%ld Index=%ld\r\n"), dwSector, dwOffset, dwOffset % pvol->v_CacheSize));
            if (cmd == DISK_IOCTL_READ) {
                if (bRWFromDisk) {
                    if (pvol->v_pFATCacheLookup[dwOffset % pvol->v_CacheSize] != dwOffset) {
                        memcpy( pvol->v_pFATCacheBuffer+((dwOffset % pvol->v_CacheSize)*512), (LPBYTE)pvBuffer+((dwSector-sector)*512),  512);
                    }    
                } else {
                    memcpy( (LPBYTE)pvBuffer+((dwSector-sector)*512),  pvol->v_pFATCacheBuffer+((dwOffset % pvol->v_CacheSize)*512), 512);
                }
            } else {
                memcpy( pvol->v_pFATCacheBuffer+((dwOffset % pvol->v_CacheSize)*512), (LPBYTE)pvBuffer+((dwSector-sector)*512),  512);
            }
            pvol->v_pFATCacheLookup[dwOffset % pvol->v_CacheSize] = dwOffset;
        }        
    }

exit:
    LeaveCriticalSection(&pvol->v_csFATCache);

    return dwError;
}
#endif



/*  ReadWriteDisk
 *
 *  Reads/writes a set of contiguous sectors from a disk to/from memory.
 *
 *  Entry:
 *      pvol        pointer to VOLUME, NULL if none
 *      hdsk        handle to disk
 *      cmd         DISK_IOCTL_READ or DISK_IOCTL_WRITE
 *      pdi         address of DISK_INFO structure
 *      sector      0-based sector number
 *      cSectors    number of sectors to read/write
 *      pvBuffer    address of buffer to read/write sectors to/from
 *
 *  Exit:
 *      ERROR_SUCCESS if successful, else GetLastError() from the FSDMGR_DiskIoControl call issued
 */         

#ifdef DISK_CACHING
DWORD ReadWriteDisk2(PVOLUME pvol, HANDLE hdsk, DWORD cmd, PDISK_INFO pdi, DWORD sector, int cSectors, PVOID pvBuffer)
#else
DWORD ReadWriteDisk(PVOLUME pvol, HANDLE hdsk, DWORD cmd, PDISK_INFO pdi, DWORD sector, int cSectors, PVOID pvBuffer, BOOL fRemapFats)
#endif
{
    int k;
    int cRetry;
    SG_REQ Sg;
    DWORD dwError = ERROR_SUCCESS;

    ASSERT(hdsk != INVALID_HANDLE_VALUE);

#ifdef MAXDEBUG
    // See if we can catch excessive use of stack -JTP

    if (((DWORD)&k & 0xf000) <= 0x8000) {
        RETAILMSG(TRUE,(TEXT("FATFS!ReadWriteDisk: suspicious stack usage (%x)\n"), &k));
        DEBUGBREAK(TRUE);
    }
#endif

    DEBUGMSGW(ZONE_DISKIO,(DBGTEXTW("FATFS!ReadWriteDisk(%s, sector %d (0x%08x), total %d) at TC %ld\n"), cmd==DISK_IOCTL_READ? TEXTW("READ") : TEXTW("WRITE"), sector, sector, cSectors, GetTickCount()));

    

#ifdef TFAT
    // TFAT: remove the lock check since TFAT writes to the boot sector
        if (sector == 0 && cmd == DISK_IOCTL_WRITE && !pvol || sector + cSectors > pdi->di_total_sectors) {
            RETAILMSG(TRUE,(TEXT("FATFS!ReadWriteDisk: suspicious I/O! (sector %d)\n"), sector));
            ASSERT(FALSE);
            return ERROR_GEN_FAILURE;
        }
#else
        if (sector == 0 && cmd == DISK_IOCTL_WRITE && (!pvol || !(pvol->v_flags & VOLF_LOCKED)) || sector + cSectors > pdi->di_total_sectors) {
            RETAILMSG(TRUE,(TEXT("FATFS!ReadWriteDisk: suspicious I/O! (sector %d)\n"), sector));
            ASSERT(FALSE);
            return ERROR_GEN_FAILURE;
        }
#endif    

    cRetry = 1;         
    do {
        Sg.sr_start = sector;
        Sg.sr_num_sec = cSectors;
        Sg.sr_num_sg = 1;
#ifdef UNDER_WIN95
        Sg.sr_callback = (PFN_REQDONE)pdi;
#else
        Sg.sr_callback = NULL;
#endif
        Sg.sr_sglist[0].sb_len = cSectors * pdi->di_bytes_per_sect;
        Sg.sr_sglist[0].sb_buf = pvBuffer;

#ifdef DEBUG
        if (ZONE_DISKIO && ZONE_PROMPTS && cmd == DISK_IOCTL_WRITE) {

            FATUIDATA fui;
            DWORD dwVolFlags;

            if (pvol) {

                // Setting some bit (like VOLF_LOCKED) that will prevent most
                // any other call from entering FATFS while the following dialog is
                // active is very important, because this thread is temporarily
                // leaving our control, so it could re-enter FATFS and mess up the
                // state of any streams we've currently got locked (since critical
                // sections only protect our state from *other* threads, not from
                // ourselves).

                




                dwVolFlags = pvol->v_flags;
                pvol->v_flags |= VOLF_LOCKED;
            }

            fui.dwSize = sizeof(fui);
            fui.dwFlags = FATUI_CANCEL | FATUI_DEBUGMESSAGE;
            fui.idsEvent = (UINT)TEXTW("Preparing to write %1!d! sector(s) at sector %2!d! on %3!s!.\n\nSelect OK to allow, or Cancel to fail.");
            fui.idsEventCaption = IDS_FATUI_WARNING;
            fui.cuiParams = 3;
            fui.auiParams[0].dwType = UIPARAM_VALUE;
            fui.auiParams[0].dwValue = cSectors;
            fui.auiParams[1].dwType = UIPARAM_VALUE;
            fui.auiParams[1].dwValue = sector;
            fui.auiParams[2].dwType = UIPARAM_VALUE;
            fui.auiParams[2].dwValue = (DWORD)((pvol && pvol->v_pwsHostRoot)? (pvol->v_pwsHostRoot+1) : TEXTW("Storage Card"));

#ifdef FATUI
            if (FATUIEvent(hFATFS, (PWSTR)fui.auiParams[3].dwValue, &fui) == FATUI_CANCEL)
                dwError = ERROR_BAD_UNIT;
#endif                

            if (pvol)
                pvol->v_flags = (pvol->v_flags & ~VOLF_LOCKED) | (dwVolFlags & VOLF_LOCKED);
        
            if (dwError)
                goto error;
        }
#endif

        // Here, at last, is where we actually do the I/O...

        if (pdi->di_flags & DISK_INFO_FLAGS_FATFS_SIMULATED) {

            DWORD cb;

            if (SetFilePointer(hdsk, sector * pdi->di_bytes_per_sect, NULL, FILE_BEGIN) == 0xFFFFFFFF) {
                dwError = GetLastError();
                goto error;
            }

            if (cmd != DISK_IOCTL_WRITE) {
                if (ReadFile(hdsk, pvBuffer, Sg.sr_sglist[0].sb_len, &cb, NULL) == FALSE) {
                    dwError = GetLastError();
                    goto error;
                }
            }
            else {
                if (WriteFile(hdsk, pvBuffer, Sg.sr_sglist[0].sb_len, &cb, NULL) == FALSE) {
                    dwError = GetLastError();
                    goto error;
                }
            }

            if (cb != Sg.sr_sglist[0].sb_len) {
                dwError = ERROR_GEN_FAILURE;
                goto error;
            }

            Sg.sr_status = ERROR_SUCCESS;
        }
        else {
            DWORD dwIOCTL = cmd;

#ifdef TFAT_TIMING_TEST
            extern DWORD g_dwWriteCnt, g_dwReadCnt, g_dwWsecCnt, g_dwRsecCnt;
            if(cmd == DISK_IOCTL_WRITE){
                g_dwWriteCnt++;
                g_dwWsecCnt += cSectors;
            }
            if (cmd == DISK_IOCTL_READ){
                g_dwReadCnt++;
                g_dwRsecCnt += cSectors;
            }
#endif

            if (pdi->di_flags & DISK_INFO_FLAGS_FATFS_NEW_IOCTLS) {
                if (cmd == DISK_IOCTL_READ)
                    dwIOCTL = IOCTL_DISK_READ;
                else
                if (cmd == DISK_IOCTL_WRITE)
                    dwIOCTL = IOCTL_DISK_WRITE;
            }

            // TEST_BREAK -- also should go deeper into the DeviceIoControl code
            // NOTE: should also check breakpoints in public\common\oak\drivers\atadisk\diskio.c, in method
            // ATAWrite(), and around that area...
            PWR_BREAK_NOTIFY(1);

            if (FSDMGR_DiskIoControl((HDSK)hdsk, dwIOCTL, &Sg, sizeof(Sg), NULL, 0, &k, NULL) == FALSE) {
                dwError = GetLastError();
                goto error;
            }
            else {
                if (pdi->di_flags & DISK_INFO_FLAGS_FATFS_NEW_IOCTLS)
                    Sg.sr_status = ERROR_SUCCESS;       
            }

		}

        dwError = Sg.sr_status;

     error:
        DEBUGMSGW(ZONE_ERRORS && dwError,(DBGTEXTW("FATFS!ReadWriteDisk(%s, sector %d) failed (%d)\n"), cmd == DISK_IOCTL_READ? TEXTW("READ") : TEXTW("WRITE"), sector, dwError));

        // If the I/O was successful, or we got an unrecoverable error like
        // ERROR_NOT_READY (driver has disabled itself) or ERROR_BAD_UNIT (card
        // has been removed), or we've retried enough already, then get out.

        if (dwError == ERROR_SUCCESS ||
            dwError == ERROR_NOT_READY ||
            dwError == ERROR_BAD_UNIT ||
            cRetry-- == 0)
            break;

        DEBUGMSG(ZONE_DISKIO || ZONE_ERRORS,(DBGTEXT("FATFS!ReadWriteDisk: retrying...\n")));

    } while (TRUE);

    if (dwError) {

        // To be safe, any disk I/O error will set VOLF_UNCERTAIN.  This will
        // prevent us from making unsafe assumptions later (like whether the disk
        // is formatted or not).  VOLF_UNCERTAIN is cleared every time a volume
        // is created or remounted/recycled (in OpenVolume).

        if (pvol)
            pvol->v_flags |= VOLF_UNCERTAIN;
    }

#ifdef DEBUG
    else if (cmd == DISK_IOCTL_READ && ZONE_READVERIFY && pvol) {

        PBYTE pbTmp;

        ALLOCA(pbTmp, pdi->di_bytes_per_sect+2);
        if (pbTmp) {
            while (cSectors--) {
                DEBUGMSG(ZONE_DISKIO,(DBGTEXT("FATFS!ReadWriteDisk: verifying read from sector %d\n"), Sg.sr_start));

                memset(pbTmp, 'J', pdi->di_bytes_per_sect+2);

                Sg.sr_num_sec = 1;
                Sg.sr_sglist[0].sb_len = pdi->di_bytes_per_sect;
                Sg.sr_sglist[0].sb_buf = pbTmp+1;

                if (FSDMGR_DiskIoControl((HDSK)hdsk,
                                    (pdi->di_flags & DISK_INFO_FLAGS_FATFS_NEW_IOCTLS) ? IOCTL_DISK_READ : DISK_IOCTL_READ,
                                    &Sg, sizeof(Sg),
                                    NULL, 0, &k, NULL) == FALSE) {
                    dwError = GetLastError();
                    DEBUGMSG(ZONE_DISKIO || ZONE_ERRORS,(DBGTEXT("FATFS!ReadWriteDisk(READ, sector %d) failed (%d)\n"), Sg.sr_start, dwError));
                }
                else {
                    if (memcmp(pvBuffer, pbTmp+1, pdi->di_bytes_per_sect) != 0) {
                        dwError = ERROR_GEN_FAILURE;
                        DEBUGMSGW(ZONE_DISKIO || ZONE_ERRORS,(DBGTEXTW("FATFS!ReadWriteDisk(VERIFY, sector %d) failed\n"), Sg.sr_start));
                    }
                    if (pbTmp[0] != 'J' || pbTmp[pdi->di_bytes_per_sect+1] != 'J') {
                        dwError = ERROR_GEN_FAILURE;
                        DEBUGMSGW(ZONE_DISKIO || ZONE_ERRORS,(DBGTEXTW("FATFS!ReadWriteDisk(BOUNDSCHECK, sector %d) failed\n"), Sg.sr_start));
                    }
                }
                Sg.sr_start++;
                (PBYTE)pvBuffer += pdi->di_bytes_per_sect;
            }
        }
    }
#endif

    else if (cmd == DISK_IOCTL_WRITE) {

#ifdef DEBUG
        creqWrite++;            // keep track of total write requests and sectors written
        csecWrite += cSectors;
#endif

        // If the operation was successful, and it was a WRITE, then set the
        // modified flag in the associated VOLUME structure (if there is one).  We
        // use this flag to avoid a full unmount/remount cycle after a scan or format
        // if no changes were actually made.

        if (pvol) {
            pvol->v_flags |= VOLF_MODIFIED;
            
            // Also on writes, if we still have outstanding reads-after-writes to do, do them now;
            // note that we only support this feature on non-simulated volumes.

            if ((pvol->v_bVerifyCount || ZONE_WRITEVERIFY) && !(pdi->di_flags & DISK_INFO_FLAGS_FATFS_SIMULATED)) {

                PBYTE pbTmp;

                ALLOCA(pbTmp, pdi->di_bytes_per_sect);
                if (pbTmp) {
                    cRetry = 1;         
                    while (cSectors--) {
reverify:
                        DEBUGMSG(pvol->v_bVerifyCount || ZONE_DISKIO,(DBGTEXT("FATFS!ReadWriteDisk: verifying write to sector %d\n"), Sg.sr_start));

                        memset(pbTmp, 'J', pdi->di_bytes_per_sect);

                        Sg.sr_num_sec = 1;
                        Sg.sr_sglist[0].sb_len = pdi->di_bytes_per_sect;
                        Sg.sr_sglist[0].sb_buf = pbTmp;

                        if (FSDMGR_DiskIoControl((HDSK)hdsk,
                                            (pdi->di_flags & DISK_INFO_FLAGS_FATFS_NEW_IOCTLS) ? IOCTL_DISK_READ : DISK_IOCTL_READ,
                                            &Sg, sizeof(Sg),
                                            NULL, 0, &k, NULL) == FALSE) {
                            dwError = GetLastError();
                            DEBUGMSG(pvol->v_bVerifyCount || ZONE_DISKIO || ZONE_ERRORS,(DBGTEXT("FATFS!ReadWriteDisk(READ, sector %d) failed (%d)\n"), Sg.sr_start, dwError));
                        }
                        else {
                            if (memcmp(pvBuffer, pbTmp, pdi->di_bytes_per_sect) != 0) {
                                dwError = ERROR_GEN_FAILURE;
                                DEBUGMSGW(pvol->v_bVerifyCount || ZONE_DISKIO || ZONE_ERRORS,(DBGTEXTW("FATFS!ReadWriteDisk(VERIFY, sector %d) failed\n"), Sg.sr_start));
                            }
                        }

                        if (dwError) {

                            FATUIDATA fui;
                            DWORD dwVolFlags;

                            // Setting some bit (like VOLF_LOCKED) that will prevent most
                            // any other call from entering FATFS while the following dialog is
                            // active is very important, because this thread is temporarily
                            // leaving our control, so it could re-enter FATFS and mess up the
                            // state of any streams we've currently got locked (since critical
                            // sections only protect our state from *other* threads, not from
                            // ourselves).

                            // This work-around only blocks most of the
                            // path-based calls, but at least it takes care of FAT_OidGetInfo, which 
                            // was the one causing us real grief. -JTP

                            dwVolFlags = pvol->v_flags;
                            pvol->v_flags |= VOLF_LOCKED;

                            fui.dwSize = sizeof(fui);
                            fui.dwFlags = FATUI_YES | FATUI_NO;
                            fui.idsEvent = IDS_FATUI_WRITEVERIFY_WARNING;
                            fui.idsEventCaption = IDS_FATUI_WARNING;
                            fui.cuiParams = 2;
                            fui.auiParams[0].dwType = UIPARAM_VALUE;
                            fui.auiParams[0].dwValue = Sg.sr_start;
                            fui.auiParams[1].dwType = UIPARAM_VALUE;
                            fui.auiParams[1].dwValue = dwError;

#ifdef FATUI
                            k = FATUIEvent(hFATFS, pvol->v_pwsHostRoot? pvol->v_pwsHostRoot+1 : NULL, &fui);
#else
                            k = FATUI_YES;
#endif                            

                            pvol->v_flags = (pvol->v_flags & ~VOLF_LOCKED) | (dwVolFlags & VOLF_LOCKED);

                            if (k == FATUI_YES) {
                                dwError = ERROR_SUCCESS;
                                if (cRetry-- == 0)
                                    goto exit;  // leave the verify count alone in this case
                                goto reverify;
                            }
                            break;              // just report 1 error per multi-sector write
                        }

                        Sg.sr_start++;
                        (PBYTE)pvBuffer += pdi->di_bytes_per_sect;
                    }
                }

                // As long as there were no verify errors, go ahead and reduce the verify count

                if (!dwError && pvol->v_bVerifyCount > 0 && pvol->v_bVerifyCount < 255)
                    pvol->v_bVerifyCount--;
            }
        }
    }

exit:
    DEBUGMSG(ZONE_DISKIO,(DBGTEXT("FATFS!ReadWriteDisk returned 0x%x at TC %ld\n"), dwError, GetTickCount()));
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

        if (!FSDMGR_DiskIoControl((HDSK)pvol->v_pdsk->d_hdsk, 
                        IOCTL_DISK_DELETE_SECTORS, 
                        &dsi, sizeof(DELETE_SECTOR_INFO), 
                        NULL, 0, 
                        &dwRet, 
                        NULL)) 
        {
            pvol->v_pdsk->d_flags &= ~DSKF_SENDDELETE;

#ifdef TFAT
            // No need for the frozen cluster list any more.
            if (pvol->v_pFrozenClusterList)
            {
                VirtualFree (pvol->v_pFrozenClusterList, 0, MEM_RELEASE);
                pvol->v_pFrozenClusterList = NULL;
            }    
#endif      
            return FALSE;
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
                DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!FindDisk: remounting frozen disk...\n")));
                goto init2;
            }
        }
        pdsk = pdsk->d_dlOpenDisks.pdskNext;
    }

    if (!pdi) {
        pdsk = NULL;
        goto exit;
    }    

    pdsk = (PDSK)LocalAlloc(LPTR, sizeof(DSK)-sizeof(pdsk->d_wsName) + cwDisk*sizeof(pwsDisk[0]));
    if (!pdsk)
        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!FindDisk: out of memory!\n")));
    else {
        DEBUGALLOC(sizeof(DSK)-sizeof(pdsk->d_wsName) + cwDisk*sizeof(pwsDisk[0]));
        AddItem((PDLINK)&dlDisks, (PDLINK)&pdsk->d_dlOpenDisks);
        // InitList((PDLINK)&pdsk->d_dlPartitions);
        pdsk->d_diActive = *pdi;
init2:
        pdsk->d_cwName = cwDisk;
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

        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!MountDisk: no valid disk info (%d)\n"), GetLastError()));

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
        DEBUGMSG((di.di_flags & DISK_INFO_FLAGS_FATFS_SIMULATED),(DBGTEXT("FATFS!MountDisk: driver is setting reserved bit(s)!\n")));
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

    pbSector = (PBYTE)LocalAlloc(LPTR, di.di_bytes_per_sect);
    if (!pbSector) {
        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!MountDisk: out of memory, aborting mount\n")));
        return NULL;
    }
    DEBUGALLOC(di.di_bytes_per_sect);

    dwError = ReadWriteDisk(NULL, hdsk, DISK_IOCTL_READ, &di, 0, 1, pbSector, TRUE);
    DEBUGMSG(ZONE_ERRORS && dwError,(DBGTEXT("FATFS!MountDisk: error reading MBR (%d)\n"), dwError));

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
        DEBUGMSGW(ZONE_ERRORS,(DBGTEXTW("FATFS!MountDisk: BPB_HiddenSectors(%d) != 0\n"), ((PBIGFATBOOTSEC)pbSector)->bgbsBPB.oldBPB.BPB_HiddenSectors));
        ((PBIGFATBOOTSEC)pbSector)->bgbsBPB.oldBPB.BPB_HiddenSectors = 0;
    }
    pdsk->pVol = MountVolume(pdsk, (PBIGFATBOOTSEC *)&pbSector, NULL, flVol);

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
    VERIFYNULL(LocalFree((HLOCAL)pbSector));
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

    DEBUGMSGW(ZONE_INIT,(DBGTEXTW("FATFS!UnmountDisk: unmounting %s volumes on disk %s\n"), fFrozen? TEXTW("frozen") : TEXTW("all"), pdsk->d_wsName));

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
        VERIFYNULL(LocalFree((HLOCAL)pdsk));
    }

    LeaveCriticalSection(&csFATFS);

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!UnmountDisk returned %d\n"), fSuccess));
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
