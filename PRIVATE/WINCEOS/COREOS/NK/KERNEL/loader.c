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
//      NK Kernel loader code
//
//
// Module Name:
//
//      loader.c
//
// Abstract:
//
//      This file implements the NK kernel loader for EXE/DLL
//
//
//------------------------------------------------------------------------------
//
// The loader is derived from the Win32 executable file format spec.  The only
// interesting thing is how we load a process.  When we load a process, we
// simply schedule a thread in a new process context.  That thread (in coredll)
// knows how to read in an executable, and does so.  Then it traps into the kernel
// and starts running in the new process.  Most of the code below is simply a direct
// implimentation of the executable file format specification
//
//------------------------------------------------------------------------------

#include "kernel.h"
#include "pager.h"
#include "bldver.h"
#include "dbgrapi.h"
#include "cepolicy.h"
#include <oalioctl.h>

#define SYSTEMDIR L"\\Windows\\"
#define SYSTEMDIRLEN 9

#define DBGDIR    L"\\Release\\"
#define DBGDIRLEN    9

ERRFALSE(offsetof(MODULELIST, pMod) == offsetof(MODULE, lpSelf));
ERRFALSE(offsetof(MODULELIST, wRefCnt) == offsetof(MODULE, wRefCnt));
ERRFALSE(offsetof(MODULELIST, wFlags) == offsetof(MODULE, wFlags));
ERRFALSE(IsDwordAligned (offsetof(MODULELIST, wRefCnt)));

fslog_t *LogPtr;
BOOL fForceCleanBoot;
BOOL g_fNoKDebugger; // Prevents Kernel Debugger to be loaded
BOOL g_fKDebuggerLoaded; // Indicates that the Kernel Debugger is already loaded

FREEINFO FreeInfo[MAX_MEMORY_SECTIONS];
MEMORYINFO MemoryInfo;

ROMChain_t FirstROM;
ROMChain_t *ROMChain;

PROMINFO g_pROMInfo;

// Page-out functionality
extern CRITICAL_SECTION PageOutCS;
#ifdef DEBUG
extern CRITICAL_SECTION MapCS;  // Used to sanity-check CS ordering on debug builds
#endif

DLIST   g_thrdCallList;

extern CRITICAL_SECTION ModListcs;

#ifdef x86
PVOID g_pCoredllFPEmul;
PVOID g_pKCoredllFPEmul;
#endif

// OEMs can update this via fixupvar in config.bib file under platform
// kernel.dll:initialKernelLogZones 00000000 00000100 FIXUPVAR
DWORD initialKernelLogZones=0x0100;

// OAL ioctl module entry point
PFN_Ioctl g_pfnOalIoctl;

void LockModuleList (void)
{
    EnterCriticalSection (&ModListcs);
}

void UnlockModuleList (void)
{
    LeaveCriticalSection (&ModListcs);
}

BOOL OwnModListLock (void)
{
    return OwnCS (&ModListcs);
}

static void UnlockModule (PMODULE pMod, BOOL fCallDllMain);

extern CRITICAL_SECTION PagerCS;
extern PMODULE LoadMUI (HANDLE hModule, LPBYTE BasePtr, e32_lite* eptr);

HMODULE hCoreDll, hKCoreDll;
HMODULE hKernDll, hKitlDll;
void (*TBFf)(LPVOID, ulong);
void (*KTBFf)(LPVOID, ulong);   // kernel thread base function
void (*MTBFf)(LPVOID, ulong, ulong, ulong, ulong);
void (*pSignalStarted)(DWORD);
DWORD (WINAPI *pfnUsrGetHeapSnapshot) (THSNAP *pSnap);
DWORD (WINAPI *pfnKrnGetHeapSnapshot) (THSNAP *pSnap);
DWORD (WINAPI *pfnCeGetCanonicalPathNameW)(LPCWSTR, LPWSTR, DWORD, DWORD);

LPDWORD pKrnMainHeap;
LPDWORD pUsrMainHeap;

DWORD PagedInCount;
DWORD (*pNKEnumExtensionDRAM)(PMEMORY_SECTION pMemSections, DWORD cMemSections);
DWORD (*pOEMCalcFSPages)(DWORD dwMemPages, DWORD dwDefaultFSPages);
DWORD dwOEMCleanPages;

PFN_GetProcAddrA g_pfnGetProcAddrA;
PFN_GetProcAddrW g_pfnGetProcAddrW;
PFN_DoImport     g_pfnDoImports;
PFN_UndoDepends  g_pfnUndoDepends;
PFN_AppVerifierIoControl g_pfnAppVerifierIoControl;
PFN_FindResource g_pfnFindResource;


extern PNAME pSearchPath;

PNAME pDbgList;

#ifndef x86
FARPROC g_pfnRtlUnwindOneFrame;
#endif

#ifdef SH3
extern unsigned int SH3DSP;
#endif

typedef struct {
    DWORD dwFlags;
    LPCWSTR pszDllName;
} LdrModuleData, *PLdrModuleData;

int strcmponeiW(const wchar_t *pwc1, const wchar_t *pwc2) {
    while (*pwc1 && (NKtowlower(*pwc1) == *pwc2)) {
        pwc1++;
        pwc2++;
    }
    return (*pwc1 ? 1 : 0);
}

int strcmpdllnameW(LPCWSTR src, LPCWSTR tgt) {
    while (*src && (*src == NKtowlower(*tgt))) {
        src++;
        tgt++;
    }
    return ((*tgt && strcmponeiW(tgt,L".dll") && strcmponeiW(tgt,L".cpl")) || (*src && memcmp(src,L".dll",10) && memcmp(src,L".cpl",10))) ? 1 : 0;
}


DWORD CopyPath (LPWSTR pDst, LPCWSTR pSrc, DWORD cchLen)
{
    DWORD cchRet = 0;

    PREFAST_DEBUGCHK ((int) cchLen > 0);

    for ( ; (cchRet + 1 < cchLen) && pSrc[cchRet]; cchRet ++) {
        pDst[cchRet] = pSrc[cchRet];
    }
    pDst[cchRet] = 0;   // EOS
    return pSrc[cchRet]? 0 : cchRet;    // return 0 if source is longer than we can hold.
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
o32_lite *FindOptr (o32_lite *optr, int nCnt, DWORD dwAddr)
{
    for ( ; nCnt; nCnt --, optr ++) {
        if ((DWORD) (dwAddr - optr->o32_realaddr) < optr->o32_vsize) {
            return optr;
        }
    }

    return NULL;
}

//------------------------------------------------------------------------------
//
// For NK and Modules with Z flag (fixed up to static mapped address), RO and RW data are
// seperated and the size of RO section is smaller than the vsize specified in
// e32 header. Debugger and exception handler get confused because we then sees NK and Kernal DLLs
// overlapping with each other. This function is to re-caculate the size to reflect
// the real size of the so debugger will work on kernel libraries
//
//------------------------------------------------------------------------------
void UpdateKmodVSize (openexe_t *oeptr, e32_lite *eptr)
{

    if (FA_DIRECTROM & oeptr->filetype) {

        unsigned long dwEndAddr = 0;
        int loop;
        o32_rom *o32rp;
        for (loop = 0; loop < eptr->e32_objcnt; loop++) {
            o32rp = (o32_rom *)(oeptr->tocptr->ulO32Offset+loop*sizeof(o32_rom));
            // find the last RO section
            if ((o32rp->o32_dataptr == o32rp->o32_realaddr)
                && (o32rp->o32_realaddr > dwEndAddr)) {
                dwEndAddr = o32rp->o32_realaddr + o32rp->o32_vsize;
            }
        }

        // compressed Kernel modules will have dwEndAddr == 0
        if (dwEndAddr) {
            // page align the end addr
            dwEndAddr = PAGEALIGN_UP (dwEndAddr);
            eptr->e32_vsize = dwEndAddr - eptr->e32_vbase;
            DEBUGMSG (1, (L"Updated eptr->e32_vsize to = %8.8lx\r\n", eptr->e32_vsize));
        }
    }
}



ERRFALSE(!OEM_CERTIFY_FALSE);

static BOOL MatchMod (PDLIST pItem, LPVOID pEnumData)
{
    return ((PMODULELIST) pItem)->pMod == (PMODULE) pEnumData;
}

static BOOL MatchDllNameAndFlags (PDLIST pItem, LPVOID pEnumData)
{
    LPCWSTR pszName = ((PLdrModuleData)pEnumData)->pszDllName;
    DWORD dwFlags = ((PLdrModuleData)pEnumData)->dwFlags;
    PMODULE pMod = ((PMODULELIST) pItem)->pMod;
    LPCWSTR pszDllName = pMod->lpszModName;
    WCHAR ch = NKtowlower (pszName[0]);

    if (((int) pMod->BasePtr < 0)           // kernel DLL
        && ('k' == pszDllName[0])           // and start with "k."
        && ('.' == pszDllName[1])
        && (('k' != ch) || ('.' != pszName[1]))) {
        pszDllName += 2;                    // skip "k." if pszName doesn't start with "k."
    }

    DEBUGMSG (ZONE_LOADER1, (L"MatchDllNameAndFlags: testing '%s'\r\n", pMod->lpszModName));
    return (   (ch == pszDllName[0])
            && (   !(pMod->wFlags & LOAD_LIBRARY_AS_DATAFILE)   // current module is not loaded as data file, or
                || (dwFlags & LOAD_LIBRARY_AS_DATAFILE)         // request to load the dll as data file, or
                || !pMod->e32.e32_unit[EXP].rva)                // current module is a resource only DLL
            && !strcmpdllnameW (pszDllName, pszName));
}

__inline BOOL IsAddrInMod (PMODULE pMod, DWORD dwAddr)
{
    return (DWORD) (dwAddr - (DWORD) pMod->BasePtr) < pMod->e32.e32_vsize;
}

static BOOL MatchAddr (PDLIST pItem, LPVOID pEnumData)
{
    return IsAddrInMod (((PMODULELIST) pItem)->pMod, (DWORD) pEnumData);
}

static PMODULELIST EnumModInProc (PPROCESS pprc, BOOL fUMode, PFN_DLISTENUM pfnApply, LPVOID pEnumData)
{
    PDLIST ptrav;
    DEBUGCHK (OwnModListLock ());
    if (fUMode) {
        // user address, search forward
        for (ptrav = pprc->modList.pFwd; ptrav != &pprc->modList; ptrav = ptrav->pFwd) {

            if ((int) ((PMODULELIST) ptrav)->pMod->BasePtr < 0) {
                DEBUGCHK (pprc == g_pprcNK);
                break;
            }
            if ((*pfnApply) (ptrav, pEnumData)) {
                // maintain MRU
                RemoveDList (ptrav);
                AddToDListHead (&pprc->modList, ptrav);
                return (PMODULELIST) ptrav;
            }
        }
    } else if (pprc == g_pprcNK) {
        // kernel address, search backward
        for (ptrav = pprc->modList.pBack; ptrav != &pprc->modList; ptrav = ptrav->pBack) {
            if ((int) ((PMODULE) ptrav)->BasePtr > 0) {
                break;
            }
            if ((*pfnApply) (ptrav, pEnumData)) {
                // maintain MRU
                RemoveDList (ptrav);
                AddToDListTail (&pprc->modList, ptrav);
                return (PMODULELIST) ptrav;
            }
        }
    }
    return NULL;
}


//
// FindModInProc - find a module in a process by module handle
//
PMODULELIST FindModInProc (PPROCESS pprc, PMODULE pMod)
{
    PMODULELIST pEntry;

    DEBUGCHK (OwnModListLock ());
    pEntry = IsValidModule (pMod)
        ? EnumModInProc (pprc, ((int) pMod->BasePtr > 0), MatchMod, pMod)
        : NULL;

    return pEntry;
}

//
// FindModInProcByName - find a module in process by name
//
PMODULELIST FindModInProcByName (PPROCESS pprc, LPCWSTR pszName, BOOL fUMode, DWORD dwFlags)
{
    LdrModuleData ModData = {dwFlags, pszName};
    DEBUGCHK (OwnModListLock ());
    return EnumModInProc (pprc, fUMode, MatchDllNameAndFlags, (LPVOID) &ModData);
}

//
// FindModInProcByAddr - find a module in process by address
//
PMODULE FindModInProcByAddr (PPROCESS pprc, DWORD dwAddr)
{
    PMODULELIST pEntry;
    DEBUGCHK (OwnModListLock ());
    pEntry = EnumModInProc (pprc, ((int) dwAddr > 0), MatchAddr, (LPVOID) dwAddr);

    return pEntry? pEntry->pMod : NULL;
}

//
// remove reference to a module from a process
//
void FreeModFromProc (PPROCESS pprc, PMODULELIST pEntry)
{
    PMODULE pMod = pEntry->pMod;
    DEBUGCHK (OwnLoaderLock (pprc));

    if (!(DONT_RESOLVE_DLL_REFERENCES & pMod->wFlags)) {
        (* HDModUnload) ((DWORD) pMod);

        // notify the application debugger
        if (pprc->pDbgrThrd) {
            DbgrNotifyDllUnload (pCurThread, pprc, pMod);
        }
    }

    DEBUGCHK (pprc != g_pprcNK);

    // Log a free event for the current process on its last unload
    CELOG_ModuleFree (pprc, pMod);

    // free per-process Module memory
    LockModuleList ();
    RemoveDList (&pEntry->link);
    UnlockModuleList ();

    VMFreeAndRelease (pprc, pMod->BasePtr, pMod->e32.e32_vsize);
    FreeMem (pEntry, HEAP_MODLIST);

    // call unlock VM to decrement the global ref-cnt
    UnlockModule (pMod, FALSE);
}

//
// FreeAllProcLibraries - free all modules in current process (called on process exit)
//
void FreeAllProcLibraries (PPROCESS pprc)
{
    PDLIST ptrav;
    DEBUGCHK (pprc != g_pprcNK);

    LockLoader (pprc);
    while ((ptrav = pprc->modList.pFwd) != &pprc->modList) {
        FreeModFromProc (pprc, (PMODULELIST) ptrav);
    }
    UnlockLoader (pprc);
}

PMODULE FindModInKernel (PMODULE pMod)
{
    LockModuleList ();
    pMod = (PMODULE) FindModInProc (g_pprcNK, pMod);
    UnlockModuleList ();
    return pMod;
}

static void AddToThreadNotifyList (PMODULELIST pml, PMODULE pMod)
{
    DEBUGCHK (OwnLoaderLock (g_pprcNK));
    RETAILMSG (1, (L"Kernel DLL '%s' needs thread creation/deletion notification\r\n", pMod->lpszModName));

    pml->pMod = pMod;
    AddToDListHead (&g_thrdCallList, &pml->link);
}

static void RemoveFromThreadNotifyList (PMODULE pMod)
{
    PDLIST pml = EnumerateDList (&g_thrdCallList, MatchMod, pMod);
    DEBUGCHK (OwnLoaderLock (g_pprcNK));
    DEBUGCHK (pml || (MF_LOADING & pMod->wFlags));
    if (pml) {
        RemoveDList (pml);
        FreeMem (pml, HEAP_MODLIST);
    }
}

static
BOOL EnumCallDllMain (PDLIST pItem, LPVOID pEnumData)
{
    PMODULE pMod = ((PMODULELIST) pItem)->pMod;

    DEBUGCHK (((MF_IMPORTED|MF_NO_THREAD_CALLS) & pMod->wFlags) == MF_IMPORTED);

    __try {
        ((dllntry_t) pMod->startip) ((HMODULE) pMod, (DWORD) pEnumData, 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // empty handler
    }

    // always return false to keep the enumeration going
    return FALSE;
}

void DllThreadNotify (DWORD dwReason)
{
    DEBUGCHK (pActvProc == g_pprcNK);
    DEBUGCHK (pCurThread->pprcOwner == g_pprcNK);
    // don't try to grab CS if the list is empty
    if (!IsDListEmpty (&g_thrdCallList)) {
        LockLoader (g_pprcNK);
        EnumerateDList (&g_thrdCallList, EnumCallDllMain, (LPVOID) dwReason);
        UnlockLoader (g_pprcNK);
    }
}

PMODULELIST InitModInProc (PPROCESS pprc, PMODULE pMod);

//------------------------------------------------------------------------------
// check if a particular module can be debugged
//------------------------------------------------------------------------------
static BOOL ChkDebug (openexe_t *oeptr)
{
    BOOL fRet = TRUE;

    if (FA_PREFIXUP & oeptr->filetype) {

        PPROCESS pprc     = pActvProc;
        DWORD    dwAttrib = (FA_DIRECTROM & oeptr->filetype)? oeptr->tocptr->dwFileAttributes : oeptr->dwExtRomAttrib;

        if (dwAttrib & MODULE_ATTR_NODEBUG) {
            if (pprc->pDbgrThrd || pprc->DbgActive || g_fKDebuggerLoaded) {
                fRet = FALSE;
            } else {
                // set both the process flag and the global flag to prevent loading KD
                // or attaching debugger to this process
                pprc->fNoDebug = g_fNoKDebugger = TRUE;
            }
        }
    }
    DEBUGMSG(ZONE_LOADER1,(TEXT("ChkDebug returns %d\r\n"), fRet));

    return fRet;
}

//------------------------------------------------------------------------------
// check if we should load a module from debug directory
//------------------------------------------------------------------------------
BOOL IsInDbgList (LPCWSTR pszName)
{
    if (pDbgList) {
        LPCWSTR pTrav = pDbgList->name;
        do {
            if (!NKwcsicmp (pszName, pTrav)) {
                return TRUE;
            }
            pTrav += NKwcslen (pTrav) + 1;
        } while (*pTrav);
    }
    return FALSE;
}


//------------------------------------------------------------------------------
// update the list for modules to be loaded from debug directory
// Note: nLen is the total length, including double 0 at the end
//------------------------------------------------------------------------------
BOOL SetDbgList (LPCWSTR pList, int nTotalLen)
{

    PNAME  pNewList = NULL;

    if (pList) {
        int     idx = 0;

        // validate the list
        if (nTotalLen < 3) {    // at least 2 zeros + a charactor
            return FALSE;
        }

        // make sure it's in multi-sz format
        do {
            idx += NKwcslen (pList + idx) + 1;
        } while ((idx < nTotalLen) && pList[idx]);

        if ((idx > nTotalLen) || pList[idx]) {
            return FALSE;
        }

        // allocate memory
        if (!(pNewList = AllocName (nTotalLen * sizeof (WCHAR)))) {
            // out of memory or list too big
            return FALSE;
        }

        // copy the list
        memcpy (pNewList->name, pList, nTotalLen * sizeof(WCHAR));
    }

    // update the list
    LockLoader (g_pprcNK);

    if (pDbgList) {
        FreeName (pDbgList);
    }
    pDbgList = pNewList;

    UnlockLoader (g_pprcNK);

    return TRUE;
}

//------------------------------------------------------------------------------
// read O32/E32 from filesys for FT_EXTIMAGE/FT_EXTXIP
//------------------------------------------------------------------------------
BOOL ReadExtImageInfo (HANDLE hf, DWORD dwCode, DWORD cbSize, LPVOID pBuf)
{
    DWORD cbRead;
    return FSIoctl (hf, dwCode, NULL, 0, pBuf, cbSize, &cbRead, NULL)
        && (cbSize == cbRead);
}

static DWORD CanonicalPathName (
    LPCWSTR lpPathName,
    LPWSTR lpCanonicalPathName,
    DWORD cchCanonicalPathName
)
{
    DWORD dwRet = 0;
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    __try {
        dwRet = (*pfnCeGetCanonicalPathNameW) (lpPathName, lpCanonicalPathName, cchCanonicalPathName, 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    SwitchActiveProcess (pprc);

    return dwRet;
}

//------------------------------------------------------------------------------
//  OpenFileFromFilesys: call into filesys to open a file
//
//  return: 0                       -- success
//          ERROR_FILE_NOT_FOUND    -- file doesn't exist
//          ERROR_FILE_CORRUPT      -- file is corrupted
//------------------------------------------------------------------------------

static DWORD OpenFileFromFilesys (
    LPCWSTR     lpszName,
    openexe_t   *oeptr,
    HANDLE      hRefTok,
    HANDLE      *phTokRet,
    DWORD       dwPolicyFlags
    )
{

    DWORD dwErr = 0;

    // try open the file
    oeptr->hf = FSOpenModuleByPolicy (hRefTok, lpszName, dwPolicyFlags, phTokRet);

    if (INVALID_HANDLE_VALUE == oeptr->hf) {
        dwErr = GetLastError();
        if (ERROR_SUCCESS == dwErr) {
            dwErr = ERROR_FILE_NOT_FOUND;
        }

    } else {

        DWORD bytesread;
        WCHAR wszName[MAX_PATH];
        BY_HANDLE_FILE_INFORMATION bhfi;

        // default to object store
        oeptr->filetype = FT_OBJSTORE;

        DEBUGLOG (ZONE_LOADER1, oeptr->hf);

        if (!FSGetFileInfo (oeptr->hf, &bhfi)) {
            // something wrong with the file
            DEBUGMSG (ZONE_OPENEXE, (L"OpenFileFromFilesys: FSGetFileInfo Failed!!\r\n"));
            dwErr = ERROR_FILE_CORRUPT;

        } else if (!CanonicalPathName (lpszName, wszName, MAX_PATH)
                   || !(oeptr->lpName = DupStrToPNAME (wszName))) {

            dwErr = ERROR_OUTOFMEMORY;

        } else {

            // is this an extern ROM MODULE?
            if (bhfi.dwFileAttributes & FILE_ATTRIBUTE_ROMMODULE) {
                // external MODULE (BINFS or IMGFS MODULEs)
                oeptr->filetype = FT_EXTIMAGE;
                oeptr->dwExtRomAttrib = bhfi.dwFileAttributes;

            } else {
                // regular file
                DWORD offset;  // Local var instead of ReadFile into static-mapped addr

                if (!FSReadFileWithSeek (oeptr->hf,0,0,0,0,0,0)) {
                    // can't page from media that doesn't support paging even if paging is allowed
                    oeptr->pagemode = PM_CANNOT_PAGE;
                }

                // read the offset information from PE header
                if ((FSSetFilePointer (oeptr->hf, 0x3c, 0, FILE_BEGIN) != 0x3c)
                    || !FSReadFile (oeptr->hf, (LPBYTE)&offset, sizeof(DWORD), &bytesread, 0)
                    || (bytesread != sizeof(DWORD))) {
                    DEBUGMSG (ZONE_OPENEXE, (L"OpenFileFromFilesys: FSSetFilePointer Failed!!\r\n"));
                    dwErr = ERROR_FILE_CORRUPT;
                } else {
                    oeptr->offset = offset;
                }
            }
        }

        if (dwErr) {
            CloseExe(oeptr);
        }
    }

    DEBUGMSG(ZONE_OPENEXE,(TEXT("OpenFileFromFilesys %s: returns: %8.8lx, oeptr->hf = %8.8lx\r\n"), lpszName, dwErr, oeptr->hf));

    return dwErr;
}



//--------------------------------------------------------------------
// FindDllEntry - find the entry point to a ROM Dll
//----------------------------------------------------------------------
DWORD FindROMDllEntry (const ROMHDR *pTOC, LPCSTR pszName)
{
    ULONG       loop;
    e32_rom     *e32rp = NULL;
    TOCentry    *tocptr;

    // find kernel.dll (case sensitive)
    tocptr = (TOCentry *)(pTOC+1);  // toc entries follow the header

    for (loop = 0; loop < pTOC->nummods; loop++) {

        if (_stricmp (tocptr->lpszFileName, pszName) == 0) {
            e32rp = (e32_rom *)(tocptr->ulE32Offset);
            break;
        }
        tocptr++;
    }

    return e32rp? (e32rp->e32_vbase + e32rp->e32_entryrva) : 0;
}


static DWORD OpenFileFromROM (LPCWSTR pPlainName, openexe_t *oeptr)
{
    ROMChain_t *pROM;
    o32_rom     *o32rp;
    TOCentry    *tocptr;
    int         loop;

    DEBUGMSG (ZONE_LOADER2, (L"OpenFileFromROM: trying '%s'\r\n", pPlainName));
    for (pROM = ROMChain; pROM; pROM = pROM->pNext) {
        // check ROM for any copy
        tocptr = (TOCentry *)(pROM->pTOC+1);  // toc entries follow the header
        for (loop = 0; loop < (int)pROM->pTOC->nummods; loop++) {
            if (!NKstrcmpiAandW(tocptr->lpszFileName, pPlainName)) {
                DEBUGMSG(ZONE_OPENEXE,(TEXT("OpenExe %s: ROM: %8.8lx\r\n"),pPlainName,tocptr));
                o32rp = (o32_rom *)(tocptr->ulO32Offset);
                DEBUGMSG(ZONE_LOADER1,(TEXT("(10) o32rp->o32_realaddr = %8.8lx\r\n"), o32rp->o32_realaddr));
                oeptr->tocptr = tocptr;
                oeptr->filetype = FT_ROMIMAGE;
                return 0;
            }
            tocptr++;
        }
    }
    return ERROR_FILE_NOT_FOUND;
}

static DWORD TryDir (openexe_t *oeptr, LPCWSTR pDirName, DWORD nDirLen, LPCWSTR pFileName, DWORD nFileLen, HANDLE hTokRef, HANDLE *phTokRet, DWORD dwPolicyFlags)
{
    WCHAR tmpname[MAX_PATH];

    PREFAST_DEBUGCHK (nDirLen <= MAX_PATH);
    PREFAST_DEBUGCHK (nFileLen <= MAX_PATH);

    if (nDirLen + nFileLen >= MAX_PATH) {
        // fail if path too long
        return ERROR_FILE_NOT_FOUND;
    }

    // construct the path
    memcpy (tmpname, pDirName, nDirLen * sizeof(WCHAR));
    memcpy (&tmpname[nDirLen], pFileName, nFileLen * sizeof(WCHAR));
    tmpname [nDirLen + nFileLen] = 0;
    DEBUGMSG (ZONE_LOADER2, (L"TryDir: trying '%s'\r\n", tmpname));

    return OpenFileFromFilesys (tmpname, oeptr, hTokRef, phTokRet, dwPolicyFlags);
}

static LPCWSTR GetPlainName (LPCWSTR pFullPath)
{
    LPCWSTR pPlainName = pFullPath + NKwcslen (pFullPath);
    while ((pPlainName != pFullPath) && (*(pPlainName-1) != (WCHAR)'\\') && (*(pPlainName-1) != (WCHAR)'/')) {
        pPlainName --;
    }
    return pPlainName;
}

static DWORD TrySameDir (openexe_t *oeptr, LPCWSTR lpszFileName, PPROCESS pProc, HANDLE hTok, HANDLE phTokRet, DWORD dwPolicyFlags)
{
    LPCWSTR     pProcPath = pProc->oe.lpName->name;
    return TryDir (oeptr, pProcPath, GetPlainName (pProcPath) - pProcPath, lpszFileName, NKwcslen (lpszFileName), hTok, phTokRet, dwPolicyFlags);
}

//
// open an executable
//      dwFlags consists of the following bits
//          - wFlags fields of DLL if we're opening a DLL
//          - LDROE_ALLOW_PAGING, if paging is allowed
//
DWORD OpenExecutable (PPROCESS pprc, LPCWSTR lpszName, openexe_t *oeptr, HANDLE hTok, HANDLE *phTokRet, DWORD dwFlags)
{
    BOOL bIsAbs = ((*lpszName == '\\') || (*lpszName == '/'));
    DWORD dwErr = ERROR_FILE_NOT_FOUND;
    DWORD dwLastErr = KGetLastError (pCurThread);
    int nTotalLen = NKwcslen (lpszName);
    LPCWSTR pPlainName;
    BOOL bIsInWindDir;
    DWORD dwPolicyFlags;


    DEBUGMSG (ZONE_OPENEXE,(TEXT("OpenExecutable: %s\r\n"),lpszName));

    if (MAX_PATH <= nTotalLen) {
        // path too long
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        return ERROR_INVALID_PARAMETER;
    }

    dwPolicyFlags = (pprc? POLICY_MODULE_OPEN_LOADLIBRARY : POLICY_MODULE_OPEN_EXECUTE)
                  | ((LDROE_OPENDEBUG & dwFlags)? POLICY_MODULE_OPEN_DEBUG : 0)
                  | ((LOAD_LIBRARY_AS_DATAFILE & dwFlags)? POLICY_MODULE_OPEN_DATAFILE : 0)
                  | ((LOAD_LIBRARY_IN_KERNEL & dwFlags)? POLICY_MODULE_OPEN_LOADKERNEL : 0);


    memset (oeptr, 0, sizeof (openexe_t));
    oeptr->pagemode = (pTOC->ulKernelFlags & KFLAG_DISALLOW_PAGING)
                        ? PM_CANNOT_PAGE
                        : ((dwFlags & LDROE_ALLOW_PAGING)? PM_FULLPAGING : PM_NOPAGING);


    // find the file name part of lpszName
    pPlainName = GetPlainName (lpszName);

    DEBUGMSG (ZONE_OPENEXE,(TEXT("OpenExecutable: plain name = %s\r\n"), pPlainName));

    // see if we're looking for file in Windows directory
    bIsInWindDir = ((pPlainName == lpszName + SYSTEMDIRLEN)     && !NKwcsnicmp(lpszName,SYSTEMDIR,SYSTEMDIRLEN))
                || ((pPlainName == lpszName + SYSTEMDIRLEN - 1) && !NKwcsnicmp(lpszName,SYSTEMDIR+1,SYSTEMDIRLEN-1));

    // load from ROM if filesys is not ready
    if (!SystemAPISets[SH_FILESYS_APIS]) {
        DEBUGMSG (ZONE_OPENEXE,(TEXT("OpenExecutable: Filesys not loaded, Opening from ROM\r\n"), pPlainName));
        dwErr = OpenFileFromROM (pPlainName, oeptr);

    } else if (bIsAbs) {

        DEBUGMSG (ZONE_OPENEXE,(TEXT("OpenExecutable: absolute path\r\n"), lpszName));
        // absolute path

        // try debug list if it's in \Windows
        if (bIsInWindDir && IsInDbgList (pPlainName)) {
            dwErr = TryDir (oeptr, DBGDIR, DBGDIRLEN, pPlainName, NKwcslen (pPlainName), hTok, phTokRet, POLICY_MODULE_ALLOW_SHADOW|dwPolicyFlags);
        }

        // try filesystem
        if (ERROR_FILE_NOT_FOUND == dwErr) {
            dwErr = OpenFileFromFilesys ((LPWSTR)lpszName, oeptr, hTok, phTokRet, dwPolicyFlags);
        }

        // if not found in filesystem and is in windows directory, try ROM
        if ((ERROR_FILE_NOT_FOUND == dwErr) && bIsInWindDir) {
            dwErr = OpenFileFromROM (pPlainName, oeptr);
        }
    } else {

        // relative path
        DEBUGMSG (ZONE_OPENEXE,(TEXT("OpenExecutable: relative path\r\n"), lpszName));

        // try debug list first
        if (IsInDbgList (pPlainName)) {
            dwErr = TryDir (oeptr, DBGDIR, DBGDIRLEN, lpszName, nTotalLen, hTok, phTokRet, POLICY_MODULE_ALLOW_SHADOW|dwPolicyFlags);
        }

        // try same directory as EXE for DLL
        if (pprc && (ERROR_FILE_NOT_FOUND == dwErr)) {
            if (!(FA_PREFIXUP & pprc->oe.filetype)) {
                dwErr = TrySameDir (oeptr, pPlainName, pprc, hTok, phTokRet, dwPolicyFlags);
            }
        }

        if ((ERROR_FILE_NOT_FOUND == dwErr)
            && ((dwErr = TryDir (oeptr, SYSTEMDIR, SYSTEMDIRLEN, lpszName, nTotalLen, hTok, phTokRet, dwPolicyFlags)) == ERROR_FILE_NOT_FOUND)     // try \windows\lpszName
            && ((!bIsInWindDir && (lpszName != pPlainName))                                                                // try ROM
                || ((dwErr = OpenFileFromROM (pPlainName, oeptr) == ERROR_FILE_NOT_FOUND)))
            && ((dwErr = TryDir (oeptr, L"\\", 1, lpszName, nTotalLen, hTok, phTokRet, dwPolicyFlags)) == ERROR_FILE_NOT_FOUND)) {                 // try \lpszName

            if (pSearchPath) {
                // check the alternative search path
                LPCWSTR pTrav = pSearchPath->name;
                int nLen;

                while (*pTrav && (ERROR_FILE_NOT_FOUND == dwErr)) {
                    nLen = NKwcslen(pTrav);
                    dwErr = TryDir (oeptr, pTrav, nLen, lpszName, nTotalLen, hTok, phTokRet, dwPolicyFlags);
                    pTrav += nLen + 1;
                }
            }
        }
    }

    DEBUGMSG (ZONE_LOADER1, (L"OpenExecutable '%s' returns %8.8lx\r\n", lpszName, dwErr));
    // restore last err if succeeded, set last err if failed.
    KSetLastError (pCurThread, dwErr? dwErr : dwLastErr);

    return dwErr;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
CloseExe(
    openexe_t *oeptr
    )
{
    if (oeptr && oeptr->filetype) {

        if (!(FA_DIRECTROM & oeptr->filetype)) {
            HNDLCloseHandle (g_pprcNK, oeptr->hf);
            oeptr->hf = INVALID_HANDLE_VALUE;
        }

        if (oeptr->lpName)
            FreeName(oeptr->lpName);
        oeptr->lpName = 0;
        oeptr->filetype = 0;
    }
}


#ifdef DEBUG

LPWSTR e32str[STD_EXTRA] = {
    TEXT("EXP"),TEXT("IMP"),TEXT("RES"),TEXT("EXC"),TEXT("SEC"),TEXT("FIX"),TEXT("DEB"),TEXT("IMD"),
    TEXT("MSP"),TEXT("TLS"),TEXT("CBK"),TEXT("RS1"),TEXT("RS2"),TEXT("RS3"),TEXT("RS4"),TEXT("RS5"),
};


//------------------------------------------------------------------------------
// Dump e32 header
//------------------------------------------------------------------------------
void
DumpHeader(
    e32_exe *e32
    )
{
    int loop;

    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_magic      : %8.8lx\r\n"),*(LPDWORD)&e32->e32_magic));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_cpu        : %8.8lx\r\n"),e32->e32_cpu));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_objcnt     : %8.8lx\r\n"),e32->e32_objcnt));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_symtaboff  : %8.8lx\r\n"),e32->e32_symtaboff));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_symcount   : %8.8lx\r\n"),e32->e32_symcount));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_timestamp  : %8.8lx\r\n"),e32->e32_timestamp));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_opthdrsize : %8.8lx\r\n"),e32->e32_opthdrsize));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_imageflags : %8.8lx\r\n"),e32->e32_imageflags));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_coffmagic  : %8.8lx\r\n"),e32->e32_coffmagic));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_linkmajor  : %8.8lx\r\n"),e32->e32_linkmajor));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_linkminor  : %8.8lx\r\n"),e32->e32_linkminor));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_codesize   : %8.8lx\r\n"),e32->e32_codesize));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_initdsize  : %8.8lx\r\n"),e32->e32_initdsize));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_uninitdsize: %8.8lx\r\n"),e32->e32_uninitdsize));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_entryrva   : %8.8lx\r\n"),e32->e32_entryrva));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_codebase   : %8.8lx\r\n"),e32->e32_codebase));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_database   : %8.8lx\r\n"),e32->e32_database));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_vbase      : %8.8lx\r\n"),e32->e32_vbase));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_objalign   : %8.8lx\r\n"),e32->e32_objalign));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_filealign  : %8.8lx\r\n"),e32->e32_filealign));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_osmajor    : %8.8lx\r\n"),e32->e32_osmajor));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_osminor    : %8.8lx\r\n"),e32->e32_osminor));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_usermajor  : %8.8lx\r\n"),e32->e32_usermajor));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_userminor  : %8.8lx\r\n"),e32->e32_userminor));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_subsysmajor: %8.8lx\r\n"),e32->e32_subsysmajor));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_subsysminor: %8.8lx\r\n"),e32->e32_subsysminor));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_vsize      : %8.8lx\r\n"),e32->e32_vsize));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_hdrsize    : %8.8lx\r\n"),e32->e32_hdrsize));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_filechksum : %8.8lx\r\n"),e32->e32_filechksum));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_subsys     : %8.8lx\r\n"),e32->e32_subsys));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_dllflags   : %8.8lx\r\n"),e32->e32_dllflags));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_stackmax   : %8.8lx\r\n"),e32->e32_stackmax));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_stackinit  : %8.8lx\r\n"),e32->e32_stackinit));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_heapmax    : %8.8lx\r\n"),e32->e32_heapmax));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_heapinit   : %8.8lx\r\n"),e32->e32_heapinit));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_hdrextra   : %8.8lx\r\n"),e32->e32_hdrextra));
    for (loop = 0; loop < STD_EXTRA; loop++)
        DEBUGMSG(ZONE_LOADER1,(TEXT("  e32_unit:%3.3s    : %8.8lx %8.8lx\r\n"),
            e32str[loop],e32->e32_unit[loop].rva,e32->e32_unit[loop].size));
}



