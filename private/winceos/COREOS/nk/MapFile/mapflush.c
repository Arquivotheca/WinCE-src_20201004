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

#include "kernel.h"
#include <diskio.h>


extern CRITICAL_SECTION MapCS;
CRITICAL_SECTION FlushCS;  // Ensures only one flush is ongoing at a time


//------------------------------------------------------------------------------
// GATHERED FLUSHES
//
// Write data back to the file, using file system scatter-gather writes to
// guarantee atomicity.  This flush is faster than atomic flushes because it
// does not need to write data twice.  It can also result in less
// fragmentation/wear-leveling problems in the file system than simple or
// atomic flushes.  So it should be used whenever file system support is
// available.
//------------------------------------------------------------------------------


// Callback from FlushPages
static BOOL
GatherProlog (
    FlushParams* pParams
    )
{
    // sanity check
    DEBUGCHK(!(pParams->pfsmap->dwFlags & (MAP_DIRECTROM | MAP_FILECACHE)));
    DEBUGCHK(pParams->pfsmap->dwFlags & MAP_GATHER);

#ifdef DEBUG
    // Since it is possible to create a file using atomic flushes, and then
    // use it with gathered flushes later (eg. move it to a different file
    // system or a different device) we need to be careful with atomicity
    if (pParams->pfsmap->dwFlags & MAP_ATOMIC) {
    
        // Atomic maps still require proper restore data on the first page,
        // but that data should already be correct.
        FSLogFlushSettings* pMemStruct;
        DWORD cbRead;
        
        // Read the flush status out of the file
        if (!PHD_ReadFileWithSeek (pParams->pfsmap->phdFile, &pParams->FileStruct,
                                   sizeof(FSLogFlushSettings), &cbRead, 0,
                                   offsetof(fslog_t, dwRestoreFlags), 0)
            || (sizeof(FSLogFlushSettings) != cbRead)) {
            // We can exit here because nothing has been changed.
            DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: GatherProlog: Failed to read flush status from file\r\n"));
            DEBUGCHK (0);  // Shouldn't happen
            return FALSE;
        }

        // Views to atomic maps always start at the front of the map, thus the
        // first page of the view is the first page of the map, which contains
        // the fslog struct
        DEBUGCHK(pParams->pViewBase->liMapOffset.QuadPart == 0);

        pMemStruct = (FSLogFlushSettings*) ((DWORD) pParams->pViewBase->pBaseAddr
                                            + offsetof(fslog_t, dwRestoreFlags));

        DEBUGCHK(pMemStruct->dwRestoreFlags == FSLOG_RESTORE_FLAG_NONE);
        // pMemStruct->dwRestoreSize >> 16 (dirty page count) can be nonzero
        // if the mapping was created on a different file system and then moved
        // to one that supports gathered flushes.

        // File and memory should be in sync
        DEBUGCHK(pParams->FileStruct.dwRestoreFlags == pMemStruct->dwRestoreFlags);
        DEBUGCHK(pParams->FileStruct.dwRestoreStart == pMemStruct->dwRestoreStart);
        DEBUGCHK(pParams->FileStruct.dwRestoreSize  == pMemStruct->dwRestoreSize);
    }
#endif // DEBUG

    return TRUE;
}


// Callback from VMMapClearDirtyPages
static BOOL
GatherRecordPage (
    FlushParams* pParams,
    PVOID        pAddr,    // Page start address
    DWORD        idxPage   // Which dirty page # this is, first dirty page is 0
    )
{
    DEBUGCHK(((DWORD)pAddr % VM_PAGE_SIZE) == 0);

    // Sanity check: atomic views should not have any dirty data past dwRestoreStart
    DEBUGCHK(!(pParams->pfsmap->dwFlags & MAP_ATOMIC)
             || ((LPBYTE)pAddr <= pParams->pViewBase->pBaseAddr + pParams->FileStruct.dwRestoreStart));

    if (idxPage < pParams->pFlush->dwNumElements) {
        // Segment = page address in memory
        DEBUGCHK (pParams->pfsmap->phdFile);
        pParams->pFlush->gather.prgSegments[idxPage].Alignment = (DWORD) pAddr;
        
        // Offset = file offset to write to
        pParams->pFlush->gather.prgliOffsets[idxPage].QuadPart
            = pParams->pViewBase->liMapOffset.QuadPart + ((DWORD)pAddr - (DWORD)pParams->pViewBase->pBaseAddr);

        DEBUGMSG (ZONE_MAPFILE && ZONE_PAGING, (L"MMFILE: Prepare to flush page %8.8lx (gather)\r\n", pAddr));
        return TRUE;
    }
    
    // Number of pages to write should never exceed number of segments available
    DEBUGCHK(0);
    return FALSE;
}


