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
//
// This source code is licensed under Microsoft Shared Source License
// Version 6.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//
//    loader.h - internal kernel loader header
//
#ifndef __NK_LOADER_H__
#define __NK_LOADER_H__

#include "kerncmn.h"
#include "pgpool.h"
#include <psapi.h>
#include <pehdr.h>
#include <romldr.h>

extern BOOL g_fNoKDebugger;
extern BOOL g_fKDebuggerLoaded;

//
// openexe structure
//
typedef struct openexe_t {
    union {
        HANDLE hf;          // object store handle
        TOCentry *tocptr;   // rom entry pointer
    };
    BYTE filetype;
    BYTE bPad;
    WORD pagemode;
    union {
        DWORD offset;
        DWORD dwExtRomAttrib;
    };
    PNAME lpName;
} openexe_t;

//
// MODULE structure, one for eah DLL
//
struct _MODULE {
    DLIST       link;                   /* Doubly-linked list to link all modules (must be 1st) */
    PMODULE     lpSelf;                 /* ptr to self for validation */
    WORD        wRefCnt;                /* ref-cnt (# of processes loaded this module) */
    WORD        wFlags;                 /* module flags */
    ulong       startip;                /* entrypoint */
    LPVOID      BasePtr;                /* Base pointer of dll load (not 0 based) */
    PMODULE     pmodResource;           /* module that contains the resources */
    e32_lite    e32;                    /* E32 header */
    LPWSTR      lpszModName;            /* Module name */
    DWORD       DbgFlags;               /* Debug flags */
    LPDBGPARAM  ZonePtr;                /* address of Debug zone */
    DWORD       dwZone;                 /* the zone value */
    PFNIOCTL    pfnIoctl;               /* LoadKernelLibrary specific - entry to IOCTL funciton */
    openexe_t   oe;                     /* Pointer to executable file handle */
    o32_lite    *o32_ptr;               /* O32 chain ptr */
    PHDATA      phdAuth;                /* authentication data object for user mode dll */
};

typedef FARPROC (* PFN_GetProcAddrA) (HMODULE hMod, LPCSTR pszFuncName);
typedef FARPROC (* PFN_GetProcAddrW) (HMODULE hMod, LPCWSTR pszFuncName);
typedef PMODULE (* PFN_DoImport) (PMODULE pMod);
typedef void    (* PFN_UndoDepends) (PMODULE pMod);
typedef BOOL (*PFN_AppVerifierIoControl) (
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    );
typedef DWORD (*PFN_SetSystemPowerState) (LPCWSTR, DWORD, DWORD);

typedef const struct info   *PCInfo;

extern PFN_GetProcAddrA g_pfnGetProcAddrA;
extern PFN_GetProcAddrW g_pfnGetProcAddrW;
extern PFN_DoImport     g_pfnDoImports;
extern PFN_UndoDepends  g_pfnUndoDepends;
extern PFN_AppVerifierIoControl g_pfnAppVerifierIoControl;

// module state flags, must not conflict with valid loader flags
//#define DONT_RESOLVE_DLL_REFERENCES     0x00000001
//#define LOAD_LIBRARY_AS_DATAFILE        0x00000002
//#define LOAD_WITH_ALTERED_SEARCH_PATH   0x00000008
#define MF_PAGED_IN                 0x0010
#define MF_DEP_COMPACT              0x0080      // Data Execution Prevention compatible
#define MF_LOADING                  0x0100
#define MF_IMPORTED                 0x0200
#define MF_NO_THREAD_CALLS          0x0400
#define MF_APP_VERIFIER             0x0800
#define MF_IMPORTING                0x1000
#define MF_SKIPPOLICY               0x2000
#define MF_COREDLL                  0x4000
#define LOAD_LIBRARY_IN_KERNEL      0x8000

#define NO_IMPORT_FLAGS             (LOAD_LIBRARY_AS_DATAFILE|DONT_RESOLVE_DLL_REFERENCES)

ERRFALSE(MF_DEP_COMPACT < 255);

#define IsModuleNxCompatible(pMod)  ((pMod)->wFlags & MF_DEP_COMPACT)

