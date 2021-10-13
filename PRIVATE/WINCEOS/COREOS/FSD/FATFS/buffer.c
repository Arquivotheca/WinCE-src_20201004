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

    buffer.c

Abstract:

    This file contains routines for managing disk data buffers.  A
    buffer is one or more contiguous blocks.  Blocks are a fixed
    (compile-time) size, independent of sector size.  Buffer size is
    dynamic however; it is calculated as the LARGER of the sector size
    and the block size, and there MUST be an integral number of both
    blocks AND sectors per buffer (and normally it's the SAME integral
    number, because our fixed block size is normally the same as
    the sector size of most media -- 512 bytes).

    All read/write access to a volume is through the buffers managed
    by this module.  Every buffer has a variety of states:  it may be
    VALID or INVALID, HELD or UNHELD, DIRTY or CLEAN, and ASSIGNED or
    UNASSIGNED.

    VALID means the buffer contains data for some block on some volume;
    INVALID means it doesn't contain anything yet (or anymore, if a
    dirty buffer couldn't be committed due to some disk error).  A buffer
    is valid if its pvol points to a VOLUME structure and is invalid if
    null.

    HELD means the buffer is currently being examined and/or modified
    by one or more threads; UNHELD means it isn't.  Unless a buffer
    size was chosen that spans multiple sectors (and therefore multiple
    streams), it will be rare for a buffer to be held by more than one
    thread, because buffers are normally accessed only on behalf of streams,
    and streams are normally accessed only while their critical section
    is owned.

    DIRTY means a buffer contains changes that need to be written to
    a volume;  CLEAN means it matches the volume.  There is no lazy-write
    mechanism currently, so when a function is done modifying buffers, it
    needs to commit those dirty buffers synchronously.  CommitBuffer does
    this by clearing a buffer's DIRTY bit and then writing the buffer's
    data to disk;  thus, if another thread dirties the same buffer before
    the write completes, the buffer remains dirty.  Because a DIRTY buffer
    is not necessarily a HELD buffer, CommitBuffer also holds a buffer
    across the entire "cleaning" operation to insure that FindBuffer
    doesn't steal the buffer until it's fully committed.  In summary, a
    buffer must always be HELD while its DIRTY/CLEAN state is being changed.

    ASSIGNED means a buffer is assigned to some stream.  We assign
    buffers to streams so that when CommitStreamBuffers is called, we
    can readily find all the buffers containing data for that stream.
    Note that since the buffer size could be such that multiple streams
    could share the same buffers, we force AssignStreamBuffer to commit
    a buffer before giving it to a different stream.  This may or may
    not result in an extra write, but at least it preserves the notion
    that once CommitStreamBuffers has completed, the stream is fully
    committed.

    Streams also have the notion of a CURRENT buffer.  When they call
    ReadStreamBuffer, whatever buffer supplies the data is HELD and then
    recorded in the stream as the stream's current buffer (s_pbufCur).
    If a subsequent ReadStreamBuffer returns the same buffer, it remains
    the current buffer;  if it returns a different buffer, the previous
    current buffer is unheld and no longer current.

    A stream's current buffer state is protected by the stream's
    critical section;  ie, another thread cannot seek to another part of
    some stream, thereby forcing another buffer to become current, while
    an earlier thread was still examining data in an earlier current
    buffer.  A problem, however, can arise on the SAME thread.  MoveFile
    is a good example:  if MoveFile tries to open two streams at once
    (one for the source filename and one for destination) and both
    source and destination happen to be the same directory (ie, the same
    stream), MoveFile will simply get two pointers to the SAME stream
    data structure;  thus, every time it seeks and reads something using
    one stream, it could be modifying the stream's current buffer and
    thereby invalidating any pointer(s) it obtained via the other stream.

    So MoveFile, and any other code that juggles multiple streams, needs
    to be sure that its streams are unique, or take care to explicitly
    hold a stream's current buffer before operating on a second, potentially
    identical stream.

    Furthermore, every function that juggles multiple streams probably
    also uses multiple buffers.  Even if we had N buffers in the buffer
    pool (where N is a large number), we could have N threads all arrive
    in MoveFile at the same time, all obtain their first buffer, and then
    all block trying to get a second buffer.  Ouch.

    The easiest way to solve that problem is to count buffer-consuming
    threads in FATFS, and when the number of threads * MIN_BUFFERS exceeds
    the number of available buffers, block until available buffers increases
    sufficiently.

Revision History:

    Lazy Writer Thread (added post v2.0 -JTP)

    Initial design goals for lazy writing include:

        1. Simplicity.  Create a dedicated lazy-writer thread when FATFS.DLL
        is loaded and initialized for the first time.  The natural place
        to manage creation/destruction of the thread is entirely within the
        BufInit and BufDeinit functions.

        Although it might be nice to have "lazy lazy-writer thread creation"
        (ie, to defer thread creation until we get to a point where something
        has actually been written), I think that for now WE will be the
        lazy ones, and defer that feature to a later date.  Besides, we reduce
        the risk of running into some architectural problem with that approach
        downstream.

        2. Minimum lazy-writer wake-ups (ie, it shouldn't wake up if there's
        no work to do), balanced by a maximum age for dirty data.  Age will be
        tracked by timestamps in the buffer headers, updating those timestamps
        every time DirtyBuffer is called.

        3. Maximum transparency (ie, existing FATFS code shouldn't change too
        much).  However, we do need to more carefully differentiate between
        those places where we really need to COMMIT as opposed to simply WRITE.
        Thus, we now have WriteStreamBuffers and WriteAndReleaseStreamBuffers to
        complement CommitStreamBuffers and CommitAndReleaseStreamBuffers.
--*/

#include "fatfs.h"


/*  The minimum number of buffers is the number of buffers that may be
 *  simultaneously held during any one operation.  For example, when
 *  CreateName is creating an entry for a new subdirectory, it can have
 *  holds on 1 FAT buffer and up to 2 directory buffers (in the case of
 *  zeroing a growing directory), all simultaneously.  We round the minimum
 *  up to 4, (a) to be safe, and (b) to evenly divide the default number of
 *  buffers.
 */

#define MIN_BUFFERS     4
#ifdef TFAT
#define DEF_BUFFERS     64      
#else
#define DEF_BUFFERS     32
#endif












//#define BIG_BUFFERS
DWORD       cDefBuffers;    // Number of buffers defined in the registry
DWORD       cbBuf;              // buffer size (computed at run-time)
DWORD       cbufTotal;          // total number of buffers
DWORD       cbufError;          // total buffers with outstanding write errors


DWORD       flBuf;              // see GBUF_*
#define GBUF_LOCALALLOC         0x00000001

DWORD       cblkBuf;            // blocks per buffer (computed at run-time)
DWORD       maskBufBlock;       // to convert blocks to buffer-granular blocks
PBYTE       pbBufCarve;         // address of carved buffer memory, if any
BUF_DLINK   dlBufMRU;           // MRU list of buffers
DWORD       cBufVolumes;        // number of volumes using buffer pool
DWORD       cBufThreads;        // number of threads buffer pool can handle
DWORD       cBufThreadsOrig;    // number of threads buffer pool can handle
HANDLE      hevBufThreads;      // auto-reset event signalled when another thread can be handled
CRITICAL_SECTION csBuffers;     // buffer pool critical section




