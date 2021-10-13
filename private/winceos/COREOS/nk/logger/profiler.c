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

//------------------------------------------------------------------------------
//
// PROFILER INTERFACE
//
// This file implements the kernel profiler support that is ONLY included in
// IMGPROFILER=1 builds of the kernel.  This file implements "classic"
// KITL-based profiling to the debug output window, based on a symbol table
// that is built into the image on IMGPROFILER=1 builds.
//
//------------------------------------------------------------------------------

#define C_ONLY
#include "kernel.h"
#include <profiler.h>

#define PROFILEMSG(cond,printf_exp) ((cond)?(NKDbgPrintfW printf_exp),1:0)


// Turning this zone on can break unbuffered mode, and can break buffered mode
// if the buffer overruns
#define ZONE_UNACCOUNTED 0


#include "kcall.h"
KPRF_t (* KPRFInfo)[MAX_KCALL_PROFILE] = NULL;


#if defined(x86)  // Callstack capture is only supported on x86 right now
// Temp buffer used to hold callstack during profiler hit; not threadsafe but
// all profiler hits should be serialized
#define PROFILER_MAX_STACK_FRAME        50
static BYTE g_ProfilerStackBuffer[PROFILER_MAX_STACK_FRAME*sizeof(CallSnapshot)];
static CONTEXT g_ProfilerContext;
#endif // defined(x86)


static void DoDumpKCallProfile(DWORD bReset);
static BOOL ProfilerAllocBuffer(void);
static BOOL ProfilerFreeBuffer(void);
static void ClearProfileHits(void);            // clear all profile counters


#define NUM_MODULES 200                 // max number of modules to display in report
#define NUM_SYMBOLS 500                 // max number of symbols to display in report


typedef struct {
    DWORD ra;                           // Interrupt program counter
    TOCentry* pte;
} PROFBUFENTRY, *PPROFBUFENTRY;