// stub module - per process list of modules
typedef struct _MODULELIST {
    DLIST       link;                   /* Doubly-linked list to link all modules in the same process (must be 1st) */
    PMODULE     pMod;                   /* ptr to MODULE */
    WORD        wRefCnt;                /* ref-cnt (# of processes loaded this module */
    WORD        wFlags;                 /* module flags */
} MODULELIST, *PMODULELIST;

//
// every user mode DLL cached the following informations in e32_units
//      EXP - export directory              (defined in pehdr.h == 0)
//      IMP - import directory              (defined in pehdr.h == 1)
//      RES - resource information          (defined in pehdr.h == 2)
//
#define NUM_E32_UNITS   3

typedef BOOL (*dllntry_t)  (HMODULE, DWORD, LPVOID);

// user process module list
typedef struct _USERMODULELIST USERMODULELIST, *PUSERMODULELIST;
struct _USERMODULELIST {
    DLIST       link;                   /* Doubly-linked list to link all modules in the same process (must be 1st) */
    HMODULE     hModule;                /* ptr to MODULE */
    WORD        wRefCnt;                /* ref-cnt (# of processes loaded this module */
    WORD        wFlags;                 /* module flags */
    dllntry_t   pfnEntry;               /* entry point */
    DWORD       dwBaseAddr;             /* Base address of the Dll */
    PUSERMODULELIST     pmodResource;   /* module that contains the resources */
    DLIST       loadOrderLink;          /* list to maintain load order */
    struct info udllInfo[NUM_E32_UNITS];/* e32 information for user mode DLLs */
};

typedef struct _LOADERINITINFO {
    FARPROC     pfnGetProcAddr;
    FARPROC     pfnImportAndCallDllMain;
    FARPROC     pfnUndoDepends;
    FARPROC     pfnAppVerifierIoControl;
    FARPROC     pfnFindModule;
    FARPROC     pfnNKwvsprintfW;
} LOADERINITINFO, *PLOADERINITINFO;


//------------------------------------------------------------------------------
//
// macros
//

//
// paging modes
//  Note: the relative order is important as we use < or > to determine the code path
//
#define PM_FULLPAGING   0       // fully pageable
#define PM_PARTPAGING   1       // partly pageable
#define PM_NOPAGING     2       // not pageable,
#define PM_CANNOT_PAGE  3       // paging not supported at all (hardware limitation)

#define FullyPageAble(oeptr)        (PM_FULLPAGING == (oeptr)->pagemode)
#define PageAble(oeptr)             (PM_PARTPAGING >= (oeptr)->pagemode)
#define PagingSupported(oeptr)      (PM_CANNOT_PAGE != (oeptr)->pagemode)

#define FA_XIP          0x1     // xip-able media
#define FA_PREFIXUP     0x2     // already fixedup by romimage (in module section)
#define FA_DIRECTROM    0x4     // can be accessed directly with pointer
#define FA_NORMAL       0x8     // normal executable (not fixed up)

#define FT_OBJSTORE     (FA_NORMAL)                             // object store
#define FT_EXTIMAGE     (FA_PREFIXUP)                           // external Module, not XIP-able
#define FT_EXTXIP       (FA_PREFIXUP|FA_XIP)                    // external XIP-able media
#define FT_ROMIMAGE     (FA_PREFIXUP|FA_XIP|FA_DIRECTROM)       // ROM XIP (the region where pTOC resides)

ERRFALSE(sizeof(CEOID) == 4);

ERRFALSE(!(DONT_RESOLVE_DLL_REFERENCES & 0xffff0000));
ERRFALSE(!(LOAD_LIBRARY_AS_DATAFILE & 0xffff0000));
ERRFALSE(!(LOAD_WITH_ALTERED_SEARCH_PATH & 0xffff0000));

// Lowest DLL load address
#define DllLoadBase KInfoTable[KINX_DLL_LOW]

#define DBG_IS_DEBUGGER    0x00000001
#define DBG_SYMBOLS_LOADED 0x80000000

#define IsValidModule(p) (IsValidKPtr(p) && ((p)->lpSelf == (p)))

#define KERNELDLL   "kernel.dll"
#define KITLDLL     "kitl.dll"

