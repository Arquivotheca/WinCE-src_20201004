//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 *              NK Kernel loader code
 *
 *
 * Module Name:
 *
 *              mapfile.c
 *
 * Abstract:
 *
 *              This file implements the NK kernel mapped file routines
 *
 */

#include "kernel.h"

#ifndef _PREFAST_
#pragma warning(disable: 4068) // Disable pragma warnings
#endif

DWORD rbRes;
DWORD rbSubRes[NUM_MAPPER_SECTIONS];
extern CRITICAL_SECTION VAcs, PagerCS, MapCS, MapNameCS, WriterCS;
HANDLE hMapList;

extern DWORD g_fSysFileReadable;


BOOL SC_MapCloseHandle(HANDLE hMap);
BOOL ValidateFile(LPFSMAP lpm);
BOOL FlushMapBuffersLogged(LPFSMAP lpm, DWORD dwOffset, DWORD dwLength, DWORD dwFlags);

const PFNVOID MapMthds[] = {
    (PFNVOID)SC_MapCloseHandle,
    (PFNVOID)0,
    (PFNVOID)SC_MapViewOfFile,
};

const CINFO cinfMap = {
    "FMAP",
    DISPATCH_KERNEL_PSL,
    HT_FSMAP,
    sizeof(MapMthds)/sizeof(MapMthds[0]),
    MapMthds
};

#define SUB_MAPPER_INCR (MAPPER_INCR/32)
#define MapBits(n) ((n) == 0x1f ? 0xffffffff : (1<<((n)+1))-1)

#define IsFromFilesys()     (1 == pCurProc->procnum)


// The pDirty list is an array of BYTEs, one bit per page (1=dirty, 0=clean)
// DIRTY_INDEX is the index of the byte containing the bit for a given page number
#define DIRTY_INDEX(dwPage)   ((dwPage) / 8)
// DIRTY_MASK is the mask for getting the bit for a given page number from its byte
// (page & (8-1)) is faster than (page % 8)
#define DIRTY_MASK(dwPage)    (1 << ((dwPage) & (8-1)))


///////////////////////////////////////////
// Definitions for logged flush
//

// A set of flags and values are stored at the front of the file, on the volume
// log page, to track mapped flush progress.  Only dwRestoreStart (which we do
// not modify) is used by filesys.

ERRFALSE(offsetof(fslog_t, dwRestoreStart) == offsetof(fslog_t, dwRestoreFlags) + 4);
ERRFALSE(offsetof(fslog_t, dwRestoreSize) == offsetof(fslog_t, dwRestoreFlags) + 8);

typedef struct {
    DWORD dwRestoreFlags;
    DWORD dwRestoreStart;
    DWORD dwRestoreSize;
} FlushSettings;

#define RESTORE_FLAG_NONE      0
#define RESTORE_FLAG_UNFLUSHED 1
#define RESTORE_FLAG_FLUSHED   2

// All data is written to the end of the file and tagged with its proper offset
// inside the file.  Once all pages are flushed, they are committed by copying
// them to their proper offsets.
typedef struct {
    DWORD dataoffset;
    BYTE restorepage[4096]; // must be last!
} RestoreInfo;

///////////////////////////////////////////


static BOOL FSWriteFile (HANDLE hFile, LPVOID lpBuffer, DWORD nBytes, LPDWORD pBytesWritten)
{
    BOOL fRet;
    SetNKCallOut (pCurThread);
    fRet = WriteFile (hFile, lpBuffer, nBytes, pBytesWritten, 0);
    ClearNKCallOut (pCurThread);
    return fRet;
}


//------------------------------------------------------------------------------
// reserves a memory region of length len
// rbRes: each bit indicate a 32MB of VM slot.
// rbSubRes: each it indicate 1MB within each 32MB slot
//------------------------------------------------------------------------------
LPVOID FSMapMemReserve(
    DWORD len,
    BOOL fAddProtect
    )
{
    DWORD  secs;
    DWORD  first, curr, trav;
    LPVOID retval = 0;
    
    DEBUGCHK(len);
    if (!len)
        return NULL;
    
    DEBUGMSG(ZONE_VIRTMEM || ZONE_MAPFILE,
             (L"FSMapMemReserve: len = 0x%x, fAddProtect = 0x%x, pCurProc->aky = 0x%x\n",
              len, fAddProtect, pCurProc->aky));

    EnterCriticalSection(&VAcs);
    
    if (len >= MAPPER_INCR) {
        // if the requested size is > 32M, find the consective free sections that can
        // satisfy the request.
        
        secs = (len + MAPPER_INCR - 1)/MAPPER_INCR; // # of 32 M sections required
        first = 0;

        // try to find a consective 32M slots that can hold the requested allocation
        for (curr = 0; curr <= NUM_MAPPER_SECTIONS - secs; curr++) {
            if (rbRes & (1<<curr))
                first = curr+1;
            else if (curr - first + 1 == secs)
                break;
        }

        // not consective VM available, fail the call
        if (curr > NUM_MAPPER_SECTIONS - secs)
            goto exit;

        // create the mapper section for each 32M slot
        for (trav = first; trav <= curr; trav++) {
            if (!CreateMapperSection(FIRST_MAPPER_ADDRESS+(MAPPER_INCR*trav), fAddProtect)) {
                // if we fail to create all the mapper section, undo the allocations
                // and fail the call
                while (trav-- != first) {
                    DeleteMapperSection(FIRST_MAPPER_ADDRESS+(MAPPER_INCR*trav));
                    rbRes &= ~(1<<trav);
                }
                
                goto exit;
            }

            // mark the 32M slot in use
            rbRes |= (1<<trav);

            // if this is the last 32M slot we allocate, mark the rbSubRes bits only for
            // the MB's we use. Otherwise set it to -1 (all in use)
            rbSubRes[trav] = (trav == curr
                              ? MapBits(((len - 1) & (MAPPER_INCR-1))/SUB_MAPPER_INCR)  
                              : (DWORD)-1);
        }
        
        retval = (LPVOID)(FIRST_MAPPER_ADDRESS + (MAPPER_INCR*first));
    
    } else {

        // if the request size is < 32M, start with what's left in the last 32M block
        PSECTION pscn;
        DWORD aky = fAddProtect? pCurProc->aky : ProcArray[0].aky;

        secs = (len + SUB_MAPPER_INCR - 1) / SUB_MAPPER_INCR;   // # of 1MB blocks needed

        // look through all the existing mapper sections to see if there is any holes
        // that could be used for this allocation.
        for (curr = 0; curr < NUM_MAPPER_SECTIONS; curr++) {
            if (rbRes & (1<<curr)) {
                pscn = SectionTable[curr+(MAX_PROCESSES+RESERVED_SECTIONS)];
                DEBUGCHK (pscn && (pscn != NULL_SECTION) && ((*pscn)[0] != RESERVED_BLOCK));

                // check if we have the right to use this section
                if (!TestAccess (&((*pscn)[0]->alk), &aky)) {
                    DEBUGMSG(ZONE_VIRTMEM || ZONE_MAPFILE,
                             (L"FSMapMemReserve: No Access to mapper section %d (%x, %x), try the next one\n",
                              curr, (*pscn)[0]->alk, aky));
                    continue;
                }
                
                first = 0;
                
                for (trav = 0; trav <= 32-secs; trav++) {
                    if (rbSubRes[curr] & (1<<trav))
                        first = trav+1;
                    else if (trav - first == secs)
                        break;  // found a 'hole' big enough for the allocation
                }
                
                if (trav <= 32-secs)
                    break;
            }
        }

        // if there is no hole big enough, we need to create a new mapper section.
        if (curr == NUM_MAPPER_SECTIONS) {

            // find an unused 32M slot and create the section for it.
            for (curr = 0; curr < NUM_MAPPER_SECTIONS; curr++) {
                if (!(rbRes & (1<<curr))) {
                    if (!CreateMapperSection(FIRST_MAPPER_ADDRESS + (MAPPER_INCR*curr), fAddProtect))
                        goto exit;

                    // mark the seciton in use and set the proper sub-bits.
                    rbRes |= (1<<curr);
                    rbSubRes[curr] = MapBits(((len-1)/SUB_MAPPER_INCR));
                    retval = (LPVOID)(FIRST_MAPPER_ADDRESS + (MAPPER_INCR*curr));
                    
                    break;
                }
            }
        
        } else {

            // we found a hole enough to satisfy the allocation, just mark the sub-bits
            // and we're done.
            rbSubRes[curr] |= (MapBits(((len-1)/SUB_MAPPER_INCR)) << first);
            retval = (LPVOID)(FIRST_MAPPER_ADDRESS + (MAPPER_INCR*curr)
                              + (SUB_MAPPER_INCR*first));
        }
    }
    
exit:
    
    LeaveCriticalSection(&VAcs);
    DEBUGMSG(ZONE_VIRTMEM || ZONE_MAPFILE, (L"FSMapMemReserve: returns %8.8lx\n", retval));
    return retval;
}


CLEANEVENT *pHugeCleanList;



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID 
HugeVirtualReserve(
    DWORD dwSize,
    DWORD dwFlags  // Flags from DoVirtualAlloc
    )
{
    LPCLEANEVENT lpce;
    LPVOID pMem;
    BOOL fAddProtect;
    
    dwSize = PAGEALIGN_UP(dwSize);
    
    if (!(lpce = AllocMem(HEAP_CLEANEVENT))) {
        KSetLastError(pCurThread, ERROR_OUTOFMEMORY);
        return 0;
    }

    fAddProtect = !(dwFlags & MEM_SHAREDONLY) && IsFromFilesys();

    // special casing filesys. We'll add protection for filesys in the 2nd Gig
    // to prevent accidentental filesys corruption.
    if (!(pMem = FSMapMemReserve(dwSize, fAddProtect))) {
        FreeMem(lpce, HEAP_CLEANEVENT);
        KSetLastError(pCurThread, ERROR_OUTOFMEMORY);
        return 0;
    }
    
    lpce->base = pMem;
    lpce->op = dwSize;
    lpce->size = (DWORD)pCurProc;
    
    EnterCriticalSection(&MapNameCS);
    lpce->ceptr = pHugeCleanList;
    pHugeCleanList = lpce;
    LeaveCriticalSection(&MapNameCS);
    
    return pMem;
}


