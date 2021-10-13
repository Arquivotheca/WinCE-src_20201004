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
#include <oalioctl.h>

#define SYSTEMDIR L"\\Windows\\"
#define SYSTEMDIRLEN 9

#define DBGDIR    L"\\Release\\"
#define DBGDIRLEN    9

ERRFALSE(offsetof(MODULELIST, pMod) == offsetof(MODULE, lpSelf));
ERRFALSE(offsetof(MODULELIST, wRefCnt) == offsetof(MODULE, wRefCnt));
ERRFALSE(offsetof(MODULELIST, wFlags) == offsetof(MODULE, wFlags));
ERRFALSE(IsDwordAligned (offsetof(MODULELIST, wRefCnt)));
ERRFALSE(VM_PAGE_SIZE == sizeof(fspagelist_t));

fslog_t *LogPtr;
BOOL fForceCleanBoot;
BOOL g_fNoKDebugger; // Prevents Kernel Debugger to be loaded
BOOL g_fKDebuggerLoaded; // Indicates that the Kernel Debugger is already loaded
BOOL g_fAppVerifierEnabled = (DWORD) -1; // Indicates whether app verifier is enabled or not in the system

REGIONINFO FreeInfo[MAX_MEMORY_SECTIONS];
MEMORYINFO MemoryInfo;

ROMChain_t FirstROM;
ROMChain_t *ROMChain;

PROMINFO g_pROMInfo;

// debugger externs
extern WCHAR* g_wszKdDll;
extern BOOL DoAttachDebugger (LPCWSTR *pszDbgrNames, int nNames);

// Page-out functionality
extern CRITICAL_SECTION PageOutCS;
#ifdef DEBUG
extern CRITICAL_SECTION MapCS;  // Used to sanity-check CS ordering on debug builds
#endif

extern PNAME pDebugger;

DLIST   g_thrdCallList;

extern CRITICAL_SECTION ModListcs;

#ifdef x86
FARPROC g_pCoredllFPEmul;
FARPROC g_pKCoredllFPEmul;
#endif

// OEMs can update this via fixupvar in config.bib file under platform
// kernel.dll:initialKernelLogZones 00000000 00000100 FIXUPVAR
DWORD initialKernelLogZones = 0x2100; // ZONE_ERROR | ZONE_DEBUG

// OAL ioctl module entry point
PFN_Ioctl g_pfnOalIoctl;

// coredll LoadLibraryW() entry point
FARPROC g_pfnUsrLoadLibraryW;

// default to be by app, such that we'll set NX bit for kernel, k.coredll
// and everything loaded before filesys finished initialzation
DWORD g_dwNXFlags = NX_SUPPORT_BY_APP;

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
PMODULE g_pModKernDll, g_pModKitlDll;
FARPROC TBFf;
FARPROC KTBFf;
FARPROC MTBFf;
DWORD (WINAPI *pfnUsrGetHeapSnapshot) (THSNAP *pSnap);
DWORD (WINAPI *pfnKrnGetHeapSnapshot) (THSNAP *pSnap);
DWORD (WINAPI *pfnCeGetCanonicalPathNameW)(LPCWSTR, LPWSTR, DWORD, DWORD);
FARPROC g_pfnCompactAllHeaps;

LPDWORD pKrnMainHeap;
LPDWORD pUsrMainHeap;

PFN_GetProcAddrA g_pfnGetProcAddrA;
PFN_GetProcAddrW g_pfnGetProcAddrW;
PFN_DoImport     g_pfnDoImports;
PFN_UndoDepends  g_pfnUndoDepends;
PFN_AppVerifierIoControl g_pfnAppVerifierIoControl;
PFN_FindResource g_pfnFindResource;
PFN_SetSystemPowerState g_pfnSetSystemPowerState;

PHDATA phdPslNotifyEvent;
extern PNAME pSearchPath;

LPWSTR pDbgList;
#ifndef x86
FARPROC g_pfnRtlUnwindOneFrame;
#endif

#ifdef SH3
extern unsigned int SH3DSP;
#endif

#ifdef ARM

#define ARM_CLEAR_MOVS_MASK             0xFFF0F000
#define THUMB2_CLEAR_MOVS_MASK          0x8F00FBF0

DWORD ArmT2EncodeImmediate16(WORD val)
{
    DWORD result = 0;

    result |= (val & 0x00FF) << 16;
    result |= (val & 0x0700) << 20;
    result |= (val & 0x0800) >> 1;
    result |= (val & 0xF000) >> 12;

    return result;
}

DWORD ArmEncodeImmediate16(WORD val)
{
    DWORD result = 0;

    result |= (val & 0x0FFF);
    result |= (val & 0xF000) << 4;

    return result;
}

DWORD ArmDecodeImmediate16(DWORD dw)
{
    DWORD ret = 0;

    ret |= (dw & 0x00000FFF);               // bits[11:0]  in [11:0]
    ret |= (dw & 0x000F0000) >> 4;          // bits[15:12] in [19:16]

    return ret;
}

DWORD ArmT2DecodeImmediate16(DWORD dw)
{
    DWORD ret = 0;

    ret |= (dw & 0x00FF0000) >> 16;         // bits[7:0]   in HW2[7:0]
    ret |= (dw & 0x70000000) >> 20;         // bits[10:8]  in HW2[14:12]
    ret |= (dw & 0x00000400) << 1;          // bits[11]    in HW1[10]
    ret |= (dw & 0x0000000F) << 12;         // bits[15:12] in HW1[3:0]

    return ret;
}

DWORD ArmXEncodeImmediate16(WORD type, WORD val)
{
    if (type == IMAGE_REL_BASED_ARM_MOV32)
        return ArmEncodeImmediate16(val);
    else
        return ArmT2EncodeImmediate16(val);
}

DWORD ArmXDecodeImmediate16(WORD type, DWORD val)
{
    if (type == IMAGE_REL_BASED_ARM_MOV32)
        return ArmDecodeImmediate16(val);
    else
        return ArmT2DecodeImmediate16(val);
}
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
        WCHAR wch = pSrc[cchRet];
        if (wch == L'/')
            wch = L'\\';
        pDst[cchRet] = wch;
    }
    pDst[cchRet] = 0;   // EOS
    return pSrc[cchRet]? 0 : cchRet;    // return 0 if source is longer than we can hold.
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
const o32_lite *FindOptr (const o32_lite *optr, int nCnt, DWORD dwAddr)
{
    for ( ; nCnt; nCnt --, optr ++) {
        if ((DWORD) (dwAddr - optr->o32_realaddr) < optr->o32_vsize) {
            return optr;
        }
    }

    return NULL;
}

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
            && ((dwFlags & MF_SKIPPOLICY) == (DWORD) (pMod->wFlags & MF_SKIPPOLICY))    // policy skip must match
            && (  !(pMod->wFlags & LOAD_LIBRARY_AS_DATAFILE)        // current module is not loaded as data file, or
                || (dwFlags & LOAD_LIBRARY_AS_DATAFILE)             // request to load the dll as data file, or
                || !pMod->e32.e32_unit[EXP].rva)                    // current module is a resource only DLL
            && !strcmpdllnameW (pszDllName, pszName));
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
                // maintain MRU - only exception is kernel.dll which 
                // is always maintained at the boundary of user and 
                // kernel dlls
                if (ptrav != (PDLIST)g_pModKernDll) {
                    RemoveDList (ptrav);
                    AddToDListTail (&pprc->modList, ptrav);
                }
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
        (* HDModUnload) ((DWORD) pMod, FALSE);
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
    RETAILMSG (1, (L"Warning! Kernel DLL '%s' is registered for thread creation/deletion notification.  To improve performance if you don’t need thread notifications call DisableThreadLibraryCalls from inside DLLMain on DLL_PROCESS_ATTACH.\r\n", pMod->lpszModName));

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
    // don't try to grab CS if the list is empty
    if (!IsDListEmpty (&g_thrdCallList)) {
        LockLoader (g_pprcNK);
        EnumerateDList (&g_thrdCallList, EnumCallDllMain, (LPVOID) dwReason);
        UnlockLoader (g_pprcNK);
    }
}

static PMODULELIST InitModInProc (PPROCESS pprc, PMODULE pMod);


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
            if (pprc->pDbgrThrd || g_fKDebuggerLoaded) {
                fRet = FALSE;
            } else {
                // set both the process flag and the global flag to prevent loading KD
                // or attaching debugger to this process
                pprc->fFlags |= PROC_NOT_DEBUGGABLE;
                g_fNoKDebugger = TRUE;
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
        LPCWSTR pTrav = pDbgList;
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

    LPWSTR  pNewList = NULL;

    if (!KernelIoctl (IOCTL_EDBG_IS_STARTED, NULL, 0, NULL, 0, NULL)) {
        // completely disallow setting a debuglist when KITL is not enabled
        return FALSE;
    }

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
        pNewList = NKmalloc (nTotalLen * sizeof (WCHAR));
        if (!pNewList) {
            // out of memory or list too big
            return FALSE;
        }

        // copy the list
        memcpy (pNewList, pList, nTotalLen * sizeof(WCHAR));
    }

    // update the list
    LockLoader (g_pprcNK);

    if (pDbgList) {
        NKfree (pDbgList);
    }
    pDbgList = pNewList;

    UnlockLoader (g_pprcNK);

    // load kernel debugger if it is in rld modules
    if (!g_fKDebuggerLoaded 
        && IsInDbgList(g_wszKdDll)) {
        
        PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
        DoAttachDebugger (&g_wszKdDll, 1);
        SwitchActiveProcess (pprc);
    }

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
    DWORD       dwPolicyFlags
    )
{
    DWORD dwErr = 0;

    // try open the file
    oeptr->hf = FSOpenModule (lpszName, dwPolicyFlags);

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
               || (NULL == (oeptr->lpName = DupStrToPNAME (wszName)))) {

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
            FreeExeName(oeptr);
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

//------------------------------------------------------------------------------
//  OpenFileFromROM: call into filesys to verify the module and then open
//  it from list of ROM modules.
//
//  return: 0                       -- success
//          ERROR_FILE_NOT_FOUND    -- file is not in the list of ROM modules.
//------------------------------------------------------------------------------

static DWORD OpenFileFromROM(
    LPCWSTR     pPlainName,
    LPCWSTR     lpszName,
    openexe_t   *oeptr,
    DWORD       dwPolicyFlags
    )
{
    ROMChain_t *pROM;
    o32_rom     *o32rp;
    TOCentry    *tocptr;
    int         loop;
    HANDLE      hFile = NULL;
    BOOL        fContinue = TRUE;
    DWORD       dwErr = ERROR_FILE_NOT_FOUND;

    DEBUGMSG (ZONE_LOADER2, (L"OpenFileFromROM: trying '%s'\r\n", pPlainName));
    DEBUGCHK (OPENMODULE_OPEN_ROMMODULE & dwPolicyFlags);

    for (pROM = ROMChain; pROM && fContinue; pROM = pROM->pNext) {

        // check ROM for any copy
        tocptr = (TOCentry *)(pROM->pTOC+1);  // toc entries follow the header

        for (loop = 0; loop < (int)pROM->pTOC->nummods; loop++) {

            if (!NKstrcmpiAandW(tocptr->lpszFileName, pPlainName)) {

                // Verify that the module can be opened.
                if (!SystemAPISets[SH_FILESYS_APIS] // filesys not ready
                    || (INVALID_HANDLE_VALUE != (hFile = FSOpenModule (lpszName, dwPolicyFlags)))) {

                    DEBUGMSG(ZONE_OPENEXE,(TEXT("OpenExe %s: ROM: %8.8lx\r\n"),pPlainName,tocptr));
                    o32rp = (o32_rom *)(tocptr->ulO32Offset);
                    DEBUGMSG(ZONE_LOADER1,(TEXT("(10) o32rp->o32_realaddr = %8.8lx\r\n"), o32rp->o32_realaddr));
                    oeptr->tocptr = tocptr;
                    oeptr->filetype = FT_ROMIMAGE;
                    dwErr = 0;

                } else {
                    dwErr = GetLastError ();
                    DEBUGCHK (dwErr);
                }

                fContinue = FALSE; // break out of the outer loop
                break;
            }

            tocptr++;
        }
    }

    if (hFile) {
        HNDLCloseHandle (g_pprcNK, hFile);
    }

    return dwErr;
}

static DWORD TryDir (openexe_t *oeptr, LPCWSTR pDirName, DWORD nDirLen, LPCWSTR pFileName, DWORD nFileLen, DWORD dwPolicyFlags)
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

    if (OPENMODULE_OPEN_ROMMODULE & dwPolicyFlags)
        return OpenFileFromROM (pFileName, tmpname, oeptr, dwPolicyFlags);
    else
        return OpenFileFromFilesys (tmpname, oeptr, dwPolicyFlags);
}

static LPCWSTR GetPlainName (LPCWSTR pFullPath)
{
    LPCWSTR pPlainName = pFullPath + NKwcslen (pFullPath);
    while ((pPlainName != pFullPath) && (*(pPlainName-1) != (WCHAR)'\\') && (*(pPlainName-1) != (WCHAR)'/')) {
        pPlainName --;
    }
    return pPlainName;
}

static DWORD TrySameDir (openexe_t *oeptr, LPCWSTR lpszFileName, PPROCESS pProc, DWORD dwPolicyFlags)
{
    LPCWSTR     pProcPath = pProc->oe.lpName->name;
    return TryDir (oeptr, pProcPath, GetPlainName (pProcPath) - pProcPath, lpszFileName, NKwcslen (lpszFileName), dwPolicyFlags);
}

