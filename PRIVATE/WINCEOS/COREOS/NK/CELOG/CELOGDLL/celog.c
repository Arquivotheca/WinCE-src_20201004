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
//      celog.c
//  
//  Abstract:  
//
//      Implements the CeLog event logging functions.
//      
//------------------------------------------------------------------------------

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
    DWORD  dwBytesLeft; // Calculated during flush for RingBuf, always valid for IntBuf
} RINGBUFFER, *PRINGBUFFER;

RINGBUFFER RingBuf;
RINGBUFFER IntBuf;
PCEL_BUFFER pCelBuf;

CEL_BUFFER CelBuf;

//------------------------------------------------------------------------------
// EXPORTED GLOBAL VARIABLES

#define SMALLBUF_MAX         4096*1024  // Maximum size an OEM can boost the small buffer to
#define PAGES_IN_MAX_BUFFER  (SMALLBUF_MAX/PAGE_SIZE)
#define RINGBUF_SIZE         128*1024   // Default size of large buffer

#ifndef CELOG_SIZE
#define SMALLBUF_SIZE        4*1024     // Default size of small buffer
#else
#define SMALLBUF_SIZE        CELOG_SIZE // Default size of small buffer
#endif                                                 


//------------------------------------------------------------------------------
// MISC

// RESERVED zones cannot be turned on, CELZONE_ALWAYSON cannot be turned off
#define VALIDATE_ZONES(dwVal)   (((dwVal) & ~(CELZONE_RESERVED1|CELZONE_RESERVED2)) | CELZONE_ALWAYSON)

#define MAX_ROLLOVER_MINUTES    30
#define MAX_ROLLOVER_COUNT      (0xFFFFFFFF / (MAX_ROLLOVER_MINUTES * 60))

HANDLE g_hFillEvent;  // Used to indicate the secondary buffer is getting full
BOOL   g_fInit;
DWORD  g_dwDiscardedInterrupts;
DWORD  g_dwPerfTimerShift;

// Inputs from kernel
CeLogImportTable    g_PubImports;
CeLogPrivateImports g_PrivImports;


#define DATAMAP_NAME      CELOG_DATAMAP_NAME
#define FILLEVENT_NAME    CELOG_FILLEVENT_NAME


//------------------------------------------------------------------------------
// Abstract accesses to imports

#define PUB_FUNC(Api, Args)     ((g_PubImports.p##Api) Args)
#define PUB_VAR(Var)            (g_PubImports.Var)
#define PRIV_FUNC(Api, Args)    ((g_PrivImports.p##Api) Args)
#define PRIV_VAR(Var)           (g_PrivImports.Var)

