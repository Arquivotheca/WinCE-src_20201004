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

Abstract:  
 This file implements the NK kernel profiler support.

Functions:


Notes: 

--*/
#define C_ONLY
#include "kernel.h"
#include <profiler.h>

#define PROFILEMSG(cond,printf_exp)   \
   ((cond)?(NKDbgPrintfW printf_exp),1:0)

// Turning this zone on can break unbuffered mode, and can break buffered mode
// if the buffer overruns
#define ZONE_UNACCOUNTED 0

#ifdef KCALL_PROFILE
#include "kcall.h"
KPRF_t KPRFInfo[MAX_KCALL_PROFILE];

#ifdef NKPROF
#define KCALL_NextThread    10  // Next thread's entry in array
#endif
#endif // KCALL_PROFILE




int (*PProfileInterrupt)(void) = NULL;  // pointer to profiler ISR, 
                                        // set in platform profiler code

BOOL bProfileTimerRunning = FALSE;      // this variable is used in OEMIdle(), so must be defined even in a non-profiling kernel
const volatile DWORD dwProfileBufferMax = 0;  // Can be overwritten as a FIXUPVAR in config.bib

#ifdef NKPROF

// Used to track what mode the profiler is in
DWORD g_dwProfilerFlags;                // ObjCall, KCall, Buffer, CeLog

// Used for symbol lookup
DWORD g_dwNumROMModules;                // Number of modules in all XIP regions
DWORD g_dwFirstROMDLL;                  // First DLL address over all XIP regions
DWORD g_dwNKIndex;                      // Index (wrt first XIP region) of NK in PROFentry array


#if defined(x86)  // Callstack capture is only supported on x86 right now
// Temp buffer used to hold callstack during profiler hit; not threadsafe but
// all profiler hits should be serialized
#define PROFILER_MAX_STACK_FRAME        50
static BYTE g_ProfilerStackBuffer[PROFILER_MAX_STACK_FRAME*sizeof(CallSnapshot)];
static CONTEXT g_ProfilerContext;
#endif // defined(x86)


// Total number of profiler samples is g_ProfilerState_dwSamplesRecorded + g_ProfilerState_dwSamplesDropped + g_ProfilerState_dwSamplesInIdle
static DWORD g_ProfilerState_dwSamplesRecorded;    // Number of profiler hits that were recorded in the buffer or symbol list
static DWORD g_ProfilerState_dwSamplesDropped;     // Number of profiler hits that were not recorded due to buffer overrun

//------------------------------------------------------------------------------
// System state that is tracked while the profiler is running in Monte Carlo mode.

       DWORD g_ProfilerState_dwInterrupts;         // Number of interrupts that occurred while profiling, including profiler interrupt
       DWORD g_ProfilerState_dwProfilerIntsInTLB;  // Number of TLB misses that had profiler interrupts pending
static DWORD g_ProfilerState_dwSamplesInIdle;      // Number of profiler samples when no threads were scheduled
extern DWORD dwCeLogTLBMiss;                       // Number of TLB misses since boot (always running)
// System state counters that will run when profiling is paused are tracked by 
// recording start values while running and elapsed values during the pause.
static DWORD g_ProfilerState_dwTLBCount;           // Used to count TLB misses while profiling
static DWORD g_ProfilerState_dwTickCount;          // Used to count ticks while profiling

//------------------------------------------------------------------------------


extern void SC_DumpKCallProfile(DWORD bReset);
extern BOOL ProfilerAllocBuffer(void);
extern BOOL ProfilerFreeBuffer(void);


#define NUM_MODULES 200                 // max number of modules to display in report

                                        // platform routine to disable profiler timer
extern void OEMProfileTimerDisable(void);
                                        // platform routine to enable profiler timer
extern void OEMProfileTimerEnable(DWORD dwUSec);

typedef struct {
    DWORD ra;                           // Interrupt program counter
    TOCentry * pte;
} PROFBUFENTRY, *PPROFBUFENTRY;

#define ENTRIES_PER_PAGE    (PAGE_SIZE / sizeof(PROFBUFENTRY))

//
// The number of bits to shift is a page shift - log2(sizeof(PROFBUFENTRY))
//
#if PAGE_SIZE == 4096
  #define PROF_PAGE_SHIFT       9
#elif PAGE_SIZE == 2048
  #define PROF_PAGE_SHIFT       8
#elif PAGE_SIZE == 1024
  #define PROF_PAGE_SHIFT       7
#else
  #error "Unsupported Page Size"
#endif // PAGE_SIZE

#define PROF_ENTRY_MASK       ((1 << PROF_PAGE_SHIFT) - 1)

void ClearProfileHits(void);            // clear all profile counters

#define PROFILE_BUFFER_MAX   256*1024*1024  // Absolute max of 256 MB
#define PAGES_IN_MAX_BUFFER    PROFILE_BUFFER_MAX/PAGE_SIZE
#define BLOCKS_PER_SECTION  0x200
#define PAGES_PER_SECTION   BLOCKS_PER_SECTION * PAGES_PER_BLOCK

PPROFBUFENTRY g_pPBE[PAGES_IN_MAX_BUFFER];
HANDLE g_hProfileBuf;                   // Handle of memory-mapped file
PDWORD g_pdwProfileBufStart;            // Pointer to profile buffer (mmfile)
DWORD  g_dwProfileBufNumPages;          // Total number of pages in buffer
DWORD  g_dwProfileBufNumEntries;        // Total number of profiler hits that fit in buffer


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Look up the profiler symbol entry for the given module
PROFentry* GetProfEntryFromTOCEntry(
    TOCentry *tocexeptr
    )
{
    ROMChain_t *pROM = ROMChain;   // Chain of ROM XIP regions
    DWORD       dwModIndex;        // Index of module wrt first XIP region
    TOCentry   *tocptr;            // table of contents entry pointer

    // Look for the module in each successive XIP region, to find its index
    // in the profiler symbol table.
    dwModIndex = 0;
    while (pROM) {
        tocptr = (TOCentry *)(pROM->pTOC+1);  // tocptr-> first entry in ROM
        if ((tocexeptr >= tocptr)
            && (tocexeptr < tocptr + pROM->pTOC->nummods)) {
            // Found it
            dwModIndex += ((DWORD)tocexeptr - (DWORD)tocptr) / sizeof(TOCentry);
            return (PROFentry *)/*pROM->*/pTOC->ulProfileOffset + dwModIndex;  // All ROMs point to same profiler table
        }

        dwModIndex += pROM->pTOC->nummods;
        pROM = pROM->pNext;
    }
    
    // Not found in any XIP region
    return NULL;
}


/*
 *  ProfilerSymbolHit - Lookup profiler interrupt call address and record hit count
 *
 *  Input:  ra - interrupt return address
 *          tocexeptr - pCurProf.oe->tocexeptr at time of interrupt
 *
 *  Attempts to find symbol address for the interrupt call address and increments
 *  hit counts for module and symbol if found.
 */