//------------------------------------------------------------------------------
// Dump o32 header
//------------------------------------------------------------------------------
void
DumpSection(
    o32_obj *o32,
    o32_lite *o32l
    )
{
    ulong loop;

    DEBUGMSG(ZONE_LOADER1,(TEXT("Section %a:\r\n"),o32->o32_name));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  o32_vsize      : %8.8lx\r\n"),o32->o32_vsize));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  o32_rva        : %8.8lx\r\n"),o32->o32_rva));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  o32_psize      : %8.8lx\r\n"),o32->o32_psize));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  o32_dataptr    : %8.8lx\r\n"),o32->o32_dataptr));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  o32_realaddr   : %8.8lx\r\n"),o32->o32_realaddr));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  o32_flags      : %8.8lx\r\n"),o32->o32_flags));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  o32l_vsize     : %8.8lx\r\n"),o32l->o32_vsize));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  o32l_rva       : %8.8lx\r\n"),o32l->o32_rva));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  o32l_realaddr  : %8.8lx\r\n"),o32l->o32_realaddr));
    DEBUGMSG(ZONE_LOADER1,(TEXT("  o32l_flags     : %8.8lx\r\n"),o32l->o32_flags));
    for (loop = 0; loop+16 <= o32l->o32_vsize; loop+=16) {
        DEBUGMSG(ZONE_LOADER2,(TEXT(" %8.8lx: %8.8lx %8.8lx %8.8lx %8.8lx\r\n"),
            loop,
            *(LPDWORD)(o32l->o32_realaddr+loop),
            *(LPDWORD)(o32l->o32_realaddr+loop+4),
            *(LPDWORD)(o32l->o32_realaddr+loop+8),
            *(LPDWORD)(o32l->o32_realaddr+loop+12)));
        }
    if (loop != o32l->o32_vsize) {
        DEBUGMSG(ZONE_LOADER2,(TEXT(" %8.8lx:"),loop));
        while (loop+4 <= o32l->o32_vsize) {
            DEBUGMSG(ZONE_LOADER2,(TEXT(" %8.8lx"),*(LPDWORD)(o32l->o32_realaddr+loop)));
            loop += 4;
        }
        if (loop+2 <= o32l->o32_vsize)
            DEBUGMSG(ZONE_LOADER2,(TEXT(" %4.4lx"),(DWORD)*(LPWORD)(o32l->o32_realaddr+loop)));
        DEBUGMSG(ZONE_LOADER2,(TEXT("\r\n")));
    }
}

#endif