BOOL BufInit(void)
{
    InitializeCriticalSection(&csBuffers);
    DEBUGALLOC(DEBUGALLOC_CS);
    hevBufThreads = CreateEvent(NULL, FALSE, FALSE, NULL);      // auto-reset, not initially signalled
    if (hevBufThreads) {
        DEBUGALLOC(DEBUGALLOC_EVENT);
        return TRUE;
    }
    return FALSE;
}


void BufDeinit(void)
{
    if (hevBufThreads) {
        CloseHandle(hevBufThreads);
        DEBUGFREE(DEBUGALLOC_EVENT);
    }
    DEBUGFREE(DEBUGALLOC_CS);
    DeleteCriticalSection(&csBuffers);
}


/*  BufEnter - Gates threads using the buffer pool
 *
 *  Originally, this function was very simple.  If cBufThreads dropped
 *  below zero, it waited for the supportable thread count to rise again
 *  before letting another thread enter.
 *
 *  Unfortunately, because we retain dirty data indefinitely, in the hopes
 *  that it can eventually be committed, dirty buffers may not actually be
 *  usable if the data cannot actually be committed at this time (eg, card
 *  is bad, card has been removed, etc).  So, now we charge uncommitable
 *  buffers against cBufThreads as well.
 *
 *  ENTRY
 *      fForce - TRUE to force entry, FALSE if not
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (SetLastError is already set)
 *
 *  NOTES
 *      Called at the start of *all* FATFS API entry points, either directly
 *      or via FATEnter.
 */

BOOL BufEnter(BOOL fForce)
{
    if (fForce) {
        // Keep the buffer thread count balanced, but don't tarry
        InterlockedDecrement(&cBufThreads);
        return TRUE;
    }

    if (cLoads == 0) {
        // Any non-forced request during unloading is promptly failed
        SetLastError(ERROR_NOT_READY);
        return FALSE;
    }

    if (InterlockedDecrement(&cBufThreads) < 0) {

        // We have exceeded the number of threads that the buffer pool
        // can handle simultaneously.  We shall wait on an event that will
        // be set as soon as someone increments cBufThreads from within
        // negative territory.

        WaitForSingleObject(hevBufThreads, INFINITE);
    }
    return TRUE;
}


/*  BufExit - Gates threads finished with the buffer pool
 *
 *  ENTRY
 *      None
 *
 *  EXIT
 *      None
 *
 *  NOTES
 *      Called at the end of *all* FATFS API entry points, either directly
 *      or via FATExit.
 */

void BufExit()
{
    if (InterlockedIncrement(&cBufThreads) <= 0) {
        SetEvent(hevBufThreads);
    }
}


/*  NewBuffer - allocate a new buffer
 *
 *  ENTRY
 *      cb - size of buffer, in bytes
 *      ppbCarve - pointer to address of buffer memory to carve up;
 *      if either the pointer or the address it points is NULL, then
 *      we will allocate memory for the buffer internally.
 *
 *  EXIT
 *      pointer to buffer, NULL if error.  Note that this allocates
 *      both a BUF structure and the block of memory referenced by the
 *      BUF structure that is actually used to cache the disk data.
 */

__inline PBUF NewBuffer(DWORD cb, PBYTE *ppbCarve)
{
    PBUF pbuf;

    pbuf = (PBUF)LocalAlloc(LPTR, sizeof(BUF));

    if (pbuf) {

        DEBUGALLOC(sizeof(BUF));

        if (ppbCarve && *ppbCarve) {
            pbuf->b_pdata = *ppbCarve;
            pbuf->b_flags |= BUF_CARVED;
            *ppbCarve += cb;
        }
        else {
            if (flBuf & GBUF_LOCALALLOC) {
                pbuf->b_pdata = (PVOID)LocalAlloc(LMEM_FIXED, cb);
            }
            else {
                pbuf->b_pdata = VirtualAlloc(0,
                                             cb,
                                             MEM_RESERVE|MEM_COMMIT,
                                           //MEM_RESERVE|MEM_MAPPED,
                                             PAGE_READWRITE
                                           //PAGE_NOACCESS
                                            );
            }
            if (!pbuf->b_pdata) {
                DEBUGFREE(sizeof(BUF));
                VERIFYNULL(LocalFree((HLOCAL)pbuf));
                return NULL;
            }
            DEBUGALLOC(cb);
        }
    }
    return pbuf;
}


/*  AllocBufferPool - pre-allocate all buffers in buffer pool
 *
 *  ENTRY
 *      pvol - pointer to VOLUME currently being mounted
 *
 *  EXIT
 *      TRUE if buffer pool successfully (or already) allocated.  Every
 *      successful call must ultimately be matched by a call to
 *      FreeBufferPool;  only when the last volume has called FreeBufferPool
 *      will the pool actually be deallocated.
 *
 *  NOTES
 *      This function can fail if (a) the minimum number of buffers
 *      could not be allocated, or (b) the buffer pool was already allocated
 *      but the size of the buffers cannot accomodate the sector size of
 *      this particular volume.
 */

