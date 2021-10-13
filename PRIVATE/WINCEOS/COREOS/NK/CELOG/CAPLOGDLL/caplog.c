//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//  
//------------------------------------------------------------------------------
//
//  Module Name:  
//  
//      caplog.c
//  
//  Abstract:  
//
//      Implements the CECAP event logging functions.
//      
//------------------------------------------------------------------------------
#include <kernel.h>
#ifndef _PREFAST_
#pragma warning(disable: 4068) // Disable pragma warnings
#endif

#define MODNAME TEXT("CAPLog")
#ifdef DEBUG
DBGPARAM dpCurSettings = { MODNAME, {
        TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"),
        TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"),
        TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"),
        TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>") },
    0x00000000};
#endif

#define ZONE_VERBOSE 0


//------------------------------------------------------------------------------
// BUFFERS

typedef struct  __CEL_BUFFER {
    DWORD   dwMaskProcess;              // Process mask (1 bit per process slot)
    DWORD   dwMaskUser;                 // User zone mask
    DWORD   dwMaskCE;                   // Predefined zone mask (CELZONE_xxx)
    PDWORD  pWrite;                     // Pointer to next entry to be written
    DWORD   dwBytesLeft;                // Bytes left available in the buffer
    DWORD   dwSize;                     // Total number of bytes in the buffer
    PDWORD  pBuffer;                    // Start of the data
} CEL_BUFFER, *PCEL_BUFFER;

typedef struct _RINGBUFFER {
    HANDLE hMap;        // Used for RingBuf memory mapping
    PMAPHEADER pHeader; // Pointer to RingBuf map header
    LPBYTE pBuffer;
    LPBYTE pRead;
    LPBYTE pWrite;
    LPBYTE pWrap;
    DWORD  dwSize;
    DWORD  dwBytesLeft; // Calculated during flush for RingBuf
} RINGBUFFER, *PRINGBUFFER;

RINGBUFFER RingBuf;
PCEL_BUFFER pCelBuf;

CEL_BUFFER CelBuf;

//------------------------------------------------------------------------------
// EXPORTED GLOBAL VARIABLES

#define SMALLBUF_MAX         4096*1024  // Maximum size an OEM can boost the small buffer to
#define PAGES_IN_MAX_BUFFER  (SMALLBUF_MAX/PAGE_SIZE)
#define RINGBUF_SIZE         128*1024   // Default size of large buffer
#define SMALLBUF_SIZE        4*1024     // Default size of small buffer

#define CAPLOG_ZONES         (CELZONE_PROCESS | CELZONE_THREAD | CELZONE_RESCHEDULE | CELZONE_LOADER)


//------------------------------------------------------------------------------
// MISC

#define MAX_ROLLOVER_MINUTES    30
#define MAX_ROLLOVER_COUNT      (0xFFFFFFFF / (MAX_ROLLOVER_MINUTES * 60))

HANDLE g_hFillEvent;  // Used to indicate the secondary buffer is getting full
BOOL   g_fInit;
DWORD  g_dwPerfTimerShift;

// Inputs from kernel
CeLogImportTable    g_PubImports;
CeLogPrivateImports g_PrivImports;


#define DATAMAP_NAME      CELOG_DATAMAP_NAME TEXT(" CAP")
#define FILLEVENT_NAME    CELOG_FILLEVENT_NAME TEXT(" CAP")


//------------------------------------------------------------------------------
// Abstract accesses to imports

#define PUB_FUNC(Api, Args)     ((g_PubImports.p##Api) Args)
#define PUB_VAR(Var)            (g_PubImports.Var)
#define PRIV_FUNC(Api, Args)    ((g_PrivImports.p##Api) Args)
#define PRIV_VAR(Var)           (g_PrivImports.Var)

// KData accesses are hidden in SWITCHKEY, SETCURKEY
#undef KData
#define KData                    (*PRIV_VAR(pKData))

#ifndef SHIP_BUILD
#undef RETAILMSG
#define RETAILMSG(cond, printf_exp) ((cond)?(PUB_FUNC(NKDbgPrintfW, printf_exp),1):0)
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
                :(PUB_FUNC(NKDbgPrintfW, (TEXT("%s: DEBUGCHK failed in file %s at line %d\r\n"), \
                                          (LPWSTR)module, TEXT(__FILE__), __LINE__)), \
                  DebugBreak(),                                                \
	              0                                                            \
                 )))