//
// OAL can call this function to force a clean boot
//
void NKForceCleanBoot (void)
{
    fForceCleanBoot = TRUE;
    if (LogPtr)
        LogPtr->magic1 = 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD
CalcFSPages(
    DWORD pages
    )
{
    DWORD fspages;

    if (pages <= 2*1024*1024/VM_PAGE_SIZE) {
        fspages = (pages*(pTOC->ulFSRamPercent&0xff))/256;
    } else {
        fspages = ((2*1024*1024/VM_PAGE_SIZE)*(pTOC->ulFSRamPercent&0xff))/256;
        if (pages <= 4*1024*1024/VM_PAGE_SIZE) {
            fspages += ((pages-2*1024*1024/VM_PAGE_SIZE)*((pTOC->ulFSRamPercent>>8)&0xff))/256;
        } else {
            fspages += ((2*1024*1024/VM_PAGE_SIZE)*((pTOC->ulFSRamPercent>>8)&0xff))/256;
            if (pages <= 6*1024*1024/VM_PAGE_SIZE) {
                fspages += ((pages-4*1024*1024/VM_PAGE_SIZE)*((pTOC->ulFSRamPercent>>16)&0xff))/256;
            } else {
                fspages += ((2*1024*1024/VM_PAGE_SIZE)*((pTOC->ulFSRamPercent>>16)&0xff))/256;
                fspages += ((pages-6*1024*1024/VM_PAGE_SIZE)*((pTOC->ulFSRamPercent>>24)&0xff))/256;
            }
        }
    }
    return fspages;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD
CarveMem(
    LPBYTE pMem,
    DWORD dwExt,
    DWORD dwLen,
    DWORD dwFSPages
    )
{
    DWORD dwPages, dwNow, loop;
    LPBYTE pCur, pPage;

    dwPages = dwLen / VM_PAGE_SIZE;
    pCur = pMem + dwExt;
    while (dwPages && dwFSPages) {
        pPage = pCur;
        pCur += VM_PAGE_SIZE;
        dwPages--;
        dwFSPages--;
        *(LPBYTE *)pPage = LogPtr->pFSList;
        LogPtr->pFSList = pPage;
        dwNow = dwPages;
        if (dwNow > dwFSPages)
            dwNow = dwFSPages;
        if (dwNow > (VM_PAGE_SIZE/4) - 2)
            dwNow = (VM_PAGE_SIZE/4) - 2;
        *((LPDWORD)pPage+1) = dwNow;
        for (loop = 0; loop < dwNow; loop++) {
            *((LPDWORD)pPage+2+loop) = (DWORD)pCur;
            pCur += VM_PAGE_SIZE;
        }
        dwPages -= dwNow;
        dwFSPages -= dwNow;
    }
    return dwFSPages;
}



void RemovePage(DWORD dwAddr);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
GrabFSPages(void)
{
    LPBYTE pPageList;
    DWORD loop,max;

    pPageList = LogPtr->pFSList;
    while (pPageList) {
        max = *((LPDWORD)pPageList+1);
        RemovePage((DWORD)pPageList);
        for (loop = 0; loop < max; loop++)
            RemovePage(*((LPDWORD)pPageList+2+loop));
        pPageList = *(LPBYTE *)pPageList;
    }
}

//------------------------------------------------------------------------------
//
// Divide the declared physical RAM between Object Store and User RAM.
//
//------------------------------------------------------------------------------
void
KernelFindMemory(void)
{
    ROMChain_t *pList, *pListPrev = NULL;
    extern const wchar_t NKCpuType [];
    extern void InitDrWatson (void);
    MEMORY_SECTION MemSections[MAX_MEMORY_SECTIONS];
    DWORD cSections;
    DWORD dwRAMStart, dwRAMEnd;
    DWORD i;

    // flush cache before we start doing anything, in case OEMInit access kernel owned memory...
    OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_DISCARD);

    for (pList = g_pOemGlobal->pROMChain; pList; pList = pList->pNext) {
        if (pList->pTOC == pTOC) {
            // main region is already in the OEMROMChain, just use it
            ROMChain = g_pOemGlobal->pROMChain;
            break;
        }
        pListPrev = pList;
    }
    if (g_pOemGlobal->pROMChain && !pList) {
        DEBUGCHK (pListPrev);
        pListPrev->pNext = ROMChain;
        ROMChain = g_pOemGlobal->pROMChain;
    }

    if (!MDValidateRomChain (ROMChain)) {
        NKDbgPrintfW (L"WARNING! XIP region span accross discontigious memory!!! System might not work properly on some flash parts!\r\n");
    }

    // initialize Kernel Dr.Watson support
    InitDrWatson ();

    // initialize SoftLog (will be no-op if not debug build)
    InitSoftLog ();

    // discard cache
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_DISCARD);

    //
    // LogPtr at the start of RAM holds some header information
    // Using Cached access here.
    //
    LogPtr = (fslog_t *) PAGEALIGN_UP (pTOC->ulRAMFree+MemForPT);

    RETAILMSG(1,(L"Booting Windows CE version %d.%2.2d for (%s)\r\n",CE_MAJOR_VER,CE_MINOR_VER, NKCpuType));
#ifdef MIPS
    if (IsMIPS16Supported)
        RETAILMSG (1, (L"MIPS16 Instructions Supported\r\n"));
    else
        RETAILMSG (1, (L"MIPS16 Instructions NOT Supported\r\n"));
#endif
    DEBUGMSG(1,(L"&pTOC = %8.8lx, pTOC = %8.8lx, pTOC->ulRamFree = %8.8lx, MemForPT = %8.8lx\r\n", &pTOC, pTOC, pTOC->ulRAMFree, MemForPT));

    if (LogPtr->version != CURRENT_FSLOG_VERSION) {
        RETAILMSG(1,(L"\r\nOld or invalid version stamp in kernel structures - starting clean!\r\n"));
        LogPtr->magic1 = LogPtr->magic2 = 0;
        LogPtr->version = CURRENT_FSLOG_VERSION;
    }

    if (fForceCleanBoot || (LogPtr->magic1 != LOG_MAGIC)) {
        DWORD fspages, mainpages, secondpages, cExtSections, cPages;

        //
        // Ask OEM if extension RAM exists.
        //
        if (g_pOemGlobal->pfnEnumExtensionDRAM) {
            cExtSections = (*g_pOemGlobal->pfnEnumExtensionDRAM)(MemSections, MAX_MEMORY_SECTIONS - 1);
            DEBUGCHK(cExtSections < MAX_MEMORY_SECTIONS);
        } else if (OEMGetExtensionDRAM (&MemSections[0].dwStart, &MemSections[0].dwLen)) {
            cExtSections = 1;
        } else {
            cExtSections = 0;
        }

        dwRAMStart = pTOC->ulRAMStart;
        dwRAMEnd = g_pOemGlobal->dwMainMemoryEndAddress;
        mainpages = (PAGEALIGN_DOWN(g_pOemGlobal->dwMainMemoryEndAddress) - PAGEALIGN_UP(pTOC->ulRAMFree+MemForPT))/VM_PAGE_SIZE - 4096/VM_PAGE_SIZE;

        //
        // Setup base RAM area
        //
        LogPtr->fsmemblk[0].startptr  = PAGEALIGN_UP(pTOC->ulRAMFree+MemForPT) + 4096;
        LogPtr->fsmemblk[0].extension = PAGEALIGN_UP(mainpages*2);   // 2 bytes per page for ref-cnt
        mainpages -= LogPtr->fsmemblk[0].extension/VM_PAGE_SIZE;
        LogPtr->fsmemblk[0].length    = mainpages * VM_PAGE_SIZE;
        cSections = 1;

        //
        // Setup extension RAM sections
        //
        secondpages = 0;
        for (i = 0; i< cExtSections; i++) {
            if (MemSections[i].dwLen < (PAGEALIGN_UP(MemSections[i].dwStart) - MemSections[i].dwStart)) {
                MemSections[i].dwStart = MemSections[i].dwLen = 0;
            } else {
                MemSections[i].dwLen -= (PAGEALIGN_UP(MemSections[i].dwStart) - MemSections[i].dwStart);
                MemSections[i].dwStart = PAGEALIGN_UP(MemSections[i].dwStart);
                MemSections[i].dwLen = PAGEALIGN_DOWN(MemSections[i].dwLen);

                DEBUGCHK (!(MemSections[i].dwLen & VM_PAGE_OFST_MASK));
                DEBUGCHK (!(MemSections[i].dwStart & VM_PAGE_OFST_MASK));

                if (dwRAMStart > MemSections[i].dwStart)
                    dwRAMStart = MemSections[i].dwStart;
                if (dwRAMEnd < (MemSections[i].dwStart + MemSections[i].dwLen))
                    dwRAMEnd = MemSections[i].dwStart + MemSections[i].dwLen;

                cPages = MemSections[i].dwLen / VM_PAGE_SIZE;

                LogPtr->fsmemblk[cSections].startptr = MemSections[i].dwStart;
                LogPtr->fsmemblk[cSections].extension = PAGEALIGN_UP(cPages * 2);   // 2 bytes per page for ref-cnt
                LogPtr->fsmemblk[cSections].length = MemSections[i].dwLen - LogPtr->fsmemblk[cSections].extension;
                secondpages += LogPtr->fsmemblk[cSections].length / VM_PAGE_SIZE;
                cSections++;
            }
        }
        memset(&LogPtr->fsmemblk[cSections], 0, sizeof(mem_t) * (MAX_MEMORY_SECTIONS-cSections));

        fspages = CalcFSPages(mainpages+secondpages);

        // ask OAL to change fs pages if required
        if (g_pOemGlobal->pfnCalcFSPages)
            fspages = g_pOemGlobal->pfnCalcFSPages (mainpages+secondpages, fspages);

        if (fspages > MAX_FS_PAGE) {
            fspages = MAX_FS_PAGE;
        }
        RETAILMSG(1,(L"Configuring: Primary pages: %d, Secondary pages: %d, Filesystem pages = %d\r\n",
            mainpages,secondpages,fspages));

        //
        // Now split the base and extension RAM sections into Object Store and
        // User RAM.
        //
        LogPtr->pFSList = 0;
        for (i = 0; i <cSections; i++) {
            fspages = CarveMem((LPBYTE)LogPtr->fsmemblk[i].startptr,LogPtr->fsmemblk[i].extension,LogPtr->fsmemblk[i].length,fspages);
        }
        OEMCacheRangeFlush (0, 0, CACHE_SYNC_WRITEBACK);
        //
        // Mark the header as initialized.
        //
        LogPtr->magic1 = LOG_MAGIC;
        LogPtr->magic2 = 0;
        RETAILMSG(1,(L"\r\nBooting kernel with clean memory configuration:\r\n"));
    } else {
        //
        // The magic number was found. Use the memory configuration already
        // set up at LogPtr.
        //
        RETAILMSG(1,(L"\r\nBooting kernel with existing memory configuration:\r\n"));
        dwRAMStart = pTOC->ulRAMStart;
        dwRAMEnd = g_pOemGlobal->dwMainMemoryEndAddress;
        cSections = 0;
        for (i = 0; i < MAX_MEMORY_SECTIONS; i++) {
            if (LogPtr->fsmemblk[i].length) {
                cSections++;
                if (dwRAMStart > LogPtr->fsmemblk[i].startptr)
                    dwRAMStart = LogPtr->fsmemblk[i].startptr;
                if (dwRAMEnd <(LogPtr->fsmemblk[i].startptr + LogPtr->fsmemblk[i].extension + LogPtr->fsmemblk[i].length))
                    dwRAMEnd = LogPtr->fsmemblk[i].startptr + LogPtr->fsmemblk[i].extension + LogPtr->fsmemblk[i].length;
            }
        }
    }

    RETAILMSG(1, (L"Memory Sections:\r\n"));
    for (i = 0; i <cSections; i++) {
        RETAILMSG(1,(L"[%d] : start: %8.8lx, extension: %8.8lx, length: %8.8lx\r\n",
                    i, LogPtr->fsmemblk[i].startptr,LogPtr->fsmemblk[i].extension,LogPtr->fsmemblk[i].length));
    }

    MemoryInfo.pKData = (LPVOID)dwRAMStart;
    MemoryInfo.pKEnd  = (LPVOID)dwRAMEnd;
    MemoryInfo.cFi = cSections;
    MemoryInfo.pFi = &FreeInfo[0];
    for (i = 0; i < cSections; i++) {
        DEBUGCHK (!((DWORD) LogPtr->fsmemblk[i].startptr & 1));
        FreeInfo[i].pUseMap = (PWORD)LogPtr->fsmemblk[i].startptr;
        memset((LPVOID)LogPtr->fsmemblk[i].startptr, 0, LogPtr->fsmemblk[i].extension);
        FreeInfo[i].paStart = GetPFN((LPVOID)(LogPtr->fsmemblk[i].startptr+LogPtr->fsmemblk[i].extension - VM_PAGE_SIZE)) + PFN_INCR;
        FreeInfo[i].paEnd = FreeInfo[i].paStart + PFN_INCR * (LogPtr->fsmemblk[i].length / VM_PAGE_SIZE);
    }
    memset(&FreeInfo[cSections], 0, sizeof(FREEINFO) * (MAX_MEMORY_SECTIONS-cSections));

    GrabFSPages();

    // writeback everything we updated
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_WRITEBACK);


    KInfoTable[KINX_GWESHEAPINFO] = 0;
    KInfoTable[KINX_TIMEZONEBIAS] = 0;
}



//------------------------------------------------------------------------------
// Relocate a page
//------------------------------------------------------------------------------
static BOOL
RelocatePage(
    PPROCESS pprc,
    e32_lite *eptr,
    o32_lite *optr,
    ulong BasePtr,
    DWORD pMem,
    DWORD pData,
    DWORD prevdw
    )
{

    DWORD Comp1 = 0, Comp2;
#if defined(MIPS)
    DWORD PrevPage;
#endif
    struct info *blockptr, *blockstart;
    ulong blocksize;
    LPDWORD FixupAddress;
#ifndef x86
    LPWORD FixupAddressHi;
    BOOL MatchedReflo=FALSE;
#endif
    DWORD FixupValue;
    const WORD *currptr;
    DWORD offset;

#if defined(x86)
#define RELOC_LIMIT 8192
#else
#define RELOC_LIMIT 4096
#endif

    DWORD reloc_limit = RELOC_LIMIT;

#ifdef MIPS
    if (IsMIPS16Supported)
        reloc_limit = 8192;
#endif

    if (!(blocksize = eptr->e32_unit[FIX].size)) { // No relocations
        return TRUE;
    }
    blockstart = blockptr = (struct info *)(BasePtr+eptr->e32_unit[FIX].rva);

    DEBUGMSG(ZONE_LOADER1,(TEXT("Relocations: BasePtr = %8.8lx, VBase = %8.8lx, pMem = %8.8lx, pData = %8.8lx\r\n"),
        BasePtr, eptr->e32_vbase, pMem, pData));
    if (!(offset = BasePtr - eptr->e32_vbase)
        || (pMem - (DWORD) blockstart < blocksize)) {
        DEBUGMSG(ZONE_LOADER1,(TEXT("Relocations: No Relocation Required\r\n")));
        return TRUE;                                                    // no adjustment needed
    }
    DEBUGMSG(ZONE_LOADER1,(TEXT("RelocatePage: Offset is %8.8lx, process id = %8.8lx\r\n"),offset, pprc->dwId));

    DEBUGCHK(OwnCS (&PagerCS));
    LeaveCriticalSection(&PagerCS);

    // we need to access reloc section to perform fixup. In order to be able to access it directly
    // with point, we need to switch to the destination VM.
    pprc = SwitchVM (pprc);
    while (((ulong)blockptr < (ulong)blockstart + blocksize) && blockptr->size) {
        currptr = (LPWORD)(((ulong)blockptr)+sizeof(struct info));
        if ((ulong)currptr >= ((ulong)blockptr+blockptr->size)) {
            blockptr = (struct info *)(((ulong)blockptr)+blockptr->size);
            continue;
        }
        if ((BasePtr+blockptr->rva > pMem) || (BasePtr+blockptr->rva+reloc_limit <= pMem)) {
            blockptr = (struct info *)(((ulong)blockptr)+blockptr->size);
            continue;
        }
        goto fixuppage;
    }
    goto DoneFixup;

fixuppage:
    DEBUGMSG(ZONE_LOADER1,(L"Fixing up %8.8lx %8.8lx, %8.8lx\r\n",blockptr->rva,optr->o32_rva,optr->o32_realaddr));
    while ((ulong)currptr < ((ulong)blockptr+blockptr->size)) {
#ifdef x86
        Comp1 = pMem;
        Comp2 = BasePtr + blockptr->rva;
        Comp1 = (Comp2 + 0x1000 == Comp1) ? 1 : 0;
#else
        Comp1 = pMem - (BasePtr + blockptr->rva);
        Comp2 = (DWORD)(*currptr&0xfff);
#if defined(MIPS)
        // Comp1 is relative start of page being relocated
        // Comp2 is relative address of fixup
        // For MIPS16 jump, deal with fixups on this page or the preceding page
        // For all other fixups, deal with this page only
        if (IsMIPS16Supported && (*currptr>>12 == IMAGE_REL_BASED_MIPS_JMPADDR16)) {
            if ((Comp1 > Comp2 + VM_PAGE_SIZE) || (Comp1 + VM_PAGE_SIZE <= Comp2)) {
                currptr++;
                continue;
            }
            // PrevPage: is fixup located on the page preceding the one being relocated?
            PrevPage = (Comp1 > Comp2);
            // Comp1: is fixup located in the block preceding the one that contains the current page?
            Comp1 = PrevPage && ((Comp1 & 0xfff) == 0);
        } else {
#endif
            if ((Comp1 > Comp2) || (Comp1 + VM_PAGE_SIZE <= Comp2)) {
                if (*currptr>>12 == IMAGE_REL_BASED_HIGHADJ)
                    currptr++;
                currptr++;
                continue;
            }
#if defined(MIPS)
            // Comp1: is fixup located in the block preceding the one that contains the current page? (No.)
            if (IsMIPS16Supported)
                Comp1 = 0;
        }
#endif
#endif
#if defined(x86)
        FixupAddress = (LPDWORD)(((pData&0xfffff000) + (*currptr&0xfff)) - 4096*Comp1);
#else
        FixupAddress = (LPDWORD)((pData&0xfffff000) + (*currptr&0xfff));
#ifdef MIPS
        if (IsMIPS16Supported)
            FixupAddress = (LPDWORD) (FixupAddress - 4096*Comp1);
#endif
#endif
        DEBUGMSG(ZONE_LOADER2,(TEXT("type %d, low %8.8lx, page %8.8lx, addr %8.8lx\r\n"),
            (*currptr>>12)&0xf, (*currptr)&0x0fff,blockptr->rva,FixupAddress));
        switch (*currptr>>12) {
            case IMAGE_REL_BASED_ABSOLUTE: // Absolute - no fixup required.
                break;
#ifdef x86
            case IMAGE_REL_BASED_HIGHLOW: // Word - (32-bits) relocate the entire address.
                if (Comp1 && (((DWORD)FixupAddress & 0xfff) > 0xffc)) {
                    switch ((DWORD)FixupAddress & 0x3) {
                        case 1:
                            FixupValue = (prevdw>>8) + (*((LPBYTE)FixupAddress+3) <<24 ) + offset;
                            *((LPBYTE)FixupAddress+3) = (BYTE)(FixupValue >> 24);
                            break;
                        case 2:
                            FixupValue = (prevdw>>16) + (*((LPWORD)FixupAddress+1) << 16) + offset;
                            *((LPWORD)FixupAddress+1) = (WORD)(FixupValue >> 16);
                            break;
                        case 3:
                            FixupValue = (prevdw>>24) + ((*(LPDWORD)((LPBYTE)FixupAddress+1)) << 8) + offset;
                            *(LPWORD)((LPBYTE)FixupAddress+1) = (WORD)(FixupValue >> 8);
                            *((LPBYTE)FixupAddress+3) = (BYTE)(FixupValue >> 24);
                            break;
                    }
                } else if (!Comp1) {
                    if (((DWORD)FixupAddress & 0xfff) > 0xffc) {
                        switch ((DWORD)FixupAddress & 0x3) {
                            case 1:
                                FixupValue = *(LPWORD)FixupAddress + (((DWORD)*((LPBYTE)FixupAddress+2))<<16) + offset;
                                *(LPWORD)FixupAddress = (WORD)FixupValue;
                                *((LPBYTE)FixupAddress+2) = (BYTE)(FixupValue>>16);
                                break;
                            case 2:
                                *(LPWORD)FixupAddress += (WORD)offset;
                                break;
                            case 3:
                                *(LPBYTE)FixupAddress += (BYTE)offset;
                                break;
                        }
                    } else
                        *FixupAddress += offset;
                }
                break;
#else
            case IMAGE_REL_BASED_HIGH: // Save the address and go to get REF_LO paired with this one
                FixupAddressHi = (LPWORD)FixupAddress;
                MatchedReflo = TRUE;
                break;
            case IMAGE_REL_BASED_LOW: // Low - (16-bit) relocate high part too.
                if (MatchedReflo) {
                    FixupValue = (DWORD)(long)((*FixupAddressHi) << 16) +
                        *(LPWORD)FixupAddress + offset;
                    *FixupAddressHi = (short)((FixupValue + 0x8000) >> 16);
                    MatchedReflo = FALSE;
                } else
                    FixupValue = *(short *)FixupAddress + offset;
                *(LPWORD)FixupAddress = (WORD)(FixupValue & 0xffff);
                break;
            case IMAGE_REL_BASED_HIGHLOW: // Word - (32-bits) relocate the entire address.
                    if ((DWORD)FixupAddress & 0x3)
                        *(UNALIGNED DWORD *)FixupAddress += (DWORD)offset;
                    else
                        *FixupAddress += (DWORD)offset;
                break;
            case IMAGE_REL_BASED_HIGHADJ: // 32 bit relocation of 16 bit high word, sign extended
                DEBUGMSG(ZONE_LOADER2,(TEXT("Grabbing extra data %8.8lx\r\n"),*(currptr+1)));
                *(LPWORD)FixupAddress += (WORD)((*(short *)(++currptr)+offset+0x8000)>>16);
                break;
            case IMAGE_REL_BASED_MIPS_JMPADDR: // jump to 26 bit target (shifted left 2)
                FixupValue = ((*FixupAddress)&0x03ffffff) + (offset>>2);
                *FixupAddress = (*FixupAddress & 0xfc000000) | (FixupValue & 0x03ffffff);
                break;
#if defined(MIPS)
            case IMAGE_REL_BASED_MIPS_JMPADDR16: // MIPS16 jal/jalx to 26 bit target (shifted left 2)
                if (IsMIPS16Supported) {
                    if (PrevPage && (((DWORD)FixupAddress & (VM_PAGE_SIZE-1)) == VM_PAGE_SIZE-2)) {
                        // Relocation is on previous page, crossing into the current one
                        // Do this page's portion == last 2 bytes of jal/jalx == least-signif 16 bits of address
                        *((LPWORD)FixupAddress+1) += (WORD)(offset >> 2);
                        break;
                    } else if (!PrevPage) {
                        // Relocation is on this page. Put the most-signif bits in FixupValue
                        FixupValue = (*(LPWORD)FixupAddress) & 0x03ff;
                        FixupValue = ((FixupValue >> 5) | ((FixupValue & 0x1f) << 5)) << 16;
                        if (((DWORD)FixupAddress & (VM_PAGE_SIZE-1)) == VM_PAGE_SIZE-2) {
                            // We're at the end of the page. prevdw has the 2 bytes we peeked at on the next page,
                            // so use them instead of loading them from FixupAddress+1
                            FixupValue |= (WORD)prevdw;
                            FixupValue += offset >> 2;
                        } else {
                            // All 32 bits are on this page. Go ahead and fixup last 2 bytes
                            FixupValue |= *((LPWORD)FixupAddress+1);
                            FixupValue += offset >> 2;
                            *((LPWORD)FixupAddress+1) = (WORD)(FixupValue & 0xffff);
                        }
                        // In either case, we now have the right bits for the upper part of the address
                        // Rescramble them and put them in the first 2 bytes of the fixup
                        FixupValue = (FixupValue >> 16) & 0x3ff;
                        *(LPWORD)FixupAddress = (WORD)((*(LPWORD)FixupAddress & 0xfc00) | (FixupValue >> 5) | ((FixupValue & 0x1f) << 5));
                    }
                }
                break;
#endif
#endif
            default :
                DEBUGMSG(ZONE_LOADER1,(TEXT("Not doing fixup type %d\r\n"),*currptr>>12));
                DEBUGCHK(0);
                break;
        }
        //DEBUGMSG(ZONE_LOADER2,(TEXT("reloc complete, new op %8.8lx\r\n"),*FixupAddress));
        currptr++;
    }
#ifdef MIPS
    if (IsMIPS16Supported) {
#endif
#if defined(x86) || defined(MIPS)
    if (Comp1) {
        blockptr = (struct info *)(((ulong)blockptr)+blockptr->size);
        if (((ulong)blockptr < (ulong)blockstart + blocksize) && blockptr->size) {
            currptr = (LPWORD)(((ulong)blockptr)+sizeof(struct info));
            if ((ulong)currptr < ((ulong)blockptr+blockptr->size))
                if ((BasePtr+blockptr->rva <= pMem) && (BasePtr+blockptr->rva+4096 > pMem))
                    goto fixuppage;
        }
    }
#endif
#ifdef MIPS
    }
#endif
DoneFixup:
    // switch back to the VM we were using before.
    SwitchVM (pprc);
    EnterCriticalSection(&PagerCS);
    return TRUE;
}