BOOL FSMapMemFree(LPBYTE pBase, DWORD len, DWORD flags);



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
CloseHugeMemoryAreas(
    LPVOID pMem
    )
{
    LPCLEANEVENT pce, pce2;
    
    EnterCriticalSection(&MapNameCS);
    
    pce = pHugeCleanList;
    while (pce && (pce->size == (DWORD)pCurProc) && (!pMem || (pMem == pce->base))) {
        
        pHugeCleanList = pce->ceptr;
        VirtualFree(pce->base, pce->op, MEM_DECOMMIT);
        FSMapMemFree(pce->base, pce->op, MEM_RELEASE);
        FreeMem(pce, HEAP_CLEANEVENT);
        pce = pHugeCleanList;
        
        if (pMem) {
            LeaveCriticalSection(&MapNameCS);
            return TRUE;
        }
    }
    
    if (pce) {
        while (pce->ceptr) {
            if ((pce->ceptr->size == (DWORD)pCurProc)
                && (!pMem || (pMem == pce->ceptr->base))) {
                
                pce2 = pce->ceptr;
                pce->ceptr = pce2->ceptr;
                VirtualFree(pce2->base, pce2->op, MEM_DECOMMIT);
                FSMapMemFree(pce2->base, pce2->op, MEM_RELEASE);
                FreeMem(pce2, HEAP_CLEANEVENT);
                if (pMem) {
                    LeaveCriticalSection(&MapNameCS);
                    return TRUE;
                }
            } else
                pce = pce->ceptr;
        }
    }
    
    LeaveCriticalSection(&MapNameCS);
    
    return FALSE;
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
HugeVirtualRelease(
    LPVOID pMem
    )
{
    if (!CloseHugeMemoryAreas(pMem)) {
        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    
    return TRUE;
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DecommitROPages(
    LPBYTE pBase,
    DWORD len
    )
{
    MEMORY_BASIC_INFORMATION mbi;

    EnterCriticalSection(&VAcs);

    while (len) {
        if (!VirtualQuery(pBase, &mbi, sizeof(mbi))) {
            DEBUGCHK(0);
            break;
        }

        if ((mbi.State == MEM_COMMIT) && (mbi.Protect == PAGE_READONLY))
            VirtualFree(pBase, mbi.RegionSize, MEM_DECOMMIT);

        pBase += mbi.RegionSize;
        if (len < mbi.RegionSize)
            break;

        len -= mbi.RegionSize;
    }

    LeaveCriticalSection(&VAcs);
}



//------------------------------------------------------------------------------
// Decommits or releases a MapMemReserve'd chunk of memory.  Length must be passed in.
// Flags must be MEM_DECOMMIT *or* MEM_RELEASE, and to RELEASE the range must be
// decommitted already.  pBase and len must be page aligned for DECOMMIT.  pBase must be
// SUB_MAPPER_INCR aligned and len must be page aligned for RELEASE
//------------------------------------------------------------------------------
BOOL 
FSMapMemFree(
    LPBYTE pBase,
    DWORD  len,
    DWORD  flags
    )
{
    BOOL   res, retval = FALSE;
    LPBYTE pCurr = pBase;
    DWORD  curr, trav, currbytes, bytesleft = len;

    DEBUGCHK((DWORD)pCurr == PAGEALIGN_UP((DWORD)pCurr));
    DEBUGCHK(bytesleft == PAGEALIGN_UP(bytesleft));

    EnterCriticalSection(&VAcs);

    if (flags == MEM_DECOMMIT) {

        while (bytesleft) {
            currbytes = ((DWORD)(pCurr + MAPPER_INCR) & ~(MAPPER_INCR-1)) - (DWORD)pCurr;
            if (currbytes > bytesleft)
                currbytes = bytesleft;

            DEBUGCHK((DWORD)pCurr == ((DWORD)pCurr & ~(PAGE_SIZE-1)));
            DEBUGCHK(currbytes == PAGEALIGN_UP(currbytes));

            res = VirtualFree(pCurr, currbytes, MEM_DECOMMIT);
            DEBUGCHK(res);

            pCurr += currbytes;
            bytesleft -= currbytes;
        }

        retval = TRUE;

    } else if (flags == MEM_RELEASE) {

        

        do {
            curr = ((DWORD)pCurr - FIRST_MAPPER_ADDRESS) / MAPPER_INCR;
            trav = ((DWORD)pCurr / SUB_MAPPER_INCR) & (32-1);  // & (32-1) is faster than % 32
            DEBUGCHK(rbSubRes[curr] & (1 << trav));
            if (!(rbSubRes[curr] &= ~(1 << trav))) {
                rbRes &= ~(1 << curr);
                DeleteMapperSection(FIRST_MAPPER_ADDRESS + (MAPPER_INCR * curr));
            }

            pCurr += SUB_MAPPER_INCR;

        } while (pCurr < pBase + len);

        retval = TRUE;
    }

    LeaveCriticalSection(&VAcs);

    return retval;
}



//------------------------------------------------------------------------------
// pBase and len must be page-aligned
//------------------------------------------------------------------------------
BOOL 
FSMapMemCommit(
    LPBYTE pBase,
    DWORD  len,
    DWORD  access
    )
{
    BOOL   retval = FALSE;
    LPBYTE pFree, pCurr = pBase;
    DWORD  currbytes, bytesleft = len;

    DEBUGCHK((DWORD)pCurr == PAGEALIGN_UP((DWORD)pCurr));
    DEBUGCHK(bytesleft == PAGEALIGN_UP(bytesleft));

    EnterCriticalSection(&VAcs);

    while (bytesleft) {

        currbytes = (((DWORD)pCurr + MAPPER_INCR) & ~(MAPPER_INCR-1)) - (DWORD)pCurr;
        if (currbytes > bytesleft)
            currbytes = bytesleft;
        DEBUGCHK(currbytes == PAGEALIGN_UP(currbytes));

        if (!VirtualAlloc(pCurr, currbytes, MEM_COMMIT, access)) {
            pFree = pBase;
            bytesleft = len;

            while (pFree != pCurr) {
                currbytes = (((DWORD)pCurr + MAPPER_INCR) & ~(MAPPER_INCR-1)) - (DWORD)pCurr;
                if (currbytes > bytesleft)
                    currbytes = bytesleft;

                VirtualFree(pFree, currbytes, MEM_DECOMMIT);

                pFree += currbytes;
                bytesleft -= currbytes;
            }

            goto exit;
        }

        pCurr += currbytes;
        bytesleft -= currbytes;
    }

    retval = TRUE;

exit:
    LeaveCriticalSection(&VAcs);

    return retval;
}



//------------------------------------------------------------------------------
// Provides r/w view if request r/o view of r/w map
//------------------------------------------------------------------------------
LPVOID 
SC_MapViewOfFile(
    HANDLE  hMap,
    DWORD   fdwAccess,
    DWORD   dwOffsetHigh,
    DWORD   dwOffsetLow,
    DWORD   cbMap
    )
{
    LPFSMAP lpm;
    LPVOID  lpret = 0;
    DWORD   length;
    CLEANEVENT *lpce;

    DEBUGMSG(ZONE_ENTRY || ZONE_MAPFILE,
             (L"SC_MapViewOfFile entry: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
              hMap, fdwAccess, dwOffsetHigh, dwOffsetLow, cbMap));

    EnterCriticalSection(&MapNameCS);


    //
    // Check args
    //

    lpm = HandleToMapPerm(hMap);
    if (!lpm) {
        KSetLastError(pCurThread, ERROR_INVALID_HANDLE);
        goto exit;
    }

    length = (cbMap ? cbMap : lpm->length - dwOffsetLow);

    if ((fdwAccess == FILE_MAP_ALL_ACCESS)
        || (fdwAccess == (FILE_MAP_WRITE | FILE_MAP_READ)))
        fdwAccess = FILE_MAP_WRITE;

    if (dwOffsetHigh
        || ((fdwAccess == FILE_MAP_WRITE)
            && (lpm->hFile != INVALID_HANDLE_VALUE)
            && !lpm->pDirty)
        || (fdwAccess & ~(FILE_MAP_READ | FILE_MAP_WRITE))
        || ((dwOffsetLow + length) > lpm->length)
        || ((dwOffsetLow + length) < dwOffsetLow)
        || ((dwOffsetLow + length) < length)) {
        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
        goto exit;
    }

    
    //
    // Prepare CleanEvent
    //

    lpce = AllocMem(HEAP_CLEANEVENT);
    if (!lpce) {
        KSetLastError(pCurThread, ERROR_OUTOFMEMORY);
        goto exit;
    }

    IncRef(hMap, pCurProc);
    lpret = lpm->pBase + dwOffsetLow;
    lpce->base = lpret;
    lpce->size = (DWORD)pCurProc;
    lpce->ceptr = lpm->lpmlist;
    lpm->lpmlist = lpce;

exit:
    LeaveCriticalSection(&MapNameCS);

    DEBUGMSG(ZONE_ENTRY || ZONE_MAPFILE,
             (L"SC_MapViewOfFile exit: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx -> %8.8lx\r\n",
              hMap, fdwAccess, dwOffsetHigh, dwOffsetLow, cbMap, lpret));

    return lpret;
}


#define FMB_LEAVENAMECS     0x1
#define FMB_DOFULLDISCARD   0x2
#define FMB_NOWRITEOUT      0x4




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
FlushMapBuffers(
    LPFSMAP   lpm,
    DWORD     dwOffset,
    DWORD     dwLength,
    DWORD     dwFlags
    )
{
    DWORD     remain, page, end, dw1;
    BOOL      res, retval = FALSE;
    HANDLE    h;
    LPBYTE    pb;
    ACCESSKEY ulOldKey;

    // No pages to flush on ROM maps
    if (lpm->bDirectROM) {
        if (dwFlags & FMB_LEAVENAMECS)
            LeaveCriticalSection(&MapNameCS);
        return TRUE;
    }

    SWITCHKEY(ulOldKey, 0xffffffff);

    //
    // Figure out where we're starting and stopping, page-aligned
    //

    if (!dwLength)
        dwLength = lpm->length;
    end = dwOffset + dwLength;
    dwOffset = PAGEALIGN_DOWN(dwOffset);
    end = PAGEALIGN_UP(end);
    if (end > lpm->length)
        end = lpm->length;
    if (end < dwOffset) {
        if (dwFlags & FMB_LEAVENAMECS)
            LeaveCriticalSection(&MapNameCS);
        goto exit;
    }


    if (lpm->pDirty == (LPBYTE)1) {
        //
        // Old-style (non-transactionable) file system
        //

        h = lpm->hFile;
        pb = lpm->pBase;
        if (dwFlags & FMB_LEAVENAMECS)
            LeaveCriticalSection(&MapNameCS);
        SetFilePointer(h, dwOffset, 0, FILE_BEGIN);
        if (!FSWriteFile(h, pb, end - dwOffset, &remain) || (remain != end - dwOffset)) {
            DEBUGMSG(ZONE_PAGING || ZONE_MAPFILE, (L"MMFILE: FlushMap failed on write 1\r\n"));
            DEBUGCHK(0);
            goto exit;
        }
        FlushFileBuffers(h);
        retval = TRUE;

    } else if (lpm->pDirty) {
        //
        // The file is read/write -- we may have dirty pages to flush
        //

        EnterCriticalSection(&WriterCS);

        if (!(dwFlags & FMB_NOWRITEOUT)) {

            DEBUGMSG(ZONE_MAPFILE, (L"MMFILE: FlushMap\r\n"));
            EnterCriticalSection(&PagerCS);

            //
            // Now flush the pages in memory back to the file
            //

            for (page = dwOffset/PAGE_SIZE; (page+1)*PAGE_SIZE <= end; page++) {

                // Flush the page only if it is dirty
                if (lpm->pDirty[DIRTY_INDEX(page)] & DIRTY_MASK(page)) {

                    res = VirtualProtect(lpm->pBase + page*PAGE_SIZE, PAGE_SIZE,
                                         PAGE_READONLY, &dw1);
                    DEBUGCHK(res);

                    LeaveCriticalSection(&PagerCS);

                    if (!WriteFileWithSeek(lpm->hFile, lpm->pBase + page*PAGE_SIZE, PAGE_SIZE,
                                           &dw1, 0, page*PAGE_SIZE, 0) ||
                        (dw1 != PAGE_SIZE)) {
                        DEBUGMSG(ZONE_MAPFILE, (L"MMFILE: FlushMap failed on write 2\r\n"));
                        LeaveCriticalSection(&WriterCS);
                        if (dwFlags & FMB_LEAVENAMECS)
                            LeaveCriticalSection(&MapNameCS);
                        DEBUGCHK(0);
                        goto exit;
                    }

                    EnterCriticalSection(&PagerCS);
                    
                    lpm->bRestart = 1;
                    lpm->pDirty[DIRTY_INDEX(page)] &= ~DIRTY_MASK(page);
                    lpm->dwDirty--;
                    DEBUGCHK(!(lpm->dwDirty & 0x80000000));
                }
            }

            // Flush the incomplete page at the end, if present
            remain = end - (page*PAGE_SIZE);
            if (remain && (lpm->pDirty[DIRTY_INDEX(page)] & DIRTY_MASK(page))) {

                DEBUGCHK(remain <= PAGE_SIZE);
                res = VirtualProtect(lpm->pBase + page*PAGE_SIZE, PAGE_SIZE,
                                     PAGE_READONLY, &dw1);
                DEBUGCHK(res);

                LeaveCriticalSection(&PagerCS);

                if (!WriteFileWithSeek(lpm->hFile, lpm->pBase + page*PAGE_SIZE, remain,
                                       &dw1, 0, page*PAGE_SIZE, 0)
                    || (dw1 != remain)) {
                    DEBUGMSG(ZONE_MAPFILE, (L"MMFILE: FlushMap failed on write 3\r\n"));
                    LeaveCriticalSection(&WriterCS);
                    if (dwFlags & FMB_LEAVENAMECS)
                        LeaveCriticalSection(&MapNameCS);
                    DEBUGCHK(0);
                    goto exit;
                }
                
                EnterCriticalSection(&PagerCS);
                
                lpm->bRestart = 1;
                lpm->pDirty[DIRTY_INDEX(page)] &= ~DIRTY_MASK(page);
                lpm->dwDirty--;
                DEBUGCHK(!(lpm->dwDirty & 0x80000000));
            }
            
            LeaveCriticalSection(&PagerCS);

            DEBUGMSG(ZONE_MAPFILE, (L"MMFILE: FlushMap succeeded\r\n"));

            // Flush the dirty blocks in the cache back to the file
            FlushFileBuffers(lpm->hFile);
        }

        DEBUGCHK (!lpm->bDirectROM);
        if (dwFlags & FMB_DOFULLDISCARD)
            DecommitROPages(lpm->pBase, lpm->reslen);
        LeaveCriticalSection(&WriterCS);
        if (dwFlags & FMB_LEAVENAMECS)
            LeaveCriticalSection(&MapNameCS);

    } else {
        //
        // The file is read-only (no dirty pages to flush)
        //
        DEBUGCHK (!lpm->bDirectROM);

        if ((lpm->hFile != INVALID_HANDLE_VALUE) && (dwFlags & FMB_DOFULLDISCARD)) {
            FSMapMemFree(lpm->pBase, lpm->reslen, MEM_DECOMMIT);
            FreeAllPagesFromQ (&lpm->pgqueue);
        }
        if (dwFlags & FMB_LEAVENAMECS)
            LeaveCriticalSection(&MapNameCS);
    }

    retval = TRUE;

exit:
    SETCURKEY(ulOldKey);
    return retval;
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
KernelCloseHandle(
    HANDLE hFile
    )
{
    CALLBACKINFO cbi;

    cbi.hProc = ProcArray[0].hProc;
    cbi.pfn = (FARPROC)CloseHandle;
    cbi.pvArg0 = hFile;

    return (BOOL)PerformCallBack4Int(&cbi);
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
FreeMap(
    HANDLE  hMap
    )
{
    CLEANEVENT *lpce;
    LPFSMAP lpm, lpm2;
    HANDLE  hm;

    DEBUGCHK(MapNameCS.OwnerThread == pCurThread->hTh); // Must own MapNameCS

    lpm = HandleToMap(hMap);
    if (!lpm) {
        // No way to signal an error
        DEBUGCHK(0);
        return;
    }

    EnterCriticalSection(&MapCS);

    // Remove from the list of maps
    if (hMapList == hMap)
        hMapList = lpm->hNext;
    else {
        for (hm = hMapList; hm; hm = lpm2->hNext) {
            lpm2 = HandleToMap(hm);
            if (lpm2) {
                if (lpm2->hNext == hMap) {
                    lpm2->hNext = lpm->hNext;
                    break;
                }
            } else {
                // No way to signal an error -- just continue
                DEBUGCHK(0);
                break;
            }
        }
        DEBUGCHK(hm);
    }

    LeaveCriticalSection(&MapCS);

    // Flush all dirty pages back to the file
    if (lpm->bNoAutoFlush) {
        if (FlushMapBuffersLogged(lpm, 0, lpm->length, FMB_LEAVENAMECS)) {
            FlushSettings FlushStruct;
            DWORD         dwTemp;

            // Remove kernel logging area reserved at the end of the file.
            // This must ONLY be done if the flush completed successfully.
            // To avoid growing/shrinking/growing/shrinking repeatedly, it is
            // only done when the map is freed.
            if (ReadFileWithSeek(lpm->hFile, &FlushStruct, sizeof(FlushSettings),
                                 &dwTemp, 0, offsetof(fslog_t, dwRestoreFlags), 0)
                && (dwTemp == sizeof(FlushSettings))
                && (SetFilePointer(lpm->hFile, FlushStruct.dwRestoreStart, NULL,
                                   FILE_BEGIN) == FlushStruct.dwRestoreStart)) {
                DEBUGMSG(ZONE_MAPFILE, (TEXT("MMFILE: Removing logging area from flushed map 0x%08x at 0x%08x, old size 0x%08x\r\n"),
                                        lpm, FlushStruct.dwRestoreStart, GetFileSize(lpm->hFile, NULL)));
                SetEndOfFile(lpm->hFile);
            }
        }
    } else {
        FlushMapBuffers(lpm, 0, lpm->length, FMB_LEAVENAMECS);
    }

    if ((lpm->hFile != INVALID_HANDLE_VALUE) && !lpm->bNoAutoFlush)
        KernelCloseHandle(lpm->hFile);

    if (lpm->name)
        FreeName(lpm->name);

    while (lpm->lpmlist) {
        lpce = lpm->lpmlist->ceptr;
        FreeMem(lpm->lpmlist, HEAP_CLEANEVENT);
        lpm->lpmlist = lpce;
    }

    {
        LPBYTE pBase = (LPBYTE) PAGEALIGN_DOWN((DWORD)lpm->pBase);
        DWORD  len = PAGEALIGN_UP((DWORD)lpm->pBase + lpm->reslen - (DWORD) pBase);
        FSMapMemFree(pBase, len, MEM_DECOMMIT);
        FreeAllPagesFromQ (&lpm->pgqueue);
        FSMapMemFree(pBase, len, MEM_RELEASE);
    }
    
    FreeMem(lpm, HEAP_FSMAP);
    FreeHandle(hMap);
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HANDLE 
FindMap(
    LPBYTE  pMem
    )
{
    HANDLE  hm;
    LPFSMAP lpm;

    EnterCriticalSection(&MapCS);

    for (hm = hMapList; hm; hm = lpm->hNext) {
        lpm = HandleToMap(hm);
        if (lpm) {
            if ((lpm->pBase == pMem) || ((lpm->pBase <  pMem) && (pMem < lpm->pBase + lpm->length)))
                break;
        } else {
            DEBUGCHK(0);
            hm = (HANDLE)0;
            break;
        }
    }

    LeaveCriticalSection(&MapCS);

    return hm;
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_UnmapViewOfFile(
    LPVOID lpvAddr
    )
{
    BOOL retval = FALSE;
    HANDLE hm;
    BOOL bLeft = 0;
    LPFSMAP lpm;
    CLEANEVENT *lpce, *lpce2;

    DEBUGMSG(ZONE_ENTRY, (L"SC_UnmapViewOfFile entry: %8.8lx\r\n", lpvAddr));

    EnterCriticalSection(&MapNameCS);

    if ((hm = FindMap(lpvAddr)) && (lpm = HandleToMap(hm))){

        lpce2 = NULL;
        
        if (lpce = lpm->lpmlist) {
            if ((lpce->base == lpvAddr) && (lpce->size == (DWORD)pCurProc))
                lpm->lpmlist = lpce->ceptr;
            else {
                do {
                    lpce2 = lpce;
                    lpce = lpce->ceptr;
                } while (lpce && ((lpce->base != lpvAddr)
                                  || (lpce->size != (DWORD)pCurProc)));

                if (lpce)
                    lpce2->ceptr = lpce->ceptr;
            }
        }

        if (lpce) {
            FreeMem(lpce, HEAP_CLEANEVENT);
            if (DecRef(hm, pCurProc, FALSE)) {
                FreeMap(hm);
                bLeft = 1;
            }
            retval = TRUE;
        } else
            KSetLastError(pCurThread, ERROR_INVALID_ADDRESS);

    } else {
        DEBUGCHK(!hm); // DEBUGCHK only if lpm assignment failed
        KSetLastError(pCurThread, ERROR_INVALID_ADDRESS);
    }

    if (!bLeft)
        LeaveCriticalSection(&MapNameCS);

    DEBUGMSG(ZONE_ENTRY, (L"SC_UnmapViewOfFile exit: %8.8lx -> %8.8lx\r\n",
                          lpvAddr, retval));

    return retval;
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_FlushViewOfFile(
    LPCVOID lpBaseAddress,
    DWORD dwNumberOfBytesToFlush
    )
{
    BOOL retval = FALSE;
    LPFSMAP lpm;
    HANDLE hm;

    DEBUGMSG(ZONE_ENTRY, (L"SC_FlushViewOfFile entry: %8.8lx %8.8lx\r\n",
                          lpBaseAddress, dwNumberOfBytesToFlush));

    EnterCriticalSection(&MapNameCS);

    if ((hm = FindMap((LPBYTE)lpBaseAddress)) && (lpm = HandleToMap(hm))) {
        
        if (lpm->bNoAutoFlush) {
            retval = FlushMapBuffersLogged(lpm, (LPBYTE)lpBaseAddress - lpm->pBase,
                                           dwNumberOfBytesToFlush, FMB_LEAVENAMECS);
        } else {
            retval = FlushMapBuffers(lpm, (LPBYTE)lpBaseAddress - lpm->pBase,
                                     dwNumberOfBytesToFlush, FMB_LEAVENAMECS);
        }
    
    } else {
        DEBUGCHK(!hm); // DEBUGCHK only if lpm assignment failed
        LeaveCriticalSection(&MapNameCS);
    }
    
    DEBUGMSG(ZONE_ENTRY, (L"SC_FlushViewOfFile exit: %8.8lx %8.8lx -> %8.8lx\r\n",
                          lpBaseAddress, dwNumberOfBytesToFlush, retval));
    
    return retval;
}


//------------------------------------------------------------------------------
// Not exposed directly, but through IOCTL_KLIB_CHANGEMAPFLUSHING.  Changes the
// flush settings for the mapped view of a file starting at lpBaseAddress.
// The flush settings affect the entire mapping, so no range is given.  Flush
// changes are only supported on logged maps.  Suspending flushing should not
// be taken lightly: it can lead to data loss, OOM, or ballooned file sizes.
//------------------------------------------------------------------------------
BOOL 
ChangeMapFlushing(
    LPCVOID lpBaseAddress,
    DWORD   dwFlags
    )
{
    BOOL retval = FALSE;
    LPFSMAP lpm;
    HANDLE hm;

    if (SC_CeGetCallerTrust() != OEM_CERTIFY_TRUST) {
        KSetLastError(pCurThread, ERROR_ACCESS_DENIED);
        return FALSE;
    }

    EnterCriticalSection(&MapNameCS);
    if ((hm = FindMap((LPBYTE)lpBaseAddress)) && (lpm = HandleToMap(hm))) {
        
        // Flush changes are only used for database volumes so they are only
        // supported on logged maps
        if (lpm->bNoAutoFlush) {
            DEBUGMSG(ZONE_MAPFILE, (TEXT("MMFILE: Changing map 0x%08x flush flags 0x%08x  (%u dirty)\r\n"),
                                    lpm, dwFlags, lpm->dwDirty));
            lpm->bFlushFlags = (BYTE)(dwFlags & FILEMAP_NOBACKGROUNDFLUSH); // Only one flag supported
            retval = TRUE;
        } else {
            KSetLastError(pCurThread, ERROR_ACCESS_DENIED);
        }
    
    } else {
        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
        DEBUGCHK(!hm); // DEBUGCHK only if lpm assignment failed
    }
    LeaveCriticalSection(&MapNameCS);
    
    return retval;
}



#if (PAGE_SIZE == 1024)
    #define MAYBE_LIMIT_MEMORY 32
    #define MAYBE_LIMIT_DIRTY  72
#else
    #define MAYBE_LIMIT_MEMORY 10
    #define MAYBE_LIMIT_DIRTY  18
#endif


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_FlushViewOfFileMaybe(
    LPCVOID lpBaseAddress,
    DWORD   dwNumberOfBytesToFlush
    )
{
    extern long PageOutTrigger;
    BOOL    retval = FALSE;
    LPFSMAP lpm;
    HANDLE  hm;

    DEBUGMSG(ZONE_ENTRY, (L"SC_FlushViewOfFileMaybe entry: %8.8lx %8.8lx\r\n",
                          lpBaseAddress, dwNumberOfBytesToFlush));

    EnterCriticalSection(&MapNameCS);

    if ((hm = FindMap((LPBYTE)lpBaseAddress)) && (lpm = HandleToMap(hm))) {

        if ((PageFreeCount >= MAYBE_LIMIT_MEMORY + PageOutTrigger)
            && (lpm->dwDirty < MAYBE_LIMIT_DIRTY)) {
            LeaveCriticalSection(&MapNameCS);
            retval = TRUE;

        } else if (lpm->bNoAutoFlush) {
            if (lpm->bFlushFlags & FILEMAP_NOBACKGROUNDFLUSH) {
                // Skip background flushing
                LeaveCriticalSection(&MapNameCS);
                retval = TRUE;
            } else {
                retval = FlushMapBuffersLogged(lpm, (LPBYTE)lpBaseAddress - lpm->pBase,
                                               dwNumberOfBytesToFlush, FMB_LEAVENAMECS);
            }
        
        } else {
            retval = FlushMapBuffers(lpm, (LPBYTE)lpBaseAddress-lpm->pBase,
                                     dwNumberOfBytesToFlush, FMB_LEAVENAMECS);
        }

    } else {
        DEBUGCHK(!hm); // DEBUGCHK only if lpm assignment failed
        LeaveCriticalSection(&MapNameCS);
    }

    DEBUGMSG(ZONE_ENTRY, (L"SC_FlushViewOfFileMaybe exit: %8.8lx %8.8lx -> %8.8lx\r\n",
                          lpBaseAddress, dwNumberOfBytesToFlush, retval));

    return retval;
}

int MappedPageInPage (LPFSMAP lpm, BOOL bWrite, DWORD addr, LPBYTE pMem)
{
    DWORD   cbRead, cbToRead;
    
    // Flush logged mapfiles if too many pages are dirty
    if (bWrite
        && lpm->bNoAutoFlush
        && (lpm->dwDirty >= MAYBE_LIMIT_DIRTY - 1)
        && !(lpm->bFlushFlags & FILEMAP_NOBACKGROUNDFLUSH)) {
        
        if (FlushMapBuffersLogged(lpm, 0, 0, 0)) {
            return PAGEIN_OTHERRETRY;
        }
        // Continue with pagein if the flush failed
        RETAILMSG(1, (TEXT("MMFILE: PageIn continuing after mapped flush failure\r\n")));
    }

    if ((cbToRead = (DWORD) lpm->pBase + lpm->filelen - addr) > PAGE_SIZE)
        cbToRead = PAGE_SIZE;

    if (!ReadFileWithSeek (lpm->hFile, pMem, cbToRead, &cbRead, 0, addr - (DWORD)lpm->pBase, 0)) {
        DEBUGMSG(ZONE_PAGING || ZONE_MAPFILE,
                 (L"MappedPageInPage: PageIn failed on ReadFile (0x%08x of 0x%08x read, pos 0x%08x of 0x%08x)\r\n",
                  cbRead, cbToRead, addr - (DWORD)lpm->pBase, lpm->filelen));
        return (KGetLastError (pCurThread) != ERROR_DEVICE_REMOVED)?  PAGEIN_RETRY : PAGEIN_FAILURE;
    }

    // if the mapfile changed underneath, don't bother retrying
    return (cbRead == cbToRead)? PAGEIN_SUCCESS : PAGEIN_FAILURE;
}

int MMQueryState (DWORD addr, BOOL bWrite)
{
    MEMORY_BASIC_INFORMATION mbi;
    DWORD dwOldProtect;

    if (!VirtualQuery((LPVOID)addr, &mbi, sizeof(mbi))) {
        DEBUGMSG(ZONE_PAGING || ZONE_MAPFILE, (L"MMQueryState: VirtualQuery failed\r\n"));
        return PAGEIN_FAILURE;
    }
    
    if (MEM_COMMIT == mbi.State) {
        // memory already commited, someone else beat us paging the page in.
        // Just check/update protection and return accordingly
        
        switch (mbi.Protect & ~(PAGE_NOCACHE|PAGE_GUARD)) {
        case PAGE_READWRITE:
        case PAGE_EXECUTE_READWRITE:
            return PAGEIN_SUCCESS;
            
        case PAGE_READONLY:
        case PAGE_EXECUTE_READ:
            if (!bWrite
                || VirtualProtect((LPVOID)addr, PAGE_SIZE, PAGE_READWRITE, &dwOldProtect)) {
                return PAGEIN_SUCCESS;
            }

            DEBUGMSG(ZONE_PAGING || ZONE_MAPFILE, (L"MMQueryState: Failed: write on RO pages\r\n"));
            // fall through
        default:
            break;
        }
        return PAGEIN_FAILURE;
    }

    // return PAGEIN_RETRY if the memory is not commited (not yet paged in)
    return PAGEIN_RETRY;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int MappedPageInHelper (LPFSMAP lpm, BOOL bWrite, DWORD addr)
{
    LPVOID  pMem = 0, pMemPage = 0;
    BOOL    retval, fPageUsed = TRUE;
    WORD    idxPgMem = INVALID_PG_INDEX;

    DEBUGCHK (PagerCS.OwnerThread == hCurThread);
    
    DEBUGMSG(ZONE_PAGING || ZONE_MAPFILE,
             (L"MappedPageInHelper: Taking fault at 0x%08x (%s)\r\n", addr, bWrite ? L"write" : L"read"));

    // cannot write to RO file
    if (bWrite && (INVALID_HANDLE_VALUE != lpm->hFile) && !lpm->pDirty) {
        DEBUGMSG(ZONE_PAGING || ZONE_MAPFILE, (L"MappedPageInHelper:  Failed - Write on Read Only file\r\n"));
        return PAGEIN_FAILURE;
    }

    // check if the memory is already paged-in by others.
    if ((retval = MMQueryState (addr, bWrite)) != PAGEIN_RETRY) {
        return retval;
    }

    // memory back'd file -- simply commit and return
    if (INVALID_HANDLE_VALUE == lpm->hFile) {

        if (!(pMem = VirtualAlloc((LPVOID)addr, PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE))) {
            DEBUGMSG(ZONE_PAGING || ZONE_MAPFILE, (L"MappedPageInHelper: PageIn failed COMMITING (%8.8lx)\r\n", addr));
            return PAGEIN_FAILURE;
        }
        PagedInCount++;
        return PAGEIN_SUCCESS;

    }

    //
    // allocate memory for paging
    //
    if (!lpm->pDirty) {
        // RO page, try to get it from paging pool
        pMemPage = GetPagingPage (&lpm->pgqueue, 0, 0, &idxPgMem);
    }
            
    if (!pMemPage) {
        // no paging pool, get a page from secure slot for paging
        if (pMem = VirtualAlloc ((LPVOID)ProcArray[0].dwVMBase, 0x10000, MEM_RESERVE, PAGE_NOACCESS)) {
            pMemPage = VirtualAlloc ((LPVOID)((DWORD)pMem + (addr & 0xffff)), PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);
        }
            
        if (!pMemPage) {
            DEBUGMSG(ZONE_PAGING || ZONE_MAPFILE, (L"MappedPageInHelper: PageIn failed on VirtualAlloc\r\n"));
            // it's debatable whether we return PAGEIN_RETRY or PAGEIN_FAILURE when memory allocation failed.
            // for now we're returning PAGEIN_RETRY
        }
    }

    if (pMemPage) {

        lpm->bRestart = 0;

        LeaveCriticalSection(&PagerCS);

        // page in the memory mapped file
        retval = MappedPageInPage (lpm, bWrite, addr, pMemPage);

        EnterCriticalSection(&PagerCS);

        if (PAGEIN_SUCCESS == retval) {

            if (!lpm->bRestart
                && ((retval = MMQueryState (addr, bWrite)) == PAGEIN_RETRY)) {

	            OEMCacheRangeFlush (pMemPage, PAGE_SIZE, CACHE_SYNC_WRITEBACK);
                if (VirtualCopy ((LPVOID)addr, pMemPage, PAGE_SIZE, bWrite ? PAGE_READWRITE : PAGE_READONLY)) {
                    // note: if VirtualCopy Failed, we will return PAGEIN_RETRY since it might be someone
                    //       else paged in the page for us
                    retval = PAGEIN_SUCCESS;
                    PagedInCount++;
                }
                
            } else {
                // if we get here, the page we allocated for paging isn't used. Make sure we return it to
                // the paging pool's free list.
                fPageUsed = FALSE;
            }

        }

        
    }

    if (INVALID_PG_INDEX != idxPgMem) {
        // if we're using paging pool, add the page to the memory mapped file's queue when succeeded,
        // or return it to free list when failed
        if ((PAGEIN_SUCCESS == retval) && fPageUsed) {
            AddPageToQueue (&lpm->pgqueue, idxPgMem, addr, lpm);
        } else {
            AddPageToFreeList (idxPgMem);
        }
    } else {
        if (pMemPage && !VirtualFree(pMemPage, PAGE_SIZE, MEM_DECOMMIT)) {
            DEBUGMSG(1, (L"MappedPageInHelper: VirtualFree 1 failed during PageIn!\r\n"));
        }

        if (pMem && !VirtualFree(pMem, 0, MEM_RELEASE)) {
            DEBUGMSG(1, (L"MappedPageInHelper: VirtualFree 2 failed during PageIn!\r\n"));
        }

    }
    DEBUGMSG(ZONE_PAGING || ZONE_MAPFILE,
             (L"MappedPageInHelper: PageIn exit, 0x%08x (%d) -> %u\r\n", addr, bWrite, retval));

    return retval;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int MappedPageIn (BOOL bWrite, DWORD addr)
{
    int     retval;
    HANDLE  hm;
    LPFSMAP lpm;
    ACCESSKEY ulOldKey;

    addr = PAGEALIGN_DOWN(addr);

    if (!(hm = FindMap((LPBYTE)addr))) {
        DEBUGMSG(ZONE_PAGING || ZONE_MAPFILE,
                 (L"MMFILE: PageIn failed on FindMap 0x%08x (%d)\r\n", addr, bWrite));
        return PAGEIN_FAILURE;
    }

    if (!(lpm = HandleToMap(hm))) {
        DEBUGMSG(ZONE_PAGING || ZONE_MAPFILE, (L"MMFILE: PageIn failed on bad hMap\r\n"));
        DEBUGCHK(0);
        return PAGEIN_FAILURE;
    }
    DEBUGCHK(!(lpm->bDirectROM)); // Should never have to page in ROM maps

    EnterCriticalSection(&PagerCS);
    SWITCHKEY(ulOldKey, 0xffffffff);

    retval = MappedPageInHelper (lpm, bWrite, addr);
    if ((PAGEIN_SUCCESS == retval) && bWrite && lpm->pDirty) {

        DWORD page = (addr - (DWORD)lpm->pBase + PAGE_SIZE - 1)/PAGE_SIZE;

        DEBUGMSG(lpm->pDirty[DIRTY_INDEX(page)] & DIRTY_MASK(page),
                 (TEXT("MappedPageIn: Pagein dirtying already-dirty page %u of map 0x%08x!\r\n"),
                  page, lpm));
        lpm->pDirty[DIRTY_INDEX(page)] |= DIRTY_MASK(page);
        lpm->dwDirty++;

    }
    
    SETCURKEY(ulOldKey);
    LeaveCriticalSection(&PagerCS);
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
PageOutFile(void)
{
    HANDLE hMap;
    LPFSMAP lpm;
    
    EnterCriticalSection(&MapNameCS);
    DEBUGMSG(ZONE_PAGING || ZONE_MAPFILE,
             (L"MMFILE: PageOut starting free count: %d\r\n", PageFreeCount));
    
    for (hMap = hMapList; hMap; hMap = lpm->hNext) {
        lpm = HandleToMap(hMap);
        if (lpm) {
            if (lpm->pDirty != (LPBYTE)1) {
                FlushMapBuffers(lpm, 0, lpm->length,
                                FMB_DOFULLDISCARD | (lpm->bNoAutoFlush ? FMB_NOWRITEOUT : 0));
            }
        } else {
            // No way to signal an error -- just continue
            DEBUGCHK(0);
            break;
        }
    }
    
    DEBUGMSG(ZONE_PAGING || ZONE_MAPFILE,
             (L"MMFILE: PageOut ending free count: %d\r\n", PageFreeCount));
    LeaveCriticalSection(&MapNameCS);
}


CLEANEVENT *pCFMCleanup;




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
TryCloseMappedHandle(
    HANDLE h
    )
{
    BOOL retval;
    CLEANEVENT *pce, *pce2;

    EnterCriticalSection(&MapNameCS);

    pce = pCFMCleanup;
    if (pce) {
        if ((pce->size == (DWORD)pCurProc) && (pce->base == h)) {
            pCFMCleanup = pce->ceptr;
            FreeMem(pce, HEAP_CLEANEVENT);
            retval = KernelCloseHandle(h);
            LeaveCriticalSection(&MapNameCS);
            return retval;
        }

        while (pce->ceptr) {
            if ((pce->ceptr->size == (DWORD)pCurProc) && (pce->ceptr->base == h)) {
                pce2 = pce->ceptr;
                pce->ceptr = pce2->ceptr;
                FreeMem(pce2, HEAP_CLEANEVENT);
                retval = KernelCloseHandle(h);
                LeaveCriticalSection(&MapNameCS);
                return retval;
            }

            pce = pce->ceptr;
        }
    }

    LeaveCriticalSection(&MapNameCS);

    KSetLastError(pCurThread, ERROR_INVALID_HANDLE);

    return FALSE;
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
CloseMappedFileHandles()
{
    HANDLE  hm;
    LPFSMAP lpm;
    CLEANEVENT *pce, *pce2;

    CloseHugeMemoryAreas(0);

    EnterCriticalSection(&MapNameCS);
    pce = pCFMCleanup;
    while (pce && (pce->size == (DWORD)pCurProc)) {
        pCFMCleanup = pce->ceptr;
        KernelCloseHandle(pce->base);
        FreeMem(pce, HEAP_CLEANEVENT);
        pce = pCFMCleanup;
    }

    if (pce) {
        while (pce->ceptr) {
            if (pce->ceptr->size == (DWORD)pCurProc) {
                pce2 = pce->ceptr;
                pce->ceptr = pce2->ceptr;
                KernelCloseHandle(pce2->base);
                FreeMem(pce2, HEAP_CLEANEVENT);
            } else
                pce = pce->ceptr;
        }
    }

    EnterCriticalSection(&MapCS);
    for (hm = hMapList; hm; hm = lpm->hNext) {

        lpm = HandleToMap(hm);
        if (lpm) {
            
            while ((pce = lpm->lpmlist) && (pce->size == (DWORD)pCurProc)) {
                lpm->lpmlist = pce->ceptr;
                FreeMem(pce, HEAP_CLEANEVENT);
            }
    
            if (lpm->lpmlist) {
                pce2 = lpm->lpmlist;
                pce = pce2->ceptr;
                while (pce) {
                    if (pce->size == (DWORD)pCurProc) {
                        pce2->ceptr = pce->ceptr;
                        FreeMem(pce, HEAP_CLEANEVENT);
                        pce = pce2->ceptr;
                    } else {
                        pce2 = pce;
                        pce = pce->ceptr;
                    }           
                }
            }
        
        } else {
            // No way to signal an error -- just continue
            DEBUGCHK(0);
            break;
        }
    }

    LeaveCriticalSection(&MapCS);
    LeaveCriticalSection(&MapNameCS);
}   



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HANDLE 
SC_CreateFileForMapping(
    LPCTSTR lpFileName, 
    DWORD   dwDesiredAccess, 
    DWORD   dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,                                
    DWORD   dwCreationDisposition, 
    DWORD   dwFlagsAndAttributes,                               
    HANDLE  hTemplateFile
    )
{
    HANDLE  hFile;
    CALLBACKINFO cbi;
    CLEANEVENT *pce;
    DWORD   dwFlags;

    // Make sure the caller has access to this file
    if ((SC_CeGetCallerTrust() != OEM_CERTIFY_TRUST) 
        && (IsSystemFile(lpFileName) || (dwFlagsAndAttributes & FILE_ATTRIBUTE_SYSTEM))
        && (!g_fSysFileReadable || (dwDesiredAccess & GENERIC_WRITE))) {
        ERRORMSG(1, (TEXT("CreateFileForMapping failed due to insufficient trust\r\n")));
        KSetLastError(pCurThread, ERROR_ACCESS_DENIED);
        return INVALID_HANDLE_VALUE;
    }

    pce = AllocMem(HEAP_CLEANEVENT);
    if (!pce) {
        KSetLastError(pCurThread, ERROR_NOT_ENOUGH_MEMORY);
        return INVALID_HANDLE_VALUE;
    }

    // Prepare to create the file
    cbi.hProc = ProcArray[0].hProc;
    cbi.pfn = (FARPROC)CreateFile;
    cbi.pvArg0 = MapPtr(lpFileName);

    if (dwShareMode & FILE_SHARE_WRITE_OVERRIDE) {
        dwFlags = FILE_SHARE_WRITE | FILE_SHARE_READ;
    } else {
        dwFlags = (dwDesiredAccess == GENERIC_READ) ? FILE_SHARE_READ : 0;
    }

    // Create the file
    hFile = (HANDLE)PerformCallBack(&cbi, dwDesiredAccess, dwFlags, lpSecurityAttributes,
                                    dwCreationDisposition,
                                    dwFlagsAndAttributes | FILE_FLAG_WRITE_THROUGH,
                                    hTemplateFile);
    if (hFile != INVALID_HANDLE_VALUE) {
        // Add to mapper cleanup list
        EnterCriticalSection(&MapNameCS);
        pce->base = hFile;
        pce->size = (DWORD)pCurProc;
        pce->ceptr = pCFMCleanup;
        pCFMCleanup = pce;
        LeaveCriticalSection(&MapNameCS);
    } else
        FreeMem(pce, HEAP_CLEANEVENT);

    return hFile;
}



//------------------------------------------------------------------------------
// flProtect can only combine PAGE_READONLY, PAGE_READWRITE, and SEC_COMMIT
// lpsa ignored
//------------------------------------------------------------------------------
HANDLE 
SC_CreateFileMapping(
    HANDLE  hFile, 
    LPSECURITY_ATTRIBUTES lpsa,
    DWORD   flProtect, 
    DWORD   dwMaxSizeHigh,
    DWORD   dwMaxSizeLow, 
    LPCTSTR lpName
    )
{
    HANDLE  hMap = 0;
    LPFSMAP lpm = 0;
    BYTE    testbyte;
    BOOL    bPage = TRUE;
    DWORD   len, dwFileLen, dwMapLen;
    LPBYTE  pData = 0;
    BOOL    bNoAutoFlush;
    BOOL    bDirectROM = FALSE;
    BY_HANDLE_FILE_INFORMATION info;

    DEBUGMSG(ZONE_ENTRY, (L"SC_CreateFileMapping entry: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
                          hFile, lpsa, flProtect, dwMaxSizeHigh, dwMaxSizeLow, lpName));

    SetNKCallOut (pCurThread);
    
    // Special treatment of uncompressed files in ROM: Map directly from ROM
    // without using any RAM.
    if ((hFile != INVALID_HANDLE_VALUE)
        && !(flProtect & PAGE_READWRITE)
        && GetFileInformationByHandle(hFile, &info)
        && (info.dwFileAttributes & FILE_ATTRIBUTE_INROM)
        && !(info.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED)
        && IS_ROMFILE_OID(info.dwOID)
        // Only trusted applications can map directly from ROM because they may
        // get access to nearby data
        && (SC_CeGetCallerTrust() == OEM_CERTIFY_TRUST)) {
        
        DEBUGMSG(ZONE_PAGING || ZONE_MAPFILE, (TEXT("CreateFileMapping: map file directly from ROM\r\n")));
        bDirectROM = TRUE;
        bPage = FALSE;
        dwMapLen = 0; // Will be figured out later
    }
    ClearNKCallOut (pCurThread);

    bNoAutoFlush = (flProtect & PAGE_INTERNALDBMAPPING) ? 1 : 0;
    flProtect &= ~(SEC_COMMIT | PAGE_INTERNALDBMAPPING);

    // Remove non-database mappings from mapper cleanup list
    if ((hFile != INVALID_HANDLE_VALUE) && !bNoAutoFlush) {
        CLEANEVENT *pce, *pce2;

        EnterCriticalSection(&MapNameCS);

        if (pce = pCFMCleanup) {
            if (pce->base == hFile) {
                pCFMCleanup = pce->ceptr;
                FreeMem(pce, HEAP_CLEANEVENT);
            } else {
                while (pce->ceptr && (pce->ceptr->base != hFile))
                    pce = pce->ceptr;
                if (pce2 = pce->ceptr) {
                    pce->ceptr = pce2->ceptr;
                    FreeMem(pce2, HEAP_CLEANEVENT);
                }
            }
        }

        LeaveCriticalSection(&MapNameCS);
    }

    
    //
    // Determine the size the map should have
    //
    if (!bDirectROM) {
        if ((hFile != INVALID_HANDLE_VALUE)
            && ((dwFileLen = SetFilePointer(hFile, 0, 0, FILE_END)) == 0xffffffff)) {
            KernelCloseHandle(hFile);
            KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
            return 0;
        }
    
        // Adjust based on parameters
        if ((hFile == INVALID_HANDLE_VALUE) || dwMaxSizeLow)
            dwMapLen = dwMaxSizeLow;
        else
            dwMapLen = dwFileLen;
        if (dwMapLen & 0x80000000) {
            KernelCloseHandle(hFile);
            KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
            return 0;
        }
    
        // Extend the file to the new size, if necessary
        if ((hFile != INVALID_HANDLE_VALUE) && (dwFileLen < dwMapLen)) {
            if (!bNoAutoFlush && ((SetFilePointer(hFile, dwMapLen, 0, FILE_BEGIN) != dwMapLen)
                                  || !SetEndOfFile(hFile))) {
                KernelCloseHandle(hFile);
                KSetLastError(pCurThread, ERROR_ACCESS_DENIED);
                return 0;
            }
            // For logged maps we still fake the file size, and leave it up to
            // filesys to fill it.  (see FSVolExtendFile)
    
            dwFileLen = dwMapLen;
        }
    }


    //
    // Make sure the mapping doesn't already exist
    //

    EnterCriticalSection(&MapNameCS);

    if (lpName) {
        // Check name
        len = strlenW(lpName);
        if (len > MAX_PATH - 1) {
            KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
            goto errexit;
        }

        EnterCriticalSection(&MapCS);

        for (hMap = hMapList; hMap; hMap = lpm->hNext) {
            lpm = HandleToMap(hMap);
            if (lpm) {
                if (lpm->name && !strcmpW(lpm->name->name, lpName)) {
                    // Found an existing map
                    IncRef(hMap, pCurProc);
                    LeaveCriticalSection(&MapCS);
                    if (hFile != INVALID_HANDLE_VALUE)
                        KernelCloseHandle(hFile);
                    KSetLastError(pCurThread, ERROR_ALREADY_EXISTS);
                    goto exit;
                }
            } else {
                // No way to signal an error -- just continue
                DEBUGCHK(0);
                break;
            }
        }
        KSetLastError(pCurThread, 0);

        LeaveCriticalSection(&MapCS);
    }

    LeaveCriticalSection(&MapNameCS);


    //
    // Not found, validate params outside critsecs to avoid deadlocks
    //

    if ((hFile != INVALID_HANDLE_VALUE) && !bDirectROM) {
        // Determine if paging is possible (Read/WriteFileWithSeek must be usable)
        if (!ReadFileWithSeek(hFile, 0, 0, 0, 0, 0, 0)) {
            SetFilePointer(hFile, 0, 0, 0);
            if (dwFileLen && (!ReadFile(hFile, &testbyte, sizeof(BYTE), &len, 0)
                              || (len != sizeof(BYTE)))) {
                KernelCloseHandle(hFile);
                KSetLastError(pCurThread, ERROR_INVALID_HANDLE);
                return 0;
            }
            
            // Database volumes require paging
            if (bNoAutoFlush) {
                DEBUGMSG(1, (TEXT("Failing CreateMap on non-pageable file system\r\n")));
                KernelCloseHandle(hFile);
                KSetLastError(pCurThread, ERROR_NOT_SUPPORTED);
                return 0;
            }
            
            bPage = FALSE;
        }
    
        // Verify that we can write to the file
        if (dwFileLen && (flProtect == PAGE_READWRITE)) {
            if ((bPage && (!ReadFileWithSeek(hFile, &testbyte, sizeof(BYTE), &len, 0, 0, 0)
                           || (len != sizeof(BYTE))
                           || !WriteFileWithSeek(hFile, &testbyte, sizeof(BYTE), &len, 0, 0, 0)
                           || (len != sizeof(BYTE))))
                || (!bPage && (SetFilePointer(hFile, 0, 0, FILE_BEGIN)
                               || !FSWriteFile(hFile, &testbyte, sizeof(BYTE), &len)
                               || (len != sizeof(BYTE))))) {
                KernelCloseHandle(hFile);
                KSetLastError(pCurThread, ERROR_ACCESS_DENIED);
                return 0;
            }
        }
    
        if (!bPage) {
            // Reserve memory for mapping
            pData = FSMapMemReserve(PAGEALIGN_UP(dwMapLen), bNoAutoFlush);
            if (!pData) {
                if (hFile != INVALID_HANDLE_VALUE)
                    KernelCloseHandle(hFile);
                KSetLastError(pCurThread, ERROR_NOT_ENOUGH_MEMORY);
                return 0;
            }
    
            // Commit
            if (!FSMapMemCommit(pData, PAGEALIGN_UP(dwMapLen), PAGE_READWRITE)) {
                FSMapMemFree(pData, PAGEALIGN_UP(dwMapLen), MEM_RELEASE);
                if (hFile != INVALID_HANDLE_VALUE)
                    KernelCloseHandle(hFile);
                KSetLastError(pCurThread, ERROR_NOT_ENOUGH_MEMORY);
                return 0;
            }
    
            // Copy the file data into the mapped memory
            SetFilePointer(hFile, 0, 0, FILE_BEGIN);
            if (!ReadFile(hFile, pData, dwMapLen, &len, 0) || (len != dwMapLen)) {
                // Free up the memory we've grabbed
                FSMapMemFree(pData, PAGEALIGN_UP(dwMapLen), MEM_DECOMMIT);
                FSMapMemFree(pData, PAGEALIGN_UP(dwMapLen), MEM_RELEASE);
                if (hFile != INVALID_HANDLE_VALUE)
                    KernelCloseHandle(hFile);
                KSetLastError(pCurThread, ERROR_INVALID_HANDLE);
                return 0;
            }
    
            // Mark the memory as R/O, if necessary
            if (flProtect != PAGE_READWRITE)
                VirtualProtect(pData, PAGEALIGN_UP(dwMapLen), PAGE_READONLY, &len);
        }
    }


    //
    // Check again to make sure the mapping doesn't already exist,
    // since not holding critsecs above
    //
    
    EnterCriticalSection(&MapNameCS);
    
    if (lpName) {
        EnterCriticalSection(&MapCS);

        for (hMap = hMapList; hMap; hMap = lpm->hNext) {
            lpm = HandleToMap(hMap);
            if (lpm) {
                if (lpm->name && !strcmpW(lpm->name->name, lpName)) {
                    // Found an existing map
                    IncRef(hMap, pCurProc);
                    if (hFile != INVALID_HANDLE_VALUE)
                        KernelCloseHandle(hFile);
                    KSetLastError(pCurThread, ERROR_ALREADY_EXISTS);
                    LeaveCriticalSection(&MapCS);
                    
                    if (!bPage && pData) {
                        // Free up the memory we've grabbed
                        FSMapMemFree(pData, PAGEALIGN_UP(dwMapLen), MEM_DECOMMIT);
                        FSMapMemFree(pData, PAGEALIGN_UP(dwMapLen), MEM_RELEASE);
                    }
                    
                    goto exit;
                }
            } else {
                // No way to signal an error -- just continue
                DEBUGCHK(0);
                break;
            }
        }
        KSetLastError(pCurThread, 0);

        LeaveCriticalSection(&MapCS);
    }
    

    //
    // Prepare the map control struct
    //
    
    lpm = 0;
    hMap = 0;

    // Validate args -- Why do we do it here?
    if (((flProtect != PAGE_READONLY) && (flProtect != PAGE_READWRITE))
        || dwMaxSizeHigh
        || ((hFile == INVALID_HANDLE_VALUE) && !dwMaxSizeLow)
        || ((flProtect == PAGE_READONLY) && (hFile == INVALID_HANDLE_VALUE))) {
        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
        goto errexit;
    }

    // Allocate space for the map struct
    lpm = (LPFSMAP)AllocMem(HEAP_FSMAP);
    if (!lpm) {
        KSetLastError(pCurThread, ERROR_NOT_ENOUGH_MEMORY);
        goto errexit;
    }

    lpm->pgqueue.idxHead = INVALID_PG_INDEX;
    lpm->lpmlist = 0;
    lpm->bNoAutoFlush = bNoAutoFlush;
    lpm->bDirectROM = bDirectROM;
    lpm->dwDirty = 0;
    lpm->pBase = NULL;

    // Copy the name
    if (lpName) {
        lpm->name = (Name *)AllocName((strlenW(lpName) + 1) * 2);
        if (!lpm->name) {
            KSetLastError(pCurThread, ERROR_NOT_ENOUGH_MEMORY);
            goto errexit;
        }
        kstrcpyW(lpm->name->name, lpName);
    } else
        lpm->name = 0;

    hMap = AllocHandle(&cinfMap, lpm, pCurProc);
    if (!hMap) {
        KSetLastError(pCurThread, ERROR_NOT_ENOUGH_MEMORY);
        goto errexit;
    }

    lpm->length = dwMapLen;
    lpm->reslen = PAGEALIGN_UP(lpm->length);

    // Leave space for dirty bits
    if ((flProtect == PAGE_READWRITE) && (hFile != INVALID_HANDLE_VALUE))
        lpm->reslen += PAGEALIGN_UP(((lpm->reslen + PAGE_SIZE - 1) / PAGE_SIZE + 7) / 8); // one bit per page
    if (!lpm->reslen)
        lpm->reslen = PAGE_SIZE;

    
    //
    // If we haven't already reserved memory for mapping, do it now
    //
    
    if (bDirectROM) {
        DWORD  dwType, dwFileIndex, dwOffset;
        LPBYTE lpPhys, lpVirt;
            
        ROMFILE_FROM_OID(info.dwOID, dwType, dwFileIndex);
        if (FindROMFile(dwType, dwFileIndex, &lpPhys, &dwFileLen)) {
        	// Round address down to page boundary
        	dwOffset = (DWORD)lpPhys & (PAGE_SIZE - 1);
        	lpPhys -= dwOffset;
            dwMapLen = dwFileLen + dwOffset;

        	// Map location of the file in ROM into shared virtual address space
            lpVirt = FSMapMemReserve (dwMapLen, FALSE);
        	if (!lpVirt) {
                goto errexit;
            }
            if (!VirtualCopy(lpVirt, lpPhys, dwMapLen, PAGE_READONLY)) {
                VirtualFree(lpVirt, 0, MEM_RELEASE); // Won't stomp lasterror
                goto errexit;
            }
            
            lpm->pBase = lpVirt + dwOffset;
            lpm->length = dwMapLen;
            lpm->reslen = dwMapLen;
            lpm->bRestart = 0;
            lpm->pDirty = 0;
            
            // lpVirt is recoverable for doing the free later
            DEBUGCHK(lpVirt == (LPBYTE)PAGEALIGN_DOWN((DWORD)lpm->pBase));

        } else {
            // File info says the file is in ROM and uncompressed, but it wasn't found
            DEBUGCHK(0);
            KSetLastError(pCurThread, ERROR_FILE_NOT_FOUND);
            goto errexit;
        }
	
    } else if (bPage) {
        // Reserve memory for mapping
        lpm->pBase = FSMapMemReserve(lpm->reslen, bNoAutoFlush);
        if (!lpm->pBase) {
            KSetLastError(pCurThread, ERROR_NOT_ENOUGH_MEMORY);
            goto errexit;
        }

        // Commit
        if ((flProtect == PAGE_READWRITE) && (hFile != INVALID_HANDLE_VALUE)) {
            lpm->pDirty = lpm->pBase + PAGEALIGN_UP(lpm->length);
            lpm->bRestart = 0;
            if (!FSMapMemCommit(lpm->pDirty, lpm->reslen - PAGEALIGN_UP(lpm->length),
                                PAGE_READWRITE)) {
                KSetLastError(pCurThread, ERROR_NOT_ENOUGH_MEMORY);
                goto errexit;
            }
        } else
            lpm->pDirty = 0;
    
    } else {
        // We already reserved memory, so fill in the struct
        lpm->pBase = pData;
        lpm->bRestart = 0;
        lpm->pDirty = (flProtect == PAGE_READWRITE) ? (LPBYTE)1 : 0;
    }

    // Final sanity checks on the file
    lpm->hFile = hFile;
    if (hFile != INVALID_HANDLE_VALUE) {
        PHDATA  phd;
        
        phd = HandleToPointer(hFile);
        if ((phd->lock != 1) || (phd->ref.count != 1)) {
            RETAILMSG(1, (L"CreateFileMapping called with handle not created with " \
                          L"CreateFileForMapping!\r\n"));
            KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
            goto errexit;
        }
        
        lpm->filelen = dwFileLen;
        if (lpm->filelen == 0xFFFFFFFF) {
            KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
            goto errexit;
        }
    }

    // Add to the list of mapped files
    EnterCriticalSection(&MapCS);
    lpm->hNext = hMapList;
    hMapList = hMap;
    LeaveCriticalSection(&MapCS);

exit:
    LeaveCriticalSection(&MapNameCS);
    
    if (bNoAutoFlush) {
        if (!ValidateFile(lpm)) {
            // We cannot commit the file.  Fail.
            goto errexitnocs;
        }
    }
    DEBUGMSG(ZONE_ENTRY || ZONE_MAPFILE, (L"SC_CreateFileMapping exit: %8.8lx\r\n", hMap));
    
    return hMap;

errexit:
    LeaveCriticalSection(&MapNameCS);

errexitnocs:

    // Remove from the list of maps, if it's there
    if (hMap && lpm) {
        LPFSMAP lpmTemp;
        
        EnterCriticalSection(&MapNameCS);
        EnterCriticalSection(&MapCS);
        
        if (hMapList == hMap) {
            lpmTemp = HandleToMap(hMap);
            if (lpmTemp) {
                hMapList = lpmTemp->hNext;
            } else {
                hMapList = (HANDLE)0;
            }
        
        } else {
            HANDLE hmTemp;
            
            for (hmTemp = hMapList; hmTemp; hmTemp = lpmTemp->hNext) {
                lpmTemp = HandleToMap(hmTemp);
                if (lpmTemp) {
                    if (lpmTemp->hNext == hMap) {
                        lpmTemp->hNext = lpm->hNext;
                        break;
                    }
                } else {
                    DEBUGCHK(0);
                    break;
                }
            }
        }
        
        LeaveCriticalSection(&MapCS);
        LeaveCriticalSection(&MapNameCS);
    }


    // Free up allocated memory
    if (lpm) {
        if (lpm->name)
            FreeName(lpm->name);
        if (hMap) {
            FreeHandle(hMap);
        }
        if (lpm->pBase) {
            if (!lpm->bDirectROM) {
                FSMapMemFree(lpm->pBase, lpm->reslen, MEM_DECOMMIT);
                FreeAllPagesFromQ (&lpm->pgqueue);
                FSMapMemFree(lpm->pBase, lpm->reslen, MEM_RELEASE);
            } else {
                VirtualFree((LPBYTE)PAGEALIGN_DOWN((DWORD)lpm->pBase), 0, MEM_RELEASE);
            }
        }
        FreeMem(lpm, HEAP_FSMAP);
    }

    if (!bPage && pData) {
        FSMapMemFree(pData, PAGEALIGN_UP(dwMapLen), MEM_DECOMMIT);
        FSMapMemFree(pData, PAGEALIGN_UP(dwMapLen), MEM_RELEASE);
    }

    if (hFile != INVALID_HANDLE_VALUE)
        KernelCloseHandle(hFile);

    DEBUGMSG(ZONE_ENTRY || ZONE_MAPFILE, (L"SC_CreateFileMapping exit: %8.8lx\r\n", 0));

    return 0;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_MapCloseHandle(
    HANDLE hMap
    )
{
    DEBUGMSG(ZONE_ENTRY, (L"SC_MapCloseHandle entry: %8.8lx\r\n", hMap));

    EnterCriticalSection(&MapNameCS);
    if (DecRef(hMap, pCurProc, FALSE))
        FreeMap(hMap);
    else
        LeaveCriticalSection(&MapNameCS);

    DEBUGMSG(ZONE_ENTRY, (L"SC_MapCloseHandle exit: %8.8lx -> %8.8lx\r\n", hMap, TRUE));

    return TRUE;
}


//------------------------------------------------------------------------------
// This function is only run on non-auto-flush maps, ie. mapped database files.
//------------------------------------------------------------------------------
#pragma prefast(disable: 262, "use 4K buffer")
BOOL 
ValidateFile(
    LPFSMAP lpm
    )
{
    FlushSettings FlushStruct;  // Current flush progress
    RestoreInfo   RestStruct;   // Used to copy pages of data
    DWORD bread, dwToWrite, offset, pagesize, size, count;

    // Read restore data
    if (!ReadFileWithSeek(lpm->hFile, &FlushStruct, sizeof(FlushStruct), &bread, 0, 
                          offsetof(fslog_t, dwRestoreFlags), 0)
        || (bread != sizeof(FlushStruct))) {
        // This shouldn't happen.
        DEBUGCHK(0);
        return FALSE;
    }

    // Now act on the restore state
    switch (FlushStruct.dwRestoreFlags) {
    case RESTORE_FLAG_FLUSHED:

        // The whole file has been flushed, but the dirty pages were flushed to
        // the end of the file for safety.  Now we must move all of those pages
        // into their proper places within the file.

        offset = FlushStruct.dwRestoreStart;
        count = FlushStruct.dwRestoreSize >> 16;
        pagesize = FlushStruct.dwRestoreSize & 0xffff;
        size = (DWORD)&RestStruct.restorepage - (DWORD)&RestStruct + pagesize;

        // Move each dirty page from the end of the file to its proper place.
        while (count--
               && ReadFileWithSeek(lpm->hFile, &RestStruct, size, &bread, 0, offset, 0)
               && (bread == size)) {

            dwToWrite = ((RestStruct.dataoffset + pagesize <= FlushStruct.dwRestoreStart)
                         ? pagesize
                         : ((RestStruct.dataoffset < FlushStruct.dwRestoreStart)
                            ? (FlushStruct.dwRestoreStart - RestStruct.dataoffset)
                            : 0));

            DEBUGMSG(ZONE_MAPFILE, (TEXT("MMFILE: Validate page 0x%08x of map 0x%08x (count=%u)\r\n"),
                                    RestStruct.dataoffset, lpm, count));
            
            // Page 0 contains the flush flags.  Copy them before writing it to
            // the file.  That way flags currently in the file aren't overwritten
            // with garbage data from RAM.
            if (RestStruct.dataoffset == 0) {
                memcpy(&RestStruct.restorepage[offsetof(fslog_t, dwRestoreFlags)],
                       &FlushStruct, sizeof(FlushStruct));
            }
            
            if (!WriteFileWithSeek(lpm->hFile, &RestStruct.restorepage[0],
                                   dwToWrite, &bread, 0, RestStruct.dataoffset, 0)
                || (bread != dwToWrite)) {
                ERRORMSG(1, (L"MMFILE: ValidateFile failure to commit page! pos=0x%08x\r\n", RestStruct.dataoffset));
                
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
                return FALSE;
            }

            offset += size;
        }

        FlushFileBuffers(lpm->hFile);
        
        // intentionally fall through

    case RESTORE_FLAG_UNFLUSHED:

        // Clear the restore flag
        FlushStruct.dwRestoreFlags = RESTORE_FLAG_NONE;
        if (!WriteFileWithSeek(lpm->hFile, &FlushStruct.dwRestoreFlags,
                               sizeof(DWORD), &bread, 0,
                               offsetof(fslog_t, dwRestoreFlags), 0)
            || (bread != sizeof(DWORD))) {
            DEBUGMSG(1, (L"MMFILE: ValidateFile failed to clear flush flags\r\n"));                    
            
            // This failure will result in another flush attempt later, but
            // no data has been lost.
            return FALSE;
        }

        FlushFileBuffers(lpm->hFile);
        return TRUE;

    case RESTORE_FLAG_NONE:
        return TRUE;

    default:
        // Even on new maps the flags should be valid; initialized by filesys
        DEBUGCHK(0);
    }

    return FALSE;
}
#pragma prefast(pop)



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
FlushMapBuffersLogged(
    LPFSMAP lpm, 
    DWORD   dwOffset, 
    DWORD   dwLength, 
    DWORD   dwFlags
    )
{
    DWORD   remain, page, end, dw1, count;
    BOOL    retval = FALSE;
    HANDLE  h;
    LPBYTE  pb;
    ACCESSKEY ulOldKey;

    SWITCHKEY(ulOldKey, 0xffffffff);

    DEBUGCHK(lpm->bNoAutoFlush);
    DEBUGCHK(!(lpm->bDirectROM));
    
    //
    // Figure out where we're starting and stopping, page-aligned
    //

    if (!dwLength)
        dwLength = lpm->length;
    end = dwOffset+dwLength;
    dwOffset = PAGEALIGN_DOWN(dwOffset);
    end = PAGEALIGN_UP(end);
    if (end > lpm->length)
        end = lpm->length;
    if (end < dwOffset) {
        if (dwFlags & FMB_LEAVENAMECS)
            LeaveCriticalSection(&MapNameCS);
        return FALSE;
    }


    if (lpm->pDirty == (LPBYTE)1) {
        //
        // Old-style (non-transactionable) file system
        //

        ERRORMSG(1, (L"MMFILE: FlushMapBuffersLogged being used on old-style " \
                     L"(non-transactionable) file system!\r\n"));
        h = lpm->hFile;
        pb = lpm->pBase;
        if (dwFlags & FMB_LEAVENAMECS)
            LeaveCriticalSection(&MapNameCS);
        SetFilePointer(h, dwOffset, 0, FILE_BEGIN);
        if (!FSWriteFile(h, pb, end-dwOffset, &remain) || (remain != end-dwOffset)) {
            DEBUGMSG(ZONE_MAPFILE, (L"MMFILE: logged FlushMap failure on write 1\r\n"));
            DEBUGCHK(0);
            goto exit;
        }
        FlushFileBuffers(h);
        retval = TRUE;

    } else if (lpm->pDirty) {
        //
        // The file is read/write -- we may have dirty pages to flush
        //

        EnterCriticalSection(&WriterCS);

        if (!(dwFlags & FMB_NOWRITEOUT)) {

            DWORD dwFilePos, dwMemPos;
            FlushSettings FlushStruct;  // Current flush progress

            count = 0;


            //
            // Read and update restore data
            //

            DEBUGMSG(ZONE_MAPFILE, (L"MMFILE: logged FlushMap\r\n"));
            if (!ReadFileWithSeek(lpm->hFile, &FlushStruct, 2*sizeof(DWORD),
                                  &dw1, 0, offsetof(fslog_t, dwRestoreFlags), 0)
                || (dw1 != 2*sizeof(DWORD))) {
                // We can exit here because nothing has been changed.
                RETAILMSG(1, (L"MMFILE: logged FlushMap failed on read 1\r\n"));
                LeaveCriticalSection(&WriterCS);
                if (dwFlags & FMB_LEAVENAMECS)
                    LeaveCriticalSection(&MapNameCS);
                
                // debugchk because this shouldn't happen
                DEBUGCHK(0);
                
                goto exit;
            }
            
            // If a previous flush is incomplete, it must be cleaned up before
            // continuing.  Typically this happens in an out-of-disk scenario.
            if (FlushStruct.dwRestoreFlags != RESTORE_FLAG_NONE) {
                RETAILMSG(1, (L"MMFILE: resuming/backing out previous logged FlushMap (flags=0x%08x, start=0x%08x, pages=0x%04x)\r\n",
                              FlushStruct.dwRestoreFlags, FlushStruct.dwRestoreStart,
                              FlushStruct.dwRestoreSize >> 16));
                if (!ValidateFile(lpm)) {
                    // Cannot do any more flushing until the file is valid
                    LeaveCriticalSection(&WriterCS);
                    if (dwFlags & FMB_LEAVENAMECS)
                        LeaveCriticalSection(&MapNameCS);
                    goto exit;
                }
            }
            
            // Jump out now if there are no dirty pages to flush
            if (lpm->dwDirty == 0) {
                retval = TRUE;
                LeaveCriticalSection(&WriterCS);
                if (dwFlags & FMB_LEAVENAMECS)
                    LeaveCriticalSection(&MapNameCS);
                DEBUGMSG(ZONE_MAPFILE, (TEXT("MMFILE: Skipping FlushMapBuffersLogged, no dirty pages to flush!\r\n")));
                goto exit;
            }

            dwFilePos = FlushStruct.dwRestoreStart;
            FlushStruct.dwRestoreFlags = RESTORE_FLAG_UNFLUSHED;
            FlushStruct.dwRestoreSize = PAGE_SIZE;
            if (!WriteFileWithSeek(lpm->hFile, &FlushStruct, sizeof(FlushStruct),
                                   &dw1, 0, offsetof(fslog_t, dwRestoreFlags), 0)
                || (dw1 != sizeof(FlushStruct))) {
                // We can exit here because nothing has been changed.
                RETAILMSG(1, (L"MMFILE: logged FlushMap failed on write 2\r\n"));
                LeaveCriticalSection(&WriterCS);
                if (dwFlags & FMB_LEAVENAMECS)
                    LeaveCriticalSection(&MapNameCS);
                goto exit;
            }


            //
            // Now flush the pages in memory back to the file.  We write the
            // new pages to the end of the file for safety, and only copy them
            // into their rightful places when we are done, inside ValidateFile.
            // We cannot clear the dirty flags until all pages are successfully
            // flushed, so we only update their dirty flags at the end.
            //

            // Flush the dirty blocks in the cache back to the file
            FlushFileBuffers(lpm->hFile);

            EnterCriticalSection(&PagerCS);
            
            for (page = dwOffset/PAGE_SIZE; (page+1)*PAGE_SIZE <= end; page++) {

                // Flush the page only if it is dirty
                if (lpm->pDirty[DIRTY_INDEX(page)] & DIRTY_MASK(page)) {

                    // If it's the first page, make sure the restore data is correct.
                    if (!page)
                        memcpy(lpm->pBase + offsetof(fslog_t, dwRestoreFlags),
                               &FlushStruct, sizeof(FlushStruct));
                    
                    if (VirtualProtect(lpm->pBase+page*PAGE_SIZE, PAGE_SIZE,
                                       PAGE_READONLY, &dw1)) {

                        LeaveCriticalSection(&PagerCS);

                        // Write the page's location to the start of the page
                        dwMemPos = page*PAGE_SIZE;
                        if (!WriteFileWithSeek(lpm->hFile, &dwMemPos, sizeof(DWORD),
                                               &dw1, 0, dwFilePos, 0)
                            || (dw1 != sizeof(DWORD))) {
                            // We can exit here because nothing has really changed.
                            RETAILMSG(1, (L"MMFILE: logged FlushMap failed on write 3\r\n"));
                            goto exitUnlock;
                        }

                        // Now write the page of data
                        dwFilePos += sizeof(DWORD);
                        if (!WriteFileWithSeek(lpm->hFile, lpm->pBase + page*PAGE_SIZE, PAGE_SIZE,
                                               &dw1, 0, dwFilePos, 0)
                            || (dw1 != PAGE_SIZE)) {
                            // We can exit here because nothing has really changed.
                            RETAILMSG(1, (L"MMFILE: logged FlushMap failed on write 4\r\n"));
                            goto exitUnlock;
                        }

                        DEBUGMSG(ZONE_MAPFILE, (TEXT("MMFILE: flushed map 0x%08x page 0x%08x (%u, count=%u)\r\n"),
                                                lpm, lpm->pBase + page*PAGE_SIZE, page, count+1));
                        dwFilePos += PAGE_SIZE;
                        count++;

                        EnterCriticalSection(&PagerCS);
                    
                    } else {
                        // Failed the VirtualProtect - abort the flush
                        // This can happen when a call to LockFile induces a
                        // pagein, which triggers a flush
                        RETAILMSG(1, (L"MMFILE: logged FlushMap failed on VirtualProtect 1\r\n"));
                        LeaveCriticalSection(&PagerCS);
                        goto exitUnlock;
                    }
                }
            }

            // Flush the incomplete page at the end, if present
            remain = end - (page*PAGE_SIZE);
            if (remain && (lpm->pDirty[DIRTY_INDEX(page)] & DIRTY_MASK(page))) {

                DEBUGCHK(remain <= PAGE_SIZE);

                // If it's the first page, make sure the restore data is correct.
                if (!page)
                    memcpy(lpm->pBase + offsetof(fslog_t, dwRestoreFlags), 
                           &FlushStruct, sizeof(FlushStruct));

                if (VirtualProtect(lpm->pBase + page*PAGE_SIZE, PAGE_SIZE, 
                                   PAGE_READONLY, &dw1)) {

                    LeaveCriticalSection(&PagerCS);

                    // Write the page's location to the start of the page
                    dwMemPos = page*PAGE_SIZE;
                    if (!WriteFileWithSeek(lpm->hFile, &dwMemPos, sizeof(DWORD),
                                           &dw1, 0, dwFilePos, 0)
                        || (dw1 != sizeof(DWORD))) {
                        // We can exit here because nothing has really changed.
                        RETAILMSG(1, (L"MMFILE: logged FlushMap failed on write 5\r\n"));
                        goto exitUnlock;
                    }

                    // Now write the page of data
                    dwFilePos += sizeof(DWORD);
                    if (!WriteFileWithSeek(lpm->hFile, lpm->pBase + page*PAGE_SIZE, PAGE_SIZE,
                                           &dw1, 0, dwFilePos, 0)
                        || (dw1 != PAGE_SIZE)) {
                        // We can exit here because nothing has really changed.
                        RETAILMSG(1, (L"MMFILE: logged FlushMap failed on write 6\r\n"));
                        goto exitUnlock;
                    }

                    DEBUGMSG(ZONE_MAPFILE, (TEXT("MMFILE: flushed map 0x%08x page 0x%08x (%u, count=%u)\r\n"),
                                            lpm, lpm->pBase + page*PAGE_SIZE, page, count+1));
                    dwFilePos += PAGE_SIZE;
                    count++;
                
                } else {
                    // Failed the VirtualProtect - abort the flush
                    RETAILMSG(1, (L"MMFILE: logged FlushMap failed on VirtualProtect 2\r\n"));
                    LeaveCriticalSection(&PagerCS);
                }

            } else
                LeaveCriticalSection(&PagerCS);

            // Flush the dirty blocks in the cache back to the file
            FlushFileBuffers(lpm->hFile);


            //
            // Update restore data again
            //

            FlushStruct.dwRestoreFlags = RESTORE_FLAG_FLUSHED;
            FlushStruct.dwRestoreSize |= count << 16;
            if (!WriteFileWithSeek(lpm->hFile, &FlushStruct, sizeof(FlushStruct),
                                   &dw1, 0, offsetof(fslog_t, dwRestoreFlags), 0)
                || (dw1 != sizeof(FlushStruct))) {
                // We can exit here because nothing has really changed.
                RETAILMSG(1, (L"MMFILE: logged FlushMap failed on write 7\r\n"));
                goto exitUnlock;
            }


            // Flush the dirty blocks in the cache back to the file
            FlushFileBuffers(lpm->hFile);

            DEBUGMSG(ZONE_MAPFILE, (L"MMFILE: logged FlushMap completed first phase\r\n"));

            
            // Commit the flush.
            if (!ValidateFile(lpm)) {
                // We cannot validate the file, so we must fail the flush!
                goto exitUnlock;
            }
        
        
            //
            // Now that everything is totally done, we can mark the blocks
            // as clean.
            //
            
            EnterCriticalSection(&PagerCS);
            for (page = dwOffset/PAGE_SIZE; (page+1)*PAGE_SIZE <= end; page++) {
                if (lpm->pDirty[DIRTY_INDEX(page)] & DIRTY_MASK(page)) {
                    // Mark as clean
                    lpm->bRestart = 1;
                    lpm->pDirty[DIRTY_INDEX(page)] &= ~DIRTY_MASK(page);
                    lpm->dwDirty--;
                    DEBUGCHK(!(lpm->dwDirty & 0x80000000));
                }
            }
            remain = end - (page*PAGE_SIZE);
            if (remain && (lpm->pDirty[DIRTY_INDEX(page)] & DIRTY_MASK(page))) {
                // Mark as clean
                lpm->bRestart = 1;
                lpm->pDirty[DIRTY_INDEX(page)] &= ~DIRTY_MASK(page);
                lpm->dwDirty--;
                DEBUGCHK(!(lpm->dwDirty & 0x80000000));
            }
            LeaveCriticalSection(&PagerCS);

            DEBUGCHK(lpm->dwDirty == 0);
            lpm->dwDirty = 0;
        }

        DEBUGCHK (!lpm->bDirectROM);
        if (dwFlags & FMB_DOFULLDISCARD)
            DecommitROPages(lpm->pBase, lpm->reslen);
        LeaveCriticalSection(&WriterCS);

    } else {
        //
        // The file is read-only (no dirty pages to flush)
        //
        DEBUGCHK (!lpm->bDirectROM);

        if ((lpm->hFile != INVALID_HANDLE_VALUE) && (dwFlags & FMB_DOFULLDISCARD)) {
            FSMapMemFree(lpm->pBase, lpm->reslen, MEM_DECOMMIT);
            FreeAllPagesFromQ (&lpm->pgqueue);
        }
    }
    
    if (dwFlags & FMB_LEAVENAMECS)
        LeaveCriticalSection(&MapNameCS);

    // Successful return
    retval = TRUE;
    goto exit;

exitUnlock:
    // If an error occurred after some pages were VirtualProtected, we must
    // go back and unlock them.
    
    // File must be read/write
    DEBUGCHK(lpm->pDirty > (LPBYTE)1);
        
    RETAILMSG(1, (TEXT("MMFILE: logged FlushMap unlocking protected pages\r\n")));
    
    EnterCriticalSection(&PagerCS);

    // Here we assume that all dirty pages should be read/write...
    for (page = dwOffset/PAGE_SIZE; (page+1)*PAGE_SIZE <= end; page++) {
        if ((lpm->pDirty[DIRTY_INDEX(page)] & DIRTY_MASK(page))
            && !VirtualProtect(lpm->pBase + page*PAGE_SIZE, PAGE_SIZE,
                               PAGE_READWRITE, &dw1)) {
            RETAILMSG(1, (L"MMFILE: logged FlushMap failure to unlock page 0x%08x\r\n",
                          lpm->pBase + page*PAGE_SIZE));
            DEBUGCHK(0);
        }
    }
    remain = end - (page*PAGE_SIZE);
    if (remain && (lpm->pDirty[DIRTY_INDEX(page)] & DIRTY_MASK(page))
        && !VirtualProtect(lpm->pBase + page*PAGE_SIZE, PAGE_SIZE,
                           PAGE_READWRITE, &dw1)) {
        RETAILMSG(1, (L"MMFILE: logged FlushMap failure to unlock page 0x%08x\r\n",
                      lpm->pBase + page*PAGE_SIZE));
        DEBUGCHK(0);
    }
    
    LeaveCriticalSection(&PagerCS);
    
    // Flush the dirty blocks in the cache back to the file
    FlushFileBuffers(lpm->hFile);

    LeaveCriticalSection(&WriterCS);
    if (dwFlags & FMB_LEAVENAMECS)
        LeaveCriticalSection(&MapNameCS);

exit:
    DEBUGMSG(ZONE_MAPFILE, (L"MMFILE: logged FlushMap exit (0x%08x)\r\n", retval));
    SETCURKEY(ulOldKey);
    return retval;
}