BOOL AllocBufferPool(PVOLUME pvol)
{
    ASSERT(OWNCRITICALSECTION(&pvol->v_cs));

    if (cBufVolumes > 0) {

        

#ifdef BIG_BUFFERS
        if (cbBuf < pvol->v_pdsk->d_diActive.di_bytes_per_sect)
            return FALSE;
#else
        if (cbBuf != pvol->v_pdsk->d_diActive.di_bytes_per_sect)
            return FALSE;
#endif
    }

    // If the current volume is already using the buffer pool,
    // then we're done.  Otherwise, increment total buffer pool clients.

    EnterCriticalSection(&csBuffers);

    // If the volume is already buffered, then we can skip most of this
    // work, but we'll still need to perform the recycled check below, because
    // if the volume changed on us, we need to throw away all its buffers.

    if (!(pvol->v_flags & VOLF_BUFFERED)) {

        pvol->v_flags |= VOLF_BUFFERED;

        if (cBufVolumes++ == 0) {

            DWORD cbTotal;
            PBYTE pbCarve = NULL;

            if (cbufTotal == 0) {

                SYSTEM_INFO si;
                DWORD dwError; 
                HKEY hKeyFATFS;

                cDefBuffers = DEF_BUFFERS;
                dwError = RegOpenKeyEx(HKEY_LOCAL_MACHINE, awcFATFS, 0, KEY_ALL_ACCESS, &hKeyFATFS);
                if (ERROR_SUCCESS == dwError) {
                    if (!FSDMGR_GetRegistryValue((HDSK)pvol->v_pdsk->d_hdsk, L"BufferSize", &cDefBuffers)) {
                        cDefBuffers = DEF_BUFFERS;
                    }
                    RegCloseKey( hKeyFATFS);
                }                

                // Check that default buffer pool size is not too small, a multiple of minimum, and 
                // multiple of typical page size
                if ((cDefBuffers < MIN_BUFFERS) || (cDefBuffers % MIN_BUFFERS) ||  ((DEF_BUFFERS*512) & (4096-1)))
                        cDefBuffers = DEF_BUFFERS;                             

                InitList((PDLINK)&dlBufMRU);

                // Determine buffer size (and number of blocks per that size)

                GetSystemInfo(&si);

#ifdef BIG_BUFFERS
                cbBuf = si.dwPageSize;

                // Make sure that a system with, say, 4K pages will still work
                // with a device that has, say, 8K sectors.

                if (cbBuf < pvol->v_pdsk->d_diActive.di_bytes_per_sect) {
                    cbBuf = pvol->v_pdsk->d_diActive.di_bytes_per_sect;
                    flBuf |= GBUF_LOCALALLOC;
                }
#else
                cbBuf = pvol->v_pdsk->d_diActive.di_bytes_per_sect;

                if (cbBuf != si.dwPageSize)
                    flBuf |= GBUF_LOCALALLOC;
#endif
                // Make sure the optimal buffer size for this volume is
                // ALSO at least as large as our block size.

                if (cbBuf < BLOCK_SIZE)
                    cbBuf = BLOCK_SIZE;

                // Make sure buffer size is a multiple of the block size.

                if ((cbBuf & BLOCK_OFFSET_MASK) != 0) {
                    DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!AllocBufferPool: cbBuf(%d) not a multiple of BLOCK_SIZE(%d), failing...\n"), cbBuf, BLOCK_SIZE));
                    goto error;
                }

                // Calculate the number of blocks per buffer.

                cblkBuf = cbBuf >> BLOCK_LOG2;

                // Calculate mask that converts a block into a buffer-granular block.

                maskBufBlock = ~(cblkBuf-1);

                // Make sure number of blocks per buffer is power of 2.

                if ((cblkBuf & ~maskBufBlock) != 0) {
                    DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FATFS!AllocBufferPool: cblkBuf(%d) is not a power of two, failing...\n"), cblkBuf));
                    goto error;
                }

                // See if it makes sense to allocate a large chunk of virtual
                // memory and carve it up (ie, if the total size of the buffer
                // pool is a perfect number of pages).

                ASSERT(pbBufCarve == NULL);

                if ((cbTotal = cDefBuffers * cbBuf) % si.dwPageSize == 0) {

                    pbBufCarve = VirtualAlloc(0,
                                              cbTotal,
                                              MEM_RESERVE|MEM_COMMIT,
                                            //MEM_RESERVE|MEM_MAPPED,
                                              PAGE_READWRITE
                                            //PAGE_NOACCESS
                                             );
                    if (pbBufCarve) {
                        DEBUGALLOC(cbTotal);
                        pbCarve = pbBufCarve;
                    }
                }
            }


            while (cbufTotal < cDefBuffers) {
                PBUF pbuf;
                if (!(pbuf = NewBuffer(cbBuf, &pbCarve)))
                    break;
                cbufTotal++;
                AddItem((PDLINK)&dlBufMRU, (PDLINK)&pbuf->b_dlink);
            }

            if (cbufTotal < MIN_BUFFERS) {

              error:
                FreeBufferPool(pvol);
                LeaveCriticalSection(&csBuffers);
                return FALSE;
            }

            // Since we have at least MIN_BUFFERS buffers now, calculate
            // the number of threads our buffer pool can handle simultaneously.
            // If DEMAND_PAGING is enabled, we reduce that number by 1 so that
            // a Pagein thread can always assume there are adequate buffers.

#ifdef DEMAND_PAGING
            cBufThreads = cBufThreadsOrig = cbufTotal / MIN_BUFFERS - 1;
#else
            cBufThreads = cBufThreadsOrig = cbufTotal / MIN_BUFFERS;
#endif
        }
    }

    // For all calls (first time or not), we must invalidate any buffers
    // we retained for an unmounted volume that was subsequently *recycled*
    // instead of *remounted*.

    if (pvol->v_flags & VOLF_RECYCLED)
        InvalidateBufferSet(pvol, TRUE);

    LeaveCriticalSection(&csBuffers);
    return TRUE;
}


/*  FreeBufferPool - free all buffers in buffer pool
 *
 *  ENTRY
 *      pvol - pointer to VOLUME currently being unmounted
 *
 *  EXIT
 *      TRUE if volume can be freed, or FALSE if not (ie, there are still
 *      dirty buffers that could not be committed).  Note that if the volume
 *      wasn't going to be freed to begin with, we always return FALSE.
 *
 *      This module keeps track of the number of buffer clients (volumes),
 *      and only when the last client has called FreeBufferPool will the pool
 *      actually be freed (subject to all buffers being clean, of course).
 */

