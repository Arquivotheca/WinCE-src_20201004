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

#include <kernel.h>


#ifndef _PREFAST_
#pragma warning(disable: 4068) // Disable pragma warnings
#endif

#define MODNAME TEXT("CeLog")
#ifdef DEBUG
DBGPARAM dpCurSettings = { MODNAME, {
        TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"),
        TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"),
        TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"),
        TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>") },
    0x00000000};
#endif

#define ZONE_VERBOSE 0

#ifndef VM_PAGE_SIZE
#define VM_PAGE_SIZE PAGE_SIZE
#endif // VM_PAGE_SIZE

#ifndef VM_KMODE_BASE
#define VM_KMODE_BASE 0x80000000
#endif // VM_KMODE_BASE


//------------------------------------------------------------------------------
// MISC

// RESERVED zones cannot be turned on, CELZONE_ALWAYSON cannot be turned off
// (BUGBUG need to block unused zones too, ideally by querying AVAILABLE_ZONES from logger.c)
#define VALIDATE_ZONES(dwVal)   (((dwVal) & ~(CELZONE_RESERVED1|CELZONE_RESERVED2)) | CELZONE_ALWAYSON)

#define MAX_ROLLOVER_MINUTES    30
#define MAX_ROLLOVER_COUNT      (0xFFFFFFFF / (MAX_ROLLOVER_MINUTES * 60))

// Historically this has been a limit, and now is needed to make room for the
// CPU# in the event header
#define MAX_DATA_SIZE           (4096 - 1)

// 1GB max used to sanity check the size stored in the map
#define MAX_MAP_SIZE            (1*1024*1024*1024)

#define DATAMAP_NAME            CELOG_DATAMAP_NAME
#define FILLEVENT_NAME          CELOG_FILLEVENT_NAME

#define REGKEY_CELOG                 TEXT("System\\CeLog")
#define REGKEY_CELOG_PASSED_SETTINGS TEXT("System\\CeLog\\PassedSettings")

#define REGVAL_BUFFER_ADDR      TEXT("BufferAddress")
#define REGVAL_BUFFER_SIZE      TEXT("BufferSize")
#define REGVAL_SYNC_BUFFER_SIZE TEXT("SyncBufferSize")
#define REGVAL_AUTO_ERASE_MODE  TEXT("AutoEraseMode")
#define REGVAL_ZONE_CE          TEXT("ZoneCE")


//------------------------------------------------------------------------------
// EXPORTED GLOBAL VARIABLES

// Default address of main memory buffer (0=dynamically allocated)
#ifndef MAP_ADDR
#define MAP_ADDR                0
#endif

// Default size of main memory buffer
#ifndef MAP_SIZE
#define MAP_SIZE                128*1024
#endif

// The interrupt buffer no longer requires 8-byte alignment, but we still align
#define INT_ALIGN_DOWN(x)  (((x)                             ) & ~(sizeof(CEL_INT_DATA) - 1))
#define INT_ALIGN_UP(x)    (((x) + (sizeof(CEL_INT_DATA) - 1)) & ~(sizeof(CEL_INT_DATA) - 1))

// Size of interrupt buffer is fixed
#define INTBUF_SIZE             (INT_ALIGN_DOWN(MAX_DATA_SIZE - sizeof(DWORD)))  // -sizeof(DWORD) to fit inside MAX_DATA_SIZE after discard count is added
// Keep interrupt events within the legal event size
C_ASSERT(INTBUF_SIZE + sizeof(DWORD) <= MAX_DATA_SIZE);

// Sync buffer is carved out of main memory buffer (0=no sync buffer)
#ifndef SYNCBUF_SIZE
#define SYNCBUF_SIZE            (MAP_SIZE/4)
#endif

#define HEADER_SIZE             (INT_ALIGN_UP(sizeof(MAPHEADER)))

//------------------------------------------------------------------------------

typedef struct {
    LPBYTE pBuffer;
    DWORD  dwSize;
    LPBYTE pWrite;
    DWORD* pdwWriteOffset;      // Pointer to mmfile write offset, write-only (never read)
} DataBuf;

static struct {
    DWORD  ZoneUser;            // User zone mask
    DWORD  ZoneCE;              // Predefined zone mask (CELZONE_xxx)
    
    BOOL   Initialized;
    DWORD  dwPerfTimerShift;
    BOOL   AutoEraseMode;       // TRUE: Assume there is no flush app, and discard data to make room.
                                // FALSE: Expect the flush app to make room.
    DWORD  Logging;             // Count of active loggers; used to test if interrupt handler can
                                // safely flush the interrupt buffer
    BOOL   NeedSetEvent;        // Used to cause LogData to wake flush thread
    HANDLE hFillEvent;          // Used to indicate the secondary buffer is getting full
    HANDLE hMap;                // Used for RingBuf memory mapping
    PMAPHEADER pHeader;         // Pointer to RingBuf map header

    //
    // Local copy of header data prevents tampering from apps
    //

    // Primary data buffer (ring buffer) state 
    DataBuf RingBuf;

    // Interrupt buffer state - also a ring buffer
    // Buffer empty: pWrite == pRead
    // Buffer full: pRead == pWrite + sizeof(CEL_INT_DATA)
    DataBuf IntBuf;        // pWrite only moved by LogInterrupt / LogInterruptData
    LPBYTE  IntBuf_pRead;  // pRead only moved by FlushInterruptBuffer
    DWORD   dwDiscardedInterrupts;
    // The interrupt buffer holds either interrupt data, or Monte Carlo profiling data
    // (which is also logged during an interrupt), but not both.  We only add interrupt
    // headers to interrupt data.  Monte Carlo turns off the interrupt zone during
    // profiling, and SetZones prevents both from being turned on at the same time.
    // If the user turns on both zones at the same time, it won't hurt anything since
    // Monte Carlo won't be logging unless someone starts profiling.
    BOOL    IntBufNeedsHeader;

    // "Sync" buffer to store thread, process & module list, for use with
    // auto-erase mode.  This way it is easier to make sense of the data.
    DataBuf SyncBuf;
    LPBYTE  SyncBuf_pRead; // Not a ring buffer, pRead never moves

} g_CeLog;

// Inputs from kernel
CeLogImportTable    g_PubImports;
FARPROC             g_pfnKernelLibIoControl;


//------------------------------------------------------------------------------
// Abstract accesses to imports

#ifndef SHIP_BUILD
#undef RETAILMSG
#define RETAILMSG(cond, printf_exp) ((cond)?((g_PubImports.pNKDbgPrintfW printf_exp),1):0)
#endif // SHIP_BUILD

#ifdef DEBUG
#undef DEBUGMSG
#define DEBUGMSG                 RETAILMSG
#endif // DEBUG

#ifdef DEBUG
#undef DBGCHK
#undef DEBUGCHK
#define DBGCHK(module,exp)                                                     \
   ((void)((exp)?1                                                             \
                :(g_PubImports.pNKDbgPrintfW(TEXT("%s: DEBUGCHK failed in file %s at line %d\r\n"), \
                                             (LPWSTR)module, TEXT(__FILE__), __LINE__), \
                  DebugBreak(),                                                \
                  0                                                            \
                 )))
#define DEBUGCHK(exp) DBGCHK(dpCurSettings.lpszName, exp)
#endif // DEBUG


//------------------------------------------------------------------------------
// Helper function to avoid code duplication
//------------------------------------------------------------------------------
static __inline DWORD GetBytesLeft (DataBuf* pBuf, LPBYTE pReadCopy)
{
    DWORD dwBytesLeft = pReadCopy - pBuf->pWrite + pBuf->dwSize;
    if (dwBytesLeft > pBuf->dwSize) {
        // wrapped.
        dwBytesLeft -= pBuf->dwSize;
    }
    return dwBytesLeft;
}


//------------------------------------------------------------------------------
// This worker takes generic data buf info, but is only used with the main
// buffer right now.
// Returns TRUE if there is enough free space to store the given amount of data.
//------------------------------------------------------------------------------
static BOOL
EnsureFreeSpace (DataBuf* pBuf, DWORD* pdwReadOffset, DWORD cbToWrite,
                 BOOL fAutoErase, BOOL fSetEvent)
{
    LPBYTE pReadCopy;
    DWORD  dwBytesLeft;

    // Only read the read offset from the header once, in case it changes asynchronously
    pReadCopy = (*pdwReadOffset + pBuf->pBuffer);

    // Safety check the read pointer, since the mapfile could have been corrupted
    if ((pReadCopy < pBuf->pBuffer) || (pReadCopy >= pBuf->pBuffer + pBuf->dwSize)) {
        RETAILMSG (ZONE_VERBOSE, (MODNAME TEXT(": ERROR, Secondary Buffer read pointer is corrupt (0x%08x).\r\n"),
                                  pReadCopy));
        return FALSE;
    }

    dwBytesLeft = GetBytesLeft (pBuf, pReadCopy);
    
    if (!fAutoErase) {
        // Set the flush event if the buffer passes 3/4 full.
        // SetEvent can generate CeLog events (and change the # bytes left), 
        // and if only interrupt and/or thread switch zone is on, this code will
        // only run during a SysCall so we can't call SetEvent here anyway.
        if (fSetEvent && (dwBytesLeft < pBuf->dwSize / 4)) {
            g_CeLog.NeedSetEvent = TRUE;  // Interlocking not required: it's OK to set to TRUE too many times
        }

        if (dwBytesLeft <= cbToWrite) {
            // Not enough room to write
            DEBUGMSG (ZONE_VERBOSE, (MODNAME TEXT(": Secondary Buffer full.\r\n")));
            return FALSE;
        }

    } else {
        // Clear space as necessary
        while (dwBytesLeft <= cbToWrite) {
            DWORD EventHeader = *((DWORD*) pReadCopy);
            BOOL  HasTimestamp = (EventHeader & CEL_HEADER_TIMESTAMP);
            BOOL  IsFlagged =   ((EventHeader & CEL_HEADER_ID_MASK)  == (CELID_FLAGGED << 16));
            BOOL  HasCPU =      ((EventHeader & CEL_HEADER_CPU_MASK) == (15 << 12));
            WORD  wLen = (WORD)  (EventHeader & CEL_HEADER_LENGTH_MASK);

            // Compute the full event size, header + data
            wLen += sizeof(DWORD);
            if (HasTimestamp) {
                wLen += sizeof(DWORD);
            }
            if (IsFlagged) {
                wLen += sizeof(DWORD);
            }
            if (HasCPU) {
                wLen += sizeof(DWORD);
            }
            wLen = (wLen + 3) & ~3;  // DWORD align

            // Advance the read pointer past the event, modulo buffer size
            pReadCopy += wLen;
            if (pReadCopy >= pBuf->pBuffer + pBuf->dwSize) {
                pReadCopy -= pBuf->dwSize;
            }
            *pdwReadOffset = pReadCopy - pBuf->pBuffer;

            dwBytesLeft = GetBytesLeft (pBuf, pReadCopy);
        }
    }

    // There is enough space to write the given number of bytes.
    return TRUE;
}


//------------------------------------------------------------------------------
// Used for the sync buffer and the interrupt buffer.  Returns the write pointer
// if there is enough free space to store the given amount of data.  We use
// read-once semantics on the write pointer for a little bit more timing safety
// on the interrupt buffer; the sync buffer doesn't need it but it doesn't hurt.
// Flush apps can't advance the read pointers on these buffers so we never read
// any read offsets from g_CeLog.pHeader.
//------------------------------------------------------------------------------
static LPBYTE
HasFreeSpace (DataBuf* pBuf, LPBYTE pBuf_pRead, DWORD cbToWrite)
{
    // Only read the write offset once, in case it changes asynchronously
    LPBYTE pWriteCopy = pBuf->pWrite;
    DWORD dwBytesLeft = pBuf_pRead - pWriteCopy + pBuf->dwSize;
    if (dwBytesLeft > pBuf->dwSize) {
        // wrapped.
        dwBytesLeft -= pBuf->dwSize;
    }
    return (dwBytesLeft > cbToWrite) ? pWriteCopy : NULL;
}


//------------------------------------------------------------------------------
// These workers assume that we have already made space for the data.
//------------------------------------------------------------------------------

static __inline void WriteDWORD (DataBuf* pBuf, DWORD data)
{
    // All data is DWORD-aligned so this write will never cross the end of the buffer
    *((LPDWORD) pBuf->pWrite) = data;
    pBuf->pWrite += sizeof(DWORD);
    
    if (pBuf->pWrite >= pBuf->pBuffer + pBuf->dwSize) {
        pBuf->pWrite = pBuf->pBuffer;
    }
}

// Same as WriteData but provides read-once semantics on the write pointer.
// Returns the new write pointer.
static __inline LPBYTE WriteDataWithPointer (DataBuf* pBuf, LPBYTE pWriteCopy, LPBYTE pData, DWORD cbData)
{
    // 1 or 2 memcpy calls, depending on whether the write crosses the end of the buffer
    DWORD cbToEndOfBuffer = pBuf->pBuffer + pBuf->dwSize - pWriteCopy;
    DWORD cbWrite1 = min (cbToEndOfBuffer, cbData);
#pragma prefast(disable: 22018, "cbWrite1 is guaranteed to be less than or equal to cbData")
    DWORD cbWrite2 = (cbData - cbWrite1);
#pragma prefast(pop)

    memcpy (pWriteCopy, pData, cbWrite1);
    if (!cbWrite2) {
        cbWrite1 = (cbWrite1 + 3) & ~3;  // DWORD align
        pWriteCopy += cbWrite1;
        if (pWriteCopy >= pBuf->pBuffer + pBuf->dwSize) {
            pWriteCopy = pBuf->pBuffer;
        }
    } else {
        memcpy (pBuf->pBuffer, pData + cbWrite1, cbWrite2);
        cbWrite2 = (cbWrite2 + 3) & ~3;  // DWORD align
        pWriteCopy = pBuf->pBuffer + cbWrite2;
    }
    return pWriteCopy;
}


static __inline void WriteData (DataBuf* pBuf, LPBYTE pData, DWORD cbData)
{
    pBuf->pWrite = WriteDataWithPointer (pBuf, pBuf->pWrite, pData, cbData);
}

static __inline void CompleteWrite (DataBuf* pBuf)
{
    // Update map header
    *pBuf->pdwWriteOffset = pBuf->pWrite - pBuf->pBuffer;
}


//------------------------------------------------------------------------------
// Copy interrupt and TLB events to the main buffer.
// We hold the CeLog spinlock during this call, but it may still be interrupted.
//------------------------------------------------------------------------------
static void
FlushInterruptBuffer (BOOL fForce)
{
    static DWORD s_dwTLBPrev = 0;
    static DWORD s_LastDiscardCount = 0;
    DWORD dwSrcLen1, dwSrcLen2, dwSrcTotal;
    LPBYTE pWrite = g_CeLog.IntBuf.pWrite;  // Only read once, in case we're interrupted
    DWORD dwDiscarded = g_CeLog.dwDiscardedInterrupts;  // Only read once

    DEBUGCHK(g_CeLog.Logging);
    
    // Determine where the data is in the buffer
    if (pWrite >= g_CeLog.IntBuf_pRead) {
        //   pBuffer                 pWrap
        //   v                       v
        //  +-----------------------+ 
        //  |      |====1===|       | 
        //  +-----------------------+ 
        //          ^        ^
        //          pRead    pWrite 
        dwSrcLen1 = pWrite - g_CeLog.IntBuf_pRead;
        dwSrcLen2 = 0;

    } else {
        //   pBuffer                 pWrap
        //   v                       v
        //  +-----------------------+ 
        //  |===2==|        |===1===| 
        //  +-----------------------+ 
        //          ^        ^
        //          pWrite   pRead  
        dwSrcLen1 = g_CeLog.IntBuf.pBuffer + g_CeLog.IntBuf.dwSize - g_CeLog.IntBuf_pRead;
        dwSrcLen2 = pWrite - g_CeLog.IntBuf.pBuffer;
    }
    dwSrcTotal = dwSrcLen1 + dwSrcLen2;

    // Flush if forced or if the interrupt buffer is greater than half full.
    if (!fForce && (dwSrcTotal < INTBUF_SIZE / 2)) {
        return;
    }

    if (dwSrcTotal) {
        BOOL  fAddIntHeader = g_CeLog.IntBufNeedsHeader;
        DWORD cbWrite = dwSrcTotal + (fAddIntHeader ? 2*sizeof(DWORD) : 0);  // Header + discard count
        if (EnsureFreeSpace (&g_CeLog.RingBuf, &g_CeLog.pHeader->dwReadOffset, cbWrite,
                             g_CeLog.AutoEraseMode, g_CeLog.pHeader->fSetEvent)) {
            if (fAddIntHeader) {
                WriteDWORD (&g_CeLog.RingBuf, CELID_INTERRUPTS << 16 | (WORD) (dwSrcTotal + sizeof(DWORD)));
                WriteDWORD (&g_CeLog.RingBuf, dwDiscarded - s_LastDiscardCount);
                s_LastDiscardCount = dwDiscarded;
            }
            if (dwSrcLen1) {
                WriteData (&g_CeLog.RingBuf, g_CeLog.IntBuf_pRead, dwSrcLen1);
                g_CeLog.IntBuf_pRead += dwSrcLen1;
            }
            if (dwSrcLen2) {
                WriteData (&g_CeLog.RingBuf, g_CeLog.IntBuf.pBuffer, dwSrcLen2);
                g_CeLog.IntBuf_pRead = g_CeLog.IntBuf.pBuffer + dwSrcLen2;
            }
            CompleteWrite (&g_CeLog.RingBuf);
        
        } else {
            RETAILMSG(ZONE_VERBOSE, (MODNAME TEXT(": ERROR, Secondary Buffer Overrun. Dropping %d bytes (interrupts)\r\n"),
                                     dwSrcTotal + 2*sizeof(DWORD)));
            // Drop the data by moving the read pointer
            if (dwSrcLen2) {
                g_CeLog.IntBuf_pRead = g_CeLog.IntBuf.pBuffer + dwSrcLen2;
            } else {
                g_CeLog.IntBuf_pRead += dwSrcLen1;
            }
            InterlockedExchangeAdd ((volatile LONG*) &g_CeLog.pHeader->dwLostBytes, dwSrcTotal + 2*sizeof(DWORD));
        }

        // Update map header
        g_CeLog.pHeader->dwIntReadOffset = pWrite - g_CeLog.IntBuf.pBuffer;
    }


    // Copy in the TLB Misses that happened since the last flush
    if ((g_CeLog.ZoneCE & CELZONE_TLB) && g_PubImports.pdwCeLogTLBMiss) {
        DWORD dwTLBCopy = *g_PubImports.pdwCeLogTLBMiss;
        if ((dwTLBCopy != s_dwTLBPrev)
            && EnsureFreeSpace (&g_CeLog.RingBuf, &g_CeLog.pHeader->dwReadOffset,
                                2*sizeof(DWORD), g_CeLog.AutoEraseMode,
                                g_CeLog.pHeader->fSetEvent)) {  // If not enough space, log it later
        
            // Write in the TLB miss event
            WriteDWORD (&g_CeLog.RingBuf, CELID_SYSTEM_TLB << 16 | sizeof(CEL_SYSTEM_TLB));
            WriteDWORD (&g_CeLog.RingBuf, dwTLBCopy - s_dwTLBPrev);
            CompleteWrite (&g_CeLog.RingBuf);

            s_dwTLBPrev = dwTLBCopy;
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SetZones ( 
    DWORD dwZoneUser,
    DWORD dwZoneCE,
    DWORD dwZoneProcess
    )
{
    if (!g_CeLog.Initialized) {
        //
        // we either failed the init, or haven't been initialized yet!
        //
        return;
    }
    
    // Because the profiler data is stored in the interrupt buffer, the
    // interrupt zone and profiler zone cannot be turned on at the same time.
    if ((dwZoneCE & (CELZONE_INTERRUPT | CELZONE_PROFILER)) == (CELZONE_INTERRUPT | CELZONE_PROFILER)) {
        dwZoneCE &= ~CELZONE_INTERRUPT;
    }

    // Flush interrupt buffer if the interrupt zone is changing
    if ((g_CeLog.ZoneCE & CELZONE_INTERRUPT) != (dwZoneCE & CELZONE_INTERRUPT)) {
        // Grab locks, turn off interrupt and profiler logging, flush the buffer,
        // release locks, and then let the zones change.
        g_PubImports.pAcquireCeLogSpinLock ();
        InterlockedIncrement ((volatile LONG*) &g_CeLog.Logging);
        g_CeLog.ZoneCE &= ~(CELZONE_PROFILER | CELZONE_INTERRUPT);
        FlushInterruptBuffer (TRUE);
        g_CeLog.IntBufNeedsHeader = (dwZoneCE & CELZONE_INTERRUPT) ? TRUE : FALSE;
        InterlockedDecrement ((volatile LONG*) &g_CeLog.Logging);
        g_PubImports.pReleaseCeLogSpinLock ();
    }

    g_CeLog.ZoneUser = dwZoneUser;
    g_CeLog.ZoneCE   = VALIDATE_ZONES(dwZoneCE);
}


// Zones which get a lot of data but are logged during KCalls, so they can't call SetEvent
#define ZONE_FREQUENT_KCALL (CELZONE_INTERRUPT | CELZONE_RESCHEDULE | CELZONE_PROFILER)
// Zones which get a lot of data and are not KCalls, so can be relied on to call SetEvent
#define ZONE_FREQUENT       (CELZONE_MIGRATE | CELZONE_SYNCH | CELZONE_HEAP | CELZONE_VIRTMEM)


//------------------------------------------------------------------------------
// Called by CeLogGetZones to retrieve the current zone settings.
//------------------------------------------------------------------------------
BOOL
QueryZones ( 
    LPDWORD lpdwZoneUser,
    LPDWORD lpdwZoneCE,
    LPDWORD lpdwZoneProcess
    )
{
    DWORD ZoneCE;

    // Check whether the library has been initialized.  Use pHeader instead of 
    // g_CeLog.Initialized because CeLogQueryZones is called during
    // IOCTL_CELOG_REGISTER before g_CeLog.Initialized is set.
    if (!g_CeLog.pHeader) {
        return FALSE;
    }
    
    // If only the interrupt and thread switch zones are enabled, then this DLL
    // won't be called during other events.  The problem with that is that this
    // DLL needs to call SetEvent if the buffer is getting full, and cannot call
    // SetEvent during KCalls.  So enable the synchronization zone, to get
    // called relatively frequently with other events.  ZoneCE still won't have
    // CELZONE_SYNCH enabled, so we won't put those events into the log, but
    // it'll give LogData a chance to call SetEvent.  This minimizes data loss.
    ZoneCE = g_CeLog.ZoneCE;
    if (ZoneCE && (ZoneCE & ZONE_FREQUENT_KCALL) && !(ZoneCE & ZONE_FREQUENT)) {
        ZoneCE |= (CELZONE_SYNCH | CELZONE_MIGRATE);  // Should call in often enough
    }

    if (lpdwZoneUser)
        *lpdwZoneUser = g_CeLog.ZoneUser;
    if (lpdwZoneCE)
        *lpdwZoneCE = ZoneCE;
    if (lpdwZoneProcess)
        *lpdwZoneProcess = (DWORD) -1;

    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
LogData ( 
    BOOL  fTimeStamp,
    WORD  wID,
    VOID* pData,
    WORD  wLen,
    DWORD dwZoneUser,
    DWORD dwZoneCE,
    WORD  wFlag,
    BOOL  fFlagged
    )
{
    static LARGE_INTEGER s_liLastLoggedTime;  // Protected by spinlock

    LARGE_INTEGER liCurTime;
    DWORD Header[4];
    DWORD HeaderIndex = 0;
    DWORD cbFullEvent;
    WORD  wFlagID = 0;
    DWORD dwCPU, dwHeaderCPUNum;
    BOOL  IsZoneEnabled;
    DWORD LoopCount;

    if (!g_CeLog.Initialized) {
        //
        // we either failed the init, or haven't been initialized yet!
        //
        return;
    }

    if (g_CeLog.pHeader->IsLocked) {
        // Locked to prevent new data -- skip logging
        return;
    }
    
    // Sanity-check buffer size and pointer
    if ((wLen > MAX_DATA_SIZE) || ((pData == NULL) && (wLen != 0))) {
        return;
    }
    
    DEBUGCHK (wID < CELID_MAX);                // Out of range
    DEBUGCHK (dwZoneCE || dwZoneUser);         // Need to provide some zone.

    // Don't waste time getting a timestamp if the zone is disabled
    IsZoneEnabled = (g_CeLog.ZoneCE & dwZoneCE) || (g_CeLog.ZoneUser & dwZoneUser);
    if (!IsZoneEnabled) {
        fTimeStamp = FALSE;
    }

    // If a previous EnsureFreeSpace call detected that the buffer was getting full,
    // wake the flush thread.  The SetEvent call will generate CeLog events itself.
    // Protect the T->F transition, but the F->T transition is safe without (It's
    // okay to set to TRUE too many times, it just means we'll call SetEvent a little
    // more than necessary.)
    if (InterlockedCompareExchange ((volatile LONG*) &g_CeLog.NeedSetEvent, FALSE, TRUE)) {
        if (InterlockedCompareExchange ((volatile LONG*) &g_CeLog.pHeader->fSetEvent, FALSE, TRUE)) {
            if (!g_PubImports.pEventModify (g_CeLog.hFillEvent, EVENT_SET)) {  // SetEvent
                g_CeLog.pHeader->fSetEvent = TRUE;
                g_CeLog.NeedSetEvent = TRUE;
            }
        } else {
            g_CeLog.NeedSetEvent = TRUE;
        }
    }

    
    // Get the current time and acquire the spinlock.
    // Don't call QPC inside the spinlock, to avoid deadlocks with OAL spinlock.
    // But use the spinlock to ensure that timestamps are always logged in order.
    liCurTime.QuadPart = 0;
    if (fTimeStamp && !g_PubImports.pQueryPerformanceCounterEx (&liCurTime, FALSE)) {
        fTimeStamp = FALSE;
    }
    g_PubImports.pAcquireCeLogSpinLock ();
    
    // Check for out-of-order timestamps, safe in case of rollover.  But put a
    // limit on how many times this loop can spin, in case QueryPerformanceCounter
    // jumps wildly.  Without the loop limit, a big QPC jump forward and then back
    // again could leave all CeLog callers looping until they reach the time QPC
    // previously jumped to.  Instead, break out of the loop and fail to get a
    // timestamp in that case.
    LoopCount = 0;
    while (fTimeStamp && (s_liLastLoggedTime.QuadPart - liCurTime.QuadPart > 0)) {
        // Someone else beat us to the spinlock; leave the spinlock
        // and get the timestamp again to keep the timestamps in order
        g_PubImports.pReleaseCeLogSpinLock ();
        if ((LoopCount >= 10) || !g_PubImports.pQueryPerformanceCounterEx (&liCurTime, FALSE)) {
            // Exceeded the maximum loop count, or QPC failed
            fTimeStamp = FALSE;
        }
        LoopCount++;
        g_PubImports.pAcquireCeLogSpinLock ();
    }
    if (fTimeStamp) {
        s_liLastLoggedTime.QuadPart = liCurTime.QuadPart;
    }
    
    // Prevent the interrupt handler from calling FlushInterruptBuffer
    InterlockedIncrement ((volatile LONG*) &g_CeLog.Logging);


    // Flush on every thread switch or if the buffer's filling
    FlushInterruptBuffer (CELID_THREAD_SWITCH == wID);

    // Check zones after FlushInterruptBuffer so that we flush even when zones are off
    if (!IsZoneEnabled) {
        InterlockedDecrement ((volatile LONG*) &g_CeLog.Logging);
        g_PubImports.pReleaseCeLogSpinLock ();
        return;
    }


    // Header for unflagged data, CPU < 15:
    
    //   <---------------- 1 DWORD ----------------->
    //  |T|R|       ID        |  CPU#  |     Len     |
    //  |                 Timestamp                  | (if T=1)
    //  |                   Data...                  | (etc)

    // Header for flagged data, CPU < 15:
    
    //   <---------------- 1 DWORD ----------------->
    //  |T|R| CELID_FLAGGED   |  CPU#  |     Len     |
    //  |                 Timestamp                  | (if T=1)
    //  |         ID          |         Flag         |
    //  |                   Data...                  | (etc)
        
    // If CPU >= 15, the CPU# will be set to 15 and the real CPU# follows
    // the flag (if flagged) or timestamp (if not flagged).
    
    //   <---------------- 1 DWORD ----------------->
    //  |T|R|       ID        |   15   |     Len     |
    //  |                 Timestamp                  | (if T=1)
    //  |         ID          |         Flag         | (if flagged)
    //  |                   CPU#                     |
    //  |                   Data...                  | (etc)
    
    // Shift timestamp to a lower frequency if necessary
    if (fTimeStamp) {
        liCurTime.QuadPart >>= g_CeLog.dwPerfTimerShift;
    }

    // If the data has a flag, use the ID CELID_FLAGGED and move the real ID
    // inside the data.
    if (fFlagged) {
        wFlagID = wID;
        wID = CELID_FLAGGED;
    }

    dwCPU = g_PubImports.pGetCurrentCPU();
    if (dwCPU >= 15) {
        dwHeaderCPUNum = 15;
    } else {
        dwHeaderCPUNum = dwCPU;
    }
    
    // Build up the header information
    HeaderIndex = 0;
    Header[HeaderIndex++] = (fTimeStamp << 31) | (wID << 16) | (dwHeaderCPUNum << 12) | wLen;
    if (fTimeStamp) {
        Header[HeaderIndex++] = liCurTime.LowPart;
    }
    if (fFlagged) {
        Header[HeaderIndex++] = (DWORD) (wFlagID << 16) | wFlag;
    }
    if (dwCPU >= 15) {
        Header[HeaderIndex++] = dwCPU;
    }
    
    // Write the event into the ring buffer
    cbFullEvent = HeaderIndex * sizeof(DWORD) + wLen;
    cbFullEvent = (cbFullEvent + 3) & ~3;  // DWORD align
    if (EnsureFreeSpace (&g_CeLog.RingBuf, &g_CeLog.pHeader->dwReadOffset, cbFullEvent,
                         g_CeLog.AutoEraseMode, g_CeLog.pHeader->fSetEvent)) {
        WriteData (&g_CeLog.RingBuf, (LPBYTE) Header, HeaderIndex * sizeof(DWORD));
        if (pData && wLen) {
            WriteData (&g_CeLog.RingBuf, pData, wLen);
        }
        CompleteWrite (&g_CeLog.RingBuf);
        
    } else {
        RETAILMSG(ZONE_VERBOSE, (MODNAME TEXT(": ERROR, Secondary Buffer Overrun. Dropping %d bytes (interrupts)\r\n"),
                                 cbFullEvent));
        InterlockedExchangeAdd ((volatile LONG*) &g_CeLog.pHeader->dwLostBytes, cbFullEvent);
    }
    
    // Write a second copy of "sync" events to the sync buffer
    if (g_CeLog.AutoEraseMode && g_CeLog.SyncBuf.dwSize
        && ((wID == CELID_LOG_MARKER) || (wID == CELID_SYNC_END)
            || (wID == CELID_THREAD_CREATE)
            || (wID == CELID_MODULE_LOAD) || (wID == CELID_EXTRA_MODULE_INFO)
            || (wID == CELID_PROCESS_CREATE) || (wID == CELID_EXTRA_PROCESS_INFO))) {
        
        // Build up the header information again, this time without a timestamp or CPU#
        HeaderIndex = 0;
        Header[HeaderIndex++] = (wID << 16) | wLen;
        if (fFlagged) {
            Header[HeaderIndex++] = (DWORD) (wFlagID << 16) | wFlag;
        }

        cbFullEvent = HeaderIndex * sizeof(DWORD) + wLen;
        cbFullEvent = (cbFullEvent + 3) & ~3;  // DWORD align
        if (HasFreeSpace (&g_CeLog.SyncBuf, g_CeLog.SyncBuf_pRead, cbFullEvent)) {
            WriteData (&g_CeLog.SyncBuf, (LPBYTE) Header, HeaderIndex * sizeof(DWORD));
            if (pData && wLen) {
                WriteData (&g_CeLog.SyncBuf, pData, wLen);
            }
            CompleteWrite (&g_CeLog.SyncBuf);
        }
    }
    
    InterlockedDecrement ((volatile LONG*) &g_CeLog.Logging);
    g_PubImports.pReleaseCeLogSpinLock ();
}


//------------------------------------------------------------------------------
// LogInterrupt is called during an ISR while profiling is disabled.
//------------------------------------------------------------------------------
void 
LogInterrupt( 
    DWORD dwLogValue
    )
{
    LPBYTE pWriteCopy;
    
    if (!g_CeLog.Initialized                     // Not ready to log
        || !(g_CeLog.ZoneCE & CELZONE_INTERRUPT) // Interrupt zone is disabled
        || (g_CeLog.pHeader->IsLocked)) {        // Locked to prevent new data
        return;
    }

    // Only read IntBuf.pWrite once
    pWriteCopy = HasFreeSpace (&g_CeLog.IntBuf, g_CeLog.IntBuf_pRead, sizeof(LARGE_INTEGER));

    // If the buffer is too full, but nobody else is actively logging, then flush the buffer
    if (!pWriteCopy
        && (0 == InterlockedCompareExchange ((volatile LONG*) &g_CeLog.Logging, 1, 0))) {
        FlushInterruptBuffer (TRUE);
        InterlockedDecrement ((volatile LONG*) &g_CeLog.Logging);

        pWriteCopy = HasFreeSpace (&g_CeLog.IntBuf, g_CeLog.IntBuf_pRead, sizeof(LARGE_INTEGER));
    }
    
    if (pWriteCopy) {
        LARGE_INTEGER liCurTime;  // Overloaded to use as CEL_INT_DATA!

        // The kernel's QPC wrapper will return FALSE if it's not safe to call the OAL right now.
        liCurTime.QuadPart = 0;
        if (g_PubImports.pQueryPerformanceCounterEx (&liCurTime, TRUE)) {
            liCurTime.QuadPart >>= g_CeLog.dwPerfTimerShift;
        }

        // CEL_INT_DATA has timestamp (LowPart) + interrupt info (HighPart)
        liCurTime.HighPart = dwLogValue;
        
        pWriteCopy = WriteDataWithPointer (&g_CeLog.IntBuf, pWriteCopy,
                                           (LPBYTE) &liCurTime, sizeof(LARGE_INTEGER));
        g_CeLog.IntBuf.pWrite = pWriteCopy;
        CompleteWrite (&g_CeLog.IntBuf);

    } else {
        // Buffer overflow.  It's not safe to call FlushInterruptBuffer here so lose the data.
        InterlockedIncrement ((volatile LONG*) &g_CeLog.dwDiscardedInterrupts);
    }
}


//------------------------------------------------------------------------------
// LogInterruptData is much like LogData, but it is called during ISRs.  So it
// can interrupt LogData calls, assumes only one LogInterruptData call is
// ongoing at once, and is more restricted in what calls it can make.  Right
// now the kernel only calls this entry point to log kernel profiler hits. So
// this implementation is profiling-specific.  Since the interrupt buffer is a
// convenient side buffer for recording data into, we turn off interrupt logging
// and write the profiler data there.  Thus profiler logging and interrupt
// logging cannot both be enabled at the same time.
//------------------------------------------------------------------------------
void 
LogInterruptData ( 
    BOOL  fTimeStamp,
    WORD  wID,
    VOID* pData,
    WORD  wLen
    )
{
    LARGE_INTEGER liCurTime;
    DWORD Header[2];
    DWORD HeaderIndex = 0;
    DWORD cbFullEvent;
    LPBYTE pWriteCopy;

    if (!g_CeLog.Initialized                     // Not ready to log
        || (g_CeLog.ZoneCE & CELZONE_INTERRUPT)  // Interrupt zone is enabled - don't confuse profiling data with interrupt data
        || !(g_CeLog.ZoneCE & CELZONE_PROFILER)  // Profiler zone is disabled
        || (g_CeLog.pHeader->IsLocked)) {        // Locked to prevent new data
        return;
    }

    // The kernel's QPC wrapper will return FALSE if it's not safe to call the OAL right now.
    liCurTime.QuadPart = 0;
    if (fTimeStamp && g_PubImports.pQueryPerformanceCounterEx (&liCurTime, TRUE)) {
        liCurTime.QuadPart >>= g_CeLog.dwPerfTimerShift;
    } else {
        fTimeStamp = FALSE;
    }

    // Build up the header information; no CPU#
    HeaderIndex = 0;
    Header[HeaderIndex++] = (fTimeStamp << 31) | (wID << 16) | wLen;
    if (fTimeStamp) {
        Header[HeaderIndex++] = liCurTime.LowPart;
    }

    // Only read IntBuf.pWrite once
    cbFullEvent = HeaderIndex * sizeof(DWORD) + wLen;
    cbFullEvent = (cbFullEvent + 3) & ~3;  // DWORD align
    pWriteCopy = HasFreeSpace (&g_CeLog.IntBuf, g_CeLog.IntBuf_pRead, cbFullEvent);
    
    // If the buffer is too full, but nobody else is actively logging, then flush the buffer
    if (!pWriteCopy
        && (0 == InterlockedCompareExchange ((volatile LONG*) &g_CeLog.Logging, 1, 0))) {
        FlushInterruptBuffer (TRUE);
        InterlockedDecrement ((volatile LONG*) &g_CeLog.Logging);

        pWriteCopy = HasFreeSpace (&g_CeLog.IntBuf, g_CeLog.IntBuf_pRead, sizeof(LARGE_INTEGER));
    }
    
    // Write the event into the interrupt buffer
    if (pWriteCopy) {
        pWriteCopy = WriteDataWithPointer (&g_CeLog.IntBuf, pWriteCopy,
                                           (LPBYTE) Header, HeaderIndex * sizeof(DWORD));
        if (pData && wLen) {
            pWriteCopy = WriteDataWithPointer (&g_CeLog.IntBuf, pWriteCopy,
                                               pData, wLen);
        }
        g_CeLog.IntBuf.pWrite = pWriteCopy;
        CompleteWrite (&g_CeLog.IntBuf);

    } else {
        // Buffer overflow.  It's not safe to call FlushInterruptBuffer here so lose the data.
        InterlockedExchangeAdd ((volatile LONG*) &g_CeLog.pHeader->dwLostBytes, cbFullEvent);
    }
}


//------------------------------------------------------------------------------
// Helper function to read a DWORD value out of the device registry.
// Returns TRUE if it successfully read a value, FALSE otherwise.
//------------------------------------------------------------------------------
static BOOL
GetRegDWORD (
    LPCWSTR pszKeyPath,
    LPCWSTR pszValueName,
    DWORD*  pDestValue      // Not modified unless the registry value is valid
    )
{
    DWORD type, value, cbValue;
    LONG lResult;

    // No IsAPIReady check because NKRegQueryValueExW will safely fail if no registry
    cbValue = sizeof(DWORD);
    lResult = g_PubImports.pRegQueryValueExW (HKEY_LOCAL_MACHINE, pszValueName,
                                              (LPDWORD) pszKeyPath, &type,
                                              (LPBYTE) &value, &cbValue);
    if ((ERROR_SUCCESS == lResult) && (REG_DWORD == type)
        && (sizeof(DWORD) == cbValue)) {
        *pDestValue = value;
        return TRUE;
    }

    return FALSE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void
GetBufferParams (
    DWORD* pMapAddr,
    DWORD* pMapSize,
    DWORD* pSyncBufSize,
    BOOL*  pClearExistingData, // Says whether to wipe map contents if the map preexisted
    BOOL*  pAutoEraseMode      // Says whether to continuously discard old data to make room for new
    )
{
    // Built-in defaults
    *pMapAddr = MAP_ADDR;
    *pMapSize = MAP_SIZE;
    *pSyncBufSize = 0;
    *pClearExistingData = FALSE;
    *pAutoEraseMode = FALSE;

    // Override with what came from the import table
    if (g_PubImports.dwCeLogLargeBuf) {
        *pMapSize = g_PubImports.dwCeLogLargeBuf;
    }
    
//#if (CE_MAJOR_VER >= 0x6)
    // Then query the OAL
    {
        OEMCeLogParameters OEMParams;

        // Pre-populate the OEMParams struct with current settings, so the OEM
        // doesn't have to specify all the fields.
        OEMParams.dwVersion = 1;
        OEMParams.MainBufferAddress = *pMapAddr;
        OEMParams.MainBufferSize = *pMapSize;
        OEMParams.SyncDataSize = *pSyncBufSize;
        OEMParams.ClearExistingData = *pClearExistingData;
        OEMParams.AutoEraseMode = *pAutoEraseMode;
        OEMParams.ZoneCE = g_CeLog.ZoneCE;

        if (g_pfnKernelLibIoControl ((HANDLE)KMOD_OAL, IOCTL_HAL_GET_CELOG_PARAMETERS,
                                     NULL, 0, &OEMParams, sizeof(OEMCeLogParameters), NULL)
            && (OEMParams.dwVersion >= 1)) {
            g_CeLog.ZoneCE = VALIDATE_ZONES(OEMParams.ZoneCE);
            *pMapAddr = OEMParams.MainBufferAddress;
            *pMapSize = OEMParams.MainBufferSize;
            *pSyncBufSize = OEMParams.SyncDataSize;
            *pClearExistingData = OEMParams.ClearExistingData;
            *pAutoEraseMode = OEMParams.AutoEraseMode;
        }
    }
//#endif // CE_MAJOR_VER

    // Look for the import parameters in the local registry if the registry is
    // available.  This won't be possible if CeLog is loaded on boot 
    // (IMGCELOGENABLE=1) but will be if it's loaded later.
    
    // Require that pMapAddr comes from a secure area of the registry (HKLM\System),
    // to prevent attackers from getting CeLog to use kernel memory.

    // First read from HKLM\System\CeLog
    GetRegDWORD (REGKEY_CELOG, REGVAL_BUFFER_ADDR, pMapAddr);
    GetRegDWORD (REGKEY_CELOG, REGVAL_BUFFER_SIZE, pMapSize);
    GetRegDWORD (REGKEY_CELOG, REGVAL_SYNC_BUFFER_SIZE, pSyncBufSize);
    GetRegDWORD (REGKEY_CELOG, REGVAL_AUTO_ERASE_MODE, (DWORD*) pAutoEraseMode);

    // Then read from the volatile regkey HKLM\System\CeLog\PassedSettings, which
    // the flush apps write to in order to pass command-line parameters to celog.dll
    GetRegDWORD (REGKEY_CELOG_PASSED_SETTINGS, REGVAL_BUFFER_ADDR, pMapAddr);
    GetRegDWORD (REGKEY_CELOG_PASSED_SETTINGS, REGVAL_BUFFER_SIZE, pMapSize);
    GetRegDWORD (REGKEY_CELOG_PASSED_SETTINGS, REGVAL_SYNC_BUFFER_SIZE, pSyncBufSize);
    GetRegDWORD (REGKEY_CELOG_PASSED_SETTINGS, REGVAL_AUTO_ERASE_MODE, (DWORD*) pAutoEraseMode);


    //
    // Check the values of the parameters before using them.
    //
    
    if (*pMapAddr && (*pMapAddr < VM_KMODE_BASE)) {  // Require that it's a kernel mode address
        DEBUGMSG(1, (MODNAME TEXT(": Invalid user-mode map address, ignoring.\r\n")));
        *pMapAddr = MAP_ADDR;
    }
    if (*pMapSize & (VM_PAGE_SIZE-1)) {
        DEBUGMSG(1, (MODNAME TEXT(": Large buffer size not page-aligned, rounding down.\r\n")));
        *pMapSize &= ~(VM_PAGE_SIZE-1);
    }
    if (*pMapSize < HEADER_SIZE + INTBUF_SIZE) {
        DEBUGMSG(1, (MODNAME TEXT(": Large buffer too small, using default size\r\n")));
        *pMapSize = MAP_SIZE;
    }
    // Different map size limits for static buffer vs. regular memory-mapped file
    if (0 == *pMapAddr) {
        if (*pMapSize >= (DWORD) (UserKInfo[KINX_PAGEFREE] * VM_PAGE_SIZE)) {
            // Try 3/4 of available RAM
            *pMapSize = ((UserKInfo[KINX_PAGEFREE] * VM_PAGE_SIZE * 3) / 4) & ~(VM_PAGE_SIZE-1);
            DEBUGMSG(1, (MODNAME TEXT(": Only 0x%08X RAM available, using 0x%08x for large buffer.\r\n"), 
                         (UserKInfo[KINX_PAGEFREE] * VM_PAGE_SIZE), *pMapSize));
        }
    } else {
        // Sanity check; impose a limit to avoid wrap
        DWORD dwMax = ((DWORD) -1 - (DWORD) *pMapAddr) & ~(VM_PAGE_SIZE-1);
        if ((*pMapSize > dwMax) || (*pMapSize > MAX_MAP_SIZE)) {
            DEBUGMSG(1, (MODNAME TEXT(": Invalid static buffer size, rejecting static buffer.\r\n")));
            *pMapAddr = MAP_ADDR;
            *pMapSize = MAP_SIZE;
        }
    }

    // Only using a sync buffer in auto-erase mode for now
    if (!*pAutoEraseMode) {
        *pSyncBufSize = 0;
    } else if (!*pSyncBufSize) {
        // If the user says to use AutoErase mode, but doesn't set a sync buffer size,
        // then choose a default
        *pSyncBufSize = SYNCBUF_SIZE;
    }
    if (*pSyncBufSize >= *pMapSize - HEADER_SIZE - INTBUF_SIZE) {
        DEBUGMSG(1, (MODNAME TEXT(": Sync buffer too large, using half of large buffer.\r\n")));
        *pSyncBufSize = (*pMapSize - HEADER_SIZE - INTBUF_SIZE) / 2;
    }
    *pSyncBufSize = INT_ALIGN_DOWN(*pSyncBufSize);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL
CreateMap (DWORD* pMapSize)
{
    do {
        // CreateFileMapping will succeed as long as there's enough VM, but
        // LockPages will only succeed if there is enough physical memory.
        g_CeLog.hMap = (HANDLE) g_PubImports.pCreateFileMappingW (
                                            INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                            0, *pMapSize, DATAMAP_NAME);
        DEBUGMSG (!g_CeLog.hMap,
                  (MODNAME TEXT(": CreateFileMapping size=%u failed (%u)\r\n"),
                   *pMapSize, g_PubImports.pGetLastError ()));
        if (g_CeLog.hMap) {
            // Require that the map did not already exist, to prevent name hijacking
            if (ERROR_ALREADY_EXISTS == g_PubImports.pGetLastError ()) {
                RETAILMSG(1, (MODNAME TEXT(": Map already exists, failing\r\n")));
                g_PubImports.pCloseHandle (g_CeLog.hMap);
                g_CeLog.hMap = 0;
                return FALSE;
            }

            g_CeLog.pHeader = (PMAPHEADER) g_PubImports.pMapViewOfFile (
                                                g_CeLog.hMap, FILE_MAP_ALL_ACCESS,
                                                0, 0, *pMapSize);
            DEBUGMSG (!g_CeLog.pHeader,
                      (MODNAME TEXT(": MapViewOfFile size=%u failed (%u)\r\n"),
                       *pMapSize, g_PubImports.pGetLastError ()));
            if (g_CeLog.pHeader) {
                // We can't take page faults during the logging so lock the pages.
                if (g_PubImports.pLockPages (g_CeLog.pHeader, *pMapSize,
                                             NULL, LOCKFLAG_WRITE)) {
                    // Success!
                    DEBUGMSG (1, (MODNAME TEXT(": Large buffer 0x%08X-0x%08X\r\n"),
                                  g_CeLog.pHeader, (DWORD) g_CeLog.pHeader + *pMapSize));
                    return TRUE;
                }
                
                // Probably not enough physical memory to commit the whole map
                DEBUGMSG(1, (MODNAME TEXT(": LockPages ptr=0x%08x size=%u failed (%u)\r\n"),
                             g_CeLog.pHeader, *pMapSize, g_PubImports.pGetLastError ()));
                
                g_PubImports.pUnmapViewOfFile (g_CeLog.pHeader);
                g_CeLog.pHeader = NULL;
            }
            g_PubImports.pCloseHandle (g_CeLog.hMap);
            g_CeLog.hMap = 0;
        }
        
        // Keep trying smaller buffer sizes until we succeed
        *pMapSize /= 2;
        if (*pMapSize < VM_PAGE_SIZE) {
            RETAILMSG (1, (MODNAME TEXT(": Large Buffer alloc failed\r\n")));
            return FALSE;
        }
    
    } while (*pMapSize >= VM_PAGE_SIZE);

    return FALSE;
}


// The map contains the header, then the sync buffer, then the
// interrupt buffer, then the ring buffer:
//
// |        |                 |      |                               |
// | header |    sync data    | ints |  normal CeLog data (ringbuf)  |
// |        |                 |      |                               |
// |HEADR_SZ|<--SyncBufSize-->|INT_SZ|<---------RingBufSize--------->|
// |<--------------------------- MapSize --------------------------->|
//
// The header is always present.  The ring buffer and interrupt buffer
// will be omitted together, if SyncBufSize is the entire map.  The
// sync data will be omitted if SyncBufSize is 0.
//
// Since IntBuf requires 8-byte alignment, all of the buffer sizes are
// 8-byte aligned.  This simplifies size and offset calculations.


//------------------------------------------------------------------------------
// Returns TRUE if the static buffer is valid.
//------------------------------------------------------------------------------
static BOOL
ValidateStaticBuffer (
    DWORD  MapSize,
    DWORD* pRingBufSize,
    DWORD* pWriteOffset,
    DWORD* pSyncBufSize,
    DWORD* pIntWriteOffset
    )
{
    DWORD RingBufStart, RingBufSize;
    DWORD SyncBufStart, SyncBufSize;
    DWORD IntBufStart;

    // Make copies of the header data, to prevent async modification
    RingBufStart = g_CeLog.pHeader->dwBufferStart;
    RingBufSize = g_CeLog.pHeader->dwBufSize;
    *pWriteOffset = g_CeLog.pHeader->dwWriteOffset;
    SyncBufStart = g_CeLog.pHeader->dwSyncBufferStart;
    SyncBufSize = g_CeLog.pHeader->dwSyncBufferSize;
    IntBufStart = g_CeLog.pHeader->dwIntBufferStart;
    *pIntWriteOffset = g_CeLog.pHeader->dwIntWriteOffset;

    if ((g_CeLog.pHeader->dwVersion < 3)
        || (g_CeLog.pHeader->Signature != CELOG_DATAMAP_SIGNATURE)) {
        // No signature -- the buffer didn't persist
        return FALSE;
    }

    // There are always a ring buffer and an interrupt buffer.
    // There may not be a sync buffer.

    // Validate ring buffer size & write position
    if ((RingBufStart < HEADER_SIZE + INTBUF_SIZE)
        || (RingBufStart >= MapSize)
        || (RingBufSize < MAX_DATA_SIZE)  // Not really required but this is pretty small...
        || (RingBufSize != MapSize - RingBufStart)
        || (*pWriteOffset >= RingBufSize)) {
        return FALSE;
    }
    
    // Validate int buffer size & write position
    if ((IntBufStart != RingBufStart - INTBUF_SIZE)
        || (g_CeLog.pHeader->dwIntBufferSize != INTBUF_SIZE)
        || (*pIntWriteOffset >= INTBUF_SIZE)) {
        return FALSE;
    }

    // Validate sync buffer size & write position
    if (!SyncBufSize) {
        // No sync buffer
        if (SyncBufStart) {
            return FALSE;
        }
    } else {
        // Sync buffer exists
        if ((SyncBufStart != HEADER_SIZE)
            || (SyncBufSize > RingBufStart - INTBUF_SIZE - HEADER_SIZE)) {
            return FALSE;
        }
    }

    *pRingBufSize = RingBufSize;
    *pSyncBufSize = SyncBufSize;
    
    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void
CleanupBuffers ()
{
    DWORD MapSize = 0;
    
    if (g_CeLog.RingBuf.dwSize) {
        MapSize = g_CeLog.RingBuf.pBuffer + g_CeLog.RingBuf.dwSize - (LPBYTE) g_CeLog.pHeader;
    } else if (g_CeLog.SyncBuf.dwSize) {
        MapSize = g_CeLog.SyncBuf.pBuffer + g_CeLog.SyncBuf.dwSize - (LPBYTE) g_CeLog.pHeader;
    }
    
    if (g_CeLog.hFillEvent) {
        g_PubImports.pCloseHandle (g_CeLog.hFillEvent);
        g_CeLog.hFillEvent = NULL;
    }
    if (g_CeLog.pHeader && MapSize) {
        g_PubImports.pUnlockPages (g_CeLog.pHeader, MapSize);
    }
    if (g_CeLog.hMap) {
        if (g_CeLog.pHeader) {
            g_PubImports.pUnmapViewOfFile (g_CeLog.pHeader);
        }
        g_PubImports.pCloseHandle (g_CeLog.hMap);
        g_CeLog.hMap = 0;
    }
    g_CeLog.pHeader = NULL;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL
InitBuffers ()
{
    DWORD StaticAddr, MapSize, SyncBufSize, RingBufSize;
    DWORD WriteOffset, IntWriteOffset;
    BOOL  ClearExistingData;

    DEBUGMSG (ZONE_VERBOSE, (MODNAME TEXT(": +InitBuffers\r\n")));
    
    GetBufferParams (&StaticAddr, &MapSize, &SyncBufSize,
                     &ClearExistingData, &g_CeLog.AutoEraseMode);
        
    
    // Use an uncached address to write to the static buffer
    if (StaticAddr) {
        StaticAddr = (DWORD) g_PubImports.pVirtualAllocCopyInKernel (
                                (LPVOID) StaticAddr, MapSize, PAGE_READWRITE | PAGE_NOCACHE);
        RETAILMSG (!StaticAddr, (MODNAME TEXT(": VirtualAllocCopy failed to get uncached address for static buffer\r\n")));
    }

    // Allocate the main memory buffer
    if (StaticAddr) {
        // Unfortunately, there is no way to expose the static buffer as a named
        // memory-mapped file the way CeLog has traditionally done.  The flushing
        // can only be done by a kernel-mode DLL that knows what address the
        // buffer is stored at.
        g_CeLog.hMap = 0;
        g_CeLog.pHeader = (PMAPHEADER) StaticAddr;

    } else if (!CreateMap (&MapSize)) {
        goto error;
    }
    if (MapSize <= HEADER_SIZE + INTBUF_SIZE) {  // Safety check
        goto error;
    }

    
    // Check if there is a valid signature on the buffer already.
    RETAILMSG(StaticAddr, (MODNAME TEXT(": Static buffer Addr = 0x%08X, Size = 0x%08X\r\n"),
                           StaticAddr, MapSize));
    if (StaticAddr && !ClearExistingData
        && ValidateStaticBuffer (MapSize, &RingBufSize, &WriteOffset,
                                 &SyncBufSize, &IntWriteOffset)) {
        // The RAM buffer preexisted and has valid contents.
        RETAILMSG (1, (MODNAME TEXT(": Persisted static buffer from previous boot!\r\n")));

        // Lock buffer for later dump
        if (g_CeLog.AutoEraseMode) {
            g_CeLog.pHeader->IsLocked = TRUE;
        }
    
    } else {
        RETAILMSG (StaticAddr, (MODNAME TEXT(": Clean static buffer\r\n")));
        
        RingBufSize = MapSize - HEADER_SIZE - INTBUF_SIZE;
        RingBufSize -= SyncBufSize;
        RingBufSize = INT_ALIGN_DOWN(RingBufSize);

        WriteOffset = 0;                            // Only read once: on non-clean boot
        IntWriteOffset = 0;                         // Only read once: on non-clean boot

        // A few header values should only be written when the buffer is clean.
        g_CeLog.pHeader->dwLostBytes       = 0;     // Write-only
        g_CeLog.pHeader->IsLocked          = FALSE;
        g_CeLog.pHeader->dwReadOffset      = 0;
        g_CeLog.pHeader->dwIntReadOffset   = 0;     // Write-only
        g_CeLog.pHeader->dwSyncWriteOffset = 0;     // Write-only
        g_CeLog.pHeader->dwSyncReadOffset  = 0;     // Write-only, never changes
    }
        
    // Prepare our local version of the buffer state
    g_CeLog.RingBuf.pBuffer        = (LPBYTE) g_CeLog.pHeader + MapSize - RingBufSize;
    g_CeLog.RingBuf.dwSize         = RingBufSize;
    g_CeLog.RingBuf.pWrite         = g_CeLog.RingBuf.pBuffer + WriteOffset;
    g_CeLog.RingBuf.pdwWriteOffset = &g_CeLog.pHeader->dwWriteOffset;
    g_CeLog.IntBuf.pBuffer         = g_CeLog.RingBuf.pBuffer - INTBUF_SIZE;
    g_CeLog.IntBuf.dwSize          = INTBUF_SIZE;
    g_CeLog.IntBuf.pWrite          = g_CeLog.IntBuf.pBuffer + IntWriteOffset;
    g_CeLog.IntBuf.pdwWriteOffset  = &g_CeLog.pHeader->dwIntWriteOffset;
    g_CeLog.IntBuf_pRead           = g_CeLog.IntBuf.pWrite;    // Empty buffer
    g_CeLog.SyncBuf.pBuffer        = SyncBufSize ? ((LPBYTE) g_CeLog.pHeader + HEADER_SIZE) : NULL;
    g_CeLog.SyncBuf.dwSize         = SyncBufSize;
    g_CeLog.SyncBuf.pWrite         = g_CeLog.SyncBuf.pBuffer;  // Empty buffer
    g_CeLog.SyncBuf.pdwWriteOffset = &g_CeLog.pHeader->dwSyncWriteOffset;
    g_CeLog.SyncBuf_pRead          = g_CeLog.SyncBuf.pBuffer;
    DEBUGCHK (!SyncBufSize || g_CeLog.AutoEraseMode);   // Sync buf only supported in auto-erase case right now

    // Initialize the header on the map
    g_CeLog.pHeader->dwVersion         = 3;
    g_CeLog.pHeader->Signature         = CELOG_DATAMAP_SIGNATURE;
    g_CeLog.pHeader->fSetEvent         = TRUE;
    g_CeLog.pHeader->dwBufferStart     = g_CeLog.RingBuf.pBuffer - (LPBYTE) g_CeLog.pHeader;
    g_CeLog.pHeader->dwBufSize         = RingBufSize;
    g_CeLog.pHeader->pWrite            = NULL;  // obsolete
    g_CeLog.pHeader->pRead             = NULL;  // obsolete
    g_CeLog.pHeader->dwWriteOffset     = WriteOffset;
    g_CeLog.pHeader->dwIntBufferStart  = g_CeLog.IntBuf.pBuffer - (LPBYTE) g_CeLog.pHeader;
    g_CeLog.pHeader->dwIntBufferSize   = INTBUF_SIZE;
    g_CeLog.pHeader->dwIntWriteOffset  = IntWriteOffset;
    g_CeLog.pHeader->dwSyncBufferStart = g_CeLog.SyncBuf.pBuffer ? (g_CeLog.SyncBuf.pBuffer - (LPBYTE) g_CeLog.pHeader) : 0;
    g_CeLog.pHeader->dwSyncBufferSize  = SyncBufSize;


    DEBUGMSG ((SyncBufSize != 0),
              (MODNAME TEXT(": Sync buffer size = %u\r\n"), SyncBufSize));
    DEBUGMSG ((MapSize != MAP_SIZE),
              (MODNAME TEXT(": Large buffer size = %u\r\n"), MapSize));
    
    
    // Create the event to flag when the buffer is getting full
    // (Must be auto-reset so we can call SetEvent during a flush!)
    g_CeLog.hFillEvent = (HANDLE)g_PubImports.pCreateEventW (NULL, FALSE, FALSE, FILLEVENT_NAME);
    if (g_CeLog.hFillEvent == NULL) {
        DEBUGMSG (1, (MODNAME TEXT(": Fill event creation failed\r\n")));
        goto error;
    }
    
    DEBUGMSG (ZONE_VERBOSE, (MODNAME TEXT(": -InitBuffers\r\n")));
    
    return TRUE;

error:
    CleanupBuffers ();
    return FALSE;
}


//------------------------------------------------------------------------------
// Check preset zones
//------------------------------------------------------------------------------
void static
InitZones ()
{
    BOOL  fOk;
    DWORD dwVal;
    
    // We start with built-in zones (CELOG_*_MASK), overwritten by
    // the OEM's IOCTL_HAL_GET_CELOG_PARAMETERS

    // Check the desktop PC's registry
    fOk = g_pfnKernelLibIoControl((HANDLE)KMOD_CELOG, IOCTL_CELOG_GETDESKTOPZONE,
                            TEXT("CeLogZoneUser"), 13*sizeof(WCHAR),
                            &(dwVal), sizeof(DWORD), NULL);
    if (fOk && (CELOG_USER_MASK != dwVal) && (0xFFBFFFFF != dwVal))
    {
        //Only take the value if it isn't the default value. The default 
        //CELOG_USER_MASK is defined in celognk.h. The hard coded value of
        //0xFFBFFFFF is the default value returned from PB. This value is hard
        //coded in tools\platman\source\kitl\kitldll\kitlprot.cpp.
        g_CeLog.ZoneUser = dwVal;
    }

    fOk = g_pfnKernelLibIoControl((HANDLE)KMOD_CELOG, IOCTL_CELOG_GETDESKTOPZONE,
                            TEXT("CeLogZoneCE"), 11*sizeof(WCHAR),
                            &(dwVal), sizeof(DWORD), NULL);
    if (fOk && (CELOG_CE_MASK != dwVal) && (0xFFBFFFFF != dwVal))
    {
        //Only take the value if it isn't the default value. The default 
        //CELOG_USER_MASK is defined in celognk.h. The hard coded value of
        //0xFFBFFFFF is the default value returned from PB. This value is hard
        //coded in tools\platman\source\kitl\kitldll\kitlprot.cpp.
        g_CeLog.ZoneCE = dwVal;
    }
    
    // Check the device-side registry, allowing the PassedSettings key to override the main key
    GetRegDWORD (REGKEY_CELOG, REGVAL_ZONE_CE, &g_CeLog.ZoneCE);
    GetRegDWORD (REGKEY_CELOG_PASSED_SETTINGS, REGVAL_ZONE_CE, &g_CeLog.ZoneCE);

    // Turn off any zones the user's not allowed to turn on
    g_CeLog.ZoneCE = VALIDATE_ZONES(g_CeLog.ZoneCE);

    // IntBufNeedsHeader is true whenever interrupts are enabled, and false whenever they're disabled
    g_CeLog.IntBufNeedsHeader = (g_CeLog.ZoneCE & CELZONE_INTERRUPT) ? TRUE : FALSE;

    RETAILMSG(1, (TEXT("CeLog Zones : CE = 0x%08X, User = 0x%08X, Proc = 0x%08X\r\n"),
                  g_CeLog.ZoneCE, g_CeLog.ZoneUser, (DWORD)-1));
}


//------------------------------------------------------------------------------
// Hook up function pointers and initialize logging
//------------------------------------------------------------------------------
BOOL static
InitLibrary ()
{
    CeLogExportTable exports;
    LARGE_INTEGER liPerfFreq;


    // Start with built-in zone defaults
    g_CeLog.ZoneUser = CELOG_USER_MASK;
    g_CeLog.ZoneCE   = CELOG_CE_MASK;


    //
    // KernelLibIoControl provides the back doors we need to obtain kernel
    // function pointers and register logging functions.
    //
    
    // Get public imports from kernel
    g_PubImports.dwVersion = CELOG_IMPORT_VERSION;
    if (!g_pfnKernelLibIoControl ((HANDLE)KMOD_CELOG, IOCTL_CELOG_IMPORT, &g_PubImports,
                                  sizeof(CeLogImportTable), NULL, 0, NULL)) {
        // Can't DEBUGMSG or anything here b/c we need the import table to do that
        return FALSE;
    }

    // CeLog requires MapViewOfFile
    if (g_PubImports.pMapViewOfFile == NULL) {
        DEBUGMSG(1, (MODNAME TEXT(": Error, cannot run because this kernel does not support memory-mapped files\r\n")));
        g_PubImports.pSetLastError (ERROR_NOT_SUPPORTED);
        return FALSE;
    }
    
    // Not using any private imports from kernel; skip IOCTL_CELOG_IMPORT_PRIVATE

    // Now initialize logging
    if (!InitBuffers ()) {
        return FALSE;
    }

    // Get the high-performance timer's frequency
    g_PubImports.pQueryPerformanceFrequency (&liPerfFreq);

    // Scale the frequency down so that the 32 bits of the high-performance
    // timer we associate with log events doesn't wrap too quickly.
    g_CeLog.dwPerfTimerShift = 0;
    while (liPerfFreq.QuadPart > MAX_ROLLOVER_COUNT) {
        g_CeLog.dwPerfTimerShift++;
        liPerfFreq.QuadPart >>= 1;
    }

    InitZones ();
    
    // Register logging functions with the kernel
    exports.dwVersion             = 3;
    exports.pfnCeLogData          = LogData;
    exports.pfnCeLogInterrupt     = LogInterrupt;
    exports.pfnCeLogInterruptData = LogInterruptData;
    exports.pfnCeLogSetZones      = SetZones;
    exports.pfnCeLogQueryZones    = QueryZones;
    exports.dwCeLogTimerFrequency = liPerfFreq.LowPart;
    if (!g_pfnKernelLibIoControl ((HANDLE)KMOD_CELOG, IOCTL_CELOG_REGISTER, &exports,
                                  sizeof(CeLogExportTable), NULL, 0, NULL)) {
        DEBUGMSG(1, (MODNAME TEXT(": Unable to register logging functions with kernel\r\n")));
        CleanupBuffers();
        return FALSE;
    }

    g_CeLog.Initialized = TRUE;
    
    // Now that logging functions are registered, request a resync
    DEBUGMSG(ZONE_VERBOSE, (TEXT("CeLog resync\r\n")));
    g_PubImports.pCeLogReSync ();
    
    return TRUE;
}


//------------------------------------------------------------------------------
// Shut down logging and unhook function pointers.  Note that this is not really
// threadsafe because other threads could be in the middle of a logging call --
// there is no way to force them out.
//------------------------------------------------------------------------------
BOOL static
CleanupLibrary ()
{
    CeLogExportTable exports;

    g_CeLog.Initialized = FALSE;
    
    // Wake any flush thread that is listening
    if (g_CeLog.pHeader->fSetEvent) {
        g_PubImports.pEventModify (g_CeLog.hFillEvent, EVENT_SET);  // SetEvent
    }

    CleanupBuffers ();

    // Deregister logging functions with the kernel
    exports.dwVersion             = 3;
    exports.pfnCeLogData          = LogData;
    exports.pfnCeLogInterrupt     = LogInterrupt;
    exports.pfnCeLogInterruptData = LogInterruptData;
    exports.pfnCeLogSetZones      = SetZones;
    exports.pfnCeLogQueryZones    = QueryZones;
    exports.dwCeLogTimerFrequency = 0;
    if (!g_pfnKernelLibIoControl ((HANDLE)KMOD_CELOG, IOCTL_CELOG_DEREGISTER, &exports,
                                  sizeof(CeLogExportTable), NULL, 0, NULL)) {
        DEBUGMSG(1, (MODNAME TEXT(": Unable to deregister logging functions with kernel\r\n")));
        return FALSE;
    }
    
    return TRUE;
}


//------------------------------------------------------------------------------
// DLL entry
//------------------------------------------------------------------------------
BOOL WINAPI
KernelDLLEntry(HINSTANCE DllInstance, INT Reason, LPVOID Reserved)
{
    switch (Reason) {
    case DLL_PROCESS_ATTACH:
        if (!g_CeLog.Initialized) {
            InitInterlockedFunctions ();
            if (Reserved) {
                // Reserved parameter is a pointer to KernelLibIoControl function
#pragma warning(disable:4055) // typecast from data pointer LPVOID to function pointer FARPROC
                g_pfnKernelLibIoControl = (FARPROC) Reserved;
#pragma warning(default:4055)
                return InitLibrary ();
            }
            // Loaded via LoadLibrary instead of LoadKernelLibrary?
#ifndef CELOG_UNIT_TEST
            return FALSE;
#endif
        }
        break;

    case DLL_PROCESS_DETACH:
        if (g_CeLog.Initialized) {
            return CleanupLibrary ();
        }
        break;
    }
    
    return TRUE;
}