#define ISAPIREADY(hAPI)         ((UserKInfo[KINX_API_MASK] & (1 << (hAPI))) != 0)

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
    DWORD     dwSrcLen1, dwSrcLen2, dwSrcTotal;
    DWORD     dwDestLen1, dwDestLen2; // Locals instead of ringbuf members because temp.
    LPBYTE    pReadCopy, pWriteCopy;
    DWORD     dwBytesLeftCopy;
    ACCESSKEY akyOld;
    static DWORD dwTLBPrev = 0;
     
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
    

    //--------------------------------------------------------------
    // Now copy in the interrupts that happened during this quantum
    //
    
    // Copy the counters because interrupts are still on (retry if interrupted)
    do {
        pWriteCopy = *(volatile LPBYTE*)&IntBuf.pWrite;
        dwBytesLeftCopy = *(volatile DWORD*)&IntBuf.dwBytesLeft;
    } while ((pWriteCopy != *(volatile LPBYTE*)&IntBuf.pWrite)
             || (dwBytesLeftCopy != *(volatile DWORD*)&IntBuf.dwBytesLeft));

    dwSrcTotal = IntBuf.dwSize - dwBytesLeftCopy;
    
    if (dwSrcTotal) {
        
        if (RingBuf.dwBytesLeft < (dwSrcTotal + 8)) {
            RETAILMSG(ZONE_VERBOSE, (MODNAME TEXT(": ERROR, Secondary Buffer Overrun. Dropping %d bytes (interrupts) (%d available)\r\n"),
                                     dwSrcTotal, RingBuf.dwBytesLeft));
            RingBuf.pHeader->dwLostBytes += dwSrcTotal + 8;

            // Reset interrupt buffer to the beginning
            IntBuf.pRead = pWriteCopy;
            IntBuf.dwBytesLeft += dwSrcTotal;
            
            SETCURKEY(akyOld);
            return;
        }
        
        // Write in the interrupt header
        *((PDWORD) RingBuf.pWrite) = CELID_INTERRUPTS << 16 | (WORD) (dwSrcTotal + 4);
        INT_WriteRingBuffer(&RingBuf, NULL, sizeof(DWORD), &dwDestLen1, &dwDestLen2);
        *((PDWORD) RingBuf.pWrite) = g_dwDiscardedInterrupts;
        INT_WriteRingBuffer(&RingBuf, NULL, sizeof(DWORD), &dwDestLen1, &dwDestLen2);

        // Determine where to copy from in the interrupt buffer
        if (pWriteCopy > IntBuf.pRead) {
            //         pRead            pWrap
            //         v                v
            //  +-----------------------+ 
            //  |      |XX 1 X|         | 
            //  +-----------------------+ 
            //  ^             ^
            //  pBuffer       pWrite
            dwSrcLen1 = (DWORD) pWriteCopy - (DWORD) IntBuf.pRead;
            dwSrcLen2 = 0;
    
        } else {
            //  pBuffer         pRead
            //  v               v
            //  +-----------------------+  
            //  |X 2 X|         |XX 1 XX|
            //  +-----------------------+  
            //        ^                 ^
            //        pWrite            pWrap
            dwSrcLen1 = (DWORD) IntBuf.pWrap  - (DWORD) IntBuf.pRead;
            dwSrcLen2 = (DWORD) pWriteCopy - (DWORD) IntBuf.pBuffer;
        }
    
        // Copy the data
        if (dwSrcLen1) {
            INT_WriteRingBuffer(&RingBuf, IntBuf.pRead, dwSrcLen1, &dwDestLen1, &dwDestLen2);
        }
        if (dwSrcLen2) {
            INT_WriteRingBuffer(&RingBuf, IntBuf.pBuffer, dwSrcLen2, &dwDestLen1, &dwDestLen2);
        }
        RingBuf.pHeader->pWrite = RingBuf.pWrite; // Update map header
        IntBuf.pRead = pWriteCopy;
        IntBuf.dwBytesLeft += dwSrcLen1 + dwSrcLen2;
        g_dwDiscardedInterrupts = 0;
    }


    //--------------------------------------------------------------
    // Now copy in the TLB Misses that happened in this quantum
    //
    if (PUB_VAR(pdwCeLogTLBMiss) && (*PUB_VAR(pdwCeLogTLBMiss) != dwTLBPrev)
        && (pCelBuf->dwMaskCE & CELZONE_TLB)) {
        
        if (RingBuf.dwBytesLeft < 2*sizeof(DWORD)) {
            RETAILMSG(ZONE_VERBOSE, (MODNAME TEXT(": ERROR, Secondary Buffer Overrun. Delaying TLB Miss Data report\r\n")));
            RingBuf.pHeader->dwLostBytes += 2*sizeof(DWORD);
            SETCURKEY(akyOld);
            return;
        }
        
        // Write in the TLB miss header
        *((PDWORD) RingBuf.pWrite) = CELID_SYSTEM_TLB << 16 | sizeof(CEL_SYSTEM_TLB);
        INT_WriteRingBuffer(&RingBuf, NULL, sizeof(DWORD), &dwDestLen1, &dwDestLen2);
        *((PDWORD) RingBuf.pWrite) = *PUB_VAR(pdwCeLogTLBMiss) - dwTLBPrev;
        INT_WriteRingBuffer(&RingBuf, NULL, sizeof(DWORD), &dwDestLen1, &dwDestLen2);
        RingBuf.pHeader->pWrite = RingBuf.pWrite; // Update map header
        
        dwTLBPrev = *PUB_VAR(pdwCeLogTLBMiss);
    }

    SETCURKEY(akyOld);
}