BOOL FreeBufferPool(PVOLUME pvol)
{
    PBUF pbuf, pbufEnd;
    BOOL fFree = TRUE;
    BOOL fFreeCarve = FALSE;

    ASSERT(OWNCRITICALSECTION(&pvol->v_cs));

    // If the current volume isn't even using the buffer pool (yet),
    // then we're done.

    if (!(pvol->v_flags & VOLF_BUFFERED))
        return fFree;

    EnterCriticalSection(&csBuffers);

    // This is our last chance to commit all dirty buffers.  With any
    // luck, this will clean all the buffers, allowing us to free them all.
    // That will be our assumption at least.  If dirty buffers still remain
    // for this volume, its DIRTY bit will get set again, below.

    CommitVolumeBuffers(pvol);
    pvol->v_flags &= ~VOLF_DIRTY;

    // If we have buffers carved from a single chunk of memory, then
    // quickly walk the entire list and determine if ALL the carved buffers
    // can be freed.  A single dirty or held carved buffer means that NONE
    // of the carved buffers can be freed.

    pbuf = dlBufMRU.pbufNext;
    pbufEnd = (PBUF)&dlBufMRU;

    if (pbBufCarve) {
        if (cBufVolumes == 1) {
            fFreeCarve = TRUE;
            
#ifdef TFAT
            //  no need to keep the buffer in TFAT
            if (!pvol->v_fTfat)
#endif
            {
                if (!(pvol->v_flags & VOLF_CLOSING)) {
                    while (pbuf != pbufEnd) {
                        if ((pbuf->b_flags & BUF_DIRTY) || HeldBuffer(pbuf)) {
                            DEBUGMSG(TRUE,(DBGTEXT("FATFS!FreeBufferPool: dirty/held buffer for block %d!\n"), pbuf->b_blk));
                            if (pbuf->b_flags & BUF_CARVED) {
                                fFreeCarve = fFree = FALSE;
    #ifndef DEBUG
                                break;  // in debug builds, we like to see all the dirty buffers
    #endif
                            }
                        }
                        pbuf = pbuf->b_dlink.pbufNext;
                    }
                }
            }
        }
        if (fFreeCarve) {
            DEBUGFREE(cDefBuffers * cbBuf);
            VERIFYTRUE(VirtualFree(pbBufCarve, 0, MEM_RELEASE));
            pbBufCarve = NULL;
        }
    }

    // Now walk the buffer list again, freeing every buffer that we can.

    pbuf = dlBufMRU.pbufNext;

    while (pbuf != pbufEnd) {

        BOOL fClean = !(pbuf->b_flags & BUF_DIRTY) && !HeldBuffer(pbuf);

//  no need to keep the buffer in TFAT
#ifdef TFAT
        if (pvol->v_fTfat || (pvol->v_flags & VOLF_CLOSING) )
#else            
        if (pvol->v_flags & VOLF_CLOSING) 
#endif
        {
            // The only reason we hold the buffer is so that CleanBuffer
            // won't generate a bogus assertion.  We don't care about the
            // data anymore, so it is otherwise pointless.

            if (pbuf->b_pvol == pvol) {
            	HoldBuffer(pbuf);
            	CleanBuffer(pbuf);
            	UnholdBuffer(pbuf);

            	fClean = TRUE;
            }
        }

        if (cBufVolumes > 1) {

            // If there are still other clients (ie, volumes), then all we
            // want to do is remove any buffer references to this volume
            // if it's being destroyed, so that we avoid false matches
            // if another volume with the same address gets allocated later.

            if (pbuf->b_pvol == pvol) {
                if (fClean) {
                    pbuf->b_pvol = NULL;
                } else {
                    pvol->v_flags |= VOLF_DIRTY;
                    fFree = FALSE;
                }
            }
        }
        else {

            ASSERT(pbuf->b_pvol == NULL || pbuf->b_pvol == pvol);

            if (fClean) {

                pbuf->b_pvol = NULL;

                // The current buffer is clean.  Now, if this is NOT a carved
                // buffer, or it is but we can free all the carved buffers anyway,
                // then free the current buffer.

                if (!(pbuf->b_flags & BUF_CARVED) || fFreeCarve) {

                    cbufTotal--;

                    if (!(pbuf->b_flags & BUF_CARVED)) {
                        DEBUGFREE(cbBuf);
                        if (flBuf & GBUF_LOCALALLOC)
                            VERIFYNULL(LocalFree((HLOCAL)pbuf->b_pdata));
                        else
                            VERIFYTRUE(VirtualFree(pbuf->b_pdata, 0, MEM_RELEASE));
                    }

                    RemoveItem((PDLINK)&pbuf->b_dlink);

                    DEBUGFREE(sizeof(BUF));
                    VERIFYNULL(LocalFree((HLOCAL)pbuf));

                    pbuf = dlBufMRU.pbufNext;
                    continue;
                }
            }
            else {
                ASSERT(pbuf->b_pvol == pvol);

                pvol->v_flags |= VOLF_DIRTY;
                fFree = FALSE;
            }
        }
        pbuf = pbuf->b_dlink.pbufNext;
    }

  //DEBUGMSG(cbufTotal, (DBGTEXT("FATFS!FreeBufferPool: %d buffers retained\n"), cbufTotal));

    // If there were no dirty buffers for this volume, then we can remove
    // its reference to buffer pool.

    if (!(pvol->v_flags & VOLF_DIRTY)) {
        pvol->v_flags &= ~VOLF_BUFFERED;
        cBufVolumes--;
    }

    LeaveCriticalSection(&csBuffers);

    return fFree;
}


/*  ModifyBuffer - Prepare to dirty buffer
 *
 *  ENTRY
 *      pbuf - pointer to buffer
 *      pMod - pointer to modification
 *      cbMod - length of modification (ZERO to prevent logging)
 *
 *  EXIT
 *      ERROR_SUCCESS if the buffer is allowed to be modified, otherwise
 *      some error code (eg, ERROR_WRITE_PROTECT).
 */

DWORD ModifyBuffer(PBUF pbuf, PVOID pMod, int cbMod)
{
    DWORD dwError = ERROR_SUCCESS;

    ASSERTHELDBUFFER(pbuf);

    if (pbuf->b_pvol->v_flags & VOLF_READONLY) {
        dwError = ERROR_WRITE_PROTECT;
    }

    if (!dwError)
        DirtyBuffer(pbuf);

    return dwError;
}


/*  DirtyBuffer - Dirty a buffer
 *
 *  ENTRY
 *      pbuf - pointer to BUF
 *
 *  EXIT
 *      The buffer is marked DIRTY.
 */

void DirtyBuffer(PBUF pbuf)
{
    ASSERTHELDBUFFER(pbuf);

    pbuf->b_flags |= BUF_DIRTY;
}


/*  DirtyBufferError - Dirty a buffer that cannot be cleaned
 *
 *  ENTRY
 *      pbuf - pointer to BUF
 *      pMod - pointer to modification (NULL if none)
 *      cbMod - length of modification (ZERO if none)
 *
 *  EXIT
 *      The buffer is marked DIRTY, and any error is recorded.
 */

void DirtyBufferError(PBUF pbuf, PVOID pMod, int cbMod)
{
    ASSERTHELDBUFFER(pbuf);

    pbuf->b_flags |= BUF_DIRTY;

    EnterCriticalSection(&csBuffers);

    if (!(pbuf->b_flags & BUF_ERROR)) {
        if (cbufError % MIN_BUFFERS == 0)
            InterlockedDecrement(&cBufThreads);
        cbufError++;
        pbuf->b_flags |= BUF_ERROR;
    }
    LeaveCriticalSection(&csBuffers);
}


/*  CommitBuffer - Commit a dirty buffer
 *
 *  ENTRY
 *      pbuf - pointer to BUF
 *      fCS - TRUE if csBuffers is held, FALSE if not
 *
 *  EXIT
 *      ERROR_SUCCESS (0) if successful, non-zero if not.
 *
 *  NOTES
 *      Since we clear BUF_DIRTY before calling WriteVolume, it's important
 *      we do something to prevent FindBuffer from getting any bright ideas
 *      about reusing it before we have finished writing the original dirty
 *      contents.  Furthermore, since we don't want to hold csBuffers across
 *      the potentially lengthy WriteVolume request, applying another hold to
 *      the buffer is really our only option.
 */

DWORD CommitBuffer(PBUF pbuf, BOOL fCS)
{
    PBYTE pbMod = NULL;
    int cbMod = 0;
    DWORD dwError = ERROR_SUCCESS;

#ifdef UNDER_CE 
    ASSERT(fCS == OWNCRITICALSECTION(&csBuffers));
#endif

    // If buffer is valid and dirty...

    if (pbuf->b_pvol && (pbuf->b_flags & BUF_DIRTY)) {

        if (pbuf->b_pvol->v_flags & VOLF_READONLY) {
            dwError = ERROR_WRITE_PROTECT;
        }
        
        DEBUGMSG(ZONE_BUFFERS,(DBGTEXT("FATFS!CommitBuffer(block %d)\n"), pbuf->b_blk));

        // Hold the buffer to insure that FindBuffer won't try to steal
        // it while the DIRTY bit is clear yet the buffer is still dirty.

        HoldBuffer(pbuf);


        // We clear the DIRTY bit before writing the buffer out, to
        // insure that we don't inadvertently lose some other thread's
        // modification to the buffer before our WriteVolume call is
        // complete.

        CleanBuffer(pbuf);

        // If the caller tells us that csBuffers is currently held, release
        // it across the WriteVolume call.

        pbuf->b_flags |= BUF_BUSY;

        // We used to release this critical section across a disk write but it would
        // sometimes cause data corruption on multithreaded writes with odd buffer
        // sizes. Beware if you try to "optimize" by adding the release back in.  
        //if (fCS)
        //    LeaveCriticalSection(&csBuffers);
        dwError = WriteVolume(pbuf->b_pvol, pbuf->b_blk, cblkBuf, pbuf->b_pdata);
        pbuf->b_flags &= ~BUF_BUSY;
        //if (fCS)
        //    EnterCriticalSection(&csBuffers);

        if (dwError) {
            if (dwError == ERROR_WRITE_PROTECT) {

                // Invalidate the buffer, it can't be written and is presumably
                // now out of sync with the disk's actual contents.

                pbuf->b_pvol = NULL;
            }
            else {

                // If the cause of failure might simply be that the disk has been
                // yanked, we can always hope that the user will put the disk back
                // in, so let's just re-dirty the buffer.

                DirtyBufferError(pbuf, pbMod, cbMod);
            }
        }
        UnholdBuffer(pbuf);
    }
    return dwError;
}


