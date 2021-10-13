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

#define VERIFIER_IMPORT_VERSION 2

// Provided by the kernel for celog to use
typedef struct _VerifierImportTable {
    DWORD    dwVersion;               // Version of this structure, set to VERIFIER_IMPORT_VERSION
    const FARPROC *pWin32Methods;     // Kernel Win32 (non-handle-based) methods
    const FARPROC *pExtraMethods;      
    struct KDataStruct *pKData;       // Pointer to KData struct
    FARPROC  NKDbgPrintfW;            // Pointer to NKDbgPrintfW function
    FARPROC LoadOneLibraryW;          // Pointer to LoadOneLibraryW
    FARPROC FreeOneLibrary;           // Pointer to FreeOneLibrary
    FARPROC CallDLLEntry;             // Pointer to CallDLLEntry
    FARPROC KAsciiToUnicode;
    FARPROC AllocMem;
    FARPROC FreeMem;
    FARPROC AllocName;
} VerifierImportTable;

#define VERIFIER_EXPORT_VERSION 2

// Provided by celog for the kernel to use
typedef struct _VerifierExportTable {
    DWORD   dwVersion;                // Version of this structure, set to VERIFIER_EXPORT_VERSION
    PFNVOID pfnInitLoaderHook;
    PFNVOID pfnWhichMod;
    PFNVOID pfnUndoDepends;
    PFNVOID pfnVerifierIoControl;
} VerifierExportTable;

#endif // _ALTIMPORTS
