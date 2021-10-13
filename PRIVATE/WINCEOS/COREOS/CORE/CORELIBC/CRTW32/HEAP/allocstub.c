//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>

// Only need this stub if debug or LMEM_DEBUG defined
#if defined(DEBUG) || defined(LMEM_DEBUG)
HLOCAL
WINAPI
LocalAllocTrace (UINT fuFlags, UINT cbBytes, UINT cLineNum, PCHAR szFilename)
{
#undef LocalAlloc
    return LocalAlloc(fuFlags, cbBytes);
}

#endif