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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include <windows.h>
#include <loader.h>
#include "loader_core.h"
#include "..\lmem\heap.h"
#include "dllheap.h"

//
// defined in rheap.cpp in coredll
//
extern MODULEHEAPINFO g_ModuleHeapInfo;

//------------------------------------------------------------------------------
// SetHeapId - useful for dll heap. "dwProcessId" field is assumed to be set to 
// the module base address passed in from loader.
//------------------------------------------------------------------------------
BOOL SetHeapId(HANDLE hHeap, DWORD dwId)
{
    PRHEAP php = (PRHEAP) hHeap;
    if (!hHeap || php->dwSig != HEAPSIG) {
        return FALSE;
    }
    
    php->dwProcessId = dwId;
    return TRUE;
}


//------------------------------------------------------------------------------
// DllHeapImportOneBlock: helper function to override the heap related functions
// to point to the dll heap implementation.
//------------------------------------------------------------------------------
static
DWORD
DllHeapImportOneBlock (
    PUSERMODULELIST pMod,         // DLL to be imported from ({k}coredll.dll)
    DWORD           dwImpBase,    // base address of the DLL importing
    PCImpHdr        blockptr,     // block currently importing
    LPDWORD         ltptr,        // starting ltptr
    LPDWORD         atptr,        // starting atptr
    PMODULEHEAPINFO pModuleHeapInfo
    )
{
    DEBUGCHK(pMod->wFlags & MF_COREDLL);

    for (; *ltptr; ltptr ++, atptr ++) {

        if (*ltptr & 0x80000000) {
            DWORD ord = *ltptr & 0x7fffffff;

           switch(ord) {

                // local heap functions
                case 33:
                    *atptr = (DWORD)pModuleHeapInfo->pfnLocalAlloc;
                break;
                case 34:
                    *atptr = (DWORD)pModuleHeapInfo->pfnLocalReAlloc;
                break;
                case 35:
                    *atptr = (DWORD)pModuleHeapInfo->pfnLocalSize;
                break;
                case 36:
                    *atptr = (DWORD)pModuleHeapInfo->pfnLocalFree;
                break;
                case 50:
                    *atptr = (DWORD)pModuleHeapInfo->pfnGetProcessHeap;
                break;
                case 2602:
                    *atptr = (DWORD)pModuleHeapInfo->pfnLocalAllocTrace;
                break;

                // crt function overrides
                case 1041:
                    *atptr = (DWORD)pModuleHeapInfo->pfnMalloc;
                break;
                case 1049:
                    *atptr = (DWORD)pModuleHeapInfo->pfnmsize;
                break;
                case 1054:
                    *atptr = (DWORD)pModuleHeapInfo->pfnRealloc;
                break;
                case 1094:
                    *atptr = (DWORD)pModuleHeapInfo->pfnDelete;
                break;
                case 1095:
                    *atptr = (DWORD)pModuleHeapInfo->pfnNew;
                break;
                case 1346:
                    *atptr = (DWORD)pModuleHeapInfo->pfnCalloc;
                break;
                case 1456:
                    *atptr = (DWORD)pModuleHeapInfo->pfnNew2;
                break;
                case 1457:
                    *atptr = (DWORD)pModuleHeapInfo->pfnDelete2;
                break;
                case 2656:
                    *atptr = (DWORD)pModuleHeapInfo->pfnRecalloc;
                break;

                // string function overrides
                case 74:
                    *atptr = (DWORD)pModuleHeapInfo->pfnWcsDup;
                break;
                case 1409:
                    *atptr = (DWORD)pModuleHeapInfo->pfnStrDup;
                break;

                default:
                break;
            }
        }
    }

    return TRUE;
}


//------------------------------------------------------------------------------
// DllHeapDoImports: called by loader to override any heap functions.
//------------------------------------------------------------------------------
BOOL
DllHeapDoImports (
    DWORD  dwBaseAddr,
    LPVOID pImportInfo
    )
{    
    // make sure we have a valid dll heap init structure and a valid dll heap
    if (SetHeapId(g_ModuleHeapInfo.hModuleHeap, dwBaseAddr)) {
        
        //
        // dll heap is enabled and the module heap is valid
        // re-wire all the heap functions from the import
        // table of this dll to the ones defined in dll heap
        // entry structure
        //
        PCInfo pImpInfo = (PCInfo) pImportInfo;
        if (pImpInfo->size && g_ModuleHeapInfo.pfnGetProcessHeap) {

            LPDWORD     ltptr, atptr;
            PUSERMODULELIST pMod = NULL;
            WCHAR       ucptr[MAX_DLLNAME_LEN];
            PCImpHdr    blockstart = (PCImpHdr) (dwBaseAddr+pImpInfo->rva);
            PCImpHdr    blockptr;

            for (blockptr = blockstart; blockptr->imp_lookup; blockptr++) {

                AsciiToUnicode (ucptr, (LPCSTR)dwBaseAddr+blockptr->imp_dllname, MAX_DLLNAME_LEN);
                pMod = DoLoadLibrary (ucptr, 0);

                // all dependent modules should have been loaded before calling this function
                DEBUGCHK(pMod);

                if (!pMod || !(pMod->wFlags & MF_COREDLL)) {
                    // if this is not coredll block, skip
                    continue;
                }

                ltptr = (LPDWORD) (dwBaseAddr + blockptr->imp_lookup);
                atptr = (LPDWORD) (dwBaseAddr + blockptr->imp_address);

                DllHeapImportOneBlock (pMod, dwBaseAddr, blockptr, ltptr, atptr, &g_ModuleHeapInfo);
                g_ModuleHeapInfo.nDllHeapModules++;
                break;
            }
        }
    }

    // reset dll heap pointer; this will be populated again when the next 
    // module which is using dll heap is loaded and dllmain is called.
    g_ModuleHeapInfo.hModuleHeap = NULL;
    
    return TRUE;
}

