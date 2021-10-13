//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// Structures for hooking into loader

#ifndef _ALTIMPORTS
#define _ALTIMPORTS

//------------------------------------------------------------------------------
// Interface to a Verifier plugin DLL
//

#define VERIFIER_IMPORT_VERSION 3

// Provided by the kernel for shimeng to use
typedef struct _VerifierImportTable {
    DWORD    dwVersion;               // Version of this structure, set to VERIFIER_IMPORT_VERSION
    const FARPROC *pWin32Methods;     // Kernel Win32 (non-handle-based) methods
    const FARPROC *pExtraMethods;      
    struct KDataStruct *pKData;       // Pointer to KData struct
    FARPROC  NKDbgPrintfW;            // Pointer to NKDbgPrintfW function
    FARPROC LoadOneLibraryW;          // Pointer to LoadOneLibraryW
    FARPROC FreeOneLibrary;           // Pointer to FreeOneLibrary
    FARPROC KAsciiToUnicode;
    FARPROC AllocMem;
    FARPROC FreeMem;
    FARPROC AllocName;
    FARPROC InitializeCriticalSection;
    FARPROC EnterCriticalSection;
    FARPROC LeaveCriticalSection;
    LPCRITICAL_SECTION pModListcs;
} VerifierImportTable;

#define VERIFIER_EXPORT_VERSION 3

// Provided by shimeng for the kernel to use
typedef struct _VerifierExportTable {
    DWORD   dwVersion;                // Version of this structure, set to VERIFIER_EXPORT_VERSION
    PFNVOID pfnShimInitModule;
    PFNVOID pfnShimWhichMod;
    PFNVOID pfnShimUndoDepends;
    PFNVOID pfnShimGetProcModList;
    PFNVOID pfnShimCloseModule;
    PFNVOID pfnShimCloseProcess;
    PFNVOID pfnShimIoControl;
} VerifierExportTable;

typedef BOOL (*PFN_SHIMINITMODULE)(e32_lite *, o32_lite *, DWORD, LPCTSTR);
typedef PMODULE (*PFN_SHIMWHICHMOD)(PMODULE, LPCTSTR, LPCTSTR, DWORD, DWORD, e32_lite *);
typedef BOOL (*PFN_SHIMUNDODEPENDS)(e32_lite *eptr, DWORD BaseAddr, BOOL fAddToList);
typedef BOOL (*PFN_SHIMIOCONTROL)(DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);
typedef BOOL (*PFN_SHIMGETPROCMODLIST)(PDLLMAININFO pList, DWORD nMods);
typedef void (*PFN_SHIMCLOSEMODULE)(PMODULE pMod);
typedef void (*PFN_SHIMCLOSEPROCESS)(HANDLE hProc);

#endif // _ALTIMPORTS