// Callback from FlushPages
static DWORD
GatherWritePages (
    FlushParams* pParams,
    DWORD        cPages
    )
{
    // Flush the pages to the file using the file system's scatter-gather
    // transactioning.
    if (cPages) {
        // WriteFileGather() = IOCTL_FILE_WRITE_GATHER
        DEBUGCHK (pParams->pfsmap->phdFile);
        DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: GatherWritePages: Writing %u dirty pages\r\n", cPages));
        if (PHD_FSIoctl (pParams->pfsmap->phdFile, IOCTL_FILE_WRITE_GATHER,
                         pParams->pFlush->gather.prgSegments, cPages*VM_PAGE_SIZE,
                         (LPDWORD) pParams->pFlush->gather.prgliOffsets, 0, NULL, NULL)) {
            DEBUGCHK (pParams->pViewBase->dwDirty >= cPages);
            InterlockedExchangeAdd ((LONG*)&pParams->pViewBase->dwDirty, - (int) cPages);
            return cPages;
        }
        DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: GatherWritePages: WriteFileGather failed (%u)\r\n",
                                 KGetLastError (pCurThread)));
    }

    return 0;
}


// Callback from FlushPages
static BOOL
GatherEpilog (
    FlushParams* pParams,
    BOOL         fSuccess,
    DWORD        cTotalPages,
    DWORD        idxLastPage
    )
{
    // Flush the dirty blocks in the cache back to the file
    if (!PHD_FlushFileBuffers (pParams->pfsmap->phdFile)) {
        DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: GatherEpilog: Flush commit failed\r\n"));
        fSuccess = FALSE;
    }
    
    if (!fSuccess) {
        // Failed to flush all pages
        DWORD idxPage;

        // Mark all of the pages that were being flushed R/W again
        for (idxPage = 0; idxPage < cTotalPages; idxPage++) {
            DEBUGCHK (pParams->pfsmap->phdFile);
            DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: GatherEpilog: Restoring dirty page %8.8lx\r\n",
                                     pParams->pFlush->gather.prgSegments[idxPage].Alignment));
            VERIFY (VMProtect (pParams->pprc,
                               (LPVOID) (DWORD)pParams->pFlush->gather.prgSegments[idxPage].Alignment,
                               VM_PAGE_SIZE, PAGE_READWRITE, NULL));
        }
    }
    
    return fSuccess;
}


//------------------------------------------------------------------------------
// ATOMIC FLUSHES
//
// Write data back to the file, guaranteeing atomicity by writing all of the
// pages to the end of the file (=FlushMapAtomic) and then copying the data
// into the proper place inside the file (=ValidateFile).
//------------------------------------------------------------------------------

// Used to copy pages of data
typedef struct {
    DWORD dwWriteOffset;
    BYTE  data[1]; // followed by a buffer the size of pages in the file
} RestoreInfo;