#define DEBUGCHK(exp) DBGCHK(dpCurSettings.lpszName, exp)
#endif // DEBUG


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void
INT_WriteRingBuffer(
    PRINGBUFFER pRingBuf,
    LPBYTE  lpbData,        // NULL = just move pointers/counters
    DWORD   dwBytes,
    LPDWORD lpdwBytesLeft1,
    LPDWORD lpdwBytesLeft2
    )
{
    DWORD dwSrcLen1, dwSrcLen2;

    dwSrcLen1 = min(dwBytes, *lpdwBytesLeft1);
    dwSrcLen2 = dwBytes - dwSrcLen1;
    
    // Write to middle or end of buffer
    if (dwSrcLen1) {
        if (lpbData) {
            memcpy(pRingBuf->pWrite, lpbData, dwSrcLen1);
            lpbData += dwSrcLen1;
        }
        pRingBuf->pWrite += dwSrcLen1;
        pRingBuf->dwBytesLeft -= dwSrcLen1;
        *lpdwBytesLeft1 -= dwSrcLen1;
        if (pRingBuf->pWrite >= pRingBuf->pWrap) {
            pRingBuf->pWrite = pRingBuf->pBuffer;
        }
    }
    
    // Write to front of buffer in case of wrap
    if (dwSrcLen2) {
        if (lpbData) {
            memcpy(pRingBuf->pWrite, lpbData, dwSrcLen2);
        }
        pRingBuf->pWrite += dwSrcLen2;
        pRingBuf->dwBytesLeft -= dwSrcLen2;
        *lpdwBytesLeft2 -= dwSrcLen2;
    }
}


//------------------------------------------------------------------------------
// FlushBuffer is interruptable but non-preemptible
//------------------------------------------------------------------------------
static void
INT_FlushBuffer()
{
    DWORD     dwSrcTotal;
    DWORD     dwDestLen1, dwDestLen2; // Locals instead of ringbuf members because temp.
    LPBYTE    pReadCopy;
    ACCESSKEY akyOld;
     
    SWITCHKEY(akyOld, 0x00000001); // Guarantee access to NK
    
    pReadCopy = RingBuf.pHeader->pRead;
    RingBuf.dwBytesLeft = (DWORD) pReadCopy - (DWORD) RingBuf.pWrite + RingBuf.dwSize;
    if (RingBuf.dwBytesLeft > RingBuf.dwSize) {
        // wrapped.
        RingBuf.dwBytesLeft -= RingBuf.dwSize;
    }
    
    if ((RingBuf.dwBytesLeft < RingBuf.dwSize / 4) && RingBuf.pHeader->fSetEvent) {
       // Signal that the secondary buffer is getting full
       RingBuf.pHeader->fSetEvent = FALSE;  // Block any more SetEvent calls
       PUB_FUNC(EventModify, (g_hFillEvent, EVENT_SET));  // SetEvent
    }
    
    dwSrcTotal = (DWORD) pCelBuf->pWrite - (DWORD) pCelBuf->pBuffer;
    if (RingBuf.dwBytesLeft <= dwSrcTotal) {
        RETAILMSG(ZONE_VERBOSE, (MODNAME TEXT(": ERROR, Secondary Buffer Overrun. Dropping %d bytes (%d available)\r\n"),
                                 dwSrcTotal, RingBuf.dwBytesLeft));
        RingBuf.pHeader->dwLostBytes += dwSrcTotal;

        // Reset primary buffer to the beginning
        pCelBuf->pWrite = pCelBuf->pBuffer;
        pCelBuf->dwBytesLeft = pCelBuf->dwSize;
        
        SETCURKEY(akyOld);
        return;
    }

    //
    // Determine where we can write into the large buffer
    //

    if (RingBuf.pWrite >= pReadCopy) {
        //          pRead            pWrap
        //          v                v
        //  +-----------------------+ 
        //  |   2  |XXXXXX|    1    | 
        //  +-----------------------+ 
        //  ^             ^
        //  pBuffer       pWrite
        dwDestLen1 = (DWORD) RingBuf.pWrap - (DWORD) RingBuf.pWrite;
        dwDestLen2 = (DWORD) pReadCopy - (DWORD) RingBuf.pBuffer;

    } else {
        //  pBuffer          pRead
        //  v                v
        //  +-----------------------+  
        //  |XXXXX|         |XXXXXXX|
        //  +-----------------------+  
        //        ^                 ^
        //        pWrite            pWrap
        dwDestLen1 = (DWORD) pReadCopy - (DWORD) RingBuf.pWrite;
        dwDestLen2 = 0;
    }
    
    
    // Copy the data
    INT_WriteRingBuffer(&RingBuf, (LPBYTE)pCelBuf->pBuffer, dwSrcTotal, &dwDestLen1, &dwDestLen2);
    RingBuf.pHeader->pWrite = RingBuf.pWrite; // Update map header
    pCelBuf->pWrite = pCelBuf->pBuffer;
    pCelBuf->dwBytesLeft = pCelBuf->dwSize;
    

    SETCURKEY(akyOld);
}