static struct {
    PROFBUFENTRY* pProfileBuf;          // Pointer to profile buffer (VirtualAlloc)
    DWORD  cbSize;                      // Size of buffer, in bytes
    DWORD  dwProfileBufNumEntries;      // Total number of profiler hits that fit in buffer
} g_ProfilerBufferInfo;


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Look up the profiler symbol entry for the given module
static PROFentry*
GetProfEntryFromTOCEntry(
    TOCentry *tocexeptr
    )
{
    ROMChain_t *pROM = ROMChain;   // Chain of ROM XIP regions
    DWORD       dwModIndex;        // Index of module wrt first XIP region
    TOCentry   *tocptr;            // table of contents entry pointer

    // Look for the module in each successive XIP region, to find its index
    // in the profiler symbol table.
    dwModIndex = 0;
    while (pROM && /*pROM->*/pTOC->ulProfileOffset) {
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


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Look up the profiler symbol entry for the given address
static PROFentry*
GetProfEntryFromAddr(
    unsigned int ra,
    // This input value is used while looking up profiler hits, because the
    // process may already have exited
    TOCentry *pProcessTocEntry,
    // This input value is used while looking up thread names, because the
    // process is still active
    PPROCESS pProcess,
    // Output value used to return the module handle
    HANDLE* phModule
    )
{
    PROFentry* profptr = NULL;            // profile entry pointer

    if (phModule) {
        *phModule = INVALID_HANDLE_VALUE;
    }

    // If no profile section just return
    if (!pTOC->ulProfileLen || !pTOC->ulProfileOffset) {
        return NULL;
    }

    // Since ROM modules and other DLLs are intermixed in the address space,
    // there are only two cases: user-mode EXE code and everyone else.  For
    // everyone else we have to walk ROM to figure out if the hit falls into a
    // ROM module.
    
    if (ra < VM_DLL_BASE) {
        // User-mode EXE
        if (ra > VM_USER_BASE) {
            if (pProcessTocEntry) {
                profptr = GetProfEntryFromTOCEntry(pProcessTocEntry);
            } else if (pProcess && pProcess->oe.filetype & FA_DIRECTROM) {
                profptr = GetProfEntryFromTOCEntry(pProcess->oe.tocptr);
                if (phModule) {
                    *phModule = (HANDLE) pProcess->dwId;
                }
            }
        }
    
        // Verify that the hit is really in bounds
        if (!profptr || (ra < profptr->ulStartAddr) || (ra > profptr->ulEndAddr)) {
            PROFILEMSG(ZONE_UNACCOUNTED, (TEXT("Hit dropped (out of bounds), ra=0x%08x pProcessTocEntry=0x%08x (0x%08x 0x%08x 0x%08x)\r\n"),
                       ra, pProcessTocEntry, profptr, profptr->ulStartAddr, profptr->ulEndAddr));
            return NULL;
        }

    } else {
        // NK.EXE, one of the DLLs in the static-mapped area, a kernel DLL, or
        // a user-mode DLL
        profptr = (PROFentry *)pTOC->ulProfileOffset;
        while (!((ra >= profptr->ulStartAddr) && (ra <= profptr->ulEndAddr))) {
            //
            // This is not the module/section for this hit.
            //
            profptr++;
            if (profptr >= (PROFentry *)pTOC->ulProfileOffset + g_ProfilerState.dwNumROMModules) {
                PROFILEMSG(ZONE_UNACCOUNTED, (TEXT("Hit dropped (ROM DLL not found), ra=0x%08x pProcessTocEntry=0x%08x\r\n"), ra, pProcessTocEntry));
                return NULL;
            }
        }
        
        // Note: there is no way to get the module handle
    }
    
    return profptr;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL
FindSymbolInModule(
    unsigned int pc,            // IN
    PROFentry*   profptr,       // IN
    DWORD*       pdwClosestSym, // OUT
    SYMentry**   ppClosestSym   // OUT
    )
{
    DWORD     dwClosestSym;             // index of nearest symbol entry
    SYMentry* pClosestSym;              // ptr to nearest symbol entry
    
    dwClosestSym = 0;
    pClosestSym = NULL;
    
    if (profptr->ulNumSym) {
        // Enumeration variables
        SYMentry* symptr = (SYMentry*) profptr->ulHitAddress;
        DWORD     dwSymIndex = dwClosestSym;
        
        pClosestSym = symptr;

        // Scan the whole list of symbols, looking for the closest match.
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
    }

    if (pdwClosestSym) {
        *pdwClosestSym = dwClosestSym;
    }
    if (ppClosestSym) {
        *ppClosestSym = pClosestSym;
    }
    
    return (pClosestSym ? TRUE : FALSE);
}


//------------------------------------------------------------------------------
//  ProfilerSymbolHit - Lookup profiler interrupt call address and record hit count
//
//  Input:  ra - interrupt return address
//          tocexeptr - pActvProc->oe.tocptr at time of interrupt
//
//  Attempts to find symbol address for the interrupt call address and increments
//  hit counts for module and symbol if found.
//------------------------------------------------------------------------------
static void
ProfilerSymbolHit(
    unsigned int ra,
    TOCentry *pProcessTocEntry
    )
{
    PROFentry* profptr;                 // profile entry pointer
    SYMentry* pClosestSym = NULL;

    profptr = GetProfEntryFromAddr(ra, pProcessTocEntry, NULL, NULL);
    if (!profptr) {
        // Not found in any XIP region
        PROFILEMSG(ZONE_UNACCOUNTED, (TEXT("Hit dropped (module not found), ra=0x%08x pProcessTocEntry=0x%08x\r\n"), ra, pProcessTocEntry));
        return;
    }

    profptr->ulHits++;                  // increment hits for this module
    if (FindSymbolInModule(ra, profptr, NULL, &pClosestSym)) {
        pClosestSym->ulFuncHits++;      // inc hit count for this symbol
    }
}


//------------------------------------------------------------------------------
// Pre-compute information about all of the XIP regions for faster access while
// profiling
//------------------------------------------------------------------------------
static VOID
InitProfilerROMState()
{
    // Compute the g_ProfilerState.dwNumROMModules value
    ROMChain_t *pROM = ROMChain;
    
    while (pROM) {
        // Count the total number of modules
        g_ProfilerState.dwNumROMModules += pROM->pTOC->nummods;
        pROM = pROM->pNext;
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL
ProfilerFreeBuffer(void)
{
    if (g_ProfilerBufferInfo.pProfileBuf) {
        VERIFY (VMFreeAndRelease (g_pprcNK, g_ProfilerBufferInfo.pProfileBuf,
                                  g_ProfilerBufferInfo.cbSize));
        g_ProfilerBufferInfo.pProfileBuf = NULL;
    }

    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL
ProfilerAllocBuffer(void)
{
    DWORD dwMemRequest = 0;

    if (dwProfileBufferMax) {
        // Buffer max was specified in BIB file via FIXUPVAR
        if (dwProfileBufferMax < (DWORD) (UserKInfo[KINX_PAGEFREE] * VM_PAGE_SIZE)) {
            dwMemRequest = dwProfileBufferMax;
        }
    }
    if (!dwMemRequest) {
        // Default to half the free RAM.
        dwMemRequest = ((UserKInfo[KINX_PAGEFREE] * VM_PAGE_SIZE) / 2);
    }
    dwMemRequest &= ~(VM_PAGE_SIZE-1);  // Page-align the size

    g_ProfilerBufferInfo.cbSize = dwMemRequest;
    g_ProfilerBufferInfo.dwProfileBufNumEntries = dwMemRequest / sizeof(PROFBUFENTRY);


    // Allocate a buffer to profile into
    g_ProfilerBufferInfo.pProfileBuf = VMAlloc (g_pprcNK, NULL, dwMemRequest,
                                                MEM_COMMIT, PAGE_READWRITE);
    
    if (g_ProfilerBufferInfo.pProfileBuf) {
        PROFILEMSG(1, (TEXT("ProfileStart() : Allocated %d KB for Profiler Buffer (0x%08X)\r\n"), dwMemRequest >> 10, g_ProfilerBufferInfo.pProfileBuf));
        return TRUE;
    }

    PROFILEMSG(1, (TEXT("ProfileStart() : Error allocating buffer... defaulting to symbol lookup in ISR.\r\n")));
    ProfilerFreeBuffer ();
    return FALSE;
}


//------------------------------------------------------------------------------
// Takes a raw index and looks up the tiered entry pointer.
//------------------------------------------------------------------------------
static _inline PPROFBUFENTRY
GetEntryPointer(
    DWORD dwIndex
    )
{
    if (dwIndex < g_ProfilerBufferInfo.dwProfileBufNumEntries) {
        return (g_ProfilerBufferInfo.pProfileBuf + dwIndex);
    }
    return NULL;
}


//------------------------------------------------------------------------------
//  ClearProfileHits - zero all profiler counters
//------------------------------------------------------------------------------
static void
ClearProfileHits(void)
{
    unsigned int iMod;
    PROFentry   *profptr;
    SYMentry    *symptr;
    unsigned int iSym;

    g_ProfilerState.dwSamplesRecorded = 0;
    g_ProfilerState.dwSamplesDropped  = 0;
    g_ProfilerState.dwSamplesInIdle   = 0;

    // If no profile section just return
    if (!pTOC->ulProfileLen || !pTOC->ulProfileOffset) {
        return;
    }
    
    profptr = (PROFentry *)pTOC->ulProfileOffset; // profptr-> first profile entry
    for (iMod = 0; iMod < g_ProfilerState.dwNumROMModules; iMod++) {
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
static LPBYTE
GetSymbol(LPBYTE lpSym, DWORD dwSymNum)
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
static void
ProfilerReportHeader(void)
{
    DWORD dwTotalSamples, dwRecordedSamples, dwNonIdleSamples;
    ULONG ulPercent;

    // Simplify some logic by adding up the totals
    dwTotalSamples    = g_ProfilerState.dwSamplesRecorded + g_ProfilerState.dwSamplesDropped + g_ProfilerState.dwSamplesInIdle;
    dwRecordedSamples = g_ProfilerState.dwSamplesRecorded + g_ProfilerState.dwSamplesInIdle;
    dwNonIdleSamples  = g_ProfilerState.dwSamplesRecorded;

    if (g_ProfilerState.dwProfilerFlags & PROFILE_OBJCALL) {
        PROFILEMSG(1, (TEXT("=================== SYSTEM CALL HIT REPORT ===================\r\n")));
    } else {
        PROFILEMSG(1, (TEXT("=================== MONTE CARLO HIT REPORT ===================\r\n")));
    }

    PROFILEMSG(1, (TEXT("Total samples recorded = %8lu\r\n"), dwRecordedSamples));
    PROFILEMSG(1, (TEXT("Total samples dropped  = %8lu\r\n"), g_ProfilerState.dwSamplesDropped));
    PROFILEMSG(1, (TEXT("Ticks elapsed          = %8lu\r\n"), g_ProfilerState.dwTickCount));
    
    // Object call profiling has no other header info
    if (g_ProfilerState.dwProfilerFlags & PROFILE_OBJCALL) {
        return;
    }

    // MONTE CARLO REPORT

    // Print idle report
    PROFILEMSG(1, (TEXT("IDLE TIME:\r\n")));
    PROFILEMSG(1, (TEXT("  * Total non-idle samples recorded                  = %8lu\r\n"),
                   dwNonIdleSamples));
    PROFILEMSG(1, (TEXT("  * Total idle samples recorded                      = %8lu\r\n"),
                   g_ProfilerState.dwSamplesInIdle));
    if (dwTotalSamples) {  // sanity check for division safety
        ulPercent = g_ProfilerState.dwSamplesInIdle;
        ulPercent *= 1000;
        ulPercent /= dwRecordedSamples;
        PROFILEMSG(1, (TEXT("  * Percentage of recorded samples in idle           = %6lu.%1d\r\n"),
                       ulPercent / 10, ulPercent % 10));
    }

    // Print interrupt report if we have data
    if (g_ProfilerState.dwInterrupts) {
        PROFILEMSG(1, (TEXT("INTERRUPTS:\r\n")));
        PROFILEMSG(1, (TEXT("  * Total interrupts during sample period            = %8lu\r\n"),
                       g_ProfilerState.dwInterrupts));
        PROFILEMSG(1, (TEXT("  * Non-profiler interrupts                          = %8lu\r\n"),
                       g_ProfilerState.dwInterrupts - dwTotalSamples));
        if (g_ProfilerState.dwTickCount) {  // sanity check for division safety
            ulPercent = g_ProfilerState.dwInterrupts - dwTotalSamples;
            ulPercent *= 10;
            ulPercent /= g_ProfilerState.dwTickCount;
            PROFILEMSG(1, (TEXT("  * Non-profiler interrupts per tick                 = %6lu.%1d\r\n"),
                           ulPercent / 10, ulPercent % 10));
        }
    }

    // Print TLB report if we have data.  Only SH and MIPS software TLB miss 
    // handlers produce this data right now, but CPU perf counters could be
    // used for other architectures in the future.
    if ((g_ProfilerState.dwTLBCount) || (g_ProfilerState_dwProfilerIntsInTLB)) {
        PROFILEMSG(1, (TEXT("TLB MISSES:\r\n")));
        
        PROFILEMSG(1, (TEXT("  * Total TLB misses during sample period            = %8lu\r\n"),
                       g_ProfilerState.dwTLBCount));
        if (g_ProfilerState.dwTickCount) {  // sanity check for division safety
            ulPercent = g_ProfilerState.dwTLBCount;
            ulPercent *= 10;
            ulPercent /= g_ProfilerState.dwTickCount;
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
static void
ProfilerReport(void)
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
    TOCentry  *pModuleTocEntry;
    DWORD      dwHits[NUM_MODULES], dwMods[NUM_MODULES], dwTemp;  // Sorted list of module hits
    PPROFBUFENTRY pEntry;
    DWORD      dwTotalSamples, dwRecordedSamples;
    PPROCESS   pprcTemp;

    #define IDLE_SIGNATURE ((DWORD)-1)
    
    // Simplify some logic by adding up the totals
    dwTotalSamples    = g_ProfilerState.dwSamplesRecorded + g_ProfilerState.dwSamplesDropped + g_ProfilerState.dwSamplesInIdle;
    dwRecordedSamples = g_ProfilerState.dwSamplesRecorded + g_ProfilerState.dwSamplesInIdle;
    
    if (!dwTotalSamples) {
        PROFILEMSG(1, (TEXT("Kernel Profiler: No hits recorded.  Make sure profiling\r\n")));
        PROFILEMSG(1, (TEXT("is implemented in the OAL, and the profiler was started.\r\n")));
        goto profile_exit;
    }

    // Sanity check: we should not have any samples if there's no profile section
    if (!pTOC->ulProfileLen || !pTOC->ulProfileOffset) {
        DEBUGCHK(0);
        goto profile_exit;
    }

    // If profiling to buffer, lookup symbols now
    if (g_ProfilerState.dwProfilerFlags & PROFILE_BUFFER) {
        PROFILEMSG(1, (TEXT("Kernel Profiler: Looking up symbols for %u hits.\r\n"), g_ProfilerState.dwSamplesRecorded));
        for (loop = 0; loop < g_ProfilerState.dwSamplesRecorded; loop++) {
            if (loop % 10000 == 0)    // display a . every 10000 hits, so user knows we are working
                PROFILEMSG(1, (TEXT(".")));

            pEntry = GetEntryPointer(loop);
            if (pEntry) {
                ProfilerSymbolHit(pEntry->ra, pEntry->pte);
            }
        }
        PROFILEMSG(1, (TEXT("\r\n")));
    }


    //
    // Report header contains lots of interesting goodies
    //
    
    ProfilerReportHeader();

    // Don't print anything else if the only recorded samples were in idle.
    if (!g_ProfilerState.dwSamplesRecorded) {
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
    if (g_ProfilerState.dwSamplesInIdle && dwModCount < NUM_MODULES) {
        dwHits[dwModCount] = g_ProfilerState.dwSamplesInIdle;
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
    PROFILEMSG(1, (TEXT("Module            Hits        Percent\r\n")));
    PROFILEMSG(1, (TEXT("----------------  ----------  -------\r\n")));
    
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
                pModuleTocEntry = ((TOCentry *)(pROM->pTOC+1)) + dwModIndex;
                pProf = ((PROFentry *)/*pROM->*/pTOC->ulProfileOffset) + dwMods[loop];  // All ROMs point to same profiler table
                DEBUGCHK(pProf->ulHits && (pProf->ulHits == dwHits[loop]));
                if (pProf->ulHits) {
                    // Display module name, hits, percentage
                    ulPercent = pProf->ulHits;
                    ulPercent *= 1000;
                    ulPercent /= dwRecordedSamples;
                    PROFILEMSG(1, (TEXT("%-16a  %10lu  %5lu.%1d\r\n"),
                                   pModuleTocEntry->lpszFileName, pProf->ulHits, ulPercent / 10, ulPercent % 10));
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
            PROFILEMSG(1, (TEXT("IDLE              %10lu  %5lu.%1d\r\n"),
                           dwHits[loop], ulPercent / 10, ulPercent % 10));
        }
    }
    // dwSymCount is now the count of symbols with nonzero hits
    
    // Print hits in unknown modules
    // Unknown modules are usually modules that are not in ROM.
    if (g_ProfilerState.dwSamplesRecorded - dwCount) {
        ulPercent = g_ProfilerState.dwSamplesRecorded - dwCount;
        ulPercent *= 1000;
        ulPercent /= dwRecordedSamples;
        PROFILEMSG(1, (TEXT("%-16a  %10lu  %5lu.%1d\r\n"),
                   "UNKNOWN", g_ProfilerState.dwSamplesRecorded - dwCount, ulPercent/10, ulPercent%10));
    }
    
    
    //
    // Display hits by symbol
    //
    
    if (!dwSymCount) {
        PROFILEMSG(1, (TEXT("No symbols found.\r\n")));
        goto profile_exit;
    }

    // Allocate memory for sorting
    pprcTemp = SwitchActiveProcess(g_pprcNK);
    pHits = (SortSYMentry *)PROCVMAlloc(g_pprcNK, NULL,
                                        (dwSymCount+1)*sizeof(SortSYMentry),  // +1 to hold idle count
                                        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE, NULL);
    SwitchActiveProcess(pprcTemp);
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
    if (g_ProfilerState.dwSamplesInIdle) {
        pHits[dwSymCount].ulFuncAddress = 0;
        pHits[dwSymCount].ulFuncHits = g_ProfilerState.dwSamplesInIdle;
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
    PROFILEMSG(1, (TEXT("Hits       Percent Address  Module           Routine\r\n")));
    PROFILEMSG(1, (TEXT("---------- ------- -------- ----------------:---------------------\r\n")));
    
    // Print the sorted list; stop after NUM_SYMBOLS
    dwCount = 0;  // # of hits that were successfully attributed to a symbol
    for (loop = 0; (loop < dwSymCount) && (loop < NUM_SYMBOLS); loop++) {
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
                pModuleTocEntry = (TOCentry *)(pROM->pTOC+1) + dwModIndex;
                DEBUGCHK(pProf->ulHits);

                // Find profile entry for this symbol
                pSym = (SYMentry *)pProf->ulHitAddress;
                for (loop2 = 0; loop2 < pProf->ulNumSym; loop2++) {
                    if ((pSym->ulFuncAddress == pHits[loop].ulFuncAddress)
                        && (pSym->ulFuncHits == pHits[loop].ulFuncHits)) {

                        ulPercent = pHits[loop].ulFuncHits;
                        ulPercent *= 1000;
                        ulPercent /= dwRecordedSamples;
                        PROFILEMSG(1, (TEXT("%10d %5d.%1d %8.8lx %-16a:%a\r\n"), pHits[loop].ulFuncHits,
                                   ulPercent / 10, ulPercent % 10, pHits[loop].ulFuncAddress, pModuleTocEntry->lpszFileName,
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
            PROFILEMSG(1, (TEXT("%10d %5d.%1d -------- ----------------:IDLE\r\n"),
                           pHits[loop].ulFuncHits, ulPercent / 10, ulPercent % 10));
        }
    }

    // Print hits in unlisted symbols.  These unlisted hits are hits beyond the
    // NUM_SYMBOLS that we printed, or hits in modules that are not in ROM.
    if (dwCount != g_ProfilerState.dwSamplesRecorded) {
        ulPercent = g_ProfilerState.dwSamplesRecorded - dwCount;
        ulPercent *= 1000;
        ulPercent /= dwRecordedSamples;
        PROFILEMSG(1, (TEXT("%10d %5d.%1d                          :<UNACCOUNTED FOR>\r\n"),
                       g_ProfilerState.dwSamplesRecorded - dwCount, ulPercent / 10, ulPercent % 10));
    }

profile_exit:
    if (pHits)
        PROCVMFree(g_pprcNK, pHits, 0, MEM_RELEASE, 0);
    return;
}


#if defined(x86)  // Callstack capture is only supported on x86 right now

//------------------------------------------------------------------------------
// Called by ProfilerHitEx when the profiler is using PROFILE_CELOG and
// PROFILE_CALLSTACK.  Logs the call stack of the interrupted thread to CeLog.
//------------------------------------------------------------------------------
BOOL
PROF_LogIntCallStack()
{
    DWORD dwLastError;
    DWORD dwNumFrames, dwSkip, dwFlags;
    
    // NKGetThreadCallStack may overwrite thread LastError
    dwLastError = KGetLastError(pCurThread);
    KSetLastError(pCurThread, 0);
    
    // Get context for the thread that was interrupted
    g_ProfilerContext.Ebp = pCurThread->ctx.TcxEbp;
    g_ProfilerContext.Eip = pCurThread->ctx.TcxEip;

    if (g_ProfilerState.dwProfilerFlags & PROFILE_CALLSTACK_INPROC) {
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
            g_pfnCeLogInterruptData(FALSE, CELID_CALLSTACK, g_ProfilerStackBuffer,
                                    dwNumFrames * sizeof(CallSnapshot));
            dwSkip += dwNumFrames;
        }

    } while ((dwNumFrames == PROFILER_MAX_STACK_FRAME)
             && (KGetLastError(pCurThread) == ERROR_INSUFFICIENT_BUFFER));

    // Restore thread LastError
    KSetLastError(pCurThread, dwLastError);

    return TRUE;
}

#endif  // defined(x86)


//------------------------------------------------------------------------------
// ClassicProfilerHitEx - ONLY used for buffered/unbuffered profiling.  CeLog
// profiling does not go through here.
// Input:  pData - OEM-specified buffer of profiling information (RA may be fixed up)
//------------------------------------------------------------------------------
void 
PROF_ClassicProfilerHit(
    OEMProfilerData *pData
    ) 
{
    BOOL fIdle = !GetPCB ()->pthSched && !RunList.pHead;

    DEBUGCHK(!(g_ProfilerState.dwProfilerFlags & PROFILE_CELOG));

    if (fIdle) {  // IDLE
        // Just increment a counter and exit.  If profiler samples are being dropped, then
        // drop idle samples too so that the idle stops recording at the same time.
        if (g_ProfilerState.dwSamplesDropped) {
            g_ProfilerState.dwSamplesDropped++;
        } else {
            g_ProfilerState.dwSamplesInIdle++;
        }
        return;
    }

    if (g_ProfilerState.dwProfilerFlags & (PROFILE_BUFFER)) {
        //
        // If profiling to buffer and there is still room in the buffer
        //
        PPROFBUFENTRY pEntry = GetEntryPointer (g_ProfilerState.dwSamplesRecorded);
        if (pEntry) {
            g_ProfilerState.dwSamplesRecorded++;
            //
            // Record an entry point
            //
            pEntry->ra  = pData->ra;
            pEntry->pte = pActvProc->oe.tocptr;
        } else {
            //
            // No place to record this hit. Let's remember how many we dropped.
            //
            g_ProfilerState.dwSamplesDropped++;
            PROFILEMSG(ZONE_UNACCOUNTED, (TEXT("Hit dropped (buffer full), ra=0x%08x\r\n"), pData->ra));
        }

    } else {
        // No buffer. Just lookup the symbol now.
        //
        g_ProfilerState.dwSamplesRecorded++;
        ProfilerSymbolHit(pData->ra, pActvProc->oe.tocptr);
    }
}


//------------------------------------------------------------------------------
// ClassicProfileStart - ONLY used for buffered/unbuffered/KCall profiling.
// CeLog profiling does not go through here.
// Resulting state is in g_ProfilerState.bStart.
//------------------------------------------------------------------------------
void 
PROF_ClassicProfileStart(
    const ProfilerControl* pControl,    // variable-sized struct, IN only
    DWORD dwControlSize
    )
{
    // Protect against running a profiling kernel on an unprofiled image
    if ((pTOC->ulProfileLen == 0) || (pTOC->ulProfileOffset == 0)) {
        PROFILEMSG(1, (TEXT("Kernel Profiler: Unable to start because profiling symbols are not built into\r\n")));
        PROFILEMSG(1, (TEXT("the run-time image.  Either this is a run-time image based on Image Update,\r\n")));
        PROFILEMSG(1, (TEXT("or PROFILE is set to OFF in config.bib. With this run-time image, you can only\r\n")));
        PROFILEMSG(1, (TEXT("run the kernel profiler in combination with CeLog and parse the log on the\r\n")));
        PROFILEMSG(1, (TEXT("desktop side.  To increase functionality, try the Remote Profiler or combine a\r\n")));
        PROFILEMSG(1, (TEXT("flush application such as CeLogFlush.exe with the following command:\r\n")));
        PROFILEMSG(1, (TEXT("\"prof on -l\".\r\n")));
        return;
    }

    // Pre-compute information about all of the XIP regions for
    // faster access while profiling
    if (0 == g_ProfilerState.dwNumROMModules) {
        InitProfilerROMState();
    }

    CommonProfileStop(pControl, dwControlSize);
    ClearProfileHits();         // reset all profile counts

    if (pControl->dwOptions & PROFILE_BUFFER) {
        // Attempt to alloc the buffer; skip buffering if the alloc fails
        if (ProfilerAllocBuffer()) {
            g_ProfilerState.dwProfilerFlags |= PROFILE_BUFFER;
        }
    }
    // Note: KCall doesn't work together with any of the storage
    // modes; it has its own
    
    // allocate KCall profiler buffer when at first time.
    // buffer won't be freed up until reboot
    if (pControl->dwOptions & PROFILE_KCALL) {
         if (KPRFInfo == NULL) {
            PREFAST_ASSUME(g_pKData->nCpus  <= MAX_CPU);  
            KPRFInfo = (KPRF_t (*) [MAX_KCALL_PROFILE])NKmalloc(sizeof(*KPRFInfo) * g_pKData->nCpus);
        }
        VERIFY (KPRFInfo);
        memset(KPRFInfo, 0, sizeof(*KPRFInfo) * g_pKData->nCpus);
    }

    if (!CommonProfileStart(pControl, dwControlSize)) {
        g_ProfilerState.dwProfilerFlags = 0;
        g_ProfilerState.bStart = FALSE;
    }
}


//------------------------------------------------------------------------------
// ClassicProfileStop - ONLY used for buffered/unbuffered/KCall profiling.
// CeLog profiling does not go through here.
// Resulting state is in g_ProfilerState.bStart.
//------------------------------------------------------------------------------
void 
PROF_ClassicProfileStop(
    const ProfilerControl* pControl,    // variable-sized struct, IN only
    DWORD dwControlSize
    )
{
    UNREFERENCED_PARAMETER (pControl);
    UNREFERENCED_PARAMETER (dwControlSize);
    if (g_ProfilerState.dwProfilerFlags & PROFILE_KCALL) {
        // Dump the KCall profile data
        DoDumpKCallProfile(TRUE);
        g_ProfilerState.dwProfilerFlags &= ~PROFILE_KCALL;

    } else {
        // Display profile hit report
        ProfilerReport();
    }

    if (g_ProfilerState.dwProfilerFlags & PROFILE_BUFFER) {
        g_ProfilerState.dwProfilerFlags &= ~PROFILE_BUFFER;
        ProfilerFreeBuffer();
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void 
GetKCallProfile(
    KPRF_t *pkprf,
    int loop,
    int cpu,
    BOOL bReset
    )
{
    KCALLPROFON(26);
    memcpy(pkprf,&KPRFInfo[cpu][loop],sizeof(KPRF_t));
    if (bReset && (loop != 26))
        memset(&KPRFInfo[cpu][loop],0,sizeof(KPRF_t));
    KCALLPROFOFF(26);
    if (bReset && (loop == 26))
        memset(&KPRFInfo[cpu][loop],0,sizeof(KPRF_t));
}


//------------------------------------------------------------------------------
// Convert the number of ticks to microseconds.
//------------------------------------------------------------------------------
static LONGLONG
local_ScaleDown(
    LONGLONG dwIn
    )
{
    LARGE_INTEGER liFreq;

    NKQueryPerformanceFrequency(&liFreq);

    return (((__int64) dwIn * 1000000) / liFreq.QuadPart);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void 
DoDumpKCallProfile(
    DWORD bReset
    )
{
    int loop;
    KPRF_t kprf = {0};
    KPRF_t kprfNextThread = {0};
    DWORD cpu;

    PROFILEMSG(1, (L"Resolution: %i64d.%6.6i64d usec per tick\r\n",
                   local_ScaleDown(1000000) / 1000000,
                   local_ScaleDown(1000000) % 1000000));

    for (cpu = 0; cpu < g_pKData->nCpus; cpu++) {
        LONGLONG min = MAXLONGLONG, max = 0, total = 0, calls = 0;

        PROFILEMSG(1, (L"CPU%2d: ============================================================================\r\n", cpu));
        PROFILEMSG(1, (L"Index Entrypoint                             Calls      uSecs    Min    Max    Avg\r\n"));

        for (loop = 0; loop < MAX_KCALL_PROFILE; loop++) {
            KCall((PKFN)GetKCallProfile, &kprf, loop, cpu, bReset);
        
            if (KCALL_NextThread == loop) {
                memcpy(&kprfNextThread, &kprf, sizeof(KPRF_t));
            } else {
                if (kprf.min && (kprf.min < min))
                    min = kprf.min;
                if (kprf.max > max)
                    max = kprf.max;
                total += kprf.total;
                calls += kprf.hits;
            }
        
            PROFILEMSG(1, (L"%5d %-35s %8d %10i64d %6i64d %6i64d %6i64d\r\n",
                           loop, pKCallName[loop], kprf.hits, local_ScaleDown(kprf.total), local_ScaleDown(kprf.min),
                           local_ScaleDown(kprf.max), kprf.hits ? local_ScaleDown(kprf.total) / kprf.hits : 0));
        }
        PROFILEMSG(1, (L"      %-35s %8i64d %10i64d %6i64d %6i64d %6i64d\r\n",
            L"TOTAL", calls, local_ScaleDown(total), local_ScaleDown(min), local_ScaleDown(max),calls ? local_ScaleDown(total) / calls : 0));
    
        PROFILEMSG(1, (L"-- Summary -------------------------------------\r\n"));
        PROFILEMSG(1, (L"NextThread: Calls=%u Min=%i64u Max=%i64u Avg=%i64u\r\n",
                       kprfNextThread.hits, local_ScaleDown(kprfNextThread.min),
                       local_ScaleDown(kprfNextThread.max),
                       kprfNextThread.hits ? local_ScaleDown(kprfNextThread.total) / kprfNextThread.hits : 0));
        PROFILEMSG(1, (L"Other Kernel calls: Max=%i64u Avg=%i64u\r\n",
                       local_ScaleDown(max), calls ? local_ScaleDown(total) / calls : 0));

        PROFILEMSG(1, (L"\r\n"));
    }

    
    PROFILEMSG(1, (L"\r\n"));
}


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


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
PROF_GetThreadName(
    PTHREAD pth,
    HANDLE* phModule,
    WCHAR*  pszThreadFunctionName
    )
{
    DWORD dwProgramCounter = 0;

    if (phModule != NULL) {
        *phModule = INVALID_HANDLE_VALUE;
    }
    if (pszThreadFunctionName != NULL) {
        pszThreadFunctionName[0] = 0;
    }

    //
    // Return a pointer to the thread's name if available.
    //
    if (pth != NULL) {
        PROFentry* profptr = NULL;

        // Initialize state if necessary
        if (0 == g_ProfilerState.dwNumROMModules) {
            InitProfilerROMState();
        }

        // The thread's program counter is saved off when it is created.
        dwProgramCounter = pth->dwStartAddr;

        // Get the module handle and get a profile pointer to use if necessary
        profptr = GetProfEntryFromAddr(dwProgramCounter, NULL, pth->pprcOwner, phModule);

        if (pszThreadFunctionName != NULL) {
            if (dwProgramCounter == (DWORD) CreateNewProc) {
                // Creating a new process, so use the proc name instead of the func
                LPCSTR lpszTemp;

                // First try to use the TOC to get the process name
                if (pth->pprcOwner
                    && (pth->pprcOwner->oe.filetype & FA_DIRECTROM)
                    && (pth->pprcOwner->oe.tocptr)
                    && (pth->pprcOwner->oe.tocptr->lpszFileName)) {

                    lpszTemp = pth->pprcOwner->oe.tocptr->lpszFileName;
                    NKAsciiToUnicode(pszThreadFunctionName, lpszTemp, MAX_PATH);

                } else if (!InSysCall()) {
                    // If we are not inside a KCall we can use the proc struct
                    LPCWSTR lpszTempW;

                    if (pth->pprcOwner
                        && (pth->pprcOwner->lpszProcName)) {

                        DWORD dwLen;
                        lpszTempW = pth->pprcOwner->lpszProcName;
                        dwLen = NKwcslen(lpszTempW) + 1;
                        memcpy(pszThreadFunctionName, lpszTempW, dwLen * sizeof(WCHAR));
                    }
                }

            } else if (profptr) {
                // Look up the function name
                DWORD  dwClosestSym = 0;      // index of nearest symbol entry

                if (FindSymbolInModule(dwProgramCounter, profptr, &dwClosestSym, NULL)) {
                    LPCSTR lpszTemp;
                    
                    lpszTemp = (LPCSTR) GetSymbol((LPBYTE) profptr->ulSymAddress, dwClosestSym);
                    if (lpszTemp) {
                        NKAsciiToUnicode(pszThreadFunctionName, lpszTemp, MAX_PATH);
                    }
                }
            }
        }
    }
}