// This function is only run on atomic maps, ie. mapped database files.
// It is called once when gathered atomic maps are opened, to clear out any old
// flushes in the file.  Other than that it is only used to finish flushes for
// non-gathered atomic maps.
DWORD
ValidateFile(
    PFSMAP pfsmap
    )
{
    FSLogFlushSettings FlushStruct;  // Current flush progress
    DWORD cbReadWritten;

    DEBUGCHK(pfsmap->dwFlags & MAP_ATOMIC);

    // Read restore data
    if (!PHD_ReadFileWithSeek (pfsmap->phdFile, &FlushStruct,
                               sizeof(FSLogFlushSettings), &cbReadWritten, 0, 
                               offsetof(fslog_t, dwRestoreFlags), 0)
        || (sizeof(FSLogFlushSettings) != cbReadWritten)) {
        // This shouldn't happen.
        DEBUGCHK (0);
        goto error;
    }

    DEBUGCHK(!pfsmap->liSize.HighPart);  // Atomic maps are never >16MB
    CELOG_MapFileValidate(pfsmap->liSize.LowPart, FlushStruct.dwRestoreSize,
                          FlushStruct.dwRestoreFlags, TRUE);

    // Now act on the restore state
    switch (FlushStruct.dwRestoreFlags) {
    case FSLOG_RESTORE_FLAG_FLUSHED:
    {
        PVOID pBuffer = NULL;
        DWORD cbBuffer = 0;
        DWORD dwReadOffset, cPages, cbPageSize;

        // AtomicWritePages wrote all data to the end of the file, tagged with
        // its proper offset.  Now that they are all flushed to the end, they
        // are committed by copying them to their proper offsets in the main
        // part of the file.

        dwReadOffset = FlushStruct.dwRestoreStart;
        cPages       = (FlushStruct.dwRestoreSize >> 16);
        cbPageSize   = (FlushStruct.dwRestoreSize & 0xffff);  // May differ from this device's VM_PAGE_SIZE
        cbBuffer     = sizeof(DWORD) + cbPageSize;  // Contains a RestoreInfo struct

        // Sanity check, the restore-start must be an even multiple of the page size
        if (0 != (FlushStruct.dwRestoreStart % cbPageSize)) {
            DEBUGMSG(ZONE_MAPFILE, (TEXT("MMFILE: ValidateFile: Invalid atomic file\r\n")));  // file is corrupt?
            DEBUGCHK(0);
            goto error;
        }

        pBuffer = NKmalloc (cbBuffer);
        if (pBuffer) {
            RestoreInfo* pRestore = (RestoreInfo*) pBuffer;
            BOOL fSuccess = TRUE;
            
            // Move each dirty page from the end of the file to its proper place.
            while (cPages--) {

                // Read the page of data from the file
                if (!PHD_ReadFileWithSeek (pfsmap->phdFile, pRestore, cbBuffer,
                                           &cbReadWritten, 0, dwReadOffset, 0)
                    || (cbReadWritten != cbBuffer)) {
                    fSuccess = FALSE;
                    break;
                }
    
                DEBUGMSG(ZONE_MAPFILE && ZONE_PAGING,
                         (TEXT("MMFILE: ValidateFile: page 0x%08x of map 0x%08x (count=%u)\r\n"),
                          pRestore->dwWriteOffset, pfsmap, cPages));
                
                // Page 0 contains the flush flags.  Copy them before writing it to
                // the file.  That way flags currently in the file aren't overwritten
                // with stale data from RAM.
                if (0 == pRestore->dwWriteOffset) {
                    memcpy(&pRestore->data[offsetof(fslog_t, dwRestoreFlags)],
                           &FlushStruct, sizeof(FSLogFlushSettings));
                }
                
                if (!PHD_WriteFileWithSeek (pfsmap->phdFile, &pRestore->data[0],
                                            cbPageSize, &cbReadWritten, 0,
                                            pRestore->dwWriteOffset, 0)
                    || (cbReadWritten != cbPageSize)) {
                    DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: ValidateFile: failure to commit page! pos=0x%08x\r\n",
                                             pRestore->dwWriteOffset));
                    
                    // We have moved some, but not all, of the data into its
                    // proper place in the file.  The rest we cannot move because
                    // we're out of disk space.  However, the data is still stored
                    // and available at the end of the file.  The RestoreFlag is
                    // still RESTORE_FLAG_FLUSHED, so on the next flush we'll again
                    // attempt to validate the file.  Note that this will redo some
                    // work that has already been done, but it does us no harm.  In
                    // the meantime the current state of the map is still buffered
                    // in RAM, and the dirty pages are still marked as "dirty."
                    // That prevents them from being paged out, which would leave
                    // us open to disastrous pageins of stale data from the main
                    // section of the file.  If disk space is freed up at some point
                    // in the future, a flush will succeed and the data will be
                    // committed.  If the mapfile is unmounted before space is
                    // cleared up, changes that are made to the map after this
                    // partial flush may be lost.  We will resume our attempts to
                    // flush during the volume mount, and will not allow the file
                    // to be mapped until it is fully flushed.  That again prevents
                    // us from disastrous pageins of stale data from the file.  So
                    // if no space is available when the file is next mapped, it
                    // will be unusable.
                    fSuccess = FALSE;
                    break;
                }
    
                dwReadOffset += cbBuffer;
            }
    
            if (!PHD_FlushFileBuffers (pfsmap->phdFile)) {
                DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: ValidateFile: Flush 2nd-phase commit failed\r\n"));
                fSuccess = FALSE;
            }
        
            NKfree (pBuffer);
            if (!fSuccess) {
                goto error;
            }
            
            DEBUGMSG(ZONE_MAPFILE, (L"MMFILE: ValidateFile: second flush phase complete\r\n"));
        }

        // intentionally fall through
        __fallthrough;
    }

    case FSLOG_RESTORE_FLAG_UNFLUSHED:

        // Clear the restore flag and page count
        FlushStruct.dwRestoreFlags = FSLOG_RESTORE_FLAG_NONE;
        FlushStruct.dwRestoreSize  = VM_PAGE_SIZE;
        if (!PHD_WriteFileWithSeek (pfsmap->phdFile, &FlushStruct,
                                    sizeof(FSLogFlushSettings), &cbReadWritten,
                                    0, offsetof(fslog_t, dwRestoreFlags), 0)
            || (sizeof(FSLogFlushSettings) != cbReadWritten)) {
            DEBUGMSG(ZONE_MAPFILE, (L"MMFILE: ValidateFile: Failed to clear flush flags\r\n"));                    
            
            // This failure will result in another flush attempt later, but
            // no data has been lost.
            goto error;
        }

        if (!PHD_FlushFileBuffers (pfsmap->phdFile)) {
            DEBUGMSG(ZONE_MAPFILE, (L"MMFILE: ValidateFile: Failed to commit clean flush state\r\n"));
            goto error;
        }
        break;

    case FSLOG_RESTORE_FLAG_NONE:
        break;

    default:
        // Even on new maps the flags should be valid; initialized by filesys
        DEBUGCHK(0);  // File is corrupt?
        goto error;
    }

    CELOG_MapFileValidate(pfsmap->liSize.LowPart, FlushStruct.dwRestoreSize,
                          FlushStruct.dwRestoreFlags, TRUE);

    return ERROR_SUCCESS;

error:
    {
        // Guarantee that we never return ERROR_SUCCESS on failure
        DWORD dwErr = KGetLastError(pCurThread);
        if (ERROR_SUCCESS == dwErr) {
            dwErr = ERROR_FILE_INVALID;
        }
        return dwErr;
    }
}