//------------------------------------------------------------------------------
// Used to allocate the small data buffer
//------------------------------------------------------------------------------
#pragma prefast(disable: 262, "use 4K buffer")
static PBYTE
INT_AllocBuffer(
    DWORD  dwReqSize,       // Requested size
    DWORD* lpdwAllocSize    // Actual size allocated
    )
{
    DWORD  dwBufNumPages;
    LPBYTE pBuffer = NULL;
    BOOL   fRet;
    DWORD  i;
    DWORD  rgdwPageList[PAGES_IN_MAX_BUFFER];

    *lpdwAllocSize = 0;

    //
    // Allocate the buffer.
    //
    pBuffer = (LPBYTE) PUB_FUNC(VirtualAlloc, (NULL, dwReqSize, MEM_COMMIT, PAGE_READWRITE));
    if (pBuffer == NULL) {
        DEBUGMSG(1, (MODNAME TEXT(": Failed buffer VirtualAlloc (%u bytes, error=%u)\r\n"),
                     dwReqSize, (DWORD) PUB_FUNC(GetLastError, ())));
        return NULL;
    }
    
    //
    // Lock the pages and get the physical addresses.
    //
    fRet = (BOOL) PUB_FUNC(LockPages, (pBuffer, dwReqSize, rgdwPageList, LOCKFLAG_WRITE | LOCKFLAG_READ));
    if (fRet == FALSE) {
        DEBUGMSG(1, (MODNAME TEXT(": LockPages failed\r\n")));
    }

    //
    // Convert the physical addresses to statically-mapped virtual addresses
    //
    dwBufNumPages = (dwReqSize / PAGE_SIZE) + (dwReqSize % PAGE_SIZE ? 1 : 0);
    DEBUGCHK(dwBufNumPages);
    for (i = 0; i < dwBufNumPages; i++) {
        rgdwPageList[i] = (DWORD) PRIV_FUNC(Phys2Virt, (rgdwPageList[i]));
    }

    //
    // Make sure the virtual addresses are on contiguous pages
    //
    for (i = 0; i < dwBufNumPages - 1; i++) {
        if (rgdwPageList[i] != rgdwPageList[i+1] - PAGE_SIZE) {
           
            // The pages need to be together, so force a single-page buffer
            DEBUGMSG(1, (MODNAME TEXT(": Non-contiguous pages.  Forcing single-page buffer.\r\n")));
            
            PUB_FUNC(VirtualFree, (pBuffer, 0, MEM_RELEASE));
            
            // Allocate a new, single-page buffer
            dwReqSize = PAGE_SIZE;
            dwBufNumPages = 1;
            pBuffer = (LPBYTE) PUB_FUNC(VirtualAlloc, (NULL, PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE));
            if (pBuffer == NULL) {
                DEBUGMSG(1, (MODNAME TEXT(": Failed buffer VirtualAlloc\r\n")));
                return NULL;
            }
            
            // Lock the page and get the physical address.
            fRet = (BOOL) PUB_FUNC(LockPages, (pBuffer, PAGE_SIZE, rgdwPageList, LOCKFLAG_WRITE | LOCKFLAG_READ));
            if (fRet == FALSE) {
                DEBUGMSG(1, (MODNAME TEXT(": LockPages failed\r\n")));
            }
            
            // Convert the physical address to a statically-mapped virtual address
            rgdwPageList[0] = (DWORD) PRIV_FUNC(Phys2Virt, (rgdwPageList[0]));
            
            break;
        }
    }

    if (dwBufNumPages) {
       pBuffer = (LPBYTE) rgdwPageList[0];
    }

    DEBUGMSG(ZONE_VERBOSE, (MODNAME TEXT(": AllocBuffer allocated %d kB for Buffer (0x%08X)\r\n"), 
                            (dwReqSize) >> 10, pBuffer));
    
    *lpdwAllocSize = dwReqSize;
    return pBuffer;
}
#pragma prefast(pop)


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL
INT_Init()
{
    BOOL  fRet;
    DWORD dwBufSize;  // temp val

    DEBUGMSG(1, (MODNAME TEXT(": +Init\r\n")));
    
    RingBuf.hMap    = 0;
    RingBuf.pHeader = NULL;
    

    // Ignore OEM-specified parameters and use hard-coded buffer sizes
    PUB_VAR(dwCeLogLargeBuf) = RINGBUF_SIZE;
    PUB_VAR(dwCeLogSmallBuf) = SMALLBUF_SIZE;

    
    //
    // Allocate the large ring buffer that will hold the logging data.
    //
    
    do {
        // CreateFileMapping will succeed as long as there's enough VM, but
        // LockPages will only succeed if there is enough physical memory.
        RingBuf.hMap = (HANDLE)PUB_FUNC(CreateFileMappingW,
                                        (INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                         0, PUB_VAR(dwCeLogLargeBuf), DATAMAP_NAME));
        if (RingBuf.hMap) {
            RingBuf.pHeader = (PMAPHEADER)PUB_FUNC(MapViewOfFile,
                                                   (RingBuf.hMap, FILE_MAP_ALL_ACCESS,
                                                    0, 0, PUB_VAR(dwCeLogLargeBuf)));
            if (RingBuf.pHeader) {
                // We can't take page faults during the logging so lock the pages.
                fRet = (BOOL) PUB_FUNC(LockPages,
                                       (RingBuf.pHeader, PUB_VAR(dwCeLogLargeBuf),
                                        NULL, LOCKFLAG_WRITE | LOCKFLAG_READ));
                if (fRet) {
                    // Success!
                    break;
                }
                
                PUB_FUNC(UnmapViewOfFile, (RingBuf.pHeader));
                RingBuf.pHeader = NULL;
            }
            PUB_FUNC(CloseHandle, (RingBuf.hMap));
            RingBuf.hMap = 0;
        }
        
        // Keep trying smaller buffer sizes until we succeed
        PUB_VAR(dwCeLogLargeBuf) /= 2;
        if (PUB_VAR(dwCeLogLargeBuf) < PAGE_SIZE) {
            RETAILMSG(1, (MODNAME TEXT(": Large Buffer alloc failed\r\n")));
            goto error;
        }
    
    } while (PUB_VAR(dwCeLogLargeBuf) >= PAGE_SIZE);


    // Explicitly map the pointer to the kernel. Kernel isn't always the current
    // process during the logging, but will have permissions.
    RingBuf.pHeader = (PMAPHEADER)PUB_FUNC(MapPtrToProcess,
                                           ((PVOID) RingBuf.pHeader, GetCurrentProcess()));
    DEBUGMSG(ZONE_VERBOSE, (MODNAME TEXT(": RingBuf (VA) 0x%08X\r\n"), RingBuf.pHeader));
    
    RingBuf.dwSize  = PUB_VAR(dwCeLogLargeBuf) - sizeof(MAPHEADER);
    RingBuf.pBuffer = RingBuf.pWrite = RingBuf.pRead
                    = (LPBYTE)RingBuf.pHeader + sizeof(MAPHEADER);
    RingBuf.pWrap   = (LPBYTE)RingBuf.pBuffer + RingBuf.dwSize;
    RingBuf.dwBytesLeft = 0; // not used
    
    // Initialize the header on the map
    RingBuf.pHeader->dwBufSize = RingBuf.dwSize;
    RingBuf.pHeader->pWrite = RingBuf.pWrite;
    RingBuf.pHeader->pRead = RingBuf.pRead;
    RingBuf.pHeader->fSetEvent = TRUE;
    RingBuf.pHeader->dwLostBytes = 0;

    
    //
    // Initialize the immediate buffer (small).
    //
            
    pCelBuf = &CelBuf;

    // Allocate the buffer
    pCelBuf->pBuffer = (PDWORD)INT_AllocBuffer(PUB_VAR(dwCeLogSmallBuf), &dwBufSize);
    if ((pCelBuf->pBuffer == NULL) || (dwBufSize < PUB_VAR(dwCeLogSmallBuf))) {
        DEBUGMSG(1, (MODNAME TEXT(": Small buffer alloc failed\r\n")));
        goto error;
    }

    pCelBuf->pWrite      = pCelBuf->pBuffer;
	pCelBuf->dwSize      = dwBufSize;
    pCelBuf->dwBytesLeft = pCelBuf->dwSize;

    // CAPLog zones are hard-coded and never change
    pCelBuf->dwMaskUser     = 0;
    pCelBuf->dwMaskCE       = CAPLOG_ZONES;
    pCelBuf->dwMaskProcess  = (DWORD)-1;
    

    //
    // Final init stuff
    //
        
    // Create the event to flag when the buffer is getting full
    // (Must be auto-reset so we can call SetEvent during a flush!)
    g_hFillEvent = (HANDLE)PUB_FUNC(CreateEventW, (NULL, FALSE, FALSE, FILLEVENT_NAME));
    if (g_hFillEvent == NULL) {
        DEBUGMSG(1, (MODNAME TEXT(": Fill event creation failed\r\n")));
        goto error;
    }
    
    DEBUGMSG(1, (MODNAME TEXT(": -Init\r\n")));
    
    return TRUE;

error:
    // Dealloc buffers
    if (RingBuf.pHeader) {
        PUB_FUNC(UnlockPages, (RingBuf.pHeader, PUB_VAR(dwCeLogLargeBuf)));
        PUB_FUNC(UnmapViewOfFile, (RingBuf.pHeader));
    }
    if (RingBuf.hMap) {
        PUB_FUNC(CloseHandle, (RingBuf.hMap));
    }
    if (pCelBuf) {
        if (pCelBuf->pBuffer) {
            PUB_FUNC(VirtualFree, (pCelBuf->pBuffer, 0, MEM_DECOMMIT));
        }
        pCelBuf = NULL;
    }

    return FALSE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Called by CeLogGetZones to retrieve the current zone settings.
BOOL
EXT_QueryZones( 
    LPDWORD lpdwZoneUser,
    LPDWORD lpdwZoneCE,
    LPDWORD lpdwZoneProcess
    )
{
    // Check whether the library has been initialized.  Use pCelBuf instead of 
    // g_fInit because CeLogQueryZones is called during IOCTL_CELOG_REGISTER 
    // before g_fInit is set.
    if (!pCelBuf) {
        return FALSE;
    }
    
    if (lpdwZoneUser)
        *lpdwZoneUser = pCelBuf->dwMaskUser;
    if (lpdwZoneCE)
        *lpdwZoneCE = pCelBuf->dwMaskCE;
    if (lpdwZoneProcess)
        *lpdwZoneProcess = pCelBuf->dwMaskProcess;

    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
EXT_LogData( 
    BOOL fTimeStamp,
    WORD wID,
    VOID *pData,
    WORD wLen,
    DWORD dwZoneUser,
    DWORD dwZoneCE,
    WORD wFlag,
    BOOL fFlagged
    )
{
    LARGE_INTEGER liPerfCount;
    DWORD dwLen, dwHeaderLen;
    WORD wFlagID = 0;
    BOOL fWasIntEnabled;

    if (!g_fInit) {
        //
        // we either failed the init, or haven't been initialized yet!
        //
        return;
    }

    // Better provide data if wLen > 0
    if ((pData == NULL) && (wLen != 0)) {
        DEBUGCHK(0);
        return;
    }
    
    DEBUGCHK(wID < CELID_MAX);                // Out of range
    DEBUGCHK(dwZoneCE || dwZoneUser);         // Need to provide some zone.

    fWasIntEnabled = PRIV_FUNC(INTERRUPTS_ENABLE, (FALSE));
    if (wID == CELID_THREAD_SWITCH) {
        //
        // We handle thread switches specially. Flush the immediate buffer.
        //
        if (PUB_FUNC(InSysCall, ())) {
            INT_FlushBuffer();
        } else {
            PRIV_FUNC(KCall, ((PKFN)INT_FlushBuffer));
        }
    }

    if (!(pCelBuf->dwMaskCE & dwZoneCE)) {
        //
        // This zone(s) is disabled.
        //
        PRIV_FUNC(INTERRUPTS_ENABLE, (fWasIntEnabled));
        return;
    }
       
    //
    // DWORD-aligned length
    //
    dwLen = (wLen + 3) & ~3;
    dwHeaderLen = 8 + (fFlagged ? 4 : 0);
    
    if (pCelBuf->dwBytesLeft < (dwLen + dwHeaderLen)) {
        if (PUB_FUNC(InSysCall, ())) {
            INT_FlushBuffer();
        } else {
            PRIV_FUNC(KCall, ((PKFN)INT_FlushBuffer));
        }
    }
    
    // Must get timestamp AFTER the flushbuffer call, because CeLog calls can
    // be generated by the flush itself, and appear out-of-order.
    if (fTimeStamp) {
        if (PUB_FUNC(QueryPerformanceCounter, (&liPerfCount))) {
            liPerfCount.QuadPart >>= g_dwPerfTimerShift;
        } else {
            // Call failed; caller is probably not trusted
            fTimeStamp = FALSE;
        }
    }

    // Header for unflagged data:
    
    //   <---------------- 1 DWORD ----------------->
    //  |T|R|       ID        |          Len         |
    //  |                 Timestamp                  | (if T=1)
    //  |                   Data...                  | (etc)

    // Header for flagged data:
    
    //   <---------------- 1 DWORD ----------------->
    //  |T|R|  CELID_FLAGGED  |          Len         |
    //  |                 Timestamp                  | (if T=1)
    //  |         ID          |         Flag         |
    //  |                   Data...                  | (etc)
        
    
    // If the data has a flag, use the ID CELID_FLAGGED and move the real ID
    // inside the data.
    if (fFlagged) {
        wFlagID = wID;
        wID = CELID_FLAGGED;
    }


    *pCelBuf->pWrite++ = fTimeStamp << 31 | wID << 16 | wLen;
    pCelBuf->dwBytesLeft -= sizeof(DWORD);

    if (fTimeStamp) {
        *pCelBuf->pWrite++ = liPerfCount.LowPart;
        pCelBuf->dwBytesLeft -= sizeof(DWORD);
    }

    if (fFlagged) {
        *pCelBuf->pWrite++ = wFlagID << 16 | wFlag;
        pCelBuf->dwBytesLeft -= sizeof(DWORD);
    }

    if (pData && wLen) {
        memcpy(pCelBuf->pWrite, pData, wLen);
    }
    pCelBuf->pWrite += (dwLen >> 2);
    pCelBuf->dwBytesLeft -= dwLen;
    
    PRIV_FUNC(INTERRUPTS_ENABLE, (fWasIntEnabled));
}


//------------------------------------------------------------------------------
// Hook up function pointers and initialize logging
//------------------------------------------------------------------------------
BOOL static
INT_InitLibrary(
    FARPROC pfnKernelLibIoControl
    )
{
    CeLogExportTable exports;
    LARGE_INTEGER liPerfFreq;

    //
    // KernelLibIoControl provides the back doors we need to obtain kernel
    // function pointers and register logging functions.
    //
    
    // Get public imports from kernel
    g_PubImports.dwVersion = CELOG_IMPORT_VERSION;
    if (!pfnKernelLibIoControl((HANDLE)KMOD_CELOG, IOCTL_CELOG_IMPORT, &g_PubImports,
                               sizeof(CeLogImportTable), NULL, 0, NULL)) {
        // Can't DEBUGMSG or anything here b/c we need the import table to do that
        return FALSE;
    }

    // CAPLog requires MapViewOfFile
    if (g_PubImports.pMapViewOfFile == NULL) {
        DEBUGMSG(1, (MODNAME TEXT(": Error, cannot run because this kernel does not support memory-mapped files\r\n")));
        PUB_FUNC(SetLastError, (ERROR_NOT_SUPPORTED));
        return FALSE;
    }
    
    // Get private imports from kernel
    g_PrivImports.dwVersion = CELOG_PRIVATE_IMPORT_VERSION;
    if (!pfnKernelLibIoControl((HANDLE)KMOD_CELOG, IOCTL_CELOG_IMPORT_PRIVATE, &g_PrivImports,
                               sizeof(CeLogPrivateImports), NULL, 0, NULL)) {
        DEBUGMSG(1, (MODNAME TEXT(": Error, private import failed\r\n")));
        PUB_FUNC(SetLastError, (ERROR_NOT_SUPPORTED));
        return FALSE;
    }

    // Now initialize logging
    if (!INT_Init()) {
        return FALSE;
    }

    // Get the high-performance timer's frequency
    PUB_FUNC(QueryPerformanceFrequency, (&liPerfFreq));

    




    g_dwPerfTimerShift = 0;
    while (liPerfFreq.QuadPart > MAX_ROLLOVER_COUNT) {
        g_dwPerfTimerShift++;
        liPerfFreq.QuadPart >>= 1;
    }

    // Register logging functions with the kernel
    exports.dwVersion          = 2;  // Hard-code so this code doesn't have to
                                     // change if CELOG_EXPORT_VERSION changes
    exports.pfnCeLogData       = EXT_LogData;
    exports.pfnCeLogInterrupt  = NULL;  // Not supported, interrupt data ignored
    exports.pfnCeLogSetZones   = NULL;  // Not supported, zones are static
    exports.pfnCeLogQueryZones = EXT_QueryZones;
    // The exported frequency is only used by the kernel to set the frequency 
    // in the sync header during CeLogReSync().
    exports.dwCeLogTimerFrequency = 0;
    
    if (!pfnKernelLibIoControl((HANDLE)KMOD_CELOG, IOCTL_CELOG_REGISTER, &exports,
                               sizeof(CeLogExportTable), NULL, 0, NULL)) {
        DEBUGMSG(1, (MODNAME TEXT(": Unable to register logging functions with kernel\r\n")));
        return FALSE;
    }

    g_fInit = TRUE;
    
    // Now that logging functions are registered, request a resync
    DEBUGMSG(ZONE_VERBOSE, (TEXT("CeLog resync\r\n")));
    PUB_FUNC(CeLogReSync, ());
    
    return TRUE;
}


//------------------------------------------------------------------------------
// DLL entry
//------------------------------------------------------------------------------
BOOL WINAPI
CAPLogDLLEntry(HINSTANCE DllInstance, INT Reason, LPVOID Reserved)
{
    switch (Reason) {
    case DLL_PROCESS_ATTACH:
        if (!g_fInit) {
            if (Reserved) {
                // Reserved parameter is a pointer to KernelLibIoControl function
                return INT_InitLibrary((FARPROC)Reserved);
            }
            // Loaded via LoadLibrary instead of LoadKernelLibrary?
            return FALSE;
        }
        break;

    case DLL_PROCESS_DETACH:
        break;
    }
    
    return TRUE;
}

