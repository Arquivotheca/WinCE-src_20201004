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
#include "romldr.h"
#include "xtime.h"
#include <profiler.h>
#include <celog.h>
#include "altimports.h"

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
#endif

int (*PProfileInterrupt)(void)=NULL;    // pointer to profiler ISR, 
                                        // set in platform profiler code

BOOL bProfileTimerRunning=FALSE;        // this variable is used in OEMIdle(), so must be defined even in a non-profiling kernel

#ifdef NKPROF
DWORD g_dwProfilerFlags;                // ObjCall, KCall, Buffer, CeLog
DWORD g_dwNumROMModules;                // Number of modules in all XIP regions
DWORD g_dwFirstROMDLL;                  // First DLL address over all XIP regions
DWORD g_dwNKIndex;                      // Index (wrt first XIP region) of NK in PROFentry array
extern void SC_DumpKCallProfile(DWORD bReset);
extern BOOL ProfilerAllocBuffer(void);
extern BOOL ProfilerFreeBuffer(void);
#endif

// Globals that allow a logging engine to be hot-plugged in the system.
PFNVOID g_pfnCeLogData;
PFNVOID g_pfnCeLogInterrupt;
PFNVOID g_pfnCeLogSetZones;
FARPROC g_pfnCeLogQueryZones;
DWORD g_dwCeLogTimerFrequency;

DWORD dwCeLogTLBMiss;

// OEM can override these globals
DWORD dwCeLogLargeBuf;
DWORD dwCeLogSmallBuf;

const volatile DWORD dwProfileBufferMax = 0;

typedef BOOL (*PFN_INITLOADERHOOK)(DWORD, LPCTSTR);
typedef PMODULE (*PFN_WHICHMOD)(PMODULE, LPCTSTR, LPCTSTR, DWORD, DWORD);
typedef BOOL (*PFN_UNDODEPENDS)(e32_lite *eptr, DWORD BaseAddr);
typedef BOOL (*PFN_VERIFIERIOCONTROL)(DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);

PFN_INITLOADERHOOK g_pfnInitLoaderHook = NULL;
PFN_WHICHMOD g_pfnWhichMod = NULL;
PFN_UNDODEPENDS g_pfnUndoDepends = NULL;
PFN_VERIFIERIOCONTROL g_pfnVerifierIoControl = NULL;

#ifdef NKPROF

#define NUM_MODULES 200                 // max number of modules to display in report
#define NUM_SYMBOLS 500                 // max number of symbols to display in report

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
#endif

#define PROF_ENTRY_MASK       ((1 << PROF_PAGE_SHIFT) - 1)

PDWORD g_pdwProfileBufStart;            // platform profile buffer start
HANDLE g_hProfileBuf;

DWORD dwProfileCount;                   // total number of profiler calls
DWORD  dwPerProcess;                    // per process profiling flag
void ClearProfileHits(void);            // clear all profile counters

#define PROFILE_BUFFER_MAX   256*1024*1024  // Absolute max of 256 MB
#define PAGES_IN_MAX_BUFFER    PROFILE_BUFFER_MAX/PAGE_SIZE
#define BLOCKS_PER_SECTION  0x200
#define PAGES_PER_SECTION   BLOCKS_PER_SECTION * PAGES_PER_BLOCK

