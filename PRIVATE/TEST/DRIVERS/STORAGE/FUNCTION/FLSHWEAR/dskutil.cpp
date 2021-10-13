//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "dskutil.h"

static DWORD LargestCommonFactor(DWORD val1, DWORD val2)
{
    DWORD factor;
    DWORD max = (val1 < val2) ? val1 : val2;

    for(factor = max; factor > 1; factor--) {
        if(0 == (val1 % factor) && 0 == (val2 % factor)) {
            break;
        }
    }
    return factor;
}

BOOL Util_FillDiskLinear(HANDLE hDisk, DWORD percentToFill)
{
    BOOL fRet = FALSE;
    BYTE *pBuffer = NULL;
    DISK_INFO di;
    DWORD percentFilled, sectorsFilled, sectorsToFill;
    
    // low level format should put all blocks on the free list and 
    // get rid of any dirty blocks
    //
    LOG(L"formatting the media...");
    if(!Dsk_FormatMedia(hDisk)) {
        ERRFAIL("Dsk_FormatMedia()");
        goto done;
    }

    // disk info will give us the total sector count and the sector size
    //
    LOG(L"querrying disk information...");
    if(!Dsk_GetDiskInfo(hDisk, &di)) {
        ERRFAIL("Dsk_GetDiskInfo()");
        goto done;
    }

    LOG(L"disk contains %u total sectors of size %u bytes", di.di_total_sectors,
        di.di_bytes_per_sect);

    // allocate write buffer
    //
    pBuffer = new BYTE[di.di_bytes_per_sect];
    if(NULL == pBuffer) {
        ERRFAIL("new BYTE[]");
        goto done;
    }

    // calculate the approx. number of sectors to fill to equal the requested
    // percentage
    //
    sectorsToFill = (percentToFill * di.di_total_sectors) / 100;
    LOG(L"filling the disk to %u%% capacity (%u of %u sectors)...",
        percentToFill, sectorsToFill, di.di_total_sectors);

    // fill the disk to the requested capacity
    //
    percentFilled = 0xFFFFFFFF;
    for(sectorsFilled = 0; sectorsFilled < sectorsToFill; sectorsFilled++) {
        if(percentFilled != (100*sectorsFilled) / (di.di_total_sectors)) {
            percentFilled = (100*sectorsFilled) / (di.di_total_sectors);
            LOG(L"disk is filled to %u%% capacity...", percentFilled);
        }

        // perform single sector writes for filling
        //
        if(!Dsk_WriteSectors(hDisk, sectorsFilled, 1, pBuffer)) {
            ERRFAIL("Dsk_WriteSectors()");
            goto done;
        }
    }

    LOG(L"disk is filled to %u%% capacity...", percentToFill);

    fRet = TRUE;

done:
    if(pBuffer) {
        delete[] pBuffer;
    }
    return fRet;
}