/*  CleanBuffer
 *
 *  ENTRY
 *      pbuf - pointer to BUF
 *
 *  EXIT
 *      The buffer is marked CLEAN, if it wasn't already.
 */

void CleanBuffer(PBUF pbuf)
{
    ASSERTHELDBUFFER(pbuf);

    pbuf->b_flags &= ~BUF_DIRTY;

    if (pbuf->b_flags & BUF_ERROR) {

        EnterCriticalSection(&csBuffers);

        if (pbuf->b_flags & BUF_ERROR) {
            cbufError--;
            if (cbufError % MIN_BUFFERS == 0)
                InterlockedIncrement(&cBufThreads);
            pbuf->b_flags &= ~BUF_ERROR;
        }

        LeaveCriticalSection(&csBuffers);
    }
}

/*  FindBuffer
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      blk - desired block on VOLUME
 *      pstm - pointer to DSTREAM containing block
 *      fNoRead - TRUE to skip reading the coresponding disk data
 *      ppbuf - pointer to address of BUF (to be returned)
 *
 *  EXIT
 *      ERROR_SUCCESS (0) if successful, non-zero if not.
 */

DWORD FindBuffer(PVOLUME pvol, DWORD blk, PDSTREAM pstm, BOOL fNoRead, PBUF *ppbuf)
{
    PBUF pbuf, pbufOld, pbufEnd;
    DWORD dwError = ERROR_SUCCESS;

    DEBUGMSG(ZONE_LOGIO && pstm && (pstm->s_flags & STF_DEMANDPAGED),(DBGTEXT("FATFS!FindBuffer(DP): begin entercritsec\n")));
    EnterCriticalSection(&csBuffers);
    DEBUGMSG(ZONE_LOGIO && pstm && (pstm->s_flags & STF_DEMANDPAGED),(DBGTEXT("FATFS!FindBuffer(DP): end entercritsec\n")));

    DEBUGMSG(ZONE_BUFFERS,(DBGTEXT("FATFS!FindBuffer(block %d, stream %.11hs)\n"), blk, pstm? pstm->s_achOEM : "<NONE>"));

    // Make sure the volume is allowed to use the buffer pool

    if (!(pvol->v_flags & VOLF_BUFFERED)) {
        DEBUGMSG(TRUE,(DBGTEXT("FATFS!FindBuffer: volume not allowed to use buffer pool yet!\n")));
        dwError = ERROR_GEN_FAILURE;
        goto error;
    }

    // If an associated stream is specified, the volume better match

    ASSERT(!pstm || pstm->s_pvol == pvol);

    // If the stream has a valid pbufCur, we'll check that buffer
    // first, but otherwise we'll walk the buffers in MRU order.  If we
    // don't find the desired block, then we'll read the data into the
    // oldest unheld buffer we saw.

    if (pstm && (pbuf = pstm->s_pbufCur)) {
        if (pbuf->b_pvol && pbuf->b_pvol == pvol && pbuf->b_blk == blk)
            goto foundLastBuffer;
        ReleaseStreamBuffer(pstm, TRUE);    
    }

    // Search the buffer pool in MRU order, and also keep track of
    // a likely candidate to re-use (pbufOld), in case we don't find what 
    // we're looking for.

restart:
    pbuf = dlBufMRU.pbufNext;
    pbufEnd = (PBUF)&dlBufMRU;

    pbufOld = NULL;
    while (pbuf != pbufEnd) {

        if (pbuf->b_pvol && pbuf->b_pvol == pvol && pbuf->b_blk == blk) {

            // An existing buffer might be in the BUF_UNCERTAIN state (ie, if another
            // thread had wanted the same buffer, started reading it, but hadn't finished
            // by the time *we* came along).  In that case, we must block until the
            // BUF_UNCERTAIN flag is cleared.

            while (pbuf->b_flags & BUF_UNCERTAIN) {
                DEBUGMSG(TRUE,(DBGTEXT("FATFS!FindBuffer: waiting for block %d to become certain...\n"), pbuf->b_blk));
                Sleep(2000);
            }

            // Furthermore, if an UNCERTAIN buffer could not be read, then we will have
            // set b_pvol to NULL to invalidate the buffer.  We need to check for that condition
            // now;  it needs to be checked outside the UNCERTAIN loop, since the timing might
            // have been such that we never even saw the UNCERTAIN bit get set.

            if (pbuf->b_pvol == NULL)
                goto restart;
            else
                goto foundBuffer;
        }

        // If this buffer isn't held, pick it.  The only exception is
        // if we already picked (a newer) one and this (older) one's dirty.
	 // also, if the picked one was dirty, we would want to pick older dirty one

        if (!HeldBuffer(pbuf) && !(pbuf->b_flags & BUF_BUSY)) {
// #ifdef TFAT
//            if (!pbufOld || !(pbuf->b_flags & BUF_DIRTY))
// #else
            if (!pbufOld || !(pbuf->b_flags & BUF_DIRTY) || (pbufOld->b_flags & BUF_DIRTY))
 // #endif
                pbufOld = pbuf;
        }

        pbuf = pbuf->b_dlink.pbufNext;
    }

    // There should always be an unheld buffer in the pool (which we
    // now assert).  If there isn't, then MIN_BUFFERS has been set too low
    // (ie, the current thread must already be holding onto at least that
    // many buffers, or some other thread is holding onto more than MIN_BUFFERS).

    if (!pbufOld) {
        DEBUGMSGBREAK(TRUE,(DBGTEXT("FATFS!FindBuffer: buffer pool unexpectedly exhausted!\n")));
        dwError = ERROR_GEN_FAILURE;
        goto error;
    }

    pbuf = pbufOld;

    // Make sure any buffer we're about to (re)use has been committed;  the
    // only time we should end up using a dirty buffer is when no clean ones
    // are available, and if we can't commit a particular buffer, we probably
    // can't commit *any* of them, which is why I just give up.

    DEBUGMSG(ZONE_LOGIO && pstm && (pstm->s_flags & STF_DEMANDPAGED),(DBGTEXT("FATFS!FindBuffer(DP): begin commitbuffer\n")));
    dwError = CommitBuffer(pbuf, TRUE);
    DEBUGMSG(ZONE_LOGIO && pstm && (pstm->s_flags & STF_DEMANDPAGED),(DBGTEXT("FATFS!FindBuffer(DP): end commitbuffer\n")));
    if (dwError)
        goto error;

    pbuf->b_pvol = pvol;
    pbuf->b_blk = blk;
#ifdef DEBUG
    pbuf->b_refs = 0;
#endif

    // WARNING: The buffer critical section is not held across the actual
    // read, to insure that critical I/O (like demand-paging) can still occur
    // even if other threads start stacking up inside the driver.  Since
    // BUF_UNCERTAIN is set for the duration, no other threads entering FindBuffer
    // for the same block/buffer should continue until we're done here.

    if (!fNoRead) {
        pbuf->b_flags |= BUF_UNCERTAIN | BUF_BUSY;
        LeaveCriticalSection(&csBuffers);
        DEBUGMSG(ZONE_LOGIO && pstm && (pstm->s_flags & STF_DEMANDPAGED),(DBGTEXT("FATFS!FindBuffer(DP): begin readvolume\n")));
        dwError = ReadVolume(pvol, pbuf->b_blk, cblkBuf, pbuf->b_pdata);
        DEBUGMSG(ZONE_LOGIO && pstm && (pstm->s_flags & STF_DEMANDPAGED),(DBGTEXT("FATFS!FindBuffer(DP): end readvolume\n")));
        if (dwError) {
            // Invalidate the buffer if the data could not be read, and do it *before*
            // clearing BUF_UNCERTAIN, so that anyone waiting on BUF_UNCERTAIN cannot miss
            // the fact that this buffer is now invalid.
            pbuf->b_pvol = NULL;
            pbuf->b_flags &= ~(BUF_UNCERTAIN | BUF_BUSY);
            return dwError;
        }
        // We must clear BUF_UNCERTAIN *before* taking csBuffers, because otherwise we could
        // block forever due to another FindBuffer thread waiting for us to clear the same bit
        // on the same buffer.
        pbuf->b_flags &= ~BUF_UNCERTAIN;
        DEBUGMSG(ZONE_LOGIO && pstm && (pstm->s_flags & STF_DEMANDPAGED),(DBGTEXT("FATFS!FindBuffer(DP): begin entercritsec\n")));
        EnterCriticalSection(&csBuffers);
        DEBUGMSG(ZONE_LOGIO && pstm && (pstm->s_flags & STF_DEMANDPAGED),(DBGTEXT("FATFS!FindBuffer(DP): end entercritsec\n")));
        // We must clear BUF_BUSY *after* taking csBuffers, because otherwise the buffer is
        // vulnerable to re-use by another thread while (A) we're outside csBuffers and (B) no
        // hold has been applied yet.
        pbuf->b_flags &= ~BUF_BUSY;
    }

    // Move the buffer to MRU position now

  foundBuffer:
    RemoveItem((PDLINK)&pbuf->b_dlink);
    AddItem((PDLINK)&dlBufMRU, (PDLINK)&pbuf->b_dlink);

    // Apply a conventional hold or a stream hold, as appropriate

  foundLastBuffer:
    if (!pstm) {
        HoldBuffer(pbuf);
    }
    else {
        AssignStreamBuffer(pstm, pbuf, TRUE);
    }

#ifdef DEBUG
    pbuf->b_refs++;
#endif

    // Last but not least, return a pointer to the buffer structure

    *ppbuf = pbuf;

  error:
    LeaveCriticalSection(&csBuffers);
    return dwError;
}