//------------------------------------------------------------------------------
// Relocate a DLL or EXE
//------------------------------------------------------------------------------
static BOOL
Relocate (
    e32_lite *eptr,
    o32_lite *oarry,
    ulong BasePtr
    )
{
    o32_lite *dataptr;
    struct info *blockptr, *blockstart;
    ulong blocksize;
    LPDWORD FixupAddress;
    LPWORD FixupAddressHi, currptr;
    WORD curroff;
    DWORD FixupValue, offset;
    BOOL MatchedReflo=FALSE;
    int loop;
    BOOL fRet = TRUE;

    DEBUGMSG(ZONE_LOADER1,(TEXT("Relocations: BasePtr = %8.8lx, VBase = %8.8lx\r\n"),
            BasePtr, eptr->e32_vbase));
    __try {
        if ((blocksize = eptr->e32_unit[FIX].size)               // has relocations, and
            && (offset = BasePtr - eptr->e32_vbase)) {           // adjustment needed

            DEBUGMSG(ZONE_LOADER1,(TEXT("Relocate: Offset is %8.8lx\r\n"),offset));
            blockstart = blockptr = (struct info *)(BasePtr+eptr->e32_unit[FIX].rva);
            while (((ulong)blockptr < (ulong)blockstart + blocksize) && blockptr->size) {
                currptr = (LPWORD)(((ulong)blockptr)+sizeof(struct info));
                if ((ulong)currptr >= ((ulong)blockptr+blockptr->size)) {
                    blockptr = (struct info *)(((ulong)blockptr)+blockptr->size);
                    continue;
                }
                dataptr = 0;
                while ((ulong)currptr < ((ulong)blockptr+blockptr->size)) {
                    curroff = *currptr&0xfff;
                    if (!curroff && !blockptr->rva) {
                        currptr++;
                        continue;
                    }
                    if (!dataptr || (dataptr->o32_rva > blockptr->rva + curroff) ||
                            (dataptr->o32_rva + dataptr->o32_vsize <= blockptr->rva + curroff)) {
                        for (loop = 0; loop < eptr->e32_objcnt; loop++) {
                            dataptr = &oarry[loop];
                            if ((dataptr->o32_rva <= blockptr->rva + curroff) &&
                                (dataptr->o32_rva+dataptr->o32_vsize > blockptr->rva + curroff))
                                break;
                        }
                    }
                    DEBUGCHK(loop != eptr->e32_objcnt);
                    FixupAddress = (LPDWORD)(blockptr->rva - dataptr->o32_rva + curroff + dataptr->o32_realaddr);
                    DEBUGMSG(ZONE_LOADER2,(TEXT("type %d, low %8.8lx, page %8.8lx, addr %8.8lx, op %8.8lx\r\n"),
                        (*currptr>>12)&0xf, (*currptr)&0x0fff,blockptr->rva,FixupAddress,*FixupAddress));
                    switch (*currptr>>12) {
                        case IMAGE_REL_BASED_ABSOLUTE: // Absolute - no fixup required.
                            break;
                        case IMAGE_REL_BASED_HIGH: // Save the address and go to get REF_LO paired with this one
                            FixupAddressHi = (LPWORD)FixupAddress;
                            MatchedReflo = TRUE;
                            break;
                        case IMAGE_REL_BASED_LOW: // Low - (16-bit) relocate high part too.
                            if (MatchedReflo) {
                                FixupValue = (DWORD)(long)((*FixupAddressHi) << 16) +
                                    *(LPWORD)FixupAddress + offset;
                                *FixupAddressHi = (short)((FixupValue + 0x8000) >> 16);
                                MatchedReflo = FALSE;
                            } else
                                FixupValue = *(short *)FixupAddress + offset;
                            *(LPWORD)FixupAddress = (WORD)(FixupValue & 0xffff);
                            break;
                        case IMAGE_REL_BASED_HIGHLOW: // Word - (32-bits) relocate the entire address.
                            if ((DWORD)FixupAddress & 0x3)
                                *(UNALIGNED DWORD *)FixupAddress += (DWORD)offset;
                            else
                                *FixupAddress += (DWORD)offset;
                            break;
                        case IMAGE_REL_BASED_HIGHADJ: // 32 bit relocation of 16 bit high word, sign extended
                            DEBUGMSG(ZONE_LOADER2,(TEXT("Grabbing extra data %8.8lx\r\n"),*(currptr+1)));
                            *(LPWORD)FixupAddress += (WORD)((*(short *)(++currptr)+offset+0x8000)>>16);
                            break;
                        case IMAGE_REL_BASED_MIPS_JMPADDR: // jump to 26 bit target (shifted left 2)
                            FixupValue = ((*FixupAddress)&0x03ffffff) + (offset>>2);
                            *FixupAddress = (*FixupAddress & 0xfc000000) | (FixupValue & 0x03ffffff);
                            break;
#if defined(MIPS)
                        case IMAGE_REL_BASED_MIPS_JMPADDR16: // MIPS16 jal/jalx to 26 bit target (shifted left 2)
                            if (IsMIPS16Supported) {
                                FixupValue = (*(LPWORD)FixupAddress) & 0x03ff;
                                FixupValue = (((FixupValue >> 5) | ((FixupValue & 0x1f) << 5)) << 16) | *((LPWORD)FixupAddress+1);
                                FixupValue += offset >> 2;
                                *((LPWORD)FixupAddress+1) = (WORD)(FixupValue & 0xffff);
                                FixupValue = (FixupValue >> 16) & 0x3ff;
                                *(LPWORD)FixupAddress = (WORD) ((*(LPWORD)FixupAddress & 0x1c00) | (FixupValue >> 5) | ((FixupValue & 0x1f) << 5));
                                break;
                            }
                            // fall through
#endif
                        default :
                            DEBUGMSG(ZONE_LOADER1,(TEXT("Not doing fixup type %d\r\n"),*currptr>>12));
                            DEBUGCHK(0);
                            break;
                    }
                    DEBUGMSG(ZONE_LOADER2,(TEXT("reloc complete, new op %8.8lx\r\n"),*FixupAddress));
                    currptr++;
                }
                blockptr = (struct info *)(((ulong)blockptr)+blockptr->size);
            }
        }
    }__except (EXCEPTION_EXECUTE_HANDLER) {
        fRet = FALSE;
    }
    return fRet;
}


//------------------------------------------------------------------------------
// FreeModuleMemory - called when a module is fully unloaded.
//------------------------------------------------------------------------------
void FreeModuleMemory (PMODULE pMod)
{

    DEBUGMSG (ZONE_LOADER1, (L"Last ref of module '%s' removed, completely freeing the module\r\n", pMod->lpszModName));

    // decommit memory allocated in NK's VM
    if (pMod->BasePtr && !IsKernelVa (pMod->BasePtr)) {

        // Return any pool memory to the loader page pool
        DecommitExe (g_pprcNK, &pMod->oe, &pMod->e32, pMod->o32_ptr, TRUE);

        // Any DLL loaded in kernel VM are either RAM DLL or modules loaded as data file.
        // We need to release VM in both cases.
        if (!IsInKVM ((DWORD) pMod->BasePtr)
            && (FA_PREFIXUP & pMod->oe.filetype)
            && ((DWORD) pMod->BasePtr == pMod->e32.e32_vbase)) {
            // ROM MODULE, just decommit
            VMDecommit (g_pprcNK, pMod->BasePtr, pMod->e32.e32_vsize, VM_PAGER_LOADER);

        } else {
            // RAM DLL, release the VM completely

            // Then release the VM completely
            VMFreeAndRelease (g_pprcNK, pMod->BasePtr, pMod->e32.e32_vsize);
        }
    }

    if (pMod->o32_ptr) {
        PNAME pAlloc = (PNAME) ((DWORD) pMod->o32_ptr - 4);
        FreeMem (pAlloc, pAlloc->wPool);
    }

    CloseExe (&pMod->oe);

    FreeMem (pMod, HEAP_MODULE);
}


//
// LockModule - prevent a module from being fully unloaded from the system
//
static void LockModule (PMODULE pMod)
{
    DEBUGCHK (OwnModListLock ());
    DEBUGCHK (pMod->wRefCnt);  // Should not be locking a dying module
    DEBUGCHK (pMod->wRefCnt < 0xffff);
    InterlockedIncrement ((PLONG) &pMod->wRefCnt);
}