BOOL Util_FillDiskRandom(HANDLE hDisk, DWORD percentToFill)
{
    BOOL fRet = FALSE;
    BYTE *pBuffer = NULL;
    BOOL *pfFilled = NULL;
    DISK_INFO di;
    DWORD percentFilled, sectorsFilled, sectorsToFill, sector;

    // low level format should put all blocks on the free list and 
    // get rid of any dirty blocks
    //
    LOG(L"formatting the media...");
    if(!Dsk_FormatMedia(hDisk)) {
        ERRFAIL("Dsk_FormatMedia()");
        goto done;
    }

    // disk info will give us the total sector count and the sector size
    //
    LOG(L"querrying disk information...");
    if(!Dsk_GetDiskInfo(hDisk, &di)) {
        ERRFAIL("Dsk_GetDiskInfo()");
        goto done;
    }

    LOG(L"disk contains %u total sectors of size %u bytes", di.di_total_sectors,
        di.di_bytes_per_sect);

    // allocate write buffer
    //
    pBuffer = new BYTE[di.di_bytes_per_sect];
    if(NULL == pBuffer) {
        ERRFAIL("new BYTE[]");
        goto done;
    }

    pfFilled = new BOOL[di.di_total_sectors];
    if(NULL == pfFilled) {
        ERRFAIL("new BOOL[]");
        goto done;
    }

    sectorsToFill = (percentToFill * di.di_total_sectors) / 100;
    LOG(L"filling the disk to %u%% capacity (%u of %u sectors)...",
        percentToFill, sectorsToFill, di.di_total_sectors);

    // fill the disk to the requested percentage
    //
    memset(pfFilled, 0, sizeof(BOOL)*di.di_total_sectors);
    percentFilled = 0xFFFFFFFF;
    for(sectorsFilled = 0; sectorsFilled < sectorsToFill; sectorsFilled++) {
        if(percentFilled != (100*sectorsFilled) / (di.di_total_sectors)) {
            percentFilled = (100*sectorsFilled) / (di.di_total_sectors);
            LOG(L"disk is filled to %u%% capacity...", percentFilled);
        }

        // fill sectors randomly, but track which one's have been
        // already filled using the pfFilled buffer (so the same sector
        // doesn't get written multiple times)
        do
        {
            sector = Random() % di.di_total_sectors;

        } while(TRUE == pfFilled[sector]);

        // perform single sector writes for filling
        //
        if(!Dsk_WriteSectors(hDisk, sectorsFilled, 1, pBuffer)) {
            ERRFAIL("Dsk_WriteSectors()");
            goto done;
        }

        pfFilled[sector] = TRUE;
    }

    LOG(L"disk is filled to %u%% capacity...", percentToFill);

    fRet = TRUE;

done:
    if(pBuffer) {
        delete[] pBuffer;
    }
    if(pfFilled) {
        delete[] pfFilled;
    }
    return fRet;
}

BOOL Util_FillDiskMaxFragment(HANDLE hDisk, DWORD percentToFill)
{
    BOOL fRet = FALSE;
    BYTE *pBuffer = NULL;
    DISK_INFO di;
    DWORD sector, chunk, percentFilled, sectorsFilled;
    DWORD factor, sectorsToFillPerChunk, sectorsPerChunk, sectorsFilledThisChunk;

    // low level format should put all blocks on the free list and 
    // get rid of any dirty blocks
    //
    LOG(L"formatting the media...");
    if(!Dsk_FormatMedia(hDisk)) {
        ERRFAIL("Dsk_FormatMedia()");
        goto done;
    }

    // disk info will give us the total sector count and the sector size
    //
    LOG(L"querrying disk information...");
    if(!Dsk_GetDiskInfo(hDisk, &di)) {
        ERRFAIL("Dsk_GetDiskInfo()");
        goto done;
    }

    LOG(L"disk contains %u total sectors of size %u bytes", di.di_total_sectors,
        di.di_bytes_per_sect);

    if(percentToFill > 0) {

        // allocate write buffer
        //
        pBuffer = new BYTE[di.di_bytes_per_sect];
        if(NULL == pBuffer) {
            ERRFAIL("new BYTE[]");
            goto done;
        }

        factor = LargestCommonFactor(percentToFill, 100);
        sectorsToFillPerChunk = percentToFill / factor;
        sectorsPerChunk = 100 / factor;

        LOG(L"writing %u out of every %u sectors...", sectorsToFillPerChunk, sectorsPerChunk);

        // loop through every chunk-sized region of sectors
        sectorsFilled = 0;
        percentFilled = 0;
        for(chunk = 0; chunk < (di.di_total_sectors - (di.di_total_sectors % sectorsPerChunk)); 
            chunk += sectorsPerChunk) {
            if(percentFilled != (100*sectorsFilled) / (di.di_total_sectors)) {
                percentFilled = (100*sectorsFilled) / (di.di_total_sectors);
                LOG(L"disk is filled to %u%% capacity...", percentFilled);
            }

            sectorsFilledThisChunk = 0;
            for(sector = chunk; sector < chunk+sectorsPerChunk; sector++) {

                if(percentToFill <= 50) {
                    
                    factor = sectorsPerChunk / sectorsToFillPerChunk;

                    if(0 == (sector % factor)) {
                        if(!Dsk_WriteSectors(hDisk, sector, 1, pBuffer)) {
                            ERRFAIL("Dsk_WriteSectors");
                            goto done;
                        }
                        sectorsFilledThisChunk += 1;
                    }
                    
                } else { // if(percentToFill > 50)

                    if(sectorsPerChunk == sectorsToFillPerChunk) {
                        factor = 0;
                    } else {
                        factor = (10*sectorsPerChunk) / (sectorsPerChunk - sectorsToFillPerChunk);
                        factor += 5;
                        factor /= 10; // round up
                    }

                    if(0 != (sector % factor) || 0 == factor) {
                        if(!Dsk_WriteSectors(hDisk, sector , 1, pBuffer)) {
                            ERRFAIL("Dsk_WriteSectors");
                            goto done;
                        }
                        sectorsFilledThisChunk += 1;
                    }
                }
                if(sectorsFilledThisChunk >= sectorsToFillPerChunk) {
                    break;
                }
            }
            sectorsFilled += sectorsFilledThisChunk;
        }
        
    }

    LOG(L"disk is filled to %u%% capacity...", percentToFill);

    fRet = TRUE;
done:
    if(pBuffer) {
        delete[] pBuffer;
    }
    return fRet;
}