//------------------------------------------------------------------------------
//
// globals
//
#ifdef IN_KERNEL
#ifndef NK_LOADER
extern const ROMHDR        *pTOC;
#endif
#endif

extern fslog_t *LogPtr;
extern ROMChain_t FirstROM, *ROMChain;

//------------------------------------------------------------------------------
//
// function prototypes
//

typedef BOOL (* PFN_DllMain) (HMODULE hInst, DWORD reason, DWORD dwReserved);


#if defined(x86) || defined (ARM)
BOOL MDValidateRomChain (ROMChain_t *pROMChain);
#else
// MIPS and x86 doesn't have OEMAddressTable and therefore nothing to validate
#define MDValidateRomChain(x)       (TRUE)
#endif

PMODULE NKLoadLibraryEx(LPCWSTR lpLibFileName, DWORD dwFlags, PUSERMODULELIST unused);
PMODULE EXTLoadLibraryEx(LPCWSTR lpLibFileName, DWORD dwFlags, PUSERMODULELIST pUMod);
BOOL NKFreeLibrary(HMODULE hInst);
HANDLE NKLoadKernelLibrary(LPCWSTR lpszFileName);

void LockModuleList (void);
void UnlockModuleList (void);
BOOL OwnModListLock (void);

PMODULELIST FindModInProc (PPROCESS pprc, PMODULE pMod);
PMODULE LDRGetModByName (PPROCESS pprc, LPCWSTR pszName, DWORD dwFlags);
PMODULE FindModInProcByAddr (PPROCESS pprc, DWORD dwAddr);

BOOL ReadExtImageInfo (HANDLE hFile, DWORD dwCode, DWORD cbSize, LPVOID pBuf);

PMODULE LoadOneLibrary (LPCWSTR lpszFileName, DWORD fLbFlags, DWORD dwFlags);
VOID FreeAllProcLibraries(PPROCESS);
void NKForceCleanBoot (void);
const o32_lite *FindOptr (const o32_lite *optr, int nCnt, DWORD dwAddr);

//
// open an executable
//      dwFlags consists of the following bits
//          - wFlags fields of DLL if we're opening a DLL
//          - LDROE_ALLOW_PAGING, if paging is not allowed
// NOTE: cannot use the lower 16 bits (or'd with the 16-bit LoadLibrary flag.
//
#define LDROE_ALLOW_PAGING    0x20000
#define LDROE_OPENDEBUG       0x40000  // caller is a debugger, or being debugged.
#define LDROE_OPENDLL         0x80000  // loading a dll
#define LDROE_OPENINACCT      0x100000 // load exe in a specific account
DWORD OpenExecutable (PPROCESS pprc, LPCWSTR lpszName, openexe_t *oeptr, DWORD dwFlags);

void FreeExeName(openexe_t *oeptr);
void CloseExe(openexe_t *oeptr);
DWORD LoadE32(openexe_t *oeptr, e32_lite *eptr, DWORD *lpFlags, DWORD *lpEntry, BOOL bIgnoreCPU);
BOOL LoaderInit (void);

// Page-out functions
BOOL DecommitExe (PPROCESS pprc, const openexe_t *oeptr, const e32_lite *eptr, const o32_lite *o32_ptr);
DWORD DoPageOutModule (PPROCESS pprc, PMODULE pMod, DWORD dwFlags);
void PageOutProcesses (BOOL fCritical);
void PageOutModules (BOOL fCritical);

HANDLE NKLoadIntChainHandler (LPCWSTR lpszFileName, LPCWSTR lpszHandlerName, BYTE bIRQ);
BOOL NKFreeIntChainHandler (HANDLE hLib);

DWORD FindROMDllEntry (const ROMHDR *pTOC, LPCSTR pszName);

void DllThreadNotify (DWORD dwReason);
DWORD CopyPath (LPWSTR pDst, LPCWSTR pSrc, DWORD cchLen);

FORCEINLINE BOOL IsAddrInMod (PMODULE pMod, DWORD dwAddr)
{
    return (DWORD) (dwAddr - (DWORD) pMod->BasePtr) < pMod->e32.e32_vsize;
}


#endif  // __NK_LOADER_H__

