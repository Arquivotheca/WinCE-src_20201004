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
//      kcover.c
//  
//  Abstract:  
//
//      Part of the code coverage tool runtime system, this component allocates the global,
//	  universally-accessible logging region.
//      
//------------------------------------------------------------------------------
#include <kernel.h>
#include <altimports.h>
#include <kcover.h>

#ifdef DEBUG
DBGPARAM dpCurSettings = { TEXT("KCover"), {
        TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"),
        TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"),
        TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"),
        TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>") },
    0x00000000};
#endif

//------------------------------------------------------------------------------
// ZONES

#define ZONE_VERBOSE 0

//------------------------------------------------------------------------------
// MISC

VerifierImportTable g_Imports;
BOOL g_fDone;

// Print debug messages even in retail builds
#ifdef RETAILMSG
#undef RETAILMSG
#define RETAILMSG(cond, printf_exp) ((cond)?(g_Imports.NKDbgPrintfW printf_exp),1:0)
#endif // SHIP_BUILD


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#define WIN32CALL(Api, Args)	((g_Imports.pWin32Methods[W32_ ## Api]) Args)

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL
KCoverAllocLoggingRegion()
{
    LPBYTE pBuffer = NULL;
    LPBYTE pBufferCommitted = NULL;

    //
    // Allocate the buffer.
    //
    pBuffer = (LPBYTE) WIN32CALL(VirtualAlloc, (NULL, KCOVER_LOGGING_REGION_SIZE, MEM_RESERVE, PAGE_NOACCESS));
    if (pBuffer == NULL) {
        RETAILMSG(1, (TEXT("KCover: Failed to reserve logging buffer\r\n")));
        return FALSE;
    }

    if (((unsigned char*)0x40000000) > pBuffer) {
        RETAILMSG(1, (TEXT("KCover: Coverage logging region not allocated in globally shared area (0x%08X)\r\n"),
        			pBuffer));
        WIN32CALL(VirtualFree, (pBuffer, 0, 0));
    	return FALSE;
    }

    if (((unsigned char*)KCOVER_EXPECTED_LOGGING_REGION_ADDRESS) != pBuffer) {
        RETAILMSG(1, (TEXT("KCover: Coverage logging region allocated in an unexpected place (0x%08X)\r\n"),
        			pBuffer));
        WIN32CALL(VirtualFree, (pBuffer, 0, 0));
    	return FALSE;
    }

    pBufferCommitted = (LPBYTE) WIN32CALL(VirtualAlloc, (pBuffer, KCOVER_LOGGING_REGION_SIZE, MEM_COMMIT, PAGE_READWRITE));
    if (pBufferCommitted == NULL) {
        RETAILMSG(1, (TEXT("KCover: Failed to commit logging buffer\r\n")));
        WIN32CALL(VirtualFree, (pBuffer, 0, 0));
        return FALSE;
    }
    
    if (pBufferCommitted != pBuffer) {
        RETAILMSG(1, (TEXT("KCover: Committed buffer isn't at expected reserved address (0x%08X vs. 0x%08X)\r\n"),
        			pBufferCommitted, pBuffer));
        WIN32CALL(VirtualFree, (pBuffer, 0, 0));
        return FALSE;
    }

    
    RETAILMSG(1, (TEXT("KCOVER: KCoverAllocBuffer() : Allocated 0x%X kB for Buffer at 0x%08X\r\n"), 
                            KCOVER_LOGGING_REGION_SIZE >> 10, pBuffer));
    return TRUE;
}


//------------------------------------------------------------------------------
// Hook up function pointers and allocate logging region
//------------------------------------------------------------------------------
BOOL static
InitLibrary(
    FARPROC pfnKernelLibIoControl
    )
{
    //
    // KernelLibIoControl provides the back doors we need to obtain kernel
    // function pointers.
    //
    
    // Get imports from kernel
    g_Imports.dwVersion = VERIFIER_EXPORT_VERSION;
    if (!pfnKernelLibIoControl((HANDLE)KMOD_VERIFIER, IOCTL_VERIFIER_IMPORT, &g_Imports,
                               sizeof(VerifierImportTable), NULL, 0, NULL)) {
        // Can't DEBUGMSG or anything here b/c we need the import table to do that;
        // But the consequences of silently failing are too ugly for any instrumented exe, so at least we
        // should signal that there is a problem here
        DebugBreak();
        return FALSE;
    }
    
    // Now allocate coverage logging region
    return KCoverAllocLoggingRegion();
}


//------------------------------------------------------------------------------
// DLL entry
//------------------------------------------------------------------------------
BOOL WINAPI
KCoverDLLEntry(HINSTANCE DllInstance, INT Reason, LPVOID Reserved)
{
    if ( (DLL_PROCESS_ATTACH == Reason ) && !g_fDone ) {
            // Reserved parameter is a pointer to KernelLibIoControl function
       return (g_fDone = InitLibrary((FARPROC)Reserved));
            }
    else {
    	return FALSE;
    	}
}

