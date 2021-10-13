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
#include <windows.h>
#include "fpredir.h"


// Special markers for the start and end of the FPCRT redirection entries.
//
__declspec(allocate(INITIALIZER_SECTION_BEGIN))
static _FPAPI_ENTRY g_fpApiBegin = {};

__declspec(allocate(INITIALIZER_SECTION_END))
static _FPAPI_ENTRY g_fpApiEnd = {};


// Merge our 'special' sections with the standard data and executable sections
//
#pragma comment(linker, "/MERGE:" INITIALIZER_SECTION "=.data")
#pragma comment(linker, "/MERGE:" FPTHUNKS_SECTION "=.text")


// the handle to the FPCRT dll
//
static
HMODULE g_hFpCrtDLL = NULL;


// This function fixes the redirection entries so that the <pfn>
// members will point to the real import from FPCRT dll.
//
// The redirection happens when the first call is made to any
// of the FPCRT redirected functions.
//
extern "C"
void _FixupFPCrtRedirection()
{
    // The linker should have ordered the start/end appropriately.
    //
    DEBUGCHK(&g_fpApiBegin < &g_fpApiEnd);

    // load the FPCRT dll
    //
    HMODULE hFpCrtDLL = LoadLibrary(L"FPCRT.DLL");
    
    // FPCRT should not fail to load, if this happens
    // there's something bad happening - unfortunatelly we
    // can't do much at this point.
    //
    if(hFpCrtDLL == NULL)
    {
        DEBUGCHK(!"Failed to load FPCRT.DLL");
        return;
    }
    
    // Fix the redirection entries to point to the FPCRT exports
    //
    // NOTE: there may be another thread that will overlap here,
    //   but that's ok as we'll write the same pointers to <pfn>
    //   (we want to make sure that when this function returns
    //    the redirection entries have the correct values)
    //
    for(_FPAPI_ENTRY* pEntry = &g_fpApiBegin + 1; pEntry < &g_fpApiEnd; ++pEntry)
    {
        DEBUGCHK(pEntry->ordinal != 0);
        
        pEntry->pfn = reinterpret_cast<__dword32>(
            GetProcAddressA(hFpCrtDLL, reinterpret_cast<LPCSTR>(pEntry->ordinal)));
        DEBUGCHK(pEntry->pfn != NULL);
    }
    
    // only one thread should hold the reference to the FPCRT dll
    //
    if(InterlockedCompareExchange(
        reinterpret_cast<PLONG>(&g_hFpCrtDLL),
        reinterpret_cast<LONG>(hFpCrtDLL),
        NULL) != NULL)
    {
        FreeLibrary(hFpCrtDLL);
    }
}