//
// open an executable
//      dwFlags consists of the following bits
//          - wFlags fields of DLL if we're opening a DLL
//          - LDROE_ALLOW_PAGING, if paging is allowed
//
DWORD OpenExecutable (PPROCESS pprc, LPCWSTR lpszName, openexe_t *oeptr, DWORD dwFlags)
{
    BOOL bIsAbs = ((*lpszName == '\\') || (*lpszName == '/'));
    DWORD dwErr = ERROR_FILE_NOT_FOUND;
    DWORD dwLastErr = KGetLastError (pCurThread);
    int nTotalLen = NKwcslen (lpszName);
    LPCWSTR pPlainName;
    BOOL bIsInWindDir, bIsInDbgDir;
    DWORD dwPolicyFlags;

    DEBUGMSG (ZONE_OPENEXE,(TEXT("OpenExecutable: %s\r\n"),lpszName));

    if (MAX_PATH <= nTotalLen) {
        // path too long
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        return ERROR_INVALID_PARAMETER;
    }

    dwPolicyFlags = ((LDROE_OPENDLL & dwFlags)? OPENMODULE_OPEN_LOADLIBRARY : OPENMODULE_OPEN_EXECUTE)
                  | ((LDROE_OPENDEBUG & dwFlags)? OPENMODULE_OPEN_DEBUG : 0)
                  | ((LOAD_LIBRARY_AS_DATAFILE & dwFlags)? OPENMODULE_OPEN_DATAFILE : 0)
                  | ((LOAD_LIBRARY_IN_KERNEL & dwFlags)? OPENMODULE_OPEN_LOADKERNEL : 0)
                  | ((LDROE_OPENINACCT & dwFlags)? OPENMODULE_OPEN_INACCOUNT : 0)
                  | ((MF_SKIPPOLICY & dwFlags)? OPENMODULE_SKIPPOLICY : 0);


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

    // see if we're looking for file in Release directory
    bIsInDbgDir = ((pPlainName == lpszName + DBGDIRLEN)     && !NKwcsnicmp(lpszName,DBGDIR,DBGDIRLEN))
                || ((pPlainName == lpszName + DBGDIRLEN - 1) && !NKwcsnicmp(lpszName,DBGDIR+1,DBGDIRLEN-1));

    // load from ROM if filesys is not ready
    if (!SystemAPISets[SH_FILESYS_APIS]) {
        DEBUGMSG (ZONE_OPENEXE,(TEXT("OpenExecutable: Filesys not loaded, Opening from ROM\r\n"), pPlainName));
        dwErr = OpenFileFromROM (pPlainName, pPlainName, oeptr, OPENMODULE_OPEN_ROMMODULE);

    } else if (bIsAbs) {

        DEBUGMSG (ZONE_OPENEXE,(TEXT("OpenExecutable: absolute path\r\n"), lpszName));
        // absolute path

        // try debug list if it's in \Windows or \Release (\Release case accounts for mui dlls when base dll is loaded from \Release)
        if ((bIsInWindDir || bIsInDbgDir) && IsInDbgList (pPlainName)) {
            dwErr = TryDir (oeptr, DBGDIR, DBGDIRLEN, pPlainName, NKwcslen (pPlainName), OPENMODULE_ALLOW_SHADOW|dwPolicyFlags);
        }

        // try filesystem
        if (ERROR_FILE_NOT_FOUND == dwErr) {
            dwErr = OpenFileFromFilesys ((LPWSTR)lpszName, oeptr, dwPolicyFlags);
        }

        // if not found in filesystem and is in windows directory, try ROM
        if ((ERROR_FILE_NOT_FOUND == dwErr) && bIsInWindDir) {
            dwErr = OpenFileFromROM (pPlainName, lpszName, oeptr, OPENMODULE_OPEN_ROMMODULE | dwPolicyFlags);
        }
    } else {

        // relative path
        DEBUGMSG (ZONE_OPENEXE,(TEXT("OpenExecutable: relative path\r\n"), lpszName));

        // try debug list first
        if (IsInDbgList (pPlainName)) {
            dwErr = TryDir (oeptr, DBGDIR, DBGDIRLEN, pPlainName, NKwcslen (pPlainName), OPENMODULE_ALLOW_SHADOW|dwPolicyFlags);
        }

        // try same directory as EXE for DLL
        if (pprc && (ERROR_FILE_NOT_FOUND == dwErr)) {
            if (!(FA_PREFIXUP & pprc->oe.filetype)) {
                dwErr = TrySameDir (oeptr, lpszName, pprc, dwPolicyFlags);
            }
        }

        if ((ERROR_FILE_NOT_FOUND == dwErr)
            && ((dwErr = TryDir (oeptr, SYSTEMDIR, SYSTEMDIRLEN, lpszName, nTotalLen, dwPolicyFlags)) == ERROR_FILE_NOT_FOUND)     // try \windows\lpszName
            && ((!bIsInWindDir && (lpszName != pPlainName))                                                                // try ROM
                || ((dwErr = TryDir (oeptr, SYSTEMDIR, SYSTEMDIRLEN, pPlainName, nTotalLen, OPENMODULE_OPEN_ROMMODULE | dwPolicyFlags)) == ERROR_FILE_NOT_FOUND)) // try ROM module in \windows\pPlainName
            && ((dwErr = TryDir (oeptr, L"\\", 1, lpszName, nTotalLen, dwPolicyFlags)) == ERROR_FILE_NOT_FOUND)) {                 // try \lpszName

            if (pSearchPath) {
                // check the alternative search path
                LPCWSTR pTrav = pSearchPath->name;
                int nLen;

                while (*pTrav && (ERROR_FILE_NOT_FOUND == dwErr)) {
                    nLen = NKwcslen(pTrav);
                    dwErr = TryDir (oeptr, pTrav, nLen, lpszName, nTotalLen,  dwPolicyFlags);
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
FreeExeName(
    openexe_t *oeptr
    )
{
    PNAME pname = InterlockedExchangePointer (&oeptr->lpName, NULL);
    if (pname) {
        FreeName (pname);
    }
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
            oeptr->filetype = 0;
        }
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
    if ((fspages % (MAX_PAGES_IN_PAGELIST+1)) == 1) {
        // in this case we'll end up with an empty list page. just get rid of it.
        fspages --;
    }
    return fspages;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
CarveFSMem (
    fslog_t *pLogPtr,
    DWORD   fspages
    )
{
    // the "list pages" needs to be statically mapped, while the rest doesn't have to, for we
    // need to traverse the pagelist with statically mapped address
    DWORD clistpage = LISTPAGE_NEEDED (fspages);

    fspages  -= clistpage;

    if (!fspages) {
        // no object store pages...
        pLogPtr->pFSList = NULL;

    } else {
        fspagelist_t *pfsListPage = (fspagelist_t *) (pLogPtr->fsmemblk[0].BaseAddr + pLogPtr->fsmemblk[0].ExtnAndAttrib);
        mem_t        *pfsmemblk   = &pLogPtr->fsmemblk[pLogPtr->nMemblks];    // 1 past the end. will be decremented in loop before use.
        DWORD        cPagesInRegion, pa256 = 0;

        // the 1st memory block must be statically mappped, with no RAMAttributes
        DEBUGCHK (!(pLogPtr->fsmemblk[0].BaseAddr      & VM_PAGE_OFST_MASK));
        DEBUGCHK (!(pLogPtr->fsmemblk[0].ExtnAndAttrib & VM_PAGE_OFST_MASK));

        // if you hit this debug check, it means that we do not have enough statically mapped memory.
        // Increase the size for RAM in OEMAddressTable to increase statically mapped RAM.
        DEBUGCHK ((clistpage << VM_PAGE_SHIFT) < (pLogPtr->fsmemblk[0].Length - pLogPtr->fsmemblk[0].ExtnAndAttrib));

        // initialize pLogPtr->pFSList
        pLogPtr->pFSList = pfsListPage;

        for (cPagesInRegion = 0; fspages; pfsListPage ++) {

            // initialize the "list pages", and link them together
            pfsListPage->cPages = 0;
            pfsListPage->pNext  = pfsListPage + 1;

            // fill physical pages of the "list page"
            do {

                if (!cPagesInRegion) {
                    // we used up the pages in one region, move on to the previous one.

                    // skip RAM that can be turn off.
                    do {
                        pfsmemblk --;
                    } while (pfsmemblk->ExtnAndAttrib & CE_RAM_CAN_TURN_OFF);

                    DEBUGCHK (pfsmemblk >= pLogPtr->fsmemblk);

                    cPagesInRegion = PAGECOUNT (pfsmemblk->Length - PAGEALIGN_DOWN (pfsmemblk->ExtnAndAttrib));

                    if (pfsmemblk->ExtnAndAttrib & DYNAMIC_MAPPED_RAM) {
                        // dynamic mapped RAM, base is already Physical >> 8
                        pa256 = pfsmemblk->BaseAddr + (PAGEALIGN_DOWN (pfsmemblk->ExtnAndAttrib) >> 8);
                    } else {
                        // static mapped RAM
                        pa256 = PA256FromPfn (GetPFN ((LPVOID) (pfsmemblk->BaseAddr + PAGEALIGN_DOWN (pfsmemblk->ExtnAndAttrib))));
                        if (pLogPtr->fsmemblk == pfsmemblk) {
                            // take the list pages into account
                            cPagesInRegion -= clistpage;
                            pa256 += clistpage << (VM_PAGE_SHIFT - 8);
                        }
                    }
                }
                pfsListPage->pa256pages[pfsListPage->cPages] = pa256;
                pfsListPage->cPages ++;

                if (! -- fspages) {
                    // NULL terminate the page list
                    pfsListPage->pNext = NULL;
                    break;
                }
                pa256 += (VM_PAGE_SIZE >> 8);
                cPagesInRegion --;
            } while (pfsListPage->cPages < MAX_PAGES_IN_PAGELIST);
        }
    }
}



void RemovePage (DWORD dwPfn);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
GrabFSPages (fspagelist_t *pfspage)
{
    DWORD loop;

    while (pfspage) {
        for (loop = 0; loop < pfspage->cPages; loop ++) {
            RemovePage (PFNfrom256 (pfspage->pa256pages[loop]));
        }
        RemovePage (GetPFN (pfspage));
        pfspage = pfspage->pNext;
    }
}

#define MAX_REGION_SIZE         (0x10000 << VM_PAGE_SHIFT)  // 64K pages max

static DWORD CalcUseMapSize (DWORD RamSize)
{
    DWORD cbExtention = PAGECOUNT(RamSize) * sizeof (PHYSPAGELIST);

    // we need some extra entries to keep track of freelist. If the number of pages used by extention is
    // less than the extra entries, we need to take them into account when calculating extentions.

    if (PAGECOUNT (cbExtention) < IDX_PAGEBASE) {
        cbExtention += IDX_PAGEBASE * sizeof (PHYSPAGELIST);
    }

    return PAGEALIGN_UP (cbExtention);
}

static mem_t *AddRegion (mem_t *pfsmemblk, DWORD BaseAddr, DWORD dwLength, DWORD RamAttributes)
{
    DEBUGCHK (!(RamAttributes & ~VM_PAGE_OFST_MASK));       // can only be bottom 12 bits
    DEBUGCHK (!(dwLength & VM_PAGE_OFST_MASK));             // length must be page aligned

    // ignore CPU local RAM for now.
    if (!(CE_RAM_CPU_LOCAL & RamAttributes)) {
        DWORD dwLengthInRegion;

        do {
            // calculate pages in this region
            dwLengthInRegion = (dwLength < MAX_REGION_SIZE)? dwLength : MAX_REGION_SIZE;

            // setup fsmemblock
            pfsmemblk->BaseAddr      = BaseAddr;
            pfsmemblk->Length        = dwLengthInRegion;
            pfsmemblk->ExtnAndAttrib = CalcUseMapSize (dwLengthInRegion) | RamAttributes;

            // move on to next if > MAX_REGION_SIZE
            dwLength  -= dwLengthInRegion;
            if (DYNAMIC_MAPPED_RAM & RamAttributes) {
                BaseAddr += dwLengthInRegion >> 8;
            } else {
                BaseAddr += dwLengthInRegion;
            }
            pfsmemblk ++;
        } while (dwLength);
    }

    // return the next free fsmemblk
    return pfsmemblk;
}

#define RAM_DETECT_SIG      0xfd1247e36a08bc95UL

static DWORD DetectRAM (DWORD dwPfnBase, DWORD MaxRamSize, LPVOID pAutoDetectVM)
{
    DWORD RamSize = 0;

    if (MaxRamSize) {
        volatile ULONGLONG *pRam  = (volatile ULONGLONG *) pAutoDetectVM;
        PAGETABLE *pptbl = GetPageTable (g_ppdirNK, VA2PDIDX (pRam));
        DWORD       idx2nd        = VA2PT2ND (pRam);
        DWORD       dwPgProt      = PageParamFormProtect (g_pprcNK, PAGE_READWRITE|PAGE_NOCACHE, (DWORD) pRam);
        DWORD       dwPfnMask     = (DWORD) ~ PA2PFN (VM_PAGE_OFST_MASK);
        DWORD       dwPfnLow      = dwPfnBase;
        DWORD       dwPfnHigh     = dwPfnBase + PA2PFN (MaxRamSize);
        DWORD       dwPfnMid;
        ULONGLONG   u64Save;

        // cannot wrap around (no integer overflow).
        PREFAST_DEBUGCHK (dwPfnBase < dwPfnHigh);
        DEBUGMSG (ZONE_MEMORY, (L"Detecting RAM from %8.8lx, of size %8.8lx\r\n", dwPfnBase, MaxRamSize));

        // use binary search to detect RAM
        do {
            // Calculate mid = (low + high) / 2.
            // Since integer overflow can occur with (low + high), we need to do individual
            // divide before the addition (mid = low/2 + high/2).
            dwPfnMid = ((dwPfnLow >> 1) + (dwPfnHigh >> 1)) & dwPfnMask;

            // create a temporary uncached mapping to map the dwPfnMid uncached
            pptbl->pte[idx2nd] = dwPfnMid | dwPgProt;
#ifdef ARM
            ARMPteUpdateBarrier (&pptbl->pte[idx2nd], 1);
#endif
            OEMCacheRangeFlush ((LPVOID) pRam, VM_PAGE_SIZE, CACHE_SYNC_FLUSH_D_TLB);

            // save original RAM content
            u64Save = *pRam;

            // write our signature
            *pRam = RAM_DETECT_SIG;

            if (RAM_DETECT_SIG == *pRam) {
                // RAM found, there are more RAM, search upper half
                dwPfnLow  = dwPfnMid + PFN_INCR;

                // restore orignal RAM content
                *pRam = u64Save;
            } else {
                // RAM not found, search lower half
                dwPfnHigh = dwPfnMid;
            }
        } while (dwPfnLow < dwPfnHigh);

        DEBUGCHK (dwPfnLow == dwPfnHigh);

        pptbl->pte[idx2nd] = MakeReservedEntry (VM_PAGER_NONE);
#ifdef ARM
        ARMPteUpdateBarrier (&pptbl->pte[idx2nd], 1);
#endif
        OEMCacheRangeFlush ((LPVOID) pRam, VM_PAGE_SIZE, CACHE_SYNC_FLUSH_D_TLB);

        RamSize = PFN2PA (dwPfnHigh - dwPfnBase);
    }

    return RamSize;
}


static void InitMemorySections (fslog_t *pLogPtr, PCRamTable pOEMRamTable, LPVOID pAutoDetectVM)
{
    mem_t *pfsmemblk        = pLogPtr->fsmemblk;
    DWORD  dwStaticRamStart = pTOC->ulRAMFree+MemForPT;
    DWORD  cTotalRamPages   = PAGECOUNT (g_pOemGlobal->dwMainMemoryEndAddress - dwStaticRamStart);
    DWORD  idx, fspages, RamSize;

    DEBUGCHK (cTotalRamPages > 1);

    // add the main region
    pfsmemblk = AddRegion (pfsmemblk, dwStaticRamStart, g_pOemGlobal->dwMainMemoryEndAddress - dwStaticRamStart, 0);

    // add extended/dynamic regions
    if (pOEMRamTable) {
        PCRAMTableEntry pRamEntry = pOEMRamTable->pRamEntries;

        // new style ram table.
        for (idx = 0; idx < pOEMRamTable->dwNumEntries; idx ++, pRamEntry ++) {
            DEBUGCHK (!((pRamEntry->ShiftedRamBase << 8) & VM_PAGE_OFST_MASK)); // base must be page aligned
            RamSize = DetectRAM (PFNfrom256 (pRamEntry->ShiftedRamBase), pRamEntry->RamSize, pAutoDetectVM);

            // if RamSize is less than a page, the page can only be used for bookkeeping, might as well get rid of it.
            // otherwise, add it to memory section.
            if (RamSize > VM_PAGE_SIZE) {
                cTotalRamPages += (RamSize >> VM_PAGE_SHIFT);
                pfsmemblk = AddRegion (pfsmemblk, pRamEntry->ShiftedRamBase, RamSize, pRamEntry->RamAttributes | DYNAMIC_MAPPED_RAM);
                DEBUGMSG (ZONE_MEMORY, (L"Dynamically mapped RAM, pfn start = %8.8lx, size %8.8lx\r\n",
                                PFNfrom256 (pRamEntry->ShiftedRamBase), RamSize));
            }
        }
    } else {
        MEMORY_SECTION MemSections[MAX_MEMORY_SECTIONS];
        DWORD          cExtSections = 0;

        // old style ram extention.
        if (g_pOemGlobal->pfnEnumExtensionDRAM) {
            cExtSections = g_pOemGlobal->pfnEnumExtensionDRAM (MemSections, MAX_MEMORY_SECTIONS - (pfsmemblk - pLogPtr->fsmemblk));
        } else if (OEMGetExtensionDRAM (&MemSections[0].dwStart, &MemSections[0].dwLen)) {
            cExtSections = 1;
        }

        for (idx = 0; idx < cExtSections; idx ++) {
            DEBUGCHK (!(MemSections[idx].dwStart & VM_PAGE_OFST_MASK));     // start must be page aligned
            DEBUGCHK (!(MemSections[idx].dwLen   & VM_PAGE_OFST_MASK));     // size must be page aligned
            RamSize = DetectRAM (GetPFN ((LPCVOID) MemSections[idx].dwStart), MemSections[idx].dwLen, pAutoDetectVM);
            if (RamSize > VM_PAGE_SIZE) {
                cTotalRamPages += (RamSize >> VM_PAGE_SHIFT);
                pfsmemblk = AddRegion (pfsmemblk, MemSections[idx].dwStart, RamSize, 0);
                DEBUGMSG (ZONE_MEMORY, (L"Statically mapped RAM, VA start = %8.8lx, size %8.8lx\r\n",
                                            MemSections[idx].dwStart, RamSize));
            }
        }
    }

    // update total number of mem blocks.
    pLogPtr->nMemblks = (pfsmemblk - pLogPtr->fsmemblk);
    DEBUGCHK (pLogPtr->nMemblks && (pLogPtr->nMemblks <= MAX_MEMORY_SECTIONS));

    // zero the rest (needed?)
    memset(&pLogPtr->fsmemblk[pLogPtr->nMemblks], 0, sizeof(mem_t) * (MAX_MEMORY_SECTIONS-pLogPtr->nMemblks));

    fspages = CalcFSPages (cTotalRamPages);

    // ask OAL to change fs pages if required
    if (g_pOemGlobal->pfnCalcFSPages)
        fspages = g_pOemGlobal->pfnCalcFSPages (cTotalRamPages, fspages);

    if (fspages > MAX_FS_PAGE) {
        fspages = MAX_FS_PAGE;
    }
    RETAILMSG(1,(L"Memory Configuring: Total pages: %d, Filesystem pages = %d\r\n", cTotalRamPages, fspages));

    // clear the header fields
    pLogPtr->pFSList = NULL;
    pLogPtr->magic2  = 0;
    pLogPtr->version = CURRENT_FSLOG_VERSION;

    // carve up the memory for filesys
    CarveFSMem (pLogPtr, fspages);

}

void SetupMemoryInfo (const mem_t *pfsmemblk, DWORD cSections, LPVOID pUseMapVM)
{
    DWORD       dwCurUseMapAddr = (DWORD) pUseMapVM;
    PREGIONINFO pfi;
    DWORD       cbExtension;

    // NOTE: pKData/pKEnd should never be used again. Keeping only the section now.
    MemoryInfo.pKData = (LPVOID) pfsmemblk->BaseAddr;
    MemoryInfo.pKEnd  = (LPVOID) (pfsmemblk->BaseAddr + pfsmemblk->Length);
    MemoryInfo.cFi    = cSections;
    MemoryInfo.pFi    = FreeInfo;

    for (pfi = FreeInfo; cSections --; pfsmemblk ++, pfi ++) {
        cbExtension   = PAGEALIGN_DOWN (pfsmemblk->ExtnAndAttrib);

        if (pfsmemblk->ExtnAndAttrib & DYNAMIC_MAPPED_RAM) {
            // dynamic mapped region
            VERIFY (VMCopyPhysical (g_pprcNK, dwCurUseMapAddr, PFNfrom256(pfsmemblk->BaseAddr), PAGECOUNT(cbExtension), PG_KRN_READ_WRITE, FALSE));
            pfi->pUseMap          = (PPHYSPAGELIST) dwCurUseMapAddr;
            dwCurUseMapAddr       += cbExtension;
            pfi->paStart          = PFNfrom256 (pfsmemblk->BaseAddr + (cbExtension >> 8));
        } else {
            // static mapped region
            pfi->pUseMap          = (PPHYSPAGELIST) pfsmemblk->BaseAddr;
            pfi->dwStaticMappedVA = pfsmemblk->BaseAddr + cbExtension;
            pfi->paStart          = GetPFN ((LPVOID) pfi->dwStaticMappedVA);
        }

        pfi->dwRAMAttributes  = pfsmemblk->ExtnAndAttrib & VM_PAGE_OFST_MASK;
        pfi->paEnd = pfi->paStart + PFN_INCR * ((pfsmemblk->Length - cbExtension) >> VM_PAGE_SHIFT);
        DEBUGMSG (ZONE_MEMORY, (L"section %d: pUseMap = %8.8lx, paStart = %8.8lx, paEnd = %8.8lx, cbExtention = %8.8lx, static VA = %8.8lx\r\n",
                                    pfi-FreeInfo, pfi->pUseMap, pfi->paStart, pfi->paEnd, cbExtension, pfi->dwStaticMappedVA));

        memset (pfi->pUseMap, 0, cbExtension);
    }
}


LPVOID ReserveVMForUseMap (PCRamTable pOEMRamTable)
{
    LPVOID pUseMapVM = NULL;

    if (pOEMRamTable) {
        DWORD cbUseMap = 0;
        DWORD idx, dwLength, dwLengthInRegion;

        // figure out total size needed for usemap
        for (idx = 0; idx < pOEMRamTable->dwNumEntries; idx ++) {
            dwLength = pOEMRamTable->pRamEntries[idx].RamSize;

            while (dwLength) {
                // calculate pages in this region
                dwLengthInRegion = (dwLength < MAX_REGION_SIZE)? dwLength : MAX_REGION_SIZE;

                cbUseMap += CalcUseMapSize (dwLengthInRegion);

                // move on to next if > MAX_REGION_SIZE
                dwLength  -= dwLengthInRegion;
            }
        }

        if (cbUseMap) {
            pUseMapVM = VMReserve (g_pprcNK, cbUseMap, 0, 0);
            DEBUGCHK (pUseMapVM);
            DEBUGMSG (ZONE_MEMORY, (L"ReserveVMForUseMap: VM reserved %8.8lx of length = %8.8lx\r\n", pUseMapVM, cbUseMap));
        }
    }

    return pUseMapVM;
}

static void AutoDetectMainMemory (LPVOID pAutoDetectVM)
{
    DWORD  dwStaticRamStart = pTOC->ulRAMFree + MemForPT;
    DWORD  RamSize = g_pOemGlobal->dwMainMemoryEndAddress - dwStaticRamStart;

    // detect the static mapped RAM regardless. Potentially grow/shrink RAM size.
    RamSize = DetectRAM (GetPFN ((LPCVOID) dwStaticRamStart), RamSize, pAutoDetectVM);
    DEBUGCHK (RamSize > VM_PAGE_SIZE);
    DEBUGMSG (dwStaticRamStart+RamSize != g_pOemGlobal->dwMainMemoryEndAddress,
              (L"KernelFindMemory: MainMemoryEndAddress adjusted from %8.8lx to %8.8lx(size excluding image ~= %dMB) by RAM auto-detect\r\n",
              g_pOemGlobal->dwMainMemoryEndAddress, dwStaticRamStart+RamSize, RamSize >> 20));
    g_pOemGlobal->dwMainMemoryEndAddress = dwStaticRamStart + RamSize;

}

extern const wchar_t NKCpuType [];
extern void InitDrWatson (void);
extern void InitVoidPages (void);

//------------------------------------------------------------------------------
//
// Divide the declared physical RAM between Object Store and User RAM.
//
//------------------------------------------------------------------------------
void
KernelFindMemory(void)
{
    PCRamTable pOEMRamTable = g_pOemGlobal->pfnGetOEMRamTable ();
    ROMChain_t *pList, *pListPrev = NULL;
    LPVOID     pUseMapVM     = ReserveVMForUseMap (pOEMRamTable);
    LPVOID     pAutoDetectVM = VMReserve (g_pprcNK, VM_BLOCK_SIZE, 0, 0);
    BOOL       fCleanBoot;
    // page align memory end address
    g_pOemGlobal->dwMainMemoryEndAddress &= ~VM_PAGE_OFST_MASK;

    // flush cache before we start doing anything, in case OEMInit access kernel owned memory...
    OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_DISCARD);

    // perform RAM auto-detection on main memory region
    AutoDetectMainMemory (pAutoDetectVM);

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

#if defined(x86) || defined (ARM)
    if (!MDValidateRomChain (ROMChain)) {
        NKDbgPrintfW (L"WARNING! XIP region span accross discontigious memory!!! System might not work properly on some flash parts!\r\n");
    }
#endif

    // initialize Kernel Dr.Watson support
    InitDrWatson ();

    // initialize SoftLog (will be no-op if not debug build)
    InitSoftLog ();

    // initialize void page support
    InitVoidPages ();


    DEBUGCHK (!(VM_PAGE_OFST_MASK & pTOC->ulRAMFree));
    DEBUGCHK (!(VM_PAGE_OFST_MASK & MemForPT));

    //
    // LogPtr at the start of RAM holds some header information
    // Using Cached access here.
    //
    LogPtr = (fslog_t *) (pTOC->ulRAMFree+MemForPT);
    MemForPT += VM_PAGE_SIZE;           // 1 page used for LogPtr


    RETAILMSG(1,(L"Booting Windows CE version %d.%2.2d for (%s)\r\n",CE_MAJOR_VER,CE_MINOR_VER, NKCpuType));
#ifdef MIPS
    if (IsMIPS16Supported)
        RETAILMSG (1, (L"MIPS16 Instructions Supported\r\n"));
    else
        RETAILMSG (1, (L"MIPS16 Instructions NOT Supported\r\n"));
#endif
    DEBUGMSG(ZONE_MEMORY, (L"pTOC = %8.8lx, pTOC->ulRamFree = %8.8lx, MemForPT = %8.8lx\r\n", pTOC, pTOC->ulRAMFree, MemForPT));

    fCleanBoot = fForceCleanBoot || (CURRENT_FSLOG_VERSION != LogPtr->version) || (LOG_MAGIC != LogPtr->magic1);

    if (fCleanBoot) {
        InitMemorySections (LogPtr, pOEMRamTable, pAutoDetectVM);
    }

    // release VM we reserved for auto-detection
    VMFreeAndRelease (g_pprcNK, pAutoDetectVM, VM_BLOCK_SIZE);

    // always reset on boot
    LogPtr->magic1 = 0;

#ifndef SHIP_BUILD
{
    DWORD idx;
    mem_t *pfsmemblk = LogPtr->fsmemblk;
    RETAILMSG(1, (L"Booting kernel with %s memory configuration:\r\n", fCleanBoot? L"clean" : L"existing"));

    RETAILMSG(1, (L"Total Memory Sections:%d\r\n", LogPtr->nMemblks));
    for (idx = 0; idx < LogPtr->nMemblks; idx++, pfsmemblk ++) {
        RETAILMSG(1,(L"[%d] (%s): start: %8.8lx, extension/attributes: %8.8lx, length: %8.8lx\r\n",
                    idx,
                    (DYNAMIC_MAPPED_RAM & pfsmemblk->ExtnAndAttrib)? L"dynamic" : L"static",
                    pfsmemblk->BaseAddr, pfsmemblk->ExtnAndAttrib, pfsmemblk->Length));
    }
}
#endif
    SetupMemoryInfo (LogPtr->fsmemblk, LogPtr->nMemblks, pUseMapVM);

    GrabFSPages (LogPtr->pFSList);

    InitMemoryPool ();      // setup physical memory
}


//------------------------------------------------------------------------------
// Format of RELOC section:
//
//  e32_unit[FIX].rva  = start of reloc info
//  e32_unit[FIX].size = size  of reloc info
//
//  ----- start of reloc section
//  block descrptor { rva, size } - 2 dwords, size include the block descriptor
//      FixupRecord - 16 bit each, 12 bit offset, 4 bit type, or a 16-bit value if the previous record is HIGHADJ
//      FixupRecord
//      ...
//  block descriptor
//      FixupRecord
//      FixupRecord
//      ...
//  ...
//  ----- end of reloc info
//
// (1) block descriptor must be 32-bit aligned, sorted by its rva field.
// (2) rva of block descriptor is always page-aligned.
// (3) each block descriptor is followed by all relocations needed within the same page.
// (4) offset field in FixupRecord is the offset into the page that needs to be fixed up.
//
//------------------------------------------------------------------------------

typedef union {
    struct {
        WORD offset : 12;
        WORD type   : 4;
    };
    SHORT AsShort;    // used only in HIGHADJ type fixup
} FixupRecord;
ERRFALSE (sizeof (FixupRecord) == 2);

typedef const FixupRecord *PCFixupRecord;

#define NEXT_RELOC_BLOCK(pReloc)     ((PCInfo) ((LPBYTE) (pReloc) + (pReloc)->size))

#ifdef DEBUG
void DumpRelocations (DWORD BasePtr, const e32_lite *eptr, LPCWSTR pszExecuteableName)
{
    DWORD dwTotalRelocSize = eptr->e32_unit[FIX].size;

    NKDbgPrintfW (L"===== Dumping relocation records for '%s', dwTotalRelocSize = %d\r\n", pszExecuteableName, dwTotalRelocSize);
    if (dwTotalRelocSize) {
        PCInfo pReloc    = (PCInfo) (BasePtr + eptr->e32_unit[FIX].rva);
        PCInfo pRelocEnd = (PCInfo) ((LPBYTE) pReloc + dwTotalRelocSize);
        PCFixupRecord pCurFixup, pLastFixup;
        NKDbgPrintfW (L"pReloc = %8.8lx, pRelocEnd = %8.8lx\r\n", pReloc, pRelocEnd);

        while ((pReloc < pRelocEnd) && pReloc->size) {
            NKDbgPrintfW (L"  -- Block at rva = %8.8lx, size %8.8lx\r\n", pReloc->rva, pReloc->size);
            pCurFixup  = (PCFixupRecord) (pReloc+1);
            pLastFixup = (PCFixupRecord) ((LPBYTE) pReloc + pReloc->size);

            while (pCurFixup < pLastFixup) {
                NKDbgPrintfW (L"  --   %8.8lx %8.8lx\r\n", pCurFixup->type, pCurFixup->offset);
                pCurFixup ++;
            }

            pReloc = NEXT_RELOC_BLOCK (pReloc);
        }
    }
}
#endif

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD
ReadFromFilesys(
    const openexe_t *oeptr,
    LPVOID pMem2,
    DWORD offset,
    const o32_lite *o32ptr,
    DWORD size
    )
{
    DWORD bytesread;
    DWORD retval = PAGEIN_SUCCESS;
    DWORD cbToRead = min(o32ptr->o32_psize - offset, size);

    DEBUGCHK(!OwnCS (&PagerCS));

    if ((o32ptr->o32_psize > offset)
        && (!FSReadFileWithSeek(oeptr->hf,pMem2,cbToRead,&bytesread,0,o32ptr->o32_dataptr + offset,0)
            || (bytesread != cbToRead))) {
        retval = (ERROR_DEVICE_REMOVED == KGetLastError (pCurThread))? PAGEIN_FAILURE : PAGEIN_RETRY;
    }
    return retval;
}


#ifdef MIPS
static DWORD CalculateMips16JumpFixup (DWORD dwOffset, WORD loWord, WORD hiWord)
{
    DWORD FixupValue = loWord & 0x03ff;

    FixupValue = ((((FixupValue >> 5) | ((FixupValue & 0x1f) << 5)) << 16) | hiWord) + (dwOffset >> 2);

    return FixupValue;
}

static void WriteMip16JumpLoWord (LPWORD pMem, DWORD FixupValue)
{
    FixupValue = (FixupValue >> 16) & 0x3ff;
    *pMem = (WORD)((*pMem & 0xfc00) | (FixupValue >> 5) | ((FixupValue & 0x1f) << 5));
}

#endif



static BOOL RelocateOnePage (
    const e32_lite *eptr,
    PCInfo         pReloc,
    DWORD          dwPageAddress,
    const DWORD    dwOffset,
    PCFixupRecord  *ppFixupPageCrossed
    )
{
    PCFixupRecord pFixup    = (PCFixupRecord) (pReloc+1);
    PCFixupRecord pFixupEnd = (PCFixupRecord) ((LPBYTE) pReloc + pReloc->size);
    BOOL     fRet = TRUE;
    DWORD    FixupAddress;      // address to be fixed up (== dwPageAddress + offset)
    DWORD    FixupValue;        // the fixup value
#ifdef ARM
    // tmp vars to calculate ARM fixups
    DWORD    dwCodeBytes, dwTmp, dwLowPart;
    DWORD    dwClearMask = THUMB2_CLEAR_MOVS_MASK;
#endif
    LPWORD   pFixupAddressHi = NULL;

    DEBUGMSG (ZONE_LOADER2, (L"RelocateOnePage: Fixup page @%8.8lx, offset = %8.8lx\r\n", dwPageAddress, dwOffset));

    DEBUGCHK (IsPageAligned (dwPageAddress));

    for ( ; fRet && (pFixup < pFixupEnd); pFixup ++) {

        FixupAddress = dwPageAddress + pFixup->offset;

        switch (pFixup->type) {
        case IMAGE_REL_BASED_ABSOLUTE:
            // no relocation needed
            break;

        case IMAGE_REL_BASED_HIGHLOW:
            // 32-bit fixup
            if (IsDwordAligned (FixupAddress)) {
                * (LPDWORD) FixupAddress += dwOffset;
            } else {
                // unaligned, check if page is crossed
                if ((pFixup->offset > 0xffc) && ppFixupPageCrossed) {
                    // page crossed, there can only be one of them
                    *ppFixupPageCrossed = pFixup;
                    break;
                }
                // Relocation is on this page, or page crossing allowed.
                *(UNALIGNED DWORD *)FixupAddress += dwOffset;
            }
            break;

        case IMAGE_REL_BASED_HIGH: // Save the address and go to get REF_LO paired with this one
            DEBUGMSG(1,(TEXT("RelocateOnepage: IMAGE_REL_BASE_HIGH detected %8.8lx\r\n"),pFixup->AsShort));
            pFixupAddressHi = (LPWORD) FixupAddress;
            break;

        case IMAGE_REL_BASED_LOW: // Low - (16-bit) relocate high part too.
            if (pFixupAddressHi) {
                FixupValue = (DWORD)(long)((*pFixupAddressHi) << 16) + *(LPWORD)FixupAddress + dwOffset;
                *pFixupAddressHi = (short)((FixupValue + 0x8000) >> 16);
                pFixupAddressHi = NULL;
            } else {
                FixupValue = *(PSHORT)FixupAddress + dwOffset;
            }
            *(LPWORD)FixupAddress = (WORD)(FixupValue & 0xffff);
            break;

        case IMAGE_REL_BASED_HIGHADJ: // 32 bit relocation of 16 bit high word, sign extended
            if (++ pFixup >= pFixupEnd) {
                // bad relocation record
                fRet = FALSE;
                break;
            }
            DEBUGMSG(ZONE_LOADER2,(TEXT("Grabbing extra data %8.8lx @%8.8lx\r\n"),*(LPWORD)pFixup, pFixup));
            *(LPWORD)FixupAddress += (WORD) ((dwOffset + pFixup->AsShort + 0x8000) >> 16);
            break;

#ifdef ARM
        case IMAGE_REL_BASED_ARM_MOV32:
        case IMAGE_REL_BASED_THUMB_MOV32:

            if (pFixup->type == IMAGE_REL_BASED_ARM_MOV32) {
                dwClearMask = ARM_CLEAR_MOVS_MASK;
            }

            // check if this is a cross page relocation
            if (IsDwordAligned (FixupAddress)) {

                // fixup the MOVW instruction

                // aligned address: we can dereference a DWORD pointer
                dwCodeBytes = *(LPDWORD)FixupAddress;

                // get current value encoded in the instruction
                FixupValue = ArmXDecodeImmediate16(pFixup->type, dwCodeBytes);
                dwLowPart = FixupValue;

                // fixing up MOVW only if the delta is less than 64k
                if (dwOffset & 0xFFFF) {
                    // add the offset to the decoded value
                    FixupValue += dwOffset;
                    FixupValue &= 0xFFFF;

                    // clear the fixup bits in the instruction
                    dwTmp = *(LPDWORD)FixupAddress;
                    dwTmp &= dwClearMask;
                    // calculate updated fixup and apply it to the code
                    dwTmp |= ArmXEncodeImmediate16(pFixup->type, (WORD)FixupValue);
                    *(LPDWORD)FixupAddress = dwTmp;
                }

                if ((pFixup->offset == 0xffc) && ppFixupPageCrossed) {
                    // MOVT is in next page, handling somewhere else
                    *ppFixupPageCrossed = pFixup;
                    break;
                }

                // fixup the MOVT instruction
                FixupAddress += sizeof(DWORD);

                // aligned address: we can dereference a DWORD pointer
                dwCodeBytes = *(LPDWORD)(FixupAddress);

                // get current value encoded in the instruction
                FixupValue = ArmXDecodeImmediate16(pFixup->type, dwCodeBytes);

                // add the offset to the decoded value
                FixupValue = (FixupValue << 16) | dwLowPart;
                FixupValue += dwOffset;
                FixupValue >>= 16;

                // clear the fixup bits in the instruction
                *(LPDWORD)FixupAddress &= dwClearMask;

                // calculate updated fixup and apply it to the code
                *(LPDWORD)FixupAddress |= ArmXEncodeImmediate16(pFixup->type, (WORD)FixupValue);

            } else {

                if ((pFixup->offset > 0xffc) && ppFixupPageCrossed) {
                    // page crossed, there can only be one of them
                    *ppFixupPageCrossed = pFixup;

                    break;
                }

                // Relocation is on this page, or page crossing allowed.
                // not aligned address: we have two use two WORD indirections
                dwCodeBytes = *(LPWORD)FixupAddress | (*(((LPWORD)FixupAddress)+1) << 16);

                // get current value encoded in the instruction
                FixupValue = ArmXDecodeImmediate16(pFixup->type, dwCodeBytes);
                dwLowPart = FixupValue;

                // fixing up MOVW only if the delta is less than 64k
                if (dwOffset & 0xFFFF) {
                    // add the offset to the decoded value
                    FixupValue += dwOffset;
                    FixupValue &= 0xFFFF;

                    // clear the fixup bits in the instruction
                    dwTmp = *(LPWORD)FixupAddress | ((*(LPWORD)(FixupAddress+2)) << 16);
                    dwTmp &= dwClearMask;

                    // calculate updated fixup and apply it to the codebytes
                    dwTmp |= ArmXEncodeImmediate16(pFixup->type, (WORD)FixupValue);
                    *(LPWORD)FixupAddress     = (dwTmp & 0xFFFF);
                    *(LPWORD)(FixupAddress+2) = (dwTmp >> 16);
                }

                // fixup the MOVT instruction

                if ((pFixup->offset == 0xffa) && ppFixupPageCrossed) {
                    // MOVT crosses this page, handling somewhere else
                    *ppFixupPageCrossed = pFixup;
                    break;
                }

                // calculating address of MOVT instruction
                FixupAddress += sizeof(DWORD);

                // Relocation is on this page, or page crossing allowed.
                // Not aligned address: we have two use two WORD indirections
                dwCodeBytes = *(LPWORD)FixupAddress | (*(((LPWORD)FixupAddress)+1) << 16);

                // decode value encoded in the instruction
                FixupValue = ArmXDecodeImmediate16(pFixup->type, dwCodeBytes);

                // add the offset to the decoded value
                FixupValue = (FixupValue << 16) | dwLowPart;
                FixupValue += dwOffset;
                FixupValue >>= 16;

                // clear the fixup bits in the instruction
                *(LPWORD)FixupAddress     &= (dwClearMask & 0xFFFF);
                *(LPWORD)(FixupAddress+2) &= (dwClearMask >> 16);

                // calculate updated fixup and apply it to the codebytes
                dwTmp = ArmXEncodeImmediate16(pFixup->type, (WORD)FixupValue);
                *(LPWORD)FixupAddress     |= (dwTmp & 0xFFFF);
                *(LPWORD)(FixupAddress+2) |= (dwTmp >> 16);

            }

            break;
#endif

#ifdef MIPS
        case IMAGE_REL_BASED_MIPS_JMPADDR: // jump to 26 bit target (shifted left 2)
            FixupValue = ((*(LPDWORD) FixupAddress) & 0x03ffffff) + (dwOffset >> 2);
            *(LPDWORD)FixupAddress = (*(LPDWORD)FixupAddress & 0xfc000000) | (FixupValue & 0x03ffffff);
            break;

        case IMAGE_REL_BASED_MIPS_JMPADDR16: // MIPS16 jal/jalx to 26 bit target (shifted left 2)
            if (IsMIPS16Supported) {

                if ((pFixup->offset > 0xffc) && ppFixupPageCrossed) {
                    // page crossed, there can only be one of them
                    *ppFixupPageCrossed = pFixup;
                    break;
                }

                // Relocation is on this page, or page crossing allowed.
                FixupValue = CalculateMips16JumpFixup (dwOffset, *(LPWORD) FixupAddress, *((LPWORD)FixupAddress+1));
                *((LPWORD)FixupAddress+1) = (WORD) FixupValue;
                WriteMip16JumpLoWord ((LPWORD) FixupAddress, FixupValue);
                break;
            }

            // fall through to fail the call
            __fallthrough;
#endif
        default:
            // unknown fixup type
            fRet = FALSE;
            DEBUGCHK (0);
        }
    }
    return fRet;
}

static BOOL IsValidRelocBlock (PCInfo pReloc, PCInfo pRelocEnd)
{
    return IsDwordAligned (pReloc)                      // block descriptor must be 32-bit aligned
        && IsDwordAligned (pReloc->size)                // size must be dword aligned
        && IsPageAligned  (pReloc->rva)                 // rva must be page aligned
        && ((int) pReloc->size > sizeof (struct info))  // cast to int, no negative size here
        && ((DWORD) pReloc + pReloc->size <= (DWORD) pRelocEnd);    // within the relocation area
}

static DWORD RelocatePageCrossingFixup (
    const openexe_t *oeptr,
    const o32_lite *optr,
    DWORD dwAddr,
    const DWORD dwOffset,
    LPBYTE pPgMem,
    PCFixupRecord pFixupPageCrossed,
    BOOL fPrevPage)          // has last page been paged/relocated ?
{
    DWORD dwPageResult = PAGEIN_SUCCESS;
    DWORD tmpDw = 0;
    DWORD FixupValue;
#ifdef ARM
    DWORD dwArmMask = THUMB2_CLEAR_MOVS_MASK;
    DWORD dwMovw, dwMovt, dwTmp, dwLowPart;
    DWORD FixupAddress;
    ULONGLONG llTmp = 0;
    WORD wTmp;
#endif
    DEBUGCHK (pFixupPageCrossed->offset > 0xff8);
    DEBUGCHK (!OwnCS (&PagerCS));

    switch (pFixupPageCrossed->type) {
    case IMAGE_REL_BASED_HIGHLOW:
        if (fPrevPage) {
            // relocation starts from previous page. read 4 bytes preceeding this page for fixup calculation
            dwPageResult = ReadFromFilesys (oeptr, &tmpDw, dwAddr - optr->o32_realaddr - 4, optr, 4);
            if (PAGEIN_SUCCESS == dwPageResult) {
                // update up to the 1st 3 bytes in the page
                switch (pFixupPageCrossed->offset & 0x3) {
                case 1:
                    FixupValue = (tmpDw >> 8) + (*pPgMem << 24) + dwOffset;
                    *pPgMem = (BYTE)(FixupValue >> 24);
                    break;
                case 2:
                    FixupValue = (tmpDw >> 16) + (*(LPWORD)pPgMem << 16) + dwOffset;
                    *(LPWORD)pPgMem = (WORD) (FixupValue >> 16);
                    break;
                case 3:
                    FixupValue = (tmpDw >> 24) + (*(LPDWORD)pPgMem << 8) + dwOffset;
                    *(LPWORD)pPgMem = (WORD) (FixupValue >> 8);
                    pPgMem[2] = (BYTE) (FixupValue >> 24);
                    break;
                }
            }
        } else {
            // relocation expand though the next page, update up to the last 3 bytes of the current page
            pPgMem += pFixupPageCrossed->offset;
            switch (pFixupPageCrossed->offset & 0x3) {
            case 1:
                // offset at 0xFFD, pPgMem = 0xnnnnnFFD, *(LPDWORD)(pPgMem-1) = last dword in the page
                FixupValue = dwOffset + ((*(LPDWORD) (pPgMem-1)) >> 8);
                *pPgMem = (BYTE)FixupValue;
                *(LPWORD) (pPgMem + 1) = (WORD) (FixupValue>>8);
                break;
            case 2:
                *(LPWORD)pPgMem += (WORD)dwOffset;
                break;
            case 3:
                *pPgMem += (BYTE)dwOffset;
                break;
            }
        }
        break;
#ifdef ARM

    case IMAGE_REL_BASED_ARM_MOV32:
    case IMAGE_REL_BASED_THUMB_MOV32:
        DEBUGMSG(ZONE_LOADER2, (L"ARM base reloc crosspage.\n"));

        if (pFixupPageCrossed->type == IMAGE_REL_BASED_ARM_MOV32) {
            dwArmMask = ARM_CLEAR_MOVS_MASK;
        }
        if (fPrevPage) {
            // relocation starts from previous page. read 2, 4 or 6 bytes preceeding this page for fixup calculation
            int nBytes = 0x10 - (pFixupPageCrossed->offset & 0xF);
            dwPageResult = ReadFromFilesys (oeptr, &llTmp, dwAddr - optr->o32_realaddr - nBytes, optr, nBytes);

            if (PAGEIN_SUCCESS == dwPageResult) {
                switch (nBytes) {
                    case 2:
                        dwMovw = (llTmp & 0xFFFF) | (( *(LPWORD)pPgMem) << 16);
                        dwMovt = *(LPWORD)(pPgMem+2) | ((*(LPWORD)(pPgMem+4)) << 16);
                        break;

                    case 4:
                        dwMovw = llTmp & 0xFFFFFFFF;
                        dwMovt = *(LPDWORD)pPgMem;
                        break;

                    case 6:
                        dwMovw = llTmp & 0xFFFFFFFF;
                        dwMovt = ((llTmp & 0xFFFF00000000) >> 32) | ((*(LPWORD)pPgMem) << 16);
                        break;

                    default:
                        // the compiler is not so smart to realize that dwMovw is *always* initialized
                        // making him happy
                        dwMovt = dwMovw = 0;
                        break;
                }

                FixupValue = ArmXDecodeImmediate16(pFixupPageCrossed->type, dwMovw);
                dwLowPart = FixupValue;

                // encoding the MOVW instruction
                // this happens only if MOVW starts at FFE
                //  AND
                // the delta we're adding is less than 64K

                if ((nBytes == 2) && (dwOffset & 0xFFFF)) {

                    FixupValue += dwOffset;

                    FixupValue = ArmXEncodeImmediate16(pFixupPageCrossed->type, (WORD)FixupValue);

                    wTmp = *(LPWORD)pPgMem;
                    wTmp &= dwArmMask >> 16;
                    wTmp |= FixupValue >> 16;
                    *(LPWORD)pPgMem = wTmp;
                }

                // relocating the MOVT instruction

                FixupValue = ArmXDecodeImmediate16(pFixupPageCrossed->type, dwMovt);
                FixupValue = (FixupValue << 16) | dwLowPart;
                FixupValue += dwOffset;
                FixupValue >>= 16;

                FixupValue = ArmXEncodeImmediate16(pFixupPageCrossed->type, (WORD)FixupValue);

                switch (nBytes) {
                    case 2:
                        pPgMem += 2;
                        wTmp = *(LPWORD)pPgMem;
                        wTmp &= (dwArmMask & 0xFFFF);
                        wTmp |= (FixupValue & 0xFFFF);
                        *(LPWORD)pPgMem = wTmp;
                        wTmp = *(LPWORD)(pPgMem+2);
                        wTmp &= (dwArmMask >> 16);
                        wTmp |= (FixupValue >> 16);
                        *(LPWORD)(pPgMem+2) = wTmp;
                        break;

                    case 4:
                        dwTmp = *(LPDWORD)pPgMem;
                        dwTmp &= dwArmMask;
                        dwTmp |= FixupValue;
                        *(LPDWORD)pPgMem = dwTmp;
                        break;

                    case 6:
                        wTmp = *(LPWORD)pPgMem;
                        wTmp &= (dwArmMask >> 16);
                        wTmp |= (FixupValue >> 16);
                        *(LPWORD)pPgMem = wTmp;
                        break;
                }

                break;
            }
        } else {
            // relocation starts on this page. read 0 or 2 bytes following this page for fixup calculation
            DWORD nBytes = (pFixupPageCrossed->offset & 0xF) % 4;
            dwTmp = 0;

            if (nBytes == 2) {
                dwPageResult = ReadFromFilesys (oeptr, &dwTmp, dwAddr - optr->o32_realaddr + VM_PAGE_SIZE, optr, 2);

                if (PAGEIN_SUCCESS != dwPageResult) {
                    break;
                }
            }

            FixupAddress = (DWORD)pPgMem + pFixupPageCrossed->offset;

            switch (pFixupPageCrossed->offset & 7) {
                case 6:
                    // Read higher 2 bytes of MOVW.
                    // MOVT is entirely in the next page so we don't care about it here
                    // Note that FixupAddress is not DWORD aligned here (FFE)
                    dwMovw = *(LPWORD)FixupAddress | (dwTmp << 16);
                    dwMovt = 0;
                    break;

                case 4:
                    // MOVW is entirely in current page,
                    // MOVT is entirely in next page: no action to be taken
                    // Note that FixupAddress IS aligned here (FFC).
                    dwMovw = *(LPDWORD)FixupAddress;
                    dwMovt = 0;
                    break;

                case 2:
                    // MOVW is entirely in current page,
                    // MOVT is crossing the page.
                    // Note that FixupAddress is UNaligned here
                    dwMovw = *(LPWORD)FixupAddress | ((*(LPWORD)(FixupAddress + 2)) << 16);
                    dwMovt = *(LPWORD)(FixupAddress+4) | (dwTmp << 16);
                    break;

                default:
                    // the compiler is not so smart to realize that dwMovw is *always* initialized
                    // making him happy
                    dwMovt = dwMovw = 0;
                    break;
            }

            FixupValue = ArmXDecodeImmediate16(pFixupPageCrossed->type, dwMovw);
            dwLowPart = FixupValue;

            // Relocating the MOVW only if the delta we're using is less than 64K
            if (dwOffset & 0xFFFF) {
                FixupValue += dwOffset;
                FixupValue &= 0xFFFF;

                FixupValue = ArmXEncodeImmediate16(pFixupPageCrossed->type, (WORD)FixupValue);

                switch (pFixupPageCrossed->offset & 7) {
                    case 6:
                        wTmp = *(LPWORD)FixupAddress;
                        wTmp &= (dwArmMask >> 16);
                        wTmp |= FixupValue >> 16;
                        *(LPWORD)FixupAddress = wTmp;
                        break;

                    case 4:
                        wTmp = *(LPWORD)FixupAddress;
                        wTmp &= dwArmMask;
                        wTmp |= FixupValue;
                        *(LPWORD)FixupAddress = wTmp;
                        break;

                    case 2:
                        wTmp = *(LPWORD)FixupAddress;
                        wTmp &= (dwArmMask & 0xFFFF);
                        wTmp |= (FixupValue & 0xFFFF);
                        *(LPWORD)FixupAddress = wTmp;
                        wTmp = *(LPWORD)(FixupAddress+2);
                        wTmp &= (dwArmMask >> 16);
                        wTmp |= (FixupValue >> 16);
                        *(LPWORD)(FixupAddress+2) = wTmp;
                        break;
                }
            }

            // relocating the MOVT instruction
            if ((pFixupPageCrossed->offset & 0xF) == 0xA) {
                FixupValue = ArmXDecodeImmediate16(pFixupPageCrossed->type, dwMovt);
                FixupValue = (FixupValue << 16) | dwLowPart;
                FixupValue += dwOffset;
                FixupValue >>= 16;

                FixupValue = ArmXEncodeImmediate16(pFixupPageCrossed->type, (WORD)FixupValue);
                wTmp = *(LPWORD)(FixupAddress+4);
                wTmp &= (dwArmMask & 0xFFFF);
                wTmp |= (FixupValue & 0xFFFF);
                *(LPWORD)(FixupAddress+4) = wTmp;
            }

        }
        break;
#endif
#ifdef MIPS
    case IMAGE_REL_BASED_MIPS_JMPADDR16:
        if (!IsMIPS16Supported || ((pFixupPageCrossed->offset & 3) != 2)) {
            // bad record
            dwPageResult = PAGEIN_FAILURE;
            break;
        }
        if (fPrevPage) {
            // Relocation is on previous page, crossing into the current one
            // Do this page's portion == last 2 bytes of jal/jalx == least-signif 16 bits of address
            *(LPWORD)pPgMem += (WORD)(dwOffset >> 2);

        } else {
            // Relocation is on this page, span into next page. peek into the next page.
            // read the next 2 bytes following this page
            dwPageResult = ReadFromFilesys (oeptr, &tmpDw, dwAddr - optr->o32_realaddr + VM_PAGE_SIZE, optr, 2);
            if (PAGEIN_SUCCESS == dwPageResult) {
                pPgMem += pFixupPageCrossed->offset;
                FixupValue = CalculateMips16JumpFixup (dwOffset, *(LPWORD) pPgMem, (WORD) tmpDw);
                WriteMip16JumpLoWord ((LPWORD) pPgMem, FixupValue);
            }
        }
        break;
#endif
    default:
        dwPageResult = PAGEIN_FAILURE;
        DEBUGCHK (0);
    }

    return dwPageResult;
}

//------------------------------------------------------------------------------
// Relocate a page in DLL or EXE during paging
//------------------------------------------------------------------------------
static DWORD
RelocateForPaging (
    PPROCESS pprc,
    const e32_lite *eptr,
    const o32_lite *optr,
    const openexe_t *oeptr,
    DWORD          NewBaseAddr,
    DWORD          dwAddr,
    LPBYTE         pPgMem
    )
{
    BOOL  dwRelocateResult       = PAGEIN_SUCCESS;
    const DWORD dwTotalRelocSize = eptr->e32_unit[FIX].size;
    const DWORD dwOffset         = NewBaseAddr - eptr->e32_vbase;
    const DWORD dwPageRva        = dwAddr - NewBaseAddr;

    DEBUGCHK (IsPageAligned (dwAddr) && IsPageAligned ((DWORD)pPgMem));
    DEBUGCHK (!OwnCS (&PagerCS));

    if (dwTotalRelocSize
        && dwOffset
        && (dwPageRva - eptr->e32_unit[FIX].rva >= dwTotalRelocSize)) {     // not in .reloc area
        PCInfo pReloc    = (PCInfo) (NewBaseAddr + eptr->e32_unit[FIX].rva);
        PCInfo pRelocEnd = (PCInfo) ((LPBYTE) pReloc + dwTotalRelocSize);
        PCFixupRecord pFixupPageCrossed;

        pprc = SwitchVM (pprc);
        __try {
            for ( ; (pReloc < pRelocEnd) && (pReloc->rva <= dwPageRva); pReloc = NEXT_RELOC_BLOCK (pReloc)) {

                DEBUGMSG (ZONE_LOADER2, (L"'pReloc = %8.8lx, pReloc->rva = %8.8lx, pReloc->size = %8.8lx\r\n",
                            pReloc, pReloc->rva, pReloc->size));

                if (!IsValidRelocBlock (pReloc, pRelocEnd)) {
                    dwRelocateResult = PAGEIN_FAILURE;
                    break;
                }

                if (pReloc->rva == dwPageRva) {
                    // found the reloc block for the page to be paged in.
                    pFixupPageCrossed = NULL;
                    if (!RelocateOnePage (eptr, pReloc, (DWORD) pPgMem, dwOffset, &pFixupPageCrossed)) {
                        dwRelocateResult = PAGEIN_FAILURE;
                        break;
                    }
                    if (pFixupPageCrossed) {
                        // last record of the page cross page boundary, fix it up here
                        dwRelocateResult = RelocatePageCrossingFixup (oeptr, optr, dwAddr, dwOffset, pPgMem, pFixupPageCrossed, FALSE);
                    }
                    break;
                }

                if (pReloc->rva + VM_PAGE_SIZE == dwPageRva) {
                    // previous page, check for cross page fixup.
                    // only need to check 2 fixup record at the end of the fixup block (possibly a filler)
                    pFixupPageCrossed = (PCFixupRecord) ((DWORD) pReloc + pReloc->size - sizeof (FixupRecord));

                    if (IMAGE_REL_BASED_ABSOLUTE == pFixupPageCrossed->type) {
                        // last record is a filler, look into previous one
                        pFixupPageCrossed --;
                    }

                    switch (pFixupPageCrossed->type) {
                        case IMAGE_REL_BASED_HIGHLOW:
#ifdef MIPS
                        case IMAGE_REL_BASED_MIPS_JMPADDR16:
#endif
                            if (pFixupPageCrossed->offset > 0xffc) {
                                dwRelocateResult = RelocatePageCrossingFixup (oeptr, optr, dwAddr, dwOffset, pPgMem, pFixupPageCrossed, TRUE);
                            }
                            break;

#ifdef ARM
                        case IMAGE_REL_BASED_THUMB_MOV32:
                        case IMAGE_REL_BASED_ARM_MOV32:
                            if (pFixupPageCrossed->offset > 0xff8) {
                                dwRelocateResult = RelocatePageCrossingFixup (oeptr, optr, dwAddr, dwOffset, pPgMem, pFixupPageCrossed, TRUE);
                            }
                            break;
#endif

                        default:
                            break;
                    }

                    if (PAGEIN_SUCCESS != dwRelocateResult) {
                        break;
                    }
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            dwRelocateResult = PAGEIN_FAILURE;
        }
        SwitchVM (pprc);
    }

    return dwRelocateResult;
}



//------------------------------------------------------------------------------
// Relocate a DLL or EXE
//------------------------------------------------------------------------------
static BOOL
Relocate (
    const e32_lite *eptr,
    const o32_lite *optr,
    DWORD   NewBaseAddr
    )
{
    BOOL  fRet                   = TRUE;
    const DWORD dwTotalRelocSize = eptr->e32_unit[FIX].size;
    const DWORD dwOffset         = NewBaseAddr - eptr->e32_vbase;

    DEBUGCHK (!OwnCS (&PagerCS));

    if (dwTotalRelocSize && dwOffset) {

        PCInfo pReloc    = (PCInfo) (NewBaseAddr + eptr->e32_unit[FIX].rva);
        PCInfo pRelocEnd = (PCInfo) ((LPBYTE) pReloc + dwTotalRelocSize);

        __try {
            for ( ; fRet && (pReloc < pRelocEnd); pReloc = NEXT_RELOC_BLOCK (pReloc)) {

                DEBUGMSG (ZONE_LOADER2, (L"'pReloc = %8.8lx, pReloc->rva = %8.8lx, pReloc->size = %8.8lx\r\n",
                            pReloc, pReloc->rva, pReloc->size));

                if (optr) {
                    if (optr->o32_rva > pReloc->rva) {
                        continue;
                    }
                    if (optr->o32_rva + optr->o32_vsize <= pReloc->rva) {
                        break;
                    }
                }

                fRet = IsValidRelocBlock (pReloc, pRelocEnd)
                    && RelocateOnePage (eptr, pReloc, NewBaseAddr + pReloc->rva, dwOffset, NULL);

            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            fRet = FALSE;
        }
    }

    return fRet;
}

static BOOL RelocateNonPageAbleSections (
    const openexe_t *oeptr,
    const e32_lite *eptr,
    const o32_lite *optr,
    DWORD   NewBaseAddr
    )
{
    BOOL fRet = TRUE;
    if (!PageAble (oeptr)) {
        fRet = Relocate (eptr, NULL, NewBaseAddr);

    } else if (!FullyPageAble (oeptr)) {
        const o32_lite *optrEnd = optr + eptr->e32_objcnt;
        for ( ; optr < optrEnd; optr ++) {
            if (   (optr->o32_flags & IMAGE_SCN_MEM_NOT_PAGED)
                && !Relocate (eptr, optr, NewBaseAddr)) {
                fRet = FALSE;
                break;
            }
        }
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
        DecommitExe (g_pprcNK, &pMod->oe, &pMod->e32, pMod->o32_ptr);

        if (   (FA_PREFIXUP & pMod->oe.filetype)
            && ((DWORD) pMod->BasePtr == pMod->e32.e32_vbase)) {
            // ROM MODULE not relocated, just decommit
            VMDecommit (g_pprcNK, pMod->BasePtr, pMod->e32.e32_vsize, VM_PAGER_LOADER);

        } else {
            // Relocated ROM DLL or RAM DLL, release the VM completely

            // Then release the VM completely
            VMFreeAndRelease (g_pprcNK, pMod->BasePtr, pMod->e32.e32_vsize);
        }
    }

    if (pMod->o32_ptr) {
        PNAME pAlloc = (PNAME) ((DWORD) pMod->o32_ptr - 4);
        FreeMem (pAlloc, pAlloc->wPool);
    }

    FreeExeName(&pMod->oe);
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
    PHDATA phdAuth = NULL;
    PMODULE pmodRes = NULL;
    HANDLE  hf = INVALID_HANDLE_VALUE;

    LockModuleList ();

    // Notify KD of fully unloaded module, before removing our refcount.  That
    // way if KD gets a page fault it doesn't end up locking/unlocking and
    // destroying the module under us.
    if (1 == pMod->wRefCnt) {
        if (!(DONT_RESOLVE_DLL_REFERENCES & pMod->wFlags)) {
            (* HDModUnload) ((DWORD) pMod, TRUE);
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

            // unloading a kernel dll, so log a kernel-specific free notification
            CELOG_ModuleFree (g_pprcNK, pMod);
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

        // cleanup authorization handle outside the module list lock
        phdAuth = pMod->phdAuth;

        // FreeModuleMemory only access VM, and it's safe to do it holding modulelist lock. In addition, we have
        // to do it while holding the lock, or a LoadLibrary call to the same module can fail due to VM allocation failure.
        FreeModuleMemory (pMod);

    }

    UnlockModuleList ();

    if (INVALID_HANDLE_VALUE != hf) {
        HNDLCloseHandle (g_pprcNK, hf);
    }

    if (phdAuth) {
        UnlockHandleData ((PHDATA)phdAuth->dwData);
        UnlockHandleData (phdAuth);
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
        *lpflags             = MAKELONG (e32rptr->e32_imageflags, IMAGE_DLLCHARACTERISTICS_NX_COMPAT);
        *lpEntry             = e32rptr->e32_entryrva;
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
        DWORD bytesread;

        DEBUGMSG(ZONE_LOADER1,(TEXT("PEBase at %8.8lx, hf = %8.8lx\r\n"),oeptr->offset, oeptr->hf));
        if ((FSSetFilePointer(oeptr->hf,oeptr->offset,0,FILE_BEGIN) != oeptr->offset)
            || !FSReadFile(oeptr->hf,(LPBYTE)&e32,sizeof(e32_exe),&bytesread,0)
            || (bytesread != sizeof(e32_exe))
            || (*(LPDWORD)e32ptr->e32_magic != 0x4550)
            || (!e32.e32_vsize)
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
#elif defined(ARMV5) || defined(ARMV6) || defined(ARMV7)
            if (e32ptr->e32_cpu != IMAGE_FILE_MACHINE_ARM)
                return ERROR_BAD_EXE_FORMAT;
#else
            return ERROR_BAD_EXE_FORMAT;
#endif
        }

        if ((NX_SUPPORT_STRICT == g_dwNXFlags) && !(IMAGE_DLLCHARACTERISTICS_NX_COMPAT & e32ptr->e32_dllflags)) {
            // strick mode NX support - all executables must be NX compatible
            DEBUGMSG (ZONE_ERROR, (L"LoadE32: Failed Loading executables that is not NX compatible\r\n"));
            return ERROR_BAD_EXE_FORMAT;
        }

        eptr->e32_objcnt     = e32ptr->e32_objcnt;
        *lpflags             = MAKELONG (e32ptr->e32_imageflags, e32ptr->e32_dllflags);
        *lpEntry             = e32ptr->e32_entryrva;
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

        // validate relocation section
        if (eptr->e32_unit[FIX].size
            && (    (eptr->e32_unit[FIX].rva  > eptr->e32_vsize)
                 || (eptr->e32_unit[FIX].size > eptr->e32_vsize)
                 || (eptr->e32_unit[FIX].rva + eptr->e32_unit[FIX].size > eptr->e32_vsize))) {
            // relocation record out or range
            return ERROR_BAD_EXE_FORMAT;
        }

        // validate import section
        if (eptr->e32_unit[IMP].size
            && (    (eptr->e32_unit[IMP].rva  > eptr->e32_vsize)
                 || (eptr->e32_unit[IMP].size > eptr->e32_vsize)
                 || (eptr->e32_unit[IMP].rva + eptr->e32_unit[IMP].size > eptr->e32_vsize))) {
            // import record out or range
            return ERROR_BAD_EXE_FORMAT;
        }

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
// PageInOnePage
//------------------------------------------------------------------------------
static DWORD PageInOnePage (
        PPROCESS pprc,
        const openexe_t *oeptr,
        const e32_lite *eptr,
        DWORD BasePtr,
        const o32_lite *optr,
        DWORD addr,
        BOOL fRelocate,
        LPBYTE pPgMem)
{
    DWORD retval;
    DWORD dwOffset;

    DEBUGCHK (!(addr & (VM_PAGE_SIZE-1)));    // addr must be page aligned.
    DEBUGCHK (OwnCS (&PagerCS));

    LeaveCriticalSection (&PagerCS);

    // we should've never page-in XIP, r/o, and uncompressed page. If the debugchk failed, most likely we paged
    // out something that we shouldn't.
    DEBUGCHK (!(FA_XIP & oeptr->filetype) || (optr->o32_flags & (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_COMPRESSED)));

    dwOffset = addr - optr->o32_realaddr;

    if (FA_DIRECTROM & oeptr->filetype) {

        if (optr->o32_flags & IMAGE_SCN_COMPRESSED) {
            DecompressROM ((LPVOID)(optr->o32_dataptr), optr->o32_psize, pPgMem, VM_PAGE_SIZE, dwOffset);
        } else if (optr->o32_psize > dwOffset) {
            memcpy ((LPVOID)pPgMem, (LPVOID)(optr->o32_dataptr+dwOffset), min(optr->o32_psize-dwOffset,VM_PAGE_SIZE));
        }
        retval = PAGEIN_SUCCESS;
    } else {
        retval = ReadFromFilesys (oeptr, pPgMem, addr - optr->o32_realaddr, optr, VM_PAGE_SIZE);
    }

    DEBUGMSG ((PAGEIN_SUCCESS != retval) && ZONE_PAGING, (L"    PI-RAM: PIP Failed, retval = %d!\r\n", retval));

    if ((PAGEIN_SUCCESS == retval) && fRelocate) {

        retval = RelocateForPaging (pprc, eptr, optr, oeptr, BasePtr, addr, pPgMem);
        DEBUGMSG ((PAGEIN_SUCCESS != retval) && ZONE_PAGING, (L"    PI-RAM: Relocate Failed, retval = %d!\r\n", retval));

    }

    EnterCriticalSection (&PagerCS);

    return retval;
}

//------------------------------------------------------------------------------
// The address is committed in kernel but not in the destination process
// Since this is a r/w page, make a copy of the data and virtual copy
// to the destination process.
//------------------------------------------------------------------------------
int CopyFromKernel(PPROCESS pprcDst, const o32_lite *optr, DWORD addr)
{
    BOOL retval = PAGEIN_SUCCESS;
    LPBYTE pPagingMem = NULL;
    PPROCESS pprcActv, pprcVM;

    // Get a page to write to
    pPagingMem = GetPagingPage (NULL, addr);

    if (pPagingMem) {

        // addr is committed in kernel; switch to kernel to do the memcpy
        pprcActv = SwitchActiveProcess(g_pprcNK);
        pprcVM = SwitchVM(g_pprcNK);

        // copy the memory from kernel to the paging mem
        memcpy(pPagingMem, (LPVOID)addr, VM_PAGE_SIZE);

        SwitchActiveProcess(pprcActv);
        SwitchVM(pprcVM);

        // move the mapping to the destination address
        if (!VMMove (pprcDst, addr, (DWORD) pPagingMem, VM_PAGE_SIZE, optr->o32_access)) {
            retval = PAGEIN_RETRY;
            DEBUGMSG(ZONE_PAGING,(L"  CopyFromKernel: Page in failure in VC!\r\n"));
        }
        FreePagingPage (NULL, pPagingMem);

    } else {
        // Failed to get a page.
        retval = PAGEIN_RETRY;
    }

    return retval;
}

FORCEINLINE BOOL SectionCanBePagedOut (const openexe_t *oeptr, const o32_lite *optr)
{
    return  !(optr->o32_flags & (IMAGE_SCN_MEM_WRITE|IMAGE_SCN_MEM_NOT_PAGED))
         && (!(oeptr->filetype & FA_XIP) || (optr->o32_flags & IMAGE_SCN_COMPRESSED));
}

int PageIntoKernel (PMODULE pMod, const o32_lite *optr, DWORD addr)
{
    PagePool_t *pPool   = NULL;
    ULONG       access;
    LPBYTE      pPagingMem;
    int         retval;

    if (IsKModeAddr ((DWORD) pMod->BasePtr)) {
        // kernel DLL, use paging pool only for r/o pages
        if (!(optr->o32_flags & IMAGE_SCN_MEM_WRITE)) {
            pPool = &g_LoaderPool;
        }
        access = optr->o32_access;
    } else {
        // user DLL - use paging pool unless it's r/w and shared.
        //            ALWAYS PAGEIN READ-ONLY to kernel's master copy
        if (((IMAGE_SCN_MEM_SHARED|IMAGE_SCN_MEM_WRITE) & optr->o32_flags) != (IMAGE_SCN_MEM_SHARED|IMAGE_SCN_MEM_WRITE)) {
            pPool = &g_LoaderPool;
        }
        access = (VM_EXECUTABLE_PROT & optr->o32_access)? PAGE_EXECUTE_READ : PAGE_READONLY;
    }

    if (optr->o32_flags & IMAGE_SCN_MEM_NOT_PAGED) {
        // if we hit this debugchk, it means that we're paging in something that is not pageable.
        // i.e. we paged out something that we shouldn't.
        DEBUGCHK (0);
        pPool = NULL;
    }

    // Get a page for paging
    pPagingMem = GetPagingPage (pPool, addr);

    if (pPagingMem) {
        // call the paging function based on filetype
        retval = PageInOnePage (g_pprcNK, &pMod->oe, &pMod->e32, (DWORD) pMod->BasePtr,
                             optr, addr, !(pMod->wFlags & LOAD_LIBRARY_AS_DATAFILE), pPagingMem);

        // Move VM mapping from NK to destination if paged-in successfully
        if ((PAGEIN_SUCCESS == retval)
            && !VMMove (g_pprcNK, addr, (DWORD) pPagingMem, VM_PAGE_SIZE, access)) {
            retval = PAGEIN_RETRY;
            DEBUGMSG(ZONE_PAGING,(L"  PageInModule: Page in failure in VMMove!\r\n"));
        }

        // Decommit and release the temporary memory
        FreePagingPage (pPool, pPagingMem);

    } else {
        // Failed to get a page.  The loader pool trim thread must be at too
        // low of a priority to help this thread.

        retval = PAGEIN_RETRY;
    }

    return retval;
}

#ifndef SHIP_BUILD
LONG g_lPageInCount;
#endif 

//------------------------------------------------------------------------------
// PageInModule
//------------------------------------------------------------------------------
static int PageInModule (PPROCESS pprcDst, PMODULE pMod, DWORD addr, BOOL fWrite)
{
    const o32_lite *optr;
    int     retval;

    DEBUGMSG (ZONE_PAGING, (L"PageInModule: Paging in %8.8lx\r\n", addr));

    // use page start to page in
    addr &= -VM_PAGE_SIZE;

    DEBUGCHK ((pprcDst != g_pprcNK) || (addr > VM_KMODE_BASE) || !fWrite);
    DEBUGCHK (OwnCS(&PagerCS));

    // app debuggers use special address to read dll names; don't break in that case.
    DEBUGCHK (((pCurThread)->hMQDebuggerRead) || PageAble (&pMod->oe));

    optr = FindOptr (pMod->o32_ptr, pMod->e32.e32_objcnt, addr);
    if (!optr
        || (fWrite && !(optr->o32_flags & IMAGE_SCN_MEM_WRITE))) {
        // fail if can't find it in any section, or try to write to R/O section
        retval = PAGEIN_FAILURE;

    // we can directly look at NK's VM because
    // (1) we've locked the module, thus the VM of NK cannot be freed, and
    // (2) we hold the pager critical section, thus the page cannot be paged out.
    } else if (IsAddrCommitted (g_pprcNK, addr)) {
        // kernel copy already paged in
        retval = PAGEIN_SUCCESS;
    } else {
        // always page into kenrel 1st
        retval = PageIntoKernel (pMod, optr, addr);
    }

    if ((PAGEIN_SUCCESS == retval) && (pprcDst != g_pprcNK)) {
        if ((optr->o32_flags & IMAGE_SCN_MEM_WRITE) && !(optr->o32_flags & IMAGE_SCN_MEM_SHARED)) {
            // r/w, not shared, make a copy from kernel
            DEBUGMSG (ZONE_PAGING, (L"PageInModule: Copy from NK %8.8lx!\r\n", addr));
            retval = CopyFromKernel (pprcDst, optr, addr);
        } else {
            // for RO or Shared-write sections, virtual copy from NK
            DEBUGMSG (ZONE_PAGING, (L"PageInModule: VirtualCopy from NK %8.8lx!\r\n", addr));

            if (!VMCopyCommittedPages (pprcDst, addr, VM_PAGE_SIZE, optr->o32_access)) {
                DEBUGMSG(ZONE_PAGING,(L"PIM: VirtualCopy Failed 1\r\n"));
                retval = PAGEIN_RETRY;
            }
        }
    }

    if (PAGEIN_SUCCESS == retval) {
        pMod->wFlags |= MF_PAGED_IN; // hint to page out code to scan this module on low memory
#ifndef SHIP_BUILD
        InterlockedIncrement (&g_lPageInCount);
#endif
    }

    return retval;
}

//------------------------------------------------------------------------------
// PageInProcess
//------------------------------------------------------------------------------
static int PageInProcess (PPROCESS pProc, DWORD addr, BOOL fWrite)
{
    const e32_lite *eptr = &pProc->e32;
    const o32_lite *optr;
    BOOL     retval;

    DEBUGMSG (ZONE_PAGING, (L"PageInProcess: Paging in %8.8lx\r\n", addr));
    DEBUGCHK (OwnCS(&PagerCS));    

    optr = FindOptr (pProc->o32_ptr, eptr->e32_objcnt, addr);
    if (!optr) {
        DEBUGMSG (ZONE_PAGING, (L"PIP: FindOptr failed\r\n"));
        retval = PAGEIN_FAILURE;

    } else {

        LPBYTE   pPagingMem;
        PagePool_t *pPool = SectionCanBePagedOut (&pProc->oe, optr)? &g_LoaderPool : NULL;

        // use page start to page in
        addr &= -VM_PAGE_SIZE;

        // app debuggers use special address to read exe names; don't break in that case.
        DEBUGCHK (((pCurThread)->hMQDebuggerRead) || PageAble (&pProc->oe));

        // Get a page to write to
        pPagingMem = GetPagingPage (pPool, addr);

        if (pPagingMem) {

            retval = PageInOnePage (pProc, &pProc->oe, eptr, (DWORD) pProc->BasePtr, optr, addr, TRUE, pPagingMem);

            // VirtualCopy from NK to destination if paged-in successfully
            if ((PAGEIN_SUCCESS == retval)
                && !VMMove (pProc, addr, (DWORD) pPagingMem, VM_PAGE_SIZE, optr->o32_access)) {
                retval = PAGEIN_RETRY;
                DEBUGMSG(ZONE_PAGING,(L"  PageInProcess: Page in failure in VC!\r\n"));
            }

            // Decommit and release the temporary memory
            FreePagingPage (pPool, pPagingMem);

        } else {
            retval = PAGEIN_RETRY;
        }
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
    const openexe_t *oeptr,
    const e32_lite  *eptr,
    const o32_lite  *optr // Array of O32's from the module
    )
{
    BOOL fRet = TRUE;

    if (PageAble (oeptr)) {
        
        const o32_lite *optrEnd = optr + eptr->e32_objcnt;

        for ( ; optr < optrEnd; optr ++) {

            BOOL fUsePool = SectionCanBePagedOut (oeptr, optr)              // r/o non-xip pages
                         || ((pprc == g_pprcNK)                             // paging out a module
                             && !IsKModeAddr (optr->o32_realaddr)           // user mode DLL
                             && (optr->o32_flags & IMAGE_SCN_MEM_WRITE)     // read/write, not-shared, pageable
                             && !(optr->o32_flags & (IMAGE_SCN_MEM_SHARED|IMAGE_SCN_MEM_NOT_PAGED)));
            
            // Return the pages to the loader page pool
            if (fUsePool
                && !VMDecommit (pprc, (LPVOID) optr->o32_realaddr, optr->o32_vsize, VM_PAGER_LOADER | VM_PAGER_POOL)) {
                fRet = FALSE;

                // decommitting the last instance of a DLL should've never failed.
                DEBUGCHK (pprc != g_pprcNK);
            }
        }
    }

    return fRet;
}

static BOOL DecommitModuleInProcess (PPROCESS pprc, PMODULE pMod)
{
    const o32_lite *optr = pMod->o32_ptr;
    const o32_lite *optrEnd = optr + pMod->e32.e32_objcnt;
    
    DEBUGCHK (pprc != g_pprcNK);
    DEBUGCHK (PageAble (&pMod->oe));
        
    for ( ; optr < optrEnd; optr ++) {

        if (SectionCanBePagedOut (&pMod->oe, optr)
            && !VMDecommit (pprc, (LPVOID) optr->o32_realaddr, optr->o32_vsize, VM_PAGER_LOADER)) {
            // decommit failed
            break;
        }
    }
    return optr == optrEnd;
}


//------------------------------------------------------------------------------
// Worker function for DecommitModule.
// Use with EnumerateDList on NK process list
//------------------------------------------------------------------------------
static BOOL
EnumDecommitModule (
    PDLIST pItem,       // Pointer to process
    LPVOID pEnumData    // Pointer to module
    )
{
    PPROCESS pprc = (PPROCESS) pItem;
    PMODULE  pMod = (PMODULE) pEnumData;
    PDLIST   ptrav;
    BOOL fRet = FALSE; // by default, continue the enumeration

    DEBUGCHK (OwnModListLock ());  // Safely enumerate modules
    DEBUGCHK (pprc != g_pprcNK);
    DEBUGCHK (PageAble (&pMod->oe));

    // Find the module inside this process.  Don't want to use FindModInProc
    // because that affects the list LRU order.  The module's already locked so
    // we just need to check if it's present.
    for (ptrav = pprc->modList.pFwd; ptrav != &pprc->modList; ptrav = ptrav->pFwd) {
        if (((PMODULELIST) ptrav)->pMod == pMod) {
            // stop the enumeration if debugger is attached OR
            // decommit fails.
            fRet = IsProcessBeingDebugged(pprc)
                || !DecommitModuleInProcess (pprc, pMod);
            break;
        }
    }

    return fRet;
}

//------------------------------------------------------------------------------
// Decommit pages for a DLL, from all processes.  The module should already be
// locked.
//------------------------------------------------------------------------------
static BOOL
DecommitModule (
    PMODULE pMod
    )
{
    BOOL fRet = FALSE;
    
    if (PageAble (&pMod->oe)) {
        // Don't want page-in to occur between paging out in all procs.  When
        // we get to NK, there should be no pages left in the other procs.
        // Lock modlist for EnumDecommitModule, in proper order with PagerCS.
        DEBUGCHK (OwnModListLock ());
        EnterCriticalSection (&PagerCS);

        // First decommit in all processes except NK
        if (IsKModeAddr ((DWORD)pMod->BasePtr)                              // kernel DLL - no need to enumerate processes
            || !EnumerateDList (&g_pprcNK->prclist, EnumDecommitModule, pMod)) {   // user DLL - enumerate processes

            // If the module is paged out of all processes in the system,
            // then decommit the NK pages and return them to the pool
            if (DecommitExe (g_pprcNK, &pMod->oe, &pMod->e32, pMod->o32_ptr)) {
                // mark the module as paged out
                pMod->wFlags &= ~MF_PAGED_IN;
                fRet = TRUE;
            }
        }

        LeaveCriticalSection (&PagerCS);
    }

    return fRet;
}

//------------------------------------------------------------------------------
// Part of DoPageOut.  Page out DLL pages until enough memory is released.
//------------------------------------------------------------------------------
void
PageOutModules (
    BOOL fCritical
    )
{
    //
    // trim directions through the MRU list
    //
    enum {
        TRIM_USR = 0, // user side dlls
        TRIM_KRN // kernel side dlls
    };

    PDLIST pusrtrav, pkrntrav;
    static int s_TrimDirection = TRIM_USR;
    const PDLIST pHead = &g_pprcNK->modList;

    DEBUGMSG(ZONE_PAGING,(L"POM: Starting page free count: %d\r\n", PageFreeCount));

    DEBUGCHK (OwnCS (&PageOutCS));

    LockModuleList ();      // So that we can enumerate processes & NK modules, and safely track a start position

    //
    // starting positions for LRU user/kernel mode dlls
    //
    pusrtrav = ((PDLIST)g_pModKernDll)->pBack;
    pkrntrav = ((PDLIST)g_pModKernDll)->pFwd;

    //
    // Continue trim until we have:
    // a) enough pages out of the pool OR
    // b) we have scanned all the user and kernel dlls to pageout
    //
    while (PGPOOLNeedsTrim (&g_LoaderPool)) { 

        PMODULE pTmp, pMod = NULL;

        // scan user mode dlls
        if (TRIM_USR == s_TrimDirection) {
            while (pHead != pusrtrav) {
                pTmp = (PMODULE)pusrtrav;
                pusrtrav = pusrtrav->pBack; // advance always
                if (pTmp->wFlags & MF_PAGED_IN) {
                    pMod = pTmp;
                    break;
                }
            }

            // if kernel side has not exhausted, switch to
            // kernel side next time in the page out loop
            if  (pHead != pkrntrav) {
                s_TrimDirection = TRIM_KRN;
            }

        } else {

            // scan kernel mode dlls
            while (pHead != pkrntrav) {
                pTmp = (PMODULE)pkrntrav;
                pkrntrav = pkrntrav->pFwd; // advance always
                if (pTmp->wFlags & MF_PAGED_IN) {
                    pMod = pTmp;
                    break;
                }
            }

            // if user side has not exhausted, switch to
            // user side next time in the page out loop            
            if (pHead != pusrtrav) {
                s_TrimDirection = TRIM_USR;
            }
        }

        // if both usr and krn list are exhausted, break
        if (pkrntrav == pusrtrav) {
            DEBUGCHK (pHead == pkrntrav);
            DEBUGMSG (ZONE_PAGING, (L"PGPOOL POM: Nothing to page out! free:%d  free+pool:%d\r\n", PageFreeCount, PageFreeCount_Pool));
            break;
        }
        
        if (pMod) {
            if (DecommitModule (pMod)) {
                DEBUGMSG (ZONE_PAGING, (L"PGPOOL POM: Completely paged out %s\r\n", pMod->lpszModName));
            }
        }
    }

    UnlockModuleList ();

    DEBUGMSG(ZONE_PAGING,(L"POM: Ending page free count: %d\r\n", PageFreeCount));
}


//------------------------------------------------------------------------------
// Part of DoPageOut.  Page out EXE pages (except NK) until enough memory is released.
//------------------------------------------------------------------------------
void
PageOutProcesses (
    BOOL fCritical // unused
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
    while (pprc && PGPOOLNeedsTrim (&g_LoaderPool)) {

        // Page out the process
        if (IsProcessNormal(pprc) && !IsProcessBeingDebugged(pprc) && pprc->pVaList) {
            // if process is fully initialized and not exiting and not being debugged
            if (DecommitExe (pprc, &pprc->oe, &pprc->e32, pprc->o32_ptr)) {
                DEBUGMSG (ZONE_PAGING, (L"PGPOOL POP: Completely paged out %s\r\n", pprc->lpszProcName));
            }
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

            DecommitModule (pMod);

        } else {
            dwErr = ERROR_INVALID_HANDLE;
        }
        UnlockModuleList ();

    } else if (IsProcessBeingDebugged (pprc)) {
        // process is under app debugger - disallow paging
        dwErr = ERROR_INVALID_HANDLE;

    } else {
        // paging out a process
        DecommitExe (pprc, &pprc->oe, &pprc->e32, pprc->o32_ptr);

        // pageout dependent modules based on flags
        if (PAGE_OUT_PROCESS_ONLY != dwFlags) {
            LockModuleList ();
            EnumerateDList (&pprc->modList, DecommitDependency, (LPVOID) dwFlags);
            UnlockModuleList ();
        }
    }

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


#define STACK_BUFFER_PAGES  128

DWORD ReadSection (const openexe_t *oeptr, const o32_lite *optr, DWORD idxOptr)
{
    DWORD ftype     = oeptr->filetype;
    LPBYTE realaddr = (LPBYTE) optr->o32_realaddr;
    PPROCESS pprc   = IsKModeAddr ((DWORD) realaddr)? g_pprcNK : pActvProc;
    ulong cbRead;

    // take care of R/O, uncompressed XIP section
    if ((FA_XIP & ftype)                                                            // XIP-able?
        && !(optr->o32_flags & (IMAGE_SCN_COMPRESSED | IMAGE_SCN_MEM_WRITE))) {     // not (compressed or writeable)

        DWORD dwErr = 0;
        
        // R/O, uncompressed
        DEBUGCHK (optr->o32_vsize <= optr->o32_psize);
        if (FA_DIRECTROM & ftype) {
            // in 1st-XIP. VirtualCopy directly
            DEBUGMSG(ZONE_LOADER1,(TEXT("virtualcopying %8.8lx <- %8.8lx (%lx)!\r\n"),
                    realaddr, optr->o32_dataptr, optr->o32_vsize));

            if (!VMFastCopy (pprc, (DWORD) realaddr, g_pprcNK, optr->o32_dataptr, optr->o32_vsize, optr->o32_access)) {
                dwErr = ERROR_OUTOFMEMORY;
            }
        } else {
            // in external XIP-able media, call to filesys
            // allocate spaces for the pages
            DWORD cPages = PAGECOUNT (optr->o32_vsize);
            DWORD _Pages[STACK_BUFFER_PAGES];
            LPDWORD pPages = _Pages;

            if (cPages > STACK_BUFFER_PAGES) {
                pPages = (LPDWORD) NKmalloc (cPages * sizeof (DWORD));
                if (!pPages) {
                    dwErr = ERROR_OUTOFMEMORY;
                }
            }

            if (pPages) {
                // call to filesys to get the pages
                if (!FSIoctl (oeptr->hf, IOCTL_BIN_GET_XIP_PAGES, &idxOptr, sizeof(DWORD), pPages, cPages * sizeof(DWORD), NULL, NULL)) {
                    dwErr = ERROR_BAD_EXE_FORMAT;

                // update VM
                } else if (!VMSetPages (pprc, (DWORD) realaddr, pPages, cPages, optr->o32_access)) {
                    dwErr = ERROR_OUTOFMEMORY;
                }

                if (_Pages != pPages) {
                    NKfree (pPages);
                }
            }
        }
        return dwErr;
    }

    // do nothing if pageable
    if (PageAble (oeptr)                                    // file pageable
        && !(IMAGE_SCN_MEM_NOT_PAGED & optr->o32_flags)) {  // section pageable

        return 0;
    }

    // commit memory
    if (!VMCommit (pprc, realaddr, optr->o32_vsize, PAGE_EXECUTE_READWRITE, PM_PT_ZEROED))
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

        //read the correct size from the binfs
        //NOTE- uncompressed sections: psize > vsize except for Writable section where psize < vsize.   (psize is the real file size)
        //      compressed sections: psize (at the loader) == vsize; although the psize (actual) < vsize;  (vsize is the real file size)
        DWORD cbToRead = min(optr->o32_psize, optr->o32_vsize);

        if (!FSReadFileWithSeek (oeptr->hf, realaddr, cbToRead, &cbRead, NULL, (DWORD) optr->o32_dataptr, 0)
            || (cbRead != cbToRead)) {
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
    PPROCESS    pprc,
    const BYTE *pBaseAddr,
    openexe_t  *oeptr,
    e32_lite   *eptr,
    o32_lite   *oarry
    )
{
    int   loop;
    BOOL  fNoPaging = !PageAble (oeptr);
    BOOL  fSectionNoPaging;
    DWORD dwIatRva = 0;
    BOOL  fUModeDll = (pprc == g_pprcNK) && !IsKModeAddr ((DWORD) pBaseAddr);

    if (!(oeptr->filetype & FA_XIP) && eptr->e32_unit[IMP].size && !eptr->e32_sect14rva) {
        const struct ImpHdr *pImpInfo = (const struct ImpHdr *) (pBaseAddr + eptr->e32_unit[IMP].rva);
        DEBUGCHK(eptr->e32_unit[IMP].rva);
        VERIFY (CeSafeCopyMemory (&dwIatRva, &pImpInfo->imp_address, sizeof (DWORD)));
    }

    for (loop = 0; loop < eptr->e32_objcnt; loop++, oarry ++) {

        fSectionNoPaging = fNoPaging || (IMAGE_SCN_MEM_NOT_PAGED & oarry->o32_flags);
        //
        // when linked with -optidata, IAT can be read-only, need to change it to read/write
        //
        if (IsIATInROSection (oarry, dwIatRva)) {
            DEBUGMSG (ZONE_LOADER1, (L"Import Table in R/O section, changed to R/W\r\n"));
            DEBUGCHK (!(oeptr->filetype & FA_XIP));

            oarry->o32_flags |= IMAGE_SCN_MEM_WRITE|IMAGE_SCN_MEM_SHARED;   // change it to R/W, shared
            oarry->o32_access = AccessFromFlags (oarry->o32_flags);

            if (!fSectionNoPaging && !fUModeDll) {

                // pageable and not a user mode DLL - decommit the section such that it'll be paged in later as r/w
                // NOTE: We don't need to do it for user mode DLL as this is for the master copy in kernel and should only
                //       be paged in with r/o protection, regradless the section flag.
                VMDecommit (pprc, (LPVOID)oarry->o32_realaddr,oarry->o32_vsize,VM_PAGER_LOADER);
            }
        }

        if (fSectionNoPaging) {
            if (oarry->o32_flags & IMAGE_SCN_MEM_DISCARDABLE) {
                VMDecommit (pprc, (LPVOID)oarry->o32_realaddr,oarry->o32_vsize,VM_PAGER_NONE);
                oarry->o32_realaddr = 0;
            } else {
                ULONG access = oarry->o32_access;

                if (fUModeDll) {
                    // for user mode DLL, the master copy is always read-only.
                    access = (VM_EXECUTABLE_PROT & access)? PAGE_EXECUTE_READ : PAGE_READONLY;
                }
                    
                VMProtect (pprc, (LPVOID)oarry->o32_realaddr,oarry->o32_vsize, access, NULL);
            }
        }
    }

    // set the eptr->e32_unit[FIX] to 0 so we won't relocate anymore
    if (fNoPaging) {
        eptr->e32_unit[FIX].size = 0;
    }

}

DWORD ReadO32Info (
        openexe_t *oeptr,
        e32_lite  *eptr,
        o32_lite  *oarry,
        LPDWORD   pdwFlags
        )
{
    DWORD       dwErr = 0;
    DWORD       filetype = oeptr->filetype;

    if (    (FA_PREFIXUP  & filetype)
        && !(FA_DIRECTROM & filetype)
        && !ReadExtImageInfo (oeptr->hf, IOCTL_BIN_GET_O32, eptr->e32_objcnt * sizeof(o32_lite), oarry)) {

        dwErr = ERROR_BAD_EXE_FORMAT;

    } else {

        o32_lite    *optr;
        DWORD       dwOfst = 0;
        DWORD       nNonPageable = 0;
        DWORD       dwEndAddr = 0;
        BOOL        fRelocFound = FALSE;

        for (optr = oarry; optr < oarry + eptr->e32_objcnt; optr++) {

            DEBUGMSG(ZONE_LOADER2,(TEXT("ReadO32Info : get section information %d\r\n"), optr - oarry));

            if (FA_PREFIXUP & filetype) {
                // in module section

                // 1st XIP, grab the o32 information from ROM, otherwise it's already read at the beginning of the function
                if (FA_DIRECTROM & filetype) {
                    o32_rom *o32rp = (o32_rom *)(oeptr->tocptr->ulO32Offset+dwOfst);
                    DEBUGMSG(ZONE_LOADER1,(TEXT("(2) o32rp->o32_realaddr = %8.8lx\r\n"), o32rp->o32_realaddr));
                    optr->o32_realaddr  = o32rp->o32_realaddr;
                    optr->o32_vsize     = o32rp->o32_vsize;
                    optr->o32_psize     = o32rp->o32_psize;
                    optr->o32_rva       = o32rp->o32_rva;
                    optr->o32_flags     = o32rp->o32_flags;
                    optr->o32_dataptr   = o32rp->o32_dataptr;
                    dwOfst             += sizeof (o32_rom);

                    // find the last RO section for kernel module (realaddr == dataptr)
                    if ((optr->o32_dataptr == optr->o32_realaddr)
                        && (optr->o32_realaddr > dwEndAddr)) {
                        dwEndAddr = optr->o32_realaddr + optr->o32_vsize;
                    }

                } else {
                    optr->o32_realaddr = eptr->e32_vbase + optr->o32_rva;
                }

                // only honor "do not load" if it's XIP
                if ((optr->o32_flags & IMAGE_SCN_TYPE_NO_LOAD) && (FA_XIP & filetype)) {
                    // "do not load" seciton is always at the end of the o32s.
                    // update eptr->e32_objcnt and break the loop.
                    eptr->e32_objcnt = (USHORT) (optr - oarry);
                    break;
                }


            } else {
                // in file section
                o32_obj o32;

                // read O32 header from file
                if (!ReadO32FromFile (oeptr, oeptr->offset+sizeof(e32_exe)+dwOfst, &o32)) {
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
                optr->o32_realaddr  = eptr->e32_vbase + o32.o32_rva;
                optr->o32_vsize     = o32.o32_vsize;
                optr->o32_rva       = o32.o32_rva;
                optr->o32_flags     = o32.o32_flags;
                optr->o32_psize     = o32.o32_psize;
                optr->o32_dataptr   = o32.o32_dataptr;
                dwOfst             += sizeof (o32_obj);
                DEBUGMSG(ZONE_LOADER1,(TEXT("(1) optr->o32_realaddr = %8.8lx\r\n"), optr->o32_realaddr));

                if (optr->o32_rva == eptr->e32_unit[RES].rva) {
                    optr->o32_flags &= ~IMAGE_SCN_MEM_DISCARDABLE;

                    // BC: any pre-6.0 apps has resource section R/W
                    if (eptr->e32_cevermajor < 6) {
                        optr->o32_flags |= IMAGE_SCN_MEM_WRITE;
                    }
                }

            }

            if (IMAGE_SCN_MEM_NOT_PAGED & optr->o32_flags) {
                nNonPageable ++;
            }

            optr->o32_access = AccessFromFlags (optr->o32_flags);

            if (optr->o32_rva == eptr->e32_unit[FIX].rva) {
                fRelocFound = TRUE;
            }

        }

        if (!dwErr) {

            if (dwEndAddr
                && (FA_DIRECTROM & filetype)
                && IsKernelVa ((LPCVOID) eptr->e32_vbase)) {
                // page align the end addr
                dwEndAddr = PAGEALIGN_UP (dwEndAddr);
                eptr->e32_vsize = dwEndAddr - eptr->e32_vbase;
                DEBUGMSG (ZONE_LOADER1, (L"Updated eptr->e32_vsize to = %8.8lx\r\n", eptr->e32_vsize));
            }

            if (PagingSupported (oeptr)) {
                if (nNonPageable == eptr->e32_objcnt) {
                    // all sections are not pageable, mark the file not-pageable
                    oeptr->pagemode = PM_NOPAGING;
                } else if (nNonPageable) {
                    oeptr->pagemode = PM_PARTPAGING;
                }
            }
            if ((FA_XIP & filetype)  // XIP - cannot relocate
                || (eptr->e32_unit[FIX].size && !fRelocFound)) {
                // reloc is stripped, can't relocate
                eptr->e32_unit[FIX].size = 0;
                *pdwFlags |= IMAGE_FILE_RELOCS_STRIPPED;
            }
        }

    }

    return dwErr;
}

//------------------------------------------------------------------------------
// Load all O32 headers
//------------------------------------------------------------------------------
DWORD
LoadO32(
    const openexe_t *oeptr,
    e32_lite *eptr,
    o32_lite *oarry,
    ulong BasePtr
    )
{
    DWORD dwErr   = 0;

    DEBUGCHK ((pActvProc == g_pprcNK) || ((DWORD) BasePtr < VM_DLL_BASE));

    if (!IsKernelVa ((LPVOID) BasePtr)) {

        // regular module
        DWORD dwOfst            = BasePtr - eptr->e32_vbase;
        const o32_lite *optrEnd = oarry + eptr->e32_objcnt;
        o32_lite       *optr;

        // update optr->o32_realaddr if needed
        if (dwOfst) {
            for (optr = oarry; optr < optrEnd; optr++) {
                optr->o32_realaddr += dwOfst;
            }
        }

        if (!FullyPageAble (oeptr) || (FA_XIP & oeptr->filetype)) {        // not fully pageable, or XIP

            for (optr = oarry; optr < optrEnd; optr++) {
                dwErr = ReadSection (oeptr, optr, optr - oarry);
                if (dwErr) {
                    break;
                }
            }
        }
    }
    DEBUGMSG (dwErr && ZONE_LOADER1, (L"LoadO32 Failed, dwErr = %8.8lx\n", dwErr));

    return dwErr;
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
    PSTUBTHREAD pStubThread = NULL;

    DEBUGCHK (OwnLoaderLock (pprc));

    eptr = &pprc->e32;

    dwErr = LoadE32 (&pprc->oe, eptr, &flags, &entry, 0);
    if (dwErr) {
        DEBUGMSG(1,(TEXT("ERROR! LoadE32 failed, dwErr = %8.8lx!\r\n"), dwErr));
        return dwErr;
    }

    if (IMAGE_DLLCHARACTERISTICS_NX_COMPAT & HIWORD (flags)) {
        pprc->fFlags |= MF_DEP_COMPACT;
    }

    if (flags & IMAGE_FILE_DLL) {
        DEBUGMSG(1,(TEXT("ERROR! CreateProcess on DLL!\r\n")));
        return  ERROR_BAD_EXE_FORMAT;
    }

    // allocate memory for both name and the o32 objects in one piece
    len = (NKwcslen (procname) + 1) * sizeof(WCHAR);

    // +2 for DWORD alignment
    pAlloc = AllocName (eptr->e32_objcnt * sizeof(o32_lite) + len + 2);
    if (!pAlloc) {
        return ERROR_OUTOFMEMORY;
    }

    DEBUGCHK (IsDwordAligned(pAlloc));

    pprc->o32_ptr = (o32_lite *) ((DWORD) pAlloc + 4);  // skip header info

    // update process name
    uptr = (LPWSTR) (pprc->o32_ptr + eptr->e32_objcnt);
    memcpy (uptr, procname, len);
    pprc->lpszProcName = uptr;

    dwErr = ReadO32Info (&pprc->oe, eptr, pprc->o32_ptr, &flags);
    if (dwErr) {
        return dwErr;
    }

    if ((flags & IMAGE_FILE_RELOCS_STRIPPED) && ((eptr->e32_vbase < 0x10000) || (eptr->e32_vbase > 0x4000000))) {
        DEBUGMSG(1,(TEXT("ERROR! No relocations - can't load/fixup!\r\n")));
        return ERROR_BAD_EXE_FORMAT;
    }

    pprc->BasePtr = VMAlloc (pprc,
        (flags & IMAGE_FILE_RELOCS_STRIPPED)? (LPVOID) eptr->e32_vbase : NULL,
        eptr->e32_vsize,
        MEM_RESERVE|MEM_IMAGE,
        PAGE_NOACCESS);

    if (!pprc->BasePtr) {
        return ERROR_OUTOFMEMORY;
    }

    DEBUGMSG(ZONE_LOADER1,(TEXT("BasePtr is %8.8lx\r\n"),pprc->BasePtr));

    DEBUGMSG (ZONE_LOADER1 && (pprc->oe.filetype & FA_PREFIXUP) && ((DWORD) pprc->BasePtr != eptr->e32_vbase),
        (L"Reloate '%s' in MODULE section from 0x%8.8lx to 0x%8.8lx\r\n", procname, eptr->e32_vbase, pprc->BasePtr));

    // optimization for resoure loading - rebase resource rva
    if (eptr->e32_unit[RES].rva) {
        eptr->e32_unit[RES].rva += (DWORD) pprc->BasePtr;
    }

    dwErr = LoadO32(&pprc->oe,eptr,pprc->o32_ptr,(ulong)pprc->BasePtr);
    if (dwErr) {
        DEBUGMSG(1,(TEXT("ERROR! LoadO32 failed, dwerr = %8.8lx!\r\n"), dwErr));
        return dwErr;
    }

    //
    // relocated if needed
    //
    if ((eptr->e32_vbase != (DWORD) pprc->BasePtr)
        && !RelocateNonPageAbleSections (&pprc->oe, eptr, pprc->o32_ptr, (DWORD) pprc->BasePtr)) {
        return ERROR_OUTOFMEMORY;
    }
#if 0
    // allocate per-process run queue for BC
    if (RunOneThreadOnly (eptr)) {
        pStubThread = (PSTUBTHREAD) AllocMem (HEAP_STUBTHREAD);

        if (!pStubThread) {
            return ERROR_OUTOFMEMORY;
        }
        memset (pStubThread, 0, sizeof (STUBTHREAD));
        ((PTHREAD) pStubThread)->pprcOwner = pprc;
        pStubThread->pRunQ = &RunList;
    }
#endif
    // allocate stack
    eptr->e32_stackmax = CalcStackSize(eptr->e32_stackmax);
    lpStack = VMCreateStack (pprc, eptr->e32_stackmax);
    if (!lpStack) {
        return ERROR_OUTOFMEMORY;
    }

    pth->tlsNonSecure = TLSPTR (lpStack, eptr->e32_stackmax);

    pCmdLine = lppsi->pszCmdLine? lppsi->pszCmdLine : L"";

    DEBUGMSG (ZONE_LOADER1, (L"Initializeing user stack cmdline = '%s', program = '%s', stack = %8.8lx\r\n",
        pCmdLine, pprc->lpszProcName, lpStack));

    // Upon return of InitCmdLineAndPrgName, pCmdLine will be pointing to user accessible command line, and
    // pCurSP is the user accessible program name. pCurSP is also the top of stack inuse
    pCurSP = InitCmdLineAndPrgName (&pCmdLine, pprc->lpszProcName, lpStack, eptr->e32_stackmax);

    if (!InitModInProc (pprc, (PMODULE) hCoreDll)) {
        return ERROR_OUTOFMEMORY;
    }

    LockLoader (g_pprcNK);
    // load MUI if it exist
    pprc->pmodResource = LoadMUI (NULL, pprc->BasePtr, eptr);
    UnlockLoader (g_pprcNK);

    AdjustRegions (pprc, pprc->BasePtr, &pprc->oe, eptr, pprc->o32_ptr);

    pth->dwStartAddr = (DWORD) pprc->BasePtr + entry;

    DEBUGMSG(ZONE_LOADER1,(TEXT("Starting up the process!!! IP = %8.8lx\r\n"), pth->dwStartAddr));

    UnlockLoader (pprc);

    (* HDModLoad) ((DWORD)pprc, TRUE);

    // notify the application debugger if needed
    if (pprc->pDbgrThrd) {
        DbgrNotifyDllLoad (pth, pprc, (PMODULE) hCoreDll);
    }

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
    NKCacheRangeFlush (NULL, 0, CACHE_SYNC_WRITEBACK|CACHE_SYNC_INSTRUCTIONS|CSF_LOADER);

    //
    // Always suspend ourself before signaling the event, such that the creator can resume us
    // if needed.
    //
    // It's safe to modify bPendSusp here for no-one, but kernel has a handle to this thread.
    // and we know kernel will not suspend this thread for sure.
    //

    InterlockedExchangeAdd (&g_cpMinPageProcCreate, +4); // assume min 4 pages for process

    pth->bPendSusp = 1;

    // update process state
    pprc->bState = PROCESS_STATE_NORMAL;

    // notify PSL of process creation. Do this after marking the process state
    // as normal so PSL servers can query some process data when they get 
    // notified
    NKPSLNotify (DLL_PROCESS_ATTACH, pprc->dwId, pth->dwId);

    // notify caller process creation complete
    ForceEventModify (GetEventPtr (pprc->phdProcEvt), EVENT_SET);

    // creator signaled, we can suspend here if needed
    if (pth->bPendSusp) {
        SCHL_SuspendSelfIfNeeded ();
        CLEAR_USERBLOCK (pth);
    }
    
    InterlockedExchangeAdd (&g_cpMinPageProcCreate, -4);

    // in case we got terminated even before running user code, just go straight to NKExitThread
    if (GET_DYING (pth)) {
        if (pStubThread) {
            FreeMem (pStubThread, HEAP_STUBTHREAD);
        }
        NKExitThread (0);
        // never return
        DEBUGCHK (0);
    }

    if (RunOnCpu0Only (eptr)) {
        pprc->dwAffinity = 1;
        SCHL_SetThreadAffinity (pth, 1);

    } else if (pStubThread) {
        AcquireSchedulerLock (0);
        SET_RUNSTATE (pStubThread, RUNSTATE_RUNNING);
        pprc->pStubThread = pStubThread;
        pth->pRunQ = &pStubThread->ProcReadyQ;
        ReleaseSchedulerLock (0);
    }

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
    PTHREAD pth = pCurThread;
    LPProcStartInfo lppsi = (LPProcStartInfo)param;
    DWORD dwErr = 0;
    DWORD dwFlags = LDROE_ALLOW_PAGING;

    DEBUGMSG(ZONE_LOADER1,(TEXT("CreateNewProc %s\r\n"),lppsi->pszImageName));

    // process creation time
    pprc->ftCreate = pth->ftCreate;

    // new process is accessible after this line
    LockModuleList ();
    AddToDListTail (&g_pprcNK->prclist, &pprc->prclist);
    UnlockModuleList ();

    // set debug flags
    if (lppsi->fdwCreate & (DEBUG_PROCESS|DEBUG_ONLY_THIS_PROCESS)) {
        dwFlags |= LDROE_OPENDEBUG;
    }

    // initialize rest of the process code/data
    if (!dwErr) {
        dwErr = OpenExecutable (NULL, lppsi->pszImageName, &pprc->oe, dwFlags);
    }

    if (!dwErr) {

        if (!ChkDebug (&pprc->oe)) {
            dwErr = (DWORD) NTE_BAD_SIGNATURE;

        } else {

            DEBUGMSG(ZONE_LOADER1,(TEXT("image name = '%s'\r\n"), lppsi->pszImageName));

            LockLoader (pprc);
            dwErr = CreateNewProcHelper (lppsi);
            UnlockLoader (pprc);
            // fail to create process if the helper returns
        }
    }


    // fail to create the process, update error code and release the resources
    lppsi->dwErr = dwErr;

    DEBUGMSG(ZONE_LOADER1,(TEXT("CreateNewProc failure on %s!, dwErr = %8.8lx\r\n"),lppsi->pszImageName, dwErr));
    ForceEventModify (GetEventPtr (pprc->phdProcEvt), EVENT_SET);

    NKExitThread (0);
    DEBUGCHK(0);
    // Never returns
}

static DWORD CommitAndCopySection (PPROCESS pprc, const o32_lite* optr)
{
    LPVOID addr = (LPVOID) optr->o32_realaddr;

    DEBUGMSG(ZONE_LOADER1,(TEXT("CommitAndCopySection: Copying section at %8.8lx\n"), addr));

    return VMCommit (pprc, addr, optr->o32_vsize, PAGE_EXECUTE_READWRITE, PM_PT_ZEROED)
         ? VMReadProcessMemory (g_pprcNK, addr, addr, optr->o32_vsize)
         : ERROR_OUTOFMEMORY;
}

static DWORD VirtualCopySection (PPROCESS pprc, const o32_lite *optr)
{
    DWORD addr  = optr->o32_realaddr;

    DEBUGMSG(ZONE_LOADER1,(TEXT("VirtualCopySection: Virtual-Copying section at %8.8lx\n"), addr));

    return VMCopyCommittedPages (pprc, addr, optr->o32_vsize, optr->o32_access)
         ? 0
         : ERROR_OUTOFMEMORY;
}

//------------------------------------------------------------------------------
// Copy DLL regions from NK to current process
//------------------------------------------------------------------------------
DWORD CopyRegions (PMODULE pMod)
{
    DWORD               dwErr           = 0;
    PPROCESS            pprc            = pActvProc;
    BOOL                fModuleNoPaging = !PageAble (&pMod->oe);
    const o32_lite     *optr            = pMod->o32_ptr;
    const o32_lite     *optrEnd         = optr + pMod->e32.e32_objcnt;

    for ( ; !dwErr && (optr < optrEnd); optr ++) {
        if (!(optr->o32_flags & IMAGE_SCN_MEM_DISCARDABLE)) {        
            if ((optr->o32_flags & IMAGE_SCN_MEM_SHARED) || !(optr->o32_flags & IMAGE_SCN_MEM_WRITE)) {
                // r/o or shared, VirtualCopy
                dwErr = VirtualCopySection (pprc, optr);
            } else {
                // r/w, and not shared. commit only if not pageable
                if (fModuleNoPaging || (optr->o32_flags & IMAGE_SCN_MEM_NOT_PAGED)) {
                    dwErr = CommitAndCopySection (pprc, optr);
                }
            }
        }
    }

    DEBUGMSG (dwErr, (L"!!ERROR: CopyRegion failed for section @%8.8lx, dwErr = %8.8lx\r\n", optr->o32_realaddr, dwErr));

    return dwErr;
}

static DWORD TryOpenDll (PPROCESS pprc, PMODULE pMod, LPWSTR pszName, DWORD dwMaxLen, DWORD dwCurLen, DWORD dwFlags, BOOL fTryAppend)
{
    DWORD dwErr = ERROR_FILE_NOT_FOUND;

    // try the name with suffix 1st
    if (fTryAppend && (dwCurLen < dwMaxLen - 4)) {
        memcpy (pszName+dwCurLen, L".dll", 10);        // 10 == sizeof (L".dll")
        DEBUGMSG (ZONE_LOADER1, (L"Trying DLL '%s'\r\n", pszName));
        dwErr = OpenExecutable (pprc, pszName, &pMod->oe, dwFlags | LDROE_OPENDLL);
    }

    // try the path name without suffix if needed
    if (ERROR_FILE_NOT_FOUND == dwErr) {
        pszName[dwCurLen] = 0;  // get rid of the ".dll" we just appended
        DEBUGMSG (ZONE_LOADER1, (L"Trying DLL '%s'\r\n", pszName));
        dwErr = OpenExecutable (pprc, pszName, &pMod->oe, dwFlags | LDROE_OPENDLL);
    }

    return dwErr;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD InitModule (PPROCESS pprc, PMODULE pMod, LPCWSTR lpszFileName, LPCWSTR lpszDllName, DWORD fLbFlags, WORD wFlags)
{
    e32_lite    *eptr;
    PNAME       pAlloc = 0;
    DWORD       entry, dwErr = ERROR_FILE_NOT_FOUND, len, loop, flags;
    WCHAR       szName[MAX_PATH] = L"\\windows\\k.";
    DWORD       dwFlags = wFlags | ((fLbFlags & LLIB_NO_PAGING)? 0 : LDROE_ALLOW_PAGING);
    DWORD       filetype;

    BOOL        fTryAppend;
    LPCWSTR     pszSuffix;
    extern BOOL g_fAslrEnabled;

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
            len = CopyPath (&szName[SYSTEMDIRLEN+2], lpszDllName, MAX_PATH - (SYSTEMDIRLEN+2));

            if (len) {
                BY_HANDLE_FILE_INFORMATION bhfi;
                dwErr = TryOpenDll (pprc, pMod, szName, MAX_PATH, len+SYSTEMDIRLEN+2, dwFlags, fTryAppend);
                if (!dwErr
                    && !(FA_PREFIXUP & pMod->oe.filetype)
                    && !IsInDbgList (&szName[SYSTEMDIRLEN])
                    && (!FSGetFileInfo (pMod->oe.hf, &bhfi)
                            || !(FILE_ATTRIBUTE_INROM & bhfi.dwFileAttributes))) {
                    // Found and opened a "k." version of the requested DLL, but it is
                    // not in ROM so ignore it, close it, and continue the search.
                    FreeExeName(&pMod->oe);
                    CloseExe(&pMod->oe);
                    dwErr = ERROR_FILE_NOT_FOUND;
                }
            }
        }
    }

    if (ERROR_FILE_NOT_FOUND == dwErr) {
        len = CopyPath (szName, lpszFileName, MAX_PATH);

        if (!len) {
            dwErr = ERROR_BAD_PATHNAME;
        } else {
            dwErr = TryOpenDll (pprc, pMod, szName, MAX_PATH, len, dwFlags, fTryAppend);
        }
    }

    if (!dwErr && !ChkDebug (&pMod->oe)) {
        dwErr = (DWORD) NTE_BAD_SIGNATURE;
    }

    if (dwErr) {
        return (ERROR_FILE_NOT_FOUND == dwErr)? ERROR_MOD_NOT_FOUND : dwErr;
    }

    lpszDllName = GetPlainName (szName);

    // load O32 info for the module
    eptr = &pMod->e32;
    dwErr = LoadE32 (&pMod->oe, eptr, &flags, &entry, wFlags & LOAD_LIBRARY_AS_DATAFILE);
    if (dwErr) {
        return dwErr;
    }

    if (IMAGE_DLLCHARACTERISTICS_NX_COMPAT & HIWORD (flags)) {
        pMod->wFlags |= MF_DEP_COMPACT;
    } else if ((NX_SUPPORT_NONE != g_dwNXFlags) && (LOAD_LIBRARY_IN_KERNEL & wFlags)) {
        // DLL not NX compitable is not allowed to be loaded into kernel if NX is supported
        DEBUGMSG (ZONE_ERROR, (L"InitModule: Failed Loading DLL '%s', which is not NX compatible, into kernel\r\n", lpszDllName));
        return ERROR_BAD_EXE_FORMAT;
    }

    if (!(wFlags & LOAD_LIBRARY_AS_DATAFILE) && !(flags & IMAGE_FILE_DLL)) {
        // trying to load an exe via LoadLibrary call; fail the call
        DEBUGMSG(1,(TEXT("ERROR! LoadLibrary on EXE!\r\n")));
        return  ERROR_BAD_EXE_FORMAT;
    }

    if (IsKernelVa((LPCVOID)eptr->e32_vbase) && eptr->e32_unit[RES].rva) {
        // cannot support resources in Z modules, but MUI will still work
        RETAILMSG(1, (L"InitModule: WARNING resource section of Z flag DLL '%s' will be inaccessible; MUI resource DLL will be used if present", lpszDllName));
        eptr->e32_unit[RES].size = 0;
    }
    
    // resource only DLLs or managed DLLs always loaded as data file
    if (eptr->e32_sect14rva
        || (!eptr->e32_unit[EXP].rva && !(DONT_RESOLVE_DLL_REFERENCES & wFlags))) {
        wFlags = (pMod->wFlags |= NO_IMPORT_FLAGS);
    }

    // allocate memory for both name and the o32 objects in one piece
    len = (NKwcslen(lpszDllName) + 1)*sizeof(WCHAR);

    // +2 for DWORD alignment
    pAlloc = AllocName (eptr->e32_objcnt * sizeof(o32_lite) + len + 2);
    if (!pAlloc) {
        return ERROR_OUTOFMEMORY;
    }

    DEBUGCHK (IsDwordAligned(pAlloc));

    pMod->o32_ptr = (o32_lite *) ((DWORD) pAlloc + 4);    // skip the header info

    // update module name
    pMod->lpszModName = (LPWSTR) (pMod->o32_ptr + eptr->e32_objcnt);
    for (loop = 0; loop < len/sizeof(WCHAR); loop++) {
        pMod->lpszModName[loop] = NKtowlower(lpszDllName[loop]);
    }

    dwErr = ReadO32Info (&pMod->oe, eptr, pMod->o32_ptr, &flags);
    if (dwErr) {
        return dwErr;
    }

    filetype = pMod->oe.filetype;

    if (wFlags & LOAD_LIBRARY_AS_DATAFILE) {
        // load as data-file - always reserve a new address

        pMod->BasePtr = VMReserve (g_pprcNK, eptr->e32_vsize, MEM_IMAGE, (wFlags & LOAD_LIBRARY_IN_KERNEL)? 0 : VM_DLL_BASE);

    } else if (wFlags & LOAD_LIBRARY_IN_KERNEL) {
        // loaded in kernel - no ASLR needed

        if (FA_PREFIXUP & filetype) {
            if (!IsKModeAddr (eptr->e32_vbase)) {
                RETAILMSG (1, (L"!!!ERROR! Trying to load DLL '%s' fixed-up to user address into Kernel.\r\n", lpszFileName));
                RETAILMSG (1, (L"!!!ERROR! MUST SPECIFY 'K' FLAG BIB FILE.\r\n"));
                return ERROR_BAD_EXE_FORMAT;
            }

        } else if (IMAGE_FILE_RELOCS_STRIPPED & flags) {
            RETAILMSG (1, (L"!!!ERROR! Trying to load a DLL '%s' into Kernel without reloc information.\r\n", lpszFileName));
            return ERROR_BAD_EXE_FORMAT;
        }

        pMod->BasePtr = (FA_PREFIXUP & filetype)
            ? (LPVOID) eptr->e32_vbase                              // MODULES Have their VM reserved already
            : VMReserve (g_pprcNK, eptr->e32_vsize, MEM_IMAGE, 0);  // FILES reserve VM

    } else {
        // loaded in user process, support ASLR

        if ((!g_fAslrEnabled || (IMAGE_FILE_RELOCS_STRIPPED & flags))
            && (eptr->e32_vbase > VM_DLL_BASE)
            && (eptr->e32_vbase + eptr->e32_vsize <= VM_SHARED_HEAP_BASE)) {
            // Try to load at preferred load address if ASLR is not enabled.

            pMod->BasePtr = (FA_PREFIXUP & filetype)
                            ? (LPVOID) eptr->e32_vbase          // MODULES Have their VM reserved already
                            : VMAlloc (g_pprcNK,                // FILES reserve VM
                                (LPVOID) eptr->e32_vbase,
                                eptr->e32_vsize,
                                MEM_RESERVE|MEM_IMAGE,
                                PAGE_NOACCESS);
        }

        // if we didn't load at preferred load address, try rebase it if its reloc is not stripped
        if (!pMod->BasePtr) {

            if (IMAGE_FILE_RELOCS_STRIPPED & flags) {
                if (FA_PREFIXUP & filetype) {
                    RETAILMSG (1, (L"!!!ERROR! Trying to load DLL '%s' fixed-up to kernel address into user app.\r\n", lpszFileName));
                    RETAILMSG (1, (L"!!!ERROR! CANNOT SPECIFY 'K' FLAG BIB FILE.\r\n"));
                }
                return ERROR_BAD_EXE_FORMAT;
            }
            pMod->BasePtr = VMReserve (g_pprcNK, eptr->e32_vsize, MEM_IMAGE, VM_DLL_BASE);
            DEBUGMSG (ZONE_LOADER1 && (filetype & FA_PREFIXUP),
                (L"Reloate '%s' in MODULE section from 0x%8.8lx to 0x%8.8lx\r\n", lpszFileName, eptr->e32_vbase, pMod->BasePtr));
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

    dwErr = LoadO32 (&pMod->oe, eptr,pMod->o32_ptr, (ulong)pMod->BasePtr);
    if (dwErr) {
        DEBUGMSG(ZONE_LOADER1,(TEXT("LoadO32 failed dwErr = 0x%8.8lx\r\n"), dwErr));
        return dwErr;
    }

    //
    // relocated if needed
    //
    if ((eptr->e32_vbase != (DWORD) pMod->BasePtr)
        && !(wFlags & LOAD_LIBRARY_AS_DATAFILE)
        && !RelocateNonPageAbleSections (&pMod->oe, eptr, pMod->o32_ptr, (DWORD) pMod->BasePtr)) {
        return ERROR_OUTOFMEMORY;
    }

    if (entry) {
        pMod->startip = entry + (DWORD) pMod->BasePtr;
    } else {
        pMod->wFlags |= MF_NO_THREAD_CALLS;
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

    // AdjustRegions must be called after we add the module to the module list, such
    // that we can find out the location of Import Address Table, and adjust access bit.
    AdjustRegions (g_pprcNK, pMod->BasePtr, &pMod->oe, eptr, pMod->o32_ptr);

    //
    // Done loading the code into the memory via the data cache.
    // Now we need to write-back the data cache since there may be a separate
    // instruction cache.
    //
    NKCacheRangeFlush (NULL, 0, CACHE_SYNC_WRITEBACK|CACHE_SYNC_INSTRUCTIONS|CSF_LOADER);

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

        pAddr = VMAlloc (pprc, pMod->BasePtr, pMod->e32.e32_vsize, MEM_RESERVE|MEM_IMAGE, PAGE_NOACCESS);
        if (pAddr) {
            dwErr = CopyRegions (pMod);
            if (!dwErr) {
                // success
                LockModuleList ();
                AddToDListHead (&pprc->modList, &pEntry->link);
                LockModule (pMod);  // add an extra ref
                UnlockModuleList ();

                // Log a load event for the current process on its first load
                CELOG_ModuleLoad (pprc, pMod);

                if (!(pMod->wFlags & DONT_RESOLVE_DLL_REFERENCES)) {
                    (* HDModLoad) ((DWORD)pMod, FALSE);
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
    DWORD       dwErr = 0;

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
    pMod = (PMODULE) FindModInProcByName (g_pprcNK, pPlainName, !(dwFlags & LOAD_LIBRARY_IN_KERNEL), dwFlags);
    if (pMod) {
        LockModule (pMod);
    }

    UnlockModuleList ();

    if (pMod) {

        if ((DONT_RESOLVE_DLL_REFERENCES == (pMod->wFlags & NO_IMPORT_FLAGS))
            && !(dwFlags & NO_IMPORT_FLAGS)) {
            DEBUGCHK (LOAD_LIBRARY_IN_KERNEL & pMod->wFlags);
            // if a module is loaded previously with DONT_RESOLVE_DLL_REFERENCES flag
            // and the new request is to load as a normal dll, upgrade the module to a regular dll
            pMod->wFlags &= ~DONT_RESOLVE_DLL_REFERENCES;   // this will enable resolving imports
            pMod->wFlags |= MF_LOADING;                     // this will enable calling dllmain
        }
    }

    if (!pMod && !dwErr) {

        pMod = AllocMem (HEAP_MODULE);
        if (!pMod) {
            dwErr = ERROR_OUTOFMEMORY;

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

            dwErr = InitModule (pprc, pMod, lpszFileName, pPlainName, fLbFlags, (WORD)dwFlags);
            if (dwErr) {
                KSetLastError (pCurThread, dwErr);
                DEBUGCHK (!pMod->pmodResource);
                CloseExe (&pMod->oe);
                FreeModuleMemory (pMod);
                pMod = NULL;

            } else {

                if (!(pMod->wFlags & DONT_RESOLVE_DLL_REFERENCES)) {
                    (* HDModLoad) ((DWORD)pMod, TRUE);
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

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }

    // Indicate whether app verifier is enabled or not in the system
    if (pMod)
    {
        pMod->wFlags = (TRUE == g_fAppVerifierEnabled) ?
            (pMod->wFlags | MF_APP_VERIFIER) :
            (pMod->wFlags & ~MF_APP_VERIFIER);
    }

    return pMod;
}

//------------------------------------------------------------------------------
// Win32 LoadLibrary call (from kernel)
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

        pMod = LoadOneLibrary (pszFileName, lbFlags, (WORD)dwFlags);
        if (pMod) {
            DWORD dwRefCnt = pMod->wRefCnt;     // save refcnt for it can change during import (mutual dependent DLLs)
            pMod = (*g_pfnDoImports) (pMod);

            if (pMod
                && (1 == dwRefCnt)
                && ((MF_IMPORTED|MF_NO_THREAD_CALLS) & pMod->wFlags) == MF_IMPORTED) {
                AddToThreadNotifyList (pml, pMod);
                pml = NULL;     // so we don't free it
            }
        }

        if (pMod) {
            CELOG_ModuleLoad (g_pprcNK, pMod);
        }

        UnlockLoader (g_pprcNK);
        SwitchActiveProcess (pprc);

        if (pml) {
            FreeMem (pml, HEAP_MODLIST);
        }

        if (!pMod && IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
            // failed to load pszFileName module in kernel
            CELOG_ModuleFailedLoad (g_pprcNK->dwId, dwFlags, pszFileName);
        }
    } else {
        NKSetLastError (ERROR_NOT_ENOUGH_MEMORY);
    }

    if (unused && (TRUE == g_fAppVerifierEnabled))
    {
        unused->wFlags |= MF_APP_VERIFIER;
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
    pMod = FindModInKernel (pMod);
    if (pMod) {

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

                if (!(pMod->wFlags & (LOAD_LIBRARY_AS_DATAFILE|MF_DEP_COMPACT))) {
                    // if a process loaded a DLL that is not NX compatible, the process
                    // itself becomes not NX compatible.
                    pprc->fFlags &= ~MF_DEP_COMPACT;
                }

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

//------------------------------------------------------------------------------
// Win32 LoadLibrary call (from user)
//------------------------------------------------------------------------------
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

        PPROCESS pprc = pActvProc;

        LockLoader (g_pprcNK);
        pMod = LoadOneLibrary (szNameCpy, lbFlags, dwFlags);
        UnlockLoader (g_pprcNK);

        if (pMod) {
            PMODULE pModRet = EXTInitModule (pMod, pUMod);

            // notify the application debugger if needed
            if (pModRet && pprc->pDbgrThrd && !(pModRet->wFlags & DONT_RESOLVE_DLL_REFERENCES)) {
                DbgrNotifyDllLoad (pCurThread, pprc, pModRet);
            }

            // if pModRet is not NULL, we would have lock the module one more time in InitModInProc. So
            // all we need to do here is to unlock module regardless the return value.
            UnlockModule (pMod, FALSE);
            pMod = pModRet;
        }

        if (!pMod && IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
            // failed to load szNameCopy module in user process
            CELOG_ModuleFailedLoad (pprc->dwId, dwFlags, szNameCpy);
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
                DEBUGMSG (ZONE_LOADER1, (L"Trying to free coredll before process exit. ignored.\r\n"));
                DEBUGCHK (0);
                InterlockedIncrement ((PLONG) &pEntry->wRefCnt);
                pEntry = NULL;  // clear pEntry to indicate error

            } else {
                // process ref-cnt gets to 0, free the library
                DEBUGMSG (ZONE_LOADER1, (L"FreeOneLibrary: last reference to '%s' in process\r\n", pMod->lpszModName));

                FreeModFromProc (pprc, pEntry);
            }
        }
        
        if (pprc == g_pprcNK) {
            UnlockModule (pMod, TRUE);    // module will be completely freed if ref-count get to 0.
        }

        UnlockLoader (pprc);

        if (pprc != g_pprcNK) {
            if (pprc->pDbgrThrd
                && !wRefCnt
                && !(DONT_RESOLVE_DLL_REFERENCES & pMod->wFlags)) {

                // notify the application debugger
                DbgrNotifyDllUnload (pCurThread, pprc, pMod);
            }
            UnlockModule (pMod, FALSE);
        }
    }

    return pEntry != NULL;
}

PMODULE LDRGetModByName (PPROCESS pprc, LPCWSTR pszName, DWORD dwFlags)
{
    DWORD dwErr = 0;
    PMODULELIST pEntry = NULL;
    WCHAR szLocalName[MAX_PATH];

    if (!pszName || !CopyPath (szLocalName, pszName, MAX_PATH)) {
        dwErr = ERROR_INVALID_PARAMETER;

    } else {

        LockModuleList ();
        pEntry = FindModInProcByName (pprc, GetPlainName (szLocalName), pprc != g_pprcNK, dwFlags);
        UnlockModuleList ();

        if (!pEntry) {
            dwErr = ERROR_FILE_NOT_FOUND;
        }
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
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
        pMod = FindModInProcByAddr (pprc, dwAddr);
        if (pMod) {
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

//
// kernel thread used to:
// a) pre-load a specified list of user dlls
// b) notify psl servers of low memory notification
//
#define LOCAL_BUFFER_SIZE       0x400

void WINAPI PSLNotifyThread (LPVOID lpParam)
{    
    // pre-load dlls
    if (hCoreDll) {
        HKEY    hKey;
        LPCWSTR szBootVar = L"init\\BootVars";
        LPCWSTR pzPreLoad = L"PreLoadUserDlls";
            
        if (NKRegOpenKeyExW (HKEY_LOCAL_MACHINE, szBootVar, 0, 0, &hKey) == ERROR_SUCCESS) {
            
            LONG  lResult;
            DWORD regtype;
            WCHAR mszPreLoadDll[LOCAL_BUFFER_SIZE];
            WCHAR *pmszPreLoadDll = &mszPreLoadDll[0];
            DWORD cbSize = sizeof (mszPreLoadDll);
            
            lResult = NKRegQueryValueExW (hKey, pzPreLoad, 0, &regtype, (LPBYTE)pmszPreLoadDll, &cbSize);

            if (ERROR_MORE_DATA == lResult) {
                pmszPreLoadDll = (WCHAR *) NKmalloc (cbSize);
                if (pmszPreLoadDll) {
                    lResult = NKRegQueryValueExW (hKey, pzPreLoad, 0, &regtype, (LPBYTE)pmszPreLoadDll, &cbSize);
                }
            }

            NKRegCloseKey (hKey);

            if ((ERROR_SUCCESS == lResult) && (REG_MULTI_SZ == regtype)) {

                LPCWSTR pszDll = pmszPreLoadDll;

                while (*pszDll) {
                    LockLoader (g_pprcNK);
                    LoadOneLibrary (pszDll, 0, 0);
                    DEBUGMSG (ZONE_LOADER1, (L"Preload DLL '%s'\r\n", pszDll));
                    UnlockLoader (g_pprcNK);
                    pszDll += NKwcslen (pszDll) + 1;
                }
            }

            if (&mszPreLoadDll[0] != pmszPreLoadDll) {
                NKfree (pmszPreLoadDll);
            }
        }
    }

    // make this critical priority from now on
    SCHL_SetThreadBasePrio (pCurThread, CE_THREAD_PRIO_256_TIME_CRITICAL);

    // psl notification
    for ( ; ; ) {
        DoWaitForObjects (1, &phdPslNotifyEvent, INFINITE);
        NKPSLNotify (DLL_MEMORY_LOW, 0, 0);
    }
}

void ASLRInit (BOOL fAslrEnabled, DWORD dwASLRSeed);

void LoadUserCoreDll (BOOL fAslrEnabled, DWORD dwASLRSeed)
{
    // NO PROCESS SHOULD BE CREATED BEFORE THIS
    DEBUGCHK (IsDListEmpty (&g_pprcNK->prclist));
    DEBUGCHK (!hCoreDll);

    // initialize ASLR
    ASLRInit (fAslrEnabled, dwASLRSeed);

    LockLoader (g_pprcNK);
    hCoreDll = (HMODULE) LoadOneLibrary (L"coredll.dll", 0, 0);
    if (hCoreDll) {
        PMODULE pModOalIoctl;

        ((PMODULE) hCoreDll)->wFlags = MF_IMPORTED | MF_NO_THREAD_CALLS;

#if x86
        if (g_pKCoredllFPEmul) {
            // get the user mode FP emulator
            g_pCoredllFPEmul = GetProcAddressA(hCoreDll,(LPCSTR)"NPXNPHandler");
        }
#endif

        pExitThread     = GetProcAddressA (hCoreDll, (LPCSTR)6);                       // ExitThread
        DEBUGCHK (pExitThread);
        
        pUsrExcpExitThread = GetProcAddressA (hCoreDll, (LPCSTR) 1474);                // ThreadExceptionExit
        DEBUGCHK (pUsrExcpExitThread);
        
        MTBFf = GetProcAddressA (hCoreDll, (LPCSTR)14); // MainThreadBaseFunc
        DEBUGCHK (MTBFf);
        
        TBFf  = GetProcAddressA (hCoreDll, (LPCSTR)13); // ThreadBaseFunc
        DEBUGCHK (TBFf);
        
        g_pfnUsrRtlDispExcp = GetProcAddressA (hCoreDll, (LPCSTR) 2548);               // RtlDispatchException
        DEBUGCHK (g_pfnUsrRtlDispExcp);
        
        g_pfnUsrLoadLibraryW = GetProcAddressA (hCoreDll, (LPCSTR) 528);               // LoadLibraryW
        DEBUGCHK (g_pfnUsrLoadLibraryW);
        
        pfnUsrGetHeapSnapshot = (DWORD (*)(THSNAP *)) GetProcAddressA(hCoreDll,(LPCSTR)52);     // GetHeapSnapshot (user mode)
        pUsrMainHeap = (LPDWORD) (DWORD) GetProcAddressA(hCoreDll,(LPCSTR)2543);                // g_hProcessHeap
        pCaptureDumpFileOnDevice = GetProcAddressA(hCoreDll,(LPCSTR)2757);                // CaptureDumpFileOnDevice2 (user mode)

        // initialize remote user heap
        VERIFY (RemoteUserHeapInit (hCoreDll));

        // load OAL ioctl dll; this is valid only if image has coredll
        pModOalIoctl = LoadOneLibrary (OALIOCTL_DLL, 0, LOAD_LIBRARY_IN_KERNEL);

        if (pModOalIoctl) {
            VERIFY ((*g_pfnDoImports) (pModOalIoctl));
            g_pfnOalIoctl = (PFN_Ioctl) GetProcAddressA ((HMODULE)pModOalIoctl, OALIOCTL_DLL_IOCONTROL);
            DEBUGCHK (g_pfnOalIoctl);
            VERIFY (((PFN_DllMain) pModOalIoctl->startip) ((HMODULE)pModOalIoctl, DLL_PROCESS_ATTACH, (DWORD) &(OEMIoControl)));
        }

    }
    UnlockLoader (g_pprcNK);
}


static DWORD QueryReg (LPCWSTR pszValName, LPCWSTR pszReserved, DWORD dwType, LPVOID pBuffer, DWORD cbBufSize)
{
    DWORD dwrtype;
    return ((NKRegQueryValueExW (HKEY_LOCAL_MACHINE, pszValName, (LPDWORD) pszReserved, &dwrtype, (LPBYTE)pBuffer, &cbBufSize) == ERROR_SUCCESS)
            && (dwrtype == dwType))
        ? cbBufSize
        : 0;
}

static void InitSystemSettings (void)
{
    BYTE  pBuffer[MAX_PATH*sizeof(WCHAR)];
    DWORD cbSize;
    DWORD cPages = 0, cBytes = 0;

    // query JIT debugger path
    cbSize = QueryReg (L"JITDebugger", L"Debug", REG_SZ, pBuffer, sizeof (pBuffer));
    if (cbSize
        && (NULL != (pDebugger = AllocName (cbSize)))) {
        memcpy (pDebugger->name, pBuffer, cbSize);
    }

    // query system search path
    cbSize = QueryReg (L"SystemPath", L"Loader", REG_MULTI_SZ, pBuffer, sizeof (pBuffer));
    if (cbSize
        && (NULL != (pSearchPath = AllocName (cbSize)))) {
        memcpy (pSearchPath->name, pBuffer, cbSize);
    }

    // set default value for PageOutTrigger and PageOutLevel
    QueryReg (L"cbLow", L"SYSTEM\\OOM", REG_DWORD, &cBytes, sizeof (DWORD));
    QueryReg (L"cpLow", L"SYSTEM\\OOM", REG_DWORD, &cPages, sizeof (DWORD));

    if (cBytes || cPages) {
        cBytes = (cBytes + VM_PAGE_SIZE -1) / VM_PAGE_SIZE;   // convert to pages
        cPages = (cBytes>cPages)? cBytes : cPages;
    }
    PageOutTrigger = cPages + g_cpPageOutLow;
    PageOutLevel = cPages + g_cpPageOutHigh;

}

//
// perform rest of system initialization after filesys is ready
//
BOOL PostFsInit (PFSINITINFO pfsi)
{
    if (!hCoreDll) {

        HANDLE hTh;
        
#ifdef ARM
        if(!IsV6OrLater())
#endif        
        {
            RETAILMSG (pfsi->dwNxFlags, (L"Warning! NX support is only available for ARMv6 and ARMv7.  HKEY_LOCAL_MACHINE\\init\\BootVars\\NXSupport will be ignored \r\n"));
            DEBUGCHK (!pfsi->dwNxFlags);
        }
        
        g_dwNXFlags = pfsi->dwNxFlags;

        // BC: create the FSReady Event, initially signalled
        NKCreateEvent (NULL, TRUE, TRUE, TEXT("SYSTEM/FSReady"));

        // Initialize MUI-Resource loader (requires registry)
        if (pfsi->fMuiEnabled) {
            InitMUILanguages();
        }

        // Read system settings from registry
        InitSystemSettings ();

        LoadUserCoreDll (pfsi->fAslrEnabled, pfsi->dwASLRSeed);

        // create a thread to serve as psl notification thread
        hTh = CreateKernelThread ((LPTHREAD_START_ROUTINE) PSLNotifyThread, NULL, CE_THREAD_PRIO_256_IDLE, 0);
        HNDLCloseHandle (g_pprcNK, hTh);
    }
    
    return NULL != hCoreDll;
}



//-----------------------------------------------------------------------------
// LoaderInit - loader initialization
//
BOOL LoaderInit ()
{
    ROMChain_t *pROM;
    DWORD       ROMDllEnd;
    PNAME       pTmp;
    DWORD       dummy;

    DEBUGMSG (ZONE_DEBUG, (L"LoaderInit: Initialing loader\r\n"));

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
    ReadO32Info (&g_pprcNK->oe, &g_pprcNK->e32, g_pprcNK->o32_ptr, &dummy);

    LockLoader (g_pprcNK);
    
    g_pModKernDll = LoadOneLibrary (TEXT(KERNELDLL), LLIB_NO_PAGING, LOAD_LIBRARY_IN_KERNEL|DONT_RESOLVE_DLL_REFERENCES);
    DEBUGCHK (g_pModKernDll);
    
    if ((PFN_Ioctl) ReturnFalse != g_pNKGlobal->pfnKITLIoctl) {
        g_pModKitlDll = LoadOneLibrary (TEXT(KITLDLL), LLIB_NO_PAGING, LOAD_LIBRARY_IN_KERNEL|DONT_RESOLVE_DLL_REFERENCES);
        if (g_pModKitlDll) {
            g_pModKitlDll->ZonePtr = g_pNKGlobal->pKITLDbgZone;
        }
    }
    hKCoreDll = (HMODULE) LoadOneLibrary (L"coredll.dll", LLIB_NO_PAGING, LOAD_LIBRARY_IN_KERNEL);
    DEBUGCHK (hKCoreDll);
    ((PMODULE) hKCoreDll)->wFlags = LOAD_LIBRARY_IN_KERNEL | MF_IMPORTED | MF_NO_THREAD_CALLS | MF_COREDLL;
    VERIFY (((PFN_DllMain) ((PMODULE)hKCoreDll)->startip) (hKCoreDll, DLL_PROCESS_ATTACH, (DWORD) NKKernelLibIoControl));

    UnlockLoader (g_pprcNK);

#ifdef DEBUG
    dpCurSettings.ulZoneMask = initialKernelLogZones;
    g_pModKernDll->ZonePtr = &dpCurSettings;
#endif

    KTBFf = GetProcAddressA(hKCoreDll,(LPCSTR)13);         // ThreadBaseFunc for kernel threads
    DEBUGCHK (KTBFf);
    
    g_pfnKrnRtlDispExcp = GetProcAddressA (hKCoreDll, (LPCSTR) 2548);               // RtlExceptionDispatch
    DEBUGCHK (g_pfnKrnRtlDispExcp);
#ifndef x86
    g_pfnRtlUnwindOneFrame = GetProcAddressA (hKCoreDll, (LPCSTR) 2549);            // RtlUnwindOneFrame
    DEBUGCHK (g_pfnRtlUnwindOneFrame);
#endif    
    pKrnExcpExitThread = GetProcAddressA (hKCoreDll, (LPCSTR) 1474);                // ThreadExceptionExit
    DEBUGCHK (pKrnExcpExitThread);
    
    g_pfnGetProcAddrW = (PFN_GetProcAddrW) GetProcAddressA (hKCoreDll, (LPCSTR) 530);     // GetProcAddressW
    DEBUGCHK (g_pfnGetProcAddrW);

    VERIFY (InitGenericHeap (hKCoreDll));

    // need the "FindResrouceW" pointer for loading MUI
    g_pfnFindResource   = (PFN_FindResource) GetProcAddressA (hKCoreDll, (LPCSTR) 532);

#if x86
    {
        g_pKCoredllFPEmul = GetProcAddressA(hKCoreDll,(LPCSTR)"NPXNPHandler");

        InitFPUIDTs (g_pKCoredllFPEmul);
        if (!g_pKCoredllFPEmul) {
            DEBUGMSG(ZONE_SEH,(L"Did not find emulation code for x86... using floating point hardware.\r\n"));
            dwFPType = 1;
            KCall((PKFN)InitializeFPU);
        } else {
            DEBUGMSG(ZONE_SEH,(L"Found emulation code for x86... using software for emulation of floating point.\r\n"));
            dwFPType = 2;
            KCall((PKFN)InitializeEmx87);
        }
    }
#endif
    g_pNKGlobal->pfnCompareFileTime = (LONG (*)(const FILETIME*, const FILETIME*))GetProcAddressA(hKCoreDll,(LPCSTR)18); // CompareFileTime
    DEBUGCHK (g_pNKGlobal->pfnCompareFileTime);
    pfnKrnGetHeapSnapshot = (DWORD (*)(THSNAP *)) GetProcAddressA(hKCoreDll,(LPCSTR)52); // GetHeapSnapshot (kernel mode)
    pKCaptureDumpFileOnDevice = GetProcAddressA(hKCoreDll,(LPCSTR)2757); //  CaptureDumpFileOnDevice2 (kernel mode)
    pKrnMainHeap = (LPDWORD) (DWORD) GetProcAddressA(hKCoreDll,(LPCSTR)2543);

    pfnCeGetCanonicalPathNameW = (DWORD (*)(LPCWSTR, LPWSTR, DWORD, DWORD))GetProcAddressA(hKCoreDll,(LPCSTR)1957); //  CeGetCanonicalPathNameW
    g_pfnSetSystemPowerState = (DWORD (*)(LPCWSTR, DWORD, DWORD))GetProcAddressA(hKCoreDll,(LPCSTR)1582); //  SetSystemPowerState (used by reset code)
    g_pfnCompactAllHeaps = GetProcAddressA (hKCoreDll,(LPCSTR)54); //  CompactAllHeaps


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