// Callback from FlushPages
static BOOL
AtomicProlog (
    FlushParams* pParams
    )
{
    DWORD cbReadWritten;

    // sanity check
    DEBUGCHK(!(pParams->pfsmap->dwFlags & (MAP_DIRECTROM | MAP_FILECACHE | MAP_GATHER)));
    DEBUGCHK(pParams->pfsmap->dwFlags & MAP_ATOMIC);

    //
    // Read and update restore data
    //

    // Read the flush status out of the file
    if (!PHD_ReadFileWithSeek (pParams->pfsmap->phdFile, &pParams->FileStruct,
                               sizeof(FSLogFlushSettings), &cbReadWritten, 0,
                               offsetof(fslog_t, dwRestoreFlags), 0)
        || (sizeof(FSLogFlushSettings) != cbReadWritten)) {
        // We can exit here because nothing has been changed.
        DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: AtomicProlog: Failed to read flush status from file\r\n"));
        DEBUGCHK (0);  // Shouldn't happen
        return FALSE;
    }

    // If a previous flush is incomplete, it must be cleaned up before
    // continuing.  Typically this happens in an out-of-disk scenario.
    if (pParams->FileStruct.dwRestoreFlags != FSLOG_RESTORE_FLAG_NONE) {
        DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: AtomicProlog: Resuming/backing out previous logged flush (flags=0x%08x, start=0x%08x, pages=0x%04x)\r\n",
                                pParams->FileStruct.dwRestoreFlags,
                                pParams->FileStruct.dwRestoreStart,
                                pParams->FileStruct.dwRestoreSize >> 16));
        if (ValidateFile (pParams->pfsmap) != ERROR_SUCCESS) {
            // Cannot do any more flushing until the file is valid
            return FALSE;
        }
    }

    // Record the fact that a flush is in progress
    pParams->FileStruct.dwRestoreFlags = FSLOG_RESTORE_FLAG_UNFLUSHED;
    pParams->FileStruct.dwRestoreSize = VM_PAGE_SIZE;
    if (!PHD_WriteFileWithSeek (pParams->pfsmap->phdFile, &pParams->FileStruct,
                                sizeof(FSLogFlushSettings), &cbReadWritten, 0,
                                offsetof(fslog_t, dwRestoreFlags), 0)
        || (sizeof(FSLogFlushSettings) != cbReadWritten)) {
        // We can exit here because nothing has been changed.
        DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: AtomicProlog: Failed to write flush status to file\r\n"));
        return FALSE;
    }

    // Flush the dirty blocks in the cache back to the file
    if (!PHD_FlushFileBuffers (pParams->pfsmap->phdFile)) {
        DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: AtomicProlog: Failed to commit file flush status\r\n"));
        return FALSE;
    }

    // Views to atomic maps always start at the front of the map, thus the
    // first page of the view is the first page of the map, which contains
    // the fslog struct
    DEBUGCHK (pParams->pViewBase->liMapOffset.QuadPart == 0);

    return TRUE;
}


// Callback from FlushPages
static DWORD
AtomicWritePages (
    FlushParams* pParams,
    DWORD        cPages
    )
{
    // Atomic maps require page alignment
    DEBUGCHK (pParams->pViewBase->liMapOffset.HighPart == 0);
    DEBUGCHK ((pParams->pViewBase->liMapOffset.LowPart % VM_PAGE_SIZE) == 0);
    DEBUGCHK ((pParams->pViewBase->cbSize % VM_PAGE_SIZE) == 0);

    // Flush the pages in memory back to the file.  We write the new pages to
    // the end of the file for safety, and only copy them into their rightful
    // places when we are done, inside ValidateFile.

    // Limit 64K because we can't overflow dwRestoreSize's count WORD.
    // MapViewOfFile should not allow atomic maps over 64*1024*VM_PAGE_SIZE.
    DEBUGCHK (cPages < 64*1024);

    if (cPages && (cPages < 64*1024)) {
        DWORD idxPage;
        DWORD cbWritten, dwWriteOffset, dwFilePos;

        DEBUGCHK (pParams->pfsmap->phdFile);
        dwFilePos = pParams->FileStruct.dwRestoreStart;

        for (idxPage = 0; idxPage < cPages; idxPage++) {
            dwWriteOffset = (DWORD) pParams->pFlush->nogather.prgPages[idxPage]
                            - (DWORD) pParams->pViewBase->pBaseAddr;

            DEBUGMSG (ZONE_MAPFILE && ZONE_PAGING,
                      (L"MMFILE: AtomicWritePages: Writing dirty page %8.8lx\r\n",
                       pParams->pFlush->nogather.prgPages[idxPage]));

#ifdef DEBUG
            // If it's the first page, make sure the restore data is correct.
            if (0 == dwWriteOffset) {
                FSLogFlushSettings* pMemStruct = (FSLogFlushSettings*)
                    ((DWORD) pParams->pViewBase->pBaseAddr + offsetof(fslog_t, dwRestoreFlags));

                DEBUGCHK (pMemStruct->dwRestoreFlags == FSLOG_RESTORE_FLAG_UNFLUSHED);
                DEBUGCHK (pMemStruct->dwRestoreSize >> 16 == 0);
                // File and memory should be in sync
                DEBUGCHK (pParams->FileStruct.dwRestoreFlags == pMemStruct->dwRestoreFlags);
                DEBUGCHK (pParams->FileStruct.dwRestoreStart == pMemStruct->dwRestoreStart);
                DEBUGCHK (pParams->FileStruct.dwRestoreSize  == pMemStruct->dwRestoreSize);
            }
#endif // DEBUG
    
            // Write the page offset to the end of the file
            if (!PHD_WriteFileWithSeek (pParams->pfsmap->phdFile, &dwWriteOffset, sizeof(DWORD),
                                        &cbWritten, 0, dwFilePos, 0)
                || (sizeof(DWORD) != cbWritten)) {
                // We can exit here because nothing has really changed.
                DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: AtomicWritePages: Failed to write page offset\r\n"));
                return 0;
            }
            dwFilePos += sizeof(DWORD);

            // Now write the page of data following the offset
            if (!PHD_WriteFileWithSeek (pParams->pfsmap->phdFile,
                                        pParams->pFlush->nogather.prgPages[idxPage],
                                        VM_PAGE_SIZE, &cbWritten, 0, dwFilePos, 0)
                || (VM_PAGE_SIZE != cbWritten)) {
                // We can exit here because nothing has really changed.
                DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: AtomicWritePages: Failed to write page\r\n"));
                return 0;
            }
            dwFilePos += VM_PAGE_SIZE;
        
            DEBUGMSG (ZONE_MAPFILE && ZONE_PAGING,
                      (L"MMFILE: AtomicWritePages: Flushed map 0x%08x page 0x%08x (offset=%u, count=%u)\r\n",
                       pParams->pfsmap, pParams->pFlush->nogather.prgPages[idxPage],
                       dwWriteOffset, idxPage));
        }
    
        DEBUGCHK (pParams->pViewBase->dwDirty >= cPages);
        InterlockedExchangeAdd ((LONG*)&pParams->pViewBase->dwDirty, - (int) cPages);
        return cPages;
    }

    return 0;
}