/*  AssignStreamBuffer
 *
 *  ENTRY
 *      pstm - pointer to DSTREAM
 *      pbuf - pointer to buffer to assign to stream
 *      fCS - TRUE if csBuffers is held, FALSE if not
 *
 *  EXIT
 *      The buffer is held and assigned as the stream's current buffer.
 *
 *  NOTES
 *      Any caller messing with a stream's current buffer must also
 *      currently own the stream's critical section.
 */

void AssignStreamBuffer(PDSTREAM pstm, PBUF pbuf, BOOL fCS)
{
    ASSERT(OWNCRITICALSECTION(&pstm->s_cs));

    // If a buffer is changing hands, commit it first, because
    // CommitStreamBuffers for a given stream will only commit buffers
    // still associated with that stream, and we want to guarantee that
    // a stream is FULLY committed after calling CommitStreamBuffers.

    // We don't care if there's an error, because CommitBuffer no longer
    // invalidates a buffer on error;  it simply marks the buffer dirty again.

    if (pbuf->b_pstm != pstm) {
        if (pstm->s_flags & STF_WRITETHRU)
            CommitBuffer(pbuf, fCS);
    }

    pbuf->b_pstm = pstm;

#ifdef DEBUG
    memcpy(pbuf->b_achName, pstm->s_achOEM, min(sizeof(pbuf->b_achName),sizeof(pstm->s_achOEM)));
#endif

    if (!(pstm->s_flags & STF_BUFCURHELD)) {
        HoldBuffer(pbuf);
        pstm->s_pbufCur = pbuf;
        pstm->s_flags |= STF_BUFCURHELD;
    }
    else
        ASSERT(pstm->s_pbufCur == pbuf);
}


/*  ReleaseStreamBuffer
 *
 *  ENTRY
 *      pstm - pointer to DSTREAM
 *      fCS - TRUE if csBuffers is held, FALSE if not
 *
 *  EXIT
 *      The stream's current buffer is unheld, if it was currently in use.
 *
 *  NOTES
 *      Any caller messing with a stream's current buffer must also
 *      currently own the stream's critical section.
 */

DWORD ReleaseStreamBuffer(PDSTREAM pstm, BOOL fCS)
{
    DWORD dwError = ERROR_SUCCESS;

    ASSERT(OWNCRITICALSECTION(&pstm->s_cs));

    if (pstm->s_flags & STF_BUFCURHELD) {

        if ((pstm->s_flags & STF_WRITETHRU) && (pstm->s_pbufCur))
            dwError = CommitBuffer(pstm->s_pbufCur, fCS);

        pstm->s_flags &= ~STF_BUFCURHELD;
        UnholdBuffer(pstm->s_pbufCur);

        // Zap the buffer pointer to prevent anyone from misusing it

        pstm->s_pbufCur = NULL;
    }
    return dwError;
}


/*  ReadStreamBuffer
 *
 *  ENTRY
 *      pstm - pointer to DSTREAM
 *      pos - file position of data being requested
 *      lenMod - length of data to be modified (0 if not modifying)
 *      *ppvStart - address of pvStart variable to set to start of data
 *      *ppvEnd - address of pvEnd variable to set to end of buffer (optional)
 *
 *  EXIT
 *      ERROR_SUCCESS (0) if successful, non-zero if not.
 *
 *      The buffer containing the data is left held.  It can be explicitly
 *      unheld (eg, via ReleaseStreamBuffer or CommitAndReleaseStreamBuffers),
 *      or implicitly by another ReadStreamBuffer;  if a second ReadStreamBuffer
 *      returns a pointer to the same buffer, the buffer remains held, but the
 *      hold count is not advanced.  The idea is to relieve callers from having
 *      to pair all (or any of) their reads with unholds.
 *
 *  NOTES
 *      Callers promise that the requested POSITION (pos) lies within
 *      the current RUN (pstm->s_run).  The current RUN starts at some
 *      BLOCK (s_run.r_blk).  If the data is buffered, it will be in
 *      a buffer containing block ((pos - s_run.r_start)/BLOCK_SIZE + r_blk).
 */

