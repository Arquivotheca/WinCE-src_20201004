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
/*++

Module Name:

    mem.cpp

Abstract:

    Memory Move operation for target-side.

Environment:

    OsaxsT0

--*/

#include "osaxs_p.h"

/*++

Routine Name:

    FlushCache

Routine Description:

    Flush cache so that memory changes will be properly propagated to all aliased virtual addresses.

--*/
void FlushCache ()
{
#if defined(MIPS)
    NKCacheRangeFlush (0, 0, CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
    NKCacheRangeFlush (0, 0, CACHE_SYNC_INSTRUCTIONS);
#elif defined(SHx)
    NKCacheRangeFlush (0, 0, CACHE_SYNC_INSTRUCTIONS | CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
#elif defined(ARM)
    NKCacheRangeFlush (0, 0, CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
    NKCacheRangeFlush (0, 0, CACHE_SYNC_INSTRUCTIONS);
#endif
}


DWORD OsAxsReadMemory (void *pDestination, PROCESS *pVM, void *pSource, DWORD Length)
{
    MEMADDR Dest;
    MEMADDR Source;

    SetDebugAddr(&Dest, pDestination);
    SetVirtAddr(&Source, DD_GetProperVM(pVM, pSource), pSource);
    DWORD ret = DD_MoveMemory0(&Dest, &Source, Length);
    return ret;
}