// Callback from FlushPages
static BOOL
AtomicEpilog (
    FlushParams* pParams,
    BOOL         fSuccess,
    DWORD        cTotalPages,
    DWORD        idxLastPage
    )
{
    DEBUGCHK (pParams->pfsmap->phdFile);

    // Flush the dirty blocks in the cache back to the file
    if (!PHD_FlushFileBuffers (pParams->pfsmap->phdFile)) {
        DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: AtomicEpilog: Flush 1st-phase commit failed\r\n"));
        fSuccess = FALSE;
    }

    if (fSuccess) {
        DWORD cbWritten;

        //
        // Update restore data again
        //

        // Record the fact that the first phase succeeded
        pParams->FileStruct.dwRestoreFlags = FSLOG_RESTORE_FLAG_FLUSHED;
        pParams->FileStruct.dwRestoreSize |= (cTotalPages << 16);
        if (!PHD_WriteFileWithSeek (pParams->pfsmap->phdFile,
                                    &pParams->FileStruct, sizeof(FSLogFlushSettings),
                                    &cbWritten, 0, offsetof(fslog_t, dwRestoreFlags), 0)
            || (sizeof(FSLogFlushSettings) != cbWritten)) {
            // We can exit here because nothing has really changed.
            DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: AtomicEpilog: Failed to write flush status to file\r\n"));
            fSuccess = FALSE;
        }
    
        // Flush the dirty blocks in the cache back to the file
        if (!PHD_FlushFileBuffers (pParams->pfsmap->phdFile)) {
            DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: AtomicEpilog: Failed to commit file flush status\r\n"));
            fSuccess = FALSE;
        }
    
        DEBUGMSG (fSuccess && ZONE_MAPFILE,
                  (L"MMFILE: AtomicEpilog: First flush phase complete\r\n"));
    
        // Commit the flush.
        if (fSuccess && (ERROR_SUCCESS != ValidateFile (pParams->pfsmap))) {
            // We cannot validate the file, so we must fail the flush!
            fSuccess = FALSE;
        }
    }
    
    if (!fSuccess) {
        // Failed to flush all pages
        DWORD idxPage;

        // Mark all of the pages that were being flushed R/W again
        for (idxPage = 0; idxPage < cTotalPages; idxPage++) {
            DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: AtomicEpilog: Restoring dirty page %8.8lx\r\n",
                                     pParams->pFlush->nogather.prgPages[idxPage]));
            VERIFY (VMProtect (pParams->pprc,
                               pParams->pFlush->nogather.prgPages[idxPage],
                               VM_PAGE_SIZE, PAGE_READWRITE, NULL));
        }
    }
    
    return fSuccess;
}


//------------------------------------------------------------------------------
// SIMPLE FLUSHES
//
// Simple = non-gathered non-atomic.
// Write data back to the file without any guarantee of atomicity.
// File cache flushes use the simple flush too.
//------------------------------------------------------------------------------