//------------------------------------------------------------------------------
// Used to allocate the small data buffer and interrupt buffer
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
    IntBuf.pBuffer  = NULL;
    

    //
    // Look for the import parameters in the local registry if the registry is
    // available.  This won't be possible if CeLog is loaded on boot 
    // (IMGCELOGENABLE=1) but will be if it's loaded later.
    //
    
    if (ISAPIREADY(SH_FILESYS_APIS)) {
        DWORD dwType, dwVal;

        dwBufSize = sizeof(DWORD);
        if ((PRIV_FUNC(RegQueryValueExW, (HKEY_LOCAL_MACHINE, TEXT("BufferSize"),
                                          TEXT("System\\CeLog"), &dwType,
                                          (LPBYTE)&dwVal,
                                          &dwBufSize)) == ERROR_SUCCESS)
            && (dwType == REG_DWORD)) {

            PUB_VAR(dwCeLogLargeBuf) = dwVal;
        }
    }


    //
    // Check the values of the exported variables before using them.
    //
    
    if (PUB_VAR(dwCeLogLargeBuf) & (PAGE_SIZE-1)) {
        DEBUGMSG(1, (MODNAME TEXT(": Large buffer size not page-aligned, rounding down.\r\n")));
        PUB_VAR(dwCeLogLargeBuf) &= ~(PAGE_SIZE-1);
    }
    if (PUB_VAR(dwCeLogLargeBuf) == 0) {
        DEBUGMSG(1, (MODNAME TEXT(": Large buffer size unspecified, using default size\r\n")));
        PUB_VAR(dwCeLogLargeBuf) = RINGBUF_SIZE;
    }
    if (PUB_VAR(dwCeLogLargeBuf) >= (DWORD) (UserKInfo[KINX_PAGEFREE] * PAGE_SIZE)) {
        // Try 3/4 of available RAM
        PUB_VAR(dwCeLogLargeBuf) = ((UserKInfo[KINX_PAGEFREE] * PAGE_SIZE * 3) / 4) & ~(PAGE_SIZE-1);
        DEBUGMSG(1, (MODNAME TEXT(": Only 0x%08X RAM available, using 0x%08x for large buffer.\r\n"), 
                     (UserKInfo[KINX_PAGEFREE] * PAGE_SIZE), PUB_VAR(dwCeLogLargeBuf)));
    }

    if ((PUB_VAR(dwCeLogSmallBuf) > PUB_VAR(dwCeLogLargeBuf) / 2)
        || (PUB_VAR(dwCeLogSmallBuf) > SMALLBUF_MAX)
        || (PUB_VAR(dwCeLogSmallBuf) == 0)) {
       DEBUGMSG(1, (MODNAME TEXT(": Small buffer size invalid or unspecified, using default size\r\n")));
       PUB_VAR(dwCeLogSmallBuf) = SMALLBUF_SIZE;
    }
    
        
    
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
    

    //
    // Interrupts have a separate buffer. 
    //
    
    // Allocate the buffer.
    IntBuf.pBuffer = INT_AllocBuffer(PUB_VAR(dwCeLogSmallBuf), &dwBufSize);
    if ((IntBuf.pBuffer == NULL) || (dwBufSize < PUB_VAR(dwCeLogSmallBuf))) {
        DEBUGMSG(1, (MODNAME TEXT(": Interrupt buffer alloc failed\r\n")));
        goto error;
    }
    
    IntBuf.pRead  = (LPBYTE) IntBuf.pBuffer;
    IntBuf.pWrite = (LPBYTE) IntBuf.pBuffer;
    IntBuf.pWrap  = (LPBYTE) ((DWORD) IntBuf.pBuffer + dwBufSize);
    IntBuf.dwSize = dwBufSize;
    IntBuf.dwBytesLeft = dwBufSize;
    

    RETAILMSG((PUB_VAR(dwCeLogLargeBuf) != RINGBUF_SIZE),
              (MODNAME TEXT(": Large buffer size = %u\r\n"), PUB_VAR(dwCeLogLargeBuf)));
    DEBUGMSG((pCelBuf->dwSize != SMALLBUF_SIZE),
              (MODNAME TEXT(": Small buffer size = %u\r\n"), pCelBuf->dwSize));
    
    
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
    if (IntBuf.pBuffer) {
        PUB_FUNC(VirtualFree, (IntBuf.pBuffer, 0, MEM_DECOMMIT));
    }

    return FALSE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