//
// UnlockModule - remove a 'process-ref' from a module, release Module memory if no more ref exists
//
static void UnlockModule (PMODULE pMod, BOOL fCallDllMain)
{
    WORD wRefCnt;
    PMODULE pmodRes = NULL;
    HANDLE  hf = INVALID_HANDLE_VALUE;

    LockModuleList ();

    // Notify KD of fully unloaded module, before removing our refcount.  That
    // way if KD gets a page fault it doesn't end up locking/unlocking and
    // destroying the module under us.
    if (1 == pMod->wRefCnt) {
        if (!(DONT_RESOLVE_DLL_REFERENCES & pMod->wFlags)) {
            (* HDModUnload) ((DWORD) pMod);
        }
    }

    DEBUGCHK (pMod->wRefCnt);
    wRefCnt = (WORD) InterlockedDecrement ((PLONG)&pMod->wRefCnt);

    if (!wRefCnt) {
        if (fCallDllMain && (pMod->wFlags & LOAD_LIBRARY_IN_KERNEL) && !(DONT_RESOLVE_DLL_REFERENCES & pMod->wFlags)) {
            DEBUGCHK (pActvProc == g_pprcNK);
            DEBUGCHK (OwnLoaderLock (g_pprcNK));

            // remove the module from the "thread call list"
            if (((MF_IMPORTED|MF_NO_THREAD_CALLS) & pMod->wFlags) == MF_IMPORTED) {
                RemoveFromThreadNotifyList (pMod);
            }

            // we're going to call DllMain for process detach, which can trigger paging on this Dll. This is to ensure that
            // the pager doesn't destroy the module underneath us.
            pMod->wRefCnt = 1;

            // If fCallDllMain is specified, we're calling from NKLoadLibrary/NKFreeLibrary, where kernel's loader lock had already
            // being held. And modules can't be created/destroyed. Thus it's safe to release module list lock here.
            UnlockModuleList ();
            (* g_pfnUndoDepends) (pMod);
            LockModuleList ();

            // restore refcnt
            pMod->wRefCnt = 0;
        }

        // remove the module from NK's module list
        RemoveDList (&pMod->link);

        // Log a free for an invalid process when the module is finally unloaded
        CELOG_ModuleFree (NULL, pMod);

        // save pmodResource and file handle. We can't close the file handle while holding Module list lock.
        // And we can't recursive call UnlockModule with module list lock held.
        pmodRes = pMod->pmodResource;
        pMod->pmodResource = NULL;

        DEBUGCHK (pMod->oe.filetype);

        // retrieve file handle
        if (!(FA_DIRECTROM & pMod->oe.filetype)) {
            hf = pMod->oe.hf;
            pMod->oe.hf = INVALID_HANDLE_VALUE;
        }

        // FreeModuleMemory only access VM, and it's safe to do it holding modulelist lock. In addition, we have
        // to do it while holding the lock, or a LoadLibrary call to the same module can fail due to VM allocation failure.
        FreeModuleMemory (pMod);
    }

    UnlockModuleList ();

    if (INVALID_HANDLE_VALUE != hf) {
        HNDLCloseHandle (g_pprcNK, hf);
    }

    if (pmodRes) {
        UnlockModule (pmodRes, FALSE);
    }

}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD
AccessFromFlags(
    DWORD flags
    )
{
    if (flags & IMAGE_SCN_MEM_WRITE)
        return (flags & IMAGE_SCN_MEM_EXECUTE ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
    else
        return (flags & IMAGE_SCN_MEM_EXECUTE ? PAGE_EXECUTE_READ : PAGE_READONLY);
}


//------------------------------------------------------------------------------
// Load E32 header
//------------------------------------------------------------------------------
DWORD
LoadE32(
    openexe_t *oeptr,
    e32_lite *eptr,
    DWORD *lpflags,
    DWORD *lpEntry,
    BOOL bIgnoreCPU
    )
{

    if (FA_PREFIXUP & oeptr->filetype) {

        // in module section
        e32_rom *e32rptr, e32rom;

        if (FA_DIRECTROM & oeptr->filetype) {
            e32rptr = (struct e32_rom *)(oeptr->tocptr->ulE32Offset);
        } else if (ReadExtImageInfo (oeptr->hf, IOCTL_BIN_GET_E32, sizeof (e32rom), &e32rom)) {
            e32rptr = &e32rom;

            // check if we can XIP out of the media
            // NOTE: File type is changed here.
            if (IMAGE_FILE_XIP & e32rptr->e32_imageflags)
                oeptr->filetype = FT_EXTXIP;
        } else {
            DEBUGMSG(1,(TEXT("LoadE32: Error reading e32 from file hf=%8.8lx\r\n"),oeptr->hf));
            return ERROR_BAD_EXE_FORMAT;
        }

        eptr->e32_objcnt     = e32rptr->e32_objcnt;
        if (lpflags)
            *lpflags         = e32rptr->e32_imageflags;
        if (lpEntry)
            *lpEntry         = e32rptr->e32_entryrva;
        eptr->e32_vbase      = e32rptr->e32_vbase;
        eptr->e32_vsize      = e32rptr->e32_vsize;
        eptr->e32_timestamp  = e32rptr->e32_timestamp;
        if (e32rptr->e32_subsys == IMAGE_SUBSYSTEM_WINDOWS_CE_GUI) {
            eptr->e32_cevermajor = (BYTE)e32rptr->e32_subsysmajor;
            eptr->e32_ceverminor = (BYTE)e32rptr->e32_subsysminor;
        } else {
            eptr->e32_cevermajor = 1;
            eptr->e32_ceverminor = 0;
        }
        eptr->e32_stackmax = e32rptr->e32_stackmax;
        memcpy(&eptr->e32_unit[0],&e32rptr->e32_unit[0],
            sizeof(struct info)*LITE_EXTRA);
        eptr->e32_sect14rva = e32rptr->e32_sect14rva;
        eptr->e32_sect14size = e32rptr->e32_sect14size;

    } else {

        // not in MODULE section
        e32_exe e32;
        e32_exe *e32ptr = &e32;
        int bytesread;

        DEBUGMSG(ZONE_LOADER1,(TEXT("PEBase at %8.8lx, hf = %8.8lx\r\n"),oeptr->offset, oeptr->hf));
        if ((FSSetFilePointer(oeptr->hf,oeptr->offset,0,FILE_BEGIN) != oeptr->offset)
            || !FSReadFile(oeptr->hf,(LPBYTE)&e32,sizeof(e32_exe),&bytesread,0)
            || (bytesread != sizeof(e32_exe))
            || (*(LPDWORD)e32ptr->e32_magic != 0x4550)
            || (e32.e32_vsize >= 0x40000000)) {     // 1G max
            return ERROR_BAD_EXE_FORMAT;
        }

        if (!bIgnoreCPU && !e32ptr->e32_unit[14].rva && (e32ptr->e32_cpu != THISCPUID)) {
#ifdef SH3
            if ((e32ptr->e32_cpu != IMAGE_FILE_MACHINE_SH3DSP) || !SH3DSP)
                return ERROR_BAD_EXE_FORMAT;
#elif defined(MIPS)
            if ((e32ptr->e32_cpu != IMAGE_FILE_MACHINE_MIPS16) || !IsMIPS16Supported)
                return ERROR_BAD_EXE_FORMAT;
#elif defined(ARMV4I)
            if (e32ptr->e32_cpu != IMAGE_FILE_MACHINE_ARM)
                return ERROR_BAD_EXE_FORMAT;
#else
            return ERROR_BAD_EXE_FORMAT;
#endif
        }
        eptr->e32_objcnt     = e32ptr->e32_objcnt;
        if (lpflags)
            *lpflags         = e32ptr->e32_imageflags;
        if (lpEntry)
            *lpEntry         = e32ptr->e32_entryrva;
        eptr->e32_vbase      = e32ptr->e32_vbase;
        eptr->e32_vsize      = e32ptr->e32_vsize;
        eptr->e32_timestamp  = e32ptr->e32_timestamp;
        if (e32ptr->e32_subsys == IMAGE_SUBSYSTEM_WINDOWS_CE_GUI) {
            eptr->e32_cevermajor = (BYTE)e32ptr->e32_subsysmajor;
            eptr->e32_ceverminor = (BYTE)e32ptr->e32_subsysminor;
        } else {
            eptr->e32_cevermajor = 1;
            eptr->e32_ceverminor = 0;
        }
        if ((eptr->e32_cevermajor > 2) || ((eptr->e32_cevermajor == 2) && (eptr->e32_ceverminor >= 2)))
            eptr->e32_stackmax = e32ptr->e32_stackmax;
        else
            eptr->e32_stackmax = 0; // backward compatibility - use default stack size
        if ((eptr->e32_cevermajor > CE_MAJOR_VER) ||
            ((eptr->e32_cevermajor == CE_MAJOR_VER) && (eptr->e32_ceverminor > CE_MINOR_VER))) {
            return ERROR_OLD_WIN_VERSION;
        }
        eptr->e32_sect14rva = e32ptr->e32_unit[14].rva;
        eptr->e32_sect14size = e32ptr->e32_unit[14].size;
        memcpy(&eptr->e32_unit[0],&e32ptr->e32_unit[0],
            sizeof(struct info)*LITE_EXTRA);
#ifdef x86
        // Find the safe exception data (if any) in the image and stash it away
        // in the PDATA directory entry since x86 doesn't use it.
        eptr->e32_unit[EXC] = e32ptr->e32_unit[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG];

        if (eptr->e32_unit[EXC].rva & (E32_EXC_NO_SEH | E32_EXC_DEP_COMPAT)) {
            // Load config RVA must be 4-byte aligned
            return ERROR_BAD_EXE_FORMAT;
        }

        if (e32ptr->e32_dllflags & IMAGE_DLLCHARACTERISTICS_NO_SEH) {
            // Handlers are not allowed in this image
            eptr->e32_unit[EXC].rva = E32_EXC_NO_SEH;
            eptr->e32_unit[EXC].size = 0;
        }

        if (e32ptr->e32_dllflags & IMAGE_DLLCHARACTERISTICS_NX_COMPAT) {
            // This image supports Data Execution Prevention
            eptr->e32_unit[EXC].rva |= E32_EXC_DEP_COMPAT;
        }
#endif
    }
    return 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD
PageInPage(
    openexe_t *oeptr,
    LPVOID pMem2,
    DWORD offset,
    o32_lite *o32ptr,
    DWORD size
    )
{
    DWORD bytesread;
    DWORD retval = PAGEIN_SUCCESS;
    DWORD cbToRead = min(o32ptr->o32_psize - offset, size);

    DEBUGCHK(OwnCS (&PagerCS));
    DEBUGCHK(!(FA_PREFIXUP & oeptr->filetype));
    LeaveCriticalSection(&PagerCS);
    if ((o32ptr->o32_psize > offset)
        && (!FSReadFileWithSeek(oeptr->hf,pMem2,cbToRead,&bytesread,0,o32ptr->o32_dataptr + offset,0)
            || (bytesread != cbToRead))) {
        retval = (ERROR_DEVICE_REMOVED == KGetLastError (pCurThread))? PAGEIN_FAILURE : PAGEIN_RETRY;
    }
    EnterCriticalSection(&PagerCS);
    return retval;
}



//------------------------------------------------------------------------------
// PageInFromROM
//------------------------------------------------------------------------------
static int PageInFromROM (PPROCESS pprc, o32_lite *optr, DWORD addr, openexe_t *oeptr, LPBYTE pPgMem)
{
    DWORD  offset = addr - optr->o32_realaddr;
    DWORD  retval = PAGEIN_SUCCESS;

    DEBUGCHK (!(addr & (VM_PAGE_SIZE-1)));    // addr must be page aligned.

    if ((optr->o32_vsize > optr->o32_psize)
        || !(FA_XIP & oeptr->filetype)
        || (optr->o32_flags & (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_COMPRESSED))) {

        DEBUGMSG(ZONE_PAGING,(L"    PI-ROM: Paging in %8.8lx pPgMem = %8.8lx\r\n", addr, pPgMem));

        //
        // about to call out of the kernel. release PagerCS
        //
        DEBUGCHK (OwnCS (&PagerCS));
        LeaveCriticalSection (&PagerCS);

        // copy (decompress) the data into the temporary page commited
        if (!(FA_DIRECTROM & oeptr->filetype)) {
            // EXTIMAGE or compressed/rw-EXTXIP
            DWORD cbRead;
            if (!FSReadFileWithSeek (oeptr->hf, pPgMem, VM_PAGE_SIZE, &cbRead, 0, optr->o32_dataptr + offset, 0)) {
                retval = PAGEIN_RETRY;
            }
        } else if (optr->o32_flags & IMAGE_SCN_COMPRESSED)
            DecompressROM ((LPVOID)(optr->o32_dataptr), optr->o32_psize, pPgMem, VM_PAGE_SIZE, offset);
        else if (optr->o32_psize > offset)
            memcpy ((LPVOID)pPgMem, (LPVOID)(optr->o32_dataptr+offset), min(optr->o32_psize-offset,VM_PAGE_SIZE));

        EnterCriticalSection (&PagerCS);

    // else - RO, uncompressed, VirtualCopy directly from ROM
    } else {
        NKDbgPrintfW (L"Paging in from uncompressed R/O page from XIP module -- should've never happened\r\n");
        DEBUGCHK (0);
        retval = PAGEIN_FAILURE;
    }
    return retval;
}

//------------------------------------------------------------------------------
// PageInFromRAM
//------------------------------------------------------------------------------
int PageInFromRAM (PPROCESS pprc, openexe_t *oeptr, e32_lite *eptr, DWORD BasePtr, o32_lite *optr, DWORD addr, BOOL fRelocate, LPBYTE pPgMem)
{
    DWORD  offset, prevdw = 0;
    int    retval = PAGEIN_SUCCESS;

    DEBUGCHK (!(addr & (VM_PAGE_SIZE-1)));    // addr must be page aligned.

    offset = addr - optr->o32_realaddr;

    // if we need to relocate, peek into previous/next page
    if (fRelocate) {
#ifdef x86
        if (offset >= 4) {
            retval = PageInPage (oeptr, &prevdw, offset-4, optr, 4);
            DEBUGMSG(ZONE_PAGING && (PAGEIN_SUCCESS != retval),(L"    PI-RAM: PIP Failed 2!\r\n"));
        }
#elif defined(MIPS)
        // If we're not on the last page, peek at the first 2 bytes of the next page;
        // we may need them for a relocation that crosses into the next page.
        if (IsMIPS16Supported && (offset+VM_PAGE_SIZE < optr->o32_vsize)) {
            retval = PageInPage (oeptr, &prevdw, offset+VM_PAGE_SIZE, optr, 2);
            DEBUGMSG(ZONE_PAGING && (PAGEIN_SUCCESS != retval),(L"    PI-RAM: PIP Failed 2!\r\n"));
        }
#endif
    }

    if (PAGEIN_SUCCESS == retval) {

        __try {
            // page in the real page
            if ((retval = PageInPage (oeptr, pPgMem, offset, optr, VM_PAGE_SIZE)) != PAGEIN_SUCCESS) {
                DEBUGMSG(ZONE_PAGING,(L"    PI-RAM: PIP Failed!\r\n"));

            // relocate the page if requested
            } else if (fRelocate && !RelocatePage (pprc, eptr, optr, BasePtr, addr, (DWORD)pPgMem, prevdw)) {
                DEBUGMSG(ZONE_PAGING,(L"  PI-RAM: Page in failure in RP!\r\n"));
                retval = PAGEIN_FAILURE;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            DEBUGMSG(ZONE_PAGING,(L"  PI-RAM: Exception while paging!\r\n"));
            retval = PAGEIN_FAILURE;
        }
    }

    return retval;
}

//------------------------------------------------------------------------------
// The address is committed in kernel but not in the destination process
// Since this is a r/w page, make a copy of the data and virtual copy
// to the destination process.
//------------------------------------------------------------------------------
int CopyFromKernel(PPROCESS pprcDst, o32_lite *optr, DWORD addr)
{
    BOOL retval = PAGEIN_SUCCESS;
    LPVOID pReservation = NULL;
    LPBYTE pPagingMem = NULL;
    PPROCESS pprcActv, pprcVM;
    
    // Get a page to write to
    pPagingMem = GetPagingPage (g_pLoaderPool, &pReservation, addr, FALSE,
                                PAGE_EXECUTE_READWRITE);
    if (pPagingMem) {

        // addr is committed in kernel; switch to kernel to do the memcpy
        pprcActv = SwitchActiveProcess(g_pprcNK);
        pprcVM = SwitchVM(g_pprcNK);

        // copy the memory from kernel to the paging mem
        memcpy(pPagingMem, (LPVOID)addr, VM_PAGE_SIZE);

        SwitchActiveProcess(pprcActv);
        SwitchVM(pprcVM);

        // VirtualCopy from NK's temp page to destination                
        if (!VMFastCopy (pprcDst, addr, g_pprcNK, (DWORD) pPagingMem, VM_PAGE_SIZE, optr->o32_access)) {
            retval = PAGEIN_RETRY;
            DEBUGMSG(ZONE_PAGING,(L"  PageInModule: Page in failure in VC!fWrite=1\r\n"));
        }

        // Decommit and release the temporary memory
        FreePagingPage (g_pLoaderPool, pPagingMem, pReservation, FALSE, retval);

    } else {
        // Failed to get a page.
        retval = PAGEIN_RETRY;
    }            

    return retval;
}


//------------------------------------------------------------------------------
// PageInModule
//------------------------------------------------------------------------------
int PageInModule (PPROCESS pprcDst, PMODULE pMod, DWORD addr, BOOL fWrite)
{
    PPROCESS pprc = g_pprcNK;
    e32_lite *eptr = &pMod->e32;
    o32_lite *optr;
    BOOL     retval = PAGEIN_RETRY;

    DEBUGMSG (ZONE_PAGING, (L"PageInModule: Paging in %8.8lx\r\n", addr));

    // use page start to page in
    addr &= -VM_PAGE_SIZE;

    DEBUGCHK ((pprcDst != g_pprcNK) || (addr > VM_KMODE_BASE) || !fWrite);

    if (!(optr = FindOptr (pMod->o32_ptr, eptr->e32_objcnt, addr))
        || (fWrite && !(optr->o32_flags & IMAGE_SCN_MEM_WRITE)))
        // fail if can't find it in any section, or try to write to R/O section
        return PAGEIN_FAILURE;

    fWrite = (optr->o32_flags & IMAGE_SCN_MEM_WRITE) && !(optr->o32_flags & IMAGE_SCN_MEM_SHARED);

    // R/W not shared - page to current VM directly
    if (fWrite) {
        pprc = pprcDst;
    }

    // we can directly look at NK's VM because
    // (1) we've locked the module, thus the VM of NK cannot be freed, and
    // (2) we hold the pager critical section, thus the page cannot be paged out.

    if (IsAddrCommitted (g_pprcNK, addr)) {

        // check to see if it's already committed in kernel
        retval = fWrite ? CopyFromKernel(pprcDst, optr, addr) : PAGEIN_SUCCESS;

    } else {
        LPVOID pReservation = NULL;
        LPBYTE pPagingMem = NULL;
        BOOL   fUsePool;

        DEBUGMSG (ZONE_PAGING, (L"PageInModule: fWrite = %8.8lx\r\n", fWrite));
        DEBUGCHK (!PageAbleOptr (&pMod->oe, optr) || !fWrite);  // no writes to pageable memory

        // Get a page to write to
        fUsePool = (!fWrite && PageAbleOptr (&pMod->oe, optr));
        pPagingMem = GetPagingPage (g_pLoaderPool, &pReservation, addr, fUsePool,
                                    PAGE_EXECUTE_READWRITE);
        if (pPagingMem) {
            // call the paging function based on filetype
            retval = (FA_PREFIXUP & pMod->oe.filetype)
                ? PageInFromROM (pprc, optr, addr, &pMod->oe, pPagingMem)
                : PageInFromRAM (pprc, &pMod->oe, eptr, (DWORD) pMod->BasePtr,
                                 optr, addr, !(pMod->wFlags & LOAD_LIBRARY_AS_DATAFILE), pPagingMem);

            // VirtualCopy from NK to destination if paged-in successfully
            if (PAGEIN_SUCCESS == retval) {
                OEMCacheRangeFlush (pPagingMem, VM_PAGE_SIZE, CACHE_SYNC_WRITEBACK|CACHE_SYNC_INSTRUCTIONS);
                if (!VMFastCopy (pprc, addr, g_pprcNK, (DWORD) pPagingMem, VM_PAGE_SIZE, optr->o32_access)) {
                    retval = PAGEIN_RETRY;
                    DEBUGMSG(ZONE_PAGING,(L"  PageInModule: Page in failure in VC!\r\n"));
                }
            }

            // Decommit and release the temporary memory
            FreePagingPage (g_pLoaderPool, pPagingMem, pReservation, fUsePool, retval);

        } else {
            // Failed to get a page.  The loader pool trim thread must be at too
            // low of a priority to help this thread.

            

            retval = PAGEIN_RETRY;
        }
    }

    if ((PAGEIN_SUCCESS == retval) && (pprc != pprcDst)) {

        DEBUGCHK (!fWrite);

        // for RO or Shared-write sections not in slot 1, virtual copy from NK
        DEBUGMSG (ZONE_PAGING, (L"PageInModule: VirtualCopy from NK %8.8lx!\r\n"));

        if (!VMFastCopy (pprcDst, addr, g_pprcNK, addr, VM_PAGE_SIZE, optr->o32_access)) {
            DEBUGMSG(ZONE_PAGING,(L"PIM: VirtualCopy Failed 1\r\n"));
            retval = PAGEIN_RETRY;
        }
    }

    if (PAGEIN_SUCCESS == retval) {
        PagedInCount++;
    }

    return retval;
}


//------------------------------------------------------------------------------
// PageInProcess
//------------------------------------------------------------------------------
int PageInProcess (PPROCESS pProc, DWORD addr, BOOL fWrite)
{
    e32_lite *eptr = &pProc->e32;
    o32_lite *optr;
    BOOL     retval;
    LPVOID   pReservation = NULL;
    LPBYTE   pPagingMem = NULL;
    BOOL     fUsePool;

    DEBUGMSG (ZONE_PAGING, (L"PageInProcess: Paging in %8.8lx\r\n", addr));

    // use page start to page in
    addr &= -VM_PAGE_SIZE;

    if (!(optr = FindOptr (pProc->o32_ptr, eptr->e32_objcnt, addr))) {
        DEBUGMSG (ZONE_PAGING, (L"PIP: FindOptr failed\r\n"));
        return PAGEIN_FAILURE;
    }

    DEBUGCHK (!PageAbleOptr (&pProc->oe, optr) || !fWrite);  // no writes to pageable memory

    // Get a page to write to
    fUsePool = (!fWrite && PageAbleOptr (&pProc->oe, optr));
    pPagingMem = GetPagingPage (g_pLoaderPool, &pReservation, addr, fUsePool,
                                PAGE_EXECUTE_READWRITE);
    if (pPagingMem) {

        retval = (FA_PREFIXUP & pProc->oe.filetype)
            ? PageInFromROM (pProc, optr, addr, &pProc->oe, pPagingMem)
            : PageInFromRAM (pProc, &pProc->oe, eptr, (DWORD) pProc->BasePtr, optr, addr, TRUE, pPagingMem);

        // VirtualCopy from NK to destination if paged-in successfully
        if (PAGEIN_SUCCESS == retval) {
            OEMCacheRangeFlush (pPagingMem, VM_PAGE_SIZE, CACHE_SYNC_WRITEBACK|CACHE_SYNC_INSTRUCTIONS);
            if (!VMFastCopy (pProc, addr, g_pprcNK, (DWORD) pPagingMem, VM_PAGE_SIZE, optr->o32_access)) {
                retval = PAGEIN_RETRY;
                DEBUGMSG(ZONE_PAGING,(L"  PageInProcess: Page in failure in VC!\r\n"));
            }
        }

        // Decommit and release the temporary memory
        FreePagingPage (g_pLoaderPool, pPagingMem, pReservation, fUsePool, retval);

    } else {
        // Failed to get a page.  The loader pool trim thread must be at too
        // low of a priority to help this thread.

        

        retval = PAGEIN_RETRY;
    }

    if (PAGEIN_SUCCESS == retval) {
        PagedInCount++;
    }

    return retval;
}


//------------------------------------------------------------------------------
// Decommit pageable memory, and return pages to the page pool if necessary.
// fLastInstance=TRUE is used when the last instance of a DLL is unloaded from
// the system, or if an EXE is unloaded.
//------------------------------------------------------------------------------
BOOL
DecommitExe (
    PPROCESS  pprc,
    openexe_t *oeptr,
    e32_lite  *eptr,
    o32_lite  *o32_ptr, // Array of O32's from the module
    BOOL      fLastInstance
    )
{
    BOOL fRet = TRUE;
    
    if (PageAble (oeptr)) {
        int loop;

        for (loop = 0; loop < eptr->e32_objcnt; loop++) {
            o32_lite *optr = &o32_ptr[loop];

            if (PageAbleOptr (oeptr, optr)) {
                // Return the pages to the loader page pool
                fRet = VMDecommit (pprc, (LPVOID) optr->o32_realaddr, optr->o32_vsize,
                            fLastInstance ? (VM_PAGER_LOADER | VM_PAGER_POOL)
                                          : VM_PAGER_LOADER);
                if (!fRet)
                    break;
            }
        }
    }

    return fRet;
}


//------------------------------------------------------------------------------
// Worker function for DecommitModule.
// Use with EnumerateDList on NK process list
//------------------------------------------------------------------------------
static BOOL
DecommitModuleInProcess (
    PDLIST pItem,       // Pointer to process
    LPVOID pEnumData    // Pointer to module
    )
{
    PPROCESS pprc = (PPROCESS) pItem;
    PMODULE  pMod = (PMODULE) pEnumData;
    PDLIST   ptrav;
    BOOL fRet = FALSE; // by default, continue the enumeration

    DEBUGCHK (OwnModListLock ());  // Safely enumerate modules

    // Find the module inside this process.  Don't want to use FindModInProc
    // because that affects the list LRU order.  The module's already locked so
    // we just need to check if it's present.
    for (ptrav = pprc->modList.pFwd; ptrav != &pprc->modList; ptrav = ptrav->pFwd) {
        if (((PMODULELIST) ptrav)->pMod == pMod) {
            // The module is loaded into this process.
            fRet = IsProcessBeingDebugged(pprc) 
                   || !DecommitExe (pprc, &pMod->oe, &pMod->e32, pMod->o32_ptr, (pprc == g_pprcNK) ? TRUE : FALSE);
            break;
        }
    }

    return fRet;
}


//------------------------------------------------------------------------------
// Decommit pages for a DLL, from all processes.  The module should already be
// locked.
//------------------------------------------------------------------------------
static void
DecommitModule (
    PMODULE pMod
    )
{
    if (PageAble (&pMod->oe)) {
        // Don't want page-in to occur between paging out in all procs.  When
        // we get to NK, there should be no pages left in the other procs.
        // Lock modlist for DecommitModuleInProcess, in proper order with PagerCS.
        LockModuleList ();
        EnterCriticalSection (&PagerCS);

        // First decommit in all processes except NK
        EnumerateDList (&g_pprcNK->prclist, DecommitModuleInProcess, pMod);

        // Then decommit the NK pages and return them to the pool
        DecommitModuleInProcess ((PDLIST) g_pprcNK, (PVOID) pMod);

        LeaveCriticalSection (&PagerCS);
        UnlockModuleList ();
    }
}


//------------------------------------------------------------------------------
// Part of DoPageOut.  Page out DLL pages until enough memory is released.
//------------------------------------------------------------------------------
void
PageOutModules (
    BOOL fCritical
    )
{
    static int StoredModNum;
    int    ModCount;
    PDLIST ptrav;

    DEBUGMSG(ZONE_PAGING,(L"POM: Starting page free count: %d\r\n", PageFreeCount));

    DEBUGCHK (OwnCS (&PageOutCS));

    





    LockModuleList ();      // So that we can enumerate processes & NK modules, and safely track a start position

    // Start from the next module after the last one we paged out
    for (ptrav = g_pprcNK->modList.pFwd, ModCount = 0;
         (ptrav != &g_pprcNK->modList) && (ModCount < StoredModNum);
         ptrav = ptrav->pFwd, ModCount++)
        ;
    if (ptrav == &g_pprcNK->modList) {
        // Hit the end of the list
        ptrav = g_pprcNK->modList.pFwd;
        ModCount = 0;
        StoredModNum = 0;
    }

    //
    // Page out each module in the list until we run out of modules or get
    // enough memory.
    //

    do {
        // Page out the module
        DecommitModule ((PMODULE) ptrav);

        // Move to the next list entry
        ptrav = ptrav->pFwd;
        if (ptrav != &g_pprcNK->modList) {
            ModCount++;
        } else {
            ptrav = g_pprcNK->modList.pFwd;
            ModCount = 0;
        }

    } while ((ModCount != StoredModNum)
             && PGPOOLNeedsTrim (g_pLoaderPool, fCritical));

    // Record the number where we page out last
    StoredModNum = ModCount;

    UnlockModuleList ();

    DEBUGMSG(ZONE_PAGING,(L"POM: Ending page free count: %d\r\n", PageFreeCount));
}


//------------------------------------------------------------------------------
// Part of DoPageOut.  Page out EXE pages (except NK) until enough memory is released.
//------------------------------------------------------------------------------
void
PageOutProcesses (
    BOOL fCritical
    )
{
    PPROCESS pprc, pprcStart;

    DEBUGMSG(ZONE_PAGING, (L"POP: Starting page free count: %d\r\n", PageFreeCount));

    //
    // Page out each process in the list until we run out of processes or get
    // enough memory.
    //

    LockModuleList ();  // No process create/delete while enumerating

    pprc = PagePoolGetNextProcess ();
    pprcStart = pprc;
    while (pprc && PGPOOLNeedsTrim (g_pLoaderPool, fCritical)) {

        // Page out the process
        if (IsProcessNormal(pprc) && !IsProcessBeingDebugged(pprc)) {
            DecommitExe (pprc, &pprc->oe, &pprc->e32, pprc->o32_ptr, TRUE);
        }

        // Move to the next list entry
        pprc = PagePoolGetNextProcess ();
        if (pprc == pprcStart)
            break;
    }

    UnlockModuleList ();

    DEBUGMSG(ZONE_PAGING, (L"POP: Ending page free count: %d\r\n", PageFreeCount));
}


//------------------------------------------------------------------------------
// Worker function for DoPageOutModule. Use with EnumerateDList on a process modlist.
//------------------------------------------------------------------------------
static BOOL
DecommitDependency (
    PDLIST pItem,       // Pointer to module
    LPVOID pEnumData    // Flags from DoPageOutModule
    )
{
    PMODULE pMod = (PMODULE) pItem;
    DWORD   dwFlags = (DWORD) pEnumData;

    DEBUGCHK (OwnModListLock ());

    // Note, the wRefCnt=1 check will fail to catch locked modules
    if ((PAGE_OUT_ALL_DEPENDENT_DLL == dwFlags) // either the flags say to page out all
        || (1 == pMod->wRefCnt)) {              // or we're the only only process using this module
        DecommitModule (pMod);
    }

    return FALSE; // return FALSE to continue the enumeration
}


//------------------------------------------------------------------------------
// Implementation of PageOutModule API
//------------------------------------------------------------------------------
DWORD DoPageOutModule (PPROCESS pprc, PMODULE pMod, DWORD dwFlags)
{
    DWORD dwErr = 0;

    if ((PAGE_OUT_PROCESS_ONLY != dwFlags)
        && (PAGE_OUT_DLL_USED_ONLY_BY_THISPROC != dwFlags)
        && (PAGE_OUT_ALL_DEPENDENT_DLL != dwFlags)) {

        dwErr = ERROR_INVALID_PARAMETER;

    } else if (pMod) {
        // paging out a module
        PMODULELIST pEntry;

        LockModuleList ();
        pEntry = FindModInProc (pprc, pMod);
        if (pEntry) {
            pMod = pEntry->pMod;
            LockModule (pMod);
            UnlockModuleList ();

            DecommitModule (pMod);

            // Need to hold the kernel loader lock when unlocking DLLs in NK,
            // to guarantee serialization when calling DllMain.
            if (pprc == g_pprcNK)
                LockLoader (g_pprcNK);
            UnlockModule (pMod, pprc == g_pprcNK);
            if (pprc == g_pprcNK)
                UnlockLoader (g_pprcNK);

        } else {
            UnlockModuleList ();
            dwErr = ERROR_INVALID_HANDLE;
        }

    } else if ((pprc == g_pprcNK)        // cannot page out NK
               || (pprc == pActvProc) // page out current process - not a good idea
               || IsProcessBeingDebugged (pprc) // process is under app debugger - disallow paging
        ) {
        dwErr = ERROR_INVALID_HANDLE;

    } else {
        // paging out a process
        DecommitExe (pprc, &pprc->oe, &pprc->e32, pprc->o32_ptr, TRUE);

        // pageout dependent modules based on flags
        if (PAGE_OUT_PROCESS_ONLY != dwFlags) {
            LockModuleList ();
            EnumerateDList (&pprc->modList, DecommitDependency, (LPVOID) dwFlags);
            UnlockModuleList ();
        }
    }

    if (dwErr)
        KSetLastError (pCurThread, dwErr);

    return dwErr;
}


static BOOL ReadO32FromFile (openexe_t *oeptr, DWORD dwOffset, o32_obj *po32)
{
    ulong   bytesread;

    DEBUGCHK (!(FA_PREFIXUP & oeptr->filetype));
    return (FSSetFilePointer (oeptr->hf, dwOffset, 0, FILE_BEGIN) == dwOffset)
        && FSReadFile (oeptr->hf, (LPBYTE)po32, sizeof(o32_obj), &bytesread, 0)
        && (bytesread == sizeof(o32_obj))
        && (!po32->o32_psize || (FSSetFilePointer(oeptr->hf,0,0,FILE_END) >= po32->o32_dataptr + po32->o32_psize));
}


DWORD ReadSection (openexe_t *oeptr, o32_lite *optr, DWORD idxOptr)
{
    DWORD ftype     = oeptr->filetype;
    LPBYTE realaddr = (LPBYTE) optr->o32_realaddr;
    PPROCESS pprc   = IsKModeAddr ((DWORD) realaddr)? g_pprcNK : pActvProc;
    ulong cbRead;

    // take care of R/O, uncompressed XIP section
    if ((FA_XIP & ftype)                                                            // XIP-able?
        && !(optr->o32_flags & (IMAGE_SCN_COMPRESSED | IMAGE_SCN_MEM_WRITE))) {     // not (compressed or writeable)

        // R/O, uncompressed
        DEBUGCHK (optr->o32_vsize <= optr->o32_psize);
        if (FA_DIRECTROM & ftype) {
            // in 1st-XIP. VirtualCopy directly
            DEBUGMSG(ZONE_LOADER1,(TEXT("virtualcopying %8.8lx <- %8.8lx (%lx)!\r\n"),
                    realaddr, optr->o32_dataptr, optr->o32_vsize));
            if (!VMFastCopy (pprc, (DWORD) realaddr, g_pprcNK, optr->o32_dataptr, optr->o32_vsize, optr->o32_access)) {
                return ERROR_OUTOFMEMORY;
            }
        } else {
            // in external XIP-able media, call to filesys
            // allocate spaces for the pages
            DWORD cPages = PAGECOUNT (optr->o32_vsize);

            // we use _alloca here as 1 page can handle up to 4M of address space. We might have to
            // revisit if we have huge DLLs (> 4M per-section) to prevent stack overflow.
            LPDWORD pPages = _alloca (cPages * sizeof (DWORD));
            if (!pPages) {
                return ERROR_OUTOFMEMORY;
            }

            // call to filesys to get the pages
            if (!FSIoctl (oeptr->hf, IOCTL_BIN_GET_XIP_PAGES, &idxOptr, sizeof(DWORD), pPages, cPages * sizeof(DWORD), NULL, NULL)) {
                return ERROR_BAD_EXE_FORMAT;
            }

            // update VM
            if (!VMSetPages (pprc, (DWORD) realaddr, pPages, cPages, optr->o32_access)) {
                return ERROR_OUTOFMEMORY;
            }
        }
        return 0;
    }

    // do nothing if pageable
    if (PageAble (oeptr)                                    // file pageable
        && !(IMAGE_SCN_MEM_NOT_PAGED & optr->o32_flags)) {  // section pageable

        return 0;
    }

    // commit memory
    if (!VMCommit (pprc, realaddr, optr->o32_vsize, PAGE_EXECUTE_READWRITE, (IMAGE_SCN_MEM_WRITE & optr->o32_flags)? PM_PT_ZEROED : PM_PT_ANY))
        return ERROR_OUTOFMEMORY;

    cbRead = optr->o32_vsize;

    if (FA_DIRECTROM & ftype) {
        // 1st XIP region
        if (optr->o32_flags & IMAGE_SCN_COMPRESSED) {
            // compressed, just call decompress routine
            DecompressROM ((LPVOID)(optr->o32_dataptr), optr->o32_psize,
                realaddr, optr->o32_vsize,0);
            DEBUGMSG(ZONE_LOADER1,(TEXT("(4) realaddr = %8.8lx\r\n"), realaddr));

        } else {
            // uncompressed, read min (psize, psize) and memset the rest to 0
            if (cbRead > optr->o32_psize) {
                cbRead = optr->o32_psize;
            }
            memcpy (realaddr, (LPVOID)(optr->o32_dataptr), cbRead);
            DEBUGMSG(ZONE_LOADER1,(TEXT("(5) realaddr = %8.8lx, psize = %8.8lx\r\n"),
                realaddr, optr->o32_psize));
        }
    } else if (FA_PREFIXUP & ftype) {
        // EXTIMAGE or EXTXIP
        if (!FSReadFileWithSeek (oeptr->hf, realaddr, optr->o32_vsize, &cbRead, NULL, (DWORD) optr->o32_dataptr, 0)
            || (cbRead != optr->o32_vsize)) {
            return ERROR_BAD_EXE_FORMAT;
        }
    } else {
        // ordinary file
        DWORD cbToRead = min(optr->o32_psize, optr->o32_vsize);
        if ((FSSetFilePointer (oeptr->hf, optr->o32_dataptr, 0, FILE_BEGIN) != optr->o32_dataptr)
            || !FSReadFile (oeptr->hf, realaddr, cbToRead, &cbRead, 0)
            || (cbToRead != cbRead)) {
            return ERROR_BAD_EXE_FORMAT;
        }
    }

    // clean the rest of the page as the pages we get are from dirty list.
    memset (realaddr + cbRead, 0, PAGEALIGN_UP(optr->o32_vsize) - cbRead);
    return 0;
}

static BOOL IsIATInROSection (o32_lite *optr, DWORD dwIatRva)
{
    return !(optr->o32_flags & IMAGE_SCN_MEM_WRITE)
        && ((DWORD) (dwIatRva - optr->o32_rva) < optr->o32_vsize);
}

//------------------------------------------------------------------------------
// Adjust permissions on regions, decommitting discardable ones
//------------------------------------------------------------------------------
void
AdjustRegions(
    const BYTE *pBaseAddr,
    openexe_t *oeptr,
    e32_lite *eptr,
    o32_lite *oarry
    )
{
    int loop;
    BOOL  fNoPaging = !PageAble (oeptr);
    BOOL  fSectionNoPaging;
    DWORD dwIatRva = 0;

    if (!(oeptr->filetype & FA_XIP) && eptr->e32_unit[IMP].size && !eptr->e32_sect14rva) {
        const struct ImpHdr *pImpInfo = (const struct ImpHdr *) (pBaseAddr + eptr->e32_unit[IMP].rva);
        DEBUGCHK (eptr->e32_unit[IMP].rva);
        VERIFY (CeSafeCopyMemory (&dwIatRva, &pImpInfo->imp_address, sizeof (DWORD)));
    }

    for (loop = 0; loop < eptr->e32_objcnt; loop++, oarry ++) {

        fSectionNoPaging = fNoPaging || (IMAGE_SCN_MEM_NOT_PAGED & oarry->o32_flags);
        //
        // when linked with -optidata, IAT can be read-only, need to change it to read/write
        //
        if (IsIATInROSection (oarry, dwIatRva)) {
            DEBUGMSG (1, (L"Import Table in R/O section, changed to R/W\r\n"));
            DEBUGCHK (!(oeptr->filetype & FA_XIP));

            oarry->o32_flags |= IMAGE_SCN_MEM_WRITE|IMAGE_SCN_MEM_SHARED;   // change it to R/W, shared
            oarry->o32_access = AccessFromFlags (oarry->o32_flags);

            if (!fSectionNoPaging) {

                // pageable - decommit the section such that it'll be paged in later as r/w
                VMDecommit (pActvProc, (LPVOID)oarry->o32_realaddr,oarry->o32_vsize,VM_PAGER_LOADER);
            }
        }

        if (fSectionNoPaging) {
            if (oarry->o32_flags & IMAGE_SCN_MEM_DISCARDABLE) {
                VMDecommit (pActvProc, (LPVOID)oarry->o32_realaddr,oarry->o32_vsize,VM_PAGER_NONE);
                oarry->o32_realaddr = 0;
            } else {
                VMProtect (pActvProc, (LPVOID)oarry->o32_realaddr,oarry->o32_vsize,oarry->o32_access, NULL);
            }
        }
    }

    // set the eptr->e32_unit[FIX] to 0 so we won't relocate anymore
    if (fNoPaging) {
        eptr->e32_unit[FIX].size = 0;
    }

}


DWORD
ReadO32Info (
    openexe_t *oeptr,
    e32_lite *eptr,
    o32_lite *oarry,
    LPDWORD pdwFlags
        )
{
    int         loop;
    o32_obj     o32;
    o32_lite    *optr;
    DWORD       dwErr = 0;

    if ((FA_PREFIXUP & oeptr->filetype)         // in module section
        && !(FA_DIRECTROM & oeptr->filetype)    // not in 1st XIP
        && !ReadExtImageInfo (oeptr->hf, IOCTL_BIN_GET_O32, eptr->e32_objcnt * sizeof(o32_lite), oarry)) {
        return ERROR_BAD_EXE_FORMAT;
    }

    for (loop = 0, optr = oarry; loop < eptr->e32_objcnt; loop++) {

        DEBUGMSG(ZONE_LOADER2,(TEXT("ReadO32Info : get section information %d\r\n"),loop));

        if (FA_PREFIXUP & oeptr->filetype) {
            // in module section

            // 1st XIP, grab the o32 information from ROM, otherwise it's already read at the beginning of the function
            if (FA_DIRECTROM & oeptr->filetype) {
                o32_rom *o32rp = (o32_rom *)(oeptr->tocptr->ulO32Offset+loop*sizeof(o32_rom));
                DEBUGMSG(ZONE_LOADER1,(TEXT("(2) o32rp->o32_realaddr = %8.8lx\r\n"), o32rp->o32_realaddr));
                optr->o32_realaddr = o32rp->o32_realaddr;
                optr->o32_vsize = o32rp->o32_vsize;
                optr->o32_psize = o32rp->o32_psize;
                optr->o32_rva = o32rp->o32_rva;
                optr->o32_flags = o32rp->o32_flags;
                if (optr->o32_rva == eptr->e32_unit[RES].rva)
                    optr->o32_flags &= ~IMAGE_SCN_MEM_DISCARDABLE;
                optr->o32_dataptr = o32rp->o32_dataptr;
            }


            if (!(optr->o32_flags & (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_COMPRESSED | IMAGE_SCN_TYPE_NO_LOAD))   // not writeable or compresses
                && (optr->o32_dataptr & (VM_PAGE_SIZE-1))) {                           // and not page aligned
                NKDbgPrintfW (L"ReadO32Info FAILED: XIP code section not page aligned, o32_dataptr = %8.8lx, o32_realaddr = %8.8lx\r\n",
                    optr->o32_dataptr, optr->o32_realaddr);

                DEBUGCHK (0);
                dwErr = ERROR_BAD_EXE_FORMAT;
                break;
            }

            optr->o32_access = AccessFromFlags(optr->o32_flags);

        } else {
            // in file section
            // RAM files, read O32 header from file
            if (!ReadO32FromFile (oeptr, oeptr->offset+sizeof(e32_exe)+loop*sizeof(o32_obj), &o32)) {
                dwErr = ERROR_BAD_EXE_FORMAT;
                break;
            }

            // validate o32
            if (   (o32.o32_rva & VM_PAGE_OFST_MASK)                    // section not page aligned
                || (o32.o32_rva > eptr->e32_vsize)                      // rva too big
                || (o32.o32_vsize > eptr->e32_vsize)                    // size too big
                || ((o32.o32_rva+o32.o32_vsize) > eptr->e32_vsize)) {   // rva+vsize too big

                RETAILMSG (1, (L"Bad EXE/DLL format, rva = %8.8lx, size = %8.8lx\r\n", optr->o32_rva, optr->o32_vsize));
                dwErr = ERROR_BAD_EXE_FORMAT;
                break;
            }

            // update optr
            optr->o32_realaddr = eptr->e32_vbase + o32.o32_rva;
            optr->o32_vsize = o32.o32_vsize;
            optr->o32_rva = o32.o32_rva;
            optr->o32_flags = o32.o32_flags;
            optr->o32_psize = o32.o32_psize;
            optr->o32_dataptr = o32.o32_dataptr;

            if (optr->o32_rva == eptr->e32_unit[RES].rva) {
                optr->o32_flags &= ~IMAGE_SCN_MEM_DISCARDABLE;

                // BC: any pre-6.0 apps has resource section R/W
                if (eptr->e32_cevermajor < 6) {
                    optr->o32_flags |= IMAGE_SCN_MEM_WRITE;
                }
            }

            optr->o32_access = AccessFromFlags(optr->o32_flags);
        }

        if (!(IMAGE_SCN_TYPE_NO_LOAD & optr->o32_flags)) {

            // if we have per-section not-pageable flag, set the DLL to be pageable if in MODULE section,
            // or not pageable if in FILE section.
            if (PagingSupported (oeptr) && (IMAGE_SCN_MEM_NOT_PAGED & optr->o32_flags)) {
                oeptr->pagemode = (FA_PREFIXUP & oeptr->filetype)? PM_PARTPAGING : PM_NOPAGING;
            }

            optr ++;

        }
    }

    if (!dwErr) {
        // take into account the "Do Not Load" section, and update the object count
        eptr->e32_objcnt = optr - oarry;
    }
    else
    {
        // if fail, we don't allocate any section resource, set obj count to 0
        eptr->e32_objcnt = 0;
    }

    DEBUGMSG (dwErr && ZONE_LOADER1, (L"ReadO32Info Failed, dwErr = %8.8lx\n", dwErr));

    return dwErr;
}

//------------------------------------------------------------------------------
// Load all O32 headers
//------------------------------------------------------------------------------
DWORD
LoadO32(
    openexe_t *oeptr,
    e32_lite *eptr,
    o32_lite *oarry,
    ulong BasePtr
    )
{
    int         loop;
    o32_lite    *optr;
    DWORD       dwErr = 0;

    DWORD dwOfst = BasePtr - eptr->e32_vbase;

    DEBUGCHK ((pActvProc == g_pprcNK) || ((DWORD) BasePtr < VM_DLL_BASE));

    for (loop = 0, optr = oarry; loop < eptr->e32_objcnt; loop++) {

        if (FA_PREFIXUP & oeptr->filetype) {
            // in module section

            // update realaddr if not 'Z' module.
            if (!IsKernelVa ((LPCVOID) BasePtr)) {
                optr->o32_realaddr = BasePtr + optr->o32_rva;
            }

        } else {
 
            // update optr
            optr->o32_realaddr += dwOfst;
            DEBUGMSG(ZONE_LOADER1,(TEXT("(1) optr->o32_realaddr = %8.8lx\r\n"), optr->o32_realaddr));
        }

        if (!(IMAGE_SCN_TYPE_NO_LOAD & optr->o32_flags)) {

            // Only Dlls with 'Z' flag has base address in kernel. Do not call "ReadSection"
            // on those Dlls for they're already in place.
            if (!IsKernelVa ((LPVOID) BasePtr)) {
                // Do not read the R/W section of ROM Dll, they'll be read into per-process
                // VM or we'll use double memory
                if (((int) BasePtr > VM_DLL_BASE)
                    && (optr->o32_flags & IMAGE_SCN_MEM_WRITE)
                    && !(optr->o32_flags & IMAGE_SCN_MEM_SHARED)
                    && (FA_PREFIXUP & oeptr->filetype)) {
                    DEBUGMSG (ZONE_LOADER1, (L"LO32: Do not read R/W section %8.8lx of size %8.8lx\r\n",
                        optr->o32_realaddr, optr->o32_vsize));

                } else if (dwErr = ReadSection (oeptr, optr, loop)) {
                    break;
                }
            }

            optr ++;

        }
    }

    if (!dwErr) {
        if (IsKernelVa ((LPVOID) BasePtr)) {
            UpdateKmodVSize (oeptr, eptr);
        }
    }

    DEBUGMSG (dwErr && ZONE_LOADER1, (L"LoadO32 Failed, dwErr = %8.8lx\n", dwErr));

    return dwErr;
}



//------------------------------------------------------------------------------
// Find starting IP for EXE (or entrypoint for DLL)
//------------------------------------------------------------------------------
ulong
FindEntryPoint(
    DWORD entry,
    e32_lite *eptr,
    o32_lite *oarry
    )
{
    o32_lite *optr;
    int loop;

    for (loop = 0; loop < eptr->e32_objcnt; loop++) {
        optr = &oarry[loop];
        if ((entry >= optr->o32_rva) ||
            (entry < optr->o32_rva+optr->o32_vsize)) {
            DEBUGMSG (ZONE_LOADER2, (L"FindEntryPoint realaddr = %8.8lx, entry = %8.8lx, rva = %8.8lx\n",
                optr->o32_realaddr, entry, optr->o32_rva));
            return optr->o32_realaddr + entry - optr->o32_rva;
        }
    }
    return 0;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static DWORD
CalcStackSize(
    DWORD cbStack
    )
{
    if (cbStack < KRN_STACK_SIZE) {
        cbStack = KRN_STACK_SIZE;
    } else if (cbStack > MAX_STACK_RESERVE) {
        cbStack = MAX_STACK_RESERVE;
    } else {
        cbStack = BLOCKALIGN_UP (cbStack);
    }
    return cbStack;
}


//
// save commnad line and program name on stack and return the new stack top
//
static LPBYTE InitCmdLineAndPrgName (LPCWSTR *ppszCmdLine, LPCWSTR pszPrgName, LPBYTE pStk, DWORD cbStk)
{
    DWORD cbCmdLine = (NKwcslen(*ppszCmdLine)+1) * sizeof(WCHAR);
    DWORD cbPrgName = (NKwcslen(pszPrgName)+1) * sizeof(WCHAR);

    // copy command line onto stack
    pStk = INITIAL_SP (pStk, cbStk) - ALIGNSTK(cbCmdLine);
    memcpy (pStk, *ppszCmdLine, cbCmdLine);
    *ppszCmdLine = (LPCWSTR) pStk;

    // save program name onto stack
    pStk -= ALIGNSTK (cbPrgName);
    memcpy (pStk, pszPrgName, cbPrgName);

    return pStk;
}

void MDSwitchToUserCode (FARPROC pfn, REGTYPE *pArgs);

//------------------------------------------------------------------------------
// Helper function to create a new proces. Will NOT return if succeeds
//------------------------------------------------------------------------------
DWORD
CreateNewProcHelper(
    LPProcStartInfo lppsi
    )
{
    PTHREAD     pth = pCurThread;
    PPROCESS    pprc = pActvProc;
    e32_lite    *eptr = &pprc->e32;
    LPVOID      lpStack = NULL;
    DWORD       flags, entry, dwErr = 0, len;
    LPCWSTR     procname = GetPlainName (lppsi->pszImageName);
    LPCWSTR     pCmdLine;
    LPWSTR      uptr;
    PNAME       pAlloc;
    LPBYTE      pCurSP;
    REGTYPE     *pArgs;

    DEBUGCHK (OwnLoaderLock (pprc));

    eptr = &pprc->e32;

    if (dwErr = LoadE32 (&pprc->oe, eptr, &flags, &entry, 0)) {
        DEBUGMSG(1,(TEXT("ERROR! LoadE32 failed, dwErr = %8.8lx!\r\n"), dwErr));
        return dwErr;
    }

    if (flags & IMAGE_FILE_DLL) {
        DEBUGMSG(1,(TEXT("ERROR! CreateProcess on DLL!\r\n")));
        return  ERROR_BAD_EXE_FORMAT;
    }

    if ((flags & IMAGE_FILE_RELOCS_STRIPPED) && ((eptr->e32_vbase < 0x10000) || (eptr->e32_vbase > 0x400000))) {
        DEBUGMSG(1,(TEXT("ERROR! No relocations - can't load/fixup!\r\n")));
        return ERROR_BAD_EXE_FORMAT;
    }

    pprc->BasePtr = (flags & IMAGE_FILE_RELOCS_STRIPPED) ? (LPVOID)eptr->e32_vbase : (LPVOID)0x10000;
    DEBUGMSG(ZONE_LOADER1,(TEXT("BasePtr is %8.8lx\r\n"),pprc->BasePtr));
    len = (NKwcslen (procname) + 1) * sizeof(WCHAR);

    pAlloc = AllocName(eptr->e32_objcnt*sizeof(o32_lite) + len + 2); // +2 for DWORD alignment
    if (!pAlloc) {
        return ERROR_OUTOFMEMORY;
    }

    DEBUGCHK (IsDwordAligned(pAlloc));

    pprc->o32_ptr = (o32_lite *) ((DWORD) pAlloc + 4);  // skip header info
    uptr = (LPWSTR) (pprc->o32_ptr + eptr->e32_objcnt);
    memcpy (uptr, procname, len);
    pprc->lpszProcName = uptr;

    dwErr = ReadO32Info (&pprc->oe, eptr, pprc->o32_ptr, &flags);
    if (dwErr) {
        return dwErr;
    }

    // reserve/commit memory for the EXE
    if (!VMAlloc (pprc, pprc->BasePtr, eptr->e32_vsize, MEM_RESERVE|MEM_IMAGE, PAGE_NOACCESS)) {
        return ERROR_OUTOFMEMORY;
    }

    // optimization for resoure loading - rebase resource rva
    if (eptr->e32_unit[RES].rva) {
        eptr->e32_unit[RES].rva += (DWORD) pprc->BasePtr;
    }

    if (dwErr = LoadO32(&pprc->oe,eptr,pprc->o32_ptr,(ulong)pprc->BasePtr)) {
        DEBUGMSG(1,(TEXT("ERROR! LoadO32 failed, dwerr = %8.8lx!\r\n"), dwErr));
        return dwErr;
    }

    if (!(pprc->oe.filetype & FA_PREFIXUP)) {
        if (!PageAble (&pprc->oe) && !Relocate(eptr, pprc->o32_ptr, (ulong)pprc->BasePtr)) {
            return ERROR_OUTOFMEMORY;
        }
    }

    // allocate stack
    eptr->e32_stackmax = CalcStackSize(eptr->e32_stackmax);

    if (!(lpStack = VMCreateStack (pprc, eptr->e32_stackmax))) {
        return ERROR_OUTOFMEMORY;
    }

    pth->tlsNonSecure = TLSPTR (lpStack, eptr->e32_stackmax);

    pCmdLine = lppsi->pszCmdLine? lppsi->pszCmdLine : L"";

    DEBUGMSG (ZONE_LOADER1, (L"Initializeing user stack cmdline = '%s', program = '%s', stack = %8.8lx\r\n",
        pCmdLine, pprc->lpszProcName, lpStack));

    // Upon return of InitCmdLineAndPrgName, pCmdLine will be pointing to user accessible command line, and
    // pCurSP is the user accessible program name. pCurSP is also the top of stack inuse
    pCurSP = InitCmdLineAndPrgName (&pCmdLine, pprc->lpszProcName, lpStack, eptr->e32_stackmax);

    InitModInProc (pprc, (PMODULE) hCoreDll);

    LockLoader (g_pprcNK);
    // load MUI if it exist
    pprc->pmodResource = LoadMUI (NULL, pprc->BasePtr, eptr);
    UnlockLoader (g_pprcNK);

    AdjustRegions (pprc->BasePtr, &pprc->oe, eptr, pprc->o32_ptr);

    pth->dwStartAddr = (DWORD) pprc->BasePtr + entry;

    DEBUGMSG(ZONE_LOADER1,(TEXT("Starting up the process!!! IP = %8.8lx\r\n"), pth->dwStartAddr));

    UnlockLoader (pprc);

    (* HDModLoad) ((DWORD)pprc);

    CELOG_ProcessCreateEx (pprc);

    // setup arguments to MainThreadBaseFunc
    pArgs = (REGTYPE*) (pCurSP - 8 * REGSIZE);

    pArgs[0] = pth->dwStartAddr;            // arg1 - entry point
    pArgs[1] = (REGTYPE) pCurSP;            // arg2 - program name (returned value form InitCmdLineAndPrgName)
    pArgs[2] = (REGTYPE) pCmdLine;          // arg3 - command line
    pArgs[3] = (REGTYPE) hCoreDll;          // arg4 - hCoredll
    pArgs[4] = (REGTYPE) pprc->pmodResource;                // arg5 - MUI DLL of EXE
    pArgs[5] = (REGTYPE) ((PMODULE) hCoreDll)->pmodResource;// arg6 - MUI DLL of coredll

#ifdef x86
    {
        NK_PCR *pcr = TLS2PCR (pth->tlsNonSecure);
        // terminate registration pointer
        pcr->ExceptionList = 0;

        // setup return address on stack
        pArgs --;
        *pArgs = 0;     // return address set to 0
    }
#endif
    pth->tlsNonSecure[PRETLS_STACKBOUND] = (DWORD) pArgs & ~(VM_PAGE_SIZE-1);

    // update thread info
    pth->dwOrigBase = (DWORD) lpStack;
    pth->dwOrigStkSize = eptr->e32_stackmax;

    // write-back d-cache and flush i-cache
    OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_WRITEBACK|CACHE_SYNC_INSTRUCTIONS);

    // notify PSL of process creation
    NKPSLNotify (DLL_PROCESS_ATTACH, pprc->dwId, pth->dwId);

    //
    // if create suspended, we need to suspend ourselves before releasing the creator. Or
    // there can be a race condition where the creator calls "ResumeTherad" before we suspend,
    // and the result is that the thread will be suspended forever.
    //
    // It's safe to modify bPendSusp here for no-one, but kernel has a handle to this thread.
    // and we know kernel will not suspend this thread for sure.
    //
    if (CREATE_SUSPENDED & lppsi->fdwCreate) {
        pth->bPendSusp = 1;
    }

    // notify caller process creation complete
    ForceEventModify (GetEventPtr (pprc->phdProcEvt), EVENT_SET);

    // creator signaled, we can suspend here if needed
    if (pth->bPendSusp) {
        KCall ((PKFN) SuspendSelfIfNeeded);
        CLEAR_USERBLOCK (pth);
    }

    // in case we got terminated even before running user code, just go straight to NKExitThread
    if (GET_DYING (pth)) {
        NKExitThread (0);
        // never return
        DEBUGCHK (0);
    }

    // update process state
    pprc->bState = PROCESS_STATE_NORMAL;

    // machine dependent main thread startup
    DEBUGMSG (ZONE_LOADER1,(TEXT("CreateNewProcHelper: Switch to User code at %8.8lx!\r\n"), pth->dwStartAddr));
    MDSwitchToUserCode ((FARPROC) MTBFf, pArgs);
    DEBUGCHK (0);   // never return
    return 0;       // keep compiler happy
}

//------------------------------------------------------------------------------
// Startup thread for new process (does loading)
//------------------------------------------------------------------------------
void
CreateNewProc(
    ulong nextfunc,
    ulong param
    )
{
    PPROCESS pprc = pActvProc;
    LPProcStartInfo lppsi = (LPProcStartInfo)param;
    DWORD dwErr = 0;
    DWORD dwFlags = LDROE_ALLOW_PAGING;

    DEBUGCHK(RunList.pth == pCurThread);

    DEBUGMSG(ZONE_LOADER1,(TEXT("CreateNewProc %s\r\n"),lppsi->pszImageName));

    LockModuleList ();
    AddToDListTail (&g_pprcNK->prclist, &pprc->prclist);
    UnlockModuleList ();

    if (lppsi->fdwCreate & (DEBUG_PROCESS|DEBUG_ONLY_THIS_PROCESS)) {
        dwFlags |= LDROE_OPENDEBUG;
    }

    dwErr = OpenExecutable (NULL, lppsi->pszImageName, &pprc->oe, lppsi->pthCreator->pprcOwner->hTok, &pprc->hTok, dwFlags);

    if (!dwErr) {

        if (!ChkDebug (&pprc->oe)) {
            dwErr = NTE_BAD_SIGNATURE;

        } else {

            DEBUGMSG(ZONE_LOADER1,(TEXT("image name = '%s'\r\n"), lppsi->pszImageName));

            pCurThread->hTok = pprc->hTok;

            LockLoader (pprc);
            dwErr = CreateNewProcHelper (lppsi);
            UnlockLoader (pprc);
            // fail to create process if the helper returns
        }
    }


    // fail to create the process, update error code and release the resources
    lppsi->dwErr = dwErr;

    DEBUGMSG(1,(TEXT("CreateNewProc failure on %s!, dwErr = %8.8lx\r\n"),lppsi->pszImageName, dwErr));
    CloseExe(&pprc->oe);
    ForceEventModify (GetEventPtr (pprc->phdProcEvt), EVENT_SET);
    HNDLCloseAllHandles (pprc);

    // so we don't call GCFT, for coredll isn't loaded in the process.
    *(__int64 *)&pCurThread->ftCreate = 0;

    NKExitThread (0);
    DEBUGCHK(0);
    // Never returns
}

//------------------------------------------------------------------------------
// Copy DLL regions from NK to current process
//------------------------------------------------------------------------------
DWORD CopyRegions (PMODULE pMod)
{
    PPROCESS pprc = pActvProc;
    DWORD dwErr = 0;

    DEBUGCHK ((pprc != g_pprcNK) && !(pMod->wFlags & LOAD_LIBRARY_IN_KERNEL));

    if ((FA_XIP & pMod->oe.filetype) || !FullyPageAble (&pMod->oe)) {

        int                 loop;
        struct o32_lite     *optr;
        LPBYTE              addr;
        BOOL                fNoPaging;


        // XIP or the module is not fully pageable.
        for (loop = 0, optr = pMod->o32_ptr; loop < pMod->e32.e32_objcnt; loop ++, optr ++) {

            addr = (LPBYTE) optr->o32_realaddr;
            fNoPaging = (optr->o32_flags & IMAGE_SCN_MEM_NOT_PAGED) || !PageAble (&pMod->oe);

            if (!(optr->o32_flags & IMAGE_SCN_MEM_SHARED) && (optr->o32_flags & IMAGE_SCN_MEM_WRITE)) {

                // RW, not shared,
                if (fNoPaging) {
                    if (FA_PREFIXUP & pMod->oe.filetype) {
                        if (dwErr = ReadSection (&pMod->oe, optr, loop)) {
                            break;
                        }
                    } else {
                        DEBUGMSG(ZONE_LOADER1,(TEXT("CopyRegion: Commiting %8.8lx\n"), addr));
                        if (!VMCommit (pprc, addr, optr->o32_vsize, PAGE_EXECUTE_READWRITE, PM_PT_ANY)) {
                            DEBUGMSG(ZONE_LOADER1,(TEXT("CopyRegion: VirtualAlloc %8.8lx (0x%x)\n"),
                                    addr, optr->o32_vsize));
                            dwErr = ERROR_OUTOFMEMORY;
                            break;
                        }
                        if (dwErr = VMReadProcessMemory (g_pprcNK, addr, addr, optr->o32_vsize)) {
                            DEBUGMSG(ZONE_LOADER1,(TEXT("CopyRegion: VMReadProcessMemory %8.8lx (0x%x), returns %d\n"),
                                    addr, optr->o32_vsize, dwErr));
                            break;
                        }

                        // memset the tail of page to 0
                        memset (addr + optr->o32_vsize, 0, PAGEALIGN_UP(optr->o32_vsize) - optr->o32_vsize);

                    }
                }

            } else if (!(optr->o32_flags & IMAGE_SCN_MEM_DISCARDABLE)) {
                // RO or shared sections
                if (fNoPaging
                    || ((FA_XIP & pMod->oe.filetype)
                        && !(optr->o32_flags & (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_COMPRESSED)))) {
                    if (!VMFastCopy (pprc, (DWORD) addr, g_pprcNK, (DWORD) addr, optr->o32_vsize, optr->o32_access)) {
                        DEBUGMSG(ZONE_LOADER1,(TEXT("CopyRegion: VMCopy failed addr = %8.8lx size = %8.8lx\n"),
                                addr, optr->o32_vsize));
                        dwErr = ERROR_OUTOFMEMORY;
                        break;
                    }
                }
            }
        }
    }


    return dwErr;
}

static DWORD TryOpenDll (PPROCESS pprc, PMODULE pMod, LPWSTR pszName, DWORD dwMaxLen, DWORD dwCurLen, HANDLE hTok, DWORD dwFlags, BOOL fTryAppend)
{
    DWORD dwErr;
    
    // try the path name without adding any suffix
    DEBUGMSG (ZONE_LOADER1, (L"Trying DLL '%s'\r\n", pszName));
    dwErr = OpenExecutable (pprc, pszName, &pMod->oe, hTok, NULL, dwFlags);
    
    if ((ERROR_FILE_NOT_FOUND == dwErr) && fTryAppend && (dwCurLen < dwMaxLen - 4)) {
        memcpy (pszName+dwCurLen, L".dll", 10);        // 10 == sizeof (L".dll")
        DEBUGMSG (ZONE_LOADER1, (L"Trying DLL '%s'\r\n", pszName));
        dwErr = OpenExecutable (pprc, pszName, &pMod->oe, hTok, NULL, dwFlags);
    }
    return dwErr;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD InitModule (PPROCESS pprc, PMODULE pMod, LPCWSTR lpszFileName, LPCWSTR lpszDllName, DWORD fLbFlags, WORD wFlags)
{
    e32_lite    *eptr;
    PNAME       pAlloc = 0;
    HANDLE      hTok = pCurThread->pprcOwner->hTok;
    DWORD       entry, dwErr = ERROR_FILE_NOT_FOUND, len, loop, flags;
    WCHAR       szName[MAX_PATH] = L"\\windows\\k.";
    DWORD       dwFlags = wFlags | ((fLbFlags & LLIB_NO_PAGING)? 0 : LDROE_ALLOW_PAGING);

    BOOL        fTryAppend;
    LPCWSTR     pszSuffix;


    DEBUGCHK (pActvProc == g_pprcNK);

    // initialize pMod fields
    memset (pMod, 0, sizeof (MODULE));

    pMod->lpSelf = pMod;
    pMod->wFlags = wFlags;
    pMod->wRefCnt = 1;

    // for every path we tried, we need to make several calls to filesys based on the searching path.
    // For optimization, we'll not try to append ".DLL" if the name has a well known suffix.
    // Namely: ".EXE", ".DLL", ".MUI", and ".CPL"
    len = NKwcslen (lpszDllName);
    pszSuffix = &lpszDllName[len-3];
    fTryAppend =    (len <= 4)                      // length < 4 -> try append
                 || ('.' != lpszDllName[len-4])     // not '.XXX" -> try append
                 || (   NKwcsicmp (pszSuffix, L"dll")      // not .dll
                     && NKwcsicmp (pszSuffix, L"exe")      // not .exe
                     && NKwcsicmp (pszSuffix, L"MUI")      // not .MUI
                     && NKwcsicmp (pszSuffix, L"CPL"));    // not .cpl


    if (LOAD_LIBRARY_IN_KERNEL & wFlags) {
        if (!(DONT_RESOLVE_DLL_REFERENCES & wFlags)) {
            pMod->wFlags |= MF_LOADING;
        }
        if (NKwcsnicmp (lpszDllName, L"k.", 2)) {
            // kernel mode DLLs, and not starting with "k.", try k.dllname.dll first.
            // NOTE: only search windows directory
            len = CopyPath (&szName[11], lpszDllName, MAX_PATH - 11);

            if (len) {
                dwErr = TryOpenDll (pprc, pMod, szName, MAX_PATH, len+11, hTok, dwFlags, fTryAppend);
            }
        }
    }

    if (ERROR_FILE_NOT_FOUND == dwErr) {
        len = CopyPath (szName, lpszFileName, MAX_PATH);

        if (!len) {
            dwErr = ERROR_BAD_PATHNAME;
        } else {
            dwErr = TryOpenDll (pprc, pMod, szName, MAX_PATH, len, hTok, dwFlags, fTryAppend);
        }
    }

    if (!dwErr && !ChkDebug (&pMod->oe)) {
        dwErr = NTE_BAD_SIGNATURE;
    }

    if (dwErr) {
        return (ERROR_FILE_NOT_FOUND == dwErr)? ERROR_MOD_NOT_FOUND : dwErr;
    }

    lpszDllName = GetPlainName (szName);

    // load O32 info for the module
    eptr = &pMod->e32;
    if (dwErr = LoadE32 (&pMod->oe, eptr, &flags, &entry, (wFlags & LOAD_LIBRARY_AS_DATAFILE) ? 1 : 0)) {
        return dwErr;
    }

    if (!(wFlags & LOAD_LIBRARY_AS_DATAFILE) && !(flags & IMAGE_FILE_DLL)) {
        // trying to load an exe via LoadLibrary call; fail the call
        DEBUGMSG(1,(TEXT("ERROR! LoadLibrary on EXE!\r\n")));
        return  ERROR_BAD_EXE_FORMAT;
    }

    // resource only DLLs or managed DLLs always loaded as data file
    if (eptr->e32_sect14rva
        || (!eptr->e32_unit[EXP].rva && !(DONT_RESOLVE_DLL_REFERENCES & wFlags))) {
        wFlags = (pMod->wFlags |= NO_IMPORT_FLAGS);
    }

    // allocate memory for both name and the o32 objects in one piece
    len = (NKwcslen(lpszDllName) + 1)*sizeof(WCHAR);

    // +2 for DWORD alignment
    if (!(pAlloc = AllocName (eptr->e32_objcnt * sizeof(o32_lite) + len + 2))) {
        return ERROR_OUTOFMEMORY;
    }

    DEBUGCHK (IsDwordAligned(pAlloc));

    pMod->o32_ptr = (o32_lite *) ((DWORD) pAlloc + 4);    // skip the header info

    dwErr = ReadO32Info(&pMod->oe, eptr, pMod->o32_ptr, &flags);
    if (dwErr) {
        return dwErr;
    }

    // ROM DLLs already have their VM reserved
    if (FA_PREFIXUP & pMod->oe.filetype) {
        //
        // fixed up already, just use the address
        //

        if (wFlags & LOAD_LIBRARY_AS_DATAFILE) {
            // load as data-file - always reserve a new address
            pMod->BasePtr = VMReserve (g_pprcNK, eptr->e32_vsize, MEM_IMAGE, (wFlags & LOAD_LIBRARY_IN_KERNEL)? 0 : VM_DLL_BASE);

        } else if ((wFlags & LOAD_LIBRARY_IN_KERNEL) && !IsKModeAddr (pMod->e32.e32_vbase)) {

            RETAILMSG (1, (L"!!!ERROR! Trying to load DLL '%s' fixed-up to user address into Kernel.\r\n", lpszFileName));
            RETAILMSG (1, (L"!!!ERROR! MUST SPECIFY 'K' FLAG BIB FILE.\r\n"));
            return ERROR_BAD_EXE_FORMAT;

        } else if (!(wFlags & LOAD_LIBRARY_IN_KERNEL) && IsKModeAddr (pMod->e32.e32_vbase)) {

            RETAILMSG (1, (L"!!!ERROR! Trying to load DLL '%s' fixed-up to kernel address into user app.\r\n", lpszFileName));
            RETAILMSG (1, (L"!!!ERROR! CANNOT SPECIFY 'K' FLAG BIB FILE.\r\n"));
            return ERROR_BAD_EXE_FORMAT;

        } else {

            pMod->BasePtr = (LPVOID) pMod->e32.e32_vbase;
        }



    } else {
        //
        // ordinary DLL, reserve VM
        //

        // try to honor DLL's relocation base to avoid relocation
        if (!(wFlags & LOAD_LIBRARY_IN_KERNEL)
            && ((eptr->e32_vbase - VM_DLL_BASE) < 0x20000000)) {
            pMod->BasePtr = VMAlloc (g_pprcNK, (LPVOID) eptr->e32_vbase, eptr->e32_vsize, MEM_RESERVE|MEM_IMAGE, PAGE_NOACCESS);
        }

        if (!pMod->BasePtr) {
            // can't loaded in preferred load location, find a place to load
            pMod->BasePtr = VMReserve (g_pprcNK, eptr->e32_vsize, MEM_IMAGE,
                    (LOAD_LIBRARY_IN_KERNEL & wFlags)? 0 : VM_DLL_BASE);
        }
    }

    if (!pMod->BasePtr) {
        return ERROR_OUTOFMEMORY;
    }

    // optimization for resoure loading - rebase resource rva
    if (eptr->e32_unit[RES].rva) {
        eptr->e32_unit[RES].rva += (DWORD) pMod->BasePtr;
    }

    //
    // Read in O32 information for this module.
    //
    DEBUGMSG(ZONE_LOADER1,(TEXT("BasePtr is %8.8lx\r\n"),pMod->BasePtr));

    if (dwErr = LoadO32 (&pMod->oe, eptr,pMod->o32_ptr, (ulong)pMod->BasePtr)) {
        DEBUGMSG(ZONE_LOADER1,(TEXT("LoadO32 failed dwErr = 0x%8.8lx\r\n"), dwErr));
        return dwErr;
    }

    // update module name
    pMod->lpszModName = (LPWSTR) (pMod->o32_ptr + eptr->e32_objcnt);
    for (loop = 0; loop < len/sizeof(WCHAR); loop++) {
        pMod->lpszModName[loop] = NKtowlower(lpszDllName[loop]);
    }

    //
    // Non-XIP image needs to be relocated.
    //
    if ((eptr->e32_vbase != (DWORD) pMod->BasePtr)
        && !PageAble (&pMod->oe)
        && !(pMod->wFlags & LOAD_LIBRARY_AS_DATAFILE)
        && !Relocate (eptr, pMod->o32_ptr, (ulong)pMod->BasePtr)) {

        return ERROR_OUTOFMEMORY;
    }

    if (entry) {
        pMod->startip = entry + (DWORD) pMod->BasePtr;
    }

    // optimization - user mode and kernel mode DLL are inserted in opposite direction
    //                such that we can break loop faster while searching for DLLs.
    LockModuleList ();
    if (LOAD_LIBRARY_IN_KERNEL & wFlags) {
        AddToDListTail (&g_pprcNK->modList, &pMod->link);
    } else {
        AddToDListHead (&g_pprcNK->modList, &pMod->link);
    }
    UnlockModuleList ();

#ifdef x86
    if (pMod->e32.e32_unit[EXC].rva & E32_EXC_DEP_COMPAT) {
        pMod->wFlags |= MF_DEP_COMPACT;
    }
#endif

    // AdjustRegions must be called after we add the module to the module list, such
    // that we can find out the location of Import Address Table, and adjust access bit.
    AdjustRegions (pMod->BasePtr, &pMod->oe, eptr, pMod->o32_ptr);

    //
    // Done loading the code into the memory via the data cache.
    // Now we need to write-back the data cache since there may be a separate
    // instruction cache.
    //
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_WRITEBACK|CACHE_SYNC_INSTRUCTIONS);

    // Log a load for an invalid process when the module is first loaded
    CELOG_ModuleLoad (NULL, pMod);

    DEBUGMSG(ZONE_LOADER1,(TEXT("Done InitModule, StartIP = %8.8lx\r\n"), pMod->startip));
    return 0;
}

PMODULELIST InitModInProc (PPROCESS pprc, PMODULE pMod)
{
    PMODULELIST pEntry = AllocMem (HEAP_MODLIST);
    DWORD dwErr = ERROR_OUTOFMEMORY;

    DEBUGCHK (pprc != g_pprcNK);
    DEBUGCHK (OwnLoaderLock (pprc));

    if (pEntry) {
        LPVOID pAddr = NULL;

        pEntry->pMod    = pMod;
        pEntry->wRefCnt = 1;
        pEntry->wFlags  = pMod->wFlags;

        if (pAddr = VMAlloc (pprc, pMod->BasePtr, pMod->e32.e32_vsize, MEM_RESERVE|MEM_IMAGE, PAGE_NOACCESS)) {
            if (!(dwErr = CopyRegions (pMod))) {
                // success
                LockModuleList ();
                AddToDListHead (&pprc->modList, &pEntry->link);
                LockModule (pMod);  // add an extra ref
                UnlockModuleList ();

                // Log a load event for the current process on its first load
                CELOG_ModuleLoad (pprc, pMod);

                if (!(pMod->wFlags & DONT_RESOLVE_DLL_REFERENCES)) {
                    (* HDModLoad) ((DWORD)pMod);

                    // notify the application debugger attached to this process
                    if (pprc->pDbgrThrd) {
                        DbgrNotifyDllLoad (pCurThread, pprc, pMod);
                    }
                }

                return pEntry;
            }
            // error, release VM if allocated
            if (pAddr) {
                VMFreeAndRelease (pprc, pMod->BasePtr, pMod->e32.e32_vsize);
            }
        }
        FreeMem (pEntry, HEAP_MODLIST);
    }

    KSetLastError (pCurThread, dwErr);

    return NULL;
}


//------------------------------------------------------------------------------
// Load a library (DLL)
//------------------------------------------------------------------------------
PMODULE
LoadOneLibrary (
    LPCWSTR     lpszFileName,
    DWORD       fLbFlags,
    DWORD       dwFlags
    )
{
    PMODULE     pMod = NULL;
    LPCWSTR     pPlainName;


    DEBUGMSG (ZONE_LOADER1, (L"LoadOneLibrary: '%s', 0x%x\n", lpszFileName, dwFlags));

    DEBUGCHK (OwnLoaderLock (g_pprcNK));

    if (dwFlags & LOAD_LIBRARY_AS_DATAFILE) {
        dwFlags |= DONT_RESOLVE_DLL_REFERENCES;
    } else if (!(dwFlags & LOAD_LIBRARY_IN_KERNEL)) {
        // user-mode DLL. Unless loaded as datafile, it's always considered normal.
        // DONT_RESOLVE_DLL_REFERENCES is handled in coredll for individual processes.
        dwFlags &= ~DONT_RESOLVE_DLL_REFERENCES;
    }

    DEBUGCHK (!(dwFlags & LOAD_LIBRARY_IN_KERNEL) || (pActvProc == g_pprcNK));

    pPlainName = GetPlainName (lpszFileName);

    LockModuleList ();
    if (pMod = (PMODULE) FindModInProcByName (g_pprcNK, pPlainName, !(dwFlags & LOAD_LIBRARY_IN_KERNEL), dwFlags)) {
        // found, just increment ref-count and done
        LockModule (pMod);
        if ((DONT_RESOLVE_DLL_REFERENCES == (pMod->wFlags & NO_IMPORT_FLAGS))
            && !(dwFlags & NO_IMPORT_FLAGS)) {
            DEBUGCHK (LOAD_LIBRARY_IN_KERNEL & pMod->wFlags);
            // if a module is loaded previously with DONT_RESOLVE_DLL_REFERENCES flag
            // and the new request is to load as a normal dll, upgrade the module to a regular dll
            pMod->wFlags &= ~DONT_RESOLVE_DLL_REFERENCES;   // this will enable resolving imports
            pMod->wFlags |= MF_LOADING;                     // this will enable calling dllmain
        }
    }
    UnlockModuleList ();

    if (!pMod) {

        if (!(pMod = AllocMem (HEAP_MODULE))) {
            KSetLastError (pCurThread, ERROR_OUTOFMEMORY);

        } else {

            PPROCESS    pprc = NULL, pprcVM = NULL;
            DWORD       dwErr;
            //
            //
            // Module instance does not currently exist.
            // switch VM to kernel to initialize the module
            //
            if (!(dwFlags & LOAD_LIBRARY_IN_KERNEL)) {
                pprcVM = SwitchVM (g_pprcNK);
                pprc = SwitchActiveProcess (g_pprcNK);
            }

            if (dwErr = InitModule (pprc, pMod, lpszFileName, pPlainName, fLbFlags, (WORD)dwFlags)) {
                KSetLastError (pCurThread, dwErr);
                DEBUGCHK (!pMod->pmodResource);
                FreeModuleMemory (pMod);
                pMod = NULL;

            } else {
                if (!(pMod->wFlags & DONT_RESOLVE_DLL_REFERENCES)) {
                    (* HDModLoad) ((DWORD)pMod);
                }

                if (!(fLbFlags & LLIB_NO_MUI)) {
                    pMod->pmodResource = LoadMUI (pMod, pMod->BasePtr, &pMod->e32);
                }
            }

            if (!(dwFlags & LOAD_LIBRARY_IN_KERNEL)) {
                SwitchActiveProcess (pprc);
                SwitchVM (pprcVM);
            }
        }
    }
    return pMod;
}

//------------------------------------------------------------------------------
// Win32 LoadLibrary call
//------------------------------------------------------------------------------
PMODULE
NKLoadLibraryEx(
    LPCWSTR pszFileName,
    DWORD   dwFlags,
    PUSERMODULELIST unused
    )
{
    PMODULE    pMod = NULL;
    // for simplicity, we pre-allocate the node. Will be freed if we don't use it.
    PMODULELIST pml = (PMODULELIST) AllocMem (HEAP_MODLIST);

    DEBUGMSG(ZONE_ENTRY,(L"NKLoadLibraryEx entry: %8.8lx %8.8lx\r\n", pszFileName, dwFlags));

    if (pml) {
        PPROCESS pprc    = SwitchActiveProcess (g_pprcNK);
        DWORD    lbFlags = HIWORD(dwFlags);
        dwFlags = LOWORD (dwFlags) | LOAD_LIBRARY_IN_KERNEL;

        DEBUGCHK (!(dwFlags & 0xffff0000));

        LockLoader (g_pprcNK);

        if (pMod = LoadOneLibrary (pszFileName, lbFlags, (WORD)dwFlags)) {
            DWORD dwRefCnt = pMod->wRefCnt;     // save refcnt for it can change during import (mutual dependent DLLs)
            pMod = (*g_pfnDoImports) (pMod);

            if (pMod
                && (1 == dwRefCnt)
                && ((MF_IMPORTED|MF_NO_THREAD_CALLS) & pMod->wFlags) == MF_IMPORTED) {
                AddToThreadNotifyList (pml, pMod);
                pml = NULL;     // so we don't free it
            }
        }

        UnlockLoader (g_pprcNK);
        SwitchActiveProcess (pprc);

        if (pml) {
            FreeMem (pml, HEAP_MODLIST);
        }
    } else {
        NKSetLastError (ERROR_NOT_ENOUGH_MEMORY);
    }

    DEBUGMSG(ZONE_ENTRY,(L"NKLoadLibraryEx exit: %8.8lx (%8.8lx)\r\n", pMod, pMod? ((PMODULE)pMod)->BasePtr: NULL));
    return pMod;
}

//------------------------------------------------------------------------------
// Win32 LoadExistingModule call
//------------------------------------------------------------------------------
PMODULE
EXTInitModule (
    PMODULE pMod,
    PUSERMODULELIST pUMod)
{
    if (pMod = FindModInKernel (pMod)) {

        PPROCESS    pprc = pActvProc;
        PMODULELIST pml;

        LockLoader (pprc);
        pml = InitModInProc (pprc, pMod);

        if (!pml) {
            pMod = NULL;
        } else {

            __try {
                if (pMod->ZonePtr
                    && !(pMod->wFlags & DONT_RESOLVE_DLL_REFERENCES)) {
                    // user mode DLL, update debugzone.
                    // this assignment actually updates dpCurSettings.ulZoneMask
                    pMod->ZonePtr->ulZoneMask = pMod->dwZone;
                }
                pUMod->dwBaseAddr    = (DWORD) pMod->BasePtr;
                pUMod->pfnEntry      = (dllntry_t) pMod->startip;
                pUMod->wFlags        = pMod->wFlags;
                pUMod->pmodResource  = (PUSERMODULELIST) pMod->pmodResource;
                pUMod->udllInfo[EXP] = pMod->e32.e32_unit[EXP];
                pUMod->udllInfo[IMP] = pMod->e32.e32_unit[IMP];
                pUMod->udllInfo[RES] = pMod->e32.e32_unit[RES];

            } __except (EXCEPTION_EXECUTE_HANDLER) {
                FreeModFromProc (pprc, pml);
                SetLastError (ERROR_INVALID_PARAMETER);
                pMod = NULL;
            }
        }
        UnlockLoader (pprc);
    }

    return pMod;
}


PMODULE
EXTLoadLibraryEx (
    LPCWSTR pszFileName,
    DWORD   dwFlags,
    PUSERMODULELIST pUMod
    )
{
    PMODULE pMod = NULL;
    WCHAR   szNameCpy[MAX_PATH];
    DWORD   lbFlags = HIWORD(dwFlags);

    dwFlags &= ~(LLIB_NO_PAGING << 16);

    DEBUGCHK (pActvProc != g_pprcNK);

    // the following line can except, but okay, as we don't hold any kernel CS here.
    if (!CopyPath (szNameCpy, pszFileName, MAX_PATH)) {
        KSetLastError(pCurThread,ERROR_BAD_PATHNAME);

    } else if (dwFlags & ~NO_IMPORT_FLAGS) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);

    } else {
        LockLoader (g_pprcNK);
        pMod = LoadOneLibrary (szNameCpy, lbFlags, dwFlags);
        UnlockLoader (g_pprcNK);

        if (pMod) {
            PMODULE pModRet = EXTInitModule (pMod, pUMod);

            // if pModRet is not NULL, we would have lock the module one more time in InitModInProc. So
            // all we need to do here is to unlock module regardless the return value.
            UnlockModule (pMod, FALSE);
            pMod = pModRet;
        }
    }

    return pMod;
}

//------------------------------------------------------------------------------
// Win32 FreeLibrary call
//------------------------------------------------------------------------------
BOOL
NKFreeLibrary(
    HMODULE hInst
    )
{
    PPROCESS pprc = pActvProc;
    PMODULE  pMod = (PMODULE) hInst;
    PMODULELIST pEntry;

    DEBUGMSG(ZONE_ENTRY,(L"NKFreeLibrary entry: %8.8lx\r\n",hInst));


    LockModuleList ();
    pEntry = FindModInProc (pprc, pMod);
    if (pEntry) {
        LockModule (pMod);
    }
    UnlockModuleList ();


    if (pEntry) {
        WORD wRefCnt;

        LockLoader (pprc);

        DEBUGCHK (pEntry->wRefCnt);

        wRefCnt = (WORD) InterlockedDecrement ((PLONG) &pEntry->wRefCnt);

        if (!wRefCnt) {

            // for kernel modules, if we found it in the module list, it has a ref-count of at least 1,
            // and the call to LockModule would increment the ref-count. So wRefCnt would never be 0.
            DEBUGCHK (pprc != g_pprcNK);
            DEBUGCHK (pMod != (PMODULE) hKCoreDll);

            // coredll cannot be freed before process exit
            if ((pMod == (PMODULE) hCoreDll)) {
                DEBUGMSG (1, (L"FreeOneLibrary: Trying to free coredll before process exit. ignored.\r\n"));
                DEBUGCHK (0);
                InterlockedIncrement ((PLONG) &pEntry->wRefCnt);
                pEntry = NULL;  // clear pEntry to indicate error

            } else {
                // process ref-cnt gets to 0, free the library
                DEBUGMSG (ZONE_LOADER1, (L"FreeOneLibrary: last reference to '%s' in process\r\n", pMod->lpszModName));

                FreeModFromProc (pprc, pEntry);
            }
        }
        UnlockModule (pMod, pprc == g_pprcNK);    // module will be completely freed if ref-count get to 0.

        UnlockLoader (pprc);
    }

    return pEntry != NULL;
}

PMODULE LDRGetModByName (PPROCESS pprc, LPCWSTR pszName, DWORD dwFlags)
{
    PMODULELIST pEntry = NULL;

    if (pszName) {

        LockModuleList ();

        __try {

            pEntry = FindModInProcByName (pprc, GetPlainName (pszName), pprc != g_pprcNK, dwFlags);

        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }

        UnlockModuleList ();
    }

    if (!pEntry) {
        KSetLastError (pCurThread, ERROR_FILE_NOT_FOUND);
    }

    return pEntry? pEntry->pMod : NULL;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD PageFromLoader (PPROCESS pprc, DWORD dwAddr, BOOL fWrite)
{
    BOOL        retval = PAGEIN_FAILURE;
    PMODULE     pMod = NULL;

    DEBUGMSG (ZONE_PAGING, (L"PageFromLoader: proc id = %8.8lx, addr = %8.8lx, fWrite = %8.8lx\r\n", pprc->dwId, dwAddr, fWrite));

    if (dwAddr >= VM_DLL_BASE) {
        // in DLL
        LockModuleList ();
        if (pMod = FindModInProcByAddr (pprc, dwAddr)) {
            LockModule (pMod);
        }
        UnlockModuleList ();
    }

    EnterCriticalSection(&PagerCS);
    if (pMod)
        retval = PageInModule (pprc, pMod, dwAddr, fWrite);
    else
        retval = PageInProcess (pprc, dwAddr, fWrite);
    DEBUGMSG(ZONE_PAGING,(L"PageFromLoader: returns %d\r\n", retval));
    LeaveCriticalSection(&PagerCS);
    if (pMod) {
        // if this debugchk failed, the kernel DLL is unloaded while we're paging in page for the DLL, which is usually due to
        // user error. DEBUGCHK here to help catching the situation.
        DEBUGCHK (pMod->wRefCnt > 1);

        // we cannot call DllMain here if we endup freeing the module - CS order violation. We would have hit the debugchk above
        // if we ever need to call DllMain in this case, where we'll need to fix the code not to free library while it's still in use.
        UnlockModule (pMod, FALSE);
    }
    return retval;
}



//-----------------------------------------------------------------------------
// LoaderInit - loader initialization
//
BOOL LoaderInit ()
{
    ROMChain_t *pROM;
    DWORD       ROMDllEnd;
    PNAME       pTmp;
    PMODULE     pMod;
    DWORD dummy = 0;

    DEBUGMSG (1, (L"LoaderInit: Initialing loader\r\n"));

    // initialize "thread call list"
    InitDList (&g_thrdCallList);

    ROMDllEnd = VM_DLL_BASE;    // the first address of ROM DLL code

    // find out the end address of ROM DLLs
    for (pROM = ROMChain; pROM; pROM = pROM->pNext) {

        if (ROMDllEnd < pROM->pTOC->dlllast) {
            ROMDllEnd = pROM->pTOC->dlllast;
        }
    }
    ROMDllEnd &= ~VM_BLOCK_OFST_MASK;

    DEBUGMSG (ZONE_LOADER1, (L"LoaderInit: ROMDllEnd = %8.8lx, pActvProc = %8.8lx\r\n", ROMDllEnd, pActvProc));

    // reserve VM for ROM DLLs
    VERIFY (VMAlloc (g_pprcNK, (LPVOID) VM_DLL_BASE, ROMDllEnd - VM_DLL_BASE, MEM_RESERVE|MEM_IMAGE, PAGE_NOACCESS));

    // fill up the o32_ptr for kernel process
    pTmp = AllocName (g_pprcNK->e32.e32_objcnt*sizeof(o32_lite) + 2);
    g_pprcNK->o32_ptr = (o32_lite*) ((DWORD)pTmp + 4);
    ReadO32Info(&g_pprcNK->oe, &g_pprcNK->e32, g_pprcNK->o32_ptr, &dummy);

    LockLoader (g_pprcNK);
    VERIFY (hKernDll = (HMODULE) LoadOneLibrary (TEXT(KERNELDLL), LLIB_NO_PAGING, LOAD_LIBRARY_IN_KERNEL|DONT_RESOLVE_DLL_REFERENCES));
    if ((PFN_Ioctl) ReturnFalse != g_pNKGlobal->pfnKITLIoctl) {
        if (hKitlDll = (HMODULE) LoadOneLibrary (TEXT(KITLDLL), LLIB_NO_PAGING, LOAD_LIBRARY_IN_KERNEL|DONT_RESOLVE_DLL_REFERENCES)) {
            ((PMODULE) hKitlDll)->ZonePtr = g_pNKGlobal->pKITLDbgZone;
        }
    }
    VERIFY (hKCoreDll = (HMODULE) LoadOneLibrary (L"coredll.dll", LLIB_NO_PAGING, LOAD_LIBRARY_IN_KERNEL));
    ((PMODULE) hKCoreDll)->wFlags = LOAD_LIBRARY_IN_KERNEL | MF_IMPORTED | MF_NO_THREAD_CALLS | MF_COREDLL;
    VERIFY (((PFN_DllMain) ((PMODULE)hKCoreDll)->startip) (hKCoreDll, DLL_PROCESS_ATTACH, (DWORD) NKKernelLibIoControl));

    hCoreDll = (HMODULE) LoadOneLibrary (L"coredll.dll", 0, 0);
    if (hCoreDll)
        ((PMODULE) hCoreDll)->wFlags = MF_IMPORTED | MF_NO_THREAD_CALLS;
    UnlockLoader (g_pprcNK);

#ifdef DEBUG
    dpCurSettings.ulZoneMask = initialKernelLogZones;
    ((PMODULE) hKernDll)->ZonePtr = &dpCurSettings;
#endif

    VERIFY (KTBFf           = (void (*)(LPVOID, ulong))GetProcAddressA(hKCoreDll,(LPCSTR)13)); // ThreadBaseFunc for kernel threads
    pSignalStarted  = (void (*)(DWORD))GetProcAddressA(hKCoreDll,(LPCSTR)639);          // SignalStarted (might not present)
    VERIFY (g_pfnKrnRtlDispExcp = GetProcAddressA (hKCoreDll, (LPCSTR) 2548));          // RtlExceptionDispatch
#ifndef x86
    VERIFY (g_pfnRtlUnwindOneFrame = GetProcAddressA (hKCoreDll, (LPCSTR) 2549));
#endif
    VERIFY (pKrnExcpExitThread = GetProcAddressA (hKCoreDll, (LPCSTR) 1474));       // ThreadExceptionExit
    VERIFY (g_pfnGetProcAddrW   = (PFN_GetProcAddrW) GetProcAddressA (hKCoreDll, (LPCSTR) 530));     // GetProcAddressW

    VERIFY (InitGenericHeap (hKCoreDll));

    // need the "FindResrouceW" pointer for loading MUI
    g_pfnFindResource   = (PFN_FindResource) GetProcAddressA (hKCoreDll, (LPCSTR) 532);

    VERIFY (g_pNKGlobal->pfnCompareFileTime = (LONG (*)(const FILETIME*, const FILETIME*))GetProcAddressA(hKCoreDll,(LPCSTR)18)); // CompareFileTime
    pfnKrnGetHeapSnapshot = (DWORD (*)(THSNAP *)) GetProcAddressA(hKCoreDll,(LPCSTR)52); // GetHeapSnapshot (kernel mode)
    pKCaptureDumpFileOnDevice = (PVOID)GetProcAddressA(hKCoreDll,(LPCSTR)1960); //  CaptureDumpFileOnDevice (kernel mode)
    pKrnMainHeap = (LPDWORD) GetProcAddressA(hKCoreDll,(LPCSTR)2543);

    pfnCeGetCanonicalPathNameW = (DWORD (*)(LPCWSTR, LPWSTR, DWORD, DWORD))GetProcAddressA(hKCoreDll,(LPCSTR)1957); //  CeGetCanonicalPathNameW

#if x86
    if (!(g_pKCoredllFPEmul = GetProcAddressA(hKCoreDll,(LPCSTR)81))) {
        DEBUGMSG(1,(L"Did not find emulation code for x86... using floating point hardware.\r\n"));
        dwFPType = 1;
        KCall((PKFN)InitializeFPU);
    } else {
        DEBUGMSG(1,(L"Found emulation code for x86... using software for emulation of floating point.\r\n"));
        dwFPType = 2;
        KCall((PKFN)InitializeEmx87);
        KCall((PKFN)InitNPXHPHandler, g_pKCoredllFPEmul);
        if (hCoreDll)
            g_pCoredllFPEmul = GetProcAddressA(hCoreDll,(LPCSTR)81);            
    }
#endif

    if (hCoreDll) {
        VERIFY (pExitThread     = GetProcAddressA (hCoreDll, (LPCSTR)6));           // ExitThread
        VERIFY (pUsrExcpExitThread = GetProcAddressA (hCoreDll, (LPCSTR) 1474));       // ThreadExceptionExit
        VERIFY (MTBFf           = (void (*)(LPVOID, ulong, ulong, ulong, ulong)) GetProcAddressA (hCoreDll, (LPCSTR)14));   // MainThreadBaseFunc
        VERIFY (TBFf            = (void (*)(LPVOID, ulong))GetProcAddressA (hCoreDll, (LPCSTR)13)); // ThreadBaseFunc
        VERIFY (g_pfnUsrRtlDispExcp = GetProcAddressA (hCoreDll, (LPCSTR) 2548));
        pfnUsrGetHeapSnapshot = (DWORD (*)(THSNAP *)) GetProcAddressA(hCoreDll,(LPCSTR)52);  // GetHeapSnapshot (user mode)
        pUsrMainHeap = (LPDWORD) GetProcAddressA(hCoreDll,(LPCSTR)2543);
        pCaptureDumpFileOnDevice =(PVOID)GetProcAddressA(hCoreDll,(LPCSTR)1960); //  CaptureDumpFileOnDevice (user mode)

        // initialize remote user heap
        VERIFY (RemoteUserHeapInit (hCoreDll));

        // load OAL ioctl dll; this is valid only if image has coredll
        LockLoader (g_pprcNK);
        if (pMod = LoadOneLibrary (OALIOCTL_DLL, 0, LOAD_LIBRARY_IN_KERNEL)) {
            pMod = (*g_pfnDoImports) (pMod);
            VERIFY (g_pfnOalIoctl = (PFN_Ioctl) GetProcAddressA ((HMODULE)pMod, OALIOCTL_DLL_IOCONTROL));
            VERIFY (((PFN_DllMain) pMod->startip) ((HMODULE)pMod, DLL_PROCESS_ATTACH, (DWORD) &(OEMIoControl)));            
        }
        UnlockLoader (g_pprcNK);        
    }

    return NULL != hKCoreDll;
}

PMODULELIST
AppVerifierFindModule (
    LPCWSTR pszName
    )
{
    PMODULELIST pMod;

    LockLoader (g_pprcNK);
    LockModuleList ();
    pMod = FindModInProcByName (g_pprcNK, pszName, FALSE, 0);
    UnlockModuleList ();
    UnlockLoader (g_pprcNK);

    return pMod;
}