// Callback from VMMapClearDirtyPages
// Used by both atomic and simple flushes
static BOOL
NonGatherRecordPage (
    FlushParams* pParams,
    PVOID        pAddr,    // Page start address
    DWORD        idxPage   // Which dirty page # this is, first dirty page is 0
    )
{
    // Atomic maps require page alignment
    DEBUGCHK (!(pParams->pfsmap->dwFlags & MAP_ATOMIC) || (((DWORD)pAddr % VM_PAGE_SIZE) == 0));
    DEBUGCHK (pParams->pfsmap->phdFile || (pParams->pfsmap->dwFlags & MAP_FILECACHE));

    // Sanity check: atomic views should not have any dirty data past dwRestoreStart
    DEBUGCHK(!(pParams->pfsmap->dwFlags & MAP_ATOMIC)
             || ((LPBYTE)pAddr <= pParams->pViewBase->pBaseAddr + pParams->FileStruct.dwRestoreStart));

    if ((pParams->pfsmap->dwFlags & MAP_ATOMIC) && (pAddr == pParams->pViewBase->pBaseAddr)) {
        // The first page of an atomic map contains a copy of the flush state,
        // which we read directly from the disk.  Ensure that the disk copy does not get
        // overwritten during the flush.  This write can be done here because
        // VMMapClearDirtyPages has not yet made the page R/O.
        memcpy (pParams->pViewBase->pBaseAddr + offsetof(fslog_t, dwRestoreFlags),
                &pParams->FileStruct, sizeof(FSLogFlushSettings));
    }

    if (idxPage < pParams->pFlush->dwNumElements) {
        pParams->pFlush->nogather.prgPages[idxPage] = pAddr;
        DEBUGMSG (ZONE_MAPFILE && ZONE_PAGING, (L"MMFILE: Prepare to flush page %8.8lx (non-gather)\r\n", pAddr));
        return TRUE;
    }
    
    // Number of pages to write should never exceed number of segments available
    DEBUGCHK(0);
    return FALSE;
}


// Callback from FlushPages
DWORD SimpleWritePages (
    FlushParams* pParams,
    DWORD        cPages
    )
{
    DWORD idxPage = 0;
    DWORD cbToWrite, cbWritten, dwViewOffset;
    ULARGE_INTEGER liFileOffset;
    PVOID* prgPages = pParams->pFlush->nogather.prgPages;

    // sanity check
    DEBUGCHK(!(pParams->pfsmap->dwFlags & (MAP_DIRECTROM | MAP_GATHER | MAP_ATOMIC)));
    DEBUGCHK (pParams->pfsmap->phdFile || (pParams->pfsmap->dwFlags & MAP_FILECACHE));

    while (idxPage < cPages) {

        PVOID pStartAddress = prgPages[idxPage];
        cbToWrite = VM_PAGE_SIZE;

        // Find all consecutive pages to write at once
        while ((idxPage < (cPages - 1)) &&
               (((DWORD)prgPages[idxPage] + VM_PAGE_SIZE) == (DWORD)prgPages[idxPage+1])) {
            idxPage++;
            cbToWrite += VM_PAGE_SIZE;
        }

        // Make sure the write doesn't run past the end of the view size
        dwViewOffset = (DWORD) pStartAddress - (DWORD) pParams->pViewBase->pBaseAddr;
        if ((dwViewOffset + cbToWrite) > pParams->pViewBase->cbSize) {
            cbToWrite = pParams->pViewBase->cbSize - dwViewOffset;
        }

        DEBUGMSG (ZONE_MAPFILE && ZONE_PAGING, (L"MMFILE: SimpleWritePages: Writing dirty pages starting at %8.8lx.  Bytes = %8.8lx.\r\n",
                                                pStartAddress, cbToWrite));
        
        liFileOffset.QuadPart = pParams->pViewBase->liMapOffset.QuadPart + dwViewOffset;
        if (pParams->pfsmap->dwFlags & MAP_FILECACHE) {
            // File cache
            DEBUGCHK (!IsKernelVa (pStartAddress));  // Don't hand static-mapped VA to file system
            DEBUGCHK (pActvProc == g_pprcNK);  // Never call out to filesys if active proc != NK
            if (!g_FSCacheFuncs.pReadWriteCacheData (pParams->pfsmap->FSSharedFileMapId,
                                                     pStartAddress,
                                                     cbToWrite, &cbWritten,
                                                     &liFileOffset, TRUE)) {
                DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: SimpleWritePages: Failed to write cache page %8.8lx; error=%8.8lx\r\n",
                                         pStartAddress, KGetLastError (pCurThread)));
                return idxPage;
            }
        } else {
            // Regular file: not atomic, not file cache, not gathered.
            if (!PHD_WriteFileWithSeek (pParams->pfsmap->phdFile,
                                        pStartAddress,
                                        cbToWrite, &cbWritten, NULL,
                                        liFileOffset.LowPart, liFileOffset.HighPart)) {
                DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: SimpleWritePages: Failed to write dirty page %8.8lx; error=%8.8lx\r\n",
                                         pStartAddress, KGetLastError (pCurThread)));
                return idxPage;
            }
        }

        DEBUGCHK(pParams->pViewBase->dwDirty >= (cbWritten >> VM_PAGE_SHIFT));
        InterlockedExchangeAdd ((LONG*)&pParams->pViewBase->dwDirty, - (int) (cbWritten >> VM_PAGE_SHIFT));

        if (cbToWrite != cbWritten) {
            // Failed to write the entire request.
            DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: SimpleWritePages: Failed to write all dirty pages %8.8lx!\r\n",
                                     (DWORD)pStartAddress + cbWritten));
            return idxPage;
        }
        
        idxPage++;
    }

    return idxPage;
}