DWORD ReadStreamBuffer(PDSTREAM pstm, DWORD pos, int lenMod, PVOID *ppvStart, PVOID *ppvEnd)
{
    PBUF pbuf;
    DWORD dwError;
    DWORD blk, blkBuf, offset;
    PBYTE pdata, pdataEnd;
    PVOLUME pvol = pstm->s_pvol;

    if (pos < pstm->s_run.r_start || pos >= pstm->s_run.r_end) {
        DEBUGMSGBREAK(ZONE_INIT || ZONE_ERRORS,
                 (DBGTEXT("FATFS!ReadStreamBuffer: bad position (%d) for stream %.11hs\n"), pos, pstm->s_achOEM));
        return ERROR_INVALID_DATA;
    }

    // Compute the stream-relative offset
    // and the corresponding volume-relative block

    offset = pos - pstm->s_run.r_start;
    blk = pstm->s_run.r_blk + (offset >> BLOCK_LOG2);

    // Compute the offset into the volume-relative block

    offset &= BLOCK_OFFSET_MASK;


    // Convert the volume-relative block # into a buffer-granular block #

    blkBuf = blk & maskBufBlock;

    // Find (or create) a buffer that starts with that buffer-granular
    // block number.  If lenMod >= cbBuf and blkBuf == blk and offset == 0,
    // then FindBuffer does not need to actuall fill the buffer with data from
    // the volume, because the entire buffer is to be modified anyway.

    DEBUGMSG(ZONE_LOGIO && (pstm->s_flags & STF_DEMANDPAGED),(DBGTEXT("FATFS!ReadStreamBuffer(DP): begin findbuffer\n")));
    dwError = FindBuffer(pvol, blkBuf, pstm,
                         (lenMod >= (int)cbBuf && blkBuf == blk && offset == 0),
                         &pbuf);
    DEBUGMSG(ZONE_LOGIO && (pstm->s_flags & STF_DEMANDPAGED),(DBGTEXT("FATFS!ReadStreamBuffer(DP): end findbuffer\n")));

    if (!dwError) {

        pdata = pbuf->b_pdata + ((blk - blkBuf) << BLOCK_LOG2) + offset;
        *ppvStart = pdata;

        // There's data all the way up to (buf_pdata + cbBuf-1), but it's
        // not necessarily VALID data.  We need to limit it to the amount of data
        // specified in the RUN.

        if (ppvEnd) {
            pdataEnd = pdata + (pstm->s_run.r_end - pos);
            if (pdataEnd > pbuf->b_pdata + cbBuf)
                pdataEnd = pbuf->b_pdata + cbBuf;
            *ppvEnd = pdataEnd;
        }
    }
    return dwError;
}


/*  ModifyStreamBuffer
 *
 *  ENTRY
 *      pstm - pointer to stream
 *      pMod - pointer to modification
 *      cbMod - length of modification (ZERO to prevent logging)
 *
 *  EXIT
 *      ERROR_SUCCESS if the stream's buffer is allowed to be modified, otherwise
 *      some error code (eg, ERROR_WRITE_PROTECT).
 */

DWORD ModifyStreamBuffer(PDSTREAM pstm, PVOID pMod, int cbMod)
{
    PBYTE pbMod = pMod;
    PBUF pbuf = pstm->s_pbufCur;
    
    // A just-dirtied buffer should also be a held buffer

    DEBUGMSG(ZONE_BUFFERS,(DBGTEXT("FATFS!ModifyStreamBuffer(block %d, stream %.11hs)\n"), pbuf->b_blk, pstm->s_achOEM));

    ASSERT(pbuf);
    ASSERT(pstm->s_flags & STF_BUFCURHELD);

    // Make sure the specified address lies within the buffer's range,
    // and then adjust the length if it extends past the end of the buffer

    ASSERT(pbMod >= pbuf->b_pdata && pbMod <= pbuf->b_pdata+cbBuf);

    if (cbMod > 0 && pbMod+cbMod > pbuf->b_pdata+cbBuf)
        cbMod = (pbuf->b_pdata+cbBuf) - pbMod;

    // If this stream contains file system data structures (FAT or directory info),
    // then pass the region being modified to ModifyBuffer so that it can do appropriate
    // buffer logging;  otherwise, pass a zero length to indicate that no logging
    // is required.

    return ModifyBuffer(pbuf, pbMod, (pstm->s_flags & STF_VOLUME)? cbMod : 0);
}




/*  CommitBufferSet
 *
 *  ENTRY
 *      pstm - pointer to stream if iCommit >= 0;
 *             pointer to volume (or NULL) if iCommit < 0
 *
 *      if iCommit >= 0 (ie, pstm points to stream):
 *          1 means release stream buffer; 0 means retain
 *
 *      if iCommit < 0 (ie, pstm points to volume, or NULL):
 *          -1 means commit everything not held;
 *          -2 means commit everything not held older than msWriteDelayMin
 *
 *  EXIT
 *      ERROR_SUCCESS (0) if successful, non-zero if not.
 *
 *  NOTES
 *      The assumption we make here is that a dirty buffer that is
 *      currently held will be explicitly committed once it is unheld,
 *      so we leave it alone.  Besides, the buffer could be in a
 *      transitional/incomplete state.
 *
 *      This doesn't mean that UnholdBuffer will do the commit for you
 *      either (because it won't).  Callers are allowed to hold/unhold
 *      dirtied buffers repeatedly without incuring the overhead of a
 *      write until they choose to do so, but they must choose to do so
 *      EVENTUALLY, in order to minimize the periods of time where the
 *      file system is in an inconsistent state.
 */

DWORD CommitBufferSet(PDSTREAM pstm, int iCommit)
{
    DWORD dw, dwError;
    PBUF pbuf, pbufEnd;

    // If the buffer pool hasn't been initialized yet, feign success

    if (cBufVolumes == 0)
        return ERROR_SUCCESS;


    dwError = ERROR_SUCCESS;
    EnterCriticalSection(&csBuffers);

    // If the caller is done with the stream's current buffer, then release
    // it first, so that the logic below (to commit only those buffers that are
    // no longer held) does the right thing.
    
    if (iCommit > 0 && pstm)
        dwError = ReleaseStreamBuffer(pstm, TRUE);

    pbuf = dlBufMRU.pbufPrev;
    pbufEnd = (PBUF)&dlBufMRU;

    while (pbuf != pbufEnd) {

        // If you didn't want to release the current buffer (ie, you want to
        // commit it even though you -- or someone else -- is still holding it)
        // then we'll commit ALL the stream's buffers (held or not), on the premise
        // that what you're *really* trying to do is get the entire stream into a
        // consistent state (eg, a directory stream that was both modified and
        // grown).

        if (!pstm ||
            iCommit >= 0 && pbuf->b_pstm == pstm ||
            iCommit <  0 && pbuf->b_pvol == (PVOLUME)pstm) {

            BOOL fCommit = (iCommit == 0 || !HeldBuffer(pbuf));
            
            //TEST_BREAK
            PWR_BREAK_NOTIFY(61);

            if (fCommit) {
                dw = CommitBuffer(pbuf, TRUE);
                if (!dwError)
                    dwError = dw;
            }
        }
        pbuf = pbuf->b_dlink.pbufPrev;
    }

    LeaveCriticalSection(&csBuffers);

    return dwError;
}