PPROFBUFENTRY g_pPBE[PAGES_IN_MAX_BUFFER];
DWORD  dwBufNumPages;              // Total number of pages
DWORD  dwBufNumEntries;            // Total number of entries
DWORD  dwBufRecDropped;            // How many hits were not recorded?

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
        ROMChain_t *pROM = ROMChain;   // Chain of ROM XIP regions
        DWORD dwModIndex;              // Index of module wrt first XIP region
        TOCentry *tocptr;              // table of contents entry pointer

        // Look for the symbol in each successive XIP region
        dwModIndex = 0;
        while (pROM) {
            tocptr = (TOCentry *)(pROM->pTOC+1);  // tocptr-> first entry in ROM
            if ((tocexeptr >= tocptr)
                && (tocexeptr < tocptr + pROM->pTOC->nummods)) {

                // found it
                                                // compute module number
                dwModIndex += ((DWORD)tocexeptr - (DWORD)tocptr) / sizeof(TOCentry);
                                                // make profptr point to entry for this module
                profptr = (PROFentry *)pROM->pTOC->ulProfileOffset + dwModIndex;
                break;
            }

            dwModIndex += pROM->pTOC->nummods;
            pROM = pROM->pNext;
        }
        if (!pROM) {
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
BOOL
ProfilerFreeBuffer(void)
{
    DWORD dwMemSize;
    BOOL fRet;

    dwMemSize = dwBufNumPages * PAGE_SIZE;

    fRet = SC_UnlockPages(g_pdwProfileBufStart, dwMemSize);
    ASSERT(fRet);
    fRet = UnmapViewOfFile(g_pdwProfileBufStart);
    ASSERT(fRet);
    fRet = CloseHandle(g_hProfileBuf);
    ASSERT(fRet);

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
    BOOL fRet;
    DWORD i;

    if (dwProfileBufferMax) {
        //
        // Buffer max was specified in BIB file via FIXUP_VAR
        //
        if (dwProfileBufferMax < PROFILE_BUFFER_MAX) {
            if (dwProfileBufferMax < (DWORD) (UserKInfo[KINX_PAGEFREE] * PAGE_SIZE)) {
                dwMemRequest = dwProfileBufferMax;
            } else {
                DEBUGMSG(1, (TEXT("dwProfileBufferMax = 0x%08X, but only 0x%08X available.\r\n"), 
                             dwProfileBufferMax, (UserKInfo[KINX_PAGEFREE] * PAGE_SIZE)));
            }
        } else {
            DEBUGMSG(1, (TEXT("dwProfileBufferMax = 0x%08X, but max is 0x%08X.\r\n"), 
                         dwProfileBufferMax, PROFILE_BUFFER_MAX));
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
    dwBufNumPages   = dwMemRequest / PAGE_SIZE;
    dwBufNumEntries = dwBufNumPages * ENTRIES_PER_PAGE;

    //
    // Create a shared memory-mapped object
    //
    g_hProfileBuf = CreateFileMapping((HANDLE) -1, NULL,
                                     PAGE_READWRITE | SEC_COMMIT, 0,
                                     dwMemRequest, NULL);
    if (!g_hProfileBuf) {
        goto EXIT_FALSE;
    }

    // Map in the object
    g_pdwProfileBufStart = (PDWORD) MapViewOfFile(g_hProfileBuf, FILE_MAP_ALL_ACCESS, 0, 0, 0);

    if (g_pdwProfileBufStart == NULL) {
        CloseHandle(g_hProfileBuf);
        goto EXIT_FALSE;
    }

    //
    // Lock the pages and get the physical addresses.
    //
    fRet = ProfLockPages(g_pdwProfileBufStart, dwMemRequest, (PDWORD) g_pPBE, LOCKFLAG_READ | LOCKFLAG_WRITE);

    if (fRet == FALSE) {
        UnmapViewOfFile(g_pdwProfileBufStart);
        CloseHandle(g_hProfileBuf);
        goto EXIT_FALSE;
    }

    //
    // Convert the physical addresses to statically-mapped virtual addresses
    //
    for (i = 0; i < dwBufNumPages; i++) {
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
    dwBufRecDropped = 0;
}


//---------------------------------------------------------------------------
//  GetSymbol - searches symbol table for passed symbol number
//
//  Input:  lpSym - pointer to start of symbol buffer
//          dwSymNum - symbol number to lookup
//
//  Output: returns pointer to symbol
//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------
//  ProfilerReport -display hit report
//
//
//---------------------------------------------------------------------------
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
    ULONG      ulTotalPercent;           // total percent of modules
    ULONG      ulTotalHits;
    TOCentry  *tocptr;
    DWORD      dwHits[NUM_MODULES], dwMods[NUM_MODULES], dwTemp;  // Sorted list of module hits
    PPROFBUFENTRY pEntry;

    if (dwProfileCount == 0) {
        PROFILEMSG(1, (TEXT("Kernel Profiler: No hits recorded.  Make sure profiling\r\n")));
        PROFILEMSG(1, (TEXT("is implemented in the OAL, and the profiler was started.\r\n")));
        goto profile_exit;
    }

    // If profiling to buffer, lookup symbols now
    if (g_dwProfilerFlags & PROFILE_BUFFER) {
        PROFILEMSG(1, (TEXT("Kernel Profiler: Looking up symbols for %u hits.\r\n"), dwProfileCount));
        for (loop = 0; loop < dwProfileCount; loop++) {
            if (loop % 10000 == 0)    // display a . every 10000 hits, so user knows we are working
                PROFILEMSG(1, (TEXT(".")));

            pEntry = GetEntryPointer(loop);
            ProfilerSymbolHit(pEntry->ra, pEntry->pte);
        }
        PROFILEMSG(1, (TEXT("\r\n")));
    }


    //
    // Display hits by module and count number of symbols hit
    //

    PROFILEMSG(1,(TEXT("Total samples recorded = %lu\r\n"), dwProfileCount));
    if (dwBufRecDropped) {
        PROFILEMSG(1,(TEXT("Total samples dropped  = %lu\r\n"), dwBufRecDropped));
    }

    ulTotalPercent = 0;
    ulTotalHits = 0;
    dwModCount = 0;
    dwSymCount = 0;
    
    // Insert the modules with nonzero hits into the list
    dwModIndex = 0;
    pROM = ROMChain;
    pProf = (PROFentry *)pROM->pTOC->ulProfileOffset;
    while (pROM) {
        dwNumModules = pROM->pTOC->nummods;
        for (loop = 0; loop < dwNumModules; loop++) {
            if (pProf->ulHits) {
                if (dwModCount < NUM_MODULES) {
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

    PROFILEMSG(1, (TEXT("Module        Hits        Percent\r\n")));
    PROFILEMSG(1, (TEXT("------------  ----------  -------\r\n")));
    
    // Print the sorted list
    dwModIndex = 0; // index wrt current XIP region
    dwCount = 0;  // # of symbol hits that were successfully attributed to a module
    for (loop = 0; loop < dwModCount; loop++) {
        // Figure out which XIP region the module was in
        pROM = ROMChain;
        dwModIndex = dwMods[loop];
        while ((dwModIndex >= pROM->pTOC->nummods) && pROM) {
            dwModIndex -= pROM->pTOC->nummods;
            pROM = pROM->pNext;
        }
        DEBUGCHK(pROM);
        if (pROM) {
            tocptr = ((TOCentry *)(pROM->pTOC+1)) + dwModIndex;
            pProf = ((PROFentry *)pROM->pTOC->ulProfileOffset) + dwMods[loop];
            DEBUGCHK(pProf->ulHits && (pProf->ulHits == dwHits[loop]));
            if (pProf->ulHits) {
                // Display module name, hits, percentage
                ulPercent = pProf->ulHits;
                ulPercent *= 1000;
                ulPercent /= dwProfileCount;
                ulTotalPercent += ulPercent;
                ulTotalHits += pProf->ulHits;
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
    }
    // dwSymCount is now the count of symbols with nonzero hits
    
    



    ulTotalHits = dwProfileCount - dwCount;
    if (ulTotalHits) {
        ulPercent = ulTotalHits * 1000 / dwProfileCount;
        PROFILEMSG(1, (TEXT("%-12a  %10lu  %5lu.%1d\r\n"),
                   "UNKNOWN", ulTotalHits, ulPercent/10, ulPercent%10));
    }
    
    
    //
    // Display hits by symbol
    //
    
    if (!dwSymCount) {
        PROFILEMSG(1, (TEXT("No symbols found.\r\n")));
        goto profile_exit;
    }

    // Allocate memory for sorting
    pHits = (SortSYMentry *)VirtualAlloc(NULL, dwSymCount*sizeof(SortSYMentry),
                                         MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (pHits == NULL) {
        PROFILEMSG(1,(TEXT("ProfileStop: Sort memory allocation size %u failed.\r\n"),dwSymCount*sizeof(SYMentry)));
        goto profile_exit;
    }
    
    
    // Insert the symbols with nonzero hits into the list
    dwCount = dwSymCount;  // Temp holder to make sure we don't exceed this number
    dwSymCount = 0;
    dwModIndex = 0;
    pROM = ROMChain;
    pProf = (PROFentry *)pROM->pTOC->ulProfileOffset;
    while (pROM) {
        dwNumModules = pROM->pTOC->nummods;
        for (loop = 0; loop < dwNumModules; loop++) {
            if (pProf->ulHits) {
                pSym = (SYMentry *)pProf->ulHitAddress;
                for (loop2 = 0; loop2 < pProf->ulNumSym; loop2++) {
                    if (pSym->ulFuncHits && (dwSymCount < dwCount)) {
                        pHits[dwSymCount].ulFuncAddress = pSym->ulFuncAddress;
                        pHits[dwSymCount].ulFuncHits = pSym->ulFuncHits;
                        pHits[dwSymCount++].ulModuleIndex = dwModIndex + loop;  // index wrt first XIP region
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

    PROFILEMSG(1, (TEXT("\r\n")));
    PROFILEMSG(1, (TEXT("Hits       Percent Address  Module       Routine\r\n")));
    PROFILEMSG(1, (TEXT("---------- ------- -------- ------------:---------------------\r\n")));
    
    // Print the sorted list; stop after NUM_SYMBOLS
    dwCount = 0;  // # of hits that were successfully attributed to a symbol
    for (loop = 0; (loop < dwSymCount) && (loop < NUM_SYMBOLS); loop++) {
        // Figure out which XIP region and module the symbol was in
        pROM = ROMChain;
        dwModIndex = pHits[loop].ulModuleIndex;
        while ((dwModIndex >= pROM->pTOC->nummods) && pROM) {
            dwModIndex -= pROM->pTOC->nummods;
            pROM = pROM->pNext;
        }
        DEBUGCHK(pROM);
        if (pROM) {
            pProf = (PROFentry *)pROM->pTOC->ulProfileOffset + pHits[loop].ulModuleIndex;
            tocptr = (TOCentry *)(pROM->pTOC+1) + dwModIndex;
            DEBUGCHK(pProf->ulHits);
            
            // Find profile entry for this symbol
            pSym = (SYMentry *)pProf->ulHitAddress;
            for (loop2 = 0; loop2 < pProf->ulNumSym; loop2++) {
                if ((pSym->ulFuncAddress == pHits[loop].ulFuncAddress)
                    && (pSym->ulFuncHits == pHits[loop].ulFuncHits)) {

                    ulPercent = pHits[loop].ulFuncHits;
                    ulPercent *= 1000;
                    ulPercent /= dwProfileCount;
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
    }

    // Print hits in unlisted symbols.  These unlisted hits are hits beyond the
    // NUM_SYMBOLS that we printed, or hits in modules that are not in ROM.
    if (dwCount != dwProfileCount) {
        dwTemp     = (dwProfileCount - dwCount) * 1000;
        ulPercent  = dwTemp / dwProfileCount;      // % of overall hits
        PROFILEMSG(1, (TEXT("%10d %5d.%1d                      :<UNACCOUNTED FOR>\r\n"),
                       dwProfileCount - dwCount, ulPercent / 10, ulPercent % 10));
    }

profile_exit:
    if (pHits)
        VirtualFree(pHits, 0, MEM_DECOMMIT | MEM_FREE);
    return;
}

#endif


#ifdef NKPROF
//---------------------------------------------------------------------------
// This is a dummy function for the profiler
//---------------------------------------------------------------------------
void IDLE_STATE()
{
    //
    // Force the function to be non-empty so the hit engine can find it.
    //
    static volatile DWORD dwVal;
    dwVal++;
}
#endif

//---------------------------------------------------------------------------
//  GetEPC - Get exception program counter
//
//  Output: returns EPC, zero if cpu not supported
//---------------------------------------------------------------------------
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

    pData->ra = (RunList.pth || RunList.pRunnable) ? pData->ra : (DWORD) IDLE_STATE;

	if(g_dwProfilerFlags & PROFILE_CELOG) {
		// Fixup RA
	    if (IsKernelVa(pData->ra)) {     // if high bit set, this is NK
	        pData->ra &= 0xdfffffff;     // mask off uncached bit
	    } else {
	        pData->ra = (DWORD)MapPtrProc(pData->ra, pCurProc);
	    }

        // Send data to celog
        if (IsCeLogProfilingEnabled()) {
        	if(g_dwProfilerFlags & PROFILE_OEMDEFINED) {
	            g_pfnCeLogData(TRUE, CELID_OEMPROFILER_HIT, pData,
	                           pData->dwBufSize + 2*sizeof(DWORD),  // dwBufSize does not include header; can't use sizeof(OEMProfilerData) because the 0-byte array has nonzero size
	                           0, CELZONE_PROFILER, 0, FALSE);
        	}
    		else {
	            g_pfnCeLogData(FALSE, CELID_MONTECARLO_HIT, &(pData->ra), sizeof(DWORD), 0,
	                           CELZONE_PROFILER, 0, FALSE);
    		}
        }
	}
	else if(g_dwProfilerFlags & (PROFILE_BUFFER)) {
        //
        // If profiling to buffer and there is still room in the buffer
        //
        if (dwProfileCount < dwBufNumEntries) {
            PPROFBUFENTRY pEntry;

            if (IsKernelVa(pData->ra))       // if high bit set, this is NK
                pData->ra &= 0xdfffffff;     // mask off uncached bit
  
            pEntry = GetEntryPointer(dwProfileCount);
            dwProfileCount++;
            //
            // Record an entry point
            //
            pEntry->ra = pData->ra;
            pEntry->pte = pCurProc->oe.tocptr;
        } else {
            //
            // No place to record this hit. Let's remember how many we dropped.
            //
            dwBufRecDropped++;
            PROFILEMSG(ZONE_UNACCOUNTED, (TEXT("Hit dropped (buffer full), ra=0x%08x\r\n"), pData->ra));
        }
	

	}
	else {
        // No buffer. Just lookup the symbol now.
        //
        dwProfileCount++;
        ProfilerSymbolHit(pData->ra,pCurProc->oe.tocptr);
	}
   
#endif
}



//------------------------------------------------------------------------------
// SC_ProfileSyscall - Turns profiler on and off.  The workhorse behind
// ProfileStart(), ProfileStartEx(), ProfileStop() and ProfileCaptureStatus().
//------------------------------------------------------------------------------
void 
SC_ProfileSyscall(
    LPXT lpXt
    )
{
#ifdef NKPROF
    static int   scPauseContinueCalls = 0;
    static BOOL  bStart = FALSE;
    static DWORD dwCeLogZoneUser, dwCeLogZoneCE, dwCeLogZoneProcess;

    if (lpXt->dwOp == XTIME_PROFILE_DATA) {
        switch (lpXt->dwTime[0]) {
        
        case 0: {
            // ProfileStart() / ProfileStartEx()
            DWORD dwOptions;  // copy so that it can be modified, and accessed without a pointer
            ProfilerControl *pControl = (ProfilerControl*)MapPtrProc(lpXt->dwTime[2], pCurProc);

            if (!pControl || (pControl->dwVersion != 1)) {
                return;
            }
            dwOptions = pControl->dwOptions;

            if (dwOptions & PROFILE_CONTINUE) {
                if (bStart && ((g_dwProfilerFlags & (PROFILE_OBJCALL | PROFILE_KCALL)) == 0)) {
                    ++scPauseContinueCalls;
                    // start profiler timer on 0 to 1 transition
                    if (1 == scPauseContinueCalls) {
                        if (g_dwProfilerFlags & PROFILE_OEMDEFINED) {
                            // OEM-specific profiler
                            OEMIoControl(IOCTL_HAL_OEM_PROFILER, pControl,
                                         pControl->OEM.dwControlSize,
                                         NULL, 0, NULL);

                        } else {
                            // Monte Carlo profiling
                            OEMProfileTimerEnable(pControl->Kernel.dwUSecInterval);
                            bProfileTimerRunning = TRUE;
                        }
                    }
                }
            
            } else if (dwOptions & PROFILE_PAUSE) {
                if (bStart && ((g_dwProfilerFlags & (PROFILE_OBJCALL | PROFILE_KCALL)) == 0)) {
                    --scPauseContinueCalls;
                    // start profiler timer on 1 to 0 transition
                    if (!scPauseContinueCalls) {
                        if (g_dwProfilerFlags & PROFILE_OEMDEFINED) {
                            // OEM-specific profiler
                            OEMIoControl(IOCTL_HAL_OEM_PROFILER, pControl,
                                         pControl->OEM.dwControlSize,
                                         NULL, 0, NULL);
                        } else {
                            // Monte Carlo profiling
                            OEMProfileTimerDisable();
                            bProfileTimerRunning = FALSE;
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
                    && (dwOptions & PROFILE_CELOG == 0)) { // Allow profiling with celog even if no symbols
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

                if ((dwOptions & PROFILE_CELOG) && !IsCeLogLoaded()) {
                    PROFILEMSG(1, (TEXT("Kernel Profiler: Unable to start, celog is not loaded!\r\n")));
                    return;
                }

                // Debug output so the user knows exactly what's going on
                if (dwOptions & PROFILE_KCALL) {
                    PROFILEMSG(1, (TEXT("Kernel Profiler: Gathering KCall data\r\n")));
                } else {
                    PROFILEMSG(1, (TEXT("Kernel Profiler: Gathering %s data in %s mode\r\n"),
                                   (dwOptions & PROFILE_OEMDEFINED) ? TEXT("OEM-Defined")
                                       : (dwOptions & PROFILE_OBJCALL) ? TEXT("ObjectCall")
                                       : TEXT("MonteCarlo"),
                                   (dwOptions & PROFILE_CELOG) ? TEXT("CeLog")
                                       : ((dwOptions & PROFILE_BUFFER) ? TEXT("buffered")
                                       : TEXT("unbuffered"))));
                }
                
                
                OEMProfileTimerDisable();   // disable profiler timer
                bProfileTimerRunning = FALSE;
                bStart = TRUE;
                ++scPauseContinueCalls;

                dwProfileCount = 0;         // clear profile count
                ClearProfileHits();         // reset all profile counts
                
                g_dwProfilerFlags = 0;

                //
                // Determine the storage mode for the data
                //
                if (dwOptions & PROFILE_CELOG) {

                    DEBUGCHK(IsCeLogLoaded());
                    
                    



                    
                    // Disable CeLog general logging; log only profile events
                    CeLogEnableProfiling();

                    // Make sure the correct zones are turned on; save the
                    // old zone settings to restore later
                    SC_CeLogGetZones(&dwCeLogZoneUser, &dwCeLogZoneCE,
                                     &dwCeLogZoneProcess, NULL);
                    SC_CeLogSetZones(0xFFFFFFFF, CELZONE_PROFILER, 0xFFFFFFFF);
                    SC_CeLogReSync();

                    // Log the start event
                    if(dwOptions & PROFILE_OEMDEFINED)
                    {
	                    g_pfnCeLogData(TRUE, CELID_PROFILER_START, pControl,
	                                   sizeof(ProfilerControl) + pControl->OEM.dwControlSize, 0,
	                                   CELZONE_PROFILER, 0, FALSE);
                    }
                    else
                    {
	                    g_pfnCeLogData(TRUE, CELID_PROFILER_START, pControl,
	                                   sizeof(ProfilerControl), 0,
	                                   CELZONE_PROFILER, 0, FALSE);

                    }

	                g_dwProfilerFlags |= PROFILE_CELOG;
                
                } else if (dwOptions & PROFILE_BUFFER) {
                    // Attempt to alloc the buffer; skip buffering if the alloc fails
                    if (ProfilerAllocBuffer()) {
                        g_dwProfilerFlags |= PROFILE_BUFFER;
                    }
                }
                
                


                // Determine which type of data is being gathered
                if (dwOptions & PROFILE_OEMDEFINED) {
                    // OEM-specific profiler
                    if (!OEMIoControl(IOCTL_HAL_OEM_PROFILER, pControl,
                                      sizeof(ProfilerControl) + pControl->OEM.dwControlSize,
                                      NULL, 0, NULL)) {
                        PROFILEMSG(1, (TEXT("Kernel Profiler: OEM Profiler start failed!\r\n")));
                        // Restore CeLog state
                        if (IsCeLogLoaded()) {
                            SC_CeLogSetZones(dwCeLogZoneUser, dwCeLogZoneCE, dwCeLogZoneProcess);
                            CeLogEnableGeneral();
                        }
                        g_dwProfilerFlags = 0;
                        return;
                    }
                    g_dwProfilerFlags |= PROFILE_OEMDEFINED;

                } else if (dwOptions & PROFILE_OBJCALL) {
                    // Object call profiling
                    g_dwProfilerFlags |= PROFILE_OBJCALL;
                
                } else if (dwOptions & PROFILE_KCALL) {
                    // Kernel call profiling
                    g_dwProfilerFlags |= PROFILE_KCALL;
                
                } else {
                    // Monte Carlo profiling
                    if (!(dwOptions & PROFILE_STARTPAUSED)) {
                        // start profiler timer
                        OEMProfileTimerEnable(pControl->Kernel.dwUSecInterval);
                        bProfileTimerRunning = TRUE;
                    }
                }
            }
            break;
        }

        case 1: {
            // ProfileStop()
            
            if (bProfileTimerRunning) {
                OEMProfileTimerDisable();   // disable profiler timer
                bProfileTimerRunning = FALSE;
            } else if (g_dwProfilerFlags & PROFILE_OEMDEFINED) {
                // OEM-specific profiler
                ProfilerControl control;
                control.dwVersion = 1;
                control.dwOptions = PROFILE_STOP;
                control.OEM.dwControlSize = 0;    // no OEM struct passed to stop profiler
                control.OEM.dwProcessorType = 0;  
                OEMIoControl(IOCTL_HAL_OEM_PROFILER, &control,
                             sizeof(ProfilerControl), NULL, 0, NULL);
            }

            bStart = FALSE;
            scPauseContinueCalls = 0;
            
            if (g_dwProfilerFlags & PROFILE_CELOG) {
                // Resume general logging
                if (IsCeLogLoaded()) {
                    // Log the stop event
                    g_pfnCeLogData(FALSE, CELID_PROFILER_STOP, NULL, 0, 0,
                                   CELZONE_PROFILER, 0, FALSE);
                    
                    // Restore the original zone settings
                    SC_CeLogSetZones(dwCeLogZoneUser, dwCeLogZoneCE, dwCeLogZoneProcess);

                    // Restore CeLog general logging
                    CeLogEnableGeneral();
                }
                g_dwProfilerFlags &= ~PROFILE_CELOG;
                
                // No report to print...  The reports are made by desktop tools
                // that parse the log
                PROFILEMSG(1, (TEXT("Profiler data written to CeLog.\r\n")));
            
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
            break;
        }

        case 2: {
            // ProfileCaptureStatus()
            // Insert the current state of the OEM-defined profiler into the
            // CeLog data stream
            BYTE  Buf[1024];  
            DWORD dwBufSize = 1024;
            DWORD dwBufUsed;
	        
            // Only supported via CeLog for now
            if ( (g_dwProfilerFlags & PROFILE_OEMDEFINED) &&
            	 (g_dwProfilerFlags & PROFILE_CELOG) && 
            	 IsCeLogProfilingEnabled()) {

                ProfilerControl control;
                control.dwVersion = 1;
                control.dwOptions = PROFILE_OEM_QUERY;
                control.OEM.dwControlSize = 0;    // no OEM struct being passed here
                control.OEM.dwProcessorType = 0;  
                if (OEMIoControl(IOCTL_HAL_OEM_PROFILER, &control,
                                 sizeof(ProfilerControl), Buf, dwBufSize,
                                 &dwBufUsed)) {
                    // Clear the RA since it does not apply when the data is just being queried
                    ((OEMProfilerData*)Buf)->ra = 0;
        
                    // Now that we have the data, send it to CeLog
                    g_pfnCeLogData(TRUE, CELID_OEMPROFILER_HIT, Buf, dwBufUsed, 0,
                                   CELZONE_PROFILER, 0, FALSE);
                }
            }
            break;
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
#endif

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
    PROFILEMSG(1,(L"Resolution: %d.%3.3d usec per tick\r\n",calls/1000,calls%1000));
    KCall((PKFN)GetKCallProfile,&kprf,KCALL_NextThread,bReset);
    PROFILEMSG(1,(L"NextThread: Calls=%u Min=%u Max=%u Ave=%u\r\n",
        kprf.hits,local_ScaleDown(kprf.min),
        local_ScaleDown(kprf.max),kprf.hits ? local_ScaleDown(kprf.total)/kprf.hits : 0));
    for (loop = 0; loop < MAX_KCALL_PROFILE; loop++) {
        if (loop != KCALL_NextThread) {
            KCall((PKFN)GetKCallProfile,&kprf,loop,bReset);
            if (kprf.max > max)
                max = kprf.max;
            total+=kprf.total;
            calls+= kprf.hits;
        }
    }
    PROFILEMSG(1,(L"Other Kernel calls: Max=%u Avg=%u\r\n",max,calls ? local_ScaleDown(total)/calls : 0));
#else
    calls = local_ScaleDown(1000);
    PROFILEMSG(1,(L"Resolution: %d.%3.3d usec per tick\r\n",calls/1000,calls%1000));
    PROFILEMSG(1,(L"Index Entrypoint                        Calls      uSecs    Min    Max    Ave\r\n"));
    for (loop = 0; loop < MAX_KCALL_PROFILE; loop++) {
        KCall((PKFN)GetKCallProfile,&kprf,loop,bReset);
        PROFILEMSG(1,(L"%5d %-30s %8d %10d %6d %6d %6d\r\n",
            loop, pKCallName[loop], kprf.hits,local_ScaleDown(kprf.total),local_ScaleDown(kprf.min),
            local_ScaleDown(kprf.max),kprf.hits ? local_ScaleDown(kprf.total)/kprf.hits : 0));
        if (kprf.min && (kprf.min < min))
            min = kprf.min;
        if (kprf.max > max)
            max = kprf.max;
        calls += kprf.hits;
        total += kprf.total;
    }
    PROFILEMSG(1,(L"\r\n"));
    PROFILEMSG(1,(L"      %-30s %8d %10d %6d %6d %6d\r\n",
        L"Summary", calls,local_ScaleDown(total),local_ScaleDown(min),local_ScaleDown(max),calls ? local_ScaleDown(total)/calls : 0));
#endif
#endif
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Wrappers because these calls are functions on some processors and macros on
// others.
DWORD
Phys2VirtWrapper(
    DWORD pfn
    )
{
    return (DWORD) Phys2Virt(pfn);
}

BOOL
InSysCallWrapper(
    )
{
    return InSysCall();
}

BOOL FreeOneLibrary (PMODULE, BOOL);
BOOL CallDLLEntry (PMODULE pMod, DWORD Reason, LPVOID Reserved);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL KernelLibIoControl_Verifier(
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    )
{
    switch (dwIoControlCode) {
        case IOCTL_VERIFIER_IMPORT: {
            VerifierImportTable *pImports = (VerifierImportTable*)lpInBuf;

            extern const PFNVOID Win32Methods[];
            extern const PFNVOID ExtraMethods[];

            if ((nInBufSize != sizeof(VerifierImportTable))
                || (pImports->dwVersion != VERIFIER_IMPORT_VERSION)) {
                KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            pImports->dwVersion       = VERIFIER_IMPORT_VERSION;
            pImports->pWin32Methods   = (FARPROC*) Win32Methods;
            pImports->NKDbgPrintfW    = (FARPROC) NKDbgPrintfW;
            pImports->pExtraMethods = (FARPROC*) ExtraMethods;
            pImports->pKData          = &KData;
            pImports->KAsciiToUnicode = (FARPROC) KAsciiToUnicode;
            pImports->LoadOneLibraryW = (FARPROC) LoadOneLibraryW;
            pImports->FreeOneLibrary = (FARPROC) FreeOneLibrary;
            pImports->CallDLLEntry = (FARPROC) CallDLLEntry;
            pImports->AllocMem = (FARPROC) AllocMem;
            pImports->FreeMem = (FARPROC) FreeMem;
            pImports->AllocName = (FARPROC) AllocName;
            
            break;
        }
        case IOCTL_VERIFIER_REGISTER: {
            VerifierExportTable *pExports = (VerifierExportTable*)lpInBuf;

            if ((nInBufSize != sizeof(VerifierExportTable))
                || (pExports->dwVersion != VERIFIER_EXPORT_VERSION)) {
                KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            // Hook up the logging function pointers
            g_pfnInitLoaderHook = (PFN_INITLOADERHOOK) pExports->pfnInitLoaderHook;
            g_pfnWhichMod = (PFN_WHICHMOD) pExports->pfnWhichMod;
            g_pfnUndoDepends = (PFN_UNDODEPENDS) pExports->pfnUndoDepends;
            g_pfnVerifierIoControl = (PFN_VERIFIERIOCONTROL) pExports->pfnVerifierIoControl;

            break;
        }
        default:
            if (g_pfnVerifierIoControl) {
                return g_pfnVerifierIoControl (dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
            }
            return FALSE;
    }

    return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL KernelLibIoControl_CeLog(
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    )
{
    switch (dwIoControlCode) {

        case IOCTL_CELOG_IMPORT: {
            CeLogImportTable *pImports = (CeLogImportTable*)lpInBuf;

            extern const PFNVOID Win32Methods[];
            extern const PFNVOID EvntMthds[];
            extern const PFNVOID MapMthds[];
            extern DWORD dwDefaultThreadQuantum;

            if ((nInBufSize != sizeof(CeLogImportTable))
                || (pImports->dwVersion != CELOG_IMPORT_VERSION)) {
                ERRORMSG(1, (TEXT("CeLog DLL version does not match kernel\r\n")));
                KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            pImports->dwVersion         = CELOG_IMPORT_VERSION;
            pImports->pWin32Methods     = (FARPROC*) Win32Methods;
            pImports->pEventMethods     = (FARPROC*) EvntMthds;
            pImports->pMapMethods       = (FARPROC*) MapMthds;
            pImports->INTERRUPTS_ENABLE = (FARPROC) INTERRUPTS_ENABLE;
            pImports->Phys2Virt         = (FARPROC) Phys2VirtWrapper;
            pImports->InSysCall         = (FARPROC) InSysCallWrapper;
            pImports->KCall             = (FARPROC) KCall;
            pImports->NKDbgPrintfW      = (FARPROC) NKDbgPrintfW;
            pImports->pKData            = &KData;
            pImports->pdwCeLogTLBMiss   = &dwCeLogTLBMiss;
            pImports->dwCeLogLargeBuf   = dwCeLogLargeBuf;
            pImports->dwCeLogSmallBuf   = dwCeLogSmallBuf;
            pImports->dwDefaultThreadQuantum = dwDefaultThreadQuantum;

            break;
        }

        case IOCTL_CELOG_REGISTER: {
            CeLogExportTable *pExports = (CeLogExportTable*)lpInBuf;

            if (IsCeLogLoaded()) {
                KSetLastError(pCurThread, ERROR_ALREADY_INITIALIZED);
                return FALSE;
            }

            if ((nInBufSize != sizeof(CeLogExportTable))
                || (pExports->dwVersion != CELOG_EXPORT_VERSION)) {
                ERRORMSG(1, (TEXT("CeLog DLL version does not match kernel\r\n")));
                KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            // Hook up the logging function pointers
            g_pfnCeLogData       = pExports->pfnCeLogData;
            g_pfnCeLogInterrupt  = pExports->pfnCeLogInterrupt;
            g_pfnCeLogSetZones   = pExports->pfnCeLogSetZones;
            g_pfnCeLogQueryZones = pExports->pfnCeLogQueryZones;
            g_dwCeLogTimerFrequency = pExports->dwCeLogTimerFrequency;

            // Enable logging
            CeLogEnableGeneral();
            DEBUGMSG(1, (TEXT("CeLog is now live!\r\n")));

            break;
        }
        case IOCTL_CELOG_GETDESKTOPZONE: {
            if (lpInBuf && lpOutBuf && (nOutBufSize == sizeof(DWORD))) {
                DWORD i;
                extern DWORD g_dwKeys[HOST_TRANSCFG_NUM_REGKEYS];
                extern WCHAR* g_keyNames[];
                for (i = 0; i < HOST_TRANSCFG_NUM_REGKEYS; i++) {
                    if (strcmpW (lpInBuf, g_keyNames[i]) == 0) {
                        *(DWORD *)lpOutBuf = g_dwKeys[i];
                        break;
                    }
                }
            } else {
                return FALSE;
            }    
            break;
        }

        default:
            return FALSE;
    }

    return TRUE;
}


#ifdef CELOG  // Only present in profiling builds

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
CeLogInterrupt( 
    DWORD dwLogValue
    )
{
    // Do the IsCeLogEnabled() test here rather than in assembly because it does 
    // not affect non-profiling builds.  The function call(s) to get to this 
    // point are wasted work only on profiling builds in which CeLog is not 
    // currently active.
    if (IsCeLogEnabled()) {
        g_pfnCeLogInterrupt(dwLogValue);
    }
}

#endif // CELOG


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
CeLogThreadMigrate(
    HANDLE hProcess,
    DWORD dwReserved
    )
{
    CEL_THREAD_MIGRATE cl;

    // CeLog must be enabled and should not be in profile-only mode
    DEBUGCHK(KInfoTable[KINX_CELOGSTATUS] == CELOGSTATUS_ENABLED_GENERAL);

    cl.hProcess = hProcess;
    // Ignore the reserved DWORD for now

    g_pfnCeLogData(TRUE, CELID_THREAD_MIGRATE, &cl, sizeof(CEL_THREAD_MIGRATE),
                   0, CELZONE_MIGRATE, 0, FALSE);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_CeLogData( 
    BOOL  fTimeStamp,
    WORD  wID,
    VOID  *pData,
    WORD  wLen,
    DWORD dwZoneUser,
    DWORD dwZoneCE,
    WORD  wFlag,
    BOOL  fFlagged
    )
{
    if (IsCeLogEnabled()) {
        // Check caller buffer access
        if ((KERN_TRUST_FULL != pCurProc->bTrustLevel)
            && pData && wLen) {
            pData = SC_MapPtrWithSize(pData, wLen, SC_GetCallerProcess());
            DEBUGCHK(pData);
            if (!pData) {
                // Caller does not have access to the buffer.  Allow the event
                // to be logged, but without any data.
                wLen = 0;
            }
        }

        g_pfnCeLogData(fTimeStamp, wID, pData, wLen, dwZoneUser, dwZoneCE, wFlag, fFlagged);
    }
}


// CeLog zones available in all builds
#define AVAILABLE_ZONES_ALL_BUILDS                                              \
    (CELZONE_RESCHEDULE | CELZONE_MIGRATE | CELZONE_DEMANDPAGE | CELZONE_THREAD \
     | CELZONE_PROCESS | CELZONE_PRIORITYINV | CELZONE_CRITSECT | CELZONE_SYNCH \
     | CELZONE_HEAP | CELZONE_VIRTMEM | CELZONE_LOADER | CELZONE_MEMTRACKING    \
     | CELZONE_ALWAYSON | CELZONE_MISC | CELZONE_BOOT_TIME)
// CeLog zones available only in profiling builds
#define AVAILABLE_ZONES_PROFILING \
    (CELZONE_INTERRUPT | CELZONE_TLB | CELZONE_KCALL | CELZONE_PROFILER)


// Define the zones available on this kernel
#ifdef CELOG
#define AVAILABLE_ZONES (AVAILABLE_ZONES_ALL_BUILDS | AVAILABLE_ZONES_PROFILING)
#else
#define AVAILABLE_ZONES (AVAILABLE_ZONES_ALL_BUILDS)
#endif // CELOG


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_CeLogSetZones( 
    DWORD dwZoneUser,
    DWORD dwZoneCE,
    DWORD dwZoneProcess
    )
{
    if (IsCeLogLoaded()) {
        g_pfnCeLogSetZones(dwZoneUser, dwZoneCE, dwZoneProcess);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_CeLogGetZones( 
    LPDWORD lpdwZoneUser,        // User-defined zones
    LPDWORD lpdwZoneCE,          // Predefined zones
    LPDWORD lpdwZoneProcess,     // Process zones
    LPDWORD lpdwAvailableZones   // Zones supported by this kernel
    )
{
    BOOL fResult = FALSE;

    // Check caller buffer access
    if (SC_CeGetCallerTrust() != OEM_CERTIFY_TRUST) {
        lpdwZoneUser       = SC_MapPtrWithSize(lpdwZoneUser, sizeof(DWORD), SC_GetCallerProcess());
        lpdwZoneCE         = SC_MapPtrWithSize(lpdwZoneCE, sizeof(DWORD), SC_GetCallerProcess());
        lpdwZoneProcess    = SC_MapPtrWithSize(lpdwZoneProcess, sizeof(DWORD), SC_GetCallerProcess());
        lpdwAvailableZones = SC_MapPtrWithSize(lpdwAvailableZones, sizeof(DWORD), SC_GetCallerProcess());
    }

    if (IsCeLogLoaded()) {
        // Supply the available zones for this kernel
        if (lpdwAvailableZones)
            *lpdwAvailableZones = AVAILABLE_ZONES;

        // Call the CeLog DLL to get the current zones
        fResult = g_pfnCeLogQueryZones(lpdwZoneUser, lpdwZoneCE, lpdwZoneProcess);
    }

    return fResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CELOG RESYNC API:  This function generates a series of calls to CeLogData, 
// to log all currently-existing processes and threads.
//

// This buffer is used during resync to minimize stack usage
BYTE g_pCeLogSyncBuffer[(MAX_PATH * sizeof(WCHAR))
                        + max(sizeof(CEL_PROCESS_CREATE), sizeof(CEL_THREAD_CREATE))];

// Cause the scheduler's default thread quantum to be visible
extern DWORD dwDefaultThreadQuantum;


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Helper func to generate a celog process create event, minimizing stack usage.
// At most one of lpProcNameA, lpProcNameW can be non-null!
_inline static void 
CELOG_SyncProcess(
    HANDLE hProcess,
    DWORD  dwVMBase,
    LPWSTR lpProcName
    )
{
    PCEL_PROCESS_CREATE pcl = (PCEL_PROCESS_CREATE) &g_pCeLogSyncBuffer;
    WORD wLen = 0;

    // Special case: fixup nk.exe to tell where the code lives, not the VM
    if (dwVMBase == (SECURE_SECTION << VA_SECTION)) {
        dwVMBase = pTOC->physfirst;
    }
    
    pcl->hProcess = hProcess;
    pcl->dwVMBase = dwVMBase;

    if (lpProcName) {
        wLen = strlenW(lpProcName) + 1;
        kstrcpyW(pcl->szName, lpProcName);
    }

    g_pfnCeLogData(TRUE, CELID_PROCESS_CREATE, (PVOID) pcl,
                   (WORD) (sizeof(CEL_PROCESS_CREATE) + (wLen * sizeof(WCHAR))),
                   0, CELZONE_PROCESS | CELZONE_THREAD | CELZONE_MEMTRACKING | CELZONE_PROFILER,  // CELZONE_THREAD necessary for getting thread names
                   0, FALSE);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Helper func to generate a celog thread create event, minimizing stack usage.
_inline static void 
CELOG_SyncThread(
    PTHREAD pThread,
    HANDLE hProcess
    )
{
    if (pThread) {
        PCEL_THREAD_CREATE pcl = (PCEL_THREAD_CREATE) &g_pCeLogSyncBuffer;
        WORD wLen = 0;

        pcl->hProcess    = hProcess;
        pcl->hThread     = pThread->hTh;
        pcl->hModule     = (HANDLE)0;  // The reader can figure out hModule from dwStartAddr
        pcl->dwStartAddr = pThread->dwStartAddr;
        pcl->nPriority   = pThread->bBPrio;

#ifdef NKPROF  // GetThreadName only available in profiling builds
        GetThreadName(pThread, &(pcl->hModule), pcl->szName);
        if (pcl->szName[0] != 0) {
            wLen = strlenW(pcl->szName) + 1;
        }
#endif // NKPROF

        g_pfnCeLogData(TRUE, CELID_THREAD_CREATE, (PVOID) pcl, 
                       (WORD)(sizeof(CEL_THREAD_CREATE) + (wLen * sizeof(WCHAR))),
                       0, CELZONE_THREAD | CELZONE_MEMTRACKING | CELZONE_PROFILER, 0, FALSE);

        // Log priority inversion if necessary
        if (pThread->bBPrio != pThread->bCPrio) {
            PCEL_SYSTEM_INVERT pclInvert = (PCEL_SYSTEM_INVERT) &g_pCeLogSyncBuffer;
            
            pclInvert->hThread   = pThread->hTh;
            pclInvert->nPriority = pThread->bCPrio;

            g_pfnCeLogData(TRUE, CELID_SYSTEM_INVERT, (PVOID) pclInvert,
                           sizeof(CEL_SYSTEM_INVERT), 0, CELZONE_PRIORITYINV,
                           0, FALSE);
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Helper func to generate a celog module attach event, minimizing stack usage.
_inline static void 
CELOG_SyncModule(
    HANDLE hProcess,
    PMODULE pMod
    )
{
    if (pMod) {
        PCEL_MODULE_LOAD pcl = (PCEL_MODULE_LOAD) &g_pCeLogSyncBuffer;
        WORD wLen = 0;
        
        pcl->hProcess = hProcess;
        pcl->hModule  = (HANDLE)pMod;
        pcl->dwBase   = (DWORD)pMod->BasePtr;
        if (pMod->lpszModName) {
            wLen = strlenW(pMod->lpszModName) + 1;
            kstrcpyW(pcl->szName, pMod->lpszModName);
        }
        
        g_pfnCeLogData(TRUE, CELID_MODULE_LOAD, (PVOID) pcl,
                       (WORD)(sizeof(CEL_MODULE_LOAD) + (wLen * sizeof(WCHAR))),
                       0, CELZONE_LOADER | CELZONE_THREAD | CELZONE_MEMTRACKING | CELZONE_PROFILER,  // CELZONE_THREAD necessary for getting thread names
                       0, FALSE);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
SC_CeLogReSync()
{
    ACCESSKEY      ulOldKey;
    int            nProc;
    CEL_LOG_MARKER marker;
    PMODULE        pMod;

    // KCall so nobody changes ProcArray or pModList out from under us
    if (!InSysCall()) {
        return KCall((PKFN)SC_CeLogReSync);
    }

    KCALLPROFON(74);

    if (IsCeLogKernelEnabled()) {
        // Send a marker
        marker.dwFrequency = g_dwCeLogTimerFrequency;
        marker.dwDefaultQuantum = dwDefaultThreadQuantum;
        marker.dwVersion = CELOG_VERSION;
        g_pfnCeLogData(FALSE, CELID_LOG_MARKER, &marker, sizeof(CEL_LOG_MARKER),
                       0, CELZONE_ALWAYSON, 0, FALSE);


        // Since we're in a KCall, we must limit stack usage, so we can't call 
        // CELOG_ProcessCreate, CELOG_ThreadCreate, CELOG_ModuleLoad -- instead
        // use CELOG_Sync*.

        SWITCHKEY(ulOldKey, 0xffffffff); // Need access to touch procnames
        for (nProc = 0; nProc < 32; nProc++) {

            if (ProcArray[nProc].dwVMBase) {
                THREAD* pThread;

                // Log the process name.  It is safe to touch the process name
                // inside a KCall because it is always on the stack, ie. always
                // paged in.
                CELOG_SyncProcess(ProcArray[nProc].hProc,
                                  ProcArray[nProc].dwVMBase,
                                  ProcArray[nProc].lpszProcName);

                // Log the existence of each of the process' threads as a ThreadCreate
                pThread = ProcArray[nProc].pTh;
                while (pThread != NULL) {
                    CELOG_SyncThread(pThread, pThread->pOwnerProc->hProc);
                    pThread = pThread->pNextInProc;
                }
            }
        }
        SETCURKEY(ulOldKey);

        for (pMod = pModList; pMod; pMod = pMod->pMod) {
            CELOG_SyncModule(INVALID_HANDLE_VALUE, pMod);
        }

        // Send a sync end marker
        g_pfnCeLogData(FALSE, CELID_SYNC_END, NULL, 0, 0, CELZONE_ALWAYSON,
                       0, FALSE);
    }

    KCALLPROFOFF(74);

    return TRUE;
}


#ifdef NKPROF  // TOC info available only in profiling builds

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//
//  Functions used to look up symbol information included in the TOC in
//  profiling builds.
//
//  GetFunctionName and GetModName are used by schedlog.  GetSymbolText is used
//  by memtrk.  The rest of the functions below serve as support for these.
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
            if (pModMax->oe.filetype == FT_ROMIMAGE) {
                *dwResult = (DWORD) pModMax;
                return MODULEPOINTER_DLL;
            }
        }
    } else {
        //
        // We assume it's a process
        //
        if (pth->pOwnerProc->oe.filetype == FT_ROMIMAGE) {
            *dwResult = (DWORD) (pth->pOwnerProc);
            return MODULEPOINTER_PROC;
        }
    }

    return (MODULEPOINTER_ERROR);
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
    DWORD dwModulePointer = (DWORD) NULL;
    DWORD dwMPFlag = GetModulePointer(pth, pc, &dwModulePointer);

    switch (dwMPFlag) {
        case MODULEPOINTER_NK: {
            // nk.exe: tocptr-> first entry in ROM
            tocptr=(TOCentry *)(pTOC+1);
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

    return (tocptr);
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
    DWORD dwModulePointer = (DWORD) NULL;
    DWORD dwMPFlag = GetModulePointer(pth, pc, &dwModulePointer);

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

    return (hModule);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPCSTR
GetModName(
    PTHREAD pth,
    unsigned int pc
    ) 
{
    LPCSTR pszModName = NULL;
    TOCentry* tocptr;

    tocptr = GetTocPointer(pth, pc);
    if (tocptr != NULL) {
        pszModName = tocptr->lpszFileName;
    }

    return (pszModName);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPBYTE
GetSymbolText(
    LPBYTE lpSym,
    DWORD dwSymNum
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
    LPCSTR      pszFunctionName = NULL;
    TOCentry    *tocptr;                // table of contents entry pointer
    PROFentry   *profptr = NULL;        // profile entry pointer
    SYMentry    *symptr;                // profiler symbol entry pointer
    unsigned int iMod;                  // module enty in TOC
    unsigned int iSym;                  // symbol counter

    if (!pTOC->ulProfileOffset) {
        //
        // No symbols available.
        //
        return (NULL);
    }

    if (IsKernelVa(pc)) {
        //
        // These addresses belong to NK.EXE.
        //
        if (pc >= 0xA0000000) {
            //
            // Hit a PSL exception address or perhaps a KDATA routine. No name.
            //
            return (NULL);
        }
        //
        // Mask off the caching bit.
        //
        pc &= 0xdfffffff;
    }

    //
    // Find the TOC entry if one exists. If not in ROM, then we can't get a name.
    //
    tocptr = GetTocPointer(pth, pc);
    if (tocptr == NULL) {
        return (NULL);
    }

    //
    // Compute the module number
    //
    iMod = ((DWORD)tocptr - (DWORD)(pTOC+1)) / sizeof(TOCentry);
    //
    // make profptr point to entry for this module
    //
    profptr  = (PROFentry *)pTOC->ulProfileOffset;
    profptr += iMod;

    //
    // If there are any symbols in this module, scan for the function.
    //
    if (profptr->ulNumSym) {
        unsigned int iClosestSym;            // index of nearest symbol entry
        SYMentry*    pClosestSym;            // ptr to nearest symbol entry

        iSym = 0;
        symptr = (SYMentry*) profptr->ulHitAddress;
        iClosestSym = iSym;
        pClosestSym = symptr;

        //
        // Scan the whole list of symbols, looking for the closest match.
        //
        while ((iSym < profptr->ulNumSym) && (pClosestSym->ulFuncAddress != pc)) {
            // Keep track of the closest symbol found
            if ((symptr->ulFuncAddress <= pc)
                && (symptr->ulFuncAddress > pClosestSym->ulFuncAddress)) {
                iClosestSym = iSym;
                pClosestSym = symptr;
            }

            iSym++;
            symptr++;
        }

        pszFunctionName = (LPCSTR) GetSymbolText((LPBYTE) profptr->ulSymAddress, iClosestSym);
    }

    return (pszFunctionName);
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
                    && (pth->pOwnerProc->oe.filetype == FT_ROMIMAGE)
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