// Callback from FlushPages
static BOOL
SimpleEpilog (
    FlushParams* pParams,
    BOOL         fSuccess,
    DWORD        cTotalPages,
    DWORD        idxLastPage
    )
{
    DEBUGCHK (pParams->pfsmap->phdFile || (pParams->pfsmap->dwFlags & MAP_FILECACHE));

    // Flush the dirty blocks in the cache back to the file
    if (pParams->pfsmap->dwFlags & MAP_FILECACHE) {
        // File cache
        DEBUGCHK (pActvProc == g_pprcNK);  // Never call out to filesys if active proc != NK
        if (!g_FSCacheFuncs.pFlushFile (pParams->pfsmap->FSSharedFileMapId)) {
            DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: SimpleEpilog: Cache flush commit failed\r\n"));
            fSuccess = FALSE;
        }
    } else {
        if (!PHD_FlushFileBuffers (pParams->pfsmap->phdFile)) {
            DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: SimpleEpilog: Flush commit failed\r\n"));
            fSuccess = FALSE;
        }
    }
    
    if (!fSuccess) {
        // Failed to flush all pages
        // Mark the first failed page and all of those following it R/W again
        for ( ; idxLastPage < cTotalPages; idxLastPage++) {
            DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: SimpleEpilog: Restoring dirty page %8.8lx\r\n",
                                     pParams->pFlush->nogather.prgPages[idxLastPage]));
            VERIFY (VMProtect (pParams->pprc, pParams->pFlush->nogather.prgPages[idxLastPage],
                               VM_PAGE_SIZE, PAGE_READWRITE, NULL));
        }
    }
    
    return fSuccess;
}


//------------------------------------------------------------------------------
// Common flush implementation
//------------------------------------------------------------------------------

static BOOL
WriteBackPageableView (
    PPROCESS pprc,
    CommonMapView_t* pViewBase,
    ViewFlushInfo_t* pFlush,
    PFSMAP   pfsmap,
    __in_bcount(cbSize) LPBYTE   pAddr,
    DWORD    cbSize
    )
{
    BOOL fSuccess = FALSE;

    if (!(pfsmap->dwFlags & PAGE_READWRITE)
        || (0 == pViewBase->dwDirty)) {
        return TRUE;

    } else {
        MapFlushFunctions funcs;
        FlushParams params;
        DWORD cbExtra;

        DEBUGCHK (!(pfsmap->dwFlags & MAP_DIRECTROM));

        // Figure out the start and length, page-aligned
        if (!cbSize) {
            // NOTENOTE BC behavior would be to flush to the end of the entire
            // map, not just the view
            cbSize = pViewBase->cbSize;
        }
        cbExtra = (DWORD) pAddr & (VM_PAGE_SIZE-1);
        pAddr -= cbExtra;
        cbSize += cbExtra;
        if (cbSize > pViewBase->cbSize - (pAddr - pViewBase->pBaseAddr)) {
            cbSize = pViewBase->cbSize - (pAddr - pViewBase->pBaseAddr);
        }
        DEBUGCHK(pAddr >= pViewBase->pBaseAddr);
        DEBUGCHK(pAddr + cbSize <= pViewBase->pBaseAddr + pViewBase->cbSize);

        // Prepare to do the right type of flush
        if (pfsmap->dwFlags & MAP_GATHER) {
            // Gather
            DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: DoFlushView: Begin gathered flush\r\n"));
            funcs.pfnProlog     = GatherProlog;
            funcs.pfnEpilog     = GatherEpilog;
            funcs.pfnRecordPage = GatherRecordPage;
            funcs.pfnWritePages = GatherWritePages;
        } else if (pfsmap->dwFlags & MAP_ATOMIC) {
            // Atomic
            DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: DoFlushView: Begin atomic flush\r\n"));
            funcs.pfnProlog     = AtomicProlog;
            funcs.pfnEpilog     = AtomicEpilog;
            funcs.pfnRecordPage = NonGatherRecordPage;
            funcs.pfnWritePages = AtomicWritePages;
        } else {
            // Simple / File Cache
            DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: DoFlushView: Begin %s flush\r\n",
                                     (pfsmap->dwFlags & MAP_FILECACHE) ? L"file-cache" : L"simple"));
            funcs.pfnProlog     = NULL;
            funcs.pfnEpilog     = SimpleEpilog;
            funcs.pfnRecordPage = NonGatherRecordPage;
            funcs.pfnWritePages = SimpleWritePages;
        }

        params.pprc      = pprc;
        params.pfsmap    = pfsmap;
        params.pViewBase = pViewBase;
        params.pFlush    = pFlush;

        // NOTENOTE it is possible that another thread will dirty another page
        // in the view during the flush.  But that should work out safely.

        // Write dirty pages out to the file
        if (!funcs.pfnProlog || funcs.pfnProlog (&params)) {
            DWORD cPages;
            DWORD idxLastPage = 0;

            // Making the page read-only here means that we will have to go back
            // and make the page read/write again if we hit any failures.  
            
            cPages = VMMapClearDirtyPages (pprc, pAddr, cbSize, funcs.pfnRecordPage, &params);
            if ((DWORD)-1 == cPages) {
                // Failed to clear dirty pages, because pages in the view are locked
                cPages = 0;  // Avoid confusing the epilog function

            } else if (0 == cPages) {
                // No dirty pages to flush.  There are dirty pages elsewhere in
                // the view, but not inside the range that's being flushed.
                fSuccess = TRUE;
                
                // Atomic view should be flushed all at once and have no data past dwRestoreStart
                DEBUGCHK (!(pfsmap->dwFlags & MAP_ATOMIC));
            
            } else {
                DEBUGCHK (cPages <= pViewBase->dwDirty);

                idxLastPage = funcs.pfnWritePages (&params, cPages);
                if (idxLastPage == cPages) {
                    fSuccess = TRUE;
                }
            }

            // Call the epilog whether the flush succeeded or failed
            DEBUGMSG (ZONE_MAPFILE && fSuccess,
                      (L"MMFILE: DoFlushView: Flushed %u pages\r\n", cPages));
            fSuccess = funcs.pfnEpilog (&params, fSuccess, cPages, idxLastPage);
        }
    }

    return fSuccess;
}