BOOL Util_FreeDiskLinear(HANDLE hDisk, DWORD percentToFree)
{
    BOOL fRet = FALSE;
    DISK_INFO di;
    DWORD sectorsToFree;

    // disk info will give us the total sector count and the sector size
    //
    LOG(L"querrying disk information...");
    if(!Dsk_GetDiskInfo(hDisk, &di)) {
        ERRFAIL("Dsk_GetDiskInfo()");
        goto done;
    }

    LOG(L"disk contains %u total sectors of size %u bytes", di.di_total_sectors,
        di.di_bytes_per_sect);

    // calculate the approx. number of sectors to fill to equal the requested
    // percentage
    //
    sectorsToFree = (percentToFree * di.di_total_sectors) / 100;
    LOG(L"linearly freeing %u%% of the disk capacity (%u of %u sectors)...",
        percentToFree, sectorsToFree, di.di_total_sectors);

    if(!Dsk_DeleteSectors(hDisk, 0, sectorsToFree)) {
        ERRFAIL("Dsk_DeleteSectors()");
        goto done;
    }

    LOG(L"%u%% of disk capacity freed linearly!", percentToFree);

    fRet = TRUE;

done:    return fRet;
}

BOOL Util_FreeDiskRandom(HANDLE hDisk, DWORD percentToFree)
{
    BOOL fRet = FALSE;
    BOOL *pfFreed = NULL;
    DISK_INFO di;
    DWORD percentFreed, sectorsFreed, sectorsToFree, sector;

    // disk info will give us the total sector count and the sector size
    //
    LOG(L"querrying disk information...");
    if(!Dsk_GetDiskInfo(hDisk, &di)) {
        ERRFAIL("Dsk_GetDiskInfo()");
        goto done;
    }

    LOG(L"disk contains %u total sectors of size %u bytes", di.di_total_sectors,
        di.di_bytes_per_sect);

    pfFreed = new BOOL[di.di_total_sectors];
    if(NULL == pfFreed) {
        ERRFAIL("new BOOL[]");
        goto done;
    }

    sectorsToFree = (percentToFree * di.di_total_sectors) / 100;
    LOG(L"randomly freeing %u%% of disk capacity (%u of %u sectors)...",
        percentToFree, sectorsToFree, di.di_total_sectors);

    // free the requested percentage of sectors
    //
    memset(pfFreed, 0, sizeof(BOOL)*di.di_total_sectors);
    percentFreed = 0xFFFFFFFF;
    for(sectorsFreed = 0; sectorsFreed < sectorsToFree; sectorsFreed++) {
        if(percentFreed != (100*sectorsFreed) / (di.di_total_sectors)) {
            percentFreed = (100*sectorsFreed) / (di.di_total_sectors);
            LOG(L"%u%% of disk has been freed...", percentFreed);
        }

        // free sectors randomly, but track which one's have already
        // been freed using the pfFilled buffer (so the same sector
        // doesn't get freed multiple times)
        do
        {
            sector = Random() % di.di_total_sectors;

        } while(TRUE == pfFreed[sector]);

        // perform single sector deletes
        //
        if(!Dsk_DeleteSectors(hDisk, sectorsFreed, 1)) {
            ERRFAIL("Dsk_DeleteSectors()");
            goto done;
        }

        pfFreed[sector] = TRUE;
    }

    LOG(L"%u%% of disk capacity freed randomly!", percentToFree);

    fRet = TRUE;

done:
    if(pfFreed) {
        delete[] pfFreed;
    }
    return fRet;
}