/*  InvalidateBufferSet - Invalidate all buffers for specified volume
 *
 *  ENTRY
 *      pvol -> VOLUME structure
 *      fAll == FALSE to invalidate only clean buffers, TRUE to also invalidate dirty ones
 *
 *  EXIT
 *      None
 */

void InvalidateBufferSet(PVOLUME pvol, BOOL fAll)
{
    BOOL fDirty;
    PBUF pbuf, pbufEnd;

    EnterCriticalSection(&csBuffers);

    fDirty = FALSE;
    pbuf = dlBufMRU.pbufNext;
    pbufEnd = (PBUF)&dlBufMRU;

    while (pbuf != pbufEnd) {

        // If this buffer contains data that belonged to the specified
        // volume, and no thread is using that data, invalidate the buffer.

        if (pbuf->b_pvol == pvol) {
            
            BOOL fInvalidate = !HeldBuffer(pbuf) && (fAll || !(pbuf->b_flags & BUF_DIRTY));

            DEBUGMSG(fInvalidate && (pbuf->b_flags & BUF_DIRTY), (DBGTEXT("FATFS!InvalidateBufferSet: dirty buffer for block %d being invalidated!\n"), pbuf->b_blk));

            // The only reason we hold the buffer is so that CleanBuffer
            // won't generate a bogus assertion.  We don't care about the
            // data anymore, so it is otherwise pointless.

            if (fInvalidate) {
                HoldBuffer(pbuf);
                CleanBuffer(pbuf);
                UnholdBuffer(pbuf);
                pbuf->b_pvol = NULL;    // invalidated!
            }
            else if (pbuf->b_flags & BUF_DIRTY)
                fDirty = TRUE;
        }
        pbuf = pbuf->b_dlink.pbufNext;
    }

    // If all dirty buffers were invalidated, the volume should no longer be considered dirty.

    if (!fDirty)
        pvol->v_flags &= ~VOLF_DIRTY;

    LeaveCriticalSection(&csBuffers);
}


/*  LockBlocks - Lock a range of blocks for direct (non-buffered) I/O
 *
 *  ENTRY
 *      pstm - pointer to DSTREAM
 *      pos - file position of data being requested
 *      len - length of data being requested
 *      pblk - pointer to first unbuffered block (if successful)
 *      pcblk - pointer to count of unbuffered blocks (if successful)
 *      fWrite - FALSE if locking for read, TRUE if write
 *
 *  EXIT
 *      TRUE if successful (ie, the request starts at a buffer-granular
 *      block number and spans some multiple of blocks-per-buffer, none
 *      of which are currently buffered), FALSE if not.
 *
 *  NOTES
 *      If the call returns TRUE, then the caller can issue ReadVolume or
 *      WriteVolume directly.  The buffer critical section is left held on
 *      return for writes only -- to avoid races that might result in
 *      buffer pool inconsistency -- so it is critical that UnlockBlocks(TRUE)
 *      be called when done writing a range of blocks.
 *
 *      As with ReadStreamBuffer, callers promise that the requested POSITION
 *      (pos) lies within the current RUN (pstm->s_run).  The current RUN
 *      starts at some BLOCK (s_run.r_blk).  If the data is buffered, it will
 *      be in a buffer containing block ((pos - s_run.r_start)/BLOCK_SIZE + r_blk).
 */

BOOL LockBlocks(PDSTREAM pstm, DWORD pos, DWORD len, DWORD *pblk, DWORD *pcblk, BOOL fWrite)
{
    PBUF pbuf, pbufEnd;
    DWORD blk, blkBuf, blkBufEnd;
    DWORD offset, cblks;
    PVOLUME pvol = pstm->s_pvol;

    if (pos < pstm->s_run.r_start || pos >= pstm->s_run.r_end) {
        DEBUGMSGBREAK(ZONE_INIT || ZONE_ERRORS,
                 (DBGTEXT("FATFS!LockBlocks: bad position (%d) for stream %.11hs\n"), pos, pstm->s_achOEM));
        return FALSE;
    }

    // Compute the stream-relative offset
    // and the corresponding volume-relative block

    offset = pos - pstm->s_run.r_start;
    blk = pstm->s_run.r_blk + (offset >> BLOCK_LOG2);

    // Limit the size of the request to what exists in the current run

    if (len > pstm->s_run.r_end - pos)
        len = pstm->s_run.r_end - pos;

    // Convert the volume-relative block # into a buffer-granular block #

    blkBuf = blk & maskBufBlock;
    if (blkBuf != blk || (offset & BLOCK_OFFSET_MASK) || (cblks = len >> BLOCK_LOG2) < cblkBuf) {

        // The request does not start at the beginning of a buffer-granular
        // block, or does not start at the beginning of that block, or is
        // smaller the number of blocks in a buffer.

        






        return FALSE;
    }

    // Compute an ending buffer-granular block #, by adding cblks after
    // it has been truncated to the number of blocks in a buffer.

    blkBufEnd = blkBuf + (cblks & ~(cblkBuf-1));

    // Now we just need to find the largest cblkBuf chunk of blocks that
    // is not currently buffered.

    EnterCriticalSection(&csBuffers);

    pbuf = dlBufMRU.pbufNext;
    pbufEnd = (PBUF)&dlBufMRU;

    while (pbuf != pbufEnd) {

        if (pbuf->b_pvol && pbuf->b_pvol == pvol) {

            // If the first block is buffered, then fail the request.

            if (pbuf->b_blk == blkBuf) {
                blkBuf = blkBufEnd;
                break;
            }

            // If this buffer contains some information inside the range
            // being requested, then shrink the range down.

            if (pbuf->b_blk > blkBuf && pbuf->b_blk < blkBufEnd) {
                blkBufEnd = pbuf->b_blk;
            }
        }
        pbuf = pbuf->b_dlink.pbufNext;
    }

    if (blkBuf != blkBufEnd) {
        *pblk = blkBuf;
        *pcblk = blkBufEnd - blkBuf;
        if (!fWrite)
            LeaveCriticalSection(&csBuffers);
        return TRUE;
    }

    LeaveCriticalSection(&csBuffers);
    return FALSE;
}


void UnlockBlocks(DWORD blk, DWORD cblk, BOOL fWrite)
{
    if (fWrite)
        LeaveCriticalSection(&csBuffers);
}