//
// DoFlushView - worker function to flush a view. The view must have been locked
//
DWORD DoFlushView (
    PPROCESS pprc,
    PFSMAP   pfsmap,
    CommonMapView_t* pViewBase,
    ViewFlushInfo_t* pFlush,
    LPBYTE   pAddr,
    DWORD    cbSize,
    DWORD    FlushFlags
    )
{
    BOOL   fSuccess = TRUE;
    DWORD  dwErr;

    DEBUGCHK (!OwnCS (&MapCS));

    // Memory-backed view, no flushing
    if (!pfsmap->phdFile && !(pfsmap->dwFlags & MAP_FILECACHE)) {
        return ERROR_SUCCESS;
    }

    if (!(pfsmap->dwFlags & MAP_PAGEABLE)) {
        // Non-pageable, write-back the whole view but don't decommit anything
        if (!(pfsmap->dwFlags & PAGE_READWRITE)) {
            // Read-only view, no flushing
            return ERROR_SUCCESS;
        
        } else if (FlushFlags & MAP_FLUSH_WRITEBACK) {
            DWORD cbWritten;

            DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: DoFlushView: Begin non-pageable flush\r\n"));
            
            CELOG_MapFileViewFlush (pprc, pfsmap, pViewBase->pBaseAddr, pViewBase->cbSize,
                                    PAGECOUNT(pViewBase->cbSize), TRUE);

            EnterCriticalSection (&FlushCS);
            fSuccess = PHD_SetLargeFilePointer (pfsmap->phdFile, pViewBase->liMapOffset)
                       && PHD_WriteFile (pfsmap->phdFile, pViewBase->pBaseAddr, pViewBase->cbSize, &cbWritten, NULL)
                       && (cbWritten == pViewBase->cbSize);
            LeaveCriticalSection (&FlushCS);

            CELOG_MapFileViewFlush (pprc, pfsmap, pViewBase->pBaseAddr, pViewBase->cbSize,
                                    PAGECOUNT(pViewBase->cbSize), FALSE);
        }
    
    } else {
        // Pageable
        
        // The FlushCS prevents two flushes on the same fsmap at the same time.
        // Else one thread could mark pages as R/O and start writing, while another
        // comes in and decommits those pages before they're written.
        if (pfsmap->dwFlags & MAP_FILECACHE) {
            DEBUGCHK (pActvProc == g_pprcNK);  // Never call out to filesys if active proc != NK
            g_FSCacheFuncs.pLockUnlockFile (pfsmap->FSSharedFileMapId, TRUE);
        } else {
            EnterCriticalSection (&FlushCS);
        }
        CELOG_MapFileViewFlush (pprc, pfsmap, pAddr, cbSize, pViewBase->dwDirty, TRUE);
        DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: DoFlushView: %u dirty pages in view\r\n", pViewBase->dwDirty));

        // Write back dirty pages
        if (FlushFlags & MAP_FLUSH_WRITEBACK) {
            fSuccess = WriteBackPageableView (pprc, pViewBase, pFlush, pfsmap, pAddr, cbSize);
        }

        // Decommit R/O pages
        if (FlushFlags & (MAP_FLUSH_DECOMMIT_RO | MAP_FLUSH_DECOMMIT_RW)) {
            // This call returns failure if any committed pages remain.  Besides write-back
            // failures, it could fail if another thread pages in a R/W page after we flush.
            if (!MapVMDecommit (pfsmap, pprc, pViewBase->pBaseAddr, pViewBase->liMapOffset,
                                pViewBase->cbSize, (FlushFlags & MAP_FLUSH_DECOMMIT_RW))
                && (FlushFlags & MAP_FLUSH_FAIL_IF_COMMITTED)) {
                fSuccess = FALSE;
            }
        }

        CELOG_MapFileViewFlush (pprc, pfsmap, pAddr, cbSize, pViewBase->dwDirty, FALSE);
        if (pfsmap->dwFlags & MAP_FILECACHE) {
            DEBUGCHK (pActvProc == g_pprcNK);  // Never call out to filesys if active proc != NK
            g_FSCacheFuncs.pLockUnlockFile (pfsmap->FSSharedFileMapId, FALSE);
        } else {
            LeaveCriticalSection (&FlushCS);
        }
    }

    if (fSuccess) {
        dwErr = ERROR_SUCCESS;
    } else {
        // Guarantee that we never return ERROR_SUCCESS on failure
        dwErr = KGetLastError (pCurThread);
        if (ERROR_SUCCESS == dwErr) {
            dwErr = ERROR_OUTOFMEMORY;
        }
    }

    DEBUGMSG (ZONE_MAPFILE, (L"MMFILE: DoFlushView: Flush returns, dwErr %8.8lx\r\n", dwErr));
    return dwErr;
}