EXT_SetZones( 
    DWORD dwZoneUser,
    DWORD dwZoneCE,
    DWORD dwZoneProcess
    )
{
    if (!g_fInit) {
        //
        // we either failed the init, or haven't been initialized yet!
        //
        return;
    }
    
    pCelBuf->dwMaskUser    = VALIDATE_ZONES(dwZoneUser);
    pCelBuf->dwMaskCE      = VALIDATE_ZONES(dwZoneCE);
    pCelBuf->dwMaskProcess = dwZoneProcess;
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

    

//    if (!(pCelBuf->dwMaskProcess & (pCurThread->pOwnerProc->aky))) {
//        //
//        // Logging is disabled for all threads of this process
//        //
//        return;
//    }

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

    if (!(pCelBuf->dwMaskUser & dwZoneUser) && !(pCelBuf->dwMaskCE & dwZoneCE)) {
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
//------------------------------------------------------------------------------
void 
EXT_LogInterrupt( 
    DWORD dwLogValue
    )
{
    LARGE_INTEGER liPerfCount;

    if (!g_fInit) {
        //
        // we either failed the init, or haven't been initialized yet!
        //
        return;
    }

    if (!(pCelBuf->dwMaskCE & CELZONE_INTERRUPT)) {
        //
        // This zone(s) is disabled.
        //
        return;
    }

    if (PUB_FUNC(QueryPerformanceCounter, (&liPerfCount))) {
        liPerfCount.QuadPart >>= g_dwPerfTimerShift;
    } else {
        // Call failed; caller is probably not trusted
        liPerfCount.QuadPart = 0;
    }
    
    if (!IntBuf.dwBytesLeft) {
        //
        // The interrupt buffer is full! Just have to drop some from the 
        // beginning (keep most recent)
        //
        g_dwDiscardedInterrupts++;

        IntBuf.dwBytesLeft = 2*sizeof(DWORD);
        IntBuf.pRead += 2*sizeof(DWORD);
        if (IntBuf.pRead >= IntBuf.pWrap) {
            IntBuf.pRead = IntBuf.pBuffer;
        }
    }

    //
    // Write the data to the interrupt buffer.
    //
    *((PDWORD) (IntBuf.pWrite)) = liPerfCount.LowPart;
    *((PDWORD) (IntBuf.pWrite + sizeof(DWORD))) = dwLogValue;

    



    IntBuf.pWrite += 2*sizeof(DWORD);
     
    if (IntBuf.pWrite >= IntBuf.pWrap) {
        IntBuf.pWrite = IntBuf.pBuffer;
    }
    IntBuf.dwBytesLeft -= 2*sizeof(DWORD);
}


//------------------------------------------------------------------------------
// Check preset zones
//------------------------------------------------------------------------------
void static
INT_InitZones(
    FARPROC pfnKernelLibIoControl
    )
{
    DWORD dwType, dwVal, dwBufSize;
    
    // Start with built-in zone defaults
    pCelBuf->dwMaskUser    = CELOG_USER_MASK;
    pCelBuf->dwMaskCE      = CELOG_CE_MASK;
    pCelBuf->dwMaskProcess = CELOG_PROCESS_MASK;   

    // Check the desktop PC's registry
    pfnKernelLibIoControl((HANDLE)KMOD_CELOG, IOCTL_CELOG_GETDESKTOPZONE,
                          TEXT("CeLogZoneUser"), 13*sizeof(WCHAR),
                          &(pCelBuf->dwMaskUser), sizeof(DWORD), NULL);
    pCelBuf->dwMaskUser = VALIDATE_ZONES(pCelBuf->dwMaskUser);
    pfnKernelLibIoControl((HANDLE)KMOD_CELOG, IOCTL_CELOG_GETDESKTOPZONE,
                          TEXT("CeLogZoneCE"), 11*sizeof(WCHAR),
                          &(pCelBuf->dwMaskCE), sizeof(DWORD), NULL);
    pCelBuf->dwMaskCE = VALIDATE_ZONES(pCelBuf->dwMaskCE);
    pfnKernelLibIoControl((HANDLE)KMOD_CELOG, IOCTL_CELOG_GETDESKTOPZONE,
                          TEXT("CeLogZoneProcess"), 16*sizeof(WCHAR),
                          &(pCelBuf->dwMaskProcess), sizeof(DWORD), NULL);
    
    // Check the device-side registry
    if (ISAPIREADY(SH_FILESYS_APIS)) {
        dwBufSize = sizeof(DWORD);
        if ((PRIV_FUNC(RegQueryValueExW, (HKEY_LOCAL_MACHINE, TEXT("ZoneCE"),
                                          TEXT("System\\CeLog"), &dwType,
                                          (LPBYTE)&dwVal,
                                          &dwBufSize)) == ERROR_SUCCESS)
            && (dwType == REG_DWORD)) {
            pCelBuf->dwMaskCE = VALIDATE_ZONES(dwVal);
        }
    }

    RETAILMSG(1, (TEXT("CeLog Zones : CE = 0x%08X, User = 0x%08X, Proc = 0x%08X\r\n"),
                  pCelBuf->dwMaskCE, pCelBuf->dwMaskUser, pCelBuf->dwMaskProcess));
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

    // CeLog requires MapViewOfFile
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

    // scale the frequency down so that the 32 bits of the high-performance
    // timer we associate with log events doesn't wrap too quickly.
    g_dwPerfTimerShift = 0;
    while (liPerfFreq.QuadPart > MAX_ROLLOVER_COUNT) {
        g_dwPerfTimerShift++;
        liPerfFreq.QuadPart >>= 1;
    }

    INT_InitZones(pfnKernelLibIoControl);
    
    // Register logging functions with the kernel
    exports.dwVersion          = CELOG_EXPORT_VERSION;
    exports.pfnCeLogData       = EXT_LogData;
    exports.pfnCeLogInterrupt  = EXT_LogInterrupt;
    exports.pfnCeLogSetZones   = EXT_SetZones;
    exports.pfnCeLogQueryZones = EXT_QueryZones;
    exports.dwCeLogTimerFrequency = liPerfFreq.LowPart;
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
CeLogDLLEntry(HINSTANCE DllInstance, INT Reason, LPVOID Reserved)
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