void ProfilerSymbolHit(
    unsigned int ra,
    TOCentry *tocexeptr
    )
{
    PROFentry  *profptr;               // profile entry pointer

    if (IsKernelVa(ra)) {
        // The hit is inside NK
        ra &= 0xdfffffff;              // mask off uncached bit
        profptr = (PROFentry *)pTOC->ulProfileOffset + g_dwNKIndex;
    
    } else if (ZeroPtr(ra) >= g_dwFirstROMDLL) {
        // The hit is in a dll in the ROM
        profptr = (PROFentry *)pTOC->ulProfileOffset+1;
        while (!((ZeroPtr(ra) >= ZeroPtr(profptr->ulStartAddr)) && (ZeroPtr(ra) <= ZeroPtr(profptr->ulEndAddr)))) {
            //
            // This is not the module/section for this hit.
            //
            profptr++;
            if (profptr >= (PROFentry *)pTOC->ulProfileOffset + g_dwNumROMModules) {
                PROFILEMSG(ZONE_UNACCOUNTED, (TEXT("Hit dropped (ROM DLL not found), ra=0x%08x tocexeptr=0x%08x\r\n"), ra, tocexeptr));
                return;
            }
        }

    } else {
        // This is an exe in the ROM, something loaded in RAM or unknown
        profptr = GetProfEntryFromTOCEntry(tocexeptr);
        if (!profptr) {
            // Not found in any XIP region
            PROFILEMSG(ZONE_UNACCOUNTED, (TEXT("Hit dropped (module not found), ra=0x%08x tocexeptr=0x%08x\r\n"), ra, tocexeptr));
            return;
        }
    }


    // 
    // We've picked a particular module and section (profptr), now verify the
    // hit is really in bounds.
    //
    if ((ra < profptr->ulStartAddr) || (ra > profptr->ulEndAddr)) {
        // Most likely a 0xDFFFnnnn hit inside PSL entry
        PROFILEMSG(ZONE_UNACCOUNTED, (TEXT("Hit dropped (out of bounds), ra=0x%08x tocexeptr=0x%08x (0x%08x 0x%08x 0x%08x)\r\n"),
                   ra, tocexeptr, profptr, profptr->ulStartAddr, profptr->ulEndAddr));
        return;
    }
    
    profptr->ulHits++;                  // increment hits for this module
    if (profptr->ulNumSym) {
        SYMentry    *symptr;            // profiler symbol entry pointer
        unsigned int iSym;              // symbol counter
        SYMentry    *pClosestSym;       // nearest symbol entry found

        // look up the symbol if module found
        iSym = profptr->ulNumSym - 1;
        symptr = (SYMentry*)profptr->ulHitAddress;
        pClosestSym = symptr;

        // Scan the whole list of symbols, looking for the closest match.
        while (iSym) {
            // Keep track of the closest symbol found
            if (((unsigned int)symptr->ulFuncAddress <= ra)
                && (symptr->ulFuncAddress >= pClosestSym->ulFuncAddress)) {
                pClosestSym = symptr;
            }

            iSym--;
            symptr++;
        }

        pClosestSym->ulFuncHits++;      // inc hit count for this symbol
    }
    return;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static VOID
DoFreeBuffer(void)
{
    BOOL fRet;
    CALLBACKINFO cbi;

    // Need to use callbacks because the kernel owns the map instead of the app
    
    if (g_pdwProfileBufStart) {
        // UnmapViewOfFile(g_pdwProfileBufStart);
        cbi.hProc = ProcArray[0].hProc;
        cbi.pfn = (FARPROC)UnmapViewOfFile;
        cbi.pvArg0 = g_pdwProfileBufStart;
        fRet = (BOOL)PerformCallBack(&cbi);
        ASSERT(fRet);

        g_pdwProfileBufStart = NULL;
    }
    
    if (g_hProfileBuf) {
        // CloseHandle(g_hProfileBuf);
        cbi.hProc = ProcArray[0].hProc;
        cbi.pfn = (FARPROC)CloseHandle;
        cbi.pvArg0 = g_hProfileBuf;
        fRet = (BOOL)PerformCallBack(&cbi);
        ASSERT(fRet);

        g_hProfileBuf = NULL;
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
ProfilerFreeBuffer(void)
{
    DWORD dwMemSize;
    BOOL  fRet;
    
    if (g_pdwProfileBufStart) {
        dwMemSize = g_dwProfileBufNumPages * PAGE_SIZE;
        fRet = SC_UnlockPages(g_pdwProfileBufStart, dwMemSize);
        ASSERT(fRet);
    }
    
    DoFreeBuffer();

    return TRUE;
}


#define SECTION_SIZE (1 << VA_SECTION)

//------------------------------------------------------------------------------
// Lock a large block with iterative calls to LockPages
//------------------------------------------------------------------------------
BOOL 
ProfLockPages(
    LPVOID lpvAddress,
    DWORD cbSize,
    PDWORD pPFNs,
    int fOptions
    ) 
{
    BOOL fRet = FALSE;
    DWORD section;
    DWORD dwNumSections = (cbSize + SECTION_SIZE - 1) >> VA_SECTION;

    for (section = 0; section < dwNumSections; section++) {
        fRet = SC_LockPages(lpvAddress, cbSize < SECTION_SIZE ? cbSize : SECTION_SIZE, pPFNs, fOptions);
        if (fRet == FALSE) {
            RETAILMSG(1, (TEXT("ProfLockPages : Failed to lock @ 0x%08X\r\n"), lpvAddress));
            return FALSE;
        }
        cbSize -= SECTION_SIZE;
        lpvAddress = (LPVOID) ((DWORD) lpvAddress + SECTION_SIZE);
        pPFNs += PAGES_PER_SECTION;
    }
    return fRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
ProfilerAllocBuffer(void)
{
    DWORD dwMemRequest = 0;
    BOOL  fRet;
    DWORD i;
    CALLBACKINFO cbi;

    if (dwProfileBufferMax) {
        //
        // Buffer max was specified in BIB file via FIXUP_VAR
        //
        if (dwProfileBufferMax > PROFILE_BUFFER_MAX) {
            DEBUGMSG(1, (TEXT("dwProfileBufferMax = 0x%08X exceeds limit 0x%08X, fixing.\r\n"), 
                         dwProfileBufferMax, PROFILE_BUFFER_MAX));
            dwMemRequest = PROFILE_BUFFER_MAX;
        } else {
            dwMemRequest = dwProfileBufferMax;
        }
        
        if (dwMemRequest >= (DWORD) (UserKInfo[KINX_PAGEFREE] * PAGE_SIZE)) {
            // Try 3/4 of available RAM
            dwMemRequest = ((UserKInfo[KINX_PAGEFREE] * PAGE_SIZE * 3) / 4) & ~(PAGE_SIZE-1);
            DEBUGMSG(1, (TEXT("dwProfileBufferMax = 0x%08X, but only 0x%08X available, using 0x%08x instead.\r\n"), 
                         dwProfileBufferMax, (UserKInfo[KINX_PAGEFREE] * PAGE_SIZE),
                         dwMemRequest));
        }
    }
    
    if (dwMemRequest == 0) {
        //
        // Default to half the free RAM.
        //
        dwMemRequest = (UserKInfo[KINX_PAGEFREE] * PAGE_SIZE) / 2;
        //
        // Make an upper limit.
        //
        if (dwMemRequest > PROFILE_BUFFER_MAX) {
            dwMemRequest = PROFILE_BUFFER_MAX;
        }
    }
    g_dwProfileBufNumPages   = dwMemRequest / PAGE_SIZE;
    g_dwProfileBufNumEntries = g_dwProfileBufNumPages * ENTRIES_PER_PAGE;

    //
    // Create a shared memory-mapped object
    //
    
    // Need to use callbacks so that the kernel owns the map instead of the app
    
    // CreateFileMapping(INVALID_HANDLE_VALUE, ...);
    cbi.hProc = ProcArray[0].hProc;
    cbi.pfn = (FARPROC)CreateFileMapping;
    cbi.pvArg0 = INVALID_HANDLE_VALUE;
    g_hProfileBuf = (HANDLE)PerformCallBack(&cbi, NULL,
                                            PAGE_READWRITE | SEC_COMMIT, 0,
                                            dwMemRequest, NULL);
    if (!g_hProfileBuf) {
        goto EXIT_FALSE;
    }

    // MapViewOfFile(g_hProfileBuf, ...);
    cbi.pfn = (FARPROC)MapViewOfFile;
    cbi.pvArg0 = g_hProfileBuf;
    g_pdwProfileBufStart = (PDWORD)PerformCallBack(&cbi, FILE_MAP_ALL_ACCESS,
                                                   0, 0, 0);
    if (!g_pdwProfileBufStart) {
        DoFreeBuffer();
        goto EXIT_FALSE;
    }

    //
    // Lock the pages and get the physical addresses.
    //
    fRet = ProfLockPages(g_pdwProfileBufStart, dwMemRequest, (PDWORD) g_pPBE, LOCKFLAG_READ | LOCKFLAG_WRITE);

    if (fRet == FALSE) {
        DoFreeBuffer();
        goto EXIT_FALSE;
    }

    //
    // Convert the physical addresses to statically-mapped virtual addresses
    //
    for (i = 0; i < g_dwProfileBufNumPages; i++) {
        g_pPBE[i] = Phys2Virt((DWORD) g_pPBE[i]);
    }

    PROFILEMSG(1, (TEXT("ProfileStart() : Allocated %d kB for Profiler Buffer (0x%08X)\r\n"), dwMemRequest >> 10, g_pdwProfileBufStart));
    return TRUE;

EXIT_FALSE:
    PROFILEMSG(1, (TEXT("ProfileStart() : Error allocating buffer... defaulting to symbol lookup in ISR.\r\n")));
    return FALSE;
}


//------------------------------------------------------------------------------
//
// Takes a raw index and looks up the tiered entry pointer.
//
//------------------------------------------------------------------------------
static _inline PPROFBUFENTRY
GetEntryPointer(
    DWORD dwIndex
    )
{
    DWORD dwPage, dwEntry;

    dwPage  = dwIndex >> PROF_PAGE_SHIFT;
    dwEntry = dwIndex & PROF_ENTRY_MASK;

    return (&(g_pPBE[dwPage][dwEntry]));
}


/*
 *  ClearProfileHits - zero all profiler counters
 *
 */
void ClearProfileHits(void)
{
    unsigned int iMod;
    PROFentry   *profptr;
    SYMentry    *symptr;
    unsigned int iSym;

    g_ProfilerState_dwSamplesRecorded = 0;
    g_ProfilerState_dwSamplesDropped  = 0;
    g_ProfilerState_dwSamplesInIdle   = 0;

    // If no profile section just return
    if (!pTOC->ulProfileLen)
        return;
    
    profptr = (PROFentry *)pTOC->ulProfileOffset; // profptr-> first profile entry
    for (iMod = 0; iMod < g_dwNumROMModules; iMod++) {
        profptr->ulHits = 0;            // clear module hit count
        if (profptr->ulNumSym) {        // if there are symbols for this module
            // clear symbol hits
            iSym = profptr->ulNumSym;
            symptr = (SYMentry*)profptr->ulHitAddress;
            while (iSym) {
                symptr->ulFuncHits = 0;
                iSym--;
                symptr++;
            }
        }
        profptr++;
    }
}


//------------------------------------------------------------------------------
//  GetSymbol - searches symbol table for passed symbol number
//
//  Input:  lpSym - pointer to start of symbol buffer
//          dwSymNum - symbol number to lookup
//
//  Output: returns pointer to symbol
//------------------------------------------------------------------------------
static LPBYTE GetSymbol(LPBYTE lpSym, DWORD dwSymNum)
{
    while (dwSymNum > 0) {
        while (*lpSym++);
        dwSymNum--;
    }
    return lpSym;
}


// Used to sort symbol hits, copied from SYMentry but ulModuleIndex allows us to
// print faster
typedef struct SortSYMentry {
    ULONG   ulFuncAddress;          // function starting address
    ULONG   ulFuncHits;             // function hit counter
    ULONG   ulModuleIndex;          // index (wrt XIP 1) of module
} SortSYMentry;


//------------------------------------------------------------------------------
//  ProfilerReportHeader - display header for hit report
//------------------------------------------------------------------------------
void ProfilerReportHeader(void)
{
    DWORD dwTotalSamples, dwRecordedSamples, dwNonIdleSamples;
    ULONG ulPercent;

    // Simplify some logic by adding up the totals
    dwTotalSamples    = g_ProfilerState_dwSamplesRecorded + g_ProfilerState_dwSamplesDropped + g_ProfilerState_dwSamplesInIdle;
    dwRecordedSamples = g_ProfilerState_dwSamplesRecorded + g_ProfilerState_dwSamplesInIdle;
    dwNonIdleSamples  = g_ProfilerState_dwSamplesRecorded + g_ProfilerState_dwSamplesDropped;

    if (g_dwProfilerFlags & PROFILE_OBJCALL) {
        PROFILEMSG(1, (TEXT("=================== SYSTEM CALL HIT REPORT ===================\r\n")));
    } else {
        PROFILEMSG(1, (TEXT("=================== MONTE CARLO HIT REPORT ===================\r\n")));
    }

    PROFILEMSG(1, (TEXT("Total samples recorded = %8lu\r\n"), dwRecordedSamples));
    PROFILEMSG(1, (TEXT("Total samples dropped  = %8lu\r\n"), g_ProfilerState_dwSamplesDropped));
    PROFILEMSG(1, (TEXT("Ticks elapsed          = %8lu\r\n"), g_ProfilerState_dwTickCount));
    
    // Object call profiling has no other header info
    if (g_dwProfilerFlags & PROFILE_OBJCALL) {
        return;
    }

    // MONTE CARLO REPORT

    // Print idle report
    PROFILEMSG(1, (TEXT("IDLE TIME:\r\n")));
    PROFILEMSG(1, (TEXT("  * Total non-idle samples (recorded+dropped)        = %8lu\r\n"),
                   dwNonIdleSamples));
    PROFILEMSG(1, (TEXT("  * Total idle samples (recorded+dropped)            = %8lu\r\n"),
                   g_ProfilerState_dwSamplesInIdle));
    if (dwTotalSamples) {  // sanity check for division safety
        ulPercent = g_ProfilerState_dwSamplesInIdle;
        ulPercent *= 1000;
        ulPercent /= dwTotalSamples;
        PROFILEMSG(1, (TEXT("  * Percentage of samples in idle                    = %6lu.%1d\r\n"),
                       ulPercent / 10, ulPercent % 10));
    }

    // Print interrupt report if we have data
    if (g_ProfilerState_dwInterrupts) {
        PROFILEMSG(1, (TEXT("INTERRUPTS:\r\n")));
        PROFILEMSG(1, (TEXT("  * Total interrupts during sample period            = %8lu\r\n"),
                       g_ProfilerState_dwInterrupts));
        PROFILEMSG(1, (TEXT("  * Non-profiler interrupts                          = %8lu\r\n"),
                       g_ProfilerState_dwInterrupts - dwTotalSamples));
        if (g_ProfilerState_dwTickCount) {  // sanity check for division safety
            ulPercent = g_ProfilerState_dwInterrupts - dwTotalSamples;
            ulPercent *= 10;
            ulPercent /= g_ProfilerState_dwTickCount;
            PROFILEMSG(1, (TEXT("  * Non-profiler interrupts per tick                 = %6lu.%1d\r\n"),
                           ulPercent / 10, ulPercent % 10));
        }
    }

    // Print TLB report if we have data.  Only SH and MIPS software TLB miss 
    // handlers produce this data right now, but CPU perf counters could be
    // used for other architectures in the future.
    if ((g_ProfilerState_dwTLBCount) || (g_ProfilerState_dwProfilerIntsInTLB)) {
        PROFILEMSG(1, (TEXT("TLB MISSES:\r\n")));
        
        PROFILEMSG(1, (TEXT("  * Total TLB misses during sample period            = %8lu\r\n"),
                       g_ProfilerState_dwTLBCount));
        if (g_ProfilerState_dwTickCount) {  // sanity check for division safety
            ulPercent = g_ProfilerState_dwTLBCount;
            ulPercent *= 10;
            ulPercent /= g_ProfilerState_dwTickCount;
            PROFILEMSG(1, (TEXT("  * TLB misses per tick                              = %6lu.%1d\r\n"),
                           ulPercent / 10, ulPercent % 10));
        }

        // Only display profiler hits in TLB miss handler if we got any.  If
        // this data can't be gathered on this processor, we shouldn't
        // erroneously report that there were no hits during TLB miss handling.
        if (g_ProfilerState_dwProfilerIntsInTLB) {
            PROFILEMSG(1, (TEXT("  * Profiler samples during TLB miss handler         = %8lu\r\n"),
                           g_ProfilerState_dwProfilerIntsInTLB));
            if (dwTotalSamples) {  // sanity check for division safety
                ulPercent = g_ProfilerState_dwProfilerIntsInTLB;
                ulPercent *= 1000;
                ulPercent /= dwTotalSamples;
                PROFILEMSG(1, (TEXT("  * Percentage of samples in TLB miss handler        = %6lu.%1d\r\n"),
                               ulPercent / 10, ulPercent % 10));
            }
        }
    }
}


//------------------------------------------------------------------------------
//  ProfilerReport - display hit report
//------------------------------------------------------------------------------
void ProfilerReport(void)
{
    ROMChain_t *pROM;                    // Chain of ROM XIP regions
    DWORD      loop, loop2;              // index
    PROFentry *pProf;                    // profile section pointer
    DWORD      dwModCount, dwSymCount, dwCount;  // number of symbols hit
    ULONG      ulPercent;                // hit percentage
    DWORD      dwModIndex, dwNumModules; // number of modules in ROM
    SYMentry  *pSym;                     // symbol address/hit pointer
    SortSYMentry *pHits = NULL;          // Sorted list of function hits
    SortSYMentry  symTemp;               // sorting temp
    TOCentry  *tocptr;
    DWORD      dwHits[NUM_MODULES], dwMods[NUM_MODULES], dwTemp;  // Sorted list of module hits
    PPROFBUFENTRY pEntry;
    DWORD      dwTotalSamples, dwRecordedSamples;

    #define IDLE_SIGNATURE ((DWORD)-1)
    
    // Simplify some logic by adding up the totals
    dwTotalSamples    = g_ProfilerState_dwSamplesRecorded + g_ProfilerState_dwSamplesDropped + g_ProfilerState_dwSamplesInIdle;
    dwRecordedSamples = g_ProfilerState_dwSamplesRecorded + g_ProfilerState_dwSamplesInIdle;
    
    if (!dwTotalSamples) {
        PROFILEMSG(1, (TEXT("Kernel Profiler: No hits recorded.  Make sure profiling\r\n")));
        PROFILEMSG(1, (TEXT("is implemented in the OAL, and the profiler was started.\r\n")));
        goto profile_exit;
    }

    // If profiling to buffer, lookup symbols now
    if (g_dwProfilerFlags & PROFILE_BUFFER) {
        PROFILEMSG(1, (TEXT("Kernel Profiler: Looking up symbols for %u hits.\r\n"), g_ProfilerState_dwSamplesRecorded));
        for (loop = 0; loop < g_ProfilerState_dwSamplesRecorded; loop++) {
            if (loop % 10000 == 0)    // display a . every 10000 hits, so user knows we are working
                PROFILEMSG(1, (TEXT(".")));

            pEntry = GetEntryPointer(loop);
            ProfilerSymbolHit(pEntry->ra, pEntry->pte);
        }
        PROFILEMSG(1, (TEXT("\r\n")));
    }


    //
    // Report header contains lots of interesting goodies
    //
    
    ProfilerReportHeader();

    // Don't print anything else if the only recorded samples were in idle.
    if (!g_ProfilerState_dwSamplesRecorded) {
        return;
    }
    
    
    //
    // Display hits by module and count number of symbols hit
    //

    dwModCount = 0;
    dwSymCount = 0;
    
    // Insert the modules with nonzero hits into the list
    dwModIndex = 0;
    pROM = ROMChain;
    pProf = (PROFentry *)/*pROM->*/pTOC->ulProfileOffset;  // All ROMs point to same profiler table
    while (pROM) {
        dwNumModules = pROM->pTOC->nummods;
        for (loop = 0; loop < dwNumModules; loop++) {
            if (pProf->ulHits) {
                if (dwModCount < NUM_MODULES-1) {  // -1 to leave room for idle
                    dwHits[dwModCount] = pProf->ulHits;
                    dwMods[dwModCount] = dwModIndex + loop;  // index wrt first XIP region
                    dwModCount++;
                } else {
                    PROFILEMSG(ZONE_UNACCOUNTED, (TEXT("Module %u dropped, not all modules with hits will be printed!\r\n"), dwModIndex + loop));
                }
            }
            pProf++;
        }
        dwModIndex += dwNumModules;

        pROM = pROM->pNext;
    }
    // dwModCount is now the count of modules with nonzero hits

    // Count idle as a "module"
    if (g_ProfilerState_dwSamplesInIdle) {
        dwHits[dwModCount] = g_ProfilerState_dwSamplesInIdle;
        dwMods[dwModCount] = IDLE_SIGNATURE;
        dwModCount++;
    }

    // Sort the list into decreasing order (bubble sort)
    for (loop = 1; loop < dwModCount; loop++) {
        for (loop2 = dwModCount-1; loop2 >= loop; loop2--) {
            if (dwHits[loop2-1] < dwHits[loop2]) {
                dwTemp = dwHits[loop2-1];
                dwHits[loop2-1] = dwHits[loop2];
                dwHits[loop2] = dwTemp;
                dwTemp = dwMods[loop2-1];
                dwMods[loop2-1] = dwMods[loop2];
                dwMods[loop2] = dwTemp;
            }
        }
    }

    PROFILEMSG(1, (TEXT("\r\nMODULES:  (Does not include dropped samples!)\r\n\r\n")));
    PROFILEMSG(1, (TEXT("Module        Hits        Percent\r\n")));
    PROFILEMSG(1, (TEXT("------------  ----------  -------\r\n")));
    
    // Print the sorted list
    dwModIndex = 0; // index wrt current XIP region
    dwCount = 0;  // # of symbol hits that were successfully attributed to a module
    for (loop = 0; loop < dwModCount; loop++) {
        dwModIndex = dwMods[loop];
        if (dwModIndex != IDLE_SIGNATURE) {
            // Figure out which XIP region the module was in
            pROM = ROMChain;
            while ((dwModIndex >= pROM->pTOC->nummods) && pROM) {
                dwModIndex -= pROM->pTOC->nummods;
                pROM = pROM->pNext;
            }
            DEBUGCHK(pROM);
            if (pROM) {
                tocptr = ((TOCentry *)(pROM->pTOC+1)) + dwModIndex;
                pProf = ((PROFentry *)/*pROM->*/pTOC->ulProfileOffset) + dwMods[loop];  // All ROMs point to same profiler table
                DEBUGCHK(pProf->ulHits && (pProf->ulHits == dwHits[loop]));
                if (pProf->ulHits) {
                    // Display module name, hits, percentage
                    ulPercent = pProf->ulHits;
                    ulPercent *= 1000;
                    ulPercent /= dwRecordedSamples;
                    PROFILEMSG(1, (TEXT("%-12a  %10lu  %5lu.%1d\r\n"),
                                   tocptr->lpszFileName, pProf->ulHits, ulPercent / 10, ulPercent % 10));
                    dwCount += pProf->ulHits;
                    pSym = (SYMentry *)pProf->ulHitAddress;
    
                    // While we're here walking the TOC, count the nonzero symbols
                    for (loop2 = 0; loop2 < pProf->ulNumSym; loop2++) {
                        if (pSym->ulFuncHits) {
                            dwSymCount++;
                        }
                        pSym++;
                    }
                }
            }
        } else {
            // False "module" representing idle
            ulPercent = dwHits[loop];
            ulPercent *= 1000;
            ulPercent /= dwRecordedSamples;
            PROFILEMSG(1, (TEXT("IDLE          %10lu  %5lu.%1d\r\n"),
                           dwHits[loop], ulPercent / 10, ulPercent % 10));
        }
    }
    // dwSymCount is now the count of symbols with nonzero hits
    
    



    if (g_ProfilerState_dwSamplesRecorded - dwCount) {
        ulPercent = g_ProfilerState_dwSamplesRecorded - dwCount;
        ulPercent *= 1000;
        ulPercent /= dwRecordedSamples;
        PROFILEMSG(1, (TEXT("%-12a  %10lu  %5lu.%1d\r\n"),
                   "UNKNOWN", g_ProfilerState_dwSamplesRecorded - dwCount, ulPercent/10, ulPercent%10));
    }
    
    
    //
    // Display hits by symbol
    //
    
    if (!dwSymCount) {
        PROFILEMSG(1, (TEXT("No symbols found.\r\n")));
        goto profile_exit;
    }

    // Allocate memory for sorting
    pHits = (SortSYMentry *)VirtualAlloc(NULL, (dwSymCount+1)*sizeof(SortSYMentry),  // +1 to hold idle count
                                         MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (pHits == NULL) {
        PROFILEMSG(1, (TEXT("ProfileStop: Sort memory allocation size %u failed.\r\n"), (dwSymCount+1)*sizeof(SYMentry)));
        goto profile_exit;
    }
    
    
    // Insert the symbols with nonzero hits into the list
    dwCount = dwSymCount;  // Temp holder to make sure we don't exceed this number
    dwSymCount = 0;
    dwModIndex = 0;
    pROM = ROMChain;
    pProf = (PROFentry *)/*pROM->*/pTOC->ulProfileOffset;  // All ROMs point to same profiler table
    while (pROM) {
        dwNumModules = pROM->pTOC->nummods;
        for (loop = 0; loop < dwNumModules; loop++) {
            if (pProf->ulHits) {
                pSym = (SYMentry *)pProf->ulHitAddress;
                for (loop2 = 0; loop2 < pProf->ulNumSym; loop2++) {
                    if (pSym->ulFuncHits && (dwSymCount < dwCount)) {
                        pHits[dwSymCount].ulFuncAddress = pSym->ulFuncAddress;
                        pHits[dwSymCount].ulFuncHits = pSym->ulFuncHits;
                        pHits[dwSymCount].ulModuleIndex = dwModIndex + loop;  // index wrt first XIP region
                        dwSymCount++;
                    }
                    pSym++;
                }
            }
            pProf++;
        }
        dwModIndex += dwNumModules;

        pROM = pROM->pNext;
    }
    // dwSymCount is now the count of symbols with nonzero hits

    // Count idle as a "symbol"
    if (g_ProfilerState_dwSamplesInIdle) {
        pHits[dwSymCount].ulFuncAddress = 0;
        pHits[dwSymCount].ulFuncHits = g_ProfilerState_dwSamplesInIdle;
        pHits[dwSymCount].ulModuleIndex = IDLE_SIGNATURE;
        dwSymCount++;
    }

    // Sort the list into decreasing order (bubble sort)
    for (loop = 1; loop < dwSymCount; loop++) {
        for (loop2 = dwSymCount-1; loop2 >= loop; loop2--) {
            if ((unsigned int)pHits[loop2-1].ulFuncHits < (unsigned int)pHits[loop2].ulFuncHits) {
                symTemp.ulFuncHits    = pHits[loop2-1].ulFuncHits;
                symTemp.ulFuncAddress = pHits[loop2-1].ulFuncAddress;
                symTemp.ulModuleIndex = pHits[loop2-1].ulModuleIndex;
                pHits[loop2-1].ulFuncHits    = pHits[loop2].ulFuncHits;
                pHits[loop2-1].ulFuncAddress = pHits[loop2].ulFuncAddress;
                pHits[loop2-1].ulModuleIndex = pHits[loop2].ulModuleIndex;
                pHits[loop2].ulFuncHits    = symTemp.ulFuncHits;
                pHits[loop2].ulFuncAddress = symTemp.ulFuncAddress;
                pHits[loop2].ulModuleIndex = symTemp.ulModuleIndex;
            }
        }
    }

    PROFILEMSG(1, (TEXT("\r\nSYMBOLS:  (Does not include dropped samples!)\r\n\r\n")));
    PROFILEMSG(1, (TEXT("Hits       Percent Address  Module       Routine\r\n")));
    PROFILEMSG(1, (TEXT("---------- ------- -------- ------------:---------------------\r\n")));
    
    // Print the sorted list
    dwCount = 0;  // # of hits that were successfully attributed to a symbol
    for (loop = 0; loop < dwSymCount; loop++) {
        dwModIndex = pHits[loop].ulModuleIndex;
        if (dwModIndex != IDLE_SIGNATURE) {
            // Figure out which XIP region and module the symbol was in
            pROM = ROMChain;
            while ((dwModIndex >= pROM->pTOC->nummods) && pROM) {
                dwModIndex -= pROM->pTOC->nummods;
                pROM = pROM->pNext;
            }
            DEBUGCHK(pROM);
            if (pROM) {
                pProf = (PROFentry *)/*pROM->*/pTOC->ulProfileOffset + pHits[loop].ulModuleIndex;  // All ROMs point to same profiler table
                tocptr = (TOCentry *)(pROM->pTOC+1) + dwModIndex;
                DEBUGCHK(pProf->ulHits);

                // Find profile entry for this symbol
                pSym = (SYMentry *)pProf->ulHitAddress;
                for (loop2 = 0; loop2 < pProf->ulNumSym; loop2++) {
                    if ((pSym->ulFuncAddress == pHits[loop].ulFuncAddress)
                        && (pSym->ulFuncHits == pHits[loop].ulFuncHits)) {

                        ulPercent = pHits[loop].ulFuncHits;
                        ulPercent *= 1000;
                        ulPercent /= dwRecordedSamples;
                        PROFILEMSG(1, (TEXT("%10d %5d.%1d %8.8lx %-12a:%a\r\n"), pHits[loop].ulFuncHits,
                                   ulPercent / 10, ulPercent % 10, pHits[loop].ulFuncAddress, tocptr->lpszFileName,
                                   GetSymbol((LPBYTE)pProf->ulSymAddress, loop2)));
                        dwCount += pHits[loop].ulFuncHits;
                        goto next_sym;
                    }
                    pSym++;
                }
            }
        next_sym:
            ;
        } else {
            // False "symbol" representing idle
            ulPercent = pHits[loop].ulFuncHits;
            ulPercent *= 1000;
            ulPercent /= dwRecordedSamples;
            PROFILEMSG(1, (TEXT("%10d %5d.%1d -------- ------------:IDLE\r\n"),
                           pHits[loop].ulFuncHits, ulPercent / 10, ulPercent % 10));
        }
    }

    // Print hits in unlisted symbols.  These unlisted hits are hits in modules
    // that are not in ROM.
    if (dwCount != g_ProfilerState_dwSamplesRecorded) {
        ulPercent = g_ProfilerState_dwSamplesRecorded - dwCount;
        ulPercent *= 1000;
        ulPercent /= dwRecordedSamples;
        PROFILEMSG(1, (TEXT("%10d %5d.%1d                      :<UNACCOUNTED FOR>\r\n"),
                       g_ProfilerState_dwSamplesRecorded - dwCount, ulPercent / 10, ulPercent % 10));
    }

profile_exit:
    if (pHits)
        VirtualFree(pHits, 0, MEM_DECOMMIT | MEM_FREE);
    return;
}


//------------------------------------------------------------------------------
// This is a dummy function for the profiler
//------------------------------------------------------------------------------
void IDLE_STATE()
{
    //
    // Force the function to be non-empty so the hit engine can find it.
    //
    static volatile DWORD dwVal;
    dwVal++;
}


#if defined(x86)  // Callstack capture is only supported on x86 right now

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Called by ProfilerHitEx when the profiler is using PROFILE_CELOG and
// PROFILE_CALLSTACK.  Logs the call stack of the interrupted thread to CeLog.
static BOOL LogCallStack()
{
    DWORD dwLastError;
    DWORD dwNumFrames, dwSkip, dwFlags;
    
    // NKGetThreadCallStack may overwrite thread LastError
    dwLastError = KGetLastError(pCurThread);
    KSetLastError(pCurThread, 0);
    
    // Get context for the thread that was interrupted
    g_ProfilerContext.Ebp = pCurThread->ctx.TcxEbp;
    g_ProfilerContext.Eip = pCurThread->ctx.TcxEip;

    if (g_dwProfilerFlags & PROFILE_CALLSTACK_INPROC) {
        dwFlags = (STACKSNAP_INPROC_ONLY | STACKSNAP_FAIL_IF_INCOMPLETE);
    } else {
        dwFlags = STACKSNAP_FAIL_IF_INCOMPLETE;
    }
    
    // Iterate if the buffer is not large enough to hold the whole stack
    dwSkip = 0;
    do {
        // Use the context to get the callstack
        dwNumFrames = NKGetThreadCallStack(pCurThread, PROFILER_MAX_STACK_FRAME,
                                           g_ProfilerStackBuffer, dwFlags,
                                           dwSkip, &g_ProfilerContext);

        // Log the callstack to CeLog
        if (dwNumFrames) {
            g_pfnCeLogData(FALSE, CELID_CALLSTACK, g_ProfilerStackBuffer,
                           dwNumFrames * sizeof(CallSnapshot),
                           0, CELZONE_PROFILER, 0, FALSE);
            dwSkip += dwNumFrames;
        }

    } while ((dwNumFrames == PROFILER_MAX_STACK_FRAME)
             && (KGetLastError(pCurThread) == ERROR_INSUFFICIENT_BUFFER));

    // Restore thread LastError
    KSetLastError(pCurThread, dwLastError);

    return TRUE;
}
#endif  // defined(x86)


#endif  // NKPROF


//------------------------------------------------------------------------------
//  GetEPC - Get exception program counter
//
//  Output: returns EPC, zero if cpu not supported
//------------------------------------------------------------------------------
DWORD GetEPC(void) {
#if defined(x86) && defined(NKPROF)
    extern PTHREAD pthFakeStruct;
    PTHREAD pTh;

    if (KData.cNest < 0) {
        pTh = pthFakeStruct;
    } else {
        pTh = pCurThread;
    }
    return(GetThreadIP(pTh));

#elif defined(ARM)

    // -- NOT SUPPORTED ON ARM --
    // The address is not saved to the thread context on ARM, so GetEPC
    // cannot be used within an ISR.  Instead use the "ra" value that is
    // passed to OEMInterruptHandler.
    return 0;
 
#else

    return(GetThreadIP(pCurThread));

#endif
}


//------------------------------------------------------------------------------
// ProfilerHit - OEMProfilerISR calls this routine to record profile hit
// 
// Input:  ra - interrupt return address
// 
// if buffer profiling, save ra and pCurProc->oe.tocptr in buffer
// else do symbol lookup in ISR
//------------------------------------------------------------------------------
void 
ProfilerHit(
    unsigned int ra
    ) 
{
#ifdef NKPROF
    OEMProfilerData data;

    data.ra = ra;
    data.dwBufSize = 0;

    

    ProfilerHitEx(&data);
#endif // NKPROF
}


//------------------------------------------------------------------------------
// ProfilerHitEx - Generalized version of ProfilerHit.  A hardware-specific
// profiler ISR calls this routine to record a buffer of profiling data.
// No lookups done during this routine -- just copy the data into a buffer and
// return.
// Input:  pData - OEM-specified buffer of profiling information (RA may be fixed up)
//------------------------------------------------------------------------------
void 
ProfilerHitEx(
    OEMProfilerData *pData
    ) 
{
#ifdef NKPROF

    if (!RunList.pth && !RunList.pRunnable) {  // IDLE
        // CeLog should record idle hits, but other modes should just
        // increment a counter and exit.
        g_ProfilerState_dwSamplesInIdle++;
        if (!(g_dwProfilerFlags & PROFILE_CELOG)) {
            return;
        }
        pData->ra = (DWORD) IDLE_STATE;
    }
    
    if (g_dwProfilerFlags & PROFILE_CELOG) {
        if (IsCeLogStatus(CELOGSTATUS_ENABLED_PROFILE)) {
            // Fixup RA
            if (IsKernelVa(pData->ra)) {     // if high bit set, this is NK
                pData->ra &= 0xdfffffff;     // mask off uncached bit
            } else {
                pData->ra = (DWORD)MapPtrProc(pData->ra, pCurProc);
            }

            // Send data to celog
            if (g_dwProfilerFlags & PROFILE_OEMDEFINED) {
                g_pfnCeLogData(FALSE, CELID_OEMPROFILER_HIT, pData,
                               pData->dwBufSize + 2*sizeof(DWORD),  // dwBufSize does not include header; can't use sizeof(OEMProfilerData) because the 0-byte array has nonzero size
                               0, CELZONE_PROFILER, 0, FALSE);
            } else {
                g_pfnCeLogData(FALSE, CELID_MONTECARLO_HIT, &(pData->ra), sizeof(DWORD), 0,
                               CELZONE_PROFILER, 0, FALSE);
            }
        
#if defined(x86)  // Callstack capture is only supported on x86 right now
            // Send call stack data if necessary
            if ((g_dwProfilerFlags & PROFILE_CALLSTACK)
                && (RunList.pth || RunList.pRunnable)   // Not IDLE_STATE - idle has no stack
                && !(KData.cNest < 0)) {                // Not a nested interrupt or KCall - no way to get stack in those cases?
                LogCallStack();
            }
#endif
        }
        
    } else if (g_dwProfilerFlags & (PROFILE_BUFFER)) {
        //
        // If profiling to buffer and there is still room in the buffer
        //
        if (g_ProfilerState_dwSamplesRecorded < g_dwProfileBufNumEntries) {
            PPROFBUFENTRY pEntry;

            if (IsKernelVa(pData->ra))       // if high bit set, this is NK
                pData->ra &= 0xdfffffff;     // mask off uncached bit
  
            pEntry = GetEntryPointer(g_ProfilerState_dwSamplesRecorded);
            g_ProfilerState_dwSamplesRecorded++;
            //
            // Record an entry point
            //
            pEntry->ra = pData->ra;
            pEntry->pte = pCurProc->oe.tocptr;
        } else {
            //
            // No place to record this hit. Let's remember how many we dropped.
            //
            g_ProfilerState_dwSamplesDropped++;
            PROFILEMSG(ZONE_UNACCOUNTED, (TEXT("Hit dropped (buffer full), ra=0x%08x\r\n"), pData->ra));
        }

    } else {
        // No buffer. Just lookup the symbol now.
        //
        g_ProfilerState_dwSamplesRecorded++;
        ProfilerSymbolHit(pData->ra, pCurProc->oe.tocptr);
    }

#endif // NKPROF
}


//------------------------------------------------------------------------------
// SC_ProfileSyscall - Turns profiler on and off.  The workhorse behind
// ProfileStart(), ProfileStartEx(), ProfileStop() and ProfileCaptureStatus().
//------------------------------------------------------------------------------
void 
SC_ProfileSyscall(
    DWORD* lpdwArg
    )
{
#ifdef NKPROF
    static int   scPauseContinueCalls = 0;
    static BOOL  bStart = FALSE;
    static DWORD dwCeLogZoneUser, dwCeLogZoneCE, dwCeLogZoneProcess;
    const ProfilerControl* pControl = (const ProfilerControl*) lpdwArg;

    if (pControl
        && (pControl->dwVersion == 1)
        && (pControl->dwReserved == 0)) {

        if (pControl->dwOptions & PROFILE_START) {
            // ProfileStart() / ProfileStartEx()

            if (pControl->dwOptions & PROFILE_CONTINUE) {
                if (bStart && ((g_dwProfilerFlags & (PROFILE_OBJCALL | PROFILE_KCALL)) == 0)) {
                    ++scPauseContinueCalls;
                    // Start profiler timer on 0 to 1 transition
                    if (1 == scPauseContinueCalls) {
                        if (g_dwProfilerFlags & PROFILE_OEMDEFINED) {
                            // OEM-specific profiler
                            OEMIoControl(IOCTL_HAL_OEM_PROFILER,
                                         (ProfilerControl*) pControl,  // non-const
                                         sizeof(ProfilerControl) + pControl->OEM.dwControlSize,
                                         NULL, 0, NULL);

                        } else {
                            // Monte Carlo profiling
                            
                            // Set system state counters to running (total) values
                            g_ProfilerState_dwTLBCount  = dwCeLogTLBMiss - g_ProfilerState_dwTLBCount;
                            g_ProfilerState_dwTickCount = GetTickCount() - g_ProfilerState_dwTickCount;

                            OEMProfileTimerEnable(pControl->Kernel.dwUSecInterval);
                            bProfileTimerRunning = TRUE;
                        }
                    }
                }
            
            } else if (pControl->dwOptions & PROFILE_PAUSE) {
                if (bStart && ((g_dwProfilerFlags & (PROFILE_OBJCALL | PROFILE_KCALL)) == 0)) {
                    --scPauseContinueCalls;
                    // Stop profiler timer on 1 to 0 transition
                    if (!scPauseContinueCalls) {
                        if (g_dwProfilerFlags & PROFILE_OEMDEFINED) {
                            // OEM-specific profiler
                            OEMIoControl(IOCTL_HAL_OEM_PROFILER,
                                         (ProfilerControl*) pControl,  // non-const
                                         sizeof(ProfilerControl) + pControl->OEM.dwControlSize,
                                         NULL, 0, NULL);
                        } else {
                            // Monte Carlo profiling
                            OEMProfileTimerDisable();
                            bProfileTimerRunning = FALSE;

                            // Set system state counters to paused (elapsed) values
                            g_ProfilerState_dwTLBCount  = dwCeLogTLBMiss - g_ProfilerState_dwTLBCount;
                            g_ProfilerState_dwTickCount = GetTickCount() - g_ProfilerState_dwTickCount;
                        }
                    }
                }
            
            } else {
                // Protect against multiple starts
                if (bStart) {
                    PROFILEMSG(1, (TEXT("Kernel Profiler: Ignoring multiple profiler starts\r\n")));
                    return;
                }
                
                // Protect against running a profiling kernel on an unprofiled image
                if (((pTOC->ulProfileLen == 0) || (pTOC->ulProfileOffset == 0))
                    && ((pControl->dwOptions & PROFILE_CELOG) == 0)) { // Allow profiling with celog even if no symbols
                    PROFILEMSG(1, (TEXT("Kernel Profiler: Unable to start, PROFILE=OFF in config.bib!\r\n")));
                    return;
                }
                
                // Pre-compute information about all of the XIP regions for
                // faster access while profiling
                if ((g_dwNumROMModules == 0) || (g_dwFirstROMDLL == 0)) {
                    ROMChain_t *pROM = ROMChain;
                    g_dwFirstROMDLL = (DWORD)-1;
                    while (pROM) {
                        // Figure out the module index (wrt first XIP region) of nk.exe
                        // NK is the first module of the pTOC
                        if (pROM->pTOC == pTOC) {
                            g_dwNKIndex = g_dwNumROMModules;
                        }
                        
                        // Count the total number of modules
                        g_dwNumROMModules += pROM->pTOC->nummods;
                        
                        // Find the lowest ROM DLL address
                        if (pROM->pTOC->dllfirst < g_dwFirstROMDLL) {
                            g_dwFirstROMDLL = pROM->pTOC->dllfirst;
                        }
                        pROM = pROM->pNext;
                    }
                    DEBUGCHK(g_dwFirstROMDLL != (DWORD)-1);
                }

                if ((pControl->dwOptions & PROFILE_CELOG) && !IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
                    PROFILEMSG(1, (TEXT("Kernel Profiler: Unable to start, CeLog is not loaded!\r\n")));
                    return;
                }

                // Debug output so the user knows exactly what's going on
                if (pControl->dwOptions & PROFILE_KCALL) {
                    PROFILEMSG(1, (TEXT("Kernel Profiler: Gathering KCall data\r\n")));
                } else {
                    PROFILEMSG(1, (TEXT("Kernel Profiler: Gathering %s data in %s mode\r\n"),
                                   (pControl->dwOptions & PROFILE_OEMDEFINED) ? TEXT("OEM-Defined")
                                       : (pControl->dwOptions & PROFILE_OBJCALL) ? TEXT("ObjectCall")
                                       : TEXT("MonteCarlo"),
                                   (pControl->dwOptions & PROFILE_CELOG) ? TEXT("CeLog")
                                       : ((pControl->dwOptions & PROFILE_BUFFER) ? TEXT("buffered")
                                       : TEXT("unbuffered"))));
                }
                
                
                OEMProfileTimerDisable();   // disable profiler timer
                bProfileTimerRunning = FALSE;
                bStart = TRUE;
                ++scPauseContinueCalls;

                ClearProfileHits();         // reset all profile counts
                
                g_dwProfilerFlags = 0;

                //
                // Determine the storage mode for the data
                //
                if (pControl->dwOptions & PROFILE_CELOG) {

                    



                    
                    // Disable CeLog general logging; log only profile events
                    CeLogEnableStatus(CELOGSTATUS_ENABLED_PROFILE);

                    // Make sure the correct zones are turned on; save the
                    // old zone settings to restore later
                    SC_CeLogGetZones(&dwCeLogZoneUser, &dwCeLogZoneCE,
                                     &dwCeLogZoneProcess, NULL);
                    SC_CeLogSetZones(0xFFFFFFFF, CELZONE_PROFILER, 0xFFFFFFFF);
                    SC_CeLogReSync();

                    // Log the start event
                    if (pControl->dwOptions & PROFILE_OEMDEFINED) {
                        g_pfnCeLogData(TRUE, CELID_PROFILER_START, pControl,
                                       sizeof(ProfilerControl) + pControl->OEM.dwControlSize, 0,
                                       CELZONE_PROFILER, 0, FALSE);
                    } else {
                        g_pfnCeLogData(TRUE, CELID_PROFILER_START, pControl,
                                       sizeof(ProfilerControl), 0,
                                       CELZONE_PROFILER, 0, FALSE);
                    }

                    g_dwProfilerFlags |= PROFILE_CELOG;

#if defined(x86)  // Callstack capture is only supported on x86 right now
                    // CALLSTACK flag can only be used with CeLog & Monte Carlo right now
                    if ((pControl->dwOptions & PROFILE_CALLSTACK)
                        && !(pControl->dwOptions & (PROFILE_OBJCALL | PROFILE_KCALL))) {

                        g_dwProfilerFlags |= PROFILE_CALLSTACK;

                        // INPROC flag modifies the callstack flag
                        if (pControl->dwOptions & PROFILE_CALLSTACK_INPROC) {
                            g_dwProfilerFlags |= PROFILE_CALLSTACK_INPROC;
                        }
                    }
#endif  // defined(x86)

                } else if (pControl->dwOptions & PROFILE_BUFFER) {
                    // Attempt to alloc the buffer; skip buffering if the alloc fails
                    if (ProfilerAllocBuffer()) {
                        g_dwProfilerFlags |= PROFILE_BUFFER;
                    }
                }
                
                


                // Determine which type of data is being gathered
                if (pControl->dwOptions & PROFILE_OEMDEFINED) {
                    // OEM-specific profiler
                    if ((pControl->dwOptions & (PROFILE_OBJCALL | PROFILE_KCALL))
                        || !OEMIoControl(IOCTL_HAL_OEM_PROFILER,
                                         (ProfilerControl*) pControl,  // non-const
                                         sizeof(ProfilerControl) + pControl->OEM.dwControlSize,
                                         NULL, 0, NULL)) {
                        PROFILEMSG(1, (TEXT("Kernel Profiler: OEM Profiler start failed!\r\n")));
                        SetLastError(ERROR_NOT_SUPPORTED);
                        // Restore CeLog state
                        if (IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
                            SC_CeLogSetZones(dwCeLogZoneUser, dwCeLogZoneCE, dwCeLogZoneProcess);
                            CeLogEnableStatus(CELOGSTATUS_ENABLED_GENERAL);
                        }
                        g_dwProfilerFlags = 0;
                        bStart = FALSE;
                        return;
                    }
                    g_dwProfilerFlags |= PROFILE_OEMDEFINED;

                } else if (pControl->dwOptions & PROFILE_OBJCALL) {
                    // Object call profiling
                    g_dwProfilerFlags |= PROFILE_OBJCALL;
                
                } else if (pControl->dwOptions & PROFILE_KCALL) {
                    // Kernel call profiling
                    g_dwProfilerFlags |= PROFILE_KCALL;
                
                } else {
                    // Monte Carlo profiling
                    
                    // Initialize the system state counters that will not run
                    // whenever profiling is paused
                    g_ProfilerState_dwInterrupts = 0;
                    g_ProfilerState_dwProfilerIntsInTLB = 0;

                    if (pControl->dwOptions & PROFILE_STARTPAUSED) {
                        --scPauseContinueCalls;
                        
                        // Set elapsed values for system counters that will
                        // continue to run whenever profiling is paused
                        // (paused now)
                        g_ProfilerState_dwTLBCount  = 0;
                        g_ProfilerState_dwTickCount = 0;
                    } else {
                        // Set start values for system counters that will
                        // continue to run whenever profiling is paused
                        // (running now)
                        g_ProfilerState_dwTLBCount  = dwCeLogTLBMiss;
                        g_ProfilerState_dwTickCount = GetTickCount();
                        
                        // Start profiler timer
                        OEMProfileTimerEnable(pControl->Kernel.dwUSecInterval);
                        bProfileTimerRunning = TRUE;
                    }
                }
            }
        
        } else if (pControl->dwOptions & PROFILE_STOP) {
            // ProfileStop()
            
            if (bProfileTimerRunning) {
                OEMProfileTimerDisable();   // disable profiler timer
                bProfileTimerRunning = FALSE;
                
                // Set system state counters to paused (elapsed) values
                g_ProfilerState_dwTLBCount  = dwCeLogTLBMiss - g_ProfilerState_dwTLBCount;
                g_ProfilerState_dwTickCount = GetTickCount() - g_ProfilerState_dwTickCount;
                
            } else if (g_dwProfilerFlags & PROFILE_OEMDEFINED) {
                // OEM-specific profiler
                ProfilerControl control;
                control.dwVersion = 1;
                control.dwOptions = PROFILE_STOP | PROFILE_OEMDEFINED;
                control.dwReserved = 0;
                control.OEM.dwControlSize = 0;    // no OEM struct passed to stop profiler
                control.OEM.dwProcessorType = 0;  
                OEMIoControl(IOCTL_HAL_OEM_PROFILER, &control,
                             sizeof(ProfilerControl), NULL, 0, NULL);
                g_dwProfilerFlags &= ~PROFILE_OEMDEFINED;
            }

            scPauseContinueCalls = 0;
            
            if (g_dwProfilerFlags & PROFILE_CELOG) {
                // Resume general logging
                if (IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
                    // Log the stop event
                    g_pfnCeLogData(TRUE, CELID_PROFILER_STOP, NULL, 0, 0,
                                   CELZONE_PROFILER, 0, FALSE);
                    
                    // Restore the original zone settings
                    SC_CeLogSetZones(dwCeLogZoneUser, dwCeLogZoneCE, dwCeLogZoneProcess);

                    // Restore CeLog general logging
                    CeLogEnableStatus(CELOGSTATUS_ENABLED_GENERAL);
                }
                g_dwProfilerFlags &= ~(PROFILE_CELOG | PROFILE_CALLSTACK | PROFILE_CALLSTACK_INPROC);
                
                // The Monte Carlo report header can still be printed, though
                // the full report must be generated by desktop tools that parse
                // the log
                PROFILEMSG(1, (TEXT("Profiler data written to CeLog.\r\n")));
                if (!(g_dwProfilerFlags & PROFILE_OBJCALL)) {
                    ProfilerReportHeader();
                    PROFILEMSG(1, (TEXT("\r\nMODULES:  Written to CeLog\r\n")));
                    PROFILEMSG(1, (TEXT("\r\nSYMBOLS:  Written to CeLog\r\n")));
                }
            
            } else if (g_dwProfilerFlags & PROFILE_KCALL) {
                // Dump the KCall profile data
                SC_DumpKCallProfile(TRUE);
                g_dwProfilerFlags &= ~PROFILE_KCALL;
            
            } else {
                // Display profile hit report
                ProfilerReport();
            }
        
            if (g_dwProfilerFlags & PROFILE_BUFFER) {
                ProfilerFreeBuffer();
                g_dwProfilerFlags &= ~PROFILE_BUFFER;
            }

            g_dwProfilerFlags = 0;
            bStart = FALSE;
        
        } else if (pControl->dwOptions & PROFILE_OEM_QUERY) {
            // ProfileCaptureStatus()
            // Insert the current state of the OEM-defined profiler into the
            // CeLog data stream
            BYTE  Buf[1024];  
            DWORD dwBufSize = 1024;
            DWORD dwBufUsed;
            
            // Only supported via CeLog for now
            if ((g_dwProfilerFlags & PROFILE_OEMDEFINED)
                && (g_dwProfilerFlags & PROFILE_CELOG)
                && IsCeLogStatus(CELOGSTATUS_ENABLED_PROFILE)) {

                ProfilerControl control;
                control.dwVersion = 1;
                control.dwOptions = PROFILE_OEM_QUERY | PROFILE_OEMDEFINED;
                control.dwReserved = 0;
                control.OEM.dwControlSize = 0;    // no OEM struct being passed here
                control.OEM.dwProcessorType = 0;  
                if (OEMIoControl(IOCTL_HAL_OEM_PROFILER, &control,
                                 sizeof(ProfilerControl), Buf, dwBufSize,
                                 &dwBufUsed)) {

                    // Clear the RA since it does not apply when the data is just being queried
                    ((OEMProfilerData*)Buf)->ra = 0;
        
                    // Now that we have the data, send it to CeLog
                    g_pfnCeLogData(FALSE, CELID_OEMPROFILER_HIT, Buf, dwBufUsed, 0,
                                   CELZONE_PROFILER, 0, FALSE);
                }
            }
        }
    }
#endif // NKPROF
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_TurnOnProfiling(void)
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_TurnOnProfiling entry\r\n"));
    SET_PROFILE(pCurThread);
#if SHx
    ProfileFlag = 1;    // profile status
#endif
    DEBUGMSG(ZONE_ENTRY,(L"SC_TurnOnProfiling exit\r\n"));
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_TurnOffProfiling(void)
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_TurnOffProfiling entry\r\n"));
    CLEAR_PROFILE(pCurThread);
#if SHx
    ProfileFlag = 0;    // profile status
#endif
    DEBUGMSG(ZONE_ENTRY,(L"SC_TurnOffProfiling exit\r\n"));
}


#ifdef KCALL_PROFILE

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
GetKCallProfile(
    KPRF_t *pkprf,
    int loop,
    BOOL bReset
    )
{
    KCALLPROFON(26);
    memcpy(pkprf,&KPRFInfo[loop],sizeof(KPRF_t));
    if (bReset && (loop != 26))
        memset(&KPRFInfo[loop],0,sizeof(KPRF_t));
    KCALLPROFOFF(26);
    if (bReset && (loop == 26))
        memset(&KPRFInfo[loop],0,sizeof(KPRF_t));
}

#endif // KCALL_PROFILE


//
// Convert the number of ticks to microseconds.
//

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static DWORD 
local_ScaleDown(
    DWORD dwIn
    )
{
    LARGE_INTEGER liFreq;

    SC_QueryPerformanceFrequency(&liFreq);

    return ((DWORD) (((__int64) dwIn * 1000000) / liFreq.LowPart));
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_DumpKCallProfile(
    DWORD bReset
    )
{
#ifdef KCALL_PROFILE
    extern DWORD local_ScaleDown(DWORD);
    int loop;
    KPRF_t kprf;
    DWORD min = 0xffffffff, max = 0, total = 0, calls = 0;

    TRUSTED_API_VOID (L"SC_DumpKCallProfile");
#ifdef NKPROF
    calls = local_ScaleDown(1000);
    PROFILEMSG(1, (L"Resolution: %d.%3.3d usec per tick\r\n", calls / 1000, calls % 1000));
    KCall((PKFN)GetKCallProfile, &kprf, KCALL_NextThread, bReset);
    PROFILEMSG(1, (L"NextThread: Calls=%u Min=%u Max=%u Ave=%u\r\n",
        kprf.hits, local_ScaleDown(kprf.min),
        local_ScaleDown(kprf.max), kprf.hits ? local_ScaleDown(kprf.total) / kprf.hits : 0));
    for (loop = 0; loop < MAX_KCALL_PROFILE; loop++) {
        if (loop != KCALL_NextThread) {
            KCall((PKFN)GetKCallProfile, &kprf, loop, bReset);
            if (kprf.max > max)
                max = kprf.max;
            total += kprf.total;
            calls += kprf.hits;
        }
    }
    PROFILEMSG(1, (L"Other Kernel calls: Max=%u Avg=%u\r\n", max, calls ? local_ScaleDown(total) / calls : 0));
#else  // NKPROF
    calls = local_ScaleDown(1000);
    PROFILEMSG(1, (L"Resolution: %d.%3.3d usec per tick\r\n", calls / 1000, calls % 1000));
    PROFILEMSG(1, (L"Index Entrypoint                        Calls      uSecs    Min    Max    Ave\r\n"));
    for (loop = 0; loop < MAX_KCALL_PROFILE; loop++) {
        KCall((PKFN)GetKCallProfile, &kprf, loop, bReset);
        PROFILEMSG(1, (L"%5d %-30s %8d %10d %6d %6d %6d\r\n",
            loop, pKCallName[loop], kprf.hits, local_ScaleDown(kprf.total), local_ScaleDown(kprf.min),
            local_ScaleDown(kprf.max), kprf.hits ? local_ScaleDown(kprf.total) / kprf.hits : 0));
        if (kprf.min && (kprf.min < min))
            min = kprf.min;
        if (kprf.max > max)
            max = kprf.max;
        calls += kprf.hits;
        total += kprf.total;
    }
    PROFILEMSG(1, (L"\r\n"));
    PROFILEMSG(1, (L"      %-30s %8d %10d %6d %6d %6d\r\n",
        L"Summary", calls, local_ScaleDown(total), local_ScaleDown(min), local_ScaleDown(max),calls ? local_ScaleDown(total) / calls : 0));
#endif  // NKPROF
#endif  // KCALL_PROFILE
}


#ifdef NKPROF  // TOC info available only in profiling builds

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//
//  Functions used to look up symbol information included in the TOC in
//  profiling builds.
//
//  GetThreadName is used by CeLog on profiling builds.  The rest of the 
//  functions below serve as support for it.
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


// Return values from GetModulePointer
#define MODULEPOINTER_ERROR  0
#define MODULEPOINTER_NK     1
#define MODULEPOINTER_DLL    2
#define MODULEPOINTER_PROC   4

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static DWORD
GetModulePointer(
    PTHREAD pth,
    unsigned int pc,
    DWORD* dwResult
    ) 
{
    if (!dwResult) {
       return MODULEPOINTER_ERROR;
    }
    *dwResult = (DWORD) NULL;


    if (IsKernelVa(pc)) {
        //
        // NK.EXE
        //
        return MODULEPOINTER_NK;

    } else if (ZeroPtr(pc) > (DWORD) DllLoadBase && ZeroPtr(pc) < (2 << VA_SECTION)) {    // ROM dll is in slot 1
        //
        // It's a DLL address
        //
        PMODULE pModMax = NULL, pModTrav = NULL;
        DWORD dwMax = 0;

        //
        // Scan the modules list looking for a module with the highest base 
        // pointer that's less than the PC
        //
        pModTrav = pModList;

        while (pModTrav) {
            DWORD dwBase = ZeroPtr(pModTrav->BasePtr);

            if (dwBase > dwMax && dwBase < ZeroPtr(pc)) {
                dwMax = dwBase;
                pModMax = pModTrav;
            }
            pModTrav = pModTrav->pMod;  // Next module.
        }

        if (pModMax != NULL) {
            if (pModMax->oe.filetype & FA_DIRECTROM) {
                *dwResult = (DWORD) pModMax;
                return MODULEPOINTER_DLL;
            }
        }
    } else {
        //
        // We assume it's a process
        //
        if (pth->pOwnerProc->oe.filetype & FA_DIRECTROM) {
            *dwResult = (DWORD) (pth->pOwnerProc);
            return MODULEPOINTER_PROC;
        }
    }

    return MODULEPOINTER_ERROR;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static TOCentry*
GetTocPointer(
    PTHREAD pth,
    unsigned int pc
    )
{
    TOCentry* tocptr = NULL;
    DWORD     dwModulePointer = (DWORD) NULL;
    DWORD     dwMPFlag = GetModulePointer(pth, pc, &dwModulePointer);

    switch (dwMPFlag) {
        case MODULEPOINTER_NK: {
            // nk.exe: tocptr-> first entry in ROM
            tocptr = (TOCentry *)(pTOC+1);
            break;
        }

        case MODULEPOINTER_DLL: {
            PMODULE pMod = (PMODULE) dwModulePointer;
            tocptr = pMod->oe.tocptr;
            break;
        }

        case MODULEPOINTER_PROC: {
            PPROCESS pProc = (PPROCESS) dwModulePointer;
            tocptr = pProc->oe.tocptr;
            break;
        }

        default: {
            // Error
            tocptr = NULL;
        }
    }

    return tocptr;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static HANDLE
GetModHandle(
    PTHREAD pth,
    unsigned int pc
    ) 
{
    HANDLE hModule;
    DWORD  dwModulePointer = (DWORD) NULL;
    DWORD  dwMPFlag = GetModulePointer(pth, pc, &dwModulePointer);

    switch (dwMPFlag) {
        case MODULEPOINTER_NK: {
            // nk.exe is always the first process
            hModule = (HANDLE) ProcArray[0].hProc;
            break;
        }

        case MODULEPOINTER_DLL: {
            // A DLL doesn't have a handle, so return the pointer.
            hModule = (HANDLE) dwModulePointer;
            break;
        }

        case MODULEPOINTER_PROC: {
            hModule = (HANDLE) dwModulePointer;
            break;
        }

        default: {
            // Error
            hModule = INVALID_HANDLE_VALUE;
        }
    }

    return hModule;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPBYTE
GetSymbolText(
    LPBYTE lpSym,
    DWORD  dwSymNum
    )
{
    while (dwSymNum > 0) {
        while (*lpSym++);
        dwSymNum--;
    }
    return lpSym;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPCSTR
GetFunctionName(
    PTHREAD pth,
    unsigned int pc
    ) 
{
    LPCSTR     pszFunctionName = NULL;
    TOCentry*  tocexeptr;             // Pointer to TOC entry for module of interest
    PROFentry* profptr = NULL;        // profile entry pointer
    SYMentry*  symptr;                // profiler symbol entry pointer
    DWORD      dwSymIndex;            // symbol counter

    if (!pTOC->ulProfileOffset) {
        //
        // No symbols available.
        //
        return NULL;
    }

    if (IsKernelVa(pc)) {
        //
        // These addresses belong to NK.EXE.
        //
        if (pc >= 0xA0000000) {
            //
            // Hit a PSL exception address or perhaps a KDATA routine. No name.
            //
            return NULL;
        }
        //
        // Mask off the caching bit.
        //
        pc &= 0xdfffffff;
    }

    //
    // Find the TOC entry if one exists. If not in ROM, then we can't get a name.
    //
    tocexeptr = GetTocPointer(pth, pc);
    if (tocexeptr == NULL) {
        return NULL;
    }

    // Look up the profiler symbol entry for the module
    profptr = GetProfEntryFromTOCEntry(tocexeptr);
    if (!profptr) {
        // Not found in any XIP region
        return NULL;
    }
    
    //
    // If there are any symbols in this module, scan for the function.
    //
    if (profptr->ulNumSym) {
        DWORD     dwClosestSym;           // index of nearest symbol entry
        SYMentry* pClosestSym;            // ptr to nearest symbol entry

        dwSymIndex = 0;
        symptr = (SYMentry*) profptr->ulHitAddress;
        dwClosestSym = dwSymIndex;
        pClosestSym = symptr;

        //
        // Scan the whole list of symbols, looking for the closest match.
        //
        while ((dwSymIndex < profptr->ulNumSym) && (pClosestSym->ulFuncAddress != pc)) {
            // Keep track of the closest symbol found
            if ((symptr->ulFuncAddress <= pc)
                && (symptr->ulFuncAddress > pClosestSym->ulFuncAddress)) {
                dwClosestSym = dwSymIndex;
                pClosestSym = symptr;
            }

            dwSymIndex++;
            symptr++;
        }

        pszFunctionName = (LPCSTR) GetSymbolText((LPBYTE) profptr->ulSymAddress, dwClosestSym);
    }

    return pszFunctionName;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
GetThreadName(
    PTHREAD pth,
    HANDLE* hModule,
    WCHAR*  szThreadFunctionName
    )
{
    DWORD  dwProgramCounter = 0;

    //
    // Return a pointer to the thread's name if available.
    //
    if (pth != NULL) {

        // The thread's program counter is saved off when it is created.
        dwProgramCounter = pth->dwStartAddr;

        if (hModule != NULL) {
            *hModule = GetModHandle(pth, dwProgramCounter);
        }

        if (szThreadFunctionName != NULL) {

            if (dwProgramCounter == (DWORD) CreateNewProc) {
                //
                // Creating a new process, so use the proc name instead of the func
                //

                LPSTR lpszTemp;

                // First try to use the TOC to get the process name
                if (pth->pOwnerProc
                    && (pth->pOwnerProc->oe.filetype & FA_DIRECTROM)
                    && (pth->pOwnerProc->oe.tocptr)
                    && (lpszTemp = pth->pOwnerProc->oe.tocptr->lpszFileName)) {

                    KAsciiToUnicode(szThreadFunctionName, lpszTemp, MAX_PATH);

                } else if (!InSysCall()) {
                    // If we are not inside a KCall we can use the proc struct

                    LPWSTR lpszTempW;

                    if (pth->pOwnerProc
                        && (lpszTempW = pth->pOwnerProc->lpszProcName)) {

                        DWORD dwLen = strlenW(lpszTempW) + 1;
                        memcpy(szThreadFunctionName, lpszTempW, dwLen * sizeof(WCHAR));
                    } else {
                        szThreadFunctionName[0] = 0;
                    }

                } else {
                    // Otherwise we have no way to get the thread's name
                    szThreadFunctionName[0] = 0;
                }

            } else {
                //
                // Look up the function name
                //

                LPSTR lpszTemp = (LPSTR) GetFunctionName(pth, dwProgramCounter);
                if (lpszTemp) {
                    KAsciiToUnicode(szThreadFunctionName, lpszTemp, MAX_PATH);
                } else {
                    szThreadFunctionName[0] = 0;
                }
            }
        }

    } else {
        if (hModule != NULL) {
            *hModule = INVALID_HANDLE_VALUE;
        }
        if (szThreadFunctionName != NULL) {
            szThreadFunctionName[0] = 0;
        }
    }
}


#endif // NKPROF