BOOL Util_FreeDiskMaxFragment(HANDLE hDisk, DWORD percentToFree)
{
    BOOL fRet = FALSE;
    DISK_INFO di;
    DWORD sector, chunk, percentFreed, sectorsFreed;
    DWORD factor, sectorsToFreePerChunk, sectorsPerChunk, sectorsFreedThisChunk;

    // disk info will give us the total sector count and the sector size
    //
    LOG(L"querrying disk information...");
    if(!Dsk_GetDiskInfo(hDisk, &di)) {
        ERRFAIL("Dsk_GetDiskInfo()");
        goto done;
    }

    LOG(L"disk contains %u total sectors of size %u bytes", di.di_total_sectors,
        di.di_bytes_per_sect);

    if(percentToFree > 0) {        

        factor = LargestCommonFactor(percentToFree, 100);
        sectorsToFreePerChunk = percentToFree / factor;
        sectorsPerChunk = 100 / factor;

        LOG(L"deleting %u out of every %u sectors...", sectorsToFreePerChunk, sectorsPerChunk);

        // loop through every chunk-sized region of sectors
        sectorsFreed = 0;
        percentFreed = 0;
        for(chunk = 0; chunk < (di.di_total_sectors - (di.di_total_sectors % sectorsPerChunk)); 
            chunk += sectorsPerChunk) {
            if(percentFreed != (100*sectorsFreed) / (di.di_total_sectors)) {
                percentFreed = (100*sectorsFreed) / (di.di_total_sectors);
                LOG(L"%u%% of disk has been freed...", percentFreed);
            }

            sectorsFreedThisChunk = 0;
            for(sector = chunk; sector < chunk+sectorsPerChunk; sector++) {

                if(percentToFree <= 50) {
                    
                    factor = sectorsPerChunk / sectorsToFreePerChunk;

                    if(0 == (sector % factor)) {
                        if(!Dsk_DeleteSectors(hDisk, sector, 1)) {
                            ERRFAIL("Dsk_DeleteSectors");
                            goto done;
                        }
                        sectorsFreedThisChunk += 1;
                    }
                    
                } else { // if(percentToFree > 50)

                    if(sectorsPerChunk == sectorsToFreePerChunk) {
                        factor = 0;
                    } else {
                        factor = (10*sectorsPerChunk) / (sectorsPerChunk - sectorsToFreePerChunk);
                        factor += 5;
                        factor /= 10; // round up
                    }

                    if(0 == factor || 0 != (sector % factor)) {
                        if(!Dsk_DeleteSectors(hDisk, sector , 1)) {
                            ERRFAIL("Dsk_DeleteSectors");
                            goto done;
                        }
                        sectorsFreedThisChunk += 1;
                    }
                }
                if(sectorsFreedThisChunk >= sectorsToFreePerChunk) {
                    break;
                }
            }
            sectorsFreed += sectorsFreedThisChunk;
        }
        
    }

    LOG(L"%u%% of disk capacity freed for maximum fragmentation!", percentToFree);

    fRet = TRUE;
done:
    return fRet;
}
